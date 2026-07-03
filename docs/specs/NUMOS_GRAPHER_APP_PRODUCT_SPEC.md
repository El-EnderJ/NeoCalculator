# NumOS — GrapherApp Product Specification

> **The product-level definition of "done" for the Grapher.** This document is the umbrella over the
> engine-level Grapher spec set and adds the product/UX/visual/QA surface those documents deliberately
> left out. Where an engine question is already decided, this document cites the decision instead of
> restating it.
>
> Audit base: branch `claude/numos-core-apps-spec-9at366` @ `6da9012` (2026-07-03), source tree unchanged
> since the `98f61bd`/`ce1b725` audits (all commits since are docs-only — verified via `git log`).
> Notation: **[V]** = verified current behavior with `file:line` evidence; **[P]** = proposed normative
> behavior; **PD-Gn** = product decision recorded in this document; **OQ-Gn** = open decision with options.
>
> Companion documents (already merged, still authoritative for their scope):
> - `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md` (GEOM) — relation taxonomy, plotting algorithms, geometry cache, failure model.
> - `NUMOS_GRAPHER_TRACE_POI_ANALYSIS_SPEC.md` (TPA) — trace state machine, POI markers, Calculate-menu matrix, tangent/integral semantics.
> - `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` (TEST) — test tiers T1–T7, 26-row seed corpus, golden plan.
> - `NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md` (CLASS) — frozen classifier rules R0–R14 and invalid-reason vocabulary.
> - `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md` (GHOOK) — `assert_graph_*` command grammar (GR-14).
> - `NUMOS_GRAPHER_PHASE10_CI_WIRING_PLAN.md` (CIWIRE) — script-by-script CI disposition (GR-00).
> - `NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md` — GR-00 … GR-16 (this document adds GR-17 … GR-24 in §K).
> - `NUMOS_CORE_APPS_UX_AND_INPUT_CONSISTENCY_SPEC.md`, `NUMOS_CORE_APPS_QA_ORACLE_AND_STRESS_PLAN.md`,
>   `NUMOS_CORE_APPS_IMPLEMENTATION_ROADMAP.md` (this session's siblings).

---

## A. Executive summary — what Grapher must be

The Grapher is one of the two flagship apps. The product bar is: **a student can type any reasonable
relation from a secondary-school or first-year-university course and get either a correct plot with
working trace/table/analysis, or an explicit, comprehensible refusal — never a silent wrong picture.**

Concretely, "polished" means all of the following hold (each expanded in the lettered sections):

1. **Input** (§C): every relation class — bare `f(x)`, `y=f(x)`, `x=f(y)`, implicit `G(x,y)=0`,
   strict/non-strict inequalities — is either classified into a first-class kind or visibly rejected
   with a canonical reason (CLASS §A.2). No input may plot the wrong curve (today `x=` plots the line
   x=0 [V `GraphModel.cpp:200-207` + `:247-257`] and `xy=1` plots nothing while claiming validity
   [V `Tokenizer.cpp:147-169`, `Evaluator.cpp:47-77`]).
2. **Plotting** (§D): equal-aspect, artifact-free curves — no asymptote smears, no NaN pixels, no
   truncated-expression plots; bounded work per replot.
3. **Trace** (§E): pixel-stepped, camera-followed trace on every traceable class, with honest
   off-domain display and a refusal message that teaches the workaround for non-traceable classes.
4. **Table** (§F): honest numeric table — `--` for anything the numeric pipeline cannot answer,
   never `nan`, never a silently transposed axis.
5. **Calculate menu** (§G): six analysis actions that are either enabled-and-correct or visibly
   disabled per curve class; no action may silently answer the wrong question (TPA §C.4 rule zero).
6. **Templates** (§H): every shipped template inserts, renders, plots, traces, and tabulates
   correctly — insertion round-trip is CI-gated [V `emulator-build.yml:525-530`].
7. **Visual quality** (§I): no tofu, no clipping, no jitter, deterministic pixels; the graph body is
   byte-golden-able with only the StatusBar clock masked [V `tests/emulator/masks/README.md` policy].
8. **QA** (§J + QA plan doc): every row of the §J corpus has an oracle and a current status; the
   implicit/inequality/aspect surface stops being CI-invisible (today all Phase-10 scripts are tier-T4
   orphans — run by nothing [V verified: zero matches in `emulator-build.yml` for those stems]).

Out of scope for this product cycle (inherited from GEOM §A.3): polar/parametric/3D, region area,
CAS-assisted exact analysis, anti-aliasing, touch input, `VariableManager`/`VariableContext`
unification.

---

## B. Current architecture [V]

Verified code-path map (full detail in the per-file dossier sections below; this is the canonical
short form for future sessions).

### B.1 Structure

- **MVC split**: `GraphModel` = math/eval engine (`GraphModel.h:126`); `GraphView` = LVGL-free
  RGB565 rasterizer (`GraphView.h:40`); `GrapherApp` = LVGL controller (3,283 lines).
- **Three tabs** `EXPRESSIONS / GRAPH / TABLE` (`GrapherApp.h:93`), manual tab bar with pills
  (`GrapherApp.cpp:291-321`), focus chain `TAB_BAR → TOOLBAR → CONTENT` (`GrapherApp.h:94`,
  header contract `GrapherApp.h:24-25`).
- **Expression list**: up to `MAX_FUNCS=6` slots (`GrapherApp.h:64`), each a white pill row with a
  color dot, a VPAM `MathCanvas` editor, and a Templates button on empty rows
  (`GrapherApp.cpp:369-427`, `:1443-1460`). Slot colors: NumWorks palette red/blue/green/orange/
  purple/teal (`GrapherApp.h:83-90`).
- **Graph panel**: created lazily on first Graph-tab entry (`GrapherApp.cpp:284`, `:817-819`);
  320×143 RGB565 PSRAM buffer (`GRAPH_CANVAS_W/H`, `GrapherApp.h:159-160`); PSRAM-fail shows a red
  "ERR: INSUFFICIENT PSRAM" label instead of crashing (`GrapherApp.cpp:578-587`).
- **Table panel**: native `lv_table`, 21 rows × up to 7 columns, sticky yellow header, zebra rows
  (`GrapherApp.h:201-206`, `GrapherApp.cpp:684-698`, `:701-793`).

### B.2 The two-engine bridge

Expression **entry** is structural VPAM (`_exprAST[]`, `CursorController` per slot,
`GrapherApp.h:134-137`). Expression **evaluation** is the legacy double-only pipeline: the AST is
serialized to a ≤63-char string (`syncASTtoText`, `GrapherApp.cpp:1266-1275`; `char text[64]`,
`GraphModel.h:55`), then `GraphModel::preCacheRPN` classifies and compiles it via
`Tokenizer→Parser→Evaluator` (`GraphModel.cpp:125-208`, `GraphModel.h:30-33`). Consequences that
this product spec treats as first-order facts, not implementation trivia:

- The numeric function set is only `sin, cos, tan, sqrt, log(=log10), ln, abs`
  (`Evaluator.cpp:125-161`). Inverse trig does **not** exist in the Grapher: `arcsin(` etc. would
  fail as "Función no soportada" — and no key can even insert it (§C.9).
- `log_b(x)` is serialized as the change-of-base ratio `(log(x)/log(b))`
  (`GrapherApp.cpp:1232-1255`) — correct semantics, gated by `grapher_logbase_smoke`
  (`emulator-build.yml:528`).
- The parser has no unary minus; a leading `-` is rescued by string rewrite `normalizeLeadingUnary`
  (`GraphModel.cpp:38-52`, `Parser.cpp:20-22`). Non-leading unary (e.g. `2*-x`) is NOT rescued [V].
- The Grapher's `Evaluator` has its own angle mode, constructed RAD and never set
  (`Evaluator.cpp:24-25`; zero `setAngleMode` calls in Grapher sources — verified), while the
  StatusBar badge shows the vpam global (`StatusBar.cpp:240-246`). GEOM §C.14 / CLASS §B
  angle-sync rule closes this (GR-02).

### B.3 Relation classes [V]

Classifier `GraphModel::preCacheRPN` (`GraphModel.cpp:125-208`), in order: inequality character scan
first (`:142-157`), bare expression without `y` → explicitY (`:161-170`), bare with `y` → implicit
G=expr (`:171-176`), `y=rhs`/`lhs=y` → explicitY (`:187-198`), any other `=` → implicit G=lhs−rhs
(`:200-207`). `x=f(y)` therefore currently rides marching squares; it becomes first-class explicitX
in GR-02 (CLASS §B R6). Inequality strictness is stored (`Relation` enum, `GraphModel.h:46`) but
not rendered differently (`GraphModel.h:41-46`; dash = GR-11).

### B.4 Feature inventory [V]

| Feature | Mechanism | Evidence |
|---|---|---|
| Explicit plot | 40-sample adaptive sampler, depth 3, 2-px threshold, 3×range jump guard | `GrapherApp.cpp:73-74,1645-1706` |
| Implicit plot | marching squares, CELL=3 px, NaN-corner skip, saddle cases 5/10 | `GrapherApp.cpp:1716-1832` |
| Inequality shade | 50 % checkerboard stipple of 55 %-whitened tint + boundary contour | `GrapherApp.cpp:1750-1794`, `GraphView.cpp:220-241` |
| Equal aspect | `normalizeAspect()` every replot, expands the more-zoomed axis | `GrapherApp.cpp:1843-1873` |
| Auto-fit | 120-sample y-fit, 10 % margin, fallback y∈[−7,7] | `GrapherApp.cpp:2011-2049` |
| Pan/zoom | ±10 % pan; ×1.5 zoom in/out about center; no zoom clamps | `GrapherApp.cpp:2700-2727` |
| Trace | pixel-step, camera follow, POI snap 8 px, escape window 10 | `GrapherApp.cpp:2762-2866,2193-2232` |
| Table | 21 rows, start −5 step 1, LEFT/RIGHT halve/double step (0.1…10) | `GrapherApp.cpp:117,2869-2904` |
| Calculate menu | 6 items from TRACE-ENTER; bisection/golden-section/Simpson n=100 | `GrapherApp.cpp:2910-2975,3083-3183`, `MathAnalysis.h:50-113` |
| POIs | async 10-ms timer; intercept/root/min/max/intersections; only intersections drawn (purple r=3) | `GrapherApp.cpp:2052-2185,2981-3015,1901-1905`, `GraphView.cpp:388-408` |
| Templates | 6 fixed entries, text labels, mini-parser insertion | `GrapherApp.cpp:82-90,1462-1606,1277-1437` |
| Visibility toggle | LEFT on a row; dot dims to 30 % opacity | `GrapherApp.cpp:2466-2481` |
| Toolbar | Auto / Axes / Pan / Trace — **Axes is an empty handler** | `GrapherApp.cpp:2636-2697`, no-op `:2660-2661` |

---

## C. Expression input contract

Normative classification is CLASS §B (R0–R14) — this section states the **product-visible** contract
per input family and its current status. "Reason" strings are the CLASS §A.2 canonical vocabulary.

### C.1 Bare expressions — `x`, `2x`, `sin(x)`

[V] classify explicitY (`GraphModel.cpp:161-170`); implicit multiplication `2x → 2*x` at tokenize
(`Tokenizer.cpp:223-245`). [P] unchanged; regression-frozen (CLASS §C.2).

### C.2 `y=x`, `y=x^2`, `sin(x)=y`

[V] explicitY both orientations (`GraphModel.cpp:187-198`). [P] unchanged.

### C.3 `x=y^2`, `x=3`, `x=sin(y)`, `y^2=x`

[V] implicit via marching squares (`GraphModel.cpp:200-207`; rendering acceptable but multi-sampled,
untraceable, untabulable — `grapher_implicit_sideways_smoke.numos` covers pixels only). [P] first-class
**explicitX** (CLASS §B R6): y-sweep sampler (GEOM §E.2), trace GR-07, y-table GR-12, menu column
TPA §C.2. `x=y` stays explicitY by R5 precedence.

### C.4 Implicit — `x^2+y^2=1`, `y=x^2+y^2`, `x*y=1`

[V] marching squares, both hyperbola branches render (`GrapherApp.cpp:1716-1832`); no trace/table/
POI/menu. [P] contour polylines + arclength trace + segment intersections (GR-03/04/06/07); table
stays `--` by decision (GEOM §B.4).

### C.5 Inequalities — `y>x^2`, `x^2+y^2<1`, `y<=2x`, `x>1`

[V] first `<`/`>` wins, `<=`/`>=` via following `=` (`GraphModel.cpp:142-148`); shading is an
open-set test, NaN never inside (`GraphModel.h:98-107`); strict and non-strict render identically
[V `GraphModel.h:41-46`]. Input: `lt`/`gt` keys insert `<`/`>` as variable glyphs
(`GrapherApp.cpp:2607-2608`); non-strict is typed as `<` then `=` — canonical two-keystroke form,
no dedicated key (GEOM §B.5 decision). [P] dashed-strict boundary (GR-11); trace refused with
teaching message (TPA §A.5).

### C.6 Functions — `sin/cos/tan/ln/log`

[V] inserted as VPAM function nodes (`GrapherApp.cpp:2586-2591`), serialized `name(arg)`
(`GrapherApp.cpp:1219-1231`), evaluated by the legacy dispatch (`Evaluator.cpp:125-161`). SHIFT
inverse-trig does **not** exist in Grapher's editor [V `handleExprEdit` has no shift branch,
`GrapherApp.cpp:2555-2626`] — divergence from CalculationApp recorded in the UX consistency spec.

### C.7 `log_b(x)`

[V] `LOG_BASE` key → `insertLogBase()` (`GrapherApp.cpp:2593`); serialized `(log(arg)/log(base))`
(`GrapherApp.cpp:1232-1255`); CI-gated (`grapher_logbase_smoke`, `emulator-build.yml:528`).
[P] unchanged. Edge [P]: base ≤0 or =1 yields per-point domain NaN (ratio of logs) — acceptable,
row in §J corpus.

### C.8 Templates

See §H.

### C.9 Invalid input and unknown identifiers

[V] today: unbalanced parens / unknown symbols → silent `valid=false` (`GraphModel.cpp:132,153,205`;
`Parser.cpp:157-171`, `Tokenizer.cpp:216-219`); but four **silent-wrong-answer holes** exist —
one-sided relations plot G=0 (`x=` ⇒ x=0 line), multi-relation `y=x=2` is valid-but-dead,
`xy`/`x2`/`abc` are single identifiers that never resolve (plots nothing, stays "valid"),
and >63-char serialization silently truncates-and-plots the wrong curve (GEOM §B.6, CLASS §B R8-R14).
[P] all become `invalid` with canonical reasons + visible slot error state (GR-02 records, GR-10
renders). The **product contract**: an invalid slot must be visually distinct on the expression row
(red border + short reason, GR-10 design), excluded from plot/table/trace, and recover on the next
successful compile.

### C.10 Editor caps

[V] structure/function insertion is depth-capped at `MAX_NEST=8`, silently dropped beyond
(`GrapherApp.cpp:2547-2553,2580-2593`). [P] the drop must not be silent: reuse the info-bar/hint
line to show "max depth" for 1 s (new ticket GR-20, §K) — silently eating keystrokes reads as a
dead keyboard. (The MathRenderer v2 input spec proposes unifying on shared caps later, MR-16;
until then 8 stays.)

---

## D. Plotting semantics

Engine-normative content is GEOM §C/§E. Product-level additions and re-statements:

### D.1 Explicit y=f(x) [V, frozen]

Polyline through adaptive samples; segments dropped when non-finite or when |Δy| > 3×y-range
(`GrapherApp.cpp:1651-1660`). Product statement: **tan(x), 1/x, ln(x) must render as disconnected
branches with zero vertical smear** — CI-gated for tan (`grapher_tan_exit_smoke`,
`emulator-build.yml:526`).

### D.2 Explicit x=f(y) [P — GR-02]

Same sampler over y (GEOM §E.2). `x=3` = clean 1-px vertical line (today: marching-squares cells).
Golden `grapher_implicit_sideways` re-blessed only post-GR-02 (CIWIRE row 21).

### D.3 Implicit contours [V, frozen pixels]

Marching squares constants CELL=3, inside=G<0, saddle 5/10 fixed (`GrapherApp.cpp:1723,1802,
1819-1826`) — pixels are the regression surface once the Phase-10 goldens are blessed (GR-00).
GR-03 chains segments into polylines without changing pixels (byte-identical acceptance).

### D.4 Inequality shading [V, frozen]

Whole-in cells stippled cheaply; mixed/NaN cells per-pixel (≤16 evals/cell)
(`GrapherApp.cpp:1774-1794`). Worst case ~80 k evals — the GEOM §E.11 budget (GR-13) is the
containment. [P] strict boundaries dash (GR-11); region pixels never change.

### D.5 Discontinuities and domain holes

Single product rule (GEOM §C.9): **a NaN point is invisible and inert** — no pixel, `--` cell,
hidden cursor + "off domain" text, no POI, terminates nothing. [V] enforced at
`GraphModel.cpp:221-224`, `GrapherApp.cpp:1697-1698,1930-1936,2349-2353`, `GraphView.cpp:198-202`.

### D.6 Equal aspect [V, frozen semantics]

Always-on; Auto means "center the y-range of interest under equal aspect", not "fill vertically"
(GEOM §E.9). Product consequence to document in help/manual copy: users cannot get a stretched
viewport; circles are always round (`grapher_aspect_circle_smoke`).

### D.7 Pan/zoom

[V] ±10 % pan; ×1.5 zoom steps; no clamps (`GrapherApp.cpp:2700-2727`); `normalizeAspect` guards
degenerate/NaN viewports (`GrapherApp.cpp:1851-1852`). **Gap [V]**: unbounded zoom-in eventually
hits float resolution (units-per-pixel underflow → grid step collapse) with no user feedback.
[P] ticket GR-21 (§K): clamp zoom when `(xMax−xMin) < 1e-5` or `> 1e9` with info-bar "zoom limit";
constants chosen so the full corpus never hits them in normal use. Until GR-21, behavior at extreme
zoom is explicitly **undefined-but-must-not-hang** (covered by `grapher_breakit_stress`).

### D.8 Performance budget

GEOM §E.11 is normative (replot ≤200 ms, trace keystroke ≤100 ms, table ≤50 ms, calc op ≤500 ms;
GR-13 dual wall-clock/eval-count budget). Product addition [P]: the truncation banner
"⚠ plot truncated" must never appear for any §J corpus row at default viewport — that is the
regression definition of "fast enough".

---

## E. Trace semantics

Engine-normative: TPA §A. Product restatement of the contract per class:

| Class | Trace | Pill | Refusal text |
|---|---|---|---|
| explicitY | [V] full: pixel step, camera follow, UP/DOWN slot switch, POI snap | `x: %.3f   y: %.3f`; snapped: `<label>: (x, y)` (`GrapherApp.cpp:1962-1972`) | — |
| explicitX | [P GR-07] y-parameterized; LEFT/RIGHT = y∓/y+ | same x-first order (TPA §A.3) | until GR-07: refusal |
| implicit | [P GR-07] arclength contour walk, UP/DOWN cycles contours, lost-contour exits | `Trace (implicit): x=… y=…` | until GR-04/07: refusal |
| inequality | **never** (TPA §A.5 decision) | — | [P GR-11] "Trace n/a — add G=0 as its own function to trace the boundary" |
| invalid/empty | never | — | [V] "Trace n/a: no y=f(x) (implicit/inequality)" (`GrapherApp.cpp:2673-2678,2731-2735`) |

Product-level invariants (all [V] unless marked):

1. Off-domain: dot/crosshair/pill hidden, info bar `y=--- (off domain)` — never the string `nan`
   (`GrapherApp.cpp:1929-1936,1981-1983`).
2. Camera follow re-centers every step (`syncViewportToCursor`, `GrapherApp.cpp:2235-2253`) —
   the cursor is visually pinned; the world scrolls.
3. POI snap radius 8 px with a 10-move escape window (`GrapherApp.h:79`,
   `GrapherApp.cpp:2193-2232,2772-2799`).
4. ENTER opens the Calculate menu (`GrapherApp.cpp:2834-2837`); AC exits to NAVIGATE clearing
   overlays (`GrapherApp.cpp:2838-2851`); MODE exits the app from any trace state without hang
   (CI gate `grapher_home_return_smoke`).
5. First-ever Graph entry deliberately skips POI/cursor computation until the first trace key
   (lazy branch, `GrapherApp.cpp:836-842`) — frozen for golden determinism (TPA §A.1).
6. [P] Trace precision: the pill's `%.3f` is retained (PD-G1: fixed 3 decimals beats adaptive
   precision for stable goldens; revisit only if a zoomed-in user story demands it — recorded as
   accepted limitation for very small ranges where 3 decimals collapse).

---

## F. Table semantics

Engine-normative: GEOM §C.1 row "Table", GR-12. Product contract:

1. [V] Window: start −5, step 1, row cursor; UP/DOWN scroll, LEFT/RIGHT halve/double step within
   [0.1, 10] (`GrapherApp.cpp:117,2869-2904`). **Gap [V]**: the current start/step are displayed
   nowhere — the user cannot see what step they've dialed. [P] ticket GR-22 (§K): a right-aligned
   `Δx=%.3g` chip in the sticky header bar. No new modal; keys unchanged.
2. [V] Columns: col 0 `x` (`%.4g`), one column per **valid** slot, header `f%d(x)` numbered by
   original slot (`GrapherApp.cpp:2276-2310`); implicit/inequality slots ARE valid, so they appear
   as all-`--` columns (`GraphModel.cpp:216` NAN + `%.6g`/`--` formatting,
   `GrapherApp.cpp:2349-2353`). [P GR-12]: headers annotate the class (`x=f(y)`, `G=0`) so an
   all-`--` column is self-explaining; a lone explicitX slot gets a y-table. No silent transposition
   in mixed sets (GR-12 rule).
3. [V] Empty state: single `f(x)` column of `--` (`GrapherApp.cpp:2308-2309,2355-2357`).
4. [V] Cells never show `nan`/`inf` — `--` only (mixed-relations script header oracle).
5. [V] Zebra striping + sticky header (`GrapherApp.cpp:684-698,708-761`); cells use `stix_math_18`
   (`GrapherApp.cpp:776`), headers montserrat.
6. [P] Determinism: table pixels are golden-able (`grapher_table_smoke` blessed;
   `grapher_multifn_table_smoke` candidate). Semantic cell asserts arrive with GR-14
   (`assert_graph_table_value`, GHOOK §D.5).

OQ-G1 (open): should the table window follow the graph viewport (NumWorks does not; Casio does)?
Options: (a) keep independent window [recommended — matches current code, zero risk];
(b) re-seed start from viewport center on tab switch (breaks table goldens; surprising when
returning to Table). Consequence of (a): a user who pans the graph must scroll the table manually.
Decision deferred to a human product pass; corpus rows assume (a).

---

## G. Calculate menu semantics

Engine-normative: TPA §C (support matrix), §D (tangent), §E (integral). Product restatements:

1. [V] Six items (`CALC_MENU_LABELS`, `GrapherApp.cpp:2910-2917`), reachable only from TRACE-ENTER,
   so today always explicitY (`GrapherApp.cpp:2834-2837`).
2. [V] Backends: Root = bisection with 200-point sign-change scan fallback
   (`MathAnalysis.cpp:39-126`); Min/Max = 200-point scan + golden-section
   (`MathAnalysis.cpp:135-193`); Intersection = root of f1−f2 against the **first other valid slot**
   — including implicit slots whose evalAt is NAN, which yields a false "Not found in range"
   [V hole, `GrapherApp.cpp:3117-3126`, fixed by GR-06/GR-08]; Integral = Simpson n=100 over the
   **whole viewport** (`GrapherApp.cpp:3138-3144`, surprise fixed by the GR-08 two-press interval);
   Tangent = central difference, silent abort on NaN [V `GrapherApp.cpp:3250-3276`; pill gains
   slope + explicit n/a states per TPA §D].
3. [V] Failure pill "Not found in range" (`GrapherApp.cpp:3176-3181`); success moves the cursor to
   the result except Integral (`GrapherApp.cpp:3160-3167`).
4. [P GR-08] Disabled-row rendering per the TPA §C.2 matrix; ENTER on disabled row → pill
   "n/a for this curve type"; dispatch re-checks (defense in depth).
5. [P] Result language: every pill states the object it reports (`Root: x=…`, `Min: (x, y)`,
   `∫[a,b] f dx = … (signed)`, `Tangent: slope=…`). Exact strings are pinned in the QA plan so
   goldens and semantic asserts agree.

---

## H. Templates

1. [V] Six templates (insert-string → label): `y=2*x+3` Linear, `y=x^2-4` Parabola, `y=sin(x)`,
   `y=cos(x)`, `y=x^3` Cubic, `y=1/x` Hyperbola (`TEMPLATES[]`, `GrapherApp.cpp:82-90`).
2. [V] Opened by RIGHT on an empty expression (`GrapherApp.cpp:2534-2542`) or the per-row Templates
   button (`GrapherApp.cpp:1443-1460`). Modal: centered 260×180 card, plain montserrat_14 text
   previews of the exact insert string — VPAM mini-previews were abandoned as unreadable at ~20 px
   row height (`GrapherApp.cpp:1462-1541`); rows lazy-load on a 30-ms timer (`GrapherApp.h:76`).
3. [V] ENTER builds a real VPAM AST via `buildTemplateAST` (`GrapherApp.cpp:1277-1437`), serializes,
   compiles; `OpKind::Eq` serialization is the Phase-9F fix (`GrapherApp.cpp:1129-1134`), gated by
   `grapher_template_insert_smoke` + `grapher_template_all_smoke` (`emulator-build.yml:529-530`).
4. [V] Semantic validity: all six classify explicitY; all plot, trace, tabulate (CLASS §C oracle
   row "templates 1-6"). Every template's graph/table/trace behavior appears as corpus rows §J.10.
5. [V limitation] `buildTemplateAST` recognizes only digits, `. + - * /^`, `x/y`, `=`, `sin(`,
   `cos(` (`GrapherApp.cpp:1387-1432`) — adding a tan/ln/log template today would silently drop the
   function name. [P] ticket GR-19 (§K): either extend the mini-parser or (preferred) build template
   ASTs from the same VPAM constructors the editor uses, then freeze an expanded set (candidates:
   `y=tan(x)`, `y=sqrt(x)`, `y=ln(x)`, `y=abs(x)`†, circle `x^2+y^2=4`, inequality `y>x`).
   † `abs` is evaluable (`Evaluator.cpp:156-157`) but has **no key and no VPAM node** — the template
   would be the only entry path; OQ-G2 records the decision (add NodeFunction Abs end-to-end vs skip
   the template; skipping is the recommended v1).
6. [P] The templates modal is frozen as a golden surface (`grapher_templates_smoke` blessed); any
   set change re-blesses it deliberately.

---

## I. Visual quality contract

The Grapher's visual bar, element by element. "Frozen" = blessed golden exists; changes require
human re-bless (`scripts/promote-emulator-golden.py`, ground-truth constraint #6).

| Element | Contract | Evidence / status |
|---|---|---|
| Tab bar | 3 equal pills, montserrat_14 (STIX overflowed the 22-px pill — `GrapherApp.cpp:310-319`); active pill white, focus outline blue when `_focus==TAB_BAR` (`GrapherApp.cpp:875-897`) | frozen (`grapher_smoke`) |
| Toolbar | 4 tools; focused tool highlighted; **Axes must stop being a dead affordance** (empty handler `GrapherApp.cpp:2660-2661`) | GR-16 (implement toggle, default visuals unchanged) |
| Expression rows | white pills, slot-color dot (30 % opacity when hidden — `GrapherApp.cpp:2466-2481`), MathCanvas math rendering, Templates button only on empty visible rows (`GrapherApp.cpp:1446`) | frozen (`grapher_expr_smoke`); [P] + red error state (GR-10) |
| Notebook background | ruled-lines callback behind expression list (`GrapherApp.cpp:324-343`) | frozen |
| Graph grid | one shared 1/2/5×10ⁿ step targeting ~48 px, square cells under equal aspect, grey `0xE0E0E0` (`GraphView.cpp:124-169`) | frozen (`grapher_graph_smoke`) |
| Axes | dark grey `0x333333`, drawn only when 0 in range (`GraphView.cpp:171-185`) | frozen |
| Axis labels | LVGL draw-end callback, on grid lines, `%d` or `%.1g`, zero suppressed, edge-clamped (`GrapherApp.cpp:467-535`) | frozen |
| Curves | 1-px Bresenham, slot colors, no AA (GEOM §E.8) | frozen |
| Trace pill | floating bottom pill, montserrat_14 (STIX 18 has no space glyph — `GrapherApp.cpp:666-668`), color dot, `%.3f` values | frozen (`grapher_trace_smoke`) |
| Crosshair | two 1-px 60 %-opacity lines (`GrapherApp.cpp:613-628`) | frozen |
| Info bar + mode badge | bottom 20-px bar, `[Trace]`/`[Pan]` badge (`GrapherApp.cpp:1975-2008`) | frozen |
| Table | sticky yellow header `0xFFB531`, zebra `0xEEF4FF`, stix_math_18 cells (`GrapherApp.cpp:713,684-698,776`) | frozen (`grapher_table_smoke`) |
| Templates modal | centered card, title, 6 rows, selected row highlighted (`GrapherApp.cpp:1462-1541`) | frozen (`grapher_templates_smoke`) |
| POI markers | purple `0xAA00CC` filled r=3 circles, intersections only (`GrapherApp.cpp:1901-1905`, `GraphView.cpp:388-408`) | candidate only — needs golden (QA plan) |
| No tofu | every glyph shown must exist in the subsetted fonts; montserrat for prose, STIX for math — any new symbol requires font regeneration (risk REN-2) | policy |
| No clipping | expression MathCanvas rows must not clip at MAX_NEST=8 depth; table cells must not overlap; pill must not overflow screen edge | corpus rows §J.16; no automated guard today [V gap → QA plan] |
| No jitter | with `--deterministic`, identical key sequences give byte-identical frames — the T5 self-diff gate (TEST §E.3) | proposed gate |

PD-G2 — **font-mixing rule (recorded)**: math content renders in STIX; all chrome (tab bar, pills,
info bar, table headers, template labels) renders in montserrat_14. This is current verified
behavior driven by glyph coverage constraints [V citations above]; it is now the *intended* design,
not an accident. Any "unify fonts" proposal must re-bless every Grapher golden and is out of scope.

---

## J. Edge-case corpus (Grapher)

Conventions: default viewport −10..10 × −7..7 equal-aspected to y≈±4.47 (GEOM §E.9); "typed" means
via emulator key names (`NativeHal.cpp:456-565`). Columns — **Class**: [V]→[P] classification;
**Plot/Table/Trace**: expected behavior (the oracle); **Status**: `gated` (CI-gated now), `golden`
(blessed golden), `T4` (script exists, unwired — CIWIRE dispositions apply), `bug` (known silent
wrong answer, [P] flips it), `untested` (no script), `future` (requires GR-nn). Oracle types:
G = golden, A = assert hook (GR-14 vocabulary), NH = no-hang gate, M = manual/HIL.
This corpus supersedes TEST §B (26 rows) by inclusion; row ids GC-nn are stable for scripts.

### J.1 Explicit basics (14)

| # | Input | Class | Plot | Table | Trace | Status / oracle |
|---|---|---|---|---|---|---|
| GC-01 | `x` | explicitY | line slope 1 | values | full | gated+golden (`grapher_expr/graph/table/trace_smoke`) G+A |
| GC-02 | `2x` | explicitY | slope 2 | 2x | full | T4 (`grapher_implicit_2x_smoke` — misnamed, it is explicit) G |
| GC-03 | `y=x` | explicitY | slope 1 | x | full | gated G |
| GC-04 | `x=y` | explicitY (R5 precedence) | 45° line | values | full | untested → A (`assert_graph_slot_kind`) |
| GC-05 | `y=x^2` | explicitY | parabola | x² | full | T4 (`grapher_explicit_parabola_smoke`) G |
| GC-06 | `sin(x)=y` | explicitY | sine | values | full | untested → A |
| GC-07 | `-x` | explicitY (leading-unary rescue `GraphModel.cpp:38-52`) | slope −1 | −x | full | untested → A |
| GC-08 | `y=-x^2` | explicitY | downward parabola | values | full | untested → A |
| GC-09 | `2*-x` | invalid [V syntax — unary not rescued mid-expression, `Parser.cpp:20-22`] | nothing | excluded | refused | untested → A (negative control: must NOT plot) |
| GC-10 | `x+1` | explicitY | shifted line | values | full | untested → A |
| GC-11 | `2(x+1)` | explicitY (implicit mul Number→LParen, `Tokenizer.cpp:223-245`) | slope 2 line | values | full | untested → A |
| GC-12 | `(x+1)(x-2)` | explicitY (RParen→LParen mul) | parabola | values | full | untested → A |
| GC-13 | `x(x+1)` | valid-but-dead [V: `x(` lookahead → Function token → "Función no soportada", `Tokenizer.cpp:157-167`] → invalid [P R11 dry-run] | nothing | `--` | refused | **bug** → A invalid-reason `syntax` |
| GC-14 | `y=0` | explicitY constant | x-axis overdraw | 0 | full | untested → A |

### J.2 Polynomials & rationals (10)

| # | Input | Class | Plot | Table | Trace | Status |
|---|---|---|---|---|---|---|
| GC-15 | `y=x^3` | explicitY | cubic | values | full | gated via template_all NH |
| GC-16 | `y=x^3-3x` | explicitY | two extrema | values | full; Min/Max find both | untested → A |
| GC-17 | `y=1/x` | explicitY | 2 branches, no smear | `--` at 0 | off-domain at 0 | template NH; domain row untested → A |
| GC-18 | `y=1/(x^2-1)` | explicitY | 3 branches, poles ±1 | `--` at ±1 | off-domain | untested → G+A |
| GC-19 | `y=x/(x+1)` | explicitY | hyperbola, pole −1 | values | full | untested → A |
| GC-20 | `y=x^4-x^2` | explicitY | W shape | values | full | untested → A |
| GC-21 | `y=(x+1)/(x-1)` | explicitY | pole at 1 | `--` at 1 | off-domain | untested → A |
| GC-22 | `y=x^0.5` | explicitY [V: `^` with double exponent — `pow` NaN for x<0 → half-domain] | right half | `--` x<0 | off-domain left | untested → A |
| GC-23 | `y=x^(1/3)` | explicitY; x<0 → NaN (std::pow fractional) — right half only [V `Evaluator.cpp` pow path] | right half | `--` x<0 | off-domain | untested → A; **document**: no real odd root in Grapher (VPAM D-3 decision does not extend here) |
| GC-24 | `y=x^10` | explicitY | steep even curve | huge values `%.6g` | full | untested → A |

### J.3 Trig (10)

| # | Input | Class | Plot | Notes | Status |
|---|---|---|---|---|---|
| GC-25 | `y=sin(x)` | explicitY | RAD sine [V — even under DEG badge, GEOM §C.14] → follows system mode [P GR-02] | DEG assert = period 360 (GHOOK §E.6) | gated NH (`grapher_functions_smoke`); DEG row future GR-02 A |
| GC-26 | `y=cos(x)` | explicitY | cosine | — | gated NH |
| GC-27 | `y=tan(x)` | explicitY | branches, no smear (`Evaluator.cpp:129-137` pole error + jump guard) | trace off-domain at poles (`grapher_trace_domain_smoke`) | gated NH (`grapher_tan_exit_smoke`); domain T4 |
| GC-28 | `y=sin(2x)` | explicitY | period π | — | untested → A |
| GC-29 | `y=sin(x)^2` | explicitY (power of function call serializes `sin(x)^(2)`) | [0,1] wave | — | untested → A |
| GC-30 | `y=sin(1/x)` | explicitY | oscillation cluster near 0; bounded work (depth 3) | must not hang | stress (in `grapher_curve_stress` family) NH |
| GC-31 | `y=x*sin(x)` | explicitY | growing wave | — | untested → A |
| GC-32 | `y=tan(x)/tan(x)` | explicitY | y=1 with holes at poles | `--` at poles | untested → A (no `x/x→1` simplification — numeric pipeline) |
| GC-33 | `y=sin(cos(x))` | explicitY | nested | — | untested → A |
| GC-34 | `arcsin` entry | **impossible** [V: no SHIFT layer in `handleExprEdit`, `GrapherApp.cpp:2555-2626`] | — | UX divergence row (UX spec) | n/a |

### J.4 Logs & roots (8)

| # | Input | Class | Plot | Table | Status |
|---|---|---|---|---|---|
| GC-35 | `y=ln(x)` | explicitY | x>0 only (`Evaluator.cpp:150-155`) | `--` x≤0 | gated NH (functions_smoke) |
| GC-36 | `y=log(x)` | explicitY, log10 (`Evaluator.cpp:144-149`) | through (1,0),(10,1) | `--` x≤0 | gated NH |
| GC-37 | `y=log_2(x)` | explicitY via `(log(x)/log(2))` (`GrapherApp.cpp:1232-1255`) | (1,0),(2,1) | value 1 at x=2 | gated NH (`grapher_logbase_smoke`); cell assert future GR-14 |
| GC-38 | `y=log_1(x)` | explicitY; log(1)=0 denominator → div-0 → NaN everywhere | empty plot, all `--` | valid-but-empty [V acceptable; per-point domain rule] | untested → A |
| GC-39 | `y=ln(x-5)` | explicitY | x>5 | `--` x≤5 | untested → A (also CLASS §A.3 dry-run tolerance case) |
| GC-40 | `y=sqrt(x)` | explicitY | half parabola (`Evaluator.cpp:138-143`) | `--` x<0 | untested → A |
| GC-41 | `y=sqrt(1-x^2)` | explicitY | upper unit semicircle | `--` outside [−1,1] | untested → G |
| GC-42 | `y=sqrt(x^2)` | explicitY | |x| V-shape | values | untested → A |

### J.5 Explicit x=f(y) (8)

| # | Input | Class [V]→[P] | Plot | Trace/Table | Status |
|---|---|---|---|---|---|
| GC-43 | `x=y^2` | implicit → explicitX | sideways parabola | refused/`--` [V]; y-trace + y-table [P GR-07/12] | T4 (`grapher_implicit_sideways_smoke`); re-bless post-GR-02 |
| GC-44 | `x=3` | implicit → explicitX | vertical line | Root correctly "Not found" | untested → A future GR-02 |
| GC-45 | `x=sin(y)` | implicit → explicitX | vertical sine | y-trace [P] | untested → A future |
| GC-46 | `y^2=x` | implicit → explicitX (R6 rhs form) | same as GC-43 | — | untested → A future |
| GC-47 | `x=y` | explicitY (R5 wins) | 45° line | full | untested → A (precedence pin) |
| GC-48 | `x=-y^2` | implicit → explicitX | left parabola | — | untested → A future |
| GC-49 | `x=1/y` | implicit → explicitX | hyperbola, hole y=0 | off-domain [P] | untested → A future |
| GC-50 | `x=y` + `x=y^2` two slots | mixed explicitY+explicitX | both; intersections (0,0),(1,1) [P GR-06] | — | untested → G future |

### J.6 Implicit (12)

| # | Input | Plot | Trace [P GR-07] | Status |
|---|---|---|---|---|
| GC-51 | `x^2+y^2=1` | unit circle, closed, round | contour wrap | T4 (`grapher_implicit_circle_smoke`, `grapher_aspect_circle_smoke`) → bless (GR-00) |
| GC-52 | `y=x^2+y^2` | circle r=½ @ (0,½) (`GrapherApp.cpp:1713-1715` comment) | contour | T4 (`grapher_implicit_ycircle_smoke`) |
| GC-53 | `x*y=1` | two hyperbola branches | UP/DOWN cycles branches | untested → G+A |
| GC-54 | `x^2-y^2=1` | hyperbola opening left/right | 2 contours | untested → G |
| GC-55 | `x^2/4+y^2=1` | ellipse (typed as `x^2/4+y^2=1`; fraction node serializes `(x^2)/(4)`) | contour | untested → G |
| GC-56 | bare `y` | implicit G=y ⇒ x-axis line (CLASS §B R4) | — | untested → A |
| GC-57 | `y^2=x^2` | X cross (two lines); saddle cells at origin | trace never silently switches branch [P §C.11] | untested → G |
| GC-58 | `sin(x)=sin(y)` | diagonal line family | bounded work | untested → G (stress family) |
| GC-59 | `x^2+y^2=0` | single point (may render 0–4 px at grid resolution — accepted) | trace n/a (no polyline) → "lost contour" rules | untested → M/G |
| GC-60 | `x^2+y^2=-1` | empty set — axes-only frame, slot stays valid | trace refused | untested → G (negative visual) |
| GC-61 | `sqrt(x)+sqrt(y)=2` | quarter curve in x,y≥0; NaN cells clip it cleanly | chains break at domain cells (GR-03 acceptance) | untested → G |
| GC-62 | `x^2+y^2=1` + `y=x` two slots | circle + line; [P GR-06] `≈` intersections at ±(√½,√½) | menu Find Intersection partner fix (TPA §C.1 hole) | **bug** today (always "Not found"); future A |

### J.7 Inequalities (10)

| # | Input | Region | Boundary | Status |
|---|---|---|---|---|
| GC-63 | `y>x^2` | stipple above parabola | solid [V] → dashed [P GR-11] | T4 (`grapher_ineq_parabola_smoke`) |
| GC-64 | `y>=x^2` | same region | solid (non-strict) | untested; golden pair differs only on boundary post-GR-11 |
| GC-65 | `x^2+y^2<1` | open disk | circle | T4 (`grapher_ineq_disk_smoke`) |
| GC-66 | `x^2+y^2>1` | exterior | circle | T4 (`grapher_ineq_exterior_smoke`) |
| GC-67 | `y<x` | half-plane below | line | T4 (`grapher_ineq_halfplane_smoke`) |
| GC-68 | `x>1` | right half-plane | vertical line | untested → G |
| GC-69 | `y<=2x` (typed `y`,`lt`,`=`,`2`,`x`) | half-plane | solid | untested → A (`assert_graph_relation_op 0 le` is the only pre-GR-11 oracle, CLASS §C) |
| GC-70 | `y>ln(x)` | region only where ln defined; NaN cells outside-but-per-pixel-feathered (GEOM §E.4 asymmetry — do not "fix") | boundary x>0 | untested → G |
| GC-71 | `sin(x*y)>0` zoomed out | worst-case mixed cells (~80 k evals) — must complete under GR-13 budget with banner | — | stress → NH+A future |
| GC-72 | `y>x^2` + `y<x+2` two slots | overlapping stipples slot-order overdraw (GEOM §B.7) | both boundaries | untested → G |

### J.8 Invalid & adversarial (14)

All [P] rows assert `assert_graph_slot_kind i invalid` + reason (GHOOK §D.3); today's behavior is
the **bug** column. Negative visual controls assert the graph is axes-only.

| # | Input | Today [V] | [P] reason | Status |
|---|---|---|---|---|
| GC-73 | `x=` | **plots x=0 line** (`GraphModel.cpp:200-207,247-257`) | `one-sided relation` | bug → gated A post-GR-02 |
| GC-74 | `=x` | plots x=0 | `one-sided relation` | bug |
| GC-75 | `y<` | **shades y<0** (`GraphModel.cpp:149-156`) | `one-sided relation` | bug |
| GC-76 | `xy=1` | valid, plots nothing (`Tokenizer.cpp:147-169`) | `unknown: xy` | bug — flagship negative oracle (TEST #9); positive twin `x*y=1` = GC-53 |
| GC-77 | `x2` | valid-but-dead (digits are ident-parts, `Tokenizer.cpp:58-60`) | `unknown: x2` | bug |
| GC-78 | `abc=1` | valid-but-dead | `unknown: abc` | bug |
| GC-79 | `y=x=2` | valid, never draws (`Parser.cpp:145-149`, `Evaluator.cpp:239-242`) | `multiple relations` | bug |
| GC-80 | `0<x<1` | first-`<` split, rhs fails → one-sided shading of nothing | `multiple relations` | bug |
| GC-81 | `1=2` | valid, empty plot | `constant relation` | bug (CLASS R10) |
| GC-82 | `pi<e` | valid, no shading | `constant relation` | bug |
| GC-83 | `((x` | invalid silently (`Parser.cpp:157-171`) | `syntax` + visible state (GR-10) | partial — needs error UI |
| GC-84 | `sin()` | valid-but-dead (operand-less RPN) | `syntax` (dry-run) | bug |
| GC-85 | 64+-char nested expr | **silently truncated → wrong curve** (`GrapherApp.cpp:1271`) | `expression too long` | bug (reliability C7.3) |
| GC-86 | `y=a*x` (unset a) | explicitY, plots y=0 — single letters default 0 (`VariableContext.cpp:23-32`) | unchanged [documented], header chip idea rejected (PD-G3: defined-as-zero is the contract) | untested → A |

### J.9 Multi-slot & visibility (8)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-87 | `y=x` + `y=x^2` | both curves, colors 1/2; purple markers (0,0),(1,1) | T4 partial (`grapher_multifn_graph_smoke` candidate, no golden) |
| GC-88 | same, Table | 3 columns `x, f1(x), f2(x)` (`GrapherApp.cpp:2299-2310`) | candidate (`grapher_multifn_table_smoke`) |
| GC-89 | 6 slots filled | all plot; Add row hidden at cap (`GrapherApp.cpp:942-946,994`) | T4 (`grapher_expr_scroll_smoke`) |
| GC-90 | LEFT toggles slot 0 hidden | dot dims 30 % (`GrapherApp.cpp:2466-2481`); curve disappears; table column REMAINS (valid-only filter ignores visibility [V `GrapherApp.cpp:2276-2282`]) | untested → A+G; OQ-G3: should hidden slots leave the table? Options: (a) keep [V, recommended — table is about values]; (b) filter to visible (consistency with plot). Decide with human |
| GC-91 | duplicate `y=x` twice | one visible curve (second color overdraw); collinear-overlap → zero intersection POIs [P GEOM §C.10] | untested → A future GR-06 |
| GC-92 | delete slot 1 of 3 (DEL on row) | remaining slots renumber; colors stay per-slot | untested → G+A |
| GC-93 | mixed `sin(x)` + circle + disk | three layered kinds, slot-order z (GEOM §B.7) | T4 (`grapher_mixed_relations_smoke`) |
| GC-94 | trace UP/DOWN across mixed set | skips implicit/inequality slots (`GrapherApp.cpp:2804-2833`, `:1632-1637`) | untested → A |

### J.10 Templates (7)

| # | Template | Expected | Status |
|---|---|---|---|
| GC-95…100 | each of the 6 | inserts exact string, explicitY, plots+traces+tabulates; `=` round-trip | gated NH (`grapher_template_insert/all_smoke`) |
| GC-101 | RIGHT on non-empty row | does NOT open templates (`GrapherApp.cpp:2534-2542` empty-only) | untested → A |

### J.11 Trace scenarios (10)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-102 | trace `y=x`, one RIGHT | cursor+crosshair+pill; x advances one world-pixel (`GrapherApp.cpp:2763`) | golden (`grapher_trace_smoke`) |
| GC-103 | trace into `ln(x)` x<0 | pill hidden, `y=--- (off domain)` | T4 (`grapher_trace_domain_smoke`) |
| GC-104 | trace across tan pole | off-domain at pole, resumes after | T4 (same script) |
| GC-105 | POI snap: `y=x^2-4` near x=2 | snap to Root, pill `Root: (2.000, 0.000)`; 10-move escape | untested → A future GR-14 |
| GC-106 | trace with camera follow to viewport edge | world scrolls; cursor stays centered; no hang | gated NH (implicit in tan_exit walk) |
| GC-107 | ENTER in NAVIGATE with only implicit slots | refusal message, stays NAVIGATE (`GrapherApp.cpp:2731-2736`) | T4 (`grapher_implicit_tabletrace_safe`) |
| GC-108 | AC from TRACE | → NAVIGATE, overlays cleared (`GrapherApp.cpp:2838-2851`) | gated NH (home_return walk) |
| GC-109 | MODE from TRACE | exits app, no hang | gated (`grapher_home_return_smoke`) |
| GC-110 | trace slot switch retargets tangent overlay | tangent redraws at new fn (`GrapherApp.cpp:2811-2816`) | untested → G |
| GC-111 | zoom while tracing (ADD in TRACE) | key ignored in TRACE [V: no zoom case in `handleGraphTrace`, `GrapherApp.cpp:2762-2866`] — user must AC to NAVIGATE first; documented behavior | untested → A |

### J.12 Table scenarios (7)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-112 | default table `y=x` | rows −5…; x `%.4g`, y `%.6g` | golden (`grapher_table_smoke`) |
| GC-113 | LEFT×5 | step 0.1 floor (`GrapherApp.cpp:2883-2889`) | untested → A future GR-14 |
| GC-114 | RIGHT×5 | step 10 cap (`GrapherApp.cpp:2890-2896`) | untested → A |
| GC-115 | UP at top row | window scrolls start down one step (`GrapherApp.cpp:2871-2878`) | untested → A |
| GC-116 | implicit slot column | header `f%d(x)`, all `--` [V] → annotated header [P GR-12] | T4 (mixed_relations) |
| GC-117 | empty table | single `f(x)` col of `--` (`GrapherApp.cpp:2355-2357`) | untested → G |
| GC-118 | `y=1/x` row at x=0 | exactly that cell `--` | untested → A |

### J.13 Calculate menu (10)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-119 | Root of `y=x^2-4` | `Root: x=±2` (nearest-to-cursor pin [P GR-08]) | untested → A |
| GC-120 | Root of `y=x^2+1` | "Not found in range" | untested → A |
| GC-121 | Root of `y=x^2` (tangent root) | "Not found" [V correct — no sign change, GEOM §C.12]; `Touch` POI [P] | untested → A |
| GC-122 | Min of `y=x^2` | (0,0) | untested → A |
| GC-123 | Max of `y=x` (monotone) | viewport-edge extremum + `(at view edge)` suffix [P GR-08] | untested → A |
| GC-124 | Intersection `y=x` vs `y=x^2` | nearest of (0,0)/(1,1) | untested → A |
| GC-125 | Intersection with only implicit partner | [V] false "Not found" — **bug** (`GrapherApp.cpp:3117-3126`) → segment-based or labeled partner [P GR-06/08] | bug |
| GC-126 | Intersection, single function | "No second function" (`GrapherApp.cpp:3122-3126`) | untested → A |
| GC-127 | Integral of `x^2` (default viewport) | Simpson n=100; shading; pill states bounds [P] | untested → A (0.3333±1e-4 on [0,1] post-GR-08) |
| GC-128 | Tangent at x=1 on `y=x^2` | line drawn; pill `Tangent at x=1.000` [V] → +slope [P GR-09] | untested → G |

### J.14 Viewport ops (8)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-129 | zoom in ×3 on circle | stays round (equal aspect) | T4 (`grapher_aspect_circle_smoke`) |
| GC-130 | pan all 4 directions | 10 % shifts; labels update on grid lines | untested → G |
| GC-131 | Auto with `y=1/x` | y-fit ignores |y|>1e6 (`GrapherApp.cpp:2029`) | untested → G |
| GC-132 | Auto with no functions | y∈[−7,7] default (`GrapherApp.cpp:2043-2045`) | untested → A |
| GC-133 | Auto with only implicit | fit ignores implicit (evalAt NAN) → default | untested → A |
| GC-134 | zoom out ×20 rapidly | no hang; inequality budget banner allowed [P GR-13] | stress NH |
| GC-135 | zoom in ×50 | no hang; [P GR-21] clamp + "zoom limit" | **undefined today** → future |
| GC-136 | sideways parabola under aspect | correct opening | T4 (`grapher_aspect_sideways_parabola_smoke`) |

### J.15 Angle mode & variables (5)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-137 | DEG mode + `y=sin(x)` | [V bug: RAD curve under DEG badge, GEOM §C.14] → period 360 [P GR-02] | bug → A (`set_angle_mode`, GHOOK §D.4) |
| GC-138 | DEG mode table sin(90) | cell 1 [P] | future A |
| GC-139 | RAD↔DEG switch mid-session | replot follows on next `preCacheRPN`/`replot()` sync (CLASS §B angle rule) | future A |
| GC-140 | `y=A` after STO 4→A in Calculation | **plots y=0, not 4** [V — disjoint stores: Grapher reads `VariableContext` (`Evaluator.cpp:47-77`), Calculation writes `VariableManager` (`CalculationApp.cpp:894-901`)] | documented divergence (UX spec; unification = ground-truth P-04, out of scope) → A pins current behavior |
| GC-141 | `y=ans` in Grapher | resolves `VariableContext` Ans = 0.0 default, not Calculation Ans [V `Evaluator.cpp:49-60`] | same as GC-140 |

### J.16 Stress & visual-integrity (12)

| # | Scenario | Expected | Status |
|---|---|---|---|
| GC-142 | `grapher_breakit_stress` walk | exit 0, no hang | T4 → gate (GR-00) NH |
| GC-143 | `grapher_curve_stress` | completes in frame budget | T4 → gate NH |
| GC-144 | MAX_NEST=8 deep fraction in editor | renders without clipping the pill row; 9th insert dropped + [P] feedback (GR-20) | untested → G+M |
| GC-145 | 63-char expression renders in row | MathCanvas must not overflow the pill (scroll or shrink); today unverified | untested → G+M |
| GC-146 | rapid tab cycling ×20 | no leak, no hang; lazy graph panel init once | untested → NH script |
| GC-147 | HOME/relaunch ×10 | teardown clean (`GrapherApp.cpp:167-228` reference pattern) | gated (home_return) + QA plan loop |
| GC-148 | trace key held (REPEAT) 100 steps | ≤100 ms/step; POI async never blocks (`GrapherApp.cpp:2981-3015`) | untested → NH |
| GC-149 | double-run byte determinism per script | SHA-256 equal (T5 self-diff) | proposed gate (TEST §E.3) |
| GC-150 | PSRAM alloc failure at graph creation | red ERR label, app navigable (`GrapherApp.cpp:578-587`) | firmware-only M (TEST §E.6) |
| GC-151 | `y=sin(10x)` vs `y=0` | ≤20 POIs + [P] "POIs limited" notice (GEOM F.6) | untested → A future |
| GC-152 | all 6 slots invalid | axes-only graph; table `--`; trace refused; no hang | untested → A |
| GC-153 | Templates modal open + MODE | app exits cleanly (modal in teardown list, `GrapherApp.cpp:167-228`) | gated (home_return walk covers) |

**Corpus count: 153 rows** (GC-01…GC-153). Every `untested → A` row becomes a script in the QA
plan's break-it/coverage batches; every `bug` row is a tracked expected-fail until its ticket lands
(GR-02 for classification rows, GR-06/08 for menu rows, GR-11/12/13/21 as marked).

---

## K. Implementation tickets

GR-00 … GR-16 are already specified in `NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md` and remain the
engine backbone (CI wiring, transforms, classifier+angle, contour chains, GeomStore, POI coverage,
segment intersections, trace v2, menu matrix, implicit tangent, error UI, dash, table, budget,
hooks, corpus, Axes). This product spec adds:

### GR-17 — Trace/table/menu pill copy freeze
- **Motivation**: §E/§F/§G pin exact user-visible strings; today several are implicit in code and
  would drift under refactors (e.g. `GrapherApp.cpp:1962-1972,1981-1983,3176-3181`).
- **Files**: `src/apps/GrapherApp.cpp` (string constants hoisted to one table), QA plan string list.
- **Acceptance**: all strings sourced from one `namespace grapher_strings`; semantic asserts match.
- **Risk**: low. **Owner: Sonnet.**

### GR-18 — Table honesty polish (GR-12 companion)
- **Motivation**: §F items 1–2 — visible `Δx` chip; annotated headers for non-explicitY columns.
- **Evidence**: no step display exists (`createTablePanel`, `GrapherApp.cpp:701-793`); headers
  `f%d(x)` only (`GrapherApp.cpp:2304-2310`).
- **Files**: `src/apps/GrapherApp.cpp`.
- **Acceptance**: `Δx=1` chip golden; halve/double reflects immediately; explicitY-only goldens
  re-blessed once (header bar changes) — deliberate golden wave, human-reviewed.
- **Risk**: low-medium (golden churn). **Owner: Sonnet.**

### GR-19 — Template engine generalization + expanded set
- **Motivation**: §H.5 — mini-parser only knows `sin(`/`cos(` (`GrapherApp.cpp:1387-1432`); the set
  cannot grow safely.
- **Design**: build template ASTs via the editor's own VPAM constructors (no text parser), then
  (human decision) expand to ≤10 entries incl. `y=tan(x)`, `y=sqrt(x)`, `y=ln(x)`, circle,
  one inequality. Modal paging if >6.
- **Acceptance**: template_all gate extended per entry; templates golden re-blessed.
- **Risk**: medium (input path). **Owner: Opus.** Depends: GR-02 (circle/inequality templates
  classify correctly).

### GR-20 — Editor cap feedback (no silent keystroke drops)
- **Motivation**: §C.10 — MAX_NEST=8 drops are silent (`GrapherApp.cpp:2547-2553`).
- **Design**: hint-line flash "max depth" 1 s; no behavior change to the cap itself.
- **Acceptance**: A-script: 9 nested fractions → 8 inserted + hint state assert (needs GR-14-style
  hook or golden).
- **Risk**: low. **Owner: Sonnet.**

### GR-21 — Zoom clamps
- **Motivation**: §D.7 — unbounded zoom reaches float degeneracy with no feedback.
- **Design**: clamp range to [1e-5, 1e9] world units at `handleGraphNav` zoom cases
  (`GrapherApp.cpp:2709-2727`); info bar "zoom limit".
- **Acceptance**: zoom-in ×50 stress row GC-135 flips from undefined to asserted; goldens unaffected.
- **Risk**: low. **Owner: Sonnet.**

### GR-22 — Expression-row long-expression rendering audit
- **Motivation**: §J GC-145 — the 63-char / MAX_NEST=8 envelope's *visual* behavior in the 32-px
  row is unverified (clipping risk).
- **Design**: audit + fix (horizontal scroll within the row, or metric shrink) — decision after
  audit; then golden.
- **Risk**: medium (MathCanvas geometry). **Owner: Opus.**

### GR-23 — Grapher variable-semantics pinning
- **Motivation**: GC-140/141 — `y=A`/`y=ans` read `VariableContext` defaults (0), invisible to
  Calculation's stores; until P-04 unification, current behavior must be pinned and documented.
- **Design**: docs + A-scripts only (no source change).
- **Risk**: none. **Owner: Sonnet.**

### GR-24 — Grapher product corpus execution (GC-01…GC-153)
- **Motivation**: §J — convert every `untested → A/G` row into scripts/candidates per the QA plan
  batching; supersedes/extends GR-15's 26-row scope to the full 153-row corpus.
- **Files**: `tests/emulator/scripts/*`, `scripts/generate-emulator-candidates.py`,
  `emulator-build.yml` (append), goldens via human promotion.
- **Risk**: low, volume. **Owner: Sonnet** execution, **human** promotion.

Ordering relative to the engine set: GR-00 → GR-14 → GR-02 remain first (already planned);
GR-17/20/21/23 are independent early wins; GR-18 after GR-12; GR-19 after GR-02; GR-22 anytime;
GR-24 continuous. Full cross-app ordering: `NUMOS_CORE_APPS_IMPLEMENTATION_ROADMAP.md`.

---

*End of Grapher product spec.*
