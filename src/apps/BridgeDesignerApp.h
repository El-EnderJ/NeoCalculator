/**
 * BridgeDesignerApp.h — Bridge Structural Simulator for NumOS.
 *
 * LVGL-native app with 2 modes:
 *   1. Edit   — Place nodes (wood/steel/cable) and connect beams on a grid
 *   2. Sim    — Run Verlet-integration physics with vehicles crossing
 *
 * Physics engine:
 *   - Verlet integration at fixed 60 Hz timestep
 *   - 8-iteration constraint solver for beam rigidity
 *   - Per-material elasticity and break thresholds
 *   - Vehicle load distribution across deck nodes
 *
 * Custom draw engine via LV_EVENT_DRAW_MAIN (no lv_canvas).
 * Node/beam arrays allocated in PSRAM (heap_caps_malloc in .cpp).
 *
 * Part of: NumOS — Engineering Module
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

// ── Physics structs ──────────────────────────────────────────────────────────

struct BridgeNode {
    float x, y;         // current position
    float ox, oy;       // old position (Verlet)
    bool  fixed;        // anchored — immune to forces
    bool  is_deck;      // part of the driveable surface
};

struct BridgeBeam {
    int      a, b;      // node indices
    float    restLen;   // natural length (pixels)
    uint8_t  material;  // 0 = WOOD, 1 = STEEL, 2 = CABLE
    float    stress;    // normalized stress 0..1
    bool     broken;    // beam has snapped
};

enum class VehicleType : uint8_t {
    TRUCK,
    CAR
};

struct Vehicle {
    float       pos;    // distance along the deck (pixels)
    float       mass;
    bool        active;
    VehicleType type;
};

// ── Application ──────────────────────────────────────────────────────────────

class BridgeDesignerApp {
public:
    BridgeDesignerApp();
    ~BridgeDesignerApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Modes & tools ────────────────────────────────────────────────────
    enum class AppMode  : uint8_t { EDIT_MODE, SIM_MODE };
    enum class ToolType : uint8_t { TOOL_WOOD, TOOL_STEEL, TOOL_CABLE, TOOL_DELETE };

    // ── Screen geometry ──────────────────────────────────────────────────
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;

    // ── Simulation constants ─────────────────────────────────────────────
    static constexpr int   MAX_NODES   = 64;
    static constexpr int   MAX_BEAMS   = 128;
    static constexpr float GRAVITY     = 9.81f;
    static constexpr float DT          = 1.0f / 60.0f;
    static constexpr int   ITER_COUNT  = 8;
    static constexpr int   GRID_SNAP   = 10;

    // ── Material indices (match BridgeBeam::material) ──────────────────
    static constexpr uint8_t MAT_WOOD  = 0;
    static constexpr uint8_t MAT_STEEL = 1;
    static constexpr uint8_t MAT_CABLE = 2;
    static constexpr int     MAT_COUNT = 3;

    // ── Material properties indexed by MAT_WOOD / MAT_STEEL / MAT_CABLE ─
    static constexpr float MAT_ELASTICITY[MAT_COUNT]  = { 0.8f, 0.3f, 0.5f };
    static constexpr float MAT_BREAK_POINT[MAT_COUNT] = { 0.7f, 0.95f, 0.5f };

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;       // custom-drawn bridge area
    lv_obj_t*       _toolbarObj;    // bottom toolbar container
    lv_obj_t*       _infoLabel;     // context info text
    lv_timer_t*     _physicsTimer;  // 60 Hz physics tick
    ui::StatusBar   _statusBar;

    // ── Node / beam storage (PSRAM) ──────────────────────────────────────
    BridgeNode*     _nodes;
    int             _nodeCount;
    BridgeBeam*     _beams;
    int             _beamCount;

    // ── Vehicles ─────────────────────────────────────────────────────────
    static constexpr int MAX_VEHICLES = 2;
    Vehicle         _vehicles[MAX_VEHICLES]; // [0] = truck, [1] = car

    // ── Editor / sim state ───────────────────────────────────────────────
    AppMode         _appMode;
    ToolType        _currentTool;
    int             _cursorX, _cursorY;     // grid-snapped cursor
    int             _selectedNode;          // -1 = none
    int             _startBeamNode;         // -1 = none (beam creation)
    float           _viewOffX, _viewOffY;   // camera pan offset

    // ── Text buffer for info bar ─────────────────────────────────────────
    char            _infoBuf[64];

    // ── UI construction ──────────────────────────────────────────────────
    void createUI();
    void createToolbar();

    // ── Input handling per mode ──────────────────────────────────────────
    void handleKeyEdit(const KeyEvent& ev);
    void handleKeySim(const KeyEvent& ev);

    // ── Physics engine ───────────────────────────────────────────────────
    void physicsStep();
    void applyGravity();
    void satisfyConstraints();
    void updateVehicle(Vehicle& v);

    // ── Editing helpers ──────────────────────────────────────────────────
    void addNode(float x, float y, bool fixed, bool isDeck);
    void addBeam(int a, int b, uint8_t material);
    void deleteNodeOrBeam();
    int  findNodeAt(int x, int y) const;

    // ── Deck interpolation ───────────────────────────────────────────────
    void findClosestDeckNodes(float pos, int& a, int& b, float& t) const;

    // ── Simulation lifecycle ─────────────────────────────────────────────
    void resetSimulation();

    // ── Custom draw callback ─────────────────────────────────────────────
    static void onDraw(lv_event_t* e);

    // ── Physics timer callback ───────────────────────────────────────────
    static void onPhysicsTimer(lv_timer_t* timer);

    // ── Color helpers ────────────────────────────────────────────────────
    static lv_color_t lerpColor(float stress);
};
