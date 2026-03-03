/**
 * IntegralApp.h — Symbolic Integration App for NumOS.
 *
 * Phase 6B: Integration App & Final Polish.
 *
 * LVGL-native app with states:
 *   INPUT → COMPUTING → RESULT → STEPS
 *
 * Pipeline:
 *   MathAST → ASTFlattener → SymExpr → SymIntegrate::integrate() →
 *   SymSimplify::simplify() → SymExprToAST::convertIntegral() → MathCanvas
 *
 * Features:
 *   - Full symbolic integration (table, linearity, u-sub, parts)
 *   - Automatic simplification (0+x→x, 1·x→x, x^1→x, etc.)
 *   - 2D rendering of antiderivative result + C via MathCanvas
 *   - Unevaluated ∫ display when no closed-form found
 *   - Step logger showing integration strategies tried
 *   - Function key support (sin, cos, tan, ln, log, √, π, e)
 *   - Dynamic input height adapting to expression content
 *   - PSRAM arena-based memory (no heap fragmentation)
 *
 * Part of: NumOS Pro-CAS — Phase 6B (Integration App & Final Polish)
 */

#pragma once

#include <lvgl.h>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SymIntegrate.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/SymExprToAST.h"
#include "../math/cas/SymExprArena.h"
#include "../math/cas/CASStepLogger.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class IntegralApp {
public:
    IntegralApp();
    ~IntegralApp();

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
        RESULT,     ///< Integral result display
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
    lv_obj_t*       _resultLabel;    ///< "∫f(x)dx =" label
    lv_obj_t*       _resultHint;
    vpam::MathCanvas   _resultCanvas;  ///< Rendered antiderivative
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
    char    _variable;        ///< Variable to integrate w.r.t. (default: 'x')

    // ── CAS arena & results ──────────────────────────────────────────
    cas::SymExprArena  _arena;
    cas::CASStepLogger _integSteps;
    cas::SymExpr*      _integralExpr;   ///< Antiderivative (arena-owned)
    bool               _integralFound;  ///< true if closed-form found

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

    // ── Core computation ─────────────────────────────────────────────
    void computeIntegral();
    void buildResultDisplay();
    void buildStepsDisplay();

    // ── Input management ─────────────────────────────────────────────
    void resetInput();
    void refreshInput();
    void adjustInputHeight();

    // ── Helpers ──────────────────────────────────────────────────────
    char detectVariable(const cas::SymExpr* expr);
};
