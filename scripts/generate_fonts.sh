#!/usr/bin/env bash
#
# generate_fonts.sh — Build-time font generation for NumOS
#
# Generates LVGL-compatible .c font files from STIX Two Math at three sizes:
#   18 px  — body text / display math
#   12 px  — superscripts, subscripts, script-level
#   8 px   — scriptscript-level (second-order sub/superscripts)
#
# Requires: lv_font_conv (https://github.com/lvgl/lv_font_conv)
#
# Usage:
#   ./scripts/generate_fonts.sh [path-to-STIXTwoMath-Regular.ttf]
#
# Output goes to assets/fonts/ — adjust the destination as needed.
# The .c files are compiled into the firmware and referenced via
# lvgl's LV_FONT_DECLARE / lv_font_t.
#
# Reference ranges (Tier 1+2 math codepoints):
#   0x0020-0x007E   Basic Latin
#   0x00B0-0x00B9   Degree, superscript 1/2/3
#   0x00D7          Multiplication sign
#   0x00F7          Division sign
#   0x0391-0x03C9   Greek
#   0x2102-0x2132   Numbers (ℂℕℙℚℝℤ)
#   0x2200-0x22FF   Mathematical Operators
#   0x2212          Minus sign
#   0x2260-0x228B   Relations
#   0x2308-0x230B   Ceil/Floor
#   0x239B-0x23A6   Delimiter assembly (paren hooks + bracket corners) — MANDATORY for elastic (/{/[
#   0x27C0-0x27EF   Misc math symbols
#   0x27E8-0x27EB   Angle brackets
#   0x2980-0x29FF   Fences
#   0x2A00-0x2AFF   Supplemental Math Ops
#   0x1D400-0x1D7FF Mathematical Alphanumeric Symbols
#   + combining accents (0x0300-0x036F)
#   + superscript/subscript (0x2070-0x209F)
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Resolve font path ──────────────────────────────────────────────────────
if [ $# -ge 1 ]; then
    FONT_PATH="$1"
else
    FONT_PATH="$REPO_ROOT/assets/fonts/STIXTwoMath-Regular.ttf"
fi

if [ ! -f "$FONT_PATH" ]; then
    echo "ERROR: Font not found: $FONT_PATH"
    echo "Usage: $0 [path-to-STIXTwoMath-Regular.ttf]"
    exit 1
fi

OUTPUT_DIR="$REPO_ROOT/assets/fonts"
mkdir -p "$OUTPUT_DIR"

# ── Check for lv_font_conv ─────────────────────────────────────────────────
if ! command -v lv_font_conv &>/dev/null; then
    echo "ERROR: lv_font_conv not found in PATH."
    echo "Install from: https://github.com/lvgl/lv_font_conv"
    echo "  npm install -g lv_font_conv"
    exit 1
fi

# ── Common glyph ranges ────────────────────────────────────────────────────
RANGES=(
    --range 0x0020-0x007E    # Basic Latin
    --range 0x00B0-0x00B9    # Degree, superscript 1/2/3
    --range 0x00D7           # ×
    --range 0x00F7           # ÷
    --range 0x0391-0x03C9    # Greek
    --range 0x2102-0x2132    # ℂℕℙℚℝℤ etc.
    --range 0x2200-0x22FF    # Math Operators
    --range 0x2212           # − (minus sign)
    --range 0x2308-0x230B    # ⌈⌉⌊⌋
    --range 0x239B-0x23A6    # Delimiter assembly parts (paren hooks/extensions AND bracket corners/extensions)
    --range 0x27C0-0x27EF    # Misc math
    --range 0x27E8-0x27EB    # ⟨⟩⟪⟫
    --range 0x2980-0x29FF    # Fences
    --range 0x2A00-0x2AFF    # Supplemental Ops
    --range 0x2260-0x228B    # Relations
    --range 0x0300-0x036F    # Combining accents
    --range 0x2070-0x209F    # Superscripts/subscripts
)

# ── Large math alphanumeric symbols (sparse range, handle separately) ───────
# The 1D400-1D7FF block is large (~1024 glyphs). Include for full rendering.
# Comment out to reduce font size if flash space is tight.
RANGES_EXTRA=(
    --range 0x1D400-0x1D7FF  # Math Alphanumeric Symbols
)

# ════════════════════════════════════════════════════════════════════════════
# Generate fonts at 18, 12, and 8 px
# ════════════════════════════════════════════════════════════════════════════

generate_font() {
    local SIZE="$1"
    local SUFFIX="$2"
    local BPP="${3:-4}"

    local OUTPUT_BASENAME="stix_math_${SIZE}"
    local OUTPUT_C="${OUTPUT_DIR}/${OUTPUT_BASENAME}.c"

    echo "  Generating ${OUTPUT_BASENAME} (${SIZE}px, ${BPP}bpp)..."

    # Build the full set of ranges for this size
    local CMD_RANGES=("${RANGES[@]}")
    if [ "$SIZE" -ge 12 ]; then
        # Only include the big alphanumeric block at 18px and 12px
        CMD_RANGES+=("${RANGES_EXTRA[@]}")
    fi

    lv_font_conv \
        --font "$FONT_PATH" \
        --size "${SIZE}" \
        "${CMD_RANGES[@]}" \
        --bpp "${BPP}" \
        --output "${OUTPUT_C}" \
        --format lvgl

    echo "    → ${OUTPUT_C}"
}

echo "Source font: ${FONT_PATH}"
echo "Output dir:  ${OUTPUT_DIR}"
echo ""

generate_font 18 "body"  4
generate_font 12 "script" 4
generate_font 8 "scriptscript" 2

echo ""
echo "Done. Add the generated .c files to your build_src_filter in platformio.ini."
