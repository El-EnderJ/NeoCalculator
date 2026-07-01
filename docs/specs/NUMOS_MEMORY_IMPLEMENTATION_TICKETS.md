# NumOS — Memory Implementation Tickets

> Prioritized mitigation backlog for the risks in `NUMOS_MEMORY_RISK_REGISTER.md`. Ordering = recommended execution order (instrumentation first: you cannot manage what you cannot see). "Model-safe" states whether an Opus/Sonnet-class agent can execute it without human/hardware in the loop; anything touching `main.cpp` boot order, DMA, or requiring on-device confirmation is flagged.
>
> Global constraints for ALL tickets: never move the LVGL draw buffer to PSRAM or double-buffer it (`main.cpp:131-137`); never `lv_obj_delete` app screens synchronously on exit (`SystemApp.cpp:229-240`); goldens/masks/CI workflows are untouchable except where a ticket explicitly says otherwise; renderer geometry must not change (golden invalidation).

---

## MT-01 — Memory probe header + standard probe points  *(P0)*

- **Goal**: one grep-stable log line exposing internal/PSRAM free + largest block, LVGL pool usage, and stack high-water at defined points; replaces the internal-only heartbeat.
- **Touched files**: new `src/utils/MemProbe.h` (+`.cpp` if needed); `src/main.cpp:233-236` (heartbeat line); `src/SystemApp.cpp` (`launchApp` before `load()`, end of deferred-teardown block ~`:237`); `src/math/giac/GiacBridge.cpp:389-440` (probe before/after `solveWithGiac`).
- **Memory contract**: probe itself allocates nothing (fixed `char[160]` on stack; `printf`-style direct to Serial). No behavior change in emulator (`#ifndef ARDUINO` ⇒ no-op inline).
- **Implementation outline**: `heap_caps_get_free_size/get_largest_free_block` for `MALLOC_CAP_INTERNAL` and `MALLOC_CAP_SPIRAM`; `heap_caps_get_minimum_free_size` for low-water; `lv_mem_monitor()` (guarded — builtin allocator only); `uxTaskGetStackHighWaterMark(nullptr)`. Format: `[MEM] <tag> int=F/L psram=F/L low=I/P lvgl=U%/M% stack=W`.
- **Acceptance tests**: firmware compiles (`pio run -e esp32s3_n16r8`); emulator compiles + full CI suite green (no-op path); on-device: boot log shows `[MEM] boot`, each app open/close emits enter/exit lines, two consecutive exits of the same app report identical psram-free (leak canary).
- **Risk**: minimal — additive logging. Serial noise is the only regression vector.
- **Rollback**: delete the header + revert 4 call sites.
- **Effort**: S (<1 day).
- **Model-safe**: **Sonnet-safe** (compile-verifiable; on-device confirmation is a human follow-up but not required to merge).

## MT-02 — Decide and document the real LVGL heap; add exhaustion visibility  *(P0)*

- **Goal**: make the 64 KB-builtin-pool reality explicit and survivable: define `LV_MEM_SIZE` deliberately, log pool stats, and replace the silent `while(1)` failure with a serial scream.
- **Touched files**: `platformio.ini` (`build_flags` of `esp32s3_n16r8`: add `-DLV_MEM_SIZE=…` and `-DLV_ASSERT_HANDLER=…` or `LV_USE_LOG` routing); `src/lv_conf.h` (delete or clearly comment the dead `LV_MEM_CUSTOM` block as historical — comment-only change); docs.
- **Memory contract**: pool stays internal `.bss`; size decision after MT-01 data (start 64 KB, permit ≤ 96 KB if measurements demand; every +32 KB comes out of the ~212 KB post-static internal budget).
- **Implementation outline**: (1) add explicit `-DLV_MEM_SIZE=65536` (documents current truth; ratchet later); (2) `-DLV_ASSERT_HANDLER='{Serial.printf("[LVGL] OOM %s:%d\n",__FILE__,__LINE__);while(1)delay(100);}'` or equivalent macro include so exhaustion is visible on serial; (3) rewrite the `lv_conf.h:24-29` strategy comment to match reality (keep all live LVGL-9 keys unchanged — widget flags etc. are honored and MUST NOT change, they affect flash and behavior).
- **Acceptance tests**: firmware + emulator build; emulator CI fully green (emulator uses CLIB, unaffected); on-device boot OK; artificially shrinking to `-DLV_MEM_SIZE=16384` on a scratch build produces the new OOM serial line instead of a silent hang.
- **Risk**: low; flag-only. Must NOT touch widget enable flags (behavior/flash).
- **Rollback**: remove flags.
- **Effort**: S.
- **Model-safe**: **Sonnet-safe** for flags+comments; the deliberate pool-size increase is a human decision informed by MT-01 data.

## MT-03 — Delete the splash screen after menu load  *(P0, trivial)*

- **Goal**: reclaim the leaked splash screen objects from the 64 KB pool.
- **Touched files**: `src/ui/SplashScreen.h/.cpp` (add `destroy()` that `lv_obj_delete(_screen)` + nulls); `src/main.cpp:170-197` (call after `g_app.begin()` has loaded the menu screen — NOT before, the splash screen must not be the active screen when deleted).
- **Memory contract**: frees ~1 KB pool + 3 objects; no new allocations.
- **Implementation outline**: after `g_app.begin()` (menu screen is now active via `_mainMenu.load()`), call `g_splash.destroy()`; guard against double-delete; mirror in `NativeHal.cpp` splash path if it keeps its own (check its Phase transition code — append-only change).
- **Acceptance tests**: emulator boot smoke + menu goldens byte-identical (deletion happens after load; no visual change); firmware compiles; on-device boot visually unchanged.
- **Risk**: low-medium — deleting a screen at the wrong moment is the documented hang class; keep the call strictly after menu `lv_screen_load`.
- **Rollback**: remove the call.
- **Effort**: S.
- **Model-safe**: **Opus-safe** (lifecycle-order sensitivity; verify with emulator boot/determinism suite which covers the same code path in NativeHal).

## MT-04 — Lazy first block for `SymExprArena`  *(P1)*

- **Goal**: stop pinning ~288 KB PSRAM at boot in constructors of apps never opened; arena allocates block 0 on first `allocRaw`.
- **Touched files**: `src/math/cas/SymExprArena.h` only (`:65-71` ctor, `:86-97` allocRaw, `:128-142` reset).
- **Memory contract**: zero bytes at construction; first CAS use allocates 64 KB (32 KB NeoLang); `reset()` on a never-used arena stays no-op; failure surfaces as existing nullptr contract.
- **Implementation outline**: drop `allocateBlock()` from ctor; in `allocRaw`, if `_numBlocks==0` allocate first; `reset()` keeps current keep-block-0 semantics when blocks exist. Fold in the MT-05 oversize guard if executed together.
- **Acceptance tests**: emulator build + full CI (cas:: whitelisted files compile in emulator; calc edu-steps semantic suite 8C exercises `_eduArena`); firmware build; on-device MT-01 probes show boot psram-free higher by ~288 KB.
- **Risk**: low — single-header change, contract-preserving. Watch: any code reading `totalAllocated()` at boot for diagnostics.
- **Rollback**: restore ctor call.
- **Effort**: S.
- **Model-safe**: **Sonnet-safe** (emulator CI exercises the arena via edu-steps suite).

## MT-05 — Arena/pool hardening: oversize guard, no bulk internal-fallback, pool-overflow accounting  *(P1)*

- **Goal**: close the three correctness holes: (a) `allocRaw` oversize corruption, (b) ≥ 4 KB blocks falling back into internal RAM, (c) `CasMemoryPool` overflow leak.
- **Touched files**: `src/math/cas/SymExprArena.h:86-97,199-217`; `src/math/cas/CasMemory.h` (shim `monotonic_buffer_resource:149-204`, `CasMemoryPool:259-345`).
- **Memory contract**: `allocRaw(size> _blockSize)` returns nullptr (never writes OOB); arena block / pool slab allocation is SPIRAM-only (remove `MALLOC_CAP_8BIT` fallback at `SymExprArena.h:206-208`, `CasMemory.h:328-330`) — allocation failure now propagates as nullptr/`valid()==false`, which callers already must handle; shim upstream-overflow allocations are tracked in a `std::vector<void*>` (PSRAM) and freed in `release()`.
- **Implementation outline**: (a) one `if (size > _blockSize) return nullptr;` before the bump; (b) delete fallback branches (keep small-node fallbacks elsewhere per policy §1.4); (c) give the shim MBR an owned list of upstream blocks; `release()` frees them; document that on GCC≥9 (`CAS_HAS_PMR=1`) `std::pmr::monotonic_buffer_resource::release()` already frees upstream — behavior converges.
- **Acceptance tests**: emulator build + full CI; add a host-side unit test (g++ like `tests/host/keycode_digit_test.cpp`) for: oversize returns nullptr; overflow blocks freed on release (allocation counter). Firmware compiles. NOTE: cas:: has no running test suite (ground-truth P-01) — the host mini-test is mandatory, not optional.
- **Risk**: medium — removing the fallback changes failure behavior under PSRAM pressure from "silently take internal RAM" to "fail the CAS operation"; that is the policy intent (§1.4) but callers with missing null-checks become visible. Grep + fix `create<`/`allocRaw` call sites without checks in the same PR.
- **Rollback**: revert header changes (self-contained).
- **Effort**: M (1–2 days incl. call-site sweep + host test).
- **Model-safe**: **Opus** (call-site sweep judgment; Sonnet can do the mechanical parts with the host test as the gate).

## MT-06 — OOM-safe `begin()`/`end()` for Fluid2D, BridgeDesigner, NeuralLab  *(P1)*

- **Goal**: a failed `begin()` leaves zero allocations behind; `end()` is safe after partial `begin()`.
- **Touched files**: `src/apps/Fluid2DApp.cpp` (`begin` ~`:123-170`, `end` ~`:187-226`), `src/apps/BridgeDesignerApp.cpp` (`:98-154`), `src/apps/NeuralLabApp.cpp` (`:128-182`).
- **Memory contract**: unchanged sizes; frees become unconditional (null-guarded per-pointer instead of gated on `_screen`); alloc-failure path frees everything acquired so far before returning.
- **Implementation outline**: extract `freeBuffers()` per app (null-safe, idempotent); call from `end()` (regardless of `_screen`) and from the `begin()` failure path; keep LVGL teardown gated on `_screen` as today.
- **Acceptance tests**: firmware compiles; these apps are firmware-only (no emulator coverage — ground-truth EMU-5), so: code-review + on-device open/close×2 of each app with MT-01 probes showing exit-state parity. Idempotence check: call `end()` twice in a scratch build.
- **Risk**: low — refactor of free paths; UAF risk if a free is reordered before task-stop (NeuralLab has no task; Fluid2D none; keep Fractal out of scope — already correct).
- **Rollback**: per-app revert.
- **Effort**: S-M.
- **Model-safe**: **Sonnet-safe to write**, but merging deserves the on-device probe check (human) since no automated coverage exists.

## MT-07 — Remove Arduino `String` from `Token`  *(P2)*

- **Goal**: kill the main internal-heap churn/fragmentation source: `Token::text` (`Tokenizer.h:59`) rebuilt on every Grapher edit across 6 cached RPN vectors.
- **Touched files**: `src/math/Tokenizer.h/.cpp`, `src/math/Parser.cpp`, `src/math/Evaluator.cpp`, `src/apps/GraphModel.h/.cpp` (Token consumers), possibly `src/math/EquationSolver.*`.
- **Memory contract**: `Token` becomes fixed-size (e.g. `char text[24]` — longest current token is a function name/number literal; verify max, numbers can be long → choose 32 and truncate-with-error) or `std::string` (PSRAM via global override). Fixed `char[]` preferred: zero heap per token.
- **Implementation outline**: change the field; sweep all `.text` uses (String API → C string/std::string API); keep tokenizer grammar byte-identical.
- **Acceptance tests**: **full emulator CI is the gate** — Grapher has 36 scripts + 6 goldens + Phase 9F no-hang; calc suites cover Evaluator via EquationSolver paths. All must stay green with zero golden diffs.
- **Risk**: medium — wide mechanical sweep; truncation policy must reject over-long tokens explicitly (parse error), never silently truncate numbers.
- **Rollback**: revert commit (self-contained representation change).
- **Effort**: M.
- **Model-safe**: **Opus** (semantic edge: token length policy); CI gives a strong safety net.

## MT-08 — Cap widget-backed step renderers (LVGL pool protection)  *(P2)*

- **Goal**: bound `_stepRenderers` growth (one MathCanvas widget per solver step) in EquationsApp/CalculusApp/CalculationApp so step count can never exhaust the 64 KB pool.
- **Touched files**: `src/apps/EquationsApp.cpp` (push sites `:1141-3231`), `src/apps/CalculusApp.cpp`, `src/apps/CalculationApp.cpp` (`_stepRenderers` uses); shared cap constant in a small header.
- **Memory contract**: ≤ N rendered step widgets alive (N≈12, sized from MT-01 data); further steps text-only or paged (create on scroll, delete off-screen ones).
- **Implementation outline**: simplest safe version: hard cap + "… (k more steps)" tail label; paging is a follow-up. Also cap CalculationApp history per-entry node count here (MEMX-15) if trivial.
- **Acceptance tests**: emulator: CalculationApp edu-steps suite (8C) green; Equations/Calculus are firmware-only → compile + on-device scroll-through-steps session with MT-01 lvgl% probe staying < 80 %.
- **Risk**: medium (UX change for long solutions; LVGL group/focus handling when capping).
- **Rollback**: raise cap to ∞ (constant).
- **Effort**: M.
- **Model-safe**: **Opus** (UI lifecycle + focus semantics).

## MT-09 — Emulator ASAN lifecycle job (partial substitute for on-device memory truth)  *(P2)*

- **Goal**: catch UAF/leaks in app lifecycle (begin/end/re-enter, arena reset ordering) in CI, within the emulator's covered app set.
- **Touched files**: `platformio.ini` (new `[env:emulator_pc_asan]` extending `emulator_pc` with `-fsanitize=address,undefined -g`); `.github/workflows/emulator-build.yml` — **additive job only** (new job, no edits to existing gates; per repo rules this is the one CI change allowed and it must be reviewed by the owner).
- **Memory contract**: n/a (tooling).
- **Implementation outline**: build ASAN env; run the existing script suite headless (`--headless --deterministic`); leaks-at-exit + UAF fail the job. `LSAN_OPTIONS=suppressions=` file for LVGL statics and the never-freed menu (expected-alive set: menu screen, fonts, indev).
- **Acceptance tests**: job green on current tree (after suppressions); seeded UAF (scratch branch reverting a teardown null) turns it red.
- **Risk**: low-medium — CI flakiness; suppression list maintenance. Explicitly does NOT validate domains/pool (MEMX-13 stands).
- **Rollback**: delete env + job.
- **Effort**: M.
- **Model-safe**: **Sonnet-safe** technically, but CI-workflow edits are owner-gated in this repo — deliver as a PR for human review; do not self-merge.

## MT-10 — Ratify and rename the global allocator override  *(P2, mostly docs+rename)*

- **Goal**: make MEMX-02 impossible to trip over: the file that defines system-wide allocation policy must say so.
- **Touched files**: `src/math/giac/GiacAlloc.cpp` → move/rename to `src/SystemAllocator.cpp` (or keep path, rewrite header comment); decide the `#if` guard: policy says the override should hold whenever `ARDUINO && BOARD_HAS_PSRAM`, not only when Giac is on — **that guard change is a deliberate decision** (today disabling Giac silently flips every domain); add one boot log line "`[ALLOC] global new -> PSRAM-first (SystemAllocator)`" near `main.cpp:116-121`.
- **Memory contract**: zero behavioral change with Giac on (the shipped config). With the guard widened, behavior becomes Giac-independent (strictly safer/more predictable).
- **Acceptance tests**: firmware builds; firmware.map still attributes `_Znwj` to the (renamed) object file; boot log shows the line; emulator unaffected.
- **Risk**: low (rename + comment + log). Guard widening is flag-level but semantic — call it out in the PR.
- **Rollback**: revert rename.
- **Effort**: S.
- **Model-safe**: **Sonnet-safe** (keep guard as-is unless owner approves widening).

## MT-11 — Reclaim the dead 32 KB DMA staging buffer  *(P2)*

- **Goal**: stop allocating the staging buffer while DMA is force-disabled; keep the code path for a future DMA bring-up.
- **Touched files**: `src/display/DisplayDriver.cpp:166-199` (gate `heap_caps_malloc` on `_dmaEnabled` or a `DISPLAY_ENABLE_DMA_STAGING` compile flag, default off), destructor `:36-43` already null-safe.
- **Memory contract**: +32 KB internal-DMA headroom every boot; no change to flush behavior (live path never used it: `DisplayDriver.cpp:263,337`).
- **Acceptance tests**: firmware compiles; **on-device boot + visual check required** (display init path is HIL-only, no emulator equivalent); boot log line changes from staging OK → skipped.
- **Risk**: medium *process* risk (touching `DisplayDriver`/boot order is the "do not touch casually" list) but the change is a guard around an allocation, not a sequencing change.
- **Rollback**: revert guard.
- **Effort**: S.
- **Model-safe**: **NOT model-verifiable** — Sonnet can write it; a human must boot real hardware before merge.

## MT-12 — Fix arena BigInt lifetime (registry or prohibition)  *(P3)*

- **Goal**: eliminate the `mbedtls_mpi` leak when arena-placed `SymNum` values hold promoted `CASInt`s.
- **Touched files**: `src/math/cas/CASInt.h` (`allocBig` `:449-458` — take an optional arena to register with), `src/math/cas/SymExprArena.h:108-116`, SymExpr construction paths that copy `CASRational` into arena nodes (`SymExpr.h:145-172`, `SymExpr.cpp`).
- **Memory contract**: every promoted mpi reachable from an arena is freed exactly once at `reset()`; no double-free with `CASInt`'s own destructor (registry entries must be ownership-transferred, i.e. the arena-resident copy's dtor must NOT run — it doesn't — and the stack-side original must keep self-managing).
- **Implementation outline**: option A (wire registry): when a `CASInt` is copied into an arena node, deep-copy the mpi via `allocBig` and `registerBigInt(theCopy)`; option B (prohibition): make arena `SymNum` demote/reject promoted values (turn into Floating with `approximate` flag) — simpler, loses exactness beyond 2^63 in CAS steps only. Decide with the owner; A is correct, B is cheap.
- **Acceptance tests**: host-ported BigInt suite (`tests/BigIntTest.h` — requires the P-01 host port, which this ticket should piggyback on for the mpi-relevant cases with a libc mbedtls or a stub) + on-device solve loop with MT-01 probes flat across 100 iterations.
- **Risk**: medium-high — ownership semantics in a hot math path; native builds have no BigInt so emulator CI cannot see regressions.
- **Rollback**: revert; leak returns (status quo).
- **Effort**: M-L.
- **Model-safe**: **Fable/Opus with tests-first**; not Sonnet.

## MT-13 — Giac guard rails  *(P3)*

- **Goal**: bound the blast radius of a single `solveWithGiac` call: pre-flight PSRAM check, input length cap, stack high-water logging; document the no-cap reality.
- **Touched files**: `src/math/giac/GiacBridge.cpp:389-440` only (bridge is the single entry point — keep it that way).
- **Memory contract**: refuse evaluation when `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < 512 KB` (tunable) or input > 4 KB chars; log stack HW before/after; behavior otherwise unchanged. `lib/giac/**` stays vendored-frozen (repo rule).
- **Acceptance tests**: firmware compiles; Giac has zero automated tests (ground-truth P-02) — manual serial spot-checks: normal queries unchanged; an over-long input returns the refusal string; artificial PSRAM pressure (scratch allocation) triggers the guard.
- **Risk**: low (guard clauses at one entry point, inside existing try/catch).
- **Rollback**: remove guards.
- **Effort**: S.
- **Model-safe**: **Sonnet-safe to write**; meaningful verification is manual/HIL until P-02 exists.

## MT-14 — Flash diet: legacy menu path + icons (+ decide `src/lua`)  *(P3)*

- **Goal**: recover flash headroom (79.4 % used): remove the legacy non-LVGL menu render path and its ~80 KB `Icons.h` bitmaps; execute the already-flagged `src/lua` decision (28.8 k lines compiled for a stubbed `LuaVM`, ground-truth P-06).
- **Touched files**: `src/SystemApp.cpp` (legacy `_apps` vector + `renderMenu`/`drawStatusBar` legacy paths `:161-179,347-440`), `src/ui/Icons.h` (delete or shrink), `src/apps/LuaVM.*` + `src/lua/**` (delete or wire — owner decision), `platformio.ini` only if exclusion via filter is chosen.
- **Memory contract**: flash −80 KB (icons) − (lua object code, measure from map); zero RAM change expected (all flash-resident); no runtime path change (legacy menu is not the LVGL launcher).
- **Acceptance tests**: firmware builds; flash % drops (record before/after from `pio run` output); emulator CI fully green (emulator never compiled these); on-device boot + menu navigation unchanged.
- **Risk**: medium — "legacy — kept for compatibility" code (`SystemApp.h:30`) may have hidden callers; grep-verify `renderMenu`/`icon_` references before deleting.
- **Rollback**: git revert.
- **Effort**: M.
- **Model-safe**: **Opus** (dead-code determination), with owner sign-off on the lua decision.

---

## Priority summary

| Order | Ticket | Risk closed | Effort | Model |
|---|---|---|---|---|
| 1 | MT-01 probes | MEMX-10 detection, enables all sizing | S | Sonnet |
| 2 | MT-02 LVGL heap ratification | MEMX-01 | S | Sonnet (+human size call) |
| 3 | MT-03 splash delete | MEMX-01 (leak) | S | Opus |
| 4 | MT-04 lazy arenas | MEMX-11 | S | Sonnet |
| 5 | MT-05 arena/pool hardening | MEMX-05/06/07 | M | Opus |
| 6 | MT-06 OOM-safe begin/end | MEMX-08 | S-M | Sonnet + HIL check |
| 7 | MT-07 de-String Token | MEMX-10 | M | Opus |
| 8 | MT-08 step-widget cap | MEMX-01 | M | Opus |
| 9 | MT-09 ASAN job | MEMX-13 (partial) | M | Sonnet, owner-gated CI |
| 10 | MT-10 allocator ratification | MEMX-02 | S | Sonnet |
| 11 | MT-11 staging reclaim | MEMX-03 | S | HIL-gated |
| 12 | MT-12 BigInt lifetime | MEMX-04 | M-L | Fable/Opus |
| 13 | MT-13 Giac guards | MEMX-09 (partial) | S | Sonnet + manual verify |
| 14 | MT-14 flash diet | MEMX-14 | M | Opus + owner |
