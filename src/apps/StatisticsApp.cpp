/**
 * StatisticsApp.cpp — Statistics application for NumOS.
 *
 * Three-tab LVGL app: Data Entry, Results, Histogram.
 * Follows the same pattern as SettingsApp/GrapherApp.
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#include "StatisticsApp.h"
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
static constexpr uint32_t COL_CHART_BAR  = 0x4A90D9;

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

StatisticsApp::StatisticsApp()
    : _screen(nullptr)
    , _tabBar(nullptr)
    , _dataPanel(nullptr)
    , _table(nullptr)
    , _dataHint(nullptr)
    , _statsPanel(nullptr)
    , _graphPanel(nullptr)
    , _chart(nullptr)
    , _chartSeries(nullptr)
    , _activeTab(Tab::DATA)
    , _tableRow(0)
    , _tableCol(0)
    , _numRows(1)
    , _editing(false)
    , _editLen(0)
{
    memset(_editBuf, 0, sizeof(_editBuf));
    memset(_tabBtns, 0, sizeof(_tabBtns));
    memset(_tabLabels, 0, sizeof(_tabLabels));
    memset(_statLabels, 0, sizeof(_statLabels));

    for (int i = 0; i < MAX_ROWS; ++i) {
        _values[i] = 0.0;
        _freqs[i]  = 1.0;
    }
}

StatisticsApp::~StatisticsApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin — Create LVGL screen
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Statistics");

    createTabBar();
    createDataTab();
    createStatsTab();
    createGraphTab();

    switchTab(Tab::DATA);
}

// ════════════════════════════════════════════════════════════════════════════
// end — Destroy screen
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::end() {
    if (_screen) {
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen     = nullptr;
        _tabBar     = nullptr;
        _dataPanel  = nullptr;
        _table      = nullptr;
        _dataHint   = nullptr;
        _statsPanel = nullptr;
        _graphPanel = nullptr;
        _chart      = nullptr;
        _chartSeries = nullptr;
        memset(_tabBtns, 0, sizeof(_tabBtns));
        memset(_tabLabels, 0, sizeof(_tabLabels));
        memset(_statLabels, 0, sizeof(_statLabels));
    }
}

// ════════════════════════════════════════════════════════════════════════════
// load — Activate the screen
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Statistics");
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
// createTabBar — Manual tab buttons (like GrapherApp)
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::createTabBar() {
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

    const char* names[NUM_TABS] = { "Data", "Stats", "Graph" };
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
// createDataTab — Table with V1 (values) and N1 (frequency) columns
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::createDataTab() {
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
    lv_table_set_cell_value(_table, 0, 0, "V1 (Value)");
    lv_table_set_cell_value(_table, 0, 1, "N1 (Freq)");

    // Initialize data rows
    _numRows = 1;
    refreshTable();

    // Hint label at the bottom
    _dataHint = lv_label_create(_dataPanel);
    lv_label_set_text(_dataHint,
        LV_SYMBOL_UP LV_SYMBOL_DOWN " Nav  ENTER Edit  DEL Clear  AC New row");
    lv_obj_set_style_text_font(_dataHint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_dataHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_dataHint, 6, panelH - 18);
}

// ════════════════════════════════════════════════════════════════════════════
// createStatsTab — List of computed statistical results
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::createStatsTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _statsPanel = lv_obj_create(_screen);
    lv_obj_set_size(_statsPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_statsPanel, 0, topY);
    lv_obj_set_style_bg_opa(_statsPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_statsPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_statsPanel, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(_statsPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_statsPanel, LV_OBJ_FLAG_SCROLLABLE);

    const char* statNames[7] = {
        "Mean:",
        "Median:",
        "Std Dev:",
        "Min:",
        "Max:",
        "Sum:",
        "Count (n):"
    };

    for (int i = 0; i < 7; ++i) {
        int y = 6 + i * 24;

        // Name label (left)
        lv_obj_t* name = lv_label_create(_statsPanel);
        lv_label_set_text(name, statNames[i]);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(name, lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_set_pos(name, 10, y);

        // Value label (right)
        _statLabels[i] = lv_label_create(_statsPanel);
        lv_label_set_text(_statLabels[i], "---");
        lv_obj_set_style_text_font(_statLabels[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(_statLabels[i], lv_color_hex(0x1565C0), LV_PART_MAIN);
        lv_obj_set_pos(_statLabels[i], 180, y);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// createGraphTab — Histogram chart
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::createGraphTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _graphPanel = lv_obj_create(_screen);
    lv_obj_set_size(_graphPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_graphPanel, 0, topY);
    lv_obj_set_style_bg_opa(_graphPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_graphPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_graphPanel, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(_graphPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_graphPanel, LV_OBJ_FLAG_SCROLLABLE);

    _chart = lv_chart_create(_graphPanel);
    lv_obj_set_size(_chart, SCREEN_W - 24, panelH - 16);
    lv_obj_set_pos(_chart, 8, 4);
    lv_chart_set_type(_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_div_line_count(_chart, 4, 0);
    lv_chart_set_point_count(_chart, 1);

    _chartSeries = lv_chart_add_series(_chart, lv_color_hex(COL_CHART_BAR),
                                        LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_set_style_bg_color(_chart, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
    lv_obj_set_style_border_color(_chart, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_border_width(_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_chart, 4, LV_PART_MAIN);
}

// ════════════════════════════════════════════════════════════════════════════
// switchTab — Show/hide panels
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::switchTab(Tab t) {
    _activeTab = t;

    if (_dataPanel)  lv_obj_add_flag(_dataPanel,  LV_OBJ_FLAG_HIDDEN);
    if (_statsPanel) lv_obj_add_flag(_statsPanel, LV_OBJ_FLAG_HIDDEN);
    if (_graphPanel) lv_obj_add_flag(_graphPanel, LV_OBJ_FLAG_HIDDEN);

    switch (t) {
        case Tab::DATA:
            if (_dataPanel) lv_obj_remove_flag(_dataPanel, LV_OBJ_FLAG_HIDDEN);
            refreshTable();
            break;
        case Tab::STATS:
            if (_statsPanel) lv_obj_remove_flag(_statsPanel, LV_OBJ_FLAG_HIDDEN);
            recompute();
            updateStatsDisplay();
            break;
        case Tab::GRAPH:
            if (_graphPanel) lv_obj_remove_flag(_graphPanel, LV_OBJ_FLAG_HIDDEN);
            recompute();
            updateHistogram();
            break;
    }

    updateTabStyles();
}

void StatisticsApp::updateTabStyles() {
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
// refreshTable — Update table display from _values/_freqs arrays
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::refreshTable() {
    if (!_table) return;

    lv_table_set_row_count(_table, _numRows + 1);  // +1 for header

    // Header
    lv_table_set_cell_value(_table, 0, 0, "V1 (Value)");
    lv_table_set_cell_value(_table, 0, 1, "N1 (Freq)");

    char buf[16];
    for (int r = 0; r < _numRows; ++r) {
        // Value column
        if (_values[r] == 0.0 && r >= _numRows - 1) {
            lv_table_set_cell_value(_table, r + 1, 0, "");
        } else {
            snprintf(buf, sizeof(buf), "%.4g", _values[r]);
            lv_table_set_cell_value(_table, r + 1, 0, buf);
        }
        // Frequency column
        snprintf(buf, sizeof(buf), "%.4g", _freqs[r]);
        lv_table_set_cell_value(_table, r + 1, 1, buf);
    }

    highlightCell();
}

// ════════════════════════════════════════════════════════════════════════════
// highlightCell — Visual indicator for focused cell
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::highlightCell() {
    if (!_table) return;
    lv_table_set_selected_cell(_table, _tableRow + 1, _tableCol);
}

// ════════════════════════════════════════════════════════════════════════════
// Edit mode
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::startEdit() {
    _editing = true;
    _editLen = 0;
    _editBuf[0] = '\0';

    // Show current value in edit buffer
    double val = (_tableCol == 0) ? _values[_tableRow] : _freqs[_tableRow];
    if (val != 0.0 || _tableRow < _numRows - 1) {
        snprintf(_editBuf, sizeof(_editBuf), "%.4g", val);
        _editLen = strlen(_editBuf);
    }

    // Update the cell to show edit state
    char display[20];
    snprintf(display, sizeof(display), ">%s_", _editBuf);
    lv_table_set_cell_value(_table, _tableRow + 1, _tableCol, display);
}

void StatisticsApp::finishEdit() {
    if (!_editing) return;
    _editing = false;

    double val = 0.0;
    if (_editLen > 0) {
        val = atof(_editBuf);
    }

    if (_tableCol == 0) {
        _values[_tableRow] = val;
    } else {
        _freqs[_tableRow] = (val < 1.0) ? 1.0 : val;
    }

    refreshTable();
}

void StatisticsApp::cancelEdit() {
    _editing = false;
    refreshTable();
}

// ════════════════════════════════════════════════════════════════════════════
// recompute — Feed data into StatsEngine
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::recompute() {
    std::vector<StatsEngine::DataPoint> data;
    for (int i = 0; i < _numRows; ++i) {
        if (_values[i] != 0.0 || i < _numRows - 1 || _freqs[i] > 1.0) {
            data.push_back({_values[i], _freqs[i]});
        }
    }
    _engine.setData(data);
}

// ════════════════════════════════════════════════════════════════════════════
// updateStatsDisplay — Refresh the Stats tab
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::updateStatsDisplay() {
    if (_engine.data().empty()) {
        for (int i = 0; i < 7; ++i) {
            if (_statLabels[i]) lv_label_set_text(_statLabels[i], "---");
        }
        return;
    }

    char buf[32];
    auto fmt = [&](double v) {
        snprintf(buf, sizeof(buf), "%.6g", v);
        return buf;
    };

    if (_statLabels[0]) lv_label_set_text(_statLabels[0], fmt(_engine.getMean()));
    if (_statLabels[1]) lv_label_set_text(_statLabels[1], fmt(_engine.getMedian()));
    if (_statLabels[2]) lv_label_set_text(_statLabels[2], fmt(_engine.getStandardDeviation()));
    if (_statLabels[3]) lv_label_set_text(_statLabels[3], fmt(_engine.getMin()));
    if (_statLabels[4]) lv_label_set_text(_statLabels[4], fmt(_engine.getMax()));
    if (_statLabels[5]) lv_label_set_text(_statLabels[5], fmt(_engine.getSum()));

    snprintf(buf, sizeof(buf), "%zu", _engine.count());
    if (_statLabels[6]) lv_label_set_text(_statLabels[6], buf);
}

// ════════════════════════════════════════════════════════════════════════════
// updateHistogram — Refresh the histogram chart
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::updateHistogram() {
    if (!_chart || !_chartSeries) return;

    recompute();

    auto& data = _engine.data();
    int n = (int)data.size();
    if (n == 0) {
        lv_chart_set_point_count(_chart, 1);
        lv_chart_set_next_value(_chart, _chartSeries, 0);
        lv_chart_refresh(_chart);
        return;
    }

    lv_chart_set_point_count(_chart, n);

    // Find max frequency for Y axis range
    double maxFreq = 1;
    for (auto& dp : data) {
        if (dp.frequency > maxFreq) maxFreq = dp.frequency;
    }
    lv_chart_set_axis_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0,
                            (int32_t)(maxFreq * 1.1) + 1);

    // Set values
    for (int i = 0; i < n; ++i) {
        lv_chart_set_value_by_id(_chart, _chartSeries, i,
                                  (int32_t)data[i].frequency);
    }

    lv_chart_refresh(_chart);
}

// ════════════════════════════════════════════════════════════════════════════
// handleKey — Route to active tab handler
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // Tab switching with LEFT/RIGHT when not editing
    if (!_editing) {
        if (ev.code == KeyCode::F1 || (ev.code == KeyCode::LEFT && _activeTab != Tab::DATA)) {
            int t = static_cast<int>(_activeTab);
            if (t > 0) switchTab(static_cast<Tab>(t - 1));
            return;
        }
        if (ev.code == KeyCode::F2 || (ev.code == KeyCode::RIGHT && _activeTab != Tab::DATA)) {
            int t = static_cast<int>(_activeTab);
            if (t < NUM_TABS - 1) switchTab(static_cast<Tab>(t + 1));
            return;
        }
        // GRAPH key switches between tabs
        if (ev.code == KeyCode::GRAPH) {
            int t = (static_cast<int>(_activeTab) + 1) % NUM_TABS;
            switchTab(static_cast<Tab>(t));
            return;
        }
    }

    switch (_activeTab) {
        case Tab::DATA:  handleKeyData(ev);  break;
        case Tab::STATS: handleKeyStats(ev); break;
        case Tab::GRAPH: handleKeyGraph(ev); break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyData — Table editing
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::handleKeyData(const KeyEvent& ev) {
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
            if (_tableCol == 0) _values[_tableRow] = 0.0;
            else _freqs[_tableRow] = 1.0;
            refreshTable();
            break;
        case KeyCode::AC:
            // Add new row
            if (_numRows < MAX_ROWS) {
                _values[_numRows] = 0.0;
                _freqs[_numRows]  = 1.0;
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
// handleKeyStats — Results tab (read-only)
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::handleKeyStats(const KeyEvent& ev) {
    // Stats tab is read-only — no special keys needed
    (void)ev;
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyGraph — Histogram tab (read-only)
// ════════════════════════════════════════════════════════════════════════════

void StatisticsApp::handleKeyGraph(const KeyEvent& ev) {
    // Graph tab is read-only — no special keys needed
    (void)ev;
}
