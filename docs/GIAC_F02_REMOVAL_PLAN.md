# GIAC-F02 File-Level Removal Plan

GIAC-F02 is a later, evidence-gated deletion change. GIAC-F01 does **not**
execute any item below. Before starting, retain the GIAC-F01 hardware logs,
repeat normal firmware and emulator baselines, and obtain explicit approval
that rollback support may be removed.

## 1. Obsolete Calculation primary-result rollback

In `src/apps/CalculationApp.cpp`, remove only the former Calculation
compile-time primary evaluation branch in `evaluateExpression()` and collapse
the normal Giac branch. Remove the matching
conditional wrappers around Giac result/history/display/store behavior.

In `src/apps/CalculationApp.h`, remove `debugCalcEngine()`’s legacy answer and
remove the `_evaluator` member only after a use scan proves no non-rollback
consumer. Do not remove static `MathEvaluator` result-to-AST formatting helpers
still used by presentation code.

Verification: all 33 normal Calculation scripts, serializer fixtures, Ans/STO,
S⇔D behavior and all blessed goldens.

## 2. Obsolete Grapher sampling rollback

In `src/apps/GraphModel.cpp`, remove only the legacy `#else` sampling bodies in
`evalAt`, `evalImplicit` and `evalAtY`, plus the legacy full-parse sample
fallback. Collapse `ensureGiacFresh` and retained-handle compilation into the
single implementation.

In `src/apps/GraphModel.h`, remove only conditional fields/hooks that exist
solely to select sampling authority. Keep RPN token vectors, parser/evaluator
members and classifier state because commit-time classification and structural
dry-run remain product behavior.

In `src/apps/GrapherApp.h/.cpp`, remove only the legacy engine debug return and
conditional sampling assertions. Do not change geometry, trace, table, POI or
shading consumers.

Verification: full Grapher corpus, intentional negative control, all GRBUG
cases, retained-reset tests, 1-D/2-D host sweeps and all Grapher goldens.

## 3. Obsolete Equations primary solver branch

In `src/apps/EquationsApp.cpp`, remove the former Equations compile-time
dispatcher branch that calls `solveOmni()` or `solveSystem()` as the answer.
Remove legacy-only result presentation paths
only where a use graph proves they cannot be reached by the retained tutor.

In `src/apps/EquationsApp.h`, remove legacy-only state/methods, but retain
`generateTutorCandidate`, `tutorCandidateAgrees`, tutor render state and every
custom-CAS field used by the consistency-gated steps lane.

Verification: all single/system/domain/failure/text-fallback normal scripts,
host solve adapter forms, tutor agreement/disagreement and goldens.

## 4. Obsolete Calculus primary-result branch

In `src/apps/CalculusApp.cpp`, remove the former Calculus compile-time primary
branch in `computeResult()` and the legacy `_legacyExactText` answer path.
Do **not** remove
`runNativeTutor`, `computeDerivative`, `computeIntegral`, `ASTFlattener`,
`SymDiff`, `SymIntegrate`, `SymSimplify`, step logging or Giac tutor
verification: those remain normal-build tutor infrastructure.

In `src/apps/CalculusApp.h`, remove only fields/methods proven exclusive to the
legacy primary-result branch.

Verification: all normal Calculus scripts, 26-case host suite, forced tutor
disagreement, angle restoration, unevaluated results and goldens.

## 5. Dedicated rollback environments and scripts

After categories 1–4 are accepted, delete the four dedicated Calculation,
Grapher, Equations, and Calculus rollback sections from `platformio.ini`.

Delete their four focused rollback scripts from `tests/emulator/scripts/`.

Remove any CI/document references in the same change. Keep normal, Neo smoke,
giacdiag and mathdiag environments.

## 6. Legacy serializers/adapters used only by rollback branches

Perform a symbol-level use graph after categories 1–4, then remove only
functions with zero normal/tutor/probe users:

- legacy primary evaluation methods in `src/math/MathEvaluator.cpp/.h`;
- legacy Equations result adapters embedded in
  `src/apps/EquationsApp.cpp/.h`;
- legacy Calculus answer serialization fields embedded in
  `src/apps/CalculusApp.cpp/.h`;
- legacy Grapher full-parse sampling helpers embedded in
  `src/apps/GraphModel.cpp/.h`.

Do not delete `ExactVal`, result-to-AST presentation helpers,
`CalculationEngine::serializeForGiac`, Giac structured adapters, or any
serializer used by tutors. No replacement grammar is permitted.

## 7. Custom-CAS components that must remain for tutors

Retain the custom-CAS sources used by Calculation educational steps,
Equations verified tutors, Calculus verified tutors and their host tests,
including at minimum:

`ASTFlattener`, `SymExpr`, `SymExprArena`, `ConsTable`, `SymSimplify`,
`SymDiff`, `SymIntegrate`, `SymExprToAST`, `SymToAST`, `OmniSolver`,
`SingleSolver`, `SystemSolver`, `SystemTutor`, `TutorTemplates`,
`PedagogicalLogger`, `CASStepLogger`, `RuleEngine`, `RuleBasedTutor`,
`HybridNewton`, `SymPoly`, `SymPolyMulti`, `AlgebraicRules`, `CASNumber`,
`CasMemory`, `CasToVpam` and their required tests.

Deletion of an individual component requires a fresh link/use graph; apparent
implementation similarity is not enough.

## 8. Grapher RPN classifier/probe components that must remain

Retain:

- `src/math/Tokenizer.cpp/.h`;
- `src/math/Parser.cpp/.h`;
- `src/math/Evaluator.cpp/.h`;
- `src/math/VariableContext.cpp/.h`;
- GraphModel RPN caches and `dryRunStructural`;
- classifier identifier/function/relation policies.

They define user-visible accept/reject behavior and are not obsolete merely
because Giac owns numeric sampling.

## 9. Neo Native backend that intentionally remains selectable

Retain `src/apps/NeoMathBackend.cpp/.h`, `NeoValue`, the
`math_engine("native")`/`math_engine("giac")` API in `NeoStdLib`, Native
plot retention and all custom-CAS dependencies reachable through explicit
Native selection. Retain the counter-isolation, switching and lifecycle tests.

GIAC-F02 must not convert Native to fallback, Auto mode or a separate firmware
environment.

## 10. Dead/unreachable apps and historical members

Handle these only in a separate product/reachability cleanup decision:

- `src/apps/IntegralApp.cpp/.h` (not registered);
- `src/apps/TutorApp.cpp/.h` (not registered);
- old native tails in `src/apps/NeoStdLib.cpp` after unconditional backend
  returns;
- `src/SystemApp.h/.cpp`’s constructed but unused `_equationSolver`;
- `src/math/EquationSolver.cpp/.h`;
- historical comments, members or build inclusions associated with them.

They are not rollback branches and must not be swept into GIAC-F02 merely to
reduce code size.

## Required GIAC-F02 gates

1. Confirm real-hardware approval to remove rollback support.
2. Delete one app rollback category at a time with an exact use graph.
3. Run host Giac/Calculus/Neo/cross-app and CAS suites after each category.
4. Run every normal emulator app/angle script and all goldens.
5. Build normal, validate, giacdiag and mathdiag firmware.
6. Repeat at least three physical mathdiag cold boots and the 100-cycle soak.
7. Reject any mathematical, serialization, result-tree, UI pixel, memory or
   context-generation regression.

## GIAC-F02 execution status

The four category gates passed independently. Their compile-time rollback
branches, dedicated environments, focused scripts, and macro definitions were
then removed. Tutor, Grapher classifier/probe, Neo Native, and historical
dead-app policy boundaries remain in force.
