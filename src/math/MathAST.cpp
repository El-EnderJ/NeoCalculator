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
 * MathAST.cpp — Implementación del AST y Layout Recursivo V.P.A.M.
 *
 * Fase 1+5: Cálculo geométrico puro (sin LVGL).
 *
 * Cada nodo calcula su bounding box {width, ascent, descent} de forma
 * recursiva bottom-up. Los nodos contenedores (Fraction, Power, Root, Paren,
 * Function, LogBase) primero calculan el layout de sus hijos y luego
 * componen el suyo propio.
 *
 * Convenciones:
 *   · ascent  = píxeles SOBRE el baseline (positivo hacia arriba)
 *   · descent = píxeles BAJO  el baseline (positivo hacia abajo)
 *   · El baseline es la línea imaginaria donde "se apoyan" los dígitos.
 *   · El eje matemático (axis) está a fontSize/2 sobre el baseline:
 *     ahí se dibujan las barras de fracción y se centra ±×.
 */

#include "MathAST.h"
#include "MathRowLayout.h"
#include "MathScriptGeometry.h"
#include "MathOperatorGeometry.h"
#include "MathVectorDelimiterGeometry.h"
#include "InputSymbols.h"
#include "ScientificUncertainty.h"
#include <algorithm>

#include "../ui/MathSymbols.h"
#include "font/MathGlyphAssembly.h"
#include "font/StixParentheses.h"
#include "font/stix_math_italics.h"

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

namespace vpam {

#ifdef NUMOS_MATH_AST_TEST_ALLOCATOR
// Host-only allocator seam; absent from native/web/firmware product builds.
extern void* testNodeAllocate(std::size_t);
extern void testNodeRelease(void*);
#endif

// ════════════════════════════════════════════════════════════════════════════
// MathNode — PSRAM allocation
// ════════════════════════════════════════════════════════════════════════════
void* MathNode::operator new(std::size_t size) {
    void* p = nullptr;
#ifdef NUMOS_MATH_AST_TEST_ALLOCATOR
    p = testNodeAllocate(size);
#elif defined(ARDUINO)
    p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(size, MALLOC_CAP_8BIT); // fallback
#else
    p = std::malloc(size);
#endif
    if (!p) throw std::bad_alloc();
    return p;
}

void MathNode::operator delete(void* ptr) noexcept {
    if (!ptr) return;
#ifdef NUMOS_MATH_AST_TEST_ALLOCATOR
    testNodeRelease(ptr);
#elif defined(ARDUINO)
    heap_caps_free(ptr);
#else
    std::free(ptr);
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// Utilidad interna
// ════════════════════════════════════════════════════════════════════════════
static NodePtr makeEmptyRow() {
    auto row = std::make_unique<NodeRow>();
    row->appendChild(std::make_unique<NodeEmpty>());
    return row;
}

static inline void applyScriptLevel(MathNode* node, const FontMetrics& fm) {
    if (node) node->setScriptLevel(fm.scriptLevel);
}

LayoutStatus calculateLayoutChecked(MathNode& root, const FontMetrics& fm) {
    // WHY: existing transient traversal state avoids a depth-sized stack and
    // global scratch. Semantic fields and AST ownership remain unchanged.
    auto walk = [&](bool inspectBoxes) {
        MathNode* node = &root;
        node->_paint.nextChild = 0;
        int depth = 0, nodes = 1, steps = 0;
        for (;;) {
            if (++steps > kLayoutMaxNodes * 40) return LayoutStatus::InvalidTree;
            const int count = node->childCount();
            if (count < 0 || count > kLayoutMaxNodes) return LayoutStatus::NodeLimit;
            if (inspectBoxes && node->_paint.nextChild == 0) {
                const auto& box = node->_layout;
                if (box.width < 0 || box.ascent < 0 || box.descent < 0 ||
                    box.width > kLayoutMaxExtent ||
                    int32_t(box.ascent) + box.descent > kLayoutMaxExtent)
                    return LayoutStatus::CoordinateLimit;
            }
            if (node->_paint.nextChild == count) {
                if (node == &root) break;
                node = node->parent();
                --depth;
                continue;
            }
            auto* child = node->child(node->_paint.nextChild++);
            // A matrix under construction may have vacant cells. Other node
            // kinds expose only owned, non-null slots through childCount.
            if (!child) {
                if (node->type() == NodeType::Matrix) continue;
                return LayoutStatus::InvalidTree;
            }
            if (child == &root || child->parent() != node) return LayoutStatus::InvalidTree;
            if (++depth > kLayoutMaxDepth) return LayoutStatus::DepthLimit;
            if (++nodes > kLayoutMaxNodes) return LayoutStatus::NodeLimit;
            node = child;
            node->_paint.nextChild = 0;
        }
        return LayoutStatus::Ok;
    };
    auto status = walk(false);
    if (status == LayoutStatus::Ok) {
        bool overflow = false;
        auto checked = fm;
        checked.coordinateOverflow = &overflow;
        root.calculateLayout(checked);
        status = overflow ? LayoutStatus::CoordinateLimit : walk(true);
    }
    if (status != LayoutStatus::Ok) {
        root._layout = {};
        root._layout.ascent = fm.ascent;
        root._layout.descent = fm.descent;
    }
    return status;
}

/// Decode the first Unicode codepoint from a UTF-8 string and return the
/// number of bytes consumed (1–4). Returns 0 for empty/null with outCp=0.
/// Thin wrapper around the canonical vpam::utf8Decode().
static inline uint8_t decodeFirstUtf8(const char* str, uint32_t& outCp) {
    return vpam::utf8Decode(reinterpret_cast<const uint8_t*>(str), outCp);
}

struct AxisDelimiterGeometry {
    int16_t ascent = 0;
    int16_t descent = 0;
    int16_t height = 0;
};

static inline AxisDelimiterGeometry symmetricAxisDelimiterGeometry(
        const LayoutResult& content, const FontMetrics& fm, int16_t padPx) {
    const int16_t axis = fm.axisHeight();
    const int16_t aboveAxis = (content.ascent > axis)
        ? fm.coordinate(content.ascent - axis)
        : fm.coordinate(axis - content.ascent);
    const int16_t belowAxis = fm.coordinate(content.descent + axis);
    const int16_t maxHalf = fm.coordinate(std::max(aboveAxis, belowAxis) + padPx);

    AxisDelimiterGeometry out;
    out.ascent = fm.coordinate(axis + maxHalf);
    out.descent = fm.coordinate(maxHalf - axis);
    if (out.descent < 0) {
        out.ascent = fm.coordinate(out.ascent - out.descent);
        out.descent = 0;
    }
    out.height = fm.coordinate(out.ascent + out.descent);
    return out;
}

// Parentheses enclose the child ink box, not an axis-reflected line box.
// DelimitedSubFormulaMinHeight is not a minimum for every ordinary (x).
static inline AxisDelimiterGeometry parenthesisGeometry(
        const LayoutResult& content, const FontMetrics& fm) {
    AxisDelimiterGeometry out;
    out.ascent = fm.coordinate(layoutInkAscentPx(content) + 1);
    out.descent = fm.coordinate(layoutInkDescentPx(content) + 1);
    out.height = fm.coordinate(out.ascent + out.descent);
    return out;
}

static inline int16_t delimiterVerticalPadPx(
        const LayoutResult& content, const FontMetrics& fm) {
    const int16_t contentMinH = MathConstantsProvider(fm.emSize).delimitedSubFormulaMinHeight();
    return fm.coordinate(std::max<int32_t>(
        1, fm.coordinate((contentMinH - content.height() + 1) / 2)));
}

static inline void expandDelimiterGeometryToAssembly(
        AxisDelimiterGeometry& geom,
        const DelimiterAssembler::AssemblyResult& assembly, const FontMetrics& fm) {
    if (!assembly.valid || assembly.totalHeightPx <= geom.height) return;

    const int16_t extra = fm.coordinate(assembly.totalHeightPx - geom.height);
    geom.ascent = fm.coordinate(geom.ascent + (extra + 1) / 2);
    geom.descent = fm.coordinate(geom.descent + extra / 2);
    geom.height = fm.coordinate(geom.ascent + geom.descent);
}

static inline int16_t assembledDelimiterWidthPx(
        AxisDelimiterGeometry& geom, const FontMetrics& fm, uint32_t baseCp) {
    if (baseCp == '(' || baseCp == ')' || baseCp == '[' || baseCp == ']') {
        const auto plan = stixParenthesisPlan(geom.height, fm.emSize, baseCp == '[' || baseCp == ']');
        const int16_t extra = plan.height - geom.height;
        geom.ascent = fm.coordinate(geom.ascent + (extra + 1) / 2);
        geom.descent = fm.coordinate(geom.descent + extra / 2);
        geom.height = plan.height;
        return plan.width;
    }
    const int16_t glyphFallbackPx = fm.coordinate(std::max<int32_t>(
        2, fm.coordinate(std::max<int32_t>(
               DelimiterAssembler::glyphWidthPx(baseCp, fm.emSize),
               fm.coordinate(fm.charWidth / 2)))));
    const MathVariantTable* table = lookupVariantTable(baseCp);
    if (!table) return std::max(glyphFallbackPx,
        vectorDelimiterGeometry(geom.height, fm.emSize, baseCp).width);

    auto assembly = DelimiterAssembler::assemble(geom.height, *table, fm.emSize);
    // Only grow the delimiter to the assembly's minimum renderable height when the
    // font actually has the assembly glyphs. With the stix_math subset (which omits
    // them) the renderer draws a vector delimiter that scales to any height, so the
    // delimiter should hug its content instead of ballooning to the assembly floor.
    if (g_delimiterAssemblyRenderable) {
        expandDelimiterGeometryToAssembly(geom, assembly, fm);
    }

    int16_t width = assembly.valid ? assembly.widthPx : glyphFallbackPx;
    // WHY: a subset can omit a selected variant/assembly even when its table
    // exists. Every fallback must fit the same reserved slot as layout/cursor.
    return std::max<int16_t>(std::max<int16_t>(2, width),
        vectorDelimiterGeometry(geom.height, fm.emSize, baseCp).width);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e R o w
// ════════════════════════════════════════════════════════════════════════════
NodeRow::NodeRow() : MathNode(NodeType::Row) {}

MathClass NodeRow::mathClass() const {
    // WHY: a container has no single static atom class. RowLayout traverses
    // transparent rows; a boxed multi-atom nucleus is ordinary (TeX sub_mlist).
    if (_children.size() == 1) return atomBoundary(_children.front().get()).left;
    return MathClass::ORD;
}

void NodeRow::calculateLayout(const FontMetrics& fm) {
    RowLayout::calculate(*this, fm);
}

int NodeRow::childCount() const {
    return static_cast<int>(_children.size());
}

MathNode* NodeRow::child(int index) const {
    if (index < 0 || index >= static_cast<int>(_children.size())) return nullptr;
    return _children[static_cast<size_t>(index)].get();
}

void NodeRow::appendChild(NodePtr node) {
    if (!node) return;
    node->setParent(this);
    _children.push_back(std::move(node));
}

void NodeRow::insertChild(int index, NodePtr node) {
    if (!node) return;
    node->setParent(this);
    if (index >= static_cast<int>(_children.size())) {
        _children.push_back(std::move(node));
    } else {
        _children.insert(_children.begin() + index, std::move(node));
    }
}

NodePtr NodeRow::removeChild(int index) {
    if (index < 0 || index >= static_cast<int>(_children.size())) return nullptr;
    NodePtr node = std::move(_children[static_cast<size_t>(index)]);
    _children.erase(_children.begin() + index);
    node->setParent(nullptr);
    return node;
}

void NodeRow::clear() {
    for (auto& c : _children) c->setParent(nullptr);
    _children.clear();
}

void NodeRow::replaceChild(int index, NodePtr node) {
    if (index < 0 || index >= static_cast<int>(_children.size())) return;
    if (!node) return;
    _children[static_cast<size_t>(index)]->setParent(nullptr);
    node->setParent(this);
    _children[static_cast<size_t>(index)] = std::move(node);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e N u m b e r
// ════════════════════════════════════════════════════════════════════════════
NodeNumber::NodeNumber(const std::string& value)
    : MathNode(NodeType::Number), _value(value)
{
}

void NodeNumber::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _layout.width = fm.coordinate(_value.empty() ? fm.charWidth : fm.textAdvance(_value.c_str()));
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
    _layout.inkAscent = fm.coordinate((fm.numberAscent > 0) ? fm.numberAscent : fm.ascent);
    _layout.inkDescent = fm.coordinate((fm.numberAscent > 0 || fm.numberDescent > 0)
        ? fm.numberDescent
        : fm.descent);
}

void NodeNumber::appendChar(char c) {
    // Solo aceptar dígitos y punto decimal (máximo un punto)
    if (c >= '0' && c <= '9') {
        _value += c;
    } else if (c == '.' && !hasDecimalPoint()) {
        _value += c;
    }
}

void NodeNumber::deleteLastChar() {
    if (!_value.empty()) _value.pop_back();
}

bool NodeNumber::hasDecimalPoint() const {
    return _value.find('.') != std::string::npos;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e O p e r a t o r
// ════════════════════════════════════════════════════════════════════════════
NodeOperator::NodeOperator(OpKind op)
    : MathNode(NodeType::Operator), _op(op)
{
}

void NodeOperator::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    if(_op==OpKind::UnitAttach || _op==OpKind::UnitAttachTight) {
        // WHY: semantic spacing is geometry, not invisible string characters.
        _layout.width=fm.coordinate(_op==OpKind::UnitAttach ? MathConstantsProvider(fm.emSize).muToPx(3):0);
        _layout.ascent=fm.ascent;_layout.descent=fm.descent;return;
    }
    // Width is purely the character width. All inter-atom padding is handled
    // exclusively by the TeX spacing logic in NodeRow::calculateLayout.
    // WHY: STIX plus-minus is wider than a digit. Use the captured glyph extent
    // so row layout, cursor advances and the shared glyph draw agree.
    _layout.width = fm.coordinate(fm.measureText ? fm.textAdvance(symbol())
        : (_op == OpKind::PlusMinus && fm.plusMinusWidth > 0) ? fm.plusMinusWidth : fm.charWidth);
    const auto ink = fm.textInk(symbol());
    _layout.ascent = fm.coordinate(std::max(fm.ascent, ink.ascent));
    _layout.descent = fm.coordinate(std::max(fm.descent, ink.descent));
    _layout.inkAscent = ink.ascent;
    _layout.inkDescent = ink.descent;
}

const char* NodeOperator::symbol() const {
    switch (_op) {
        case OpKind::UnitProduct: return "·";
        case OpKind::UnitAttach: case OpKind::UnitAttachTight: return "";
        case OpKind::Add:       return "+";
        case OpKind::Sub:       return "-";              // ASCII hyphen-minus (U+002D)
        case OpKind::Mul:       return numos::mathsym::SYMB_TIMES;
        case OpKind::Div:       return numos::mathsym::SYMB_DIVIDE;
        case OpKind::PlusMinus: return numos::mathsym::SYMB_PLUS_MINUS;
        case OpKind::Eq:        return "=";
        case OpKind::Lt:        return "<";
        case OpKind::Gt:        return ">";
        case OpKind::Le:        return numos::mathsym::SYMB_LEQ;
        case OpKind::Ge:        return numos::mathsym::SYMB_GEQ;
        case OpKind::Ne:        return numos::mathsym::SYMB_NEQ;
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e E m p t y
// ════════════════════════════════════════════════════════════════════════════
NodeEmpty::NodeEmpty() : MathNode(NodeType::Empty) {}

void NodeEmpty::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // El placeholder se alinea con el texto normal por baseline.
    // Su ancho/alto es al menos el mínimo visual, pero no menor que la fuente.
    _layout.width   = fm.coordinate(std::max(MIN_WIDTH,  fm.charWidth));
    _layout.ascent  = fm.coordinate(std::max(MIN_HEIGHT, fm.ascent));
    _layout.descent = fm.descent;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e F r a c t i o n
// ════════════════════════════════════════════════════════════════════════════
NodePtr NodeFraction::makeEmptySlot() {
    return makeEmptyRow();
}

NodeFraction::NodeFraction() : MathNode(NodeType::Fraction) {
    _numerator   = makeEmptySlot();
    _numerator->setParent(this);
    _denominator = makeEmptySlot();
    _denominator->setParent(this);
}

NodeFraction::NodeFraction(NodePtr numerator, NodePtr denominator)
    : MathNode(NodeType::Fraction)
{
    setNumerator(std::move(numerator));
    setDenominator(std::move(denominator));
}

void NodeFraction::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // 1. Calcular layout de hijos con estilo reducido (TeX: num/den bajan un
    //    nivel de estilo). El bar/eje/shift siguen usando el estilo padre `fm`.
    const FontMetrics childFm = fractionPartMetrics(fm);
    _numerator->calculateLayout(childFm);
    _denominator->calculateLayout(fractionDenominatorMetrics(fm));

    const auto& numL = _numerator->layout();
    const auto& denL = _denominator->layout();

    // 2. MATH table-driven layout constants (Spec §7.3)
    const FractionLayoutMetrics metrics = fractionLayoutMetrics(fm);
    _ruleThicknessPx = metrics.ruleThicknessPx;

    // Shared OpenType baseline-shift and ink-gap constraints.
    const FractionBarGaps gaps = fractionBarGaps(fm, numL, denL);
    _numeratorShiftPx = gaps.numeratorShiftPx;
    _denominatorShiftPx = gaps.denominatorShiftPx;

    // 4. Width: widest child + rule overhang (1 mu each side)
    _ruleOverhangPx = metrics.ruleOverhangPx;
    int16_t contentW = std::max(numL.width, denL.width);
    _layout.width = fm.coordinate(contentW + 2 * _ruleOverhangPx);

    // 5. Vertical layout (baseline-relative)
    //    The rule is centered on the math axis above the fraction baseline.
    //    Ascent  = distance from axis to numerator TOP.
    //    Descent = distance from axis to denominator BOTTOM.
    _barHalfUpPx = fm.coordinate((_ruleThicknessPx + 1) / 2);
    _barHalfDownPx = fm.coordinate(_ruleThicknessPx / 2);

    int16_t axis = fm.axisHeight();
    int16_t aboveAxis = fm.coordinate(_barHalfUpPx + _numeratorShiftPx + numL.ascent);
    int16_t belowAxis = fm.coordinate(_barHalfDownPx + _denominatorShiftPx + denL.descent);

    _layout.ascent  = fm.coordinate(axis + aboveAxis);
    _layout.descent = fm.coordinate(belowAxis - axis);

    // Safety: descent must be non-negative
    if (_layout.descent < 0) _layout.descent = fm.coordinate(0);
    _layout.inkAscent = _layout.ascent;
    _layout.inkDescent = _layout.descent;
}

MathNode* NodeFraction::child(int index) const {
    if (index == 0) return _numerator.get();
    if (index == 1) return _denominator.get();
    return nullptr;
}

void NodeFraction::setNumerator(NodePtr node) {
    _numerator = node ? std::move(node) : makeEmptySlot();
    _numerator->setParent(this);
}

void NodeFraction::setDenominator(NodePtr node) {
    _denominator = node ? std::move(node) : makeEmptySlot();
    _denominator->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e P o w e r
// ════════════════════════════════════════════════════════════════════════════
NodePower::NodePower() : MathNode(NodeType::Power) {
    _base     = makeEmptyRow();
    _base->setParent(this);
    _exponent = makeEmptyRow();
    _exponent->setParent(this);
}

NodePower::NodePower(NodePtr base, NodePtr exponent)
    : MathNode(NodeType::Power)
{
    setBase(std::move(base));
    setExponent(std::move(exponent));
}

void NodePower::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // 1. Base en fuente normal
    _base->calculateLayout(fm);
    const auto& baseL = _base->layout();

    // 2. Exponente en fuente reducida (superscript)
    FontMetrics fmSup = fm.superscript();
    _exponent->calculateLayout(fmSup);
    const auto& expL = _exponent->layout();

    // WHY: glyph nuclei use the shift/bottom minima; boxed nuclei additionally
    // constrain baseline drop from the top of their actual measured box.
    const MathConstantsProvider mc(fm.emSize);
    auto* nested = const_cast<MathNode*>(scriptNucleus(_base.get()));
    auto* combined = nested && nested->type() == NodeType::Subscript
        ? static_cast<NodeSubscript*>(nested) : nullptr;
    const MathNode* nucleus = combined ? combined->base() : _base.get();
    const auto& nucleusBox = nucleus->layout();
    const uint32_t baseCp = scriptGlyph(nucleus);
    _scriptShiftPx = superscriptShiftPx(fm, expL);
    if (!isTextScriptNucleus(nucleus)) _scriptShiftPx = fm.coordinate(std::max<int32_t>(_scriptShiftPx,
        nucleusBox.ascent - mc.superscriptBaselineDropMax()));
    if (combined) {
        int16_t down = combined->scriptShiftPx();
        separateScriptPair(fm, expL, combined->subscript()->layout(), _scriptShiftPx, down);
        combined->placeScriptAt(down, fm);
        ScriptLayout::refreshWrappers(combined, _base.get());
    }
    const int16_t expShift = _scriptShiftPx;
    _italicCorrectionPx = baseCp ? fm.coordinate(std::max<int32_t>(0,
        mc.scalePx(lookupItalicsCorrection(baseCp)))) : 0;
    _mathKernPx = scriptMathKernPx(baseCp, scriptGlyph(_exponent.get()),
        fm, fmSup, nucleusBox, expL, expShift, true);
    _scriptX = fm.coordinate(nucleusBox.width - nucleusItalicReserve(nucleus, fm)
        + _italicCorrectionPx + _mathKernPx);
    _layout.width = fm.coordinate(std::max<int32_t>(baseL.width,
        _scriptX + expL.width + spaceAfterScriptPx(fm)));

    // 6. Ascent: the exponent top might extend above the base top
    _layout.ascent  = fm.coordinate(std::max(baseL.ascent,
                               fm.coordinate(expShift + expL.ascent)));
    // Descent: keep the conservative layout box valid even for unusual
    // complex exponents. Simple digit exponents remain fully above baseline.
    _layout.descent = fm.coordinate(std::max<int32_t>(
        baseL.descent,
        fm.coordinate(std::max<int32_t>(
            0, fm.coordinate(expL.descent - expShift)))));

    _layout.inkAscent = fm.coordinate(std::max<int32_t>(
        layoutInkAscentPx(baseL),
        fm.coordinate(expShift + layoutInkAscentPx(expL))));
    _layout.inkDescent = fm.coordinate(std::max<int32_t>(
        layoutInkDescentPx(baseL),
        fm.coordinate(std::max<int32_t>(
            0, fm.coordinate(layoutInkDescentPx(expL) - expShift)))));
}

MathNode* NodePower::child(int index) const {
    if (index == 0) return _base.get();
    if (index == 1) return _exponent.get();
    return nullptr;
}

void NodePower::setBase(NodePtr node) {
    _base = node ? std::move(node) : makeEmptyRow();
    _base->setParent(this);
}

void NodePower::setExponent(NodePtr node) {
    _exponent = node ? std::move(node) : makeEmptyRow();
    _exponent->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e R o o t
// ════════════════════════════════════════════════════════════════════════════
NodeRoot::NodeRoot() : MathNode(NodeType::Root) {
    _radicand = makeEmptyRow();
    _radicand->setParent(this);
    _degree = nullptr;
}

NodeRoot::NodeRoot(NodePtr radicand, NodePtr degree)
    : MathNode(NodeType::Root)
{
    setRadicand(std::move(radicand));
    if (degree) setDegree(std::move(degree));
}

void NodeRoot::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    MathConstantsProvider mc(fm.emSize);
    const bool display = fm.isDisplayStyle();

    // 1. Radicand
    _radicand->calculateLayout(crampedMetrics(fm));
    const auto& radL = _radicand->layout();

    // 2. MATH table-driven layout constants (Spec §2.3, constants 49–55)
    _radicalRuleThickness = mc.radicalRuleThickness();
    _radicalVerticalGap   = mc.radicalVerticalGap(display);
    _radicalExtraAscender = mc.radicalExtraAscender();
    if (_radicalExtraAscender < 1) _radicalExtraAscender = 1;

    // WHY: the degree can enlarge the node, but must not move the radical
    // bar or the radicand baseline. Cache the one placement used by painting
    // and the cursor; a reserved width alone does not move either consumer.
    // WHY: preserve the existing vector shape at em18, scaled in mu at
    // script sizes. The vector policy is explicit: hook3mu, slope6mu, pad2mu.
    _hookWidthPx = fm.coordinate(std::max<int32_t>(1, mc.muToPx(3)));
    _slopeWidthPx = fm.coordinate(std::max<int32_t>(1, mc.muToPx(6)));
    _rightPadPx = fm.coordinate(std::max<int32_t>(1, mc.muToPx(2)));
    _radicalAscent = fm.coordinate(layoutInkAscentPx(radL) + _radicalVerticalGap +
        _radicalRuleThickness + _radicalExtraAscender);
    _layout.ascent = fm.coordinate(std::max(radL.ascent, _radicalAscent));
    _layout.descent = radL.descent;
    _radicalX = fm.coordinate(_degreeX = _degreeBaseline = 0);
    _radicalKernBefore = _radicalKernAfter = 0;
    if (_degree) {
        _degree->calculateLayout(rootDegreeMetrics(fm));
        const auto& degree = _degree->layout();
        _radicalKernBefore = mc.radicalKernBeforeDegree();
        _radicalKernAfter = mc.radicalKernAfterDegree();
        _degreeX = fm.coordinate(std::max<int32_t>(0, _radicalKernBefore));
        // The existing vector radical has no glyph's empty left bearing.
        // Limit negative MATH kern to its hook, leaving 1mu ink clearance.
        const int after = std::max<int>(
            _radicalKernAfter, fm.coordinate(std::max<int32_t>(1, mc.muToPx(1))) - _hookWidthPx);
        _radicalX = fm.coordinate(std::max<int>(0, _degreeX + degree.width + after));
        const int radicalHeight = _radicalAscent - _radicalExtraAscender + radL.descent;
        const int raise = radicalHeight * mc.radicalDegreeBottomRaisePercent() / 100;
        _degreeBaseline = fm.coordinate(radL.descent - raise - layoutInkDescentPx(degree));
        _layout.ascent = fm.coordinate(std::max<int>(_layout.ascent,
                                      degree.ascent - _degreeBaseline));
        _layout.descent = fm.coordinate(std::max<int>(radL.descent,
                                        _degreeBaseline + degree.descent));
    }
    _layout.width = fm.coordinate(radicandX() + radL.width + _rightPadPx);
    _layout.inkAscent = fm.coordinate(std::max<int32_t>(_radicalAscent - _radicalExtraAscender,
        _degree ? layoutInkAscentPx(_degree->layout()) - _degreeBaseline : 0));
    _layout.inkDescent = _layout.descent;

}

int NodeRoot::childCount() const {
    return _degree ? 2 : 1;
}

MathNode* NodeRoot::child(int index) const {
    if (index == 0) return _radicand.get();
    if (index == 1 && _degree) return _degree.get();
    return nullptr;
}

void NodeRoot::setRadicand(NodePtr node) {
    _radicand = node ? std::move(node) : makeEmptyRow();
    _radicand->setParent(this);
}

void NodeRoot::setDegree(NodePtr node) {
    if (node) {
        _degree = std::move(node);
        _degree->setParent(this);
    } else {
        _degree.reset();
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e P a r e n
// ════════════════════════════════════════════════════════════════════════════
NodeParen::NodeParen(DelimKind delimKind)
    : MathNode(NodeType::Paren), _delimKind(delimKind)
{
    _content = makeEmptyRow();
    _content->setParent(this);
}

NodeParen::NodeParen(NodePtr content, DelimKind delimKind)
    : MathNode(NodeType::Paren), _delimKind(delimKind)
{
    setContent(std::move(content));
}

void NodeParen::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _content->calculateLayout(fm);
    const auto& cl = _content->layout();

    const int16_t padPx = delimiterVerticalPadPx(cl, fm);
    AxisDelimiterGeometry geom = (_delimKind == DelimKind::Paren || _delimKind == DelimKind::Bracket)
        ? parenthesisGeometry(cl, fm)
        : symmetricAxisDelimiterGeometry(cl, fm, padPx);
    _parenWidth = assembledDelimiterWidthPx(geom, fm, leftCp());

    // ── Layout: [leftParen] + pad + content + pad + [rightParen] ──
    const int16_t innerPadPx = innerPad();
    _layout.width = fm.coordinate(_parenWidth * 2 + innerPadPx * 2 + cl.width);
    // WHY: delimiters follow ink, while the group still owns the complete
    // child's logical slot (including cursor/placeholder padding).
    _layout.ascent  = fm.coordinate(std::max(geom.ascent, cl.ascent));
    _layout.descent = fm.coordinate(std::max(geom.descent, cl.descent));
    _layout.inkAscent = fm.coordinate(std::max(geom.ascent, layoutInkAscentPx(cl)));
    _layout.inkDescent = fm.coordinate(std::max(geom.descent, layoutInkDescentPx(cl)));
}

MathNode* NodeParen::child(int index) const {
    if (index == 0) return _content.get();
    return nullptr;
}

void NodeParen::setContent(NodePtr node) {
    _content = node ? std::move(node) : makeEmptyRow();
    _content->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e F u n c t i o n
// ════════════════════════════════════════════════════════════════════════════
NodeFunction::NodeFunction()
    : MathNode(NodeType::Function), _kind(FuncKind::Sin), _labelWidth(0)
{
    _argument = makeEmptyRow();
    _argument->setParent(this);
}

NodeFunction::NodeFunction(FuncKind kind, NodePtr argument)
    : MathNode(NodeType::Function), _kind(kind), _labelWidth(0)
{
    setArgument(std::move(argument));
}

const char* NodeFunction::label() const {
    switch (_kind) {
        case FuncKind::Factorial: return "!";
        case FuncKind::Sin:    return "sin";
        case FuncKind::Cos:    return "cos";
        case FuncKind::Tan:    return "tan";
        case FuncKind::ArcSin: return "sin\xe2\x81\xbb\xc2\xb9";  // sin⁻¹
        case FuncKind::ArcCos: return "cos\xe2\x81\xbb\xc2\xb9";  // cos⁻¹
        case FuncKind::ArcTan: return "tan\xe2\x81\xbb\xc2\xb9";  // tan⁻¹
        case FuncKind::Ln:     return "ln";
        case FuncKind::Log:    return "log";
    }
    return "?";
}

void NodeFunction::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // WHY: proportional label advance must end where the drawn run ends.
    const auto labelInk = fm.textInk(label());
    _labelWidth = labelInk.width;

    // 2. Argumento entre paréntesis
    _argument->calculateLayout(fm);
    const auto& argL = _argument->layout();

    AxisDelimiterGeometry geom = parenthesisGeometry(argL, fm);
    _parenWidth = assembledDelimiterWidthPx(geom, fm, 0x0028);
    _innerPad = fm.coordinate(std::max<int32_t>(1, _parenWidth / 3));
    _parenAscent = geom.ascent;
    _parenDescent = geom.descent;

    if (_kind == FuncKind::Factorial &&
        (_argument->type() != NodeType::Row || _argument->childCount() == 1)) {
        _parenWidth = _innerPad = 0;
        _parenAscent = argL.ascent;
        _parenDescent = argL.descent;
    }

    // 3. Ancho total: etiqueta + gap + ( + pad + argumento + pad + )
    _layout.width = fm.coordinate(_labelWidth + LABEL_GAP
                  + _parenWidth + _innerPad + argL.width + _innerPad + _parenWidth);

    // 4. The full node must cover both label text and the argument delimiter.
    _layout.ascent  = fm.coordinate(std::max({fm.ascent, labelInk.ascent, _parenAscent, argL.ascent}));
    _layout.descent = fm.coordinate(std::max({fm.descent, labelInk.descent, _parenDescent, argL.descent}));
    _layout.inkAscent = fm.coordinate(std::max(labelInk.ascent, _parenAscent));
    _layout.inkDescent = fm.coordinate(std::max(labelInk.descent, _parenDescent));
}

MathNode* NodeFunction::child(int index) const {
    if (index == 0) return _argument.get();
    return nullptr;
}

void NodeFunction::setArgument(NodePtr node) {
    _argument = node ? std::move(node) : makeEmptyRow();
    _argument->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e L o g B a s e
// ════════════════════════════════════════════════════════════════════════════
NodeLogBase::NodeLogBase()
    : MathNode(NodeType::LogBase), _labelWidth(0)
{
    _base = makeEmptyRow();
    _base->setParent(this);
    _argument = makeEmptyRow();
    _argument->setParent(this);
}

NodeLogBase::NodeLogBase(NodePtr base, NodePtr argument)
    : MathNode(NodeType::LogBase), _labelWidth(0)
{
    setBase(std::move(base));
    setArgument(std::move(argument));
}

void NodeLogBase::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    const auto labelInk = fm.textInk("log");
    _labelWidth = labelInk.width;

    // 2. Subíndice (base) en fuente reducida
    FontMetrics fmSub = subscriptMetrics(fm);   // misma escala que superscript
    _base->calculateLayout(fmSub);
    const auto& baseL = _base->layout();

    // 3. Argumento entre paréntesis
    _argument->calculateLayout(fm);
    const auto& argL = _argument->layout();

    AxisDelimiterGeometry geom = parenthesisGeometry(argL, fm);
    _parenWidth = assembledDelimiterWidthPx(geom, fm, 0x0028);
    _innerPad = fm.coordinate(std::max<int32_t>(1, _parenWidth / 3));
    _parenAscent = geom.ascent;
    _parenDescent = geom.descent;

    // 4. Ancho total: "log" + base_subscript + SpaceAfterScript + gap + ( + pad + arg + pad + )
    //    SpaceAfterScript is added after the subscript base to match NodePower
    //    and NodeSubscript behavior (OpenType constant 17).
    _layout.width = fm.coordinate(_labelWidth + baseL.width
                  + spaceAfterScriptPx(fm) + LABEL_GAP
                  + _parenWidth + _innerPad + argL.width + _innerPad + _parenWidth);

    LayoutResult label{};
    label.ascent = std::max(fm.ascent, labelInk.ascent);
    label.descent = std::max(fm.descent, labelInk.descent);
    _scriptShiftPx = subscriptShiftPx(fm, label, baseL, true);
    _layout.ascent = fm.coordinate(std::max({label.ascent, _parenAscent, argL.ascent,
        fm.coordinate(baseL.ascent - _scriptShiftPx)}));
    _layout.descent = fm.coordinate(std::max({label.descent, _parenDescent, argL.descent,
        fm.coordinate(_scriptShiftPx + baseL.descent)}));
}

MathNode* NodeLogBase::child(int index) const {
    if (index == 0) return _base.get();
    if (index == 1) return _argument.get();
    return nullptr;
}

void NodeLogBase::setBase(NodePtr node) {
    _base = node ? std::move(node) : makeEmptyRow();
    _base->setParent(this);
}

void NodeLogBase::setArgument(NodePtr node) {
    _argument = node ? std::move(node) : makeEmptyRow();
    _argument->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e C o n s t a n t
// ════════════════════════════════════════════════════════════════════════════
NodeConstant::NodeConstant(ConstKind kind)
    : MathNode(NodeType::Constant), _kind(kind)
{
}

const char* NodeConstant::symbol() const {
    switch (_kind) {
        case ConstKind::Pi:   return numos::mathsym::SYMB_PI;
        case ConstKind::E:    return "e";          // Euler's number
        case ConstKind::Imag: return "i";          // imaginary unit
    }
    return "?";
}

// WHY: digits alone do not describe Greek/Latin descenders or italic overhang.
// The font adapter measures actual ink once; rendering uses this cached offset.
static int16_t layoutTextAtom(MathNode* node,LayoutResult& layout,const char* text,const FontMetrics& fm) {
    applyScriptLevel(node,fm);
    const auto ink=fm.textInk(text);
    layout.width=ink.width;
    layout.ascent=std::max(fm.ascent,ink.ascent);
    layout.descent=std::max(fm.descent,ink.descent);
    layout.inkAscent=ink.ascent;
    layout.inkDescent=ink.descent;
    return ink.leftPad;
}
// Single-letter identifiers use a real mathematical italic glyph. Their
// advance stays distinct from ink overhang; script attachment consumes the
// advance plus MATH italic correction/cut-in, never a second ink-width pad.
static int16_t layoutIdentifierAtom(MathNode* node, LayoutResult& layout,
                                    const char* text, const FontMetrics& fm) {
    char buffer[5];
    const char* display = identifierDisplayText(text, buffer);
    uint32_t cp = 0;
    const auto count = utf8Decode(reinterpret_cast<const uint8_t*>(display), cp);
    if (count && !display[count] && isMathItalicGlyph(cp) && fm.measureAtom && fm.textFont) {
        applyScriptLevel(node, fm);
        const auto ink = fm.textInk(display);
        // TeX appends italic correction to an ordinary math glyph's advance.
        // Script attachment removes this reserve before applying its own rule.
        layout.width = ink.advance + fm.coordinate(std::max<int32_t>(0,
            MathConstantsProvider(fm.emSize).scalePx(lookupItalicsCorrection(cp))));
        layout.ascent = std::max(fm.ascent, ink.ascent);
        layout.descent = std::max(fm.descent, ink.descent);
        layout.inkAscent = ink.ascent;
        layout.inkDescent = ink.descent;
        return 0;
    }
    return layoutTextAtom(node, layout, display, fm);
}
void NodeConstant::calculateLayout(const FontMetrics& fm) {
    _textOffset=layoutTextAtom(this,_layout,symbol(),fm);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e V a r i a b l e
// ════════════════════════════════════════════════════════════════════════════
NodeVariable::NodeVariable(char name)
    : MathNode(NodeType::Variable), _name(name), _labelWidth(0)
{
}

const char* NodeVariable::label() const {
    switch (_name) {
        case '#': return "Ans";
        case '$': return "PreAns";
        case 'A': return "A";
        case 'B': return "B";
        case 'C': return "C";
        case 'D': return "D";
        case 'E': return "E";
        case 'F': return "F";
        case 'G': return "G";
        case 'H': return "H";
        case 'I': return "I";
        case 'J': return "J";
        case 'K': return "K";
        case 'L': return "L";
        case 'M': return "M";
        case 'N': return "N";
        case 'O': return "O";
        case 'P': return "P";
        case 'Q': return "Q";
        case 'R': return "R";
        case 'S': return "S";
        case 'T': return "T";
        case 'U': return "U";
        case 'V': return "V";
        case 'W': return "W";
        case 'X': return "X";
        case 'Y': return "Y";
        case 'Z': return "Z";
        case 'a': return "a";
        case 'b': return "b";
        case 'c': return "c";
        case 'd': return "d";
        case 'e': return "e";
        case 'f': return "f";
        case 'g': return "g";
        case 'h': return "h";
        case 'i': return "i";
        case 'j': return "j";
        case 'k': return "k";
        case 'l': return "l";
        case 'm': return "m";
        case 'n': return "n";
        case 'o': return "o";
        case 'p': return "p";
        case 'q': return "q";
        case 'r': return "r";
        case 's': return "s";
        case 't': return "t";
        case 'u': return "u";
        case 'v': return "v";
        case 'w': return "w";
        case 'x': return "x";
        case 'y': return "y";
        case 'z': return "z";
        // Relational operators entered as variable glyphs in the Grapher (for
        // equations / inequalities). Without these explicit cases '<' and '>'
        // fell through to the default and rendered as a literal "?" in the
        // expression rows (the bug Phase 10E fixes). '=' was already handled.
        case '=': return "=";
        case '<': return "<";
        case '>': return ">";
        default:  return "?";
    }
}

void NodeVariable::calculateLayout(const FontMetrics& fm) {
    _textOffset=layoutIdentifierAtom(this,_layout,label(),fm);
    _labelWidth=_layout.width;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e P e r i o d i c D e c i m a l
// ════════════════════════════════════════════════════════════════════════════
NodePeriodicDecimal::NodePeriodicDecimal(const std::string& intPart,
                                         const std::string& nonRepeat,
                                         const std::string& repeat,
                                         bool negative)
    : MathNode(NodeType::PeriodicDecimal)
    , _intPart(intPart), _nonRepeat(nonRepeat)
    , _repeat(repeat), _negative(negative)
{
}

void NodePeriodicDecimal::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // Each component is a separate drawn run (no kerning across components).
    _overbarWidth = fm.textAdvance(_repeat.c_str());
    _layout.width = fm.coordinate(prefixAdvance(fm) + _overbarWidth);
    if (_layout.width == 0) _layout.width = fm.charWidth;

    const auto integerInk = fm.textInk(_intPart.c_str());
    const auto nonRepeatInk = fm.textInk(_nonRepeat.c_str());
    const auto repeatInk = fm.textInk(_repeat.c_str());
    _layout.inkAscent = std::max({integerInk.ascent, nonRepeatInk.ascent, repeatInk.ascent});
    _layout.inkDescent = std::max({integerInk.descent, nonRepeatInk.descent, repeatInk.descent});
    _layout.ascent = std::max(fm.ascent, _layout.inkAscent);
    _layout.descent = std::max(fm.descent, _layout.inkDescent);
    _overbarTop = _overbarThickness = 0;
    if (!_repeat.empty()) {
        const MathConstantsProvider mc(fm.emSize);
        _overbarThickness = mc.overbarRuleThickness();
        _overbarTop = fm.coordinate(-repeatInk.ascent - mc.overbarVerticalGap() - _overbarThickness);
        // WHY: ExtraAscender is logical padding above the rule; the gap is
        // measured below the rule against the repeated digits' actual ink.
        _layout.inkAscent = std::max<int16_t>(_layout.inkAscent, -_overbarTop);
        _layout.ascent = fm.coordinate(std::max<int32_t>(_layout.ascent,
            -_overbarTop + mc.overbarExtraAscender()));
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e D e f I n t e g r a l
// ════════════════════════════════════════════════════════════════════════════
NodeDefIntegral::NodeDefIntegral()
    : MathNode(NodeType::DefIntegral)
    , _lower(makeEmptyRow())
    , _upper(makeEmptyRow())
    , _body(makeEmptyRow())
    , _variable(makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
    _variable->setParent(this);
}

NodeDefIntegral::NodeDefIntegral(NodePtr lower, NodePtr upper,
                                 NodePtr body, NodePtr variable)
    : MathNode(NodeType::DefIntegral)
    , _lower(lower ? std::move(lower) : makeEmptyRow())
    , _upper(upper ? std::move(upper) : makeEmptyRow())
    , _body(body ? std::move(body) : makeEmptyRow())
    , _variable(variable ? std::move(variable) : makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
    _variable->setParent(this);
}

MathNode* NodeDefIntegral::child(int index) const {
    switch (index) {
        case 0: return _lower.get();
        case 1: return _upper.get();
        case 2: return _body.get();
        case 3: return _variable.get();
        default: return nullptr;
    }
}

void NodeDefIntegral::setLower(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _lower = std::move(node);
    _lower->setParent(this);
}

void NodeDefIntegral::setUpper(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _upper = std::move(node);
    _upper->setParent(this);
}

void NodeDefIntegral::setBody(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _body = std::move(node);
    _body->setParent(this);
}

void NodeDefIntegral::setVariable(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _variable = std::move(node);
    _variable->setParent(this);
}

void NodeDefIntegral::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _body->calculateLayout(fm);
    _lower->calculateLayout(subscriptMetrics(fm));
    _upper->calculateLayout(fm.superscript());
    _layout = largeOperatorGeometry(0x222B, fm, _lower->layout(),
        _upper->layout(), _body->layout()).box;
    _variable->calculateLayout(fm);
    _layout = integralDifferentialGeometry(fm, _layout, _variable->layout()).box;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e S u m m a t i o n
// ════════════════════════════════════════════════════════════════════════════
NodeSummation::NodeSummation()
    : MathNode(NodeType::Summation)
    , _lower(makeEmptyRow())
    , _upper(makeEmptyRow())
    , _body(makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
}

NodeSummation::NodeSummation(NodePtr lower, NodePtr upper,
                             NodePtr body)
    : MathNode(NodeType::Summation)
    , _lower(lower ? std::move(lower) : makeEmptyRow())
    , _upper(upper ? std::move(upper) : makeEmptyRow())
    , _body(body ? std::move(body) : makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
}

MathNode* NodeSummation::child(int index) const {
    switch (index) {
        case 0: return _lower.get();
        case 1: return _upper.get();
        case 2: return _body.get();
        default: return nullptr;
    }
}

void NodeSummation::setLower(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _lower = std::move(node);
    _lower->setParent(this);
}

void NodeSummation::setUpper(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _upper = std::move(node);
    _upper->setParent(this);
}

void NodeSummation::setBody(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _body = std::move(node);
    _body->setParent(this);
}

void NodeSummation::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _body->calculateLayout(fm);
    _lower->calculateLayout(subscriptMetrics(fm));
    _upper->calculateLayout(fm.superscript());
    _layout = largeOperatorGeometry(0x2211, fm, _lower->layout(),
        _upper->layout(), _body->layout()).box;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e B i g O p
// ════════════════════════════════════════════════════════════════════════════
NodeBigOp::NodeBigOp()
    : MathNode(NodeType::BigOp)
    , _kind(BigOpKind::Product)
    , _operatorCp(bigOpGlyph(BigOpKind::Product).cp)
    , _operatorUtf8(bigOpGlyph(BigOpKind::Product).utf8)
    , _lower(makeEmptyRow())
    , _upper(makeEmptyRow())
    , _body(makeEmptyRow())
    , _useDisplayLimits(false)
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
}

NodeBigOp::NodeBigOp(BigOpKind kind, NodePtr lower, NodePtr upper, NodePtr body)
    : MathNode(NodeType::BigOp)
    , _kind(kind)
    , _lower(lower ? std::move(lower) : makeEmptyRow())
    , _upper(upper ? std::move(upper) : makeEmptyRow())
    , _body(body ? std::move(body) : makeEmptyRow())
    , _useDisplayLimits(false)
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);

    const BigOpGlyph g = bigOpGlyph(_kind);
    _operatorCp   = g.cp;
    _operatorUtf8 = g.utf8;
}


MathNode* NodeBigOp::child(int index) const {
    switch (index) {
        case 0: return _lower.get();
        case 1: return _upper.get();
        case 2: return _body.get();
        default: return nullptr;
    }
}

void NodeBigOp::setLower(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _lower = std::move(node);
    _lower->setParent(this);
}

void NodeBigOp::setUpper(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _upper = std::move(node);
    _upper->setParent(this);
}

void NodeBigOp::setBody(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _body = std::move(node);
    _body->setParent(this);
}

void NodeBigOp::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _body->calculateLayout(fm);
    _lower->calculateLayout(subscriptMetrics(fm));
    _upper->calculateLayout(fm.superscript());
    const auto geometry = largeOperatorGeometry(_operatorCp, fm, _lower->layout(),
        _upper->layout(), _body->layout());
    _layout = geometry.box;
    // TXP §H.9.4 / GAP-37: BigOp obeys the same gate as Summation and DefIntegral.
    _useDisplayLimits = geometry.displayLimits;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e S u b s c r i p t
// ════════════════════════════════════════════════════════════════════════════
NodeSubscript::NodeSubscript() : MathNode(NodeType::Subscript) {
    _base = makeEmptyRow();
    _base->setParent(this);
    _subscript = makeEmptyRow();
    _subscript->setParent(this);
}

NodeSubscript::NodeSubscript(NodePtr base, NodePtr subscript)
    : MathNode(NodeType::Subscript)
{
    setBase(std::move(base));
    setSubscript(std::move(subscript));
}

void NodeSubscript::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // 1. Base en fuente normal
    _base->calculateLayout(fm);
    const auto& baseL = _base->layout();

    // 2. Subíndice en fuente reducida (same scale as superscript, ~70%)
    FontMetrics fmSub = subscriptMetrics(fm);
    _subscript->calculateLayout(fmSub);
    const auto& subL = _subscript->layout();

    placeScriptAt(subscriptShiftPx(fm, baseL, subL, isTextScriptNucleus(_base.get())), fm);
}

void NodeSubscript::placeScriptAt(int16_t shift, const FontMetrics& fm) {
    const auto& baseL = _base->layout();
    const auto& subL = _subscript->layout();
    const auto fmSub = subscriptMetrics(fm);
    const uint32_t baseCp = scriptGlyph(_base.get());
    _scriptShiftPx = shift;
    _mathKernPx = scriptMathKernPx(baseCp, scriptGlyph(_subscript.get()),
        fm, fmSub, baseL, subL, _scriptShiftPx, false);
    _scriptX = fm.coordinate(baseL.width - nucleusItalicReserve(_base.get(), fm) + _mathKernPx);
    _layout.width = fm.coordinate(std::max<int32_t>(baseL.width,
        _scriptX + subL.width + spaceAfterScriptPx(fm)));
    // WHY: a shift is a baseline displacement, not a top coordinate.
    _layout.ascent = fm.coordinate(std::max<int32_t>(baseL.ascent, subL.ascent - _scriptShiftPx));
    _layout.descent = fm.coordinate(std::max<int32_t>(baseL.descent, _scriptShiftPx + subL.descent));
    _layout.inkAscent = fm.coordinate(std::max<int32_t>(layoutInkAscentPx(baseL),
        layoutInkAscentPx(subL) - _scriptShiftPx));
    _layout.inkDescent = fm.coordinate(std::max<int32_t>(layoutInkDescentPx(baseL),
        _scriptShiftPx + layoutInkDescentPx(subL)));
}

MathNode* NodeSubscript::child(int index) const {
    if (index == 0) return _base.get();
    if (index == 1) return _subscript.get();
    return nullptr;
}

void NodeSubscript::setBase(NodePtr node) {
    _base = node ? std::move(node) : makeEmptyRow();
    _base->setParent(this);
}

void NodeSubscript::setSubscript(NodePtr node) {
    _subscript = node ? std::move(node) : makeEmptyRow();
    _subscript->setParent(this);
}

// MATH-RESULTS-01 semantic result nodes. Ownership is established before
// layout; the routines below use fixed-capacity geometry and allocate nothing.


static int16_t boundedTextWidth(const char* text,
                                const FontMetrics& fm) {
    return fm.coordinate(!*text ? fm.charWidth : fm.textAdvance(text));
}

static void setTextLayout(MathNode* node, LayoutResult& layout,
                          const char* text, const FontMetrics& fm) {
    applyScriptLevel(node, fm);
    const auto ink = fm.textInk(text);
    layout.width = ink.width;
    layout.ascent = std::max(fm.ascent, ink.ascent);
    layout.descent = std::max(fm.descent, ink.descent);
    layout.inkAscent = ink.ascent;
    layout.inkDescent = ink.descent;
}

NodePtr makeUnit(numos::units::Atom atom) {
    if(!numos::units::valid(atom))return {};
    return NodePtr(new NodeUnit(numos::units::normalized(atom)));
}
void NodeUnit::calculateLayout(const FontMetrics& fm) {
    char text[32]{};numos::units::symbol(_atom,text,sizeof(text));
    _textOffset=layoutTextAtom(this,_layout,text,fm);
}

NodePtr makeQuantityReference(numos::units::ReferenceAtom atom) {
    if (!numos::units::valid(atom)) return {};
    const auto& definition = *numos::units::reference(atom.reference);
    // Registry strings are immutable; bounds protect the constructor from bad
    // generated metadata as well as keeping each owned notation tree tiny.
    auto bounded = [](const char* text, bool required) {
        if (!text) return !required;
        unsigned length = 0;
        while (text[length] && length < 48) ++length;
        return length < 48 && (!required || length != 0);
    };
    if (!bounded(definition.base, true) || !bounded(definition.subscript, false) ||
        !bounded(definition.denominatorBase, false) || !bounded(definition.denominatorSubscript, false) ||
        !bounded(definition.superscript, false)) return {};
    auto notationPart = [&definition](const char* base, const char* subscript, const char* prefix) -> NodePtr {
        std::string text(prefix ? prefix : "");
        uint32_t codepoint = 0;
        const auto bytes = utf8Decode(reinterpret_cast<const uint8_t*>(base), codepoint);
        if (definition.kind == numos::units::ReferenceKind::PhysicalConstant && bytes && base[bytes] == 0) {
            // WHY: only the private notation of physical quantities is italic.
            // Unit symbols, prefixes and descriptive subscripts remain upright;
            // no global variable normalization or font/style policy is changed.
            if (codepoint >= 'A' && codepoint <= 'Z') codepoint += 0x1D434 - 'A';
            else if (codepoint == 'h') codepoint = 0x210E;
            else if (codepoint >= 'a' && codepoint <= 'z') codepoint += 0x1D44E - 'a';
            else if (codepoint >= 0x391 && codepoint <= 0x3A9 && codepoint != 0x3A2) codepoint += 0x1D6E2 - 0x391;
            else if (codepoint >= 0x3B1 && codepoint <= 0x3C9) codepoint += 0x1D6FC - 0x3B1;
            char glyph[5]{};
            if (codepoint < 0x80) glyph[0] = char(codepoint);
            else if (codepoint < 0x800) { glyph[0] = char(0xC0 | (codepoint >> 6)); glyph[1] = char(0x80 | (codepoint & 63)); }
            else if (codepoint < 0x10000) {
                glyph[0] = char(0xE0 | (codepoint >> 12)); glyph[1] = char(0x80 | ((codepoint >> 6) & 63)); glyph[2] = char(0x80 | (codepoint & 63));
            } else {
                glyph[0] = char(0xF0 | (codepoint >> 18)); glyph[1] = char(0x80 | ((codepoint >> 12) & 63));
                glyph[2] = char(0x80 | ((codepoint >> 6) & 63)); glyph[3] = char(0x80 | (codepoint & 63));
            }
            text += glyph;
        } else text += base;
        auto glyphs = makeSymbol(text);
        if (subscript && *subscript) return makeSubscript(std::move(glyphs), std::make_unique<NodeSymbol>(subscript, true));
        return glyphs;
    };
    auto notation = notationPart(definition.base, definition.subscript,
                                numos::units::prefix(atom.prefix)->symbol);
    if (definition.superscript && *definition.superscript)
        notation = makePower(std::move(notation), makeSymbol(definition.superscript));
    if (definition.denominatorBase && *definition.denominatorBase)
        notation = makeFraction(std::move(notation),
            notationPart(definition.denominatorBase, definition.denominatorSubscript, nullptr));
    else if (definition.denominatorSubscript && *definition.denominatorSubscript) return {};
    return NodePtr(new NodeQuantityReference(atom, std::move(notation)));
}

void NodeQuantityReference::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _notation->calculateLayout(fm);
    // Same rectangle and baseline for layout, common paint traversal and the
    // containing row's cursor; no duplicated script or fraction formula.
    _layout = _notation->layout();
}

NodeMeasurement::NodeMeasurement(NodePtr nominal, NodePtr uncertainty, uint32_t sourceId)
    : MathNode(NodeType::Measurement),
      _nominal(nominal ? std::move(nominal) : makeEmptyRow()),
      _uncertainty(uncertainty ? std::move(uncertainty) : makeEmptyRow()), _sourceId(sourceId) {
    _nominal->setParent(this); _uncertainty->setParent(this);
}
NodePtr makeMeasurement(NodePtr nominal, NodePtr uncertainty, uint32_t sourceId) {
    if(!sourceId) sourceId=numos::scientific::allocateUserSource();
    return NodePtr(new NodeMeasurement(std::move(nominal),std::move(uncertainty),sourceId));
}
void NodeMeasurement::calculateLayout(const FontMetrics& fm) {
    // WHY: measured glyph ink and MATH spacing are cached here. Paint and
    // cursor consume these placements; neither allocates nor propagates u.
    LayoutResult sign{};
    const auto pad=layoutTextAtom(this,sign,"±",fm);
    _nominal->calculateLayout(fm); _uncertainty->calculateLayout(fm);
    const auto& a=_nominal->layout(); const auto& b=_uncertainty->layout();
    const int gap=std::max<int>(1,MathConstantsProvider(fm.emSize).muToPx(4));
    _signX=fm.coordinate(a.width+gap+pad);
    _uncertaintyX=fm.coordinate(a.width+2*gap+sign.width);
    _layout.width=fm.coordinate(_uncertaintyX+b.width);
    _layout.ascent=fm.coordinate(_layout.inkAscent=std::max({a.ascent,b.ascent,sign.ascent}));
    _layout.descent=fm.coordinate(_layout.inkDescent=std::max({a.descent,b.descent,sign.descent}));
    // WHY: the measured pair is one operand, including in products and before
    // an attached unit. Reuse the established delimiter geometry and cache all
    // child origins for the painter/cursor instead of exposing an ambiguous ±.
    auto geometry=parenthesisGeometry(_layout, fm);
    _delimiterWidth=assembledDelimiterWidthPx(geometry,fm,0x0028);
    _nominalX=_delimiterWidth+std::max<int>(1,MathConstantsProvider(fm.emSize).muToPx(1));
    _signX=fm.coordinate(_signX+_nominalX);
    _uncertaintyX=fm.coordinate(_uncertaintyX+_nominalX);
    _layout.width=fm.coordinate(_layout.width+2*_nominalX);
    // WHY: parent fraction/root gaps must clear the final physical fences.
    // Their full painted span also encloses the child logical editing slots.
    _layout.ascent=_layout.inkAscent=geometry.ascent;
    _layout.descent=_layout.inkDescent=geometry.descent;
}

UnitScan scanUnits(const MathNode* root) {
    if(!root)return UnitScan::None;
    // WHY: inspect the authored tree before any zero-product/cancellation pass.
    struct Frame {const MathNode* node=nullptr;uint16_t next=0;};
    std::array<Frame,82> stack{};size_t depth=1,visited=0;stack[0]={root,0};
    bool found=false;
    while(depth) {
        auto& frame=stack[depth-1];const auto* n=frame.node;
        if(!frame.next) {
            if(++visited>400)return UnitScan::Limit;
            if(n->type()==NodeType::Unit) {
                if(!numos::units::valid(static_cast<const NodeUnit*>(n)->atom()))return UnitScan::Invalid;
                found=true;
            }
            if(n->type()==NodeType::QuantityReference) {
                if(!numos::units::valid(static_cast<const NodeQuantityReference*>(n)->atom()))return UnitScan::Invalid;
                found=true;
            }
            if(n->type()==NodeType::Measurement) found=true;
        }
        if(frame.next>=n->childCount()){--depth;continue;}
        const auto* child=n->child(frame.next++);if(!child)continue;
        // WHY: bound depth, not the number of siblings. A flat 300-node input
        // remains inside Calculation's existing 400-node admission contract.
        if(depth==stack.size())return UnitScan::Limit;
        stack[depth++]={child,0};
    }
    return found?UnitScan::Present:UnitScan::None;
}

NodeSymbol::NodeSymbol(std::string name) : NodeSymbol(std::move(name), false) {}

NodeSymbol::NodeSymbol(std::string name, bool upright)
    : NodeSymbol(std::move(name), upright, MathClass::ORD) {}

NodeSymbol::NodeSymbol(std::string name, bool upright, MathClass atomClass)
    : MathNode(NodeType::Symbol), _name(std::move(name)), _upright(upright), _atomClass(atomClass) {}

void NodeSymbol::calculateLayout(const FontMetrics& fm) {
    _textOffset=0;
    if (_name == "\xCE\x94" && fm.deltaWidth) {
        // WHY: a UTF-8 quantity is one glyph, not one-byte variable D. The
        // canvas captures its real font box before allocation-free layout.
        applyScriptLevel(this, fm);
        _layout.width = fm.deltaWidth;
        _layout.ascent = fm.coordinate(_layout.inkAscent = fm.deltaAscent);
        _layout.descent = fm.coordinate(_layout.inkDescent = fm.deltaDescent);
        return;
    }
    _textOffset = _upright ? layoutTextAtom(this,_layout,_name.c_str(),fm)
        : layoutIdentifierAtom(this,_layout,_name.c_str(),fm);
}

NodeSpecialValue::NodeSpecialValue(SpecialValueKind kind)
    : MathNode(NodeType::SpecialValue), _kind(kind) {}

const char* NodeSpecialValue::label() const {
    switch (_kind) {
        case SpecialValueKind::PositiveInfinity: return "+\xE2\x88\x9E";
        case SpecialValueKind::NegativeInfinity: return "-\xE2\x88\x9E";
        case SpecialValueKind::UnsignedInfinity: return "\xE2\x88\x9E";
        case SpecialValueKind::Undefined:        return "undefined";
        case SpecialValueKind::BarePositiveInfinity: return "\xE2\x88\x9E";
        case SpecialValueKind::BothInfinities: return "\xC2\xB1\xE2\x88\x9E";
    }
    return "undefined";
}

void NodeSpecialValue::calculateLayout(const FontMetrics& fm) {
    setTextLayout(this, _layout, label(), fm);
}

NodeCollection::NodeCollection(CollectionKind kind)
    : MathNode(NodeType::Collection), _kind(kind) {}

int NodeCollection::childCount() const {
    return static_cast<int>(_elements.size());
}

MathNode* NodeCollection::child(int index) const {
    return index >= 0 && index < childCount() ? _elements[index].get() : nullptr;
}

void NodeCollection::appendElement(NodePtr element) {
    if (!element || _elements.size() >= 32) return;
    element->setParent(this);
    _elements.push_back(std::move(element));
}

void NodeCollection::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    LayoutResult content{};
    content.ascent = fm.ascent;
    content.descent = fm.descent;
    _separatorWidth = fm.coordinate(fm.textAdvance(",") +
        MathConstantsProvider(fm.emSize).muToPx(3));
    for (size_t i = 0; i < _elements.size(); ++i) {
        _elements[i]->calculateLayout(fm);
        const auto& childLayout = _elements[i]->layout();
        content.width = fm.coordinate(static_cast<int32_t>(content.width) + childLayout.width +
            (i + 1 < _elements.size() ? _separatorWidth : 0));
        content.ascent = std::max(content.ascent, childLayout.ascent);
        content.descent = std::max(content.descent, childLayout.descent);
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, leftCp());
    _innerPad = fm.coordinate(std::max<int32_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(1)));
    _layout.width = fm.coordinate(static_cast<int32_t>(content.width) + 2 * _delimiterWidth +
        2 * _innerPad);
    _layout.ascent = geom.ascent;
    _layout.descent = geom.descent;
}

NodeEquation::NodeEquation(NodePtr lhs, NodePtr rhs, EquationKind kind)
    : MathNode(NodeType::Equation),
      _lhs(lhs ? std::move(lhs) : makeEmptyRow()),
      _rhs(rhs ? std::move(rhs) : makeEmptyRow()), _kind(kind) {
    _lhs->setParent(this);
    _rhs->setParent(this);
}

MathNode* NodeEquation::child(int index) const {
    if (index == 0) return _lhs.get();
    if (index == 1) return _rhs.get();
    return nullptr;
}

void NodeEquation::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _lhs->calculateLayout(fm);
    _rhs->calculateLayout(fm);
    const auto relationInk = fm.textInk("=");
    _relationWidth = relationInk.width;
    _relationGap = fm.coordinate(std::max<int32_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(5)));
    _layout.width = fm.coordinate(static_cast<int32_t>(_lhs->layout().width) + _rhs->layout().width +
        _relationWidth + 2 * _relationGap);
    _layout.ascent = fm.coordinate(std::max({_lhs->layout().ascent,
                               _rhs->layout().ascent, fm.ascent, relationInk.ascent}));
    _layout.descent = fm.coordinate(std::max({_lhs->layout().descent,
                                _rhs->layout().descent, fm.descent, relationInk.descent}));
}

NodeMatrix::NodeMatrix(uint8_t rows, uint8_t columns)
    : MathNode(NodeType::Matrix),
      _rows(std::min(rows, kCapacity)),
      _columns(std::min(columns, kCapacity)) {
    _cells.resize(static_cast<size_t>(_rows) * _columns);
}

int NodeMatrix::childCount() const { return static_cast<int>(_cells.size()); }

MathNode* NodeMatrix::child(int index) const {
    return index >= 0 && index < childCount() ? _cells[index].get() : nullptr;
}

MathNode* NodeMatrix::cell(uint8_t row, uint8_t column) const {
    if (row >= _rows || column >= _columns) return nullptr;
    return _cells[static_cast<size_t>(row) * _columns + column].get();
}

bool NodeMatrix::setCell(uint8_t row, uint8_t column, NodePtr value) {
    if (!value || row >= _rows || column >= _columns) return false;
    value->setParent(this);
    _cells[static_cast<size_t>(row) * _columns + column] = std::move(value);
    return true;
}

int16_t NodeMatrix::columnWidth(uint8_t column) const {
    return column < _columns ? _columnWidths[column] : 0;
}

int16_t NodeMatrix::rowBaseline(uint8_t row) const {
    return row < _rows ? _rowBaselines[row] : 0;
}

void NodeMatrix::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _columnWidths.fill(0);
    _rowAscents.fill(0);
    _rowDescents.fill(0);
    const auto mc = MathConstantsProvider(fm.emSize);
    _horizontalPad = fm.coordinate(std::max<int32_t>(1, mc.muToPx(2)));
    _verticalPad = fm.coordinate(std::max<int32_t>(1, mc.muToPx(2)));
    for (uint8_t row = 0; row < _rows; ++row) {
        for (uint8_t column = 0; column < _columns; ++column) {
            MathNode* value = cell(row, column);
            if (!value) continue;
            value->calculateLayout(fm);
            const auto& cellLayout = value->layout();
            _columnWidths[column] = std::max(_columnWidths[column], cellLayout.width);
            _rowAscents[row] = std::max(_rowAscents[row], cellLayout.ascent);
            _rowDescents[row] = std::max(_rowDescents[row], cellLayout.descent);
        }
        if (_rowAscents[row] == 0) _rowAscents[row] = fm.ascent;
        if (_rowDescents[row] == 0) _rowDescents[row] = fm.descent;
    }
    int32_t contentWidth = 0;
    for (uint8_t column = 0; column < _columns; ++column)
        contentWidth += _columnWidths[column] + 2 * _horizontalPad;
    int32_t contentHeight = 0;
    for (uint8_t row = 0; row < _rows; ++row)
        contentHeight += _rowAscents[row] + _rowDescents[row] +
            (row + 1 < _rows ? _verticalPad : 0);
    contentHeight = fm.coordinate(contentHeight);
    const int16_t axis = fm.axisHeight();
    LayoutResult content{};
    content.width = fm.coordinate(contentWidth);
    content.ascent = fm.coordinate(axis + (contentHeight + 1) / 2);
    content.descent = fm.coordinate(std::max<int32_t>(0, contentHeight - content.ascent));
    int16_t top = fm.coordinate(-content.ascent);
    for (uint8_t row = 0; row < _rows; ++row) {
        _rowBaselines[row] = fm.coordinate(top + _rowAscents[row]);
        top = fm.coordinate(top + _rowAscents[row] +
            _rowDescents[row] + (row + 1 < _rows ? _verticalPad : 0));
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, 0x005B);
    _layout.width = fm.coordinate(contentWidth + 2 * _delimiterWidth);
    _layout.ascent = geom.ascent;
    _layout.descent = geom.descent;
}

NodeInterval::NodeInterval(NodePtr lower, NodePtr upper,
                           bool leftClosed, bool rightClosed)
    : MathNode(NodeType::Interval),
      _lower(lower ? std::move(lower) : makeEmptyRow()),
      _upper(upper ? std::move(upper) : makeEmptyRow()),
      _leftClosed(leftClosed), _rightClosed(rightClosed) {
    _lower->setParent(this);
    _upper->setParent(this);
}

MathNode* NodeInterval::child(int index) const {
    if (index == 0) return _lower.get();
    if (index == 1) return _upper.get();
    return nullptr;
}

void NodeInterval::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _lower->calculateLayout(fm);
    _upper->calculateLayout(fm);
    _separatorWidth = fm.coordinate(fm.textAdvance(",") +
        MathConstantsProvider(fm.emSize).muToPx(3));
    _innerPad = fm.coordinate(std::max<int32_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(1)));
    LayoutResult content{};
    content.width = fm.coordinate(static_cast<int32_t>(_lower->layout().width) + _upper->layout().width +
        _separatorWidth);
    content.ascent = std::max(_lower->layout().ascent, _upper->layout().ascent);
    content.descent = std::max(_lower->layout().descent, _upper->layout().descent);
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    auto leftGeometry = geom;
    auto rightGeometry = geom;
    // WHY: both sides can have different native bitmap heights and widths.
    // Select against the same request and union, without repeated enlargement.
    const int16_t leftWidth = assembledDelimiterWidthPx(leftGeometry, fm, leftCp());
    const int16_t rightWidth = assembledDelimiterWidthPx(rightGeometry, fm, rightCp());
    _delimiterWidth = std::max(leftWidth, rightWidth);
    _leftDelimiterAscent = leftGeometry.ascent;
    _leftDelimiterDescent = leftGeometry.descent;
    _rightDelimiterAscent = rightGeometry.ascent;
    _rightDelimiterDescent = rightGeometry.descent;
    _layout.width = fm.coordinate(static_cast<int32_t>(content.width) + 2 * _delimiterWidth + 2 * _innerPad);
    _layout.ascent = std::max(leftGeometry.ascent, rightGeometry.ascent);
    _layout.descent = std::max(leftGeometry.descent, rightGeometry.descent);
}

NodePiecewise::NodePiecewise() : MathNode(NodeType::Piecewise) {}

bool NodePiecewise::appendBranch(NodePtr expression, NodePtr condition,
                                 bool otherwise) {
    if (!expression || _branches.size() >= kCapacity ||
        (!otherwise && !condition)) return false;
    expression->setParent(this);
    if (condition) condition->setParent(this);
    _branches.push_back({std::move(expression), std::move(condition), otherwise});
    return true;
}

int NodePiecewise::childCount() const {
    int count = 0;
    for (const auto& branch : _branches)
        count += branch.condition ? 2 : 1;
    return count;
}

MathNode* NodePiecewise::child(int index) const {
    for (const auto& branch : _branches) {
        if (index-- == 0) return branch.expression.get();
        if (branch.condition && index-- == 0) return branch.condition.get();
    }
    return nullptr;
}

void NodePiecewise::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _expressionColumnWidth = 0;
    _conditionColumnWidth = 0;
    _columnGap = fm.coordinate(std::max<int32_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(4)));
    const int16_t rowGap = fm.coordinate(std::max<int32_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(2)));
    std::array<int16_t, kCapacity> rowAscents{};
    std::array<int16_t, kCapacity> rowDescents{};
    int32_t totalHeight = 0;
    for (size_t i = 0; i < _branches.size(); ++i) {
        auto& branch = _branches[i];
        branch.expression->calculateLayout(fm);
        const auto& expressionLayout = branch.expression->layout();
        _expressionColumnWidth = std::max(_expressionColumnWidth,
                                          expressionLayout.width);
        rowAscents[i] = expressionLayout.ascent;
        rowDescents[i] = expressionLayout.descent;
        const auto labelInk = fm.textInk(branch.condition ? "if " : "otherwise");
        rowAscents[i] = std::max(rowAscents[i], labelInk.ascent);
        rowDescents[i] = std::max(rowDescents[i], labelInk.descent);
        if (branch.condition) {
            branch.condition->calculateLayout(fm);
            const auto& conditionLayout = branch.condition->layout();
            _conditionColumnWidth = std::max(_conditionColumnWidth,
                fm.coordinate(conditionLayout.width +
                    boundedTextWidth("if ", fm)));
            rowAscents[i] = std::max(rowAscents[i], conditionLayout.ascent);
            rowDescents[i] = std::max(rowDescents[i], conditionLayout.descent);
        } else {
            _conditionColumnWidth = std::max(_conditionColumnWidth,
                boundedTextWidth("otherwise", fm));
        }
        totalHeight += rowAscents[i] + rowDescents[i] +
            (i + 1 < _branches.size() ? rowGap : 0);
    }
    totalHeight = fm.coordinate(totalHeight);
    const int16_t axis = fm.axisHeight();
    LayoutResult content{};
    content.width = fm.coordinate(static_cast<int32_t>(_expressionColumnWidth) + _columnGap +
        _conditionColumnWidth);
    content.ascent = fm.coordinate(axis + (totalHeight + 1) / 2);
    content.descent = fm.coordinate(std::max<int32_t>(0, totalHeight - content.ascent));
    int16_t top = fm.coordinate(-content.ascent);
    for (size_t i = 0; i < _branches.size(); ++i) {
        _rowBaselines[i] = fm.coordinate(top + rowAscents[i]);
        top = fm.coordinate(top + rowAscents[i] + rowDescents[i] +
            (i + 1 < _branches.size() ? rowGap : 0));
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _braceWidth = assembledDelimiterWidthPx(geom, fm, 0x007B);
    _bracePad = MathConstantsProvider(fm.emSize).muToPx(2);
    _layout.width = fm.coordinate(static_cast<int32_t>(content.width) + _braceWidth +
        _bracePad);
    _layout.ascent = geom.ascent;
    _layout.descent = geom.descent;
}

NodeCall::NodeCall(std::string name)
    : MathNode(NodeType::Call), _name(std::move(name)) {}

void NodeCall::appendArgument(NodePtr argument) {
    if (!argument || _arguments.size() >= 32) return;
    argument->setParent(this);
    _arguments.push_back(std::move(argument));
}

int NodeCall::childCount() const { return static_cast<int>(_arguments.size()); }

MathNode* NodeCall::child(int index) const {
    return index >= 0 && index < childCount() ? _arguments[index].get() : nullptr;
}

void NodeCall::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    const auto labelInk = fm.textInk(_name.c_str());
    _labelWidth = labelInk.width;
    _textOffset = labelInk.leftPad;
    _separatorWidth = fm.coordinate(fm.textAdvance(",") +
        MathConstantsProvider(fm.emSize).muToPx(3));
    LayoutResult content{};
    content.ascent = fm.ascent;
    content.descent = fm.descent;
    for (size_t i = 0; i < _arguments.size(); ++i) {
        _arguments[i]->calculateLayout(fm);
        const auto& argumentLayout = _arguments[i]->layout();
        content.width = fm.coordinate(static_cast<int32_t>(content.width) + argumentLayout.width +
            (i + 1 < _arguments.size() ? _separatorWidth : 0));
        content.ascent = std::max(content.ascent, argumentLayout.ascent);
        content.descent = std::max(content.descent, argumentLayout.descent);
    }
    AxisDelimiterGeometry geom = parenthesisGeometry(content, fm);
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, 0x0028);
    _innerPad = fm.coordinate(std::max<int32_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(1)));
    _labelGap = MathConstantsProvider(fm.emSize).muToPx(2);
    _layout.width = fm.coordinate(static_cast<int32_t>(_labelWidth) + content.width + 2 * _delimiterWidth +
        2 * _innerPad + _labelGap);
    _layout.ascent = fm.coordinate(std::max({geom.ascent, fm.ascent, labelInk.ascent}));
    _layout.descent = fm.coordinate(std::max({geom.descent, fm.descent, labelInk.descent}));
}

NodeUnevaluated::NodeUnevaluated(NodePtr expression)
    : MathNode(NodeType::Unevaluated),
      _expression(expression ? std::move(expression) : makeEmptyRow()) {
    _expression->setParent(this);
}

MathNode* NodeUnevaluated::child(int index) const {
    return index == 0 ? _expression.get() : nullptr;
}

void NodeUnevaluated::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    _expression->calculateLayout(fm);
    _labelWidth = boundedTextWidth("unevaluated", fm);
    _gap = fm.coordinate(std::max<int32_t>(1, MathConstantsProvider(fm.emSize).muToPx(4)));
    _layout.width = fm.coordinate(static_cast<int32_t>(_labelWidth) + _gap + _expression->layout().width);
    _layout.ascent = fm.coordinate(std::max<int32_t>(fm.ascent, _expression->layout().ascent));
    _layout.descent = fm.coordinate(std::max<int32_t>(fm.descent, _expression->layout().descent));
}

// ════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════
//  F a c t o r y   H e l p e r s
// ════════════════════════════════════════════════════════════════════════════
NodePtr makeRow() {
    return std::make_unique<NodeRow>();
}

NodePtr makeNumber(const std::string& value) {
    return std::make_unique<NodeNumber>(value);
}

NodePtr makeOperator(OpKind op) {
    return std::make_unique<NodeOperator>(op);
}

NodePtr makeRelation(OpKind op) {
    return std::make_unique<NodeRelation>(op);
}

NodePtr makeEmpty() {
    return std::make_unique<NodeEmpty>();
}

NodePtr makeFraction(NodePtr num, NodePtr den) {
    if (num || den) {
        return std::make_unique<NodeFraction>(std::move(num), std::move(den));
    }
    return std::make_unique<NodeFraction>();   // Dos slots vacíos
}

NodePtr makePower(NodePtr base, NodePtr exp) {
    if (base || exp) {
        return std::make_unique<NodePower>(std::move(base), std::move(exp));
    }
    return std::make_unique<NodePower>();
}

NodePtr makeRoot(NodePtr radicand, NodePtr degree) {
    if (radicand || degree) {
        return std::make_unique<NodeRoot>(std::move(radicand), std::move(degree));
    }
    return std::make_unique<NodeRoot>();
}

NodePtr makeParen(NodePtr content, DelimKind kind) {
    if (content) {
        return std::make_unique<NodeParen>(std::move(content), kind);
    }
    return std::make_unique<NodeParen>(kind);
}

NodePtr makeFunction(FuncKind kind, NodePtr argument) {
    return std::make_unique<NodeFunction>(kind, std::move(argument));
}

NodePtr makeLogBase(NodePtr base, NodePtr argument) {
    if (base || argument) {
        return std::make_unique<NodeLogBase>(std::move(base), std::move(argument));
    }
    return std::make_unique<NodeLogBase>();
}

NodePtr makeConstant(ConstKind kind) {
    return std::make_unique<NodeConstant>(kind);
}

NodePtr makeVariable(char name) {
    return std::make_unique<NodeVariable>(name);
}

NodePtr makeSymbol(const std::string& name) {
    return std::make_unique<NodeSymbol>(name);
}

NodePtr makeSpecialValue(SpecialValueKind kind) {
    return std::make_unique<NodeSpecialValue>(kind);
}

NodePtr makeCollection(CollectionKind kind) {
    return std::make_unique<NodeCollection>(kind);
}

NodePtr makeEquation(NodePtr lhs, NodePtr rhs, EquationKind kind) {
    return std::make_unique<NodeEquation>(std::move(lhs), std::move(rhs), kind);
}

NodePtr makeMatrix(uint8_t rows, uint8_t columns) {
    return std::make_unique<NodeMatrix>(rows, columns);
}

NodePtr makeInterval(NodePtr lower, NodePtr upper,
                     bool leftClosed, bool rightClosed) {
    return std::make_unique<NodeInterval>(std::move(lower), std::move(upper),
                                          leftClosed, rightClosed);
}

NodePtr makePiecewise() {
    return std::make_unique<NodePiecewise>();
}

NodePtr makeCall(const std::string& name) {
    return std::make_unique<NodeCall>(name);
}

NodePtr makeUnevaluated(NodePtr expression) {
    return std::make_unique<NodeUnevaluated>(std::move(expression));
}

NodePtr makePeriodicDecimal(const std::string& intPart,
                            const std::string& nonRepeat,
                            const std::string& repeat,
                            bool negative) {
    return std::make_unique<NodePeriodicDecimal>(intPart, nonRepeat, repeat, negative);
}

NodePtr makeDefIntegral(NodePtr lower, NodePtr upper,
                        NodePtr body, NodePtr variable) {
    if (lower || upper || body || variable) {
        return std::make_unique<NodeDefIntegral>(std::move(lower), std::move(upper),
                                                 std::move(body), std::move(variable));
    }
    return std::make_unique<NodeDefIntegral>();
}

NodePtr makeSummation(NodePtr lower, NodePtr upper,
                      NodePtr body) {
    if (lower || upper || body) {
        return std::make_unique<NodeSummation>(std::move(lower), std::move(upper),
                                               std::move(body));
    }
    return std::make_unique<NodeSummation>();
}

NodePtr makeBigOp(BigOpKind kind, NodePtr lower, NodePtr upper, NodePtr body) {
    return std::make_unique<NodeBigOp>(kind, std::move(lower),
                                       std::move(upper), std::move(body));
}

NodePtr makeSubscript(NodePtr base, NodePtr subscript) {
    if (base || subscript) {
        return std::make_unique<NodeSubscript>(std::move(base), std::move(subscript));
    }
    return std::make_unique<NodeSubscript>();
}

// ════════════════════════════════════════════════════════════════════════════
//  D e b u g   D u m p
// ════════════════════════════════════════════════════════════════════════════
static void appendIndent(std::string& out, int indent) {
    for (int i = 0; i < indent; ++i) out += "  ";
}

std::string dumpTree(const MathNode* node, int indent) {
    if (!node) return "(null)\n";

    std::string out;
    appendIndent(out, indent);

    const auto& l = node->layout();
    // Sufijo con métricas: [w×h  asc/desc]
    auto metrics = [&]() -> std::string {
        return "  [" + std::to_string(l.width) + "x" + std::to_string(l.height())
             + " a" + std::to_string(l.ascent) + "/d" + std::to_string(l.descent)
             + "]";
    };

    switch (node->type()) {
        case NodeType::Row: {
            out += "Row" + metrics() + "\n";
            auto* row = static_cast<const NodeRow*>(node);
            for (int i = 0; i < row->childCount(); ++i) {
                out += dumpTree(row->child(i), indent + 1);
            }
            break;
        }
        case NodeType::Number: {
            auto* num = static_cast<const NodeNumber*>(node);
            out += "Number \"" + num->value() + "\"" + metrics() + "\n";
            break;
        }
        case NodeType::Operator: {
            auto* op = static_cast<const NodeOperator*>(node);
            out += std::string("Operator ") + op->symbol() + metrics() + "\n";
            break;
        }
        case NodeType::Empty: {
            out += "Empty \xe2\x96\xa1" + metrics() + "\n";  // □ U+25A1
            break;
        }
        case NodeType::Fraction: {
            out += "Fraction" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "num:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "den:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::Power: {
            out += "Power" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "base:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "exp:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::Root: {
            auto* root = static_cast<const NodeRoot*>(node);
            out += std::string("Root") + (root->hasDegree() ? "(n)" : "") + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "radicand:\n";
            out += dumpTree(node->child(0), indent + 2);
            if (root->hasDegree()) {
                appendIndent(out, indent + 1);
                out += "degree:\n";
                out += dumpTree(node->child(1), indent + 2);
            }
            break;
        }
        case NodeType::Paren: {
            auto* paren = static_cast<const NodeParen*>(node);
            const char* delimName = "";
            switch (paren->delimKind()) {
                case DelimKind::Paren:   delimName = "()"; break;
                case DelimKind::Bracket: delimName = "[]"; break;
                case DelimKind::Brace:   delimName = "{}"; break;
                case DelimKind::Bar:     delimName = "||"; break;
            }
            out += std::string("Paren ") + delimName + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "content:\n";
            out += dumpTree(node->child(0), indent + 2);
            break;
        }
        case NodeType::Function: {
            auto* func = static_cast<const NodeFunction*>(node);
            out += std::string("Function ") + func->label() + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "arg:\n";
            out += dumpTree(node->child(0), indent + 2);
            break;
        }
        case NodeType::LogBase: {
            out += "LogBase" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "base:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "arg:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::Constant: {
            auto* cnst = static_cast<const NodeConstant*>(node);
            out += std::string("Constant ") + cnst->symbol() + metrics() + "\n";
            break;
        }
        case NodeType::Variable: {
            auto* vr = static_cast<const NodeVariable*>(node);
            out += std::string("Variable \"") + vr->label() + "\"" + metrics() + "\n";
            break;
        }
        case NodeType::PeriodicDecimal: {
            auto* pd = static_cast<const NodePeriodicDecimal*>(node);
            out += "PeriodicDecimal " + pd->intPart() + "."
                 + pd->nonRepeat() + "[" + pd->repeat() + "]"
                 + metrics() + "\n";
            break;
        }
        case NodeType::DefIntegral: {
            out += "DefIntegral" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "lower:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "upper:\n";
            out += dumpTree(node->child(1), indent + 2);
            appendIndent(out, indent + 1);
            out += "body:\n";
            out += dumpTree(node->child(2), indent + 2);
            appendIndent(out, indent + 1);
            out += "var:\n";
            out += dumpTree(node->child(3), indent + 2);
            break;
        }
        case NodeType::Summation: {
            out += "Summation" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "lower:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "upper:\n";
            out += dumpTree(node->child(1), indent + 2);
            appendIndent(out, indent + 1);
            out += "body:\n";
            out += dumpTree(node->child(2), indent + 2);
            break;
        }
        case NodeType::Subscript: {
            out += "Subscript" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "base:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "sub:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::BigOp: {
            auto* bo = static_cast<const NodeBigOp*>(node);
            out += std::string("BigOp ") + bo->operatorUtf8() + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "lower:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "upper:\n";
            out += dumpTree(node->child(1), indent + 2);
            appendIndent(out, indent + 1);
            out += "body:\n";
            out += dumpTree(node->child(2), indent + 2);
            break;
        }
        case NodeType::Unit: {
            const auto a=static_cast<const NodeUnit*>(node)->atom();char text[32]{};
            numos::units::symbol(a,text,sizeof(text));
            out += "Unit["+std::to_string(uint16_t(a.unit))+":"+std::to_string(uint8_t(a.prefix))+"] "+text+metrics()+"\n";break;
        }
        case NodeType::Measurement: {
            out += "Measurement["+std::to_string(static_cast<const NodeMeasurement*>(node)->sourceId())+"]"+metrics()+"\n";
            out += dumpTree(node->child(0),indent+1); out += dumpTree(node->child(1),indent+1); break;
        }
        case NodeType::QuantityReference: {
            const auto a=static_cast<const NodeQuantityReference*>(node)->atom();
            out += "QuantityReference["+std::to_string(uint16_t(a.reference))+":"+
                std::to_string(uint8_t(a.prefix))+"]"+metrics()+"\n";
            out += dumpTree(node->child(0), indent + 1);
            break;
        }
        case NodeType::Symbol:
            out += "Symbol \"" + static_cast<const NodeSymbol*>(node)->name() +
                   "\"" + metrics() + "\n";
            break;
        case NodeType::SpecialValue:
            out += std::string("SpecialValue ") +
                   static_cast<const NodeSpecialValue*>(node)->label() +
                   metrics() + "\n";
            break;
        case NodeType::Collection:
        case NodeType::Equation:
        case NodeType::Matrix:
        case NodeType::Interval:
        case NodeType::Piecewise:
        case NodeType::Call:
        case NodeType::Unevaluated: {
            const char* name = "Semantic";
            switch (node->type()) {
                case NodeType::Collection: name = "Collection"; break;
                case NodeType::Equation: name = "Equation"; break;
                case NodeType::Matrix: name = "Matrix"; break;
                case NodeType::Interval: name = "Interval"; break;
                case NodeType::Piecewise: name = "Piecewise"; break;
                case NodeType::Call: name = "Call"; break;
                case NodeType::Unevaluated: name = "Unevaluated"; break;
                default: break;
            }
            out += std::string(name) + metrics() + "\n";
            for (int i = 0; i < node->childCount(); ++i)
                out += dumpTree(node->child(i), indent + 1);
            break;
        }
    }

    return out;
}

// ════════════════════════════════════════════════════════════════════════════
// cloneNode() — Deep clone recursivo del AST
// ════════════════════════════════════════════════════════════════════════════

NodePtr cloneNode(const MathNode* node) {
    if (!node) return nullptr;

    switch (node->type()) {
        case NodeType::Row: {
            auto* row = static_cast<const NodeRow*>(node);
            auto out = makeRow();
            auto* outRow = static_cast<NodeRow*>(out.get());
            for (int i = 0; i < row->childCount(); ++i) {
                outRow->appendChild(cloneNode(row->child(i)));
            }
            return out;
        }
        case NodeType::Number: {
            auto* num = static_cast<const NodeNumber*>(node);
            return makeNumber(num->value());
        }
        case NodeType::Operator: {
            auto* op = static_cast<const NodeOperator*>(node);
            return makeOperator(op->op());
        }
        case NodeType::Empty:
            return makeEmpty();

        case NodeType::Fraction: {
            auto* frac = static_cast<const NodeFraction*>(node);
            return makeFraction(cloneNode(frac->numerator()),
                                cloneNode(frac->denominator()));
        }
        case NodeType::Power: {
            auto* pow = static_cast<const NodePower*>(node);
            return makePower(cloneNode(pow->base()),
                             cloneNode(pow->exponent()));
        }
        case NodeType::Root: {
            auto* root = static_cast<const NodeRoot*>(node);
            return makeRoot(cloneNode(root->radicand()),
                            root->hasDegree() ? cloneNode(root->degree()) : nullptr);
        }
        case NodeType::Paren: {
            auto* paren = static_cast<const NodeParen*>(node);
            return makeParen(cloneNode(paren->content()), paren->delimKind());
        }
        case NodeType::Function: {
            auto* fn = static_cast<const NodeFunction*>(node);
            auto result = makeFunction(fn->funcKind(), cloneNode(fn->argument()));
            static_cast<NodeFunction*>(result.get())->setGeneratedOperatorSpacing(fn->generatedOperatorSpacing());
            return result;
        }
        case NodeType::LogBase: {
            auto* lb = static_cast<const NodeLogBase*>(node);
            return makeLogBase(cloneNode(lb->base()), cloneNode(lb->argument()));
        }
        case NodeType::Constant: {
            auto* cnst = static_cast<const NodeConstant*>(node);
            return makeConstant(cnst->constKind());
        }
        case NodeType::Variable: {
            auto* var = static_cast<const NodeVariable*>(node);
            return makeVariable(var->name());
        }
        case NodeType::PeriodicDecimal: {
            auto* pd = static_cast<const NodePeriodicDecimal*>(node);
            return makePeriodicDecimal(pd->intPart(), pd->nonRepeat(),
                                       pd->repeat(), pd->isNegative());
        }
        case NodeType::DefIntegral: {
            auto* di = static_cast<const NodeDefIntegral*>(node);
            return makeDefIntegral(cloneNode(di->lower()), cloneNode(di->upper()),
                                   cloneNode(di->body()), cloneNode(di->variable()));
        }
        case NodeType::Summation: {
            auto* sm = static_cast<const NodeSummation*>(node);
            return makeSummation(cloneNode(sm->lower()), cloneNode(sm->upper()),
                                 cloneNode(sm->body()));
        }
        case NodeType::Subscript: {
            auto* sub = static_cast<const NodeSubscript*>(node);
            return makeSubscript(cloneNode(sub->base()),
                                 cloneNode(sub->subscript()));
        }
        case NodeType::BigOp: {
            auto* bo = static_cast<const NodeBigOp*>(node);
            return makeBigOp(bo->bigOpKind(),
                             cloneNode(bo->lower()),
                             cloneNode(bo->upper()),
                             cloneNode(bo->body()));
        }
        case NodeType::Unit:
            return makeUnit(static_cast<const NodeUnit*>(node)->atom());
        case NodeType::Measurement:
            return makeMeasurement(cloneNode(node->child(0)),cloneNode(node->child(1)),
                static_cast<const NodeMeasurement*>(node)->sourceId());
        case NodeType::QuantityReference:
            return makeQuantityReference(static_cast<const NodeQuantityReference*>(node)->atom());
        case NodeType::Symbol:
            // WHY: Tutor's explicit membership REL role survives slicing.
            return std::make_unique<NodeSymbol>(static_cast<const NodeSymbol*>(node)->name(),
                static_cast<const NodeSymbol*>(node)->upright(), node->mathClass());
        case NodeType::SpecialValue:
            return makeSpecialValue(
                static_cast<const NodeSpecialValue*>(node)->specialKind());
        case NodeType::Collection: {
            const auto* source = static_cast<const NodeCollection*>(node);
            auto result = makeCollection(source->collectionKind());
            auto* collection = static_cast<NodeCollection*>(result.get());
            for (int i = 0; i < source->childCount(); ++i)
                collection->appendElement(cloneNode(source->child(i)));
            return result;
        }
        case NodeType::Equation: {
            const auto* equation = static_cast<const NodeEquation*>(node);
            return makeEquation(cloneNode(equation->lhs()),
                                cloneNode(equation->rhs()),
                                equation->equationKind());
        }
        case NodeType::Matrix: {
            const auto* source = static_cast<const NodeMatrix*>(node);
            auto result = makeMatrix(source->rows(), source->columns());
            auto* matrix = static_cast<NodeMatrix*>(result.get());
            for (uint8_t row = 0; row < source->rows(); ++row)
                for (uint8_t column = 0; column < source->columns(); ++column)
                    matrix->setCell(row, column,
                        cloneNode(source->cell(row, column)));
            return result;
        }
        case NodeType::Interval: {
            const auto* interval = static_cast<const NodeInterval*>(node);
            return makeInterval(cloneNode(interval->lower()),
                                cloneNode(interval->upper()),
                                interval->leftClosed(),
                                interval->rightClosed());
        }
        case NodeType::Piecewise: {
            const auto* source = static_cast<const NodePiecewise*>(node);
            auto result = makePiecewise();
            auto* piecewise = static_cast<NodePiecewise*>(result.get());
            for (uint8_t i = 0; i < source->branchCount(); ++i) {
                const auto& branch = source->branch(i);
                piecewise->appendBranch(cloneNode(branch.expression.get()),
                    cloneNode(branch.condition.get()), branch.otherwise);
            }
            return result;
        }
        case NodeType::Call: {
            const auto* source = static_cast<const NodeCall*>(node);
            auto result = makeCall(source->name());
            auto* call = static_cast<NodeCall*>(result.get());
            for (int i = 0; i < source->childCount(); ++i)
                call->appendArgument(cloneNode(source->child(i)));
            return result;
        }
        case NodeType::Unevaluated: {
            const auto* value = static_cast<const NodeUnevaluated*>(node);
            return makeUnevaluated(cloneNode(value->expression()));
        }
    }
    return nullptr;  // unreachable
}

// ════════════════════════════════════════════════════════════════════════════
// classifyCursorTargetRow — Allocation-free cursor target slot classifier
// ════════════════════════════════════════════════════════════════════════════

namespace {

constexpr int kCursorTargetSearchMaxDepth = 16;

const NodeRow* asRowNode(const MathNode* node) {
    if (!node || node->type() != NodeType::Row) return nullptr;
    return static_cast<const NodeRow*>(node);
}

CursorTargetRole searchRoleInNode(const MathNode* node,
                                  const NodeRow* target,
                                  int depth);

CursorTargetRole searchRoleInSlot(const MathNode* slot,
                                  const NodeRow* target,
                                  CursorTargetRole roleIfDirectMatch,
                                  int depth) {
    const NodeRow* slotRow = asRowNode(slot);
    if (slotRow && slotRow == target) return roleIfDirectMatch;
    if (slot) {
        const CursorTargetRole found = searchRoleInNode(slot, target, depth + 1);
        if (found != CursorTargetRole::Other) return found;
    }
    return CursorTargetRole::Other;
}

CursorTargetRole searchRoleInNode(const MathNode* node,
                                  const NodeRow* target,
                                  int depth) {
    if (!node || !target || depth > kCursorTargetSearchMaxDepth) {
        return CursorTargetRole::Other;
    }

    switch (node->type()) {
        case NodeType::Row: {
            const auto* row = static_cast<const NodeRow*>(node);
            for (int i = 0; i < row->childCount(); ++i) {
                const CursorTargetRole found =
                    searchRoleInNode(row->child(i), target, depth + 1);
                if (found != CursorTargetRole::Other) return found;
            }
            return CursorTargetRole::Other;
        }
        case NodeType::Fraction: {
            const auto* frac = static_cast<const NodeFraction*>(node);
            CursorTargetRole found = searchRoleInSlot(
                frac->numerator(), target, CursorTargetRole::FractionNumerator, depth);
            if (found != CursorTargetRole::Other) return found;
            return searchRoleInSlot(
                frac->denominator(), target, CursorTargetRole::FractionDenominator, depth);
        }
        case NodeType::Power: {
            const auto* pow = static_cast<const NodePower*>(node);
            CursorTargetRole found = searchRoleInSlot(
                pow->base(), target, CursorTargetRole::PowerBase, depth);
            if (found != CursorTargetRole::Other) return found;
            return searchRoleInSlot(
                pow->exponent(), target, CursorTargetRole::PowerExponent, depth);
        }
        case NodeType::Root: {
            const auto* root = static_cast<const NodeRoot*>(node);
            CursorTargetRole found = searchRoleInSlot(
                root->radicand(), target, CursorTargetRole::RootRadicand, depth);
            if (found != CursorTargetRole::Other) return found;
            if (root->hasDegree()) {
                return searchRoleInSlot(
                    root->degree(), target, CursorTargetRole::RootDegree, depth);
            }
            return CursorTargetRole::Other;
        }
        case NodeType::Paren:
            return searchRoleInSlot(
                static_cast<const NodeParen*>(node)->content(),
                target, CursorTargetRole::ParenContent, depth);
        case NodeType::Function:
            return searchRoleInSlot(
                static_cast<const NodeFunction*>(node)->argument(),
                target, CursorTargetRole::FunctionArg, depth);
        case NodeType::LogBase: {
            const auto* lb = static_cast<const NodeLogBase*>(node);
            CursorTargetRole found = searchRoleInSlot(
                lb->base(), target, CursorTargetRole::LogBase, depth);
            if (found != CursorTargetRole::Other) return found;
            return searchRoleInSlot(
                lb->argument(), target, CursorTargetRole::LogArg, depth);
        }
        case NodeType::Subscript:
            return searchRoleInSlot(
                static_cast<const NodeSubscript*>(node)->subscript(),
                target, CursorTargetRole::Subscript, depth);
        case NodeType::BigOp: {
            const auto* bo = static_cast<const NodeBigOp*>(node);
            CursorTargetRole found = searchRoleInSlot(
                bo->upper(), target, CursorTargetRole::BigOpUpper, depth);
            if (found != CursorTargetRole::Other) return found;
            found = searchRoleInSlot(
                bo->lower(), target, CursorTargetRole::BigOpLower, depth);
            if (found != CursorTargetRole::Other) return found;
            return searchRoleInSlot(
                bo->body(), target, CursorTargetRole::BigOpBody, depth);
        }
        case NodeType::Measurement:
        case NodeType::DefIntegral:
        case NodeType::Summation: {
            for (int i = 0; i < node->childCount(); ++i) {
                const CursorTargetRole found =
                    searchRoleInNode(node->child(i), target, depth + 1);
                if (found != CursorTargetRole::Other) return found;
            }
            return CursorTargetRole::Other;
        }
        default:
            return CursorTargetRole::Other;
    }
}

} // namespace

CursorTargetRole classifyCursorTargetRow(const NodeRow* astRoot,
                                         const NodeRow* target) {
    if (!astRoot || !target) return CursorTargetRole::Other;
    if (astRoot == target) return CursorTargetRole::Root;
    return searchRoleInNode(astRoot, target, 0);
}

const char* cursorTargetRoleName(CursorTargetRole role) {
    switch (role) {
        case CursorTargetRole::Root:                 return "root";
        case CursorTargetRole::FractionNumerator:    return "fraction_numerator";
        case CursorTargetRole::FractionDenominator:  return "fraction_denominator";
        case CursorTargetRole::PowerBase:            return "power_base";
        case CursorTargetRole::PowerExponent:        return "power_exponent";
        case CursorTargetRole::RootRadicand:         return "root_radicand";
        case CursorTargetRole::RootDegree:           return "root_degree";
        case CursorTargetRole::ParenContent:         return "paren_content";
        case CursorTargetRole::FunctionArg:          return "function_arg";
        case CursorTargetRole::LogBase:              return "log_base";
        case CursorTargetRole::LogArg:               return "log_arg";
        case CursorTargetRole::Subscript:            return "subscript";
        case CursorTargetRole::BigOpUpper:           return "bigop_upper";
        case CursorTargetRole::BigOpLower:           return "bigop_lower";
        case CursorTargetRole::BigOpBody:            return "bigop_body";
        case CursorTargetRole::Other:                return "other";
    }
    return "other";
}

} // namespace vpam
