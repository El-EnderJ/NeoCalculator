# WASM-FEAS-01 — SDL2 browser feasibility

## Status and scope

`emulator_web` is an opt-in Emscripten build of the same NumOS emulator source
allowlist used by `emulator_pc`. It renders the real LVGL UI through SDL2 and
links the repository's Giac and libtommath source closure. There is no
JavaScript math implementation, RPC, remote CAS, or native custom-CAS fallback
in the browser path.

This target is independent of the ESP32-S3 N16R8 CAM and WROOM-1U hardware
contracts. It does not select or compile ESP32 HAL, pin, USB, display, or board
sources.

## Baseline and portability audit

The pre-edit baseline was branch `main`, commit
`cbd29c2fb824f60c7b73c9c93ae6f91f29a9475a`, with a clean working tree.
The repository contained 1,039 tracked files, 147 emulator scripts, 18 accepted
PPM goldens, and 18 masks. The accepted goldens totalled 4,147,470 bytes.

`emulator_pc` resolves to:

- 68 NumOS C/C++ translation units selected by its `build_src_filter`;
- 40 Giac translation units after the existing GUI/platform exclusions;
- 161 libtommath C translation units;
- LVGL 9.5.0, SDL2, libintl on macOS, and the host C/C++ runtime.

`scripts/emulator_sources.py` derives the web NumOS allowlist directly from
`[env:emulator_pc]`, preventing a second hand-maintained application closure.
The web CMake target applies the same Giac filename exclusions as the native
Giac library builder.

The pre-edit desktop entry point in `src/hal/NativeHal.cpp` owned SDL video and
timer initialization, the 320×240 RGB565 LVGL display, renderer/texture
presentation, SDL event polling, the blocking desktop loop, application
transitions, and shutdown. Presentation was already deferred until after
`lv_timer_handler()`. The desktop renderer requested accelerated rendering and
vsync with a software fallback.

Portability findings:

| Area | Finding and disposition |
|---|---|
| Blocking loop | `NativeHal.cpp` could not run unchanged in a browser. It is now split into initialize, one-frame, request-shutdown, and shutdown operations. |
| SDL | Emscripten's supported SDL2 port builds the existing SDL path. The browser uses its software canvas renderer because creating the EGL renderer before installing the browser main loop emits an invalid timing request. Desktop accelerated/vsync behavior is unchanged. |
| Giac C++ | All 40 translation units compile without source removal. Target flags provide removed C++17 binders, `unistd.h` for the generated lexer, `NO_BSD`, native Wasm exceptions, and existing embedded definitions. |
| Emscripten `EMCC` macro | Giac's historical `EMCC` branches use removed Emscripten/GMP APIs. The web target undefines only `EMCC`; `__EMSCRIPTEN__` remains defined and the normal embedded Giac/libtommath code compiles. |
| Filesystem | The host shim uses stdio and `std::filesystem`. In the browser its root is fixed to MEMFS `/numos`; no host path is visible. |
| Threads | No NumOS source in the selected closure starts a thread. Giac thread code is disabled by the embedded configuration. Pthreads are not enabled. |
| Locale/time | `StatusBar.cpp` uses the C time/localtime API, supplied by Emscripten. Giac's embedded configuration supplies its normal locale settings. |
| Process/signal/environment | Desktop-only filesystem sandbox PID/temp-path and headless `SDL_VIDEODRIVER` handling remain behind the non-Emscripten path. No selected web code installs signals or launches processes. |
| Dynamic libraries | The browser target is statically linked into one Wasm module. It performs no `dlopen` and assumes no browser-side shared library. |
| Absolute paths | Optional Giac share-directory constants are not startup dependencies. NumOS storage is fixed to virtual `/numos`; no host absolute path is packaged. |

The only selected source file that fundamentally could not compile and execute
unchanged as a browser program was `src/hal/NativeHal.cpp`, due to its native
entry-point loop, process/filesystem setup, and lack of browser exports.
`GiacEngine.cpp` and `MainMenu.cpp` compiled as-is but received emulator-only
diagnostics and pointer-hit behavior needed to prove acceptance. Giac library
sources remain unmodified.

On the audit machine the original macOS `emulator_pc` build also exposed
pre-existing Apple libc++/linker portability issues: removed C++17 binders,
Giac's incorrect 64-bit tagged-pointer selection, GNU-only
`--gc-sections`, a missing libintl link, and libtommath random-provider dead
branches at `-O0`. The native build scripts now select equivalent
platform-specific flags. Three Apple-only, compile-time-unreachable provider
stubs let libtommath retain its original `-O0` semantics; they compile empty
for firmware and are excluded from the web closure.

## Toolchain and build

The pinned SDK is Emscripten **6.0.3**, recorded in
`wasm/emscripten.version`. The build fails early if `emcc` reports a different
version. C++ sources use C++17.

Prerequisites are PlatformIO's installed `emulator_pc` LVGL dependency,
Emscripten 6.0.3, Node/npm, and a Playwright-supported Chromium:

```sh
pio pkg install -e emulator_pc
npm ci --prefix tests/wasm
npx --prefix tests/wasm playwright install chromium
wasm/build.sh Release
wasm/build.sh Debug
```

Outputs are written below ignored `out/wasm/release/` and
`out/wasm/debug/`. Each contains `numos-emulator.wasm`, an ES-module
`numos-emulator.js` factory, and the small HTML/CSS/module shell. Serve the
directory over HTTP; browsers do not reliably instantiate Wasm ES modules from
`file:` URLs:

```sh
python3 -m http.server 8000 --directory out/wasm/release
```

Then open `http://127.0.0.1:8000/`.

The release build uses `-O2`; debug uses `-O0`, Emscripten assertions, Safe
Heap, and `-g2`. Full `-g3` is intentionally not used because the pinned 6.0.3
`wasm-emscripten-finalize` asserts while processing this large Giac DWARF map.
Neither build enables Asyncify or pthreads.

## Runtime and canvas

Desktop continues to call `emulatorRunFrame()` from its ordinary loop and delay
between frames. Web calls it from `emscripten_set_main_loop`, returning to the
browser after every frame. Shutdown cancels that loop before releasing
applications, LVGL, SDL texture/renderer/window, and SDL.

SDL owns one canvas and retains NumOS's logical 320×240 RGB565 framebuffer.
The shell preserves 4:3 aspect ratio, chooses whole-number CSS scaling where
the viewport permits, and requests pixelated/nearest-neighbour presentation.
SDL accounts for the CSS/backing-store scale before delivering browser mouse
coordinates. Browser pointer presses and releases enter a web-only LVGL pointer
indev, while keyboard input continues through the existing SDL text/key mapping
and `dispatchKey` path. Desktop pointer/input behavior is unchanged. Clicking
focuses the canvas. Resize recalculates only the canvas CSS size and does not
alter NumOS geometry.

Touch overlays and mobile virtual-keyboard UX are not included.

## Storage

The browser creates `/numos` in Emscripten MEMFS and hands only that root to
the existing LittleFS-compatible shim. No preload/data package is required at
startup; fonts and UI resources are compiled into the module. Missing settings
and variable files retain their existing non-fatal first-run behavior.

MEMFS contents disappear on reload. The single `/numos` mount boundary is the
intended insertion point for a future, separately reviewed IDBFS settings and
NeoLanguage persistence task. This milestone neither syncs nor persists broad
state. Browser code cannot access the host filesystem.

## JavaScript boundary

`numos-shell.js` exposes `window.numos` after module creation:

- `isReady()`;
- `diagnosticState()`;
- `requestShutdown()`;
- `sendLogicalKey(keyCode, actionCode)` and `pressLogicalKey(keyCode)`.

`window.numosReady` resolves only after the real launcher transition and splash
teardown complete. Diagnostics are concise copied JSON; they do not expose
`giac::gen`, a Giac context pointer, memory views, or other raw CAS internals.

## Giac proof and smoke test

Native-simulator-only counters in `GiacEngine` record context creation and
destruction, structured evaluations/solves, retained expression compilation,
numeric sampling, live retained handles, and reset generation. They compile
out of firmware.

The Playwright smoke:

1. instantiates the module and waits for the real launcher;
2. clicks the Calculation card through browser pointer → SDL → LVGL;
3. types `2+2` through physical-keyboard events and requires structured exact
   result `4` plus an increased Giac structured-evaluation counter;
4. enters `y=x^2` in Grapher and requires one successful retained Giac compile;
5. solves `2*x+4=0` in Equations and requires structured exact solution `-2`
   plus an increased structured-solve counter;
6. completes 18 additional settled app/menu transitions;
7. requires exactly one active Giac context, unchanged reset generation, and
   one stable retained Grapher handle, with no fatal console/page errors,
   non-monotonic allocator samples, and no more than 1 MiB bounded drift.

Run release or debug smoke with:

```sh
wasm/test.sh Release
wasm/test.sh Debug
```

The test intentionally allows the production 200 ms LVGL screen transition to
settle before requesting the reverse transition. Teardown additionally checks
LVGL's loading/previous-screen state before deleting an app screen. It ends by
requesting shutdown and checking that the browser main loop returns and tears
down without a fatal console or page error.

## Verification on the audit machine

The following passed:

- Giac host suite: 177/177;
- Calculus host suite: 26/26;
- Neo host suite: 44/44;
- cross-app host suite: 14/14;
- CAS/tutor host suites: 8/8 suite groups;
- `pio run -e emulator_pc`;
- all 147 emulator scripts with their expected polarity (146 normal scripts
  plus the separate Neo Giac smoke; the deliberate Grapher negative control
  produced its expected assertion failure);
- candidate generation: 42/42;
- keycode guard and its self-test;
- Release and Debug `emulator_web` builds and browser smokes;
- `esp32s3_n16r8`, `esp32s3_n16r8_validate`,
  `esp32s3_n16r8_giacdiag`, and `esp32s3_n16r8_mathdiag`.

On this macOS host, 16/18 masked raster comparisons pass byte-for-byte outside
their masks. `grapher_graph_smoke` and `grapher_trace_smoke` each differ by the
same seven pixels in a 4×4 area. Rebuilding unmodified baseline commit
`cbd29c2f` on the same host reproduces those exact seven outside-mask pixels.
The current and baseline captures otherwise differ only in the masked status
bar clock. This is therefore a pre-existing cross-platform golden discrepancy,
not a WebAssembly change. No golden or mask was edited or re-blessed.

No physical validation is claimed for the WROOM-1U PCBAs in transit.

## Measurements

Measurements were taken on the audit Mac with Emscripten 6.0.3 and Playwright
Chromium. They are feasibility observations, not performance budgets.

| Metric | Release (`-O2`) | Debug (`-O0 -g2`, assertions, Safe Heap) |
|---|---:|---:|
| Wasm | 6,298,251 B | 11,342,394 B |
| Wasm gzip -9 | 2,027,025 B | 2,798,421 B |
| generated ES loader | 213,258 B | 364,246 B |
| loader gzip -9 | 53,500 B | 87,506 B |
| HTML/CSS/shell assets | 3,717 B | 3,717 B |
| module factory ready | 25.2 ms | 52.8 ms |
| launcher, browser clock | 1,706.4 ms | 1,759.9 ms |
| launcher, test wall clock | 1,743.3 ms | 1,806.8 ms |
| first Giac `2+2` | 20.0 ms | 56.5 ms |
| Wasm heap capacity | 64 MiB | 64 MiB |
| steady used heap after bounded switching | 1,578,384 B | 1,578,728 B |
| six heap samples, min–max spread | 48 B | 32 B |
| frame time p50 / p95 / max (512 samples) | 0 / 8 / 9 ms | 0 / 21 / 28 ms |

The module-factory measurement includes browser Wasm compilation,
instantiation, and generated-loader initialization; compilation was not
separately instrumented. There is no `.data` preload, so packaged startup asset
payload is zero; the 3,717-byte host shell is reported separately above.

The final desktop executable is 8,403,944 bytes, versus 8,402,424 bytes for the
same-host baseline rebuild: +1,520 bytes. The production firmware build uses
5,346,433 bytes flash and 117,040 bytes static RAM. A clean baseline-source
firmware rebuild reports the same values, for measured deltas of **0 bytes
flash** and **0 bytes static RAM**.

Diagnostic firmware sizes are:

| Environment | Flash | Static RAM |
|---|---:|---:|
| `esp32s3_n16r8_validate` | 5,380,205 B | 117,040 B |
| `esp32s3_n16r8_giacdiag` | 5,350,649 B | 117,040 B |
| `esp32s3_n16r8_mathdiag` | 5,385,313 B | 117,040 B |

## Current limitations

- Temporary MEMFS only; reload discards variables/settings.
- Desktop-class keyboard and pointer UX only; no mobile overlays.
- The shell is a feasibility host, not the future NumOS website integration.
- Browser shutdown is explicit and terminal for that module instance; create a
  new factory instance to restart.
- WebGL acceleration is not used in this milestone. At 320×240 the SDL
  software canvas path is adequate and avoids configuring EGL before the
  asynchronous browser loop exists.
- Browser measurements depend on machine, browser cache, and debug/release
  configuration and should be re-recorded in release CI before setting budgets.
