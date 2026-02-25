#include <Wire.h>
#include <ArduinoJson.h>
#include <Arduino.h>

// Kleine, robuuste sender van een JSON-record over I2C (master)
void setup() {
  // Start I2C als master (ESP32: default SDA=21, SCL=22). Pas aan als nodig:
  Wire.begin();
  Serial.begin(115200);
  // seed random zodat waarden variëren
  randomSeed(analogRead(0));
}

void loop() {
  // 3 variabelen: int, float, String
  int waardeInt = random(0, 100);
  float waardeFloat = random(0, 1000) / 10.0;
  String waardeString = "msg" + String(random(0, 10));

  // Maak JSON (reserveer genoeg ruimte)
  StaticJsonDocument<256> doc;
  doc["intVar"] = waardeInt;
  doc["floatVar"] = waardeFloat;
  doc["stringVar"] = waardeString;

  // Serialiseer naar een char buffer zodat we lengte kennen
  char buf[192];
  size_t len = serializeJson(doc, buf, sizeof(buf));

  // Stuur via I2C naar slave met adres 8
  Wire.beginTransmission(8); // I²C-adres van ESP-2
  // geef expliciet lengte mee — anders kan Wire intern op een limiet stuiten
  Wire.write((const uint8_t*)buf, len);
  uint8_t status = Wire.endTransmission();

  // Log voor debug: status en de feitelijke payload
  Serial.print("I2C status: ");
  Serial.println(status); // 0 == success
  Serial.print("Verzonden (len="); Serial.print(len); Serial.print("): ");
  Serial.println(buf);

  delay(5000); // elke 5 sec
}