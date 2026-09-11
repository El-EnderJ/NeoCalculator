# TUTOR-TEACHING-UX-01 — independent teaching review

Teaching review is complete. No blocking teaching or visible mathematical-content issue remains in the inspected final native coverage. The two defects found during independent screen review were repaired and recaptured; mathematical/resource reviews remain separately attributed.

The [complete before/after walkthroughs](../out/tutor-teaching-ux-01/review-teaching/walkthroughs.md) contain every linear, quadratic, rational, and system page with actual prose, mathematical objects, 320×240 captures, and readable contact sheets.

## Evidence and independence

The teaching reviewer changed only review evidence and this report. UI, mathematical planning, checking, and catalog changes belong to other contributors. The baseline native executable was rebuilt by the resource reviewer from the preserved 1,188-file source snapshot, fingerprint `8b4220ade0d7b30dc9fadbe7f71b1eaba945ade476e96c583c236bc2c6be3c5e`. The exact executable SHA-256 is `6faa97a6cbbc2760bd9d3b0e502a65c0fec19e4da9d991ddc5f285ab3720fdfa`.

Reproduce the baseline evidence with:

```text
python out/tutor-teaching-ux-01/review-teaching/capture-baseline.py
```

The probe enters equations through the native application controls, opens Steps, independently checks the trace, and captures every default and detailed page at four vertical positions. It verifies formula widgets, existing layout assertions, and a single cached derivation build. Its 16 complete sequences contain 63 pages and 252 unmodified 320×240 framebuffer captures. Contact-sheet captions are outside the captured pixels. No goldens, masks, or production sources were altered.

Evidence is in `out/tutor-teaching-ux-01/review-teaching/baseline/`: `summary.json` records the binary hash and every capture; `traces.txt` records actual authored, before, after, and condition states; per-case JSON retains machine-readable replay data; `.numos` scripts and logs reproduce each sequence. Actual contact sheets and full mathematical traces were inspected for all eight equations/systems below, not just initial screenshots.

## Baseline findings, ordered by teaching impact

| Finding | Actual evidence | Required presentation change |
|---|---|---|
| Default linear presentation omits intermediate equations. | `linear-default-contact.png` goes directly to x=5 while combining subtract 5 and divide 3. Detail reveals 3x=15. `linear-unfamiliar-default-contact.png` hides both −6x+7=1 and −6x=−6 for 7−4x=2x+1. | Default pages must show one operation with its checked before and after. Preserve all intermediate states without requiring a detail toggle. |
| Quadratic work is compressed into a source-language sentence. | `quadratic-default-contact.png` and `quadratic-unfamiliar-default-contact.png` show `sqrt`, `^`, `*`, and `+/-` in prose, then jump to two roots. | Separate coefficient identification, discriminant calculation, general formula, substitution, and final roots using checked parameters and structured math. |
| Repeated DOWN can scroll entirely beyond the content. | Every sequence has empty later scroll captures, including the one-line final x=5 screen. | Clamp scrolling to actual content bounds. A key intended to inspect a solution must not lose it in empty space. |
| Operations lack their starting equation; general purposes obscure distinct goals. | Linear detail shows only the after state. The unfamiliar linear trace repeats “remove that term” for moving 2x and removing 7, although the purposes differ. | Show the before state; explain collecting variable terms, isolating the variable term, then isolating the variable. Keep the actual operand checked. |
| Domain information is repeated in ASCII and becomes detached from the relevant candidate during scrolling. | `rational-default-contact.png` repeats `x-1 != 0` in prose and a label. The rejection page separates prose, restriction, accepted root, and rejected root over several viewports. | Render the existing condition as structured x−1≠0. Make the rejected candidate and reason clearly associated; retain the restriction on the final solution page. |
| “Alternative” confuses proof cases and final solution sets. | The unfamiliar rational trace finishes with the single accepted x=2/3 labelled “Alternative 2”; the quadratic terminal still labels both roots as alternatives. | Use case labels while solving factors, solution labels at the terminal, and simultaneous-solution wording for systems. Keep the active case visible near its before/after work. |
| System grouping hides useful row states and prose embeds awkward fractional operands. | `system-default-contact.png` combines scaling equation 2 and eliminating y. `system-unfamiliar-detail-contact.png` displays `/(-19/2)` and a 3/2 row multiplier only in prose. | Show each checked row operation and both affected row states; use structured row notation where useful. Keep unchanged equations visually distinct. |
| Progress and availability are conflated; signed scalar presentation is needlessly parenthesized. | Headers read “Steps 1/2 | Complete”; rational roots render x=(−1), and negative system sides receive large parentheses. | Separate “Step 1 of …” from checked explanation availability and final solution. Use the agreed narrow signed-scalar presentation fix while preserving mathematically necessary parentheses. |

All baseline traces passed the independent replay exposed by the application. The findings above are teaching/presentation defects; they do not establish a mathematical checker failure.

## Complete baseline trace review

| Entered equation(s) | Default / detail pages | What the actual trace establishes |
|---|---:|---|
| 3x+5=20 | 2 / 3 | Subtract 5 → 3x=15; divide 3 → x=5; terminal. Default hides the first result. |
| 2x²+3x−4=0 | 2 / 2 | a=2, b=3, c=−4, D=41; two exact roots. No displayed coefficient/discriminant/formula progression. |
| (x²−1)/(x−1)=0 | 6 / 6 | Exclusion x−1≠0 precedes clearing; x²=1 yields both signs; x=1 is rejected; x=−1 remains. |
| x+y=3; x−y=1 | 3 / 4 | Equation 2 minus equation 1, divide by −2, eliminate y from equation 1, simultaneous x=2,y=1. |
| 7−4x=2x+1 | 2 / 4 | Subtract 2x, subtract 7, divide by −6, x=1. Actual signs and declared operands agree. |
| 3x²+2x−2=0 | 2 / 2 | a=3, b=2, c=−2, D=28; roots (−1±√7)/3. This unfamiliar case confirms the same explanatory gap beyond the acceptance example. |
| ((3x−2)(x+4))/(x+4)=0 | 8 / 9 | Preserve x+4≠0; clear, factor, solve two cases with actual balancing, reject x=−4, retain x=2/3. Case 2 alone is the final accepted solution. |
| 2x+3y=13; 5x−2y=4 | 3 / 5 | Scale equation 1, eliminate x from equation 2, scale equation 2, eliminate y from equation 1; simultaneous x=2,y=3. Exact fractional operands agree with row changes. |

## Wording recommendations passed to the mathematical reviewer

Use whole sentence templates whose purpose is validated against the declared operation. For example: “Subtract 5 from both sides to isolate the term containing x.” Then: “Divide both sides by 3 to isolate x.” Moving a variable term should instead explain collecting variable terms. Complex expression operands belong in structured math, with prose referring clearly to that displayed operation.

Explain the zero-product principle before treating the factors as cases. Explain taking both square-root signs without calling complex roots positive and negative. Explain that an original denominator cannot be zero before clearing it. Associate rejection with that original denominator and the actual candidate. For systems, name both the source and changed equation and the variable being eliminated; do not present a tuple as alternatives.

## Candidate wording review

An additional fresh execution of 18 candidate host traces was frozen under `out/tutor-teaching-ux-01/review-teaching/candidate-wording/`, executable SHA-256 `6ad8d7c63ec2c80c157b1f3566d256d667f5adf9475fc9f81be134c37ec58931`. Reproduce it with `review-wording.py` in the parent evidence directory. The reviewer inspected every before/after state, declared operand, sentence, and auxiliary for the eight baseline families plus fractional linear work, a zero square, a complex square, a scaled square, a repeated root, a zero-product with a zero solution, conditional identity, excluded-all rational equation, dependent system, and inconsistent system.

No blocking wording/sign issue was found in those inspected traces. The distinct collecting, isolating-a-term, isolating-a-variable, and isolating-a-square purposes agree with their actual operations. The zero-product wording explains why cases are exhaustive; complex square roots use both signs without ordering complex numbers. Original denominator restrictions and rejection references remain explicit in the trace.

The new “displayed term/amount/coefficient/multiple” messages require the corresponding exact structured operation to be visible in the UI. This was checked in the final screens, especially for 2/3, −19/2, and a 3/2 row multiplier. Host wording review alone does not demonstrate that connection. The unfamiliar quadratic 3x²+2x−2=0 retains discriminant 28 in the instructional calculation even though its final roots simplify to (−1±√7)/3.

One nonblocking content refinement remains: clearing constant denominators mentions keeping original restrictions even when the trace contains none. This does not invent a restriction or change the mathematics.

Final review inspected all 18 requested complete native sequences, the complex policy and unfamiliar cases, actual structured quadratic facts, branch/domain continuity, scalar signs, scroll bounds, and locale rendering. Mathematical invariance, resource measurements, and regression results remain separately attributed to the responsible reviewers.

## First guided native candidate — reviewed, two blockers

Frozen native SHA-256 `551776accfec2ba5a36ca4fbfcc016d2c7ac6dba08ccf64d10beeafdd7c71189` was exercised by `capture-guided.py`. The two evidence directories `candidate-smoke/` and `candidate-remaining/` contain 32 complete sequences, 128 presentation pages, and 260 framebuffer captures. Coverage includes all 18 acceptance inputs, actual production NEGATE (row 1, column 9), complex policy, four unfamiliar cases, selected summary sequences, and English/Spanish/French/pseudolocalized linear plus pseudolocalized quadratic work. Each page was captured from the top through its measured bottom, with overlapping scroll positions. Additional DOWN presses must stop at the measured bottom without changing the page. Toggling summary/guided must retain the same mathematical trace. All executable assertions passed; that did not preclude the visual findings below.

1. **Blocking: the quadratic ± glyph renders as +.** `candidate-smoke/quadratic-guided-02-scroll1.png` shows only a plus in both general and substituted formulas, despite an actual PlusMinus AST and prose referring to both signs. The same loss occurs for the unfamiliar quadratic. This is a pixel-level mathematical-content defect, not a proof-graph defect. It was reported immediately with its exact framebuffer; final approval is withheld until repaired and recaptured.
2. **Blocking: the real-empty pure-square explanation loses the coefficient normalization bridge.** In `candidate-remaining/no-real-guided-contact.png`, the learner sees x²=−1, then is asked to match it to ax²+bx+c=0 while the coefficients show c=1. Those coefficients belong to the zero-right residual x²+1=0, which this page does not display or explain. A checked pure-square impossibility explanation or an explicit checked normalization presentation is needed.

The remaining inspected sequences preserve the requested mathematical teaching chain: linear intermediate states are visible by default; explicit expression/fraction operands match the prose; zero-product branches retain x=0; square roots retain both signs; original restrictions use ≠ and stay with rejected candidates; the unfamiliar rational final shows x=2/3 as its sole accepted solution; system rows identify their source/target and retain dependent relationships and simultaneous tuples. Standalone negative values have a clear minus without needless enclosing parentheses. Vertical scrolling no longer reaches an empty viewport. Unfamiliar discriminant 28 remains 28 during substitution, with √7 appearing only in the simplified final roots.

Nonblocking refinements reported separately were the footer's vague “Arrows” hint, the division-page title “Isolate the variable term” rather than “Isolate the variable,” and crowded spacing around the row-operation arrow. No additional mathematical or teaching blocker was observed in this bounded candidate review.

## Final evidence and disposition

| Evidence | Exact candidate and result |
|---|---|
| [Full native matrix](../out/tutor-teaching-ux-01/review-teaching/final-ui/capture-ledger.json) | SHA-256 `45d3498dc30c50e536103652a5a1d5eba659ec9b42ace62afa3bc42e722ee4d2`: 35 complete sequences, 134 presentation pages, 267 captures. All family assertions passed, including 12 Steps reopen cycles and HOME/re-entry for each of 26 equation/system cases. The separate first pan fixtures were invalid test choices (one unsupported, one too narrow), not product failures. |
| [Final glyph/normalization/precedence refresh](../out/tutor-teaching-ux-01/review-teaching/final-glyph-ui/manifest.json) | Release SHA-256 `bf3c1521b58c1d8c1eaf94fa1efb31dd213abee7535c2ea2ebebfcf8de5388e0`: six complete sequences, 28 pages, 60 captures, all assertions passed. This reran affected quadratic guided/summary/pseudo work, the unfamiliar discriminant, real-empty normalization, and a new negative-b challenge after the final ± width correction. |
| [Actual wide-formula pan/restore](../out/tutor-teaching-ux-01/review-teaching/final-glyph-ui/wide-pan-results.json) | Supported six-digit-coefficient rational equation: 33 captured viewports, visible motion, exact initial content pixels restored on the fourth VAR press, no page or derivation change. |

The final ± images show a distinct lower minus stroke with clear spacing from the radical. The negative-b challenge visibly and structurally retains `(−3)²`, and negative c remains parenthesized as a product factor. The real-empty coefficient page explicitly shows the equivalent zero-right equation x²+1=0 before identifying c=1. These close the two blocking findings. Final footer text explains step navigation, scrolling, summary, pan, and BACK, including in the inspected pseudolocalization. The division heading now says “Divide both sides.”

The durable [teaching UI suite](../scripts/test-tutor-teaching-ui.py) checks source references, verified-step ownership, at most four formula widgets, explicit ≠ conditions, coefficient and discriminant values, ±/fraction/root AST structure, negative-square/product parentheses, every accepted final branch or system relationship, retained final restrictions, the coefficient normalization bridge, and the visible linear intermediate. It compares the same mathematical graph across summary/guided and language changes, and captures every page through its measured scroll bottom. Pixel-level ± review remains explicit: the earlier missing stroke passed AST checks, which is why this review did not equate structured-node presence with readable mathematics. The old `test-tutor-ui.py` entry point delegates to this suite and retains its negative/repeated-root, cache, locale, and HOME coverage.

Reproduce the final affected review with:

```text
python scripts/test-tutor-teaching-ui.py --bin out/tutor-teaching-ux-01/native-release.exe --out out/tutor-teaching-ux-01/review-teaching/final-glyph-ui --cases quadratic quadratic-unfamiliar quadratic-negative-b no-real wide-pan
```

No open blocker remains in this bounded teaching review. Minor refinements remain available: shorter pure-square real-empty wording could avoid the existing discriminant detour; row-operation arrows are readable but crowded; clearing constant denominators still mentions keeping restrictions when none exist. Full Spanish/French content is intentionally incomplete and visibly uses the English fallback. This review does not establish universal pedagogical quality or replace the separate [embedded resource/lifecycle evidence](TUTOR_TEACHING_UX_RESOURCES.md).

Reviewer-owned changed source files for this task are this report, `scripts/test-tutor-teaching-ui.py`, and `scripts/test-tutor-ui.py`. Review-only probes, JSON, logs, framebuffers, contacts, and walkthroughs are under `out/tutor-teaching-ux-01/review-teaching/`. No engine, catalog, or UI production code was edited by the teaching reviewer.
