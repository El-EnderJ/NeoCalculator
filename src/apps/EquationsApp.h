/**
 * EquationsApp.h — Equation Solver App for NumOS.
 *
 * LVGL-native app that allows users to:
 *   1. Select equation type: Grado 1, Grado 2, Sistema 2×2, Sistema 3×3
 *   2. Input equations using MathCanvas + CursorController (with '=' via S⇔D)
 *   3. Solve using CAS-Lite (SingleSolver / SystemSolver)
 *   4. View step-by-step solution via SHOW_STEPS key
 *
 * Lifecycle (follows CalculationApp pattern):
 *   begin() → creates LVGL screen + widgets
 *   load()  → makes the screen visible
 *   end()   → destroys LVGL screen
 *
 * Part of: NumOS CAS-Lite — Phase E (EquationsApp UI)
 */

#pragma once

#include <lvgl.h>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SingleSolver.h"
#include "../math/cas/SystemSolver.h"
#include "../math/cas/SymToAST.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class EquationsApp {
public:
    EquationsApp();
    ~EquationsApp();

    /// Creates the LVGL screen and base widgets.
    void begin();

    /// Destroys the LVGL screen and frees resources.
    void end();

    /// Loads (makes visible) the LVGL screen.
    void load();

    /// Processes a key event from the system.
    void handleKey(const KeyEvent& ev);

    /// Returns true if the screen is created and active.
    bool isActive() const { return _screen != nullptr; }

private:
    // ── States ───────────────────────────────────────────────────────
    enum class State : uint8_t {
        SELECT,    ///< Type selection menu
        EQ_INPUT,  ///< Equation input (1–3 MathCanvas)
        RESULT,    ///< Solution display
        STEPS      ///< Step-by-step view
    };

    enum class EqType : uint8_t {
        DEGREE_1,    ///< Linear equation (ax + b = 0)
        DEGREE_2,    ///< Quadratic equation (ax² + bx + c = 0)
        SYSTEM_2X2,  ///< 2-variable system
        SYSTEM_3X3   ///< 3-variable system
    };

    // ── LVGL widgets ─────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // SELECT state
    lv_obj_t*       _selectContainer;
    lv_obj_t*       _menuLabels[4];

    // INPUT state
    static constexpr int MAX_EQS = 3;
    lv_obj_t*       _inputContainer;
    lv_obj_t*       _inputTitle;
    lv_obj_t*       _inputHint;
    lv_obj_t*       _inputIndicators[MAX_EQS];
    vpam::MathCanvas   _inputCanvas[MAX_EQS];
    vpam::NodePtr      _inputNode[MAX_EQS];
    vpam::NodeRow*     _inputRow[MAX_EQS];
    vpam::CursorController _inputCursor[MAX_EQS];

    // RESULT state
    lv_obj_t*       _resultContainer;
    lv_obj_t*       _resultTitle;
    lv_obj_t*       _resultHint;
    vpam::MathCanvas   _resultCanvas[MAX_EQS];
    vpam::NodePtr      _resultNode[MAX_EQS];
    vpam::NodeRow*     _resultRow[MAX_EQS];
    int                _resultCount;

    // STEPS state
    lv_obj_t*       _stepsContainer;

    // ── App state ────────────────────────────────────────────────────
    State   _state;
    EqType  _type;
    int     _selectIndex;     ///< Selected item in SELECT menu (0–3)
    int     _numInputs;       ///< How many equation inputs for current type
    int     _activeInput;     ///< Which input has focus (0–2)
    int     _stepScroll;      ///< Scroll offset in STEPS view

    // ── CAS results ──────────────────────────────────────────────────
    cas::SolveResult   _singleResult;
    cas::SystemResult  _systemResult;
    bool               _isSingleEq;

    // ── UI creation / state management ───────────────────────────────
    void createUI();
    void showSelect();
    void showInput();
    void showResult();
    void showSteps();
    void hideAllContainers();

    // ── Key handlers per state ───────────────────────────────────────
    void handleKeySelect(const KeyEvent& ev);
    void handleKeyInput(const KeyEvent& ev);
    void handleKeyResult(const KeyEvent& ev);
    void handleKeySteps(const KeyEvent& ev);

    // ── Solving logic ────────────────────────────────────────────────

    /// Run the appropriate solver on the current inputs.
    void solveEquations();

    /// Build the RESULT display from solver outputs.
    void buildResultDisplay();

    /// Build the STEPS display from solver step logs.
    void buildStepsDisplay();

    /// Split a NodeRow at the '=' NodeVariable into LHS and RHS sub-rows.
    /// Returns true if '=' was found; otherwise outRHS gets a "0" node.
    bool splitAtEquals(vpam::NodeRow* row,
                       vpam::NodePtr& outLHS,
                       vpam::NodePtr& outRHS);

    /// Convert a SymEquation (after moveAllToLHS) to a LinEq by extracting
    /// coefficients for the given variables.
    cas::LinEq symEquationToLinEq(const cas::SymEquation& eq,
                                  const char* vars, int numVars);

    /// Reset a single equation input slot to an empty row.
    void resetInput(int idx);

    /// Refresh the MathCanvas display for the active input.
    void refreshActiveInput();

    /// Update the active-indicator markers (►) on input lines.
    void updateIndicators();
};
