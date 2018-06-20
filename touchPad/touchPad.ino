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
#include <FlashAsEEPROM.h>
#include <Adafruit_DotStar.h>

// Comment out for no serial output
// #define DEBUG

#if numkeys == 2
byte ftPin[] = { 3, 4, 1 }; // Trinket FreeTouch pins
byte initMapping[] = { 122, 120, 177 };
#else
byte ftPin[] = { A0, A1, A2, A3, 9 }; // ItsyBitsy FreeTouch pins
byte initMapping[] = { 122, 120, 99, 118, 177 };
#endif

// Constructors
Adafruit_DotStar dotStar = Adafruit_DotStar( 1, DATAPIN, CLOCKPIN, DOTSTAR_RGB);

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
byte mapping[numkeys+1][3];
byte rgb[3];
byte bpsBuffer[3];
byte custom[3] = { 200, 0, 200 };

// This lock makes it so the key is only pressed/depressed once, rather than spamming either.
// Only one event is required for each, so this functions as a normal keyboard would.
// It works without this, but will reduce the program speed to around 334 loops per second
// compared to the 108,000 that can be achieved with this optimization.
bool pressedLock[3];

// Millis timers
unsigned long previousMillis;
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

bool set = 0;
bool colorLock[2];

byte reactiveStep;
byte reactCycle = 170;


// Remap code
byte specialLength = 34; // Number of "special keys"
String specialKeys[] = {
  "shift", "ctrl", "super",
  "alt", "f1", "f2", "f3",
  "f4", "f5", "f6", "f7",
  "f8", "f9", "f10", "f11",
  "f12", "insert",
  "delete", "backspace",
  "enter", "home", "end",
  "pgup", "pgdn", "up",
  "down", "left", "right",
  "tab", "escape", "MB1",
  "MB2", "MB3", "altGr"
};
byte specialByte[] = {
  129, 128, 131, 130,
  194, 195, 196, 197,
  198, 199, 200, 201,
  202, 203, 204, 205,
  209, 212, 178, 176,
  210, 213, 211, 214,
  218, 217, 216, 215,
  179, 177, 1, 2, 3,
  134
};

byte inputBuffer; // Stores specialByte after conversion
bool version = 1;
byte ledMode = 0;

/*
███████ ███████ ████████ ██    ██ ██████
██      ██         ██    ██    ██ ██   ██
███████ █████      ██    ██    ██ ██████
     ██ ██         ██    ██    ██ ██
███████ ███████    ██     ██████  ██
*/

void setup() {
  Serial.begin(9600);

  // Initialize EEPROM
	if (EEPROM.read(0) != version) {
		EEPROM.write(0, version);
		EEPROM.write(20, ledMode);
    for (byte x=0; x<3; x++) EEPROM.write(30+x, custom[x]);
		for (byte x = 0; x <= numkeys; x++) { // default custom RGB values
			for (byte  y= 0; y < 3; y++) { if (y == 0) EEPROM.write(40+(x*3)+y, initMapping[x]); if (y > 0) EEPROM.write(40+(x*3)+y, 0);	}
		}
		EEPROM.commit();
	}
	// Load values from EEPROM
	for (int x = 0; x <= numkeys; x++) { for (int  y= 0; y < 3; y++) mapping[x][y] = EEPROM.read(40+(x*3)+y); }
  for (byte x=0; x<3; x++) custom[x] = EEPROM.read(30+x);
  ledMode = EEPROM.read(20);

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

/*
██       ██████   ██████  ██████
██      ██    ██ ██    ██ ██   ██
██      ██    ██ ██    ██ ██████
██      ██    ██ ██    ██ ██
███████  ██████   ██████  ██
*/



void loop() {

  // Run debugger or remapper
  #if defined (DEBUG)
    serialDebug();
    if ((millis() - countMillis) > 1000) { countCheck = countBuffer; countBuffer = 0; countMillis = millis(); }
    countBuffer++;
  #else
    if ((millis() - previousMillis) > 1000) { // Check once a second to reduce overhead
      // Run once when serial monitor is opened to avoid flooding the serial monitor
      if (Serial && set == 0) { mainMenu(0); set = 1; }
      if (Serial.available() > 0) {
        String serialInput = Serial.readString(); byte input= serialInput.toInt();
        if (input == 0) remapSerial(); if (input == 1) changeMode(); if (input == 2) customSet(); if (input == 3) mainMenu(2); else mainMenu(1); }
      if (!Serial) set = 0; // If the serial monitor is closed, reset so it can prompt the user to press 0 again.
      previousMillis = millis();
    }
  #endif

  readValues();

  // Set LED mode
  if (ledMode == 0) cycle();
  if (ledMode == 1) reactive(0);
  if (ledMode == 2) reactive(1);
  if (ledMode == 3) colorChange();
  if (ledMode == 4) bps();
  if (ledMode == 5) customMode();
  if (ledMode == 6 && colorLock[0] == 0) { setColor(0, 0, 0); colorLock[0] = 1; }
  if (ledMode != 6 && colorLock[0] == 1) colorLock[0] = 0;

  // Keybnoard code
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
    Serial.print("CycleCount: "); Serial.println(cycleCount);
    Serial.println("***************** BPS Values *****************");
    for (byte x=0; x<3; x++) { Serial.print(bpsBuffer[x]); if(x<2) Serial.print(", "); else Serial.println(); }
    Serial.println("****************** Reactive ******************");
    Serial.print("ReactiveStep: "); Serial.println(reactiveStep);

    Serial.println();Serial.print("Counter: "); Serial.println(countCheck);

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
    if (pressed[numkeys]) dotStar.setPixelColor(0, 0x000000);
    dotStar.show();
    lightMillis = millis();
  }
}

void reactive(bool flip) {
  if ((millis() - lightMillis) > 0) {
    // If either key is pressed, turn LED white.
    for (byte x=0; x<numkeys; x++) if ((pressed[x] && flip == 0) || (!pressed[0] && !pressed[1] && flip == 1)) { reactiveStep = 1; reactCycle = 169; for (byte y=0; y<3; y++) rgb[y] = 255; }
    if (reactiveStep == 1) { rgb[0] = rgb[0]-1; rgb[1] = rgb[0]; if (rgb[0] == 0) reactiveStep = 2; } // white to red
    if (reactiveStep == 2) { wheel(reactCycle); reactCycle--; if (rgb[0] == 255) reactiveStep = 3; } // red to green to blue
    if (reactiveStep == 3) { if (rgb[0] != 0) rgb[0] = rgb[0]-1; }
    dotStar.setPixelColor(0, rgb[0], rgb[1], rgb[2]);
    dotStar.show();
    lightMillis = millis();
  }
}

void setColor(byte r, byte g, byte b) { dotStar.setPixelColor(0, r, g, b); dotStar.show(); }

// This is used in the serial configurator when it's waiting for an input
void fastCycle() { if ((millis() - lightMillis) > 1) { wheel(cycleCount); cycleCount++; dotStar.setPixelColor(0, rgb[0], rgb[1], rgb[2]); dotStar.show(); lightMillis = millis(); } }

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

void customMode() {
  if ((millis() - lightMillis) > lightInterval) {
    // If either key is pressed, turn LED white.
    for (byte x=0; x<2; x++) if (pressed[x]) dotStar.setPixelColor(0, 0xFFFFFF);
    // Otherwise, cycle through colors.
    if (!pressed[0] && !pressed[1]) { dotStar.setPixelColor(0, custom[0], custom[1], custom[2]); }
    if (pressed[numkeys]) dotStar.setPixelColor(0, 0x000000);
    dotStar.show();
    lightMillis = millis();
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
███████ ███████ ██████  ██  █████  ██          ███████ ███████ ████████ ████████ ██ ███    ██  ██████  ███████
██      ██      ██   ██ ██ ██   ██ ██          ██      ██         ██       ██    ██ ████   ██ ██       ██
███████ █████   ██████  ██ ███████ ██          ███████ █████      ██       ██    ██ ██ ██  ██ ██   ███ ███████
     ██ ██      ██   ██ ██ ██   ██ ██               ██ ██         ██       ██    ██ ██  ██ ██ ██    ██      ██
███████ ███████ ██   ██ ██ ██   ██ ███████     ███████ ███████    ██       ██    ██ ██   ████  ██████  ███████
*/

#ifndef DEBUG

void mainMenu(byte reenter) {
  if (reenter == 2) Serial.print("Invalid, p");
  else if (reenter == 1) Serial.print("Saved, p");
  else Serial.print("P");
  Serial.println("lease enter the number for the corresponding setting you'd like to change:");
  Serial.println("0 | Remap keys");
  Serial.println("1 | Set LED mode");
  Serial.println("2 | Change color for custom mode");
  Serial.println();
}

void changeMode() {
  Serial.println("Select an LED mode."); Serial.println();
  Serial.println("Num |   Mode   | Description");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 0  |   Cycle  | Cycles through rainbow");
  Serial.println("    |          | Turns to white when keys are pressed");
  Serial.println("    |          | Turns off when side button is pressed");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 1  | Reactive | Turns to white when pressed");
  Serial.println("    |          | Fades through r>g>b>off when released");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 2  | Reactive | Fades through r>g>b>off when pressed");
  Serial.println("    | Inverted | Turns to white when released");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 3  |  Color   | Color changes from blue to green to");
  Serial.println("    |  Change  | red every time a key is pressed");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 4  |   BPS    | Color changes from blue to green to");
  Serial.println("    |          | red, depending on how many times the");
  Serial.println("    |          | keys are pressed per second");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 5  |  Custom  | Uses custom color set in settings.");
  Serial.println("    |          | Turns to white when keys are pressed");
  Serial.println("    |          | Turns off when side button is pressed");
  Serial.println("-----------------------------------------------------");
  Serial.println(" 6  |    Off   | Turns LED off (green LED is always on)");

  while (true){
    while(!Serial.available()){ fastCycle(); }
    String serialInput = Serial.readString(); byte inputBuffer = serialInput.toInt();
    if (inputBuffer > 6) Serial.println("Invalid value, please try again.");
    if (inputBuffer <=6) { ledMode=inputBuffer; break; } }
  for (byte x=0; x<3; x++) rgb[x] = 0; // Reset LEDs
  EEPROM.write(20, ledMode); EEPROM.commit(); // Save value to EEPROM
  Serial.println(); Serial.print("Mode ");
  blinkLEDs(2);
}

void customSet() {
  String csRGB[] = { "Blue: ", ", Green: ", ", Red: " };
  Serial.println("Enter 0-255 value for each color.");
  for (byte x=0; x<3; x++) {
    Serial.print(csRGB[x]);
    while(!Serial.available()){ fastCycle(); }
    String serialInput = Serial.readString();
    custom[x] = serialInput.toInt(); // no error checking necessary because custom is a byte, so it will automatically convert it if an invalid value is given
    EEPROM.write(30+x, custom[x]);
    Serial.print(custom[x]);
  }
  Serial.println();
  EEPROM.commit();
  Serial.println(); Serial.print("Custom colors ");
  blinkLEDs(2);
}

byte inputInterpreter(String input) { // Checks inputs for a preceding colon and converts said input to byte
  if (input[0] == ':') { // Check if user input special characters
    input.remove(0, 1); // Remove colon
    int inputInt = input.toInt(); // Convert to integer
    if (inputInt >= 0 && inputInt < specialLength) { // Checks to make sure length matches
      inputBuffer = specialByte[inputInt];
      Serial.print(specialKeys[inputInt]); // Print within function for easier access
      Serial.print(" "); // Space for padding
      return 1;
    }
    Serial.println(); Serial.println("Invalid code added, please try again."); return 2;
  }
  else if (input[0] != ':' && input.length() > 3){ Serial.println(); Serial.println("Invalid, please try again."); return 2; }
  else return 0;
}

void remapSerial() {
  Serial.println("Welcome to the serial remapper!");
  // Buffer variables (puting these at the root of the relevant scope to reduce memory overhead)
  byte input = 0;

  // Print current EEPROM values
  Serial.print("Current values are: ");
  for (int x = 0; x < numkeys; x++) {
    for (int y = 0; y < 3; y++) {
      byte mapCheck = mapping[x][y];
      if (mapCheck != 0){ // If not null...
        // Print if regular character (prints as a char)
        if (mapCheck > 33 && mapCheck < 126) Serial.print(char(mapping[x][y]));
        // Otherwise, check it through the byte array and print the text version of the key.
        else for (int z = 0; z < specialLength; z++) if (specialByte[z] == mapCheck){
          Serial.print(specialKeys[z]);
          Serial.print(" ");
        }
      }
    }
    // Print delineation
    if (x < (numkeys - 1)) Serial.print(", ");
  }
  Serial.println();
  // End of print

  // Take serial inputs
  Serial.println("Please input special keys first and then a printable character.");
  Serial.println();
  Serial.println("For special keys, please enter a colon and then the corresponding");
  Serial.println("number (example: ctrl = ':1')");
  // Print all special keys
  byte lineLength = 0;

  // Print table of special values
  for (int y = 0; y < 67; y++) Serial.print("-");
  Serial.println();
  for (int x = 0; x < specialLength; x++) {
    // Make every line wrap at 30 characters
    byte spLength = specialKeys[x].length(); // save as variable within for loop for repeated use
    lineLength += spLength + 6;
    Serial.print(specialKeys[x]);
    spLength = 9 - spLength;
    for (spLength; spLength > 0; spLength--) { // Print a space
      Serial.print(" ");
      lineLength++;
    }
    if (x > 9) lineLength++;
    Serial.print(" = ");
    if (x <= 9) {
      Serial.print(" ");
      lineLength+=2;
    }
    Serial.print(x);
    if (x != specialLength) Serial.print(" | ");
    if (lineLength > 55) {
      lineLength = 0;
      Serial.println();
    }
  }
  // Bottom line
  if ((specialLength % 4) != 0) Serial.println(); // Add a new line if table doesn't go to end
  for (int y = 0; y < 67; y++) Serial.print("-"); // Bottom line of table
  Serial.println();
  Serial.println("If you want two or fewer modifiers for a key and");
  Serial.println("no printable characters, finish by entering 'xx'");
  // End of table

  for (int x = 0; x < numkeys; x++) { // Main for loop for each key

    byte y = 0; // External loop counter for while loop
    byte z = 0; // quickfix for bug causing wrong input slots to be saved
    while (true) {
      while(!Serial.available()){ fastCycle(); }
      String serialInput = Serial.readString();
      byte loopV = inputInterpreter(serialInput);

      // If key isn't converted
      if (loopV == 0){ // Save to array and EEPROM and quit; do and break
        // If user finishes key
        if (serialInput[0] == 'x' && serialInput[1] == 'x') { // Break if they use the safe word
          for (y; y < 3; y++) { // Overwrite with null values (0 char = null)
            EEPROM.write((40+(x*3)+y), 0);
            mapping[x][y] = 0;
          }
          if (x < numkeys-1) Serial.print("(finished key,) ");
          if (x == numkeys-1) Serial.print("(finished key)");
          break;
        }
        // If user otherwise finishes inputs
        Serial.print(serialInput); // Print once
        if (x < 5) Serial.print(", ");
        for (y; y < 3; y++) { // Normal write/finish
          EEPROM.write((40+(x*3)+y), int(serialInput[y-z]));
          mapping[x][y] = serialInput[y-z];
        }
        break;
      }

      // If key is converted
      if (loopV == 1){ // save input buffer into slot and take another serial input; y++ and loop
        EEPROM.write((40+(x*3)+y), inputBuffer);
        mapping[x][y] = inputBuffer;
        y++;
        z++;
      }

      // If user input is invalid, print keys again.
      if (loopV == 2){
        for (int a = 0; a < x; a++) {
          for (int d = 0; d < 3; d++) {
            byte mapCheck = int(mapping[a][d]);
            if (mapCheck != 0){ // If not null...
              // Print if regular character (prints as a char)
              if (mapCheck > 33 && mapCheck < 126) Serial.print(mapping[a][d]);
              // Otherwise, check it through the byte array and print the text version of the key.
              else for (int c = 0; c < specialLength; c++) if (specialByte[c] == mapCheck){
                Serial.print(specialKeys[c]);
                // Serial.print(" ");
              }
            }
          }
          // Print delineation
          Serial.print(", ");
        }
        if (y > 0) { // Run through rest of current key if any inputs were already entered
          for (int d = 0; d < y; d++) {
            byte mapCheck = int(mapping[x][d]);
            if (mapCheck != 0){ // If not null...
              // Print if regular character (prints as a char)
              if (mapCheck > 33 && mapCheck < 126) Serial.print(mapping[x][d]);
              // Otherwise, check it through the byte array and print the text version of the key.
              else for (int c = 0; c < specialLength; c++) if (specialByte[c] == mapCheck){
                Serial.print(specialKeys[c]);
                Serial.print(" ");
              }
            }
          }
        }

      }
    } // Mapping loop
  } // Key for loop
  EEPROM.commit();
  Serial.println(); Serial.println(); Serial.print("Mapping ");

} // Remapper loop

#endif

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
  for (byte x=0; x<=numkeys; x++){
    if (pressed[x] && pressedLock[x]) { for (byte y=0; y<3; y++) { Keyboard.press(mapping[x][y]); } pressedLock[x] = 0; }
    if (!pressed[x] && !pressedLock[x]){ for (byte y=0; y<3; y++) { Keyboard.release(mapping[x][y]); } pressedLock[x] = 1; }
  }
}
