/**
 * TutorApp.cpp — Step-by-step algebraic equation tutor (Phase 13C).
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13C (TRS→Display Bridge)
 */

#include "TutorApp.h"
#include "../math/cas/AlgebraicRules.h"
#include "../math/cas/CasToVpam.h"
#include "../math/cas/PersistentAST.h"
#include "../math/MathAST.h"

#include <cstring>
#include <cmath>
#include <cctype>

// ─────────────────────────────────────────────────────────────────────────────
// §1  Simple Equation Parser
//
// Supports:  digits  |  '.'  |  single-letter var  |  +  -  *  ^  ()  =
// Precedence:  ^ (right) > unary -/+ > * > + -
// Equations:  <expr>  '='  <expr>
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct ParseCtx {
    const char* src;
    std::size_t pos;
    cas::CasMemoryPool& pool;
    char var;

    char peek() const { return src[pos]; }
    char eat()  { return src[pos++]; }
    void skipWS() { while (src[pos] == ' ') ++pos; }
};

static cas::NodePtr parseExpr(ParseCtx& ctx);
static cas::NodePtr parseTerm(ParseCtx& ctx);
static cas::NodePtr parsePower(ParseCtx& ctx);
static cas::NodePtr parseUnary(ParseCtx& ctx);
static cas::NodePtr parseAtom(ParseCtx& ctx);

/// Parse an additive expression: term (('+' | '-') term)*
static cas::NodePtr parseExpr(ParseCtx& ctx) {
    ctx.skipWS();
    cas::NodePtr lhs = parseTerm(ctx);

    while (true) {
        ctx.skipWS();
        char c = ctx.peek();
        if (c != '+' && c != '-') break;
        ctx.eat();
        ctx.skipWS();
        cas::NodePtr rhs = parseTerm(ctx);
        if (c == '-') rhs = cas::makeNegation(ctx.pool, std::move(rhs));
        // Flatten into a sum
        std::vector<cas::NodePtr> kids;
        if (lhs && lhs->isSum()) {
            const auto* s = static_cast<const cas::SumNode*>(lhs.get());
            for (const auto& ch : s->children) kids.push_back(ch);
        } else {
            kids.push_back(std::move(lhs));
        }
        kids.push_back(std::move(rhs));
        lhs = cas::makeSum(ctx.pool, std::move(kids));
    }
    return lhs;
}

/// Parse a multiplicative expression: power ('*' power)*  or implicit coeff·var
static cas::NodePtr parseTerm(ParseCtx& ctx) {
    ctx.skipWS();
    cas::NodePtr lhs = parsePower(ctx);

    while (true) {
        ctx.skipWS();
        char c = ctx.peek();

        // Explicit *
        if (c == '*') {
            ctx.eat();
            ctx.skipWS();
            cas::NodePtr rhs = parsePower(ctx);
            std::vector<cas::NodePtr> kids;
            if (lhs && lhs->isProduct()) {
                const auto* p = static_cast<const cas::ProductNode*>(lhs.get());
                for (const auto& ch : p->children) kids.push_back(ch);
            } else {
                kids.push_back(std::move(lhs));
            }
            kids.push_back(std::move(rhs));
            lhs = cas::makeProduct(ctx.pool, std::move(kids));
            continue;
        }

        // Implicit multiplication: digit/literal followed directly by variable/paren
        // e.g. "2x", "3(x+1)"
        if (lhs) {
            bool lhsConst = lhs->isConstant();
            bool rhsVar   = std::isalpha(c) && c != 'e' && c != 'E';
            bool rhsParen = (c == '(');
            if (lhsConst && (rhsVar || rhsParen)) {
                cas::NodePtr rhs = parsePower(ctx);
                std::vector<cas::NodePtr> kids;
                if (lhs->isProduct()) {
                    const auto* p = static_cast<const cas::ProductNode*>(lhs.get());
                    for (const auto& ch : p->children) kids.push_back(ch);
                } else {
                    kids.push_back(std::move(lhs));
                }
                kids.push_back(std::move(rhs));
                lhs = cas::makeProduct(ctx.pool, std::move(kids));
                continue;
            }
        }

        break;
    }
    return lhs;
}

/// Parse a power expression: unary ('^' unary)?   (right-associative)
static cas::NodePtr parsePower(ParseCtx& ctx) {
    ctx.skipWS();
    cas::NodePtr base = parseUnary(ctx);
    ctx.skipWS();
    if (ctx.peek() == '^') {
        ctx.eat();
        ctx.skipWS();
        cas::NodePtr exp = parseUnary(ctx);  // right-associative
        return cas::makePower(ctx.pool, std::move(base), std::move(exp));
    }
    return base;
}

/// Parse a unary negation or positive: ('-' | '+')? atom
static cas::NodePtr parseUnary(ParseCtx& ctx) {
    ctx.skipWS();
    if (ctx.peek() == '-') {
        ctx.eat();
        ctx.skipWS();
        cas::NodePtr inner = parseAtom(ctx);
        return cas::makeNegation(ctx.pool, std::move(inner));
    }
    if (ctx.peek() == '+') {
        ctx.eat();
        ctx.skipWS();
    }
    return parseAtom(ctx);
}

/// Parse an atom: number | variable | '(' expr ')'
static cas::NodePtr parseAtom(ParseCtx& ctx) {
    ctx.skipWS();
    char c = ctx.peek();

    // Parenthesised sub-expression
    if (c == '(') {
        ctx.eat();
        cas::NodePtr inner = parseExpr(ctx);
        ctx.skipWS();
        if (ctx.peek() == ')') ctx.eat();
        return inner;
    }

    // Number literal
    if (std::isdigit(c) || c == '.') {
        std::string buf;
        while (std::isdigit(ctx.peek()) || ctx.peek() == '.') buf += ctx.eat();
        double val = std::stod(buf);
        return cas::makeConstant(ctx.pool, val);
    }

    // Single-char variable
    if (std::isalpha(c)) {
        ctx.eat();
        return cas::makeVariable(ctx.pool, c);
    }

    // Fallback: zero constant for empty/malformed input
    return cas::makeConstant(ctx.pool, 0.0);
}

/// Parse "lhsExpr = rhsExpr" and produce an EquationNode.
/// Returns nullptr on parse failure.
static cas::NodePtr parseEquationString(const std::string& text,
                                        cas::CasMemoryPool& pool,
                                        char& varOut) {
    // Find the '=' separator
    auto eqPos = text.find('=');
    if (eqPos == std::string::npos) return nullptr;

    std::string lhsStr = text.substr(0, eqPos);
    std::string rhsStr = text.substr(eqPos + 1);

    // Detect the variable (first alpha char found)
    varOut = 'x';
    for (char ch : text) {
        if (std::isalpha(ch)) { varOut = ch; break; }
    }

    ParseCtx lhsCtx{lhsStr.c_str(), 0, pool, varOut};
    ParseCtx rhsCtx{rhsStr.c_str(), 0, pool, varOut};

    cas::NodePtr lhs = parseExpr(lhsCtx);
    cas::NodePtr rhs = parseExpr(rhsCtx);
    if (!lhs || !rhs) return nullptr;

    return cas::makeEquation(pool, std::move(lhs), std::move(rhs));
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// §2  TutorApp implementation
// ─────────────────────────────────────────────────────────────────────────────

TutorApp::TutorApp() = default;
TutorApp::~TutorApp() { end(); }

void TutorApp::begin() {
    if (_screen) return;
    buildUI();
}

void TutorApp::end() {
    clearSteps();
    _pool.reset();
    _engine.reset();
    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
    }
    _titleLabel     = nullptr;
    _inputField     = nullptr;
    _solveBtn       = nullptr;
    _statusLabel    = nullptr;
    _stepsContainer = nullptr;
}

void TutorApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// §3  UI construction
// ─────────────────────────────────────────────────────────────────────────────

void TutorApp::buildUI() {
    // ── Screen ──────────────────────────────────────────────────────────────
    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(_screen, LV_SCROLLBAR_MODE_OFF);

    // ── Title bar ───────────────────────────────────────────────────────────
    lv_obj_t* bar = lv_obj_create(_screen);
    lv_obj_set_size(bar, SCREEN_W, BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COL_BAR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    _titleLabel = lv_label_create(bar);
    lv_label_set_text(_titleLabel, "Equation Tutor");
    lv_obj_set_style_text_font(_titleLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_titleLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(_titleLabel, LV_ALIGN_LEFT_MID, PAD, 0);

    // ── Input field ─────────────────────────────────────────────────────────
    _inputField = lv_textarea_create(_screen);
    lv_obj_set_size(_inputField, SCREEN_W - 2 * PAD, INPUT_H);
    lv_obj_set_pos(_inputField, PAD, BAR_H + 2);
    lv_textarea_set_one_line(_inputField, true);
    lv_textarea_set_max_length(_inputField, MAX_INPUT_LEN);
    lv_textarea_set_placeholder_text(_inputField, "e.g. 2x + 6 = 0");
    lv_obj_set_style_bg_color(_inputField, lv_color_hex(COL_INPUT_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(_inputField, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(_inputField, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_border_color(_inputField, lv_color_hex(0x4A90D9), LV_PART_MAIN);
    lv_obj_set_style_border_width(_inputField, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_inputField, 4, LV_PART_MAIN);

    // ── Solve button ────────────────────────────────────────────────────────
    _solveBtn = lv_btn_create(_screen);
    lv_obj_set_size(_solveBtn, SCREEN_W - 2 * PAD, BTN_H);
    lv_obj_set_pos(_solveBtn, PAD, BAR_H + INPUT_H + 4);
    lv_obj_set_style_bg_color(_solveBtn, lv_color_hex(COL_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_solveBtn, lv_color_hex(0x0D47A1), LV_STATE_PRESSED);
    lv_obj_set_style_radius(_solveBtn, 4, LV_PART_MAIN);

    lv_obj_t* btnLbl = lv_label_create(_solveBtn);
    lv_label_set_text(btnLbl, "Solve  (ENTER)");
    lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(btnLbl, lv_color_hex(COL_BTN_TXT), LV_PART_MAIN);
    lv_obj_align(btnLbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(_solveBtn, solveBtnCb, LV_EVENT_CLICKED, this);

    // ── Status label (below button) ─────────────────────────────────────────
    _statusLabel = lv_label_create(_screen);
    lv_obj_set_pos(_statusLabel, PAD, BAR_H + INPUT_H + BTN_H + 6);
    lv_obj_set_width(_statusLabel, SCREEN_W - 2 * PAD);
    lv_label_set_long_mode(_statusLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(_statusLabel, "Type an equation above and press Solve.");
    lv_obj_set_style_text_font(_statusLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(0x888888), LV_PART_MAIN);

    // ── Scrollable steps container ──────────────────────────────────────────
    _stepsContainer = lv_obj_create(_screen);
    lv_obj_set_size(_stepsContainer, SCREEN_W, STEPS_H);
    lv_obj_set_pos(_stepsContainer, 0, STEPS_Y + 14);  // below status label
    lv_obj_set_style_bg_color(_stepsContainer, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_stepsContainer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_stepsContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_stepsContainer, PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_row(_stepsContainer, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(_stepsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(_stepsContainer, LV_SCROLLBAR_MODE_AUTO);
}

// ─────────────────────────────────────────────────────────────────────────────
// §4  Helper — clear step list
// ─────────────────────────────────────────────────────────────────────────────

void TutorApp::clearSteps() {
    _stepRenderers.clear();
    if (_stepsContainer) lv_obj_clean(_stepsContainer);
}

void TutorApp::showStatus(const char* msg, lv_color_t color) {
    if (!_statusLabel) return;
    lv_label_set_text(_statusLabel, msg);
    lv_obj_set_style_text_color(_statusLabel, color, LV_PART_MAIN);
}

// ─────────────────────────────────────────────────────────────────────────────
// §5  onSolveClicked — the main integration point
// ─────────────────────────────────────────────────────────────────────────────

void TutorApp::onSolveClicked() {
    if (!_inputField) return;

    clearSteps();
    showStatus("", lv_color_hex(0x888888));

    // ── 1. Get equation text ─────────────────────────────────────────────────
    const char* raw = lv_textarea_get_text(_inputField);
    if (!raw || raw[0] == '\0') {
        showStatus("Enter an equation first.", lv_color_hex(0xFF4444));
        return;
    }
    std::string text(raw);

    // ── 2. Initialise CAS memory pool & engine ───────────────────────────────
    _pool.reset();
    _engine.reset();
    _pool   = std::make_unique<cas::CasMemoryPool>();
    _engine = std::make_unique<cas::RuleEngine>(*_pool);

    char var = 'x';
    cas::NodePtr parsedEq = parseEquationString(text, *_pool, var);
    if (!parsedEq) {
        showStatus("Parse error — use e.g. \"2x + 6 = 0\"", lv_color_hex(0xFF4444));
        return;
    }

    // ── 3. Load algebraic rules & solve ─────────────────────────────────────
    for (auto& rule : cas::makeAlgebraicRules(*_pool, var))
        _engine->addRule(std::move(rule));

    cas::RuleEngine::SolveResult result =
        _engine->applyToFixedPoint(parsedEq, 80);

    // ── 4. Quadratic handover check ──────────────────────────────────────────
    cas::checkNonLinearHandover(result, var);

    // ── 5. Update status label ───────────────────────────────────────────────
    char statusBuf[80];
    snprintf(statusBuf, sizeof(statusBuf),
             "%d step%s", (int)result.steps.size(),
             result.steps.size() == 1 ? "" : "s");
    showStatus(statusBuf, lv_color_hex(0x90CAF9));

    // ── 6. Render steps in the flex column ───────────────────────────────────
    // Canvas available width (padding on each side)
    const int16_t CANVAS_MAX_W = static_cast<int16_t>(SCREEN_W - 2 * PAD - 8);

    for (std::size_t i = 0; i < result.steps.size(); ++i) {
        const auto& step = result.steps[i];

        // Choose colours: last step or NonLinearHandover → orange, else blue
        bool isFinal = (i == result.steps.size() - 1) ||
                       (step.ruleName == cas::RULE_NONLINEAR_HANDOVER);
        uint32_t descColHex = isFinal ? COL_DESC_FINAL : COL_DESC;
        lv_color_t hlColor  = isFinal ? lv_color_hex(COL_HL_FINAL)
                                      : lv_color_hex(COL_HL_INTER);

        // ── Description label ──────────────────────────────────────────────
        char buf[120];
        if (!step.ruleDesc.empty()) {
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.ruleDesc.c_str());
        } else {
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.ruleName.c_str());
        }
        lv_obj_t* descLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(descLbl, buf);
        lv_obj_set_width(descLbl, CANVAS_MAX_W);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(descColHex), LV_PART_MAIN);

        // ── MathCanvas for this step's equation ───────────────────────────
        if (!step.tree) continue;

        // Convert the CAS tree → VPAM, tracking the affectedNode for highlight
        const vpam::MathNode* highlightPtr = nullptr;
        vpam::NodePtr vpamTree;

        if (step.affectedNode) {
            vpamTree = cas::CasToVpam::convert(step.tree,
                                               step.affectedNode,
                                               &highlightPtr);
        } else {
            vpamTree = cas::CasToVpam::convert(step.tree);
        }

        // Wrap in a NodeRow if needed (required by MathCanvas::setExpression)
        if (!vpamTree) continue;
        if (vpamTree->type() != vpam::NodeType::Row) {
            auto rowWrap = vpam::makeRow();
            static_cast<vpam::NodeRow*>(rowWrap.get())
                ->appendChild(std::move(vpamTree));
            vpamTree = std::move(rowWrap);
        }

        auto srd = std::make_unique<StepRenderData>();
        srd->nodeData      = std::move(vpamTree);
        srd->highlightNode = highlightPtr;
        srd->highlightColor= hlColor;

        srd->canvas.create(_stepsContainer);

        auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
        srd->canvas.setExpression(row, nullptr);
        row->calculateLayout(srd->canvas.normalMetrics());

        int16_t w = static_cast<int16_t>(row->layout().width + 24);
        int16_t h = static_cast<int16_t>(row->layout().ascent + row->layout().descent + 8);
        if (w > CANVAS_MAX_W) w = CANVAS_MAX_W;
        if (h < 20) h = 20;
        lv_obj_set_size(srd->canvas.obj(), w, h);

        if (highlightPtr) {
            srd->canvas.setHighlightNode(highlightPtr, hlColor);
        }
        srd->canvas.invalidate();

        _stepRenderers.push_back(std::move(srd));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §6  Event callbacks & key handling
// ─────────────────────────────────────────────────────────────────────────────

void TutorApp::solveBtnCb(lv_event_t* e) {
    auto* self = static_cast<TutorApp*>(lv_event_get_user_data(e));
    if (self) self->onSolveClicked();
}

void TutorApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (ev.code) {
        case KeyCode::ENTER:
        case KeyCode::F5:
            onSolveClicked();
            break;

        case KeyCode::AC:
            // Clear input and steps
            if (_inputField) lv_textarea_set_text(_inputField, "");
            clearSteps();
            showStatus("Type an equation above and press Solve.",
                       lv_color_hex(0x888888));
            break;

        case KeyCode::DEL:
            if (_inputField) lv_textarea_delete_char(_inputField);
            break;

        case KeyCode::UP:
            if (_stepsContainer) lv_obj_scroll_by(_stepsContainer, 0, -20, LV_ANIM_OFF);
            break;

        case KeyCode::DOWN:
            if (_stepsContainer) lv_obj_scroll_by(_stepsContainer, 0,  20, LV_ANIM_OFF);
            break;

        // ── Digit keys ─────────────────────────────────────────────────────
        case KeyCode::NUM_0: if (_inputField) lv_textarea_add_char(_inputField, '0'); break;
        case KeyCode::NUM_1: if (_inputField) lv_textarea_add_char(_inputField, '1'); break;
        case KeyCode::NUM_2: if (_inputField) lv_textarea_add_char(_inputField, '2'); break;
        case KeyCode::NUM_3: if (_inputField) lv_textarea_add_char(_inputField, '3'); break;
        case KeyCode::NUM_4: if (_inputField) lv_textarea_add_char(_inputField, '4'); break;
        case KeyCode::NUM_5: if (_inputField) lv_textarea_add_char(_inputField, '5'); break;
        case KeyCode::NUM_6: if (_inputField) lv_textarea_add_char(_inputField, '6'); break;
        case KeyCode::NUM_7: if (_inputField) lv_textarea_add_char(_inputField, '7'); break;
        case KeyCode::NUM_8: if (_inputField) lv_textarea_add_char(_inputField, '8'); break;
        case KeyCode::NUM_9: if (_inputField) lv_textarea_add_char(_inputField, '9'); break;

        // ── Operator / symbol keys ──────────────────────────────────────────
        case KeyCode::DOT:     if (_inputField) lv_textarea_add_char(_inputField, '.'); break;
        case KeyCode::ADD:     if (_inputField) lv_textarea_add_char(_inputField, '+'); break;
        case KeyCode::SUB:
        case KeyCode::NEG:     if (_inputField) lv_textarea_add_char(_inputField, '-'); break;
        case KeyCode::MUL:     if (_inputField) lv_textarea_add_char(_inputField, '*'); break;
        case KeyCode::POW:     if (_inputField) lv_textarea_add_char(_inputField, '^'); break;
        case KeyCode::LPAREN:  if (_inputField) lv_textarea_add_char(_inputField, '('); break;
        case KeyCode::RPAREN:  if (_inputField) lv_textarea_add_char(_inputField, ')'); break;
        case KeyCode::FREE_EQ: if (_inputField) lv_textarea_add_char(_inputField, '='); break;

        // ── Variable keys ───────────────────────────────────────────────────
        case KeyCode::VAR_X:   if (_inputField) lv_textarea_add_char(_inputField, 'x'); break;
        case KeyCode::VAR_Y:   if (_inputField) lv_textarea_add_char(_inputField, 'y'); break;
        case KeyCode::ALPHA_A: if (_inputField) lv_textarea_add_char(_inputField, 'a'); break;
        case KeyCode::ALPHA_B: if (_inputField) lv_textarea_add_char(_inputField, 'b'); break;
        case KeyCode::ALPHA_C: if (_inputField) lv_textarea_add_char(_inputField, 'c'); break;
        case KeyCode::ALPHA_D: if (_inputField) lv_textarea_add_char(_inputField, 'd'); break;
        case KeyCode::ALPHA_E: if (_inputField) lv_textarea_add_char(_inputField, 'e'); break;
        case KeyCode::ALPHA_F: if (_inputField) lv_textarea_add_char(_inputField, 'f'); break;

        default: break;
    }
}
