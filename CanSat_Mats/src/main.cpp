// ----------------------------------------------------------------------------
// CanSat Project 2025-2026 - Team SkyByte
// - BMP280 sensor reading                                                 done
// - GPS module reading                                                    done
// - Data logging to SD card                                               done
// - Sending data through LoRa (SPI connected)                             done
//
// Do this loop every 1 sec.
//
// Notes:
// 
// (1) Set LoRa sync word the same at transmitter & receiver
// (2) LoRa frequency is set for Europe (866 MHz)
// ----------------------------------------------------------------------------

#include <Arduino.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "SPI.h"                 // SPI
#include <Wire.h>                // I2C
#include <Adafruit_Sensor.h>     // Adafruit unified sensor base
#include <Adafruit_BMP280.h>     // BMP280 
#include <SD.h>                  // SD card 
#include <TinyGPSPlus.h>         // GPS parsing (TinyGPS++)
#include <LoRa.h>                // LoRa

// Declarations functions
void printValues();                                  // Create + publish + log CSV line
bool I2C_check(TwoWire *bus, byte address);          // I2C device presence check
void writeToSD(String data);                         // Append one CSV line to current file
int  readLastVersion();                              // Read last used version from VERSION_FILE (create with 0 if missing)
void saveLastVersion(int newLast);                   // Store last used version back to VERSION_FILE
void serviceGPS(unsigned long ms);                   // Read and parse NMEA data, keep last valid GPS values

void sendMsg( String outgoing );
void readRPi( String &msg );
void readBMP280( String &msg );
void readGPS( String &msg );
void setManualDateTime(int year, int month, int day, int hour, int minute, int second);
String getFormattedTime();

#define MSG_LEN 120
#define DELAY_TIME 1000 // Delay time between measurements (ms)

#define DEBUG // define / undefine to enable / disable debug mode
#define START_OF_NEW_TRANSMISSION "START-OF-NEW-TRANSMISSION"

#define csPIN     5  // Set chip select PIN for SPI connection 
#define resetPIN  14 // Set reset PIN for SPI connection
#define irqPIN    2  // Set IReq PIN for SPIconnection        

// --------------- LoRa Settings ---------------
// Frequentie-instelling (voor Europa: 868 MHz)
#define LORA_FREQ 868E6

// Adresinstellingen
#define SENDER_ADDRESS   0xAA
#define RECEIVER_ADDRESS 0xBB
#define LORA_ID          0x1B

// LoRa Paremeters
// Voor lange afstand — lage datasnelheid, hoge gevoeligheid
#define TxPower         20      // maximaal vermogen
#define SignalBandwidth 125E3   // 125 kHz standaard
#define CodingRate4     5       // 4/5 codering
#define SpreadingFactor 10      // 6–12 (hoger = groter bereik)
#define CRC             1       // CRC validatie AAN  
// #define SYNCWORD  0xF3 // Set Syncword for LoRa transmitter
                          // !! ensure same syncword is used at LoRa receiver

// General defines
#define MSG_LEN   150 // Message lengt of to be transmitted message
#define DELIMETER ";" // Set CSV DELIMETER to ;
#define DELAY_TIME  10000 // Delay time between 2 transmissions

// Define record types
#define MOTOR_RECORD  "M"
#define BMP_RECORD    "B"
#define GPS_RECORD    "G"

// Set the value of the sea level pressure correct at your location
// You can find it at https://www.meteo.be/nl/weer/waarnemingen/belgie
// If you do so, the height will be calculated approx. correctly.
// + or - 8 m height difference is normal, as the sensor has a deviation.
#define SEALEVELPRESSURE_HPA (989.5)

// BMP280 I2C address (some boards use 0x77)
#define BMP280_ADR 0x76

// Non-default pins for I2C connection
#define SDA_2 32    // Use this pin as secondary I2C SDA connection
#define SCL_2 33    // Use this pin as secondary I2C SCL connection

// SD Card SPI pins 
#define SD_CS_PIN   25    // CS        = GPIO25
#define SD_MOSI_PIN 23    // MOSI (DI) = GPIO23
#define SD_MISO_PIN 19    // MISO (DO) = GPIO19
#define SD_SCK_PIN  18    // SCK (CLK) = GPIO18

// Filenames / paths on SD
#define DATA_FILE_BASE "/CanSatSend"        // Base name for data files
#define VERSION_FILE   "/CanSatVersion.txt" // Stores last used version number

// GPS configuration
#define GPS_RX 16          // ESP32 RX2  <- GPS TX
#define GPS_TX 17          // ESP32 TX2  -> GPS RX
#define GPS_BAUD 9600      // GPS module baudrate (typical 9600)

// Create secondary I2C bus object on ESP32
TwoWire I2Ctwo = TwoWire(1);

// Bind BMP280 to the secondary I2C bus
Adafruit_BMP280 bmp(&I2Ctwo);

bool bmp_connected = false;    // Tracks BMP280 availability
bool sd_available = false;     // Tracks SD availability
int currentVersion;            // version number (saved on SD)
String currentFilename = "";   // Active data filename for this session

HardwareSerial GPSSerial(2);   // Use UART2 for GPS (hardware serial 2)
TinyGPSPlus gps;               // TinyGPS++ parser instance

int    gps_sats = 0;           // Last valid satellites count
float  gps_hdop = 0.0;         // Last valid HDOP
double gps_lat  = 0.0;         // Last valid latitude
double gps_lon  = 0.0;         // Last valid longitude

int  msgCount = 1;             // Set trasmit message counter to start

double gps_altm = 0.0;         // Last valid GPS altitude in meters

// -------------------------------------------------------------------------
// This is the onetime used setup function
// (1) aanvullen
// (2) 
// (3)
// -------------------------------------------------------------------------
void setup() {
  unsigned status;
  unsigned bmp_status; // Holds BMP initialization result

  // Stel tijd in: YYYY, MM, DD, HH, MM, SS
  setManualDateTime(2025, 11, 17, 15, 22, 00); // Set manual date/time to 17-11-2025 15:22:00

  // Serial Monitor start
  Serial.begin( 115200 ); 
  while (!Serial) {
    delay( 10 );
  }
  delay( 200 ); // Short delay to allow printout message on serial monitor
  Serial.println( "=======================================" );
  Serial.print( "Start of Program\n" );
  Serial.println( "=======================================" );
  
  // SD card 
  Serial.println("----------------------------------------------------------------------");
  Serial.println("\nInitialiseren SD kaart...");

  // Initialize the SPI bus with explicit pins
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(100);

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN, SPI)) {
    Serial.println("SD kaart initialisatie mislukt!");
    sd_available = false; // Continue without logging
  } else {
    Serial.println("SD kaart succesvol geïnitialiseerd");

    // Optional: print SD card info (size/type)
    uint64_t cardSize = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.print("SD kaart grootte: ");
    Serial.print(cardSize);
    Serial.println(" MB");

    uint8_t cardType = SD.cardType();
    Serial.print("SD kaart type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }

    sd_available = true;

    int lastUsed = readLastVersion();

    // Current version = last used + 1
    currentVersion = lastUsed + 1;

    // Build filename for this session
    currentFilename = String(DATA_FILE_BASE) + String(currentVersion) + ".txt";
    Serial.print("Data wordt weggeschreven naar: ");
    Serial.println(currentFilename);

    saveLastVersion(currentVersion);
  }

  // Start LoRa communication
  LoRa.setPins( csPIN, resetPIN, irqPIN );

  // Start de LoRa-module
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Fout: kon LoRa-module niet starten!");
    while (1);
  }

  // Voor lange afstand — lage datasnelheid, hoge gevoeligheid
  LoRa.setTxPower( TxPower );                   // maximaal vermogen
  LoRa.setSignalBandwidth( SignalBandwidth );   // 125 kHz standaard
  LoRa.setCodingRate4( CodingRate4 );           // 4/5 codering
  LoRa.setSpreadingFactor( SpreadingFactor );   // 6–12 (hoger = groter bereik)
  #if CRC
    LoRa.enableCrc();                           // CRC aan (idem pico )
  #endif

  Serial.println( "1. => LoRa module successful connected (SPI)" );   
  
  // Send Start of new transmission to receiver
  sendMsg( START_OF_NEW_TRANSMISSION );
  Serial.printf( "\t%s\n", START_OF_NEW_TRANSMISSION ); 
  Serial.println( "2. => Start of new transmission message send to receiver" );
  Serial.println();

  // Bring up secondary I2C bus on custom pins
  I2Ctwo.begin(SDA_2, SCL_2);  // SDA=32, SCL=33

  // Initialize BMP280 at given address
  bmp_status = bmp.begin(BMP280_ADR);
  if (!bmp_status) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring, address, sensor ID!");
  } else {
    bmp_connected = true;  // Sensor found

    // Optional: configure oversampling/filter to improve stability/noise
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* IIR Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    Serial.println("-- Default Test --");
    Serial.println("CanSat => BMP-280 successful connected (I2C-2)");
    Serial.println("\n----------------------------------------------------------------------\n");
  }

  // GPS init
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);  // Start UART2 for GPS
  Serial.println("GPS module initialized on UART2 @ 9600 baud.");

  Serial.println();
}

// -------------------------------------------------------------------------
// This is the loop function
// (1) 
// (2)
// (3)
// Restart loop
// -------------------------------------------------------------------------

void loop() {
  String RPi_msg = "";
  String BMP_msg = "";
  String GPS_msg = "";
  
  // Collect and parse GPS data each loop
  serviceGPS(200); // Read NMEA chars for ~200 ms and update last valid GPS values

  // Re-check BMP280 each loop
  bmp_connected = I2C_check(&I2Ctwo, BMP280_ADR);

  // Read data
  readRPi( RPi_msg );
  readBMP280( BMP_msg );
  readGPS( GPS_msg );
  
  // Transmit msg through LoRa
  // In 3 payloads, due to limited transmission time
  sendMsg( RPi_msg ); // Transmit RPi msg through LoRa
  sendMsg( BMP_msg ); // Transmit BMP280 msg through LoRa
  sendMsg( GPS_msg ); // Transmit GPS msg through LoRa
  
  #ifdef DEBUG
    Serial.println( RPi_msg ); // Print data on Serial monitor
    Serial.println( BMP_msg ); // Print data on Serial monitor
    Serial.println( GPS_msg ); // Print data on Serial monitor
    Serial.println();
  #endif

  // SD logging
  if (sd_available) {
    writeToSD(RPi_msg);
    writeToSD(BMP_msg);
    writeToSD(GPS_msg);
  }

  delay( DELAY_TIME );
}


// -------------------------------------------------------------------------
// This function sends the output CSV record through LoRa to the receiver
// -------------------------------------------------------------------------
void sendMsg( String outgoing ) {
  char buffer[MSG_LEN];

  outgoing.toCharArray(buffer, MSG_LEN); // convert String to char array

  // Transmit msg through LoRa
  LoRa.beginPacket();
  // Voeg ontvangeradres, zenderadres, ID en bericht lengte toe aan de LoRa header
    LoRa.write(RECEIVER_ADDRESS);
    LoRa.write(SENDER_ADDRESS);
    LoRa.write( LORA_ID );
    LoRa.write( strlen( buffer ) ); // bericht lengte

    // Voeg het bericht toe als één blok
    LoRa.write((const uint8_t*)buffer, strlen( buffer ) );
  
  LoRa.endPacket();

  msgCount++; // increment the message counter
}

// -------------------------------------------------------------------------
// This function reads values from the RPi Zero (through interupt call)
// and sets Engine mode according input.
// The CSV-record is set to include the desired stage of the engines
// -------------------------------------------------------------------------
void readRPi( String &msg ) {
    // Simulate turning state (0=false, 1=true)
    int turnL = 0; 
    int turnR = 1;

    // State of landing position
    String state = "Unsafe";

    msg = String( MOTOR_RECORD ) + DELIMETER + String(turnL) + DELIMETER + String(turnR) + DELIMETER + state;
}

// -------------------------------------------------------------------------
// This function reads values from the BMP280 Sensor
// and appends them to he CSV-record 
// Values read and appended are: Temperature in °C
//                               Pressure in Pa
//                               Altitude in m
//                               Humidity in %
// For accurate altitude calculation, set the sea level pressure correct at 
// begin of this program.
// -------------------------------------------------------------------------
void readBMP280( String &msg ) {
  if (!bmp_connected) {
    msg = String( BMP_RECORD ) + DELIMETER + "NA" + DELIMETER + "NA" + DELIMETER + "NA";
    return;
  }

  float temp = bmp.readTemperature();                 // °C
  float press = bmp.readPressure() / 100.0F;          // hPa 
  float alt = bmp.readAltitude(SEALEVELPRESSURE_HPA); // m

  msg = String( BMP_RECORD ) + DELIMETER;
  msg = msg + String( temp, 2 ) + DELIMETER;
  msg = msg + String( press, 2 ) + DELIMETER;
  msg = msg + String( alt, 2 );
}

// -------------------------------------------------------------------------
// This function reads values from the GPS Module
// and appends them to he CSV-record 
// Values read and appended are: Number of satelites in vieuw
// Quality of GPS data
// GPS coordinates - latitude, longitude
// GPS time in GMT time
// GPS altitude
// Note: altitude can only be measured with > 3 satelites in view
// Ot may take a while before the GPS sensor "connects" to GPOS satelites
// Therefore start this program well ahead of rocket launch to ensure proper 
// GPS reading.
// -------------------------------------------------------------------------
void readGPS( String &msg ) {
  String  date_time = getFormattedTime();

  // Prefer GPS time if valid (TinyGPS++)
  if (gps.date.isValid() && gps.time.isValid()) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d-%02d-%02d:%02d:%02d:%02d",
             gps.date.day(), gps.date.month(), (gps.date.year() % 100),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    date_time = String(buf);
  }

  int     nr_sats = gps_sats;
  float   hdoop = gps_hdop;
  float   latitude = (float)gps_lat;
  float   longitude = (float)gps_lon;
  float   altitude = (float)gps_altm;

  msg = String( GPS_RECORD ) + DELIMETER;
  msg = msg + String( nr_sats ) + DELIMETER;
  msg = msg + String( hdoop, 2 ) + DELIMETER;
  msg = msg + String( latitude, 6 ) + DELIMETER;
  msg = msg + String( longitude, 6 ) + DELIMETER;
  msg = msg + date_time + DELIMETER;
  msg = msg + String( altitude, 2 );
}

// ----- Handmatig datum/tijd instellen ----
void setManualDateTime(int year, int month, int day, int hour, int minute, int second)
{
    struct tm timeinfo = {};

    timeinfo.tm_year = year - 1900;  // jaar vanaf 1900
    timeinfo.tm_mon  = month - 1;    // maanden tellen vanaf 0
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min  = minute; 
    timeinfo.tm_sec  = second;

    time_t t = mktime(&timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
}

String getFormattedTime() {
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        return "00-00-00:00:00:00";   // fallback
    }

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%d-%m-%y:%H:%M:%S", &timeinfo);

    return String(buffer);
}

// -------------------------------------------------------------------------
// From code 1: SD + I2C + GPS service helpers
// -------------------------------------------------------------------------

// writeToSD(): append one CSV line to currentFilename
void writeToSD(String data) {
  File dataFile = SD.open(currentFilename, FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
  } else {
    Serial.println("Fout: kon niet schrijven naar SD kaart");
  }
}

// I2C_check(): returns true if device at 'address' ACKs on given bus
bool I2C_check(TwoWire *bus, byte address) {
  bus->beginTransmission(address);
  byte error = bus->endTransmission();
  return (error == 0);
}

// readLastVersion(): read last used version (create with "0" if missing)
int readLastVersion() {
  // The file stores the LAST used version number.
  // Doesn't exist? Create it with "0"
  int lastUsed = 0;

  File versionFileRead = SD.open(VERSION_FILE, FILE_READ);
  if (!versionFileRead) {
    File versionFileCreate = SD.open(VERSION_FILE, FILE_WRITE);
    if (versionFileCreate) {
      versionFileCreate.println("0");
      versionFileCreate.close();
    }
    lastUsed = 0;
    Serial.println("Versie bestand aangemaakt met laatste versie 0");
  } else {
    String versionStr = versionFileRead.readStringUntil('\n');
    versionFileRead.close();
    int v = versionStr.toInt();
    if (v >= 0) lastUsed = v;
    else lastUsed = 0;
  }
  return lastUsed;
}

// saveLastVersion(): store last used version number
void saveLastVersion(int newLast) {
  // Sla de HUIDIGE versie terug op als "laatst gebruikt"
  File versionFileWrite = SD.open(VERSION_FILE, FILE_WRITE);
  if (versionFileWrite) {
    versionFileWrite.seek(0);          // Go to start of file
    versionFileWrite.println(newLast); // last used = current
    versionFileWrite.close();
  } else {
    Serial.println("Fout: kon versienummer niet bijwerken");
  }
}

// serviceGPS(): read NMEA bytes for a given time and update last valid GPS values
void serviceGPS(unsigned long ms) {
  unsigned long start = millis();

  while (millis() - start < ms) {
    while (GPSSerial.available()) {
      char c = GPSSerial.read();  // Read raw character from GPS
      gps.encode(c);              // Feed it to TinyGPS++ parser
    }

    // Update latitude/longitude when new location is decoded
    if (gps.location.isUpdated()) {
      gps_lat = gps.location.lat();
      gps_lon = gps.location.lng();
    }

    // Update satellite count
    if (gps.satellites.isUpdated()) {
      gps_sats = gps.satellites.value();
    }

    // Update HDOP value
    if (gps.hdop.isUpdated()) {
      gps_hdop = gps.hdop.hdop();
    }

    // Update altitude (meters)
    if (gps.altitude.isUpdated()) {
      gps_altm = gps.altitude.meters();
    }
  }
}
