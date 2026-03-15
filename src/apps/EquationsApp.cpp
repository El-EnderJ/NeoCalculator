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
#include "../math/MathAST.h"
#include "../math/cas/SymToAST.h"
#include "../math/cas/SymExprToAST.h"
#include "../math/cas/SymExprArena.h"
#include "../math/cas/AlgebraicRules.h"
#include "../math/cas/CasToVpam.h"
#include "../math/cas/PersistentAST.h"
#include "../math/cas/RuleEngine.h"
#include <cstring>
#include <cstdlib>

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
    _statusBar.destroy();

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
        _stepsContainer    = nullptr;
        for (int i = 0; i < MAX_EQS; ++i) {
            _eqRows[i] = nullptr;
            _eqLabels[i] = nullptr;
        }
        for (int i = 0; i < NUM_TEMPLATES; ++i) {
            _templateCanvas[i].destroy();
            _templateNode[i].reset();
            _templateItems[i] = nullptr;
            _templateLabels[i] = nullptr;
        }
    }

    _state = State::EQ_LIST;
    _numEquations = 0;
    _listFocus = 0;
    _resultCount = 0;

    _omniResult.steps.clear();
    _systemResult.steps.clear();
    _arena.reset();
}

void EquationsApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();

    if (_state == State::EDITING) {
        _editCanvas.startCursorBlink();
    }
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
        lv_obj_set_style_text_font(_eqLabels[i], &lv_font_montserrat_14, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(_addLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_addLabel, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_align(_addLabel, LV_ALIGN_LEFT_MID, 10, 0);

    // "Solve" button row
    _solveRow = createRowContainer(_listContainer, addY + ROW_H + ROW_GAP, rowW, ROW_H, COL_SOLVE_HEX);
    _solveLabel = lv_label_create(_solveRow);
    lv_label_set_text(_solveLabel, "Solve the equation");
    lv_obj_set_style_text_font(_solveLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_solveLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(_solveLabel, LV_ALIGN_CENTER, 0, 0);

    // Hint at bottom
    _listHint = lv_label_create(_listContainer);
    lv_label_set_text(_listHint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   ENTER Select   DEL Remove");
    lv_obj_set_style_text_font(_listHint, &lv_font_montserrat_12, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(_templateTitle, &lv_font_montserrat_14, LV_PART_MAIN);
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
        lv_obj_set_style_text_font(_templateLabels[i], &lv_font_montserrat_14, LV_PART_MAIN);

        // MathCanvas for the formula (templates 1-3 have math)
        if (i > 0) {
            _templateNode[i] = buildTemplateAST(i);
            auto* row = static_cast<vpam::NodeRow*>(_templateNode[i].get());
            _templateCanvas[i].create(_templateItems[i]);
            _templateCanvas[i].setExpression(row, nullptr);
            row->calculateLayout(_templateCanvas[i].normalMetrics());
            int16_t cw = row->layout().width + 8;
            int16_t ch = row->layout().ascent + row->layout().descent + 4;
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
    lv_obj_set_style_text_font(_editTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_editTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_editTitle, PAD, 4);

    _editCanvas.create(_editContainer);
    lv_obj_set_pos(_editCanvas.obj(), PAD + 4, 28);
    lv_obj_set_size(_editCanvas.obj(), SCREEN_W - 2 * PAD - 8, 120);

    _editHint = lv_label_create(_editContainer);
    lv_label_set_text(_editHint, LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Move   "
                                  "= Equals   ENTER Confirm   AC Cancel");
    lv_obj_set_style_text_font(_editHint, &lv_font_montserrat_12, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(_solvingLabel, &lv_font_montserrat_14, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(_resultTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_resultTitle, PAD, 4);

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
    hideAllContainers();
    _state = State::EQ_LIST;
    _statusBar.setTitle("Equations");

    refreshAllPreviews();
    updateListFocus();

    lv_obj_remove_flag(_listContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showTemplate() {
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
    hideAllContainers();
    _state = State::RESULT;
    _statusBar.setTitle("Result");

    buildResultDisplay();

    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showSteps() {
    hideAllContainers();
    _state = State::STEPS;
    _statusBar.setTitle("Steps");
    _stepScroll = 0;

    // For 2-equation systems, use the SystemTutor CAS step display.
    if (_numEquations == 2 && _eqRowData[0] && _eqRowData[1]) {
        buildSystemCASStepsDisplay();
    }
    // For single-equation solves, try to build algebraic CAS steps
    // using the RuleEngine.  Fall back to the OmniSolver step log if the
    // equation cannot be parsed by the simple TRS parser.
    else if (_isOmniSolve && _numEquations == 1 && _eqRowData[0]) {
        buildCASStepsDisplay();
    } else {
        buildStepsDisplay();
    }

    if (_stepsContainer == nullptr) {
        return;
    }
    lv_obj_remove_flag(_stepsContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
    lv_obj_invalidate(_screen);
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
                    lv_obj_set_style_text_color(_templateLabels[i],
                        (i == _templateFocus) ? lv_color_hex(COL_FOCUS_HEX) : lv_color_black(),
                        LV_PART_MAIN);
                }
                lv_obj_invalidate(_templateOverlay);
            }
            break;

        case KeyCode::DOWN:
            if (_templateFocus < NUM_TEMPLATES - 1) {
                ++_templateFocus;
                for (int i = 0; i < NUM_TEMPLATES; ++i) {
                    lv_obj_set_style_text_color(_templateLabels[i],
                        (i == _templateFocus) ? lv_color_hex(COL_FOCUS_HEX) : lv_color_black(),
                        LV_PART_MAIN);
                }
                lv_obj_invalidate(_templateOverlay);
            }
            break;

        case KeyCode::ENTER: {
            // Apply template and go to editor
            int slot = _numEquations;
            if (slot >= MAX_EQS) { break; }

            applyTemplate(_templateFocus, slot);
            ++_numEquations;

            // Hide template overlay
            lv_obj_add_flag(_templateOverlay, LV_OBJ_FLAG_HIDDEN);

            // Go to editing mode for the new equation
            showEditing(slot);
            break;
        }

        case KeyCode::AC:
        case KeyCode::DEL:
            // Cancel template, back to list
            lv_obj_add_flag(_templateOverlay, LV_OBJ_FLAG_HIDDEN);
            _state = State::EQ_LIST;
            lv_obj_invalidate(_screen);
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
        case KeyCode::VAR_Y: cc.insertVariable('y'); changed = true; break;

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
            lv_obj_scroll_by(_stepsContainer, 0, 30, LV_ANIM_ON);
            break;
        case KeyCode::DOWN:
            lv_obj_scroll_by(_stepsContainer, 0, -30, LV_ANIM_ON);
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
            r->appendChild(makeVariable('='));
            r->appendChild(makeNumber("0"));
            break;

        case 2: { // Exponential: a·e^x + b = 0
            r->appendChild(makeVariable('a'));
            r->appendChild(makePower(
                makeConstant(ConstKind::E), makeVariable('x')));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('b'));
            r->appendChild(makeVariable('='));
            r->appendChild(makeNumber("0"));
            break;
        }

        case 3: { // Logarithmic: ln(x) + a = 0
            r->appendChild(makeFunction(FuncKind::Ln, makeVariable('x')));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('a'));
            r->appendChild(makeVariable('='));
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

    int contentH = _editRow->layout().ascent + _editRow->layout().descent;
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
    if (!row) return false;

    int eqIdx = -1;
    int count = row->childCount();

    for (int i = 0; i < count; ++i) {
        const MathNode* ch = row->child(i);
        if (ch->type() == NodeType::Variable) {
            const auto* v = static_cast<const NodeVariable*>(ch);
            if (v->name() == '=') {
                eqIdx = i;
                break;
            }
        }
    }

    if (eqIdx < 0) {
        outLHS = cloneNode(row);
        outRHS = makeRow();
        static_cast<NodeRow*>(outRHS.get())->appendChild(makeNumber("0"));
        return false;
    }

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

    if (lhsRow->isEmpty()) lhsRow->appendChild(makeNumber("0"));
    if (rhsRow->isEmpty()) rhsRow->appendChild(makeNumber("0"));

    return true;
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
    if (_numEquations == 1) {
        _isOmniSolve = true;
        solveOmni();
    } else {
        _isOmniSolve = false;
        solveSystem();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// solveOmni — OmniSolver pipeline
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::solveOmni() {
    showSolving();

    _arena.reset();

    cas::ASTFlattener flattener;
    flattener.setArena(&_arena);

    NodePtr lhs, rhs;
    splitAtEquals(_eqRowData[0], lhs, rhs);

    cas::SymExpr* lhsExpr = flattener.flattenToExpr(lhs.get());
    cas::SymExpr* rhsExpr = flattener.flattenToExpr(rhs.get());

    if (!lhsExpr || !rhsExpr) {
        _omniResult = cas::OmniResult();
        _omniResult.ok = false;
        _omniResult.error = "Syntax error";
        showResult();
        return;
    }

    char var = 'x';

    cas::OmniSolver solver;
    _omniResult = solver.solve(lhsExpr, rhsExpr, var, _arena);

    if (!_omniResult.ok) {
        _statusBar.setTitle("No solution");
        _statusBar.update();
    }

    showResult();
}

// ════════════════════════════════════════════════════════════════════════════
// solveSystem — System of equations pipeline
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::solveSystem() {
    _isNLSolve = false;
    cas::ASTFlattener flattener;

    int numEqs = _numEquations;
    const char vars2[] = {'x', 'y'};
    const char vars3[] = {'x', 'y', 'z'};
    const char* vars = (numEqs == 2) ? vars2 : vars3;
    int numVars = numEqs;

    showSolving();

    // ── Try linear path first ───────────────────────────────────────
    cas::LinEq linEqs[3];
    bool linearOk = true;

    for (int i = 0; i < numEqs; ++i) {
        NodePtr lhs, rhs;
        splitAtEquals(_eqRowData[i], lhs, rhs);

        if (!_eqRowData[i]) {
            _systemResult = cas::SystemResult();
            _systemResult.ok = false;
            _systemResult.error = "Empty equation";
            showResult();
            return;
        }

        auto eqResult = flattener.flattenEquation(lhs.get(), rhs.get());
        if (!eqResult.ok) {
            linearOk = false;
            break;
        }

        linEqs[i] = symEquationToLinEq(
            cas::SymEquation(eqResult.eq.lhs, eqResult.eq.rhs),
            vars, numVars);
    }

    if (linearOk) {
        cas::SystemSolver sysSolver;
        if (numEqs == 2) {
            _arena.reset();
            _systemResult = sysSolver.solve2x2(linEqs[0], linEqs[1], 'x', 'y', &_arena);
        } else {
            _systemResult = sysSolver.solve3x3(
                linEqs[0], linEqs[1], linEqs[2]);
        }

        if (_systemResult.ok) {
            showResult();
            return;
        }
    }

    // ── Nonlinear fallback for 2×2 systems ──────────────────────────
    if (numEqs == 2) {
        _arena.reset();
        flattener.setArena(&_arena);

        cas::SymExpr* exprs[2] = {nullptr, nullptr};
        for (int i = 0; i < 2; ++i) {
            NodePtr lhs, rhs;
            splitAtEquals(_eqRowData[i], lhs, rhs);

            cas::SymExpr* lhsExpr = flattener.flattenToExpr(lhs.get());
            cas::SymExpr* rhsExpr = flattener.flattenToExpr(rhs.get());

            if (!lhsExpr || !rhsExpr) {
                _systemResult = cas::SystemResult();
                _systemResult.ok = false;
                _systemResult.error = "Syntax error";
                showResult();
                return;
            }

            exprs[i] = symAdd(_arena, lhsExpr, symNeg(_arena, rhsExpr));
        }

        cas::SystemSolver sysSolver;
        _nlResult = sysSolver.solveNonlinear2x2(
            exprs[0], exprs[1], 'x', 'y', _arena);
        _isNLSolve = true;

        if (!_nlResult.ok) {
            _statusBar.setTitle(_nlResult.error.empty()
                                    ? "No solution" : "Error");
            _statusBar.update();
        }
        showResult();
        return;
    }

    // ── Linear path failed, nonlinear fallback for 3×3 ────────────
    if (numEqs == 3) {
        _arena.reset();
        flattener.setArena(&_arena);

        cas::SymExpr* exprs[3] = {nullptr, nullptr, nullptr};
        bool exprOk = true;
        for (int i = 0; i < 3; ++i) {
            NodePtr lhs, rhs;
            splitAtEquals(_eqRowData[i], lhs, rhs);

            cas::SymExpr* lhsExpr = flattener.flattenToExpr(lhs.get());
            cas::SymExpr* rhsExpr = flattener.flattenToExpr(rhs.get());

            if (!lhsExpr || !rhsExpr) {
                exprOk = false;
                break;
            }

            exprs[i] = symAdd(_arena, lhsExpr, symNeg(_arena, rhsExpr));
        }

        if (exprOk) {
            cas::SystemSolver sysSolver;
            _nlResult = sysSolver.solveNonlinear3x3(
                exprs[0], exprs[1], exprs[2], 'x', 'y', 'z', _arena);
            _isNLSolve = true;

            if (!_nlResult.ok) {
                _statusBar.setTitle(_nlResult.error.empty()
                                        ? "No solution" : "Error");
                _statusBar.update();
            }
            showResult();
            return;
        }
    }

    if (!_systemResult.ok) {
        _statusBar.setTitle(_systemResult.error.empty()
                                ? "No solution" : "Error");
        _statusBar.update();
    }
    showResult();
}

// ════════════════════════════════════════════════════════════════════════════
// Build result display
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildResultDisplay() {
    // Clear previous results
    for (int i = 0; i < MAX_RESULTS; ++i) {
        _resultCanvas[i].stopCursorBlink();
        lv_obj_add_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
        _resultNode[i].reset();
        _resultRow[i] = nullptr;
    }
    _resultCount = 0;

    if (_isOmniSolve) {
        // ── OmniSolver result ──────────────────────────────────────
        if (!_omniResult.ok) {
            lv_label_set_text(_resultTitle,
                _omniResult.error.empty() ? "No real solution"
                                          : _omniResult.error.c_str());
            goto addHint;
        }

        // Handle complex roots (negative discriminant)
        if (_omniResult.hasComplexRoots) {
            lv_label_set_text(_resultTitle, "Complex roots:");

            const auto& re = _omniResult.complexReal;
            const auto& im = _omniResult.complexImagMag;

            // Helper: append ExactVal nodes from SymToAST into a row
            auto appendExactNodes = [](NodeRow* r, const vpam::ExactVal& val) {
                NodePtr node = cas::SymToAST::fromExactVal(val);
                if (node->type() == NodeType::Row) {
                    auto* vr = static_cast<NodeRow*>(node.get());
                    while (vr->childCount() > 0)
                        r->appendChild(vr->removeChild(0));
                } else {
                    r->appendChild(std::move(node));
                }
            };

            // Root 1: x₁ = re + im·𝑖
            {
                auto row = makeRow();
                auto* r = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(_omniResult.variable));
                r->appendChild(makeNumber("1"));
                r->appendChild(makeVariable('='));

                // Real part
                appendExactNodes(r, re);

                // + operator
                r->appendChild(makeOperator(OpKind::Add));

                // Imaginary magnitude (rendered as fraction/radical)
                appendExactNodes(r, im);

                // Italic 'i' — mathematical constant style
                r->appendChild(makeConstant(ConstKind::Imag));

                _resultNode[0] = std::move(row);
                _resultRow[0]  = static_cast<NodeRow*>(_resultNode[0].get());

                lv_obj_set_pos(_resultCanvas[0].obj(), PAD + 20, 28);
                lv_obj_set_size(_resultCanvas[0].obj(), SCREEN_W - 2 * PAD - 20, 38);
                lv_obj_remove_flag(_resultCanvas[0].obj(), LV_OBJ_FLAG_HIDDEN);
                _resultCanvas[0].setExpression(_resultRow[0], nullptr);
                _resultRow[0]->calculateLayout(_resultCanvas[0].normalMetrics());
                _resultCanvas[0].invalidate();
                ++_resultCount;
            }

            // Root 2: x₂ = re - im·𝑖
            {
                auto row = makeRow();
                auto* r = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(_omniResult.variable));
                r->appendChild(makeNumber("2"));
                r->appendChild(makeVariable('='));

                // Real part
                appendExactNodes(r, re);

                // - operator
                r->appendChild(makeOperator(OpKind::Sub));

                // Imaginary magnitude
                appendExactNodes(r, im);

                // Italic 'i'
                r->appendChild(makeConstant(ConstKind::Imag));

                _resultNode[1] = std::move(row);
                _resultRow[1]  = static_cast<NodeRow*>(_resultNode[1].get());

                lv_obj_set_pos(_resultCanvas[1].obj(), PAD + 20, 72);
                lv_obj_set_size(_resultCanvas[1].obj(), SCREEN_W - 2 * PAD - 20, 38);
                lv_obj_remove_flag(_resultCanvas[1].obj(), LV_OBJ_FLAG_HIDDEN);
                _resultCanvas[1].setExpression(_resultRow[1], nullptr);
                _resultRow[1]->calculateLayout(_resultCanvas[1].normalMetrics());
                _resultCanvas[1].invalidate();
                ++_resultCount;
            }

            goto addHint;
        }

        // Handle ok=true but no solutions (identity 0=0)
        if (_omniResult.solutions.empty()) {
            lv_label_set_text(_resultTitle, "Identity: infinite solutions");
            goto addHint;
        }

        {
            const char* clsName = cas::equationClassName(_omniResult.classification);
            bool isNumeric = (_omniResult.classification ==
                              cas::EquationClass::MixedTranscendental);

            if (isNumeric) {
                lv_label_set_text(_resultTitle, "Numeric root:");
            } else {
                char titleBuf[80];
                snprintf(titleBuf, sizeof(titleBuf), "Solution (%s):", clsName);
                lv_label_set_text(_resultTitle, titleBuf);
            }

            int numSols = static_cast<int>(_omniResult.solutions.size());
            if (numSols > MAX_RESULTS) numSols = MAX_RESULTS;

            for (int i = 0; i < numSols; ++i) {
                const auto& sol = _omniResult.solutions[i];
                auto row = makeRow();
                auto* r = static_cast<NodeRow*>(row.get());

                r->appendChild(makeVariable(_omniResult.variable));
                if (numSols > 1) {
                    r->appendChild(makeNumber(std::to_string(i + 1)));
                }
                r->appendChild(makeVariable('='));

                if (sol.symbolic) {
                    NodePtr valNode = cas::SymExprToAST::convert(sol.symbolic);
                    if (valNode->type() == NodeType::Row) {
                        auto* vr = static_cast<NodeRow*>(valNode.get());
                        while (vr->childCount() > 0) {
                            r->appendChild(vr->removeChild(0));
                        }
                    } else {
                        r->appendChild(std::move(valNode));
                    }
                } else if (sol.isExact) {
                    NodePtr valNode = cas::SymToAST::fromExactVal(sol.exact);
                    if (valNode->type() == NodeType::Row) {
                        auto* vr = static_cast<NodeRow*>(valNode.get());
                        while (vr->childCount() > 0) {
                            r->appendChild(vr->removeChild(0));
                        }
                    } else {
                        r->appendChild(std::move(valNode));
                    }
                } else {
                    char numBuf[32];
                    snprintf(numBuf, sizeof(numBuf), "%.10g", sol.numeric);
                    r->appendChild(makeNumber(numBuf));
                }

                _resultNode[i] = std::move(row);
                _resultRow[i]  = static_cast<NodeRow*>(_resultNode[i].get());

                int y = 28 + i * 44;
                lv_obj_set_pos(_resultCanvas[i].obj(), PAD + 20, y);
                lv_obj_set_size(_resultCanvas[i].obj(), SCREEN_W - 2 * PAD - 20, 38);
                lv_obj_remove_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);

                _resultCanvas[i].setExpression(_resultRow[i], nullptr);
                _resultRow[i]->calculateLayout(_resultCanvas[i].normalMetrics());
                _resultCanvas[i].invalidate();
                ++_resultCount;
            }
        }

    } else if (_isNLSolve) {
        // ── Nonlinear system result ────────────────────────────────
        if (!_nlResult.ok) {
            lv_label_set_text(_resultTitle,
                _nlResult.error.empty() ? "System has no solution"
                                        : _nlResult.error.c_str());
            goto addHint;
        }

        lv_label_set_text(_resultTitle, "System solutions:");

        int numSols = static_cast<int>(_nlResult.solutions.size());
        int varsPerSol = _nlResult.numVars;   // 2 or 3
        int maxSols = MAX_RESULTS / varsPerSol;
        if (numSols > maxSols) numSols = maxSols;

        for (int i = 0; i < numSols && _resultCount + (varsPerSol - 1) < MAX_RESULTS; ++i) {
            const auto& sol = _nlResult.solutions[i];

            // Helper: append expr or numeric fallback
            auto appendSolNode = [](NodeRow* r, cas::SymExpr* expr, double num) {
                if (expr) {
                    NodePtr valNode = cas::SymExprToAST::convert(expr);
                    if (valNode->type() == NodeType::Row) {
                        auto* vr = static_cast<NodeRow*>(valNode.get());
                        while (vr->childCount() > 0)
                            r->appendChild(vr->removeChild(0));
                    } else {
                        r->appendChild(std::move(valNode));
                    }
                } else {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.10g", num);
                    r->appendChild(makeNumber(buf));
                }
            };

            // Row for var1 (x)
            {
                auto row = makeRow();
                auto* r = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(_nlResult.var1));
                if (numSols > 1) r->appendChild(makeNumber(std::to_string(i + 1)));
                r->appendChild(makeVariable('='));
                appendSolNode(r, sol.exprX, sol.numX);

                int idx = _resultCount;
                _resultNode[idx] = std::move(row);
                _resultRow[idx] = static_cast<NodeRow*>(_resultNode[idx].get());

                int y = 28 + idx * 30;
                lv_obj_set_pos(_resultCanvas[idx].obj(), PAD + 20, y);
                lv_obj_set_size(_resultCanvas[idx].obj(), SCREEN_W - 2 * PAD - 20, 26);
                lv_obj_remove_flag(_resultCanvas[idx].obj(), LV_OBJ_FLAG_HIDDEN);
                _resultCanvas[idx].setExpression(_resultRow[idx], nullptr);
                _resultRow[idx]->calculateLayout(_resultCanvas[idx].normalMetrics());
                _resultCanvas[idx].invalidate();
                ++_resultCount;
            }

            // Row for var2 (y)
            {
                auto row = makeRow();
                auto* r = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(_nlResult.var2));
                if (numSols > 1) r->appendChild(makeNumber(std::to_string(i + 1)));
                r->appendChild(makeVariable('='));
                appendSolNode(r, sol.exprY, sol.numY);

                int idx = _resultCount;
                _resultNode[idx] = std::move(row);
                _resultRow[idx] = static_cast<NodeRow*>(_resultNode[idx].get());

                int y = 28 + idx * 30;
                lv_obj_set_pos(_resultCanvas[idx].obj(), PAD + 20, y);
                lv_obj_set_size(_resultCanvas[idx].obj(), SCREEN_W - 2 * PAD - 20, 26);
                lv_obj_remove_flag(_resultCanvas[idx].obj(), LV_OBJ_FLAG_HIDDEN);
                _resultCanvas[idx].setExpression(_resultRow[idx], nullptr);
                _resultRow[idx]->calculateLayout(_resultCanvas[idx].normalMetrics());
                _resultCanvas[idx].invalidate();
                ++_resultCount;
            }

            // Row for var3 (z) — only for 3×3 systems
            if (varsPerSol >= 3 && _resultCount < MAX_RESULTS) {
                auto row = makeRow();
                auto* r = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(_nlResult.var3));
                if (numSols > 1) r->appendChild(makeNumber(std::to_string(i + 1)));
                r->appendChild(makeVariable('='));
                appendSolNode(r, sol.exprZ, sol.numZ);

                int idx = _resultCount;
                _resultNode[idx] = std::move(row);
                _resultRow[idx] = static_cast<NodeRow*>(_resultNode[idx].get());

                int y = 28 + idx * 30;
                lv_obj_set_pos(_resultCanvas[idx].obj(), PAD + 20, y);
                lv_obj_set_size(_resultCanvas[idx].obj(), SCREEN_W - 2 * PAD - 20, 26);
                lv_obj_remove_flag(_resultCanvas[idx].obj(), LV_OBJ_FLAG_HIDDEN);
                _resultCanvas[idx].setExpression(_resultRow[idx], nullptr);
                _resultRow[idx]->calculateLayout(_resultCanvas[idx].normalMetrics());
                _resultCanvas[idx].invalidate();
                ++_resultCount;
            }
        }
    } else {
        // ── System result ──────────────────────────────────────────
        if (!_systemResult.ok) {
            lv_label_set_text(_resultTitle,
                _systemResult.error.empty() ? "Incompatible system"
                                            : _systemResult.error.c_str());
            goto addHint;
        }

        lv_label_set_text(_resultTitle, "System solution:");

        for (int i = 0; i < _systemResult.numVars && i < MAX_RESULTS; ++i) {
            auto row = makeRow();
            auto* r = static_cast<NodeRow*>(row.get());
            r->appendChild(makeVariable(_systemResult.vars[i]));
            r->appendChild(makeVariable('='));

            NodePtr valNode = cas::SymToAST::fromExactVal(
                _systemResult.solutions[i]);
            if (valNode->type() == NodeType::Row) {
                auto* vr = static_cast<NodeRow*>(valNode.get());
                while (vr->childCount() > 0) {
                    r->appendChild(vr->removeChild(0));
                }
            } else {
                r->appendChild(std::move(valNode));
            }

            _resultNode[i] = std::move(row);
            _resultRow[i]  = static_cast<NodeRow*>(_resultNode[i].get());

            int y = 28 + i * 44;
            lv_obj_set_pos(_resultCanvas[i].obj(), PAD + 20, y);
            lv_obj_set_size(_resultCanvas[i].obj(), SCREEN_W - 2 * PAD - 20, 38);
            lv_obj_remove_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);

            _resultCanvas[i].setExpression(_resultRow[i], nullptr);
            _resultRow[i]->calculateLayout(_resultCanvas[i].normalMetrics());
            _resultCanvas[i].invalidate();
            ++_resultCount;
        }
    }

addHint:
    if (!_resultHint || lv_obj_get_parent(_resultHint) != _resultContainer) {
        _resultHint = lv_label_create(_resultContainer);
        lv_obj_set_style_text_font(_resultHint, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(_resultHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    }
    lv_label_set_text(_resultHint, "STEPS: View steps    AC: Back");
    lv_obj_set_pos(_resultHint, PAD, SCREEN_H - BAR_H - 22);
}

// ════════════════════════════════════════════════════════════════════════════
// Build steps display
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildStepsDisplay() {
    // ── 1. Clean up previous content (persistent container) ─────────
    if (_stepsContainer == nullptr) {
        return;
    }
    lv_obj_clean(_stepsContainer);

    const cas::CASStepLogger& log = _isOmniSolve
        ? _omniResult.steps
        : (_isNLSolve ? _nlResult.steps : _systemResult.steps);

    const auto& steps = log.steps();

    if (steps.empty()) {
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        lv_label_set_text(lbl, "No steps available.");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    static constexpr int CANVAS_W = SCREEN_W - 2 * PAD - 16;

    // Helper: create a MathCanvas from a NodePtr, add to _stepRenderers
    auto emitCanvas = [&](vpam::NodePtr node) {
        if (!node) return;
        node = cas::SymExprToAST::ensureRow(std::move(node));
        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData = std::move(node);
        srd->canvas.create(_stepsContainer);

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = row->layout().width + 24;
        int16_t h = row->layout().ascent + row->layout().descent + 8;
        if (w > CANVAS_W) w = CANVAS_W;
        if (h < 22) h = 22;
        lv_obj_set_size(srd->canvas.obj(), w, h);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
    };

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

            lv_obj_t* descLbl = lv_label_create(_stepsContainer);
            lv_label_set_text(descLbl, buf);
            lv_obj_set_width(descLbl, SCREEN_W - 2 * PAD - 8);
            lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12,
                                       LV_PART_MAIN);
            lv_obj_set_style_text_color(descLbl, lv_color_hex(descColor),
                                        LV_PART_MAIN);
        }

        // ── 2b. MathCanvas for CAS mathExpr ────────────────────────
        // Any step with a SymExpr tree gets a mandatory MathCanvas
        if (step.mathExpr) {
            vpam::NodePtr astNode = cas::SymExprToAST::convert(step.mathExpr);
            emitCanvas(std::move(astNode));
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
                emitCanvas(std::move(snapNode));
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
                r->appendChild(makeVariable('='));
                appendExactVPAM(r, re);
                r->appendChild(makeOperator(OpKind::Add));
                appendExactVPAM(r, im);
                r->appendChild(makeConstant(ConstKind::Imag));
                emitCanvas(std::move(row));
            }

            // Root 2: x2 = re - im·i
            {
                auto row = makeRow();
                auto* r  = static_cast<NodeRow*>(row.get());
                r->appendChild(makeVariable(var));
                r->appendChild(makeNumber("2"));
                r->appendChild(makeVariable('='));
                appendExactVPAM(r, re);
                r->appendChild(makeOperator(OpKind::Sub));
                appendExactVPAM(r, im);
                r->appendChild(makeConstant(ConstKind::Imag));
                emitCanvas(std::move(row));
            }
        }
    }

    // ── 3. Footer hint ─────────────────────────────────────────────
    lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
    lv_label_set_text(hintLbl,
                      LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_12, LV_PART_MAIN);
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
    if (_stepsContainer == nullptr) {
        return;
    }
    lv_obj_clean(_stepsContainer);

    // #region agent log
    printf("[CAS UI] Rendering Steps. _stepsContainer ptr: %p\n", (void*)_stepsContainer);
    FILE* dbg = fopen("debug-bdf12a.log", "a");
    if (dbg) {
        unsigned long ptrVal = (unsigned long)_stepsContainer;
        unsigned long ts = (unsigned long)lv_tick_get();
        fprintf(
            dbg,
            "{\"id\":\"log_%lu\",\"timestamp\":%lu,"
            "\"location\":\"EquationsApp.cpp:buildCASStepsDisplay\","
            "\"message\":\"CAS steps render (delegated)\",\"data\":{\"stepsPtr\":%lu},"
            "\"runId\":\"pre-fix\",\"hypothesisId\":\"A\"}\n",
            ts, ts, ptrVal
        );
        fclose(dbg);
    }
    // #endregion

    // TEMPORAL HOTFIX: delegar en el visor clásico (Omni/System) para
    // evitar el crash en la ruta TRS avanzada. No tocamos más LVGL aquí.
    buildStepsDisplay();
    return;

    // ── 1. Serialise the equation NodeRow to a text string ─────────────
    std::string eqText;
    if (_eqRowData[0]) {
        nodeToText(_eqRowData[0], eqText);
    }

    // Verify it contains '='
    if (eqText.find('=') == std::string::npos) {
        buildStepsDisplay();   // fall back
        return;
    }

    // ── 2. Parse text → cas::EquationNode ──────────────────────────────
    _casPool.reset();
    _casEngine.reset();
    _casPool   = std::make_unique<cas::CasMemoryPool>();
    _casEngine = std::make_unique<cas::RuleEngine>(*_casPool);
    _hasCasResult = false;

    char var = 'x';
    cas::NodePtr parsedEq = casParseEquation(eqText, *_casPool, var);
    if (!parsedEq) {
        buildStepsDisplay();   // fall back
        return;
    }

    // ── 3. Load rules & solve ──────────────────────────────────────────
    for (auto& rule : cas::makeAlgebraicRules(*_casPool, var))
        _casEngine->addRule(std::move(rule));

    _casResult    = _casEngine->applyToFixedPoint(parsedEq, 80);
    _hasCasResult = true;
    cas::checkNonLinearHandover(_casResult, var);

    const auto& steps = _casResult.steps;

    static constexpr int16_t CANVAS_MAX_W =
        static_cast<int16_t>(SCREEN_W - 2 * PAD - 8);

    // Helper: emit a MathCanvas for a CAS tree with optional highlight
    auto emitCasCanvas = [&](const cas::NodePtr& tree,
                              const cas::NodePtr& highlight,
                              lv_color_t hlColor)
    {
        if (!tree) return;

        const vpam::MathNode* hlPtr = nullptr;
        vpam::NodePtr vpamTree;
        if (highlight) {
            vpamTree = cas::CasToVpam::convert(tree, highlight, &hlPtr);
        } else {
            vpamTree = cas::CasToVpam::convert(tree);
        }
        if (!vpamTree) return;

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

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = static_cast<int16_t>(row->layout().width + 24);
        int16_t h = static_cast<int16_t>(
            row->layout().ascent + row->layout().descent + 8);
        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
        if (h < 20) h = 20;
        lv_obj_set_size(srd->canvas.obj(), w, h);

        if (hlPtr) srd->canvas.setHighlightNode(hlPtr, hlColor);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
    };

    // ── 4a. Step 0 — Original Equation (always shown first) ─────────
    if (_stepsContainer != nullptr) {
        lv_obj_t* descLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(descLbl, "0. Original Equation");
        lv_obj_set_width(descLbl, CANVAS_MAX_W);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(COL_DESC_HEX), LV_PART_MAIN);
        emitCasCanvas(parsedEq, nullptr, lv_color_hex(0x4FC3F7));
    }

    if (steps.empty()) {
        if (_stepsContainer != nullptr) {
            lv_obj_t* lbl = lv_label_create(_stepsContainer);
            lv_label_set_text(lbl, "No further steps available.");
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        }
        return;
    }

    // ── 4b. Iterate steps ────────────────────────────────────
    for (std::size_t i = 0; i < steps.size(); ++i) {
        if (_stepsContainer == nullptr) return;

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

        lv_obj_t* descLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(descLbl, buf);
        lv_obj_set_width(descLbl, CANVAS_MAX_W);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(descColHex),
                                    LV_PART_MAIN);

        // MathCanvas for this step's equation tree
        if (step.tree) {
            emitCasCanvas(step.tree, step.affectedNode, hlColor);
        }
    }

    // ── 5. Footer hint ───────────────────────────────────────
    if (_stepsContainer != nullptr) {
        lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(hintLbl, LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
        lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX),
                                    LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// buildSystemCASStepsDisplay  —  SystemTutor step view for 2x2 systems
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildSystemCASStepsDisplay() {
    // ── 1. Clean up previous content (persistent container) ─────────
    if (_stepsContainer == nullptr) {
        return;
    }
    lv_obj_clean(_stepsContainer);

    // Ruta avanzada (SystemTutor + MathCanvas) está causando StoreProhibited
    // al crear objetos LVGL en esp32s3. Como hotfix estable, delegamos
    // el renderizado de pasos de sistemas 2x2 al visor clásico basado en
    // Omni/SystemResult, que ya hemos comprobado que es estable.
    buildStepsDisplay();
}
