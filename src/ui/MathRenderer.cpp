/**
 * MathRenderer.cpp — Motor de Renderizado LVGL 9.5 para el AST Matemático
 *
 * Fase 3 del Motor V.P.A.M.
 *
 * Implementación completa del widget MathCanvas:
 *   · Recorrido recursivo del AST con posicionamiento baseline-aligned.
 *   · Dibujo de texto, barras de fracción, radicales, paréntesis elásticos.
 *   · Placeholders (NodeEmpty) como cuadrados gris tenue.
 *   · Cursor parpadeante con altura adaptativa.
 */

#include "MathRenderer.h"
#include <algorithm>
#include <cstring>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

MathCanvas::MathCanvas()
    : _obj(nullptr)
    , _root(nullptr)
    , _cursorCtrl(nullptr)
    , _fontNormal(&lv_font_montserrat_14)
    , _fontSmall(&lv_font_montserrat_12)
    , _cursorTimer(nullptr)
    , _cursorVisible(true)
    , _cursorX(0), _cursorY(0), _cursorH(0)
    , _scrollX(0)
{
    _fmNormal = metricsFromFont(_fontNormal);
    _fmSmall  = metricsFromFont(_fontSmall);
}

MathCanvas::~MathCanvas() {
    destroy();
}

// ════════════════════════════════════════════════════════════════════════════
// Utilidad estática: FontMetrics desde lv_font_t
// ════════════════════════════════════════════════════════════════════════════

FontMetrics MathCanvas::metricsFromFont(const lv_font_t* font) {
    FontMetrics fm;
    fm.ascent  = static_cast<int16_t>(font->line_height - font->base_line);
    fm.descent = static_cast<int16_t>(font->base_line);

    // Medir el ancho del carácter '0' como referencia
    lv_font_glyph_dsc_t glyph;
    bool ok = lv_font_get_glyph_dsc(font, &glyph, '0', '1');
    fm.charWidth = ok ? static_cast<int16_t>(glyph.adv_w)
                      : static_cast<int16_t>(font->line_height * 6 / 10);
    return fm;
}

// ════════════════════════════════════════════════════════════════════════════
// Crear / Destruir
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::create(lv_obj_t* parent) {
    if (_obj) return;

    _obj = lv_obj_create(parent);

    // Fondo blanco, sin bordes, sin scroll
    lv_obj_set_style_bg_color(_obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_obj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_obj, LV_OBJ_FLAG_SCROLLABLE);

    // Registrar el callback de dibujo
    lv_obj_set_user_data(_obj, this);
    lv_obj_add_event_cb(_obj, drawEventCb, LV_EVENT_DRAW_MAIN, this);
}

void MathCanvas::destroy() {
    stopCursorBlink();
    if (_obj) {
        lv_obj_delete(_obj);
        _obj = nullptr;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Datos y redibujado
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::setExpression(NodeRow* root, const CursorController* ctrl) {
    _root       = root;
    _cursorCtrl = ctrl;
    _scrollX    = 0;
    invalidate();
}

void MathCanvas::invalidate() {
    if (_obj) lv_obj_invalidate(_obj);
}

void MathCanvas::scrollBy(int16_t delta) {
    _scrollX += delta;
    if (_scrollX > 0) _scrollX = 0;   // No pasar del borde izquierdo
    invalidate();
}

// ════════════════════════════════════════════════════════════════════════════
// Cursor Blink Animation
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::startCursorBlink() {
    if (_cursorTimer) return;   // already running
    _cursorVisible = true;
    // lv_timer fires every BLINK_PERIOD ms and toggles visibility.
    // Much more reliable than lv_anim_t which got reset on every keypress.
    _cursorTimer = lv_timer_create(cursorTimerCb, BLINK_PERIOD, this);
    invalidate();
}

void MathCanvas::stopCursorBlink() {
    if (!_cursorTimer) return;
    lv_timer_delete(_cursorTimer);
    _cursorTimer   = nullptr;
    _cursorVisible = false;
    invalidate();
}

void MathCanvas::resetCursorBlink() {
    // Make cursor instantly visible, then restart the 500 ms countdown.
    // Called on every keypress so the cursor is visible right after input.
    _cursorVisible = true;
    if (_cursorTimer) {
        lv_timer_reset(_cursorTimer);   // restart 500 ms from now
    } else {
        _cursorTimer = lv_timer_create(cursorTimerCb, BLINK_PERIOD, this);
    }
    invalidate();
}

void MathCanvas::cursorTimerCb(lv_timer_t* t) {
    auto* self = static_cast<MathCanvas*>(lv_timer_get_user_data(t));
    self->_cursorVisible = !self->_cursorVisible;
    self->invalidate();
}

// ════════════════════════════════════════════════════════════════════════════
// Draw Event Callback
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawEventCb(lv_event_t* e) {
    auto* self = static_cast<MathCanvas*>(lv_event_get_user_data(e));
    if (self) self->onDraw(e);
}

void MathCanvas::onDraw(lv_event_t* e) {
    if (!_root || !_obj) return;

    lv_layer_t* layer = lv_event_get_layer(e);

    // Obtener coordenadas del widget en pantalla
    lv_area_t objArea;
    lv_obj_get_coords(_obj, &objArea);

    int16_t widgetW = static_cast<int16_t>(objArea.x2 - objArea.x1 + 1);
    int16_t widgetH = static_cast<int16_t>(objArea.y2 - objArea.y1 + 1);

    // Recalcular layout del AST
    _root->calculateLayout(_fmNormal);
    const auto& rootL = _root->layout();

    // Posicionar la expresión: centrada verticalmente, alineada a la izquierda
    int16_t baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT + _scrollX;
    int16_t baseY = static_cast<int16_t>(objArea.y1) + (widgetH + rootL.ascent - rootL.descent) / 2;

    // Calcular posición del cursor ANTES de dibujar
    computeCursorPosition(baseX, baseY);

    // Auto-scroll horizontal: mantener el cursor visible
    {
        int16_t visLeft  = static_cast<int16_t>(objArea.x1) + PADDING_LEFT;
        int16_t visRight = static_cast<int16_t>(objArea.x2) - PADDING_RIGHT;

        if (_cursorX < visLeft) {
            _scrollX += (visLeft - _cursorX + 4);
            baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT + _scrollX;
            computeCursorPosition(baseX, baseY);
        } else if (_cursorX > visRight) {
            _scrollX -= (_cursorX - visRight + 4);
            baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT + _scrollX;
            computeCursorPosition(baseX, baseY);
        }

        // No dejar que el scroll sea positivo (expresión se sale por la izquierda)
        if (_scrollX > 0) {
            _scrollX = 0;
            baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT;
            computeCursorPosition(baseX, baseY);
        }
    }

    // Dibujar la expresión recursivamente
    drawRow(layer, _root, baseX, baseY, _fmNormal, _fontNormal);

    // Dibujar el cursor parpadeante
    drawCursor(layer);
}

// ════════════════════════════════════════════════════════════════════════════
// Cálculo de posición del cursor
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::computeCursorPosition(int16_t baseX, int16_t baseY) {
    if (!_cursorCtrl || !_cursorCtrl->cursor().isValid()) {
        _cursorX = baseX;
        _cursorY = baseY - _fmNormal.ascent;
        _cursorH = _fmNormal.height();
        return;
    }

    const Cursor& cur = _cursorCtrl->cursor();

    // Necesitamos encontrar la posición absoluta del NodeRow del cursor.
    // Recorremos el AST de forma recursiva para encontrar el row y acumular offsets.
    struct FindResult {
        bool   found;
        int16_t x;
        int16_t yBaseline;
        FontMetrics fm;
    };

    // Función recursiva para buscar el row del cursor en el árbol
    struct Finder {
        const NodeRow* target;
        FindResult     result;

        void search(const MathNode* node, int16_t x, int16_t yBaseline,
                    const FontMetrics& fm, const FontMetrics& fmSmall) {
            if (result.found) return;
            if (!node) return;

            if (node->type() == NodeType::Row) {
                auto* row = static_cast<const NodeRow*>(node);
                if (row == target) {
                    result = { true, x, yBaseline, fm };
                    return;
                }
                // Buscar dentro de los hijos del row
                int16_t cx = x;
                for (int i = 0; i < row->childCount(); ++i) {
                    if (i > 0) cx += NodeRow::CHILD_GAP;
                    search(row->child(i), cx, yBaseline, fm, fmSmall);
                    if (result.found) return;
                    cx += row->child(i)->layout().width;
                }
            }
            else if (node->type() == NodeType::Fraction) {
                auto* frac = static_cast<const NodeFraction*>(node);
                const auto& fl = frac->layout();
                const auto& numL = frac->numerator()->layout();
                const auto& denL = frac->denominator()->layout();

                int16_t axis = fm.axisHeight();
                int16_t barHalfUp = (NodeFraction::BAR_THICK + 1) / 2;
                int16_t barHalfDown = NodeFraction::BAR_THICK / 2;
                int16_t contentW = std::max(numL.width, denL.width);

                int16_t yAxis = static_cast<int16_t>(yBaseline - axis);

                // Numerador
                int16_t numX = static_cast<int16_t>(x + NodeFraction::BAR_H_PAD +
                               (contentW - numL.width) / 2);
                int16_t numY = static_cast<int16_t>(yAxis - barHalfUp -
                               NodeFraction::BAR_V_GAP - numL.descent);
                search(frac->numerator(), numX, numY, fm, fmSmall);
                if (result.found) return;

                // Denominador
                int16_t denX = static_cast<int16_t>(x + NodeFraction::BAR_H_PAD +
                               (contentW - denL.width) / 2);
                int16_t denY = static_cast<int16_t>(yAxis + barHalfDown +
                               NodeFraction::BAR_V_GAP + denL.ascent);
                search(frac->denominator(), denX, denY, fm, fmSmall);
            }
            else if (node->type() == NodeType::Power) {
                auto* pow = static_cast<const NodePower*>(node);
                const auto& baseL = pow->base()->layout();

                // Base
                search(pow->base(), x, yBaseline, fm, fmSmall);
                if (result.found) return;

                // Exponente (fuente reducida)
                FontMetrics fmSup = fm.superscript();
                int16_t expShift = static_cast<int16_t>(
                    (baseL.ascent * NodePower::EXP_RAISE_NUM) / NodePower::EXP_RAISE_DEN);
                int16_t expX = static_cast<int16_t>(x + baseL.width);
                int16_t expY = static_cast<int16_t>(yBaseline - expShift -
                               pow->exponent()->layout().descent);
                // Ajustar baseline del exponente correctamente
                const auto& expL = pow->exponent()->layout();
                int16_t expBaseline = static_cast<int16_t>(yBaseline - expShift);

                search(pow->exponent(), expX, expBaseline, fmSup,
                       fmSup.superscript());
            }
            else if (node->type() == NodeType::Root) {
                auto* root = static_cast<const NodeRoot*>(node);
                int16_t radSymW = NodeRoot::HOOK_W + NodeRoot::SLOPE_W;

                // Radicando
                int16_t radX = static_cast<int16_t>(x + radSymW);
                search(root->radicand(), radX, yBaseline, fm, fmSmall);
                if (result.found) return;

                // Índice (si existe)
                if (root->hasDegree()) {
                    FontMetrics fmDeg = fm.superscript();
                    const auto& degL  = root->degree()->layout();
                    int16_t degX = static_cast<int16_t>(x + 1);
                    int16_t degY = static_cast<int16_t>(yBaseline -
                                   root->layout().ascent + degL.ascent);
                    search(root->degree(), degX, degY, fmDeg,
                           fmDeg.superscript());
                }
            }
            else if (node->type() == NodeType::Paren) {
                auto* paren = static_cast<const NodeParen*>(node);
                int16_t contentX = static_cast<int16_t>(x + NodeParen::PAREN_W +
                                   NodeParen::INNER_PAD);
                search(paren->content(), contentX, yBaseline, fm, fmSmall);
            }
            else if (node->type() == NodeType::Function) {
                auto* func = static_cast<const NodeFunction*>(node);
                const auto& funcL = func->layout();
                // labelWidth + LABEL_GAP + PAREN_W + INNER_PAD
                int16_t argX = static_cast<int16_t>(x + func->layout().width
                               - NodeFunction::PAREN_W - NodeFunction::INNER_PAD
                               - func->argument()->layout().width
                               - NodeFunction::INNER_PAD);
                // Más simple: label ocupa los primeros píxeles
                // argX = x + labelWidth + LABEL_GAP + PAREN_W + INNER_PAD
                // Necesitamos labelWidth del func... usamos layout aritmética
                int16_t contentW = func->argument()->layout().width;
                int16_t parenBlock = NodeFunction::PAREN_W + NodeFunction::INNER_PAD
                                   + contentW + NodeFunction::INNER_PAD + NodeFunction::PAREN_W;
                int16_t labelW = static_cast<int16_t>(funcL.width - NodeFunction::LABEL_GAP - parenBlock);
                int16_t contentX = static_cast<int16_t>(x + labelW + NodeFunction::LABEL_GAP
                                   + NodeFunction::PAREN_W + NodeFunction::INNER_PAD);
                search(func->argument(), contentX, yBaseline, fm, fmSmall);
            }
            else if (node->type() == NodeType::LogBase) {
                auto* lb = static_cast<const NodeLogBase*>(node);
                const auto& lbL = lb->layout();
                const auto& baseL = lb->base()->layout();
                const auto& argL = lb->argument()->layout();

                // "log" label width: lbL.width − baseL.width − parenBlock − LABEL_GAP
                int16_t parenBlock = NodeLogBase::PAREN_W + NodeLogBase::INNER_PAD
                                   + argL.width + NodeLogBase::INNER_PAD + NodeLogBase::PAREN_W;
                int16_t labelW = static_cast<int16_t>(lbL.width - baseL.width
                                 - NodeLogBase::LABEL_GAP - parenBlock);

                // Base (subíndice) → fuente reducida, bajada
                FontMetrics fmSub = fm.superscript();
                int16_t subDrop = static_cast<int16_t>(
                    std::max(static_cast<int>(fm.descent * NodeLogBase::SUB_DROP_NUM / NodeLogBase::SUB_DROP_DEN), 2));
                int16_t baseX = static_cast<int16_t>(x + labelW);
                int16_t baseBaseline = static_cast<int16_t>(yBaseline + subDrop);
                search(lb->base(), baseX, baseBaseline, fmSub, fmSub.superscript());
                if (result.found) return;

                // Argumento (dentro de paréntesis)
                int16_t argX = static_cast<int16_t>(x + labelW + baseL.width
                               + NodeLogBase::LABEL_GAP + NodeLogBase::PAREN_W
                               + NodeLogBase::INNER_PAD);
                search(lb->argument(), argX, yBaseline, fm, fmSmall);
            }
        }
    };

    Finder finder;
    finder.target = cur.row;
    finder.result = { false, 0, 0, _fmNormal };
    finder.search(_root, baseX, baseY, _fmNormal, _fmSmall);

    if (finder.result.found) {
        int16_t offsetX = childXOffset(cur.row, cur.index, finder.result.fm);
        _cursorX = static_cast<int16_t>(finder.result.x + offsetX);
        _cursorY = static_cast<int16_t>(finder.result.yBaseline -
                   cur.row->layout().ascent - CURSOR_PAD);
        _cursorH = static_cast<int16_t>(cur.row->layout().height() + 2 * CURSOR_PAD);
    } else {
        // Fallback: inicio de la expresión
        _cursorX = baseX;
        _cursorY = static_cast<int16_t>(baseY - _fmNormal.ascent - CURSOR_PAD);
        _cursorH = static_cast<int16_t>(_fmNormal.height() + 2 * CURSOR_PAD);
    }
}

int16_t MathCanvas::childXOffset(const NodeRow* row, int index,
                                  const FontMetrics& fm) const {
    int16_t offset = 0;
    int count = std::min(index, row->childCount());
    for (int i = 0; i < count; ++i) {
        if (i > 0) offset += NodeRow::CHILD_GAP;
        offset += row->child(i)->layout().width;
    }
    // Añadir gap antes del cursor si no está al inicio y hay más nodos
    if (count > 0 && count < row->childCount()) {
        offset += NodeRow::CHILD_GAP;
    }
    return offset;
}

// ════════════════════════════════════════════════════════════════════════════
// Motor de dibujo recursivo
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawNode(lv_layer_t* layer, const MathNode* node,
                          int16_t x, int16_t yBaseline,
                          const FontMetrics& fm, const lv_font_t* font) {
    if (!node) return;

    switch (node->type()) {
        case NodeType::Row:
            drawRow(layer, static_cast<const NodeRow*>(node),
                    x, yBaseline, fm, font);
            break;
        case NodeType::Number:
            drawNumber(layer, static_cast<const NodeNumber*>(node),
                       x, yBaseline, fm, font);
            break;
        case NodeType::Operator:
            drawOperator(layer, static_cast<const NodeOperator*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::Empty:
            drawEmpty(layer, static_cast<const NodeEmpty*>(node),
                      x, yBaseline, fm);
            break;
        case NodeType::Fraction:
            drawFraction(layer, static_cast<const NodeFraction*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::Power:
            drawPower(layer, static_cast<const NodePower*>(node),
                      x, yBaseline, fm, font);
            break;
        case NodeType::Root:
            drawRoot(layer, static_cast<const NodeRoot*>(node),
                     x, yBaseline, fm, font);
            break;
        case NodeType::Paren:
            drawParen(layer, static_cast<const NodeParen*>(node),
                      x, yBaseline, fm, font);
            break;
        case NodeType::Function:
            drawFunction(layer, static_cast<const NodeFunction*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::LogBase:
            drawLogBase(layer, static_cast<const NodeLogBase*>(node),
                        x, yBaseline, fm, font);
            break;
        case NodeType::Constant:
            drawConstant(layer, static_cast<const NodeConstant*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::Variable:
            drawVariable(layer, static_cast<const NodeVariable*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::PeriodicDecimal:
            drawPeriodicDecimal(layer, static_cast<const NodePeriodicDecimal*>(node),
                                x, yBaseline, fm, font);
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawRow — Secuencia horizontal de nodos
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawRow(lv_layer_t* layer, const NodeRow* row,
                         int16_t x, int16_t yBaseline,
                         const FontMetrics& fm, const lv_font_t* font) {
    int16_t cx = x;
    for (int i = 0; i < row->childCount(); ++i) {
        if (i > 0) cx += NodeRow::CHILD_GAP;
        drawNode(layer, row->child(i), cx, yBaseline, fm, font);
        cx += row->child(i)->layout().width;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawNumber — Dígitos y punto decimal
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawNumber(lv_layer_t* layer, const NodeNumber* node,
                            int16_t x, int16_t yBaseline,
                            const FontMetrics& fm, const lv_font_t* font) {
    const std::string& val = node->value();
    if (val.empty()) return;
    drawText(layer, x, yBaseline, val.c_str(), font, lv_color_black());
}

// ════════════════════════════════════════════════════════════════════════════
// drawOperator — Símbolos +, −, ×
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawOperator(lv_layer_t* layer, const NodeOperator* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    // El operador tiene padding: OP_PAD | símbolo | OP_PAD
    int16_t textX = static_cast<int16_t>(x + NodeOperator::OP_PAD);
    drawText(layer, textX, yBaseline, node->symbol(), font,
             lv_color_hex(0x333333));
}

// ════════════════════════════════════════════════════════════════════════════
// drawEmpty — Placeholder □ (cuadrado gris tenue)
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawEmpty(lv_layer_t* layer, const NodeEmpty* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm) {
    const auto& l = node->layout();

    // Centrar el cuadrado placeholder dentro de la bounding box del nodo
    int16_t sqSize = EMPTY_SIZE;
    int16_t sqX = static_cast<int16_t>(x + (l.width - sqSize) / 2);
    int16_t sqY = static_cast<int16_t>(yBaseline - sqSize / 2 - 1);

    // Dibujar cuadrado con borde gris y relleno muy tenue
    drawFilledRect(layer, sqX, sqY, sqSize, sqSize,
                   lv_color_hex(EMPTY_COLOR), LV_OPA_30);
    drawBorderRect(layer, sqX, sqY, sqSize, sqSize,
                   lv_color_hex(EMPTY_COLOR), LV_OPA_80, 1);
}

// ════════════════════════════════════════════════════════════════════════════
// drawFraction — Numerador / barra / denominador
//
//    ┌────────────────┐
//    │   numerador    │  centrado
//    │────────────────│  barra en el eje
//    │  denominador   │  centrado
//    └────────────────┘
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawFraction(lv_layer_t* layer, const NodeFraction* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    const auto& fracL = node->layout();
    const auto& numL  = node->numerator()->layout();
    const auto& denL  = node->denominator()->layout();

    int16_t axis       = fm.axisHeight();
    int16_t barHalfUp  = (NodeFraction::BAR_THICK + 1) / 2;
    int16_t barHalfDown = NodeFraction::BAR_THICK / 2;
    int16_t contentW   = std::max(numL.width, denL.width);

    int16_t yAxis = static_cast<int16_t>(yBaseline - axis);

    // ── Barra de fracción ──
    int16_t barX1 = x;
    int16_t barX2 = static_cast<int16_t>(x + fracL.width - 1);
    drawLine(layer, barX1, yAxis, barX2, yAxis,
             NodeFraction::BAR_THICK, lv_color_black());

    // ── Numerador (centrado sobre la barra) ──
    int16_t numX = static_cast<int16_t>(x + NodeFraction::BAR_H_PAD +
                   (contentW - numL.width) / 2);
    int16_t numBaseline = static_cast<int16_t>(yAxis - barHalfUp -
                          NodeFraction::BAR_V_GAP - numL.descent);
    drawNode(layer, node->numerator(), numX, numBaseline, fm, font);

    // ── Denominador (centrado bajo la barra) ──
    int16_t denX = static_cast<int16_t>(x + NodeFraction::BAR_H_PAD +
                   (contentW - denL.width) / 2);
    int16_t denBaseline = static_cast<int16_t>(yAxis + barHalfDown +
                          NodeFraction::BAR_V_GAP + denL.ascent);
    drawNode(layer, node->denominator(), denX, denBaseline, fm, font);
}

// ════════════════════════════════════════════════════════════════════════════
// drawPower — Base ^ Exponente (superíndice)
//
//    ┌base┐┌exp┐     Exponente elevado al 60% del ascent de base
//    │ 23 ││ 4 │     Fuente reducida (~70%)
//    └────┘└───┘
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawPower(lv_layer_t* layer, const NodePower* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font) {
    const auto& baseL = node->base()->layout();

    // ── Base (fuente normal) ──
    drawNode(layer, node->base(), x, yBaseline, fm, font);

    // ── Exponente (fuente reducida, elevado) ──
    FontMetrics fmSup = fm.superscript();
    int16_t expShift = static_cast<int16_t>(
        (baseL.ascent * NodePower::EXP_RAISE_NUM) / NodePower::EXP_RAISE_DEN);

    // El baseline del exponente: el fondo del exp está a expShift sobre el baseline
    int16_t expBaseline = static_cast<int16_t>(yBaseline - expShift);

    int16_t expX = static_cast<int16_t>(x + baseL.width);
    drawNode(layer, node->exponent(), expX, expBaseline, fmSup, _fontSmall);
}

// ════════════════════════════════════════════════════════════════════════════
// drawRoot — Símbolo radical √ con gancho, pendiente y overline
//
//         ╱‾‾‾‾‾‾‾‾┐  overline
//        ╱ radicand │
//       ╱           │
//    └╱─────────────┘
//    hook  slope
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawRoot(lv_layer_t* layer, const NodeRoot* node,
                          int16_t x, int16_t yBaseline,
                          const FontMetrics& fm, const lv_font_t* font) {
    const auto& rootL = node->layout();
    const auto& radL  = node->radicand()->layout();
    int16_t radSymW   = NodeRoot::HOOK_W + NodeRoot::SLOPE_W;

    // Coordenadas clave
    int16_t yTop    = static_cast<int16_t>(yBaseline - rootL.ascent);
    int16_t yBottom = static_cast<int16_t>(yBaseline + rootL.descent);
    int16_t yOverline = static_cast<int16_t>(yTop + NodeRoot::OVERLINE_T);

    // Punto medio para el inicio del gancho (≈ 60% desde arriba)
    int16_t hookStartY = static_cast<int16_t>(yTop + (yBottom - yTop) * 6 / 10);

    // ── Hook: pequeño trazo descendente ──
    drawLine(layer, x, hookStartY,
             static_cast<int16_t>(x + NodeRoot::HOOK_W),
             yBottom,
             1, lv_color_black());

    // ── Slope: trazo ascendente desde el fondo hasta la overline ──
    drawLine(layer,
             static_cast<int16_t>(x + NodeRoot::HOOK_W), yBottom,
             static_cast<int16_t>(x + radSymW), yOverline,
             1, lv_color_black());

    // ── Overline: línea horizontal sobre el radicando ──
    int16_t overlineX1 = static_cast<int16_t>(x + radSymW);
    int16_t overlineX2 = static_cast<int16_t>(x + radSymW + radL.width + NodeRoot::RIGHT_PAD);
    drawLine(layer, overlineX1, yOverline, overlineX2, yOverline,
             NodeRoot::OVERLINE_T, lv_color_black());

    // ── Radicando ──
    int16_t radX = static_cast<int16_t>(x + radSymW);
    drawNode(layer, node->radicand(), radX, yBaseline, fm, font);

    // ── Índice de raíz (si existe) ──
    if (node->hasDegree()) {
        FontMetrics fmDeg = fm.superscript();
        const auto& degL  = node->degree()->layout();
        int16_t degX = static_cast<int16_t>(x + 1);
        int16_t degY = static_cast<int16_t>(yTop + degL.ascent);
        drawNode(layer, node->degree(), degX, degY, fmDeg, _fontSmall);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawParen — Paréntesis elásticos que se estiran al contenido
//
//    (  contenido  )     Los paréntesis cubren toda la altura del interior
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawParen(lv_layer_t* layer, const NodeParen* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font) {
    const auto& parenL   = node->layout();
    const auto& contentL = node->content()->layout();

    int16_t yTop    = static_cast<int16_t>(yBaseline - parenL.ascent);
    int16_t yBottom = static_cast<int16_t>(yBaseline + parenL.descent);
    int16_t yMid    = static_cast<int16_t>((yTop + yBottom) / 2);

    int16_t pw = NodeParen::PAREN_W;

    // ── Paréntesis izquierdo: ( ──
    // Top curve: de la parte superior derecha al centro izquierdo
    drawLine(layer,
             static_cast<int16_t>(x + pw - 1), yTop,
             static_cast<int16_t>(x + 1), yMid,
             1, lv_color_black());
    // Bottom curve: del centro izquierdo a la parte inferior derecha
    drawLine(layer,
             static_cast<int16_t>(x + 1), yMid,
             static_cast<int16_t>(x + pw - 1), yBottom,
             1, lv_color_black());

    // ── Contenido ──
    int16_t contentX = static_cast<int16_t>(x + pw + NodeParen::INNER_PAD);
    drawNode(layer, node->content(), contentX, yBaseline, fm, font);

    // ── Paréntesis derecho: ) ──
    int16_t rpX = static_cast<int16_t>(x + parenL.width - pw);
    // Top curve: de la parte superior izquierda al centro derecho
    drawLine(layer,
             static_cast<int16_t>(rpX + 1), yTop,
             static_cast<int16_t>(rpX + pw - 1), yMid,
             1, lv_color_black());
    // Bottom curve: del centro derecho a la parte inferior izquierda
    drawLine(layer,
             static_cast<int16_t>(rpX + pw - 1), yMid,
             static_cast<int16_t>(rpX + 1), yBottom,
             1, lv_color_black());
}

// ════════════════════════════════════════════════════════════════════════════
// drawFunction — Función: label(argumento)
//
//    ┌─────┐┌─────────────┐
//    │ sin ││( argumento )│
//    └─────┘└─────────────┘
//
// El label ("sin", "cos⁻¹", "ln", etc.) se dibuja como texto, seguido
// de paréntesis elásticos automáticos (como NodeParen) alrededor del arg.
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawFunction(lv_layer_t* layer, const NodeFunction* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    const auto& funcL = node->layout();
    const auto& argL  = node->argument()->layout();

    // Calcular el ancho del label a partir del layout
    int16_t contentW = argL.width;
    int16_t parenBlock = NodeFunction::PAREN_W + NodeFunction::INNER_PAD
                       + contentW + NodeFunction::INNER_PAD + NodeFunction::PAREN_W;
    int16_t labelW = static_cast<int16_t>(funcL.width - NodeFunction::LABEL_GAP - parenBlock);

    // ── Label (texto de la función) ──
    drawText(layer, x, yBaseline, node->label(), font,
             lv_color_hex(0x1a1a1a));

    // ── Paréntesis automáticos + argumento ──
    int16_t parenX = static_cast<int16_t>(x + labelW + NodeFunction::LABEL_GAP);
    int16_t yTop    = static_cast<int16_t>(yBaseline - funcL.ascent);
    int16_t yBottom = static_cast<int16_t>(yBaseline + funcL.descent);
    int16_t yMid    = static_cast<int16_t>((yTop + yBottom) / 2);
    int16_t pw      = NodeFunction::PAREN_W;

    // Paréntesis izquierdo (
    drawLine(layer,
             static_cast<int16_t>(parenX + pw - 1), yTop,
             static_cast<int16_t>(parenX + 1), yMid,
             1, lv_color_black());
    drawLine(layer,
             static_cast<int16_t>(parenX + 1), yMid,
             static_cast<int16_t>(parenX + pw - 1), yBottom,
             1, lv_color_black());

    // Contenido (argumento)
    int16_t contentX = static_cast<int16_t>(parenX + pw + NodeFunction::INNER_PAD);
    drawNode(layer, node->argument(), contentX, yBaseline, fm, font);

    // Paréntesis derecho )
    int16_t rpX = static_cast<int16_t>(parenX + parenBlock - pw);
    drawLine(layer,
             static_cast<int16_t>(rpX + 1), yTop,
             static_cast<int16_t>(rpX + pw - 1), yMid,
             1, lv_color_black());
    drawLine(layer,
             static_cast<int16_t>(rpX + pw - 1), yMid,
             static_cast<int16_t>(rpX + 1), yBottom,
             1, lv_color_black());
}

// ════════════════════════════════════════════════════════════════════════════
// drawLogBase — Logaritmo con subíndice: log_n(x)
//
//    ┌─────┐┌base┐┌─────────────┐
//    │ log ││ 2  ││( argumento )│
//    └─────┘└────┘└─────────────┘
//            subscript
//
// "log" en fuente normal, base en fuente reducida bajada 1/3 del descent,
// luego paréntesis elásticos alrededor del argumento.
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawLogBase(lv_layer_t* layer, const NodeLogBase* node,
                             int16_t x, int16_t yBaseline,
                             const FontMetrics& fm, const lv_font_t* font) {
    const auto& lbL  = node->layout();
    const auto& baseL = node->base()->layout();
    const auto& argL  = node->argument()->layout();

    // Calcular labelW de "log"
    int16_t parenBlock = NodeLogBase::PAREN_W + NodeLogBase::INNER_PAD
                       + argL.width + NodeLogBase::INNER_PAD + NodeLogBase::PAREN_W;
    int16_t labelW = static_cast<int16_t>(lbL.width - baseL.width
                     - NodeLogBase::LABEL_GAP - parenBlock);

    // ── Label "log" ──
    drawText(layer, x, yBaseline, "log", font, lv_color_hex(0x1a1a1a));

    // ── Base / subíndice (fuente reducida, bajada) ──
    FontMetrics fmSub = fm.superscript();
    int16_t subDrop = static_cast<int16_t>(
        std::max(static_cast<int>(fm.descent * NodeLogBase::SUB_DROP_NUM / NodeLogBase::SUB_DROP_DEN), 2));
    int16_t baseX = static_cast<int16_t>(x + labelW);
    int16_t baseBaseline = static_cast<int16_t>(yBaseline + subDrop);
    drawNode(layer, node->base(), baseX, baseBaseline, fmSub, _fontSmall);

    // ── Paréntesis automáticos + argumento ──
    int16_t parenX = static_cast<int16_t>(x + labelW + baseL.width + NodeLogBase::LABEL_GAP);
    int16_t yTop    = static_cast<int16_t>(yBaseline - lbL.ascent);
    int16_t yBottom = static_cast<int16_t>(yBaseline + lbL.descent);
    int16_t yMid    = static_cast<int16_t>((yTop + yBottom) / 2);
    int16_t pw      = NodeLogBase::PAREN_W;

    // Paréntesis izquierdo (
    drawLine(layer,
             static_cast<int16_t>(parenX + pw - 1), yTop,
             static_cast<int16_t>(parenX + 1), yMid,
             1, lv_color_black());
    drawLine(layer,
             static_cast<int16_t>(parenX + 1), yMid,
             static_cast<int16_t>(parenX + pw - 1), yBottom,
             1, lv_color_black());

    // Contenido (argumento)
    int16_t contentX = static_cast<int16_t>(parenX + pw + NodeLogBase::INNER_PAD);
    drawNode(layer, node->argument(), contentX, yBaseline, fm, font);

    // Paréntesis derecho )
    int16_t rpX = static_cast<int16_t>(parenX + parenBlock - pw);
    drawLine(layer,
             static_cast<int16_t>(rpX + 1), yTop,
             static_cast<int16_t>(rpX + pw - 1), yMid,
             1, lv_color_black());
    drawLine(layer,
             static_cast<int16_t>(rpX + pw - 1), yMid,
             static_cast<int16_t>(rpX + 1), yBottom,
             1, lv_color_black());
}

// ════════════════════════════════════════════════════════════════════════════
// drawConstant — Símbolo π o e
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawConstant(lv_layer_t* layer, const NodeConstant* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    drawText(layer, x, yBaseline, node->symbol(), font,
             lv_color_hex(0x0060C0));   // Azul oscuro para constantes
}

// ════════════════════════════════════════════════════════════════════════════
// drawVariable — Variable algebraica: x, y, z (azul), A-F, Ans, PreAns
//
// Estilo visual:
//   · x, y, z      → Azul #4A90D9 para diferenciar de la × de multiplicar
//   · A-F           → Negro normal
//   · Ans, PreAns   → Bloque de texto integrado en negro
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawVariable(lv_layer_t* layer, const NodeVariable* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    // All variables render in black — x, y, z are independent variables,
    // NOT multiplication.  Previously blue, now black to avoid confusion
    // with the × operator.
    lv_color_t color = lv_color_black();

    drawText(layer, x, yBaseline, node->label(), font, color);
}

// ════════════════════════════════════════════════════════════════════════════
// drawPeriodicDecimal — Decimal periódico con overline
//
//    [-] intPart . nonRepeat  repeat
//                              ‾‾‾‾‾ ← overline
//
// Ejemplo: −0.1̄6̄ (= −1/6)
//   Se renderiza: "−0.1" normal + "6" con overline encima
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawPeriodicDecimal(lv_layer_t* layer,
                                     const NodePeriodicDecimal* node,
                                     int16_t x, int16_t yBaseline,
                                     const FontMetrics& fm,
                                     const lv_font_t* font) {
    // Construir la cadena de texto fija (todo excepto la overline)
    std::string prefix;
    if (node->isNegative()) prefix += "-";
    prefix += node->intPart();
    if (!node->nonRepeat().empty() || !node->repeat().empty()) {
        prefix += ".";
    }
    prefix += node->nonRepeat();

    // Dibujar la parte que NO tiene overline
    int16_t cx = x;
    if (!prefix.empty()) {
        drawText(layer, cx, yBaseline, prefix.c_str(), font, lv_color_black());
        // Calcular ancho del texto dibujado
        cx += static_cast<int16_t>(prefix.size() * fm.charWidth);
    }

    // Dibujar la parte periódica (con overline)
    const std::string& rep = node->repeat();
    if (!rep.empty()) {
        drawText(layer, cx, yBaseline, rep.c_str(), font, lv_color_black());
        int16_t repW = static_cast<int16_t>(rep.size() * fm.charWidth);

        // Overline: línea horizontal sobre los dígitos periódicos
        int16_t overY = static_cast<int16_t>(yBaseline - fm.ascent
                        - NodePeriodicDecimal::OVERLINE_GAP);
        drawLine(layer, cx, overY,
                 static_cast<int16_t>(cx + repW - 1), overY,
                 NodePeriodicDecimal::OVERLINE_T, lv_color_black());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawCursor — Línea vertical parpadeante
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawCursor(lv_layer_t* layer) {
    if (!_cursorVisible || !_cursorCtrl) return;
    if (!_cursorCtrl->cursor().isValid()) return;

    drawFilledRect(layer, _cursorX, _cursorY, CURSOR_WIDTH, _cursorH,
                   lv_color_hex(CURSOR_COLOR), LV_OPA_COVER);
}

// ════════════════════════════════════════════════════════════════════════════
// Helpers de dibujo — Wrappers limpios sobre la API de LVGL 9.x
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawText(lv_layer_t* layer, int16_t x, int16_t yBaseline,
                          const char* text, const lv_font_t* font,
                          lv_color_t color) {
    if (!text || !text[0]) return;

    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font  = font;
    dsc.color = color;
    dsc.text  = text;
    dsc.opa   = LV_OPA_COVER;

    // El área define el bounding box del texto.
    // LVGL dibuja el texto desde el tope del área.
    // Tope = yBaseline - ascent (donde ascent = line_height - base_line)
    int16_t fontAscent = static_cast<int16_t>(font->line_height - font->base_line);
    int16_t yTop = static_cast<int16_t>(yBaseline - fontAscent);

    // Calcular el ancho del texto
    // Usamos el largo del texto y el ancho de carácter de la fuente
    int32_t textLen = static_cast<int32_t>(std::strlen(text));

    // Para el ancho, medimos cada carácter usando la fuente
    int32_t textWidth = 0;
    const char* p = text;
    while (*p) {
        lv_font_glyph_dsc_t glyph;
        uint32_t letter = static_cast<uint32_t>(static_cast<uint8_t>(*p));

        // Manejar UTF-8 multibyte (operadores −, ×)
        if ((letter & 0x80) != 0) {
            // Decodificar UTF-8
            if ((letter & 0xE0) == 0xC0 && p[1]) {
                letter = ((letter & 0x1F) << 6) | (static_cast<uint8_t>(p[1]) & 0x3F);
                p += 2;
            } else if ((letter & 0xF0) == 0xE0 && p[1] && p[2]) {
                letter = ((letter & 0x0F) << 12)
                       | ((static_cast<uint8_t>(p[1]) & 0x3F) << 6)
                       | (static_cast<uint8_t>(p[2]) & 0x3F);
                p += 3;
            } else {
                p++;  // Saltar byte inválido
                continue;
            }
        } else {
            p++;
        }

        bool ok = lv_font_get_glyph_dsc(font, &glyph, letter, 0);
        if (ok) textWidth += glyph.adv_w;
    }

    lv_area_t area;
    area.x1 = x;
    area.y1 = yTop;
    area.x2 = static_cast<int32_t>(x + textWidth);
    area.y2 = static_cast<int32_t>(yTop + font->line_height - 1);

    lv_draw_label(layer, &dsc, &area);
}

void MathCanvas::drawLine(lv_layer_t* layer,
                          int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                          int16_t width, lv_color_t color) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.opa   = LV_OPA_COVER;
    dsc.p1.x  = x1;  dsc.p1.y = y1;
    dsc.p2.x  = x2;  dsc.p2.y = y2;
    dsc.round_start = 0;
    dsc.round_end   = 0;
    lv_draw_line(layer, &dsc);
}

void MathCanvas::drawFilledRect(lv_layer_t* layer,
                                int16_t x, int16_t y, int16_t w, int16_t h,
                                lv_color_t color, lv_opa_t opa) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa   = opa;
    dsc.radius   = 0;

    lv_area_t area;
    area.x1 = x;
    area.y1 = y;
    area.x2 = static_cast<int32_t>(x + w - 1);
    area.y2 = static_cast<int32_t>(y + h - 1);

    lv_draw_rect(layer, &dsc, &area);
}

void MathCanvas::drawBorderRect(lv_layer_t* layer,
                                int16_t x, int16_t y, int16_t w, int16_t h,
                                lv_color_t color, lv_opa_t opa,
                                int16_t borderW) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa        = LV_OPA_TRANSP;
    dsc.border_color  = color;
    dsc.border_opa    = opa;
    dsc.border_width  = borderW;
    dsc.border_side   = LV_BORDER_SIDE_FULL;
    dsc.radius        = 1;

    lv_area_t area;
    area.x1 = x;
    area.y1 = y;
    area.x2 = static_cast<int32_t>(x + w - 1);
    area.y2 = static_cast<int32_t>(y + h - 1);

    lv_draw_rect(layer, &dsc, &area);
}

} // namespace vpam
