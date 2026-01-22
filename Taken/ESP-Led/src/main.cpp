#include <WiFi.h>
#include <WebServer.h>

#define LED1 12
#define LED2 14

// WiFi instellingen
#define SECRET_SSID "E109-E110"       // WiFi netwerknaam
#define SECRET_PASS "DBHaacht24"      // WiFi wachtwoord

// MQTT instellingen
#define BROKER  "192.168.0.157"       // Adres van MQTT broker
#define PORT    1883                  // Brokerpoort
#define TOPIC   "CanSat"              // Topic voor data

WebServer server(80);

void handleLED1On() {
  digitalWrite(LED1, HIGH);
  server.send(200, "text/plain", "LED1 ON");
}

void handleLED1Off() {
  digitalWrite(LED1, LOW);
  server.send(200, "text/plain", "LED1 OFF");
}

void handleLED2On() {
  digitalWrite(LED2, HIGH);
  server.send(200, "text/plain", "LED2 ON");
}

void handleLED2Off() {
  digitalWrite(LED2, LOW);
  server.send(200, "text/plain", "LED2 OFF");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP = " + WiFi.localIP().toString());

  server.on("/led1/on", handleLED1On);
  server.on("/led1/off", handleLED1Off);
  server.on("/led2/on", handleLED2On);
  server.on("/led2/off", handleLED2Off);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  server.handleClient();
}
