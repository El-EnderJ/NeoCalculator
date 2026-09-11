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

#pragma once

#include <lvgl.h>
#include <array>
#include <memory>
#include <string>
#include "../math/CalculationEngine.h"
#include "../math/giac/GiacEngine.h"
#include "../math/CursorController.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/OmniSolver.h"
#include "../math/cas/SystemSolver.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"

// One committed equation set, one transactional draft, one Giac answer.
// Views are constructed lazily in a bounded, reusable content viewport.
class EquationsApp {
public:
    EquationsApp() = default;
    ~EquationsApp() { end(); }
    void begin();
    void end();
    void load();
    void update();
    void handleKey(const KeyEvent& ev);
    bool isActive() const { return _screen != nullptr; }
    bool navigateBack();
#ifdef NATIVE_SIM
    const char* debugEngineName() const;
    const char* debugStatusName() const;
    int debugSolutionCount() const;
    bool debugSolutionNear(const std::string&, int, double, double) const;
    bool debugSolutionExact(const std::string&, int, const std::string&) const;
    std::string debugSolutionExactText(const std::string&, int) const;
    const char* debugResultKindName() const;
    const char* debugTutorStatusName() const;
    bool debugAssert(const std::string& expected);
#endif
private:
    enum class State : uint8_t { EQ_LIST, TEMPLATE, EDITING, SOLVING, RESULT, STEPS };
    enum class ResultKind : uint8_t { None, Structured, TextFallback };
    enum class TutorStatus : uint8_t { Disabled, Agreed, Unavailable };
    static constexpr int MAX_EQS = 3;
    static constexpr int MAX_RESULTS = 4;
    static constexpr int NUM_TEMPLATES = 4;
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _title = nullptr;
    lv_obj_t* _body = nullptr;
    lv_obj_t* _hint = nullptr;
    lv_obj_t* _variableMenu = nullptr;
    ui::StatusBar _statusBar;
    std::array<lv_obj_t*, 5> _rows{};
    std::array<vpam::MathCanvas, MAX_RESULTS> _canvas;
    std::array<vpam::NodePtr, MAX_RESULTS> _viewNodes;
    vpam::NodePtr _eqNode[MAX_EQS];
    vpam::NodeRow* _eqRowData[MAX_EQS]{};
    vpam::NodePtr _editNode;
    vpam::NodeRow* _editRow = nullptr;
    bool _draftTouched = false;
    vpam::CursorController _editCursor;
    int _editingIndex = -1;
    int _numEquations = 0;
    int _listFocus = 0;
    int _templateFocus = 0;
    int _variableFocus = 0;
    int _followCursor = 0;
    int _page = 0;
    int _errorRow = -1;
    State _state = State::EQ_LIST;
    uint32_t _equationEpoch = 1, _solveEpoch = 0, _stepsEpoch = 0;
    numos::StructuredSolveResult _giacResult;
    ResultKind _resultKind = ResultKind::None;
    TutorStatus _tutorStatus = TutorStatus::Disabled;
    std::string _tutorDiagnostic;
    cas::SymExprArena _arena;
    cas::OmniResult _omniResult;
    cas::SystemResult _systemResult;
    void clearView();
    void header(const char* title, const char* hint);
    lv_obj_t* text(lv_obj_t* parent, const char* value, int x, int y, int width = 0);
    int formula(int slot, vpam::NodeRow* row, int y, const char* label = nullptr);
    void showEqList();
    void showTemplate();
    void showEditing(int index, int templateIndex = -1);
    void refreshEditor();
    void confirmDraft();
    void cancelDraft();
    void invalidateAnswer();
    void updateFocus();
    void showVariables();
    void closeVariables();
    void solveEquations();
    void showResult();
    void showSteps();
    void handleEditor(const KeyEvent& ev);
    int listItemCount() const;
    vpam::NodePtr buildTemplateAST(int index);
    bool splitAtEquals(vpam::NodeRow*, vpam::NodePtr&, vpam::NodePtr&);
    cas::LinEq symEquationToLinEq(const cas::SymEquation&, const char*, int);
    void generateTutorCandidate();
    bool tutorCandidateAgrees() const;
};
