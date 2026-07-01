# NumOS — Phase MEM-A implementation note (MT-01…MT-04)

> Companion to `NUMOS_MEMORY_IMPLEMENTATION_TICKETS.md`. Records what Phase
> MEM-A actually changed, how to read the new diagnostics, and what was
> deliberately left alone. Base: `main` @ `f054021`.

## MT-01 — Memory probes (`src/utils/MemProbe.h`)

One macro, one flag:

```c++
#include "utils/MemProbe.h"
NUMOS_MEM_PROBE("tag");          // firmware: one serial line; native: no-op
// Disable all probes: -DNUMOS_MEM_PROBE_ENABLE=0
```

Line format (grep-stable, one line per probe, no heap use):

```
[MEM] <tag> int=<free>/<largest> psram=<free>/<largest> low=<intLow>/<psramLow> lvgl=<used%>/<max%> stack=<words>
```

- `int`/`psram`: `heap_caps_get_free_size` / `heap_caps_get_largest_free_block`
  for `MALLOC_CAP_INTERNAL` / `MALLOC_CAP_SPIRAM`. A largest-block much
  smaller than free = fragmentation (policy §5.4 alarm: SPIRAM largest < 1 MB
  with > 4 MB free).
- `low`: `heap_caps_get_minimum_free_size` lifetime low-water for both domains.
- `lvgl`: builtin 64 KB pool via `lv_mem_monitor()` — current used % / pool
  high-water % (`max_used`). Omitted when LVGL runs the CLIB allocator
  (emulator). LVGL 9.5 offers no finer high-water hook.
- `stack`: `uxTaskGetStackHighWaterMark(nullptr)` of the calling task, in
  words — minimum headroom ever observed; watch it around `giac-*` tags.

Probe points (lifecycle boundaries only): `boot` (end of `setup()`),
`hb <t>s` (5 s heartbeat, replaces the old internal-only `[HB]` line),
`enter id=<n>` (`SystemApp::launchApp` before `load()`), `exit mode=<n>`
(end of `teardownModeNow` — covers deferred and immediate teardown),
`giac-pre`/`giac-post`/`giac-post-ex` (around `solveWithGiac`).

Leak canary protocol: open/close the same app twice; the two `exit` lines
must report identical `psram` free.

## MT-02 — LVGL heap ratified + visible OOM (`src/lv_conf.h`)

- The dead LVGL-8 `LV_MEM_CUSTOM` block (512 B internal/PSRAM cutoff — never
  read by LVGL 9, audit §1.1) is replaced by explicit LVGL-9 keys:
  `LV_USE_STDLIB_MALLOC = LV_STDLIB_BUILTIN`, `LV_MEM_SIZE = 64 KB`,
  `LV_MEM_POOL_EXPAND_SIZE = 0` — all `#ifndef`-guarded so the emulator's
  `-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB` and scratch `-DLV_MEM_SIZE=16384`
  builds still override. **The pool size is unchanged (64 KB), now on
  purpose**; raising it (≤ 96 KB) is a human decision after MT-01 data.
- `LV_ASSERT_HANDLER` replaces the silent `while(1)` hang: prints the failing
  LVGL file:line + OOM hint to stdout (UART0 on firmware), then `abort()` —
  panic backtrace decodable by `monitor_filters = esp32_exception_decoder`;
  fast CI failure instead of a timeout on the emulator.
- Documented limitation: LVGL 9.5 exposes pool high-water only through
  `lv_mem_monitor().max_used` (surfaced by MT-01); no per-allocation hook.

Scratch verification of the OOM path: build with `-DLV_MEM_SIZE=16384` — boot
must print the `[LVGL] ASSERT FAILED …` line and panic instead of freezing.

## MT-03 — Splash screen deletion

`SplashScreen::destroy()` (new): null-safe, double-delete-safe, and refuses
to delete while the splash is still the active screen. Lifetime order is the
same contract as deferred app teardown — the next screen's 200 ms FADE_IN
animation must finish first:

- Firmware (`main.cpp`): after `g_app.begin()` (menu loaded), pump
  `lv_timer_handler()` for 250 ms, then `g_splash.destroy()`.
- Emulator (`NativeHal.cpp`): `g_splashTeardownPending` deferred by the same
  `TEARDOWN_DELAY_MS` (260 ms) on the same `lv_tick` clock as Phase 9F —
  deterministic in CI.

Reclaims the previously leaked ~1 KB + 3 objects from the fixed 64 KB pool.
No visual change (deletion happens after the menu is fully faded in).

## MT-04 — Lazy first arena block (`src/math/cas/SymExprArena.h`)

Constructor no longer eagerly allocates block 0; `allocRaw()` allocates it on
first use. Effect: ~288 KB PSRAM no longer pinned at boot by the constructors
of CalculationApp (`_eduArena` 64 KB), EquationsApp (`_stepsTutorArena` +
`_arena` 128 KB), CalculusApp (`_arena` 64 KB), NeoLanguageApp (`_symArena`
32 KB). Contract preserved: failure still surfaces as `nullptr` from
`allocRaw`/`create` (previously a failed eager block was silently ignored by
the ctor and failed at first use anyway); `reset()` on a never-used arena is
a no-op; `totalAllocated()` now reads 0 until first use (its only consumer,
NeoLanguageApp's 70 % soft-reset check, already guards `capacity > 0`).

Deliberately NOT lazified: `NeoArena` (already lazy, `NeoAST.h`),
`CasMemoryPool` (already lazy `unique_ptr` in EquationsApp), sim/render
buffers (session-scoped in `begin()`/`end()`, not boot-pinned).

## Deliberately unchanged (out of MEM-A scope)

`platformio.ini` (lv_conf.h was the correct config location); LVGL widget and
font flags; the `allocRaw` oversize guard, bulk internal-fallback removal and
`CasMemoryPool` overflow accounting (MT-05); OOM-safe sim `begin()`/`end()`
(MT-06); the dead 32 KB DMA staging buffer (MT-11, HIL-gated); CAS/Giac
semantics, `lib/giac`, renderer/parser/evaluator, goldens/masks, CI
workflows.

## On-device follow-up (human)

Boot log should show `[MEM] boot` with psram free higher by ~288 KB vs the
pre-MEM-A build; open/close each app twice and compare `exit` lines; run one
heavy Giac query and check `giac-pre`→`giac-post` deltas and `stack`.
