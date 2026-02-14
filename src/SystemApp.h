/**
 * SystemApp.h
 * NumOS System Application — Main controller for menu, navigation, and apps.
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"
#include "math/Tokenizer.h"
#include "math/Parser.h"
#include "math/Evaluator.h"
#include "math/VariableContext.h"
#include "math/EquationSolver.h"
#include "math/StepLogger.h"
#include "ui/GraphView.h"
#include "ui/Icons.h"
#include "ui/MainMenu.h"
#include "apps/CalculationApp.h"
#include "apps/GrapherApp.h"

// ── App descriptor ──
struct AppData {
    int id;
    String name;
    const uint16_t* iconData;
    AppData(int i, String n, const uint16_t* d) : id(i), name(n), iconData(d) {}
};

// ── System modes ──
enum class Mode : uint8_t {
    MENU,              // Icon grid
    APP_CALCULATION,   // Scientific calculator
    APP_GRAPHER,       // Graph viewer
    APP_PYTHON,        // Python shell (placeholder)
    APP_STATISTICS,    // Statistics (placeholder)
    APP_EQUATIONS,     // Equation solver
    APP_SETTINGS,      // Settings (placeholder)
    STEP_VIEW          // Step-by-step view
};

class SystemApp {
public:
    SystemApp(DisplayDriver &display, KeyMatrix &keypad);

    void begin();
    void update();

    /// Inject a KeyEvent from external source (e.g. SerialBridge)
    void injectKey(const KeyEvent &ev);

private:
    DisplayDriver &_display;
    KeyMatrix &_keypad;

    // Legacy MainMenu object (kept for compatibility)
    MainMenu _mainMenu;

    // ── Apps ──
    CalculationApp* _calcApp;
    GrapherApp*     _grapherApp;

    // Math Engine
    Tokenizer _tokenizer;
    Parser _parser;
    Evaluator _evaluator;
    VariableContext _vars;
    EquationSolver _equationSolver;
    StepLogger _stepLogger;

    // Graphing (shared or deprecated? GrapherApp handles its own)
    GraphView _graphView;

    // Shared state
    bool _shiftActive;
    bool _alphaActive;
    AngleMode _angleMode;

    // System state
    Mode _mode;
    bool _hasSteps;
    int _stepScroll;

    // Grid Menu state
    std::vector<AppData> _apps;
    int _selectedAppIndex;
    int _menuScrollOffset;
    bool _redraw;

    // ── Initialization ──
    void initApps();

    // ── Key handling ──
    void handleKey(const KeyEvent &ev);

    // ── Key handling (internal) ──
    void handleKeyMenu(const KeyEvent &ev);
    void handleKeySteps(const KeyEvent &ev);
    void handleKeyGraph(const KeyEvent &ev);
    void handleKeyApp(const KeyEvent &ev);   // Generic placeholder apps

    // ── App transitions ──
    void switchApp(int id);

    // ── Rendering ──
    void render();
    void renderMenu();
    void renderSteps();
    void renderGraphMode();
    void renderAppView();   // Generic placeholder for unfinished apps

    // ── Shared UI helpers ──
    void drawStatusBar();
    void drawStatusBar(const String &title);

};
