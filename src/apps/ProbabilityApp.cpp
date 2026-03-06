/**
 * ProbabilityApp.cpp — Probability Distributions application for NumOS.
 *
 * Gaussian bell curve with PDF/CDF computation.
 * Uses lv_chart (line type) for the bell curve visualization.
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#include "ProbabilityApp.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ══ Color palette ════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0xFFFFFF;
static constexpr uint32_t COL_ROW_BG     = 0xF5F5F5;
static constexpr uint32_t COL_ROW_FOCUS  = 0xE3F2FD;
static constexpr uint32_t COL_BORDER     = 0xD0D0D0;
static constexpr uint32_t COL_FOCUS_BD   = 0x4A90D9;
static constexpr uint32_t COL_TEXT       = 0x1A1A1A;
static constexpr uint32_t COL_VALUE      = 0x1565C0;
static constexpr uint32_t COL_HINT       = 0x808080;
static constexpr uint32_t COL_BELL       = 0x4A90D9;   // Blue bell curve
static constexpr uint32_t COL_SHADE      = 0x90CAF9;   // Light blue shading
static constexpr uint32_t COL_EDIT_BG    = 0xFFF9C4;
static constexpr uint32_t COL_RESULT     = 0x2E7D32;

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

ProbabilityApp::ProbabilityApp()
    : _screen(nullptr)
    , _paramPanel(nullptr)
    , _resultPanel(nullptr)
    , _pdfLabel(nullptr)
    , _cdfLabel(nullptr)
    , _chart(nullptr)
    , _bellSeries(nullptr)
    , _shadeSeries(nullptr)
    , _hintLabel(nullptr)
    , _focus(Focus::MU)
    , _editing(false)
    , _mu(0.0)
    , _sigma(1.0)
    , _xVal(0.0)
    , _editLen(0)
{
    memset(_editBuf, 0, sizeof(_editBuf));
    memset(_paramRows, 0, sizeof(_paramRows));
    memset(_paramLabels, 0, sizeof(_paramLabels));
    memset(_paramValues, 0, sizeof(_paramValues));
}

ProbabilityApp::~ProbabilityApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin — Create LVGL screen
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Probability");

    createUI();
    createChart();
}

// ════════════════════════════════════════════════════════════════════════════
// end — Destroy screen
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::end() {
    if (_screen) {
        _statusBar.destroy();
        lv_obj_del(_screen);
        _screen      = nullptr;
        _paramPanel  = nullptr;
        _resultPanel = nullptr;
        _pdfLabel    = nullptr;
        _cdfLabel    = nullptr;
        _chart       = nullptr;
        _bellSeries  = nullptr;
        _shadeSeries = nullptr;
        _hintLabel   = nullptr;
        memset(_paramRows, 0, sizeof(_paramRows));
        memset(_paramLabels, 0, sizeof(_paramLabels));
        memset(_paramValues, 0, sizeof(_paramValues));
    }
}

// ════════════════════════════════════════════════════════════════════════════
// load — Activate screen
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Probability");
    _statusBar.update();
    _focus   = Focus::MU;
    _editing = false;
    updateParamDisplay();
    updateFocusStyle();
    recompute();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ════════════════════════════════════════════════════════════════════════════
// createUI — Parameter inputs and result labels
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::createUI() {
    int topY = ui::StatusBar::HEIGHT + 1;

    // Parameter panel (left side)
    _paramPanel = lv_obj_create(_screen);
    lv_obj_set_size(_paramPanel, 140, 120);
    lv_obj_set_pos(_paramPanel, 0, topY);
    lv_obj_set_style_bg_opa(_paramPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_paramPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_paramPanel, 4, LV_PART_MAIN);
    lv_obj_remove_flag(_paramPanel, LV_OBJ_FLAG_SCROLLABLE);

    const char* labels[NUM_FIELDS] = { "mu:", "sigma:", "x:" };

    for (int i = 0; i < NUM_FIELDS; ++i) {
        int y = 4 + i * 36;

        _paramRows[i] = lv_obj_create(_paramPanel);
        lv_obj_set_size(_paramRows[i], 128, 30);
        lv_obj_set_pos(_paramRows[i], 4, y);
        lv_obj_set_style_bg_color(_paramRows[i], lv_color_hex(COL_ROW_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_paramRows[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(_paramRows[i], lv_color_hex(COL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(_paramRows[i], 1, LV_PART_MAIN);
        lv_obj_set_style_radius(_paramRows[i], 4, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_paramRows[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_paramRows[i], LV_OBJ_FLAG_SCROLLABLE);

        _paramLabels[i] = lv_label_create(_paramRows[i]);
        lv_label_set_text(_paramLabels[i], labels[i]);
        lv_obj_set_style_text_font(_paramLabels[i], &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(_paramLabels[i], lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_align(_paramLabels[i], LV_ALIGN_LEFT_MID, 6, 0);

        _paramValues[i] = lv_label_create(_paramRows[i]);
        lv_obj_set_style_text_font(_paramValues[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(_paramValues[i], lv_color_hex(COL_VALUE), LV_PART_MAIN);
        lv_obj_align(_paramValues[i], LV_ALIGN_RIGHT_MID, -6, 0);
    }

    // Result panel (below params)
    _resultPanel = lv_obj_create(_screen);
    lv_obj_set_size(_resultPanel, 140, 60);
    lv_obj_set_pos(_resultPanel, 0, topY + 124);
    lv_obj_set_style_bg_opa(_resultPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_resultPanel, 4, LV_PART_MAIN);
    lv_obj_remove_flag(_resultPanel, LV_OBJ_FLAG_SCROLLABLE);

    _pdfLabel = lv_label_create(_resultPanel);
    lv_label_set_text(_pdfLabel, "PDF: ---");
    lv_obj_set_style_text_font(_pdfLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_pdfLabel, lv_color_hex(COL_RESULT), LV_PART_MAIN);
    lv_obj_set_pos(_pdfLabel, 8, 4);

    _cdfLabel = lv_label_create(_resultPanel);
    lv_label_set_text(_cdfLabel, "P(X<x): ---");
    lv_obj_set_style_text_font(_cdfLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_cdfLabel, lv_color_hex(COL_RESULT), LV_PART_MAIN);
    lv_obj_set_pos(_cdfLabel, 8, 24);

    // Hint at very bottom
    _hintLabel = lv_label_create(_screen);
    lv_label_set_text(_hintLabel,
        LV_SYMBOL_UP LV_SYMBOL_DOWN " Select  ENTER Edit  MODE Back");
    lv_obj_set_style_text_font(_hintLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_hintLabel, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_hintLabel, 8, SCREEN_H - 16);
}

// ════════════════════════════════════════════════════════════════════════════
// createChart — Bell curve using lv_chart (line type)
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::createChart() {
    int topY = ui::StatusBar::HEIGHT + 1;

    _chart = lv_chart_create(_screen);
    lv_obj_set_size(_chart, 168, 170);
    lv_obj_set_pos(_chart, 146, topY + 4);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(_chart, CHART_POINTS);
    lv_chart_set_div_line_count(_chart, 3, 0);

    lv_obj_set_style_bg_color(_chart, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
    lv_obj_set_style_border_color(_chart, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_chart, 4, LV_PART_MAIN);
    lv_obj_set_style_size(_chart, 0, 0, LV_PART_INDICATOR);  // Hide point markers

    // Shaded area series (drawn first, underneath)
    _shadeSeries = lv_chart_add_series(_chart, lv_color_hex(COL_SHADE),
                                        LV_CHART_AXIS_PRIMARY_Y);

    // Bell curve series (drawn on top)
    _bellSeries = lv_chart_add_series(_chart, lv_color_hex(COL_BELL),
                                       LV_CHART_AXIS_PRIMARY_Y);

    // Set line width
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS);

    updateBellCurve();
}

// ════════════════════════════════════════════════════════════════════════════
// updateFocusStyle — Highlight the focused parameter row
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::updateFocusStyle() {
    for (int i = 0; i < NUM_FIELDS; ++i) {
        if (!_paramRows[i]) continue;
        if (i == static_cast<int>(_focus)) {
            lv_obj_set_style_bg_color(_paramRows[i], lv_color_hex(COL_ROW_FOCUS), LV_PART_MAIN);
            lv_obj_set_style_border_color(_paramRows[i], lv_color_hex(COL_FOCUS_BD), LV_PART_MAIN);
            lv_obj_set_style_border_width(_paramRows[i], 2, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_paramRows[i], lv_color_hex(COL_ROW_BG), LV_PART_MAIN);
            lv_obj_set_style_border_color(_paramRows[i], lv_color_hex(COL_BORDER), LV_PART_MAIN);
            lv_obj_set_style_border_width(_paramRows[i], 1, LV_PART_MAIN);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// updateParamDisplay — Show current parameter values
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::updateParamDisplay() {
    char buf[16];

    snprintf(buf, sizeof(buf), "%.4g", _mu);
    if (_paramValues[0]) lv_label_set_text(_paramValues[0], buf);

    snprintf(buf, sizeof(buf), "%.4g", _sigma);
    if (_paramValues[1]) lv_label_set_text(_paramValues[1], buf);

    snprintf(buf, sizeof(buf), "%.4g", _xVal);
    if (_paramValues[2]) lv_label_set_text(_paramValues[2], buf);
}

// ════════════════════════════════════════════════════════════════════════════
// recompute — Calculate PDF/CDF and update display
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::recompute() {
    if (_sigma <= 0.0) {
        if (_pdfLabel) lv_label_set_text(_pdfLabel, "PDF: invalid");
        if (_cdfLabel) lv_label_set_text(_cdfLabel, "P(X<x): invalid");
        return;
    }

    double pdf = ProbEngine::normalPDF(_xVal, _mu, _sigma);
    double cdf = ProbEngine::normalCDF(_xVal, _mu, _sigma);

    char buf[32];
    snprintf(buf, sizeof(buf), "PDF: %.6f", pdf);
    if (_pdfLabel) lv_label_set_text(_pdfLabel, buf);

    snprintf(buf, sizeof(buf), "P(X<x): %.6f", cdf);
    if (_cdfLabel) lv_label_set_text(_cdfLabel, buf);

    updateBellCurve();
}

// ════════════════════════════════════════════════════════════════════════════
// updateBellCurve — Redraw the bell curve with shading
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::updateBellCurve() {
    if (!_chart || !_bellSeries || !_shadeSeries) return;
    if (_sigma <= 0.0) return;

    // Range: μ ± 4σ
    double xMin = _mu - 4.0 * _sigma;
    double xMax = _mu + 4.0 * _sigma;
    double step = (xMax - xMin) / (CHART_POINTS - 1);

    // Generate PDF values
    double yValues[CHART_POINTS];
    ProbEngine::generatePDFCurve(_mu, _sigma, xMin, xMax, yValues, CHART_POINTS);

    // Find max for Y axis scaling (multiply by 1000 for integer precision)
    double maxY = 0;
    for (int i = 0; i < CHART_POINTS; ++i) {
        if (yValues[i] > maxY) maxY = yValues[i];
    }

    int32_t scale = 1000;
    lv_chart_set_axis_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0,
                            (int32_t)(maxY * scale) + 1);

    // Set bell curve points and shade
    for (int i = 0; i < CHART_POINTS; ++i) {
        double x = xMin + i * step;
        int32_t y = (int32_t)(yValues[i] * scale);

        lv_chart_set_value_by_id(_chart, _bellSeries, i, y);

        // Shade: if x <= _xVal, show the PDF value; otherwise 0
        if (x <= _xVal) {
            lv_chart_set_value_by_id(_chart, _shadeSeries, i, y);
        } else {
            lv_chart_set_value_by_id(_chart, _shadeSeries, i, 0);
        }
    }

    lv_chart_refresh(_chart);
}

// ════════════════════════════════════════════════════════════════════════════
// Edit mode
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::startEdit() {
    _editing = true;
    _editLen = 0;
    _editBuf[0] = '\0';

    double val = 0;
    switch (_focus) {
        case Focus::MU:    val = _mu;    break;
        case Focus::SIGMA: val = _sigma; break;
        case Focus::X_VAL: val = _xVal;  break;
    }

    snprintf(_editBuf, sizeof(_editBuf), "%.4g", val);
    _editLen = strlen(_editBuf);

    // Show edit indicator
    int idx = static_cast<int>(_focus);
    if (_paramRows[idx]) {
        lv_obj_set_style_bg_color(_paramRows[idx], lv_color_hex(COL_EDIT_BG), LV_PART_MAIN);
    }
    char display[20];
    snprintf(display, sizeof(display), ">%s_", _editBuf);
    if (_paramValues[idx]) lv_label_set_text(_paramValues[idx], display);
}

void ProbabilityApp::finishEdit() {
    if (!_editing) return;
    _editing = false;

    double val = atof(_editBuf);
    switch (_focus) {
        case Focus::MU:    _mu    = val; break;
        case Focus::SIGMA: _sigma = (val <= 0.0) ? 0.1 : val; break;
        case Focus::X_VAL: _xVal  = val; break;
    }

    updateParamDisplay();
    updateFocusStyle();
    recompute();
}

void ProbabilityApp::cancelEdit() {
    _editing = false;
    updateParamDisplay();
    updateFocusStyle();
}

// ════════════════════════════════════════════════════════════════════════════
// handleKey — Main key handler
// ════════════════════════════════════════════════════════════════════════════

void ProbabilityApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    if (_editing) {
        bool isDigit = (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9);
        if (isDigit && _editLen < 14) {
            _editBuf[_editLen++] = '0' + (static_cast<int>(ev.code) - static_cast<int>(KeyCode::NUM_0));
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            int idx = static_cast<int>(_focus);
            if (_paramValues[idx]) lv_label_set_text(_paramValues[idx], display);
            return;
        }
        if (ev.code == KeyCode::DOT && _editLen < 14) {
            _editBuf[_editLen++] = '.';
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            int idx = static_cast<int>(_focus);
            if (_paramValues[idx]) lv_label_set_text(_paramValues[idx], display);
            return;
        }
        if (ev.code == KeyCode::SUB && _editLen == 0) {
            _editBuf[_editLen++] = '-';
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            int idx = static_cast<int>(_focus);
            if (_paramValues[idx]) lv_label_set_text(_paramValues[idx], display);
            return;
        }
        if (ev.code == KeyCode::DEL && _editLen > 0) {
            _editBuf[--_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            int idx = static_cast<int>(_focus);
            if (_paramValues[idx]) lv_label_set_text(_paramValues[idx], display);
            return;
        }
        if (ev.code == KeyCode::ENTER) {
            finishEdit();
            return;
        }
        if (ev.code == KeyCode::AC) {
            cancelEdit();
            return;
        }
        return;
    }

    // Navigation mode
    switch (ev.code) {
        case KeyCode::UP: {
            int f = static_cast<int>(_focus);
            if (f > 0) {
                _focus = static_cast<Focus>(f - 1);
                updateFocusStyle();
            }
            break;
        }
        case KeyCode::DOWN: {
            int f = static_cast<int>(_focus);
            if (f < NUM_FIELDS - 1) {
                _focus = static_cast<Focus>(f + 1);
                updateFocusStyle();
            }
            break;
        }
        case KeyCode::ENTER:
            startEdit();
            break;
        case KeyCode::LEFT:
            // Quick decrement
            switch (_focus) {
                case Focus::MU:    _mu    -= 0.5; break;
                case Focus::SIGMA: _sigma  = (_sigma > 0.5) ? _sigma - 0.5 : 0.1; break;
                case Focus::X_VAL: _xVal  -= 0.5; break;
            }
            updateParamDisplay();
            recompute();
            break;
        case KeyCode::RIGHT:
            // Quick increment
            switch (_focus) {
                case Focus::MU:    _mu    += 0.5; break;
                case Focus::SIGMA: _sigma += 0.5; break;
                case Focus::X_VAL: _xVal  += 0.5; break;
            }
            updateParamDisplay();
            recompute();
            break;
        default:
            // Direct digit input starts editing
            if (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9) {
                startEdit();
                handleKey(ev);  // Re-process the digit
            }
            break;
    }
}
