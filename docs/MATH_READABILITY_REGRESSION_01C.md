# MATH-READABILITY-REGRESSION-01C

Restores the display-limits gate (TXP §H.9.4) and re-applies the three READABILITY-02 fixes that do not depend on the 14 px profile. Start state: the 01B end state on HEAD `39d7d1e` (all per-file hashes in `deliberate-edits.json` and `restoration-result.json` matched; baseline patch SHA-256 `43128662…4496`). Nothing is committed, flashed or deployed. Hardware: NOT EXECUTED.

Evidence: `out/math-readability-regression-01c/` (`baseline/`, `captures/`, `cursor/`, `adv-h01/`, `goldens/`, `build/`, `tools/`, `files.json`); raw verification logs are under `C:/.codex-cache/mrr01c/verify/`.

## 1. Display-limits gate

Rule (TXP `docs/specs/NUMOS_TEX_PARITY_TYPESETTING_SPEC.md` §H.9.4): stacked limits iff `style == DISPLAY && glyph height >= displayOperatorMinHeight()`. One rule, one call site, for `NodeSummation`, `NodeDefIntegral` and `NodeBigOp` (GAP-37: all three go through `largeOperatorGeometry`).

### What the gate reads

At em 18 the minimum is 32 px. PARITY-02 draws the source font's display variant (Σ 25 px, ∏ 25 px, ⋂/⋃/⋀/⋁ 28 px, ∫ 43 px `uni222B.dsp`). Gating on that drawn height would leave ∫ stacked and send everything else to the side, the opposite of the accepted PCB behaviour. The gate therefore reads the variant-ladder height for `largeOperatorTargetHeightPx()` (`DelimiterAssembler::glyphMetricsForHeightPx`), exactly the input the pre-PARITY-02 gate used and the input `testDisplayLimitsHonorsOperatorMinHeightThreshold` models. Glyph selection stays PARITY-02's (display variant in display style). Consequence at em 18/12/8: every large operator takes side limits in every style, as on the accepted PCB. Σ/∏ stack again when a ladder variant reaches the minimum (TXP-1); the stacked branch is unreachable in production until then and is covered by tests through an explicit-height seam.

### Diff

| File:line | Change |
|---|---|
| `src/math/MathAST.h:533` | `shouldUseDisplayLimits` restores `DISPLAY && height >= displayOperatorMinHeight()` (it was `return fm.isDisplayStyle()`) |
| `src/math/MathOperatorGeometry.h:18` | `LargeOperatorGeometry::displayLimits` carries the gate result |
| `src/math/MathOperatorGeometry.h:25` | `largeOperatorGateHeightPx()`: ladder height for the target size |
| `src/math/MathOperatorGeometry.h:31,38,52` | `largeOperatorGeometryWithGate()`; `limits = shouldUseDisplayLimits(...)` selects the stacked/side branch; `display` still selects the glyph |
| `src/math/MathOperatorGeometry.h:79` | SpaceAfterScript after the operator follows `limits`, not `display` (the only other term that depended on the old style-only branch) |
| `src/math/MathOperatorGeometry.h:97-100` | `largeOperatorGeometry()` = `...WithGate(..., largeOperatorGateHeightPx())` |
| `src/math/MathAST.cpp:1256-1260` | `NodeBigOp::_useDisplayLimits = geometry.displayLimits` (was `fm.isDisplayStyle()`, the bypass) |

`NodeSummation` and `NodeDefIntegral` need no edit: they already consume `largeOperatorGeometry`. Kept untouched from PARITY-02: axis centering, `UpperLimitGapMin`/`LowerLimitGapMin` and the rise/drop minima, removal of the 4 mu symbol pad (PD-11), italic-corrected limit x, the side-script branch, the display-variant glyph choice.

### Tests

- `testDisplayLimitsHonorsOperatorMinHeightThreshold` (`tests/MathEnginePhaseRegression.cpp:685`): not touched; its body is byte-identical to HEAD's. It passes.
- `:548-554` `testBigOpDisplayLayoutMatchesRendererGeometry`: its first assertion (`product uses display limits in display style`) asserted the BigOp bypass and cannot coexist with the uniform gate. It now asserts side limits at em 18; the rule-13a geometry checks run against the stacked branch through `largeOperatorGeometryWithGate(..., displayOperatorMinHeight())`, and the node/renderer agreement is asserted for the real (side) geometry.
- `:608` new `testLargeOperatorFamiliesShareDisplayLimitGate`: Σ, ∫ and ∏ follow the same gate, stack when it is opened, never stack in TEXT.
- `scripts/math-tex-parity-oracle.py` (operator rules): the oracle's `if display:` branch encoded "stack in every display style" (88 issues in 1,160 size cases after the change, all four operator-limit rules). It now derives the gate independently from the font's base variant record. Result: 0 issues in 122,283 comparisons, 9/9 negative controls detected, on both the fixed and the draw-mode runs.

MathEnginePhaseRegression: 0 failed checks (it had 1 at 01B).

## 2. The three fixes

None of the three uses `MathFontProfile`, `textStyleMetrics`, the Calculation Display root or any 14 px symbol. Each was reproduced as a failure on the current 18/12/8 tree before it was applied. Everything else in `readability-02.patch` stays retired, including the `drawTextBaseline(..., fm, ...)` signature change that shares a MathRenderer hunk with the interval fix.

### ADV-H01: Grapher `)` (contextual close)

- Finding (`out/math-readability-02/reviews/adversarial/final-v1-review.json`, narrated in `MATH_READABILITY_02.md` checkpoint 4): in Grapher the `)` key was ignored. Typing `y=(x+1)*2` stored `y=(x+1*2)` and evaluated to 4 at x=2 instead of 6.
- Patch hunks: `GrapherApp.cpp` @3332 (`case RPAREN`), `CursorController.cpp` @614 (`closeParen`, `closeDelimiter`), `CursorController.h` @166/@231. The patch's Calculation/Calculus/Equations call-site hunks are not part of this finding and are not ported (they keep `moveRight`).
- Applied: `src/math/CursorController.cpp:617-` (helper; `closeBracket` now delegates), `CursorController.h:172,239`, `src/apps/GrapherApp.cpp:3335`.
- Test: the seven original `grapher_readability_close_*.numos` scripts (real key events, no fixtures), copied unchanged to `tests/emulator/scripts/`. Before: 0/7 pass (`group`: 4 instead of 6; `power`: 65 instead of 15). After: 7/7.
- Independence: no profile symbols; the cursor walk uses only `kLayoutMaxDepth`/`kLayoutMaxNodes` and node types.

### V01: 1 px blue cursor

- Finding (`reviews/visual-selection/blind-visual-review.md` V01): "caret stands on/against the exponent 3's left strokes; at native size the result becomes a B/P-like composite". The 3 px black caret changed the perceived digit.
- Patch hunks: `MathRenderer.h` @27 (comment), @258 (`CURSOR_WIDTH` 3→1, `CURSOR_COLOR` black→`0x1565C0`); `MathRenderer.cpp` @914 (draw order). The rest of the 31 `.cpp` and 12 `.h` hunks are the font profile and are not ported.
- Applied: `src/ui/MathRenderer.h:30,261,265`, `src/ui/MathRenderer.cpp:918-925`. Caret anchor, viewport follow (`:663,808`) and bounds (`.h:140`) already share `CURSOR_WIDTH`, so no tracking change is needed. The caret is drawn under mathematical ink, and over an empty slot's outline so it stays visible.
- Test: `out/.../tools/cursor_check.py` (real keys `3`, `^`, `3`, LEFT; blink-phase frames). Before: 3 px, black → FAIL. After: 1 px, `(16,101,198)` (= `0x1565C0` on the 16-bit panel), no ink-core pixel lost → PASS. Images: `cursor/{before,after}/visible_x4.png`.

### Mixed-interval delimiter union

- Finding (`MATH_READABILITY_02.md` line 110): in a mixed interval such as `[0,+∞)` the right side was selected from the left side's already-quantised height and could exceed the box.
- Patch hunks: `MathAST.cpp` @1710, `MathAST.h` @1680/@1694, `MathRenderer.cpp` @2058 (only the left/right ascent/descent lines).
- Applied: `MathAST.cpp:1715-1728`, `MathAST.h:1715-1718,1729-1730` (four `int16_t`), `MathRenderer.cpp:2060-2077`.
- Independence: the document files this under the 14 px profile ("a 14 px"), but the failure reproduces with the standard profile. New `testMixedIntervalDelimitersFitLayoutBox` (`tests/MathEnginePhaseRegression.cpp:717`, 3 sizes × 4 depths × 4 closures): before, 10 failed checks at em 18 (depth 1/2/3), em 12 and em 6, e.g. `em=18 depth=2 box=88 left=88x9 right=88x11 slot=9` (the right glyph is wider than the reserved slot). After: 0. The test checks what the renderer draws (each side from its own stored extent).
- The interval renders pixel-identically to 01B in `result_intervals` (0 px changed), so the correction only matters when the two families quantise differently.

## 3. Verification (final source; native `C:/.piobuild/gate-native`)

| Gate | Result |
|---|---|
| MathEnginePhaseRegression | PASS, 0 failed checks |
| Geometry harness + oracle (2,000 random trees, seed 1346458163; also draw mode) | PASS, 0 issues, 9/9 negative controls |
| Hot paths | PASS, 96 phase records, 48,000 denied layouts |
| `test-math-readability-guard.py` | PASS, 100 % on every case |
| PARITY-02 product gate with observer and the 900 Tutor traces | 17/17 PASS, `fullGateClaim: true` |
| Native replays (`math-tex-parity-product-native.py --current-contract`) | 139/139 PASS, including the 7 new close scripts. Without the flag, the three documented pre-FORMAT fixtures fail exactly as the script says; they are not related to this change |
| MATH-TEX-01 | spacing 36/36 PASS; calculation input, shared-app fractions, FORMAT, viewport inside the 17 |
| SCIENTIFIC-QUANTITIES-01 | standalone 2,223 assertions PASS; product, visuals, quantities, temperatures inside the 17 |
| Fixed-pool lifecycle | PASS, 100 measured cycles after 4 warmups, 50-entry mixed history; HOME 57,696 / 19,352 B free, 67 objects, 3 timers, 0 handles (identical to 01B) |
| Browser matrix (full WASM rebuild) | 8/9: temperatures, scientific, brackets, spacing, input on Chromium/Firefox/WebKit, legacy PASS; `event-timings` FAIL (see below) |

`event-timings`: the full-matrix run failed at the Firefox component surface with the app still on `Menu` where `Calculation` was expected (same assertion as 01B). Five isolated reruns on the new package: 2/5 PASS; all three failures are the same `Menu`/`Calculation` assertion. Control: the same test and harness, 5 reruns against the 01B package: 4/5 PASS (the one failure is the same `Menu`/`Calculation` assertion). 2/5 against 4/5 is too few runs to call it a regression or to clear it (Fisher exact two-sided p = 0.52); the failing step is app-open timing (the surface is still on `Menu`), which none of these changes touch. Not fixed in this task, as instructed.

Builds (outside the repo):

| Target | firmware.bin | Partition | Free | Static RAM |
|---|---:|---:|---:|---:|
| WROOM | 6,314,768 B | 6,553,600 B | **238,832 B** (−1,056 B vs 01B 239,888 B) | 119,672 B |
| CAM | 6,227,072 B | 6,553,600 B | 326,528 B (−1,232 B vs 327,760 B) | 118,264 B |

Native emulator, pool profile and WASM builds succeed. `pio -t compiledb` rewrote `compile_commands.json` (and wiped the build tree it ran in); it was restored byte for byte (SHA-256 `18d2dd12…734d`).

## 4. Goldens (no re-bless)

Candidates regenerated with the final binary and compared pixel by pixel (clock excluded) with the 01B candidates: 41 of 42 identical (0 px). `math_results_undefined_unevaluated` differs by 3,212 px only because the 01B file was a stale intermediate frame of the multi-shot script (head, parity and fixed all show case 22/58, this run the intended 32/58); it is not comparable. `calc_structured_infinity` fails its own assertion in both runs (pre-existing). The blessed-golden differences are exactly those already recorded in 01B. Before/after captures of the changed surfaces are in `captures/`:

| Surface | Changed px | Meaning |
|---|---:|---|
| Calculus ∫ with limits | 447 | limits move from stacked to side; caret blue 1 px |
| Calculus ∫ wide limits | 604 | same |
| Display-style Σ (Math Visual `summation_limits`) | 680 | side limits |
| Calculation ∫ (TEXT) | 51 | caret only |
| Result intervals | 0 | |

No golden uses a changed surface with a cursor, so none changes.

## 5. Files

See `out/math-readability-regression-01c/files.json` for before/after SHA-256 of every file touched: `MathAST.h/.cpp`, `MathOperatorGeometry.h`, `MathRenderer.h/.cpp`, `CursorController.h/.cpp`, `GrapherApp.cpp`, `MathEnginePhaseRegression.cpp`, `math-tex-parity-oracle.py`, and the seven new `tests/emulator/scripts/grapher_readability_close_*.numos`.

## 6. Residual risks

- With the gate fed by the ladder, Σ/∏/⋂/⋃ never stack at em 18/12/8; LaTeX stacks them in display style. That needs a ladder variant ≥ 32 px (TXP-1).
- The side-limit ∫ uses the 43 px display glyph with its 540 DU italic correction, so the upper limit sits visibly to the right of the integral's top.
- Calculation, Calculus and Equations still use `moveRight` for `)`. The same helper is available if they should follow the Grapher.
- `event-timings` is intermittent in this environment.
