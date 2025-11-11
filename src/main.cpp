// ----------------------------------------------------------------------------
// Primary Mission CanSat
// BMP280 op I2C-2 + CSV naar MQTT + SD-backup met versiebeheer
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

// Communicatie / sensorinstellingen
#define SEALEVELPRESSURE_HPA 1018.2   // Referentiedruk op zeeniveau
#define BMP280_ADDR  0x76             // Adres BMP280-sensor
#define SDA_2        32               // SDA-pin I2C-bus 2
#define SCL_2        33               // SCL-pin I2C-bus 2
#define DELAY_TIME   1000             // Tijd tussen metingen (ms)

// WiFi instellingen
#define SECRET_SSID "Proximus-Home-9270"       // WiFi netwerknaam
#define SECRET_PASS "w9y9xb64a93zd"      // WiFi wachtwoord

// MQTT instellingen
#define BROKER  "192.168.1.51"       // Adres van MQTT broker
#define PORT    1883                  // Brokerpoort
#define TOPIC   "CanSat"              // Topic voor data
#define MSG_LEN 120                   // Maximale berichtlengte

/*
// WiFi instellingen
#define SECRET_SSID "E109-E110"       // WiFi netwerknaam
#define SECRET_PASS "DBHaacht24"      // WiFi wachtwoord

// MQTT instellingen
#define BROKER  "192.168.0.157"       // Adres van MQTT broker
#define PORT    1883                  // Brokerpoort
#define TOPIC   "CanSat"              // Topic voor data
#define MSG_LEN 120                   // Maximale berichtlengte
*/

// SD-kaart instellingen
#define SD_CS   25                    // CS-pin SD-module
#define SD_SCK  18                    // Clock pin
#define SD_MISO 19                    // MISO pin
#define SD_MOSI 23                    // MOSI pin
static const char* BASE_FILE = "/CanSatSend.txt"; // Hoofdbestand

// Objecten en variabelen
WiFiClient wifiClient;                // WiFi clientobject
MqttClient mqttClient(wifiClient);    // MQTT clientobject
Adafruit_BMP280 bmp(&Wire1);          // BMP280 via tweede I2C-bus
bool sd_ok = false;                   // SD-status

// zoek volgend vrij versienummer
int nextVersionNumber() {
  for (int n = 1; n < 10000; n++) {
    char name[40];
    snprintf(name, sizeof(name), "/CanSatSend%04d.txt", n);
    if (!SD.exists(name)) return n;
  }
  return -1;
}

// Functiedeclaraties
void rotateBaseFileIfExists();   // Maakt backup met nieuw versienummer
void ensureBaseFile();           // Controleert of nieuw hoofd bestand bestaat + header
void appendCSVLine(const char*); // Voegt nieuwe lijn toe aan CSV-bestand

// Setup: start sensoren, WiFi, MQTT en SD-kaart
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Wire1.begin(SDA_2, SCL_2, 400000);  // Start tweede I2C-bus

  if (!bmp.begin(BMP280_ADDR)) {      // Controleer BMP280
    Serial.println("Fout: BMP280 niet gevonden.");
    while (1) delay(100);
  }

  Serial.println("time_ms;hoogte_m;druk_hPa;temperatuur_C"); // CSV-header op Serial

  WiFi.begin(SECRET_SSID, SECRET_PASS); // Verbinden met WiFi
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(1000); }
  Serial.printf("\nWiFi OK, IP = %s\n", WiFi.localIP().toString().c_str());

  mqttClient.setId("cansat-bmp280");   // MQTT-ID instellen
  while (!mqttClient.connect(BROKER, PORT)) {
    Serial.print("MQTT fout: "); Serial.println(mqttClient.connectError());
    delay(1000);
  }
  Serial.println("MQTT verbonden.");

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);  // Start SPI-bus
  sd_ok = SD.begin(SD_CS, SPI, 4000000);       // SD-kaart initialiseren
  if (sd_ok) {
    rotateBaseFileIfExists(); // Backup oude CSV
    ensureBaseFile();         // Maak nieuw bestand aan + header
  } else {
    Serial.println("SD niet beschikbaar (ga toch verder met MQTT).");
  }
}

// Loop: lees sensor, stuur via MQTT, log op SD
void loop() {
  mqttClient.poll();  // Houd MQTT actief

  unsigned long t = millis(); // Tijd in ms
  float tempC   = bmp.readTemperature();
  float druk_hP = bmp.readPressure() / 100.0f;
  float hoogte  = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  // Schrijf data naar seriële monitor
  Serial.print(t);        Serial.print(';');
  Serial.print(hoogte);   Serial.print(';');
  Serial.print(druk_hP);  Serial.print(';');
  Serial.println(tempC);

  // Bouw MQTT-payload (hoogte;druk;temp)
  char payload[MSG_LEN];
  snprintf(payload, MSG_LEN, "%.2f;%.2f;%.2f", hoogte, druk_hP, tempC);

  mqttClient.beginMessage(TOPIC); // Start MQTT-bericht
  mqttClient.print(payload);      // Voeg payload toe
  mqttClient.endMessage();        // Verstuur bericht

  // Bouw CSV-lijn (tijd;hoogte;druk;temp)
  char line[MSG_LEN];
  snprintf(line, MSG_LEN, "%lu;%.2f;%.2f;%.2f", t, hoogte, druk_hP, tempC);
  appendCSVLine(line);            // Schrijf lijn naar SD

  delay(DELAY_TIME);              // Wacht 1 seconde
}

// Maak backup van huidig CSV-bestand
void rotateBaseFileIfExists() {
  if (!SD.exists(BASE_FILE)) return;
  int ver = nextVersionNumber();
  if (ver < 0) return;
  char name[40];
  snprintf(name, sizeof(name), "/CanSatSend%04d.txt", ver);
  SD.rename(BASE_FILE, name);
}

// Controleer of hoofd-CSV-bestand bestaat, anders maak nieuw aan
void ensureBaseFile() {
  if (SD.exists(BASE_FILE)) return;
  File f = SD.open(BASE_FILE, FILE_WRITE);
  if (f) {
    f.println("time_ms;hoogte_m;druk_hPa;temperatuur_C");
    f.flush();
    f.close();
  }
}

// Voeg nieuwe lijn toe aan CSV-bestand
void appendCSVLine(const char* line) {
  if (!sd_ok) return;
  File f = SD.open(BASE_FILE, FILE_APPEND);
  if (!f) { 
    sd_ok = false; 
    Serial.println("SD write fail: open APPEND mislukt");
    return; 
  }
  f.println(line);
  f.flush();  
  f.close();
}
