/**
 * Config.h
 * Pinout and global configuration for the NumOS calculator.
 *
 * Hardware target: ESP32-S3 N16R8 CAM (16 MB QIO Flash + 8 MB OPI PSRAM)
 * Display       : IPS TFT 320×240 — ILI9341
 *
 * ⚠ AJUSTA los pines de TFT y teclado a tu wiring real antes de flashear.
 *   Los valores de TFT ya están en platformio.ini como -DTFT_xxx=nn.
 *   Los valores de aquí se usan en el código C++ para documentación y
 *   para módulos que NO usan TFT_eSPI directamente.
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif

// ── Pantalla TFT (ESP32-S3 CAM, bus SPI2 / FSPI) ───────────────────────────
// Deben coincidir con los -DTFT_xxx de platformio.ini.
static const int PIN_TFT_MOSI =  13;
static const int PIN_TFT_SCLK =  12;
static const int PIN_TFT_CS   =  10;
static const int PIN_TFT_DC   =   4;
static const int PIN_TFT_RST  =   5;
static const int PIN_TFT_BL   =  45;   // Backlight (PWM)

// Tamaño lógico después de la rotación 1 (landscape: 320 w × 240 h)
static const uint16_t SCREEN_WIDTH    = 320;
static const uint16_t SCREEN_HEIGHT   = 240;
static const uint8_t  SCREEN_ROTATION = 1;

// ── Teclado físico 5×10 — hardware actual (PCB en progreso) ────────────────
//
// Escaneo: Filas = OUTPUT (LOW activo), Columnas = INPUT_PULLUP (LOW = pulsada).
//
// ✅ Conflicto GPIO 4/5 resuelto (2026-03-02):
//   C0 y C1 reasignados de GPIO 4 (TFT_DC) y GPIO 5 (TFT_RST)
//   a GPIO 6 y GPIO 7, que están libres.
//
// Filas (OUTPUT) — 5 filas → GPIO 1, 2, 41, 42, 40.
static const int KBD_ROW_PINS[5] = { 1, 2, 41, 42, 40 };

// Columnas (INPUT_PULLUP) — 10 posibles; solo las 3 primeras están cableadas.
// Cableado activo: C0=GPIO6, C1=GPIO7, C2=GPIO8
static const int KBD_COL_PINS[10] = { 6, 7, 8, 3, 15, 16, 17, 18, 21, 47 };

static const uint8_t KBD_ROWS           = 5;
static const uint8_t KBD_COLS           = 10;
static const uint8_t KBD_CONNECTED_COLS = 3;   // ← Aumentar al conectar más columnas

// ── Temporización del escaneo (compartida con ambos drivers) ────────────────
static const uint16_t KEY_SCAN_INTERVAL_MS    =  5;   // ms entre escaneos
static const uint16_t KEY_DEBOUNCE_MS         = 20;   // ms de anti-rebote
static const uint16_t KEY_AUTOREPEAT_DELAY_MS = 500;  // ms antes del autorepeat
static const uint16_t KEY_AUTOREPEAT_RATE_MS  =  80;  // ms entre REPEAT events

// ── CAS settings ────────────────────────────────────────────────────────────
extern bool setting_complex_enabled;   // true = show complex roots, false = "No real solutions"
extern int  setting_decimal_precision;  // number of decimal digits (6, 8, 10, 12)
extern bool setting_edu_steps;          // true = step-by-step educational mode for arithmetic

// ── Matriz legacy 6×8 (reservada / compatibilidad con KeyMatrix.h) ───────────
// Filas: INPUT_PULLUP.  Columnas: OUTPUT activo-LOW.
// Solo usado por la clase KeyMatrix; no conectado en el hardware actual.
static const int PIN_KEY_R0 =  1;
static const int PIN_KEY_R1 =  2;
static const int PIN_KEY_R2 =  3;
static const int PIN_KEY_R3 =  4;
static const int PIN_KEY_R4 =  5;
static const int PIN_KEY_R5 =  6;

static const int PIN_KEY_C0 = 38;
static const int PIN_KEY_C1 = 39;
static const int PIN_KEY_C2 = 40;
static const int PIN_KEY_C3 = 41;
static const int PIN_KEY_C4 = 42;
static const int PIN_KEY_C5 = 47;
static const int PIN_KEY_C6 = 48;
static const int PIN_KEY_C7 = 21;

static const uint8_t KEY_ROWS = 6;
static const uint8_t KEY_COLS = 8;

