# NumOS — MathRenderer v2 MR-00 Decision Records (ADR set)

> **Ratification document for ticket MR-00** (`NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md:40-45`).
> Formalizes every load-bearing MRV2 decision as an ADR so the human maintainer can ratify or
> amend each one **before any Phase-0 code moves**. Amendments here propagate to the six MRV2
> spec documents (MR-00 acceptance rule).
>
> Verified against the working tree at `b234e15` (2026-07-03). The only commits since the MRV2
> spec set was verified (`main @ 2d6b796`) are the spec documents themselves
> (`git diff --stat 2d6b796..HEAD` touches `docs/specs/` only — re-checked in this audit), so all
> `file:line` citations below were re-verified directly against source, not inherited.
>
> **Status vocabulary**: `accepted-by-spec` = already decided in the MRV2 spec set; this ADR
> restates it with evidence and makes it ratifiable. `proposed` = new precision added by this
> document (Phase-0 execution detail the spec set left open). `open` = genuinely undecided;
> the ADR names a recommendation and the deadline ticket.
>
> Companions: `NUMOS_MATHRENDERER_V2_ARCHITECTURE_SPEC.md` (AD-1, invariants Y-1…Y-9),
> `NUMOS_MATH_INPUT_SEMANTICS_AND_SERIALIZATION_SPEC.md` (MRD-1…MRD-5),
> `NUMOS_MATHRENDERER_MR01_OBSERVABILITY_AND_CAPS_EXECUTION_PLAN.md`,
> `NUMOS_MATHRENDERER_MR02_GEOMETRY_HARNESS_EXECUTION_PLAN.md`,
> `NUMOS_MATHRENDERER_PHASE0_ACCEPTANCE_AND_ROLLBACK_PLAN.md`.

---

## Ratification sheet (sign here)

| ADR | Title | Status | Recommendation |
|---|---|---|---|
| ADR-MR00-01 | One authored VPAM tree, derived semantic views | accepted-by-spec | ratify |
| ADR-MR00-02 | NumIR as the canonical textual serialization | accepted-by-spec | ratify |
| ADR-MR00-03 | Budgets: layout depth 24 / editor 16 / nodes 4096 (+ editor nodes 512, digits 32) | accepted-by-spec (+ proposed detail) | ratify |
| ADR-MR00-04 | Display-only notation is first-class and poisons evaluation | accepted-by-spec | ratify |
| ADR-MR00-05 | Glyph fallback: 5-rung ladder ending in visible tofu | accepted-by-spec | ratify |
| ADR-MR00-06 | Geometry snapshots: per-node, CI-gated, fixture-metrics-based | proposed (this doc adds the fixture design) | ratify |
| ADR-MR00-07 | Golden-change policy: byte-identical everywhere except MR-11 + exemplar gates | accepted-by-spec | ratify |
| ADR-MR00-08 | Exemplar A = Phase 1 gate (MR-10); Exemplar B = Phase 3 gate (MR-18), display-only | accepted-by-spec | ratify |

---

## ADR-MR00-01 — One authored VPAM tree with derived semantic views (vs split semantic/presentation trees)

**Status**: accepted-by-spec (arch spec §2.1, decision AD-1). Ratification requested.

### Current source reality

- The VPAM tree (`vpam::MathAST`, 17 node kinds, `MathAST.h:72-90`) is simultaneously:
  1. **Edit model** — `CursorController` stores the cursor as `{NodeRow*, index}` into it and
     mutates it structurally (`CursorController.h:110-174` inserters; impl
     `CursorController.cpp:320-1033`).
  2. **Presentation model** — every node carries a cached `LayoutResult _layout`
     (`MathAST.h:447-459`, field at `MathAST.h:646`) and a `calculateLayout(FontMetrics)`
     virtual (`MathAST.h:634`); geometry policy helpers live in the same header
     (`fractionPartMetrics` `MathAST.h:314-326`, `fractionBarGaps` `MathAST.h:569-588`,
     `superscriptShiftMetrics` `MathAST.h:484-513`).
  3. **Evaluation input** — `MathEvaluator::evaluate(const MathNode*)` walks it directly
     (`MathEvaluator.cpp:517-535`).
  4. **Interchange tree** — GrapherApp serializes out of it by hand
     (`GrapherApp.cpp:1103-1275`); CAS results are converted into it (`SymExprToAST`).
- There is **no model-synchronization code anywhere**, and consequently zero
  model-sync bug class.

### Options considered

| Option | Sketch | Cost |
|---|---|---|
| A. Split trees (NumWorks Poincaré `Layout`/`Expression`, MathML presentation/content) | authored presentation tree + derived-and-editable semantic tree, bidirectional converter | every edit feature implemented twice; converter must be total and bug-free; doubles per-expression PSRAM and doubles the raw-parent-pointer fix-up surface (`cloneNode`, `MathAST.h:1429`) |
| B. One authored tree + capability lattice + derivation at commit points (**chosen**) | keep VPAM as the single source of truth; derive `ExactVal`/SymExpr/Giac text/NumIR at ENTER/solve/plot; per-kind capability bits make semantic gaps typed | must keep layout policy from re-entangling with structure (B-1 boundary, MR-05); display-only nodes need explicit refusal machinery (T-2) |
| C. Status quo (no formalization) | do nothing | C-1…C-4 costs persist: golden-fragile AST header, invisible semantic gaps ("Math ERROR" for Summation, `MathEvaluator.cpp:533`), no storage format, per-frame relayout (`MathRenderer.cpp:737`) |

### Decision

**Option B.** One authored tree (the VPAM tree). Semantic artifacts are *derived views* produced
only at commit points (ENTER / solve / plot / serialize), never maintained in parallel.
Presentation-only constructs (future `NodeEllipsis` etc.) are ordinary nodes whose capability set
excludes `CAP_EVAL` — they are not exiled to a separate tree.

### Consequences

- Layout policy must migrate **out of** `MathAST.h` into `src/math/layout/` (MR-05) with
  bit-identical values (the geometry-frozen refactor that MR-02's snapshots make provable).
- The capability lattice (`mrCapabilities`, arch spec §2.3) becomes the single semantic gate;
  totality is compiler-enforced (exhaustive switch, no `default`).
- Every future node kind lands once (structure + capability row + layout + draw + NumIR),
  not twice.

### Rejected alternatives

- **Split trees** — rejected: NumOS's zero-sync-bug property is worth more than architectural
  purity on a 320×240 single-user device; the converter is the highest-defect-density component
  in every split-tree system.
- **Semantic-tree-only with layout annotations** — rejected: the structural cursor
  (`{NodeRow*, index}`) *is* the editing UX; discarding it re-opens the entire editor.

### Implementation implications / affected files & tickets

- Phase 0 touches nothing structural; MR-01/MR-02 are pure observability on the existing tree.
- MR-05 (policy extraction, memoization), MR-07…MR-14 (new node kinds), MR-16 (capability
  enforcement at CAS bridges) all assume this ADR.

### Tests unlocked

- Tree well-formedness oracle (parent pointers, slot arity) can be written once (O-A layer).
- MR-02 geometry snapshots are meaningful only because the rendered tree *is* the authored tree.

### Risks

- **Medium**: layout policy can silently re-entangle with `MathAST.h` in future tickets.
  Mitigation: B-1 boundary rule + MR-02 snapshot gate (any policy move must be bit-identical).

---

## ADR-MR00-02 — NumIR as the canonical textual serialization

**Status**: accepted-by-spec (input spec §2 MRD-1, §3 grammar). Ratification requested.
Implementation is MR-03 (Phase 0 exit); MR-01/MR-02 only *reserve* the vocabulary.

### Why the debug `dumpTree` is insufficient

`dumpTree` (`MathAST.h:1421`, impl `MathAST.cpp:1558-1734`) is a human-oriented indented dump that
interleaves **geometry** with structure — verified in this audit, output shape:
`Row  [42x65 a50/d15]` (metrics suffix built at `MathAST.cpp:1563-1570`). It is:

1. **Not parseable back** — there is no reader; round-trip identity (S-1) is undefined.
2. **Geometry-coupled** — its output changes whenever layout changes, so it cannot serve as a
   *structural* oracle (a spacing fix would "change" every stored expression).
3. **Not versioned, not stable** — labels are ad-hoc (`"num:"`, `"den:"`) and Spanish/English
   mixed; no grammar contract exists.
4. **Allocation-heavy** (`std::string` concatenation) — unsuitable as a storage format on device.

It stays exactly what it is: a debug aid (and, stripped of its metrics suffix, a useful *input*
to MR-02's structural sanity checks).

### Why Grapher's hand serialization is insufficient

`serializeNode/serializeRow` (`GrapherApp.cpp:1103-1264`) + `syncASTtoText`
(`GrapherApp.cpp:1266-1275`) is a **lossy, app-private, 63-char-capped** flattener:

- Hard cap: `serializeRow(_exprASTRow[idx], f.text, pos, 63)` (`GrapherApp.cpp:1271`) —
  silent truncation (REL C7.3).
- Lossy by design: `NodeLogBase` emits the change-of-base identity
  `(log(a)/log(b))` (`GrapherApp.cpp:1232-1254`) — the original notation is unrecoverable.
- Covers only the node kinds the legacy RPN grammar accepts (`NodeType::Empty`/`Subscript`/
  `Summation`/`DefIntegral`/`BigOp`/`PeriodicDecimal` fall through to nothing,
  `GrapherApp.cpp:1260-1262`).
- Already caused a shipped bug class: `OpKind::Eq` fell through to `'+'` until Phase 9F
  (comment at `GrapherApp.cpp:1129-1135`).

### Decision

**NumIR** — deterministic, LL(1), ASCII, prefix/functional text (`frac(1;2)+frac(1;3)`),
isomorphic to the VPAM tree by construction. Grammar: input spec §3.1.

- **Structural, textual, or hybrid?** **Textual with structural semantics**: the text is the
  storage/interchange form; parsing yields the structural tree; identity is defined structurally
  (`parse(emit(t)) ≡ t`, S-1). There is no separate binary structural format.
- **Grammar stability policy**: append-only, like the `.numos` command vocabulary
  (GHOOK precedent, `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md` §A). Existing structure `NAME`s,
  slot arities and slot order are frozen once MR-03 lands; new notation adds new `NAME`s.
- **Backwards compatibility / versioning**: every persisted string carries the `numir1:` prefix
  (S-4). Under `numir1`, an **unknown structure NAME is a parse error** — no silent skip, no
  best-effort recovery. A breaking grammar change requires `numir2:` plus a one-way migrator;
  none is anticipated inside MRV2.
- **Examples** (normative, from input spec §3.1):
  - `frac(1;2)+frac(1;3)` ⇔ the `calc_fraction_sum` tree.
  - `pow(paren(id(x)+1); 2)` ⇔ `(x+1)²`.
  - Exemplar A: `frac(1;pi) = frac(2*root(2);9801) * @display sum(id(k)=0; inf; …)`.
- **LaTeX**: import rejected; export deferred to optional MR-20 (arch spec Q8).

### Rejected alternatives

- **LaTeX subset as internal format** — rejected (input spec §2 comparison table): context-
  dependent grammar, lossy both ways, no repo alignment; NumIR matches the proposed Giac
  serializer style (CGIAC §C.2) and the CAS corpus JSONL inputs.
- **Binary serialization** — rejected: not human-authorable in `.numos` tests, not diffable
  in PRs, no size pressure that justifies it (expressions are ≤512 editor nodes by ADR-MR00-03).
- **Promoting `dumpTree`** — rejected per above.

### Affected files & tickets / tests unlocked / risks

- MR-03 implements `src/math/numir/NumIR.h/.cpp` + `type_numir` / `assert_math_serialized`.
- Phase 0 dependency direction: MR-01's `.numos` grammar additions must not collide with the
  names MR-03 reserves (`type_numir`, `assert_math_serialized`) — MR-01 does not implement them.
- Unlocks: round-trip oracle (O-A), history persistence, cross-engine alignment, `mathv2_*`
  scripts that build trees without 40-keypress sequences.
- Risk: **Medium** — grammar freeze quality; that is why MR-03 is Fable-owned
  (tickets doc, MR-03).

---

## ADR-MR00-03 — Layout depth/node budgets (24 / 16 / 4096, editor nodes 512, digits 32)

**Status**: accepted-by-spec for 24/16/4096 (arch spec Y-3/Y-4, REL H-5, tickets MR-01/MR-05);
**proposed** for the split of enforcement across MR-01 vs MR-05 and the exact refusal semantics
(detailed in the MR-01 plan §E/§F).

### Current source reality (what each cap fixes)

| Layer | Today | Evidence |
|---|---|---|
| Draw recursion | capped at 12, red `...` box | `MathRenderer.h:239` (`MAX_RENDER_DEPTH = 12`), `MathRenderer.cpp:1507-1514` |
| Layout recursion | **uncapped** (runs before draw ever gets a chance) | `MathAST.h:634` virtual has no depth parameter; `NodeRow::calculateLayout` recurses freely (`MathAST.cpp:190-210`); REL C5.1 |
| Editor insertion (CalculationApp) | **uncapped** | `CalculationApp.cpp:326-429` inserts with no guard; `CursorController.cpp` has no depth/node/digit limit anywhere (grep-verified this audit) |
| Editor insertion (GrapherApp) | capped at `MAX_NEST = 8` | `GrapherApp.cpp:2546-2593` (`cursorDepth()` walks parent pointers) |
| Digit length | **uncapped** | `NodeNumber::appendChar` (`MathAST.cpp:287-293`); REL C5.3 |
| Node count | **uncapped** everywhere (editor, CAS→VPAM `SymExprToAST`, history clones) | REL C5.3/C5.8 |
| Evaluator recursion | uncapped | `MathEvaluator.cpp:517-535`; REL C5.4 |

### Decision (the five numbers)

| Cap | Value | Enforced at | Ticket |
|---|---|---|---|
| **Editor insertion depth** | **16** (`kMaxEditorDepth`) | `CursorController` structural inserters (refusal before mutation) | MR-01 |
| **Editor node count** | **512** (`kMaxEditorNodes`) | same | MR-01 |
| **Editor digit length** | **32** chars per `NodeNumber` (`kMaxNumberDigits`) | `CursorController::insertDigit` guard (before `appendChar`) | MR-01 |
| **Layout hard cap (depth)** | **24** (`LayoutBudget.maxDepth`) — breach ⇒ fixed ⚠ box, `[LVGL] layout-depth` log | layout entry (`layoutIfNeeded`) | MR-05 (MR-01 ships *detection only*: counters + budget-exceeded flag + warning log, no refusal) |
| **Renderer draw cap** | raised 12 → **24 in lockstep with the layout cap** (draw must never out-recurse layout) | `drawNodeBaseline` | MR-05 (stays 12 through Phase 0) |
| **Layout node budget** | **4096** per canvas — breach ⇒ same ⚠ refusal; also the cap applied to `SymExprToAST` output (closes C5.8 display-side) | LayoutBudget + flattener | MR-05/MR-16 (MR-01: detection only) |

### Why these numbers (stack-risk rationale + exemplar-B relation)

- **24 layout depth**: exemplar B's worst boxed branch is **17 levels**
  (arch spec §9.2 audit: Row→Power→Paren→Row→DefIntegral→Row→Fraction→Row→Root→Row→Fraction→Row→
  Fraction→Row→Fraction→Row→Ellipsis). 24 = 17 + ~40 % headroom. Stack cost: everything runs on
  the shared 64 KB loopTask stack (`platformio.ini:44`, `-DARDUINO_LOOP_STACK_SIZE=65536`);
  at a conservative ~200 B per `calculateLayout`/`draw*Baseline` frame, depth 24 ≈ 5 KB — well
  inside REL M-5's ≥8 KB high-water floor. (MR-05's firmware acceptance measures this with the
  `[MEM]` `stack=` field.)
  - **Why not 16**: refuses exemplar B (17 > 16) — the acceptance target itself.
  - **Why not 20**: only 3 levels of headroom over B; the first future construct that costs a
    wrapper level (e.g. `@display` wrapper, or `NodeScripts` around a fraction) would force a
    budget bump, and budget bumps re-open the stack-audit work. 24 absorbs one construct
    generation.
  - **Why not 32**: ~+1.6 KB more worst-case stack for no exemplar that needs it; deeper trees
    are unreadable at 320×240 anyway (a 24-deep tower is already ~4× the screen height).
- **16 editor depth**: REL H-5's number. Hand input never *needs* more — the deepest thing a
  human types in Calculation today is ~4-6 boxed levels (nested fraction exercises); 16 is 3-4×
  that. Being **8 below the layout cap** guarantees hand-typed trees can never reach layout
  refusal even after capture-insertions wrap existing content (`insertFraction` captures the left
  operand, `CursorController.h:125-133` — each capture can add 2 boxed levels around content that
  is itself ≤16... see MR-01 plan §E for the exact "capture can exceed by construction" analysis
  and the post-capture re-check). Template/CAS/NumIR-built trees may legitimately reach 24 —
  they are constructed, not keyed (arch spec Y-3).
- **512 editor nodes**: a full-width scrolled expression at em 18 is ~40-60 visible atoms;
  512 is ~10 screens of typed content — beyond any calculator use, small enough that the O(n)
  `countNodes` walk on structural insert is negligible (<10 µs native, <1 ms device).
- **4096 layout nodes**: bounds CAS-result display (C5.8). A 6×6 matrix ≈ 150 nodes; 4096 admits
  ~27 of them — generous for anything legible. At 4096 the *node-count walk itself* stays
  sub-millisecond.
- **32 digits**: `ExactVal` is int64-backed (≤19 significant digits); Extended mode renders up to
  500 digits but via **result trees**, not typed input. 32 typed digits covers every meaningful
  literal plus slack; C5.3's unbounded `appendChar` growth closes.

### Behavior when exceeded (summary; normative detail in MR-01 plan §E/§F)

- Editor caps (MR-01): the insert is **refused before any mutation** — tree unchanged, cursor
  unchanged, `[INPUT] depth-cap` / `[INPUT] node-cap` / `[INPUT] digit-cap` log (closed DIAG tag
  set, telemetry spec §2.10), CalculationApp shows a 1.5 s toast, NATIVE_SIM exposes the refusal
  for asserts. GrapherApp keeps its `MAX_NEST=8` until MR-16 unifies.
- Layout budget (MR-05, future): ⚠ box for the offending subtree, `[LVGL] layout-depth` log,
  never partial layout, never a crash.
- In between (Phase 0 reality, stated honestly): trees built by CAS results deeper than the draw
  cap render the existing red `...` marker at depth 12 [V]; layout recursion for such trees
  remains uncapped until MR-05 — MR-01 adds **visibility** (depth/node counters + budget-flag +
  `[LVGL] layout-budget` warning line), not protection.

### Rejected alternatives

- **Single shared cap for editor and renderer** — rejected: it either blocks constructed trees
  (cap 16) or lets hand input reach refusal UX (cap 24); two-tier is strictly better.
- **Global recursion guard via stack-pointer probing** — rejected: non-portable, masks the
  design problem, unusable on host oracles.

### Affected files & tickets / tests unlocked / risks

- MR-01: `CursorController.{h,cpp}`, `CalculationApp.cpp`, `MathAST.{h,cpp}` (pure stat
  helpers), `NativeHal.cpp`, new `mathv2_caps_*.numos`.
- MR-05: `LayoutBudget`, ⚠ box, draw cap raise.
- Unlocks test categories 4.14 (deep-nesting stress) and the exemplar-B depth assert (=17).
- Risks: **Medium** — capture-insert edge cases at the boundary (MR-01 plan §E.5 enumerates
  them); refusal UX regressing normal typing (mitigated by cap≫normal-use + full 8C suite).

---

## ADR-MR00-04 — Display-only notation policy

**Status**: accepted-by-spec (arch spec §2.3 T-2, input spec §5.1 MRD-2, notation spec §P).
Ratification requested. No Phase-0 code implements it; Phase 0 only fixes the vocabulary
(refusal kinds) that MR-01's assert grammar reserves.

### Decision

1. **NumOS may store and render display-only constructs.** The capability lattice admits
   `Renderable ∧ Editable ∧ Serializable ∧ ¬Evaluable ∧ ¬CAS` nodes. `NodeEllipsis`
   (⋯ ⋮ ⋱ …, P2) is the archetype; `IdentKind::Styled` (blackboard ℝ, P2) is the decorative
   variant.
2. **Poisoning rule**: evaluation admissibility is decided by a **whole-tree pre-flight**
   (`evalAdmissible`, arch spec §2.7) *before* any computation. One node without `CAP_EVAL`
   anywhere in the tree ⇒ typed refusal `EvalRefusal::DisplayOnly` for the whole expression.
   Never partial evaluation, never a silent wrong value, and **never the generic `"Math ERROR"`**
   — today `MathEvaluator` returns exactly that for `DefIntegral/Summation/Subscript/BigOp`
   via its `default:` arm (`MathEvaluator.cpp:533`), indistinguishable from a domain error;
   that conflation is the bug this ADR retires (MR-09 implements the split).
3. **CalculationApp surfacing** (input spec §5.5, §7): refusals are a third result class,
   visually distinct from math errors — neutral chip ("display only" / "needs finite bounds" /
   "unbound variable"), expression preserved and history-navigable, S⇔D rotation disabled.
   Refusals are *not* errors: the expression is stored in history and round-trips via NumIR.
4. **CAS boundary**: nodes without `CAP_CAS` are refused at `ASTFlattener`/Giac serializers
   (J.2-class refusal, CGIAC §C.2), never approximated (T-3; MR-16).
5. **Are ellipsis/ddots/cdots semantic?** **No.** `NodeEllipsis(*)` has no semantics —
   `CAP_RENDER|CAP_EDIT|CAP_SERIALIZE` only (notation spec §P). An infinite continued fraction
   or repeating radical is expressed as finite nesting **terminated by** an ellipsis node and is
   display-only *as a whole*; its finite prefixes (without the ellipsis) evaluate normally.
   Rationale: giving dots limit semantics would require a summation/limit inference engine with
   no defined grammar — "pretty but meaningless" is exactly what T-2 exists to make impossible
   to *mislabel*, not to fake.

### Rejected alternatives

- **Refuse to store display-only constructs** — rejected: exemplar B is the acceptance target
  and is display-only as a whole (arch spec §9.2).
- **Evaluate "best effort" around display-only nodes** — rejected: partial evaluation of a tree
  whose meaning the machine does not know is a silent-wrong-answer generator (CSEM honesty rule).
- **A fifth "decorative" capability** — rejected: decorative = `R∧S∧¬E∧¬V∧¬C` already
  (input spec §5.3, MRD-4).

### Affected tickets / tests unlocked / risks

- MR-09 (`evalAdmissible` + refusal chip), MR-14 (`NodeEllipsis`), MR-16 (CAS refusals).
- Phase 0 reserves the refusal-name vocabulary in the `.numos` grammar
  (`DisplayOnly | UnboundedDomain | UnboundVariable | DepthExceeded | None`) so scripts written
  later never need a grammar break (MR-01 plan §H).
- Unlocks 4.16 (unsupported-notation suite) and the T-2 negative control
  (`assert_math_refusal name=None` on an ellipsis tree must FAIL).
- Risk: **Low** in Phase 0 (vocabulary only).

---

## ADR-MR00-05 — Glyph fallback policy (no invisible math)

**Status**: accepted-by-spec (arch spec §2.5/Y-8, layout spec §10-§12). Ratification requested.
Phase 0 ships **counters only** (MR-01); the ladder itself is MR-06.

### Current source reality (the three paths that can end in nothing or near-nothing)

1. `drawDelimiterGlyph` (`MathRenderer.cpp:184-283`): a 3-fallback chain — no variant table →
   draw base glyph or **vector strokes** (`drawStrokedDelimiter`, `MathRenderer.cpp:87-172`);
   assembly invalid → base glyph or strokes; assembly valid but pieces missing from the subset →
   strokes (`assemblyGlyphsAvailable`, `MathRenderer.cpp:174-182`, checked at `:238-241`).
   Delimiters therefore never vanish **today** — but only because of the per-call-site stroke
   fallback, and `drawPieceAtTop` still silently skips a piece whose glyph lookup fails
   (`MathRenderer.cpp:243-245`).
2. `drawTextBaseline` (`MathRenderer.cpp:2434-2487`): a codepoint missing from the font advances
   the pen by `max(1, font->line_height/3)` and draws **nothing** (`MathRenderer.cpp:2480-2483`)
   — the REL C5.7 invisible-glyph class.
3. Assembly availability is gated by a single boot-time probe of U+239C setting
   `g_delimiterAssemblyRenderable` (`MathRenderer.cpp:431-435`; default `false`,
   `MathGlyphAssembly.cpp:15-18`) — a whole-family proxy, not per-piece.

### Decision

- **Ladder** (MR-06, `GlyphProvider`): exact glyph → size variant → assembly (per-piece
  availability, generalizing the U+239C probe) → **vector synthesis** → **visible tofu**
  (box + hex codepoint) + one `[LVGL] glyph-missing cp=%X` log per codepoint. Rung order flips
  in favor of assembly only when MR-04's font regeneration lands (MR-11 re-bless wave).
- **No-invisible-math invariant (Y-8)**: no draw path may resolve to "nothing"; the silent
  `line_height/3` advance is retired by the tofu rung.
- **Allowed vector-synthesis set** (permanent tier, layout spec §12): `( ) [ ] { } |` and the
  radical `√` hook/slope/overline (already vector today, `MathAST.h:881-883`), `±` centering
  strokes, and floor/ceil rendered as bracket variants. Stroke geometry stays bit-identical to
  today's (`drawStrokedDelimiter` weight `max(2,(em+4)/8)`, `MathRenderer.cpp:96`).
- **Forbidden vector-synthesis set**: letterforms (Greek, blackboard), ∑ ∏ ∫ and all large
  operators, ⋯ ⋱ dots, arrows, ∞ — anything whose hand-drawn approximation would be a *lie about
  the glyph*. These fall to tofu.
- **Refusal is never a fallback rung**: refusal at draw time has no user-visible channel (T-4);
  something is always drawn.
- **Font regeneration policy**: fonts are generator-only artifacts
  (`scripts/generate_stix_math_font.sh`; hand edits are overwritten — GT fragile-files rule).
  Regeneration is its own ticket (MR-04) with byte-identical golden acceptance; the renderer
  must not silently change rungs when the font changes — rung flips are explicit (MR-11).
  MR-02 pins this: the geometry fixture records a **font-metrics hash**, so a regeneration that
  moves any metric fails the geometry gate until deliberately re-baselined
  (ADR-MR00-06, MR-02 plan §E/§G).

### Rejected alternatives

- **Refuse to render on missing glyph** — rejected (T-4): no channel to tell the user.
- **Synthesize everything** — rejected: fake Greek/∑ glyphs misrepresent math.
- **Silent skip (status quo for text runs)** — rejected: C5.7.

### Affected tickets / tests unlocked / risks

- MR-01: counters `debugMissingGlyphCount` / `debugVectorDelimCount` (observability only — the
  no-golden-impact proof is trivial because counting changes no pixels).
- MR-06: the ladder; MR-04/MR-11: assembly glyphs + sanctioned re-bless.
- Unlocks 4.15 (fallback paths) and the future `assert_math_glyphplan tofu=0` gate.
- Risk: **Low** in Phase 0; **Medium-High** at MR-06 (draw-path churn — geometry frozen for all
  present glyphs).

---

## ADR-MR00-06 — Geometry snapshot policy

**Status**: **proposed** (this document + the MR-02 plan add the concrete design the spec set
only sketched as oracle O-B). Ratification requested — this is the highest-leverage new decision
in Phase 0.

### What geometry snapshots measure

- Per expression case: the full **per-node** `LayoutResult` tree — for every node: kind, slot
  path, depth, scriptLevel, `{width, ascent, descent, inkAscent, inkDescent}` — plus node-kind
  extras the layout caches (fraction rule/shift/overhang getters `MathAST.h:785-790`, paren
  width `MathAST.h:937`, radical gaps `MathAST.h:886-890`, power italic correction
  `MathAST.h:836`), plus per-row inter-atom gaps read through the production
  `interAtomSpacingPx` (`MathTypography.h:166-176` — the single DRY path layout and cursor
  geometry already share).
- **Every node, not just root metrics** (Special Question 4): root-only metrics miss
  compensating drift (numerator up 1 px + denominator down 1 px = identical root box, visibly
  different pixels). Per-node data is what lets a reviewer localize a diff to "the superscript
  shift of case 41 changed by 1 px" without opening an image.
- Computed with **committed fixture FontMetrics** (see below), on the host, by calling the
  production `calculateLayout` — never a re-implementation (Special Question 9; feasibility
  proven in this audit: `g++ -std=c++17 -I src` over exactly `MathAST.cpp` +
  `font/MathGlyphAssembly.cpp` compiles and lays out deterministically with zero LVGL/Arduino,
  one pre-existing `-Wunused-variable` warning at `MathAST.cpp:1112`).

### The fixture-metrics decision (how host geometry equals device geometry without LVGL)

The renderer derives `FontMetrics` by probing LVGL glyph descriptors at runtime
(`metricsFromFont`, `MathRenderer.cpp:446-535`). Two ways to reproduce that on host:

| Option | Cost | Verdict |
|---|---|---|
| Link LVGL + the three `stix_math_*.c` fonts into the harness and call the real probe | drags the LVGL library into a plain-g++ harness; couples the geometry gate to LVGL version pins | rejected |
| **Commit the probed values as a fixture header** (`tests/host/mathlayout_fixed_metrics.h`) captured once from the emulator via an MR-01 metrics dump, and **gate fixture drift in CI** with an emulator step that re-dumps and diffs | zero LVGL in the harness; fixture is 3 × 9 int16 fields + wiring; drift = hard CI failure with an obvious re-baseline path | **chosen** |

The parity gate makes the fixture safe: if `metricsFromFont` output ever changes (font
regeneration, LVGL glyph-descriptor change, probe change), the emulator dump step fails **before**
anyone trusts stale snapshots.

### Policy answers

- **CI-gated?** **Yes, hard gate**, placed after the KeyCode guard and **before** the emulator
  build (fails in ~30 s; CAS-harness precedent, `NUMOS_CAS_HOST_HARNESS_SPEC.md` §E), plus the
  fixture-parity step after the emulator build.
- **When updated?** Only intentionally, via a `--update` mode that a human runs and commits, with
  the PR diff (JSONL is line-diffable) as the review record — the exact analog of golden
  promotion (`tests/emulator/golden/README.md` flow). CI never writes snapshots.
- **Relation to PPM goldens**: snapshots are the *cheap, localized, font-rasterization-free*
  layer under the goldens. Snapshots catch measurement drift (boxes, shifts, gaps) per node and
  per case; goldens catch what snapshots cannot: draw-path bugs (placement code, stroke
  rendering, colors, clipping, cursor, LVGL compositing). Both are required for any
  geometry-visible change; freeze-refactors (MR-05/06) must hold **both** at zero diff.
- **Stability across font regeneration** (Special Question 5): **not stable, by design** —
  snapshots depend on the metrics fixture; a regeneration that changes any probed metric fails
  the parity gate and forces an explicit two-artifact re-baseline (fixture + snapshots) in the
  regenerating PR, reviewed like a golden re-bless. A regeneration that changes *no* metric
  (pure glyph additions, the MR-04 goal) leaves snapshots untouched — which is exactly the
  byte-identical claim MR-04 must prove.
- **False positives**: near-zero — layout is integer math over committed constants (Y-6);
  the double-pass determinism check (run layout twice, compare — verified identical in this
  audit's probe) plus exact integer comparison leaves OS/libm variance no entry point.
  The realistic false-positive is an *intentional* metric change forgotten in the fixture —
  which the parity gate converts into a loud failure.
- **False negatives** (accepted, covered elsewhere): draw-path-only bugs (goldens), cursor
  geometry (`childXOffset`, `MathRenderer.h:392` — covered by cursor-role asserts + goldens),
  placement conversions (`drawNodeWithLayout` top/baseline math, `MathRenderer.cpp:1481-1493`
  — goldens), glyph raster differences (goldens).

### Rejected alternatives

- **Snapshot only root metrics** — rejected (compensating drift, above).
- **Tolerance-banded comparison** — rejected for the committed snapshots: layout is integers;
  any drift is a decision, not noise. (Tolerance bands exist only in the *emulator*
  `assert_math_layout` command for cases meant to survive planned tuning — test plan §2/§9.)
- **PlatformIO `native` env for the harness** — rejected for the same three reasons the CAS
  harness rejected it (`NUMOS_CAS_HOST_HARNESS_SPEC.md` §C.4): LDF/lib_ignore dance, the
  Windows `build_dir` trap (`platformio.ini:6`), and implicit-filter coverage loss.

### Affected files & tickets / tests unlocked / risks

- MR-02 (the harness), MR-01 (the metrics dump it consumes), MR-05 (cache-off/cache-on equality
  gate — Y-7), MR-04/MR-11 (fixture re-baseline protocol).
- Unlocks: bit-identical proof for every freeze-refactor; layout-drift detection **before** any
  pixel changes; per-construct geometry regression tests (test plan 4.2-4.14).
- Risk: **Low-Medium** — fixture capture procedure must be exact (MR-02 plan §D.4 gives the
  verbatim procedure); snapshot churn noise if the corpus includes unstable cases (corpus is
  fixed and versioned, MR-02 plan §F).

---

## ADR-MR00-07 — Golden-change policy for renderer work

**Status**: accepted-by-spec (tickets doc "Cross-ticket honesty ledger", test plan §5, REN-1).
Ratification requested.

### Ground truth being protected

19 committed goldens + 18 masks + 71 `.numos` scripts, 25 candidate stems in
`generate-emulator-candidates.py:39-143` (all counts re-verified in this audit — note the older
"20 goldens / 43 stems" figures in `NUMOS_ARCHITECTURE_GROUND_TRUTH.md` §A are stale). Golden
mismatch fails CI (`emulator-build.yml:575-607`); missing golden warns; promotion is human-only
(`scripts/promote-emulator-golden.py`, never invoked by CI); masks cover only the StatusBar
clock rect (`tests/emulator/masks/README.md`).

### Decision

- **Byte-identical always** (any diff = ticket bug, never a re-bless): MR-01, MR-02, MR-03,
  MR-04, MR-05, MR-06, MR-07, MR-08, MR-09, MR-15, MR-16, and — for pre-existing stems —
  MR-12, MR-13, MR-14, MR-17. Phase 0 specifically: **all 19 goldens byte-identical**
  (per-golden list in the MR-01 plan §K).
- **Sanctioned re-bless**: **MR-11 only** (assembly-delimiter switch) — a planned, batched,
  before/after-gallery-reviewed wave — plus the two exemplar gates (MR-10, MR-18) which bless
  *new* goldens, not change old ones.
- **Human review requirements**: every re-bless PR carries the candidate gallery
  (old vs new), one stem per review line, and is promoted only via
  `promote-emulator-golden.py --force` after visual confirmation (golden README "Updating a
  golden" flow — the tool refuses to clobber otherwise). Goldens/masks are never edited by tool
  or by hand (global rule, tickets doc header).
- **Negative controls** (T6 discipline, every Phase-0 PR): at least one deliberately-broken
  input must fail the new gate before it merges — a corrupted geometry snapshot must fail
  MR-02's comparator; an `assert_math_depth` with a wrong expected value must exit 4;
  a stale metrics fixture must fail the parity step.
- **Masks policy**: unchanged and frozen — StatusBar clock rect only; **never mask math-canvas
  pixels** (masks README; test plan §5). New `mathv2_*` captures are taken cursor-off
  (showcase pattern, `NativeHal.cpp:1139-1147`) so they need no mask at all.
- **New stems**: enter the candidate generator immediately as **warning-only** (missing golden
  warns, `emulator-build.yml:598-604`); blessed only when geometry has stabilized
  (post-MR-05 for existing notation; per-feature after).

### Rejected alternatives

- **Tolerance-based image compare** — rejected: byte-exactness is the repo's core visual
  contract (deterministic tick + in-tree font rasterization make variance a bug, GTEST rule).
- **Auto-promotion of goldens when asserts pass** — rejected: semantic asserts don't see
  pixels; ossifying an unreviewed image is the failure mode the warning-tier exists to prevent.

### Tests unlocked / risks

- Makes "freeze-refactor" a checkable property instead of a promise.
- Risk: **Low** — policy only; the risk it *manages* (REN-1 golden churn) is High and is why
  MR-11 is the only sanctioned wave.

---

## ADR-MR00-08 — Exemplar A/B phase targets

**Status**: accepted-by-spec (arch spec §9, tickets phase map). Ratification requested.

### Phase definitions (exit gates, from the tickets phase map)

- **Phase 0 (MR-00…MR-03)** — *measurable renderer*: host geometry harness + math asserts green
  in CI; NumIR round-trips all existing notation; editor caps live. All 19 goldens
  byte-identical. **No new notation renders.**
- **Phase 1 (MR-04…MR-10)** — *Exemplar A*: identifiers (Greek/∞), factorial, finite-Σ/Π
  evaluation with typed refusals, glyph inventory regen (cold), GlyphProvider, layout
  memoization + budgets. Exit: `mathv2_exemplarA` golden blessed; truncated-eval asserts green.
- **Phase 2 (MR-11…MR-15)** — *structure wave*: assembly delimiters go live (the one sanctioned
  re-bless), `NodeScripts`/`NodeLimit`/`@display`, `NodeMatrix`+det, `NodeBinom`+`NodeEllipsis`+
  floor/ceil, template palette + Greek input. Exit: every exemplar-B subconstruct renders in
  isolation, editable + serialized.
- **Phase 3 (MR-16…MR-18)** — *composition & final boss*: CAS-boundary capability enforcement,
  vertical viewport/scroll, whole-formula composition. Exit: exemplar-B golden pair blessed +
  on-device HIL sign-off.
- **Future-only (F / MR-19 / MR-20)**: evaluating exemplar B as a whole (**permanently out of
  scope** — display-only by the ellipsis rule, ADR-MR00-04), lim / exact-∫ / symbolic-det
  evaluation (CAS/Giac-backed), LaTeX export, MatricesApp math-view, selection model, accents,
  piecewise, multi-line alignment.

### Exact target status — Ramanujan 1/π (Exemplar A)

`1/π = (2√2/9801)·Σ_{k=0}^{∞} ((4k)!(1103+26390k))/((k!)⁴·396^{4k})`

| Property | Target | Phase |
|---|---|---|
| Renders whole, navigable, serializes (NumIR) | yes | **end of Phase 1** (gate MR-10) |
| Depth/width | ≈9 boxed levels (inside today's draw cap 12); width ≈300-340 px ⇒ horizontal scroll [V] engages (`MathRenderer.cpp:762-783`) | — |
| Truncated evaluation (finite upper bound N) | exact rational; N=0 term = `1103·2√2/9801` asserted symbolically | Phase 1 |
| Infinite-bound evaluation | **refused by design** (`UnboundedDomain` chip); Giac route is F | permanent refusal in-device; F via CAS |

### Exact target status — "final boss" (Exemplar B)

| Property | Target | Phase |
|---|---|---|
| Every subconstruct renders in isolation (matrix, binom, lim, ⋱-cfrac, radical chain, display-Σ-in-fraction, tall bracket^det) | yes | **end of Phase 2** |
| Whole formula composed, on-device, navigable (depth 17, height ≈210-260 px ⇒ vertical scroll required) | yes | **end of Phase 3** (gate MR-18) |
| Whole-formula evaluation | **never** (display-only: contains `NodeEllipsis`) | permanent, by design |
| Individually evaluable subtrees (numeric det ≤5×5, finite Σ, ∫ numeric) | yes | Phase 2 |
| lim / exact ∫ / symbolic det | refusal on-device; Giac-backed | F |

### Phase-0 relevance (why this ADR is ratified now)

Every Phase-0 artifact is sized by these targets: the depth budget (17→24), the MR-02 corpus
(decomposed A/B pieces buildable **today** become snapshot cases; the rest become explicit
placeholders), and the assert vocabulary (refusal names, depth asserts `n=17`).

### Risks

- **Medium**: exemplar B's height estimate makes MR-17 (vertical scroll) a hard dependency of
  the P3 gate — if MR-17 slips, B can only be shown as two viewport pages (the
  `mathv2_exemplarB_upper/lower` stems are designed for exactly that contingency).

---

*End of MR-00 decision records. Ratify or amend each ADR; amendments propagate to all MRV2 docs
before MR-01 implementation begins.*
