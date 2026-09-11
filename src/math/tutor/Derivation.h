// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "TraceAllocator.h"
#include <array>
#include <cstdint>
#include <string>

namespace numos::tutor {
// Stable, engine-neutral wire model. Mathematical strings use the existing
// Giac serialization grammar; translated text is NEVER parsed as mathematics.
enum class Verdict : uint8_t { Unknown, Verified, Rejected };
enum class Status : uint8_t { Unsupported, Partial, Complete, CheckFailed, ReconciliationFailed };
enum class Relation : uint8_t {
    Equivalent,
    Candidates,
    ExhaustiveSplit,
    Rejection,
    Terminal,
    Approximation
};
enum class Rule : uint8_t {
    Domain,
    SubstituteValues,
    Expand,
    Collect,
    SwapSides,
    AddBoth,
    DivideBoth,
    ClearDenominators,
    Factor,
    SquareRoots,
    QuadraticFormula,
    ZeroProduct,
    RejectCandidate,
    AlreadySolved,
    Identity,
    Contradiction,
    Finish,
    RowSwap,
    RowScale,
    RowAdd,
    SystemFinish
};
enum class Message : uint8_t {
    Domain,
    SubstituteValues,
    Expand,
    Collect,
    SwapSides,
    Add,
    Subtract,
    Divide,
    ClearDenominators,
    Factor,
    SquareRoots,
    QuadraticFormula,
    ZeroProduct,
    RejectCandidate,
    AlreadySolved,
    Identity,
    ConditionalIdentity,
    Contradiction,
    NoRealRoots,
    Finish,
    RowSwap,
    RowScale,
    RowAdd,
    RowSubtract,
    UniqueSystem,
    DependentSystem,
    InconsistentSystem,
    Unsupported,
    Partial,
    CheckFailed,
    ReconciliationFailed,
    RepeatedFactor,
    EmptyAfterExclusions,
    RowAddUnit,
    RowSubtractUnit,
    Alternative,
    ActiveAlternative,
    RejectedAlternative,
    EquationRow,
    FormulaUnavailable,
    AddCollect,
    SubtractCollect,
    AddZeroRight,
    SubtractZeroRight,
    AddDisplayedIsolate,
    SubtractDisplayedIsolate,
    AddDisplayedZeroRight,
    SubtractDisplayedZeroRight,
    AddSystem,
    SubtractSystem,
    AddDisplayedSystem,
    SubtractDisplayedSystem,
    DivideSquare,
    DivideDisplayed,
    DivideDisplayedSquare,
    SquareRootZero,
    RowScaleDisplayed,
    RowAddScaled,
    RowSubtractScaled,
    ViewStep, ViewSummary, ViewStart, ViewSolutions, ViewNoSolution, ViewFamily,
    ViewAlready, ViewIdentity, ViewGuidedHint, ViewSummaryHint,
    ViewBefore, ViewAfter, ViewRestrictions, ViewOperand, ViewUnchangedCase,
    ViewSolutionNumber, ViewSystemTogether, ViewVerified, ViewPartial,
    ViewCoefficients, ViewDiscriminant, ViewFormula, ViewSubstitution,
    ViewMatchCoefficients, ViewComputeDiscriminant, ViewUseFormula, ViewReadRoots,
    ViewStandardForm, ViewCoefficientValues, ViewDefinition, ViewEvaluated,
    ViewStartingSystem, ViewStartEquation, ViewOriginal, ViewCurrent, ViewUnavailable,
    ViewExpand, ViewCollect, ViewBalance, ViewDivide, ViewClear, ViewFactor,
    ViewSquareRoots, ViewCases, ViewCheckCandidate, ViewRowSwap, ViewRowScale,
    ViewRowAdd, ViewSavedValues, ViewDomain, ViewSolveFirst, ViewZeroRight,
    Count
};
enum class ParameterKind : uint8_t { Expression, Variable, Row, Integer };
struct Parameter {
    ParameterKind kind = ParameterKind::Expression;
    std::string value;
};
struct Equation {
    std::string lhs, rhs;
};
struct Binding {
    std::string variable, value;
};
struct Snapshot {
    Vector<Equation> authored;
    Vector<std::string> variables;
    Vector<Binding> bindings;
    Vector<Binding> contextValues; // solve-variable assignments/assumptions, never overwritten
    uint32_t inputEpoch = 0, engineGeneration = 0;
    bool complex = false, degrees = false;
};
struct Path {
    uint8_t equation = 0, side = 0;
    Vector<uint8_t> children;
};
struct Condition {
    std::string nonzero;
    Path source;
};
enum class BranchStatus : uint8_t { Active, Rejected };
struct Branch {
    uint8_t id = 0;
    BranchStatus status = BranchStatus::Active;
    Vector<Equation> equations;
};
enum class Conclusion : uint8_t { None, Finite, Empty, Identity, Family };
struct State {
    Vector<Branch> branches;
    Vector<Condition> conditions;
    Conclusion conclusion = Conclusion::None;
    uint64_t fingerprint = 0;
};
struct Step {
    Rule rule = Rule::Collect;
    Relation relation = Relation::Equivalent;
    uint16_t before = 0, after = 0;
    uint8_t branch = 0, row = 0, otherRow = 0;
    std::string operand;             // exact declared operation, including sign
    Vector<std::string> auxiliaries; // factors or a,b,c,discriminant
    Vector<Path> affected;
    Vector<uint8_t> prerequisites, introduced, discharged;
    Message explanation = Message::Collect;
    Vector<Parameter> parameters;
    uint16_t group = 0;        // contiguous verified primitives, detail retained
    Vector<uint16_t> substeps; // optional references; primitive plans leave empty
    Verdict verification = Verdict::Unknown;
};
struct Limits {
    static constexpr unsigned steps = 48, branches = 2, equations = 3;
    static constexpr unsigned conditions = 8, depth = 20, sourceNodes = 160;
    static constexpr unsigned sourceBytes = 512, expressionBytes = 512;
    // Full replay of dense rational 3x3 elimination needs more calls than a
    // single equation. This bounds BOTH construction checks and final replay.
    static constexpr unsigned retainedBytes = 64 * 1024, symbolicCalls = 4096;
};
struct Metrics {
    uint32_t retainedBytes = 0, symbolicCalls = 0, elapsedMicros = 0, vectorHeapBytes = 0,
             peakVectorHeapBytes = 0;
};
struct Derivation {
    Snapshot input;
    Vector<State> states;
    Vector<Step> steps;
    Status status = Status::Unsupported;
    Verdict validity = Verdict::Unknown, completeness = Verdict::Unknown,
            candidates = Verdict::Unknown, reconciliation = Verdict::Unknown;
    Metrics metrics;
    std::string diagnostic; // developer diagnostic; UI uses semantic status key
};
enum class Locale : uint8_t { English, Spanish, French, Pseudo };
const char *ruleId(Rule rule);
const char *messageKey(Message key);
// Allocation-free English fallback for a zero-parameter recovery message.
const char *messageFallback(Message key);
std::string explain(Message key, const Vector<Parameter> &parameters,
                    Locale locale = Locale::English);
bool validateMessage(Message key, const Vector<Parameter> &parameters);
bool validateCatalogs();
uint64_t fingerprint(const State &state);
uint64_t fingerprint(const Snapshot &snapshot);
size_t retainedBytes(const Derivation &trace);
std::string replayJson(const Derivation &trace, Locale locale = Locale::English);
} // namespace numos::tutor
