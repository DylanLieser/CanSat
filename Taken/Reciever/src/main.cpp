#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

String data = "";

void receiveEvent(int howMany) {
  data = "";
  while (Wire.available()) {
    data += (char)Wire.read();
  }

  StaticJsonDocument<100> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.print("JSON fout: ");
    Serial.println(error.c_str());
    return;
  }

  int intVar = doc["intVar"];
  float floatVar = doc["floatVar"];
  const char* stringVar = doc["stringVar"];

  Serial.println("Ontvangen JSON:");
  Serial.println(data);
  Serial.print("intVar: "); Serial.println(intVar);
  Serial.print("floatVar: "); Serial.println(floatVar);
  Serial.print("stringVar: "); Serial.println(stringVar);
}

void setup() {
  Wire.begin(8);  // I²C-adres
  Wire.onReceive(receiveEvent);
  Serial.begin(115200);
}

void loop() {
  delay(100);
}