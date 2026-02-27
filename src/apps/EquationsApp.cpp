/**
 * EquationsApp.cpp — Equation Solver App for NumOS.
 *
 * LVGL-native app with four states:
 *   SELECT → INPUT → RESULT → STEPS
 *
 * Uses CAS-Lite pipeline:
 *   MathAST → ASTFlattener → SingleSolver / SystemSolver → SymToAST → display
 *
 * Part of: NumOS CAS-Lite — Phase E (EquationsApp UI)
 */

#include "EquationsApp.h"
#include "../math/MathAST.h"
#include "../math/cas/SymToAST.h"
#include <cstring>

using namespace vpam;

// ════════════════════════════════════════════════════════════════════════════
// Layout constants (320×240 display)
// ════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t COL_BG_HEX      = 0xFFFFFF;   // White background
static constexpr uint32_t COL_SEP_HEX     = 0x333333;   // Separator gray
static constexpr uint32_t COL_ACCENT_HEX  = 0xE05500;   // Orange accent
static constexpr uint32_t COL_SELECT_HEX  = 0x4A90D9;   // Blue selection
static constexpr uint32_t COL_HINT_HEX    = 0x888888;   // Gray hint text
static constexpr uint32_t COL_ACTIVE_HEX  = 0xE05500;   // Orange active indicator
static constexpr uint32_t COL_STEP_HEX    = 0x1A1A1A;   // Dark step text
static constexpr uint32_t COL_DESC_HEX    = 0x2E7D32;   // Green step description

static constexpr int SCREEN_W  = 320;
static constexpr int SCREEN_H  = 240;
static constexpr int BAR_H     = ui::StatusBar::HEIGHT + 1;  // 25
static constexpr int PAD       = 6;
static constexpr int CONTENT_Y = BAR_H + 4;

// ════════════════════════════════════════════════════════════════════════════
// Menu labels
// ════════════════════════════════════════════════════════════════════════════

static const char* MENU_ITEMS[] = {
    "Ecuacion de Grado 1",
    "Ecuacion de Grado 2",
    "Sistema 2x2",
    "Sistema 3x3"
};
static constexpr int MENU_COUNT = 4;

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

EquationsApp::EquationsApp()
    : _screen(nullptr)
    , _selectContainer(nullptr)
    , _inputContainer(nullptr)
    , _inputTitle(nullptr)
    , _inputHint(nullptr)
    , _resultContainer(nullptr)
    , _resultTitle(nullptr)
    , _resultHint(nullptr)
    , _stepsContainer(nullptr)
    , _state(State::SELECT)
    , _type(EqType::DEGREE_1)
    , _selectIndex(0)
    , _numInputs(1)
    , _activeInput(0)
    , _stepScroll(0)
    , _isSingleEq(true)
    , _resultCount(0)
{
    for (int i = 0; i < MAX_EQS; ++i) {
        _menuLabels[i] = nullptr;
        _inputIndicators[i] = nullptr;
        _inputRow[i] = nullptr;
        _resultRow[i] = nullptr;
    }
    _menuLabels[3] = nullptr;   // 4th menu label
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
    _state = State::SELECT;
    showSelect();
}

void EquationsApp::end() {
    // Destroy MathCanvas instances
    for (int i = 0; i < MAX_EQS; ++i) {
        _inputCanvas[i].stopCursorBlink();
        _inputCanvas[i].destroy();
        _inputNode[i].reset();
        _inputRow[i] = nullptr;

        _resultCanvas[i].destroy();
        _resultNode[i].reset();
        _resultRow[i] = nullptr;
    }
    _statusBar.destroy();

    if (_screen) {
        lv_obj_delete(_screen);
        _screen           = nullptr;
        _selectContainer  = nullptr;
        _inputContainer   = nullptr;
        _inputTitle       = nullptr;
        _inputHint        = nullptr;
        _resultContainer  = nullptr;
        _resultTitle      = nullptr;
        _resultHint       = nullptr;
        _stepsContainer   = nullptr;
        for (int i = 0; i < 4; ++i) _menuLabels[i] = nullptr;
        for (int i = 0; i < MAX_EQS; ++i) _inputIndicators[i] = nullptr;
    }

    _state = State::SELECT;
    _selectIndex = 0;
    _resultCount = 0;

    // Free PSRAM-backed step logs to prevent leaks across sessions
    _singleResult.steps.clear();
    _systemResult.steps.clear();
}

void EquationsApp::load() {
    if (!_screen) begin();
    lv_screen_load(_screen);
    _statusBar.update();

    if (_state == State::EQ_INPUT) {
        _inputCanvas[_activeInput].startCursorBlink();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// UI Creation
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::createUI() {
    // ── Screen ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── StatusBar ──
    _statusBar.create(_screen);
    _statusBar.setTitle("Ecuaciones");
    _statusBar.setBatteryLevel(100);

    // ─────────────────────────────────────────────────────────────────
    // SELECT container
    // ─────────────────────────────────────────────────────────────────
    _selectContainer = lv_obj_create(_screen);
    lv_obj_set_size(_selectContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_selectContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_selectContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_selectContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_selectContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_selectContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    lv_obj_t* selectTitle = lv_label_create(_selectContainer);
    lv_label_set_text(selectTitle, "Selecciona el tipo:");
    lv_obj_set_style_text_font(selectTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(selectTitle, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_set_pos(selectTitle, PAD + 20, 12);

    // Menu items
    for (int i = 0; i < MENU_COUNT; ++i) {
        _menuLabels[i] = lv_label_create(_selectContainer);
        lv_label_set_text(_menuLabels[i], MENU_ITEMS[i]);
        lv_obj_set_style_text_font(_menuLabels[i], &lv_font_montserrat_14,
                                   LV_PART_MAIN);
        lv_obj_set_pos(_menuLabels[i], PAD + 30, 44 + i * 32);
    }

    // Hint
    lv_obj_t* selectHint = lv_label_create(_selectContainer);
    lv_label_set_text(selectHint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navegar    "
                                  "ENTER Seleccionar");
    lv_obj_set_style_text_font(selectHint, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(selectHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(selectHint, PAD + 20, SCREEN_H - BAR_H - 26);

    // ─────────────────────────────────────────────────────────────────
    // INPUT container
    // ─────────────────────────────────────────────────────────────────
    _inputContainer = lv_obj_create(_screen);
    lv_obj_set_size(_inputContainer, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_set_pos(_inputContainer, 0, BAR_H);
    lv_obj_set_style_bg_opa(_inputContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_inputContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_inputContainer, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_inputContainer, LV_OBJ_FLAG_SCROLLABLE);

    _inputTitle = lv_label_create(_inputContainer);
    lv_obj_set_style_text_font(_inputTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_inputTitle, lv_color_hex(COL_ACCENT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_inputTitle, PAD, 4);

    // Create MathCanvas for each equation slot
    for (int i = 0; i < MAX_EQS; ++i) {
        // Active line indicator (►)
        _inputIndicators[i] = lv_label_create(_inputContainer);
        lv_label_set_text(_inputIndicators[i], LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(_inputIndicators[i],
                                    lv_color_hex(COL_ACTIVE_HEX), LV_PART_MAIN);
        lv_obj_set_style_text_font(_inputIndicators[i],
                                   &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_add_flag(_inputIndicators[i], LV_OBJ_FLAG_HIDDEN);

        // MathCanvas
        _inputCanvas[i].create(_inputContainer);
        lv_obj_add_flag(_inputCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
    }

    _inputHint = lv_label_create(_inputContainer);
    lv_obj_set_style_text_font(_inputHint, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_inputHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    lv_obj_set_pos(_inputHint, PAD, SCREEN_H - BAR_H - 18);

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

    // MathCanvas for each solution line
    for (int i = 0; i < MAX_EQS; ++i) {
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
    // Scrollable
    lv_obj_add_flag(_stepsContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_stepsContainer, LV_DIR_VER);

    // ── Start with all containers hidden ──
    hideAllContainers();
}

// ════════════════════════════════════════════════════════════════════════════
// Container visibility
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::hideAllContainers() {
    if (_selectContainer) lv_obj_add_flag(_selectContainer, LV_OBJ_FLAG_HIDDEN);
    if (_inputContainer)  lv_obj_add_flag(_inputContainer,  LV_OBJ_FLAG_HIDDEN);
    if (_resultContainer) lv_obj_add_flag(_resultContainer, LV_OBJ_FLAG_HIDDEN);
    if (_stepsContainer)  lv_obj_add_flag(_stepsContainer,  LV_OBJ_FLAG_HIDDEN);

    // Stop cursor blinks
    for (int i = 0; i < MAX_EQS; ++i) {
        _inputCanvas[i].stopCursorBlink();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// State transitions
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::showSelect() {
    hideAllContainers();
    _state = State::SELECT;
    _statusBar.setTitle("Ecuaciones");

    // Update selection highlight
    for (int i = 0; i < MENU_COUNT; ++i) {
        if (i == _selectIndex) {
            lv_obj_set_style_text_color(_menuLabels[i],
                lv_color_hex(COL_SELECT_HEX), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(_menuLabels[i],
                lv_color_black(), LV_PART_MAIN);
        }
    }

    lv_obj_remove_flag(_selectContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showInput() {
    hideAllContainers();
    _state = State::EQ_INPUT;

    // Configure based on equation type
    switch (_type) {
        case EqType::DEGREE_1:
            _numInputs = 1;
            lv_label_set_text(_inputTitle, "Ecuacion de Grado 1");
            lv_label_set_text(_inputHint,
                "S" LV_SYMBOL_LEFT LV_SYMBOL_RIGHT "D: =   ENTER: Resolver   AC: Volver");
            break;
        case EqType::DEGREE_2:
            _numInputs = 1;
            lv_label_set_text(_inputTitle, "Ecuacion de Grado 2");
            lv_label_set_text(_inputHint,
                "S" LV_SYMBOL_LEFT LV_SYMBOL_RIGHT "D: =   ENTER: Resolver   AC: Volver");
            break;
        case EqType::SYSTEM_2X2:
            _numInputs = 2;
            lv_label_set_text(_inputTitle, "Sistema 2x2");
            lv_label_set_text(_inputHint,
                LV_SYMBOL_UP LV_SYMBOL_DOWN ": Ec.   S" LV_SYMBOL_LEFT LV_SYMBOL_RIGHT
                "D: =   ENTER: Resolver");
            break;
        case EqType::SYSTEM_3X3:
            _numInputs = 3;
            lv_label_set_text(_inputTitle, "Sistema 3x3");
            lv_label_set_text(_inputHint,
                LV_SYMBOL_UP LV_SYMBOL_DOWN ": Ec.   S" LV_SYMBOL_LEFT LV_SYMBOL_RIGHT
                "D: =   ENTER: Resolver");
            break;
    }

    _statusBar.setTitle(
        _type == EqType::DEGREE_1 ? "Grado 1" :
        _type == EqType::DEGREE_2 ? "Grado 2" :
        _type == EqType::SYSTEM_2X2 ? "Sistema 2x2" : "Sistema 3x3");

    // Layout input canvases
    int canvasY = 24;   // Below input title
    int canvasH;
    if (_numInputs == 1) {
        canvasH = 120;
    } else if (_numInputs == 2) {
        canvasH = 65;
    } else {
        canvasH = 45;
    }
    int canvasGap = 6;

    for (int i = 0; i < MAX_EQS; ++i) {
        if (i < _numInputs) {
            // Position indicator
            int y = canvasY + i * (canvasH + canvasGap);
            lv_obj_set_pos(_inputIndicators[i], PAD, y + canvasH / 2 - 6);
            lv_obj_remove_flag(_inputIndicators[i], LV_OBJ_FLAG_HIDDEN);

            // Position canvas
            lv_obj_set_pos(_inputCanvas[i].obj(), PAD + 16, y);
            lv_obj_set_size(_inputCanvas[i].obj(),
                            SCREEN_W - 2 * PAD - 16, canvasH);
            lv_obj_remove_flag(_inputCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);

            // Initialize input if needed
            if (!_inputNode[i]) {
                resetInput(i);
            }
            _inputCanvas[i].setExpression(_inputRow[i], &_inputCursor[i]);
            _inputCanvas[i].invalidate();
        } else {
            lv_obj_add_flag(_inputCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_inputIndicators[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    _activeInput = 0;
    updateIndicators();

    lv_obj_remove_flag(_inputContainer, LV_OBJ_FLAG_HIDDEN);
    _inputCanvas[_activeInput].startCursorBlink();
    lv_obj_invalidate(_screen);
}

void EquationsApp::showResult() {
    hideAllContainers();
    _state = State::RESULT;
    _statusBar.setTitle("Resultado");

    buildResultDisplay();

    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_screen);
}

void EquationsApp::showSteps() {
    hideAllContainers();
    _state = State::STEPS;
    _statusBar.setTitle("Pasos");
    _stepScroll = 0;

    buildStepsDisplay();

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
        case State::SELECT: handleKeySelect(ev); break;
        case State::EQ_INPUT:  handleKeyInput(ev);  break;
        case State::RESULT: handleKeyResult(ev); break;
        case State::STEPS:  handleKeySteps(ev);  break;
    }
}

// ────────────────────────────────────────────────────────────────────
// SELECT state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeySelect(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
            if (_selectIndex > 0) {
                --_selectIndex;
                showSelect();
            }
            break;

        case KeyCode::DOWN:
            if (_selectIndex < MENU_COUNT - 1) {
                ++_selectIndex;
                showSelect();
            }
            break;

        case KeyCode::ENTER:
            _type = static_cast<EqType>(_selectIndex);
            _isSingleEq = (_type == EqType::DEGREE_1 || _type == EqType::DEGREE_2);
            // Reset all inputs
            for (int i = 0; i < MAX_EQS; ++i) {
                _inputNode[i].reset();
                _inputRow[i] = nullptr;
            }
            showInput();
            break;

        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────────
// INPUT state keys
// ────────────────────────────────────────────────────────────────────

void EquationsApp::handleKeyInput(const KeyEvent& ev) {
    // Handle modifier keys
    auto& km = KeyboardManager::instance();
    if (ev.code == KeyCode::SHIFT) { km.pressShift(); _statusBar.update(); return; }
    if (ev.code == KeyCode::ALPHA) { km.pressAlpha(); _statusBar.update(); return; }

    // Effective code (for SHIFT combos)
    KeyCode effectiveCode = ev.code;

    auto& cc = _inputCursor[_activeInput];
    bool changed = false;

    switch (effectiveCode) {
        // ── Navigation ──
        case KeyCode::LEFT:
            cc.moveLeft();
            changed = true;
            break;
        case KeyCode::RIGHT:
            cc.moveRight();
            changed = true;
            break;

        // ── Switch equation (system mode) ──
        case KeyCode::UP:
            if (_numInputs > 1 && _activeInput > 0) {
                _inputCanvas[_activeInput].stopCursorBlink();
                --_activeInput;
                updateIndicators();
                _inputCanvas[_activeInput].startCursorBlink();
                refreshActiveInput();
            }
            break;
        case KeyCode::DOWN:
            if (_numInputs > 1 && _activeInput < _numInputs - 1) {
                _inputCanvas[_activeInput].stopCursorBlink();
                ++_activeInput;
                updateIndicators();
                _inputCanvas[_activeInput].startCursorBlink();
                refreshActiveInput();
            }
            break;

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
        case KeyCode::DIV:   cc.insertFraction(); changed = true; break;
        case KeyCode::POW:   cc.insertPower();    changed = true; break;
        case KeyCode::SQRT:  cc.insertRoot();     changed = true; break;
        case KeyCode::LPAREN: cc.insertParen();   changed = true; break;

        // ── Variables ──
        case KeyCode::VAR_X: cc.insertVariable('x'); changed = true; break;
        case KeyCode::VAR_Y: cc.insertVariable('y'); changed = true; break;

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

        // ── AC → back to SELECT ──
        case KeyCode::AC:
            for (int i = 0; i < MAX_EQS; ++i) {
                _inputNode[i].reset();
                _inputRow[i] = nullptr;
            }
            showSelect();
            break;

        // ── ENTER → solve ──
        case KeyCode::ENTER:
            solveEquations();
            break;

        default:
            break;
    }

    // Consume keyboard modifiers
    if (km.isShift()) km.consumeModifier();

    if (changed) {
        refreshActiveInput();
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
            showSelect();
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
// Input management
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::resetInput(int idx) {
    _inputNode[idx] = makeRow();
    _inputRow[idx]  = static_cast<NodeRow*>(_inputNode[idx].get());
    _inputCursor[idx].init(_inputRow[idx]);
}

void EquationsApp::refreshActiveInput() {
    _inputCanvas[_activeInput].setExpression(
        _inputRow[_activeInput], &_inputCursor[_activeInput]);
    _inputCanvas[_activeInput].invalidate();
    _inputCanvas[_activeInput].resetCursorBlink();
}

void EquationsApp::updateIndicators() {
    for (int i = 0; i < _numInputs; ++i) {
        if (i == _activeInput) {
            lv_obj_set_style_text_color(_inputIndicators[i],
                lv_color_hex(COL_ACTIVE_HEX), LV_PART_MAIN);
            lv_label_set_text(_inputIndicators[i], LV_SYMBOL_RIGHT);
        } else {
            lv_obj_set_style_text_color(_inputIndicators[i],
                lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
            lv_label_set_text(_inputIndicators[i], " ");
        }
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

    // Find the '=' NodeVariable
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
        // No '=' found: LHS = entire expression, RHS = 0
        outLHS = cloneNode(row);
        outRHS = makeRow();
        static_cast<NodeRow*>(outRHS.get())->appendChild(makeNumber("0"));
        return false;
    }

    // Build LHS: children [0 .. eqIdx-1]
    outLHS = makeRow();
    auto* lhsRow = static_cast<NodeRow*>(outLHS.get());
    for (int i = 0; i < eqIdx; ++i) {
        lhsRow->appendChild(cloneNode(row->child(i)));
    }

    // Build RHS: children [eqIdx+1 .. count-1]
    outRHS = makeRow();
    auto* rhsRow = static_cast<NodeRow*>(outRHS.get());
    for (int i = eqIdx + 1; i < count; ++i) {
        rhsRow->appendChild(cloneNode(row->child(i)));
    }

    // If LHS or RHS is empty, fill with "0"
    if (lhsRow->isEmpty()) lhsRow->appendChild(makeNumber("0"));
    if (rhsRow->isEmpty()) rhsRow->appendChild(makeNumber("0"));

    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Convert SymEquation → LinEq for system solver
// ════════════════════════════════════════════════════════════════════════════

cas::LinEq EquationsApp::symEquationToLinEq(const cas::SymEquation& eq,
                                            const char* vars, int numVars) {
    // Move everything to LHS: poly = 0
    cas::SymEquation norm = eq.moveAllToLHS();
    const auto& terms = norm.lhs.terms();

    cas::LinEq lin;

    // Extract coefficients for each variable
    for (int v = 0; v < numVars; ++v) {
        lin.coeffs[v] = vpam::ExactVal::fromInt(0);
        for (const auto& t : terms) {
            if (t.var == vars[v] && t.power == 1) {
                lin.coeffs[v] = t.coeff;
                break;
            }
        }
    }

    // Extract constant term and move to RHS (negate)
    vpam::ExactVal constant = vpam::ExactVal::fromInt(0);
    for (const auto& t : terms) {
        if (t.isConstant()) {
            constant = t.coeff;
            break;
        }
    }
    // poly = 0  →  vars = -constant
    lin.rhs = vpam::exactNeg(constant);

    return lin;
}

// ════════════════════════════════════════════════════════════════════════════
// Solve
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::solveEquations() {
    cas::ASTFlattener flattener;

    if (_isSingleEq) {
        // ── Single equation (Grado 1 or 2) ─────────────────────────
        NodePtr lhs, rhs;
        splitAtEquals(_inputRow[0], lhs, rhs);

        auto eqResult = flattener.flattenEquation(lhs.get(), rhs.get());
        if (!eqResult.ok) {
            // Show error on status bar and stay in INPUT
            _statusBar.setTitle(eqResult.error.empty() ? "Error" : "Syntax Error");
            _statusBar.update();
            return;
        }

        cas::SingleSolver solver;
        _singleResult = solver.solve(eqResult.eq);

        if (!_singleResult.ok) {
            _statusBar.setTitle(_singleResult.error.empty()
                                    ? "Sin solucion" : "Error");
            _statusBar.update();
            // Still show result (may have error message)
        }
        showResult();

    } else {
        // ── System of equations ────────────────────────────────────
        int numEqs = _numInputs;
        const char vars2[] = {'x', 'y'};
        const char vars3[] = {'x', 'y', 'z'};
        const char* vars = (numEqs == 2) ? vars2 : vars3;
        int numVars = numEqs;

        cas::LinEq linEqs[3];

        for (int i = 0; i < numEqs; ++i) {
            NodePtr lhs, rhs;
            splitAtEquals(_inputRow[i], lhs, rhs);

            auto eqResult = flattener.flattenEquation(lhs.get(), rhs.get());
            if (!eqResult.ok) {
                _statusBar.setTitle("Error en ecuacion");
                _statusBar.update();
                return;
            }

            linEqs[i] = symEquationToLinEq(
                cas::SymEquation(eqResult.eq.lhs, eqResult.eq.rhs),
                vars, numVars);
        }

        cas::SystemSolver sysSolver;
        if (numEqs == 2) {
            _systemResult = sysSolver.solve2x2(linEqs[0], linEqs[1]);
        } else {
            _systemResult = sysSolver.solve3x3(
                linEqs[0], linEqs[1], linEqs[2]);
        }

        if (!_systemResult.ok) {
            _statusBar.setTitle(_systemResult.error.empty()
                                    ? "Sin solucion" : "Error");
            _statusBar.update();
        }
        showResult();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Build result display
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildResultDisplay() {
    // Clear previous result canvases
    for (int i = 0; i < MAX_EQS; ++i) {
        _resultCanvas[i].stopCursorBlink();
        lv_obj_add_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);
        _resultNode[i].reset();
        _resultRow[i] = nullptr;
    }
    _resultCount = 0;

    if (_isSingleEq) {
        // ── Single equation result ─────────────────────────────────
        if (!_singleResult.ok) {
            lv_label_set_text(_resultTitle,
                _singleResult.error.empty() ? "Sin solucion real"
                                            : _singleResult.error.c_str());
            return;
        }

        lv_label_set_text(_resultTitle, _singleResult.numeric
            ? "Solucion (numerica):" : "Solucion:");

        for (int i = 0; i < _singleResult.count && i < MAX_EQS; ++i) {
            // Build "var = value" AST (with subscript for multiple solutions)
            auto row = makeRow();
            auto* r = static_cast<NodeRow*>(row.get());

            if (_singleResult.count > 1) {
                // x₁, x₂ — variable with subscript index
                auto baseRow = makeRow();
                static_cast<NodeRow*>(baseRow.get())
                    ->appendChild(makeVariable(_singleResult.variable));
                auto subRow = makeRow();
                static_cast<NodeRow*>(subRow.get())
                    ->appendChild(makeNumber(std::to_string(i + 1)));
                // Use power node visually (rendered small) as subscript hack
                // Actually, just display as "x1 =" / "x2 =" for clarity
                r->appendChild(makeVariable(_singleResult.variable));
                r->appendChild(makeNumber(std::to_string(i + 1)));
            } else {
                r->appendChild(makeVariable(_singleResult.variable));
            }
            r->appendChild(makeVariable('='));

            NodePtr valNode = cas::SymToAST::fromExactVal(_singleResult.solutions[i]);
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

            int y = 28 + i * 50;
            lv_obj_set_pos(_resultCanvas[i].obj(), PAD + 20, y);
            lv_obj_set_size(_resultCanvas[i].obj(), SCREEN_W - 2 * PAD - 20, 44);
            lv_obj_remove_flag(_resultCanvas[i].obj(), LV_OBJ_FLAG_HIDDEN);

            _resultCanvas[i].setExpression(_resultRow[i], nullptr);
            _resultCanvas[i].invalidate();
            ++_resultCount;
        }

    } else {
        // ── System result ──────────────────────────────────────────
        if (!_systemResult.ok) {
            lv_label_set_text(_resultTitle,
                _systemResult.error.empty() ? "Sistema incompatible"
                                            : _systemResult.error.c_str());
            return;
        }

        lv_label_set_text(_resultTitle, "Solucion del sistema:");

        for (int i = 0; i < _systemResult.numVars && i < MAX_EQS; ++i) {
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
            _resultCanvas[i].invalidate();
            ++_resultCount;
        }
    }

    // Add hint label at bottom of result container
    if (!_resultHint || lv_obj_get_parent(_resultHint) != _resultContainer) {
        _resultHint = lv_label_create(_resultContainer);
        lv_obj_set_style_text_font(_resultHint, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(_resultHint, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
    }
    lv_label_set_text(_resultHint, "STEPS: Ver pasos    AC: Nuevo");
    lv_obj_set_pos(_resultHint, PAD, SCREEN_H - BAR_H - 22);
}

// ════════════════════════════════════════════════════════════════════════════
// Build steps display
// ════════════════════════════════════════════════════════════════════════════

void EquationsApp::buildStepsDisplay() {
    // Clear previous step labels
    lv_obj_clean(_stepsContainer);

    // Get the step log to display
    const cas::CASStepLogger& log = _isSingleEq
        ? _singleResult.steps
        : _systemResult.steps;

    const auto& steps = log.steps();

    if (steps.empty()) {
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        lv_label_set_text(lbl, "No hay pasos disponibles.");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];

        // Step number + description
        char buf[160];
        snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                 step.description.c_str());

        lv_obj_t* descLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(descLbl, buf);
        lv_obj_set_width(descLbl, SCREEN_W - 2 * PAD - 8);
        lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(descLbl, lv_color_hex(COL_DESC_HEX), LV_PART_MAIN);

        // Equation snapshot as text (toString)
        std::string eqText = "   " + step.snapshot.toString();
        lv_obj_t* eqLbl = lv_label_create(_stepsContainer);
        lv_label_set_text(eqLbl, eqText.c_str());
        lv_obj_set_width(eqLbl, SCREEN_W - 2 * PAD - 8);
        lv_label_set_long_mode(eqLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(eqLbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(eqLbl, lv_color_hex(COL_STEP_HEX), LV_PART_MAIN);
    }

    // Footer hint
    lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
    lv_label_set_text(hintLbl, LV_SYMBOL_UP LV_SYMBOL_DOWN " Desplazar    AC: Volver");
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
}
