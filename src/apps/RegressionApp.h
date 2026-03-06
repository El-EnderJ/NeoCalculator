/**
 * RegressionApp.h — Regression analysis application for NumOS.
 *
 * LVGL-native app with 3 tabs (manual, like StatisticsApp):
 *   Tab 0 — Data:     lv_table editor (X, Y columns)
 *   Tab 1 — Equation: Display regression equation, coefficients, R²
 *   Tab 2 — Graph:    Scatter plot + regression curve
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "RegressionEngine.h"

class RegressionApp {
public:
    RegressionApp();
    ~RegressionApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Tabs ─────────────────────────────────────────────────────────────
    enum class Tab : uint8_t { DATA = 0, EQUATION = 1, GRAPH = 2 };
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

    // Tab 1: Equation results
    lv_obj_t*       _eqPanel;
    lv_obj_t*       _eqModelLabel;       // "Linear" / "Quadratic"
    static constexpr int EQ_LABEL_COUNT = 6;
    lv_obj_t*       _eqLabels[EQ_LABEL_COUNT]; // equation, coeff labels, R²

    // Tab 2: Graph (scatter + curve)
    lv_obj_t*       _graphPanel;
    lv_obj_t*       _graphCanvas;        // Drawing area

    // ── State ────────────────────────────────────────────────────────────
    Tab             _activeTab;
    int             _tableRow;
    int             _tableCol;
    int             _numRows;
    bool            _editing;

    // Cell edit buffer
    char            _editBuf[16];
    int             _editLen;

    // Data storage
    double          _xData[MAX_ROWS];
    double          _yData[MAX_ROWS];

    // Regression
    RegressionEngine::Model  _model;
    RegressionEngine::Result _result;

    // ── Builders ─────────────────────────────────────────────────────────
    void createTabBar();
    void createDataTab();
    void createEquationTab();
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
    void updateEquationDisplay();
    void updateGraph();

    // ── Graph drawing helpers ────────────────────────────────────────────
    void drawAxes(lv_obj_t* canvas, int w, int h,
                  double xMin, double xMax, double yMin, double yMax);
    void drawScatter(lv_obj_t* canvas, int w, int h,
                     double xMin, double xMax, double yMin, double yMax);
    void drawCurve(lv_obj_t* canvas, int w, int h,
                   double xMin, double xMax, double yMin, double yMax);

    // ── Key handlers per tab ─────────────────────────────────────────────
    void handleKeyData(const KeyEvent& ev);
    void handleKeyEquation(const KeyEvent& ev);
    void handleKeyGraph(const KeyEvent& ev);
};
