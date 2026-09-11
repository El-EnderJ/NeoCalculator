# TUTOR-TEACHING-UX-01 resource review

Independent embedded/lifecycle review. No production source, hardware configuration, Git index or hardware was changed by this reviewer. Evidence is under `out/tutor-teaching-ux-01/review-resource`.

## Baseline and build identity

The previous build directories were absent. Baseline binaries were therefore **reconstructed**, not recovered: all 1,188 source files were checked against fingerprint `8b4220ade0d7b30dc9fadbe7f71b1eaba945ade476e96c583c236bc2c6be3c5e`, using the preserved uncommitted overlay. `snapshot-copy.json` records the explicit copy; no older Git HEAD was substituted. The original temporary baseline build/source directories were later removed from the shared PlatformIO build directory; preserved executables/evidence survived. The matching immutable source at `C:/.codex-cache/numos-teaching-ux-baseline` was used to rebuild profiling objects. `final-source-verification.json` checks its relevant source hashes against the original baseline measurement.

Baseline binaries are preserved in `out/tutor-teaching-ux-01/baseline`; final production ELF/bin and fixed-pool native executable are in `review-resource/final-binaries`. The final isolated builds are outside the shared build directory, at `C:/.codex-cache/numos-teaching-resource`. Separate configuration files change only the build output path, and the native test configuration switches CLIB LVGL allocation to its BUILTIN 65,536-byte pool. The production target remains `numos-esp32-s3-wroom-1u-n16r8`; nothing was flashed.

## Production resources

| Measurement | Immediate baseline | Final candidate | Delta |
|---|---:|---:|---:|
| Linked flash | 5,458,549 B | 5,482,961 B | +24,412 B |
| Static DRAM | 118,984 B | 118,984 B | 0 B |
| `.iram0.text` | 60,407 B | 60,407 B | 0 B |
| Firmware binary | 5,458,912 B | 5,483,328 B | +24,416 B |
| Xtensa `drawStep` own frame | 528 B | 752 B | +224 B |
| Xtensa `showSteps` own frame | 80 B | 112 B | +32 B |
| Xtensa `solveEquations` own frame | 480 B | 480 B | 0 B |
| `sizeof(FontMetrics)`, Xtensa | 28 B | 32 B | +4 B |
| `sizeof(MathCanvas)`, Xtensa | 132 B | 144 B | +12 B |
| `sizeof(EquationsApp)`, Xtensa | 972 B | 1,068 B | +96 B |

The instance-size increase is real despite unchanged static RAM: inline font profiles and new view members grow the app object. It is not zero heap cost. `candidate-production-resources.json`, its disassemblies and `xtensa-object-sizes.json` contain executable evidence. The largest separately listed `drawStep` helper frame is 144 B; these are own frames, not total nested stack high-water. Giac and recursive AST callees add stack.

Final ELF SHA-256 is `b0e0bae7ddc2b3d735eeb1cb95d11ce690d5396c66e742586da92bd208b74796`; firmware binary SHA-256 is `4e867d499a26cc81de80bcccc9755d6614c13f57da379e6470f56aecb3348f86`. Production full build passed in 325.24 s and final dependent-header refresh in 184.66 s. Fixed-pool native full build passed in 60.21 s and final refresh in 15.63 s. Timings describe this host, not firmware latency.

## Page preparation and allocation

Test-only derived copies of `EquationsApp.cpp`/`TutorStepsView.inc` wrap `drawStep`; a derived `MathAST.cpp` redirects its existing custom `malloc` allocation to a counting hook. Production and frozen source remain unchanged. Global C++ `new`/`delete` and **MathNode payload** are counted separately, with a simultaneous combined peak. This corrects the initial profiler's omission of node payload: MathNode does not use global C++ `new`.

The C++ runtime is linked statically so allocations and deallocations share the probe implementation; SDL remains its usual DLL. Initial cross-DLL allocator mismatch and a fully static SDL link failure were isolated harness setup failures, corrected before measurement. Counting headers are excluded from reported requested bytes. Baseline's 34 screenshots and final candidate's 140 screenshots match their uninstrumented counterparts below the clock header.

| Measured page preparation | Baseline, 305 samples | Final, 387 samples |
|---|---:|---:|
| Maximum additional C++ payload above entry | 1,817 B | 2,316 B |
| Maximum additional MathNode payload above entry | 2,008 B | 2,816 B |
| Maximum simultaneous additional combined payload | 3,241 B | 3,931 B |
| Maximum retained view AST node count | 35 | 54 |
| Maximum C++ allocations per page | 331 | 409 |
| Median instrumented host duration | 255 us | 523 us |
| Maximum sampled host duration | 818 us | 1,802 us |

These maxima include temporary old/new tree overlap during page preparation. The separate maxima cannot be added to obtain the simultaneous peak. Node live-before/after counters also include authored/editor nodes still alive; they are not exclusively the displayed page. The candidate sample includes 70 guided/summary pages for factor branches, rational rejection, pure squares, general/complex quadratics, 3x3 systems, physical negative constants and pseudolocalization. Its maximum observed boundary conversions is four and maximum prepared-node count before wrapper rows is 51. The measured page sets differ because the guided view now exposes additional teaching pages; timing ratios are not a same-operation benchmark.

Explicit production bounds remain four reused formula slots, sixteen boundary conversions and 640 prepared nodes per page. Up to four row wrappers are additional; the previous page remains alive while the next page is prepared. The 640 count is **not** a combined old/new limit. The zero-allocation TeachingPlan scan operates over the already bounded trace. Construction happens on page changes, outside core layout/render traversal. The plus-minus fix initializes one font metric per profile and uses existing glyph geometry; it introduces no layout-loop allocation.

Direct Giac C allocations, other direct C allocations, allocator overhead and physical PSRAM/internal placement are outside the C++/node counters. On firmware, MathNode requests PSRAM first and falls back to ordinary 8-bit-capable heap; strings/general C++ temporaries follow the existing allocator. Trace-vector PSRAM accounting remains separate. These host measurements do not establish hardware fragmentation, peak total heap, physical latency, cache behavior or total task-stack high-water.

## Fixed 64 KB pool and lifecycle

Baseline normal and fixed-pool builds each passed fifty solve/Steps/detail/close/reopen/edit-cancel/delete/HOME/re-entry cycles with 250 probes. The final instrumented candidate and fixed-pool candidate repeat fifty cycles successfully. Reopening checks the same cached derivation and build count; edits invalidate it. Final fixed-pool evidence also includes 140 page/scroll probes, for 390 probes total.

| Phase | Baseline objects / free pool | Final objects / free pool range |
|---|---:|---:|
| Results | 20 / 11,504 B | 20 / 11,440 to 11,504 B |
| Steps | 25 / 9,376 B | 29 / 7,640 to 7,680 B |
| Reopened Steps | 25 / 9,392 B | 29 / 7,720 to 7,768 B |
| Post-edit/delete | 21 / 11,192 B | 21 / 11,160 to 11,200 B |
| HOME/menu | 67 / 19,160 B | 67 / 19,160 B |

The candidate's Equations pool bookkeeping settles into an exact four-cycle pattern after warm-up. The initial helper's stronger constant-equality check reported false; the evidence retains that result. Final analysis records the exact periodic values and constant object/timer/handle counts. There is no downward trend, and HOME restores exactly 19,160 B every cycle. This is bounded periodic restoration, not constant in-app free space.

Minimum free space across **all** sampled final pages is 7,528 B on a pseudolocalized summary. Three timers and zero retained Grapher handles persist. The pool slab remains 65,536 B; the varying probe `pool_total` includes allocator bookkeeping effects. CLIB-native zeros mean unavailable telemetry, not zero allocation. Native pointer/object sizes differ from ESP32.

Final 320x240 evidence: `candidate-final-pool64/contact.png` and individual frames, notably `quadratic-formula-guided-02.png`, `isolated-negative-physical-guided-00.png`, rational rejected/final pages and long pseudo pages. Repeated DOWN/UP commands clamp to actual content. `candidate-final-view-resources.json` and `candidate-final-profile-pixel-parity.json` summarize the measurements and 140-frame parity.

## Allocation-failure review

Source review found that a boundary failure returning no expression could enter AST constructors accepting empty slots, potentially publishing missing mathematics after a one-shot allocation failure. The author fixed `typedTerm` to fail before composition and added nonnull checks to rational conversion.

The isolated harness injects one-shot and persistent failures at scoped global C++ and MathNode allocation attempts during page preparation, including early allocations, spaced interior points and final allocations. **264/264** cases pass across coefficient, discriminant, quadratic formula, fractional balance, denominator condition and system-operation pages. **36/36** additional cases pass on the final plus-minus-metric build. Results and scripts are in `candidate-faults` and `candidate-final-faults`.

Every injection checks that no partial mathematical formula is published: the page either matches the healthy formula AST exactly or exposes the allocation-free unavailable message with no trusted formula references. BACK preserves the ordinary result; reopening reproduces the exact healthy formula AST, verified trace and unchanged tutor build count. Faults are scoped to page preparation, so this does not claim exhaustive coverage of initial LVGL widget allocation or Giac's C allocator.

## Assessment and remaining hardware evidence

The final candidate meets the measured host resource envelope with the existing 64 KB LVGL pool and bounded view ownership. The +24.4 KB flash cost and +96 B EquationsApp instance cost are explicit. No global arena or per-trace LVGL object tree was introduced. The engine's checked graph remains separate from view ownership; failure leaves the primary result and trace usable.

A physical-device measurement is still needed for task stack high-water, internal/PSRAM peak and fragmentation, bus/cache contention, and actual interactive latency. No flashing or invented hardware measurement was used to close those unknowns. Full mathematical and teaching reviews are separate from this resource assessment.
