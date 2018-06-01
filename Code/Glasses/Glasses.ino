/*
This example will receive multiple universes via Artnet and control a strip of ws2811 leds via 
Adafruit's NeoPixel library: https://github.com/adafruit/Adafruit_NeoPixel
This example may be copied under the terms of the MIT license, see the LICENSE file for details
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include "FastLED.h"
#include <Wire.h>  // Include Wire if you're using I2C
#include <SFE_MicroOLED.h>
#include <Encoder.h>

//Screen Settings
#define PIN_RESET 15
#define DC_JUMPER 1
MicroOLED oled(PIN_RESET, DC_JUMPER); 

Encoder knob(1, 3);

//Wifi settings
const char* ssid = "esp32devnet";
const char* password = "password";

// LED Strips
const int numLeds = 254; // change for your setup
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)

#define LEFT_DATA_PIN 13
#define LEFT_CLOCK_PIN 12

#define RIGHT_DATA_PIN 0
#define RIGHT_CLOCK_PIN 4


CRGB leftLeds[numLeds];
CRGB rightLeds[numLeds];

// Artnet settings
ArtnetWifi artnet;
const int startUniverse = 0;

bool sendFrame = 1;
int previousDataLength = 0;

// connect to wifi – returns true if successful or false if not
boolean ConnectWifi(void)
{
  boolean state = true;
  int i = 0;

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");
  
  // Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false;
      break;
    }
    i++;
  }
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  
  return state;
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  sendFrame = 1;
  // set brightness of the whole strip 
  if (universe == 15)
  {
    FastLED.setBrightness(data[0]);
  }
  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < numLeds /2)
    {
      rightLeds[led] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
    }
    else
    {  
        leftLeds[led - 170] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
    }
  }
  previousDataLength = length;     
  FastLED.show();
}

void printTitle(String title, int font)
{
  int middleX = oled.getLCDWidth() / 2;
  int middleY = oled.getLCDHeight() / 2;
  
  oled.clear(PAGE);
  oled.setFontType(font);
  // Try to set the cursor in the middle of the screen
  oled.setCursor(middleX - (oled.getFontWidth() * (title.length()/2)),
                 middleY - (oled.getFontWidth() / 2));
  // Print the title:
  oled.print(title);
  oled.display();
  delay(3000);
  oled.clear(PAGE);
}

void mainMenu ()
{
  oled.clear(PAGE);
  oled.setFontType(0);
  oled.setCursor(7, 0);
  oled.print("ArtNet:");
  oled.setCursor(7, 13);
  oled.print("Patterns");
  oled.setCursor(7, 26);
  oled.print("Colors");
  oled.setCursor(7, 39);
  oled.print("Settings");
  wifiStatus();
  oled.setCursor(0, 0);
  oled.write(253);
  oled.display();
  oled.clear(PAGE);
}

//this function will write wifi status to different things based on wht page it's on, maybe pass the page in as an argument?
void wifiStatus()
{
  long rssi = WiFi.RSSI();
  oled.setCursor(49, 0);
  oled.print(rssi);
}

void setup()
{
  Serial.begin(115200);
  
  delay(100);
  oled.begin();    // Initialize the OLED
  oled.clear(ALL); // Clear the display's internal memory
  oled.display();  // Display what's in the buffer (splashscreen)
  delay(1000);     // Delay 1000 ms
  oled.clear(PAGE);
  
  printTitle("REZZ v0.1", 1);
  
  ConnectWifi();
  mainMenu();
  artnet.begin();
  
  FastLED.addLeds<APA102, LEFT_DATA_PIN, LEFT_CLOCK_PIN, BGR>(leftLeds, numLeds);
  FastLED.addLeds<APA102, RIGHT_DATA_PIN, RIGHT_CLOCK_PIN, BGR>(rightLeds, numLeds);
  
  // onDmxFrame will execute every time a packet is received by the ESP32
  artnet.setArtDmxCallback(onDmxFrame);
}

long oldPosition  = -999;

void loop()
{
  // we call the read function inside the loop
  artnet.read();
}
