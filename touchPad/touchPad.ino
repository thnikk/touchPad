/******* thnikk's Capacitive Touch osu! Keypad ********
  This is code for an experimental capacitive touch
  keypad. It uses two metal plates that can be touched
  with your bare fingers. At the moment, it only
  suports two inputs because the trinket M0 only has
  three QTouch-capable pins.

  My idea for the third pin is to use it for the side
  button, but using a screw instead of the button.
  It's a little hard to reach the button on the side,
  so using a screw would not only make it easier to
  access with your finger (since it only needs to be
  touched and not depressed,) but also offer a way to
  lock the case shut, which is missing at the moment.

  Planned features:
  -Serial remapper
  -Same LED modes as my other keypads
  -EEPROM support

  Author: thnikk
*/

#include "Adafruit_FreeTouch.h"
#include <Keyboard.h>
#include <Adafruit_DotStar.h>

// Comment out for no serial output
#define DEBUG

#if numkeys == 2
byte ftPin[] = { 3, 4, 1 }; // Trinket FreeTouch pins
#else
byte ftPin[] = { A0, A1, A2, A3, 9 }; // ItsyBitsy FreeTouch pins
#endif

// Constructors
Adafruit_DotStar dotStar = Adafruit_DotStar( 1, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

Adafruit_FreeTouch qt_1 = Adafruit_FreeTouch(ftPin[0], OVERSAMPLE_8, RESISTOR_50K, FREQ_MODE_NONE);
Adafruit_FreeTouch qt_2 = Adafruit_FreeTouch(ftPin[1], OVERSAMPLE_8, RESISTOR_50K, FREQ_MODE_NONE);
Adafruit_FreeTouch qt_3 = Adafruit_FreeTouch(ftPin[2], OVERSAMPLE_8, RESISTOR_50K, FREQ_MODE_NONE);
#if numkeys == 4
Adafruit_FreeTouch qt_4 = Adafruit_FreeTouch(ftPin[3], OVERSAMPLE_8, RESISTOR_50K, FREQ_MODE_NONE);
Adafruit_FreeTouch qt_5 = Adafruit_FreeTouch(ftPin[4], OVERSAMPLE_8, RESISTOR_50K, FREQ_MODE_NONE);
#endif

// Arrays
int qt[numkeys+1];
bool pressed[numkeys+1];
byte mapping[] = { 122, 120, 99, 118, 177 };
byte rgb[3];
byte bpsBuffer[3];

// This lock makes it so the key is only pressed/depressed once, rather than spamming either.
// Only one event is required for each, so this functions as a normal keyboard would.
// It works without this, but will reduce the program speed to around 334 loops per second
// compared to the 108,000 that can be achieved with this optimization.
bool pressedLock[3];

// Millis timers
unsigned long reportMillis;
unsigned long updateMillis;
unsigned long lightMillis;
unsigned long countMillis;
unsigned long bpsMillis;
unsigned long bpsLEDMillis;
// For program speed check
unsigned long countCheck = 0;
unsigned long countBuffer = 0;
// Counter for Cycle LED
byte cycleCount;
// Millis timer intervals
unsigned long lightInterval = 10;
unsigned long reportInterval = 10;
unsigned long bpsInterval = 1000;
// Value for determining keypress for FreeTouch
int pressThreshold[] = { 450, 500 };

byte changeVal = 10; // This is the amount that the colors change per press on the color change mode
byte bpsCount;
byte dscc = 0;
bool changeCheck = 0;

/*
███████ ███████ ████████ ██    ██ ██████
██      ██         ██    ██    ██ ██   ██
███████ █████      ██    ██    ██ ██████
     ██ ██         ██    ██    ██ ██
███████ ███████    ██     ██████  ██
*/

void setup() {
  // This will need to be universal after the remapper is implemented
  #if defined (DEBUG)
    Serial.begin(9600);
  #endif

  // Initialize inputs
  qt_1.begin();
  qt_2.begin();
  qt_3.begin();
  #if numkeys == 4
  qt_4.begin();
  qt_5.begin();
  #endif

  // Start LED lib
  dotStar.begin(); // Initialize pins for output
  dotStar.show();  // Turn all LEDs off ASAP
}

void loop() {
  #if defined (DEBUG)
    serialDebug();
    if ((millis() - countMillis) > 1000) { countCheck = countBuffer; countBuffer = 0; countMillis = millis(); }
    countBuffer++;
  #endif

  readValues();
  // cycle();
  // colorChange();
  bps();
  keyboard();
}

/*
██████  ███████ ██████  ██    ██  ██████
██   ██ ██      ██   ██ ██    ██ ██
██   ██ █████   ██████  ██    ██ ██   ███
██   ██ ██      ██   ██ ██    ██ ██    ██
██████  ███████ ██████   ██████   ██████
*/

// Shows some debugging info, like the cap touch values and bool values as well as the loop counter.
#if defined (DEBUG)
void serialDebug() {
  if ((millis() - reportMillis) > reportInterval) {
    Serial.write(27); Serial.print("[2J"); Serial.write(27); Serial.print("[H"); // Resets serial monitor
    Serial.println("***************** Int Values *****************");
    for (byte x=0; x<numkeys; x++) { Serial.print("Button "); Serial.print(x+1); Serial.print(": "); Serial.println(qt[x]); }
    Serial.println("***************** Bool Values *****************");
    for (byte x=0; x<numkeys; x++) { Serial.print("Button "); Serial.print(x+1); Serial.print(": "); Serial.println(pressed[x]); }
    Serial.println("***************** pressedLock *****************");
    for (byte x=0; x<numkeys; x++) { Serial.print("Button "); Serial.print(x+1); Serial.print(": "); Serial.println(pressedLock[x]); }
    Serial.println("***************** RGB Values *****************");
    for (byte x=0; x<3; x++) { Serial.print(rgb[x]); if(x<2) Serial.print(", "); else Serial.println(); }
    Serial.println("***************** BPS Values *****************");
    for (byte x=0; x<3; x++) { Serial.print(bpsBuffer[x]); if(x<2) Serial.print(", "); else Serial.println(); }
    Serial.print("Counter: "); Serial.println(countCheck);

    // Any more debugging output should go here so it outputs at the same rate.

    reportMillis = millis();
  }
}
#endif

/*
██      ███████ ██████      ███    ███  ██████  ██████  ███████ ███████
██      ██      ██   ██     ████  ████ ██    ██ ██   ██ ██      ██
██      █████   ██   ██     ██ ████ ██ ██    ██ ██   ██ █████   ███████
██      ██      ██   ██     ██  ██  ██ ██    ██ ██   ██ ██           ██
███████ ███████ ██████      ██      ██  ██████  ██████  ███████ ███████
*/

// Simple LED mode
void cycle() {
  if ((millis() - lightMillis) > lightInterval) {
    // If either key is pressed, turn LED white.
    for (byte x=0; x<2; x++) if (pressed[x]) dotStar.setPixelColor(0, 0xFFFFFF);
    // Otherwise, cycle through colors.
    if (!pressed[0] && !pressed[1]) { wheel(cycleCount); cycleCount++; dotStar.setPixelColor(0, rgb[0], rgb[1], rgb[2]); }
    if (pressed[2]) dotStar.setPixelColor(0, 0x000000);
    dotStar.show();
    lightMillis = millis();
  }
}

void bps() {

  for (byte x=0; x<numkeys; x++) if (pressed[x] && pressedLock[x]) bpsCount++;
  if ((millis() - bpsMillis) > bpsInterval) { wheel(bpsCount*changeVal); bpsCount = 0; bpsMillis = millis(); }

  if ((millis() - bpsLEDMillis) > 1) {
    for (byte x=0; x<3; x++) { if (rgb[x] > bpsBuffer[x]) bpsBuffer[x] = bpsBuffer[x]+1; if (rgb[x] < bpsBuffer[x]) bpsBuffer[x] = bpsBuffer[x]-1; }
    dotStar.setPixelColor(0, bpsBuffer[0], bpsBuffer[1], bpsBuffer[2]); dotStar.show();
    bpsLEDMillis = millis();
  }
}

void colorChange(){
  if (changeCheck == 0) { wheel(dscc); dotStar.setPixelColor(0, rgb[0], rgb[1], rgb[2]); dotStar.show(); changeCheck = 1; }
  for (byte x=0; x<numkeys; x++){
    if (pressed[x] && pressedLock[x]) { dscc += changeVal; wheel(dscc); dotStar.setPixelColor(0, rgb[0], rgb[1], rgb[2]); dotStar.show(); }
  }
}

// Blinks LEDs based on paramteter value
void blinkLEDs(byte times) { for (int y = 0; y < times; y++) { dotStar.setPixelColor(0, 0x000000); dotStar.show(); delay(20); dotStar.setPixelColor(0, 0xFFFFFF); dotStar.show(); delay(50); } }

// Converts a byte into 3 bytes for R, G, and B color and stores it into the rgb[] array.
void wheel(byte shortColor) {
  if (shortColor >= 0 && shortColor < 85) { rgb[0] = (shortColor * -3) +255; rgb[1] = shortColor * 3; rgb[2] = 0; }
  else if (shortColor >= 85 && shortColor < 170) { rgb[0] = 0; rgb[1] = ((shortColor - 85) * -3) +255; rgb[2] = (shortColor - 85) * 3; }
  else { rgb[0] = (shortColor - 170) * 3; rgb[1] = 0; rgb[2] = ((shortColor - 170) * -3) +255; }
}

/*
██   ██ ███████ ██    ██ ██████   ██████   █████  ██████  ██████
██  ██  ██       ██  ██  ██   ██ ██    ██ ██   ██ ██   ██ ██   ██
█████   █████     ████   ██████  ██    ██ ███████ ██████  ██   ██
██  ██  ██         ██    ██   ██ ██    ██ ██   ██ ██   ██ ██   ██
██   ██ ███████    ██    ██████   ██████  ██   ██ ██   ██ ██████
*/

// Checks the values from the FreeTouch library and puts bool values into the pressed array for easy access.
// Also takes care of reading the side button and puts it into the same array.
void readValues() {
  if ((millis() - updateMillis) >= 1) { // Only update values once every ms
    // Load values into qt array (unnecessary, but useful for debugging)
    qt[0] = qt_1.measure();
    qt[1] = qt_2.measure();
    qt[2] = qt_3.measure();
    #if numkeys == 4
      qt[3] = qt_4.measure();
      qt[4] = qt_5.measure();
    #endif
    // Convert values to bools depending on threshold value (varies since screw is not aluminum)
    for (byte x=0; x<numkeys; x++){ if (qt[x] > pressThreshold[0]) pressed[x] = 1;  else pressed[x] = 0; }
    if (qt[numkeys] > pressThreshold[1]) pressed[numkeys] = 1;  else pressed[numkeys] = 0;
  }
}

void keyboard() {
  for (byte x=0; x<numkeys; x++){
    if (pressed[x] && pressedLock[x]) { Keyboard.press(mapping[x]); pressedLock[x] = 0; }
    if (!pressed[x] && !pressedLock[x]){ Keyboard.release(mapping[x]); pressedLock[x] = 1; }
  }
  // Since the side button is always the last value in the array, we handle this seperately
  if (pressed[numkeys] && pressedLock[numkeys]) { Keyboard.press(mapping[sizeof(mapping)-1]); pressedLock[numkeys] = 0; }
  if (!pressed[numkeys] && !pressedLock[numkeys]){ Keyboard.release(mapping[sizeof(mapping)-1]); pressedLock[numkeys] = 1; }
}
