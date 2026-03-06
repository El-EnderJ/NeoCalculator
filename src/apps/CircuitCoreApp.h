/**
 * CircuitCoreApp.h — Real-time SPICE-like Circuit Simulator for NumOS.
 *
 * LVGL-native app with:
 *   - Modified Nodal Analysis (MNA) engine running at 30Hz
 *   - Component placement: Resistor, VCC, GND, LED, MCU, Wire
 *   - Lua virtual microcontroller (coroutine-based)
 *   - Custom draw via LV_EVENT_DRAW_MAIN (no lv_canvas)
 *
 * Layout:
 *   [StatusBar 24px] → [Toolbar 20px] → [Grid Area] → [InfoBar 18px]
 *
 * Controls:
 *   - Arrow keys: move crosshair cursor (snapped to 20px grid)
 *   - ENTER: place selected component at cursor
 *   - BACK/AC on grid: focus toolbar
 *   - BACK/AC on toolbar: return to menu
 *   - F1-F4/1-7: select toolbar tool
 *
 * Part of: NumOS — Circuit Core Module (App ID 13)
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "MnaMatrix.h"
#include "CircuitComponent.h"
#include "LuaVM.h"

class CircuitCoreApp {
public:
    CircuitCoreApp();
    ~CircuitCoreApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

    /** True when LVGL focus is on the toolbar (AC should exit to menu). */
    bool isToolbarFocused() const { return _focusArea == FocusArea::TOOLBAR; }

private:
    // ── Tool types (toolbar order) ──────────────────────────────────────
    enum class Tool : uint8_t {
        RES = 0,   // Resistor
        VCC,       // Voltage Source
        GND,       // Ground
        LED,       // LED
        MCU,       // Microcontroller
        WIRE,      // Wire
        RUN,       // Run/Stop simulation
        TOOL_COUNT
    };

    // ── Focus state ─────────────────────────────────────────────────────
    enum class FocusArea : uint8_t {
        GRID,      // Cursor in grid area
        TOOLBAR    // Focus on toolbar buttons
    };

    // ── Screen geometry ─────────────────────────────────────────────────
    static constexpr int SCREEN_W    = 320;
    static constexpr int SCREEN_H    = 240;
    static constexpr int STATUSBAR_H = 24;
    static constexpr int TOOLBAR_H   = 20;
    static constexpr int INFOBAR_H   = 18;
    static constexpr int TOOLBAR_Y   = STATUSBAR_H;
    static constexpr int GRID_Y      = STATUSBAR_H + TOOLBAR_H;
    static constexpr int GRID_H      = SCREEN_H - STATUSBAR_H - TOOLBAR_H - INFOBAR_H;
    static constexpr int GRID_W      = SCREEN_W;
    static constexpr int GRID_SNAP   = 20;

    // ── Component storage ───────────────────────────────────────────────
    static constexpr int MAX_COMPONENTS = 64;

    // ── LVGL objects ────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;       // grid area with custom draw
    lv_obj_t*       _toolbarObj;    // toolbar container
    lv_obj_t*       _toolBtns[static_cast<int>(Tool::TOOL_COUNT)];
    lv_obj_t*       _infoLabel;     // bottom info bar
    lv_timer_t*     _simTimer;      // 60Hz frame timer (MNA at 30Hz)
    ui::StatusBar   _statusBar;

    // ── MNA Engine ──────────────────────────────────────────────────────
    MnaMatrix       _mna;

    // ── Lua VM ──────────────────────────────────────────────────────────
    LuaVM           _luaVM;

    // ── Component storage (PSRAM) ───────────────────────────────────────
    CircuitComponent** _components;
    int                _compCount;

    // ── Editor state ────────────────────────────────────────────────────
    Tool        _currentTool;
    int         _toolbarIdx;    // focused toolbar button index
    FocusArea   _focusArea;
    int         _cursorX;       // grid-snapped cursor (pixels relative to grid)
    int         _cursorY;
    bool        _simRunning;
    uint32_t    _frameCount;    // for 30Hz MNA decimation
    int         _nextNodeId;    // next MNA node to assign
    int         _nextVsIdx;     // next voltage source index

    // ── Text buffer for info bar ────────────────────────────────────────
    char        _infoBuf[80];

    // ── UI construction ─────────────────────────────────────────────────
    void createUI();
    void createToolbar();
    void createInfoBar();
    void updateInfoBar();
    void updateToolbarHighlight();

    // ── Input handling ──────────────────────────────────────────────────
    void handleKeyGrid(const KeyEvent& ev);
    void handleKeyToolbar(const KeyEvent& ev);

    // ── Component management ────────────────────────────────────────────
    void placeComponent();
    void deleteComponentAt(int gx, int gy);
    CircuitComponent* findComponentAt(int gx, int gy) const;

    // ── Simulation ──────────────────────────────────────────────────────
    void runMnaTick();
    void startSimulation();
    void stopSimulation();

    // ── Custom draw callback ────────────────────────────────────────────
    static void onDraw(lv_event_t* e);

    // ── Simulation timer callback ───────────────────────────────────────
    static void onSimTimer(lv_timer_t* timer);
};
