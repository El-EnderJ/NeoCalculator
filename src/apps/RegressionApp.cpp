/**
 * RegressionApp.cpp — Regression analysis application for NumOS.
 *
 * Three-tab LVGL app: Data Entry, Equation, Graph (scatter + curve).
 * Follows the same pattern as StatisticsApp.
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#include "RegressionApp.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ══ Color palette ════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0xFFFFFF;
static constexpr uint32_t COL_TAB_BG     = 0xF0F0F0;
static constexpr uint32_t COL_TAB_ACTIVE = 0x4A90D9;
static constexpr uint32_t COL_TAB_TEXT   = 0x333333;
static constexpr uint32_t COL_TAB_TEXT_A = 0xFFFFFF;
static constexpr uint32_t COL_CELL_FOCUS = 0xE3F2FD;
static constexpr uint32_t COL_CELL_EDIT  = 0xFFF9C4;
static constexpr uint32_t COL_TEXT       = 0x1A1A1A;
static constexpr uint32_t COL_HINT       = 0x808080;
static constexpr uint32_t COL_HEADER_BG  = 0xE0E0E0;
static constexpr uint32_t COL_GRAPH_BG   = 0xFAFAFA;
static constexpr uint32_t COL_GRID       = 0xD0D0D0;
static constexpr uint32_t COL_AXIS       = 0x555555;
static constexpr uint32_t COL_SCATTER    = 0xE53935;   // Red dots
static constexpr uint32_t COL_CURVE_LIN  = 0x1565C0;   // Blue line
static constexpr uint32_t COL_CURVE_QUAD = 0x6A1B9A;   // Purple curve

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

RegressionApp::RegressionApp()
    : _screen(nullptr)
    , _tabBar(nullptr)
    , _dataPanel(nullptr)
    , _table(nullptr)
    , _dataHint(nullptr)
    , _eqPanel(nullptr)
    , _eqModelLabel(nullptr)
    , _graphPanel(nullptr)
    , _graphCanvas(nullptr)
    , _activeTab(Tab::DATA)
    , _tableRow(0)
    , _tableCol(0)
    , _numRows(1)
    , _editing(false)
    , _editLen(0)
    , _model(RegressionEngine::Model::LINEAR)
{
    memset(_editBuf, 0, sizeof(_editBuf));
    memset(_tabBtns, 0, sizeof(_tabBtns));
    memset(_tabLabels, 0, sizeof(_tabLabels));
    memset(_eqLabels, 0, sizeof(_eqLabels));

    for (int i = 0; i < MAX_ROWS; ++i) {
        _xData[i] = 0.0;
        _yData[i] = 0.0;
    }

    _result = { false, RegressionEngine::Model::LINEAR, 0, 0, 0, 0 };
}

RegressionApp::~RegressionApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin — Create LVGL screen
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Regression");

    createTabBar();
    createDataTab();
    createEquationTab();
    createGraphTab();

    switchTab(Tab::DATA);
}

// ════════════════════════════════════════════════════════════════════════════
// end — Destroy screen
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::end() {
    if (_screen) {
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen      = nullptr;
        _tabBar      = nullptr;
        _dataPanel   = nullptr;
        _table       = nullptr;
        _dataHint    = nullptr;
        _eqPanel     = nullptr;
        _eqModelLabel = nullptr;
        _graphPanel  = nullptr;
        _graphCanvas = nullptr;
        memset(_tabBtns, 0, sizeof(_tabBtns));
        memset(_tabLabels, 0, sizeof(_tabLabels));
        memset(_eqLabels, 0, sizeof(_eqLabels));
    }
}

// ════════════════════════════════════════════════════════════════════════════
// load — Activate the screen
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Regression");
    _statusBar.update();
    _activeTab = Tab::DATA;
    _tableRow  = 0;
    _tableCol  = 0;
    _editing   = false;
    switchTab(Tab::DATA);
    refreshTable();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ════════════════════════════════════════════════════════════════════════════
// createTabBar — Manual tab buttons (like StatisticsApp)
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::createTabBar() {
    int topY = ui::StatusBar::HEIGHT + 1;

    _tabBar = lv_obj_create(_screen);
    lv_obj_set_size(_tabBar, SCREEN_W, TAB_BAR_H);
    lv_obj_set_pos(_tabBar, 0, topY);
    lv_obj_set_style_bg_color(_tabBar, lv_color_hex(COL_TAB_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tabBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tabBar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_tabBar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_tabBar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tabBar, LV_OBJ_FLAG_SCROLLABLE);

    const char* names[NUM_TABS] = { "Data", "Equation", "Graph" };
    int tabW = SCREEN_W / NUM_TABS;

    for (int i = 0; i < NUM_TABS; ++i) {
        _tabBtns[i] = lv_obj_create(_tabBar);
        lv_obj_set_size(_tabBtns[i], tabW - 4, TAB_BAR_H - 4);
        lv_obj_set_pos(_tabBtns[i], i * tabW + 2, 2);
        lv_obj_set_style_radius(_tabBtns[i], 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_tabBtns[i], LV_OBJ_FLAG_SCROLLABLE);

        _tabLabels[i] = lv_label_create(_tabBtns[i]);
        lv_label_set_text(_tabLabels[i], names[i]);
        lv_obj_set_style_text_font(_tabLabels[i], &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_align(_tabLabels[i], LV_ALIGN_CENTER, 0, 0);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// createDataTab — Table with X and Y columns
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::createDataTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _dataPanel = lv_obj_create(_screen);
    lv_obj_set_size(_dataPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_dataPanel, 0, topY);
    lv_obj_set_style_bg_opa(_dataPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_dataPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_dataPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_dataPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_dataPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Table
    _table = lv_table_create(_dataPanel);
    lv_table_set_column_count(_table, 2);
    lv_table_set_column_width(_table, 0, 150);
    lv_table_set_column_width(_table, 1, 150);
    lv_obj_set_size(_table, SCREEN_W - 8, panelH - 24);
    lv_obj_set_pos(_table, 4, 2);
    lv_obj_set_style_text_font(_table, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_border_width(_table, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_table, lv_color_hex(0xD0D0D0), LV_PART_MAIN);

    // Header row
    lv_table_set_cell_value(_table, 0, 0, "X");
    lv_table_set_cell_value(_table, 0, 1, "Y");

    // Initialize data rows
    _numRows = 1;
    refreshTable();

    // Hint label at the bottom
    _dataHint = lv_label_create(_dataPanel);
    lv_label_set_text(_dataHint,
        LV_SYMBOL_UP LV_SYMBOL_DOWN " Nav  ENTER Edit  AC New row");
    lv_obj_set_style_text_font(_dataHint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_dataHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_dataHint, 6, panelH - 18);
}

// ════════════════════════════════════════════════════════════════════════════
// createEquationTab — Display regression results
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::createEquationTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _eqPanel = lv_obj_create(_screen);
    lv_obj_set_size(_eqPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_eqPanel, 0, topY);
    lv_obj_set_style_bg_opa(_eqPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_eqPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_eqPanel, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(_eqPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_eqPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Model label (toggle with SHIFT)
    _eqModelLabel = lv_label_create(_eqPanel);
    lv_label_set_text(_eqModelLabel, "Model: Linear  (SHIFT to toggle)");
    lv_obj_set_style_text_font(_eqModelLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_eqModelLabel, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_eqModelLabel, 8, 6);

    // Equation results
    const char* names[EQ_LABEL_COUNT] = {
        "Equation:",
        "Slope (m) / a:",
        "Intercept (b):",
        "c:",
        "R\xC2\xB2:",       // R² (UTF-8)
        "Points (n):"
    };

    for (int i = 0; i < EQ_LABEL_COUNT; ++i) {
        int y = 30 + i * 24;

        // Name label (left)
        lv_obj_t* name = lv_label_create(_eqPanel);
        lv_label_set_text(name, names[i]);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(name, lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_set_pos(name, 10, y);

        // Value label (right)
        _eqLabels[i] = lv_label_create(_eqPanel);
        lv_label_set_text(_eqLabels[i], "---");
        lv_obj_set_style_text_font(_eqLabels[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(_eqLabels[i], lv_color_hex(0x1565C0), LV_PART_MAIN);
        lv_obj_set_pos(_eqLabels[i], 150, y);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// createGraphTab — Scatter plot + regression curve
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::createGraphTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _graphPanel = lv_obj_create(_screen);
    lv_obj_set_size(_graphPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_graphPanel, 0, topY);
    lv_obj_set_style_bg_opa(_graphPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_graphPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_graphPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_graphPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_graphPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Graph drawing area (we use an lv_obj with draw event)
    _graphCanvas = lv_obj_create(_graphPanel);
    lv_obj_set_size(_graphCanvas, SCREEN_W - 16, panelH - 8);
    lv_obj_set_pos(_graphCanvas, 8, 4);
    lv_obj_set_style_bg_color(_graphCanvas, lv_color_hex(COL_GRAPH_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_graphCanvas, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_graphCanvas, lv_color_hex(COL_GRID), LV_PART_MAIN);
    lv_obj_set_style_border_width(_graphCanvas, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_graphCanvas, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_graphCanvas, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_graphCanvas, LV_OBJ_FLAG_SCROLLABLE);
}

// ════════════════════════════════════════════════════════════════════════════
// switchTab — Show/hide panels
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::switchTab(Tab t) {
    _activeTab = t;

    if (_dataPanel)  lv_obj_add_flag(_dataPanel,  LV_OBJ_FLAG_HIDDEN);
    if (_eqPanel)    lv_obj_add_flag(_eqPanel,    LV_OBJ_FLAG_HIDDEN);
    if (_graphPanel) lv_obj_add_flag(_graphPanel, LV_OBJ_FLAG_HIDDEN);

    switch (t) {
        case Tab::DATA:
            if (_dataPanel) lv_obj_remove_flag(_dataPanel, LV_OBJ_FLAG_HIDDEN);
            refreshTable();
            break;
        case Tab::EQUATION:
            if (_eqPanel) lv_obj_remove_flag(_eqPanel, LV_OBJ_FLAG_HIDDEN);
            recompute();
            updateEquationDisplay();
            break;
        case Tab::GRAPH:
            if (_graphPanel) lv_obj_remove_flag(_graphPanel, LV_OBJ_FLAG_HIDDEN);
            recompute();
            updateGraph();
            break;
    }

    updateTabStyles();
}

void RegressionApp::updateTabStyles() {
    for (int i = 0; i < NUM_TABS; ++i) {
        if (!_tabBtns[i]) continue;
        if (i == static_cast<int>(_activeTab)) {
            lv_obj_set_style_bg_color(_tabBtns[i], lv_color_hex(COL_TAB_ACTIVE), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tabBtns[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(_tabLabels[i], lv_color_hex(COL_TAB_TEXT_A), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_tabBtns[i], lv_color_hex(COL_TAB_BG), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tabBtns[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(_tabLabels[i], lv_color_hex(COL_TAB_TEXT), LV_PART_MAIN);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// refreshTable — Update table display from _xData/_yData arrays
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::refreshTable() {
    if (!_table) return;

    lv_table_set_row_count(_table, _numRows + 1);  // +1 for header

    // Header
    lv_table_set_cell_value(_table, 0, 0, "X");
    lv_table_set_cell_value(_table, 0, 1, "Y");

    char buf[16];
    for (int r = 0; r < _numRows; ++r) {
        // X column
        if (_xData[r] == 0.0 && _yData[r] == 0.0 && r >= _numRows - 1) {
            lv_table_set_cell_value(_table, r + 1, 0, "");
            lv_table_set_cell_value(_table, r + 1, 1, "");
        } else {
            snprintf(buf, sizeof(buf), "%.4g", _xData[r]);
            lv_table_set_cell_value(_table, r + 1, 0, buf);
            snprintf(buf, sizeof(buf), "%.4g", _yData[r]);
            lv_table_set_cell_value(_table, r + 1, 1, buf);
        }
    }

    highlightCell();
}

// ════════════════════════════════════════════════════════════════════════════
// highlightCell — Visual indicator for focused cell
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::highlightCell() {
    if (!_table) return;
    lv_table_set_selected_cell(_table, _tableRow + 1, _tableCol);
}

// ════════════════════════════════════════════════════════════════════════════
// Edit mode
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::startEdit() {
    _editing = true;
    _editLen = 0;
    _editBuf[0] = '\0';

    // Show current value in edit buffer
    double val = (_tableCol == 0) ? _xData[_tableRow] : _yData[_tableRow];
    bool hasData = (_xData[_tableRow] != 0.0 || _yData[_tableRow] != 0.0 || _tableRow < _numRows - 1);
    if (val != 0.0 || hasData) {
        snprintf(_editBuf, sizeof(_editBuf), "%.4g", val);
        _editLen = strlen(_editBuf);
    }

    // Update the cell to show edit state
    char display[20];
    snprintf(display, sizeof(display), ">%s_", _editBuf);
    lv_table_set_cell_value(_table, _tableRow + 1, _tableCol, display);
}

void RegressionApp::finishEdit() {
    if (!_editing) return;
    _editing = false;

    double val = 0.0;
    if (_editLen > 0) {
        val = atof(_editBuf);
    }

    if (_tableCol == 0) {
        _xData[_tableRow] = val;
    } else {
        _yData[_tableRow] = val;
    }

    refreshTable();
}

void RegressionApp::cancelEdit() {
    _editing = false;
    refreshTable();
}

// ════════════════════════════════════════════════════════════════════════════
// recompute — Run regression on current data
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::recompute() {
    // Gather valid data points
    RegressionEngine::Point pts[MAX_ROWS];
    int n = 0;
    for (int i = 0; i < _numRows; ++i) {
        // Include points that have at least one non-zero value, or are not the last empty row
        if (_xData[i] != 0.0 || _yData[i] != 0.0 || i < _numRows - 1) {
            pts[n].x = _xData[i];
            pts[n].y = _yData[i];
            ++n;
        }
    }

    if (_model == RegressionEngine::Model::LINEAR) {
        _result = RegressionEngine::fitLinear(pts, n);
    } else {
        _result = RegressionEngine::fitQuadratic(pts, n);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// updateEquationDisplay — Refresh the Equation tab
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::updateEquationDisplay() {
    if (_eqModelLabel) {
        if (_model == RegressionEngine::Model::LINEAR) {
            lv_label_set_text(_eqModelLabel, "Model: Linear  (SHIFT to toggle)");
        } else {
            lv_label_set_text(_eqModelLabel, "Model: Quadratic  (SHIFT to toggle)");
        }
    }

    if (!_result.ok) {
        for (int i = 0; i < EQ_LABEL_COUNT; ++i) {
            if (_eqLabels[i]) lv_label_set_text(_eqLabels[i], "---");
        }
        return;
    }

    char buf[64];

    // Equation string
    if (_model == RegressionEngine::Model::LINEAR) {
        snprintf(buf, sizeof(buf), "y = %.4gx + %.4g", _result.a, _result.b);
    } else {
        snprintf(buf, sizeof(buf), "y = %.4gx\xC2\xB2 + %.4gx + %.4g",
                 _result.a, _result.b, _result.c);
    }
    if (_eqLabels[0]) lv_label_set_text(_eqLabels[0], buf);

    // Coefficients
    if (_model == RegressionEngine::Model::LINEAR) {
        snprintf(buf, sizeof(buf), "%.6g", _result.a);
        if (_eqLabels[1]) lv_label_set_text(_eqLabels[1], buf);

        snprintf(buf, sizeof(buf), "%.6g", _result.b);
        if (_eqLabels[2]) lv_label_set_text(_eqLabels[2], buf);

        if (_eqLabels[3]) lv_label_set_text(_eqLabels[3], "N/A");
    } else {
        snprintf(buf, sizeof(buf), "%.6g", _result.a);
        if (_eqLabels[1]) lv_label_set_text(_eqLabels[1], buf);

        snprintf(buf, sizeof(buf), "%.6g", _result.b);
        if (_eqLabels[2]) lv_label_set_text(_eqLabels[2], buf);

        snprintf(buf, sizeof(buf), "%.6g", _result.c);
        if (_eqLabels[3]) lv_label_set_text(_eqLabels[3], buf);
    }

    // R²
    snprintf(buf, sizeof(buf), "%.6g", _result.r2);
    if (_eqLabels[4]) lv_label_set_text(_eqLabels[4], buf);

    // Point count
    int nPts = 0;
    for (int i = 0; i < _numRows; ++i) {
        if (_xData[i] != 0.0 || _yData[i] != 0.0 || i < _numRows - 1)
            ++nPts;
    }
    snprintf(buf, sizeof(buf), "%d", nPts);
    if (_eqLabels[5]) lv_label_set_text(_eqLabels[5], buf);
}

// ════════════════════════════════════════════════════════════════════════════
// updateGraph — Render scatter plot + regression curve using LVGL line objects
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::updateGraph() {
    if (!_graphCanvas) return;

    // Clear previous children (old lines/points)
    lv_obj_clean(_graphCanvas);

    int w = lv_obj_get_width(_graphCanvas);
    int h = lv_obj_get_height(_graphCanvas);
    if (w < 10 || h < 10) return;

    // Gather valid data points
    int nPts = 0;
    double xMin = 1e30, xMax = -1e30, yMin = 1e30, yMax = -1e30;

    for (int i = 0; i < _numRows; ++i) {
        if (_xData[i] != 0.0 || _yData[i] != 0.0 || i < _numRows - 1) {
            ++nPts;
            if (_xData[i] < xMin) xMin = _xData[i];
            if (_xData[i] > xMax) xMax = _xData[i];
            if (_yData[i] < yMin) yMin = _yData[i];
            if (_yData[i] > yMax) yMax = _yData[i];
        }
    }

    if (nPts == 0) return;

    // Add margin
    double xRange = xMax - xMin;
    double yRange = yMax - yMin;
    if (xRange < 1e-9) xRange = 2.0;
    if (yRange < 1e-9) yRange = 2.0;
    xMin -= xRange * 0.15;
    xMax += xRange * 0.15;
    yMin -= yRange * 0.15;
    yMax += yRange * 0.15;

    // Also include regression curve extremes if available
    if (_result.ok && _model == RegressionEngine::Model::QUADRATIC) {
        for (int px = 0; px <= 20; ++px) {
            double xVal = xMin + (xMax - xMin) * px / 20.0;
            double yVal = RegressionEngine::evaluate(_result, xVal);
            if (yVal < yMin) yMin = yVal - yRange * 0.1;
            if (yVal > yMax) yMax = yVal + yRange * 0.1;
        }
    }

    drawAxes(_graphCanvas, w, h, xMin, xMax, yMin, yMax);
    drawScatter(_graphCanvas, w, h, xMin, xMax, yMin, yMax);
    if (_result.ok) {
        drawCurve(_graphCanvas, w, h, xMin, xMax, yMin, yMax);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawAxes — Grid and axis lines
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::drawAxes(lv_obj_t* parent, int w, int h,
                              double xMin, double xMax, double yMin, double yMax) {
    double xRange = xMax - xMin;
    double yRange = yMax - yMin;
    if (xRange < 1e-15 || yRange < 1e-15) return;

    // Draw X axis (y=0) if visible
    if (yMin <= 0 && yMax >= 0) {
        int yPx = (int)((1.0 - (0 - yMin) / yRange) * (h - 1));
        if (yPx >= 0 && yPx < h) {
            static lv_point_precise_t axPts[2];
            axPts[0] = {0, (lv_value_precise_t)yPx};
            axPts[1] = {(lv_value_precise_t)(w - 1), (lv_value_precise_t)yPx};
            lv_obj_t* line = lv_line_create(parent);
            lv_line_set_points(line, axPts, 2);
            lv_obj_set_style_line_color(line, lv_color_hex(COL_AXIS), LV_PART_MAIN);
            lv_obj_set_style_line_width(line, 1, LV_PART_MAIN);
        }
    }

    // Draw Y axis (x=0) if visible
    if (xMin <= 0 && xMax >= 0) {
        int xPx = (int)(((0 - xMin) / xRange) * (w - 1));
        if (xPx >= 0 && xPx < w) {
            static lv_point_precise_t ayPts[2];
            ayPts[0] = {(lv_value_precise_t)xPx, 0};
            ayPts[1] = {(lv_value_precise_t)xPx, (lv_value_precise_t)(h - 1)};
            lv_obj_t* line = lv_line_create(parent);
            lv_line_set_points(line, ayPts, 2);
            lv_obj_set_style_line_color(line, lv_color_hex(COL_AXIS), LV_PART_MAIN);
            lv_obj_set_style_line_width(line, 1, LV_PART_MAIN);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawScatter — Data points as small crosses
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::drawScatter(lv_obj_t* parent, int w, int h,
                                 double xMin, double xMax, double yMin, double yMax) {
    double xRange = xMax - xMin;
    double yRange = yMax - yMin;
    if (xRange < 1e-15 || yRange < 1e-15) return;

    // We draw each point as a small filled circle (4px radius)
    for (int i = 0; i < _numRows; ++i) {
        if (_xData[i] == 0.0 && _yData[i] == 0.0 && i >= _numRows - 1) continue;

        int px = (int)((_xData[i] - xMin) / xRange * (w - 1));
        int py = (int)((1.0 - (_yData[i] - yMin) / yRange) * (h - 1));

        // Create a small dot (circle)
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_pos(dot, px - 4, py - 4);
        lv_obj_set_style_bg_color(dot, lv_color_hex(COL_SCATTER), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawCurve — Regression line/curve using lv_line segments
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::drawCurve(lv_obj_t* parent, int w, int h,
                               double xMin, double xMax, double yMin, double yMax) {
    if (!_result.ok) return;

    double xRange = xMax - xMin;
    double yRange = yMax - yMin;
    if (xRange < 1e-15 || yRange < 1e-15) return;

    // Number of segments for drawing the curve
    static constexpr int NUM_PTS = 60;
    static lv_point_precise_t curvePts[NUM_PTS];
    int validPts = 0;

    for (int i = 0; i < NUM_PTS; ++i) {
        double xVal = xMin + xRange * i / (NUM_PTS - 1);
        double yVal = RegressionEngine::evaluate(_result, xVal);

        int px = (int)((xVal - xMin) / xRange * (w - 1));
        int py = (int)((1.0 - (yVal - yMin) / yRange) * (h - 1));

        // Clamp to canvas bounds
        if (py < -h) py = -h;
        if (py > 2 * h) py = 2 * h;

        curvePts[validPts] = {(lv_value_precise_t)px, (lv_value_precise_t)py};
        ++validPts;
    }

    if (validPts >= 2) {
        lv_obj_t* line = lv_line_create(parent);
        lv_line_set_points(line, curvePts, validPts);
        uint32_t curveColor = (_model == RegressionEngine::Model::LINEAR)
                              ? COL_CURVE_LIN : COL_CURVE_QUAD;
        lv_obj_set_style_line_color(line, lv_color_hex(curveColor), LV_PART_MAIN);
        lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKey — Route to active tab handler
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // Tab switching with F1/F2 when not editing
    if (!_editing) {
        if (ev.code == KeyCode::F1) {
            int t = static_cast<int>(_activeTab);
            if (t > 0) switchTab(static_cast<Tab>(t - 1));
            return;
        }
        if (ev.code == KeyCode::F2) {
            int t = static_cast<int>(_activeTab);
            if (t < NUM_TABS - 1) switchTab(static_cast<Tab>(t + 1));
            return;
        }
        // GRAPH key cycles tabs
        if (ev.code == KeyCode::GRAPH) {
            int t = (static_cast<int>(_activeTab) + 1) % NUM_TABS;
            switchTab(static_cast<Tab>(t));
            return;
        }
    }

    switch (_activeTab) {
        case Tab::DATA:     handleKeyData(ev);     break;
        case Tab::EQUATION: handleKeyEquation(ev); break;
        case Tab::GRAPH:    handleKeyGraph(ev);    break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyData — Table editing (same pattern as StatisticsApp)
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::handleKeyData(const KeyEvent& ev) {
    if (_editing) {
        // Numeric input
        bool isDigit = (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9);
        if (isDigit && _editLen < 14) {
            _editBuf[_editLen++] = '0' + (static_cast<int>(ev.code) - static_cast<int>(KeyCode::NUM_0));
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_table, _tableRow + 1, _tableCol, display);
            return;
        }
        if (ev.code == KeyCode::DOT && _editLen < 14) {
            _editBuf[_editLen++] = '.';
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_table, _tableRow + 1, _tableCol, display);
            return;
        }
        if (ev.code == KeyCode::SUB && _editLen == 0) {
            _editBuf[_editLen++] = '-';
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_table, _tableRow + 1, _tableCol, display);
            return;
        }
        if (ev.code == KeyCode::DEL && _editLen > 0) {
            _editBuf[--_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_table, _tableRow + 1, _tableCol, display);
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
        case KeyCode::UP:
            if (_tableRow > 0) {
                --_tableRow;
                highlightCell();
            }
            break;
        case KeyCode::DOWN:
            if (_tableRow < _numRows - 1) {
                ++_tableRow;
                highlightCell();
            }
            break;
        case KeyCode::LEFT:
            if (_tableCol > 0) {
                --_tableCol;
                highlightCell();
            }
            break;
        case KeyCode::RIGHT:
            if (_tableCol < 1) {
                ++_tableCol;
                highlightCell();
            }
            break;
        case KeyCode::ENTER:
            startEdit();
            break;
        case KeyCode::DEL:
            // Clear current cell
            if (_tableCol == 0) _xData[_tableRow] = 0.0;
            else _yData[_tableRow] = 0.0;
            refreshTable();
            break;
        case KeyCode::AC:
            // Add new row
            if (_numRows < MAX_ROWS) {
                _xData[_numRows] = 0.0;
                _yData[_numRows] = 0.0;
                ++_numRows;
                refreshTable();
            }
            break;
        default:
            // Direct digit input starts editing
            if (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9) {
                startEdit();
                handleKeyData(ev);  // Re-process the digit
            }
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyEquation — Equation tab (toggle model with SHIFT)
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::handleKeyEquation(const KeyEvent& ev) {
    if (ev.code == KeyCode::SHIFT) {
        // Toggle model
        _model = (_model == RegressionEngine::Model::LINEAR)
                 ? RegressionEngine::Model::QUADRATIC
                 : RegressionEngine::Model::LINEAR;
        recompute();
        updateEquationDisplay();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyGraph — Graph tab (read-only, but can toggle model)
// ════════════════════════════════════════════════════════════════════════════

void RegressionApp::handleKeyGraph(const KeyEvent& ev) {
    if (ev.code == KeyCode::SHIFT) {
        _model = (_model == RegressionEngine::Model::LINEAR)
                 ? RegressionEngine::Model::QUADRATIC
                 : RegressionEngine::Model::LINEAR;
        recompute();
        updateGraph();
    }
}
