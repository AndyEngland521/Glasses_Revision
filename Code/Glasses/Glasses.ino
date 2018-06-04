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
#include "ESPRotary.h";

/////////////////////////////////////////////////////////////////

#define ROTARY_PIN1 15
#define ROTARY_PIN2 5

/////////////////////////////////////////////////////////////////

ESPRotary r = ESPRotary(ROTARY_PIN1, ROTARY_PIN2);

//Screen Settings
#define PIN_RESET 16
#define DC_JUMPER 1
MicroOLED oled(PIN_RESET, DC_JUMPER); 

//menuState selects which menu we are in (main, artnet, etc) while location tells where in each menu we are
//menuSlotTotal tells us how menu available menulocation slots we have in each menu
int8_t modeSelect = 0;
int8_t menuState = 0;
int8_t menuLocation = 0;
int8_t menuSlotTotal[5] = {4, 2, 1, 2, 2};
uint16_t oldButton = 0;

//Wifi settings
const char* ssid = "NETGEAR14";
const char* password = "brightplanet943";

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
  //Serial.println("");
  //Serial.println("Connecting to WiFi");
  
  // Wait for connection
  //Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
    if (i > 20){
      state = false;
      break;
    }
    i++;
  }
  if (state){
    //Serial.println("");
    //Serial.print("Connected to ");
    //Serial.println(ssid);
    //Serial.print("IP address: ");
    //Serial.println(WiFi.localIP());
  } else {
    //Serial.println("");
    //Serial.println("Connection failed.");
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

void fadeall() 
{ 
  for(int i = 0; i < numLeds; i++) 
  { 
    leftLeds[i].nscale8(250);
    rightLeds[i].nscale8(250); 
  } 
}

void cylon ()
{
  static uint8_t hue = 0;
  Serial.print("x");
  // First slide the led in one direction
  for(int i = 0; i < numLeds / 2; i++) {
    // Set the i'th led to red 
    leftLeds[i] = CHSV(hue++, 255, 255);
    rightLeds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show(); 
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
  Serial.print("x");

  // Now go in the other direction.  
  for(int i = (numLeds / 2)-1; i >= 0; i--) {
    // Set the i'th led to red 
    leftLeds[i] = CHSV(hue++, 255, 255);
    rightLeds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
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

//this function will write wifi status to different things based on wht page it's on, maybe pass the page in as an argument?
int wifiStatus()
{
  long rssi = WiFi.RSSI();
  if (rssi = 31) {
    return 0;
  }
  else
  {
    return -rssi;
  }
}

void checkButton ()
{
  uint16_t button = analogRead(A0);
 
  if (button != oldButton)
  {
    if (button > 525 && oldButton < 100)
    {
      menuLocation++;
    }
    else if (button > 400 && oldButton < 100)
    {
      changeMenuState();
    }
    if (menuLocation >= menuSlotTotal[menuState])
    {
      menuLocation = 0;
    }
    if (menuLocation < 0)
    {
      menuLocation = menuSlotTotal[menuState] - 1;
    }
    drawMenu();
    oldButton = button;
  }
  delay(10);
}

void changeMenuState ()
{
  switch (menuState) 
  {
    case 0:
      switch (menuLocation) 
      {
        case 0:
          menuState = 1;
          break;
        case 1:
          menuState = 2;
          break;
        case 2:
          menuState = 3;
          break;
        case 3:
          menuState = 4;
          break;
      
      }
      break;
    case 1:
      switch (menuLocation)
      {
        case 0:
          ConnectWifi();
          break;
        case 1:
          modeSelect = 2;
      }
    break;
    case 2:
      switch (menuLocation)
      {
        case 0:
          modeSelect = 1;
          break;
      }
    break;
    case 3:
      switch (menuLocation)
      {
        case 0:
          menuState = 0;
          break;
      }
    break;
    case 4:
      switch (menuLocation)
      {
        case 0:
          menuState = 0;
          break;
      }
    break;    
  }
}

void drawMenu() 
{ 
  oled.clear(PAGE);
  header();
  switch (menuState)
  {
    case 0:
      mainMenu();
      break;      
    case 1:
      artnetMenu();
      break;
    case 2:
      patternMenu();
      break;
    case 3:
      colorMenu();
      break;
    case 4:
      settingsMenu();
      break; 
  }
  oled.display();
  oled.clear(PAGE);
}

void header ()
{
  oled.setFontType(0);
  oled.setCursor(0, 0);
  oled.print("EYEZ");
  oled.setFontType(0);
  switch(modeSelect) {
    case 0:
      oled.setCursor(46, 0);
      oled.print("Off");
      break;
    case 1:
      oled.setCursor(40, 0);
      oled.print("Cyln");
      break;
    case 2:
      oled.setCursor(46, 0);
      oled.print("DMX");
      break;
  }
}

void drawIndicator (int offset)
{
  int yPosition = 11 + (10 * menuLocation) + offset;
  oled.setCursor(0, yPosition);
  oled.write(253);
}

void mainMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("ArtNet");
  oled.setCursor(7, 21);
  oled.print("Patterns");
  oled.setCursor(7, 31);
  oled.print("Colors");
  oled.setCursor(7, 41);
  oled.print("Settings");
  drawIndicator(0);
}

void artnetMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("Str:");
  oled.print(wifiStatus());
  oled.setCursor(7, 21);
  if (WiFi.status() == WL_CONNECTED)
  {
    oled.print(WiFi.localIP());
  }
  else
  {
    oled.print("Connect?");
  }
  oled.setCursor(7, 31);
  oled.print("Use?");
  drawIndicator(10);
}

void patternMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("Cylon");
  drawIndicator(0);
}

void colorMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("HSV");
  oled.setCursor(7, 21);
  oled.print("RGB");
  drawIndicator(0);
}

void settingsMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("SSID");
  oled.setCursor(7, 21);
  oled.print("Password");
  drawIndicator(0);
}

void splashScreen ()
{ 
  oled.invert(true);
  oled.setFontType(1);
  oled.setCursor(16, 10);
  oled.print("EYEZ");
  oled.setFontType(0);
  oled.setCursor(0, 40);
  oled.print("V0.1");
  oled.display();
  oled.clear(PAGE);
  delay(750);
  oled.invert(false);
}

void off()
{
  for (int led = 0; led < 126; led++)
  {
    leftLeds[led] = CRGB::Black;
    rightLeds[led] = CRGB::Black;
  }
  FastLED.show();
}

void setup()
{
  //Serial.begin(115200);
  
  delay(100);
  oled.begin();    // Initialize the OLED
  oled.clear(ALL); // Clear the display's internal memory
  oled.display();  // Display what's in the buffer (splashscreen)
  delay(500);     // Delay 1000 ms
  oled.clear(PAGE);
  splashScreen();
  drawMenu();
  
  FastLED.addLeds<APA102, LEFT_DATA_PIN, LEFT_CLOCK_PIN, BGR>(leftLeds, numLeds);
  FastLED.addLeds<APA102, RIGHT_DATA_PIN, RIGHT_CLOCK_PIN, BGR>(rightLeds, numLeds);
  artnet.begin();
  // onDmxFrame will execute every time a packet is received by the ESP32
  artnet.setArtDmxCallback(onDmxFrame);
}

void loop()
{
  switch (modeSelect)
  {
    case 0:
      off();
      break;
    case 1:
      cylon();
      break;
    case 2:
      artnet.read();
  }
  checkButton();
}
