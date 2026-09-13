# MATH-NOTATION-01

Generated scalar mathematics now uses conservative juxtaposition. The checked
factorization of `2*x^2+3*x-5=0` displays **(2x+5)(x−1)=0**. Quadratic quantities
use **U+0394 Δ**, with the broad authentic Latin Modern Math/TeX outline requested
by the user. No rule, solution family, checked parameter, message catalog,
solver/context implementation or page-generation policy changed.

## Baseline and scope

Started on `main`, `62a4dc33136162aaf1ed74b4d2a81c5783d7b9ca`, with a clean index
and working tree. This already contains the committed tutor page optimization.
The original full-source fingerprint is
`cd7215d4eaec56584f6c5f2d9836bd8bb020d3684d9ea9a0bf5944aba95d4570`.
The readable archive, separate binary-capable patches, manifests, worktree list
and original editor file are preserved under `out/math-notation-01/`.
An external change subsequently added `pioarduino.pioarduino-ide` to
`.vscode/extensions.json`'s unwanted recommendations; it is preserved separately.

Candidate source manifest: `out/math-notation-01/final-source.json`. Its explicit
source/test/tool/font scope excludes reports, editor settings and ignored output.
Fingerprint:
`1e92aacb46aaac7ef901adfc2a59345a3663b843f6bc168f8778b4a70f12620b`.
This scoped hash is not directly comparable to the original full-tree hash.
HEAD and the index remain unchanged. No flash, commit, push or storage operation.

## Diagnosis and implementation

The physical authored AST uses existing adjacent VPAM operands. Its canonical
equation remains `2*((x)^(2))+3*x-5=0`. The checked `polynomial.factor` rule
retains `2*x+5` and `x-1` and its FormulaRef identifies the checked resulting
state. Giac's structured tree represents their multiplication as `Mul`.
`CalculationEngine::resultTreeToAST` previously converted every such boundary
into `NodeOperator(Mul)`, whose existing renderer draws ×. The baseline AST
dumps and full screen sequence reproduce this difference.

`GeneratedMathNotation.h` adds an explicit opt-in `ScalarNatural` policy at
read-only construction. It reuses VPAM's existing implicit-product Row form;
`serializeForGiac` already restores `*` between adjacent operands. The original
typed `EngineResultNode::Mul`, its operands and order are untouched. There is
no string deletion, new parser, mathematical simplification or draw-time rule.
The default conversion remains Explicit. User-authored ASTs are not normalized.

The single policy serves Equations results, tutor formulas and Calculus results.
Authored/input formulas, programming/source views, persisted expressions,
specialized apps and default callers remain unchanged. Grapher template previews
remain on their existing path. Collections/matrices/unknown calls do not acquire
scalar adjacency. Explicit instructional products remain explicit, including
system row multipliers and discriminant numeric substitutions.

### Exact conservative decision table

Known scalar symbols in these callers: `a,b,c,x,y,z,A,B,C,D,E,F`. This is a
finite caller contract, not an attempt to infer units/types from typography.
Single-child rows are transparent. A power of a known symbol has that symbol's
factor identity. A group must contain scalar algebra and a known symbol, and
must not start with a unary sign. Fractions, roots and built-in functions must
also contain only supported scalar nodes.

| Left factor | Right factor | Presentation |
|---|---|---|
| Any unknown/unsupported factor | Any, or conversely | Explicit × |
| Any | Number or fraction | Explicit × |
| Number, fraction, group, root, distinct known symbol/power | Known symbol/power | Juxtapose |
| Function or same base symbol/power | Known symbol/power | Explicit × |
| Number or algebraic parenthesized group | Algebraic parenthesized group | Juxtapose |
| Other supported factor | Parenthesized group | Explicit × |
| Supported scalar factor | Root or built-in function | Juxtapose |
| Any | Explicit instructional policy | Explicit × |

Thus `2x`, `3x²`, `ax²`, `4ac`, `2(x+1)`, `(x+1)(x−1)`, `2√x`, `2 sin(x)`
and a stacked rational coefficient followed by x are supported. `2×3`, decimal
and scientific-number pairs, `2×(1/3)`, `2×(−3)`, `(x+1)×2`, `x×x`, `ab×cd`,
`x×(x+1)`, unknown identifiers and unsupported non-scalars retain signs.
No factor is reordered. The first signed scalar coefficient may lose redundant
outer parentheses structurally; right negative factors and power bases keep them.

Existing TeX inter-atom spacing supplies the function gap. A default-false
NodeFunction presentation flag changes only its generated left boundary to OP;
cloning preserves it. No literal spaces, negative offsets, font shrinking,
new renderer product logic or editable-input geometry changes were introduced.

Traversal and classification are each bounded by 512 nodes and depth 32.
Exhaustion retains explicit signs. No new allocations occur in layout/drawing.
Construction still uses the existing transactional ownership/failure boundary.

## Discriminant and font

The private `Quantity::Discriminant` builder produces a Unicode NodeSymbol with
U+0394. Definition, value substitution and the general formula all use this
role, never a one-byte variable or a Giac binding. A user identifier D remains D.
The value still comes from the verified quadratic auxiliary parameter.

Displays include `Δ=b²−4ac`, `x=(−b±√Δ)/(2a)` and, for **−4**, the checked
`Δ=3²−4×2×(−4)=41`. The **−5** reproducer uses factoring and does not display a
discriminant page; it has not inherited 41 from the other fixture.

Compiled STIX already contained U+0394, but its outline was narrower than the
requested TeX style. Only Delta is routed to a renamed one-glyph Latin Modern
Math 1.959 subset. All other STIX glyphs, authentic parentheses, ± and fonts
remain unchanged. This is the actual TeX font outline, not U+2206, a triangle
icon, a stretched glyph or a claim of exact geometric equilateralness.

| Nominal font size | Existing STIX Δ ink | TeX Δ ink / advance |
|---|---|---|
| 18 px | 12×12 px | 15×13 px / 15 px |
| 12 px | 8×8 px | 10×9 px / 10 px |
| 8 px | 6×5 px | 7×6 px / 7 px |

The canvas captures actual glyph metrics once per font profile; NodeSymbol
layout and the Unicode draw path use the same font selection. Real raster tests
bound all ink in standalone, radical, fraction and nested-script boxes. Native,
production, CAM and web builds include the three tiny generated C fonts.
Offline regeneration is byte-identical with `lv_font_conv 1.5.3`.

The 2,028-byte renamed OTF subset, GUST/LPPL notices, original font hash and
reproducible generator are included. See
[`MANIFEST-NumOSDiscriminant.txt`](../assets/fonts/MANIFEST-NumOSDiscriminant.txt)
and `THIRD_PARTY_NOTICES.md`. No full additional font is embedded.

## Verification and visual evidence

All evidence is under ignored `out/math-notation-01/`:

| Gate | Result |
|---|---|
| Structured notation/serialization matrix | 30 cases PASS; immutable input tree, order, explicit fallback, canonical serialization, clone/idempotence and row width |
| Existing MathEnginePhaseRegression | PASS, including editor/cursor geometry |
| Full checked replay | 180 traces / 997 steps identical; only elapsed `micros` excluded |
| Actual teaching captures | 16 before/after trace pairs identical; 611 formula instances checked; page/message/ref provenance unchanged |
| Teaching mathematical mutations | 21 mutation categories PASS |
| Composite conclusion guards | 35 mutations rejected; six fixtures / 20 checked steps unchanged |
| Engine allocation recovery | 96 fault points, no escaping failure, recovery complete |
| Page allocation recovery | 246 once/persistent fault cases across quadratic, fraction, rational and system pages PASS |
| Fixed 64 KiB LVGL pool | 50 mixed cycles PASS; four-period bounded active-page allocation pattern, exact post-HOME restoration |
| Equations, teaching/locales/wide pan, Calculus/F7, Grapher Templates, STIX, ± | PASS |
| Ordinary Giac / Calculus / cross-app / Neo math | PASS |
| Native / production-normal / CAM-normal | PASS |
| Web package/smoke and teaching in Chromium/Firefox/WebKit | PASS |
| Headless WASM Math Release and Debug | PASS |
| Whitespace check | PASS |

The native notation unit uses the existing serializer, not a test parser. Some
generic-symbol, scientific-text or collection ASTs already lack a supported
VPAM-to-Giac copy path: its availability is asserted unchanged, and the original
typed result remains authoritative. No unsupported export is claimed to work.

Captures are exactly 320×240; contact sheets retain every scrolled frame:

- [Before factorization](../out/math-notation-01/before/notation-factor-guided-contact.png)
  / [after, complete sequence](../out/math-notation-01/after/notation-factor-guided-contact.png).
- [Before quadratic](../out/math-notation-01/before/quadratic-guided-contact.png)
  / [after, complete sequence](../out/math-notation-01/after/quadratic-guided-contact.png).
- [Glyphs and safe explicit products](../out/math-notation-01/glyphs-final/contact.png).

These images were inspected directly, including the two quadratic roots and
active factor branches. Narrower products change measured widths without
changing page count, prose, branch order or visible font sizes. Additional
linear/rational/system/complex/locale/wide captures are in `after/`.
No golden promotion or mask expansion. The diagnostic MathVisual catalog grows
from 49 to 58; existing case indices are unchanged. Its total-count label is an
intentional diagnostic screenshot difference. Prior golden drift remains
historical evidence, not a new pass claim. No independent reviewer was used.

## Resources and performance

Production target remains `numos-esp32-s3-wroom-1u-n16r8`, pinned ESP platform
6.12.0 / Arduino 2.0.17 / Xtensa 8.4.0 / LVGL 9.5.0. Native MinGW 15.2;
WASM Emscripten 6.0.3. Build directories are isolated from the accepted baseline.

| Measurement | Immediate baseline | Candidate |
|---|---:|---:|
| Linked flash | 5,483,749 B | 5,486,773 B (+3,024) |
| Firmware image | 5,484,112 B | 5,487,136 B |
| Static RAM | 118,984 B | 118,984 B |
| IRAM text | 60,407 B | 60,407 B |
| Xtensa NodeFunction | 44 B | 44 B (flag fits padding) |
| Xtensa FontMetrics | 32 B | 36 B |
| Post-HOME native LVGL free | 19,160 B | 19,160 B |
| Minimum sampled pool free in 50 cycles | historical comparison in prior report | 7,640 B |

Three FontMetrics per canvas add 12 B of C++ object payload on Xtensa, including
48 B across the four result canvases; this is not zero dynamic cost. There is
no additional permanent LVGL object or retained page cache. The isolated Xtensa
`-Os -fstack-usage` probe reports 64 B for an `apply` frame and 32 B for a
`scalarWalk` frame, with depth bounded at 32; this is not a board task-stack
high-water measurement or a total-call-stack bound.

Matched host page instrumentation counts C++ requested allocation bytes and
MathNode requested bytes together at allocation events, excluding malloc
metadata, direct Giac C allocation, LVGL and total process memory. Maximum
simultaneous extra payload during the sampled quadratic sequence falls from
3,931 to 3,771 B; factorization 3,190 to 3,110 B. These are not sums of independent
heap maxima. Raw page records are in `allocation-comparison/results.json`.

Thirty warmed final-page reopen samples per fixture, after five warmups:

| Fixture | Before median/p95/max ms | After median/p95/max ms |
|---|---|---|
| x=1 | 0.774 / 3.062 / 9.173 | 0.749 / 6.349 / 6.424 |
| Linear | 0.878 / 1.344 / 10.545 | 0.765 / 1.406 / 5.165 |
| General quadratic | 1.272 / 5.945 / 6.086 | 1.193 / 1.796 / 2.127 |
| Complex | 1.118 / 5.646 / 5.926 | 0.966 / 1.464 / 4.835 |
| Rational | 1.072 / 3.785 / 3.891 | 0.985 / 1.482 / 5.582 |
| Unique 2×2 | 1.003 / 4.178 / 9.132 | 0.919 / 1.237 / 2.331 |
| Dependent | 0.928 / 1.954 / 2.417 | 0.992 / 6.835 / 7.253 |
| 3×3 | 1.088 / 1.939 / 1.943 | 0.791 / 1.845 / 2.219 |
| Wide | 1.064 / 3.998 / 5.010 | 0.953 / 1.939 / 2.493 |

First preparation of that exact final page, one sample: quadratic 0.664→0.877 ms;
complex 0.853→0.417 ms. This is not cold-boot Solve time. Runs included concurrent
build activity; timings are noisy host regression signals, not ESP32 speedup
claims or LCD first-paint measurements. Slow samples are retained. Repeat,
revisit and guided/summary distributions are also retained in raw JSON/logs.
Across 1,932 matched page events all phase call counters are identical,
polynomial captures stay zero, and valid navigation retains one derivation
build. `GiacTutor.inc` and Solve-time generation remain unchanged.

Firmware SHA-256:
`3197d7b05a0bafe88fdb3fdc3bdebf403840ffe144857bf00f99191cf6084efe`.
`resources.json`, build JSON/logs and probe identities record exact binaries,
commands and source boundaries. This notation image has **not** been flashed.
Previous board timings/heap/stack measurements belong to the earlier optimized
image and are not attributed to this binary.

## Reproduction and limits

Build the pinned native target, then run:

```text
python scripts/test-math-notation.py --source SOURCE --build NATIVE_OBJECT_DIR --out OUTPUT --compiler GXX --phase
python scripts/test-tutor-teaching-ui.py --bin EMULATOR --out OUTPUT --cases notation-factor linear factoring quadratic complex rational system system3 wide-pan
python scripts/test-math-notation-ui.py --before BASELINE_CAPTURES --after OUTPUT
python scripts/test-math-notation-glyphs.py --bin EMULATOR --out GLYPH_OUTPUT
python scripts/generate-discriminant-font.py --converter LV_FONT_CONV
```

The existing page profiler and benchmark scripts reproduce counters and raw
samples; `benchmarks.py`, `host-build.py`, `native-final.py`, `wasm-final.py`,
`allocation-compare.py` and their logs under the ignored evidence directory
record this run's precise orchestration. Failed harness-link attempts (missing
closure/stale test ABI), wrong-profile compile-database attempts and a Windows
Unicode tool-path failure are preserved. They were corrected in test plumbing,
without changing answer expectations or suppressing exits.

Remaining explicit fallbacks include unrecognized scalar identifiers, repeated
symbol factors, symbol-followed-by-group ambiguity and specialized non-scalars.
No additional equation families or generalized type system were added.
Optional physical acceptance: inspect factorization, Δ=41 and √Δ; navigate the
same final pages, BACK/reopen, VAR pan and HOME. Physical notation/response and
new-image internal/PSRAM/stack measurements remain untested in this task.

Exact changed files are listed in `out/math-notation-01/changed-files.txt`;
the unrelated editor change is separately identified there. Suggested future
subject: `feat(math): use natural product notation and discriminant delta`.
