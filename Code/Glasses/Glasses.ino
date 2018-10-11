#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include "FastLED.h"
#include <Wire.h>  // Include Wire if you're using I2C
#include <SFE_MicroOLED.h> //Make sure to edit Wire.begin in hardware.cpp when changing platforms
#include "Splash.h"
#include <ESPRotary.h>
#include <EEPROM.h>

#define ROTARY_PIN1 32
#define ROTARY_PIN2 14
#define buttonPin1 27
#define buttonPin2 33
#define PIN_RESET 13 //OLED
#define DC_JUMPER 1 //OLED
#define LEFT_DATA_PIN 21
#define RIGHT_DATA_PIN 17
#define CLOCK_PIN 16

ESPRotary rotary = ESPRotary(ROTARY_PIN1, ROTARY_PIN2, 4);
MicroOLED oled(PIN_RESET, DC_JUMPER);//Change hardware.cpp to initialize proper wire pins for esp32 WROOOM(23,22), may change for sparkfun wroom

//Menus (Consider a struct once things are nailed down)
enum colorMode {
  Off,
  DMX,
  Cylon,
  Ripple
};
enum menu {
  Main = 0,
  ArtNet = 1,
  Patterns = 2,
  Colors = 3,
  Settings = 4,
  StringEdit = 5,
  HSVEdit = 6,
  RGBEdit = 7
};
colorMode pattern = Off;
menu page = Main;
uint8_t menuLocation = 0; //Tracks location in a menu
uint8_t menuSlotTotal[5] = {4, 2, 2, 2, 2};

uint8_t asciiChar = 0;
uint8_t charPosition = 0;//consider changing to "editPosition"
bool editChar = false;//Consider changing to one variable? "edit"
bool editColor = false;
uint16_t button1 = 0;
uint16_t button2 = 0;
long encoder = 0;

//Wifi settings
char ssid[] = "NECTARKATZ";
char password[] = "garrettiscuffed";
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4);
char* editString;
const int numLeds = 252; // change for your setup
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)

//LED's and Map
CRGB leftLeds[numLeds];
CRGB rightLeds[numLeds];
uint8_t leftEyeMap[5][255];
uint8_t rightEyeMap[5][255];
const uint8_t circleNum[5] = {4, 16, 25, 36, 45};
const float stepsPerRow[5] = {64, 16, 10.24, 7.11111111, 5.6888888};
uint8_t numPrev[5] = {0, 4, 20, 45, 81};
//Value to rotate around circle
uint8_t rotation = 0;

// Artnet settings
ArtnetWifi artnet;
const int startUniverse = 0;

bool sendFrame = 1;
int previousDataLength = 0;

//Preset Colors
CRGB red = CRGB::Red;
CRGB orange = CRGB::Orange;
CRGB yellow = CRGB::Yellow;
CRGB green = CRGB::Green;
CRGB cyan = CRGB::Cyan;
CRGB blue = CRGB::Blue;
CRGB magenta = CRGB::Magenta;
CRGB gray = CRGB::Gray;
CRGB black = CRGB::Black;

//Palette Stuff
CRGBPalette16 currentPalette =
{
  red, yellow, orange, red,
  magenta, blue, green, black,
  gray, yellow, orange, red,
  magenta, blue, green, black
};
TBlendType currentBlending = LINEARBLEND;

void mapEye () //we map LED's to a 360 degree circle where 360 == 255
{
  uint8_t centerOffset;
  uint8_t leftAngleOffset;
  uint8_t rightAngleOffset;
  for (int row = 0; row < 5; row++)
  {
    if (row == 0)
    {
      centerOffset = -32;
    }
    else
    {
      centerOffset = 0;
    }
    for (int i = 0; i < circleNum[row]; i++)
    {
      for (int j = round(i * stepsPerRow[row]); j < round((i + 1) * stepsPerRow[row]); j++)
      {
        leftAngleOffset = j - round((stepsPerRow[row] / 2.0) + centerOffset - 16);
        rightAngleOffset = j - round((stepsPerRow[row] / 2.0) + centerOffset - 84);
        leftEyeMap[row][leftAngleOffset] = i + numPrev[row];
        rightEyeMap[row][rightAngleOffset] = i + numPrev[row];
      }
    }
  }
}

void setRowAngle (uint8_t row, uint8_t angle, CRGB color)
{
  leftLeds[leftEyeMap[row][angle]] = color;
  rightLeds[rightEyeMap[row][angle]] = color;
}

void setAngle (uint8_t angle, CRGB color)
{
  for (int row = 0; row < 5; row++)
  {
    setRowAngle(row, angle, color);
  }
}

//This sets a circle or row to one color.
void setRow(uint8_t row, CRGB color)
{
  uint8_t numPrev;
  if (row == 0)
  {
    numPrev = 0;
  }
  else
  {
    numPrev = circleNum[row - 1];
  }
  for (int ledPosition = numPrev; ledPosition < numPrev + circleNum[row]; ledPosition++)
  {
    leftLeds[ledPosition] = color;
    rightLeds[ledPosition] = color;
  }
}

// connect to wifi – returns true if successful or false if not
boolean ConnectWifi(void)
{
  boolean state = true;
  int i = 0;

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) 
  {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    wifiLoading();
    if (i > 20) {
      state = false;
      break;
    }
    i++;
  }
  drawMenu();

  return state;
}

//this function will write wifi status to different things based on what page it's on, maybe pass the page in as an argument?
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

/*
 * Button / Rotary Handling
 */

void checkRotary (ESPRotary& rotary)
{
  if (page != StringEdit && page != HSVEdit && page != RGBEdit)//fix this so this block is at end of switch statement
  {
    menuLocation = rotary.getPosition() % menuSlotTotal[page];
  }
  else
  {
    if (editColor == true)
    {
      switch(menuLocation)
      {
        case 0:
        uint8_t colorValue = currentPalette[0];
        Serial.println(currentPalette[1]);
        currentPalette[1] = colorValue;
        Serial.println(currentPalette[1]);
      }
    }
    //Code for editing characters
    /*
    if (editChar == false)
    {
      charPosition = rotary.getPosition() % (strlen(editString) + 1);
    }
    else
    {
      editString[charPosition] = abs(rotary.getPosition() % 96) + 32;
    }*/
  }
  drawMenu();
}

void encoderCheck ()
{
  Serial.println("ROTARY");
  rotary.loop();
}

void checkButton ()
{

  button1 = digitalRead(buttonPin1);
  button2 = digitalRead(buttonPin2);
  delay(20);
  if (button1 == true)
  {
    selectButton();
  }
  if (button2 == true)
  {
    backButton();
  }
  drawMenu();
}

void selectButton ()
{
  switch (page)
  {
    case Main:
      switch (menuLocation)
      {
        case 0:
          page = ArtNet;
          menuLocation = 0;
          break;
        case 1:
          page = Patterns;
          menuLocation = 0;
          break;
        case 2:
          page = Colors;
          menuLocation = 0;
          break;
        case 3:
          page = Settings;
          menuLocation = 0;
          break;
      }
      break;
    case ArtNet:
      switch (menuLocation)
      {
        case 0://DMX/ConnectWiFi
          if (WiFi.status() == WL_CONNECTED)
          {
            pattern = DMX;
          }
          else
          {
            ConnectWifi();
          }
          break;
        case 1:
          pattern = Off;
          break;
      }
      break;
    case Patterns:
      switch (menuLocation)
      {
        case 0:
          pattern = Cylon;
          break;
        case 1:
          pattern = Ripple;
          break;
      }
      break;
    case Colors://colors
      switch (menuLocation)
      {
        case 0://Edit HSV
          page = HSVEdit;
          menuLocation = 0;
          break;
        case 1:       
          page = RGBEdit;
          menuLocation = 0;
          break;
      }
      break;
    case Settings://Do I even need this menu? we'll see
      switch (menuLocation)
      {
        case 0:
        case 1:
          break;
      }
      break;
    case RGBEdit://color editing    
      //code to edit characters in a string using rotary
      if (editColor == true)
      {
        rotary.setPosition(editString[charPosition]);
      }
      else
      {
        rotary.resetPosition();
      }
      break;
    case HSVEdit:
      break;
    case StringEdit:
      editChar = !editChar;
      if (editChar == true)
      {
        rotary.setPosition(editString[charPosition]);
      }
      else
      {
        rotary.resetPosition();
      }
      break;
  }
}

void backButton ()
{
  if (page == HSVEdit || RGBEdit)
  {
    //add stuff, save to eeprom here?
  }
  else if (page == StringEdit)
  {
    //what happens if i press button?
  }
  else
  {
    page = Main;
    menuLocation = 0;
  }
}

/*
 * Menu Drawing
 */

void drawMenu()
{
  oled.clear(PAGE);
  header();
  switch (page)
  {
    case Main:
      mainMenu();
      break;
    case ArtNet:
      artnetMenu();
      break;
    case Patterns:
      patternMenu();
      break;
    case Colors:
      colorMenu();
      break;
    case Settings:
      settingsMenu();
      break;
    case StringEdit:
      changeStringMenu();
      break;
    case HSVEdit:
      //Pass "hsv" string into editColor menu
      break;
    case RGBEdit:
      //editColorMenu("RGB");
      break;
  }
  oled.display();
}

void mainMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("ArtNet");
  Serial.println("ArtNet");
  oled.setCursor(7, 21);
  oled.print("Patterns");
  Serial.println("Patterns");
  oled.setCursor(7, 31);
  oled.print("Colors");
  Serial.println("Colors");
  oled.setCursor(7, 41);
  oled.print("Settings");
  Serial.println("Settings");
  drawIndicator(0);
}

void artnetMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  if (WiFi.status() == WL_CONNECTED)
  {
    String wifi = WiFi.localIP().toString().substring(8, 13);
    oled.print("IP: ");
    Serial.print("IP: ");
    oled.print(wifi);
    Serial.println(wifi);
  }
  else
  {
    oled.print("Connect?");
    Serial.println("Connect?");
  }
  oled.setCursor(7, 21);
  oled.print("Off");
  Serial.print("Off");
  drawIndicator(0);
}

void patternMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("Cylon");
  Serial.println("Cylon");
  oled.setCursor(7, 21);
  oled.print("Spin");
  Serial.println("Spin");
  drawIndicator(0);
}

void colorMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("HSV");
  Serial.println("HSV");
  oled.setCursor(7, 21);
  oled.print("RGB");
  Serial.println("RGB");
  drawIndicator(0);
}

void settingsMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("SSID");
  Serial.println("SSID");
  oled.setCursor(7, 21);
  oled.print("Password");
  Serial.println("Password");
  drawIndicator(0);
}

void changeStringMenu ()
{
  oled.setFontType(0);
  oled.setCursor(0, 11);
  switch (menuLocation)
  {
    case 0:
      editString = ssid;
      oled.print("SSID");
      Serial.println("SSID");
      break;
    case 1:
      editString = password;
      oled.print("Password");
      Serial.println("Password");
      break;
  }
  oled.setCursor(0, 21);
  for (int i = 0; i < strlen(editString) + 1; i++) {
    if (i == charPosition)
    {
      oled.setColor(BLACK);
      oled.write((int)editString[i]);
      Serial.print("A");
      Serial.print(editString[i]);
      oled.setColor(WHITE);
    }
    else
    {
      oled.setColor(WHITE);
      oled.write((int)editString[i]);
      Serial.print(editString[i]);
    }
  }
}

void wifiLoading()
{
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.print("CONNECTING");
  Serial.print("CONNECTING");
  oled.display();
}

void header ()
{
  oled.setFontType(0);
  oled.setCursor(0, 0);
  oled.print("EYEZ");
  Serial.print("EYEZ    ");
  oled.setFontType(0);
  switch (pattern) {
    case Off:
      oled.setCursor(46, 0);
      oled.print("Off");
      Serial.println("Off");
      break;
    case Cylon:
      oled.setCursor(40, 0);
      oled.print("Cyln");
      Serial.println("CYLN");
      break;
    case DMX:
      oled.setCursor(46, 0);
      oled.print("DMX");
      Serial.println("DMX");
      break;
    case Ripple:
      oled.setCursor(40, 0);
      oled.print("Spin");
      Serial.println("SPIN");
      break;
  }
}

void drawIndicator (int offset)
{
  uint8_t yPosition = 11 + (10 * menuLocation) + offset;
  oled.setCursor(0, yPosition);
  oled.write(253);
  Serial.println(menuLocation);
}


void splashScreen ()
{
  oled.invert(true);
  oled.clear(PAGE);
  oled.drawBitmap(Splashscreen);
  oled.display();
  delay(1500);
  oled.invert(false);
}

/*
 *Color Patterns 
 */

void off()
{
  for (int led = 0; led < 126; led++)
  {
    leftLeds[led] = CRGB::Black;
    rightLeds[led] = CRGB::Black;
  }
  FastLED.show();
  delay(5);
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
    if (led < numLeds / 2)
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
  for (int i = 0; i < numLeds; i++)
  {
    leftLeds[i].nscale8(250);
    rightLeds[i].nscale8(250);
  }
}

void cylon ()
{
  static uint8_t hue1 = 0;
  static uint8_t hue2 = 0;
  // First slide the led in one direction
  for (int i = 0; i < numLeds / 2; i++) {
    // Set the i'th led to red
    leftLeds[i] = CHSV(hue1++, 255, 255);
    rightLeds[i] = CHSV(hue2--, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }

  // Now go in the other direction.
  for (int i = (numLeds / 2) - 1; i >= 0; i--) {
    // Set the i'th led to red
    leftLeds[i] = CHSV(hue1++, 255, 255);
    rightLeds[i] = CHSV(hue2--, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
}

void gradientSpin ()
{
  for (int i = 0; i < 256; i++)
  {
    uint8_t gradientPosition = i + rotation;
    setAngle(i, ColorFromPalette(currentPalette, gradientPosition, 255, currentBlending));
  }
  rotation++;
  FastLED.show();
  //delay(10);
}

void setup()
{
  delay(1000);
  EEPROM.begin(512);
  Serial.begin(115200);
  FastLED.addLeds<APA102, LEFT_DATA_PIN, CLOCK_PIN, BGR>(leftLeds, numLeds);
  FastLED.addLeds<APA102, RIGHT_DATA_PIN, CLOCK_PIN, BGR>(rightLeds, numLeds);
  //FastLED.setBrightness(195);//Waiting on ring tests for max brightness
  FastLED.setBrightness(32);
  off();
  mapEye();
  
  //Rotary Encoder
  pinMode(ROTARY_PIN1, INPUT_PULLUP);
  pinMode(ROTARY_PIN2, INPUT_PULLUP);
  digitalWrite(ROTARY_PIN1, HIGH);
  digitalWrite(ROTARY_PIN2, HIGH);
  rotary.setChangedHandler(checkRotary);
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN1), encoderCheck, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN2), encoderCheck, CHANGE);
  
  //Buttons
  pinMode(buttonPin1, INPUT);  
  pinMode(buttonPin2, INPUT);
  digitalWrite(buttonPin1, LOW);
  digitalWrite(buttonPin2, LOW);
  attachInterrupt(digitalPinToInterrupt(buttonPin1), checkButton, RISING);  
  attachInterrupt(digitalPinToInterrupt(buttonPin2), checkButton, RISING);
  
  //Start Screen (get rid of connectWiFi? Shorten timeout?)
  oled.begin();
  oled.clear(ALL);
  oled.display();
  delay(500);
  splashScreen();
  ConnectWifi();
  drawMenu();
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
}

void loop()
{
  switch (pattern)
  {
    case Off:
      off();
      break;
    case Cylon:
      cylon();
      break;
    case ArtNet:
      artnet.read();
      break;
    case Ripple://gotta fix this when I add more patterns
      gradientSpin();
      break;
  }
}
