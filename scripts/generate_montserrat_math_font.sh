#!/usr/bin/env bash
set -euo pipefail

# Generate extended Montserrat fonts for LVGL with broad math coverage.
# Requires: npm i -g lv_font_conv
#
# Output:
#   src/fonts/lv_font_montserrat_math_12.c
#   src/fonts/lv_font_montserrat_math_14.c
#
# Unicode coverage includes:
# - Latin basic + punctuation
# - Greek upper/lower (full block)
# - Mathematical Operators, Arrows, Letterlike Symbols
# - Superscripts/subscripts, Geometric Shapes, misc technical symbols
# - Additional explicit symbols from NeoCalculator master list

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FONT_FILE="${ROOT_DIR}/assets/fonts/Montserrat-Regular.ttf"
OUT_DIR="${ROOT_DIR}/src/fonts"

mkdir -p "${OUT_DIR}"

if [[ ! -f "${FONT_FILE}" ]]; then
  echo "Missing font file: ${FONT_FILE}" >&2
  echo "Place Montserrat-Regular.ttf at assets/fonts/Montserrat-Regular.ttf" >&2
  exit 1
fi

# Core ranges:
# 0020-007E   Basic Latin
# 00A0-00FF   Latin-1 supplement (includes ±, ×, ÷, °)
# 0370-03FF   Greek and Coptic
# 2070-209F   Superscripts and subscripts
# 2100-214F   Letterlike symbols (ℕ ℤ ℚ ℝ ℂ ℍ ℵ)
# 2190-21FF   Arrows
# 2200-22FF   Mathematical operators
# 2300-23FF   Misc technical (⌈⌉⌊⌋)
# 25A0-25FF   Geometric shapes (□ △)
# 27C0-27EF   Misc math symbols-A
RANGES="0x20-0x7E,0xA0-0xFF,0x370-0x3FF,0x2070-0x209F,0x2100-0x214F,0x2190-0x21FF,0x2200-0x22FF,0x2300-0x23FF,0x25A0-0x25FF,0x27C0-0x27EF"

# Explicit symbols required by math list that may live outside selected ranges.
# Grouped for auditability:
SYMBOLS_GREEK=""
SYMBOLS_CALC="∇∂∞∆∫∬∭∮∯∑∏√"
SYMBOLS_LOGIC_SETS="∀∃∄∴∵⇒⇔¬∧∨∈∉⊂⊆∪∩∖∅≡≢"
SYMBOLS_ARROWS="→←↔"
SYMBOLS_ALG_GEO="⊕∝≤≥≠±∓×⊗⊥∥∠≅∼≈°△"
SYMBOLS_DERIVATIVES="′″‴"
SYMBOLS_SPECIAL_SETS="ℏℕℤℚℝℂℍℵ"
SYMBOLS_BRACKETS_PROOF="⌊⌋⌈⌉†∗⋘⋙∘□"
SYMBOLS="${SYMBOLS_GREEK}${SYMBOLS_CALC}${SYMBOLS_LOGIC_SETS}${SYMBOLS_ARROWS}${SYMBOLS_ALG_GEO}${SYMBOLS_DERIVATIVES}${SYMBOLS_SPECIAL_SETS}${SYMBOLS_BRACKETS_PROOF}"

gen_font() {
  local size="$1"
  local output="$2"
  lv_font_conv \
    --font "${FONT_FILE}" \
    --size "${size}" \
    --bpp 4 \
    --format lvgl \
    --range "${RANGES}" \
    --symbols "${SYMBOLS}" \
    --no-compress \
    --full-info \
    -o "${output}"
}

gen_font 12 "${OUT_DIR}/lv_font_montserrat_math_12.c"
gen_font 14 "${OUT_DIR}/lv_font_montserrat_math_14.c"

cat <<EOF
Generated:
  ${OUT_DIR}/lv_font_montserrat_math_12.c
  ${OUT_DIR}/lv_font_montserrat_math_14.c

Example quick command without script:
  lv_font_conv --font "${FONT_FILE}" --size 14 --bpp 4 --format lvgl \\
    --range "${RANGES}" --symbols "${SYMBOLS}" -o "${OUT_DIR}/lv_font_montserrat_math_14.c"
EOF
