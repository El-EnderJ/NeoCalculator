/**
 * CalculusApp.h — Unified Symbolic Calculus App for NumOS.
 *
 * Phase 4: Unified Calculus (Derivatives + Integrals) in one app.
 *
 * LVGL-native app with states:
 *   INPUT → COMPUTING → RESULT → STEPS
 *
 * Modes (selectable via tabs):
 *   DERIVATIVE — d/dx pipeline through SymDiff
 *   INTEGRAL   — ∫dx pipeline through SymIntegrate
 *
 * Pipeline:
 *   MathAST → ASTFlattener → SymExpr →
 *     [SymDiff::diff() | SymIntegrate::integrate()] →
 *   SymSimplify::simplify() → SymExprToAST → MathCanvas display
 *
 * Features:
 *   - Full symbolic differentiation (17+ rules, chain/product/quotient)
 *   - Full symbolic integration (table, linearity, u-sub, parts/LIATE)
 *   - Tab-based mode switching (d/dx ↔ ∫dx) via F1/F2 or GRAPH key
 *   - Automatic simplification (0+x→x, 1·x→x, x^1→x, etc.)
 *   - 2D rendering of result via MathCanvas (VPAM)
 *   - Step logger showing rules applied
 *   - Function key support (sin, cos, tan, ln, log, √, π, e)
 *   - Dynamic input height adapting to expression content
 *   - PSRAM arena-based memory (no heap fragmentation)
 *
 * Part of: NumOS Pro-CAS — Phase 4 (Unified Calculus App)
 */

#pragma once

#include <lvgl.h>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SymDiff.h"
#include "../math/cas/SymIntegrate.h"
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
    // ── App states ───────────────────────────────────────────────────
    enum class State : uint8_t {
        EDITING,    ///< Expression input with mode tabs
        COMPUTING,  ///< Computing (spinner)
        RESULT,     ///< Result display (derivative or antiderivative)
        STEPS       ///< Step-by-step view
    };

    // ── Calculus mode ────────────────────────────────────────────────
    enum class CalcMode : uint8_t {
        DERIVATIVE, ///< d/dx mode (orange accent)
        INTEGRAL    ///< ∫dx mode (purple accent)
    };

    // ── LVGL widgets ─────────────────────────────────────────────────
    lv_obj_t*       _scr;
    ui::StatusBar   _statusBar;

    // Mode tabs
    lv_obj_t*       _tabDerivative;  ///< "d/dx" tab button
    lv_obj_t*       _tabIntegral;    ///< "∫dx" tab button

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
    lv_obj_t*       _resultLabel;    ///< "f'(x) =" or "F(x) =" label
    lv_obj_t*       _resultHint;
    vpam::MathCanvas   _resultCanvas;  ///< Rendered result
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
    State    _state;
    CalcMode _calcMode;       ///< Current mode (derivative or integral)
    int      _stepScroll;
    char     _variable;       ///< Variable (default: 'x')

    // ── CAS arena & results ──────────────────────────────────────────
    cas::SymExprArena  _arena;
    cas::CASStepLogger _casSteps;
    cas::SymExpr*      _resultExpr;   ///< Simplified result (arena-owned)
    bool               _integralFound; ///< True if closed-form integral found

    // ── UI creation / state management ───────────────────────────────
    void createUI();
    void showInput();
    void showComputing();
    void showResult();
    void showSteps();
    void hideAllContainers();

    // ── Mode tab management ──────────────────────────────────────────
    void setMode(CalcMode mode);
    void updateTabStyles();
    uint32_t accentColor() const;

    // ── Key handlers per state ───────────────────────────────────────
    void handleKeyInput(const KeyEvent& ev);
    void handleKeyResult(const KeyEvent& ev);
    void handleKeySteps(const KeyEvent& ev);

    // ── Computation ──────────────────────────────────────────────────
    void computeResult();
    void computeDerivative(cas::SymExpr* expr);
    void computeIntegral(cas::SymExpr* expr);
    void buildResultDisplay();
    void buildStepsDisplay();

    void resetInput();
    void refreshInput();
    void adjustInputHeight();

    // ── Detect variable ──────────────────────────────────────────────
    char detectVariable(const cas::SymExpr* expr);
};
