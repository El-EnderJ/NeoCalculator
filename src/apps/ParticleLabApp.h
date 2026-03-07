/**
 * ParticleLabApp.h
 * Falling-sand / Powder Toy style particle simulation for NumOS.
 * App ID 15 — LVGL-native, renders via 2x-upscaled image buffer.
 *
 * "The Alchemy Update" — 30+ materials, electronics, phase transitions,
 * professional UI overlay, Bresenham line tool, save/load.
 */

#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "hal/ArduinoCompat.h"
#endif

#include <lvgl.h>
#include "ui/StatusBar.h"
#include "input/KeyCodes.h"
#include "ParticleEngine.h"

class ParticleLabApp {
public:
    ParticleLabApp();
    ~ParticleLabApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Screen dimensions ──
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;
    static constexpr int STATUS_H = 24;
    static constexpr int INFO_H   = 14;
    static constexpr int TOOLBAR_H = 20;
    static constexpr int DRAW_Y   = STATUS_H;
    static constexpr int DRAW_H   = SCREEN_H - STATUS_H;

    // ── Colors ──
    static constexpr uint32_t COL_BG       = 0x0A0A0E;
    static constexpr uint32_t COL_TEXT_DIM  = 0x888888;
    static constexpr uint32_t COL_TOOLBAR   = 0x1A1A22;
    static constexpr uint32_t COL_CURSOR    = 0xFFD700;
    static constexpr uint32_t COL_SELECT    = 0x40FF40;

    // ── LVGL objects ──
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;
    lv_obj_t*       _infoLabel;
    lv_timer_t*     _simTimer;
    ui::StatusBar   _statusBar;

    // ── Engine ──
    ParticleEngine  _engine;

    // ── Render buffer (320x240 RGB565 in PSRAM) ──
    uint16_t*       _renderBuf;
    lv_image_dsc_t  _imgDsc;

    // ── Cursor ──
    int _cursorX;
    int _cursorY;

    // ── Brush ──
    int         _brushRadius;   // 0=1px, 1=3px(r=1), 2=5px(r=2)
    BrushShape  _brushShape;    // CIRCLE, SQUARE, SPRAY

    // ── Selected material ──
    int  _selectedMat;   // index into material palette

    // ── Drawing state ──
    bool _drawing;       // ENTER held = drawing
    int  _drawStartX;    // Bresenham line start X
    int  _drawStartY;    // Bresenham line start Y
    bool _thermoMode;    // EXE held = thermometer

    // ── Palette overlay ──
    bool _paletteOpen;   // F3 palette overlay active
    int  _paletteCurX;   // cursor in palette grid
    int  _paletteCurY;
    bool _paused;        // simulation paused

    // ── Info label buffer ──
    char _infoBuf[120];

    // ── Toolbar material palette (selectable materials) ──
    static constexpr int MAT_PALETTE_COUNT = 28;
    static const uint8_t MAT_PALETTE[MAT_PALETTE_COUNT];

    // ── Palette grid layout ──
    static constexpr int PAL_COLS = 7;
    static constexpr int PAL_ROWS = 4;

    // ── UI setup ──
    void createUI();
    void updateInfoLabel();

    // ── Rendering ──
    void renderToBuffer();
    uint16_t getTempGlowColor(uint16_t baseColor, int16_t temp);
    void renderPaletteOverlay();

    // ── Save/Load ──
    void saveSandbox();
    void loadSandbox();

    // ── LVGL callbacks ──
    static void onDraw(lv_event_t* e);
    static void onSimTimer(lv_timer_t* timer);
};
