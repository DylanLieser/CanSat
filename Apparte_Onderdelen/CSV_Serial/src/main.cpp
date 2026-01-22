// ============================================================================
// CSV,Serial
// ============================================================================

#include <Arduino.h>

void setup() {
  // Seriële monitor
  Serial.begin(115200);
  while (!Serial) {;}

  // CSV-header tonen
  Serial.println("time_ms;temp_C;druk_hPa;hoogte_m");
}

void loop() {
  // Tijd in milliseconden
  unsigned long t = millis();

  // Dummy-waarden laten variëren met sin/cos voor test
  float tempC = 20.0 + (sin(t / 5000.0) * 5.0);   // temp rond 20°C
  float druk  = 1013.0 + (cos(t / 4000.0) * 3.0); // druk rond 1013 hPa
  float hoog  = 50.0 + (sin(t / 7000.0) * 2.0);   // hoogte rond 50m

  // Buffer voor één CSV-lijn
  char line[128];
  snprintf(line, sizeof(line),
           "%lu;%.2f;%.2f;%.2f",
           t, tempC, druk, hoog);

  // CSV-lijn printen
  Serial.println(line);

  // Wachten tot volgende update
  delay(1000);
}
