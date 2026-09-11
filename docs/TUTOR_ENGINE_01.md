# TUTOR-ENGINE-01 — checked Equations explanations

The rebuilt Equations app now has a real offline Steps view. A deterministic
planner constructs a derivation from the authored equations; an independent
rule checker replays every operation before it can be displayed. Giac remains
the ordinary answer service. English explanations are catalog messages, never
runtime-generated prose. No external solver, LLM, Node/JVM mathematics runtime,
third-party tutor code, or noncommercial language resources are shipped.

## Preserved source and reuse decision

Work started on `main`, HEAD `1a95bb4f0f738fc653352c3fbd280fe08bc3685b` **with
the uncommitted Equations rebuild present**. No Git checkout, stash, reset, stage, commit, push or
hardware flash was performed. The initial source fingerprint is
`a0176658aca637b864da0418595223ff85b6ee691b3553766d277bb0de3ccdf6`.
The manifest and dirty overlay are in
[`out/tutor-engine-01/baseline`](../out/tutor-engine-01/baseline).

[The exact-vendor investigation](TUTOR_GIAC_REUSE.md) records 252 isolated
probe combinations and all flag/log restoration checks. Giac 1.4.9+khicas.57
under this embedded feature profile provides useful parser/exact/symbolic
primitives, but the tested solve commands emit no usable derivations. Its
process-global formatting callback is not a proof protocol. Console scraping
and the old answer-agreement logger were therefore rejected.

`RuleEngine`, `TutorTemplates`, `OmniSolver` and `SystemTutor` were audited;
their old trace pipeline is removed from Equations. Existing AST display
conversion is reused. No competing general-purpose CAS was added.

Two executable integration defects were repaired: canonical copies of equation
sides avoid direct-solve stalls while authored sides remain available for
domain checks; the vendored complex-display accessor now addresses the real
struct member instead of LLP64 padding. The latter has a provenance note in
[`lib/giac/NUMOS_CHANGES.md`](../lib/giac/NUMOS_CHANGES.md).

## Supported methods

| Family | Release behavior | Explicit boundary |
|---|---|---|
| Already isolated | One terminal explanation; reversed sides are swapped | Exact rational isolated values |
| Identities / contradictions | Exact classification, including denominator-conditional identities | Supported rational-polynomial syntax |
| Linear | Distribution, collection, both-side balancing and rational division | Exact rational coefficients; variables on either side |
| Products | Preserve authored factors; exhaustive factor-zero branches; isolate each factor by checked primitives | Two nonconstant linear factors, including repeated factors |
| Quadratics | Pure-square method first; useful exact rational factoring; otherwise quadratic formula | Degree at most two; repeated/zero roots retained; real/complex policy respected |
| Rational | Capture original exclusions, clear the polynomial LCM, solve, reject excluded candidates | Each original denominator polynomial has degree at most two; cleared residual degree at most two |
| 2×2 / 3×3 systems | Exact elimination, row swapping/scaling/addition; unique tuple, inconsistent system or related-variable family | Linear rational coefficients, including constant rational denominators |
| Stored values | Snapshot referenced A–F exact rational values without assigning temporary values | Nonrational stored values and assumptions on solve variables are refused |
| Other families | Separate unsupported tutor status; ordinary result remains usable | Variable radicals, absolute values, exponentials, logs, trig, higher degrees and numerical methods are extension work |

The boundary accepts `x`, `y`, `z`; the current Equations UI selects `x` for one
equation, `x,y` for two, and `x,y,z` for three. A refusal means this implementation
does not provide a checked method, not that the mathematics is impossible.

## Model and checker

[`Derivation.h`](../src/math/tutor/Derivation.h) is engine-neutral. A snapshot
contains authored sides, solve variables, real/complex and angle policy,
referenced stored values, solve-variable context values, input epoch and engine
generation. Conditions retain original denominator syntax provenance before
evaluation. States own equations, branch identities/status, conditions,
classification and a fingerprint. Source paths identify an equation, a side and
child indices; displayed row highlighting uses only safe whole-equation
provenance.

Snapshot counts, string lengths and duplicate identifiers are bounded before
copying or shared-context parsing. Ten malformed snapshot probes reject oversized
values, duplicate names and effectful identifiers without changing stored values
or the engine generation.

Each step has a stable rule ID, before/after references, declared operands,
auxiliaries, affected paths, prerequisite/introduced/discharged conditions,
relation, semantic message key, typed parameters, optional detail references and
a verification outcome. Equivalent transformations, exhaustive splits,
rejections and classifications are distinct. Domain/terminal annotations may
keep equations unchanged; the condition set or classification records their
purpose. Candidate-producing implications and numerical approximation have
reserved relation types for later methods.

The checking kernel lives privately inside the existing `GiacEngine` boundary.
No `giac::gen` or context escapes into UI headers. It treats steps and even their
"verified" flags as untrusted: it validates pure syntax before evaluation,
reconstructs the declared operation on **both sides**, checks the actual operand
and purpose, preserves unaffected rows/branches/conditions, and regenerates the
expected message parameters. Polynomial coefficients use exact derivatives at
zero plus reconstruction because this snapshot's `_coeff` path lost rational
denominators in a reviewed reproducer.

Finite quadratic completeness follows from the checked degree, nonzero leading
coefficient and Vieta reconstruction, including branch count/distinctness.
Product completeness uses the zero-product theorem and both linear factors.
System completeness follows from reversible row operations and checked final
rank/relationships. Checking candidates in the original equations is an
additional validity test, not the completeness proof. No symbolic proof relies
solely on numerical spot checks or residual equality after scaling.

Validity, candidate validity, completeness and reconciliation with the ordinary
adapter have separate VERIFIED/REJECTED/UNKNOWN outcomes. Failed/unknown
instructions are withheld. A reconciliation failure preserves the ordinary
answer and its authored reproducer; no primary answer is rewritten to force
agreement. Resource exhaustion returns partial status with no claim of
completion. State/condition/branch fingerprints prevent cycles.

## Rule catalog

| Stable rule ID | Required check / operation |
|---|---|
| `domain.nonzero` | Introduce the next exact exclusion captured from authored denominator syntax |
| `values.substitute` | Replay saved rational bindings structurally; preserve branch identities |
| `algebra.expand` | Exact declared expansion on both sides; preserve the common domain |
| `algebra.collect` | Exact normal form on both sides; reject identical displayed states |
| `equation.swap` | Exchange sides and preserve all other equations |
| `equation.add` | Add the declared nonzero term to both sides; verify removal of an actual term |
| `equation.divide` | Divide both sides by the nonzero rational leading coefficient, excluding one |
| `rational.clear` | Use the exact original-denominator LCM under every captured nonzero prerequisite |
| `polynomial.factor` | Two nonconstant linear factors reconstruct the degree-two left side; right side is zero |
| `product.zero` | Factor equations cover both zero sets; merge only coincident linear roots |
| `square.branches` | Left side is the variable squared; both exact roots retained, zero only once |
| `quadratic.formula` | Check a,b,c, discriminant, degree and exhaustive exact branches / negative-real discriminant |
| `candidate.excluded` | Candidate makes a specific original denominator zero |
| `terminal.isolated` | Variable already isolated, rational value, no undisclosed exclusions |
| `terminal.identity` | Equality on the captured domain, with conditional wording where necessary |
| `terminal.contradiction` | Nonzero constant residual, zero variable coefficients |
| `terminal.finish` | All active candidates satisfy original sides and every exclusion; rejected-only set is empty |
| `system.swap` | Swap the declared distinct rows and preserve the rest |
| `system.scale` | Nonzero rational divisor equals the named pivot coefficient, excluding one |
| `system.add` | Replay the declared row multiple and verify elimination of the named variable |
| `system.finish` | Check reduced relationships, rank, inconsistency or each isolated variable of a unique tuple |

Plans are separate from these rules. Presentation groups fold at most three
contiguous equivalent primitives in one branch. Detail retains each operation;
conditions, branch splits, rejections and classifications cannot be hidden in a
group. The checker validates these references too.

## Wording, view and representative explanations

[`Messages.inc`](../src/math/tutor/Messages.inc) contains the complete English
catalog and placeholder schemas. Several Spanish/French messages demonstrate
the seam; untranslated messages explicitly fall back to English. Pseudo locale
expands prose. Mathematical parameters never become localized parser input.
Locale changes preserve all state/branch/condition fingerprints and verification
results.

- `x=1`: “x is already isolated. This equation gives its solution directly.”
- `3*x+5=20`: subtract 5 from both sides → `3*x=15`; divide by 3 → `x=5`;
  classify the checked complete solution set.
- `x^2=9`: take both square roots → alternatives `x=-3`, `x=3`; finish.
- `x^2-5*x+6=0`: factor → `(x-2)*(x-3)=0`; split → `x-2=0` or `x-3=0`;
  add 2 in the first branch and 3 in the second → `x=2` or `x=3`; finish.
- `(x^2-1)/(x-1)=0`: require `x-1!=0`; multiply by `x-1`; add 1; take both
  roots; reject 1 because the original denominator vanishes; retain `x=-1`.
- Dependent system `x+y=2; 2*x+2*y=4`: subtract twice equation 1 from equation 2;
  retain `x+y=2` and `0=0`; explain the family with the remaining relationship.

TOOLBOX opens Steps. LEFT/RIGHT select main steps; EXE toggles compact/detail;
UP/DOWN scroll; VAR scrolls wide mathematics; BACK returns to the ordinary
result. Prose uses Montserrat and formulas use VPAM/MathCanvas/STIX. Three math
widgets and three branch/row labels are reused. Long traces do not create an
LVGL object tree per step. Mathematical trace ownership is independent of view
ownership. The view uses explicit alternative/rejection labels and safe row
highlighting. Signed equation subformulas use authentic STIX parentheses to
avoid the legacy nested-row unary-minus spacing collision; no renderer geometry
formula changed.

Committed edits invalidate the cache. Reopening Steps, changing language and
switching detail modes do not rebuild it. Snapshot guards include saved values,
angle/domain, epoch and generation. HOME tears down the trace and views. Web
input now accepts the full public semantic-key range and routes HOME globally.
Equation list/editor layouts, transactional editing, templates, physical key
semantics, headers/modifier badges, Calculus commands and Grapher templates are
preserved.

## Bounds and measured resources

Enforced bounds: 48 primitives, two branches, three equations, eight original
conditions, depth 20, 160 authored syntax nodes, 512 authored bytes / bytes per
serialized expression, 64 KB conservative retained-capacity estimate and 128 KB
aggregate tracked vector payload during construction. The initial 1,200-call
allowance correctly refused 13 dense 3×3 cases as partial; the final 4,096-call
allowance covers their full independent proof replay (observed maximum 1,571).
Each primitive cooperatively yields on firmware after committing its checked
state. Individual synchronous Giac calls are not planner-cancellable.

| Measurement against immediate preserved baseline | Final candidate |
|---|---:|
| Production flash | 5,458,549 B; **+54,944 B** |
| Static DRAM | 118,984 B; **+24 B** |
| IRAM text | 60,407 B; **0 B delta** |
| Maximum retained capacity in 180-case corpus | 13,456 B |
| Maximum tracked vector payload during construction | 11,496 B |
| Instrumented additional ordinary C++ payload peak, 10-case probe | 14,984 B |
| Fixed 64 KB LVGL pool, minimum sampled free | 9,336 B |
| Fixed-pool Menu restoration | 19,160 B free; 67 objects, 3 timers, zero retained handles |

Vectors request PSRAM explicitly on demand, with no arena and no internal-heap
fallback. The existing global C++ allocator prefers PSRAM for strings/Giac
objects and may fall back to 8-bit heap. Ordinary C++ payload and vector payload
are separately instrumented; their maxima are not a simultaneous total. Direct
C allocations, allocator metadata, fragmentation and actual fallback placement
are not measured by those host counters. No zero-heap claim is made.

All ten allocation probes restore steady post-destruction C++/vector payload
over 30 repetitions. Some Giac context scratch persists after a first call
(up to 1,536 B in the unsupported probe), then remains bounded; a separate reset
control releases it. Production does not reset context to obtain that result.
Instrumented warm host medians were approximately 0.04–6.55 ms, with 8.15 ms
maximum in that probe; these are not ESP32 latency measurements.

With allocation failure kept armed through recovery, all 96 injected failure
sites return Partial, release tracked vectors and permit a subsequent Complete
explanation. The preserved pre-fix control lets all 96 failures escape; the
nonallocating recovery fix is therefore covered by a detecting test.

Actual Xtensa own frames include app solve 480 B, Steps open 80 B, draw 528 B,
engine explanation 544 B, single planner 1,168 B, system planner 736 B and checker
512 B. Recursive source/domain traversal is bounded. These are **individual
frames**, not a full call-chain/high-water measurement; Giac callees and the
physical task stack remain unmeasured. See the independent
[resource review](TUTOR_RESOURCE_REVIEW.md) and its raw section/stack/allocation
evidence. Hardware was not flashed.

## Replay, mutation and regression evidence

- [180-case seeded/acceptance replay corpus](../out/tutor-engine-01/seeded-final/replay.jsonl)
  and [inputs with seed 20260910](../out/tutor-engine-01/seeded-final/inputs.json):
  supported traces independently replayed, exact family roots/tuples, conditions,
  no-ops, grouping, determinism and honest extension refusals checked.
- [Independent challenge corpus](../tests/host/tutor_probe_challenge.json): kept
  separate from planner selection; [mathematical review](../out/tutor-engine-01/review-math/mathematical-review.md)
  includes the actual traces, original-equation checks and mutation evidence.
- [Mutation results](../out/tutor-engine-01/review-math/mutation-results.json)
  cover wrong balancing sign/operand, division losing zero, missing quadratic
  branch, removed exclusion, changed row operation, stale epoch and false
  verification flags. No mutation was accepted as verified; unsupported proof
  syntax may return UNKNOWN. Of 193 mutations, 192 are REJECTED and one is
  UNKNOWN. All eight required categories are rejected.
- [Teaching review](TUTOR_TEACHING_REVIEW.md): 43/43 independently replayed
  traces and 526/526 wording mutations rejected; all four locales preserve the
  mathematical graph. Actual fixed-pool screenshots were inspected separately.
- [Real Steps UI](../out/tutor-engine-01/ui/results.json): 22/22 family flows,
  every primitive formula, compact/detail, four locales, cache reuse and HOME.
  [320×240 contact sheet](../out/tutor-engine-01/ui/contact.png).
- [Equations rebuild regression](../out/tutor-engine-01/equations-final/results.json):
  91/91, including physical semantic keys and 50 lifecycle cycles. The only old
  tutor expectation changes replace logger “agreed” with checked “complete”, or
  unsupported for families without a new method; answer assertions remain.
- [Regression ledger](../out/tutor-engine-01/regression-ledger.json) records the
  final Giac/cross-app, Calculus/F7, STIX, Grapher, existing app replay, native,
  production and browser/WASM results, plus golden drift and scope limits.

Reproduce from a fresh checkout with `bash scripts/build-giac-host-harness.sh`
(sets up the ordinary host suites and tutor CLI), or
`python scripts/build-tutor-host.py` (bootstraps that closure if absent). Then run
`python scripts/test-tutor-engine.py` and the focused native UI scripts. Probe
scripts accept explicit object-cache paths. On Windows use Git Bash and the
existing MinGW toolchain; browser builds use an explicitly fingerprinted ASCII
source snapshot, including the initial dirty source overlay, because of current
Windows toolchain path limitations. No golden was promoted or mask expanded.

The [changed-file manifest](../out/tutor-engine-01/changed-files.json) compares
the final working snapshot with the **immediate uncommitted baseline**, while
recording pre-existing changes separately.

## Next nonlinear phase

Add methods through the same rule/checker boundary: radical isolation with
sign prerequisites and implication/candidate rejection; absolute-value cases
with exhaustive signed branches; logarithmic positivity and base conditions;
exponential injectivity; then trig interval/periodic families and numerical
approximations with explicitly weaker proof status. Extend typed conditions and
branch limits only alongside verifier theorems, mutation tests and resource
measurements. Full Spanish/French content and a shifted-square teaching strategy
are separate follow-up work. Passing these finite corpora is evidence for this
candidate, not proof that all equations or all symbolic paths are correct.
