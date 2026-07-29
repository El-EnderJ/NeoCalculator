/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include "input/KeyMatrix.h"   // For KeyEvent, KeyAction, KeyCode

class SerialBridge {
public:
    using LineHandler = bool (*)(const char* line, void* context);

    SerialBridge();

    /// Call once in setup() after Serial.begin()
    void begin();

    /// Non-blocking: reads available serial chars, pushes events.
    /// Returns true if at least one event was generated.
    bool pollEvent(KeyEvent &outEvent);
    void setLineHandler(LineHandler handler, void* context);

private:
    // Small circular buffer for generated events
    static const int BUF_SIZE = 32;
    KeyEvent _buf[BUF_SIZE];
    int _head;
    int _tail;
    LineHandler _lineHandler = nullptr;
    void* _lineHandlerContext = nullptr;

    void push(KeyCode code, const char* label);
    bool pop(KeyEvent &out);
    void processChar(int ch);
};
