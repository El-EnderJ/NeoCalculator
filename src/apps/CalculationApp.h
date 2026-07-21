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
 * CalculationApp.h — Calculadora V.P.A.M. con LVGL 9.5
 *
 * Integración de las Fases 1-4 del Motor Matemático:
 *   · MathAST           — Árbol de sintaxis abstracta (la expresión)
 *   · CursorController  — Cursor estructural + inserción VPAM
 *   · MathCanvas        — Renderizado LVGL pixel-perfect
 *   · CalculationEngine — Autoridad Giac estructurada
 *   · MathEvaluator     — Conversión de resultado para S⇔D
 *
 * La app crea una pantalla LVGL propia con:
 *   · Header naranja con título "Calculation"
 *   · Zona superior con MathCanvas (la expresión en formato 2D)
 *   · Línea separadora entre expresión y resultado
 *   · Zona inferior con MathCanvas para el resultado
 *
 * Educational Mode (setting_edu_steps):
 *   · When enabled, arithmetic expressions are broken down step-by-step
 *   · Uses CAS SymSimplify in atomic mode (one transformation at a time)
 *   · F2: View Steps — opens scrollable step viewer
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
#include <memory>
#include <string>
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/MathEvaluator.h"
#include "../math/VariableManager.h"
// Giac is the sole Calculation answer engine. MathEvaluator remains included
// only for the presentation helpers used by ExactVal/S<=>D rendering.
#include "../math/CalculationEngine.h"
#include "../math/cas/CASStepLogger.h"
#include "../math/cas/SymExpr.h"
#include "../math/cas/SymExprArena.h"
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

#ifdef NATIVE_SIM
    // ── Sonda de prueba (SOLO emulador, read-only) ────────────────────────
    // Expone el último resultado evaluado para las aserciones semánticas del
    // runner de scripts .numos (NativeHal::scriptStepBegin). Son accesores
    // const sin asignación de memoria que devuelven miembros ya existentes;
    // NO alteran el comportamiento ni la geometría del render.
    //
    // Firmware-neutro: NATIVE_SIM solo está definido en [env:emulator_pc]
    // (platformio.ini), nunca en [env:esp32s3_n16r8], así que todo este bloque
    // queda fuera del preprocesador en firmware → firmware.bin no cambia.
    bool                  debugHasResult() const { return _hasResult; }
    const vpam::ExactVal& debugLastResult() const { return _lastResult; }

    // ── GIAC-B01 probes (assert_calc_engine / _result_kind / _status) ────
    const char* debugCalcEngine() const { return "giac"; }
    const char* debugCalcResultKind() const;   // "structured"|"text_fallback"|"none"
    const char* debugCalcStatus() const;       // "ok"|"undefined"|"parse_error"|...
    const std::string& debugCalcExactText() const;
#endif // NATIVE_SIM

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
    // ── Estado del resultado ─────────────────────────────────────────────
    bool                   _hasResult;       ///< Hay un resultado visible
    bool                   _showDecimal;     ///< Legacy (conservado)
    vpam::ResultMode       _resultMode;      ///< 3-state: Symbolic/Periodic/Extended
    vpam::ExactVal         _lastResult;      ///< Último resultado evaluado
    vpam::NodePtr          _resultNode;      ///< AST del resultado (owned)
    vpam::NodeRow*         _resultRow;       ///< Puntero directo al NodeRow del resultado

    // ── GIAC-B01: estado de presentación del motor Giac ──────────────────
    // _lastResult sigue siendo el espejo ExactVal (tier 1 exacto, o numérico
    // aproximado); estos campos llevan el estado que ExactVal no puede.
    numos::MathEngineStatus _lastStatus = numos::MathEngineStatus::Ok;
    numos::CalcResultKind   _lastKind   = numos::CalcResultKind::None;
    bool                    _exactValValid = false;  ///< tier 1 (display legacy)
    vpam::NodePtr           _structuredResult;       ///< tier 2 master AST
    std::string             _exactText;              ///< Giac exact print
    std::string             _approxText;             ///< companion decimal
    lv_obj_t*               _resultTextLabel = nullptr;  ///< tier 3 fallback

    // ── Historial ────────────────────────────────────────────────────────
    struct HistoryEntry {
        vpam::NodePtr  exprAST;     ///< Copia profunda del AST de la expresión
        vpam::ExactVal result;      ///< Resultado evaluado
        // GIAC-B01 presentation state (defaults reproduce the legacy shape)
        numos::MathEngineStatus status = numos::MathEngineStatus::Ok;
        numos::CalcResultKind   kind   = numos::CalcResultKind::Structured;
        bool                    exactValValid = true;
        vpam::NodePtr           resultAST;   ///< tier-2 clone (may be null)
        std::string             exactText;
        std::string             approxText;
    };
    std::vector<HistoryEntry> _history;      ///< Entradas de historial
    int  _historyIndex;                      ///< -1 = nueva expresión, 0..N-1 = historial
    static constexpr int MAX_HISTORY = 50;   ///< Máximo de entradas guardadas

    // ── Educational step-by-step mode ────────────────────────────────────
    cas::CASStepLogger     _eduStepLogger;   ///< Step logger for educational mode
    cas::SymExprArena      _eduArena;        ///< Arena for CAS expressions
    bool                   _hasEduSteps;     ///< Steps available for viewing

    // Step viewer UI
    bool                   _stepViewerActive; ///< Step viewer is visible
    lv_obj_t*              _stepsContainer;   ///< Scrollable step list container

    struct StepRenderData {
        vpam::NodePtr    nodeData;   ///< Owns the MathAST tree
        vpam::MathCanvas canvas;     ///< LVGL widget for 2D rendering
    };
    std::vector<std::unique_ptr<StepRenderData>> _stepRenderers;

    // ── Estado ───────────────────────────────────────────────────────────
    // (KeyboardManager singleton gestiona SHIFT/ALPHA/LOCK/STO)

    // ── Helpers ──────────────────────────────────────────────────────────
    void createUI();
    void refreshExpression();
    void resetExpression();
    void evaluateExpression();
    void showResult();
    void clearResult();

    /// Dynamically repositions the separator and result canvas after
    /// trimming the expression canvas to its actual content height.
    void applyResultLayout();

    // ── GIAC-B01 text-fallback presentation (tier 3) ─────────────────────
    /// Shows Giac's own printed result as plain text (montserrat label —
    /// stix_math has no space glyph) when no structured conversion exists.
    void showTextResult(const std::string& text);
    void hideTextResult();
    void toggleSD();
    void navigateHistory(int direction);  ///< -1 = arriba (atrás), +1 = abajo (reciente)
    void loadHistoryEntry(int index);     ///< Carga una entrada del historial en el canvas

    /// Mapea una tecla numérica al char de variable Alpha correspondiente
    /// (en modo ALPHA: NUM_1→A, NUM_2→B, ..., NUM_6→F, VAR_X→x, VAR_Y→y)
    static char alphaKeyToVarName(KeyCode code);

    /// Ejecuta la acción STO: guarda Ans en la variable indicada
    void executeStore(char varName);

    // ── Educational step-by-step helpers ─────────────────────────────────
    void generateEduSteps();          ///< Generate step-by-step breakdown
    void openStepViewer();            ///< Show step viewer screen
    void closeStepViewer();           ///< Return from step viewer
    void buildStepsDisplay();         ///< Build LVGL step list items
};
