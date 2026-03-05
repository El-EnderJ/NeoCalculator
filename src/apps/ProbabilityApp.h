/**
 * ProbabilityApp.h — Probability Distributions application for NumOS.
 *
 * LVGL-native app:
 *   - Normal distribution N(μ, σ)
 *   - Bell curve visualization (lv_chart line)
 *   - PDF/CDF computation with area shading
 *   - Input fields for μ, σ, and x (boundary)
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "ProbEngine.h"

class ProbabilityApp {
public:
    ProbabilityApp();
    ~ProbabilityApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Screen dimensions ────────────────────────────────────────────────
    static constexpr int SCREEN_W     = 320;
    static constexpr int SCREEN_H     = 240;
    static constexpr int CHART_POINTS = 80;   ///< Resolution of bell curve

    // ── Focus targets ────────────────────────────────────────────────────
    enum class Focus : uint8_t { MU = 0, SIGMA = 1, X_VAL = 2 };
    static constexpr int NUM_FIELDS = 3;

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // Parameter input rows
    lv_obj_t*       _paramPanel;
    lv_obj_t*       _paramRows[NUM_FIELDS];
    lv_obj_t*       _paramLabels[NUM_FIELDS];
    lv_obj_t*       _paramValues[NUM_FIELDS];

    // Results
    lv_obj_t*       _resultPanel;
    lv_obj_t*       _pdfLabel;
    lv_obj_t*       _cdfLabel;

    // Chart (bell curve)
    lv_obj_t*       _chart;
    lv_chart_series_t* _bellSeries;
    lv_chart_series_t* _shadeSeries;   ///< Shaded area series

    // Hint
    lv_obj_t*       _hintLabel;

    // ── State ────────────────────────────────────────────────────────────
    Focus           _focus;
    bool            _editing;

    // Parameters
    double          _mu;
    double          _sigma;
    double          _xVal;

    // Edit buffer
    char            _editBuf[16];
    int             _editLen;

    // ── Builders ─────────────────────────────────────────────────────────
    void createUI();
    void createChart();

    // ── Updates ──────────────────────────────────────────────────────────
    void updateFocusStyle();
    void updateParamDisplay();
    void recompute();
    void updateBellCurve();

    // ── Edit mode ────────────────────────────────────────────────────────
    void startEdit();
    void finishEdit();
    void cancelEdit();
};
