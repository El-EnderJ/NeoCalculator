/**
 * SerialBridge.h
 * PC → NumOS keyboard bridge via Serial Monitor.
 *
 * Reads characters from Serial.read() (non-blocking) and
 * translates them into KeyEvent structs that SystemApp understands.
 *
 * Mapping (PC → Calculator):
 *   w/s/a/d      → UP / DOWN / LEFT / RIGHT
 *   Enter / z    → ENTER (OK/EXE)
 *   Backspace / x→ DEL
 *   Escape / h   → MODE (HOME)
 *   c            → AC   (Clear All)
 *   0–9          → NUM_0..NUM_9
 *   + - * /      → ADD SUB MUL DIV
 *   .            → DOT
 *   ^            → POW
 *   (            → LPAREN
 *   )            → RPAREN
 *   f            → SHIFT+DIV (Fraction)
 *   S            → SHIFT
 *   g            → GRAPH
 *   t            → SIN  (trig shortcut)
 */

#pragma once

#include <Arduino.h>
#include "input/KeyMatrix.h"   // For KeyEvent, KeyAction, KeyCode

class SerialBridge {
public:
    SerialBridge();

    /// Call once in setup() after Serial.begin()
    void begin();

    /// Non-blocking: reads available serial chars, pushes events.
    /// Returns true if at least one event was generated.
    bool pollEvent(KeyEvent &outEvent);

private:
    // Small circular buffer for generated events
    static const int BUF_SIZE = 8;
    KeyEvent _buf[BUF_SIZE];
    int _head;
    int _tail;

    void push(KeyCode code, const char* label);
    bool pop(KeyEvent &out);
    void processChar(int ch);
};
