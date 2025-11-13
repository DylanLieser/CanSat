//Slave_Program
#include <Arduino.h>
#include <Wire.h>  // Inclusie van de I2C-bibliotheek

#define SLAVE_ADDR  3 // Definieer het I2C-adres van deze Slave 

int led = 3;  // LED is verbonden met pin 4 op de microcontroller

// Setup functie: wordt eenmaal uitgevoerd bij het opstarten
void setup() {
  Serial.begin(115200);  // Start seriële communicatie met een baudrate van 115200
  while (!Serial) {
    delay(10);
  }  // Time to get serial running

  pinMode(led, OUTPUT);     // Stel pin 4 in als output (voor de LED)
  digitalWrite(led, LOW);   // Zet de LED standaard uit bij het opstarten

  // Print een opstartbericht op de seriële monitor
  Serial.println("START");
  Serial.println("++++++++++++++++++++++++++++++++++++++++++++++++++++");

  // Start I2C in Slave-modus met adres SLAVE_ADDR
  Wire.begin(SLAVE_ADDR);

  // Registreer de functie `receiveEvent` als interrupt handler
  // Deze functie wordt aangeroepen wanneer de Slave data ontvangt
  Wire.onReceive(receiveEvent);
}

// receiveEvent functie: wordt aangeroepen als de Slave data ontvangt via I2C
void receiveEvent(int aantal_char) {
  char inp[5]; // Buffer om maximaal 4 karakters + null-terminator ('\0') op te slaan
  int i = 0;   // Teller om het aantal ontvangen karakters bij te houden

  // Lees inkomende gegevens uit de I2C-buffer, zolang er data beschikbaar is
  while (Wire.available() > 0 && i < 4) { // Maximaal 4 karakters lezen
    inp[i] = Wire.read(); // Lees één byte uit de I2C-buffer
    i++;
  }
  inp[i] = '\0'; // Voeg een null-terminator toe om een geldige string te maken

  // Converteer de ontvangen string naar een byte
  byte ledwaarde = atoi(inp);

  // Stuur de LED aan met de juiste ledwaarde
  analogWrite(led, ledwaarde);

  // Print het ontvangen bericht en de waarde op de seriële monitor
  Serial.print("Receive value:  ");
  Serial.println(ledwaarde);
}

// Loop functie: blijft continu herhalen tijdens de werking
void loop() {
  // De loop blijft leeg, omdat de Slave volledig werkt via interrupts
  // (de `receiveEvent` functie wordt automatisch aangeroepen bij inkomende data)
}
