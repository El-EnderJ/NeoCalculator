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
 * MathAST.h — Abstract Syntax Tree para Motor Matemático V.P.A.M.
 *
 * Fase 1+5: Estructuras de datos del AST y sistema de Layout Recursivo.
 *
 * REGLA DE ORO: La expresión NUNCA se representa como un String lineal.
 * Todo (input, UI, cálculo) se basa al 100% en este árbol dinámico.
 *
 * Jerarquía de nodos:
 *   MathNode (base abstracta)
 *   ├── NodeRow             Secuencia horizontal de hijos [h1, h2, ...]
 *   ├── NodeNumber          Literal numérico: "42", "3.14"
 *   ├── NodeOperator        Operador binario: +, −, ×
 *   ├── NodeEmpty           Placeholder □ (donde el usuario debe escribir)
 *   ├── NodeFraction        Fracción apilada: numerador / denominador
 *   ├── NodePower           Superíndice: base^exponente
 *   ├── NodeRoot            Radical: √(contenido) o ⁿ√(contenido)
 *   ├── NodeParen           Grupo entre paréntesis: (contenido)
 *   ├── NodeFunction        Función: sin(x), cos(x), ln(x), log(x), etc.
 *   ├── NodeLogBase         Logaritmo base custom: log_n(x) con subíndice
 *   ├── NodeConstant        Constante algebraica: π, e
 *   └── NodePeriodicDecimal Decimal periódico: 0.̄3̄ con overline
 *
 * Layout:
 *   Cada nodo expone calculateLayout(FontMetrics) que calcula recursivamente
 *   { width, ascent, descent }. El baseline queda a 'ascent' píxeles del tope.
 *
 *   ascent   ↑  distancia del baseline hacia ARRIBA (positivo)
 *   descent  ↓  distancia del baseline hacia ABAJO  (positivo)
 *   height() =  ascent + descent
 *
 * Dependencias: C++ estándar únicamente (sin LVGL, sin Arduino).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <array>
#include <vector>
#include <string>

#include "MathTypography.h"
#include "font/stix_math_variants.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Forward declarations
// ════════════════════════════════════════════════════════════════════════════
class MathNode;
using NodePtr = std::unique_ptr<MathNode>;

// ════════════════════════════════════════════════════════════════════════════
// NodeType — Identifica el tipo concreto de cada nodo
// ════════════════════════════════════════════════════════════════════════════
enum class NodeType : uint8_t {
    Row,
    Number,
    Operator,
    Empty,
    Fraction,
    Power,
    Root,
    Paren,
    Function,         // sin, cos, tan, arcsin, arccos, arctan, ln, log
    LogBase,          // log_n(x) con subíndice VPAM
    Constant,         // π, e
    Variable,         // Variable: x, y, z, A-F, Ans, PreAns
    PeriodicDecimal,  // Decimal periódico con overline (solo resultado)
    DefIntegral,      // Definite integral: ∫[lower,upper] expr d(var)
    Summation,        // Summation series: ∑[lower,upper] expr (var=n)
    Subscript,        // Generic subscript: base_subscript (e.g. x₁, x₂)
    BigOp,            // Generic large operator: ∏, ⋂, ⋃ (style-dependent limits)
    Symbol,           // Engine-owned identifier of arbitrary bounded length
    SpecialValue,     // ±infinity / unsigned infinity / undefined
    Collection,       // Semantic finite list or set
    Equation,         // Semantic equality or assignment-style pair
    Matrix,           // Bounded rectangular matrix
    Interval,         // Semantic interval with endpoint openness
    Piecewise,        // Bounded expression/condition branches
    Call,             // Named function call not covered by FuncKind
    Unevaluated,      // Valid expression explicitly marked unevaluated
};

enum class CollectionKind : uint8_t { List, Set };
enum class EquationKind : uint8_t { Equation, Assignment };
enum class SpecialValueKind : uint8_t {
    PositiveInfinity,
    NegativeInfinity,
    UnsignedInfinity,
    Undefined
};

// ════════════════════════════════════════════════════════════════════════════
// OpKind — Tipo de operador binario
// ════════════════════════════════════════════════════════════════════════════
enum class OpKind : uint8_t {
    Add,       // +
    Sub,       // −  (resta, no signo negativo)
    Mul,       // ×
    PlusMinus, // ±  (plus-minus, for quadratic formula)
    // ── Relations (MathClass::REL) ──
    Eq,        // =
    Lt,        // <
    Gt,        // >
    Le,        // ≤
    Ge,        // ≥
    Ne,        // ≠
};

/// Returns true if the OpKind is a relation operator (=, <, >, ≤, ≥, ≠).
constexpr bool isRelation(OpKind op) {
    return op >= OpKind::Eq;
}

// ════════════════════════════════════════════════════════════════════════════
// FuncKind — Tipo de función matemática
// ════════════════════════════════════════════════════════════════════════════
enum class FuncKind : uint8_t {
    Sin,       // sin
    Cos,       // cos
    Tan,       // tan
    ArcSin,    // sin⁻¹  (arcsin)
    ArcCos,    // cos⁻¹  (arccos)
    ArcTan,    // tan⁻¹  (arctan)
    Ln,        // ln     (logaritmo natural)
    Log,       // log    (logaritmo base 10)
};

// ════════════════════════════════════════════════════════════════════════════
// BigOpKind — Generic large n-ary operator type
// ════════════════════════════════════════════════════════════════════════════
enum class BigOpKind : uint8_t {
    Product,       // ∏  (U+220F)
    CoProduct,     // ∐  (U+2210)
    Intersection,  // ⋂  (U+22C2)
    Union,         // ⋃  (U+22C3)
    LogicalAnd,    // ⋀  (U+22C0)
    LogicalOr,     // ⋁  (U+22C1)
};

/// Single-source mapping from BigOpKind to the operator glyph (codepoint + UTF-8).
/// Used by NodeBigOp constructors so the codepoint→UTF-8 relationship cannot drift.
struct BigOpGlyph {
    uint32_t    cp;
    const char* utf8;
};
inline constexpr BigOpGlyph bigOpGlyph(BigOpKind kind) {
    switch (kind) {
        case BigOpKind::Product:      return { 0x220F, "\xE2\x88\x8F" };  // ∏
        case BigOpKind::CoProduct:    return { 0x2210, "\xE2\x88\x90" };  // ∐
        case BigOpKind::Intersection: return { 0x22C2, "\xE2\x8B\x82" };  // ⋂
        case BigOpKind::Union:        return { 0x22C3, "\xE2\x8B\x83" };  // ⋃
        case BigOpKind::LogicalAnd:   return { 0x22C0, "\xE2\x8B\x80" };  // ⋀
        case BigOpKind::LogicalOr:    return { 0x22C1, "\xE2\x8B\x81" };  // ⋁
    }
    return { 0x220F, "\xE2\x88\x8F" };
}

// ════════════════════════════════════════════════════════════════════════════
// DelimKind — Delimiter pair type for NodeParen
//
// Each kind maps to a specific left/right Unicode codepoint pair.
// Variant tables for all of these are extracted in stix_math_variants.h.
// ════════════════════════════════════════════════════════════════════════════
enum class DelimKind : uint8_t {
    Paren,    // (  U+0028  /  )  U+0029
    Bracket,  // [  U+005B  /  ]  U+005D
    Brace,    // {  U+007B  /  }  U+007D
    Bar,      // |  U+007C  /  |  U+007C
};

/// Left (opening) codepoint for a delimiter kind.
inline constexpr uint32_t delimLeftCp(DelimKind k) {
    switch (k) {
        case DelimKind::Paren:   return 0x0028;
        case DelimKind::Bracket: return 0x005B;
        case DelimKind::Brace:   return 0x007B;
        case DelimKind::Bar:     return 0x007C;
    }
    return 0x0028;
}

/// Right (closing) codepoint for a delimiter kind.
inline constexpr uint32_t delimRightCp(DelimKind k) {
    switch (k) {
        case DelimKind::Paren:   return 0x0029;
        case DelimKind::Bracket: return 0x005D;
        case DelimKind::Brace:   return 0x007D;
        case DelimKind::Bar:     return 0x007C;
    }
    return 0x0029;
}

// ════════════════════════════════════════════════════════════════════════════
// ConstKind — Tipo de constante algebraica
// ════════════════════════════════════════════════════════════════════════════
enum class ConstKind : uint8_t {
    Pi,    // π
    E,     // e (Euler)
    Imag,  // i (imaginary unit)
};

// ════════════════════════════════════════════════════════════════════════════
// FontMetrics — Métricas tipográficas para el cálculo de layout
//
// No depende de tipos LVGL, pero transporta tanto la caja visual de tinta
// como la line-box necesaria para convertir baseline -> top al dibujar.
// ════════════════════════════════════════════════════════════════════════════
struct FontMetrics {
    int16_t charWidth;   ///< Ancho promedio de un dígito (monoespaciado)
    int16_t ascent;      ///< Píxeles del baseline al tope visual de la tinta
    int16_t descent;     ///< Píxeles del baseline a la tinta inferior (≥0)
    int16_t lineAscent;  ///< Píxeles del baseline al top de la line-box LVGL
    int16_t lineDescent; ///< Píxeles del baseline al bottom de la line-box LVGL
    int16_t capHeight;   ///< Píxeles del baseline al tope de las mayúsculas (Cap Height)
    uint8_t scriptLevel = 0;             ///< 0 = base, 1 = script, 2 = scriptscript
    const FontMetrics* script = nullptr; ///< Métricas del nivel de script (Level 1)
    int16_t emSize = 18;                 ///< Font size in pixels (for duToPx / muToPx conversion)
    MathStyle style = MathStyle::DISPLAY_STYLE; ///< Current math typesetting context
    int16_t numberAscent = 0;            ///< Actual digit ink above baseline from LVGL glyph boxes.
    int16_t numberDescent = 0;           ///< Actual digit ink below baseline from LVGL glyph boxes.

    /// Altura total de la caja visual usada por layout.
    int16_t height() const { return ascent + descent; }

    /// Altura total de la caja de línea LVGL, solo para render de texto.
    int16_t lineHeight() const { return lineAscent + lineDescent; }

    /// Eje matemático: donde van las barras de fracción y el centro de +/−.
    /// Uses the STIX Two Math AxisHeight constant via MathConstantsProvider,
    /// which returns round-to-nearest scaled pixels (258 DU → 5 px at 18pt).
    int16_t axisHeight() const {
        return std::max<int16_t>(1,
            MathConstantsProvider(emSize).axisHeight());
    }

    /// True if this is a display or text (non-script) style.
    bool isDisplayStyle() const {
        return style == MathStyle::DISPLAY_STYLE;
    }

    /// Métricas reducidas para superíndices/subíndices.
    ///
    /// Level 1 (script): scales by scriptPercentScaleDown (70% for STIX Two Math).
    /// Level 2 (scriptscript): scales by scriptScriptPercentScaleDown (55%).
    ///
    /// When a pre-computed script FontMetrics is already attached (e.g. from
    /// metricsFromFont for the 12pt font), it is returned directly for level 1;
    /// level 2 falls through to the MathConstantsProvider-based scaling.
    FontMetrics superscript() const {
        if (scriptLevel < 2 && script) {
            // Level 0→1: use the pre-computed script font metrics
            FontMetrics out = *script;
            out.scriptLevel = static_cast<uint8_t>(scriptLevel + 1);
            out.style = (out.scriptLevel >= 2) ? MathStyle::SCRIPTSCRIPT
                                               : MathStyle::SCRIPT;
            return out;
        }

        // Level 1→2 or fallback: scale by the appropriate MATH constant
        MathConstantsProvider mc(emSize);
        int16_t scalePercent = (scriptLevel >= 1)
            ? mc.scriptScriptPercentScaleDown()   // 55% for scriptscript
            : mc.scriptPercentScaleDown();         // 70% for script

        auto scale = [scalePercent](int16_t v, int16_t mn) -> int16_t {
            int16_t r = static_cast<int16_t>((static_cast<int32_t>(v) * scalePercent) / 100);
            return r < mn ? mn : r;
        };
        auto scaleAllowZero = [scalePercent](int16_t v) -> int16_t {
            int16_t r = static_cast<int16_t>((static_cast<int32_t>(v) * scalePercent) / 100);
            return r < 0 ? 0 : r;
        };

        FontMetrics out;
        out.charWidth  = scale(charWidth, 6);
        out.ascent     = scale(ascent, 8);
        out.descent    = scale(descent, 1);
        out.lineAscent = scale(lineAscent > 0 ? lineAscent : ascent, 8);
        out.lineDescent = scale(lineDescent > 0 ? lineDescent : descent, 1);
        out.capHeight  = scale(capHeight, 8);
        out.scriptLevel = (scriptLevel >= 2) ? 2 : static_cast<uint8_t>(scriptLevel + 1);
        out.emSize     = scale(emSize, 6);
        out.style      = (out.scriptLevel >= 2) ? MathStyle::SCRIPTSCRIPT : MathStyle::SCRIPT;
        out.script     = nullptr;
        out.numberAscent = scale(numberAscent > 0 ? numberAscent : ascent, 1);
        out.numberDescent = (numberAscent > 0 || numberDescent > 0)
            ? scaleAllowZero(numberDescent)
            : scaleAllowZero(descent);
        return out;
    }
};

/// NumOS ReadableInlineFractionPolicy — fraction numerator/denominator metrics.
///
/// Returns the FontMetrics the children of a fraction must use.
///
/// Policy (device-specific):
///   Top-level inline fractions on the 320x240 TFT must keep readable glyph
///   sizes for numerator/denominator.  Compactness comes from the tighter
///   TEXT-style fraction shift/gap MATH constants, NOT from tiny script glyphs.
///   Reduction only kicks in once the fraction is already inside a script
///   context (exponent, subscript, limit), i.e. scriptLevel >= 1.
///
/// Style lattice:
///   DISPLAY L0  ->  TEXT  L0  (same glyph size, text-style fraction shifts)
///   TEXT    L0  ->  TEXT  L0  (same glyph size, text-style fraction shifts)
///   SCRIPT  L1  ->  SCRIPTSCRIPT L2  (~55%, via superscript() clamp)
///   SCRIPTSCRIPT L2  ->  SCRIPTSCRIPT L2  (clamped)
///
/// This deliberately departs from strict TeX, which would step TEXT->SCRIPT.
/// That step produces 8 px glyphs that are illegible on this hardware.
///
/// Allocation-free, no UI dependency, host-testable.
inline FontMetrics fractionPartMetrics(const FontMetrics& fm) {
    if (fm.scriptLevel >= 1) {
        // Already inside a reduced context (exponent, subscript, etc.):
        // continue stepping down via canonical superscript() path.
        return fm.superscript();               // L1->L2, L2->L2 (clamped)
    }
    // scriptLevel == 0: top-level display or inline expression.
    // Keep readable glyph size; switch to TEXT style so the tighter
    // non-display fraction gap/shift MATH constants are applied.
    FontMetrics out = fm;                      // same font + same size
    out.style = MathStyle::TEXT;               // compact text-style shifts
    return out;
}

/// Human-readable math style label (pointer to string literal). Diagnostics only.
inline const char* mathStyleName(MathStyle style) {
    switch (style) {
        case MathStyle::DISPLAY_STYLE: return "display";
        case MathStyle::TEXT:          return "text";
        case MathStyle::SCRIPT:        return "script";
        case MathStyle::SCRIPTSCRIPT:  return "scriptscript";
    }
    return "display";
}

constexpr int16_t glyphInkAscentPx(int16_t boxHeight, int16_t offsetY) {
    const int16_t ascent = static_cast<int16_t>(boxHeight + offsetY);
    return ascent > 0 ? ascent : 0;
}

constexpr int16_t glyphInkDescentPx(int16_t offsetY) {
    return offsetY < 0 ? static_cast<int16_t>(-offsetY) : 0;
}

/// Métricas por defecto razonables (≈ STIX Two Math 18 a ~10 px de ancho).
inline FontMetrics defaultFontMetrics() {
    return { 10, 14, 3, 14, 3, 10, 0, nullptr, 18, MathStyle::DISPLAY_STYLE };
}

inline int16_t largeOperatorSymbolPadPx(const FontMetrics& fm) {
    const int16_t px = MathConstantsProvider(fm.emSize).muToPx(4);
    return px < 1 ? 1 : px;
}

inline int16_t largeOperatorTargetHeightPx(const FontMetrics& fm) {
    const int16_t naturalHeight = fm.height();
    if (!fm.isDisplayStyle()) return naturalHeight;
    const int16_t displayMin = MathConstantsProvider(fm.emSize).displayOperatorMinHeight();
    return std::max<int16_t>(naturalHeight, displayMin);
}

inline int16_t largeOperatorGlyphAscentPx(const FontMetrics& fm, int16_t glyphHeightPx) {
    if (glyphHeightPx < 1) glyphHeightPx = fm.height();
    return static_cast<int16_t>(fm.axisHeight() + (glyphHeightPx + 1) / 2);
}

inline int16_t largeOperatorGlyphDescentPx(const FontMetrics& fm, int16_t glyphHeightPx) {
    const int16_t descent = static_cast<int16_t>(
        glyphHeightPx - largeOperatorGlyphAscentPx(fm, glyphHeightPx));
    return descent < 0 ? 0 : descent;
}

inline int16_t largeOperatorBodyGapPx(const FontMetrics& fm) {
    const int16_t px = MathConstantsProvider(fm.emSize).muToPx(3);
    return px < 1 ? 1 : px;
}

inline int16_t integralDifferentialGapPx(const FontMetrics& fm) {
    const int16_t px = MathConstantsProvider(fm.emSize).muToPx(2);
    return px < 1 ? 1 : px;
}

inline int16_t spaceAfterScriptPx(const FontMetrics& fm) {
    return std::max<int16_t>(1, MathConstantsProvider(fm.emSize).spaceAfterScript());
}

#ifndef NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX
#define NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX 0
#endif

/// Signed VPAM readability policy for superscripts.
/// Positive values lower the exponent by reducing the upward shift; negative
/// values raise it. The default remains pure OpenType/TeX placement until
/// hardware photos justify a non-zero policy.
inline constexpr int16_t kSuperscriptVpamAdjustPx =
    static_cast<int16_t>(NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX);

/// Determine if a large operator (∫, ∑, ∏, etc.) should use display-style
/// limits (above/below) rather than inline limits (to the right).
///
/// Per OpenType Spec §4.5: limits are placed above/below only when the
/// style is DISPLAY and the actual selected/assembled operator glyph
/// height meets or exceeds the DisplayOperatorMinHeight MATH constant.
///
/// Callers must compute the operator height first via
/// DelimiterAssembler::glyphMetricsForHeightPx() and pass it here.
inline bool shouldUseDisplayLimits(const FontMetrics& fm,
                                   int16_t operatorGlyphHeightPx) {
    if (!fm.isDisplayStyle()) return false;
    const int16_t displayMin = MathConstantsProvider(fm.emSize).displayOperatorMinHeight();
    return operatorGlyphHeightPx >= displayMin;
}

/// Layout metrics for a fraction, computed by fractionLayoutMetrics().
/// Inline initialization is cleaner than default members since every
/// field is always written by the factory function below.
struct FractionLayoutMetrics {
    int16_t ruleThicknessPx;
    int16_t numeratorShiftUpPx;
    int16_t denominatorShiftDownPx;
    int16_t numeratorGapMinPx;
    int16_t denominatorGapMinPx;
    int16_t ruleOverhangPx;
};

inline FractionLayoutMetrics fractionLayoutMetrics(const FontMetrics& fm) {
    MathConstantsProvider mc(fm.emSize);
    const bool display = fm.isDisplayStyle();

    FractionLayoutMetrics out = {
        mc.fractionRuleThickness(),               // ruleThicknessPx (px with 1px min)
        mc.fractionNumeratorShiftUp(display),     // numeratorShiftUpPx (px)
        mc.fractionDenominatorShiftDown(display), // denominatorShiftDownPx (px)
        mc.fractionNumeratorGapMin(display),      // numeratorGapMinPx (px)
        mc.fractionDenominatorGapMin(display),    // denominatorGapMinPx (px)
        std::max<int16_t>(mc.muToPx(1), 1)       // ruleOverhangPx
    };
    return out;
}

// ════════════════════════════════════════════════════════════════════════════
// LayoutResult — Resultado geométrico del layout de un nodo
// ════════════════════════════════════════════════════════════════════════════
struct LayoutResult {
    int16_t width   = 0;   ///< Ancho total en píxeles
    int16_t ascent  = 0;   ///< Píxeles sobre el baseline (positivo = arriba)
    int16_t descent = 0;   ///< Píxeles bajo el baseline  (positivo = abajo)
    int16_t inkAscent  = 0; ///< Actual visible glyph ink above baseline, if known.
    int16_t inkDescent = 0; ///< Actual visible glyph ink below baseline, if known.

    /// Altura total = ascent + descent
    int16_t height() const { return ascent + descent; }

    /// Distancia del tope de la bounding box al baseline
    int16_t baseline() const { return ascent; }
};

inline bool layoutHasInkBounds(const LayoutResult& layout) {
    return layout.inkAscent > 0 || layout.inkDescent > 0;
}

inline int16_t layoutInkAscentPx(const LayoutResult& layout) {
    return layoutHasInkBounds(layout) ? layout.inkAscent : layout.ascent;
}

inline int16_t layoutInkDescentPx(const LayoutResult& layout) {
    return layoutHasInkBounds(layout) ? layout.inkDescent : layout.descent;
}

struct SuperscriptShiftMetrics {
    int16_t shiftUpPx;
    int16_t bottomMinPx;
    int16_t layoutBottomMinShiftPx;
    int16_t visibleBottomMinShiftPx;
    int16_t texShiftPx;
    int16_t configuredAdjustPx;
    int16_t appliedAdjustPx;
    int16_t shiftPx;
};

inline SuperscriptShiftMetrics superscriptShiftMetrics(
    const FontMetrics& fm, const LayoutResult& expL) {
    MathConstantsProvider mc(fm.emSize);

    SuperscriptShiftMetrics out{};
    out.shiftUpPx = mc.superscriptShiftUp();
    out.bottomMinPx = mc.superscriptBottomMin();
    out.layoutBottomMinShiftPx = static_cast<int16_t>(
        out.bottomMinPx + expL.descent);
    out.visibleBottomMinShiftPx = static_cast<int16_t>(
        out.bottomMinPx + layoutInkDescentPx(expL));
    out.texShiftPx = std::max<int16_t>(
        out.shiftUpPx, out.layoutBottomMinShiftPx);
    if (out.texShiftPx < 1) out.texShiftPx = 1;

    // WHY: the OpenType-derived shift is the baseline. NumOS can optionally
    // apply a signed VPAM readability correction for TFT photo comparison, but
    // the default is zero until hardware proves a non-zero adjustment better.
    // The clamp preserves the existing bottom-min rule for nested powers and
    // fraction exponents.
    const int16_t minShift = std::max<int16_t>(1, out.layoutBottomMinShiftPx);
    out.configuredAdjustPx = kSuperscriptVpamAdjustPx;
    int16_t shifted = static_cast<int16_t>(
        out.texShiftPx - out.configuredAdjustPx);
    if (shifted < minShift) shifted = minShift;
    out.shiftPx = shifted;
    out.appliedAdjustPx = static_cast<int16_t>(
        out.texShiftPx - out.shiftPx);
    return out;
}

inline int16_t superscriptShiftPx(const FontMetrics& fm,
                                  const LayoutResult& expL) {
    return superscriptShiftMetrics(fm, expL).shiftPx;
}

/// Convert a bounding-box top coordinate to the node baseline.
inline int16_t layoutBaselineFromTop(const LayoutResult& layout,
                                     int16_t yTop) {
    return static_cast<int16_t>(yTop + layout.ascent);
}

/// Convert a node baseline coordinate to the top of its bounding box.
inline int16_t layoutTopFromBaseline(const LayoutResult& layout,
                                     int16_t yBaseline) {
    return static_cast<int16_t>(yBaseline - layout.ascent);
}

/// Top coordinate for a row child whose baseline must align to the row baseline.
inline int16_t rowChildTopFromRowTop(const LayoutResult& rowLayout,
                                     const LayoutResult& childLayout,
                                     int16_t rowYTop) {
    return static_cast<int16_t>(
        rowYTop + rowLayout.ascent - childLayout.ascent);
}

inline int16_t mathObjectHeightPx(const LayoutResult& layout,
                                  const FontMetrics& fm,
                                  int16_t verticalPadPx) {
    const int16_t lineH = fm.lineHeight();
    const int16_t minTextH = lineH > 0 ? lineH : fm.height();
    const int16_t contentH = std::max<int16_t>(layout.height(), minTextH);
    return static_cast<int16_t>(contentH + verticalPadPx);
}

/// Gap-based fraction bar geometry — NumOS readable fraction policy.
///
/// Instead of trusting baseline-to-axis shift constants (which produce
/// asymmetric visible gaps because ink ascent/descent differs between
/// numerator and denominator), this helper derives the two shifts from a
/// single symmetric gap G so that:
///
///   numeratorBottom   = barTop  - G   (gapAboveBar == G)
///   denominatorTop    = barBottom + G  (gapBelowBar == G)
///
/// Gap source (no arbitrary offset):
///   G = max(FractionNumDisplayStyleGapMin, FractionDenomDisplayStyleGapMin)
///
/// STIX Two Math provides these as 150 DU ≈ 3 px @ em18.  The inline
/// FractionGapMin (68 DU ≈ 1 px) is too tight to read on the 320x240 TFT,
/// so the NumOS readable-fraction policy intentionally adopts the
/// display-style gap-min.  It scales with em (≈ 2 px at the 12 px script
/// font) and is derived from real OpenType MATH data.
///
/// Allocation-free, no UI dependency, host-testable.
struct FractionBarGaps {
    int16_t numeratorShiftPx;    ///< gap + numerator visible ink descent.
    int16_t denominatorShiftPx;  ///< gap + denominator visible ink ascent.
    int16_t gapPx;               ///< symmetric gap G used for both sides
};

inline FractionBarGaps fractionBarGaps(const FontMetrics& fm,
                                       const LayoutResult& numL,
                                       const LayoutResult& denL) {
    MathConstantsProvider mc(fm.emSize);
    const int16_t gapA = mc.fractionNumeratorGapMin(true);
    const int16_t gapB = mc.fractionDenominatorGapMin(true);
    int16_t gap = gapA > gapB ? gapA : gapB;
    if (gap < 1) gap = 1;
    return {
        static_cast<int16_t>(gap + layoutInkDescentPx(numL)),
        static_cast<int16_t>(gap + layoutInkAscentPx(denL)),
        gap
    };
}

// ════════════════════════════════════════════════════════════════════════════
// MathNode — Clase base abstracta de todos los nodos del AST
// ════════════════════════════════════════════════════════════════════════════
class MathNode {
public:
    virtual ~MathNode() = default;

    // ── PSRAM allocation — all AST nodes go to external RAM ──
    void* operator new(std::size_t size);
    void  operator delete(void* ptr) noexcept;

    // ── Identidad ──
    NodeType type() const { return _type; }

    // ── TeX Math Class (for inter-atom spacing) ──
    /// Returns the TeX atom class of this node.
    /// Default is ORD (ordinary). Override in nodes that need different spacing.
    virtual MathClass mathClass() const { return MathClass::ORD; }

    /// TeX atom class for the LEFT side of this node when it appears in a row.
    /// Defaults to mathClass(). Override when left/right classes differ
    /// (e.g. NodeParen: OPEN on left, CLOSE on right).
    virtual MathClass leftMathClass() const { return mathClass(); }

    /// TeX atom class for the RIGHT side of this node when it appears in a row.
    /// Defaults to mathClass(). Override when left/right classes differ.
    virtual MathClass rightMathClass() const { return mathClass(); }

    // ── Script level ──
    uint8_t scriptLevel() const { return _scriptLevel; }
    void setScriptLevel(uint8_t level) { _scriptLevel = (level > 2) ? 2 : level; }

    // ── Navegación por el árbol (padre no-owning) ──
    MathNode* parent() const    { return _parent; }
    void setParent(MathNode* p) { _parent = p; }

    // ── Layout ──
    const LayoutResult& layout() const { return _layout; }

    /**
     * Calcula recursivamente {width, ascent, descent} para este nodo
     * y todos sus descendientes.
     * @param fm  Métricas tipográficas del contexto actual.
     */
    virtual void calculateLayout(const FontMetrics& fm) = 0;

    // ── Hijos (para navegación genérica del cursor) ──
    virtual int        childCount()          const { return 0; }
    virtual MathNode*  child(int /*index*/)  const { return nullptr; }

protected:
    explicit MathNode(NodeType t) : _type(t), _parent(nullptr), _scriptLevel(0) {}

    NodeType     _type;
    MathNode*    _parent;
    uint8_t      _scriptLevel;
    LayoutResult _layout;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeRow — Secuencia horizontal de nodos hijos
//
// Es el "contenedor por defecto". Las ranuras de FractionNode, PowerNode,
// RootNode y ParenNode son todas NodeRow, lo que permite insertar
// múltiples nodos dentro de cualquier ranura.
// ════════════════════════════════════════════════════════════════════════════
class NodeRow : public MathNode {
public:
    NodeRow();

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override;  // Propagates from first non-empty child

    // ── Hijos ──
    int       childCount()        const override;
    MathNode* child(int index)    const override;

    const std::vector<NodePtr>& children() const { return _children; }
    bool isEmpty() const { return _children.empty(); }

    // ── Mutación ──
    void    appendChild(NodePtr node);
    void    insertChild(int index, NodePtr node);
    NodePtr removeChild(int index);
    void    replaceChild(int index, NodePtr node);
    void    clear();

private:
    std::vector<NodePtr> _children;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeNumber — Literal numérico (secuencia de dígitos + punto decimal)
//
// Almacena la representación textual: "42", "3.14", "0".
// El cursor (Fase 2) podrá moverse por dentro carácter a carácter.
// ════════════════════════════════════════════════════════════════════════════
class NodeNumber : public MathNode {
public:
    explicit NodeNumber(const std::string& value = "");

    void calculateLayout(const FontMetrics& fm) override;

    // ── Acceso al valor ──
    const std::string& value() const { return _value; }
    void setValue(const std::string& v) { _value = v; }

    // ── Edición carácter a carácter ──
    void appendChar(char c);
    void deleteLastChar();
    bool hasDecimalPoint() const;
    int  length() const { return static_cast<int>(_value.size()); }

private:
    std::string _value;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeOperator — Operador binario: +, −, ×
// ════════════════════════════════════════════════════════════════════════════
class NodeOperator : public MathNode {
public:
    explicit NodeOperator(OpKind op);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override {
        return isRelation(_op) ? MathClass::REL : MathClass::BINARY;
    }

    OpKind      op()     const { return _op; }
    const char* symbol() const;

    // ── No manual padding — all inter-atom spacing is handled by
    //     NodeRow::calculateLayout via the TeX kSpacingTable.

private:
    OpKind _op;
};

class NodeRelation : public NodeOperator {
public:
    explicit NodeRelation(OpKind op)
        : NodeOperator(isRelation(op) ? op : OpKind::Eq) {}

    MathClass mathClass() const override { return MathClass::REL; }
};

// ════════════════════════════════════════════════════════════════════════════
// NodeEmpty — Placeholder □ ("cuadrado vacío")
//
// Marca una posición donde el usuario aún no ha escrito nada.
// Se renderiza como un rectángulo tenue/punteado.
// ════════════════════════════════════════════════════════════════════════════
class NodeEmpty : public MathNode {
public:
    NodeEmpty();

    void calculateLayout(const FontMetrics& fm) override;

    /// Tamaño visual mínimo del placeholder
    static constexpr int16_t MIN_WIDTH  = 11;
    static constexpr int16_t MIN_HEIGHT = 13;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeFraction — Fracción vertical: numerador / denominador
//
// Ambos hijos son NodeRow (pueden contener múltiples nodos).
// Constructor por defecto → dos NodeRow con un NodeEmpty cada uno.
//
// Layout:
//   ┌──────────────┐  ← tope: ascent desde baseline
//   │  numerador    │
//   │───────────────│  ← barra sobre el eje matemático
//   │  denominador  │
//   └──────────────┘  ← fondo: descent desde baseline
// ════════════════════════════════════════════════════════════════════════════
class NodeFraction : public MathNode {
public:
    NodeFraction();
    NodeFraction(NodePtr numerator, NodePtr denominator);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    // ── Acceso directo ──
    MathNode* numerator()   const { return _numerator.get(); }
    MathNode* denominator() const { return _denominator.get(); }

    void setNumerator(NodePtr node);
    void setDenominator(NodePtr node);

    int16_t ruleThicknessPx() const { return _ruleThicknessPx; }
    int16_t barHalfUpPx() const { return _barHalfUpPx; }
    int16_t barHalfDownPx() const { return _barHalfDownPx; }
    int16_t numeratorShiftPx() const { return _numeratorShiftPx; }
    int16_t denominatorShiftPx() const { return _denominatorShiftPx; }
    int16_t ruleOverhangPx() const { return _ruleOverhangPx; }

private:
    NodePtr _numerator;
    NodePtr _denominator;
    int16_t _ruleThicknessPx = 1;
    int16_t _barHalfUpPx = 1;
    int16_t _barHalfDownPx = 0;
    int16_t _numeratorShiftPx = 1;
    int16_t _denominatorShiftPx = 1;
    int16_t _ruleOverhangPx = 1;

    /// Crea un NodeRow con un NodeEmpty dentro
    static NodePtr makeEmptySlot();
};

// ════════════════════════════════════════════════════════════════════════════
// NodePower — Potencia: base^exponente
//
// base     → NodeRow (contenido capturado, ej. el "3" de "2+3^")
// exponent → NodeRow (superíndice, se renderiza en fuente reducida)
//
// Layout:
//   ┌base┐┌exp┐   El exponente se eleva al ~45% del ascent de la base.
//   │ 23 ││ 4 │   La fuente del exponente es ~70% de la normal.
//   └────┘└───┘
// ════════════════════════════════════════════════════════════════════════════
class NodePower : public MathNode {
public:
    NodePower();
    NodePower(NodePtr base, NodePtr exponent);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    MathNode* base()     const { return _base.get(); }
    MathNode* exponent() const { return _exponent.get(); }

    void setBase(NodePtr node);
    void setExponent(NodePtr node);

    /// Italics correction in px added between base and exponent (Spec §2.5).
    /// Non-zero for italic single-letter bases like f, V, W.
    int16_t italicCorrectionPx() const { return _italicCorrectionPx; }

private:
    NodePtr _base;
    NodePtr _exponent;
    int16_t _italicCorrectionPx = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeRoot — Radical: √(contenido) o ⁿ√(contenido)
//
// radicand → NodeRow (contenido bajo el radical)
// degree   → NodeRow (índice opcional, ej. ³√ — nullptr para raíz cuadrada)
//
// Layout:
//   ┌degree─┐
//   │   ╱‾‾‾‾‾‾‾‾┐   overline
//   │  ╱ radicand │
//   │ ╱           │
//   └╱────────────┘
//    hook  slope
//
// The overline gap and thickness are driven by MATH table constants
// (radicalVerticalGap, radicalRuleThickness). The hook and slope are
// drawn manually from rendering constants.
// ════════════════════════════════════════════════════════════════════════════
class NodeRoot : public MathNode {
public:
    NodeRoot();
    explicit NodeRoot(NodePtr radicand, NodePtr degree = nullptr);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }

    int       childCount()     const override;
    MathNode* child(int index) const override;

    MathNode* radicand()  const { return _radicand.get(); }
    MathNode* degree()    const { return _degree.get(); }
    bool      hasDegree() const { return _degree != nullptr; }

    void setRadicand(NodePtr node);
    void setDegree(NodePtr node);

    // ── Rendering geometry (drawn manually — not in MATH table) ──
    static constexpr int16_t RADICAL_HOOK_W    = 3;   ///< Width of the small √ hook
    static constexpr int16_t RADICAL_SLOPE_W   = 6;   ///< Width of the ascending line
    static constexpr int16_t RADICAL_RIGHT_PAD = 2;   ///< Right padding after content

    // ── MATH table-driven values (computed in calculateLayout) ──
    int16_t radicalRuleThickness()  const { return _radicalRuleThickness; }
    int16_t radicalVerticalGap()    const { return _radicalVerticalGap; }
    int16_t radicalExtraAscender()  const { return _radicalExtraAscender; }
    int16_t radicalKernBefore()     const { return _radicalKernBefore; }
    int16_t radicalKernAfter()      const { return _radicalKernAfter; }

private:
    NodePtr _radicand;
    NodePtr _degree;   ///< nullptr → raíz cuadrada

    int16_t _radicalRuleThickness = 1;
    int16_t _radicalVerticalGap   = 2;
    int16_t _radicalExtraAscender = 1;
    int16_t _radicalKernBefore    = 0;
    int16_t _radicalKernAfter     = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeParen — Delimited sub-formula: ( contenido ), [ contenido ], etc.
//
// content → NodeRow
//
// The delimiter pair is selected at construction time via DelimKind.
// Delimiters stretch vertically via GlyphAssembly to match content height.
// ════════════════════════════════════════════════════════════════════════════
class NodeParen : public MathNode {
public:
    explicit NodeParen(DelimKind delimKind = DelimKind::Paren);
    NodeParen(NodePtr content, DelimKind delimKind = DelimKind::Paren);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }  // TeXbook Rule 15: parenthesized subformula is INNER
    MathClass leftMathClass() const override { return MathClass::OPEN; }   // '(' / '[' / '{' opens
    MathClass rightMathClass() const override { return MathClass::CLOSE; } // ')' / ']' / '}' closes

    int       childCount()     const override { return 1; }
    MathNode* child(int index) const override;

    MathNode* content() const { return _content.get(); }
    void setContent(NodePtr node);

    /// Delimiter pair kind (Paren, Bracket, Brace, Bar).
    DelimKind delimKind() const { return _delimKind; }

    /// Left delimiter codepoint for font lookup.
    uint32_t leftCp() const { return delimLeftCp(_delimKind); }

    /// Right delimiter codepoint for font lookup.
    uint32_t rightCp() const { return delimRightCp(_delimKind); }

    /// Dynamic delimiter width computed during layout (px).
    int16_t parenWidth() const { return _parenWidth; }

private:
    NodePtr   _content;
    int16_t   _parenWidth = 0;
    DelimKind _delimKind = DelimKind::Paren;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeFunction — Función matemática: sin(x), cos(x), ln(x), etc.
//
// Visualmente: texto de la función + paréntesis automáticos + contenido.
//   argument → NodeRow (contenido dentro de los paréntesis)
//
// Layout:
//   ┌────────┐┌───────────────┐
//   │ sin    ││(  argument   )│
//   └────────┘└───────────────┘
//   label       auto-paren
//
// El paréntesis se dibuja automáticamente (NO es un NodeParen hijo),
// para mantener la semántica de "función aplicada a argumento".
// ════════════════════════════════════════════════════════════════════════════
class NodeFunction : public MathNode {
public:
    NodeFunction();
    explicit NodeFunction(FuncKind kind, NodePtr argument = nullptr);

    void calculateLayout(const FontMetrics& fm) override;

    int       childCount()     const override { return 1; }
    MathNode* child(int index) const override;

    FuncKind    funcKind()  const { return _kind; }
    MathNode*   argument()  const { return _argument.get(); }
    const char* label()     const;   ///< "sin", "cos", "ln", etc.

    void setArgument(NodePtr node);

    int16_t labelWidth() const { return _labelWidth; }
    int16_t parenWidth() const { return _parenWidth; }
    int16_t innerPad() const { return _innerPad; }
    int16_t parenAscent() const { return _parenAscent; }
    int16_t parenDescent() const { return _parenDescent; }

    /// Gap entre el texto de la función y el paréntesis abierto
    static constexpr int16_t LABEL_GAP = 0;

private:
    FuncKind _kind;
    NodePtr  _argument;   ///< Contenido del argumento (NodeRow)

    int16_t _labelWidth;  ///< Ancho del texto de la etiqueta (calculado)
    int16_t _parenWidth = 0;
    int16_t _innerPad = 0;
    int16_t _parenAscent = 0;
    int16_t _parenDescent = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeLogBase — Logaritmo de base custom: log_n(x)
//
// Visualmente: "log" + subíndice (base) + paréntesis(argumento)
//   base     → NodeRow (subíndice, ej. "2" para log₂)
//   argument → NodeRow (contenido entre paréntesis)
//
// Layout:
//   ┌─────┐┌base┐┌─────────────┐
//   │ log ││ 2  ││( argument  )│
//   └─────┘└────┘└─────────────┘
//           subscript
//
// El subíndice se renderiza en fuente reducida (~70%) BAJADA respecto
// al baseline, como la lógica inversa del NodePower (superíndice).
// ════════════════════════════════════════════════════════════════════════════
class NodeLogBase : public MathNode {
public:
    NodeLogBase();
    explicit NodeLogBase(NodePtr base, NodePtr argument = nullptr);

    void calculateLayout(const FontMetrics& fm) override;

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    MathNode* base()     const { return _base.get(); }
    MathNode* argument() const { return _argument.get(); }

    void setBase(NodePtr node);
    void setArgument(NodePtr node);

    int16_t labelWidth() const { return _labelWidth; }
    int16_t parenWidth() const { return _parenWidth; }
    int16_t innerPad() const { return _innerPad; }
    int16_t parenAscent() const { return _parenAscent; }
    int16_t parenDescent() const { return _parenDescent; }

    static constexpr int16_t LABEL_GAP = 0;

private:
    NodePtr _base;       ///< Subíndice (base del log)
    NodePtr _argument;   ///< Argumento entre paréntesis

    int16_t _labelWidth; ///< Ancho de "log" (calculado)
    int16_t _parenWidth = 0;
    int16_t _innerPad = 0;
    int16_t _parenAscent = 0;
    int16_t _parenDescent = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeConstant — Constante algebraica: π, e
//
// Se renderiza como un solo símbolo. En la evaluación (Parte 2) se trata
// como variable algebraica para permitir simplificaciones (3*π, e²).
// ════════════════════════════════════════════════════════════════════════════
class NodeConstant : public MathNode {
public:
    explicit NodeConstant(ConstKind kind = ConstKind::Pi);

    void calculateLayout(const FontMetrics& fm) override;

    ConstKind   constKind() const { return _kind; }
    const char* symbol()    const;   ///< "π" o "e"

private:
    ConstKind _kind;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeVariable — Variable algebraica: x, y, z, A-F, Ans, PreAns
//
// Se renderiza como texto (nombre), con estilo visual especial:
//   · x, y, z → azul (#4A90D9), cursiva si la fuente lo soporta
//   · A-F     → color normal, texto en mayúscula
//   · Ans     → bloque "Ans"
//   · PreAns  → bloque "PreAns"
//
// En evaluación, pide su valor al VariableManager::instance().
//
// El char 'name_' identifica la variable:
//   '#' = Ans, '$' = PreAns, 'A'-'F', 'x', 'y', 'z'
// ════════════════════════════════════════════════════════════════════════════
class NodeVariable : public MathNode {
public:
    explicit NodeVariable(char name = 'x');

    void calculateLayout(const FontMetrics& fm) override;

    char        name()  const { return _name; }
    const char* label() const;   ///< "x", "A", "Ans", "PreAns", etc.

    /// ¿Es una variable de función (x, y, z)?
    bool isFunctionVar() const { return _name == 'x' || _name == 'y' || _name == 'z'; }

    /// ¿Es Ans o PreAns?
    bool isAnsVar() const { return _name == '#' || _name == '$'; }

private:
    char _name;
    int16_t _labelWidth;   ///< Ancho calculado del label
};

// ════════════════════════════════════════════════════════════════════════════
// NodePeriodicDecimal — Decimal periódico con overline (SOLO RESULTADO)
//
// Representación visual de un decimal periódico: ej. 0.1̄6̄ o 0.̄3̄
//
// Estructura: integerPart . nonRepeating  repeating
//                                          ‾‾‾‾‾‾‾‾ ← overline
//
// Ejemplo: 1/6 = 0.1666... → intPart="0", nonRepeat="1", repeat="6"
// Ejemplo: 1/3 = 0.333...  → intPart="0", nonRepeat="",  repeat="3"
// Ejemplo: 1/7 = 0.142857142857... → intPart="0", nonRepeat="", repeat="142857"
//
// Este nodo es generado EXCLUSIVAMENTE por el evaluador para el modo
// periódico (Modo 2 S⇔D). No se crea por el usuario.
// ════════════════════════════════════════════════════════════════════════════
class NodePeriodicDecimal : public MathNode {
public:
    NodePeriodicDecimal(const std::string& intPart,
                        const std::string& nonRepeat,
                        const std::string& repeat,
                        bool negative = false);

    void calculateLayout(const FontMetrics& fm) override;

    const std::string& intPart()    const { return _intPart; }
    const std::string& nonRepeat()  const { return _nonRepeat; }
    const std::string& repeat()     const { return _repeat; }
    bool               isNegative() const { return _negative; }

    /// Grosor de la overline sobre los dígitos periódicos
    static constexpr int16_t OVERLINE_T   = 1;
    /// Espacio entre tope de dígitos y la overline
    static constexpr int16_t OVERLINE_GAP = 1;

private:
    std::string _intPart;     ///< Parte entera: "0", "1", "123"
    std::string _nonRepeat;   ///< Dígitos no periódicos tras el punto
    std::string _repeat;      ///< Patrón que se repite (con overline)
    bool        _negative;    ///< ¿Valor negativo?
};

inline std::size_t periodicDecimalPrefixCharCount(bool negative,
                                                  std::size_t intPartLen,
                                                  std::size_t nonRepeatLen,
                                                  std::size_t repeatLen) {
    std::size_t count = negative ? 1 : 0;
    count += intPartLen;
    if (nonRepeatLen != 0 || repeatLen != 0) {
        ++count;
        count += nonRepeatLen;
    }
    return count;
}

// ════════════════════════════════════════════════════════════════════════════
// NodeDefIntegral — Integral definida: ∫_a^b f(x) dx
//
// 4 hijos, todos NodeRow:
//   lower    → Límite inferior (ej. "0")
//   upper    → Límite superior (ej. "1")
//   body     → Expresión integrando (ej. "x^2+1")
//   variable → Variable de integración (ej. "x")
//
// Layout:
//   ┌ upper ┐
//   │  ∫    │ body  d(var)
//   └ lower ┘
//
// El símbolo ∫ se dibuja como vectores (curva S) con la API de LVGL.
// Los límites se renderizan en fuente reducida arriba y abajo del símbolo.
// ════════════════════════════════════════════════════════════════════════════
class NodeDefIntegral : public MathNode {
public:
    NodeDefIntegral();
    NodeDefIntegral(NodePtr lower, NodePtr upper, NodePtr body, NodePtr variable);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::OP; }  // Large operator

    int       childCount()     const override { return 4; }
    MathNode* child(int index) const override;

    MathNode* lower()    const { return _lower.get(); }
    MathNode* upper()    const { return _upper.get(); }
    MathNode* body()     const { return _body.get(); }
    MathNode* variable() const { return _variable.get(); }

    void setLower(NodePtr node);
    void setUpper(NodePtr node);
    void setBody(NodePtr node);
    void setVariable(NodePtr node);

private:
    NodePtr _lower;
    NodePtr _upper;
    NodePtr _body;
    NodePtr _variable;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeSummation — Sumatorio: ∑_{n=a}^{b} expr
//
// 3 hijos, todos NodeRow:
//   lower    → Límite inferior (ej. "n=1")
//   upper    → Límite superior (ej. "10")
//   body     → Expresión a sumar (ej. "n^2")
//
// NOTA: A diferencia de NodeDefIntegral, NodeSummation NO tiene un hijo
// "variable" separado — la variable de iteración va embebida en el límite
// inferior ("n=1"). Anteriormente tenía 4 hijos con un _variable muerto
// (nunca usado en layout/renderizado), eliminado en la auditoría Phases 1-4.
//
// Layout:
//   ┌ upper ┐
//   │  ∑    │ body
//   └ lower ┘
//
// El símbolo ∑ se dibuja como glifo nativo de STIX Two Math (U+2211).
// Los límites se renderizan en fuente reducida arriba y abajo (display style)
// o como sub/superíndices a la derecha (text/script style).
// ════════════════════════════════════════════════════════════════════════════
class NodeSummation : public MathNode {
public:
    NodeSummation();
    NodeSummation(NodePtr lower, NodePtr upper, NodePtr body);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::OP; }  // Large operator

    int       childCount()     const override { return 3; }
    MathNode* child(int index) const override;

    MathNode* lower()    const { return _lower.get(); }
    MathNode* upper()    const { return _upper.get(); }
    MathNode* body()     const { return _body.get(); }

    void setLower(NodePtr node);
    void setUpper(NodePtr node);
    void setBody(NodePtr node);

private:
    NodePtr _lower;
    NodePtr _upper;
    NodePtr _body;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeSubscript — Generic subscript: base_subscript (e.g., x₁, x₂)
//
// Visually: base + subscript bajado respecto al baseline
//   base      → NodeRow (contenido principal, ej. "x")
//   subscript → NodeRow (subíndice, ej. "1", "2")
//
// Layout:
//   ┌base┐┌sub┐   El subíndice se baja ~33% del descent (como NodeLogBase)
//   │ x  ││ 1 │   La fuente del subíndice es ~70% de la normal.
//   └────┘└───┘
//
// Used by the Educational Tutor Engine for x₁, x₂ in quadratic solutions.
// ════════════════════════════════════════════════════════════════════════════
class NodeSubscript : public MathNode {
public:
    NodeSubscript();
    NodeSubscript(NodePtr base, NodePtr subscript);

    void calculateLayout(const FontMetrics& fm) override;

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    MathNode* base()      const { return _base.get(); }
    MathNode* subscript() const { return _subscript.get(); }

    void setBase(NodePtr node);
    void setSubscript(NodePtr node);

private:
    NodePtr _base;
    NodePtr _subscript;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeBigOp — Generic large n-ary operator: ∏, ⋂, ⋃, ⋀, ⋁
//
// 3 hijos, todos NodeRow:
//   lower    → Límite inferior (ej. "i=1")
//   upper    → Límite superior (ej. "n")
//   body     → Expresión (ej. "x_i")
//
// Style-dependent limit positioning (OpenType Spec §4.5, TeXbook Rule 13):
//   MathStyle::DISPLAY:
//     Limits centered above/below the operator.
//     Uses a larger size variant if glyph advance ≥ DisplayOperatorMinHeight.
//     Layout:
//       ┌ upper ┐
//       │  ∏    │ body
//       └ lower ┘
//
//   MathStyle::TEXT / SCRIPT / SCRIPTSCRIPT:
//     Limits as sub/superscript to the RIGHT of the operator (inline form).
//     Layout:
//       ∏_{lower}^{upper} body
//     (lower=subscript, upper=superscript, both to the right)
// ════════════════════════════════════════════════════════════════════════════
class NodeBigOp : public MathNode {
public:
    NodeBigOp();
    NodeBigOp(BigOpKind kind, NodePtr lower, NodePtr upper, NodePtr body);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::OP; }

    int       childCount()     const override { return 3; }
    MathNode* child(int index) const override;

    BigOpKind bigOpKind() const { return _kind; }
    uint32_t  operatorCodepoint() const { return _operatorCp; }
    const char* operatorUtf8() const { return _operatorUtf8; }

    MathNode* lower()   const { return _lower.get(); }
    MathNode* upper()   const { return _upper.get(); }
    MathNode* body()    const { return _body.get(); }

    void setLower(NodePtr node);
    void setUpper(NodePtr node);
    void setBody(NodePtr node);

    /// True if the operator glyph is tall enough for display-style limits (Spec §4.5).
    bool useDisplayLimits() const { return _useDisplayLimits; }

private:
    BigOpKind   _kind;
    uint32_t    _operatorCp;        ///< Unicode codepoint for rendering
    const char* _operatorUtf8;      ///< UTF-8 bytes for rendering (points to string literal)
    NodePtr     _lower;
    NodePtr   _upper;
    NodePtr   _body;
    bool      _useDisplayLimits;  ///< Computed during layout
};

// MATH-RESULTS-01 semantic result nodes.  Their owning vectors are populated
// during typed conversion; calculateLayout() only traverses them and uses
// fixed-capacity geometry, so the rendering hot path performs no allocation.

class NodeSymbol : public MathNode {
public:
    explicit NodeSymbol(std::string name);
    void calculateLayout(const FontMetrics& fm) override;
    const std::string& name() const { return _name; }
private:
    std::string _name;
};

class NodeSpecialValue : public MathNode {
public:
    explicit NodeSpecialValue(SpecialValueKind kind);
    void calculateLayout(const FontMetrics& fm) override;
    SpecialValueKind specialKind() const { return _kind; }
    const char* label() const;
private:
    SpecialValueKind _kind;
};

class NodeCollection : public MathNode {
public:
    explicit NodeCollection(CollectionKind kind);
    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }
    MathClass leftMathClass() const override { return MathClass::OPEN; }
    MathClass rightMathClass() const override { return MathClass::CLOSE; }
    int childCount() const override;
    MathNode* child(int index) const override;
    CollectionKind collectionKind() const { return _kind; }
    void appendElement(NodePtr element);
    int16_t delimiterWidth() const { return _delimiterWidth; }
    int16_t innerPad() const { return _innerPad; }
    int16_t separatorWidth() const { return _separatorWidth; }
    uint32_t leftCp() const { return _kind == CollectionKind::Set ? 0x007B : 0x005B; }
    uint32_t rightCp() const { return _kind == CollectionKind::Set ? 0x007D : 0x005D; }
private:
    CollectionKind _kind;
    std::vector<NodePtr> _elements;
    int16_t _delimiterWidth = 0;
    int16_t _innerPad = 0;
    int16_t _separatorWidth = 0;
};

class NodeEquation : public MathNode {
public:
    NodeEquation(NodePtr lhs, NodePtr rhs,
                 EquationKind kind = EquationKind::Equation);
    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }
    int childCount() const override { return 2; }
    MathNode* child(int index) const override;
    MathNode* lhs() const { return _lhs.get(); }
    MathNode* rhs() const { return _rhs.get(); }
    EquationKind equationKind() const { return _kind; }
    int16_t relationWidth() const { return _relationWidth; }
    int16_t relationGap() const { return _relationGap; }
private:
    NodePtr _lhs;
    NodePtr _rhs;
    EquationKind _kind;
    int16_t _relationWidth = 0;
    int16_t _relationGap = 0;
};

class NodeMatrix : public MathNode {
public:
    static constexpr uint8_t kCapacity = 6;
    NodeMatrix(uint8_t rows, uint8_t columns);
    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }
    int childCount() const override;
    MathNode* child(int index) const override;
    uint8_t rows() const { return _rows; }
    uint8_t columns() const { return _columns; }
    MathNode* cell(uint8_t row, uint8_t column) const;
    bool setCell(uint8_t row, uint8_t column, NodePtr cell);
    int16_t delimiterWidth() const { return _delimiterWidth; }
    int16_t horizontalPad() const { return _horizontalPad; }
    int16_t verticalPad() const { return _verticalPad; }
    int16_t columnWidth(uint8_t column) const;
    int16_t rowBaseline(uint8_t row) const;
private:
    uint8_t _rows;
    uint8_t _columns;
    std::vector<NodePtr> _cells;
    std::array<int16_t, kCapacity> _columnWidths{};
    std::array<int16_t, kCapacity> _rowBaselines{};
    std::array<int16_t, kCapacity> _rowAscents{};
    std::array<int16_t, kCapacity> _rowDescents{};
    int16_t _delimiterWidth = 0;
    int16_t _horizontalPad = 0;
    int16_t _verticalPad = 0;
};

class NodeInterval : public MathNode {
public:
    NodeInterval(NodePtr lower, NodePtr upper,
                 bool leftClosed, bool rightClosed);
    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }
    int childCount() const override { return 2; }
    MathNode* child(int index) const override;
    MathNode* lower() const { return _lower.get(); }
    MathNode* upper() const { return _upper.get(); }
    bool leftClosed() const { return _leftClosed; }
    bool rightClosed() const { return _rightClosed; }
    uint32_t leftCp() const { return _leftClosed ? 0x005B : 0x0028; }
    uint32_t rightCp() const { return _rightClosed ? 0x005D : 0x0029; }
    int16_t delimiterWidth() const { return _delimiterWidth; }
    int16_t innerPad() const { return _innerPad; }
    int16_t separatorWidth() const { return _separatorWidth; }
private:
    NodePtr _lower;
    NodePtr _upper;
    bool _leftClosed;
    bool _rightClosed;
    int16_t _delimiterWidth = 0;
    int16_t _innerPad = 0;
    int16_t _separatorWidth = 0;
};

class NodePiecewise : public MathNode {
public:
    static constexpr uint8_t kCapacity = 6;
    struct Branch {
        NodePtr expression;
        NodePtr condition;
        bool otherwise = false;
    };
    NodePiecewise();
    bool appendBranch(NodePtr expression, NodePtr condition, bool otherwise);
    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }
    int childCount() const override;
    MathNode* child(int index) const override;
    uint8_t branchCount() const { return static_cast<uint8_t>(_branches.size()); }
    const Branch& branch(uint8_t index) const { return _branches[index]; }
    int16_t braceWidth() const { return _braceWidth; }
    int16_t expressionColumnWidth() const { return _expressionColumnWidth; }
    int16_t columnGap() const { return _columnGap; }
    int16_t bracePad() const { return _bracePad; }
    int16_t rowBaseline(uint8_t row) const { return _rowBaselines[row]; }
private:
    std::vector<Branch> _branches;
    std::array<int16_t, kCapacity> _rowBaselines{};
    int16_t _braceWidth = 0;
    int16_t _expressionColumnWidth = 0;
    int16_t _conditionColumnWidth = 0;
    int16_t _columnGap = 0;
    int16_t _bracePad = 0;
};

class NodeCall : public MathNode {
public:
    explicit NodeCall(std::string name);
    void appendArgument(NodePtr argument);
    void calculateLayout(const FontMetrics& fm) override;
    int childCount() const override;
    MathNode* child(int index) const override;
    const std::string& name() const { return _name; }
    int16_t labelWidth() const { return _labelWidth; }
    int16_t delimiterWidth() const { return _delimiterWidth; }
    int16_t innerPad() const { return _innerPad; }
    int16_t separatorWidth() const { return _separatorWidth; }
    int16_t labelGap() const { return _labelGap; }
private:
    std::string _name;
    std::vector<NodePtr> _arguments;
    int16_t _labelWidth = 0;
    int16_t _delimiterWidth = 0;
    int16_t _innerPad = 0;
    int16_t _separatorWidth = 0;
    int16_t _labelGap = 0;
};

class NodeUnevaluated : public MathNode {
public:
    explicit NodeUnevaluated(NodePtr expression);
    void calculateLayout(const FontMetrics& fm) override;
    int childCount() const override { return 1; }
    MathNode* child(int index) const override;
    MathNode* expression() const { return _expression.get(); }
    int16_t labelWidth() const { return _labelWidth; }
    int16_t gap() const { return _gap; }
private:
    NodePtr _expression;
    int16_t _labelWidth = 0;
    int16_t _gap = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// Factory helpers — Creación rápida de nodos
// ════════════════════════════════════════════════════════════════════════════
NodePtr makeRow();
NodePtr makeNumber(const std::string& value);
NodePtr makeOperator(OpKind op);
NodePtr makeEmpty();

/// Fracción con numerador y denominador opcionales (nullptr → slot vacío)
NodePtr makeFraction(NodePtr num = nullptr, NodePtr den = nullptr);

/// Potencia con base y exponente opcionales (nullptr → slot vacío)
NodePtr makePower(NodePtr base = nullptr, NodePtr exp = nullptr);

/// Raíz con radicando y grado opcionales (nullptr → slot vacío / raíz cuadrada)
NodePtr makeRoot(NodePtr radicand = nullptr, NodePtr degree = nullptr);

/// Delimited sub-formula with optional content (nullptr → empty slot).
/// @param kind  Delimiter pair (default: parentheses).
NodePtr makeParen(NodePtr content = nullptr, DelimKind kind = DelimKind::Paren);
inline NodePtr makeBracket(NodePtr content = nullptr) {
    return makeParen(std::move(content), DelimKind::Bracket);
}
inline NodePtr makeBrace(NodePtr content = nullptr) {
    return makeParen(std::move(content), DelimKind::Brace);
}
inline NodePtr makeBar(NodePtr content = nullptr) {
    return makeParen(std::move(content), DelimKind::Bar);
}

/// Función matemática con argumento opcional
NodePtr makeFunction(FuncKind kind, NodePtr argument = nullptr);

/// Logaritmo de base custom con base y argumento opcionales
NodePtr makeLogBase(NodePtr base = nullptr, NodePtr argument = nullptr);

/// Constante algebraica (π o e)
NodePtr makeConstant(ConstKind kind);

/// Variable algebraica (x, y, z, A-F, Ans, PreAns)
NodePtr makeVariable(char name);
NodePtr makeSymbol(const std::string& name);
NodePtr makeSpecialValue(SpecialValueKind kind);
NodePtr makeCollection(CollectionKind kind);
NodePtr makeEquation(NodePtr lhs, NodePtr rhs,
                     EquationKind kind = EquationKind::Equation);
NodePtr makeMatrix(uint8_t rows, uint8_t columns);
NodePtr makeInterval(NodePtr lower, NodePtr upper,
                     bool leftClosed, bool rightClosed);
NodePtr makePiecewise();
NodePtr makeCall(const std::string& name);
NodePtr makeUnevaluated(NodePtr expression);

/// Decimal periódico (solo para resultados)
NodePtr makePeriodicDecimal(const std::string& intPart,
                            const std::string& nonRepeat,
                            const std::string& repeat,
                            bool negative = false);

/// Integral definida con límites, cuerpo y variable opcionales
NodePtr makeDefIntegral(NodePtr lower = nullptr, NodePtr upper = nullptr,
                        NodePtr body = nullptr, NodePtr variable = nullptr);

/// Subscript genérico: base con subíndice (ej. x₁, x₂)
NodePtr makeSubscript(NodePtr base = nullptr, NodePtr subscript = nullptr);

/// Sumatorio con límites y cuerpo opcionales
/// NOTA: a diferencia de makeDefIntegral, no tiene parámetro "variable" —
/// la variable de iteración va embebida en el límite inferior (ej. "n=1").
NodePtr makeSummation(NodePtr lower = nullptr, NodePtr upper = nullptr,
                      NodePtr body = nullptr);

/// Large n-ary operator (∏, ⋂, ⋃, etc.) with style-dependent limits
NodePtr makeBigOp(BigOpKind kind, NodePtr lower = nullptr,
                  NodePtr upper = nullptr, NodePtr body = nullptr);

/// Relation operator (=, <, >, ≤, ≥, ≠) with MathClass::REL for TeX spacing.
NodePtr makeRelation(OpKind op);

// ════════════════════════════════════════════════════════════════════════════
// Debug — Volcado legible del árbol
//
// Ejemplo de salida:
//   Row
//     Number "2"
//     Operator "+"
//     Fraction
//       num: Row
//         Number "3"
//       den: Row
//         Empty □
// ════════════════════════════════════════════════════════════════════════════
std::string dumpTree(const MathNode* node, int indent = 0);

// ════════════════════════════════════════════════════════════════════════════
// Deep clone — Copia profunda de un subárbol AST
//
// Devuelve un nuevo árbol independiente. Los punteros parent se actualizan
// correctamente en la copia. Útil para el sistema de historial.
// ════════════════════════════════════════════════════════════════════════════
NodePtr cloneNode(const MathNode* node);

// ════════════════════════════════════════════════════════════════════════════
// CursorTargetRole — Classify a NodeRow relative to the math AST root
//
// Allocation-free diagnostic helper for [MATH_CURSOR] traces and host tests.
// Does not encode UI policy; only reports structural slot identity.
// ════════════════════════════════════════════════════════════════════════════
enum class CursorTargetRole : uint8_t {
    Root,
    FractionNumerator,
    FractionDenominator,
    PowerBase,
    PowerExponent,
    RootRadicand,
    RootDegree,
    ParenContent,
    FunctionArg,
    LogBase,
    LogArg,
    Subscript,
    BigOpUpper,
    BigOpLower,
    BigOpBody,
    Other
};

/// Classify target relative to the tree rooted at astRoot.
/// Returns Root if target == astRoot.
/// Returns Other if target is null or not found anywhere in the tree.
/// Allocation-free: stack locals only, bounded to depth 16.
CursorTargetRole classifyCursorTargetRow(const NodeRow* astRoot, const NodeRow* target);

/// Lowercase-underscore label string for a role (pointer to string literal).
const char* cursorTargetRoleName(CursorTargetRole role);

} // namespace vpam
