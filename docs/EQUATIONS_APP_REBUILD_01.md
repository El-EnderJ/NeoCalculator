# EQUATIONS-APP-REBUILD-01

Implementation candidate, 2026-09-10. **No commit, push, golden promotion, or hardware flash.**
The native screenshots were opened and visually inspected. Hardware acceptance remains a separate gate.

## Baseline and preservation

Immediate source: `main`, `1a95bb4f0f738fc653352c3fbd280fe08bc3685b`.
`e768f9c`, `a695455`, and `1a95bb4` are ancestors. The index was empty;
the only initial dirty overlay was `.vscode/extensions.json` (2 insertions, 1 deletion), preserved untouched.
No overlapping Equations work was found. No stash, reset, branch switch, or history rewrite was used.
Detached baseline/candidate worktrees were used for comparative pool and ASCII-path WASM builds.

The unchanged application was built and replayed before implementation. Evidence is under
[`out/equations-rebuild/baseline`](../out/equations-rebuild/baseline/): source copies,
native executable, production ELF, build logs, scripted captures, object bounds, and golden comparisons.
The geometry-instrumented baseline adds only an opt-in read-only LVGL bounds walk.

Native configuration: existing `emulator_pc`, C++17, SDL2, deterministic 320×240 RGB565,
LVGL 9.5.0 with its existing CLIB allocator. Firmware keeps the existing 64 KB LVGL pool.
Pinned Giac/KhiCAS 1.4.9+khicas.57, PlatformIO dependencies, keypad map, display clocks,
refresh cadence, brightness, global header and modifier badges are unchanged.

Existing coverage included 19 Equations Giac replays and the host engine/cross-app/tutor suites.
Existing `assert_equations_status`, solution count/exact/near, result-kind and tutor-status hooks remain compatible.

## Reproduced defects versus source concerns

| Evidence | Finding |
|---|---|
| Unchanged list/editor/result captures and bounds | STIX prose produced missing-space boxes; the list footer measured `(8,220)..(459,250)`, outside both screen edges. Other titles/hints competed with math surfaces. |
| Cancel-new replay | Cancelling a new draft retained an empty E1: opening the workflow had already changed the equation set. |
| Physical EXE/FRAC/SQUARE and fraction-UP probes | Canonical keypad events did not behave like passing legacy aliases; structured vertical navigation was ignored. |
| Two/three-row and structured-editor captures | Cramped rows/actions and an unclear editing surface; tall-content clipping from the fixed row height was a source concern, not a separate measured baseline case. |
| Nonlinear host/app reproducer | `x^2=1, y=x` produced the pinned Giac output `list[[1/(2*x),1/(2*x)]]`, previously classified as an all-values family. |
| Development screenshot review | Repeated RIGHT could scroll a result completely away; generic structured division rendered as the function `/(1,sqrt(2))`. Both now have explicit handling. |

Source-level findings, not claimed as separate baseline visual reproductions: normal firmware intercepted BACK before the app while demo/native had local unwind; four result widgets could hide additional groups; borrowed canvas/cursor pointers made replacement ordering important; stale-step caches and fabricated progress text needed removal.
Intentional behavior retained: maximum three equations, one synchronous Giac context, the existing variable/domain policy, and an optional native tutor. No second answer engine was introduced.

## State, focus, and transactional editing

The workflow is **list → Templates/edit draft → list → solve → results → optional answer check**.
List items are committed equations, Add (only below capacity), and Solve (only with input).
EXE selects the focused item. DEL removes only a focused equation and repairs focus to the next
valid item or action. At capacity there is no fourth slot. The selected item is scrolled into view.

An existing equation is cloned into a draft. EXE replaces the committed tree; BACK discards the draft.
A new equation is counted only on confirmation. Cancelling Templates or a new draft creates no row.
Confirming an untouched new blank returns to Add. An explicitly cleared, touched draft is retained as
an incomplete row when confirmed, even when new; navigation alone does not mark a draft touched.
Clearing and confirming an existing equation
retains an explicitly incomplete row: validation identifies that row instead of silently omitting it.
Changing any committed equation clears the result, tutor data, and solve/steps epochs.
Cancelling an existing edit preserves both the original input and its still-current answer.

BACK closes the variable menu or Templates, cancels editing, returns results to the list, and returns
answer check to results. BACK at the list exits. An Equations-only `SystemApp::navigateBack()` dispatch
branch makes this true in normal firmware as well as demo/native. Other apps retain their dispatcher.
HOME uses existing deferred teardown. Re-entry starts a fresh Equations session. No hidden/deleted row
retains focus. No staged Equations timer or callback survives teardown.

## Pixel budget and overflow

| Region | Inclusive bounds / budget |
|---|---|
| Approved shared header | y=0..23, separator y=24; unchanged |
| Local title | x=8..311, y=30..44; 304 px wide |
| Content viewport | x=6..313, y=54..219; 308×166 |
| Footer | x=6..313, y=225..235; Montserrat 10 |
| Equation/template surfaces | 300 px wide; measured math height plus identifier and padding |

Montserrat 12 handles titles, identifiers, notes and labels. STIX/MathCanvas handles formulas.
No global MathAST spacing, fraction geometry, delimiter assembly, font files or MATH constants changed.
The title/footer remain outside the vertically scrolling body. Result variable labels remain outside
their horizontally scrolling canvases. Mathematical sizes are not reduced to fit more rows.

UP/DOWN traverses the remaining portion of a row taller than the body before moving focus.
Whole selected template rows are brought into view. LEFT/RIGHT uses an opt-in bounded MathCanvas
scroll helper, clamped to measured content width; other apps retain their scrolling behavior.
The editor uses the existing horizontal cursor-follow behavior and a small app-level vertical adjustment
from the renderer's cached cursor bounds. Four bounded follow checks accommodate LVGL's draw cadence;
an earlier single check could stop on an old cursor position. No duplicated fraction/root cursor geometry is used.
A tall fraction with balanced nested branches exercises vertical following within the existing render-depth bound.
Beyond the shared depth-28 rendering limit, the existing depth warning remains; this task does not redesign it.

## Templates and ownership

Four intents remain: blank, polynomial `A*x^2+B*x+C=0`, exponential `A*exp(x)+B=0`, and logarithmic
`ln(x)+A=0`. Coefficient symbols use the supported stored-variable names A/B/C; the old lowercase
a/b/c were not supported authored variables. These remain editable templates, not coefficient-entry modes.

`buildTemplateAST()` is the only mathematical constructor. Each preview owns its AST; insertion calls
the same constructor to create independent editable ownership. Prose and preview canvases are separate.
Tests compare tree structure, operators, constants, variables, independent node identities, and explicit
expected inserted serialization. The blank option is first. The working Grapher picker was not refactored.

Every view detaches canvases before releasing borrowed/owned ASTs. Draft cursor detachment precedes
move, reset, cancellation and AC. Result view trees are separate from engine-owned structured values.

## Production keyboard contract

The physical test hook reads the generated electrical matrix map, runs `KeySemanticResolver` in Math
context, and dispatches the resulting event. Host keypad tests separately exercise scanner/resolver contracts.
This is stronger than legacy replay alone, but does not claim an electrical PCB test.

Canonical digits/decimal, arithmetic, FRAC versus DIVIDE, power/SQUARE, roots, parentheses, NEG,
equality, pi/e, functions and shifted functions, variables, DEL/AC/EXE are handled. Equality inserts an
equality; EXE saves/selects/solves. UP/DOWN navigates structured slots. SHIFT+power exposes the root index.
`VAR` opens x/y/z; SHIFT+x also enters y. The physical 3×3 replay uses these accessible routes.
ALPHA A–F retains the shared stored-variable policy; unsupported symbols receive validation errors.

Resolved events do not consume SHIFT/ALPHA twice. One-shot, locked and combined states, navigation
preservation, and HOME cleanup are asserted. Legacy aliases still work. Repeat is accepted for arrows
and editor deletion; it cannot confirm, solve, insert a template, delete a list row, or toggle a modifier.
SHIFT+HOME remains the existing SETUP mapping; release SHIFT to issue HOME.

## Giac, domains, and results

Authored ASTs pass through `CalculationEngine::serializeForGiac`. Exactly one filled equality is required;
missing sides, multiple equalities, empty fraction/root/power slots and unsupported variables are rejected
with an E1/E2/E3 reason. Expressions remain available for correction. No missing side is replaced with zero.

One equation solves for x. Two solve for x,y; three for x,y,z, in that order. The existing shared
`setting_complex_enabled` supplies the real/complex policy. No app-local setting or variable inference was added.
One engine-owned context, structured exact results, angle scoping and existing resource limits remain.
The static “Solving with Giac...” screen is painted before a blocking call; no animation/cancellation is promised.

Finite results preserve solution groups and assignment order. Up to four single-variable roots appear in
one scrollable view; larger sets and systems use complete group pages, advanced with VAR. Five distinct
exact roots are tested past the old widget cap. Every engine-returned group remains accessible.
Decimal-valued candidates explicitly say completeness is not established. Their printed values are not
reconstructed into exact roots. No real roots, identity, conditional identity, dependent family, unsupported
output, invalid input and resource/evaluation failure have different presentations.

An app-local presentation helper maps Giac's structured binary division directly to a VPAM fraction,
including its exact numerator and denominator, without evaluating or parsing printed text.
Unrenderable nested structures use grouped exact text instead of misleading operator-as-function display. Text beyond 3072 bytes
ends with an explicit **remaining text is not shown** notice; this is a display limit, not an engine truncation
or a claim that the full result was displayed. Horizontal formula and vertical result access are bounded.

Identity handling checks the original authored tree for possible domain-bearing operations. Thus `x/x=1`
is explicitly conditional with unresolved exclusions, not an assertion including zero. Polynomial identity
without such operations is all real/complex x. Dependent systems retain the relation and tuple order,
not independent pools of variable values. Conservative conditional/unsupported output is intentional;
there is no new symbolic domain solver.

### Narrow shared-engine correction

Before calling an output containing solve variables a family, Giac now substitutes that proposed tuple
into the original equation objects and checks equality with Giac. Undefined/unproved results are rejected
as unsupported. The nonlinear reproducer above now refuses an incorrect family. The valid dependent
linear case still passes. Scope restores angle mode and quoted-variable state; it creates no temporary
user bindings. Finite-result adaptation and accepted Calculus command handlers are unchanged.
Host tests cover this reproducer and the denominator exclusion `(x^2-1)/(x-1)=0`.

## Tutor contract

OmniSolver/SystemSolver executable calls generate optional tutor candidates only; their old “primary
engine” comments were stale. Exceptions or disagreement cannot replace/suppress Giac's result.
Candidate generation occurs once per solve, not per frame or each opening of answer check.
Equation, solve and steps epochs prevent reuse after committed edits/deletion. Cancelling a draft does
not invalidate its unchanged source equations.

Final-answer agreement does **not** verify intermediate transformations. The view says so explicitly
and shows only authored input and the agreement note. Unsupported cases show “Steps unavailable”.
Unverified snapshots are omitted; no native CAS coverage was expanded. This is an answer-check view,
not a claim of verified step-by-step derivation.

## Tests and visual evidence

Run `pio run -e emulator_pc`, then `python scripts/test-equations-rebuild.py`.
The runner creates output directories, scripts, logs, 320×240 captures and an optional Pillow contact sheet.
It requires the script-completion marker as well as exit status. Unknown assertions fail closed.
Negative controls for root 99, a stale epoch and a mismatched template must each exit 4.
Two standalone replays cover draft preservation and canonical physical 3×3 entry.

The focused corpus covers all requested single equations, exact radical/cubic roots, repeated roots,
pi/e, real/complex policy, authored denominator exclusions, identities, contradiction, linear systems,
rational/reordered input, dependent and inconsistent systems, and honest nonlinear refusal. Known
answers use exact Giac equivalence; returned groups are substituted into original authored sides.
It also covers incomplete system rows, templates, cancel/commit/delete, capacity/focus, modifiers/repeat,
scrolling/pagination, error recovery, stale state, early modal close, HOME and re-entry.

Actual reviewed artifacts:
[baseline contact](../out/equations-rebuild/baseline/contact.png),
[final contact](../out/equations-rebuild/final/contact.png),
[before/after](../out/equations-rebuild/final/before-after.png).
Screenshots include empty/one/two/three lists, all selected templates, blank/structured/incomplete/tall editors,
single/radical/cubic/complex roots, 2×2/3×3 tuples, dependent/inconsistent output, errors, wide/tall results,
exact/numerical text fallback and available/unavailable answer check. Bounds are recorded after LVGL layout.
Assertions require fixed title/footer/body inside 320×240 and selected fitting rows fully inside their viewport;
intentional scrolling content is allowed beyond the viewport.

## Lifecycle and memory evidence

Fifty deterministic mixed cycles include systems, early Templates cancellation, solve/results/answer check,
edit/cancel, deletion, HOME and reopen. After ten warm-up cycles, live objects/timers/handles return to
stable values. Native CLIB reports zero for unavailable pool/heap probes: **not a measured zero-byte heap**.

Identical fixed-pool host builds provide the following supplemental measurements (64-bit host, not PCB):

| 96 KB pool phase | Baseline objects/free bytes | Candidate objects/free bytes |
|---|---|---|
| System results | 55 / 29192–29216 | 20 / 44008 |
| Answer check | 57 / 28360–28384 | 15 / 46096 |
| Post-edit/delete | 57 / 28344–28368 | 21 / 43376 |
| Menu after teardown | 67 / 51664 | 67 / 51664 |
| Minimum sampled free | 28336 | 43296 |

All phases: three timers, zero retained engine handles. Candidate values stabilize after warm-up.
At 64 KB the baseline aborts before the first probe; candidate completes 200 probes/50 cycles,
minimum sampled free 10792, stable menu free 19160. No production pool enlargement was made.
The complete 91-case focused runner also passes with the candidate's fixed 64 KB host pool.
These samples are not allocator-wide peak measurements. Target heap fragmentation, task high-water
mark, actual PSRAM/cache timing and allocation-failure injection are **NOT RUN** without hardware.

Production-normal immediate baseline → candidate:

| Metric | Baseline | Candidate | Delta |
|---|---:|---:|---:|
| Linked flash bytes | 5464153 | 5403605 | -60548 |
| Static RAM bytes | 118976 | 118960 | -16 |
| Firmware binary bytes | 5464512 | 5403968 | -60544 |
| `.iram0.text` bytes | 60407 | 60407 | 0 |

Xtensa disassembly gives own stack frames: update 48, formula 64, editor handler 64, main key handler 80,
showResult 304, solveEquations 592, tutor candidate 1792 bytes. The solve→tutor chain therefore already
accounts for 2384 bytes before its callees; this is **not** a measured full task stack peak. Existing AST
traversals remain recursive. New traversal helpers are depth-bounded; no allocation was added to core
MathAST layout/draw/cursor calculations. UI construction and Giac/tutor work allocate outside those paths.

## Regression matrix and limitations

- Focused Equations: 91/91 checks, including three intentional failures and both lifecycle stability checks.
- Host Giac: 179/179; Calculus/F7: 56/56; Neo: 44/44; cross-app: 14/14. Native CAS/tutor: 8/8 suites.
- Focused Calculus rebuild, authentic STIX parentheses and Grapher preview/insertion suites: PASS.
- Existing Calculation/Calculus/Equations/Grapher replays, plus both new replays: PASS (133 regression entries including the three focused suites); `grapher_negctl_assert_kind` exits 4 as intended.
- Production keypad: PASS (50 mappings); display profile, production demo/contract and digit-pattern checks: PASS.
- Native emulator: PASS. Production normal/bring-up/demo and CAM normal/validation build logs are in `final/`.
- Web Release build/package and browser smoke: PASS, including real Equations entry/solve and launcher re-entry.
- WASM-MATH Release and Debug: PASS in Chromium, Firefox and WebKit. Pinned Playwright 1.54.1 was installed unchanged.

The WASM build initially hit Windows Unicode-path tool failures; an isolated ASCII-path candidate worktree
resolved them. No toolchain was upgraded. An early CAM compilation caught an Arduino `radians` macro
collision in a local guard name; it was renamed and rebuilt. The host replot RSS guard fluctuated:
one candidate run reported 4560 KB growth and 178/179, after a preliminary unchanged-engine run also
exceeded its threshold. The failure log is retained as `giac-host-rss-failure.log`; three subsequent
candidate runs and the final run from the harness output directory passed 179/179. No threshold changed.
These were not hidden or changed into passing expectations.

Before and after: all 42 candidate captures generated. Existing golden comparisons give 18 differences
and 24 missing goldens on both runs. All 42 unrelated screens are pixel-identical below y=25 between
same-platform source baselines; remaining differences are the existing wall-clock header. Approved header,
Grapher-template and previously documented host graph/trace golden differences were not promoted away.
No goldens or masks changed. The focused runner is appended to existing emulator CI; broader CI redesign
and remote CI execution are **NOT RUN** in this local task.

Remaining product limits: three fixed solve variables by row count; synchronous uncancellable solve;
conservative domain/family refusals; no verified intermediate tutor formulas; explicit text display cap;
shared renderer depth limit. No arbitrary new solve mode, native CAS answer fallback, or symbolic domain solver.

## Physical acceptance checklist — NOT RUN

After separate authorization, build/observe serial automatically and ask one concrete LCD/key question
at a time, waiting for each answer. Do not infer these results from emulator images.

1. Open Equations; check header, modifier badges, Add focus and footer on the 320×240 LCD.
2. Enter `2*x+4=0` with physical equality; EXE saves, select Solve, verify x=-2.
3. Edit to `x^2-2=0`; verify both exact roots and scrolling. Cancel another edit; verify preservation.
4. Enter `x+y=3`, `x-y=1`; verify one tuple x=2,y=1.
5. Enter `x+y+z=6`, `x-y+z=2`, `x+y-z=0`, using SHIFT+x and VAR; verify (1,2,3).
6. Inspect tall Templates/editor content, fraction UP/DOWN, root index, wide/tall results and fixed labels.
7. Exercise SHIFT/ALPHA once/locked/combined, repeat, local BACK, HOME during open modal/edit/results, then re-entry.

## Changed files and review command

The index remains untouched. `.vscode/extensions.json` is the user's pre-existing overlay and is excluded.
Generated evidence and temporary build worktrees are not part of the source commit.

```sh
git add -- .github/workflows/emulator-build.yml src/SystemApp.cpp src/apps/EquationsApp.h src/apps/EquationsApp.cpp src/hal/NativeHal.cpp src/math/CursorController.h src/math/giac/GiacEngine.cpp src/ui/MathRenderer.h tests/host/giac_engine_suite_main.cpp scripts/test-equations-rebuild.py tests/emulator/scripts/equations_rebuild_transaction.numos tests/emulator/scripts/equations_rebuild_physical.numos docs/EQUATIONS_APP_REBUILD_01.md
```

Suggested subject: `feat(equations): rebuild equation editing and solution workflows`
No commit has been created.
