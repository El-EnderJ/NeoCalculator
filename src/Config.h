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

// ── Matriz de teclado 6×8 (ESP32-S3) ───────────────────────────────────────
// Filas: entradas con INPUT_PULLUP interno.
// Columnas: salidas (activo LOW durante el escaneo).
//
// ⚠ El ESP32-S3 NO tiene GPIO 34/35/36/39 sin pull-up como el ESP32 clásico.
//   Estos pines son los GPIO del ESP32-S3 DevKit C-1 de 38 pines lado A/B.
//   Ajusta si tu placa tiene un layout diferente.
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

// ── Dimensiones de la matriz ────────────────────────────────────────────────
static const uint8_t KEY_ROWS = 6;
static const uint8_t KEY_COLS = 8;

// ── Temporización del escaneo ───────────────────────────────────────────────
static const uint16_t KEY_SCAN_INTERVAL_MS    =  5;   // Período de scan
static const uint16_t KEY_DEBOUNCE_MS         = 20;   // Debounce
static const uint16_t KEY_AUTOREPEAT_DELAY_MS = 500;  // Delay antes del autorepeat
static const uint16_t KEY_AUTOREPEAT_RATE_MS  =  80;  // Velocidad de autorepeat

