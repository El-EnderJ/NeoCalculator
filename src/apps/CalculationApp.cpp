#include "ui/Toolbox.h"
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
 * CalculationApp.cpp — Calculadora V.P.A.M. con LVGL 9.5
 *
 * Integración completa de las cuatro fases del motor matemático:
 *   Fase 1: AST dinámico (MathAST) ✓
 *   Fase 2: Cursor + Inserción VPAM (CursorController) ✓
 *   Fase 3: Renderizado LVGL pixel-perfect (MathCanvas) ✓
 *   Fase 4: Autoridad Giac + presentación exacta/decimal ✓
 *
 * La pantalla es 100% LVGL (no TFT directo). Dos widgets MathCanvas:
 *   · Expresión (arriba): editable, con cursor parpadeante
 *   · Resultado (abajo):  solo lectura, muestra tras ENTER
 *
 * S⇔D (FREE_EQ): alterna entre resultado exacto (fracción/radical)
 * y decimal (double formateado).
 */

#include "CalculationApp.h"
#include "../math/tutor/Locale.h"
#include "../math/AngleModeRuntime.h"
#include "../input/KeyCodes.h"
#include "../input/generated/ProductionKeypadMap.generated.h"
#include "../Config.h"
#include <cmath>
#include <cstdlib>
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/SymExprToAST.h"
#include "../ui/MathTypography.h"
#include "../ui/TutorFonts.h"
#include "../ui/UnitQuickChoices.h"
#include "../math/ScientificReporting.h"
#include "../utils/HwUxProbe.h"
#ifdef NATIVE_SIM
  #include <cstdio>
#endif

// ── Colores ──
static constexpr uint32_t COL_BG_HEX     = 0xFFFFFF;   // Blanco puro
static constexpr uint32_t COL_SEP_HEX    = 0x333333;   // Gris separador

// ── Dimensiones ──
static constexpr int SCREEN_W      = 320;
static constexpr int SCREEN_H      = 240;
static constexpr int BAR_H         = ui::StatusBar::HEIGHT + 1;  // 24 + 1 separator
static constexpr int PAD           = 6;     // Safety margin on all edges
static constexpr int CONTENT_TOP   = BAR_H;                     // y = 25: top of content area
static constexpr int CONTENT_BOT   = SCREEN_H - PAD;            // y = 234: bottom of content area
static constexpr int CONTENT_FULL_H = CONTENT_BOT - CONTENT_TOP; // 209 px: full edit-mode height
static constexpr int CONTENT_W     = SCREEN_W - 2 * PAD;        // 308 px: usable content width
static constexpr int SEP_THICK     = 1;   // Separator line height (px)
static constexpr int SEP_GAP       = 4;   // Gap on each side of the separator (band→sep and sep→band)
static constexpr int BAND_MIN_H    = 8;   // Absolute minimum height for each result-mode band

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

struct UnitOutputMenu {
    ui::UnitQuickChoices choices;
    uint32_t generation=0;
    std::array<vpam::MathCanvas,3> canvases;
    std::array<vpam::NodePtr,3> previews;
    std::array<uint8_t,3> previewModes{}; // compact, stacked, name-only
    std::array<int,3> rows{{-1,-1,-1}};
    ~UnitOutputMenu(){for(auto& canvas:canvases)canvas.destroy();}
};

CalculationApp::CalculationApp()
    : _screen(nullptr)
    , _resultSep(nullptr)
    , _rootRow(nullptr)
    , _hasResult(false)
    , _showDecimal(false)
    , _resultMode(numos::CalculationFormat::Standard)
    , _resultRow(nullptr)
    , _historyIndex(-1)
    , _hasEduSteps(false)
    , _stepViewerActive(false)
    , _stepsContainer(nullptr)
{
}

CalculationApp::~CalculationApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin() — Crea pantalla LVGL, widgets y AST inicial
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::begin() {
    if (_screen) return;   // Ya creada

    createUI();
    resetExpression();
}

// ════════════════════════════════════════════════════════════════════════════
// end() — Destruye la pantalla LVGL y limpia
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::end() {
    ui::toolbox::closeOwner(this);
    closeFormatMenu();
    closeStepViewer();

    _mathCanvas.stopCursorBlink();
    _mathCanvas.destroy();
    _resultCanvas.destroy();
    _statusBar.destroy();

    if (_screen) {
        lv_obj_delete(_screen);
        _screen      = nullptr;
        _resultSep   = nullptr;
        _stepsContainer = nullptr;
        _resultTextLabel = nullptr;   // child of _screen, deleted with it
    }
    _structuredResult.reset();

    ++_resultGeneration;_quantity.reset();_quantityView.reset();_quantityError=numos::quantity::Error::None;

    _rootNode.reset();
    _rootRow = nullptr;
    _resultNode.reset();
    _resultRow = nullptr;
    _hasResult = false;

    _eduStepLogger.clear();
    _eduArena.reset();
    _hasEduSteps = false;
}

// ════════════════════════════════════════════════════════════════════════════
// load() — Hace visible la pantalla de la calculadora
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _mathCanvas.startCursorBlink();
    _statusBar.update();
    refreshExpression();
}

// ════════════════════════════════════════════════════════════════════════════
// createUI() — Construye la jerarquía de widgets LVGL
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::createUI() {
    ui::initMathTypography();

    // ── Pantalla ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── StatusBar global (24 px + 1 px separador) ──
    _statusBar.create(_screen);
    _statusBar.setTitle("Calculation");
    _statusBar.setBatteryLevel(100);

    // ── MathCanvas — Expression (full-area, vertically centered in edit mode) ──
    _mathCanvas.create(_screen);
    _mathCanvas.setEmptyRootPlaceholderVisible(false);
    _mathCanvas.setAutoHeightEnabled(false);
    _mathCanvas.setTraceLabel("calc_input_edit");
    // LaTeX inline look: top-level atoms use TEXT style so fractions step their
    // numerator/denominator down to SCRIPT style (compact inline rendering).
    _mathCanvas.setMathStyle(vpam::MathStyle::TEXT);
    lv_obj_set_pos(_mathCanvas.obj(), PAD, CONTENT_TOP);
    lv_obj_set_size(_mathCanvas.obj(), CONTENT_W, CONTENT_FULL_H);
    lv_obj_add_style(_mathCanvas.obj(), &ui::style_math_primary, LV_PART_MAIN);

    // ── Separator line expr↔result (#333) — initially hidden ──
    _resultSep = lv_obj_create(_screen);
    lv_obj_set_size(_resultSep, CONTENT_W, 1);
    lv_obj_set_style_bg_color(_resultSep, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_resultSep, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultSep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_resultSep, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_resultSep, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);

    // ── MathCanvas — Result (fills remaining area when visible) ──
    _resultCanvas.create(_screen);
    _resultCanvas.setAutoHeightEnabled(false);
    _resultCanvas.setTraceLabel("calc_result");
    _resultCanvas.setMathStyle(vpam::MathStyle::TEXT);
    lv_obj_set_pos(_resultCanvas.obj(), PAD, CONTENT_TOP);
    lv_obj_set_size(_resultCanvas.obj(), CONTENT_W, CONTENT_FULL_H);
    lv_obj_add_style(_resultCanvas.obj(), &ui::style_math_primary, LV_PART_MAIN);
    lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
}

// ════════════════════════════════════════════════════════════════════════════
// resetExpression() — Crea un AST vacío y conecta al MathCanvas
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::resetExpression() {
    _rootNode = vpam::makeRow();
    _rootRow  = static_cast<vpam::NodeRow*>(_rootNode.get());

    // Insertar un NodeEmpty inicial para que el usuario sepa dónde escribir
    _rootRow->appendChild(vpam::makeEmpty());

    // Inicializar el cursor al inicio de la fila raíz
    _cursor.init(_rootRow);

    // Conectar al canvas
    _mathCanvas.setExpression(_rootRow, &_cursor);
    refreshExpression();
}

// ════════════════════════════════════════════════════════════════════════════
// refreshExpression() — Recalcula layout e invalida el canvas
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::refreshExpression() {
    if (!_rootRow) return;
    _rootRow->calculateLayout(_mathCanvas.normalMetrics());
    _mathCanvas.invalidate();
}

// ════════════════════════════════════════════════════════════════════════════
// handleKey() — Traduce KeyCode a operaciones VPAM
//
// Usa KeyboardManager para gestionar SHIFT/ALPHA/LOCK/STO.
// En modo ALPHA, las teclas numéricas insertan variables (A-F).
// En modo STO, las teclas de variable ejecutan un guardado.
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::handleKey(const KeyEvent& ev) {
    if (ui::toolbox::handle(ev)) return;
    if(_formatSuppressed!=KeyCode::NONE) {
        if(ev.code==_formatSuppressed && ev.action==KeyAction::RELEASE){_formatSuppressed=KeyCode::NONE;return;}
        if(ev.code==_formatSuppressed && ev.action==KeyAction::REPEAT)return;
        if(ev.action==KeyAction::PRESS)_formatSuppressed=KeyCode::NONE;
    }
#ifdef NATIVE_SIM
    std::printf("[CALCAPP] handleKey: code=%d action=%d\n",
                static_cast<int>(ev.code), static_cast<int>(ev.action));
#endif
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // ── Step viewer active: intercept keys for scroll/back ──────────────
    if (_stepViewerActive) {
        switch (ev.code) {
            case KeyCode::UP:
                if (_stepsContainer)
                    lv_obj_scroll_by(_stepsContainer, 0, 30, LV_ANIM_ON);
                break;
            case KeyCode::DOWN:
                if (_stepsContainer)
                    lv_obj_scroll_by(_stepsContainer, 0, -30, LV_ANIM_ON);
                break;
            case KeyCode::AC:
            case KeyCode::DEL:
            case KeyCode::F2:
                closeStepViewer();
                break;
            default:
                break;
        }
        return;
    }

    if (ev.code == KeyCode::TOOLBOX && !_formatMenu && ev.action == KeyAction::PRESS) {
        ui::toolbox::open(_screen, {this, &_cursor, numos::toolbox::Calculation, [](void* owner) {
            auto* self=static_cast<CalculationApp*>(owner);
            self->clearResult(); self->refreshExpression();
            self->_mathCanvas.resetCursorBlink();
        }, _mathCanvas.normalMetrics().style});
        return;
    }
    auto& km = vpam::KeyboardManager::instance();

    const auto semantic =
        static_cast<numos::input::SemanticId>(ev.semanticId);
    if (_formatMenu) {
        if (ev.code == KeyCode::UP && _formatChoice) --_formatChoice;
        else if (ev.code == KeyCode::DOWN && _formatChoice + 1 < _formatChoiceCount) ++_formatChoice;
        else if ((ev.code == KeyCode::ENTER || ev.code==KeyCode::EXE) && ev.action==KeyAction::PRESS) {
            _formatSuppressed=ev.code;
            applyFormatChoice();
            return;
        } else if (ev.code == KeyCode::AC || ev.code == KeyCode::DEL ||
                   ev.code == KeyCode::FORMAT || ev.code == KeyCode::FORMAT_MENU) {
            closeFormatMenu();
            return;
        }
        updateFormatMenu();
        return;
    }
    if (ev.code == KeyCode::FORMAT_MENU ||
        ((ev.code == KeyCode::FORMAT || ev.code == KeyCode::FREE_EQ) && km.isShift() && km.isAlpha())) {
        km.consumeModifier();
        openFormatMenu();
        _statusBar.update();
        return;
    }
    if (semantic == numos::input::SemanticId::engineering_notation ||
        (ev.code == KeyCode::EXP && km.isShift() && !km.isAlpha())) {
        km.consumeModifier();
        engineeringFormat();
        _statusBar.update();
        return;
    }
    if (_hasResult && _resultMode == numos::CalculationFormat::Engineering &&
        !km.isShift() && (ev.code == KeyCode::LEFT || ev.code == KeyCode::RIGHT)) {
        engineeringFormat(ev.code == KeyCode::LEFT ? -1 : 1);
        return;
    }

    if (semantic == numos::input::SemanticId::left_bracket ||
        semantic == numos::input::SemanticId::right_bracket) {
        clearResult();
        if (semantic == numos::input::SemanticId::left_bracket)
            _cursor.insertParen(vpam::DelimKind::Bracket);
        else _cursor.closeBracket();
        _statusBar.update();
        _mathCanvas.resetCursorBlink();
        refreshExpression();
        return;
    }
    if (semantic == numos::input::SemanticId::pow10) {
        if (_cursor.insertPowerOfTen()) clearResult();
        _statusBar.update();
        _mathCanvas.resetCursorBlink();
        refreshExpression();
        return;
    }
    if (semantic >= numos::input::SemanticId::alpha_A &&
        semantic <= numos::input::SemanticId::alpha_Z) {
        const char variable = static_cast<char>(
            'A' + static_cast<int>(semantic) -
            static_cast<int>(numos::input::SemanticId::alpha_A));
        clearResult();
        _cursor.insertVariable(variable);
        _statusBar.update();
        _mathCanvas.resetCursorBlink();
        refreshExpression();
        return;
    }
    if (semantic == numos::input::SemanticId::asin ||
        semantic == numos::input::SemanticId::acos ||
        semantic == numos::input::SemanticId::atan) {
        clearResult();
        const auto function =
            semantic == numos::input::SemanticId::asin
                ? vpam::FuncKind::ArcSin
                : semantic == numos::input::SemanticId::acos
                    ? vpam::FuncKind::ArcCos
                    : vpam::FuncKind::ArcTan;
        _cursor.insertFunction(function);
        _statusBar.update();
        _mathCanvas.resetCursorBlink();
        refreshExpression();
        return;
    }

    // ── Modificadores: SHIFT / ALPHA / STO ──────────────────────────────
    if (ev.code == KeyCode::SHIFT) {
        km.pressShift();
        _statusBar.update();
        return;
    }
    if (ev.code == KeyCode::ALPHA) {
        km.pressAlpha();
        _statusBar.update();
        return;
    }
    if (ev.code == KeyCode::STO) {
        km.pressStore();
        _statusBar.update();
        return;
    }

    // ── Modo STO: esperando tecla de variable para guardar ──────────────
    if (km.isStore()) {
        char varName = alphaKeyToVarName(ev.code);
        if (varName != '\0') {
            executeStore(varName);
            km.reset();
            _statusBar.update();
            return;
        }
        // Cualquier otra tecla cancela STO
        km.reset();
        _statusBar.update();
        // Continuar procesando la tecla normalmente
    }

    // ── Modo ALPHA: teclas numéricas → variables ────────────────────────
    if (km.isAlpha()) {
        char varName = alphaKeyToVarName(ev.code);
        if (varName != '\0') {
            clearResult();
            _cursor.insertVariable(varName);
            km.consumeModifier();
            _statusBar.update();
            _mathCanvas.resetCursorBlink();
            refreshExpression();
            return;
        }
        // Si no es una tecla mapeada a variable, consuma el modifier y siga
        km.consumeModifier();
        _statusBar.update();
    }

    // ── SHIFT + combos que generan key codes virtuales ──────────────────
    KeyCode effectiveCode = ev.code;
    if (km.isShift() && ev.code == KeyCode::TABLE) {
        effectiveCode = KeyCode::FACT;
        km.consumeModifier();
    }

    bool changed = true;

    switch (effectiveCode) {
        // ── Step viewer (F2) ──
        case KeyCode::F2:
            if (_stepViewerActive) {
                closeStepViewer();
            } else if (_hasEduSteps && _hasResult) {
                openStepViewer();
            }
            changed = false;
            break;

        // ── Dígitos ──
        case KeyCode::NUM_0: clearResult(); _cursor.insertDigit('0'); break;
        case KeyCode::NUM_1: clearResult(); _cursor.insertDigit('1'); break;
        case KeyCode::NUM_2: clearResult(); _cursor.insertDigit('2'); break;
        case KeyCode::NUM_3: clearResult(); _cursor.insertDigit('3'); break;
        case KeyCode::NUM_4: clearResult(); _cursor.insertDigit('4'); break;
        case KeyCode::NUM_5: clearResult(); _cursor.insertDigit('5'); break;
        case KeyCode::NUM_6: clearResult(); _cursor.insertDigit('6'); break;
        case KeyCode::NUM_7: clearResult(); _cursor.insertDigit('7'); break;
        case KeyCode::NUM_8: clearResult(); _cursor.insertDigit('8'); break;
        case KeyCode::NUM_9: clearResult(); _cursor.insertDigit('9'); break;
        // Punto decimal: la inserción correcta vive ahora en el guard compartido
        // de CursorController::insertDigit (Phase 8E). Se revierte el workaround
        // local de Phase 8D — ya no se necesita.
        case KeyCode::DOT:   clearResult(); _cursor.insertDigit('.'); break;

        // ── Operadores ──
        case KeyCode::ADD: clearResult(); _cursor.insertOperator(vpam::OpKind::Add); break;
        case KeyCode::NEG:
        case KeyCode::NEGATE:
        case KeyCode::SUB: clearResult(); _cursor.insertOperator(vpam::OpKind::Sub); break;
        case KeyCode::MUL: clearResult(); _cursor.insertOperator(vpam::OpKind::Mul); break;
        case KeyCode::DIVIDE: clearResult(); _cursor.insertOperator(vpam::OpKind::Div); break;

        // ── Estructuras VPAM ──
        case KeyCode::DIV:
        case KeyCode::FRAC:   clearResult(); _cursor.insertFraction(); break;
        case KeyCode::POW:    clearResult(); _cursor.insertPower();    break;
        case KeyCode::SQUARE:
            clearResult();
            _cursor.insertPower();
            _cursor.insertDigit('2');
            break;
        case KeyCode::SQRT:   clearResult(); _cursor.insertRoot();     break;
        case KeyCode::LPAREN:
            clearResult();
            _cursor.insertParen(km.isShift() ? vpam::DelimKind::Bracket : vpam::DelimKind::Paren);
            if (km.isShift()) km.consumeModifier();
            break;
        case KeyCode::LBRACKET: clearResult(); _cursor.insertParen(vpam::DelimKind::Bracket); break;
        case KeyCode::RBRACKET: clearResult(); _cursor.closeBracket(); break;
        case KeyCode::RPAREN:
            clearResult();
            if (km.isShift()) { _cursor.closeBracket(); km.consumeModifier(); }
            else _cursor.moveRight();
            break;
        case KeyCode::COMMA:  clearResult(); _cursor.insertVariable(','); break;
        case KeyCode::EQUAL:
            clearResult();
            _cursor.insertOperator(vpam::OpKind::Eq);
            break;
        case KeyCode::EXP:
            if (_cursor.insertPowerOfTen()) clearResult();
            break;

        // ── Funciones trigonométricas / logarítmicas ──
        case KeyCode::SIN:
            clearResult();
            if (km.isShift()) {
                _cursor.insertFunction(vpam::FuncKind::ArcSin);
                km.consumeModifier();
            } else {
                _cursor.insertFunction(vpam::FuncKind::Sin);
            }
            break;
        case KeyCode::COS:
            clearResult();
            if (km.isShift()) {
                _cursor.insertFunction(vpam::FuncKind::ArcCos);
                km.consumeModifier();
            } else {
                _cursor.insertFunction(vpam::FuncKind::Cos);
            }
            break;
        case KeyCode::TAN:
            clearResult();
            if (km.isShift()) {
                _cursor.insertFunction(vpam::FuncKind::ArcTan);
                km.consumeModifier();
            } else {
                _cursor.insertFunction(vpam::FuncKind::Tan);
            }
            break;

        // ── Logaritmos ──
        case KeyCode::LN:
            clearResult();
            _cursor.insertFunction(vpam::FuncKind::Ln);
            break;
        case KeyCode::LOG:
            if (km.isShift()) {
                if (_cursor.insertPowerOfTen()) clearResult();
                km.consumeModifier();
            } else {
                clearResult();
                _cursor.insertFunction(vpam::FuncKind::Log);
            }
            break;
        case KeyCode::LOG_BASE:
            clearResult();
            _cursor.insertLogBase();
            break;

        // ── Constantes algebraicas ──
        case KeyCode::CONST_PI:
            clearResult();
            _cursor.insertConstant(vpam::ConstKind::Pi);
            break;
        case KeyCode::CONST_E:
            clearResult();
            _cursor.insertConstant(vpam::ConstKind::E);
            break;

        // ── Variables directas (teclado físico o serial) ──
        case KeyCode::VAR_X:
            clearResult();
            _cursor.insertVariable('x');
            break;
        case KeyCode::VAR_Y:
            clearResult();
            _cursor.insertVariable('y');
            break;
        case KeyCode::ANS:
            clearResult();
            _cursor.insertVariable(vpam::VAR_ANS);
            break;
        case KeyCode::PREANS:
            clearResult();
            _cursor.insertVariable(vpam::VAR_PREANS);
            break;

        // ── Variables Alpha (via serial o mapeo directo) ──
        case KeyCode::ALPHA_A: clearResult(); _cursor.insertVariable('A'); break;
        case KeyCode::ALPHA_B: clearResult(); _cursor.insertVariable('B'); break;
        case KeyCode::ALPHA_C: clearResult(); _cursor.insertVariable('C'); break;
        case KeyCode::ALPHA_D: clearResult(); _cursor.insertVariable('D'); break;
        case KeyCode::ALPHA_E: clearResult(); _cursor.insertVariable('E'); break;
        case KeyCode::ALPHA_F: clearResult(); _cursor.insertVariable('F'); break;

        // ── Navegación ──
        case KeyCode::LEFT:
            if (_hasResult && (km.isShift() || _resultCanvas.hasHorizontalOverflow())) {
                (km.isShift() ? _mathCanvas : _resultCanvas).scrollBounded(20);
                changed = false;
            } else {
                _cursor.moveLeft();
            }
            break;
        case KeyCode::RIGHT:
            if (_hasResult && (km.isShift() || _resultCanvas.hasHorizontalOverflow())) {
                (km.isShift() ? _mathCanvas : _resultCanvas).scrollBounded(-20);
                changed = false;
            } else {
                _cursor.moveRight();
            }
            break;
        case KeyCode::UP:
            if (_hasResult && km.isShift()) {
                // WHY: keep plain UP/DOWN for history. A tall result takes
                // precedence; otherwise expose the tall authored expression.
                (_resultCanvas.hasVerticalOverflow() ? _resultCanvas : _mathCanvas)
                    .scrollVerticalBounded(20);
                changed = false;
                break;
            }
            if (_hasResult || _historyIndex >= 0) {
                // Navegar historial: ir a la entrada anterior
                navigateHistory(-1);
                changed = false;
            } else {
                _cursor.moveUp();
            }
            break;
        case KeyCode::DOWN:
            if (_hasResult && km.isShift()) {
                (_resultCanvas.hasVerticalOverflow() ? _resultCanvas : _mathCanvas)
                    .scrollVerticalBounded(-20);
                changed = false;
                break;
            }
            if (_historyIndex >= 0) {
                // Navegar historial: ir a la entrada más reciente
                navigateHistory(1);
                changed = false;
            } else {
                _cursor.moveDown();
            }
            break;

        // ── Borrado ──
        case KeyCode::DEL: clearResult(); _cursor.backspace(); break;

        // ── AC: limpiar toda la expresión, resultado y modificador ──
        case KeyCode::AC:
            clearResult();
            resetExpression();
            _historyIndex = -1;
            km.reset();
            break;

        // ── ENTER: evaluar el AST ──
        case KeyCode::ENTER:
            evaluateExpression();
            changed = false;
            break;

        // ── S⇔D: alternar exacto / decimal ──
        case KeyCode::FREE_EQ:
        case KeyCode::FORMAT:
            if (_hasResult) {
                toggleSD();
            }
            changed = false;
            break;

        // ── FACT: factorización en primos ──
        case KeyCode::FACT:
            if (_hasResult) {
                clearResult();
                resetExpression();
                _cursor.insertVariable(vpam::VAR_ANS);
            }
            changed = _cursor.insertFactorial();
            break;

        default:
            changed = false;
            break;
    }

    // Consumir SHIFT simple después de cualquier acción
    km.consumeModifier();
    _statusBar.update();

    if (changed) {
        _mathCanvas.resetCursorBlink();
        refreshExpression();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// evaluateExpression() — Evalúa el AST y muestra el resultado
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::evaluateExpression() {
    if(!_rootRow)return;
    numos::HwUxProbe hwux("calculation","evaluate");
    auto& engine=numos::CalculationEngine::instance();
    auto ev=engine.evaluate(_rootRow);
#if defined(NATIVE_SIM) && !defined(__EMSCRIPTEN__)
    if(std::getenv("NUMOS_CALC_INPUT_TRACE"))std::printf("[CALC-EVAL] serialized=%s diagnostic=%s status=%d\n",ev.serialized.c_str(),ev.diagnostic.c_str(),int(ev.status));
#endif
    const bool spanish=numos::i18n::isSpanish(numos::i18n::productLocale);
    vpam::ExactVal value;
    bool numericMirror=false;
    std::unique_ptr<numos::quantity::Display> quantityView;
    numos::quantity::Descriptor output;
    HistoryEntry entry;
    vpam::NodePtr initialView;
#if defined(__cpp_exceptions)
    try {
#endif
        if(ev.ok()) {
            if(ev.quantity) value=vpam::ExactVal::makeError("Quantity");
            else if(ev.exactValValid)value=ev.exactVal;
            else {
                char* end=nullptr;const double d=std::strtod(ev.approximateText.c_str(),&end);
                numericMirror=!ev.approximateText.empty() && end && !*end && std::isfinite(d);
                value=numericMirror?vpam::ExactVal::fromDouble(d):vpam::ExactVal::makeError("Math ERROR");
            }
        } else {
            const char* text=ev.status==numos::MathEngineStatus::ParseError?"Syntax ERROR":
                ev.status==numos::MathEngineStatus::Unsupported?(spanish?"Limite de expresion":"Expression limit"):"Math ERROR";
            if(ev.quantityError!=numos::quantity::Error::None)text=numos::quantity::errorMessage(ev.quantityError,spanish);
            value=vpam::ExactVal::makeError(text);
        }
        if(ev.quantity) {
            output=numos::quantity::coherent(*ev.quantity);
            quantityView=std::make_unique<numos::quantity::Display>();
            if(numos::quantity::convert(*ev.quantity,output,*quantityView)!=numos::quantity::Error::None) {
                ui::StatusBar::showActiveNotice(spanish?"No se puede preparar el resultado":"Cannot prepare result");return;
            }
        }
        entry.exprAST=vpam::cloneNode(_rootRow);entry.result=value;entry.status=ev.status;
        entry.kind=ev.kind;entry.exactValValid=ev.exactValValid;entry.exactText=ev.exactText;entry.approxText=ev.approximateText;
        entry.angleResult=ev.ok() && !ev.quantity && numos::authoredAngle(_rootRow);
        entry.resultInDegrees=numos::angleModeIsDeg();entry.quantity=ev.quantity;entry.quantityError=ev.quantityError;
        if(ev.exactAST){entry.resultAST=vpam::cloneNode(ev.exactAST.get());initialView=vpam::cloneNode(ev.exactAST.get());}
        if(ev.approximateAST)entry.approximateAST=vpam::cloneNode(ev.approximateAST.get());
        entry.reusePolicy=ev.reusePolicy;entry.sToDPolicy=ev.sToDPolicy;entry.fallbackReason=ev.fallbackReason;
        if(_history.size()==_history.capacity())_history.reserve(std::min<size_t>(MAX_HISTORY+1,std::max<size_t>(1,_history.capacity()*2)));
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){ui::StatusBar::showActiveNotice(spanish?"Memoria insuficiente":"Not enough memory");return;}
#endif
    if(ev.ok() && ev.reusePolicy!=numos::ResultReusePolicy::NonReusable &&
       !engine.commitResultAns(ev,ev.exactValValid || numericMirror?&value:nullptr)) {
        ui::StatusBar::showActiveNotice(spanish?"No se pudo conservar Ans":"Could not preserve Ans");return;
    }
    ++_resultGeneration;
    _hasResult=true;_lastStatus=ev.status;_lastKind=ev.kind;_exactValValid=ev.exactValValid;
    _exactText=std::move(ev.exactText);_approxText=std::move(ev.approximateText);_lastResult=std::move(value);
    _structuredResult=std::move(ev.exactAST);_structuredApproxResult=std::move(ev.approximateAST);
    _reusePolicy=ev.reusePolicy;_sToDPolicy=ev.sToDPolicy;_fallbackReason=ev.fallbackReason;
    _angleResult=entry.angleResult;_resultInDegrees=entry.resultInDegrees;
    _phaseUnit=_resultInDegrees?numos::CalculationFormat::Degrees:numos::CalculationFormat::Radians;
    _quantity=std::move(ev.quantity);_quantityError=ev.quantityError;_quantityView=std::move(quantityView);_outputUnit=output;
    _resultMode=numos::CalculationFormat::Standard;_engineeringShift=0;_showDecimal=false;
    _history.push_back(std::move(entry));if(_history.size()>MAX_HISTORY)_history.erase(_history.begin());_historyIndex=-1;
    _mathCanvas.stopCursorBlink();_mathCanvas.setExpression(_rootRow,nullptr);
    _eduStepLogger.clear();_eduArena.reset();_hasEduSteps=false;
    if(setting_edu_steps && ev.ok() && !_quantity)generateEduSteps();
    showResult(std::move(initialView));
    const char* status="error";
    switch(_lastStatus) {
        case numos::MathEngineStatus::Ok:status="ok";break;
        case numos::MathEngineStatus::Undefined:status="undefined";break;
        case numos::MathEngineStatus::ParseError:status="parse_error";break;
        case numos::MathEngineStatus::EvaluationError:status="evaluation_error";break;
        case numos::MathEngineStatus::Unsupported:status="unsupported";break;
        case numos::MathEngineStatus::OutOfMemory:status="out_of_memory";break;
        case numos::MathEngineStatus::UnitsUnavailable:status="units_unavailable";break;
        case numos::MathEngineStatus::QuantityError:status="quantity_error";break;
    }
    const char* kind=_lastKind==numos::CalcResultKind::Structured?"structured":_lastKind==numos::CalcResultKind::TextFallback?"text_fallback":"none";
    hwux.finish(status,kind,"giac",numos::hwUxHash(_exactText.c_str()));
}

bool CalculationApp::navigateBack() {
    if (ui::toolbox::back()) return true;
    if (_formatMenu) {
        const bool components=_formatPickingComponents,uncertainty=_formatPickingUncertainty;closeFormatMenu();
        if(uncertainty){openFormatMenu();return true;}
        if(components){openFormatMenu(false,true);if(_unitMenu){_formatChoice=_unitMenu->choices.count+2;updateFormatMenu();}}
        return true;
    }
    if (!_stepViewerActive) return false;
    closeStepViewer();
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// applyResultLayout() — Dynamically trim expression, reveal separator, fill result
//
// Called after ENTER (evaluation) or FACT (factorization) to transition
// from full-screen Edit Mode to the split-screen Result Mode layout.
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::applyResultLayout() {
    // ── Content-proportional result-mode split ──────────────────────────────
    //
    // Available space for the two bands (separating overhead excluded):
    //   avail = CONTENT_FULL_H - SEP_THICK - 2*SEP_GAP
    //         = 209 - 1 - 8 = 200 px
    //
    // Invariants (always satisfied before any LVGL call):
    //   inH  >= BAND_MIN_H
    //   resH >= BAND_MIN_H
    //   inH + resH == avail  (so total == CONTENT_FULL_H)
    //
    // Centering is handled by the renderer: each MathCanvas centers its content
    // vertically using:  baseline = y1 + (widgetH + ascent - descent) / 2

    // Bare content heights (0 pad — centering is done by the renderer, not by padding).
    const int16_t cIn  = vpam::mathObjectHeightPx(
        _rootRow->layout(),   _mathCanvas.normalMetrics(),  0);
    const int16_t cRes = vpam::mathObjectHeightPx(
        _resultRow->layout(), _resultCanvas.normalMetrics(), 0);

    // Fixed overhead = separator + gaps on both sides.
    const int16_t overhead = static_cast<int16_t>(SEP_THICK + 2 * SEP_GAP);
    const int16_t avail    = static_cast<int16_t>(CONTENT_FULL_H - overhead);

    // Proportional split; handle every degenerate case with no branching surprise.
    int16_t inH, resH;
    bool overflowed = false;

    const int16_t combined = static_cast<int16_t>(cIn + cRes);
    if (combined <= 0) {
        // Both layouts are empty/invalid: give each band half.
        inH  = static_cast<int16_t>(avail / 2);
        resH = static_cast<int16_t>(avail - inH);
    } else {
        // Proportional assignment (integer multiply first to avoid precision loss).
        inH  = static_cast<int16_t>(static_cast<int32_t>(avail) * cIn / combined);
        resH = static_cast<int16_t>(avail - inH);

        if (combined > avail) {
            // Overflow: both bands are already shrunk proportionally — accept it.
            overflowed = true;
        } else {
            // Ensure each band is at least as tall as its content.
            // Adjustments are self-canceling: the sum stays == avail.
            if (inH < cIn) {
                inH  = cIn;
                resH = static_cast<int16_t>(avail - inH);
            } else if (resH < cRes) {
                resH = cRes;
                inH  = static_cast<int16_t>(avail - resH);
            }
        }
    }

    // Apply absolute minimums last.  Shrink the other band by the same amount
    // to preserve the invariant  inH + resH == avail.
    if (inH < BAND_MIN_H) {
        inH  = BAND_MIN_H;
        resH = static_cast<int16_t>(avail - inH);
    }
    if (resH < BAND_MIN_H) {
        resH = BAND_MIN_H;
        inH  = static_cast<int16_t>(avail - resH);
    }
    // Final safety: if avail itself is too small (extreme screen/content edge case)
    // force each to BAND_MIN_H and let LVGL clip; total may exceed avail but screen
    // hardware bounds remain valid.
    if (inH < BAND_MIN_H)  inH  = BAND_MIN_H;
    if (resH < BAND_MIN_H) resH = BAND_MIN_H;

    // ── Derive absolute y-coordinates ──────────────────────────────────────
    // Layout (top-to-bottom):
    //   [inY .. inY+inH)   : input  canvas
    //   [inY+inH .. sepY)  : SEP_GAP
    //   [sepY .. sepY+SEP_THICK) : separator
    //   [sepY+SEP_THICK .. resY) : SEP_GAP
    //   [resY .. resY+resH)      : result canvas
    //
    const int16_t inY  = static_cast<int16_t>(CONTENT_TOP);
    const int16_t sepY = static_cast<int16_t>(inY  + inH  + SEP_GAP);
    const int16_t resY = static_cast<int16_t>(sepY + SEP_THICK + SEP_GAP);

#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    Serial.printf(
        "[CALC-SPLIT] cIn=%d cRes=%d avail=%d inH=%d resH=%d "
        "inY=%d inBot=%d sepY=%d resY=%d resBot=%d overflow=%s\n",
        (int)cIn, (int)cRes, (int)avail, (int)inH, (int)resH,
        (int)inY,  (int)(inY  + inH),
        (int)sepY,
        (int)resY, (int)(resY + resH),
        overflowed ? "yes" : "no");
#else
    (void)overflowed;
#endif

    // ── Apply to LVGL objects ───────────────────────────────────────────────
    lv_obj_set_pos(_mathCanvas.obj(), PAD, inY);
    lv_obj_set_size(_mathCanvas.obj(), CONTENT_W, inH);

    lv_obj_set_pos(_resultSep, PAD, sepY);
    lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_pos(_resultCanvas.obj(), PAD, resY);
    lv_obj_set_size(_resultCanvas.obj(), CONTENT_W, resH);
    lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);

    _mathCanvas.setTraceLabel("calc_input_result_compact");
}

// ════════════════════════════════════════════════════════════════════════════
// GIAC-B01: presentación de texto plano (tier 3 — fallback honesto)
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::showTextResult(const std::string& text) {
    if (!_screen || !_rootRow) return;

    // No structured result AST in this tier.
    _resultNode.reset();
    _resultRow = nullptr;
    if (_resultCanvas.obj())
        lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);

    if (!_resultTextLabel) {
        _resultTextLabel = lv_label_create(_screen);
        // stix_math fonts have no space glyph — plain text must use the
        // default (montserrat) font.
        lv_obj_set_style_text_color(_resultTextLabel, lv_color_hex(0x1A1A1A),
                                    LV_PART_MAIN);
        lv_label_set_long_mode(_resultTextLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(_resultTextLabel, CONTENT_W);
    }
    // WHY: the unit notice is localized prose; retain ASCII geometry while
    // supplying the existing Spanish fallback for accented letters.
    lv_obj_set_style_text_font(_resultTextLabel,
        _quantityError != numos::quantity::Error::None
            ? ui::tutorFont14() : LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_label_set_text(_resultTextLabel, text.c_str());
    lv_obj_remove_flag(_resultTextLabel, LV_OBJ_FLAG_HIDDEN);

    // Same band geometry idea as applyResultLayout(), with a fixed-height
    // text band instead of a result canvas.
    const int16_t cIn = vpam::mathObjectHeightPx(
        _rootRow->layout(), _mathCanvas.normalMetrics(), 0);
    const int16_t overhead = static_cast<int16_t>(SEP_THICK + 2 * SEP_GAP);
    const int16_t avail    = static_cast<int16_t>(CONTENT_FULL_H - overhead);
    constexpr int16_t TEXT_BAND_H = 48;   // up to two wrapped lines

    int16_t inH = static_cast<int16_t>(avail - TEXT_BAND_H);
    if (inH > cIn) inH = cIn;
    if (inH < BAND_MIN_H) inH = BAND_MIN_H;

    const int16_t inY  = static_cast<int16_t>(CONTENT_TOP);
    const int16_t sepY = static_cast<int16_t>(inY + inH + SEP_GAP);
    const int16_t resY = static_cast<int16_t>(sepY + SEP_THICK + SEP_GAP);

    lv_obj_set_pos(_mathCanvas.obj(), PAD, inY);
    lv_obj_set_size(_mathCanvas.obj(), CONTENT_W, inH);

    lv_obj_set_pos(_resultSep, PAD, sepY);
    lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_pos(_resultTextLabel, PAD, resY);

    _mathCanvas.setTraceLabel("calc_input_result_compact");
    _mathCanvas.invalidate();

    if (_hasEduSteps) {
        _statusBar.setTitle("F2: View Steps");
    }
}

void CalculationApp::hideTextResult() {
    if (_resultTextLabel)
        lv_obj_add_flag(_resultTextLabel, LV_OBJ_FLAG_HIDDEN);
}

// ════════════════════════════════════════════════════════════════════════════
// showResult() — Genera y muestra el AST del resultado (3 estados)
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::showResult(vpam::NodePtr prepared) {
    if (_rootRow) {
        // WHY: failed input is still an editor expression. Keep its pending
        // slots visible after EXE; a read-only canvas would hide that evidence.
        _mathCanvas.setExpression(_rootRow,
            _lastStatus == numos::MathEngineStatus::Ok ? nullptr : &_cursor);
        _mathCanvas.stopCursorBlink();
    }

    hideTextResult();
    if(_quantityError!=numos::quantity::Error::None) {
        showTextResult(numos::quantity::errorMessage(_quantityError,numos::i18n::isSpanish(numos::i18n::productLocale)));return;
    }

    if (_lastStatus == numos::MathEngineStatus::Ok &&
        _lastKind == numos::CalcResultKind::TextFallback &&
        _resultMode == numos::CalculationFormat::Standard) {
        // Tier 3: Giac's own printed result as plain text — honest fallback,
        // never a legacy re-evaluation.
        std::string visible = "Giac text fallback (";
        visible += numos::engineFallbackReasonName(_fallbackReason);
        visible += "): ";
        visible += _exactText;
        showTextResult(visible);
        return;
    }

    _resultNode = prepared ? std::move(prepared) : formattedResult();
    if (!_resultNode) {
        _resultMode = numos::CalculationFormat::Standard;
        _resultNode = _structuredResult ? vpam::cloneNode(_structuredResult.get())
            : vpam::MathEvaluator::resultToAST(_lastResult);
    }
    _resultRow = static_cast<vpam::NodeRow*>(_resultNode.get());
    if (!_resultRow) return;
    // Conectar al canvas de resultado (sin cursor)
    _resultCanvas.setExpression(_resultRow, nullptr);
    _resultCanvas.resetScroll();   // Resetear scroll al cambiar de estado

    // Calcular layout del resultado
    _resultRow->calculateLayout(_resultCanvas.normalMetrics());

    // Dynamic layout: trim expression, position separator, fill result area
    applyResultLayout();

    _resultCanvas.invalidate();
    _mathCanvas.invalidate();

    // Show F2 hint in status bar when edu steps are available
    if (_hasEduSteps) {
        _statusBar.setTitle("F2: View Steps");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// clearResult() — Oculta el resultado y restaura el cursor
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::clearResult() {
    if (!_hasResult) return;

    ++_resultGeneration;
    _quantity.reset();_quantityView.reset();_quantityError=numos::quantity::Error::None;
    _hasResult = false;
    _resultNode.reset();
    _resultRow = nullptr;
    _structuredResult.reset();
    _structuredApproxResult.reset();
    _reusePolicy = numos::ResultReusePolicy::NonReusable;
    _sToDPolicy = numos::ResultSToDPolicy::Unavailable;
    _fallbackReason = numos::EngineFallbackReason::None;
    _lastKind = numos::CalcResultKind::None;
    _exactValValid = false;
    _exactText.clear();
    _approxText.clear();

    // Clear educational steps
    _eduStepLogger.clear();
    _eduArena.reset();
    _hasEduSteps = false;

    // Hide separator, result canvas and text-fallback label
    if (_resultSep) lv_obj_add_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
    if (_resultCanvas.obj()) lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    hideTextResult();

    // Restore expression canvas to full-area vertically-centered Edit Mode.
    // Re-set position too: applyResultLayout() moves the canvas, clearResult() must undo it.
    lv_obj_set_pos(_mathCanvas.obj(), PAD, CONTENT_TOP);
    lv_obj_set_height(_mathCanvas.obj(), CONTENT_FULL_H);
    _mathCanvas.setTraceLabel("calc_input_edit");
    _mathCanvas.setExpression(_rootRow, &_cursor);
    _mathCanvas.invalidate();

    // Restore title
    _statusBar.setTitle("Calculation");

    // Restaurar cursor
    _mathCanvas.startCursorBlink();
}

// ════════════════════════════════════════════════════════════════════════════
// toggleSD() — Rota entre 3 estados: Simbólico → Periódico → Extendido → ...
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::toggleSD() {
    if (!_hasResult || _lastStatus != numos::MathEngineStatus::Ok) return;
    if(_quantity) {
        const auto mode=_resultMode==numos::CalculationFormat::Standard?numos::CalculationFormat::Decimal:numos::CalculationFormat::Standard;
        if(formatAvailable(mode))publishQuantityFormat(mode,0,_fixedPlaces);
        return;
    }
    if (_resultMode == numos::CalculationFormat::Standard) {
        if (!formatAvailable(numos::CalculationFormat::Decimal)) return;
        _resultMode = numos::CalculationFormat::Decimal;
    } else _resultMode = numos::CalculationFormat::Standard;
    _engineeringShift = 0;
    _showDecimal = _resultMode == numos::CalculationFormat::Decimal;
    showResult();
}

// ════════════════════════════════════════════════════════════════════════════
// navigateHistory() — Navega por el historial con flechas arriba/abajo
//
// direction: -1 = ir atrás (más antigua), +1 = ir adelante (más reciente)
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::navigateHistory(int direction) {
    if (_history.empty()) return;

    int newIndex;
    if (_historyIndex < 0) {
        // Estamos en "nueva expresión": arriba va al último
        if (direction < 0) {
            newIndex = static_cast<int>(_history.size()) - 1;
        } else {
            return;  // Ya estamos al final
        }
    } else {
        newIndex = _historyIndex + direction;
    }

    // Clamp
    if (newIndex < 0) newIndex = 0;
    if (newIndex >= static_cast<int>(_history.size())) {
        // Pasamos del final → volver a nueva expresión
        _historyIndex = -1;
        clearResult();
        resetExpression();
        return;
    }

    loadHistoryEntry(newIndex);
}

// ════════════════════════════════════════════════════════════════════════════
// loadHistoryEntry() — Carga una entrada del historial en los canvas
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::loadHistoryEntry(int index) {
    if(index<0 || index>=int(_history.size()))return;
    const auto& entry=_history[index];
#if defined(__cpp_exceptions)
    try {
#endif
        // Allocate/convert privately. A failed recovery keeps the current editor,
        // cursor and result, not just the history's copy of the expression.
        auto expression=vpam::cloneNode(entry.exprAST.get());
        auto exact=entry.resultAST?vpam::cloneNode(entry.resultAST.get()):vpam::NodePtr{};
        auto approximate=entry.approximateAST?vpam::cloneNode(entry.approximateAST.get()):vpam::NodePtr{};
        auto visible=exact?vpam::cloneNode(exact.get()):vpam::NodePtr{};
        auto exactText=entry.exactText,approxText=entry.approxText;auto value=entry.result;
        std::unique_ptr<numos::quantity::Display> view;numos::quantity::Descriptor output;
        if(entry.quantity){output=numos::quantity::coherent(*entry.quantity);view=std::make_unique<numos::quantity::Display>();
            if(numos::quantity::convert(*entry.quantity,output,*view)!=numos::quantity::Error::None)return;}
        ++_resultGeneration;_historyIndex=index;
        _mathCanvas.setExpression(nullptr,nullptr);_rootNode=std::move(expression);_rootRow=static_cast<vpam::NodeRow*>(_rootNode.get());_cursor.init(_rootRow);
        _mathCanvas.setExpression(_rootRow,nullptr);_mathCanvas.resetScroll();refreshExpression();_mathCanvas.stopCursorBlink();
        _lastResult=std::move(value);_hasResult=true;_showDecimal=false;_resultMode=numos::CalculationFormat::Standard;
        _lastStatus=entry.status;_lastKind=entry.kind;_exactValValid=entry.exactValValid;
        _exactText=std::move(exactText);_approxText=std::move(approxText);
        _angleResult=entry.angleResult;_resultInDegrees=entry.resultInDegrees;
        _phaseUnit=_resultInDegrees?numos::CalculationFormat::Degrees:numos::CalculationFormat::Radians;
        _structuredResult=std::move(exact);_structuredApproxResult=std::move(approximate);
        _reusePolicy=entry.reusePolicy;_sToDPolicy=entry.sToDPolicy;_fallbackReason=entry.fallbackReason;
        _quantity=entry.quantity;_quantityError=entry.quantityError;_outputUnit=output;_quantityView=std::move(view);
        showResult(std::move(visible));
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){ui::StatusBar::showActiveNotice(numos::i18n::isSpanish(numos::i18n::productLocale)?"No se pudo recuperar la entrada":"Could not recover input");}
#endif
}

// ============================================================================
// ════════════════════════════════════════════════════════════════════════════
// alphaKeyToVarName() — Mapea tecla → char de variable en modo ALPHA
//
// Mapeo estilo Casio:
//   NUM_1→A, NUM_2→B, NUM_3→C, NUM_4→D, NUM_5→E, NUM_6→F
//   VAR_X→x, VAR_Y→y, SOLVE(→z)
//   ENTER→Ans (#), FREE_EQ→PreAns ($)
// ════════════════════════════════════════════════════════════════════════════

char CalculationApp::alphaKeyToVarName(KeyCode code) {
    switch (code) {
        case KeyCode::NUM_1:  return 'A';
        case KeyCode::NUM_2:  return 'B';
        case KeyCode::NUM_3:  return 'C';
        case KeyCode::NUM_4:  return 'D';
        case KeyCode::NUM_5:  return 'E';
        case KeyCode::NUM_6:  return 'F';
        case KeyCode::VAR_X:  return 'x';
        case KeyCode::VAR_Y:  return 'y';
        case KeyCode::SOLVE:  return 'z'; // z mapped to SOLVE key position
        default:              return '\0';
    }
}

// ════════════════════════════════════════════════════════════════════════════
// executeStore() — Guarda el Ans actual en la variable indicada
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::executeStore(char varName) {
    auto& engine=numos::CalculationEngine::instance();
    const bool spanish=numos::i18n::isSpanish(numos::i18n::productLocale);
#if defined(__cpp_exceptions)
    try {
#endif
        const bool quantity=engine.ansIsQuantity();
        if(!engine.storeAns(varName))ui::StatusBar::showActiveNotice(spanish?"No se pudo guardar; valor conservado":"Could not store; value retained");
        else if(quantity)ui::StatusBar::showActiveNotice(spanish?"Solo sesión; se borra al reiniciar":"Session only; clears on restart");
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){ui::StatusBar::showActiveNotice(spanish?"Memoria insuficiente":"Not enough memory");}
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// generateEduSteps() — Generate step-by-step breakdown of arithmetic
//
// Converts the VPAM AST to a CAS SymExpr, then uses SymSimplify
// in single-pass mode to produce atomic simplification steps.
// Each intermediate expression is logged with a reason string.
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::generateEduSteps() {
    if (!_rootRow) return;

    _eduStepLogger.clear();
    _eduArena.reset();
    _hasEduSteps = false;

    // Convert VPAM AST → CAS SymExpr tree
    cas::ASTFlattener flattener;
    flattener.setVariable('x');
    flattener.setArena(&_eduArena);
    cas::SymExpr* expr = flattener.flattenToExpr(_rootRow);
    if (!expr) return;

    // Log the original expression
    _eduStepLogger.logExpr("Original expression", expr,
                           cas::MethodId::General, "Input");

    // Run simplification passes one at a time (atomic mode).
    // Safety limit prevents infinite loops in edge-case expressions.
    cas::SymExpr* current = expr;
    static constexpr int MAX_EDU_STEPS = 20;

    for (int step = 0; step < MAX_EDU_STEPS; ++step) {
        cas::SymExpr* next = cas::SymSimplify::simplifyPass(current, _eduArena);

        // Fixed point reached — no more simplifications
        if (next == current) break;

        // Determine reason based on the transformation type
        std::string reason;
        if (next->type == cas::SymExprType::Num) {
            reason = "Evaluate result";
        } else if (current->type == cas::SymExprType::Pow ||
                   (current->type == cas::SymExprType::Add && next->type != cas::SymExprType::Add)) {
            reason = "Solve exponent";
        } else if (current->type == cas::SymExprType::Mul ||
                   (current->type == cas::SymExprType::Add &&
                    next->type == cas::SymExprType::Add)) {
            reason = "Simplify terms";
        } else {
            reason = "Simplify";
        }

        // FPU guard: evaluate at x=0 to check for math errors (e.g. 1/0)
        double intermediateVal = next->evaluate(0.0);
        if (!std::isfinite(intermediateVal) && next->type != cas::SymExprType::Num) {
            // Skip logging steps with math errors in intermediate results
            current = next;
            continue;
        }

        _eduStepLogger.logExpr(next->toString(), next,
                               cas::MethodId::General, reason);

        current = next;
    }

    // Log final result
    if (current != expr && _eduStepLogger.count() > 1) {
        _hasEduSteps = true;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// openStepViewer() — Show the step-by-step viewer screen
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::openStepViewer() {
    if (_stepViewerActive || !_hasEduSteps) return;
    _stepViewerActive = true;

    // Hide the main expression/result canvases
    if (_mathCanvas.obj()) lv_obj_add_flag(_mathCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    if (_resultCanvas.obj()) lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    if (_resultSep) lv_obj_add_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
    hideTextResult();

    _statusBar.setTitle("Steps");

    // Create scrollable steps container
    int barH = ui::StatusBar::HEIGHT + 1;
    _stepsContainer = lv_obj_create(_screen);
    lv_obj_set_size(_stepsContainer, SCREEN_W, SCREEN_H - barH);
    lv_obj_set_pos(_stepsContainer, 0, barH);
    lv_obj_set_style_bg_color(_stepsContainer, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_stepsContainer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_stepsContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_stepsContainer, PAD, LV_PART_MAIN);
    lv_obj_set_flex_flow(_stepsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_stepsContainer, 4, LV_PART_MAIN);
    lv_obj_add_flag(_stepsContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_stepsContainer, LV_DIR_VER);
    lv_obj_add_style(_stepsContainer, &ui::style_math_primary, LV_PART_MAIN);

    buildStepsDisplay();
    lv_obj_scroll_to_y(_stepsContainer, 0, LV_ANIM_OFF);
    lv_obj_invalidate(_screen);
}

// ════════════════════════════════════════════════════════════════════════════
// closeStepViewer() — Return from step viewer to normal calculator view
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::closeStepViewer() {
    if (!_stepViewerActive) return;
    _stepViewerActive = false;

    // Clean up step renderers
    _stepRenderers.clear();

    // Remove steps container
    if (_stepsContainer) {
        lv_obj_delete(_stepsContainer);
        _stepsContainer = nullptr;
    }

    // Restore main UI
    if (_mathCanvas.obj()) lv_obj_remove_flag(_mathCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
    if (_hasResult) {
        if (_lastStatus == numos::MathEngineStatus::Ok &&
            _lastKind == numos::CalcResultKind::TextFallback) {
            std::string visible = "Giac text fallback (";
            visible += numos::engineFallbackReasonName(_fallbackReason);
            visible += "): ";
            visible += _exactText;
            showTextResult(visible);
        } else {
            if (_resultCanvas.obj()) lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
            if (_resultSep) lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
        }
        _statusBar.setTitle("F2: View Steps");
    } else {
        _statusBar.setTitle("Calculation");
    }

    lv_obj_invalidate(_screen);
}

// ════════════════════════════════════════════════════════════════════════════
// buildStepsDisplay() — Build LVGL widgets for each step
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::buildStepsDisplay() {
    _stepRenderers.clear();
    if (!_stepsContainer) return;

    lv_obj_clean(_stepsContainer);

    const auto& steps = _eduStepLogger.steps();

    if (steps.empty()) {
        lv_obj_t* lbl = lv_label_create(_stepsContainer);
        lv_label_set_text(lbl, "No steps available.");
        lv_obj_add_style(lbl, &ui::style_math_primary, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x808080), LV_PART_MAIN);
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
        srd->canvas.setAutoHeightEnabled(false);

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
    };

    // Iterate all steps
    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];

        // Step number and description
        if (!step.description.empty()) {
            char buf[200];
            snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1),
                     step.description.c_str());

            lv_obj_t* descLbl = lv_label_create(_stepsContainer);
            lv_label_set_text(descLbl, buf);
            lv_obj_set_width(descLbl, SCREEN_W - 2 * PAD - 8);
            lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
            lv_obj_add_style(descLbl, &ui::style_math_primary, LV_PART_MAIN);
            lv_obj_set_style_text_color(descLbl, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
        }

        // Reason label (right side / below)
        if (!step.reason.empty()) {
            lv_obj_t* reasonLbl = lv_label_create(_stepsContainer);
            char reasonBuf[128];
            snprintf(reasonBuf, sizeof(reasonBuf), "  " LV_SYMBOL_RIGHT " %s",
                     step.reason.c_str());
            lv_label_set_text(reasonLbl, reasonBuf);
            lv_obj_add_style(reasonLbl, &ui::style_math_primary, LV_PART_MAIN);
            lv_obj_set_style_text_color(reasonLbl, lv_color_hex(0x4A90D9), LV_PART_MAIN);
        }

        // MathCanvas for CAS expression
        if (step.mathExpr) {
            vpam::NodePtr astNode = cas::SymExprToAST::convert(step.mathExpr);
            emitCanvas(std::move(astNode));
        }
    }

    // Footer hint
    lv_obj_t* hintLbl = lv_label_create(_stepsContainer);
    lv_label_set_text(hintLbl,
                      LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll    F2/AC: Back");
    lv_obj_add_style(hintLbl, &ui::style_math_primary, LV_PART_MAIN);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(0x808080), LV_PART_MAIN);
}

#ifdef NATIVE_SIM
// ════════════════════════════════════════════════════════════════════════════
// Calculation emulator probes — read-only accessors for the .numos runner.
// ════════════════════════════════════════════════════════════════════════════

const char* CalculationApp::debugCalcResultKind() const {
    switch (_lastKind) {
        case numos::CalcResultKind::Structured:   return "structured";
        case numos::CalcResultKind::TextFallback: return "text_fallback";
        case numos::CalcResultKind::None:
        default:                                  return "none";
    }
}

const char* CalculationApp::debugCalcStatus() const {
    switch (_lastStatus) {
        case numos::MathEngineStatus::Ok:              return "ok";
        case numos::MathEngineStatus::Undefined:       return "undefined";
        case numos::MathEngineStatus::ParseError:      return "parse_error";
        case numos::MathEngineStatus::EvaluationError: return "evaluation_error";
        case numos::MathEngineStatus::Unsupported:     return "unsupported";
        case numos::MathEngineStatus::UnitsUnavailable: return "units_unavailable";
        case numos::MathEngineStatus::QuantityError: return "quantity_error";
        case numos::MathEngineStatus::OutOfMemory:     return "out_of_memory";
    }
    return "?";
}

const std::string& CalculationApp::debugCalcExactText() const {
    return _quantityView ? _quantityView->exact : _exactText;
}
#endif // NATIVE_SIM

#ifdef NATIVE_SIM
namespace {
bool calcInputTreeValid(const vpam::MathNode* node, const vpam::MathNode* parent,
                        const vpam::MathNode* cursor, bool& found, unsigned depth = 0) {
    if (!node || depth > 64 || node->parent() != parent) return false;
    if (node == cursor) found = true;
    if (node->type() == vpam::NodeType::Number &&
        (node->layout().width <= 0 || node->layout().height() <= 0)) return false;
    for (int i = 0; i < node->childCount(); ++i)
        if (!calcInputTreeValid(node->child(i),node,cursor,found,depth+1)) return false;
    return true;
}
}
bool CalculationApp::debugInput(const std::string& expected) const {
    std::string serialized, diagnostic;
    const bool complete = numos::CalculationEngine::serializeForGiac(_rootRow, serialized, diagnostic);
    const auto& cursor = _cursor.cursor();
    if (expected.compare(0,8,"toolbox ") == 0) return ui::toolbox::debug(expected.c_str()+8);
    if(expected=="unit_menu closed")return !_unitMenu;
    if(expected=="unit_menu all")return _unitMenu && !_formatPickingComponents && _formatChoice==_unitMenu->choices.count+1;
    if(expected=="unit_menu components")return _unitMenu && _formatPickingComponents;
    if(expected=="unit_menu geometry") {
        if(!_unitMenu)return false;
        lv_obj_update_layout(_formatMenu);
        for(unsigned i=0;i<_unitMenu->canvases.size();++i) {
            const auto& canvas=_unitMenu->canvases[i];
            if(lv_obj_has_flag(_formatMenuRows[i],LV_OBJ_FLAG_HIDDEN) || lv_obj_has_flag(canvas.obj(),LV_OBJ_FLAG_HIDDEN))continue;
            const auto* node=_unitMenu->previews[i].get();
            if(!node || node->layout().width+16>lv_obj_get_width(canvas.obj()) || node->layout().height()+2>lv_obj_get_height(canvas.obj()))return false;
        }return true;
    }
    if(expected=="unit_menu dump") {
        if(!_unitMenu)return false;
        std::printf("[UNIT-MENU] choice=%u count=%u quick=%u favorites=%u components=%u generation=%u ids=",unsigned(_formatChoice),unsigned(_formatChoiceCount),_unitMenu->choices.count,_unitMenu->choices.favorites,_formatPickingComponents,_unitMenu->generation);
        for(unsigned i=0;i<_unitMenu->choices.count;++i)std::printf("%u:%u,",_unitMenu->choices.ids[i].id,_unitMenu->choices.ids[i].variant);
        std::puts("");return true;
    }
    if(expected=="quantity dump") {
        std::printf("[QUANTITY] canonical=%s error=%s mode=%u shift=%d places=%u generation=%u terms=",
            _quantity?_quantity->coefficient.c_str():"none",numos::quantity::errorName(_quantityError),unsigned(_resultMode),_engineeringShift,unsigned(_fixedPlaces),unsigned(_resultGeneration));
        for(unsigned i=0;i<_outputUnit.count;++i)std::printf("%u:%u:%d,",unsigned(_outputUnit.terms[i].atom.unit),unsigned(_outputUnit.terms[i].atom.prefix),int(_outputUnit.terms[i].power));
        std::printf("\n");return true;
    }
    if(expected=="uncertainty dump") {
        if(!_quantity)return false;
        double standard=0;
        if(numos::quantity::standardUncertainty(*_quantity,standard)!=numos::quantity::Error::None)return false;
        std::printf("[UNCERTAINTY] u=%.17g displayed_u=%.17g relative=%.17g provenance=%u sources=%u ids=",
            standard,_quantityView?_quantityView->standardUncertainty:0,_quantityView?_quantityView->relativeUncertainty:0,
            unsigned(_quantity->provenance),_quantity->uncertainty?unsigned(_quantity->uncertainty->count):0);
        if(_quantity->uncertainty)for(unsigned i=0;i<_quantity->uncertainty->count;++i)
            std::printf("%u:%.17g,",unsigned(_quantity->uncertainty->contributions[i].source),_quantity->uncertainty->contributions[i].coefficient);
        std::printf("\n");return true;
    }
    if(expected.compare(0,18,"uncertainty count ")==0)return _quantity &&
        (_quantity->uncertainty?_quantity->uncertainty->count:0)==std::strtoul(expected.c_str()+18,nullptr,10);
    if(expected.compare(0,12,"uncertainty ")==0) {
        char name[24]{};double wanted=0,tolerance=1e-10;
        if(std::sscanf(expected.c_str()+12,"%23s %lf %lf",name,&wanted,&tolerance)<2 || !_quantity)return false;
        double actual=0;
        if(!std::strcmp(name,"standard")){if(numos::quantity::standardUncertainty(*_quantity,actual)!=numos::quantity::Error::None)return false;}
        else if(!std::strcmp(name,"display") && _quantityView)actual=_quantityView->standardUncertainty;
        else if(!std::strcmp(name,"relative") && _quantityView && _quantityView->relativeDefined)actual=_quantityView->relativeUncertainty;
        else return false;
        return std::isfinite(actual) && std::abs(actual-wanted)<=tolerance;
    }
    if(expected.compare(0,19,"quantity canonical ")==0)return _quantity && _quantity->coefficient==expected.substr(19);
    if(expected.compare(0,15,"quantity error ")==0)return expected.substr(15)==numos::quantity::errorName(_quantityError);
    if(expected=="quantity none")return !_quantity;
    if(expected.compare(0,14,"quantity role ")==0) {
        if(!_quantity)return false;
        using R=numos::quantity::Value::Role;
        const char* role=_quantity->role==R::TemperaturePoint?"point":_quantity->role==R::TemperatureDifference?"difference":"linear";
        return expected.substr(14)==role;
    }
    if(expected.compare(0,15,"quantity terms ")==0) {
        std::string actual;
        for(unsigned i=0;i<_outputUnit.count;++i){const auto& t=_outputUnit.terms[i];if(i)actual+=",";actual+=std::to_string(unsigned(t.atom.unit))+":"+std::to_string(unsigned(t.atom.prefix))+":"+std::to_string(int(t.power));}
        return actual==expected.substr(15);
    }
    if (expected == "dump") {
        std::printf("[CALC-INPUT] cursor=%d slot=%s complete=%d serialized=%s diagnostic=%s\n%s",
                    cursor.index, cursor.row == _rootRow ? "root" : "nested", complete,
                    serialized.c_str(), diagnostic.c_str(), vpam::dumpTree(_rootRow).c_str());
        return true;
    }
    if (expected == "no_result") return !_hasResult;
    unsigned unitId=0,prefixId=0,expectedCount=0;
    if (std::sscanf(expected.c_str(),"unit %u %u %u",&unitId,&prefixId,&expectedCount)==3) {
        std::array<const vpam::MathNode*,82> pending{};size_t size=1;pending[0]=_rootRow;unsigned matches=0;
        while(size) {
            const auto* node=pending[--size];if(!node)continue;
            if(node->type()==vpam::NodeType::Unit) {
                const auto atom=static_cast<const vpam::NodeUnit*>(node)->atom();
                if(uint16_t(atom.unit)==unitId && uint8_t(atom.prefix)==prefixId)++matches;
            }
            for(int i=0;i<node->childCount();++i){if(size==pending.size())return false;pending[size++]=node->child(i);}
        }
        return matches==expectedCount;
    }
    if (expected == "incomplete") return !complete;
    if (expected == "complete") return complete;
    if (expected == "structure") {
        bool found = false;
        return calcInputTreeValid(_rootRow,nullptr,cursor.row,found) && found &&
            cursor.index >= 0 && cursor.index <= cursor.row->childCount();
    }
    if (expected.compare(0,5,"near ") == 0) {
        double value=0, tolerance=0; char extra=0;
        if (std::sscanf(expected.c_str()+5,"%lf %lf %c",&value,&tolerance,&extra)!=2) return false;
        return _hasResult && _lastStatus == numos::MathEngineStatus::Ok &&
            std::isfinite(_lastResult.toDouble()) && tolerance >= 0 &&
            std::fabs(_lastResult.toDouble()-value) <= tolerance;
    }
    if (expected.compare(0,9,"rational ") == 0) {
        long long numerator=0, denominator=0;char extra=0;
        int64_t lhs=0,rhs=0;
        if (std::sscanf(expected.c_str()+9,"%lld %lld %c",&numerator,&denominator,&extra)!=2 ||
            denominator==0 || !_hasResult || !_exactValValid || !_lastResult.ok ||
            _lastResult.approximate || !_lastResult.isRational()) return false;
        // Exact rational comparison, including a sign kept in Giac's denominator.
        return !__builtin_mul_overflow(_lastResult.num, denominator, &lhs) &&
               !__builtin_mul_overflow(numerator, _lastResult.den, &rhs) && lhs==rhs;
    }
    if (expected == "cursor") return cursor.row && cursor.index >= 0 && cursor.index <= cursor.row->childCount();
    if (expected.compare(0,11,"serialized ") == 0) {
        if (complete && serialized == expected.substr(11)) return true;
        std::printf("[CALC-INPUT] actual=%s diagnostic=%s\n",serialized.c_str(),diagnostic.c_str());
    }
    return false;
}
#endif


const std::string& CalculationApp::numericResultText() const {
    numos::DecimalParts scalar;
    if(_quantityView)return numos::decimalParts(_quantityView->approximate,scalar)?_quantityView->approximate:_quantityView->exact;
    return numos::decimalParts(_approxText, scalar) ? _approxText : _exactText;
}

bool CalculationApp::complexResult() const {
    bool imaginary = false;
    unsigned budget = 400;
    return numos::numericComplex(_structuredResult.get(), imaginary, budget) && imaginary;
}

bool CalculationApp::formatAvailable(numos::CalculationFormat format) const {
    using F = numos::CalculationFormat;
    if (!_hasResult || _lastStatus != numos::MathEngineStatus::Ok) return false;
    if(_quantity) {
        if(!_quantityView)return false;
        if(format>=F::Uncertainty && format<=F::UncertaintyParenthetical)return _quantityView->hasUncertainty;
        if(_quantityView->hasUncertainty)return format==F::Standard || format==F::Decimal || format==F::OutputUnit;
        if(format==F::Standard || format==F::OutputUnit)return true;
        if(format==F::Decimal)return bool(_quantityView->approximateCoefficient);
        numos::DecimalParts value;
        return (format==F::Scientific || format==F::Engineering || format==F::Fixed) &&
            numos::decimalParts(numericResultText(),value) && (format!=F::Fixed || value.exponent<=255);
    }
    const bool rational = _exactValValid && _lastResult.isRational();
    switch (format) {
        case F::Standard: return true;
        case F::Decimal: return _structuredApproxResult != nullptr &&
            _sToDPolicy != numos::ResultSToDPolicy::Unavailable;
        case F::Periodic:
        case F::Extended: return rational && _lastResult.den > 1;
        case F::MixedFraction:
            return rational && _lastResult.den > 1 &&
                _lastResult.num != INT64_MIN &&
                (_lastResult.num / _lastResult.den != 0);
        case F::PrimeFactors:
            // Bounded interactive factorization; unlike normal evaluation,
            // factoring a large semiprime can have unpredictable cost.
            return !_angleResult && rational && _lastResult.den == 1 &&
                _lastResult.num >= -1000000000000LL && _lastResult.num <= 1000000000000LL &&
                (_lastResult.num < -1 || _lastResult.num > 1);
        case F::Scientific:
        case F::Engineering: {
            numos::DecimalParts scalar;
            return numos::decimalParts(numericResultText(), scalar);
        }
        case F::Polar:
        case F::Exponential: return complexResult();
        case F::Fixed: {
            numos::DecimalParts scalar;
            return numos::decimalParts(numericResultText(), scalar) && scalar.exponent <= 255;
        }
        case F::Radians: case F::Degrees: case F::Gradians: {
            numos::DecimalParts scalar;
            return (_angleResult && numos::decimalParts(numericResultText(), scalar)) ||
                   (complexResult() && _resultMode == F::Polar);
        }
        default: return false;
    }
}

namespace {
vpam::NodePtr quantityFormatted(const numos::quantity::Display& view,const numos::quantity::Descriptor& unit,
        numos::CalculationFormat format,int shift,unsigned places) {
    using F=numos::CalculationFormat;
    if(view.hasUncertainty)return numos::scientific::uncertaintyReport(view,unit,format);
    numos::DecimalParts parts;
    const auto& numeric=numos::decimalParts(view.approximate,parts)?view.approximate:view.exact;
    vpam::NodePtr coefficient;
    switch(format) {
        case F::Standard:coefficient=vpam::cloneNode(view.exactCoefficient.get());break;
        case F::Decimal:coefficient=vpam::cloneNode(view.approximateCoefficient.get());break;
        case F::Scientific:coefficient=numos::powerOfTenFormat(numeric,false);break;
        case F::Engineering:coefficient=numos::powerOfTenFormat(numeric,true,shift);break;
        case F::Fixed:{std::string text;if(!numos::fixedDecimal(numeric,places,text))return {};
            numos::EngineResultNode n;n.kind=numos::EngineNodeKind::Decimal;n.text=std::move(text);
            coefficient=numos::CalculationEngine::resultTreeToAST(n);break;}
        default:return {};
    }
    return numos::quantity::compose(std::move(coefficient),unit);
}
}

vpam::NodePtr CalculationApp::formattedResult() {
    using F = numos::CalculationFormat;
    if(_quantity && _quantityView)return quantityFormatted(*_quantityView,_outputUnit,_resultMode,_engineeringShift,_fixedPlaces);
    switch (_resultMode) {
        case F::Standard:
            return _structuredResult ? vpam::cloneNode(_structuredResult.get())
                : vpam::MathEvaluator::resultToAST(_lastResult);
        case F::Decimal:
            return _structuredApproxResult ? vpam::cloneNode(_structuredApproxResult.get()) : nullptr;
        case F::Periodic: return vpam::MathEvaluator::resultToPeriodicAST(_lastResult);
        case F::Extended: return vpam::MathEvaluator::resultToExtendedAST(_lastResult, 200);
        case F::Scientific: return numos::powerOfTenFormat(numericResultText(), false);
        case F::Engineering: return numos::powerOfTenFormat(numericResultText(), true, _engineeringShift);
        case F::MixedFraction: return numos::mixedFractionFormat(_lastResult);
        case F::PrimeFactors: {
            auto result = numos::GiacEngine::instance().evaluateStructured(("ifactor(" + _exactText + ")").c_str());
            return result.base.ok() && result.hasTree ? numos::CalculationEngine::resultTreeToAST(result.tree) : nullptr;
        }
        case F::Polar:
        case F::Exponential: {
            auto& engine = numos::GiacEngine::instance();
            auto magnitude = engine.evaluateStructured(("abs(" + _exactText + ")").c_str());
            // WHY: Giac's arg() in DEG can round an exact phase to a double.
            // Evaluate this display transform in a scoped radian context;
            // the engine restores its mode and the input setting is untouched.
            const std::string angle = "arg(" + _exactText + ")";
            std::string radians = angle;
            if (_resultMode == F::Polar) {
                if (_phaseUnit == F::Degrees) radians = "(" + radians + ")*180/pi";
                if (_phaseUnit == F::Gradians) radians = "(" + radians + ")*200/pi";
            }
            auto argument = engine.evaluateStructured(radians.c_str(), true,
                numos::GiacEngine::EvaluationAngle::Radians);
            if (!magnitude.base.ok() || !magnitude.hasTree || !argument.base.ok() || !argument.hasTree) return nullptr;
            auto r = numos::CalculationEngine::resultTreeToAST(magnitude.tree);
            auto theta = numos::CalculationEngine::resultTreeToAST(argument.tree);
            if (!r || !theta) return nullptr;
            if (_resultMode == F::Polar) {
                auto* row = static_cast<vpam::NodeRow*>(r.get());
                row->appendChild(vpam::makeSymbol("\xE2\x80\x89\xE2\x88\xA0\xE2\x80\x89"));
                row->appendChild(std::move(theta));
                row->appendChild(vpam::makeSymbol(_phaseUnit == F::Degrees ? "\xC2\xB0" :
                    _phaseUnit == F::Gradians ? "\xE2\x80\x89gon" : "\xE2\x80\x89rad"));
                return r;
            }
            auto exponent = vpam::makeRow();
            auto* exp = static_cast<vpam::NodeRow*>(exponent.get());
            exp->appendChild(vpam::makeConstant(vpam::ConstKind::Imag));
            exp->appendChild(vpam::makeOperator(vpam::OpKind::Mul));
            exp->appendChild(vpam::makeParen(std::move(theta)));
            auto* row = static_cast<vpam::NodeRow*>(r.get());
            row->appendChild(vpam::makeOperator(vpam::OpKind::Mul));
            row->appendChild(vpam::makePower(vpam::makeConstant(vpam::ConstKind::E), std::move(exponent)));
            return r;
        }
        case F::Fixed: {
            std::string decimal;
            if (!numos::fixedDecimal(numericResultText(), _fixedPlaces, decimal)) return nullptr;
            numos::EngineResultNode number;
            number.kind = numos::EngineNodeKind::Decimal;
            number.text = std::move(decimal);
            return numos::CalculationEngine::resultTreeToAST(number);
        }
        case F::Radians: case F::Degrees: case F::Gradians: {
            std::string expression = "(" + _exactText + ")";
            if (_resultInDegrees) expression += "*pi/180";
            if (_resultMode == F::Degrees) expression += "*180/pi";
            if (_resultMode == F::Gradians) expression += "*200/pi";
            auto angle = numos::GiacEngine::instance().evaluateStructured(expression.c_str(), true);
            if (!angle.base.ok() || !angle.hasTree) return nullptr;
            auto value = numos::CalculationEngine::resultTreeToAST(angle.tree);
            if (value) static_cast<vpam::NodeRow*>(value.get())->appendChild(vpam::makeSymbol(
                _resultMode == F::Degrees ? "\xC2\xB0" :
                _resultMode == F::Gradians ? "\xE2\x80\x89gon" : "\xE2\x80\x89rad"));
            return value;
        }
        default: return nullptr;
    }
}

void CalculationApp::selectFormat(numos::CalculationFormat format) {
    if (!formatAvailable(format)) return;
    if(format==numos::CalculationFormat::Uncertainty){openFormatMenu(false,false,false,true);return;}
    if(format==numos::CalculationFormat::OutputUnit){openFormatMenu(false,true);return;}
    if (format == numos::CalculationFormat::Fixed) { openFormatMenu(true); return; }
    if(_quantity){publishQuantityFormat(format,0,_fixedPlaces);return;}
    if (complexResult() && (format == numos::CalculationFormat::Radians ||
        format == numos::CalculationFormat::Degrees || format == numos::CalculationFormat::Gradians)) {
        _phaseUnit = format;
        _resultMode = numos::CalculationFormat::Polar;
    } else _resultMode = format;
    _engineeringShift = 0;
    _showDecimal = format == numos::CalculationFormat::Decimal;
    showResult();
}

void CalculationApp::engineeringFormat(int direction) {
    if (!formatAvailable(numos::CalculationFormat::Engineering)) return;
    if(_quantity){publishQuantityFormat(numos::CalculationFormat::Engineering,
        _resultMode==numos::CalculationFormat::Engineering?std::max(-6,std::min(6,_engineeringShift-direction)):0,_fixedPlaces);return;}
    if (_resultMode != numos::CalculationFormat::Engineering) _engineeringShift = 0;
    else _engineeringShift = std::max(-6, std::min(6, _engineeringShift - direction));
    _resultMode = numos::CalculationFormat::Engineering;
    showResult();
}

void CalculationApp::openFormatMenu(bool pickDigits,bool pickUnits,bool pickComponents,bool pickUncertainty) {
    if (_formatMenu || !_hasResult || _lastStatus != numos::MathEngineStatus::Ok) return;
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    if(pickUnits) {
        lv_mem_monitor_t memory{};lv_mem_monitor(&memory);
        if(memory.free_size<15500 || memory.free_biggest_size<5000) {
            ui::StatusBar::showActiveNotice(numos::i18n::isSpanish(numos::i18n::productLocale)?"Sin memoria":"Low memory");return;
        }
    }
#endif
    _formatPickingDigits = pickDigits;_formatPickingUnit=pickUnits;_formatPickingComponents=pickComponents;_formatPickingUncertainty=pickUncertainty;
    _formatChoiceCount = _formatChoice = 0;
    if(pickUnits) {
        if(!_quantity)return;
#if defined(__cpp_exceptions)
        try {
#endif
            _unitMenu=std::make_unique<UnitOutputMenu>();_unitMenu->generation=_resultGeneration;
            if(pickComponents)_formatChoiceCount=_outputUnit.count;
            else {_unitMenu->choices.prepare(*_quantity,_outputUnit);_formatChoiceCount=4+_unitMenu->choices.count;}
#if defined(__cpp_exceptions)
        }catch(const std::bad_alloc&){_unitMenu.reset();ui::StatusBar::showActiveNotice(numos::i18n::isSpanish(numos::i18n::productLocale)?"Sin memoria":"Low memory");return;}
#endif
    }
    else if(pickUncertainty) {
        for(unsigned i=unsigned(numos::CalculationFormat::UncertaintyStandard);i<=unsigned(numos::CalculationFormat::UncertaintyParenthetical);++i)
            _formatChoices[_formatChoiceCount++]=static_cast<numos::CalculationFormat>(i);
    }
    else if (pickDigits) { _formatChoiceCount = 10; _formatChoice = _fixedPlaces; }
    else {
      if(_quantity)_formatChoices[_formatChoiceCount++]=numos::CalculationFormat::OutputUnit;
      for (unsigned i = 0; i < static_cast<unsigned>(numos::CalculationFormat::Count); ++i) {
        const auto format = static_cast<numos::CalculationFormat>(i);
        if(format>=numos::CalculationFormat::UncertaintyStandard)continue;
        if(_quantity && format==numos::CalculationFormat::OutputUnit)continue;
        if (!formatAvailable(format)) continue;
        if (!_quantity && format == _resultMode) _formatChoice = _formatChoiceCount;
        _formatChoices[_formatChoiceCount++] = format;
      }
    }
    if (!_formatChoiceCount) return;
    const bool spanish = numos::i18n::isSpanish(numos::i18n::productLocale);
    _formatMenu = lv_obj_create(_screen);
    lv_obj_remove_style_all(_formatMenu);
    lv_obj_set_pos(_formatMenu, 0, 24);
    lv_obj_set_size(_formatMenu, 320, 216);
    lv_obj_remove_flag(_formatMenu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(_formatMenu, lv_color_hex(0x14243B), 0);
    lv_obj_set_style_bg_opa(_formatMenu, LV_OPA_40, 0);

    auto* panel = lv_obj_create(_formatMenu);
    lv_obj_remove_style_all(panel);
    const int rowHeight=pickUnits?40:24;
    const int visibleRows = std::min<int>(pickUnits?3:5, _formatChoiceCount);
    const int panelHeight = 82 + visibleRows * rowHeight;
    lv_obj_set_pos(panel, 8, (216 - panelHeight) / 2);
    lv_obj_set_size(panel, 304, panelHeight);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xDCE3EE), 0);
    auto label = [](lv_obj_t* parent, const char* text, int x, int y,
                    const lv_font_t* font, uint32_t color) {
        auto* object = lv_label_create(parent);
        lv_label_set_text(object, text);
        lv_obj_set_pos(object, x, y);
        lv_obj_set_style_text_font(object, font, 0);
        lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
        return object;
    };
    label(panel, spanish ? "MOSTRAR COMO" : "DISPLAY AS", 14, 9,
          &lv_font_montserrat_12, 0x526D91);
    label(panel, pickUncertainty ? (spanish?"Incertidumbre":"Uncertainty") : pickComponents ? (spanish?"Por componentes":"By components") : pickUnits ? (spanish?"Unidad de salida":"Output unit") : pickDigits ? (spanish ? "Decimales fijos" : "Decimal places") :
          (spanish ? "Formato del resultado" : "Result format"), 14, 25,
          &lv_font_montserrat_14, 0x18283F);
    _formatMenuCounter = label(panel, "", 256, 26, &lv_font_montserrat_12, 0x758399);

    for (unsigned i = 0; i < 5; ++i) {
        auto* row = _formatMenuRows[i] = lv_obj_create(panel);
        if(!row){closeFormatMenu();ui::StatusBar::showActiveNotice(spanish?"Sin memoria":"Low memory");return;}
        lv_obj_remove_style_all(row);
        lv_obj_set_pos(row, 8, 51 + i * rowHeight);
        lv_obj_set_size(row, 278, rowHeight);
        lv_obj_set_style_radius(row, 5, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        // WHY: reuse the existing Spanish fallback; ASCII geometry, size and
        // kerning stay unchanged, while í/ó and their capitals have real glyphs.
        _formatMenuLabels[i] = label(row, "", 9, 4, ui::tutorFont14(), 0x344155);
        lv_obj_set_width(_formatMenuLabels[i],240);lv_label_set_long_mode(_formatMenuLabels[i],LV_LABEL_LONG_DOT);
        _formatMenuMarks[i] = label(row, "", 254, 5, &lv_font_montserrat_12, 0x526D91);
        if(_unitMenu && i<3) {
            auto& canvas=_unitMenu->canvases[i];canvas.create(row);canvas.setAutoHeightEnabled(false);canvas.stopCursorBlink();
            canvas.setMathStyle(vpam::MathStyle::SCRIPT);canvas.setEmptyRootPlaceholderVisible(false);
            lv_obj_set_pos(canvas.obj(),0,0);lv_obj_set_size(canvas.obj(),88,rowHeight);
            lv_obj_remove_flag(canvas.obj(),LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_bg_opa(canvas.obj(),LV_OPA_TRANSP,0);
        }
        lv_obj_add_event_cb(row, [](lv_event_t* event) {
            auto* self = static_cast<CalculationApp*>(lv_event_get_user_data(event));
            auto* target = lv_event_get_target_obj(event);
            for (unsigned slot = 0; slot < 5; ++slot) {
                if (self->_formatMenuRows[slot] != target) continue;
                const unsigned visible=self->_unitMenu?3:5;
                const unsigned first = self->_formatChoice < visible ? 0 : self->_formatChoice - visible+1;
                if(slot>=visible)return;
                if (first + slot >= self->_formatChoiceCount) return;
                self->_formatChoice = first + slot;
                self->applyFormatChoice(); return;
            }
        }, LV_EVENT_CLICKED, this);
    }
    auto* track = lv_obj_create(panel);
    lv_obj_remove_style_all(track);
    lv_obj_set_pos(track, 292, 55); lv_obj_set_size(track, 2, 112);
    lv_obj_set_style_bg_color(track, lv_color_hex(0xE6ECF4), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    if (_formatChoiceCount <= unsigned(visibleRows)) lv_obj_add_flag(track, LV_OBJ_FLAG_HIDDEN);
    _formatMenuThumb = lv_obj_create(track);
    lv_obj_remove_style_all(_formatMenuThumb);
    lv_obj_set_style_bg_color(_formatMenuThumb, lv_color_hex(0x9BACBF), 0);
    lv_obj_set_style_bg_opa(_formatMenuThumb, LV_OPA_COVER, 0);
    lv_obj_set_width(_formatMenuThumb, 2);
    const int footerY = panelHeight - 20;
    label(panel, LV_SYMBOL_UP LV_SYMBOL_DOWN, 15, footerY, &lv_font_montserrat_12, 0x64748B);
    label(panel, spanish ? "Elegir" : "Choose", 41, footerY, &lv_font_montserrat_12, 0x64748B);
    label(panel, spanish ? "EXE  Aplicar" : "EXE  Apply", 113, footerY, &lv_font_montserrat_12, 0x344155);
    label(panel, "BACK", 254, footerY, &lv_font_montserrat_12, 0x64748B);
    updateFormatMenu();
}

void CalculationApp::updateFormatMenu() {
    if (!_formatMenu) return;
#if defined(__cpp_exceptions)
    try {
#endif
    const bool spanish = numos::i18n::isSpanish(numos::i18n::productLocale);
    lv_label_set_text_fmt(_formatMenuCounter, "%u / %u", _formatChoice + 1, _formatChoiceCount);
    const unsigned visible = std::min<unsigned>(_unitMenu?3:5, _formatChoiceCount);
    const unsigned first = _formatChoice < visible ? 0 : _formatChoice - visible+1;
    for (unsigned slot = 0; slot < 5; ++slot) {
        auto* row = _formatMenuRows[slot];
        const unsigned i = first + slot;
        if (slot>=visible || i >= _formatChoiceCount) { lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
        const bool focused = i == _formatChoice;
        lv_obj_set_style_bg_color(row, lv_color_hex(0x245DB2), 0);
        lv_obj_set_style_bg_opa(row, focused ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if(_formatPickingUnit) {
            auto& menu=*_unitMenu;auto& canvas=menu.canvases[slot];
            const unsigned n=menu.choices.count;
            const bool preview=_formatPickingComponents || i<=n;
            lv_obj_set_style_bg_color(row,lv_color_hex(0xFFF0D3),0);
            if(preview) {
                const numos::toolbox::Entry* entry=nullptr;
                numos::quantity::Descriptor descriptor=_outputUnit;
                if(_formatPickingComponents) {
                    descriptor={};descriptor.count=1;descriptor.terms[0]=_outputUnit.terms[i];
                } else if(i) {
                    const auto id=menu.choices.ids[i-1];entry=numos::toolbox::find(id);
                    if(!entry || !numos::quantity::descriptor(uint16_t(entry->argument),uint8_t(id.variant),descriptor))return;
                }
                if(menu.rows[slot]!=int(i)) {
                    // Common compose/layout machinery, only for the three
                    // visible rows. Remove the illustrative coefficient to
                    // preview the exact unit structure without manual spacing.
                    auto composed=numos::quantity::compose(vpam::makeNumber("1"),descriptor);
                    auto node=static_cast<vpam::NodeRow*>(composed.get())->removeChild(2);
                    auto root=vpam::makeRow();static_cast<vpam::NodeRow*>(root.get())->appendChild(std::move(node));
                    canvas.setExpression(nullptr,nullptr);canvas.setMathStyle(vpam::MathStyle::SCRIPT);
                    menu.previews[slot]=std::move(root);menu.rows[slot]=int(i);menu.previewModes[slot]=0;
                    canvas.setExpression(static_cast<vpam::NodeRow*>(menu.previews[slot].get()),nullptr);
                    const auto& box=menu.previews[slot]->layout();
                    if(box.width+16>88 || box.height()+2>40) {
                        // WHY: a long compound must not lose components at the
                        // preview edge. Reuse the row's full width and the MATH
                        // script style; extremely long forms retain their name.
                        menu.previewModes[slot]=1;canvas.setMathStyle(vpam::MathStyle::SCRIPTSCRIPT);
                        const auto& small=menu.previews[slot]->layout();
                        if(small.width+16>278 || small.height()+2>28)menu.previewModes[slot]=2;
                    }
                }
                const auto previewMode=menu.previewModes[slot];
                if(previewMode==2)lv_obj_add_flag(canvas.obj(),LV_OBJ_FLAG_HIDDEN);
                else lv_obj_remove_flag(canvas.obj(),LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(canvas.obj(),0,previewMode==1?12:0);
                lv_obj_set_size(canvas.obj(),previewMode==1?278:88,previewMode==1?28:40);
                lv_obj_set_style_text_font(_formatMenuLabels[slot],previewMode==1?ui::tutorFont12():ui::tutorFont14(),0);
                lv_obj_set_pos(_formatMenuLabels[slot],previewMode?9:88,previewMode==1?0:12);
                lv_obj_set_width(_formatMenuLabels[slot],previewMode?240:157);
                if(_formatPickingComponents) {
                    const auto* d=numos::units::definition(descriptor.terms[0].atom.unit);
                    lv_label_set_text(_formatMenuLabels[slot],spanish?d->es:d->en);
                } else lv_label_set_text(_formatMenuLabels[slot],entry?numos::toolbox::displayName(*entry):(spanish?"Unidad actual":"Current unit"));
            } else {
                lv_obj_add_flag(canvas.obj(),LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_font(_formatMenuLabels[slot],ui::tutorFont14(),0);
                lv_obj_set_pos(_formatMenuLabels[slot],9,12);lv_obj_set_width(_formatMenuLabels[slot],240);
                const char* en[]={"All units","By components","Default SI"};
                const char* es[]={"Todas las unidades","Por componentes","SI predeterminado"};
                lv_label_set_text(_formatMenuLabels[slot],spanish?es[i-n-1]:en[i-n-1]);
            }
        } else if (_formatPickingDigits) {
            const char* precisionLabel = spanish
                ? (i == 1 ? "%u decimal" : "%u decimales")
                : (i == 1 ? "%u decimal place" : "%u decimal places");
            lv_label_set_text_fmt(_formatMenuLabels[slot], precisionLabel, i);
        } else {
            const char* name = numos::calculationFormatLabel(_formatChoices[i], spanish);
            if (_formatChoices[i] == numos::CalculationFormat::Standard && complexResult())
                name = spanish ? "Cartesiana (a + bi)" : "Cartesian (a + bi)";
            if(_quantityView && _quantityView->hasUncertainty) {
                if(_formatChoices[i]==numos::CalculationFormat::Standard)name=spanish?"Valor e incertidumbre":"Value and uncertainty";
                if(_formatChoices[i]==numos::CalculationFormat::Decimal)name=spanish?"Más dígitos + u":"More digits + u";
            }
            lv_label_set_text(_formatMenuLabels[slot], name);
        }
        lv_obj_set_style_text_color(_formatMenuLabels[slot], lv_color_hex(focused && !_formatPickingUnit ? 0xFFFFFF : 0x344155), 0);
        lv_label_set_text(_formatMenuMarks[slot], _formatPickingUnit?(!_formatPickingComponents && i>0 && i<=_unitMenu->choices.favorites?"*":""):
            (_formatPickingDigits ? i == _fixedPlaces : _formatChoices[i] == _resultMode) ? LV_SYMBOL_OK : "");
        lv_obj_set_style_text_color(_formatMenuMarks[slot], lv_color_hex(focused && !_formatPickingUnit ? 0xFFFFFF : 0x245DB2), 0);
    }
    const unsigned height = 112 * visible / _formatChoiceCount;
    lv_obj_set_height(_formatMenuThumb, height);
    lv_obj_set_y(_formatMenuThumb, _formatChoiceCount > visible ?
                 (112-height)*first/(_formatChoiceCount-visible) : 0);
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){closeFormatMenu();ui::StatusBar::showActiveNotice(numos::i18n::isSpanish(numos::i18n::productLocale)?"Sin memoria":"Low memory");}
#endif
}

void CalculationApp::closeFormatMenu() {
    _unitMenu.reset();
    if (_formatMenu) lv_obj_delete(_formatMenu);
    _formatMenu = _formatMenuCounter = _formatMenuThumb = nullptr;
    for (unsigned i = 0; i < 5; ++i)
        _formatMenuRows[i] = _formatMenuLabels[i] = _formatMenuMarks[i] = nullptr;
}

void CalculationApp::applyFormatChoice() {
    if(_formatPickingUnit) {
        if(!_unitMenu || !_quantity || _unitMenu->generation!=_resultGeneration){closeFormatMenu();return;}
        const unsigned choice=_formatChoice,n=_unitMenu->choices.count;
        if(_formatPickingComponents){closeFormatMenu();openOutputSelector(int(choice));return;}
        const auto id=choice && choice<=n?_unitMenu->choices.ids[choice-1]:numos::toolbox::Identity{};
        if(!choice){closeFormatMenu();return;}
        if(choice<=n) {
            _unitTargetComponent=-1;_unitSelectionGeneration=_resultGeneration;
            if(selectOutput(id))closeFormatMenu();
            else ui::StatusBar::showActiveNotice(numos::i18n::isSpanish(numos::i18n::productLocale)?"Resultado conservado":"Result retained");
        } else if(choice==n+1){closeFormatMenu();openOutputSelector(-1);}
        else if(choice==n+2){closeFormatMenu();openFormatMenu(false,true,true);}
        else if(publishOutput(numos::quantity::coherent(*_quantity)))closeFormatMenu();
    } else if (_formatPickingDigits) {
        if(_quantity){const unsigned places=_formatChoice;closeFormatMenu();publishQuantityFormat(numos::CalculationFormat::Fixed,0,places);return;}
        _fixedPlaces = _formatChoice;
        closeFormatMenu();
        _resultMode = numos::CalculationFormat::Fixed;
        showResult();
    } else {
        const auto chosen = _formatChoices[_formatChoice];
        closeFormatMenu();
        selectFormat(chosen);
    }
}

bool CalculationApp::outputAllowed(const numos::toolbox::Entry& entry) const {
    using namespace numos;
    if(!_quantity || entry.recipe!=toolbox::Recipe::Unit)return false;
    quantity::Descriptor candidate;
    if(!quantity::descriptor(uint16_t(entry.argument),uint8_t(entry.identity.variant),candidate))return false;
    if(_unitTargetComponent<0)return quantity::compatible(*_quantity,candidate);
    if(unsigned(_unitTargetComponent)>=_outputUnit.count || candidate.count!=1 || candidate.terms[0].power!=1 ||
       candidate.numerator!=1 || candidate.denominator!=1)return false;
    auto composed=_outputUnit;composed.terms[_unitTargetComponent].atom=candidate.terms[0].atom;
    return quantity::compatible(*_quantity,composed);
}

void CalculationApp::openOutputSelector(int component) {
    if(!_quantity || !_hasResult || _lastStatus!=numos::MathEngineStatus::Ok)return;
    _unitTargetComponent=component;_unitSelectionGeneration=_resultGeneration;
    ui::toolbox::Receiver receiver{this,&_cursor,numos::toolbox::Calculation,nullptr,_mathCanvas.normalMetrics().style};
    receiver.filter=[](void* owner,const numos::toolbox::Entry& entry){return static_cast<CalculationApp*>(owner)->outputAllowed(entry);};
    receiver.selected=[](void* owner,numos::toolbox::Identity id){return static_cast<CalculationApp*>(owner)->selectOutput(id);};
    receiver.initialGroup=200;
    receiver.filterUnitFamilies=true;
    ui::toolbox::open(_screen,receiver);
}

bool CalculationApp::selectOutput(numos::toolbox::Identity id) {
    using namespace numos;
    if(_unitSelectionGeneration!=_resultGeneration || !_hasResult || _lastStatus!=MathEngineStatus::Ok)return false;
    const auto* entry=toolbox::find(id);if(!entry || !outputAllowed(*entry))return false;
    quantity::Descriptor selected;if(!quantity::descriptor(uint16_t(entry->argument),uint8_t(id.variant),selected))return false;
    if(_unitTargetComponent>=0){auto candidate=_outputUnit;candidate.terms[_unitTargetComponent].atom=selected.terms[0].atom;selected=candidate;}
    return publishOutput(selected);
}

bool CalculationApp::publishOutput(const numos::quantity::Descriptor& descriptor) {
    if(!_quantity || !_hasResult || !numos::quantity::compatible(*_quantity,descriptor))return false;
#if defined(__cpp_exceptions)
    try {
#endif
        auto view=std::make_unique<numos::quantity::Display>();
        if(numos::quantity::convert(*_quantity,descriptor,*view)!=numos::quantity::Error::None)return false;
        auto mode=_resultMode;
        if(mode==numos::CalculationFormat::Decimal && !view->approximateCoefficient)mode=numos::CalculationFormat::Standard;
        auto prepared=quantityFormatted(*view,descriptor,mode,_engineeringShift,_fixedPlaces);
        if(!prepared)return false;
        prepared->calculateLayout(_resultCanvas.normalMetrics());
        const auto box=prepared->layout();if(box.width>4096 || box.height()>1024)return false;
        // WHY: coefficient, unit and AST are complete before any visible state
        // changes. The canonical value and Ans never participate in this commit.
        _quantityView=std::move(view);_outputUnit=descriptor;_resultMode=mode;
        showResult(std::move(prepared));return true;
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){return false;}
#endif
}

bool CalculationApp::publishQuantityFormat(numos::CalculationFormat mode,int shift,unsigned places) {
    if(!_quantityView)return false;
#if defined(__cpp_exceptions)
    try {
#endif
        auto prepared=quantityFormatted(*_quantityView,_outputUnit,mode,shift,places);
        if(!prepared)return false;
        prepared->calculateLayout(_resultCanvas.normalMetrics());
        const auto box=prepared->layout();if(box.width>4096 || box.height()>1024)return false;
        _resultMode=mode;_engineeringShift=shift;_fixedPlaces=places;
        _showDecimal=mode==numos::CalculationFormat::Decimal;
        showResult(std::move(prepared));return true;
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){return false;}
#endif
}
