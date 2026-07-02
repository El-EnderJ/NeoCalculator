# NumOS — Reliability Implementation Tickets (RT-01 … RT-16)

> Execution backlog for `NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md` (invariants D.x), `NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md` (log/console formats), and `NUMOS_SAFE_MODE_AND_RECOVERY_SPEC.md`. Ordered by **risk reduction per unit effort** — the order below is the recommended execution order. Verified against `63e7408`; all cited line numbers are current.
>
> **Global constraints for every ticket** (repo fragile-file rules): never move the LVGL draw buffer to PSRAM or double-buffer it (`main.cpp:131-143`); never `lv_obj_delete` app screens synchronously on exit (`SystemApp.cpp:238-248`); goldens/masks/CI workflows untouched except where a ticket explicitly says otherwise (and then additive-only, owner-reviewed); renderer geometry must not change (golden invalidation); `.numos` grammar and SerialBridge key map are append-only; KeyCode enum append-only; `lib/giac/**` stays vendored-frozen — Giac behavior changes happen in `GiacBridge.cpp` only.
>
> Owner legend: **Sonnet** = mechanical, compile/CI-verifiable; **Opus** = judgment across call sites / lifecycle semantics; **Fable** = cross-cutting design risk or adversarial surfaces; **Human** = hardware-in-the-loop or product decisions. Every hardware-touching ticket ships with its HIL step explicitly listed — models must not claim those verified.

---

## RT-01 — Arm and own the task watchdog  *(P0)*

- **Motivation:** Nothing in `src/` arms or feeds any WDT (verified; only a lone `yield()` at `GrapherApp.cpp:2335`); framework defaults are unverified (no local sdkconfig — memory-audit-style proof impossible in-repo). Hangs (`solveWithGiac` C4.1, adversarial replot C8.2) currently freeze the device forever. Invariant **H-1**.
- **Touched files:** `src/main.cpp` (`setup()` after `:164`; `loop()` `:241`), `platformio.ini` (no flag changes expected; document measured defaults in a comment), new `docs/specs/` note or appendix recording the measured `qio_opi` sdkconfig values (`CONFIG_ESP_TASK_WDT*`, `CONFIG_ESP_INT_WDT*`, `CONFIG_ESP_SYSTEM_PANIC`, `CONFIG_ESP_BROWNOUT_DET`) from the real build host.
- **Implementation outline:** (1) On the build host, read `framework-arduinoespressif32/tools/sdk/esp32s3/qio_opi/include/sdkconfig.h` and record the actual defaults — this closes fault-spec C9.2. (2) `esp_task_wdt_init(8, true); esp_task_wdt_add(NULL);` in `setup()`; `esp_task_wdt_reset()` as the first statement of `loop()`. (3) Explicit policy comment: the WDT is fed **only** here; long ops must finish or cancel (H-2/H-3/H-4), never self-feed. (4) Leave the interrupt WDT/brownout at measured defaults.
- **Invariants enforced:** H-1; enables R-1 to fire for hangs.
- **Acceptance tests:** firmware builds; boots; 10-minute idle + app-cycling session with zero WDT resets (heartbeat continuity proves feeding).
- **Emulator tests:** none (no TWDT natively); emulator build must remain untouched (`esp_task_wdt*` under `#ifdef ARDUINO`).
- **Hardware tests:** I-H1 — scratch build with `while(1);` in `loop()` resets in ≈8 s; after RT-02, reset reason reads `task_wdt`.
- **Failure injection:** `-DNUMOS_FAULT_INJECT_HANG=1` compiles a `!hang` DiagConsole command (post-RT-04) or a temporary key mapping that enters `while(1);`.
- **Risk:** medium — if any *legitimate* path exceeds 8 s today (worst adversarial implicit replot on hardware is unmeasured), users get resets where they had freezes. Mitigation: this is strictly better (freeze→recorded reboot), and RT-12/RT-09 shrink the offenders; start at 8 s, tune with `[GRAPH]/[GIAC]` timing data.
- **Rollback:** remove the three calls; behavior returns to today's unverified defaults.
- **Difficulty:** S.
- **Owner:** **Sonnet** to write; **Human** must run I-H1 on hardware before merge (WDT behavior is HIL-only).

## RT-02 — RTC crash record, boot classification, `[BOOT]`/`[PANIC]` lines  *(P0)*

- **Motivation:** Crashes leave no trace (no `esp_reset_reason`, no RTC state, no counters — verified absent). Everything downstream (safe mode, field debugging) needs this. Invariants **R-1**, prerequisite for **R-2**.
- **Touched files:** new `src/utils/CrashRecord.h/.cpp` (struct per safe-mode spec §2.1); `src/main.cpp` (classification + `[BOOT]`/`[PANIC]` emit before display init, around `:99`; heartbeat refresh at `:256-261`); `src/SystemApp.cpp` (`launchApp` `:727` / `returnToMenu` `:860` mirror `lastMode`; STABLE latch in `update()`).
- **Implementation outline:** `RTC_NOINIT_ATTR CrashRecord g_crash;` + CRC validation; classification table per safe-mode spec §2.1; session id via `esp_random()`; emit formats per diagnostics spec §2.2/§2.11. No flash writes, no panic-context code. Native build: struct lives in a plain global, classification driven by RT-16 CLI flags (compiled but inert by default).
- **Invariants enforced:** R-1; feeds R-2.
- **Acceptance tests:** firmware builds; boot log shows `[BOOT] session=… reset=poweron boots=1 abnormal=0`; sw-reset (`!reboot` post-RT-04 or re-flash) shows `reset=sw` and increments `boots`.
- **Emulator tests:** unit-style host test for the classification function (pure logic — table in, decision out), compiled like `tests/host/keycode_digit_test.cpp`; emulator boots unchanged.
- **Hardware tests:** I-H2 (panic via `-DLV_MEM_SIZE=16384` scratch build → next boot replays `[PANIC] reason=abort app=… `); I-H9 (brownout via PSU sag → `reason=brownout`).
- **Failure injection:** scratch LVGL-OOM build (already the MEM-A verification vehicle); `abort()` behind a debug command.
- **Risk:** low — additive; RTC struct layout must be treated as append-only once shipped (versioned by magic).
- **Rollback:** delete the two emit blocks + mirror calls; header removal.
- **Difficulty:** S-M.
- **Owner:** **Sonnet** (classification logic host-testable); **Human** confirms reset-reason mapping on device once.

## RT-03 — Boot-loop detection → safe mode  *(P0, depends RT-02)*

- **Motivation:** Persistent-state crashes are permanent boot loops today (auto-loads at `SystemApp.cpp:144-156`, `NeoLanguageApp.cpp:116`, `CircuitCoreApp.cpp:1712-1725`; no escape). Invariants **R-2**, **R-5**.
- **Touched files:** `src/main.cpp` (safe flag consult; splash skip `:173-186`); `src/SystemApp.cpp` (`begin()` skips per safe-mode spec §3; menu banner); `src/ui/MainMenu.h/.cpp` (card filter + banner label — additive params, no geometry change to existing cards); `src/apps/CalculationApp.cpp` (`executeStore` save gate `:894-901`); `src/input/SerialBridge.cpp` (Giac console refusal in safe mode, `:149-160`).
- **Implementation outline:** exactly safe-mode spec §2–§7: trigger table, skip list, whitelist {0, 10, Diagnostics}, banner, exit action, serial-only floor mode (§7.3) sharing the DiagConsole loop with the `main.cpp:145-159` failure paths (converted from eternal spin to "console alive + reboot after 30 s").
- **Invariants enforced:** R-2, R-4 (floor mode), R-5.
- **Acceptance tests:** normal boot unaffected (full CI); `[BOOT] safe=0` in normal runs.
- **Emulator tests:** I-E8 (`--boot-count 3` → safe menu, whitelist asserted via `assert_safe_mode` + `assert_menu_focus`; deterministic screenshot as a **new** golden candidate — additive stem, owner-blessed).
- **Hardware tests:** I-H3 (three real crash boots → safe mode; stable session clears); negative test (2 crashes + stable + 2 crashes ≠ safe mode).
- **Failure injection:** the RT-02 scratch OOM build ×3 boots; `!safemode` command.
- **Risk:** medium — touches boot order and MainMenu build; the splash-skip must keep the screen-load sequencing valid (menu becomes the first loaded screen; splash-destroy pump at `main.cpp:208-215` must no-op cleanly — `SplashScreen::destroy()` is already null-safe, `SplashScreen.cpp:120`).
- **Rollback:** feature-flag `NUMOS_SAFE_MODE_ENABLE=0` compiles the consult out; trigger data still recorded by RT-02.
- **Difficulty:** M.
- **Owner:** **Fable/Opus** (boot-order sensitivity), HIL step **Human**.

## RT-04 — Serial diagnostic console (`!` commands) + log ring buffer  *(P0)*

- **Motivation:** No diagnostic surface beyond ad-hoc prints; recovery actions need a UI-independent lever (serial is currently the only input at all — `Keyboard.h:82`). Invariant **R-4**; diagnostics spec §6/§7.
- **Touched files:** new `src/utils/DiagConsole.h/.cpp`; `src/input/SerialBridge.cpp` (branch on leading `!` in the line handler `:127-171` — append-only; `:` and key map untouched); new ring-buffer mirror in the log emit path; `src/main.cpp` (floor-mode console loop shared with RT-03).
- **Implementation outline:** command table per diagnostics spec §6.2 (v1: help/ver/mem/uptime/app/panic[/clear]/fs/vars[/reset]/log/safemode/reboot/selftest); destructive ops require literal `CONFIRM`; all replies `[DIAG]`-tagged, fixed-buffer `snprintf`, zero heap. Ring buffer: 8 KB PSRAM, memcpy mirror, `!log` dump (spec §7).
- **Invariants enforced:** R-4; enables I-E9/I-H5-7 test plumbing.
- **Acceptance tests:** firmware builds; `!help`/`!mem`/`!ver` answer documented formats; unknown → `ERR unknown`; `!vars reset` without CONFIRM refuses.
- **Emulator tests:** I-E9 via the RT-16 `diag <cmd>` script command (SerialBridge isn't compiled natively — `platformio.ini:195-266`); host unit test for the command parser (pure string→enum logic).
- **Hardware tests:** full command sweep over the monitor; `!log` returns recent heartbeat lines.
- **Failure injection:** n/a (this ticket *is* the injection/observation channel: later tickets add `!hang`, fault flags).
- **Risk:** low — SerialBridge additions are append-only; parser is host-testable. Watch: `!` lines must not echo into key injection (early branch before `:174`).
- **Rollback:** remove the `!` branch; DiagConsole header deleted; ring mirror no-op.
- **Difficulty:** S-M.
- **Owner:** **Sonnet**.

## RT-05 — App lifecycle guard (explicit state machine + launch verification)  *(P1)*

- **Motivation:** Lifecycle state is three loose members (`SystemApp.h:176,184-185`); `begin()` cannot report failure (all `void`); 17/20 `load()`s deref a possibly-null `_screen` (fault spec C3.5); no post-launch verification exists. Invariants **L-3, L-4, L-8, D.2 G-2 hook**.
- **Touched files:** `src/SystemApp.h/.cpp` (state enum; `launchApp` `:727-855` gains pool gate + post-load verification + failure path; `update()` `:238-304` transition assertions; `injectKey` drop during transitions); `src/ui/MainMenu.cpp` (toast label helper — additive object, hidden by default so menu goldens are unaffected in idle state).
- **Implementation outline:** enum `IDLE/LAUNCHING/ACTIVE/PENDING_TEARDOWN/TEARING_DOWN`; map existing code onto it (no behavior change on the happy path — the 250 ms constant and flush-before-launch at `:735` stay byte-identical in effect); after `_app->load()`: verify `lv_screen_active()` belongs to the app (each app exposes `lv_obj_t* screen() const` — trivial accessor, 20 one-liners) — on failure: `end()` (safe by RT-06), `[APP] begin-fail`, toast, state `IDLE`. Emit `[APP] enter/exit/launch-refused/state-violation` per diagnostics spec §2.4.
- **Invariants enforced:** L-3, L-4, L-8; G-2 (gate wired here, thresholds from RT-07/08).
- **Acceptance tests:** full emulator CI green (happy path unchanged — Phase 9F, home-return, menu goldens byte-identical); `[APP] enter/exit` pairs balanced across a scripted session.
- **Emulator tests:** I-E3 (begin-fail containment via RT-16 injection: app returns to menu, toast shown, relaunch works); state-violation assert never fires across the full 71-script suite.
- **Hardware tests:** open/close every app twice with serial capture; `enter`/`exit` pairing + M-4 leak canary intact.
- **Failure injection:** `-DNUMOS_FAULT_INJECT_BEGIN=<id>` (RT-16) forces the target app's first guarded alloc to fail.
- **Risk:** medium-high — this touches the exact "do not touch casually" teardown block. Mitigation: state machine is a *relabeling* of existing control flow plus verification; every Phase 9F/home-return script must stay green; no timing constants change.
- **Rollback:** the enum and verification are behind `NUMOS_LIFECYCLE_GUARD=1` (default on); flag off restores today's flow with zero diff in behavior.
- **Difficulty:** M.
- **Owner:** **Fable** (teardown-adjacent semantics), emulator CI as the gate.

## RT-06 — OOM-safe `begin()`/`end()` + universal `load()` null guard  *(P1; supersedes MT-06 and widens it)*

- **Motivation:** Partial-`begin()` strands PSRAM permanently in Fluid2D (13 buffers, `Fluid2DApp.cpp:128-163` vs `end()` gate `:187`), NeuralLab (`NeuralLabApp.cpp:131-136,162`), BridgeDesigner (`BridgeDesignerApp.cpp:98-105,134`); CircuitCore re-entry leak (`CircuitCoreApp.cpp:126-136`); Fractal ignores `_orbit` alloc failure (`FractalApp.cpp:96-97`) and blocks up to 1.5 s in teardown (`:522-531`); 17 `load()`s null-deref on failed `begin()` (e.g. `Fluid2DApp.cpp:235`, `CalculationApp.cpp:131`). Invariants **L-2, L-3 (app side), L-6**.
- **Touched files:** `src/apps/Fluid2DApp.cpp`, `BridgeDesignerApp.cpp`, `NeuralLabApp.cpp`, `CircuitCoreApp.cpp`, `FractalApp.cpp`, plus a one-line `if (!_screen) return;` after the `begin()` call in the `load()` of all 20 apps (pattern: `OpticsLabApp.cpp:158-163`).
- **Implementation outline:** per app: extract `freeBuffers()` (null-safe, per-pointer, idempotent); call unconditionally from `end()` (LVGL teardown stays gated on `_screen`) and from every `begin()` failure exit *before* returning; CircuitCore: `begin()` early-outs if buffers already live (or frees first). Fractal: `_orbit==nullptr` disables perturbation path; `stopRenderTask` wait loop budget 1500→250 ms (force-kill branch retained, `FractalApp.cpp:527-531`).
- **Invariants enforced:** L-2, L-3, L-6.
- **Acceptance tests:** firmware builds; `end()` called twice in a scratch harness is a no-op; per-app open/close×2 leak canary (M-4) flat on device.
- **Emulator tests:** these five apps are firmware-only (`platformio.ini:195-266`) — emulator coverage limited to the `load()` guard in whitelisted apps (I-E3 once RT-16 lands); ASAN job (MT-09, if landed) covers whitelisted lifecycle.
- **Hardware tests:** fault-injected begin (RT-16 flag) per app: enter → toast/menu → `[MEM]` probes show zero PSRAM delta vs pre-entry; then normal entry works.
- **Failure injection:** `NUMOS_FAULT_INJECT_BEGIN` (compile-time app id + alloc index).
- **Risk:** low-medium — pure free-path refactors; UAF risk only if a free reorders before timer/task stop (keep existing stop-then-free order; Fractal join-budget change is the only timing edit).
- **Rollback:** per-app revert (each app is one self-contained commit).
- **Difficulty:** M (5 apps + 20 one-liners).
- **Owner:** **Sonnet** writes (mechanical, per the MT-06 pattern), **Opus** reviews Fractal task-lifecycle change; HIL leak check **Human**.

## RT-07 — LVGL pool launch gate + step-widget caps everywhere  *(P1; extends merged MT-02, absorbs MT-08)*

- **Motivation:** Pool OOM is now loud (`lv_conf.h:153-164`) but still fatal mid-session; the launcher will happily start an app into a nearly-full pool; Equations has budget gates (`EquationsApp.cpp:486-503`) but Calculation edu-steps and Calculus step rendering do not. Invariants **G-2, G-3, G-4**.
- **Touched files:** `src/SystemApp.cpp` (`launchApp` gate — one `lv_mem_monitor` check inside the RT-05 guard); `src/apps/CalculationApp.cpp` (step-renderer creation sites; arena/step vectors cleared at `:114-122` already), `src/apps/CalculusApp.cpp` (canvas-per-step sites); shared constants header with `EquationsApp.h:267-270`-style budgets.
- **Implementation outline:** launch gate: refuse when `used_pct ≥ 80` → `[APP] launch-refused` + toast. Port `canAllocStep()` (child cap + largest-internal-block floor) from Equations into Calculation/Calculus step paths; truncation UI: "… (N more steps)" tail label (the MT-08 design, now under this ticket).
- **Invariants enforced:** G-2, G-3 (alarm via `[LVGL] pool` exit lines), G-4.
- **Acceptance tests:** emulator: calc edu-steps semantic suite (8C) green with caps at generous defaults; scratch `-DLV_MEM_SIZE=16384` build now *refuses launches* instead of aborting mid-build for the launch-gate path (the boot-time menu itself still asserts if 16 KB can't hold it — that path stays the MEM-A loud-abort).
- **Emulator tests:** scripted long-steps session asserts the truncation label text; pool gate unit-testable natively (CLIB `lv_mem_monitor` still reports used%… **caveat:** under `LV_STDLIB_CLIB` `lv_mem_monitor` is not meaningful — gate compiles to always-pass on native; assert only the firmware branch by review + hardware).
- **Hardware tests:** drive Equations to a 100+ step solve; `lvgl=` field stays < 85 %; no aborts.
- **Failure injection:** scratch `-DLV_MEM_SIZE=32768` firmware build to make gates trip early.
- **Risk:** medium — UX change on long solutions (MT-08's known trade-off); LVGL group/focus handling when capping lists.
- **Rollback:** raise caps to ∞ / gate threshold to 100 %.
- **Difficulty:** M.
- **Owner:** **Opus** (UI lifecycle + focus semantics).

## RT-08 — Central memory guard + low-water warnings  *(P1)*

- **Motivation:** Post-MEM-A the numbers are visible but nothing *acts* on them; internal floor has no defender (`RESERVE_INTERNAL=0`, memory audit §2 mech.5); heavy ops start blind. Invariants **M-1, M-2, M-5**; feeds G-2, H-2 pre-flight.
- **Touched files:** new `src/utils/MemGuard.h` (header-only, `#ifdef ARDUINO` like MemProbe); consumers: `SystemApp::launchApp` (via RT-05), `GiacBridge.cpp` pre-flight (RT-09), `EquationsApp`/`CalculusApp` solve entries, sim `begin()`s, `PythonEngine::init`; heartbeat check in `main.cpp:256-261`; StatusBar "▲MEM" indicator (`src/ui/StatusBar.h/.cpp` — additive label, hidden by default → menu goldens unaffected; grapher/statusbar goldens must be checked for the hidden-object no-op).
- **Implementation outline:** `MemGuard::allows(Op)` reading `heap_caps_get_largest_free_block` for both domains + `lv_mem_monitor`; thresholds per fault spec §E.5 (SPIRAM largest ≥ 512 KB for heavy ops; internal largest 40 KB warn / 20 KB critical; pool 80 %); `[MEM] LOW/CRITICAL` lines (diagnostics §2.3) with 30 s rate limit; CRITICAL escalation: force `returnToMenu()` + `flushPendingTeardownNow`.
- **Invariants enforced:** M-1, M-2, M-5 (stack floor logged from the existing `stack=` source).
- **Acceptance tests:** firmware builds; thresholds compile-time tunable; no false LOW during a normal full-app tour (tune with MT-01 field data).
- **Emulator tests:** MemGuard compiles to always-allow natively (no heap_caps) — logic unit-tested on host with a mocked stats struct.
- **Hardware tests:** scratch PSRAM-pinning allocation (debug command `!eat <kb>` behind `NUMOS_DIAG_VERBOSE`) drives SPIRAM largest below 512 KB → Giac/solve/sims refuse with the documented messages; releasing recovers.
- **Failure injection:** the `!eat` debug command (also used by I-H6).
- **Risk:** low-medium — false-positive refusals if thresholds are wrong; all thresholds are single constants.
- **Rollback:** thresholds to 0 (guards always pass); header removable.
- **Difficulty:** S-M.
- **Owner:** **Sonnet** (guard) + **Human** threshold sign-off after a field-data pass.

## RT-09 — Giac query deadline/cancel + CAS cooperative deadlines + AC-match cap  *(P1)*

- **Motivation:** `solveWithGiac` has **no timeout**: `caseval_maxtime` is compiled out, nothing sets `giac::ctrl_c`, recursion cap 100 throws too late for a 64 KB stack (`GiacBridge.cpp:390-448`; `lib/giac/src/kglobal.cc:180-222,1679`); reachable by any serial user (`SerialBridge.cpp:155`). In-tree CAS can freeze via O(N!) AC-matching (`RuleEngine.cpp:122-138,201-217`) inside a 100-step fixpoint (`RuleEngine.h:380`). Invariants **H-2, H-4**; extends merged MT-13's plan.
- **Touched files:** `src/math/giac/GiacBridge.cpp` only for Giac (single entry point — keep it that way); `src/math/cas/RuleEngine.cpp/.h` (term-count cap + deadline check per fixpoint step); `src/math/cas/SymSimplify.cpp` (deadline check per pass); `src/apps/EquationsApp.cpp`/`CalculusApp.cpp` (pass a deadline into solves). **`lib/giac/**` untouched.**
- **Implementation outline:** (a) Giac: pre-flight length ≤ 2048 + `MemGuard::allows(GIAC)`; one-shot `esp_timer` armed for 10 s before `giac::eval`, callback sets `giac::ctrl_c = true; giac::interrupted = true;` (both are extern globals in the vendored tree — verify exact symbols against `lib/giac/src/global.h` at implementation; ISR-safe: plain bool stores); Giac's `control_c()` checks raise an exception → existing catch (`:441-447`) → `Error: timeout`; disarm timer + clear flags on every exit; emit `[GIAC]` line (diagnostics §2.7). (b) CAS: `CasDeadline` (millis-based) checked in `applyToFixedPoint` per step and `SymSimplify` per pass → error result "computation too complex"; AC-matching refuses > 8 terms per match attempt (`[CAS] ac-skip`). (c) **Hardware validation is part of the ticket:** after a forced timeout, run 20 normal queries — if the global context misbehaves, escalate contract to "Giac disabled until reboot" (fault spec F.3 fallback) and log.
- **Invariants enforced:** H-2, H-4.
- **Acceptance tests:** firmware builds; normal `:1+1`, `:solve(x^2=4,x)` outputs unchanged (manual spot list — Giac has zero automated tests, ground-truth P-02 stands).
- **Emulator tests:** CAS deadline/AC-cap only (giac excluded natively): host unit test for `CasDeadline` + an emulator script driving a many-term simplify asserting no `[ASSERT] FAIL` and bounded runtime; Giac paths not emulator-testable.
- **Hardware tests:** I-H5 (slow query → `Error: timeout` ≤ 10.5 s, heartbeats continuous, context healthy after); I-H6 (pre-flight refusal under `!eat` pressure).
- **Failure injection:** adversarial serial inputs (`:factor(...)` on a measured-slow composite); `!eat` for the mem gate.
- **Risk:** medium-high — `ctrl_c` semantics inside this KhiCAS build are the unknown; the ticket's Plan B (post-timeout context poisoned ⇒ disable-until-reboot) bounds the downside. esp_timer callback must only set flags (no Giac calls from timer context).
- **Rollback:** remove timer + caps; the catch-all remains (today's behavior).
- **Difficulty:** M.
- **Owner:** **Fable** (adversarial surface + vendored-lib interaction), HIL **Human**.

## RT-10 — Expression depth guard (editor-first) + evaluator/layout defensive caps  *(P1)*

- **Motivation:** CursorController has no nesting limit (`CursorController.cpp:365-985`); Calculation doesn't gate inserts at all (Grapher does, `MAX_NEST=8`, `GrapherApp.cpp:2547-2593`); layout recursion is uncapped and runs before the draw-depth check (`MathAST.cpp:191,377-378…`; `MathRenderer.cpp:1477` vs `:1507-1514`); `MathEvaluator::evaluate` recurses unguarded (`MathEvaluator.cpp:517-533`). A held DIV key is a user-reachable stack overflow. Invariant **H-5**.
- **Touched files:** `src/math/CursorController.h/.cpp` (central `nestingDepth()` + refusal in the six structural inserts); `src/apps/CalculationApp.cpp` (nothing if the cap lives in CursorController; remove-or-keep Grapher's local gate — keep, it's tighter); `src/math/MathEvaluator.cpp` (depth parameter, error at 24); `src/math/MathAST.cpp`/`MathRenderer.cpp` (layout depth counter, cap 24, "⚠" fixed box on breach — see risk note).
- **Implementation outline:** `vpam::MAX_EXPR_DEPTH = 16` structural cap at insertion (single choke point: CursorController's insert* methods compute depth by parent-walk — the same walk Grapher's `cursorDepth()` lambda does at `GrapherApp.cpp:2547`); evaluator/layout carry `depth` params with cap 24 (> editor cap ⇒ unreachable in normal operation, purely defensive) returning `makeError("Too deep")` / fixed-size box. History per-entry node-count cap (500 nodes) at clone time (`CalculationApp.cpp:559`) folds in here (C5.3).
- **Invariants enforced:** H-5; makes C5.1/C5.2/C5.4 unreachable.
- **Acceptance tests:** **zero golden diffs** — the caps must not alter layout of any existing golden expression (all goldens are ≤ depth 12 by construction; run full candidate generation + compare to prove it).
- **Emulator tests:** I-E4 (40× nested insert script: inserts stop, no crash, expression still evaluates); full calc semantic suites green.
- **Hardware tests:** hold DIV via serial autorepeat 60 s; device alive; `[INPUT] depth-cap` (debug) counts.
- **Failure injection:** scratch build with `MAX_EXPR_DEPTH=4` to make the cap trivially reachable in scripts.
- **Risk:** medium — MathAST/MathRenderer are golden-fragile files; the layout depth counter must be a pure pass-through when under the cap (no geometry change). The evaluator error string must match existing "Math ERROR" display conventions (`MathEvaluator.cpp:175-178`).
- **Rollback:** caps to 255 (inert); parameters remain.
- **Difficulty:** M.
- **Owner:** **Fable** (renderer-adjacent + golden risk), with full candidate re-generation in the PR.

## RT-11 — Renderer allocation-failure & layout hardening  *(P2)*

- **Motivation:** `SymExprToAST::convert` builds unbounded VPAM trees from CAS results (`SymExprToAST.cpp:188+`); three `SymExpr.cpp` clone chains wrap unchecked possibly-null children (`SymExpr.cpp:149,327,331`) — a live node with a null child derefs later; Grapher's failed-buffer descriptor keeps stale fields (`GrapherApp.cpp:593-594`). Invariants **F.4 contract**, closes C4.5/C5.8.
- **Touched files:** `src/math/cas/SymExpr.cpp` (null-check the three clone chains → return nullptr up); `src/math/cas/SymExprToAST.cpp` (node-count budget, e.g. 2000, → nullptr + caller shows "result too large to render" text fallback); `src/apps/EquationsApp.cpp`/`CalculusApp.cpp`/`CalculationApp.cpp` (text-fallback branch at their convert call sites); `src/apps/GrapherApp.cpp:588-597` (zero the descriptor on failure).
- **Implementation outline:** mechanical null-propagation + one budget counter threaded through `convert`; fallback = plain `lv_label` with the `toString()` of the expression (strings already exist for step logs).
- **Invariants enforced:** F.4 rows 2-3; C4.5 closed.
- **Acceptance tests:** emulator CI green (whitelisted cas:: files compile natively — `SymExprToAST` is in the whitelist, `platformio.ini:251-258`); no golden diffs (fallback only triggers past budgets no golden reaches).
- **Emulator tests:** host unit test: clone under a 1-block arena forced to exhaustion returns nullptr (no partial tree); convert with budget 10 on a 50-node tree returns nullptr.
- **Hardware tests:** Equations solve with a large system under `!eat` PSRAM pressure → "out of memory" step, no crash.
- **Failure injection:** tiny-arena scratch build (`SymExprArena` block count 1); convert budget=10 debug flag.
- **Risk:** low — additive checks; the budget constant is the only tuning point.
- **Rollback:** budget to SIZE_MAX; null-checks are pure hardening (keep even on rollback).
- **Difficulty:** S-M.
- **Owner:** **Opus**.

## RT-12 — Graphing stress watchdog (replot wall-clock budget)  *(P2)*

- **Motivation:** `replot()` is synchronous in key handlers with no wall-clock cap; implicit/inequality worst case = tens of thousands of RPN evals per keystroke (`GrapherApp.cpp:1716-1832`, per-pixel shading `:1774-1793`); `yield()` no-ops natively (`:34-41`). With RT-01's WDT, an adversarial plot becomes a *reset* — it must become a *truncated plot* instead. Invariant **H-3**.
- **Touched files:** `src/apps/GrapherApp.cpp` (`replot()` `:1865-1912`, `plotImplicit` `:1716-1832`, `sampleFuncAdaptive` `:1685-1706`, `rebuildTable` `:2272-2359`, `executeCalcOption` `:3083-3155`); `src/ui/GraphView` untouched.
- **Implementation outline:** `PlotBudget{uint32_t deadlineMs}` captured at `replot()` entry (200 ms; native uses the deterministic tick so scripts stay reproducible); checked per implicit row (`:1738`), per boundary cell batch, per adaptive segment batch, per table row; on expiry: stop plotting further primitives, set `_plotTruncated`, draw existing info-bar text "⚠ plot truncated", emit `[GRAPH] replot … aborted=1`. `executeCalcOption` analyses get a 500 ms budget → "no result (timeout)". Truncation must be deterministic under `--deterministic` (budget counted in *evals*, not wall-clock, when `NATIVE_SIM` deterministic mode is active — e.g. 50 000 evals ≈ the same envelope).
- **Invariants enforced:** H-3.
- **Acceptance tests:** **all 36 grapher scripts + 6 goldens byte-identical** (no existing golden may come near the budget; if any does, the budget is wrong — raise it, do not re-bless); Phase 9F green.
- **Emulator tests:** I-E5 (injected slow-eval + implicit plot → `aborted=1`, exit 0, next-frame keys processed); new stress script with `assert_no_error`.
- **Hardware tests:** I-H10 (adversarial inequality via serial; responsiveness ≤ 300 ms/keystroke; no WDT reset).
- **Failure injection:** `-DNUMOS_FAULT_INJECT_SLOW_EVAL=<µs per eval>` delay shim in `GraphModel::evalAt`/`evalImplicit` (native + firmware scratch).
- **Risk:** medium — partial plots are a visible UX change on adversarial input; determinism plumbing must not touch normal timing. The `_plotDirty` interaction (`:1865-1867`) must leave truncated plots *retryable* (dirty stays set? No — clear dirty, retry on next input; document).
- **Rollback:** budget constant to UINT32_MAX (inert).
- **Difficulty:** M.
- **Owner:** **Opus** (Grapher is well-gated by CI), **Fable** review of determinism design.

## RT-13 — Storage corruption recovery (quarantine, validation, atomic writes, no auto-format)  *(P2)*

- **Motivation:** `/vars.dat` accepts garbage incl. `den==0` (`VariableManager.cpp:153-163`), truncated loads return success (`:219-225`), writes are unchecked+non-atomic (`:169-190`), **every mount is format-on-fail** (6 sites — C6.4), ParticleLab's grid deserialization is memory-unsafe (`ParticleEngine.cpp:256-264` + `MAT_LUT[31]`, `ParticleEngine.h:97,228`). Invariant **R-3**; fault-spec C6 wholesale.
- **Touched files:** `src/math/VariableManager.cpp/.h` (format v2: magic `VR02` + version + CRC32; content validation `den!=0`, magnitude sanity; temp+rename write; quarantine-on-fail); `src/SystemApp.cpp:144-156` (`begin(false)`, `[FS]` lines); `src/apps/NeoLanguageApp.cpp:903,926`, `CircuitCoreApp.cpp:1364,1483,1720`, `Fluid2DApp.cpp:1454,1485,1517` (`begin(false)`); `src/apps/ParticleLabApp.cpp:426-445` + `ParticleEngine.cpp:256-264` (clamp `type < MAT_COUNT` on load, else reject file).
- **Implementation outline:** v2 loader accepts v1 ("VR01") read-only for migration (load, validate, rewrite as v2); any validation failure ⇒ rename to `.bad` (one generation) + `[FS] …quarantine` + empty store; ParticleLab: byte-wise clamp pass before `memcpy` into the live grid, reject on any out-of-range type (cheap: single pass over 76 800 bytes). Write path: `open("/vars.tmp","w")` → checked writes → `close` → `LittleFS.rename` (available in the FS API; emulator shim gains `rename` — 5-line addition to `hal/FileSystem`).
- **Invariants enforced:** R-3; C6.1/2/3/4/5 closed; C6.9 unchanged (wear is acceptable; note for future).
- **Acceptance tests:** round-trip: store → reboot → load, values identical (emulator + hardware); v1 file migrates to v2 once.
- **Emulator tests:** I-E6/I-E7 (corrupt / truncated `emulator_data/vars.dat` fixtures → quarantine lines + empty store, exit 0); host unit test for CRC/validation on crafted buffers; ParticleLab has no emulator coverage — host-test `ParticleEngine::deserialize` clamp with a crafted grid (engine is plain C++, compiles on host).
- **Hardware tests:** I-H8 (power-cut mid-store loop: next boot = old file or quarantine, never format); manual: corrupt `/vars.dat` via `!fs`-adjacent debug write, reboot, verify quarantine.
- **Failure injection:** crafted files in `emulator_data/`; on hardware, a debug `!corrupt vars` command (debug builds) that flips bytes in place.
- **Risk:** medium — the committed `emulator_data/vars.dat` (379 B v1, verified well-formed) and any user devices migrate formats; migration bug = perceived data loss. Mitigation: v1 read support + quarantine (never delete).
- **Rollback:** loader accepts both formats indefinitely; revert = keep v1 writer (one constant).
- **Difficulty:** M.
- **Owner:** **Opus** (format/migration correctness), ParticleLab clamp **Sonnet**.

## RT-14 — Input hardening: modifier reset on menu return + overflow drop telemetry  *(P2)*

- **Motivation:** `KeyboardManager` LOCK/STORE persists across app exit (singleton; `returnToMenu` never resets — `SystemApp.cpp:860-883`; states `KeyboardManager.h:47-54`, clear paths `.cpp:106-130`) — invariant **L-7**. All three queues drop silently (`Keyboard.cpp:188-193`; `SerialBridge.cpp:74-75,116`; `LvglKeypad.cpp:55-59`) — C7.1.
- **Touched files:** `src/SystemApp.cpp` (`returnToMenu` gains `KeyboardManager::instance().reset()`); `src/drivers/Keyboard.cpp`, `src/input/SerialBridge.cpp`, `src/input/LvglKeypad.cpp` (drop counters); heartbeat flush of `[INPUT] drop` (diagnostics §2.10).
- **Implementation outline:** one `reset()` call + `[INPUT] mod-reset` debug line; per-queue `uint16_t _dropCount` incremented at the existing drop branches, drained by the heartbeat.
- **Invariants enforced:** L-7; C7.1 observable.
- **Acceptance tests:** emulator CI green; I-E11 (ALPHA-LOCK in Calculation → exit → Grapher digit interpreted as digit).
- **Emulator tests:** I-E10 (200-key storm → drop lines, consistent final state); I-E11.
- **Hardware tests:** none specific (serial-driven identical).
- **Failure injection:** storm scripts.
- **Risk:** low. One behavioral check: no app *relies* on a modifier surviving exit (grep `indicatorText`/state reads at entry — none found in audit; verify in PR).
- **Rollback:** remove the reset call + counters.
- **Difficulty:** S.
- **Owner:** **Sonnet**.

## RT-15 — Diagnostics app / hardware bring-up screen  *(P2, depends RT-02/04)*

- **Motivation:** Safe mode needs a surface (safe-mode spec §8); hardware bring-up (keyboard matrix HW-1, display batches HW-5) needs input-echo and display-test pages; today `HardwareTest.cpp` is dead code (`src/HardwareTest.cpp:86-94`, unreachable — verified).
- **Touched files:** new `src/apps/DiagnosticsApp.h/.cpp`; `src/SystemApp.h/.cpp` (member + mode + dispatch + teardown case — the `teardownModeNow` switch `SystemApp.cpp:182-218` gains one case); `src/ui/MainMenu.cpp` (card id 21, appended — **launcher menu golden changes ⇒ new goldens must be candidate-generated and owner-blessed**, or the card ships behind `NUMOS_DIAG_APP=1` default-on-validate only: **decision for the owner; spec default = flag-gated to protect goldens**); optionally delete `src/HardwareTest.cpp` (+ its `tests/` twin) in the same PR.
- **Implementation outline:** six pages per safe-mode spec §8; ≤ 10 LVGL objects; pull-based refresh; actions page safe-mode-gated; standard `begin/end/load/handleKey/update` contract (L-2/L-3 compliant from day one — reference implementation for the guard).
- **Invariants enforced:** supports R-2/R-4 surfaces; none directly.
- **Acceptance tests:** app opens/closes cleanly ×2 with flat `[MEM] exit` (M-4); all pages render.
- **Emulator tests:** whitelist the app + a smoke script (`diag_smoke.numos`: open, page through, `assert_app`, home); golden optional (mem numbers are no-op natively → page 2 shows placeholders — deterministic).
- **Hardware tests:** input-echo page shows serial-injected keys; display-test page visually inspected (BGR/rotation); after keyboard bring-up: matrix keys echo with correct codes.
- **Failure injection:** n/a.
- **Risk:** low-medium — launcher-card golden impact is the only trap; flag-gating removes it.
- **Rollback:** remove app + card (flag off).
- **Difficulty:** M.
- **Owner:** **Sonnet** (app is boilerplate), **Human** blesses any golden change.

## RT-16 — Emulator fault-injection + safe-mode/diag plumbing  *(P2, enabler)*

- **Motivation:** Fault paths need CI-visible exercise: begin-fail (I-E3), slow-eval (I-E5), boot-count (I-E8), diag console (I-E9). The emulator masks memory truth (MEMX-13) but can prove *logic*.
- **Touched files:** `src/hal/NativeHal.cpp` (CLI flags `--boot-count/--safe-mode/--reset-reason`, `.numos` commands `diag <cmd>`, `assert_safe_mode` — all append-only per the script-grammar contract `NativeHal.cpp:1447-1612`); fault-inject shims: `src/utils/FaultInject.h` (`NUMOS_FAULT_INJECT_*` compile flags consumed by the guarded alloc sites from RT-06 and `GraphModel::evalAt`); `tests/emulator/scripts/*.numos` (new scripts); `.github/workflows/emulator-build.yml` — **additive job/steps only, owner-reviewed** (repo CI rule).
- **Implementation outline:** compile-time injection (no runtime flag parsing in firmware paths): `FaultInject.h` defines `NUMOS_FI_BEGIN_APP(id, idx)` macros returning forced-failure at configured sites, all zero-cost when flags unset; NativeHal boot consults `--boot-count/--safe-mode` to seed the (native) CrashRecord global; `diag` command feeds DiagConsole directly.
- **Invariants enforced:** none directly; makes I-E3/5/8/9/10/11 executable.
- **Acceptance tests:** default build byte-identical behavior (all flags off ⇒ zero object-code delta at the alloc sites — verify macro collapses); full existing 71-script suite green.
- **Emulator tests:** the new scripts themselves; determinism double-run (SHA-256) holds for all new scripts.
- **Hardware tests:** the same `FaultInject.h` flags work on firmware scratch builds (used by RT-01/05/06 HIL steps).
- **Failure injection:** n/a (this is the mechanism).
- **Risk:** medium *process* risk: NativeHal script machinery is a CI contract (exit codes 0/2/3/4 append-only); CI edits are owner-gated — deliver as reviewed PR, never self-merge.
- **Rollback:** flags default-off; new scripts removable; NativeHal additions inert without flags.
- **Difficulty:** M.
- **Owner:** **Sonnet** (mechanics) with **Human** review of the CI additions.

---

## Priority summary

| Order | Ticket | Closes | Effort | Owner |
|---|---|---|---|---|
| 1 | RT-01 watchdog | H-1; hangs→recorded resets | S | Sonnet + HIL |
| 2 | RT-02 crash record | R-1; C9.1 | S-M | Sonnet + HIL |
| 3 | RT-04 diag console + ring | R-4 | S-M | Sonnet |
| 4 | RT-06 OOM-safe begin/end | L-2/L-3/L-6; C3 | M | Sonnet/Opus + HIL |
| 5 | RT-10 depth guard | H-5; C5.1/2/4 | M | Fable |
| 6 | RT-09 Giac/CAS deadlines | H-2/H-4; C4.1/4 | M | Fable + HIL |
| 7 | RT-03 safe mode | R-2/R-5 | M | Fable/Opus + HIL |
| 8 | RT-13 storage recovery | R-3; C6 | M | Opus |
| 9 | RT-05 lifecycle guard | L-3/L-4/L-8 | M | Fable |
| 10 | RT-08 memory guard | M-1/M-2/M-5 | S-M | Sonnet |
| 11 | RT-12 replot budget | H-3; C8 | M | Opus |
| 12 | RT-07 pool gates/caps | G-2/G-3/G-4 | M | Opus |
| 13 | RT-11 renderer hardening | C4.5/C5.8 | S-M | Opus |
| 14 | RT-14 input hardening | L-7; C7.1 | S | Sonnet |
| 15 | RT-15 diagnostics app | bring-up surface | M | Sonnet + Human |
| 16 | RT-16 emulator plumbing | test enabler | M | Sonnet + Human CI review |

Sequencing notes: RT-16 can start any time and unblocks the emulator halves of RT-05/06/12; RT-03 needs RT-02 (+RT-04 for its serial floor); RT-07/08 share threshold constants — land the constants header once. Open memory tickets MT-05 (arena oversize/fallback/pool-overflow) and MT-06 (superseded by RT-06) from `NUMOS_MEMORY_IMPLEMENTATION_TICKETS.md` remain valid; MT-05 is a **prerequisite for M-3** and should ride alongside RT-08.
