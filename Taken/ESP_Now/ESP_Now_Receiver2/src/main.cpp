#include <esp_now.h>
#include <WiFi.h>

#define ID_LEN  32

// REPLACE WITH YOUR SENDER & RECEIVER MAC Address
uint8_t senderAddress[] = {0xD4, 0x8A, 0xFC, 0xA2, 0xD2, 0x04};
uint8_t myAddress[]     = {0x1B, 0x69, 0x20, 0xCC, 0xE8, 0x8C};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  char sender_id[ ID_LEN ];
  char receiver_id[ ID_LEN ];
  float temp;
  float humid;
} struct_message;

// Create a struct_message called myData
struct_message Mydata;

bool data_received = false;

void OnDataRecv( const esp_now_recv_info_t *esp_now_info, const uint8_t *In_data, int data_len ) {
  memcpy( &Mydata, In_data, sizeof( Mydata ) );

  if ( ( memcmp( Mydata.sender_id, senderAddress, 6 ) == 0 ) && 
       ( memcmp( Mydata.receiver_id, myAddress, 6 ) == 0 ) ) {
    data_received = true;
  } else {
    data_received = false;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin( 9600 );
  Serial.println( "Start of receiving ESP NOW messages" );
  Serial.println( "+++++++++++++++++++++++++++++++++++" );

  WiFi.mode( WIFI_STA );
  WiFi.disconnect();

  if ( esp_now_init() != ESP_OK ) {
    Serial.println( "Error starting up ESP Now" );
    return;
  }

  esp_now_register_recv_cb( OnDataRecv );
}

void loop() {
  if ( data_received ) {
    // Print sebnder Address
    Serial.print( "Data received from Sender = " );
    for( int i = 0 ; i < 6 ; i++ ) {
      Serial.print( Mydata.sender_id[i], HEX );
      Serial.print( " " );
    }
    Serial.println();
    Serial.print( "Temp: " );
    Serial.println( Mydata.temp );
    Serial.print( "Humid: " );
    Serial.println( Mydata.humid );
    Serial.println();
    data_received = false;
  }
}
