#!/usr/bin/env python3
"""
extract_stix_math.py — Build-time STIX Two Math font data extraction.

Extracts the 56 OpenType MATH table constants from STIXTwoMath-Regular.otf/.ttf
and generates a C/C++ header for the NumOS ESP32-S3 calculator firmware.

Requirements:
    pip install fonttools

Usage:
    python scripts/extract_stix_math.py [path-to-STIXTwoMath-Regular.ttf]

Output:
    src/math/font/stix_math_constants.h   — constexpr int16_t kStixMathConstants[56]
    src/math/font/stix_math_italics.h     — MathItalicsCorrectionInfo table
    src/math/font/stix_math_variants.h    — GlyphAssembly tables for delimiters
"""

import os
import sys
from datetime import datetime
from pathlib import Path

# ── Find fontTools ──────────────────────────────────────────────────────────
try:
    from fontTools.ttLib import TTFont
except ImportError:
    print("ERROR: fontTools is not installed.", file=sys.stderr)
    print("Install it with:  pip install fonttools", file=sys.stderr)
    sys.exit(1)


# ═══════════════════════════════════════════════════════════════════════════════
# The 56 MathConstants in OpenType spec serialization order.
# Index → (C++ name, OpenType spec name, storage type)
#
# Storage types:
#   "i16"  = direct int16_t  (constants 0–1: scale percentages)
#   "u16"  = direct uint16_t (constants 2–3: min heights, constant 55: raise %)
#   "mvr"  = MathValueRecord (constants 4–54: {int16_t Value; Offset16 DeviceTable})
#            We extract only the .Value — DeviceTable is always NULL in STIX Two Math
#            and we have no runtime device-table interpreter on ESP32.
# ═══════════════════════════════════════════════════════════════════════════════

CONSTANT_SPEC = [
    # idx,  C++ name,                               storage
    ( 0,    "ScriptPercentScaleDown",                "i16"),
    ( 1,    "ScriptScriptPercentScaleDown",          "i16"),
    ( 2,    "DelimitedSubFormulaMinHeight",          "u16"),
    ( 3,    "DisplayOperatorMinHeight",              "u16"),
    ( 4,    "MathLeading",                           "mvr"),
    ( 5,    "AxisHeight",                            "mvr"),
    ( 6,    "AccentBaseHeight",                      "mvr"),
    ( 7,    "FlattenedAccentBaseHeight",             "mvr"),
    ( 8,    "SubscriptShiftDown",                    "mvr"),
    ( 9,    "SubscriptTopMax",                       "mvr"),
    (10,    "SubscriptBaselineDropMin",              "mvr"),
    (11,    "SuperscriptShiftUp",                    "mvr"),
    (12,    "SuperscriptShiftUpCramped",             "mvr"),
    (13,    "SuperscriptBottomMin",                  "mvr"),
    (14,    "SuperscriptBaselineDropMax",            "mvr"),
    (15,    "SubSuperscriptGapMin",                  "mvr"),
    (16,    "SuperscriptBottomMaxWithSubscript",     "mvr"),
    (17,    "SpaceAfterScript",                      "mvr"),
    (18,    "UpperLimitGapMin",                      "mvr"),
    (19,    "UpperLimitBaselineRiseMin",             "mvr"),
    (20,    "LowerLimitGapMin",                      "mvr"),
    (21,    "LowerLimitBaselineDropMin",             "mvr"),
    (22,    "StackTopShiftUp",                       "mvr"),
    (23,    "StackTopDisplayStyleShiftUp",           "mvr"),
    (24,    "StackBottomShiftDown",                  "mvr"),
    (25,    "StackBottomDisplayStyleShiftDown",      "mvr"),
    (26,    "StackGapMin",                           "mvr"),
    (27,    "StackDisplayStyleGapMin",               "mvr"),
    (28,    "StretchStackTopShiftUp",                "mvr"),
    (29,    "StretchStackBottomShiftDown",           "mvr"),
    (30,    "StretchStackGapAboveMin",               "mvr"),
    (31,    "StretchStackGapBelowMin",               "mvr"),
    (32,    "FractionNumeratorShiftUp",              "mvr"),
    (33,    "FractionNumeratorDisplayStyleShiftUp",  "mvr"),
    (34,    "FractionDenominatorShiftDown",          "mvr"),
    (35,    "FractionDenominatorDisplayStyleShiftDown","mvr"),
    (36,    "FractionNumeratorGapMin",               "mvr"),
    (37,    "FractionNumDisplayStyleGapMin",         "mvr"),
    (38,    "FractionRuleThickness",                 "mvr"),
    (39,    "FractionDenominatorGapMin",             "mvr"),
    (40,    "FractionDenomDisplayStyleGapMin",       "mvr"),
    (41,    "SkewedFractionHorizontalGap",           "mvr"),
    (42,    "SkewedFractionVerticalGap",             "mvr"),
    (43,    "OverbarVerticalGap",                    "mvr"),
    (44,    "OverbarRuleThickness",                  "mvr"),
    (45,    "OverbarExtraAscender",                  "mvr"),
    (46,    "UnderbarVerticalGap",                   "mvr"),
    (47,    "UnderbarRuleThickness",                 "mvr"),
    (48,    "UnderbarExtraDescender",                "mvr"),
    (49,    "RadicalVerticalGap",                    "mvr"),
    (50,    "RadicalDisplayStyleVerticalGap",        "mvr"),
    (51,    "RadicalRuleThickness",                  "mvr"),
    (52,    "RadicalExtraAscender",                  "mvr"),
    (53,    "RadicalKernBeforeDegree",               "mvr"),
    (54,    "RadicalKernAfterDegree",                "mvr"),
    (55,    "RadicalDegreeBottomRaisePercent",       "u16"),
]

EXPECTED_COUNT = 56  # Sanity check


def extract_constants(font_path: str) -> tuple[list[int], int]:
    """
    Extract all 56 MathConstants from the font's MATH table.

    Returns:
        (values[56], unitsPerEm) — list of int16_t-compatible values + font UPM.
    """
    font = TTFont(font_path)

    if "MATH" not in font:
        raise ValueError(f"Font '{font_path}' does not contain a MATH table. "
                         f"Is this STIX Two Math?")

    mc = font["MATH"].table.MathConstants
    upm = font["head"].unitsPerEm

    values = []
    for idx, name, storage in CONSTANT_SPEC:
        try:
            raw = getattr(mc, name, None)
        except Exception:
            raw = None

        if raw is None:
            print(f"  [WARN] Constant {idx:2d} '{name}' missing — falling back to 0")
            values.append(0)
            continue

        if storage == "mvr":
            # MathValueRecord — extract .Value (int16_t)
            if hasattr(raw, "Value"):
                val = raw.Value
            else:
                print(f"  [WARN] Constant {idx:2d} '{name}' is not a "
                      f"MathValueRecord — using raw value as fallback")
                val = int(raw)
        else:
            val = int(raw)

        # Clamp to int16_t range (OpenType spec guarantees these fit, but be safe)
        if val < -32768 or val > 32767:
            print(f"  [WARN] Constant {idx:2d} '{name}' = {val} exceeds int16_t "
                  f"range — clamping")
            val = max(-32768, min(32767, val))

        values.append(val)

    if len(values) != EXPECTED_COUNT:
        raise RuntimeError(f"Internal error: extracted {len(values)} constants, "
                           f"expected {EXPECTED_COUNT}")

    font.close()
    return values, upm


def generate_header(values: list[int], upm: int, output_path: str,
                    source_font: str) -> None:
    """Generate C/C++ header with the extracted constants array."""

    # Build per-constant comments
    constant_lines = []
    for i, (val, (_, name, _)) in enumerate(zip(values, CONSTANT_SPEC)):
        constant_lines.append(f"    {val:4d},  // [{i:2d}] {name}")

    header_content = f"""\
/*
 * NumOS — STIX Two Math Font Constants
 * Auto-generated by scripts/extract_stix_math.py on {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
 * Source font: {source_font}
 * Font UPM (unitsPerEm): {upm}
 *
 * This file is part of NumOS.
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * DO NOT EDIT MANUALLY — regenerated at build time.
 */

#pragma once

#include <cstdint>

namespace vpam {{

/// STIX Two Math units-per-em (UPM).
/// All design-unit constants in kStixMathConstants are expressed relative to this value.
constexpr int16_t kStixMathUPM = {upm};

/// OpenType MATH table constants extracted from STIX Two Math.
///
/// Array index matches the OpenType spec serialization order (56 entries).
/// All values are in font design units (UPM = {upm}).
/// Scale to pixels by multiplying by (fontSizePx / upm).
///
/// Access patterns (matching Mozilla Gecko's GetMathConstant dispatch):
///   [ 0– 1]  int16  — ScriptPercentScaleDown, ScriptScriptPercentScaleDown
///   [ 2– 3]  uint16 — DelimitedSubFormulaMinHeight, DisplayOperatorMinHeight
///   [ 4–54]  int16  — MathValueRecord.Value (DeviceTable offsets omitted)
///   [55]     uint16 — RadicalDegreeBottomRaisePercent
///
/// Reference: OpenType 1.9.1 spec, MATH table — MathConstants sub-table.
constexpr int16_t kStixMathConstants[{EXPECTED_COUNT}] = {{
{chr(10).join(constant_lines)}
}};

/// Enumeration index into kStixMathConstants for type-safe access.
/// Values match the OpenType MATH table serialization order.
enum class MathConstant : uint8_t {{
    // ── int16 direct (0–1) ──
    ScriptPercentScaleDown                 = 0,
    ScriptScriptPercentScaleDown           = 1,

    // ── uint16 direct (2–3) ──
    DelimitedSubFormulaMinHeight           = 2,
    DisplayOperatorMinHeight               = 3,

    // ── MathValueRecord int16 values (4–54) ──
    MathLeading                            = 4,
    AxisHeight                             = 5,
    AccentBaseHeight                       = 6,
    FlattenedAccentBaseHeight              = 7,
    SubscriptShiftDown                     = 8,
    SubscriptTopMax                        = 9,
    SubscriptBaselineDropMin               = 10,
    SuperscriptShiftUp                     = 11,
    SuperscriptShiftUpCramped              = 12,
    SuperscriptBottomMin                   = 13,
    SuperscriptBaselineDropMax             = 14,
    SubSuperscriptGapMin                   = 15,
    SuperscriptBottomMaxWithSubscript      = 16,
    SpaceAfterScript                       = 17,
    UpperLimitGapMin                       = 18,
    UpperLimitBaselineRiseMin              = 19,
    LowerLimitGapMin                       = 20,
    LowerLimitBaselineDropMin              = 21,
    StackTopShiftUp                        = 22,
    StackTopDisplayStyleShiftUp            = 23,
    StackBottomShiftDown                   = 24,
    StackBottomDisplayStyleShiftDown       = 25,
    StackGapMin                            = 26,
    StackDisplayStyleGapMin                = 27,
    StretchStackTopShiftUp                 = 28,
    StretchStackBottomShiftDown            = 29,
    StretchStackGapAboveMin                = 30,
    StretchStackGapBelowMin                = 31,
    FractionNumeratorShiftUp               = 32,
    FractionNumeratorDisplayStyleShiftUp   = 33,
    FractionDenominatorShiftDown           = 34,
    FractionDenominatorDisplayStyleShiftDown = 35,
    FractionNumeratorGapMin                = 36,
    FractionNumDisplayStyleGapMin          = 37,
    FractionRuleThickness                  = 38,
    FractionDenominatorGapMin              = 39,
    FractionDenomDisplayStyleGapMin        = 40,
    SkewedFractionHorizontalGap            = 41,
    SkewedFractionVerticalGap              = 42,
    OverbarVerticalGap                     = 43,
    OverbarRuleThickness                   = 44,
    OverbarExtraAscender                   = 45,
    UnderbarVerticalGap                    = 46,
    UnderbarRuleThickness                  = 47,
    UnderbarExtraDescender                 = 48,
    RadicalVerticalGap                     = 49,
    RadicalDisplayStyleVerticalGap         = 50,
    RadicalRuleThickness                   = 51,
    RadicalExtraAscender                   = 52,
    RadicalKernBeforeDegree                = 53,
    RadicalKernAfterDegree                 = 54,

    // ── uint16 standalone (55) ──
    RadicalDegreeBottomRaisePercent        = 55,

    COUNT                                  = 56
}};

}}  // namespace vpam
"""

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header_content)

    print(f"  Generated: {output_path}")
    print(f"  Constants: {EXPECTED_COUNT}")
    print(f"  Font UPM:  {upm}")


def _build_reverse_cmap(font: TTFont) -> dict[str, int]:
    """
    Build a reverse mapping from glyph name to Unicode codepoint.

    Uses the cmap's best sub-table (typically format 4 or 12) and also
    parses 'uniXXXX' naming convention as a fallback.
    """
    reverse: dict[str, int] = {}
    best = font["cmap"].getBestCmap()
    if best:
        for cp, glyph_name in best.items():
            if glyph_name not in reverse:
                reverse[glyph_name] = cp
    return reverse


def _glyph_name_to_codepoint(glyph_name: str, reverse_cmap: dict[str, int]) -> int:
    """Resolve a glyph name to a Unicode codepoint."""
    # Direct lookup
    if glyph_name in reverse_cmap:
        return reverse_cmap[glyph_name]
    # 'uniXXXX' naming convention
    if glyph_name.startswith("uni") and len(glyph_name) == 7:
        try:
            return int(glyph_name[3:], 16)
        except ValueError:
            pass
    # 'uXXXXX' naming convention (5+ hex digits)
    if glyph_name.startswith("u") and len(glyph_name) > 4:
        try:
            return int(glyph_name[1:], 16)
        except ValueError:
            pass
    return 0


def extract_italics_correction(font_path: str) -> list[tuple[int, int]]:
    """
    Extract MathItalicsCorrectionInfo from the MATH table.

    Returns sorted list of (codepoint, correction_in_du) tuples.
    Only entries with valid codepoints are included.
    """
    font = TTFont(font_path)

    if "MATH" not in font:
        print("  [WARN] No MATH table — skipping italics correction extraction")
        font.close()
        return []

    math_table = font["MATH"].table
    if not hasattr(math_table, "MathGlyphInfo"):
        print("  [WARN] No MathGlyphInfo — skipping italics correction extraction")
        font.close()
        return []

    mg = math_table.MathGlyphInfo
    if not hasattr(mg, "MathItalicsCorrectionInfo"):
        print("  [WARN] No MathItalicsCorrectionInfo in font")
        font.close()
        return []

    ici = mg.MathItalicsCorrectionInfo
    reverse_cmap = _build_reverse_cmap(font)

    entries: list[tuple[int, int]] = []
    coverage = ici.Coverage
    corrections = ici.ItalicsCorrection

    for i, glyph_name in enumerate(coverage.glyphs):
        cp = _glyph_name_to_codepoint(glyph_name, reverse_cmap)
        if cp == 0:
            continue
        raw = corrections[i]
        val = raw.Value if hasattr(raw, "Value") else int(raw)
        if val < -32768 or val > 32767:
            continue  # shouldn't happen, but be safe
        entries.append((cp, int(val)))

    entries.sort(key=lambda x: x[0])
    font.close()
    return entries


def extract_variants(font_path: str,
                     base_cps: list[int]) -> dict[int, dict]:
    """
    Extract MathVariants (SizeVariants + GlyphAssembly) for a list of
    base Unicode codepoints from the font's MATH table.

    Returns a dict keyed by base codepoint:
      { cp: {
          "baseHeight": int,
          "baseWidth": int,
          "sizeVariants": [(glyphCp, height, width), ...],
          "assemblyParts": [(glyphCp, fullAdvance, glyphWidth,
                             startConnector, endConnector, isExtender), ...],
          "italicsCorrection": int
        }
      }
    """
    font = TTFont(font_path)
    upm = font["head"].unitsPerEm
    best = font["cmap"].getBestCmap()
    reverse_cmap: dict[str, int] = {}
    if best:
        for cp, gn in best.items():
            if gn not in reverse_cmap:
                reverse_cmap[gn] = cp

    def _glyph_name_to_codepoint(glyph_name: str) -> int:
        if glyph_name in reverse_cmap:
            return reverse_cmap[glyph_name]
        if glyph_name.startswith("uni") and len(glyph_name) == 7:
            try:
                return int(glyph_name[3:], 16)
            except ValueError:
                pass
        if glyph_name.startswith("u") and len(glyph_name) > 4:
            try:
                return int(glyph_name[1:], 16)
            except ValueError:
                pass
        return 0

    result: dict[int, dict] = {}

    if "MATH" not in font:
        print("  [WARN] Font has no MATH table — skipping variants extraction")
        font.close()
        return result

    math_table = font["MATH"].table
    if not hasattr(math_table, "MathVariants"):
        print("  [WARN] No MathVariants sub-table — skipping")
        font.close()
        return result

    mv = math_table.MathVariants
    glyph_order = font.getGlyphOrder()

    # Build a mapping: glyph name → index in VertGlyphConstruction
    vert_construction_map: dict[str, int] = {}
    if getattr(mv, "VertGlyphCoverage", None):
        for idx, glyph_name in enumerate(mv.VertGlyphCoverage.glyphs):
            vert_construction_map[glyph_name] = idx

    # Get italics correction info
    italics_map: dict[int, int] = {}
    if hasattr(math_table, "MathGlyphInfo") and math_table.MathGlyphInfo:
        mgi = math_table.MathGlyphInfo
        if hasattr(mgi, "MathItalicsCorrectionInfo") and mgi.MathItalicsCorrectionInfo:
            ici = mgi.MathItalicsCorrectionInfo
            for i, glyph_name in enumerate(ici.Coverage.glyphs):
                cp = _glyph_name_to_codepoint(glyph_name)
                if cp:
                    raw = ici.ItalicsCorrection[i]
                    val = raw.Value if hasattr(raw, "Value") else int(raw)
                    if -32768 <= val <= 32767:
                        italics_map[cp] = int(val)

    for base_cp in base_cps:
        # Find the glyph name for this codepoint
        target_glyph = None
        if best:
            for cp, gn in best.items():
                if cp == base_cp:
                    target_glyph = gn
                    break

        if not target_glyph:
            print(f"  [SKIP] Codepoint U+{base_cp:04X} not found in cmap")
            continue

        if target_glyph not in vert_construction_map:
            # No vertical construction for this glyph — just use base metrics
            try:
                gid = glyph_order.index(target_glyph)
            except ValueError:
                gid = 0
            hmtx = font["hmtx"]
            adv, lsb = hmtx[target_glyph]
            base_height_raw = 0
            # Try to get bounding box from CFF or glyf
            glyf_table = font.get("glyf")
            cff_table = font.get("CFF ")
            if glyf_table and hasattr(glyf_table, target_glyph):
                glyph = glyf_table[target_glyph]
                if glyph and glyph.yMax != glyph.yMin:
                    base_height_raw = glyph.yMax - glyph.yMin
                else:
                    base_height_raw = adv  # fallback
            elif cff_table:
                # CFF charstrings require parsing — use advance as fallback
                base_height_raw = adv
            else:
                base_height_raw = adv

            itl_corr = italics_map.get(base_cp, 0)
            entry = {
                "baseHeight": max(base_height_raw, 700),  # ensure >= reasonable min
                "baseWidth": int(adv),
                "sizeVariants": [],
                "assemblyParts": [],
                "italicsCorrection": itl_corr,
            }
            result[base_cp] = entry
            continue

        constr = mv.VertGlyphConstruction[vert_construction_map[target_glyph]]

        # Base metrics
        hmtx = font["hmtx"]
        adv, _ = hmtx[target_glyph]

        glyf_table = font.get("glyf")
        base_h = adv  # fallback
        if glyf_table and hasattr(glyf_table, target_glyph):
            glyph = glyf_table[target_glyph]
            if glyph and glyph.yMax != glyph.yMin:
                base_h = glyph.yMax - glyph.yMin

        itl_corr = italics_map.get(base_cp, 0)

        # Size variants
        sv_list = []
        for sv in constr.MathGlyphVariantRecord:
            sv_cp = _glyph_name_to_codepoint(sv.VariantGlyph)
            if sv_cp == 0:
                continue
            sv_adv, _ = hmtx[sv.VariantGlyph]
            sv_h = getattr(sv, "AdvanceMeasurement", sv_adv)
            if glyf_table and hasattr(glyf_table, sv.VariantGlyph):
                sv_glyph = glyf_table[sv.VariantGlyph]
                if sv_glyph and sv_glyph.yMax != sv_glyph.yMin:
                    sv_h = max(sv_h, sv_glyph.yMax - sv_glyph.yMin)
            sv_list.append((sv_cp, int(sv_h), int(sv_adv)))

        # Assembly parts (GlyphPartRecord)
        parts = []
        if getattr(constr, "GlyphAssembly", None):
            for part in constr.GlyphAssembly.PartRecords:
                part_cp = _glyph_name_to_codepoint(part.glyph)
                if part_cp == 0:
                    continue
                part_h = int(part.FullAdvance)
                part_w, _ = hmtx[part.glyph]
                start_c = int(part.StartConnectorLength)
                end_c = int(part.EndConnectorLength)
                is_ext = bool(part.PartFlags & 1)  # bit 0 = extender
                parts.append((part_cp, part_h, int(part_w),
                              start_c, end_c, is_ext))

        entry = {
            "baseHeight": max(base_h, 700) if sv_list else int(base_h),
            "baseWidth": int(adv),
            "sizeVariants": sv_list,
            "assemblyParts": parts,
            "italicsCorrection": itl_corr,
        }
        result[base_cp] = entry

    font.close()
    return result


def generate_italics_header(entries: list[tuple[int, int]], upm: int,
                            output_path: str, source_font: str) -> None:
    """Generate C/C++ header with italic correction lookup table."""

    entry_lines = []
    for cp, corr in entries:
        entry_lines.append(f"    {{ 0x{cp:04X}, {corr:4d} }},  // U+{cp:04X}")

    header = f"""\
/*
 * NumOS — STIX Two Math Italics Correction Table
 * Auto-generated by scripts/extract_stix_math.py on {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
 * Source font: {source_font}
 * Font UPM (unitsPerEm): {upm}
 *
 * This file is part of NumOS.
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * DO NOT EDIT MANUALLY — regenerated at build time.
 */

#pragma once

#include <cstdint>
#include <algorithm>

namespace vpam {{

/// Single entry in the italic correction table.
/// Sorted by codepoint for binary search (std::lower_bound).
struct ItalicsCorrectionEntry {{
    uint32_t codepoint;   ///< Unicode scalar value
    int16_t  correction;  ///< Italic correction in font design units (UPM={upm})
}};

/// Total number of italic correction entries.
constexpr uint16_t kItalicsCorrectionCount = {len(entries)};

/// Sorted array of italic correction values keyed by Unicode codepoint.
/// Find corrections via std::lower_bound on the codepoint field.
constexpr ItalicsCorrectionEntry kItalicsCorrectionTable[] = {{
{chr(10).join(entry_lines)}
}};

/// Look up the italic correction for a given Unicode codepoint.
/// Returns the correction in design units, or 0 if not found.
/// O(log N) via binary search.
inline int16_t lookupItalicsCorrection(uint32_t codepoint) {{
    const auto* end = kItalicsCorrectionTable + kItalicsCorrectionCount;
    const auto* it = std::lower_bound(
        kItalicsCorrectionTable, end, codepoint,
        [](const ItalicsCorrectionEntry& e, uint32_t cp) {{
            return e.codepoint < cp;
        }});
    if (it != end && it->codepoint == codepoint)
        return it->correction;
    return 0;
}}

}}  // namespace vpam
"""

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header)

    print(f"  Generated: {output_path}")
    print(f"  Entries:   {len(entries)}")


def generate_variants_header(
        variants: dict[int, dict], upm: int,
        output_path: str, source_font: str) -> None:
    """Generate C/C++ header with MathVariantTable data dynamically extracted."""

    # Determine the base codepoints list in insertion order
    base_cps = list(variants.keys())

    # Build per-table string
    tables = []
    table_names = []

    for idx, base_cp in enumerate(base_cps):
        v = variants[base_cp]
        table_name = f"kVarTable_{base_cp:04X}"

        sv = v["sizeVariants"]
        parts = v["assemblyParts"]

        # ── Detect fake assembly tables ──
        # If all assembly parts reference the same codepoint as the base glyph,
        # the font lacks genuine multi-part construction entries. Using such an
        # assembly would render three copies of the base glyph stacked — a visual
        # artifact. Nullify the assembly in this case.
        if parts and all(p[0] == base_cp for p in parts):
            print(f"  [WARN] U+{base_cp:04X}: skipping fake assembly "
                  f"(all parts = base glyph)")
            parts = []

        if parts:
            extender_cps = [p[0] for p in parts if p[5]]
            non_extender_count = len(parts) - len(extender_cps)
            if (len(extender_cps) != 1 or len(set(extender_cps)) != 1
                    or non_extender_count > 3):
                print(f"  [WARN] U+{base_cp:04X}: skipping assembly outside "
                      f"DelimiterAssembler constraints")
                parts = []
        v["assemblyParts"] = parts

        # Size variant array
        sv_name = f"{table_name}_sv"
        sv_lines = []
        for sv_cp, sv_h, sv_w in sv:
            sv_lines.append(
                f"    {{ 0x{sv_cp:04X}, {sv_h:5d}, {sv_w:5d} }},  // U+{sv_cp:04X}"
            )
        sv_body = "\n".join(sv_lines)

        # Assembly parts array
        parts_name = f"{table_name}_parts"
        parts_lines = []
        for p_cp, p_h, p_w, s_c, e_c, is_ext in parts:
            ext_str = "true" if is_ext else "false"
            parts_lines.append(
                f"    {{ 0x{p_cp:04X}, {p_h:5d}, {p_w:5d}, {s_c:4d}, {e_c:4d}, {ext_str} }},  // U+{p_cp:04X}"
            )
        parts_body = "\n".join(parts_lines)

        # Italics correction
        itl = v["italicsCorrection"]

        # Assemble the table comment and data
        base_h = v["baseHeight"]
        base_w = v["baseWidth"]

        table_text = f"""
// ── U+{base_cp:04X} ─────────────────────────────────────────────────────────
{("inline constexpr SizeVariantRecord " + sv_name + "[] = {").strip()}
{sv_body}
}};

inline constexpr GlyphPartRecord {parts_name}[] = {{
{parts_body}
}};

inline constexpr MathVariantTable {table_name} = {{
    0x{base_cp:04X},          // baseCodepoint
    {base_h},             // baseHeight (DU)
    {base_w},             // baseWidth  (DU)
    {sv_name}, {len(sv)},  // sizeVariants
    {parts_name if parts else "nullptr"}, {len(parts)},  // assemblyParts
    {itl},               // italicsCorrection
}};
"""
        tables.append(table_text)
        table_names.append((base_cp, table_name))

    # Build lookup switch
    switch_cases = []
    for base_cp, tbl_name in table_names:
        switch_cases.append(f"        case 0x{base_cp:04X}: return &{tbl_name};")

    header_content = f"""\
/*
 * NumOS — STIX Two Math Glyph Variant & Assembly Tables
 * Auto-generated by scripts/extract_stix_math.py on {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
 * Source font: {source_font}
 * Font UPM (unitsPerEm): {upm}
 *
 * All values are in font design units (UPM = {upm}).
 * Scale to pixels by multiplying by (fontSizePx / upm).
 *
 * Reference: OpenType 1.9.1, MATH table — MathVariants sub-table.
 */

#pragma once

#include <cstdint>

namespace vpam {{

// ════════════════════════════════════════════════════════════════════════════
// GlyphPartRecord — Single piece in a vertical glyph assembly
//
// OpenType stores assembly parts bottom-to-top. The DelimiterAssembler FSM
// reorders them top-to-bottom for execution in the renderer.
//
// Parts overlap by startConnector / endConnector lengths to eliminate
// visual seams (rasterisation gaps) on low-resolution displays.
// ════════════════════════════════════════════════════════════════════════════
struct GlyphPartRecord {{
    uint32_t glyphCp;            ///< Unicode codepoint
    int16_t  fullAdvance;        ///< Full glyph height in design units
    int16_t  glyphWidth;         ///< Horizontal advance width in design units
    int16_t  startConnector;     ///< Connector length at glyph start (DU)
    int16_t  endConnector;       ///< Connector length at glyph end (DU)
    bool     isExtender;         ///< Repeatable piece that fills the middle
}};

// ════════════════════════════════════════════════════════════════════════════
// SizeVariantRecord — Discrete larger variant of a base glyph
//
// Used when the target height fits within the variant's height without
// needing assembly. Variants are listed in increasing height order.
// ════════════════════════════════════════════════════════════════════════════
struct SizeVariantRecord {{
    uint32_t glyphCp;            ///< Unicode codepoint of the variant
    int16_t  height;             ///< Glyph height in design units
    int16_t  glyphWidth;         ///< Horizontal advance width in design units
}};

// ════════════════════════════════════════════════════════════════════════════
// MathVariantTable — Complete variant data for one delimiter/operator
//
// Each table covers one base glyph and enumerates:
//   1. The base glyph's natural height and width
//   2. Size variants (increasing height) — used before falling back to assembly
//   3. Assembly parts (stored bottom-to-top per OpenType) — used for tall content
// ════════════════════════════════════════════════════════════════════════════
struct MathVariantTable {{
    uint32_t baseCodepoint;          ///< Base glyph (e.g. U+0028 for '(')
    int16_t  baseHeight;             ///< Base glyph height in design units
    int16_t  baseWidth;              ///< Base glyph horizontal advance in design units
    const SizeVariantRecord* sizeVariants;
    uint8_t  sizeVariantCount;
    const GlyphPartRecord*  assemblyParts;
    uint8_t  assemblyPartCount;
    int16_t  italicsCorrection;      ///< MathItalicsCorrection in design units
}};

// ════════════════════════════════════════════════════════════════════════════
// Static Variant Tables — dynamically extracted from font
// ════════════════════════════════════════════════════════════════════════════
{chr(10).join(tables)}
// ════════════════════════════════════════════════════════════════════════════
// Lookup helper — returns the variant table for a known base codepoint
// ════════════════════════════════════════════════════════════════════════════
inline const MathVariantTable* lookupVariantTable(uint32_t baseCp) {{
    switch (baseCp) {{
{chr(10).join(switch_cases)}
        default:     return nullptr;
    }}
}}

}}  // namespace vpam
"""

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header_content)

    print(f"  Generated: {output_path}")
    print(f"  Tables:    {len(variants)}")
    for base_cp, v in variants.items():
        parts = v.get("assemblyParts", [])
        if parts:
            all_same = all(p[0] == base_cp for p in parts)
            if all_same:
                print(f"  [WARN] U+{base_cp:04X}: all assembly parts reference the base glyph "
                      f"— consider removing assembly entries to prevent visual artifact")


def generate_glyph_map_header(glyph_map: list[tuple[int, int]],
                              output_path: str, source_font: str) -> None:
    """[REMOVED] Glyph map generation removed — LVGL's lv_font_get_glyph_dsc
    already handles codepoint→glyph-index mapping efficiently.
    This function exists only as a stub to avoid breaking the main() call chain."""
    print("  [SKIP] stix_math_glyph_map.h — LVGL already handles codepoint→glyph mapping")


def main() -> int:
    # ── Resolve font path ────────────────────────────────────────────────────
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    if len(sys.argv) > 1:
        font_path = Path(sys.argv[1])
    else:
        font_path = repo_root / "assets" / "fonts" / "STIXTwoMath-Regular.ttf"

    font_path = font_path.resolve()

    if not font_path.exists():
        print(f"ERROR: Font file not found: {font_path}", file=sys.stderr)
        print(f"Usage: {sys.argv[0]} [path-to-STIXTwoMath-Regular.ttf]",
              file=sys.stderr)
        return 1

    # ── Output directory ─────────────────────────────────────────────────────
    output_dir = repo_root / "src" / "math" / "font"

    print(f"Source font:  {font_path}")
    print()

    # ── 1. MathConstants ───────────────────────────────────────────────────
    const_path = output_dir / "stix_math_constants.h"
    try:
        values, upm = extract_constants(str(font_path))
        generate_header(values, upm, str(const_path), str(font_path))
    except Exception as e:
        print(f"ERROR extracting constants: {e}", file=sys.stderr)
        return 1

    print()

    # ── 2. MathItalicsCorrectionInfo ───────────────────────────────────────
    ital_path = output_dir / "stix_math_italics.h"
    try:
        ital_entries = extract_italics_correction(str(font_path))
        if ital_entries:
            generate_italics_header(ital_entries, upm, str(ital_path),
                                    str(font_path))
        else:
            print("  [SKIP] No italic correction entries extracted")
    except Exception as e:
        print(f"ERROR extracting italic corrections: {e}", file=sys.stderr)
        print("  (continuing - italic correction is optional)", file=sys.stderr)

    print()

    # ── 3. MathVariants (SizeVariants + GlyphAssembly) ────────────────────────
    variants_path = output_dir / "stix_math_variants.h"
    # Supported delimiters and large operators for glyph assembly
    BASE_CODEPOINTS = [
        0x0028,  # (
        0x0029,  # )
        0x005B,  # [
        0x005D,  # ]
        0x007B,  # {
        0x007D,  # }
        0x007C,  # |
        0x2211,  # ∑
        0x220F,  # ∏
        0x2210,  # ∐
        0x22C0,  # ⋀
        0x22C1,  # ⋁
        0x22C2,  # ⋂
        0x22C3,  # ⋃
        0x222B,  # ∫
    ]
    try:
        variants = extract_variants(str(font_path), BASE_CODEPOINTS)
        if variants:
            generate_variants_header(variants, upm, str(variants_path),
                                     str(font_path))
        else:
            print("  [SKIP] No variant data extracted")
    except Exception as e:
        print(f"ERROR extracting variants: {e}", file=sys.stderr)
        print("  (continuing - variants are optional)", file=sys.stderr)

    print()

    # ── Summary ──────────────────────────────────────────────────────────────
    print("Key constants for reference:")
    key_indices = {
        "AxisHeight": 5, "FractionRuleThickness": 38,
        "SuperscriptShiftUp": 11, "SubscriptShiftDown": 8,
        "RadicalRuleThickness": 51, "RadicalVerticalGap": 49,
        "ScriptPercentScaleDown": 0, "ScriptScriptPercentScaleDown": 1,
    }
    for name, idx in key_indices.items():
        print(f"  {name:35s} = {values[idx]:5d} du  "
              f"({values[idx] * 18 // upm:3d} px @ 18pt)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
