// ============================================================================
// WiFi + MQTT
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>

// WiFi-gegevens
#define SECRET_SSID "E109-E110"
#define SECRET_PASS "DBHaacht24"

// MQTT broker-instellingen
#define BROKER  "192.168.0.157"
#define PORT    1883
#define TOPIC   "CanSat"

// Maximale lengte van MQTT-berichtbuffer
#define MSG_LEN 180
#define DELAY_TIME 1000

// WiFiClient voor de MQTT-client
WiFiClient wifiClient;
// MQTT-client die over WiFiClient gaat
MqttClient mqttClient(wifiClient);

// Teller om aantal berichten bij te houden
unsigned long teller = 0;

void setup() {
  // Seriële monitor
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // Verbinding maken met WiFi
  Serial.println("WiFi verbinden...");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.printf("\nWiFi OK, IP = %s\n", WiFi.localIP().toString().c_str());

  // MQTT-client een ID geven
  mqttClient.setId("cansat-mqtt-only");

  // Verbinding maken met MQTT-broker
  Serial.println("MQTT verbinden...");
  while (!mqttClient.connect(BROKER, PORT)) {
    Serial.print("MQTT fout: ");
    Serial.println(mqttClient.connectError());
    delay(1000);
  }
  Serial.println("MQTT verbonden.");
}

void loop() {
  // Hou de MQTT-verbinding actief (pings, reconnects, …)
  mqttClient.poll();

  // Tijd en teller bijwerken
  unsigned long t = millis();
  teller++;

  // Buffer om bericht in te steken
  char line[MSG_LEN];
  snprintf(line, sizeof(line),
           "count=%lu;time_ms=%lu",
           teller, t);

  // Ook naar Serial printen
  Serial.println(line);

  // Bericht naar topic "CanSat" sturen
  mqttClient.beginMessage(TOPIC);
  mqttClient.print(line);
  mqttClient.endMessage();

  // Even wachten
  delay(DELAY_TIME);
}
