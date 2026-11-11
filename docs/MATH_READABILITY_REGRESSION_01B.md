# MATH-READABILITY-REGRESSION-01B — readable fractions restored on top of MATH-TEX-PARITY-02

## Summary

MATH-TEX-PARITY-02 replaced the level-0 ReadableInlineFractionPolicy (HEAD `src/math/MathAST.h:362-395`) with strict TeX stepping (`fm.isDisplayStyle() ? fm : fm.superscript()`). In Text style this drew numerators and denominators at 12 px and their scripts at 8 px. The published PARITY-02 emulator (`4987f4ea…`) showed exactly that, and the user rejected it. MATH-READABILITY-02 then compensated only in Calculation, using a Display root and a new 14 px font (WROOM free 134,304 B); nested level-0 fractions still shrank.

The user chose option 1:

- retire READABILITY-02 reversibly;
- restore the policy on top of PARITY-02;
- keep the 18/12/8 trio (TXP PD-4 and PD-10).

Result:

- At script level 0, numerators and denominators keep the base size (18 px) on every surface.
- First-level scripts are 12 px.
- Size reduction starts only at script level 1.

No font size, partition, semantic AST or frozen fraction geometry (`fractionBarGaps`, visible-gap placement, `drawFractionBaseline`, `drawRow`) was changed. Hardware: NOT EXECUTED.

## Retirement of READABILITY-02 (reversible)

- Full delta: `out/math-readability-regression-01/superseded/readability-02.patch`
  - 1,120,139 B, SHA-256 `54cb34b781e46a4a704babf9e05e92b3b21bf5661f80951927fee6fdfd9837d7`, 65 files (listed in `FILES.txt`).
  - Byte copies of all 65 current files are in `superseded/files/`.
  - Re-applying the patch to the predecessor state reproduces all 65 SHA-256 hashes (verified).
  - To re-apply: `git apply --binary out/math-readability-regression-01/superseded/readability-02.patch` on this tree.
- Predecessors come from the package manifests:
  - READABILITY-02 received state = PARITY-02 + SCIENTIFIC-QUANTITIES-01 (`out/math-readability-02/A-received/manifest.json`).
  - The PARITY-02 received state is in `out/math-tex-parity-02/A-received/manifest.json`.
- Restoration result (65/65 match, no ambiguous file, no mixed hunks; full table in `out/math-readability-regression-01/restoration-table.md`):

  | Predecessor | Files | Action |
  |---|---:|---|
  | Absent (new in READABILITY-02) | 48 | Deleted by explicit path after a hash-verified copy |
  | PARITY-02 | 12 | Restored |
  | SCIENTIFIC-QUANTITIES-01 | 4 | Restored |
  | HEAD | 1 | Restored |

- After restoration, the whole tree equals the PARITY-02 state byte for byte, except for:
  - the HEAD commit `39d7d1e` (hardware files, README);
  - the deliberate edits listed below.
- Calculation's root style is back to TEXT (`CalculationApp.cpp:188,208`), as in both HEAD and the PARITY-02 snapshot.
- READABILITY-02 also carried three fixes that do not depend on the 14 px profile. They are retired with it and remain in the patch for separate re-application:
  - Grapher `)` contextual close (ADV-H01);
  - 1 px blue cursor (V01);
  - mixed-interval delimiter union.

## Restored policy (`src/math/MathAST.h`)

`fractionPartMetrics`:

- At `scriptLevel == 0`: same font and size, style TEXT. The parent's cramped bit is kept for the numerator (TXP §H.2).
- From `scriptLevel >= 1`: `superscript()`, with style `numeratorStyle(...)`.

`fractionDenominatorMetrics` forces cramped.

`rootDegreeMetrics` now takes one ordinary script step (12 px at level 0) instead of ScriptScript (8 px). This is the only other place where PARITY-02 applied a two-step reduction at level 0.

Every other level-0 construct already keeps the base size and reaches fraction parts only through `fractionPartMetrics`:

- radicand (`crampedMetrics`);
- delimiters, functions and Measurement content (inherit);
- Grapher expressions and templates (Display root);
- Toolbox previews (receiver style).

The menu unit previews keep their deliberate pre-existing SCRIPT/SCRIPTSCRIPT roots, as at HEAD.

Layout, draw and cursor consume the same `fractionPartMetrics` / `fractionDenominatorMetrics` / `rootDegreeMetrics`. The glyph face follows the stored `scriptLevel`; the large-operator glyph follows `scriptLevelForStyle(style)`, which agrees because the level-0 fraction style is TEXT.

Measured cursor height (Calculation, pixel diff between blink phases):

| State | Base | Numerator | Denominator | First-level exponent |
|---|---:|---:|---:|---:|
| Fixed | 17 px | 17 px | 17 px | 12 px |
| PARITY-02 | 17 px | 12 px | 12 px | 12 px |

## MathEnginePhaseRegression triage

Results by state:

| State | Failed checks |
|---|---:|
| HEAD | 0 |
| PARITY-02 | 21 |
| READABILITY-02 | 21 |
| Fixed | 1 |

The 8 readable-policy checks pass unmodified, and the 8 "visible ink gaps are balanced" checks pass as a consequence of the fix. No check was weakened or deleted; three new strict checks were added.

| Check (`tests/MathEnginePhaseRegression.cpp`) | HEAD | PARITY-02 / fixed | Class | Action |
|---|---|---|---|---|
| NodeSubscript width includes SpaceAfterScript (:298) | width 18 = 10+7+1 | 19: scriptX 11 = base 10 + MathKern 1 | (b) TXP PD-7 (MathKernInfo BottomRight cut-ins) | Expectation = scriptX + sub + space, plus new check scriptX == base + MathKern |
| BigOp display ascent / descent matches renderer (:577/:579) | 33 / 22 (old formula with 4 mu symbol pad) | 32 / 21 | (b) TXP §H.9.2 rule 13a + PD-11 (pad deleted) | Expectation from §H.9.2 with source ink; new check that layout box == `largeOperatorGeometry` box and limit baselines |
| integral below DisplayOperatorMinHeight rejects display limits (:639) | false (18 px < 32 px gate) | true | (c) PARITY-02 drops the height gate (REFERENCE:73-74); TXP §H.9.4 keeps it | Unchanged; reported for decision |
| fraction denominator shift is based on ink ascent (:1031) | 16 | 17 = max(12 + axis 5 − 0, 3 + 13) | (b) TXP PD-4 / rule 15d: baseline-relative constant converted to the bar-relative stored shift | Expectation converted by +axis − barHalfDown |

Lane:

- `scripts/test-math-engine-phase.py` compiles the suite against a native build's own objects.
- `scripts/math-tex-parity-harness.py` now runs it after the geometry capture, so the PARITY-02 lane stays red until the (c) item is decided.
- Command: `python scripts/test-math-engine-phase.py --build C:/.piobuild/<dir>/emulator_pc --out <dir>`. CI configuration is unchanged.

## Oracle and viewport expectations

`scripts/math-tex-parity-oracle.py`:

- Its `styles()` table encoded the strict TeX T→S fraction step and the ScriptScript root degree. It now encodes PD-4 (D/T→T at level 0, S/SS→SS) and the one-step degree; it is still exact equality.
- Its `E-paren-tall` delimiter-ink negative control mutated a clipped assembly extender, which moves no visible pixel. It now mutates the topmost piece not clipped at its top, so the control is stronger.
- On the fixed geometry it reports 0 issues in 3,160 cases and 312,292 comparisons, with all nine negative controls detected.
- The same rule flags the PARITY-02 geometry with 268 style-transition issues.

`scripts/math-tex-parity-product-viewport.py`:

- It assumed that twelve nested fractions fit, which was only true under TeX demotion.
- Twelve fractions are again the legacy pan case (root 245 > canvas 177 px), exactly as in the unmodified HEAD `test-calculation-viewport.py`, which passes 7/7.
- An explicit six-fraction fit case (131 < 161) and an eighteen-fraction overflow case (359 > 184) each assert their precondition.

## Readability guard

`scripts/test-math-readability-guard.py --bin <program.exe> --out <dir>` (not wired into CI):

- Enters the corpus with real keys on Calculation: `1 + 2/(3^3)`, `1/2`, `1 + 1/2`, `(A+B)/(C+D)`, `1/(1+1/X)`, `X^((A+B)/(C+D))`, `66.63/(2×10^-3)`, and history.
- Removes the blinking cursor by taking the lightest value of ten frames.
- Fails when the level-0 numerator or denominator ink is below 90 % of the base digit ink (12 px, calibrated per binary).

| Binary | Result | Worst ratio |
|---|---|---|
| HEAD | PASS | 100 % |
| PARITY-02 `4987f4ea…` | FAIL | 67 %; 42 % on the nested fraction |
| READABILITY-02 `dc7ccd65…` | FAIL | 75 % on the nested fraction |
| Fixed | PASS | 100 % everywhere |

## Verification

| Gate | Result |
|---|---|
| PARITY-02 harness geometry + oracle (2,000 random drawn trees, seed 1346458163) | 0 issues / 3,160 cases, 9/9 negative controls, 0 layout allocations |
| PARITY-02 hot paths (`profile-math-tex-parity-hotpaths.py`) | PASS, 96 phase records, 48,000 denied layouts; 0 own C++/AST/cursor allocations (LVGL draw C allocations counted, not denied, as before) |
| PARITY-02 product suites (17) | 17/17 PASS: 13 executable suites, viewport, FORMAT, FORMAT presentation, 900 Tutor traces (host binaries rebuilt against the fixed objects) |
| MathEnginePhaseRegression | 1 failure, (c) above |
| MATH-TEX-01 | calculation input 114 PASS incl. 50-cycle lifecycle; math spacing 36/36; shared-app fractions 6/6 |
| SCIENTIFIC-QUANTITIES-01 | constants check, physical constants, standalone (2,223 assertions), product and visuals PASS. Fault and mutation overlays not run: uncertainty kernel, unchanged here |
| Fixed-pool lifecycle | PASS: 100 measured cycles after 4 warmups, 50-entry mixed history; HOME 57,696/19,352 B free, 67 objects, 3 timers, 0 handles in all 104 samples; modal minimum 6,128 B free / 5,520 B largest |
| Periodic host checks | 3/3 PASS |
| Browser lanes (fully rebuilt WASM package, `math-tex-parity-product-web.py`) | 8/9 suites PASS in the full matrix: temperatures, scientific, brackets, spacing, input on Chromium/Firefox/WebKit, legacy. `event-timings` failed once after 54 Chromium-shell samples: the next surface was still on `Menu` when `Calculation` was expected. Two isolated re-runs both PASS on all 6 surfaces (3 browsers × shell/component, 54 samples each). The failure is recorded as intermittent app-open timing under load; it is not typography |

Builds (outside the repository, `C:/.piobuild/mrr01b-*`):

| Target | firmware.bin | App partition | Free | Static RAM |
|---|---:|---:|---:|---:|
| WROOM | 6,313,712 B | 6,553,600 B | **239,888 B** (+48 B vs 239,840) | 119,672 B |
| CAM | 6,225,840 B | 6,553,600 B | 327,760 B | 118,264 B |

The native emulator builds. The readable-policy change is size-neutral.

## Goldens (no re-bless)

Content diffs exclude the status-bar clock. Before/after/diff images are in `out/math-readability-regression-01/goldens/`.

- Against PARITY-02, only two goldens change:
  - `calc_fraction_sum`: 529 px;
  - `math_showcase_smoke`: 460 px.
- Against HEAD, the changed goldens are:
  - `calc_fraction_sum`: 399 px;
  - `math_showcase_smoke`: 349 px;
  - `grapher_templates_smoke`: 1,631 px;
  - `grapher_expr_smoke`: 54 px;
  - 1–11 px each in grapher_smoke/table/trace, probability ×2, regression_data, sequences ×2 and statistics ×2.

  All of these except the two fraction goldens are identical to PARITY-02, so they come from PARITY-02 (identifier italics, spacing), not from this change.
- `calc_1_plus_2`, `grapher_graph_smoke`, `launcher_smoke` and `regression_smoke` are unchanged.

## Evidence

- `out/math-readability-regression-01/`: baseline, superseded, restoration tables, guard runs, `size-ink-table.md`, `exponent-comparison/`, goldens, verify.
- `tools/` holds the measurement scripts.
