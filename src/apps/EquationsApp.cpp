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
 * EquationsApp.cpp — Equation Solver App for NumOS.
 *
 * NumWorks-inspired redesign:
 *   EQ_LIST → TEMPLATE/EDITING → SOLVING → RESULT → STEPS
 *
 * Features:
 *   - Equation list with up to 3 slots + live MathCanvas preview
 *   - Template dropdown with preset equations
 *   - "Add an equation" and "Solve" action rows
 *   - Full MathCanvas editor with VPAM cursor
 *   - OmniSolver (single eq) / SystemSolver (2x2/3x3 systems)
 *   - Step-by-step CAS display
 *
 * Part of: NumOS — Equation Solver
 */

#include "EquationsApp.h"
#include "../Config.h"
#include "../math/MathAST.h"
#include "../math/cas/SymToAST.h"
#include "../math/cas/SymExprToAST.h"
#include "../math/cas/SymExprArena.h"
#include "../math/cas/AlgebraicRules.h"
#include "../math/cas/CasToVpam.h"
#include "../math/cas/PersistentAST.h"
#include "../math/cas/RuleEngine.h"
#include "../math/cas/TutorTemplates.h"
#include "../utils/HwUxProbe.h"
#ifdef ARDUINO
#include "Arduino.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include "../hal/ArduinoCompat.h"
#endif
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath>

using namespace vpam;

// ════════════════════════════════════════════════════════════════════════════
// Layout constants (320×240 display)
// ════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t COL_BG_HEX      = 0xFFFFFF;
static constexpr uint32_t COL_SEP_HEX     = 0x333333;
static constexpr uint32_t COL_ACCENT_HEX  = 0xE05500;  // Orange accent
static constexpr uint32_t COL_FOCUS_HEX   = 0x4A90D9;  // Blue focus
static constexpr uint32_t COL_HINT_HEX    = 0x888888;
static constexpr uint32_t COL_ROW_BG_HEX  = 0xF5F5F5;  // Light grey row background
static constexpr uint32_t COL_ROW_SEL_HEX = 0xE3F2FD;  // Light blue selected row
static constexpr uint32_t COL_SOLVE_HEX   = 0xE05500;  // Orange solve button
static constexpr uint32_t COL_TMPL_BG_HEX = 0xF0F0F0;  // Template overlay background
static constexpr uint32_t COL_STEP_HEX    = 0x1A1A1A;
static constexpr uint32_t COL_DESC_HEX    = 0x2E7D32;

static constexpr int SCREEN_W  = 320;
static constexpr int SCREEN_H  = 240;
static constexpr int BAR_H     = ui::StatusBar::HEIGHT + 1;
static constexpr int PAD       = 8;
static constexpr int ROW_H     = 36;   // Height of each equation row
static constexpr int ROW_GAP   = 3;    // Gap between rows

// ════════════════════════════════════════════════════════════════════════════
// Template labels
// ════════════════════════════════════════════════════════════════════════════

const char* EquationsApp::TEMPLATE_LABELS[NUM_TEMPLATES] = {
    "Empty",
    "Polynomial:",
    "Exponential:",
    "Logarithmic:",
};

// ════════════════════════════════════════════════════════════════════════════
// Loading messages (randomly selected)
// ════════════════════════════════════════════════════════════════════════════

static const char* SOLVING_MESSAGES[] = {
    "Solving...",
    "Computing roots...",
    "Finding solutions...",
    "Analyzing equation...",
    "Newton's method...",
    "Deriving symbolically...",
    "Almost there...",
    "Crunching numbers...",
    "Iterating...",
    "Converging...",
    "Factoring polynomial...",
};
static constexpr int NUM_SOLVING_MSGS = sizeof(SOLVING_MESSAGES) / sizeof(SOLVING_MESSAGES[0]);

const char* EquationsApp::randomSolvingMessage() {
    return SOLVING_MESSAGES[rand() % NUM_SOLVING_MSGS];
}

#ifdef NATIVE_SIM
const char* EquationsApp::debugEngineName() const {
    return "giac";
}

const char* EquationsApp::debugStatusName() const {
    if (_giacResult.status == numos::MathEngineStatus::Ok) {
        if (_giacResult.setKind == numos::SolutionSetKind::NoSolution)
            return "no_solution";
        if (_giacResult.setKind == numos::SolutionSetKind::AllValues)
            return "all_values";
        if (_giacResult.setKind == numos::SolutionSetKind::Unsupported)
            return "unsupported";
        return "ok";
    }
    switch (_giacResult.status) {
        case numos::MathEngineStatus::ParseError: return "parse_error";
        case numos::MathEngineStatus::Undefined: return "undefined";
        case numos::MathEngineStatus::EvaluationError:
            return "evaluation_error";
        case numos::MathEngineStatus::OutOfMemory: return "out_of_memory";
        default: return "unsupported";
    }
}

int EquationsApp::debugSolutionCount() const {
    return static_cast<int>(_giacResult.groups.size());
}

bool EquationsApp::debugSolutionNear(const std::string& variable, int index,
                                     double expected,
                                     double epsilon) const {
    if (index < 0 || index >= static_cast<int>(_giacResult.groups.size()))
        return false;
    const auto& group = _giacResult.groups[static_cast<std::size_t>(index)];
    for (const auto& value : group.values) {
        if (value.variable != variable) continue;
        double actual = 0.0;
        if (value.hasApproximateReal) {
            actual = value.approximateReal;
        } else {
            vpam::ExactVal exact;
            if (!numos::CalculationEngine::resultTreeToExactVal(
                    value.exactValue, exact)) {
                return false;
            }
            actual = exact.toDouble();
        }
        return std::isfinite(actual) &&
               std::fabs(actual - expected) <= epsilon;
    }
    return false;
}

bool EquationsApp::debugSolutionExact(const std::string& variable, int index,
                                      const std::string& expected) const {
    if (index < 0 || index >= static_cast<int>(_giacResult.groups.size()))
        return false;
    const auto& group = _giacResult.groups[static_cast<std::size_t>(index)];
    for (const auto& value : group.values) {
        if (value.variable == variable) return value.exactText == expected;
    }
    return false;
}

std::string EquationsApp::debugSolutionExactText(
    const std::string& variable, int index) const {
    if (index < 0 || index >= static_cast<int>(_giacResult.groups.size()))
        return {};
    for (const auto& value :
         _giacResult.groups[static_cast<std::size_t>(index)].values) {
        if (value.variable == variable) return value.exactText;
    }
    return {};
}

const char* EquationsApp::debugResultKindName() const {
    switch (_resultKind) {
        case ResultKind::Structured: return "structured";
        case ResultKind::TextFallback: return "text_fallback";
        default: return "none";
    }
}

const char* EquationsApp::debugTutorStatusName() const {
    switch (_tutorStatus) {
        case TutorStatus::Agreed: return "agreed";
        case TutorStatus::Unavailable: return "unavailable";
        default: return "disabled";
    }
}
#endif

// ════════════════════════════════════════════════════════════════════════════
// CAS helpers — VPAM NodeRow → equation text → cas::NodePtr
// ════════════════════════════════════════════════════════════════════════════

namespace {

/// Recursively converts a VPAM MathNode tree to a simple infix text string
/// suitable for the TRS equation parser.
static void nodeToText(const MathNode* node, std::string& out) {
    if (!node) return;
    switch (node->type()) {
        case NodeType::Row: {
            const auto* r = static_cast<const NodeRow*>(node);
            for (int i = 0; i < r->childCount(); ++i)
                nodeToText(r->child(i), out);
            break;
        }
        case NodeType::Number: {
            const auto* n = static_cast<const NodeNumber*>(node);
            out += n->value();
            break;
        }
        case NodeType::Variable: {
            const auto* v = static_cast<const NodeVariable*>(node);
            out += v->name();
            break;
        }
        case NodeType::Operator: {
            const auto* op = static_cast<const NodeOperator*>(node);
            switch (op->op()) {
                case OpKind::Add: out += "+"; break;
                case OpKind::Sub: out += "-"; break;
                case OpKind::Mul: out += "*"; break;
                default:          out += "+"; break;
            }
            break;
        }
        case NodeType::Power: {
            // NodePower has exactly two slot children: base, exponent
            if (node->childCount() >= 2) {
                nodeToText(node->child(0), out);
                out += "^";
                nodeToText(node->child(1), out);
            }
            break;
        }
        case NodeType::Fraction: {
            if (node->childCount() >= 2) {
                out += "(";
                nodeToText(node->child(0), out);
                out += ")/(";
                nodeToText(node->child(1), out);
                out += ")";
            }
            break;
        }
        default:
            break;
    }
}

// ── Equation parser (migrated from TutorApp, Phase 13C) ──────────────────────
// Supports: digits | '.' | single-letter variable | + - * ^ () =
// Precedence: ^ (right) > unary -/+ > * > + -
// Note: 'e'/'E' are excluded from variable detection to avoid conflicts
//       with exponential notation in number literals.  Scientific notation
//       itself is not supported; inputs like "2e5" are not parsed.

struct CasParseCtx {
    const char* src;
    std::size_t pos;
    cas::CasMemoryPool& pool;
    char var;
    char peek() const { return src[pos]; }
    char eat()  { return src[pos++]; }
    void skipWS() { while (src[pos] == ' ') ++pos; }
};

static cas::NodePtr casParseExpr(CasParseCtx& ctx);
static cas::NodePtr casParseTerm(CasParseCtx& ctx);
static cas::NodePtr casParsePower(CasParseCtx& ctx);
static cas::NodePtr casParseUnary(CasParseCtx& ctx);
static cas::NodePtr casParseAtom(CasParseCtx& ctx);

static cas::NodePtr casParseExpr(CasParseCtx& ctx) {
    ctx.skipWS();
    cas::NodePtr lhs = casParseTerm(ctx);
    while (true) {
        ctx.skipWS();
        char c = ctx.peek();
        if (c != '+' && c != '-') break;
        ctx.eat(); ctx.skipWS();
        cas::NodePtr rhs = casParseTerm(ctx);
        if (c == '-') rhs = cas::makeNegation(ctx.pool, std::move(rhs));
        std::vector<cas::NodePtr> kids;
        if (lhs && lhs->isSum()) {
            const auto* s = static_cast<const cas::SumNode*>(lhs.get());
            for (const auto& ch : s->children) kids.push_back(ch);
        } else { kids.push_back(std::move(lhs)); }
        kids.push_back(std::move(rhs));
        lhs = cas::makeSum(ctx.pool, std::move(kids));
    }
    return lhs;
}

static cas::NodePtr casParseTerm(CasParseCtx& ctx) {
    ctx.skipWS();
    cas::NodePtr lhs = casParsePower(ctx);
    while (true) {
        ctx.skipWS();
        char c = ctx.peek();
        if (c == '*') {
            ctx.eat(); ctx.skipWS();
            cas::NodePtr rhs = casParsePower(ctx);
            std::vector<cas::NodePtr> kids;
            if (lhs && lhs->isProduct()) {
                const auto* p = static_cast<const cas::ProductNode*>(lhs.get());
                for (const auto& ch : p->children) kids.push_back(ch);
            } else { kids.push_back(std::move(lhs)); }
            kids.push_back(std::move(rhs));
            lhs = cas::makeProduct(ctx.pool, std::move(kids));
            continue;
        }
        if (lhs) {
            bool lhsConst = lhs->isConstant();
            bool rhsVar   = std::isalpha(c) && c != 'e' && c != 'E';
            bool rhsParen = (c == '(');
            if (lhsConst && (rhsVar || rhsParen)) {
                cas::NodePtr rhs = casParsePower(ctx);
                std::vector<cas::NodePtr> kids;
                if (lhs->isProduct()) {
                    const auto* p = static_cast<const cas::ProductNode*>(lhs.get());
                    for (const auto& ch : p->children) kids.push_back(ch);
                } else { kids.push_back(std::move(lhs)); }
                kids.push_back(std::move(rhs));
                lhs = cas::makeProduct(ctx.pool, std::move(kids));
                continue;
            }
        }
        break;
    }
    return lhs;
}

static cas::NodePtr casParsePower(CasParseCtx& ctx) {
    ctx.skipWS();
    cas::NodePtr base = casParseUnary(ctx);
    ctx.skipWS();
    if (ctx.peek() == '^') {
        ctx.eat(); ctx.skipWS();
        cas::NodePtr exp = casParseUnary(ctx);
        return cas::makePower(ctx.pool, std::move(base), std::move(exp));
    }
    return base;
}

static cas::NodePtr casParseUnary(CasParseCtx& ctx) {
    ctx.skipWS();
    if (ctx.peek() == '-') {
        ctx.eat(); ctx.skipWS();
        cas::NodePtr inner = casParseAtom(ctx);
        return cas::makeNegation(ctx.pool, std::move(inner));
    }
    if (ctx.peek() == '+') { ctx.eat(); ctx.skipWS(); }
    return casParseAtom(ctx);
}

static cas::NodePtr casParseAtom(CasParseCtx& ctx) {
    ctx.skipWS();
    char c = ctx.peek();
    if (c == '(') {
        ctx.eat();
        cas::NodePtr inner = casParseExpr(ctx);
        ctx.skipWS();
        if (ctx.peek() == ')') ctx.eat();
        return inner;
    }
    if (std::isdigit(c) || c == '.') {
        std::string buf;
        while (std::isdigit(ctx.peek()) || ctx.peek() == '.') buf += ctx.eat();
        return cas::makeConstant(ctx.pool, std::stod(buf));
    }
    if (std::isalpha(c)) {
        ctx.eat();
        return cas::makeVariable(ctx.pool, c);
    }
    return cas::makeConstant(ctx.pool, 0.0);
}

/// Parse "lhsExpr = rhsExpr" into a cas::EquationNode.  Returns nullptr on error.
static cas::NodePtr casParseEquation(const std::string& text,
                                     cas::CasMemoryPool& pool,
                                     char& varOut) {
    auto eqPos = text.find('=');
    if (eqPos == std::string::npos) return nullptr;
    std::string lhsStr = text.substr(0, eqPos);
    std::string rhsStr = text.substr(eqPos + 1);
    varOut = 'x';
    for (char ch : text) { if (std::isalpha(ch)) { varOut = ch; break; } }
    CasParseCtx lhsCtx{lhsStr.c_str(), 0, pool, varOut};
    CasParseCtx rhsCtx{rhsStr.c_str(), 0, pool, varOut};
    cas::NodePtr lhs = casParseExpr(lhsCtx);
    cas::NodePtr rhs = casParseExpr(rhsCtx);
    if (!lhs || !rhs) return nullptr;
    return cas::makeEquation(pool, std::move(lhs), std::move(rhs));
}

} // anonymous namespace

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

EquationsApp::EquationsApp()
    : _screen(nullptr)
    , _listContainer(nullptr)
    , _addRow(nullptr), _addLabel(nullptr)
    , _solveRow(nullptr), _solveLabel(nullptr)
    , _listHint(nullptr)
    , _numEquations(0)
    , _listFocus(0)
    , _templateOverlay(nullptr), _templateTitle(nullptr)
    , _templateFocus(0)
    , _editContainer(nullptr), _editTitle(nullptr), _editHint(nullptr)
    , _editRow(nullptr), _editingIndex(-1)
    , _solvingContainer(nullptr), _solvingSpinner(nullptr), _solvingLabel(nullptr)
    , _resultContainer(nullptr), _resultTitle(nullptr), _resultHint(nullptr)
    , _resultFallback(nullptr)
    , _resultCount(0)
    , _stepsContainer(nullptr)
    , _state(State::EQ_LIST)
    , _stepScroll(0)
    , _isOmniSolve(true)
{
    for (int i = 0; i < MAX_EQS; ++i) {
        _eqRows[i] = nullptr;
        _eqLabels[i] = nullptr;
        _eqRowData[i] = nullptr;
    }
    for (int i = 0; i < NUM_TEMPLATES; ++i) {
        _templateItems[i] = nullptr;
        _templateLabels[i] = nullptr;
    }
    for (int i = 0; i < MAX_RESULTS; ++i) _resultRow[i] = nullptr;
}

EquationsApp::~EquationsApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::begin() {
    if (_screen) return;
    createUI();
    _state = State::EQ_LIST;
    showEqList();
}

void EquationsApp::resetStepsPipeline() {
    _stepsSource = StepsSource::NONE;
    _stepsStage = StepsStage::IDLE;
    _stepsPipelineActive = false;
    _stepsReachedFixedPoint = false;
    _stepsMemoryLimitReached = false;
    _stepsHeaderRendered = false;
    _stepsPipelineError.clear();
    _stepsEqText1.clear();
    _stepsEqText2.clear();
    _stepsVar1 = 'x';
    _stepsVar2 = 'y';
    _stepsParsedEq1.reset();
    _stepsParsedEq2.reset();
    _stepsCurrentTree.reset();
    _stepsCasPendingLogs.clear();
    _stepsSystemPendingLogs.clear();
    _stepsTutorResult = cas::SolveResult();
    _stepsTutorActive = false;
    _stepsTutorArena.reset();
    _stepsSolveIterations = 0;
    _stepsRenderIndex = 0;
    _stepsProgressLabel = nullptr;
}

void EquationsApp::invalidateStepsCache() {
    ++_equationEpoch;
    _stepsEpoch = 0;
    resetStepsPipeline();
}

void EquationsApp::end() {
    _editCanvas.stopCursorBlink();
    _editCanvas.destroy();
    _editNode.reset();
    _editRow = nullptr;

    for (int i = 0; i < MAX_EQS; ++i) {
        _eqPreview[i].destroy();
        _eqNode[i].reset();
        _eqRowData[i] = nullptr;
    }
    for (int i = 0; i < MAX_RESULTS; ++i) {
        _resultCanvas[i].destroy();
        _resultNode[i].reset();
        _resultRow[i] = nullptr;
    }
    for (int i = 0; i < NUM_TEMPLATES; ++i) {
        _templateCanvas[i].destroy();
        _templateNode[i].reset();
        _templateItems[i] = nullptr;
        _templateLabels[i] = nullptr;
    }
    _statusBar.destroy();

    _stepRenderers.clear();
    resetStepsPipeline();

    if (_screen) {
        lv_obj_delete(_screen);
        _screen            = nullptr;
        _listContainer     = nullptr;
        _addRow            = nullptr;
        _addLabel          = nullptr;
        _solveRow          = nullptr;
        _solveLabel        = nullptr;
        _listHint          = nullptr;
        _templateOverlay   = nullptr;
        _templateTitle     = nullptr;
        _editContainer     = nullptr;
        _editTitle         = nullptr;
        _editHint          = nullptr;
        _solvingContainer  = nullptr;
        _solvingSpinner    = nullptr;
        _solvingLabel      = nullptr;
        _resultContainer   = nullptr;
        _resultTitle       = nullptr;
        _resultHint        = nullptr;
        _resultFallback    = nullptr;
        _stepsContainer    = nullptr;
        for (int i = 0; i < MAX_EQS; ++i) {
            _eqRows[i] = nullptr;
            _eqLabels[i] = nullptr;
        }
        for (int i = 0; i < NUM_TEMPLATES; ++i) {
            _templateItems[i] = nullptr;
            _templateLabels[i] = nullptr;
        }
    }

    _state = State::EQ_LIST;
    _numEquations = 0;
    _listFocus = 0;
    _resultCount = 0;
    _equationEpoch = 1;
    _solveEpoch = 0;
    _stepsEpoch = 0;

    _omniResult.steps.clear();
    _systemResult.steps.clear();
    _arena.reset();
}

void EquationsApp::load() {
    // Lifecycle hardening: if any critical container pointer is missing while
    // screen exists, rebuild atomically to avoid partial trees after teardown races.
    if (!_screen || !_listContainer || !_resultContainer || !_stepsContainer) {
        end();
        begin();
    }
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();

    if (_state == State::EDITING && _editCanvas.obj()) {
        _editCanvas.startCursorBlink();
    }
}

bool EquationsApp::isValidLvObj(const lv_obj_t* obj) const {
    return obj != nullptr && lv_obj_is_valid(obj);
}

bool EquationsApp::canAllocStep(std::size_t requiredContiguous) const {
#ifdef ARDUINO
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const std::size_t internalLargestBlock =
        static_cast<std::size_t>(heap_caps_get_largest_free_block(caps));
    const std::size_t internalFree =
        static_cast<std::size_t>(heap_caps_get_free_size(caps));

    if (isValidLvObj(_stepsContainer)) {
        const std::size_t childCount =
            static_cast<std::size_t>(lv_obj_get_child_count(_stepsContainer));
        if (childCount >= STEPS_CHILD_HARD_CAP) return false;
    }

    if (internalLargestBlock < STEPS_INTERNAL_MAXBLOCK_FLOOR) return false;
    if (internalLargestBlock < requiredContiguous) return false;
    if (internalFree < requiredContiguous) return false;
    return true;
#else
    (void)requiredContiguous;
    return !isValidLvObj(_stepsContainer) ||
           static_cast<std::size_t>(lv_obj_get_child_count(_stepsContainer)) <
               STEPS_CHILD_HARD_CAP;
#endif
}

void EquationsApp::logStepBudget(int stepIndex) const {
#ifdef ARDUINO
    int children = 0;
    if (isValidLvObj(_stepsContainer)) {
        children = static_cast<int>(lv_obj_get_child_count(_stepsContainer));
    }

    const uint32_t internalFree =
        static_cast<uint32_t>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t internalMaxBlock =
        static_cast<uint32_t>(
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t freePsram =
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    Serial.printf(
        "[DEBUG] Step:%d | Children:%d | InternalFree:%u | InternalMaxBlock:%u | PSRAMFree:%u.\n",
        stepIndex, children, internalFree, internalMaxBlock, freePsram);
#else
    (void)stepIndex;
#endif
}

void EquationsApp::failClosedStepRender(std::size_t stepIndex) {
    _stepsMemoryLimitReached = true;
    _stepsPipelineError.clear();

    if (isValidLvObj(_stepsProgressLabel)) {
        char warn[96];
        snprintf(warn, sizeof(warn),
                 "\xE2\x9A\xA0\xEF\xB8\x8F Limit reached at step %u. Memory full.",
                 static_cast<unsigned>(stepIndex));
        lv_label_set_text(_stepsProgressLabel, warn);
        lv_obj_remove_flag(_stepsProgressLabel, LV_OBJ_FLAG_HIDDEN);
    }

    _stepsStage = StepsStage::FINALIZE;
}

// ════════════════════════════════════════════════════════════════════════════
// UI Creation
// ════════════════════════════════════════════════════════════════════════════

static lv_obj_t* createRowContainer(lv_obj_t* parent, int y, int w, int h,
                                     uint32_t bgColor, bool rounded = true) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, w, h);
    lv_obj_set_pos(row, PAD, y);
    lv_obj_set_style_bg_color(row, lv_color_hex(bgColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    if (rounded) {
        lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
    }
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

void EquationsApp::createUI() {
    // ── Screen ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── StatusBar ──
    _statusBar.create(_screen);
    _statusBar.setTitle("Equations");
    _statusBar.setBatteryLevel(100);

    // ─────────────────────────────────────────────────────────────────
    // EQ_LIST container
    // ─────────────────────────────────────────────────────────────────
    _listContainer = lv_obj_create(_screen);
    lv_obj_set_size(_listContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_listContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_listContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_listContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_listContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_listContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Equation rows (E1, E2, E3) — all created but hidden initially
    int rowW = SCREEN_W - 2 * PAD;
    for (int i = 0; i < MAX_EQS; ++i) {
        int y = 8 + i * (ROW_H + ROW_GAP);
        _eqRows[i] = createRowContainer(_listContainer, y, rowW, ROW_H, COL_ROW_BG_HEX);
        lv_obj_add_flag(_eqRows[i], LV_OBJ_FLAG_HIDDEN);

        // Label "E1:", "E2:", "E3:"
        _eqLabels[i] = lv_label_create(_eqRows[i]);
        char buf[8];
        snprintf(buf, sizeof(buf), "E%d:", i + 1);
        lv_label_set_text(_eqLabels[i], buf);
        lv_obj_set_style_text_font(_eqLabels[i], &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(_eqLabels[i], lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
        lv_obj_set_pos(_eqLabels[i], 8, (ROW_H - 14) / 2);

        // Preview MathCanvas (small, read-only)
        _eqPreview[i].create(_eqRows[i]);
        lv_obj_set_pos(_eqPreview[i].obj(), 38, 2);
        lv_obj_set_size(_eqPreview[i].obj(), rowW - 48, ROW_H - 4);
    }

    // "Add an equation" row
    int addY = 8;  // Position updated dynamically in showEqList()
    _addRow = createRowContainer(_listContainer, addY, rowW, ROW_H, COL_ROW_BG_HEX);
    _addLabel = lv_label_create(_addRow);
    lv_label_set_text(_addLabel, LV_SYMBOL_PLUS "  Add an equation");
    lv_obj_set_style_text_font(_addLabel, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_addLabel, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_align(_addLabel, LV_ALIGN_LEFT_MID, 10, 0);

    // "Solve" button row
    _solveRow = createRowContainer(_listContainer, addY + ROW_H + ROW_GAP, rowW, ROW_H, COL_SOLVE_HEX);
    _solveLabel = lv_label_create(_solveRow);
    lv_label_set_text(_solveLabel, "Solve the equation");
    lv_obj_set_style_text_font(_solveLabel, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_solveLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(_solveLabel, LV_ALIGN_CENTER, 0, 0);

    // Hint at bottom
    _listHint = lv_label_create(_listContainer);
    lv_label_set_text(_listHint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   ENTER Select   DEL Remove");
    lv_obj_set_style_text_font(_listHint, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_listHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_listHint, PAD, SCREEN_H - BAR_H - 20);

    // ─────────────────────────────────────────────────────────────────
    // TEMPLATE overlay
    // ─────────────────────────────────────────────────────────────────
    _templateOverlay = lv_obj_create(_screen);
    lv_obj_set_size(_templateOverlay, 260, 150);
    lv_obj_align(_templateOverlay, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(_templateOverlay, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_templateOverlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_templateOverlay, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_set_style_border_width(_templateOverlay, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_templateOverlay, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_templateOverlay, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(_templateOverlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(_templateOverlay, 60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_templateOverlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_templateOverlay, LV_OBJ_FLAG_SCROLLABLE);

    _templateTitle = lv_label_create(_templateOverlay);
    lv_label_set_text(_templateTitle, "Use a template:");
    lv_obj_set_style_text_font(_templateTitle, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_templateTitle, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_templateTitle, 12, 8);

    for (int i = 0; i < NUM_TEMPLATES; ++i) {
        // Row container for text + MathCanvas side by side
        _templateItems[i] = lv_obj_create(_templateOverlay);
        lv_obj_set_size(_templateItems[i], 236, 24);
        lv_obj_set_pos(_templateItems[i], 12, 30 + i * 26);
        lv_obj_set_style_bg_opa(_templateItems[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(_templateItems[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_templateItems[i], 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(_templateItems[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(_templateItems[i], LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(_templateItems[i], 4, LV_PART_MAIN);
        lv_obj_remove_flag(_templateItems[i], LV_OBJ_FLAG_SCROLLABLE);

        // Text prefix label
        _templateLabels[i] = lv_label_create(_templateItems[i]);
        lv_label_set_text(_templateLabels[i], TEMPLATE_LABELS[i]);
        lv_obj_set_style_text_font(_templateLabels[i], &stix_math_18, LV_PART_MAIN);

        // MathCanvas for the formula (templates 1-3 have math)
        if (i > 0) {
            _templateNode[i] = buildTemplateAST(i);
            auto* row = static_cast<vpam::NodeRow*>(_templateNode[i].get());
            _templateCanvas[i].create(_templateItems[i]);
            _templateCanvas[i].setExpression(row, nullptr);
            row->calculateLayout(_templateCanvas[i].normalMetrics());
            int16_t cw = row->layout().width + 8;
            int16_t ch = vpam::mathObjectHeightPx(row->layout(), _templateCanvas[i].normalMetrics(), 4);
            if (ch < 20) ch = 20;
            if (cw > 160) cw = 160;
            lv_obj_set_size(_templateCanvas[i].obj(), cw, ch);
            _templateCanvas[i].invalidate();
        }
    }

    lv_obj_add_flag(_templateOverlay, LV_OBJ_FLAG_HIDDEN);

    // ─────────────────────────────────────────────────────────────────
    // EDITING container
    // ─────────────────────────────────────────────────────────────────
    _editContainer = lv_obj_create(_screen);
    lv_obj_set_size(_editContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_editContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_editContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_editContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_editContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_editContainer, LV_OBJ_FLAG_SCROLLABLE);

    _editTitle = lv_label_create(_editContainer);
    lv_obj_set_style_text_font(_editTitle, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_editTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_editTitle, PAD, 4);

    _editCanvas.create(_editContainer);
    lv_obj_set_pos(_editCanvas.obj(), PAD + 4, 28);
    lv_obj_set_size(_editCanvas.obj(), SCREEN_W - 2 * PAD - 8, 120);

    _editHint = lv_label_create(_editContainer);
    lv_label_set_text(_editHint, LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Move   "
                                  "= Equals   ENTER Confirm   AC Cancel");
    lv_obj_set_style_text_font(_editHint, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_editHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_editHint, PAD, SCREEN_H - BAR_H - 20);

    lv_obj_add_flag(_editContainer, LV_OBJ_FLAG_HIDDEN);

    // ─────────────────────────────────────────────────────────────────
    // SOLVING container (spinner + message)
    // ─────────────────────────────────────────────────────────────────
    _solvingContainer = lv_obj_create(_screen);
    lv_obj_set_size(_solvingContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_solvingContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_solvingContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_solvingContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_solvingContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_solvingContainer, LV_OBJ_FLAG_SCROLLABLE);

    _solvingLabel = lv_label_create(_solvingContainer);
    lv_label_set_text(_solvingLabel, "Solving...");
    lv_obj_set_style_text_font(_solvingLabel, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_solvingLabel, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_align(_solvingLabel, LV_ALIGN_CENTER, 0, -20);

    _solvingSpinner = lv_spinner_create(_solvingContainer);
    lv_spinner_set_anim_params(_solvingSpinner, 1000, 200);
    lv_obj_set_size(_solvingSpinner, 40, 40);
    lv_obj_align(_solvingSpinner, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_arc_color(_solvingSpinner,
                               lv_color_hex(COL_ACCENT_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_solvingSpinner,
                               lv_color_hex(0xDDDDDD), LV_PART_MAIN);

    // ─────────────────────────────────────────────────────────────────
    // RESULT container
    // ─────────────────────────────────────────────────────────────────
    _resultContainer = lv_obj_create(_screen);
    lv_obj_set_size(_resultContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_resultContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_resultContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_resultContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_SCROLLABLE);

    _resultTitle = lv_label_create(_resultContainer);
    lv_obj_set_style_text_font(_resultTitle, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_resultTitle, PAD, 4);

    _resultFallback = lv_label_create(_resultContainer);
    lv_obj_set_pos(_resultFallback, PAD + 4, 32);
    lv_obj_set_width(_resultFallback, SCREEN_W - 2 * PAD - 8);
    lv_label_set_long_mode(_resultFallback, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(_resultFallback, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultFallback, lv_color_hex(COL_SEP_HEX),
                                LV_PART_MAIN);
    lv_obj_add_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < MAX_RESULTS; ++i) {
        _resultCanvas[i].create(_resultContainer);
        lv_obj_add_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
    }

    // ─────────────────────────────────────────────────────────────────
    // STEPS container (scrollable)
    // ─────────────────────────────────────────────────────────────────
    _stepsContainer = lv_obj_create(_screen);
    lv_obj_set_size(_stepsContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_stepsContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_stepsContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_stepsContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_stepsContainer, PAD, LV_PART_MAIN);
    lv_obj_set_flex_flow(_stepsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_stepsContainer, 4, LV_PART_MAIN);
    lv_obj_add_flag(_stepsContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_stepsContainer, LV_DIR_VER);

    // ── Start hidden ──
    hideAllContainers();
}
// ════════════════════════════════════════════════════════════════════════════
// Container visibility
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::hideAllContainers() {
    if (_listContainer)     lv_obj_add_flag(_listContainer,     LV_OBJ_FLAG_HIDDEN);
    if (_templateOverlay)   lv_obj_add_flag(_templateOverlay,   LV_OBJ_FLAG_HIDDEN);
    if (_editContainer)     lv_obj_add_flag(_editContainer,     LV_OBJ_FLAG_HIDDEN);
    if (_solvingContainer)  lv_obj_add_flag(_solvingContainer,  LV_OBJ_FLAG_HIDDEN);
    if (_resultContainer)   lv_obj_add_flag(_resultContainer,   LV_OBJ_FLAG_HIDDEN);
    if (_stepsContainer)    lv_obj_add_flag(_stepsContainer,    LV_OBJ_FLAG_HIDDEN);

    _editCanvas.stopCursorBlink();
}

// ════════════════════════════════════════════════════════════════════════════
// List item count & focus management
// ════════════════════════════════════════════════════════════════════════════

int EquationsApp::listItemCount() const {
    // Items: existing equations + "Add" (if < MAX_EQS) + "Solve" (if > 0 equations)
    int count = _numEquations;
    if (_numEquations < MAX_EQS) count++;  // "Add an equation"
    if (_numEquations > 0) count++;         // "Solve"
    if (count == 0) count = 1;  // At least the "Add" button
    return count;
}

void EquationsApp::updateListFocus() {
    // Determine what each virtual position maps to
    int total = listItemCount();
    if (_listFocus >= total) _listFocus = total - 1;
    if (_listFocus < 0) _listFocus = 0;

    int addIdx = _numEquations;  // position of "Add" row
    int solveIdx = -1;

    if (_numEquations > 0 && _numEquations < MAX_EQS) {
        solveIdx = _numEquations + 1;
    } else if (_numEquations > 0) {
        solveIdx = _numEquations;  // no "Add" when at MAX_EQS
    }

    bool hasAdd = (_numEquations < MAX_EQS);

    // Layout: position equation rows
    int rowW = SCREEN_W - 2 * PAD;
    int y = 8;

    for (int i = 0; i < MAX_EQS; ++i) {
        if (i < _numEquations) {
            lv_obj_remove_flag(_eqRows[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(_eqRows[i], PAD, y);

            // Focus style
            if (_listFocus == i) {
                lv_obj_set_style_bg_color(_eqRows[i], lv_color_hex(COL_ROW_SEL_HEX), LV_PART_MAIN);
                lv_obj_set_style_border_width(_eqRows[i], 2, LV_PART_MAIN);
                lv_obj_set_style_border_color(_eqRows[i], lv_color_hex(COL_FOCUS_HEX), LV_PART_MAIN);
            } else {
                lv_obj_set_style_bg_color(_eqRows[i], lv_color_hex(COL_ROW_BG_HEX), LV_PART_MAIN);
                lv_obj_set_style_border_width(_eqRows[i], 0, LV_PART_MAIN);
            }
            y += ROW_H + ROW_GAP;
        } else {
            lv_obj_add_flag(_eqRows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // "Add an equation" row
    if (hasAdd) {
        lv_obj_remove_flag(_addRow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(_addRow, PAD, y);
        if (_listFocus == addIdx) {
            lv_obj_set_style_bg_color(_addRow, lv_color_hex(COL_ROW_SEL_HEX), LV_PART_MAIN);
            lv_obj_set_style_border_width(_addRow, 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(_addRow, lv_color_hex(COL_FOCUS_HEX), LV_PART_MAIN);
            lv_obj_set_style_text_color(_addLabel, lv_color_hex(COL_FOCUS_HEX), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_addRow, lv_color_hex(COL_ROW_BG_HEX), LV_PART_MAIN);
            lv_obj_set_style_border_width(_addRow, 0, LV_PART_MAIN);
            lv_obj_set_style_text_color(_addLabel, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        }
        y += ROW_H + ROW_GAP;
    } else {
        lv_obj_add_flag(_addRow, LV_OBJ_FLAG_HIDDEN);
    }

    // "Solve" button row
    if (_numEquations > 0) {
        lv_obj_remove_flag(_solveRow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(_solveRow, PAD, y);

        // Update label text based on number of equations
        if (_numEquations == 1) {
            lv_label_set_text(_solveLabel, "Solve the equation");
        } else {
            lv_label_set_text(_solveLabel, "Solve the system");
        }

        if (_listFocus == solveIdx) {
            lv_obj_set_style_bg_color(_solveRow, lv_color_hex(0xC04000), LV_PART_MAIN);
            lv_obj_set_style_border_width(_solveRow, 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(_solveRow, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_solveRow, lv_color_hex(COL_SOLVE_HEX), LV_PART_MAIN);
            lv_obj_set_style_border_width(_solveRow, 0, LV_PART_MAIN);
        }
    } else {
        lv_obj_add_flag(_solveRow, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_invalidate(_screen);
}

// ════════════════════════════════════════════════════════════════════════════
// State transitions
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::showEqList() {
    resetStepsPipeline();
    hideAllContainers();
    _state = State::EQ_LIST;
    _statusBar.setTitle("Equations");

    refreshAllPreviews();
    updateListFocus();

    lv_obj_remove_flag(_listContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showTemplate() {
    resetStepsPipeline();
    _state = State::TEMPLATE;
    _templateFocus = 0;

    // Update template item styles
    for (int i = 0; i < NUM_TEMPLATES; ++i) {
        if (i == _templateFocus) {
            lv_obj_set_style_text_color(_templateLabels[i],
                lv_color_hex(COL_FOCUS_HEX), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(_templateLabels[i],
                lv_color_black(), LV_PART_MAIN);
        }
    }

    lv_obj_remove_flag(_templateOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_templateOverlay);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showEditing(int idx) {
    resetStepsPipeline();
    hideAllContainers();
    _state = State::EDITING;
    _editingIndex = idx;

    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), "Edit equation E%d", idx + 1);
    lv_label_set_text(_editTitle, titleBuf);
    _statusBar.setTitle("Edit");

    // Clone the equation data into the editor
    if (_eqNode[idx]) {
        _editNode = cloneNode(_eqRowData[idx]);
    } else {
        _editNode = makeRow();
    }
    _editRow = static_cast<NodeRow*>(_editNode.get());
    _editCursor.init(_editRow);

    _editCanvas.setExpression(_editRow, &_editCursor);
    _editCanvas.invalidate();

    lv_obj_remove_flag(_editContainer, LV_OBJ_FLAG_HIDDEN);
    _editCanvas.startCursorBlink();
    lv_obj_invalidate(_screen);
}

void EquationsApp::showSolving() {
    resetStepsPipeline();
    hideAllContainers();
    _state = State::SOLVING;
    _statusBar.setTitle("Solving");

    lv_label_set_text(_solvingLabel, randomSolvingMessage());
    lv_obj_remove_flag(_solvingContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);

    // Force LVGL to render the spinner before blocking solve
    lv_timer_handler();
}

void EquationsApp::showResult() {
    resetStepsPipeline();
    hideAllContainers();
    _state = State::RESULT;
    _statusBar.setTitle("Result");

    buildResultDisplay();

    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showSteps() {
    resetStepsPipeline();
    hideAllContainers();
    _state = State::STEPS;
    _statusBar.setTitle("Steps");
    _stepScroll = 0;

    if (_stepsContainer == nullptr || !isValidLvObj(_stepsContainer)) {
        _stepsContainer = nullptr;
        return;
    }

    lv_obj_clean(_stepsContainer);
    _stepRenderers.clear();

    auto appendInfoLabel = [&](const char* text, uint32_t colorHex) {
        if (!isValidLvObj(_stepsContainer)) return;
        logStepBudget(-1);
        if (!canAllocStep(STEPS_LABEL_BUDGET)) return;
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        if (!lbl) return;
        lv_label_set_text(lbl, text);
        lv_obj_set_width(lbl, SCREEN_W - 2 * PAD - 8);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(colorHex), LV_PART_MAIN);
    };

    // Epoch coherence gate: steps are valid only for the latest solved state.
    if (_solveEpoch == 0 || _solveEpoch != _equationEpoch) {
        appendInfoLabel("Re-solve required: equation changed.", COL_ACCENT_HEX);
        appendInfoLabel("Press AC, solve again, then open STEPS.", COL_HINT_HEX);
        lv_obj_remove_flag(_stepsContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
        lv_obj_invalidate(_screen);
        return;
    }

    // WHY: tutor rendering is fail-closed. The authoritative Giac answer
    // remains visible in RESULT even when the retained tutor disagrees.
    if (_tutorStatus != TutorStatus::Agreed) {
        appendInfoLabel("Steps unavailable", COL_ACCENT_HEX);
        appendInfoLabel(_tutorDiagnostic.empty()
                            ? "Tutor result could not be verified."
                            : _tutorDiagnostic.c_str(),
                        COL_HINT_HEX);
        lv_obj_remove_flag(_stepsContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
        lv_obj_invalidate(_screen);
        return;
    }

    logStepBudget(0);
    _stepsProgressLabel = nullptr;
    if (canAllocStep(STEPS_LABEL_BUDGET)) {
        _stepsProgressLabel = lv_label_create(_stepsContainer);
    }
    if (_stepsProgressLabel) {
        lv_label_set_text(_stepsProgressLabel, "Preparing steps...");
        lv_obj_set_width(_stepsProgressLabel, SCREEN_W - 2 * PAD - 8);
        lv_label_set_long_mode(_stepsProgressLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(_stepsProgressLabel, &stix_math_18,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(_stepsProgressLabel, lv_color_hex(COL_HINT_HEX),
                                    LV_PART_MAIN);
    }

    // WHY: the consistency gate compared Giac specifically with
    // _omniResult/_systemResult. Render only that candidate's own logged
    // steps; starting a second tutor pipeline here would introduce an
    // unverified mathematical source after the gate.
    _stepsSource = StepsSource::LEGACY_LOG;

    _stepsStage = StepsStage::PARSE;
    _stepsPipelineActive = true;

    lv_obj_remove_flag(_stepsContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
    lv_obj_invalidate(_screen);
}

void EquationsApp::update() {
    if (!_screen || _state != State::STEPS || !_stepsPipelineActive) {
        return;
    }

    if (!isValidLvObj(_screen) || !isValidLvObj(_stepsContainer)) {
        _stepsProgressLabel = nullptr;
        resetStepsPipeline();
        return;
    }

    static constexpr int16_t CANVAS_MAX_W =
        static_cast<int16_t>(SCREEN_W - 2 * PAD - 8);

    auto setProgress = [&](const char* text) {
        if (isValidLvObj(_stepsProgressLabel)) {
            lv_label_set_text(_stepsProgressLabel, text);
        } else {
            _stepsProgressLabel = nullptr;
        }
    };

    auto appendHint = [&](int stepNumber) {
        if (!isValidLvObj(_stepsContainer)) return false;
        logStepBudget(stepNumber);
        if (!canAllocStep(STEPS_LABEL_BUDGET)) return false;

        lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
        if (!hintLbl) return false;
        lv_label_set_text(hintLbl,
                          LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
        lv_obj_set_style_text_font(hintLbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX),
                                    LV_PART_MAIN);
        return true;
    };

    auto appendMessage = [&](int stepNumber, const char* text, uint32_t colorHex) {
        if (!isValidLvObj(_stepsContainer)) return false;
        logStepBudget(stepNumber);
        if (!canAllocStep(STEPS_LABEL_BUDGET)) return false;

        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        if (!lbl) return false;
        lv_label_set_text(lbl, text);
        lv_obj_set_width(lbl, CANVAS_MAX_W);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(colorHex), LV_PART_MAIN);
        return true;
    };

    auto appendMemoryLimitReached = [&](std::size_t stepNumber) {
        failClosedStepRender(stepNumber);
    };

    auto emitCasCanvas = [&](std::size_t stepNumber,
                             const cas::NodePtr& tree,
                             const cas::NodePtr& highlight,
                             lv_color_t hlColor) {
        if (!tree) return true;
        if (!isValidLvObj(_stepsContainer)) return false;

        logStepBudget(static_cast<int>(stepNumber));
        if (!canAllocStep(STEPS_CANVAS_BUDGET)) return false;

        const vpam::MathNode* hlPtr = nullptr;
        vpam::NodePtr vpamTree;
        if (highlight) {
            vpamTree = cas::CasToVpam::convert(tree, highlight, &hlPtr);
        } else {
            vpamTree = cas::CasToVpam::convert(tree);
        }
        if (!vpamTree) return true;

        if (vpamTree->type() != vpam::NodeType::Row) {
            auto rowWrap = vpam::makeRow();
            static_cast<vpam::NodeRow*>(rowWrap.get())
                ->appendChild(std::move(vpamTree));
            vpamTree = std::move(rowWrap);
        }

        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData = std::move(vpamTree);
        srd->canvas.create(_stepsContainer);
        if (!srd->canvas.obj()) {
            return false;
        }

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = static_cast<int16_t>(row->layout().width + 24);
        int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
        if (h < 20) h = 20;
        lv_obj_set_size(srd->canvas.obj(), w, h);

        if (hlPtr) srd->canvas.setHighlightNode(hlPtr, hlColor);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
        return true;
    };

    auto emitSystemCanvas = [&](std::size_t stepNumber, const cas::NodePtr& tree) {
        if (!tree) return true;
        if (!isValidLvObj(_stepsContainer)) return false;

        logStepBudget(static_cast<int>(stepNumber));
        if (!canAllocStep(STEPS_CANVAS_BUDGET)) return false;

        vpam::NodePtr vpamTree = cas::CasToVpam::convert(tree);
        if (!vpamTree) return true;

        if (vpamTree->type() != vpam::NodeType::Row) {
            auto rowWrap = vpam::makeRow();
            static_cast<vpam::NodeRow*>(rowWrap.get())
                ->appendChild(std::move(vpamTree));
            vpamTree = std::move(rowWrap);
        }

        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData = std::move(vpamTree);
        srd->canvas.create(_stepsContainer);
        if (!srd->canvas.obj()) {
            return false;
        }

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = static_cast<int16_t>(row->layout().width + 24);
        int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
        if (h < 20) h = 20;
        lv_obj_set_size(srd->canvas.obj(), w, h);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
        return true;
    };

    switch (_stepsStage) {
        case StepsStage::PARSE: {
            _stepsPipelineError.clear();
            _stepsMemoryLimitReached = false;
            _stepsReachedFixedPoint = false;
            _stepsRenderIndex = 0;
            _stepsSolveIterations = 0;
            _stepsHeaderRendered = false;
            _stepsCasPendingLogs.clear();
            _stepsSystemPendingLogs.clear();
            _stepsTutorResult = cas::SolveResult();
            _stepsTutorActive = false;
            _stepsTutorArena.reset();
            _stepsParsedEq1.reset();
            _stepsParsedEq2.reset();
            _stepsCurrentTree.reset();

            if (_stepsSource == StepsSource::LEGACY_LOG) {
                setProgress("Rendering...");
                _stepsStage = StepsStage::RENDER_CHUNK;
                break;
            }

            _casPool.reset();
            _casEngine.reset();
            _hasCasResult = false;
            _casPool = std::make_unique<cas::CasMemoryPool>();
            if (!_casPool || !_casPool->valid()) {
                _stepsPipelineError = "Memory Limit Reached";
                _stepsStage = StepsStage::FINALIZE;
                break;
            }

            if (_stepsSource == StepsSource::SINGLE_CAS) {
                _stepsEqText1.clear();
                if (_eqRowData[0]) nodeToText(_eqRowData[0], _stepsEqText1);

                if (_stepsEqText1.find('=') == std::string::npos) {
                    _stepsPipelineError = "Parse error: equation must include '='.";
                    _stepsStage = StepsStage::FINALIZE;
                    break;
                }

                _stepsVar1 = 'x';
                _stepsParsedEq1 = casParseEquation(_stepsEqText1, *_casPool, _stepsVar1);
                if (!_stepsParsedEq1) {
                    _stepsPipelineError = "CAS parse failed: unsupported equation format.";
                    _stepsStage = StepsStage::FINALIZE;
                    break;
                }

                _casEngine = std::make_unique<cas::RuleEngine>(*_casPool);
                for (auto& rule : cas::makeAlgebraicRules(*_casPool, _stepsVar1)) {
                    _casEngine->addRule(std::move(rule));
                }

                _stepsCurrentTree = _stepsParsedEq1;
                setProgress("Solving... 0/80");
                _stepsStage = StepsStage::SOLVE;
            } else {
                _stepsEqText1.clear();
                _stepsEqText2.clear();
                if (_eqRowData[0]) nodeToText(_eqRowData[0], _stepsEqText1);
                if (_eqRowData[1]) nodeToText(_eqRowData[1], _stepsEqText2);

                if (_stepsEqText1.find('=') == std::string::npos ||
                    _stepsEqText2.find('=') == std::string::npos) {
                    _stepsPipelineError = "Parse error: invalid system equations.";
                    _stepsStage = StepsStage::FINALIZE;
                    break;
                }

                _stepsVar1 = 'x';
                _stepsVar2 = 'y';
                _stepsParsedEq1 = casParseEquation(_stepsEqText1, *_casPool, _stepsVar1);
                _stepsParsedEq2 = casParseEquation(_stepsEqText2, *_casPool, _stepsVar2);
                if (!_stepsParsedEq1 || !_stepsParsedEq2) {
                    _stepsPipelineError = "CAS parse failed: unsupported system format.";
                    _stepsStage = StepsStage::FINALIZE;
                    break;
                }

                setProgress("Solving system steps...");
                _stepsStage = StepsStage::SOLVE;
            }
            break;
        }

        case StepsStage::SOLVE: {
            if (_stepsSource == StepsSource::SINGLE_CAS) {
                if (!_casEngine || !_stepsCurrentTree) {
                    _stepsPipelineError = "CAS engine not ready.";
                    _stepsStage = StepsStage::FINALIZE;
                    break;
                }

                _casResult = _casEngine->applyToFixedPoint(_stepsCurrentTree,
                                                           STEPS_SOLVE_MAX);
                cas::checkNonLinearHandover(_casResult, _stepsVar1);

                _stepsTutorResult = cas::SolveResult();
                _stepsTutorActive = false;
                _stepsTutorArena.reset();

                bool hasHandover = false;
                for (const auto& step : _casResult.steps) {
                    if (step.ruleName == cas::RULE_NONLINEAR_HANDOVER) {
                        hasHandover = true;
                        break;
                    }
                }

                if (hasHandover && _stepsParsedEq1) {
                    NodePtr lhsTree;
                    NodePtr rhsTree;
                    if (splitAtEquals(_eqRowData[0], lhsTree, rhsTree)) {
                        cas::ASTFlattener flattener;
                        flattener.setArena(&_stepsTutorArena);
                        auto eqFlat = flattener.flattenEquation(lhsTree.get(), rhsTree.get());
                        if (eqFlat.ok && !eqFlat.transcendental) {
                            cas::PedagogicalLogger tutorLog;
                            const cas::SymEquation normalizedEq = eqFlat.eq.moveAllToLHS();
                            const int16_t degree = normalizedEq.lhs.degree();
                            bool ranTutor = false;
                            if (degree == 2) {
                                _stepsTutorResult = cas::solveQuadraticTutor(eqFlat.eq, _stepsVar1,
                                                                             tutorLog, &_stepsTutorArena);
                                ranTutor = true;
                            } else if (degree == 3) {
                                _stepsTutorResult = cas::solveCubicTutor(eqFlat.eq, _stepsVar1,
                                                                         tutorLog, &_stepsTutorArena);
                                ranTutor = true;
                            }
                            if (ranTutor) {
                                _stepsTutorResult.steps = std::move(tutorLog);
                                _stepsTutorActive = (_stepsTutorResult.steps.count() > 0U);
                            }
                        }
                    }
                }

                _stepsCurrentTree = _casResult.finalTree;
                _stepsCasPendingLogs = _casResult.steps;
                _stepsSolveIterations = _stepsCasPendingLogs.size();
                _stepsSolveIterations += _stepsTutorResult.steps.count();
                _stepsReachedFixedPoint = _casResult.reachedFixedPoint;

                _hasCasResult = true;
                _stepsRenderIndex = 0;
                _stepsStage = StepsStage::RENDER_CHUNK;
                setProgress("Rendering...");
            } else {
                cas::SystemTutorResult tutorResult = cas::SystemTutor::solveSystem(
                    _stepsParsedEq1, _stepsParsedEq2, *_casPool, _stepsVar1, _stepsVar2);
                _stepsSystemPendingLogs = tutorResult.steps;
                _stepsRenderIndex = 0;
                _stepsHeaderRendered = false;
                _stepsStage = StepsStage::RENDER_CHUNK;
                setProgress("Rendering...");
            }
            break;
        }

        case StepsStage::RENDER_CHUNK: {
            if (!isValidLvObj(_stepsContainer)) {
                _stepsProgressLabel = nullptr;
                resetStepsPipeline();
                break;
            }

            if (_stepsSource == StepsSource::LEGACY_LOG) {
                buildStepsDisplay();
                _stepsStage = StepsStage::FINALIZE;
                break;
            }

            if (_stepsSource == StepsSource::SINGLE_CAS) {
                const std::size_t casStepCount = _stepsCasPendingLogs.size();
                const std::size_t tutorStepCount = _stepsTutorResult.steps.count();

                if (_stepsRenderIndex == 0) {
                    if (!appendMessage(0, "0. Original Equation", COL_DESC_HEX) ||
                        !emitCasCanvas(0, _stepsParsedEq1, nullptr,
                                       lv_color_hex(0x4FC3F7))) {
                        appendMemoryLimitReached(0);
                        break;
                    }
                    _stepsRenderIndex = 1;
                }

                constexpr std::size_t kRenderBudgetPerTick = 2;
                std::size_t rendered = 0;
                while (_stepsRenderIndex > 0 &&
                       (_stepsRenderIndex - 1) < casStepCount &&
                       rendered < kRenderBudgetPerTick)
                {
                    const std::size_t idx = _stepsRenderIndex - 1;
                    const auto& step = _stepsCasPendingLogs[idx];
                    const bool isFinal = (idx == casStepCount - 1) ||
                                         (step.ruleName == cas::RULE_NONLINEAR_HANDOVER);
                    const uint32_t descColHex = isFinal ? COL_ACCENT_HEX : COL_DESC_HEX;
                    const lv_color_t hlColor = isFinal ? lv_color_hex(0xFFB300)
                                                       : lv_color_hex(0x4FC3F7);

                    char buf[120];
                    if (!step.ruleDesc.empty()) {
                        snprintf(buf, sizeof(buf), "%d. %s", (int)(idx + 1),
                                 step.ruleDesc.c_str());
                    } else {
                        snprintf(buf, sizeof(buf), "%d. %s", (int)(idx + 1),
                                 step.ruleName.c_str());
                    }

                    if (!appendMessage(static_cast<int>(idx + 1), buf, descColHex) ||
                        !emitCasCanvas(idx + 1, step.tree, step.affectedNode, hlColor)) {
                        appendMemoryLimitReached(idx + 1);
                        break;
                    }

                    ++_stepsRenderIndex;
                    ++rendered;
                }

                while (rendered < kRenderBudgetPerTick) {
                    if ((_stepsRenderIndex - 1) < casStepCount) break;
                    const std::size_t tutorIdx = (_stepsRenderIndex - 1) - casStepCount;
                    if (tutorIdx >= tutorStepCount) break;
                    const auto& step = _stepsTutorResult.steps.steps()[tutorIdx];
                    const bool isFinal =
                        (tutorIdx + 1 == tutorStepCount) &&
                        (step.kind == cas::StepKind::Result ||
                         step.kind == cas::StepKind::ComplexResult);
                    const uint32_t descColHex = isFinal ? COL_ACCENT_HEX : COL_DESC_HEX;

                    char buf[200];
                    if (!step.description.empty()) {
                        snprintf(buf, sizeof(buf), "%d. %s", (int)(_stepsRenderIndex),
                                 step.description.c_str());
                    } else {
                        snprintf(buf, sizeof(buf), "%d. Step", (int)(_stepsRenderIndex));
                    }

                    if (!appendMessage(static_cast<int>(_stepsRenderIndex), buf, descColHex)) {
                        appendMemoryLimitReached(_stepsRenderIndex);
                        break;
                    }

                    if (step.mathExpr) {
                        vpam::NodePtr astNode = cas::SymExprToAST::convert(step.mathExpr);
                        astNode = cas::SymExprToAST::ensureRow(std::move(astNode));
                        if (!astNode || !isValidLvObj(_stepsContainer)) {
                            appendMemoryLimitReached(_stepsRenderIndex);
                            break;
                        }
                        logStepBudget(static_cast<int>(_stepsRenderIndex));
                        if (!canAllocStep(STEPS_CANVAS_BUDGET)) {
                            appendMemoryLimitReached(_stepsRenderIndex);
                            break;
                        }
                        auto srd = std::make_unique<StepRenderData>();
                        srd->nodeData = std::move(astNode);
                        srd->canvas.create(_stepsContainer);
                        if (!srd->canvas.obj()) {
                            appendMemoryLimitReached(_stepsRenderIndex);
                            break;
                        }
                        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
                        srd->canvas.setExpression(row, nullptr);
                        row->calculateLayout(srd->canvas.normalMetrics());
                        int16_t w = static_cast<int16_t>(row->layout().width + 24);
                        int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
                        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
                        if (h < 20) h = 20;
                        lv_obj_set_size(srd->canvas.obj(), w, h);
                        srd->canvas.invalidate();
                        _stepRenderers.push_back(std::move(srd));
                    } else if (step.kind == cas::StepKind::Transform ||
                               step.kind == cas::StepKind::Result) {
                        std::string eqText = step.snapshot.toString();
                        if (!eqText.empty() && eqText != "0" && eqText != "0 = 0") {
                            vpam::NodePtr snapNode = cas::SymToAST::fromSymEquation(step.snapshot);
                            snapNode = cas::SymExprToAST::ensureRow(std::move(snapNode));
                            if (!snapNode || !isValidLvObj(_stepsContainer)) {
                                appendMemoryLimitReached(_stepsRenderIndex);
                                break;
                            }
                            logStepBudget(static_cast<int>(_stepsRenderIndex));
                            if (!canAllocStep(STEPS_CANVAS_BUDGET)) {
                                appendMemoryLimitReached(_stepsRenderIndex);
                                break;
                            }
                            auto srd = std::make_unique<StepRenderData>();
                            srd->nodeData = std::move(snapNode);
                            srd->canvas.create(_stepsContainer);
                            if (!srd->canvas.obj()) {
                                appendMemoryLimitReached(_stepsRenderIndex);
                                break;
                            }
                            auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
                            srd->canvas.setExpression(row, nullptr);
                            row->calculateLayout(srd->canvas.normalMetrics());
                            int16_t w = static_cast<int16_t>(row->layout().width + 24);
                            int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
                            if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
                            if (h < 20) h = 20;
                            lv_obj_set_size(srd->canvas.obj(), w, h);
                            srd->canvas.invalidate();
                            _stepRenderers.push_back(std::move(srd));
                        }
                    }

                    ++_stepsRenderIndex;
                    ++rendered;
                }

                if (_stepsStage == StepsStage::RENDER_CHUNK) {
                    const std::size_t totalCas = casStepCount;
                    const std::size_t totalTutor = tutorStepCount;
                    const std::size_t totalSteps = totalCas + totalTutor;
                    if ((_stepsRenderIndex - 1) >= totalSteps) {
                        if (!appendHint(static_cast<int>(_stepsRenderIndex - 1))) {
                            appendMemoryLimitReached(_stepsRenderIndex - 1);
                        }
                        if (_stepsStage == StepsStage::RENDER_CHUNK) {
                            _stepsStage = StepsStage::FINALIZE;
                        }
                    } else {
                        char prog[40];
                        snprintf(prog, sizeof(prog), "Rendering... %u/%u",
                                 static_cast<unsigned>(_stepsRenderIndex - 1),
                                 static_cast<unsigned>(totalSteps));
                        setProgress(prog);
                    }
                }
            } else {
                if (!_stepsHeaderRendered) {
                    if (!appendMessage(0, "System of Equations", COL_ACCENT_HEX) ||
                        !appendMessage(0, "0. Original System", COL_DESC_HEX) ||
                        !emitSystemCanvas(0, _stepsParsedEq1) ||
                        !emitSystemCanvas(0, _stepsParsedEq2)) {
                        appendMemoryLimitReached(0);
                        break;
                    }
                    _stepsHeaderRendered = true;
                }

                constexpr std::size_t kRenderBudgetPerTick = 1;
                std::size_t rendered = 0;
                while (_stepsRenderIndex < _stepsSystemPendingLogs.size() &&
                       rendered < kRenderBudgetPerTick)
                {
                    const auto& step = _stepsSystemPendingLogs[_stepsRenderIndex];
                    char buf[160];
                    if (!step.ruleDesc.empty()) {
                        snprintf(buf, sizeof(buf), "%d. %s", (int)(_stepsRenderIndex + 1),
                                 step.ruleDesc.c_str());
                    } else {
                        snprintf(buf, sizeof(buf), "%d. %s", (int)(_stepsRenderIndex + 1),
                                 step.ruleName.c_str());
                    }

                    if (!appendMessage(static_cast<int>(_stepsRenderIndex + 1), buf,
                                       COL_DESC_HEX) ||
                        !emitSystemCanvas(_stepsRenderIndex + 1, step.eq1Tree) ||
                        !emitSystemCanvas(_stepsRenderIndex + 1, step.eq2Tree)) {
                        appendMemoryLimitReached(_stepsRenderIndex + 1);
                        break;
                    }

                    ++_stepsRenderIndex;
                    ++rendered;
                }

                if (_stepsStage == StepsStage::RENDER_CHUNK) {
                    if (_stepsRenderIndex >= _stepsSystemPendingLogs.size()) {
                        if (!appendHint(static_cast<int>(_stepsRenderIndex))) {
                            appendMemoryLimitReached(_stepsRenderIndex);
                        }
                        if (_stepsStage == StepsStage::RENDER_CHUNK) {
                            _stepsStage = StepsStage::FINALIZE;
                        }
                    } else {
                        char prog[40];
                        snprintf(prog, sizeof(prog), "Rendering... %u/%u",
                                 static_cast<unsigned>(_stepsRenderIndex),
                                 static_cast<unsigned>(_stepsSystemPendingLogs.size()));
                        setProgress(prog);
                    }
                }
            }
            break;
        }

        case StepsStage::FINALIZE: {
            if (!_stepsPipelineError.empty()) {
                if (isValidLvObj(_stepsContainer)) {
                    lv_obj_clean(_stepsContainer);
                }
                _stepRenderers.clear();
                _stepsProgressLabel = nullptr;

                if (isValidLvObj(_stepsContainer)) {
                    if (appendMessage(-1, _stepsPipelineError.c_str(), COL_ACCENT_HEX)) {
                        appendHint(-1);
                    }
                }
            }

            if (isValidLvObj(_stepsProgressLabel)) {
                if (_stepsMemoryLimitReached) {
                    lv_obj_remove_flag(_stepsProgressLabel, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(_stepsProgressLabel, LV_OBJ_FLAG_HIDDEN);
                }
            }

            _stepsEpoch = _solveEpoch;
            _stepsPipelineActive = false;
            _stepsStage = StepsStage::IDLE;
            if (isValidLvObj(_screen)) {
                lv_obj_invalidate(_screen);
            }
            break;
        }

        case StepsStage::IDLE:
        default:
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Key Dispatch
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_state) {
        case State::EQ_LIST:   handleKeyList(ev);      break;
        case State::TEMPLATE:  handleKeyTemplate(ev);   break;
        case State::EDITING:   handleKeyEditing(ev);    break;
        case State::SOLVING:   /* ignore while solving */ break;
        case State::RESULT:    handleKeyResult(ev);     break;
        case State::STEPS:     handleKeySteps(ev);      break;
    }
}

// ────────────────────────────────────────────────────────────────────
// EQ_LIST state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeyList(const KeyEvent& ev) {
    int total = listItemCount();
    int addIdx = _numEquations;
    bool hasAdd = (_numEquations < MAX_EQS);
    int solveIdx = -1;
    if (_numEquations > 0 && hasAdd) solveIdx = _numEquations + 1;
    else if (_numEquations > 0) solveIdx = _numEquations;

    switch (ev.code) {
        case KeyCode::UP:
            if (_listFocus > 0) {
                --_listFocus;
                updateListFocus();
            }
            break;

        case KeyCode::DOWN:
            if (_listFocus < total - 1) {
                ++_listFocus;
                updateListFocus();
            }
            break;

        case KeyCode::ENTER:
            if (_listFocus < _numEquations) {
                // Edit existing equation
                showEditing(_listFocus);
            } else if (hasAdd && _listFocus == addIdx) {
                // Add new equation → show template chooser
                showTemplate();
            } else if (_listFocus == solveIdx) {
                // Solve!
                solveEquations();
            }
            break;

        case KeyCode::DEL:
            // Delete focused equation
            if (_listFocus < _numEquations && _numEquations > 0) {
                int delIdx = _listFocus;
                // Shift equations down
                for (int i = delIdx; i < _numEquations - 1; ++i) {
                    _eqNode[i] = std::move(_eqNode[i + 1]);
                    _eqRowData[i] = _eqRowData[i + 1];
                }
                _eqNode[_numEquations - 1].reset();
                _eqRowData[_numEquations - 1] = nullptr;
                --_numEquations;

                if (_listFocus >= listItemCount()) {
                    _listFocus = listItemCount() - 1;
                }
                invalidateStepsCache();
                showEqList();
            }
            break;

        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────────
// TEMPLATE state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeyTemplate(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
            if (_templateFocus > 0) {
                --_templateFocus;
                for (int i = 0; i < NUM_TEMPLATES; ++i) {
                    if (_templateLabels[i]) {
                        lv_obj_set_style_text_color(_templateLabels[i],
                            (i == _templateFocus) ? lv_color_hex(COL_FOCUS_HEX) : lv_color_black(),
                            LV_PART_MAIN);
                    }
                }
                if (_templateOverlay) lv_obj_invalidate(_templateOverlay);
            }
            break;

        case KeyCode::DOWN:
            if (_templateFocus < NUM_TEMPLATES - 1) {
                ++_templateFocus;
                for (int i = 0; i < NUM_TEMPLATES; ++i) {
                    if (_templateLabels[i]) {
                        lv_obj_set_style_text_color(_templateLabels[i],
                            (i == _templateFocus) ? lv_color_hex(COL_FOCUS_HEX) : lv_color_black(),
                            LV_PART_MAIN);
                    }
                }
                if (_templateOverlay) lv_obj_invalidate(_templateOverlay);
            }
            break;

        case KeyCode::ENTER: {
            // Apply template and go to editor
            int slot = _numEquations;
            if (slot >= MAX_EQS) { break; }

            applyTemplate(_templateFocus, slot);
            ++_numEquations;
            invalidateStepsCache();

            // Hide template overlay
            if (_templateOverlay) {
                lv_obj_add_flag(_templateOverlay, LV_OBJ_FLAG_HIDDEN);
            }

            // Go to editing mode for the new equation
            showEditing(slot);
            break;
        }

        case KeyCode::AC:
        case KeyCode::DEL:
            // Cancel template, back to list
            if (_templateOverlay) {
                lv_obj_add_flag(_templateOverlay, LV_OBJ_FLAG_HIDDEN);
            }
            _state = State::EQ_LIST;
            if (_screen) lv_obj_invalidate(_screen);
            break;

        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────────
// EDITING state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeyEditing(const KeyEvent& ev) {
    auto& km = KeyboardManager::instance();
    if (ev.code == KeyCode::SHIFT) { km.pressShift(); _statusBar.update(); return; }
    if (ev.code == KeyCode::ALPHA) { km.pressAlpha(); _statusBar.update(); return; }

    auto& cc = _editCursor;
    bool changed = false;

    switch (ev.code) {
        // ── Navigation ──
        case KeyCode::LEFT:  cc.moveLeft();  changed = true; break;
        case KeyCode::RIGHT: cc.moveRight(); changed = true; break;
        case KeyCode::UP:    /* reserved */  break;
        case KeyCode::DOWN:  /* reserved */  break;

        // ── Digits ──
        case KeyCode::NUM_0: cc.insertDigit('0'); changed = true; break;
        case KeyCode::NUM_1: cc.insertDigit('1'); changed = true; break;
        case KeyCode::NUM_2: cc.insertDigit('2'); changed = true; break;
        case KeyCode::NUM_3: cc.insertDigit('3'); changed = true; break;
        case KeyCode::NUM_4: cc.insertDigit('4'); changed = true; break;
        case KeyCode::NUM_5: cc.insertDigit('5'); changed = true; break;
        case KeyCode::NUM_6: cc.insertDigit('6'); changed = true; break;
        case KeyCode::NUM_7: cc.insertDigit('7'); changed = true; break;
        case KeyCode::NUM_8: cc.insertDigit('8'); changed = true; break;
        case KeyCode::NUM_9: cc.insertDigit('9'); changed = true; break;
        case KeyCode::DOT:   cc.insertDigit('.'); changed = true; break;

        // ── Operators ──
        case KeyCode::ADD: cc.insertOperator(OpKind::Add); changed = true; break;
        case KeyCode::SUB: cc.insertOperator(OpKind::Sub); changed = true; break;
        case KeyCode::MUL: cc.insertOperator(OpKind::Mul); changed = true; break;

        // ── VPAM structures ──
        case KeyCode::DIV:    cc.insertFraction(); changed = true; break;
        case KeyCode::POW:    cc.insertPower();    changed = true; break;
        case KeyCode::SQRT:   cc.insertRoot();     changed = true; break;
        case KeyCode::LPAREN: cc.insertParen();    changed = true; break;

        // ── Variables ──
        case KeyCode::VAR_X: cc.insertVariable('x'); changed = true; break;
        case KeyCode::VAR_Y:
            if (km.isAlpha()) {
                // WHY: Three-variable systems need z, but the keypad has only
                // dedicated x/y keys. ALPHA+y preserves an authored AST path.
                cc.insertVariable('z');
                km.consumeModifier();
            } else {
                cc.insertVariable('y');
            }
            changed = true;
            break;

        // ── Functions (sin, cos, tan, ln, log) ──
        case KeyCode::SIN:
            if (km.isShift()) {
                cc.insertFunction(FuncKind::ArcSin);
                km.consumeModifier();
            } else {
                cc.insertFunction(FuncKind::Sin);
            }
            changed = true;
            break;
        case KeyCode::COS:
            if (km.isShift()) {
                cc.insertFunction(FuncKind::ArcCos);
                km.consumeModifier();
            } else {
                cc.insertFunction(FuncKind::Cos);
            }
            changed = true;
            break;
        case KeyCode::TAN:
            if (km.isShift()) {
                cc.insertFunction(FuncKind::ArcTan);
                km.consumeModifier();
            } else {
                cc.insertFunction(FuncKind::Tan);
            }
            changed = true;
            break;
        case KeyCode::LN:
            cc.insertFunction(FuncKind::Ln);
            changed = true;
            break;
        case KeyCode::LOG:
            cc.insertFunction(FuncKind::Log);
            changed = true;
            break;
        case KeyCode::LOG_BASE:
            cc.insertLogBase();
            changed = true;
            break;

        // ── Constants ──
        case KeyCode::CONST_PI: cc.insertConstant(ConstKind::Pi); changed = true; break;
        case KeyCode::CONST_E:  cc.insertConstant(ConstKind::E);  changed = true; break;

        // ── FREE_EQ → insert '=' sign ──
        case KeyCode::FREE_EQ:
            cc.insertVariable('=');
            changed = true;
            break;

        // ── NEG → negative sign ──
        case KeyCode::NEG:
        case KeyCode::NEGATE:
            cc.insertOperator(OpKind::Sub);
            changed = true;
            break;

        // ── Editing ──
        case KeyCode::DEL:
            cc.backspace();
            changed = true;
            break;

        // ── AC → cancel editing, back to list ──
        case KeyCode::AC:
            _editCanvas.stopCursorBlink();
            _editNode.reset();
            _editRow = nullptr;
            showEqList();
            break;

        // ── ENTER → confirm editing, save to slot, back to list ──
        case KeyCode::ENTER: {
            _editCanvas.stopCursorBlink();

            // Save the edited expression to the equation slot
            _eqNode[_editingIndex] = std::move(_editNode);
            _eqRowData[_editingIndex] = static_cast<NodeRow*>(_eqNode[_editingIndex].get());
            _editRow = nullptr;
            invalidateStepsCache();

            // Focus on the next item after the edited equation
            _listFocus = _editingIndex;
            showEqList();
            break;
        }

        default:
            break;
    }

    if (km.isShift()) km.consumeModifier();

    if (changed) {
        adjustEditHeight();
        _editCanvas.setExpression(_editRow, &_editCursor);
        _editCanvas.invalidate();
        _editCanvas.resetCursorBlink();
    }
}

// ────────────────────────────────────────────────────────────────────
// RESULT state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeyResult(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::SHOW_STEPS:
            showSteps();
            break;
        case KeyCode::AC:
            showEqList();
            break;
        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────────
// STEPS state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeySteps(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
            if (_stepsContainer && isValidLvObj(_stepsContainer)) {
                lv_obj_scroll_by(_stepsContainer, 0, 30, LV_ANIM_ON);
            }
            break;
        case KeyCode::DOWN:
            if (_stepsContainer && isValidLvObj(_stepsContainer)) {
                lv_obj_scroll_by(_stepsContainer, 0, -30, LV_ANIM_ON);
            }
            break;
        case KeyCode::AC:
        case KeyCode::DEL:
            showResult();
            break;
        default:
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Template application
// ════════════════════════════════════════════════════════════════════════════

vpam::NodePtr EquationsApp::buildTemplateAST(int templateIdx) {
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());

    switch (templateIdx) {
        case 0:  // Empty
            break;

        case 1:  // Polynomial: ax² + bx + c = 0
            r->appendChild(makeVariable('a'));
            r->appendChild(makePower(makeVariable('x'), makeNumber("2")));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('b'));
            r->appendChild(makeVariable('x'));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('c'));
            r->appendChild(makeRelation(OpKind::Eq));
            r->appendChild(makeNumber("0"));
            break;

        case 2: { // Exponential: a·e^x + b = 0
            r->appendChild(makeVariable('a'));
            r->appendChild(makePower(
                makeConstant(ConstKind::E), makeVariable('x')));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('b'));
            r->appendChild(makeRelation(OpKind::Eq));
            r->appendChild(makeNumber("0"));
            break;
        }

        case 3: { // Logarithmic: ln(x) + a = 0
            r->appendChild(makeFunction(FuncKind::Ln, makeVariable('x')));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('a'));
            r->appendChild(makeRelation(OpKind::Eq));
            r->appendChild(makeNumber("0"));
            break;
        }
    }

    return row;
}

void EquationsApp::applyTemplate(int templateIdx, int eqSlot) {
    _eqNode[eqSlot] = buildTemplateAST(templateIdx);
    _eqRowData[eqSlot] = static_cast<NodeRow*>(_eqNode[eqSlot].get());
}

// ════════════════════════════════════════════════════════════════════════════
// Equation preview
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::refreshPreview(int idx) {
    if (idx < 0 || idx >= _numEquations) return;
    if (!_eqRowData[idx]) return;

    _eqPreview[idx].setExpression(_eqRowData[idx], nullptr);
    _eqRowData[idx]->calculateLayout(_eqPreview[idx].normalMetrics());
    _eqPreview[idx].invalidate();
}

void EquationsApp::refreshAllPreviews() {
    for (int i = 0; i < _numEquations; ++i) {
        refreshPreview(i);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Dynamic height for editor
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::adjustEditHeight() {
    if (!_editRow) return;

    _editRow->calculateLayout(_editCanvas.normalMetrics());

    int contentH = vpam::mathObjectHeightPx(_editRow->layout(), _editCanvas.normalMetrics(), 0);
    int minH = 50;
    int maxH = 140;

    int newH = contentH + 16;
    if (newH < minH) newH = minH;
    if (newH > maxH) newH = maxH;

    int curH = lv_obj_get_height(_editCanvas.obj());
    if (curH > 0 && (newH - curH > 2 || curH - newH > 2)) {
        lv_obj_set_height(_editCanvas.obj(), newH);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Split at '=' sign
// ════════════════════════════════════════════════════════════════════════════

bool EquationsApp::splitAtEquals(NodeRow* row,
                                  NodePtr& outLHS,
                                  NodePtr& outRHS) {
    outLHS.reset();
    outRHS.reset();
    if (!row) return false;

    int eqIdx = -1;
    int count = row->childCount();

    for (int i = 0; i < count; ++i) {
        const MathNode* ch = row->child(i);
        bool isEquals = false;
        if (ch->type() == NodeType::Variable) {
            const auto* v = static_cast<const NodeVariable*>(ch);
            isEquals = (v->name() == '=');
        } else if (ch->type() == NodeType::Operator) {
            const auto* relation = static_cast<const NodeOperator*>(ch);
            isEquals = (relation->op() == OpKind::Eq);
        }

        // WHY: authored equation rows must contain exactly one complete
        // equality. Treating a missing side as zero fabricated input and made
        // template relations take a different route from the '=' key.
        if (isEquals) {
            if (eqIdx >= 0) return false;
            eqIdx = i;
        }
    }

    if (eqIdx <= 0 || eqIdx >= count - 1) return false;

    outLHS = makeRow();
    auto* lhsRow = static_cast<NodeRow*>(outLHS.get());
    for (int i = 0; i < eqIdx; ++i) {
        lhsRow->appendChild(cloneNode(row->child(i)));
    }

    outRHS = makeRow();
    auto* rhsRow = static_cast<NodeRow*>(outRHS.get());
    for (int i = eqIdx + 1; i < count; ++i) {
        rhsRow->appendChild(cloneNode(row->child(i)));
    }

    return !lhsRow->isEmpty() && !rhsRow->isEmpty();
}

// ════════════════════════════════════════════════════════════════════════════
// Convert SymEquation → LinEq for system solver
// ════════════════════════════════════════════════════════════════════════════

cas::LinEq EquationsApp::symEquationToLinEq(const cas::SymEquation& eq,
                                            const char* vars, int numVars) {
    cas::SymEquation norm = eq.moveAllToLHS();
    const auto& terms = norm.lhs.terms();

    cas::LinEq lin;

    for (int v = 0; v < numVars; ++v) {
        lin.coeffs[v] = vpam::ExactVal::fromInt(0);
        for (const auto& t : terms) {
            if (t.var == vars[v] && t.power == 1) {
                lin.coeffs[v] = t.coeff.toExactVal();
                break;
            }
        }
    }

    vpam::ExactVal constant = vpam::ExactVal::fromInt(0);
    for (const auto& t : terms) {
        if (t.isConstant()) {
            constant = t.coeff.toExactVal();
            break;
        }
    }
    lin.rhs = vpam::exactNeg(constant);

    return lin;
}

// ════════════════════════════════════════════════════════════════════════════
// Solve — Main dispatcher
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::solveEquations() {
    _solveEpoch = 0;
    solveWithGiac();

    // Mark solve data as coherent with the current equation generation.
    _solveEpoch = _equationEpoch;
}

void EquationsApp::solveWithGiac() {
    numos::HwUxProbe hwux("equations", "solve");
    showSolving();

    _isOmniSolve = (_numEquations == 1);
    _resultKind = ResultKind::None;
    _tutorStatus = TutorStatus::Unavailable;
    _tutorDiagnostic.clear();
    _giacResult = numos::StructuredSolveResult();

    std::vector<numos::SolveEquation> equations;
    equations.reserve(static_cast<std::size_t>(_numEquations));

    for (int i = 0; i < _numEquations; ++i) {
        NodePtr lhs;
        NodePtr rhs;
        if (!splitAtEquals(_eqRowData[i], lhs, rhs)) {
            _giacResult.status = numos::MathEngineStatus::ParseError;
            _giacResult.diagnostic =
                "Equation row must contain one complete equality";
            showResult();
            return;
        }

        numos::SolveEquation serialized;
        std::string error;
        if (!numos::CalculationEngine::serializeForGiac(
                lhs.get(), serialized.lhs, error) ||
            !numos::CalculationEngine::serializeForGiac(
                rhs.get(), serialized.rhs, error)) {
            _giacResult.status = numos::MathEngineStatus::ParseError;
            _giacResult.diagnostic = error.empty()
                ? "Unsupported equation input" : error;
            showResult();
            return;
        }
        equations.push_back(std::move(serialized));
    }

    const numos::SolveDomainPolicy policy = setting_complex_enabled
        ? numos::SolveDomainPolicy::RealAndComplex
        : numos::SolveDomainPolicy::RealOnly;

    if (_numEquations == 1) {
        _giacResult = numos::GiacEngine::instance().solveStructured(
            equations.front(), "x", policy);
    } else {
        static const char* const kVariables[MAX_EQS] = {"x", "y", "z"};
        std::vector<std::string> variables;
        variables.reserve(static_cast<std::size_t>(_numEquations));
        for (int i = 0; i < _numEquations; ++i) {
            variables.emplace_back(kVariables[i]);
        }
        _giacResult =
            numos::GiacEngine::instance().solveSystemStructured(
                equations, variables, policy);
    }

    // WHY: the custom CAS is invoked only as an optional tutor producer.
    // Its roots never enter _giacResult or the result-presentation path.
    if (_giacResult.ok() &&
        _giacResult.setKind == numos::SolutionSetKind::Solutions) {
        generateTutorCandidate();
        if (tutorCandidateAgrees()) {
            _tutorStatus = TutorStatus::Agreed;
            _tutorDiagnostic = "normalized tutor solutions agree with Giac";
        } else {
            _tutorStatus = TutorStatus::Unavailable;
            _tutorDiagnostic =
                "tutor solution set disagrees with or cannot represent Giac";
        }
    } else {
        _tutorDiagnostic =
            "tutor disabled for non-ordinary Giac solution result";
    }

    showResult();

    const char* status = _giacResult.ok() ? "ok" : "error";
    const char* kind = _resultKind == ResultKind::Structured
        ? "structured" : (_resultKind == ResultKind::TextFallback
        ? "text_fallback" : "none");
    uint32_t hash = 2166136261u;
    for (const auto& group : _giacResult.groups) {
        for (const auto& value : group.values) {
            hash ^= numos::hwUxHash(value.exactText.c_str());
            hash *= 16777619u;
        }
    }
    hwux.finish(status, kind, "giac", hash);
}

void EquationsApp::generateTutorCandidate() {
    _arena.reset();
    _omniResult = cas::OmniResult();
    _systemResult = cas::SystemResult();

    cas::ASTFlattener flattener;
    flattener.setArena(&_arena);

    if (_numEquations == 1) {
        NodePtr lhs;
        NodePtr rhs;
        if (!splitAtEquals(_eqRowData[0], lhs, rhs)) return;
        cas::SymExpr* lhsExpr = flattener.flattenToExpr(lhs.get());
        cas::SymExpr* rhsExpr = flattener.flattenToExpr(rhs.get());
        if (!lhsExpr || !rhsExpr) return;

        cas::OmniSolver solver;
        _omniResult = solver.solve(lhsExpr, rhsExpr, 'x', _arena);
        return;
    }

    // The retained SystemTutor is linear. Do not broaden its mathematics
    // during GIAC-D01; nonlinear systems simply report steps unavailable.
    cas::LinEq linear[3];
    static const char kVariables[MAX_EQS] = {'x', 'y', 'z'};
    for (int i = 0; i < _numEquations; ++i) {
        NodePtr lhs;
        NodePtr rhs;
        if (!splitAtEquals(_eqRowData[i], lhs, rhs)) return;
        const auto flattened = flattener.flattenEquation(lhs.get(), rhs.get());
        if (!flattened.ok) return;
        linear[i] = symEquationToLinEq(flattened.eq, kVariables,
                                       _numEquations);
    }

    cas::SystemSolver solver;
    if (_numEquations == 2) {
        _systemResult = solver.solve2x2(linear[0], linear[1],
                                        'x', 'y', &_arena);
    } else if (_numEquations == 3) {
        _systemResult = solver.solve3x3(linear[0], linear[1], linear[2]);
    }
}

bool EquationsApp::tutorCandidateAgrees() const {
    auto exactEqual = [](vpam::ExactVal a, vpam::ExactVal b) {
        if (!a.ok || !b.ok || a.approximate || b.approximate) return false;
        a.simplify();
        a.simplifyRadical();
        b.simplify();
        b.simplifyRadical();
        return a.num == b.num && a.den == b.den &&
               a.outer == b.outer && a.inner == b.inner &&
               a.piMul == b.piMul && a.eMul == b.eMul;
    };

    auto matches = [&](const numos::StructuredSolution& giac,
                       const vpam::ExactVal* exact,
                       const double* approximate) {
        vpam::ExactVal giacExact;
        if (exact &&
            numos::CalculationEngine::resultTreeToExactVal(
                giac.exactValue, giacExact)) {
            return exactEqual(*exact, giacExact);
        }

        // WHY: tolerance is permitted only when the tutor explicitly marked
        // its own result approximate and Giac supplied a real approximation.
        return approximate && giac.hasApproximateReal &&
               std::isfinite(*approximate) &&
               std::fabs(*approximate - giac.approximateReal) <= 1e-8;
    };

    if (!_giacResult.ok() ||
        _giacResult.setKind != numos::SolutionSetKind::Solutions) {
        return false;
    }

    if (_numEquations == 1) {
        if (!_omniResult.ok || _omniResult.hasComplexRoots) return false;

        std::vector<const cas::OmniSolution*> uniqueTutor;
        for (const auto& candidate : _omniResult.solutions) {
            bool duplicate = false;
            for (const auto* existing : uniqueTutor) {
                if (candidate.isExact && existing->isExact) {
                    duplicate = exactEqual(candidate.exact, existing->exact);
                } else if (!candidate.isExact && !existing->isExact) {
                    duplicate =
                        std::fabs(candidate.numeric - existing->numeric) <=
                        1e-10;
                }
                if (duplicate) break;
            }
            if (!duplicate) uniqueTutor.push_back(&candidate);
        }

        if (_giacResult.groups.size() != uniqueTutor.size()) return false;
        std::vector<bool> consumed(_giacResult.groups.size(), false);
        for (const auto* tutor : uniqueTutor) {
            bool found = false;
            for (std::size_t i = 0; i < _giacResult.groups.size(); ++i) {
                if (consumed[i] || _giacResult.groups[i].values.size() != 1)
                    continue;
                const vpam::ExactVal* exact =
                    tutor->isExact ? &tutor->exact : nullptr;
                const double* approximate =
                    tutor->isExact ? nullptr : &tutor->numeric;
                if (matches(_giacResult.groups[i].values[0],
                            exact, approximate)) {
                    consumed[i] = true;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    if (!_systemResult.ok || _giacResult.groups.size() != 1 ||
        _giacResult.groups[0].values.size() != _systemResult.numVars ||
        _systemResult.numVars != _numEquations) {
        return false;
    }

    for (int i = 0; i < _numEquations; ++i) {
        const auto& value = _giacResult.groups[0].values[
            static_cast<std::size_t>(i)];
        if (value.variable.size() != 1 ||
            value.variable[0] != _systemResult.vars[i] ||
            !matches(value, &_systemResult.solutions[i], nullptr)) {
            return false;
        }
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Build result display
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildGiacResultDisplay() {
    for (int i = 0; i < MAX_RESULTS; ++i) {
        _resultCanvas[i].stopCursorBlink();
        lv_obj_add_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
        _resultNode[i].reset();
        _resultRow[i] = nullptr;
    }
    _resultCount = 0;
    _resultKind = ResultKind::None;
    if (_resultFallback) {
        lv_label_set_text(_resultFallback, "");
        lv_obj_add_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);
    }

    auto showTextFallback = [&](const char* title, const std::string& text) {
        _resultKind = ResultKind::TextFallback;
        lv_label_set_text(_resultTitle, title);
        if (!_resultFallback) return;
        std::string visible = "Giac result (text presentation): ";
        visible += text.empty() ? "(no printable result)" : text;
        static constexpr std::size_t kUiTextLimit = 512;
        if (visible.size() > kUiTextLimit) {
            visible.resize(kUiTextLimit);
            visible += "...";
        }
        lv_label_set_text(_resultFallback, visible.c_str());
        lv_obj_remove_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);
    };

    if (!_giacResult.ok()) {
        const char* title = "Solve error";
        switch (_giacResult.status) {
            case numos::MathEngineStatus::ParseError:
                title = "Invalid equation";
                break;
            case numos::MathEngineStatus::Undefined:
                title = "Undefined";
                break;
            case numos::MathEngineStatus::Unsupported:
                title = "Unsupported solve";
                break;
            case numos::MathEngineStatus::OutOfMemory:
                title = "Not enough memory";
                break;
            default:
                break;
        }
        lv_label_set_text(_resultTitle, title);
        if (_resultFallback && !_giacResult.diagnostic.empty()) {
            lv_label_set_text(_resultFallback, _giacResult.diagnostic.c_str());
            lv_obj_remove_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (_giacResult.setKind == numos::SolutionSetKind::NoSolution) {
        _resultKind = ResultKind::Structured;
        lv_label_set_text(_resultTitle, "No solution");
        return;
    }
    if (_giacResult.setKind == numos::SolutionSetKind::AllValues) {
        _resultKind = ResultKind::Structured;
        lv_label_set_text(_resultTitle, "Infinitely many solutions");
        return;
    }
    if (_giacResult.setKind == numos::SolutionSetKind::Unsupported) {
        showTextFallback("Solution:", _giacResult.rawExactText);
        return;
    }

    std::size_t rowCount = 0;
    for (const auto& group : _giacResult.groups) {
        rowCount += group.values.size();
    }
    if (rowCount == 0 || rowCount > MAX_RESULTS) {
        showTextFallback("Solutions:", _giacResult.rawExactText);
        return;
    }

    std::vector<NodePtr> rows;
    rows.reserve(rowCount);
    const bool indexGroups = _giacResult.groups.size() > 1;
    for (std::size_t groupIndex = 0;
        groupIndex < _giacResult.groups.size(); ++groupIndex) {
        for (const auto& solution : _giacResult.groups[groupIndex].values) {
            if (solution.variable.empty()) {
                showTextFallback("Solutions:", _giacResult.rawExactText);
                return;
            }

            NodePtr value =
                numos::CalculationEngine::resultTreeToAST(solution.exactValue);
            if (!value) {
                showTextFallback("Solutions:", _giacResult.rawExactText);
                return;
            }

            auto row = makeRow();
            auto* resultRow = static_cast<NodeRow*>(row.get());
            NodePtr variable = makeRow();
            auto* variableRow = static_cast<NodeRow*>(variable.get());
            variableRow->appendChild(solution.variable.size() == 1
                ? makeVariable(solution.variable[0])
                : makeSymbol(solution.variable));
            if (indexGroups) {
                auto index = makeRow();
                static_cast<NodeRow*>(index.get())->appendChild(
                    makeNumber(std::to_string(groupIndex + 1)));
                auto indexed = makeRow();
                static_cast<NodeRow*>(indexed.get())->appendChild(
                    makeSubscript(std::move(variable), std::move(index)));
                variable = std::move(indexed);
            }
            resultRow->appendChild(makeEquation(
                std::move(variable), std::move(value),
                EquationKind::Assignment));
            rows.push_back(std::move(row));
        }
    }

    _resultKind = ResultKind::Structured;
    lv_label_set_text(_resultTitle,
                      _giacResult.groups.size() > 1
                          ? "Solutions:" : "Solution:");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        _resultNode[i] = std::move(rows[i]);
        _resultRow[i] = static_cast<NodeRow*>(_resultNode[i].get());

        const int y = 28 + static_cast<int>(i) * 38;
        lv_obj_set_pos(_resultCanvas[i].obj(), PAD + 20, y);
        lv_obj_set_size(_resultCanvas[i].obj(),
                        SCREEN_W - 2 * PAD - 20, 34);
        lv_obj_remove_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
        _resultCanvas[i].setExpression(_resultRow[i], nullptr);
        _resultRow[i]->calculateLayout(_resultCanvas[i].normalMetrics());
        _resultCanvas[i].invalidate();
        ++_resultCount;
    }
}

void EquationsApp::buildResultDisplay() {
    buildGiacResultDisplay();
    if (!_resultHint || lv_obj_get_parent(_resultHint) != _resultContainer) {
        _resultHint = lv_label_create(_resultContainer);
        lv_obj_set_style_text_font(_resultHint, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(_resultHint, lv_color_hex(COL_HINT_HEX),
                                    LV_PART_MAIN);
    }
    lv_label_set_text(_resultHint, "STEPS: View steps    AC: Back");
    lv_obj_set_pos(_resultHint, PAD, SCREEN_H - BAR_H - 22);
}

// ════════════════════════════════════════════════════════════════════════════
// Build steps display
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildStepsDisplay() {
    // ── 1. Clean up previous content (persistent container) ─────────
    if (!isValidLvObj(_stepsContainer)) {
        _stepsContainer = nullptr;
        return;
    }
    lv_obj_clean(_stepsContainer);
    _stepRenderers.clear();

    const cas::CASStepLogger& log = _isOmniSolve
        ? _omniResult.steps : _systemResult.steps;

    const auto& steps = log.steps();

    static constexpr int CANVAS_W = SCREEN_W - 2 * PAD - 16;

    auto createStepLabel = [&](int stepIndex) -> lv_obj_t* {
        if (!isValidLvObj(_stepsContainer)) return nullptr;
        logStepBudget(stepIndex);
        if (!canAllocStep(STEPS_LABEL_BUDGET)) return nullptr;
        return lv_label_create(_stepsContainer);
    };

    // Helper: create a MathCanvas from a NodePtr, add to _stepRenderers
    auto emitCanvas = [&](int stepIndex, vpam::NodePtr node) -> bool {
        if (!node) return true;
        if (!isValidLvObj(_stepsContainer)) return false;

        logStepBudget(stepIndex);
        if (!canAllocStep(STEPS_CANVAS_BUDGET)) return false;

        node = cas::SymExprToAST::ensureRow(std::move(node));
        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData = std::move(node);
        srd->canvas.create(_stepsContainer);
        if (!srd->canvas.obj()) return false;

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = row->layout().width + 24;
        int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
        if (w > CANVAS_W) w = CANVAS_W;
        if (h < 22) h = 22;
        lv_obj_set_size(srd->canvas.obj(), w, h);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
        return true;
    };

    if (steps.empty()) {
        lv_obj_t* lbl = createStepLabel(0);
        if (!lbl) {
            failClosedStepRender(0);
            return;
        }
        lv_label_set_text(lbl, "No steps available.");
        lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    // ── 2. Iterate all steps ───────────────────────────────────────
    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];

        // Color: orange accent for results, green for normal steps
        uint32_t descColor = COL_DESC_HEX;
        if (step.kind == cas::StepKind::Result
            || step.kind == cas::StepKind::ComplexResult)
            descColor = COL_ACCENT_HEX;

        // ── 2a. Text description label ─────────────────────────────
        // Only create a label if there IS a description to show
        if (!step.description.empty()) {
            char buf[200];
            if (step.kind == cas::StepKind::ComplexResult
                && _isOmniSolve && _omniResult.hasComplexRoots)
            {
                snprintf(buf, sizeof(buf), "%d. Complex conjugate roots:",
                         (int)(i + 1));
            } else {
                snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                         step.description.c_str());
            }

            lv_obj_t* descLbl = createStepLabel(static_cast<int>(i + 1));
            if (!descLbl) {
                failClosedStepRender(i + 1);
                return;
            }
            lv_label_set_text(descLbl, buf);
            lv_obj_set_width(descLbl, SCREEN_W - 2 * PAD - 8);
            lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(descLbl, &stix_math_18,
                                       LV_PART_MAIN);
            lv_obj_set_style_text_color(descLbl, lv_color_hex(descColor),
                                        LV_PART_MAIN);
        }

        // ── 2b. MathCanvas for CAS mathExpr ────────────────────────
        // Any step with a SymExpr tree gets a mandatory MathCanvas
        if (step.mathExpr) {
            vpam::NodePtr astNode = cas::SymExprToAST::convert(step.mathExpr);
            if (!emitCanvas(static_cast<int>(i + 1), std::move(astNode))) {
                failClosedStepRender(i + 1);
                return;
            }
        }

        // ── 2c. MathCanvas for equation snapshots ──────────────────
        // Transform/Result steps may carry a SymEquation snapshot
        if ((step.kind == cas::StepKind::Transform
             || step.kind == cas::StepKind::Result)
            && !step.mathExpr)
        {
            std::string eqText = step.snapshot.toString();
            if (!eqText.empty() && eqText != "0" && eqText != "0 = 0") {
                vpam::NodePtr snapNode =
                    cas::SymToAST::fromSymEquation(step.snapshot);
                if (!emitCanvas(static_cast<int>(i + 1), std::move(snapNode))) {
                    failClosedStepRender(i + 1);
                    return;
                }
            }
        }

        // ── 2d. ComplexResult: two root canvases ───────────────────
        if (step.kind == cas::StepKind::ComplexResult
            && _isOmniSolve && _omniResult.hasComplexRoots)
        {
            using namespace vpam;
            const auto& re  = _omniResult.complexReal;
            const auto& im  = _omniResult.complexImagMag;
            char var = _omniResult.variable;

            auto appendExactVPAM = [](NodeRow* r, const ExactVal& val) {
                NodePtr node = cas::SymToAST::fromExactVal(val);
                if (node->type() == NodeType::Row) {
                    auto* vr = static_cast<NodeRow*>(node.get());
                    while (vr->childCount() > 0)
                        r->appendChild(vr->removeChild(0));
                } else {
                    r->appendChild(std::move(node));
                }
            };

            // Root 1: x1 = re + im·i
            {
                auto row = makeRow();
                auto* r  = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(var));
                r->appendChild(makeNumber("1"));
                r->appendChild(makeRelation(OpKind::Eq));
                appendExactVPAM(r, re);
                r->appendChild(makeOperator(OpKind::Add));
                appendExactVPAM(r, im);
                r->appendChild(makeConstant(ConstKind::Imag));
                if (!emitCanvas(static_cast<int>(i + 1), std::move(row))) {
                    failClosedStepRender(i + 1);
                    return;
                }
            }

            // Root 2: x2 = re - im·i
            {
                auto row = makeRow();
                auto* r  = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(var));
                r->appendChild(makeNumber("2"));
                r->appendChild(makeRelation(OpKind::Eq));
                appendExactVPAM(r, re);
                r->appendChild(makeOperator(OpKind::Sub));
                appendExactVPAM(r, im);
                r->appendChild(makeConstant(ConstKind::Imag));
                if (!emitCanvas(static_cast<int>(i + 1), std::move(row))) {
                    failClosedStepRender(i + 1);
                    return;
                }
            }
        }
    }

    // ── 3. Footer hint ─────────────────────────────────────────────
    lv_obj_t* hintLbl = createStepLabel(static_cast<int>(steps.size()));
    if (!hintLbl) {
        failClosedStepRender(steps.size());
        return;
    }
    lv_label_set_text(hintLbl,
                      LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
    lv_obj_set_style_text_font(hintLbl, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX),
                                LV_PART_MAIN);
}

// ════════════════════════════════════════════════════════════════════════════
// buildCASStepsDisplay — Algebraic TRS step viewer
//
// Parses the first equation from the edit canvas, runs it through the
// RuleEngine + makeAlgebraicRules pipeline, and populates the steps
// container with description labels + MathCanvas widgets.
//
// This replaces the old TutorApp solve path and shows exact rational
// results (e.g. x = 3/322) as vertical fraction bars.
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildCASStepsDisplay() {
    if (!isValidLvObj(_stepsContainer)) {
        _stepsContainer = nullptr;
        return;
    }
    lv_obj_clean(_stepsContainer);
    _stepRenderers.clear();

    static constexpr int16_t CANVAS_MAX_W =
        static_cast<int16_t>(SCREEN_W - 2 * PAD - 8);

    auto createScopedLabel = [&](int stepIndex) -> lv_obj_t* {
        if (!isValidLvObj(_stepsContainer)) return nullptr;
        logStepBudget(stepIndex);
        if (!canAllocStep(STEPS_LABEL_BUDGET)) return nullptr;
        return lv_label_create(_stepsContainer);
    };

    auto appendInfoLabel = [&](const char* text,
                               uint32_t colorHex,
                               int stepIndex = -1) -> bool {
        lv_obj_t* lbl = createScopedLabel(stepIndex);
        if (!lbl) return false;
        lv_label_set_text(lbl, text);
        lv_obj_set_width(lbl, CANVAS_MAX_W);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(colorHex), LV_PART_MAIN);
        return true;
    };

    auto appendMemoryLimitReached = [&](std::size_t stepIndex = 0U) {
        failClosedStepRender(stepIndex);
    };

    // ── 1. Serialise the equation NodeRow to a text string ─────────────
    std::string eqText;
    if (_eqRowData[0]) {
        nodeToText(_eqRowData[0], eqText);
    }

    // Verify it contains '='
    if (eqText.find('=') == std::string::npos) {
        appendInfoLabel("Parse error: equation must include '='.", COL_ACCENT_HEX);
        appendInfoLabel("STEPS unavailable for this input.", COL_HINT_HEX);
        return;
    }

    // ── 2. Parse text → cas::EquationNode ──────────────────────────────
    _casPool.reset();
    _casEngine.reset();
    _casPool   = std::make_unique<cas::CasMemoryPool>();
    if (!_casPool || !_casPool->valid()) {
        appendMemoryLimitReached();
        return;
    }
    _casEngine = std::make_unique<cas::RuleEngine>(*_casPool);
    _hasCasResult = false;

    char var = 'x';
    cas::NodePtr parsedEq = casParseEquation(eqText, *_casPool, var);
    if (!parsedEq) {
        appendInfoLabel("CAS parse failed: unsupported equation format.",
                        COL_ACCENT_HEX);
        appendInfoLabel("Try a simpler algebraic form.", COL_HINT_HEX);
        return;
    }

    // ── 3. Load rules & solve ──────────────────────────────────────────
    for (auto& rule : cas::makeAlgebraicRules(*_casPool, var))
        _casEngine->addRule(std::move(rule));

    _casResult    = _casEngine->applyToFixedPoint(parsedEq, 80);
    _hasCasResult = true;
    cas::checkNonLinearHandover(_casResult, var);

    const auto& steps = _casResult.steps;

    // Helper: emit a MathCanvas for a CAS tree with optional highlight
    auto emitCasCanvas = [&](const cas::NodePtr& tree,
                              const cas::NodePtr& highlight,
                              lv_color_t hlColor)
    {
        if (!tree) return true;
        if (!isValidLvObj(_stepsContainer)) return false;
        logStepBudget(-1);
        if (!canAllocStep(STEPS_CANVAS_BUDGET)) return false;

        const vpam::MathNode* hlPtr = nullptr;
        vpam::NodePtr vpamTree;
        if (highlight) {
            vpamTree = cas::CasToVpam::convert(tree, highlight, &hlPtr);
        } else {
            vpamTree = cas::CasToVpam::convert(tree);
        }
        if (!vpamTree) return true;

        // Wrap in a Row if needed
        if (vpamTree->type() != vpam::NodeType::Row) {
            auto rowWrap = vpam::makeRow();
            static_cast<vpam::NodeRow*>(rowWrap.get())
                ->appendChild(std::move(vpamTree));
            vpamTree = std::move(rowWrap);
        }

        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData = std::move(vpamTree);
        srd->canvas.create(_stepsContainer);
        if (!srd->canvas.obj()) {
            printf("[CAS] emitCasCanvas: lv_obj_create failed\n");
            return false;
        }

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = static_cast<int16_t>(row->layout().width + 24);
        int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
        if (h < 20) h = 20;
        lv_obj_set_size(srd->canvas.obj(), w, h);

        if (hlPtr) srd->canvas.setHighlightNode(hlPtr, hlColor);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
        return true;
    };

    bool memoryLimitHit = false;

    // ── 4a. Step 0 — Original Equation (always shown first) ─────────
    if (_stepsContainer != nullptr) {
        lv_obj_t* descLbl = createScopedLabel(0);
        if (!descLbl) {
            appendMemoryLimitReached(0);
            return;
        }
        lv_label_set_text(descLbl, "0. Original Equation");
        lv_obj_set_width(descLbl, CANVAS_MAX_W);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(COL_DESC_HEX), LV_PART_MAIN);
        if (!emitCasCanvas(parsedEq, nullptr, lv_color_hex(0x4FC3F7))) {
            appendMemoryLimitReached(0);
            return;
        }
    }

    if (steps.empty()) {
        if (_stepsContainer != nullptr) {
            lv_obj_t* lbl = createScopedLabel(0);
            if (!lbl) {
                appendMemoryLimitReached(0);
                return;
            }
            lv_label_set_text(lbl, "No further steps available.");
            lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        }
        return;
    }

    // ── 4b. Iterate steps ────────────────────────────────────
    for (std::size_t i = 0; i < steps.size(); ++i) {
        if (!isValidLvObj(_stepsContainer)) return;

        const auto& step = steps[i];

        bool isFinal = (i == steps.size() - 1)
                       || (step.ruleName == cas::RULE_NONLINEAR_HANDOVER);
        uint32_t descColHex = isFinal ? COL_ACCENT_HEX : COL_DESC_HEX;
        lv_color_t hlColor  = isFinal ? lv_color_hex(0xFFB300)
                                      : lv_color_hex(0x4FC3F7);

        // Description label
        char buf[120];
        if (!step.ruleDesc.empty())
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.ruleDesc.c_str());
        else
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.ruleName.c_str());

        lv_obj_t* descLbl = createScopedLabel(static_cast<int>(i + 1));
        if (!descLbl) {
            memoryLimitHit = true;
            break;
        }
        lv_label_set_text(descLbl, buf);
        lv_obj_set_width(descLbl, CANVAS_MAX_W);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &stix_math_18,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(descColHex),
                                    LV_PART_MAIN);

        // MathCanvas for this step's equation tree
        if (step.tree) {
            if (!emitCasCanvas(step.tree, step.affectedNode, hlColor)) {
                memoryLimitHit = true;
                break;
            }
        }
    }

    if (memoryLimitHit) {
        appendMemoryLimitReached(steps.size());
        return;
    }

    // ── 5. Footer hint ───────────────────────────────────────
    if (_stepsContainer != nullptr) {
        lv_obj_t* hintLbl = createScopedLabel(static_cast<int>(steps.size()));
        if (!hintLbl) {
            appendMemoryLimitReached(steps.size());
            return;
        }
        lv_label_set_text(hintLbl, LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
        lv_obj_set_style_text_font(hintLbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX),
                                    LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// buildSystemCASStepsDisplay  —  SystemTutor step view for 2x2 systems
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildSystemCASStepsDisplay() {
    if (!isValidLvObj(_stepsContainer)) {
        _stepsContainer = nullptr;
        return;
    }
    lv_obj_clean(_stepsContainer);
    _stepRenderers.clear();

    static constexpr int16_t CANVAS_MAX_W =
        static_cast<int16_t>(SCREEN_W - 2 * PAD - 8);

    auto createScopedLabel = [&](int stepIndex) -> lv_obj_t* {
        if (!isValidLvObj(_stepsContainer)) return nullptr;
        logStepBudget(stepIndex);
        if (!canAllocStep(STEPS_LABEL_BUDGET)) return nullptr;
        return lv_label_create(_stepsContainer);
    };

    auto appendInfoLabel = [&](const char* text,
                               uint32_t colorHex,
                               int stepIndex = -1) -> bool {
        lv_obj_t* lbl = createScopedLabel(stepIndex);
        if (!lbl) return false;
        lv_label_set_text(lbl, text);
        lv_obj_set_width(lbl, CANVAS_MAX_W);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(colorHex), LV_PART_MAIN);
        return true;
    };

    // ── 1. Serialise both equation rows to text ────────────────────────
    std::string eq1Text, eq2Text;
    if (_eqRowData[0]) nodeToText(_eqRowData[0], eq1Text);
    if (_eqRowData[1]) nodeToText(_eqRowData[1], eq2Text);

    if (eq1Text.find('=') == std::string::npos ||
        eq2Text.find('=') == std::string::npos) {
        appendInfoLabel("Parse error: invalid system equations.", COL_ACCENT_HEX);
        appendInfoLabel("Both equations must include '='.", COL_HINT_HEX);
        return;
    }

    // ── 2. Parse equations → cas::EquationNode ────────────────────────
    _casPool.reset();
    _casPool = std::make_unique<cas::CasMemoryPool>();

    char var1 = 'x', var2 = 'y';
    cas::NodePtr parsedEq1 = casParseEquation(eq1Text, *_casPool, var1);
    cas::NodePtr parsedEq2 = casParseEquation(eq2Text, *_casPool, var2);
    if (!parsedEq1 || !parsedEq2) {
        appendInfoLabel("CAS parse failed: unsupported system format.",
                        COL_ACCENT_HEX);
        appendInfoLabel("Try simpler linear equations.", COL_HINT_HEX);
        return;
    }

    // ── 3. Run SystemTutor ────────────────────────────────────────────
    cas::SystemTutorResult result = cas::SystemTutor::solveSystem(
        parsedEq1, parsedEq2, *_casPool, var1, var2);

    // Helper: emit a MathCanvas for a CAS tree as a child of `_stepsContainer`
    auto emitSysCasCanvas = [&](const cas::NodePtr& tree) {
        if (!tree) return true;
        if (!isValidLvObj(_stepsContainer)) return false;
        logStepBudget(-1);
        if (!canAllocStep(STEPS_CANVAS_BUDGET)) return false;

        vpam::NodePtr vpamTree = cas::CasToVpam::convert(tree);
        if (!vpamTree) return true;

        if (vpamTree->type() != vpam::NodeType::Row) {
            auto rowWrap = vpam::makeRow();
            static_cast<vpam::NodeRow*>(rowWrap.get())
                ->appendChild(std::move(vpamTree));
            vpamTree = std::move(rowWrap);
        }

        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData = std::move(vpamTree);
        srd->canvas.create(_stepsContainer);
        if (!srd->canvas.obj()) {
            printf("[SYS] emitSysCasCanvas: lv_obj_create failed\n");
            return false;
        }

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = static_cast<int16_t>(row->layout().width + 24);
        int16_t h = vpam::mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
        if (h < 20) h = 20;
        lv_obj_set_size(srd->canvas.obj(), w, h);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
        return true;
    };

    auto appendMemoryLimitReached = [&](std::size_t stepIndex = 0U) {
        failClosedStepRender(stepIndex);
    };

    // ── 4. Header ─────────────────────────────────────────────────────
    lv_obj_t* headerLbl = createScopedLabel(0);
    if (!headerLbl) {
        appendMemoryLimitReached(0);
        return;
    }
    lv_label_set_text(headerLbl, "System of Equations");
    lv_obj_set_width(headerLbl, CANVAS_MAX_W);
    lv_label_set_long_mode(headerLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(headerLbl, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(headerLbl, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);

    // ── 5. Original equations ──────────────────────────────────────────
    lv_obj_t* origLbl = createScopedLabel(0);
    if (!origLbl) {
        appendMemoryLimitReached(0);
        return;
    }
    lv_label_set_text(origLbl, "0. Original System");
    lv_obj_set_width(origLbl, CANVAS_MAX_W);
    lv_label_set_long_mode(origLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(origLbl, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(origLbl, lv_color_hex(COL_DESC_HEX), LV_PART_MAIN);
    if (!emitSysCasCanvas(parsedEq1) || !emitSysCasCanvas(parsedEq2)) {
        appendMemoryLimitReached(0);
        return;
    }

    if (result.steps.empty()) {
        lv_obj_t* lbl = createScopedLabel(0);
        if (!lbl) {
            appendMemoryLimitReached(0);
            return;
        }
        lv_label_set_text(lbl, "No further steps available.");
        lv_obj_set_style_text_font(lbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    // ── 6. Iterate SystemTutor steps ───────────────────────────────────
    for (std::size_t i = 0; i < result.steps.size(); ++i) {
        if (!isValidLvObj(_stepsContainer)) return;

        const auto& step = result.steps[i];

        // Description label
        char buf[160];
        if (!step.ruleDesc.empty())
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.ruleDesc.c_str());
        else
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.ruleName.c_str());

        logStepBudget(static_cast<int>(i + 1));
        if (!canAllocStep(STEPS_CANVAS_BUDGET)) {
            appendMemoryLimitReached(i + 1);
            return;
        }

        lv_obj_t* stepRow = lv_obj_create(_stepsContainer);
        if (!stepRow) {
            appendMemoryLimitReached(i + 1);
            return;
        }
        lv_obj_set_width(stepRow, CANVAS_MAX_W);
        lv_obj_set_height(stepRow, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(stepRow, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(stepRow, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(stepRow, 0, LV_PART_MAIN);
        lv_obj_set_layout(stepRow, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(stepRow, LV_FLEX_FLOW_COLUMN);
        lv_obj_remove_flag(stepRow, LV_OBJ_FLAG_SCROLLABLE);

        logStepBudget(static_cast<int>(i + 1));
        if (!canAllocStep(STEPS_LABEL_BUDGET)) {
            appendMemoryLimitReached(i + 1);
            return;
        }

        lv_obj_t* descLbl = lv_label_create(stepRow);
        if (!descLbl) {
            appendMemoryLimitReached(i + 1);
            return;
        }
        lv_label_set_text(descLbl, buf);
        lv_obj_set_width(descLbl, CANVAS_MAX_W);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(COL_DESC_HEX), LV_PART_MAIN);

        if ((step.eq1Tree && !emitSysCasCanvas(step.eq1Tree)) ||
            (step.eq2Tree && !emitSysCasCanvas(step.eq2Tree))) {
            appendMemoryLimitReached(i + 1);
            return;
        }
    }

    // ── 7. Footer hint ────────────────────────────────────────────────
    if (_stepsContainer != nullptr) {
        lv_obj_t* hintLbl = createScopedLabel(static_cast<int>(result.steps.size()));
        if (!hintLbl) {
            appendMemoryLimitReached(result.steps.size());
            return;
        }
        lv_label_set_text(hintLbl, LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
        lv_obj_set_style_text_font(hintLbl, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    }
}

