/**
 * Fluid2DApp.h — Real-Time 2D Fluid Simulator for NumOS.
 *
 * LVGL-native app implementing Jos Stam's "Stable Fluids" algorithm:
 *   - Semi-Lagrangian advection (unconditionally stable)
 *   - Gauss-Seidel diffusion solver
 *   - Pressure projection (Helmholtz-Hodge decomposition)
 *
 * Grid: 64×48 cells mapped to 320×240 display (5×5 px per cell).
 * Physics tick at ~30 Hz via lv_timer_t.
 * Custom draw via LV_EVENT_DRAW_MAIN (no lv_canvas).
 *
 * Controls:
 *   - Arrow keys: move emitter cursor
 *   - ENTER: toggle continuous emission
 *   - F1: cycle visualization (density / velocity / vorticity)
 *   - F2: toggle velocity arrows overlay
 *   - F3: reset simulation
 *   - F4: cycle emitter shape (point / cross / ring)
 *   - EXE: pause / resume simulation
 *   - 0-9: adjust viscosity / diffusion
 *
 * Part of: NumOS — Physics Simulation Module
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

// ── Application ──────────────────────────────────────────────────────────────

class Fluid2DApp {
public:
    Fluid2DApp();
    ~Fluid2DApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Grid dimensions ──────────────────────────────────────────────────
    static constexpr int N       = 64;      // grid width (cells)
    static constexpr int M       = 48;      // grid height (cells)
    static constexpr int SIZE    = (N + 2) * (M + 2);  // with boundary cells
    static constexpr int CELL_W  = 5;       // pixels per cell X (320/64)
    static constexpr int CELL_H  = 5;       // pixels per cell Y (240/48)

    // ── Screen geometry ──────────────────────────────────────────────────
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;

    // ── Simulation constants ─────────────────────────────────────────────
    static constexpr float DT           = 1.0f / 30.0f;
    static constexpr int   GS_ITERS     = 4;   // Gauss-Seidel iterations
    static constexpr float DEFAULT_DIFF = 0.0001f;
    static constexpr float DEFAULT_VISC = 0.0001f;

    // ── Visualization modes ──────────────────────────────────────────────
    enum class VizMode : uint8_t {
        DENSITY,
        VELOCITY,
        VORTICITY
    };

    // ── Emitter shapes ───────────────────────────────────────────────────
    enum class EmitterShape : uint8_t {
        POINT,
        CROSS,
        RING
    };

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;       // custom-drawn fluid area
    lv_obj_t*       _infoLabel;     // bottom info text
    lv_timer_t*     _simTimer;      // ~30 Hz simulation tick
    ui::StatusBar   _statusBar;

    // ── Fluid grids (allocated dynamically) ──────────────────────────────
    // Density field
    float*  _dens;          // current density
    float*  _densPrev;      // previous density (source term)

    // Velocity field (X component)
    float*  _velX;
    float*  _velXPrev;

    // Velocity field (Y component)
    float*  _velY;
    float*  _velYPrev;

    // ── Simulation state ─────────────────────────────────────────────────
    float   _diffusion;     // diffusion rate
    float   _viscosity;     // viscosity
    bool    _paused;        // simulation paused
    bool    _emitting;      // continuous emission active

    // ── Emitter / cursor ─────────────────────────────────────────────────
    int     _cursorX, _cursorY;     // cursor position in grid coords
    EmitterShape _emitterShape;

    // ── Visualization ────────────────────────────────────────────────────
    VizMode _vizMode;
    bool    _showArrows;    // velocity arrow overlay

    // ── Text buffer ──────────────────────────────────────────────────────
    char    _infoBuf[80];

    // ── Indexing helper ──────────────────────────────────────────────────
    static inline int IX(int x, int y) { return x + (N + 2) * y; }

    // ── UI construction ──────────────────────────────────────────────────
    void createUI();
    void updateInfoLabel();

    // ── Fluid solver (Jos Stam "Stable Fluids") ──────────────────────────
    void fluidStep();
    void addSource(float* field, const float* source);
    void diffuse(int b, float* x, const float* x0, float diff);
    void advect(int b, float* d, const float* d0,
                const float* u, const float* v);
    void project(float* u, float* v, float* p, float* div);
    void setBoundary(int b, float* x);

    // ── Emitter ──────────────────────────────────────────────────────────
    void addEmitterForces();

    // ── Reset ────────────────────────────────────────────────────────────
    void resetFields();

    // ── Rendering helpers ────────────────────────────────────────────────
    static lv_color_t densityToColor(float d);
    static lv_color_t velocityToColor(float vx, float vy);
    static lv_color_t vorticityToColor(float w);

    // ── Callbacks ────────────────────────────────────────────────────────
    static void onDraw(lv_event_t* e);
    static void onSimTimer(lv_timer_t* timer);
};
