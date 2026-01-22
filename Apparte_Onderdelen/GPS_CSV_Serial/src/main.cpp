// ============================================================================
// GPS,CSV naar Serial (eerste versie)
// ============================================================================

#include <Arduino.h>
#include <TinyGPSPlus.h>

// Pin-defines voor UART1 (GPS)
#define GPS_RX   16  // RX-pin van de ESP32 (verbonden met TX van GPS)
#define GPS_TX   17  // TX-pin van de ESP32 (verbonden met RX van GPS)
#define GPS_BAUD 9600

// Meet-interval
#define DELAY_TIME 1000

// GPS-parser object
TinyGPSPlus gps;

// Seriële poort 1 voor GPS (HardwareSerial instance)
HardwareSerial SerialGPS(1);

void setup() {
  // Seriële monitor
  Serial.begin(115200);
  while (!Serial) { 
    delay(10); 
  }

  // Seriële poort voor GPS starten (UART1 op de ESP32)
  SerialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  // CSV-header
  Serial.println("time_ms;sats;hdop;lon;lat;datetimeUTC;alt_m");
}

void loop() {
  // Zolang er bytes van GPS binnenkomen, naar TinyGPS++ doorsturen
  while (SerialGPS.available()) {
    gps.encode(SerialGPS.read());
  }

  unsigned long t = millis();

  // Aantal satellieten (of -1 als niet geldig)
  int sats = gps.satellites.isValid() ? gps.satellites.value() : -1;
  // HDOP (horizontale nauwkeurigheid)
  float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 0.0f;
  // Breedtegraad
  double lat = gps.location.isValid() ? gps.location.lat() : 0.0;
  // Lengtegraad
  double lon = gps.location.isValid() ? gps.location.lng() : 0.0;
  // Hoogte in meter
  float alt = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;

  // Datum + tijd string (UTC)
  char dateTime[30];
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(dateTime, sizeof(dateTime),
             "%02d/%02d/%04d %02d:%02d:%02d",
             gps.date.day(), gps.date.month(), gps.date.year(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    // Indien geen geldige datum/tijd
    snprintf(dateTime, sizeof(dateTime),
             "00/00/0000 00:00:00");
  }

  // CSV-lijn samenstellen
  char line[160];
  snprintf(line, sizeof(line),
           "%lu;%d;%.2f;%.6f;%.6f;%s;%.2f",
           t, sats, hdop, lon, lat, dateTime, alt);

  // Naar Serial sturen
  Serial.println(line);

  // Wachten tot volgende update
  delay(DELAY_TIME);
}
