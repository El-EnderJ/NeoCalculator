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

// ── Estructura completa de KeyEvent (usa KeyCode definido arriba) ────────
struct KeyEvent {
    KeyCode   code   = KeyCode::NONE;
    KeyAction action = KeyAction::NONE;
    int       row    = -1;
    int       col    = -1;
};
