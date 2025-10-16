// ----------------------------------------------------------------------------
// Primary Mission CanSat
// BMP280 op I2C-2 + CSV naar MQTT (hoogte, druk, temperatuur)
// Dylan Lieser 5 611ICW
// ----------------------------------------------------------------------------

//Alle libraries ide we gebruiken
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>

// Communicatie
#define SEALEVELPRESSURE_HPA (1018.2) // pas aan
#define BMP280_ADDR  0x76
#define SDA_2        32
#define SCL_2        33
#define DELAY_TIME   1000

// WIFI
#define SECRET_SSID "E109-E110"
#define SECRET_PASS "DBHaacht24"

// MQTT
#define BROKER  "192.168.0.250"
#define PORT    1883
#define TOPIC   "CanSat"

#define MSG_LEN 120

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

//Geef Wire1 aan de CONSTRUCTOR
Adafruit_BMP280 bmp(&Wire1);

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // I2C-2 starten
  Wire1.begin(SDA_2, SCL_2, 400000);

  // BMP280 initialiseren
  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("Fout: BMP280 niet gevonden (check wiring/adres).");
    while (1) { delay(100); }
  }

  Serial.println("time_ms,hoogte_m,druk_hPa,temperatuur_C");

  // WiFi
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(1000); }
  Serial.print("\nWiFi OK, IP = ");
  Serial.println(WiFi.localIP());

  // MQTT
  mqttClient.setId("cansat-bmp280");
  while (!mqttClient.connect(BROKER, PORT)) {
    Serial.print("MQTT fout: "); Serial.println(mqttClient.connectError());
    delay(1000);
  }
  Serial.println("MQTT verbonden.");
}

void loop() {
  mqttClient.poll();

  // waardes aflezen van sensor en initialiseren
  unsigned long tijd = millis();
  float tempC    = bmp.readTemperature();                 // °C
  float druk_hP = bmp.readPressure() / 100.0f;           // hPa
  float hoogte    = bmp.readAltitude(SEALEVELPRESSURE_HPA);// m

  // Serial CSV maken
  Serial.print(hoogte); Serial.print(';');
  Serial.print(druk_hP); Serial.print(';');
  Serial.println(tempC);

  // MQTT CSV: tijd,hoogte,druk,temperatuur
  char payload[MSG_LEN];
  snprintf(payload, MSG_LEN, "%.2f;%.2f;%.2f", hoogte, druk_hP, tempC);

  mqttClient.beginMessage(TOPIC);
  mqttClient.print(payload);
  mqttClient.endMessage();

  delay(DELAY_TIME);
}
