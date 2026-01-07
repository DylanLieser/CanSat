// ----------------------------------------------------------------------------
// Primary Mission CanSat
// BMP280 op I2C-2 + GPS + CSV naar MQTT + SD-backup met versiebeheer
// Dylan Lieser 5 611ICW
// ----------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <FS.h>
#include <SD.h>
#include <TinyGPSPlus.h>

// Communicatie en sensorinstellingen
#define SEALEVELPRESSURE_HPA 1018.2   // Referentiedruk op zeeniveau
#define BMP280_ADDR  0x76             // Adres BMP280-sensor
#define SDA_2        32               // SDA-pin I2C-bus 2
#define SCL_2        33               // SCL-pin I2C-bus 2
#define DELAY_TIME   1000             // Tijd tussen metingen in milliseconden

// GPS-instellingen via UART1
#define GPS_RX 16                     // RX-pin ESP32 (verbonden met TX van GPS)
#define GPS_TX 17                     // TX-pin ESP32 (verbonden met RX van GPS)
#define GPS_BAUD 9600                 // Baudrate van GPS-module

// WiFi instellingen
#define SECRET_SSID "E109-E110"       // WiFi netwerknaam
#define SECRET_PASS "DBHaacht24"      // WiFi wachtwoord

// MQTT instellingen
#define BROKER  "192.168.0.157"       // Adres van MQTT broker
#define PORT    1883                  // Brokerpoort
#define TOPIC   "CanSat"              // Topic voor data
#define MSG_LEN 180                   // Maximale berichtlengte

// SD-kaart instellingen (SPI)
#define SD_CS   25                    // CS-pin SD-module
#define SD_SCK  18                    // Clock pin
#define SD_MISO 19                    // MISO pin
#define SD_MOSI 23                    // MOSI pin
static const char* BASE_FILE = "/CanSatSend.txt"; // Hoofdbestand

// Objecten en variabelen
WiFiClient wifiClient;                // WiFi client
MqttClient mqttClient(wifiClient);    // MQTT client
Adafruit_BMP280 bmp(&Wire1);          // BMP280 via tweede I2C-bus
TinyGPSPlus gps;                      // GPS object
HardwareSerial SerialGPS(1);          // UART1 voor GPS
bool sd_ok = false;                   // SD-status

//zoek volgend vrij versienummer
int nextVersionNumber() {
  for (int n = 1; n < 10000; n++) {
    char name[32];
    snprintf(name, sizeof(name), "/CanSatSend%04d.txt", n);
    if (!SD.exists(name)) return n;
  }
  return -1;
}

// Functiedeclaraties
void rotateBaseFileIfExists();   // Maakt backup met nieuw versienummer
void ensureBaseFile();           // Controleert of nieuw hoofd bestand bestaat
void appendCSVLine(const char*); // Voegt nieuwe lijn toe aan CSV-bestand

//start sensoren, WiFi, MQTT, SD-kaart en GPS
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire1.begin(SDA_2, SCL_2, 400000);  // Start tweede I2C-bus

  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("Fout: BMP280 niet gevonden.");
    while (1) delay(100);
  }

  SerialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX); // Start GPS

  Serial.println("time_ms;sats;hdop;lon;lat;datetimeUTC;alt_m;hoogte_bmp_m;druk_hPa;temperatuur_C"); // CSV-header

  // Verbind met WiFi
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(1000); }
  Serial.printf("\nWiFi OK, IP = %s\n", WiFi.localIP().toString().c_str());

  // Verbind met MQTT broker
  mqttClient.setId("cansat-bmp280");
  while (!mqttClient.connect(BROKER, PORT)) {
    Serial.print("MQTT fout: ");
    Serial.println(mqttClient.connectError());
    delay(1000);
  }
  Serial.println("MQTT verbonden.");

  // Initialiseer SD-kaart
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sd_ok = SD.begin(SD_CS, SPI, 4000000);
  if (sd_ok) {
    rotateBaseFileIfExists();
    ensureBaseFile();
  } else {
    Serial.println("SD niet beschikbaar. Ga verder met alleen MQTT.");
  }
}

// Loop: lees GPS en BMP280, stuur via MQTT en log naar SD
void loop() {
  // Zorg dat MQTT verbonden blijft
  mqttClient.poll();

  // Lees GPS-gegevens
  while (SerialGPS.available()) gps.encode(SerialGPS.read());

  // Verzamel sensor- en GPS-gegevens
  unsigned long t = millis();
  float tempC   = bmp.readTemperature();
  float druk_hP = bmp.readPressure() / 100.0f;
  float hoogte_bmp  = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  // GPS-gegevens ophalen
  int sats  = gps.satellites.isValid() ? gps.satellites.value() : -1;
  float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 0.0f;
  double lat = gps.location.isValid() ? gps.location.lat() : 0.0;
  double lon = gps.location.isValid() ? gps.location.lng() : 0.0;
  float alt  = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;

  // Format datum en tijd
  char dateTime[30];
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(dateTime, sizeof(dateTime), "%02d/%02d/%04d %02d:%02d:%02d",
             gps.date.day(), gps.date.month(), gps.date.year(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    snprintf(dateTime, sizeof(dateTime), "00/00/0000 00:00:00");
  }

  // Maak CSV-lijn
  char line[MSG_LEN];
  snprintf(line, MSG_LEN,
           "%lu;%d;%.2f;%.6f;%.6f;%s;%.2f;%.2f;%.2f;%.2f",
           t, sats, hdop, lon, lat, dateTime,
           alt, hoogte_bmp, druk_hP, tempC);

  Serial.println(line);

  // Stuur via MQTT
  mqttClient.beginMessage(TOPIC);
  mqttClient.print(line);
  mqttClient.endMessage();

  appendCSVLine(line);

  delay(DELAY_TIME);
}

// Hulpfuncties voor SD-bestandbeheer
void rotateBaseFileIfExists() {
  if (!SD.exists(BASE_FILE)) return;
  int ver = nextVersionNumber();
  if (ver < 0) return;
  char name[32];
  snprintf(name, sizeof(name), "/CanSatSend%04d.txt", ver);
  SD.rename(BASE_FILE, name);
}

// Zorg dat het hoofdbestand bestaat, anders maak het aan met header
void ensureBaseFile() {
  if (SD.exists(BASE_FILE)) return;
  File f = SD.open(BASE_FILE, FILE_WRITE);
  if (f) {
    f.println("time_ms;sats;hdop;lon;lat;datetimeUTC;alt_m;hoogte_bmp_m;druk_hPa;temperatuur_C");
    f.close();
  }
}

// Voeg een nieuwe CSV-lijn toe aan het hoofdbestand
void appendCSVLine(const char* line) {
  if (!sd_ok) return;
  File f = SD.open(BASE_FILE, FILE_APPEND);
  if (!f) { sd_ok = false; return; }
  f.println(line);
  f.close();
}
