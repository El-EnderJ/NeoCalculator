# NumOS Glyph Inventory

Phase 5 inventory for the STIX Two Math dynamic layout pipeline.

## Generated Font Subset

| Asset | Size | Encoded glyphs | Notes |
| --- | ---: | ---: | --- |
| `src/fonts/stix_math_18.c` | 1,459,925 bytes (1425.7 KiB) | 2,504 | Primary display math font, 4 bpp |
| `src/fonts/stix_math_12.c` | 873,788 bytes (853.3 KiB) | 2,504 | Script font, 4 bpp |
| `src/fonts/stix_math_8.c` | 474,737 bytes (463.6 KiB) | 2,504 | Scriptscript font, 2 bpp |

Total generated LVGL font source footprint: 2,808,450 bytes (2742.6 KiB).

The current generated files expose 2,504 encoded codepoints per size through 31 LVGL cmaps. This is the actual generated state, derived from the cmap tables in the C files, and supersedes older generator comments that estimated about 769 glyphs.

## MATH Data Tables

| Asset | Size | Inventory |
| --- | ---: | --- |
| `src/math/font/stix_math_constants.h` | 6,968 bytes | 56 OpenType MATH constants |
| `src/math/font/stix_math_variants.h` | 16,464 bytes | 15 variant tables, 15 size variants, 12 assembly parts |
| `src/math/font/stix_math_italics.h` | 15,954 bytes | 413 italics-correction entries |

Total MATH metadata source footprint: 39,386 bytes (38.5 KiB).

Combined generated font plus MATH metadata source footprint: 2,847,836 bytes (2781.1 KiB).

## Removed Dead Glyph Map

The old parallel glyph map is no longer generated. `scripts/extract_stix_math.py` leaves `generate_glyph_map()` as a stub and relies on LVGL glyph lookup plus the dedicated MATH tables instead.

Removed map saving: approximately 12,288 bytes (12.0 KiB).

Net source footprint after the removed map saving: approximately 2,835,548 bytes (2769.1 KiB), before compiler/linker section alignment.

## Lookup Path

- Normal glyph metrics/rendering: `lv_font_get_glyph_dsc()` and `lv_draw_letter()` over the generated LVGL cmap tables.
- Elastic delimiter/operator data: direct `switch` over 15 base codepoints in `lookupVariantTable()`, O(1), no heap.
- Italic correction: binary search over 413 sorted entries in `lookupItalicsCorrection()`, at most 9 comparisons.
- Inter-atom spacing: constexpr 8x8 `kSpacingTable` lookup, O(1), no heap.

## Phase 5 Stress Coverage

The runtime stress catalog in `src/math/MathStressExpressions.cpp` contains 17 expressions. It covers the quadratic formula, display/text definite integrals with internal fractions, nested exponents reaching scriptscript metrics, radicals, fractions, summations, products/unions, and tall bracket/brace/bar delimiter assemblies.
