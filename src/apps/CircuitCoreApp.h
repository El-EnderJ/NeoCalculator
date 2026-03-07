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
#include "ComponentFactory.h"
#include "LogicGates.h"
#include "PowerSystems.h"
#include "LuaVM.h"

class CircuitCoreApp {
public:
    CircuitCoreApp();
    ~CircuitCoreApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    /** Auto-save circuit to LittleFS (called from SystemApp before menu transition). */
    void autoSave();

    bool isActive() const { return _screen != nullptr; }

    /** True when LVGL focus is on the toolbar (AC should exit to menu). */
    bool isToolbarFocused() const { return _focusArea == FocusArea::TOOLBAR; }

private:
    // ── Tool types (toolbar order, across pages) ──────────────────────────
    // Page 0: Basic components (original)
    // Page 1: Sensors & Semiconductors  
    // Page 2: Logic Gates & Power
    enum class Tool : uint8_t {
        // Page 0 — Basic
        RES = 0,   // Resistor
        VCC,       // Voltage Source
        GND,       // Ground
        LED,       // LED
        MCU,       // Microcontroller
        WIRE,      // Wire
        POT,       // Potentiometer
        BTN,       // Push-Button
        CAP,       // Capacitor
        DIODE,     // Diode
        // Page 1 — Sensors & Semiconductors
        LDR_T,     // Light Dependent Resistor
        THERM,     // Thermistor/TMP36
        FLEX,      // Flex Sensor
        FSR_T,     // Force Sensitive Resistor
        NPN,       // NPN BJT
        PNP,       // PNP BJT
        NMOS_T,    // N-Channel MOSFET
        OPAMP,     // Op-Amp
        BUZZER_T,  // Buzzer
        SEG7,      // 7-Segment Display
        // Page 2 — Logic & Power
        AND_T,     // AND Gate
        OR_T,      // OR Gate
        NOT_T,     // NOT Gate
        NAND_T,    // NAND Gate
        XOR_T,     // XOR Gate
        BAT_AA,    // 1.5V Battery
        BAT_COIN,  // 3V Battery
        BAT_9V,    // 9V Battery
        MULTI,     // Multimeter probe
        // Control
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
    static constexpr int MAX_COMPONENTS = 96;

    // ── Toolbar pages (10 tools per page) ───────────────────────────────
    static constexpr int TOOLS_PER_PAGE = 10;
    static constexpr int NUM_PAGES      = 3;  // Basic / Sensors+Semi / Logic+Power

    // ── LVGL objects ────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;       // grid area with custom draw
    lv_obj_t*       _toolbarObj;    // toolbar container
    lv_obj_t*       _toolBtns[TOOLS_PER_PAGE]; // visible toolbar buttons (per page)
    lv_obj_t*       _infoLabel;     // bottom info bar
    lv_obj_t*       _pageLabel;     // toolbar page indicator
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
    int         _toolbarIdx;    // focused toolbar button index (within page)
    int         _toolbarPage;   // current toolbar page (0-2)
    FocusArea   _focusArea;
    int         _cursorX;       // grid-snapped cursor (pixels relative to grid)
    int         _cursorY;
    int         _prevCursorX;   // previous cursor position (for invalidation)
    int         _prevCursorY;
    bool        _simRunning;
    uint32_t    _frameCount;    // for 30Hz MNA decimation
    int         _nextNodeId;    // next MNA node to assign
    int         _nextVsIdx;     // next voltage source index

    // ── Sensor simulation slider ────────────────────────────────────────
    float       _sensorSliderValue;  // 0.0 - 1.0 for sensor adjustment

    // ── Text buffer for info bar ────────────────────────────────────────
    char        _infoBuf[80];

    // ── Oscilloscope (Dual-Trace Scope) ────────────────────────────────
    static constexpr int SCOPE_W       = 80;
    static constexpr int SCOPE_H       = 40;
    static constexpr int SCOPE_SAMPLES = 80;
    float       _scopeBuffer[SCOPE_SAMPLES];   // channel 1 ring buffer
    float       _scopeBuffer2[SCOPE_SAMPLES];  // channel 2 ring buffer
    int         _scopeWriteIdx;
    int         _probeNode;                    // channel 1 probe node
    int         _probeNode2;                   // channel 2 probe node
    bool        _scopeActive;
    int         _scopeTimebase;                // timebase divisor (1-8)

    // ── Multimeter ──────────────────────────────────────────────────────
    int         _meterNodeA;        // probe A node (-1 = inactive)
    int         _meterNodeB;        // probe B node (-1 = inactive)
    bool        _meterActive;       // multimeter display enabled

    // ── Simulation speed ────────────────────────────────────────────────
    enum class SimSpeed : uint8_t { SPEED_1X = 0, SPEED_05X, SPEED_025X, PAUSED };
    SimSpeed    _simSpeed;

    // ── Voltage heatmap toggle ──────────────────────────────────────────
    bool        _voltageHeatmap;

    // ── Undo/Redo system ────────────────────────────────────────────────
    static constexpr int MAX_UNDO = 8;
    struct UndoSnapshot {
        int compCount;
        struct CompData {
            int typeVal, gridX, gridY, nodeA, nodeB;
            float param1, param2;
            int extra1, extra2;
            char label[5];
        } comps[MAX_COMPONENTS];
    };
    UndoSnapshot _undoBuffer[MAX_UNDO];
    int          _undoHead;
    int          _undoCount;

    // ── Auto-save timer ─────────────────────────────────────────────────
    uint32_t    _lastEditTime;
    static constexpr uint32_t AUTOSAVE_INTERVAL_MS = 30000;

    // ── IDE (Programming Editor) ────────────────────────────────────────
    bool        _ideOpen;
    lv_obj_t*   _ideTextArea;
    lv_obj_t*   _ideContainer;
    lv_obj_t*   _ideAutoLabel;
    char        _ideBuffer[512];
    int         _ideBufferLen;

    // ── Tooltip descriptions (now provided by ComponentFactory) ────────

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
    bool runMnaTick();   // returns true if circuit state changed
    void startSimulation();
    void stopSimulation();

    // ── Oscilloscope ────────────────────────────────────────────────────
    void updateScope();
    void drawScope(lv_layer_t* layer, int objX, int objY);

    // ── Visual enhancements ─────────────────────────────────────────────
    void drawNodeLabels(lv_layer_t* layer, int objX, int objY);

    // ── Persistence (LittleFS) ──────────────────────────────────────────
    void saveCircuit(const char* filename);
    void loadCircuit(const char* filename);
    void autoLoad();

    // ── Component interaction ───────────────────────────────────────────
    void toggleButtonAt(int gx, int gy);
    void adjustPotAt(int gx, int gy, bool up);
    void adjustSensorAt(int gx, int gy, float value);

    // ── Multimeter ──────────────────────────────────────────────────────
    void drawMultimeter(lv_layer_t* layer, int objX, int objY);
    void probeMultimeter(int gx, int gy);

    // ── Undo/Redo ───────────────────────────────────────────────────────
    void saveUndoSnapshot();
    void performUndo();

    // ── Voltage heatmap drawing ─────────────────────────────────────────
    void drawVoltageHeatmap(lv_layer_t* layer, int objX, int objY);

    // ── Current arrows ──────────────────────────────────────────────────
    void drawCurrentArrows(lv_layer_t* layer, int objX, int objY);

    // ── Circuit sharing ─────────────────────────────────────────────────
    void generateShareString(char* outBuf, int maxLen);

    // ── Property editor ─────────────────────────────────────────────────
    void openPropertyEditor(CircuitComponent* comp);

    // ── IDE ──────────────────────────────────────────────────────────────
    void openIDE();
    void closeIDE();
    void updateAutoComplete();
    void handleKeyIDE(const KeyEvent& ev);

    // ── Toolbar pages ───────────────────────────────────────────────────
    void rebuildToolbarPage();
    Tool pageToolAt(int page, int idx) const;
    int  toolsOnPage(int page) const;

    // ── Custom draw callback ────────────────────────────────────────────
    static void onDraw(lv_event_t* e);

    // ── Simulation timer callback ───────────────────────────────────────
    static void onSimTimer(lv_timer_t* timer);
};
