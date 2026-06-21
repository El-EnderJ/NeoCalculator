# NumOS SDL2 Desktop Emulator — Quickstart

How to build and run the **native desktop emulator** (`pio run -e emulator_pc`) on
Windows and Linux. This is a desktop *port* of NumOS (the real LVGL/UI/math code
compiled against an SDL2 desktop HAL) — **not** a cycle-accurate ESP32 emulator.

This quickstart covers **Phase 2** (reproducible, portable build & run with no
machine-specific SDL2 paths), **Phase 3A** (stable 320×240 logical scaling,
key press/release, and a headless auto-exit mode for CI — see
[Run modes](#run-modes-phase-3a) and [Keyboard map](#keyboard-map-phase-3a)),
**Phase 3B** (a deterministic fixed-step tick and a dependency-free PPM screenshot
dump for reproducible CI artifacts — see
[Deterministic mode & screenshots](#deterministic-mode--screenshots-phase-3b)), and
**Phase 4A** (deterministic scripted input replay — `.numos` scripts that inject
keys through the existing dispatch path and capture screenshots, see
[Scripted input replay](#scripted-input-replay-phase-4a)).

> The firmware build is completely separate and unchanged:
> `pio run -e esp32s3_n16r8`. Nothing here affects it.

---

## TL;DR

```bash
# Windows (PowerShell)
pio run -e emulator_pc
./scripts/run-emulator-windows.ps1

# Linux
sudo apt-get install -y libsdl2-dev
PLATFORMIO_BUILD_DIR=.pio/build pio run -e emulator_pc
./scripts/run-emulator-linux.sh

# Timed / headless smoke (auto-exit; no manual window close — see Run modes)
./scripts/run-emulator-windows.ps1 --headless --run-for-ms 5000 --quiet          # Windows
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh --run-for-ms 5000 --quiet   # Linux
```

Diagnose your setup any time:

```bash
python scripts/check-emulator-deps.py
```

---

## How SDL2 is discovered (no hardcoded paths)

SDL2 include/library/link flags are injected at build time by
[`scripts/sdl2_env.py`](../scripts/sdl2_env.py), a PlatformIO `pre:` extra_script
wired **only** into `[env:emulator_pc]`. There are no `-IC:/SDL2/...` paths in
`platformio.ini` anymore.

| OS | Discovery method |
|:--|:--|
| **Windows** | An SDL2 *development* root (folder containing `include/`, `lib/`, `bin/`). Resolved from `NUMOS_SDL2_ROOT` → `SDL2_DIR` → `SDL2_ROOT` → the historical default `C:/SDL2/x86_64-w64-mingw32`. Links `-lmingw32 -lSDL2main -lSDL2`. |
| **Linux / macOS** | System SDL2 via `pkg-config sdl2`, falling back to `sdl2-config`, falling back to a bare `-lSDL2`. |

At runtime the **Windows** binary additionally needs `SDL2.dll`;
[`run-emulator-windows.ps1`](../scripts/run-emulator-windows.ps1) finds it and
extends `PATH` for the child process only (it never edits your global `PATH`).

---

## Windows

### Prerequisites
- **PlatformIO Core** (`pip install -U platformio`).
- **A C++ compiler** — MinGW-w64 GCC (the project is built/tested with
  `x86_64-...-posix-seh` GCC).
- **SDL2 MinGW development package** — download `SDL2-devel-2.x.y-mingw` from
  <https://github.com/libsdl-org/SDL/releases> and extract it. You want the
  `x86_64-w64-mingw32` subfolder (it contains `include/`, `lib/`, `bin/`).

  > ABI note: use a **MinGW** SDL2 dev package (not the MSVC one). The project's
  > known-good combo is the SDL2 `x86_64-w64-mingw32` prebuilt with MinGW-w64 GCC.

### Point the build at SDL2
Either drop it at the default location `C:\SDL2\x86_64-w64-mingw32`, **or** set an
environment variable (per-shell):

```powershell
$env:NUMOS_SDL2_ROOT = 'C:\path\to\SDL2-2.30.x\x86_64-w64-mingw32'
```

`SDL2_DIR` / `SDL2_ROOT` work too.

### Build & run
```powershell
pio run -e emulator_pc
./scripts/run-emulator-windows.ps1
```

The run script locates the executable (honoring the project's custom `build_dir`,
see [Build directory](#build-directory)), resolves `SDL2.dll`
(next to the exe → SDL2 root → `PATH`), then launches. Pass-through args are
forwarded: `./scripts/run-emulator-windows.ps1 <args>`.

---

## Linux

### Prerequisites
- **PlatformIO Core** (`pip install -U platformio`).
- **g++** (or clang++).
- **SDL2 dev package:**
  - Debian/Ubuntu: `sudo apt-get install -y libsdl2-dev`
  - Fedora: `sudo dnf install -y SDL2-devel`
  - Arch: `sudo pacman -S sdl2`

### Build & run
```bash
PLATFORMIO_BUILD_DIR=.pio/build pio run -e emulator_pc
./scripts/run-emulator-linux.sh
```

Why `PLATFORMIO_BUILD_DIR=.pio/build`? See [Build directory](#build-directory).

The run script locates the binary, sanity-checks the SDL2 runtime (`ldd` /
`ldconfig`), then runs it.

> **Status:** the Linux path is designed and the discovery logic is in place, but
> a Linux build has **not** been executed on the author's machine (Windows-only).
> The additive CI workflow
> ([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml))
> is the mechanism that will actually prove the Linux build; treat Linux as
> "expected to work, pending first green CI run" until then.

### macOS
Untested/future. `brew install sdl2` provides `pkg-config`/`sdl2-config`, so the
same discovery path *should* apply; no macOS guarantee is made.

---

## Run modes (Phase 3A)

By default the emulator is interactive (the window stays open until you close
it). Phase 3A adds a few command-line flags — **emulator-only; the firmware is
unaffected** — mainly so CI (or you) can launch it briefly without a human:

| Flag | Effect |
|:--|:--|
| `--frames N` | Run N main-loop iterations, then exit cleanly (liveness / no-crash smoke). |
| `--run-for-ms N` | Run ~N ms of wall-clock time, then exit cleanly. Prefer this for "did it reach the menu" checks — the splash is ~2 s, so use ≥ 4000. |
| `--scale N` | Integer window scale 1..8 (default 2). The logical surface is always 320×240; SDL scales it with crisp integer scaling. |
| `--headless` | Force `SDL_VIDEODRIVER=dummy` (no visible window) for CI / no-display hosts. You can also `export SDL_VIDEODRIVER=dummy` yourself. |
| `--quiet` | Silence the per-key / per-iteration debug logs (lifecycle logs remain). |
| `--deterministic` | **(Phase 3B)** Drive LVGL from a synthetic fixed-step clock instead of the wall clock. Animation/timer state becomes a function of the *frame index*, so a given `--frames N` is reproducible across runs and machines. Pair it with `--frames N`. See [Deterministic mode & screenshots](#deterministic-mode--screenshots-phase-3b). |
| `--step-ms N` | **(Phase 3B)** Virtual milliseconds advanced per frame in `--deterministic` mode, 1..1000 (default 16 ≈ 60 fps). No effect without `--deterministic`. |
| `--screenshot P` / `--dump-frame P` | **(Phase 3B)** After the final frame (before shutdown), dump the **320×240 logical** framebuffer to PPM (P6) at path `P`. Dependency-free; works under `--headless` and at any `--scale`. |
| `--script P` | **(Phase 4A)** Replay a deterministic input script (`.numos`) from path `P`: inject key events through the same dispatch path as live SDL input and capture in-script screenshots. Pair with `--deterministic --frames N`. See [Scripted input replay](#scripted-input-replay-phase-4a). |
| `--help` | Print usage and exit. |

The run scripts forward all flags, e.g.:

```bash
# Windows
./scripts/run-emulator-windows.ps1 --headless --run-for-ms 5000 --quiet
# Linux
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh --run-for-ms 5000 --quiet
```

A healthy timed run prints a `[SCALE] …` geometry line, reaches
`[MENU] Launcher cargado`, then `[SIM] auto-exit: …` and `[SIM] Bye!` (exit 0).
That is exactly what the additive CI workflow asserts
([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml)).

### Display scaling

The renderer presents a fixed **320×240 logical** surface
(`SDL_RenderSetLogicalSize`) with **integer scaling**
(`SDL_RenderSetIntegerScale`) and nearest-neighbor sampling, so pixels stay
crisp at ×2/×3/×4. This is *output* scaling only — the device geometry and the
math renderer are untouched. The startup `[SCALE]` log reports logical size,
window size, renderer output size, scale factor, integer-scale state, and the
selected backend (e.g. `direct3d(accel)` or `software`).

### Deterministic mode & screenshots (Phase 3B)

Phase 3B adds a **reproducible** headless mode plus a **screenshot** dump, so CI
can produce a stable visual artifact. Both are **emulator-only** and **opt-in**;
without the flags the runtime is byte-for-byte the Phase 3A behavior.

```bash
# Deterministic headless run that writes a 320x240 PPM screenshot
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh \
  --headless --frames 600 --deterministic --screenshot smoke.ppm --quiet   # Linux
./scripts/run-emulator-windows.ps1 `
  --headless --frames 600 --deterministic --screenshot smoke.ppm --quiet   # Windows
```

- **Deterministic tick.** Normally LVGL reads the wall clock
  (`lv_tick_set_cb(SDL_GetTicks)`), so the same `--frames N` samples animations at
  different real-time instants each run. With `--deterministic`, LVGL instead reads
  a synthetic counter the loop advances by `--step-ms` (default **16 ms**) **per
  frame**, *before* `lv_timer_handler()`. The visual state then depends only on the
  frame index — reproducible across runs/machines, with no `SDL_Delay`/VSYNC jitter
  (the per-frame sleep is skipped in this mode). The boot log states which clock is
  active: `[LVGL] lv_init() OK — tick = determinista (paso fijo por frame)` vs
  `… = SDL_GetTicks() (reloj de pared)`. At 16 ms/frame the ~2 s splash finishes by
  ~frame 125, so `--frames 600` lands well inside the launcher.
- **Screenshot.** `--screenshot PATH` (alias `--dump-frame PATH`) dumps the final
  frame just before shutdown. The source is the CPU-side full-screen LVGL buffer
  (`g_lvBuf`, RGB565, `LV_DISPLAY_RENDER_MODE_FULL`), so the image is always the
  **exact 320×240 logical display** — independent of `--scale` and valid under
  `--headless`. Output is **PPM P6** (a 3-line ASCII header + raw RGB), chosen to
  avoid any new dependency (no PNG/libpng). RGB565 is expanded to RGB888 by bit
  replication. A success line prints `[SHOT] PPM 320x240 escrito: <path>`.

> View a PPM with most image tools (GIMP, ImageMagick `display`/`convert`, IrfanView
> with plugins). To convert to PNG: `convert smoke.ppm smoke.png`.

**CI artifacts.** The additive emulator workflow
([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml))
runs this exact deterministic command and uploads, under the
**`numos-emulator-smoke`** artifact, both the run log (`emu-det-smoke.log`) and the
screenshot (`smoke.ppm`). Download it from the workflow run's *Artifacts* section.
The job only asserts the launcher was reached, the deterministic tick was used, and
the PPM exists and is 320×240 — it makes **no pixel-correctness claim yet** (that is
a later phase).

**Limitations.** Determinism covers LVGL's tick-driven animation/timer state; it
does not freeze any wall-clock reads elsewhere (there are none on the render path
today). The screenshot is the logical framebuffer, not the scaled host window. The
`--screenshot` flag captures once at exit; the **Phase 4A** `screenshot` script
command (below) adds per-frame capture during a scripted run.

### Scripted input replay (Phase 4A)

Phase 4A adds `--script PATH`: a deterministic, line-oriented **`.numos`** script
that injects key events and captures screenshots. Replayed keys go through the
**exact same dispatch path** as live SDL input (`dispatchKey` in
[`NativeHal.cpp`](../src/hal/NativeHal.cpp)) — MENU navigation and CalculationApp
typing behave identically to a human at the keyboard. Like Phase 3B this is
**emulator-only** and **opt-in**; without `--script` the runtime is unchanged.

```bash
# Type "1+2", evaluate, screenshot — headless, deterministic, reproducible.
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh \
  --headless --deterministic --script tests/emulator/scripts/calc_1_plus_2.numos \
  --frames 1400 --screenshot out/calc_1_plus_2.ppm --quiet                    # Linux
./scripts/run-emulator-windows.ps1 `
  --headless --deterministic --script tests/emulator/scripts/calc_1_plus_2.numos `
  --frames 1400 --screenshot out/calc_1_plus_2.ppm --quiet                    # Windows
```

**Script syntax.** One command per line; tokens are whitespace-separated;
commands and key names are case-insensitive; paths are case-preserved and may not
contain spaces.

| Line | Meaning |
|:--|:--|
| `# ...` | Comment (whole line ignored). Blank lines ignored too. |
| `wait N` | Idle `N` emulator frames (`N ≥ 0`). At `--step-ms 16`, `N` frames ≈ `16·N` ms of virtual time. |
| `key NAME` | Inject a PRESS+RELEASE pulse of `NAME` (the normal "tap a key"). |
| `keydown NAME` / `keyup NAME` | Inject only the press / only the release. |
| `screenshot PATH` | Dump a 320×240 PPM at `PATH`, captured *after* this frame's render. |
| `log "message"` | Print `message` to stdout (handy for CI log markers). |
| `open_app NAME` | (Phase 5A) Launch an app directly by name through the same `launchApp()` the launcher uses — deterministic, no fragile grid navigation. `NAME` ∈ `Calculation`/`Settings`/`MathShowcase` (aliases `Calc`, `Showcase`/`Math_Showcase`). Use it from the launcher (MENU) state. |
| `assert_app NAME` | (Phase 4B-C / 5A) Assert the active app is `NAME` ∈ `Calculation`/`Menu`/`Splash`/`Settings`/`MathShowcase` (aliases `Calc`/`Launcher`/`Showcase`). |
| `assert_result TEXT` | (Phase 4B-C) Assert CalculationApp's *computed* result equals `TEXT` exactly (e.g. `3`, `5/6`). |
| `assert_result_contains TEXT` | (Phase 4B-C) Like `assert_result` but a substring match. |
| `assert_no_error` | (Phase 4B-C) Assert the last evaluated result has no error flag. |

**Key names** map onto existing calculator `KeyCode`s (no parallel key system):

- digits `0`–`9`; operators `+` `-` `*` `/`; `^` (= `POW`); `.` (= `DOT`); `(` `)`.
- `ENTER`, `LEFT`, `RIGHT`, `UP`, `DOWN`.
- `BACKSPACE` (alias `DEL`), `AC` (alias `ESC`), `HOME` (alias `MODE`).
- `X`/`Y` (variables), `SQRT`, `SIN`/`COS`/`TAN`, `LN`/`LOG`, `PI`, `E`, `NEGATE`.
- `FRAC` — **alias for `/`**: in NumOS the division key *is* the fraction key
  (`KeyCode::DIV` → `insertFraction`), so `key /` and `key FRAC` are equivalent and
  both open a fraction template in VPAM mode.

**Scheduling model.** In deterministic mode the replay executes **one command per
frame**, *before* SDL polling, so the injected key is visible to that same frame's
tick advance and `lv_timer_handler()`. `wait N` consumes `N` frames; `screenshot`
is captured *after* the frame's render (so it reflects everything injected so far).
The whole script is parsed and validated **up front** — a malformed script aborts
before any frame runs.

**Example script** (`tests/emulator/scripts/calc_1_plus_2.numos`):

```text
# wait for splash -> launcher (~frame 125 @ step-ms 16; 200 for margin)
wait 200
key ENTER          # open Calculation (centered, default-focused card)
wait 60
key 1
key +
key 2
key ENTER          # evaluate
wait 120
screenshot out/calc_1_plus_2.ppm
```

**Where screenshots go.** Wherever the `screenshot PATH` (or `--screenshot PATH`)
argument points; the bundled scripts write under `out/` (git-ignored). PPM P6,
320×240, same dependency-free format as Phase 3B.

**Error handling (fail-fast, non-zero exit).** A bad script never runs partway:

| Condition | Exit |
|:--|:--|
| Success (all commands ran; any assertions passed) | `0` |
| Unknown command, invalid key name, missing argument, negative `wait`, unknown `assert_app` name, unreadable script file | `2` |
| `screenshot` write failure at run time (bad path / disk full) | `3` |
| Assertion failure (`assert_*`) at run time (Phase 4B-C) | `4` |

Parse errors print `[SCRIPT] <file>:<line>: <reason>` to stderr; assertion failures
print `[ASSERT] <file>:<line>: FAIL - expected … got …` to stderr — both fail CI
loudly. Assertion diagnostics print regardless of `--quiet` (which only silences
per-frame noise).

**Determinism.** Two runs of the same scripted command produce **byte-identical**
PPMs (verified locally via SHA-256, and re-checked in CI by running
`calc_1_plus_2.numos` twice and comparing hashes).

**What Phase 4A does *not* do.** It proves scripted input is *deterministic and
reproducible*; it does **not** assert the pixels are *correct* (no OCR, no
golden-image diff). Confirming the *rendered pixels* read "`3`" is the golden flow
below; confirming the *computed value* is `3` is Phase 4B-C, next.

### Semantic assertions (Phase 4B-C)

Phase 4B-C lets a `.numos` script assert the value NumOS **actually computed**,
without OCR and without relying only on a golden image. The four `assert_*`
commands (see the syntax table above) read CalculationApp's evaluated result
through an **emulator-only, read-only** accessor (`debugLastResult()`, guarded by
`#ifdef NATIVE_SIM`), format the exact `vpam::ExactVal` to text
(`NativeHal::formatExactVal`: `3`, `5/6`, …), and compare. No pixels are parsed,
no renderer geometry is touched, and the firmware binary is unchanged (the
accessor is excised when `NATIVE_SIM` is undefined).

```text
# … type 1 + 2, press ENTER, let it settle …
key ENTER
wait 120
screenshot out/calc_1_plus_2.ppm   # capture FIRST, so the artifact exists even if an assert fails
assert_app Calculation             # active app is the calculator
assert_no_error                    # evaluation produced no error
assert_result 3                    # the computed value is exactly 3
```

Run it (exit 0 on pass, `4` on assertion failure):

```bash
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh \
  --headless --deterministic --script tests/emulator/scripts/calc_1_plus_2.numos \
  --frames 1400 --quiet
```

Each assertion prints `[ASSERT] <file>:<line>: PASS|FAIL - …` (the `actual='…'`
value is shown on both outcomes), regardless of `--quiet`. `tests/emulator/scripts/`
ships two asserting fixtures: `calc_1_plus_2.numos` (`== 3`) and
`calc_fraction_sum.numos` (`== 5/6`). CI runs both as a headless deterministic
**semantic smoke** (separate from the golden flow below).

**Semantic assertions vs. visual goldens — they are complementary, not redundant.**
`assert_result` proves the *math engine computed the right value*; it says nothing
about whether the 2D layout *draws* it correctly. A golden image proves the
*rendered pixels* match a human-accepted reference; it says nothing about
correctness unless a human vetted that reference. Use assertions for value
correctness (cheap, no human gate) and goldens for render correctness (human-gated).
**Current limitation:** assertions cover CalculationApp's `ExactVal` result only —
`formatExactVal` is exact for integers and pure fractions (what the fixtures
assert) and best-effort for radicals/π/e; there is no assertion of the *rendered*
output, and other apps are not covered.

### Candidate screenshots & golden comparison (Phase 4B-A)

Phase 4B-A adds the *machinery* for visual assertions: a way to generate
**candidate** screenshots, a dependency-free way to **compare** a candidate to an
accepted **golden**, and additive CI that uploads candidates and gates only when a
human-reviewed golden exists. It is tooling only — **no renderer, CAS, firmware,
or math-geometry change** — and it still makes **no semantic correctness claim** on
its own (that is a human decision at promotion time; automated result extraction is
Phase 4B-B).

**Generate candidates** (stdlib Python, Windows + Linux):

```bash
pio run -e emulator_pc
python scripts/generate-emulator-candidates.py
# -> out/emulator-candidates/launcher_smoke.ppm
#    out/emulator-candidates/calc_1_plus_2.ppm
#    out/emulator-candidates/calc_fraction_sum.ppm
#    out/emulator-candidates/settings_smoke.ppm        (Phase 5A)
#    out/emulator-candidates/math_showcase_smoke.ppm   (Phase 5A)
#    out/emulator-candidates/statistics_smoke.ppm      (Phase 6A)
#    out/emulator-candidates/probability_smoke.ppm     (Phase 6A)
#    out/emulator-candidates/statistics_data_smoke.ppm (Phase 6C; Phase 6D types {1,2,3})
#    out/emulator-candidates/probability_edit_smoke.ppm(Phase 6C; Phase 6D types mu=2.5; each 320x240, 230415 bytes)
```

**Deeper interaction smokes (Phase 6C, upgraded in Phase 6D).** Beyond the Phase 6A
default-open smokes, Statistics and Probability have scripts that drive the apps past
their default state with *real numeric entry*, proving they are *interactable*, not
merely launchable:

- `statistics_data_smoke.numos` types the dataset `{1, 2, 3}` into the value column —
  `1 ENTER`, then `AC DOWN 2 ENTER`, then `AC DOWN 3 ENTER` (the table starts with one
  row, `_numRows=1`; `AC` appends a row and `DOWN` moves to it) — and switches to the
  computed **Stats** tab with `GRAPH`. Reaching a computed tab from the Data tab requires
  the `GRAPH` key — on the Data tab `LEFT`/`RIGHT` are column navigation — so the emulator
  script-key table maps a `graph` token (`scriptNameToKeyCode`, `src/hal/NativeHal.cpp`,
  emulator-only, guarded by `#ifdef NATIVE_SIM` so it is firmware-neutral). The Stats tab
  then renders the 1-Var statistics via `switchTab` → `recompute()` →
  `updateStatsDisplay()`: over `{1,2,3}` that is `n=3, Mean=2, Median=2, Sum=6, Min=1,
  Max=3`.
- `probability_edit_smoke.numos` edits the focused `mu` field through the numeric
  editor: `ENTER` opens the editor (which pre-fills the buffer with the current value
  `0`), `DEL` clears it, `2 . 5` types the value, and `ENTER` commits via `finishEdit()`
  (`atof("2.5")` → `mu=2.5`) and `recompute()`s the PDF/CDF and bell curve, which centers
  at `2.5`.

> **Phase 6D fixed the numeric-entry firmware bug.** Both apps previously dropped every
> digit key: digit entry into StatisticsApp and ProbabilityApp used the range test
> `code >= KeyCode::NUM_0 && code <= KeyCode::NUM_9`, but `KeyCodes.h:68-77` declares the
> digits non-contiguously and out of order (NUM_7..NUM_9, then NUM_4..NUM_6, then
> NUM_1..NUM_3 and finally NUM_0 near `ENTER`), so `NUM_0 > NUM_9` and the range was
> always empty. The ordering-dependent companion arithmetic `'0' + (code - NUM_0)` was
> likewise unsafe. CalculationApp was immune only because it uses explicit per-digit
> `case` labels (`CalculationApp.cpp:326-335`). **Phase 6D** added an explicit,
> order-independent helper `keyCodeDigitValue(KeyCode)` to `src/input/KeyCodes.h` (a
> `switch` returning `0`–`9` or `-1`; no heap, no tables, host/firmware safe) and routed
> both apps' digit detection through it (`StatisticsApp.cpp`, `ProbabilityApp.cpp`).
> Digit keys now register, so the two scripts above type real values instead of working
> around the bug. The same broken range test still exists in four other firmware apps
> not built into the emulator — `CircuitCoreApp`, `MatricesApp`, `RegressionApp`,
> `SequencesApp` — and is left for a follow-up **Phase 6E** sweep (each would take the
> identical one-line `keyCodeDigitValue` substitution). Because these scripts now reach
> data-driven states, their candidate screenshots changed and **need fresh human-reviewed
> goldens** before any byte-exact comparison is meaningful.

Both new screens are **human-gated**: this phase makes **no golden claim** — the
candidates are for human review only, and no golden/mask is promoted automatically.
There are **no semantic numeric assertions** for Statistics/Probability yet (only the
`assert_app` smoke); adding one would require a read-only app-state accessor and is
deferred. A clock mask (shared rect `4,6,37,13`) is added by the human at promotion
time, as with the other app smokes.

It drives each bundled `.numos` script with `--headless --deterministic --script
--frames --screenshot`, writing one PPM per script into `out/emulator-candidates/`
(under git-ignored `out/`). `--bin` and `--out-dir` override the defaults.

**Compare** a candidate against an expected/golden PPM, byte-for-byte:

```bash
python scripts/compare-ppm.py EXPECTED.ppm ACTUAL.ppm [--write-diff diff.ppm]
```

It validates both are P6 320×240 maxval-255 with a full 230415-byte payload,
prints each file's dimensions / size / SHA-256, and on difference reports the
mismatching byte count, pixel count, and bounding box. `--write-diff` emits a PPM
that paints differing pixels magenta. Exit codes: **0** identical, **1** valid but
different, **2** invalid input. (Run it with the same file twice to validate a
single PPM's format — exit 0.)

**Masked comparison (Phase 4B-B).** Some screens are not byte-stable across
launches: the CalculationApp input cursor blinks (an `lv_timer` toggling every
500 ms), and the deterministic tick only pins the blink *phase within a single
run*, so spaced-apart launches differ inside a tiny band around the cursor.
`launcher_smoke` has no cursor and stays byte-stable; `calc_1_plus_2` /
`calc_fraction_sum` differ only inside `x[10..34] y[8..16]`. To compare them
anyway, ignore that region:

```bash
# Ad-hoc rectangle(s) (top-left origin, X,Y,W,H; repeatable):
python scripts/compare-ppm.py golden.ppm candidate.ppm --ignore-rect 8,6,30,13

# Or a committed mask file (one 'x,y,w,h' per line; '#' comments, blank lines ok):
python scripts/compare-ppm.py golden.ppm candidate.ppm \
  --mask-file tests/emulator/masks/calc_1_plus_2.mask --write-diff diff.ppm
```

Pixels inside an ignore-rect are excluded from the mismatch count, the bounding
box, and the equality decision (the diff image paints them **teal**). If the only
differences fall inside masks, the result is `IDENTICAL (after masking …)` → exit
0; differences *outside* a mask still exit 1. Rects with `x<0`, `y<0`, `w<=0` or
`h<=0` are an error (exit 2); rects past the frame edge are clipped.

**When is a mask acceptable?** Only for genuinely nondeterministic UI regions (the
blinking cursor). Keep it tight — never mask the expression or result pixels, and
never use a mask to hide a rendering bug (the renderer/cursor is frozen, so the
blink is masked in the comparator, not fixed in `src/`). A mask is **not** a
substitute for a semantic assertion that the result is correct — for *value*
correctness use the Phase 4B-C [`assert_result`](#semantic-assertions-phase-4b-c)
commands; render correctness remains a human judgement at promotion time. See
[`tests/emulator/masks/README.md`](../tests/emulator/masks/README.md).

**Promote a candidate to a golden (Phase 4B-D)** by visually confirming it is
correct, then using the human-run helper to do the byte-exact copy and commit it
(with its mask, if it needs one):

```bash
python scripts/promote-emulator-golden.py --list-candidates   # stem, sha256, golden/mask state
python scripts/promote-emulator-golden.py --dry-run calc_1_plus_2   # preview, writes nothing
python scripts/promote-emulator-golden.py calc_1_plus_2            # copy candidate -> golden
git add tests/emulator/golden/calc_1_plus_2.ppm tests/emulator/masks/calc_1_plus_2.mask
git commit -m "Accept golden+mask: calc_1_plus_2 renders 3 (cursor region masked)"
```

[`promote-emulator-golden.py`](../scripts/promote-emulator-golden.py) refuses a
missing or malformed candidate, refuses to clobber an existing golden without
`--force`, prints the promoted file's SHA-256, and reports whether CI will compare
it masked or byte-exact. It **never** generates images, **never** writes a mask,
and **never** commits — the commit is the human acceptance record. The equivalent
manual step is `cp out/emulator-candidates/<stem>.ppm
tests/emulator/golden/<stem>.ppm`. No *automated* step ever writes into
`tests/emulator/golden/` or `tests/emulator/masks/`; CI never invokes the promote
tool.

**Accepted goldens & the review gate.** Accepted, human-reviewed goldens live in
[`tests/emulator/golden/`](../tests/emulator/golden/) (committed; same stem as the
script). A candidate becomes a golden only by a human visually confirming it is
correct and then promoting + committing it:

```bash
python scripts/promote-emulator-golden.py calc_1_plus_2
git add tests/emulator/golden/calc_1_plus_2.ppm && git commit -m "Accept golden: 1+2 = 3"
```

No *automated* step writes into `tests/emulator/golden/`; the only writer is the
human-run promote helper, and it never commits.

**Updating a golden after an intentional UI/render change.** A deliberate render
change will (correctly) make the old golden mismatch. Regenerate candidates,
**re-confirm** the new one is correct, then re-bless with
`python scripts/promote-emulator-golden.py --force <stem>` (it refuses to clobber
a golden without `--force`) and commit the changed bytes in their own PR so the
old-vs-new diff is what reviewers approve. See
[`tests/emulator/golden/README.md`](../tests/emulator/golden/README.md) for the full
policy.

**CI behavior.** The emulator workflow
([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml))
generates candidates, uploads them under the **`numos-emulator-candidates`**
artifact, and then for each candidate:

- golden **and** mask present → **masked** compare (`--mask-file`), fails on
  mismatch outside the mask;
- golden present, no mask → **exact** byte compare, fails on mismatch;
- golden absent → prints a `::warning::` and continues.

So an unreviewed image can never silently become a passing gate, missing goldens
never fail CI (they just produce a downloadable candidate to review), and adding a
mask never weakens a screen that already compares exactly.

### Additional emulator apps: Settings & Math Showcase (Phase 5A)

Phase 5A widens emulator app coverage beyond CalculationApp with two **safe**
apps. Both are **emulator-only** wiring in
[`NativeHal.cpp`](../src/hal/NativeHal.cpp); the firmware, the renderer geometry,
the STIX assets, and the CAS/Giac engine are **untouched**. Open either with the
new [`open_app`](#scripted-input-replay-phase-4a) script command (or, for
Settings, by navigating the launcher to its card and pressing ENTER).

**Settings.** The real, unmodified [`SettingsApp`](../src/apps/SettingsApp.cpp) —
the same LVGL-native panel the firmware ships (complex-numbers toggle, decimal
precision, step-by-step mode). It needs **no HAL work**: it touches only LVGL +
`ui::StatusBar` and the three `setting_*` globals already defined for the native
build ([NativeHal.cpp:103-105](../src/hal/NativeHal.cpp#L103)). 

> **Settings persistence is in-memory only (both firmware and emulator).**
> `SettingsApp` reads/writes the `setting_*` globals directly; it performs **no**
> Preferences/NVS/flash persistence in either build, so there is nothing to stub.
> Changes last for the session. This is the app's existing design, not an
> emulator fallback.

**Math Showcase.** An **emulator-only**, screenshot-friendly display of a
*curated* subset of the **accepted** `MathRenderVisualCases`, rendered through the
same `MathCanvas` the firmware uses — with the **cursor OFF** (deterministic, no
blink), **no diagnostic colored overlays**, and **no per-case serial spam**. It is
deliberately **separate** from the diagnostic `MathRenderVisualTestApp` (which is
firmware-validation-only, behind `NUMOS_MATH_VISUAL_VERIFY`, and prints layout
dumps). The Showcase **builds nothing new**: it reuses the accepted renderer
geometry verbatim. Cycle the curated cases with LEFT/RIGHT (or UP/DOWN):

```text
1 + 2/3 + x^2      2 + 2/2      2^2      x^10      (2/3)^2      2^(1/2)      (1 + 1/2) / (x + 3)
```

> Math Showcase is **emulator-only** (not a firmware launcher card). The same
> expressions remain accessible to firmware validation through
> `MathRenderVisualTestApp` under `NUMOS_MATH_VISUAL_VERIFY`; the Showcase is the
> *clean, demo/screenshot* presentation of that shared, accepted case list.

**Smoke scripts.** `tests/emulator/scripts/settings_smoke.numos` and
`math_showcase_smoke.numos` each `open_app` the app, screenshot, and
`assert_app <Name>`. They are also wired into
[`generate-emulator-candidates.py`](../scripts/generate-emulator-candidates.py), so
CI generates and uploads their candidate PPMs and warns (does not fail) on a
missing golden — **no golden is blessed by this phase**.

```bash
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh \
  --headless --deterministic --script tests/emulator/scripts/settings_smoke.numos \
  --frames 800 --screenshot out/settings_smoke.ppm --quiet            # Linux
```

**Limitations.** Only `assert_app` (not `assert_result`) is supported for these
non-calculation apps in this phase. Settings persistence is in-memory only (see
above). Math Showcase is read-only display (it evaluates nothing). Both screens
are byte-deterministic within a run (verified by a back-to-back SHA-256 compare);
across runs the only volatile region is the StatusBar clock (`HH:MM`, ~`x[27..33]
y[8..16]`) — the same per-minute variation every status-bar screen has — so a
future golden would mask the clock, just as `calc_1_plus_2` masks the blinking
cursor.

### Additional emulator apps: Statistics & Probability (Phase 6A)

Phase 6A widens emulator app coverage with two more **safe**, LVGL-only educational
apps. As in Phase 5A, this is **emulator-only** wiring in
[`NativeHal.cpp`](../src/hal/NativeHal.cpp); the firmware, the renderer geometry,
the STIX assets, and the CAS/Giac engine are **untouched**. Open either with the
[`open_app`](#scripted-input-replay-phase-4a) script command (or by navigating the
launcher to its card — Statistics or Probability — and pressing ENTER).

**Statistics.** The real, unmodified [`StatisticsApp`](../src/apps/StatisticsApp.cpp)
— a 3-tab panel (Data table editor / computed Stats / histogram) backed by a pure
`<vector>`/`<cmath>` [`StatsEngine`](../src/apps/StatsEngine.cpp). It needs **no HAL
work**: it touches only LVGL + `ui::StatusBar` + `KeyboardManager`, with no Giac,
no filesystem, no `heap_caps`/PSRAM, and no sensors. It opens on the **Data** tab, so
no statistics are computed on open (no empty-dataset edge case).

**Probability.** The real, unmodified [`ProbabilityApp`](../src/apps/ProbabilityApp.cpp)
— a normal-distribution N(μ, σ) explorer with an `lv_chart` bell curve and PDF/CDF
readout, backed by a pure `<cmath>` [`ProbEngine`](../src/apps/ProbEngine.cpp). Same
dependency profile as Statistics. It opens with defaults μ=0, σ=1, x=0 (σ≠0, so the
curve computation has no division-by-zero).

> Both apps are the **same code the firmware ships** (already compiled there via
> `+<*>`); Phase 6A only adds their `.cpp` to the native
> [`build_src_filter`](../platformio.ini#L188) and routes `open_app`/`assert_app`/
> launcher id 4 (Statistics) and 5 (Probability) in `NativeHal.cpp`. No app logic is
> modified.

**Smoke scripts.** `tests/emulator/scripts/statistics_smoke.numos` and
`probability_smoke.numos` each `open_app` the app, screenshot, and
`assert_app <Name>`. They are wired into
[`generate-emulator-candidates.py`](../scripts/generate-emulator-candidates.py), so
CI generates and uploads their candidate PPMs and **warns (does not fail)** on a
missing golden — **no golden is blessed by this phase**.

```bash
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh \
  --headless --deterministic --script tests/emulator/scripts/statistics_smoke.numos \
  --frames 800 --screenshot out/statistics_smoke.ppm --quiet           # Linux
```

**Limitations.** These two apps are **smoke-only** (open + `assert_app` + screenshot);
they have **no `assert_result`-style semantic assertions** (that path is
CalculationApp-only). Both open in their default state and are not driven with data
entry in this phase. As with every emulator screen the only across-run volatile
region is the StatusBar clock (`HH:MM`); a future golden would mask it, exactly as
`calc_1_plus_2` masks the blinking cursor.

## Keyboard map (Phase 3A)

PC keys map directly to calculator `KeyCode`s in
[`NativeHal.cpp`](../src/hal/NativeHal.cpp) (`mapSdlToKeyCode`). SDL **KEYDOWN**
produces a press (or `REPEAT` on OS auto-repeat); SDL **KEYUP** now produces a
release. In the launcher (MENU) only navigation/confirm keys do anything; in
CalculationApp every mapped key is forwarded.

| PC key | Calculator | PC key | Calculator |
|:--|:--|:--|:--|
| `Enter` / KP Enter | ENTER | `0`–`9` (top row + keypad) | NUM_0..NUM_9 |
| Arrow keys | UP / DOWN / LEFT / RIGHT | `+ - * /` | ADD / SUB / MUL / DIV |
| `Esc` | AC | `( )` | LPAREN / RPAREN |
| `Backspace` / `Delete` | DEL | `^` | POW |
| `Home` / `m` | MODE (back to launcher) | `.` | DOT |
| `LShift` / `RShift` | SHIFT | `s` `c` `t` | SIN / COS / TAN |
| `Tab` | ALPHA | `l` `g` `r` | LN / LOG / SQRT |
| `Insert` | STO (Store) | `p` `e` | π / e |
| `f` / `F5` / `=` | FREE_EQ (S⇔D) | `x` `y` | VAR_X / VAR_Y |
| `n` | NEGATE | | |

Notes:
- **SHIFT / ALPHA / STO** are resolved by `KeyboardManager` inside
  CalculationApp (they do nothing in the launcher). `Insert`→STO is new in
  Phase 3A.
- **OS key-repeat** is forwarded as `KeyAction::REPEAT` (a held key repeats),
  matching the hardware keyboard driver; it is distinct from a fresh PRESS.
- Unmapped keys are logged as `[KEY] sin-mapear SDL=<name>` unless `--quiet`.
- This is **not** the full hardware 5×10 matrix; it is a direct desktop keymap.

---

## Build directory

This repo sets `build_dir = C:/.piobuild/numOS` in `platformio.ini` (top
`[platformio]` section) to keep Windows build paths short (MinGW + deep
PlatformIO paths can hit the Windows path-length limit). Consequences:

- On **Windows** the emulator builds to
  `C:/.piobuild/numOS/emulator_pc/program.exe`. The run script finds it
  automatically.
- On **Linux/CI**, that `C:/...` value is not a sensible path. Override it with
  `PLATFORMIO_BUILD_DIR=.pio/build` (recommended), which is what the commands and
  CI above do. The firmware CI also strips the line with `sed`.
- The run scripts resolve the build dir in this order:
  `PLATFORMIO_BUILD_DIR` → `build_dir` from `platformio.ini` (ignored on
  non-Windows if it is a `C:\` path) → `.pio/build`.

---

## Known limitations (Phase 3A)

- **Native Giac is unavailable.** All Giac entry points are `#ifdef ARDUINO`
  ([GiacBridge.h:19,29](../src/math/giac/GiacBridge.h#L19)); the emulator does not
  compile or call Giac. The Giac library is intentionally excluded from the native
  build (`lib_ignore = giac libtommath`, [platformio.ini:185](../platformio.ini#L185)).
- **BigInt parity is not guaranteed.** The in-tree `cas::` engine builds natively
  but with `CAS_HAS_BIGINT 0` ([CASInt.h:53-59](../src/math/cas/CASInt.h#L53)) —
  int64 only; on overflow it returns an error state rather than promoting to
  arbitrary precision like the device. Large factorials / huge exact fractions can
  differ from hardware. Small results (e.g. `1/2 + 1/3 = 5/6`) are fine.
- **Not a cycle-accurate ESP32 emulator.** No Xtensa CPU emulation, no ILI9341 /
  SPI / DMA timing, no real PSRAM/`heap_caps` behavior (PSRAM is plain `malloc`).
- **Hardware keyboard matrix is not emulated.** The physical 5×10 GPIO scan
  (`drivers/Keyboard`) is not compiled natively; PC keys are mapped directly to
  `KeyCode` in [NativeHal.cpp](../src/hal/NativeHal.cpp) (`mapSdlToKeyCode`).
  As of Phase 3A, `KEYUP` produces `RELEASE` and `Insert`→`STO` is mapped (see
  [Keyboard map](#keyboard-map-phase-3a)); full matrix / serial-text input is
  still future. SHIFT/ALPHA/STO are only meaningful inside CalculationApp, not
  in the launcher.
- **CalculationApp, Settings, Math Showcase, Statistics, and Probability are wired**
  in the emulator (Settings + Math Showcase in Phase 5A; Statistics + Probability in
  Phase 6A); the remaining apps still print "no implementada"
  ([NativeHal.cpp](../src/hal/NativeHal.cpp)). Settings, Statistics, and Probability
  are real firmware apps; Math Showcase is emulator-only. See
  [Additional emulator apps (Phase 5A)](#additional-emulator-apps-settings--math-showcase-phase-5a)
  and [Statistics & Probability (Phase 6A)](#additional-emulator-apps-statistics--probability-phase-6a).
- **Scripted input replay, golden tooling, AND semantic value assertions exist;
  rendered-pixel assertion does not.** Phase 3A adds `--frames` / `--run-for-ms` /
  `--headless` for unattended boot smokes, Phase 3B adds `--deterministic` +
  `--screenshot` for a reproducible 320×240 PPM artifact, Phase 4A adds `--script`
  for deterministic `.numos` input replay (see
  [Scripted input replay](#scripted-input-replay-phase-4a)), **Phase 4B-A**
  adds candidate-screenshot generation + a dependency-free PPM comparator + a
  human-gated golden directory, **Phase 4B-B** adds cursor-safe masking
  (`--ignore-rect` / `--mask-file`) so calc screenshots compare despite cursor
  blink (see
  [Candidate screenshots & golden comparison](#candidate-screenshots--golden-comparison-phase-4b-a)),
  and **Phase 4B-C** adds `assert_*` commands that check the value CalculationApp
  actually *computed* — no OCR, no pixels — via an emulator-only read-only probe
  (see [Semantic assertions](#semantic-assertions-phase-4b-c)). Byte-comparison
  against a golden (even masked) only proves a candidate equals a human-accepted
  image; a Phase 4B-C assertion proves the *math result* is correct but says
  nothing about the *rendered pixels*. Still future: asserting that the rendered
  output reads `3` *without* a hand-reviewed golden (e.g. OCR), and extending
  assertions beyond CalculationApp's `ExactVal` result. The interactive window
  (no `--script`/`--frames`) still runs until you close it.

---

## Troubleshooting

### "SDL2.dll could not be located" (Windows, at runtime)
The build succeeded but the DLL isn't reachable. Do one of:
- copy `SDL2.dll` next to `program.exe`, **or**
- set `NUMOS_SDL2_ROOT` to your SDL2 dev folder (the one with `bin\SDL2.dll`), **or**
- add the SDL2 `bin` folder to `PATH` for your shell.

The run script searches all three; run it after fixing one.

### "SDL2 development root not found" / "cannot find SDL headers/libs" (build)
- **Windows:** the extra_script couldn't find an SDL2 dev root. Set
  `NUMOS_SDL2_ROOT` (see [Windows](#windows)) and rebuild.
- **Linux:** install `libsdl2-dev` (or distro equivalent) so `pkg-config sdl2`
  succeeds. Verify with `pkg-config --modversion sdl2`.
- Run `python scripts/check-emulator-deps.py` for a full diagnosis.

### Emulator builds but the window does not open
- **Linux headless / SSH:** there is no display. Either run on a desktop session,
  or run with `--headless` (or `export SDL_VIDEODRIVER=dummy`). Pair it with
  `--run-for-ms N` so it auto-exits instead of running forever, e.g.
  `./scripts/run-emulator-linux.sh --headless --run-for-ms 5000 --quiet`.
- Check stdout: a healthy boot prints `[SDL2] Ventana ... creada OK`, a
  `[SCALE] ...` line, and reaches `[MENU] Launcher cargado`.
- If `SDL_CreateRenderer` fails (e.g. no GPU under the dummy driver), the code
  already falls back to the software renderer
  ([NativeHal.cpp](../src/hal/NativeHal.cpp), `SDL_RENDERER_SOFTWARE`); the
  `[SCALE]` log shows `backend=software(software)` in that case.

### The window opens but no keys work
- **Focus:** the SDL window must have OS keyboard focus — click it once.
- **Mode matters:** in the launcher (MENU) only arrows, `Enter`, `Esc`, and
  `Backspace` do anything (LVGL grid navigation); digits/operators are ignored
  there by design. Press `Enter` on the *Calculation* card first, then type.
- **Check the log (omit `--quiet`):** each accepted key prints
  `[KEY] SDL=<name> code=<n> action=PRESS|REPEAT|RELEASE mode=<MODE>`. If you
  see `[KEY] sin-mapear SDL=<name>` the key has no calculator mapping — consult
  the [Keyboard map](#keyboard-map-phase-3a) (note `(` / `)` / `*` need the same
  Shift combos as on a US keyboard).
- **STO:** press `Insert` (the only key bound to `KeyCode::STO`); it is
  meaningful only inside CalculationApp.

### The firmware build is separate
The emulator and firmware are independent PlatformIO environments. The emulator
work never changes firmware behavior:

```bash
pio run -e esp32s3_n16r8        # production firmware (unchanged by Phase 2)
```

If a firmware build misbehaves, it is not related to the emulator scripts/config.
