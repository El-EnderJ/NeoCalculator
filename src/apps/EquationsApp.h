/**
 * EquationsApp.h — Equation Solver App for NumOS.
 *
 * NumWorks-inspired redesign with:
 *   - Equation list: up to 3 equation slots with live MathCanvas preview
 *   - Template dropdown: presets (Empty, x+y=0, x²+x+1=0, etc.)
 *   - "Add an equation" and "Solve" buttons
 *   - Full MathCanvas editor for each equation
 *   - OmniSolver for single equations, SystemSolver for systems
 *   - Step-by-step display
 *
 * Part of: NumOS — Equation Solver
 */

#pragma once

#include <lvgl.h>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SingleSolver.h"
#include "../math/cas/SystemSolver.h"
#include "../math/cas/OmniSolver.h"
#include "../math/cas/SymExprToAST.h"
#include "../math/cas/SymToAST.h"
#include "../math/cas/SymExprArena.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class EquationsApp {
public:
    EquationsApp();
    ~EquationsApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── States ───────────────────────────────────────────────────────
    enum class State : uint8_t {
        EQ_LIST,    ///< Main list: equation slots + Add + Solve
        TEMPLATE,   ///< Template dropdown overlay
        EDITING,    ///< Full MathCanvas editor for one equation
        SOLVING,    ///< Solving in progress (spinner + message)
        RESULT,     ///< Solution display
        STEPS       ///< Step-by-step view
    };

    // ── Constants ────────────────────────────────────────────────────
    static constexpr int MAX_EQS     = 3;
    static constexpr int MAX_RESULTS = 4;

    // Template definitions
    static constexpr int NUM_TEMPLATES = 6;
    static const char* TEMPLATE_LABELS[NUM_TEMPLATES];

    // ── LVGL widgets ─────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // ── EQ_LIST state ────────────────────────────────────────────────
    lv_obj_t*       _listContainer;
    lv_obj_t*       _eqRows[MAX_EQS];
    lv_obj_t*       _eqLabels[MAX_EQS];       ///< "E1:", "E2:", "E3:" indicator
    vpam::MathCanvas   _eqPreview[MAX_EQS];    ///< mini preview canvas
    lv_obj_t*       _addRow;
    lv_obj_t*       _addLabel;
    lv_obj_t*       _solveRow;
    lv_obj_t*       _solveLabel;
    lv_obj_t*       _listHint;

    // ── Equation data ────────────────────────────────────────────────
    vpam::NodePtr      _eqNode[MAX_EQS];
    vpam::NodeRow*     _eqRowData[MAX_EQS];
    int                _numEquations;

    // List navigation
    int                _listFocus;
    // Virtual items: 0..numEq-1 = equation slots, numEq = ADD, numEq+1 = SOLVE

    // ── TEMPLATE state ───────────────────────────────────────────────
    lv_obj_t*       _templateOverlay;
    lv_obj_t*       _templateTitle;
    lv_obj_t*       _templateItems[NUM_TEMPLATES];
    int             _templateFocus;

    // ── EDITING state ────────────────────────────────────────────────
    lv_obj_t*       _editContainer;
    lv_obj_t*       _editTitle;
    lv_obj_t*       _editHint;
    vpam::MathCanvas   _editCanvas;
    vpam::NodePtr      _editNode;
    vpam::NodeRow*     _editRow;
    vpam::CursorController _editCursor;
    int                _editingIndex;

    // ── SOLVING state ────────────────────────────────────────────────
    lv_obj_t*       _solvingContainer;
    lv_obj_t*       _solvingSpinner;
    lv_obj_t*       _solvingLabel;

    // ── RESULT state ─────────────────────────────────────────────────
    lv_obj_t*       _resultContainer;
    lv_obj_t*       _resultTitle;
    lv_obj_t*       _resultHint;
    vpam::MathCanvas   _resultCanvas[MAX_RESULTS];
    vpam::NodePtr      _resultNode[MAX_RESULTS];
    vpam::NodeRow*     _resultRow[MAX_RESULTS];
    int                _resultCount;

    // ── STEPS state ──────────────────────────────────────────────────
    lv_obj_t*       _stepsContainer;

    // ── App state ────────────────────────────────────────────────────
    State   _state;
    int     _stepScroll;

    // ── CAS results ──────────────────────────────────────────────────
    cas::SymExprArena  _arena;
    cas::OmniResult    _omniResult;
    cas::SystemResult  _systemResult;
    cas::NLSystemResult _nlResult;
    bool               _isOmniSolve;
    bool               _isNLSolve = false;

    // ── UI creation / state management ───────────────────────────────
    void createUI();
    void showEqList();
    void showTemplate();
    void showEditing(int idx);
    void showSolving();
    void showResult();
    void showSteps();
    void hideAllContainers();

    // ── List focus management ────────────────────────────────────────
    int  listItemCount() const;
    void updateListFocus();

    // ── Key handlers per state ───────────────────────────────────────
    void handleKeyList(const KeyEvent& ev);
    void handleKeyTemplate(const KeyEvent& ev);
    void handleKeyEditing(const KeyEvent& ev);
    void handleKeyResult(const KeyEvent& ev);
    void handleKeySteps(const KeyEvent& ev);

    // ── Template application ─────────────────────────────────────────
    void applyTemplate(int templateIdx, int eqSlot);
    vpam::NodePtr buildTemplateAST(int templateIdx);

    // ── Equation preview ─────────────────────────────────────────────
    void refreshPreview(int idx);
    void refreshAllPreviews();

    // ── Solving logic ────────────────────────────────────────────────
    void solveEquations();
    void solveOmni();
    void solveSystem();
    void buildResultDisplay();
    void buildStepsDisplay();

    bool splitAtEquals(vpam::NodeRow* row,
                       vpam::NodePtr& outLHS,
                       vpam::NodePtr& outRHS);

    cas::LinEq symEquationToLinEq(const cas::SymEquation& eq,
                                  const char* vars, int numVars);

    // ── Dynamic height ───────────────────────────────────────────────
    void adjustEditHeight();

    // ── Loading messages ─────────────────────────────────────────────
    static const char* randomSolvingMessage();
};
