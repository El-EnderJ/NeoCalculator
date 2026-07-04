# NumOS — Emulator App Enablement Parity Spec (P-05)

> Master document of the P-05 spec set. Companions:
> `NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (per-app detail),
> `NUMOS_EMULATOR_NATIVEHAL_APP_ROUTING_AND_INPUT_SPEC.md` (routing/input),
> `NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (test plan),
> `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md` (tickets).
>
> Spec-only session, 2026-07-04, branch `claude/emulator-app-parity-p05-spec-1lf7bu`.
> Current-state claims cite `file:line` from this working tree; design sections are
> marked **[PROPOSED]**. This spec deliberately reuses (never redefines) the
> contracts established by the determinism/fixture, persistence-sandbox,
> CI/golden-policy and host-portability specs.

---

## A. Executive summary

**What "emulator app parity" means.** An app is emulator-parity when: it compiles
into `emulator_pc` from the same sources as firmware; it is reachable through the
same launcher card semantics; its input contract (MODE exit, key handling,
modifier behavior) matches what `SystemApp` gives it on device; its lifecycle
(lazy `begin()`, deferred `end()`, StatusBar-before-screen destruction) is
exercised identically; its persistence surfaces behave per the sandbox spec's
parity register; and its behavior is pinned by deterministic scripts, semantic
asserts and (where justified) blessed goldens. Parity is graded — the P0-P7
ladder in §E — not a boolean.

**What it does not mean.** It does not mean pixel-identical to the device (the
emulator renders via SDL/libc; firmware via TFT DMA — divergences documented in
`NUMOS_ARCHITECTURE_GROUND_TRUTH.md` §E). It does not mean Giac: Giac never
builds natively (`platformio.ini:179-194`), so Giac-dependent behavior is
firmware-only by definition. It does not mean hardware truth: PSRAM budgets, DMA
constraints, deep-sleep, physical keys are out of emulator scope (context header
"How to interpret emulator vs firmware results"). And it does not mean the
emulator may grow behavior firmware lacks — emulator-only surface is limited to
the test harness itself (§I).

**Why enabling an app is more than adding a menu card.** The launcher already
shows every card in the emulator (`ui/MainMenu.cpp` is whitelisted,
`platformio.ini:263`; `APPS[]` has 20 unconditional entries, `MainMenu.cpp:88-113`).
What is missing per app is: whitelist compilation closure (each app drags
engines and cas:: files the whitelist doesn't have — e.g. Calculus needs
`SymDiff.cpp`/`SymIntegrate.cpp`/`SymToAST.cpp`, none whitelisted,
`CalculusApp.h:52-60` vs `platformio.ini:252-258`), NativeHal routing (eight
hand-maintained sites, routing spec §A.4), input dispatch, script/assert names,
lifecycle wiring into deferred teardown, and QA assets. Some apps additionally
import Arduino-only APIs (EquationsApp includes `Arduino.h`, `esp_heap_caps.h`,
FreeRTOS headers unconditionally, `EquationsApp.cpp:34,44-46` — none shimmed by
`hal/ArduinoCompat.h`), `rand()` nondeterminism (`EquationsApp.cpp:106`,
`CalculusApp.cpp:449-453`, `IntegralApp.cpp:334`), LVGL timers
(`BridgeDesignerApp.cpp:836`, `CircuitCoreApp.cpp:781`, `Fluid2DApp.cpp:257`,
`ParticleLabApp.cpp:203`, `NeuralLabApp.cpp:230`, `OpticsLabApp.cpp:253`), or
LittleFS write surfaces (`PythonApp.cpp:662`, `CircuitCoreApp.cpp:1374`,
`ParticleLabApp.cpp:416`, `NeuralLabApp.cpp:950`, `Fluid2DApp.cpp:1462`,
`NeoLanguageApp.cpp:907`).

**Why fixture hygiene and determinism are preconditions.** The determinism spec's
Hazard 6 says it directly: any app enabled into the emulator brings its LittleFS
writes into `emulator_data/` with no sandbox — and `emulator_data/vars.dat` is
today a **git-tracked file loaded at every boot** (`NativeHal.cpp:2171-2179`;
`git ls-files` confirms tracking). Every FS-writing app enabled before FIX-01/
FIX-02/FIX-03 land multiplies the dirty-tree hazard. Likewise, `rand()` sites in
the exact apps P-05 targets trip the EMUDET-03 seed fence, and the StatusBar
clock reads host wall time even under `--deterministic`
(`masks/README.md:22-36`), forcing the shared `4,6,37,13` mask on every new
StatusBar screen until EMUDET-01/SB-03 land.

---

## B. Current emulator app architecture (verified)

Summarized here; full detail with line-ranges in the routing spec §A-§C.

| Element | Emulator | Firmware |
|---|---|---|
| Mode enum | `AppMode`, 10 states (`NativeHal.cpp:195-206`) | `Mode`, 24 states (`SystemApp.h:76-103`) |
| App registry | 7 static pointers + showcase cluster (`NativeHal.cpp:249-267`) | 20-21 members (`SystemApp.h:138-160`) |
| Construction | `transitionToMenu()` post-splash; calc eager-begin, rest lazy (`NativeHal.cpp:877-918`) | `SystemApp::begin()` news all, all lazy (`SystemApp.cpp:105-127`) |
| Launch | `launchApp(cardId)` switch, ids {0,1,4,5,6,7,10,100}; default prints "no implementada" (`NativeHal.cpp:924-997`) | `launchApp(id)` 0-20 all routed (`SystemApp.cpp:744-855`) |
| Script launch | `open_app` → same `launchApp` (`NativeHal.cpp:1727-1729`) | n/a |
| Menu cards | all 20 render; 13 are dead on ENTER | all 20 launch |
| Key dispatch | `dispatchKey` per-mode switch; RELEASE forwarded to apps (`NativeHal.cpp:575-769`) | `handleKey` PRESS/REPEAT-only filter + global SHIFT/ALPHA layer (`SystemApp.cpp:459-671`) |
| MODE/Home | per-mode `returnToMenu()`; modifier reset (`NativeHal.cpp:1065-1087`) | `returnToMenu()` + deferred teardown (`SystemApp.cpp:860-883`) |
| Teardown | deferred 260 ms on lv_tick (`NativeHal.cpp:230-233, 2088-2092`) | deferred 250 ms on millis (`SystemApp.cpp:243-250`) |
| App update tick | **none** in main loop (`NativeHal.cpp:2045-2130`) | Equations + Fractal ticked (`SystemApp.cpp:265-269, 290-292`) |
| Persistence boot | `nativeFS_init()` mounts `./emulator_data/`, loads vars (`NativeHal.cpp:2171-2179`; root `hal/FileSystem.cpp:38`) | LittleFS mount + `/vars.dat` (`SystemApp.cpp:144-156`) |

The full divergence inventory (D1-D15) is in the routing spec §C.

## C. Current enabled app inventory (verified)

Status legend: **stable** = scripts + goldens + semantic asserts; **partial** =
scripts + goldens, no semantic hooks; **warning-only** = candidate exists, no
blessed golden.

| App (card id) | Source | AppMode / open_app | Input dispatch | Scripts | Goldens (masks) | Persistence | Known parity gaps | Status |
|---|---|---|---|---|---|---|---|---|
| Calculation (0) | `apps/CalculationApp.*` | `CALCULATION` / `open_app Calculation` | direct handleKey (`NativeHal.cpp:623-645`) | 19 | 2 (2) | `VariableManager` → `/vars.dat` writes on STO (`CalculationApp.cpp:895-900`), Ans update (`:553`) | teardown re-begins eagerly (D9) | **stable** (only app with result hooks, `CalculationApp.h:95-107`) |
| Grapher (1) | `apps/GrapherApp.*` + GraphModel + GraphView | `GRAPHER` / `open_app Grapher` | direct (`NativeHal.cpp:733-749`) | 36 | 6 (6) | `VariableContext` no-ops natively (`VariableContext.h:36-50`) | AC-at-tab-level exit not replicated (D7); missing from exit cleanup (D10); no debug hooks | **partial** (+5 warning-only 9E candidates) |
| Settings (10) | `apps/SettingsApp.*` | `SETTINGS` / `open_app Settings` | direct (`NativeHal.cpp:647-663`) | 1 | 1 (1) | mutates `setting_*` globals only, never persisted (`SettingsApp.cpp:213-305`) | rows not honesty-gated (ST-03); no setting asserts until GS-* | **partial** |
| Statistics (4) | `apps/StatisticsApp.*` + StatsEngine | `STATISTICS` / `open_app Statistics` | direct (`NativeHal.cpp:665-680`) | 2 | 2 (2) | none (RAM arrays, `StatisticsApp.h:93-94`) | no semantic hooks | **partial** |
| Probability (5) | `apps/ProbabilityApp.*` + ProbEngine | `PROBABILITY` / `open_app Probability` | direct | 2 | 2 (2) | none | no semantic hooks | **partial** |
| Regression (6) | `apps/RegressionApp.*` + Engine | `REGRESSION` / `open_app Regression` | direct | 2 | 2 (2) | none | no semantic hooks | **partial** |
| Sequences (7) | `apps/SequencesApp.*` | `SEQUENCES` / `open_app Sequences` | direct | 2 | 2 (2) | none | no semantic hooks | **partial** |
| MathShowcase (100, emulator-only) | inline in NativeHal + `MathRenderVisualCases` | `MATH_SHOWCASE` / `open_app MathShowcase` | inline LEFT/RIGHT/ENTER (`NativeHal.cpp:751-767`) | 2 | 1 (1) | none | not a firmware app (deliberate; reuses accepted cases) | **partial** |

Coverage counts verified against `tests/emulator/`: 71 scripts, 19 goldens,
18 masks; 25 candidate stems in `generate-emulator-candidates.py:39-143`, of
which 6 are warning-only (no golden): `menu_focus_grapher_smoke` and the five
9E grapher stems.

## D. Current firmware-only / not-whitelisted app inventory (verified)

Full field-by-field detail is in the per-app matrix (Document 2). Summary:

| App (card id) | Source | Emulator on select | Engines / heavy deps | Timers | Storage | rand() | Candidate status |
|---|---|---|---|---|---|---|---|
| Equations (2) | `apps/EquationsApp.*` (284/3350) | dead card (console line, `NativeHal.cpp:993-995`) | 13 cas:: headers (`EquationsApp.h:37-52`); **Arduino.h/esp_heap_caps/FreeRTOS unconditional** (`EquationsApp.cpp:34,44-46`) | none, but `update()`-driven staged pipeline + inline `lv_timer_handler()` (`EquationsApp.cpp:975,1059-1631`) | none | yes (`:106`) | P-05 flagship, gated on shims + closure |
| Calculus (3) | `apps/CalculusApp.*` (187/1130) | dead card | cas:: SymDiff/SymIntegrate (`CalculusApp.h:52-60`), MathCanvas | none; inline `lv_timer_handler()` (`CalculusApp.cpp:464`) | none | yes (`:449-453`) | P-05, clean of Arduino APIs |
| Integral | `apps/IntegralApp.*` (159/879) | not a card — **dead in firmware too** (no SystemApp member/dispatch; ground truth §F) | cas:: SymIntegrate | none | none | yes (`:334`) | do not enable (§H) |
| Matrices (9) | `apps/MatricesApp.*` + MatrixEngine | dead card | none — pure LVGL + `<cmath>` (`MatricesApp.h:31-35`) | none (`load()` anim only, `MatricesApp.cpp:156`) | none | no | **cleanest P-05 candidate** |
| Python (8) | `apps/PythonApp.*` + PythonEngine | dead card | custom interpreter (not MicroPython) | — | LittleFS `/py/*.py` incl. seed file (`PythonApp.cpp:340,657-698`) | no | defer (FS writes + text-heavy UI) |
| PeriodicTable (11) | `apps/PeriodicTableApp.*` + ChemCAS + constexpr DBs | dead card | none — LVGL draw-layer + exact rational ChemCAS | none | none | no | **clean P-05 candidate** |
| Bridge (12) | `apps/BridgeDesignerApp.*` | dead card | own Verlet/FEA, fixed DT=1/60 (`BridgeDesignerApp.h:98`) | 16 ms physics timer (`BridgeDesignerApp.cpp:836`) | none (grep-clean) | no | defer (timer/sim wave) |
| Circuit (13) | `apps/CircuitCoreApp.*` + MnaMatrix + LuaVM stub | dead card | MNA solver; Lua stub never executes (`LuaVM.cpp:44-46,77-80`) | 16 ms sim timer (`CircuitCoreApp.cpp:781`) + 30 s lv_tick idle autosave (`:440-441`) | LittleFS `/circuits/*` + autosave — **ARDUINO-gated, native no-op today** (`CircuitCoreApp.cpp:1363,1707-1708`); SystemApp-side exit hook (`SystemApp.cpp:595-606`) | no | defer |
| Fluid2D (14) | `apps/Fluid2DApp.*` | dead card | Stable-Fluids grids ~159 KB (`Fluid2DApp.cpp:128-155`) | 33 ms sim timer, always-on from createUI (`Fluid2DApp.cpp:257`) | LittleFS `/fluid/*` — **ARDUINO-gated, native no-op** (`Fluid2DApp.cpp:1453,1484,1516`) | no | defer |
| ParticleLab (15) | `apps/ParticleLabApp.*` + Engine | dead card | 160×120 CA grid 76.8 KB + 153.6 KB render buf (`ParticleEngine.cpp:118`, `ParticleLabApp.cpp:106-108`) | 33 ms sim timer (`ParticleLabApp.cpp:203`) | LittleFS `/save.pt` — **ARDUINO-gated, native no-op** (`ParticleLabApp.cpp:409,427`) | **fixed-seed static xorshift (12345), never re-seeded across relaunch** (`ParticleEngine.cpp:84-95,147`) | defer |
| NeuralLab (16) | `apps/NeuralLabApp.*` + Engine | dead card | MLP + backprop | 33 ms train timer, 50 epochs/frame (`NeuralLabApp.cpp:230,1012`) | LittleFS `/neural.nn` — **NOT gated: writes natively into emulator_data/** (`NeuralLabApp.cpp:950,965-967`) | **yes, unseeded** (`NeuralLabApp.cpp:372-380`, `NeuralEngine.cpp:140-141`) | defer |
| OpticsLab (17) | `apps/OpticsLabApp.*` + Engine | dead card | 2D ray tracing; own software rasterizer (`OpticsRenderer.h:37-170`) | 33 ms timer, re-render only, no physics step (`OpticsLabApp.cpp:253,587`) | none (grep-clean) | no | defer (easiest sim; first of the timer wave) |
| NeoLang (18) | `apps/NeoLanguageApp.*` + ~11k-line Neo* stack | dead card | interpreter + cas arena soft-reset | none (`time_it` builtin reads wall clock, `NeoInterpreter.cpp:767-800`) | LittleFS `/neolang.nl` — **writes natively** (`NeoLanguageApp.cpp:897-944`); **NeoIO `file()` uses raw cwd-relative `std::fopen`, bypassing the FS root** (`NeoIO.cpp:62-119`) | no | defer |
| Fractals (19) | `apps/FractalApp.*` + math/FractalEngine | dead card | escape-time math; FreeRTOS render task is ARDUINO-only, native renders synchronously (`FractalApp.cpp:490-535, 739-762`) | no lv_timer — `update()` state machine (`SystemApp.cpp:290-292`); AC-exit + `consumeExitRequest` (`SystemApp.cpp:649-658`) | none (grep-clean) | no | defer (needs ROUTE-04) |
| Math Visual (20, validate env only) | `apps/MathRenderVisualTestApp.*` | n/a (card absent without `NUMOS_MATH_VISUAL_VERIFY`); emulator has MathShowcase instead | vpam fixtures | — | none | no | keep as-is (§H) |
| Tutor (no card) | `apps/TutorApp.*` | dead in firmware too | RuleEngine tutor CAS closure | none | none | no | do not enable (§H) |
| StixGlyphGallery (no card) | `ui/StixGlyphGallery.*` (two free functions, not an app class; `StixGlyphGallery.h:24,27`) | diag-only, `NUMOS_STIX_DIAGNOSTICS` (`main.cpp:49-52,187-194` per ground truth §F) | stix fonts; **unconditional `#include <Arduino.h>`** (`StixGlyphGallery.cpp:18`); gallery is a blocking `millis()` busy-loop modal (`:165-169`) | — | none | no | P-06 diagnostic candidate (needs include guard + non-blocking wrapper) |

## E. Parity definition — the P0-P7 ladder **[PROPOSED]**

| Level | Criteria (all required) | Allowed gaps | Disallowed gaps | Required artifacts |
|---|---|---|---|---|
| **P0 — card visible** | card renders in launcher grid (true for all 20 today) | everything else | crash on ENTER | none |
| **P1 — opens/exits clean** | compiled into whitelist; NativeHal route per checklist (routing spec §D.1); ENTER opens; MODE returns; process exits 0 after open→home→exit | no scripts, no asserts, no goldens | any exit≠0; missing teardown case; missing exit-cleanup entry | manual run log in PR |
| **P2 — deterministic smoke script** | `<app>_smoke.numos`: `open_app` → wait → `assert_app` → screenshot; byte-identical across two `--deterministic` runs (modulo clock mask) | no semantic asserts; screenshot warning-only | wall-time-dependent pixels outside clock rect; rand-driven visible text (must be neutralized or asserted-around first) | script + candidate stem registered |
| **P3 — basic semantic asserts** | at least one NATIVE_SIM debug hook or existing generic assert proving app state (result, mode, selection, table value) | golden still warning-only | asserting via pixels; hooks with mutation surface | hook + negative control script |
| **P4 — golden candidate** | candidate stem generated in CI, classified *pending* per CI-05 (`PENDING.md` row with owner+reason) | not blessed | unclassified stem | candidate in `numos-emulator-candidates` artifact |
| **P5 — blessed golden** | human-promoted golden (+clock mask iff StatusBar screen); CI gates byte-exact | — | body-region masks (>1% frame or y≥24, CI-05 lint); promotion by automation | golden + mask + promotion commit |
| **P6 — firmware-parity checklist passed** | §I checklist reviewed: exit-key table matches SystemApp §B.4; modifier semantics; persistence rows registered in sandbox §A.4 parity table; update-tick parity; StatusBar contract (repaint-on-key, title ≤120 px) | documented, justified divergences listed in routing spec §C table | undocumented divergence | checklist in PR description citing lines |
| **P7 — stress/teardown loop passed** | LIFE-01 enter/exit ×5 loop green; timer-active exit green (if app owns timers); modal-exit green (if app has modals) | leak detection warning-only until `assert_lvgl_object_count` exists | hang, exit≠0, growing object count once gate is on | loop scripts in CI |

Rules: levels are strictly ordered per app; CI gating begins at P2 (script suite)
and P5 (pixels). An app may ship in a release wave at P3+ with goldens pending;
it may not be called "emulator-enabled" below P2.

## F. App enablement prerequisites (global) **[PROPOSED, reusing existing tickets]**

1. **Fixture sandbox** — FIX-01 (settable FS root, `--fs-sandbox*`/`--fixture-dir`
   flags) and FIX-03 (untrack `emulator_data/vars.dat`, gitignore) from the
   persistence sandbox spec §G. Hard prerequisite for any FS-writing app; strongly
   recommended before *any* new app (Hazard 6).
2. **Dirty-tree guard** — CI-01 (`git diff --exit-code emulator_data/` post-suite).
3. **Deterministic clock** — EMUDET-01/SB-03 (virtual `time()`); until it lands,
   every new StatusBar screen inherits the `4,6,37,13` clock mask and no other.
4. **Seed fence** — EMUDET-03 (`srand(1)` + time/rand grep over the whitelist set):
   must land with or before the first `rand()`-carrying app (Equations/Calculus).
5. **Assert-failure screenshot capture** — CI-04/QA-05 (`<stem>.assertfail.ppm`),
   one shared implementation.
6. **App teardown loop support** — LIFE-01 harness (QA plan §E).
7. **NativeHal routing pattern** — ROUTE-01 checklist (routing spec §D.1) is the
   single normative recipe; ROUTE-05 mechanizes drift detection.
8. **App state reset policy** — process-per-script + sandboxed FS root (routing
   spec §D.5); in-script resets are app actions, never harness magic.
9. **Script naming convention** — `<app>_smoke`, `<app>_<feature>_smoke`,
   `<app>_semantic_<case>`, `life_<app>_*`, `route_*` negative controls; stems
   shared verbatim across script/golden/mask/candidate (existing convention,
   `golden/README.md:15-24`).
10. **Golden/mask policy** — unchanged from CI/golden policy spec: candidates →
    pending → human-blessed; clock-rect mask only; promotion via
    `promote-emulator-golden.py` never in CI.

## G. App categories **[PROPOSED]**

| Category | Apps | Shared risk profile |
|---|---|---|
| Low-risk UI-only | Matrices, PeriodicTable, StixGlyphGallery, (Settings/Stats/Prob/Seq/Reg already in) | pure LVGL+libc; deterministic; no FS |
| Math-renderer apps | Calculation (in), Calculus, Equations, Integral(dead), Tutor(dead), MathShowcase (in) | MathCanvas geometry ↔ goldens; cas:: closure; rand() captions |
| Storage/persistence apps | Python, NeoLang, Circuit, Fluid2D, ParticleLab, NeuralLab | LittleFS writes into fixture root; sandbox-gated |
| Timer/animation apps | Bridge (16 ms), Circuit (16 ms), Optics/Particle/Neural/Fluid (33 ms), Grapher (in; 10/30 ms) | lv_timer OK under deterministic tick, but sims run unbounded frames → screenshot timing and long-run pixel churn |
| Hardware-dependent | none strictly (keyboard scan disabled on device too); power/battery surfaces excluded via SB-02 | — |
| Large-memory/simulation | Fluid2D (~200 KB grids), ParticleLab (160×120 CA), Fractal (~280 KB buffers), Python (1 MB heap) | native heap hides PSRAM budget truth (MEM-5); enable late, never as memory evidence |
| CAS/Giac-dependent | Equations, Calculus (in-tree cas:: — allowed); anything Giac — impossible natively (`platformio.ini:192-194`) | closure size; arena lifetime (MEM-2) |
| Educational/demo | MathShowcase (in), Math Visual (validate-only), Tutor (dead) | fixture-style; low risk, low urgency |

## H. Recommended P-05 scope **[PROPOSED]**

**Include in P-05 (in order):**

1. **Matrices** — reason: zero new engine risk (pure LVGL/`<cmath>`,
   `MatricesApp.h:31-35`; static 5×5 doubles, `MatrixEngine.h:41`); identical
   safety profile to already-enabled Stats/Prob. Risk: low. Prereqs: ROUTE-01
   checklist only (no FS, no rand, no timers). Minimum acceptance: P3 (smoke +
   an `assert`-able cell/result hook) targeting P5.
2. **PeriodicTable** — reason: constexpr data + LVGL draw-layer only
   (`ChemDatabase.h:51-186`; no PROGMEM reads); exercises `LV_EVENT_DRAW_MAIN`
   custom drawing, a renderer path the emulator has never covered. Risk: low.
   Prereqs: whitelist `ChemCAS.cpp`. Minimum acceptance: P3 (balancer result
   hook) targeting P5 for the grid screen.
3. **Calculus** — reason: first CAS-UI app; Arduino-clean
   (no `Arduino.h`/heap_caps, agent-verified over full source); brings
   SymDiff/SymIntegrate under emulator coverage for the first time (unblocks the
   CAS corpus rows tagged "once Calculus is emulator-enabled per P-05",
   `NUMOS_CAS_IMPLEMENTATION_TICKETS.md:94`). Risk: medium — cas:: closure size,
   `rand()` caption (`CalculusApp.cpp:449-453`), synchronous
   `lv_timer_handler()` inside `showComputing()` (`CalculusApp.cpp:464`).
   Prereqs: EMUDET-03 seed fence; whitelist closure (SymDiff, SymIntegrate,
   SymToAST + link-verified transitive set). Minimum acceptance: P3 with a
   result-string hook; goldens only for the static input screen initially
   (spinner/steps screens stay assert-only).
4. **Equations** — reason: highest test value (untested 3350-line CAS UI;
   EMU-5/PRD-3, MEM-6) but the most work: requires guarding
   `Arduino.h`/`esp_heap_caps.h`/FreeRTOS includes and the `heap_caps_*` step
   budget behind `#ifdef ARDUINO` with a native fallback
   (`EquationsApp.cpp:34,44-46,487-518`), the largest cas:: closure
   (SingleSolver/SystemSolver/OmniSolver/tutor stack + transitive), the ROUTE-04
   update tick (firmware ticks it, `SystemApp.cpp:265-269`), and the seed fence.
   Risk: medium-high. Minimum acceptance: P3 (solution hook + `steps.clear()`
   leak canary per MEM-6) — golden only for the equation-list screen.

**Defer beyond P-05 (P-06 or later):**

- **StixGlyphGallery** (P-06): diagnostic screen, near-zero code risk, but two
  concrete blockers: unconditional `#include <Arduino.h>`
  (`StixGlyphGallery.cpp:18`, no native Arduino.h exists) and a blocking
  `millis()` busy-loop modal (`:165-169`) incompatible with the frame loop.
  Enable as a MathShowcase-style `open_app StixGallery` wrapper (no busy-wait)
  if REN-2 tofu coverage is wanted; the alignment-diagnostics function
  (`:57-112`) is separately CI-able as a text assert.
- **Python** (defer, decision required): every session writes `/py/hello.py`
  seed file (`PythonApp.cpp:657-698`) — sandbox-first; and its product identity
  (custom interpreter vs MicroPython) is an open product decision
  (ground truth §F) that should be settled before investing QA assets.
- **Optics, Bridge, Circuit, Fluid2D, ParticleLab, NeuralLab, Fractal**
  (P-06+/split): all but Fractal are lv_timer-driven sims. Storage nuance from
  the audit: Circuit/Fluid/Particle FS code is **ARDUINO-gated** (native no-op
  today — enabling them without un-gating creates a persistence parity *lie*,
  with un-gating requiring `File::printf` shims + sandbox), while
  **NeuralLab's `/neural.nn` writes natively already**
  (`NeuralLabApp.cpp:950-967`) — a hard FIX-01 dependency. NeuralLab is also
  rand()-driven at its core (weight init, `NeuralEngine.cpp:140-141`) so it
  can never hold pixel goldens without a seeded-RNG hook; ParticleLab's engine
  RNG is a fixed-seed static xorshift whose state survives relaunch
  (`ParticleEngine.cpp:84-95,147`) — needs a NATIVE_SIM `resetRng()` hook for
  per-scenario determinism. Fractal needs the ROUTE-04 update tick and the
  AC-exit/`consumeExitRequest` special path (`SystemApp.cpp:649-658`); its
  FreeRTOS render task is already ARDUINO-gated with a synchronous native
  fallback (`FractalApp.cpp:490-535,739-762`). Recommended split: Optics
  first (timer is render-only, static scene — best sim golden candidate),
  Bridge second, then Circuit/Fluid/Particle after FIX-02 with an explicit
  un-gate-or-register-divergence decision per app, NeuralLab last, Fractal
  alongside ROUTE-04.
- **NeoLang** (defer): FS writes + arena lifecycle + a text-editor input surface
  the script grammar can't drive well yet.
- **Integral, Tutor** (do not enable): both are dead on firmware — no SystemApp
  member, no dispatch case, no launcher card (ground truth §F). Enabling them
  would create emulator-only reachability, inverting parity. Resolution belongs
  to the dead-code sweep (P-07); if IntegralApp is deleted after confirming
  CalculusApp parity, APPPAR-03 closes as won't-enable.
- **Math Visual (id 20)**: firmware validate-env only; emulator already has the
  equivalent MathShowcase (id 100). Keep both as-is; do not add a second route.

## I. Firmware parity philosophy **[PROPOSED]**

**Must match firmware:** app source files byte-identical (enablement must not
fork app code except adding `#ifdef ARDUINO` guards around genuinely
firmware-only APIs, each with a native fallback that preserves behavior
semantics); launcher card ids and names; exit-key semantics per the SystemApp
table (routing spec §B.4); lazy-begin/deferred-end lifecycle; StatusBar
destruction order; persistence file formats and paths (sandbox spec §F.1 parity
contract); modifier FSM behavior; update-tick presence for ticked apps.

**May legitimately differ:** allocator (libc vs heap_caps — documented MEM-5
limitation); absence of Giac; SDL vs TFT presentation; wall-clock vs virtual
tick under `--deterministic`; heap-budget *values* in native fallbacks (e.g. a
native `canAllocStep()` may return "always fits" — but then the step cap
`STEPS_CHILD_HARD_CAP` must still be enforced so UI behavior stays comparable);
the test harness surface itself (script runner, debug hooks, screenshots).

**Documenting emulator-only shims:** every `#ifdef ARDUINO`/`#ifdef NATIVE_SIM`
introduced by enablement must (a) cite the firmware line it shims in a comment,
(b) appear in the routing spec §C divergence table in the enabling PR, and
(c) if it affects persistence, add/update a row in the sandbox spec §A.4 parity
register. Debug hooks are read-only const accessors under `#ifdef NATIVE_SIM`
only (the `CalculationApp.h:95-107` pattern) — no mutation surface, ever.

**Preventing an emulator fork of the product:** three hard rules. (1) No
emulator-only features visible in app UI — emulator-only surface lives in
NativeHal and `debug*` accessors. (2) The whitelist adds files, never patched
copies. (3) Any behavioral divergence found during enablement is either fixed,
or recorded as a numbered divergence with a ticket — silent divergence is the
failure mode that turns the emulator into a different product (risk EMU-1).

## J. Open decisions

1. **Dead-card presentation** (special Q12): recommend **visible-but-refusing
   with dimmed style** per SB-06/PD-B3 (already specced in the StatusBar spec) —
   hiding cards would fork launcher goldens and navigation geometry between
   targets (the 3-column focus model indexes by card id, `MainMenu.cpp:159-197`).
   Needs human ratification because it re-blesses `launcher_smoke` (and the
   future `launcher_dimmed_cards`) once dimming lands, and each P-05 enablement
   un-dims a card → coordinated re-bless waves.
2. **EquationsApp native heap-budget fallback semantics**: "always fits" vs a
   fixed synthetic budget that keeps `STEPS_CHILD_HARD_CAP` behavior testable.
   Recommend fixed synthetic budget; Human/Opus review at APPPAR-01.
3. **`rand()` captions**: EMUDET-03 `srand(1)` pins them, but captions still vary
   with call order. Options: mask caption region (violates mask policy), assert
   around them (fragile), or add a deterministic-mode caption index (tiny shim).
   Recommend: keep captions out of golden frames (screenshot before/after
   compute, never during spinner) — no code change needed. Ratify at APPQA-02.
4. **IntegralApp fate** (delete vs wire): owned by P-07 dead-code sweep;
   APPPAR-03 blocked on it.
5. **Python product identity** (toy interpreter vs MicroPython): Human decision
   before APPPAR-06 invests QA assets.
6. **Whether P-05 waits for FIX-02 (sandbox default) or only FIX-01/FIX-03**:
   recommend FIX-01+FIX-03 suffice for the P-05 four (none write FS), with
   FIX-02 required before the storage wave. Ratify in roadmap review.

## K. Special questions — explicit answers

1. **Which apps must wait for fixture sandboxing?** Hard: NeuralLab and NeoLang
   (their saves run natively today and would write `emulator_data/` —
   `NeuralLabApp.cpp:950-967`, `NeoLanguageApp.cpp:897-944`; NeoLang's NeoIO
   additionally bypasses the FS root via raw `std::fopen`, `NeoIO.cpp:62-119`),
   plus Circuit/Fluid2D/ParticleLab/Python **if** their ARDUINO-gated FS is
   un-gated at enablement. The P-05 four (Matrices, Periodic, Calculus,
   Equations) write no files and may precede FIX-02; FIX-01/FIX-03 are still
   recommended first because the committed `emulator_data/vars.dat` fixture is
   loaded at every boot (`NativeHal.cpp:2171-2179`).
2. **Which apps can be enabled before the deterministic clock (EMUDET-01)?**
   All P-05 four and every app whose only wall-clock pixel is the StatusBar
   `HH:MM` — they inherit the shared `4,6,37,13` mask. No audited app renders
   wall time anywhere else (NeoLang's `time_it` prints elapsed time to its
   console, `NeoInterpreter.cpp:767-800` — assert-only screens for it).
3. **Which apps import new filesystem writes?** Firmware surfaces: Python
   `/py/*`, Circuit `/circuits/*`, Fluid2D `/fluid/*`, ParticleLab `/save.pt`,
   NeuralLab `/neural.nn`, NeoLang `/neolang.nl` (+NeoIO arbitrary paths).
   Effective-on-native today: only NeuralLab, NeoLang, NeoIO (the rest are
   ARDUINO-gated no-ops — matrix doc cross-app table). The P-05 four import
   none.
4. **Which apps use timers or async LVGL callbacks?** lv_timer owners: Grapher
   (30 ms template + 10 ms POI), Bridge 16 ms, Circuit 16 ms (+30 s lv_tick
   idle autosave), Fluid 33 ms (always-on), ParticleLab 33 ms, NeuralLab
   33 ms, Optics 33 ms (render-only); MathCanvas cursor blink 500 ms in every
   canvas app (`MathRenderer.cpp:660-697`). Non-timer async: Equations staged
   `update()` pipeline; Fractal `update()` state machine + firmware-only
   FreeRTOS render task.
5. **Which apps are likely to produce nondeterministic pixels?** Every
   StatusBar screen (clock, until EMUDET-01); spinner/caption frames of
   Equations/Calculus (unseeded `rand()`); NeuralLab everywhere (unseeded
   weight init + point jitter); ParticleLab after any simulation (RNG stream
   position depends on process history); unframed mid-simulation captures of
   any 16/33 ms sim. Deterministic by construction: Matrices, Periodic,
   Optics static scenes, Fractal completed frames, Bridge/Circuit/Fluid fixed
   tick sequences.
6. **Which apps require new fonts/glyphs?** None. All audited apps use
   `stix_math_18` + `lv_font_montserrat_14` (both already linked) or LVGL
   built-ins (`lv_font_unscii_8`, Circuit toolbar); Optics rasterizes its own
   text. The unused `src/fonts/lv_font_montserrat_math_*.c` remain unneeded.
7. **Which apps require MathRenderer v2 before useful goldens?** None as a
   hard block, but Calculus/Equations **result and steps screens** should stay
   assert-only until the MR freeze/v2 wave settles — every CAS-app golden
   added now joins the "must stay byte-identical during freeze" set and
   multiplies re-bless cost. Input/list screens are safe goldens now.
8. **Which apps can have smoke tests without semantic hooks?** Matrices
   (manager screen), PeriodicTable (grid), StixGallery, Optics (static
   scene), Equations list, Calculus input — any static screen where
   `assert_app` + byte-exact pixels already pin behavior.
9. **Which apps need semantic hooks before golden promotion?** Any whose
   golden frame contains computed content: Calculus (result), Equations
   (solutions/steps), Matrices ops results, Periodic balancer output,
   Fractal (render-complete flag), NeuralLab (everything). Hook-before-golden
   rule in QA plan §D.
10. **Which apps are visible in the launcher but dead in the emulator?**
    Cards 2, 3, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19 — thirteen cards
    render (`MainMenu.cpp:88-113`) and fall to the default refusal branch
    (`NativeHal.cpp:993-995`).
11. **How should NativeHal report selecting a dead app?** Keep the refusal in
    MENU mode but emit a structured `[APP] launch-refused id=<n> name=<name>`
    line and cover it with the `route_dead_card_control.numos` negative
    control; `open_app` for unrouted apps stays a load-time exit-2 error
    (routing spec §F.3, ROUTE-02).
12. **Hidden, disabled, or visible-but-refusing?** Visible-but-refusing with
    dimmed styling (SB-06/PD-B3). Hiding would fork launcher geometry and
    focus arithmetic between targets (`MainMenu.cpp:159-197` indexes by card
    id); silent-refusing without dimming is the current honesty gap.
13. **Interaction with the 20-card MainMenu?** Card ids, names, order and
    grid geometry never change per-target; enablement only flips routing.
    Consequences: `assert_menu_focus` tokens stay valid; launcher goldens
    re-bless only when the dimmed set changes (SB-06 waves); the emulator
    keeps rendering all cards so menu-navigation parity scripts remain
    target-identical.
14. **How should app scripts reset state between runs?** Fresh process per
    script (already how CI and the candidate generator run) + sandboxed FS
    root per FIX-01/FIX-02; the committed `vars.dat` is treated as a declared
    read-only fixture until FIX-03 untracks it. Within a script, state reset
    is an app action (AC etc.), never harness magic — matching firmware,
    where app objects survive menu round-trips (routing spec §D.5).
15. **How should firmware-only hardware features be represented?** Honestly,
    via capability bits and disabled-style UI (GSTATE §F.4 / SET §H.4), never
    fake data: battery constants are being removed (SB-02), Giac-backed rows
    show unavailable, heap-budget fallbacks are synthetic but must preserve
    the *behavioral* caps (Equations step limits), and any such shim is
    registered as a numbered divergence (§I).
