/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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
#include <vector>
#include <memory>
#include <string>
#include <cstddef>
#include "../math/MathAST.h"
#include "../math/CalculationEngine.h"
#include "../math/giac/GiacEngine.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SingleSolver.h"
#include "../math/cas/SystemSolver.h"
#include "../math/cas/OmniSolver.h"
#include "../math/cas/SymExprToAST.h"
#include "../math/cas/SymToAST.h"
#include "../math/cas/SymExprArena.h"
#include "../math/cas/RuleEngine.h"
#include "../math/cas/AlgebraicRules.h"
#include "../math/cas/CasToVpam.h"
#include "../math/cas/CasMemory.h"
#include "../math/cas/SystemHeuristics.h"
#include "../math/cas/SystemTutor.h"
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
    void update();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

#ifdef NATIVE_SIM
    // GIAC-D01 semantic probes. These expose NumOS-owned state only.
    const char* debugEngineName() const;
    const char* debugStatusName() const;
    int debugSolutionCount() const;
    bool debugSolutionNear(const std::string& variable, int index,
                           double expected, double epsilon) const;
    bool debugSolutionExact(const std::string& variable, int index,
                            const std::string& expected) const;
    std::string debugSolutionExactText(const std::string& variable,
                                       int index) const;
    const char* debugResultKindName() const;
    const char* debugTutorStatusName() const;
#endif

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
    static constexpr int NUM_TEMPLATES = 4;
    static const char* TEMPLATE_LABELS[NUM_TEMPLATES];

    // ── LVGL widgets ─────────────────────────────────────────────────
    lv_obj_t*       _screen         = nullptr;
    ui::StatusBar   _statusBar;

    // ── EQ_LIST state ────────────────────────────────────────────────
    lv_obj_t*       _listContainer  = nullptr;
    lv_obj_t*       _eqRows[MAX_EQS];
    lv_obj_t*       _eqLabels[MAX_EQS];       ///< "E1:", "E2:", "E3:" indicator
    vpam::MathCanvas   _eqPreview[MAX_EQS];    ///< mini preview canvas
    lv_obj_t*       _addRow         = nullptr;
    lv_obj_t*       _addLabel       = nullptr;
    lv_obj_t*       _solveRow       = nullptr;
    lv_obj_t*       _solveLabel     = nullptr;
    lv_obj_t*       _listHint       = nullptr;

    // ── Equation data ────────────────────────────────────────────────
    vpam::NodePtr      _eqNode[MAX_EQS];
    vpam::NodeRow*     _eqRowData[MAX_EQS];
    int                _numEquations = 0;

    // List navigation
    int                _listFocus    = 0;
    // Virtual items: 0..numEq-1 = equation slots, numEq = ADD, numEq+1 = SOLVE

    // ── TEMPLATE state ───────────────────────────────────────────────
    lv_obj_t*       _templateOverlay = nullptr;
    lv_obj_t*       _templateTitle   = nullptr;
    lv_obj_t*       _templateItems[NUM_TEMPLATES];   ///< row containers
    lv_obj_t*       _templateLabels[NUM_TEMPLATES];  ///< text-only prefix labels
    vpam::MathCanvas   _templateCanvas[NUM_TEMPLATES]; ///< VPAM formula preview
    vpam::NodePtr      _templateNode[NUM_TEMPLATES];   ///< AST data for canvas
    int             _templateFocus   = 0;

    // ── EDITING state ────────────────────────────────────────────────
    lv_obj_t*       _editContainer   = nullptr;
    lv_obj_t*       _editTitle       = nullptr;
    lv_obj_t*       _editHint        = nullptr;
    vpam::MathCanvas   _editCanvas;
    vpam::NodePtr      _editNode;
    vpam::NodeRow*     _editRow      = nullptr;
    vpam::CursorController _editCursor;
    int                _editingIndex = -1;

    // ── SOLVING state ────────────────────────────────────────────────
    lv_obj_t*       _solvingContainer = nullptr;
    lv_obj_t*       _solvingSpinner   = nullptr;
    lv_obj_t*       _solvingLabel     = nullptr;

    // ── RESULT state ─────────────────────────────────────────────────
    lv_obj_t*       _resultContainer = nullptr;
    lv_obj_t*       _resultTitle     = nullptr;
    lv_obj_t*       _resultHint      = nullptr;
    lv_obj_t*       _resultFallback  = nullptr;
    vpam::MathCanvas   _resultCanvas[MAX_RESULTS];
    vpam::NodePtr      _resultNode[MAX_RESULTS];
    vpam::NodeRow*     _resultRow[MAX_RESULTS];
    int                _resultCount   = 0;

    enum class ResultKind : uint8_t {
        None,
        Structured,
        TextFallback,
    };

    enum class TutorStatus : uint8_t {
        Disabled,
        Agreed,
        Unavailable,
    };

    numos::StructuredSolveResult _giacResult;
    ResultKind                   _resultKind = ResultKind::None;
    TutorStatus                  _tutorStatus = TutorStatus::Disabled;
    std::string                  _tutorDiagnostic;

    // ── STEPS state ──────────────────────────────────────────────────
    lv_obj_t*       _stepsContainer  = nullptr;

    // Dynamic step renderers — each owns a MathAST tree + MathCanvas
    struct StepRenderData {
        vpam::NodePtr    nodeData;   ///< Owns the MathAST tree
        vpam::MathCanvas canvas;     ///< LVGL widget for 2D rendering
    };
    std::vector<std::unique_ptr<StepRenderData>> _stepRenderers;

    // ── App state ────────────────────────────────────────────────────
    State   _state       = State::EQ_LIST;
    int     _stepScroll  = 0;

    // Epoch coherence: steps can only be shown for the latest solved equation set.
    uint32_t _equationEpoch = 1;
    uint32_t _solveEpoch    = 0;
    uint32_t _stepsEpoch    = 0;

    // ── Staged Steps pipeline (Parse -> Solve -> RenderChunk -> Finalize) ──
    enum class StepsSource : uint8_t {
        NONE,
        LEGACY_LOG,
        SINGLE_CAS,
        SYSTEM_CAS,
    };

    enum class StepsStage : uint8_t {
        IDLE,
        PARSE,
        SOLVE,
        RENDER_CHUNK,
        FINALIZE,
    };

    StepsSource _stepsSource = StepsSource::NONE;
    StepsStage  _stepsStage  = StepsStage::IDLE;
    bool        _stepsPipelineActive = false;
    bool        _stepsReachedFixedPoint = false;
    bool        _stepsMemoryLimitReached = false;
    bool        _stepsHeaderRendered = false;

    lv_obj_t*   _stepsProgressLabel = nullptr;
    std::string _stepsPipelineError;
    std::string _stepsEqText1;
    std::string _stepsEqText2;
    char        _stepsVar1 = 'x';
    char        _stepsVar2 = 'y';

    cas::NodePtr _stepsParsedEq1;
    cas::NodePtr _stepsParsedEq2;
    cas::NodePtr _stepsCurrentTree;

    std::vector<cas::RuleEngine::StepLog> _stepsCasPendingLogs;
    std::vector<cas::SystemTutorStep>     _stepsSystemPendingLogs;
    cas::SolveResult                      _stepsTutorResult;
    bool                                  _stepsTutorActive = false;
    cas::SymExprArena                     _stepsTutorArena;

    std::size_t _stepsSolveIterations = 0;
    std::size_t _stepsRenderIndex = 0;
    static constexpr std::size_t STEPS_SOLVE_MAX = 80;

    // ── CAS results ──────────────────────────────────────────────────
    cas::SymExprArena  _arena;
    cas::OmniResult    _omniResult;
    cas::SystemResult  _systemResult;
    cas::NLSystemResult _nlResult;
    bool               _isOmniSolve;
    bool               _isNLSolve = false;

    // ── Algebraic TRS state (single-equation CAS step view) ──────────
    std::unique_ptr<cas::CasMemoryPool> _casPool;
    std::unique_ptr<cas::RuleEngine>    _casEngine;
    cas::RuleEngine::SolveResult        _casResult;
    bool                                _hasCasResult = false;

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
    void solveWithGiac();
    void generateTutorCandidate();
    bool tutorCandidateAgrees() const;
    void buildResultDisplay();
    void buildGiacResultDisplay();
    void buildStepsDisplay();
    void buildCASStepsDisplay();         ///< Algebraic TRS step display (RuleEngine)
    void buildSystemCASStepsDisplay();   ///< System of equations TRS step display
    void invalidateStepsCache();
    void resetStepsPipeline();
    bool isValidLvObj(const lv_obj_t* obj) const;
    bool canAllocStep(std::size_t requiredContiguous) const;
    void logStepBudget(int stepIndex) const;
    void failClosedStepRender(std::size_t stepIndex);

    static constexpr std::size_t STEPS_INTERNAL_MAXBLOCK_FLOOR = 40U * 1024U;
    static constexpr std::size_t STEPS_CHILD_HARD_CAP = 90U;
    static constexpr std::size_t STEPS_LABEL_BUDGET = 4U * 1024U;
    static constexpr std::size_t STEPS_CANVAS_BUDGET = 8U * 1024U;

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
