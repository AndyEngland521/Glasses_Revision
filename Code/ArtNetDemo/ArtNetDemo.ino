/*
This example will receive multiple universes via Artnet and control a strip of ws2811 leds via 
Adafruit's NeoPixel library: https://github.com/adafruit/Adafruit_NeoPixel
This example may be copied under the terms of the MIT license, see the LICENSE file for details
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include <FastLED.h>

//Wifi settings

const char* ssid = "esp32devnet";
const char* password = "password";

// LED Strip
const int numLeds = 120; // change for your setup
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
#define DATA_PIN 12
#define CLOCK_PIN 13
CRGB leds[numLeds];

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
    if (led < numLeds)
    {
      leds[led] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
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
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, GRB>(leds, numLeds);
  
  // onDmxFrame will execute every time a packet is received by the ESP32
  artnet.setArtDmxCallback(onDmxFrame);
}

void loop()
{
  // we call the read function inside the loop
  artnet.read();
}
