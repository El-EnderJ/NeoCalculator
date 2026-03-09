/**
 * OpticsLabApp.h
 * OpticsLab — 2D Ray-Tracing Application for NumOS (App ID 17).
 *
 * LVGL-native app.  Uses OpticsEngine for physics and OpticsRenderer for
 * custom RGB565 canvas drawing.
 *
 * Layout (320 × 240):
 *   [  0 …  23]  StatusBar  (24 px — LVGL widget)
 *   [ 24 … 179]  Viewport   (156 px — ray-tracing canvas, part of drawObj)
 *   [180 … 239]  Telemetry  ( 60 px — ABCD results,       part of drawObj)
 *
 * Controls:
 *   UP / DOWN        — cycle selected element (Object, Lens1, Lens2, Screen)
 *   LEFT / RIGHT     — move selected element along X-axis (0.5 mm / step)
 *   ADD  / SUB       — increase / decrease R1 of selected lens (+/- 1 mm)
 *   MUL  / DIV       — increase R2 (+1 mm) / decrease n (-0.01) of lens
 *   F1               — toggle parameter: curvature mode ↔ index/thickness mode
 *   MODE             — return to main menu (handled by SystemApp)
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "OpticsEngine.h"

class OpticsLabApp {
public:
    OpticsLabApp();
    ~OpticsLabApp();

    // ── Standard app interface ────────────────────────────────────────
    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Screen layout ─────────────────────────────────────────────────
    static constexpr int SCREEN_W    = 320;
    static constexpr int SCREEN_H    = 240;
    static constexpr int STATUS_H    = 24;
    static constexpr int CANVAS_H    = SCREEN_H - STATUS_H;   // 216
    static constexpr int VIEWPORT_H  = 156;   // 180 - 24 (status bar)
    static constexpr int TELEMETRY_H = 60;
    static constexpr int AXIS_ROW    = VIEWPORT_H / 2;   // 78 in buffer coords

    // ── Colour helpers ────────────────────────────────────────────────
    static uint16_t rgb888to565(uint32_t rgb888);

    // ── LVGL objects ──────────────────────────────────────────────────
    lv_obj_t*       _screen   = nullptr;
    lv_obj_t*       _drawObj  = nullptr;   ///< image canvas
    lv_obj_t*       _teleLabel = nullptr;  ///< telemetry text
    lv_obj_t*       _infoLabel = nullptr;  ///< bottom info bar
    lv_timer_t*     _simTimer  = nullptr;
    ui::StatusBar   _statusBar;

    // ── Render buffer (RGB565, PSRAM) ─────────────────────────────────
    uint16_t*       _renderBuf = nullptr;  ///< SCREEN_W × CANVAS_H pixels
    lv_image_dsc_t  _imgDsc    = {};

    // ── Physics ───────────────────────────────────────────────────────
    OpticsEngine    _engine;

    // ── Scene / UI state ──────────────────────────────────────────────
    /// Selected element index into the logical list [Object, L1, L2, Screen]
    int  _selIdx     = 0;

    /// If true: ADD/SUB adjust R2 and MUL/DIV adjust n.
    /// If false: ADD/SUB adjust R1 and MUL/DIV adjust d.
    bool _altMode    = false;

    char _infoBuf[128] = {};

    // Logical element index mapping (filled during begin())
    int _objIdx    = -1;   ///< index in _engine for the Object element
    int _lensAIdx  = -1;   ///< index in _engine for Lens 1
    int _lensBIdx  = -1;   ///< index in _engine for Lens 2
    int _screenIdx = -1;   ///< index in _engine for the Screen element

    // ── Scene coordinate mapping ──────────────────────────────────────
    // Maps optical-axis mm → buffer pixel coordinates.
    float _sceneXMin = -40.0f;
    float _sceneXMax = 240.0f;
    float _scaleX    = 1.14f;   // px per mm (computed in begin())
    float _scaleY    = 2.6f;    // px per mm (VIEWPORT_H / Y_RANGE)

    int mmToPixX(float mm) const;
    int mmToPixY(float mm) const;  // y-offset from optical axis

    // ── Internal rendering ────────────────────────────────────────────
    void loadDefaultScene();
    void createUI();
    void renderToBuffer();
    void renderBackground();
    void renderAxis();
    void renderElements();
    void renderRays();
    void renderTelemetryBar();
    void updateInfoLabel();

    // ── LVGL callbacks ────────────────────────────────────────────────
    static void onDraw(lv_event_t* e);
    static void onSimTimer(lv_timer_t* timer);
};
