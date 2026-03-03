/**
 * CalculusApp.h — Symbolic Calculus App for NumOS.
 *
 * Phase 6: Derivative App & Production Polish.
 *
 * LVGL-native app with states:
 *   INPUT → COMPUTING → RESULT → STEPS
 *
 * Pipeline:
 *   MathAST → ASTFlattener → SymExpr → SymDiff::diff() →
 *   SymSimplify::simplify() → SymExprToAST → MathCanvas display
 *
 * Features:
 *   - Full symbolic differentiation of polynomials & transcendentals
 *   - Automatic simplification (0+x→x, 1·x→x, x^1→x, etc.)
 *   - 2D rendering of derivative result via MathCanvas
 *   - Step logger showing differentiation rules applied
 *   - Function key support (sin, cos, tan, ln, log, √, π, e)
 *   - Dynamic input height adapting to expression content
 *   - PSRAM arena-based memory (no heap fragmentation)
 *
 * Part of: NumOS Pro-CAS — Phase 6 (Calculus & Production Polish)
 */

#pragma once

#include <lvgl.h>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SymDiff.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/SymExprToAST.h"
#include "../math/cas/SymExprArena.h"
#include "../math/cas/CASStepLogger.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class CalculusApp {
public:
    CalculusApp();
    ~CalculusApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _scr != nullptr; }

private:
    // ── States ───────────────────────────────────────────────────────
    enum class State : uint8_t {
        EDITING,    ///< Expression input
        COMPUTING,  ///< Computing (spinner)
        RESULT,     ///< Derivative result display
        STEPS       ///< Step-by-step view
    };

    // ── LVGL widgets ─────────────────────────────────────────────────
    lv_obj_t*       _scr;
    ui::StatusBar   _statusBar;

    // INPUT state
    lv_obj_t*       _inputContainer;
    lv_obj_t*       _inputTitle;
    lv_obj_t*       _inputHint;
    vpam::MathCanvas   _inputCanvas;
    vpam::NodePtr      _inputNode;
    vpam::NodeRow*     _inputRow;
    vpam::CursorController _inputCursor;

    // COMPUTING state (loading spinner)
    lv_obj_t*       _computingContainer;
    lv_obj_t*       _computingSpinner;
    lv_obj_t*       _computingLabel;

    // RESULT state
    lv_obj_t*       _resultContainer;
    lv_obj_t*       _resultTitle;
    lv_obj_t*       _resultLabel;    ///< "f'(x) =" label
    lv_obj_t*       _resultHint;
    vpam::MathCanvas   _resultCanvas;  ///< Rendered derivative
    vpam::NodePtr      _resultNode;
    vpam::NodeRow*     _resultRow;

    // Original expression for display
    lv_obj_t*       _originalLabel;
    vpam::MathCanvas   _originalCanvas;
    vpam::NodePtr      _originalNode;
    vpam::NodeRow*     _originalRow;

    // STEPS state (scrollable)
    lv_obj_t*       _stepsContainer;

    // ── App state ────────────────────────────────────────────────────
    State   _state;
    int     _stepScroll;
    char    _variable;        ///< Variable to differentiate w.r.t. (default: 'x')

    // ── CAS arena & results ──────────────────────────────────────────
    cas::SymExprArena  _arena;
    cas::CASStepLogger _diffSteps;
    cas::SymExpr*      _derivExpr;  ///< Simplified derivative (arena-owned)

    // ── UI creation / state management ───────────────────────────────
    void createUI();
    void showInput();
    void showComputing();
    void showResult();
    void showSteps();
    void hideAllContainers();

    // ── Key handlers per state ───────────────────────────────────────
    void handleKeyInput(const KeyEvent& ev);
    void handleKeyResult(const KeyEvent& ev);
    void handleKeySteps(const KeyEvent& ev);

    // ── Differentiation logic ────────────────────────────────────────
    void computeDerivative();
    void buildResultDisplay();
    void buildStepsDisplay();

    void resetInput();
    void refreshInput();
    void adjustInputHeight();

    // ── Detect variable ──────────────────────────────────────────────
    char detectVariable(const cas::SymExpr* expr);
};
