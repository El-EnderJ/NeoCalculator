# NumOS — Grapher Geometry Engine v2: Implementation Tickets

> Ordered, dependency-aware ticket set implementing `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md` (GEOM) and `NUMOS_GRAPHER_TRACE_POI_ANALYSIS_SPEC.md` (TPA), tested per `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` (TEST). Audit base `98f61bd`, 2026-07-02.
>
> Global rules for every ticket: (1) never edit `tests/emulator/golden/**`, `masks/**`, or `NativeHal.cpp`'s existing script grammar except append (ground truth fragile-files list); (2) any renderer-visible change re-runs `scripts/generate-emulator-candidates.py` and gets human golden review; (3) `end()` teardown additions follow the reference pattern `GrapherApp.cpp:167-228` (reliability L-5); (4) emulator whitelist additions to `platformio.ini:195-266` are forbidden here — all files below are already whitelisted (Grapher, GraphModel, GraphView, Tokenizer/Parser/Evaluator/VariableContext, MathAnalysis).
>
> Model-owner legend: **Sonnet** = mechanical/bounded, **Opus** = multi-file feature with regression risk, **Fable** = cross-cutting semantics/architecture, **human** = judgment or hardware.

---

## GR-00 — CI wiring for existing Phase-10 scripts (do first, no source changes)

- **Motivation**: all implicit/inequality/aspect behavior is currently untested in CI (TEST §A T4 row; verified zero matches in `emulator-build.yml`).
- **Source evidence**: gate loop pattern `emulator-build.yml:495-531`; candidate list `generate-emulator-candidates.py:39-143`.
- **Files**: `.github/workflows/emulator-build.yml`, `scripts/generate-emulator-candidates.py` (list only).
- **Design**: append ~18 stems to the `run_grapher` no-hang loop (TEST §E.1); add the same stems to `CANDIDATES` with frame budgets copied from each script's header.
- **Invariants**: existing gates untouched; new stems warn-only at golden-compare until blessed.
- **Acceptance**: CI green with new steps; deliberately breaking `plotImplicit` (local experiment) turns the gate red.
- **Visual/semantic/firmware tests**: none new — this ticket IS the test wiring.
- **Memory/perf budget**: CI wall +~2 min (18 runs × ~5 s).
- **Rollback**: revert the two files.
- **Risk**: low. **Owner: Sonnet.**

## GR-01 — Transform unification (single world↔screen authority)

- **Motivation**: four parallel transforms (GEOM §D.1) are an off-by-one/regression factory; prerequisite hygiene for the cache.
- **Source evidence**: `GraphView.cpp:55-74` vs `GrapherApp.cpp:1938-1939` (trace), `:485-486` (tick labels), `:1729-1730` (marching nodes), `:3216` (shading recomputes x from px inline).
- **Files**: `src/apps/GrapherApp.cpp`, `src/ui/GraphView.h/.cpp`.
- **Design**: route `drawTraceCursor`, `graphTickLabelsCb` (offset by `coords.x1/y1`), and `drawIntegralShading` through `GraphView::worldToScreen*/screenToWorld*`; add float-exact equivalence comments where int truncation matters.
- **Invariants**: **byte-identical goldens** — the six blessed Grapher goldens must pass unchanged (transforms are numerically identical today; the ticket only de-duplicates).
- **Acceptance/semantic**: full T1 gate suite + all 6 goldens byte-pass; trace pill coordinates unchanged in `grapher_trace_smoke`.
- **Firmware test**: none needed (pure refactor).
- **Budget**: zero heap delta; zero perf delta.
- **Rollback**: revert commit. **Risk**: low-medium (int-truncation subtleties). **Owner: Sonnet.**

## GR-02 — Classification hardening: explicitX + invalid-relation rules + angle-mode sync

- **Motivation**: `x=y^2`/`x=3` deserve first-class treatment (GEOM §B.3); `"x="` plotting x=0, `xy=1` plotting nothing, >63-char truncation, and RAD-lock are silent wrong answers (GEOM §B.6, §C.13, §C.14).
- **Source evidence**: `GraphModel.cpp:125-208` (classifier), `:247-257` (missing-side=0), `GrapherApp.cpp:1266-1275` (63-char cap), `Evaluator.cpp:24-25` + zero `setAngleMode` calls in Grapher sources (verified), `StatusBar.cpp:240-246` (badge reads vpam mode).
- **Files**: `src/apps/GraphModel.h/.cpp` (kind enum, `evalAtY`, dry-run validation, angle sync), `src/apps/GrapherApp.cpp` (axis-templated sampler §E.2, serialization-cap error), `src/apps/GrapherApp.h`.
- **Design**: GEOM §B.1-§B.6 and §C.14 verbatim: `kind` enum replaces `implicit` bool (keep a `bool implicit() const` shim so `isExplicit` call sites don't churn); reject one-sided/multi-relation/unknown-identifier/overlong; sync `_eval.setAngleMode` from `vpam::g_angleMode` in `preCacheRPN` and `replot()`.
- **Invariants**: explicitY classification outcomes for corpus #1-4, 13-23 unchanged; `x=y` stays explicitY (precedence rule GEOM §B.3).
- **Acceptance (semantic)**: corpus rows 5, 6, 9, 24, 25 flip to their [P] oracles; DEG-mode sine period-360 (TEST §B #13).
- **Visual**: re-bless `grapher_implicit_sideways` (now sampled, smoother) — expected golden change, human-reviewed; all inequality/circle goldens unchanged.
- **Firmware**: smoke on device: `x=y^2` traces vertically without watchdog reset.
- **Budget**: +1 dry-run eval per compile; sampler evals unchanged (320/slot).
- **Rollback**: kind enum is additive; revert re-routes explicitX to implicit.
- **Risk**: medium — classifier is the semantic heart; every branch needs a corpus row. **Owner: Fable** (semantics) with Sonnet follow-up for the sampler templatization.

## GR-03 — Implicit contour polyline ordering (marching squares → chains)

- **Motivation**: unordered per-cell segments block contour trace, intersections, and closure detection (GEOM §D.4).
- **Source evidence**: `GrapherApp.cpp:1764-1831` (cell sweep draws immediately); saddle cases `:1819-1826`.
- **Files**: `src/apps/GrapherApp.cpp` (emit segments), new `src/apps/GraphGeom.h/.cpp` (chainer — must be added to the emulator whitelist `platformio.ini:195-266`; this is the one permitted whitelist edit, mirroring how GraphModel/GraphView entered in Phase 8G).
- **Design**: edge-key hash chaining per GEOM §D.4; saddle cells break chains; output world-space polylines; rasterization of chained output must reproduce today's pixel set (same interpolated endpoints).
- **Invariants**: implicit goldens (once blessed via GR-00) byte-identical — chaining changes *order*, not pixels; deterministic chain ordering (row-major first-seen).
- **Acceptance**: circle → exactly 1 closed polyline at default zoom; `x*y=1` → 2 open polylines; NaN-holed curve (`sqrt(x)+sqrt(y)=2` style) → chains break at domain cells.
- **Visual**: implicit goldens unchanged. **Semantic**: `assert_graph_poi_count`-style hook deferred to GR-14; interim asserts via logs.
- **Firmware**: stress `grapher_breakit_stress` on device, no WDT.
- **Budget**: transient hash + segment list ≤ 2×5,015 entries ≈ 80 KB worst-case transient PSRAM — cap at the 4,096-point pool (GEOM §D.5) BEFORE chaining by truncating the sweep; document in `[GRAPH] geom-trunc`.
- **Rollback**: keep direct-draw path compiled; flag-select.
- **Risk**: medium. **Owner: Opus.**

## GR-04 — Geometry cache (GeomStore) unification

- **Motivation**: single source of truth for plotted geometry (GEOM §D) — enables GR-05/06/07, trace-on-implicit, budget-bounded replots.
- **Source evidence**: streaming discard (`GrapherApp.cpp:1683-1706`), camera-follow full replot per keystroke (`:2235-2253`), duplicated transforms (GR-01).
- **Files**: `src/apps/GraphGeom.h/.cpp` (store), `src/apps/GrapherApp.cpp` (build in `replot()`, rasterize from store), `src/apps/GrapherApp.h`.
- **Design**: GEOM §D.2-§D.5 verbatim: PSRAMBuffer pool (4,096 pts / 32 KB), 64 polyline headers, viewport+slot-hash invalidation, OOM fallback to streaming, `truncated` flag + info-bar notice.
- **Invariants**: explicit-curve rasterization from cache is **byte-identical** to streaming (GEOM §D.3 determinism row) — gate: all blessed goldens pass without re-bless; OOM path never crashes (emulator alloc-fault injection via a NATIVE_SIM test hook forcing `allocate()` failure).
- **Acceptance**: goldens byte-pass; self-diff T5 green; `[GRAPH] geom-oom` path exercised in a dedicated script.
- **Firmware**: measure PSRAM before/after in Grapher session (target ≤146 KB total, GEOM §D.5); confirm on-device replot latency unchanged ±10 %.
- **Budget**: +34 KB PSRAM session; build cost = existing eval cost (no re-eval; segments captured in-line).
- **Rollback**: compile-time flag `NUMOS_GRAPH_GEOMSTORE=0` restores streaming.
- **Risk**: high (touches every draw path). **Owner: Fable** design review + **Opus** implementation.

## GR-05 — POI coverage fix + async determinism

- **Motivation**: slots before the traced one get no POIs (TPA §B.2.1); tolerance inconsistency (TPA §B.2.2).
- **Source evidence**: `GrapherApp.cpp:2981-3015` (fi…N−1 iteration), `:2065,2103,2121,2170` vs `GraphModel.cpp:349` (tolerances).
- **Files**: `src/apps/GrapherApp.cpp`.
- **Design**: `startAsyncPOI` always sweeps 0…N−1; unify dedup tolerance to `max(2·px_world, 1e-4·xRange)`; keep the 10-ms slicing and MAX_POIS=20.
- **Invariants**: POI determinism under `--deterministic` (candidate comment `generate-emulator-candidates.py:106-112` must stay true); snapping thresholds unchanged (`GrapherApp.h:79`).
- **Acceptance**: trace slot 3 of {y=x, y=x², y=x³, y=2x} — intersection markers for the (0,1) pair now appear.
- **Visual**: `grapher_intersect_markers` golden (new, TEST §D). **Firmware**: none specific.
- **Budget**: worst case unchanged: ≤6 slots × (40-grid scans) per rebuild, async-sliced.
- **Rollback**: revert. **Risk**: low. **Owner: Sonnet.**

## GR-06 — Segment-based intersections (all curve kinds)

- **Motivation**: implicit curves currently intersect nothing (TPA §B.2.3); Find Intersection silently fails against implicit partners (TPA §C.1 hole).
- **Source evidence**: `GrapherApp.cpp:2140-2184` (evalAt-based), `:3117-3131` (partner pick), `GraphModel.cpp:308-370` (bisection refiner to reuse).
- **Files**: `src/apps/GraphGeom.cpp` (pairwise polyline intersect), `src/apps/GrapherApp.cpp` (POI feed + menu action).
- **Design**: GEOM §D.6 (bbox broad-phase, segment solve, explicit-pair bisection refinement, collinear-overlap zero-POI rule GEOM §C.10, unified dedup, ≤20 budget, `≈` pill marker for unrefined hits per TPA §C.3).
- **Invariants**: explicit×explicit results match today's bisection results within tolerance (regression corpus #21).
- **Acceptance**: `y=x` ∩ circle r=2 → two `≈` markers; overlap case `y=x` twice → zero markers; cap case `y=sin(10x)` vs `y=0` → 20 markers + "POIs limited" notice.
- **Visual**: mixed-intersections golden. **Firmware**: stress timing within replot budget.
- **Budget**: ≤64² bbox tests + segment count product bounded by pool cap; O(n²) worst ≈ 4M segment pairs is NOT acceptable — require bbox rejection first and per-pair early exit at 20 hits (documented in code).
- **Rollback**: keep evalAt-based path behind flag.
- **Risk**: medium-high (numerics + perf). **Owner: Opus.**

## GR-07 — Trace v2: seed selection, contour trace, branch switching

- **Motivation**: TPA §A.3-§A.4 — explicitX trace and implicit contour trace.
- **Source evidence**: trace machinery `GrapherApp.cpp:2762-2866`, traceable gate `:1632-1637`, camera-follow `:2235-2253`.
- **Files**: `src/apps/GrapherApp.cpp/.h`.
- **Design**: TPA §A.3 (y-parameterized explicitX), §A.4 (arclength walk, center-nearest seed, contourId cycling, rebuild re-anchor, lost-contour exit). Traceable set = explicitY ∪ explicitX ∪ implicit-with-polylines; inequality stays excluded (TPA §A.5 decision).
- **Invariants**: explicitY trace behavior byte-identical (`grapher_trace_smoke` golden passes); deterministic seed/step (pure function of cache).
- **Acceptance**: circle trace wraps closed contour; `x*y=1` UP/DOWN cycles 2 branches; zoom-out until contour vanishes → "trace lost contour" + NAVIGATE.
- **Visual**: `grapher_trace_implicit` golden. **Semantic**: `assert_graph_trace` hook (GR-14). **Firmware**: trace latency ≤100 ms/keystroke on device.
- **Budget**: zero new allocations (walks the cache).
- **Rollback**: gate expansion behind flag; explicit-only fallback.
- **Risk**: medium. **Owner: Opus.**

## GR-08 — Calculate-menu support-matrix enforcement + integral interval UI

- **Motivation**: no menu item may silently do the wrong thing (TPA §C.4); Find Intersection hole (TPA §C.1); viewport-bound integral surprise (TPA §E.1); root-nearest-cursor pin (TPA §C.3).
- **Source evidence**: `GrapherApp.cpp:2910-2975` (menu build), `:3083-3183` (dispatch), `MathAnalysis.h:69-113`.
- **Files**: `src/apps/GrapherApp.cpp/.h`.
- **Design**: TPA §C.2 matrix computed at `openCalcMenu`; disabled rows gray + refusal pill; dispatch re-check; two-press integral interval (TPA §E.1) with signed-area pill text; root search outward from cursor; Min/Max view-edge suffix; explicitX actions per matrix.
- **Invariants**: explicitY happy-path results identical (same backends, same constants MAX_ITER=100/N=100).
- **Acceptance**: full matrix walk per class (16 class×action cells asserted); disabled-ENTER shows pill and menu stays open; integral over [a,b] equals Simpson reference for x² on [0,1] = 0.3333 ±1e-4.
- **Visual**: `grapher_calcmenu_explicit`, `grapher_calcmenu_implicit_disabled`, disabled-action golden (TEST §D).
- **Firmware**: none specific. **Budget**: none.
- **Rollback**: matrix flag-off restores current all-enabled menu.
- **Risk**: medium (UX-visible). **Owner: Opus**, matrix content signed off by **human**.

## GR-09 — Tangent for implicit curves (+ explicitX tangent, vertical display)

- **Motivation**: TPA §D.2-§D.4.
- **Source evidence**: `drawTangentOverlay` (`GrapherApp.cpp:3250-3276`), FD constants `:75-76`.
- **Files**: `src/apps/GrapherApp.cpp`, `src/apps/GraphModel.cpp` (gradient probes via `evalImplicit`).
- **Design**: ∇G central differences with the §D.4 epsilon policy; tangent dir (−Gy, Gx); vertical/singular/domain pills; explicitX dx/dy variant; slope added to the explicitY pill.
- **Invariants**: explicitY tangent line pixels unchanged (only pill text gains slope — mask-free golden change, re-bless `grapher_trace_smoke`? No — tangent isn't in that golden; new goldens only).
- **Acceptance**: circle at (1,0) → vertical tangent + `slope: vertical`; at (0,1) → horizontal, slope 0.0000; center of `x^2+y^2=0`-style singular → `tangent n/a (singular point)`.
- **Visual**: implicit-tangent golden. **Firmware**: none specific.
- **Budget**: 4 extra evals per tangent draw.
- **Rollback**: disable tangent row for implicit (matrix flag).
- **Risk**: low-medium. **Owner: Sonnet** with formula review by **Fable**.

## GR-10 — Disabled-action & slot-error UI

- **Motivation**: failure model requires visible states (GEOM §F.1-F.2, F.13); currently invalid slots look identical to valid ones.
- **Source evidence**: silent invalid (`GraphModel.cpp:132`, reliability F.2 row 1 "no error dialog (deliberate)" — superseded by this ticket with a lightweight state, not a dialog); expression row styling (`GrapherApp.cpp:952-991`).
- **Files**: `src/apps/GrapherApp.cpp/.h`.
- **Design**: invalid slot ⇒ red row border + small reason text (montserrat_14 — STIX has no space glyph, `GrapherApp.cpp:435-438`); disabled menu rows gray (GR-08 consumes); error state cleared on next successful compile. All new widgets registered in `end()` teardown.
- **Invariants**: valid-slot visuals unchanged (existing goldens pass); LVGL-pool delta ≤ +6 labels (~1 KB pool — pool is the scarce 64 KB resource, memory audit §5.3).
- **Acceptance**: corpus row 24 asserts error state; `grapher_slot_error` golden.
- **Firmware**: LVGL pool headroom check on device with 6 error rows.
- **Rollback**: revert; classification errors fall back to silent-invalid.
- **Risk**: low. **Owner: Sonnet.**

## GR-11 — Inequality boundary: dashed-strict rendering + trace-refusal wording

- **Motivation**: strict/non-strict visually identical today (GEOM §B.5, §C.8); refusal message should teach the workaround (TPA §A.5).
- **Source evidence**: `GraphModel.h:41-46` (informational comment), boundary draw `GrapherApp.cpp:1796-1828`, refusal text `:2676,2734`.
- **Files**: `src/ui/GraphView.h/.cpp` (dashed `drawSegmentPx` variant), `src/apps/GrapherApp.cpp`.
- **Design**: 4-on/4-off pixel-phase dash per GEOM §E.7 (per-segment phase reset accepted); relation passed to the boundary pass; refusal message extended.
- **Invariants**: non-strict and equality contours byte-identical to today; region shading untouched.
- **Acceptance**: `y>x^2` vs `y>=x^2` golden pair differs only on boundary pixels (TEST §D).
- **Visual**: the golden pair. **Firmware**: none. **Budget**: none.
- **Rollback**: dash flag off. **Risk**: low. **Owner: Sonnet.**

## GR-12 — Table semantics for non-y=f(x)

- **Motivation**: explicitX slots deserve a real table; implicit stays `--` by decision (GEOM §C.1; TPA — no silent transposition).
- **Source evidence**: `rebuildTable` (`GrapherApp.cpp:2272-2359`), header labels `:2299-2317`, `--` path `:2349-2350`.
- **Files**: `src/apps/GrapherApp.cpp`.
- **Design**: when the table's function set contains exactly one explicitX slot and no explicitY slots, first column becomes `y` and the slot column header `x=f(y)`; any mixed set keeps the x-table with `--` for explicitX/implicit columns (headers annotated `x=f(y)`/`G=0`). No user toggle in v2 — the rule is deterministic from slot kinds (ambiguity resolved by rule, not UI).
- **Invariants**: explicitY-only tables byte-identical (`grapher_table_smoke`, `grapher_multifn_table` goldens).
- **Acceptance**: `x=y^2` alone → y-table with values 0,1,4…; `y=x` + `x=y^2` → x-table, second column `--` with annotated header.
- **Visual**: y-table golden. **Firmware**: none. **Budget**: none (same 21×N evals).
- **Rollback**: revert to `--`. **Risk**: low. **Owner: Sonnet.**

## GR-13 — Replot time/evaluation budget guard

- **Motivation**: reliability H-3 (200 ms replot bound) and C8.2 (inequality worst case ~80 k evals, multi-second freeze); GEOM §E.11, F.9.
- **Source evidence**: synchronous replot in key handlers ending in `lv_refr_now` (`GrapherApp.cpp:2757-2758,2865-2866,1909`; reliability C8.1); per-pixel shading loop (`:1783-1793`); native `yield()` no-op (`:34-41`).
- **Files**: `src/apps/GrapherApp.cpp`, `src/ui/GraphView.*` (none expected), telemetry lines per diagnostics spec §2.8.
- **Design**: dual budget — wall-clock 200 ms on firmware AND an evaluation-count budget (e.g. 30,000 evals) that is the binding limit under NATIVE_SIM `--deterministic` (so CI can test truncation deterministically, TEST §E.5); checks every marching row / 8 explicit samples; on expiry stop, draw partial, info-bar "⚠ plot truncated", `[GRAPH] replot … aborted=1`.
- **Invariants**: budget never triggers for the entire TEST §B corpus at default zoom (headroom ≥4× measured); truncation is deterministic under determinism.
- **Acceptance**: stress corpus triggers truncation banner; normal corpus never does; T1 no-hang gates stay green.
- **Firmware**: verify TWDT never fires during worst-case inequality zoom-out on device (reliability H-1 interplay).
- **Budget**: the ticket IS the budget. **Rollback**: guard flag off.
- **Risk**: medium (partial-draw visuals under truncation must remain deterministic for any golden that intersects it — rule: no golden may be captured in a truncated state except the dedicated truncation golden). **Owner: Opus.**

## GR-14 — Grapher semantic assert hooks + script grammar extensions

- **Motivation**: classification/trace/POI/table oracles need font-independent asserts (TEST §C, §E.4).
- **Source evidence**: hook precedent `CalculationApp::debugLastResult` / `MainMenu::debugFocusedCardId` (ground truth §C); grammar append-only (`NativeHal.cpp:1447-1612`, fragile-files list).
- **Files**: `src/apps/GrapherApp.h` (NATIVE_SIM-only accessors), `src/hal/NativeHal.cpp` (append `assert_graph_kind`, `assert_graph_trace`, `assert_graph_poi_count`, `assert_graph_table_cell`).
- **Design**: read-only accessors; asserts exit 4 on mismatch like existing ones; documented in `docs/emulator-sdl2-quickstart.md`.
- **Invariants**: zero firmware footprint (`#ifdef NATIVE_SIM`); existing scripts unaffected.
- **Acceptance**: negative control — deliberately wrong assert exits 4 (TEST §D negative controls).
- **Rollback**: hooks are additive. **Risk**: low. **Owner: Sonnet.**

## GR-15 — Graph stress corpus + golden promotion plan execution

- **Motivation**: lock v2 behavior for the full TEST §B corpus; formalize promotion.
- **Source evidence**: promotion tooling `scripts/promote-emulator-golden.py` (human-only, ground truth constraint #6); mask policy TEST §D.
- **Files**: `tests/emulator/scripts/*` (new scripts for corpus rows 9, 21, 24, 25, calc-menu, negative controls), `scripts/generate-emulator-candidates.py` (stems), `.github/workflows/emulator-build.yml` (gate + self-diff step TEST §E.3), goldens via human promotion only.
- **Design**: one script per corpus row lacking coverage; T5 self-diff step; T6 negative controls; promotion checklist (Linux-generated, visually reviewed, mask = clock only).
- **Invariants**: no golden promoted from a truncated or nondeterministic frame; every new golden has a mask review.
- **Acceptance**: corpus table fully mapped to scripts (traceability table committed alongside); CI green.
- **Firmware**: manual checklist per TEST §E.6 executed once and recorded.
- **Rollback**: scripts are additive. **Risk**: low, but volume. **Owner: Sonnet** execution, **human** promotion.

## GR-16 — "Axes" toolbar item: implement or remove (cleanup)

- **Motivation**: the toolbar's "Axes" tool is a UI affordance with an **empty handler** — pressing it does nothing (`handleToolbar` case 1, `GrapherApp.cpp:2660-2661`), violating the no-dead-affordance principle behind TPA §C.4.
- **Files**: `src/apps/GrapherApp.cpp`.
- **Design (decision required, default = implement)**: toggle a `_gridVisible` flag consumed by `replot()` (skip `drawGrid`, keep axes); alternative is removing the label and renumbering `_toolLabels[4]`→3 (golden-invalidating for `grapher_graph_smoke`— avoid). Implement-toggle changes no default visuals.
- **Acceptance**: toggle golden pair; default-state goldens unchanged.
- **Risk**: low. **Owner: Sonnet**, decision by **human**.

---

## Ordering & dependency summary

```
GR-00 (CI wiring)                         — immediately
GR-01 (transforms)                        — anytime; before GR-04
GR-02 (classification+angle)              — before GR-07/08/12; blesses sideways golden
GR-03 (contour chains)  ─┐
GR-01, GR-02             ├─→ GR-04 (GeomStore) ─→ GR-06 (intersections) ─→ GR-05 tolerance part
                          │                     └→ GR-07 (trace v2)
GR-05 (POI coverage)      — independent, early win
GR-08 (menu matrix)       — after GR-02; full value after GR-06/07
GR-09 (implicit tangent)  — after GR-07
GR-10 (error UI)          — after GR-02
GR-11 (dash)              — independent
GR-12 (table)             — after GR-02
GR-13 (budget)            — after GR-04 (budget checks live in the build pass)
GR-14 (hooks)             — early, unlocks semantic asserts for everything above
GR-15 (corpus+promotion)  — continuous; final sweep last
GR-16 (Axes)              — anytime
```

*End of Document 4.*
