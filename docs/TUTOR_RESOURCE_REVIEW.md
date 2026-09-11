# TUTOR-ENGINE-01 resource and lifecycle review

Independent embedded review. No hardware was flashed. The source candidate and its uncommitted Equations rebuild were preserved.

## Immediate baseline

Source fingerprint: `a0176658aca637b864da0418595223ff85b6ee691b3553766d277bb0de3ccdf6`.
The copied EquationsApp header/implementation, NativeHal and GiacEngine hashes equal the frozen source overlay, and their timestamps precede the native/production links. Exact executable, ELF and binary copies are under `out/tutor-engine-01/baseline/binaries`; hashes and times are recorded in `out/tutor-engine-01/review-resource/baseline-binaries.json` and `baseline-source-validation.json`. The existing fixed-pool executable is also preserved there as `program-pool64.exe`.

| Immediate baseline metric | Bytes |
|---|---:|
| Linked production flash | 5,403,605 |
| Firmware binary | 5,403,968 |
| Static DRAM (`.dram0.data` + `.dram0.bss`) | 118,960 |
| `.dram0.data` | 21,240 |
| `.dram0.bss` | 97,720 |
| `.iram0.text` | 60,407 |

Values were remeasured from the preserved production ELF with the installed Xtensa size tool. The unchanged production partition allows 6,553,600 linked flash bytes, leaving 1,149,995 bytes at baseline. No platform settings or LVGL allocation settings changed.

The preserved native executable passed all 91 focused Equations checks again, including physical keys, templates, the existing answer-check view, deliberate negative assertions and 50 mixed lifecycle cycles. Evidence: `review-resource/baseline-native/`. The contact sheet was opened for inspection. Native CLIB reports zero for unavailable heap/pool measurements; these are not zero-byte allocation measurements.

A separate 50-cycle replay of the preserved **64-bit host fixed 64 KB pool** executable samples results, Steps, edit/cancel/delete and HOME on each cycle:

| Phase | Live LVGL objects | Pool free bytes after warm-up |
|---|---:|---:|
| System results | 20 | 11,552 |
| Existing answer-check Steps | 15 | 13,480 |
| Post-edit/delete | 21 | 11,136 |
| Menu after teardown | 67 | 19,160 |

All 200 probes complete; each phase stabilizes after ten cycles, with three timers and zero retained Giac handles throughout. Minimum sampled pool free is 11,136 bytes. This differs slightly from the previous report's wider probe set (10,792 minimum); these are sampling points, not an allocator-wide high-water mark. Host pointer/object sizes differ from ESP32. The probe's `pool_total` field varies with allocator bookkeeping and must not be mistaken for the configured 65,536-byte slab size.

## Engineering budgets

The implemented limits are 48 instructional steps, two candidate branches, three input equations, eight retained exclusions, 512 aggregate authored-source bytes, depth 20 and 160 authored expression nodes. Retained representation is capped at 64 KB; aggregate tracked vector payload during construction is capped at 128 KB. Individual vector allocations are capped at 64 KB. They are separate on-demand allocations outside the LVGL pool, explicitly requesting PSRAM on firmware; no global slab is reserved.

The symbolic-operation budget was raised from 1,200 to 4,096 after seeded dense 3x3 systems demonstrated that the smaller budget cut valid proofs short. The final 180-case run uses at most 1,571 counted operations. This bounds declared primitive calls, not the execution time or temporary memory inside a synchronous Giac operation. Firmware yields cooperatively after a committed primitive; this does not make a Giac call cancellable.

Account for capacities, nested string/vector ownership and retained presentation trees, not only `sizeof(Trace)` or static RAM. Reject budget exhaustion before publishing a complete trace. Giac parser/symbolic temporaries and rendered VPAM trees are separate transient allocations and need separate reporting. Bound traversals by depth/node count before expensive algebra; coefficient size and Giac output growth still need honest limits. Never place a 64 KB trace object on the 64 KB Arduino loop task stack.

Use one bounded visible step group and reuse its canvases/text widgets. Detach MathCanvas expression pointers before releasing presentation trees; retain the derivation independently of the view. Preserve existing epoch invalidation on commit/delete, cancel preservation and HOME teardown. Reopening Steps should not allocate another trace or trigger another solve. Zero allocations in MathAST layout/draw/cursor paths remain mandatory; this review does not recommend changing their geometry.

Baseline Xtensa own-frame evidence from the Equations rebuild is: update 48, formula 64, editor handler 64, main key handler 80, showResult 304, solveEquations 592 and old tutor candidate 1,792 bytes. The 2,384-byte solve-to-old-tutor sum excludes callees and is not a full stack high-water measurement. New compiler stack-usage files or disassembly must be evaluated separately. Physical internal/PSRAM peaks, task stack high-water and cache timing are unmeasured without hardware; native memory telemetry cannot stand in for those values.

## Regression setup

Production/native PlatformIO output is `C:/.piobuild/numOS/<environment>`, configured by the existing `platformio.ini`. Run builds sequentially per shared output directory; do not flash:

```text
pio run -e emulator_pc
pio run -e numos-esp32-s3-wroom-1u-n16r8
python scripts/test-equations-rebuild.py --out out/tutor-engine-01/final-equations
python scripts/test-calculus-rebuild.py
python scripts/test-stretchy-parens.py
python scripts/test-grapher-template-previews.py
python scripts/generate-emulator-candidates.py --out-dir out/tutor-engine-01/final-candidates
```

Each focused runner accepts `--bin` for archived/comparative executables. Existing host build scripts are `scripts/build-giac-host-harness.sh` and `scripts/build-cas-host-tests.sh`; on this Windows host invoke Git Bash explicitly (`C:/Program Files/Git/bin/bash.exe`), not the unrelated system `bash.exe`. The Giac script accepts `GIAC_HOST_OUT`, `JOBS` and `--build-only`; the existing cached final harness is `out/equations-rebuild/final/giac-host`. Its executable suites include engine, calculus, Neo and cross-app. Preserve known RSS-guard fluctuation evidence and do not adjust thresholds to force passes.

At baseline, the ASCII candidate worktree was `C:/.piobuild/numOS/equations-candidate`, with an uncommitted candidate overlay. Those old caches were subsequently removed outside this review. The main implementation/review team owns the final synchronized WASM snapshot and regression evidence. Recreating a build copy requires all intended uncommitted and untracked source, with its fingerprint; plain HEAD is insufficient. The available toolchain is `C:/.codex-cache/emsdk/upstream/emscripten`; Ninja is `C:/mingw64/bin/ninja.exe`.

After synchronizing the source and CMake source lists, rebuild each cache with `cmake --build <cache> --parallel 8`; package from the ASCII worktree with `node wasm/package.mjs Release` and `node wasm/math/package.mjs Release` / `Debug`. Browser validation uses `npm --prefix tests/wasm run smoke` and `npm --prefix tests/wasm run math` / `math -- --debug`. Playwright is pinned to 1.54.1; no upgrade is needed. The CMake targets write raw output to that worktree's `out/wasm` / `out/wasm-math`, which the package scripts consume.

Compare immediate-baseline screenshots below the wall-clock header, while separately checking header/modifier behavior. The prior Equations rebuild documented 18 golden differences and 24 missing goldens; no goldens/masks should be changed or promoted automatically.

## Candidate review status

The actual new tutor source received an independent lifetime/allocation review. Transactional rollback when checking throws, enforcing the retained cap after insertion, counting work on failure paths and including copied input vectors in allocation metrics were corrected and rechecked in source and executable probes.

### Confirmed vendored complex-display undefined behavior

Twenty fresh host processes solving `x^2+1=0` in complex mode produced five complete traces, ten checked refusals containing `undef`, and five reconciliation failures. The checker did not bless the failed traces. A separate raw Giac probe reproduced nondeterministic complex **printing without invoking Tutor at all**: `i`, `sqrt(-4)` and `-sqrt(-4)/2` occasionally printed polar strings such as `1∡0` or `2∡0` instead of Cartesian exact values. Evidence is `review-resource/complex-repeat-*.json`, `raw-complex.cpp`, `raw-complex.log` and `raw-complex-layout.log`.

The defect is `lib/giac/src/kgen.cc::complex_display_ptr`: subtracting one `int` from the real-component pointer assumes that `display` immediately precedes it. The actual Windows compiler reports `sizeof(ref_count_t)=8`, `offsetof(ref_complex,display)=8`, and `offsetof(ref_complex,re)=16`; the old accessor addresses offset **12**, which is uninitialized alignment padding. The same accessor is also written by complex-display propagation. The required repair addresses the actual member using the object layout, including SMARTPTR64, instead of relying on adjacency. Merely replacing the `cst_i` alias cannot repair newly allocated complex values.

The implementer's actual member-access repair was independently retested: 100 fresh raw-Giac processes produce identical Cartesian values and 100 fresh Tutor processes complete `x^2+1=0` in complex mode, including independent kernel replay. Evidence: `review-resource/complex-fixed-repeat.json`. This validates the specific portability repair; it is not a universal symbolic-correctness claim.

The budget rollback concern has an executable reproducer: `1/(x-1)+1/(x-2)+1/(x-3)+1/(x-4)+1/(x-5)+1/(x-6)+1/(x-7)=0` returned Partial with eight states and six steps during intermediate review, violating the one-more-state invariant after a symbolic-call exception. It also reported zero symbolic calls. The repaired 1,200-call build returned Partial with seven states, six steps and 1,201 attempts (the last throws before execution). With the final 4,096 limit it reaches the degree refusal honestly: Unsupported, eight states, seven verified domain annotations, 1,308 calls. It does not display a completed explanation. Evidence retains both intermediate and final outcomes.

Native compiler own-stack estimates include `explainEquations` 848 B, `Planner::single` 3,136 B, `Planner::system` 1,792 B and `verifyStep` 1,952 B. These exclude callees and are not ESP32 measurements; actual Xtensa own frames are reported below.

## Implemented limits and measured candidate

The implementation explicitly enforces the retained and transient vector budgets described above using `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` on firmware. The rollback guard restores step/state counts if checking throws. All 300 final allocation replays preserve the one-more-state invariant.

The existing production `GiacAlloc.cpp` global C++ allocator also prefers PSRAM, with an any-8-bit-heap fallback. Thus ordinary strings/Giac C++ allocations are not necessarily internal DRAM; their actual fallback placement has not been measured on hardware. The new vector allocator deliberately has no internal-heap fallback. LVGL retains its independent 64 KB internal pool.

An isolated host executable overrides ordinary C++ `new`/`delete` to count requested payload, in addition to the real `TraceAllocator` counters. Ten cases were repeated thirty times each, including already solved, distributed linear, irrational and complex quadratics, excluded rational roots, conditional identity, three 3x3 systems (including the seeded case with the largest retained trace) and unsupported degree. The probe and linker response files are under `review-resource/trace-*-probe.*`; `allocation-final-summary.json` records the final cases.

| Sampled maximum | Bytes |
|---|---:|
| Retained C++ capacity estimate | 13,456 |
| Retained tracked vectors | 10,614 |
| Construction peak tracked vectors | 11,496 |
| Additional ordinary C++ requested-payload peak | 14,984 |

These columns are distinct measurements, not an allocator-wide simultaneous peak. Capacity accounting includes inline-string capacity conservatively; actual heap ownership differs. The ordinary C++ probe covers strings and Giac C++ allocations, but excludes direct C `malloc`/libtommath, target heap metadata and physical PSRAM/internal fallback placement. The instrumentation itself adds allocation headers; only requested payload is reported.

All ten cases restore both tracked-vector and ordinary C++ payload after warm-up. The first irrational quadratic retains 224 B of Giac context scratch; the final unsupported-degree case retains 1,536 B (the earlier 1,200-call Partial case retained 96 B). Twenty-nine subsequent identical explanations retain zero additional bytes; the isolated final `engine.reset()` probe releases the scratch. Reset is not used or proposed in the production tutoring path. Alternating ordinary solve/explain replaces context scratch, which initially looked like per-call growth; consecutive replay and reset controls establish bounded retention for these samples.

Instrumented native warm median construction times range from approximately 0.04 ms to 6.55 ms across these samples, with concurrent builds causing timing jitter. These are host measurements, not ESP32 latency predictions. No synchronous Giac operation is presented as preemptible.

The allocation-failure handler was also checked independently. Its old attempt to assign a diagnostic string could allocate while memory was already exhausted; the final handler clears the diagnostic without allocation and preserves Partial status. A host probe keeps ordinary C++ allocation failure armed through the handler and injects failure at each of the first 96 allocation positions: 96 Partial returns, zero escaped exceptions, 96 tracked-vector restorations, followed by a healthy Complete explanation. The preserved pre-fix object fails the same control at all 96 positions by letting an exception escape. Source, objects, linker responses and results are in `allocation-failure-*.{cpp,rsp,log,json}`. This checks those failure sites, not every possible Giac/C allocation failure. Snapshot counts, string lengths and duplicate identifiers are additionally bounded before copying or shared-context parsing.

The final production-normal build completed successfully with unchanged hardware configuration (`production-physical-final-build.log`, 192.47 seconds). Measured flash is 5,458,549 B (+54,944); static DRAM is 118,984 B (+24); `.iram0.text` remains 60,407 B (0 delta). Binary size is 5,458,912 B. `production-resources.json` contains the ELF/bin hashes, source hashes, sections and disassembly-derived own frames. The firmware bin SHA256 is `85f3a5b47be2621395b6bfdaa1567e199e8a44d9fbfff8b91911182c3144d5ea`.

Actual Xtensa own frames: app solve 480 B, showSteps 80 B, drawStep 528 B; engine explanation 544 B; planner run 576 B, single 1,168 B, system 736 B and add 240 B; checker 512 B. `domainSafe` uses 128 B per recursive frame with explicit depth bounds. These are own frames; their callers, Giac internals and exception unwinding contribute additional stack. No full physical task-stack high-water mark, internal/PSRAM heap high-water, fragmentation or cache-timing claim is made.

## Final fixed-pool lifecycle and visual review

An isolated native configuration copied the exact current `platformio.ini`, changing only its output path and the native LVGL allocator from CLIB to BUILTIN. It preserves the 65,536-byte LVGL pool. The final refresh passed in 10.92 seconds, without changing the production configuration. Its ASCII build directory is `C:/.piobuild/numOS/tutor-review-pool64`; a Unicode output path initially exposed an archiver response-file failure and was replaced only in the isolated configuration. The failed setup logs are retained.

The final replay performs 50 solve / Steps / detail navigation / close / reopen / edit-cancel / delete / HOME / re-entry cycles, with 250 probes. Reopening asserts a single cached trace build. Each phase stabilizes after warm-up:

| Final phase | Live LVGL objects | Pool free bytes |
|---|---:|---:|
| System results | 20 | 11,504 |
| Steps | 25 | 9,376 |
| Reopened Steps | 25 | 9,392 |
| Post-edit/delete | 21 | 11,192 |
| Menu after teardown | 67 | 19,160 |

All phases retain three timers and zero Grapher handles. Minimum sampled pool free is 9,336 B. This is a stable, bounded native view allocation, not a physical-device heap high-water claim. Relative to the immediate baseline, Steps uses ten additional LVGL objects and approximately 4.1 KB more sampled pool storage; Menu restoration is identical. The normal CLIB-native replay also passes fifty cycles with the same object/timer/handle counts, but its zero heap fields mean unavailable telemetry.

Final evidence is `review-resource/pool64-physical-final/lifecycle.json`, the executable `.numos` replay and its full log. The contact sheet and twenty-five individual 320×240 PNGs in the same directory show physical entry of `(2*x-3)*(3*x+4)=0` and `x=-3`, annotated branch operations, denominator exclusion and candidate rejection, negative scalar and complex roots, and pseudolocalized prose with scrolling. Every capture checks layout bounds and a structured formula tree. The reviewer opened the actual contact sheet and individual physical-negative and complex captures: active alternatives are labeled/highlighted, rejected `x=1` is marked and omitted at the rational terminal state, and negative `-1`/`-3`/`-i` remain visible. The prior twenty-four images remain pixel-identical below the clock header (`prior-pixel-comparison.json`).

Visual review found and resolved a real intermediate defect: nested unary-negative rows overlapped the preceding equals sign. The narrow view conversion now wraps signed subformulas in existing authentic STIX parentheses, including unchanged authored rows and the Variable `=` token produced by physical input. The final `x=-3` replay uses physical matrix events throughout and shows exactly one already-solved explanation. MathAST geometry and layout/draw/cursor hot paths are unchanged. Pre-fix screenshots remain in intermediate evidence directories and are not the final contact sheet. Longer content is scrollable; the view does not allocate one LVGL tree per derivation step.

Physical ESP32 latency, total heap peak (including direct C allocations), allocator fragmentation, capability fallback distribution and complete task-stack high-water remain hardware-only validation items. No hardware was flashed. Review-owned source change: this report; all executable probes, logs, measurements and screenshots are evidence under `out/tutor-engine-01/review-resource`.
