// ============================================================================
// SD-logging + rotatie van bestandsnamen
// ============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

// Pin-defines voor SD-kaart (SPI)
#define SD_CS   25
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

// Interval tussen logregels
#define DELAY_TIME 1000

// Basisbestand waarnaar we loggen
static const char* BASE_FILE = "/CanSatSend.txt";

// Vlag of SD-kaart beschikbaar is
bool sd_ok = false;

// Zoek het volgende vrije versienummer voor een logfile
int nextVersionNumber() {
  // Loopt door tot max 9999 bestanden
  for (int n = 1; n < 10000; n++) {
    char name[32];
    // Bestandsnaam /CanSatSend0001.txt, 0002, ...
    snprintf(name, sizeof(name), "/CanSatSend%04d.txt", n);
    // Als deze naam niet bestaat, kunnen we die gebruiken
    if (!SD.exists(name)) return n;
  }
  // Geen vrije nummer gevonden
  return -1;
}

// Hernoem BASE_FILE naar een genummerde versie, als BASE_FILE al bestaat
void rotateBaseFileIfExists() {
  if (!SD.exists(BASE_FILE)) return;  // niets te roteren
  int ver = nextVersionNumber();
  if (ver < 0) return;               // geen versienummer beschikbaar

  char name[32];
  snprintf(name, sizeof(name), "/CanSatSend%04d.txt", ver);

  // Bestand hernoemen
  SD.rename(BASE_FILE, name);
}

// Zorg dat het basisbestand bestaat, en schrijf eventueel een header
void ensureBaseFile() {
  if (SD.exists(BASE_FILE)) return;   // bestaat al

  File f = SD.open(BASE_FILE, FILE_WRITE);
  if (f) {
    // Een CSV-header schrijven (voorbeeld-kolommen)
    f.println("time_ms;dummy1;dummy2");
    f.close();
  }
}

// Voeg één CSV-lijn toe aan het logbestand
void appendCSVLine(const char* line) {
  if (!sd_ok) return;   // als SD niet werkt, stop

  File f = SD.open(BASE_FILE, FILE_APPEND);
  if (!f) {
    // Als openen mislukt, SD foutief
    sd_ok = false;
    return;
  }
  // Lijn schrijven
  f.println(line);
  f.close();
}

void setup() {
  // Seriële monitor
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // SPI-bus starten met de SD-pinnen
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // SD-kaart initialiseren
  sd_ok = SD.begin(SD_CS, SPI, 4000000);

  if (sd_ok) {
    Serial.println("SD-kaart OK");
    // Als er al een basisbestand is, hernoem het naar een nieuwe versie
    rotateBaseFileIfExists();
    // Zorg dat er een nieuw basisbestand is met header
    ensureBaseFile();
  } else {
    Serial.println("SD niet beschikbaar.");
  }
}

void loop() {
  // Dummydata genereren
  unsigned long t = millis();
  int dummy1 = (t / 1000) % 100;    // voorbeeld-integer
  float dummy2 = t / 1000.0f;       // voorbeeld-float

  // CSV-lijn opbouwen
  char line[128];
  snprintf(line, sizeof(line),
           "%lu;%d;%.2f", t, dummy1, dummy2);

  // Naar Serial tonen
  Serial.println(line);

  // Naar SD-logbestand schrijven
  appendCSVLine(line);

  // Interval
  delay(DELAY_TIME);
}
