/**
 * CalculationApp.h — Calculadora V.P.A.M. con LVGL 9.5
 *
 * Integración de las Fases 1-4 del Motor Matemático:
 *   · MathAST           — Árbol de sintaxis abstracta (la expresión)
 *   · CursorController  — Cursor estructural + inserción VPAM
 *   · MathCanvas        — Renderizado LVGL pixel-perfect
 *   · MathEvaluator     — Evaluador simbólico + decimal (S⇔D)
 *
 * La app crea una pantalla LVGL propia con:
 *   · Header naranja con título "Calculation"
 *   · Zona superior con MathCanvas (la expresión en formato 2D)
 *   · Línea separadora entre expresión y resultado
 *   · Zona inferior con MathCanvas para el resultado
 *
 * Flujo:
 *   1. begin() → crea pantalla LVGL + 2 MathCanvas + AST raíz
 *   2. handleKey() → traduce KeyCode a acciones del CursorController
 *   3. El CursorController modifica el AST
 *   4. ENTER → evalúa el AST y muestra resultado (modo S por defecto)
 *   5. FREE_EQ (S⇔D) → alterna entre resultado exacto y decimal
 *   6. Cualquier input nuevo con resultado visible → limpia resultado
 */

#pragma once

#include <lvgl.h>
#include <vector>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/MathEvaluator.h"
#include "../math/VariableManager.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class CalculationApp {
public:
    CalculationApp();
    ~CalculationApp();

    /**
     * Crea la pantalla LVGL y los widgets.
     * Debe llamarse DESPUÉS de lv_init() y del registro del display.
     */
    void begin();

    /**
     * Destruye la pantalla LVGL y libera recursos.
     * Llamar al volver al menú.
     */
    void end();

    /**
     * Procesa un evento de teclado.
     * Traduce KeyCodes a operaciones del CursorController.
     */
    void handleKey(const KeyEvent& ev);

    /**
     * Carga la pantalla LVGL de la calculadora (la hace visible).
     */
    void load();

    /**
     * Indica si la pantalla LVGL está creada y activa.
     */
    bool isActive() const { return _screen != nullptr; }

private:
    // ── LVGL UI ──────────────────────────────────────────────────────────
    lv_obj_t*          _screen;        ///< Pantalla LVGL propia
    ui::StatusBar      _statusBar;     ///< Barra de estado global (24 px)
    lv_obj_t*          _resultSep;     ///< Línea separadora expr↔resultado
    vpam::MathCanvas   _mathCanvas;    ///< Canvas de la expresión (arriba)
    vpam::MathCanvas   _resultCanvas;  ///< Canvas del resultado (abajo)

    // ── Motor VPAM ───────────────────────────────────────────────────────
    vpam::NodePtr              _rootNode;    ///< Nodo raíz del AST (owned)
    vpam::NodeRow*             _rootRow;     ///< Puntero directo al NodeRow raíz
    vpam::CursorController     _cursor;      ///< Controlador de cursor
    vpam::MathEvaluator        _evaluator;   ///< Evaluador simbólico

    // ── Estado del resultado ─────────────────────────────────────────────
    bool                   _hasResult;       ///< Hay un resultado visible
    bool                   _showDecimal;     ///< Legacy (conservado)
    vpam::ResultMode       _resultMode;      ///< 3-state: Symbolic/Periodic/Extended
    vpam::ExactVal         _lastResult;      ///< Último resultado evaluado
    vpam::NodePtr          _resultNode;      ///< AST del resultado (owned)
    vpam::NodeRow*         _resultRow;       ///< Puntero directo al NodeRow del resultado

    // ── Historial ────────────────────────────────────────────────────────
    struct HistoryEntry {
        vpam::NodePtr  exprAST;     ///< Copia profunda del AST de la expresión
        vpam::ExactVal result;      ///< Resultado evaluado
    };
    std::vector<HistoryEntry> _history;      ///< Entradas de historial
    int  _historyIndex;                      ///< -1 = nueva expresión, 0..N-1 = historial
    static constexpr int MAX_HISTORY = 50;   ///< Máximo de entradas guardadas

    // ── Estado ───────────────────────────────────────────────────────────
    // (KeyboardManager singleton gestiona SHIFT/ALPHA/LOCK/STO)

    // ── Helpers ──────────────────────────────────────────────────────────
    void createUI();
    void refreshExpression();
    void resetExpression();
    void evaluateExpression();
    void showResult();
    void clearResult();
    void toggleSD();
    void navigateHistory(int direction);  ///< -1 = arriba (atrás), +1 = abajo (reciente)
    void loadHistoryEntry(int index);     ///< Carga una entrada del historial en el canvas

    /// Mapea una tecla numérica al char de variable Alpha correspondiente
    /// (en modo ALPHA: NUM_1→A, NUM_2→B, ..., NUM_6→F, VAR_X→x, VAR_Y→y)
    static char alphaKeyToVarName(KeyCode code);

    /// Ejecuta la acción STO: guarda Ans en la variable indicada
    void executeStore(char varName);
};