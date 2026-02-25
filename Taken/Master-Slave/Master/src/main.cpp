//Master_Program

#include <Arduino.h>
#include <Wire.h>  // Inclusie van de I2C-bibliotheek

#define SLAVE_ADDR  3 // Definieer het I2C-adres van de Slave die we willen aanspreken

const int pot = A0; // Potentiometer is verbonden met pin A0 op de microcontroller

// Setup functie: wordt eenmaal uitgevoerd bij het opstarten
void setup() {
  Serial.begin(9600); // Start seriële communicatie met een baudrate van 9600
  while (!Serial) {
    delay(10); // Wacht tot de seriële poort beschikbaar is
  }

  pinMode(pot, INPUT); // Stel de pin A0 in als input

  // Print een opstartbericht op de seriële monitor
  Serial.println("Start of Master/Slave communication");

  // Start I2C in Master-modus
  Wire.begin();
}

// Loop functie: wordt continu herhaald tijdens de werking
void loop() {
  int ledwaarde = 0; // startwaarde van de led instellen op 0

  // Variabele om de waarde van de potmeter op te slaan
  int potwaarde = analogRead(pot); // Lees de potmeter in en stel gelijk aan potwaarde

  ledwaarde = map(potwaarde, 0, 1023, 0, 255); // zet de waarde van de potmeter (0-1023) om tot een waarde die op de led gebruikt kan worden (0-255)

  // Buffer om de waarden om te zetten naar een string
  // - Maximaal 3 karakters + 1 voor de null-terminator ('\0')
  char message[5];
  sprintf(message, "%3d", ledwaarde); // Zet de integer om in een string met 3 karakters

  // Start transmissie naar de Slave op adres SLAVE_ADDR
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(message);          // Stuur de string met de ledwaarde
  Wire.endTransmission();       // Beëindig de transmissie

  // Print de waarden die naar de Slave is verzonden op de seriële monitor
  Serial.print("Pot Val: ");
  Serial.print(potwaarde);
  Serial.print("   ");
  Serial.print("LED val: ");
  Serial.println(message);

  // Wacht 2 seconden voordat een nieuwe waarde wordt verzonden
  delay(500);
}