/**
 * KeyMatrix.cpp
 */

#include "KeyMatrix.h"

KeyMatrix::KeyMatrix() {
    // Reset state
    for(int r=0; r<6; r++) {
        for(int c=0; c<8; c++) {
            _keyState[r][c] = 0;
            _lastDebounceTime[r][c] = 0;
            _map[r][c] = KeyCode::NONE;
        }
    }

    // --- Define Layout Map ---
    // Row 0
    _map[0][0] = KeyCode::SHIFT;  _map[0][1] = KeyCode::ALPHA;  _map[0][2] = KeyCode::MODE;  _map[0][3] = KeyCode::SETUP;
    _map[0][4] = KeyCode::F1;     _map[0][5] = KeyCode::F2;     _map[0][6] = KeyCode::F3;    _map[0][7] = KeyCode::F4;

    // Row 1
    _map[1][0] = KeyCode::ON;     _map[1][1] = KeyCode::AC;     _map[1][2] = KeyCode::DEL;   _map[1][3] = KeyCode::FREE_EQ;
    _map[1][4] = KeyCode::LEFT;   _map[1][5] = KeyCode::UP;     _map[1][6] = KeyCode::DOWN;  _map[1][7] = KeyCode::RIGHT;

    // Row 2
    _map[2][0] = KeyCode::VAR_X;  _map[2][1] = KeyCode::VAR_Y;  _map[2][2] = KeyCode::TABLE; _map[2][3] = KeyCode::GRAPH;
    _map[2][4] = KeyCode::ZOOM;   _map[2][5] = KeyCode::TRACE;  _map[2][6] = KeyCode::SHOW_STEPS; _map[2][7] = KeyCode::SOLVE;

    // Row 3
    _map[3][0] = KeyCode::NUM_7;  _map[3][1] = KeyCode::NUM_8;  _map[3][2] = KeyCode::NUM_9; _map[3][3] = KeyCode::LPAREN;
    _map[3][4] = KeyCode::RPAREN; _map[3][5] = KeyCode::DIV;    _map[3][6] = KeyCode::POW;   _map[3][7] = KeyCode::SQRT;

    // Row 4
    _map[4][0] = KeyCode::NUM_4;  _map[4][1] = KeyCode::NUM_5;  _map[4][2] = KeyCode::NUM_6; _map[4][3] = KeyCode::MUL;
    _map[4][4] = KeyCode::SUB;    _map[4][5] = KeyCode::SIN;    _map[4][6] = KeyCode::COS;   _map[4][7] = KeyCode::TAN;

    // Row 5
    _map[5][0] = KeyCode::NUM_1;  _map[5][1] = KeyCode::NUM_2;  _map[5][2] = KeyCode::NUM_3; _map[5][3] = KeyCode::ADD;
    _map[5][4] = KeyCode::NEG;    _map[5][5] = KeyCode::NUM_0;  _map[5][6] = KeyCode::DOT;   _map[5][7] = KeyCode::ENTER;
}

void KeyMatrix::begin() {
    // Rows as Inputs with Pullup
    for (int i = 0; i < 6; i++) {
        pinMode(_rowPins[i], INPUT_PULLUP);
    }
    // Cols as Outputs, default HIGH (inactive)
    for (int i = 0; i < 8; i++) {
        pinMode(_colPins[i], OUTPUT);
        digitalWrite(_colPins[i], HIGH);
    }
}

void KeyMatrix::update() {
    uint32_t now = millis();

    // Iterate Columns
    for (int c = 0; c < 8; c++) {
        // Activate Column (LOW)
        digitalWrite(_colPins[c], LOW);
        // Short delay for signal settling? usually not needed on ESP32 ~microsecs
        // delayMicroseconds(2); 

        // Read Rows
        for (int r = 0; r < 6; r++) {
            // LOW means pressed because of Pull-Up
            bool pressed = (digitalRead(_rowPins[r]) == LOW);
            
            // Debounce
            if (pressed != (_keyState[r][c] == 1)) {
                if (now - _lastDebounceTime[r][c] > KEY_DEBOUNCE_MS) {
                    _lastDebounceTime[r][c] = now;
                    _keyState[r][c] = pressed ? 1 : 0;
                    
                    KeyEvent ev;
                    ev.code = _map[r][c];
                    ev.row = r;
                    ev.col = c;
                    ev.action = pressed ? KeyAction::PRESS : KeyAction::RELEASE;
                    
                    if (ev.code != KeyCode::NONE) {
                        pushEvent(ev);
                    }
                }
            }
        }
        
        // Deactivate Column (HIGH)
        digitalWrite(_colPins[c], HIGH);
    }
}

bool KeyMatrix::pollEvent(KeyEvent &outEvent) {
    if (_head == _tail) return false; // Empty
    
    outEvent = _eventBuf[_tail];
    _tail = (_tail + 1) % EVENT_BUF_SIZE;
    return true;
}

void KeyMatrix::pushEvent(KeyEvent ev) {
    int next = (_head + 1) % EVENT_BUF_SIZE;
    if (next != _tail) { // Not full
        _eventBuf[_head] = ev;
        _head = next;
    }
}
