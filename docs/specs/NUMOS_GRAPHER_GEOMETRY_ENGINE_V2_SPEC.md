# NumOS — Grapher Geometry Engine v2 Specification

> Formal product + engineering spec for the Grapher's curve/relation model, plotting algorithms, geometry cache, and failure model.
>
> Audit date: 2026-07-02 · Branch `claude/grapher-geometry-engine-v2-spec-gt4pdh` @ `98f61bd` (clean tree).
> Evidence basis: full read of `src/apps/GrapherApp.{h,cpp}`, `src/apps/GraphModel.{h,cpp}`, `src/ui/GraphView.{h,cpp}`, `src/math/{Tokenizer,Parser,Evaluator,VariableContext}.*`, `src/math/MathAnalysis.h`, `tests/emulator/scripts/grapher_*.numos`, `scripts/generate-emulator-candidates.py`, `.github/workflows/emulator-build.yml`, plus `docs/specs/NUMOS_{ARCHITECTURE_GROUND_TRUTH,MEMORY_AND_PSRAM_AUDIT,FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC,RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC,FABLE_CONTEXT_HEADER}.md`.
>
> Notation: **[V]** = verified current behavior (with `file:line`), **[P]** = proposed v2 behavior (normative for implementation tickets in `NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md`). Companion docs: `NUMOS_GRAPHER_TRACE_POI_ANALYSIS_SPEC.md` (trace/POI/analysis), `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` (tests).

---

## A. Executive summary

### A.1 What the Grapher currently does (verified)

The Grapher is a three-tab (Expressions / Graph / Table) LVGL-9 app (`GrapherApp.h:17-26`) with up to **6 function slots** (`MAX_FUNCS`, `GrapherApp.h:64`). Expressions are edited as VPAM ASTs (`_exprAST[]`, `GrapherApp.h:135`), serialized to a **63-char** flat string (`syncASTtoText`, `GrapherApp.cpp:1266-1275`; `CartesianFunction::text[64]`, `GraphModel.h:55`), and compiled by the **legacy RPN pipeline** (`Tokenizer`→`Parser`→`Evaluator`, doubles; `GraphModel.h:30-33`, ground truth §G "Legacy pipeline").

Classification (`GraphModel::preCacheRPN`, `GraphModel.cpp:125-208`) recognizes three families:

1. **Explicit y=f(x)** — bare expression without `y`, `y=rhs` where rhs has no `y`, or `lhs=y` where lhs has no `y` (`GraphModel.cpp:161-169,187-198`). Plotted by an adaptive sampler streaming world-space segments (`sampleFuncAdaptive`/`adaptSegStream`, `GrapherApp.cpp:1645-1706`).
2. **Implicit relation G(x,y)=0** — everything else with `=` (including `x=y^2`, `x^2+y^2=1`, `y=x^2+y^2`) and bare expressions referencing `y` (`GraphModel.cpp:171-176,200-207`). Plotted by 16-case marching squares on a 3-px cell grid (`plotImplicit`, `GrapherApp.cpp:1716-1832`).
3. **Inequalities** `<`, `>`, `<=`, `>=` — detected by a raw character scan *before* `=` handling (`GraphModel.cpp:142-157`); region shaded with a 50 % stipple of a 55 %-whitened tint (`GrapherApp.cpp:1755-1794`; `GraphView::fillRectStipple/plotPixelStipple`, `GraphView.cpp:220-241`) plus the same G=0 boundary contour on top.

The viewport is **always equal-aspect**: `normalizeAspect()` runs at the top of every `replot()` (`GrapherApp.cpp:1843-1873`), expanding the more-zoomed axis about the center. Rendering targets a 320×143 RGB565 PSRAM buffer ("Kandinsky", `GRAPH_CANVAS_W/H`, `GrapherApp.h:159-160`; 91,520 B, memory audit §4.3) shown through an `lv_image` (`GrapherApp.cpp:589-601`).

Trace, table, auto-fit, POIs, and the Calculate menu are all **explicit-only** consumers: `evalAt` returns NAN for implicit slots by design (`GraphModel.cpp:212-216`; `CartesianFunction::isExplicit`, `GraphModel.h:80`), and trace refuses implicit-only configurations with an info-bar message (`GrapherApp.cpp:2673-2678,2731-2735`).

### A.2 What it must do (v2 mandate)

1. Introduce **explicitX (x=f(y))** as a first-class, traceable, tabulable class instead of routing it through generic marching squares (§B.3).
2. Introduce a **geometry cache** (§D) as the single source of truth for plotted geometry, replacing today's four disjoint transforms (GraphView, `drawTraceCursor`, `graphTickLabelsCb`, marching squares) and enabling contour tracing, segment-based intersections, and bounded-work replots.
3. Make **inequality strictness visible** (dashed vs solid boundary, §C.8) — today `Relation` distinguishes them but the renderer does not (`GraphModel.h:41-46`: "strict vs non-strict distinction is informational here").
4. Define an explicit **failure model** (§F): every failure has visible UI, bounded time, and a test.
5. Close silent-wrong-answer holes: 63-char truncation (reliability spec C7.3), one-sided relations (`"x="` plots x=0, §B.6), unknown multi-char identifiers (`xy=1` plots nothing silently, §C.13), Grapher-vs-system **angle-mode divergence** (§C.14).

### A.3 Explicitly out of scope for v2

- **Polar/parametric curves, sequences plotting, 3D** — no slot syntax, no renderer, no tickets.
- **Area of an inequality region / integral of implicit closed curves** — Calculate-menu items remain *disabled* for these classes (trace/POI spec §E).
- **CAS-assisted exact analysis** (symbolic roots/derivatives via `cas::`/Giac) — the Grapher stays on the numeric RPN pipeline; the two-engine split (ground truth §G) is not resolved here.
- **Anti-aliased curve rendering** — v2 keeps 1-px Bresenham (`GraphView::fastDrawLine`, `GraphView.cpp:84-118`); AA is a cosmetic follow-on (§E.8).
- **Unifying `VariableContext` with `VariableManager`** (ground truth constraint #7).
- Touch/pointer input; everything remains key-driven.

---

## B. Relation taxonomy

Normative classification is implemented in exactly one place: `GraphModel::preCacheRPN` (`GraphModel.cpp:125-208`). v2 keeps that single-point rule and extends it. Definitions below use *G(x,y) = lhs − rhs* as computed by `evalImplicit` (`GraphModel.cpp:241-261`).

Terminology: "references y" = the tokenized expression contains an `Identifier` token whose text is exactly `y` or `Y` (`GraphModel::referencesY`, `GraphModel.cpp:95-109`). Identifiers are maximal letter runs (`Tokenizer::readIdentifierOrFunction`, `Tokenizer.cpp:147-169`), so `xy` is ONE identifier and does **not** reference y (§C.13). Variables are case-insensitive at resolution (`VariableContext::indexFromName`, `VariableContext.cpp:28-32`).

### B.1 Class table (v2 normative)

| Class | Definition | Current handling [V] | v2 handling [P] |
|---|---|---|---|
| `explicitY` | `y = f(x)`: bare expr w/o y; `y=rhs` (rhs w/o y); `lhs=y` (lhs w/o y) | fast adaptive sampler | unchanged, plus geometry cache |
| `explicitX` | `x = f(y)`: `x=rhs` (rhs w/o x); `lhs=x` (lhs w/o x); rhs/lhs may reference y | **does not exist** — falls into `implicit` (`GraphModel.cpp:200-207`) | new class: y-sweep sampler (§E.2), traceable, tabulable |
| `implicit` | `G(x,y)=0` with both variables (or any `=` form not matching the explicit rules) | marching squares (`GrapherApp.cpp:1716-1832`) | marching squares + contour polylines in cache (§D.4) |
| `ineqStrict` | `G<0` / `G>0` | stipple region + solid boundary (`GrapherApp.cpp:1774-1828`) | stipple region + **dashed** boundary (§C.8) |
| `ineqNonStrict` | `G<=0` / `G>=0` | same rendering as strict | stipple region + **solid** boundary |
| `invalid` | fails classification or compilation | `valid=false`, silently not plotted (`GraphModel.cpp:132,153,205`) | `valid=false` + visible per-slot error state (§F.1) |

### B.2 explicitY — y=f(x)

- **Input examples**: `x`, `2x`, `y=x`, `y=x^2`, `sin(x)=y`, `y=1/x`, templates (`TEMPLATES[]`, `GrapherApp.cpp:82-90`).
- **Classification rule [V]**: no `=` and `!referencesY(expr)` (`GraphModel.cpp:161-169`); or `isJustY(lhs) && !rhsHasY` (`GraphModel.cpp:187-192`); or `isJustY(rhs) && !lhsHasY` (`GraphModel.cpp:193-198`). `isJustY` = trimmed string is exactly `y`/`Y` (`GraphModel.cpp:64-67`).
- **Parser/evaluator path [V]**: RHS is unary-normalized (`normalizeLeadingUnary`, `GraphModel.cpp:38-52` — the binary-only `Parser` has no unary minus, `Parser.cpp:20-22`), tokenized with implicit multiplication (`Tokenizer.cpp:223-245`), shunting-yard to RPN (`Parser.cpp:56-175`), evaluated per-x on a fixed 64-slot double stack (`Evaluator.cpp:79-253`, `Evaluator.h` STACK_MAX; reliability invariant H-6).
- **Plotted representation [V]**: 40 initial samples, ≤3 recursion levels, 2-px y-deviation refinement threshold, 3×y-range discontinuity guard (`INIT_SAMPLE_N/ADAPT_DEPTH`, `GrapherApp.cpp:73-74`; `adaptSegStream`, `GrapherApp.cpp:1645-1681`). Segments stream straight to the pixel buffer; nothing is retained.
- **Trace [V]**: full support — pixel-stepped cursor with camera follow (`handleGraphTrace`, `GrapherApp.cpp:2762-2866`).
- **Table [V]**: real values per row (`rebuildTable`, `GrapherApp.cpp:2334-2358`).
- **POI [V]**: intercept/roots/extrema/intersections vs other explicit slots (`appendPOIsForFunction`, `GrapherApp.cpp:2052-2185`).
- **Analysis menu [V]**: all 6 items operate (`executeCalcOption`, `GrapherApp.cpp:3083-3183`).
- **Failure behavior [V]**: per-x evaluation error ⇒ NAN ⇒ segment/row/cursor skipped (`GraphModel.cpp:221-224`; `GrapherApp.cpp:1697-1698,2349-2350,1930-1936`).

### B.3 explicitX — x=f(y) [P new class]

- **Input examples**: `x=y^2`, `x=3` (degenerate, constant in y), `x=sin(y)`, `y^2=x`.
- **Classification rule [P]**: after the existing `isJustY` checks, add symmetric checks: `isJustX(lhs) && !referencesX(rhs)` ⇒ explicitX with f = rhs; `isJustX(rhs) && !referencesX(lhs)` ⇒ explicitX with f = lhs. `referencesX` mirrors `referencesY` (`GraphModel.cpp:95-109`) for token text `x`/`X`. Precedence (normative, resolves `x=y` ambiguity): **explicitY rules are tested first** (`GraphModel.cpp:187-198` order preserved), so `x=y` classifies as explicitY with f(x)=x — both readings plot the same line, and explicitY gives the richer trace. `x=3` classifies explicitX (rhs `3` has no x) — NOT implicit as today.
- **Storage [P]**: reuse `rpn` for f(y); add `CartesianFunction::kind` enum {ExplicitY, ExplicitX, Implicit} replacing the boolean `implicit` (`GraphModel.h:73`). `evalAtY(fn, y)` mirrors `evalAt` (`GraphModel.cpp:212-237`) setting variable `y` instead of `x`.
- **Plotted representation [P]**: the same adaptive sampler run over **y** (swap axes): 40 initial samples across [yMin,yMax], deviation measured on screen-x, discontinuity guard 3×x-range. Marching squares is no longer used for this class (it currently draws `x=y^2` acceptably but multi-samples and cannot trace — `grapher_implicit_sideways_smoke.numos`, `grapher_aspect_sideways_parabola_smoke.numos` cover the current path).
- **Trace [P]**: cursor parameterized by y; LEFT/RIGHT move y down/up (screen-vertical motion); pill shows `x: f(y)  y: y` (trace/POI spec §A.3).
- **Table [P]**: slot's column is rendered against a `y` first-column when it is the selected table function; in mixed tables it shows `--` with header `x=f(y)` (trace/POI spec §C and ticket GR-12; ambiguity resolved there — no silent transposition).
- **POI [P]**: x-axis crossings (f(y)=0 in y), extrema in x over y, intersections via the segment-based engine only (§D.6).
- **Analysis menu [P]**: Root=solve f(y)=0 (reported as y-value), Min/Max = extremal **x**, Tangent = dx/dy converted for display (trace/POI spec §D.2), Integral **disabled** unless the y-domain UI ships (trace/POI spec §E.2).
- **Failure behavior [P]**: identical NAN-skip semantics per y.

### B.4 Implicit equality G(x,y)=0

- **Input examples**: `x^2+y^2=1`, `y=x^2+y^2` (circle of radius ½ centered (0,½) — comment `GrapherApp.cpp:1713-1715`), `x*y=1`, bare `y` (G=y), bare expressions containing y.
- **Classification rule [V]**: `=` present and neither explicit rule matches ⇒ implicit with `rpnL`=lhs, `rpn`=rhs (`GraphModel.cpp:200-207`); bare expression referencing y ⇒ G=expr, rpnL only (`GraphModel.cpp:171-176`).
- **Parser/evaluator path [V]**: both sides compiled independently (`compileExpr`, `GraphModel.cpp:83-93`); at eval, missing/empty side contributes **0.0** (`GraphModel.cpp:247-257`).
- **Plotted representation [V]**: marching squares, CELL=3 px, grid 107×48 nodes ≈ 5,136 evals per replot (`GrapherApp.cpp:1723-1739`; reliability C8.2), inside = G<0 (`GrapherApp.cpp:1802-1803`), linear edge interpolation with degenerate-denominator guard (`GrapherApp.cpp:1742-1748`), saddle cases 5/10 draw two segments (`GrapherApp.cpp:1819-1826`), NaN-corner cells skipped (`GrapherApp.cpp:1798-1799`).
- **Trace [V]**: none — `firstTraceableFunc` skips (`GrapherApp.cpp:1632-1637`); [P] arclength contour trace via cached polylines (trace/POI spec §A.4).
- **Table [V]**: column shows `--` (`GrapherApp.cpp:2349-2350` via `evalAt`→NAN); [P] unchanged (decision recorded — no y-solving in table).
- **POI [V]**: none; [P] segment-based intersections with any other cached curve (§D.6).
- **Analysis menu [V]**: unreachable via trace gating; but see the Find-Intersection partner hole (trace/POI spec §C.4). [P] support matrix in trace/POI spec §C.
- **Failure behavior [V]**: NaN cells skipped; whole-relation compile failure of *both* sides ⇒ `valid=false` (`GraphModel.cpp:205`). **One side failing is silently tolerated (=0)** — see §B.6.

### B.5 Inequalities (strict and non-strict)

- **Input examples**: `y>x^2`, `x^2+y^2<1`, `x^2+y^2>1`, `y<=2x`, `x>1`.
- **Classification rule [V]**: first `<` or `>` character anywhere in the raw text wins, before any `=` logic (`GraphModel.cpp:142-148`); `<=`/`>=` detected by `p[1]=='='`. Region truth: `regionHolds` — Less* ⇒ G<0, Greater* ⇒ G>0, NAN never inside (`GraphModel.h:98-107`).
- **Input path [V]**: `<`/`>` keys insert plain variable glyph nodes into VPAM (`GrapherApp.cpp:2603-2608`); emulator script names `lt`/`gt` (`NativeHal.cpp:561-562`). **`<=`/`>=` currently have no key** — the serializer would need `<` followed by `=` (FREE_EQ inserts `=` as a variable, `GrapherApp.cpp:2600-2601`), which works textually but has no dedicated UI. [P] v2 accepts this two-keystroke form as the canonical non-strict input and documents it; no new key.
- **Plotted representation [V]**: whole-inside cells (4 corners hold, no NaN) filled by `fillRectStipple`; mixed/NaN-edge cells re-evaluate **every pixel** (≤16 extra evals/cell) through `plotPixelStipple` (`GrapherApp.cpp:1774-1794`); boundary = same marching-squares contour on top (`GrapherApp.cpp:1796-1828`). Global checkerboard parity `(x+y)&1` guarantees seamless tiling (`GraphView.cpp:220-241`, `useStipplePixel` `GrapherApp.cpp:78-80`).
- **Strict vs non-strict [P]**: boundary stroke becomes dashed (4 px on / 4 px off along the Bresenham run) for `ineqStrict`, stays solid for `ineqNonStrict`. This is the *only* rendering difference; the shaded region is identical (sub-pixel boundary, rationale at `GraphModel.h:95-97`). Requires a dash-capable `drawSegmentPx` variant (ticket GR-11).
- **Trace [V]**: never traceable (`GraphModel.h:69-72`); [P] **decision: inequality boundary is NOT traceable in v2** (trace/POI spec §A.5 records the decision and rationale).
- **Table [V]/[P]**: `--` column, as implicit.
- **POI [P]**: boundary participates in segment-based intersections as an `implicit`-kind curve; region itself never generates POIs.
- **Analysis menu [P]**: all items disabled (trace/POI spec §C).
- **Failure behavior [V]**: both sides fail ⇒ relation reset to Equal + `valid=false` (`GraphModel.cpp:153`). One side failing ⇒ silently `0` (§B.6).

### B.6 Invalid / unsupported relations (and the one-sided hole)

**Current [V]**: `valid=false` slots are not plotted, traced, or tabulated (`GrapherApp.cpp:1886-1887`, `firstTraceableFunc` `GrapherApp.cpp:1632-1637`, `rebuildTable` filter `GrapherApp.cpp:2278-2282`). No error is shown anywhere (deliberate per reliability spec F.2 row 1). Examples: unbalanced parens (`Parser.cpp:157-171`), unknown symbol `!` (`Tokenizer.cpp:216-219`), empty slot.

**Silent-wrong-answer holes [V]** (each becomes a visible `invalid` in v2 [P]):

1. **One-sided relation**: `"x="` classifies as implicit with rhs failing to compile; `okL && !okR` still sets `valid=true` (`GraphModel.cpp:200-207`), and the missing side evaluates as 0 (`GraphModel.cpp:247-257`) ⇒ `"x="` plots the line x=0. Same for inequalities: `"y<"` shades y<0 (`GraphModel.cpp:149-156`). **[P] Rule: a relation with `=`, `<`, `>`, `<=`, `>=` whose lhs or rhs is empty or fails to compile is `invalid`.** (Ticket GR-02.)
2. **Multiple relation operators**: `y=x=2` — `strchr` takes the first `=` (`GraphModel.cpp:159`), rhs `x=2` tokenizes (Equal token passes the parser straight to output, `Parser.cpp:145-149`) and compiles, but evaluation always fails (stack ends with 2 operands, `Evaluator.cpp:239-242`) ⇒ valid slot that never plots. Chained inequalities (`0<x<1`) split on the first `<`, making rhs `x<1` fail tokenize? — no: `<` tokenize fails (`Tokenizer.cpp:216-219`, only `+-*/^(),=` accepted), so rhs fails ⇒ one-sided hole (1) applies and `0<…` shades G=0<0 = nothing. **[P] Rule: more than one relation operator (`=`,`<`,`>`) in a slot ⇒ `invalid` with reason "multiple relations".** (Ticket GR-02.)
3. **Unknown multi-char identifier**: `xy=1` — see §C.13.
4. **Truncated serialization**: AST longer than 63 serialized chars silently truncates (`syncASTtoText`, `GrapherApp.cpp:1266-1275`; reliability C7.3). **[P] Rule: if serialization hits the 63-char cap, the slot is `invalid` with reason "expression too long"** (matches reliability F.2 contract row 6). (Ticket GR-02.)

**Unsupported node types [V]**: the AST→text serializer drops `DefIntegral`, `Summation`, `Subscript`, `BigOp`, `PeriodicDecimal`, `Empty` silently (`serializeNode` default case, `GrapherApp.cpp:1260-1263`). These are not insertable from Grapher's key handler (`handleExprEdit` inserts only digits/operators/fraction/power/root/paren/sin/cos/tan/ln/log/logbase/x/y/=/</>/π/e, `GrapherApp.cpp:2555-2626`), so today they are unreachable; [P] the serializer must return a "unsupported node" failure rather than silently dropping, as defense against future editor growth.

### B.7 Multi-relation expressions (multiple slots)

- [V] Each of the ≤6 slots is classified independently; `replot()` renders all valid+visible slots in index order — explicit sampler or marching squares per slot (`GrapherApp.cpp:1886-1893`). Later slots overdraw earlier ones pixel-wise; there is no compositing.
- [V] Mixed configurations are exercised by `grapher_mixed_relations_smoke.numos` (explicit sin(x) + implicit circle + inequality disk; script header documents intent).
- [P] Draw order stays slot-index order (deterministic for goldens). Inequality regions draw before all boundaries and curves of the *same* slot only (current behavior: shading and boundary interleave per cell, `GrapherApp.cpp:1774-1828`); v2 keeps per-slot interleave — cross-slot z-order remains slot order. POI markers and overlays always draw last (`GrapherApp.cpp:1895-1905`).

### B.8 Templates

- [V] 6 hardcoded templates (`TEMPLATES[]`, `GrapherApp.cpp:82-90`: linear, parabola, sin, cos, cubic, hyperbola), opened by RIGHT on an empty expression (`GrapherApp.cpp:2534-2542`) or the per-row "Templates" button (`refreshTemplateButtons`, `GrapherApp.cpp:1443-1460`); previews are plain text labels of the exact inserted string (`loadNextTemplate`, `GrapherApp.cpp:1516-1541`); insertion builds a real VPAM AST via the mini-parser `buildTemplateAST` (`GrapherApp.cpp:1277-1437` — digits, `+ - *`, `x/y`, `=` as `NodeRelation(OpKind::Eq)`, simple `^`, simple `/`, `sin(`, `cos(` only) and immediately serializes + compiles (`GrapherApp.cpp:1582-1598`).
- [V] The template `=` serializes correctly because `OpKind::Eq` is special-cased (`serializeNode`, `GrapherApp.cpp:1129-1135` — the Phase 9F fix; regression-gated by `grapher_template_insert_smoke`/`grapher_template_all_smoke` in CI, `emulator-build.yml:525-530`).
- [P] Templates always produce `explicitY` slots; adding an implicit/inequality template requires extending `buildTemplateAST` (no ticket in v2 — out of scope; the six current templates are frozen as the golden surface).
- Failure: template insertion into an out-of-range slot is a guarded no-op (`GrapherApp.cpp:1584-1585`).

---

## C. Mathematical semantics

This section fixes the exact meaning of every operation for every class. Where the operation table below says "NAN-skip", the rule is: any per-point evaluation error (parser-level domain errors `Error: Dominio`, div-by-0, non-finite results — `Evaluator.cpp:125-167,196-219,244-248`) surfaces as NAN via `EvalResult.ok=false` → `evalAt`/`evalImplicit` NAN (`GraphModel.cpp:221-224,249-259`), and the consumer skips that point without aborting the operation.

### C.1 Operation × class matrix (v2 normative)

| Operation | explicitY | explicitX [P] | implicit | ineq(Strict/Non) | invalid |
|---|---|---|---|---|---|
| Graph | adaptive x-sweep [V] | adaptive y-sweep [P] | marching squares [V] | region + boundary [V], dash [P] | not drawn [V] |
| Table | values [V] | `--` in mixed; y-table when selected [P] | `--` [V] | `--` [V] | excluded [V] |
| Trace | full [V] | y-parameterized [P] | contour arclength [P] | none (decision) | none |
| Intersection | bisection on f1−f2 [V] → segment-based [P] | segment-based [P] | segment-based [P] | boundary participates [P] | none |
| Root | bisection over viewport [V] | f(y)=0 in y [P] | disabled | disabled | disabled |
| Min/Max | scan+golden-section [V] | extremal x over y [P] | disabled | disabled | disabled |
| Tangent | central-difference dy/dx [V] | dx/dy line [P] | gradient tangent [P] | disabled | disabled |
| Integral | Simpson over viewport [V] → interval [P] | disabled (v2) | disabled | disabled | disabled |
| Auto/Zoom fit | y-fit sampling [V] | x-fit sampling [P] | ignored [V] | ignored [V] | ignored |
| Pan/zoom | viewport ops, class-independent [V] (`GrapherApp.cpp:2700-2727`) | idem | idem | idem | idem |
| Overlay (mixed) | slot-order overdraw [V] §B.7 | idem | idem | idem | — |

### C.2 y=f(x) — exact semantics

- **Graph**: the plotted set is the polyline through samples {(xᵢ, f(xᵢ))} where xᵢ are the 40-sample grid plus adaptive midpoints; a segment is drawn iff both endpoints are finite AND |Δy| ≤ 3·(yMax−yMin) (`GrapherApp.cpp:1656-1660,1679-1680,1694-1699`). Consequence (accepted): a genuine continuous ultra-steep segment spanning >3× the view height is dropped — indistinguishable from an asymptote at sample resolution.
- **Table**: cell(r, c) = f(tblStart + (tblRow+r)·tblStep) formatted `%.6g`; NAN/Inf ⇒ `--` (`GrapherApp.cpp:2337-2353`).
- **Trace**: cursor x advances by exactly one screen pixel of world width, `(xMax−xMin)/SCREEN_W` (`GrapherApp.cpp:2763`); y = f(x) recomputed live; camera re-centers each step (`syncViewportToCursor`, `GrapherApp.cpp:2235-2253`).
- **Intersection**: sign change of f1−f2 on a 40-sample grid refined by 25-iteration bisection inside `appendPOIsForFunction` (`GrapherApp.cpp:2140-2184`); menu action uses `math::findIntersection` (100-sample grid + bisection, `MathAnalysis.h:89-100`). Tangential (non-crossing) intersections are **not found** — no sign change (documented limitation; §C.12).
- **Root/Min/Max/Tangent/Integral**: trace/POI spec §C-§E.

### C.3 x=f(y) [P]

Mirror-image of §C.2 with x↔y swapped. Displayed values are always in (x, y) screen terms: the trace pill prints `x: <f(y)> y: <y>`. A vertical-line slot `x=3` has f(y)=3 constant: graph = vertical line; trace moves along it vertically; Root means f(y)=0 (never true for `x=3` — "Not found in range" is the correct result).

### C.4 Closed implicit curves (e.g. x²+y²=1)

- [V] Marching squares produces per-cell segments with no topology; closure is emergent from the grid.
- [P] The geometry cache chains cell segments into ordered polylines (§D.4). A closed contour is a polyline whose endpoints coincide within one cell; the cache marks `closed=true`. Semantics: closure detection is **best-effort at grid resolution** — a contour exiting/re-entering the viewport is stored as multiple open polylines (each clipped at the viewport edge), never falsely joined.

### C.5 Multiple disconnected contours (e.g. x·y=1 — must be typed `x*y=1`, §C.13)

- [V] Both hyperbola branches render because marching squares is local.
- [P] Each connected chain becomes its own polyline with a `contourId`; trace UP/DOWN cycles contours (trace/POI spec §A.4).

### C.6 Vertical tangents (e.g. x=y², circle at x=±1)

- [V] Explicit sampler cannot represent them (single-valued in x); marching-squares contours draw them correctly since cells are direction-agnostic.
- [P] explicitX sampler handles the sideways parabola natively; for implicit contours, vertical tangency needs no special casing in rendering. For the Tangent analysis on implicit curves, a vertical tangent (G_y=0, G_x≠0) is drawn as a vertical line segment and the pill prints `slope: vertical` (trace/POI spec §D.4).

### C.7 Asymptotes and discontinuities

- [V] Explicit: the 3×y-range jump guard drops the straddling sub-segment after recursion has isolated it (`GrapherApp.cpp:1651-1660`); `tan(x)` renders as disconnected branches (gated by `grapher_tan_exit_smoke` in CI, `emulator-build.yml:526`). The evaluator additionally errors on tan near poles (|cos|<1e-15, `Evaluator.cpp:130-137`), log/ln/sqrt domains (`Evaluator.cpp:138-155`), div-by-0 (`Evaluator.cpp:195-200`).
- [V] Implicit: any cell with a NaN corner draws no contour (`GrapherApp.cpp:1798-1799`) and inequality shading treats NAN as outside (`GraphModel.h:98-99`).
- [P] Unchanged mechanism; the geometry cache records the dropped-segment gap as a `domainGap` marker between consecutive polylines of the same function so trace can jump across (trace/POI spec §A.3).

### C.8 Inequality boundaries

- Region: open-set test always (`regionHolds`, `GraphModel.h:98-107`) — pixels where G=0 exactly are not shaded; the boundary contour overdraws them.
- [P] Dash rule (§B.5): strict ⇒ 4-on/4-off dashed boundary; non-strict ⇒ solid. Acceptance: `y>x^2` and `y>=x^2` goldens differ only in boundary dashing (test plan §D).

### C.9 NaN/domain errors

Single normative rule (all classes): **a NaN point is invisible and inert** — no pixel, no table value (`--`), no trace position (crosshair hidden + "off domain" info text, `GrapherApp.cpp:1930-1936,1981-1983`), no POI, terminates no computation. NaN may never propagate into a drawn coordinate: `drawFunctionSegment` re-guards (`GraphView.cpp:198-202`).

### C.10 Overlapping curves

- [V] Pixel overdraw in slot order; last slot wins the pixel. Identical functions in two slots produce one visible curve (second color).
- [P] Segment-based intersection between overlapping (collinear over an interval) curves must NOT emit an unbounded POI set: collinear-overlap detection emits **zero** intersection POIs for the overlapping span (they intersect everywhere — a marker would be arbitrary). Endpoint crossings into/out of overlap are found normally. (Ticket GR-06 acceptance case.)

### C.11 Self-intersections (implicit, e.g. lemniscates)

- [V] Marching squares draws saddle cells with the fixed 5/10 disambiguation (`GrapherApp.cpp:1819-1826`) — topology at the crossing is resolution-dependent and may connect the wrong branches within one cell. Accepted as-is.
- [P] Contour chaining treats a saddle cell as a polyline break (two independent segments, never chained through) — trace never silently switches branch at a self-intersection.

### C.12 Tangency (curves touching without crossing)

- [V] Not detected by any current mechanism (bisection needs a sign change; marching squares needs a corner-sign flip). E.g. y=x² vs y=0 at x=0: f1−f2 = x² has no sign change ⇒ no Intersection POI; but the *root* finder of f1 on the axis catches x=0 as Root only if a sign change exists — x² has none, so **no POI at all** for the parabola's tangent root today (`appendPOIsForFunction` sign-change test, `GrapherApp.cpp:2085`).
- [P] v2 adds even-multiplicity root detection *only* via the local-minimum-of-|f| heuristic in the POI pass: a sample where |f| < ε_root = 1e-6·yRange and neighbors are larger emits a Root/Tangency POI, labeled `Touch`. Bounded cost: reuses the existing extremum scan (`GrapherApp.cpp:2114-2132`). Failure case: a curve grazing within ε but not touching may emit a spurious `Touch` — accepted, documented, tested (test plan §B corpus `y=x^2` vs `y=0`).

### C.13 Unknown identifiers — `xy=1` decision

[V] `xy` tokenizes as one identifier (`Tokenizer.cpp:147-169`); `resolveIdentifier` fails for length>1 names other than PI/E/ANS (`Evaluator.cpp:47-77`); classification calls `referencesY("xy")` = false, so `xy=1` becomes implicit G with lhs compiling *syntactically* but every evaluation failing ⇒ `valid=true`, plots **nothing**, silently.

**[P] Decision (no ambiguity): implicit multiplication is NOT inserted inside identifier runs.** `xy=1` is rejected at compile time: after RPN compilation, a **static dry-run evaluation** with x=1,y=1 (all registered variables defined) is performed; if it fails with "Variable no definida", the slot is `invalid` with visible reason "unknown: xy". Users must type `x*y=1`. Rationale: splitting `xy` would break future multi-letter identifiers (`pi` handled as constant, `ans`), and dry-run rejection is 1 evaluation. Caveat handled: the dry run must set both x and y to avoid falsely failing implicit slots; domain errors (e.g. `ln(-1)` at the probe point) do NOT invalidate — only the unknown-identifier error string does. (Ticket GR-02.)

### C.14 Angle mode

[V] The Grapher's `Evaluator` has its own `_angleMode`, constructed RAD (`Evaluator.cpp:24-25`); neither `GrapherApp.cpp` nor `GraphModel.cpp` ever calls `setAngleMode` (verified by grep — zero hits), while the StatusBar shown above the graph displays the **vpam** global mode (`StatusBar.cpp:240-246`, `vpam::g_angleMode`). Consequence: with the system in DEG, `y=sin(x)` plots the radian curve under a "DEG" badge — silently wrong.

**[P] Rule: at every `preCacheRPN` and at the top of `replot()`, GraphModel syncs `_eval.setAngleMode(...)` from `vpam::g_angleMode`** (`Evaluator.h:47`). Graph/table/trace/analysis all follow. Acceptance: DEG-mode `sin(x)` has period 360 in world units (test plan §B). (Ticket GR-02 scope.)

---

## D. Geometry cache architecture [P]

### D.1 Problem statement (why a cache)

Today four independent world→screen transforms coexist: `GraphView::worldToScreen*` (`GraphView.cpp:55-74`), `drawTraceCursor`'s inline math (`GrapherApp.cpp:1938-1939`), `graphTickLabelsCb`'s lambdas (`GrapherApp.cpp:485-486`), and marching squares' node mapping (`GrapherApp.cpp:1729-1730`). Geometry is produced and immediately discarded (streaming, `GrapherApp.cpp:1683-1684`), so trace-on-implicit, segment intersections, and "what did we actually draw" queries are impossible. Every consumer re-evaluates the function (camera-follow re-plots + re-POIs per keystroke, `GrapherApp.cpp:2235-2253`).

### D.2 Data model (normative)

One `GeomStore` owned by `GrapherApp` beside `_view` (`GrapherApp.h:241-242`). All coordinates stored in **world space, float**; screen positions derive through the single `GraphView` transform.

```
struct GeomPoint   { float x, y; };                       // 8 B
struct GeomPolyline {
    uint16_t funcIdx;        // source slot 0..5
    uint16_t contourId;      // per-function chain id
    uint8_t  kind;           // ExplicitY | ExplicitX | ImplicitContour | IneqBoundary
    uint8_t  flags;          // CLOSED | CLIPPED_AT_VIEWPORT | AFTER_DOMAIN_GAP | TRUNCATED
    uint16_t first, count;   // range into the shared point pool
};
struct GeomStore {
    utils::PSRAMBuffer<GeomPoint>    pool;   // fixed cap, §D.5
    std::vector<GeomPolyline>        lines;  // small; ≤ MAX_POLYLINES=64
    float xMin,xMax,yMin,yMax;               // viewport it was built for
    uint32_t buildSeq;                       // invalidation stamp
    bool     truncated;                      // any cap hit during build
};
```

- **Per-function metadata**: color and visibility stay in `CartesianFunction` (`GraphModel.h:58-60`) — the cache references slots by index, never copies style.
- **Bounding boxes**: per-polyline bbox computed at build (4 floats in `GeomPolyline` — extend struct; used for intersection broad-phase §D.6 and marker culling).
- **Visible/invisible regions**: the cache only ever contains geometry generated for the current viewport (explicit sweeps span [xMin,xMax]; marching squares spans the pixel grid). Off-viewport geometry is *not* cached — pan/zoom rebuilds (§D.3). `CLIPPED_AT_VIEWPORT` marks polylines that touch the edge so trace can stop cleanly.
- **Invalid segments / domain gaps**: never stored; a gap ends one polyline and sets `AFTER_DOMAIN_GAP` on the next (§C.7).
- **Inequality regions are NOT cached** — shading remains a direct raster pass (it is a pixel-space area, not geometry); only the boundary contour enters the store with `kind=IneqBoundary`.

### D.3 Build & invalidation (normative answers)

| Question | Answer |
|---|---|
| What is cached? | World-space polylines for every valid+visible slot: explicit sweep output (the segments currently streamed at `GrapherApp.cpp:1659,1680`), explicitX sweep output, chained marching-squares contours (§D.4), inequality boundaries. Plus per-polyline bboxes and flags. NOT cached: pixels, shading, POIs (own store `_pois`, `GrapherApp.h:209`), tick labels. |
| When is it built? | Inside `replot()` between viewport normalization and rasterization: build (or reuse) → rasterize *from the cache* → overlays. Single producer; `replot()` remains the only writer (`GrapherApp.cpp:1865-1912` structure preserved). |
| When invalidated? | Stamp compare: rebuild iff (viewport != stored) OR any slot's (text hash, valid, visible) changed OR `_plotDirty` forced. Trace camera-follow with unchanged zoom is a *translation*: v2 still rebuilds (correct, simple); an incremental-pan optimization is explicitly deferred. |
| RAM budget? | Point pool cap **4,096 points = 32,768 B** + 64 polyline headers ≈ 1.3 KB ⇒ ≤ **34 KB PSRAM**, ~37 % of the existing 91.5 KB pixel buffer (memory audit §4.3). Explicit worst case 6 slots × 321 points = 1,926; implicit worst case ≈ 2×(107+48) segment chain points per contour-rich slot — cap forces truncation, never overflow. |
| PSRAM vs internal? | Pool via `utils::PSRAMBuffer` (PSRAM-only, clean failure — `MemoryUtils.h:64-80` per memory audit §2 mech. 3). Headers via std::vector (global-new ⇒ PSRAM on firmware, memory audit §1.2). Nothing internal; nothing DMA. |
| OOM behavior? | `pool.allocate` fails ⇒ `GeomStore` disabled for the session; rendering **falls back to the current direct-streaming path** (kept compiled); trace-on-implicit and segment intersections report "unavailable (low memory)" via the info bar; `[GRAPH] geom-oom bytes=N` log (telemetry spec §2.8 grammar). No crash, no hang, no partial store. |
| Determinism for goldens? | Build order = slot index, then sweep order (x ascending / y ascending), then marching-squares row-major cell order with the fixed chain rule (§D.4). Float ops identical to today's; rasterization from cached segments must be **byte-identical** to streaming for explicit curves (acceptance gate: existing goldens `grapher_graph_smoke.ppm` etc. pass unchanged — test plan §E). |
| Cap hit? | Set `truncated=true` + per-polyline `TRUNCATED`; info bar shows "⚠ plot detail limited"; drawing continues with what fits (aligns with reliability H-3 partial-draw philosophy). |

### D.4 Contour polyline ordering (marching squares → chains)

Normative algorithm (ticket GR-03): during the existing cell sweep (`GrapherApp.cpp:1764-1831`), instead of drawing immediately, emit (cellIndex, edgeA, edgeB) segments; after the sweep, chain segments by shared cell-edge identity (edge key = (cellX, cellY, side)): start at any unconsumed segment, extend both ends while a unique continuation exists, stop at viewport edges, NaN cells, or saddle cells (§C.11). Each chain = one `GeomPolyline`; closed iff first/last edge keys match. Complexity O(segments) with a hash on edge keys; segment count is bounded by cell count (≤ 2·(106×47) segments worst case, but the 4,096-point pool cap truncates pathological cases first, §D.3). Endpoint coordinates keep the existing linear interpolation (`GrapherApp.cpp:1742-1748`) converted to world space so trace and intersections operate in world units.

### D.5 Memory limits (hard numbers)

| Item | Cap | Enforcement |
|---|---|---|
| Point pool | 4,096 pts / 32,768 B PSRAM | fixed PSRAMBuffer; build stops appending, sets TRUNCATED |
| Polyline headers | 64 | further chains merged into "truncated" or dropped with flag |
| Per-replot transient (marching grid) | 20,544 B float grid, unchanged (`GrapherApp.cpp:1728`, memory audit §4.3) | freed at end of `plotImplicit` (vector scope) |
| Total Grapher session PSRAM | ≤ 91,520 (pixels) + 34 KB (geom) + 20.5 KB (transient) ≈ **146 KB** | fits the memory-audit Grapher budget line with margin (audit §1.5 lists Grapher at 91.5 KB; new figure must be reflected in the next audit revision) |

### D.6 Segment-based intersections (consumer contract)

Intersections in v2 are computed **on cached polylines**, class-agnostic: broad-phase bbox overlap per polyline pair (different funcIdx only), narrow-phase segment/segment solve; hits refined for explicit×explicit pairs by the existing bisection (`GraphModel::findIntersections` machinery, `GraphModel.cpp:308-370`) and accepted as-is (segment precision ≈ cell/pixel size) for pairs involving implicit contours. Dedup tolerance: merge hits closer than `max(2 px world-width, 1e-4·xRange)` (unifies today's two inconsistent tolerances: `step*0.1` at `GrapherApp.cpp:2103,2170` vs `0.01` absolute at `GraphModel.cpp:349`). Budget: ≤ MAX_POIS=20 total (`GrapherApp.h:77`), first-found in deterministic build order. Full POI semantics: trace/POI spec §B.

---

## E. Plotting algorithms

### E.1 Explicit y=f(x) sampling [V, retained]

Constants frozen: `INIT_SAMPLE_N=40`, `ADAPT_DEPTH=3`, `ADAPT_THRESHOLD_PX=2.0` (`GrapherApp.cpp:73-74,1650`). Worst case 40·2³=320 evals/function ×6 slots = 1,920 evals (reliability C8.5 "acceptable; keep constants"). Refinement metric is screen-y deviation only (`GrapherApp.cpp:1667-1673`) — cheap and adequate at 143 px height. v2 change: emit into GeomStore instead of drawing directly; rasterization pass reads the store (§D.3 byte-identical requirement).

### E.2 Explicit x=f(y) sampling [P]

Same algorithm with axes swapped: 40 samples across [yMin,yMax], midpoint refinement measured on screen-x deviation, jump guard 3·(xMax−xMin). Implementation: templatize `adaptSegStream`/`sampleFuncAdaptive` over an axis selector rather than duplicating (ticket GR-02). Eval budget identical (320/function).

### E.3 Adaptive segmentation invariants

1. Recursion depth ≤ ADAPT_DEPTH — bounded stack (ESP32 loopTask 64 KB, memory audit §3 Stack row; each frame ~48 B ⇒ trivial).
2. A segment is emitted at most once; emission order is strictly x- (or y-) ascending — required for cache determinism (§D.3).
3. `yield()` every 8 initial samples (`GrapherApp.cpp:1704`) — feeds the watchdog on firmware; no-op native (`GrapherApp.cpp:34-41`).

### E.4 Discontinuity guards

- Explicit: 3×range jump drop (§C.7). The constant 3.0 is normative; changing it invalidates goldens (`grapher_tan_exit` behavior).
- Implicit: NaN-corner cell skip (`GrapherApp.cpp:1798-1799`). Note asymmetry [V]: a cell with NaN corners still gets **per-pixel inequality shading** (`GrapherApp.cpp:1783-1793` runs when `anyNan`), which is correct (region test is pointwise) — do not "fix" this.

### E.5 Marching squares [V, retained; chained per §D.4]

CELL=3 px normative (`GrapherApp.cpp:1723`); grid (⌊320/3⌋+1)×(⌊143/3⌋+1)=107×48 nodes = 5,136 G-evals per implicit slot per replot. Inside = G<0 strictly (`GrapherApp.cpp:1802`); zero corners are "outside" for the case index but the interpolator clamps t∈[0,1] with denom-0 ⇒ 0.5 (`GrapherApp.cpp:1742-1748`), so contours through exact grid zeros still draw. Saddle disambiguation fixed (cases 5/10, `GrapherApp.cpp:1819-1826`); no midpoint-value tie-break — deterministic.

### E.6 Inequality shading [V, retained]

Whole-in cells: corner test only, zero extra evals (`GrapherApp.cpp:1781-1782`). Mixed/NaN cells: per-pixel eval, ≤ (CELL+1)² = 16 evals/cell (`GrapherApp.cpp:1783-1793`). Worst case (boundary-dense, e.g. `sin(x*y)>0` at high zoom-out): all 5,015 cells mixed ⇒ ~80 k evals — this is the case the **time budget** (§E.10) must clamp; reliability C8.2 documents multi-second freezes today.

### E.7 Boundary drawing

G=0 contour after shading, same color as the slot, 55 %-white tint reserved for the region only (`GrapherApp.cpp:1758-1762`). [P] dash for strict (§C.8): dashing is applied in `drawSegmentPx` by a phase counter over the Bresenham pixel run — per-segment phase resets are acceptable (cell segments are ≤ ~4 px; a 4/4 dash across resets still reads as dashed; acceptance is the golden, not dash phase continuity).

### E.8 Anti-aliasing expectations

None. 1-px Bresenham RGB565 (`GraphView.cpp:84-118`), no blending; the buffer is byte-compared in CI (ground truth §"Goldens are byte-exact"). Any future AA is a golden-invalidating event and requires full re-blessing — out of v2 scope (§A.3).

### E.9 Equal-aspect viewport normalization [V, retained]

`normalizeAspect` expands the more-zoomed axis about center to the larger units-per-pixel; idempotent; NaN/degenerate-guarded (`GrapherApp.cpp:1843-1863`). It runs before the layout-readiness early-out so the canonical viewport is square on every intent (`GrapherApp.cpp:1869-1876`). **Interaction with Auto-fit [V, documented semantics]**: `autoFit()` fits y tightly (`GrapherApp.cpp:2011-2049`), then `normalizeAspect` re-expands y if x is the coarser axis — Auto therefore means "center the y-range of interest under equal aspect", not "fill the view vertically". This is intentional; the spec freezes it (test plan §B oracle for Auto).

### E.10 Tick/grid/label rules [V, retained]

Single shared "nice" step 1/2/5×10ⁿ targeting ~48 px from `GraphView::squareGridStep` (`GraphView.cpp:124-132`), used by BOTH the grid rasterizer (`GraphView.cpp:134-169`) and the label callback (`GrapherApp.cpp:497,517-519`) — labels always sit on grid lines; equal aspect ⇒ square cells. Labels: integers as `%d`, else `%.1g`; zero suppressed; edge-clamped (`GrapherApp.cpp:500-534`). Axes drawn only when 0 is inside the range (`GraphView.cpp:171-185`).

### E.11 Performance budget (normative, aligns reliability H-3)

| Operation | Budget | Enforcement |
|---|---|---|
| Full `replot()` (6 slots incl. 1 implicit + 1 inequality) | **≤ 200 ms** wall | budget check every marching-squares row and every 8 explicit samples; on expiry: stop producing, draw what exists, info bar "⚠ plot truncated", `[GRAPH] replot … aborted=1` (telemetry §2.8) — ticket GR-13 |
| Trace keystroke (camera-follow replot included) | ≤ 100 ms | consequence of replot budget; POI recompute stays async (`startAsyncPOI` 10 ms slices, `GrapherApp.cpp:2981-2995`) |
| Table rebuild (21 rows × ≤6 funcs = ≤126 evals, `GrapherApp.cpp:2330-2358`) | ≤ 50 ms | bounded by construction (reliability C8.4); no new guard |
| Calc-menu op | ≤ 500 ms | MAX_ITER=100 / N=100 bounds (`MathAnalysis.h:50`, `MathAnalysis.cpp:236`) + budget wrap, "no result (timeout)" per reliability F.2 |

---

## F. Error / failure model

Every row: visible UI + log (telemetry `[GRAPH]` grammar, diagnostics spec §2.8) + no-hang invariant + test (test plan doc). "Info bar" = `_infoLabel` (`GrapherApp.cpp:671-672`); "slot error state" [P] = red border + short reason label on the expression row (ticket GR-10 UI).

| # | Failure | Detection point | Visible UI | Log | No-hang invariant | Test |
|---|---|---|---|---|---|---|
| F.1 | Invalid expression (tokenize/parse fail) | `preCacheRPN` (`GraphModel.cpp:149-153,188,194,201-205`) | [V] silently unplotted → [P] slot error state "syntax" | none [V] → `[GRAPH] invalid slot=N reason=syntax` [P] | classification is O(len) | `grapher_invalid_expr` (new, test plan §B) |
| F.2 | One-sided / multi-relation / unknown-identifier / >63-char (§B.6, §C.13) | `preCacheRPN` [P] rules | slot error state with reason | `[GRAPH] invalid slot=N reason=…` | dry-run adds exactly 1 eval | new corpus rows |
| F.3 | Evaluator domain error at a point (`Error: Dominio`, div-0, `Evaluator.cpp:125-207`) | per-eval | point simply absent (§C.9); trace shows `y=--- (off domain)` (`GrapherApp.cpp:1981-1983`) | none (per-point too hot) | eval is iterative, fixed stack (H-6) | `grapher_trace_domain_smoke.numos` [V exists] |
| F.4 | Overflow / NaN / Inf result | `Evaluator.cpp:164-167,216-219,244-248` reject non-finite | as F.3 | none | same | `y=1/x`, `y=tan(x)` corpus |
| F.5 | Geometry pool cap hit (too many segments) | GeomStore append [P] | "⚠ plot detail limited" info bar | `[GRAPH] geom-trunc pts=4096` | build loop bounded by caps | stress corpus `grapher_breakit_stress.numos` extended |
| F.6 | Too many intersections (> MAX_POIS 20) | POI append (`GrapherApp.cpp:2080,2144` guards) | markers stop appearing beyond 20 [V]; [P] info bar "POIs limited to 20" once | `[GRAPH] poi-cap` | array-bounded | `y=sin(10x)` vs `y=0` corpus |
| F.7 | Pixel-buffer OOM | `createGraphPanel` alloc (`GrapherApp.cpp:564-587`) | [V] red "ERR: INSUFFICIENT PSRAM" label, app navigable | [V] serial FAIL print `GrapherApp.cpp:579` + [P] `[GRAPH] buf-fail bytes=91520` (reliability F.2 row 3) | no draw path touches null buffer (`replot` guard `GrapherApp.cpp:1876`) | firmware-only manual (test plan §E) |
| F.8 | GeomStore OOM | pool.allocate fail [P] | features degrade per §D.3 | `[GRAPH] geom-oom` | fallback = current streaming path | emulator alloc-fault injection (test plan §E) |
| F.9 | Replot time budget exceeded | row/sample budget checks [P] | "⚠ plot truncated" info bar | `[GRAPH] replot … aborted=1` | H-3: single pass ≤ 200 ms | `grapher_curve_stress.numos` promoted to gate |
| F.10 | Trace unavailable (no traceable slot) | `firstTraceableFunc` < 0 (`GrapherApp.cpp:2673-2678,2731-2735`) | [V] info bar "Trace n/a: no y=f(x) (implicit/inequality)" | none | immediate return | `grapher_implicit_tabletrace_safe.numos` [V exists] |
| F.11 | Table unavailable (no valid slot) | `rebuildTable` numActive==0 (`GrapherApp.cpp:2285,2355-2357`) | [V] single `f(x)` column of `--` | none | bounded 21 rows | `grapher_table` empty-state (new) |
| F.12 | Analysis finds nothing | `res.found=false` (`GrapherApp.cpp:3176-3181`) | [V] pill "Not found in range" | [P] `[GRAPH] calc op=… status=none` | MAX_ITER bounded | calc-menu corpus (trace/POI spec §C) |
| F.13 | Analysis wrong-class request | [P] support matrix gate (trace/POI spec §C) | disabled row + "n/a for this curve" pill | `[GRAPH] calc refused` | gate is O(1) | disabled-state golden (test plan §D) |

Non-negotiable invariant inherited from CI: **MODE/Home exits from every Grapher state without hang** — gated by `grapher_home_return_smoke` (`emulator-build.yml:525`); every new modal/overlay added by v2 must be reachable by that script's walk or extend it.

---

*End of Document 1. Trace/POI/analysis semantics: `NUMOS_GRAPHER_TRACE_POI_ANALYSIS_SPEC.md`. Test oracle & CI: `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md`. Tickets: `NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md`.*
