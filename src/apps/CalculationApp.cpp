/**
 * CalculationApp.cpp — Calculadora V.P.A.M. con LVGL 9.5
 *
 * Integración completa de las cuatro fases del motor matemático:
 *   Fase 1: AST dinámico (MathAST) ✓
 *   Fase 2: Cursor + Inserción VPAM (CursorController) ✓
 *   Fase 3: Renderizado LVGL pixel-perfect (MathCanvas) ✓
 *   Fase 4: Evaluador Simbólico + Decimal (MathEvaluator) ✓
 *
 * La pantalla es 100% LVGL (no TFT directo). Dos widgets MathCanvas:
 *   · Expresión (arriba): editable, con cursor parpadeante
 *   · Resultado (abajo):  solo lectura, muestra tras ENTER
 *
 * S⇔D (FREE_EQ): alterna entre resultado exacto (fracción/radical)
 * y decimal (double formateado).
 */

#include "CalculationApp.h"
#include "../input/KeyCodes.h"
#include "../Config.h"
#include "../math/cas/ASTFlattener.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/SymExprToAST.h"
#ifdef NATIVE_SIM
  #include <cstdio>
#endif

// ── Colores ──
static constexpr uint32_t COL_BG_HEX     = 0xFFFFFF;   // Blanco puro
static constexpr uint32_t COL_SEP_HEX    = 0x333333;   // Gris separador

// ── Dimensiones ──
static constexpr int SCREEN_W    = 320;
static constexpr int SCREEN_H    = 240;
static constexpr int BAR_H       = ui::StatusBar::HEIGHT + 1;  // 24 + 1 separador
static constexpr int PAD          = 6;     // Margen de seguridad en todos los bordes
static constexpr int CANVAS_Y    = BAR_H;
static constexpr int EXPR_H      = 98;     // Altura zona expresión
static constexpr int SEP_Y       = CANVAS_Y + EXPR_H;
static constexpr int RESULT_Y    = SEP_Y + 1;
static constexpr int RESULT_H    = SCREEN_H - RESULT_Y;

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

CalculationApp::CalculationApp()
    : _screen(nullptr)
    , _resultSep(nullptr)
    , _rootRow(nullptr)
    , _hasResult(false)
    , _showDecimal(false)
    , _resultMode(vpam::ResultMode::Symbolic)
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
    }

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
    // ── Pantalla ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── StatusBar global (24 px + 1 px separador) ──
    _statusBar.create(_screen);
    _statusBar.setTitle("Calculation");
    _statusBar.setBatteryLevel(100);

    // ── MathCanvas — Expresión (zona superior, con padding lateral) ──
    _mathCanvas.create(_screen);
    lv_obj_set_pos(_mathCanvas.obj(), PAD, CANVAS_Y);
    lv_obj_set_size(_mathCanvas.obj(), SCREEN_W - 2 * PAD, EXPR_H);

    // ── Línea separadora expr↔resultado (#333) ──
    _resultSep = lv_obj_create(_screen);
    lv_obj_set_size(_resultSep, SCREEN_W - 2 * PAD, 1);
    lv_obj_set_pos(_resultSep, PAD, SEP_Y);
    lv_obj_set_style_bg_color(_resultSep, lv_color_hex(COL_SEP_HEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_resultSep, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultSep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_resultSep, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_resultSep, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_SCROLLABLE);
    // Ocultar separador hasta que haya resultado
    lv_obj_add_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);

    // ── MathCanvas — Resultado (zona inferior, con padding lateral) ──
    _resultCanvas.create(_screen);
    lv_obj_set_pos(_resultCanvas.obj(), PAD, RESULT_Y);
    lv_obj_set_size(_resultCanvas.obj(), SCREEN_W - 2 * PAD, RESULT_H - PAD);
    // Ocultar canvas de resultado hasta que se evalúe
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

    auto& km = vpam::KeyboardManager::instance();

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
        case KeyCode::DOT:   clearResult(); _cursor.insertDigit('.'); break;

        // ── Operadores ──
        case KeyCode::ADD: clearResult(); _cursor.insertOperator(vpam::OpKind::Add); break;
        case KeyCode::SUB: clearResult(); _cursor.insertOperator(vpam::OpKind::Sub); break;
        case KeyCode::MUL: clearResult(); _cursor.insertOperator(vpam::OpKind::Mul); break;

        // ── Estructuras VPAM ──
        case KeyCode::DIV:    clearResult(); _cursor.insertFraction(); break;
        case KeyCode::POW:    clearResult(); _cursor.insertPower();    break;
        case KeyCode::SQRT:   clearResult(); _cursor.insertRoot();     break;
        case KeyCode::LPAREN: clearResult(); _cursor.insertParen();    break;

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
            clearResult();
            _cursor.insertFunction(vpam::FuncKind::Log);
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
            if (_hasResult && _resultMode == vpam::ResultMode::Extended) {
                _resultCanvas.scrollBy(20);
                changed = false;
            } else {
                _cursor.moveLeft();
            }
            break;
        case KeyCode::RIGHT:
            if (_hasResult && _resultMode == vpam::ResultMode::Extended) {
                _resultCanvas.scrollBy(-20);
                changed = false;
            } else {
                _cursor.moveRight();
            }
            break;
        case KeyCode::UP:
            if (_hasResult || _historyIndex >= 0) {
                // Navegar historial: ir a la entrada anterior
                navigateHistory(-1);
                changed = false;
            } else {
                _cursor.moveUp();
            }
            break;
        case KeyCode::DOWN:
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
            if (_hasResult) {
                toggleSD();
            }
            changed = false;
            break;

        // ── FACT: factorización en primos ──
        case KeyCode::FACT:
            if (_hasResult && _lastResult.ok) {
                _resultNode = vpam::MathEvaluator::factorizeToAST(_lastResult);
                _resultRow  = static_cast<vpam::NodeRow*>(_resultNode.get());
                _resultRow->calculateLayout(_resultCanvas.normalMetrics());
                _resultCanvas.setExpression(_resultRow, nullptr);
                _resultCanvas.resetScroll();
                _resultCanvas.invalidate();
                // Mostrar canvas de resultado
                if (_resultSep)  lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
                if (_resultCanvas.obj()) lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
            }
            changed = false;
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
    if (!_rootRow) return;

    // Evaluar
    _lastResult  = _evaluator.evaluate(_rootRow);
    _hasResult   = true;
    _showDecimal = false;
    _resultMode  = vpam::ResultMode::Symbolic;

    // Guardar en Ans (rota Ans → PreAns)
    if (_lastResult.ok) {
        vpam::VariableManager::instance().updateAns(_lastResult);
    }

    // ── Guardar en historial ─────────────────────────────────────────
    {
        HistoryEntry entry;
        entry.exprAST = vpam::cloneNode(_rootRow);
        entry.result  = _lastResult;
        _history.push_back(std::move(entry));

        // Limitar historial
        if (static_cast<int>(_history.size()) > MAX_HISTORY) {
            _history.erase(_history.begin());
        }
    }
    _historyIndex = -1;  // volver a modo "nueva expresión"

    // Ocultar cursor de la expresión
    _mathCanvas.stopCursorBlink();

    // Generate educational steps if enabled
    _eduStepLogger.clear();
    _eduArena.reset();
    _hasEduSteps = false;
    if (setting_edu_steps && _lastResult.ok) {
        generateEduSteps();
    }

    // Mostrar resultado
    showResult();
}

// ════════════════════════════════════════════════════════════════════════════
// showResult() — Genera y muestra el AST del resultado (3 estados)
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::showResult() {
    // Generar el AST del resultado según el estado
    switch (_resultMode) {
        case vpam::ResultMode::Symbolic:
            _resultNode = vpam::MathEvaluator::resultToAST(_lastResult);
            break;
        case vpam::ResultMode::Periodic:
            _resultNode = vpam::MathEvaluator::resultToPeriodicAST(_lastResult);
            break;
        case vpam::ResultMode::Extended:
            _resultNode = vpam::MathEvaluator::resultToExtendedAST(_lastResult, 200);
            break;
    }
    _resultRow = static_cast<vpam::NodeRow*>(_resultNode.get());

    // Conectar al canvas de resultado (sin cursor)
    _resultCanvas.setExpression(_resultRow, nullptr);
    _resultCanvas.resetScroll();   // Resetear scroll al cambiar de estado

    // Calcular layout del resultado
    _resultRow->calculateLayout(_resultCanvas.normalMetrics());

    // Hacer visibles separador y canvas de resultado
    lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);

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

    _hasResult = false;
    _resultNode.reset();
    _resultRow = nullptr;

    // Clear educational steps
    _eduStepLogger.clear();
    _eduArena.reset();
    _hasEduSteps = false;

    // Ocultar separador y canvas de resultado
    if (_resultSep) lv_obj_add_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
    if (_resultCanvas.obj()) lv_obj_add_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);

    // Restore title
    _statusBar.setTitle("Calculation");

    // Restaurar cursor
    _mathCanvas.startCursorBlink();
}

// ════════════════════════════════════════════════════════════════════════════
// toggleSD() — Rota entre 3 estados: Simbólico → Periódico → Extendido → ...
// ════════════════════════════════════════════════════════════════════════════

void CalculationApp::toggleSD() {
    switch (_resultMode) {
        case vpam::ResultMode::Symbolic:
            _resultMode = vpam::ResultMode::Periodic;
            break;
        case vpam::ResultMode::Periodic:
            _resultMode = vpam::ResultMode::Extended;
            break;
        case vpam::ResultMode::Extended:
            _resultMode = vpam::ResultMode::Symbolic;
            break;
    }
    _showDecimal = (_resultMode != vpam::ResultMode::Symbolic);
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
    if (index < 0 || index >= static_cast<int>(_history.size())) return;

    _historyIndex = index;
    auto& entry = _history[index];

    // Clonar el AST de la expresión (el original se queda en el historial)
    _rootNode = vpam::cloneNode(entry.exprAST.get());
    _rootRow  = static_cast<vpam::NodeRow*>(_rootNode.get());

    // Inicializar cursor al final de la expresión cargada
    _cursor.init(_rootRow);

    // Mostrar en el canvas de expresión
    _mathCanvas.setExpression(_rootRow, &_cursor);
    _mathCanvas.resetScroll();
    refreshExpression();
    _mathCanvas.startCursorBlink();

    // Mostrar el resultado almacenado
    _lastResult  = entry.result;
    _hasResult   = true;
    _showDecimal = false;
    _resultMode  = vpam::ResultMode::Symbolic;
    showResult();
}

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
    auto& vm = vpam::VariableManager::instance();
    vpam::ExactVal ans = vm.getAns();
    vm.setVariable(varName, ans);

    // Intentar persistir en flash
    vm.saveToFlash();
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
        if (_resultCanvas.obj()) lv_obj_remove_flag(_resultCanvas.obj(), LV_OBJ_FLAG_HIDDEN);
        if (_resultSep) lv_obj_remove_flag(_resultSep, LV_OBJ_FLAG_HIDDEN);
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
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
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
            lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(descLbl, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
        }

        // Reason label (right side / below)
        if (!step.reason.empty()) {
            lv_obj_t* reasonLbl = lv_label_create(_stepsContainer);
            char reasonBuf[128];
            snprintf(reasonBuf, sizeof(reasonBuf), "  " LV_SYMBOL_RIGHT " %s",
                     step.reason.c_str());
            lv_label_set_text(reasonLbl, reasonBuf);
            lv_obj_set_style_text_font(reasonLbl, &lv_font_montserrat_12, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(0x808080), LV_PART_MAIN);
}