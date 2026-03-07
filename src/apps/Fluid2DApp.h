/**
 * Fluid2DApp.h — Real-Time 2D Fluid Simulator for NumOS.
 *
 * LVGL-native app implementing Jos Stam's "Stable Fluids" algorithm
 * with professional-grade physical enhancements:
 *   - Semi-Lagrangian advection (unconditionally stable)
 *   - Gauss-Seidel diffusion solver
 *   - Pressure projection (Helmholtz-Hodge decomposition)
 *   - Vorticity confinement (small-scale eddy amplification)
 *   - Thermal buoyancy (temperature-driven convection)
 *   - CFL-based dynamic timestep for stability
 *   - Lagrangian particle overlay
 *   - Internal wall obstacles
 *   - Blinn-Phong liquid shading with density-gradient normals
 *
 * Grid: 64×48 cells mapped to 320×240 display (5×5 px per cell).
 * Physics tick at ~30 Hz via lv_timer_t.
 * Custom draw via LV_EVENT_DRAW_MAIN (no lv_canvas).
 *
 * Controls:
 *   - Arrow keys: move cursor (WALL paints, ERASER clears while moving)
 *   - ENTER: toggle emission (ink) or toggle wall (WALL mode)
 *   - F1: cycle brush (RedInk → BlueInk → Wall → Eraser)
 *   - F2: cycle palette (Classic → Thermal → Velocity → Mixed)
 *   - F3: toggle telemetry HUD
 *   - F4: save scene to autosave.f2d
 *   - F5: cycle flow presets (None / Wind Tunnel / Convection Cell)
 *   - EXE (<): high-pressure radial injection at cursor
 *   - 0-9: adjust viscosity (0=Gas … 9=Honey)
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
    void autoSave();

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
    static constexpr float BASE_DT       = 1.0f / 30.0f;
    static constexpr int   GS_ITERS      = 4;   // Gauss-Seidel iterations
    static constexpr float DEFAULT_DIFF  = 0.0001f;
    static constexpr float DEFAULT_VISC  = 0.0001f;
    static constexpr float VORT_EPSILON  = 0.5f;  // vorticity confinement
    static constexpr float BUOY_A        = 0.1f;  // thermal buoyancy coeff
    static constexpr float BUOY_B        = 0.05f; // density drag coeff
    static constexpr float CFL_MAX       = 5.0f;  // max CFL factor

    // ── Sub-stepping ─────────────────────────────────────────────────────
    static constexpr float SUBSTEP_THRESHOLD = 10.0f;

    // ── Particle system ──────────────────────────────────────────────────
    static constexpr int MAX_PARTICLES   = 256;
    static constexpr int PARTICLE_TAIL   = 3;  // tail length in frames

    // ── Visualization modes ──────────────────────────────────────────────
    enum class BrushMode : uint8_t {
        RED_INK,
        BLUE_INK,
        WALL,
        ERASER
    };

    // ── Color palettes ───────────────────────────────────────────────────
    enum class Palette : uint8_t {
        CLASSIC,
        THERMAL,
        VELOCITY,
        MIXED
    };

    // ── Emitter shapes ───────────────────────────────────────────────────
    enum class EmitterShape : uint8_t {
        POINT,
        CROSS,
        RING
    };

    // ── Flow presets ─────────────────────────────────────────────────────
    enum class FlowPreset : uint8_t {
        NONE,
        WIND_TUNNEL,
        CONVECTION_CELL
    };

    // ── Particle ─────────────────────────────────────────────────────────
    struct Particle {
        float x, y;        // position in grid coords
        float prevX[PARTICLE_TAIL];
        float prevY[PARTICLE_TAIL];
        float life;         // remaining life (0 = dead)
        bool  active;
    };

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;       // custom-drawn fluid area
    lv_obj_t*       _infoLabel;     // bottom info text
    lv_timer_t*     _simTimer;      // ~30 Hz simulation tick
    ui::StatusBar   _statusBar;

    // ── Fluid grids (allocated dynamically) ──────────────────────────────
    float*  _dens;          // current density
    float*  _densPrev;      // previous density (source term)
    float*  _velX;          // velocity X
    float*  _velXPrev;
    float*  _velY;          // velocity Y
    float*  _velYPrev;
    float*  _temp;          // temperature field
    float*  _tempPrev;      // temperature source
    uint8_t* _obstacle;     // obstacle mask (0=fluid, 1=wall)
    float*  _densB;         // second density field (blue ink)
    float*  _densBPrev;     // second density source
    float*  _smoothObst;    // smoothed obstacle boundary

    // ── Simulation state ─────────────────────────────────────────────────
    float   _diffusion;     // diffusion rate
    float   _viscosity;     // viscosity
    float   _dt;            // dynamic timestep (CFL-based)
    bool    _emitting;      // continuous emission active

    // ── Emitter / cursor ─────────────────────────────────────────────────
    int     _cursorX, _cursorY;     // cursor position in grid coords
    EmitterShape _emitterShape;
    BrushMode _brushMode;

    // ── Visualization ────────────────────────────────────────────────────
    Palette  _palette;

    // ── Telemetry ────────────────────────────────────────────────────────
    bool    _showTelemetry;
    int     _idleFrames;
    static constexpr int ENERGY_SAMPLES = 30;
    float   _energyHistory[ENERGY_SAMPLES];
    int     _energyIdx;

    // ── Flow presets ─────────────────────────────────────────────────────
    FlowPreset _flowPreset;

    // ── Particle system ──────────────────────────────────────────────────
    Particle* _particles;
    int       _activeParticles;

    // ── Text buffer ──────────────────────────────────────────────────────
    char    _infoBuf[128];

    // ── Indexing helper ──────────────────────────────────────────────────
    static inline int IX(int x, int y) { return x + (N + 2) * y; }

    // ── UI construction ──────────────────────────────────────────────────
    void createUI();
    void updateInfoLabel();

    // ── Fluid solver (Jos Stam "Stable Fluids") ──────────────────────────
    void fluidStep();
    void addSource(float* field, const float* source, float dt);
    void diffuse(int b, float* x, const float* x0, float diff, float dt);
    void advect(int b, float* d, const float* d0,
                const float* u, const float* v, float dt);
    void project(float* u, float* v, float* p, float* div);
    void setBoundary(int b, float* x);

    // ── Physics enhancements ─────────────────────────────────────────────
    void vorticityConfinement();
    void applyBuoyancy(float dt);
    float computeCFLdt() const;
    void sanitizeField(float* field);

    // ── Obstacles ────────────────────────────────────────────────────────
    void enforceObstacles(float* u, float* v);
    void setupWindTunnel();
    void setupConvectionCell();

    // ── Emitter ──────────────────────────────────────────────────────────
    void addEmitterForces();
    void applyFlowPresetForces();

    // ── Particles ────────────────────────────────────────────────────────
    void initParticles();
    void stepParticles(float dt);
    void spawnParticle(int idx);

    // ── Reset ────────────────────────────────────────────────────────────
    void resetFields();

    // ── Rendering helpers ────────────────────────────────────────────────
    lv_color_t applyPalette(float d) const;
    static lv_color_t infernoColor(float t);
    static lv_color_t oceanColor(float t);
    static lv_color_t toxicColor(float t);
    static lv_color_t classicColor(float t);
    lv_color_t shadedDensityColor(int i, int j) const;
    static lv_color_t velocityToColor(float vx, float vy);
    static lv_color_t vorticityToColor(float w);
    static lv_color_t mixedColor(float densA, float densB);

    // ── HUD helpers ──────────────────────────────────────────────────────
    float computeAvgVelocity() const;
    float computeReynolds() const;
    float computeTotalEnergy() const;
    float computeLocalDivergence(int i, int j) const;
    void  smoothObstacles();

    // ── Persistence ──────────────────────────────────────────────────────
    void saveScene(const char* name);
    void loadScene(const char* name);
    void autoLoad();

    // ── Callbacks ────────────────────────────────────────────────────────
    static void onDraw(lv_event_t* e);
    static void onSimTimer(lv_timer_t* timer);
};
