// ============================================================================
// Alleen BMP280 via I2C (Wire1), CSV naar Serial
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// Druk op zeeniveau voor hoogteberekening (pas aan indien nodig)
#define SEALEVELPRESSURE_HPA 1018.2

// I2C-adres van de BMP280 (meestal 0x76 of 0x77)
#define BMP280_ADDR  0x76

// Pin-defines voor de tweede I2C-bus (Wire1) op een ESP32
#define SDA_2        32
#define SCL_2        33

// Meet-interval in milliseconden
#define DELAY_TIME   1000

// Maak een BMP280-object dat Wire1 gebruikt in plaats van standaard Wire
Adafruit_BMP280 bmp(&Wire1);

void setup() {
  // Seriële monitor starten
  Serial.begin(115200);
  while (!Serial) { 
    // Wachten tot Serial klaar is
    delay(10); 
  }

  // I2C-bus starten op de opgegeven SDA/SCL-pinnen
  Wire1.begin(SDA_2, SCL_2, 400000);  // 400kHz I2C

  // BMP280 initialiseren op het opgegeven adres
  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("Fout: BMP280 niet gevonden!");
    // Oneindige lus als de sensor niet gevonden wordt
    while (1) {
      delay(100);
    }
  }

  // CSV-header om duidelijk te maken welke kolom wat is
  Serial.println("time_ms;hoogte_bmp_m;druk_hPa;temperatuur_C");
}

void loop() {
  // Tijd sinds opstart in milliseconden
  unsigned long t = millis();

  // Temperatuur in graden Celsius
  float tempC      = bmp.readTemperature();
  // Druk in hPa (sensor geeft Pa, dus delen door 100)
  float druk_hP    = bmp.readPressure() / 100.0f;
  // Hoogte in meter berekend t.o.v. de opgegeven zeeniveaudruk
  float hoogte_bmp = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  // Buffer om één CSV-lijn in te zetten
  char line[128];
  snprintf(line, sizeof(line),
           "%lu;%.2f;%.2f;%.2f",
           t, hoogte_bmp, druk_hP, tempC);

  // CSV-lijn naar Serial sturen
  Serial.println(line);

  // Even wachten tot de volgende meting
  delay(DELAY_TIME);
}
