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
#include <algorithm>

#include "../ui/MathSymbols.h"
#include "font/MathGlyphAssembly.h"
#include "font/stix_math_italics.h"

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// MathNode — PSRAM allocation
// ════════════════════════════════════════════════════════════════════════════
void* MathNode::operator new(std::size_t size) {
    void* p = nullptr;
#ifdef ARDUINO
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
#ifdef ARDUINO
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
        ? static_cast<int16_t>(content.ascent - axis)
        : static_cast<int16_t>(axis - content.ascent);
    const int16_t belowAxis = static_cast<int16_t>(content.descent + axis);
    const int16_t maxHalf = static_cast<int16_t>(std::max(aboveAxis, belowAxis) + padPx);

    AxisDelimiterGeometry out;
    out.ascent = static_cast<int16_t>(axis + maxHalf);
    out.descent = static_cast<int16_t>(maxHalf - axis);
    if (out.descent < 0) {
        out.ascent = static_cast<int16_t>(out.ascent - out.descent);
        out.descent = 0;
    }
    out.height = static_cast<int16_t>(out.ascent + out.descent);
    return out;
}

static inline int16_t delimiterVerticalPadPx(
        const LayoutResult& content, const FontMetrics& fm) {
    const int16_t contentMinH = MathConstantsProvider(fm.emSize).delimitedSubFormulaMinHeight();
    return std::max<int16_t>(
        1, static_cast<int16_t>((contentMinH - content.height() + 1) / 2));
}

static inline void expandDelimiterGeometryToAssembly(
        AxisDelimiterGeometry& geom,
        const DelimiterAssembler::AssemblyResult& assembly) {
    if (!assembly.valid || assembly.totalHeightPx <= geom.height) return;

    const int16_t extra = static_cast<int16_t>(assembly.totalHeightPx - geom.height);
    geom.ascent = static_cast<int16_t>(geom.ascent + (extra + 1) / 2);
    geom.descent = static_cast<int16_t>(geom.descent + extra / 2);
    geom.height = static_cast<int16_t>(geom.ascent + geom.descent);
}

static inline int16_t assembledDelimiterWidthPx(
        AxisDelimiterGeometry& geom, const FontMetrics& fm, uint32_t baseCp) {
    const int16_t glyphFallbackPx = std::max<int16_t>(
        2, std::max<int16_t>(
               DelimiterAssembler::glyphWidthPx(baseCp, fm.emSize),
               static_cast<int16_t>(fm.charWidth / 2)));
    const MathVariantTable* table = lookupVariantTable(baseCp);
    if (!table) return glyphFallbackPx;

    auto assembly = DelimiterAssembler::assemble(geom.height, *table, fm.emSize);
    // Only grow the delimiter to the assembly's minimum renderable height when the
    // font actually has the assembly glyphs. With the stix_math subset (which omits
    // them) the renderer draws a vector delimiter that scales to any height, so the
    // delimiter should hug its content instead of ballooning to the assembly floor.
    if (g_delimiterAssemblyRenderable) {
        expandDelimiterGeometryToAssembly(geom, assembly);
    }

    int16_t width = assembly.valid ? assembly.widthPx : glyphFallbackPx;
    return width < 2 ? 2 : width;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e R o w
// ════════════════════════════════════════════════════════════════════════════
NodeRow::NodeRow() : MathNode(NodeType::Row) {}

MathClass NodeRow::mathClass() const {
    // Propagate from the first non-empty child (TeXbook convention)
    for (auto& child : _children) {
        if (child->type() != NodeType::Empty)
            return child->mathClass();
    }
    return MathClass::ORD;
}

void NodeRow::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    if (_children.empty()) {
        _layout.width   = 0;
        _layout.ascent  = fm.ascent;
        _layout.descent = fm.descent;
        _layout.inkAscent = fm.ascent;
        _layout.inkDescent = fm.descent;
        return;
    }

    int16_t totalW     = 0;
    int16_t maxAscent  = 0;
    int16_t maxDescent = 0;
    int16_t maxInkAscent = 0;
    int16_t maxInkDescent = 0;

    bool      hasPrev   = false;

    for (size_t i = 0; i < _children.size(); ++i) {
        _children[i]->calculateLayout(fm);
        const auto& cl = _children[i]->layout();

        // ── TeX Inter-Atom Spacing (Spec §3.2) ──
        // Use rightMathClass of the prev node and leftMathClass of the curr node
        // so that opening/closing delimiters use the correct type on each side.
        if (hasPrev) {
            MathClass prevRight = _children[i - 1]->rightMathClass();
            MathClass currLeft  = _children[i]->leftMathClass();
            totalW = static_cast<int16_t>(
                totalW + interAtomSpacingPx(prevRight, currLeft, fm.style, fm.emSize));
        }

        totalW     += cl.width;
        maxAscent   = std::max(maxAscent,  cl.ascent);
        maxDescent  = std::max(maxDescent, cl.descent);
        maxInkAscent = std::max<int16_t>(maxInkAscent, layoutInkAscentPx(cl));
        maxInkDescent = std::max<int16_t>(maxInkDescent, layoutInkDescentPx(cl));
        hasPrev     = true;
    }

    _layout.width   = totalW;
    _layout.ascent  = maxAscent;
    _layout.descent = maxDescent;
    _layout.inkAscent = maxInkAscent;
    _layout.inkDescent = maxInkDescent;
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
    int len = static_cast<int>(_value.size());
    if (len == 0) len = 1;   // Mínimo 1 carácter de ancho (para cursor)

    _layout.width   = fm.charWidth * static_cast<int16_t>(len);
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
    _layout.inkAscent = (fm.numberAscent > 0) ? fm.numberAscent : fm.ascent;
    _layout.inkDescent = (fm.numberAscent > 0 || fm.numberDescent > 0)
        ? fm.numberDescent
        : fm.descent;
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
    // Width is purely the character width. All inter-atom padding is handled
    // exclusively by the TeX spacing logic in NodeRow::calculateLayout.
    _layout.width   = fm.charWidth;
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
}

const char* NodeOperator::symbol() const {
    switch (_op) {
        case OpKind::Add:       return "+";
        case OpKind::Sub:       return "-";              // ASCII hyphen-minus (U+002D)
        case OpKind::Mul:       return numos::mathsym::SYMB_TIMES;
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
    _layout.width   = std::max(MIN_WIDTH,  fm.charWidth);
    _layout.ascent  = std::max(MIN_HEIGHT, fm.ascent);
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
    _denominator->calculateLayout(childFm);

    const auto& numL = _numerator->layout();
    const auto& denL = _denominator->layout();

    // 2. MATH table-driven layout constants (Spec §7.3)
    const FractionLayoutMetrics metrics = fractionLayoutMetrics(fm);
    _ruleThicknessPx = metrics.ruleThicknessPx;

    // 3. Gap-based shifts (NumOS readable-fraction policy).
    //    fractionBarGaps() uses the display-style gap-min (150 DU ≈ 3 px)
    //    as the symmetric bar gap G so that:
    //      gapAboveBar = barTop  - numBottom  == G
    //      gapBelowBar = denTop  - barBottom  == G
    //    The previous baseline-shift constants (585 DU) produced
    //    gapAboveBar=9 px and gapBelowBar=1 px because ink ascent/descent
    //    differs; the gap-based formula is symmetric by construction.
    const FractionBarGaps gaps = fractionBarGaps(fm, numL, denL);
    _numeratorShiftPx   = gaps.numeratorShiftPx;   // = gapPx + numDescent
    _denominatorShiftPx = gaps.denominatorShiftPx; // = gapPx + denAscent

    // 4. Width: widest child + rule overhang (1 mu each side)
    _ruleOverhangPx = metrics.ruleOverhangPx;
    int16_t contentW = std::max(numL.width, denL.width);
    _layout.width = contentW + 2 * _ruleOverhangPx;

    // 5. Vertical layout (baseline-relative)
    //    Fraction baseline = math axis.
    //    Ascent  = distance from axis to numerator TOP.
    //    Descent = distance from axis to denominator BOTTOM.
    _barHalfUpPx = static_cast<int16_t>((_ruleThicknessPx + 1) / 2);
    _barHalfDownPx = static_cast<int16_t>(_ruleThicknessPx / 2);

    int16_t axis = fm.axisHeight();
    int16_t aboveAxis = static_cast<int16_t>(_barHalfUpPx + _numeratorShiftPx + numL.ascent);
    int16_t belowAxis = static_cast<int16_t>(_barHalfDownPx + _denominatorShiftPx + denL.descent);

    _layout.ascent  = axis + aboveAxis;
    _layout.descent = belowAxis - axis;

    // Safety: descent must be non-negative
    if (_layout.descent < 0) {
        _layout.ascent += -_layout.descent;
        _layout.descent = 0;
    }
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

    // 3. MATH table-driven superscript shift plus a bounded NumOS VPAM
    //    readability correction. The helper is shared with draw/diagnostics
    //    so layout and pixels cannot drift.
    MathConstantsProvider mc(fm.emSize);
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);
    const int16_t expShift = sup.shiftPx;

    // 4. Italics correction (Spec §2.5): add extra space before superscript
    //    when the base is an italic letter with overhang (e.g., f, V, W, π).
    //    Uses the canonical vpam::utf8Decode() — the previously hand-rolled
    //    UTF-8 byte-length scan is replaced by the returned advance count.
    _italicCorrectionPx = 0;
    if (_base->type() == NodeType::Variable) {
        auto* var = static_cast<const NodeVariable*>(_base.get());
        const char* lbl = var->label();
        uint32_t cp = 0;
        uint8_t len = decodeFirstUtf8(lbl, cp);
        // Single-codepoint variables only — len > 0 && lbl[len] == '\0'
        if (cp != 0 && len > 0 && lbl[len] == '\0') {
            int16_t corrDu = lookupItalicsCorrection(cp);
            if (corrDu > 0) {
                _italicCorrectionPx = mc.duToPx(corrDu);
            }
        }
    } else if (_base->type() == NodeType::Constant) {
        auto* cnst = static_cast<const NodeConstant*>(_base.get());
        const char* symb = cnst->symbol();
        uint32_t cp = 0;
        uint8_t len = decodeFirstUtf8(symb, cp);
        if (cp != 0 && len > 0) {
            int16_t corrDu = lookupItalicsCorrection(cp);
            if (corrDu > 0) {
                _italicCorrectionPx = mc.duToPx(corrDu);
            }
        }
    }

    // 5. Ancho: base + italics correction + exponente
    _layout.width = static_cast<int16_t>(
        baseL.width + _italicCorrectionPx + expL.width + spaceAfterScriptPx(fm));

    // 6. Ascent: the exponent top might extend above the base top
    _layout.ascent  = std::max(baseL.ascent,
                               static_cast<int16_t>(expShift + expL.ascent));
    // Descent: keep the conservative layout box valid even for unusual
    // complex exponents. Simple digit exponents remain fully above baseline.
    _layout.descent = std::max<int16_t>(
        baseL.descent,
        static_cast<int16_t>(std::max<int16_t>(
            0, static_cast<int16_t>(expL.descent - expShift))));

    _layout.inkAscent = std::max<int16_t>(
        layoutInkAscentPx(baseL),
        static_cast<int16_t>(expShift + layoutInkAscentPx(expL)));
    _layout.inkDescent = std::max<int16_t>(
        layoutInkDescentPx(baseL),
        static_cast<int16_t>(std::max<int16_t>(
            0, static_cast<int16_t>(layoutInkDescentPx(expL) - expShift))));
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
    _radicand->calculateLayout(fm);
    const auto& radL = _radicand->layout();

    // 2. MATH table-driven layout constants (Spec §2.3, constants 49–55)
    _radicalRuleThickness = mc.radicalRuleThickness();
    _radicalVerticalGap   = mc.radicalVerticalGap(display);
    _radicalExtraAscender = mc.radicalExtraAscender();
    if (_radicalExtraAscender < 1) _radicalExtraAscender = 1;

    // 3. Radical symbol width: hook + slope (drawn manually, not from font)
    int16_t radSymW = RADICAL_HOOK_W + RADICAL_SLOPE_W;

    // 4. Degree (superscript font), if present
    if (_degree) {
        FontMetrics fmDeg = fm.superscript();
        _degree->calculateLayout(fmDeg);
        const auto& degL = _degree->layout();

        // Kern values from MATH table (kernAfter is negative for STIX Two Math)
        _radicalKernBefore = mc.radicalKernBeforeDegree();
        _radicalKernAfter  = mc.radicalKernAfterDegree();

        // Symbol width must accommodate degree
        radSymW = std::max(radSymW,
            static_cast<int16_t>(_radicalKernBefore + degL.width
                               + _radicalKernAfter + RADICAL_HOOK_W));
    }

    // 5. Total width: symbol + content + right padding
    _layout.width = static_cast<int16_t>(radSymW + radL.width + RADICAL_RIGHT_PAD);

    // 6. Ascent: radicand.ascent + verticalGap + ruleThickness + extraAscender
    _layout.ascent = static_cast<int16_t>(
        radL.ascent + _radicalVerticalGap + _radicalRuleThickness
        + _radicalExtraAscender);
    _layout.descent = radL.descent;

    // 7. Degree may extend above the overline
    if (_degree) {
        const auto& degL = _degree->layout();
        // Degree bottom raised at RadicalDegreeBottomRaisePercent % of em above baseline
        // For STIX Two Math this is 55% → degree sits near the top of the radicand
        int16_t degreeBottomRaise = static_cast<int16_t>(
            (fm.emSize * mc.radicalDegreeBottomRaisePercent()) / 100);
        int16_t degTop = static_cast<int16_t>(
            _layout.ascent - degreeBottomRaise + degL.ascent);
        _layout.ascent = std::max(_layout.ascent, degTop);
    }
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
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(cl, fm, padPx);
    _parenWidth = assembledDelimiterWidthPx(geom, fm, leftCp());

    // ── Layout: [leftParen] + pad + content + pad + [rightParen] ──
    const int16_t innerPad = std::max<int16_t>(1, _parenWidth / 3);
    _layout.width = static_cast<int16_t>(_parenWidth * 2 + innerPad * 2 + cl.width);
    _layout.ascent  = geom.ascent;
    _layout.descent = geom.descent;
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
    // 1. Etiqueta: ancho basado en charWidth × longitud del texto visible
    // Para funciones cortas (sin, cos, ln) es suficiente
    const char* lbl = label();
    int labelChars = 0;
    // Contar caracteres visibles (no bytes UTF-8)
    const char* p = lbl;
    while (*p) {
        if ((*p & 0xC0) != 0x80) labelChars++;  // No es byte de continuación
        p++;
    }
    _labelWidth = static_cast<int16_t>(fm.charWidth * labelChars);

    // 2. Argumento entre paréntesis
    _argument->calculateLayout(fm);
    const auto& argL = _argument->layout();

    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        argL, fm, delimiterVerticalPadPx(argL, fm));
    _parenWidth = assembledDelimiterWidthPx(geom, fm, 0x0028);
    _innerPad = std::max<int16_t>(1, _parenWidth / 3);
    _parenAscent = geom.ascent;
    _parenDescent = geom.descent;

    // 3. Ancho total: etiqueta + gap + ( + pad + argumento + pad + )
    _layout.width = static_cast<int16_t>(_labelWidth + LABEL_GAP
                  + _parenWidth + _innerPad + argL.width + _innerPad + _parenWidth);

    // 4. The full node must cover both label text and the argument delimiter.
    _layout.ascent  = std::max(fm.ascent, _parenAscent);
    _layout.descent = std::max(fm.descent, _parenDescent);
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
    // 1. Etiqueta "log": 3 chars
    _labelWidth = fm.charWidth * 3;

    // 2. Subíndice (base) en fuente reducida
    FontMetrics fmSub = fm.superscript();   // misma escala que superscript
    _base->calculateLayout(fmSub);
    const auto& baseL = _base->layout();

    // 3. Argumento entre paréntesis
    _argument->calculateLayout(fm);
    const auto& argL = _argument->layout();

    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        argL, fm, delimiterVerticalPadPx(argL, fm));
    _parenWidth = assembledDelimiterWidthPx(geom, fm, 0x0028);
    _innerPad = std::max<int16_t>(1, _parenWidth / 3);
    _parenAscent = geom.ascent;
    _parenDescent = geom.descent;

    // 4. Ancho total: "log" + base_subscript + SpaceAfterScript + gap + ( + pad + arg + pad + )
    //    SpaceAfterScript is added after the subscript base to match NodePower
    //    and NodeSubscript behavior (OpenType constant 17).
    _layout.width = static_cast<int16_t>(_labelWidth + baseL.width
                  + spaceAfterScriptPx(fm) + LABEL_GAP
                  + _parenWidth + _innerPad + argL.width + _innerPad + _parenWidth);

    // 5. Descenso del subíndice: use MATH table subscriptShiftDown.
    int16_t subDrop = MathConstantsProvider(fm.emSize).subscriptShiftDown();
    if (subDrop < 2) subDrop = 2;

    // El fondo del subíndice está a subDrop + baseL.descent bajo el baseline
    int16_t subBottom = subDrop + baseL.descent;

    // 6. Ascent/descent cover label, subscript, and argument delimiter.
    _layout.ascent = std::max(fm.ascent, _parenAscent);
    _layout.descent = std::max({fm.descent, subBottom, _parenDescent});
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
        case ConstKind::E:    return "e";          // Euler's number (blue via drawConstant)
        case ConstKind::Imag: return "i";          // imaginary unit (blue via drawConstant)
    }
    return "?";
}

void NodeConstant::calculateLayout(const FontMetrics& fm) {
    applyScriptLevel(this, fm);
    // La constante ocupa exactamente 1 carácter de ancho
    _layout.width   = fm.charWidth;
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
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
    applyScriptLevel(this, fm);
    // Ancho = charWidth × longitud del label
    const char* lbl = label();
    int len = 0;
    while (lbl[len]) ++len;
    _labelWidth = fm.charWidth * static_cast<int16_t>(len);
    if (_labelWidth < fm.charWidth) _labelWidth = fm.charWidth;
    _layout.width   = _labelWidth;
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
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
    // Contar caracteres totales:
    //   [signo] + intPart + "." + nonRepeat + repeat
    int chars = 0;
    if (_negative) chars += 1;  // "-"
    chars += static_cast<int>(_intPart.size());
    if (!_nonRepeat.empty() || !_repeat.empty()) {
        chars += 1;  // "."
        chars += static_cast<int>(_nonRepeat.size());
        chars += static_cast<int>(_repeat.size());
    }
    if (chars == 0) chars = 1;

    _layout.width = fm.charWidth * static_cast<int16_t>(chars);

    // Si hay dígitos periódicos, necesitamos espacio extra encima para la overline
    if (!_repeat.empty()) {
        _layout.ascent = static_cast<int16_t>(fm.ascent + OVERLINE_GAP + OVERLINE_T);
    } else {
        _layout.ascent = fm.ascent;
    }
    _layout.descent = fm.descent;
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
    _variable->calculateLayout(fm);

    const auto& bodyL  = _body->layout();
    const auto& varL   = _variable->layout();

    const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        0x222B, largeOperatorTargetHeightPx(fm), fm.emSize);
    int16_t symW = symMetrics.valid ? symMetrics.widthPx : 0;
    int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();
    if (symW < 6) symW = std::max<int16_t>(6, fm.charWidth);

    const int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    const int16_t bodyGapPx  = largeOperatorBodyGapPx(fm);
    const int16_t dGapPx = integralDifferentialGapPx(fm);
    const int16_t symbolPadPx = largeOperatorSymbolPadPx(fm);

    // Display limits governed by operator height threshold (Spec §4.5)
    if (shouldUseDisplayLimits(fm, symH)) {
        // ── DISPLAY style: limits above/below the integral sign ──
        FontMetrics fmLimit = fm.superscript();

        _lower->calculateLayout(fmLimit);
        _upper->calculateLayout(fmLimit);

        const auto& lowerL = _lower->layout();
        const auto& upperL = _upper->layout();

        // Symbol column width = max(symW, lowerL.width, upperL.width)
        int16_t symColW = std::max({symW, lowerL.width, upperL.width});

        // "d" + variable
        int16_t dVarW = static_cast<int16_t>(fm.charWidth + dGapPx + varL.width);

        _layout.width = static_cast<int16_t>(symColW + bodyGapPx + bodyL.width
                      + dGapPx + dVarW);

        const int16_t symbolAscent = largeOperatorGlyphAscentPx(fm, symH);
        const int16_t symbolDescent = largeOperatorGlyphDescentPx(fm, symH);

        _layout.ascent = std::max<int16_t>(
            bodyL.ascent,
            static_cast<int16_t>(symbolAscent + symbolPadPx
                                 + limitGapPx + upperL.height()));
        _layout.descent = std::max<int16_t>(
            bodyL.descent,
            static_cast<int16_t>(symbolDescent + symbolPadPx
                                 + limitGapPx + lowerL.height()));
    } else {
        // ── TEXT / SCRIPT style: limits as sub/superscript to the right ──
        FontMetrics fmLimit = fm.superscript();

        _lower->calculateLayout(fmLimit);
        _upper->calculateLayout(fmLimit);

        const auto& lowerL = _lower->layout();
        const auto& upperL = _upper->layout();

        // Symbol + superscript + subscript to the right
        int16_t limitsW = std::max(upperL.width, lowerL.width);
        int16_t limitsH = upperL.height() + lowerL.height();

        int16_t dVarW = static_cast<int16_t>(fm.charWidth + dGapPx + varL.width);

        _layout.width = static_cast<int16_t>(symW + limitsW + bodyGapPx
                      + bodyL.width + dGapPx + dVarW);

        int16_t bodyAscent  = std::max({bodyL.ascent, fm.ascent,
                                        static_cast<int16_t>(upperL.height())});
        int16_t bodyDescent = std::max({bodyL.descent, fm.descent,
                                        static_cast<int16_t>(lowerL.height())});

        _layout.ascent  = bodyAscent;
        _layout.descent = bodyDescent;
    }
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

    const auto& bodyL  = _body->layout();

    const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        0x2211, largeOperatorTargetHeightPx(fm), fm.emSize);
    int16_t symW = symMetrics.valid ? symMetrics.widthPx : 0;
    int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();
    if (symW < 6) symW = std::max<int16_t>(6, fm.charWidth);

    const int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    const int16_t bodyGapPx  = largeOperatorBodyGapPx(fm);
    const int16_t symbolPadPx = largeOperatorSymbolPadPx(fm);

    FontMetrics fmLimit = fm.superscript();

    // Display limits governed by operator height threshold (Spec §4.5)
    if (shouldUseDisplayLimits(fm, symH)) {
        // ── DISPLAY style: limits centered above/below the operator ──
        _lower->calculateLayout(fmLimit);
        _upper->calculateLayout(fmLimit);

        const auto& lowerL = _lower->layout();
        const auto& upperL = _upper->layout();

        // Symbol column width = max(symW, lowerL.width, upperL.width)
        int16_t symColW = std::max({symW, lowerL.width, upperL.width});

        _layout.width = symColW + bodyGapPx + bodyL.width;

        const int16_t symbolAscent = largeOperatorGlyphAscentPx(fm, symH);
        const int16_t symbolDescent = largeOperatorGlyphDescentPx(fm, symH);

        _layout.ascent = std::max<int16_t>(
            bodyL.ascent,
            static_cast<int16_t>(symbolAscent + symbolPadPx
                                 + limitGapPx + upperL.height()));
        _layout.descent = std::max<int16_t>(
            bodyL.descent,
            static_cast<int16_t>(symbolDescent + symbolPadPx
                                 + limitGapPx + lowerL.height()));
    } else {
        // ── TEXT / SCRIPT style: limits as sub/superscript to the right ──
        _lower->calculateLayout(fmLimit);
        _upper->calculateLayout(fmLimit);

        const auto& lowerL = _lower->layout();
        const auto& upperL = _upper->layout();

        int16_t limitsW = std::max(upperL.width, lowerL.width);

        _layout.width = symW + limitsW + bodyGapPx + bodyL.width;

        // Vertical: body height + max of superscript/subscript
        int16_t supTop = upperL.height();   // superscript above baseline
        int16_t subBot = lowerL.height();   // subscript below baseline

        _layout.ascent  = std::max({bodyL.ascent, fm.ascent, supTop});
        _layout.descent = std::max({bodyL.descent, fm.descent, subBot});
    }
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

    const auto opMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        _operatorCp, largeOperatorTargetHeightPx(fm), fm.emSize);
    int16_t opW = opMetrics.valid ? opMetrics.widthPx : 0;
    int16_t opH = opMetrics.valid ? opMetrics.heightPx : fm.height();
    if (opW < 6) opW = std::max<int16_t>(6, fm.charWidth);

    // WHY: Generic operators such as products/unions/intersections follow
    // TeX display-style limit placement directly. The OpenType minimum-height
    // gate remains for integral/summation glyph selection, but a display BigOp
    // must keep layout and renderer geometry in the above/below branch even
    // when a subset font has no taller variant available.
    _useDisplayLimits = fm.isDisplayStyle();

    FontMetrics fmLimit = fm.superscript();
    _body->calculateLayout(fm);
    const auto& bodyL = _body->layout();

    const int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    const int16_t bodyGapPx  = largeOperatorBodyGapPx(fm);

    if (_useDisplayLimits) {
        // ── DISPLAY: limits above/below operator ──
        _lower->calculateLayout(fmLimit);
        _upper->calculateLayout(fmLimit);

        const auto& lowerL = _lower->layout();
        const auto& upperL = _upper->layout();

        // Symbol column width
        int16_t symColW = std::max({opW, lowerL.width, upperL.width});

        _layout.width = symColW + bodyGapPx + bodyL.width;

        const int16_t symbolAscent = largeOperatorGlyphAscentPx(fm, opH);
        const int16_t symbolDescent = largeOperatorGlyphDescentPx(fm, opH);
        const int16_t symbolPadPx = largeOperatorSymbolPadPx(fm);

        _layout.ascent = std::max<int16_t>(
            bodyL.ascent,
            static_cast<int16_t>(symbolAscent + symbolPadPx
                                 + limitGapPx + upperL.height()));
        _layout.descent = std::max<int16_t>(
            bodyL.descent,
            static_cast<int16_t>(symbolDescent + symbolPadPx
                                 + limitGapPx + lowerL.height()));
    } else {
        // ── TEXT / SCRIPT: limits as sub/superscript to the right ──
        _lower->calculateLayout(fmLimit);
        _upper->calculateLayout(fmLimit);

        const auto& lowerL = _lower->layout();
        const auto& upperL = _upper->layout();

        int16_t limitsW = std::max(upperL.width, lowerL.width);

        _layout.width = opW + limitsW + bodyGapPx + bodyL.width;

        // Vertical: max of operator body, superscript top, subscript bottom
        _layout.ascent  = std::max({bodyL.ascent, fm.ascent,
                                    static_cast<int16_t>(upperL.height())});
        _layout.descent = std::max({bodyL.descent, fm.descent,
                                    static_cast<int16_t>(lowerL.height())});
    }
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
    FontMetrics fmSub = fm.superscript();
    _subscript->calculateLayout(fmSub);
    const auto& subL = _subscript->layout();

    // 3. Ancho: base + subíndice + SpaceAfterScript
    //    (OpenType constant 17 — matches NodePower and NodeLogBase behavior)
    _layout.width = static_cast<int16_t>(
        baseL.width + subL.width + spaceAfterScriptPx(fm));

    // 4. Descenso del subíndice: use MATH table subscriptShiftDown.
    int16_t subDrop = MathConstantsProvider(fm.emSize).subscriptShiftDown();
    if (subDrop < 2) subDrop = 2;  // mínimo sensato

    // Reservar todo el alto del subíndice una vez bajado, no solo su descent.
    // Esto evita clipping y colisión con el siguiente carácter.
    int16_t subBottom = subDrop + subL.ascent + subL.descent;

    // 5. Ascent: de la base (el subíndice no sube)
    _layout.ascent = baseL.ascent;

    // 6. Descent: mayor entre base y subíndice
    _layout.descent = std::max(baseL.descent, subBottom);
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

constexpr int32_t kResultWidthOverflowSentinel = 4097;
constexpr int32_t kResultHeightOverflowSentinel = 1025;

static int16_t boundedTextWidth(const std::string& text,
                                const FontMetrics& fm) {
    int glyphs = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.c_str());
    while (*p) {
        uint32_t cp = 0;
        const uint8_t used = utf8Decode(p, cp);
        if (used == 0) break;
        p += used;
        ++glyphs;
    }
    if (glyphs == 0) glyphs = 1;
    return static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(fm.charWidth) * glyphs));
}

static void setTextLayout(MathNode* node, LayoutResult& layout,
                          const std::string& text, const FontMetrics& fm) {
    applyScriptLevel(node, fm);
    layout.width = boundedTextWidth(text, fm);
    layout.ascent = fm.ascent;
    layout.descent = fm.descent;
    layout.inkAscent = fm.ascent;
    layout.inkDescent = fm.descent;
}

NodeSymbol::NodeSymbol(std::string name)
    : MathNode(NodeType::Symbol), _name(std::move(name)) {}

void NodeSymbol::calculateLayout(const FontMetrics& fm) {
    setTextLayout(this, _layout, _name, fm);
}

NodeSpecialValue::NodeSpecialValue(SpecialValueKind kind)
    : MathNode(NodeType::SpecialValue), _kind(kind) {}

const char* NodeSpecialValue::label() const {
    switch (_kind) {
        case SpecialValueKind::PositiveInfinity: return "+\xE2\x88\x9E";
        case SpecialValueKind::NegativeInfinity: return "-\xE2\x88\x9E";
        case SpecialValueKind::UnsignedInfinity: return "\xE2\x88\x9E";
        case SpecialValueKind::Undefined:        return "undefined";
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
    _separatorWidth = static_cast<int16_t>(fm.charWidth +
        MathConstantsProvider(fm.emSize).muToPx(3));
    for (size_t i = 0; i < _elements.size(); ++i) {
        _elements[i]->calculateLayout(fm);
        const auto& childLayout = _elements[i]->layout();
        content.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
            static_cast<int32_t>(content.width) + childLayout.width +
            (i + 1 < _elements.size() ? _separatorWidth : 0)));
        content.ascent = std::max(content.ascent, childLayout.ascent);
        content.descent = std::max(content.descent, childLayout.descent);
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, leftCp());
    _innerPad = std::max<int16_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(1));
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(content.width) + 2 * _delimiterWidth +
        2 * _innerPad));
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
    _relationWidth = fm.charWidth;
    _relationGap = std::max<int16_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(5));
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(_lhs->layout().width) + _rhs->layout().width +
        _relationWidth + 2 * _relationGap));
    _layout.ascent = std::max({_lhs->layout().ascent,
                               _rhs->layout().ascent, fm.ascent});
    _layout.descent = std::max({_lhs->layout().descent,
                                _rhs->layout().descent, fm.descent});
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
    _horizontalPad = std::max<int16_t>(1, mc.muToPx(2));
    _verticalPad = std::max<int16_t>(1, mc.muToPx(2));
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
    contentHeight = std::min<int32_t>(kResultHeightOverflowSentinel, contentHeight);
    const int16_t axis = fm.axisHeight();
    LayoutResult content{};
    content.width = static_cast<int16_t>(std::min<int32_t>(
        kResultWidthOverflowSentinel, contentWidth));
    content.ascent = static_cast<int16_t>(axis + (contentHeight + 1) / 2);
    content.descent = static_cast<int16_t>(contentHeight - (content.ascent - axis));
    int16_t top = static_cast<int16_t>(-content.ascent);
    for (uint8_t row = 0; row < _rows; ++row) {
        _rowBaselines[row] = static_cast<int16_t>(top + _rowAscents[row]);
        top = static_cast<int16_t>(top + _rowAscents[row] +
            _rowDescents[row] + (row + 1 < _rows ? _verticalPad : 0));
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, 0x005B);
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        contentWidth + 2 * _delimiterWidth));
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
    _separatorWidth = static_cast<int16_t>(fm.charWidth +
        MathConstantsProvider(fm.emSize).muToPx(3));
    _innerPad = std::max<int16_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(1));
    LayoutResult content{};
    content.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(_lower->layout().width) + _upper->layout().width +
        _separatorWidth));
    content.ascent = std::max(_lower->layout().ascent, _upper->layout().ascent);
    content.descent = std::max(_lower->layout().descent, _upper->layout().descent);
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, leftCp());
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(content.width) + 2 * _delimiterWidth + 2 * _innerPad));
    _layout.ascent = geom.ascent;
    _layout.descent = geom.descent;
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
    _columnGap = std::max<int16_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(4));
    const int16_t rowGap = std::max<int16_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(2));
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
        if (branch.condition) {
            branch.condition->calculateLayout(fm);
            const auto& conditionLayout = branch.condition->layout();
            _conditionColumnWidth = std::max(_conditionColumnWidth,
                static_cast<int16_t>(conditionLayout.width +
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
    totalHeight = std::min<int32_t>(kResultHeightOverflowSentinel, totalHeight);
    const int16_t axis = fm.axisHeight();
    LayoutResult content{};
    content.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(_expressionColumnWidth) + _columnGap +
        _conditionColumnWidth));
    content.ascent = static_cast<int16_t>(axis + (totalHeight + 1) / 2);
    content.descent = static_cast<int16_t>(totalHeight - (content.ascent - axis));
    int16_t top = static_cast<int16_t>(-content.ascent);
    for (size_t i = 0; i < _branches.size(); ++i) {
        _rowBaselines[i] = static_cast<int16_t>(top + rowAscents[i]);
        top = static_cast<int16_t>(top + rowAscents[i] + rowDescents[i] +
            (i + 1 < _branches.size() ? rowGap : 0));
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _braceWidth = assembledDelimiterWidthPx(geom, fm, 0x007B);
    _bracePad = MathConstantsProvider(fm.emSize).muToPx(2);
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(content.width) + _braceWidth +
        _bracePad));
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
    _labelWidth = boundedTextWidth(_name, fm);
    _separatorWidth = static_cast<int16_t>(fm.charWidth +
        MathConstantsProvider(fm.emSize).muToPx(3));
    LayoutResult content{};
    content.ascent = fm.ascent;
    content.descent = fm.descent;
    for (size_t i = 0; i < _arguments.size(); ++i) {
        _arguments[i]->calculateLayout(fm);
        const auto& argumentLayout = _arguments[i]->layout();
        content.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
            static_cast<int32_t>(content.width) + argumentLayout.width +
            (i + 1 < _arguments.size() ? _separatorWidth : 0)));
        content.ascent = std::max(content.ascent, argumentLayout.ascent);
        content.descent = std::max(content.descent, argumentLayout.descent);
    }
    AxisDelimiterGeometry geom = symmetricAxisDelimiterGeometry(
        content, fm, delimiterVerticalPadPx(content, fm));
    _delimiterWidth = assembledDelimiterWidthPx(geom, fm, 0x0028);
    _innerPad = std::max<int16_t>(1,
        MathConstantsProvider(fm.emSize).muToPx(1));
    _labelGap = MathConstantsProvider(fm.emSize).muToPx(2);
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(_labelWidth) + content.width + 2 * _delimiterWidth +
        2 * _innerPad + _labelGap));
    _layout.ascent = std::max<int16_t>(geom.ascent, fm.ascent);
    _layout.descent = std::max<int16_t>(geom.descent, fm.descent);
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
    _gap = std::max<int16_t>(1, MathConstantsProvider(fm.emSize).muToPx(4));
    _layout.width = static_cast<int16_t>(std::min<int32_t>(kResultWidthOverflowSentinel,
        static_cast<int32_t>(_labelWidth) + _gap + _expression->layout().width));
    _layout.ascent = std::max<int16_t>(fm.ascent, _expression->layout().ascent);
    _layout.descent = std::max<int16_t>(fm.descent, _expression->layout().descent);
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
            return makeFunction(fn->funcKind(), cloneNode(fn->argument()));
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
        case NodeType::Symbol:
            return makeSymbol(static_cast<const NodeSymbol*>(node)->name());
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
