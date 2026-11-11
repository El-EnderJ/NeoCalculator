// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MathScriptGeometry.h"
#include "font/MathGlyphAssembly.h"
#include "font/stix_operator_ink.h"

namespace vpam {

// Relative origins, shared by measurement, painting and cursor placement.
// No AST cache or allocation is needed: only bounded MATH/bitmap lookups.
struct LargeOperatorGeometry {
    LayoutResult box{};
    int16_t symbolX = 0, symbolBaseline = 0;
    int16_t upperX = 0, upperBaseline = 0;
    int16_t lowerX = 0, lowerBaseline = 0;
    int16_t bodyX = 0;
    uint32_t glyph = 0;
    bool displayLimits = false; // TXP §H.9.4 gate result: limits above/below
};

/// Height the §H.9.4 gate compares with DisplayOperatorMinHeight: the glyph the
/// variant ladder selects for largeOperatorTargetHeightPx() (the same input the
/// gate had before PARITY-02 dropped it). It is independent of the source-ink
/// glyph that is drawn, so a taller drawn variant never changes limit placement.
inline int16_t largeOperatorGateHeightPx(uint32_t cp, const FontMetrics& fm) {
    const auto m = DelimiterAssembler::glyphMetricsForHeightPx(
        cp, largeOperatorTargetHeightPx(fm), fm.emSize);
    return m.valid ? m.heightPx : fm.height();
}

inline LargeOperatorGeometry largeOperatorGeometryWithGate(uint32_t cp,
        const FontMetrics& fm, const LayoutResult& lower,
        const LayoutResult& upper, const LayoutResult& body,
        int16_t gateHeightPx) {
    LargeOperatorGeometry g;
    // Glyph selection follows the style; limit placement follows the gate.
    const bool display = fm.isDisplayStyle();
    const bool limits = shouldUseDisplayLimits(fm, gateHeightPx);
    g.displayLimits = limits;
    const auto* ink = stixOperatorInk(cp, fm.emSize, display);
    if (!ink) return g; // BigOpKind and the two dedicated operators are exhaustive.
    const MathConstantsProvider mc(fm.emSize);
    g.glyph = stixOperatorCodepoint(cp, display);
    const int16_t width = std::max(ink->advance,
        fm.coordinate(ink->xOffset + ink->width));
    const int16_t ascent = largeOperatorGlyphAscentPx(fm, ink->height);
    const int16_t descent = fm.coordinate(ink->height - ascent);
    // WHY: variants such as STIX union have a different native baseline.
    // Center the actual raster on AxisHeight; do not center an estimated box.
    g.symbolBaseline = fm.coordinate(ink->height + ink->yOffset - ascent);
    const int16_t italic = mc.scalePx(ink->italicCorrectionDu);
    if (limits) {
        // §H.9.4: DISPLAY style and glyph >= DisplayOperatorMinHeight. The drawn
        // glyph is still the source font's true display variant; the gate only
        // picks stacked limits (rule 13a) over side scripts below.
        g.upperBaseline = fm.coordinate(-ascent - std::max(mc.upperLimitBaselineRiseMin(),
            fm.coordinate(mc.upperLimitGapMin() + layoutInkDescentPx(upper))));
        g.lowerBaseline = fm.coordinate(descent + std::max(mc.lowerLimitBaselineDropMin(),
            fm.coordinate(mc.lowerLimitGapMin() + layoutInkAscentPx(lower))));
        g.upperX = fm.coordinate((width - upper.width + italic) / 2);
        g.lowerX = fm.coordinate((width - lower.width - italic) / 2);
    } else {
        int16_t up = std::max({
            isCrampedStyle(fm.style) ? mc.superscriptShiftUpCramped() : mc.superscriptShiftUp(),
            fm.coordinate(layoutInkDescentPx(upper) + mc.superscriptBottomMin()),
            fm.coordinate(ascent - mc.superscriptBaselineDropMax())});
        int16_t down = std::max({mc.subscriptShiftDown(),
            fm.coordinate(layoutInkAscentPx(lower) - mc.subscriptTopMax()),
            fm.coordinate(descent + mc.subscriptBaselineDropMin())});
        separateScriptPair(fm, upper, lower, up, down);
        g.upperBaseline = fm.coordinate(-up); g.lowerBaseline = fm.coordinate(down);
        g.upperX = fm.coordinate(width + italic); g.lowerX = fm.coordinate(width);
    }
    const int16_t left = std::min({int16_t(0), g.upperX, g.lowerX});
    g.symbolX = -left; g.upperX -= left; g.lowerX -= left;
    g.bodyX = fm.coordinate(std::max({fm.coordinate(g.symbolX + width),
        fm.coordinate(g.upperX + upper.width),
        fm.coordinate(g.lowerX + lower.width)}) + largeOperatorBodyGapPx(fm)
        + (limits ? 0 : spaceAfterScriptPx(fm)));
    g.box.width = fm.coordinate(g.bodyX + body.width);
    g.box.ascent = std::max({ascent, body.ascent,
        fm.coordinate(upper.ascent - g.upperBaseline),
        fm.coordinate(lower.ascent - g.lowerBaseline)});
    g.box.descent = std::max({int16_t(0), descent, body.descent,
        fm.coordinate(upper.descent + g.upperBaseline),
        fm.coordinate(lower.descent + g.lowerBaseline)});
    g.box.inkAscent = std::max({ascent, layoutInkAscentPx(body),
        fm.coordinate(layoutInkAscentPx(upper) - g.upperBaseline),
        fm.coordinate(layoutInkAscentPx(lower) - g.lowerBaseline)});
    g.box.inkDescent = std::max({int16_t(0), descent, layoutInkDescentPx(body),
        fm.coordinate(layoutInkDescentPx(upper) + g.upperBaseline),
        fm.coordinate(layoutInkDescentPx(lower) + g.lowerBaseline)});
    return g;
}

inline LargeOperatorGeometry largeOperatorGeometry(uint32_t cp,
        const FontMetrics& fm, const LayoutResult& lower,
        const LayoutResult& upper, const LayoutResult& body) {
    return largeOperatorGeometryWithGate(cp, fm, lower, upper, body,
                                         largeOperatorGateHeightPx(cp, fm));
}

inline void largeOperatorUtf8(uint32_t cp, char (&text)[4]) {
    text[0] = static_cast<char>(0xE0 | (cp >> 12));
    text[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    text[2] = static_cast<char>(0x80 | (cp & 0x3F));
    text[3] = 0;
}

struct IntegralDifferentialGeometry {
    LayoutResult box;
    int16_t dX, variableX;
};
inline IntegralDifferentialGeometry integralDifferentialGeometry(
        const FontMetrics& fm, const LayoutResult& op, const LayoutResult& variable) {
    const int16_t gap = integralDifferentialGapPx(fm);
    IntegralDifferentialGeometry g{op, fm.coordinate(op.width + gap), 0};
    g.variableX = fm.coordinate(g.dX + fm.textAdvance("d") + gap);
    g.box.width = fm.coordinate(g.variableX + variable.width);
    const auto ink = fm.measureAtom && fm.textFont ? fm.measureAtom(fm.textFont, "d")
        : TextAtomMetrics{fm.charWidth, fm.ascent, fm.descent, 0};
    g.box.ascent = std::max({op.ascent, variable.ascent, ink.ascent});
    g.box.descent = std::max({op.descent, variable.descent, ink.descent});
    g.box.inkAscent = std::max({layoutInkAscentPx(op), layoutInkAscentPx(variable), ink.ascent});
    g.box.inkDescent = std::max({layoutInkDescentPx(op), layoutInkDescentPx(variable), ink.descent});
    return g;
}
} // namespace vpam
