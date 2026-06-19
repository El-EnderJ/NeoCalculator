# NumOS Math Engine Specification

**Version:** 1.0 — Deep Research Synthesis  
**Date:** 2026-05-30  
**Status:** Architecture Blueprint  
**Context:** Refactoring NumOS (ESP32-S3, LVGL 9.5) to a full TeX/OpenType-compliant Math Engine using STIX Two Math

---

## Table of Contents

1. [Research Methodology](#1-research-methodology)
2. [OpenType MATH Table Architecture](#2-opentype-math-table-architecture)
3. [TeX Box Model & Atom Types](#3-tex-box-model--atom-types)
4. [Glyph Assembly Engine](#4-glyph-assembly-engine)
5. [Font Subsetting Strategy](#5-font-subsetting-strategy)
6. [MathAST Class Hierarchy — TeX-Refactored](#6-mathast-class-hierarchy--tex-refactored)
7. [MathRenderer — MATH Table-Driven Spacing](#7-mathrenderer--math-table-driven-spacing)
8. [Implementation Roadmap](#8-implementation-roadmap)

---

## 1. Research Methodology

This specification synthesizes findings from a 104-agent deep research workflow across 22 primary sources, with 3-vote adversarial verification on 25 claims. 13 claims survived (confidence ≥ 2/3), 12 were refuted.

**Key primary sources:**

| Source | Role |
|:-------|:-----|
| [Microsoft OpenType 1.9.1 MATH spec](https://learn.microsoft.com/en-gb/typography/opentype/otspec190/math) | Canonical MATH table format |
| [Mozilla Gecko gfxMathTable.h](https://webkit.crisal.io/mozilla-esr45/source/gfx/thebes/gfxMathTable.h) | Reference C++ implementation |
| [Mozilla OTS math.cc validator](https://phabricator.services.mozilla.com/source/mozilla-central/browse/GECKO440b8_2016011121_RELBRANCH/gfx/ots/src/math.cc) | Binary validation logic |
| [LyX MathClass.h](https://www.lyx.org/trac/browser/features/src/mathed/MathClass.h) | Canonical TeX atom type → spacing table |
| [lv_font_conv README](https://github.com/lvgl/lv_font_conv/blob/master/README.md) | LVGL font subsetting capabilities |
| [Noto Math building guide](https://github.com/notofonts/math/blob/main/documentation/building-math-fonts/index.md) | Font authoring constraints |
| [BachoTEX 2009](https://www.gust.org.pl/bachotex/2009/conference-materials/b2009inside.pdf) | MATH table ↔ TeX Appendix G mapping |

**Gap acknowledged:** No surviving claim addressed C++ MathAST class hierarchy design patterns for embedded CAS/renderer dual-use. Section 6 fills this gap with first-principles architecture grounded in the existing NumOS codebase.

---

## 2. OpenType MATH Table Architecture

### 2.1 Binary Layout

The OpenType `MATH` table header is exactly **10 bytes**:

```
Offset  Size  Field
0       4     MajorVersion (uint32, must be 0x00010000)
4       2     MathConstants offset (Offset16 → within 64 KB of table origin)
6       2     MathGlyphInfo offset  (Offset16)
8       2     MathVariants offset   (Offset16)
```

**Critical constraint:** All three sub-table offsets are `Offset16` (not `Offset32`). This means every sub-table must start within **64 KB** of the MATH table origin. After font subsetting, if the MATH table is relocated or split, these offsets must be recalculated.

### 2.2 MathConstants Sub-Table — 56 Constants

The `MathConstants` sub-table defines exactly **56 layout constants** organized in three range-bucketed typed arrays:

| Range | Constants | Type | Access Pattern |
|:------|:----------|:-----|:---------------|
| 0–1 | `ScriptPercentScaleDown`, `ScriptScriptPercentScaleDown` | `int16[2]` | Direct index |
| 2–3 | `DelimitedSubFormulaMinHeight`, `DisplayOperatorMinHeight` | `uint16[2]` | Offset from base |
| 4–54 | 51 `MathValueRecord` entries (each: int16 Value + uint16 DeviceTable offset) | `MathValueRecord[51]` | Offset from base |
| 55 | `RadicalDegreeBottomRaisePercent` | standalone `uint16` | Fallthrough scalar |

**Mozilla Gecko's GetMathConstant() dispatch** (the reference implementation):

```cpp
int16_t GetMathConstant(gfxFontEntry*, MathConstant constant) {
    uint32_t idx = static_cast<uint32_t>(constant);  // ordered enum → positional index
    if (idx <= 1)  return mInt16[idx];               // ScriptPercentScaleDown, ScriptScriptPercentScaleDown
    if (idx <= 3)  return mUint16[idx - 2];           // DelimitedSubFormulaMinHeight, DisplayOperatorMinHeight
    if (idx <= 54) return mMathValues[idx - 4].value; // 51 MathValueRecords
    return mRadicalDegreeBottomRaisePercent;           // constant 55
}
```

**Design implication for NumOS:** We can replicate this with a `constexpr` array of 56 `int16_t` values, populated at build time from extracted font data, enabling O(1) lookup with zero branches.

### 2.3 Style-Aware Constant Pairs

The MATH table defines **separate display-style and inline-style constant pairs** across at least 8 parameter groups, systematically matching TeX's σ₈–σ₁₂ parameters from Appendix G:

| Parameter Group | Display-Style Constant | Inline-Style Constant | TeX σ mapping |
|:----------------|:-----------------------|:----------------------|:--------------|
| Fraction numerator shift | `FractionNumeratorDisplayStyleShiftUp` | `FractionNumeratorShiftUp` | σ₈ |
| Fraction denominator shift | `FractionDenominatorDisplayStyleShiftDown` | `FractionDenominatorShiftDown` | σ₉ |
| Fraction numerator gap min | `FractionNumDisplayStyleGapMin` | `FractionNumeratorGapMin` | σ₈ |
| Fraction denominator gap min | `FractionDenomDisplayStyleGapMin` | `FractionDenominatorGapMin` | σ₉ |
| Stack top shift | `StackTopDisplayStyleShiftUp` | `StackTopShiftUp` | σ₁₀ |
| Stack bottom shift | `StackBottomDisplayStyleShiftDown` | `StackBottomShiftDown` | σ₁₁ |
| Stack gap min | `StackDisplayStyleGapMin` (7× rule thickness) | `StackGapMin` (3× rule thickness) | — |
| Radical vertical gap | `RadicalDisplayStyleVerticalGap` | `RadicalVerticalGap` | σ₁₂ |
| Superscript shift | `SuperscriptShiftUp` (cramped variant exists) | `SuperscriptShiftUp` | σ₁₃/σ₁₄ |
| Subscript shift | `SubscriptShiftDown` | `SubscriptShiftDown` | σ₁₆/σ₁₇ |

**Design implication:** Any compliant engine must maintain **two style-specific FontMetrics sets** (display + inline) and select based on current math style context. A third "cramped" variant exists for superscripts inside radicals.

### 2.4 Complete MathConstant Enum

The 56-constant ordered enum (matching OpenType spec serialization order):

```
 0: ScriptPercentScaleDown
 1: ScriptScriptPercentScaleDown
 2: DelimitedSubFormulaMinHeight
 3: DisplayOperatorMinHeight
 4: MathLeading
 5: AxisHeight
 6: AccentBaseHeight
 7: FlattenedAccentBaseHeight
 8: SubscriptShiftDown
 9: SubscriptTopMax
10: SubscriptBaselineDropMin
11: SuperscriptShiftUp
12: SuperscriptShiftUpCramped
13: SuperscriptBottomMin
14: SuperscriptBaselineDropMax
15: SubSuperscriptGapMin
16: SuperscriptBottomMaxWithSubscript
17: SpaceAfterScript
18: UpperLimitGapMin
19: UpperLimitBaselineRiseMin
20: LowerLimitGapMin
21: LowerLimitBaselineDropMin
22: StackTopShiftUp
23: StackTopDisplayStyleShiftUp
24: StackBottomShiftDown
25: StackBottomDisplayStyleShiftDown
26: StackGapMin
27: StackDisplayStyleGapMin
28: StretchStackTopShiftUp
29: StretchStackBottomShiftDown
30: StretchStackGapAboveMin
31: StretchStackGapBelowMin
32: FractionNumeratorShiftUp
33: FractionNumeratorDisplayStyleShiftUp
34: FractionDenominatorShiftDown
35: FractionDenominatorDisplayStyleShiftDown
36: FractionNumeratorGapMin
37: FractionNumDisplayStyleGapMin
38: FractionRuleThickness
39: FractionDenominatorGapMin
40: FractionDenomDisplayStyleGapMin
41: SkewedFractionHorizontalGap
42: SkewedFractionVerticalGap
43: OverbarVerticalGap
44: OverbarRuleThickness
45: OverbarExtraAscender
46: UnderbarVerticalGap
47: UnderbarRuleThickness
48: UnderbarExtraDescender
49: RadicalVerticalGap
50: RadicalDisplayStyleVerticalGap
51: RadicalRuleThickness
52: RadicalExtraAscender
53: RadicalKernBeforeDegree
54: RadicalKernAfterDegree
55: RadicalDegreeBottomRaisePercent
```

### 2.5 MathItalicsCorrection Sub-Table

The `MathGlyphInfo` sub-table contains `MathItalicsCorrectionInfo` — a coverage table mapping glyph IDs to italic correction values (used for placing superscripts after tall/italic glyphs like `f`). NumOS should extract this into a **sorted array of `{glyphID, correction}` pairs** for binary search at render time.

---

## 3. TeX Box Model & Atom Types

### 3.1 Eight-Atom Classification

LyX's `MathClass.h` defines exactly 8 atom types (collapsing TeX's 13 internal types by treating Over, Under, Acc, Rad, and Vcent as `Ord`):

```cpp
enum class MathClass : uint8_t {
    MC_ORD    = 0,  // Ordinary: variables, digits, constants
    MC_OP     = 1,  // Large operator: ∑, ∫, ∏
    MC_BIN    = 2,  // Binary operator: +, −, ×, ±
    MC_REL    = 3,  // Relation: =, <, >, ≤, ≥, ≠
    MC_OPEN   = 4,  // Opening delimiter: (, [, {
    MC_CLOSE  = 5,  // Closing delimiter: ), ], }
    MC_PUNCT  = 6,  // Punctuation: ,, ;
    MC_INNER  = 7,  // Inner sub-formula (fractions, radicals, etc.)
    MC_UNKNOWN = 8  // Sentinel
};
```

**Mapping to existing NumOS NodeTypes:**

| Existing NodeType | TeX MathClass |
|:------------------|:--------------|
| `Number` | `MC_ORD` |
| `Variable` | `MC_ORD` |
| `Constant` | `MC_ORD` |
| `Function` (sin, cos, ln…) | `MC_ORD` (treated as ordinary — TeXbook p.170) |
| `Operator` (+/−/×) | `MC_BIN` |
| `Operator` (= assignment) | `MC_REL` (future expansion) |
| `Paren` ( → `MC_OPEN`, ) → `MC_CLOSE` |
| `Fraction` | `MC_INNER` |
| `Power` | `MC_INNER` |
| `Root` | `MC_INNER` |
| `Empty` | `MC_ORD` |
| `Row` | Propagates from first/last child |
| `LogBase` | `MC_ORD` (function-like) |
| `DefIntegral` | `MC_OP` (large operator) |
| `Summation` | `MC_OP` (large operator) |
| `PeriodicDecimal` | `MC_ORD` |

### 3.2 Inter-Atom Spacing Table

The `pair_spc[8][8]` table encodes spacing with integer codes **(verified against LyX 2.3.2-1 MathClass.h from Debian sources)**:

```
              Ord  Op  Bin  Rel  Open  Close  Punct  Inner
Ord           0    1    2*   3    0     0      0      1*
Op            1    1    9*   3    0     0      0      1*
Bin           2*   9*   9*   9*   2*    9*     9*     2*
Rel           3    3    9*   0    3     0      0      3
Open          0    0    9*   0    0     0      0      0
Close         0    1    2*   3    0     0      0      1*
Punct         1*   1*   9*   1*   1*    1*     1*     1*
Inner         1*   1    2*   3    1*    0      1*     1*
```

**Spacing code legend:**

| Code | Meaning | mu value | Pixel at 10px em |
|:-----|:--------|:---------|:-----------------|
| 0 | No space | 0 mu | 0 px |
| 1 | Thin space | 3 mu | ~1.67 px |
| 2 | Medium space | 4 mu | ~2.22 px |
| 3 | Thick space | 5 mu | ~2.78 px |
| 9 | Impossible (error) | — | — |
| −1, −2, −3 | Absolute (display/text only, zero in script/scriptscript) | 3/4/5 mu | — |

**Starred entries (*) are compacted:** In Knuth's full TeX, these have nuanced conditional behavior. LyX simplifies them to the listed codes for embedded use.

**Mu-to-pixel conversion:** 18 mu = 1 em. In display/text style, `1 mu = em_size / 18` pixels. In script/scriptscript, the mu unit itself scales down proportionally to the script font size.

### 3.3 Implementing Spacing as constexpr

```cpp
// In MathSpacing.h
namespace vpam::tex {

enum class MathClass : uint8_t { ORD, OP, BIN, REL, OPEN, CLOSE, PUNCT, INNER, COUNT };

// Code 9 = impossible, 0 = none, 1 = thin, 2 = medium, 3 = thick
// Negative = apply in display/text only, zero in script/scriptscript
constexpr int8_t kSpacingTable[8][8] = {
    //        ORD  OP  BIN  REL  OPEN CLOSE PUNCT INNER
    /*ORD*/  { 0,   1,  2,   3,   0,   0,    0,    1   },
    /*OP*/   { 1,   1,  9,   3,   0,   0,    0,    1   },
    /*BIN*/  { 2,   9,  9,   9,   2,   9,    9,    2   },
    /*REL*/  { 3,   3,  9,   0,   3,   0,    0,    3   },
    /*OPEN*/ { 0,   0,  9,   0,   0,   0,    0,    0   },
    /*CLOSE*/{ 0,   1,  2,   3,   0,   0,    0,    1   },
    /*PUNCT*/{ 1,   1,  9,   1,   1,   1,    1,    1   },
    /*INNER*/{ 1,   1,  2,   3,   1,   0,    1,    1   }
};

// Convert mu to pixels at current font em-size
constexpr int16_t muToPx(int8_t mu, int16_t emSize) {
    return static_cast<int16_t>((mu * emSize) / 18);
}

} // namespace vpam::tex
```

---

## 4. Glyph Assembly Engine

### 4.1 GlyphPartRecord Binary Format

Each stretchy delimiter, integral, or large operator is composed from `GlyphPartRecord` structs:

```
Field                  Size    Type
glyphID               2 bytes  uint16
startConnectorLength  2 bytes  uint16
endConnectorLength    2 bytes  uint16
fullAdvance           2 bytes  uint16
partFlags             2 bytes  uint16 (only bit 0x0001 = EXTENDER_FLAG valid)
```

OTS validator enforces: `partFlags & ~0x0001` must be zero — any other bits set = invalid font.

### 4.2 Canonical Three-Step Assembly Algorithm

Quoted verbatim from the OpenType 1.9.1 spec implementation guidance:

```
Step 1: Assemble all non-extender parts using maximum connector overlap
        and zero extenders. This gives the minimum achievable size.

Step 2: If the result is still smaller than the target, extend all
        connections equally by reducing overlap down to the
        minConnectorOverlap value from the MathVariants table.

Step 3: If further growth is needed beyond what Step 2 achieves,
        add one instance of each extender glyph and repeat from Step 1.
```

### 4.3 Gecko's Production Constraints

Mozilla's `GetMathVariantsParts()` (searchfox.org) adds practical constraints verified across three independent source mirrors:

1. **6-state FSM** translates OpenType's native **bottom-to-top** vertical piece ordering to **top-to-bottom** for rendering. The FSM handles piece type sequencing and connector matching.

2. **At most 3 non-extender pieces** allowed. Fonts with more than 3 non-extender parts exceed Gecko's complexity budget.

3. **Exactly one distinct extender glyph** required. The assembly algorithm iterates adding one extender per cycle; having multiple distinct extender glyphs in a single assembly would have ambiguous semantics.

4. **Size variants list must start with the base glyph at position 0.** Engines that iterate variants and select the first with advance ≥ target size will overshoot if the base glyph is omitted. This is explicitly documented in the Noto Math building guide: *"The variants list should always start with the base glyph, otherwise some implementations will skip it."*

### 4.4 NumOS Assembly Implementation Design

```cpp
// In MathGlyphAssembly.h
namespace vpam::math {

struct GlyphPart {
    uint16_t glyphID;
    uint16_t startConnectorLength;  // in design units
    uint16_t endConnectorLength;
    uint16_t fullAdvance;           // in design units
    bool     isExtender;
};

struct GlyphAssembly {
    uint16_t italicsCorrection;     // from MathVariants
    std::vector<GlyphPart> parts;   // bottom-to-top in font; reorder to top-to-bottom
};

struct SizeVariant {
    uint16_t glyphID;
    uint16_t advance;               // vertical advance in design units
};

class DelimiterAssembler {
public:
    /// Select the appropriate variant glyph or assemble from parts.
    /// @param variants  Size variants list (position 0 = base glyph).
    /// @param assembly  Assembly parts if no variant fits.
    /// @param targetHeight  Required height in pixels at current font scale.
    /// @param emScale       Design-units-to-pixels scale factor.
    /// @return Rendered glyph stack from top to bottom.
    GlyphStack assemble(const SizeVariant* variants, uint16_t variantCount,
                        const GlyphAssembly* assembly,
                        int16_t targetHeight, float emScale);
};

} // namespace vpam::math
```

### 4.5 Style-Dependent Large Operator Behavior

For `NodeBigOp` (∑, ∫, ∏):

- **Display style:** Limits (upper/lower) are placed above and below the operator. The operator glyph may use a larger size variant.
- **Inline style:** Limits are placed as subscripts/superscripts to the right of the operator. The operator uses the base glyph or a smaller variant.
- The switch is governed by `DisplayOperatorMinHeight` (MathConstant #3): if the operator's display-size glyph advance exceeds this threshold, limits go above/below.

---

## 5. Font Subsetting Strategy

### 5.1 Tool Constraint: lv_font_conv

Verified: `lv_font_conv` supports **explicit Unicode codepoint ranges only**. It performs **no automatic glyph dependency traversal** — no GSUB/GPOS table parsing, no substitution-rule-following, no transitive glyph inclusion.

**CLI invocation pattern:**

```bash
lv_font_conv --font STIXTwoMath-Regular.otf \
  --format lvgl --size 18 --bpp 4 \
  --range 0x0021-0x007E \          # Basic Latin (printable)
  --range 0x0391-0x03C9 \          # Greek
  --range 0x2200-0x22FF \          # Mathematical Operators
  --range 0x2A00-0x2AFF \          # Supplemental Math Operators
  --range 0x1D400-0x1D7FF \        # Mathematical Alphanumeric Symbols
  --range 0x2308-0x230B \          # Ceil/Floor brackets
  --range 0x27E8-0x27EB \          # Angle brackets
  --range 0x239B-0x23A6 \          # Stretchy delimiter assembly parts (MANDATORY for elastic (/{/[)
  -o stix_math_18.c
```

### 5.2 Minimum Glyph Inventory for Scientific Calculator

STIX Two Math contains ~4,000 glyphs. We cannot fit all of them. The following Unicode ranges cover a **complete scientific/calculus feature set**:

#### Tier 1 — Core Arithmetic & Algebra (MUST HAVE, ~600 glyphs)

| Unicode Range | Glyphs | Purpose |
|:--------------|:-------|:--------|
| `U+0021–U+007E` | 94 | Basic Latin (digits, letters, operators + − × ÷ = < > ( ) [ ] { } , ; .) |
| `U+00B0–U+00B9` | 10 | Degree, superscript 1/2/3 |
| `U+00D7` | 1 | Multiplication sign × |
| `U+00F7` | 1 | Division sign ÷ |
| `U+0391–U+03C9` | ~70 | Greek upper/lower (α–ω, Α–Ω) |
| `U+2102, U+2115, U+2119, U+211A, U+211D, U+2124` | 6 | ℂ, ℕ, ℙ, ℚ, ℝ, ℤ (double-struck) |
| `U+2200–U+22FF` | ~256 | Mathematical Operators (∀, ∃, ∈, ∉, ⊂, ⊆, ∪, ∩, ∫, ∬, ∭, ∂, ∇, √, ∛, ∜, ∞, ∼, ≈, ≅, ≠, ≡, ≤, ≥, ≪, ≫, ⊕, ⊗, ⊥, ∥, ∠, ∡, ∢, ∤, ∥, ∦, ∧, ∨, ¬, →, ←, ↔, ⇒, ⇐, ⇔, ↑, ↓, ±, ∓, ×, ⋅, ⋆, ⋄) |
| `U+2308–U+230B` | 4 | ⌈ ⌉ ⌊ ⌋ (ceiling/floor) |
| `U+239B–U+23A6` | 12 | Delimiter assembly pieces: parentheses hooks/extensions (U+239B–U+23A0) and bracket corners/extensions (U+23A1–U+23A6). **CRITICAL:** without these, elastic delimiters silently fall back to the unscaled base glyph — tall fractions inside parentheses will show undersized delimiters. |
| `U+2A00–U+2AFF` | ~60 | Supplemental Math Ops (⨀, ⨁, ⨂, large operators etc.) |
| **Subtotal** | **~514** | |

#### Tier 2 — Calculus & Advanced Notation (SHOULD HAVE, ~300 glyphs)

| Unicode Range | Glyphs | Purpose |
|:--------------|:-------|:--------|
| `U+1D400–U+1D7FF` subset | ~150 | Math Alphanumeric: italic Latin (𝐴–𝑍, 𝑎–𝑧), bold (𝐀–𝐙, 𝐚–𝐳), bold italic (𝑨–𝒁, 𝒂–𝒛), script (𝒜–𝒵), Fraktur (𝔄–𝔜) |
| `U+27E8–U+27EB` | 4 | ⟨ ⟩ (angle brackets) |
| `U+2210–U+2213` | 4 | ∐, ∏, ∐, ∑ (n-ary operators) |
| `U+222B–U+2233` | ~15 | Integrals (single → quadruple, ∮ ∯ ∰ ∱ ∲ ∳) |
| `U+2260–U+228B` | ~40 | Relations (≠, ≡, ≤, ≥, ⊂, ⊃, ⊆, ⊇) |
| `U+2190–U+21FF` | ~50 | Arrows |
| **Subtotal** | **~267** | |

#### Tier 3 — Full Typesetting (NICE TO HAVE, ~200 glyphs)

| Unicode Range | Glyphs | Purpose |
|:--------------|:-------|:--------|
| `U+1D700–U+1D7FF` full | ~100 | Complete math alphanumeric block |
| `U+0300–U+036F` | ~20 | Combining diacritics (overbar, hat, tilde, dot) |
| `U+2070–U+209F` | ~30 | Superscripts/subscripts |
| `U+27C0–U+27EF` | ~20 | Miscellaneous math symbols |
| `U+2980–U+29FF` | ~30 | Fences |
| **Subtotal** | **~200** | |

**Total: ~981 glyphs** (Tier 1+2+3, including 12 assembly pieces). This is ~25% of the full STIX Two Math font and should fit comfortably within 16 MB Flash.

### 5.3 Build-Time Font Extraction Pipeline

```
STIXTwoMath-Regular.otf (1.2 MB)
    │
    ▼
[Python extract script]
    │  Parse MATH table → dump MathConstants as C header
    │  Parse cmap → generate codepoint → glyphID map
    │  Parse MathVariants → dump GlyphAssembly tables as C arrays
    │
    ▼
stix_math_constants.h    (56 int16_t constants)
stix_math_variants.h     (assembly tables for ~20 delimiters)
stix_math_glyph_map.h    (codepoint → glyphID sparse map)
    │
    ▼
lv_font_conv --font STIXTwoMath-Regular.otf \
    --range <Tier1 ranges> --range <Tier2 ranges> \
    --format lvgl --size 18 --bpp 4 -o stix_math_18.c
lv_font_conv --font STIXTwoMath-Regular.otf \
    --range <Tier1 ranges> --range <Tier2 ranges> \
    --format lvgl --size 12 --bpp 4 -o stix_math_12.c
```

### 5.4 Memory-Efficient Glyph Indexing

For ~1,000 glyphs, a **sorted array + binary search** is optimal:

```cpp
// In MathGlyphMap.h
namespace vpam::math {

struct GlyphEntry {
    uint32_t codepoint;    // Unicode scalar value
    uint16_t glyphIndex;   // Index into lv_font_t glyph array
};

// Sorted by codepoint at build time for binary search
extern const GlyphEntry kGlyphMap[];
extern const uint16_t kGlyphMapSize;  // ~1000 entries × 6 bytes = ~6 KB PSRAM

// O(log n) lookup
inline int16_t findGlyph(uint32_t codepoint) {
    // Binary search over kGlyphMap
}

} // namespace vpam::math
```

**Storage cost:** ~1,000 entries × 6 bytes = **6 KB** in Flash. Negligible against 16 MB.

---

## 6. MathAST Class Hierarchy — TeX-Refactored

### 6.1 Design Principles

1. **Single tree, dual purpose:** The same AST serves both CAS evaluation and typesetting. Nodes carry both mathematical semantics AND typesetting metadata (TeX atom class, style context, glyph references).

2. **Embedded-optimized:** No virtual dispatch in inner loops. Use `NodeType` enum + `switch` for render dispatch. PSRAM allocation via `heap_caps_malloc(MALLOC_CAP_SPIRAM)`.

3. **Composable layout:** Each node computes its own `LayoutResult {width, ascent, descent}`. Composition is pure math — no LVGL dependency in the AST layer.

### 6.2 Refactored Node Hierarchy

```
MathNode (abstract base)
├── NodeRow              Horizontal sequence [h₁, h₂, …]
│   └── NEW: interAtomSpacing = tex::spacing(class(prev), class(next))
│
├── NodeNumber           Literal numeric "42", "3.14"        [MC_ORD]
├── NodeVariable         x, y, z, A-F, Ans, PreAns            [MC_ORD]
├── NodeConstant         π, e, i                              [MC_ORD]
├── NodeEmpty            Placeholder □                        [MC_ORD]
│
├── NodeOperator         Binary operator: +, −, ×             [MC_BIN]
│   └── NEW: NodeRelation (=, <, >, ≤, ≥, ≠)                 [MC_REL]
│
├── NodeFraction         Stacked numerator / denominator      [MC_INNER]
│   └── NEW: uses MATH FractionNumeratorShiftUp etc.
│
├── NodePower            base^exponent                        [MC_INNER]
│   └── NEW: uses MATH SuperscriptShiftUp + ItalicsCorrection
│
├── NodeRoot             √(radicand) or ⁿ√(radicand)          [MC_INNER]
│   └── NEW: uses MATH RadicalVerticalGap, RadicalRuleThickness
│
├── NodeParen            ( content )                          [MC_OPEN/MC_CLOSE]
│   └── NEW: GlyphAssembly-driven elastic delimiters
│
├── NodeFunction         sin(x), cos(x), ln(x), log(x), …     [MC_ORD]
│
├── NodeLogBase          log_n(x) with subscript base         [MC_ORD]
│
├── NodeSubscript        generic base_subscript               [MC_ORD]
│
├── NodePeriodicDecimal  Overlined periodic digits            [MC_ORD]
│
├── NodeDefIntegral      ∫[lower,upper] body d(var)           [MC_OP]
│   └── NEW: display/inline style switch for limit placement
│
├── NodeSummation        ∑[lower,upper] body                  [MC_OP]
│   └── NEW: display/inline style switch for limit placement
│
└── NodeBigOp (NEW)      Generic large operator: ∏, ⋂, ⋃      [MC_OP]
    └── Uses GlyphAssembly for operator sizing
```

### 6.3 MathClass Annotation

Every node gains a `MathClass mathClass() const` virtual method. The base implementation returns `MC_ORD`. Overrides:

```cpp
// In each node .cpp:
MathClass NodeOperator::mathClass() const {
    // Future: differentiate BIN vs REL by OpKind
    return (_op == OpKind::Eq) ? MathClass::REL : MathClass::BIN;
}

MathClass NodeParen::mathClass() const {
    return _isOpen ? MathClass::OPEN : MathClass::CLOSE;
}

MathClass NodeDefIntegral::mathClass() const { return MathClass::OP; }
MathClass NodeSummation::mathClass()  const { return MathClass::OP; }
MathClass NodeFraction::mathClass()   const { return MathClass::INNER; }
MathClass NodePower::mathClass()      const { return MathClass::INNER; }
MathClass NodeRoot::mathClass()       const { return MathClass::INNER; }

// Edge case: NodeRow propagates from its first non-empty child
MathClass NodeRow::mathClass() const {
    for (auto& child : _children) {
        if (child->type() != NodeType::Empty)
            return child->mathClass();
    }
    return MathClass::ORD;
}
```

### 6.4 Style Context Propagation

Introduce a `MathStyle` enum passed through `calculateLayout()`:

```cpp
enum class MathStyle : uint8_t {
    DISPLAY,        // Display math (centered, large operators with limits above/below)
    TEXT,           // Inline math
    SCRIPT,         // First-level sub/superscripts
    SCRIPTSCRIPT    // Second-level sub/superscripts
};

// Revised calculateLayout signature
virtual void calculateLayout(const FontMetrics& fm, MathStyle style);
```

The style determines:
- Which MATH constant to use (display vs inline)
- Large operator limit placement (above/below vs right)
- Fraction bar thickness (stays constant across styles)
- Mu-to-pixel conversion factor

---

## 7. MathRenderer — MATH Table-Driven Spacing

### 7.1 Current State: Hardcoded `_PAD` Constants

The existing [MathAST.h](src/math/MathAST.h) uses hardcoded pixel constants:

```cpp
// NodeOperator
static constexpr int16_t OP_PAD = 2;         // → should be mu-based: 4mu = medium space

// NodeFraction
static constexpr int16_t BAR_THICK = 1;      // → MATH FractionRuleThickness
static constexpr int16_t BAR_H_PAD = 2;      // → MATH axis alignment
static constexpr int16_t BAR_V_GAP = 1;      // → MATH FractionNumeratorGapMin

// NodeParen
static constexpr int16_t PAREN_W = 5;        // → GlyphAssembly determines width
static constexpr int16_t INNER_PAD = 1;      // → mu-based: 1mu = thin space
static constexpr int16_t VERT_PAD = 1;       // → MATH DelimitedSubFormulaMinHeight

// NodeRoot
static constexpr int16_t HOOK_W = 3;         // → GlyphAssembly from font
static constexpr int16_t OVERLINE_GAP = 2;   // → MATH RadicalVerticalGap
static constexpr int16_t OVERLINE_T = 1;     // → MATH RadicalRuleThickness

// NodePower
static constexpr int16_t EXP_RAISE_NUM = 1;  // → MATH SuperscriptShiftUp
static constexpr int16_t EXP_RAISE_DEN = 2;  // → 50% of capHeight (font-derived)
```

### 7.2 Proposed: MathConstantsProvider

```cpp
// In MathTypography.h
namespace vpam::math {

/// Singleton that provides MATH table constants for the active font.
/// On ESP32-S3, populated from stix_math_constants.h at boot.
class MathConstantsProvider {
public:
    static MathConstantsProvider& instance();

    /// Load constants from compiled-in font data.
    void load(const int16_t constants[56], int16_t emSize, int16_t ruleThickness);

    // ── Typed accessors matching the 56-constant enum ──
    int16_t scriptPercentScaleDown()        const { return _c[0]; }
    int16_t scriptScriptPercentScaleDown()  const { return _c[1]; }
    int16_t axisHeight()                    const { return _c[5]; }
    int16_t subscriptShiftDown()            const { return _c[8]; }
    int16_t superscriptShiftUp()            const { return _c[11]; }
    int16_t superscriptShiftUpCramped()     const { return _c[12]; }
    int16_t subSuperscriptGapMin()          const { return _c[15]; }
    int16_t spaceAfterScript()              const { return _c[17]; }
    int16_t upperLimitGapMin()              const { return _c[18]; }
    int16_t lowerLimitGapMin()              const { return _c[20]; }

    // Fraction
    int16_t fractionNumeratorShiftUp(bool display)       const;
    int16_t fractionDenominatorShiftDown(bool display)   const;
    int16_t fractionNumeratorGapMin(bool display)        const;
    int16_t fractionDenominatorGapMin(bool display)      const;
    int16_t fractionRuleThickness()                      const { return _c[38]; }

    // Radical
    int16_t radicalVerticalGap(bool display)             const;
    int16_t radicalRuleThickness()                       const { return _c[51]; }
    int16_t radicalKernBeforeDegree()                    const { return _c[53]; }
    int16_t radicalKernAfterDegree()                     const { return _c[54]; }

    // Stack (for limits above/below operators)
    int16_t stackTopShiftUp(bool display)                const;
    int16_t stackBottomShiftDown(bool display)           const;
    int16_t stackGapMin(bool display)                    const;

    // ── Mu conversion ──
    /// Convert math units to pixels at the current font em-size.
    int16_t muToPx(int16_t mu) const {
        return static_cast<int16_t>((mu * _emSize) / 18);
    }

private:
    int16_t _c[56];     // All 56 MATH constants
    int16_t _emSize;    // Font em-size in pixels
};

} // namespace vpam::math
```

### 7.3 Refactored Layout Methods

Example: `NodeFraction::calculateLayout` before and after:

**Before (hardcoded):**
```cpp
void NodeFraction::calculateLayout(const FontMetrics& fm) {
    _numerator->calculateLayout(fm);
    _denominator->calculateLayout(fm);
    int16_t w = std::max(_numerator->layout().width, _denominator->layout().width) + 2 * BAR_H_PAD;
    int16_t ascent = _numerator->layout().height() + BAR_V_GAP + BAR_THICK / 2;
    int16_t descent = BAR_THICK / 2 + BAR_V_GAP + _denominator->layout().height();
    _layout = LayoutResult{w, ascent, descent};
}
```

**After (MATH table-driven):**
```cpp
void NodeFraction::calculateLayout(const FontMetrics& fm, MathStyle style) {
    auto& mc = MathConstantsProvider::instance();
    bool display = (style == MathStyle::DISPLAY);

    FontMetrics numFm = fm;  // numerator in same style
    FontMetrics denFm = fm;  // denominator in same style
    _numerator->calculateLayout(numFm, style);
    _denominator->calculateLayout(denFm, style);

    int16_t ruleThickness = mc.fractionRuleThickness();
    int16_t numShiftUp = mc.fractionNumeratorShiftUp(display);
    int16_t denShiftDown = mc.fractionDenominatorShiftDown(display);
    int16_t numGapMin = mc.fractionNumeratorGapMin(display);
    int16_t denGapMin = mc.fractionDenominatorGapMin(display);

    // Apply gaps: at least gapMin, but shift constants may push further
    int16_t numShift = std::max(numShiftUp, numGapMin + _numerator->layout().descent);
    int16_t denShift = std::max(denShiftDown, denGapMin + _denominator->layout().ascent);

    int16_t contentW = std::max(_numerator->layout().width, _denominator->layout().width);

    // Rule extends slightly beyond widest child (min 1px each side)
    int16_t ruleOverhang = mc.muToPx(1);  // 1 mu overhang each side
    int16_t barW = contentW + 2 * ruleOverhang;

    int16_t ascent = numShift + _numerator->layout().descent + ruleThickness / 2;
    int16_t descent = ruleThickness / 2 + _denominator->layout().ascent + denShift;

    _layout = LayoutResult{barW, ascent, descent};
}
```

### 7.4 Inter-Atom Spacing in NodeRow

The most impactful change: `NodeRow::calculateLayout` applies TeX spacing between consecutive children:

```cpp
void NodeRow::calculateLayout(const FontMetrics& fm, MathStyle style) {
    int16_t totalW = 0;
    const MathClass* prevClass = nullptr;

    for (auto& child : _children) {
        child->calculateLayout(fm, style);

        if (prevClass) {
            MathClass currClass = child->mathClass();
            int8_t spc = kSpacingTable[static_cast<int>(*prevClass)][static_cast<int>(currClass)];

            if (spc > 0) {
                totalW += MathConstantsProvider::instance().muToPx(spc);
            } else if (spc < 0 && (style == MathStyle::DISPLAY || style == MathStyle::TEXT)) {
                totalW += MathConstantsProvider::instance().muToPx(-spc);
            }
            // spc == 0 → no space
            // spc == 9 → error (should not happen; treated as 0)
        }

        totalW += child->layout().width;
        prevClass = &currClass;
    }

    // Compute ascent/descent as max of children
    int16_t maxAsc = 0, maxDesc = 0;
    for (auto& child : _children) {
        maxAsc = std::max(maxAsc, child->layout().ascent);
        maxDesc = std::max(maxDesc, child->layout().descent);
    }

    _layout = LayoutResult{totalW, maxAsc, maxDesc};
}
```

---

## 8. Implementation Roadmap

### Phase 1: Font Extraction Pipeline (Week 1–2)

- [ ] Write Python script `scripts/extract_stix_math.py` that:
  - Parses STIXTwoMath-Regular.otf binary
  - Extracts all 56 MathConstants → `src/math/font/stix_math_constants.h`
  - Extracts MathVariants (size variants + assembly parts) for ~20 delimiter pairs → `src/math/font/stix_math_variants.h`
  - Extracts MathItalicsCorrectionInfo → `src/math/font/stix_math_italics.h`
  - Generates the codepoint → glyphID map for Tier 1+2 ranges
- [ ] Run `lv_font_conv` for sizes 18, 12, and 8 (scriptscript) with Tier 1+2 ranges
- [ ] Integrate generated `.c` files into `platformio.ini` build

### Phase 2: MathConstantsProvider (Week 2–3)

- [ ] Implement `MathConstantsProvider` singleton in `src/math/MathTypography.h/.cpp`
- [ ] Wire into `MathCanvas` constructor (loads constants from compiled-in data)
- [ ] Add `MathStyle` enum to `FontMetrics` and propagate through `calculateLayout()`
- [ ] Implement `muToPx()` conversion
- [ ] **Refactor methods one at a time,** verifying rendering after each:
  1. `NodeFraction::calculateLayout` → use MATH fraction constants
  2. `NodePower::calculateLayout` → use SuperscriptShiftUp + ItalicsCorrection
  3. `NodeRoot::calculateLayout` → use RadicalVerticalGap, RadicalRuleThickness
  4. `NodeOperator::calculateLayout` → mu-based spacing

### Phase 3: TeX MathClass & Inter-Atom Spacing (Week 3–4)

- [ ] Add `MathClass mathClass() const` virtual to `MathNode` base
- [ ] Implement `kSpacingTable[8][8]` constexpr
- [ ] Modify `NodeRow::calculateLayout` to apply inter-atom spacing
- [ ] Add `NodeRelation` type (extends `NodeOperator`) for =, <, >, ≤, ≥, ≠
- [ ] Test with mixed expressions: `2+3×4`, `sin(x)+cos(y)`, `a=b+c`

### Phase 4: Glyph Assembly Engine (Week 4–5)

- [ ] Implement `DelimiterAssembler` class in `src/math/font/MathGlyphAssembly.h/.cpp`
  - FSM for bottom-to-top → top-to-bottom reordering
  - Three-step iterative assembly algorithm
  - 3-piece + 1-extender limit enforcement
  - Fallback to size variants when assembly not needed
- [ ] Replace `NodeParen` static `PAREN_W = 5` with assembled delimiters
- [ ] Add `NodeBigOp` class for generic large operators
- [ ] Implement display/inline style switch for `NodeDefIntegral` and `NodeSummation` limit placement

### Phase 5: Verification & Tuning (Week 5–6)

- [ ] Visual regression: screenshot comparison before/after for 50 canonical expressions
- [ ] Measure heap/PSRAM impact of MATH constants + glyph map (~10 KB total)
- [ ] Measure rendering performance (frame time for full-screen expression)
- [ ] Tune Tier 1/2/3 glyph ranges based on actual usage
- [ ] Document final glyph inventory in `docs/GLYPH_INVENTORY.md`

---

## Appendix A: Key Architectural Decisions

| Decision | Rationale |
|:---------|:----------|
| Single AST for eval + typesetting | PSRAM constraint; separate trees would double memory for complex expressions |
| `MathClass` annotation on nodes | 8-byte enum per node — negligible overhead vs. separate typesetting tree |
| Sorted-array glyph lookup | ~6 KB for 1000 entries vs. ~256 KB for direct-mapped hash table on MCU |
| `lv_font_conv` + manual ranges | No automatic dependency traversal available; explicit is more predictable |
| Gecko's 3-piece assembly limit | Proven safe; fonts violating this are rare; prevents runaway assembly on constrained MCU |
| Build-time constant extraction | Runtime OTF parsing would cost ~200 KB of binary + 10s of seconds of CPU time |

## Appendix B: Open Questions for Further Research

1. **MathAST class hierarchy for dual use** — The SymEngine Composite+Visitor pattern was refuted but no verified alternative emerged. The existing NumOS pattern (enum-based dispatch + PSRAM allocation) is pragmatic. A formal evaluation of CRTP vs. virtual dispatch vs. tagged union for this specific MCU would be valuable.

2. **HarfBuzz hb-subset pipeline** — The claim that HarfBuzz cannot subset MATH tables was refuted, but alternative subsetting via HarfBuzz+lb_font_conv conversion was not investigated. This could provide automatic glyph dependency traversal if a pipeline can be built from HarfBuzz output to LVGL format.

3. **Style-dependent assembly thresholds** — The MATH constants differ between display and inline styles, but the glyph assembly algorithm itself does not mention style. Display-style operators may need different assembly thresholds, but the spec is silent on this.

4. **Existing MathAST retrofitting** — Can the current VPAM AST gain TeX annotations incrementally, or does the rendering layer need a separate parallel "typesetting tree"? The current architecture leans toward annotation, but this should be validated with a benchmark.

---

**Sources:** See the [deep research output](C:\Users\JUANRA~1\AppData\Local\Temp\claude\c--Users-Juan-Ram-n-Documents-Calculadora\4707d09f-9734-474c-9a36-f482f2695c98\tasks\w99w6oor0.output) for the complete list of 22 verified sources, 13 confirmed claims, and 12 refuted claims.
