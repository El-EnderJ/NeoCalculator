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
 * CALCULUS-APP-REBUILD-01 — physical-keypad calculus at 320 x 240.
 * VPAM authored input -> Giac command semantics -> structured NumOS result.
 * First derivative and indefinite integral; native steps are optional,
 * verified by Giac, and never used as an answer fallback.
 * Editing has explicit editor/mode focus; results and steps use bounded views.
 */

#pragma once

#include <lvgl.h>
#include <memory>
#include <vector>
#include "../math/MathAST.h"
#include "../math/CalculationEngine.h"
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
    bool navigateBack();

    // NativeHAL semantic-test seam. No Giac-owned type is exposed.
    const char* debugEngineName() const;
    const char* debugStatusName() const;
    const char* debugResultKindName() const;
    const char* debugTutorStatusName() const;
    const char* debugOperationName() const;
    const std::string& debugExactText() const;
    bool debugResultNear(double expected, double epsilon) const;
#ifdef NATIVE_SIM
    const char* debugStateName() const;
    const char* debugFocusName() const;
    bool debugResultEquivalent(const std::string& expected) const;
    bool debugLayoutFits() const;
    void debugForceTutorDisagreement(bool enabled) {
        _debugForceTutorDisagreement = enabled;
    }
#endif

private:
    // ── App states ───────────────────────────────────────────────────
    enum class State : uint8_t {
        EDITING,    ///< Expression input with mode tabs
        COMPUTING,  ///< Computing (bounded busy label)
        RESULT,     ///< Result display (derivative or antiderivative)
        STEPS       ///< Step-by-step view
    };

    // ── Calculus mode ────────────────────────────────────────────────
    enum class CalcMode : uint8_t {
        DERIVATIVE, ///< d/dx mode (orange accent)
        INTEGRAL    ///< ∫dx mode (purple accent)
    };

    enum class ResultKind : uint8_t {
        None,
        Structured,
        TextFallback
    };

    enum class TutorStatus : uint8_t {
        Agreed,
        Unavailable,
        Disabled
    };

    // ── LVGL widgets ─────────────────────────────────────────────────
    lv_obj_t*       _scr;
    ui::StatusBar   _statusBar;

    // Mode tabs
    lv_obj_t*       _tabDerivative;  ///< Derivative mode
    lv_obj_t*       _tabIntegral;    ///< Integral mode

    // INPUT state
    lv_obj_t*       _inputContainer;
    lv_obj_t*       _inputTitle;
    lv_obj_t*       _inputHint;
    vpam::MathCanvas   _inputCanvas;
    vpam::NodePtr      _inputNode;
    vpam::NodeRow*     _inputRow;
    vpam::CursorController _inputCursor;

    // COMPUTING state (synchronous, one busy label)
    lv_obj_t*       _computingContainer;
    lv_obj_t*       _computingLabel;

    // RESULT state
    lv_obj_t*       _resultContainer;
    lv_obj_t*       _resultTitle;
    lv_obj_t*       _resultFallback; ///< Labelled exact Giac text fallback
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

    // ── Step MathCanvas renderers (Pre-Phase 5 Steering Visual) ──────
    // Each StepRenderData owns a NodePtr (MathAST tree) and a MathCanvas
    // widget for 2D pixel-perfect rendering of mathematical expressions
    // in the step-by-step display.  Heap-allocated via unique_ptr to
    // avoid moves that would invalidate LVGL user_data pointers.
    struct StepRenderData {
        vpam::NodePtr    nodeData;   ///< Owns the MathAST tree
        vpam::MathCanvas canvas;     ///< LVGL widget for 2D rendering
    };
    std::vector<std::unique_ptr<StepRenderData>> _stepRenderers;

    lv_obj_t* _inputPlaceholder = nullptr;
    lv_obj_t* _originalViewport = nullptr;
    lv_obj_t* _resultSeparator = nullptr;
    lv_obj_t* _resultViewport;
    bool _modeFocused;

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
    numos::StructuredCalculusResult _giacResult;
    ResultKind         _resultKind;
    TutorStatus        _tutorStatus;
    std::string        _serializedInput;
    std::string        _tutorDiagnostic;
#ifdef NATIVE_SIM
    bool               _debugForceTutorDisagreement;
#endif

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
    void computeGiacResult();
    void runNativeTutor(const numos::CalculusRequest& request);
    char detectAuthoredVariable(const vpam::MathNode* node) const;
    void buildResultDisplay();
    void buildStepsDisplay();

    void resetInput();
    void refreshInput();
    void adjustInputHeight();

    // ── Detect variable ──────────────────────────────────────────────
    char detectVariable(const cas::SymExpr* expr);
};
