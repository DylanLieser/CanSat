#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi netwerk credentials
//#define SECRET_SSID "Proximus-Home-9270"
//#define SECRET_PASS "w9y9xb64a93zd"

// WiFi instellingen
//#define SECRET_SSID "E109-E110"       // WiFi netwerknaam
//#define SECRET_PASS "DBHaacht24"      // WiFi wachtwoord

// WiFi netwerk credentials
#define SECRET_SSID "CanSat24"
#define SECRET_PASS "DBHaacht24"

// OpenWeatherMap API key
#define apiKey "019b3c6350ea5416d05f4b56216bd8a7"

// U8g2 OLED (I2C, 0x3C adres)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

String city = "";
const String server = "http://api.openweathermap.org/data/2.5/weather";

void getWeather(String cityName);

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Verbinden met WiFi...");
  u8g2.sendBuffer();

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "WiFi Verbonden!");
  u8g2.sendBuffer();

  Serial.println("Geef stad in (bv. Haacht,BE):");
}

void loop() {
  if (Serial.available() > 0) {
    city = Serial.readStringUntil('\n');
    city.trim();
    if (city.length() > 0) {
      getWeather(city);   // hier wordt de functie gebruikt
    }
  }
}

void getWeather(String cityName) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi niet verbonden!");
    return;
  }

  HTTPClient http;
  String url = server + "?q=" + cityName + "&appid=" + apiKey + "&units=metric";
  http.begin(url);
  int code = http.GET();

  if (code > 0) {
    String payload = http.getString();

    JsonDocument doc;      // nieuwe, niet-deprecated manier
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("JSON-fout: ");
      Serial.println(error.c_str());
      return;
    }

    float temp  = doc["main"]["temp"];
    float tmin  = doc["main"]["temp_min"];
    float tmax  = doc["main"]["temp_max"];
    int hum     = doc["main"]["humidity"];
    int press   = doc["main"]["pressure"];
    const char* desc = doc["weather"][0]["description"];

    // Serial
    Serial.printf("\n%s\nTemp: %.1f°C (%.1f / %.1f)\nVocht: %d%%  Druk: %d hPa\nWeer: %s\n",
                  cityName.c_str(), temp, tmin, tmax, hum, press, desc);

    // OLED
    u8g2.clearBuffer();
    u8g2.setCursor(0, 10);  u8g2.print(cityName);
    u8g2.setCursor(0, 22);  u8g2.printf("T:%.1fC (%.1f/%.1f)", temp, tmin, tmax);
    u8g2.setCursor(0, 34);  u8g2.printf("RH:%d%%  P:%dhPa", hum, press);
    u8g2.setCursor(0, 46);  u8g2.print(desc);
    u8g2.sendBuffer();
  } else {
    Serial.printf("HTTP fout: %d\n", code);
  }

  http.end();
}