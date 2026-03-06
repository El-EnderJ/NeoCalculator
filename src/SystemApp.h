/**
 * SystemApp.h
 * NumOS System Application — Main controller for menu, navigation, and apps.
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <vector>
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"   // legacy — mantenido para compatibilidad
#include "drivers/Keyboard.h"   // nuevo driver 5×10
#include "math/Tokenizer.h"
#include "math/Parser.h"
#include "math/Evaluator.h"
#include "math/VariableContext.h"
#include "math/EquationSolver.h"
#include "math/StepLogger.h"
#include "ui/GraphView.h"
#include "ui/Icons.h"
#include "ui/MainMenu.h"
#include "input/LvglKeypad.h"
#include "apps/CalculationApp.h"
#include "apps/GrapherApp.h"
#include "apps/EquationsApp.h"
#include "apps/CalculusApp.h"
#include "apps/SettingsApp.h"
#include "apps/StatisticsApp.h"
#include "apps/ProbabilityApp.h"
#include "apps/RegressionApp.h"
#include "apps/MatricesApp.h"
#include "apps/PythonApp.h"
#include "apps/SequencesApp.h"
#include "apps/PeriodicTableApp.h"

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
    APP_TABLE,         // Function table (placeholder)
    APP_STATISTICS,    // Statistics (placeholder)
    APP_PROBABILITY,   // Probability distribution (placeholder)
    APP_EQUATIONS,     // Equation solver
    APP_CALCULUS,      // Calculus (derivatives + integrals)
    APP_MATRICES,      // Matrix algebra (LVGL-native)
    APP_REGRESSION,    // Regression analysis (LVGL-native)
    APP_SEQUENCES,     // Sequences & series
    APP_PYTHON,        // Python shell (placeholder)
    APP_PERIODIC_TABLE, // Periodic table & chemistry
    APP_SETTINGS,      // Settings (placeholder)
    STEP_VIEW          // Step-by-step view
};


class SystemApp {
public:
    SystemApp(DisplayDriver &display, Keyboard &keypad);

    void begin();
    void update();

    /// Inject a KeyEvent from external source (e.g. SerialBridge)
    void injectKey(const KeyEvent &ev);

    /**
     * Lanza una app por su ID (0-9).
     * Llamado desde el callback del launcher LVGL.
     * Pausa el rendering de LVGL y activa la app heredada.
     */
    void launchApp(int id);

    /**
     * Apaga el sistema: fade-out LVGL → backlight off → deep sleep.
     * Se activa con SHIFT + AC desde cualquier modo.
     * Despierta con ext0 en PIN_KEY_R1 (tecla ON).
     */
    void powerOff();

private:
    DisplayDriver &_display;
    Keyboard &_keypad;

    // Legacy MainMenu object (kept for compatibility)
    MainMenu _mainMenu;

    // ── Apps ──
    CalculationApp* _calcApp;
    GrapherApp*     _grapherApp;
    EquationsApp*   _equationsApp;
    CalculusApp*    _calculusApp;
    SettingsApp*    _settingsApp;
    StatisticsApp*  _statisticsApp;
    ProbabilityApp* _probabilityApp;
    RegressionApp*  _regressionApp;
    MatricesApp*    _matricesApp;
    PythonApp*      _pythonApp;
    SequencesApp*   _sequencesApp;
    PeriodicTableApp* _periodicTableApp;

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

    // Deferred teardown (SAFE_HOME_EXIT):
    // After returnToMenu(), the old app screen stays alive for 250ms so that
    // the LVGL 200ms FADE_IN animation can reference it safely.  _pendingTeardownMode
    // holds the mode we LEFT; MENU means "nothing to tear down".
    Mode     _pendingTeardownMode;
    uint32_t _teardownStartMs;

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

    /**
     * Vuelve al modo MENU, reanuda LVGL y recarga el launcher screen.
     * Todos los retornos al menú desde apps deben usar este helper.
     */
    void returnToMenu();

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
