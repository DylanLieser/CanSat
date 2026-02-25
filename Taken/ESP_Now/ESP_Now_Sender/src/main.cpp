// Simple program to demonstrate ESP NOW Protocol
// This code sends messages to a number of receivers
// using the ESP NOW protocol
//
// F.Demonie 23/2/25
// =======================================================
#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>

#define ID_LEN      32
#define ADDRES_LEN  6
#define NR_RCVS     2

// REPLACE WITH YOUR SENDER & RECEIVER MAC Address
uint8_t SendFromAddress[] = {0xD4, 0x8A, 0xFC, 0xA2, 0xD2, 0x04};
uint8_t SendToAddress[NR_RCVS][ADDRES_LEN]   = { 
  { 0x1C, 0x69, 0x20, 0xCC, 0xE8, 0x8C },
  { 0xA8, 0x42, 0xE3, 0xAB, 0xC6, 0x5C } 
};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  char  sender_id[ ID_LEN ];
  char  receiver_id[ ID_LEN ];
  float temp;
  float humid;
} struct_message;

// Create a struct_message called myData
struct_message myData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println();
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(9600);
 
  // Set device as a Wi-Fi Station in order to be able to use the ESP*NOW protocoll
  WiFi.mode(WIFI_STA);     // <-- VERPLICHT
  WiFi.disconnect();       // (aan te raden)

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send ESP to
  // get the status of Transmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peers
  for( int i = 0 ; i < NR_RCVS ; i ++ ) {
    memcpy(peerInfo.peer_addr, SendToAddress[i], 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
  
    // Add peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }
  }
}
 
void loop() {
  int receiver = 0;
  int count    = 1;

  while( true ) {
    if ( receiver >= NR_RCVS ) {
      receiver = 0; // reset to first receiver
    }

    // Set some random fake temp an humid
    myData.temp = random(0,50) + (float)random(0,50) /100;
    myData.humid = random( 0,100) + (float)random(0,100) /100;

    // set sender & receiver MAC-adress
    memcpy( myData.sender_id, SendFromAddress, 6 );
    memcpy( myData.receiver_id, SendToAddress[ receiver ], 6 );

    // Send message via ESP-NOW to receivers
    esp_err_t result = esp_now_send(SendToAddress[receiver], (uint8_t *) &myData, sizeof(myData));
  
    // Validate send success / failure
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
  
    // Print message
    Serial.print( "Package Nr: " );
    Serial.print( count ); 

    Serial.print( "\tFrom: " );
    for( int i = 0 ; i < 6 ; i++ ) {
      Serial.print( myData.sender_id[i], HEX );
      Serial.print( " " );
    }

    Serial.print( "\tTo: " );
    for( int i = 0 ; i < 6 ; i++ ) {
      Serial.print( myData.receiver_id[i], HEX );
      Serial.print( " " );
    }

    Serial.print( "\t" );
    Serial.print( myData.temp );
    Serial.print( "\t" );
    Serial.println( myData.humid );
  
    receiver++; // Increase receiver counter
    count++; // Increase messqge counter

    // Delay 5sec before sending next message
    delay(5000);
  }
}