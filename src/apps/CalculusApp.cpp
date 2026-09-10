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
 * CalculusApp.cpp — Unified Symbolic Calculus App for NumOS.
 *
 * GIAC-E01: Giac is the sole normal-build answer authority for the enabled
 * first-derivative and indefinite-integral modes. The native symbolic
 * engines remain available only for consistency-gated educational steps.
 *
 * Normal results reuse CalculationEngine's authored-AST serializer and
 * structured result adapter; unsupported Giac presentation shapes retain
 * their exact text.
 *
 * Part of: NumOS CAS — Phase 4 (Unified Calculus App)
 */

#include "CalculusApp.h"
#include "../math/MathAST.h"
#include "../math/cas/SymToAST.h"
#include "../utils/HwUxProbe.h"
#include "../input/generated/ProductionKeypadMap.generated.h"
#include <cmath>
#include <cstdlib>

#ifdef NATIVE_SIM
#include <chrono>
class CalculusTiming {
    const char* _action;
    int _key;
    std::chrono::steady_clock::time_point _start = std::chrono::steady_clock::now();
public:
    explicit CalculusTiming(const char* action, int key = -1) : _action(action), _key(key) {}
    ~CalculusTiming() {
        if (std::getenv("NUMOS_CALCULUS_METRICS"))
            std::printf("CALCULUS_TIME|%s|key=%d|us=%lld\n", _action, _key,
                (long long)std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now()-_start).count());
    }
};
#endif

using namespace vpam;

// ════════════════════════════════════════════════════════════════════════════
// Layout constants (320×240 display)
// ════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t COL_BG_HEX      = 0xFFFFFF;
static constexpr uint32_t COL_HINT_HEX    = 0x888888;
static constexpr uint32_t COL_STEP_HEX    = 0x1A1A1A;
static constexpr uint32_t COL_DESC_HEX    = 0x2E7D32;

// Mode-specific accent colors
static constexpr uint32_t COL_DERIV_HEX   = 0xE05500;  // Orange for d/dx
static constexpr uint32_t COL_INTEG_HEX   = 0x6A1B9A;  // Purple for ∫dx

static constexpr int SCREEN_W  = 320;
static constexpr int SCREEN_H  = 240;
// Every region is inside the production 320 x 240 viewport.
static constexpr int BAR_H = ui::StatusBar::HEIGHT + 1; // includes separator
static constexpr int PAD = 6;
static constexpr int TAB_H = 33;
static constexpr int CONTENT_Y = BAR_H + TAB_H; // 58
static constexpr int FOOTER_Y = 222;
static constexpr int CONTENT_H = FOOTER_Y - CONTENT_Y; // 164

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

CalculusApp::CalculusApp()
    : _scr(nullptr)
    , _tabDerivative(nullptr)
    , _tabIntegral(nullptr)
    , _inputContainer(nullptr)
    , _inputTitle(nullptr)
    , _inputHint(nullptr)
    , _computingContainer(nullptr)
    , _computingLabel(nullptr)
    , _resultContainer(nullptr)
    , _resultTitle(nullptr)
    , _resultFallback(nullptr)
    , _resultHint(nullptr)
    , _originalLabel(nullptr)
    , _stepsContainer(nullptr)
    , _resultViewport(nullptr)
    , _modeFocused(false)
    , _state(State::EDITING)
    , _calcMode(CalcMode::DERIVATIVE)
    , _stepScroll(0)
    , _variable('x')
    , _inputRow(nullptr)
    , _resultRow(nullptr)
    , _originalRow(nullptr)
    , _resultExpr(nullptr)
    , _integralFound(false)
    , _resultKind(ResultKind::None)
    , _tutorStatus(TutorStatus::Disabled)
#ifdef NATIVE_SIM
    , _debugForceTutorDisagreement(false)
#endif
{
}

CalculusApp::~CalculusApp() {
    end();
}

const char* CalculusApp::debugEngineName() const {
    return "giac";
}

const char* CalculusApp::debugStatusName() const {
    switch (_giacResult.status) {
        case numos::MathEngineStatus::Ok:              return "ok";
        case numos::MathEngineStatus::Undefined:       return "undefined";
        case numos::MathEngineStatus::ParseError:      return "parse_error";
        case numos::MathEngineStatus::EvaluationError: return "evaluation_error";
        case numos::MathEngineStatus::Unsupported:     return "unsupported";
        case numos::MathEngineStatus::OutOfMemory:     return "out_of_memory";
    }
    return "unsupported";
}

const char* CalculusApp::debugResultKindName() const {
    switch (_resultKind) {
        case ResultKind::Structured:   return "structured";
        case ResultKind::TextFallback: return "text_fallback";
        case ResultKind::None:         return "none";
    }
    return "none";
}

const char* CalculusApp::debugTutorStatusName() const {
    switch (_tutorStatus) {
        case TutorStatus::Agreed:      return "agreed";
        case TutorStatus::Unavailable: return "unavailable";
        case TutorStatus::Disabled:    return "disabled";
    }
    return "disabled";
}

const char* CalculusApp::debugOperationName() const {
    return _calcMode == CalcMode::DERIVATIVE
        ? "differentiate" : "integrate_indefinite";
}

const std::string& CalculusApp::debugExactText() const {
    return _giacResult.exactText;
}

bool CalculusApp::debugResultNear(double expected, double epsilon) const {
    const std::string& exact = debugExactText();
    const std::string& candidate = _giacResult.approximateText.empty()
        ? exact : _giacResult.approximateText;
    if (candidate.empty() || epsilon < 0.0) return false;
    char* end = nullptr;
    const double actual = std::strtod(candidate.c_str(), &end);
    return end && *end == '\0' && std::isfinite(actual) &&
           std::fabs(actual - expected) <= epsilon;
}

#ifdef NATIVE_SIM
const char* CalculusApp::debugStateName() const {
    switch (_state) {
        case State::EDITING: return "editing";
        case State::COMPUTING: return "computing";
        case State::RESULT: return "result";
        case State::STEPS: return "steps";
    }
    return "unknown";
}
const char* CalculusApp::debugFocusName() const {
    return _state == State::EDITING ? (_modeFocused ? "mode" : "editor") : debugStateName();
}
bool CalculusApp::debugResultEquivalent(const std::string& expected) const {
    if (!_giacResult.ok() || _giacResult.unevaluated || _giacResult.exactText.empty()) return false;
    const std::string delta = "(" + _giacResult.exactText + ")-(" + expected + ")";
    auto result = numos::GiacEngine::instance().simplify(delta.c_str());
    return result.ok() && result.exactText == "0";
}
static bool fitsChildren(lv_obj_t* parent, bool viewport = false) {
    lv_area_t p; lv_obj_get_coords(parent, &p);
    for (uint32_t i=0; i<lv_obj_get_child_count(parent); ++i) {
        auto* child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t c; lv_obj_get_coords(child, &c);
        // A scroll viewport intentionally contains content taller than itself.
        if (!viewport && (c.x1 < p.x1 || c.y1 < p.y1 || c.x2 > p.x2 || c.y2 > p.y2)) return false;
        if (!fitsChildren(child, lv_obj_has_flag(child, LV_OBJ_FLAG_SCROLLABLE))) return false;
    }
    return true;
}
bool CalculusApp::debugLayoutFits() const {
    if (!_scr) return false;
    lv_obj_update_layout(_scr);
    return fitsChildren(_scr);
}
#endif

// ════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::begin() {
#ifdef NATIVE_SIM
    CalculusTiming timing("open");
#endif
    if (_scr) return;
    createUI();
    _state = State::EDITING;
    showInput();
}

void CalculusApp::end() {
#ifdef NATIVE_SIM
    CalculusTiming timing("teardown");
#endif
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

    _stepRenderers.clear();
    _statusBar.destroy();

    if (_scr) {
        lv_obj_delete(_scr);
        _scr               = nullptr;
        _tabDerivative     = nullptr;
        _tabIntegral       = nullptr;
        _inputContainer    = nullptr;
        _inputTitle        = nullptr;
        _inputHint         = nullptr;
        _computingContainer = nullptr;
        _computingLabel    = nullptr;
        _resultContainer   = nullptr;
        _resultTitle       = nullptr;
        _resultFallback    = nullptr;
        _resultHint        = nullptr;
        _originalLabel     = nullptr;
        _stepsContainer    = nullptr;
        _inputPlaceholder = nullptr;
        _originalViewport = nullptr;
        _resultViewport = nullptr;
        _resultSeparator = nullptr;
    }

    _state = State::EDITING;
    _modeFocused = false;
    _calcMode = CalcMode::DERIVATIVE;
    _resultExpr = nullptr;
    _integralFound = false;
    _giacResult = numos::StructuredCalculusResult();
    _resultKind = ResultKind::None;
    _tutorStatus = TutorStatus::Disabled;
    _serializedInput.clear();
    _tutorDiagnostic.clear();
    _casSteps.clear();
    _arena.reset();
}

void CalculusApp::load() {
    if (!_scr) begin();
    lv_screen_load_anim(_scr, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();

    if (_state == State::EDITING && !_modeFocused) {
        _inputCanvas.startCursorBlink();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Mode management
// ════════════════════════════════════════════════════════════════════════════

uint32_t CalculusApp::accentColor() const {
    return (_calcMode == CalcMode::DERIVATIVE) ? COL_DERIV_HEX : COL_INTEG_HEX;
}

void CalculusApp::setMode(CalcMode mode) {
#ifdef NATIVE_SIM
    CalculusTiming timing("mode");
#endif
    if (_calcMode == mode) return;
    _calcMode = mode;
    // A result always belongs to its operation. Preserve authored input only.
    _giacResult = numos::StructuredCalculusResult();
    _resultKind = ResultKind::None;
    _tutorStatus = TutorStatus::Unavailable;
    showInput();
    updateTabStyles();
}

void CalculusApp::updateTabStyles() {
    if (!_tabDerivative || !_tabIntegral) return;
    lv_obj_t* tabs[] = {_tabDerivative, _tabIntegral};
    for (int i = 0; i < 2; ++i) {
        const bool active = i == (_calcMode == CalcMode::DERIVATIVE ? 0 : 1);
        lv_obj_set_style_bg_color(tabs[i], lv_color_hex(active ? 0xF0F1F3 : COL_BG_HEX), 0);
        lv_obj_set_style_text_color(tabs[i], lv_color_hex(active ? accentColor() : 0x656565), 0);
        lv_obj_set_style_border_color(tabs[i], lv_color_hex(active ? accentColor() : 0xDDDDDD), 0);
        lv_obj_set_style_border_width(tabs[i], active ? 3 : 1, 0);
        lv_obj_set_style_border_side(tabs[i], LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_outline_width(tabs[i], _modeFocused && active ? 1 : 0, 0);
        lv_obj_set_style_outline_pad(tabs[i], 0, 0);
    }
    lv_obj_set_style_border_color(_inputContainer,
        lv_color_hex(_modeFocused ? 0xDDDDDD : accentColor()), 0);
    lv_label_set_text(_inputHint, _modeFocused
        ? LV_SYMBOL_LEFT "  " LV_SYMBOL_RIGHT " Mode     " LV_SYMBOL_DOWN " Edit"
        : "EXE Calculate    VAR x    UP Mode");
    if (_modeFocused) _inputCanvas.stopCursorBlink();
    else if (_state == State::EDITING) _inputCanvas.startCursorBlink();
}

// ════════════════════════════════════════════════════════════════════════════
// UI Creation
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::createUI() {
    _scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(_scr);
    lv_obj_set_size(_scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(_scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);
    _statusBar.create(_scr);
    _statusBar.setTitle("Calculus");
    _statusBar.setBatteryLevel(100);

    auto panel = [&](lv_obj_t* parent, int x, int y, int w, int h) {
        auto* obj = lv_obj_create(parent);
        lv_obj_remove_style_all(obj);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_size(obj, w, h);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    };
    auto label = [&](lv_obj_t* parent, const char* text, int x, int y,
                     const lv_font_t* font = &lv_font_montserrat_12) {
        auto* obj = lv_label_create(parent);
        lv_label_set_text(obj, text);
        lv_obj_set_style_text_font(obj, font, 0);
        lv_obj_set_style_text_color(obj, lv_color_hex(0x656565), 0);
        lv_obj_set_pos(obj, x, y);
        return obj;
    };
    auto surface = [&](lv_obj_t* obj) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(0xFAFAFA), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(obj, 1, 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_radius(obj, 3, 0);
    };
    auto* strip = panel(_scr, PAD, BAR_H + 3, SCREEN_W - 2*PAD, TAB_H - 6);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(strip, 4, 0);
    _tabDerivative = panel(strip, 0, 0, 152, TAB_H - 6);
    _tabIntegral = panel(strip, 0, 0, 152, TAB_H - 6);
    for (auto* tab : {_tabDerivative, _tabIntegral}) {
        lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
        lv_obj_set_style_outline_color(tab, lv_color_hex(0x333333), 0);
    }
    lv_obj_align(label(_tabDerivative, "Derivative", 0, 0, &lv_font_montserrat_14), LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_align(label(_tabIntegral, "Integral", 0, 0, &lv_font_montserrat_14), LV_ALIGN_TOP_MID, 0, 4);

    _inputContainer = panel(_scr, PAD, CONTENT_Y + 4, SCREEN_W - 2*PAD, CONTENT_H - 10);
    surface(_inputContainer);
    _inputTitle = label(_inputContainer, "f(x)", 8, 5);
    auto* inputViewport = panel(_inputContainer, 2, 24, SCREEN_W - 2*PAD - 6, CONTENT_H - 40);
    _inputCanvas.create(inputViewport);
    _inputCanvas.setAutoHeightEnabled(false);
    lv_obj_set_pos(_inputCanvas.obj(), 0, 0);
    lv_obj_set_size(_inputCanvas.obj(), SCREEN_W - 2*PAD - 6, CONTENT_H - 40);
    _inputPlaceholder = label(inputViewport, "Enter expression", 14, 52);
    lv_obj_set_style_text_color(_inputPlaceholder, lv_color_hex(0x888888), 0);
    _inputHint = label(_scr, "", PAD, FOOTER_Y + 2, &lv_font_montserrat_10);

    _computingContainer = panel(_scr, PAD, CONTENT_Y, SCREEN_W - 2*PAD, CONTENT_H);
    _computingLabel = label(_computingContainer, "Calculating...", 8, 60);
    lv_obj_center(_computingLabel);

    _resultContainer = panel(_scr, PAD, CONTENT_Y + 4, SCREEN_W - 2*PAD, CONTENT_H - 10);
    surface(_resultContainer);
    _originalLabel = label(_resultContainer, "f(x)", 8, 4);
    _originalViewport = panel(_resultContainer, 42, 2, SCREEN_W - 2*PAD - 46, 48);
    _originalCanvas.create(_originalViewport);
    _originalCanvas.setAutoHeightEnabled(false);
    lv_obj_set_pos(_originalCanvas.obj(), 0, 0);
    lv_obj_set_size(_originalCanvas.obj(), SCREEN_W - 2*PAD - 46, 48);
    _resultTitle = label(_resultContainer, "Result", 8, 56);
    _resultSeparator = panel(_resultContainer, 8, 51, SCREEN_W - 2*PAD - 18, 1);
    lv_obj_set_style_bg_color(_resultSeparator, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_bg_opa(_resultSeparator, LV_OPA_COVER, 0);
    _resultViewport = panel(_resultContainer, 2, 76, SCREEN_W - 2*PAD - 6, 74);
    lv_obj_set_scroll_dir(_resultViewport, LV_DIR_VER);
    lv_obj_add_flag(_resultViewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_resultViewport, LV_SCROLLBAR_MODE_AUTO);
    _resultCanvas.create(_resultViewport);
    _resultCanvas.setAutoHeightEnabled(false);
    lv_obj_set_size(_resultCanvas.obj(), SCREEN_W - 2*PAD - 8, 72);
    _resultFallback = label(_resultViewport, "", 8, 5);
    lv_obj_set_width(_resultFallback, SCREEN_W - 2*PAD - 24);
    lv_label_set_long_mode(_resultFallback, LV_LABEL_LONG_WRAP);
    _resultHint = label(_scr, "", PAD, FOOTER_Y + 2, &lv_font_montserrat_10);

    _stepsContainer = panel(_scr, PAD, CONTENT_Y + 4, SCREEN_W - 2*PAD, CONTENT_H - 10);
    lv_obj_set_flex_flow(_stepsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_stepsContainer, 4, 0);
    lv_obj_add_flag(_stepsContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_stepsContainer, LV_DIR_VER);
    updateTabStyles();
    hideAllContainers();
}

// ════════════════════════════════════════════════════════════════════════════
// Container visibility
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::hideAllContainers() {
    if (_inputContainer)    lv_obj_add_flag(_inputContainer,    LV_OBJ_FLAG_HIDDEN);
    if (_computingContainer) lv_obj_add_flag(_computingContainer, LV_OBJ_FLAG_HIDDEN);
    if (_resultContainer)   lv_obj_add_flag(_resultContainer,   LV_OBJ_FLAG_HIDDEN);
    if (_stepsContainer)    lv_obj_add_flag(_stepsContainer,    LV_OBJ_FLAG_HIDDEN);
    if (_inputHint) lv_obj_add_flag(_inputHint, LV_OBJ_FLAG_HIDDEN);
    if (_resultHint) lv_obj_add_flag(_resultHint, LV_OBJ_FLAG_HIDDEN);

    _inputCanvas.stopCursorBlink();
}

// ════════════════════════════════════════════════════════════════════════════
// State transitions
// ════════════════════════════════════════════════════════════════════════════

#ifdef NATIVE_SIM
static void calculusLayoutDump(lv_obj_t* obj, int depth = 0) {
    if (!std::getenv("NUMOS_CALCULUS_LAYOUT") || !obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_update_layout(obj);
    lv_area_t a; lv_obj_get_coords(obj, &a);
    const char* text = lv_obj_check_type(obj, &lv_label_class) ? lv_label_get_text(obj) : "";
    std::printf("CALCULUS_BOX|%d|%d,%d,%d,%d|%s\n", depth, a.x1,a.y1,a.x2,a.y2,text);
    for (uint32_t i=0;i<lv_obj_get_child_count(obj);++i) calculusLayoutDump(lv_obj_get_child(obj,i),depth+1);
}
#endif

void CalculusApp::showInput() {
    hideAllContainers();
    _state = State::EDITING;
    _statusBar.setTitle("Calculus");

    if (!_inputNode) resetInput();
    _giacResult = numos::StructuredCalculusResult();
    _resultKind = ResultKind::None;
    _inputCanvas.setExpression(_inputRow, &_inputCursor);
    _inputCanvas.invalidate();

    // Update UI for current mode
    updateTabStyles();

    lv_obj_remove_flag(_inputContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_inputHint, LV_OBJ_FLAG_HIDDEN);
    adjustInputHeight();
    if (!_modeFocused) _inputCanvas.startCursorBlink();
    lv_obj_invalidate(_scr);
#ifdef NATIVE_SIM
    calculusLayoutDump(_scr);
#endif
}

void CalculusApp::showComputing() {
    hideAllContainers();
    _state = State::COMPUTING;

    lv_label_set_text(_computingLabel, _calcMode == CalcMode::DERIVATIVE
        ? "Differentiating..." : "Integrating...");
    lv_obj_remove_flag(_computingContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_scr);

    // Paint once before blocking compute; there is no per-frame math work.
    lv_timer_handler();
}

void CalculusApp::showResult() {
    hideAllContainers();
    _state = State::RESULT;

    if (_calcMode == CalcMode::DERIVATIVE) {
        _statusBar.setTitle("Derivative");
    } else {
        _statusBar.setTitle("Integral");
    }

    _modeFocused = false;
    updateTabStyles();
    buildResultDisplay();
    lv_obj_remove_flag(_resultHint, LV_OBJ_FLAG_HIDDEN);

    lv_obj_remove_flag(_resultContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_scr);
#ifdef NATIVE_SIM
    calculusLayoutDump(_scr);
#endif
}

void CalculusApp::showSteps() {
    hideAllContainers();
    _state = State::STEPS;
    _statusBar.setTitle("Steps");
    lv_label_set_text(_resultHint, LV_SYMBOL_UP " " LV_SYMBOL_DOWN " Scroll    EXE Result");
    lv_obj_remove_flag(_resultHint, LV_OBJ_FLAG_HIDDEN);
    _stepScroll = 0;

    buildStepsDisplay();

    lv_obj_remove_flag(_stepsContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
    lv_obj_invalidate(_scr);
#ifdef NATIVE_SIM
    calculusLayoutDump(_scr);
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// Key Dispatch
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::handleKey(const KeyEvent& ev) {
#ifdef NATIVE_SIM
    CalculusTiming timing("key", (int)ev.code);
#endif
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    if (_state != State::COMPUTING &&
        (ev.code == KeyCode::GRAPH || ev.code == KeyCode::F1 || ev.code == KeyCode::F2)) {
        if (ev.action != KeyAction::PRESS) return;
        _modeFocused = false;
        setMode(ev.code == KeyCode::F1 ? CalcMode::DERIVATIVE :
                ev.code == KeyCode::F2 ? CalcMode::INTEGRAL :
                _calcMode == CalcMode::DERIVATIVE ? CalcMode::INTEGRAL : CalcMode::DERIVATIVE);
        KeyboardManager::instance().consumeModifier();
        return;
    }
    if (ev.action == KeyAction::REPEAT &&
        (ev.code == KeyCode::EXE || ev.code == KeyCode::ENTER || ev.code == KeyCode::FREE_EQ ||
         ev.code == KeyCode::AC || ev.code == KeyCode::SHOW_STEPS)) return;
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

void CalculusApp::handleKeyInput(const KeyEvent& ev) {
    auto& km = KeyboardManager::instance();
    if (ev.code == KeyCode::SHIFT) { km.pressShift(); _statusBar.update(); return; }
    if (ev.code == KeyCode::ALPHA) { km.pressAlpha(); _statusBar.update(); return; }

    if (_modeFocused) {
        if (ev.action != KeyAction::PRESS) return;
        if (ev.code == KeyCode::LEFT || ev.code == KeyCode::RIGHT) {
            setMode(ev.code == KeyCode::LEFT ? CalcMode::DERIVATIVE : CalcMode::INTEGRAL);
            return;
        }
        _modeFocused = false;
        updateTabStyles();
        if (ev.code == KeyCode::DOWN || ev.code == KeyCode::ENTER || ev.code == KeyCode::EXE) return;
    }
    auto& cc = _inputCursor;
    const auto semantic = static_cast<numos::input::SemanticId>(ev.semanticId);
    using numos::input::SemanticId;
    bool semanticChanged = true;
    if (semantic >= SemanticId::alpha_A && semantic <= SemanticId::alpha_Z) {
        cc.insertVariable('A' + static_cast<int>(semantic) - static_cast<int>(SemanticId::alpha_A));
    } else if (semantic == SemanticId::asin || semantic == SemanticId::acos || semantic == SemanticId::atan) {
        cc.insertFunction(semantic == SemanticId::asin ? FuncKind::ArcSin :
                          semantic == SemanticId::acos ? FuncKind::ArcCos : FuncKind::ArcTan);
    } else if (semantic == SemanticId::pow_e) {
        cc.insertConstant(ConstKind::E); cc.insertPower();
    } else {
        semanticChanged = false;
    }
    if (semanticChanged) { adjustInputHeight(); refreshInput(); _statusBar.update(); return; }
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
        case KeyCode::FRAC:
        case KeyCode::DIV:    cc.insertFraction(); changed = true; break;
        case KeyCode::POW:    cc.insertPower();    changed = true; break;
        case KeyCode::SQRT:   cc.insertRoot();     changed = true; break;
        case KeyCode::LPAREN: cc.insertParen();    changed = true; break;

        case KeyCode::RPAREN: cc.moveRight(); changed = true; break;
        case KeyCode::DIVIDE: cc.insertOperator(OpKind::Div); changed = true; break;
        case KeyCode::SQUARE:
            cc.insertPower(); cc.insertDigit('2'); cc.moveRight(); changed = true; break;
        // Calculus has a fixed primary variable, so VAR inserts it directly.
        case KeyCode::VAR: cc.insertVariable('x'); changed = true; break;

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
            _inputCanvas.setExpression(nullptr, nullptr);
            _inputNode.reset();
            _inputRow = nullptr;
            showInput();
            break;

        // ── ENTER or = → compute ──
        case KeyCode::EXE:
        case KeyCode::ENTER:
        case KeyCode::FREE_EQ:
            computeResult();
            break;

        case KeyCode::UP:
            if (cc.cursor().row == _inputRow) {
                if (ev.action == KeyAction::PRESS) { _modeFocused = true; updateTabStyles(); }
            } else { cc.moveUp(); changed = true; }
            break;
        case KeyCode::DOWN:
            cc.moveDown(); changed = true;
            break;

        default:
            break;
    }

    if (changed) km.consumeModifier();

    if (changed) {
        adjustInputHeight();
        refreshInput();
    }
}

// ────────────────────────────────────────────────────────────────────
// RESULT state keys
// ────────────────────────────────────────────────────────────────────

void CalculusApp::handleKeyResult(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::TOOLBOX:
        case KeyCode::SHOW_STEPS:
            if (_tutorStatus == TutorStatus::Agreed) showSteps();
            break;
        case KeyCode::UP: lv_obj_scroll_by(_resultViewport, 0, 24, LV_ANIM_OFF); break;
        case KeyCode::DOWN: lv_obj_scroll_by(_resultViewport, 0, -24, LV_ANIM_OFF); break;
        case KeyCode::LEFT: _resultCanvas.scrollBy(24); break;
        case KeyCode::RIGHT: _resultCanvas.scrollBy(-24); break;
        case KeyCode::AC:
            _inputCanvas.setExpression(nullptr, nullptr);
            _inputNode.reset(); _inputRow = nullptr;
            showInput();
            break;
        case KeyCode::DEL:
            showInput(); handleKeyInput(ev); break;
        case KeyCode::EXE:
        case KeyCode::ENTER:
        case KeyCode::FREE_EQ:
            showInput(); break;
        default: break;
    }
}

// ────────────────────────────────────────────────────────────────────
// STEPS state keys
// ────────────────────────────────────────────────────────────────────

void CalculusApp::handleKeySteps(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
            lv_obj_scroll_by(_stepsContainer, 0, 30, LV_ANIM_ON);
            break;
        case KeyCode::DOWN:
            lv_obj_scroll_by(_stepsContainer, 0, -30, LV_ANIM_ON);
            break;
        case KeyCode::EXE:
        case KeyCode::ENTER:
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

void CalculusApp::resetInput() {
    _inputNode = makeRow();
    _inputRow  = static_cast<NodeRow*>(_inputNode.get());
    _inputCursor.init(_inputRow);
    _inputCanvas.resetScroll();
}

void CalculusApp::refreshInput() {
    _inputCanvas.setExpression(_inputRow, &_inputCursor);
    _inputCanvas.invalidate();
    _inputCanvas.resetCursorBlink();
}

void CalculusApp::adjustInputHeight() {
    // Fixed, bounded viewport. MathCanvas owns horizontal cursor following;
    // oversized vertical structures are deliberately clipped inside this surface.
    if (!_inputRow) return;
    if (_inputRow->isEmpty()) lv_obj_remove_flag(_inputPlaceholder, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(_inputPlaceholder, LV_OBJ_FLAG_HIDDEN);
    _inputRow->calculateLayout(_inputCanvas.normalMetrics());
}

// ════════════════════════════════════════════════════════════════════════════
// Detect variable in expression
// ════════════════════════════════════════════════════════════════════════════

char CalculusApp::detectVariable(const cas::SymExpr* expr) {
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

bool CalculusApp::navigateBack() {
    switch (_state) {
        case State::STEPS:
            showResult();
            return true;
        case State::RESULT:
            showInput();
            return true;
        case State::COMPUTING:
            // The pinned Giac boundary is synchronous and cannot be torn down
            // safely. HOME/BACK are serviced immediately after it returns.
            return true;
        case State::EDITING:
            if (_modeFocused) { _modeFocused = false; updateTabStyles(); return true; }
            return false;
    }
    return false;
}

char CalculusApp::detectAuthoredVariable(const vpam::MathNode* node) const {
    if (!node) return 0;
    if (node->type() == vpam::NodeType::Variable) {
        const char name =
            static_cast<const vpam::NodeVariable*>(node)->name();
        return (name == 'x' || name == 'y') ? name : 0;
    }
    for (int i = 0; i < node->childCount(); ++i) {
        const char found = detectAuthoredVariable(node->child(i));
        if (found != 0) return found;
    }
    return 0;
}

// ════════════════════════════════════════════════════════════════════════════
// Compute result — Dispatches to derivative or integral
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::computeResult() {
#ifdef NATIVE_SIM
    CalculusTiming timing(_calcMode == CalcMode::DERIVATIVE ? "derivative" : "integral");
#endif
    if (!_inputRow || _inputRow->isEmpty()) {
        _statusBar.setTitle("Empty input");
        _statusBar.update();
        return;
    }

    numos::HwUxProbe hwux("calculus",
        _calcMode == CalcMode::DERIVATIVE ? "differentiate" : "integrate");

    // Paint a bounded busy state before the synchronous Giac call.
    showComputing();

    // Reset arena and steps for this computation
    _arena.reset();
    _casSteps.clear();
    _stepRenderers.clear();
    _resultExpr = nullptr;
    _integralFound = false;
    _resultKind = ResultKind::None;
    computeGiacResult();

    const char* kind = _resultKind == ResultKind::Structured
        ? "structured" : (_resultKind == ResultKind::TextFallback
        ? "text_fallback" : "none");
    hwux.finish(debugStatusName(), kind, "giac",
                 numos::hwUxHash(_giacResult.exactText.c_str()));
}

void CalculusApp::computeGiacResult() {
    _giacResult = numos::StructuredCalculusResult();
    _tutorStatus = TutorStatus::Unavailable;
    _tutorDiagnostic.clear();
    _serializedInput.clear();

    std::string serializerError;
    if (!numos::CalculationEngine::serializeForGiac(
            _inputRow, _serializedInput, serializerError)) {
        _giacResult.status = numos::MathEngineStatus::ParseError;
        _giacResult.diagnostic = serializerError;
        showResult();
        return;
    }

    _variable = detectAuthoredVariable(_inputRow);
    if (_variable == 0) _variable = 'x';

    numos::CalculusRequest request;
    request.operation = _calcMode == CalcMode::DERIVATIVE
        ? numos::CalculusOperation::Differentiate
        : numos::CalculusOperation::IntegrateIndefinite;
    request.expression = _serializedInput;
    request.variable.assign(1, _variable);

    // WHY: Giac runs before any native tutor work and remains the only source
    // consumed by buildResultDisplay().
    _giacResult =
        numos::GiacEngine::instance().evaluateCalculusStructured(request);
    if (_giacResult.ok() && !_giacResult.unevaluated) {
        try { runNativeTutor(request); }
        catch (...) {
            _casSteps.clear();
            _resultExpr = nullptr;
            _tutorStatus = TutorStatus::Unavailable;
            _tutorDiagnostic.clear(); // Do not allocate while handling tutor OOM.
        }
    }
    showResult();
}

void CalculusApp::runNativeTutor(const numos::CalculusRequest& request) {
    _arena.reset();
    _casSteps.clear();
    _resultExpr = nullptr;
    _integralFound = false;

    cas::ASTFlattener flattener;
    flattener.setArena(&_arena);
    cas::SymExpr* expr = flattener.flattenToExpr(_inputRow);
    if (!expr) {
        _tutorDiagnostic = "native tutor could not represent authored input";
        return;
    }
    _casSteps.logExpr("Original expression:", expr);
    if (_calcMode == CalcMode::DERIVATIVE) {
        computeDerivative(expr);
    } else {
        computeIntegral(expr);
        if (!_integralFound) _resultExpr = nullptr;
    }
    if (!_resultExpr) {
        _casSteps.clear();
        _tutorDiagnostic = "native tutor did not produce a final result";
        return;
    }

    NodePtr tutorAst = cas::SymExprToAST::convert(_resultExpr);
    std::string nativeSerialized;
    std::string serializerError;
    if (!tutorAst ||
        !numos::CalculationEngine::serializeForGiac(
            tutorAst.get(), nativeSerialized, serializerError)) {
        _casSteps.clear();
        _tutorDiagnostic = serializerError.empty()
            ? "native tutor result cannot be verified structurally"
            : serializerError;
        return;
    }

    numos::CalculusTutorVerification verification =
        numos::GiacEngine::instance().verifyCalculusTutor(
            request, nativeSerialized);
#ifdef NATIVE_SIM
    if (_debugForceTutorDisagreement) {
        verification.agreed = false;
        verification.diagnostic = "forced native tutor disagreement";
    }
#endif
    _tutorDiagnostic = verification.diagnostic;
    if (verification.agreed) {
        _tutorStatus = TutorStatus::Agreed;
    } else {
        // WHY: fail closed. Native expressions and their steps never enter the
        // result path and are discarded whenever verification is unavailable.
        _tutorStatus = TutorStatus::Unavailable;
        _casSteps.clear();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Compute derivative
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::computeDerivative(cas::SymExpr* expr) {
    {
        char varBuf[48];
        snprintf(varBuf, sizeof(varBuf),
                 "Differentiating with respect to '%c'", _variable);
        _casSteps.logNote(varBuf);
    }

    _casSteps.logNote("Applying differentiation rules");

    cas::SymExpr* rawDeriv = cas::SymDiff::diff(expr, _variable, _arena);

    if (!rawDeriv) {
        _casSteps.logNote("Error: differentiation not supported for this expression");
        _resultExpr = nullptr;
        return;
    }

    // Render unsimplified derivative via MathCanvas (no toString)
    _casSteps.logExpr("Unsimplified derivative:", rawDeriv);

    _casSteps.logNote("Simplifying result");

    cas::SymExpr* simplified = cas::SymSimplify::simplify(rawDeriv, _arena);
    _resultExpr = simplified ? simplified : rawDeriv;

    if (_resultExpr->isPolynomial()) {
        _casSteps.logNote("Result: polynomial expression");
    } else {
        _casSteps.logNote("Result: transcendental expression");
    }

    // Render final result via MathCanvas (no toString)
    {
        char label[32];
        snprintf(label, sizeof(label), "f'(%c) =", _variable);
        _casSteps.logExpr(label, _resultExpr);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Compute integral
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::computeIntegral(cas::SymExpr* expr) {
    {
        char varBuf[48];
        snprintf(varBuf, sizeof(varBuf),
                 "Integrating with respect to '%c'", _variable);
        _casSteps.logNote(varBuf);
    }

    _casSteps.logNote("Searching for symbolic antiderivative");

    cas::SymExpr* antideriv = cas::SymIntegrate::integrate(expr, _variable, _arena);

    if (antideriv) {
        _integralFound = true;
        _resultExpr = antideriv;

        // Render antiderivative via MathCanvas (no toString)
        _casSteps.logExpr("Antiderivative found:", antideriv);

        if (_resultExpr->isPolynomial()) {
            _casSteps.logNote("Result: polynomial expression");
        } else {
            _casSteps.logNote("Result: transcendental expression");
        }
    } else {
        _integralFound = false;
        _resultExpr = expr;

        _casSteps.logNote("No closed-form antiderivative found");
        _casSteps.logNote("Displaying unevaluated integral");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Build result display
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::buildResultDisplay() {
    // Clear previous
    _resultCanvas.stopCursorBlink();
    lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    if (_resultFallback)
        lv_obj_add_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);
    _resultCanvas.setExpression(nullptr, nullptr);
    _resultCanvas.resetScroll();
    lv_obj_scroll_to_y(_resultViewport, 0, LV_ANIM_OFF);
    _resultNode.reset();
    _resultRow = nullptr;
    _resultKind = ResultKind::None;

    _originalCanvas.stopCursorBlink();
    lv_obj_add_flag(_originalCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    _originalCanvas.setExpression(nullptr, nullptr);
    _originalCanvas.resetScroll();
    _originalNode.reset();
    _originalRow = nullptr;

    // Show the authored expression independently of result success.
    if (_inputRow) {
        _originalNode = cloneNode(_inputRow);
        _originalRow = static_cast<NodeRow*>(_originalNode.get());
        char fLabel[16];
        snprintf(fLabel, sizeof(fLabel), "f(%c)", _variable);
        lv_label_set_text(_originalLabel, fLabel);
        lv_obj_remove_flag(_originalCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
        _originalCanvas.setExpression(_originalRow, nullptr);
        _originalRow->calculateLayout(_originalCanvas.normalMetrics());
        _originalCanvas.invalidate();
        int previewH = mathObjectHeightPx(_originalRow->layout(), _originalCanvas.normalMetrics(), 8);
        previewH = std::max(48, std::min(72, previewH));
        lv_obj_set_height(_originalViewport, previewH);
        lv_obj_set_height(_originalCanvas.obj(), previewH);
        lv_obj_set_y(_resultSeparator, previewH + 3);
        lv_obj_set_y(_resultTitle, previewH + 8);
        lv_obj_set_y(_resultViewport, previewH + 28);
        lv_obj_set_height(_resultViewport, 150 - previewH - 28);
    }

    lv_obj_set_style_text_color(_resultTitle, lv_color_hex(accentColor()), 0);
    char title[40];
    snprintf(title, sizeof(title), _calcMode == CalcMode::DERIVATIVE
        ? "Derivative  f'(%c)" : "Antiderivative  F(%c)", _variable);
    lv_label_set_text(_resultTitle, _giacResult.unevaluated
        ? (_calcMode == CalcMode::INTEGRAL ? "Integral unevaluated" : "Derivative unevaluated") : title);
    lv_label_set_text(_resultHint, _tutorStatus == TutorStatus::Agreed
        ? "EXE Edit    AC Clear    TOOLS Steps"
        : "EXE Edit    AC Clear    SHIFT 1/2 Mode");

    if (!_giacResult.ok()) {
        const char* title = "Calculation failed";
        const char* message = "Try a simpler expression.";
        switch (_giacResult.status) {
            case numos::MathEngineStatus::ParseError:
                title = "Syntax error"; message = "Complete the expression, then press EXE."; break;
            case numos::MathEngineStatus::Undefined:
                title = "Undefined"; message = "Check the expression's domain."; break;
            case numos::MathEngineStatus::Unsupported:
                title = "Input limit"; message = "Use a shorter or simpler expression."; break;
            case numos::MathEngineStatus::OutOfMemory:
                title = "Memory full"; message = "Clear the input and try again."; break;
            default: break;
        }
        lv_label_set_text(_resultTitle, title);
        lv_label_set_text(_resultFallback, message);
        lv_obj_remove_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (_giacResult.hasTree) {
        _resultNode =
            numos::CalculationEngine::resultTreeToAST(_giacResult.tree);
        if (_resultNode && _calcMode == CalcMode::INTEGRAL && !_giacResult.unevaluated) {
            // Product policy: Giac supplies the authoritative primitive;
            // NumOS presents the general antiderivative by appending + C.
            auto* row = static_cast<NodeRow*>(_resultNode.get());
            row->appendChild(makeOperator(OpKind::Add));
            row->appendChild(makeVariable('C'));
        }
    }
    if (_resultNode) {
        _resultRow = static_cast<NodeRow*>(_resultNode.get());
        lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
        _resultCanvas.setExpression(_resultRow, nullptr);
        _resultRow->calculateLayout(_resultCanvas.normalMetrics());
        const int height = mathObjectHeightPx(_resultRow->layout(), _resultCanvas.normalMetrics(), 8);
        const int viewportH = lv_obj_get_height(_resultViewport);
        lv_obj_set_height(_resultCanvas.obj(), std::max(height, viewportH - 2));
        _resultCanvas.invalidate();
        if (height > lv_obj_get_height(_resultViewport) || _resultRow->layout().width > SCREEN_W - 36)
            lv_label_set_text(_resultHint, LV_SYMBOL_LEFT " " LV_SYMBOL_RIGHT " " LV_SYMBOL_UP " " LV_SYMBOL_DOWN " Scroll    EXE Edit    AC Clear");
        _resultKind = ResultKind::Structured;
        return;
    }

    // Valid Giac values whose shape is unsupported by MathAST remain exact.
    std::string visible = "Exact: ";
    visible += _giacResult.exactText;
    if (_calcMode == CalcMode::INTEGRAL && !_giacResult.unevaluated) visible += " + C";
    lv_label_set_text(_resultFallback, visible.c_str());
    lv_obj_remove_flag(_resultFallback, LV_OBJ_FLAG_HIDDEN);
    _resultKind = ResultKind::TextFallback;
}

// ════════════════════════════════════════════════════════════════════════════
// Build steps display
// ════════════════════════════════════════════════════════════════════════════

void CalculusApp::buildStepsDisplay() {
    // ── 1. Clean up ────────────────────────────────────────────────
    _stepRenderers.clear();
    lv_obj_clean(_stepsContainer);

    if (_tutorStatus != TutorStatus::Agreed) {
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        std::string message = "Steps unavailable.";
        if (!_tutorDiagnostic.empty()) {
            message += "\n";
            message += _tutorDiagnostic;
        }
        lv_label_set_text(lbl, message.c_str());
        lv_obj_set_width(lbl, SCREEN_W - 2 * PAD - 8);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(
            lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    const auto& steps = _casSteps.steps();

    if (steps.empty()) {
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        lv_label_set_text(lbl, "No steps available.");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_HINT_HEX), LV_PART_MAIN);
        return;
    }

    static constexpr int CANVAS_W = SCREEN_W - 2 * PAD - 16;

    // Helper: create a MathCanvas from a NodePtr
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
        int16_t h = mathObjectHeightPx(row->layout(), srd->canvas.normalMetrics(), 8);
        if (w > CANVAS_W) w = CANVAS_W;
        if (h < 22) h = 22;
        lv_obj_set_size(srd->canvas.obj(), w, h);
        srd->canvas.invalidate();
        _stepRenderers.push_back(std::move(srd));
    };

    // ── 2. Iterate all steps ───────────────────────────────────────
    size_t displayIndex = 0;
    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];
        // Only the native final candidate was verified. Do not turn arbitrary
        // intermediate tutor snapshots into apparently authoritative formulas.
        // The original and final formula reuse the authored/Giac MathASTs.
        if (step.mathExpr && i != 0 && step.mathExpr != _resultExpr) continue;
        ++displayIndex;

        // Text description label (skip if empty)
        if (!step.description.empty()) {
            char buf[200];
            snprintf(buf, sizeof(buf), "%d. %s", (int)displayIndex,
                     step.description.c_str());

            lv_obj_t* descLbl = lv_label_create(_stepsContainer);
            lv_label_set_text(descLbl, buf);
            lv_obj_set_width(descLbl, SCREEN_W - 2 * PAD - 8);
            lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12,
                                       LV_PART_MAIN);

            // Smart Highlighter: use accent colour when a sub-expression
            // was specifically modified in this step.
            uint32_t descColor = step.highlightExpr
                                 ? 0x1565C0   // LV_PALETTE_BLUE — modified sub-expression
                                 : COL_DESC_HEX;
            lv_obj_set_style_text_color(descLbl, lv_color_hex(descColor),
                                        LV_PART_MAIN);
        }

        // MathCanvas for CAS mathExpr — mandatory if present
        if (step.mathExpr) {
            emitCanvas(cloneNode(i == 0 ? _inputRow : _resultRow));
        }

        // Snapshot fallback as MathCanvas (for steps without mathExpr)
        if (!step.mathExpr && (step.kind == cas::StepKind::Transform ||
                               step.kind == cas::StepKind::Result)) {
            std::string eqText = step.snapshot.toString();
            if (!eqText.empty() && eqText != "0") {
                vpam::NodePtr snapNode =
                    cas::SymToAST::fromSymEquation(step.snapshot);
                emitCanvas(std::move(snapNode));
            }
        }
    }

    // Footer hint
    lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
    lv_label_set_text(hintLbl,
                      LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    AC: Back");
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(COL_HINT_HEX),
                                LV_PART_MAIN);
}

