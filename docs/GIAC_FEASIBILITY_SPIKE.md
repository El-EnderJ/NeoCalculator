# GIAC-FEAS-01 — Giac as Primary Engine: Feasibility Spike

> **Status (2026-07-13, GIAC-A01):** promoted to production. The spike
> adapter (`GiacFeasibility.*`), its harness and
> `scripts/build-giac-feasibility-host.sh` are gone; the production seam is
> `src/math/giac/GiacEngine.h/.cpp` (single `giac::context` owner, legacy
> GiacBridge delegates), tested by `tests/host/giac_engine_suite_main.cpp`
> via `scripts/build-giac-host-harness.sh`, and `emulator_pc` now links the
> same in-process Giac. The measurements below remain the original
> feasibility evidence.

Date: 2026-07-12 · Branch: `main` · HEAD: `80aeead` · Author: spike run (no app code modified)

Working tree at spike time additionally carried in-progress NB-work edits to
`src/math/MathEvaluator.cpp`, `src/math/cas/ASTFlattener.cpp`,
`src/math/cas/SymPoly.cpp`, `tests/CASTest.cpp` (unrelated to this spike; all
measurements were taken with those edits present).

## 1. Question

Can Giac become the in-process primary math engine under Calculation and the
semantic frontend for Grapher, on both the native emulator and the ESP32-S3
N16R8 — without SerialBridge, subprocesses, or an external service?

## 2. Current integration truth (before this spike)

- Giac source: vendored KhiCAS-lineage snapshot at `lib/giac` (package version
  string "1.9.0", internal `config.h` says giac 1.4.9-57 KhiCAS profile),
  ~6.7 MB of sources, 40 `.cc` TUs + `platform_stubs.cpp`. Bignum backend is
  vendored `lib/libtommath` (1.2.0) via `USE_GMP_REPLACEMENTS` — no GMP.
- **Giac is already compiled into the shipping firmware** (`esp32s3_n16r8`
  env: `-DNUMOS_USE_GIAC=1 -DGIAC_KHICAS -DNO_GUI -DGIAC_GENERIC -DEMBEDDED
  -UCASIO -DUMAP -fexceptions`, RTTI/exceptions un-disabled via
  `build_unflags`). Calls are **in-process**: `src/math/giac/GiacBridge.cpp`
  (`initGiac()` + `solveWithGiac(String)` over a `static giac::context`).
  SerialBridge is only the *keystroke transport* into that path.
- `src/math/giac/GiacAlloc.cpp` globally overrides `operator new/delete` to
  prefer PSRAM (`MALLOC_CAP_SPIRAM`) for the whole firmware when
  `NUMOS_USE_GIAC && ARDUINO && BOARD_HAS_PSRAM`.
- Consumers: `MathEvaluator.cpp::evaluateWithGiac()` (`#ifdef ARDUINO` only)
  and the UART command path. CalculationApp's primary path and Grapher do NOT
  use Giac.
- The native emulator (`emulator_pc`) hard-excludes Giac (`lib_ignore = giac,
  libtommath`) citing spec risk R3: "native Giac fails: `SIZEOF_INT` not
  declared". **This spike found that verdict obsolete** — see §4.
- `docs/CAS_UPGRADE_ROADMAP.md` already declares Giac/KhiCAS the intended
  canonical engine.

## 3. Portability audit of the vendored profile (`lib/giac/src/config.h`)

Already stripped for embedded: `HAVE_NO_SIGNAL_H`, `HAVE_NO_HOME_DIRECTORY`,
`HAVE_NO_PWD_H`, `HAVE_NO_CWD`, `HAVE_NO_SYS_RESOURCE_WAIT_H`, no
`locale.h`/`unistd.h`, `NO_STDEXCEPT`, `NO_RTTI` (internal avoidance; builds
fine with RTTI on), `NO_PHYSICAL_CONSTANTS`, `NOTURTLE`,
`STATIC_BUILTIN_LEXER_FUNCTIONS` (lexer tables are compile-time — no runtime
init cost), `USE_GMP_REPLACEMENTS` + `HAVE_LIBTOMMATH`.

- Exceptions: **required** (`-fexceptions`; error paths throw internally and
  the bridge catches). Already enabled on firmware.
- Threads: pthread paths compiled out; single-threaded operation confirmed.
- Filesystem: no home/cwd/pwd; `access()` calls are guarded (see §4 mingw
  note). No file I/O on the eval path.
- fork/dynamic loading/signals: absent in this profile.
- Writable global state: `giac::context` carries state; KhiCAS globals exist
  but the eval path is context-scoped. One `static giac::context` is the
  existing production pattern.
- Large static tables: static lexer/help tables live in flash (`.rodata`) —
  part of the 2.66 MiB flash cost, not RAM.
- Stack: recursion-heavy; firmware already sets
  `-DARDUINO_LOOP_STACK_SIZE=65536`.

## 4. What the spike built (files changed)

| File | Purpose |
|:-----|:--------|
| `src/math/giac/GiacFeasibility.h/.cpp` | Provisional adapter: `GiacEvalResult evaluate(const char*)` + `CompiledExpr` (parse-once, numeric `evalAt(double)` per sample). Compiled only under `-DNUMOS_GIAC_FEASIBILITY`. Context config mirrors production `initGiac()`. |
| `tests/host/giac_feasibility_main.cpp` | Harness: representative suite, failure probes, warm/cold timings, RSS stability, 1000/200k-sample sweeps. |
| `tests/host/giac_feasibility_host_stubs.cpp` | Two host-only link stubs (`giac::lang`, `::paste_clipboard`) that kdisplay.cc provides on real KhiCAS devices; ELF gc-sections strips their referencer, PE cannot. |
| `scripts/build-giac-feasibility-host.sh` | Native build of vendored Giac + libtommath + adapter + harness. |
| `platformio.ini` | Added `esp32s3_n16r8_giacfeas_baseline` (firmware WITHOUT Giac, link-stubbed) purely to measure Giac's flash/RAM delta. |
| `src/math/giac/GiacFeasibilityNoGiacStub.cpp` | The link stub for that measurement env. |

Key discovery: the "native Giac is broken (SIZEOF_INT)" blocker was only a
missing `-DHAVE_CONFIG_H` — on firmware the Arduino-ESP32 platform injects it
globally; no repo file defines it. With the firmware's macro set (+
`-D__MINGW_H -fpermissive` on Windows and gc-sections + the two stubs), the
full vendored Giac compiles and links natively (mingw g++ 15.2, x86_64,
10.3 MB static binary).

## 5. Host results (Windows x86_64, mingw, `-O1`, `-DDOUBLEVAL`)

Suite: **24/24 pass** (`/tmp/giac-feas-host/giac_feasibility.exe`).

| Expression | Result | Exact? | Warm time |
|:-----------|:-------|:------:|----------:|
| `2+2` | `4` | ✓ | 3 µs |
| `1/2+1/3` | `5/6` | ✓ | 43 µs |
| `2^100` | `1267650600228229401496703205376` | ✓ | 47 µs |
| `x-2*x` | `x-2*x` (unchanged; `regroup(...)` → `-x`) | ✓ | 60 µs |
| `simplify((x^2-1)/(x-1))` | `x+1` | ✓ | 373 µs |
| `factor(x^3-6*x^2+11*x-6)` | `(x-1)*(x-2)*(x-3)` | ✓ | 122 µs |
| `solve(x^2-2=0,x)` | `list[-sqrt(2), sqrt(2)]` | ✓ | 234 µs |
| `solve(ln(x)=1,x)` | `list[exp(1)]` | ✓ | 91 µs |
| `diff(sin(x)^2,x)` | `2*cos(x)*sin(x)` | ✓ | 26 µs (6.9 µs warm avg ×100) |
| `integrate(x^2,x)` | `x^3/3` | ✓ | 59 µs |
| `pi` / `sqrt(2)` / `sin(pi/6)` | `pi` / `sqrt(2)` / `1/2` | ✓ | 3–12 µs |
| `simplify(1/x)` / `diff(sin(x),x)` | `1/x` / `cos(x)` | ✓ | 8–25 µs |

Timings/behavior:
- init: 65 µs; first eval: 109 µs (static lexer tables → no runtime build).
- Failure probes: syntax error → `undef` + parser diagnostic; `1/0` → `oo`
  (Giac semantics, not an error); unknown function echoes symbolically;
  `idivis(x)` → `undef` + "Error: Bad Argument Type". No crashes; all caught.
- Context reset (delete/new `giac::context`): assigned variable forgotten,
  engine keeps working.
- Repeated-eval stability: 180 mixed evals → RSS +8 KB (flat).
- Process RSS: 4.9 MB start → 7.7 MB peak (64-bit `gen` = 16 B; expect
  roughly half the working-set growth on 32-bit ESP32).
- Allocation-failure injection: not realistically testable on host (static
  64-bit binary); on firmware `GiacAlloc.cpp` already falls back
  PSRAM→internal and throws `bad_alloc`, which the bridge catches.

## 6. Grapher repeated-evaluation viability

`CompiledExpr` retains the parsed/eval'd `gen` and per sample does
`subst(expr, x, gen(double)) → evalf_double` — **no string parsing per
sample**, context reuse safe (single-threaded).

| Function | 1000 samples | 200,000 samples | per-sample |
|:---------|-------------:|----------------:|-----------:|
| `sin(x)` | ~0.1 ms | 8.8 ms | ~50 ns |
| `1/x` | ~0.1 ms | 10.0 ms | ~55 ns |
| `x^2-2` | ~0.1 ms | 12.9 ms | ~65 ns |

Checksums match analytic expectations (values verified, linear scaling).
Re-parsing per sample costs ~196 µs/sample — ≈3000× slower; the retained-
expression path is mandatory and works. Even at a pessimistic 100–200×
CPU derating (240 MHz in-order Xtensa + PSRAM heap), 1000 samples ≈
5–13 ms — inside an interactive replot budget. Lowering a Giac-normalized
expression into the existing NumOS Graph IR (RPN) remains available as a
belt-and-braces option: `gen.print()` → Tokenizer/Parser is exactly the
pipeline GraphModel uses today.

## 7. ESP32-S3 results

Both builds succeeded on this tree (toolchain xtensa-esp32s3, Arduino core):

| Build | Flash | Static RAM |
|:------|------:|-----------:|
| `esp32s3_n16r8` (with Giac, = shipping config) | 5,232,925 B (79.8% of 6,553,600 B app partition) | 115,928 B (35.4%) |
| `esp32s3_n16r8_giacfeas_baseline` (Giac+libtommath stripped) | 2,448,341 B (37.4%) | 105,756 B (32.3%) |
| **Giac + libtommath delta** | **+2,784,584 B (2.66 MiB)** | **+10,172 B** |

- Linker-map cross-check (`firmware.map`, non-debug sections): Giac
  `.text` ≈ 2,429,540 B, libtommath `.text` ≈ 28,505 B — consistent with the
  environment-delta measurement above (remainder is rodata/static tables).
- Free app-partition headroom with Giac: 1.32 MB (partition table
  `default_16MB.csv`; the 16 MB part leaves room to grow the app partition
  if ever needed).
- Runtime on hardware: **no ESP32-S3 was attached during this spike**, so
  no fresh on-device timings/heap high-water were captured. Compile
  viability is fresh evidence; runtime viability rests on the existing
  shipping UART path (`solveWithGiac`) which runs this exact engine
  in-process on the device today. Heap: all firmware `operator new` already
  routes to the 8 MB PSRAM; host working-set growth (~2.7 MB on 64-bit,
  ~half on 32-bit) fits with wide margin. Stack high-water: unmeasured;
  64 KB loop stack exists specifically for Giac.

## 8. Licensing / provenance (evidence only)

- NumOS-authored software carrying the project notice: GPL-3.0-or-later
  (`LICENSE-SOFTWARE`; see `LICENSE.md` for the licence-history qualification).
- Vendored Giac sources: GPL-3.0-or-later headers (B. Parisse, Institut
  Fourier).
- Vendored libtommath: `SPDX-License-Identifier: Unlicense`.
- No license conclusions drawn here beyond noting all three are present in
  the repo and NumOS already ships Giac in firmware.

## 9. Decision

| Target | Verdict | Basis |
|:-------|:--------|:------|
| 1. Calculation on ESP32-S3 | **GO** | Engine already links & runs in-process on device; +2.66 MiB flash fits (79.8%, 1.32 MB free); static RAM cost negligible; heap in PSRAM; full suite exact & fast on identical sources. |
| 2. Grapher semantics on ESP32-S3 | **GO** | Parse-once/eval-many proven ~3000× faster than reparse; extrapolated 1000-sample sweep well inside budget; optional lowering to existing Graph IR available. |
| 3. Native emulator | **CONDITIONAL GO** | Full native compile+run proven by this spike on the same toolchain the emulator uses. Condition: wire `emulator_pc` (drop `lib_ignore`, add the §4 flag set, keep the two PE link stubs; expect LDF/build-time cost ~7 min cold). |
| 4. WASM/browser | **CONDITIONAL GO (unproven here)** | Upstream Giac has first-class EMCC support (giacjs; `first.h` EMCC paths) and this snapshot compiles on a generic 64-bit target with GMP replacements. Condition: an actual emcc build + memory-ceiling check; not attempted in this spike. |

## 10. Recommended architecture & phases (post-spike)

Final shape: a single `MathEngine` seam with Giac as canonical backend;
NumOS CAS retained for tutor step generation, offline fallback, and the
numeric RPN fast path for graph sampling.

- **Phase A — engine abstraction**: promote the spike API into
  `EngineResult evaluate(...)` / `CompiledFn compile(...)` behind an
  interface both Giac and the legacy path implement; single owner of the
  Giac context + PSRAM policy; result → MathAST via existing print/parse or
  a `gen`→AST bridge.
- **Phase B — Calculation migration**: route CalculationApp evaluate through
  the engine seam (Giac primary, NumOS CAS fallback on error/timeout);
  display via existing MathRenderer; keep tutor steps on NumOS CAS.
- **Phase C — Grapher semantic frontend**: definition-time Giac pass
  (normalize, domain hints, derivative for POI) via `CompiledExpr`-style
  retained `gen`.
- **Phase D — graph numerical IR**: lower Giac-normalized expressions to the
  existing RPN Graph IR for per-frame sampling; Giac `evalAt` as
  cross-check/fallback.
- **Phase E — demotion**: remove NumOS-CAS-primary paths from Calculation;
  keep the tutor stepper and RPN sampler; delete dead solver entry points.

## 11. Reproduce

```bash
bash scripts/build-giac-feasibility-host.sh            # build + run host suite
pio run -e esp32s3_n16r8                               # firmware with Giac
pio run -e esp32s3_n16r8_giacfeas_baseline             # measurement build without Giac
```
