/**
 * GrapherApp.h — Graphing Calculator App
 * UI Redesign: NumWorks Style (Tabs: Expressions, Graph, Table).
 */
#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <vector> // For std::vector if needed, though we use fixed array for exprs
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h" // KeyCode
#include "math/Evaluator.h"
#include "math/Parser.h"
#include "math/Tokenizer.h"
#include "math/VariableContext.h"

enum class GrapherTab { EXPRESSIONS, GRAPH, TABLE };

struct GraphFunction {
    String expression;
    bool   enabled;
    uint16_t color;
};

class GrapherApp {
public:
    GrapherApp(DisplayDriver &disp, VariableContext &vars);
    ~GrapherApp();

    void begin();
    void handleKey(const KeyEvent &ev);
    void render();

private:
    DisplayDriver   &_display;
    VariableContext &_vars;
    Evaluator       _evaluator;
    Parser          _parser;
    Tokenizer       _tokenizer;

    /* ── State ── */
    GrapherTab    _currentTab;
    GraphFunction _expressions[3]; // Y1, Y2, Y3
    int           _selectedExprIdx;
    bool          _exprEditMode;
    bool          _shiftActive; // Helper for Shift key overlay/logic
    bool          _redraw;

    /* ── Graph View State ── */
    float _offsetX, _offsetY;
    float _scaleX, _scaleY;

    /* ── Methods ── */
    void drawTabs();
    void drawExpressions();
    void drawGraph();
    void drawTable(); // Placeholder for now

    void handleKeyExpressions(const KeyEvent &ev);
    void handleKeyGraph(const KeyEvent &ev);
    void handleKeyTable(const KeyEvent &ev); // Placeholder

    // Helper to evaluate function at X
    double evaluateFunction(int funcIdx, double x);
};
