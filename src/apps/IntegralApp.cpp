/**
 * IntegralApp.cpp — Symbolic Integration App for NumOS.
 *
 * Phase 6B: Integration App & Final Polish.
 *
 * Pipeline:
 *   MathAST → ASTFlattener → SymExpr → SymIntegrate::integrate() →
 *   SymSimplify::simplify() → SymExprToAST::convertIntegral() → MathCanvas
 *
 * Part of: NumOS Pro-CAS — Phase 6B (Integration App & Final Polish)
 */

#include "IntegralApp.h"
#include "../math/MathAST.h"
#include "../math/cas/SymToAST.h"
#include <cstdlib>

using namespace vpam;

// ════════════════════════════════════════════════════════════════════════════
// Layout constants (320×240 display)
// ════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t COL_BG_HEX      = 0xFFFFFF;
static constexpr uint32_t COL_SEP_HEX     = 0x333333;
static constexpr uint32_t COL_ACCENT_HEX  = 0x6A1B9A;  // Purple accent for integration
static constexpr uint32_t COL_HINT_HEX    = 0x888888;
static constexpr uint32_t COL_ACTIVE_HEX  = 0x6A1B9A;
static constexpr uint32_t COL_STEP_HEX    = 0x1A1A1A;
static constexpr uint32_t COL_DESC_HEX    = 0x2E7D32;
static constexpr uint32_t COL_INTEG_HEX   = 0x6A1B9A;  // Purple for integral label

static constexpr int SCREEN_W  = 320;
static constexpr int SCREEN_H  = 240;
static constexpr int BAR_H     = ui::StatusBar::HEIGHT + 1;
static constexpr int PAD       = 6;

// ════════════════════════════════════════════════════════════════════════════
// Computing messages
// ════════════════════════════════════════════════════════════════════════════

static const char* COMPUTING_MESSAGES[] = {
    "Integrating...",
    "Finding antiderivative...",
    "Trying substitution...",
    "Integration by parts...",
    "Riemann would be proud...",
    "Computing integral...",
};
static constexpr int NUM_COMPUTING_MSGS = sizeof(COMPUTING_MESSAGES) / sizeof(COMPUTING_MESSAGES[0]);

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

IntegralApp::IntegralApp()
    : _scr(nullptr)
    , _inputContainer(nullptr)
    , _inputTitle(nullptr)
    , _inputHint(nullptr)
    , _computingContainer(nullptr)
    , _computingSpinner(nullptr)
    , _computingLabel(nullptr)
    , _resultContainer(nullptr)
    , _resultTitle(nullptr)
    , _resultLabel(nullptr)
    , _resultHint(nullptr)
    , _originalLabel(nullptr)
    , _stepsContainer(nullptr)
    , _state(State::EDITING)
    , _stepScroll(0)
    , _variable('x')
    , _inputRow(nullptr)
    , _resultRow(nullptr)
    , _originalRow(nullptr)
    , _integralExpr(nullptr)
    , _integralFound(false)
{
}

IntegralApp::~IntegralApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::begin() {
    if (_scr) return;
    createUI();
    _state = State::EDITING;
    showInput();
}

void IntegralApp::end() {
    _inputCanvas.stopCursorBlink();
    _inputCanvas.destroy();
    _inputNode.reset();
    _inputRow = nullptr;

    _resultCanvas.destroy();
    _resultNode.reset();
    _resultRow = nullptr;

    _originalCanvas.destroy();
    _originalNode.reset();
    _originalRow = nullptr;

    _statusBar.destroy();

    if (_scr) {
        lv_obj_delete(_scr);
        _scr               = nullptr;
        _inputContainer    = nullptr;
        _inputTitle        = nullptr;
        _inputHint         = nullptr;
        _computingContainer = nullptr;
        _computingSpinner  = nullptr;
        _computingLabel    = nullptr;
        _resultContainer   = nullptr;
        _resultTitle       = nullptr;
        _resultLabel       = nullptr;
        _resultHint        = nullptr;
        _originalLabel     = nullptr;
        _stepsContainer    = nullptr;
    }

    _state = State::EDITING;
    _integralExpr = nullptr;
    _integralFound = false;
    _integSteps.clear();
    _arena.reset();
}

void IntegralApp::load() {
    if (!_scr) begin();
    lv_screen_load_anim(_scr, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();

    if (_state == State::EDITING) {
        _inputCanvas.startCursorBlink();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// UI Creation
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::createUI() {
    // ── Screen ──
    _scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── StatusBar ──
    _statusBar.create(_scr);
    _statusBar.setTitle("Calculus");
    _statusBar.setBatteryLevel(100);

    // ─────────────────────────────────────────────────────────────────
    // INPUT container
    // ─────────────────────────────────────────────────────────────────
    _inputContainer = lv_obj_create(_scr);
    lv_obj_set_size(_inputContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_inputContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_inputContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_inputContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_inputContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_inputContainer, LV_OBJ_FLAG_SCROLLABLE);

    _inputTitle = lv_label_create(_inputContainer);
    lv_label_set_text(_inputTitle, "Introduce f(x):");
    lv_obj_set_style_text_font(_inputTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_inputTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_inputTitle, PAD, 4);

    _inputCanvas.create(_inputContainer);
    lv_obj_set_pos(_inputCanvas.obj(), PAD + 4, 24);
    lv_obj_set_size(_inputCanvas.obj(), SCREEN_W - 2 * PAD - 4, 130);

    _inputHint = lv_label_create(_inputContainer);
    lv_label_set_text(_inputHint, "EXE: Integrar   AC: Volver");
    lv_obj_set_style_text_font(_inputHint, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_inputHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_inputHint, PAD, SCREEN_H - BAR_H - 18);

    // ─────────────────────────────────────────────────────────────────
    // COMPUTING container (spinner + message)
    // ─────────────────────────────────────────────────────────────────
    _computingContainer = lv_obj_create(_scr);
    lv_obj_set_size(_computingContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_computingContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_computingContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_computingContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_computingContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_computingContainer, LV_OBJ_FLAG_SCROLLABLE);

    _computingLabel = lv_label_create(_computingContainer);
    lv_label_set_text(_computingLabel, "Integrando...");
    lv_obj_set_style_text_font(_computingLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_computingLabel, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_align(_computingLabel, LV_ALIGN_CENTER, 0, -20);

    _computingSpinner = lv_spinner_create(_computingContainer);
    lv_spinner_set_anim_params(_computingSpinner, 1000, 200);
    lv_obj_set_size(_computingSpinner, 40, 40);
    lv_obj_align(_computingSpinner, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_arc_color(_computingSpinner,
                               lv_color_hex(COL_ACCENT_HEX), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_computingSpinner,
                               lv_color_hex(0xDDDDDD), LV_PART_MAIN);

    // ─────────────────────────────────────────────────────────────────
    // RESULT container
    // ─────────────────────────────────────────────────────────────────
    _resultContainer = lv_obj_create(_scr);
    lv_obj_set_size(_resultContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_resultContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_resultContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_resultContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_SCROLLABLE);

    _resultTitle = lv_label_create(_resultContainer);
    lv_label_set_text(_resultTitle, "Integral");
    lv_obj_set_style_text_font(_resultTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_resultTitle, PAD, 4);

    // Original expression label
    _originalLabel = lv_label_create(_resultContainer);
    lv_label_set_text(_originalLabel, "f(x) =");
    lv_obj_set_style_text_font(_originalLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_originalLabel, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_originalLabel, PAD, 24);

    // Original expression canvas (small)
    _originalCanvas.create(_resultContainer);
    lv_obj_set_pos(_originalCanvas.obj(), PAD + 50, 20);
    lv_obj_set_size(_originalCanvas.obj(), SCREEN_W - PAD - 54, 30);
    lv_obj_add_flag(_originalCanvas.obj(), LV_OBJ_FLAG_HIDDEN);

    // Integral label
    _resultLabel = lv_label_create(_resultContainer);
    lv_label_set_text(_resultLabel, LV_SYMBOL_REFRESH " F(x) =");
    lv_obj_set_style_text_font(_resultLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultLabel, lv_color_hex(COL_INTEG_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_resultLabel, PAD, 58);

    // Result canvas
    _resultCanvas.create(_resultContainer);
    lv_obj_set_pos(_resultCanvas.obj(), PAD + 4, 78);
    lv_obj_set_size(_resultCanvas.obj(), SCREEN_W - 2 * PAD - 4, 80);
    lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);

    // Result hint
    _resultHint = lv_label_create(_resultContainer);
    lv_label_set_text(_resultHint, "STEPS: Ver pasos    AC: Nuevo");
    lv_obj_set_style_text_font(_resultHint, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_resultHint, PAD, SCREEN_H - BAR_H - 22);

    // ─────────────────────────────────────────────────────────────────
    // STEPS container (scrollable)
    // ─────────────────────────────────────────────────────────────────
    _stepsContainer = lv_obj_create(_scr);
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

void IntegralApp::hideAllContainers() {
    if (_inputContainer)    lv_obj_add_flag(_inputContainer,    LV_OBJ_FLAG_HIDDEN);
    if (_computingContainer) lv_obj_add_flag(_computingContainer, LV_OBJ_FLAG_HIDDEN);
    if (_resultContainer)   lv_obj_add_flag(_resultContainer,   LV_OBJ_FLAG_HIDDEN);
    if (_stepsContainer)    lv_obj_add_flag(_stepsContainer,    LV_OBJ_FLAG_HIDDEN);

    _inputCanvas.stopCursorBlink();
}

// ════════════════════════════════════════════════════════════════════════════
// State transitions
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::showInput() {
    hideAllContainers();
    _state = State::EDITING;
    _statusBar.setTitle("Calculus");

    if (!_inputNode) resetInput();
    _inputCanvas.setExpression(_inputRow, &_inputCursor);
    _inputCanvas.invalidate();

    lv_obj_remove_flag(_inputContainer, LV_OBJ_FLAG_HIDDEN);
    _inputCanvas.startCursorBlink();
    lv_obj_invalidate(_scr);
}

void IntegralApp::showComputing() {
    hideAllContainers();
    _state = State::COMPUTING;
    _statusBar.setTitle("Computing...");

    int idx = rand() % NUM_COMPUTING_MSGS;
    lv_label_set_text(_computingLabel, COMPUTING_MESSAGES[idx]);
    lv_obj_remove_flag(_computingContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_scr);

    // Force LVGL to render the spinner before blocking compute
    lv_timer_handler();
}

void IntegralApp::showResult() {
    hideAllContainers();
    _state = State::RESULT;
    _statusBar.setTitle("Antiderivative");

    buildResultDisplay();

    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_scr);
}

void IntegralApp::showSteps() {
    hideAllContainers();
    _state = State::STEPS;
    _statusBar.setTitle("Steps");
    _stepScroll = 0;

    buildStepsDisplay();

    lv_obj_remove_flag(_stepsContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
    lv_obj_invalidate(_scr);
}

// ════════════════════════════════════════════════════════════════════════════
// Key Dispatch
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_state) {
        case State::EDITING:  handleKeyInput(ev);   break;
        case State::COMPUTING: /* ignore keys during compute */ break;
        case State::RESULT:   handleKeyResult(ev);  break;
        case State::STEPS:    handleKeySteps(ev);   break;
    }
}

// ────────────────────────────────────────────────────────────────────
// INPUT state keys
// ────────────────────────────────────────────────────────────────────

void IntegralApp::handleKeyInput(const KeyEvent& ev) {
    auto& km = KeyboardManager::instance();
    if (ev.code == KeyCode::SHIFT) { km.pressShift(); _statusBar.update(); return; }
    if (ev.code == KeyCode::ALPHA) { km.pressAlpha(); _statusBar.update(); return; }

    auto& cc = _inputCursor;
    bool changed = false;

    switch (ev.code) {
        // ── Navigation ──
        case KeyCode::LEFT:  cc.moveLeft();  changed = true; break;
        case KeyCode::RIGHT: cc.moveRight(); changed = true; break;

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

        // ── AC → reset input ──
        case KeyCode::AC:
            _inputNode.reset();
            _inputRow = nullptr;
            showInput();
            break;

        // ── ENTER → integrate ──
        case KeyCode::ENTER:
            computeIntegral();
            break;

        default:
            break;
    }

    if (km.isShift()) km.consumeModifier();

    if (changed) {
        adjustInputHeight();
        refreshInput();
    }
}

// ────────────────────────────────────────────────────────────────────
// RESULT state keys
// ────────────────────────────────────────────────────────────────────

void IntegralApp::handleKeyResult(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::SHOW_STEPS:
            showSteps();
            break;
        case KeyCode::AC:
            showInput();
            break;
        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────────
// STEPS state keys
// ────────────────────────────────────────────────────────────────────

void IntegralApp::handleKeySteps(const KeyEvent& ev) {
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
// Input management
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::resetInput() {
    _inputNode = makeRow();
    _inputRow  = static_cast<NodeRow*>(_inputNode.get());
    _inputCursor.init(_inputRow);
}

void IntegralApp::refreshInput() {
    _inputCanvas.setExpression(_inputRow, &_inputCursor);
    _inputCanvas.invalidate();
    _inputCanvas.resetCursorBlink();
}

void IntegralApp::adjustInputHeight() {
    if (!_inputRow) return;

    _inputRow->calculateLayout(_inputCanvas.normalMetrics());
    int contentH = _inputRow->layout().ascent + _inputRow->layout().descent;

    int newH = contentH + 16;
    if (newH < 50) newH = 50;
    if (newH > 140) newH = 140;

    int curH = lv_obj_get_height(_inputCanvas.obj());
    if (curH > 0 && (newH - curH > 2 || curH - newH > 2)) {
        lv_obj_set_height(_inputCanvas.obj(), newH);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Detect variable in expression
// ════════════════════════════════════════════════════════════════════════════

char IntegralApp::detectVariable(const cas::SymExpr* expr) {
    if (!expr) return 'x';

    switch (expr->type) {
        case cas::SymExprType::Var: {
            const auto* v = static_cast<const cas::SymVar*>(expr);
            return v->name;
        }
        case cas::SymExprType::Neg: {
            const auto* n = static_cast<const cas::SymNeg*>(expr);
            return detectVariable(n->child);
        }
        case cas::SymExprType::Add: {
            const auto* a = static_cast<const cas::SymAdd*>(expr);
            for (uint16_t i = 0; i < a->count; ++i) {
                char v = detectVariable(a->terms[i]);
                if (v != 0) return v;
            }
            return 0;
        }
        case cas::SymExprType::Mul: {
            const auto* m = static_cast<const cas::SymMul*>(expr);
            for (uint16_t i = 0; i < m->count; ++i) {
                char v = detectVariable(m->factors[i]);
                if (v != 0) return v;
            }
            return 0;
        }
        case cas::SymExprType::Pow: {
            const auto* p = static_cast<const cas::SymPow*>(expr);
            char v = detectVariable(p->base);
            return (v != 0) ? v : detectVariable(p->exponent);
        }
        case cas::SymExprType::Func: {
            const auto* f = static_cast<const cas::SymFunc*>(expr);
            return detectVariable(f->argument);
        }
        default:
            return 0;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Compute integral — Core pipeline
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::computeIntegral() {
    if (!_inputRow || _inputRow->isEmpty()) {
        _statusBar.setTitle("Empty input");
        _statusBar.update();
        return;
    }

    // Show computing animation
    showComputing();

    // Reset arena and steps for this computation
    _arena.reset();
    _integSteps.clear();
    _integralExpr = nullptr;
    _integralFound = false;

    // Step 1: Flatten MathAST → SymExpr
    _integSteps.logNote("Converting expression to symbolic form");

    cas::ASTFlattener flattener;
    flattener.setArena(&_arena);
    cas::SymExpr* expr = flattener.flattenToExpr(_inputRow);

    if (!expr) {
        _integSteps.logNote("Error: could not interpret the expression");
        _integralExpr = nullptr;
        showResult();
        return;
    }

    // Step 2: Detect variable
    _variable = detectVariable(expr);
    if (_variable == 0) _variable = 'x';

    // Render original expression via MathCanvas (no toString)
    _integSteps.logExpr("Original expression:", expr);
    {
        char varBuf[48];
        snprintf(varBuf, sizeof(varBuf),
                 "Integrating with respect to '%c'", _variable);
        _integSteps.logNote(varBuf);
    }

    // Step 3: Integrate
    _integSteps.logNote("Searching for symbolic antiderivative");

    cas::SymExpr* antideriv = cas::SymIntegrate::integrate(expr, _variable, _arena);

    if (antideriv) {
        // Closed-form antiderivative found
        _integralFound = true;
        _integralExpr = antideriv;

        // Render antiderivative via MathCanvas (no toString)
        _integSteps.logExpr("Antiderivative found:", antideriv);

        // Check if the result is polynomial
        if (_integralExpr->isPolynomial()) {
            _integSteps.logNote("Result: polynomial expression");
        } else {
            _integSteps.logNote("Result: transcendental expression");
        }
    } else {
        // No closed-form found — store original expr for unevaluated display
        _integralFound = false;
        _integralExpr = expr;

        _integSteps.logNote("No closed-form antiderivative found");
        _integSteps.logNote("Displaying unevaluated integral");
    }

    showResult();
}

// ════════════════════════════════════════════════════════════════════════════
// Build result display
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::buildResultDisplay() {
    // Clear previous
    _resultCanvas.stopCursorBlink();
    lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    _resultNode.reset();
    _resultRow = nullptr;

    _originalCanvas.stopCursorBlink();
    lv_obj_add_flag(_originalCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    _originalNode.reset();
    _originalRow = nullptr;

    if (!_integralExpr) {
        // Error case
        lv_label_set_text(_resultTitle, "Error de integracion");
        lv_label_set_text(_resultLabel, "");
        return;
    }

    // Show original expression (small)
    if (_inputRow) {
        _originalNode = cloneNode(_inputRow);
        _originalRow = static_cast<NodeRow*>(_originalNode.get());

        char fLabel[16];
        snprintf(fLabel, sizeof(fLabel), "f(%c) =", _variable);
        lv_label_set_text(_originalLabel, fLabel);

        lv_obj_remove_flag(_originalCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
        _originalCanvas.setExpression(_originalRow, nullptr);
        _originalRow->calculateLayout(_originalCanvas.normalMetrics());
        _originalCanvas.invalidate();
    }

    if (_integralFound) {
        // ── Closed-form result: show F(x) = antiderivative + C ──
        lv_label_set_text(_resultTitle, "Integral simbolica");

        char iLabel[24];
        snprintf(iLabel, sizeof(iLabel), "F(%c) =", _variable);
        lv_label_set_text(_resultLabel, iLabel);

        // Convert antiderivative to MathAST with + C
        NodePtr integAST = cas::SymExprToAST::convertIntegral(_integralExpr);

        if (integAST) {
            if (integAST->type() == NodeType::Row) {
                _resultNode = std::move(integAST);
            } else {
                auto row = makeRow();
                static_cast<NodeRow*>(row.get())->appendChild(std::move(integAST));
                _resultNode = std::move(row);
            }
            _resultRow = static_cast<NodeRow*>(_resultNode.get());

            lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
            _resultCanvas.setExpression(_resultRow, nullptr);
            _resultRow->calculateLayout(_resultCanvas.normalMetrics());
            _resultCanvas.invalidate();
        }
    } else {
        // ── Unevaluated integral: show ∫f(x)dx ──
        lv_label_set_text(_resultTitle, "Integral sin evaluar");

        char iLabel[32];
        snprintf(iLabel, sizeof(iLabel), "\xE2\x88\xAB" "f(%c)d%c =", _variable, _variable);
        lv_label_set_text(_resultLabel, iLabel);

        // Show the original expression (it stays as ∫)
        NodePtr exprAST = cas::SymExprToAST::convert(_integralExpr);

        if (exprAST) {
            if (exprAST->type() == NodeType::Row) {
                _resultNode = std::move(exprAST);
            } else {
                auto row = makeRow();
                static_cast<NodeRow*>(row.get())->appendChild(std::move(exprAST));
                _resultNode = std::move(row);
            }
            _resultRow = static_cast<NodeRow*>(_resultNode.get());

            lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
            _resultCanvas.setExpression(_resultRow, nullptr);
            _resultRow->calculateLayout(_resultCanvas.normalMetrics());
            _resultCanvas.invalidate();
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Build steps display
// ════════════════════════════════════════════════════════════════════════════

void IntegralApp::buildStepsDisplay() {
    // Destroy step MathCanvas widgets FIRST (avoids double-free)
    _stepRenderers.clear();

    lv_obj_clean(_stepsContainer);

    const auto& steps = _integSteps.steps();

    if (steps.empty()) {
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        lv_label_set_text(lbl, "No steps available.");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    static constexpr int STEP_CANVAS_W = SCREEN_W - 2 * PAD - 16;

    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];

        // ── Text description label ───────────────────────────────────
        char buf[200];
        snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                 step.description.c_str());

        lv_obj_t* descLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(descLbl, buf);
        lv_obj_set_width(descLbl, SCREEN_W - 2 * PAD - 8);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(COL_DESC_HEX), LV_PART_MAIN);

        // ── 2D MathCanvas rendering (Phase 5 Steering Visual) ────────
        if (step.mathExpr) {
            vpam::NodePtr astNode = cas::SymExprToAST::convert(step.mathExpr);
            if (astNode && astNode->type() == vpam::NodeType::Row) {
                auto srd = std::make_unique<StepRenderData>();
                srd->nodeData = std::move(astNode);
                srd->canvas.create(_stepsContainer);

                auto* row = static_cast<vpam::NodeRow*>(srd->nodeData.get());
                srd->canvas.setExpression(row, nullptr);
                row->calculateLayout(srd->canvas.normalMetrics());

                int contentH = row->layout().ascent + row->layout().descent;
                int canvasH = contentH + 8;
                if (canvasH < 22) canvasH = 22;
                if (canvasH > 80) canvasH = 80;
                lv_obj_set_size(srd->canvas.obj(), STEP_CANVAS_W, canvasH);
                lv_obj_set_style_bg_color(srd->canvas.obj(),
                                          lv_color_hex(0xF5F5F0), LV_PART_MAIN);
                srd->canvas.invalidate();

                _stepRenderers.push_back(std::move(srd));
            }
        }

        // ── Equation snapshot fallback (text, for legacy steps) ──────
        if (!step.mathExpr) {
            std::string eqText = step.snapshot.toString();
            if (!eqText.empty() && eqText != "0") {
                std::string full = "   " + eqText;
                lv_obj_t* eqLbl = lv_label_create(_stepsContainer);
                lv_label_set_text(eqLbl, full.c_str());
                lv_obj_set_width(eqLbl, SCREEN_W - 2 * PAD - 8);
                lv_label_set_long_mode(eqLbl, LV_LABEL_LONG_WRAP);
                lv_obj_set_style_text_font(eqLbl, &lv_font_montserrat_14, LV_PART_MAIN);
                lv_obj_set_style_text_color(eqLbl, lv_color_hex(COL_STEP_HEX), LV_PART_MAIN);
            }
        }
    }

    // Footer hint
    lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
    lv_label_set_text(hintLbl, LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
}
