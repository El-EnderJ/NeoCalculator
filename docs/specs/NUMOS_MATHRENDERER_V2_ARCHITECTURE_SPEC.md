# NumOS — MathRenderer V2 Architecture Specification

> **Normative architecture spec** for the next-generation math notation pipeline (MathRenderer v2, program code **MRV2**).
> Verified against the working tree at `main @ 2d6b796` (2026-07-02). Every source claim cites `file:line`.
> Markers: **[V]** = verified true today (must not regress), **[P]** = proposed (does not exist yet), **[D-n]** = open decision.
> Companion documents:
> `NUMOS_MATH_NOTATION_SURFACE_AND_SYMBOL_COVERAGE_SPEC.md` (what math exists),
> `NUMOS_MATH_LAYOUT_ENGINE_AND_TYPESYSTEM_SPEC.md` (how it is measured and drawn),
> `NUMOS_MATH_INPUT_SEMANTICS_AND_SERIALIZATION_SPEC.md` (how it is typed, stored, and evaluated),
> `NUMOS_MATHRENDERER_TEST_ORACLE_AND_GOLDEN_PLAN.md` (how it is tested),
> `NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md` (MR-00…MR-20 roadmap).
> Constraint context: `NUMOS_MEMORY_ALLOCATION_POLICY.md`, `NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md` (invariants H-5, C5.x, RT-10/RT-11), `NUMOS_RISK_REGISTER.md` (REN-1…REN-5).

---

## 0. Executive summary

NumOS already has a far stronger typographic core than a typical calculator firmware: a single
**VPAM tree** (`vpam::MathAST`, 17 node types, `MathAST.h:72-90`) that is simultaneously the edit
model (`CursorController`, structural cursor `{NodeRow*, index}`, `CursorController.h:62-85`), the
layout model (per-node `LayoutResult`, `MathAST.h:447-459,646`), and the evaluation input
(`MathEvaluator::evaluate(const MathNode*)`, `MathEvaluator.h:109`). Layout is driven by the real
OpenType MATH table of STIX Two Math (56 constants, `stix_math_constants.h:37-94`;
`MathConstantsProvider`, `MathTypography.h:263-506`), a TeX 8-class inter-atom spacing table
(`MathTypography.h:96-106`), a 4-level style lattice (`MathStyle`, `MathTypography.h:45-50`), and a
Gecko-style extensible-glyph assembler (`MathGlyphAssembly.cpp:88-280`). Sums, products, definite
integrals, big operators with style-dependent limits, nth roots, stretchy delimiters (with vector
fallback), subscripts and periodic decimals all render today.

**What v2 is therefore NOT:** a rewrite, a new typesetter, or "make it like LaTeX." The MATH-table
engine is kept.

**What v2 IS:** five targeted architectural moves that the acceptance exemplars force:

1. **Coverage extension** — new node types the exemplars need and the current tree cannot express:
   identifiers beyond `char` (Greek Φ/α/β/γ/δ; `NodeVariable` stores a single `char`,
   `MathAST.h:1096`), factorial (zero support anywhere in `src/math` — grep-verified), `lim` with
   an under-limit, matrices/determinants (no matrix node in any of the three tree families;
   the "matrix" stress case is faked with fractions in brackets, `MathStressExpressions.cpp:103-116`),
   binomial coefficients, ellipsis/dots nodes, ∞ as a first-class symbol, combined sub+superscript.
2. **Capability honesty** — a per-node capability lattice (Renderable / Editable / Serializable /
   Evaluable / CAS-transformable) so that display-rich notation is never silently mis-evaluated.
   Today `MathEvaluator` hard-errors `"Math ERROR"` on `DefIntegral/Summation/BigOp/Subscript`
   (`MathEvaluator.cpp:533`) with no distinction from a genuine domain error.
3. **A canonical serialization (NumIR)** — the VPAM tree currently has **no serializer** (only the
   debug `dumpTree`, `MathAST.h:1421`; GrapherApp hand-rolls a lossy text flattener,
   `GrapherApp.cpp:1103-1264`). NumIR becomes the storage format, the test oracle format, and the
   alignment target for the CAS/Giac serializer proposed in
   `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md` §C.2.
4. **Layout lifecycle discipline** — memoized, invalidation-driven layout (today `onDraw`
   recomputes the full tree every frame, `MathRenderer.cpp:737`), bounded depth with graceful
   refusal (today layout recursion is uncapped — REL C5.1 — while draw caps at 12,
   `MathRenderer.h:239`), and a depth budget of 24 that exemplar B actually fits in (§9.2).
5. **Glyph pipeline honesty** — one `GlyphProvider` ladder (exact → size variant → assembly →
   vector synthesis → visible tofu + log) replacing today's three fallback paths that can end in
   "draw nothing" (`MathRenderer.cpp:184-283`, C5.7) or a silent `line_height/3` advance
   (`MathRenderer.cpp:2480-2483`).

Both acceptance exemplars are decomposed in §9. Exemplar A (Ramanujan) needs exactly two new node
kinds (factorial, ∞-capable identifier) plus NumIR and bounded-sum evaluation — it is a **Phase 1**
target. Exemplar B ("final boss") additionally needs matrix, binomial, limit, ellipsis and combined
scripts, plus the depth-budget raise — renderable end-to-end at **Phase 3**, with each subconstruct
landing incrementally and testable in isolation.

---

## 1. Current-state architecture map

### 1.1 The three engines that exist today [V]

| Engine | Tree / format | Numeric model | Consumers | Evidence |
|---|---|---|---|---|
| **VPAM** | `vpam::MathAST` (17 node kinds, `unique_ptr` children, raw parent ptrs, PSRAM `operator new`) | `ExactVal` (a·√b·π^m·e^n rationals) | CalculationApp, GrapherApp editors, EquationsApp, CalculusApp, IntegralApp, TutorApp; rendered by `MathCanvas` | `MathAST.h:72-90`, `MathAST.cpp:50-69`, `MathEvaluator.h:109` |
| **Legacy text** | `String` → `Tokenizer` → shunting-yard `Parser` → RPN token vector | `double` | GraphModel plotting, numeric `EquationSolver` | `Tokenizer.h:37-62`, `Parser.cpp:28-50`, `Evaluator.h:49` |
| **CAS** | `cas::SymExpr` (hash-consed arena DAG) + `PersistentAST` (rule engine) | `CASRational` + exact tags | OmniSolver, SystemSolver, SymDiff/Integrate/Simplify, tutors | `SymExpr.h:93-105`, `SymExprArena.h:63-64` |
| **Giac** (device-only) | text in / text out over serial | Giac internal | SerialBridge `:` commands only — zero app callers | `GiacBridge.cpp:390-448`, CGIAC §B |

Bridges: `ASTFlattener` (VPAM→SymExpr/SymPoly), `SymExprToAST` / `SymToAST` / `CasToVpam`
(CAS→VPAM for display; **uncapped**, REL C5.8), GrapherApp's private AST→text serializer
(`GrapherApp.cpp:1103-1264`).

### 1.2 What responsibilities are conflated today

The VPAM tree is **four things at once**:

1. **Semantic tree** — `MathEvaluator` walks it directly (`MathEvaluator.cpp:517-535`).
2. **Presentation tree** — every node carries `LayoutResult _layout` (`MathAST.h:646`) and a
   `calculateLayout(FontMetrics)` virtual (`MathAST.h:634`); layout constants and MATH-table
   policy live *inside the AST header* (`fractionPartMetrics`, `MathAST.h:314-326`;
   `fractionBarGaps`, `MathAST.h:575-588`).
3. **Edit tree** — `CursorController` mutates it structurally and stores cursor position as
   `{NodeRow*, int}` into it (`CursorController.h:62-85`).
4. **Interchange tree** — CAS results are converted *into* it for display (`SymExprToAST`),
   Grapher serializes *out of* it for the numeric pipeline.

This conflation is not all bad — it is why NumOS has WYSIWYG editing with zero
model-synchronization bugs. But it produces four concrete costs, each evidenced:

- **C-1 Geometry contract cycle**: layout metrics live in `MathAST.h` while drawing lives in
  `ui/MathRenderer.*`; "changing either side changes rendered geometry (and therefore goldens)"
  (`NUMOS_ARCHITECTURE_GROUND_TRUTH.md:167`).
- **C-2 Semantic gaps are invisible**: a node that renders beautifully (Summation) evaluates to
  `"Math ERROR"` (`MathEvaluator.cpp:533`) — indistinguishable from `tan(90°)`.
- **C-3 No storage/test format**: expressions cannot be saved, replayed, or asserted on except by
  pixels or by app-private flatteners (Grapher's, 63-char cap, `GrapherApp.cpp:1266-1275`).
- **C-4 Layout recomputed every frame**: `onDraw` runs `_root->calculateLayout(_fmNormal)`
  unconditionally (`MathRenderer.cpp:737`) — wasted work and a moving perf target as trees grow.

### 1.3 Where current limitations come from (root causes, not symptoms)

| Limitation | Root cause | Evidence |
|---|---|---|
| No Greek/multi-char identifiers | `NodeVariable` stores `char _name` with a closed 11-slot variable map | `MathAST.h:1096`, `VariableManager.h:138-145` |
| No factorial | No postfix concept in AST, parser, or evaluators | grep `factorial|"!"` in `src/math` = 0 hits |
| No matrices | No grid node in any tree family; MatricesApp bypasses VPAM with `lv_table` | `MatricesApp.cpp:294-298` |
| Tall delimiters are strokes, not glyphs | Font subset omits assembly glyphs U+239B…U+23B3 | `MathRenderer.cpp:57-61`, `MathGlyphAssembly.h:29-34` |
| Deep trees crash or clip | Layout recursion uncapped (C5.1); draw capped at 12 with red `...` box | `MathRenderer.h:239`, `MathRenderer.cpp:1507-1514`, REL C5.1 |
| Sums/integrals not computable | Evaluator dispatch `default:` errors; no iteration machinery | `MathEvaluator.cpp:533` |
| No persistence of rich expressions | No serializer on the tree | `MathAST.h:1421` (debug dump only) |
| Missing glyph = invisible | Fallback chains can end in skip / min-advance | `MathRenderer.cpp:2480-2483`, REL C5.7 |

---

## 2. V2 architecture

### 2.1 The core decision: one authored tree, derived semantic views (Special Question 1)

**Decision AD-1 [P]: NumOS keeps a single authored tree (the VPAM tree, role renamed to
"expression tree") serving edit + presentation, and derives semantic artifacts from it at
well-defined commit points, instead of maintaining two synchronized trees.**

Rationale (explicit tradeoff, per the quality bar):

- The *split-tree* design (NumWorks Poincaré `Layout` vs `Expression`; MathML presentation vs
  content) buys clean semantics at the cost of a bidirectional converter that must be total and
  bug-free — every editing feature is implemented twice (once in the layout tree, once in the
  mapping). NumOS today has **zero** model-sync bugs because there is nothing to sync.
- The *derivation* design keeps that property. NumOS **already** derives semantic views at commit
  points: `MathEvaluator.evaluate(root)` on ENTER (`CalculationApp.cpp:525-584`), `ASTFlattener`
  into `cas::SymExpr` for solvers, Grapher's text serializer for the plotting pipeline. V2
  formalizes these as the **semantic boundary** rather than inventing a fourth tree.
- Memory: a second persistent tree doubles per-expression PSRAM (fine) but also doubles pointer
  fix-up surface for history/undo (`cloneNode`, `MathAST.h:1429`) — real complexity on a codebase
  where every node mutation must maintain raw parent pointers.

What changes vs today is **separation of concerns inside the single tree**, not a second tree:

- Layout policy moves out of `MathAST.h` into a `layout/` unit (see companion layout spec §2) so
  the AST header stops being golden-fragile for non-geometric edits (addresses C-1).
- Every node kind declares a **capability set** (§2.3) so semantic gaps become typed refusals
  (addresses C-2).
- A canonical serializer (**NumIR**) is added as a peer of `dumpTree` (addresses C-3).
- Layout results become memoized-with-invalidation (addresses C-4).

Presentation-only constructs (ellipsis, decorative dots) become ordinary nodes whose capability set
simply excludes evaluation — they are not exiled to a separate tree.

### 2.2 Layer diagram [P]

```
                 ┌───────────────────────────────────────────────────────────┐
   input         │  Keypad / KeyboardManager / Template palette (MR-15)      │
                 └──────────────┬────────────────────────────────────────────┘
                                ▼ structural edits
                 ┌───────────────────────────────────────────────────────────┐
   expression    │  Expression tree  (vpam::MathAST, extended node set)      │
   layer         │  + CursorController (structural cursor, 2D nav)           │
                 │  + capability flags per node kind (mrCapabilities())      │
                 └───┬──────────────────────┬───────────────────┬────────────┘
                     │ serialize/parse      │ layout(request)   │ flatten (commit points)
                     ▼                      ▼                   ▼
        ┌────────────────────┐  ┌────────────────────────┐  ┌──────────────────────────┐
        │  NumIR text        │  │  Layout engine          │  │  Semantic boundary       │
        │  (canonical,       │  │  (pure, host-testable:  │  │  MathEvaluator (ExactVal)│
        │  round-trip,       │  │  MathConstantsProvider, │  │  ASTFlattener → SymExpr  │
        │  storage + tests)  │  │  spacing table, boxes,  │  │  → OmniSolver/SymDiff…   │
        └────────────────────┘  │  memoized LayoutResult) │  │  Giac text serializer    │
                                └───────────┬─────────────┘  │  (CGIAC §C.2 mapping)    │
                                            ▼                └──────────────────────────┘
                                ┌────────────────────────┐
                                │  GlyphProvider          │
                                │  (exact → variant →     │
                                │  assembly → vector →    │
                                │  tofu; single ladder)   │
                                └───────────┬─────────────┘
                                            ▼
                                ┌────────────────────────┐
                                │  MathCanvas (LVGL 9.5   │
                                │  widget; draw callbacks │
                                │  only, zero pixel bufs) │
                                └────────────────────────┘
```

### 2.3 Capability lattice [P]

Every `NodeType` (and, where a kind is parameterized, every parameter value — e.g. `FuncKind`)
declares a static capability set:

```cpp
// [P] MathAST.h — pure data, no behavior change by itself
enum MrCap : uint8_t {
    CAP_RENDER    = 1 << 0,  // layout + draw defined
    CAP_EDIT      = 1 << 1,  // cursor can enter/mutate
    CAP_SERIALIZE = 1 << 2,  // NumIR emit + parse defined
    CAP_EVAL      = 1 << 3,  // MathEvaluator produces ExactVal
    CAP_CAS       = 1 << 4,  // ASTFlattener / Giac serializer mapping defined
};
uint8_t mrCapabilities(const MathNode& n);   // total function, switch on type()
```

Invariants (normative; test hooks in the test-oracle companion):

- **T-1 [P]** `mrCapabilities` is **total**: every node kind returns a defined set; adding a
  `NodeType` value without a capability row is a compile-time error (exhaustive switch, no default).
- **T-2 [P]** A node without `CAP_EVAL` reaching `MathEvaluator` produces the typed refusal
  `EvalError::DisplayOnly` (user text "Not computable — display only"), never the generic
  `"Math ERROR"`, and never a silent wrong value. (Semantic-honesty rule; Special Question 13.)
- **T-3 [P]** A node without `CAP_CAS` reaching the Giac/SymExpr serializers is **refused**
  (J.2 class per CGIAC §C.2), never approximated.
- **T-4 [P]** `CAP_RENDER` is universal: everything insertable or CAS-producible must lay out and
  draw (possibly as tofu), because refusal at draw time has no user-visible channel.

### 2.4 New node kinds (summary; full taxonomy in the notation-surface companion)

| Node [P] | Purpose | Exemplar need |
|---|---|---|
| `NodeIdentifier` | Unicode-codepoint identifier: Greek letters, multi-char names, ∞; replaces/extends `NodeVariable` (kept as alias during migration) | A: ∞; B: Φ, α, β, γ, δ, s, n, k |
| `NodeFactorial` | Postfix factorial with captured base | A: (4k)!, (k!)⁴ |
| `NodeLimit` | `lim` word operator with under-script row | B: lim n→∞ |
| `NodeMatrix` | r×c grid of `NodeRow` slots + `DelimKind` fence (paren/bracket/bar) | B: 2×2 matrix; determinant bars |
| `NodeBinom` | Two stacked rows inside stretchy parens (no fraction bar) | B: binom(n,k) |
| `NodeEllipsis` | `cdots/ldots/vdots/ddots` leaf, display-only (`CAP_EVAL` absent) | B: ⋱ in cfrac, ⋯ in radical chain |
| `NodeScripts` | Combined sub+sup on one base (unifies `NodePower`+`NodeSubscript` placement per MATH constants) | B-adjacent; phase 2 |
| `DelimKind::Floor/Ceil` | ⌊⌋ ⌈⌉ delimiters (glyphs already in subset, range 0x2308-230B) | notation surface |
| `OpKind::Div, OpKind::To` | inline ÷ (result display), → arrow (limit bounds) | B: n→∞ |

Continued fractions deliberately get **no new node** — see Special Question 5 (§8) and layout spec
§7: semantic `NodeFraction` nesting plus the existing NumOS readable-fraction policy
(`fractionPartMetrics` keeps full glyph size at script level 0, `MathAST.h:314-326`) already
produces `\cfrac`-quality rendering; v2 adds only a depth budget and an ellipsis terminator.

### 2.5 Render pipeline (end-to-end, pseudocode) [P]

```text
# Edit-time (every keypress)
on_key(k):
    edit = CursorController.apply(k)             # structural mutation, PSRAM nodes
    if edit.refused_by_depth: show_depth_toast() # H-5 editor cap (see §5)
    invalidate_layout_chain(edit.mutated_node)   # walk parent ptrs, clear valid bits: O(depth)
    canvas.invalidate()                          # LVGL dirty

# Frame-time (LV_EVENT_DRAW_MAIN → MathCanvas::onDraw)
onDraw(layer):
    ctx = LayoutContext{ fm=_fmNormal, style, depth=0,
                         budget = LayoutBudget{maxDepth=24, maxNodes=4096} }
    if !root.layoutValid():                      # memoization — v2 change vs :737
        result = layout(root, ctx)               # bottom-up measure; may return DepthExceeded
        if result.overflow: draw_overflow_marker(); log("[LVGL] layout-depth ..."); return
    place(root, padding, scroll)                 # baseline math, §2d of renderer dossier holds
    draw(root, layer)                            # per-kind drawXxxBaseline, GlyphProvider ladder
    draw_cursor_if_editable()

# Glyph resolution (single ladder — GlyphProvider)
resolve(cp, targetHeightPx, font):
    if glyph_exists(cp, font) and height_ok:        return Exact(cp)
    if variant_table(cp): v = best_size_variant();  if v: return Variant(v)
    if assembly_valid(cp) and parts_in_font:        return Assembly(parts)
    if vector_synth_defined(cp):                    return VectorSynth(cp)   # (), [], {}, |, √, ±
    log_once("[LVGL] glyph-missing cp=%X")
    return Tofu(cp)                                  # visible box + hex, never nothing
```

### 2.6 Layout caching / invalidation (pseudocode) [P]

```cpp
// MathNode additions — 4 bytes, PSRAM-resident like the node itself (MP §1.2)
struct MathNode {
    ...
    LayoutResult _layout;      // existing, MathAST.h:646
    uint16_t _layoutKey = 0;   // fingerprint of (emSize, style, scriptLevel) it was computed for
    bool     _layoutValid = false;
};

uint16_t layoutKey(const FontMetrics& fm) {         // pure
    return (uint16_t(fm.emSize) << 4) | (uint16_t(fm.style) << 2) | fm.scriptLevel;
}

void invalidateChain(MathNode* n) {                 // called by every CursorController mutator
    for (; n; n = n->parent()) n->_layoutValid = false;   // O(depth) ≤ 24
}

void MathNode::layoutIfNeeded(const FontMetrics& fm, LayoutBudget& b) {
    const uint16_t k = layoutKey(fm);
    if (_layoutValid && _layoutKey == k) return;    // subtree reuse: untouched siblings skip
    if (!b.enter()) { markOverflow(); return; }     // depth/node budget, graceful (RT-10 shape)
    calculateLayout(fm /* recurses via layoutIfNeeded on children */);
    _layoutKey = k; _layoutValid = true;
    b.leave();
}
```

Invalidation triggers (exhaustive): cursor mutation (chain), `setMathStyle` (whole tree — key
mismatch handles it implicitly), font/em change (key mismatch), highlight change (**no**
invalidation — color only), scroll (**no** invalidation — placement only). The key-mismatch design
means style/em changes need no explicit tree walk: stale keys lazily recompute. A node whose
subtree is untouched but whose *position* changed re-places without re-measuring — placement is
already separate from measurement in the draw path (`drawNodeWithLayout` must not call
`calculateLayout`, `MathRenderer.h:265-266` [V]).

**Correctness caveat (normative):** `NodeRow` layout consults *sibling* math classes for
inter-atom spacing (`MathAST.cpp:171-217`), so a child mutation must invalidate the **parent row**,
not just the child — the parent-chain walk above guarantees this by construction.

### 2.7 Internal API surface [P]

```cpp
// ── layout/ (new unit; pure, host-testable, no LVGL) ─────────────────────────
struct LayoutBudget { int16_t maxDepth; int32_t maxNodes; /* enter()/leave() */ };
struct LayoutContext { const FontMetrics& fm; LayoutBudget& budget; };
LayoutResult layoutNode(MathNode& n, LayoutContext& ctx);       // memoized entry
void invalidateChain(MathNode* n);

// ── serialization (new unit; see input/serialization companion §3) ───────────
namespace numir {
  bool   emit(const MathNode& root, std::string& out);          // total for CAP_SERIALIZE trees
  ParseResult parse(const char* text, NodePtr& out);            // total: ok | typed error
}

// ── capability / semantics ───────────────────────────────────────────────────
uint8_t mrCapabilities(const MathNode& n);
enum class EvalRefusal : uint8_t { None, DisplayOnly, UnboundedDomain, DepthExceeded };
EvalRefusal evalAdmissible(const MathNode& root);               // pre-flight before evaluate()

// ── glyphs (consolidates MathGlyphAssembly + drawDelimiterGlyph ladders) ─────
struct GlyphPlan { enum Kind { Exact, Variant, Assembly, Vector, Tofu } kind; ... };
GlyphPlan planGlyph(uint32_t cp, int16_t targetHeightPx, const lv_font_t* font);

// ── MathCanvas additions (ui/) ───────────────────────────────────────────────
void MathCanvas::setViewport(int16_t maxHeightPx);              // enables vertical scroll (MR-17)
void MathCanvas::scrollToCursor();                              // exists implicitly today; formalized
#ifdef NATIVE_SIM                                               // GHOOK pattern, zero fw footprint
const LayoutResult& MathCanvas::debugRootLayout() const;
int MathCanvas::debugNodeCount() const; int MathCanvas::debugMaxDepth() const;
#endif
```

---

## 3. Layering boundaries (normative)

- **B-1 [P]** `src/math/MathAST.*` owns structure + capabilities only. Geometry policy
  (`fractionPartMetrics`, `fractionBarGaps`, `superscriptShiftMetrics`, large-op helpers — today at
  `MathAST.h:314-588`) migrates to `src/math/layout/` in ticket MR-05. During migration the values
  must be **bit-identical** (golden-frozen refactor).
- **B-2 [V→P]** `src/math/**` stays LVGL-free and Arduino-free (already true for the layout core,
  `MathTypography.h:23`; preserved as a hard rule so host tests compile with plain g++ like
  `tests/host/keycode_digit_test.cpp`).
- **B-3 [P]** `ui/MathRenderer.*` (MathCanvas) contains **drawing only**: no metric decisions
  beyond consuming `LayoutResult` + `GlyphPlan`. The duplicate header pair
  `src/ui/MathTypography.h` vs `src/math/MathTypography.h` (known confusion, GT P-15) is resolved
  by renaming the UI shim to `MathFontBinding.h` (MR-05).
- **B-4 [P]** Apps never reach into layout internals; they interact via `MathCanvas` +
  `CursorController` + `numir` + `evalAdmissible/evaluate`. (GrapherApp's private serializer is
  replaced by `numir` emit + a NumIR→legacy-text adapter in MR-03.)
- **B-5 [V]** Semantic boundary crossings happen only at commit points (ENTER / solve / plot), never
  per-frame.

---

## 4. Invariants (exact list)

Renderer/notation invariant family **Y-n** (layout), **T-n** (tree/capability, §2.3), **S-n**
(serialization, defined in the input/serialization companion). Cross-referenced families: REL
H-5/G-2/M-1..M-5, DIAG closed tag set, MP §1-§5.

- **Y-1 [V]** All layout geometry derives from `MathConstantsProvider` pixels or documented NumOS
  policy functions — no new magic pixel constants (existing discipline, `MathTypography.h:242-506`).
- **Y-2 [V]** Baseline convention: `ascent`/`descent` positive from baseline; row children align
  baselines via `rowChildTopFromRowTop` (`MathAST.h:533-538`). Every new node type must define its
  baseline in these terms.
- **Y-3 [P]** Layout depth ≤ **24** enforced by `LayoutBudget`; breach → fixed-size "⚠" box +
  `[LVGL] layout-depth` log line (aligns RT-10). Draw depth cap raised 12→24 in the same ticket so
  draw can never out-recurse layout. Editor insertion cap stays **16** (REL H-5) for hand input;
  template/CAS-built trees may reach 24 (they are constructed, not keyed).
- **Y-4 [P]** Layout node budget ≤ **4096** nodes per canvas; breach → same "⚠" refusal. (Bounds
  `SymExprToAST` output too — closes C5.8 from the display side.)
- **Y-5 [P]** Zero heap allocation in the draw path (measure: existing stress diagnostic asserts
  heap delta 0 per render, `MathRenderStressDiagnostics.cpp:75-128` — extended to v2 node kinds).
- **Y-6 [P]** Layout is deterministic and platform-independent: same tree + same `FontMetrics` ⇒
  identical `LayoutResult` on host and device (basis of the geometry oracle in the test plan).
- **Y-7 [P]** Memoization is semantics-free: disabling the cache (`_layoutValid=false` always)
  must produce pixel-identical output (cache-off CI mode in test plan §6).
- **Y-8 [P]** No glyph resolves to "nothing": the GlyphProvider ladder terminates in visible tofu.
- **Y-9 [V]** MathCanvas owns zero pixel buffers (pure draw callbacks, MA:103); v2 must not add
  any (canvas caching of rendered math is **rejected** — it would collide with the 64 KB LVGL pool
  and MP §1.1/§3).
- **T-1…T-4** capability invariants, §2.3.
- **S-1…S-5** serialization invariants, input/serialization companion §3.4.

---

## 5. Failure model

Reuses the J-class taxonomy (CGIAC §J / CSEM) and REL C5 rows; every failure has a defined channel.

| Failure | Detection point | Behavior [P unless noted] | Log tag |
|---|---|---|---|
| Unsupported symbol typed/imported | NumIR parse / template insert | typed parse error, cursor unchanged | `[INPUT]` |
| Node w/o CAP_EVAL evaluated | `evalAdmissible` pre-flight | `EvalRefusal::DisplayOnly`, result band shows "display only" chip, no history entry | `[APP]` |
| Missing glyph | GlyphProvider ladder end | visible tofu box + hex cp; render continues | `[LVGL] glyph-missing` |
| Oversized expression (width) | placement | horizontal scroll [V] (`MathRenderer.cpp:762-782`) |  |
| Oversized expression (height) | placement vs viewport | vertical scroll (MR-17); until then: clip **with** on-screen "⋮" affordance |  |
| Deep nesting (edit) | CursorController insert | refusal ≥16, toast; [V for Grapher `MAX_NEST=8`, `GrapherApp.cpp:2547-2593`; CalculationApp ungated today — closed by MR-01] | `[INPUT]` |
| Deep nesting (layout) | LayoutBudget | ⚠ box at depth 24, subtree not measured | `[LVGL] layout-depth` |
| Node-count bomb (CAS result) | LayoutBudget + `SymExprToAST` cap | ⚠ box; CAS result additionally truncated at flattener with typed error | `[CAS]` |
| OOM (node alloc) | `MathNode::operator new` throws `bad_alloc` [V] (`MathAST.cpp:58`) | edit-op wrapper catches at CursorController boundary, reverts op, toast "memory full"; never partial trees | `[MEM]` |
| Layout overflow (int16) | width/ascent accumulation | saturating add helpers in layout unit; saturated boxes render clipped, never wrap negative | `[LVGL]` |
| Semantic/display mismatch | construction-time | impossible by construction: one tree, capabilities total (T-1/T-2) |  |
| Parser/display disagreement | NumIR round-trip | S-invariant `parse(emit(t)) ≡ t` enforced in CI on every corpus tree | `[DIAG]` |

---

## 6. Performance model

- **Hot path**: keypress → structural edit (O(1) node ops) → chain invalidation (O(depth)≤24) →
  frame layout of dirty subtree only → draw. Frame budget **16 ms** (`kFrameBudgetUs=16000`,
  `MathRenderStressDiagnostics.cpp` [V]); layout share target ≤ 8 ms for a 4096-node worst case,
  ≤ 1 ms for typical (<200 node) calculator expressions.
- **Layout caching**: §2.6. Expected effect: steady-state frames (cursor blink — a timer
  invalidate every 500 ms, `MathRenderer.h:235`) do **zero** measurement work vs full-tree today.
- **Subtree cache invalidation**: parent-chain only; sibling subtrees keep valid layouts. Row
  spacing correctness preserved by invalidating the containing row (§2.6 caveat).
- **Incremental re-layout**: no partial-row diffing in v2 (complexity not justified at ≤4096
  nodes); the unit of recompute is the dirty subtree + its ancestor rows.
- **Memory ownership**: nodes PSRAM (`MathAST.cpp:50-69` [V], MP §1.2); layout stack is C stack
  bounded by depth 24 (~200 B/frame ⇒ ≤ ~5 KB of the 64 KB loopTask stack — measured in MR-05
  against REL M-5 floor of 8 KB headroom); GlyphPlan/assembly are fixed stack arrays [V]
  (`MathGlyphAssembly.h:64-75`); no per-frame heap (Y-5).
- **Draw cost**: unchanged model — LVGL immediate draw calls; the new tofu path is one rect + 4
  hex glyphs. Assembly-glyph delimiters (MR-11) replace N `lv_draw_line` strokes with ≤8 glyph
  blits — cost-neutral.

---

## 7. ESP32-specific constraints (binding on all MRV2 tickets)

- **RAM domains**: internal ≈211.8 KB free heap, PSRAM ≥7.5 MB free (MA:150-176). All new
  allocations follow MP §1: nodes/history/serialized text → PSRAM; nothing new in internal; no
  bulk ≥4 KB internal fallback (MP §1.4).
- **LVGL pool**: 64 KB builtin, ≤32 KB per app tree, ≥12 KB slack (MP §3). MathCanvas adds no
  widgets per node [V] — v2 keeps drawing immediate-mode; the template palette (MR-15) must budget
  its widget tree like EquationsApp's `canAllocStep()` gate (`EquationsApp.cpp:486-503`).
- **Font flash budget**: current STIX trio ≈ 2.7 MB of the 6.25 MB app slot with **1.35 MB
  headroom** (MA:173-177). Adding assembly glyphs U+239B…U+23B3 (~26 glyphs × 3 sizes) is
  estimated ≤ 40 KB — affordable. A fourth em size is **not** (≥ 400 KB) — rejected; v2 keeps the
  18/12/8 trio (`MathRenderer.h:159-161`).
- **No lv_canvas** (`LV_USE_CANVAS 0`, MA:123): rendered-math caching to bitmaps is off the table;
  memoize **measurements**, not pixels.
- **Redraw**: full-screen single 32 KB DMA draw buffer discipline unchanged (CH:25, MP §1.1);
  MathCanvas must keep drawing through `lv_draw_*` only.
- **Stack**: everything runs on the shared 64 KB loopTask stack (MA:36); depth budgets in §4 are
  sized against it and verified with `uxTaskGetStackHighWaterMark` probes (`[MEM]` line, MEMA §6).

---

## 8. Answers to the special questions (index)

| # | Question | Answer | Where |
|---|---|---|---|
| 1 | One tree or split? | One authored tree + derived semantic views at commit points (AD-1) | §2.1 |
| 2 | Large operators: semantic, presentation, or both? | **Both in one node**: `NodeSummation/NodeBigOp/NodeDefIntegral` stay single nodes carrying semantic children (bounds/body) *and* style-dependent presentation; evaluability is a capability bit, not a node split | §2.3, notation spec §K |
| 3 | det(matrix) vs determinant box distinct? | Two display forms, one semantics: `det`-as-function (`NodeFunction` ext.) and `NodeMatrix` with `DelimKind::Bar`; both serialize to `det(matrix(...))` | input spec §5.4 |
| 4 | Matrix representation | `NodeMatrix`: row-major `std::vector<NodePtr>` of NodeRow slots, dims ≤ 6×6, fence kind | notation spec §M, layout spec §9 |
| 5 | Continued fractions | Semantic `NodeFraction` nesting; **no dedicated node**; display quality already guaranteed by `fractionPartMetrics` policy [V]; `NodeEllipsis` terminates the infinite form | layout spec §7 |
| 6 | ∞, ⋯, ⋱, repeating radicals | ∞ = `NodeIdentifier` reserved symbol, evaluable only as a bound; dots = `NodeEllipsis` display-only (no CAP_EVAL); repeating radicals = ordinary nesting + ellipsis, display-only as a whole | notation spec §O/§P |
| 7 | Greek/styled identifiers | `NodeIdentifier{cp, IdentKind}`; semantics = user variable (extended VariableManager map) or reserved constant | notation spec §C, input spec §5.2 |
| 8 | LaTeX subset import/export | **Export only** (debug/docs, MR-20, optional); import rejected — NumIR is the canonical text; cost/benefit table | input spec §4.3 |
| 9 | Degrade on missing glyph | 5-rung GlyphProvider ladder ending in visible tofu (Y-8) | §2.5, layout spec §12 |
| 10 | Support non-evaluable notation? | Yes — capability lattice makes it explicit and safe (T-2) | §2.3 |
| 11 | CalculationApp exposure | Template palette groups by capability; "display only" chip on result band; refusal ≠ error styling | input spec §7 |
| 12 | How much TeX machinery? | Keep (already built): 8-class spacing, style lattice, MATH constants, italic correction. Skip: cramped styles, math kerning cut-ins, \mathchoice, penalty line-breaking, auto display/text switching. Rationale + cost table | layout spec §14 |
| 13 | Preventing rich-display/lossy-semantics mismatch | One tree + total capabilities + typed refusals + round-trip S-invariants | §2.3, §5 |
| 14 | Storing/testing oversized expressions | NumIR text is viewport-independent; geometry oracle asserts on `LayoutResult`, not pixels; goldens only for viewport-sized cases | test plan §5 |
| 15 | Minimum architecture for A solid + B feasible | Phase 1 set {NumIR, NodeIdentifier, NodeFactorial, bounded-sum eval, GlyphProvider, budgets} ⇒ A; + Phase 2/3 set {Matrix, Binom, Limit, Ellipsis, Scripts, depth 24, vertical scroll} ⇒ B | §9, tickets doc |

---

## 9. Acceptance exemplars

### 9.1 Exemplar A — Ramanujan 1/π

```
1/π = (2√2 / 9801) · Σ_{k=0}^{∞} ( (4k)!·(1103+26390k) ) / ( (k!)⁴·396^{4k} )
```

| Subconstruct | Node(s) | Status | Phase |
|---|---|---|---|
| Top-level fraction 1/π | `NodeFraction(NodeNumber, NodeConstant::Pi)` | [V] renders today | — |
| Relation `=` | `NodeRelation(Eq)` | [V] | — |
| 2√2 / 9801 | `NodeFraction(Row[Number,Root], Number)` | [V] | — |
| Σ with k=0 lower, ∞ upper, display limits | `NodeSummation` [V] + ∞ upper bound needs `NodeIdentifier(∞)` [P] | partial | 1 |
| (4k)! and (k!)⁴ | `NodeFactorial` [P] (inside `NodeParen`, under `NodePower`) | missing | 1 |
| 1103 + 26390k grouped numerator | `NodeParen(Row[...])` | [V] | — |
| 396^{4k} compound exponent | `NodePower` with Row exponent | [V] | — |
| Big fraction inside Σ body at display style | readable-fraction policy [V] | — | — |
| **Evaluation** (finite truncation k=0..N) | bounded Σ eval [P], factorial eval [P] | missing | 1 |
| **Evaluation** (infinite bound) | refusal `UnboundedDomain` [P]; Giac route future | by design | 3+ |

Depth audit: max boxed depth ≈ 9 (Row→Sum→Row→Frac→Row→Power→Row→Factorial→Row) — inside today's
12 and trivially inside 24. Width at em 18 ≈ 300–340 px ⇒ horizontal scroll engages [V].
**Conclusion: Exemplar A is fully renderable and (truncated) evaluable at end of Phase 1.**

### 9.2 Exemplar B — "final boss"

Required subconstructs and their node mapping:

| Subconstruct | Node(s) | Phase |
|---|---|---|
| Φ identifier | `NodeIdentifier(U+03A6)` | 1 |
| lim with n→∞ under-script | `NodeLimit` (word-op + under row) + `OpKind::To` + ∞ | 2 |
| Giant bracketed group `[ ... ]` | `NodeParen(DelimKind::Bracket)` [V]; needs tall-delimiter quality (vector fallback [V], real assembly glyphs MR-11) | 0/2 |
| ∫ from 0 to π/2 | `NodeDefIntegral` [V]; π/2 bound = `NodeFraction` in upper slot [V] | — |
| Display-style Σ in the numerator | `NodeSummation` [V] with explicit-display flag (layout spec §8.4) | 2 |
| binom(n,k) | `NodeBinom` [P] | 2 |
| (−1)^{k+1} / k^s | `NodePower`+`NodeFraction` [V] + identifier s | 1 |
| √ of continued-fraction tower | `NodeRoot` + nested `NodeFraction`×3 [V policy] + `NodeEllipsis(ddots)` [P] | 2 |
| Superscript of the whole bracket = det(matrix) | `NodePower(base=NodeParen, exp=Row[det, NodeMatrix])` — power-of-paren [V]; `det` label + `NodeMatrix` [P] | 2 |
| 2×2 matrix with Greek entries | `NodeMatrix` + `NodeIdentifier` | 2 |
| + √(2+√(2+√(2+√(2+⋯)))) | nested `NodeRoot` [V] + `NodeEllipsis(cdots)` [P] | 2 |
| Whole formula composed, on-device, navigable | depth 24 budget + vertical scroll + width scroll | 3 |
| **Semantics** | display-only as a whole (ellipsis ⇒ no CAP_EVAL); det/matrix/finite-Σ subtrees individually evaluable; limit/∫ CAS-route future | 3/future |

Depth audit (worst branch): Row(1) NodePower(2) NodeParen(3) Row(4) NodeDefIntegral(5) Row(6)
NodeFraction(7) Row(8) NodeRoot(9) Row(10) NodeFraction(11) Row(12) NodeFraction(13) Row(14)
NodeFraction(15) Row(16) NodeEllipsis(17) ⇒ **17 levels** — exceeds today's draw cap 12 (red `...`
box would fire, `MathRenderer.cpp:1507-1514`) and today's *uncapped* layout recursion would
silently deep-recurse (C5.1). This single number justifies invariant Y-3 (budget 24) and ticket
MR-05. Height at em 18 ≈ 210–260 px ⇒ exceeds the 209 px content area (`CalculationApp.cpp:44-59`)
⇒ vertical scroll (MR-17) is a hard requirement for B, not a nicety.

**Conclusion: every part of B is architecturally covered; rendering completeness lands at Phase 3
(MR-18 gate); whole-formula evaluation is explicitly out of scope (display-only) with individually
evaluable subtrees.**

---

## 10. Migration strategy (v1 → v2 without breaking the world)

Renderer files are the most golden-fragile surface in the repo ("any px change breaks byte-exact
goldens; blast radius all 20 goldens", GT §L#1; renderer = Fable/Opus lane, GT:395). Migration is
therefore staged as **geometry-frozen refactors** alternating with **geometry-changing feature
waves**, each wave ending in a candidate-regen + human re-bless cycle:

1. **Observability first (MR-01/02, Phase 0)** — depth/node counters, debug accessors, host layout
   harness, NumIR skeleton. Byte-identical goldens required (pure additions).
2. **Freeze-refactor (MR-05/06)** — move layout policy out of `MathAST.h`, introduce memoization
   and GlyphProvider **with outputs proven bit-identical** (cache-off A/B, host geometry snapshots,
   candidate diff must be empty). Goldens must NOT need re-blessing; any diff is a bug.
3. **Additive nodes (MR-07…MR-14)** — new node kinds cannot change existing trees' geometry;
   goldens stay green; new *warning-only* candidates cover new notation until blessed.
4. **Geometry-changing improvements (MR-11 assembly delimiters; any spacing fixes)** — batched,
   each with explicit re-bless in the PR (REN-1 protocol).
5. **App integration (MR-15…MR-17)** and **exemplar gates (MR-10, MR-18)**.

Rollback unit = one ticket = one PR; every ticket in the tickets companion carries its own
rollback note and "must stay byte-identical" list.

---

*End of Document 1. Companion documents listed in the header; implementation order in
`NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md`.*
