# NumOS — Core Apps Implementation Roadmap (CA / GR / UX / QA)

> Ordered, dependency-aware ticket set executing the core-apps spec pack:
> `NUMOS_CALCULATION_APP_PRODUCT_SPEC.md` (CALC), `NUMOS_GRAPHER_APP_PRODUCT_SPEC.md` (GRAPH),
> `NUMOS_CORE_APPS_UX_AND_INPUT_CONSISTENCY_SPEC.md` (UX), `NUMOS_CORE_APPS_QA_ORACLE_AND_STRESS_PLAN.md` (QAP).
> Grapher engine tickets **GR-00…GR-16** remain as specified in
> `NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md` and are referenced, not restated; this doc adds
> GR-17…GR-24 and the CA-/UX-/QA- namespaces. MathRenderer v2 (MR-*), CAS (CAS-*), reliability
> (RT-*), memory (MT-*) tickets are cross-referenced where scopes touch — no duplicate ownership.
>
> Audit base `6da9012` (2026-07-03). Owner tiers: **Sonnet** = mechanical/bounded, **Opus** =
> multi-file feature with regression risk, **Fable** = cross-cutting semantics, **Human** =
> judgment/hardware/golden blessing.
>
> Global rules (inherit the GR-tickets preamble): goldens/masks only via human promotion;
> `NativeHal.cpp` grammar append-only; teardown additions follow `GrapherApp.cpp:167-228`;
> every renderer-visible change re-candidates + human review.

---

## 0. Recommended execution order

```
Wave 0 (no source changes)      GR-00 · QA-07 · QA-06
Wave 1 (oracles)                GR-14 · QA-03 · QA-01 · QA-04 · QA-05
Wave 2 (semantic heart)         GR-02 · UX-01/CA-06 · CA-04 · UX-03
Wave 3 (honesty & caps)         CA-01 · CA-03/GR-20 · CA-07/UX-04 · GR-21 · UX-02 · UX-05 · GR-23
Wave 4 (engine v2 chain)        GR-03 → GR-04 → GR-06 → GR-07 → GR-08 → GR-09 · GR-05 · GR-10 · GR-11 · GR-12/GR-18 · GR-13
Wave 5 (product polish)         CA-08 · CA-10 · CA-12 · CA-05 · CA-09 · GR-16 · GR-17 · GR-19 · GR-22 · UX-07 · CA-02 (golden wave last)
Continuous                      QA-02 · QA-08 · GR-24 · CA-11 (corpus execution tracks every wave)
```

Rationale: oracles before behavior changes (every wave-2+ ticket lands with its corpus rows
flipping in the same PR); the classifier (GR-02) and angle mode (UX-01) unblock the largest
bug-row sets; display honesty (CA-01) precedes golden-heavy polish so goldens are re-blessed once.

---

## 1. Calculation tickets (CA-*)

### CA-01 — Exact/approx display hardening (the ≈ contract)
- **Motivation**: silent `fromDouble` degrades masquerade as exact (CALC §D.2; CAS D.4 "largest
  current contract violation"). Required-list item: *exact/approx display hardening*.
- **Evidence**: `MathEvaluator.cpp:107-149` (`approximate` never set on degrade), degrade sites
  `:309-311,375-400,431-437,500-502`; display `:1052-1191,1198-1287`.
- **Files**: `src/math/MathEvaluator.cpp/.h`, `src/math/ExactVal.h` (comment/contract),
  `src/apps/CalculationApp.cpp` (mode heuristic interaction §E.4).
- **Expected**: every non-exact value carries `approximate=true`; `≈` prefix in all 3 modes;
  Periodic-mode period detection skipped for approx values (plain 12-digit decimal).
- **Non-goals**: exact sums of unlike species (OQ-C2); special-angle trig exactness (CAS D.11);
  rationalizing `1/√2` (recorded option inside ticket).
- **Tests**: corpus flips CC-29/30/40/41/42/43/55/70; `assert_result_approx` (QA-03).
- **Goldens**: `calc_approx_marker` new; existing calc goldens unaffected (integer/fraction cases).
- **Firmware impact**: none beyond shared code; no allocation change.
- **Rollback**: flag `NUMOS_APPROX_MARKER=0` restores old display; flag removed after one release.
- **Risk**: medium (display-visible). **Owner: Opus** (semantics touch), review **Fable**.
- Equals CAS-10 scope for this app — coordinate; one implementation, two spec checkboxes.

### CA-02 — Function-call & delimiter rendering polish
- **Motivation**: CALC §E.7 exhibit (detached oversized paren in `info/Calculation/…use case….png`);
  auto-paren metrics and stroked-delimiter weights need an audit. Required-list item:
  *function-call rendering polish*.
- **Evidence**: `MathRenderer.cpp:1970-2001` (function parens), `:59-84,430` (vector fallback);
  `MathAST.h:957-958`.
- **Files**: `src/ui/MathRenderer.cpp`, possibly `src/math/MathAST.h` metrics.
- **Expected**: step 1 reproduce the exhibit on current source (it may already be fixed — then the
  ticket closes as docs); step 2 fix spacing/height selection; consistent paren weight vs glyph
  parens.
- **Non-goals**: assembly-glyph delimiters (MR-11 owns); new node types (MR-07+).
- **Tests**: MR-02 geometry harness cases if landed; else visual.
- **Goldens**: full calc + showcase re-candidate; **planned re-bless wave** — schedule last (wave 5).
- **Firmware**: none. **Rollback**: revert (pure rendering). **Risk**: medium-high (goldens).
- **Owner: Opus**, blessing **Human**.

### CA-03 — Editor depth/node caps + evaluator guard
- **Motivation**: no cap anywhere in CalculationApp editing/eval [V CALC §C.13]; stack-overflow
  hazard (RT-10). Required-list item: *depth/cap handling*.
- **Evidence**: dossier-verified absence; renderer-only clip `MathRenderer.h:239`,
  `MathRenderer.cpp:1507-1514`; layout unguarded `MathRenderer.cpp:737`.
- **Files**: `src/math/CursorController.cpp/.h` (insertion refusal), `src/apps/CalculationApp.cpp`
  (feedback UI), `src/math/MathEvaluator.cpp` (defensive depth check → typed refusal).
- **Expected**: inserts refused past depth 16 / 512 nodes with visible feedback (MINPUT §1.2);
  evaluator returns `Depth exceeded` instead of recursing unboundedly.
- **Non-goals**: Grapher cap unification (stays MAX_NEST=8 until MR-16); vertical scroll (MR-17).
- **Tests**: BC-02/BC-03; CC-101; negative control.
- **Goldens**: `calc_deep_nesting` refusal state (new only).
- **Firmware**: protects the 64-KB loop stack. **Rollback**: cap constants → 0 disables.
- **Risk**: low-medium. **Owner: Sonnet**, cap values ratified by **Fable** (must match MR-01).

### CA-04 — Error taxonomy
- **Motivation**: single `"Math ERROR"` string hides cause (CALC §F). Required-list item:
  *error taxonomy*.
- **Evidence**: `ExactVal.h:79`, `MathEvaluator.cpp:175-180` + trigger inventory CALC §F.1.
- **Files**: `src/math/MathEvaluator.cpp` (per-site messages), `src/apps/CalculationApp.cpp`
  (error-band styling), one string table.
- **Expected**: CALC §F.2 table verbatim; input preserved; errors never write Ans (unchanged).
- **Non-goals**: MINPUT refusal chips (future node types); legacy Spanish strings (frozen — UX §D.5).
- **Tests**: CC-83…92 with `assert_error <substr>` per class.
- **Goldens**: `calc_error_band`. **Firmware**: none. **Rollback**: strings-only revert.
- **Risk**: low. **Owner: Sonnet.**

### CA-05 — Variable/Ans/PreAns persistence policy
- **Motivation**: Ans unsaved without STO; `approximate` flag dropped by serialization —
  exactness-laundering once CA-01 lands. Required-list item: *variable/Ans/PreAns persistence*.
- **Evidence**: `CalculationApp.cpp:552-554` (no save), `VariableManager.cpp:143-163`
  (flag not serialized, `ok` forced true).
- **Files**: `src/math/VariableManager.cpp/.h` (format v2 `VR02` with flag; migration read of
  `VR01`), `src/apps/CalculationApp.cpp` (OQ-C4 decision hookpoint).
- **Expected**: approx flag round-trips; Ans persistence per OQ-C4 decision (recommend (b) persist
  on ENTER with wear note: ≤1 write/s human rate, LittleFS wear-leveled — acceptable).
- **Non-goals**: store unification (P-04); Grapher's VariableContext.
- **Tests**: BC-37, CC-80/81/82; host-side serialize/deserialize unit.
- **Goldens**: none. **Firmware**: flash write frequency — verify on device.
- **Rollback**: format reader keeps VR01 path. **Risk**: low-medium (persisted data).
- **Owner: Opus** (migration), decision **Human** (OQ-C4).

### CA-06 — Angle-mode UI (with UX-01)
- Same change set as UX-01 (below) — listed here because the required list names it under
  Calculation; single implementation, see UX-01 for fields.

### CA-07 — Dead-key dispositions (RPAREN, NEGATE)
- **Motivation**: two silently inert keys (CALC §C.2/§C.8; UX K-4).
- **Evidence**: no cases in `CalculationApp.cpp:314-509`; serial maps `)` (`SerialBridge.cpp:207-208`).
- **Files**: `src/apps/CalculationApp.cpp`, `src/math/CursorController.cpp/.h` (exit-container-right
  helper), Grapher parallel in UX-04.
- **Expected**: RPAREN = structural close (exit innermost Paren/Function right); NEGATE = Sub
  insert at start (alias).
- **Non-goals**: unmatched-paren literal nodes; interior-unary in Grapher grammar.
- **Tests**: CC-111/112, BC-15.
- **Goldens**: none (no default-visual change). **Rollback**: revert. **Risk**: low.
- **Owner: Sonnet.**

### CA-08 — Degree-n root entry + odd-root semantics (D-3)
- **Motivation**: NodeRoot degree exists, no entry path; `³√−8` errors (CALC §C.5; CAS D.18 D-3).
- **Evidence**: `CursorController.cpp:461-477` (no degree), `MathEvaluator.cpp:775-793`
  (pow-NaN path), `SerialBridge.cpp:218` (`R` combo dead-ends).
- **Files**: `src/apps/CalculationApp.cpp` (SHIFT+SQRT branch), `src/math/CursorController.cpp`
  (`insertRoot(bool withDegree)`), `src/math/MathEvaluator.cpp` (odd-integer-degree real root).
- **Expected**: SHIFT+SQRT → root with degree slot, cursor in degree; `³√−8 = −2`;
  `pow(-8, 1/3)` (power form) unchanged (stays error — D-3 distinction).
- **Non-goals**: Grapher (legacy pipeline has only sqrt).
- **Tests**: CC-46 flip; degree-2 regression rows.
- **Goldens**: root-with-degree render golden (new). **Rollback**: shift branch removal.
- **Risk**: low-medium. **Owner: Sonnet**, semantics review **Fable**.

### CA-09 — History polish
- **Motivation**: pinning cap/order/error entries; CALC §G.6.
- **Evidence**: `CalculationApp.cpp:556-568,807-864`, `CalculationApp.h:136-138`.
- **Files**: scripts only unless OQ-driven changes; optional `assert_history_count` consumer.
- **Expected**: CC-98/99/100, BC-26/27/28 green; no behavior change by default.
- **Non-goals**: NumIR persistence (MR-03).
- **Risk**: low. **Owner: Sonnet.**

### CA-10 — Result scrolling in all modes + Extended digit chip
- **Motivation**: wide Symbolic results clip (CALC §E.6); Extended count invisible (§E.3).
- **Evidence**: scroll wired only in Extended (`CalculationApp.cpp:432-447`).
- **Files**: `src/apps/CalculationApp.cpp`.
- **Expected**: LEFT/RIGHT scroll result whenever wider than canvas; `(200 digits)` chip in
  Extended; scroll resets on mode change (already `:729,854`).
- **Tests**: CC-96/97/103, BC-25. **Goldens**: extended-result golden re-bless (chip).
- **Risk**: low. **Owner: Sonnet.**

### CA-11 — Calculation corpus execution
- **Motivation**: CC-01…CC-112 + BC-01…BC-50 → scripts; doc-drift fixes (stale comments
  `CalculationApp.cpp:869-872`, `CursorController.h:243`, KeyboardManager indicator doc).
- **Files**: `tests/emulator/scripts/*`, `scripts/generate-emulator-candidates.py`,
  `emulator-build.yml` (append), comment fixes in cited headers.
- **Expected**: QAP staging (batch 1 pre-hooks, batch 2 post-QA-03).
- **Risk**: low, volume. **Owner: Sonnet**, blessing **Human**.

### CA-12 — Auto-Periodic threshold pinning
- **Motivation**: PD-C5; CC-22 pair pins `den ≥ 100000` (`CalculationApp.cpp:532-549`).
- **Files**: script + doc only. **Risk**: none. **Owner: Sonnet.**

---

## 2. Grapher tickets

**GR-00…GR-16**: as published (CI wiring; transforms; classifier+explicitX+invalid-reasons+angle
sync; contour chains; GeomStore; POI coverage; segment intersections; trace v2; calc-menu matrix +
integral interval; implicit tangent; slot-error UI; dashed strict boundaries; table for explicitX;
replot budget; assert hooks; corpus/goldens; Axes toggle). Required-list mapping: *explicit x=f(y)
table/trace* = GR-02+GR-07+GR-12; *implicit trace & calculate-menu support-or-refusal matrix* =
GR-04+GR-07+GR-08 (TPA §C.2 is the deliberate-refusal matrix); *inequality strict/non-strict
boundary style* = GR-11; *POI/intersection markers for implicit/mixed* = GR-05+GR-06;
*pan/zoom/autofit semantics* = GEOM §E.9 frozen + GR-21 below; *graph/table visual polish* =
GR-10/GR-16/GR-18/GR-22 below.

New (GRAPH §K, full fields there; roadmap summary):

### GR-17 — Pill/info-bar string table freeze — Sonnet, low.
### GR-18 — Table honesty polish (Δx chip, annotated headers) — Sonnet, low-medium (golden wave), after GR-12.
### GR-19 — Template engine generalization + expanded set — Opus, medium, after GR-02; set expansion **Human** decision.
### GR-20 — Editor cap feedback (MAX_NEST silent drop → hint) — Sonnet, low.
### GR-21 — Zoom clamps ([1e-5, 1e9] + "zoom limit" info) — Sonnet, low. Evidence: no clamps `GrapherApp.cpp:2709-2727`.
### GR-22 — Expression-row long-expression rendering audit (63-char/8-deep visual envelope) — Opus, medium.
### GR-23 — Variable-semantics pinning (`y=A`→0, `y=ans`→0; docs+scripts only) — Sonnet, none.
### GR-24 — Product corpus execution (GC-01…GC-153 + BG-01…BG-75) — Sonnet + Human promotion; continuous.

---

## 3. Shared UX tickets (UX-*)

### UX-01 — Angle-mode single source of truth (+ CA-06)
- **Motivation**: three unsynced states, zero setters, badge can contradict engine (UX §D.4).
  Required-list item: *core input consistency* (largest sub-item).
- **Evidence**: `MathEvaluator.cpp:53` (never assigned — grep), `SystemApp.cpp:79,96-97`,
  `Evaluator.cpp:24-25`, `StatusBar.cpp:243-248`, SettingsApp has no angle row (grep),
  enum value orders differ (`Evaluator.h:32-35` vs `MathEvaluator.h:70-73`).
- **Files**: `src/apps/SettingsApp.cpp/.h` (toggle row), `src/SystemApp.cpp` (retire `_angleMode`
  pushes), `src/apps/GraphModel.cpp/.h` (`syncAngleModeFromSystem()` by name — GR-02 shared piece),
  `src/math/EquationSolver.cpp` (sync at solve), `src/hal/NativeHal.cpp` (append `set_angle_mode`,
  `assert_graph_angle_mode` — GHOOK §D.4).
- **Expected**: Settings toggle writes `vpam::g_angleMode`; StatusBar truthful; DEG trig correct in
  both apps; persisted with settings globals.
- **Non-goals**: cas:: DEG folds (CAS D.19 boundary rule owns); Giac (defined RAD-only).
- **Tests**: CC-56/57/58, GC-137/138/139; badge-equals-engine assert.
- **Goldens**: none in RAD default (all existing goldens RAD — unchanged); DEG goldens new.
- **Firmware**: settings persistence (NVS globals pattern, `Config.h:77-79`).
- **Rollback**: toggle hidden; global stays RAD. **Risk**: medium (touches every trig consumer).
- **Owner: Fable** (cross-engine), UI row **Sonnet**.

### UX-02 — Modifier unification + input-docs truth pass
- **Evidence**: `SystemApp.cpp:478-493` dual booleans; SerialBridge doc drift
  (`SerialBridge.cpp:44,211-212`, `SerialBridge.h:36-37`); indicator doc (`KeyboardManager.h:143`).
- **Files**: `src/SystemApp.cpp`, `src/input/SerialBridge.{h,cpp}` comments,
  `src/input/KeyboardManager.h` comment; `docs/KEYBOARD_LAYOUT.md` refresh.
- **Expected**: KeyboardManager sole modifier state; modifiers reset on returnToMenu (RT-14);
  docs match code. **Tests**: BC-08/09. **Risk**: low-medium (SystemApp dispatch). **Owner: Sonnet.**

### UX-03 — Cross-app error-string tables
- **Files**: per-app string tables (with CA-04/GR-17); style guide section in UX spec.
- **Expected**: UX §D.5 vocabularies; Spanish internals untouched (dry-run classifier depends on
  them — CLASS §A.3). **Risk**: low. **Owner: Sonnet.**

### UX-04 — RPAREN structural close in Grapher editor (pairs CA-07)
- **Files**: `src/apps/GrapherApp.cpp` (`handleExprEdit` case). **Tests**: BG-15 analog.
- **Risk**: low. **Owner: Sonnet.**

### UX-05 — Grapher honest-inert keys (ANS/ALPHA/STO/SHIFT hints)
- **Evidence**: silent no-ops in `handleExprEdit` (`GrapherApp.cpp:2555-2626` — no cases).
- **Expected**: hint-line message ("n/a in Grapher"), indicator never sticks.
- **Non-goals**: store coupling (P-04). **Tests**: BG-68/69 + hint asserts. **Risk**: low.
- **Owner: Sonnet.**

### UX-06 — (stretch) inverse trig in the legacy engine
- **Evidence**: `Evaluator.cpp:125-161` function set; PROJECT_BIBLE §9 recipe.
- **Expected**: arcsin/arccos/arctan tokens+eval+angle-mode output conversion; then Grapher SHIFT
  layer; unblocks PD-U1 reversal.
- **Tests**: engine host rows + GC additions. **Risk**: medium (shared engine: EquationSolver,
  SystemApp consumers). **Owner: Opus.** Optional this cycle.

### UX-07 — Grapher AC-in-edit cancel semantics (executes OQ-U1)
- **Blocked on**: human decision. **Files**: `GrapherApp.cpp` (pre-edit AST clone).
- **Risk**: low-medium (teardown of clones). **Owner: Sonnet.**

---

## 4. QA tickets (QA-*)

### QA-01 — Corpus batch 1 (existing grammar) — Sonnet.
~40 scripts from CC/BC/GC/BG needing only `assert_result*/error/variable/app`; gate + candidates.
Includes the **Calc home-return walk** (gap §B.7 of QAP). Rollback: scripts additive.

### QA-02 — Corpus batch 2 (hook-dependent) — Sonnet, after GR-14+QA-03.

### QA-03 — Calculation assert hooks — Sonnet.
- **Files**: `src/apps/CalculationApp.h` (NATIVE_SIM accessors: `_resultMode`,
  `_lastResult.approximate`, `_history.size()`, canonical-exact formatter),
  `src/hal/NativeHal.cpp` (append `assert_result_exact`, `assert_result_mode`,
  `assert_result_approx`, `assert_history_count`, `assert_status_indicator`).
- **Expected**: QAP §C table; negative control each; zero firmware footprint (`#ifdef NATIVE_SIM`).
- **Risk**: low. Grammar append-only (fragile-file rule).

### QA-04 — Expect-fail ledger tooling — Sonnet.
Inverted-run CI step over the ⚠bug rows (BG-01…14, CC-29…, etc.); a row that *passes* fails the
step with "flip me" — known bugs stay visible without red CI. Rollback: drop the step.

### QA-05 — Failure artifacts — Sonnet.
Assert-fail auto-screenshot (capture at `assertFail`, `NativeHal.cpp:1686-1687` — behavior
addition, not grammar change); `compare-ppm.py --diff-out`; 14-day retention naming.

### QA-06 — Traceability sweep — Sonnet.
Committed corpus↔script cross-reference (CSV under `tests/emulator/`), CI-checked; a corpus row
with status `gated` but no script fails the sweep. This is the mechanism that keeps the 112+153+
50+75-row corpora honest over time.

### QA-07 — Self-diff + negative-control CI steps — Sonnet.
TEST §E.3 T5 double-run `cmp` per graph/result script; T6 inverted-exit controls (BC-43…46,
BG-73/74). No source changes.

### QA-08 — Stress suite — Sonnet.
QAP §F scripts incl. teardown loops (BC-36/37, BG-53/54) and input storms; gate after stability.

---

## 5. Cross-reference to required-items checklist

| Required item | Ticket(s) |
|---|---|
| Calculation exact/approx display hardening | CA-01 (=CAS-10) |
| Calculation function-call rendering polish | CA-02 (+MR-06/11 coordination) |
| Calculation depth/cap handling | CA-03 (+RT-10) |
| Calculation error taxonomy | CA-04 (+UX-03) |
| Calculation variable/Ans/PreAns persistence | CA-05 |
| Grapher explicit x=f(y) table/trace | GR-02, GR-07, GR-12 |
| Grapher implicit trace + calc-menu support/refusal matrix | GR-04, GR-07, GR-08 (TPA §C.2) |
| Grapher inequality strict/non-strict boundary style | GR-11 |
| Grapher POI/intersection markers implicit/mixed | GR-05, GR-06 |
| Grapher graph/table visual polish | GR-10, GR-16, GR-18, GR-22 |
| Grapher pan/zoom/autofit semantics | GEOM §E.9 (frozen) + GR-21 |
| Core input consistency | UX-01…UX-07, CA-06/07 |
| Emulator automation loops | QA-01/02/07/08 + QAP §G |
| Golden promotion plan | QAP §D/§I stages 7 + GR-24/CA-11 (human-only promotion, TEST §E.7 rules) |

---

*End of roadmap.*
