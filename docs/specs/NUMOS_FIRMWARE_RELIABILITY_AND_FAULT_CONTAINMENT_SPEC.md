# NumOS — Firmware Reliability & Fault-Containment Specification

> Normative reliability architecture for NumOS on ESP32-S3 N16R8. Verified against the working tree at `63e7408` (post Phase MEM-A, 2026-07-02). Every source claim cites `file:line` against that tree. **VERIFIED** = read from source during this audit. **PROPOSED** = design to be implemented; does not exist yet.
>
> Companion documents (same date/commit): `NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md` (log/serial protocol), `NUMOS_SAFE_MODE_AND_RECOVERY_SPEC.md` (boot-loop/safe-mode), `NUMOS_RELIABILITY_IMPLEMENTATION_TICKETS.md` (RT-xx backlog). Prior art: `NUMOS_MEMORY_AND_PSRAM_AUDIT.md`, `NUMOS_MEMORY_ALLOCATION_POLICY.md`, `NUMOS_MEM_A_IMPLEMENTATION_NOTE.md`.
>
> **Corrections to earlier specs found during this audit** (source wins over docs):
> 1. `NUMOS_ARCHITECTURE_GROUND_TRUTH.md` §G claims "Giac escalation: `MathEvaluator.cpp:45`… call site `#ifdef ARDUINO`". In the current tree there is **no VPAM→Giac escalation**: `MathEvaluator::evaluateWithGiac` (`src/math/MathEvaluator.cpp:509-513`) and the `GiacBridge` class shim (`src/math/GiacBridge.h:25-34`) have **zero callers**. The only runtime path into Giac is the serial console: `SerialBridge.cpp:155` → `solveWithGiac()` (`src/math/giac/GiacBridge.cpp:390`).
> 2. Ground truth §D says SerialBridge has a "16-event queue"; it is 32 (`SerialBridge.h:63-65`) — already corrected in the memory audit §9.
> 3. `SymSimplify.cpp:26` comments `MAX_PASSES = 8`; the compiled constant is 10 (`SymSimplify.h:60`).
> 4. CircuitCore persists text CSV at `/circuits/*.dat` (`CircuitCoreApp.cpp:1362-1479`), not `/circuit.bin`; PythonApp uses `/py/*.py` (`PythonApp.cpp:654-671`), not `/scripts/*.py`.

---

## A. Executive summary

NumOS today is a **single-task, cooperatively-scheduled firmware with no fault containment**. Everything — LVGL rendering, all 20 apps, VPAM evaluation, the in-tree CAS, and the entire Giac CAS — runs synchronously on one 64 KB `loopTask` stack (`platformio.ini:44`), driven by a 5 ms loop (`main.cpp:241-269`). The current reliability posture, verified from source:

- **Hangs are invisible and unrecoverable.** No code in `src/` arms, feeds, or configures any watchdog (grep: the only WDT-adjacent call is a cooperative `yield()` inside one plot loop, `GrapherApp.cpp:2335`). Whether the framework's task WDT even watches `loopTask` is an **unverified framework default** (no local `sdkconfig`; `platformio.ini` sets no WDT flags). A blocked `solveWithGiac()` — which has **no timeout of any kind** (§C4) — either freezes the device forever or trips an unconfigured framework default we have never characterized.
- **Crashes leave no trace.** There is no `esp_reset_reason()` call, no `RTC_NOINIT_ATTR`/`RTC_DATA_ATTR` state, and no boot counter anywhere in `src/` (verified by grep). A panic reboots into a normal boot; a boot-looping device just loops. The only panic tooling is `monitor_filters = esp32_exception_decoder` (`platformio.ini:19`) — useful only with a PC attached.
- **Boot failure = silent brick with serial-only symptom.** Draw-buffer allocation failure spins forever (`main.cpp:145-148`: `while (1) delay(1000)` after one serial line); display-init failure likewise (`main.cpp:156-159`).
- **App `begin()` cannot fail, by signature.** Every app's `begin()` returns `void`; no `lv_obj_create` return is null-checked anywhere in `src/apps/` or `src/ui/MainMenu.cpp:362-419`; 17 of 20 apps call `lv_screen_load_anim(_screen, …)` in `load()` without re-checking `_screen` (§C3). Four sim apps leak PSRAM buffers permanently on a partial `begin()` (§C3).
- **MEM-A (merged) already delivered the first bricks**: grep-stable `[MEM]` probes at lifecycle boundaries (`src/utils/MemProbe.h`, probes at `main.cpp:231,261`, `SystemApp.cpp:216,741`, `GiacBridge.cpp:394,439,442,445`), a visible LVGL OOM path (`lv_conf.h:153-164`: assert → printf → `abort()` instead of silent `while(1)`), splash reclamation (`SplashScreen.cpp:119-141`), and lazy CAS arenas.

This spec defines: the failure taxonomy grounded in source (§C), the exact invariants the firmware must maintain (§D), a proposed containment architecture — lifecycle guard, watchdog policy, memory floors, safe mode, diagnostics (§E), per-subsystem failure contracts (§F), the required externally-visible behavior for every failure class (§G), rejected alternatives (§H), and acceptance tests (§I). Implementation is decomposed in `NUMOS_RELIABILITY_IMPLEMENTATION_TICKETS.md` (RT-01…RT-16).

Design stance (justified in §H): **contain what can be contained cheaply (allocation failures, runaway loops, bad input, bad files); convert everything else into a fast, *recorded*, *bounded* reboot** (watchdog + panic record + boot-loop safe mode). We do not attempt task isolation or exception-based app sandboxing.

---

## B. Current reliability architecture (verified from source)

### B.1 Execution model

| Fact | Evidence |
|---|---|
| One task runs everything; 64 KB stack | `-DARDUINO_LOOP_STACK_SIZE=65536` `platformio.ini:44`; Giac/CAS/LVGL all called from `loop()` paths |
| Main loop: `lv_timer_handler` → `g_app.update()` → drain serial keys → 5 s heartbeat → `delay(5)` | `main.cpp:241-269` |
| Key processing drains the whole queue synchronously per frame | `SystemApp.cpp:252-257` (`while (_keypad.pollEvent(ev)) handleKey(ev);`) |
| Only other task: FractalApp render task, 32 KB stack, core 1, transient | `FractalApp.cpp:481-511` create; `:513-540` stop (1.5 s bounded join, then force `vTaskDelete`) |
| Exceptions are enabled firmware-wide (Giac requirement) and the global `operator new` **throws `std::bad_alloc`** on full exhaustion | `platformio.ini` build_unflags removes `-fno-exceptions`; `GiacAlloc.cpp:42-46,52-56` (PSRAM-first, any-heap fallback, then `throw`) |
| No app or dispatcher catches anything; only `solveWithGiac` has try/catch | `GiacBridge.cpp:395,441-447`; grep: no other `catch` in `src/apps/` or `SystemApp.cpp` |

### B.2 Boot sequence (post-MEM-A, current line numbers)

`setup()` (`main.cpp:84-232`): `Serial.begin(115200)` `:85` → UART0 50 ms settle `:96` (the 3 s CDC wait `:91-94` is compiled out in the shipped UART0 config, `platformio.ini:46-47`) → PSRAM report `:117-122` → `g_display.begin()` `:125` → `lv_init` + tick cb `:128-129` → **32 KB internal-DMA draw buffer** `:139-143`, **fail ⇒ eternal spin** `:145-148` → `g_display.initLvgl` `:154`, **no-display ⇒ eternal spin** `:156-159` → `LvglKeypad::init` `:164` → splash create/show + pump to completion `:173-186` → `g_app.begin()` `:198` (apps `new`ed lazily; LVGL trees not built) → 250 ms pump + `g_splash.destroy()` `:208-215` → `g_serial.begin()` `:218` → `NUMOS_MEM_PROBE("boot")` `:231`.

`SystemApp::begin()` (`SystemApp.cpp:93-159`): constructs `_vars.begin()` (NVS load) `:95`, `new`s all 20 apps `:104-123`, menu `create()`/`load()` `:136-138`, `LittleFS.begin(true)` **format-on-fail** `:144`, `VariableManager` load `:149`.

### B.3 App lifecycle machinery

- State is three members, no state machine: `_mode` (`SystemApp.h:176`), `_pendingTeardownMode` (`SystemApp.h:184`, sentinel `Mode::MENU` = none), `_teardownStartMs` (`SystemApp.h:185`).
- `returnToMenu()` (`SystemApp.cpp:860-883`): loads the persistent menu screen with a 200 ms FADE_IN (`MainMenu.cpp:152-157`), records `_pendingTeardownMode = _mode`, stamps time. **No `end()` here.**
- Deferred teardown: `update()` runs `teardownModeNow(_pendingTeardownMode)` after **250 ms** (`SystemApp.cpp:243-248`); `teardownModeNow` (`:182-218`) is a mode switch calling the app's `end()`, followed by the MEM-A exit probe (`:213-217`).
- `launchApp(id)` (`SystemApp.cpp:727-855`) force-flushes any pending teardown **synchronously first** via `flushPendingTeardownNow("launchApp")` (`:735`, impl `:220-232`), fires the enter probe (`:738-742`), then `switchApp(id)` + `_app->load()`.
- Key routing: after `returnToMenu()`, `_mode == MENU` immediately (`SystemApp.cpp:878`), so keys cannot reach an app whose `end()` is pending. MODE is intercepted by SystemApp before any app sees it (`SystemApp.cpp:501-505` Calculation; `:1019-1023` Grapher; pattern repeats `:514-664`).
- StatusBar contract: `destroy()`/`resetPointers()` only null pointers; widgets die with the parent screen (`StatusBar.cpp:152-166`); `update()` is pull-based, guarded, no `lv_timer` (`StatusBar.cpp:193-199`).
- MainMenu screen/group persist for process lifetime; deleted only in dtor or on re-`create()` (`MainMenu.cpp:139-142,282-287`). `buildGrid`/`buildCard` never null-check `lv_obj_create` (`MainMenu.cpp:362-419,425-465`).

### B.4 Existing guards worth preserving (the good parts)

| Guard | Evidence |
|---|---|
| LVGL OOM is now loud: assert → `[LVGL] ASSERT FAILED …` → `abort()` | `lv_conf.h:126-127,153-164` |
| `[MEM]` probes at boot/heartbeat/enter/exit/giac-pre/post | `MemProbe.h:65-105`; call sites §A |
| Deferred-teardown discipline + flush-before-launch | `SystemApp.cpp:220-248,735` |
| EquationsApp step-widget budget: `canAllocStep()` gates on largest-internal-block ≥ 40 KB and ≤ 90 children | `EquationsApp.cpp:486-503`; constants `EquationsApp.h:267-270` |
| EquationsApp `load()` rebuilds atomically on torn tree (`end(); begin();`) | `EquationsApp.cpp:467-473` |
| OpticsLab `load()` re-checks `_screen` after `begin()` | `OpticsLabApp.cpp:158-163` |
| ParticleLab cleans the engine when its buffer alloc fails | `ParticleLabApp.cpp:112-115` |
| Grapher editor nesting cap `MAX_NEST = 8` | `GrapherApp.cpp:2547-2593` |
| Grapher asymptote/NaN plot guards; render depth cap 12 with red overflow marker | `GrapherApp.cpp:1656-1679`; `MathRenderer.h:239`, `MathRenderer.cpp:1507-1514` |
| Grapher buffer-alloc failure degrades to a visible "ERR: INSUFFICIENT PSRAM" label; `replot()` guards `!_graphBuf` | `GrapherApp.cpp:563-601,1876` |
| RPN evaluator: iterative, fixed 64-slot stack, error-by-value, no exceptions | `Evaluator.h:52`, `Evaluator.cpp:27-39,79-253` |
| Fractal render task: bounded join (1.5 s) + force-kill | `FractalApp.cpp:513-540` |
| CI no-hang gates (Phase 9F, six scripts, per-script timeout) | `.github/workflows/emulator-build.yml:471-531` |

---

## C. Failure taxonomy

Each class: mechanism → current behavior → evidence. Ranked severity within class in §G.

### C1. Memory exhaustion

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C1.1 | LVGL 64 KB pool exhausted (step widgets, Grapher table cells, object-heavy screens) | `lv_malloc` NULL → `LV_ASSERT_MALLOC` → printf + `abort()` → panic reboot (post-MEM-A; previously silent hang) | `lv_conf.h:81-87,153-164` |
| C1.2 | PSRAM exhausted (Giac swell, Python 1 MB, sims, arenas) | per-site: `PSRAMBuffer` fails cleanly; arena `create` returns nullptr; global `new` falls back to **internal** RAM then throws `bad_alloc` — uncaught outside Giac ⇒ `abort()` | `MemoryUtils.h:70`; `SymExprArena.h` (nullptr contract); `GiacAlloc.cpp:28-31,42-46` |
| C1.3 | Bulk block falls back into internal RAM under PSRAM pressure, collapsing the internal heap (MEMX-07, still open) | silent domain flip; later DMA/LVGL failure | `SymExprArena.h` any-heap fallback; `CasMemory.h:328-330` |
| C1.4 | Internal heap erosion (Arduino `String` churn, mbedtls limbs) with `RESERVE_INTERNAL=0` | slow largest-block decay; no floor, no alarm | memory audit §2 mech.5; `Tokenizer.h:59` |
| C1.5 | Uncaught `std::bad_alloc` from any plain `new` under total exhaustion | `abort()` → panic, no record | `GiacAlloc.cpp:42-46`; no app-level catch (grep) |
| C1.6 | Stack exhaustion on the shared 64 KB loopTask (deep recursion — see C2/C4/C5) | corruption or StoreProhibited panic; no stack canary tooling beyond FreeRTOS defaults (unverified) | `platformio.ini:44` |

### C2. LVGL object lifetime

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C2.1 | Synchronous screen delete under FADE_IN (the documented hang class) | prevented by deferred teardown; **no runtime assertion** enforces it — a future regression re-introduces the hang | `SystemApp.cpp:238-248`; incident history PROJECT_BIBLE §11 |
| C2.2 | App timers firing during the 250 ms teardown window against dying widgets | apps delete their timers in `end()` (Grapher `:169-172,189`; sims similar) — but `end()` runs *at the end* of the window; a timer that fires *during* the window touches a still-alive-but-doomed tree. Currently safe only because timers reference objects deleted in the same `end()` | `GrapherApp.cpp:169-189`; `SystemApp.cpp:243-248` |
| C2.3 | `lv_obj_create` returns NULL mid-tree-build; subsequent style/child calls deref it | crash (or LVGL assert → abort, since `LV_USE_ASSERT_NULL 1` covers LVGL-internal derefs but app code calls `lv_obj_set_size(card,…)` on the null directly) | `MainMenu.cpp:425-431`; every app `createUI` (no checks — lifecycle audit §2) |
| C2.4 | MODE during modal (Grapher templates/calc-menu, Calculation step viewer) abandons modal state via teardown rather than closing it | works today because all modal objects are children of the app screen; fragile against any modal that owns non-child resources | `GrapherApp.cpp:2369-2378`; `CalculationApp.cpp:233-252`; `SystemApp.cpp:501-505,1019-1023` |
| C2.5 | StatusBar pointers dangle between screen delete and `destroy()` | benign under current call orders; contract is convention-only | `StatusBar.cpp:152-166` |

### C3. App lifecycle failure (partial `begin`, unsafe `end`, `load` null-deref)

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C3.1 | Fluid2D partial `begin()`: 13 PSRAM buffers allocated, guard returns with `_screen==nullptr`; `end()` frees **everything** only `if (_screen)` | up to ~150 KB stranded forever; re-entry overwrites pointers (compounding); then `load()` derefs null `_screen` | `Fluid2DApp.cpp:128-141,158-163,187`; `:231-236` |
| C3.2 | NeuralLab: `_dbBuffer` allocated, `_renderBuf` fails → return; `end()` opens `if(!_screen) return` | `_dbBuffer` leaked; `load()` null-deref | `NeuralLabApp.cpp:131-136,162,185-190` |
| C3.3 | BridgeDesigner: same pattern (`_nodes`/`_beams`) | leak + null-deref | `BridgeDesignerApp.cpp:98-105,134,165-170` |
| C3.4 | CircuitCore: `_mna.allocate()` fails after `_components` succeeds → return; `end()` *does* free unconditionally, but re-`begin()` without `end()` re-mallocs `_components` over the live pointer | re-entry leak only; `load()` is guarded (`:245`) | `CircuitCoreApp.cpp:126-136,214-225,243-248` |
| C3.5 | 17/20 apps: `load()` calls `lv_screen_load_anim(_screen,…)` without re-checking `_screen` after `begin()` | reachable null-deref crash whenever `begin()` bails (only OpticsLab `:160`, CircuitCore `:245` guard; Fractal always creates the screen) | lifecycle audit §2; e.g. `CalculationApp.cpp:131`, `GrapherApp.cpp:234` |
| C3.6 | Fractal `_orbit = new(std::nothrow)` failure is logged and **ignored**; `_orbit` used later | null-deref during render | `FractalApp.cpp:96-97` |
| C3.7 | Fractal `end()` → `stopRenderTask()` can block the main loop up to 1500 ms inside the deferred-teardown tick | UI freeze ≤1.5 s; TWDT hazard once a watchdog exists | `FractalApp.cpp:522-531`; called from `SystemApp.cpp:247` |
| C3.8 | `teardownModeNow` default case: `MENU`/`APP_TABLE`/`STEP_VIEW` have no teardown | benign today; trap for future modes | `SystemApp.cpp:207` |
| C3.9 | KeyboardManager modifier LOCK/STORE persists across app exit (singleton never reset in `returnToMenu`) | next app misinterprets keys until user clears | `KeyboardManager.cpp:106-130`; `SystemApp.cpp:860-883` (no reset) |

### C4. CAS / Giac runaway

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C4.1 | **Giac `eval` has no timeout and no interrupt.** `caseval_maxtime` machinery is compiled out (guards `NSPIRE_NEWLIB`/`TIMEOUT` undefined); nothing ever sets `giac::ctrl_c`; recursion cap 100 throws but arrives too late for stack (64 KB shared) | `:factor(…)`/`:ifactor(bignum)` over serial blocks `loop()` indefinitely — frozen UI, frozen input, forever (no armed WDT in `src/`) | `GiacBridge.cpp:390-448`; `lib/giac/src/kglobal.cc:180-222,1679`; caller `SerialBridge.cpp:155` |
| C4.2 | Giac memory swell: uncapped PSRAM growth; `bad_alloc` caught at bridge (`:441-447`) but transient pressure pushes other allocators into internal-fallback (C1.3) | error string returned if lucky; collateral internal-heap damage regardless | `GiacBridge.cpp:441-447`; `GiacAlloc.cpp:28-31` |
| C4.3 | In-tree CAS unbounded recursion: `SymDiff::diff`, `SymSimplify::simplify*`, `ASTFlattener::flattenNode`, `RuleEngine::tryRewrite`, `SymExprToAST::convert`, `SymExpr::clone` all recurse with **no depth counter** (only `OmniSolver::isolateVar` guards at depth 20, `OmniSolver.cpp:638`) | deep user AST ⇒ stack overflow on the shared stack; silent corruption/panic | `SymDiff.cpp:33+`; `SymSimplify.cpp:209-736`; `ASTFlattener.cpp:107`; `RuleEngine.cpp:633-690`; `SymExprToAST.cpp:188` |
| C4.4 | RuleEngine AC-matching permutes sum/product terms via `std::next_permutation` with **no term-count cap** — O(N!) per rule per node, inside a 100-step fixpoint | ~10+ terms ⇒ combinatorial freeze of the main loop | `RuleEngine.cpp:122-138,201-217`; `RuleEngine.h:380` |
| C4.5 | Arena exhaustion mid-solve: factory helpers null-propagate via `ConsTable` (`ConsTable.cpp:198`), but `SymExpr.cpp` clone chains construct parents from unchecked possibly-null clones | live node with null child ⇒ later deref in simplify/print | `SymExpr.cpp:149,327,331` |
| C4.6 | Solver iteration caps exist and are sane: SymSimplify ≤10 passes (`SymSimplify.h:60`), SymIntegrate u-sub ≤5 / parts ≤3 (`SymIntegrate.h:45-49`, enforced `SymIntegrate.cpp:478`), Newton ≤50 (`SingleSolver.cpp:458`), HybridNewton ≤60 (`HybridNewton.h:87`) | bounded — keep | cited |
| C4.7 | Solves run synchronously in key handlers; only feedback is a one-frame spinner | multi-second UI freeze by design | `EquationsApp.cpp:964-975,2155-2257`; `CalculusApp.cpp:783-886` |

### C5. Renderer / pathological expression

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C5.1 | **Layout recursion is uncapped** while draw recursion caps at 12: `calculateLayout` recurses the full tree with no depth parameter, and runs on every edit/invalidate *before* the draw-depth check | deep AST ⇒ stack overflow in layout, the red "depth 12" marker never gets a chance | `MathAST.cpp:191,377-378,463-468`; `MathRenderer.cpp:591,629,737,1477` vs `:1507-1514` |
| C5.2 | CursorController has **no nesting or node-count limit** (Grapher gates at `MAX_NEST=8` app-side; **CalculationApp does not gate at all**) | held DIV/POW key in Calculation nests unbounded → C5.1 | `CursorController.cpp:365,418,461,482,961,985`; `GrapherApp.cpp:2547-2593`; no equivalent in `CalculationApp.cpp` |
| C5.3 | `NodeNumber` digits unbounded (`appendChar` has no cap); history deep-clones each entry (50 max entries, unbounded per entry) | heap growth; slow layout | `MathAST.cpp:287-294`; `CalculationApp.h:138`, `CalculationApp.cpp:559-566` |
| C5.4 | `MathEvaluator::evaluate` recursion has no depth guard; errors are by-value (`ExactVal::makeError`) | deep AST ⇒ stack overflow before any "Math ERROR" | `MathEvaluator.cpp:517-533`; `ExactVal.h:48-49,79` |
| C5.5 | PeriodicDecimal `longDivision` is O(maxDigits²) at 500 digits (~125 k comparisons) but hard-bounded | ≤ tens of ms; acceptable, keep bound | `Constants.h:189-252`; call `MathEvaluator.cpp:1263` |
| C5.6 | ExactVal fraction arithmetic can silently overflow int64 (no `__builtin_*_overflow` checks); `fromDouble` edges are guarded | wrong results, not crashes | `ExactVal.h:107-149`; `MathEvaluator.cpp:270-297,427-459` |
| C5.7 | Glyph assembly triple-fallback ends in "draw nothing" if even the base glyph is missing | invisible symbol, no crash | `MathRenderer.cpp:184-276` |
| C5.8 | `SymExprToAST::convert` puts no size cap on the VPAM tree it builds from a CAS result | a large simplified result feeds C5.1 | `SymExprToAST.cpp:188+` |

### C6. Storage corruption

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C6.1 | `/vars.dat` body is unvalidated: magic "VR01" is the only gate; `deserializeExactVal` memcpys raw int64s and sets `ok=true` unconditionally — **`den==0` is accepted** | corrupt-but-magic-valid file yields poisoned ExactVals (÷0 hazard downstream) | `VariableManager.cpp:153-163,196-226`; format `VariableManager.h:152-155` |
| C6.2 | Truncated `/vars.dat` loads partially and **returns true** | mixed loaded/zeroed state reported as success | `VariableManager.cpp:219-225`; logged as loaded `SystemApp.cpp:149` |
| C6.3 | No write verification anywhere; files opened `"w"` (truncate-in-place, not atomic) | power-loss mid-write ⇒ truncated file next boot | `VariableManager.cpp:169-190`; `PythonApp.cpp:664`; `NeoLanguageApp.cpp:916`; `ParticleLabApp.cpp:418` |
| C6.4 | **Every** `LittleFS.begin(true)` is format-on-fail: one bad mount silently erases all user data | data loss without consent | `SystemApp.cpp:144`; `NeoLanguageApp.cpp:903,926`; `CircuitCoreApp.cpp:1364,1483,1720`; `Fluid2DApp.cpp:1454,1485,1517` |
| C6.5 | ParticleLab `/save.pt`: headerless raw grid; exact-size check only; `Particle.type` unvalidated then indexes `MAT_LUT[31]` | corrupt full-size file ⇒ **OOB reads every sim tick** | `ParticleLabApp.cpp:426-445`; `ParticleEngine.cpp:256-264`; `ParticleEngine.h:97,228` |
| C6.6 | NVS store (`calcVars`) loads robustly (exact-size + magic) but `saveToNVS()` has **zero callers** — write-dead | vestigial; harmless but misleading | `VariableContext.cpp:37-70`; grep: no callers |
| C6.7 | Settings are never persisted (globals re-initialized every boot; SettingsApp has no save path) | user surprise, not a fault; also means safe mode can't "reset settings" — nothing is stored | `main.cpp:31-33`; `SettingsApp.cpp` (no persistence calls) |
| C6.8 | No factory-reset/clear-storage path exists anywhere | recovery from bad state requires reflashing or manual serial file surgery | grep (storage audit §8) |
| C6.9 | Var-store flash wear: full `/vars.dat` rewrite on **every** `→VAR` store | slow wear; low risk at 379 B | `CalculationApp.cpp:894-901` |

### C7. Input storm / bad input

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C7.1 | All queues drop silently on overflow: Keyboard 16, SerialBridge events 32 + 255-char line, LvglKeypad 8 | lost keystrokes under load, undetectable | `Keyboard.cpp:188-193`; `SerialBridge.cpp:74-75,116`; `LvglKeypad.cpp:55-59` |
| C7.2 | ENTER/replot storms serialize full synchronous evaluations/replots (queue drained per frame; emulator OS auto-repeat unthrottled) | multi-second freeze from held keys | `SystemApp.cpp:252-257`; `CalculationApp.cpp:479-525`; `NativeHal.cpp:809,832-834` |
| C7.3 | Grapher serialization truncates silently at 63 chars — plotted RPN can diverge from the displayed AST | wrong plot, no error | `GraphModel.h:55`; `GrapherApp.cpp:1112-1118,1266-1273` |
| C7.4 | Giac console accepts arbitrary `:`-prefixed input straight into C4.1 | serial user can freeze the device | `SerialBridge.cpp:149-160` |
| C7.5 | Invalid syntax paths are actually well-behaved: legacy parser fails → slot `valid=false`, no plot; RPN evaluator returns error struct | keep | `GraphModel.cpp:125-208`; `Evaluator.cpp:79-253` |
| C7.6 | Physical matrix inert (`CONNECTED_COLS=0`) — the entire input reliability story currently rides on SerialBridge | single input path; keyboard bring-up will add a new untested one | `Keyboard.h:82`, `Keyboard.cpp:84-86,135` |

### C8. Graphing stress

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C8.1 | `replot()` fully synchronous in key handlers, ends with blocking `lv_refr_now` | every pan/zoom key = full replot on the UI thread | `GrapherApp.cpp:1865-1912` (`:1909`) |
| C8.2 | Inequality/implicit worst case: 107×48 grid (≈5.1 k evals, each 1–2 RPN evals) + per-pixel shading on boundary cells (≤16 extra evals/cell) — **tens of thousands of evals per keystroke**, no wall-clock cap; `yield()` is a no-op on native | multi-second freeze; hardware TWDT exposure once armed | `GrapherApp.cpp:1716-1832` (`:1774-1793`); `:34-41` |
| C8.3 | Calc-menu analyses run synchronously: 200-pt scans ×100 iterations (root/extremum), Simpson N=100 | freeze proportional to function cost | `MathAnalysis.cpp:56,91,141,169,236`; `GrapherApp.cpp:3083-3155` |
| C8.4 | Table rebuild ≤126 evals + LVGL cell-string churn from the 64 KB pool, synchronous | bounded but stacked on C1.1 | `GrapherApp.cpp:2272-2359` |
| C8.5 | Explicit-curve worst case bounded: 40 segs × 2³ ≈ 320 evals/function ×6 | acceptable; keep constants | `GrapherApp.h:73-74`; `GrapherApp.cpp:1645-1706` |

### C9. Hardware reset / brownout / power

| # | Failure | Current behavior | Evidence |
|---|---|---|---|
| C9.1 | No reset-reason inspection, no boot counter, no RTC-retained state → crash loops are indistinguishable from clean boots | boot-loop = infinite loop with no escape | grep (boot audit §3) |
| C9.2 | Brownout/panic/WDT framework configuration is **unverified** (no local sdkconfig; no packages installed on this host) — the device's actual behavior on brownout is unknown to the repo | must be characterized on hardware; RT-01 | `platformio.ini` (no overrides); boot audit §2 |
| C9.3 | Draw-buffer/display failure ⇒ eternal spin with backlight state undefined | silent black brick (serial-only symptom) | `main.cpp:145-148,156-159` |
| C9.4 | `powerOff()` disabled no-op; deep-sleep block commented out; wake pin in dead code is `PIN_KEY_R1`=GPIO2 (`Config.h:85`) which belongs to the legacy matrix map | no sleep = no sleep-related bricks today; the dead code is a trap if re-enabled unaudited | `SystemApp.cpp:475-481,1052-1105` |
| C9.5 | Power-loss during `/vars.dat`/autosave writes | C6.3 truncation | cited above |

---

## D. Invariants (normative)

Notation: **[V]** already true in source (cite) — must not regress; **[P]** proposed — enforced by tickets in `NUMOS_RELIABILITY_IMPLEMENTATION_TICKETS.md`. Each invariant names its enforcement point and its acceptance test (§I).

### D.1 App lifecycle invariants

- **L-1 [V]** An app screen is never deleted synchronously on exit; teardown occurs ≥250 ms after `returnToMenu()` or via `flushPendingTeardownNow` before the next `load()`. (`SystemApp.cpp:220-248,735`.) *Regression gate: Phase 9F scripts.*
- **L-2 [P]** `end()` is idempotent and safe after a partial `begin()`: every dynamic resource is freed by a null-guarded, per-pointer path **not** gated on `_screen`. (Closes C3.1–C3.4; ticket RT-06.)
- **L-3 [P]** `begin()` reports failure: apps implement `bool beginOk()` (or the guard checks `_screen != nullptr` post-`begin()`); `load()` never passes a null screen to `lv_screen_load_anim`. (Closes C3.5; RT-05/RT-06.)
- **L-4 [P]** On `begin()` failure, the system returns to the menu and shows a visible error toast; the app remains launchable (retry allowed) and MUST have leaked nothing (L-2). (RT-05.)
- **L-5 [V]** After `end()`: all of the app's LVGL pointers are null; StatusBar pointers are null; no `lv_timer` owned by the app survives. (Current pattern; e.g. `GrapherApp.cpp:167-228`.) **[P]** extended: enforced by a debug-build walk that asserts the app contributed zero objects to the active tree after teardown. (RT-05.)
- **L-6 [P]** `end()` completes within **300 ms** of invocation (bounds the main-loop stall inside the teardown tick). Fractal's 1500 ms join budget must shrink to ≤250 ms with the force-kill retained. (Closes C3.7; RT-06.)
- **L-7 [P]** `KeyboardManager` state is `NONE` on every menu entry (`returnToMenu` resets the singleton). (Closes C3.9; RT-14.)
- **L-8 [V]** At most one non-menu app screen exists at any time (single `_pendingTeardownMode` slot + flush-before-launch, `SystemApp.cpp:735`). **[P]** assert in the lifecycle guard.

### D.2 LVGL screen/object invariants

- **G-1 [V]** LVGL allocation failure is always loud: assert handler prints file:line and aborts. (`lv_conf.h:153-164`.)
- **G-2 [P]** The launcher never *starts* an app when the pool is already ≥ **80 %** used (`lv_mem_monitor`): launch is refused with a visible message instead of aborting mid-build. (RT-07.)
- **G-3 [P]** Persistent baseline (menu + StatusBar machinery) ≤ **30 %** of the pool at menu idle; measured by the `[MEM] exit` probe's `lvgl=` field after any app exit. (Budget from allocation policy §3; RT-07 alarms on breach.)
- **G-4 [P]** Any per-item widget population (steps, table rows, lists) is capped or budget-gated the way `canAllocStep()` already does for Equations. (`EquationsApp.cpp:486-503` is the reference; extend to Calculation edu-steps and Calculus; MT-08/RT-07.)
- **G-5 [V]** The active screen is never the target of `lv_obj_delete` (splash guard `SplashScreen.cpp:128`; deferred teardown for apps). **[P]** generalized: a debug assert wraps screen deletion.

### D.3 Memory floor invariants

- **M-1 [P]** Internal-RAM largest free block ≥ **40 KB** at menu idle (the floor Equations already assumes, `EquationsApp.h:267-270`). Breach ⇒ `[MEM] LOW` warning + heavy-op refusal (RT-08).
- **M-2 [P]** PSRAM largest free block ≥ **512 KB** before starting any heavy op (Giac query, CAS solve, sim `begin()`, Python init). Breach ⇒ op refused with visible message. (Extends MT-13; RT-08/RT-09.)
- **M-3 [P]** Bulk blocks (≥4 KB) never take internal RAM as fallback (policy §1.4; MT-05 closes `SymExprArena`/`CasMemory` violations). Reliability spec depends on this: internal floor (M-1) is meaningless while C1.3 is open.
- **M-4 [P]** Leak canary: two consecutive `[MEM] exit mode=N` lines for the same app report equal `psram` free (±0). Already measurable post-MEM-A; becomes an *invariant* checked by the on-device soak procedure (§I-H4) and, for whitelisted apps, by the emulator ASAN job (MT-09).
- **M-5 [P]** loopTask stack high-water never drops below **8 KB** (`stack=` field, words×4). Breach logs `[MEM] STACK-LOW` and (in debug) aborts. (RT-08.)

### D.4 No-hang invariants

- **H-1 [P]** The task watchdog is armed and fed only from `loop()` (`main.cpp:241-269`): any single pass through `lv_timer_handler` + `g_app.update()` + key drain exceeding the TWDT budget (**8 s** production) resets the device *with a panic record*. No code may feed the WDT from inside a long computation to mask it. (RT-01.)
- **H-2 [P]** A Giac query is bounded: input ≤ **2048** chars, pre-flight M-2 check, and a cancellation deadline of **10 s** enforced via `giac::ctrl_c`/`interrupted` set from an `esp_timer` callback; expiry returns `Error: timeout` through the existing catch path. (Closes C4.1; RT-09.)
- **H-3 [P]** A single `replot()` pass is bounded by a wall-clock budget of **200 ms**; on expiry the pass aborts, draws what it has, and shows a "plot truncated" marker; implicit-grid and per-pixel loops check the budget every row. (Closes C8.2; RT-12.)
- **H-4 [P]** CAS solve/diff/integrate calls carry a cooperative deadline (default **5 s**) checked in `RuleEngine::applyToFixedPoint` steps and simplify passes; AC-matching refuses sums/products with > **8** terms (closes C4.4). (RT-09.)
- **H-5 [P]** Structural depth of any VPAM tree ≤ **MAX_EXPR_DEPTH = 16** enforced at the *editor* (CursorController insert refusal, mirroring Grapher's `MAX_NEST` pattern) — making evaluator/layout recursion depth bounded by construction; evaluator and layout additionally carry a defensive depth counter that error-outs instead of overflowing. (Closes C5.1/C5.2/C5.4; RT-10/RT-11.)
- **H-6 [V]** RPN evaluation is iterative with a fixed 64-slot stack (`Evaluator.h:52`) — never convert to recursion.

### D.5 Recovery invariants

- **R-1 [P]** Every abnormal reset leaves a retrievable record: reset reason, active app id, uptime, boot-session id, and the last `[MEM]` snapshot, stored in `RTC_NOINIT` memory and replayed as `[PANIC]` lines on the next boot. (RT-02.)
- **R-2 [P]** **3** abnormal resets in a row, each with uptime < **60 s**, ⇒ next boot enters safe mode (see `NUMOS_SAFE_MODE_AND_RECOVERY_SPEC.md`). A stable session (≥60 s) clears the counter. (RT-03.)
- **R-3 [P]** User data is never destroyed without an explicit user action. `LittleFS.begin(true)` auto-format is replaced by `begin(false)` + quarantine + safe-mode prompt; corrupt `/vars.dat` is renamed `vars.bad`, never overwritten silently. (Closes C6.4; RT-13.)
- **R-4 [P]** The serial diagnostic console (`!` commands) is available within **10 s** of any boot, including safe mode and including LVGL-init failure (the eternal-spin paths at `main.cpp:145-159` become "log + minimal serial loop + reboot after 30 s"). (RT-04.)
- **R-5 [P]** Safe mode never loads app persistence (no `/vars.dat`, no autoloads at `NeoLanguageApp.cpp:116`, `CircuitCoreApp` autoLoad), never initializes Giac, and only exposes the safe-app whitelist. (RT-03.)

---

## E. Proposed fault-containment architecture

Six mechanisms, ordered by dependency. All are **[P]** unless noted.

### E.1 Watchdog policy (RT-01)

- Explicitly configure the task WDT instead of trusting unverified framework defaults: `esp_task_wdt_init(8, true)` + `esp_task_wdt_add(NULL)` in `setup()` after display init (`main.cpp` ~`:160`), `esp_task_wdt_reset()` exactly once per `loop()` iteration (`main.cpp:241`).
- Rationale for 8 s: the longest *legitimate* synchronous operations today are a worst-case implicit replot (sub-second on hardware for typical input, unbounded for adversarial — bounded to 200 ms by H-3) and CAS solves (bounded to 5 s by H-4). 8 s leaves margin while still converting genuine hangs into recorded resets.
- The **only** feed point is `loop()`. Long operations must *finish or be cancelled* within budget; they may not feed the dog themselves (that would reclassify hangs as normal).
- Interrupt WDT and brownout detector: characterize the actual `qio_opi` defaults on the build host (`sdkconfig.h`; unverifiable in this sandbox — boot audit §2) and record them in this spec's follow-up; do not override until measured.
- Fractal's render task registers with the TWDT only if it gains long uninterrupted loops; today its cooperative-exit flags suffice (`FractalApp.cpp:513-540`).

### E.2 Panic record + boot accounting (RT-02; formats in the diagnostics spec)

- `RTC_NOINIT_ATTR numos::CrashRecord g_crash` — magic, boot-session id, boot counter, consecutive-abnormal counter, last reset reason, last app id (`SystemApp::_mode` mirrored on every `launchApp`/`returnToMenu`), uptime at death (coarse, updated each heartbeat), last MEM snapshot (5×u32).
- Written: continuously (cheap field updates from the heartbeat), *not* from the panic handler — RTC RAM survives the panic without any panic-context code. Validated by magic + checksum on boot; power-on reset re-initializes.
- On boot: `esp_reset_reason()` + record replay as `[PANIC]`/`[BOOT]` lines *before* display init, so even display-dead devices report over serial.

### E.3 App lifecycle guard (RT-05) — the state machine SystemApp lacks

- Formalize the existing three variables into an explicit enum: `IDLE → LAUNCHING → ACTIVE → PENDING_TEARDOWN → TEARING_DOWN → IDLE`, owned by SystemApp; illegal transitions log `[APP] state-violation` and (debug) abort.
- `launchApp` becomes: flush pending teardown (existing `:735`) → **G-2 pool gate** → `_app->load()` → **post-load verification**: `_app` screen non-null and `lv_screen_active()` is the app's screen. Verification failure ⇒ `end()` the app (safe by L-2), `[APP] begin-fail id=N`, toast on the menu, state back to `IDLE`.
- `injectKey`/`update` consult the state: keys are dropped (with a rate-limited `[INPUT] drop` log) during `LAUNCHING`/`TEARING_DOWN`.
- This guard is the single place where per-launch MEM probes, pool gates, and failure toasts live — apps stay dumb.

### E.4 Safe teardown & OOM-safe begin (RT-06)

- Per the allocation policy §4.3 (already normative): extract `freeBuffers()` in Fluid2D/Bridge/NeuralLab/CircuitCore — null-safe, idempotent, called unconditionally from `end()` and from every `begin()` failure exit. `load()` in all 20 apps re-checks `_screen` after `begin()` (copy the OpticsLab pattern, `OpticsLabApp.cpp:158-163`).
- Fractal: handle `_orbit == nullptr` (`FractalApp.cpp:96-97`) by disabling deep-zoom mode instead of ignoring; reduce `stopRenderTask` join budget to 250 ms (L-6).
- Add the `end()`-twice and begin-fail-then-end tests to the emulator suite for whitelisted apps; firmware-only apps get the failure-injection harness (§I).

### E.5 Memory low-water handling (RT-08, building on MEM-A probes)

- `numos::memGuard(op)` — one header, checked before heavy ops: Giac query (M-2), CAS solve, sim `begin()`, Python `init()`, step-render expansion (reuses `canAllocStep` thresholds), app launch (G-2).
- Thresholds (initial; ratchet with MT-01 field data): SPIRAM largest < 512 KB ⇒ refuse heavy ops; internal largest < 40 KB ⇒ refuse + `[MEM] LOW int` warning; internal largest < 20 KB ⇒ escalate: force return-to-menu, flush teardown, log `[MEM] CRITICAL`; LVGL pool > 80 % ⇒ refuse app launch / step expansion.
- StatusBar surface: a small "▲MEM" indicator when any floor is breached (pull-based, set from the heartbeat; StatusBar already has a modifier slot pattern, `StatusBar.cpp:46-146`).

### E.6 Safe mode, diagnostics screen, serial protocol

Specified in full in the companion documents:
- **Safe mode** (`NUMOS_SAFE_MODE_AND_RECOVERY_SPEC.md`): boot-loop detection per R-2, minimal launcher (Calculation, Settings, Diagnostics), no persistence loads, no Giac init, explicit user-consent reset actions replacing all auto-format paths.
- **Diagnostic screen** (safe-mode app + normal-mode entry): boot/reset/panic record, live MEM stats, FS status, input echo — the hardware bring-up screen (RT-15).
- **Serial diagnostic protocol** (`NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md`): `!`-prefixed commands (`!mem`, `!panic`, `!fs`, `!vars`, `!safemode`, `!reboot`, …). The `!` prefix is currently unmapped in `SerialBridge::processChar` (`SerialBridge.cpp:108-241` — verified no `'!'` case), so the grammar extension is append-only.

### E.7 Panic behavior (consolidated)

On any `abort()`/panic (LVGL assert `lv_conf.h:161`, uncaught `bad_alloc`, TWDT, LoadProhibited…): the IDF panic handler prints a decodable backtrace (`platformio.ini:19`) and reboots (default `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT` — **verify on hardware**, C9.2). Our additions: the RTC crash record already holds app/uptime/mem context (E.2); next boot replays it and counts toward safe mode (R-2). We deliberately do **not** hook `esp_register_shutdown_handler` for flash writes — no flash I/O in panic context (§H.6).

---

## F. Per-subsystem failure contracts

Format: *trigger → required behavior (user-visible / serial / state)*. **[V]** parts exist; unmarked = proposed.

### F.1 Calculation

| Trigger | Contract |
|---|---|
| Evaluation error (domain, ÷0, overflow→error) | **[V]** "Math ERROR" result via `ExactVal::makeError` (`MathEvaluator.cpp:175-178`); editor state preserved; no log needed |
| Depth > MAX_EXPR_DEPTH during edit | insert refused silently (cursor stays), same UX as Grapher's `MAX_NEST` today (`GrapherApp.cpp:2580-2593`); no crash possible by construction (H-5) |
| Edu-steps arena/pool exhaustion | steps truncated with "… (more steps omitted)" tail; `[CAS] steps-cap` log; result itself always shown; arena reset on exit **[V]** (`CalculationApp.cpp:114-122`) |
| History growth | **[V]** 50-entry cap (`CalculationApp.h:138`); per-entry node cap added with the depth guard (C5.3) |
| `begin()` LVGL failure | L-3/L-4: back to menu + toast; retry allowed |

### F.2 Grapher

| Trigger | Contract |
|---|---|
| Invalid expression | **[V]** slot `valid=false`, not plotted (`GraphModel.cpp:125-208`); no error dialog (deliberate) |
| NaN/asymptote | **[V]** segment dropped (`GrapherApp.cpp:1656-1679`) |
| PSRAM buffer alloc fail | **[V]** red "ERR: INSUFFICIENT PSRAM" label, app still navigable (`GrapherApp.cpp:578-587`); + `[GRAPH] buf-fail` log; stale-descriptor fields at `:593-594` cleaned (RT-12) |
| Replot budget exceeded (H-3) | partial plot + "⚠ plot truncated" info-bar text + `[GRAPH] replot … aborted=1` log; next keystroke retries |
| Analysis (root/extremum/∫) exceeds budget | operation cancelled, "no result (timeout)" in the calc panel, `[GRAPH] calc timeout` log |
| Serialization truncation at 63 chars (C7.3) | refuse to mark the slot valid; show "expression too long" instead of silently plotting a truncated function |
| Exit during trace/modal | **[V]** teardown discards state safely (all modal objects are screen children); timers deleted in `end()` (`GrapherApp.cpp:169-189`) |

### F.3 CAS (in-tree) / Giac

| Trigger | Contract |
|---|---|
| Giac query > 2048 chars | refused: `Error: input too long` (bridge-level, before `gen`) |
| PSRAM largest < 512 KB pre-flight | refused: `Error: low memory`; `[GIAC] refused reason=mem` |
| Deadline (10 s) expiry | `giac::ctrl_c` set from esp_timer → Giac throws → **[V]** caught (`GiacBridge.cpp:441-447`) → `Error: timeout`; `[GIAC] … status=timeout`; ctrl_c cleared; context remains usable (verify on hardware; if the global context is poisoned after interrupt, the contract escalates to "Giac disabled until reboot" + log) |
| Giac `bad_alloc`/any exception | **[V]** `Error: <what>` string (`GiacBridge.cpp:441-447`); post probe fires **[V]** `:442,445` |
| In-tree solve exceeds 5 s cooperative deadline | solver returns error step "computation too complex"; partial steps shown; arena reset on next solve **[V]** (`EquationsApp.cpp:2176`) |
| AC-match > 8 terms | rule skipped (not the whole solve); `[CAS] ac-skip terms=N` debug log |
| Arena exhaustion mid-solve | nullptr propagates (fix the three unchecked clone chains, `SymExpr.cpp:149,327,331`); solver aborts with "out of memory" step; no partial-null tree may reach `SymExprToAST` |

### F.4 Renderer (MathRenderer / MathAST)

| Trigger | Contract |
|---|---|
| Draw depth > 12 | **[V]** red box + "…" (`MathRenderer.cpp:1507-1514`) |
| Layout depth > 16 (defensive counter, H-5) | layout returns a fixed-size "⚠" box for the offending subtree; `[LVGL] layout-depth` log; never recurses past the cap |
| MathCanvas creation fails (pool) | **[V]** global assert-abort today (G-1); after G-2/G-4 gating this is treated as a bug, not a handled state — the gates must make it unreachable |
| Missing glyph | **[V]** tofu/vector/skip fallback chain (`MathRenderer.cpp:184-276`); no crash |
| Cursor-blink timer | **[V]** deleted on destroy/non-editable (`MathRenderer.cpp:564,585-586,673-678`) |

### F.5 MainMenu

| Trigger | Contract |
|---|---|
| Card/grid build alloc failure | today unhandled (C2.3); after G-1 it aborts loudly; menu build happens once at boot when the pool is empty — treated as boot failure, not runtime containment |
| Focus loss on return | **[V]** re-binds group + snaps to first card (`SystemApp.cpp:681-683`; `MainMenu.cpp:159-197`) |
| Launch refused (pool gate G-2 / begin-fail L-4) | toast label on the menu screen ("Cannot open: memory low" / "App failed to start"), auto-hides 2 s, `[APP]` log line |

### F.6 Settings

| Trigger | Contract |
|---|---|
| Any failure | Settings mutates three globals only (`SettingsApp.cpp:244-305`); no persistence **[V]** (C6.7); nothing to contain beyond L-2/L-3 boilerplate. When settings persistence is added (future), it must follow the atomic-write rule (R-3/C6.3) |

### F.7 Firmware-only apps (sims, Python, NeoLang, Circuit, PeriodicTable, Fractal)

| Trigger | Contract |
|---|---|
| Any buffer alloc failure in `begin()` | full rollback (L-2 `freeBuffers()`), `[APP] begin-fail id=N stage=<buf>`, menu toast (L-4). Reference implementation: ParticleLab (`ParticleLabApp.cpp:112-115`) generalized |
| Corrupt save file | validated on load (magic/version/size/content clamps per C6.5 fix); invalid ⇒ ignore file + start fresh + `[FS] <file> corrupt action=ignored`; file renamed `*.bad` (kept for diagnosis, one generation) |
| Sim step NaN/instability | app-local clamp (existing sims clamp internally); never propagates outside the app screen |
| Background task (Fractal) | stop-join ≤250 ms then force-kill **[V pattern]** (`FractalApp.cpp:513-540`, budget tightened) |
| Python/NeoLang interpreter runaway | interpreters are line-stepped in `update()` (bounded per frame) — verify per-frame statement budget when RT-09 lands; NeoArena soft-reset at 70 % **[V]** (`NeoLanguageApp.cpp:620-631`) |

---

## G. Required behavior on each failure (visible / serial / cleanup / persistence / reboot)

The matrix below is the normative "what must happen", cross-referencing §C classes. "Toast" = menu-level transient label (E.3). Log formats are exact in the diagnostics spec.

| Failure | User sees | Serial | State cleanup | Persistence | Reboot/safe-mode |
|---|---|---|---|---|---|
| LVGL pool OOM (C1.1) | device reboots to menu (< 3 s); after R-2 threshold: safe mode | `[LVGL] ASSERT FAILED …` **[V]** + backtrace; next boot `[PANIC] reason=abort app=N` | none possible (abort) | untouched — no flash writes in panic | counts toward boot-loop |
| Launch refused, pool ≥80 % (G-2) | toast "Memory low — close and retry" | `[APP] launch-refused id=N lvgl=NN%` | menu unchanged | untouched | no |
| App `begin()` failure (C3.x) | toast "App failed to start"; menu responsive | `[APP] begin-fail id=N stage=S` + `[MEM]` probe | L-2 rollback; state `IDLE` | untouched | no |
| Uncaught `bad_alloc` (C1.5) | reboot | panic + next-boot `[PANIC]` | — | untouched | counts |
| Giac timeout (H-2) | serial user gets `Error: timeout` (UI unaffected — Giac is serial-only today, C4.1) | `[GIAC] … status=timeout ms=10000` | ctrl_c cleared; bridge re-armed | untouched | no |
| Giac refused (mem/length) | `Error: low memory` / `Error: input too long` | `[GIAC] refused reason=…` | none | untouched | no |
| CAS solve deadline (H-4) | "computation too complex" step in the steps panel | `[CAS] solve … status=deadline` | arena reset at next solve **[V]** | untouched | no |
| Replot budget hit (H-3) | partial plot + "⚠ plot truncated" | `[GRAPH] replot ms=200 aborted=1` | `_plotDirty` stays clear; next key retries | untouched | no |
| Deep-nesting insert refused (H-5) | keystroke ignored (cursor unchanged) | none (debug: `[INPUT] depth-cap`) | none | untouched | no |
| `/vars.dat` corrupt (C6.1/2) | variables come up empty; StatusBar "▲" once safe-mode spec lands; Diagnostics shows quarantine | `[FS] vars=corrupt action=quarantine file=vars.bad` | in-RAM vars zeroed **[V]** (`resetAll`, `VariableManager.cpp:112-116`) | original preserved as `vars.bad`; fresh file written on next store | no |
| LittleFS mount fail (C6.4) | normal UI, persistence disabled banner in Settings/Diagnostics | `[FS] mount=fail action=disabled` | all persistence no-ops | **never auto-format** (R-3); format offered only in safe mode with confirm | no |
| ParticleLab corrupt save (C6.5) | sandbox starts empty | `[FS] save.pt=invalid action=ignored` | grid cleared | file renamed `.bad` | no |
| Input queue overflow (C7.1) | (unchanged UX) | rate-limited `[INPUT] drop q=<name> n=<count>` | none | — | no |
| Watchdog reset (H-1) | reboot; menu in < 3 s | next boot: `[PANIC] reason=twdt app=N uptime=…` + last `[MEM]` snapshot | — | untouched | counts |
| Brownout | reboot (hardware) | next boot `[PANIC] reason=brownout` | — | possible torn write → C6.3 handling | counts |
| Boot-loop (R-2) | SAFE MODE screen | `[SAFE] enter cause=bootloop count=3` | minimal init only | nothing loaded (R-5) | safe mode |
| Draw-buffer fail (C9.3) | black screen; **reboots after 30 s** instead of eternal spin | `[BOOT] BUFFER FAIL` **[V]** + `[PANIC]`-style record + serial console alive (R-4) | — | untouched | counts (⇒ safe mode can't help a hardware fault, but the record identifies it) |

---

## H. Design alternatives rejected

1. **Per-app FreeRTOS task isolation** (each app on its own task/stack, kill on fault). Rejected: LVGL 9 is single-threaded by design; every app's UI work must serialize onto the LVGL task anyway, so isolation buys only stack separation at the cost of a full concurrency model (locks around every `lv_*` call), +N×stack RAM from a 512 KB budget, and a rewrite of all 20 apps. The failure classes that kill us (pool OOM, layout recursion, storage corruption) are not task-boundary faults.
2. **C++ exceptions as the app fault boundary** (wrap `load()/update()/handleKey()` in try/catch). Rejected as *primary* mechanism: the dominant faults are hard faults (null-deref, stack overflow) that exceptions cannot catch, and LVGL/app code is not exception-safe (RAII coverage is partial) — unwinding mid-widget-build leaks worse than aborting. Retained *narrowly*: the existing catch-all around Giac (`GiacBridge.cpp:441-447`), which is exactly the boundary where a well-behaved exception source (Giac throws by design) meets non-exception-safe code.
3. **Feeding the watchdog inside long loops** (`yield()`-style, as `GrapherApp.cpp:2335` does today). Rejected: it converts "hang detected" into "hang tolerated". The spec's direction is the opposite — budgets + cancellation (H-2/H-3/H-4), with the WDT fed only by a healthy `loop()`.
4. **Moving the LVGL heap to PSRAM** to make pool OOM "impossible". Rejected: render-latency regression risk, the historical PSRAM/DMA bug class (`main.cpp:131-137`), and allocation policy §3.5 already forbids it without hardware validation. Pool sizing (≤96 KB) + gates (G-2/G-4) achieve the goal within the internal budget.
5. **Running Giac on a dedicated task with kill-on-timeout.** Rejected for now: Giac's global context is not re-entrant and not designed to be destroyed/rebuilt cheaply (`GiacBridge.cpp:44,192-211`); killing a task mid-allocation leaks arbitrary PSRAM and may corrupt the context, forcing a reboot anyway. The `ctrl_c` cooperative interrupt (H-2) achieves cancellation within Giac's own design. Revisit only if hardware testing shows `ctrl_c` leaves the context unusable.
6. **Writing panic records to flash (NVS/LittleFS) from the panic handler.** Rejected: flash writes from panic context risk filesystem corruption — the exact failure we're trying to survive. `RTC_NOINIT` memory is free, survives all resets except power-on, and needs no panic-context code at all (E.2). Cost: records lost on power-cycle — acceptable; boot-loop detection only matters across warm resets.
7. **Full ESP-IDF core dumps to flash.** Rejected for the default build: costs a flash partition + config surface on an unverified framework config, and the single-maintainer workflow gets more from the decoded backtrace + RTC record. Optional for validate builds later.
8. **Auto-format on mount failure (status quo).** Rejected and reversed (R-3): a transient mount failure (brownout mid-write) currently destroys all user data. Quarantine + explicit consent replaces it. Cost: a device with a genuinely corrupt FS boots with persistence disabled until the user acts — strictly better than silent wipe.
9. **CRC-protecting every file format** including sims. Trimmed: only `/vars.dat` (v2, CRC32+version) and ParticleLab's grid (type-clamp on load is sufficient — a CRC would reject files we can cheaply sanitize). Text formats (Python/NeoLang/Circuit CSV) already fail soft (storage audit §4) and get size caps only.
10. **A general "app crash → restart app" supervisor.** Rejected: without task isolation (rejected in 1), an in-app hard fault takes the whole firmware; the honest unit of restart is the device (fast, recorded, ≤3 s to menu). Containment budget goes to *preventing* the fault classes that are preventable (allocation, recursion, input, storage) instead.

---

## I. Acceptance tests

Emulator tests (E-x) run headless deterministic (`--headless --deterministic`); hardware tests (H-x) are serial-verified procedures on the N16R8. Failure-injection hooks are defined per ticket (RT-xx). CI placement in the tickets doc; goldens/masks untouched.

### I-E: Emulator (automatable now or with the listed ticket)

| ID | Test | Pass criterion | Ticket |
|---|---|---|---|
| I-E1 | Phase 9F suite (existing 6 scripts) | unchanged green — regression floor for L-1 | — **[V]** |
| I-E2 | `end()` idempotence: for each whitelisted app, script opens app → home → wait 300 frames → open → home; ASAN build | no UAF/leak reports (menu/font suppressions applied) | MT-09/RT-06 |
| I-E3 | Begin-fail containment: build with `-DNUMOS_FAULT_INJECT_BEGIN=<appid>` forcing the app's buffer alloc to fail | app returns to menu, toast rendered, `[APP] begin-fail` on stdout, relaunch after clearing flag works; exit code 0 | RT-05/RT-06 |
| I-E4 | Depth guard: script holds DIV/POW inserts 40× in Calculation | inserts stop at MAX_EXPR_DEPTH; `assert_no_error`; no crash; result still evaluable | RT-10 |
| I-E5 | Replot budget: `-DNUMOS_FAULT_INJECT_SLOW_EVAL=1` (per-eval delay) + implicit plot script | emulator exits 0 within timeout; `[GRAPH] … aborted=1` present; UI keys still processed next frame | RT-12 |
| I-E6 | Corrupt `vars.dat`: script harness pre-writes a magic-valid/garbage-body file into `emulator_data/` | boot succeeds; `[FS] vars=corrupt action=quarantine`; `vars.bad` exists; `assert_variable` sees empty store | RT-13 |
| I-E7 | Truncated `vars.dat` | load reports failure (not success); same quarantine path | RT-13 |
| I-E8 | Safe-mode simulation: `--boot-count 3` CLI (emulator analogue of RTC counter) | safe-mode screen renders; only whitelisted cards; `[SAFE] enter cause=bootloop` printed; exit-safe-mode action reboots emulator loop into normal menu | RT-03/RT-16 |
| I-E9 | Serial diag protocol: script pipes `!mem`, `!panic`, `!fs`, `!help` | each returns its documented format; unknown `!x` returns `ERR unknown`; exit 0 | RT-04 |
| I-E10 | Input storm: script sends 200 keys in 10 frames | rate-limited `[INPUT] drop` lines appear; no assertion failures; final state consistent | RT-04/RT-14 |
| I-E11 | Modifier reset: script sets ALPHA-LOCK in Calculation, exits to menu, opens Grapher, presses a digit | digit interpreted as digit (not alpha) — L-7 | RT-14 |
| I-E12 | LVGL OOM visibility (existing MEM-A check): scratch build `-DLV_MEM_SIZE=16384` | boot prints `[LVGL] ASSERT FAILED` and aborts (no timeout-hang) | — **[V]** |

### I-H: Hardware (serial procedures; human or HIL rig)

| ID | Test | Pass criterion | Ticket |
|---|---|---|---|
| I-H1 | WDT characterization: flash a scratch build with `while(1);` injected in `loop()` | device resets within budget; next boot prints `[PANIC] reason=twdt` with correct app id/uptime | RT-01/RT-02 |
| I-H2 | Panic record: force LVGL OOM (`-DLV_MEM_SIZE=16384`) | reboot ≤3 s; `[PANIC] reason=abort` replayed; counter incremented | RT-02 |
| I-H3 | Boot-loop → safe mode: leave the OOM build in place for 3 boots | third boot enters safe mode; SAFE banner on screen; serial `[SAFE] enter cause=bootloop count=3`; normal build then boots normally and clears counter after 60 s | RT-03 |
| I-H4 | Leak canary soak: open/close every app twice; compare `[MEM] exit` psram values | equal per app (M-4); any delta = named leak | — (procedure exists post-MEM-A) |
| I-H5 | Giac timeout: `:factor(10^60+7)` (or measured-slow input) over serial | `Error: timeout` in ≤10.5 s; UI alive throughout (heartbeat lines uninterrupted); follow-up `:1+1` returns `2` (context healthy) | RT-09 |
| I-H6 | Giac pre-flight: scratch allocation pinning PSRAM below 512 KB, then any `:` query | `Error: low memory`; no evaluation attempted (`[GIAC] refused`) | RT-09 |
| I-H7 | Draw-buffer failure path: scratch build forcing `heap_caps_malloc` at `main.cpp:142` to fail | serial prints failure + console answers `!mem`, device self-reboots after 30 s (R-4) | RT-04 |
| I-H8 | Storage power-loss: cut power mid `→VAR` store loop (scripted PSU) | next boot: no format; either old or quarantined vars; `[FS]` line states which | RT-13 |
| I-H9 | Brownout: sag supply to trip detector | device resets; next boot `[PANIC] reason=brownout`; counts toward safe mode | RT-01/RT-02 |
| I-H10 | Replot budget on hardware: adversarial implicit inequality from serial keys | UI regains responsiveness ≤300 ms after each keystroke; `[GRAPH] aborted=1` lines present; no WDT reset | RT-12 |

---

*Top-10 lists, ticket ordering, and per-ticket rollback plans: `NUMOS_RELIABILITY_IMPLEMENTATION_TICKETS.md`.*
