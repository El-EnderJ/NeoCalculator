/**
 * Config.h
 * Pinout and global configuration for the ESP32 graphing/scientific calculator.
 */

#pragma once

#include <Arduino.h>

// TFT display (ILI9341) over VSPI
// These must match your TFT_eSPI User_Setup configuration.
static const int PIN_TFT_MOSI = 23;   // VSPI MOSI
static const int PIN_TFT_SCLK = 18;   // VSPI SCK
static const int PIN_TFT_CS   = 5;    // Chip select
static const int PIN_TFT_DC   = 16;   // Data/Command
static const int PIN_TFT_RST  = 17;   // Reset
static const int PIN_TFT_BL   = 4;    // Backlight (via transistor, PWM capable)

// Key matrix: 6 rows (inputs with internal pull-ups), 8 columns (outputs)
// Se han evitado los GPIO 34/35/36/39 (solo entrada y sin pull-up interno)
// para no requerir resistencias externas. ATENCIÓN: algunos pines usados
// como columnas (0, 2, 12, 15) son pines de arranque; evita mantener
// pulsadas sus teclas durante el encendido para no interferir con el boot.
static const int PIN_KEY_R0 = 32;
static const int PIN_KEY_R1 = 33;
static const int PIN_KEY_R2 = 25;
static const int PIN_KEY_R3 = 26;
static const int PIN_KEY_R4 = 27;
static const int PIN_KEY_R5 = 14;

static const int PIN_KEY_C0 = 13;
static const int PIN_KEY_C1 = 19;
static const int PIN_KEY_C2 = 21;
static const int PIN_KEY_C3 = 22;
static const int PIN_KEY_C4 = 0;
static const int PIN_KEY_C5 = 2;
static const int PIN_KEY_C6 = 12;
static const int PIN_KEY_C7 = 15;

// Screen configuration
static const uint16_t SCREEN_WIDTH  = 240;  // pixels (TFT native: 240x320 rotated)
static const uint16_t SCREEN_HEIGHT = 320;
static const uint8_t  SCREEN_ROTATION = 1;  // 0-3, 1 = vertical typical for calculator

// Key matrix dimensions
static const uint8_t KEY_ROWS = 6;
static const uint8_t KEY_COLS = 8;

// Scan timing
static const uint16_t KEY_SCAN_INTERVAL_MS = 5;    // scan period
static const uint16_t KEY_DEBOUNCE_MS      = 20;   // debounce time
static const uint16_t KEY_AUTOREPEAT_DELAY_MS = 500;
static const uint16_t KEY_AUTOREPEAT_RATE_MS  = 80;

