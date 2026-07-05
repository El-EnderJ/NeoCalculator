# NumOS — TeX-Parity Typesetting Specification

> **Normative typesetting-parity spec** for the VPAM math renderer (program code **TXP**).
> Defines, operationally, what "TeX-quality typesetting" means for NumOS on the ILI9341 320×240 /
> ESP32-S3 N16R8 target, measures the current renderer against TeXbook Appendix G and the OpenType
> MATH table of STIX Two Math, and specifies a phased implementation program (TXP-0…TXP-10) that
> closes every in-scope gap.
>
> **Audit base**: branch `claude/txp-tex-parity-spec-2l37v5` @ `a1ca537` (clean tree, equal to
> `main`), audited 2026-07-04. **Build verification**: `pio run -e emulator_pc` built successfully
> on Linux in this session (55.5 s, PlatformIO 6.1.19, SDL2 2.30.0); the
> `math_showcase_smoke.numos` script ran `--headless --deterministic` and its capture compared
> **byte-identical** to `tests/emulator/golden/math_showcase_smoke.ppm` after the standard 1-rect
> clock mask (481 px ignored). The firmware env was not built in this session (no espressif
> toolchain here); firmware claims rely on CI evidence cited from
> `NUMOS_ARCHITECTURE_GROUND_TRUTH.md`.
>
> **Markers**: **[V]** = verified in source this session (every [V] claim carries `file:line`) ·
> **[P]** = proposed (does not exist yet) · **[U]** = external reference reconstructed from
> training knowledge, not re-verified online this session · **PD-n** = product decision record
> (§C) · **OQ-n** = open question with a recommended default (§D).
>
> **Companion documents** (this spec is a companion and successor to the MRV2 set — it inherits
> every MRV2 decision it does not explicitly supersede; superseded clauses are listed in
> Appendix A):
> `NUMOS_MATHRENDERER_V2_ARCHITECTURE_SPEC.md` (MRV2 architecture),
> `NUMOS_MATH_LAYOUT_ENGINE_AND_TYPESYSTEM_SPEC.md` (layout engine),
> `NUMOS_MATH_NOTATION_SURFACE_AND_SYMBOL_COVERAGE_SPEC.md` (notation surface),
> `NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md` (MR-00…MR-20),
> `NUMOS_MATHRENDERER_TEST_ORACLE_AND_GOLDEN_PLAN.md` (oracle stack),
> `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md` (golden governance),
> `NUMOS_MEMORY_ALLOCATION_POLICY.md`, `NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md`,
> `NUMOS_RISK_REGISTER.md`, `NUMOS_FABLE_CONTEXT_HEADER.md`.

---

## Table of contents

- **Part I — Maintainer's brief**
  - §A Executive summary
  - §B Gap matrix
  - §C Decision records PD-1…PD-14
  - §D Open questions OQ-1…OQ-6
  - §E Risk-register additions REN-6…REN-11
  - §F Effort & sequencing summary
- **Part II — Agent implementation program**
  - §G Preamble: constraints, units, flags, process rules (binding on every phase)
  - §H Normative reference layer: TeX ↔ OpenType ↔ NumOS mapping, the exact spacing table,
    Appendix G rule transcriptions
  - §TXP-0 Corpus, reference oracle, flags, baseline freeze
  - §TXP-1 Font-data pipeline extension (kerns, overlap, PUA variants, assembly glyphs)
  - §TXP-2 Atom-spacing fidelity (conditional entries, Bin demotion)
  - §TXP-3 True text metrics and math-italic identifiers
  - §TXP-4 Script placement (full Rule 18, cramped styles)
  - §TXP-5 Fractions and bars (σ8–σ12 exactness)
  - §TXP-6 Radicals (degree placement, glyph radical)
  - §TXP-7 Big operators and limits (display variants, exact gaps)
  - §TXP-8 Stretchy delimiters (Rule 19 sizing, MinConnectorOverlap)
  - §TXP-9 Script kerning (MathKernInfo cut-ins)
  - §TXP-10 Parity audit and report
- Appendix A — Superseded clauses of prior specs
- Appendix B — The TXP expression corpus (C-01…C-64)
- Appendix C — Exact `.numos` script listings

---

# Part I — Maintainer's brief

## §A. Executive summary

### A.1 What "TeX parity" means here (operational definition)

For every corpus item in Appendix B, rendered by the emulator at em 18 in the item's declared
style, the following MUST hold at the end of TXP-10:

1. **Derivability.** Every placement decision (every shift, gap, advance, and rule position) is
   derivable from (a) a numbered TeXbook Appendix G rule transcribed in §H, (b) a named OpenType
   MATH constant of STIX Two Math already present in
   `src/math/font/stix_math_constants.h:37-94` [V] or added by TXP-1, or (c) a PD-n record in §C.
   No unexplained pixel remains.
2. **Reference agreement.** The feature-position metrics extracted by the TXP oracle (§TXP-0 0.5)
   from the NumOS render agree with the metrics extracted from a MathJax/KaTeX-class reference
   render of the same LaTeX string at the same em size, within the tolerance defined per metric
   class in §TXP-0 0.5.4 (default ±1 px), **except** where a PD-n explicitly declares and bounds a
   deviation (and then the deviation must equal the PD's declared value).
3. **A trained eye sees nothing wrong.** The TXP-10 side-by-side gallery (NumOS capture next to
   reference render, per corpus item) is reviewed by a human; any placement a reviewer flags as
   visibly off and that is not covered by a PD-n is a TXP-10 gate failure.

### A.2 How far along the renderer already is (credit where due)

The repo is unusually far along for calculator firmware — the audit confirms, all [V]:

- A real OpenType MATH pipeline: all 56 MathConstants extracted at UPM 1000
  (`stix_math_constants.h:37-94`), a pixel-safe constexpr provider with round-to-nearest scaling
  and min-pixel guards (`MathConstantsProvider`, `src/math/MathTypography.h:263-506`), and 413
  italic-correction entries (`stix_math_italics.h`, `kItalicsCorrectionCount = 413`).
- The TeX 8-class atom system with an 8×8 spacing table applied in one DRY path shared by layout,
  draw, and cursor geometry (`kSpacingTable`, `src/math/MathTypography.h:96-106`;
  `interAtomSpacingPx`, `:166-176`; consumed at `src/math/MathAST.cpp:197-201`,
  `src/ui/MathRenderer.cpp:1617-1619,1446-1449`).
- Correct mu semantics: 1 mu = em/18 **of the current style's em** — the row passes its own
  style-scaled `fm.emSize` into the converter (`muToPx`, `MathTypography.h:133-135`;
  `MathAST.cpp:200-201`), so spacing shrinks in scripts as TeX requires.
- Axis-based vertical architecture: fraction bars, ± centering and delimiter symmetry all sit on
  `AxisHeight` = 258 DU (`MathAST.h:231-234`, `MathAST.cpp:411-415`,
  `MathAST.cpp:97-115`).
- MATH-constant-driven fractions (rule thickness, gap minima — `MathAST.h:549-588`), radicals
  (gap/rule/extra-ascender/degree kerns — `MathAST.cpp:563-616`), scripts (shift-up/bottom-min +
  `SpaceAfterScript` — `MathAST.h:484-513`, `MathAST.cpp:509-510`), and limits (`UpperLimitGapMin`
  — `MathAST.cpp:1064`).
- Italic correction before superscripts for single-glyph italic bases (`MathAST.cpp:482-506`).
- A Gecko-grade extensible-glyph assembler (FSM: ≤3 non-extenders, exactly one extender —
  `MathGlyphAssembly.cpp:30-86`) with even per-seam overlap distribution (`:236-253`), plus a
  permanent vector-stroke fallback tier for delimiters (`MathRenderer.cpp:87-172`).
- Byte-exact golden infrastructure (19 goldens, 25 candidate stems
  [V `scripts/generate-emulator-candidates.py:39-143`], human-only promotion) and a deterministic
  emulator this spec's baselines were re-verified on in this session.

### A.3 What actually separates NumOS from TeX output (the honest list)

The gap matrix (§B) has 53 rows; the load-bearing ones are:

1. **The display-limits path is dead at em 18.** `DisplayOperatorMinHeight` = 1800 DU = 32 px
   @ em 18, but the extractor drops STIX's larger Σ/∫ size variants because they are unencoded
   glyphs with no cmap entry (`extract_stix_math.py:312-316` resolves glyph names through the
   cmap only), so `kVarTable_2211` contains a single 1031 DU "variant" — the base glyph itself
   [V `stix_math_variants.h` U+2211 block]. `shouldUseDisplayLimits` (`MathAST.h:410-415`)
   therefore always returns false for `NodeSummation`/`NodeDefIntegral` at em 18: **sums and
   integrals never render with limits above/below**, and never render at display size. (`NodeBigOp`
   bypasses the gate entirely — `MathAST.cpp:1319-1324` — so ∏/⋃ stack limits around a
   too-small glyph; the two families are inconsistent.) Fix: PUA re-encoding of variant glyphs in
   the font pipeline (TXP-1) + gate unification (TXP-7).
2. **The spacing table is close but not TeX.** Two defects: (a) all of TeXbook's style-conditional
   (parenthesized) entries are stored unconditional, so medium/thick spaces survive into script
   styles (`kSpacingTable`, `MathTypography.h:96-106` uses only positive codes; the negative-code
   machinery at `:145-176` exists but is never fed); (b) `BINARY→OP` is marked impossible (9)
   where TeX has (2) [U — TeXbook p. 170 table]. And there is **no Bin→Ord demotion at all**
   (`NodeOperator::mathClass()` is static, `MathAST.h:715-717`; the row scan is pairwise-only,
   `MathAST.cpp:190-210`): `x = −3` gets 0 px after `=` and a medium space after the unary minus,
   where TeX puts a thick space before a demoted-Ord minus and nothing after it.
3. **Measurement is monospaced where drawing is proportional.** Layout widths for numbers,
   variables and function labels are `charWidth × length` (`MathAST.cpp:273-285,934-945,715-727`)
   while the draw path advances by real per-glyph `adv_w` with the kerning pair passed
   (`MathRenderer.cpp:2450-2486`). STIX digits are tabular so digits survive, but `.`,
   letters, and multi-char labels drift; the ± operator is a hand-stroked vector
   (`MathRenderer.cpp:1672-1710`) at `charWidth`.
4. **Variables render upright.** `NodeVariable` draws its ASCII label
   (`MathRenderer.cpp:2086-2095`); STIX Two Math ASCII letters are upright. TeX renders variables
   in math italic. The Mathematical Italic alphabet (U+1D434…) is already in the font subset
   [V `stix_math_18.c:4` range `0x1D400-0x1D7FF`] — nothing maps to it. Decided in PD-3.
5. **Scripts implement half of Rule 18.** Superscript = `max(SuperscriptShiftUp,
   SuperscriptBottomMin + descent)` (`MathAST.h:484-513`); subscript = constant
   `SubscriptShiftDown` (`MathAST.cpp:1411-1413`). Missing: the boxed-base baseline-drop terms
   (σ18/σ19 ↔ `SuperscriptBaselineDropMax`/`SubscriptBaselineDropMin` — extracted at
   `MathTypography.h:352-354,340-342` and unused), the `SubscriptTopMax` clamp, cramped-style
   shift selection (`SuperscriptShiftUpCramped` extracted `:346-348`, unused), the Rule 18e
   sub/sup interlock (`SubSuperscriptGapMin`/`SuperscriptBottomMaxWithSubscript` extracted
   `:355-360`, unused), and the italic-correction split between sup and sub.
6. **Fractions use a house placement formula.** Shifts are derived from a single symmetric gap
   `G = max(display gap minima)` regardless of style (`fractionBarGaps`, `MathAST.h:569-588`,
   consumed `MathAST.cpp:387-397`); the TeX shift constants σ8–σ12 are computed
   (`fractionLayoutMetrics`, `MathAST.h:429-442`) but used only for rule thickness and a 1 mu bar
   overhang that TeX does not have. TXP-5 restores the shift+gap-minimum hybrid (the actual
   OpenType algorithm) and PD-4 keeps only the *size* policy deviation.
7. **Delimiter sizing is cover-plus-pad, not Rule 19.** `NodeParen` grows symmetric-about-axis to
   fully cover content plus a pad derived from `DelimitedSubFormulaMinHeight`
   (`MathAST.cpp:97-122,659-673`); TeX sizes to
   `max(⌈delimiterfactor·size/1000⌉, size − delimitershortfall)` [U — TeXbook Rule 19], which
   deliberately allows undershoot. NumOS delimiters read systematically taller than TeX's. Also
   non-TeX: an inner pad of `parenWidth/3` on each side of the content (`MathAST.cpp:669-670`).
8. **Assembly ignores `MinConnectorOverlap`.** The assembler may reduce any seam's overlap to
   0 px (`MathGlyphAssembly.cpp:236-253`), producing hairline seams; the OpenType minimum is not
   extracted at all (grep of `extract_stix_math.py` — no `MinConnectorOverlap`). Latent today
   (assembly glyphs absent from the subset), live the moment MR-04/TXP-1 fonts land.
9. **No MathKernInfo, no top-accent data.** The extractor emits constants, italic corrections and
   variants only (`extract_stix_math.py:15-16,35,383-448`); staircase cut-in kerns and accent
   attachment points were never extracted. MRV2 rejected cut-in kerning
   (layout spec §14) — superseded by PD-7: at em 18 the largest STIX cut-ins are 2–4 px, well
   above the visibility threshold that §14 assumed.
10. **Limits use one gap constant for both sides and add an invented pad.**
    `UpperLimitGapMin` is applied above **and** below (`MathAST.cpp:1064,1092-1099,1195,1330`);
    `LowerLimitGapMin` and both `…BaselineRiseMin/DropMin` constants are never read; a non-TeX
    `symbolPadPx = 4 mu` is inserted on both sides of the operator (`MathAST.h:353-356`,
    `MathAST.cpp:1092-1099`). (STIX happens to set both gap minima to 135 DU, so today the wrong
    constant is invisible — it becomes wrong the day the font changes.)
11. **The radical degree is raised by the wrong quantity.** Code raises the degree by
    `RadicalDegreeBottomRaisePercent × em` measured down from the radical *top*
    (`MathAST.cpp:607-616`); OpenType defines it as a percentage of the **radical's own height**
    measured up from its **bottom** [U — OpenType MATH §MathConstants]. Equal only by
    coincidence for shallow radicands.
12. **The cursor mirror.** `computeCursorPosition`'s Finder re-implements every placement formula
    (`MathRenderer.cpp:911-1232`) — each TXP geometry change must land in layout, draw, **and**
    Finder in the same PR or the cursor drifts (process rule §G.6).

Everything else in §B is either already DONE (proved), a smaller variant of the above, or scoped
out with a PD.

### A.4 Program shape

Eleven phases. TXP-0 (corpus + oracle + baseline freeze) and TXP-1 (font-data pipeline; zero
geometry change) are pure enablement. TXP-2…TXP-9 are eight geometry waves, each with its own
golden re-bless, each independently shippable and revertible. TXP-10 re-runs the whole corpus and
commits the parity report. Cross-dependencies on the MRV2 program are explicit per phase; the only
hard ones are: the host geometry harness (MR-02) is the acceptance vehicle everywhere (TXP-0
bootstraps the required subset if MR-02 has not landed), and TXP-1 subsumes MR-04's font
regeneration (they must land as one wave to avoid regenerating fonts twice).

---

## §B. Gap matrix

One row per typesetting capability. Columns: **ID** · **Capability** · **TeX/MATH normative
rule** (§H reference) · **Current NumOS state** [V] with `file:line` · **Gap** ∈ {DONE, PARTIAL,
MISSING, OUT-OF-SCOPE} · **Phase**.

Statistics: **10 DONE · 25 PARTIAL · 9 MISSING · 9 OUT-OF-SCOPE** (53 rows).

### B.1 Italic correction and kerning

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-01 | Italic correction after an italic glyph followed by an upright glyph in a row | Appendix G rule for char kerns; OpenType `MathItalicsCorrectionInfo` (§H.4) | Never applied in row layout; `NodeRow::calculateLayout` sums widths + class spacing only (`MathAST.cpp:190-210`). Table exists (`stix_math_italics.h`, 413 entries) | MISSING | TXP-3 |
| GAP-02 | Italic correction before a superscript | Rule 18f: sup shifted right by δ relative to sub (§H.6) | Applied only when the Power base is a single-codepoint `NodeVariable` or `NodeConstant` (`MathAST.cpp:482-506`); uses truncating `duToPx` (`:492`) not `scalePx`; not applied for `NodeFunction`, `NodeParen`, or general row bases | PARTIAL | TXP-4 |
| GAP-03 | Italic correction subtracted for subscripts (sub tucks under the overhang) | Rule 18f (§H.6) | `NodeSubscript` places the sub at `x + base.width` with no correction (`MathAST.cpp:1406-1409`, `MathRenderer.cpp:2410-2416`) | MISSING | TXP-4 |
| GAP-04 | Italic-correction split of inline (nolimits) big-op scripts — ∫ has δ = 230 DU: sup sits right of the slanted top, sub tucks left | Rule 18f applied to Op nucleus (§H.6) | `inlineLimitGeometry` centers upper and lower in one shared column at the same x (`MathRenderer.cpp:328-347`); `MathVariantTable.italicsCorrection` field exists and is populated for ∫ (`stix_math_variants.h:57-67`) but never read | MISSING | TXP-7 |
| GAP-05 | Corner-kern cut-ins for scripts (`MathKernInfo`: TopRight/TopLeft/BottomRight/BottomLeft staircase kerns) | OpenType MATH §MathKernInfo (§H.4); no classic-TeX equivalent — PD-7 chooses OpenType | Not extracted (`extract_stix_math.py` has no MathKernInfo pass — grep verified); no data structures, no application. MRV2 layout spec §14 rejected this — **superseded** (Appendix A) | MISSING | TXP-1 (data), TXP-9 (apply) |

### B.2 Styles

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-06 | Cramped-style propagation (denominator, radicand, subscript, under-limit use cramped variants) | Appendix G style rules (§H.2); OpenType `SuperscriptShiftUpCramped` = 252 DU | No cramped bit anywhere; `MathStyle` has 4 uncramped values (`MathTypography.h:45-50`); `superscriptShiftUpCramped()` accessor exists (`:346-348`) with zero call sites (grep). MRV2 layout spec §14 rejected — **superseded**; §14's "~1 px" estimate is wrong: 360→252 DU = 6→5 px @ em 18 and 108 DU ≈ 2 px @ em 18 after rounding interacts with the bottom-min clamp (measured in TXP-4) | MISSING | TXP-4 |
| GAP-07 | 4-style lattice with per-construct transitions | Appendix G rules 13–17 style assignments (§H.2) | DISPLAY/TEXT/SCRIPT/SCRIPTSCRIPT lattice with correct script stepping exists (`MathStyle`, `MathTypography.h:45-50`; `FontMetrics::superscript()`, `MathAST.h:249-290`) | DONE | — |
| GAP-08 | Script size ratios 70 % / 55 % (`ScriptPercentScaleDown`) | OpenType constants 0–1 (§H.3) | Physical font trio 18/12/8 px ⇒ 66.7 % and 44.4 % (`ui/MathTypography.cpp:26-28`; `FontMetrics::superscript()` L0→L1 uses prebaked 12 px metrics, `MathAST.h:249-257`). Forced by LVGL bitmap fonts + flash budget (4th size ≥ 400 KB rejected, MRV2 §7) | OUT-OF-SCOPE (PD-10) | — |
| GAP-09 | `\displaystyle` pinning inside sub-formulas | `\mathchoice`-class machinery | Missing; MRV2 replaces it with a per-node `forceDisplayStyle` bit (layout spec §2, ticket MR-12 [P]) — TXP inherits, does not duplicate | OUT-OF-SCOPE (MR-12) | — |
| GAP-10 | Top-level fraction size demotion (TEXT→SCRIPT) | Rule 15 style stepping | Deliberately rejected: readable-fraction policy keeps full glyph size at script level 0 (`fractionPartMetrics`, `MathAST.h:314-326`, rationale `:310-312`) — behaviour ≡ `\cfrac`. Kept; the oracle renders references with `\cfrac`/`\dfrac` to match (PD-4) | OUT-OF-SCOPE (PD-4) | — |

### B.3 Atom spacing

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-11 | 8×8 spacing table — base codes | TeXbook p. 170 table (§H.5) [U] | 63 of 64 cells carry the right code (`kSpacingTable`, `MathTypography.h:96-106`, "verified against LyX 2.3.2" comment `:93`); one defect: `BINARY→OP` = 9 (impossible) instead of medium-(2) | PARTIAL | TXP-2 |
| GAP-12 | Style-conditional spacing (parenthesized entries suppressed in SCRIPT/SCRIPTSCRIPT) | §H.5 | All table codes are stored positive-unconditional; the negative-code suppression machinery exists and works (`spacingCodeToMu` `:153-159`, `interAtomSpacingPx` `:166-176`) but the table never uses negative codes ⇒ medium/thick spaces wrongly survive into scripts | MISSING | TXP-2 |
| GAP-13 | Bin→Ord demotion (leading Bin; Bin after Bin/Op/Rel/Open/Punct; Bin before Rel/Close/Punct) | TeXbook mlist scan rule (§H.5) [U] | No demotion pass; classes are static per node (`NodeOperator::mathClass`, `MathAST.h:715-717`); the pairwise 9-cells accidentally zero *some* pairs but the demoted atom's *other* side still spaces as Bin — e.g. leading unary minus gets a medium space before its operand (`kSpacingTable[BIN][ORD]=2`) where TeX has 0 | MISSING | TXP-2 |
| GAP-14 | mu unit scales with the current style's em | 1 mu = em/18 of current size (§H.1) | Correct: row spacing converts with the row's own style-scaled `fm.emSize` (`muToPx`, `MathTypography.h:133-135`; call `MathAST.cpp:200-201`) | DONE | — |
| GAP-15 | One DRY spacing path for layout, draw, cursor | — (NumOS invariant, MRV2 Y-family) | Correct: `interAtomSpacingPx` consumed identically in layout (`MathAST.cpp:197-201`), draw (`MathRenderer.cpp:1617-1619`), cursor (`:1446-1449,1455-1459`) | DONE | — |

### B.4 Text-run and symbol metrics

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-16 | Glyph-true advances for measured text runs | Widths from font advance + kerning, as drawn | Layout uses `charWidth × visibleChars` for `NodeNumber` (`MathAST.cpp:273-285`), `NodeVariable` (`:934-945`), `NodeFunction` labels (`:715-727`), `NodeConstant`/`NodeOperator` (1 × charWidth, `:312-319,851-857`), `NodePeriodicDecimal` (`:960-983`); draw advances by real `glyph.adv_w` with the kerning pair (`MathRenderer.cpp:2450-2486`). STIX digits are tabular so digit runs match; `.`, letters and symbol widths (× vs +) do not | PARTIAL | TXP-3 |
| GAP-17 | ± and other operators drawn as font glyphs | Glyphs, not synthetic strokes | ± is hand-stroked vectors (`drawOperatorBaseline`, `MathRenderer.cpp:1672-1710`); U+00B1 is in the subset (`stix_math_18.c:4` range `0x00B0-0x00FF`); all other operators already draw as glyphs (`:1713`) | PARTIAL | TXP-3 |
| GAP-18 | Variables in math italic; digits/function names upright | TeX default: single-letter identifiers italic [U] | All variables draw as upright ASCII (`drawVariableBaseline`, `MathRenderer.cpp:2086-2095`); Mathematical Italic alphabet U+1D434–U+1D467 present in subset and unused. Digits and function labels upright (correct). PD-3 adopts italic single-letter variables | PARTIAL (PD-3) | TXP-3 |
| GAP-19 | Missing-glyph advance honesty | — (MRV2 Y-8) | Missing glyph advances `line_height/3` silently (`MathRenderer.cpp:2480-2483`); GlyphProvider/tofu is MR-06's job — TXP inherits, does not duplicate | OUT-OF-SCOPE (MR-06) | — |

### B.5 Scripts

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-20 | Superscript shift — full Rule 18c: `u = max(u_dropped, p_style, sup.depth + ¼·x-height)` | Rule 18a+18c (§H.6); OpenType `SuperscriptShiftUp(Cramped)`, `SuperscriptBottomMin`, `SuperscriptBaselineDropMax` | Implements `max(SuperscriptShiftUp, SuperscriptBottomMin + exp.layout-descent)` with optional policy adjust (default 0) — `superscriptShiftMetrics`, `MathAST.h:484-513`; consumed `MathAST.cpp:475-476`, `MathRenderer.cpp:1823-1827`. Missing: cramped selection (GAP-06), boxed-base drop term `h(base) − SuperscriptBaselineDropMax` (accessor `MathTypography.h:352-354` unused), ink-vs-layout descent choice (`visibleBottomMinShiftPx` computed `MathAST.h:493-494`, discarded) | PARTIAL | TXP-4 |
| GAP-21 | Subscript shift — full Rule 18a+18b: `v = max(v_dropped, σ16, sub.height − ⅘·x-height)` | Rule 18b (§H.6); OpenType `SubscriptShiftDown`, `SubscriptTopMax`, `SubscriptBaselineDropMin` | Constant `SubscriptShiftDown` with a 2 px floor (`MathAST.cpp:1411-1413`; same in `NodeLogBase` `:806-808` and draw `MathRenderer.cpp:2412-2413`); no `SubscriptTopMax` clamp, no boxed-base `d(base) + SubscriptBaselineDropMin` term (accessors `MathTypography.h:337-342` unused); descent reserves the sub's full height (`:1415-1417`) — a conservative box TeX does not make | PARTIAL | TXP-4 |
| GAP-22 | Simultaneous sub+superscript interlock — Rule 18d–e with `SubSuperscriptGapMin` and `SuperscriptBottomMaxWithSubscript` | Rule 18d–e (§H.6) | No combined-scripts node exists (MR-12 [P] `NodeScripts`); both constants extracted (`MathTypography.h:355-360`) with zero call sites. TXP-4 delivers the placement engine; MR-12 delivers the node | PARTIAL | TXP-4 (engine) + MR-12 (node) |
| GAP-23 | `SpaceAfterScript` kern after every script | OpenType constant 17 | Applied by `NodePower` (`MathAST.cpp:509-510`), `NodeSubscript` (`:1406-1409`), `NodeLogBase` (`:799-804`) via `spaceAfterScriptPx` (`MathAST.h:386-388`) | DONE | — |
| GAP-24 | Exponent baseline never below the bottom-min floor for nested/complex exponents | Rule 18c floor | Enforced: clamp at `layoutBottomMinShiftPx` with 1 px minimum (`MathAST.h:495-509`) | DONE | — |

### B.6 Fractions, stacks, bars

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-25 | Numerator/denominator shifts σ8–σ12 with gap-minimum enforcement (Rule 15b/15d ≙ OpenType shift constants + `Fraction*GapMin`) | Rule 15 (§H.7) | Shifts derived from a single symmetric gap `G = max(FractionNumDisplayStyleGapMin, FractionDenomDisplayStyleGapMin)` — **always the display-style minima**, regardless of style (`fractionBarGaps`, `MathAST.h:569-588`, `display=true` hardwired `:579-580`); the σ constants are computed by `fractionLayoutMetrics` (`MathAST.h:429-442`) but not used for placement (`MathAST.cpp:387-397`). Documented deliberate departure (`MathAST.h:560-566`) — replaced by the exact hybrid in TXP-5; the *readability* concern is preserved because at em 18 the OpenType gap minima dominate anyway (proof obligation in TXP-5 acceptance) | PARTIAL | TXP-5 |
| GAP-26 | Fraction rule thickness ξ8, never 0 px | Rule 15; `FractionRuleThickness` = 68 DU | `scalePxMin(…, 1)` (`MathTypography.h:417-420`; `MathAST.cpp:384-385`) | DONE (PD-1) | — |
| GAP-27 | Bar centered on the math axis | Rule 15; axis σ22 | Bar at `yBaseline − axisHeight()` with half-thickness split (`MathAST.cpp:408-415`, `MathRenderer.cpp:1753-1763`) | DONE | — |
| GAP-28 | No bar overhang beyond content | TeX `\over`: bar width = max(num, den) width [U] | 1 mu overhang added per side (`fractionLayoutMetrics` `MathAST.h:439`, consumed `MathAST.cpp:400-402`) — non-TeX; removed in TXP-5 (PD-4 consequence) | PARTIAL | TXP-5 |
| GAP-29 | Fraction ink bounds = child ink, not the layout box | — (feeds delimiter sizing) | `NodeFraction` reports `inkAscent = ascent` (`MathAST.cpp:423-424`), inflating enclosing delimiters | PARTIAL | TXP-5 |
| GAP-30 | Binomial/stack placement (`Stack*ShiftUp/Down`, `StackGapMin`) | Rule 15 bar-less case (§H.7) | No stack node ([P] MR-14 `NodeBinom`); constants extracted (`MathTypography.h:382-393`) unused. Layout-spec §6.3 already specifies the box — TXP contributes the §H.7 formula check only | OUT-OF-SCOPE (MR-14; formulas in §H.7) | — |
| GAP-31 | Periodic-decimal overline uses Overbar constants | OpenType `OverbarVerticalGap/RuleThickness/ExtraAscender` | Hardcoded `OVERLINE_T = 1`, `OVERLINE_GAP = 1` (`MathAST.h:1129-1132`; layout `MathAST.cpp:976-982`; draw `MathRenderer.cpp:2144-2149`); accessors exist (`MathTypography.h:447-455`) unused | PARTIAL | TXP-5 |

### B.7 Radicals

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-32 | Radical vertical gap (display vs text variant) + rule + extra ascender | Rule 11 ≙ `RadicalVerticalGap`(85)/`RadicalDisplayStyleVerticalGap`(170), `RadicalRuleThickness`, `RadicalExtraAscender` | Implemented with style-correct selection (`MathAST.cpp:572-576,600-604`) | DONE | — |
| GAP-33 | Degree placement: kern-before 65 DU, kern-after −335 DU, raised by `RadicalDegreeBottomRaisePercent` (55 %) **of the radical's height, measured from the radical bottom** | OpenType MATH constants 53–55 (§H.8) [U for the measurement convention] | Kerns correct (`MathAST.cpp:586-594`); raise computed as `em × 55 %` measured down from the radical **top** (`:607-616`; draw places degree top-aligned `MathRenderer.cpp:1918-1925`) — wrong quantity and wrong reference edge; coincides only for shallow radicands | PARTIAL | TXP-6 |
| GAP-34 | Radical glyph from the font (√ size variants / assembly), vector only as fallback | OpenType `MathVariants` for U+221A | Always a hand-drawn hook+slope (`RADICAL_HOOK_W=3, RADICAL_SLOPE_W=6`, `MathAST.h:881-883`; draw `MathRenderer.cpp:1870-1925`); no U+221A variant table exists (15 tables enumerated in `stix_math_variants.h` — delimiters + big ops only) | PARTIAL | TXP-1 (data) + TXP-6 (use) |
| GAP-35 | Radicand set in cramped style | Rule 11 (§H.8) | Radicand laid out with the parent's uncramped `fm` (`MathAST.cpp:568-570`) — depends on GAP-06 | MISSING | TXP-4 → TXP-6 |

### B.8 Big operators and limits

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-36 | Display-size operator glyph selection (`DisplayOperatorMinHeight`) | Rule 13 (§H.9); OpenType §4.5 | **Dead path at em 18**: the only "variant" is the base glyph (see §A.3-1); `largeOperatorTargetHeightPx` (`MathAST.h:358-363`) asks for 32 px, `assemble` returns 19 px, gate fails (`MathAST.h:410-415`) ⇒ Σ/∫ always small with side scripts | PARTIAL (effectively MISSING) | TXP-1 (PUA fonts) + TXP-7 |
| GAP-37 | Gate consistency across operator families | one rule for all Op nuclei | `NodeSummation`/`NodeDefIntegral` gate on glyph height (`MathAST.cpp:1070,1202`); `NodeBigOp` deliberately ignores the gate (`_useDisplayLimits = fm.isDisplayStyle()`, `MathAST.cpp:1319-1324`) | PARTIAL | TXP-7 |
| GAP-38 | Limit gaps and baseline minima: `UpperLimitGapMin`+`UpperLimitBaselineRiseMin` above, `LowerLimitGapMin`+`LowerLimitBaselineDropMin` below (Rule 13a) | Rule 13a (§H.9) | `upperLimitGapMin()` used for **both** sides (`MathAST.cpp:1064,1092-1099,1195,1330`; draw `MathRenderer.cpp:296-316`); `lowerLimitGapMin`/`…RiseMin`/`…DropMin` accessors (`MathTypography.h:367-378`) unused. STIX values happen to be equal (135/135) so no visible error **today**; the rise/drop minima (300/670 DU) are real and unenforced | PARTIAL | TXP-7 |
| GAP-39 | Operator vertically centered on the axis, in ink | Rule 13: shift nucleus by ½(h−d) − axis [U] | Layout reserves an axis-centered box (`largeOperatorGlyphAscentPx`, `MathAST.h:365-374`) but draw places the glyph at its natural baseline ink position (`drawTextBaseline(…, yBaseline, SYMB_SUM…)`, `MathRenderer.cpp:2273-2275`; same for ∫ `:2192-2195`) — box and ink disagree | PARTIAL | TXP-7 |
| GAP-40 | No invented padding around the operator | Rule 13/13a use only the named constants | `largeOperatorSymbolPadPx = 4 mu` inserted above and below the glyph before the limit gap (`MathAST.h:353-356`; `MathAST.cpp:1092-1099`; `MathRenderer.cpp:302-309`) — non-TeX; body gap of 3 mu (`MathAST.h:376-379`) is a defensible thin-space stand-in (Op→Ord = thin) kept by PD-11 | PARTIAL | TXP-7 |
| GAP-41 | Inline-limit (nolimits) placement uses Rule 18 script shifts | Rule 13/18 | Uses raw `SuperscriptShiftUp`/`SubscriptShiftDown` (`inlineLimitGeometry`, `MathRenderer.cpp:328-347`) without the Rule 18 interlock or the δ split (GAP-04) | PARTIAL | TXP-7 (after TXP-4 engine) |

### B.9 Delimiters and assemblies

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-42 | Delimiter sizing per Rule 19: `target = max(⌈901·size/1000⌉, size − shortfall)`, symmetric about axis, where `size = 2·max(h−axis, d+axis)` | Rule 19 (§H.10) [U]; PD-5 fixes NumOS values | Cover-everything + pad: `symmetricAxisDelimiterGeometry` (`MathAST.cpp:97-115`) + `delimiterVerticalPadPx` driven by `DelimitedSubFormulaMinHeight` (`:117-122`) — never undershoots, systematically taller than TeX; axis symmetry itself is correct | PARTIAL | TXP-8 |
| GAP-43 | No inner padding between delimiter and content | TeX inserts none [U] | `innerPad = max(1, parenWidth/3)` per side (`MathAST.cpp:669-670`; draw `MathRenderer.cpp:1939-1951`; also `NodeFunction`/`NodeLogBase` `MathAST.cpp:735-742,794-804`) | PARTIAL | TXP-8 |
| GAP-44 | Extensible assembly honors `MinConnectorOverlap` | OpenType MathVariants header field (§H.4) | Not extracted; assembler distributes overlap evenly but allows any seam down to 0 px (`MathGlyphAssembly.cpp:226-253`) | MISSING | TXP-1 (data) + TXP-8 (enforce) |
| GAP-45 | Assembly part flags / FSM constraints | OpenType §GlyphAssembly | Correct Gecko-grade FSM: ≤3 non-extenders, exactly one extender, bottom-to-top→top-to-bottom reorder (`MathGlyphAssembly.cpp:30-86`); `duToPxCeil` heights / `duToPxRound` widths (`:20-28`) | DONE | — |
| GAP-46 | Assembly glyphs present in the font subset | — | U+239B…U+23B3 intentionally absent (`generate_stix_math_font.sh:96-116` ranges; runtime probe `MathRenderer.cpp:431-435`); vector fallback covers (`:87-172`). MR-04/MR-11 own the re-subset + go-live; TXP-1 executes the regeneration jointly with its own data needs | PARTIAL (joint with MR-04/11) | TXP-1 |

### B.10 Cross-cutting

| ID | Capability | Normative rule | Current NumOS state [V] | Gap | Phase |
|---|---|---|---|---|---|
| GAP-47 | Matrices / cases / piecewise layout | `\vcenter` on axis; array strut rules | No grid node in any tree family; specified as `NodeMatrix` [P] with axis-centering in layout spec §9 and ticket MR-13. TXP supplies the §H formulas it must obey; implementation is MR-13's | OUT-OF-SCOPE (MR-13) | — |
| GAP-48 | Multi-line breaking of wide expressions on 320 px | TeX display-breaking penalties | Horizontal scroll implemented (`MathRenderer.cpp:762-783`, `_scrollX ≤ 0` `:650-654`); vertical viewport is MR-17. Line breaking rejected (layout spec §14, ratified by PD-9) | OUT-OF-SCOPE (PD-9) | — |
| GAP-49 | Accents (top attachment, `AccentBaseHeight`, `FlattenedAccentBaseHeight`, wide accents) | OpenType MATH accent model | No accent node (notation spec §T marks accents F); constants extracted (`MathTypography.h:471-476`) unused; `MathTopAccentAttachment` not extracted. TXP-1 extracts the data so the future accent ticket has it; layout stays future | OUT-OF-SCOPE (data in TXP-1; layout F) | TXP-1 (data only) |
| GAP-50 | Phantom/strut equivalents | `\vphantom`/`\strut` | No phantom node. Empty `NodeRow` reports full font ascent/descent (`MathAST.cpp:173-179`) — an implicit strut that keeps empty slots line-height tall; sufficient for the current notation surface (PD-13) | OUT-OF-SCOPE (PD-13) | — |
| GAP-51 | Highlight/color never changes geometry | — (Smart Highlighter contract) | Color-only by construction: highlight state only selects the draw color inside draw calls (`setHighlightNode` `MathRenderer.cpp:637-648`; per-node color picks `:1657,1670,2073,2092`) and no layout function reads `_highlightNode`/`_highlightActive` (grep-verified in `MathAST.cpp`) | DONE | — |
| GAP-52 | Cursor geometry consistent with layout under all new placements | — (edit-mode correctness) | Cursor Finder duplicates every placement formula (`MathRenderer.cpp:911-1232`) and `childXOffset` mirrors row spacing (`:1438-1461`); correct today, but every TXP wave MUST update it in lockstep (§G.6) and prove it with cursor-trace asserts | PARTIAL (process) | every wave |
| GAP-53 | Number vs identifier font policy | TeX: digits upright, 1-letter identifiers italic [U] | Digits upright [V correct]; identifiers upright (GAP-18); function words upright [V correct `MathRenderer.cpp:1981-1983`]; `Ans`/`PreAns` word chips upright (correct as UI words, PD-3) | PARTIAL (PD-3) | TXP-3 |

---

## §C. Decision records (PD-1…PD-14)

Format per record: **Context → Options → Decision → Consequence.** Every deviation from TeX that
survives TXP-10 MUST be traceable to exactly one PD below.

### PD-1 — Pixel-grid rounding policy (ratifies existing)
- **Context.** All layout is integer `int16_t` pixels; DU→px conversion loses sub-pixel
  information; hairlines can round to 0.
- **Options.** (a) round-to-nearest everywhere; (b) mixed policy — round-to-nearest for shifts,
  ceiling for heights that must cover, minimum-pixel floors for rules; (c) fixed-point 26.6 layout.
- **Decision.** (b), exactly as already implemented [V]: `MathConstantsProvider::scalePx`
  round-to-nearest sign-correct (`MathTypography.h:274-279`), `scalePxMin(du, 1)` for every rule
  thickness (`:284-288`, used `:417-420,428-430`), `DelimiterAssembler::duToPxCeil` for glyph
  heights and `duToPxRound` for widths (`MathGlyphAssembly.cpp:20-28`). Every TXP formula MUST
  name which of these three converters it uses. The truncating `duToPx`
  (`MathTypography.h:126-128,293-295`) is **deprecated for new code**; TXP-4 removes its one
  geometry call site (`MathAST.cpp:492`).
- **Consequence.** Any single converted constant deviates from the real-valued TeX result by at
  most 0.5 px (1.0 px for ceilinged heights); acceptance tolerances in §TXP-0 0.5.4 are set to
  ±1 px per feature accordingly. Sums of k converted terms may deviate by up to k/2 px; formulas
  in Part II are therefore written to convert **once at the end** where possible (convert the DU
  sum, not the sum of converted DUs) — each phase's algorithm states its conversion points.

### PD-2 — Integer arithmetic, no fixed point
- **Context.** TeX computes in scaled points (2^−16 pt); NumOS in `int16_t` px.
- **Options.** (a) keep int16 px; (b) migrate layout to 26.6 fixed point and round at draw.
- **Decision.** (a). The display is 320×240; sub-pixel accumulation across a 320 px row is
  bounded and the ±1 px acceptance band absorbs it. 26.6 would double the diff surface of every
  layout file for zero visible gain at em 18.
- **Consequence.** TXP acceptance criteria are stated in integer px with explicit tolerance; the
  reference oracle rounds its real-valued reference metrics to the same grid before comparing.

### PD-3 — Identifier styling: math-italic single-letter variables, upright everything else
- **Context.** TeX sets single-letter identifiers in math italic; NumOS draws upright ASCII
  (`MathRenderer.cpp:2086-2095`) [V]. The italic alphabet U+1D400–U+1D7FF is already subset
  [V `stix_math_18.c:4`]. A calculator audience is used to both conventions (Casio upright,
  TI-Nspire/textbooks italic).
- **Options.** (a) keep upright and normalize the reference (`\mathrm` wrap of every identifier);
  (b) adopt math italic for single Latin letters, keep digits, function words, `Ans`/`PreAns`,
  and multi-letter names upright.
- **Decision.** (b) — see OQ-1 (recommended default = adopt). Mapping: lowercase `a–z` →
  U+1D44E+(c−'a') with the single exception `h` → U+210E (planck; the U+1D455 slot is unassigned
  [U — Unicode]); uppercase `A–Z` → U+1D434+(c−'A'). `NodeConstant` π/e keep their current
  glyphs and blue color (`MathRenderer.cpp:2070-2075` [V]); `Ans`/`PreAns` stay upright word
  chips; NodeFunction labels stay upright (TeX `\sin` is upright — matches).
- **Consequence.** Every golden showing a variable re-blesses in the TXP-3 wave. Italic
  correction (GAP-01/02) becomes *visible* — which is why TXP-3 (italics) precedes TXP-4
  (scripts) in the wave order. The digit/identifier policy becomes: digits upright [V unchanged],
  1-letter identifiers italic, words upright — identical to TeX defaults, so the reference
  needs no normalization.

### PD-4 — Fraction *size* policy kept; fraction *placement* goes exact; references use `\cfrac`
- **Context.** NumOS deliberately keeps numerator/denominator at full glyph size at script level
  0 (readable-fraction policy, `fractionPartMetrics`, `MathAST.h:314-326` [V]; rationale: 8 px
  glyphs are illegible on this TFT, `:310-312`). Separately, the bar *placement* uses a
  symmetric-gap house formula (GAP-25) and a 1 mu overhang (GAP-28).
- **Options.** (a) full TeX (size demotion + exact shifts); (b) keep both house policies and PD
  the whole fraction; (c) keep the size policy, restore exact OpenType shift+gap placement.
- **Decision.** (c). Size policy is a genuine pixel-grid/legibility constraint — kept and
  normalized in the oracle by rendering fraction references as `\cfrac` (nested) / `\dfrac`
  (top-level), which reproduce the constant-size behavior in TeX itself. Placement has no such
  excuse: TXP-5 implements Rule 15b/15d via the OpenType constants (shift constants σ8–σ12 with
  `Fraction*GapMin` enforcement, style-correct display/text selection) and deletes the overhang.
- **Consequence.** Fraction goldens re-bless in TXP-5. The visible change is bounded: at em 18
  the gap minima dominate the shift constants for 1-line children, so simple fractions move ≤1 px;
  tall children (nested fractions) tighten toward TeX. The TXP-5 acceptance suite carries the
  before/after table proving the bound.

### PD-5 — Delimiter sizing adopts TeX Rule 19 with TeX plain values
- **Context.** GAP-42: NumOS delimiters always fully cover content plus a pad; TeX sizes
  delimiters to `max(⌈f·size/1000⌉, size − shortfall)` with plain-TeX `\delimiterfactor f = 901`,
  `\delimitershortfall = 5 pt` at 10 pt ⇒ 0.5 em [U — TeXbook Rule 19 / plain.tex].
- **Options.** (a) keep cover+pad (never undershoot); (b) Rule 19 with TeX plain values scaled to
  em (`shortfall = round(0.5 em) = 9 px @ em 18`); (c) Rule 19 with a smaller shortfall for the
  small screen.
- **Decision.** (b), with two NumOS floors: the result is never below the base glyph height
  (`duToPxCeil(baseHeight)`), and never below the content of an `NodeEmpty`-only slot's line box.
  See OQ-2 for the shortfall value.
- **Consequence.** Delimiters may now be shorter than extreme content, exactly like TeX — the
  familiar "parens don't quite reach the outer fraction bars" look. Delimiter-bearing goldens
  re-bless in TXP-8 (coordinated with MR-11's assembly-glyph wave so there is **one** delimiter
  re-bless, not two).

### PD-6 — Cramped styles adopted (supersedes MRV2 layout spec §14 row 1)
- **Context.** §14 rejected cramped styles as "~1 px, doubles the style lattice". The audit
  falsifies both halves: `SuperscriptShiftUp` 360 DU → 6 px vs `SuperscriptShiftUpCramped`
  252 DU → 5 px @ em 18 (round-to-nearest), and at em 12 (script level) 4 px vs 3 px — a 1–2 px
  systematic error in exactly the positions (denominators, radicands) where stacked constructs
  make it visible; and the lattice does not double — cramping is 1 bool in `FontMetrics`, not new
  styles, because the only cramped-sensitive constant NumOS uses is the superscript shift.
- **Options.** (a) keep rejection; (b) full cramped lattice (8 styles); (c) a `cramped` flag on
  `FontMetrics` consulted exactly where OpenType has cramped variants.
- **Decision.** (c) in TXP-4. Cramping set by: fraction numerator — no; fraction denominator —
  yes; radicand — yes; subscript — yes; under-limit (lower) — yes; overline body — yes (future);
  superscript of a cramped context stays cramped. (TeXbook style table [U]; OQ-6 confirms the
  slot list.)
- **Consequence.** +1 byte in `FontMetrics` (host-tested struct, no ABI concern); goldens with
  superscripts inside denominators/radicands re-bless in TXP-4.

### PD-7 — MathKernInfo cut-in kerning adopted (supersedes MRV2 layout spec §14 row 3)
- **Context.** §14 rejected cut-ins as "subpixel-level; not extracted from the font". The STIX
  cut-in tables carry corrections up to ~0.2 em [U — STIX MATH table] ⇒ 2–4 px @ em 18, the same
  magnitude as the italic corrections the repo already applies; and "not extracted" is a pipeline
  gap (TXP-1), not a rationale.
- **Options.** (a) keep rejection; (b) full 4-corner staircase kerning; (c) TopRight+BottomRight
  only (the sub/superscript-after-base cases NumOS can render today).
- **Decision.** (c) in TXP-9, extending to TopLeft/BottomLeft when prescripts ever exist. Classic
  TeX has no equivalent — where Appendix G and OpenType disagree here, OpenType wins because the
  reference render engines (MathJax/KaTeX with real OpenType fonts) apply it, and parity is
  measured against them.
- **Consequence.** Flash cost of the kern table (measured in TXP-1, budget ≤ 24 KB — abort
  criterion in the phase); script-bearing goldens re-bless in TXP-9 (bounded: only pairs whose
  kern value rounds to ≥1 px move).

### PD-8 — Spacing-table fidelity via an effective-class pass (not per-node mutation)
- **Context.** GAP-11/12/13. Bin demotion is contextual; NumOS classes are static virtuals
  (`MathAST.h:607-616`).
- **Options.** (a) mutate node classes during layout (stateful, breaks Y-6 determinism if a node
  is reused); (b) compute an *effective class array* per row inside `NodeRow::calculateLayout`
  and share it with draw/cursor through the same helper; (c) full mlist translation pass.
- **Decision.** (b): a pure function
  `effectiveMathClasses(const NodeRow&, EffectiveClass out[], int n)` (TXP-2 §design) computed
  from the static classes by applying the demotion rules of §H.5 — no node state, deterministic,
  and callable identically from the three consumers (layout / draw / cursor), preserving the DRY
  invariant GAP-15.
- **Consequence.** Row layout becomes two-pass (classify then space) — O(n) extra, zero heap
  (fixed stack array bounded by the row-length cap from MR-01; until MR-01 lands, a 64-entry
  stack array with silent fallback to static classes beyond 64 — documented in TXP-2).

### PD-9 — No formula line-breaking (ratifies MRV2 layout spec §14 row 4)
- **Context.** 320 px wide screen; TeX breaks displayed math with penalties.
- **Decision.** Ratified as permanent for TXP: horizontal scroll [V `MathRenderer.cpp:762-783`]
  plus MR-17 vertical viewport are the mechanism. TXP-10's parity report measures corpus items
  against references **unwrapped** at full natural width.
- **Consequence.** Corpus comparisons are width-unbounded; on-device wide items scroll.

### PD-10 — Physical script sizes 18/12/8 kept; oracle normalizes by baseline positions
- **Context.** GAP-08: 12/18 = 66.7 % and 8/18 = 44.4 % vs OpenType 70 %/55 %. LVGL bitmap fonts
  cannot scale at runtime; a 4th size costs ≥400 KB flash (rejected, MRV2 §7 [V spec]).
- **Decision.** Keep the trio. The oracle (§TXP-0 0.5.4) compares **baseline-relative positions**
  (shifts, gaps, bar positions) — which the MATH constants define independent of glyph size —
  and does NOT compare script glyph bounding-box heights against the reference. Script-size
  disagreement is measured once, reported as a constant known bias in the TXP-10 report, and
  excluded from pass/fail.
- **Consequence.** A trained eye comparing side-by-side sees NumOS scripts ~3 % larger than
  TeX's at level 1 — declared acceptable; every *position* still matches.

### PD-11 — Operator-body gap: keep 3 mu as the Op→operand thin space; drop the 4 mu symbol pad
- **Context.** GAP-40. TeX inserts a thin space (3 mu) between an Op atom and a following Ord —
  NumOS's `largeOperatorBodyGapPx = 3 mu` (`MathAST.h:376-379` [V]) is exactly that and is kept
  (it stands in for the row-level Op→X spacing the composite node body swallows). The extra
  `largeOperatorSymbolPadPx = 4 mu` above/below the glyph (`MathAST.h:353-356`) has no TeX
  counterpart.
- **Decision.** Keep the 3 mu body gap (documented as the internalized Op→Ord thin space; when a
  future refactor makes big-op bodies row siblings, it must be removed in the same PR that lets
  the row space them). Delete the 4 mu vertical pad in TXP-7.
- **Consequence.** Limits sit `GapMin` from the glyph ink, as in the reference; big-op goldens
  re-bless in TXP-7.

### PD-12 — Rollback flag mechanism: one legacy build flag per geometry wave
- **Context.** Each TXP wave changes goldens; the repo precedents are compile-time A/B flags
  (`NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX`, `MathAST.h:390-399` [V]; MR-05's planned
  `NUMOS_MATH_LAYOUT_CACHE_OFF`).
- **Decision.** Every geometry wave TXP-n (n ∈ 2…9) guards its formula changes behind
  `#if !defined(NUMOS_TXP<n>_LEGACY)` — default **new** behavior; defining the flag restores the
  pre-wave formulas bit-exactly. CI builds default only. The flag and its legacy code are deleted
  two waves later (e.g. TXP-2's flag is removed in the TXP-4 PR) once the wave's goldens have
  survived. Rollback of a shipped wave = one-line `platformio.ini` define + golden re-bless back,
  no revert surgery.
- **Consequence.** Transient dual-path code (bounded to ≤2 concurrent legacy flags by the
  deletion rule); each phase's Rollback section names its flag.

### PD-13 — No phantom/strut node
- **Context.** GAP-50. TeX uses `\vphantom`/`\strut` to stabilize heights.
- **Decision.** Not needed: empty rows already report full font ascent/descent
  (`MathAST.cpp:173-179` [V]) — the strut behavior falls out; nothing in the notation surface
  (including MR-13 matrices, whose row-strut rule is in layout spec §9) requires user-visible
  phantoms. Revisit only if the future accent/underbrace work needs it.
- **Consequence.** None today; recorded so future agents do not invent one.

### PD-14 — Reference engine: MathJax-SVG metrics primary, KaTeX PNG gallery secondary
- **Context.** Parity needs a locally-runnable ground-truth pipeline. Raw pixel-diffing against
  a foreign rasterizer is meaningless (different anti-aliasing/hinting), so the comparison must
  be geometric.
- **Options.** (a) KaTeX in headless Chromium → PNG → ink-component analysis; (b) MathJax v3
  `tex2svg` in plain Node (no browser) → parse SVG geometry directly → exact positions; (c) a
  local LuaTeX + `\showbox` parse.
- **Decision.** (b) as the **metric oracle** — deterministic vector output, no rasterizer, no
  browser dependency, same Appendix G + OpenType algorithms [U — MathJax implements the OpenType
  MATH model]; plus (a) as an optional **human gallery** generator (side-by-side PNG pairs for
  the TXP-10 review; may use the pre-installed Chromium on dev machines; never gates CI).
  (c) rejected: TeX Live is a heavyweight, non-hermetic dependency.
- **Consequence.** Tool spec in §TXP-0 0.5; `tools/tex-parity-oracle/` (Node, `package.json` pins
  `mathjax@3.2.x`); never enters any PlatformIO env (`tools/` is outside every
  `build_src_filter` [V `platformio.ini:195-266` whitelists only `src/` paths]).

---

## §D. Open questions (OQ-1…OQ-6)

An agent that reaches an OQ MUST take the recommended default, note it in its PR description, and
MUST NOT stall. The maintainer can overturn a default by editing this section before the phase
lands.

| ID | Question | Options | Recommended default (rationale) |
|---|---|---|---|
| OQ-1 | Adopt math-italic single-letter variables (PD-3)? | (a) adopt italic; (b) keep upright + `\mathrm`-normalize references | **(a)** — reference-true, glyphs already in the subset, one re-bless wave; upright was never a recorded decision, just the default of drawing ASCII |
| OQ-2 | `delimitershortfall` value at em 18 | (a) 9 px (= 0.5 em, TeX plain scaled); (b) 5 px; (c) 3 px | **(a)** 9 px stored as `round(em/2)` so it scales with style — parity with the reference engines, which use TeX plain values; if TXP-8's gallery shows objectionable undershoot on this panel, drop to (b) in a follow-up PD amendment |
| OQ-3 | How does the oracle treat script glyph size mismatch (PD-10)? | (a) exclude script-size metrics from pass/fail, report bias; (b) scale reference scripts to 66.7 % via macros | **(a)** — (b) distorts the reference's own gap arithmetic and stops it being "what TeX does" |
| OQ-4 | Fix `BINARY→OP` table cell to medium-(2) in TXP-2? | (a) yes; (b) keep 9 | **(a)** — pure fidelity fix; visible in `x + ∑…` corpus items; the 9 was a transcription artifact, not a decision (comment cites LyX, and LyX's own table has (2) here [U]) |
| OQ-5 | Switch the radical to font glyph/variants when TXP-1 fonts provide U+221A variants? | (a) yes, vector stays fallback; (b) keep vector permanently | **(a)** — the hand hook (3+6 px fixed, `MathAST.h:881-883`) does not scale its slope with radicand height like the real glyph ladder; vector remains the permanent fallback tier per layout spec §12 |
| OQ-6 | Cramped-slot list (PD-6) | (a) TeX-complete: denominator, radicand, subscript, lower limit, overline body; (b) denominator+radicand only | **(a)** — the flag costs the same either way; partial cramping would be a new, third convention |

---

## §E. Risk-register additions (extend `NUMOS_RISK_REGISTER.md` §3, REN-6…REN-11)

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| REN-6 | Golden churn fatigue: 8 geometry waves × ~19 goldens ⇒ ~100+ human re-bless events; reviewer rubber-stamps a regression | H | H | human diff review only | one wave = one PR = one coordinated re-bless (golden-policy spec §D.1); every wave PR carries the oracle before/after metric table so the reviewer checks numbers, not vibes; PD-12 legacy flag allows instant A/B | Human + Fable | oracle metric diff attached to each wave PR |
| REN-7 | Font pipeline fragility: TXP-1 regenerates 3 fonts + 3 data headers + PUA mapping in one wave; a generator bug silently shifts *existing* glyph bitmaps | M | H | candidate diff non-empty on a byte-identical-required ticket | TXP-1 acceptance requires empty candidate diff for all existing stems; generator runs are reproducible (pinned lv_font_conv + fonttools versions recorded in the header comment [V pattern `stix_math_constants.h:3-4`]) | Opus + Human (flash sign-off) | TXP-1 §7 |
| REN-8 | Flash growth: assembly glyphs + PUA variants + MathKernInfo tables across 3 font sizes exceed headroom (1.35 MB known headroom, MRV2 §7 [V spec]) | M | M | size report in TXP-1 PR | hard abort criteria: fonts delta ≤ 120 KB, kern/variant headers ≤ 24 KB flash; if exceeded, drop the 8 px size from PUA variants first (scriptscript display operators are unreachable anyway) | Opus | TXP-1 §7 |
| REN-9 | Frame-time regression from per-glyph advance measurement (TXP-3) and effective-class pass (TXP-2) on device | M | M | `[MATH-STRESS] draw_us` lines (`MathRenderer.cpp:885-892` [V]) | both passes are O(row length) integer work with no heap; budget: stress-suite frame ≤ 16 ms unchanged (`kFrameBudgetUs=16000` per MRV2 §6 [V spec]); measured on validate firmware per wave | Fable | each wave's §7 firmware step |
| REN-10 | Oracle drift: MathJax version bump changes reference metrics ⇒ CI flips red with no NumOS change | M | M | oracle self-test corpus hash | pin `mathjax` exact version in `tools/tex-parity-oracle/package.json` + commit `package-lock.json`; reference metrics are **committed JSON** regenerated only deliberately (same promotion discipline as goldens) | Sonnet | TXP-0 §7 negative control |
| REN-11 | Cursor/layout formula divergence: a wave updates layout+draw but misses the cursor Finder (`MathRenderer.cpp:911-1232`) | H (8 chances) | M | cursor visibly misplaced in edit mode | §G.6 lockstep rule + mandatory cursor-trace `.numos` script per wave (asserts cursor x/y via `assert_cursor_role` + MR-01 accessors when available; until then via golden of an edit-mode screen with cursor visible at a known blink phase — deterministic ticks make the phase reproducible [V determinism contract]) | every wave agent | per-wave cursor script |

---

## §F. Effort & sequencing summary

Sizes are honest lower bounds (files touched / net LOC incl. tests / new tests). "Bless" = number
of existing goldens expected to need re-blessing (upper bound = all 19).

| Phase | Objective (1 line) | Files | LOC | New tests | Bless | Depends on |
|---|---|---|---|---|---|---|
| TXP-0 | Corpus + oracle + baseline metric freeze | ~10 (tools/, tests/, docs/) | 1,200–2,000 | 64 corpus metric snapshots + 3 negative controls | 0 (byte-identical) | MR-02 desirable (bootstraps subset if absent) |
| TXP-1 | Extractor + fonts: kerns, overlap, PUA variants, assembly glyphs | ~9 (scripts/, src/math/font/, src/fonts/) | 800–1,400 (excl. generated) | extractor unit tests + probe diagnostics | 0 (byte-identical, hard gate) | none (subsumes MR-04) |
| TXP-2 | Spacing fidelity: conditional entries + Bin demotion | 4–6 | 250–450 | 12 host spacing cases + 2 scripts | 3–6 | TXP-0 |
| TXP-3 | True advances + math-italic identifiers + ± glyph | 5–7 | 300–500 | 8 host + 2 scripts | up to 19 (most show text) | TXP-0; PD-3/OQ-1 |
| TXP-4 | Full Rule 18 scripts + cramped flag | 6–8 | 400–700 | 14 host + 2 scripts | 6–10 | TXP-3 |
| TXP-5 | Fraction σ-shift hybrid, overhang removal, overbar constants, ink honesty | 4–6 | 250–450 | 10 host + 2 scripts | 8–12 | TXP-4 (ink terms) |
| TXP-6 | Radical degree fix + glyph radical + cramped radicand | 4–6 | 200–400 | 8 host + 2 scripts | 2–4 | TXP-1, TXP-4 |
| TXP-7 | Big-op display variants, exact limit constants, axis ink, δ split | 5–7 | 400–650 | 12 host + 2 scripts | 4–8 | TXP-1, TXP-4 |
| TXP-8 | Rule 19 delimiters + MinConnectorOverlap + innerPad removal | 5–7 | 300–500 | 10 host + 2 scripts | 8–14 (co-blessed with MR-11 if concurrent) | TXP-1; coordinates MR-11 |
| TXP-9 | MathKernInfo cut-in kerning | 4–6 | 250–400 | 8 host + 2 scripts | 4–8 | TXP-1, TXP-4 |
| TXP-10 | Corpus re-run, parity report, matrix update | 3 (docs/, tests/) | 300–600 (mostly doc) | full-corpus oracle run in CI | 0 | all |

Sequencing constraints (hard): TXP-0 → everything; TXP-1 → {6, 7, 8, 9}; TXP-3 → TXP-4 → {5, 6,
7, 9}. TXP-2 is independent after TXP-0 and can run in parallel with TXP-1. Recommended serial
order for a single agent: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10.

---

# Part II — Agent implementation program

## §G. Preamble — binding on every phase agent

An agent executing any TXP phase MUST read this section and §H before writing code. Assume you
have NOT read any other conversation.

### G.1 Non-negotiable repo constraints (reprinted; violations are automatic PR rejection)

1. **Emulator compiles a whitelist, not the tree** — `platformio.ini:195-266`. New `src/` files
   that the emulator must build require explicit `build_src_filter` entries. New `tools/` and
   `scripts/` content is never compiled by PlatformIO.
2. **Giac never builds natively** — `lib_ignore = giac, libtommath` (`platformio.ini:192-194`).
   TXP work never touches Giac.
3. **KeyCode enum is append-only and digits are non-contiguous** (`src/input/KeyCodes.h`). Never
   write `code - NUM_0` or range tests; use `keyCodeDigitValue()`. CI scans for violations
   (`emulator-build.yml:96-104`).
4. **LVGL draw buffer**: single 32 KB `MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA` buffer
   (`main.cpp:138-142` per context header). TXP never allocates render buffers; MathCanvas owns
   zero pixel buffers (MRV2 invariant Y-9) and that stays true.
5. **Memory policy** (`NUMOS_MEMORY_ALLOCATION_POLICY.md`): AST nodes and any new long-lived math
   data → PSRAM (`MathNode::operator new`, `MathAST.cpp:50-60` [V]); static tables (kern tables,
   variant tables) → flash via `constexpr` arrays exactly like `stix_math_italics.h`; **no bulk
   ≥4 KB allocation may fall back to internal RAM**; zero heap allocation in the draw path
   (MRV2 Y-5).
6. **Goldens are byte-exact** P6 PPMs; mismatch fails CI; promotion is human-only via
   `scripts/promote-emulator-golden.py`; never hand-edit PPMs or masks; masks are the clock rect
   only (`tests/emulator/masks/README.md` per golden-policy spec §E). Always run scripts with
   `--deterministic`.
7. **`.numos` grammar and NativeHal script machinery are append-only** (exit-code contract
   0/2/3/4).
8. **`src/math/**` stays LVGL-free and Arduino-free** (MRV2 B-2) — layout formulas go in
   `src/math/`, drawing in `src/ui/`. Host tests compile layout with plain g++.
9. **On Linux/macOS/CI set `PLATFORMIO_BUILD_DIR=.pio/build`** before any `pio` command
   (`platformio.ini:6` is a Windows path).
10. **Generated files are generator-only**: `src/fonts/stix_math_*.c`,
    `src/math/font/stix_math_*.h` are regenerated by `scripts/generate_stix_math_font.sh` /
    `scripts/extract_stix_math.py`; hand edits are forbidden and will be overwritten
    (`stix_math_constants.h:11` "DO NOT EDIT MANUALLY").
11. **Comment language**: the codebase mixes Spanish and English [V throughout]; write new
    comments in English (the convention of all post-audit code), never mass-translate existing
    Spanish comments (churn risk, risk register DX-4).

### G.2 Unit system (used by every formula in this document)

- **DU** — font design units, UPM = 1000 (`kStixMathUPM`, `stix_math_constants.h:22`).
- **px** — integer screen pixels (`int16_t`).
- **em** — the current style's font size in px: 18 (level 0), 12 (level 1), 8 (level 2)
  (`ui/MathTypography.cpp:26-28`).
- **mu** — math unit, 1 mu = em/18 **of the current style** (`muToPx`,
  `src/math/MathTypography.h:133-135`).
- **Conversions** (PD-1): `px = scalePx(DU)` round-to-nearest (`MathTypography.h:274-279`);
  `px = scalePxMin(DU, 1)` for rules; `px = duToPxCeil(DU)` for glyph/assembly heights
  (`MathGlyphAssembly.cpp:20-23`); `px = duToPxRound(DU)` for glyph widths (`:25-28`).
  The truncating `duToPx` is deprecated (PD-1).
- Every named constant in a formula below is either `mc.<accessor>()` on
  `MathConstantsProvider mc(fm.emSize)` — returning px — or an explicit DU value converted at a
  stated point.

### G.3 Style and metrics vocabulary

- `fm` — the current `FontMetrics` (`MathAST.h:208-291`): `ascent/descent` are ink metrics from
  glyph probes, `lineAscent/lineDescent` the LVGL line box, `numberAscent/numberDescent` digit
  ink, `capHeight` from 'M' (`metricsFromFont`, `MathRenderer.cpp:446-535`).
- `fm.superscript()` — one script level down (`MathAST.h:249-290`).
- `fractionPartMetrics(fm)` — the readable-fraction child metrics (`MathAST.h:314-326`, PD-4).
- **x-height stand-in**: the OpenType constants already encode TeX's x-height terms
  (`SuperscriptBottomMin` ≈ ¼·x-height rule, `SubscriptTopMax` ≈ ⅘·x-height rule,
  `SuperscriptBottomMaxWithSubscript` ≈ ⅘·x-height) — formulas below use the OpenType constants
  directly and never need a live x-height metric.
- **Ink vs layout box**: where Appendix G says "height/depth of the box", TXP formulas use the
  **ink** bounds (`layoutInkAscentPx/layoutInkDescentPx`, `MathAST.h:461-471`) when positioning
  against rules/gaps (bar gaps, radical gaps, limit gaps, script bottom/top clamps) and the
  **layout** bounds when accumulating box extents. Each formula tags which it uses.

### G.4 Feature flags (PD-12)

Geometry waves guard changed formulas with `#if !defined(NUMOS_TXP<n>_LEGACY)`. Rules: default =
new behavior; the flag compiles the **entire pre-wave formula set** of that wave (bit-exact,
verified by building with the flag and comparing candidates against the pre-wave goldens);
declared in `platformio.ini` comments next to the validate envs [V pattern
`platformio.ini:116-137`]; deleted two waves later.

### G.5 Golden workflow per geometry wave (REN-6 discipline)

1. Land code + host tests with the wave flag OFF (new behavior).
2. `python scripts/generate-emulator-candidates.py` → inspect every diff (use
   `scripts/compare-ppm.py --write-diff`).
3. Run the TXP oracle (TXP-0) → attach the per-item metric before/after table to the PR.
4. Human re-blesses via `scripts/promote-emulator-golden.py` — never the agent.
5. New TXP `.numos` stems enter the candidate list warning-only until blessed (golden-policy
   spec §C; add them to `tests/emulator/golden/PENDING.md` if that mechanism has landed, else
   note them in the PR).

### G.6 Cursor lockstep rule (REN-11)

Any PR that changes a placement formula MUST change the same formula in all THREE consumers in
the same commit:
1. layout (`src/math/MathAST.cpp` / new `src/math/layout/` unit if MR-05 has landed),
2. draw (`src/ui/MathRenderer.cpp` draw*Baseline / geometry helpers),
3. cursor Finder (`MathCanvas::computeCursorPosition`, `MathRenderer.cpp:911-1232`, and
   `childXOffset`, `:1438-1461`).
The long-term fix (single shared placement functions) is MR-05's policy-extraction work; until it
lands, the duplication is a fact and this rule is the guard. Each wave adds one edit-mode cursor
`.numos` script (§E REN-11) proving the mirror.

### G.7 Testing vehicles available to phases

- **Host geometry harness** (MR-02, `tests/host/mathlayout_geometry_test.cpp` [P]): if it exists,
  add cases there; if it does not exist yet, TXP-0 bootstraps the minimal variant specified in
  the test-oracle plan §3 (g++ + the three `stix_math_*.c` + font shim; the emulator whitelist
  proves these sources compile on host, `platformio.ini:195-266`).
- **Emulator `.numos` scripts** with `assert_*` (semantic) and screenshot capture; MR-01's
  `assert_math_*` commands when landed.
- **TXP oracle** (TXP-0): corpus metrics vs committed reference JSON.
- **Firmware**: compile `esp32s3_n16r8`; geometry waves additionally run the stress diagnostics
  suite on the validate env when hardware is available (HIL steps are marked optional-with-owner
  in each phase; absence of hardware does not block a merge, per the emulator-first evidence
  policy of the ground truth §I).

### G.8 `.numos` script conventions for TXP

Grammar is the existing NativeHal vocabulary only (append-only contract): `log`, `wait N`,
`key NAME`, `open_app NAME`, `screenshot PATH`, `assert_app`, `assert_result[_contains]`,
`assert_no_error`, `assert_error`, `assert_variable`, `assert_menu_focus` — plus the MR-01
`assert_math_*` family **only if MR-01 has landed** (check `src/hal/NativeHal.cpp` before use;
never invent commands). Two script archetypes, used by every wave (full listings in Appendix C):

1. **Showcase capture** — corpus items that cannot be typed: `open_app MathShowcase`, `key right`
   × showcaseIndex (from `corpus.json`), `wait 20`, `screenshot out/<stem>.ppm`,
   `assert_app MathShowcase`. One scenario per script; runner flags always
   `--headless --deterministic --frames ≤1400 --quiet`.
2. **Typed edit + cursor** (REN-11 scripts) — `open_app Calculation`, `key` sequence building the
   expression (VPAM capture semantics: `(` opens a paren you exit with `key RIGHT`, `/` inserts a
   fraction capturing the left operand — see the annotated precedent
   `tests/emulator/scripts/calc_semantic_parentheses.numos` [V]), then a cursor-visible
   `screenshot` (deterministic ticks make the blink phase reproducible) and semantic
   `assert_result*` where the expression is evaluable.

New stems: add to `scripts/generate-emulator-candidates.py` in the same PR (warning-only until
blessed, golden-policy spec §C); goldens land under `tests/emulator/golden/<stem>.ppm` with a
mask **only if** the capture shows the StatusBar clock (showcase and calc screens do — reuse the
standard rect `4,6,37,13`; never mask math-canvas pixels).

### G.9 Per-phase RAM/flash budget rule

Every TXP PR records flash and RAM deltas in the MRV2 "MA format" (firmware `pio run` size lines
before/after). Budgets against `NUMOS_MEMORY_ALLOCATION_POLICY.md`: TXP-1 per REN-8 (fonts
≤ +120 KB flash, data headers ≤ +24 KB); every other wave ≤ +4 KB flash, **0 B** new heap in any
render/layout path (MRV2 Y-5; stack-array patterns only — `kEffClassMaxRow`,
`kNormalizedTextBufferBytes` precedents), and no new PSRAM allocations at all. Exceeding a
budget is an abort-and-descope event, not a judgment call.

---

## §H. Normative reference layer

This section is the single place where external TeX/OpenType rules are transcribed. Phases cite
`§H.x` and never re-derive. All Appendix G content is **[U]** (reconstructed from the TeXbook,
Appendix G rules 1–22, and the Microsoft OpenType MATH 1.9.1 spec; not re-verified online this
session) — transcription fidelity is itself checked operationally by TXP-0's oracle, because
MathJax implements the same rules.

### H.1 TeX parameter ↔ OpenType MATH constant ↔ NumOS accessor map

σ = symbol-font fontdimen (family 2), ξ = extension-font fontdimen (family 3). "Accessor" is on
`MathConstantsProvider` (`src/math/MathTypography.h`), value column is STIX Two Math DU
(`stix_math_constants.h:37-94` [V]).

| TeX | Meaning | OpenType constant | DU | Accessor (px) |
|---|---|---|---|---|
| σ5 (x-height) | x-height | (encoded into the Min/Max constants below) | — | — |
| σ8 | num shift up, display | `FractionNumeratorDisplayStyleShiftUp` | 640 | `fractionNumeratorShiftUp(true)` |
| σ9 | num shift up, text | `FractionNumeratorShiftUp` | 585 | `fractionNumeratorShiftUp(false)` |
| σ10 | (atop shift; unused — NumOS has no bar-less fraction; `NodeBinom` uses Stack constants) | `StackTopShiftUp` / `StackTopDisplayStyleShiftUp` | 470 / 780 | `stackTopShiftUp(display)` |
| σ11 | denom shift down, display | `FractionDenominatorDisplayStyleShiftDown` | 640 | `fractionDenominatorShiftDown(true)` |
| σ12 | denom shift down, text | `FractionDenominatorShiftDown` | 585 | `fractionDenominatorShiftDown(false)` |
| — | num–bar min gap | `FractionNumeratorGapMin` / `FractionNumDisplayStyleGapMin` | 68 / 150 | `fractionNumeratorGapMin(display)` |
| — | den–bar min gap | `FractionDenominatorGapMin` / `FractionDenomDisplayStyleGapMin` | 68 / 150 | `fractionDenominatorGapMin(display)` |
| σ13 | sup shift up, display | `SuperscriptShiftUp` | 360 | `superscriptShiftUp()` |
| σ14 | sup shift up, non-display uncramped | (OpenType folds σ13/σ14 into one) | 360 | `superscriptShiftUp()` |
| σ15 | sup shift up, cramped | `SuperscriptShiftUpCramped` | 252 | `superscriptShiftUpCramped()` |
| σ16 | sub shift down (no sup) | `SubscriptShiftDown` | 210 | `subscriptShiftDown()` |
| σ17 | sub shift down (with sup) | (OpenType uses σ16 + interlock constants) | 210 | `subscriptShiftDown()` |
| σ18 | sup baseline drop for boxed base | `SuperscriptBaselineDropMax` | 230 | `superscriptBaselineDropMax()` |
| σ19 | sub baseline drop for boxed base | `SubscriptBaselineDropMin` | 160 | `subscriptBaselineDropMin()` |
| ¼σ5 rule | sup bottom floor | `SuperscriptBottomMin` | 120 | `superscriptBottomMin()` |
| ⅘σ5 rule | sub top ceiling | `SubscriptTopMax` | 368 | `subscriptTopMax()` |
| 4ξ8 rule | sup/sub min ink gap | `SubSuperscriptGapMin` | 150 | `subSuperscriptGapMin()` |
| ⅘σ5 rule | sup bottom max when sub present | `SuperscriptBottomMaxWithSubscript` | 380 | `superscriptBottomMaxWithSubscript()` |
| script kern | kern after script | `SpaceAfterScript` | 40 | `spaceAfterScript()` |
| σ22 | axis height | `AxisHeight` | 258 | `axisHeight()` |
| ξ8 | default rule thickness | `FractionRuleThickness` / `RadicalRuleThickness` / `OverbarRuleThickness` / `UnderbarRuleThickness` | 68 | `fractionRuleThickness()` etc. (all `scalePxMin(·,1)`) |
| ξ9–ξ13 | big-op limit spacing | `UpperLimitGapMin`(135), `UpperLimitBaselineRiseMin`(300), `LowerLimitGapMin`(135), `LowerLimitBaselineDropMin`(670) | — | `upperLimitGapMin()`, `upperLimitBaselineRiseMin()`, `lowerLimitGapMin()`, `lowerLimitBaselineDropMin()` |
| rule 11 ψ | radical clearance | `RadicalVerticalGap`(85) / `RadicalDisplayStyleVerticalGap`(170), `RadicalExtraAscender`(78) | — | `radicalVerticalGap(display)`, `radicalExtraAscender()` |
| — | radical degree | `RadicalKernBeforeDegree`(65), `RadicalKernAfterDegree`(−335), `RadicalDegreeBottomRaisePercent`(55) | — | `radicalKernBeforeDegree()`, `radicalKernAfterDegree()`, `radicalDegreeBottomRaisePercent()` (raw %) |
| rule 13 | display op size | `DisplayOperatorMinHeight` | 1800 | `displayOperatorMinHeight()` |
| rule 19 aid | delimited min height | `DelimitedSubFormulaMinHeight` | 1325 | `delimitedSubFormulaMinHeight()` |
| — | script scale-downs | `ScriptPercentScaleDown`(70), `ScriptScriptPercentScaleDown`(55) | — | raw % accessors (PD-10: physical 66.7/44.4) |
| \delimiterfactor | 901 | (TeX register, no OpenType constant) | — | TXP-8 constant `kDelimiterFactorPermille = 901` [P] |
| \delimitershortfall | 0.5 em | (TeX register) | — | TXP-8 `kDelimiterShortfallPx = round(em/2)` [P] (OQ-2) |
| italic corr. δ | per glyph | `MathItalicsCorrectionInfo` | table | `lookupItalicsCorrection(cp)` (`stix_math_italics.h`) |
| — | script cut-in kerns | `MathKernInfo` | table [P TXP-1] | `lookupMathKern(cp, corner, heightDu)` [P] |
| — | assembly overlap floor | `MathVariants.minConnectorOverlap` | value [P TXP-1] | `kStixMinConnectorOverlapDu` [P] |

### H.2 Style rules (Appendix G rules 13–17 essentials, plus cramping)

Style of sub-parts, given current style S (D=display, T=text, S′=script, SS=scriptscript; primes
denote cramped):

- Superscript of S → script-of-S: D,T → S′; S′ → SS; SS → SS. Cramped iff S is cramped.
- Subscript of S → script-of-S, **always cramped**.
- Fraction numerator → next-smaller style uncramped; denominator → next-smaller style **cramped**.
  (NumOS size policy PD-4 overrides the *size* part at level 0 but the **cramping bit still
  propagates** per PD-6/OQ-6.)
- Radicand → current style, **cramped** (rule 11).
- Big-op display limits: upper → script style uncramped; lower → script style **cramped**.
- NumOS mapping [V]: size stepping is `FontMetrics::superscript()` (`MathAST.h:249-290`) and
  `fractionPartMetrics()` (`:314-326`); cramping is the TXP-4 `fm.cramped` bit.

### H.3 Script sizes

OpenType: script = 70 %, scriptscript = 55 % of base. NumOS physical: 12 px (66.7 %) and 8 px
(44.4 %) — PD-10. All *shift/gap constants* still scale by the current `fm.emSize`, so a script
context computes its own mu and DU→px at 12 or 8 px em [V `MathAST.cpp:200-201` and every
`MathConstantsProvider mc(fm.emSize)` construction].

### H.4 OpenType MATH sub-tables used by TXP

- `MathConstants` — all 56, extracted [V].
- `MathItalicsCorrectionInfo` — extracted, 413 entries [V `stix_math_italics.h`].
- `MathKernInfo` [P TXP-1] — per glyph, per corner (TopRight/TopLeft/BottomRight/BottomLeft), a
  staircase: correction heights h₁<h₂<…<hₙ (DU, relative to baseline) and kern values k₀…kₙ; the
  kern for attachment height h is kᵢ where i = count of hⱼ ≤ h. Applied per §H.6 step 6.
- `MathTopAccentAttachment` [P TXP-1, data only — GAP-49].
- `MathVariants` — vertical variants + assemblies extracted for 15 cps [V
  `stix_math_variants.h`]; TXP-1 adds `minConnectorOverlap` and PUA-encoded variant glyphs.
  Assembly growth algorithm (already implemented [V `MathGlyphAssembly.cpp:88-280`]): pick
  smallest variant ≥ target; else assemble with minimal extender count such that
  `maxHeight(overlap=min) ≥ target`, then choose per-seam overlaps o_i with
  `minConnectorOverlap ≤ o_i ≤ min(endConn_i, startConn_{i+1})` to hit the target as closely as
  possible (TXP-8 adds the left inequality).

### H.5 The inter-atom spacing table (TeXbook p. 170) and demotion rules [U]

Entries: 0 = none, 1 = thin (3 mu), 2 = medium (4 mu), 3 = thick (5 mu). **Parenthesized ⇒ the
space is inserted only in DISPLAY and TEXT styles, and suppressed in SCRIPT/SCRIPTSCRIPT.**
`*` = the pair cannot occur after demotion (implementation returns 0).

| left\right | Ord | Op | Bin | Rel | Open | Close | Punct | Inner |
|---|---|---|---|---|---|---|---|---|
| **Ord** | 0 | 1 | (2) | (3) | 0 | 0 | 0 | (1) |
| **Op** | 1 | 1 | * | (3) | 0 | 0 | 0 | (1) |
| **Bin** | (2) | (2) | * | * | (2) | * | * | (2) |
| **Rel** | (3) | (3) | * | 0 | (3) | 0 | 0 | (3) |
| **Open** | 0 | 0 | * | 0 | 0 | 0 | 0 | 0 |
| **Close** | 0 | 1 | (2) | (3) | 0 | 0 | 0 | (1) |
| **Punct** | (1) | (1) | * | (1) | (1) | (1) | (1) | (1) |
| **Inner** | (1) | 1 | (2) | (3) | (1) | 0 | (1) | (1) |

Encoding in NumOS after TXP-2: parenthesized entries stored as **negative** codes (the suppression
machinery already exists and is correct — `spacingCodeToMu`, `MathTypography.h:153-159`;
`interAtomSpacingPx`, `:166-176` [V]); `*` stays 9.

Diff vs today's table (`MathTypography.h:96-106` [V]): 20 entries flip positive→negative
(Ord row: Bin, Rel, Inner; Op row: Rel, Inner; Bin row: Ord, Op, Open, Inner; Rel row: Ord, Op,
Open, Inner; Close row: Bin, Rel, Inner; Punct row: all seven non-`*`... counted precisely in
TXP-2 §5), and one code change: `Bin→Op` 9 → −2 (OQ-4).

**Demotion (effective-class) rules** — applied left-to-right over a row's children before any
table lookup:

- D1: a Bin atom whose **left context** is: start-of-row, or an effective Bin, Op, Rel, Open, or
  Punct — becomes Ord.
- D2: a Bin atom whose **right neighbor** is Rel, Close, or Punct — becomes Ord.
- D3: `NodeEmpty` children are transparent: they take class Ord but do not count as "left
  context" for D1 (an Empty between Open and Bin still leaves the Bin demotable). Rationale:
  Empty is an editor placeholder, not an atom.
- The row's own propagated class (`NodeRow::mathClass`, `MathAST.cpp:162-169` [V]) uses the
  **effective** class of its first non-Empty child after TXP-2.

### H.6 Script placement (Appendix G rules 18a–18f, OpenType form)

Inputs: base box B (ink h_B = inkAscent, d_B = inkDescent; flag `isGlyphBase` true iff the base
is a single glyph — `NodeNumber` of length 1, `NodeVariable`, `NodeConstant`, single-glyph
`NodeIdentifier` when it exists), superscript box X (script style, cramped iff base context
cramped), subscript box Y (script style, always cramped). All constants at the **base** style's
em. δ = italic correction of the base's last glyph (0 if none or not a glyph run).

1. **(18a) Baseline drops for boxed bases.** If `isGlyphBase`: `u₀ = 0`, `v₀ = 0`. Else:
   `u₀ = h_B − superscriptBaselineDropMax()`, `v₀ = d_B + subscriptBaselineDropMin()`.
2. **(18b) Subscript alone.** `v = max(v₀, subscriptShiftDown(), inkAscent(Y) − subscriptTopMax())`.
   Sub x-position: `x_sub = x_base + width(B)` (no δ — the sub tucks under the overhang).
3. **(18c) Superscript alone.**
   `p = cramped ? superscriptShiftUpCramped() : superscriptShiftUp()`;
   `u = max(u₀, p, inkDescent(X) + superscriptBottomMin())`.
   Sup x-position: `x_sup = x_base + width(B) + δpx` where `δpx = scalePx(δ)`.
4. **(18d) Both.** Compute u per step 3 and `v = max(v₀, subscriptShiftDown())` (no TopMax term
   in the both-case).
5. **(18e) Interlock.** Ink gap `g = (u − inkDescent(X)) − (inkAscent(Y) − v)`. If
   `g < subSuperscriptGapMin()`: first raise u until the **sup bottom** `u − inkDescent(X)`
   reaches `superscriptBottomMaxWithSubscript()` (i.e. `u = min needed, capped:
   u ≤ superscriptBottomMaxWithSubscript() + inkDescent(X)`), then push v down by whatever
   deficit remains: `v += subSuperscriptGapMin() − g′` where g′ is the gap after the u raise.
   (This is TeX's ψ redistribution stated in OpenType terms.)
6. **(18f + PD-7) Horizontal.** After TXP-9: `x_sup += cutInKern(base.lastGlyph, TopRight,
   attachHeightDu(u)) + cutInKern(sup.firstGlyph, BottomLeft, …)` and symmetrical for the sub
   with BottomRight/TopLeft corners; kern lookups per §H.4. Width of the script group =
   `max(δpx·0 + width(Y), δpx + width(X)) + spaceAfterScript()` — i.e. sub measured from
   `x_base + width(B)`, sup from `x_base + width(B) + δpx`, group width = rightmost edge +
   `spaceAfterScript()`.
7. **Degenerate inputs** (total function): empty X/Y rows have `ink = 0/0` and full line-box
   layout bounds [V `MathAST.cpp:173-179`] — formulas above then reduce to the shift constants;
   zero-height boxes are legal; if both X and Y are absent the node is malformed (constructors
   prevent it [V `MathAST.cpp:446-451`]).

### H.7 Fraction and stack placement (Appendix G rule 15, OpenType form)

Inputs: numerator N, denominator D (styles per PD-4 size policy + §H.2 cramping), current style's
`display = fm.isDisplayStyle()`. θ = `fractionRuleThickness()` px, a = `axisHeight()` px.

1. `u = fractionNumeratorShiftUp(display)` — numerator baseline above the **main baseline**.
   `v = fractionDenominatorShiftDown(display)` — denominator baseline below.
2. Bar geometry: bar centered on axis; `barTop = a + ⌈θ/2⌉`, `barBottom = a − ⌊θ/2⌋` (distances
   above baseline; matches `_barHalfUpPx/_barHalfDownPx` [V `MathAST.cpp:408-409`]).
3. Gap enforcement (rule 15d): with γ_num = `fractionNumeratorGapMin(display)`,
   γ_den = `fractionDenominatorGapMin(display)`:
   `u = max(u, barTop + γ_num + inkDescent(N))`;
   `v = max(v, γ_den + inkAscent(D) − barBottom)`; (all in px, single conversion point per
   constant — PD-1 note).
4. Width: `w = max(width(N), width(D))`; bar spans exactly w (GAP-28: **no overhang**); children
   centered: `x_N = x + (w − width(N))/2`, likewise D.
5. Box: `ascent = u + ascent(N)`, `descent = v + descent(D)`; **ink**: `inkAscent = u +
   inkAscent(N)`, `inkDescent = v + inkDescent(D)` (GAP-29).
6. Degenerate: empty N or D behaves per §H.6 step 7; θ never 0 (PD-1).
7. **Stack (NodeBinom, MR-14)**: same with θ = 0, no bar, shifts = `stackTopShiftUp(display)` /
   `stackBottomShiftDown(display)`, single gap constraint `stackGapMin(display)` between N ink
   bottom and D ink top, resolved by moving both apart equally.

### H.8 Radical placement (Appendix G rule 11, OpenType form)

Inputs: radicand R (current style, **cramped**), display flag. θ = `radicalRuleThickness()`,
ψ = `radicalVerticalGap(display)`, ε = `radicalExtraAscender()`.

1. Overline: sits ψ above `inkAscent(R)`; thickness θ; ascender ε above the overline.
   `ascent = inkAscent(R) + ψ + θ + ε` (matches today's structure [V `MathAST.cpp:600-604`]
   except ink vs layout ascent — TXP-6 switches to ink).
2. Radical glyph: target height `H = inkAscent(R) + ψ + θ + inkDescent(R)`; select via the U+221A
   variant ladder (TXP-1 data; OQ-5), assembled per §H.4; glyph bottom aligns with content ink
   bottom, top reaches the overline. Vector fallback keeps current geometry
   [V `MathRenderer.cpp:1870-1911`].
3. Degree: with raise% ρ = `radicalDegreeBottomRaisePercent()` (55):
   `degreeBaseline = radicalBottom − ρ/100 × (radicalBottom − radicalTop)` where radicalTop/Bottom
   are the **radical glyph's** vertical extent (overline top to glyph bottom), measured in the
   node's coordinate space; x advance: `kernBefore(65 DU) + width(degree) + kernAfter(−335 DU)`,
   with the net kern clamped so the degree never starts left of x (total-function guard:
   `x_degree = x + max(0, kernBeforePx)`; the negative after-kern tucks the radical glyph back
   over the degree, floor at hook start).
4. Degenerate: degree absent → skip step 3 [V structure `MathAST.cpp:582-595`]; empty radicand →
   ink 0/0 per §H.6 step 7.

### H.9 Big operators and limits (Appendix G rule 13/13a, OpenType form)

Inputs: operator glyph selected for target `max(fm.height(), displayOperatorMinHeight())` in
display style [V `MathAST.h:358-363`]; limits U (upper; script, uncramped), L (lower; script,
cramped); body handled as a sibling with an internalized thin space (PD-11).

1. **Nucleus on axis (rule 13):** the glyph is shifted so its ink center sits on the axis:
   `glyphBaselineShift = (inkAscent_g − inkDescent_g)/2 − axisHeight()` (positive = shift down).
   TXP-7 obtains `inkAscent_g/inkDescent_g` from the LVGL glyph dsc at draw time and from
   `duToPxCeil(height)/2` symmetric approximation in layout (stated approximation, ±1 px).
2. **Display limits (rule 13a):** upper baseline above glyph top by
   `max(upperLimitGapMin() + inkDescent(U), upperLimitBaselineRiseMin())`; lower baseline below
   glyph bottom by `max(lowerLimitGapMin() + inkAscent(L), lowerLimitBaselineDropMin())`. No
   other padding (PD-11 deletes symbolPad). Limits centered on the glyph column; column width =
   `max(w_glyph, w_U, w_L)`.
3. **Inline (nolimits) scripts:** place U/L as superscript/subscript of the glyph per §H.6 with
   the glyph's italic correction δ (∫: 230 DU [V `stix_math_italics.h` U+222B]) splitting sup
   (+δ) from sub (−0) — i.e. sup x = glyph right + δpx, sub x = glyph right.
4. **Gate:** display limits iff `style == DISPLAY && selectedGlyphHeight ≥
   displayOperatorMinHeight()` — one rule for Summation, DefIntegral, **and** BigOp (GAP-37);
   reachable because TXP-1 provides real size variants.
5. Degenerate: empty U/L rows collapse their gap contribution to the BaselineRiseMin/DropMin
   terms only when the row is `NodeEmpty`-only (ink 0) — the box still reserves the line box per
   §H.6 step 7.

### H.10 Delimiter sizing (Appendix G rule 19; PD-5 values)

Inputs: content C (ink bounds), a = `axisHeight()`.

1. `halfSize = max(inkAscent(C) − a, inkDescent(C) + a)`; `size = 2·halfSize`.
2. `target = max( ⌈901·size/1000⌉ , size − kDelimiterShortfallPx )` with
   `kDelimiterShortfallPx = round(fm.emSize/2)` (OQ-2).
3. Floors: `target = max(target, duToPxCeil(baseGlyphHeightDu))`; if C is an Empty-only slot,
   `target = max(target, fm.lineHeight())`.
4. Select glyph via the variant/assembly ladder for `target` (§H.4); final delimiter is centered
   on the axis: `delimAscent = a + h_sel/2`, `delimDescent = h_sel/2 − a` (h_sel = selected
   height; matches the symmetric structure of `symmetricAxisDelimiterGeometry`
   [V `MathAST.cpp:97-115`], with the cover+pad target replaced).
5. `DelimitedSubFormulaMinHeight` (1325 DU) is used as TeX uses `\delimitershortfall`-adjacent
   floors in OpenType engines: `target = max(target, delimitedSubFormulaMinHeight())` **only in
   display style** [U — this matches HarfBuzz/MathJax practice]. TXP-8 acceptance verifies this
   floor against the reference by corpus items C-49/C-50.
6. No inner pad (GAP-43): content x = delimiter x + delimiter advance width.

### H.11 Worked example (verification anchor): corpus item C-31, `\dfrac{1}{2}` @ em 18, display

Every implementer and reviewer can recompute this by hand; the TXP-5 host suite asserts these
exact numbers.

- Constants (DU → px via `scalePx`, PD-1): axis a = 258 → **5**; θ = 68 → `scalePxMin` **1**;
  σ8 = 640 → **12**; σ11 = 640 → **12**; γ_num = γ_den = 150 → **3**.
- Children: "1" and "2" digit ink at em 18 from the font probe — `numberAscent ≈ 13,
  numberDescent = 0` [V mechanism `MathRenderer.cpp:492-515`; exact values are font-build facts
  asserted by the harness, not by this spec].
- §H.7: barTop = 5 + ⌈1/2⌉ = **6**; barBottom = 5 − 0 = **5**.
  u = max(12, 6 + 3 + 0) = **12** → numerator baseline 12 px above main baseline.
  v = max(12, 3 + 13 − 5) = **12** → denominator baseline 12 px below.
  ascent = 12 + ascent(N); width = max(w₁, w₂) exactly (no overhang).
- Feature JSON therefore: `frac.bar.y = −5`, `frac.num.baseline = −12`,
  `frac.den.baseline = +12`, and the num gap (num-ink-bottom to bar-top,
  `(u − inkDescent(N)) − barTop`) = 12 − 0 − 6 = **6 px** — the σ-shift dominates the 3 px
  minimum for one-line children at em 18. **This is the observable difference from today's
  code**, which pins the gap to exactly 3 px symmetric (`fractionBarGaps` [V]): the TXP-0
  baseline records `frac.num.gap = 3`, the reference records 6, and TXP-5 closes it. A reviewer
  seeing any other pair of numbers knows the oracle, the spec, or the code is wrong — that is
  the point of this section.

---

## TXP-0 — Corpus, reference oracle, feature-flag scaffold, baseline freeze

### 0.1 Objective
Create the 64-item expression corpus, the MathJax-based reference-metric oracle, the wave-flag
scaffold, and freeze the "before" state (goldens + geometry snapshots + oracle metrics) — with
zero geometry change (all existing goldens byte-identical).

### 0.2 Preconditions
- CI green on `main` (emulator workflow).
- MR-02 host geometry harness: **not required**; if absent (it is absent at audit base), this
  phase bootstraps the subset defined in `NUMOS_MATHRENDERER_TEST_ORACLE_AND_GOLDEN_PLAN.md` §3:
  a g++ host binary linking `src/math/MathAST.cpp`, `src/math/MathTypography.h`,
  `src/math/font/MathGlyphAssembly.cpp`, the three `src/fonts/stix_math_*.c` and a minimal LVGL
  font shim, dumping per-case `LayoutResult` JSON. If MR-02 lands first, extend it instead —
  check `tests/host/mathlayout_geometry_test.cpp` existence before writing.

### 0.3 Scope
Files (all [NEW] unless noted):
- `tools/tex-parity-oracle/package.json`, `package-lock.json` — Node ≥18, deps:
  `mathjax@3.2.2` (exact pin), no other runtime deps. Never referenced by `platformio.ini`.
- `tools/tex-parity-oracle/render-reference.mjs` — CLI (see 0.5).
- `tools/tex-parity-oracle/corpus.json` — machine-readable Appendix B (ID, latex, style,
  metricsRequested).
- `scripts/compare-tex-parity.py` — comparator (see 0.5.3), stdlib-only Python like
  `compare-ppm.py` [V dependency-free precedent].
- `tests/host/txp_corpus_cases.h` + additions to the host harness — VPAM builders for all corpus
  items (reusing `MathRenderVisualCases.cpp`/`MathStressExpressions.cpp` builders where Appendix B
  says so).
- `tests/tex_parity/reference/*.json` — committed reference metrics (one per corpus item).
- `tests/tex_parity/baseline/*.json` — committed **NumOS "before" metrics** (the frozen gap
  record; TXP-10 diffs against these).
- `src/math/MathRenderVisualCases.cpp/.h` — **additive only**: append the corpus builders that
  do not already exist as fixtures, so every corpus item is reachable through the MathShowcase
  page cycle (`open_app MathShowcase` + `key right` ×k — the existing capture vehicle
  [V pattern `tests/emulator/scripts/math_showcase_delims_smoke.numos`]). Appending cases does
  not move case index 0, so `math_showcase_smoke` stays byte-identical (verified by the
  candidate run). Record the resulting item→index map in `tools/tex-parity-oracle/corpus.json`
  (`"showcaseIndex"` field).
- `tests/emulator/scripts/txp0_corpus_pages.numos` [NEW] — smoke: cycles all showcase pages
  (exact listing in Appendix C.1), asserting `assert_app MathShowcase` + `assert_no_error` at
  the end — proves every corpus fixture lays out and draws without crashing in the emulator.
- `docs/specs/NUMOS_TEX_PARITY_TYPESETTING_SPEC.md` (this file) — status column updates only.
NON-scope: any change to layout or draw **formulas** (the fixture append is data, not geometry);
any golden re-bless; any font regeneration.

### 0.4 Design

```cpp
// tests/host/txp_corpus_cases.h  [NEW] — host-only, plain C++17, no LVGL
namespace txp {
struct CorpusCase {
    const char*        id;        // "C-01" … "C-64" (Appendix B)
    const char*        latex;     // reference LaTeX string, byte-equal to corpus.json
    vpam::MathStyle    style;     // layout style for the NumOS render
    vpam::NodePtr    (*build)();  // VPAM constructor (factories from MathAST.h)
};
const CorpusCase* corpusCases();      // static table, PSRAM-irrelevant (host only)
std::size_t       corpusCaseCount();  // == 64
} // namespace txp
```

Metric record (both reference and NumOS sides emit the same JSON shape):

```json
{ "id": "C-27", "emPx": 18,
  "metrics": {
    "width": 57, "ascent": 21, "descent": 14,
    "features": {
      "frac.bar.y": -5, "frac.num.baseline": -14, "frac.den.baseline": 10,
      "frac.num.gap": 3, "frac.den.gap": 3
    } } }
```

Feature keys are per-construct, defined exhaustively in 0.5.4. All values are px relative to the
**main baseline** (y grows downward; above-baseline = negative), rounded per PD-2.

### 0.5 Algorithm

**0.5.1 Reference generation** (`node render-reference.mjs --corpus corpus.json --out
tests/tex_parity/reference/`):
1. For each item: `tex2svg(latex, {display: style!=TEXT, em: 18, ex: derived})` via the MathJax
   v3 direct API with the STIX-Two font data if available in the pinned MathJax build, else the
   default TeX font — record which in the JSON header (`"fontModel"`); positional metrics are
   MATH-constant-driven either way, and the comparator's tolerance table (0.5.4) marks the three
   metrics that are font-metric-sensitive.
2. Walk the SVG node tree accumulating `transform` translations (MathJax emits nested
   `<g transform="translate(x,y)">` in milli-em units [U]); convert em → px at 18 px/em; extract
   the feature set for the constructs the corpus item declares (corpus.json lists expected
   feature keys per item, so a missing feature is a hard error, not a silent skip).
3. Write one JSON per item + a `manifest.json` with the MathJax version and a SHA-256 of every
   output (REN-10 pinning).

**0.5.2 NumOS metric extraction** (host harness, new mode `--txp-metrics`):
for each corpus case: build the VPAM tree, `root->calculateLayout(defaultFontMetrics()-derived
FontMetrics from the real stix fonts)` — use the same font-shim metrics the harness already
loads; then walk the tree emitting the same feature keys from node geometry (e.g.
`frac.bar.y = −axisHeight`, `frac.num.gap = (barTop) − (numBaseline + inkDescent(num))`), using
ONLY public node accessors (`NodeFraction::numeratorShiftPx()` etc. [V `MathAST.h:785-790`]) —
add missing const accessors where a feature is not reachable (additive header edits allowed,
zero behavior change).

**0.5.3 Comparison** (`python scripts/compare-tex-parity.py reference/ candidate/ --report
out/txp-report.json`): per item, per feature: `delta = numos − reference`; verdict per feature =
PASS if `|delta| ≤ tol(featureClass)`; item verdict = all features pass OR every failing feature
is covered by a PD listed in a committed waiver file `tests/tex_parity/waivers.json`
(`{"feature": "...", "pd": "PD-10", "expectedDelta": [lo, hi]}` — a waived feature FAILS if its
delta leaves the declared band). **Never compares pixels.** Exit 0/1/2 like `compare-ppm.py`
[V `:33-37` convention].

**0.5.4 Feature keys and tolerance classes** (exhaustive; a phase adding a construct adds keys
here first):

| Class | Keys (pattern) | Tolerance |
|---|---|---|
| bar/rule positions | `frac.bar.y`, `radical.rule.y`, `overline.rule.y` | ±1 px |
| baseline shifts | `sup.baseline`, `sub.baseline`, `frac.num.baseline`, `frac.den.baseline`, `limit.upper.baseline`, `limit.lower.baseline`, `degree.baseline` | ±1 px |
| ink gaps | `frac.num.gap`, `frac.den.gap`, `radical.gap`, `limit.upper.gap`, `limit.lower.gap`, `scripts.gap` | ±1 px |
| horizontal | `spacing.<i>` (i-th inter-atom gap in the top row), `sup.dx` (sup x minus base right edge), `sub.dx`, `delim.inner.dx` | ±1 px |
| extents (font-sensitive) | `width`, `ascent`, `descent`, `delim.height`, `op.height` | ±2 px, and PD-10 waivers apply to script-content extents |

**0.5.5 Baseline freeze:** run 0.5.2 at audit base and commit the outputs to
`tests/tex_parity/baseline/`. Regenerate candidates; require zero diffs; do NOT touch goldens.

**0.5.6 Flag scaffold:** add to `platformio.ini` (comments only — the flags are consumed via
`-D` when a human A/Bs): a commented block documenting `NUMOS_TXP2_LEGACY` … `NUMOS_TXP9_LEGACY`
per PD-12, next to the existing validate-env flag comments [V `platformio.ini:116-137` pattern].

### 0.6 Test plan
- Host: corpus builders compile and lay out without assert/crash for all 64 items (smoke loop in
  the harness); metric extractor emits every declared feature key (hard error otherwise).
- Negative controls (T6 pattern [V test-oracle plan §7]): (a) corrupt one reference JSON value by
  3 px → comparator exits 1; (b) delete a feature from a candidate → exit 2; (c) tamper a waiver
  band → exit 1.
- Emulator: `txp0_corpus_pages.numos` green; full existing suite green;
  candidates byte-identical (all 19).
- Oracle determinism: run 0.5.1 twice; manifest hashes equal.
- CI wiring: append one step after the existing golden compare: build host harness (if
  bootstrapped here) + run `compare-tex-parity.py baseline-mode` (NumOS metrics vs committed
  baseline — catches accidental geometry drift in later non-TXP PRs; reference-mode comparison
  stays warning-only until TXP-10 flips it gating).

### 0.7 Acceptance criteria (mechanical)
1. `pio run -e emulator_pc` green; all 19 goldens byte-identical after mask (candidate run).
2. 64/64 corpus items produce NumOS metrics and reference metrics; comparator runs end-to-end
   and its report enumerates current failures — **expected fail count is nonzero and recorded**
   in `tests/tex_parity/baseline/EXPECTED_FAILURES.md` (this is the machine-readable gap matrix).
3. All three negative controls behave as specified.
4. `esp32s3_n16r8` compiles (no firmware source touched; proves no accidental include leak).
5. No file under `tools/` or `tests/tex_parity/` is reachable from any `build_src_filter`.

### 0.8 Rollback
Delete `tools/tex-parity-oracle/`, `tests/tex_parity/`, the host-harness additions and the CI
step. No flag needed (no geometry).

### 0.9 Agent prompt block

```text
You are implementing phase TXP-0 of the NumOS TeX-parity program.
Branch: create `claude/txp0-corpus-oracle-<suffix>` from latest main.
Read first, in order:
  1. docs/specs/NUMOS_FABLE_CONTEXT_HEADER.md (all)
  2. docs/specs/NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H, §TXP-0, Appendix B
  3. docs/specs/NUMOS_MATHRENDERER_TEST_ORACLE_AND_GOLDEN_PLAN.md §3 (host harness shape)
Non-negotiables: no change to src/ layout or draw formulas; all 19 goldens byte-identical;
tools/ and tests/tex_parity/ must never enter any PlatformIO build; pin mathjax exactly;
PLATFORMIO_BUILD_DIR=.pio/build on Linux; comparator is stdlib-only Python.
Deliverables: exactly the file list in §TXP-0 0.3, the CI step in 0.6, and acceptance 0.7 proven
in the PR body (paste the candidate-compare output and the negative-control outputs).
If you hit an open question, take the §D recommended default and say so in the PR.
PR title: "test(txp): TXP-0 corpus + reference oracle + baseline freeze".
PR body must include: corpus item count, reference manifest SHA-256, expected-failure count from
EXPECTED_FAILURES.md, checklist: [goldens byte-identical] [negative controls pass] [no
build_src_filter reachability] [firmware compiles].
Do not create the PR until `python scripts/generate-emulator-candidates.py` reports zero diffs.
```

---

## TXP-1 — Font-data pipeline extension (kerns, overlap, PUA variants, assembly glyphs)

### 1.1 Objective
Extend `scripts/extract_stix_math.py` and `scripts/generate_stix_math_font.sh` so the repo's
font data contains everything later phases consume — `MinConnectorOverlap`, `MathKernInfo`,
`MathTopAccentAttachment`, PUA-encoded vertical size variants (Σ ∫ √ and the delimiter set), and
the extensible-assembly piece glyphs — with **zero rendering change** (all goldens
byte-identical; new data is dead until TXP-6/7/8/9 and MR-11 consume it).

### 1.2 Preconditions
- TXP-0 landed (baseline metrics committed) — ordering only, no artifact dependency.
- This phase **subsumes ticket MR-04** (`NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md`): execute
  MR-04's range extension (`0x239B-0x23B3, 0x2320-0x2321`) here so fonts regenerate once. Note
  the subsumption in the PR body; MR-11 (go-live) remains a separate later wave.
- Toolchain: `lv_font_conv` (npm) + Python `fonttools`; record exact versions in the generated
  headers (existing pattern [V `stix_math_constants.h:2-4`]).

### 1.3 Scope
- `scripts/extract_stix_math.py` — four new extraction passes + PUA remapping (design 1.4).
- `scripts/generate_stix_math_font.sh` — range additions; PUA range injection from the
  extractor's emitted mapping file.
- Regenerated: `src/fonts/stix_math_{18,12,8}.c`, `src/math/font/stix_math_variants.h`.
- [NEW generated] `src/math/font/stix_math_kerns.h` (MathKernInfo),
  `src/math/font/stix_math_accents.h` (top-accent attachment, data-only for GAP-49),
  `assets/fonts/stix_pua_map.json` (glyphName → PUA cp, the remap contract).
- [NEW] `src/math/font/stix_math_variants.h` gains
  `inline constexpr int16_t kStixMinConnectorOverlapDu = <extracted>;`
- `src/ui/StixGlyphGallery.cpp` — extend the diagnostic probe list with the new cps (additive).
NON-scope: any consumer change — `MathGlyphAssembly.cpp`, `MathAST.cpp`, `MathRenderer.cpp` are
untouched except zero-impact header includes if needed; `g_delimiterAssemblyRenderable` probing
(`MathRenderer.cpp:431-435` [V]) flips true automatically once U+239C exists — **verify that this
alone does not change goldens**: it gates only `expandDelimiterGeometryToAssembly`
(`MathAST.cpp:149-151` [V]) which grows delimiters to the assembly floor. If candidates diff,
gate the flip behind `NUMOS_TXP_ASSEMBLY_PROBE` (default off) and hand MR-11 the key — this is
the one anticipated trap of the phase; measure, don't assume.

### 1.4 Design

PUA remapping (the load-bearing idea — unencoded variant glyphs get stable codepoints):
- PUA block: U+F0000 (Supplementary PUA-A) onward, allocated deterministically: sort base cps
  ascending; per base cp, variants in ladder order; assign sequential cps. Deterministic ⇒
  regeneration is reproducible; the JSON map is committed and diffs reviewably.
- `lv_font_conv` receives the PUA cps in `--range` (it subsets by cmap; the extractor first
  writes a **remapped font file** `assets/fonts/STIXTwoMath-NumOS.ttf` with fonttools, adding
  cmap entries for the variant/assembly glyph names → PUA cps; the original TTF is never
  modified).
- `stix_math_variants.h` regenerates with `SizeVariantRecord.glyphCp` = PUA cps (base-size
  variant keeps the real cp), so `DelimiterAssembler` picks tall variants with **zero code
  change** — but nothing selects them until target heights demand them, and today's target
  heights already demand them for Σ/∫ (32 px), which would change layout ⇒ therefore the
  regenerated variant tables for **operator cps (Σ ∫ big-ops)** are emitted into a parallel
  header `stix_math_variants_ext.h` consumed only from TXP-7 on, while delimiter/paren tables
  regenerate in place with unchanged content for existing sizes (they already list assemblies
  [V]). This keeps the phase geometry-frozen. (Radical U+221A table also goes to `_ext`.)

```cpp
// src/math/font/stix_math_kerns.h [NEW, generated] — shape:
namespace vpam {
enum class MathKernCorner : uint8_t { TopRight, TopLeft, BottomRight, BottomLeft };
struct MathKernEntry {           // one staircase
    uint32_t codepoint; uint8_t corner;
    uint8_t  count;              // n steps
    uint16_t firstIndex;         // into kKernHeights/kKernValues flat arrays (DU)
};
extern const MathKernEntry kMathKernEntries[]; extern const uint16_t kMathKernEntryCount;
extern const int16_t kKernHeightsDu[]; extern const int16_t kKernValuesDu[];
/// Kern value (DU) for cp/corner at attachment height (DU above baseline); 0 if absent.
int16_t lookupMathKern(uint32_t cp, MathKernCorner corner, int16_t heightDu);
}
```
Flat-array layout keeps it flash-resident constexpr-friendly; `lookupMathKern` is a binary search
over entries + linear staircase walk (n ≤ ~10 [U]).

### 1.5 Algorithm (extractor passes)
1. `MinConnectorOverlap`: read `MATH.MathVariants.MinConnectorOverlap` → emit constant.
2. `MathKernInfo`: for every glyph with a `MathKernInfoRecord`, resolve name→cp via the existing
   reverse cmap (`extract_stix_math.py:296-316` [V]); **include PUA-remapped glyphs** (the remap
   table extends the reverse cmap); emit staircases for the 4 corners. Subset filter: only cps
   in the generated font ranges (no dead data).
3. `MathTopAccentAttachment`: same pattern → `stix_math_accents.h`.
4. Variant/assembly re-extraction with the PUA-extended cmap: the existing pass
   (`:383-448` [V]) now resolves previously-dropped variant glyph names.
5. Regenerate fonts with extended ranges; verify with a bitmap-level check that every cp present
   in the OLD fonts has identical bitmap+metrics in the NEW fonts (script step comparing the
   generated `.c` glyph tables for the old cp set — REN-7's hard gate).

### 1.6 Test plan
- Extractor unit tests (pure Python, `tests/host/test_extract_stix_math.py` [NEW], run in CI
  before the emulator build): staircase lookup correctness on 3 hand-checked glyphs; PUA map
  determinism (two runs byte-equal); old-cp bitmap invariance checker green.
- Host harness: `lookupMathKern` C++ smoke (monotone staircase, absent cp → 0).
- Emulator: full candidate run byte-identical (hard gate); StixGlyphGallery diagnostic build
  compiles (`NUMOS_STIX_DIAGNOSTICS` path).
- Firmware: `pio run -e esp32s3_n16r8` green; PR records flash delta per REN-8 budgets
  (fonts ≤ +120 KB across the trio, data headers ≤ +24 KB; **abort and descope the 8 px size**
  if exceeded).

### 1.7 Acceptance criteria
1. All 19 goldens byte-identical (candidate run attached to PR).
2. `kStixMinConnectorOverlapDu` present with the font's true value; kern header contains ≥1
   staircase for each of: an italic letter, ∫, and a big operator [sanity that extraction found
   real data].
3. `stix_math_variants_ext.h` lists ≥2 size variants for U+2211 and U+222B and ≥1 for U+221A
   (PUA cps), and the fonts contain those PUA glyphs (probe list output pasted in PR).
4. Old-cp bitmap invariance checker output: zero differences.
5. Flash deltas within REN-8 budgets, table in PR.

### 1.8 Rollback
Generated files: restore previous `.c`/`.h` from git; delete new headers + JSON map; revert
script changes. No behavior flag needed (nothing consumes the data yet); the
`NUMOS_TXP_ASSEMBLY_PROBE` gate (if the 1.3 trap fired) defaults off.

### 1.9 Agent prompt block

```text
You are implementing phase TXP-1 of the NumOS TeX-parity program.
Branch: create `claude/txp1-font-data-pipeline-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.4, §TXP-1;
NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md ticket MR-04 (you are subsuming it — say so in the
PR); scripts/extract_stix_math.py and scripts/generate_stix_math_font.sh in full.
Non-negotiables: ZERO rendering change — all 19 goldens byte-identical is a hard gate; generated
files only via the generators; deterministic PUA allocation; old-glyph bitmap invariance proven;
flash budgets: fonts ≤ +120 KB total, data headers ≤ +24 KB (abort per §TXP-1 1.6 if exceeded);
never modify assets/fonts/STIXTwoMath-Regular.ttf (write the remapped copy).
Watch the one trap in §TXP-1 1.3: regenerating fonts makes the U+239C probe
(src/ui/MathRenderer.cpp:431-435) flip g_delimiterAssemblyRenderable to true, which activates
expandDelimiterGeometryToAssembly (src/math/MathAST.cpp:149-151). Run candidates FIRST after
regeneration; if any delimiter stem diffs, add the NUMOS_TXP_ASSEMBLY_PROBE default-off gate
exactly as specified and re-verify byte-identity.
PR title: "feat(fonts): TXP-1 MATH-table data pipeline (kerns, overlap, PUA variants) [subsumes MR-04]".
PR body: flash-delta table, probe output, invariance-checker output, golden-compare output,
checklist: [goldens byte-identical] [budgets met] [PUA map committed] [MR-04 ranges included].
```

---

## TXP-2 — Atom-spacing fidelity (geometry wave 1)

### 2.1 Objective
Make inter-atom spacing exactly TeX's: style-conditional table entries, the `Bin→Op` correction,
and Bin→Ord demotion — verified per corpus items C-09…C-16 within ±1 px of the reference.

### 2.2 Preconditions
TXP-0 landed. Independent of TXP-1. Required green: full emulator CI at base.

### 2.3 Scope
- `src/math/MathTypography.h` — `kSpacingTable` entries (data only) + new `EffectiveClass`
  helper declarations.
- [NEW] `src/math/MathClassify.h` (header-only, LVGL-free) — effective-class computation (PD-8).
- `src/math/MathAST.cpp` — `NodeRow::calculateLayout` consumes effective classes.
- `src/ui/MathRenderer.cpp` — `drawRow` (`:1599-1636`) and cursor `childXOffset`/Finder row walk
  (`:938-954,1438-1461`) consume the same helper (G.6 lockstep).
- Tests + 2 new `.numos` scripts + candidate stems `txp2_spacing_demotion`, `txp2_cursor_row`.
NON-scope: any vertical geometry; any node's `mathClass()` virtual (static classes stay — PD-8).

### 2.4 Design

```cpp
// src/math/MathClassify.h [NEW] — pure, host-testable
namespace vpam {
constexpr int kEffClassMaxRow = 64;              // PD-8 fallback bound (see 2.5 step 4)
struct EffectiveClassResult {
    MathClass left  = MathClass::ORD;            // effective class facing left neighbors
    MathClass right = MathClass::ORD;            // effective class facing right neighbors
};
/// Computes effective classes for row children [0, n) per §H.5 D1-D3.
/// Writes min(n, kEffClassMaxRow) entries; children beyond the bound keep their
/// static classes (documented degradation, logged once under NATIVE_SIM).
void effectiveMathClasses(const NodeRow& row, EffectiveClassResult* out, int n);
}
```

`kSpacingTable` new content: exactly §H.5, parenthesized entries as negative codes; `Bin→Op` =
−2 (OQ-4). The existing `interAtomSpacingPx` (`MathTypography.h:166-176` [V]) already implements
negative-code suppression — unchanged.

### 2.5 Algorithm (`effectiveMathClasses`)
1. Pass 1: seed `out[i].left = child[i]->leftMathClass()`, `.right = rightMathClass()` [V
   virtuals `MathAST.h:607-616`].
2. Pass 2 (left-to-right, D1): for each i whose seeded left class is BINARY: let `prev` = the
   effective **right** class of the nearest previous non-Empty child (D3), or none if i is the
   first non-Empty. If `prev ∈ {none, BINARY, OP, REL, OPEN, PUNCT}` → `out[i].left =
   out[i].right = ORD`.
3. Pass 3 (right-to-left, D2): for each still-BINARY i: let `next` = seeded **left** class of
   the nearest following non-Empty child; if `next ∈ {REL, CLOSE, PUNCT}` or there is no next →
   `out[i] = ORD` both sides. (End-of-row Bin demotes: TeX's "last atom" rule.)
4. Rows longer than `kEffClassMaxRow`: children beyond the bound use seeded classes (no
   demotion); NATIVE_SIM logs `[LVGL] txp2-row-bound` once. Rationale: MR-01's node caps will
   make this unreachable; a heap allocation in layout would violate Y-5.
5. `NodeRow::calculateLayout` / `drawRow` / cursor walks call the helper once per row into a
   stack array and use `out[i-1].right`, `out[i].left` in the existing
   `interAtomSpacingPx(prevRight, currLeft, fm.style, fm.emSize)` calls
   (`MathAST.cpp:197-201`, `MathRenderer.cpp:1617-1619,1446-1449` [V]) — one mechanical
   substitution per site, guarded by `#if !defined(NUMOS_TXP2_LEGACY)`.

### 2.6 Test plan
- Host unit cases (12): `1+2` unchanged; leading `−3` (Bin demoted, 0 px before operand);
  `(−x)`; `x=−3` (thick before demoted minus in display, suppressed in script style);
  `a+−b`; trailing `x+`; `x^{a+b}` (script style: medium suppressed → sup row tightens);
  `x+∑` (Bin→Op now −2: medium in display, 0 in script); Empty-transparency case
  `[□, −, 3]`; 64+-child bound fallback; row-class propagation with leading demoted Bin;
  cursor `childXOffset` equals layout offsets for all of the above.
- Oracle: corpus C-09…C-16 features `spacing.*` within ±1 px.
- `.numos`: `txp2_spacing_demotion` (types `1 - 3 =` etc., capture), `txp2_cursor_row`
  (edit-mode capture with cursor after a demoted Bin — REN-11 script).
- Firmware compile + stress-suite frame-time note (REN-9).

### 2.7 Acceptance criteria
1. Oracle report: C-09…C-16 all `spacing.*` PASS; no other corpus item's features regress vs the
   TXP-0 baseline except spacing features (report diff attached).
2. Host cases 12/12 green; `NUMOS_TXP2_LEGACY` build reproduces pre-wave candidates byte-exactly.
3. Re-blessed goldens: only stems whose screens contain affected rows (predicted in-PR:
   `calc_1_plus_2` **unchanged** — Ord/Bin/Ord in display style is unchanged codes; list actual).
4. Cursor script green; frame budget unchanged (±5 % on `[MATH-STRESS]` lines if HIL available).

### 2.8 Rollback
`-DNUMOS_TXP2_LEGACY` restores: old table bytes + direct static-class lookups (keep the old
table as `kSpacingTableLegacy` under the flag). Flag deleted in the TXP-4 PR (PD-12).

### 2.9 Agent prompt block

```text
You are implementing phase TXP-2 of the NumOS TeX-parity program.
Branch: create `claude/txp2-spacing-fidelity-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G (esp. G.5,
G.6), §H.5, §TXP-2; src/math/MathTypography.h:70-176; src/math/MathAST.cpp:160-217;
src/ui/MathRenderer.cpp:1599-1636 and 1438-1461 and 911-960.
Non-negotiables: the THREE consumers (row layout, drawRow, cursor childXOffset + Finder row walk)
must switch to effectiveMathClasses in the same commit; no heap in layout (stack array, bound 64);
static mathClass() virtuals untouched; NUMOS_TXP2_LEGACY restores pre-wave bytes exactly; goldens
re-blessed by the human reviewer only — you regenerate candidates and attach diffs.
Deliverables: §TXP-2 2.3 file list, 12 host cases, 2 scripts, oracle before/after table in PR.
OQ-4 default: fix Bin→Op to −2 (note it in the PR).
PR title: "feat(math): TXP-2 TeX spacing fidelity (conditional entries + Bin demotion)".
PR checklist: [3-consumer lockstep] [legacy flag bit-exact] [oracle C-09..C-16 pass]
[candidate diffs attached for human bless] [firmware compiles].
```

---

## TXP-3 — True text metrics and math-italic identifiers (geometry wave 2)

### 3.1 Objective
Measured widths equal drawn widths for every text run (per-glyph advances + kerning), single
Latin letters render in STIX math italic per PD-3, and ± renders as the U+00B1 glyph — corpus
C-17…C-20 and all `width` features within tolerance.

### 3.2 Preconditions
TXP-0; TXP-2 recommended first (independent formulas, but lands before to keep wave diffs
disjoint). OQ-1 default applies.

### 3.3 Scope
- [NEW] `src/math/MathTextMetrics.h` — LVGL-free advance-measurement interface (design 3.4).
- `src/ui/MathRenderer.cpp`/`.h` — binds the interface to LVGL fonts; deletes the ± stroke path
  (`:1672-1710`); maps variable labels to italic cps at draw.
- `src/math/MathAST.cpp` — `NodeNumber/NodeVariable/NodeConstant/NodeOperator/NodeFunction/`
  `NodeLogBase/NodePeriodicDecimal::calculateLayout` width computation switches from
  `charWidth × n` to measured runs.
- `src/math/MathAST.h` — `NodeVariable::label()` gains `labelForRender()` returning the italic
  mapping (PD-3 table); host-testable pure function.
- Cursor Finder: no formula change needed (positions derive from child widths) — but the wave's
  cursor script still lands (G.6).
NON-scope: script/fraction/radical vertical geometry; `charWidth` stays in `FontMetrics` (used
by cursor width fallbacks and Empty sizing [V `MathAST.cpp:346-349`]).

### 3.4 Design

```cpp
// src/math/MathTextMetrics.h [NEW]
namespace vpam {
/// Measures a UTF-8 run in the current style's font. Implemented by the UI layer
/// (LVGL fonts); host tests implement it over the same generated font tables via the
/// harness font shim. Pure: no allocation, no global state.
struct TextMeasurer {
    /// Advance of `utf8` in px at the given script level, kerning-aware —
    /// MUST equal the sum of adv_w the draw loop produces (MathRenderer.cpp:2450-2486).
    int16_t (*advancePx)(const char* utf8, uint8_t scriptLevel);
};
/// Set once at MathCanvas construction; layout falls back to charWidth×chars when unset
/// (host tools that lay out without fonts keep working — total function).
void setTextMeasurer(const TextMeasurer* m);
int16_t measureTextAdvancePx(const char* utf8, uint8_t scriptLevel,
                             int16_t charWidthFallbackPx);
}
```

PD-3 mapping (in `MathAST.cpp`, pure):
```cpp
/// Render label for a NodeVariable: single Latin letters map to Mathematical Italic
/// (U+1D434 + i uppercase, U+1D44E + i lowercase, exception 'h' → U+210E);
/// multi-char labels (Ans, PreAns) and relational pseudo-variables ('=','<','>') unchanged.
const char* NodeVariable::labelForRender() const;   // returns static UTF-8 literals
```
The italic-correction lookup for Power bases (`MathAST.cpp:482-506` [V]) MUST use the
**rendered** codepoint (italic cp has the nonzero correction in `stix_math_italics.h`) — switch
it to decode `labelForRender()`.

### 3.5 Algorithm
1. Each text-bearing node's `calculateLayout` computes
   `width = measureTextAdvancePx(renderText, scriptLevel, fm.charWidth × visibleChars)` where
   `renderText` is exactly the string the draw path will emit (for `NodeVariable`:
   `labelForRender()`; for others: current strings [V symbols `MathAST.cpp:321-335,842-849`]).
   Normalization parity: draw normalizes via `normalizeMathTextNoAlloc`
   (`MathRenderer.cpp:2439-2442` [V]) — measurement must run the same normalization first
   (call it from the measurer binding, same 96-byte stack buffer pattern
   [V `MathTextNormalization.h:20,107-150`]).
2. `NodePeriodicDecimal` keeps per-segment layout but converts each segment with the measurer;
   its overline x-span follows the measured repeat-run width (draw `MathRenderer.cpp:2140-2149`
   updates identically).
3. ±: delete the stroke branch (`MathRenderer.cpp:1672-1710`); `NodeOperator::symbol()` already
   returns SYMB_PLUS_MINUS [V `MathAST.cpp:326`] so the generic text path takes over; width
   comes from step 1.
4. Zero-per-frame-heap discipline: the measurer walks glyphs exactly like the draw loop —
   O(bytes), stack only.
5. Guard the whole wave with `NUMOS_TXP3_LEGACY`.

### 3.6 Test plan
- Host (8): digit-run width == n×'0'-advance for tabular digits (sanity vs old behavior);
  `3.14` narrower than 4×charWidth; `sin` label vs old 3×charWidth; italic mapping table total
  (a–z, A–Z, h exception); measure==draw parity harness case that walks both code paths over the
  full corpus text inventory and asserts equality; fallback path (unset measurer) reproduces old
  widths bit-exactly.
- Oracle: all corpus `width` features re-baselined; C-17…C-20 (`sup.dx` on italic bases,
  identifier runs) within tolerance.
- `.numos`: `txp3_identifiers` (x, y, π row + capture), `txp3_cursor_text` (cursor inside a
  number after '.', REN-11).
- Firmware compile + REN-9 frame note.

### 3.7 Acceptance criteria
1. Measure==draw parity case: zero mismatches over the corpus inventory.
2. Oracle: no `spacing.*` regressions vs TXP-2 state; `width` deltas vs reference shrink or hold
   for ≥ 60 of 64 items (attach table).
3. Variables render italic in `txp3_identifiers` capture (human-visible check at bless time);
   digits and function labels unchanged upright.
4. `NUMOS_TXP3_LEGACY` build reproduces pre-wave candidates byte-exactly.
5. Re-bless list attached (expected: most text-bearing goldens).

### 3.8 Rollback
`-DNUMOS_TXP3_LEGACY` (old width formulas + upright labels + stroke ±). Deleted in TXP-5's PR.

### 3.9 Agent prompt block

```text
You are implementing phase TXP-3 of the NumOS TeX-parity program.
Branch: create `claude/txp3-text-metrics-italics-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §TXP-3,
PD-1, PD-3, OQ-1; src/ui/MathRenderer.cpp:2434-2487 (the draw loop you must mirror exactly),
1652-1714, 2086-2095; src/math/MathAST.cpp text-node layouts (:273-285, 312-319, 700-747,
851-857, 862-945, 960-983); src/ui/MathTextNormalization.h.
Non-negotiables: measurement must equal drawing (same normalization, same kerning-pair calls,
same glyph fallback advance line_height/3 — mirror MathRenderer.cpp:2480-2483); src/math stays
LVGL-free (function-pointer binding per §TXP-3 3.4); the Power italic-correction lookup switches
to the rendered codepoint; NUMOS_TXP3_LEGACY bit-exact; cursor script mandatory.
OQ-1 default: adopt italic (note in PR).
PR title: "feat(math): TXP-3 glyph-true text metrics + math-italic identifiers".
PR checklist: [measure==draw parity green] [legacy flag bit-exact] [oracle table attached]
[candidate diffs attached] [firmware compiles] [cursor script green].
```

---

## TXP-4 — Script placement: full Rule 18 + cramped styles (geometry wave 3)

### 4.1 Objective
Superscript/subscript placement implements §H.6 completely (boxed-base drops, TopMax/BottomMin
clamps in ink terms, cramped shift selection, the 18e interlock engine, italic-correction split),
with a `cramped` bit propagated per §H.2 — corpus C-21…C-30 within tolerance.

### 4.2 Preconditions
TXP-3 (italic bases make δ meaningful). Delete `NUMOS_TXP2_LEGACY` here (PD-12).

### 4.3 Scope
- `src/math/MathAST.h` — `FontMetrics` gains `bool cramped = false`; `superscript()` and
  `fractionPartMetrics()` propagate it per §H.2; `superscriptShiftMetrics` extended (4.4);
  [NEW] `scriptPlacement()` engine used by Power/Subscript/LogBase (and later NodeScripts/MR-12
  and TXP-7 inline limits).
- `src/math/MathAST.cpp` — `NodePower::calculateLayout` (`:460-529`),
  `NodeSubscript::calculateLayout` (`:1395-1424`), `NodeLogBase` (`:778-816`), radicand/denominator
  cramping call sites (`:376-378,568-570`).
- `src/ui/MathRenderer.cpp` — draw sites (`:1811-1858, 2401-2417, 2015-2057`) + inline-limit
  helpers stay on old shifts until TXP-7 (explicit NON-scope) + cursor Finder sites
  (`:988-1010, 1046-1064, 1157-1172`).
- Removes the truncating `duToPx` call at `MathAST.cpp:492` (PD-1).
NON-scope: `inlineLimitGeometry` (TXP-7); `NodeScripts` node (MR-12); MathKernInfo x-kerns
(TXP-9 — §H.6 step 6's cut-in terms are compiled out until then).

### 4.4 Design

```cpp
// MathAST.h — replaces/extends SuperscriptShiftMetrics (MathAST.h:473-513)
struct ScriptPlacement {
    int16_t supShiftPx = 0;    // u ≥ 0: sup baseline above main baseline (0 if no sup)
    int16_t subShiftPx = 0;    // v ≥ 0: sub baseline below main baseline (0 if no sub)
    int16_t supDxPx    = 0;    // sup x offset from base right edge (italic correction)
    int16_t subDxPx    = 0;    // sub x offset from base right edge (0 until TXP-9)
    int16_t groupWidthPx = 0;  // scripts group width incl. SpaceAfterScript
};
/// §H.6 total function. Pass nullptr for an absent script. baseInk*: ink bounds of the base;
/// baseIsGlyph per §H.6; deltaDu: italic correction of the base's last glyph in DU.
ScriptPlacement scriptPlacement(const FontMetrics& fm,
                                bool baseIsGlyph,
                                int16_t baseInkAscentPx, int16_t baseInkDescentPx,
                                int16_t deltaDu,
                                const LayoutResult* sup,   // laid out in fm.superscript()
                                const LayoutResult* sub);  // laid out cramped script style
```
`FontMetrics::superscript()` copies `cramped` through; new
`FontMetrics cramp() const { FontMetrics f = *this; f.cramped = true; return f; }`.
Cramping call-site map (§H.2/OQ-6): denominator (`fractionPartMetrics` on the denominator branch
— requires splitting the single childFm at `MathAST.cpp:376-378` into num/den variants),
radicand (`:568-570`), subscript slot, big-op lower slot (TXP-7 consumes), LogBase base slot.
The legacy `superscriptShiftMetrics`/`superscriptShiftPx` remain as thin wrappers over
`scriptPlacement` (they have diagnostic consumers [V `MathAST.h:515-518`]).

### 4.5 Algorithm
Exactly §H.6 steps 1–5 + 18f x-offsets (step 6's cut-ins excluded until TXP-9); the
`NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX` policy adjust [V `MathAST.h:390-399`] continues to apply to
the final `supShiftPx` with the same clamp semantics (`:495-509`) — it is a PD-recorded tuning
hook, default 0. Conversion points (PD-1): each constant converted individually by `scalePx`
(they are compared, not summed, in the max-clauses); the 18e redistribution works in px.
`baseIsGlyph` computation: type is Number(len==1)/Variable/Constant — one helper
`bool isSingleGlyphNode(const MathNode&)` shared with the italic-correction site.

### 4.6 Test plan
- Host (14): glyph base x² (u = σ13 unchanged → proves no regression); boxed base (x+1)²
  (u₀ = h−σ18 engages); deep exponent 2^{2^{2}} (BottomMin floor); cramped denominator x²∕y²
  (denominator's sup uses 252 DU); cramped radicand √(x²); subscript aₙ (TopMax clamp with tall
  sub); boxed-base subscript (v₀ = d+σ19); both-scripts interlock synthetic case at the GapMin
  boundary (±1 DU around 150) — engine-level via `scriptPlacement` directly; δ split (italic f
  base: sup dx = δ, sub dx = 0); empty-sup degenerate; empty-sub degenerate; LogBase base drop;
  descent-reservation removal check (`NodeSubscript` descent = max(base, v + sub.inkDescent) —
  no longer full sub height); wrapper equivalence (`superscriptShiftPx == scriptPlacement.sup`).
- Oracle: C-21…C-30 features `sup.baseline, sub.baseline, sup.dx, scripts.gap` pass.
- `.numos`: `txp4_scripts` capture; `txp4_cursor_scripts` (cursor in exponent + subscript).
- Firmware compile; stress cases with nested powers re-run.

### 4.7 Acceptance criteria
1. Oracle: C-21…C-30 pass; C-27 (fraction) `sup`-features inside denominators reflect cramped
   shifts (byte value asserted in host case, ±1 px vs reference).
2. Host 14/14; `NUMOS_TXP4_LEGACY` bit-exact pre-wave.
3. Subscript boxes no longer over-reserve: `x₁+1` total height equals reference ±1 px (was
   +sub.ascent too tall — GAP-21).
4. Cursor script green; re-bless list attached.

### 4.8 Rollback
`-DNUMOS_TXP4_LEGACY` (old shift formulas, no cramped bit consulted — the struct field remains,
inert). Deleted in TXP-6's PR.

### 4.9 Agent prompt block

```text
You are implementing phase TXP-4 of the NumOS TeX-parity program.
Branch: create `claude/txp4-rule18-scripts-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.2, §H.6,
§TXP-4, PD-6, OQ-6; src/math/MathAST.h:208-291,390-399,473-518; src/math/MathAST.cpp:376-378,
460-529,563-570,778-816,1395-1424; src/ui/MathRenderer.cpp:1811-1858,2015-2057,2401-2417 and the
cursor Finder :988-1010,1046-1064,1157-1172.
Non-negotiables: scriptPlacement is a total function (nullptr scripts legal); cramped set is
OQ-6 option (a) — all five slots; inlineLimitGeometry and big-op code are OUT of scope (TXP-7);
the three-consumer lockstep (layout/draw/Finder) per §G.6; NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX
semantics preserved; delete NUMOS_TXP2_LEGACY in this PR (PD-12); NUMOS_TXP4_LEGACY bit-exact.
PR title: "feat(math): TXP-4 full Rule-18 script placement + cramped styles".
PR checklist: [host 14/14] [oracle C-21..C-30 pass] [legacy flag bit-exact] [TXP2 flag removed]
[cursor script green] [candidate diffs attached] [firmware compiles].
```

---

## TXP-5 — Fractions and bars: σ-shift exactness (geometry wave 4)

### 5.1 Objective
Fraction vertical placement implements §H.7 (style-correct shift constants with gap-minimum
enforcement), the bar overhang is removed, fraction ink bounds become honest, and the
periodic-decimal overline uses the Overbar constants — corpus C-31…C-38 within tolerance.

### 5.2 Preconditions
TXP-4 (ink-based gap terms + cramped denominator). Delete `NUMOS_TXP3_LEGACY` here.

### 5.3 Scope
- `src/math/MathAST.h` — `fractionBarGaps` replaced by `fractionPlacement()` (§H.7);
  `FractionLayoutMetrics` keeps rule/thickness roles; overhang field deleted;
  `NodePeriodicDecimal` overline constants (`:1129-1132`) replaced by accessor-driven values.
- `src/math/MathAST.cpp` — `NodeFraction::calculateLayout` (`:372-425`) → §H.7; ink bounds
  (`:423-424`) → child-derived; `NodePeriodicDecimal::calculateLayout` (`:960-983`).
- `src/ui/MathRenderer.cpp` — `drawFractionBaseline` (`:1738-1801`, overhang removal at
  `:1760-1773`), periodic overline (`:2140-2149`), cursor Finder fraction block (`:956-987`).
NON-scope: `fractionPartMetrics` size policy (PD-4 keeps it); `NodeBinom` (MR-14 implements
§H.7.7 when it lands — reference it, do not build it).

### 5.4 Design

```cpp
// MathAST.h — replaces FractionBarGaps/fractionBarGaps (MathAST.h:569-588)
struct FractionPlacement {
    int16_t numShiftPx;      // numerator baseline above MAIN baseline (u)
    int16_t denShiftPx;      // denominator baseline below MAIN baseline (v)
    int16_t barTopPx;        // above baseline (= axis + ceil(θ/2))
    int16_t barBottomPx;     // above baseline (= axis − floor(θ/2))
    int16_t ruleThicknessPx; // θ
};
FractionPlacement fractionPlacement(const FontMetrics& fm,
                                    const LayoutResult& numL,   // laid out per PD-4 metrics
                                    const LayoutResult& denL);  // laid out per PD-4 + cramped
```
Note the coordinate change: shifts become baseline-relative (§H.7), not bar-relative like today's
`_numeratorShiftPx` [V `MathAST.cpp:396-397` bar-relative]. The node's cached fields and their
accessors (`MathAST.h:785-790`) switch meaning; the draw/cursor sites that compose
`yAxis − barHalfUp − numShift` (`MathRenderer.cpp:1766-1775, :975-986` [V]) become
`yBaseline − numShiftPx` — simpler and identical in all three consumers.

### 5.5 Algorithm
§H.7 steps 1–6 verbatim, one conversion point per named constant. Overline (periodic decimal):
gap = `mc.overbarVerticalGap()`, thickness = `mc.overbarRuleThickness()` (scalePxMin 1), extra
ascender = `mc.overbarExtraAscender()`; ascent formula replaces the two hardcoded 1-px constants
(`MathAST.cpp:976-982`). Wave flag `NUMOS_TXP5_LEGACY`.

### 5.6 Test plan
- Host (10): simple 1/2 in display (u = max(640 DU′, barTop+150 DU′+inkDesc) — assert the
  max-branch taken at em 18 and the resulting px); same in TEXT (585/68 constants selected);
  tall numerator (nested fraction) — gap branch engages; ink honesty: (1/2) inside parens —
  enclosing delimiter target shrinks vs baseline (uses TXP-0 metric `delim.height`); overhang
  removal: bar span == max(child widths); periodic 0.1̄6̄ overline y from Overbar constants;
  cramped denominator wired (from TXP-4); degenerate empty num; empty den; centering unchanged.
- Oracle: C-31…C-38 (`frac.*` features + `delim.height` on C-49) pass.
- `.numos`: `txp5_fractions`, `txp5_cursor_fraction` (cursor in numerator + denominator).

### 5.7 Acceptance criteria
1. Oracle C-31…C-38 pass; `calc_fraction_sum` golden re-bless shows bar/den/num moves ≤2 px
   (attach the before/after metric rows proving PD-4's "bounded change" claim).
2. Host 10/10; `NUMOS_TXP5_LEGACY` bit-exact; TXP3 flag removed.
3. Fraction `inkAscent/inkDescent` equal `u+inkAscent(N)` / `v+inkDescent(D)` (host-asserted).
4. Cursor script green; re-bless list attached.

### 5.8 Rollback
`-DNUMOS_TXP5_LEGACY` (fractionBarGaps + overhang + hardcoded overline). Deleted in TXP-7's PR.

### 5.9 Agent prompt block

```text
You are implementing phase TXP-5 of the NumOS TeX-parity program.
Branch: create `claude/txp5-fraction-exactness-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.7,
§TXP-5, PD-4; src/math/MathAST.h:417-442,549-588,755-804,1100-1139;
src/math/MathAST.cpp:352-441,950-983; src/ui/MathRenderer.cpp:1738-1801,2097-2151 and cursor
Finder :956-987.
Non-negotiables: the shift-coordinate change (bar-relative → baseline-relative) must land in
layout, draw, and cursor Finder in one commit (§G.6); fractionPartMetrics size policy untouched
(PD-4); NodeBinom untouched; single conversion point per constant (PD-1); NUMOS_TXP5_LEGACY
bit-exact; delete NUMOS_TXP3_LEGACY here.
PR title: "feat(math): TXP-5 exact fraction placement (sigma shifts + gap minima)".
PR checklist: [host 10/10] [oracle C-31..C-38 pass] [coordinate change 3-consumer lockstep]
[legacy flag bit-exact] [TXP3 flag removed] [cursor script green] [candidate diffs attached].
```

---

## TXP-6 — Radicals: degree placement and the glyph radical (geometry wave 5)

### 6.1 Objective
Radical layout implements §H.8: the degree is raised by 55 % of the radical's own vertical span
measured from its bottom, the radicand is cramped, gaps/rules use ink bounds, and — per OQ-5 —
the radical mark comes from the U+221A variant ladder when TXP-1 fonts provide it, with the
vector hook as permanent fallback. Corpus C-39…C-44 within tolerance.

### 6.2 Preconditions
TXP-1 (U+221A variants in `stix_math_variants_ext.h` + fonts), TXP-4 (cramped bit). Delete
`NUMOS_TXP4_LEGACY` here.

### 6.3 Scope
- `src/math/MathAST.cpp` — `NodeRoot::calculateLayout` (`:563-616`): ink-based ascent chain,
  cramped radicand, §H.8 degree formula, glyph-ladder symbol width when available.
- `src/math/MathAST.h` — `NodeRoot` gains `int16_t radicalGlyphHeightPx()/radicalSymbolWidthPx()`
  cached fields (existing accessor pattern `MathAST.h:885-890` [V]).
- `src/math/font/MathGlyphAssembly.*` — no change (ladder already generic); include the `_ext`
  variants header in the lookup for U+221A only under `#if !defined(NUMOS_TXP6_LEGACY)`
  (a second lookup table array; `lookupVariantTable` gains a chained search — additive).
- `src/ui/MathRenderer.cpp` — `drawRootBaseline` (`:1870-1925`): draw the selected variant/
  assembly via `drawDelimiterGlyph` when the ladder resolves (it already handles variants +
  assemblies + vector fallback [V `:184-283`]); degree baseline per §H.8; cursor Finder root
  block (`:1011-1031`).
NON-scope: delimiter Rule 19 (TXP-8); overline constants (done in TXP-5); nth-root semantics.

### 6.4 Design
Selection: `target = inkAscent(R) + ψ + θ + inkDescent(R)` (§H.8.2);
`DelimiterAssembler::glyphMetricsForHeightPx(0x221A, target, fm.emSize)` [V API
`MathGlyphAssembly.h:113-115`] — valid ⇒ glyph path (symbol width = `widthPx`, glyph bottom at
`yBaseline + inkDescent(R)`, overline continues from the glyph's top-right at the rule y); invalid
(no table / fonts without PUA variants / `NUMOS_TXP6_LEGACY`) ⇒ current vector geometry
bit-exactly [V `MathRenderer.cpp:1876-1911`].
Degree (§H.8.3): `radTop = yBaseline − ascent(node) + ε` (overline top), `radBottom = yBaseline +
inkDescent(R)`; `degBaseline = radBottom − (ρ/100)·(radBottom − radTop)` computed in px (single
multiply, PD-1).

### 6.5 Algorithm — as §H.8; ascent uses `inkAscent(R)` (today `radL.ascent` [V
`MathAST.cpp:601-603`]); radicand laid out with `fm.cramp()`; node ink = derived from radicand
ink + rule/ascender (mirror TXP-5's honesty rule).

### 6.6 Test plan
- Host (8): degree y for shallow (√2, degree 3) and deep (cube root of a 3-tall nested fraction)
  radicands — the deep case is where the old em-based formula diverges (assert ≥3 px difference
  from legacy, matching reference); cramped radicand superscript; ink ascent chain; glyph-ladder
  selection at heights below/above the first PUA variant; vector fallback bit-exactness with
  `_ext` absent; degenerate no-degree; empty radicand.
- Oracle: C-39…C-44 (`radical.gap`, `radical.rule.y`, `degree.baseline`) pass.
- `.numos`: `txp6_radicals`, `txp6_cursor_radical` (cursor in degree + radicand).

### 6.7 Acceptance criteria
1. Oracle C-39…C-44 pass.
2. Deep-radicand degree case matches reference within ±1 px (the GAP-33 fix, host-asserted).
3. With TXP-1 fonts: `assert`ed glyph path used for √ in `txp6_radicals` (NATIVE_SIM tally or
   capture diff vs vector build); without them: vector path bit-exact.
4. Host 8/8; `NUMOS_TXP6_LEGACY` bit-exact; TXP4 flag removed; cursor script green.

### 6.8 Rollback
`-DNUMOS_TXP6_LEGACY` (em-based degree raise, uncramped radicand, vector-only). Deleted in
TXP-8's PR.

### 6.9 Agent prompt block

```text
You are implementing phase TXP-6 of the NumOS TeX-parity program.
Branch: create `claude/txp6-radicals-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.8,
§TXP-6, OQ-5; src/math/MathAST.cpp:550-641; src/math/MathAST.h:844-901;
src/ui/MathRenderer.cpp:1870-1925 and 184-283 and cursor Finder :1011-1031;
src/math/font/MathGlyphAssembly.h (whole).
Non-negotiables: vector radical remains the permanent fallback tier, bit-exact under the legacy
flag and when variants are absent; radicand cramped; degree formula per §H.8.3 exactly;
3-consumer lockstep; delete NUMOS_TXP4_LEGACY here.
PR title: "feat(math): TXP-6 radical exactness (degree raise, cramped radicand, glyph radical)".
PR checklist: [host 8/8] [oracle C-39..C-44 pass] [vector fallback bit-exact] [legacy flag
bit-exact] [TXP4 flag removed] [cursor script green] [candidate diffs attached].
```

---

## TXP-7 — Big operators and limits (geometry wave 6)

### 7.1 Objective
Display-size operator glyphs become reachable (TXP-1 PUA variants), the display/inline gate is
one rule for all three operator families, limits use the four correct constants with no invented
padding, the drawn glyph is axis-centered in ink, and inline limits use the Rule-18 engine with
the ∫ italic-correction split. Corpus C-45…C-52 within tolerance.

### 7.2 Preconditions
TXP-1 (`stix_math_variants_ext.h`), TXP-4 (`scriptPlacement`). Delete `NUMOS_TXP5_LEGACY` here.

### 7.3 Scope
- `src/math/font/` — operator/radical `_ext` tables join the main lookup unconditionally (the
  TXP-6 chained search extends to Σ ∫ big-op cps under `!NUMOS_TXP7_LEGACY`).
- `src/math/MathAST.h` — `largeOperatorSymbolPadPx` deleted (PD-11);
  `largeOperatorGlyphAscentPx/DescentPx` (`:365-374`) retired in favor of ink-based centering
  fields cached on the nodes; `shouldUseDisplayLimits` (`:410-415`) becomes THE gate for all
  families.
- `src/math/MathAST.cpp` — `NodeDefIntegral::calculateLayout` (`:1049-1127`),
  `NodeSummation::calculateLayout` (`:1182-1245`), `NodeBigOp::calculateLayout` (`:1310-1376`,
  removing the gate bypass `:1319-1324`).
- `src/ui/MathRenderer.cpp` — `displayLimitGeometry`/`inlineLimitGeometry` (`:285-347`),
  the three draw functions (`:2164-2380`), operator glyph drawing switches from
  `drawTextBaseline` at natural baseline to axis-centered placement via `drawDelimiterGlyph`
  (which already draws arbitrary-cp variants at a given box [V `:184-283`]); cursor Finder
  big-op blocks (`:1066-1225`).
NON-scope: `NodeLimit` (MR-12); `forceDisplayStyle` (MR-12); Σ/Π evaluation (MR-09); the 3 mu
body gap (kept, PD-11).

### 7.4 Design

```cpp
// MathAST.h [P] — shared by the three operator nodes, cached during layout
struct LargeOpGeometry {
    uint32_t glyphCp;          // selected variant cp (may be PUA) or base cp
    bool     isAssembly;       // ladder said assemble (∫ tall cases)
    int16_t  widthPx, heightPx;
    int16_t  inkAscentPx;      // glyph ink above main baseline AFTER axis centering:
                               //   inkAscentPx = (heightPx+1)/2 + axisHeight
    int16_t  inkDescentPx;     //   inkDescentPx = heightPx − inkAscentPx
    bool     displayLimits;    // §H.9.4 gate result
};
LargeOpGeometry largeOpGeometry(uint32_t baseCp, const FontMetrics& fm);
```
(The ±1 px symmetric-ink approximation of §H.9.1 is encoded here; draw positions the glyph so
its measured LVGL ink box matches `inkAscentPx` — the draw-side correction uses the real
`glyph.ofs_y/box_h` like `drawDelimiterGlyph`'s centering [V `MathRenderer.cpp:205-209`].)

Display-limit vertical chain (§H.9.2), replacing `MathAST.cpp:1092-1099`:
```
upperBaseline = glyphTop − max(upperLimitGapMin() + inkDescent(U), upperLimitBaselineRiseMin())
lowerBaseline = glyphBottom + max(lowerLimitGapMin() + inkAscent(L), lowerLimitBaselineDropMin())
ascent  = max(body.ascent,  −upperBaseline + ascent(U))      // baseline-relative signs per §TXP-0 0.5.4
descent = max(body.descent,  lowerBaseline + descent(L))
```
Inline: `scriptPlacement(fm, /*baseIsGlyph=*/true, opInkAsc, opInkDesc, δ_op, &upperL, &lowerL)`
— δ_op from `lookupItalicsCorrection(glyphCp)` (∫ = 230 DU [V]); sup at `+δpx`, sub at `+0`
(§H.9.3). The `d`+variable tail of `NodeDefIntegral` keeps its 2 mu gaps [V `MathAST.h:381-384`]
(the differential is an Ord run; 2 mu ≈ the thin-ish space engines use — recorded under PD-11's
umbrella).

### 7.5 Algorithm — §H.9 steps 1–5; gate: `displayLimits = (fm.style == DISPLAY_STYLE) &&
(heightPx ≥ displayOperatorMinHeight())` for **all three** node families; with TXP-1 fonts the
Σ ladder now returns the ~32 px PUA variant in display style, so the gate passes at the top
level and fails inside fractions (TEXT via `fractionPartMetrics`) — the TeX behavior. Wave flag
`NUMOS_TXP7_LEGACY` (also holds the `_ext` operator lookup off).

### 7.6 Test plan
- Host (12): Σ display selects a variant ≥ 32 px and stacks limits (gate now true — the GAP-36
  headline case); Σ in TEXT inlines; ∏ (`NodeBigOp`) obeys the same gate (bypass removed);
  upper/lower use their OWN constants (synthetic table with unequal gap minima via a test-only
  provider shim — or assert the constant IDs referenced by code inspection case comments and px
  math at STIX values); RiseMin/DropMin engage for single-glyph limits (135+ink < 300/670 DU
  cases — assert the max branch); symbolPad gone (limit gap == constants exactly); inline ∫
  sup dx = scalePx(230); axis-centered ink (inkAscent − inkDescent == 2·axisHeight ± 1);
  empty-limit degenerates; body gap unchanged 3 mu; assembly ∫ for a 3-line integrand; width =
  max(glyph, U, L) centering.
- Oracle: C-45…C-52 (`op.height`, `limit.*`) pass.
- `.numos`: `txp7_bigops` (display Σ with limits — the first-ever such capture),
  `txp7_cursor_limits`.
- Firmware: flash/RAM note (ladder table growth already accounted in TXP-1).

### 7.7 Acceptance criteria
1. Oracle C-45…C-52 pass; C-45's `op.height` ≥ 32 px in display (was 19 px in the TXP-0
   baseline — cite the baseline JSON in the PR).
2. Gate identical across Summation/DefIntegral/BigOp (host-asserted on all three).
3. Host 12/12; `NUMOS_TXP7_LEGACY` bit-exact (including small-Σ layout); TXP5 flag removed;
   cursor script green; re-bless list attached (expect `math_showcase_smoke` +
   summation-bearing stems).

### 7.8 Rollback
`-DNUMOS_TXP7_LEGACY`. Deleted in TXP-9's PR.

### 7.9 Agent prompt block

```text
You are implementing phase TXP-7 of the NumOS TeX-parity program.
Branch: create `claude/txp7-big-operators-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.9,
§TXP-7, PD-11, GAP-36..GAP-41; src/math/MathAST.h:353-415; src/math/MathAST.cpp:985-1376;
src/ui/MathRenderer.cpp:285-347,2164-2380 and cursor Finder :1066-1225;
src/math/font/stix_math_variants_ext.h (from TXP-1).
Non-negotiables: ONE gate for all three operator families; upper and lower limits use their own
four constants; the 4 mu symbolPad is deleted, the 3 mu body gap stays (PD-11); inline limits go
through scriptPlacement with the operator's italic correction; drawn glyph axis-centered in ink;
3-consumer lockstep incl. displayLimitGeometry/inlineLimitGeometry mirrors in the cursor Finder;
NUMOS_TXP7_LEGACY bit-exact; delete NUMOS_TXP5_LEGACY here.
PR title: "feat(math): TXP-7 display operators + exact limit placement".
PR checklist: [gate unified] [host 12/12] [oracle C-45..C-52 pass] [legacy flag bit-exact]
[TXP5 flag removed] [cursor script green] [candidate diffs attached] [firmware compiles].
```

---

## TXP-8 — Stretchy delimiters: Rule 19 sizing (geometry wave 7)

### 8.1 Objective
Delimiter target heights follow §H.10 (delimiterfactor 901 / shortfall per OQ-2, axis-symmetric,
with the stated floors), the `parenWidth/3` inner pad is removed, and assemblies respect
`MinConnectorOverlap`. Corpus C-53…C-58 within tolerance. Coordinates with MR-11 so the
delimiter re-bless happens once.

### 8.2 Preconditions
TXP-1 (assembly glyphs + `kStixMinConnectorOverlapDu`), TXP-5 (honest fraction ink feeding
delimiter targets). Delete `NUMOS_TXP6_LEGACY` here. If MR-11 has not landed, this wave IS the
assembly go-live for delimiters (state it in the PR and satisfy MR-11's gallery requirement);
if MR-11 landed first, this wave only changes sizing.

### 8.3 Scope
- `src/math/MathAST.cpp` — `symmetricAxisDelimiterGeometry`/`delimiterVerticalPadPx`/
  `assembledDelimiterWidthPx` (`:91-155`) replaced by `delimiterTarget()` per §H.10;
  `NodeParen::calculateLayout` (`:659-673`), `NodeFunction` (`:729-747`), `NodeLogBase`
  (`:788-816`) inner-pad removal.
- `src/math/font/MathGlyphAssembly.cpp` — overlap floor: per-seam
  `chosenOverlap[i] = clamp(share, minOverlapPx, maxOv)` with
  `minOverlapPx = duToPxCeil(kStixMinConnectorOverlapDu)` (`:236-253` block); the extender-count
  loop's reachability test uses `maxHeight(overlap = minOverlapPx)` instead of raw height sums
  (`:220-228`).
- `src/ui/MathRenderer.cpp` — draw sites consuming `parenWidth/innerPad`
  (`:1934-1957,1970-2001,2015-2057`), cursor Finder paren/function/log blocks (`:1032-1064`).
NON-scope: `DelimKind::Floor/Ceil` (MR-14); GlyphProvider facade (MR-06).

### 8.4 Design

```cpp
// MathAST.cpp (or layout unit if MR-05 landed) [P]
struct DelimiterTarget { int16_t targetHeightPx; int16_t ascentPx; int16_t descentPx; };
DelimiterTarget delimiterTarget(const LayoutResult& content, const FontMetrics& fm,
                                int16_t baseGlyphHeightDuCeilPx);
inline constexpr int16_t kDelimiterFactorPermille = 901;         // TeX plain [U]
inline int16_t delimiterShortfallPx(const FontMetrics& fm) {     // OQ-2
    return static_cast<int16_t>((fm.emSize + 1) / 2);
}
```
Steps 1–5 of §H.10 verbatim; ascent/descent from the **selected** glyph height (step 4), so the
box hugs what is actually drawn (today's box uses the pre-selection geometry [V
`MathAST.cpp:666-672`]).

### 8.5 Algorithm — §H.10; content ink from `layoutInk*` helpers [V `MathAST.h:461-471`]
(honest post-TXP-5). Inner pad: content x = delim x + advance; the reserved node width becomes
`2·delimWidth + content.width`. Wave flag `NUMOS_TXP8_LEGACY`.

### 8.6 Test plan
- Host (10): 1-line parens (target = max(⌈0.901·size⌉, size−9) at em 18 — assert px);
  2-line fraction content (undershoot engages: delim < content ink — the visible Rule-19
  signature); display-style `DelimitedSubFormulaMinHeight` floor case; Empty-slot floor;
  base-glyph floor; overlap floor: synthetic assembly where even distribution would give a seam
  < min (assert clamped and target still met by an extra extender); pad removal widths;
  bar `|x|`; function auto-parens follow the same target; nested delimiters.
- Oracle: C-53…C-58 (`delim.height`, `delim.inner.dx`) pass.
- `.numos`: `txp8_delimiters`, `txp8_cursor_paren`; if this wave is the assembly go-live, also
  the MR-11 gallery set (`mathv2_assembly_delims` stem name reserved by the test plan §5 [V]).
- Firmware compile.

### 8.7 Acceptance criteria
1. Oracle C-53…C-58 pass; the undershoot case's `delim.height` equals the reference within
   ±2 px (extent class).
2. Assemblies: no seam overlap below `duToPxCeil(kStixMinConnectorOverlapDu)` (host-asserted
   over every delimiter table in the font at 5 target heights).
3. Host 10/10; `NUMOS_TXP8_LEGACY` bit-exact; TXP6 flag removed; cursor script green;
   coordinated re-bless executed once (gallery attached if go-live).

### 8.8 Rollback
`-DNUMOS_TXP8_LEGACY` (cover+pad targets, innerPad, unfloored overlap). Deleted in TXP-10's PR.

### 8.9 Agent prompt block

```text
You are implementing phase TXP-8 of the NumOS TeX-parity program.
Branch: create `claude/txp8-rule19-delimiters-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.10,
§TXP-8, PD-5, OQ-2; src/math/MathAST.cpp:91-155,643-816; src/math/font/MathGlyphAssembly.cpp
(whole); src/ui/MathRenderer.cpp:184-283,1934-2057 and cursor Finder :1032-1064;
docs/specs/NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md ticket MR-11 (coordinate — check whether
it landed; either way there must be exactly ONE delimiter re-bless wave).
Non-negotiables: §H.10 floors exactly (base glyph, Empty line box, display-only
DelimitedSubFormulaMinHeight); MinConnectorOverlap enforced in the assembler; inner pad removed
in NodeParen AND NodeFunction AND NodeLogBase; 3-consumer lockstep; OQ-2 default shortfall
round(em/2); NUMOS_TXP8_LEGACY bit-exact; delete NUMOS_TXP6_LEGACY here.
PR title: "feat(math): TXP-8 Rule-19 delimiter sizing + MinConnectorOverlap".
PR checklist: [host 10/10] [oracle C-53..C-58 pass] [overlap floor asserted] [single
coordinated re-bless] [legacy flag bit-exact] [TXP6 flag removed] [cursor script green].
```

---

## TXP-9 — Script kerning: MathKernInfo cut-ins (geometry wave 8)

### 9.1 Objective
Sub/superscript x-positions apply the OpenType corner-kern staircases (§H.6 step 6) for
glyph-adjacent script attachments — corpus C-59…C-62 within tolerance; PD-7's supersession of
MRV2 §14 becomes code.

### 9.2 Preconditions
TXP-1 (`stix_math_kerns.h`), TXP-4 (`scriptPlacement` owns the x-offsets). Delete
`NUMOS_TXP7_LEGACY` here.

### 9.3 Scope
- `src/math/MathAST.cpp` — `scriptPlacement` gains the kern terms; the glyph-pair resolution
  helper (base last-glyph cp, script first-glyph cp) shared with the italic-correction site.
- `src/ui/MathRenderer.cpp` + cursor Finder — consume the updated `supDxPx/subDxPx` (no new
  formulas — they already flow through the TXP-4 struct; verify the three consumers read the
  struct, not recompute).
NON-scope: TopLeft/BottomLeft corners (no prescripts exist); kerning of limit attachments
(display limits are centered, not corner-attached); text-run kerning (LVGL already applies pair
kerns in draw and TXP-3 in measure).

### 9.4 Design / algorithm
For a glyph base with sup: `supDxPx = scalePx(δ) + scalePx(kern)` where
`kern = lookupMathKern(baseCp, TopRight, hDu) + lookupMathKern(supCp, BottomLeft, hDu′)`;
attachment heights per OpenType: `hDu = duOf(u − inkDescent(X))` for the sup bottom corner and
the corresponding base-corner height — compute both in DU **before** conversion (PD-1: kerns are
summed in DU, converted once). Sub symmetric with BottomRight/TopLeft at depth `v`.
`duOf(px) = px·1000/fm.emSize` (inverse conversion, truncating — documented). Absent staircases
⇒ 0 (total function [P TXP-1 lookup contract]). Wave flag `NUMOS_TXP9_LEGACY`.

### 9.5 Test plan
- Host (8): known staircase glyphs from TXP-1's hand-checked trio: assert kern px at two
  attachment heights straddling a staircase step; V^a / f_a style pairs; kern + δ composition;
  non-glyph base ⇒ no kern; absent-data ⇒ 0; DU-summation rounding (kern pair whose separate
  rounding would differ by 1 px); sub corner; cursor x equality.
- Oracle: C-59…C-62 (`sup.dx`, `sub.dx`) pass.
- `.numos`: `txp9_kerns`, `txp9_cursor_kerns`.

### 9.6 Acceptance criteria
1. Oracle C-59…C-62 pass (reference engines apply the same tables — this is the direct
   OpenType-vs-OpenType check).
2. Host 8/8; `NUMOS_TXP9_LEGACY` bit-exact; TXP7 flag removed; cursor script green; re-bless
   list attached (only pairs with ≥1 px kerns move — list them from the oracle diff).

### 9.7 Rollback
`-DNUMOS_TXP9_LEGACY` (kern terms compiled out). Deleted in the TXP-10 PR.

### 9.8 Agent prompt block

```text
You are implementing phase TXP-9 of the NumOS TeX-parity program.
Branch: create `claude/txp9-mathkern-cutins-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md §G, §H.4, §H.6
step 6, §TXP-9, PD-7; src/math/font/stix_math_kerns.h (from TXP-1); the TXP-4 scriptPlacement
implementation in src/math/MathAST.h/.cpp.
Non-negotiables: kerns summed in DU, one conversion (PD-1); total function (missing data = 0);
only TopRight/BottomRight+BottomLeft/TopLeft-of-script corners as specified; consumers read
supDxPx/subDxPx from ScriptPlacement — no recomputation anywhere; NUMOS_TXP9_LEGACY bit-exact;
delete NUMOS_TXP7_LEGACY here.
PR title: "feat(math): TXP-9 OpenType script cut-in kerning".
PR checklist: [host 8/8] [oracle C-59..C-62 pass] [legacy flag bit-exact] [TXP7 flag removed]
[cursor script green] [candidate diffs attached].
```

---

## TXP-10 — Parity audit and report

### 10.1 Objective
Re-run the full corpus against the reference, flip the oracle's reference-mode comparison to a
CI gate, commit the written parity report, and update this spec's gap matrix statuses — the
program's exit gate.

### 10.2 Preconditions
TXP-2…TXP-9 landed (any OUT-OF-SCOPE-tagged MRV2 tickets need not have landed). All legacy flags
except `NUMOS_TXP8/9_LEGACY` already deleted; delete both here.

### 10.3 Scope
- [NEW] `docs/reports/NUMOS_TEX_PARITY_REPORT.md` — the committed report (10.4).
- `docs/specs/NUMOS_TEX_PARITY_TYPESETTING_SPEC.md` — §B status column updates + §B statistics.
- `.github/workflows/emulator-build.yml` — flip `compare-tex-parity.py` reference mode from
  warning to gating (append-only step edit).
- `tests/tex_parity/waivers.json` — final waiver set: every remaining failure maps to a PD.
- Optional (dev-machine, not CI): generate the human gallery via the PD-14 secondary tool;
  commit the PNG pairs under `docs/reports/tex_parity_gallery/` (≤ 2 MB total; downscale to
  1× if needed).
NON-scope: any geometry change — if the audit finds a miss, it files it as a follow-up issue and
the report says so; TXP-10 itself is docs+CI only.

### 10.4 Report structure (normative)
1. Header: commit, date, oracle manifest hash, corpus size.
2. Per-item table: ID, verdict, worst feature delta, waivers applied.
3. PD conformance table: every PD with a measurable consequence (PD-4, PD-5, PD-10, PD-11) and
   its measured value vs declared band.
4. Before/after: TXP-0 baseline expected-failure count vs final failure count (must be
   0 unwaived).
5. Human gallery sign-off line (maintainer initials + date) — the "trained eye" gate of §A.1.3.
6. Residual gaps: OUT-OF-SCOPE rows restated with their owning tickets (MR-12/13/14/17, accents
   F) so the next program has a clean handoff.

### 10.5 Acceptance criteria
1. `compare-tex-parity.py` reference mode: 64/64 items PASS (waivers all PD-mapped); CI gate
   green on two consecutive runs.
2. §B statistics row updated; every previously-PARTIAL/MISSING in-scope row reads DONE with the
   closing phase noted.
3. Report committed with gallery sign-off; EXPECTED_FAILURES.md deleted (superseded by the
   report).
4. All TXP legacy flags gone (grep `NUMOS_TXP` in `src/` returns only spec-comment references).

### 10.6 Rollback
Revert the CI gate flip; the report is documentation.

### 10.7 Agent prompt block

```text
You are implementing phase TXP-10 of the NumOS TeX-parity program.
Branch: create `claude/txp10-parity-report-<suffix>` from latest main.
Read first: NUMOS_FABLE_CONTEXT_HEADER.md; NUMOS_TEX_PARITY_TYPESETTING_SPEC.md in full (you are
updating it); tests/tex_parity/* state; docs/specs/NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md
(CI step conventions).
Non-negotiables: zero geometry changes; every waiver maps to a PD with a band; the report follows
§TXP-10 10.4 exactly; delete NUMOS_TXP8_LEGACY and NUMOS_TXP9_LEGACY; the human gallery sign-off
line is left blank for the maintainer — do not sign it yourself.
PR title: "docs(reports): TXP-10 TeX-parity audit report + CI gate".
PR checklist: [64/64 pass or waived] [gap matrix updated] [flags removed] [CI gate green twice]
[gallery generated] [sign-off line blank].
```

---

# Appendix A — Superseded clauses of prior specs

TXP inherits the MRV2 document set wholesale except for the clauses below. Each supersession
names the clause, the superseding authority, and the effective phase. An MRV2 ticket that lands
**after** the named phase MUST follow the TXP version.

| # | Superseded clause | Where | Superseded by | Effective |
|---|---|---|---|---|
| S-1 | "Skip: cramped styles" (Special Question 12 answer) | `NUMOS_MATHRENDERER_V2_ARCHITECTURE_SPEC.md` §8 row 12; `NUMOS_MATH_LAYOUT_ENGINE_AND_TYPESYSTEM_SPEC.md` §2 ("No cramped styles") and §14 row "Cramped styles" | PD-6, §H.2, TXP-4 | TXP-4 merge |
| S-2 | "Skip: math kerning cut-ins" (same Q12 answer; §14 row "Math kerning") | same two docs | PD-7, §H.4, §H.6 step 6, TXP-1/TXP-9 | TXP-9 merge |
| S-3 | "Exact TeX Rule 18 sub/sup interlock — simplify (reduced form)" | layout spec §14 last row; §5 `NodeScripts` pseudocode | §H.6 full form; MR-12's `NodeScripts` MUST call the TXP-4 `scriptPlacement` engine, not implement §5's reduced form | TXP-4 merge |
| S-4 | Gap-derived symmetric fraction shifts as the [V] contract | layout spec §6.1; `fractionBarGaps` rationale `MathAST.h:549-588` | §H.7, PD-4, TXP-5 | TXP-5 merge |
| S-5 | Delimiter sizing pseudocode (cover content + `delimiterVerticalPad`) | layout spec §10 "Delimiter-sizing pseudocode" | §H.10, PD-5, TXP-8 | TXP-8 merge |
| S-6 | Large-op layout pseudocode (single `UpperLimitGapMin`, implicit symbol pad) | layout spec §8.3 | §H.9, PD-11, TXP-7 | TXP-7 merge |
| S-7 | `NodeBigOp` display-limit gate bypass ("must keep layout … in the above/below branch even when a subset font has no taller variant") | code comment `MathAST.cpp:1319-1324` (a de-facto decision) | §H.9.4 unified gate (reachable via TXP-1 variants) | TXP-7 merge |

**Explicitly NOT superseded** (inherited unchanged, listed to prevent re-litigation): AD-1
single-tree architecture; the capability lattice; NumIR as canonical serialization; depth/node
budgets (24/4096/16); the readable-fraction *size* policy (PD-4 ratifies it); rejection of
`\mathchoice` (MR-12's `forceDisplayStyle` bit stands); rejection of formula line-breaking
(PD-9 ratifies); the GlyphProvider ladder and tofu plan (MR-06); the MR ticket decomposition and
its owner lanes; golden governance and mask policy.

---

# Appendix B — The TXP expression corpus (C-01…C-64)

Machine-readable duplicate: `tools/tex-parity-oracle/corpus.json` (TXP-0 keeps them in sync; the
JSON is generated from this table by hand once and diffed thereafter). Columns: **Build** = VPAM
construction — either a named existing fixture builder [V from
`src/math/MathRenderVisualCases.cpp:157-179` (21 cases) / `src/math/MathStressExpressions.cpp:220-238`
(17 cases)] or a factory recipe using `MathAST.h` factories (`makeRow/makeNumber/makeOperator/
makeFraction/makePower/makeRoot/makeParen/makeFunction/makeLogBase/makeConstant/makeVariable/
makeRelation/makeSummation/makeBigOp/makeDefIntegral/makeSubscript`, `MathAST.h:1343-1406` [V]).
**Style** D = DISPLAY_STYLE, T = TEXT. **Features** = oracle feature-key groups asserted
(§TXP-0 0.5.4). Key sequences are not used for corpus construction (trees are built
programmatically in the host harness and via the MathShowcase page path in the emulator
[V showcase pattern `NativeHal` MathShowcase id 100, ground truth §D]).

### B.1 Numbers, operators, relations, spacing (C-01…C-16)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-01 | `1+2` | Row[num 1, op Add, num 2] | D | width, spacing.* | Ord-Bin-Ord medium |
| C-02 | `3.14` | num "3.14" | D | width | decimal-point advance (TXP-3) |
| C-03 | `12345` | num "12345" | D | width | tabular digit run |
| C-04 | `1+2\times 3` | Row[1,Add,2,Mul,3] | D | spacing.* | two Bin gaps |
| C-05 | `x=y` | Row[var x, rel Eq, var y] | D | spacing.* | thick Rel space |
| C-06 | `x\le y\ne z` | Row[x,Le,y,Ne,z] | D | spacing.* | Rel chains (Rel-Rel = 0 via Ord operands) |
| C-07 | `\left(1+2\right)` | makeParen(Row[1,Add,2]) | D | delim.height, delim.inner.dx, spacing.* | Open/Close spacing |
| C-08 | `\left\lvert x\right\rvert` | makeBar(Row[x]) | D | delim.height | Bar delimiter |
| C-09 | `-3` | Row[op Sub, num 3] | D | spacing.0 | leading-Bin demotion (D1) |
| C-10 | `x=-3` | Row[x, Eq, Sub, 3] | D | spacing.* | Rel→demoted-Bin thick space |
| C-11 | `\left(-x\right)` | makeParen(Row[Sub, x]) | D | spacing.* | Open→Bin demotion |
| C-12 | `x+\sum_{k=1}^{n}k` | Row[x, Add, summation] | T | spacing.* | Bin→Op medium (OQ-4); inline Σ |
| C-13 | `x^{a+b}` | makePower(x, Row[a,Add,b]) | D | spacing.* (exp row), sup.baseline | script-style suppression (D2 of §H.5 conditionals) |
| C-14 | `\sin(x)+1` | Row[makeFunction(Sin,x), Add, 1] | D | width, spacing.* | function label metrics |
| C-15 | `2\times\left(3+4\right)` | Row[2, Mul, paren] | D | spacing.* | Bin→Open medium |
| C-16 | `1+\cfrac{2}{3}+x^2` | fixture `mixed_row_fraction_power` [V] | T | spacing.*, frac.*, sup.baseline | Inner-class spacing |

### B.2 Text metrics and identifiers (C-17…C-20)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-17 | `x` | var x | D | width | italic mapping (PD-3) |
| C-18 | `xyz` | Row[x,y,z] | D | width, spacing.* | Ord-Ord juxtaposition, italic run |
| C-19 | `\mathrm{Ans}+1` | Row[var '#', Add, 1] | D | width | word chip stays upright |
| C-20 | `\sin(x)+3.5` | Row[func, Add, num "3.5"] | D | width | label + decimal advances |

### B.3 Scripts (C-21…C-30)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-21 | `x^2` | fixture `power_x_squared` [V] | T | sup.baseline, sup.dx | glyph base, δ(x italic) |
| C-22 | `2^2` | fixture `power_2_squared` [V] | T | sup.baseline | digit base, δ=0 |
| C-23 | `\left(x+1\right)^2` | fixture `power_group_x_plus_1_squared` [V] | T | sup.baseline | boxed-base σ18 drop |
| C-24 | `2^{2^2}` | makePower(2, makePower(2,2)) | D | sup.baseline (outer+inner) | BottomMin floor, level-2 fonts |
| C-25 | `x_1` | makeSubscript(x, 1) | D | sub.baseline | σ16, TopMax |
| C-26 | `\log_2(8)` | makeLogBase(2, 8) | D | sub.baseline, width | LogBase drop |
| C-27 | `\cfrac{1}{2}+x^2` | fixture `fraction_plus_power` [V] | T | frac.*, sup.baseline | also GAP-51 highlight item |
| C-28 | `\cfrac{x^2}{y^2}` | makeFraction(Row[pow], Row[pow]) | D | sup.baseline in den | cramped denominator (PD-6) |
| C-29 | `\sqrt{x^2}` | makeRoot(Row[pow x 2]) | D | sup.baseline, radical.* | cramped radicand |
| C-30 | `f^2` | makePower(var f, 2) | T | sup.dx | δ(f) — largest Latin ic |

### B.4 Fractions and bars (C-31…C-38)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-31 | `\dfrac{1}{2}` | makeFraction(1, 2) | D | frac.bar.y, frac.num/den.baseline, frac.*.gap | display σ8/σ11 vs gap minima |
| C-32 | `\tfrac{1}{2}`-placement at full size: `\cfrac{1}{2}` | same tree | T | frac.* | text constants (585/68) with PD-4 size |
| C-33 | `\cfrac{1+\cfrac{1}{2}}{x+3}` | fixture `nested_fraction` [V] | T | frac.* nested | tall-numerator gap branch |
| C-34 | 4-level `\cfrac` tower | fixture `four_level_fraction` [V stress] | D | frac.* ×4 | PD-4 constant size + placement |
| C-35 | `\cfrac{\;\cfrac{1}{2}\;}{\;\cfrac{3}{4}\;}` | fixture `nested_fraction_quarters` [V] | T | frac.* | fraction-in-fraction |
| C-36 | `2+\cfrac{2}{2}` | fixture `photo_2_plus_2_over_2` [V] | T | frac.*, spacing.* | the photo-calibration case |
| C-37 | `0.1\overline{6}` | makePeriodicDecimal("0","1","6") | D | overline.rule.y | Overbar constants (GAP-31) |
| C-38 | `\cfrac{x^2}{3}` | fixture `fraction_powered_numerator` [V] | T | frac.*, sup.baseline | ink-based num gap |

### B.5 Radicals (C-39…C-44)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-39 | `\sqrt{2}` | makeRoot(2) | D | radical.rule.y, radical.gap | display ψ (170 DU) |
| C-40 | `\sqrt{x^2+1}` | fixture `root_quadratic` [V] | D | radical.* | radicand with ink ascent |
| C-41 | `\sqrt[3]{x}` | makeRoot(x, degree 3) | D | degree.baseline | 55 % raise, kerns |
| C-42 | `\sqrt[3]{\cfrac{1}{1+x}}` | makeRoot(fraction, degree 3) | D | degree.baseline | deep-radicand raise (GAP-33 divergence case) |
| C-43 | nested `\sqrt{2+\sqrt{2+\sqrt{2}}}` | fixture `nested_radicals` [V stress] | D | radical.* ×3 | composition |
| C-44 | `1+\sqrt{2}+x` | fixture `root_next_to_text` [V] | D | spacing.*, radical.* | Inner spacing around radical |

### B.6 Big operators (C-45…C-52)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-45 | `\sum_{n=1}^{10}\left(n^2+1\right)` | fixture `summation_limits` [V] | D | op.height, limit.upper/lower.baseline+gap | display variant + stacked limits (GAP-36 headline) |
| C-46 | same | fixture `summation_text` [V stress] | T | sub/sup on op | inline gate |
| C-47 | `\prod_{i=1}^{n}x` | makeBigOp(Product,…) | D | op.height, limit.* | unified gate (GAP-37) |
| C-48 | `\int_{0}^{1}x^2+1\,dx` | fixture-equivalent via makeDefIntegral | D | op.height, limit.*, width | display ∫ + differential tail |
| C-49 | `\bigcup_{i}x_i`-class: `⋃` inline | fixture `union_text` [V stress] | T | sub/sup on op | non-arith big op |
| C-50 | `\int_{0}^{1}\cfrac{1}{1+x}\,dx` | makeDefIntegral(0,1,fraction,x) | D | op.height | tall integrand → ∫ variant/assembly |
| C-51 | `\cfrac{\sum_{k=1}^{n}k}{2}` | makeFraction(Row[summation], 2) | D | op.height in num | gate OFF inside TEXT fraction parts |
| C-52 | `\sum_{k=1}^{\;}k` (empty upper) | makeSummation(lower "k=1", empty, k) | D | limit.lower.* | degenerate empty limit (§H.9.5) |

### B.7 Delimiters (C-53…C-58)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-53 | `\left(x\right)` | makeParen(x) | D | delim.height | base-glyph floor |
| C-54 | `\left(\cfrac{1}{2}\right)` | makeParen(fraction) | D | delim.height, delim.inner.dx | Rule-19 undershoot signature |
| C-55 | `\left[\cfrac{x}{y}\right]` | makeBracket(fraction) | D | delim.height | bracket ladder |
| C-56 | `\left\{\cfrac{x}{y}\right\}` | makeBrace(fraction) | D | delim.height | brace assembly (5 pieces) |
| C-57 | `\left\lvert\cfrac{x}{y}\right\rvert` | makeBar(fraction) | D | delim.height | bar extender chain |
| C-58 | bracket/brace matrix fake | fixture `elastic_matrix_brackets` [V stress] | D | delim.height ×n | multi-delimiter screen |

### B.8 Script kerns (C-59…C-62)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-59 | `V^2` | makePower(var V, 2) | T | sup.dx | TopRight cut-in of italic V |
| C-60 | `V_2` | makeSubscript(var V, 2) | T | sub.dx | BottomRight cut-in |
| C-61 | `f_x` | makeSubscript(var f, var x) | T | sub.dx | deep-descender base corner |
| C-62 | `\int_0^1 x\,dx` (inline) | makeDefIntegral in T style | T | sup.dx, sub.dx | δ split + kerns on ∫ (GAP-04) |

### B.9 Composed stress (C-63…C-64)

| ID | LaTeX | Build | Style | Features | Exercises |
|---|---|---|---|---|---|
| C-63 | quadratic formula `x=\cfrac{-b\pm\sqrt{b^2-4ac}}{2a}` | fixture `quadratic_formula` [V stress] | D | full feature sweep | everything at once; the report's cover image |
| C-64 | Gauss law `\oint...`-class display composite | fixture `gauss_law_display` [V stress] | D | full sweep | wide composite + scroll interaction (PD-9 unwrapped comparison) |

Corpus maintenance rules: IDs are append-only; a phase needing a new probe adds C-65+ with its
feature keys registered in §TXP-0 0.5.4's table (spec edit in the same PR); LaTeX strings are the
reference contract — changing one is a re-baseline event equivalent to a golden re-bless
(human-reviewed).

---

# Appendix C — Exact `.numos` script listings

Normative listings for every script named in Part II. `<k>` placeholders are the corpus item's
`showcaseIndex` from `corpus.json` (TXP-0 fixes them; the committed scripts carry literal
numbers). Every script runs with
`--headless --deterministic --frames 1400 --quiet` and the standard exit-code contract
(0 ok · 2 parse · 3 screenshot · 4 assert). Comment headers follow the repo style
[V `math_showcase_delims_smoke.numos`].

### C.1 `txp0_corpus_pages.numos` (TXP-0)

```text
# txp0_corpus_pages.numos — TXP-0. Cycle every MathShowcase page (existing 21 visual
# cases + appended TXP corpus fixtures) to prove each corpus item lays out and draws
# without crashing. No screenshots, no golden — assert-only smoke.
log "txp0 corpus pages: start"
wait 200
open_app MathShowcase
wait 60
# one RIGHT per page; N = total case count after the TXP-0 append (literal repeat in
# the committed file — the .numos grammar has no loops)
key right
wait 5
# … repeated N−1 times …
assert_app MathShowcase
assert_no_error
log "txp0 corpus pages: done"
```

### C.2 Showcase-capture template (all waves)

Concrete instances: `txp2_spacing_demotion` (item C-10), `txp3_identifiers` (C-18),
`txp4_scripts` (C-23), `txp5_fractions` (C-31), `txp6_radicals` (C-42), `txp7_bigops` (C-45),
`txp8_delimiters` (C-54), `txp9_kerns` (C-59). One file per stem, differing only in `<k>` and
the stem name:

```text
# txp<n>_<name>.numos — TXP-<n> capture of corpus item <C-id> (<latex>).
# Candidate stem: txp<n>_<name>. Golden after human bless; mask: clock rect only.
log "txp<n> <name>: start"
wait 200
open_app MathShowcase
wait 60
key right
# … repeated <k> times total …
wait 20
screenshot out/txp<n>_<name>.ppm
assert_app MathShowcase
assert_no_error
log "txp<n> <name>: captured"
```

### C.3 Typed-edit cursor scripts (REN-11, one per wave)

`txp2_cursor_row` — cursor after a demoted leading minus:

```text
# txp2_cursor_row.numos — TXP-2 REN-11 cursor-mirror check. Types "-3", captures the
# edit screen with the cursor at row end; the cursor x must sit flush after the "3"
# with the demoted-Bin spacing (0 px before "3"), proving Finder==layout.
log "txp2 cursor row: start"
wait 200
open_app Calculation
wait 60
key -
key 3
wait 20
screenshot out/txp2_cursor_row.ppm
assert_no_error
log "txp2 cursor row: captured"
```

`txp3_cursor_text` — cursor inside a decimal number:

```text
log "txp3 cursor text: start"
wait 200
open_app Calculation
wait 60
key 3
key .
key 1
key 4
key LEFT
key LEFT
wait 20
screenshot out/txp3_cursor_text.ppm
assert_no_error
log "txp3 cursor text: captured"
```

`txp4_cursor_scripts` — cursor in an exponent (POW captures the base, cursor jumps into the
exponent [V CursorController contract `CursorController.h:128-133`]):

```text
log "txp4 cursor scripts: start"
wait 200
open_app Calculation
wait 60
key 2
key ^
key 3
wait 20
screenshot out/txp4_cursor_scripts.ppm
key RIGHT
key +
key 1
key =
wait 120
assert_result 9
assert_no_error
log "txp4 cursor scripts: done"
```

`txp5_cursor_fraction` — cursor in the denominator (DIV captures the left operand into the
numerator, cursor lands in the denominator [V `CursorController.h:119-125`]):

```text
log "txp5 cursor fraction: start"
wait 200
open_app Calculation
wait 60
key 1
key /
key 2
wait 20
screenshot out/txp5_cursor_fraction.ppm
key RIGHT
key +
key 1
key /
key 3
key =
wait 120
assert_result 5/6
assert_no_error
log "txp5 cursor fraction: done"
```

`txp6_cursor_radical` — cursor in a radicand:

```text
log "txp6 cursor radical: start"
wait 200
open_app Calculation
wait 60
key sqrt
key 2
wait 20
screenshot out/txp6_cursor_radical.ppm
assert_no_error
log "txp6 cursor radical: captured"
```

(Key names verified against `scriptNameToKeyCode` [V `src/hal/NativeHal.cpp:457-540`]: symbols
`+ - * / ^ . ( ) =`, digits, `sqrt`, `left/right/up/down`, `alpha`, `pi`, `x`, `enter` are all
valid script names; `assert_result` takes the bare computed string, no quotes [V
`calc_fraction_sum.numos`].)

`txp7_cursor_limits`, `txp8_cursor_paren`, `txp9_cursor_kerns` follow the same shape: build the
smallest expression whose cursor sits inside the wave's changed construct (a summation lower
bound via the showcase edit path if no keypad key exists — in that case the script degrades to a
showcase capture plus a Calculation-app script exercising the nearest typable construct, and the
PR says which; for TXP-8: `key (` `key 1` `key /` `key 2` `key RIGHT` cursor-after-paren; for
TXP-9: `key ALPHA`-layer letter then `key ^` per the KeyboardManager ALPHA modifier flow).

### C.4 Semantic-regression rider (every wave)

Every wave PR must also re-run the existing Phase 4B/8C calc suites unmodified
(`calc_semantic_*` [V 8 scripts, `emulator-build.yml:308-356`]) — geometry waves must never
change *results*, only pixels. This comes free in CI; the rule here is that the agent runs them
locally **before** regenerating candidates, so a semantic break is caught before any bless
request.

---

*End of NUMOS_TEX_PARITY_TYPESETTING_SPEC. Program code TXP. Questions that arise during
implementation that this document does not answer are, by definition, spec bugs: file them
against this document, take the nearest §D default or PD precedent, and record the choice in the
implementing PR.*
