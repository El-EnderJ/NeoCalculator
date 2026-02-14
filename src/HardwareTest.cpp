/**
 * HardwareTest.cpp
 * Routine to test the 6x8 Key Matrix and TFT Display.
 * 
 * Instructions:
 * To run this test, you can temporarily rename this file to main.cpp 
 * (and rename the original main.cpp to main.cpp.bak), or call `runHardwareTest()` 
 * from your existing main.cpp setup/loop.
 */

#include <Arduino.h>
#include "Config.h"
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"

// Utiliza las instancias globales o crea locales para el test
static DisplayDriver tft;
static KeyMatrix keys;

void runHardwareTestSetup() {
    Serial.begin(115200);
    Serial.println("Hardware Test Start");

    tft.begin();
    tft.clear(TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.setTextSize(2);
    tft.drawText(10, 10, "HARDWARE TEST");
    tft.setTextSize(1);
    tft.drawText(10, 40, "Press any key...");
    tft.drawText(10, 60, "R: Row, C: Col");

    keys.begin();
}

void runHardwareTestLoop() {
    keys.update();

    KeyEvent ev;
    while (keys.pollEvent(ev)) {
        if (ev.action == KeyAction::PRESS) {
            tft.fillRect(0, 80, 240, 240, TFT_BLACK); // Clear area
            
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextSize(3);
            
            String keyDesc = "Key: " + String((int)ev.code);
            tft.drawText(20, 100, keyDesc);

            // Reverse engineer row/col from code is hard without map access, 
            // but we can just show the code.
            // For debug, printing to Serial is also useful:
            Serial.printf("Key Pressed: %d\n", (int)ev.code);

            tft.setTextSize(2);
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.drawText(20, 150, "Action: PRESS");
        } else if (ev.action == KeyAction::RELEASE) {
            tft.fillRect(20, 150, 200, 30, TFT_BLACK);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextSize(2);
            tft.drawText(20, 150, "Action: RELEASE");
            Serial.printf("Key Released: %d\n", (int)ev.code);
        }
    }
    
    delay(10);
}

// Uncomment main() to use this file as standalone entry point
/*
void setup() {
    runHardwareTestSetup();
}

void loop() {
    runHardwareTestLoop();
}
*/
