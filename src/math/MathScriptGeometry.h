// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MathAST.h"
#include "MathIdentifier.h"
#include "InputSymbols.h"
#include "font/stix_math_italics.h"
#include "font/stix_math_kern.h"

namespace vpam {

struct ScriptLayout {
    // WHY: a combined attachment can move a nested subscript. Update only
    // the derived boxes of its transparent Row/reference wrappers; measuring
    // the subtree again would reset that placement. No semantic state changes.
    static void refreshWrappers(MathNode* inner, MathNode* outer) {
        while (inner != outer) {
            auto* parent = inner->parent();
            if (!parent) return;
            const auto& child = inner->_layout;
            auto& box = parent->_layout;
            box.width = child.width; box.ascent = child.ascent; box.descent = child.descent;
            box.inkAscent = child.inkAscent; box.inkDescent = child.inkDescent;
            inner = parent;
        }
    }
};

inline void separateScriptPair(const FontMetrics& fm, const LayoutResult& sup,
                               const LayoutResult& sub, int16_t& up, int16_t& down) {
    const MathConstantsProvider mc(fm.emSize);
    const int gap = up + down - layoutInkDescentPx(sup) - layoutInkAscentPx(sub);
    const int missing = std::max(0, mc.subSuperscriptGapMin() - gap);
    const int raise = std::min(missing, std::max(0,
        mc.superscriptBottomMaxWithSubscript() - (up - layoutInkDescentPx(sup))));
    up = fm.coordinate(up + raise);
    down = fm.coordinate(down + missing - raise);
}

// Rows around one nucleus do not turn a glyph into a boxed expression.
inline const MathNode* scriptNucleus(const MathNode* node) {
    while (node && (node->type() == NodeType::Row ||
                   node->type() == NodeType::QuantityReference) && node->childCount() == 1)
        node = node->child(0);
    return node;
}
inline uint32_t scriptGlyph(const MathNode* node) {
    node = scriptNucleus(node);
    if (!node) return 0;
    const char* text = nullptr;
    bool identifier = false;
    switch (node->type()) {
        case NodeType::Variable: text = static_cast<const NodeVariable*>(node)->label(); identifier = true; break;
        case NodeType::Symbol: {
            const auto* symbol = static_cast<const NodeSymbol*>(node);
            text = symbol->name().c_str(); identifier = !symbol->upright(); break;
        }
        case NodeType::Constant: text = static_cast<const NodeConstant*>(node)->symbol(); break;
        case NodeType::Number: text = static_cast<const NodeNumber*>(node)->value().c_str(); break;
        default: return 0;
    }
    uint32_t cp = 0;
    const auto count = utf8Decode(reinterpret_cast<const uint8_t*>(text), cp);
    if (!count || text[count]) return 0;
    return identifier ? identifierGlyphCodepoint(cp) : numos::inputsymbol::glyphCodepoint(cp);
}

inline int16_t nucleusItalicReserve(const MathNode* node, const FontMetrics& fm) {
    const uint32_t cp = scriptGlyph(node);
    return isMathItalicGlyph(cp) ? std::max<int16_t>(0,
        MathConstantsProvider(fm.emSize).scalePx(lookupItalicsCorrection(cp))) : 0;
}

inline bool isTextScriptNucleus(const MathNode* node) {
    node = scriptNucleus(node);
    if (!node) return false;
    // WHY: an authored numeral or upright unit is an unboxed text run just
    // like TeX's final digit/letter. Its baseline must not be raised as if it
    // were a fraction or radical merely because a run has several glyphs.
    return node->type() == NodeType::Number || node->type() == NodeType::Unit ||
           node->type() == NodeType::Variable || node->type() == NodeType::Symbol ||
           node->type() == NodeType::Constant;
}

inline int16_t subscriptShiftPx(const FontMetrics& fm, const LayoutResult& base,
                               const LayoutResult& sub, bool glyphBase) {
    const MathConstantsProvider mc(fm.emSize);
    return std::max({mc.subscriptShiftDown(),
        fm.coordinate(layoutInkAscentPx(sub) - mc.subscriptTopMax()),
        fm.coordinate(glyphBase ? 0 : base.descent + mc.subscriptBaselineDropMin())});
}

// WHY: MATH cut-ins sample two touching heights. Each glyph's table uses its
// OWN em. Sum the weighted DU values before one sign-correct pixel rounding.
inline int16_t scriptMathKernPx(uint32_t baseCp, uint32_t scriptCp,
        const FontMetrics& fm, const FontMetrics& scriptFm,
        const LayoutResult& base, const LayoutResult& script, int16_t shift,
        bool superscript) {
    if (!baseCp || !scriptCp) return 0;
    const auto baseCorner = superscript ? MathKernCorner::TopRight : MathKernCorner::BottomRight;
    const auto scriptCorner = superscript ? MathKernCorner::BottomLeft : MathKernCorner::TopLeft;
    const int scriptBaseline = superscript ? shift : -shift;
    const int edgeBase = superscript ? layoutInkAscentPx(base) : -layoutInkDescentPx(base);
    const int edgeScript = superscript ? -layoutInkDescentPx(script) : layoutInkAscentPx(script);
    auto sum = [&](int height) {
        const int b = lookupMathKernDu(baseCp, baseCorner, height * kStixMathUPM / fm.emSize);
        const int s = lookupMathKernDu(scriptCp, scriptCorner,
            (height - scriptBaseline) * kStixMathUPM / scriptFm.emSize);
        return b * fm.emSize + s * scriptFm.emSize;
    };
    const int weighted = std::min(sum(edgeBase), sum(scriptBaseline + edgeScript));
    return fm.coordinate(weighted < 0 ? -((-weighted + 500) / 1000) : (weighted + 500) / 1000);
}

} // namespace vpam
