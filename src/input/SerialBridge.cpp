/**
 * SerialBridge.cpp
 * PC → NumOS keyboard bridge.
 * Non-blocking Serial reader that generates KeyEvent structs.
 */

#include "SerialBridge.h"
#include "math/giac/GiacBridge.h"
#include <ctype.h>
#include <string>
#include <algorithm>

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
    // Use a std::string buffer to accumulate characters until newline.
    static std::string inputBuffer;
    static unsigned long lastEnterMs = 0;

    // Echo and accumulate printable characters
    if (ch >= 0x20 && ch <= 0x7E) {
        Serial.printf("[SB] RX: '%c' (0x%02X)\n", (char)ch, ch);
        if (inputBuffer.size() < 255) inputBuffer.push_back((char)ch);
    } else {
        Serial.printf("[SB] RX: 0x%02X\n", ch);
        // Backspace/delete should edit the current buffer if present
        if ((ch == 8 || ch == 127) && !inputBuffer.empty()) {
            inputBuffer.pop_back();
            Serial.println("[SB] (buffer) DEL");
        }
    }

    // Handle Enter / newline (debounce CR+LF pairs)
    if (ch == '\r' || ch == '\n') {
        unsigned long now = millis();
        if (now - lastEnterMs < 50) {
            // duplicate newline from CR/LF, ignore
            return;
        }
        lastEnterMs = now;

        // Trim leading/trailing whitespace
        auto lpos = inputBuffer.find_first_not_of(" \t\r\n");
        auto rpos = inputBuffer.find_last_not_of(" \t\r\n");
        std::string line;
        if (lpos == std::string::npos) {
            // Empty line -> treat as ENTER key
            inputBuffer.clear();
            push(KeyCode::ENTER, "ENTER (PC-Enter)");
            return;
        } else {
            line = inputBuffer.substr(lpos, rpos - lpos + 1);
        }

        // If line starts with ':' → Giac command
        if (!line.empty() && line[0] == ':') {
            String in(line.c_str());
            if (in.startsWith(":")) {
                in = in.substring(1);
            }
            in.trim();
            String out = solveWithGiac(in);
            Serial.print(": => ");
            Serial.println(out);
            inputBuffer.clear();
            return;
        }

        // Case-insensitive keyword checks (HOME, AC, DEL, ENTER, EXE, F1, F2)
        std::string upper = line;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c){ return std::toupper(c); });
        if (upper == "HOME") { push(KeyCode::MODE, "MODE/HOME"); inputBuffer.clear(); return; }
        if (upper == "AC")   { push(KeyCode::AC, "AC"); inputBuffer.clear(); return; }
        if (upper == "DEL")  { push(KeyCode::DEL, "DEL"); inputBuffer.clear(); return; }
        if (upper == "ENTER") { push(KeyCode::ENTER, "ENTER (PC-Enter)"); inputBuffer.clear(); return; }
        if (upper == "EXE")  { push(KeyCode::EXE, "EXE"); inputBuffer.clear(); return; }
        if (upper == "F1")   { push(KeyCode::F1, "F1"); inputBuffer.clear(); return; }
        if (upper == "F2")   { push(KeyCode::F2, "F2"); inputBuffer.clear(); return; }

        // Single character line: map to key codes (preserve previous mappings)
        if (line.size() == 1) {
            char c = line[0];
            switch (c) {
                case 'w': case 'W': push(KeyCode::UP,    "UP");    break;
                case 'a':           push(KeyCode::LEFT,  "LEFT");  break;
                case 'd': case 'D': push(KeyCode::RIGHT, "RIGHT"); break;
                case 's':           push(KeyCode::DOWN,  "DOWN");  break;

                case 'S':  push(KeyCode::SHIFT, "SHIFT"); break;
                case 'A':  push(KeyCode::ALPHA, "ALPHA"); break;

                case 8: case 127: push(KeyCode::DEL, "DEL"); break;
                case 0x1B:      push(KeyCode::AC,    "AC");        break;
                case 'c':       push(KeyCode::AC,    "AC");        break;
                case 'h': case 'H': push(KeyCode::MODE,  "MODE/HOME"); break;
                case 'b': case 'B': push(KeyCode::AC,    "BACK (AC)"); break;

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

                case '+': push(KeyCode::ADD, "+"); break;
                case '-': push(KeyCode::SUB, "-"); break;
                case '*': push(KeyCode::MUL, "*"); break;
                case '/': push(KeyCode::DIV, "/"); break;
                case '.': push(KeyCode::DOT, "."); break;
                case '(' : push(KeyCode::LPAREN, "("); break;
                case ')' : push(KeyCode::RPAREN, ")"); break;

                case 'p': case '^': push(KeyCode::POW, "POW"); break;
                case '=': push(KeyCode::FREE_EQ, "="); break;
                case 'f': push(KeyCode::DIV, "DIV (FRAC)"); break;
                case 'x': push(KeyCode::VAR_X, "VAR_X"); break;
                case 'y': push(KeyCode::VAR_Y, "VAR_Y"); break;
                case 'g': push(KeyCode::GRAPH, "GRAPH"); break;
                case 't': push(KeyCode::SIN,   "SIN");   break;
                case 'r': push(KeyCode::SQRT,  "SQRT");  break;
                case 'R':  push(KeyCode::SHIFT, "SHIFT"); push(KeyCode::SQRT, "SQRT (=nthROOT)"); break;
                case 'n':  push(KeyCode::SHOW_STEPS, "STEPS"); break;
                case 'u':  push(KeyCode::FREE_EQ, "= (FREE_EQ)"); break;
                case 'C':  push(KeyCode::AC, "AC"); break;
                case '<':  push(KeyCode::EXE, "EXE"); break;
                case 'F':  push(KeyCode::F1, "F1"); break;
                case 'G':  push(KeyCode::F2, "F2"); break;
                default:
                    // Unrecognized single char: echo it
                    Serial.print("[SB] Unmapped char: ");
                    Serial.println(line.c_str());
                    break;
            }
            inputBuffer.clear();
            return;
        }

        // Unrecognized multi-char line: echo back and clear
        Serial.print("[SB] Unrecognized line: ");
        Serial.println(line.c_str());
        inputBuffer.clear();
        return;
    }
}
