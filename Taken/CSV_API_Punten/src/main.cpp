#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Server configuratie
#define serverIP   "192.168.25.35"   // Pi IP
#define serverPort 5001
#define maxRecords 10              // aantal studenten

// WiFi netwerk credentials
#define SECRET_SSID "CanSat24"
#define SECRET_PASS "DBHaacht25"

  // variabelen voor gemiddelden
  float sumWiskunde = 0, sumFysica = 0, sumInformatica = 0, sumMechanica = 0, sumElektronica = 0, sumEngels = 0, sumNederlands = 0, sumGeschiedenis = 0, sumAardrijkskunde = 0;


void setup() {
  // Seriële communicatie starten
  Serial.begin(115200);
  // Wifi opzetten
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.print("Verbinden met WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nVerbonden!");
}

void loop() {

  // gegevens van alle studenten ophalen
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

        // JSON parseren
        const size_t capacity = 1024;
        DynamicJsonDocument doc(capacity);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          // gegevens uitlezen
          String naam        = doc["naam"];
          float wiskunde     = doc["wiskunde"];
          float fysica       = doc["fysica"];
          float informatica  = doc["informatica"];
          float mechanica    = doc["mechanica"];
          float elektronica  = doc["elektronica"];
          float engels       = doc["engels"];
          float nederlands   = doc["nederlands"];
          float geschiedenis = doc["geschiedenis"];
          float aardrijkskunde = doc["aardrijkskunde"];

          // optellen voor gemiddelde
          sumWiskunde      += wiskunde;
          sumFysica        += fysica;
          sumInformatica   += informatica;
          sumMechanica     += mechanica;
          sumElektronica   += elektronica;
          sumEngels        += engels;
          sumNederlands    += nederlands;
          sumGeschiedenis  += geschiedenis;
          sumAardrijkskunde+= aardrijkskunde;

          // gegevens printen
          Serial.printf("Record %d\n", i);
          Serial.printf(
            "naam: %s\nwiskunde: %.1f\nfysica: %.1f\ninformatica: %.0f\nmechanica: %.0f\nelektronica: %.1f\nengels: %.1f\nnederlands: %.0f\ngeschiedenis: %.0f\naardrijkskunde: %.0f \n\n",
            naam.c_str(),wiskunde, fysica, informatica, mechanica,
            elektronica, engels, nederlands, geschiedenis, aardrijkskunde
          );
        }

      // foutafhandeling
      } else {
        Serial.printf("HTTP foutcode: %d\n", httpResponseCode);
      }

      http.end();
    }
    delay(1000);  // wacht 1 seconde tussen aanvragen
  }

  // gemiddelden uitrekenen en printen
  if (maxRecords > 0) {
    Serial.println("De gemiddelden per vak:");
    Serial.printf("Vak: wiskunde      gemiddelde = %.2f\n", sumWiskunde / maxRecords);
    Serial.printf("Vak: fysica        gemiddelde = %.2f\n", sumFysica / maxRecords);
    Serial.printf("Vak: informatica   gemiddelde = %.2f\n", sumInformatica / maxRecords);
    Serial.printf("Vak: mechanica     gemiddelde = %.2f\n", sumMechanica / maxRecords);
    Serial.printf("Vak: elektronica   gemiddelde = %.2f\n", sumElektronica / maxRecords);
    Serial.printf("Vak: engels        gemiddelde = %.2f\n", sumEngels / maxRecords);
    Serial.printf("Vak: nederlands    gemiddelde = %.2f\n", sumNederlands / maxRecords);
    Serial.printf("Vak: geschiedenis  gemiddelde = %.2f\n", sumGeschiedenis / maxRecords);
    Serial.printf("Vak: aardrijkskunde gemiddelde = %.2f\n\n", sumAardrijkskunde / maxRecords);
  }

  Serial.println("Alle records opgehaald.\n");
  delay(60000);   // 1 minuut wachten en dan opnieuw
}
