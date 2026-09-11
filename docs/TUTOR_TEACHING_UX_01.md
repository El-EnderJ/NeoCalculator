# TUTOR-TEACHING-UX-01

Equations now opens a guided explanation with one instructional operation per
page. The existing checked derivation remains authoritative. Summary mode keeps
the intermediate equations; it no longer places several operations above only
their final result. Giac remains the ordinary answer authority. No equation
families, runtime AI, external services or competing solver were added.

## Preserved candidate

Work continued on `main`, HEAD
`1a95bb4f0f738fc653352c3fbd280fe08bc3685b`, including the uncommitted Equations
rebuild and TUTOR-ENGINE-01. The immediate 1,188-file source fingerprint is
`8b4220ade0d7b30dc9fadbe7f71b1eaba945ade476e96c583c236bc2c6be3c5e`.
The [source manifest and dirty overlay](../out/tutor-teaching-ux-01/baseline)
record the branch, index and working tree. Missing baseline executables were
rebuilt from that explicit snapshot, including uncommitted files.

No checkout, reset, stash, staging, commit, push or hardware flash was performed.
Production hardware configuration, the global header, transactional editing,
Templates, original-domain analysis and the ordinary answer adapter are preserved.

## Teaching projection

[`TeachingPlan.h`](../src/math/tutor/TeachingPlan.h) projects immutable proof
steps into guided or summary pages without retaining another page vector.
Resolving a page scans at most the existing 48-step bound. Mode and language
changes reuse the same derivation and source/context snapshot.

[`TutorPresentation.h`](../src/apps/TutorPresentation.h) identifies each formula
by kind, checked step, state, branch and row. Equations and restrictions come
from checked states; balancing and row notation come from declared operations;
quadratic facts come from the four checked auxiliaries. Authored equations are
cloned before simplification can cancel domain-bearing structure such as `x/x`.
The existing engine boundary converts mathematical expressions to the existing
VPAM nodes. Localized prose is never parser input.

[`TutorStepsView.inc`](../src/apps/TutorStepsView.inc) reuses four canvases and
four captions, two prose labels and a small verification badge. New formula
trees are prepared before publishing a page. Preparation is bounded to 640
nodes and 16 engine-boundary conversions. A failed preparation hides its
formulas and leaves BACK and the ordinary result usable. The badge describes the
retained derivation; the page reports its presentation failure explicitly.

Guided linear pages show the before equation, checked operation with its purpose,
and result. A quadratic formula rule has coefficient, discriminant, formula and
root pages: these expose data already checked by that compound rule. When its
input is `x²=-1`, the coefficient page explicitly shows the equivalent
zero-right polynomial `x²+1=0`, avoiding the misleading impression that `c=-1`.

Cases identify the active branch; unchanged branches are labelled separately.
The final screen lists every accepted solution and retained exclusion. System
rows use equation numbers and exact operation notation; a unique tuple remains
simultaneous, while a dependent family retains its equations relating variables.

## Wording, formulas and navigation

The existing [typed message catalog](../src/math/tutor/Messages.inc) now explains
the purpose of the actual operation: isolating a variable term, collecting
variable terms, isolating a variable or its square, or eliminating a named system
variable. The checker still regenerates and checks the message and typed
parameters. Complex expression operands are displayed as formulas, with whole
sentence templates referring to them. English is complete; representative
Spanish/French translations and explicit English fallback remain. No English
sentence fragments are assembled by rules.

Coefficients, discriminants, fractions, roots, ± and ≠ use VPAM/STIX structures.
Standalone unary signs use existing ordinary math atoms locally; parentheses
remain around negative factors and powered bases. No delimiter sizing or global
spacing formula changed. A separate visual defect was found in the old ± draw
branch: its two horizontal bars had the same vertical coordinate. That branch
now uses the existing STIX glyph path. A captured per-font width makes this
operator's box contain its glyph, including script profiles; other operator
widths are unchanged. The isolated regression checks both strokes and their
measured bounding box, with no allocation or font query in layout.

The title is student-facing (`Step 1 of 3`); a separate badge reports derivation
availability. Terminal screens have a conclusion title. Left/Right navigate,
Up/Down scroll within the actual content, EXE toggles guided/summary, VAR pans
wide math and cycles back to its start, BACK returns to results, and HOME exits.
Expanding a page and changing locale preserve vertical scroll within the new
bounds. The [emulator quickstart](emulator-sdl2-quickstart.md#equations-steps-desktop-controls)
documents the desktop equivalents, including F6 for Toolbox/Steps.

## Evidence and review

Three independent roles reviewed actual artifacts: mathematical validity,
teaching quality, and embedded allocation/lifecycle. The first screen review
found the ± and coefficient-normalization defects despite passing structural
assertions; both were corrected before final acceptance. Wording mutation
rejection is not presented as proof of teaching quality.

- [Complete before/after screen and teaching review](TUTOR_TEACHING_UX_REVIEW.md).
- [Linear, quadratic, rational and system walkthroughs, with every screen](../out/tutor-teaching-ux-01/review-teaching/walkthroughs.md).
- [Resource methodology, measurements and lifecycle review](TUTOR_TEACHING_UX_RESOURCES.md).
- [Native screen corpus and provenance assertions](../scripts/test-tutor-teaching-ui.py).
- [Math invariance and mutation replay](../scripts/tutor-teaching-math-review.py).
- [Immediate baseline and final evidence directory](../out/tutor-teaching-ux-01).

The mathematical comparison excludes changed message payloads and timing/storage
telemetry, then compares operations, states, conditions, branches, classifications,
checks and context snapshots. Native locale/mode comparisons also require the
same cached trace. These are bounded executable checks, not a universal proof of
every possible equation or of teaching effectiveness.

| Final validation | Result and evidence |
|---|---|
| Tutor corpus / invariance | 180 returned graphs, containing 997 steps, match the immediate mathematical baseline. |
| Independent rule replay / mutations | 139 steps across 38 challenge fixtures replay; 234 mutations yield 233 REJECTED, one UNKNOWN, zero VERIFIED. The UNKNOWN is the unsupported binding case. |
| Context / projection | 30 context checks, 10 snapshot-bound checks and 38 guided/summary projections pass. |
| Complete native teaching sequences | 35 sequences / 134 pages / 267 captures pass; the final glyph-width build reruns six affected sequences / 28 pages / 60 captures, including negative-b precedence. Wide-formula pan and exact return are separately checked. |
| Equations rebuild | 91/91 checks, including 50 edit/cancel/HOME cycles. The final non-Steps list/editor/template/result captures also match the immediate baseline exactly. |
| Shared math / glyph | Complete MathEnginePhaseRegression passes, including STIX parentheses and deep script checks. Actual plus-minus ink fits its measured box with two distinct horizontal strokes. |
| Cross-app | Calculus: 23 replay groups including F7 and 50 lifecycle cycles. Grapher Templates: 14 replay groups including all six insertions and 50 cycles. |
| Immediate visual baseline | Six full 320x240 non-Steps frames are pixel-identical; no masks used. No goldens were changed. The added MathVisual fixture changes its catalog count from 48 to 49; this is recorded diagnostic drift. |
| Fixed-pool / failure recovery | 50 cycles plus 140 page probes pass with the unchanged 64 KB LVGL pool; 264 faults plus 36 final-build faults recover without publishing incomplete formulas. |
| Builds / web | Native and production firmware pass. Headless WASM Release and Debug pass in Chromium, Firefox and WebKit; the packaged web teaching sequences pass in all three, as does the existing web smoke suite. |

Exact hashes, commands and scope are retained in the [mathematical regression
ledger](../out/tutor-teaching-ux-01/review-math/regression-ledger.json),
[teaching review](TUTOR_TEACHING_UX_REVIEW.md),
[resource review](TUTOR_TEACHING_UX_RESOURCES.md), and
[pixel comparison](../out/tutor-teaching-ux-01/baseline-ui-comparison/results.json).
The complete native matrix used the corrected teaching view before the final
plus-minus width adjustment; its affected formulas were recaptured afterward.

## Resource delta

| Measurement | Immediate baseline | Final candidate |
|---|---:|---:|
| Linked flash | 5,458,549 B | 5,482,961 B (+24,412) |
| Static DRAM / IRAM text | 118,984 / 60,407 B | Unchanged |
| Steps live LVGL widgets | 25 | 29 |
| Minimum sampled fixed-pool free | 9,336 B | 7,528 B |
| Peak additional simultaneous C++ and MathNode payload during page preparation | 3,241 B | 3,931 B |
| Maximum retained view nodes in the sampled pages | 35 | 54 |
| Median instrumented host preparation | 255 us | 523 us |
| Xtensa drawStep own stack frame | 528 B | 752 B |
| Xtensa EquationsApp instance size | 972 B | 1,068 B |

These are measured sample envelopes, not maximum total hardware heap or stack.
The page sets differ because guided mode exposes more pages. The payload figure
includes old/new page overlap but excludes direct Giac C allocations and allocator
overhead. The 640 prepared-node limit excludes the previous page and four wrapper
rows. Within Equations the fixed pool has a bounded four-cycle pattern; HOME
restores 19,160 free bytes every cycle with three timers and zero retained Grapher
handles. Unchanged static RAM does not imply zero heap cost.

The final native candidate is
[`native-release.exe`](../out/tutor-teaching-ux-01/native-release.exe), SHA-256
`bf3c1521b58c1d8c1eaf94fa1efb31dd213abee7535c2ea2ebebfcf8de5388e0`.
Final firmware and fixed-pool binaries are retained in
[`review-resource/final-binaries`](../out/tutor-teaching-ux-01/review-resource/final-binaries).

## Remaining boundaries and hardware acceptance

Coverage remains the [existing supported-family matrix](TUTOR_ENGINE_01.md#supported-methods):
exact rational linear work, supported degree-two products/quadratics and rational
equations, terminal classifications, and linear 2×2/3×3 systems. Variable radicals,
absolute values, exponentials, logarithms, trigonometry, higher degrees and
numerical methods still require future checked methods. Full Spanish/French
translation is a separate content task.

Host allocation payload, native latency, linked sizes and Xtensa own frames do
not substitute for physical ESP32 heap and stack high-water measurements. Before
hardware acceptance:

1. Open linear, quadratic, rational and 3×3 explanations using the physical keys;
   inspect every page, branch, exclusion and final tuple at 320×240.
2. Confirm scrolling, wide-formula pan, BACK and HOME, plus SHIFT/ALPHA badges
   after editing with the physical negative key.
3. Measure internal/PSRAM free and largest blocks, task stack high-water and page
   latency during repeated solve/Steps/edit/cancel/HOME cycles; confirm restoration.
4. Exercise low-memory recovery while retaining a usable ordinary Giac result.

No physical-device measurement or flashing is claimed by this delivery.

## Exact changed source files

Relative to the immediate uncommitted tutor baseline:

- `docs/TUTOR_TEACHING_UX_01.md`
- `docs/TUTOR_TEACHING_UX_RESOURCES.md`
- `docs/TUTOR_TEACHING_UX_REVIEW.md`
- `docs/emulator-sdl2-quickstart.md`
- `scripts/test-plus-minus.py`
- `scripts/test-tutor-engine.py`
- `scripts/test-tutor-teaching-ui.py`
- `scripts/test-tutor-ui.py`
- `scripts/tutor-teaching-math-review.py`
- `scripts/tutor-teaching-web-review.py`
- `scripts/tutor-teaching-web.mjs`
- `src/apps/EquationsApp.cpp`
- `src/apps/EquationsApp.h`
- `src/apps/TutorPresentation.h`
- `src/apps/TutorPresentation.inc`
- `src/apps/TutorStepsView.inc`
- `src/math/MathAST.cpp`
- `src/math/MathAST.h`
- `src/math/MathRenderVisualCases.cpp`
- `src/math/giac/GiacTutor.inc`
- `src/math/tutor/Derivation.h`
- `src/math/tutor/Messages.inc`
- `src/math/tutor/TeachingPlan.h`
- `src/ui/MathRenderer.cpp`
- `src/ui/MathRenderer.h`
- `tests/MathEnginePhaseRegression.cpp`
- `tests/host/tutor_teaching_math.cpp`
- `tests/wasm/smoke.mjs`

The [final source manifest](../out/tutor-teaching-ux-01/final-source-manifest.json) records every source hash, final Git state and comparison with the baseline. [The change manifest](../out/tutor-teaching-ux-01/changed-files.json) records before/after hashes. Build outputs, screenshots, replays and profiling helpers are retained under `out/tutor-teaching-ux-01` and are not part of this source list. The build-generated compilation database was restored to its recorded clean initial state; its profiling copy is retained in the resource evidence.
