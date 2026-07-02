# NumOS — Math Layout Engine and Type-System Specification

> **Normative layout spec** for MathRenderer v2. Defines the box model, style system, per-construct
> layout rules, glyph pipeline, viewport behavior, cursor geometry, and caching, with pseudocode.
> Verified against `main @ 2d6b796` (2026-07-02); source claims cite `file:line`.
> Markers: **[V]** verified current behavior (contract — must not silently change; geometry changes
> require golden re-bless per REN-1), **[P]** proposed.
> Companions: architecture spec (layering, invariants Y-1…Y-9), notation spec (node inventory),
> input/serialization spec, test plan, tickets (MR-05/06/11/12/13/17 implement this doc).

---

## 1. Layout model overview

Two-pass immediate-mode model, unchanged in v2:

1. **Measure** (bottom-up): `calculateLayout(FontMetrics)` computes per node
   `LayoutResult{width, ascent, descent, inkAscent, inkDescent}` (`MathAST.h:447-459`) and caches
   it in the node (`MathAST.h:646`). v2 wraps this in memoization (`layoutIfNeeded`, architecture
   spec §2.6) and a `LayoutBudget` (depth ≤24, nodes ≤4096, invariants Y-3/Y-4).
2. **Place + draw** (top-down): `MathCanvas::onDraw` positions the root
   (`baseX = x1 + PADDING_LEFT + scrollX`; root baseline vertically centered,
   `MathRenderer.cpp:741-744` [V]) and recursively draws by baseline
   (`drawRowBaseline`/`drawXxxBaseline`, `MathRenderer.h:279-363`). Placement consumes cached
   `LayoutResult` and **must not re-measure** (`MathRenderer.h:265-266` [V]).

All measurement is integer pixels (`int16_t`), derived from OpenType MATH design units via
`MathConstantsProvider::scalePx` (round-to-nearest, `MathTypography.h:274-279` [V]) with
`scalePxMin` guards for rules that must not collapse to 0 px (`:284-288` [V]). v2 adds saturating
adds for width/ascent accumulation (failure model, arch spec §5).

## 2. Style system

Four styles [V] (`MathStyle`, `MathTypography.h:45-50`): `DISPLAY_STYLE, TEXT, SCRIPT,
SCRIPTSCRIPT`, bound to three physical fonts 18/12/8 px (`MathRenderer.h:159-161`). Style
transitions today, kept in v2:

| Context | Transition | Evidence |
|---|---|---|
| Superscript/subscript/limits/root degree | `fm.superscript()`: L0→prebaked 12 px metrics; L1→L2 by ScriptScriptPercentScaleDown=55%; clamped at L2 | `MathAST.h:249-290` [V] |
| Fraction parts | **NumOS readable policy**: L0 stays full size but switches DISPLAY→TEXT (tighter shift constants); only L≥1 steps down | `fractionPartMetrics`, `MathAST.h:314-326` [V] |
| Big-op limit placement | display limits only when style is DISPLAY **and** operator glyph ≥ DisplayOperatorMinHeight | `shouldUseDisplayLimits`, `MathAST.h:410-415` [V] |

v2 additions [P]:
- **`forceDisplayStyle` node flag** (TeX `\displaystyle`): a 1-bit attribute on
  `NodeSummation/NodeBigOp/NodeDefIntegral/NodeLimit` that pins their *own* style to DISPLAY even
  inside TEXT/fraction contexts. Needed by exemplar B (display Σ inside a fraction numerator,
  notation spec §2). Set by templates; never implicit.
- **No cramped styles** (decision, see §14): superscript placement uses the uncramped
  `SuperscriptShiftUp` everywhere; `SuperscriptShiftUpCramped` remains extracted
  (`MathTypography.h:346-348` [V]) but intentionally unused.

## 3. Box model and node/box vocabulary

v2 keeps "node = box": the AST node *is* the layout box, with `LayoutResult` as its box record.
The following box vocabulary is normative — new constructs must map to exactly one of these
behaviors (names ↔ implementing node):

| Box type | Node(s) | Measure contract |
|---|---|---|
| **RowBox** | `NodeRow` [V] | width = Σ child widths + inter-atom spacing; ascent/descent = max over children (`MathAST.cpp:171-217`) |
| **SymbolBox** | `NodeNumber/NodeOperator/NodeConstant/NodeVariable/NodeIdentifier[P]` | glyph-run advance; ink from probed glyph boxes |
| **FractionBox** | `NodeFraction` [V] | §6 |
| **ScriptBox** | `NodePower/NodeSubscript` [V], `NodeScripts` [P] | §5 |
| **LargeOpBox** | `NodeSummation/NodeBigOp/NodeDefIntegral` [V], `NodeLimit` [P] | §8 |
| **DelimitedBox** | `NodeParen` (all `DelimKind`) [V] | §10 |
| **RadicalBox** | `NodeRoot` [V] | §7.2 |
| **MatrixBox** | `NodeMatrix` [P] | §9 |
| **StackBox** | `NodeBinom` [P] (parenthesized stack, no rule) | §6.3 |
| **EllipsisBox** | `NodeEllipsis` [P] | single-glyph SymbolBox with class ORD (Cdots/Ddots) / PUNCT (Ldots) |
| **PostfixBox** | `NodeFactorial` [P] | base box + `!` glyph advance; class: base-left, CLOSE-right (§4) |

**ContinuedFractionBox and IntegralBox are deliberately not distinct types**: continued fractions
are FractionBoxes (policy §7.1), integrals are LargeOpBoxes with a differential tail [V]
(`integralDifferentialGapPx`, `MathAST.h:381-384`).

## 4. Baseline, spacing, and operator classes

**Baseline contract** [V] (invariant Y-2): `ascent`/`descent` are positive distances from the
baseline; `height = ascent + descent`; row children align baselines
(`rowChildTopFromRowTop`, `MathAST.h:533-538`); conversions `layoutBaselineFromTop` /
`layoutTopFromBaseline` (`MathAST.h:521-530`). The **math axis** (fraction bars, ± centering,
delimiter symmetry) sits at `axisHeight()` above baseline (AxisHeight=258 DU ≈ 5 px @18,
`MathAST.h:231-234` [V]).

**Ink vs line metrics** [V]: `FontMetrics` carries both visual ink (`ascent/descent`,
`numberAscent/Descent`) and the LVGL line box (`lineAscent/lineDescent`) — layout uses ink,
text drawing converts via line box (`MathAST.h:208-291`). New nodes must follow this split.

**Horizontal spacing** [V]: TeX 8-class system — `MathClass{ORD,OP,BINARY,REL,OPEN,CLOSE,PUNCT,
INNER}` (`MathTypography.h:70-80`), 8×8 table verified against LyX (`:96-106`), thin/med/thick =
3/4/5 mu, 18 mu = 1 em (`:133-159`), applied once in `NodeRow::calculateLayout` and mirrored by
cursor geometry (single DRY path, `interAtomSpacingPx`, `:166-176`). Class assignments per node:
`mathClass()/leftMathClass()/rightMathClass()` virtuals (`MathAST.h:607-616` [V]) — e.g. Paren is
OPEN-left/CLOSE-right, Fraction is INNER, big ops are OP.

New-node class table [P]:

| Node | left class | right class |
|---|---|---|
| `NodeIdentifier` | ORD | ORD |
| `NodeFactorial` | (base's left class) | CLOSE — makes `n! + 1` space like `) + 1`, matching TeX's treatment of `!` as closing-ish ORD; chosen over ORD to prevent `2!3` reading as tight juxtaposition |
| `NodeLimit` | OP | OP |
| `NodeMatrix` | OPEN | CLOSE |
| `NodeBinom` | OPEN | CLOSE |
| `NodeEllipsis(Cdots/Ddots/Vdots)` | ORD | ORD |
| `NodeEllipsis(Ldots)` | PUNCT | PUNCT |
| `NodeScripts` | INNER | INNER |

**Vertical spacing**: exclusively MATH-constant driven (gaps/shifts per construct, §§5-9); the
only global vertical constant is widget padding `VPAM_VERT_PAD=6` (`MathRenderer.h:150` [V]).

## 5. Script placement (sub/superscript, combined)

Superscript [V]: shift = `max(SuperscriptShiftUp, SuperscriptBottomMin + exp.descent)` with
optional signed policy adjust (default 0) — `superscriptShiftMetrics`, `MathAST.h:484-513`;
italic correction added for italic single-glyph bases (`MathAST.cpp:482-506`); `SpaceAfterScript`
appended. Subscript [V]: `SubscriptShiftDown` with 2 px min (`MathAST.cpp:1395`).

`NodeScripts` (combined) [P] — OpenType §4.3 reduced form:

```
supShift = max(SuperscriptShiftUp, SuperscriptBottomMin + sup.descent)
subShift = max(SubscriptShiftDown, SubscriptBaselineDropMin_rule)
gap      = (supBaseline - sup.descent) - (subBaseline top edge)   // vertical ink gap
if gap < SubSuperscriptGapMin:
    delta = SubSuperscriptGapMin - gap
    # TeX Rule 18e reduced: push subscript down first, then lift superscript
    subShift += min(delta, superscriptBottomMaxWithSubscript_slack)
    supShift += remaining(delta)
width = base.w + italicCorr + max(sup.w, sub.w) + SpaceAfterScript
```

All constants already extracted [V] (`MathTypography.h:334-363`). **No cramped variants** (§2).

## 6. Fraction, stack, binomial layout

### 6.1 Fraction [V] (contract)

Bar on the math axis; **gap-derived shifts** rather than raw TeX shift constants: symmetric gap
`G = max(FractionNumDisplayStyleGapMin, FractionDenomDisplayStyleGapMin)` (=150 DU ≈ 3 px @18);
`numShift = G + num.inkDescent`, `denShift = G + den.inkAscent`; rule thickness
FractionRuleThickness min 1 px; overhang `max(muToPx(1),1)` per side
(`fractionBarGaps`, `MathAST.h:549-588`; `NodeFraction::calculateLayout`, `MathAST.cpp:372-425`).
This is a documented deliberate TeX departure for 320×240 legibility (`MathAST.h:560-566`) — v2
keeps it.

### 6.2 Ascent/descent [V]: `ascent = axis + barHalfUp + numShift + num.ascent`… (existing code
is the contract; the host geometry harness (test plan §3) freezes these numbers).

### 6.3 StackBox / `NodeBinom` [P]

Binomial = two rows stacked about the axis with **no rule**, wrapped in stretchy parens:

```
gap    = StackGapMin(display)                        # MATH constants, extracted [V] :390-393
upShift   = max(StackTopShiftUp(display),   axis + gap/2 + top.inkDescent)
downShift = max(StackBottomShiftDown(display), gap/2 - axis + bottom.inkAscent)
inner.w = max(top.w, bottom.w); center both rows
then wrap: DelimitedBox(Paren) around the stack     # reuses §10 wholesale
```

Parts use `fractionPartMetrics`-style sizing (full size at L0) for legibility — same policy
rationale as fractions.

## 7. Continued fractions and radicals

### 7.1 Continued fraction policy [P — policy formalization of V behavior]

A "continued fraction" is plain `NodeFraction` nesting. Because `fractionPartMetrics` keeps L0
parts at full glyph size (`MathAST.h:314-326` [V]), a cfrac tower renders at constant size like
LaTeX `\cfrac` — **no shrinking cascade exists to fight**. v2 adds only:

- **Left-alignment attribute is NOT added**: NumOS centers numerators (TeX \cfrac centers too by
  default); acceptable.
- Depth: each cfrac level costs 2 boxed levels (Fraction+Row); budget 24 ⇒ ≥8 visible levels —
  exemplar B needs 3 + ellipsis. No cap change needed beyond Y-3.
- Termination: `NodeEllipsis(Ddots)` as the innermost denominator content (notation spec §P).

### 7.2 Radical [V] (contract)

Vector-drawn hook/slope (`RADICAL_HOOK_W=3, RADICAL_SLOPE_W=6`, `MathAST.h:881-883`) + MATH-driven
overline (`radicalVerticalGap` display/text, `radicalRuleThickness`, `radicalExtraAscender`), degree
raised by RadicalDegreeBottomRaisePercent=55% with kern-before/after (65/−335 DU)
(`MathAST.cpp:563-617`). Nested radicals compose without special cases [V]
(`nested_radicals` stress case). v2 change [P]: when MR-04 adds radical size variants/assembly
glyphs (U+221A variants), `GlyphProvider` may replace the vector radical for tall radicands —
geometry-changing, gated behind MR-11 re-bless; the vector path remains the fallback forever
(fonts lacking extenders, §12).

## 8. Large operators and limits

### 8.1 Size selection [V]

Target height = `max(fm.height(), DisplayOperatorMinHeight)` in display style
(`largeOperatorTargetHeightPx`, `MathAST.h:358-363`); glyph chosen by
`DelimiterAssembler::glyphMetricsForHeightPx` over the operator's variant table
(∑∏∐⋀⋁⋂⋃∫ tables exist [V], `stix_math_variants.h:367-386`); operator centered on the math axis
(`largeOperatorGlyphAscentPx`, `MathAST.h:365-374`).

### 8.2 Limit placement [V]

`shouldUseDisplayLimits(fm, opHeight)` — display style AND glyph ≥ DisplayOperatorMinHeight ⇒
limits stacked above/below (centered, `UpperLimitGapMin/LowerLimitGapMin` +
`upper/lowerLimitBaselineRise/DropMin`); otherwise inline sub/superscript to the right
(`displayLimitGeometry`/`inlineLimitGeometry`, `MathRenderer.cpp:296-347`).

### 8.3 Pseudocode (normative shape, matches current code + v2 flag)

```
layout_largeop(node, fm):
    style   = node.forceDisplayStyle ? DISPLAY : fm.style          # [P] flag, §2
    opH     = glyphMetricsForHeightPx(varTable(node.cp), targetHeight(fm,style)).height
    display = (style==DISPLAY) and opH >= DisplayOperatorMinHeight
    limFm   = fm.superscript()
    lower.layout(limFm); upper.layout(limFm); body.layout(fm)
    if display:
        w_op   = max(opW, lower.w, upper.w)                         # limits centered on op
        ascent = axis + ceil(opH/2) + UpperLimitGapMin + upper.h + riseAdj
        descent= opH - ascent' + LowerLimitGapMin + lower.h + dropAdj
        width  = w_op + bodyGap(3mu) + body.w        (+ dGap(2mu) + "d"+var for integral [V])
    else:
        width  = opW + scripts(lower→sub, upper→sup per §5) + bodyGap + body.w
    baseline = row baseline; op centered on axis
```

### 8.4 `NodeLimit` [P]

"lim" rendered as an upright word (three SymbolBox glyphs); under-row (`n→∞`) placed with the
**under-script** rules (LowerLimitGapMin/LowerLimitBaselineDropMin) in display style, or as a right
subscript inline — i.e., a LargeOpBox whose "operator glyph" is a text word and which has no upper
slot. Class OP both sides. Body = the bracketed expression that follows in the row (limit does not
own a body child — it is a prefix operator box; semantic binding happens at the semantic boundary,
input spec §5.3). This keeps editing simple (cursor enters only the under-row).

## 9. Matrix layout (`NodeMatrix`) [P]

Cells are NodeRow slots, row-major; dims ≤ 6×6 (notation spec §M).

```
layout_matrix(m, fm):
    cellFm = (fm.scriptLevel==0) ? fm : fm.superscript()      # full size at top level,
                                                              # readable-policy analog
    for cell in cells: cell.layoutIfNeeded(cellFm)
    colW[j] = max over rows of cell(i,j).width
    rowAsc[i] = max ascent in row i; rowDesc[i] = max descent in row i
    colGap = muToPx(12)   # 12 mu ≈ 2/3 em — quad-like TeX array column sep, scaled by em
    rowGap = max(StackGapMin(display), 2*RuleThickness + 2)   # vertical baseline separation pad
    innerW = Σ colW + (cols-1)*colGap
    innerH = Σ (rowAsc+rowDesc) + (rows-1)*rowGap
    # vertical centering on math axis (TeX \vcenter):
    ascent  = innerH/2 + axisHeight(fm)
    descent = innerH - ascent
    # fence: reuse DelimitedBox growth (§10) with content ink = (ascent, descent)
    width = fenceW*2 + innerPad*2 + innerW
    # per-cell placement: row i baseline; column j center-aligned within colW[j]
```

Decisions: **column centering** (not decimal-alignment — F), **axis centering** vertically
(matches how `(A)^T`, `det` rows sit in TeX), fence via the existing delimiter machinery so
determinant bars and pmatrix parens are the same code path with a different `DelimKind`.
Budget note: a 6×6 matrix = 36 rows + cells ≈ 80–150 nodes — well under Y-4's 4096.

## 10. Stretchy delimiters and bracket growth

Sizing [V]: delimiter target height from content ink with symmetric-about-axis geometry
(`symmetricAxisDelimiterGeometry`, `MathAST.cpp` helpers; `NodeParen::calculateLayout`,
`MathAST.cpp:659-673`), minimum growth per DelimitedSubFormulaMinHeight available
(`MathTypography.h:323-325` [V]).

Resolution ladder (GlyphProvider [P] — consolidates `drawDelimiterGlyph`, `MathRenderer.cpp:184-283` [V]):

```
plan_delimiter(cp, targetH, font):
    1 base glyph if tall enough                       # today: :193-211
    2 largest size variant from MathVariantTable      # tables [V] stix_math_variants.h
    3 assembly: validateAndSplit FSM (≤3 fixed pieces + 1 extender, Gecko constraints
      [V] MathGlyphAssembly.cpp:30-86); inject extenders to reach targetH; distribute
      overlap evenly across seams [V] :236-253
    4 if any assembly piece missing from font subset → vector synthesis
      (drawStrokedDelimiter [V] :87-172) — REQUIRED PERMANENT PATH, see §12
    5 tofu box + [LVGL] glyph-missing log [P]         # replaces silent nothing (C5.7)
```

Delimiter-sizing pseudocode (normative):

```
delimiter_target(content):
    inkUp   = content.inkAscent  - axisHeight
    inkDown = content.inkDescent + axisHeight
    halfH   = max(inkUp, inkDown) + delimiterVerticalPad
    return 2*halfH                                   # symmetric about axis
```

## 11. Glyph assembly policy / large-glyph selection

- `duToPxCeil` for heights (never undershoot target), `duToPxRound` for widths [V]
  (`MathGlyphAssembly.cpp:20-28`) — keep.
- Assembly enabled only when the font actually contains extenders: runtime probe of U+239C sets
  `g_delimiterAssemblyRenderable` [V] (`MathRenderer.cpp:431-435`) — generalized in v2 to a
  per-table availability check inside `planGlyph` (probe each piece once, cache in a static
  bitmask; zero heap).
- After MR-04 regenerates fonts **with** U+239B…U+23B3 (+ brace/bar parts + ∫/√ variants), rungs
  3 takes over from 4 for parens/brackets — a geometry change batched into MR-11 with golden
  re-bless (REN-1 protocol).

## 12. Fonts lacking extenders / fallback vector policy

Vector synthesis is a **permanent architectural tier**, not a temporary hack: any future font swap,
subset regression, or flash-budget cut must degrade to strokes, never to nothing (invariant Y-8).
Vector-synthesizable set: `( ) [ ] { } | √ ± ⌊⌋⌈⌉-as-brackets`. Everything else (Greek, ∑, ∫, ⋱ …)
is **not** synthesizable ⇒ tofu rung. Stroke weight derives from em size [V]
(`MathRenderer.cpp:87-172`); v2 keeps stroke geometry bit-identical (golden-frozen).

## 13. Nesting, clipping, scrolling, viewport

- Depth: layout budget 24 / editor cap 16 / node budget 4096 (arch spec Y-3/Y-4, REL H-5, RT-10
  alignment). Breach renders a fixed ⚠ box (never partial layout).
- Width: horizontal scroll [V] (`_scrollX ≤ 0`, auto-follow cursor, `MathRenderer.cpp:762-782`).
- Height [P, MR-17]: `setViewport(maxHeightPx)`; when `layout.height > viewport`, MathCanvas pans
  vertically (`_scrollY`), cursor-follow symmetric to X; "⋮ more" affordance chips at clipped
  edges. Auto-height keeps growing the widget until the app-set viewport cap
  (today auto-height grows unbounded and never shrinks, `MathRenderer.cpp:593-604` [V] — v2 also
  fixes shrink on smaller expressions, geometry-neutral for goldens since goldens' content fits).
- Whole-expression storage/testing is viewport-independent via NumIR (arch spec Q14).

## 14. TeX parity: what we copy, what we deliberately don't (Special Question 12)

**Adopt (already built, keep)**: MATH-constant metrics for every construct; 8-class spacing table;
4-style lattice with script percent scale-downs; display-vs-inline limits per
DisplayOperatorMinHeight; italic correction; extensible glyph assembly.

**Deliberately reject** (cost/benefit, each with rationale):

| TeX mechanism | Verdict | Why |
|---|---|---|
| Cramped styles (′ variants) | reject | affects only superscript height under bars/radicals by ~1 px at 18 px em; doubles the style lattice; invisible on a 320×240 TFT |
| \mathchoice / auto display↔text switching mid-formula | reject | replaced by the single `forceDisplayStyle` bit (§2); full lattice switching is what makes TeX layout non-cacheable |
| Math kerning (staircase cut-ins, OpenType MathKernInfo) | reject | subpixel-level refinement; not extracted from the font; italic correction already covers the visible cases |
| Glue/penalty line breaking of formulas | reject | calculator UX uses scrolling (X [V], Y [P]); breaking formulas across lines needs a paragraph model NumOS doesn't have; revisit only for a future "document" view |
| TEXT→SCRIPT fraction demotion | **already rejected** [V] | readability on hardware (`MathAST.h:310-312`) — this is the documented NumOS house style |
| Exact TeX Rule 18 sub/sup interlock | simplify | reduced form in §5 keeps the two constants that matter (GapMin, BottomMaxWithSubscript); the full rule tree adds unmeasurable differences at 18 px |

**Where exactness matters** (and is kept): axis alignment of bars/± /delimiters; script shift
minimums (legibility floor); limit gaps; radical gaps; rule thickness minimums (`scalePxMin` ≥1 px);
inter-atom spacing (reads as "professional" even at 18 px).

## 15. 2D cursor navigation, hit-testing, selection

Existing model [V]: cursor = `{NodeRow*, index}` between siblings (`CursorController.h:62-85`);
`moveLeft/Right` with structure entry/exit, `moveUp/Down` via `verticalSibling` slot mapping
(`CursorController.h:179-256`); geometry computed by the renderer along the same measured offsets
(`childXOffset`, `MathRenderer.h:392`; cursor clamped to widget, `MathRenderer.cpp:1243-1268`).

v2 extensions [P]:

- **Slot maps for new nodes** — each new node declares its vertical/horizontal slot graph:
  - `NodeMatrix`: LEFT/RIGHT walk cells row-major (exit fence at ends); UP/DOWN move within the
    column, entering from the nearest column by cursor X (see hit-testing below).
  - `NodeScripts`: UP from base→sup, DOWN from base→sub; LEFT/RIGHT exit at edges.
  - `NodeLimit`: DOWN enters under-row; `NodeBinom`: UP/DOWN between the two rows;
    `NodeFactorial`: base is the single slot.
- **Column-aware vertical moves** [P]: `moveUp/Down` currently target slot-start; v2 records the
  cursor's last X (from layout offsets) and enters the vertical sibling at the child index whose
  x-span contains it (`enterRowAtX(row, xPx)` — pure function over cached layouts). This is what
  makes matrix/cfrac editing feel 2-D.
- **Hit-testing** (touch/future mouse in emulator): `hitTest(node, px, py) -> {NodeRow*, index}`
  descends by bounding boxes (top-based placement already computes them), resolving to the nearest
  inter-child gap. Exposed only under NATIVE_SIM initially (test hook + emulator interaction).
- **Selection**: out of scope for MRV2 (no selection model exists anywhere today); the cursor/anchor
  pair design is noted for F. No geometry reserved.

Cursor-movement pseudocode (vertical, v2):

```
moveUp():
    slot = currentRow
    while slot:
        target = slot.owner.verticalSibling(slot, UP)      # per-node slot graph
        if target: cursor = enterRowAtX(target, lastCursorX); return
        slot = parentSlot(slot)
    # at root: no-op (CalculationApp history takes UP when result shown [V])
```

## 16. Caching model and invalidation

Defined normatively in architecture spec §2.6 (memo key = (emSize, style, scriptLevel); parent-chain
invalidation; row-spacing caveat; Y-7 cache-off equivalence). Additional layout-spec rules:

- `LayoutResult` remains the only cached artifact; no cached glyph plans across frames in v2
  (GlyphPlan is recomputed per draw — it is cheap table lookup; revisit only with profiling
  evidence).
- Invalidation triggers (exhaustive list): structural mutation (chain), number-text edit (chain),
  style/em change (lazy by key), font regeneration (build-time — keys unchanged but metrics
  differ ⇒ full recompute forced by `metricsFromFont` probe values feeding the key... **not**
  sufficient: em+style identical ⇒ [P] a global `g_fontGeneration` counter mixed into the key,
  bumped on font-binding init).
- Frame with valid root = zero measure calls (test plan asserts via NATIVE_SIM counter).

## 17. Incremental re-layout on edit (pseudocode)

```
on_structural_edit(mutatedRow):
    invalidateChain(mutatedRow)          # O(depth): row → … → root
    canvas.invalidate()
next onDraw:
    budget = {24, 4096}
    root.layoutIfNeeded(fmNormal, budget)
        # descends: valid subtrees return in O(1); dirty path re-measures;
        # dirty row re-runs spacing over its (cached) children
    place + draw
```

## 18. "Final boss" feasibility (exemplar B) — layout-engine view

| Aspect | Already close [V] | Needs new box [P] | Needs glyphs | Needs semantic/parser work | Too expensive for P1 |
|---|---|---|---|---|---|
| Giant bracket `[...]^{det M}` | Power-of-paren composition; delimiter growth; vector fallback | — | assembly glyphs for *clean* tall brackets (MR-04/11); vector fallback acceptable interim | — | — |
| ∫₀^{π/2} | NodeDefIntegral complete | — | — | numeric eval exists via Simpson path; exact = CAS | — |
| Display Σ in numerator | Summation box complete | `forceDisplayStyle` bit | — | — | — |
| binom(n,k) | stack machinery constants extracted | NodeBinom | — | eval P2 | — |
| cfrac tower under √ | fraction policy + radical compose today (manually built tree renders now, minus ⋱) | NodeEllipsis | ⋱ in subset [V] | display-only semantics | — |
| det + 2×2 Greek matrix exponent | power-of-paren; script sizing of exponent row | NodeMatrix, NodeIdentifier | Greek in subset [V] | det semantics via MatrixEngine (P2) | exact symbolic det of symbols → CAS-only, F |
| lim n→∞ | limit-gap constants extracted | NodeLimit, OpKind::To, ∞ identifier | → ∞ in subset [V] | binder semantics; eval F | Giac `limit()` integration (H-2 guards) |
| Repeating radical tail | nested roots compose | NodeEllipsis | ⋯ in subset [V] | display-only | — |
| Whole formula on 320×240 | X-scroll | Y-scroll (MR-17); depth 24 budget (MR-05) | — | — | evaluating B as a whole (out of scope permanently — display-only by ellipsis rule) |

Bottom line: **zero unknown layout technology** is required for exemplar B — every behavior is
either shipped, a composition, or a bounded new box with its constants already extracted from the
MATH table. The risks are engineering risks (depth budget, viewport, golden churn), not research
risks; they are ticketed as MR-05/11/12/13/14/17/18.

---

*End of Document 3.*
