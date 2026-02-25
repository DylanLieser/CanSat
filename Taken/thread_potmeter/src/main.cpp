#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

const char* ssid = "JOUW_WIFI";
const char* pass = "JOUW_WACHTWOORD";

#define POTPIN 32                // analoge pin (bv. 32)
WebSocketsServer webSocket(81);  // poort 81

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, pass);
  Serial.print("Verbinden");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi verbonden!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();                 // verbinding onderhouden

  int potValue = analogRead(POTPIN); // 0..4095
  String msg = String(potValue);

  webSocket.broadcastTXT(msg);      // stuur naar alle clients
  delay(100);                       // 10 metingen/sec
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.println("Client verbonden");
  } 
  else if (type == WStype_DISCONNECTED) {
    Serial.println("Client weg");
  }
}