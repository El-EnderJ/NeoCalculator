/**
 * HardwareTest.cpp
 * 
 * Simple test to verify:
 * 1. Keypad scanning (prints Row/Col or Key Name to Serial/LCD)
 * 2. Display functionality.
 * 
 * Usage: Swap this with main.cpp or include in main logic to test.
 */

#include <Arduino.h>
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"

// Globals for test
static DisplayDriver tftTest;
static KeyMatrix keyTest;

void setupTest() {
    Serial.begin(115200);
    
    tftTest.begin();
    tftTest.clear(TFT_BLACK);
    tftTest.setTextColor(TFT_WHITE, TFT_BLACK);
    tftTest.setTextSize(2);
    tftTest.drawText(10, 10, "HARDWARE TEST");
    tftTest.setTextSize(1);
    tftTest.drawText(10, 40, "Press any key...");
}

void loopTest() {
    keyTest.update();
    
    KeyEvent ev;
    while(keyTest.pollEvent(ev)) {
        if (ev.action == KeyAction::PRESS) {
            String msg = "Key: ";
            // Simple mapping to text manually or just code
            msg += String((int)ev.code);
            msg += " R=" + String(ev.row) + " C=" + String(ev.col);
            
            Serial.println(msg);
            
            tftTest.tft().fillRect(0, 60, 240, 20, TFT_BLACK); // Clear line
            tftTest.drawText(10, 60, msg);
        }
    }
    delay(10);
}
