#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>

void setup() {
  Wire.begin();  // als master
  Serial.begin(115200);
}

void loop() {
  int waardeInt = random(0, 100);
  float waardeFloat = random(0, 1000) / 10.0;
  String waardeString = "msg" + String(random(0, 10));

  StaticJsonDocument<100> doc;
  doc["intVar"] = waardeInt;
  doc["floatVar"] = waardeFloat;
  doc["stringVar"] = waardeString;

  String jsonStr;
  serializeJson(doc, jsonStr);

  Wire.beginTransmission(8); // I²C-adres van ESP-2
  Wire.write(jsonStr.c_str());
  Wire.endTransmission();

  Serial.println("Verzonden: " + jsonStr);
  delay(5000); // elke 5 sec
}