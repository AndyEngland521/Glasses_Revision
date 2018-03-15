/*
This example will receive multiple universes via Artnet and control a strip of ws2811 leds via 
Adafruit's NeoPixel library: https://github.com/adafruit/Adafruit_NeoPixel
This example may be copied under the terms of the MIT license, see the LICENSE file for details
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include <FastLED.h>

//Wifi settings
/*
const char* ssid = "esp32devnet";
const char* password = "password";

const char* ssid = "NECTARKATZ";
const char* password = "garrettiscuffed";
*/

const char* ssid = "NETGEAR14";
const char* password = "brightplanet943";

// Neopixel settings
const int numLeds = 252; // change for your setup
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
#define DATA_PIN 12
#define CLOCK_PIN 13
CRGB leds[numLeds];

// Artnet settings
ArtnetWifi artnet;
const int startUniverse = 1; // CHANGE FOR YOUR SETUP most software this is 1, some software send out artnet first universe as 0.

// Check if we got all universes
//const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
//bool universesReceived[maxUniverses];
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
    FastLED.show();
  }
//Serial.println(length);
  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  { //Serial.println(universe);
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    //Serial.println(led);
    if (led < numLeds)
    switch(universe){
      case 1:
        leds[led] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
        break;
      case 2:
        leds[led] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
        break;
    }

  }
  previousDataLength = length;     
  FastLED.show();
}

void setup()
{
  Serial.begin(115200);
  ConnectWifi();
  artnet.begin();
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, numLeds);

  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
}

void loop()
{
  // we call the read function inside the loop
  artnet.read();
}
