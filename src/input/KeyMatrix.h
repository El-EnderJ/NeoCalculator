/**
 * KeyMatrix.h
 * Driver for 6x8 Keypad Matrix.
 * Rows are Inputs with Pull-Up. Columns are Outputs (Active Low scan).
 */

#pragma once

#include <Arduino.h>
#include "../Config.h"
#include "KeyCodes.h"   // KeyCode, KeyAction, KeyEvent

class KeyMatrix {
public:
    KeyMatrix();

    void begin();
    void update(); // Call this frequently (e.g. in loop)
    
    // Returns true if there was an event
    bool pollEvent(KeyEvent &outEvent);

private:
    // 6 Rows (Inputs)
    const int _rowPins[6] = { PIN_KEY_R0, PIN_KEY_R1, PIN_KEY_R2, PIN_KEY_R3, PIN_KEY_R4, PIN_KEY_R5 };
    // 8 Cols (Outputs)
    const int _colPins[8] = { PIN_KEY_C0, PIN_KEY_C1, PIN_KEY_C2, PIN_KEY_C3, PIN_KEY_C4, PIN_KEY_C5, PIN_KEY_C6, PIN_KEY_C7 };

    // State
    uint8_t _keyState[6][8];
    uint32_t _lastDebounceTime[6][8];
    
    // Mapping table
    KeyCode _map[6][8];

    // Event Buffer
    static const int EVENT_BUF_SIZE = 16;
    KeyEvent _eventBuf[EVENT_BUF_SIZE];
    int _head = 0;
    int _tail = 0;

    void pushEvent(KeyEvent ev);
};
