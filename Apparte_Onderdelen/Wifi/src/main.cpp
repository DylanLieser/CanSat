// ============================================================================
// WiFi, LED aan als verbonden
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>

// Twee LEDs als indicator (optioneel)
#define LED1 12
#define LED2 14

// WiFi-gegevens
#define SECRET_SSID "E109-E110"
#define SECRET_PASS "DBHaacht24"

void setup() {
  // Seriële monitor starten
  Serial.begin(115200);

  // LED-pinnen als output instellen
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);

  Serial.println("WiFi verbinden...");

  // Verbinding maken met het WiFi-netwerk
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  // Wachten tot de verbinding tot stand komt
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nVerbonden!");
  Serial.print("IP-adres: ");
  Serial.println(WiFi.localIP());

  // Zet LED1 aan als indicatie dat WiFi verbonden is
  digitalWrite(LED1, HIGH);
}

void loop() {
  // Check WiFi-status regelmatig
  if (WiFi.status() == WL_CONNECTED) {
    // WiFi verbonden: LED2 aan
    digitalWrite(LED2, HIGH);
    Serial.println("WiFi status: VERBONDEN");
  } else {
    // WiFi niet verbonden: LED2 uit
    digitalWrite(LED2, LOW);
    Serial.println("WiFi status: NIET VERBONDEN");
    
    // Probeer opnieuw te verbinden
    Serial.println("Pogingen om opnieuw te verbinden...");
    WiFi.reconnect();
  }
  
  // Elke 5 seconden checken
  delay(5000);
}
