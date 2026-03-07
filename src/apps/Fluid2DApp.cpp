/**
 * Fluid2DApp.cpp — Real-Time 2D Fluid Simulator for NumOS.
 *
 * Implements Jos Stam's "Stable Fluids" (SIGGRAPH 1999):
 *   1. Add forces (emitter input)
 *   2. Diffuse  (implicit Gauss-Seidel)
 *   3. Project  (pressure solve → divergence-free)
 *   4. Advect   (semi-Lagrangian back-trace)
 *   5. Project  (post-advection correction)
 *
 * Custom draw via LV_EVENT_DRAW_MAIN — density mapped to heatmap colours.
 */

#include "Fluid2DApp.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

// ══ Color palette ════════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0x0A0A1A;   // deep navy
static constexpr uint32_t COL_TEXT       = 0xE0E0E0;
static constexpr uint32_t COL_TEXT_DIM   = 0x808080;
static constexpr uint32_t COL_CURSOR     = 0xFFD700;   // gold cursor

// ══ Layout constants ═════════════════════════════════════════════════════════
static constexpr int DRAW_Y  = 24;   // below StatusBar
static constexpr int DRAW_H  = 216;  // 240 - 24
static constexpr int INFO_H  = 12;

// ══ Constructor / Destructor ═════════════════════════════════════════════════

Fluid2DApp::Fluid2DApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _infoLabel(nullptr)
    , _simTimer(nullptr)
    , _dens(nullptr), _densPrev(nullptr)
    , _velX(nullptr), _velXPrev(nullptr)
    , _velY(nullptr), _velYPrev(nullptr)
    , _diffusion(DEFAULT_DIFF)
    , _viscosity(DEFAULT_VISC)
    , _paused(false)
    , _emitting(false)
    , _cursorX(N / 2), _cursorY(M / 2)
    , _emitterShape(EmitterShape::POINT)
    , _vizMode(VizMode::DENSITY)
    , _showArrows(false)
{
    _infoBuf[0] = '\0';
}

Fluid2DApp::~Fluid2DApp() {
    end();
}

// ══ Lifecycle ════════════════════════════════════════════════════════════════

void Fluid2DApp::begin() {
    if (_screen) return;

    // Allocate 6 float arrays of SIZE each
    const size_t bytes = SIZE * sizeof(float);

#ifdef ARDUINO
    _dens     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _densPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velX     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velXPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velY     = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    _velYPrev = (float*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
#else
    _dens     = new float[SIZE];
    _densPrev = new float[SIZE];
    _velX     = new float[SIZE];
    _velXPrev = new float[SIZE];
    _velY     = new float[SIZE];
    _velYPrev = new float[SIZE];
#endif

    if (!_dens || !_densPrev || !_velX || !_velXPrev || !_velY || !_velYPrev) {
        return;  // allocation failure
    }

    resetFields();

    // ── LVGL screen ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Fluid 2D");

    createUI();

    _paused   = false;
    _emitting = false;
    _cursorX  = N / 2;
    _cursorY  = M / 2;
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
        if (_dens)     { heap_caps_free(_dens);     _dens     = nullptr; }
        if (_densPrev) { heap_caps_free(_densPrev); _densPrev = nullptr; }
        if (_velX)     { heap_caps_free(_velX);     _velX     = nullptr; }
        if (_velXPrev) { heap_caps_free(_velXPrev); _velXPrev = nullptr; }
        if (_velY)     { heap_caps_free(_velY);     _velY     = nullptr; }
        if (_velYPrev) { heap_caps_free(_velYPrev); _velYPrev = nullptr; }
#else
        delete[] _dens;     _dens     = nullptr;
        delete[] _densPrev; _densPrev = nullptr;
        delete[] _velX;     _velX     = nullptr;
        delete[] _velXPrev; _velXPrev = nullptr;
        delete[] _velY;     _velY     = nullptr;
        delete[] _velYPrev; _velYPrev = nullptr;
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
    // Custom-drawn fluid area
    _drawObj = lv_obj_create(_screen);
    lv_obj_set_size(_drawObj, SCREEN_W, DRAW_H);
    lv_obj_set_pos(_drawObj, 0, DRAW_Y);
    lv_obj_set_style_bg_opa(_drawObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_drawObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_drawObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_drawObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(_drawObj, this);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Info label (bottom overlay)
    _infoLabel = lv_label_create(_screen);
    lv_obj_set_pos(_infoLabel, 4, SCREEN_H - INFO_H - 2);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    updateInfoLabel();

    // Simulation timer (~30 Hz)
    _simTimer = lv_timer_create(onSimTimer, 33, this);
}

void Fluid2DApp::updateInfoLabel() {
    const char* vizNames[] = { "Density", "Velocity", "Vorticity" };
    const char* shapeNames[] = { "Point", "Cross", "Ring" };
    snprintf(_infoBuf, sizeof(_infoBuf), "%s | %s | %s%s",
             vizNames[(int)_vizMode],
             shapeNames[(int)_emitterShape],
             _paused ? "PAUSED" : "RUNNING",
             _emitting ? " | EMIT" : "");
    lv_label_set_text(_infoLabel, _infoBuf);
}

// ══ Input Handling ═══════════════════════════════════════════════════════════

void Fluid2DApp::handleKey(const KeyEvent& ev) {
    if (!_screen) return;
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (ev.code) {
        case KeyCode::UP:
            if (_cursorY > 1) _cursorY--;
            break;
        case KeyCode::DOWN:
            if (_cursorY < M) _cursorY++;
            break;
        case KeyCode::LEFT:
            if (_cursorX > 1) _cursorX--;
            break;
        case KeyCode::RIGHT:
            if (_cursorX < N) _cursorX++;
            break;

        case KeyCode::ENTER:
            _emitting = !_emitting;
            updateInfoLabel();
            break;

        case KeyCode::EXE:
            _paused = !_paused;
            updateInfoLabel();
            break;

        case KeyCode::F1:
            // Cycle visualization mode
            _vizMode = static_cast<VizMode>(
                ((int)_vizMode + 1) % 3);
            updateInfoLabel();
            break;

        case KeyCode::F2:
            _showArrows = !_showArrows;
            break;

        case KeyCode::F3:
            resetFields();
            break;

        case KeyCode::F4:
            // Cycle emitter shape
            _emitterShape = static_cast<EmitterShape>(
                ((int)_emitterShape + 1) % 3);
            updateInfoLabel();
            break;

        default:
            break;
    }

    // Always trigger a redraw when a key is pressed
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

// ══ Emitter ══════════════════════════════════════════════════════════════════

void Fluid2DApp::addEmitterForces() {
    if (!_emitting) return;

    const float force  = 200.0f;
    const float amount = 100.0f;

    auto addAt = [&](int gx, int gy) {
        if (gx < 1 || gx > N || gy < 1 || gy > M) return;
        int idx = IX(gx, gy);
        _densPrev[idx] += amount;
        // Add slight radial velocity from cursor
        float dx = (float)(gx - _cursorX);
        float dy = (float)(gy - _cursorY);
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.01f) {
            _velXPrev[idx] += force * dx / len;
            _velYPrev[idx] += force * dy / len;
        } else {
            // At center, push upward by default
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

// ══ Field Reset ══════════════════════════════════════════════════════════════

void Fluid2DApp::resetFields() {
    if (_dens)     memset(_dens,     0, SIZE * sizeof(float));
    if (_densPrev) memset(_densPrev, 0, SIZE * sizeof(float));
    if (_velX)     memset(_velX,     0, SIZE * sizeof(float));
    if (_velXPrev) memset(_velXPrev, 0, SIZE * sizeof(float));
    if (_velY)     memset(_velY,     0, SIZE * sizeof(float));
    if (_velYPrev) memset(_velYPrev, 0, SIZE * sizeof(float));
}

// ══ Fluid Solver (Jos Stam) ══════════════════════════════════════════════════

void Fluid2DApp::addSource(float* field, const float* source) {
    for (int i = 0; i < SIZE; ++i) {
        field[i] += DT * source[i];
    }
}

void Fluid2DApp::setBoundary(int b, float* x) {
    // Top and bottom walls
    for (int i = 1; i <= N; ++i) {
        x[IX(i, 0)]     = (b == 2) ? -x[IX(i, 1)] : x[IX(i, 1)];
        x[IX(i, M + 1)] = (b == 2) ? -x[IX(i, M)] : x[IX(i, M)];
    }
    // Left and right walls
    for (int j = 1; j <= M; ++j) {
        x[IX(0, j)]     = (b == 1) ? -x[IX(1, j)] : x[IX(1, j)];
        x[IX(N + 1, j)] = (b == 1) ? -x[IX(N, j)] : x[IX(N, j)];
    }
    // Corners
    x[IX(0, 0)]         = 0.5f * (x[IX(1, 0)]     + x[IX(0, 1)]);
    x[IX(0, M + 1)]     = 0.5f * (x[IX(1, M + 1)] + x[IX(0, M)]);
    x[IX(N + 1, 0)]     = 0.5f * (x[IX(N, 0)]     + x[IX(N + 1, 1)]);
    x[IX(N + 1, M + 1)] = 0.5f * (x[IX(N, M + 1)] + x[IX(N + 1, M)]);
}

void Fluid2DApp::diffuse(int b, float* x, const float* x0, float diff) {
    float a = DT * diff * N * M;
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
                         const float* u, const float* v) {
    float dt0x = DT * N;
    float dt0y = DT * M;

    for (int j = 1; j <= M; ++j) {
        for (int i = 1; i <= N; ++i) {
            // Backtrace
            float x = (float)i - dt0x * u[IX(i, j)];
            float y = (float)j - dt0y * v[IX(i, j)];

            // Clamp to grid interior
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

    // Compute divergence
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

    // Solve pressure Poisson equation
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

    // Subtract pressure gradient
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
    // ── Velocity step ──
    addSource(_velX, _velXPrev);
    addSource(_velY, _velYPrev);

    // Diffuse velocity
    std::swap(_velXPrev, _velX);
    diffuse(1, _velX, _velXPrev, _viscosity);
    std::swap(_velYPrev, _velY);
    diffuse(2, _velY, _velYPrev, _viscosity);

    // Project to make divergence-free
    project(_velX, _velY, _velXPrev, _velYPrev);

    // Advect velocity
    std::swap(_velXPrev, _velX);
    std::swap(_velYPrev, _velY);
    advect(1, _velX, _velXPrev, _velXPrev, _velYPrev);
    advect(2, _velY, _velYPrev, _velXPrev, _velYPrev);

    // Project again
    project(_velX, _velY, _velXPrev, _velYPrev);

    // ── Density step ──
    addSource(_dens, _densPrev);

    std::swap(_densPrev, _dens);
    diffuse(0, _dens, _densPrev, _diffusion);

    std::swap(_densPrev, _dens);
    advect(0, _dens, _densPrev, _velX, _velY);

    // Clear source arrays for next frame
    memset(_densPrev, 0, SIZE * sizeof(float));
    memset(_velXPrev, 0, SIZE * sizeof(float));
    memset(_velYPrev, 0, SIZE * sizeof(float));
}

// ══ Color Mapping ════════════════════════════════════════════════════════════

lv_color_t Fluid2DApp::densityToColor(float d) {
    // Heatmap: black → blue → cyan → green → yellow → red → white
    d = std::max(0.0f, std::min(d, 5.0f));
    float t = d / 5.0f;  // normalize to 0..1

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

lv_color_t Fluid2DApp::velocityToColor(float vx, float vy) {
    float mag = sqrtf(vx * vx + vy * vy);
    mag = std::min(mag / 5.0f, 1.0f);

    // Direction → hue, magnitude → brightness
    float angle = atan2f(vy, vx);  // -π..π
    float hue = (angle + 3.14159265f) / (2.0f * 3.14159265f);  // 0..1

    // HSV to RGB (S=1, V=mag)
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
    // Vorticity: blue (CW) ← 0 → red (CCW)
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
    // Use clip area to limit iteration
    const lv_area_t& clip = layer->_clip_area;

    for (int j = 1; j <= M; ++j) {
        int py = oy + (j - 1) * CELL_H;
        if (py + CELL_H < clip.y1 || py > clip.y2) continue;

        for (int i = 1; i <= N; ++i) {
            int px = ox + (i - 1) * CELL_W;
            if (px + CELL_W < clip.x1 || px > clip.x2) continue;

            int idx = IX(i, j);
            lv_color_t col;

            switch (app->_vizMode) {
                case VizMode::DENSITY:
                    col = densityToColor(app->_dens[idx]);
                    break;
                case VizMode::VELOCITY:
                    col = velocityToColor(app->_velX[idx], app->_velY[idx]);
                    break;
                case VizMode::VORTICITY: {
                    // Compute local vorticity (curl of velocity)
                    float w = 0.0f;
                    if (i > 1 && i < N && j > 1 && j < M) {
                        w = (app->_velY[IX(i+1,j)] - app->_velY[IX(i-1,j)]) -
                            (app->_velX[IX(i,j+1)] - app->_velX[IX(i,j-1)]);
                    }
                    col = vorticityToColor(w);
                    break;
                }
            }

            // Only draw non-black cells for performance
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

    // ── Velocity arrows overlay ──────────────────────────────────────────
    if (app->_showArrows) {
        lv_draw_line_dsc_t lineDsc;
        lv_draw_line_dsc_init(&lineDsc);
        lineDsc.color = lv_color_hex(0x80FF80);
        lineDsc.width = 1;
        lineDsc.opa   = LV_OPA_60;

        // Draw arrows every 4 cells
        for (int j = 2; j <= M; j += 4) {
            for (int i = 2; i <= N; i += 4) {
                int idx = IX(i, j);
                float vx = app->_velX[idx];
                float vy = app->_velY[idx];
                float mag = sqrtf(vx * vx + vy * vy);
                if (mag < 0.1f) continue;

                int px = ox + (i - 1) * CELL_W + CELL_W / 2;
                int arrowY = oy + (j - 1) * CELL_H + CELL_H / 2;

                // Scale arrow length
                float scale = std::min(mag, 5.0f) * 2.0f;
                int ex = px + (int)(vx / mag * scale);
                int ey = arrowY + (int)(vy / mag * scale);

                lineDsc.p1.x = px;
                lineDsc.p1.y = arrowY;
                lineDsc.p2.x = ex;
                lineDsc.p2.y = ey;
                lv_draw_line(layer, &lineDsc);
            }
        }
    }

    // ── Cursor ───────────────────────────────────────────────────────────
    {
        int cx = ox + (app->_cursorX - 1) * CELL_W;
        int cy = oy + (app->_cursorY - 1) * CELL_H;

        lv_draw_rect_dsc_t curDsc;
        lv_draw_rect_dsc_init(&curDsc);
        curDsc.bg_opa    = LV_OPA_TRANSP;
        curDsc.border_color = lv_color_hex(COL_CURSOR);
        curDsc.border_width = 1;
        curDsc.border_opa   = app->_emitting ? LV_OPA_COVER : LV_OPA_70;
        curDsc.radius       = 0;

        lv_area_t curArea = {
            (int16_t)(cx - 1), (int16_t)(cy - 1),
            (int16_t)(cx + CELL_W), (int16_t)(cy + CELL_H)
        };
        lv_draw_rect(layer, &curDsc, &curArea);
    }
}

// ══ Timer Callback ═══════════════════════════════════════════════════════════

void Fluid2DApp::onSimTimer(lv_timer_t* timer) {
    Fluid2DApp* app = static_cast<Fluid2DApp*>(lv_timer_get_user_data(timer));
    if (!app || !app->_screen || !app->_dens) return;

    if (!app->_paused) {
        app->addEmitterForces();
        app->fluidStep();
    }

    if (app->_drawObj) lv_obj_invalidate(app->_drawObj);
}
