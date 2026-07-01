# NumOS — Memory Allocation Policy (normative)

> Rules for where memory MUST/MUST NOT come from in NumOS on ESP32-S3 N16R8. Derived from the verified state in `NUMOS_MEMORY_AND_PSRAM_AUDIT.md` (same date/commit). Words per RFC 2119. Every rule cites the mechanism it constrains.
>
> Scope note: today the de-facto policy is set by the global `operator new` override (`src/math/giac/GiacAlloc.cpp:24-66`): **all C++ `new` is PSRAM-first**. This document ratifies that as intentional policy (§2.3) rather than an accident of the Giac port, and adds the rules that the override does not cover.

## 1. Domain assignment rules

### 1.1 MUST stay in internal RAM (MALLOC_CAP_INTERNAL)

| What | Why | Anchor |
|---|---|---|
| LVGL draw buffer — single, 32 KB, `MALLOC_CAP_INTERNAL\|MALLOC_CAP_DMA` | TFT_eSPI SPI-DMA on S3 cannot source PSRAM (StoreProhibited); double-buffer deadlocks LVGL 9 in blocking mode | `main.cpp:131-150` |
| Any future DMA descriptor/buffer (SPI, I2S, RMT) | hardware requirement | — |
| FreeRTOS task stacks (loopTask 64 KB; any `xTaskCreate`) | tasks stacks must be internal unless explicitly created PSRAM-capable; treat as internal always | `platformio.ini:44`; `FractalApp.cpp:490-498` |
| ISR-touched state, flush-path state | cache-safety and latency | `DisplayDriver.cpp:219-347` |
| The LVGL heap itself (today: 64 KB builtin pool in `.bss`) | render-path latency; PSRAM-backed LVGL heaps caused the historical black-screen class of bugs when combined with DMA assumptions | audit §1.1 |

### 1.2 MUST go to PSRAM (MALLOC_CAP_SPIRAM)

| What | Why | Anchor |
|---|---|---|
| All pixel/render buffers ≥ 8 KB (sim canvases, Grapher Kandinsky, Neo plot) | 90–230 KB each; internal cannot host them | `GrapherApp.cpp:564-567`, `ParticleLabApp.cpp:106-108`, etc. |
| CAS arenas (`SymExprArena`, `CasMemoryPool`, `NeoArena`) and their blocks | bulk, bursty, session-scoped | `SymExprArena.h:204-207`, `CasMemory.h:324-335`, `NeoAST.h:150-156` |
| MathAST nodes, SymExpr nodes, step logs, history entries | many small long-ish-lived blocks; keeping them out of internal RAM is the whole point | `MathAST.cpp:50-60`, `CASStepLogger.h:108` |
| Giac working set | unbounded; must never fall on internal first | via global override, `GiacAlloc.cpp:27-31` |
| Python/NeoLang interpreter heaps | 1 MB / up to 1 MB | `PythonEngine.cpp:92-98` |
| Large in-object buffers (undo rings, preview atlases) — via their owning app object | app objects are `new`ed ⇒ PSRAM ⇒ their arrays ride along; keep it that way (do NOT make such apps static/global) | `CircuitCoreApp.h:202-212`, `FractalApp.h:94` |

### 1.3 MUST NEVER use PSRAM

- The LVGL draw buffer and any DMA-fed buffer (`main.cpp:133-134` rationale).
- Task stacks.
- Anything read/written from an ISR or during flush.
- Mutex/queue/FreeRTOS kernel objects.

### 1.4 Fallback discipline

Current code falls back from `MALLOC_CAP_SPIRAM` to `MALLOC_CAP_8BIT` (= may take internal) in: `GiacAlloc.cpp:30`, `MathAST.cpp:54`, `PSRAMAllocator.h:78`, `SymExprArena.h:206-208`, `CasMemory.h:328-330`, `CASInt.h:452-454`, `NeoAST.h:152-153`. Policy:

- **Small nodes (≤ 256 B): fallback to internal is ACCEPTABLE** (keeps math usable under PSRAM pressure; bounded harm).
- **Bulk blocks (≥ 4 KB: arena blocks, pool slabs, pixel buffers): fallback to internal is FORBIDDEN.** A 64 KB arena block landing in internal RAM (because Giac ate PSRAM) would destroy the internal heap. Today `SymExprArena.h:206-208` and `CasMemory.h:328-330` violate this — see ticket MT-05. `utils::PSRAMBuffer` (`MemoryUtils.h:70`) is the compliant pattern: SPIRAM-only, fail cleanly, caller handles `false`.
- Every bulk allocation MUST handle failure without crashing (Fluid2D/ParticleLab/NeuralLab already return-early; keep it that way, but see cleanup rule §4.2).

## 2. Allocator selection rules

### 2.1 When to use arenas

Use a session arena (`SymExprArena` / `NeoArena` pattern) when: (a) many allocations < ~1 KB, (b) a single well-defined end-of-session, (c) no destructors with side effects. Rules:

- Arena-placed types MUST be trivially destructible or side-effect-free on destruction. **`SymNum` holding a promoted `CASInt` violates this today** (leak on `reset()`, audit §4.4). Until fixed, arena users MUST NOT store BigInt-promoted values in arena nodes, or MUST wire `registerBigInt` (`SymExprArena.h:110-116`).
- `create<T>()` returns nullptr on exhaustion (`SymExprArena.h:101-106`) — every call site MUST null-check or the operation MUST be aborted wholesale.
- No pointer into an arena may survive `reset()` (`CasMemory.h:300-304` contract). Enforce by scoping: reset only in `end()` / between top-level operations, never mid-pipeline.
- Requests larger than the block size MUST be rejected or specially handled (only `NeoArena` does this correctly today, `NeoAST.h:146-149`).

### 2.2 When to use static/fixed buffers

Prefer fixed in-object arrays when the bound is small and known (the Stats/Prob/Seq/Regression pattern, ≤ 640 B; `MnaMatrix` fixed dims; `OpticsEngine` fixed rays). This is the least fragmenting option and self-documents worst case. Do NOT convert these to vectors "for flexibility".

### 2.3 Default C++ heap

The global override (PSRAM-first, any-heap fallback) is **the ratified default** for firmware. Rules:

- Do not remove the per-class overrides (`MathAST.cpp:50-69`) or `PSRAMAllocator` "because redundant": they are the only routing when `NUMOS_USE_GIAC` is off and on native builds.
- Any code that genuinely needs internal RAM MUST use `heap_caps_malloc(MALLOC_CAP_INTERNAL…)` explicitly — plain `new` will never reliably give internal RAM again.
- `GiacAlloc.cpp`'s `delete` uses `free()` (`GiacAlloc.cpp:48-51`) — acceptable on ESP-IDF only; never port this file as-is.

### 2.4 Arduino `String` / C malloc

`String` and other `malloc` users land internal for < 4 KB (sdkconfig `ALWAYSINTERNAL=4096`). Rules: no `String` in per-frame/update paths; no `String` in long-lived caches (the `Token::text` case, `Tokenizer.h:59`, is grandfathered but is the designated fragmentation hotspot — ticket MT-07). Prefer `std::string` (PSRAM via override) or fixed `char[]`.

## 3. LVGL heap rules (the 64 KB pool)

1. The pool is fixed and shared; **every widget allocation is a claim against 64 KB.** Budget guideline: persistent baseline (menu + splash-fix + statusbar) ≤ 20 KB; any single app's tree ≤ 32 KB; leave ≥ 12 KB slack for anims/timers/draw tasks.
2. Per-step/per-item widget growth MUST be capped (e.g. `_stepRenderers` — cap steps rendered as widgets; virtualize long lists via `lv_table`/recycling instead of N labels).
3. Never `lv_obj_delete` synchronously on app exit (deferred-teardown contract, `SystemApp.cpp:229-240`); every `end()` MUST delete its screen (StatusBar relies on parent deletion, `StatusBar.cpp:152-165`) and null all pointers.
4. Screens that are replaced MUST be deleted — fix the splash leak (`SplashScreen.cpp:51-83`, ticket MT-03).
5. Raising `LV_MEM_SIZE` (via `-DLV_MEM_SIZE=…` build flag, since `lv_conf.h`'s block is dead) is allowed up to 96 KB if instrumentation (§5) shows exhaustion; going PSRAM-backed (`LV_MEM_ADR`/pool-alloc hooks) requires hardware validation of render latency and is a separate decision — do not do it casually.

## 4. App lifecycle memory rules

1. **Constructor: allocate nothing big.** Constructors run for all 20 apps at boot (`SystemApp.cpp:104-123`). Arenas violate this today (~288 KB eager, audit §4.1) — target state: arenas allocate their first block lazily on first use (ticket MT-04). New apps MUST NOT add eager buffers in constructors.
2. **`begin()`/`load()`: allocate; `end()`: free everything dynamic.** Re-entry MUST re-allocate cleanly (the sim apps already conform).
3. **Failure-path rule: `end()` MUST be safe to call after a partially failed `begin()`**, and `begin()` MUST free already-acquired buffers before returning on failure. Never gate frees on `_screen` when allocations precede `_screen` creation (violations: `Fluid2DApp.cpp:158-226`, `BridgeDesignerApp.cpp:105-154`, `NeuralLabApp.cpp:133-136` — ticket MT-06).
4. **`end()` MUST also**: clear step logs (`steps.clear()` / `StepVec`), reset arenas, delete the screen (which deletes StatusBar children), null all LVGL pointers.
5. Transient buffers inside one operation MUST be RAII (`PSRAMBuffer` scratch in `FractalApp.cpp:818-819` is the model).
6. Background tasks MUST be stopped and joined before freeing buffers they touch (`FractalApp.cpp:513-540` pattern: flag + timeout + forced delete).

## 5. Fragmentation avoidance

1. Bulk blocks (≥ 32 KB) SHOULD be allocated early in a session and freed together (arena pattern) — interleaving small/large SPIRAM allocations with different lifetimes is the fragmentation driver.
2. Keep the "one big buffer per app, allocated in begin(), freed in end()" shape for sims — it is fragmentation-proof by construction.
3. Long-lived small allocations (history, variables) SHOULD be bounded (history is: 50 entries) and ideally pooled.
4. Monitor with largest-free-block ratio (§6); a `SPIRAM` largest-block < 1 MB with > 4 MB free = fragmentation alarm.
5. Do not add new global caches that grow monotonically (the Giac context is the grandfathered exception).

## 6. Mandatory instrumentation (target state)

Add a single header `src/utils/MemProbe.h` (ticket MT-01) providing:

```c++
// Log line format (grep-stable):
// [MEM] <tag> int=<free>/<largest> psram=<free>/<largest> lvgl=<used>/<max_used> stackHW=<words>
#define NUMOS_MEM_PROBE(tag) numos::memProbe(tag)
```

implemented with `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`, `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`, same for `MALLOC_CAP_SPIRAM`, `lv_mem_monitor()` (builtin pool only), `uxTaskGetStackHighWaterMark(nullptr)`, and `heap_caps_get_minimum_free_size` for lifetime low-water. Probe points (MUST): end of `setup()`; `SystemApp::launchApp` before `load()`; end of each `end()` (i.e. app-exit steady state); before/after `solveWithGiac`; heartbeat (replacing the internal-only line at `main.cpp:233-236`). Emulator builds compile the probe to a no-op (`#ifndef ARDUINO`).

On-device measurement session (manual, serial): boot → note `[MEM] boot`; open/close every app twice (leak check: second-exit numbers must equal first-exit); run a heavy Giac query; run Python + return; watch `psram largest` degradation. CI cannot execute this (no hardware; emulator allocator differs) — CI's role is compile-time only plus emulator ASAN for lifecycle logic (ticket MT-09).

## 7. Review checklist (for any PR touching memory)

- [ ] New allocation: which of the 6 mechanisms (audit §2) does it hit? Is the resulting domain the intended one?
- [ ] Bulk (≥ 4 KB): no internal fallback; failure handled; freed in `end()`; safe on double-`end()`.
- [ ] Arena data: trivially destructible? survives no `reset()`?
- [ ] Widgets: counted against the 64 KB pool budget? unbounded per-item widget creation?
- [ ] No new `String` in update paths; no new global mutable caches.
- [ ] Probes added around any new ≥ 32 KB allocation site.
