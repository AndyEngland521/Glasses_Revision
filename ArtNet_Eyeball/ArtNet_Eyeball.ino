#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>


#include "SPI.h"
#include "FastLED.h"  

const int number_of_universes = 2;             // set the number of universes in this subnet from 1 - 16
const byte subnet = 0;                         // set the subnet address from 0 - 15
const int number_of_leds_per_universe = 126;    // set the number of LEDs per universe from 1 - 170
const int data_pin = 2;                        // set data pin for LED data output

const char* ssid = "sparkfun-guest";
const char* password = "sparkfun6333";
/*
byte mac_address[] = { 
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06           // set MAC address
}; 

byte ip_address[] = { 
  10, 0, 0, 4                                  // set IP address
};*/ 

const int number_of_leds = number_of_universes * number_of_leds_per_universe; 
CRGB leds[number_of_leds];

uint8_t buffer[530]; 

WiFiUDP Udp;
unsigned int localUdpPort = 5151;

void setup() {  
  Serial.begin(115200);
  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
  FastLED.addLeds<NEOPIXEL, data_pin>(leds, number_of_leds); 
}

void loop() {
  int packetSize = Udp.parsePacket();
  if(packetSize > 0) {
    long st = micros(); 
    Udp.read(buffer, 18);  
    uint8_t incoming_subnet = buffer[14] >> 4;
    uint8_t incoming_universe = buffer[14] & 0x0f; 

    if(incoming_subnet == subnet) {
      Udp.read(buffer, number_of_leds_per_universe * 3);  
      for(int i = 0; i < number_of_leds_per_universe; i ++) {
        leds[i + (incoming_universe * number_of_leds_per_universe)] = CRGB (buffer[i * 3], buffer[(i * 3) + 1], buffer[(i * 3) + 2]);
        Serial.println(leds[i + (incoming_universe * number_of_leds_per_universe)]);
      }
      FastLED.show();  
    }
  }
}
