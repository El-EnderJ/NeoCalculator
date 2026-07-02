# NumOS — Grapher Test Oracle & Visual Coverage Plan

> Companion to `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md` and `NUMOS_GRAPHER_TRACE_POI_ANALYSIS_SPEC.md` (audit base `98f61bd`, 2026-07-02). Defines the test taxonomy, the required expression corpus with expected oracles, the golden/mask coverage plan, and the CI gating plan.
>
> Harness facts used throughout: `.numos` command vocabulary and exit codes (0 pass / 2 parse / 3 screenshot / 4 assert — `NativeHal.cpp:1447-1612` per ground truth §D); deterministic tick via `--deterministic` (ground truth §"How to interpret"); inequality keys `lt`/`gt` (`NativeHal.cpp:561-562`); golden compare via `scripts/compare-ppm.py` with `x,y,w,h` masks; candidate generation via `scripts/generate-emulator-candidates.py` (fixed stem list, `generate-emulator-candidates.py:39-143`); promotion human-only via `scripts/promote-emulator-golden.py` (ground truth constraint #6).

---

## A. Test taxonomy

| Tier | What it is | Gating | Current Grapher inventory |
|---|---|---|---|
| T1 semantic assert-only | `.numos` scripts ending in `assert_app`/`assert_no_error` etc., NO golden | **CI-gated** (exit≠0 fails) | 6 Phase-9F scripts gated (`emulator-build.yml:495-531`): home_return, tan_exit, functions, logbase, template_insert, template_all; plus 1 input-parity script (`emulator-build.yml:407`) |
| T2 visual goldens | byte-exact PPM vs `tests/emulator/golden/*.ppm` with masks | **CI-gated** (mismatch fails; missing warns) | 6 Grapher goldens exist: grapher_{smoke,expr,graph,table,templates,trace}_smoke (+6 masks) |
| T3 warning-only candidates | stems in `CANDIDATES` without a blessed golden | candidate generation failure fails; missing golden warns | all Grapher stems through `grapher_expr_scroll_smoke` (`generate-emulator-candidates.py:82-142`) |
| T4 direct screenshot scripts | scripts with `screenshot` but NOT in `CANDIDATES` and NOT gated | **run by nothing in CI** | **all Phase-10 scripts**: implicit_{circle,sideways,ycircle}, ineq_{disk,exterior,halfplane,parabola}, aspect_{circle,line,sin,sideways_parabola,small_circle}, mixed_relations, trace_domain, implicit_tabletrace_safe, breakit_stress, curve_stress, explicit_parabola (verified: zero matches for these stems in `emulator-build.yml`) |
| T5 deterministic self-diff | run a script twice, byte-compare frames (no golden needed) | proposed gate | precedent documented in candidate comments ("Run1-vs-run2 is byte-identical", `generate-emulator-candidates.py:93-119`) but not automated |
| T6 negative controls | scripts that MUST fail an assert / MUST differ from a golden | proposed | none exist |
| T7 firmware-only manual | PSRAM OOM, watchdog, real timing | human/HIL checklist | PSRAM-fail label path (`GrapherApp.cpp:578-587`) untestable in emulator (libc malloc — memory audit §7 "Emulator/CI limitations") |

**The single largest current gap**: every implicit/inequality/equal-aspect behavior shipped in Phase 10 is T4 — exercised by no CI job. A regression that blanks the implicit renderer would pass CI today.

## B. Required test corpus (expressions × expected behavior)

Conventions: default viewport −10..10 × −7..7, immediately equal-aspected to units-per-pixel = 20/320 = 0.0625 ⇒ y ∈ [−4.47, 4.47] approx (143 px · 0.0625 / 2; `normalizeAspect`, `GrapherApp.cpp:1843-1863`). "Class" = taxonomy of geometry spec §B ([V] = current classification, [P] = after explicitX ships). Trace/Table/POI/Menu columns state the *oracle* the tests assert.

| # | Expression | Class [V] → [P] | Plotted geometry | Table | Trace | POIs | Calc menu | Failure/disabled oracle |
|---|---|---|---|---|---|---|---|---|
| 1 | `x` | explicitY (bare, no y — `GraphModel.cpp:161-169`) | line through origin slope 1 | values = x | full | Intercept(0,0)+Root(0) dedup to one entry (both at x=0: intercept stored first, root then dups out — `GrapherApp.cpp:2060-2076,2102-2104`) | all 6 active | — |
| 2 | `2x` | explicitY (implicit mul — `Tokenizer.cpp:223-245`) | line slope 2 | 2x | full | as #1 | all | — |
| 3 | `y=x` | explicitY (`isJustY(lhs)`, `GraphModel.cpp:187-192`) | line slope 1 | x | full | as #1 | all | — |
| 4 | `y=x^2` | explicitY | parabola | x² | full | Intercept(0,0); Min(0,0) via 3-pt test; **no Root today** (no sign change — geometry spec §C.12) → [P] `Touch` POI at 0 | all; Find Root → "Not found in range" [V] is the *correct current* oracle | — |
| 5 | `x=y^2` | implicit [V] → **explicitX [P]** | sideways parabola (marching squares today — `grapher_implicit_sideways_smoke` exists) | `--` [V] | none [V] → y-trace [P] | none [V] → segment intersections [P] | unreachable [V] → matrix column explicitX [P] | Integral row disabled [P] |
| 6 | `x=3` | implicit [V] → explicitX [P] | vertical line x=3 | `--` | none → y-trace [P] | — | Root correctly "Not found" (f(y)=3≠0) | — |
| 7 | `x^2+y^2=1` | implicit (`GraphModel.cpp:200-207`) | unit circle, closed, round under equal aspect (`grapher_aspect_circle_smoke`) | `--` | none [V] → contour trace, closed wrap [P] | none [V] | disabled except Tangent+Intersection [P] | disabled-state pill "n/a for this curve type" |
| 8 | `y=x^2+y^2` | implicit (rhs has y — falls through `GraphModel.cpp:187` check) | circle r=½ center (0,½) (comment `GrapherApp.cpp:1713-1715`) | `--` | contour trace [P] | — | as #7 | — |
| 9 | `xy=1` | implicit-but-dead [V]: single identifier `xy` never resolves (`Tokenizer.cpp:147-169`, `Evaluator.cpp:62-76`) ⇒ **plots NOTHING, silently, while valid=true** | empty graph [V] → **invalid slot, reason "unknown: xy"** [P] (geometry spec §C.13) | `--` [V] → excluded [P] | none | none | — | THE negative oracle: assert error state, not hyperbola. `x*y=1` is the positive twin: two branches, two trace contours [P] |
| 10 | `y>x^2` | ineqStrict (`GraphModel.cpp:142-148`) | stippled region above parabola + boundary (solid [V] → dashed [P]) (`grapher_ineq_parabola_smoke` exists) | `--` | refused (info-bar message `GrapherApp.cpp:2676`) | boundary intersections only [P] | all disabled [P] | trace-refusal text is the assert |
| 11 | `x^2+y^2<1` | ineqStrict | stippled open disk + circle boundary (`grapher_ineq_disk_smoke`) | `--` | refused | — | disabled | — |
| 12 | `x^2+y^2>1` | ineqStrict | stippled exterior + circle (`grapher_ineq_exterior_smoke`) | `--` | refused | — | disabled | — |
| 13 | `y=sin(x)` | explicitY | sine, RAD [V even in DEG mode — geometry spec §C.14] → follows system mode [P] | values | full | roots at kπ within view (≤20) | all | DEG-mode period-360 assert [P] |
| 14 | `y=cos(x)` | explicitY | cosine | values | full | roots at π/2+kπ | all | — |
| 15 | `y=tan(x)` | explicitY | branches, NO vertical smear (jump guard `GrapherApp.cpp:1651-1660`; gated by `grapher_tan_exit_smoke`) | `--` at odd·90 only in DEG [P]; near-pole huge values otherwise | off-domain pill at poles (`grapher_trace_domain_smoke` exists) | roots at kπ | all; Integral across a pole ⇒ `∫ n/a (domain gap)` [P] | — |
| 16 | `y=ln(x)` | explicitY | right half-plane only; nothing for x≤0 (`Evaluator.cpp:150-155`) | `--` for x≤0 | off-domain left of 0 | root at x=1 | all | — |
| 17 | `y=log(x)` | explicitY, log10 (`Evaluator.cpp:144-149`) | log10 curve | `--` x≤0 | as #16 | root x=1 | all | — |
| 18 | `y=log_2(x)` | explicitY via change-of-base serialization `(log(x)/log(2))` (`GrapherApp.cpp:1232-1254`; gated `grapher_logbase_smoke`) | log₂ curve through (1,0),(2,1) | values | full | root x=1 | all | assert value at x=2 equals 1 (table) |
| 19 | `y=1/x` | explicitY | two branches, no smear at 0 (div-0 error ⇒ NAN, `Evaluator.cpp:195-200`) | `--` at x=0 exactly, else values | off-domain at 0 | no roots | Integral spanning 0 ⇒ domain-gap refusal [P] | — |
| 20 | `y=sqrt(x)` | explicitY | half-parabola x≥0 (`Evaluator.cpp:138-143`) | `--` x<0 | off-domain x<0 | root/intercept at 0 | all | — |
| 21 | `y=x` + `y=x^2` (two slots) | explicitY ×2 | both curves, slot colors 0xCC0000/0x0000CC (`FUNC_COLORS`, `GrapherApp.h:83-90`) | two value columns, headers f1(x)/f2(x) (`GrapherApp.cpp:2299-2317`) | UP/DOWN switches | purple markers at (0,0),(1,1) (`GrapherApp.cpp:1901-1905`) | Find Intersection → nearest of the two [P pin] | — |
| 22 | `sin(x)` + `x^2+y^2=4` + `x^2+y^2<1` | mixed (script `grapher_mixed_relations_smoke` exists) | all three layered, slot order | col1 values, col2/3 `--` (never "nan" — script header oracle) | traces sin only | explicit-pair POIs [V]; +circle crossings [P] | matrix per traced slot | — |
| 23 | each of the 6 templates (`TEMPLATES[]`, `GrapherApp.cpp:82-90`) | explicitY | template curve; `=` serializes correctly (Phase-9F fix `GrapherApp.cpp:1129-1135`; gated `grapher_template_all_smoke`) | values | full | — | all | insertion→plot round-trip is the assert |
| 24 | invalid: `((x`, `y=+`, `x=`, `y<`, `y=x=2` | invalid [P] (today: first two invalid silently; `x=` plots x=0!, `y<` shades y<0!, `y=x=2` valid-but-empty — geometry spec §B.6) | nothing plotted | excluded | refused | none | — | [P] assert visible slot-error state; **negative control: assert `x=` does NOT draw the y-axis-overlapping line** |
| 25 | pathological long: nested fractions/powers to the 63-char serialization cap | invalid [P] reason "expression too long" (cap `GrapherApp.cpp:1271`; reliability C7.3) | nothing | excluded | refused | none | — | today: silently truncated plot — negative control must FAIL until GR-02 lands (tracked expected-fail) |
| 26 | stress: `grapher_breakit_stress.numos`, `grapher_curve_stress.numos` [V exist, unwired] | — | completes < frame budget, exit 0, no hang | — | — | — | — | wire as T1 no-hang gates (§E) |

## C. Expected oracle — how each column is asserted

- **Classification**: today only observable indirectly (trace refusal text, `--` columns). [P] ticket GR-14 adds a NATIVE_SIM-only debug hook `GrapherApp::debugSlotKind(i)` (pattern: `CalculationApp::debugLastResult`, ground truth §C "NativeHal → apps debug hooks") plus a `.numos` `assert_graph_kind <slot> <kind>` extension. Until then, classification asserts ride on behavior (trace refusal, table `--`).
- **Plotted geometry**: T2 goldens byte-compare the entire 320×143 canvas region (the graph body is deterministic; only StatusBar clock needs masking — existing masks do exactly this, e.g. `grapher_graph_smoke.mask`).
- **Table**: `lv_table` cell strings are pixel-stable (`GrapherApp.cpp:2334-2358`); goldens cover them; [P] `assert_graph_table_cell r c "text"` debug hook for semantic (font-independent) value checks.
- **Trace**: pill/info-bar text is pixel-stable montserrat_14; goldens + [P] `assert_graph_trace x y` hook reading `_traceX`/computed y.
- **POIs**: purple markers appear in goldens; [P] `assert_graph_poi_count N` hook (deterministic given async POI timer determinism, `generate-emulator-candidates.py:106-112`).
- **Menu behavior**: goldens of the open menu; disabled rows [P] assert via golden gray styling + refusal pill golden.
- **Failure/disabled**: every [P] failure UI (slot error state, truncation banner, disabled pill) gets one golden + one T1 assert.

## D. Visual coverage plan (goldens to add)

Existing blessed goldens (keep): `grapher_smoke` (initial Expressions tab), `grapher_expr_smoke` (committed `x` row), `grapher_graph_smoke` (y=x graph), `grapher_table_smoke` (y=x table), `grapher_templates_smoke` (modal), `grapher_trace_smoke` (crosshair+pill) — all in `tests/emulator/golden/`, each with a mask.

New goldens to promote (stems; scripts mostly already exist as T4):

| Golden stem | Screen | Script status |
|---|---|---|
| `grapher_implicit_circle` | x²+y²=1 zoomed 3× | script exists (`grapher_implicit_circle_smoke.numos`) — add to CANDIDATES + bless |
| `grapher_implicit_sideways` | x=y² | exists — bless |
| `grapher_ineq_disk` | x²+y²<1 stipple | exists — bless |
| `grapher_ineq_exterior` | x²+y²>1 | exists — bless |
| `grapher_ineq_halfplane` | y>x (or y>x², `grapher_ineq_parabola`) | exists — bless one of each family |
| `grapher_aspect_circle` | equal-aspect circle roundness | exists — bless |
| `grapher_mixed_graph` / `grapher_mixed_table` | 3-kind overlay + `--` table | exists (`grapher_mixed_relations_smoke` takes both screenshots) — split stems |
| `grapher_intersect_markers` | y=x & y=x² purple dots | **new script** |
| `grapher_calcmenu_explicit` | menu open over explicit trace | **new script** (down/enter path from trace) |
| `grapher_calcmenu_implicit_disabled` | menu with disabled rows [P after GR-08] | new |
| `grapher_trace_implicit` | contour-trace cursor on circle [P after GR-04] | new |
| `grapher_ineq_dashed_boundary` | y>x² dashed vs y>=x² solid [P after GR-11] | new pair |
| `grapher_slot_error` | invalid-expression error state [P after GR-10] | new |
| `grapher_expr_row` / `grapher_table_multifn` | expression list w/ 2 fns; multi-column table | scripts exist (`grapher_multifn_*`) — bless |

**Mask policy** (mirrors `tests/emulator/masks/README.md` practice): mask ONLY the StatusBar clock rect; never mask any part of the 320×143 canvas, the info bar, or the pill — those are the subject under test. A golden whose diff requires masking canvas pixels indicates nondeterminism and must be fixed, not masked. Masks are `x,y,w,h` lines consumed by `scripts/compare-ppm.py` (ground truth §D golden pipeline).

**Negative controls** (T6, new): (a) `grapher_negctl_wrong_expr` — plot y=x but compare against the y=x² golden with the standard mask; the compare step must exit non-zero, proving the comparator + mask still detect canvas changes; run as a CI step that inverts the exit code. (b) `grapher_negctl_assert` — a script asserting `assert_app Calculation` inside Grapher must exit 4; proves assert plumbing. Negative controls run in CI but assert failure-of-the-tool, not the app.

## E. CI plan

Ordered by what can gate **now** vs later (workflow: `emulator-build.yml`, Grapher steps at 407-538):

1. **Gate now, zero new code**: add the Phase-10 T4 scripts to the Phase-9F-style no-hang gate loop (`run_grapher` helper, `emulator-build.yml:501-530`): implicit_{x,2x,circle,sideways,ycircle}, ineq_{disk,exterior,halfplane,parabola}, aspect_{5}, mixed_relations, trace_domain, implicit_tabletrace_safe, explicit_parabola, breakit_stress, curve_stress. They already `assert_app` (exit-4 on failure) and are deterministic. This closes the §A gap immediately.
2. **Gate now, promotion required (human)**: add the same stems to `CANDIDATES` (`generate-emulator-candidates.py:39-143`) so compare-warns start appearing; then a human runs `scripts/promote-emulator-golden.py <stem>` after visual review (never CI — ground truth constraint #6). After blessing, mismatch = CI failure automatically (`emulator-build.yml:575-607` behavior).
3. **Gate soon, small harness work**: T5 self-diff step — run each graph-body script twice with `--deterministic`, `cmp` the PPMs; a cheap nondeterminism tripwire that needs no golden. (The determinism property is already claimed per-script in candidate comments; automating it prevents silent decay.)
4. **Requires source instrumentation (NATIVE_SIM hooks + script grammar, ticket GR-14)**: `assert_graph_kind`, `assert_graph_trace`, `assert_graph_poi_count`, `assert_graph_table_cell`. NOTE: `NativeHal.cpp` script grammar is append-only CI contract (ground truth fragile-files list) — new asserts append, never modify.
5. **Requires v2 features first**: disabled-menu goldens (GR-08/GR-10), dashed-boundary pair (GR-11), contour-trace golden (GR-04), slot-error golden (GR-02/GR-10), time-budget truncation assert (GR-13: stress script asserts the "⚠ plot truncated" info-bar state via hook, since wall-clock budgets are masked by `--deterministic` — the budget check must count *evaluations*, not ms, under NATIVE_SIM determinism so CI can exercise it deterministically).
6. **Hardware-only (manual checklist per release)**: PSRAM alloc-failure label (`GrapherApp.cpp:578-587`), watchdog behavior during 80k-eval inequality replots (reliability C8.2 — emulator `yield()` is a no-op, `GrapherApp.cpp:34-41`), real replot latency vs the 200 ms budget, LVGL 64-KB-pool pressure with the full widget tree (memory audit §5.3: Grapher is the biggest pool consumer).
7. **OS drift risks**: goldens are generated on Linux CI; Windows/macOS devs must expect byte-identical output because rendering is CPU-side into the RGB565 buffer with fixed fonts (no OS text stack) — the only historical drift source is CRT path handling on Windows (`generate-emulator-candidates.py:286-294` uses repo-relative ASCII paths). Rule: goldens are only promoted from a Linux run; local Windows diffs must be reproduced on Linux before blessing. Font rasterization is LVGL-internal (bundled `stix_math_*.c`, montserrat) — no platform variance expected; any observed variance is a bug, not maskable.

---

*End of Document 3.*
