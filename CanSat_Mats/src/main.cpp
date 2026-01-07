// ----------------------------------------------------------------------------
// CanSat Project 2025 - Team SkyByte
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

// ----------------------------------------------------------------------------
// CanSat Project 2025
// CanSat - BMP280 sensor reading and publishing via MQTT    (no error handling for MQTT)
// CanSat - Data logging to SD card with versioned filenames (no error handling for MQTT)
//        -> (data gets send to MQTT without errors when sd is not available, but slower)
//        -> (data printed to serial when sd is not available, but with error code)
//        -> (uploading code sometimes causes to create 2 (or more) new files, resetting ESP32 causes to create 1 new file (=good))
// CanSat - GPS (TinyGPS++) integration for live position data in CSV
// ----------------------------------------------------------------------------

#include <Adafruit_BMP280.h>     // BMP280
#include <Adafruit_Sensor.h>     // Adafruit unified sensor base
#include <Arduino.h>             // Arduino core
#include <ArduinoMqttClient.h>   // MQTT client
#include <LoRa.h>                // LoRa
#include <SD.h>                  // SD card
#include <SPI.h>                 // SPI bus
#include <TinyGPSPlus.h>         // GPS parsing (TinyGPS++)
#include <WiFi.h>                // ESP32 WiFi
#include <Wire.h>                // I2C
#include <stdio.h>               // snprintf
#include <sys/time.h>            // settimeofday
#include <time.h>                // struct tm, time_t, mktime
#include "Secret.h"              // WIFI + MQTT credentials

// Declarations
void printValues(),writeToSD(String data),saveLastVersion(int newLast),serviceGPS(unsigned long ms); // Create + publish + log CSV line
bool I2C_check(TwoWire *bus, byte address);                                                          // I2C device presence check
int  readLastVersion();                                                                              // Read last used version from VERSION_FILE (create with 0 if missing)

#define MSG_LEN 120

// Sea level pressure (hPa) used for altitude calculation
#define SEALEVELPRESSURE_HPA (1015.8)

// BMP280 I2C address (some boards use 0x77)
#define BMP280_ADR 0x76

// Non-default pins for I2C connection (secondary I2C bus on ESP32)
#define SDA_2 32    // Secondary I2C SDA pin
#define SCL_2 33    // Secondary I2C SCL pin

#define DELAY_TIME 1000 // Delay time between measurements (ms)

// SD Card SPI pins
#define SD_CS_PIN   25    // SD Card CS  = GPIO25
#define SD_MOSI_PIN 23    // SD MOSI     = GPIO23
#define SD_MISO_PIN 19    // SD MISO     = GPIO19
#define SD_SCK_PIN  18    // SD SCK      = GPIO18

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
int currentVersion;            // Version number (saved on SD)
String currentFilename = "";   // Active data filename for this session

// WiFi + MQTT clients (not used for LoRa transmission, kept for code history)
WiFiClient  wifiClient;
MqttClient  mqttClient(wifiClient);

HardwareSerial GPSSerial(2);   // UART2 for GPS (hardware serial 2)
TinyGPSPlus gps;               // TinyGPS++ parser instance

int    gps_sats = 0;           // Last valid satellites count
float  gps_hdop = 0.0;         // Last valid HDOP
double gps_lat  = 0.0;         // Last valid latitude
double gps_lon  = 0.0;         // Last valid longitude
double gps_altm = 0.0;         // Last valid GPS altitude in meters

#define VERSION 1.0

#define DEBUG // define / undefine to enable / disable debug mode
#define START_OF_NEW_TRANSMISSION "START-OF-NEW-TRANSMISSION"

#define csPIN     5  // LoRa chip select pin (SPI)
#define resetPIN  14 // LoRa reset pin
#define irqPIN    2  // LoRa IRQ pin

// --------------- LoRa Settings ---------------
// Frequency setting (for Europe: 868 MHz)
#define LORA_FREQ 868E6

// Address settings
#define SENDER_ADDRESS   0xAA
#define RECEIVER_ADDRESS 0xBB
#define LORA_ID          0x1B

// LoRa parameters
#define TxPower         20      // Transmit power
#define SignalBandwidth 125E3   // Bandwidth
#define CodingRate4     5       // Coding rate 4/5
#define SpreadingFactor 10      // Spreading factor (6–12)
#define CRC             1       // CRC validation enabled
// #define SYNCWORD  0xF3 // Syncword for LoRa transmitter
                       // Same syncword is used at LoRa receiver

// General defines
#define MSG_LEN   150 // Message length of transmitted message
#define DELIMETER ";" // CSV delimiter
#define DELAY_TIME  10000 // Delay time between 2 transmissions

// Define record types
#define MOTOR_RECORD  "M"
#define BMP_RECORD    "B"
#define GPS_RECORD    "G"

// Declare functions
void sendMsg( String outgoing );
void readRPi( String &msg );
void readBMP280( String &msg );
void readGPS( String &msg );
float randomFloatDec2(float minVal, float maxVal );
void setManualDateTime(int year, int month, int day, int hour, int minute, int second);
String getFormattedTime();

int  msgCount = 1; // Transmit message counter

// -------------------------------------------------------------------------
// This is the onetime used setup function
// (1) Start LoRa connection (in Europe)
// (2) Send a START-OF-NEW-TRANSMISSION message to the receiver
// -------------------------------------------------------------------------
void setup() {
  unsigned status;
  unsigned bmp_status; // Holds BMP initialization result

  // Set time: YYYY, MM, DD, HH, MM, SS
  setManualDateTime(2025, 11, 17, 15, 22, 00); // Set manual date/time to 17-11-2025 15:22:00

  randomSeed(esp_random()); // Reset random generator

  // (1) --- USB Serial Monitor start ------------------------------
  Serial.begin( 115200 ); 
  while (!Serial) {
    delay( 10 );
  }
  delay( 200 ); // Short delay to allow printout message on serial monitor
  Serial.println( "=======================================" );
  Serial.print( "Start of Program\tVersion: " );
  Serial.println( VERSION );
  Serial.println( "=======================================" );
  
  // SD card
  Serial.println("----------------------------------------------------------------------");
  Serial.println("\nInitializing SD card...");

  // Initialize the SPI bus with explicit pins
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(100);

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN, SPI)) {
    Serial.println("SD card initialization failed!");
    sd_available = false; // Continue without logging
  } else {
    Serial.println("SD card successfully initialized");

    // Optional: print SD card info (size/type)
    uint64_t cardSize = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.print("SD card size: ");
    Serial.print(cardSize);
    Serial.println(" MB");

    uint8_t cardType = SD.cardType();
    Serial.print("SD card type: ");
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
    Serial.print("Logging data to: ");
    Serial.println(currentFilename);

    saveLastVersion(currentVersion);
  }

  // (1) --- Start LoRa communication -------------------------------
  LoRa.setPins( csPIN, resetPIN, irqPIN );

  // Start LoRa module
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Error: LoRa module start failed!");
    while (1);
  }

  // Long range parameters — low data rate, high sensitivity
  LoRa.setTxPower( TxPower );                   // Transmit power
  LoRa.setSignalBandwidth( SignalBandwidth );   // Bandwidth
  LoRa.setCodingRate4( CodingRate4 );           // Coding rate
  LoRa.setSpreadingFactor( SpreadingFactor );   // Spreading factor
  #if CRC
    LoRa.enableCrc();                           // CRC enabled
  #endif

  Serial.println( "1. => LoRa module successful connected (SPI)" );   
  
  // (2) --- Send Start of new transmission to receiver --------
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

    // Configure oversampling/filter
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
//  
//  (4) Send CSV record though LoRa
//  (5) Print CSV record on Serial monitor
//  (6) delay for 1 sec
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
// This function sends the output CSV record through LoRa
// to the receiver
// -------------------------------------------------------------------------
void sendMsg( String outgoing ) {
  char buffer[MSG_LEN];

  outgoing.toCharArray(buffer, MSG_LEN); // Convert String to char array

  // Transmit msg through LoRa
  LoRa.beginPacket();
  // Add receiver address, sender address, ID and message length to the LoRa header
    LoRa.write(RECEIVER_ADDRESS);
    LoRa.write(SENDER_ADDRESS);
    LoRa.write( LORA_ID );
    LoRa.write( strlen( buffer ) ); // Message length

    // Add message payload as one block
    LoRa.write((const uint8_t*)buffer, strlen( buffer ) );
  
  LoRa.endPacket();

  msgCount++; // Increment the message counter
}

// -------------------------------------------------------------------------
// This function reads values from the RPi Zero (through interupt call)
// and sets Engine mode according input.
// The CSV-record is set to include the desired stage of the engines
// -------------------------------------------------------------------------
void readRPi( String &msg ) {
    int engine1 = random( 0, 2 ); // Simulate engine 1 state (0=off, 1=on)
    int engine2 = random( 0, 2 ); // Simulate engine 2 state (0=off, 1=on)

    msg = String( MOTOR_RECORD ) + DELIMETER + String(engine1) + DELIMETER + String(engine2);
}

// -------------------------------------------------------------------------
// This function reads values from the BME280 Sensor
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
//                               Quality of GPS data
//                               GPS coordinates - latitude, longitude
//                               GPS time in GMT time
//                               GPS altitude
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

float randomFloatDec2(float minVal, float maxVal) {
    int minInt = minVal * 100;   // Scale to integer
    int maxInt = maxVal * 100;

    int value = random(minInt, maxInt + 1);  // Random integer
    return( (float)( value / 100.0 ) );      // Back to float with 2 decimals
}

// ----- Manually set date/time -----
void setManualDateTime(int year, int month, int day, int hour, int minute, int second)
{
    struct tm timeinfo = {};

    timeinfo.tm_year = year - 1900;  // Year since 1900
    timeinfo.tm_mon  = month - 1;    // Month [0..11]
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
        return "00-00-00:00:00:00";   // Fallback
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
    Serial.println("Error: could not write to SD card");
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
  // If it does not exist, it is created with "0"
  int lastUsed = 0;

  File versionFileRead = SD.open(VERSION_FILE, FILE_READ);
  if (!versionFileRead) {
    File versionFileCreate = SD.open(VERSION_FILE, FILE_WRITE);
    if (versionFileCreate) {
      versionFileCreate.println("0");
      versionFileCreate.close();
    }
    lastUsed = 0;
    Serial.println("Version file created with last version 0");
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
  // Store current version as "last used"
  File versionFileWrite = SD.open(VERSION_FILE, FILE_WRITE);
  if (versionFileWrite) {
    versionFileWrite.seek(0);          // Go to start of file
    versionFileWrite.println(newLast); // last used = current
    versionFileWrite.close();
  } else {
    Serial.println("Error: could not update version number");
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