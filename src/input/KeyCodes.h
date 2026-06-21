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
 * KeyCodes.h
 * Logical key codes for the ESP32 calculator keypad.
 */

#pragma once

#include <stdint.h>

// ── Acciones de tecla (movido aquí desde KeyMatrix.h para portabilidad) ──
enum class KeyAction {
    NONE = 0,
    PRESS,
    RELEASE,
    REPEAT
};

// ── Códigos lógicos de tecla ─────────────────────────────────────────────
enum class KeyCode : uint8_t {
    NONE = 0,

    // Top row: modes and softkeys
    SHIFT,      // R0C0
    ALPHA,      // R0C1
    MODE,       // R0C2
    SETUP,      // R0C3
    F1,         // R0C4
    F2,         // R0C5
    F3,         // R0C6
    F4,         // R0C7

    // Row 1: control & navigation
    ON,         // R1C0
    AC,         // R1C1
    DEL,        // R1C2
    FREE_EQ,    // R1C3
    LEFT,       // R1C4
    UP,         // R1C5
    DOWN,       // R1C6
    RIGHT,      // R1C7

    // Row 2
    VAR_X,      // R2C0
    VAR_Y,      // R2C1
    TABLE,      // R2C2
    GRAPH,      // R2C3
    ZOOM,       // R2C4
    TRACE,      // R2C5
    SHOW_STEPS, // R2C6
    SOLVE,      // R2C7

    // Row 3
    NUM_7, NUM_8, NUM_9, 
    LPAREN, RPAREN, DIV, POW, SQRT,

    // Row 4
    NUM_4, NUM_5, NUM_6,
    MUL, SUB, SIN, COS, TAN,

    // Row 5
    NUM_1, NUM_2, NUM_3,
    ADD, NEG, NUM_0, DOT, ENTER,
    
    // Virtual Keys or extras
    F5,          // F5 (top-row function key)
    EXE,         // Execute / Solve (mapped to physical '<' key)
    
    // Virtual keys for functions/constants (via SHIFT combos or serial)
    LN,          // ln (logaritmo natural)
    LOG,         // log (logaritmo base 10)
    LOG_BASE,    // log_n(x) (logaritmo de base custom)
    CONST_PI,    // π
    CONST_E,     // e (Euler)

    // ── Variables y Ans ──
    ANS,         // Ans (último resultado)
    PREANS,      // PreAns (penúltimo resultado)
    STO,         // Store (modo almacenamiento)

    // ── Variables Alpha (A-F, x, y, z) ──
    // Se activan vía ALPHA + tecla numérica o vía serial
    ALPHA_A,
    ALPHA_B,
    ALPHA_C,
    ALPHA_D,
    ALPHA_E,
    ALPHA_F,

    // ── Negación (cambio de signo) ──
    NEGATE,      // (−) toggle signo

    // ── Funciones avanzadas ──
    FACT,        // Factorización en primos (SHIFT + botón asignado)
};

// ── Mapeo de tecla numérica → valor de dígito ────────────────────────────
// El enum KeyCode coloca NUM_0..NUM_9 en el orden FÍSICO del teclado
// (NUM_7,8,9 → NUM_4,5,6 → NUM_1,2,3 → NUM_0 al final, junto a ENTER), de modo
// que los dígitos NO son contiguos y NUM_0 > NUM_9 numéricamente. Un test de
// rango como `code >= NUM_0 && code <= NUM_9` es por tanto SIEMPRE falso, y la
// aritmética `code - NUM_0` también depende del orden. Esta función explícita no
// depende de la ordenación del enum, no asigna memoria y es segura tanto en
// firmware como en el emulador host. Devuelve 0–9 para un dígito, o -1 si no.
inline int keyCodeDigitValue(KeyCode code) {
    switch (code) {
        case KeyCode::NUM_0: return 0;
        case KeyCode::NUM_1: return 1;
        case KeyCode::NUM_2: return 2;
        case KeyCode::NUM_3: return 3;
        case KeyCode::NUM_4: return 4;
        case KeyCode::NUM_5: return 5;
        case KeyCode::NUM_6: return 6;
        case KeyCode::NUM_7: return 7;
        case KeyCode::NUM_8: return 8;
        case KeyCode::NUM_9: return 9;
        default:             return -1;
    }
}

// ── Estructura completa de KeyEvent (usa KeyCode definido arriba) ────────
struct KeyEvent {
    KeyCode   code   = KeyCode::NONE;
    KeyAction action = KeyAction::NONE;
    int       row    = -1;
    int       col    = -1;
};
