/**
 * SerialBridge.cpp
 * PC → NumOS keyboard bridge.
 * Non-blocking Serial reader that generates KeyEvent structs.
 */

#include "SerialBridge.h"

SerialBridge::SerialBridge()
    : _head(0), _tail(0)
{
    memset(_buf, 0, sizeof(_buf));
}

void SerialBridge::begin() {
    Serial.println("[SerialBridge] PC keyboard bridge active.");
    Serial.println("[SerialBridge] w/a/s/d=arrows  z=OK  x=DEL  h=HOME  c=AC");
    Serial.println("[SerialBridge] 0-9 +-*/. ^()  f=FRAC  r=SQRT  R=nthROOT  S=SHIFT");
}

// ── Circular buffer helpers ──

void SerialBridge::push(KeyCode code, const char* label) {
    KeyEvent ev;
    ev.code   = code;
    ev.action = KeyAction::PRESS;
    ev.row    = -1;  // Virtual key (no physical row/col)
    ev.col    = -1;

    int next = (_head + 1) % BUF_SIZE;
    if (next == _tail) return;  // Buffer full, drop event

    _buf[_head] = ev;
    _head = next;

    // Debug feedback
    Serial.print("[Key] PC Input: '");
    Serial.print(label);
    Serial.print("' -> Action: ");
    Serial.println(label);
}

bool SerialBridge::pop(KeyEvent &out) {
    if (_tail == _head) return false;
    out = _buf[_tail];
    _tail = (_tail + 1) % BUF_SIZE;
    return true;
}

// ── Main API ──

bool SerialBridge::pollEvent(KeyEvent &outEvent) {
    // 1. Read all available serial chars and convert to events
    while (Serial.available() > 0) {
        int ch = Serial.read();
        processChar(ch);
    }

    // 2. Pop one event from queue
    return pop(outEvent);
}

// ── Character → KeyCode mapping ──

void SerialBridge::processChar(int ch) {
    // Deduplicate \r\n: if we get \n within 50ms of \r, skip it
    static unsigned long lastEnterMs = 0;
    if (ch == '\r' || ch == '\n') {
        unsigned long now = millis();
        if (now - lastEnterMs < 50) return; // Skip duplicate
        lastEnterMs = now;
        push(KeyCode::ENTER, "ENTER");
        return;
    }

    switch (ch) {
        // ── Navigation (WASD) ──
        case 'w': case 'W': push(KeyCode::UP,    "UP");    break;
        case 's':           push(KeyCode::DOWN,  "DOWN");  break;
        case 'a':           push(KeyCode::LEFT,  "LEFT");  break;
        case 'd':           push(KeyCode::RIGHT, "RIGHT"); break;

        // ── Actions ──
        case 'z': case 'Z': push(KeyCode::ENTER, "ENTER"); break;
        case 8:             // Backspace (ASCII 8)
        case 127:           // Delete (ASCII 127)
        case 'x': case 'X': push(KeyCode::DEL,   "DEL");   break;
        case 'h': case 'H': push(KeyCode::MODE,  "MODE/HOME"); break;
        case 'c': case 'C': push(KeyCode::AC,    "AC");    break;

        // ── Digits ──
        case '0': push(KeyCode::NUM_0, "0"); break;
        case '1': push(KeyCode::NUM_1, "1"); break;
        case '2': push(KeyCode::NUM_2, "2"); break;
        case '3': push(KeyCode::NUM_3, "3"); break;
        case '4': push(KeyCode::NUM_4, "4"); break;
        case '5': push(KeyCode::NUM_5, "5"); break;
        case '6': push(KeyCode::NUM_6, "6"); break;
        case '7': push(KeyCode::NUM_7, "7"); break;
        case '8': push(KeyCode::NUM_8, "8"); break;
        case '9': push(KeyCode::NUM_9, "9"); break;

        // ── Operators ──
        case '+': push(KeyCode::ADD, "+"); break;
        case '-': push(KeyCode::SUB, "-"); break;
        case '*': push(KeyCode::MUL, "*"); break;
        case '/': push(KeyCode::DIV, "/"); break;
        case '^': push(KeyCode::POW, "^"); break;
        case '.': push(KeyCode::DOT, "."); break;
        case '(': push(KeyCode::LPAREN, "("); break;
        case ')': push(KeyCode::RPAREN, ")"); break;

        // ── Special function keys ──
        case 'f': case 'F':
            // Fraction: emit SHIFT then DIV (SHIFT+DIV triggers fraction in CalculationApp)
            push(KeyCode::SHIFT, "SHIFT");
            push(KeyCode::DIV,   "DIV (=FRAC)");
            break;

        case 'S':  push(KeyCode::SHIFT, "SHIFT"); break;
        case 'g':  push(KeyCode::GRAPH, "GRAPH"); break;
        case 't':  push(KeyCode::SIN,   "SIN");   break;

        // ── Roots ──
        case 'r':  push(KeyCode::SQRT,  "SQRT");  break;
        case 'R':  // nth root: SHIFT+SQRT
            push(KeyCode::SHIFT, "SHIFT");
            push(KeyCode::SQRT,  "SQRT (=nthROOT)");
            break;

        // ── Variables ──
        case 'y':  push(KeyCode::VAR_Y, "VAR_Y"); break;

        // Ignore everything else silently
        default: break;
    }
}
