#pragma once

#include "../display/DisplayDriver.h"
#include "../math/VariableContext.h"
#include "../math/Evaluator.h"
#include "../math/Parser.h"
#include "../math/Tokenizer.h"
#include "../input/KeyMatrix.h"

// Define tabs
enum class GrapherTab { EXPRESSIONS, GRAPH, TABLE };

// Define GraphState for viewing window
struct GraphState {
    double xMin, xMax;
    double yMin, yMax;
};

class GrapherApp {
public:
    GrapherApp(DisplayDriver& display, VariableContext& vars);
    ~GrapherApp();

    void begin();
    void handleKey(const KeyEvent& ev);
    void render();

private:
    DisplayDriver&   _display;
    VariableContext& _vars;

    // State
    GrapherTab _currentTab;     // Was _activeTab
    String     _expressions[3]; // Fixed array as requested
    int        _selectedExprIdx;
    bool       _exprEditMode;
    bool       _shiftActive;
    bool       _redraw;
    
    GraphState _view;

    // Helpers
    void drawTabs();
    void drawExpressions();
    void drawGraph();
    void drawTable();
    
    // Key handlers per tab
    void handleKeyExpressions(const KeyEvent& ev);
    void handleKeyGraph(const KeyEvent& ev);
    void handleKeyTable(const KeyEvent& ev);
};
