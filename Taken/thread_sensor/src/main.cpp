#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Pin definities
#define TRIG_PIN 5
#define ECHO_PIN 18

// Ultrasoon sensor parameters
#define PULSE_TIME 30000      // maximale wachttijd echo (µs)
#define SOUND_SPEED 0.034     // geluidssnelheid (cm per µs)

// Wifi
const char* ssid = "E109-E110";
const char* pass = "DBHaacht24";

// WebSocket server op poort 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Functie declaraties
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
float Meten();
float gemiddelde();

// variabelen voor gemiddelde
float arr[5] = {0, 0, 0, 0, 0};   // laatste 5 metingen
int plaats = 0;                    // positie om nieuwe meting op te slaan
int aantal_metingen = 0;          // hoeveel geldige metingen er zijn

void setup() {
  Serial.begin(115200);

  // Pin configuratie
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Verbinden met WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Verbinden");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi verbonden!");
  Serial.print("IP-adres: ");
  Serial.println(WiFi.localIP());

  // WebSocket server starten
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();  // WebSocket actief houden

  // Afstand meten
  float afstand = Meten();

  // Alleen geldige metingen opslaan
  if (afstand >= 0) {
    arr[plaats] = afstand;

    // Volgende plaats in array
    plaats++;
    if (plaats >= 5) {
      plaats = 0;
    }

    // Aantal metingen max 5
    if (aantal_metingen < 5) {
      aantal_metingen++;
    }
  }

  // JSON-object maken
  StaticJsonDocument<256> doc;
  doc["afstand"] = afstand;
  doc["gemiddelde"] = gemiddelde();

  // JSON verzenden via WebSocket
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  webSocket.broadcastTXT(jsonBuffer);

  delay(1000);  // 10 metingen per seconde
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.println("Client verbonden");
  } 
  else if (type == WStype_DISCONNECTED) {
    Serial.println("Client verbroken");
  }
}

// afstand meten
float Meten() {
  long duration;
  float distance;

  // TRIG puls
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Echo tijd meten
  duration = pulseIn(ECHO_PIN, HIGH, PULSE_TIME);

  if (duration == 0) {
    Serial.println("Geen echo ontvangen");
    return -1;
  }

  // Afstand berekenen
  distance = duration * SOUND_SPEED / 2;

  Serial.print("Afstand: ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

//gemiddelde berekenen
float gemiddelde() {
  if (aantal_metingen == 0) {
    return -1;
  }

  float totaal = 0;
  for (int i = 0; i < aantal_metingen; i++) {
    totaal += arr[i];
  }

  Serial.print("gemiddelde: ");
  Serial.print(totaal / aantal_metingen);
  Serial.println(" cm");

  return totaal / aantal_metingen;
}
