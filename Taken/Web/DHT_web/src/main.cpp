#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// WiFi instellingen
#define SECRET_SSID "E109-E110"       // WiFi netwerknaam
#define SECRET_PASS "DBHaacht24"      // WiFi wachtwoord

// DHT11 instellingen
#define DHTPIN 15
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// IP van de laptop server
#define serverUrl "http://192.168.0.153/ESP_DHT/php.php"

void setup() {
  Serial.begin(115200);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Verbonden met WiFi");

  dht.begin();
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Fout bij uitlezen DHT11!");
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "temp=" + String(temperature) + "&hum=" + String(humidity);
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      Serial.printf("Data verzonden (%d)\n", httpResponseCode);
    } else {
      Serial.printf("Fout (%d)\n", httpResponseCode);
    }
    http.end();
  }

  delay(5000); // om de 5 s
}
