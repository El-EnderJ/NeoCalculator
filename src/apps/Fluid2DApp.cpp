/**
 * Fluid2DApp.cpp — Real-Time 2D Fluid Simulator for NumOS.
 *
 * Implements Jos Stam's "Stable Fluids" (SIGGRAPH 1999) with:
 *   1. Add forces (emitter input + flow presets)
 *   2. Vorticity confinement (eddy amplification)
 *   3. Thermal buoyancy (temperature-driven convection)
 *   4. Diffuse  (implicit Gauss-Seidel)
 *   5. Project  (pressure solve → divergence-free)
 *   6. Advect   (semi-Lagrangian back-trace)
 *   7. Project  (post-advection correction)
 *   8. Obstacle enforcement (internal solid walls)
 *   9. CFL dynamic timestep & FPU sanitisation
 *
 * Rendering: density mapped to LUT palettes with Blinn-Phong shading,
 * Lagrangian particle overlay, and velocity arrow overlay.
 */

#include "Fluid2DApp.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef ARDUINO
  #include <esp_heap_caps.h>
  #include <LittleFS.h>
#endif

// ══ Color palette ════════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0x0A0A1A;   // deep navy
static constexpr uint32_t COL_TEXT       = 0xE0E0E0;
static constexpr uint32_t COL_TEXT_DIM   = 0x808080;
static constexpr uint32_t COL_CURSOR     = 0xFFD700;   // gold cursor
static constexpr uint32_t COL_WALL       = 0x505060;   // wall obstacle color
static constexpr uint32_t COL_PARTICLE   = 0xFFFFFF;   // particle color
static constexpr uint32_t COL_RING       = 0xFF4040;   // EXE pressure ring

// ══ Layout constants ═════════════════════════════════════════════════════════
static constexpr int DRAW_Y  = 24;   // below StatusBar
static constexpr int DRAW_H  = 216;  // 240 - 24
static constexpr int INFO_H  = 12;

// ══ Viscosity presets (0=Gas … 9=Honey) ══════════════════════════════════════
static constexpr float VISC_PRESETS[] = {
    0.000001f, 0.00005f, 0.0001f, 0.0005f, 0.001f,
    0.005f,    0.01f,    0.05f,   0.1f,    0.5f
};
static constexpr const char* VISC_NAMES[] = {
    "Gas", "Air", "Smoke", "Water", "Oil",
    "Syrup", "Mud", "Tar", "Lava", "Honey"
};

static constexpr int VISC_PRESET_COUNT = 10;
static constexpr int DEFAULT_VISC_IDX  = 2;  // "Smoke" preset

// ══ Particle constants ═══════════════════════════════════════════════════════
static constexpr float PARTICLE_BASE_LIFE  = 5.0f;
static constexpr int   PARTICLE_LIFE_VAR   = 5;
static constexpr int   PARTICLE_SCATTER     = 7;  // prime for spatial distribution
static constexpr uint8_t TAIL_BASE_ALPHA   = 180;
static constexpr uint8_t TAIL_ALPHA_DECAY  = 50;
static constexpr float PARTICLE_OPA_SCALE  = 60.0f;
static constexpr float SPECULAR_INTENSITY  = 120.0f;
static constexpr int   PROBE_IDLE_FRAMES   = 60;    // 2 seconds at 30 Hz
static constexpr uint32_t SCENE_FILE_MAGIC = 0x46324430; // "F2D0"

// ══ Constructor / Destructor ═════════════════════════════════════════════════

Fluid2DApp::Fluid2DApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _infoLabel(nullptr)
    , _simTimer(nullptr)
    , _dens(nullptr), _densPrev(nullptr)
    , _velX(nullptr), _velXPrev(nullptr)
    , _velY(nullptr), _velYPrev(nullptr)
    , _temp(nullptr), _tempPrev(nullptr)
    , _obstacle(nullptr)
    , _densB(nullptr), _densBPrev(nullptr)
    , _smoothObst(nullptr)
    , _diffusion(DEFAULT_DIFF)
    , _viscosity(DEFAULT_VISC)
    , _dt(BASE_DT)
    , _emitting(false)
    , _cursorX(N / 2), _cursorY(M / 2)
    , _emitterShape(EmitterShape::POINT)
    , _brushMode(BrushMode::RED_INK)
    , _palette(Palette::CLASSIC)
    , _showTelemetry(false)
    , _idleFrames(0)
    , _energyIdx(0)
    , _flowPreset(FlowPreset::NONE)
    , _particles(nullptr)
    , _activeParticles(0)
{
    _infoBuf[0] = '\0';
    memset(_energyHistory, 0, sizeof(_energyHistory));
}

Fluid2DApp::~Fluid2DApp() {
    end();
}

// ══ Lifecycle ════════════════════════════════════════════════════════════════

void Fluid2DApp::begin() {
    if (_screen) return;

    const size_t bytes = SIZE * sizeof(float);

#ifdef ARDUINO
    _dens     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _densPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velX     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velXPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velY     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velYPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _temp     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _tempPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _obstacle = (uint8_t*)heap_caps_malloc(SIZE, MALLOC_CAP_SPIRAM);
    _densB     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _densBPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _smoothObst = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _particles = (Particle*)heap_caps_malloc(MAX_PARTICLES * sizeof(Particle),
                                              MALLOC_CAP_SPIRAM);
#else
    _dens     = new float[SIZE];
    _densPrev = new float[SIZE];
    _velX     = new float[SIZE];
    _velXPrev = new float[SIZE];
    _velY     = new float[SIZE];
    _velYPrev = new float[SIZE];
    _temp     = new float[SIZE];
    _tempPrev = new float[SIZE];
    _obstacle = new uint8_t[SIZE];
    _densB     = new float[SIZE];
    _densBPrev = new float[SIZE];
    _smoothObst = new float[SIZE];
    _particles = new Particle[MAX_PARTICLES];
#endif

    if (!_dens || !_densPrev || !_velX || !_velXPrev ||
        !_velY || !_velYPrev || !_temp || !_tempPrev ||
        !_obstacle || !_densB || !_densBPrev || !_smoothObst ||
        !_particles) {
        return;  // allocation failure
    }

    resetFields();
    initParticles();

    // ── LVGL screen ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Fluid 2D");

    createUI();

    _emitting = false;
    _cursorX  = N / 2;
    _cursorY  = M / 2;

    autoLoad();
}

void Fluid2DApp::end() {
    if (_screen) {
        if (_simTimer) {
            lv_timer_delete(_simTimer);
            _simTimer = nullptr;
        }
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen    = nullptr;
        _statusBar.resetPointers();
        _drawObj   = nullptr;
        _infoLabel = nullptr;

#ifdef ARDUINO
        if (_dens)      { heap_caps_free(_dens);      _dens      = nullptr; }
        if (_densPrev)  { heap_caps_free(_densPrev);  _densPrev  = nullptr; }
        if (_velX)      { heap_caps_free(_velX);      _velX      = nullptr; }
        if (_velXPrev)  { heap_caps_free(_velXPrev);  _velXPrev  = nullptr; }
        if (_velY)      { heap_caps_free(_velY);      _velY      = nullptr; }
        if (_velYPrev)  { heap_caps_free(_velYPrev);  _velYPrev  = nullptr; }
        if (_temp)      { heap_caps_free(_temp);      _temp      = nullptr; }
        if (_tempPrev)  { heap_caps_free(_tempPrev);  _tempPrev  = nullptr; }
        if (_obstacle)  { heap_caps_free(_obstacle);  _obstacle  = nullptr; }
        if (_densB)     { heap_caps_free(_densB);     _densB     = nullptr; }
        if (_densBPrev) { heap_caps_free(_densBPrev); _densBPrev = nullptr; }
        if (_smoothObst){ heap_caps_free(_smoothObst);_smoothObst= nullptr; }
        if (_particles) { heap_caps_free(_particles); _particles = nullptr; }
#else
        delete[] _dens;      _dens      = nullptr;
        delete[] _densPrev;  _densPrev  = nullptr;
        delete[] _velX;      _velX      = nullptr;
        delete[] _velXPrev;  _velXPrev  = nullptr;
        delete[] _velY;      _velY      = nullptr;
        delete[] _velYPrev;  _velYPrev  = nullptr;
        delete[] _temp;      _temp      = nullptr;
        delete[] _tempPrev;  _tempPrev  = nullptr;
        delete[] _obstacle;  _obstacle  = nullptr;
        delete[] _densB;     _densB     = nullptr;
        delete[] _densBPrev; _densBPrev = nullptr;
        delete[] _smoothObst;_smoothObst= nullptr;
        delete[] _particles; _particles = nullptr;
#endif
    }
}

void Fluid2DApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Fluid 2D");
    _statusBar.update();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ══ UI Construction ══════════════════════════════════════════════════════════

void Fluid2DApp::createUI() {
    _drawObj = lv_obj_create(_screen);
    lv_obj_set_size(_drawObj, SCREEN_W, DRAW_H);
    lv_obj_set_pos(_drawObj, 0, DRAW_Y);
    lv_obj_set_style_bg_opa(_drawObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_drawObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_drawObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_drawObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(_drawObj, this);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN, this);

    _infoLabel = lv_label_create(_screen);
    lv_obj_set_pos(_infoLabel, 4, SCREEN_H - INFO_H - 2);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    updateInfoLabel();

    _simTimer = lv_timer_create(onSimTimer, 33, this);
}

void Fluid2DApp::updateInfoLabel() {
    static constexpr const char* palNames[] = { "Classic", "Thermal", "Velocity", "Mixed" };
    static constexpr const char* brushNames[] = { "RedInk", "BlueInk", "Wall", "Eraser" };
    float avgV = computeAvgVelocity();
    float re   = computeReynolds();
    snprintf(_infoBuf, sizeof(_infoBuf),
             "Re:%.0f V:%.1f P:%d %s|%s%s",
             re, avgV, _activeParticles,
             palNames[(int)_palette],
             brushNames[(int)_brushMode],
             _emitting ? "|EMIT" : "");
    lv_label_set_text(_infoLabel, _infoBuf);
}

// ══ Input Handling ═══════════════════════════════════════════════════════════

void Fluid2DApp::handleKey(const KeyEvent& ev) {
    if (!_screen) return;
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    _idleFrames = 0;

    switch (ev.code) {
        case KeyCode::UP:
            if (_cursorY > 1) _cursorY--;
            if (_brushMode == BrushMode::WALL && _obstacle) {
                _obstacle[IX(_cursorX, _cursorY)] = 1;
            } else if (_brushMode == BrushMode::ERASER && _obstacle) {
                int idx = IX(_cursorX, _cursorY);
                _obstacle[idx] = 0;
                if (_dens) _dens[idx] = 0.0f;
                if (_densB) _densB[idx] = 0.0f;
            }
            break;
        case KeyCode::DOWN:
            if (_cursorY < M) _cursorY++;
            if (_brushMode == BrushMode::WALL && _obstacle) {
                _obstacle[IX(_cursorX, _cursorY)] = 1;
            } else if (_brushMode == BrushMode::ERASER && _obstacle) {
                int idx = IX(_cursorX, _cursorY);
                _obstacle[idx] = 0;
                if (_dens) _dens[idx] = 0.0f;
                if (_densB) _densB[idx] = 0.0f;
            }
            break;
        case KeyCode::LEFT:
            if (_cursorX > 1) _cursorX--;
            if (_brushMode == BrushMode::WALL && _obstacle) {
                _obstacle[IX(_cursorX, _cursorY)] = 1;
            } else if (_brushMode == BrushMode::ERASER && _obstacle) {
                int idx = IX(_cursorX, _cursorY);
                _obstacle[idx] = 0;
                if (_dens) _dens[idx] = 0.0f;
                if (_densB) _densB[idx] = 0.0f;
            }
            break;
        case KeyCode::RIGHT:
            if (_cursorX < N) _cursorX++;
            if (_brushMode == BrushMode::WALL && _obstacle) {
                _obstacle[IX(_cursorX, _cursorY)] = 1;
            } else if (_brushMode == BrushMode::ERASER && _obstacle) {
                int idx = IX(_cursorX, _cursorY);
                _obstacle[idx] = 0;
                if (_dens) _dens[idx] = 0.0f;
                if (_densB) _densB[idx] = 0.0f;
            }
            break;

        case KeyCode::ENTER:
            if (_brushMode == BrushMode::WALL && _obstacle) {
                int idx = IX(_cursorX, _cursorY);
                _obstacle[idx] = _obstacle[idx] ? 0 : 1;
            } else if (_brushMode == BrushMode::RED_INK || _brushMode == BrushMode::BLUE_INK) {
                _emitting = !_emitting;
            }
            updateInfoLabel();
            break;

        case KeyCode::EXE: {
            const float hpForce = 500.0f;
            const float hpAmount = 200.0f;
            for (int dj = -2; dj <= 2; ++dj) {
                for (int di = -2; di <= 2; ++di) {
                    int ni = _cursorX + di, nj = _cursorY + dj;
                    if (ni < 1 || ni > N || nj < 1 || nj > M) continue;
                    if (_obstacle && _obstacle[IX(ni, nj)]) continue;
                    int idx = IX(ni, nj);
                    float* targetDens = (_brushMode == BrushMode::BLUE_INK) ? _densBPrev : _densPrev;
                    if (targetDens) targetDens[idx] += hpAmount;
                    if (_tempPrev) _tempPrev[idx] += hpAmount * 0.3f;
                    float dx = (float)di, dy = (float)dj;
                    float len = sqrtf(dx * dx + dy * dy);
                    if (len > 0.01f) {
                        _velXPrev[idx] += hpForce * dx / len;
                        _velYPrev[idx] += hpForce * dy / len;
                    } else {
                        _velYPrev[idx] -= hpForce;
                    }
                }
            }
            break;
        }

        case KeyCode::F1:
            _brushMode = static_cast<BrushMode>(((int)_brushMode + 1) % 4);
            updateInfoLabel();
            break;

        case KeyCode::F2:
            _palette = static_cast<Palette>(((int)_palette + 1) % 4);
            updateInfoLabel();
            break;

        case KeyCode::F3:
            _showTelemetry = !_showTelemetry;
            break;

        case KeyCode::F4:
            saveScene("autosave.f2d");
            break;

        case KeyCode::F5: {
            _flowPreset = static_cast<FlowPreset>(((int)_flowPreset + 1) % 3);
            resetFields();
            initParticles();
            if (_flowPreset == FlowPreset::WIND_TUNNEL) {
                setupWindTunnel();
            } else if (_flowPreset == FlowPreset::CONVECTION_CELL) {
                setupConvectionCell();
            }
            updateInfoLabel();
            break;
        }

        // Viscosity control: 0-9
        case KeyCode::NUM_0: _viscosity = VISC_PRESETS[0]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_1: _viscosity = VISC_PRESETS[1]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_2: _viscosity = VISC_PRESETS[2]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_3: _viscosity = VISC_PRESETS[3]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_4: _viscosity = VISC_PRESETS[4]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_5: _viscosity = VISC_PRESETS[5]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_6: _viscosity = VISC_PRESETS[6]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_7: _viscosity = VISC_PRESETS[7]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_8: _viscosity = VISC_PRESETS[8]; _diffusion = _viscosity; updateInfoLabel(); break;
        case KeyCode::NUM_9: _viscosity = VISC_PRESETS[9]; _diffusion = _viscosity; updateInfoLabel(); break;

        default:
            break;
    }

    if (_drawObj) lv_obj_invalidate(_drawObj);
}

// ══ Emitter ══════════════════════════════════════════════════════════════════

void Fluid2DApp::addEmitterForces() {
    if (!_emitting) return;
    if (_brushMode == BrushMode::WALL || _brushMode == BrushMode::ERASER) return;

    const float force  = 200.0f;
    const float amount = 100.0f;
    float* targetDens = (_brushMode == BrushMode::BLUE_INK) ? _densBPrev : _densPrev;
    if (!targetDens) return;

    auto addAt = [&](int gx, int gy) {
        if (gx < 1 || gx > N || gy < 1 || gy > M) return;
        if (_obstacle && _obstacle[IX(gx, gy)]) return;
        int idx = IX(gx, gy);
        targetDens[idx] += amount;
        _tempPrev[idx] += amount * 0.5f;
        float dx = (float)(gx - _cursorX);
        float dy = (float)(gy - _cursorY);
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.01f) {
            _velXPrev[idx] += force * dx / len;
            _velYPrev[idx] += force * dy / len;
        } else {
            _velYPrev[idx] -= force;
        }
    };

    switch (_emitterShape) {
        case EmitterShape::POINT:
            addAt(_cursorX, _cursorY);
            break;
        case EmitterShape::CROSS:
            addAt(_cursorX, _cursorY);
            addAt(_cursorX - 1, _cursorY);
            addAt(_cursorX + 1, _cursorY);
            addAt(_cursorX, _cursorY - 1);
            addAt(_cursorX, _cursorY + 1);
            break;
        case EmitterShape::RING:
            for (int a = 0; a < 8; ++a) {
                float angle = (float)a * 3.14159265f / 4.0f;
                int rx = _cursorX + (int)(2.0f * cosf(angle));
                int ry = _cursorY + (int)(2.0f * sinf(angle));
                addAt(rx, ry);
            }
            break;
    }
}

// ══ Flow Preset Forces ═══════════════════════════════════════════════════════

void Fluid2DApp::applyFlowPresetForces() {
    if (_flowPreset == FlowPreset::WIND_TUNNEL) {
        for (int j = 1; j <= M; ++j) {
            if (!_obstacle || !_obstacle[IX(1, j)]) {
                _velXPrev[IX(1, j)] += 80.0f;
                _densPrev[IX(1, j)] += 10.0f;
            }
        }
    } else if (_flowPreset == FlowPreset::CONVECTION_CELL) {
        for (int i = N / 2 - 4; i <= N / 2 + 4; ++i) {
            if (i >= 1 && i <= N) {
                _tempPrev[IX(i, M)]     += 200.0f;
                _tempPrev[IX(i, 1)]     -= 50.0f;
                _densPrev[IX(i, M)]     += 20.0f;
            }
        }
    }
}

// ══ Obstacle Setup ═══════════════════════════════════════════════════════════

void Fluid2DApp::setupWindTunnel() {
    if (!_obstacle) return;
    int cx = N / 3;
    int cy = M / 2;
    for (int j = cy - 3; j <= cy + 3; ++j) {
        for (int i = cx - 2; i <= cx + 2; ++i) {
            if (i >= 1 && i <= N && j >= 1 && j <= M) {
                _obstacle[IX(i, j)] = 1;
            }
        }
    }
}

void Fluid2DApp::setupConvectionCell() {
    // No obstacles needed for convection — buoyancy drives the flow
}

// ══ Field Reset ══════════════════════════════════════════════════════════════

void Fluid2DApp::resetFields() {
    if (_dens)     memset(_dens,     0, SIZE * sizeof(float));
    if (_densPrev) memset(_densPrev, 0, SIZE * sizeof(float));
    if (_velX)     memset(_velX,     0, SIZE * sizeof(float));
    if (_velXPrev) memset(_velXPrev, 0, SIZE * sizeof(float));
    if (_velY)     memset(_velY,     0, SIZE * sizeof(float));
    if (_velYPrev) memset(_velYPrev, 0, SIZE * sizeof(float));
    if (_temp)     memset(_temp,     0, SIZE * sizeof(float));
    if (_tempPrev) memset(_tempPrev, 0, SIZE * sizeof(float));
    if (_obstacle) memset(_obstacle, 0, SIZE);
    if (_densB)     memset(_densB,     0, SIZE * sizeof(float));
    if (_densBPrev) memset(_densBPrev, 0, SIZE * sizeof(float));
    if (_smoothObst) memset(_smoothObst, 0, SIZE * sizeof(float));
    _dt = BASE_DT;
    _flowPreset = FlowPreset::NONE;
}

// ══ Particle System ══════════════════════════════════════════════════════════

void Fluid2DApp::initParticles() {
    if (!_particles) return;
    _activeParticles = 0;
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        _particles[i].active = false;
        _particles[i].life   = 0.0f;
        _particles[i].x = 0.0f;
        _particles[i].y = 0.0f;
        for (int t = 0; t < PARTICLE_TAIL; ++t) {
            _particles[i].prevX[t] = 0.0f;
            _particles[i].prevY[t] = 0.0f;
        }
    }
}

void Fluid2DApp::spawnParticle(int idx) {
    if (idx < 0 || idx >= MAX_PARTICLES) return;
    Particle& p = _particles[idx];
    p.x = 1.0f + (float)(idx % N);
    p.y = 1.0f + (float)((idx * PARTICLE_SCATTER) % M);
    for (int t = 0; t < PARTICLE_TAIL; ++t) {
        p.prevX[t] = p.x;
        p.prevY[t] = p.y;
    }
    p.life   = PARTICLE_BASE_LIFE + (float)(idx % PARTICLE_LIFE_VAR);
    p.active = true;
}

void Fluid2DApp::stepParticles(float dt) {
    if (!_particles || !_velX || !_velY) return;
    _activeParticles = 0;

    for (int k = 0; k < MAX_PARTICLES; ++k) {
        Particle& p = _particles[k];
        if (!p.active) {
            if (_emitting || _flowPreset != FlowPreset::NONE) {
                spawnParticle(k);
            }
            continue;
        }

        // Store tail history
        for (int t = PARTICLE_TAIL - 1; t > 0; --t) {
            p.prevX[t] = p.prevX[t - 1];
            p.prevY[t] = p.prevY[t - 1];
        }
        p.prevX[0] = p.x;
        p.prevY[0] = p.y;

        // Advect by velocity field (bilinear interpolation)
        int gi = (int)p.x;
        int gj = (int)p.y;
        if (gi < 1) gi = 1; if (gi > N) gi = N;
        if (gj < 1) gj = 1; if (gj > M) gj = M;

        float vx = _velX[IX(gi, gj)];
        float vy = _velY[IX(gi, gj)];
        p.x += vx * dt * 2.0f;
        p.y += vy * dt * 2.0f;
        p.life -= dt;

        // Bounds check
        if (p.x < 1.0f || p.x > (float)N || p.y < 1.0f || p.y > (float)M
            || p.life <= 0.0f) {
            p.active = false;
        } else {
            // Check obstacle collision
            int oi = (int)p.x;
            int oj = (int)p.y;
            if (oi >= 1 && oi <= N && oj >= 1 && oj <= M &&
                _obstacle && _obstacle[IX(oi, oj)]) {
                p.active = false;
            } else {
                _activeParticles++;
            }
        }
    }
}

// ══ Physics: Vorticity Confinement ═══════════════════════════════════════════

void Fluid2DApp::vorticityConfinement() {
    // Use _tempPrev as scratch (already consumed and zeroed before this call)
    float* curl = _tempPrev;

    // Step 1: Compute curl (vorticity) at each cell
    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            curl[IX(i, j)] =
                (_velY[IX(i + 1, j)] - _velY[IX(i - 1, j)]) * 0.5f -
                (_velX[IX(i, j + 1)] - _velX[IX(i, j - 1)]) * 0.5f;
        }
    }

    // Step 2: Compute gradient of |curl| and apply confinement force
    for (int j = 2; j < M; ++j) {
        for (int i = 2; i < N; ++i) {
            float dw_dx = (fabsf(curl[IX(i + 1, j)]) - fabsf(curl[IX(i - 1, j)])) * 0.5f;
            float dw_dy = (fabsf(curl[IX(i, j + 1)]) - fabsf(curl[IX(i, j - 1)])) * 0.5f;
            float len = sqrtf(dw_dx * dw_dx + dw_dy * dw_dy) + 1e-5f;

            // Normalized gradient
            float nx = dw_dx / len;
            float ny = dw_dy / len;

            // Confinement force: N × curl
            float w = curl[IX(i, j)];
            _velX[IX(i, j)] += VORT_EPSILON * (ny * w) * _dt;
            _velY[IX(i, j)] -= VORT_EPSILON * (nx * w) * _dt;
        }
    }

    // Clear tempPrev that we used as scratch
    memset(_tempPrev, 0, SIZE * sizeof(float));
}

// ══ Physics: Thermal Buoyancy ════════════════════════════════════════════════

void Fluid2DApp::applyBuoyancy(float dt) {
    if (!_temp) return;
    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            int idx = IX(i, j);
            if (_obstacle && _obstacle[idx]) continue;
            // F_up = a * T - b * D  (upward = negative Y)
            float buoy = BUOY_A * _temp[idx] - BUOY_B * _dens[idx];
            _velY[idx] -= buoy * dt;
        }
    }
}

// ══ Physics: CFL Dynamic Timestep ════════════════════════════════════════════

float Fluid2DApp::computeCFLdt() const {
    float maxV = 0.01f;
    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            int idx = IX(i, j);
            float v = fabsf(_velX[idx]) + fabsf(_velY[idx]);
            if (v > maxV) maxV = v;
        }
    }
    float cflDt = CFL_MAX / maxV;
    return std::min(cflDt, BASE_DT);
}

// ══ FPU Sanitisation ═════════════════════════════════════════════════════════

void Fluid2DApp::sanitizeField(float* field) {
    for (int i = 0; i < SIZE; ++i) {
        if (!std::isfinite(field[i])) field[i] = 0.0f;
    }
}

// ══ Obstacle Enforcement ═════════════════════════════════════════════════════

void Fluid2DApp::enforceObstacles(float* u, float* v) {
    if (!_obstacle) return;
    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            if (_obstacle[IX(i, j)]) {
                u[IX(i, j)] = 0.0f;
                v[IX(i, j)] = 0.0f;
                _dens[IX(i, j)] = 0.0f;
                if (_densB) _densB[IX(i, j)] = 0.0f;
                if (_temp) _temp[IX(i, j)] = 0.0f;
            }
        }
    }
}

// ══ Fluid Solver (Jos Stam) ══════════════════════════════════════════════════

void Fluid2DApp::addSource(float* field, const float* source, float dt) {
    for (int i = 0; i < SIZE; ++i) {
        field[i] += dt * source[i];
    }
}

void Fluid2DApp::setBoundary(int b, float* x) {
    for (int i = 1; i <= N; ++i) {
        x[IX(i, 0)]     = (b == 2) ? -x[IX(i, 1)] : x[IX(i, 1)];
        x[IX(i, M + 1)] = (b == 2) ? -x[IX(i, M)] : x[IX(i, M)];
    }
    for (int j = 1; j <= M; ++j) {
        x[IX(0, j)]     = (b == 1) ? -x[IX(1, j)] : x[IX(1, j)];
        x[IX(N + 1, j)] = (b == 1) ? -x[IX(N, j)] : x[IX(N, j)];
    }
    x[IX(0, 0)]         = 0.5f * (x[IX(1, 0)]     + x[IX(0, 1)]);
    x[IX(0, M + 1)]     = 0.5f * (x[IX(1, M + 1)] + x[IX(0, M)]);
    x[IX(N + 1, 0)]     = 0.5f * (x[IX(N, 0)]     + x[IX(N + 1, 1)]);
    x[IX(N + 1, M + 1)] = 0.5f * (x[IX(N, M + 1)] + x[IX(N + 1, M)]);
}

void Fluid2DApp::diffuse(int b, float* x, const float* x0, float diff, float dt) {
    float a = dt * diff * N * M;
    float denom = 1.0f + 4.0f * a;

    for (int k = 0; k < GS_ITERS; ++k) {
        for (int j = 1; j <= M; ++j) {
            for (int i = 1; i <= N; ++i) {
                x[IX(i, j)] = (x0[IX(i, j)] +
                    a * (x[IX(i - 1, j)] + x[IX(i + 1, j)] +
                         x[IX(i, j - 1)] + x[IX(i, j + 1)])) / denom;
            }
        }
        setBoundary(b, x);
    }
}

void Fluid2DApp::advect(int b, float* d, const float* d0,
                         const float* u, const float* v, float dt) {
    float dt0x = dt * N;
    float dt0y = dt * M;

    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            float x = (float)i - dt0x * u[IX(i, j)];
            float y = (float)j - dt0y * v[IX(i, j)];

            if (x < 0.5f) x = 0.5f;
            if (x > N + 0.5f) x = N + 0.5f;
            if (y < 0.5f) y = 0.5f;
            if (y > M + 0.5f) y = M + 0.5f;

            int i0 = (int)x;
            int i1 = i0 + 1;
            int j0 = (int)y;
            int j1 = j0 + 1;

            float s1 = x - (float)i0;
            float s0 = 1.0f - s1;
            float t1 = y - (float)j0;
            float t0 = 1.0f - t1;

            d[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)]) +
                           s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
        }
    }
    setBoundary(b, d);
}

void Fluid2DApp::project(float* u, float* v, float* p, float* div) {
    float hx = 1.0f / N;
    float hy = 1.0f / M;

    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            div[IX(i, j)] = -0.5f * (
                hx * (u[IX(i + 1, j)] - u[IX(i - 1, j)]) +
                hy * (v[IX(i, j + 1)] - v[IX(i, j - 1)]));
            p[IX(i, j)] = 0.0f;
        }
    }
    setBoundary(0, div);
    setBoundary(0, p);

    for (int k = 0; k < GS_ITERS; ++k) {
        for (int j = 1; j <= M; ++j) {
            for (int i = 1; i <= N; ++i) {
                p[IX(i, j)] = (div[IX(i, j)] +
                    p[IX(i - 1, j)] + p[IX(i + 1, j)] +
                    p[IX(i, j - 1)] + p[IX(i, j + 1)]) / 4.0f;
            }
        }
        setBoundary(0, p);
    }

    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            u[IX(i, j)] -= 0.5f * N * (p[IX(i + 1, j)] - p[IX(i - 1, j)]);
            v[IX(i, j)] -= 0.5f * M * (p[IX(i, j + 1)] - p[IX(i, j - 1)]);
        }
    }
    setBoundary(1, u);
    setBoundary(2, v);
}

void Fluid2DApp::fluidStep() {
    // ── Dynamic timestep (CFL condition) ──
    _dt = computeCFLdt();

    // ── Velocity step ──
    addSource(_velX, _velXPrev, _dt);
    addSource(_velY, _velYPrev, _dt);

    // Consume temperature source BEFORE vorticity confinement
    // (vorticityConfinement uses _tempPrev as scratch space)
    addSource(_temp, _tempPrev, _dt);
    memset(_tempPrev, 0, SIZE * sizeof(float));

    // Vorticity confinement (amplify small-scale eddies)
    vorticityConfinement();

    // Thermal buoyancy
    applyBuoyancy(_dt);

    // Diffuse velocity
    std::swap(_velXPrev, _velX);
    diffuse(1, _velX, _velXPrev, _viscosity, _dt);
    std::swap(_velYPrev, _velY);
    diffuse(2, _velY, _velYPrev, _viscosity, _dt);

    project(_velX, _velY, _velXPrev, _velYPrev);

    std::swap(_velXPrev, _velX);
    std::swap(_velYPrev, _velY);
    advect(1, _velX, _velXPrev, _velXPrev, _velYPrev, _dt);
    advect(2, _velY, _velYPrev, _velXPrev, _velYPrev, _dt);

    project(_velX, _velY, _velXPrev, _velYPrev);

    // ── Density step ──
    addSource(_dens, _densPrev, _dt);

    std::swap(_densPrev, _dens);
    diffuse(0, _dens, _densPrev, _diffusion, _dt);

    std::swap(_densPrev, _dens);
    advect(0, _dens, _densPrev, _velX, _velY, _dt);

    // ── DensityB step ──
    if (_densB && _densBPrev) {
        addSource(_densB, _densBPrev, _dt);
        std::swap(_densBPrev, _densB);
        diffuse(0, _densB, _densBPrev, _diffusion, _dt);
        std::swap(_densBPrev, _densB);
        advect(0, _densB, _densBPrev, _velX, _velY, _dt);
    }

    // ── Temperature step (source already consumed above) ──
    std::swap(_tempPrev, _temp);
    diffuse(0, _temp, _tempPrev, _diffusion * 0.5f, _dt);

    std::swap(_tempPrev, _temp);
    advect(0, _temp, _tempPrev, _velX, _velY, _dt);

    // Natural temperature decay
    for (int i = 0; i < SIZE; ++i) {
        _temp[i] *= 0.99f;
    }

    // ── Enforce obstacles ──
    enforceObstacles(_velX, _velY);

    // ── FPU sanitisation ──
    sanitizeField(_velX);
    sanitizeField(_velY);
    sanitizeField(_dens);
    sanitizeField(_temp);
    if (_densB) sanitizeField(_densB);

    // ── Particle advection ──
    stepParticles(_dt);

    // ── Clear source arrays ──
    memset(_densPrev, 0, SIZE * sizeof(float));
    memset(_velXPrev, 0, SIZE * sizeof(float));
    memset(_velYPrev, 0, SIZE * sizeof(float));
    memset(_tempPrev, 0, SIZE * sizeof(float));
    if (_densBPrev) memset(_densBPrev, 0, SIZE * sizeof(float));
}

// ══ HUD Helpers ══════════════════════════════════════════════════════════════

float Fluid2DApp::computeAvgVelocity() const {
    float sum = 0.0f;
    int count = 0;
    for (int j = 1; j <= M; j += 4) {
        for (int i = 1; i <= N; i += 4) {
            int idx = IX(i, j);
            sum += sqrtf(_velX[idx] * _velX[idx] + _velY[idx] * _velY[idx]);
            count++;
        }
    }
    return (count > 0) ? sum / (float)count : 0.0f;
}

float Fluid2DApp::computeReynolds() const {
    float avgV = computeAvgVelocity();
    float L = (float)N;
    return (_viscosity > 1e-8f) ? (avgV * L / _viscosity) : 0.0f;
}

// ══ Color Palettes ═══════════════════════════════════════════════════════════

lv_color_t Fluid2DApp::infernoColor(float t) {
    // Inferno LUT approximation: black → purple → orange → yellow
    t = std::max(0.0f, std::min(t, 1.0f));
    uint8_t r, g, b;
    if (t < 0.25f) {
        float s = t / 0.25f;
        r = (uint8_t)(s * 80); g = 0; b = (uint8_t)(s * 120);
    } else if (t < 0.5f) {
        float s = (t - 0.25f) / 0.25f;
        r = (uint8_t)(80 + s * 140); g = (uint8_t)(s * 40); b = (uint8_t)(120 - s * 40);
    } else if (t < 0.75f) {
        float s = (t - 0.5f) / 0.25f;
        r = (uint8_t)(220 + s * 35); g = (uint8_t)(40 + s * 120); b = (uint8_t)(80 - s * 80);
    } else {
        float s = (t - 0.75f) / 0.25f;
        r = 255; g = (uint8_t)(160 + s * 95); b = (uint8_t)(s * 60);
    }
    return lv_color_make(r, g, b);
}

lv_color_t Fluid2DApp::oceanColor(float t) {
    // Ocean LUT: black → deep blue → teal → cyan → white
    t = std::max(0.0f, std::min(t, 1.0f));
    uint8_t r, g, b;
    if (t < 0.3f) {
        float s = t / 0.3f;
        r = 0; g = (uint8_t)(s * 30); b = (uint8_t)(s * 150);
    } else if (t < 0.6f) {
        float s = (t - 0.3f) / 0.3f;
        r = 0; g = (uint8_t)(30 + s * 150); b = (uint8_t)(150 + s * 50);
    } else if (t < 0.85f) {
        float s = (t - 0.6f) / 0.25f;
        r = (uint8_t)(s * 100); g = (uint8_t)(180 + s * 55); b = (uint8_t)(200 + s * 55);
    } else {
        float s = (t - 0.85f) / 0.15f;
        r = (uint8_t)(100 + s * 155); g = (uint8_t)(235 + s * 20); b = 255;
    }
    return lv_color_make(r, g, b);
}

lv_color_t Fluid2DApp::toxicColor(float t) {
    // Toxic LUT: black → dark green → neon green → yellow-green
    t = std::max(0.0f, std::min(t, 1.0f));
    uint8_t r, g, b;
    if (t < 0.3f) {
        float s = t / 0.3f;
        r = 0; g = (uint8_t)(s * 80); b = (uint8_t)(s * 20);
    } else if (t < 0.6f) {
        float s = (t - 0.3f) / 0.3f;
        r = (uint8_t)(s * 40); g = (uint8_t)(80 + s * 175); b = (uint8_t)(20 - s * 20);
    } else {
        float s = (t - 0.6f) / 0.4f;
        r = (uint8_t)(40 + s * 180); g = 255; b = (uint8_t)(s * 60);
    }
    return lv_color_make(r, g, b);
}

lv_color_t Fluid2DApp::classicColor(float t) {
    // Classic heatmap: black → blue → cyan → green → yellow → red → white
    t = std::max(0.0f, std::min(t, 1.0f));
    uint8_t r, g, b;
    if (t < 0.2f) {
        float s = t / 0.2f;
        r = 0; g = 0; b = (uint8_t)(s * 180);
    } else if (t < 0.4f) {
        float s = (t - 0.2f) / 0.2f;
        r = 0; g = (uint8_t)(s * 220); b = 180;
    } else if (t < 0.6f) {
        float s = (t - 0.4f) / 0.2f;
        r = 0; g = 220; b = (uint8_t)(180 * (1.0f - s));
    } else if (t < 0.8f) {
        float s = (t - 0.6f) / 0.2f;
        r = (uint8_t)(s * 255); g = (uint8_t)(220 - s * 80); b = 0;
    } else {
        float s = (t - 0.8f) / 0.2f;
        r = 255; g = (uint8_t)(140 + s * 115); b = (uint8_t)(s * 200);
    }
    return lv_color_make(r, g, b);
}

lv_color_t Fluid2DApp::applyPalette(float d) const {
    float t = std::max(0.0f, std::min(d / 5.0f, 1.0f));
    switch (_palette) {
        case Palette::CLASSIC:  return classicColor(t);
        case Palette::THERMAL:  return infernoColor(t);
        default:                return classicColor(t);
    }
    return classicColor(t);
}

// ══ Blinn-Phong Liquid Shading ═══════════════════════════════════════════════

lv_color_t Fluid2DApp::shadedDensityColor(int i, int j) const {
    int idx = IX(i, j);
    float d = _dens[idx];
    lv_color_t base = applyPalette(d);

    if (d < 0.05f) return base;

    // Compute density gradient (fake normal)
    float dL = (i > 1) ? _dens[IX(i - 1, j)] : d;
    float dR = (i < N) ? _dens[IX(i + 1, j)] : d;
    float dU = (j > 1) ? _dens[IX(i, j - 1)] : d;
    float dD = (j < M) ? _dens[IX(i, j + 1)] : d;

    float gx = (dR - dL) * 0.5f;
    float gy = (dD - dU) * 0.5f;

    // Fake normal from gradient  (light from top-left: L = normalize(-1, -1, 1))
    float nLen = sqrtf(gx * gx + gy * gy + 1.0f);
    float nx = -gx / nLen;
    float ny = -gy / nLen;
    float nz = 1.0f / nLen;

    // Light direction (normalised top-left)
    static constexpr float LX = -0.577f;
    static constexpr float LY = -0.577f;
    static constexpr float LZ =  0.577f;

    // Diffuse term
    float diff = nx * LX + ny * LY + nz * LZ;
    diff = std::max(0.0f, diff);

    // Blinn-Phong specular: H = normalize(L + V), V = (0,0,1)
    static constexpr float HX = -0.408f;
    static constexpr float HY = -0.408f;
    static constexpr float HZ =  0.816f;
    float spec = nx * HX + ny * HY + nz * HZ;
    spec = std::max(0.0f, spec);
    spec = spec * spec * spec * spec;  // shininess ≈ 16

    // Apply lighting: base * (ambient + diffuse) + specular
    float ambient = 0.3f;
    float lit = ambient + 0.7f * diff;

    uint8_t r = (uint8_t)std::min(255.0f, base.red * lit + spec * SPECULAR_INTENSITY);
    uint8_t g = (uint8_t)std::min(255.0f, base.green * lit + spec * SPECULAR_INTENSITY);
    uint8_t b = (uint8_t)std::min(255.0f, base.blue * lit + spec * SPECULAR_INTENSITY);

    return lv_color_make(r, g, b);
}

lv_color_t Fluid2DApp::velocityToColor(float vx, float vy) {
    float mag = sqrtf(vx * vx + vy * vy);
    mag = std::min(mag / 5.0f, 1.0f);

    float angle = atan2f(vy, vx);
    float hue = (angle + 3.14159265f) / (2.0f * 3.14159265f);

    float h6 = hue * 6.0f;
    int hi = (int)h6 % 6;
    float f = h6 - (float)hi;
    float q = mag * (1.0f - f);
    float t = mag * f;

    uint8_t r, g, b;
    switch (hi) {
        case 0: r = (uint8_t)(mag*255); g = (uint8_t)(t*255);   b = 0;                  break;
        case 1: r = (uint8_t)(q*255);   g = (uint8_t)(mag*255); b = 0;                  break;
        case 2: r = 0;                  g = (uint8_t)(mag*255); b = (uint8_t)(t*255);   break;
        case 3: r = 0;                  g = (uint8_t)(q*255);   b = (uint8_t)(mag*255); break;
        case 4: r = (uint8_t)(t*255);   g = 0;                  b = (uint8_t)(mag*255); break;
        default:r = (uint8_t)(mag*255); g = 0;                  b = (uint8_t)(q*255);   break;
    }

    return lv_color_make(r, g, b);
}

lv_color_t Fluid2DApp::vorticityToColor(float w) {
    float t = std::max(-1.0f, std::min(w / 2.0f, 1.0f));
    if (t < 0) {
        uint8_t v = (uint8_t)(-t * 255);
        return lv_color_make(0, 0, v);
    } else {
        uint8_t v = (uint8_t)(t * 255);
        return lv_color_make(v, 0, 0);
    }
}

// ══ Custom Draw Callback ═════════════════════════════════════════════════════

void Fluid2DApp::onDraw(lv_event_t* e) {
    Fluid2DApp* app = static_cast<Fluid2DApp*>(lv_event_get_user_data(e));
    if (!app || !app->_screen || !app->_dens) return;

    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer || !obj) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int ox = coords.x1;
    int oy = coords.y1;

    // ── Background ───────────────────────────────────────────────────────
    {
        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = lv_color_hex(COL_BG);
        bg.bg_opa   = LV_OPA_COVER;
        bg.radius   = 0;
        lv_draw_rect(layer, &bg, &coords);
    }

    // ── Draw fluid cells ─────────────────────────────────────────────────
    const lv_area_t& clip = layer->_clip_area;

    for (int j = 1; j <= M; ++j) {
        int py = oy + (j - 1) * CELL_H;
        if (py + CELL_H < clip.y1 || py > clip.y2) continue;

        for (int i = 1; i <= N; ++i) {
            int px = ox + (i - 1) * CELL_W;
            if (px + CELL_W < clip.x1 || px > clip.x2) continue;

            int idx = IX(i, j);

            // Draw obstacle cells (with smooth boundaries)
            if (app->_smoothObst && app->_smoothObst[idx] > 0.01f) {
                float obs = app->_smoothObst[idx];
                if (obs > 0.5f) {
                    lv_area_t cell = {
                        (int16_t)px, (int16_t)py,
                        (int16_t)(px + CELL_W - 1), (int16_t)(py + CELL_H - 1)
                    };
                    lv_draw_rect_dsc_t rd;
                    lv_draw_rect_dsc_init(&rd);
                    rd.bg_color = lv_color_hex(COL_WALL);
                    rd.bg_opa = (lv_opa_t)std::min(255, (int)(obs * 255));
                    rd.radius = 0;
                    lv_draw_rect(layer, &rd, &cell);
                    continue;
                }
            } else if (app->_obstacle && app->_obstacle[idx]) {
                lv_area_t cell = {
                    (int16_t)px, (int16_t)py,
                    (int16_t)(px + CELL_W - 1), (int16_t)(py + CELL_H - 1)
                };
                lv_draw_rect_dsc_t rd;
                lv_draw_rect_dsc_init(&rd);
                rd.bg_color = lv_color_hex(COL_WALL);
                rd.bg_opa   = LV_OPA_COVER;
                rd.radius   = 0;
                lv_draw_rect(layer, &rd, &cell);
                continue;
            }

            lv_color_t col;
            switch (app->_palette) {
                case Palette::CLASSIC:
                case Palette::THERMAL:
                    col = app->shadedDensityColor(i, j);
                    break;
                case Palette::VELOCITY:
                    col = velocityToColor(app->_velX[idx], app->_velY[idx]);
                    break;
                case Palette::MIXED:
                    col = mixedColor(app->_dens[idx],
                                    app->_densB ? app->_densB[idx] : 0.0f);
                    break;
            }

            if (col.red > 2 || col.green > 2 || col.blue > 2) {
                lv_area_t cell = {
                    (int16_t)px, (int16_t)py,
                    (int16_t)(px + CELL_W - 1), (int16_t)(py + CELL_H - 1)
                };
                lv_draw_rect_dsc_t rd;
                lv_draw_rect_dsc_init(&rd);
                rd.bg_color = col;
                rd.bg_opa   = LV_OPA_COVER;
                rd.radius   = 0;
                lv_draw_rect(layer, &rd, &cell);
            }
        }
    }

    // ── Particle overlay ─────────────────────────────────────────────────
    if (app->_particles) {
        lv_draw_rect_dsc_t ptDsc;
        lv_draw_rect_dsc_init(&ptDsc);
        ptDsc.bg_opa = LV_OPA_COVER;
        ptDsc.radius = 0;

        lv_draw_line_dsc_t tailDsc;
        lv_draw_line_dsc_init(&tailDsc);
        tailDsc.width = 1;

        for (int k = 0; k < MAX_PARTICLES; ++k) {
            const Particle& p = app->_particles[k];
            if (!p.active) continue;

            int ppx = ox + (int)((p.x - 1.0f) * CELL_W);
            int ppy = oy + (int)((p.y - 1.0f) * CELL_H);

            if (ppx < clip.x1 || ppx > clip.x2 ||
                ppy < clip.y1 || ppy > clip.y2) continue;

            // Draw tail (motion blur)
            for (int t = 0; t < PARTICLE_TAIL; ++t) {
                int tx = ox + (int)((p.prevX[t] - 1.0f) * CELL_W);
                int ty = oy + (int)((p.prevY[t] - 1.0f) * CELL_H);
                uint8_t alpha = (uint8_t)(TAIL_BASE_ALPHA - t * TAIL_ALPHA_DECAY);
                tailDsc.color = lv_color_hex(COL_PARTICLE);
                tailDsc.opa   = alpha;
                if (t == 0) {
                    tailDsc.p1.x = ppx;
                    tailDsc.p1.y = ppy;
                } else {
                    int px2 = ox + (int)((p.prevX[t-1] - 1.0f) * CELL_W);
                    int py2 = oy + (int)((p.prevY[t-1] - 1.0f) * CELL_H);
                    tailDsc.p1.x = px2;
                    tailDsc.p1.y = py2;
                }
                tailDsc.p2.x = tx;
                tailDsc.p2.y = ty;
                lv_draw_line(layer, &tailDsc);
            }

            // Draw particle head (1px dot)
            ptDsc.bg_color = lv_color_hex(COL_PARTICLE);
            ptDsc.bg_opa   = (uint8_t)std::min(255.0f, p.life * PARTICLE_OPA_SCALE);
            lv_area_t ptArea = {
                (int16_t)ppx, (int16_t)ppy,
                (int16_t)(ppx), (int16_t)(ppy)
            };
            lv_draw_rect(layer, &ptDsc, &ptArea);
        }
    }

    // ── Cursor ───────────────────────────────────────────────────────────
    {
        int cx = ox + (app->_cursorX - 1) * CELL_W;
        int cy = oy + (app->_cursorY - 1) * CELL_H;

        lv_draw_rect_dsc_t curDsc;
        lv_draw_rect_dsc_init(&curDsc);
        curDsc.bg_opa    = LV_OPA_TRANSP;

        if (app->_brushMode == BrushMode::WALL) {
            curDsc.border_color = lv_color_hex(COL_WALL);
            curDsc.border_width = 2;
            curDsc.border_opa   = LV_OPA_COVER;
        } else if (app->_brushMode == BrushMode::ERASER) {
            curDsc.border_color = lv_color_hex(0xFF4040);
            curDsc.border_width = 2;
            curDsc.border_opa   = LV_OPA_COVER;
        } else {
            curDsc.border_color = lv_color_hex(COL_CURSOR);
            curDsc.border_width = 1;
            curDsc.border_opa   = app->_emitting ? LV_OPA_COVER : LV_OPA_70;
        }
        curDsc.radius = 0;

        lv_area_t curArea = {
            (int16_t)(cx - 1), (int16_t)(cy - 1),
            (int16_t)(cx + CELL_W), (int16_t)(cy + CELL_H)
        };
        lv_draw_rect(layer, &curDsc, &curArea);
    }

    // ── Telemetry HUD ───────────────────────────────────────────────────
    if (app->_showTelemetry) {
        // Probe tooltip (when idle > 60 frames)
        if (app->_idleFrames > PROBE_IDLE_FRAMES) {
            int ci = app->_cursorX;
            int cj = app->_cursorY;
            int probeIdx = IX(ci, cj);
            float vx = app->_velX[probeIdx];
            float vy = app->_velY[probeIdx];
            float div = app->computeLocalDivergence(ci, cj);

            char probeBuf[64];
            snprintf(probeBuf, sizeof(probeBuf),
                     "V:(%.1f,%.1f)\nDiv:%.2f", vx, vy, div);

            int tipPx = ox + (ci - 1) * CELL_W + CELL_W + 4;
            int tipPy = oy + (cj - 1) * CELL_H - 20;
            if (tipPx > 260) tipPx = ox + (ci - 1) * CELL_W - 70;
            if (tipPy < oy) tipPy = oy + 2;

            lv_area_t tipBg = {
                (int16_t)(tipPx - 2), (int16_t)(tipPy - 2),
                (int16_t)(tipPx + 68), (int16_t)(tipPy + 22)
            };
            lv_draw_rect_dsc_t tipRd;
            lv_draw_rect_dsc_init(&tipRd);
            tipRd.bg_color = lv_color_hex(0x000000);
            tipRd.bg_opa = LV_OPA_50;
            tipRd.radius = 2;
            lv_draw_rect(layer, &tipRd, &tipBg);

            lv_draw_label_dsc_t tipLbl;
            lv_draw_label_dsc_init(&tipLbl);
            tipLbl.color = lv_color_hex(0xE0E0E0);
            tipLbl.font = &lv_font_montserrat_10;
            tipLbl.text = probeBuf;
            lv_draw_label(layer, &tipLbl, &tipBg);
        }

        // Sparkline (top-left, 30 samples)
        {
            int sparkX = ox + 4;
            int sparkY = oy + 4;
            int sparkW = 60;
            int sparkH = 20;

            lv_area_t sparkBg = {
                (int16_t)sparkX, (int16_t)sparkY,
                (int16_t)(sparkX + sparkW), (int16_t)(sparkY + sparkH)
            };
            lv_draw_rect_dsc_t sparkRd;
            lv_draw_rect_dsc_init(&sparkRd);
            sparkRd.bg_color = lv_color_hex(0x000000);
            sparkRd.bg_opa = LV_OPA_40;
            sparkRd.radius = 2;
            lv_draw_rect(layer, &sparkRd, &sparkBg);

            float maxE = 0.01f;
            for (int s = 0; s < ENERGY_SAMPLES; ++s) {
                if (app->_energyHistory[s] > maxE) maxE = app->_energyHistory[s];
            }

            lv_draw_line_dsc_t sparkLine;
            lv_draw_line_dsc_init(&sparkLine);
            sparkLine.color = lv_color_hex(0x00FF80);
            sparkLine.width = 1;
            sparkLine.opa = LV_OPA_70;

            for (int s = 0; s < ENERGY_SAMPLES - 1; ++s) {
                int si  = (app->_energyIdx + s) % ENERGY_SAMPLES;
                int si1 = (app->_energyIdx + s + 1) % ENERGY_SAMPLES;

                float e0 = app->_energyHistory[si] / maxE;
                float e1 = app->_energyHistory[si1] / maxE;

                sparkLine.p1.x = sparkX + s * sparkW / ENERGY_SAMPLES;
                sparkLine.p1.y = sparkY + sparkH - (int)(e0 * (sparkH - 2));
                sparkLine.p2.x = sparkX + (s + 1) * sparkW / ENERGY_SAMPLES;
                sparkLine.p2.y = sparkY + sparkH - (int)(e1 * (sparkH - 2));
                lv_draw_line(layer, &sparkLine);
            }
        }
    }
}

// ══ Timer Callback ═══════════════════════════════════════════════════════════

void Fluid2DApp::onSimTimer(lv_timer_t* timer) {
    Fluid2DApp* app = static_cast<Fluid2DApp*>(lv_timer_get_user_data(timer));
    if (!app || !app->_screen || !app->_dens) return;

    app->addEmitterForces();
    app->applyFlowPresetForces();

    // Sub-stepping: if max velocity exceeds threshold, use 2 steps with dt/2
    float maxV = 0.01f;
    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            float v = fabsf(app->_velX[IX(i,j)]) + fabsf(app->_velY[IX(i,j)]);
            if (v > maxV) maxV = v;
        }
    }

    int steps = (maxV > SUBSTEP_THRESHOLD) ? 2 : 1;
    float origDt = app->_dt;
    if (steps > 1) app->_dt *= 0.5f;

    for (int s = 0; s < steps; ++s) {
        app->fluidStep();
    }

    if (steps > 1) app->_dt = origDt;

    // Smooth obstacles for rendering
    app->smoothObstacles();

    // Update telemetry
    app->_idleFrames++;
    if (app->_showTelemetry) {
        app->_energyHistory[app->_energyIdx] = app->computeTotalEnergy();
        app->_energyIdx = (app->_energyIdx + 1) % ENERGY_SAMPLES;
    }

    app->updateInfoLabel();
    if (app->_drawObj) lv_obj_invalidate(app->_drawObj);
}

// ══ Mixed Color (Dual-Density Chromatic) ═════════════════════════════════════

lv_color_t Fluid2DApp::mixedColor(float densA, float densB) {
    float a = std::max(0.0f, std::min(densA / 5.0f, 1.0f));
    float b = std::max(0.0f, std::min(densB / 5.0f, 1.0f));
    uint8_t r = (uint8_t)(a * 255);
    uint8_t g = 0;
    uint8_t bl = (uint8_t)(b * 255);
    return lv_color_make(r, g, bl);
}

// ══ Telemetry Helpers ════════════════════════════════════════════════════════

float Fluid2DApp::computeTotalEnergy() const {
    float sum = 0.0f;
    for (int j = 1; j <= M; j += 2) {
        for (int i = 1; i <= N; i += 2) {
            int idx = IX(i, j);
            sum += _velX[idx] * _velX[idx] + _velY[idx] * _velY[idx];
        }
    }
    return sum;
}

float Fluid2DApp::computeLocalDivergence(int i, int j) const {
    if (i < 2 || i >= N || j < 2 || j >= M) return 0.0f;
    return (_velX[IX(i+1,j)] - _velX[IX(i-1,j)]) * 0.5f +
           (_velY[IX(i,j+1)] - _velY[IX(i,j-1)]) * 0.5f;
}

// ══ Boundary Smoothing ══════════════════════════════════════════════════════

void Fluid2DApp::smoothObstacles() {
    if (!_obstacle || !_smoothObst) return;
    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            int idx = IX(i, j);
            float sum = (float)_obstacle[idx];
            int count = 1;
            if (i > 1)  { sum += (float)_obstacle[IX(i-1, j)]; count++; }
            if (i < N)  { sum += (float)_obstacle[IX(i+1, j)]; count++; }
            if (j > 1)  { sum += (float)_obstacle[IX(i, j-1)]; count++; }
            if (j < M)  { sum += (float)_obstacle[IX(i, j+1)]; count++; }
            _smoothObst[idx] = sum / (float)count;
        }
    }
}

// ══ LittleFS Persistence (Scene Manager) ═════════════════════════════════════

void Fluid2DApp::autoSave() {
    saveScene("autosave.f2d");
}

void Fluid2DApp::saveScene(const char* name) {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;
    if (!LittleFS.exists("/fluid")) {
        LittleFS.mkdir("/fluid");
    }

    char path[64];
    snprintf(path, sizeof(path), "/fluid/%s", name);

    File f = LittleFS.open(path, "w");
    if (!f) return;

    uint32_t magic = SCENE_FILE_MAGIC;
    f.write((const uint8_t*)&magic, 4);

    f.write((const uint8_t*)&_viscosity, sizeof(float));
    f.write((const uint8_t*)&_diffusion, sizeof(float));
    uint8_t pal = (uint8_t)_palette;
    f.write(&pal, 1);

    if (_obstacle) {
        f.write(_obstacle, SIZE);
    }

    f.close();
#else
    (void)name;
#endif
}

void Fluid2DApp::loadScene(const char* name) {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;

    char path[64];
    snprintf(path, sizeof(path), "/fluid/%s", name);

    File f = LittleFS.open(path, "r");
    if (!f) return;

    uint32_t magic = 0;
    f.read((uint8_t*)&magic, 4);
    if (magic != SCENE_FILE_MAGIC) { f.close(); return; }

    f.read((uint8_t*)&_viscosity, sizeof(float));
    f.read((uint8_t*)&_diffusion, sizeof(float));
    uint8_t pal = 0;
    f.read(&pal, 1);
    _palette = static_cast<Palette>(pal % 4);

    if (_obstacle) {
        f.read(_obstacle, SIZE);
    }

    f.close();
    smoothObstacles();
    updateInfoLabel();
#else
    (void)name;
#endif
}

void Fluid2DApp::autoLoad() {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;
    if (LittleFS.exists("/fluid/autosave.f2d")) {
        loadScene("autosave.f2d");
    }
#endif
}
