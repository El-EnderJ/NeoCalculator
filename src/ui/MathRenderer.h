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

    /**
     * Fuerza el redibujado completo del widget.
     * Llamar después de modificar el AST o mover el cursor.
     */
    void invalidate();

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

    /** FontMetrics para la fuente normal (Montserrat 14) */
    const FontMetrics& normalMetrics() const { return _fmNormal; }

    /** FontMetrics para la fuente pequeña/superscript (Montserrat 12) */
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

private:
    // ── Datos internos ───────────────────────────────────────────────────
    lv_obj_t*               _obj;
    NodeRow*                _root;
    const CursorController* _cursorCtrl;

    // Fuentes LVGL
    const lv_font_t*  _fontNormal;     // Montserrat 14
    const lv_font_t*  _fontSmall;      // Montserrat 12 (super/subscript)
    FontMetrics        _fmNormal;
    FontMetrics        _fmSmall;

    // ── Cursor visual ────────────────────────────────────────────────────
    lv_timer_t* _cursorTimer = nullptr;  ///< Reliable 500ms toggle timer
    bool        _cursorVisible = true;
    int16_t    _cursorX;        // Posición X absoluta (px)
    int16_t    _cursorY;        // Posición Y absoluta (px, tope)
    int16_t    _cursorH;        // Altura del cursor (px)

    // ── Scroll horizontal ────────────────────────────────────────────────
    int16_t    _scrollX;        // Desplazamiento horizontal (≤0)

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
    static constexpr int16_t  VPAM_VERT_PAD   = 10;   ///< Vertical padding (5px top + 5px bottom) for auto-size

    // ── Event callback (estático → instancia) ────────────────────────────
    static void drawEventCb(lv_event_t* e);
    void onDraw(lv_event_t* e);

    // ── Animación de cursor ──────────────────────────────────────────────
    static void cursorTimerCb(lv_timer_t* t);  ///< 500ms toggle — replaces erratic lv_anim_t

    // ── Motor de dibujo recursivo ────────────────────────────────────────

    /**
     * Dibuja un nodo del AST en la posición dada.
     * @param layer      Capa de dibujo LVGL.
     * @param node       Nodo a dibujar.
     * @param x          Posición X del borde izquierdo.
     * @param yBaseline  Posición Y del baseline (y crece hacia abajo).
     * @param fm         Métricas tipográficas del contexto actual.
     * @param font       Fuente LVGL del contexto actual.
     */
    void drawNode(lv_layer_t* layer, const MathNode* node,
                  int16_t x, int16_t yBaseline,
                  const FontMetrics& fm, const lv_font_t* font,
                  int depth = 0);

    // Dibujo especializado por tipo de nodo
    void drawRow(lv_layer_t* layer, const NodeRow* row,
                 int16_t x, int16_t yBaseline,
                 const FontMetrics& fm, const lv_font_t* font,
                 int depth = 0);

    void drawNumber(lv_layer_t* layer, const NodeNumber* node,
                    int16_t x, int16_t yBaseline,
                    const FontMetrics& fm, const lv_font_t* font);

    void drawOperator(lv_layer_t* layer, const NodeOperator* node,
                      int16_t x, int16_t yBaseline,
                      const FontMetrics& fm, const lv_font_t* font);

    void drawEmpty(lv_layer_t* layer, const NodeEmpty* node,
                   int16_t x, int16_t yBaseline,
                   const FontMetrics& fm);

    void drawFraction(lv_layer_t* layer, const NodeFraction* node,
                      int16_t x, int16_t yBaseline,
                      const FontMetrics& fm, const lv_font_t* font,
                      int depth = 0);

    void drawPower(lv_layer_t* layer, const NodePower* node,
                   int16_t x, int16_t yBaseline,
                   const FontMetrics& fm, const lv_font_t* font,
                   int depth = 0);

    void drawRoot(lv_layer_t* layer, const NodeRoot* node,
                  int16_t x, int16_t yBaseline,
                  const FontMetrics& fm, const lv_font_t* font,
                  int depth = 0);

    void drawParen(lv_layer_t* layer, const NodeParen* node,
                   int16_t x, int16_t yBaseline,
                   const FontMetrics& fm, const lv_font_t* font,
                   int depth = 0);

    void drawFunction(lv_layer_t* layer, const NodeFunction* node,
                      int16_t x, int16_t yBaseline,
                      const FontMetrics& fm, const lv_font_t* font,
                      int depth = 0);

    void drawLogBase(lv_layer_t* layer, const NodeLogBase* node,
                     int16_t x, int16_t yBaseline,
                     const FontMetrics& fm, const lv_font_t* font,
                     int depth = 0);

    void drawConstant(lv_layer_t* layer, const NodeConstant* node,
                      int16_t x, int16_t yBaseline,
                      const FontMetrics& fm, const lv_font_t* font);

    void drawVariable(lv_layer_t* layer, const NodeVariable* node,
                      int16_t x, int16_t yBaseline,
                      const FontMetrics& fm, const lv_font_t* font);

    void drawPeriodicDecimal(lv_layer_t* layer, const NodePeriodicDecimal* node,
                             int16_t x, int16_t yBaseline,
                             const FontMetrics& fm, const lv_font_t* font);

    void drawDefIntegral(lv_layer_t* layer, const NodeDefIntegral* node,
                         int16_t x, int16_t yBaseline,
                         const FontMetrics& fm, const lv_font_t* font,
                         int depth = 0);

    void drawSummation(lv_layer_t* layer, const NodeSummation* node,
                       int16_t x, int16_t yBaseline,
                       const FontMetrics& fm, const lv_font_t* font,
                       int depth = 0);

    // ── Cursor ───────────────────────────────────────────────────────────
    void drawCursor(lv_layer_t* layer);
    void computeCursorPosition(int16_t baseX, int16_t baseY);

    /**
     * Calcula la X acumulada para un índice dentro de un NodeRow.
     * @param row    La fila.
     * @param index  Índice del cursor (0..childCount).
     * @param fm     Métricas actuales.
     * @return       Offset X relativo al inicio de la fila.
     */
    int16_t childXOffset(const NodeRow* row, int index, const FontMetrics& fm) const;

    // ── Helpers de dibujo ────────────────────────────────────────────────
    void drawText(lv_layer_t* layer, int16_t x, int16_t yBaseline,
                  const char* text, const lv_font_t* font, lv_color_t color);

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
