/**
 * OpticsLabApp.cpp
 * OpticsLab — 2D Ray-Tracing Application for NumOS (App ID 17).
 *
 * Dual-engine optics visualiser:
 *   Engine A: Exact ray tracing with vector Snell's Law + TIR detection.
 *   Engine B: Paraxial ABCD matrices → EFL, BFL, magnification.
 *
 * Rendering: RGB565 PSRAM buffer → lv_image, redrawn at ~30 FPS.
 * UI:        StatusBar + custom canvas + telemetry strip.
 */

#include "OpticsLabApp.h"
#include "OpticsRenderer.h"
#include <cstdio>
#include <cstring>
#include <cmath>

#ifdef ARDUINO
#  include <esp_heap_caps.h>
#else
#  include <new>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Memory helpers (PSRAM on device, heap on desktop)
// ═══════════════════════════════════════════════════════════════════════════

static uint16_t* optAllocBuf(size_t count) {
#ifdef ARDUINO
    return (uint16_t*)heap_caps_malloc(count * sizeof(uint16_t),
                                       MALLOC_CAP_SPIRAM);
#else
    return new (std::nothrow) uint16_t[count];
#endif
}

static void optFreeBuf(uint16_t* p) {
#ifdef ARDUINO
    if (p) heap_caps_free(p);
#else
    delete[] p;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════

OpticsLabApp::OpticsLabApp()
    : _screen(nullptr), _drawObj(nullptr), _teleLabel(nullptr),
      _infoLabel(nullptr), _simTimer(nullptr),
      _renderBuf(nullptr), _selIdx(0), _altMode(false)
{
    memset(&_imgDsc, 0, sizeof(_imgDsc));
    _infoBuf[0] = '\0';
}

OpticsLabApp::~OpticsLabApp() {
    end();
}

// ═══════════════════════════════════════════════════════════════════════════
// begin() — called once on first load()
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::begin() {
    if (_screen) return;

    // ── Allocate PSRAM render buffer ─────────────────────────────────
    _renderBuf = optAllocBuf((size_t)SCREEN_W * CANVAS_H);
    if (!_renderBuf) return;
    memset(_renderBuf, 0, (size_t)SCREEN_W * CANVAS_H * sizeof(uint16_t));

    // ── Set up lv_image_dsc_t (RGB565, SCREEN_W × CANVAS_H) ──────────
    _imgDsc.header.w  = SCREEN_W;
    _imgDsc.header.h  = CANVAS_H;
    _imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _imgDsc.data_size = (uint32_t)(SCREEN_W * CANVAS_H * 2);
    _imgDsc.data      = (const uint8_t*)_renderBuf;

    // ── Build default optical scene ───────────────────────────────────
    loadDefaultScene();

    // ── Create LVGL screen and UI ─────────────────────────────────────
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);

    _statusBar.create(_screen);
    _statusBar.setTitle("OpticsLab");

    createUI();

    // ── Initial render ─────────────────────────────────────────────────
    _engine.traceRays();
    _engine.computeParaxial();
    renderToBuffer();
    updateInfoLabel();

    if (_drawObj) {
        lv_image_set_src(_drawObj, &_imgDsc);
        lv_obj_invalidate(_drawObj);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// end() — called 250 ms after returning to menu
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::end() {
    if (!_screen) return;

    if (_simTimer) {
        lv_timer_delete(_simTimer);
        _simTimer = nullptr;
    }

    lv_obj_delete(_screen);
    _screen     = nullptr;
    _drawObj    = nullptr;
    _teleLabel  = nullptr;
    _infoLabel  = nullptr;

    _statusBar.resetPointers();

    if (_renderBuf) {
        optFreeBuf(_renderBuf);
        _renderBuf = nullptr;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// load() — called every time the app is launched from the menu
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::load() {
    if (!_screen) begin();
    if (!_screen) return;  // allocation failure

    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ═══════════════════════════════════════════════════════════════════════════
// loadDefaultScene()
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::loadDefaultScene() {
    _engine.resetScene();

    // Object (source of rays)
    auto obj = OpticalElement::makeObject(0.0f);
    obj.halfHeight = 8.0f;
    _engine.addElement(obj);

    // Lens 1 — biconvex, f ≈ 40 mm
    auto lens1 = OpticalElement::makeLens(
        /*posX=*/80.0f,
        /*R1=*/  40.0f, /*R2=*/ -40.0f,
        /*d=*/   5.0f,  /*n=*/   1.5f,  /*h=*/14.0f
    );
    _engine.addElement(lens1);

    // Lens 2 — biconvex, f ≈ 40 mm
    auto lens2 = OpticalElement::makeLens(
        /*posX=*/160.0f,
        /*R1=*/  40.0f, /*R2=*/ -40.0f,
        /*d=*/   5.0f,  /*n=*/   1.5f,  /*h=*/14.0f
    );
    _engine.addElement(lens2);

    // Screen
    auto screen = OpticalElement::makeScreen(210.0f);
    _engine.addElement(screen);

    // Object position for paraxial calcs
    _engine.setObjectX(0.0f);
    _engine.setObjectHeight(8.0f);

    // Scene bounds
    _sceneXMin = -40.0f;
    _sceneXMax = 240.0f;

    // Compute scales
    _scaleX = (float)SCREEN_W / (_sceneXMax - _sceneXMin);  // px/mm
    _scaleY = (float)VIEWPORT_H / 60.0f;                    // px/mm for ±30 mm range

    // Identify element indices in the sorted engine list
    // After addElement+sortElements, the order is: Object(x=0), L1(x=80), L2(x=160), Screen(x=210)
    _objIdx = _lensAIdx = _lensBIdx = _screenIdx = -1;
    int lensCount = 0;
    for (int i = 0; i < _engine.elementCount(); ++i) {
        auto& e = _engine.element(i);
        switch (e.type) {
            case ElementType::OBJECT: _objIdx    = i; break;
            case ElementType::SCREEN: _screenIdx = i; break;
            case ElementType::LENS:
                if (lensCount == 0) { _lensAIdx = i; ++lensCount; }
                else                { _lensBIdx = i; ++lensCount; }
                break;
        }
    }

    _selIdx = 0;  // start on Object
}

// ═══════════════════════════════════════════════════════════════════════════
// createUI()
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::createUI() {
    // ── Main canvas (below status bar, full remaining height) ─────────
    _drawObj = lv_image_create(_screen);
    lv_obj_set_pos(_drawObj, 0, STATUS_H);
    lv_obj_set_size(_drawObj, SCREEN_W, CANVAS_H);
    lv_image_set_src(_drawObj, &_imgDsc);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN_END, this);

    // ── Telemetry label (LVGL text overlay inside telemetry region) ───
    // We render telemetry into the buffer directly, so no extra LVGL label
    // is needed for the numbers.  The _infoLabel at the very bottom shows
    // key-binding hints.
    _infoLabel = lv_label_create(_screen);
    lv_obj_align(_infoLabel, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_infoLabel,
                                 lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_bg_opa(_infoLabel, LV_OPA_TRANSP, 0);
    lv_label_set_text(_infoLabel, "UP/DN:sel  LR:move  +/-:R  */÷:n  F1:mode");

    // ── Simulation timer (~30 FPS) ────────────────────────────────────
    _simTimer = lv_timer_create(onSimTimer, 33, this);
}

// ═══════════════════════════════════════════════════════════════════════════
// handleKey()
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::handleKey(const KeyEvent& ev) {
    // Logical element list indices (in _engine order)
    const int logicalMap[4] = { _objIdx, _lensAIdx, _lensBIdx, _screenIdx };

    switch (ev.code) {
        // ── Cycle selected element ─────────────────────────────────
        case KeyCode::UP:
            _selIdx = (_selIdx + 3) % 4;
            updateInfoLabel();
            return;
        case KeyCode::DOWN:
            _selIdx = (_selIdx + 1) % 4;
            updateInfoLabel();
            return;

        // ── Move selected element along X ──────────────────────────
        case KeyCode::LEFT: {
            int idx = logicalMap[_selIdx];
            if (idx >= 0) {
                _engine.element(idx).posX -= 0.5f;
                if (_engine.element(idx).type == ElementType::OBJECT)
                    _engine.setObjectX(_engine.element(idx).posX);
            }
            break;
        }
        case KeyCode::RIGHT: {
            int idx = logicalMap[_selIdx];
            if (idx >= 0) {
                _engine.element(idx).posX += 0.5f;
                if (_engine.element(idx).type == ElementType::OBJECT)
                    _engine.setObjectX(_engine.element(idx).posX);
            }
            break;
        }

        // ── Adjust curvature / index / thickness ───────────────────
        case KeyCode::ADD: {
            int idx = logicalMap[_selIdx];
            if (idx >= 0 && _engine.element(idx).type == ElementType::LENS) {
                if (!_altMode)
                    _engine.element(idx).R1 += 1.0f;
                else
                    _engine.element(idx).R2 += 1.0f;
            }
            break;
        }
        case KeyCode::SUB: {
            int idx = logicalMap[_selIdx];
            if (idx >= 0 && _engine.element(idx).type == ElementType::LENS) {
                if (!_altMode)
                    _engine.element(idx).R1 -= 1.0f;
                else
                    _engine.element(idx).R2 -= 1.0f;
            }
            break;
        }
        case KeyCode::MUL: {
            int idx = logicalMap[_selIdx];
            if (idx >= 0 && _engine.element(idx).type == ElementType::LENS) {
                if (!_altMode)
                    _engine.element(idx).n += 0.01f;
                else
                    _engine.element(idx).thickness += 0.5f;
            }
            break;
        }
        case KeyCode::DIV: {
            int idx = logicalMap[_selIdx];
            if (idx >= 0 && _engine.element(idx).type == ElementType::LENS) {
                if (!_altMode) {
                    _engine.element(idx).n -= 0.01f;
                    if (_engine.element(idx).n < 1.01f)
                        _engine.element(idx).n = 1.01f;
                } else {
                    _engine.element(idx).thickness -= 0.5f;
                    if (_engine.element(idx).thickness < 0.5f)
                        _engine.element(idx).thickness = 0.5f;
                }
            }
            break;
        }

        // ── Toggle parameter mode ─────────────────────────────────
        case KeyCode::F1:
            _altMode = !_altMode;
            updateInfoLabel();
            return;

        default:
            return;
    }

    // Recompute and redraw
    _engine.traceRays();
    _engine.computeParaxial();
    renderToBuffer();
    updateInfoLabel();
    if (_drawObj) {
        lv_image_set_src(_drawObj, &_imgDsc);
        lv_obj_invalidate(_drawObj);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Coordinate helpers
// ═══════════════════════════════════════════════════════════════════════════

int OpticsLabApp::mmToPixX(float mm) const {
    return (int)((mm - _sceneXMin) * _scaleX + 0.5f);
}

int OpticsLabApp::mmToPixY(float mm) const {
    // Positive y_mm → above axis → smaller row index
    return AXIS_ROW - (int)(mm * _scaleY + 0.5f);
}

uint16_t OpticsLabApp::rgb888to565(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// ═══════════════════════════════════════════════════════════════════════════
// renderToBuffer() — master render function
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::renderToBuffer() {
    if (!_renderBuf) return;
    renderBackground();
    renderAxis();
    renderElements();
    renderRays();
    renderTelemetryBar();
}

// ───────────────────────────────────────────────────────────────────────────
// renderBackground()
// ───────────────────────────────────────────────────────────────────────────
void OpticsLabApp::renderBackground() {
    // Viewport: deep black
    optFillRect(_renderBuf, 0, 0, SCREEN_W, VIEWPORT_H,
                optRGB(OPT_COL_BG));
    // Telemetry: very dark grey
    optFillRect(_renderBuf, 0, VIEWPORT_H, SCREEN_W, TELEMETRY_H,
                optRGB(OPT_COL_TELE_BG));
}

// ───────────────────────────────────────────────────────────────────────────
// renderAxis()
// ───────────────────────────────────────────────────────────────────────────
void OpticsLabApp::renderAxis() {
    // Dotted optical axis across the full viewport width
    uint16_t axisCol = optRGB(OPT_COL_AXIS);
    optDrawDotted(_renderBuf, 0, AXIS_ROW, SCREEN_W - 1, AXIS_ROW,
                  axisCol, 8);
    // Separator between viewport and telemetry
    optHLine(_renderBuf, VIEWPORT_H, 0, SCREEN_W - 1,
             optRGB(0x2A2A2A));
}

// ───────────────────────────────────────────────────────────────────────────
// renderElements()
// ───────────────────────────────────────────────────────────────────────────
void OpticsLabApp::renderElements() {
    const int logicalMap[4] = { _objIdx, _lensAIdx, _lensBIdx, _screenIdx };

    for (int i = 0; i < _engine.elementCount(); ++i) {
        const OpticalElement& el = _engine.element(i);
        int xPx = mmToPixX(el.posX);
        bool sel = false;
        for (int k = 0; k < 4; ++k) {
            if (logicalMap[k] == i && _selIdx == k) { sel = true; break; }
        }

        switch (el.type) {
            case ElementType::OBJECT:
                optDrawObjectArrow(_renderBuf, xPx, AXIS_ROW,
                                   _engine.getObjectHeight(),
                                   _scaleY, sel);
                break;

            case ElementType::LENS: {
                int thickPx = (int)(el.thickness * _scaleX);
                if (thickPx < 3) thickPx = 3;
                optDrawLens(_renderBuf, xPx, AXIS_ROW,
                            el.R1, el.R2, el.halfHeight,
                            _scaleY, (float)thickPx, sel);
                break;
            }

            case ElementType::SCREEN:
                optDrawScreen(_renderBuf, xPx, AXIS_ROW,
                              el.halfHeight, _scaleY, sel);
                break;
        }
    }

    // ── Draw principal planes (H1, H2) if paraxial data is valid ──────
    const ParaxialData& pd = _engine.paraxial();
    if (pd.valid) {
        uint16_t hCol = optRGB(OPT_COL_HPLANE);
        int xH1 = mmToPixX(pd.H1_abs);
        int xH2 = mmToPixX(pd.H2_abs);
        int hSpan = (int)(12.0f * _scaleY);  // short tick ±12 mm
        // H1
        optVLine(_renderBuf, xH1, AXIS_ROW - hSpan, AXIS_ROW + hSpan, hCol);
        optPutPixel(_renderBuf, xH1 - 1, AXIS_ROW, hCol);
        optPutPixel(_renderBuf, xH1 + 1, AXIS_ROW, hCol);
        // H2
        optVLine(_renderBuf, xH2, AXIS_ROW - hSpan, AXIS_ROW + hSpan, hCol);
        optPutPixel(_renderBuf, xH2 - 1, AXIS_ROW, hCol);
        optPutPixel(_renderBuf, xH2 + 1, AXIS_ROW, hCol);
    }
}

// ───────────────────────────────────────────────────────────────────────────
// renderRays()
// ───────────────────────────────────────────────────────────────────────────
void OpticsLabApp::renderRays() {
    for (int ri = 0; ri < _engine.tracedRayCount(); ++ri) {
        const TracedRay& tr = _engine.tracedRay(ri);

        // Assign a slight hue variation per ray index for aesthetics
        // Centre ray: pure gold; outer rays: progressively dimmer
        int total = _engine.tracedRayCount();
        float frac = (total > 1)
                     ? (float)ri / (total - 1)
                     : 0.5f;  // 0 = top, 1 = bottom
        float dist_from_centre = fabsf(frac - 0.5f) * 2.0f;  // 0…1

        // Colour: golden-yellow to orange, dimmed for off-axis rays
        uint8_t r_col = 255;
        uint8_t g_col = (uint8_t)(215 - (int)(dist_from_centre * 130));
        uint8_t b_col = 0;

        uint16_t rayCol    = optRGB(r_col, g_col, b_col);
        uint16_t tirCol    = optRGB(0xFF, 0x20, 0x20);

        for (int si = 0; si < tr.count; ++si) {
            const RaySegment& seg = tr.segments[si];
            uint16_t col = seg.tir ? tirCol : rayCol;

            int x0 = mmToPixX(seg.start.x);
            int y0 = mmToPixY(seg.start.y);
            int x1 = mmToPixX(seg.end.x);
            int y1 = mmToPixY(seg.end.y);

            // Only draw segments within the viewport rows
            // (clamping is handled inside optDrawLine/optPutPixel)
            optDrawLine(_renderBuf, x0, y0, x1, y1, col);

            // If TIR: draw a small "X" glyph at the termination point
            if (seg.tir) {
                for (int d = -2; d <= 2; ++d) {
                    optPutPixel(_renderBuf, x1 + d, y1 + d, tirCol);
                    optPutPixel(_renderBuf, x1 - d, y1 + d, tirCol);
                }
            }
        }
    }
}

// ───────────────────────────────────────────────────────────────────────────
// renderTelemetryBar()  — draw ABCD results into the telemetry region
// ───────────────────────────────────────────────────────────────────────────
void OpticsLabApp::renderTelemetryBar() {
    // We use the LVGL label (_teleLabel) for text rendering.
    // All we do here is ensure the background colour is set.
    // (The actual text is updated by updateInfoLabel below.)
    // Nothing extra to draw in the buffer for this region beyond the BG fill.
    (void)this;
}

// ───────────────────────────────────────────────────────────────────────────
// updateInfoLabel()  — update bottom text overlay
// ───────────────────────────────────────────────────────────────────────────
void OpticsLabApp::updateInfoLabel() {
    if (!_infoLabel) return;

    const ParaxialData& pd = _engine.paraxial();
    const int logicalMap[4] = { _objIdx, _lensAIdx, _lensBIdx, _screenIdx };
    const char* elemNames[4] = { "Object", "Lens 1", "Lens 2", "Screen" };

    const char* selName = elemNames[_selIdx];
    int idx = logicalMap[_selIdx];

    if (pd.valid) {
        float efl = pd.efl;
        float bfl = pd.bfl;
        float mag = pd.magnification;
        if (idx >= 0 && _engine.element(idx).type == ElementType::LENS) {
            auto& el = _engine.element(idx);
            snprintf(_infoBuf, sizeof(_infoBuf),
                     "Sel:%s R1=%.0f R2=%.0f n=%.2f d=%.1f | "
                     "EFL=%.1f BFL=%.1f M=%.2f",
                     selName, (double)el.R1, (double)el.R2,
                     (double)el.n, (double)el.thickness,
                     (double)efl, (double)bfl, (double)mag);
        } else {
            snprintf(_infoBuf, sizeof(_infoBuf),
                     "Sel:%s x=%.1f | EFL=%.1f BFL=%.1f M=%.2f",
                     selName,
                     (idx >= 0 ? (double)_engine.element(idx).posX : 0.0),
                     (double)efl, (double)bfl, (double)mag);
        }
    } else {
        if (idx >= 0 && _engine.element(idx).type == ElementType::LENS) {
            auto& el = _engine.element(idx);
            snprintf(_infoBuf, sizeof(_infoBuf),
                     "Sel:%s R1=%.0f R2=%.0f n=%.2f d=%.1f | EFL:N/A",
                     selName,
                     (double)el.R1, (double)el.R2,
                     (double)el.n, (double)el.thickness);
        } else {
            snprintf(_infoBuf, sizeof(_infoBuf),
                     "Sel:%s x=%.1f | %s",
                     selName,
                     (idx >= 0 ? (double)_engine.element(idx).posX : 0.0),
                     _altMode ? "[R2/n mode]" : "[R1/d mode]");
        }
    }

    lv_label_set_text(_infoLabel, _infoBuf);
}

// ═══════════════════════════════════════════════════════════════════════════
// LVGL callbacks
// ═══════════════════════════════════════════════════════════════════════════

void OpticsLabApp::onDraw(lv_event_t* e) {
    (void)e; // rendering is driven by the timer
}

void OpticsLabApp::onSimTimer(lv_timer_t* timer) {
    OpticsLabApp* app = (OpticsLabApp*)lv_timer_get_user_data(timer);
    if (!app || !app->_screen) return;

    // Re-render (scene may not have changed, but this keeps the StatusBar
    // clock ticking and ensures any deferred invalidations are flushed).
    app->renderToBuffer();
    app->_statusBar.update();

    if (app->_drawObj) {
        lv_image_set_src(app->_drawObj, &app->_imgDsc);
        lv_obj_invalidate(app->_drawObj);
    }
}
