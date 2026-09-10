# CALCULUS-APP-REBUILD-01

Implementation and review evidence against baseline commit `8dd8528`.
This report is staged progressively: the Calculus rebuild first, then authentic STIX typography, then Grapher previews. Generated evidence stays under ignored `out/`; accepted goldens and masks are not promoted or edited.

**Calculus disposition:** focused UI/keypad/F7 and 50-cycle checks pass. Production PCB boot, layout, derivative/integral results, editor controls, modifier indicators and HOME/re-entry were confirmed by the user on the combined candidate. Historical software measurements below identify their evidence directories; isolated-commit and final-HEAD results are recorded in the completion report. No push is part of this task.

## 1. Baseline architecture

- `CalculusApp` is launcher app ID 3, lazily created/loaded and torn down through the existing SystemApp/NativeHal deferred lifecycle. Production uses the existing 200 ms screen transition and 250 ms teardown boundary.
- The authored expression is a real VPAM `NodeRow`, edited by `CursorController` and drawn by `MathCanvas`. `CalculationEngine::serializeForGiac` is the controlled serialization boundary.
- `GiacEngine::evaluateCalculusStructured` owns the shared Giac context, call guard, angle synchronization, typed status, result tree and diagnostics. `CalculationEngine::resultTreeToAST` converts results for NumOS rendering.
- Only first derivatives and indefinite integrals are live. There are no intentional definite-integration bounds or derivative-order controls. The existing variable policy selects the first authored `x`/`y`, defaulting to `x`.
- `ASTFlattener`, `SymDiff`/`SymIntegrate`, simplification and the step arena form the native tutor lane. Giac verification gates those steps; their answers never enter the primary result path.
- GRAPH really toggled modes; it was not a graph-launch action. UP/DOWN also toggled modes while editing, interfering with structured navigation.
- Production Revision-C events include distinct FRAC, DIVIDE, SQUARE and semantic-only ALPHA/inverse-trig events; the old Calculus handler did not consume these correctly. CAM and production key maps have not been changed.
- No F7 gauntlet document was found in the checked repository documentation. The supplied six-expression corpus was reproduced explicitly.

## 2. Measured baseline defects

The seven [baseline screenshots](../out/calculus-rebuild/baseline/contact.png) were captured before the redesign. An opt-in LVGL geometry trace then measured actual object coordinates, after layout, in the unchanged layout. [Trace](../out/calculus-rebuild/baseline/boxes.log).

| Region | Actual inclusive bounds | Defect |
|---|---|---|
| Status/header | `(0,0)..(319,23)`; separator y=24 | Accepted and retained |
| Derivative tab | `(0,25)..(159,50)` | Its label is `(-3,23)..(163,53)`: clipped left, outside tab vertically, overlaps adjacent tab |
| Integral tab label | `(173,23)..(306,53)` | Exceeds tab vertically |
| Input title | `(6,55)..(127,85)` | 31-pixel text box overlaps editor starting at y=75 |
| Initial editor | `(10,75)..(313,213)` | Unmarked white space; function label detached from input |
| Input footer | `(6,222)..(468,252)` | 149 pixels past right edge, 13 pixels past bottom |
| Result title | `(6,55)..(221,85)` | Overlaps original-expression label/preview |
| Original label/preview | label `(6,75)..(62,105)`; canvas `(56,71)..(315,107)` | Label and preview overlap; fixed small preview cannot accommodate taller input |
| Result label/canvas | label `(6,109)..(66,139)`; canvas `(10,129)..(313,208)` | Label intrudes into result canvas |
| Result footer | `(6,218)..(304,248)` | Nine pixels past bottom |

STIX was also being used for prose labels, producing visible missing-space boxes. Redundant symbol/word tab labels, inconsistent baselines and absent focus hierarchy made the problems worse.

## 3. UX redesign

Two equal-width word tabs, **Derivative** and **Integral**, replace the overfull selector. The selected tab has a three-pixel underline and a filled background; the inactive tab has a one-pixel rule. A dark outline identifies selector focus independently of color.

A restrained bordered input surface groups `f(x)`, the natural editor, its visible cursor and an empty-only placeholder. A result surface groups the authored expression, a separator, a compact mathematical result heading, and a bounded result viewport. Short action hints live in a separate footer. Existing Montserrat UI fonts handle prose; accepted STIX math fonts and renderer/AST geometry are unchanged.

## 4. Final pixel budget and overflow policy

Exactly **320 × 240**; all coordinates below are inclusive.

| Vertical allocation | Pixels | Range |
|---|---:|---|
| Existing system header | 24 | y=0..23 |
| Existing separator | 1 | y=24 |
| Mode region, including padding | 33 | y=25..57 |
| Content region, including padding | 164 | y=58..221 |
| Footer, including padding | 18 | y=222..239 |
| **Total** | **240** | |

Measured final derivative-result bounds:

| Object | Bounds |
|---|---|
| Tab strip | `(6,28)..(313,54)` |
| Tabs | x=6..157 and x=162..313; 152 px each, 4 px gap |
| Tab text | both y=32..47 |
| Input/result surface | `(6,62)..(313,215)` |
| Input label | `(15,68)..(33,82)` |
| Input viewport/canvas | `(9,87)..(310,210)`; 124 px tall |
| Original-expression viewport | `(49,65)..(310,112)` for ordinary input |
| Result heading | `(15,119)..(103,133)` for derivative |
| Result separator | `(15,114)..(304,114)` |
| Result viewport | `(9,139)..(310,212)` for ordinary input |
| Footer text | y=224..234; longest configured hint remains inside x=6..313 |

The original-expression preview grows from 48 to at most 72 px according to measured math height; the result viewport gives up the same amount of height. Results scroll vertically with UP/DOWN and horizontally with LEFT/RIGHT. Input uses MathCanvas's existing horizontal cursor following. Input taller than its fixed 124-pixel viewport and preview content taller than 72 pixels are deliberately clipped inside their own parents. Neither can overwrite a heading, selector or footer. Steps scroll inside the content region. No negative layout offsets conceal overflow.

The test runner asserts actual child/parent geometry, permitting oversized children only in deliberate scroll containers. These assertions supplement, rather than replace, visual inspection.

## 5. Derivative workflow

Open → enter expression → EXE → exact structured Giac derivative. EXE on a result returns to editing; DEL returns to editing and deletes; AC clears. VAR inserts the app's default `x`, SQUARE inserts a real power template, and ordinary parentheses/fractions/roots/powers use the shared controller.

The host oracle checks `x^2`, `sin(x)`, `cos(x)`, `ln(x)`, `exp(x)`, `x*sin(x)`, `sqrt(x)` and `(x^2+1)/(x+1)`, plus existing composition/parameter/constant cases. The live tall-input test differentiates `sqrt(x^2+1)/(x+1)` and checks equivalence to `(x-1)/((x+1)^2*sqrt(x^2+1))`.

The quotient test exposed the analogous raw-parser derivative limitation. The derivative boundary now calls Giac's registered `_derive` handler with typed arguments, allowing Giac to evaluate quotient syntax before differentiation. The edge case `diff(x*infinity,x)` now follows the normal command's `0`, rather than the old kernel's `+infinity`; its regression asserts canonical command parity.

## 6. Integral workflow

Choose Integral via the selector or physical SHIFT then 2 → enter expression → EXE. Successful structured primitives receive the existing NumOS `+ C` presentation. Valid unevaluated integrals retain their structured mathematical form and are explicitly labeled; they do not gain a misleading `+ C` or native answer.

All requested polynomial, trig and radical examples are covered. `sqrt(x^2+1)` retains Giac's logarithmic branch expression rather than being rewritten to a preferred `asinh` form. No bounds UI or new definite-integration feature was added.

## 7. F7 root cause

In the vendored `lib/giac/src/kintg.cc`, `integrate_gen` delegates to `integrate_id`, which enters the lower-level primitive machinery without the normal command's evaluation stage. The authored serializer supplies parsed, not pre-evaluated, Giac syntax. In particular, Giac's own evaluation of sqrt syntax reaches its power/radical representation (`kusual.cc`'s `sqrt` implementation); skipping that evaluation loses ordinary radical integration capability.

The registered `integrate` function points to `_integrate`. That handler evaluates its arguments with Giac's variable/context policy, handles exactness and runs the normal `integrate0` pipeline. This is also why harmless result shapes change, such as `1/3*x^3` becoming `x^3/3`.

Before the fix, all six supplied F7 cases failed explicit command-parity assertions. [Before log](../out/calculus-rebuild/f7-before.log).

## 8. F7 correction and context safety

`computeCalculusGen` now invokes `giac::_integrate(giac::makesequence(authored, variable), ctx)` inside the existing guard and scoped angle handling. It reuses the established structured-result seam without recursively calling the public engine or building app-authored command strings.

The pinned command handler ignores an undefined value from integrand evaluation and can crash when it subsequently attempts to integrate raw `0/0`. A preflight uses **Giac's evaluator** to reject undefined input. Its scoped quoted-variable guard restores context on every exit and ends **before** invoking the integration command, so it cannot change that command's stored-variable semantics. Tests bind `x:=7`, compare with the normal integrate command and verify that undefined input does not alter the binding.

No sqrt rewrite, integration table, custom fallback or native answer acceptance was introduced. Exactness, diagnostics, structured conversion, non-reentrancy, degree-to-radian symbolic handling, watchdog integration and synchronous interruption/lifecycle assumptions remain at the original engine seam.

## 9. Tutor lane

Giac runs first. A closed-form Giac answer may be followed by native tutor work. The candidate must pass Giac equivalence verification; mismatch, unsupported native input, unevaluated Giac output or an exception leaves steps unavailable. The exception path performs no new diagnostic-string allocation, preserving the primary answer during tutor OOM.

Step rendering now respects the existing logger's `StepKind`: text annotations no longer render default `0 = 0` snapshots. Only authored input and the Giac result are displayed as formulas; unverified intermediate native expressions/highlights are omitted. This also avoids ambiguous adjacent numeric factors in unsimplified snapshots. Verified native rule notes remain available. No integration coverage was added. Step renderers are destroyed before their LVGL parent; input/result/original canvases detach their ASTs when replaced or destroyed.

## 10. Keyboard and lifecycle behavior

- Initial/re-entered mode is Derivative, with editor focus.
- UP from the root editor row focuses the selector; UP/DOWN inside structures use natural VPAM navigation.
- LEFT/RIGHT in selector focus choose the mode; DOWN/EXE enters editing. LEFT/RIGHT in the editor move the math cursor.
- On the production PCB, SHIFT then 1 selects Derivative and SHIFT then 2 selects Integral. These resolve to F1/F2. GRAPH remains a legacy emulator/serial toggle; there is no physical GRAPH key. Mode change preserves input and invalidates the old answer.
- Mode changes and EXE/AC transitions ignore REPEAT events. Explicit repeat-injection tests ensure a held direction/GRAPH cannot repeatedly flip modes.
- EXE calculates in the editor, edits from the result, and returns from steps. AC clears input/results; DEL edits/deletes.
- TOOLBOX/SHOW_STEPS opens only verified steps. Result arrows scroll, without selecting another mode accidentally.
- Production FRAC, DIVIDE, SQUARE, VAR and resolved ALPHA/inverse-trig/e-power actions reuse CursorController. SHIFT/ALPHA one-shot consumption and locked-state behavior retain KeyboardManager ownership.
- HOME retains global forced release/modifier reset and deferred teardown. Normal production BACK follows its existing launcher-return contract; emulator/demo BACK unwinds local states first. Footer copy therefore uses EXE for editing rather than promising local BACK behavior on every build.

## 11. Physical hints and modifier visibility

The editor footer is `EXE Calculate    VAR x    UP Mode`. Result hints offer EXE edit, AC clear, and either verified steps or `SHIFT 1/2 Mode`; oversized results show arrow/scroll hints. All use 10-pixel Montserrat and fit the footer. The physical acceptance session exposed the misleading legacy GRAPH hint, which is now corrected.

The same session found that an active SHIFT state made launcher EXE appear inert. The production resolver consumes modifier presses before app dispatch, while the launcher previously had no modifier label. The existing input/UI loop now refreshes the visible launcher/app badge from KeyboardManager and only updates LVGL when text changes. Active-bar cleanup prevents stale pointers; no extra timer or keyboard mapping is introduced.

Final user-approved layout: S/A and the established S-LOCK/A-LOCK/S+A-LOCK notation on the left; titles stay centered; RAD and battery remain visible on the right, with the app clock beside RAD. The emulator checks all modifier phase combinations on Menu, Calculation, Calculus and Grapher, asserts visible non-overlapping header geometry, and compares fixed title/clock/angle/battery pixels across transitions. This deliberately changes header placement from the old golden images; it does not justify editing masks.

## 12. Historical screenshot review

1. **Untouched baseline:** seven states captured and inspected; real LVGL boxes confirmed tab/label/footer overflow and overlaps.
2. **Iteration 1:** equal tabs, input/result surfaces, compact UI fonts and footer. Inspection found empty-input focus still weak and preview height too rigid.
3. **Iteration 2:** placeholder, fixed input viewport, adaptive preview and scrollable results. Inspection found a one-pixel tab-baseline discrepancy and an unevaluated derivative mislabeled as an integral; the quotient case also exposed the derivative entry mismatch.
4. **Iteration 3:** aligned baselines, correct operation/error identity and canonical quotient derivatives. Physical event handling and focus/navigation were exercised.
5. **Final review:** removed spurious tutor annotation equations and ambiguous unverified intermediate formulas, verified all final candidates, and kept result scrolling explicit for structures larger than the viewport.

[Baseline](../out/calculus-rebuild/baseline/contact.png) · [Iteration 1](../out/calculus-rebuild/iteration-1/contact.png) · [Iteration 2](../out/calculus-rebuild/iteration-2/contact.png) · [Final contact sheet](../out/calculus-rebuild/final/contact.png).

## 13. Initial rebuild screenshot inventory

Each individual PPM/PNG is exactly **320×240**. Contact sheets are larger review aids, not framebuffer goldens.

| Candidate | Evidence |
|---|---|
| Derivative empty | [PNG](../out/calculus-rebuild/final/derivative-empty.png) |
| x² entered / editor cursor | [PNG](../out/calculus-rebuild/final/derivative-x2-input.png) |
| Derivative result | [PNG](../out/calculus-rebuild/final/derivative-result.png) |
| Tall structured input | [PNG](../out/calculus-rebuild/final/derivative-tall-input.png) |
| Tall structured derivative / scroll | [PNG](../out/calculus-rebuild/final/derivative-tall-result.png) |
| Syntax error | [PNG](../out/calculus-rebuild/final/derivative-syntax-error.png) |
| Selector focus | [PNG](../out/calculus-rebuild/final/mode-focus.png) |
| Editor focus after mode change | [PNG](../out/calculus-rebuild/final/editor-focus.png) |
| Integral empty | [PNG](../out/calculus-rebuild/final/integral-empty.png) |
| Integral x² result | [PNG](../out/calculus-rebuild/final/integral-x2-result.png) |
| Integral sqrt(x) result | [PNG](../out/calculus-rebuild/final/integral-sqrt-result.png) |
| Integral sqrt(x²+1) result | [PNG](../out/calculus-rebuild/final/integral-tall-result.png) |
| Valid unevaluated integral | [PNG](../out/calculus-rebuild/final/integral-unevaluated.png) |
| Undefined/domain error | [PNG](../out/calculus-rebuild/final/integral-domain-error.png) |
| Long input / cursor following | [PNG](../out/calculus-rebuild/final/long-input.png) |
| Verified steps | [PNG](../out/calculus-rebuild/final/verified-steps.png) |

Bounds screenshots are inapplicable: definite integration was not live. No existing Calculus golden was present; no non-Calculus golden or mask changed. All new files under `out/` remain candidates for human review.

## 14. Initial software math regressions

The focused host suite passes **56 assertions**, including six explicit before/after F7 failures, command parity, exact derivative-of-primitive checks, parameter/context behavior, malformed/undefined inputs, degree/radian handling and tutor agreement/disagreement. The new live keypad corpus covers all requested F7 radicals plus polynomial/trig cases, using a Giac equivalence oracle rather than display-string equality.

Giac engine: **177 pass**. Neo math backend: **44 pass**. Cross-app Giac suite: **14 pass**. Native CAS/tutor suite: **8 suites pass**, no unexpected failures. [Host log](../out/calculus-rebuild/host-final.log) · [CAS log](../out/calculus-rebuild/cas-host.log).

WASM-MATH Release and Debug pass in Chromium, Firefox and WebKit, including the new F7 command-parity and primitive-derivative assertions. The oracle uses normal Giac `simplify(...)` evaluation, like the host harness.

## 15. Historical lifecycle, memory and responsiveness

Two complete 50-cycle campaigns ran: normal emulator allocator and a scratch emulator using LVGL's fixed **64 KB BUILTIN** pool. Each cycle opens Calculus, differentiates, opens verified steps, integrates, returns HOME, verifies deferred destruction and re-enters. Normal firmware allocator configuration was not changed.

After warm-up:

- Calculus: **44 objects**, **3 timers**, zero retained Giac handles.
- Menu: **66 objects**, **3 timers**, zero retained handles.
- System-allocator live heap: Menu **2,706,064 B**, Calculus **2,722,640 B**, with no growth in the recorded final campaign.
- Fixed-pool live process heap: Menu **2,668,256 B**, Calculus **2,670,352 B**, unchanged across steady cycles.
- Fixed-pool free bytes: Menu **19,800 exactly** after every teardown; Calculus **3,760–3,768**, a repeating small allocation/alignment variation with no drift.

The 64-bit host pool has substantially larger object metadata than firmware. Its successful bounded soak is evidence of ownership/lifecycle stability, not a measurement of free RAM on BOARD A. [Normal summary](../out/calculus-rebuild/final/lifecycle-summary.json) · [Fixed-pool summary](../out/calculus-rebuild/fixed-pool/lifecycle-summary.json).

Opt-in native timers measure first open, re-entry, mode changes, editor keys, separate derivative/integral calls and teardown. The [timing report](../out/calculus-rebuild/performance/summary.json) includes matched baseline/rebuilt x² campaigns. Typical rebuilt warm operations are below one millisecond; these host timings are not presented as physical SPI/keypad latency. Existing 200 ms fades/250 ms deferred teardown remain intentional. There are no per-frame Giac calls, spinner timers or full UI-tree rebuilds on ordinary keypresses.

| Host operation | Baseline median (µs) | Rebuilt median (µs) | Rebuilt first (µs) |
|---|---:|---:|---:|
| open | 354.0 | 208.0 | 211 |
| mode | 11 | 39.0 | 40 |
| derivative | 135.0 | 101.5 | 164 |
| integral | 192.0 | 165.0 | 197 |
| teardown | 20.0 | 19.0 | 21 |

VAR_X editor-event timing (nonzero press samples): median 6.5 µs. Open samples include 40 launches/re-entries; derivative and integral each have 20 x² samples. Timings cover dispatch/compute readiness, not physical first-paint latency.

## 16. Firmware footprint and reproducibility

The matched baseline `8dd8528` production-normal build used 118,976 bytes static RAM and 5,444,433 bytes linked flash. Exact per-commit and final sizes belong in the completion report; earlier candidate sizes are not reused as final measurements. Builds use the pinned PlatformIO/toolchain setup and production-normal environment `numos-esp32-s3-wroom-1u-n16r8`. The production display remains 40 MHz write / 10 MHz read, with 16 ms LVGL cadence and the existing production LVGL-only -O2 setting.

## 17. Validation method

Run `python3 scripts/test-calculus-rebuild.py --bin <emulator> --out <evidence> --cycles 50`, `scripts/build-giac-host-harness.sh`, the Calculus replays, emulator_pc, and production normal. The focused runner covers 16 states, keyboard/focus/repeat handling, F7 radicals, modifier badge visibility and lifecycle stability. Native CAS/tutor and WASM tests cover the shared math boundary. Each conceptual commit must also build and pass its focused checks in its own clean detached worktree, without later working-tree changes.

Historical full software runs include 33 Calculation, 16 Calculus, 19 Equations and 60 Grapher replays (one intentionally failing negative control), eight native CAS/tutor suites, and WASM Release/Debug browser coverage. Those runs do not substitute for the final-HEAD run. The auxiliary PowerShell production-demo packaging check cannot run on this macOS environment; its C++ host tests use the Darwin linker spelling.

## 18. Limits

- Giac remains synchronous; HOME/BACK is serviced after a running call.
- Only first derivatives and indefinite integrals are supported. Existing x/y variable selection is retained; no new bounds/order UI.
- Native tutor coverage can be unavailable despite a correct primary Giac result.
- Very tall editor content uses the existing bounded viewport; large results scroll.
- The separate pre-existing raw `transformStructured(Simplify)` residual issue can return an opaque `poly1[...]` for a radical primitive; normal Giac command parity passes. This unrelated algebra seam is unchanged.
- The accepted Grapher graph/trace baselines contain seven known body-pixel differences at `(91,216), (90,217), (91,217), (89,218), (90,218), (88,219), (89,219)`. Final comparisons must separate these from explicitly approved header changes and must not alter masks to turn differences green.

## 19. Physical and automated acceptance

User-observed on the production ESP32-S3-WROOM-1U-N16R8 PCB: normal launcher/display/keypad, Calculus layout/cursor/footer, x² and tall quotient derivatives, sqrt(x) F7 integration and the logarithmic sqrt(x²+1) primitive, FRAC/SQRT/SQUARE/parentheses/VAR/DEL/AC, selector focus, SHIFT+1/2 mode selection, visible modifiers, and HOME/re-entry. The final left-label header and corrected UP Mode hint were accepted.

The user subsequently requested that the remaining checks be automated. Emulator screenshots and geometry checks verify rendering; AST/numeric assertions verify semantics. The existing PCB serial input bridge drives workflows while normal telemetry records heap, PSRAM, LVGL usage and teardown. These tools reduce repetitive manual checks, but emulator pixels are not an independent capture of the physical LCD, and bridge events do not exercise switch contacts. Label results as BUILD-CONFIRMED, EMULATOR-VERIFIED, SERIAL-OBSERVED/INPUT-BRIDGE EXERCISED, or PHYSICAL EVIDENCE — USER OBSERVED as appropriate.

Only the normal app partition at 0x10000 is flashed after a full pre-task flash backup and partition comparison. No filesystem formatting, eFuse/security changes, CAM provisioning, experimental profile or 80 MHz display setting is used.

## 20. Calculus commit ownership

- CalculusApp.cpp/.h: UI, focus, physical semantics, lifecycle, debug oracles and accurate hints.
- GiacEngine.cpp: canonical typed commands and scoped undefined-input guard.
- SystemApp.cpp, MainMenu.cpp/.h, StatusBar.cpp/.h, ModifierBadge.h: live modifier visibility and the fixed, user-approved header layout.
- NativeHal.cpp: Calculus/key-repeat/semantic/modifier/probe assertions and matching UI refresh. Later template assertions are excluded from this commit.
- platformio.ini: existing KeySemanticResolver.cpp included in emulator sources; later supplemental font lines are excluded.
- Focused host/WASM calculus tests, two calculus replays, and scripts/test-calculus-rebuild.py.

## 21. Staging and isolation

Stage only exact owned paths/hunks. Build intermediate NativeHal and report blobs without replacing the complete working files. Before each commit inspect cached check/stat/name-status and the semantic diff. Validate each exact commit in a clean detached worktree, then remove that temporary worktree. Do not stash, reset, clean, switch the main branch, stage ignored evidence, or create an unrelated cleanup commit. Preserve unrelated changes separately according to the user's decision.

## 22. Commit

`feat(calculus): rebuild calculus app for 320x240`

Rework the Derivative/Integral workflow for the production display, restore physical-keypad semantics, use canonical Giac calculus commands, and add focused lifecycle and F7 regression coverage.

## Authentic STIX stretchy parentheses

Parentheses use source STIX Two Math outlines: the smallest fitting MATH size variant, or authentic top/extender/bottom pieces beyond the largest variant. The old synthetic parenthesis curve path is removed. Other delimiter kinds and the existing global stix_math fonts are unchanged.

The old extractor omitted unencoded MATH variants. `scripts/generate-stix-parentheses.py` extracts 13 variants and three assembly pieces per side into supplemental 18/12/8 px fonts. FontTools and lv_font_conv 1.5.3 reproduce the generated files byte for byte. Shared generated raster metrics drive both layout and drawing: child ink plus clearance chooses size; actual glyph width determines spacing; straight extenders are clipped between complete caps. Rendering allocates no per-frame bitmap.

Ownership: MathAST parenthesis geometry and call sites; MathRenderer glyph/assembly drawing; MathTypography accessors; StixMathFont declarations; StixParentheses/ink data; three supplemental font sources and their emulator inclusion; generator; MathRenderVisualCases; geometry and raster tests. No Grapher preview behavior is part of this typography change.

Validation includes 16 visual cases and 21 delimiter pairs, 660 target sizes across three nominal fonts, catalog bounds, continuous stroke coverage, nested alignment and authentic assembly cases. Run `python3 scripts/test-stretchy-parens.py --bin <emulator> --out <evidence>` plus MathEnginePhaseRegression and relevant Calculation/Calculus tests. The real PCB's ordinary, fractional, nested and tall Calculus parentheses were accepted by the user: natural shapes, aligned pairs, no clipping, gaps, malformed caps or one-sided stretching.

Commit: `feat(math): use authentic STIX stretchy parentheses`.
