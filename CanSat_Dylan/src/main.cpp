// ----------------------------------------------------------------------------
// Primary Mission CanSat
// BMP280 (I2C-2) + GPS + CSV to MQTT + SD-backup (versioning) + LoRa TX
// Dylan Lieser 5 611ICW 2025-2026
//
// Features
// - BMP280 sensor reading                                                 
// - GPS module reading                                                    
// - Data logging to SD card (versioned filenames)                          
// - Publishing combined CSV via MQTT                                       
// - Sending data through LoRa (SPI connected)                              
//
// Notes
// (1) Set LoRa sync word the same at transmitter & receiver (if used)
// (2) LoRa frequency is set for Europe (868 MHz)
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

// Sea level pressure used for altitude calculation (hPa)
#define SEALEVELPRESSURE_HPA 1018.2   // Reference pressure at sea level

// BMP280 sensor configuration
#define BMP280_ADDR  0x76             // BMP280 I2C address (some boards use 0x77)
#define SDA_2        32               // Secondary I2C SDA pin (ESP32 I2C bus 2)
#define SCL_2        33               // Secondary I2C SCL pin (ESP32 I2C bus 2)

// GPS configuration (UART)
#define GPS_RX 16                     // ESP32 RX <- GPS TX
#define GPS_TX 17                     // ESP32 TX -> GPS RX
#define GPS_BAUD 9600                 // GPS module baudrate (typical 9600)

// MQTT configuration
#define MQTT_ID "cansat-bmp280"       // MQTT client identifier

// SD card configuration (SPI)
#define SD_CS   25                    // SD card Chip Select pin (GPIO25)
#define SD_SCK  18                    // SD card SPI Clock pin
#define SD_MISO 19                    // SD card SPI MISO pin
#define SD_MOSI 23                    // SD card SPI MOSI pin

// SD file management
#define DATA_FILE_BASE "/CanSatSend"        // Base filename for data logging
#define VERSION_FILE   "/CanSatVersion.txt" // File storing last used log version

// LoRa wiring (same SPI bus, different CS)
#define csPIN     5                   // LoRa chip select pin (SPI)
#define resetPIN  14                  // LoRa reset pin
#define irqPIN    2                   // LoRa IRQ/DIO0 pin

// LoRa settings
#define LORA_FREQ        868E6        // LoRa frequency for Europe (868 MHz)
#define SENDER_ADDRESS   0xAA         // Sender address in LoRa header
#define RECEIVER_ADDRESS 0xBB         // Receiver address in LoRa header
#define LORA_ID          0x1B         // Message group / link ID

// LoRa parameters (range vs speed tradeoff)
#define TxPower         20            // Transmit power (max)
#define SignalBandwidth 125E3         // 125 kHz bandwidth (default)
#define CodingRate4     5             // 4/5 coding rate
#define SpreadingFactor 10            // Spreading factor (6–12, higher = longer range)
#define CRC             1             // CRC check enabled
// #define SYNCWORD  0xF3              // Set Syncword (ensure same at receiver)

// Start marker
#define START_OF_NEW_TRANSMISSION "START-OF-NEW-TRANSMISSION" // Session start marker

// Record types (LoRa payloads)
#define MOTOR_RECORD  "M"             // Motor record identifier
#define BMP_RECORD    "B"             // BMP280 record identifier
#define GPS_RECORD    "G"             // GPS record identifier
#define DELIMETER ";"                 // CSV delimiter

// Timing and buffers
#define DELAY_TIME_MS 1000            // Delay time between measurements (ms)
#define MSG_LEN       180             // CSV/LoRa message buffer size

//Global Objects and Variables
WiFiClient wifiClient;                // WiFi TCP client
MqttClient mqttClient(wifiClient);    // MQTT client over WiFi

TwoWire I2Ctwo = TwoWire(1);          // Secondary I2C bus object on ESP32
Adafruit_BMP280 bmp(&I2Ctwo);         // BMP280 bound to secondary I2C bus

TinyGPSPlus gps;                      // TinyGPS++ parser instance
HardwareSerial GPSSerial(2);          // UART2 for GPS module

bool sd_available = false;            // SD card availability flag
bool lora_ok = false;                 // LoRa availability flag
bool bmp_connected = false;           // BMP280 detected flag

int currentVersion = 0;               // Current log version number
String currentFilename = "";          // Current log filename on SD

// Cached GPS values (stable output when GPS updates are intermittent)
int    gps_sats = 0;                  // Last valid satellites count
float  gps_hdop = 0.0f;               // Last valid HDOP
double gps_lat  = 0.0;                // Last valid latitude
double gps_lon  = 0.0;                // Last valid longitude
double gps_altm = 0.0;                // Last valid GPS altitude in meters

//Function Prototypes
void sendMsg(String outgoing);                                      // Send one LoRa payload
void readRPi(String &msg);                                          // Build motor record (placeholder)
void readBMP280(String &msg);                                       // Build BMP record
void readGPS(String &msg);                                          // Build GPS record

bool I2C_check(TwoWire *bus, byte address);                         // Check if I2C device exists
void serviceGPS(unsigned long ms);                                  // Feed TinyGPS++ for ms duration

int  readLastVersion();                                             // Read last used SD version
void saveLastVersion(int newLast);                                  // Save new SD version
void writeToSD(String data);                                        // Append one line to SD file

void setManualDateTime(int year, int month, int day,
                       int hour, int minute, int second);           // Set ESP32 system clock
String getFormattedTime();                                          // Format system time fallback

// Setup: initialize Serial, SD, BMP280, GPS, WiFi, MQTT, LoRa
void setup() {
  Serial.begin(115200);                                             // Start Serial monitor
  while (!Serial) { delay(10); }                                    // Wait for Serial ready

  setManualDateTime(2025, 11, 17, 15, 22, 0);                       // Optional fallback time

  Serial.println("time_ms;sats;hdop;lon;lat;datetimeUTC;alt_m;hoogte_bmp_m;druk_hPa;temperatuur_C"); // CSV header

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);                       // Start SPI bus (SD style)

  Serial.println("\nInitializing SD card...");                       // SD init info
  if (!SD.begin(SD_CS, SPI)) {                                       // Initialize SD card
    sd_available = false;                                            // Continue without SD
    Serial.println("SD card initialization failed!");
  } else {
    sd_available = true;                                             // SD is available
    Serial.println("SD card successfully initialized");

    int lastUsed = readLastVersion();                                // Read last used version
    currentVersion = lastUsed + 1;                                   // Increment version
    currentFilename = String(DATA_FILE_BASE) + String(currentVersion) + ".txt"; // Build filename
    Serial.print("Logging data to: "); Serial.println(currentFilename);

    saveLastVersion(currentVersion);                                 // Store new version
  }

  I2Ctwo.begin(SDA_2, SCL_2);                                        // Start secondary I2C bus
  if (!bmp.begin(BMP280_ADDR)) {                                     // Try to start BMP280
    bmp_connected = false;                                           // Sensor not found
    Serial.println("Could not find a valid BMP280 sensor, check wiring, address, sensor ID!");
  } else {
    bmp_connected = true;                                            // Sensor found
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,                    // Normal mode
                    Adafruit_BMP280::SAMPLING_X2,                    // Temp oversampling
                    Adafruit_BMP280::SAMPLING_X16,                   // Pressure oversampling
                    Adafruit_BMP280::FILTER_X16,                     // IIR filter
                    Adafruit_BMP280::STANDBY_MS_500);                // Standby time
    Serial.println("CanSat => BMP-280 successful connected (I2C-2)");
  }

  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);             // Start GPS UART2
  Serial.println("GPS module initialized on UART2 @ 9600 baud.");

  WiFi.begin(WIFI_SSID, WIFI_PASS);                                  // Connect to WiFi network
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(1000); } // Wait for WiFi
  Serial.printf("\nWiFi OK, IP = %s\n", WiFi.localIP().toString().c_str());

  mqttClient.setId(MQTT_ID);                                         // Set MQTT client ID
  while (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {              // Connect to MQTT broker
    Serial.print("MQTT fout: "); Serial.println(mqttClient.connectError());
    delay(1000);
  }
  Serial.println("MQTT verbonden.");

  LoRa.setPins(csPIN, resetPIN, irqPIN);                             // Configure LoRa pins
  if (!LoRa.begin(LORA_FREQ)) {                                      // Start LoRa radio
    Serial.println("Error: LoRa module start failed!");
    lora_ok = false;                                                 // Continue without LoRa
  } else {
    LoRa.setTxPower(TxPower);                                        // Set transmit power
    LoRa.setSignalBandwidth(SignalBandwidth);                        // Set bandwidth
    LoRa.setCodingRate4(CodingRate4);                                // Set coding rate
    LoRa.setSpreadingFactor(SpreadingFactor);                        // Set spreading factor
    #if CRC
      LoRa.enableCrc();                                              // Enable CRC
    #endif
    // LoRa.setSyncWord(SYNCWORD);                                   // Optional syncword
    lora_ok = true;
    Serial.println("1. => LoRa module successful connected (SPI)");

    sendMsg(START_OF_NEW_TRANSMISSION);                              // Start marker to receiver
    Serial.printf("\t%s\n", START_OF_NEW_TRANSMISSION);
    Serial.println("2. => Start of new transmission message send to receiver");
    Serial.println();
  }
}

// Loop: read sensors, build combined CSV, publish MQTT, log SD, send LoRa M/B/G
void loop() {
  mqttClient.poll();                                                 // Keep MQTT connection alive

  serviceGPS(200);                                                   // Read GPS for ~200 ms
  bmp_connected = I2C_check(&I2Ctwo, BMP280_ADDR);                    // Re-check BMP280 presence

  // ---- Build combined CSV (MQTT + SD) ----
  unsigned long t = millis();                                        // Time since boot (ms)

  float tempC = 0.0f;                                                // Temperature (°C)
  float druk_hP = 0.0f;                                              // Pressure (hPa)
  float hoogte_bmp = 0.0f;                                           // Altitude from pressure (m)

  if (bmp_connected) {                                               // Only read if sensor is OK
    tempC = bmp.readTemperature();                                   // Read temperature
    druk_hP = bmp.readPressure() / 100.0f;                           // Convert Pa -> hPa
    hoogte_bmp = bmp.readAltitude(SEALEVELPRESSURE_HPA);             // Altitude estimate
  }

  char dateTime[30];                                                 // Date/time string buffer
  if (gps.date.isValid() && gps.time.isValid()) {                     // Prefer GPS UTC time
    snprintf(dateTime, sizeof(dateTime), "%02d/%02d/%04d %02d:%02d:%02d",
             gps.date.day(), gps.date.month(), gps.date.year(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    snprintf(dateTime, sizeof(dateTime), "00/00/0000 00:00:00");     // Dylan-style fallback
  }

  int   sats = gps_sats;                                             // Cached satellites count
  float hdop = gps_hdop;                                             // Cached HDOP
  double lat = gps_lat;                                              // Cached latitude
  double lon = gps_lon;                                              // Cached longitude
  float alt  = (float)gps_altm;                                      // Cached altitude (m)

  char line[MSG_LEN];                                                // Combined CSV line
  snprintf(line, sizeof(line),
           "%lu;%d;%.2f;%.6f;%.6f;%s;%.2f;%.2f;%.2f;%.2f",
           t, sats, hdop, lon, lat, dateTime,
           alt, hoogte_bmp, druk_hP, tempC);

  Serial.println(line);                                              // Print combined CSV

  mqttClient.beginMessage(MQTT_TOPIC);                               // Publish CSV to MQTT
  mqttClient.print(line);
  mqttClient.endMessage();

  if (sd_available) {                                                // Log to SD if available
    writeToSD(String(line));                                         // Store combined CSV line
  }

  //Build and send 3 LoRa records
  String RPi_msg = "";                                               // Motor record string
  String BMP_msg = "";                                               // BMP record string
  String GPS_msg = "";                                               // GPS record string

  readRPi(RPi_msg);                                                  // Build motor record
  readBMP280(BMP_msg);                                               // Build BMP record
  readGPS(GPS_msg);                                                  // Build GPS record

  sendMsg(RPi_msg);                                                  // Send motor record via LoRa
  sendMsg(BMP_msg);                                                  // Send BMP record via LoRa
  sendMsg(GPS_msg);                                                  // Send GPS record via LoRa

  if (sd_available) {                                                // Optional: log LoRa records too
    writeToSD(RPi_msg);
    writeToSD(BMP_msg);
    writeToSD(GPS_msg);
  }

  delay(DELAY_TIME_MS);                                              // Delay between loops
}


// Functions (below loop)
void sendMsg(String outgoing) {
  if (!lora_ok) return;                                              // Skip if LoRa not available

  char buffer[MSG_LEN];                                              // TX buffer
  outgoing.toCharArray(buffer, MSG_LEN);                             // Convert String to char[]

  LoRa.beginPacket();                                                // Begin LoRa packet
  LoRa.write(RECEIVER_ADDRESS);                                      // Receiver address
  LoRa.write(SENDER_ADDRESS);                                        // Sender address
  LoRa.write(LORA_ID);                                               // Link ID
  LoRa.write(strlen(buffer));                                        // Payload length (1 byte in LoRa lib)
  LoRa.write((const uint8_t*)buffer, strlen(buffer));                // Payload bytes
  LoRa.endPacket();                                                  // End and transmit packet
}

void readRPi(String &msg) {
  int engine1 = random(0, 2);                                        // Simulate engine 1 state
  int engine2 = random(0, 2);                                        // Simulate engine 2 state
  msg = String(MOTOR_RECORD) + DELIMETER + String(engine1) + DELIMETER + String(engine2); // M;E1;E2
}

void readBMP280(String &msg) {
  if (!bmp_connected) {                                              // If sensor missing
    msg = String(BMP_RECORD) + DELIMETER + "NA" + DELIMETER + "NA" + DELIMETER + "NA";
    return;
  }

  float temp = bmp.readTemperature();                                // °C
  float press = bmp.readPressure() / 100.0F;                         // hPa
  float alt = bmp.readAltitude(SEALEVELPRESSURE_HPA);                // m

  msg = String(BMP_RECORD) + DELIMETER;
  msg = msg + String(temp, 2) + DELIMETER;
  msg = msg + String(press, 2) + DELIMETER;
  msg = msg + String(alt, 2);
}

void readGPS(String &msg) {
  String date_time = getFormattedTime();                             // Default from system time

  if (gps.date.isValid() && gps.time.isValid()) {                    // Prefer GPS time if valid
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d-%02d-%02d:%02d:%02d:%02d",
             gps.date.day(), gps.date.month(), (gps.date.year() % 100),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    date_time = String(buf);
  }

  int   nr_sats  = gps_sats;                                         // Cached sats
  float hdoop    = gps_hdop;                                         // Cached hdop
  float latitude = (float)gps_lat;                                   // Cached lat
  float longitude= (float)gps_lon;                                   // Cached lon
  float altitude = (float)gps_altm;                                  // Cached alt

  msg = String(GPS_RECORD) + DELIMETER;
  msg = msg + String(nr_sats) + DELIMETER;
  msg = msg + String(hdoop, 2) + DELIMETER;
  msg = msg + String(latitude, 6) + DELIMETER;
  msg = msg + String(longitude, 6) + DELIMETER;
  msg = msg + date_time + DELIMETER;
  msg = msg + String(altitude, 2);
}

bool I2C_check(TwoWire *bus, byte address) {
  bus->beginTransmission(address);                                   // Start I2C transmission
  byte error = bus->endTransmission();                               // End transmission
  return (error == 0);                                               // 0 means ACK
}

void serviceGPS(unsigned long ms) {
  unsigned long start = millis();                                    // Start time marker

  while (millis() - start < ms) {                                    // Run for 'ms' milliseconds
    while (GPSSerial.available()) {                                  // Read all available GPS bytes
      char c = GPSSerial.read();                                     // Read raw char
      gps.encode(c);                                                 // Feed to TinyGPS++
    }

    if (gps.location.isUpdated()) {                                  // Update coordinates if new
      gps_lat = gps.location.lat();
      gps_lon = gps.location.lng();
    }
    if (gps.satellites.isUpdated()) gps_sats = gps.satellites.value(); // Update satellites
    if (gps.hdop.isUpdated())       gps_hdop = gps.hdop.hdop();        // Update hdop
    if (gps.altitude.isUpdated())   gps_altm = gps.altitude.meters();  // Update altitude
  }
}

int readLastVersion() {
  int lastUsed = 0;                                                  // Default if missing/invalid

  File versionFileRead = SD.open(VERSION_FILE, FILE_READ);           // Try to open version file
  if (!versionFileRead) {                                            // Create if not found
    File versionFileCreate = SD.open(VERSION_FILE, FILE_WRITE);
    if (versionFileCreate) {
      versionFileCreate.println("0");                                // Initialize version to 0
      versionFileCreate.close();
    }
    lastUsed = 0;
    Serial.println("Version file created with last version 0");
  } else {
    String versionStr = versionFileRead.readStringUntil('\n');       // Read version line
    versionFileRead.close();
    int v = versionStr.toInt();                                      // Convert to int
    lastUsed = (v >= 0) ? v : 0;                                     // Clamp invalid values
  }
  return lastUsed;                                                   // Return last used version
}

void saveLastVersion(int newLast) {
  File versionFileWrite = SD.open(VERSION_FILE, FILE_WRITE);         // Open version file for write
  if (versionFileWrite) {
    versionFileWrite.seek(0);                                        // Go to start of file
    versionFileWrite.println(newLast);                               // Save version value
    versionFileWrite.close();
  } else {
    Serial.println("Error: could not update version number");
  }
}

void writeToSD(String data) {
  File dataFile = SD.open(currentFilename, FILE_APPEND);             // Open current file append mode
  if (dataFile) {
    dataFile.println(data);                                          // Write line
    dataFile.close();                                                // Close file
  } else {
    Serial.println("Error: could not write to SD card");
  }
}

void setManualDateTime(int year, int month, int day, int hour, int minute, int second) {
  struct tm timeinfo = {};                                           // Init time struct

  timeinfo.tm_year = year - 1900;                                    // Years since 1900
  timeinfo.tm_mon  = month - 1;                                      // Month [0..11]
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = hour;
  timeinfo.tm_min  = minute;
  timeinfo.tm_sec  = second;

  time_t t = mktime(&timeinfo);                                      // Convert to epoch
  struct timeval now = { .tv_sec = t };                              // Wrap epoch
  settimeofday(&now, NULL);                                          // Set system time
}

String getFormattedTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "00-00-00:00:00:00";                                      // Fallback
  }

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d-%m-%y:%H:%M:%S", &timeinfo);  // Format date/time
  return String(buffer);
}