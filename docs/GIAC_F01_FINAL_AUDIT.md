# GIAC-F01 Final Mathematical-Authority Audit

Audit date: 2026-07-20  
Repository: NumOS  
Audit base: `main` at `354fe16`

## 1. Baseline

The worktree was clean at capture time. `git diff --stat` was empty.

| Item | Baseline |
|---|---:|
| Emulator executable | 14,630,274 bytes |
| Normal ELF / BIN | 92,975,696 / 5,327,088 bytes |
| Validate ELF / BIN | 93,413,368 / 5,352,928 bytes |
| Giacdiag ELF / BIN | 93,118,704 / 5,331,248 bytes |
| `.dram0.data` | 22,104 bytes |
| `.dram0.bss` | 94,688 bytes |
| IRAM vectors / text | 1,027 / 59,039 bytes |
| Normal `.flash.text` / `.flash.rodata` | 3,744,103 / 1,500,444 bytes |
| App partition | 6,553,600 bytes |
| Normal free app margin | 1,226,512 bytes (18.72%) |
| Emulator scripts | 148 |
| Calculation / Grapher / Equations / Calculus scripts | 33 / 60 / 20 / 17 |
| Neo / angle / rollback scripts | 1 / 7 / 2 |
| Blessed goldens / masks | 18 / 18 |
| Candidate definitions | 40 |

Baseline PlatformIO environments (11):

`esp32s3_n16r8`, `esp32s3_n16r8_validate`,
`esp32s3_n16r8_giacdiag`, `esp32s3_n16r8_giacfeas_baseline`,
`esp32s3_n16r8_validate_overlay`, `esp32s3_n16r8_validate_sup1`,
`esp32s3_n16r8_validate_usbcdc`, `emulator_pc`,
`emulator_pc_equations_legacy`, `emulator_pc_calculus_legacy`, and
`emulator_pc_neo_smoke`.

## 2. Complete authority matrix

“UI” means that the operation can affect a normal user-visible result or
graph. “Tutor/probe” never means primary-result authority.

| Caller and operation | Reachability | Normal authority | Explicit rollback/native authority | Tutor/probe role | Guard | UI | Coverage |
|---|---|---|---|---|---|---|---|
| `CalculationApp::evaluateExpression` ENTER | Normal firmware/emulator | `CalculationEngine` → `GiacEngine` | `MathEvaluator` | None for result | `NUMOS_CALC_LEGACY_ENGINE` | Yes | 135-case host suite; 33 Calculation scripts; rollback script |
| Calculation result-tree/ExactVal/MathAST presentation | Normal | Existing Giac result only; no re-evaluation | Legacy display consumes legacy `ExactVal` | Presentation only | Same compile branch | Yes | Serializer/structured fixtures; 18 masked goldens |
| Calculation educational steps | Setting-dependent after successful Giac result | Giac remains displayed answer | Same custom step generator | `ASTFlattener` + `SymSimplify` produce optional steps only | `setting_edu_steps && ev.ok()` | Steps only; never replaces result/history | CAS suites and Calculation regressions |
| `GraphModel::preCacheRPN` classifier | Every committed graph slot | RPN parser/evaluator is a structural acceptance probe | Same probe | Classifier/dry-run only | Always | Valid/invalid state, not sample value | Grapher classifier scripts |
| `GraphModel::evalAt` explicit `y=f(x)` | Normal | Retained Giac 1-D handle | RPN evaluator | None | `NUMOS_GRAPHER_LEGACY_ENGINE` | Yes | Giac retained host sweeps; Grapher scripts; rollback script |
| `GraphModel::evalImplicit` residual/inequality | Normal | Retained Giac 2-D handle | RPN evaluator | None | Same macro | Yes | 2-D host sweeps and Grapher implicit/inequality scripts |
| `GraphModel::evalAtY` explicit `x=f(y)` | Normal | Retained Giac 1-D handle | RPN evaluator | None | Same macro | Yes | `grapher_giac_xfy_implicit_ineq` and graph corpus |
| Grapher POI, trace, table, shading and drawing consumers | Normal | Values returned by the three Giac-backed model entry points | Values returned by rollback entry points | No independent solver | Model compile guard | Yes | Full Grapher/GRBUG corpus |
| `EquationsApp::solveEquations`, one equation | Normal | `GiacEngine::solveStructured` | `OmniSolver` | Native candidate after Giac only | `NUMOS_EQUATIONS_LEGACY_ENGINE` | Yes | 135-case host suite; 19 normal scripts; rollback |
| `EquationsApp::solveEquations`, 2×2/3×3 | Normal | `GiacEngine::solveSystemStructured` | `SystemSolver` | Native candidate after Giac only | Same macro | Yes | Host systems matrix and emulator systems scripts |
| Equations steps/tutor | Successful ordinary Giac solution only | `_giacResult` remains immutable answer | Native result is answer in rollback | Custom candidate must normalize equal to Giac; otherwise steps unavailable | Runtime `tutorCandidateAgrees()` | Steps only | Agreement/disagreement and tutor-status assertions |
| `CalculusApp::computeResult`, derivative/integral | Normal | `GiacEngine::evaluateCalculusStructured` | `ASTFlattener` + `SymDiff`/`SymIntegrate`/`SymSimplify` | Native candidate after Giac | `NUMOS_CALCULUS_LEGACY_ENGINE` | Yes | 26-case host suite; 16 normal scripts; rollback |
| Calculus steps/tutor | Successful Giac result only | `_giacResult` is sole displayed result | Native result is answer in rollback | Giac verifies derivative equality or antiderivative modulo a constant; fail closed | `verifyCalculusTutor` | Steps only | Host agreement/disagreement plus emulator assertions |
| `NeoMathBackend` symbol, constant, binary, negate, function, equation, evaluate | Normal Neo session | `GiacEngine::instance()` | Custom CAS/value operations | None | `_engine == Native` checked before every Native call | Yes | Neo 44-case host suite; app lifecycle smoke; mathdiag |
| Neo simplify/expand/factor | Normal Neo session | Typed Giac transform | Custom CAS transform | None | `_engine == Native` | Yes | Neo host suite and mathdiag |
| Neo differentiate/integrate | Normal Neo session | Typed Giac calculus | `SymDiff`/`SymIntegrate` | None | `_engine == Native` | Yes | Neo host suite and mathdiag |
| Neo solve/system | Normal Neo session | Typed Giac solve | `OmniSolver`/`SystemSolver` | None | `_engine == Native` | Yes | Neo host suite and mathdiag |
| Neo Taylor | Normal Neo session | `GiacEngine::taylorStructured` | Repeated native differentiation | None | `_engine == Native` | Yes | Neo host suite and mathdiag |
| Neo retained plot/sample | Normal Neo session | Retained Giac handle with generation recompilation | Retained native `SymExpr` | None | Plot stores selected engine; evaluation checks it | Yes | Neo host reset test, app smoke, cross-app suite, hardware mathdiag |
| `NeoStdLib` math built-ins | Normal Neo execution | Immediate return through `NeoMathBackend` | Backend selection controls authority | None | Backend contract | Yes | Neo host/app suites |
| Old native tails in `NeoStdLib` after unconditional backend returns | Not reachable | None | None | Historical code | Unconditional return precedes code | No | Static reachability audit; separate cleanup decision |
| `SerialBridge` solve command | Firmware serial input | `solveWithGiac` → `GiacBridge` borrowing engine context | None | None | Serial command routing | Serial UI | Giac host and bridge audit |
| `MathEvaluator::evaluateWithGiac` compatibility helper | Reachable only if called | `solveWithGiac` borrowing engine context | None | Compatibility helper | Call-site dependent | Potential | Static audit; Giac host suite |
| `TutorApp` | Not registered by `SystemApp` or normal emulator | None | Custom CAS only if separately resurrected | Historical tutor app | Unreachable from launcher | No | Static reachability audit |
| `IntegralApp` | Not registered by `SystemApp` or normal emulator | None | Custom CAS only if separately resurrected | Historical app | Unreachable from launcher | No | Static reachability audit |
| `SystemApp::_equationSolver` / `EquationSolver` | Constructed and angle-synced, never invoked | None | None | Historical member | No solve call | No | Static call-site audit |
| Statistics, Probability, Regression, Sequences, Matrices, Chemistry and other domain apps | Normal where registered | Their bounded domain-specific numeric algorithms | None | Not general symbolic CAS routes | App routing | Yes | Existing app suites; excluded from CAS-authority classification |
| Giac/Math diagnostics | Opt-in firmware only | Public `GiacEngine` API | Explicit Native Neo diagnostic case only | Debug/test | Dedicated build macros | Serial diagnostics, not normal UI | Giacdiag build and physical mathdiag |

Result: **no unresolved normal-build custom-CAS answer path exists**.

## 3. Exact custom-CAS call classification

Every production-tree custom-CAS call belongs to exactly one audit category:

| Category | Exact sites/roles |
|---|---|
| Consistency-gated tutor | Calculation educational-step generation is gated on a successful authoritative result and cannot write the result/history; Equations candidates require `tutorCandidateAgrees()`; Calculus candidates require `verifyCalculusTutor()` and are discarded on disagreement. |
| Compile-time rollback | Calculation `_evaluator.evaluate`; Grapher RPN sampling/full-parse fallback; Equations `solveOmni`/`solveSystem`; Calculus primary native derivative/integral branch. |
| Explicit Neo Native selection | Every custom call in `NeoMathBackend` at the Native branches for values, transforms, calculus, solving, Taylor, plot compile/evaluate and numeric conversion. |
| Grapher classifier/numeric probe | `GraphModel::dryRunStructural` and the RPN classifier evaluation used during commit in both builds. It cannot supply a normal sample. |
| Debug/test | CAS host suites, host parity/probe code, emulator assertion hooks, Giac diagnostics, mathdiag Native counter-isolation case. |
| Dead/unreachable | Native code tails in `NeoStdLib` after unconditional backend returns; unregistered `TutorApp`; unregistered `IntegralApp`; unused `SystemApp::_equationSolver` solve machinery. |
| Unresolved normal authority | **None.** |

The Calculation step lane is not a primary-answer authority: `_lastStatus`,
`_lastKind`, `_exactText`, `_structuredResult`, history and Ans are all populated
from the Giac evaluation before optional step generation. The step lane has no
write path back to those fields.

## 4. Neo Native guard proof

`NeoMathBackend` initializes `_engine` to `Giac`; `resetSession()` also restores
Giac. `math_engine()` only reports the selection. Only
`math_engine("native")` and `math_engine("giac")` mutate it, and any other
argument is an error. There is no Auto enum and no fallback branch.

Repository-wide inspection found explicit `_engine == NeoMathEngine::Native`
guards for symbol/constant construction, binary/negate/function/equation,
evaluate, simplify, expand, factor, differentiate, integrate, solve, system,
Taylor, plot compile/evaluate and numeric conversion. Each Giac continuation
uses `GiacEngine::instance()`. The host suite proves invalid `"auto"` is an
error; physical mathdiag proves a Native derivative increments no Giac-side Neo
counter and switching back resumes Giac.

## 5. One-context proof

- Exactly one `new giac::context` exists: `GiacEngine.cpp`.
- `GiacEngine::State::ctx` is the sole owning pointer.
- `GiacEngineInternal::sharedContext()` exposes a non-owning pointer only to the
  legacy `GiacBridge` translation unit.
- `GiacBridge` constructs no context.
- Applications, Neo, diagnostics and tests construct no context.
- Neo calls `GiacEngine::instance()` exclusively.
- `reset()` increments the generation and destroys/rebuilds the sole context.
- `CompiledExpression::valid()` compares its stamp with the engine generation.
- Grapher `ensureGiacFresh` and Neo `evaluatePlot` recompile retained source
  exactly once after a stale rejection.
- `ScopedRadianMode` saves the prior Giac angle mode and restores it on scope
  exit, including failure paths. DEG differentiation followed by `sin(30)`
  returning `1/2` proves restoration.
- The 14-case cross-app host sequence covers Calculation assignment, retained
  Grapher, Neo local/free symbols, DEG Calculus, RAD Equations, Neo plotting,
  reset, stale rejection/recompile, post-reset Calculation and session-local
  Native selection.

No second context was introduced for isolation.

## 6. Shared infrastructure consolidated

`EngineContracts.h` now owns only genuinely identical contracts:

- 2,000-byte controlled source limit;
- 400-node and 40-depth structural limits;
- plain ASCII identifier lexical validation, with caller-supplied maximum
  length.

Calculation serialization, Giac typed operations and Neo conversion use those
contracts. App-specific reserved-name policy remains in `GiacEngine`; Neo’s
63-character runtime identifier limit remains explicit; Calculation’s
result-to-MathAST depth remains separate; Grapher retains its grammar and
classifier; Neo’s engine-neutral value tree remains distinct from MathAST.
No public Giac type, second grammar, UI rewrite or behavior merge was added.

## 7. Serializer/result parity

Exact locked fixtures remained unchanged:

| Fixture | Before | After |
|---|---|---|
| VPAM `2 + 2` serializer | `2+2` | `2+2` |
| `1/2 + 1/3` | exact `5/6` | exact `5/6` |
| unary `-(3^2)` | `-9` | `-9` |
| cube root of 8 | `8^(1/3)` structured | same |
| `log_2(8)` | `3` | `3` |
| `sqrt(2)` | exact radical | same |
| `sin(pi/6)` | exact `1/2` | same |
| `2^100` | structured 31-digit integer | same |
| `x-2*x` | structured free-symbol expression | same |
| `1/0` / `0/0` | text `oo` / typed Undefined | same |
| `sin(pi/4)` result tree | exact `sqrt(2)/2` | same |
| solve list | honest list fallback | same |
| factor/diff/integrate trees | structured MathAST | same |

The fresh host harness passed 135/135 Giac fixtures, 26/26 Calculus fixtures,
44/44 Neo fixtures and 14/14 cross-app fixtures.

## 8. Rollback environments

Four independent opt-in emulator environments now exist:

- `emulator_pc_calculation_legacy` defines only
  `NUMOS_CALC_LEGACY_ENGINE`;
- `emulator_pc_grapher_legacy` defines only
  `NUMOS_GRAPHER_LEGACY_ENGINE`;
- `emulator_pc_equations_legacy` defines only
  `NUMOS_EQUATIONS_LEGACY_ENGINE`;
- `emulator_pc_calculus_legacy` defines only
  `NUMOS_CALCULUS_LEGACY_ENGINE`.

Their focused scripts assert the expected engine and result/sample. All four
build and pass. They write no repository-root generated artifacts.

## 9. Mathdiag and physical soak

`esp32s3_n16r8_mathdiag` defines only
`NUMOS_MATH_ENGINE_DIAGNOSTICS`. Normal firmware does not call or compile the
runner body. Each case prints stable pipe-delimited fields for status, cold and
warm time, internal/PSRAM before/minimum/after, largest blocks, stack
high-water, generation, result kind and retained compile count.

The deterministic suite has 38 cases covering all requested Calculation,
Grapher, Equations, Calculus, Neo and cross-app operations. The final case runs
100 mixed alternating operations with four resets and five retained compiles.
It detects repeated monotonic heap loss, stale-handle survival, angle/scoping
failure and engine counter cross-contamination without adding threads or
changing watchdog policy.

Six physical cold boots were observed on an ESP32-S3 N16R8 via COM6. The first
three retained full records; the second three produced the aggregate below:

| Physical metric | Min / median / max |
|---|---:|
| Complete cases per boot | 38 / 38 / 38 |
| Failures | 0 / 0 / 0 |
| Boot diagnostic duration | 7.336 / 7.344 / 7.362 s |
| 100-cycle mixed soak | 305.436 / 305.502 / 305.583 ms |
| Final internal free heap | 214,608 / 214,608 / 214,608 bytes |
| Final PSRAM free | 8,260,071 / 8,260,503 / 8,260,503 bytes |
| Largest internal block | 163,828 / 163,828 / 163,828 bytes |
| Largest PSRAM block | 8,257,524 / 8,257,524 / 8,257,524 bytes |
| Stack high-water remainder | 45,568 / 45,568 / 45,568 bytes |
| Final Giac generation | 11 / 11 / 11 |

Representative cold/warm-average microseconds (min/median/max):

| Case | Cold | Warm average |
|---|---:|---:|
| Calculation fraction sum | 20,427 / 20,428 / 20,486 | 19,190 / 19,193 / 19,193 |
| Grapher retained `sin(x)` | 1,795 / 1,810 / 1,812 | 11 / 11 / 11 |
| Grapher 1,000-point sweep | 18,812 / 18,812 / 18,813 | 17,079 / 17,079 / 17,084 |
| Grapher 1,024-point residual grid | 1,486,416 / 1,486,469 / 1,499,051 | 1,484,763 / 1,484,767 / 1,498,524 |
| Equation exact quadratic | 23,066 / 23,084 / 23,084 | 23,029 / 23,032 / 23,043 |
| Calculus `diff(sin(x)^2)` | 4,859 / 4,878 / 4,883 | 4,798 / 4,801 / 4,810 |
| Neo solve | 64,758 / 64,825 / 64,885 | 64,819 / 64,828 / 64,868 |
| Neo retained plot | 153 / 154 / 157 | 8 / 8 / 8 |

No monotonic internal-heap or PSRAM loss, stale handle survival, angle-mode
leak, context-variable leak, Neo-local leak, graph-zero substitution, tutor
answer replacement or backend counter crossover was detected.

## 10. ESP32-aware memory accounting

Final linker/BIN accounting:

| Environment | `.dram0.data` | `.dram0.bss` | IRAM vectors+text | `.flash.text` | `.flash.rodata` | BIN | App margin |
|---|---:|---:|---:|---:|---:|---:|---:|
| Normal | 22,104 | 94,688 | 60,066 | 3,743,995 | 1,500,448 | 5,326,976 | 1,226,624 (18.72%) |
| Validate | 22,104 | 94,688 | 60,066 | 3,761,867 | 1,508,432 | 5,352,832 | 1,200,768 (18.32%) |
| Giacdiag | 22,104 | 94,688 | 60,066 | 3,746,439 | 1,502,188 | 5,331,168 | 1,222,432 (18.65%) |
| Mathdiag | 22,104 | 94,688 | 60,066 | 3,778,287 | 1,505,672 | 5,366,496 | 1,187,104 (18.11%) |

Static mapped PSRAM is zero. RTC data/BSS is zero; `.rtc_noinit` is 16 bytes.
Runtime heap/PSRAM/largest blocks/stack are reported above from physical
hardware.

All four ELFs contain a 3,801,088-byte `.flash_rodata_dummy` NOLOAD region.
This is DROM virtual-address alignment between address spaces, not physical
BSS, RAM, PSRAM, RTC memory or firmware payload. The GIAC-N01 64 KiB ELF
“BSS increase” remains explained as a DROM boundary shift within this dummy
region. GNU `size` therefore reports a misleading multi-megabyte `bss`; the
real `.dram0.bss` is 94,688 bytes.

## 11. Regression and visual results

- Giac host: 135/135.
- Calculus host: 26/26.
- Neo backend host: 44/44.
- Cross-app/context host: 14/14.
- CAS host: 8/8 suites, zero unexpected failures.
- Normal Calculation emulator: 33/33.
- Normal Grapher emulator: 59/59 plus the intentional negative-control script
  correctly exits 4.
- Normal Equations emulator: 19/19.
- Normal Calculus emulator: 16/16.
- Angle-mode scripts: 7/7.
- Neo app lifecycle smoke: pass.
- Four rollback focused scripts: 4/4.
- Candidate generation: 40/40.
- Official masked golden comparisons: 18/18 unchanged.
- KeyCode self-test, repository scan and compiled 19-check host test: pass.
- Normal, validate, giacdiag and mathdiag firmware: build pass.

No golden or mask was edited or re-blessed.

## 12. Final size impact

| Artifact | Baseline | Final | Delta |
|---|---:|---:|---:|
| Emulator executable | 14,630,274 | 14,630,366 | +92 |
| Normal ELF | 92,975,696 | 92,975,748 | +52 |
| Normal BIN | 5,327,088 | 5,326,976 | -112 |
| Validate ELF | 93,413,368 | 93,413,472 | +104 |
| Validate BIN | 5,352,928 | 5,352,832 | -96 |
| Giacdiag ELF | 93,118,704 | 93,118,804 | +100 |
| Giacdiag BIN | 5,331,248 | 5,331,168 | -80 |
| Mathdiag ELF/BIN | New | 93,576,624 / 5,366,496 | Opt-in only |

Normal static DRAM, BSS and IRAM are unchanged. Normal firmware keeps an
18.72% app-partition margin.

## 13. Remaining risks

1. Calculation educational steps are isolated from the answer but are gated by
   successful Giac evaluation rather than a separate symbolic equivalence
   check for every intermediate step. This is retained behavior, not authority.
2. The 32×32 Giac residual grid costs about 1.49 seconds on hardware; it is a
   diagnostic throughput fact, not a frame-time claim.
3. Neo’s explicit Native backend intentionally remains available and therefore
   keeps its custom-CAS dependency.
4. Dead `NeoStdLib` tails and unregistered historical apps should not be
   removed as part of rollback deletion without a separate reachability/product
   decision.
5. Hardware evidence is strong but rollback removal is explicitly deferred to
   GIAC-F02 review; this audit deletes nothing.

