// =============================================================================
// Sketch for ESP32DEV Board
// -----------------------------------------------------------------------------
// Test OLED Display
// OLED Display connection: SCK - PIN 22
//                          SCL - PIN 21
// =============================================================================
// F.Demonie  10/4/24
// =============================================================================

#include <Wire.h>
#include <U8g2lib.h>
#include <Arduino.h>

#define MSG_LEN 120

// Select correct OLED Display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =============================================================================
// Setup Function
// -----------------------------------------------------------------------------
// Connect to Serial Monitor
// Connect and initialize OLED-display
// =============================================================================
// F.Demonie  10/4/24
// =============================================================================
void setup() {
    
  // Initialize serial monitor and wait till connection established
  Serial.begin(9600);
  while( !Serial) {
    ;
  }

  Serial.println();
  Serial.println( "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  Serial.println( "Sensor Output LED Stearing - Data Transmission starts!");
  
  // Initialize OLED Display
  u8g2.begin();
  u8g2_prepare();
}

// =============================================================================
// Main loop Function
// -----------------------------------------------------------------------------
// Display message on OLED Display
// =============================================================================
// F.Demonie  10/4/24
// =============================================================================
void loop() {
  char msg[ MSG_LEN ] = "";

  for( int i = 0; i < 10 ; i++ ) {
    sprintf( msg, "%2d: Hallo, 6ICW !", i );
    u8g2_publish_data( msg, i*10 );
    delay( 2000 );
  }

}  

// =============================================================================
// u8g2_prepare Function
// -----------------------------------------------------------------------------
// Set defaults for OLED Display
// =============================================================================
// F.Demonie  3/4/24
// =============================================================================
void u8g2_prepare() {
  u8g2.setFontRefHeightExtendedText(); 
  u8g2.setDrawColor(1); 
  u8g2.setFontPosTop(); 
  u8g2.setFontDirection(0);
}

// =============================================================================
// u8g2_publish_data Function
// -----------------------------------------------------------------------------
// Publish incoming message on OLED Display
// =============================================================================
// F.Demonie  3/4/24
// =============================================================================
void u8g2_publish_data( char *msg, int boxlen ) {
  u8g2.clearBuffer(); 
  u8g2_prepare(); 
  u8g2.drawRFrame( 0,0, 127, 18, 3);
  u8g2.setFont(u8g2_font_6x13_tf );
  u8g2.drawStr( 5, 3, msg );
  
  u8g2.drawBox( 0, 40, boxlen, 20 );

  u8g2.sendBuffer(); 
}