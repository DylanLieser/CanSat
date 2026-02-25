#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Server configuratie
#define serverIP   "192.168.25.35"
#define maxRecords 10
#define serverPort 5000 

// WiFi netwerk credentials
#define SECRET_SSID "CanSat24"
#define SECRET_PASS "DBHaacht24"

void setup() {
  // WiFi verbinding maken
  Serial.begin(115200);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.print("Verbinden met WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nVerbonden!");
}

void loop() {
  // Ophalen van data records van de server
  for (int i = 0; i < maxRecords; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String("http://") + serverIP + ":" + serverPort + "/api/data/" + i;
      http.begin(url);

      // HTTP GET verzoek sturen
      int httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        String payload = http.getString();
        Serial.println("Ontvangen JSON:");
        Serial.println(payload);

        // JSON parseren met ArduinoJson
        const size_t capacity = 512;
        DynamicJsonDocument doc(capacity);
        DeserializationError error = deserializeJson(doc, payload);

        // Controleren op parse fouten
        if (!error) {
          String tijd  = doc["timestamp"];
          float temp   = doc["temperature_c"];
          float hum    = doc["humidity_pct"];
          float licht  = doc["light_level"];
          float co2    = doc["co2_ppm"];

          // Gegevens weergeven
          Serial.printf("Record %d\n", i);
          Serial.printf("Tijd: %s\n", tijd.c_str());
          Serial.printf(
            "Temp: %.1f °C | Vocht: %.1f %% | Licht: %.0f lx | CO2: %.0f ppm\n\n",
            temp, hum, licht, co2
          );
        }
      } else {
        Serial.printf("HTTP foutcode: %d\n", httpResponseCode);
      }

      http.end();
    }
    delay(1000);  // wacht 1 seconde tussen aanvragen
  }

  Serial.println("Alle records opgehaald.\n");
  delay(60000);
}
