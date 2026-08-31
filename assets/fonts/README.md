# Bundled font provenance

The binary fonts in this directory are Font Software under the SIL Open Font
License 1.1 (`OFL-1.1`). They are not relicensed under NumOS's GPL. Full,
upstream-supplied licence and copyright notices are in `LICENSES/`.

## Montserrat

- Local file: `Montserrat-Regular.ttf`
- Family / style: Montserrat Regular
- Embedded version: `Version 9.000`
- PostScript name: `Montserrat-Regular`
- SHA-256: `ef9cf99f0175bef530b88934bd904fcf56f773cec6fd4dfb8ccaf7ce2bbd395e`
- Embedded copyright: `Copyright 2011 The Montserrat Project Authors
  (https://github.com/JulietaUla/Montserrat)`
- Upstream project: <https://github.com/JulietaUla/Montserrat>
- Licence: `OFL-1.1`; see `LICENSES/Montserrat-OFL-1.1.txt`
- Repository history: introduced unchanged in commit `9ad3342346b2ae9e86a90c97229228a69cc22d48`
  on 2026-04-07 and not subsequently modified.

The binary metadata establishes version 9.000, but the exact pre-import
download source cannot be recovered from repository history. It does not
match the static TTF in upstream tag `v9.000` (tag commit
`f1f4e9b630b6ac52f1c55c572b29d2a5c3535139`; static-TTF SHA-256
`28eb076f1a16cd730ed6c8fe2390383bf85ef9353a798850bc4353191aaa5752`).
No claim of an exact upstream release artifact is therefore made. The adjacent
notice reproduces the copyright and OFL grant embedded in the local binary,
followed by the canonical OFL 1.1 text; it is not presented as proof of an
exact download source.

Derived files:

- `src/fonts/lv_font_montserrat_math_12.c`
- `src/fonts/lv_font_montserrat_math_14.c`

Regenerate them with `bash scripts/generate_montserrat_math_font.sh`.

## STIX Two Math

- Local file: `STIXTwoMath-Regular.ttf`
- Family / style: STIX Two Math Regular
- Embedded version: `Version 2.12 b168a`
- PostScript name: `STIXTwoMath-Regular`
- SHA-256: `562551b15b836e6e01d1b7350909baf3c8c8d83260c1190fbf4544333e6936de`
- Copyright: `Copyright 2001-2021 The STIX Fonts Project Authors
  (https://github.com/stipub/stixfonts)`
- Vendor / designer: Tiro Typeworks Ltd; Ross Mills, John Hudson, Paul
  Hanslow, with prior portions by MicroPress Inc. and Coen Hoffman
- Upstream project: <https://github.com/stipub/stixfonts>
- Exact distribution source: Google Fonts commit
  `76d97cb481958f9bb0976a453f8126f8e2ea87ab`, path
  `ofl/stixtwomath/STIXTwoMath-Regular.ttf`
- Upstream STIX release: `v2.12` (`02b4b9b6093e2c5d6379b935ea340ea40f7e863b`)
- Licence: `OFL-1.1`; see `LICENSES/STIXTwoMath-OFL-1.1.txt`
- Reserved Font Name notice: the upstream OFL file reserves `TM Math`.
- Repository history: introduced unchanged in commit `64fa431443adbef78fb2f4aa48a294010b1bd8e3`
  on 2026-05-06 and not subsequently modified.

The local binary is byte-for-byte identical to the Google Fonts file at the
commit above. It differs from the STIX project's tag artifact only in font name
table packaging; the embedded version, glyph count, cmap, and substantive font
tables identify STIX Two Math 2.12 b168a.

Derived files:

- `src/fonts/stix_math_8.c`, `stix_math_12.c`, `stix_math_18.c`
- `src/math/font/stix_math_constants.h`
- `src/math/font/stix_math_italics.h`
- `src/math/font/stix_math_variants.h`

Regenerate the LVGL raster data with
`bash scripts/generate_stix_math_font.sh`. Regenerate the OpenType MATH tables
with `python scripts/extract_stix_math.py`.

## Licensing of generated data

The LVGL C fonts are converted/subsetted Font Software. Their bitmaps, cmaps,
glyph metrics, and other converted font data are OFL-derived; the remaining
tool-emitted declarations are generic generation boilerplate. No independent
NumOS-authored implementation is embedded in those files, so each whole output
is conservatively marked `OFL-1.1`.

The three OpenType MATH headers are mixed works. Their numeric constants,
codepoint/value records, variant advances, and assembly connector data are
mechanically extracted from STIX Two Math. Their C++ types, bounded arrays,
lookup functions/switches, namespace structure, and explanatory integration
comments are NumOS-authored scaffolding. Some extracted values may be
non-creative facts, but the audit does not rely on that conclusion: applying
both sets of obligations conservatively yields
`GPL-3.0-or-later AND OFL-1.1`. The expression does not dual-license the font,
the scaffolding, or unrelated NumOS code.
