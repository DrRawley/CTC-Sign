#include <Arduino.h>
#include <FastLED.h>
#include <ezButton.h>
#include <EEPROM.h>
#include <math.h>

// Digital Pins
#define DATA_PIN 2
#define SELECT_BUTTON_PIN 10

// Analog Pins
#define POT_PIN 0
#define MIC_PIN 1

// Mic Sensitivities
#define EEPROM_MIC_BASE_LEVEL_LOCATION 1
float micBaseLevel = 3.0;
float micScalingFactor = 10.0;
int micOffset = 0;

#define NUM_LEDS 31
#define MAX_BRIGHTNESS 0x64
CRGB leds[NUM_LEDS];

// Setup buttons
ezButton button1(SELECT_BUTTON_PIN);

// Dither Array Definition
const byte ditherSize = 12;
byte dither[ditherSize];

// Define LEDs per Letter
const byte sizeLetter1 = 11;
const byte sizeLetter2 = 9;
const byte sizeLetter3 = 11;
const byte letter1[sizeLetter1] = {7, 8, 9, 6, 10, 5, 4, 0, 3, 2, 1};
const byte letter2[sizeLetter2] = {15, 16, 17, 18, 19, 14, 13, 12, 11};
const byte letter3[sizeLetter3] = {27, 28, 29, 26, 30, 25, 24, 20, 23, 22, 21};

// Define LEDs per row
const byte numRows = 5;
const byte rows[numRows][11] = {
    {3, 2, 1, 11, 23, 22, 21},
    {4, 0, 12, 24, 20},
    {5, 13, 25},
    {6, 10, 14, 26, 30},
    {7, 8, 9, 15, 16, 17, 18, 19, 27, 28, 29}};
const byte rowSizes[numRows] = {7, 5, 3, 5, 11};

// Define Leds per column
const byte numColumns = 17;
const byte columns[numColumns][5] = {
    {6, 5, 4},
    {7, 3},
    {8, 2},
    {9, 1},
    {10, 0},
    {},
    {15},
    {16},
    {17, 14, 13, 12, 11},
    {18},
    {19},
    {},
    {26, 25, 24},
    {27, 23},
    {28, 22},
    {29, 21},
    {30, 20}};
const byte colSizes[numColumns] = {3, 2, 2, 2, 2, 0, 1, 1, 5, 1, 1, 0, 3, 2, 2, 2, 2};

// Define LEDs per Diagonal
const byte numDiags = 21;
const byte diags[numDiags][3] = {
    {},
    {7, 6},
    {8, 5},
    {9, 4},
    {},
    {10, 3},
    {15, 2},
    {16, 0, 1},
    {17},
    {18, 14},
    {19, 13},
    {12},
    {11},
    {27, 26},
    {28, 25},
    {29, 24},
    {},
    {30, 23},
    {22},
    {20, 21},
    {}};
const byte diagsSizes[numDiags] = {0, 2, 2, 2, 0, 2, 2, 3, 1, 2, 2, 1, 1, 2, 2, 2, 0, 2, 1, 2, 0};

// Define LEDs for opposite diagonal
const byte diagsBackwards[numDiags][3] = {
    {},
    {4, 3},
    {5, 2},
    {6, 1},
    {},
    {7, 0},
    {8},
    {9, 10},
    {11},
    {12},
    {15, 13},
    {16, 14},
    {17},
    {18, 24, 23},
    {19, 25, 22},
    {26, 21},
    {},
    {27, 20},
    {28},
    {29, 30},
    {}};
const byte diagsBackwardsSizes[numDiags] = {0, 2, 2, 2, 0, 2, 1, 2, 1, 1, 2, 2, 1, 3, 3, 2, 0, 2, 1, 2, 0};

// Declare function definitions
byte selection = 0;               // Default function selection
const byte numberSelections = 12; // Number of different functions
void solid(byte hue);
void colorLettersStatic(byte hue);
void chase(byte hue);
void verticalChase(byte hue);
void verticalRainbow(byte potValue);
void horizontalChase(byte hue);
void diagChase(byte hue, byte diagsDirection);
void randomLights(byte potValue);
void yellowToWhite(byte potValue);

// Function definitions for audio input
int getMicValue(void);
void serialCheckAndSet(void);
float setMicBaseLevel(float);
float setMicScalingFactor(float);
int setMicOffset(int);
float getSerialFloat(void);
void basicVUMeter(int);
void intensityVUMeter(int);

void setup()
{
  Serial.begin(9600);
  randomSeed(analogRead(2));
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  button1.setDebounceTime(100);

  // Calculate dither array
  const float logDithersize = log10((float)(ditherSize + 1));
  for (byte i = 0; i < ditherSize; i++)
  {
    dither[i] = (byte)(MAX_BRIGHTNESS * log10(ditherSize - i) / logDithersize);
  }

  selection = EEPROM.read(0);
  if (!(selection < numberSelections))
  {
    selection = 0;
  }
  float readValue = 0;
  EEPROM.get(EEPROM_MIC_BASE_LEVEL_LOCATION, readValue);
  setMicBaseLevel(readValue);
  Serial.print("Loaded mic base level: ");
  Serial.print(micBaseLevel);
  EEPROM.get(EEPROM_MIC_BASE_LEVEL_LOCATION + sizeof(float), readValue);
  setMicScalingFactor(readValue);
  Serial.print("  Loaded mic scaling factor: ");
  Serial.println(micScalingFactor);
  int readIntValue = 0;
  EEPROM.get(EEPROM_MIC_BASE_LEVEL_LOCATION + sizeof(float) * 2, readIntValue);
  setMicOffset(readIntValue);
  Serial.print("  Loaded mic offset: ");
  Serial.println(micOffset);
  Serial.println("Enter 'b' to set Mic Base Level, 'f' to set Mic Scaling Factor, or 'o' to set Mic Offset.");
}

void loop()
{
  static byte potValue = 0;
  button1.loop();

  serialCheckAndSet();

  if (button1.isPressed())
  {
    selection++;
    if (!(selection < numberSelections))
    {
      selection = 0;
    }
    EEPROM.update(0, selection);
  }
  potValue = (byte)map(analogRead(POT_PIN), 0, 1024, 0, 0xff);

  switch (selection)
  {
  case 0:
    colorLettersStatic(0); // Make zero so that it stays on green-blue-green
    break;
  case 1:
    solid(potValue);
    break;
  case 2:
    chase(potValue);
    break;
  case 3:
    verticalChase(potValue);
    break;
  case 4:
    verticalRainbow(potValue);
    break;
  case 5:
    randomLights(potValue);
    break;
  case 6:
    horizontalChase(potValue);
    break;
  case 7:
    diagChase(potValue, 0);
    break;
  case 8:
    diagChase(potValue, 1); // 1 for backwards diagonals
    break;
  case 9:
    basicVUMeter(potValue);
    break;
  case 10:
    intensityVUMeter(potValue);
    break;
  case 11:
    yellowToWhite(potValue);
    break;
  default:
    break;
  }
}

void solid(byte hue)
{
  for (int dot = 0; dot < NUM_LEDS; dot++)
  {
    leds[dot].setHSV(hue, 0xff, 0x40);
  }
  FastLED.show();
}

void chase(byte hue)
{
  static int offset = 0;
  static byte iLimit = (byte)(NUM_LEDS / ditherSize) + 1;
  static unsigned long timer = millis();

  if (millis() - timer > 100)
  {
    for (int i = 0; i < iLimit; i++)
    {
      for (int j = 0; j < ditherSize; j++)
      {
        int index = i * ditherSize + j;
        if (index < NUM_LEDS) // Make sure not writing outside of length of leds[]
        {
          leds[i * ditherSize + j].setHSV(hue, 0xff, dither[(ditherSize - j + offset) % ditherSize]); // this index was annoying
        }
      }
    }
    FastLED.show();
    offset++;
    timer = millis();
  }
}

void verticalChase(byte hue)
{
  static int offset = 0;
  static unsigned long timer = millis();

  if (millis() - timer > 100)
  {
    for (int i = 0; i < numColumns; i++)
    {
      for (int j = 0; j < colSizes[i]; j++)
      {
        leds[columns[i][j]].setHSV(hue, 0xff, dither[(ditherSize - i % ditherSize + offset) % ditherSize]);
      }
    }
    FastLED.show();
    offset++;
    timer = millis();
  }
}

void verticalRainbow(byte potValue)
{
  static int offset = 0;
  static unsigned long timer = millis();
  int unsigned delay = map(potValue, 0, 255, 20, 2000);

  if (millis() - timer > delay)
  {
    for (int i = 0; i < numColumns; i++)
    {
      for (int j = 0; j < colSizes[i]; j++)
      {
        leds[columns[i][j]].setHSV((i * 10 + offset) % 255, 0xff, 0x40);
      }
    }
    FastLED.show();
    offset++;
    timer = millis();
  }
}

void randomLights(byte potValue)
{
  static bool firstTime = true;
  static unsigned long timer = millis();
  int unsigned delay = map(potValue, 0, 255, 100, 5000);
  if ((millis() - timer > delay) || firstTime)
  {
    firstTime = false;
    for (byte dot = 0; dot < NUM_LEDS; dot++)
    {
      byte hue = (byte)random(256);

      leds[dot] = CHSV(hue, 0xff, MAX_BRIGHTNESS / 2);
    }
    FastLED.show();
    timer = millis();
  }
}

void horizontalChase(byte hue)
{
  static int offset = 0;
  static unsigned long timer = millis();

  if (millis() - timer > 100)
  {
    for (int i = 0; i < numRows; i++)
    {
      for (int j = 0; j < rowSizes[i]; j++)
      {
        leds[rows[i][j]].setHSV(hue, 0xff, dither[(ditherSize - i % ditherSize + offset) % ditherSize]);
      }
    }
    FastLED.show();
    offset++;
    timer = millis();
  }
}

void colorLettersStatic(byte hue)
{
  static int offset = 0;
  static unsigned long timer = millis();
  static int brightness = 0x40;

  if (millis() - timer > 200)
  {
    for (int i = 0; i < sizeLetter1; i++)
    {
      leds[letter1[i]].setHSV((96 + hue) % 0xff, 0xff, brightness); // 96 = green
    }
    for (int i = 0; i < sizeLetter2; i++)
    {
      leds[letter2[i]].setHSV((160 + hue) % 0xff, 0xff, brightness); // 160 = blue
    }
    for (int i = 0; i < sizeLetter3; i++)
    {
      leds[letter3[i]].setHSV((96 + hue) % 0xff, 0xff, brightness); // 96 = green
    }

    FastLED.show();
    offset++;
  }
}

void diagChase(byte hue, byte diagsDirection)
{
  static int offset = 0;
  static unsigned long timer = millis();
  int size = 0;

  if (millis() - timer > 100)
  {
    for (int i = 0; i < numDiags; i++)
    {
      if (diagsDirection == 0)
      {
        size = diagsSizes[i];
      }
      else
      {
        size = diagsBackwardsSizes[i];
      }
      for (int j = 0; j < size; j++)
      {
        if (diagsDirection == 0)
        {
          leds[diags[i][j]].setHSV(hue, 0xff, dither[(ditherSize - i % ditherSize + offset) % ditherSize]);
        }
        else
        {
          leds[diagsBackwards[i][j]].setHSV(hue, 0xff, dither[(ditherSize - i % ditherSize + offset) % ditherSize]);
        }
      }
    }
    FastLED.show();
    offset++;
    timer = millis();
  }
}

void yellowToWhite(byte potValue)
{
  byte saturation = (byte)map(potValue, 0, 255, 255, 32);
  byte brightness = (byte)map(potValue, 0, 255, 40, 160);

  for (byte dot = 0; dot < NUM_LEDS; dot++)
  {
    leds[dot].setHSV(32, saturation, brightness); // old value 38
  }
  FastLED.show();
}

int getMicValue(void)
{
  int val = 0;
  const int sampleSize = 32;

  int max = 0;
  int min = 1024;
  for (int i = 0; i < sampleSize; i++)
  {
    val = analogRead(MIC_PIN);
    max = max(max, val);
    min = min(min, val);
  }
  int delta = max - min - micOffset;
  if (delta > 0)
  {
    return delta;
  }
  else
  {
    return 0;
  }
}

float setMicBaseLevel(float newMicBaseLevel)
{
  if (newMicBaseLevel > 0)
  {
    micBaseLevel = newMicBaseLevel;
  }

  return micBaseLevel;
}

float setMicScalingFactor(float newMicScalingFactor)
{
  if (newMicScalingFactor > 0)
  {
    micScalingFactor = newMicScalingFactor;
  }

  return micScalingFactor;
}

int setMicOffset(int newMicOffset)
{
  if (newMicOffset >= 0 && newMicOffset < 1024)
  {
    micOffset = newMicOffset;
  }

  return micOffset;
}

float getSerialFloat(void)
{
  byte index = 0;
  char endMarker = '\n';
  char rc;
  bool receivedEndMarker = false;
  const byte numChars = 6;
  char receivedChars[numChars];
  while (receivedEndMarker == false)
  {
    while (!Serial.available())
      ;

    rc = Serial.read();
    if (rc != endMarker)
    {
      if (isDigit(rc) || rc == '.')
      {
        receivedChars[index] = rc;
        index++;
        Serial.print(rc);
      }

      if (index >= numChars)
      {
        index = numChars - 1;
      }
    }
    else
    {
      receivedChars[index] = '\0';
      index = 0;
      receivedEndMarker = true;
      Serial.println("");
    }
  }

  return atof(receivedChars);
}

void serialCheckAndSet()
{
  if (Serial.available())
  {
    char option = Serial.read();
    Serial.println(option);

    if (option == '\n')
    {
      Serial.print("Current mic base level: ");
      Serial.print(micBaseLevel);
      Serial.print("  Current mic scaling factor: ");
      Serial.println(micScalingFactor);
      Serial.print("  Current mic offset: ");
      Serial.println(micOffset);
      Serial.println("Enter 'b' to set Mic Base Level, 'f' to set Mic Scaling Factor, or 'o' to set Mic Offset.");
    }
    if (option == 'b' || option == 'B')
    {
      Serial.print("Current mic base level: ");
      Serial.println(micBaseLevel);
      Serial.print("Enter new mic base level: ");

      float result = getSerialFloat();
      result = setMicBaseLevel(result);
      EEPROM.put(EEPROM_MIC_BASE_LEVEL_LOCATION, micBaseLevel);

      Serial.print("New Mic Base Level: ");
      Serial.println(result);
    }
    if (option == 'f' || option == 'F')
    {
      Serial.print("Current mic scaling factor: ");
      Serial.println(micScalingFactor);
      Serial.print("Enter new mic scaling factor: ");

      float result = getSerialFloat();
      result = setMicScalingFactor(result);
      EEPROM.put(EEPROM_MIC_BASE_LEVEL_LOCATION + sizeof(float), micScalingFactor);

      Serial.print("New Mic Scaling Factor: ");
      Serial.println(result);
    }
    if (option == 'o' || option == 'O')
    {
      Serial.print("Current mic offset: ");
      Serial.println(micOffset);
      Serial.print("Enter new mic offset factor: ");

      int result = (int)getSerialFloat();
      result = setMicOffset(result);
      EEPROM.put(EEPROM_MIC_BASE_LEVEL_LOCATION + sizeof(float) * 2, micOffset);

      Serial.print("New Mic Offset: ");
      Serial.println(result);
    }
  }
}

void basicVUMeter(int hue)
{
  int numIntensities = numRows * 4;
  int value = getMicValue();

  int intensity = (int)(micScalingFactor * log10((float)value / micBaseLevel));

  if (intensity > numIntensities - 1)
  {
    intensity = numIntensities - 1;
  }
  else if (intensity < 0)
  {
    intensity = 0;
  }
  Serial.print(value);
  Serial.print(" - ");
  Serial.println(intensity);

  // const byte brightnessGradient[4] = {32, 64, 96, 128};
  const byte brightnessGradient[4] = {MAX_BRIGHTNESS / 4, MAX_BRIGHTNESS / 2, MAX_BRIGHTNESS * 3 / 4, MAX_BRIGHTNESS};
  const byte colorGradient[6] = {96, 96, 96, 96, 64, 0}; // grn, grn, grn, grn, yellow, red
  // Loop to set row color and brightness based off of intensity; only necessary to loop up to the intensity
  for (int i = 0; i < intensity; i++)
  {
    int rowNumberIndex = (int)(i / 4);
    int brightness = brightnessGradient[i % 4];

    for (int j = 0; j < rowSizes[rowNumberIndex]; j++) // Fill leds for current row
    {
      leds[rows[rowNumberIndex][j]].setHSV(colorGradient[rowNumberIndex], 0xff, brightness);
    }
  }

  FastLED.show();

  //   for (int dot = 0; dot < NUM_LEDS; dot++)
  //   {
  //     leds[dot].setHSV(hue, 0xff, 0x10);
  //   }
  byte brightness = 0x10;
  for (int i = 0; i < sizeLetter1; i++)
  {
    leds[letter1[i]].setHSV((96), 0xff, brightness); // 96 = green
  }
  for (int i = 0; i < sizeLetter2; i++)
  {
    leds[letter2[i]].setHSV((160), 0xff, brightness); // 160 = blue
  }
  for (int i = 0; i < sizeLetter3; i++)
  {
    leds[letter3[i]].setHSV((96), 0xff, brightness); // 96 = green
  }
}

void intensityVUMeter(int hue)
{
  int numIntensities = 20;
  int value = getMicValue();

  int intensity = (int)(micScalingFactor * log10((float)value / micBaseLevel));

  if (intensity > numIntensities) // Don't need a "- 1" here because it's not being indexed
  {
    intensity = numIntensities;
  }
  else if (intensity < 0)
  {
    intensity = 0;
  }

  byte brightness = (byte)((MAX_BRIGHTNESS - 0x10) * intensity / numIntensities + 0x10);

  // for (int i = 0; i < NUM_LEDS; i++)
  // {
  //   leds[i].setHSV(hue % 0xff, 0xff, brightness);
  // }
  for (int i = 0; i < sizeLetter1; i++)
  {
    leds[letter1[i]].setHSV((96), 0xff, brightness); // 96 = green
  }
  for (int i = 0; i < sizeLetter2; i++)
  {
    leds[letter2[i]].setHSV((160), 0xff, brightness); // 160 = blue
  }
  for (int i = 0; i < sizeLetter3; i++)
  {
    leds[letter3[i]].setHSV((96), 0xff, brightness); // 96 = green
  }
  FastLED.show();
}
