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

#ifndef NUMOS_BOARD_PROD_WROOM1U_N16R8
#define NUMOS_BOARD_PROD_WROOM1U_N16R8 0
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8

#include "hardware/BoardProfile.h"

#ifndef ARDUINO_USB_MODE
#error "Production target requires an explicit Hardware CDC/JTAG USB mode"
#endif
#ifndef ARDUINO_USB_CDC_ON_BOOT
#error "Production target requires an explicit USB CDC-on-boot setting"
#endif
static_assert(ARDUINO_USB_MODE == 1,
              "Production target must use Hardware CDC/JTAG mode");
static_assert(ARDUINO_USB_CDC_ON_BOOT == 1,
              "Production target must enable USB CDC on boot");

inline constexpr int PIN_TFT_MISO =
    numos::hardware::kProductionBoard.display.miso.gpio;
inline constexpr int PIN_TFT_MOSI =
    numos::hardware::kProductionBoard.display.mosi.gpio;
inline constexpr int PIN_TFT_SCLK =
    numos::hardware::kProductionBoard.display.clock.gpio;
inline constexpr int PIN_TFT_CS =
    numos::hardware::kProductionBoard.display.chipSelect.gpio;
inline constexpr int PIN_TFT_DC =
    numos::hardware::kProductionBoard.display.dataCommand.gpio;
inline constexpr int PIN_TFT_RST =
    numos::hardware::kProductionBoard.display.reset.gpio;
inline constexpr int PIN_TFT_BL =
    numos::hardware::kProductionBoard.display.backlight.gpio;

inline constexpr uint16_t SCREEN_WIDTH =
    numos::hardware::kProductionBoard.display.logicalWidth;
inline constexpr uint16_t SCREEN_HEIGHT =
    numos::hardware::kProductionBoard.display.logicalHeight;
inline constexpr uint8_t SCREEN_ROTATION = 1;

// Electrical matrix coordinates only. No visual position or KeyCode identity
// exists for the production PCBA in this milestone.
inline constexpr auto& KBD_ROW_PINS =
    numos::hardware::kProductionBoard.electricalMatrix.rowOutputs;
inline constexpr auto& KBD_COL_PINS =
    numos::hardware::kProductionBoard.electricalMatrix.columnInputs;
inline constexpr uint8_t KBD_ROWS = 5;
inline constexpr uint8_t KBD_COLS = 10;
inline constexpr uint8_t KBD_CONNECTED_COLS = 0;

#define NUMOS_PRODUCTION_KEYPAD_MAPPING_READY 0

#if defined(TFT_MISO)
static_assert(TFT_MISO == PIN_TFT_MISO, "TFT MISO/profile mismatch");
#endif
#if defined(TFT_MOSI)
static_assert(TFT_MOSI == PIN_TFT_MOSI, "TFT MOSI/profile mismatch");
#endif
#if defined(TFT_SCLK)
static_assert(TFT_SCLK == PIN_TFT_SCLK, "TFT SCLK/profile mismatch");
#endif
#if defined(TFT_CS)
static_assert(TFT_CS == PIN_TFT_CS, "TFT CS/profile mismatch");
#endif
#if defined(TFT_DC)
static_assert(TFT_DC == PIN_TFT_DC, "TFT DC/profile mismatch");
#endif
#if defined(TFT_RST)
static_assert(TFT_RST == PIN_TFT_RST, "TFT reset/profile mismatch");
#endif
#if defined(TFT_BL)
static_assert(TFT_BL == PIN_TFT_BL, "TFT backlight/profile mismatch");
#endif
#if defined(SPI_FREQUENCY)
static_assert(SPI_FREQUENCY <=
                  numos::hardware::kProductionBoard.display.initialSpiHz,
              "Production SPI frequency exceeds the audited initial limit");
#endif
#if defined(NUMOS_BACKLIGHT_LOW_LEVEL) && \
    defined(NUMOS_BACKLIGHT_INITIAL_LEVEL) && \
    defined(NUMOS_BACKLIGHT_MAX_LEVEL)
static_assert(NUMOS_BACKLIGHT_LOW_LEVEL <= NUMOS_BACKLIGHT_INITIAL_LEVEL &&
              NUMOS_BACKLIGHT_INITIAL_LEVEL <= NUMOS_BACKLIGHT_MAX_LEVEL &&
              NUMOS_BACKLIGHT_MAX_LEVEL < 255,
              "Production backlight levels must be ordered and bounded");
#endif

#else

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

#define NUMOS_PRODUCTION_KEYPAD_MAPPING_READY 1

#endif

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
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
// No audited production 6x8 mapping exists. Invalid sentinels prevent the
// dormant legacy driver from acquiring production GPIOs by accident.
static const int PIN_KEY_R0 = -1;
static const int PIN_KEY_R1 = -1;
static const int PIN_KEY_R2 = -1;
static const int PIN_KEY_R3 = -1;
static const int PIN_KEY_R4 = -1;
static const int PIN_KEY_R5 = -1;

static const int PIN_KEY_C0 = -1;
static const int PIN_KEY_C1 = -1;
static const int PIN_KEY_C2 = -1;
static const int PIN_KEY_C3 = -1;
static const int PIN_KEY_C4 = -1;
static const int PIN_KEY_C5 = -1;
static const int PIN_KEY_C6 = -1;
static const int PIN_KEY_C7 = -1;
#else
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
#endif

static const uint8_t KEY_ROWS = 6;
static const uint8_t KEY_COLS = 8;

