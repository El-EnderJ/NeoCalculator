/*
 * NumOS — Math Typography Engine
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
 * MathTypography.h — OpenType MATH Table-Driven Layout Engine
 *
 * Phase 2+3: Provides MathConstantsProvider (pure static constexpr class
 * wrapping the 56 STIX Two Math design-unit constants), the TeX 8-class
 * atom system, inter-atom spacing table, and design-unit-to-pixel conversion.
 *
 * This is the PURE MATH layer — no LVGL, no Arduino dependencies.
 * It sits between the font data (stix_math_constants.h) and the AST
 * layout system (MathAST.h).
 *
 * Reference: OpenType 1.9.1 MATH table spec, TeXbook Appendix G.
 */

#pragma once

#include <cstdint>
#include <algorithm>

#include "font/stix_math_constants.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// MathStyle — Current math typesetting context
//
// Determines which MATH constant variant to use (display vs inline),
// large operator limit placement, and mu-to-pixel conversion.
// ════════════════════════════════════════════════════════════════════════════
enum class MathStyle : uint8_t {
    DISPLAY_STYLE = 0,  // Display math (centered equations) — renamed to avoid Arduino DISPLAY macro
    TEXT          = 1,  // Inline math (same em-size as display, tighter spacing)
    SCRIPT        = 2,  // First-level sub/superscripts (~70% of display)
    SCRIPTSCRIPT  = 3,  // Second-level sub/superscripts (~55% of display)
};

/// Returns true if style is a script level (SCRIPT or SCRIPTSCRIPT).
constexpr bool isScriptStyle(MathStyle s) {
    return s == MathStyle::SCRIPT || s == MathStyle::SCRIPTSCRIPT;
}

/// Returns true for display or text (non-script) styles.
constexpr bool isDisplayOrText(MathStyle s) {
    return s == MathStyle::DISPLAY_STYLE || s == MathStyle::TEXT;
}

// ════════════════════════════════════════════════════════════════════════════
// MathClass — TeX 8-class atom classification
//
// Every AST node is assigned one of these classes. The spacing table
// (kSpacingTable) determines the inter-atom space between adjacent classes.
//
// Maps to LyX MathClass.h and TeXbook pp.170–171.
// ════════════════════════════════════════════════════════════════════════════
enum class MathClass : uint8_t {
    ORD    = 0,  // Ordinary: variables, digits, constants, functions
    OP     = 1,  // Large operator: ∑, ∫, ∏
    BINARY = 2,  // Binary operator: +, −, ×, ±  (renamed from BIN to avoid Arduino macro)
    REL    = 3,  // Relation: =, <, >, ≤, ≥, ≠
    OPEN   = 4,  // Opening delimiter: (, [, {
    CLOSE  = 5,  // Closing delimiter: ), ], }
    PUNCT  = 6,  // Punctuation: ,, ;
    INNER  = 7,  // Inner sub-formula (fractions, radicals, etc.)
    COUNT  = 8   // Sentinel
};

// ════════════════════════════════════════════════════════════════════════════
// TeX Inter-Atom Spacing Table
//
// kSpacingTable[left][right] = spacing code:
//    0 = no space
//    1 = thin space  (3 mu)
//    2 = medium space (4 mu)  — "2*" entries in TeXbook
//    3 = thick space (5 mu)
//    9 = impossible (error)
//   -1/-2/-3 = absolute space in display/text only, zero in script/scriptscript
//
// Verified against LyX 2.3.2 MathClass.h (Debian sources).
// Starred entries (*) from the spec are simplified per LyX's embedded model.
// ════════════════════════════════════════════════════════════════════════════
constexpr int8_t kSpacingTable[8][8] = {
    //           ORD   OP   BIN  REL  OPEN CLOSE PUNCT INNER
    /* ORD    */ { 0,   1,   2,   3,   0,   0,    0,    1   },
    /* OP     */ { 1,   1,   9,   3,   0,   0,    0,    1   },
    /* BINARY */ { 2,   9,   9,   9,   2,   9,    9,    2   },
    /* REL    */ { 3,   3,   9,   0,   3,   0,    0,    3   },
    /* OPEN   */ { 0,   0,   9,   0,   0,   0,    0,    0   },
    /* CLOSE  */ { 0,   1,   2,   3,   0,   0,    0,    1   },
    /* PUNCT  */ { 1,   1,   9,   1,   1,   1,    1,    1   },
    /* INNER  */ { 1,   1,   2,   3,   1,   0,    1,    1   }
};

/// Lookup inter-atom spacing code between two MathClass values.
/// Returns 0 for invalid classes (COUNT sentinel).
constexpr int8_t getInterAtomSpace(MathClass left, MathClass right) {
    auto li = static_cast<uint8_t>(left);
    auto ri = static_cast<uint8_t>(right);
    if (li >= 8 || ri >= 8) return 0;
    return kSpacingTable[li][ri];
}

// ════════════════════════════════════════════════════════════════════════════
// Design-unit conversion utilities
//
// STIX Two Math has UPM (units-per-em) = 1000.
// Mu conversion: 18 mu = 1 em (TeXbook p.169).
// ════════════════════════════════════════════════════════════════════════════

/// Convert font design units to pixels at the given em-size.
/// duToPx(value, emSize) = (value * emSize) / kStixMathUPM
constexpr int16_t duToPx(int16_t valueDu, int16_t emSizePx) {
    return static_cast<int16_t>((static_cast<int32_t>(valueDu) * emSizePx) / kStixMathUPM);
}

/// Convert math units (mu) to pixels.
/// 18 mu = 1 em. At script level, the mu unit scales with the font.
/// @param mu  Raw math-unit count (e.g. 3 for thin space, 4 for medium, 5 for thick).
constexpr int16_t muToPx(int8_t mu, int16_t emSizePx) {
    return static_cast<int16_t>((static_cast<int32_t>(mu) * emSizePx) / 18);
}

/// Map a TeX spacing-table code to actual math units (mu).
///
/// kSpacingTable codes:
///   0 = no space (0 mu)
///   1 = thin space  (3 mu)
///   2 = medium space (4 mu)
///   3 = thick space (5 mu)
///
/// Negative codes (-1, -2, -3) carry the SAME mu values as their positive
/// counterparts (3/4/5 mu) — the sign only indicates the space is restricted
/// to display/text style. Previously abs(code) was returned directly (1/2/3 mu
/// respectively), which was a latent bug that would produce spacing 3× too
/// narrow if negative entries were ever added to kSpacingTable.
///
/// This MUST be called BEFORE muToPx() when the value comes from
/// getInterAtomSpace() / kSpacingTable.
constexpr int8_t spacingCodeToMu(int8_t code) {
    if (code == 0) return 0;
    if (code == 1 || code == -1) return 3;   // thin space
    if (code == 2 || code == -2) return 4;   // medium space
    if (code == 3 || code == -3) return 5;   // thick space
    return 0;  // code 9 (impossible) → no space
}

/// Return the final pixel spacing between adjacent TeX atom classes.
///
/// This is the single DRY path for layout, rendering, and cursor geometry.
/// Positive table codes always apply. Negative codes apply only in
/// display/text style and collapse to zero in script styles.
constexpr int16_t interAtomSpacingPx(MathClass left, MathClass right,
                                     MathStyle style, int16_t emSizePx) {
    const int8_t code = getInterAtomSpace(left, right);
    if (code > 0 && code <= 3) {
        return muToPx(spacingCodeToMu(code), emSizePx);
    }
    if (code < 0 && isDisplayOrText(style)) {
        return muToPx(spacingCodeToMu(code), emSizePx);
    }
    return 0;
}

/// Round design units up to at least minPx pixels (ceiling division).
/// Use for rule thickness and other critical elements that must never
/// collapse to zero at small font sizes.
constexpr int16_t duToPxMin(int16_t valueDu, int16_t emSizePx, int16_t minPx) {
    int16_t px = duToPx(valueDu, emSizePx);
    return px < minPx ? minPx : px;
}

/// Decode a single Unicode codepoint from a UTF-8 byte sequence.
/// Returns the number of bytes consumed (1–4). The decoded codepoint is
/// written to @p outCp. Invalid or incomplete sequences produce U+FFFD
/// (REPLACEMENT CHARACTER) and return 1 byte consumed to guarantee forward
/// progress. An empty or null input returns 0 with outCp=0.
///
/// This is the SINGLE canonical UTF-8 decoder for the entire math stack.
/// Both MathAST.cpp (italic-correction lookup) and MathRenderer.cpp
/// (glyph rendering) previously had independent, byte-identical copies.
/// Consolidation prevents drift and ensures consistent behavior.
inline uint8_t utf8Decode(const uint8_t* s, uint32_t& outCp) {
    if (!s || !s[0]) {
        outCp = 0;
        return 0;
    }
    const uint8_t b0 = s[0];

    // 1-byte sequence (U+0000–U+007F)
    if (b0 < 0x80) {
        outCp = static_cast<uint32_t>(b0);
        return 1;
    }

    // 2-byte sequence (U+0080–U+07FF) — 110xxxxx 10xxxxxx
    if ((b0 & 0xE0) == 0xC0 && s[1] && (s[1] & 0xC0) == 0x80) {
        outCp = ((static_cast<uint32_t>(b0 & 0x1F) << 6)
               |  static_cast<uint32_t>(s[1] & 0x3F));
        return 2;
    }

    // 3-byte sequence (U+0800–U+FFFF) — 1110xxxx 10xxxxxx 10xxxxxx
    if ((b0 & 0xF0) == 0xE0 && s[1] && (s[1] & 0xC0) == 0x80
                           && s[2] && (s[2] & 0xC0) == 0x80) {
        outCp = ((static_cast<uint32_t>(b0 & 0x0F) << 12)
               | (static_cast<uint32_t>(s[1] & 0x3F) << 6)
               |  static_cast<uint32_t>(s[2] & 0x3F));
        return 3;
    }

    // 4-byte sequence (U+10000–U+10FFFF) — 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((b0 & 0xF8) == 0xF0 && s[1] && (s[1] & 0xC0) == 0x80
                           && s[2] && (s[2] & 0xC0) == 0x80
                           && s[3] && (s[3] & 0xC0) == 0x80) {
        outCp = ((static_cast<uint32_t>(b0 & 0x07) << 18)
               | (static_cast<uint32_t>(s[1] & 0x3F) << 12)
               | (static_cast<uint32_t>(s[2] & 0x3F) << 6)
               |  static_cast<uint32_t>(s[3] & 0x3F));
        return 4;
    }

    // Invalid or truncated: consume one byte, emit replacement character.
    outCp = 0xFFFD;
    return 1;
}

// ════════════════════════════════════════════════════════════════════════════
// MathConstantsProvider — Pixel-safe access to the 56 MATH constants
//
// This is a lightweight value object bound to a single em-size. Every named
// MATH accessor returns PIXELS by default (round-to-nearest scaling). Raw
// design-unit access is private — callers cannot accidentally pass DU into
// pixel geometry.
//
// Percentage constants (ScriptPercentScaleDown, ScriptScriptPercentScaleDown,
// RadicalDegreeBottomRaisePercent) are returned unscaled — they are ratios,
// not lengths.
//
// All methods are constexpr, branch-free, and trivially copyable. Zero heap,
// no vtable, no exceptions.
//
// Usage:
//   MathConstantsProvider mc(fm.emSize);
//   int16_t axisPx = mc.axisHeight();                          // 258 DU → 5 px @ 18pt
//   int16_t rulePx = mc.fractionRuleThickness();               //  68 DU → 1 px min
//   int16_t shift = mc.fractionNumeratorShiftUp(true);         // 640 DU → 12 px
//   int16_t pct   = mc.scriptPercentScaleDown();               // 70 (percentage, NOT scaled)
// ════════════════════════════════════════════════════════════════════════════
class MathConstantsProvider {
public:
    static constexpr int16_t UNITS_PER_EM = kStixMathUPM;   // 1000

    constexpr explicit MathConstantsProvider(int16_t emSizePx)
        : _emSize(emSizePx > 0 ? emSizePx : 1) {}

    // ── DU→px scaling (round-to-nearest, sign-correct) ──

    /// Convert design units to pixels with round-to-nearest scaling.
    /// Handles negative DU values correctly (e.g. RadicalKernAfterDegree = -335).
    constexpr int16_t scalePx(int16_t du) const {
        const int32_t magnitude =
            (static_cast<int32_t>(du >= 0 ? du : -du) * _emSize
             + (UNITS_PER_EM / 2)) / UNITS_PER_EM;
        return static_cast<int16_t>(du >= 0 ? magnitude : -magnitude);
    }

    /// Convert design units to pixels with round-to-nearest and a minimum-pixel
    /// guard. Use for rule thickness and other elements that must not collapse
    /// to zero at small font sizes.
    constexpr int16_t scalePxMin(int16_t du, int16_t minPx) const {
        const int16_t px = scalePx(du);
        if (px == 0 && du != 0) return du > 0 ? minPx : static_cast<int16_t>(-minPx);
        return px < minPx && px >= 0 ? minPx : px;
    }

    /// Truncating DU→px conversion (for backward compatibility in rare cases
    /// where truncation is semantically required). Prefer scalePx() for all
    /// MATH-constant conversions.
    constexpr int16_t duToPx(int16_t valueDu) const {
        return static_cast<int16_t>((static_cast<int32_t>(valueDu) * _emSize) / UNITS_PER_EM);
    }

    /// Mu→px conversion. 18 mu = 1 em (TeXbook p.169).
    constexpr int16_t muToPx(int8_t mu) const {
        return static_cast<int16_t>((static_cast<int32_t>(mu) * _emSize) / 18);
    }

    /// Em-size this provider was constructed with.
    constexpr int16_t emSize() const { return _emSize; }

    // ── Named accessors — all return PIXELS unless otherwise noted ──

    /// Font scaling percentage for script level (constant 0). Returns raw % (NOT scaled).
    constexpr int16_t scriptPercentScaleDown() const {
        return rawDu(MathConstant::ScriptPercentScaleDown);
    }

    /// Font scaling percentage for scriptscript level (constant 1). Returns raw % (NOT scaled).
    constexpr int16_t scriptScriptPercentScaleDown() const {
        return rawDu(MathConstant::ScriptScriptPercentScaleDown);
    }

    /// Height of the math axis above the baseline in pixels (constant 5).
    constexpr int16_t axisHeight() const {
        return scalePx(rawDu(MathConstant::AxisHeight));
    }

    /// Minimum delimiter sub-formula height in pixels (constant 2).
    constexpr int16_t delimitedSubFormulaMinHeight() const {
        return scalePx(rawDu(MathConstant::DelimitedSubFormulaMinHeight));
    }

    /// Minimum display operator height for limit placement in pixels (constant 3).
    constexpr int16_t displayOperatorMinHeight() const {
        return scalePx(rawDu(MathConstant::DisplayOperatorMinHeight));
    }

    // ── Subscript / Superscript (all return px) ──

    constexpr int16_t subscriptShiftDown() const {
        return scalePx(rawDu(MathConstant::SubscriptShiftDown));
    }
    constexpr int16_t subscriptTopMax() const {
        return scalePx(rawDu(MathConstant::SubscriptTopMax));
    }
    constexpr int16_t subscriptBaselineDropMin() const {
        return scalePx(rawDu(MathConstant::SubscriptBaselineDropMin));
    }
    constexpr int16_t superscriptShiftUp() const {
        return scalePx(rawDu(MathConstant::SuperscriptShiftUp));
    }
    constexpr int16_t superscriptShiftUpCramped() const {
        return scalePx(rawDu(MathConstant::SuperscriptShiftUpCramped));
    }
    constexpr int16_t superscriptBottomMin() const {
        return scalePx(rawDu(MathConstant::SuperscriptBottomMin));
    }
    constexpr int16_t superscriptBaselineDropMax() const {
        return scalePx(rawDu(MathConstant::SuperscriptBaselineDropMax));
    }
    constexpr int16_t subSuperscriptGapMin() const {
        return scalePx(rawDu(MathConstant::SubSuperscriptGapMin));
    }
    constexpr int16_t superscriptBottomMaxWithSubscript() const {
        return scalePx(rawDu(MathConstant::SuperscriptBottomMaxWithSubscript));
    }
    constexpr int16_t spaceAfterScript() const {
        return scalePx(rawDu(MathConstant::SpaceAfterScript));
    }

    // ── Limits (∑, ∫ above/below) — all return px ──

    constexpr int16_t upperLimitGapMin() const {
        return scalePx(rawDu(MathConstant::UpperLimitGapMin));
    }
    constexpr int16_t upperLimitBaselineRiseMin() const {
        return scalePx(rawDu(MathConstant::UpperLimitBaselineRiseMin));
    }
    constexpr int16_t lowerLimitGapMin() const {
        return scalePx(rawDu(MathConstant::LowerLimitGapMin));
    }
    constexpr int16_t lowerLimitBaselineDropMin() const {
        return scalePx(rawDu(MathConstant::LowerLimitBaselineDropMin));
    }

    // ── Stack (for limits above/below operators) — all return px ──

    constexpr int16_t stackTopShiftUp(bool display) const {
        return scalePx(rawDu(display ? MathConstant::StackTopDisplayStyleShiftUp
                                     : MathConstant::StackTopShiftUp));
    }
    constexpr int16_t stackBottomShiftDown(bool display) const {
        return scalePx(rawDu(display ? MathConstant::StackBottomDisplayStyleShiftDown
                                     : MathConstant::StackBottomShiftDown));
    }
    constexpr int16_t stackGapMin(bool display) const {
        return scalePx(rawDu(display ? MathConstant::StackDisplayStyleGapMin
                                     : MathConstant::StackGapMin));
    }

    // ── Fraction — all return px ──

    /// Shift of the numerator baseline above the fraction bar.
    constexpr int16_t fractionNumeratorShiftUp(bool display) const {
        return scalePx(rawDu(display ? MathConstant::FractionNumeratorDisplayStyleShiftUp
                                     : MathConstant::FractionNumeratorShiftUp));
    }
    /// Shift of the denominator baseline below the fraction bar.
    constexpr int16_t fractionDenominatorShiftDown(bool display) const {
        return scalePx(rawDu(display ? MathConstant::FractionDenominatorDisplayStyleShiftDown
                                     : MathConstant::FractionDenominatorShiftDown));
    }
    /// Minimum gap between numerator baseline and fraction bar.
    constexpr int16_t fractionNumeratorGapMin(bool display) const {
        return scalePx(rawDu(display ? MathConstant::FractionNumDisplayStyleGapMin
                                     : MathConstant::FractionNumeratorGapMin));
    }
    /// Minimum gap between denominator baseline and fraction bar.
    constexpr int16_t fractionDenominatorGapMin(bool display) const {
        return scalePx(rawDu(display ? MathConstant::FractionDenomDisplayStyleGapMin
                                     : MathConstant::FractionDenominatorGapMin));
    }
    /// Thickness of the fraction bar in pixels (1 px min).
    constexpr int16_t fractionRuleThickness() const {
        return scalePxMin(rawDu(MathConstant::FractionRuleThickness), 1);
    }

    // ── Radical — all return px ──

    constexpr int16_t radicalVerticalGap(bool display) const {
        return scalePx(rawDu(display ? MathConstant::RadicalDisplayStyleVerticalGap
                                     : MathConstant::RadicalVerticalGap));
    }
    constexpr int16_t radicalRuleThickness() const {
        return scalePxMin(rawDu(MathConstant::RadicalRuleThickness), 1);
    }
    constexpr int16_t radicalExtraAscender() const {
        return scalePx(rawDu(MathConstant::RadicalExtraAscender));
    }
    constexpr int16_t radicalKernBeforeDegree() const {
        return scalePx(rawDu(MathConstant::RadicalKernBeforeDegree));
    }
    constexpr int16_t radicalKernAfterDegree() const {
        return scalePx(rawDu(MathConstant::RadicalKernAfterDegree));
    }
    /// Radical degree bottom raise percentage. Returns raw % (NOT scaled).
    constexpr int16_t radicalDegreeBottomRaisePercent() const {
        return rawDu(MathConstant::RadicalDegreeBottomRaisePercent);
    }

    // ── Overbar / Underbar — all return px ──

    constexpr int16_t overbarVerticalGap() const {
        return scalePx(rawDu(MathConstant::OverbarVerticalGap));
    }
    constexpr int16_t overbarRuleThickness() const {
        return scalePxMin(rawDu(MathConstant::OverbarRuleThickness), 1);
    }
    constexpr int16_t overbarExtraAscender() const {
        return scalePx(rawDu(MathConstant::OverbarExtraAscender));
    }
    constexpr int16_t underbarVerticalGap() const {
        return scalePx(rawDu(MathConstant::UnderbarVerticalGap));
    }
    constexpr int16_t underbarRuleThickness() const {
        return scalePxMin(rawDu(MathConstant::UnderbarRuleThickness), 1);
    }
    constexpr int16_t underbarExtraDescender() const {
        return scalePx(rawDu(MathConstant::UnderbarExtraDescender));
    }

    // ── Accent / Miscellaneous — all return px ──

    constexpr int16_t mathLeading() const {
        return scalePx(rawDu(MathConstant::MathLeading));
    }
    constexpr int16_t accentBaseHeight() const {
        return scalePx(rawDu(MathConstant::AccentBaseHeight));
    }
    constexpr int16_t flattenedAccentBaseHeight() const {
        return scalePx(rawDu(MathConstant::FlattenedAccentBaseHeight));
    }
    constexpr int16_t skewedFractionHorizontalGap() const {
        return scalePx(rawDu(MathConstant::SkewedFractionHorizontalGap));
    }
    constexpr int16_t skewedFractionVerticalGap() const {
        return scalePx(rawDu(MathConstant::SkewedFractionVerticalGap));
    }

    // ── Stretch stack — all return px ──

    constexpr int16_t stretchStackTopShiftUp() const {
        return scalePx(rawDu(MathConstant::StretchStackTopShiftUp));
    }
    constexpr int16_t stretchStackBottomShiftDown() const {
        return scalePx(rawDu(MathConstant::StretchStackBottomShiftDown));
    }
    constexpr int16_t stretchStackGapAboveMin() const {
        return scalePx(rawDu(MathConstant::StretchStackGapAboveMin));
    }
    constexpr int16_t stretchStackGapBelowMin() const {
        return scalePx(rawDu(MathConstant::StretchStackGapBelowMin));
    }

private:
    int16_t _emSize;

    /// Raw design-unit access — private to prevent accidental DU leaks.
    static constexpr int16_t rawDu(MathConstant c) {
        return kStixMathConstants[static_cast<uint8_t>(c)];
    }
};

}  // namespace vpam
