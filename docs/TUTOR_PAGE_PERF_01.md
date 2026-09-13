# TUTOR-PAGE-PERF-01

The checked quadratic final page now reopens in 61.00 ms median (61.05 ms p95),
versus 395.10 ms on the matched baseline. The complex final page changes from
286.96 to 46.59 ms (46.66 ms p95). These are instrumented function timings on the
production WROOM PCB, not measured LCD first-paint latency. Each principal
workload retains all 30 samples after five warm-ups.

## Baseline and scope

`main` / HEAD remains `90fa81e1c5d1fd96325eec87e34daabc08390504`. Its closeout
reported `READY_FOR_TUTOR_PAGE_PERF: YES`; the preceding commits are `1b8b3ce`
(storage) and `6665d38` (Equations). The existing `.vscode/extensions.json` change
is unrelated and preserved. No index changes, commits, pushes, history edits,
new equation families, clock changes, or dependency upgrades belong to this task.

`out/tutor-page-perf-01/preservation/` contains readable separate binary index
and worktree patches, the editor file, baseline archive and path/hash manifest.
The baseline canonical fingerprint is
`751bf3e11a857fb49b49f9c7fe14eb9b81f1a256e63defc5add923fbb7c769ca`.
Its convention is sorted `path + NUL + SHA256(file bytes) + newline` records for
all archived regular files. Older reports use a different source-closure
convention; their aggregate hashes are not interchangeable.

All evidence paths below are relative to `out/tutor-page-perf-01/` unless stated.
Ignored evidence, snapshots, images and logs are intentionally not source files.

## Attribution and implementation

The only runtime change is `GiacEngine::tutorFormula` in
`src/math/giac/GiacTutor.inc`. Previously every displayed formula first entered
`capture()`, the bounded polynomial authored-input scanner. Closed square roots
and complex values deliberately fail its source-shape restriction. Presentation
caught and ignored that exception, then ran the domain verifier successfully.
The expected Xtensa exception unwind dominated the final-page cost.

For the warmed quadratic reopening, authored-condition capture alone took
333.17 ms median out of 395.10 ms opening time; for the complex page it took
240.07 ms out of 286.96 ms. The optimized final pages call capture zero times.
The quadratic formula-construction scope drops from 354.27 to 19.86 ms, while
publication stays about 23.0 ms. The expensive work was not the AST layout.

The boundary now first runs the **same** `domainSafe` verifier with an empty
condition set. A formula proved safe without assumptions needs no authored
polynomial scan. If that fails, presentation retains the previous capture,
exclusion and domain-check path. This is not a syntax shortcut or a numerical
proof. Both walks have the existing depth/node bounds; their symbolic-call
budget is shared. No checker, generator, rule, teaching parameter or renderer
implementation changed. The evaluation and structured conversion after the
check are unchanged. Differential tests include undefined denominators,
assignments, variable radicals, closed radicals and rational restrictions.

No page cache was introduced. There are no new retained strings, vector
capacities, AST owners, hidden LVGL trees or global cache arenas. The existing
transactional preparation/publication remains responsible for failure handling.
The view still owns each mutable AST exclusively and detaches its canvas before
destruction. Ordinary Giac answers and explanation generation remain on Solve.

Opening Steps validates the saved input/context, creates the bounded view,
resolves its teaching page, constructs formulas, and publishes text/ASTs.
Next/previous and guided/summary reconstruct the selected page. BACK retains the
checked derivation and page selection but destroys the view. Locale switching
renders the same graph with the chosen catalog; it cannot reuse wrong prose.
Scroll/pan operate on the current view. Committed edits and engine-generation
changes retain their existing invalidation guards; cancel retains the unchanged
input; HOME destroys app-owned state. None of these contracts was replaced.

## Matched measurement protocol

Both probe images use the same pinned toolchain, phase timers, production
hardware/display configuration and physical semantic-key sequence. Actual USB
identity was rediscovered: ESP32-S3 revision 0.2, USB serial
`44:B1:76:A7:B7:2C`, COM9 during this run, 16 MB flash / 8 MB PSRAM.
No COM number is an instruction for a future run.

Fixture order on each freshly booted probe: isolated, quadratic, complex,
linear, rational, unique 2x2, dependent 2x2, 3x3, wide rational. Complex policy is
explicitly selected only for the complex fixture; the others use real policy.
No Giac reset occurs between fixtures or reopenings. The real-board bridge
injects canonical matrix events, not equations/answers into an AST. USB input
is automation evidence, not human keypad/LCD confirmation.

Each fixture records first opening and first preparation of every exact page,
then four workloads: clamped RIGHT on the final page, BACK/TOOLBOX reopening,
LEFT/RIGHT revisit, EXE/EXE summary/guided round trip. Each has five warm-ups;
quadratic/complex have 30 measured repetitions, other fixtures five. Raw logs
retain both operations in paired workloads. Summary statistics for pairs name
the final guided page; ordinary solve/generation times are single calls, not
30 independent solve samples. First preparation is uncached-page cost in that
process, not a fresh Giac boot for every fixture.

`page` contains projection, formula construction and publication. Formula
construction contains authored AST clone/build and nested Giac parse, context,
capture, domain, evaluation and structured conversion. Publication contains
prose/caption work, canvas attachment, layout and body update. Do not add nested
scope totals or medians as independent costs. Printing happens after page
scopes stop. Existing key diagnostics remain in the broader dispatch path.
`flush_us` is the existing forced LVGL refresh boundary, including drawing and
transfer; it does not establish when a human sees the LCD's first pixel.

Raw board data: `{baseline,optimized}/board-benchmark/*.{json,jsonl,log}`.
`matched-timings.csv`, `first-preparation.json`, `dispatch-samples.csv`,
`dispatch-summary.json`, `counters.csv` and `memory-samples.csv` contain the
matched summaries and samples. `analyze-board.py` asserts cross-image state,
condition/final-answer equality, one proof build and zero polynomial capture
for the optimized principal workloads. All slow samples are retained; p95 is
nearest rank.

Host profiling was diagnostic: its sub-millisecond page timings misleadingly
favored LVGL publication and were collected while builds ran. An isolated
prose-layout batching experiment did not show a clear reopening gain and was
not adopted. Native timings do not close an ESP32 latency claim.

| Fixture | n | Baseline reopen median / p95 / max (ms) | Optimized reopen median / p95 / max (ms) |
|---|---:|---:|---:|
| isolated | 5 | 34.108 / 34.124 / 34.124 | 34.507 / 34.546 / 34.546 |
| quadratic | 30 | 395.096 / 395.185 / 395.229 | 61.002 / 61.053 / 61.055 |
| complex | 30 | 286.959 / 286.999 / 287.026 | 46.587 / 46.659 / 46.659 |
| linear | 5 | 40.706 / 40.722 / 40.722 | 39.182 / 39.202 / 39.202 |
| rational | 5 | 51.008 / 51.044 / 51.044 | 46.849 / 46.862 / 46.862 |
| system | 5 | 48.940 / 48.960 / 48.960 | 45.515 / 45.541 / 45.541 |
| dependent | 5 | 43.530 / 43.544 / 43.544 | 42.342 / 42.363 / 42.363 |
| system3 | 5 | 57.610 / 57.641 / 57.641 | 52.267 / 52.276 / 52.276 |
| wide | 5 | 54.821 / 54.832 / 54.832 | 49.044 / 49.069 / 49.069 |

The slightly slower isolated terminal page (about 0.4 ms) is retained in the
results; this change targets redundant algebraic scanning, not every UI cost.

| Single first-run call (ms) | Quadratic baseline / optimized | Complex baseline / optimized |
|---|---:|---:|
| Ordinary answer | 49.089 / 48.928 | 57.409 / 57.479 |
| Fresh checked derivation | 459.580 / 462.450 | 226.826 / 227.959 |
| First opening, page 1 | 69.298 / 60.744 | 45.925 / 43.631 |
| First preparation of final page | 387.667 / 36.829 | 262.191 / 21.661 |
| Warm final reopen dispatch median | 396.110 / 62.005 | 287.960 / 47.590 |
| Warm final reopen refresh median | 70.510 / 70.516 | 67.358 / 67.157 |
| Warm dispatch + refresh median | 466.634 / 132.517 | 355.311 / 114.758 |

The final page and first page remain different workloads. No first-preparation
cost was moved into Solve. Refresh remains a material part of response time.
All per-workload median/p95/max values, including repeat/revisit/mode and nested
phases for every fixture, are in `matched-timings.csv`.

## Resources and correctness

Ordinary production linked flash: 5,483,681 -> 5,483,749 B (**+68 B**).
Firmware image: 5,484,048 -> 5,484,112 B (+64 B, including image alignment).
Static DRAM remains 118,984 B; `.iram0.text` remains 60,407 B, vectors 1,027 B.
The `tutorFormula` Xtensa stack-frame prologue changes from 592 to 560 B; this
is one function frame, not total recursive/callee stack usage. On-board stack
high-water is the minimum unused task-stack space observed by this pinned IDF
port, in **bytes**, not words multiplied by four.
No heap allocations were added to layout/draw/cursor hot paths.

Additional retained presentation payload is zero because there is no new
retained representation. This is not zero dynamic heap cost: Giac parsing,
domain traversal, temporary vectors, conversions and AST construction still
allocate. The fallback may perform two bounded domain walks. Pool50 and board
samples characterize restoration and bounded live objects; they do not measure
the total simultaneous allocation peak or all Giac allocator activity.

Native fixed-pool lifecycle: 50 mixed cycles, 250 probes, 65,536 B configured
LVGL pool. Minimum sampled free pool 7,640 B; post-HOME free pool exactly
19,160 B across the run. Existing intermediate four-cycle allocator variation
remains bounded. Post-HOME: 67 objects, three timers, zero retained handles in
this native harness. Board screen/object counts differ and are reported as
board measurements rather than compared directly to native totals.

Board samples after the quadratic warm-up through all nine mixed fixtures:
internal free/largest blocks are 143,768 / 102,388 B baseline versus
143,840 / 102,388 B optimized at HOME. PSRAM free/largest are
8,226,323 / 8,126,452 B in both, with the last wide fixture ending 8 B higher.
Both retain 71 objects, five screens and three timers at HOME; Steps peaks at
100 sampled objects, with no additional per-page permanent objects. Minimum
sampled free PSRAM is 8,214,091 B baseline and 8,214,047 B optimized. These are
sampled current free values, **not** total allocation peaks. Stack high-water
reaches 52,080 B in both after warm-up. Heap minimum-ever counters and LVGL
used/fragmentation percentages are recorded separately in `memory-samples.csv`;
these are not simultaneous maxima to be summed. Existing warm-up retention is
visible; subsequent cycles show no monotonic decline in these samples.

Core failure injection: 96 persistent faults, all explicit Partial, no escaped
exception, 96/96 vector restoration and subsequent complete recovery. View
injection: quadratic 268, rational 86, fractional-linear 88 and system 88 cases,
including persistent faults. Existing all-or-unavailable publication and later
recovery assertions pass. Cache allocation/eviction/oversize tests are not
applicable; no cache was added. The physical board was not exhausted.

## Verification ledger

- Ordinary native, WROOM production and CAM `esp32s3_n16r8`: PASS.
- 180 tutor cases / 997 checked steps: complete payload equality with the
  accepted replay, excluding only elapsed microseconds; conditions, messages,
  branches and verification included. Checker/generator source before the
  presentation function is unchanged.
- 184 direct boundary cases: exact typed-node/status output equality against
  the matching baseline, including angle/stored-variable/assumption/Grapher
  preservation and engine reset. Standalone reproduction entry point passed.
- Seeded/challenge and 234 mathematical/wording mutations: PASS; no mutated
  trace becomes Verified. Composite-conclusion guard tests: PASS.
- Equations focused suite including 50 cycles: PASS. Full teaching suite:
  37 sequences, 139 pages, 280 frames; locales, summary/guided, provenance,
  signs, branches and wide pan: PASS. Pan returns to its initial viewport.
- 134 matched 320x240 screenshots: raw differences are confined to live clock
  digits, bounding rectangles (220/221, 8, 250, 17). No goldens or masks changed.
  Screens remain in `baseline/screens` and `native-final/teaching`;
  `same-page-contact-part*.png` provides readable before/after sheets.
- Ordinary Giac, Calculus and affected cross-app/Neo suites: PASS.
  Native Calculus/F7, Grapher Templates, STIX parentheses and plus/minus: PASS.
- Web build/package/validation/smoke and teaching Chromium/Firefox/WebKit: PASS.
  Headless WASM-MATH Release and Debug build/package/regression: PASS.
- Failure recovery and fixed-pool50: PASS, with the scope above.

Initial setup failures are retained rather than relabeled PASS: three native
suites initially could not load SDL (process exit 3221225781), then passed with
the pinned DLL on PATH; web/math packaging initially lacked Git metadata in the
clean ASCII snapshot, then passed using read-only baseline GIT_DIR plus the
candidate GIT_WORK_TREE. The initial board OTA inspection stopped before a write
because the helper's CRC convention was wrong; pinned ROM API/disassembly
resolved it. Probe setup also retained its first config-path/Arduino macro
failures. No tests or source semantics were weakened for these reruns.

## Reproduction and limits

Use existing pinned dependencies. The new Windows native profiler accepts an
ASCII source snapshot, its matching object directory and compile database:

```text
python scripts/profile-tutor-pages-native.py --source <snapshot> --build <emulator_pc>
  --database <compile_commands.json> --out <ignored-logs> --scratch <ASCII-probe>
python scripts/benchmark-tutor-pages.py --bin <probe.exe> --out <ignored-results>
  --samples 30 --warmup 5 --expect-no-polynomial-scan
python scripts/test-tutor-presentation-boundary.py --source <snapshot>
  --objects <matching-host-obj> --baseline-engine <baseline-GiacEngine.cpp.o>
  --out <ASCII-test-output> --compiler <pinned-g++>
```

These are single commands split visually across lines. Build the matching host
closure with `scripts/build-giac-host-harness.sh` if needed. Object hashes and
exact executed commands are retained; do not substitute an unrelated stale CAS
cache. The native counter gate fails on the old implementation (negative
control retained), so it detects the removed redundant scanner work rather
than relying on flaky host time thresholds.

Board reproduction reuses the reviewed acceptance input/telemetry overlay;
`create-board-probe.py`, `benchmark-board.py`, `board-batch.py`, overlay sources,
path/hash manifests and build commands are retained in this evidence directory.
They have no effect on ordinary firmware. Flashing is a separate authorized
application-only workflow, not an automatic benchmark action. The probe is
opt-in by isolated source/build directory, never enabled by ordinary config.

Teaching content and supported-family limits are unchanged. This corpus is not
a universal proof of correctness. There was no new independent teaching review
or expanded human physical acceptance; old human confirmation remains limited
to x=1 and TOOLBOX. Function budgets are met for the matched principal pages;
there is no claimed LCD first-paint measurement, full allocation-peak profile,
or speed guarantee for every supported input.

## Source, image and restoration identity

The ordinary compiled source fingerprint is
`7b1b5c484980eaba48870de6bf7fdc65522dfb65eb3f130857dc648d2655265d`.
`candidate-build-source.json` verifies every baseline archive file against the
isolated ordinary build input, substituting only the runtime `.inc` change.
Windows worktree CRLF conversion is distinguished from a Git source change.
Added tests, scripts and this report are deliverables, not firmware translation
units. The complete checked-kernel prefix hash is recorded separately.

| Image | Bytes | SHA-256 |
|---|---:|---|
| Matched baseline probe | 5,488,384 | `946efea71b59faf15a91adb4c6544cafdd592a2e809bfd854f9de6f4908cf901` |
| Optimized probe | 5,488,464 | `6a34d8201040704a5497c9ec07508435cce2034821d849bad5af4d573f6618c9` |
| Optimized ordinary candidate | 5,484,112 | `f23825d6cd7efce80ec57666302a8e6192ca211b7b5b852cf8101cdefc74f118` |

The old hardware-known-good ordinary image
`d2c2df2bd16cf6f1442aa1c9858227b6f74ac5c1cd5401924762bb6ad150a67b`
and the committed ordinary baseline
`d6a4af2e0801e8b6fb1e72eda49fcf551b2b76d90e76db7a4885c71adbc59914`
were independently hash-verified and retained for recovery. Prior physical
observations are not retroactively attributed to these new binaries.

Toolchain: PlatformIO 6.1.19; Espressif32 6.12.0; Arduino-ESP32 2.0.17 /
IDF 4.4.7; Xtensa 8.4.0+2021r2-patch5; esptool 4.9; LVGL 9.5; MinGW GCC
15.2.0; Emscripten 6.0.3; CMake 4.2.1; Ninja 1.13.2; pinned Playwright 1.54.1.
Existing production WROOM target, SAFE display profile, CPU flags and 64 KiB
LVGL configuration remain unchanged. Each variant uses its own ASCII source
and build directory; no probe object is linked into ordinary firmware.

The fresh task-specific authorization and reply are preserved in
`board-authorization.json`. Each installation independently reads security/OTA
metadata, backs up the current application, writes/verifies only active app0
at `0x10000` (size `0x640000`), and verifies the first 64 KiB are unchanged.
No bootloader/partition rewrite, filesystem/NVS formatting, eFuse/security
provisioning or display-clock change was performed. Boot diagnostics confirm
USB, PSRAM and successful LittleFS mounting; the existing variable-file
missing/invalid diagnostic remains visible and is not hidden by recovery.

The extra wide-fixture control cycle makes ten mixed solve/Steps/navigation/
BACK/edit-cancel/HOME cycles on the optimized probe, beyond the repeated warmed
page workloads. All nine pages can scroll to bottom and return; four VAR pans
perform zero additional page preparations; summary and reopening preserve the
proof. SHIFT/ALPHA indicators, HOME cleanup and empty re-entry assertions pass.
This is automated semantic/telemetry evidence, not human inspection of ten LCD
sequences. Final restoration/ordinary smoke status is recorded in the closure
entry below.

Exact source changes:

- `src/math/giac/GiacTutor.inc`
- `scripts/benchmark-tutor-pages.py`
- `scripts/profile-tutor-pages-native.py`
- `scripts/test-tutor-presentation-boundary.py`
- `tests/host/tutor_presentation_boundary.cpp`
- `docs/TUTOR_PAGE_PERF_01.md`

The pre-existing `.vscode/extensions.json` edit is outside this list and remains
untouched. Suggested future subject: `perf(tutor): reduce checked page preparation latency`.

Restoration completed: the ordinary **optimized** image above was application-
only written and independently verified; boot returned normally with USB,
PSRAM and LittleFS evidence. `board/optimized-ordinary-restored/identity.json`
records the installed hash and unchanged prefix. No probe commands/counters
are present in that ordinary image. Its baseline/probe timings must not be
misrepresented as direct measurements of the uninstrumented binary.

Final ordinary smoke: quadratic Solve / Steps / page navigation / summary /
BACK-reopen / edit-cancel / HOME passed the serial main-loop barriers with no
crash or reset. This uninstrumented replay does not expose proof/page counters;
those assertions belong to the matched probe and native tests. Ordinary exit
sample: internal free/largest 144,120 / 102,388 B, PSRAM 8,226,295 / 8,126,452 B,
stack minimum-unused high-water 52,368 B. These ordinary-image numbers are
reported separately from probe deltas. The PCB is left at HOME on the optimized
ordinary firmware. Final `git diff --check` passed; the index is empty and HEAD
is unchanged. No commit or push was made.
