#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

# ═══════════════════════════════════════════════════════════════════════════════
# generate_stix_math_font.sh — STIX Two Math font subsetting for LVGL / NumOS
# ═══════════════════════════════════════════════════════════════════════════════
#
# Generates LVGL-compatible .c font files from STIXTwoMath-Regular.ttf at
# three math sizes (18pt display, 12pt script, 8pt scriptscript).
#
# Requirements:
#   npm i -g lv_font_conv
#
# Usage:
#   ./scripts/generate_stix_math_font.sh [path-to-STIXTwoMath-Regular.ttf]
#   ./scripts/generate_stix_math_font.sh [path-to-STIXTwoMath-Regular.ttf] [bpp]
#
#   STIX_SIZE=12 ./scripts/generate_stix_math_font.sh ...   # single size
#   STIX_SIZE=all ./scripts/generate_stix_math_font.sh ...   # all sizes (default)
#
# Examples:
#   ./scripts/generate_stix_math_font.sh
#   ./scripts/generate_stix_math_font.sh "C:/.../STIXTwoMath-Regular.ttf"
#   ./scripts/generate_stix_math_font.sh "C:/.../STIXTwoMath-Regular.ttf" 4
#   STIX_BPP=1 STIX_SIZE=8 ./scripts/generate_stix_math_font.sh ...
#
# Output (default):
#   src/fonts/stix_math_18.c   — display/text style  (18pt, bpp=4)
#   src/fonts/stix_math_12.c   — script style        (12pt, bpp=4)
#   src/fonts/stix_math_8.c    — scriptscript style   ( 8pt, bpp=2)
#
# Unicode coverage: Tier 1 + Tier 2 from NumOS Math Engine Spec §5.2
#   Tier 1 (~502 glyphs): Basic Latin, Greek, Math Operators, Ceil/Floor,
#                          Supplemental Math Ops, Double-struck sets
#   Tier 2 (~267 glyphs): Math Alphanumerics (subset), Arrows, Integrals,
#                          Relations, n-ary Operators, Angle Brackets
#   Total: ~769 glyphs, ~24% of full STIX Two Math
# ═══════════════════════════════════════════════════════════════════════════════

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FONT_FILE="${1:-assets/fonts/STIXTwoMath-Regular.ttf}"

cd "${ROOT_DIR}"

if command -v python3 >/dev/null 2>&1; then
  PYTHON_CMD=python3
elif command -v python >/dev/null 2>&1; then
  PYTHON_CMD=python
else
  PYTHON_CMD=python.exe
fi

# ── Normalize Windows paths to POSIX (for bash under WSL/Git Bash/MSYS2) ─────
if [[ "${FONT_FILE}" =~ ^([A-Za-z]):[\\/](.*)$ ]]; then
  drive_letter="${BASH_REMATCH[1],,}"
  path_tail="${BASH_REMATCH[2]//\\//}"
  FONT_FILE="/mnt/${drive_letter}/${path_tail}"
fi

# ── Font size selection ──────────────────────────────────────────────────────
SIZE_MODE="${STIX_SIZE:-all}"

case "${SIZE_MODE}" in
  all)   SIZES_TO_GEN=(18 12 8) ;;
  18)    SIZES_TO_GEN=(18)       ;;
  12)    SIZES_TO_GEN=(12)       ;;
  8)     SIZES_TO_GEN=(8)        ;;
  *)
    echo "Invalid STIX_SIZE '${SIZE_MODE}'. Allowed: 18, 12, 8, all" >&2
    exit 1
    ;;
esac

# ── BPP (bit-per-pixel) selection ────────────────────────────────────────────
BPP_RAW="${2:-${STIX_BPP:-4}}"
case "${BPP_RAW}" in
  1|2|3|4|8) ;;
  *)
    echo "Invalid BPP '${BPP_RAW}'. Allowed values: 1,2,3,4,8" >&2
    exit 1
    ;;
esac

# Per-size BPP overrides (smaller sizes can use fewer bits to save flash)
declare -A SIZE_BPP
SIZE_BPP[18]="${STIX_BPP_18:-${BPP_RAW}}"
SIZE_BPP[12]="${STIX_BPP_12:-${BPP_RAW}}"
SIZE_BPP[8]="${STIX_BPP_8:-2}"   # 8pt scriptscript defaults to 2bpp for size

# ── Validate font exists ─────────────────────────────────────────────────────
if [[ ! -f "${FONT_FILE}" ]]; then
  echo "Missing font file: ${FONT_FILE}" >&2
  echo "Pass STIXTwoMath-Regular.ttf path as first argument." >&2
  exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════════
# Unicode Range Definitions — Tier 1 + Tier 2 from NumOS Math Engine Spec §5.2
# ═══════════════════════════════════════════════════════════════════════════════

# ── Tier 1: Core Arithmetic & Algebra (MUST HAVE) ────────────────────────────
TIER1_BASIC_LATIN="0x0021-0x007E"        # 94 glyphs: digits, letters, operators
TIER1_LATIN1_SUPP="0x00B0-0x00FF"        # °, ±, ², ³, ´, µ, ¶, ·, ¹, º, », ¼-¾, ×, ÷
TIER1_GREEK="0x0391-0x03C9"              # ~70 glyphs: Greek upper/lower (Α–Ω, α–ω)
TIER1_DOUBLESTRUCK="0x2102,0x2115,0x2119,0x211A,0x211D,0x2124"  # ℂ,ℕ,ℙ,ℚ,ℝ,ℤ
TIER1_MATH_OPS="0x2200-0x22FF"           # ~256: ∀∃∈∉⊂⊆∪∩∫∂∇√∞∼≈≤≥≠±⊕⊗⊥∠∧∨¬→←⇒
TIER1_CEIL_FLOOR="0x2308-0x230B"         # 4: ⌈⌉⌊⌋
TIER1_SUPP_MATH="0x2A00-0x2AFF"          # ~60: ⨀⨁⨂, large operators, etc.

TIER1_RANGES="${TIER1_BASIC_LATIN},${TIER1_LATIN1_SUPP},${TIER1_GREEK},${TIER1_DOUBLESTRUCK},${TIER1_MATH_OPS},${TIER1_CEIL_FLOOR},${TIER1_SUPP_MATH}"

# ── Tier 2: Calculus & Advanced Notation (SHOULD HAVE) ───────────────────────
TIER2_MATH_ALPHANUM="0x1D400-0x1D7FF"    # ~150: Math Alphanumeric (italic, bold, script, Fraktur)
TIER2_ANGLE_BRACKETS="0x27E8-0x27EB"     # 4: ⟨⟩ (Mathematical angle brackets)
TIER2_NARY_OPS="0x2210-0x2213"           # 4: ∐∏∑ (already in Math Ops but explicit)
TIER2_INTEGRALS="0x222B-0x2233"          # ~15: ∫∬∭∮∯∰∱∲∳ (single→quadruple + variants)
TIER2_RELATIONS="0x2260-0x228B"          # ~40: ≠≡≤≥⊂⊃⊆⊇ (already in Math Ops)
TIER2_ARROWS="0x2190-0x21FF"             # ~50: Arrows (all directions)

TIER2_RANGES="${TIER2_MATH_ALPHANUM},${TIER2_ANGLE_BRACKETS},${TIER2_NARY_OPS},${TIER2_INTEGRALS},${TIER2_RELATIONS},${TIER2_ARROWS}"

# ── Extended coverage (supplementary, already partially covered above) ───────
EXTENDED_RANGES="0x2000-0x206F,0x2070-0x209F,0x2100-0x214F,0x25A0-0x25FF,0x27C0-0x27EF,0x2900-0x297F,0x2980-0x29FF,0x2B00-0x2BFF"

# ── Combined ──────────────────────────────────────────────────────────────────
ALL_RANGES="${TIER1_RANGES},${TIER2_RANGES},${EXTENDED_RANGES}"

# ═══════════════════════════════════════════════════════════════════════════════
# Explicit Symbol Inventory — ensures auditability of every glyph
# ═══════════════════════════════════════════════════════════════════════════════

SYMBOLS_CALC="∇∂∞∆∫∬∭∮∯∑∏√∛∜"                # Calculus operators
SYMBOLS_LOGIC="∀∃∄∴∵⇒⇔¬∧∨∈∉⊂⊆∪∩∖∅≡≢⊄⊅⊇⊈⊊⊋"  # Logic & set theory
SYMBOLS_ARROWS="→←↔↑↓↕↖↗↘↙↦↩↪⇐⇑⇓⇕"             # Arrows (explicit)
SYMBOLS_RELATIONS="≤≥≠±∓×⊗⊥∥∠≅∼≈≉≊≋≌≍≎≏≐≑≒≓≔≕≖≗≘≙≚≛≜≝≞≟≠≡≢≣"  # Relations
SYMBOLS_GREEK="αβγδεζηθικλμνξπρστυφχψωΑΒΓΔΕΖΗΘΙΚΛΜΝΞΠΡΣΤΥΦΧΨΩ"  # Full Greek
SYMBOLS_SPECIAL="ℕℤℚℝℂℍℵℏ"                     # Double-struck / special sets
SYMBOLS_BRACKETS="⌊⌋⌈⌉⟨⟩⟦⟧⦃⦄"                   # Fences
SYMBOLS_MISC="†∗⋘⋙∘□△†‡•…⋮⋯⋰⋱′″‴⁗"             # Misc technical

SYMBOLS="${SYMBOLS_CALC}${SYMBOLS_LOGIC}${SYMBOLS_ARROWS}${SYMBOLS_RELATIONS}${SYMBOLS_GREEK}${SYMBOLS_SPECIAL}${SYMBOLS_BRACKETS}${SYMBOLS_MISC}"

# ═══════════════════════════════════════════════════════════════════════════════
# Generation function
# ═══════════════════════════════════════════════════════════════════════════════

generate_font() {
  local size="$1"
  local bpp="$2"
  local out_file="src/fonts/stix_math_${size}.c"

  echo ""
  echo "═══ Generating STIX Two Math ${size}pt (bpp=${bpp}) ═══"
  echo "  Output: ${out_file}"

  # Run lv_font_conv with combined ranges
  if lv_font_conv \
      --font "${FONT_FILE}" \
      --size "${size}" \
      --bpp "${bpp}" \
      --format lvgl \
      --range "${ALL_RANGES}" \
      --symbols "${SYMBOLS}" \
      --no-compress \
      -o "${out_file}"; then
    echo "  ✓ Conversion succeeded"
  else
    echo "  ✗ Conversion FAILED (exit code $?)" >&2
    return 1
  fi

  # Patch PlatformIO include path
  sed -i 's|#include "lvgl/lvgl.h"|#include "lvgl.h"|g' "${out_file}"
  "${PYTHON_CMD}" scripts/add_font_provenance_header.py stix "${out_file}"
  echo "  ✓ Include path patched for PlatformIO"

  # Report glyph count from generated file
  local glyph_count
  glyph_count=$(grep -c '\.bitmap' "${out_file}" 2>/dev/null || echo "?")
  echo "  Glyphs: ~${glyph_count}"

  # Report file size
  local file_size
  file_size=$(wc -c < "${out_file}" 2>/dev/null || echo "?")
  echo "  Size:   ${file_size} bytes"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════════

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  STIX Two Math → LVGL Font Generator for NumOS               ║"
echo "║  Font:  ${FONT_FILE}"
echo "║  Sizes: ${SIZES_TO_GEN[*]}"
echo "║  BPP:   ${BPP_RAW} (with per-size overrides)"
echo "╚═══════════════════════════════════════════════════════════════╝"

mkdir -p "${ROOT_DIR}/src/fonts"

for sz in "${SIZES_TO_GEN[@]}"; do
  generate_font "${sz}" "${SIZE_BPP[$sz]}"
done

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  Generation complete!                                        ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Generated files:"
for sz in "${SIZES_TO_GEN[@]}"; do
  out="${ROOT_DIR}/src/fonts/stix_math_${sz}.c"
  if [[ -f "${out}" ]]; then
    sz_kb=$(du -k "${out}" 2>/dev/null | cut -f1 || echo "?")
    echo "  src/fonts/stix_math_${sz}.c  (${sz_kb} KB)"
  fi
done
echo ""
echo "Next steps:"
echo "  1. Run:  python scripts/extract_stix_math.py"
echo "  2. Run:  pio run -e esp32s3_n16r8"
