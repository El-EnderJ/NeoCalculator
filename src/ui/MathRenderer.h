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
 * MathRenderer.h — Motor de Renderizado LVGL 9.5 para el AST Matemático
 *
 * Fase 3 del Motor V.P.A.M.
 *
 * MathCanvas es un widget LVGL personalizado que recorre el AST
 * (MathAST.h) y dibuja la expresión matemática pixel-perfect usando
 * las APIs de dibujo de LVGL 9.x:
 *
 *   · lv_draw_label()  → texto (números, operadores)
 *   · lv_draw_line()   → barras de fracción, radical (√)
 *   · lv_draw_rect()   → placeholders (NodeEmpty), cursor
 *
 * Cursor:
 *   Línea vertical de 2 px que parpadea (lv_anim_t, 500 ms on/off).
 *   Su altura se ajusta al ascent+descent del NodeRow donde se encuentra.
 *
 * Integración:
 *   1. Crear MathCanvas sobre un lv_obj padre.
 *   2. Llamar setExpression(root, cursorController).
 *   3. Tras cada cambio en el AST, recalcular layout e invalidar:
 *        root->calculateLayout(canvas.normalMetrics());
 *        canvas.invalidate();
 *
 * Dependencias: LVGL 9.x, MathAST.h, CursorController.h.
 */

#pragma once

#include <lvgl.h>
#include "../math/MathAST.h"
#include "../math/CursorController.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// MathCanvas — Widget LVGL de Renderizado Matemático
// ════════════════════════════════════════════════════════════════════════════
class MathCanvas {
public:
    MathCanvas();
    ~MathCanvas();

    // ── Creación ─────────────────────────────────────────────────────────

    /**
     * Crea el widget sobre un padre LVGL.
     * @param parent  Contenedor LVGL (screen, panel, etc.).
     */
    void create(lv_obj_t* parent);

    /**
     * Destruye el widget y limpia recursos.
     */
    void destroy();

    // ── Datos ────────────────────────────────────────────────────────────

    /**
     * Conecta el AST y el controlador de cursor al canvas.
     * @param root    Fila raíz del AST (la expresión completa).
     * @param ctrl    Controlador de cursor (proporciona posición del cursor).
     */
    void setExpression(NodeRow* root, const CursorController* ctrl);

    void setMathStyle(MathStyle style);
    void setAutoHeightEnabled(bool enabled);
    void setTraceLabel(const char* label);

    /**
     * Fuerza el redibujado completo del widget.
     * Llamar después de modificar el AST o mover el cursor.
     */
    void invalidate();

    // ── Smart Highlighter ────────────────────────────────────────────────

    /**
     * Marks a specific VPAM node for colour-highlighted rendering.
     * During the next draw pass, that node and all its children are drawn in
     * @p color instead of the default black.
     * Use lv_color_hex(0x1565C0) for intermediate steps (blue) and
     * lv_color_hex(0xE05500) for the final result (orange).
     * @param node   VPAM MathNode* to highlight (raw pointer, must belong to
     *               the tree currently set with setExpression).
     * @param color  Highlight colour.
     */
    void setHighlightNode(const MathNode* node, lv_color_t color);

    /**
     * Clears any active highlight, restoring normal black rendering.
     */
    void clearHighlightNode();

    // ── Cursor ───────────────────────────────────────────────────────────

    /** Inicia la animación de parpadeo del cursor */
    void startCursorBlink();

    /** Detiene el parpadeo (cursor oculto) */
    void stopCursorBlink();

    /** Reinicia el ciclo de parpadeo (cursor visible inmediatamente) */
    void resetCursorBlink();

    // ── Acceso ───────────────────────────────────────────────────────────

    /** Objeto LVGL subyacente */
    lv_obj_t* obj() const { return _obj; }

    /** FontMetrics para la fuente normal (STIX Two Math 18) */
    const FontMetrics& normalMetrics() const { return _fmNormal; }

    /** FontMetrics para la fuente secundaria/superscript (STIX Two Math 12) */
    const FontMetrics& smallMetrics() const { return _fmSmall; }

    /**
     * Desplaza manualmente el contenido horizontalmente.
     * @param delta  Cantidad de píxeles (negativo = izquierda, positivo = derecha).
     */
    void scrollBy(int16_t delta);

    /** Resetea el scroll horizontal a 0 */
    void resetScroll() { _scrollX = 0; }

    // ── Utilidad estática ────────────────────────────────────────────────

    /**
     * Extrae FontMetrics de una lv_font_t de LVGL.
     * Mide el glyph '0' para obtener charWidth.
     */
    static FontMetrics metricsFromFont(const lv_font_t* font);

    // ── Layout constants (public for dynamic UI sizing) ────────────────────
    static constexpr int16_t  VPAM_VERT_PAD   = 6;    ///< Vertical padding (3px top + 3px bottom) — tight

private:
    // ── Datos internos ───────────────────────────────────────────────────
    lv_obj_t*               _obj;
    NodeRow*                _root;
    const CursorController* _cursorCtrl;

    // Fuentes LVGL
    const lv_font_t*  _fontNormal;     // STIX Two Math 18
    const lv_font_t*  _fontSmall;      // STIX Two Math 12 (super/subscript)
    const lv_font_t*  _fontScriptScript; // STIX Two Math 8 (nested scripts)
    FontMetrics        _fmNormal;
    FontMetrics        _fmSmall;
    FontMetrics        _fmScriptScript;

    // ── Cursor visual ────────────────────────────────────────────────────
    lv_timer_t* _cursorTimer = nullptr;  ///< Reliable 500ms toggle timer
    bool        _cursorEditable = false; ///< Explicit edit-mode ownership gate
    bool        _cursorVisible = true;
    int16_t    _cursorX;        // Posición X absoluta (px)
    int16_t    _cursorY;        // Posición Y absoluta (px, tope)
    int16_t    _cursorH;        // Altura del cursor (px)

    // ── Scroll horizontal ────────────────────────────────────────────────
    int16_t    _scrollX;        // Desplazamiento horizontal (≤0)
    bool       _autoHeightEnabled;

    // ── Smart Highlighter ────────────────────────────────────────────────
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    // Fixed scalar state for one-shot hardware coordinate traces.
    const char* _traceLabel;
    const MathNode* _traceLastRoot;
    int16_t _traceLastObjX1;
    int16_t _traceLastObjY1;
    int16_t _traceLastObjX2;
    int16_t _traceLastObjY2;
    int16_t _traceLastWidgetW;
    int16_t _traceLastWidgetH;
    int16_t _traceLastRootW;
    int16_t _traceLastRootH;
    int16_t _traceLastRootAscent;
    int16_t _traceLastRootDescent;
    int16_t _traceLastBaseline;
    int16_t _traceLastYTop;
    int16_t _traceLastDrawX;
    int16_t _traceLastScrollX;
    uint16_t _traceClipSamples;
    bool _traceSeen;
    bool _traceCursorSeen;
    bool _traceCursorEditable;
    bool _traceCursorBlinkVisible;
    bool _traceCursorClamped;
    const NodeRow* _traceCursorRow;
    int16_t _traceCursorIndex;
    int16_t _traceCursorRootAscent;
    int16_t _traceCursorRootDescent;
    int16_t _traceCursorRowAscent;
    int16_t _traceCursorRowDescent;
    int16_t _traceCursorRowBaseline;
    int16_t _traceCursorRowYTop;
    int16_t _traceCursorFmAscent;
    int16_t _traceCursorFmDescent;
    int16_t _traceCursorFmLineAscent;
    int16_t _traceCursorFmLineDescent;
    uint8_t _traceCursorScriptLevel;
    uint8_t _traceCursorStyle;
    int16_t _traceCursorX;
    int16_t _traceCursorY;
    int16_t _traceCursorH;
    int16_t _traceCursorCanvasX1;
    int16_t _traceCursorCanvasY1;
    int16_t _traceCursorCanvasX2;
    int16_t _traceCursorCanvasY2;
#endif

    const MathNode* _highlightNode;   ///< Node whose sub-tree to highlight (nullptr = off)
    lv_color_t      _highlightColor;  ///< Highlight colour (blue for steps, orange for result)
    bool            _highlightActive; ///< True while drawing inside the highlighted sub-tree

    // ── Constantes de estilo ─────────────────────────────────────────────
    static constexpr int16_t PADDING_LEFT   = 8;    ///< Margen izquierdo
    static constexpr int16_t PADDING_RIGHT  = 8;    ///< Margen derecho
    static constexpr int16_t CURSOR_WIDTH   = 3;    ///< Grosor del cursor (px)
    static constexpr int16_t CURSOR_PAD     = 1;    ///< Extensión vertical del cursor
    static constexpr uint32_t BLINK_PERIOD  = 500;  ///< Half-period for cursor blink (ms)
    static constexpr uint32_t EMPTY_COLOR   = 0xD1D1D1;  ///< Color del placeholder
    static constexpr uint32_t CURSOR_COLOR  = 0x000000;  ///< Color del cursor (negro puro, máximo contraste)
    static constexpr int16_t  EMPTY_SIZE    = 8;    ///< Tamaño del cuadrado placeholder
    static constexpr int      MAX_RENDER_DEPTH = 12;  ///< Limit recursion depth to avoid stack overflow

    // ── Event callback (estático → instancia) ────────────────────────────
    static void drawEventCb(lv_event_t* e);
    void onDraw(lv_event_t* e);

    // ── Animación de cursor ──────────────────────────────────────────────
    static void cursorTimerCb(lv_timer_t* t);  ///< 500ms toggle — replaces erratic lv_anim_t
    bool isCursorEditable() const;

    // ── Motor de dibujo recursivo ────────────────────────────────────────

    /**
     * Dibuja un nodo del AST en la posición dada.
     * @param layer      Capa de dibujo LVGL.
     * @param node       Nodo a dibujar.
     * @param x          Posición X del borde izquierdo.
     * @param yTop       Top exacto de la bounding box del nodo (y crece hacia abajo).
     * @param fm         Métricas tipográficas del contexto actual.
     * @param font       Fuente LVGL del contexto actual.
     */
    void drawNode(lv_layer_t* layer, const MathNode* node,
                  int16_t x, int16_t yTop,
                  const FontMetrics& fm, const lv_font_t* font,
                  int depth = 0);

    // Top-based internal path that consumes an already-computed LayoutResult.
    // It must not call calculateLayout() inside the render recursion.
    void drawNodeWithLayout(lv_layer_t* layer, const MathNode* node,
                            int16_t x, int16_t yTop,
                            const FontMetrics& fm, const lv_font_t* font,
                            int depth = 0);

    // Top-based row drawing. Children are positioned by child bounding-box top.
    void drawRow(lv_layer_t* layer, const NodeRow* row,
                 int16_t x, int16_t rowYTop,
                 const FontMetrics& fm, const lv_font_t* font,
                 int depth = 0);

    // Internal baseline-oriented drawing path. yBaseline is always a baseline.
    void drawNodeBaseline(lv_layer_t* layer, const MathNode* node,
                          int16_t x, int16_t yBaseline,
                          const FontMetrics& fm, const lv_font_t* font,
                          int depth = 0);

    void drawRowBaseline(lv_layer_t* layer, const NodeRow* row,
                         int16_t x, int16_t yBaseline,
                         const FontMetrics& fm, const lv_font_t* font,
                         int depth = 0);

    // Dibujo especializado por tipo de nodo (baseline-internal)

    void drawNumberBaseline(lv_layer_t* layer, const NodeNumber* node,
                            int16_t x, int16_t yBaseline,
                            const FontMetrics& fm, const lv_font_t* font);

    void drawOperatorBaseline(lv_layer_t* layer, const NodeOperator* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font);

    void drawEmptyBaseline(lv_layer_t* layer, const NodeEmpty* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm);

    void drawFractionBaseline(lv_layer_t* layer, const NodeFraction* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font,
                              int depth = 0);

    void drawPowerBaseline(lv_layer_t* layer, const NodePower* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font,
                           int depth = 0);

    void drawRootBaseline(lv_layer_t* layer, const NodeRoot* node,
                          int16_t x, int16_t yBaseline,
                          const FontMetrics& fm, const lv_font_t* font,
                          int depth = 0);

    void drawParenBaseline(lv_layer_t* layer, const NodeParen* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font,
                           int depth = 0);

    void drawFunctionBaseline(lv_layer_t* layer, const NodeFunction* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font,
                              int depth = 0);

    void drawLogBaseBaseline(lv_layer_t* layer, const NodeLogBase* node,
                             int16_t x, int16_t yBaseline,
                             const FontMetrics& fm, const lv_font_t* font,
                             int depth = 0);

    void drawConstantBaseline(lv_layer_t* layer, const NodeConstant* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font);

    void drawVariableBaseline(lv_layer_t* layer, const NodeVariable* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font);

    void drawPeriodicDecimalBaseline(lv_layer_t* layer, const NodePeriodicDecimal* node,
                                     int16_t x, int16_t yBaseline,
                                     const FontMetrics& fm, const lv_font_t* font);

    void drawDefIntegralBaseline(lv_layer_t* layer, const NodeDefIntegral* node,
                                 int16_t x, int16_t yBaseline,
                                 const FontMetrics& fm, const lv_font_t* font,
                                 int depth = 0);

    void drawSummationBaseline(lv_layer_t* layer, const NodeSummation* node,
                               int16_t x, int16_t yBaseline,
                               const FontMetrics& fm, const lv_font_t* font,
                               int depth = 0);

    void drawSubscriptBaseline(lv_layer_t* layer, const NodeSubscript* node,
                               int16_t x, int16_t yBaseline,
                               const FontMetrics& fm, const lv_font_t* font,
                               int depth = 0);

    void drawBigOpBaseline(lv_layer_t* layer, const NodeBigOp* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font,
                           int depth = 0);

    // ── N-ary operator glyph rendering (native STIX Two Math glyphs) ─────

    // ── Cursor ───────────────────────────────────────────────────────────
    void drawCursor(lv_layer_t* layer);
    void computeCursorPosition(int16_t baseX, int16_t rootBaseline,
                               const lv_area_t& objArea,
                               const LayoutResult& rootLayout,
                               int16_t rootYTop);
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    void traceCursorState(const LayoutResult& rootLayout,
                          const NodeRow* targetRow,
                          int16_t targetIndex,
                          const LayoutResult* targetLayout,
                          int16_t rowBaseline,
                          int16_t rowYTop,
                          const FontMetrics& cursorFm,
                          const lv_area_t& objArea,
                          bool clamped);
#endif

    /**
     * Calcula la X acumulada para un índice dentro de un NodeRow.
     * @param row    La fila.
     * @param index  Índice del cursor (0..childCount).
     * @param fm     Métricas actuales.
     * @return       Offset X relativo al inicio de la fila.
     */
    int16_t childXOffset(const NodeRow* row, int index, const FontMetrics& fm) const;

    // ── Helpers de dibujo ────────────────────────────────────────────────
    void drawTextBaseline(lv_layer_t* layer, int16_t x, int16_t yBaseline,
                          const char* text, uint8_t scriptLevel, lv_color_t color);

    void drawLine(lv_layer_t* layer,
                  int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                  int16_t width, lv_color_t color);

    void drawFilledRect(lv_layer_t* layer,
                        int16_t x, int16_t y, int16_t w, int16_t h,
                        lv_color_t color, lv_opa_t opa);

    void drawBorderRect(lv_layer_t* layer,
                        int16_t x, int16_t y, int16_t w, int16_t h,
                        lv_color_t color, lv_opa_t opa, int16_t borderW);
};

} // namespace vpam
