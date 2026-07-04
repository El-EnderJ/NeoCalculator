# NumOS — Emulator Per-App Enablement Matrix (P-05)

> Part of the P-05 spec set (see `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md`
> for the P0-P7 ladder and wave scope). One section per app in the repository.
> Every current-state claim cites `file:line` from this working tree; "Proposed"
> fields are design. "Not found" means a full-source audit found no occurrence.
>
> Shared reference facts (cited once):
> - Emulator whitelist: `platformio.ini:195-266`. Whitelisted cas:: files:
>   `ASTFlattener, SymSimplify, SymExprToAST, SymExpr, ConsTable, CASStepLogger,
>   SymPoly` (`platformio.ini:252-258`).
> - NativeHal routing sites for any new app: routing spec §A.4/§D.1 (AppMode
>   `NativeHal.cpp:195-206`, launchApp `:924-997`, teardown `:1007-1046`,
>   dispatchKey `:575-769`, name maps `:1402-1438`, activeAppName `:1666-1678`,
>   exit cleanup `:2142-2152`).
> - `hal/ArduinoCompat.h` shims: String, GPIO stubs, Serial, millis/delay/micros,
>   psramFound/ps_malloc, ESP.getPsram*, PROGMEM (`ArduinoCompat.h:41-213`).
>   **Not shimmed:** `Arduino.h` itself, `esp_heap_caps.h`/`heap_caps_*`,
>   FreeRTOS, `random()/randomSeed()/esp_random()`, `Preferences`.
> - `hal/FileSystem.h` native LittleFS: `begin/exists/open/remove` +
>   `File::write/read(buf,len)/size/position/available/seek/close` rooted at
>   `./emulator_data/` (`FileSystem.cpp:38-88`). **Not implemented:**
>   `openNextFile/isDirectory/mkdir/rename/print/printf/readBytes/name`.
> - No `srand`/`randomSeed`/`esp_random` anywhere in `src/` (repo grep).
> - Risk levels: low / medium / high / block. Phases: P-05 / P-06 / future /
>   do-not-enable.

---

## 1. Calculation — ENABLED (reference row)

- **Source:** `src/apps/CalculationApp.h/.cpp` (184/1126).
- **Reachability:** firmware card 0 (`SystemApp.cpp:744-748`); emulator
  `AppMode::CALCULATION`, card 0 + `open_app Calculation`
  (`NativeHal.cpp:932-937, 1428`).
- **NativeHal support:** full, incl. eager re-begin after teardown
  (`NativeHal.cpp:1010-1015`).
- **Input:** direct handleKey; MODE→menu (`NativeHal.cpp:623-645`).
- **Asserts:** `debugHasResult/debugLastResult` (`CalculationApp.h:95-107`)
  feeding `assert_result*/assert_no_error/assert_error`.
- **Persistence:** `VariableManager` → `/vars.dat` on STO
  (`CalculationApp.cpp:895-900`), Ans on ENTER (`:553`).
- **Timers:** cursor blink inside MathCanvas (500 ms lv_timer,
  `MathRenderer.cpp:660-697`) — frame-deterministic under `--deterministic`.
- **Coverage:** 19 scripts, 2 goldens+masks, semantic suite 8C.
- **Status:** the parity model every new app copies. Risk n/a. Phase: done.

## 2. Grapher — ENABLED

- **Source:** `src/apps/GrapherApp.h/.cpp` (343/3283) + `GraphModel.*`,
  `ui/GraphView.*`.
- **Reachability:** firmware card 1 (`SystemApp.cpp:749-753`); emulator card 1 +
  `open_app Grapher` (`NativeHal.cpp:939-945, 1433`), plus `g`/GRAPH shortcut
  from menu (`NativeHal.cpp:612-615`).
- **Timers:** `_tplLoadTimer` 30 ms (`GrapherApp.cpp:1508`), `_poiAsyncTimer`
  10 ms (`GrapherApp.cpp:2994`), both deleted in `end()`
  (`GrapherApp.cpp:169-199`).
- **Persistence:** `GraphModel`'s `VariableContext` — NVS on device, **inline
  no-op stubs on native** (`VariableContext.h:36-50`): emulator graph variables
  are session-only. Registered divergence D13.
- **Known gaps:** no debug hooks (GR-14 owns); AC-at-tab-level exit path not
  replicated (D7, `SystemApp.cpp:1024-1030` vs `NativeHal.cpp:733-749`);
  missing from exit cleanup (D10 → ROUTE-03).
- **Coverage:** 36 scripts, 6 goldens, 5 warning-only candidates.
- **Status:** partial. Phase: done (backfill via GR-*).

## 3. Settings / Statistics / Probability / Sequences / Regression — ENABLED

Uniform profile (full field detail in the enabled-app audit rows of the parity
spec §C): LVGL-only, no persistence (Settings mutates the three `setting_*`
globals only, `SettingsApp.cpp:213-305`; data apps hold RAM arrays,
e.g. `StatisticsApp.h:93-94`, `RegressionApp.h:94-95`), no timers, no rand, no
debug hooks; fonts stix_math_18 + montserrat_14; scripts+goldens per app; all
routed in NativeHal (cards 10/4/5/6/7). Status: partial (hooks pending
per-app QA backfill). Phase: done.

## 4. MathShowcase (emulator-only, id 100) — ENABLED

Inline NativeHal implementation over `MathRenderVisualCases`
(`NativeHal.cpp:1090-1222`), cursor OFF for deterministic capture
(`NativeHal.cpp:1139-1144`). 2 scripts, 1 golden. Not a firmware app —
documented emulator-only harness surface (parity spec §I). Phase: done.

---

## 5. Equations (card 2) — P-05, flagship

- **Source files:** `src/apps/EquationsApp.h/.cpp` (284/3350).
- **Firmware reachability:** card 2 → `Mode::APP_EQUATIONS`
  (`SystemApp.cpp:754-758, 889`); keys forwarded only `if (isActive())`
  (`SystemApp.cpp:513-519`); cooperative `update()` tick
  (`SystemApp.cpp:265-269`).
- **Emulator reachability:** none — dead card (`NativeHal.cpp:993-995`).
- **NativeHal needs:** `AppMode::EQUATIONS`; all 8 routing sites; **plus the
  ROUTE-04 update tick** (its staged solve pipeline advances in `update()`,
  `EquationsApp.cpp:1059-1631`).
- **Construction/destruction:** lazy begin (`EquationsApp.cpp:362-367`); `end()`
  destroys canvases → StatusBar (`:416`) → step renderers/pipeline (`:418-419`)
  → screen (`:422`) → clears vectors + `_arena.reset()` (`:454-465`). Correct
  StatusBar-before-screen order.
- **Input routing:** PRESS/REPEAT state dispatch (`:1637-1648`); SHIFT/ALPHA via
  KeyboardManager (`:1789-1791,1833,1923`); structured VPAM entry via
  CursorController (`:1804-1885`). No textarea.
- **Script commands:** standard set; new hooks below. SOLVE/EXE tokens exist
  already (`NativeHal.cpp:541-542` for exe; `solve` token missing — add only if
  its handler consumes SOLVE; audit found dispatch by state, verify at
  implementation).
- **Assert hooks (proposed):** `debugSolutionString(i)`, `debugSolutionCount()`,
  `debugStepsCount()`, `debugPipelineIdle()` (deterministic wait target),
  `debugStepsRendererCount()` (MEM-6 canary).
- **Persistence/filesystem:** none (grep-clean of LittleFS/Preferences/
  VariableManager).
- **Timers/async:** no lv_timer; `lv_spinner` (`:736-737`); **synchronous
  `lv_timer_handler()` inside `showSolving()`** (`:975`) — must be verified
  non-reentrant in the emulator loop; staged pipeline with per-tick budgets
  (`:1392,1548`).
- **MathRenderer usage:** heavy — preview/template/edit/result canvases + step
  renderers (`EquationsApp.h:97-154`).
- **CAS usage:** largest closure in the repo: SingleSolver, SystemSolver,
  OmniSolver, SymToAST, RuleEngine, AlgebraicRules, CasToVpam, CasMemory,
  SystemHeuristics, SystemTutor, PersistentAST, TutorTemplates,
  PedagogicalLogger + transitive (SymPolyMulti, AstDiff, CASNumber,
  HybridNewton, RuleBasedTutor…) — link-verify (`EquationsApp.h:37-52`,
  `EquationsApp.cpp:33-46`, closure per audit). No Giac.
- **Graph/evaluator usage:** none. **Fonts:** stix_math_18 only.
- **Memory/PSRAM risk:** `heap_caps_get_largest_free_block/get_free_size` with
  `MALLOC_CAP_*` in `canAllocStep()/logStepBudget()` (`:487-518`) — **Arduino-
  only APIs, unshimmed**; growing `_stepRenderers` vector cleared in `end()`
  (`:418`) — MEM-6.
- **LVGL lifecycle risk:** medium — many canvases + modal steps view; pipeline
  must be idle before teardown (tick must skip modes pending teardown).
- **Determinism risk:** `rand()` spinner caption (`:106`) — EMUDET-03 +
  framing policy (never capture spinner frames).
- **Blocker:** unconditional `#include "Arduino.h"` (`EquationsApp.cpp:34`),
  `<esp_heap_caps.h>` (`:44`), `freertos/FreeRTOS.h`+`task.h` (`:45-46`) —
  require `#ifdef ARDUINO` guards + native heap-budget fallback (parity spec
  open decision 2).
- **Existing scripts/goldens:** none.
- **Proposed smoke/asserts/golden:** QA plan §B/§D (equations_smoke +
  linear-solve + steps-leak; list-screen golden only).
- **Firmware parity checklist:** isActive()-gated key forwarding; update tick;
  MODE exit; modifier semantics; battery `setBatteryLevel(100)` site
  (`EquationsApp.cpp:571`) tracked by SB-02.
- **Risk: medium-high. Phase: P-05 (last of the four). Rationale:** highest
  test value (3350 untested lines, EMU-5/PRD-3/MEM-6) but needs source guards +
  biggest closure + ROUTE-04; land after Calculus proves the CAS-closure path.

## 6. Calculus (card 3) — P-05

- **Source:** `src/apps/CalculusApp.h/.cpp` (187/1130).
- **Firmware:** card 3 (`SystemApp.cpp:759-763, 890`); owns its modifier
  handling (SystemApp global SHIFT/ALPHA layer skips it,
  `SystemApp.cpp:484-493`).
- **Emulator:** dead card. **NativeHal needs:** 8 sites, card id 3; no update
  tick (firmware doesn't tick it, `SystemApp.cpp:263-264`).
- **Lifecycle:** lazy begin (`CalculusApp.cpp:122-127`); `end()` canvases →
  StatusBar (`:143`) → screen (`:145-162`) → `_casSteps.clear()` +
  `_arena.reset()` (`:167-168`). **Note:** `_stepRenderers` not cleared in
  `end()` (cleared per-compute `:786,1031`) — watch in LIFE-01.
- **Input:** PRESS/REPEAT state dispatch (`:500-509`); KeyboardManager
  (`:516-518`); UP/DOWN + GRAPH toggle derivative↔integral (`:628-639`).
- **Persistence:** none. **Timers:** none; spinner + **synchronous
  `lv_timer_handler()`** in `showComputing()` (`:464`).
- **MathRenderer:** input/result/original canvases + step renderers
  (`CalculusApp.h:105-142`).
- **CAS:** SymDiff, SymIntegrate (+SymSimplify/ASTFlattener/SymExprToAST
  already whitelisted); `.cpp` adds SymToAST (`CalculusApp.cpp:31`).
  **Needed compile additions: `SymDiff.cpp`, `SymIntegrate.cpp`,
  `SymToAST.cpp` + link-verified transitive closure.**
- **Fonts:** stix_math_18. **Memory:** no heap_caps/PSRAM (audit).
- **LVGL risk:** low-medium (canvas-heavy but no timers/pipeline).
- **Determinism:** `rand()` captions (`:449-453`) — seed fence + framing.
- **Arduino APIs:** none (clean audit) — **no source changes needed**.
- **Scripts/goldens:** none existing; proposed per QA plan §B
  (calculus_smoke/diff/int; input-screen golden).
- **Hooks:** `debugResultString()`, `debugMode()`.
- **Parity checklist:** modifier-ownership parity (firmware's global layer
  skips APP_CALCULUS — emulator has no global layer, so behavior matches by
  construction; record it); battery site `CalculusApp.cpp:247` (SB-02);
  Spanish error strings (`:921-923`) — assert with `_contains`.
- **Risk: medium. Phase: P-05 (first CAS app). Rationale:** Arduino-clean,
  bounded closure, unlocks CAS corpus rows blocked on P-05
  (`NUMOS_CAS_IMPLEMENTATION_TICKETS.md:94`).

## 7. Integral — do not enable

- **Source:** `src/apps/IntegralApp.h/.cpp` (159/879).
- **Reachability:** **dead on firmware** — no SystemApp member
  (`SystemApp.h:138-160`), no dispatch case (`SystemApp.cpp:885-913`), no card
  (`MainMenu.cpp:88-113`). Emulator: nothing.
- **Profile (for the record):** CalculusApp-shaped; cas SymIntegrate + SymToAST;
  `rand()` caption (`IntegralApp.cpp:334`); no persistence; no Arduino APIs;
  `end()` order correct (`IntegralApp.cpp:111-149`).
- **Risk: n/a. Phase: do-not-enable. Rationale:** enabling would create
  emulator-only reachability, inverting parity (spec §I). Fate belongs to the
  P-07 dead-code sweep (delete after confirming CalculusApp covers integrals).
  Roadmap APPPAR-03 records this disposition.

## 8. Matrices (card 9) — P-05, first

- **Source:** `src/apps/MatricesApp.h/.cpp` (160/927) + `MatrixEngine.h/.cpp`.
- **Firmware:** card 9 (`SystemApp.cpp:789-793, 896`).
- **Emulator:** dead card. **NativeHal needs:** 8 sites, card id 9,
  `AppMode::MATRICES`; no tick.
- **Lifecycle:** `begin()` builds all views (`MatricesApp.cpp:96-112`); `end()`
  StatusBar → screen → null+memset (`:118-141`).
- **Input:** PRESS/REPEAT view dispatch (`:692-699`); digits via
  `keyCodeDigitValue` (`:531` area); F1-F4 row/col ops (`:892-903`);
  KeyboardManager included but unused. F1/F2 also map to LVGL NEXT/PREV in
  LvglKeypad (`LvglKeypad.cpp:101-121`) — irrelevant here since keys go direct
  to handleKey, not through the indev.
- **Script commands:** existing tokens suffice (`f1..f4` exist,
  `NativeHal.cpp:543-546`).
- **Hooks:** `debugCellValue(m,r,c)`, `debugLastResultString()`.
- **Persistence:** none — static `Matrix _matrices[]` (`MatricesApp.h:117`).
- **Timers:** none (`MatricesApp.cpp:156` load anim only). **MathRenderer:**
  none — `lv_table` (`:294,334`). **CAS/Giac:** none. **Fonts:** stix_math_18.
- **Memory:** static 5×5 doubles (`MatrixEngine.h:41`); stack temporaries in
  det/inverse (`MatrixEngine.cpp:121,165`).
- **LVGL risk:** low. **Determinism risk:** none (no rand/clock; ctor inits
  all, `MatricesApp.cpp:49-86`).
- **Scripts/goldens:** none existing; proposed matrices_smoke/edit/ops + 2
  goldens (QA plan §B/§C).
- **Parity checklist:** MODE-only exit; no autosave; no modifier semantics.
- **Risk: low. Phase: P-05 (first). Rationale:** identical safety profile to
  already-enabled Stats/Prob; proves the ROUTE-01 checklist end-to-end at
  minimum cost.

## 9. Python (card 8) — defer (decision-gated)

- **Source:** `src/apps/PythonApp.h/.cpp` (160/928) + `PythonEngine.*`.
- **Firmware:** card 8 (`SystemApp.cpp:784-788, 895`).
- **Emulator:** dead card.
- **Persistence:** LittleFS `/py/<name>.py` (save `PythonApp.cpp:654-671`,
  load `:673-689`, delete `:468-470`, enumerate `:340-357`, seed
  `/py/hello.py` `:696`). **All `#ifdef ARDUINO`** — native branches are
  canned stubs (fake `hello.py` list `:358-362`). So natively today it would
  run but with fake, non-persistent scripts — a parity lie, not a crash.
- **Filesystem gap:** real enablement needs `hal/FileSystem` directory APIs
  (`openNextFile/isDirectory/mkdir` — none implemented) + `File::print/
  readBytes` shims, then un-gating the app's FS code paths.
- **Engine:** custom line interpreter, deterministic, no rand
  (`PythonEngine.cpp:450-551`); 1 MB heap allocated but **unused** by the mock
  engine (`PythonEngine.cpp:92-98`, vars in static `s_vars[32]` `:36-38`).
- **Timers/rand:** none. **Fonts:** stix_math_18 + montserrat_14; textarea
  editor with full char mapping (`PythonApp.cpp:836-915`).
- **Risk: medium (scope, not danger). Phase: future.** Rationale: product
  identity undecided (custom interpreter vs MicroPython — Human decision,
  APPPAR-06); needs FileSystem extension + sandbox (FIX-02) first; QA assets
  would test a placeholder engine.

## 10. Periodic Table / Chemistry (card 11) — P-05

- **Source:** `src/apps/PeriodicTableApp.h/.cpp` (153/1121) + `ChemCAS.*` +
  constexpr `ChemDatabase.h`/`ChemExtraData.h`.
- **Firmware:** card 11 (`SystemApp.cpp:799-803, 898`); exits on **MODE or AC**
  (`SystemApp.cpp:579-585`) — emulator must replicate the AC exit.
- **Emulator:** dead card. **NativeHal needs:** 8 sites, card id 11, canonical
  name "Chemistry" (card name, `MainMenu.cpp:101`).
- **Lifecycle:** `end()` StatusBar destroy → screen delete →
  `resetPointers()` (`PeriodicTableApp.cpp:106-111`) — unique extra call;
  keep as-is (app source untouched).
- **Input:** PRESS/REPEAT (`:677`); F1/F2/F3 tabs (`:700-702`); SHIFT+arrows
  tab-cycle via KeyboardManager (`:698,705`); text entry into `_molarBuf/
  _balBuf` via `keyToChar` with ALPHA_A-F and SHIFT case toggle (`:630-670`);
  **`KeyCode::SOLVE` inserts '='** (`:870`) → needs a `solve` script token
  (ROUTE-08, consumer-cited).
- **Persistence/timers/rand:** none. Data is constexpr rodata
  (`ChemDatabase.h:51,172,186`; `ChemExtraData.h:46,167`; no PROGMEM reads).
- **Rendering:** custom `LV_EVENT_DRAW_MAIN` grid (`:204,515-599`) —
  first emulator coverage of this path; stix_math_18 inside draw callback
  (`:571`).
- **ChemCAS:** exact int32/int64 rational balancer, stack matrices, overflow
  guards (`ChemCAS.cpp:365,481,116,360,484`) — deterministic.
- **Compile additions:** `PeriodicTableApp.cpp` + `ChemCAS.cpp` only.
- **Hooks:** `debugSelectedSymbol()`, `debugBalanceResult()`.
- **Scripts/goldens:** none existing; proposed periodic_smoke/nav/balance +
  2 goldens.
- **Risk: low. Phase: P-05. Rationale:** zero engine risk; buys unique
  draw-layer coverage + first ChemCAS oracle.

## 11. Bridge Designer (card 12) — P-06 (timer wave)

- **Source:** `src/apps/BridgeDesignerApp.h/.cpp` (175/903).
- **Firmware:** card 12 (`SystemApp.cpp:804-808, 899`).
- **Timers:** 16 ms physics timer created entering SIM mode
  (`BridgeDesignerApp.cpp:836`), deleted on stop (`:862`) and end (`:135-138`).
- **Teardown nuance:** `_statusBar.destroy()` before screen delete but
  `resetPointers()` after (`:139-142`) — ordering quirk to note, not change.
- **Persistence/rand:** none (grep-clean).
- **Sim determinism:** Verlet, fixed `DT=1/60`, 8 constraint iters
  (`BridgeDesignerApp.h:98`; `physicsStep` `:460-467`) — fully deterministic
  per tick; frame-indexed under `--deterministic`.
- **Memory:** ~4.3 KB node/beam arrays (`:98-102`).
- **Rendering:** `LV_EVENT_DRAW_MAIN` custom draw (`:186,263-444`).
- **Arduino APIs:** none unguarded.
- **Risk: medium (sim-frame golden stability unproven). Phase: P-06
  (APPPAR-09a).** Rationale: clean code, but golden value requires self-diff
  proof of frame stability; asserts need new hooks (node stress readouts).

## 12. CircuitCore (card 13) — P-06/P-07 (storage wave)

- **Source:** `src/apps/CircuitCoreApp.h/.cpp` (304/2067) + MnaMatrix,
  ComponentFactory, LogicGates, PowerSystems, LuaVM (stub).
- **Firmware:** card 13; **SystemApp-side autoSave on MODE / AC-at-toolbar
  exit** (`SystemApp.cpp:595-606`) — divergence class D8 must be replicated.
- **Timers:** 16 ms sim timer, MNA at 30 Hz via frame-modulo
  (`CircuitCoreApp.cpp:781, 888-897`); idle autosave via `lv_tick_get()`
  30 s threshold (`:440-441`) — lv_tick-based, so deterministic-safe but will
  fire in long scripts (FS write → sandbox required).
- **Persistence:** `/circuits/<name>` + `/circuits/autosave.dat`, text via
  `f.printf` (`:1372-1418,1714-1721`) — **all `#ifdef ARDUINO`; native no-op
  today** (`:1707-1708`). Enablement choice: keep no-op (register divergence)
  or un-gate with `File::printf` shim + sandbox. Recommend un-gate under
  FIX-02 so parity is real.
- **Memory:** components array in PSRAM-or-new (`:126-129`); MnaMatrix ~16 KB
  (`MnaMatrix.cpp:54-64`); large in-object undo buffer
  (`CircuitCoreApp.h:203-214`).
- **Determinism:** MNA Gaussian elimination fixed-step — deterministic
  (`MnaMatrix.cpp:230-288`); no rand.
- **LuaVM:** stub — scripts stored, never executed (`LuaVM.cpp:44-46,77-80`);
  product decision outstanding (ground truth §F).
- **Risk: medium-high (FS + exit hooks + size). Phase: P-06 storage wave
  (APPPAR-09b), after FIX-02.**

## 13. Fluid2D (card 14) — P-06/P-07 (storage wave)

- **Source:** `src/apps/Fluid2DApp.h/.cpp` (256/1523).
- **Firmware:** card 14; SystemApp-side autoSave on MODE
  (`SystemApp.cpp:608-615`).
- **Timers:** 33 ms sim timer created **unconditionally in createUI**
  (`Fluid2DApp.cpp:257`) — sim runs from load; CFL sub-stepping (`:1374-1382`).
- **Persistence:** `/fluid/autosave.f2d` binary (`:1452-1513`) — **ARDUINO-
  gated, native no-op** (`:1453,1484,1516`). Same un-gate decision as Circuit.
- **Memory:** ~159 KB grids + particles (`:128-155`; SIZE math per audit).
- **Determinism:** Stam solver, state-derived dt, index-derived particle spawn
  — **no rand** (`:542-548`); deterministic per tick.
- **Rendering:** `LV_EVENT_DRAW_MAIN` per-cell rects (`:249,1106-1350`).
- **Risk: medium-high (always-on sim + memory + FS). Phase: P-06 storage wave.**
  Goldens likely limited to paused states.

## 14. ParticleLab (card 15) — P-06/P-07 (storage wave)

- **Source:** `src/apps/ParticleLabApp.h/.cpp` (131/776) + ParticleEngine.
- **Firmware:** card 15 (`SystemApp.cpp:819-823, 902`).
- **Timers:** 33 ms sim timer (`ParticleLabApp.cpp:203, 761-768`).
- **RNG (critical):** ParticleEngine uses a **file-static xorshift seeded to
  constant 12345** (`ParticleEngine.cpp:84-95`), used throughout physics and
  the SPRAY brush (`ParticleLabApp.cpp:182`); `clear()` does **not** reset the
  RNG state (`ParticleEngine.cpp:147`) — reproducible from process start only;
  cross-relaunch state persists. Scripts must treat any post-sim frame as
  process-start-relative; an engine `resetRng()` NATIVE_SIM hook is the clean
  fix if per-scenario determinism is wanted.
- **Persistence:** `/save.pt` raw grid (`:408-445`) — **ARDUINO-gated, native
  no-op**.
- **Memory:** 76.8 KB grid (`ParticleEngine.cpp:118`, 4-byte particle
  static_assert `ParticleEngine.h:61`) + 153.6 KB render buffer
  (`ParticleLabApp.cpp:106-108`).
- **Rendering:** `lv_image` over RGB565 buffer + `lv_draw_image` in DRAW_MAIN
  (`:746-755`); positional dither hash — deterministic (`:551`).
- **Risk: medium-high. Phase: P-06 storage wave.** Goldens: initial/paused
  frames only.

## 15. NeuralLab (card 16) — P-06 late (seeded-RNG gate)

- **Source:** `src/apps/NeuralLabApp.h/.cpp` (172/1197) + NeuralEngine.
- **Firmware:** card 16 (`SystemApp.cpp:824-828, 903`).
- **Timers:** 33 ms train timer, 50 epochs/frame (`NeuralLabApp.cpp:230,
  1006-1012`).
- **RNG (blocker for pixels):** unseeded `rand()` for weight init (Box-Muller,
  `NeuralEngine.cpp:140-141,162`) and scenario point jitter
  (`NeuralLabApp.cpp:372,380`). Deterministic only from clean process start
  with fixed call order — EMUDET-03 `srand(1)` pins it, but any preceding
  rand() consumer (Equations caption!) shifts the stream. **No pixel goldens
  until a seeded-init debug hook exists.**
- **Persistence:** `/neural.nn` — **NOT ARDUINO-gated**: `LittleFS.open` runs
  natively (`NeuralLabApp.cpp:950,965-967`) → would write
  `emulator_data/neural.nn` **today** if enabled. Hard sandbox dependency
  (STORE-01 row + FIX-01 before enablement).
- **Rendering:** `lv_image` + `LV_EVENT_DRAW_MAIN_END` no-op cb
  (`:198-202,1002-1004`); 153.6 KB + 9.6 KB buffers (`:43-49`).
- **Risk: high (RNG + native FS write). Phase: P-06 late (APPPAR-09c),
  assert-only.**

## 16. OpticsLab (card 17) — P-06 (timer wave, easiest sim)

- **Source:** `src/apps/OpticsLabApp.h/.cpp` (124/601) + OpticsEngine +
  OpticsRenderer.h.
- **Firmware:** card 17 (`SystemApp.cpp:829-833, 904`).
- **Timers:** 33 ms timer, but it only re-renders static state + updates the
  StatusBar clock (`OpticsLabApp.cpp:253, 587`) — no physics stepping.
- **Persistence/rand:** none. **Memory:** 138 KB render buffer (`:50-57,92`);
  engine is fixed member arrays (`OpticsEngine.h:177-227`).
- **Rendering:** own software rasterizer into RGB565 buffer
  (`OpticsRenderer.h:37-170`) — no LVGL fonts in canvas; deterministic pure
  float math.
- **Risk: medium-low. Phase: P-06 (APPPAR-09a, first sim).** Best sim-app
  golden candidate: static scene → stable pixels (verify via self-diff; the
  clock is in the StatusBar band, already masked).

## 17. NeoLanguage (card 18) — future

- **Source:** `src/apps/NeoLanguageApp.h/.cpp` (168/958) + ~11k-line Neo*
  stack.
- **Firmware:** card 18 (`SystemApp.cpp:834-838, 905`).
- **Persistence:** `/neolang.nl` — save/load **run natively** via the
  FileSystem shim (`NeoLanguageApp.cpp:897-944`; only `LittleFS.begin` is
  gated `:902-906`), so `begin()` auto-loads and F2 writes
  `emulator_data/neolang.nl` **today** if enabled. Additionally **NeoIO's
  `file()` builtin uses raw `std::fopen` with cwd-relative paths on native**
  (`NeoIO.cpp:62-119,72`) — escapes the LittleFS root entirely; a sandbox
  bypass that must be fixed (route through hal/FileSystem) before enablement.
- **Timers/rand:** none; `time_it` builtin reads wall clock
  (`NeoInterpreter.cpp:767-800`) — output-only nondeterminism.
- **Memory:** 124.8 KB transient plot buffer (`NeoLanguageApp.cpp:726-730`);
  NeoArena + cas::SymExprArena soft reset >70% (`NeoLanguageApp.h:86`).
- **Input:** full text editor with F-key chords (`:279-408`) — script grammar
  can drive it but scripts become long char streams.
- **Risk: high (FS bypass + arenas + editor). Phase: future.** Precondition:
  NeoIO path containment + FIX-02 + STORE rows.

## 18. Fractal (card 19) — P-06 (needs ROUTE-04)

- **Source:** `src/apps/FractalApp.h/.cpp` (166/961) + `math/FractalEngine.*`.
- **Firmware:** card 19 (`SystemApp.cpp:839-843, 906`); **AC-only exit + app-
  initiated exit via `consumeExitRequest()`** (`SystemApp.cpp:649-658` — no
  MODE case!); `update()` state machine ticked (`SystemApp.cpp:290-292`).
- **Threading:** dual-core FreeRTOS render task is **ARDUINO-only**
  (`FractalApp.cpp:490-535`); native falls through to synchronous full-frame
  render (`:739-762`) — blocks the loop for that frame; acceptable headless.
- **Timers/rand/FS:** none (no lv_timer — update()-driven; grep-clean).
- **Memory:** 138 KB buffer + ~32 KB orbit + 32 KB preview member array
  (`FractalApp.h:94,139`; `MemoryUtils.h:70-72`).
- **Determinism:** escape-time math — fully deterministic both paths.
- **NativeHal needs:** 8 sites + ROUTE-04 tick + the AC/consumeExitRequest
  exit contract replicated exactly.
- **Risk: medium. Phase: P-06 (APPPAR-09d).** Goldens plausible (deterministic
  pixels) once render-complete is hook-detectable (`debugRenderIdle()`).

## 19. Math Visual (card 20, validate env) — keep as-is

- **Source:** `src/apps/MathRenderVisualTestApp.h/.cpp` (64/725); TU compiled
  `#if defined(NUMOS_MATH_VISUAL_VERIFY) || !defined(ARDUINO)`
  (`MathRenderVisualTestApp.cpp:1`) — native-compatible by design but **not
  whitelisted**, so not in the emulator binary.
- **Firmware:** card 20 only with `NUMOS_MATH_VISUAL_VERIFY`
  (`MainMenu.cpp:110-112`; `SystemApp.cpp:844-850`).
- **Emulator:** the curated MathShowcase (id 100) covers the deterministic-
  capture role (`NativeHal.cpp:1090-1144`); the interactive test app's extra
  value (cursor modes, serial metric dumps `:642`) is diagnostic.
- **Risk: low. Phase: keep as-is (APPPAR-07).** Do not create a second route;
  extend showcase cases via `MathRenderVisualCases.cpp` instead.

## 20. StixGlyphGallery — P-06 diagnostic (needs shim fix)

- **Source:** `src/ui/StixGlyphGallery.h/.cpp` — **not an app**: two free
  functions (`StixGlyphGallery.h:24,27`) called at device boot under
  `NUMOS_STIX_DIAGNOSTICS` (`main.cpp:190,194` per ground truth §F).
- **Blockers:** unconditional `#include <Arduino.h>`
  (`StixGlyphGallery.cpp:18` — no Arduino.h exists natively); and
  `showStixGlyphGallery` is a **blocking modal** that busy-loops
  `millis()/lv_timer_handler/delay` for `holdMs` (`:165-169`) — incompatible
  with the emulator's frame loop and determinism.
- **Enablement shape (proposed):** small NativeHal-side wrapper mode (like
  MathShowcase) that builds the same panel objects **without** the busy-wait,
  plus the include guarded to ArduinoCompat. Diagnostics function
  (`runStixGlyphAlignmentDiagnostics`, `:57-112`) is separately valuable as a
  boot-time REN-2 check (prints pass/fail — could gate CI as a text assert).
- **Risk: low. Phase: P-06 (APPPAR-08). Rationale:** only automated tofu/
  glyph-coverage oracle available; needs the two small fixes above.

## 21. Tutor — do not enable

- **Source:** `src/apps/TutorApp.h/.cpp`.
- **Reachability:** dead on firmware (no member/dispatch/card — ground truth
  §F). Uses `lv_textarea` input (`TutorApp.cpp:290-294`), its own title bar
  (no ui::StatusBar, `:275-287`), CAS RuleEngine closure overlapping
  Equations'.
- **Risk: n/a. Phase: do-not-enable** (same parity-inversion argument as
  Integral; P-07 owns fate; superseded by CalculationApp edu-steps).

---

## Cross-app summary table

| App | Card | Arduino blockers | New compile units | FS surface (native behavior) | Timers | rand() | Risk | Phase |
|---|---|---|---|---|---|---|---|---|
| Matrices | 9 | none | MatricesApp, MatrixEngine | none | none | none | low | **P-05** |
| PeriodicTable | 11 | none | PeriodicTableApp, ChemCAS | none | none | none | low | **P-05** |
| Calculus | 3 | none | CalculusApp + SymDiff/SymIntegrate/SymToAST + closure | none | none | captions | medium | **P-05** |
| Equations | 2 | Arduino.h/heap_caps/FreeRTOS (`EquationsApp.cpp:34,44-46,487-518`) | EquationsApp + largest cas closure | none | none (pipeline via update) | caption | med-high | **P-05** |
| OpticsLab | 17 | none | OpticsLabApp, OpticsEngine | none | 33 ms (render-only) | none | med-low | P-06 |
| Bridge | 12 | none | BridgeDesignerApp | none | 16 ms | none | medium | P-06 |
| Fractal | 19 | FreeRTOS already gated | FractalApp, FractalEngine | none | update()-driven | none | medium | P-06 |
| StixGallery | — | `Arduino.h` include (`StixGlyphGallery.cpp:18`); blocking modal | StixGlyphGallery + wrapper | none | none | none | low | P-06 |
| Circuit | 13 | none | CircuitCore + MnaMatrix + ComponentFactory + LogicGates + PowerSystems + LuaVM | `/circuits/*` (ARDUINO-gated: native no-op today) | 16 ms + idle autosave | none | med-high | P-06/07 |
| Fluid2D | 14 | none | Fluid2DApp | `/fluid/*` (gated no-op) | 33 ms always-on | none | med-high | P-06/07 |
| ParticleLab | 15 | none | ParticleLabApp, ParticleEngine | `/save.pt` (gated no-op) | 33 ms | fixed-seed xorshift, never re-seeded | med-high | P-06/07 |
| NeuralLab | 16 | none | NeuralLabApp, NeuralEngine | `/neural.nn` (**writes natively**) | 33 ms | unseeded rand() core | high | P-06 late |
| NeoLang | 18 | none | NeoLanguageApp + ~11k-line stack | `/neolang.nl` (**writes natively**) + NeoIO `std::fopen` cwd bypass | none | none | high | future |
| Python | 8 | none | PythonApp, PythonEngine + FileSystem dir APIs | `/py/*` (gated: fake stubs natively) | none | none | medium | future |
| Integral | — | — | — | — | — | caption | — | never (dead) |
| Tutor | — | — | — | — | — | none | — | never (dead) |
| Math Visual | 20 | — | — | — | — | none | — | keep as-is |
