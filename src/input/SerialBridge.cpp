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
    Serial.println("┌───────────────────────────────────────────────┐");
    Serial.println("│         NumOS Serial Keyboard Map             │");
    Serial.println("├──────────────┬────────────────────────────────┤");
    Serial.println("[│  0-9 . + - * /  │  Digits & operators        │");
    Serial.println("│  = or u           │  EQUALS symbol (not EXE)  │");
    Serial.println("│  Enter (\\r/\\n)  │  EXE / OK                 │");
    Serial.println("│  Backspace/Del   │  DEL (erase char)         │");
    Serial.println("│  c  or  Esc      │  AC  (all clear)          │");
    Serial.println("│  p  or  ^        │  POW (exponent)           │");
    Serial.println("│  x               │  VAR_X                    │");
    Serial.println("│  y               │  VAR_Y                    │");
    Serial.println("│  f               │  FRAC (fraction)          │");
    Serial.println("│  r               │  SQRT (square root)       │");
    Serial.println("│  R (shift+r)     │  nthROOT (SHIFT+SQRT)     │");
    Serial.println("│  n               │  STEPS (step-by-step)     │");
    Serial.println("│  ( )             │  Parentheses              │");
    Serial.println("│  w a s d         │  UP LEFT DOWN RIGHT       │");
    Serial.println("│  S (shift+s)     │  SHIFT modifier           │");
    Serial.println("│  A (shift+a)     │  ALPHA modifier           │");
    Serial.println("│  h               │  MODE / HOME              │");
    Serial.println("│  g               │  GRAPH                    │");
    Serial.println("│  t               │  SIN                      │");
    Serial.println("└──────────────┴────────────────────────────────┘");
    Serial.println("[SerialBridge] Type a key and press Enter.");
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
    // Eco del caracter crudo recibido (confirma que la S3 recibe datos)
    if (ch >= 0x20 && ch <= 0x7E) {
        Serial.printf("[SB] RX: '%c' (0x%02X)\n", (char)ch, ch);
    } else {
        Serial.printf("[SB] RX: 0x%02X\n", ch);
    }

    // Deduplicate \r\n: if we get \n within 50ms of \r, skip it
    static unsigned long lastEnterMs = 0;
    if (ch == '\r' || ch == '\n') {
        unsigned long now = millis();
        if (now - lastEnterMs < 50) return; // Skip duplicate \r\n pair
        lastEnterMs = now;
        push(KeyCode::ENTER, "ENTER (PC-Enter)");
        return;
    }

    switch (ch) {
        // ── Navigation (WASD) ──
        case 'w': case 'W': push(KeyCode::UP,    "UP");    break;
        case 'a':           push(KeyCode::LEFT,  "LEFT");  break;
        case 'd': case 'D': push(KeyCode::RIGHT, "RIGHT"); break;
        case 's':           push(KeyCode::DOWN,  "DOWN");  break;

        // ── Modifiers ──
        case 'S':  push(KeyCode::SHIFT, "SHIFT"); break;
        case 'A':  push(KeyCode::ALPHA, "ALPHA"); break;

        // ── Actions ──
        case 8:             // Backspace (ASCII 8)
        case 127:           // Delete (ASCII 127)
            push(KeyCode::DEL, "DEL"); break;
        case 0x1B:          // Escape
        case 'c':           push(KeyCode::AC,    "AC");        break;
        case 'h': case 'H': push(KeyCode::MODE,  "MODE/HOME"); break;

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
        case '.': push(KeyCode::DOT, "."); break;
        case '(': push(KeyCode::LPAREN, "("); break;
        case ')': push(KeyCode::RPAREN, ")"); break;

        // ── Power: 'p' or '^' ──
        case 'p': case '^': push(KeyCode::POW, "POW"); break;

        // ── Equals symbol (for equations, NOT EXE) ──
        case '=': push(KeyCode::FREE_EQ, "="); break;

        // ── Fraction ──
        case 'f': push(KeyCode::DIV, "DIV (FRAC)"); break;

        // ── Variables ──
        case 'x': push(KeyCode::VAR_X, "VAR_X"); break;
        case 'y': push(KeyCode::VAR_Y, "VAR_Y"); break;

        // ── Functions ──
        case 'g':  push(KeyCode::GRAPH, "GRAPH"); break;
        case 't':  push(KeyCode::SIN,   "SIN");   break;

        // ── Roots ──
        case 'r':  push(KeyCode::SQRT,  "SQRT");  break;
        case 'R':  // nth root: SHIFT+SQRT
            push(KeyCode::SHIFT, "SHIFT");
            push(KeyCode::SQRT,  "SQRT (=nthROOT)");
            break;

        // ── Steps ──
        case 'n':  push(KeyCode::SHOW_STEPS, "STEPS"); break;

        // ── Equals (for equations, NOT EXE) ──
        case 'u':  push(KeyCode::FREE_EQ, "= (FREE_EQ)"); break;

        // ── Misc (uppercase C = AC too) ──
        case 'C':  push(KeyCode::AC, "AC"); break;

        // Ignore everything else silently
        default: break;
    }
}
