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
#include <cmath>
#include <cstring>
#include <string>

#include "MathSymbols.h"
#include "MathTextNormalization.h"
#include "MathTypography.h"
#include "../math/font/stix_math_variants.h"
#include "../math/font/MathGlyphAssembly.h"

#if defined(NUMOS_MATH_STRESS_DIAGNOSTICS) || defined(NUMOS_MATH_RENDER_TRACE_ONCE)
#include <Arduino.h>
#endif

#ifdef NUMOS_MATH_STRESS_DIAGNOSTICS
#include <esp_timer.h>
#endif

namespace vpam {

namespace {

constexpr int16_t kPlusMinusStrokeDivisor = 10;

/// Render a single assembled (or variant) delimiter glyph at the given position.
/// @param delimCp  The Unicode codepoint of the delimiter to draw (e.g. 0x0028 for '(').
///                 The caller is responsible for choosing left vs right codepoint.
///
/// Falls back to drawing the base glyph at normal size (centered vertically)
/// when the variant table is missing or assembly fails — this guarantees the
/// user sees SOMETHING even if the assembly pieces were omitted from the font
/// subset (e.g. U+239B–U+23A6 missing from lv_font_conv --range arguments).

/// Thin stroke helper (rounded caps) for synthetic delimiters.
static void strokeSeg(lv_layer_t* layer, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, int16_t w, lv_color_t color) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = w < 1 ? 1 : w;
    dsc.opa   = LV_OPA_COVER;
    dsc.round_start = 1;
    dsc.round_end   = 1;
    dsc.p1.x = x1; dsc.p1.y = y1;
    dsc.p2.x = x2; dsc.p2.y = y2;
    lv_draw_line(layer, &dsc);
}

/// Draw a delimiter using vector strokes, scaled to [yTop, yBottom].
///
/// WHY: the NumOS stix_math subset omits the OpenType extensible-delimiter
/// assembly glyphs (U+239B..U+23B3 for parens, brackets, braces, bars). For any
/// content taller than the single bundled size variant (~17px) the assembler
/// selects those missing glyphs, so lv_font_get_glyph_dsc fails and the
/// delimiter renders as nothing at all. This vector fallback reproduces the
/// delimiter at any height with no font dependency, so parentheses (and the
/// other delimiters) are always visible and correctly sized.
static void drawStrokedDelimiter(lv_layer_t* layer, int16_t x,
                                 int16_t yTop, int16_t yBottom,
                                 uint32_t delimCp, lv_color_t color,
                                 int16_t emSizePx) {
    const int16_t H = static_cast<int16_t>(yBottom - yTop);
    if (H <= 0) return;
    // Stroke weight tuned to sit close to the digit/letter weight of the math
    // font (≈ emSize/8) so a synthetic delimiter doesn't read lighter than the
    // glyphs it wraps; clamped to ≥2 px so it never disappears at small sizes.
    const int16_t stroke = std::max<int16_t>(2, static_cast<int16_t>((emSizePx + 4) / 8));
    const int16_t bulge  = std::max<int16_t>(4, std::min<int16_t>(
                               static_cast<int16_t>(H / 5), emSizePx));
    const int16_t footW  = std::max<int16_t>(3, static_cast<int16_t>(emSizePx / 4));
    const int16_t inset  = static_cast<int16_t>(stroke / 2 + 1);

    auto vbar = [&](int16_t vx) { strokeSeg(layer, vx, yTop, vx, yBottom, stroke, color); };

    switch (delimCp) {
        case 0x0028:    // (
        case 0x0029: {  // )
            // Quadratic Bézier: tips at the inner edge (toward content), bulging
            // outward at the vertical middle — a single smooth parabolic arc.
            // A quadratic is inherently hook-free (the endpoint tangent points
            // straight at the lone control), so the tips read clean at every
            // size; a cubic with belly-placed controls flares the tips into
            // little hooks on tall delimiters. Left '(' bulges left; right ')'
            // bulges right. The belly midpoint x = 0.5·xTip + 0.5·xCtrl reaches
            // the box edge, matching the layout's reserved width. The stroke is
            // weight-matched to the font (see `stroke` above) so the arc no
            // longer reads lighter than the glyphs it wraps.
            const bool left = (delimCp == 0x0028);
            const float midY = static_cast<float>(yTop + yBottom) * 0.5f;
            const float xTip  = left ? static_cast<float>(x + bulge)
                                     : static_cast<float>(x);
            const float xCtrl = left ? static_cast<float>(x - bulge)
                                     : static_cast<float>(x + 2 * bulge);
            const int N = 22;
            int16_t prevX = 0, prevY = 0;
            for (int i = 0; i <= N; ++i) {
                const float t  = static_cast<float>(i) / N;
                const float mt = 1.0f - t;
                const float bx = mt * mt * xTip + 2.0f * mt * t * xCtrl + t * t * xTip;
                const float by = mt * mt * static_cast<float>(yTop)
                               + 2.0f * mt * t * midY
                               + t * t * static_cast<float>(yBottom);
                const int16_t px = static_cast<int16_t>(bx + 0.5f);
                const int16_t py = static_cast<int16_t>(by + 0.5f);
                if (i > 0) strokeSeg(layer, prevX, prevY, px, py, stroke, color);
                prevX = px; prevY = py;
            }
            break;
        }
        case 0x005B:    // [
        case 0x005D: {  // ]
            const bool left = (delimCp == 0x005B);
            const int16_t vx = left ? static_cast<int16_t>(x + inset)
                                    : static_cast<int16_t>(x + bulge - inset);
            const int16_t fx = left ? static_cast<int16_t>(vx + footW)
                                    : static_cast<int16_t>(vx - footW);
            vbar(vx);
            strokeSeg(layer, vx, yTop, fx, yTop, stroke, color);
            strokeSeg(layer, vx, yBottom, fx, yBottom, stroke, color);
            break;
        }
        case 0x007C: {  // |
            vbar(static_cast<int16_t>(x + bulge / 2));
            break;
        }
        case 0x007B:    // {
        case 0x007D: {  // }
            const bool left = (delimCp == 0x007B);
            const int16_t midY  = static_cast<int16_t>((yTop + yBottom) / 2);
            const int16_t inner = left ? static_cast<int16_t>(x + bulge) : x;
            const int16_t outer = left ? static_cast<int16_t>(x + inset)
                                       : static_cast<int16_t>(x + bulge - inset);
            const int16_t cusp = left ? x : static_cast<int16_t>(x + bulge);
            const int16_t shoulder = std::max<int16_t>(2,
                static_cast<int16_t>(H / 5));
            const int16_t cuspHalf = std::max<int16_t>(1,
                static_cast<int16_t>(H / 18));
            // WHY: bounded segments form a recognisable scalable brace even
            // when the compact STIX subset has no OpenType brace parts.
            strokeSeg(layer, inner, yTop, outer,
                      static_cast<int16_t>(yTop + shoulder), stroke, color);
            strokeSeg(layer, outer, static_cast<int16_t>(yTop + shoulder),
                      outer, static_cast<int16_t>(midY - cuspHalf), stroke, color);
            strokeSeg(layer, outer, static_cast<int16_t>(midY - cuspHalf),
                      cusp, midY, stroke, color);
            strokeSeg(layer, cusp, midY, outer,
                      static_cast<int16_t>(midY + cuspHalf), stroke, color);
            strokeSeg(layer, outer, static_cast<int16_t>(midY + cuspHalf),
                      outer, static_cast<int16_t>(yBottom - shoulder), stroke, color);
            strokeSeg(layer, outer, static_cast<int16_t>(yBottom - shoulder),
                      inner, yBottom, stroke, color);
            break;
        }
        default:
            vbar(static_cast<int16_t>(x + bulge / 2));
            break;
    }
}

/// True when every glyph the assembly needs is present in `font`.
static bool assemblyGlyphsAvailable(const lv_font_t* font,
                                    const DelimiterAssembler::AssemblyResult& a) {
    for (uint8_t i = 0; i < a.pieceCount; ++i) {
        lv_font_glyph_dsc_t g;
        if (!lv_font_get_glyph_dsc(font, &g, a.pieces[i].glyphCp, 0)) return false;
    }
    return true;
}

static bool drawDelimiterGlyph(lv_layer_t* layer,
                               int16_t x, int16_t yTop, int16_t yBottom,
                               uint32_t delimCp, lv_color_t color,
                               const lv_font_t* font,
                               int16_t emSizePx) {
    if (yBottom <= yTop || font == nullptr) return false;

    const int16_t requestedHeight = static_cast<int16_t>(yBottom - yTop);
    if ((delimCp == 0x007B || delimCp == 0x007D) &&
        requestedHeight > static_cast<int16_t>(font->line_height)) {
        drawStrokedDelimiter(layer, x, yTop, yBottom, delimCp, color, emSizePx);
        return true;
    }

    const MathVariantTable* table = lookupVariantTable(delimCp);
    if (!table) {
        const int16_t totalH = static_cast<int16_t>(yBottom - yTop);
        const bool scalableDelimiter =
            delimCp == 0x0028 || delimCp == 0x0029 ||
            delimCp == 0x005B || delimCp == 0x005D ||
            delimCp == 0x007B || delimCp == 0x007D || delimCp == 0x007C;
        if (scalableDelimiter &&
            totalH > static_cast<int16_t>(font->line_height)) {
            drawStrokedDelimiter(layer, x, yTop, yBottom, delimCp, color,
                                 emSizePx);
            return true;
        }
        // ── Fallback A: no variant table → draw the base codepoint directly ──
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, delimCp, 0)) {
            drawStrokedDelimiter(layer, x, yTop, yBottom, delimCp, color, emSizePx);
            return true;
        }
        lv_draw_letter_dsc_t dsc;
        lv_draw_letter_dsc_init(&dsc);
        dsc.font = font;
        dsc.color = color;
        dsc.opa  = LV_OPA_COVER;
        dsc.unicode = delimCp;
        lv_point_t pos;
        pos.x = static_cast<int32_t>(x);
        // Center the base glyph vertically in the available space
        pos.y = static_cast<int32_t>((yTop + yBottom) / 2 - glyph.ofs_y);
        lv_draw_letter(layer, &dsc, &pos);
        return true;
    }

    const int16_t totalH = static_cast<int16_t>(yBottom - yTop);
    auto assembly = DelimiterAssembler::assemble(totalH, *table, emSizePx);
    if (!assembly.valid || assembly.pieceCount == 0) {
        // ── Fallback B: assembly failed → draw the table's base codepoint ──
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, table->baseCodepoint, 0)) {
            drawStrokedDelimiter(layer, x, yTop, yBottom, delimCp, color, emSizePx);
            return true;
        }
        lv_draw_letter_dsc_t dsc;
        lv_draw_letter_dsc_init(&dsc);
        dsc.font = font;
        dsc.color = color;
        dsc.opa  = LV_OPA_COVER;
        dsc.unicode = table->baseCodepoint;
        lv_point_t pos;
        pos.x = static_cast<int32_t>(x);
        pos.y = static_cast<int32_t>((yTop + yBottom) / 2 - glyph.ofs_y);
        lv_draw_letter(layer, &dsc, &pos);
        return true;
    }

    // ── Fallback C: assembly is valid but the font subset omits one or more of
    // its glyphs (the stix_math subset lacks U+239B..U+23B3). Drawing those
    // pieces would silently render nothing, so draw a vector delimiter instead. ──
    if (!assemblyGlyphsAvailable(font, assembly)) {
        drawStrokedDelimiter(layer, x, yTop, yBottom, delimCp, color, emSizePx);
        return true;
    }

    auto drawPieceAtTop = [&](uint32_t cp, int16_t topY) {
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, cp, 0)) return;

        lv_draw_letter_dsc_t dsc;
        lv_draw_letter_dsc_init(&dsc);
        dsc.font = font;
        dsc.color = color;
        dsc.opa = LV_OPA_COVER;
        dsc.unicode = cp;

        lv_point_t pos;
        pos.x = static_cast<int32_t>(x);
        pos.y = static_cast<int32_t>(topY - glyph.ofs_y);
        lv_draw_letter(layer, &dsc, &pos);
    };

    if (!assembly.isAssembly || assembly.pieceCount == 1) {
        const auto& piece = assembly.pieces[0];
        int16_t pieceTop = yTop;
        if (totalH > piece.heightPx) {
            pieceTop = static_cast<int16_t>(yTop + (totalH - piece.heightPx) / 2);
        }
        drawPieceAtTop(piece.glyphCp, pieceTop);
        return true;
    }

    {
        // WHY: DelimiterAssembler already selected the exact extender count
        // and per-seam overlap. Drawing must not synthesize more extenders.
        int16_t penY = yTop;
        for (uint8_t i = 0; i < assembly.pieceCount; ++i) {
            const auto& piece = assembly.pieces[i];
            drawPieceAtTop(piece.glyphCp, penY);
            penY = static_cast<int16_t>(
                penY + piece.heightPx - piece.overlapAfterPx);
        }
        return true;
    }

}

struct DisplayLimitGeometry {
    int16_t symbolColumnWidth = 0;
    int16_t symbolTop = 0;
    int16_t symbolBottom = 0;
    int16_t upperX = 0;
    int16_t upperBaseline = 0;
    int16_t lowerX = 0;
    int16_t lowerBaseline = 0;
    int16_t bodyX = 0;
};

static DisplayLimitGeometry displayLimitGeometry(
        int16_t x, int16_t yBaseline, int16_t symbolWidth,
        int16_t symbolHeight,
        const LayoutResult& lowerL, const LayoutResult& upperL,
        const LayoutResult& bodyL, const FontMetrics& fm,
        int16_t limitGapPx, int16_t bodyGapPx) {
    const int16_t symbolPadPx = largeOperatorSymbolPadPx(fm);
    const int16_t symbolAscent = largeOperatorGlyphAscentPx(fm, symbolHeight);
    const int16_t symbolDescent = largeOperatorGlyphDescentPx(fm, symbolHeight);

    DisplayLimitGeometry out;
    out.symbolColumnWidth = std::max({symbolWidth, lowerL.width, upperL.width});
    out.symbolTop = static_cast<int16_t>(yBaseline - symbolAscent - symbolPadPx);
    out.symbolBottom = static_cast<int16_t>(yBaseline + symbolDescent + symbolPadPx);
    out.upperX = static_cast<int16_t>(x + (out.symbolColumnWidth - upperL.width) / 2);
    out.upperBaseline = static_cast<int16_t>(out.symbolTop - limitGapPx - upperL.descent);
    out.lowerX = static_cast<int16_t>(x + (out.symbolColumnWidth - lowerL.width) / 2);
    out.lowerBaseline = static_cast<int16_t>(out.symbolBottom + limitGapPx + lowerL.ascent);
    out.bodyX = static_cast<int16_t>(x + out.symbolColumnWidth + bodyGapPx);
    return out;
}

struct InlineLimitGeometry {
    int16_t limitsX = 0;
    int16_t limitsWidth = 0;
    int16_t upperX = 0;
    int16_t upperBaseline = 0;
    int16_t lowerX = 0;
    int16_t lowerBaseline = 0;
    int16_t bodyX = 0;
};

static InlineLimitGeometry inlineLimitGeometry(
        int16_t x, int16_t yBaseline, int16_t symbolWidth,
        const LayoutResult& lowerL, const LayoutResult& upperL,
        const FontMetrics& fm, int16_t bodyGapPx) {
    MathConstantsProvider mc(fm.emSize);
    int16_t supShift = mc.superscriptShiftUp();
    int16_t subDrop = mc.subscriptShiftDown();
    if (supShift < 1) supShift = 1;
    if (subDrop < 2) subDrop = 2;

    InlineLimitGeometry out;
    out.limitsX = static_cast<int16_t>(x + symbolWidth);
    out.limitsWidth = std::max(upperL.width, lowerL.width);
    out.upperX = static_cast<int16_t>(out.limitsX + (out.limitsWidth - upperL.width) / 2);
    out.upperBaseline = static_cast<int16_t>(yBaseline - supShift);
    out.lowerX = static_cast<int16_t>(out.limitsX + (out.limitsWidth - lowerL.width) / 2);
    out.lowerBaseline = static_cast<int16_t>(yBaseline + subDrop);
    out.bodyX = static_cast<int16_t>(out.limitsX + out.limitsWidth + bodyGapPx);
    return out;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

MathCanvas::MathCanvas()
    : _obj(nullptr)
    , _root(nullptr)
    , _cursorCtrl(nullptr)
    , _fontNormal(ui::mathPrimaryFont())
    , _fontSmall(ui::mathScriptFont())
    , _fontScriptScript(ui::mathScriptScriptFont())
    , _cursorTimer(nullptr)
    , _cursorEditable(false)
    , _cursorVisible(true)
    , _cursorX(0), _cursorY(0), _cursorH(0)
    , _scrollX(0)
    , _autoHeightEnabled(true)
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    , _traceLabel("math_canvas")
    , _traceLastRoot(nullptr)
    , _traceLastObjX1(0)
    , _traceLastObjY1(0)
    , _traceLastObjX2(0)
    , _traceLastObjY2(0)
    , _traceLastWidgetW(0)
    , _traceLastWidgetH(0)
    , _traceLastRootW(0)
    , _traceLastRootH(0)
    , _traceLastRootAscent(0)
    , _traceLastRootDescent(0)
    , _traceLastBaseline(0)
    , _traceLastYTop(0)
    , _traceLastDrawX(0)
    , _traceLastScrollX(0)
    , _traceClipSamples(0)
    , _traceSeen(false)
    , _traceCursorSeen(false)
    , _traceCursorEditable(false)
    , _traceCursorBlinkVisible(false)
    , _traceCursorClamped(false)
    , _traceCursorRow(nullptr)
    , _traceCursorIndex(-1)
    , _traceCursorRootAscent(0)
    , _traceCursorRootDescent(0)
    , _traceCursorRowAscent(0)
    , _traceCursorRowDescent(0)
    , _traceCursorRowBaseline(0)
    , _traceCursorRowYTop(0)
    , _traceCursorFmAscent(0)
    , _traceCursorFmDescent(0)
    , _traceCursorFmLineAscent(0)
    , _traceCursorFmLineDescent(0)
    , _traceCursorScriptLevel(0)
    , _traceCursorStyle(0)
    , _traceCursorX(0)
    , _traceCursorY(0)
    , _traceCursorH(0)
    , _traceCursorCanvasX1(0)
    , _traceCursorCanvasY1(0)
    , _traceCursorCanvasX2(0)
    , _traceCursorCanvasY2(0)
#endif
    , _highlightNode(nullptr)
    , _highlightColor(lv_color_black())
    , _highlightActive(false)
{
    _fmNormal = metricsFromFont(_fontNormal);
    _fmSmall  = metricsFromFont(_fontSmall);
    _fmScriptScript = metricsFromFont(_fontScriptScript);
    _fmNormal.scriptLevel = 0;
    _fmSmall.scriptLevel = 1;
    _fmScriptScript.scriptLevel = 2;
    _fmNormal.script = &_fmSmall;
    _fmSmall.script = &_fmScriptScript;
    _fmScriptScript.script = nullptr;

    // Probe once whether the active math font actually carries the extensible
    // delimiter assembly glyphs (U+239C is the parenthesis extender). If absent
    // (the stix_math subset omits them), layout must hug delimiter content and
    // the renderer draws a vector delimiter fallback — see drawStrokedDelimiter.
    if (_fontNormal) {
        lv_font_glyph_dsc_t g;
        g_delimiterAssemblyRenderable =
            lv_font_get_glyph_dsc(_fontNormal, &g, 0x239C, 0);
    }
}

MathCanvas::~MathCanvas() {
    destroy();
}

// ════════════════════════════════════════════════════════════════════════════
// Utilidad estática: FontMetrics desde lv_font_t
// ════════════════════════════════════════════════════════════════════════════

FontMetrics MathCanvas::metricsFromFont(const lv_font_t* font) {
    FontMetrics fm;
    fm.scriptLevel = 0;
    fm.script   = nullptr;
    fm.emSize   = ui::nominalMathEmSizeForFont(font);

    // ── Canonical ascent / descent from LVGL font header ────────────────
    // LVGL line-box metrics only. The visual layout box is narrowed to glyph
    // ink metrics below, while drawText still needs this line ascent.
    fm.lineAscent  = static_cast<int16_t>(font->line_height - font->base_line);
    fm.lineDescent = static_cast<int16_t>(font->base_line);
    fm.ascent      = fm.lineAscent;
    fm.descent     = fm.lineDescent;

    // WHY: Fractions and row boxes must use visible ink, while LVGL still
    // needs the full line box in drawTextBaseline(). This fixed sample is evaluated
    // once per MathCanvas font profile, not inside layout/render loops.
    static constexpr uint32_t kInkMetricSamples[] = {
        static_cast<uint32_t>('0'),
        static_cast<uint32_t>('2'),
        static_cast<uint32_t>('+'),
        static_cast<uint32_t>('-'),
        static_cast<uint32_t>('='),
    };

    int16_t inkAscent = 0;
    int16_t inkDescent = 0;
    bool inkOk = false;
    for (uint32_t cp : kInkMetricSamples) {
        lv_font_glyph_dsc_t glyph;
        if (lv_font_get_glyph_dsc(font, &glyph, cp, 0)) {
            inkAscent = std::max<int16_t>(
                inkAscent,
                glyphInkAscentPx(static_cast<int16_t>(glyph.box_h),
                                  static_cast<int16_t>(glyph.ofs_y)));
            inkDescent = std::max<int16_t>(
                inkDescent,
                glyphInkDescentPx(static_cast<int16_t>(glyph.ofs_y)));
            inkOk = true;
        }
    }
    if (inkOk) {
        fm.ascent = std::max<int16_t>(1, inkAscent);
        fm.descent = inkDescent;
    }

    int16_t numberInkAscent = 0;
    int16_t numberInkDescent = 0;
    bool numberInkOk = false;
    for (uint32_t cp = static_cast<uint32_t>('0');
         cp <= static_cast<uint32_t>('9'); ++cp) {
        lv_font_glyph_dsc_t glyph;
        if (lv_font_get_glyph_dsc(font, &glyph, cp, 0)) {
            numberInkAscent = std::max<int16_t>(
                numberInkAscent,
                glyphInkAscentPx(static_cast<int16_t>(glyph.box_h),
                                  static_cast<int16_t>(glyph.ofs_y)));
            numberInkDescent = std::max<int16_t>(
                numberInkDescent,
                glyphInkDescentPx(static_cast<int16_t>(glyph.ofs_y)));
            numberInkOk = true;
        }
    }
    if (numberInkOk) {
        fm.numberAscent = std::max<int16_t>(1, numberInkAscent);
        fm.numberDescent = numberInkDescent;
    } else {
        fm.numberAscent = fm.ascent;
        fm.numberDescent = fm.descent;
    }

    // ── Cap Height from 'M' glyph (U+004D) ──────────────────────────────
    // Used ONLY for optical positioning (e.g. superscript baseline raise).
    // This does NOT affect the math axis or baseline grid.
    lv_font_glyph_dsc_t capGlyph;
    if (lv_font_get_glyph_dsc(font, &capGlyph, 'M', '\0')) {
        fm.capHeight = glyphInkAscentPx(static_cast<int16_t>(capGlyph.box_h),
                                        static_cast<int16_t>(capGlyph.ofs_y));
        if (fm.capHeight < 1) fm.capHeight = fm.ascent;
    } else {
        fm.capHeight = fm.ascent;
    }

    // ── Character width from '0' glyph (advance width) ──────────────────
    lv_font_glyph_dsc_t widthGlyph;
    bool widthOk = lv_font_get_glyph_dsc(font, &widthGlyph, '0', '1');
    fm.charWidth = widthOk ? static_cast<int16_t>(widthGlyph.adv_w)
                           : static_cast<int16_t>(font->line_height * 6 / 10);
    return fm;
}

// ════════════════════════════════════════════════════════════════════════════
// Crear / Destruir
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::create(lv_obj_t* parent) {
    if (_obj) return;
    if (parent == nullptr || !lv_obj_is_valid(parent)) return;

    // LVGL owns object allocation strategy. MathCanvas does not force heap caps
    // here so small lv_obj metadata can follow the platform allocator policy.
    _obj = lv_obj_create(parent);
    if (_obj == nullptr) return;

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
    _root = nullptr;
    _cursorCtrl = nullptr;
    _cursorEditable = false;
    _cursorX = 0;
    _cursorY = 0;
    _cursorH = 0;
    _scrollX = 0;
    _obj = nullptr;
}

// ════════════════════════════════════════════════════════════════════════════
// Datos y redibujado
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::setExpression(NodeRow* root, const CursorController* ctrl) {
    _root       = root;
    _cursorCtrl = ctrl;
    _cursorEditable = (_cursorCtrl && _cursorCtrl->cursor().isValid());
    _scrollX    = 0;

    if (!_cursorEditable) {
        stopCursorBlink();
    }

    // Recalculate layout and optionally grow the widget to fit the expression.
    if (_obj && _root) {
        _root->calculateLayout(_fmNormal);
        const auto& rl = _root->layout();
        if (_autoHeightEnabled) {
            lv_obj_update_layout(_obj);
            int16_t neededH = mathObjectHeightPx(rl, _fmNormal, VPAM_VERT_PAD);
            int16_t curH    = static_cast<int16_t>(lv_obj_get_height(_obj));
            if (neededH > curH) {
                lv_obj_set_height(_obj, neededH);
            }
        }
    }

    invalidate();
}

void MathCanvas::setAutoHeightEnabled(bool enabled) {
    _autoHeightEnabled = enabled;
}

void MathCanvas::setTraceLabel(const char* label) {
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    _traceLabel = label ? label : "math_canvas";
    _traceSeen = false;
    _traceCursorSeen = false;
    _traceClipSamples = 0;
#else
    (void)label;
#endif
}

void MathCanvas::setMathStyle(MathStyle style) {
    _fmNormal.style = style;
    // WHY: MathCanvas owns the metrics consumed by layout, draw, and cursor
    // geometry, so diagnostics must switch style here instead of forking math.
    if (!_obj || !_root) {
        invalidate();
        return;
    }
    _root->calculateLayout(_fmNormal);
    invalidate();
}

void MathCanvas::invalidate() {
    if (_obj) lv_obj_invalidate(_obj);
}

void MathCanvas::setHighlightNode(const MathNode* node, lv_color_t color) {
    _highlightNode   = node;
    _highlightColor  = color;
    _highlightActive = false;
    invalidate();
}

void MathCanvas::clearHighlightNode() {
    _highlightNode   = nullptr;
    _highlightActive = false;
    invalidate();
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
    if (!isCursorEditable()) {
        stopCursorBlink();
        return;
    }
    if (_cursorTimer) return;   // already running
    _cursorVisible = true;
    // lv_timer fires every BLINK_PERIOD ms and toggles visibility.
    // Much more reliable than lv_anim_t which got reset on every keypress.
    _cursorTimer = lv_timer_create(cursorTimerCb, BLINK_PERIOD, this);
    invalidate();
}

void MathCanvas::stopCursorBlink() {
    if (_cursorTimer) {
        lv_timer_delete(_cursorTimer);
        _cursorTimer = nullptr;
    }
    _cursorVisible = false;
    _cursorH = 0;
    invalidate();
}

void MathCanvas::resetCursorBlink() {
    if (!isCursorEditable()) {
        stopCursorBlink();
        return;
    }
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
    if (!self || !self->isCursorEditable()) {
        if (self) self->stopCursorBlink();
        return;
    }
    self->_cursorVisible = !self->_cursorVisible;
    self->invalidate();
}

bool MathCanvas::isCursorEditable() const {
    return _cursorEditable &&
           _cursorCtrl &&
           _cursorCtrl->cursor().isValid();
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
    int16_t rootBaseline = static_cast<int16_t>(
        static_cast<int16_t>(objArea.y1) + (widgetH + rootL.ascent - rootL.descent) / 2);
    int16_t rootYTop = layoutTopFromBaseline(rootL, rootBaseline);

#ifdef NUMOS_MATH_STRESS_DIAGNOSTICS
    const int64_t drawStartUs = esp_timer_get_time();
#endif

    // Calcular posición del cursor ANTES de dibujar
    const bool cursorActive = isCursorEditable();
    if (cursorActive) {
        computeCursorPosition(baseX, rootBaseline, objArea, rootL, rootYTop);
    }
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    else {
        traceCursorState(rootL, nullptr, -1, nullptr, rootBaseline, rootYTop,
                         _fmNormal, objArea, false);
    }
#endif

    // Auto-scroll horizontal: mantener el cursor visible
    if (cursorActive) {
        int16_t visLeft  = static_cast<int16_t>(objArea.x1) + PADDING_LEFT;
        int16_t visRight = static_cast<int16_t>(objArea.x2) - PADDING_RIGHT;

        if (_cursorX < visLeft) {
            _scrollX += (visLeft - _cursorX + 4);
            baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT + _scrollX;
            computeCursorPosition(baseX, rootBaseline, objArea, rootL, rootYTop);
        } else if (_cursorX > visRight) {
            _scrollX -= (_cursorX - visRight + 4);
            baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT + _scrollX;
            computeCursorPosition(baseX, rootBaseline, objArea, rootL, rootYTop);
        }

        // No dejar que el scroll sea positivo (expresión se sale por la izquierda)
        if (_scrollX > 0) {
            _scrollX = 0;
            baseX = static_cast<int16_t>(objArea.x1) + PADDING_LEFT;
            computeCursorPosition(baseX, rootBaseline, objArea, rootL, rootYTop);
        }
    }

    // Dibujar la expresión recursivamente usando el layout ya calculado arriba.
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
    {
        const int16_t rootH = rootL.height();
        lv_area_t clipArea = {0, 0, 0, 0};
        if (layer) {
            clipArea = layer->_clip_area;
        }
        const bool traceChanged =
            !_traceSeen ||
            _traceLastRoot != _root ||
            _traceLastObjX1 != objArea.x1 ||
            _traceLastObjY1 != objArea.y1 ||
            _traceLastObjX2 != objArea.x2 ||
            _traceLastObjY2 != objArea.y2 ||
            _traceLastWidgetW != widgetW ||
            _traceLastWidgetH != widgetH ||
            _traceLastRootW != rootL.width ||
            _traceLastRootH != rootH ||
            _traceLastRootAscent != rootL.ascent ||
            _traceLastRootDescent != rootL.descent ||
            _traceLastBaseline != rootBaseline ||
            _traceLastYTop != rootYTop ||
            _traceLastDrawX != baseX ||
            _traceLastScrollX != _scrollX;

        if (traceChanged) {
            const int16_t lineBoxTop = static_cast<int16_t>(rootBaseline - _fmNormal.lineAscent);
            const int16_t lineBoxBottom = static_cast<int16_t>(rootBaseline + _fmNormal.lineDescent);

            Serial.printf(
                "[MATH-TRACE] role=%s canvas=%p root=%p obj=(%d,%d,%d,%d) clip=(%d,%d,%d,%d) "
                "clipSamples=%u widget=(%d,%d) root=(w=%d,h=%d,asc=%d,desc=%d) "
                "fm=(asc=%d,desc=%d,lineAsc=%d,lineDesc=%d,axis=%d) "
                "rootBaseline=%d rootYTop=%d draw=(x=%d,y=%d) scrollX=%d "
                "coordSpace=absolute lineBox=(top=%d,bottom=%d) "
                "objBounds=(top=%d,bottom=%d) clipBounds=(top=%d,bottom=%d)\n",
                _traceLabel ? _traceLabel : "math_canvas",
                static_cast<void*>(this),
                static_cast<void*>(_root),
                static_cast<int>(objArea.x1),
                static_cast<int>(objArea.y1),
                static_cast<int>(objArea.x2),
                static_cast<int>(objArea.y2),
                static_cast<int>(clipArea.x1),
                static_cast<int>(clipArea.y1),
                static_cast<int>(clipArea.x2),
                static_cast<int>(clipArea.y2),
                1u,
                static_cast<int>(widgetW),
                static_cast<int>(widgetH),
                static_cast<int>(rootL.width),
                static_cast<int>(rootH),
                static_cast<int>(rootL.ascent),
                static_cast<int>(rootL.descent),
                static_cast<int>(_fmNormal.ascent),
                static_cast<int>(_fmNormal.descent),
                static_cast<int>(_fmNormal.lineAscent),
                static_cast<int>(_fmNormal.lineDescent),
                static_cast<int>(_fmNormal.axisHeight()),
                static_cast<int>(rootBaseline),
                static_cast<int>(rootYTop),
                static_cast<int>(baseX),
                static_cast<int>(rootYTop),
                static_cast<int>(_scrollX),
                static_cast<int>(lineBoxTop),
                static_cast<int>(lineBoxBottom),
                static_cast<int>(objArea.y1),
                static_cast<int>(objArea.y2),
                static_cast<int>(clipArea.y1),
                static_cast<int>(clipArea.y2));

            _traceLastRoot = _root;
            _traceLastObjX1 = static_cast<int16_t>(objArea.x1);
            _traceLastObjY1 = static_cast<int16_t>(objArea.y1);
            _traceLastObjX2 = static_cast<int16_t>(objArea.x2);
            _traceLastObjY2 = static_cast<int16_t>(objArea.y2);
            _traceLastWidgetW = widgetW;
            _traceLastWidgetH = widgetH;
            _traceLastRootW = rootL.width;
            _traceLastRootH = rootH;
            _traceLastRootAscent = rootL.ascent;
            _traceLastRootDescent = rootL.descent;
            _traceLastBaseline = rootBaseline;
            _traceLastYTop = rootYTop;
            _traceLastDrawX = baseX;
            _traceLastScrollX = _scrollX;
            _traceClipSamples = 1;
            _traceSeen = true;
        } else {
            ++_traceClipSamples;
        }
    }
#endif

    drawNodeWithLayout(layer, _root, baseX, rootYTop, _fmNormal, _fontNormal);

    // Dibujar el cursor parpadeante
    drawCursor(layer);

#ifdef NUMOS_MATH_STRESS_DIAGNOSTICS
    const int64_t drawEndUs = esp_timer_get_time();
    Serial.printf("[MATH-STRESS] draw_us=%lld root_w=%d root_h=%d style=%u\n",
                  static_cast<long long>(drawEndUs - drawStartUs),
                  static_cast<int>(rootL.width),
                  static_cast<int>(rootL.height()),
                  static_cast<unsigned>(_fmNormal.style));
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// Cálculo de posición del cursor
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::computeCursorPosition(int16_t baseX, int16_t rootBaseline,
                                       const lv_area_t& objArea,
                                       const LayoutResult& rootLayout,
                                       int16_t rootYTop) {
    if (!isCursorEditable()) {
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
        traceCursorState(rootLayout, nullptr, -1, nullptr, rootBaseline,
                         rootYTop, _fmNormal, objArea, false);
#endif
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
                // Buscar dentro de los hijos del row (match drawRow spacing)
                int16_t cx = x;
                MathClass prevRight = MathClass::ORD;
                bool hasPrev = false;
                for (int i = 0; i < row->childCount(); ++i) {
                    MathNode* child = row->child(i);
                    const auto& childL = child->layout();
                    if (hasPrev) {
                        cx = static_cast<int16_t>(
                            cx + interAtomSpacingPx(prevRight, child->leftMathClass(), fm.style, fm.emSize));
                    }
                    search(child, cx, yBaseline, fm, fmSmall);
                    if (result.found) return;
                    cx += childL.width;
                    prevRight = child->rightMathClass();
                    hasPrev = true;
                }
            }
            else if (node->type() == NodeType::Fraction) {
                auto* frac = static_cast<const NodeFraction*>(node);
                const auto& fracL = frac->layout();
                const auto& numL = frac->numerator()->layout();
                const auto& denL = frac->denominator()->layout();

                const int16_t barHalfUp = frac->barHalfUpPx();
                const int16_t barHalfDown = frac->barHalfDownPx();
                const int16_t numShift = frac->numeratorShiftPx();
                const int16_t denShift = frac->denominatorShiftPx();
                const int16_t ruleOverhang = frac->ruleOverhangPx();

                int16_t axis = fm.axisHeight();
                int16_t yAxis = static_cast<int16_t>(yBaseline - axis);

                // Child metrics step down to match layout/draw (TeX style).
                const FontMetrics childFm = fractionPartMetrics(fm);
                const FontMetrics childSmall = childFm.superscript();

                // Numerador: baseline = yAxis - barHalfUp - numShift
                int16_t numX = static_cast<int16_t>(x + ruleOverhang +
                               (fracL.width - 2 * ruleOverhang - numL.width) / 2);
                int16_t numY = static_cast<int16_t>(yAxis - barHalfUp - numShift);
                search(frac->numerator(), numX, numY, childFm, childSmall);
                if (result.found) return;

                // Denominador: baseline = yAxis + barHalfDown + denShift
                int16_t denX = static_cast<int16_t>(x + ruleOverhang +
                               (fracL.width - 2 * ruleOverhang - denL.width) / 2);
                int16_t denY = static_cast<int16_t>(yAxis + barHalfDown + denShift);
                search(frac->denominator(), denX, denY, childFm, childSmall);
            }
            else if (node->type() == NodeType::Power) {
                auto* pow = static_cast<const NodePower*>(node);
                const auto& baseL = pow->base()->layout();

                // Base
                search(pow->base(), x, yBaseline, fm, fmSmall);
                if (result.found) return;

                // Exponente (fuente reducida)
                FontMetrics fmSup = fm.superscript();
                MathConstantsProvider mc(fm.emSize);
                const auto& expL = pow->exponent()->layout();
                int16_t supShiftUp = mc.superscriptShiftUp();
                int16_t supBottomMin = mc.superscriptBottomMin();
                int16_t expShift = std::max(supShiftUp,
                                            static_cast<int16_t>(supBottomMin + expL.descent));
                if (expShift < 1) expShift = 1;
                int16_t expX = static_cast<int16_t>(x + baseL.width + pow->italicCorrectionPx());
                int16_t expBaseline = static_cast<int16_t>(yBaseline - expShift);

                search(pow->exponent(), expX, expBaseline, fmSup,
                       fmSup.superscript());
            }
            else if (node->type() == NodeType::Root) {
                auto* root = static_cast<const NodeRoot*>(node);
                int16_t radSymW = static_cast<int16_t>(
                    NodeRoot::RADICAL_HOOK_W + NodeRoot::RADICAL_SLOPE_W);

                // Radicand
                int16_t radX = static_cast<int16_t>(x + radSymW);
                search(root->radicand(), radX, yBaseline, fm, fmSmall);
                if (result.found) return;

                // Degree (if present)
                if (root->hasDegree()) {
                    FontMetrics fmDeg = fm.superscript();
                    const auto& degL  = root->degree()->layout();
                    int16_t degX = static_cast<int16_t>(x + root->radicalKernBefore());
                    int16_t degBaseline = static_cast<int16_t>(yBaseline -
                                          root->layout().ascent + degL.ascent);
                    search(root->degree(), degX, degBaseline, fmDeg,
                           fmDeg.superscript());
                }
            }
            else if (node->type() == NodeType::Paren) {
                auto* paren = static_cast<const NodeParen*>(node);
                int16_t pw = paren->parenWidth();
                int16_t innerPad = std::max<int16_t>(1, pw / 3);
                int16_t contentX = static_cast<int16_t>(x + pw + innerPad);
                search(paren->content(), contentX, yBaseline, fm, fmSmall);
            }
            else if (node->type() == NodeType::Function) {
                auto* func = static_cast<const NodeFunction*>(node);
                int16_t contentX = static_cast<int16_t>(x + func->labelWidth()
                                   + NodeFunction::LABEL_GAP
                                   + func->parenWidth() + func->innerPad());
                search(func->argument(), contentX, yBaseline, fm, fmSmall);
            }
            else if (node->type() == NodeType::LogBase) {
                auto* lb = static_cast<const NodeLogBase*>(node);
                const auto& baseL = lb->base()->layout();

                // Base (subíndice) → fuente reducida, bajada
                FontMetrics fmSub = fm.superscript();
                int16_t subDrop = MathConstantsProvider(fm.emSize).subscriptShiftDown();
                if (subDrop < 2) subDrop = 2;
                int16_t baseX = static_cast<int16_t>(x + lb->labelWidth());
                int16_t baseBaseline = static_cast<int16_t>(yBaseline + subDrop);
                search(lb->base(), baseX, baseBaseline, fmSub, fmSub.superscript());
                if (result.found) return;

                // Argumento (dentro de paréntesis)
                // Mirror layout: labelWidth + baseL.width + SpaceAfterScript + LABEL_GAP
                int16_t argX = static_cast<int16_t>(x + lb->labelWidth() + baseL.width
                               + spaceAfterScriptPx(fm) + NodeLogBase::LABEL_GAP
                               + lb->parenWidth() + lb->innerPad());
                search(lb->argument(), argX, yBaseline, fm, fmSmall);
            }
            else if (node->type() == NodeType::DefIntegral) {
                auto* di = static_cast<const NodeDefIntegral*>(node);
                const auto& lowerL = di->lower()->layout();
                const auto& upperL = di->upper()->layout();
                const auto& bodyL  = di->body()->layout();

                FontMetrics fmLim = fm.superscript();
                const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
                    0x222B, largeOperatorTargetHeightPx(fm), fm.emSize);
                int16_t symW = symMetrics.valid ? symMetrics.widthPx : 0;
                int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();
                if (symW < 6) symW = std::max<int16_t>(6, fm.charWidth);

                const int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
                const int16_t bodyGapPx = largeOperatorBodyGapPx(fm);
                const int16_t dGapPx = integralDifferentialGapPx(fm);

                if (shouldUseDisplayLimits(fm, symH)) {
                    const DisplayLimitGeometry geom = displayLimitGeometry(
                        x, yBaseline, symW, symH, lowerL, upperL, bodyL,
                        fm, limitGapPx, bodyGapPx);

                    search(di->upper(), geom.upperX, geom.upperBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(di->lower(), geom.lowerX, geom.lowerBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(di->body(), geom.bodyX, yBaseline, fm, fmSmall);
                    if (result.found) return;

                    int16_t varX = static_cast<int16_t>(geom.bodyX + bodyL.width
                                  + dGapPx + fm.charWidth + dGapPx);
                    search(di->variable(), varX, yBaseline, fm, fmSmall);
                } else {
                    const InlineLimitGeometry geom = inlineLimitGeometry(
                        x, yBaseline, symW, lowerL, upperL, fm, bodyGapPx);

                    search(di->upper(), geom.upperX, geom.upperBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;
                    search(di->lower(), geom.lowerX, geom.lowerBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(di->body(), geom.bodyX, yBaseline, fm, fmSmall);
                    if (result.found) return;

                    int16_t varX = static_cast<int16_t>(geom.bodyX + bodyL.width
                                  + dGapPx + fm.charWidth + dGapPx);
                    search(di->variable(), varX, yBaseline, fm, fmSmall);
                }
            }
            else if (node->type() == NodeType::Summation) {
                auto* sm = static_cast<const NodeSummation*>(node);
                const auto& lowerL = sm->lower()->layout();
                const auto& upperL = sm->upper()->layout();
                const auto& bodyL  = sm->body()->layout();

                FontMetrics fmLim = fm.superscript();
                const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
                    0x2211, largeOperatorTargetHeightPx(fm), fm.emSize);
                int16_t symW = symMetrics.valid ? symMetrics.widthPx : 0;
                int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();
                if (symW < 6) symW = std::max<int16_t>(6, fm.charWidth);

                const int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
                const int16_t bodyGapPx = largeOperatorBodyGapPx(fm);

                if (shouldUseDisplayLimits(fm, symH)) {
                    const DisplayLimitGeometry geom = displayLimitGeometry(
                        x, yBaseline, symW, symH, lowerL, upperL, bodyL,
                        fm, limitGapPx, bodyGapPx);

                    search(sm->upper(), geom.upperX, geom.upperBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(sm->lower(), geom.lowerX, geom.lowerBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(sm->body(), geom.bodyX, yBaseline, fm, fmSmall);
                } else {
                    const InlineLimitGeometry geom = inlineLimitGeometry(
                        x, yBaseline, symW, lowerL, upperL, fm, bodyGapPx);

                    search(sm->upper(), geom.upperX, geom.upperBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;
                    search(sm->lower(), geom.lowerX, geom.lowerBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(sm->body(), geom.bodyX, yBaseline, fm, fmSmall);
                }
            }
            else if (node->type() == NodeType::Subscript) {
                auto* sub = static_cast<const NodeSubscript*>(node);
                const auto& baseL = sub->base()->layout();

                // Base (fuente normal)
                search(sub->base(), x, yBaseline, fm, fmSmall);
                if (result.found) return;

                // Subscript (fuente reducida, bajada)
                FontMetrics fmSub = fm.superscript();
                int16_t subDrop = MathConstantsProvider(fm.emSize).subscriptShiftDown();
                if (subDrop < 2) subDrop = 2;
                int16_t subX = static_cast<int16_t>(x + baseL.width);
                int16_t subBaseline = static_cast<int16_t>(yBaseline + subDrop);
                search(sub->subscript(), subX, subBaseline, fmSub, fmSub.superscript());
            }
            else if (node->type() == NodeType::BigOp) {
                auto* bo = static_cast<const NodeBigOp*>(node);
                const auto& lowerL = bo->lower()->layout();
                const auto& upperL = bo->upper()->layout();
                const auto& bodyL  = bo->body()->layout();

                FontMetrics fmLim = fm.superscript();
                const auto opMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
                    bo->operatorCodepoint(), largeOperatorTargetHeightPx(fm), fm.emSize);
                int16_t opW = opMetrics.valid ? opMetrics.widthPx : 0;
                int16_t opH = opMetrics.valid ? opMetrics.heightPx : fm.height();
                if (opW < 6) opW = std::max<int16_t>(6, fm.charWidth);

                int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
                int16_t bodyGapX = largeOperatorBodyGapPx(fm);

                if (bo->useDisplayLimits()) {
                    const DisplayLimitGeometry geom = displayLimitGeometry(
                        x, yBaseline, opW, opH, lowerL, upperL, bodyL,
                        fm, limitGapPx, bodyGapX);

                    search(bo->upper(), geom.upperX, geom.upperBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(bo->lower(), geom.lowerX, geom.lowerBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    search(bo->body(), geom.bodyX, yBaseline, fm, fmSmall);
                } else {
                    // TEXT/SCRIPT: limits as sub/superscript to the right
                    MathConstantsProvider mcT2(fm.emSize);
                    int16_t supShift = mcT2.superscriptShiftUp();
                    int16_t subDrop  = mcT2.subscriptShiftDown();
                    if (subDrop < 2) subDrop = 2;
                    if (supShift < 1) supShift = 1;

                    int16_t limitsX = static_cast<int16_t>(x + opW);
                    int16_t upperBaseline = static_cast<int16_t>(yBaseline - supShift);
                    int16_t lowerBaseline = static_cast<int16_t>(yBaseline + subDrop);

                    int16_t limitsW = std::max(upperL.width, lowerL.width);
                    int16_t upperLimitX = static_cast<int16_t>(limitsX + (limitsW - upperL.width) / 2);
                    int16_t lowerLimitX = static_cast<int16_t>(limitsX + (limitsW - lowerL.width) / 2);

                    search(bo->upper(), upperLimitX, upperBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;
                    search(bo->lower(), lowerLimitX, lowerBaseline, fmLim, fmLim.superscript());
                    if (result.found) return;

                    int16_t bodyX = static_cast<int16_t>(limitsX + limitsW + bodyGapX);
                    search(bo->body(), bodyX, yBaseline, fm, fmSmall);
                }
            }
        }
    };

    Finder finder;
    finder.target = cur.row;
    finder.result = { false, 0, 0, _fmNormal };
    finder.search(_root, baseX, rootBaseline, _fmNormal, _fmSmall);

    if (finder.result.found) {
        int16_t offsetX = childXOffset(cur.row, cur.index, finder.result.fm);
        _cursorX = static_cast<int16_t>(finder.result.x + offsetX);
        const auto& rowL = cur.row->layout();
        int16_t cursorTop = static_cast<int16_t>(
            finder.result.yBaseline - finder.result.fm.ascent - CURSOR_PAD);
        int16_t cursorBottom = static_cast<int16_t>(
            finder.result.yBaseline + finder.result.fm.descent + CURSOR_PAD);

        bool clamped = false;
        const int16_t minX = static_cast<int16_t>(objArea.x1);
        int16_t maxX = static_cast<int16_t>(objArea.x2 - CURSOR_WIDTH + 1);
        if (maxX < minX) maxX = minX;
        if (_cursorX < minX) {
            _cursorX = minX;
            clamped = true;
        } else if (_cursorX > maxX) {
            _cursorX = maxX;
            clamped = true;
        }
        if (cursorTop < objArea.y1) {
            cursorTop = static_cast<int16_t>(objArea.y1);
            clamped = true;
        }
        if (cursorBottom > objArea.y2) {
            cursorBottom = static_cast<int16_t>(objArea.y2);
            clamped = true;
        }
        if (cursorBottom < cursorTop) {
            cursorBottom = cursorTop;
            clamped = true;
        }

        _cursorY = cursorTop;
        _cursorH = static_cast<int16_t>(cursorBottom - cursorTop + 1);
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
        traceCursorState(rootLayout, cur.row, static_cast<int16_t>(cur.index),
                         &rowL, finder.result.yBaseline,
                         layoutTopFromBaseline(rowL, finder.result.yBaseline),
                         finder.result.fm, objArea, clamped);
#endif
    } else {
        // Fallback: inicio de la expresión
        _cursorX = baseX;
        int16_t cursorTop = static_cast<int16_t>(
            rootBaseline - _fmNormal.ascent - CURSOR_PAD);
        int16_t cursorBottom = static_cast<int16_t>(
            rootBaseline + _fmNormal.descent + CURSOR_PAD);
        bool clamped = false;
        const int16_t minX = static_cast<int16_t>(objArea.x1);
        int16_t maxX = static_cast<int16_t>(objArea.x2 - CURSOR_WIDTH + 1);
        if (maxX < minX) maxX = minX;
        if (_cursorX < minX) {
            _cursorX = minX;
            clamped = true;
        } else if (_cursorX > maxX) {
            _cursorX = maxX;
            clamped = true;
        }
        if (cursorTop < objArea.y1) {
            cursorTop = static_cast<int16_t>(objArea.y1);
            clamped = true;
        }
        if (cursorBottom > objArea.y2) {
            cursorBottom = static_cast<int16_t>(objArea.y2);
            clamped = true;
        }
        if (cursorBottom < cursorTop) {
            cursorBottom = cursorTop;
            clamped = true;
        }
        _cursorY = cursorTop;
        _cursorH = static_cast<int16_t>(cursorBottom - cursorTop + 1);
#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
        traceCursorState(rootLayout, nullptr, static_cast<int16_t>(cur.index),
                         nullptr, rootBaseline, rootYTop, _fmNormal, objArea,
                         clamped);
#endif
    }
}

#if defined(NUMOS_MATH_RENDER_TRACE_ONCE)
void MathCanvas::traceCursorState(const LayoutResult& rootLayout,
                                  const NodeRow* targetRow,
                                  int16_t targetIndex,
                                  const LayoutResult* targetLayout,
                                  int16_t rowBaseline,
                                  int16_t rowYTop,
                                  const FontMetrics& cursorFm,
                                  const lv_area_t& objArea,
                                  bool clamped) {
    const int16_t rowAsc = targetLayout ? targetLayout->ascent : 0;
    const int16_t rowDesc = targetLayout ? targetLayout->descent : 0;
    const uint8_t style = static_cast<uint8_t>(cursorFm.style);
    const bool changed =
        !_traceCursorSeen ||
        _traceCursorEditable != isCursorEditable() ||
        _traceCursorClamped != clamped ||
        _traceCursorRow != targetRow ||
        _traceCursorIndex != targetIndex ||
        _traceCursorRootAscent != rootLayout.ascent ||
        _traceCursorRootDescent != rootLayout.descent ||
        _traceCursorRowAscent != rowAsc ||
        _traceCursorRowDescent != rowDesc ||
        _traceCursorRowBaseline != rowBaseline ||
        _traceCursorRowYTop != rowYTop ||
        _traceCursorFmAscent != cursorFm.ascent ||
        _traceCursorFmDescent != cursorFm.descent ||
        _traceCursorFmLineAscent != cursorFm.lineAscent ||
        _traceCursorFmLineDescent != cursorFm.lineDescent ||
        _traceCursorScriptLevel != cursorFm.scriptLevel ||
        _traceCursorStyle != style ||
        _traceCursorX != _cursorX ||
        _traceCursorY != _cursorY ||
        _traceCursorH != _cursorH ||
        _traceCursorCanvasX1 != objArea.x1 ||
        _traceCursorCanvasY1 != objArea.y1 ||
        _traceCursorCanvasX2 != objArea.x2 ||
        _traceCursorCanvasY2 != objArea.y2;

    if (!changed) return;

    const char* role = _traceLabel ? _traceLabel : "math_canvas";
    const char* state = "edit";
    if (std::strstr(role, "math_visual")) {
        state = "math_visual";
    } else if (std::strstr(role, "result")) {
        state = "result";
    } else if (!isCursorEditable()) {
        state = "readonly";
    }

    const CursorTargetRole targetRole = targetRow
        ? classifyCursorTargetRow(
              static_cast<const NodeRow*>(_cursorCtrl->rootRow()), targetRow)
        : CursorTargetRole::Other;
    const char* targetRoleName = cursorTargetRoleName(targetRole);

    Serial.printf(
        "[MATH_CURSOR] role=%s target=%s state=%s editable=%s blinkVisible=%s "
        "targetRow=%p targetIndex=%d root=(asc=%d,desc=%d,h=%d) "
        "row=(asc=%d,desc=%d,h=%d) rowBaseline=%d rowYTop=%d "
        "fm=(asc=%d,desc=%d,lineAsc=%d,lineDesc=%d,script=%u,style=%u) "
        "cursor=(x=%d,top=%d,bottom=%d,h=%d) "
        "canvas=(%d,%d,%d,%d) clamped=%s\n",
        role,
        targetRoleName,
        state,
        isCursorEditable() ? "yes" : "no",
        _cursorVisible ? "yes" : "no",
        static_cast<const void*>(targetRow),
        static_cast<int>(targetIndex),
        static_cast<int>(rootLayout.ascent),
        static_cast<int>(rootLayout.descent),
        static_cast<int>(rootLayout.height()),
        static_cast<int>(rowAsc),
        static_cast<int>(rowDesc),
        static_cast<int>(rowAsc + rowDesc),
        static_cast<int>(rowBaseline),
        static_cast<int>(rowYTop),
        static_cast<int>(cursorFm.ascent),
        static_cast<int>(cursorFm.descent),
        static_cast<int>(cursorFm.lineAscent),
        static_cast<int>(cursorFm.lineDescent),
        static_cast<unsigned>(cursorFm.scriptLevel),
        static_cast<unsigned>(style),
        static_cast<int>(_cursorX),
        static_cast<int>(_cursorY),
        static_cast<int>(_cursorY + _cursorH - 1),
        static_cast<int>(_cursorH),
        static_cast<int>(objArea.x1),
        static_cast<int>(objArea.y1),
        static_cast<int>(objArea.x2),
        static_cast<int>(objArea.y2),
        clamped ? "yes" : "no");

    _traceCursorSeen = true;
    _traceCursorEditable = isCursorEditable();
    _traceCursorBlinkVisible = _cursorVisible;
    _traceCursorClamped = clamped;
    _traceCursorRow = targetRow;
    _traceCursorIndex = targetIndex;
    _traceCursorRootAscent = rootLayout.ascent;
    _traceCursorRootDescent = rootLayout.descent;
    _traceCursorRowAscent = rowAsc;
    _traceCursorRowDescent = rowDesc;
    _traceCursorRowBaseline = rowBaseline;
    _traceCursorRowYTop = rowYTop;
    _traceCursorFmAscent = cursorFm.ascent;
    _traceCursorFmDescent = cursorFm.descent;
    _traceCursorFmLineAscent = cursorFm.lineAscent;
    _traceCursorFmLineDescent = cursorFm.lineDescent;
    _traceCursorScriptLevel = cursorFm.scriptLevel;
    _traceCursorStyle = style;
    _traceCursorX = _cursorX;
    _traceCursorY = _cursorY;
    _traceCursorH = _cursorH;
    _traceCursorCanvasX1 = static_cast<int16_t>(objArea.x1);
    _traceCursorCanvasY1 = static_cast<int16_t>(objArea.y1);
    _traceCursorCanvasX2 = static_cast<int16_t>(objArea.x2);
    _traceCursorCanvasY2 = static_cast<int16_t>(objArea.y2);
}
#endif

int16_t MathCanvas::childXOffset(const NodeRow* row, int index,
                                  const FontMetrics& fm) const {
    int16_t offset = 0;
    int count = std::min(index, row->childCount());
    MathClass prevRight = MathClass::ORD;
    bool hasPrev = false;
    for (int i = 0; i < count; ++i) {
        MathNode* child = row->child(i);
        if (hasPrev) {
            offset = static_cast<int16_t>(
                offset + interAtomSpacingPx(prevRight, child->leftMathClass(), fm.style, fm.emSize));
        }
        offset += child->layout().width;
        prevRight = child->rightMathClass();
        hasPrev = true;
    }
    // Añadir gap antes del cursor si no está al inicio y hay más nodos
    if (count > 0 && count < row->childCount()) {
        MathNode* nextChild = row->child(count);
        offset = static_cast<int16_t>(
            offset + interAtomSpacingPx(prevRight, nextChild->leftMathClass(), fm.style, fm.emSize));
    }
    return offset;
}

// ════════════════════════════════════════════════════════════════════════════
// Motor de dibujo recursivo
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawNode(lv_layer_t* layer, const MathNode* node,
                          int16_t x, int16_t yTop,
                          const FontMetrics& fm, const lv_font_t* font,
                          int depth) {
    if (!node) return;

    // WHY: External callers enter with yTop and may not have a fresh layout.
    // Compute layout once for this subtree, then keep render recursion on the
    // cached-layout path to avoid repeated layout work on ESP32-S3 frames.
    MathNode* mutableNode = const_cast<MathNode*>(node);
    mutableNode->calculateLayout(fm);
    drawNodeWithLayout(layer, node, x, yTop, fm, font, depth);
}

void MathCanvas::drawNodeWithLayout(lv_layer_t* layer, const MathNode* node,
                                    int16_t x, int16_t yTop,
                                    const FontMetrics& fm,
                                    const lv_font_t* font,
                                    int depth) {
    if (!node) return;

    // WHY: The recursive renderer contract is top-of-bounding-box. Existing
    // specialized draw methods remain baseline-oriented, so this is the
    // top -> baseline conversion boundary after layout is already available.
    const int16_t yBaseline = layoutBaselineFromTop(node->layout(), yTop);
    drawNodeBaseline(layer, node, x, yBaseline, fm, font, depth);
}

void MathCanvas::drawNodeBaseline(lv_layer_t* layer, const MathNode* node,
                          int16_t x, int16_t yBaseline,
                          const FontMetrics& fm, const lv_font_t* font,
                          int depth) {
    if (!node) return;
    if (fm.scriptLevel >= 2) {
        font = _fontScriptScript;
    } else if (fm.scriptLevel == 1) {
        font = _fontSmall;
    } else {
        font = _fontNormal;
    }
    if (depth > MAX_RENDER_DEPTH) {
        // Draw a red overflow indicator instead of recursing further
        drawFilledRect(layer, x, static_cast<int16_t>(yBaseline - fm.ascent),
                       static_cast<int16_t>(fm.charWidth * 3), fm.height(),
                       lv_color_hex(0xFF0000), LV_OPA_60);
        drawTextBaseline(layer, x, yBaseline, "...", fm.scriptLevel, lv_color_hex(0xFF0000));
        return;
    }

    // ── Smart Highlighter: activate colour override when entering highlighted sub-tree ──
    bool wasHighlight = _highlightActive;
    if (_highlightNode && node == _highlightNode) _highlightActive = true;

    switch (node->type()) {
        case NodeType::Row:
            drawRowBaseline(layer, static_cast<const NodeRow*>(node),
                    x, yBaseline, fm, font, depth);
            break;
        case NodeType::Number:
            drawNumberBaseline(layer, static_cast<const NodeNumber*>(node),
                       x, yBaseline, fm, font);
            break;
        case NodeType::Operator:
            drawOperatorBaseline(layer, static_cast<const NodeOperator*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::Empty:
            drawEmptyBaseline(layer, static_cast<const NodeEmpty*>(node),
                      x, yBaseline, fm);
            break;
        case NodeType::Fraction:
            drawFractionBaseline(layer, static_cast<const NodeFraction*>(node),
                         x, yBaseline, fm, font, depth);
            break;
        case NodeType::Power:
            drawPowerBaseline(layer, static_cast<const NodePower*>(node),
                      x, yBaseline, fm, font, depth);
            break;
        case NodeType::Root:
            drawRootBaseline(layer, static_cast<const NodeRoot*>(node),
                     x, yBaseline, fm, font, depth);
            break;
        case NodeType::Paren:
            drawParenBaseline(layer, static_cast<const NodeParen*>(node),
                      x, yBaseline, fm, font, depth);
            break;
        case NodeType::Function:
            drawFunctionBaseline(layer, static_cast<const NodeFunction*>(node),
                         x, yBaseline, fm, font, depth);
            break;
        case NodeType::LogBase:
            drawLogBaseBaseline(layer, static_cast<const NodeLogBase*>(node),
                        x, yBaseline, fm, font, depth);
            break;
        case NodeType::Constant:
            drawConstantBaseline(layer, static_cast<const NodeConstant*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::Variable:
            drawVariableBaseline(layer, static_cast<const NodeVariable*>(node),
                         x, yBaseline, fm, font);
            break;
        case NodeType::PeriodicDecimal:
            drawPeriodicDecimalBaseline(layer, static_cast<const NodePeriodicDecimal*>(node),
                                x, yBaseline, fm, font);
            break;
        case NodeType::DefIntegral:
            drawDefIntegralBaseline(layer, static_cast<const NodeDefIntegral*>(node),
                            x, yBaseline, fm, font, depth);
            break;
        case NodeType::Summation:
            drawSummationBaseline(layer, static_cast<const NodeSummation*>(node),
                          x, yBaseline, fm, font, depth);
            break;
        case NodeType::Subscript:
            drawSubscriptBaseline(layer, static_cast<const NodeSubscript*>(node),
                          x, yBaseline, fm, font, depth);
            break;
        case NodeType::BigOp:
            drawBigOpBaseline(layer, static_cast<const NodeBigOp*>(node),
                      x, yBaseline, fm, font, depth);
            break;
        case NodeType::Symbol:
            drawSymbolBaseline(layer, static_cast<const NodeSymbol*>(node),
                               x, yBaseline);
            break;
        case NodeType::SpecialValue:
            drawSpecialValueBaseline(layer,
                static_cast<const NodeSpecialValue*>(node), x, yBaseline);
            break;
        case NodeType::Collection:
            drawCollectionBaseline(layer,
                static_cast<const NodeCollection*>(node), x, yBaseline,
                fm, font, depth);
            break;
        case NodeType::Equation:
            drawEquationBaseline(layer, static_cast<const NodeEquation*>(node),
                                 x, yBaseline, fm, font, depth);
            break;
        case NodeType::Matrix:
            drawMatrixBaseline(layer, static_cast<const NodeMatrix*>(node),
                               x, yBaseline, fm, font, depth);
            break;
        case NodeType::Interval:
            drawIntervalBaseline(layer, static_cast<const NodeInterval*>(node),
                                 x, yBaseline, fm, font, depth);
            break;
        case NodeType::Piecewise:
            drawPiecewiseBaseline(layer, static_cast<const NodePiecewise*>(node),
                                  x, yBaseline, fm, font, depth);
            break;
        case NodeType::Call:
            drawCallBaseline(layer, static_cast<const NodeCall*>(node),
                             x, yBaseline, fm, font, depth);
            break;
        case NodeType::Unevaluated:
            drawUnevaluatedBaseline(layer,
                static_cast<const NodeUnevaluated*>(node), x, yBaseline,
                fm, font, depth);
            break;
    }

    // ── Smart Highlighter: restore state after drawing the sub-tree ──
    if (!wasHighlight) _highlightActive = false;
}

// ════════════════════════════════════════════════════════════════════════════
// drawRow — Secuencia horizontal de nodos
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawRow(lv_layer_t* layer, const NodeRow* row,
                         int16_t x, int16_t rowYTop,
                         const FontMetrics& fm, const lv_font_t* font,
                         int depth) {
    // ── Smart Highlighter: activate if this NodeRow is the highlight target ──
    bool wasHighlight = _highlightActive;
    if (_highlightNode && row == _highlightNode) _highlightActive = true;

    const auto& rowL = row->layout();
    int16_t cx = x;
    MathClass prevRight = MathClass::ORD;
    bool hasPrev = false;

    for (int i = 0; i < row->childCount(); ++i) {
        MathNode* child = row->child(i);
        const auto& childL = child->layout();

        // ── TeX Inter-Atom Spacing (match layout in NodeRow::calculateLayout) ──
        if (hasPrev) {
            cx = static_cast<int16_t>(
                cx + interAtomSpacingPx(prevRight, child->leftMathClass(), fm.style, fm.emSize));
        }

        const int16_t childYTop = rowChildTopFromRowTop(rowL, childL, rowYTop);
#if defined(NUMOS_MATH_COORD_TRACE) && defined(ARDUINO)
        Serial.printf("[MATH-COORD] rowAsc=%d childAsc=%d rowYTop=%d childYTop=%d yBaseline=%d\n",
                      rowL.ascent, childL.ascent, rowYTop, childYTop,
                      layoutBaselineFromTop(childL, childYTop));
#endif
        drawNodeWithLayout(layer, child, cx, childYTop, fm, font, depth + 1);
        cx += childL.width;
        prevRight = child->rightMathClass();
        hasPrev = true;
    }

    // ── Smart Highlighter: restore state ──
    if (!wasHighlight) _highlightActive = false;
}

// ════════════════════════════════════════════════════════════════════════════
// drawRowBaseline - baseline adapter into the top-based row path
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawRowBaseline(lv_layer_t* layer, const NodeRow* row,
                                 int16_t x, int16_t yBaseline,
                                 const FontMetrics& fm, const lv_font_t* font,
                                 int depth) {
    const int16_t rowYTop = layoutTopFromBaseline(row->layout(), yBaseline);
    drawRow(layer, row, x, rowYTop, fm, font, depth);
}

// drawNumberBaseline - digits and decimal point

void MathCanvas::drawNumberBaseline(lv_layer_t* layer, const NodeNumber* node,
                            int16_t x, int16_t yBaseline,
                            const FontMetrics& fm, const lv_font_t* font) {
    const std::string& val = node->value();
    if (val.empty()) return;
    lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawTextBaseline(layer, x, yBaseline, val.c_str(), node->scriptLevel(), color);
}

// ════════════════════════════════════════════════════════════════════════════
// drawOperator — Símbolos +, −, ×
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawOperatorBaseline(lv_layer_t* layer, const NodeOperator* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    // No manual padding — all spacing is in the TeX inter-atom gap.
    int16_t textX = x;
    lv_color_t color = _highlightActive ? _highlightColor : lv_color_hex(0x333333);

    if (node->op() == OpKind::PlusMinus) {
        const auto& opL = node->layout();
        int16_t glyphW = static_cast<int16_t>(std::max<int16_t>(opL.width, 7));
        int16_t glyphCenterX = static_cast<int16_t>(textX + glyphW / 2);

        // Keep all strokes inside the operator box while centering around baseline axis.
        int16_t topY = static_cast<int16_t>(yBaseline - fm.ascent + 1);
        int16_t bottomY = static_cast<int16_t>(yBaseline + fm.descent - 1);
        int16_t boxMidY = static_cast<int16_t>((topY + bottomY) / 2);
        int16_t minusY = boxMidY;
        minusY = std::max<int16_t>(topY + 1, std::min<int16_t>(minusY, bottomY - 1));

        int16_t minusHalfW = static_cast<int16_t>(std::max<int16_t>((glyphW / 2) - 1, 3));
        int16_t plusHalf = static_cast<int16_t>(std::max<int16_t>(glyphW / 5, 2));
        int16_t plusCenterY = minusY;
        plusCenterY = std::max<int16_t>(static_cast<int16_t>(topY + plusHalf),
                                        std::min<int16_t>(plusCenterY,
                                                          static_cast<int16_t>(bottomY - plusHalf)));

        int16_t stroke = static_cast<int16_t>(
            std::max<int16_t>(1, glyphW / kPlusMinusStrokeDivisor));

        drawLine(layer,
                 static_cast<int16_t>(glyphCenterX - minusHalfW), minusY,
                 static_cast<int16_t>(glyphCenterX + minusHalfW), minusY,
                 stroke, color);
        drawLine(layer,
                 static_cast<int16_t>(glyphCenterX - plusHalf), plusCenterY,
                 static_cast<int16_t>(glyphCenterX + plusHalf), plusCenterY,
                 stroke, color);
        drawLine(layer,
                 glyphCenterX,
                 static_cast<int16_t>(std::max<int16_t>(
                     topY, static_cast<int16_t>(plusCenterY - plusHalf))),
                 glyphCenterX,
                 static_cast<int16_t>(std::min<int16_t>(
                     bottomY, static_cast<int16_t>(plusCenterY + plusHalf))),
                 stroke, color);
        return;
    }

    drawTextBaseline(layer, textX, yBaseline, node->symbol(), node->scriptLevel(), color);
}

// ════════════════════════════════════════════════════════════════════════════
// drawEmpty — Empty placeholder (no visual box; cursor position is sufficient)
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawEmptyBaseline(lv_layer_t* layer, const NodeEmpty* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm) {
    // Intentionally empty: the blinking cursor renders at this position,
    // so no additional placeholder glyph (▯) is needed.
    (void)layer; (void)node; (void)x; (void)yBaseline; (void)fm;
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

void MathCanvas::drawFractionBaseline(lv_layer_t* layer, const NodeFraction* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font,
                              int depth) {
    const auto& fracL = node->layout();
    const auto& numL  = node->numerator()->layout();
    const auto& denL  = node->denominator()->layout();

    const int16_t rulePx = node->ruleThicknessPx();
    const int16_t barHalfUp = node->barHalfUpPx();
    const int16_t barHalfDown = node->barHalfDownPx();
    const int16_t numShift = node->numeratorShiftPx();
    const int16_t denShift = node->denominatorShiftPx();
    const int16_t ruleOverhang = node->ruleOverhangPx();

    int16_t axis  = fm.axisHeight();
    int16_t yAxis = static_cast<int16_t>(yBaseline - axis);

    // Child style matches NodeFraction::calculateLayout (TeX style step-down).
    const FontMetrics childFm = fractionPartMetrics(fm);

    // ── Barra de fracción ──
    int16_t barX1 = static_cast<int16_t>(x + ruleOverhang);
    int16_t barX2 = static_cast<int16_t>(x + fracL.width - ruleOverhang - 1);
    drawLine(layer, barX1, yAxis, barX2, yAxis, rulePx,
             _highlightActive ? _highlightColor : lv_color_black());

    // ── Numerador: baseline = yAxis - barHalfUp - numShift ──
    int16_t numX = static_cast<int16_t>(x + ruleOverhang +
                   (fracL.width - 2 * ruleOverhang - numL.width) / 2);
    int16_t numBaseline = static_cast<int16_t>(yAxis - barHalfUp - numShift);
    drawNodeBaseline(layer, node->numerator(), numX, numBaseline, childFm, font, depth + 1);

    // ── Denominador: baseline = yAxis + barHalfDown + denShift ──
    int16_t denX = static_cast<int16_t>(x + ruleOverhang +
                   (fracL.width - 2 * ruleOverhang - denL.width) / 2);
    int16_t denBaseline = static_cast<int16_t>(yAxis + barHalfDown + denShift);
    drawNodeBaseline(layer, node->denominator(), denX, denBaseline, childFm, font, depth + 1);

#if defined(NUMOS_MATH_INK_OVERLAY)
    const int16_t guideX1 = x;
    const int16_t guideX2 = static_cast<int16_t>(x + fracL.width - 1);
    const int16_t barTop = static_cast<int16_t>(yAxis - barHalfUp);
    const int16_t barBot = static_cast<int16_t>(yAxis + barHalfDown);
    const int16_t numInkTop = static_cast<int16_t>(
        numBaseline - layoutInkAscentPx(numL));
    const int16_t numInkBot = static_cast<int16_t>(
        numBaseline + layoutInkDescentPx(numL));
    const int16_t denInkTop = static_cast<int16_t>(
        denBaseline - layoutInkAscentPx(denL));
    const int16_t denInkBot = static_cast<int16_t>(
        denBaseline + layoutInkDescentPx(denL));

    drawLine(layer, guideX1, numInkTop, guideX2, numInkTop, 1, lv_color_hex(0xD32F2F));
    drawLine(layer, guideX1, numInkBot, guideX2, numInkBot, 1, lv_color_hex(0x388E3C));
    drawLine(layer, guideX1, denInkTop, guideX2, denInkTop, 1, lv_color_hex(0x1976D2));
    drawLine(layer, guideX1, denInkBot, guideX2, denInkBot, 1, lv_color_hex(0x7B1FA2));
    drawLine(layer, guideX1, barTop, guideX2, barTop, 1, lv_color_hex(0xF57C00));
    drawLine(layer, guideX1, barBot, guideX2, barBot, 1, lv_color_hex(0xFBC02D));
    drawLine(layer, guideX1, numBaseline, guideX2, numBaseline, 1, lv_color_hex(0x00796B));
    drawLine(layer, guideX1, denBaseline, guideX2, denBaseline, 1, lv_color_hex(0x5D4037));
    drawLine(layer, guideX1, yAxis, guideX2, yAxis, 1, lv_color_hex(0xC2185B));
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// drawPower — Base ^ Exponente (superíndice)
//
//    ┌base┐┌exp┐     Exponente elevado al 45% del ascent de base
//    │ 23 ││ 4 │     Fuente reducida (~70%)
//    └────┘└───┘
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawPowerBaseline(lv_layer_t* layer, const NodePower* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font,
                           int depth) {
    const auto& baseL = node->base()->layout();

    // ── Base (fuente normal) ──
    drawNodeBaseline(layer, node->base(), x, yBaseline, fm, font, depth + 1);

    // ── Exponente (fuente reducida, elevado) ──
    FontMetrics fmSup = fm.superscript();
    const auto& expL = node->exponent()->layout();
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);
    const int16_t expShift = sup.shiftPx;

    // El baseline del exponente está a expShift sobre el baseline de la base
    int16_t expBaseline = static_cast<int16_t>(yBaseline - expShift);

    int16_t expX = static_cast<int16_t>(x + baseL.width + node->italicCorrectionPx());
    drawNodeBaseline(layer, node->exponent(), expX, expBaseline, fmSup, _fontSmall, depth + 1);

#if defined(NUMOS_MATH_INK_OVERLAY)
    const int16_t guideX1 = x;
    const int16_t guideX2 = static_cast<int16_t>(
        x + baseL.width + node->italicCorrectionPx() + expL.width - 1);
    const int16_t baseInkTop = static_cast<int16_t>(
        yBaseline - layoutInkAscentPx(baseL));
    const int16_t baseInkBot = static_cast<int16_t>(
        yBaseline + layoutInkDescentPx(baseL));
    const int16_t expInkTop = static_cast<int16_t>(
        expBaseline - layoutInkAscentPx(expL));
    const int16_t expInkBot = static_cast<int16_t>(
        expBaseline + layoutInkDescentPx(expL));
    const int16_t layoutTop = static_cast<int16_t>(
        yBaseline - node->layout().ascent);
    const int16_t layoutBot = static_cast<int16_t>(
        yBaseline + node->layout().descent);

    drawLine(layer, guideX1, layoutTop, guideX2, layoutTop, 1, lv_color_hex(0x5E35B1));
    drawLine(layer, guideX1, layoutBot, guideX2, layoutBot, 1, lv_color_hex(0x8D6E63));
    drawLine(layer, guideX1, baseInkTop, guideX2, baseInkTop, 1, lv_color_hex(0x00897B));
    drawLine(layer, guideX1, baseInkBot, guideX2, baseInkBot, 1, lv_color_hex(0x43A047));
    drawLine(layer, guideX1, expInkTop, guideX2, expInkTop, 1, lv_color_hex(0xD81B60));
    drawLine(layer, guideX1, expInkBot, guideX2, expInkBot, 1, lv_color_hex(0xF4511E));
    drawLine(layer, guideX1, yBaseline, guideX2, yBaseline, 1, lv_color_hex(0x3949AB));
    drawLine(layer, guideX1, expBaseline, guideX2, expBaseline, 1, lv_color_hex(0x039BE5));
#endif
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

void MathCanvas::drawRootBaseline(lv_layer_t* layer, const NodeRoot* node,
                          int16_t x, int16_t yBaseline,
                          const FontMetrics& fm, const lv_font_t* font,
                          int depth) {
    const auto& rootL = node->layout();
    const auto& radL  = node->radicand()->layout();
    int16_t hookW     = NodeRoot::RADICAL_HOOK_W;
    int16_t slopeW    = NodeRoot::RADICAL_SLOPE_W;
    int16_t radSymW   = static_cast<int16_t>(hookW + slopeW);

    // MATH table-driven values
    int16_t ruleT    = node->radicalRuleThickness();
    int16_t vertGap  = node->radicalVerticalGap();
    int16_t extraAsc = node->radicalExtraAscender();
    int16_t rightPad = NodeRoot::RADICAL_RIGHT_PAD;

    // ── Key coordinates ──
    int16_t yTop      = static_cast<int16_t>(yBaseline - rootL.ascent);
    int16_t yBottom   = static_cast<int16_t>(yBaseline + rootL.descent);
    int16_t yOverline = static_cast<int16_t>(yTop + ruleT + extraAsc - 1);

    // Midpoint for hook start (~60% from top)
    int16_t hookStartY = static_cast<int16_t>(yTop + (yBottom - yTop) * 6 / 10);

    lv_color_t strokeColor = _highlightActive ? _highlightColor : lv_color_black();

    // ── Hook: small descending stroke ──
    drawLine(layer, x, hookStartY,
             static_cast<int16_t>(x + hookW), yBottom,
             1, strokeColor);

    // ── Slope: ascending stroke from bottom to overline ──
    drawLine(layer,
             static_cast<int16_t>(x + hookW), yBottom,
             static_cast<int16_t>(x + radSymW), yOverline,
             1, strokeColor);

    // ── Overline: horizontal line above the radicand ──
    int16_t overlineX1 = static_cast<int16_t>(x + radSymW);
    int16_t overlineX2 = static_cast<int16_t>(x + radSymW + radL.width + rightPad);
    drawLine(layer, overlineX1, yOverline, overlineX2, yOverline,
             ruleT, strokeColor);

    // ── Radicand ──
    int16_t radX = static_cast<int16_t>(x + radSymW);
    drawNodeBaseline(layer, node->radicand(), radX, yBaseline, fm, font, depth + 1);

    // ── Degree (if present) ──
    if (node->hasDegree()) {
        FontMetrics fmDeg = fm.superscript();
        const auto& degL  = node->degree()->layout();
        int16_t kernBefore = node->radicalKernBefore();
        int16_t degX = static_cast<int16_t>(x + kernBefore);
        int16_t degBaseline = static_cast<int16_t>(yTop + degL.ascent);
        drawNodeBaseline(layer, node->degree(), degX, degBaseline, fmDeg, _fontSmall, depth + 1);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawParen — Paréntesis elásticos que se estiran al contenido
//
//    (  contenido  )     Los paréntesis cubren toda la altura del interior
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawParenBaseline(lv_layer_t* layer, const NodeParen* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font,
                           int depth) {
    const auto& parenL = node->layout();
    int16_t pw = node->parenWidth();
    // Inner padding: optical breathing room proportional to paren width
    int16_t innerPad = std::max<int16_t>(1, pw / 3);

    int16_t yTop    = static_cast<int16_t>(yBaseline - parenL.ascent);
    int16_t yBottom = static_cast<int16_t>(yBaseline + parenL.descent);
    lv_color_t parenColor = _highlightActive ? _highlightColor : lv_color_black();

    // ── Left assembled delimiter ──
    drawDelimiterGlyph(layer, x, yTop, yBottom, node->leftCp(), parenColor, font, fm.emSize);

    // ── Content ──
    int16_t contentX = static_cast<int16_t>(x + pw + innerPad);
    drawNodeBaseline(layer, node->content(), contentX, yBaseline, fm, font, depth + 1);

    // ── Right assembled delimiter ──
    int16_t rpX = static_cast<int16_t>(x + parenL.width - pw);
    drawDelimiterGlyph(layer, rpX, yTop, yBottom, node->rightCp(), parenColor, font, fm.emSize);
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

void MathCanvas::drawFunctionBaseline(lv_layer_t* layer, const NodeFunction* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font,
                              int depth) {
    const auto& argL  = node->argument()->layout();

    const int16_t parenW = node->parenWidth();
    const int16_t innerPad = node->innerPad();
    const int16_t parenBlock = static_cast<int16_t>(
        parenW + innerPad + argL.width + innerPad + parenW);

    // ── Label (texto de la función) ──
    drawTextBaseline(layer, x, yBaseline, node->label(), node->scriptLevel(),
             _highlightActive ? _highlightColor : lv_color_hex(0x1a1a1a));

    // ── Automatic parentheses + argument ──
    int16_t parenX = static_cast<int16_t>(x + node->labelWidth() + NodeFunction::LABEL_GAP);
    int16_t yTop    = static_cast<int16_t>(yBaseline - node->parenAscent());
    int16_t yBottom = static_cast<int16_t>(yBaseline + node->parenDescent());
    lv_color_t parenColor = _highlightActive ? _highlightColor : lv_color_black();

    // Left assembled delimiter (always parentheses for functions)
    drawDelimiterGlyph(layer, parenX, yTop, yBottom, 0x0028, parenColor, font, fm.emSize);

    // Content (argument)
    int16_t contentX = static_cast<int16_t>(parenX + parenW + innerPad);
    drawNodeBaseline(layer, node->argument(), contentX, yBaseline, fm, font, depth + 1);

    // Right assembled delimiter
    int16_t rpX = static_cast<int16_t>(parenX + parenBlock - parenW);
    drawDelimiterGlyph(layer, rpX, yTop, yBottom, 0x0029, parenColor, font, fm.emSize);
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

void MathCanvas::drawLogBaseBaseline(lv_layer_t* layer, const NodeLogBase* node,
                             int16_t x, int16_t yBaseline,
                             const FontMetrics& fm, const lv_font_t* font,
                             int depth) {
    const auto& baseL = node->base()->layout();
    const auto& argL  = node->argument()->layout();

    const int16_t parenW = node->parenWidth();
    const int16_t innerPad = node->innerPad();
    const int16_t parenBlock = static_cast<int16_t>(
        parenW + innerPad + argL.width + innerPad + parenW);

    // ── Label "log" ──
    drawTextBaseline(layer, x, yBaseline, "log", node->scriptLevel(),
             _highlightActive ? _highlightColor : lv_color_hex(0x1a1a1a));

    // ── Base / subíndice (fuente reducida, bajada) ──
    FontMetrics fmSub = fm.superscript();
    int16_t subDrop = MathConstantsProvider(fm.emSize).subscriptShiftDown();
    if (subDrop < 2) subDrop = 2;
    int16_t baseX = static_cast<int16_t>(x + node->labelWidth());
    int16_t baseBaseline = static_cast<int16_t>(yBaseline + subDrop);
    drawNodeBaseline(layer, node->base(), baseX, baseBaseline, fmSub, _fontSmall, depth + 1);

    // ── Automatic parentheses + argument ──
    // Mirror the layout: labelWidth + baseL.width + SpaceAfterScript + LABEL_GAP
    int16_t parenX = static_cast<int16_t>(x + node->labelWidth()
                    + baseL.width + spaceAfterScriptPx(fm) + NodeLogBase::LABEL_GAP);
    int16_t yTop    = static_cast<int16_t>(yBaseline - node->parenAscent());
    int16_t yBottom = static_cast<int16_t>(yBaseline + node->parenDescent());
    lv_color_t parenColor = _highlightActive ? _highlightColor : lv_color_black();

    // Left assembled delimiter (always parentheses for functions)
    drawDelimiterGlyph(layer, parenX, yTop, yBottom, 0x0028, parenColor, font, fm.emSize);

    // Content (argument)
    int16_t contentX = static_cast<int16_t>(parenX + parenW + innerPad);
    drawNodeBaseline(layer, node->argument(), contentX, yBaseline, fm, font, depth + 1);

    // Right assembled delimiter
    int16_t rpX = static_cast<int16_t>(parenX + parenBlock - parenW);
    drawDelimiterGlyph(layer, rpX, yTop, yBottom, 0x0029, parenColor, font, fm.emSize);
}

// ════════════════════════════════════════════════════════════════════════════
// drawAssembledDelimiter — REMOVED.
// All delimiter rendering uses drawDelimiterGlyph above, which
// consumes the canonical Piece[8] array from DelimiterAssembler::assemble.
// This eliminates the ~70-line manual fallback duplicate.
// ════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════
// drawConstant — Símbolo π o e
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawConstantBaseline(lv_layer_t* layer, const NodeConstant* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    lv_color_t color = _highlightActive ? _highlightColor : lv_color_hex(0x0060C0);
    drawTextBaseline(layer, x, yBaseline, node->symbol(), node->scriptLevel(), color);
}

// ════════════════════════════════════════════════════════════════════════════
// drawVariable — Variable algebraica: x, y, z (azul), A-F, Ans, PreAns
//
// Estilo visual:
//   · x, y, z      → Azul #4A90D9 para diferenciar de la × de multiplicar
//   · A-F           → Negro normal
//   · Ans, PreAns   → Bloque de texto integrado en negro
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawVariableBaseline(lv_layer_t* layer, const NodeVariable* node,
                              int16_t x, int16_t yBaseline,
                              const FontMetrics& fm, const lv_font_t* font) {
    // All variables render in black — x, y, z are independent variables,
    // NOT multiplication.  Previously blue, now black to avoid confusion
    // with the × operator.
    lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();

    drawTextBaseline(layer, x, yBaseline, node->label(), node->scriptLevel(), color);
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

void MathCanvas::drawPeriodicDecimalBaseline(lv_layer_t* layer,
                                     const NodePeriodicDecimal* node,
                                     int16_t x, int16_t yBaseline,
                                     const FontMetrics& fm,
                                     const lv_font_t* font) {
    // Use node-owned stable strings directly; do not build a render-time prefix.
    const std::string& rep = node->repeat();
    const std::string& intPart = node->intPart();
    const std::string& nonRepeat = node->nonRepeat();

    // Draw the non-repeating prefix at the same positions as the old prefix.
    int16_t drawX = x;
    if (node->isNegative()) {
        drawTextBaseline(layer, drawX, yBaseline, "-", node->scriptLevel(), lv_color_black());
        drawX = static_cast<int16_t>(drawX + fm.charWidth);
    }
    if (!intPart.empty()) {
        drawTextBaseline(layer, drawX, yBaseline, intPart.c_str(), node->scriptLevel(), lv_color_black());
        drawX = static_cast<int16_t>(drawX + intPart.size() * fm.charWidth);
    }
    if (!nonRepeat.empty() || !rep.empty()) {
        drawTextBaseline(layer, drawX, yBaseline, ".", node->scriptLevel(), lv_color_black());
        drawX = static_cast<int16_t>(drawX + fm.charWidth);
    }
    if (!nonRepeat.empty()) {
        drawTextBaseline(layer, drawX, yBaseline, nonRepeat.c_str(), node->scriptLevel(), lv_color_black());
    }

    const std::size_t prefixChars = periodicDecimalPrefixCharCount(
        node->isNegative(), intPart.size(), nonRepeat.size(), rep.size());
    int16_t cx = static_cast<int16_t>(x + prefixChars * fm.charWidth);

    // Dibujar la parte periódica (con overline)
    if (!rep.empty()) {
        drawTextBaseline(layer, cx, yBaseline, rep.c_str(), node->scriptLevel(), lv_color_black());
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
// drawDefIntegral — Integral definida: ∫_a^b f(x) dx
//
//    ┌ upper ┐
//    │  ∫    │  body  d(var)
//    └ lower ┘
//
// The ∫ symbol is drawn as vector lines (S-curve).
// Limits rendered in reduced font above and below the symbol.
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawDefIntegralBaseline(lv_layer_t* layer, const NodeDefIntegral* node,
                                  int16_t x, int16_t yBaseline,
                                  const FontMetrics& fm, const lv_font_t* font,
                                  int depth) {
    const auto& lowerL = node->lower()->layout();
    const auto& upperL = node->upper()->layout();
    const auto& bodyL  = node->body()->layout();

    FontMetrics fmLimit = fm.superscript();

    const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        0x222B, largeOperatorTargetHeightPx(fm), fm.emSize);
    int16_t symW = symMetrics.valid ? symMetrics.widthPx : 0;
    int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();
    if (symW < 6) symW = std::max<int16_t>(6, fm.charWidth);

    int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    int16_t bodyGapX = largeOperatorBodyGapPx(fm);
    int16_t dGapX = integralDifferentialGapPx(fm);

    lv_color_t opColor = _highlightActive ? _highlightColor : lv_color_black();

    if (shouldUseDisplayLimits(fm, symH)) {
        // ── DISPLAY: limits above/below the integral sign ──
        const DisplayLimitGeometry geom = displayLimitGeometry(
            x, yBaseline, symW, symH, lowerL, upperL, bodyL,
            fm, limitGapPx, bodyGapX);

        // Draw ∫ glyph natively from STIX Two Math at the symbol column center
        int16_t symDrawX = static_cast<int16_t>(x + (geom.symbolColumnWidth - symW) / 2);
        drawTextBaseline(layer, symDrawX, yBaseline,
                 numos::mathsym::SYMB_INT, node->scriptLevel(), opColor);

        // Upper limit (centered above symbol)
        drawNodeBaseline(layer, node->upper(), geom.upperX, geom.upperBaseline, fmLimit, _fontSmall, depth + 1);

        // Lower limit (centered below symbol)
        drawNodeBaseline(layer, node->lower(), geom.lowerX, geom.lowerBaseline, fmLimit, _fontSmall, depth + 1);

        // Body
        drawNodeBaseline(layer, node->body(), geom.bodyX, yBaseline, fm, font, depth + 1);

        // "d" + variable
        int16_t dX = static_cast<int16_t>(geom.bodyX + bodyL.width + dGapX);
        drawTextBaseline(layer, dX, yBaseline, "d", node->scriptLevel(), lv_color_hex(0x333333));
        int16_t varX = static_cast<int16_t>(dX + fm.charWidth + dGapX);
        drawNodeBaseline(layer, node->variable(), varX, yBaseline, fm, font, depth + 1);
    } else {
        // ── TEXT / SCRIPT: limits as sub/superscript to the right ──
        // Draw ∫ glyph natively at its natural position
        drawTextBaseline(layer, x, yBaseline,
                 numos::mathsym::SYMB_INT, node->scriptLevel(), opColor);

        const InlineLimitGeometry geom = inlineLimitGeometry(
            x, yBaseline, symW, lowerL, upperL, fm, bodyGapX);

        drawNodeBaseline(layer, node->upper(), geom.upperX, geom.upperBaseline, fmLimit, _fontSmall, depth + 1);
        drawNodeBaseline(layer, node->lower(), geom.lowerX, geom.lowerBaseline, fmLimit, _fontSmall, depth + 1);

        // Body expression
        drawNodeBaseline(layer, node->body(), geom.bodyX, yBaseline, fm, font, depth + 1);

        // "d" + variable
        int16_t dX = static_cast<int16_t>(geom.bodyX + bodyL.width + dGapX);
        drawTextBaseline(layer, dX, yBaseline, "d", node->scriptLevel(), lv_color_hex(0x333333));
        int16_t varX = static_cast<int16_t>(dX + fm.charWidth + dGapX);
        drawNodeBaseline(layer, node->variable(), varX, yBaseline, fm, font, depth + 1);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawSummation — Sumatorio: ∑_{n=a}^{b} expr
//
//    ┌ upper ┐
//    │  ∑    │  body
//    └ lower ┘
//
// The ∑ symbol is drawn as vector lines (zigzag shape).
// Limits rendered in reduced font above and below the symbol.
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawSummationBaseline(lv_layer_t* layer, const NodeSummation* node,
                                int16_t x, int16_t yBaseline,
                                const FontMetrics& fm, const lv_font_t* font,
                                int depth) {
    const auto& lowerL = node->lower()->layout();
    const auto& upperL = node->upper()->layout();
    const auto& bodyL  = node->body()->layout();

    FontMetrics fmLimit = fm.superscript();

    const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        0x2211, largeOperatorTargetHeightPx(fm), fm.emSize);
    int16_t symW = symMetrics.valid ? symMetrics.widthPx : 0;
    int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();
    if (symW < 6) symW = std::max<int16_t>(6, fm.charWidth);

    int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    int16_t bodyGapX = largeOperatorBodyGapPx(fm);

    lv_color_t opColor = _highlightActive ? _highlightColor : lv_color_black();

    if (shouldUseDisplayLimits(fm, symH)) {
        // ── DISPLAY: limits centered above/below the ∑ symbol ──
        const DisplayLimitGeometry geom = displayLimitGeometry(
            x, yBaseline, symW, symH, lowerL, upperL, bodyL,
            fm, limitGapPx, bodyGapX);

        // Draw ∑ glyph natively from STIX Two Math centered in the symbol column
        int16_t symDrawX = static_cast<int16_t>(x + (geom.symbolColumnWidth - symW) / 2);
        drawTextBaseline(layer, symDrawX, yBaseline,
                 numos::mathsym::SYMB_SUM, node->scriptLevel(), opColor);

        // Upper limit
        drawNodeBaseline(layer, node->upper(), geom.upperX, geom.upperBaseline, fmLimit, _fontSmall, depth + 1);

        // Lower limit
        drawNodeBaseline(layer, node->lower(), geom.lowerX, geom.lowerBaseline, fmLimit, _fontSmall, depth + 1);

        // Body
        drawNodeBaseline(layer, node->body(), geom.bodyX, yBaseline, fm, font, depth + 1);
    } else {
        // ── TEXT / SCRIPT: limits as sub/superscript to the right ──
        // Draw ∑ glyph natively at its natural position
        drawTextBaseline(layer, x, yBaseline,
                 numos::mathsym::SYMB_SUM, node->scriptLevel(), opColor);

        const InlineLimitGeometry geom = inlineLimitGeometry(
            x, yBaseline, symW, lowerL, upperL, fm, bodyGapX);

        drawNodeBaseline(layer, node->upper(), geom.upperX, geom.upperBaseline, fmLimit, _fontSmall, depth + 1);
        drawNodeBaseline(layer, node->lower(), geom.lowerX, geom.lowerBaseline, fmLimit, _fontSmall, depth + 1);

        // Body
        drawNodeBaseline(layer, node->body(), geom.bodyX, yBaseline, fm, font, depth + 1);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// drawBigOp — Generic large n-ary operator: ∏, ⋂, ⋃, ⋀, ⋁
//
// Style-dependent rendering:
//   DISPLAY: operator centered, limits above/below, body to the right
//   TEXT:    operator at normal size, limits as sub/superscript to the right
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawBigOpBaseline(lv_layer_t* layer, const NodeBigOp* node,
                           int16_t x, int16_t yBaseline,
                           const FontMetrics& fm, const lv_font_t* font,
                           int depth) {
    const auto& lowerL = node->lower()->layout();
    const auto& upperL = node->upper()->layout();
    const auto& bodyL  = node->body()->layout();

    FontMetrics fmLimit = fm.superscript();

    const auto opMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        node->operatorCodepoint(), largeOperatorTargetHeightPx(fm), fm.emSize);
    int16_t opW = opMetrics.valid ? opMetrics.widthPx : 0;
    int16_t opH = opMetrics.valid ? opMetrics.heightPx : fm.height();
    if (opW < 6) opW = std::max<int16_t>(6, fm.charWidth);

    int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    int16_t bodyGapX = largeOperatorBodyGapPx(fm);

    lv_color_t opColor = _highlightActive ? _highlightColor : lv_color_black();

    if (node->useDisplayLimits()) {
        const DisplayLimitGeometry geom = displayLimitGeometry(
            x, yBaseline, opW, opH, lowerL, upperL, bodyL,
            fm, limitGapPx, bodyGapX);

        // Operator glyph centered in its column, on the baseline
        int16_t opCenterX = static_cast<int16_t>(x + geom.symbolColumnWidth / 2);
        int16_t opDrawX = static_cast<int16_t>(opCenterX - opW / 2);

        // Draw operator as text at baseline
        drawTextBaseline(layer, opDrawX, yBaseline, node->operatorUtf8(),
                 node->scriptLevel(), opColor);

        // Upper limit centered above operator
        drawNodeBaseline(layer, node->upper(), geom.upperX, geom.upperBaseline, fmLimit, _fontSmall, depth + 1);

        // Lower limit centered below operator
        drawNodeBaseline(layer, node->lower(), geom.lowerX, geom.lowerBaseline, fmLimit, _fontSmall, depth + 1);

        // Body expression
        drawNodeBaseline(layer, node->body(), geom.bodyX, yBaseline, fm, font, depth + 1);
    } else {
        // ── TEXT / SCRIPT: limits as sub/superscript to the right ──
        // Draw operator glyph at baseline
        drawTextBaseline(layer, x, yBaseline, node->operatorUtf8(),
                 node->scriptLevel(), opColor);

        MathConstantsProvider mcT(fm.emSize);
        int16_t supShift = mcT.superscriptShiftUp();
        int16_t subDrop  = mcT.subscriptShiftDown();
        if (subDrop < 2) subDrop = 2;
        if (supShift < 1) supShift = 1;

        // Limits to the right of the operator
        int16_t limitsX = static_cast<int16_t>(x + opW);
        int16_t upperBaseline = static_cast<int16_t>(yBaseline - supShift);
        int16_t lowerBaseline = static_cast<int16_t>(yBaseline + subDrop);

        int16_t limitsW = std::max(upperL.width, lowerL.width);
        int16_t upperLimitX = static_cast<int16_t>(limitsX + (limitsW - upperL.width) / 2);
        int16_t lowerLimitX = static_cast<int16_t>(limitsX + (limitsW - lowerL.width) / 2);

        drawNodeBaseline(layer, node->upper(), upperLimitX, upperBaseline, fmLimit, _fontSmall, depth + 1);
        drawNodeBaseline(layer, node->lower(), lowerLimitX, lowerBaseline, fmLimit, _fontSmall, depth + 1);

        // Body expression
        int16_t bodyX = static_cast<int16_t>(limitsX + limitsW + bodyGapX);
        drawNodeBaseline(layer, node->body(), bodyX, yBaseline, fm, font, depth + 1);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// N-ary operator rendering — Native STIX Two Math glyphs
//
// All large operators (∫, ∑, ∏, etc.) are drawn using the STIX Two Math
// font glyphs through LVGL's lv_draw_letter. Size variants from the
// MathVariantTable are used to select the appropriate glyph height.
// Hand-drawn vector primitives have been removed in Phase 4.4.
// ════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════
// drawSubscript — Generic subscript: base_subscript (e.g., x₁, x₂)
//
//    ┌base┐┌sub┐   El subíndice se baja ~33% del descent
//    │ x  ││ 1 │   Fuente reducida (~70%)
//    └────┘└───┘
//
// Layout mirrors NodeLogBase subscript positioning.
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawSubscriptBaseline(lv_layer_t* layer, const NodeSubscript* node,
                                int16_t x, int16_t yBaseline,
                                const FontMetrics& fm, const lv_font_t* font,
                                int depth) {
    const auto& baseL = node->base()->layout();

    // ── Base (fuente normal) ──
    drawNodeBaseline(layer, node->base(), x, yBaseline, fm, font, depth + 1);

    // ── Subíndice (fuente reducida, bajada) ──
    FontMetrics fmSub = fm.superscript();
    int16_t subDrop = MathConstantsProvider(fm.emSize).subscriptShiftDown();
    if (subDrop < 2) subDrop = 2;
    int16_t subX = static_cast<int16_t>(x + baseL.width);
    int16_t subBaseline = static_cast<int16_t>(yBaseline + subDrop);
    drawNodeBaseline(layer, node->subscript(), subX, subBaseline, fmSub, _fontSmall, depth + 1);
}

void MathCanvas::drawSymbolBaseline(lv_layer_t* layer, const NodeSymbol* node,
                                    int16_t x, int16_t yBaseline) {
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawTextBaseline(layer, x, yBaseline, node->name().c_str(),
                     node->scriptLevel(), color);
}

void MathCanvas::drawSpecialValueBaseline(lv_layer_t* layer,
                                          const NodeSpecialValue* node,
                                          int16_t x, int16_t yBaseline) {
    const lv_color_t color = _highlightActive
        ? _highlightColor
        : (node->specialKind() == SpecialValueKind::Undefined
            ? lv_color_hex(0x9B1C1C) : lv_color_black());
    drawTextBaseline(layer, x, yBaseline, node->label(), node->scriptLevel(), color);
}

void MathCanvas::drawCollectionBaseline(lv_layer_t* layer,
                                        const NodeCollection* node,
                                        int16_t x, int16_t yBaseline,
                                        const FontMetrics& fm,
                                        const lv_font_t* font, int depth) {
    const auto& layout = node->layout();
    const int16_t top = static_cast<int16_t>(yBaseline - layout.ascent);
    const int16_t bottom = static_cast<int16_t>(yBaseline + layout.descent);
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawDelimiterGlyph(layer, x, top, bottom, node->leftCp(), color,
                       font, fm.emSize);
    int16_t childX = static_cast<int16_t>(x + node->delimiterWidth() +
                                          node->innerPad());
    for (int i = 0; i < node->childCount(); ++i) {
        MathNode* element = node->child(i);
        drawNodeBaseline(layer, element, childX, yBaseline, fm, font, depth + 1);
        childX = static_cast<int16_t>(childX + element->layout().width);
        if (i + 1 < node->childCount()) {
            drawTextBaseline(layer, childX, yBaseline, ",",
                             node->scriptLevel(), color);
            childX = static_cast<int16_t>(childX + node->separatorWidth());
        }
    }
    drawDelimiterGlyph(layer, static_cast<int16_t>(x + layout.width -
        node->delimiterWidth()), top, bottom, node->rightCp(), color,
        font, fm.emSize);
}

void MathCanvas::drawEquationBaseline(lv_layer_t* layer,
                                      const NodeEquation* node,
                                      int16_t x, int16_t yBaseline,
                                      const FontMetrics& fm,
                                      const lv_font_t* font, int depth) {
    drawNodeBaseline(layer, node->lhs(), x, yBaseline, fm, font, depth + 1);
    int16_t relationX = static_cast<int16_t>(x + node->lhs()->layout().width +
                                             node->relationGap());
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_hex(0x333333);
    drawTextBaseline(layer, relationX, yBaseline, "=", node->scriptLevel(), color);
    const int16_t rhsX = static_cast<int16_t>(relationX + node->relationWidth() +
                                              node->relationGap());
    drawNodeBaseline(layer, node->rhs(), rhsX, yBaseline, fm, font, depth + 1);
}

void MathCanvas::drawMatrixBaseline(lv_layer_t* layer, const NodeMatrix* node,
                                    int16_t x, int16_t yBaseline,
                                    const FontMetrics& fm,
                                    const lv_font_t* font, int depth) {
    const auto& layout = node->layout();
    const int16_t top = static_cast<int16_t>(yBaseline - layout.ascent);
    const int16_t bottom = static_cast<int16_t>(yBaseline + layout.descent);
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawDelimiterGlyph(layer, x, top, bottom, 0x005B, color, font, fm.emSize);
    const int16_t contentX = static_cast<int16_t>(x + node->delimiterWidth());
    for (uint8_t row = 0; row < node->rows(); ++row) {
        int16_t columnX = contentX;
        const int16_t rowBase = static_cast<int16_t>(yBaseline + node->rowBaseline(row));
        for (uint8_t column = 0; column < node->columns(); ++column) {
            MathNode* cell = node->cell(row, column);
            const int16_t cellX = static_cast<int16_t>(columnX + node->horizontalPad() +
                (node->columnWidth(column) - (cell ? cell->layout().width : 0)) / 2);
            if (cell)
                drawNodeBaseline(layer, cell, cellX, rowBase, fm, font, depth + 1);
            columnX = static_cast<int16_t>(columnX + node->columnWidth(column) +
                                           2 * node->horizontalPad());
        }
    }
    drawDelimiterGlyph(layer, static_cast<int16_t>(x + layout.width -
        node->delimiterWidth()), top, bottom, 0x005D, color, font, fm.emSize);
}

void MathCanvas::drawIntervalBaseline(lv_layer_t* layer,
                                      const NodeInterval* node,
                                      int16_t x, int16_t yBaseline,
                                      const FontMetrics& fm,
                                      const lv_font_t* font, int depth) {
    const auto& layout = node->layout();
    const int16_t top = static_cast<int16_t>(yBaseline - layout.ascent);
    const int16_t bottom = static_cast<int16_t>(yBaseline + layout.descent);
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawDelimiterGlyph(layer, x, top, bottom, node->leftCp(), color, font, fm.emSize);
    int16_t childX = static_cast<int16_t>(x + node->delimiterWidth() + node->innerPad());
    drawNodeBaseline(layer, node->lower(), childX, yBaseline, fm, font, depth + 1);
    childX = static_cast<int16_t>(childX + node->lower()->layout().width);
    drawTextBaseline(layer, childX, yBaseline, ",", node->scriptLevel(), color);
    childX = static_cast<int16_t>(childX + node->separatorWidth());
    drawNodeBaseline(layer, node->upper(), childX, yBaseline, fm, font, depth + 1);
    drawDelimiterGlyph(layer, static_cast<int16_t>(x + layout.width -
        node->delimiterWidth()), top, bottom, node->rightCp(), color, font, fm.emSize);
}

void MathCanvas::drawPiecewiseBaseline(lv_layer_t* layer,
                                       const NodePiecewise* node,
                                       int16_t x, int16_t yBaseline,
                                       const FontMetrics& fm,
                                       const lv_font_t* font, int depth) {
    const auto& layout = node->layout();
    const int16_t top = static_cast<int16_t>(yBaseline - layout.ascent);
    const int16_t bottom = static_cast<int16_t>(yBaseline + layout.descent);
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawDelimiterGlyph(layer, x, top, bottom, 0x007B, color, font, fm.emSize);
    const int16_t expressionX = static_cast<int16_t>(x + node->braceWidth() +
                                                     node->bracePad());
    const int16_t conditionX = static_cast<int16_t>(expressionX +
        node->expressionColumnWidth() + node->columnGap());
    for (uint8_t row = 0; row < node->branchCount(); ++row) {
        const auto& branch = node->branch(row);
        const int16_t rowBase = static_cast<int16_t>(yBaseline + node->rowBaseline(row));
        drawNodeBaseline(layer, branch.expression.get(), expressionX, rowBase,
                         fm, font, depth + 1);
        if (branch.otherwise) {
            drawTextBaseline(layer, conditionX, rowBase, "otherwise",
                             node->scriptLevel(), lv_color_hex(0x555555));
        } else {
            drawTextBaseline(layer, conditionX, rowBase, "if ",
                             node->scriptLevel(), lv_color_hex(0x555555));
            drawNodeBaseline(layer, branch.condition.get(),
                static_cast<int16_t>(conditionX + fm.charWidth * 3), rowBase,
                fm, font, depth + 1);
        }
    }
}

void MathCanvas::drawCallBaseline(lv_layer_t* layer, const NodeCall* node,
                                  int16_t x, int16_t yBaseline,
                                  const FontMetrics& fm,
                                  const lv_font_t* font, int depth) {
    const auto& layout = node->layout();
    const lv_color_t color = _highlightActive ? _highlightColor : lv_color_black();
    drawTextBaseline(layer, x, yBaseline, node->name().c_str(),
                     node->scriptLevel(), color);
    const int16_t leftX = static_cast<int16_t>(x + node->labelWidth() + node->labelGap());
    const int16_t top = static_cast<int16_t>(yBaseline - layout.ascent);
    const int16_t bottom = static_cast<int16_t>(yBaseline + layout.descent);
    drawDelimiterGlyph(layer, leftX, top, bottom, 0x0028, color, font, fm.emSize);
    int16_t argumentX = static_cast<int16_t>(leftX + node->delimiterWidth() +
                                             node->innerPad());
    for (int i = 0; i < node->childCount(); ++i) {
        MathNode* argument = node->child(i);
        drawNodeBaseline(layer, argument, argumentX, yBaseline, fm, font, depth + 1);
        argumentX = static_cast<int16_t>(argumentX + argument->layout().width);
        if (i + 1 < node->childCount()) {
            drawTextBaseline(layer, argumentX, yBaseline, ",",
                             node->scriptLevel(), color);
            argumentX = static_cast<int16_t>(argumentX + node->separatorWidth());
        }
    }
    drawDelimiterGlyph(layer, static_cast<int16_t>(x + layout.width -
        node->delimiterWidth()), top, bottom, 0x0029, color, font, fm.emSize);
}

void MathCanvas::drawUnevaluatedBaseline(lv_layer_t* layer,
                                         const NodeUnevaluated* node,
                                         int16_t x, int16_t yBaseline,
                                         const FontMetrics& fm,
                                         const lv_font_t* font, int depth) {
    const lv_color_t labelColor = _highlightActive
        ? _highlightColor : lv_color_hex(0x7A5A00);
    drawTextBaseline(layer, x, yBaseline, "unevaluated",
                     node->scriptLevel(), labelColor);
    drawNodeBaseline(layer, node->expression(),
        static_cast<int16_t>(x + node->labelWidth() + node->gap()),
        yBaseline, fm, font, depth + 1);
}

// ════════════════════════════════════════════════════════════════════════════
// drawCursor — Línea vertical parpadeante
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawCursor(lv_layer_t* layer) {
    if (!isCursorEditable() || !_cursorVisible) return;

    drawFilledRect(layer, _cursorX, _cursorY, CURSOR_WIDTH, _cursorH,
                   lv_color_hex(CURSOR_COLOR), LV_OPA_COVER);
}

// ════════════════════════════════════════════════════════════════════════════
// Helpers de dibujo — Wrappers limpios sobre la API de LVGL 9.x
// ════════════════════════════════════════════════════════════════════════════

void MathCanvas::drawTextBaseline(lv_layer_t* layer, int16_t x, int16_t yBaseline,
                          const char* text, uint8_t scriptLevel,
                          lv_color_t color) {
    if (!text || !text[0]) return;

    char normalized[numos::mathsym::kNormalizedTextBufferBytes] = {};
    const auto normalizedText = numos::mathsym::normalizeMathTextNoAlloc(
        text, normalized, sizeof(normalized));
    const char* renderText = normalizedText.text ? normalizedText.text : text;

    const lv_font_t* font = (scriptLevel >= 2)
        ? _fontScriptScript
        : ((scriptLevel == 1) ? _fontSmall : _fontNormal);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(renderText);
    int32_t penX = x;

    while (*p) {
        uint32_t cp = 0;
        const uint8_t step = vpam::utf8Decode(p, cp);

        uint32_t nextCp = 0;
        const uint8_t* nextPtr = p + step;
        if (*nextPtr) {
            vpam::utf8Decode(nextPtr, nextCp);
        }

        lv_font_glyph_dsc_t glyph;
        const bool ok = lv_font_get_glyph_dsc(font, &glyph, cp, nextCp);
        if (ok) {
            lv_draw_letter_dsc_t dsc;
            lv_draw_letter_dsc_init(&dsc);
            dsc.font = font;
            dsc.color = color;
            dsc.opa = LV_OPA_COVER;
            dsc.unicode = cp;

            // WHY: LVGL 9 lv_draw_letter() consumes a pivot point.  Its
            // internal renderer subtracts (adv_w/2, lineAscent), so passing
            // the mathematical baseline here makes the final bitmap top equal
            // baseline - box_h - ofs_y, matching glyphInkAscentPx().
            lv_point_t letterPos;
            letterPos.x = static_cast<int32_t>(penX + glyph.adv_w / 2);
            letterPos.y = static_cast<int32_t>(yBaseline);

            lv_draw_letter(layer, &dsc, &letterPos);
            penX += glyph.adv_w;
        } else {
            // Fallback de avance mínimo para no superponer caracteres desconocidos.
            penX += std::max<int32_t>(1, font->line_height / 3);
        }

        p += step;
    }
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

