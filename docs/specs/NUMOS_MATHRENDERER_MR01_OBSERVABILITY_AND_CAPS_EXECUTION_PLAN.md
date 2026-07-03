# NumOS — MR-01 Execution Plan: Renderer Observability + Editor/Layout Caps

> **Exact implementation plan** for ticket MR-01 (`NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md:47-64`).
> Written to be executed mechanically by an Opus/Sonnet-class implementer with **zero architecture
> invention**: every file, function, type, constant, command grammar, log line, test and command is
> specified here. Verified against `b234e15` (2026-07-03); all `file:line` cites re-checked in this audit.
>
> Governing decisions: ADR-MR00-03 (budgets), ADR-MR00-05 (fallback counters), ADR-MR00-06
> (metrics fixture), ADR-MR00-07 (golden policy) in `NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md`.
> Conventions inherited: GHOOK assert-hook pattern (`NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md` §A-§B —
> append-only `.numos` grammar, parse-validated at load, exit codes 0/2/3/4, PASS/FAIL verdict
> grammar), closed DIAG tag set (`NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md` §2.1).

---

## A. Executive summary

MR-01 makes the renderer **measurable** and the editor **bounded**, changing zero pixels:

1. **Pure tree statistics** (depth, node count, fraction/script nesting, digit maxima) as
   allocation-free free functions in the math layer — used by the editor caps on firmware and by
   debug accessors on the emulator.
2. **Editor insertion caps** in `CursorController` (depth 16 / nodes 512 / digits 32,
   ADR-MR00-03) with pre-mutation refusal, `[INPUT]` logs and a CalculationApp toast — closing
   REL C5.2/C5.3 for CalculationApp (Grapher already gates at `MAX_NEST=8`,
   `GrapherApp.cpp:2546-2593`, and is untouched).
3. **NATIVE_SIM observability** on `MathCanvas` + `CalculationApp`: root layout, tree stats,
   layout-pass counter, draw-depth high-water, missing-glyph and vector-delimiter counters,
   budget-exceeded detection flags, and a font-metrics dump (the MR-02 fixture source).
4. **Eight appended `.numos` commands** (assert grammar below) + six new scripts.
5. **Explicitly NOT in MR-01**: layout refusal (⚠ box), draw-cap raise 12→24, memoization
   (all MR-05); NumIR / `type_numir` / `assert_math_serialized` (MR-03); eval refusal chips
   (MR-09); any new node kind; any pixel change. MR-01 *detects* layout-budget breaches
   (depth>24 / nodes>4096) and logs them; it does not act on them.

Acceptance: all 19 goldens byte-identical; new asserts green in CI; firmware
(`esp32s3_n16r8`) compiles with flash delta ≤ +2 KB.

---

## B. Current source audit (what exists, verified)

| Concern | Where it happens today | Evidence |
|---|---|---|
| **Layout (measure)** | `MathNode::calculateLayout(const FontMetrics&)` pure virtual; per-kind overrides; result cached in `_layout` per node | `MathAST.h:634`, `MathAST.h:646`; e.g. `NodeRow::calculateLayout` `MathAST.cpp:171-217`, `NodeFraction` `:372-425` |
| Layout call sites (full-tree recompute) | `MathCanvas::setExpression` (`MathRenderer.cpp:591`), `setMathStyle` (`:629`), `onDraw` **every frame** (`:737`), external-entry `drawNode` (`:1476-1477`), app-side `refreshExpression` (`CalculationApp.cpp:211-215`), `evaluateExpression`/FACT result relayout (`CalculationApp.cpp:497`) | cited |
| **Draw** | `MathCanvas::onDraw` → `drawNodeWithLayout` (top→baseline conversion, `MathRenderer.cpp:1481-1493`) → `drawNodeBaseline` dispatch (`:1495-…`) → per-kind `draw*Baseline` | `MathRenderer.cpp:724-889` |
| **Recursion (unbounded)** | layout: no depth parameter anywhere (REL C5.1); evaluator: `MathEvaluator::evaluate` recursive, no guard (C5.4, `MathEvaluator.cpp:517-535`) | cited |
| **Draw cap (exists)** | `MAX_RENDER_DEPTH = 12`; depth overflow draws red 60 %-opacity box + `"..."` and stops recursing | `MathRenderer.h:239`, `MathRenderer.cpp:1507-1514` |
| **Layout caps (do not exist)** | — | grep `depth` in `MathAST.cpp` layout paths: none (only the cursor-role classifier caps its own walk at 16, `MathAST.cpp:1836`) |
| **Editing inserts nodes** | `CursorController::insertDigit` (`CursorController.cpp:320`), `insertOperator` (`:350`), `insertFraction` (`:365`), `insertPower` (`:418`), `insertRoot` (`:461`), `insertParen` (`:482`), `insertFunction` (`:961`), `insertLogBase` (`:985`), `insertConstant` (`:1008`), `insertVariable` (`:1023`); capture set `isCapturable` (`CursorController.h:244`); **no cap anywhere** (grep-verified) | cited |
| Digit growth unbounded | `NodeNumber::appendChar` appends without length check | `MathAST.cpp:287-293` (C5.3) |
| **CalculationApp evaluate/render** | key dispatch `handleKey` (`CalculationApp.cpp:225-519`; inserts `:326-429` uncapped); `evaluateExpression` (`:525-584`; history clone `:557-567`, cap `MAX_HISTORY=50` `CalculationApp.h:138`); content area 209 px (`CalculationApp.cpp:44-59`); canvases created in `createUI` (`:141-186`, TEXT style `:161`) | cited |
| **Emulator script accessors (precedent)** | `CalculationApp::debugHasResult/debugLastResult` in a `#ifdef NATIVE_SIM` block (`CalculationApp.h:95-107`); `MainMenu::debug*` hooks; consumed by `scriptStepBegin` (`NativeHal.cpp:1697-1850`) | cited |
| **Script grammar host** | `loadScript` parse-validates whole file, exit 2 on error (`NativeHal.cpp:1457-1622`, terminal else `:1612-1614`, `main` `:1882-1884`); `ScriptCmdType` enum (`:1345-1362`), `ScriptCmd{type,key,waitN,strArg,line}` (`:1364-1373`); verdicts `[ASSERT] <script>:<line>: PASS/FAIL - msg` (`:1682-1693`); one command per deterministic frame (`:1697-1703`) | cited |
| Glyph fallback paths | delimiter chain `drawDelimiterGlyph` (`MathRenderer.cpp:184-283`), vector strokes (`:87-172`), piece-availability check (`:174-182`, `:238-241`), silent piece skip (`:243-245`); text-run miss → invisible `line_height/3` advance (`:2480-2483`); boot probe of U+239C → `g_delimiterAssemblyRenderable` (`:431-435`; default false `MathGlyphAssembly.cpp:15-18`) | cited |
| FontMetrics source | `MathCanvas::metricsFromFont` probes LVGL glyphs (`MathRenderer.cpp:446-535`); wired for 18/12/8 px in ctor (`:417-425`, fonts `MathRenderer.h:159-164`) | cited |
| Existing-script depth headroom (why caps are golden-safe) | deepest structure any current script types: one fraction inside a paren / logbase (`calc_delimiters_smoke`, `calc_logbase_smoke`) — structural depth ≤ 5, node count ≤ ~40, digits ≤ 20 (`calc_input_extended`); far below 16/512/32 | audit of `tests/emulator/scripts/*.numos` (71 scripts) |

---

## C. Exact file-touch list

Only these seven paths change. Anything else in the diff is a review-blocker.

### C.1 `src/math/MathAST.h` + `src/math/MathAST.cpp` — pure tree statistics (unconditional)

- **Target**: new free functions at the end of the header (after `cursorTargetRoleName`,
  `MathAST.h:1463`) + impl at the end of `MathAST.cpp` (after `cursorTargetRoleName`,
  `MathAST.cpp:1978`).
- **Change**:

```cpp
// MathAST.h — [MR-01] Allocation-free tree statistics. Iterative walk with a
// fixed explicit stack; safe on trees deeper than any budget (never recurses).
struct MathTreeStats {
    int16_t maxDepth = 0;          // node depth in parent hops; root = 0
    int32_t nodeCount = 0;         // nodes visited (root included)
    int16_t maxFractionNesting = 0;// max # of NodeFraction ancestors incl. self
    int16_t maxScriptNesting = 0;  // max # of Power/Subscript ancestors incl. self
    int16_t maxNumberDigits = 0;   // longest NodeNumber value length
    bool    walkSaturated = false; // tree deeper than kStatsWalkMaxDepth
};
constexpr int16_t kStatsWalkMaxDepth = 64;
MathTreeStats mathTreeStats(const MathNode* root);
int16_t nodeDepthFromRoot(const MathNode* n);      // parent-pointer hops; O(depth)
int16_t subtreeMaxRelDepth(const MathNode* n);     // deepest descendant, relative (leaf = 0)
```

- Walker contract: explicit stack `struct Frame { const MathNode* n; int child; }
  stack[kStatsWalkMaxDepth]`; children below depth 64 are not visited and set
  `walkSaturated=true` with `maxDepth` clamped to 64. Zero heap, O(nodeCount) time.
- **Reason**: caps (firmware) and accessors (emulator) share one implementation; host-testable
  (this TU already compiles standalone with g++ — verified in this audit).
- **Risk**: Low (pure additions).
- **No-golden proof**: no existing code path calls these; no geometry touched.

### C.2 `src/math/CursorController.h` + `src/math/CursorController.cpp` — editor caps

- **Targets**: class `CursorController`; every structural inserter listed in §B; `insertDigit`.
- **Change** (header, public):

```cpp
// [MR-01] Editor insertion budget (ADR-MR00-03 / REL H-5, C5.2, C5.3).
enum class InsertRefusal : uint8_t { None, Depth, Nodes, Digits };
static constexpr int16_t kMaxEditorDepth   = 16;
static constexpr int32_t kMaxEditorNodes   = 512;
static constexpr int16_t kMaxNumberDigits  = 32;
InsertRefusal lastInsertRefusal() const { return _lastRefusal; }
void clearInsertRefusal() { _lastRefusal = InsertRefusal::None; }
```

  private helpers + member `InsertRefusal _lastRefusal = InsertRefusal::None;`:

```cpp
int16_t cursorRowDepth() const;   // parent hops from _cursor.row to root (root row = 0)
                                  // same walk Grapher uses (GrapherApp.cpp:2547-2552)
bool guardStructuralInsert(const MathNode* captured); // sets _lastRefusal, returns allow
bool guardLeafInsert();                               // node-count only
```

- **Exact guard rule** (normative — implements the tree invariant "resulting max node depth
  ≤ 16" from H-5):
  - Let `d = cursorRowDepth()`. A structural insert places the new node at depth `d+1`, its
    slot rows at `d+2`, and their `NodeEmpty` (or the captured subtree root) at `d+3`.
  - `projected = d + 3 + (captured ? subtreeMaxRelDepth(captured) : 0)`
    (capture moves the left operand from depth `d+1` to `d+3`, i.e. +2 —
    `insertFraction` capture semantics, `CursorController.h:119-133`).
  - Refuse (`InsertRefusal::Depth`) iff `projected > kMaxEditorDepth`.
  - Additionally refuse (`InsertRefusal::Nodes`) iff
    `mathTreeStats(rootRow()).nodeCount + kInsertNodeHeadroom > kMaxEditorNodes`
    with `kInsertNodeHeadroom = 4` (largest single insert: NodeDefIntegral is not editor-
    insertable; fraction = Fraction + 2 rows + ≤2 empties ⇒ 4 new nodes net of capture).
  - `insertDigit`: when extending an existing `NodeNumber`, refuse (`Digits`) iff
    `length() >= kMaxNumberDigits` **before** calling `appendChar`
    (`MathAST.cpp:287-293` itself stays untouched); when creating a new number, apply
    `guardLeafInsert()`.
  - `insertOperator/insertConstant/insertVariable`: `guardLeafInsert()` only.
  - `backspace`, `moveLeft/Right/Up/Down`: **never** guarded.
  - On refusal: **return before any mutation** — tree unchanged, cursor unchanged. Refusal state
    persists until the next insert attempt or `clearInsertRefusal()` (so the app and the script
    assert can both read it).
- **Reason**: closes C5.2/C5.3 at the single choke point every app shares.
- **Risk**: Medium — the capture-depth formula is the only nontrivial logic in MR-01; the
  boundary cases in §I pin it. GrapherApp behavior unchanged (its `MAX_NEST=8` fires first at
  depth 8 < 16; unification is MR-16).
- **No-golden proof**: guards only refuse; §B's audit shows no existing script reaches depth 16 /
  512 nodes / 32 digits, so every existing script's tree is bit-identical.

### C.3 `src/ui/MathRenderer.h` + `src/ui/MathRenderer.cpp` — NATIVE_SIM observability only

- **Targets**: class `MathCanvas` (header additions after `metricsFromFont`,
  `MathRenderer.h:147`); `onDraw` (`MathRenderer.cpp:724`); `drawNodeBaseline`
  (`:1495`); `drawTextBaseline` miss branch (`:2480-2483`); `drawStrokedDelimiter` call sites
  (`:196`, `:219`, `:239`).
- **Change** (all inside `#ifdef NATIVE_SIM`; firmware strips everything):

```cpp
// MathRenderer.h — [MR-01] emulator-only observability (GHOOK pattern; zero fw footprint)
#ifdef NATIVE_SIM
public:
    const LayoutResult* debugRootLayout() const;          // nullptr when !_root
    MathTreeStats debugTreeStats() const;                 // mathTreeStats(_root)
    uint32_t debugLayoutPassCount() const;                // full-tree relayouts since create()
    int      debugDrawDepthHWM() const;                   // deepest drawNodeBaseline last draw
    uint16_t debugMissingGlyphCount() const;              // text-run misses, last completed draw
    uint16_t debugVectorDelimCount() const;               // stroked delimiters, last completed draw
    bool     debugLayoutBudgetExceeded() const;           // detection only: depth>24 || nodes>4096
    // Grep-stable fixture dump for MR-02: one line per profile —
    // [MATHMETRICS] profile=normal em=18 cw=.. asc=.. desc=.. lineAsc=.. lineDesc=..
    //               cap=.. numAsc=.. numDesc=..   (then profile=script, profile=scriptscript)
    void debugPrintFontMetrics() const;
private:
    mutable uint32_t _dbgLayoutPasses = 0;
    mutable int      _dbgDrawDepthHWM = 0, _dbgDrawDepthCur = 0;
    mutable uint16_t _dbgMissingGlyphs = 0, _dbgVectorDelims = 0;   // per-draw, latched
#endif
```

- Increment points (each a 1-3 line `#ifdef NATIVE_SIM` block, no control-flow change):
  - `_dbgLayoutPasses`: in `onDraw` next to the existing `_root->calculateLayout(_fmNormal)`
    (`MathRenderer.cpp:737`) and in `setExpression`/`setMathStyle` recompute sites
    (`:591`, `:629`) — counts *full-tree layout passes*, the Phase-0 precursor of MR-05's
    per-node `debugLayoutCallsLastFrame` (test plan §2).
  - draw-depth HWM: entry of `drawNodeBaseline` (`:1499`) tracks `depth` maxima; reset at top
    of `onDraw`, latched at its end.
  - missing-glyph: the `else` advance branch of `drawTextBaseline` (`:2480-2483`).
  - vector-delims: each of the three `drawStrokedDelimiter(...)` call sites inside
    `drawDelimiterGlyph` (`:196`, `:219`, `:239`). (`drawStrokedDelimiter` itself is a free
    static without `this` — count at the call sites, not inside it.)
  - budget flag: computed in `onDraw` after layout from `debugTreeStats()`
    (`maxDepth > 24 || nodeCount > 4096`); when it **transitions to true**, print one
    `[LVGL] layout-budget depth=%d nodes=%ld` warning line (rate-limited by the transition;
    tag from the closed set, telemetry spec §2.5 — detection only, per ADR-MR00-03).
- **Reason**: the counters MR-02/MR-05/MR-06 asserts will consume; the fixture dump MR-02
  requires; C5.7 visibility.
- **Risk**: Low — additive, guarded, no draw call added or removed.
- **No-golden proof**: `git diff` rule — inside `MathRenderer.cpp` draw functions, every hunk
  must be enclosed in `#ifdef NATIVE_SIM`; firmware object code for this TU is byte-comparable
  (same compiler input). Emulator pixels: counters draw nothing.

### C.4 `src/apps/CalculationApp.h` + `src/apps/CalculationApp.cpp` — refusal toast + accessors

- **Targets**: `handleKey` insert arms (`CalculationApp.cpp:326-429`), `end()` (`:99-123`),
  the existing NATIVE_SIM block (`CalculationApp.h:95-107`).
- **Change**:
  1. Private helper `void afterInsertCheck();` called once after the `switch` in `handleKey`
     (immediately before the existing `km.consumeModifier()` at `:512`): if
     `_cursor.lastInsertRefusal() != None` → emit one log line and show the toast, then
     `_cursor.clearInsertRefusal()`.
     - Log lines (exact, grep-stable; `Serial.printf` under `ARDUINO`, `std::printf`
       otherwise — repo precedent `CalculationApp.cpp:226-229`):
       `[INPUT] depth-cap max=16`, `[INPUT] node-cap max=512`, `[INPUT] digit-cap max=32`.
  2. Toast: private `lv_obj_t* _toastLabel = nullptr; lv_timer_t* _toastTimer = nullptr;`
     `void showToast(const char* msg);` — lazily creates one `lv_label` (montserrat_14, dark
     pill background) bottom-centered on `_screen`, auto-hidden by a 1500 ms one-shot
     `lv_timer`; `end()` deletes the timer and nulls both pointers (screen deletion owns the
     label — StatusBar precedent). Texts: `"Expression too deep"`, `"Expression too large"`,
     `"Number too long"`.
  3. NATIVE_SIM accessors appended to the existing block (`CalculationApp.h:95-107`):

```cpp
#ifdef NATIVE_SIM
    const vpam::MathCanvas& debugExprCanvas()   const { return _mathCanvas; }
    const vpam::MathCanvas& debugResultCanvas() const { return _resultCanvas; }
    vpam::CursorController::InsertRefusal debugLastInsertRefusal() const;
    // returns the value BEFORE afterInsertCheck() cleared it (latched copy)
#endif
```

     (latch: `afterInsertCheck()` copies the refusal into a NATIVE_SIM-only
     `_dbgLastRefusal` member before clearing, so scripts can assert after the keypress.)
- **Reason**: user-visible feedback (ADR-MR00-03 behavior) + script access.
- **Risk**: Low-Medium (one new LVGL label + timer; teardown wired into `end()` per L-1/L-5).
- **No-golden proof**: toast appears **only** on refusal; no existing script triggers one
  (§B audit); `_toastLabel` is not even created until the first refusal, so widget trees of all
  golden-covered screens are unchanged.

### C.5 `src/hal/NativeHal.cpp` — appended `.numos` commands

- **Targets**: `ScriptCmdType` (append after `AssertMenuFocus`, `NativeHal.cpp:1361`),
  `ScriptCmd` (add defaulted fields; packing two values into `waitN` is forbidden — GHOOK §A),
  `loadScript` (new `else if` branches **before** the terminal else at `:1612-1614`),
  `scriptStepBegin` (new `case` labels appended, `:1697-1850`), doc comment block
  (`:1317-1343` — extend, do not rewrite).
- **Change**: grammar and semantics in §H (normative). New `ScriptCmd` fields:

```cpp
long   argA = -1, argB = -1, argC = -1, argD = -1; // parsed key=value ints (-1 = absent)
int    subKind = 0;                                 // command-specific selector
```

- **Risk**: Low — append-only; the existing branches/enums/exit codes are untouched (fragile-
  file rule, GT §N).
- **No-golden proof**: commands are inert unless a script uses them; no existing script does.

### C.6 `tests/emulator/scripts/mathv2_*.numos` — six new scripts (§I)

### C.7 `.github/workflows/emulator-build.yml` — one appended step (§J.6)

- A "MathRenderer caps & observability suite (MR-01)" step, cloned from the Phase 8C
  runner-function pattern (`emulator-build.yml:308-356`), running the §I scripts.
  Append-only: inserted after the Phase 9F step (`:531`), before candidate generation (`:558`).
  (If the maintainer prefers zero CI edits in the MR-01 PR, this step may move to PR-D —
  see the Phase-0 acceptance plan §C. The scripts still run locally either way.)

**Forbidden in the MR-01 PR** (hard review rule): `src/math/MathEvaluator.*`,
`src/math/MathTypography.h`, `src/math/font/**`, `src/fonts/**`, `src/apps/GrapherApp.*`,
`platformio.ini`, `tests/emulator/golden/**`, `tests/emulator/masks/**`, any `scripts/*.py`.

---

## D. New observability counters (normative definitions)

| # | Counter | Definition | Computed where | Type | Reset policy | Accessor | Script assert | On failure to compute |
|---|---|---|---|---|---|---|---|---|
| 1 | Expression (structural) depth | max parent-hops of any node from root (root=0) | `mathTreeStats` walker (C.1) | `int16_t` | recomputed per query (pure) | `MathCanvas::debugTreeStats().maxDepth`; Calc forwards via `debugExprCanvas()` | `assert_math_depth` | no root ⇒ assert FAILs with "no expression" |
| 2 | Node count | nodes in tree incl. root | same | `int32_t` | pure | same `.nodeCount` | `assert_math_nodes` | same |
| 3 | Layout depth | ≡ structural depth in MR-01 (every node lays out; no budget skips exist yet). Diverges from #1 only after MR-05 refusals — the assert vocabulary stays valid | same | — | — | same | same | — |
| 4 | Draw depth (HWM) | deepest `depth` parameter reached in `drawNodeBaseline` during the last completed draw; ≤ `MAX_RENDER_DEPTH+1` by construction (`MathRenderer.cpp:1507`) | `drawNodeBaseline` entry | `int` | reset at `onDraw` start; latched at end | `debugDrawDepthHWM()` | `assert_math_draw_depth` (optional key of `assert_math_depth`, §H.1) | 0 before first draw |
| 5 | Max fraction nesting | max count of `NodeType::Fraction` ancestors (incl. self) over all nodes | walker | `int16_t` | pure | `debugTreeStats().maxFractionNesting` | `assert_math_depth frac_max=` | — |
| 6 | Max script nesting | same for `Power`/`Subscript` | walker | `int16_t` | pure | `.maxScriptNesting` | `assert_math_depth script_max=` | — |
| 7 | Rendered bounding box | root `LayoutResult` `{width, ascent, descent}` (`MathAST.h:447-459`) after the last layout | cached in root node | `int16_t`×3 | recomputed by layout | `debugRootLayout()` | `assert_math_layout`, `assert_math_layout_exact` | nullptr ⇒ FAIL |
| 8 | Clipped bounding box | clip flags: `x`-clipped iff `rootL.width + 16 > widgetW` (PADDING_LEFT/RIGHT = 8, `MathRenderer.h:231-232`); `y`-clipped iff `mathObjectHeightPx(rootL, fm, VPAM_VERT_PAD) > widgetH` (`MathAST.h:540-547`) | computed in accessor from `_obj` coords + root layout | 2 bools | pure | `debugContentClipped()` → bitmask 0/1/2/3 | `assert_math_clipped x=0|1 y=0|1` (§H.5) | no obj ⇒ FAIL |
| 9 | Glyph fallback count (vector delims) | # of `drawStrokedDelimiter` invocations during last completed draw (all 3 call sites, `MathRenderer.cpp:196,219,239`) | call sites | `uint16_t` | reset at `onDraw` start; latched | `debugVectorDelimCount()` | `assert_math_glyph_fallback vector_min=/vector=` | 0 before first draw |
| 10 | Missing glyph count | # of codepoints hitting the invisible-advance branch (`MathRenderer.cpp:2480-2483`) during last completed draw | that branch | `uint16_t` | same | `debugMissingGlyphCount()` | `assert_math_glyph_fallback missing=/missing_max=` | — |
| 11 | Layout budget exceeded flag | `maxDepth > 24 \|\| nodeCount > 4096` (detection only; values from ADR-MR00-03) | `onDraw`, after layout | `bool` | recomputed per draw; `[LVGL] layout-budget` log on false→true transition | `debugLayoutBudgetExceeded()` | `assert_math_budget kind=none|depth|nodes` | — |
| 12 | Node budget exceeded flag | the `nodes>4096` half of #11, reported separately by `kind=nodes` | same | `bool` | same | same | same | — |
| 13 | Layout pass count | # of full-tree `calculateLayout` invocations through MathCanvas since `create()` (sites `MathRenderer.cpp:591,629,737`) | those sites | `uint32_t` | monotonic per canvas lifetime | `debugLayoutPassCount()` | `assert_math_layout_passes min=/max=` (§H.6) — MR-05's cache proof consumes this | — |
| 14 | Editor insert refusal | latched `InsertRefusal` of the most recent refused insert | `CursorController` guard + Calc latch (C.4) | enum | cleared by `afterInsertCheck` after latching; latch persists until next keypress | `debugLastInsertRefusal()` | `assert_math_insert_refused kind=` | — |

All counters are renderer-**instance** state (per `MathCanvas`), except #14 (per editor).
Firmware behavior: counters #4/#9/#10/#13 and all accessors do not exist (`NATIVE_SIM` only);
#1/#2/#5/#6 helpers compile on firmware (pure functions the caps use); flags #11/#12's log line
compiles on firmware too (it is the C5.8 tripwire and costs one comparison per draw — but the
*computation* `mathTreeStats` is O(n) per draw, so on firmware the budget check runs **only in
`setExpression`** (once per tree change, `MathRenderer.cpp:579-604`), never per frame; the
emulator runs it per draw for assert freshness).

---

## E. Editor insertion cap (normative behavior)

1. **Caps**: depth 16, nodes 512, digits 32 (`kMaxEditorDepth/kMaxEditorNodes/kMaxNumberDigits`,
   ADR-MR00-03). Enforced in `CursorController` (C.2) — the single choke point.
2. **Operations affected**: `insertFraction/insertPower/insertRoot/insertParen/insertFunction/
   insertLogBase` (depth + nodes), `insertDigit` (digits when extending, nodes when creating),
   `insertOperator/insertConstant/insertVariable` (nodes). Navigation and `backspace`: never.
3. **On exceeded cap**: refusal **before mutation** — tree, cursor, scroll, blink state all
   unchanged; `_lastRefusal` set.
4. **User-visible feedback** (CalculationApp only): toast per C.4 + `[INPUT]` log line.
   Result band untouched; no history entry; `changed` stays true only for accepted inserts
   (a refused insert must not call `resetCursorBlink/refreshExpression` — guard sets `changed`
   semantics implicitly because the tree is unchanged; re-rendering an unchanged tree is
   harmless but skip it anyway by checking the refusal in `afterInsertCheck`).
5. **Boundary cases the implementer must encode** (and §I tests pin):
   - Nested-fraction ladder: k-th `insertFraction` at cursor-row depth `d=2(k-1)+2`;
     refusal fires at the first k where `d+3 > 16` (see the derivation comment required in
     `mathv2_caps_deep_fraction.numos`).
   - Capture at the boundary: typing `FRAC` when the left operand is itself a deep fraction —
     `projected` uses `subtreeMaxRelDepth(captured)`; the capture must be refused, and the
     operand must **not** be half-moved (guard runs before any `removeChild`).
   - `insertParen` then typing inside: paren content row is depth `d+2`; subsequent inserts
     re-evaluate from the new cursor row — no special case needed.
   - History reload (`loadHistoryEntry`, `CalculationApp.cpp:839`): loads clones of trees that
     passed the caps when built ⇒ ≤16 by induction; no guard needed on load.
6. **CalculationApp result behavior**: unchanged — caps gate *input* only. Result trees
   (evaluator/CAS-produced) are rendered on `_resultCanvas` with no editor and are exempt
   (their bound is the MR-05 LayoutBudget; MR-01 only *detects* via counter #11).
7. **GrapherApp**: untouched. `MAX_NEST=8` (`GrapherApp.cpp:2553`) fires strictly before the
   shared cap could; unification on the shared constants is MR-16's job.
8. **Why the caps cannot break normal usage**: the deepest structure in all 71 existing scripts
   is ≤5 boxed levels (§B last row); classroom-real expressions (quadratic formula, 4-level
   fraction stress case) are ≤9; 16/512/32 sit 2-6× beyond. The full Phase-8C semantic suite
   must pass unchanged as the regression proof.
9. **Tests**: §I.2-§I.5.

---

## F. Layout refusal / graceful degradation (Phase-0 truth table)

What MR-01 ships vs what remains open (stated honestly; the closure ticket is MR-05):

| Condition | Behavior after MR-01 | Channel | Closed by |
|---|---|---|---|
| Hand input tries to exceed depth 16 / 512 nodes / 32 digits | insert refused pre-mutation; toast + `[INPUT]` log; tree unchanged | §E | **MR-01 (this ticket)** |
| Tree (CAS-result / constructed) with depth > 24 or nodes > 4096 reaches a canvas | renders as today: draw recursion stops at 12 with the red `...` box (`MathRenderer.cpp:1507-1514`); **layout recursion still runs uncapped** (C5.1 residual risk for constructed trees); MR-01 adds detection: budget flag + one `[LVGL] layout-budget` log | counter #11/#12 | refusal (⚠ box, no partial layout) = MR-05 |
| Layout too wide | horizontal scroll [V] (`_scrollX ≤ 0`, cursor auto-follow, `MathRenderer.cpp:762-783`); `debugContentClipped()` reports x-clip | counter #8 | — (already correct) |
| Layout too tall | clipped by the widget (auto-height grows but the canvas is fixed-size in Calc, `CalculationApp.cpp:162-163`); `debugContentClipped()` reports y-clip | counter #8 | vertical viewport = MR-17 |
| What is drawn on refusal | editor refusal: the unchanged expression (nothing new) | — | — |
| What is serialized | nothing changes — serialization does not exist until MR-03; Grapher's flattener untouched | — | MR-03 |
| What the evaluator sees | unchanged — evaluator never sees a tree the editor refused to build; constructed trees still evaluate as today (`MathEvaluator.cpp:517-535`, uncapped — C5.4 residual) | — | evaluator guard = MR-05/MR-09 |
| Error chip / message | none in MR-01 beyond the toast (refusal chips are MR-09's typed-refusal UX) | — | MR-09 |

---

## G. NATIVE_SIM debug accessors (exact surface)

Names differ deliberately from the task sketch (`debugMathTreeStats()` etc.) to keep one accessor
per concern and to match the GHOOK naming style already shipped (`debugHasResult`,
`debugFocusedCardId`); the mapping:

| Task-sketch name | Shipped as | File/class | Fields returned | Scope | Why firmware strips it |
|---|---|---|---|---|---|
| `debugMathTreeStats()` | `MathCanvas::debugTreeStats()` | `src/ui/MathRenderer.h/.cpp` | `MathTreeStats{maxDepth,nodeCount,maxFractionNesting,maxScriptNesting,maxNumberDigits,walkSaturated}` | renderer-global per canvas (works for Calc expr, Calc result, Showcase) | `NATIVE_SIM` is defined only by `[env:emulator_pc]` (`platformio.ini:161`); firmware never defines it (GHOOK §A precedent) — zero flash/RAM cost |
| `debugLastLayoutStats()` | `MathCanvas::debugRootLayout()` + `debugLayoutPassCount()` | same | `LayoutResult*` (w/asc/desc/inkAsc/inkDesc) + `uint32_t` passes | per canvas | same |
| `debugGlyphFallbackStats()` | `debugMissingGlyphCount()` + `debugVectorDelimCount()` | same | two `uint16_t`, last completed draw | per canvas | same |
| `debugExpressionDepth()` | `debugTreeStats().maxDepth` | — | — | — | — |
| `debugNodeCount()` | `debugTreeStats().nodeCount` | — | — | — | — |
| `debugLayoutBudgetState()` | `debugLayoutBudgetExceeded()` (+ which half via the stats themselves) | same | `bool` | per canvas | same |
| (new) | `debugDrawDepthHWM()` | same | `int` | per canvas | same |
| (new) | `debugContentClipped()` | same | bitmask x/y | per canvas | same |
| (new) | `debugPrintFontMetrics()` | same | prints 3 `[MATHMETRICS]` lines (§C.3) | per canvas (uses its own `_fmNormal/_fmSmall/_fmScriptScript`) | same |
| (new) | `CalculationApp::debugExprCanvas()/debugResultCanvas()/debugLastInsertRefusal()` | `src/apps/CalculationApp.h` | canvas refs + latched refusal | app-specific | same block as `debugHasResult` (`CalculationApp.h:95-107`) |

App-specific vs renderer-global: **tree/layout/glyph counters are per-`MathCanvas`** (the same
command can therefore target the result canvas or the showcase); **insert refusal is per-editor**
(CalculationApp). Nothing is process-global except the increment sources feeding per-draw latches.

---

## H. `.numos` assert hooks (exact grammar)

Framework contract reused verbatim (GHOOK §A): parse-validated at load (any violation ⇒
`scriptErr` ⇒ exit 2, nothing executed); one command per deterministic frame; verdicts through
`assertPass/assertFail` (`NativeHal.cpp:1682-1693`) keeping the frozen
`[ASSERT] <script>:<line>: PASS/FAIL - msg` grammar; FAIL ⇒ exit 4 and replay stops.

**App gating** (every command below): the *target canvas* must exist —
`target=expr` (default) and `target=result` require `g_mode == AppMode::CALCULATION && g_calcApp`
(`NativeHal.cpp:249`, mode enum `:195-206`); when `g_mode == AppMode::MATH_SHOWCASE` the default
target is the showcase canvas (`g_showcaseCanvas`, `:264`). Wrong app ⇒
`assertFail("assert_math_* requiere Calculation o MathShowcase activa (app actual: '<name>')")`
— fail, never no-op (the `assert_menu_focus` precedent, `:1826-1830`).
`target=result` additionally FAILs when no result is shown (`!debugHasResult()`).

**Timing discipline**: state is post-previous-frame; scripts `wait 30` after the triggering key
before asserting (GHOOK §A.1).

New `ScriptCmdType` values (append after `AssertMenuFocus`):
`AssertMathDepth, AssertMathNodes, AssertMathLayout, AssertMathLayoutExact, AssertMathBudget,
AssertMathInsertRefused, AssertMathGlyphFallback, AssertMathClipped, AssertMathLayoutPasses,
AssertCalcRenderOk, DumpMathMetrics`.

### H.1 `assert_math_depth [n=N] [min=N] [max=N] [frac_max=N] [script_max=N] [draw_max=N] [target=expr|result]`

- Parse: ≥1 of the value keys; every value integer 0..64; unknown key ⇒ exit 2.
- PASS iff every given constraint holds against `debugTreeStats()` (+ `debugDrawDepthHWM()` for
  `draw_max`). PASS/FAIL message echoes all actuals:
  `assert_math_depth n=15 (actual depth=15 nodes=41 frac=7 script=0 draw=12)`.
- FAIL additionally when `walkSaturated` (message says so — a saturated walk means the tree is
  deeper than 64 and no equality can be trusted).
- Limitation: depth is *structural* (boxed) depth per the C.1 definition — scripts must not
  confuse it with Grapher's `cursorDepth` (cursor-row hops).

### H.2 `assert_math_nodes [n=N] [min=N] [max=N] [target=…]`

Same shape; values 0..100000. Example: `assert_math_nodes max=512`.

### H.3 `assert_math_layout [w_min=N] [w_max=N] [h_min=N] [h_max=N] [target=…]`

- ≥1 key; compares root `LayoutResult` width and `height()` (`MathAST.h:455`).
- Tolerance-banded **by design** (test plan §9): use for cases meant to survive planned metric
  tuning. Example: `assert_math_layout w_min=40 w_max=60 h_min=30 h_max=50`.

### H.4 `assert_math_layout_exact w=N ascent=N descent=N [target=…]`

- All three required. Exact equality — for frozen contract cases only. FAIL message prints all
  three actuals.

### H.5 `assert_math_clipped x=0|1 y=0|1 [target=…]`

- Both keys required (0/1). Compares `debugContentClipped()`.

### H.6 `assert_math_layout_passes [min=N] [max=N] [target=…]`

- ≥1 key; compares `debugLayoutPassCount()` (monotonic). MR-01 use: sanity (`min=1`);
  MR-05 use: steady-frame `wait 60` + delta assertions via two commands is NOT supported —
  MR-05 will add its own `assert_math_layout_calls` per the test plan; this command stays as the
  pass-level oracle. Documented limitation: value depends on frame count only through
  invalidation, which is deterministic under `--deterministic`.

### H.7 `assert_math_budget kind=none|depth|nodes [target=…]`

- Exactly one `kind`. `none` PASSes iff `debugLayoutBudgetExceeded()` is false; `depth`/`nodes`
  PASS iff the flag is true **and** the named half is the exceeded one (both exceeded ⇒ either
  PASSes). This is the task's `assert_math_no_budget_error` (= `kind=none`) and
  `assert_math_budget_error KIND` folded into one command.

### H.8 `assert_math_insert_refused kind=none|depth|nodes|digits`

- Calculation-only (no `target`). Reads `debugLastInsertRefusal()` (the latched value, §C.4).
  `kind=none` asserts the last keypress was not refused. Example after 20× `key FRAC`:
  `assert_math_insert_refused kind=depth`.
- Deliberately named differently from the future `assert_math_refusal name=DisplayOnly|…`
  (MR-09 eval-refusal command, test plan §2) — two different machines; the names must never
  collide.

### H.9 `assert_math_glyph_fallback [missing=N] [missing_max=N] [vector=N] [vector_min=N] [target=…]`

- ≥1 key. Compares last-draw latches (#9/#10). Canonical uses:
  `assert_math_glyph_fallback missing=0` (no invisible glyphs on this screen — the C5.7
  tripwire), `assert_math_glyph_fallback vector_min=2` (tall delimiters used strokes).

### H.10 `assert_calc_render_ok`

- Calculation-only. PASS iff: `_rootRow` exists, `debugRootLayout() != nullptr`,
  `debugTreeStats().walkSaturated == false`, and `debugLayoutBudgetExceeded() == false`.
  One-line health probe for the end of every calc script.

### H.11 `dump_math_metrics [target=…]`

- Not an assert; executes `debugPrintFontMetrics()` on the target canvas. Output contract
  (grep-stable, consumed by MR-02's fixture-parity step):

```
[MATHMETRICS] profile=normal em=18 cw=<int> asc=<int> desc=<int> lineAsc=<int> lineDesc=<int> cap=<int> numAsc=<int> numDesc=<int>
[MATHMETRICS] profile=script em=12 …
[MATHMETRICS] profile=scriptscript em=8 …
```

  Values are whatever `metricsFromFont` (`MathRenderer.cpp:446-535`) produced — the dump adds
  no computation. Field order and key names are frozen on first ship.

**Error messages**: all parse errors follow the house pattern
(`scriptErr(path, line, "assert_math_depth: clave desconocida 'foo' (usa n=|min=|max=|frac_max=|script_max=|draw_max=|target=)")`).
**Limitations** (documented in the doc block extension at `NativeHal.cpp:1317-1343`): commands
read the state of the **active** app's canvases; they never force a redraw — scripts must `wait`
after the last key so a draw has happened before draw-latched counters (#4/#9/#10) are read.

---

## I. Acceptance tests (exact scripts to add)

All new scripts start `wait 200` (launcher settle), use `open_app Calculation` where applicable,
and end with `assert_app`. Frame budgets in parentheses. Every script must contain the derivation
of any pinned number as a comment.

1. **`mathv2_obs_normal.numos`** (1400) — types `1 + 2 FRAC 3 RIGHT + x POW 2` (the
   `mixed_row_fraction_power` shape); asserts:
   `assert_math_depth n=4` (Row0→Power1→Row2→Number3; fraction chain also depth 3 — comment
   carries the full derivation), `assert_math_nodes n=<pinned>` (pin empirically, comment the
   count), `assert_math_insert_refused kind=none`, `assert_math_budget kind=none`,
   `assert_math_glyph_fallback missing=0`, ENTER → `assert_result 11/3` composition sanity? —
   **no**: keep semantics out; end with `assert_calc_render_ok`.
2. **`mathv2_caps_deep_fraction.numos`** (1400) — `key 1`, then `key FRAC` ×8; per §E.5 the
   8th is refused: `assert_math_insert_refused kind=depth`, `assert_math_depth max=16`,
   `assert_calc_render_ok` (no crash, tree intact), then `key AC` and
   `assert_math_depth n=0` — recovery proof.
3. **`mathv2_caps_deep_power.numos`** (1400) — same ladder with `key p` (POW alias,
   `NativeHal.cpp:366-370`); refusal at the analogous count (derive in-comment; power slots are
   base/exponent rows — same +2-per-level geometry).
4. **`mathv2_caps_digits.numos`** (1400) — `key 1` ×33; `assert_math_insert_refused kind=digits`,
   `assert_math_nodes n=1`… (root row + number = pin exact), DEL once,
   `assert_math_insert_refused kind=none` after typing one more digit (cap re-opens).
5. **`mathv2_boss_subtree_depth.numos`** (1600) — builds the deepest exemplar-B-shaped subtree
   typable under the editor cap: `SQRT` then nested `FRAC` ladder to depth 15
   (`assert_math_depth n=15`), `assert_math_budget kind=none` (15 < 24),
   `assert_calc_render_ok`, `screenshot out/mathv2_boss_subtree_depth.ppm` — **warning-only
   candidate** (new stem, no golden blessed in MR-01; the draw-cap-12 red `...` marker is
   *expected content* of this candidate and documents current behavior until MR-05 raises it).
6. **`mathv2_glyph_vector_delims.numos`** (1400) — types `( 1 FRAC 2 RIGHT )`… wrapped so the
   paren must stretch (fraction inside parens): asserts
   `assert_math_glyph_fallback vector_min=2 missing=0` (tall delimiters are stroked today —
   `g_delimiterAssemblyRenderable=false` with the current subset, `MathRenderer.cpp:431-435`),
   `assert_calc_render_ok`.
7. **`mathv2_metrics_dump.numos`** (800) — `open_app Calculation`, `wait 60`,
   `dump_math_metrics`, `assert_app Calculation`. Consumed by MR-02's parity step.

**Negative control** (T6, run locally, not committed as a passing script):
`assert_math_depth n=999` on script 1 must exit 4 with a FAIL line — proves the plumbing can fail.

**Unchanged**: all 71 existing scripts byte-identical, full 8C/9A/9B/9F suites green.

---

## J. Commands the implementer must run (verbatim)

```bash
# 0) environment (Linux; CI-equivalent)
export PLATFORMIO_BUILD_DIR=.pio/build

# 1) emulator build
pio run -e emulator_pc

# 2) direct script runs (every §I script; exit 0 required)
BIN=.pio/build/emulator_pc/program
for s in mathv2_obs_normal mathv2_caps_deep_fraction mathv2_caps_deep_power \
         mathv2_caps_digits mathv2_boss_subtree_depth mathv2_glyph_vector_delims \
         mathv2_metrics_dump; do
  SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic \
    --script tests/emulator/scripts/$s.numos --frames 1600 --quiet || exit 1
done

# 3) negative control (must exit 4)
#    (temporarily edit a copy of mathv2_obs_normal with a wrong n=; do NOT commit)

# 4) candidate generation + golden sweep — MUST be all-identical
python scripts/generate-emulator-candidates.py
for g in tests/emulator/golden/*.ppm; do
  n=$(basename "$g" .ppm)
  m=tests/emulator/masks/$n.mask
  if [ -f "$m" ]; then python scripts/compare-ppm.py "$g" out/emulator-candidates/$n.ppm --mask-file "$m"; \
  else python scripts/compare-ppm.py "$g" out/emulator-candidates/$n.ppm; fi || { echo "GOLDEN DIFF: $n"; exit 1; }
done

# 5) calc semantic suite (8C contract, local mirror of emulator-build.yml:308-356)
for s in tests/emulator/scripts/calc_semantic_*.numos \
         tests/emulator/scripts/calc_error_div_by_zero.numos \
         tests/emulator/scripts/calc_store_variable.numos; do
  SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic \
    --script "$s" --frames 1400 --quiet | tee /tmp/l.log
  grep -q "PASS - " /tmp/l.log && ! grep -q "FAIL - " /tmp/l.log || exit 1
done

# 6) keycode guard (mandatory before any src/ PR)
python scripts/check-keycode-digit-patterns.py --selftest
python scripts/check-keycode-digit-patterns.py
g++ -std=c++17 -Wall -Wextra tests/host/keycode_digit_test.cpp -o /tmp/kt && /tmp/kt

# 7) firmware builds (needs espressif32 toolchain; CI covers if unavailable locally)
pio run -e esp32s3_n16r8            # production — record flash delta in the PR (≤ +2 KB)
pio run -e esp32s3_n16r8_validate   # validation env must not rot
```

---

## K. No-golden-impact contract (byte-identical list)

Every one of the 19 committed goldens must compare identical (masked where a mask exists):

`calc_1_plus_2`, `calc_fraction_sum`, `grapher_expr_smoke`, `grapher_graph_smoke`,
`grapher_smoke`, `grapher_table_smoke`, `grapher_templates_smoke`, `grapher_trace_smoke`,
`launcher_smoke` (maskless — byte-exact), `math_showcase_smoke`, `probability_edit_smoke`,
`probability_smoke`, `regression_data_smoke`, `regression_smoke`, `sequences_edit_smoke`,
`sequences_smoke`, `settings_smoke`, `statistics_data_smoke`, `statistics_smoke`.

Notes:
- `math_showcase_smoke` is unchanged because the showcase draws through the same untouched draw
  paths (`NativeHal.cpp:1089-1155`) — counters add no draw calls.
- Grapher goldens are unchanged because GrapherApp is a forbidden file in this PR and
  CursorController guards fire above Grapher's own `MAX_NEST=8`.
- The only new pixels MR-01 can ever produce is the refusal toast — reachable **only** past the
  caps, which no golden script approaches (§B audit).
- The new `mathv2_boss_subtree_depth` candidate is warning-only (no golden blessed in MR-01;
  its stem may be added to `generate-emulator-candidates.py` in PR-D, not in this PR — the
  generator is a forbidden file here).
- Proof procedure = §J.4 run **before and after** the change, plus the `git diff` review rule of
  §C.3 (draw-function hunks must be `#ifdef NATIVE_SIM`).

---

## L. Rollback plan

- MR-01 is one PR; rollback = `git revert` of that single merge commit.
- No persisted format, no golden, no mask, no generated file changes ⇒ revert is total.
- The appended `.numos` commands become unknown again after revert; the six `mathv2_*` scripts
  revert with the same commit (they live in it), so no script references a missing command.
- Firmware: cap logic reverts with the same commit; no data migration exists.
- Partial rollback (caps keep, observability out or vice versa) is NOT supported — the tests
  couple them; revert whole.

---

## M. Final PR checklist (copy into the PR description)

- [ ] Diff touches only: `src/math/MathAST.{h,cpp}`, `src/math/CursorController.{h,cpp}`,
      `src/ui/MathRenderer.{h,cpp}`, `src/apps/CalculationApp.{h,cpp}`, `src/hal/NativeHal.cpp`,
      `tests/emulator/scripts/mathv2_*.numos` (+ optionally the one CI step).
- [ ] No hunk in `MathRenderer.cpp` draw functions outside `#ifdef NATIVE_SIM`.
- [ ] No edits to goldens/masks/`generate-emulator-candidates.py`/`compare-ppm.py`/fonts/
      `platformio.ini`/`GrapherApp.*`/`MathEvaluator.*`.
- [ ] `ScriptCmdType` values appended only; existing enum order untouched; doc block at
      `NativeHal.cpp:1317-1343` extended, not rewritten; exit-code meanings unchanged.
- [ ] §J.2 all seven scripts exit 0; §J.3 negative control exits 4.
- [ ] §J.4 golden sweep identical before vs after (paste the sweep output).
- [ ] §J.5 semantic suites green; §J.6 keycode guard green.
- [ ] Firmware compiles; flash delta pasted (≤ +2 KB); validate env compiles.
- [ ] `[INPUT]`/`[LVGL]`/`[MATHMETRICS]` line formats match §C.4/§C.3 exactly (grep-stable).
- [ ] Manual firmware note (if hardware available): hold DIV 20× in Calculation → toast, no
      crash, AC recovers (ticket MR-01 HIL line).
- [ ] PR description includes: counter table (§D) deltas none, refusal derivations from §E.5,
      and the sweep/suite outputs.

*End of MR-01 execution plan.*
