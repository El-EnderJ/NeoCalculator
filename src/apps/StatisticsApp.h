/**
 * StatisticsApp.h — Statistics application for NumOS.
 *
 * LVGL-native app with 3 tabs (manual, like GrapherApp):
 *   Tab 0 — Data: lv_table editor (V1 values, N1 frequencies)
 *   Tab 1 — Stats: lv_list with computed parameters
 *   Tab 2 — Graph: lv_chart histogram of the data
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "StatsEngine.h"

class StatisticsApp {
public:
    StatisticsApp();
    ~StatisticsApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Tabs ─────────────────────────────────────────────────────────────
    enum class Tab : uint8_t { DATA = 0, STATS = 1, GRAPH = 2 };
    static constexpr int NUM_TABS = 3;

    // ── Screen dimensions ────────────────────────────────────────────────
    static constexpr int SCREEN_W  = 320;
    static constexpr int SCREEN_H  = 240;
    static constexpr int TAB_BAR_H = 28;
    static constexpr int MAX_ROWS  = 20;

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // Tab bar
    lv_obj_t*       _tabBar;
    lv_obj_t*       _tabBtns[NUM_TABS];
    lv_obj_t*       _tabLabels[NUM_TABS];

    // Tab 0: Data editor
    lv_obj_t*       _dataPanel;
    lv_obj_t*       _table;
    lv_obj_t*       _dataHint;

    // Tab 1: Results
    lv_obj_t*       _statsPanel;
    lv_obj_t*       _statLabels[7];   // mean, median, stddev, min, max, sum, n

    // Tab 2: Histogram
    lv_obj_t*       _graphPanel;
    lv_obj_t*       _chart;
    lv_chart_series_t* _chartSeries;

    // ── State ────────────────────────────────────────────────────────────
    Tab             _activeTab;
    int             _tableRow;        // Focused row in the table (0-based data row)
    int             _tableCol;        // 0 = Value, 1 = Frequency
    int             _numRows;         // Number of data rows used
    bool            _editing;         // Currently editing a cell

    // Cell edit buffer
    char            _editBuf[16];
    int             _editLen;

    // Data storage
    double          _values[MAX_ROWS];
    double          _freqs[MAX_ROWS];

    // Engine
    StatsEngine     _engine;

    // ── Builders ─────────────────────────────────────────────────────────
    void createTabBar();
    void createDataTab();
    void createStatsTab();
    void createGraphTab();

    // ── Tab switching ────────────────────────────────────────────────────
    void switchTab(Tab t);
    void updateTabStyles();

    // ── Table helpers ────────────────────────────────────────────────────
    void refreshTable();
    void highlightCell();
    void startEdit();
    void finishEdit();
    void cancelEdit();

    // ── Computation ──────────────────────────────────────────────────────
    void recompute();
    void updateStatsDisplay();
    void updateHistogram();

    // ── Key handlers per tab ─────────────────────────────────────────────
    void handleKeyData(const KeyEvent& ev);
    void handleKeyStats(const KeyEvent& ev);
    void handleKeyGraph(const KeyEvent& ev);
};
