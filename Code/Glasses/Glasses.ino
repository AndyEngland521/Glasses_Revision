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
#define PIN_RESET 13 //oled
#define DC_JUMPER 1 //oled
#define LEFT_DATA_PIN 21
#define RIGHT_DATA_PIN 17
#define CLOCK_PIN 16

ESPRotary rotary = ESPRotary(ROTARY_PIN1, ROTARY_PIN2, 4);
MicroOLED oled(PIN_RESET, DC_JUMPER);//Change hardware.cpp to initialize proper wire pins for esp32 WROOOM(23,22), may change for sparkfun wroom

void off();
void artRead();
void cylon();
void gradientSpin();
void classicRezz();
void rainbowRezz();
void redStatic();
void rainbowStatic();
void randomRipple();
void redWobble();
void placeHolder2();
void placeHolder3();
void placeHolder4();
void placeHolder5();


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

CRGBPalette16 currentPalette =
{
  red, yellow, orange, red,
  magenta, blue, green, black,
  gray, yellow, orange, red,
  magenta, blue, green, black
};
TBlendType currentBlending = LINEARBLEND;

//string oledString;
struct PatternSettings {
  String oledString;
  void (*colorFunction)();
  CRGB color;
  CRGBPalette16 palette;
};

PatternSettings patterns[14] =
{
  {"Off", off, CRGB::Black, currentPalette},
  {"DMX", artRead, CRGB::Black, currentPalette},
  {"Cylon", cylon, CRGB::Black, currentPalette},
  {"Spin", gradientSpin, CRGB::Black, currentPalette},
  {"R-Clsc", classicRezz, CRGB::Red, currentPalette},
  {"RGB-Clsc", rainbowRezz, CRGB::Black, currentPalette},
  {"R-Strb", redStatic, CRGB::Red, currentPalette},
  {"RGB-Strb", rainbowStatic, CRGB::Black, currentPalette},
  {"Ripple", randomRipple, CRGB::Black, currentPalette},
  {"Red Wbbl", redWobble, CRGB::Red, currentPalette},
  {"I/O Rpple", placeHolder2, CRGB::Red, currentPalette},
  {"Radioactv", placeHolder3, CRGB::Green, currentPalette},
  {"plchldr4", placeHolder4, CRGB::Green, currentPalette},
  {"plchldr5", placeHolder5, CRGB::Black, currentPalette},
};

/*Possible Menu Sruct*/
String defaultStringArray[1];

struct menuSettings {
  String menuName;
  uint8_t startingSubmenu; //(use slot total to draw subsequent menus as titles)
  uint8_t slotTotal;
  bool optionsAreSubmenus;
  String *optionStrings[];
};

menuSettings menu[6] =
{
  {"Main", 0, 3, true, defaultStringArray[]},
  {"ArtNet", 0, 3, true, patterns[].oledString},
  {"Patterns", 0, 14, false, patterns[].oledString},
  {"Colors", 0, 2, true, defaultStringArray[]},
  {"Motion", 0, 3, false, patterns[].oledString},
  {"RGBEdit", 0, 3, false, patterns[].oledString},
};

enum menus {
  Main = 0,
  ArtNet = 1,
  Patterns = 2,
  Colors = 3,
  Motion = 4,
  RGBEdit = 5,
};

uint8_t pattern = 0;
menus page = Main;
uint8_t menuLocation = 0; //Tracks location in a menu
uint8_t menuSlotTotal[5] = {4, 2, 14, 3, 2};// (Main, Artnet, Patterns, Colors, Motion)

bool edit = false;

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

// Artnet settings
ArtnetWifi artnet;
const int startUniverse = 0;

bool sendFrame = 1;
int previousDataLength = 0;

//Pattern Handling Variables
uint8_t row = 0;
uint8_t hue = 0;
uint8_t sat = 0;
uint8_t angle = 0;
uint8_t angleLeft = 0;
uint8_t angleRight = 0;
uint8_t rotation = 0;
uint8_t rotationLeft = 0;
uint8_t rotationRight = 0;
int rotationDirection = 1;

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

void setRowAngle (uint8_t row, uint8_t angleValue, CRGB color)
{
  leftLeds[leftEyeMap[row][angleValue]] = color;
  rightLeds[rightEyeMap[row][angleValue]] = color;
}

void setLeftRowAngle (uint8_t row, uint8_t angleValue, CRGB color)
{
  leftLeds[leftEyeMap[row][angleValue]] = color;
}

void setRightRowAngle (uint8_t row, uint8_t angleValue, CRGB color)
{
  rightLeds[rightEyeMap[row][angleValue]] = color;
}

void setAngle (uint8_t angleValue, CRGB color)
{
  for (int row = 0; row < 5; row++)
  {
    setRowAngle(row, angleValue, color);
  }
}

//This sets a circle or row to one color.
void setRow (uint8_t row, CRGB color)
{
  for (int angleValue = 0; angleValue < 255; angleValue++)
  {
    leftLeds[leftEyeMap[row][angleValue]] = color;
    rightLeds[rightEyeMap[row][angleValue]] = color;
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
   Button / Rotary Handling
*/

void checkRotary (ESPRotary& rotary)
{
  if (page == (int)Colors)
  {
    if (edit == true)
    {
      patterns[pattern].color[menuLocation] = rotary.getPosition();
    }
    else
    {
      menuLocation = rotary.getPosition() % menuSlotTotal[page];
    }
  }
  else
  {
    menuLocation = rotary.getPosition() % menuSlotTotal[page];
  }
  drawMenu();
}

void encoderCheck ()
{
  rotary.loop();
}

void selectButton ()
{
  
  switch (page)
  {
    case Main:
      switch (menuLocation)
      {
        case 0:
          page = Patterns;
          menuLocation = 0;
          break;
        case 1:
          page = Colors;
          menuLocation = 0;
          break;
        case 2:
          page = Motion;
          menuLocation = 0;
          break;
      }
      break;
    case Patterns:
      pattern = menuLocation;
      break;
    case Colors://colors
      edit = !edit;
      if (edit == true)
      {
        rotary.setPosition(patterns[pattern].color[menuLocation]);
      }
      else
      {
        rotary.resetPosition();
      }
      break;
    case Motion:
      //Use to change motion parameters of various presets
      break;
  }
  delay(50);
  drawMenu();
}

void backButton ()
{
  if (page == Patterns || Colors || Motion)
  {
    menuLocation = 0;
    page = Main;
    //add stuff, save to eeprom here?
  }
  delay(50);
  drawMenu();
}

/*
   Menu Drawing
*/

void drawMenu ()
{
  oled.clear(PAGE);
  header();
  switch (page)
  {
    case Main:
      mainMenu();
      break;
    case Patterns:
      patternMenu();
      break;
    case Colors:
      colorMenu();
      break;
    case Motion:
      settingsMenu();
      break;
  }
  oled.display();
}

void mainMenu ()
{
  oled.setFontType(0);
  oled.setCursor(7, 11);
  oled.print("Patterns");
  Serial.println("Patterns");
  oled.setCursor(7, 21);
  oled.print("Colors");
  Serial.println("Colors");
  oled.setCursor(7, 31);
  oled.print("Motion");
  Serial.println("Motion");
  drawIndicator(0);
}

void patternMenu ()
{
  oled.setFontType(0);
  int offset = 0;
  if (menuLocation > 3)
  {
    offset = -10 * (menuLocation - 3);
  }
  for (int i = 0; i < menuSlotTotal[2]; i++)
  {
    oled.setCursor(7, 11 + (i * 10) + offset);
    oled.print(patterns[i].oledString);
  }
  drawIndicator(offset);
}

void colorMenu ()
{
  oled.setCursor(7, 11);
  oled.print("R: ");
  oled.setCursor(28, 11);
  oled.print(patterns[pattern].color.r);
  oled.setCursor(7, 21);
  oled.print("G: ");
  oled.setCursor(28, 21);
  oled.print(patterns[pattern].color.g);
  oled.setCursor(7, 31);
  oled.print("B: ");
  oled.setCursor(28, 31);
  oled.print(patterns[pattern].color.b);
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
  if (menuLocation < 4)
  {
    oled.setFontType(0);
    oled.setCursor(0, 0);
    oled.setColor(BLACK);
    oled.print(patterns[pattern].oledString);
    oled.setColor(WHITE);
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
  Color Patterns
*/

void fadeall(uint8_t fadeAmount = 230)
{
  for (int i = 0; i < numLeds; i++)
  {
    leftLeds[i].nscale8(fadeAmount);
    rightLeds[i].nscale8(fadeAmount);
  }
}

void off ()
{
  fadeall();
  FastLED.show();
  delay(10);
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

void artRead ()
{
  artnet.read();
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

void classicRezz ()
{
  for (int row = 0; row < 5; row++)
  {
    angleLeft = -16 * row + rotationLeft;
    angleRight = 16 * row + rotationRight;
    setLeftRowAngle(row, angleLeft, patterns[pattern].color);
    setLeftRowAngle(row, angleLeft - 127, patterns[pattern].color);
    setRightRowAngle(row, angleRight, patterns[pattern].color);
    setRightRowAngle(row, angleRight - 127, patterns[pattern].color);
  }
  FastLED.show();
  fadeall(235);
  rotationLeft++;
  rotationRight--;
  delay(7);
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

void redStatic ()
{
  int randomValue;
  for (int led = 0; led < 126; led++)
  {
    randomValue = random(25);//Changes the odds of a pixel being turned on
    if (randomValue == 0)
    {
      leftLeds[led] = patterns[pattern].color;
    }
    else if (randomValue == 1)
    {
      rightLeds[led] = patterns[pattern].color;
    }
  }
  FastLED.show();
  fadeall(220);
  delay(5);
}

void rainbowRezz ()
{
  for (int row = 0; row < 5; row++)
  {
    angle = -16 * row + rotation;
    hue = 24 * row + rotation;
    setRowAngle(row, angle, CHSV(hue, 255, 255));
    setRowAngle(row, angle - 127, CHSV(hue, 255, 255));
  }
  fadeall(230);
  rotation++;
  FastLED.show();
  delay(7);
}

void rainbowStatic ()
{
  int randomValue;
  for (int led = 0; led < 126; led++)
  {
    randomValue = random(25);//Changes the odds of a pixel being turned on
    hue = random(255);
    sat = 191 + random(64);
    if (randomValue == 0)
    {
      leftLeds[led] = CHSV(hue, sat, 255);
    }
    else if (randomValue == 1)
    {
      rightLeds[led] = CHSV(hue, sat, 255);
    }
  }
  FastLED.show();
  fadeall(220);
  delay(5);
}

void randomRipple ()
{
  hue = random(255);
  sat = 191 + random(64);
  for (int row = 0; row < 5; row++)
  {
    setRow(row, CHSV(hue, sat, 255));
    FastLED.show();
    for (int i = 0; i < 5; i++)
    {
      FastLED.show();
      fadeall();
      delay(5);
    }
  }
  uint16_t randomWait = random(100); //gives us a small wait time
  for (int i = 0; i < randomWait; i++)
  {
    FastLED.show();
    fadeall();
    delay(5);
  }
}

void redWobble()
{
  for (int row = 0; row < 5; row++)
  {
    angle = 16 * sin(rotation / 42.666667) * row + rotation;
    setRowAngle(row, angle, patterns[pattern].color);
    setRowAngle(row, angle - 127, patterns[pattern].color);
  }
  fadeall(235);
  rotation++;
  FastLED.show();
  delay(7);
}

void placeHolder2()
{
  setRow(row, patterns[pattern].color);
  row += rotationDirection;
  if (row == 4 || row == 0)
  {
    rotationDirection = -rotationDirection;
  }
  for (int i = 0; i < 7; i++)
  {
    FastLED.show();
    fadeall();
    delay(8);
  }
}

void placeHolder3()
{
  setRowAngle(2 + round(cubicwave8((angle + sin8(rotation)) * 3) / 127.0), angle, patterns[pattern].color);
  angle++;
  if (angle == 0)
  {
    rotation++;
    FastLED.show();
    fadeall(127);
    delay(5);
  }
}

void placeHolder4()
{
  setRowAngle(2 + round(cubicwave8((angle + rotation) * 3) / (double)sin8(rotation * 2)), angle, patterns[pattern].color);
  angle++;
  if (angle == 0)
  {
    rotation++;
    FastLED.show();
    fadeall(127);
    delay(5);
  }
}

void placeHolder5()
{

}

void setup()
{
  off();
  delay(1000);

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
  //digitalWrite(buttonPin1, LOW);
  //digitalWrite(buttonPin2, LOW);
  attachInterrupt(digitalPinToInterrupt(buttonPin1), selectButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPin2), backButton, FALLING);

  EEPROM.begin(512);
  Serial.begin(115200);
  FastLED.addLeds<APA102, LEFT_DATA_PIN, CLOCK_PIN, BGR>(leftLeds, numLeds);
  FastLED.addLeds<APA102, RIGHT_DATA_PIN, CLOCK_PIN, BGR>(rightLeds, numLeds);
  FastLED.setBrightness(32);
  mapEye();

  //Begin OLED
  oled.begin();
  oled.clear(ALL);
  oled.display();
  delay(500);
  splashScreen();
  //ConnectWifi(); commented for testing
  drawMenu();
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
}

void loop()
{
  patterns[pattern].colorFunction();
}
