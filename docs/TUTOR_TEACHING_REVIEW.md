# TUTOR-ENGINE-01 teaching review

The independently compiled current-source candidate passes **43/43 trace cases**: 21 requested acceptance cases and 22 separate reviewer challenges. Every case is Complete, passes a separate kernel replay, and reproduces deterministically. All **526 independent wording mutations are Rejected**. English, Spanish, French, and pseudolocalized rendering preserve the entire mathematical graph and all verification results. Final fixed-64-KB-pool screenshots pass the bounded visual checks below, including the repaired negative-scalar sign. No blocking teaching issue remains in the inspected evidence.

The reviewer inspected actual generated states and instructions, including operands, signs, branching, denominator restrictions, terminal claims, and method selection. Production files were not edited by this reviewer.

## Reproduction and evidence

```powershell
python scripts/tutor-teaching-review.py --compile-boundary
```

The runner independently compiles `GiacEngine.cpp` with the host macro profile, verifies that its source/header hashes did not change during compilation, and links a copied fixed `kgen.cc.o` rather than the baseline vendor object. It renders the same derivation in all four locales and compares every field except rendered prose and elapsed time. Each Complete case is solved a second time to test deterministic replay. Extra assertions reject negative printed balancing magnitudes, ordered complex-root terminology, added zero, unit scaling, equivalent-state no-ops, padded `x=1`, and missing pure-square strategy for `x^2=9`. The runner also checks explicit factor equations/scaling and contiguous compact groups that cannot hide branches, restrictions, rejection, or terminal classification. Mutations replace every message key, parameter value, and parameter type; wrong keys/values retain a forged Verified flag.

Evidence is in `out/tutor-engine-01/review-teaching/{summary.json,traces.json,traces.txt}`. Current executable SHA256: `9e91444f208bfdbc4d51ce358576c0be6047ca9e408cd9e693cbd93ef0b10325`. This final trace rerun includes the 4096 symbolic-call budget. The summary records the source, header, executable, and two copied object hashes. The runner exits nonzero when any assertion fails.

## Findings in the reviewed candidate

1. **Negative-scalar blocker repaired and independently rechecked.** Earlier captures obscured the minus in -3 and -1 because it collided with the equals sign. The fixed-pool `square-negative-00.png` clearly displays `x=(-3)`, `physical-factor-03-scrolled.png` displays `3*x=(-4)`, and `physical-factor-04-scrolled.png` clearly displays the negative fraction -4/3. The mathematics is unchanged; the presentation now makes its sign unambiguous. Earlier failure pixels remain archived as `pre-sign-fix-square.png` and `pre-sign-fix-excluded.png`.
2. **Minor wording refinement.** `0*x=0` is now described as collecting like terms rather than expansion. "Evaluate the zero product" would describe that special simplification more precisely. `(2*x+1)^2=9` remains an expansion/factorization strategy rather than a shorter shifted-square strategy; its displayed transformations are valid.
3. **Other actual visual evidence is satisfactory.** Compact linear instructions, the STIX `x=5`, and physical controls fit without clipping. Active-alternative labels and row highlights identify the branch being changed; distinct alternative labels resolve the ambiguity with simultaneous system rows. `rational-rejection-04-scrolled.png` explicitly labels the red rejected `x=1`, and `rational-rejection-05.png` shows only the valid `x=(-1)`. Pseudolocalized scrolling reaches complete formulas. The earlier contact sheet also shows readable Spanish/French linear messages and equation-number labels for systems.

The final contact sheet and individual 320x240 captures are in `out/tutor-engine-01/review-resource/pool64-signfix-final`. Contact SHA256: `24e355dcccdada6c8f2f1d74c5afd5b49301ac2649649eccc1a83e948f3ed84e`. The reviewer independently viewed the contact and six full-size sign/factor/rejection/pseudolocalized captures. These images precede the call-budget-only refresh; the final trace rerun above uses the updated budget. This is evidence for the inspected cases and layouts, not a claim of universal correctness or full Spanish/French translation approval.

## Verified improvements and representative results

`x=1` has one already-isolated annotation. `x^2=9` uses square roots. `(x-1)^2=0` uses its authored square directly and explains that a repeated root occurs once. Factored inputs retain their factors instead of expanding and refactoring. `(2*x-3)*(3*x+4)=0` now retains each factor equation, addition/subtraction, and division; compact groups retain those primitives for detail inspection. `5-x=2*x-1` has no artificial ordering-only expansion. `x^2=x` keeps zero. Both-side quadratics and `x^2=4/9` complete with correct normalization and no unit operations. Complex roots are described as opposite roots; the formula message uses the selected y when solving for y.

`x/(x-1)=1/(x-1)` captures `x-1 != 0`, multiplies both sides by `x-1`, rejects 1, and concludes that no candidate remains. `(x^2-1)/(x-1)=0` keeps only -1; `x/x=1` ends in a conditional identity. System instructions identify the actual equations, variable, and signed magnitude: for example, "Subtract 1/2 times equation 2 from equation 1 to eliminate y." Dependent systems retain their simultaneous remaining relationships.

The catalog validates typed placeholders. Existing Spanish/French messages are selected when present; untranslated messages explicitly fall back to English. Pseudolocalization changes prose only. Full Spanish/French translation was not reviewed.

Earlier failing evidence is retained in `initial-*` and `before-terminal-fix-*` files. Review found and reported both a dangling reference through a temporary State and an invalid already-solved terminal claim under a denominator exclusion. The current independently compiled corpus confirms their repairs. The earlier terminal error was an invalid tutor claim, not a trustworthy derivation disagreeing with Giac.
