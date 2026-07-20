/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * GiacEngine.h — GIAC-A01 production seam over the in-process Giac engine.
 *
 * This is the ONE owner of the giac::context. Future Calculation and Grapher
 * migrations call this API; nothing outside src/math/giac/ may touch Giac
 * types directly (this header exposes none). The legacy GiacBridge UART path
 * borrows the same context through GiacEngineInternal.h during the
 * transition, so there is exactly one symbolic state in the system.
 *
 * Contract:
 *  - Single-threaded, NON-reentrant. Calls made while another engine call is
 *    on the stack are rejected with Status::Unsupported (never queued).
 *  - begin() is idempotent; every public entry point self-initializes.
 *  - reset() destroys and rebuilds the context: all Giac-side variable
 *    assignments (a:=5) are forgotten, and every CompiledExpression from
 *    before the reset becomes invalid (evaluateNumeric returns false — no
 *    silent stale-context evaluation).
 *  - Angle mode: synchronized from AngleModeRuntime (vpam::g_angleMode) at
 *    every evaluate/simplify/compileNumeric/evaluateNumeric entry. The legacy
 *    UART path pins radians instead (its historical behavior).
 *  - Semantics are Giac's, reported honestly: 1/0 -> "infinity"/oo is Ok
 *    (not an error); plain evaluate() does NOT collect like terms
 *    (x-2*x stays x-2*x — use simplify() for -x); undef and parser
 *    diagnostics come back as non-Ok statuses, never as ordinary values.
 *  - Variable policy: Giac-side assignments persist in the context until
 *    reset(). CalculationEngine mirrors supported VariableManager slots;
 *    typed Calculus requests use free x/y variables and do not mutate Ans.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace numos {

enum class MathEngineStatus : uint8_t {
    Ok,
    Undefined,        // evaluation produced Giac undef without a diagnosis
    ParseError,       // input never became a valid expression
    EvaluationError,  // Giac raised/diagnosed an error during eval
    Unsupported,      // rejected by the engine contract (e.g. reentrancy)
    OutOfMemory
};

struct MathEngineResult {
    MathEngineStatus status = MathEngineStatus::Unsupported;
    std::string exactText;        // printed result (engine print settings)
    std::string approximateText;  // evalf of the result when it adds info
    std::string diagnostic;       // parser/engine messages, never in exactText
    bool ok() const { return status == MathEngineStatus::Ok; }
};

/**
 * GIAC-B01 — engine-owned structural view of one evaluation result.
 *
 * The tree describes the SHAPE of the Giac result (integer, fraction, sum,
 * power, ...) without exposing giac::gen. Consumers (the Calculation result
 * bridge) turn it into MathAST for Natural Display; anything the tree cannot
 * describe is marked Unsupported so the caller falls back to the printed
 * exactText — never to a re-evaluation.
 *
 * Numeric literals carry their exact printed digits in `text` (big integers
 * included); Symbol/Function carry the identifier/function name.
 */
enum class EngineNodeKind : uint8_t {
    Integer,           // text = decimal digits, optional leading '-'
    Decimal,           // text = printed double literal
    Rational,          // children = { numerator, denominator }
    Symbol,            // text = identifier name
    Add,               // n-ary sum, children in printed order
    Neg,               // unary minus, 1 child
    Mul,               // n-ary product
    Inv,               // reciprocal, 1 child
    Pow,               // children = { base, exponent }
    Sqrt,              // 1 child
    Root,              // children = { radicand, integer degree } (x^(1/n))
    Function,          // text = function name, children = arguments
    Pi,                // the constant pi
    EulerE,            // the constant e (Giac exp(1))
    ImagUnit,          // the imaginary unit
    PlusInfinity,
    MinusInfinity,
    UnsignedInfinity,
    Equation,          // children = { lhs, rhs }
    List,              // children = elements
    Complex,           // children = { re, im }
    Unsupported        // shape outside this contract (text = printed form)
};

struct EngineResultNode {
    EngineNodeKind kind = EngineNodeKind::Unsupported;
    std::string text;
    std::vector<EngineResultNode> children;
};

struct StructuredEngineResult {
    MathEngineResult base;
    bool hasTree = false;      // tree only meaningful when base.ok() && hasTree
    EngineResultNode tree;
};

enum class AlgebraTransform : uint8_t {
    Simplify,
    Expand,
    Factor
};

struct TaylorRequest {
    std::string expression;
    std::string variable = "x";
    std::string center = "0";
    int order = 0;
};

/**
 * GIAC-E01 calculus boundary.
 *
 * CalculusApp currently exposes exactly these two operations. The authored
 * expression is serialized by CalculationEngine's controlled MathAST
 * serializer; Giac syntax is never assembled in the app and no Giac type
 * crosses this header.
 */
enum class CalculusOperation : uint8_t {
    Differentiate,
    IntegrateIndefinite
};

struct CalculusRequest {
    CalculusOperation operation = CalculusOperation::Differentiate;
    std::string expression;
    std::string variable = "x";
};

struct StructuredCalculusResult {
    MathEngineStatus status = MathEngineStatus::Unsupported;
    bool hasTree = false;
    EngineResultNode tree;
    std::string exactText;
    std::string approximateText;
    std::string diagnostic;
    bool unevaluated = false;  // valid Giac diff/integrate form, not a failure

    bool ok() const { return status == MathEngineStatus::Ok; }
};

struct CalculusTutorVerification {
    bool agreed = false;
    std::string diagnostic;
};

/**
 * GIAC-D01 equation-solving boundary.
 *
 * Equation sides are already-authored, controlled Giac expressions. Keeping
 * lhs/rhs separate prevents an app from manufacturing solve syntax or parsing
 * rendered pixels. All Giac-owned types remain private to GiacEngine.cpp.
 */
enum class SolveDomainPolicy : uint8_t {
    RealOnly,
    RealAndComplex
};

enum class SolutionSetKind : uint8_t {
    Solutions,
    NoSolution,
    AllValues,
    Unsupported
};

struct SolveEquation {
    std::string lhs;
    std::string rhs;
};

struct StructuredSolution {
    std::string variable;
    EngineResultNode exactValue;
    std::string exactText;
    std::string approximateText;
    bool hasApproximateReal = false;
    double approximateReal = 0.0;
};

struct StructuredSolutionGroup {
    // Ordered exactly as the request's solveVariables.
    std::vector<StructuredSolution> values;
};

struct StructuredSolveResult {
    MathEngineStatus status = MathEngineStatus::Unsupported;
    SolutionSetKind setKind = SolutionSetKind::Unsupported;
    std::vector<StructuredSolutionGroup> groups;
    std::string diagnostic;
    std::string rawExactText;

    bool ok() const { return status == MathEngineStatus::Ok; }
};

/**
 * Opaque handle to a parse-once/evaluate-many numeric expression
 * (Grapher-shaped). Movable, non-copyable; owns its Giac-side state
 * privately. Invalidated by GiacEngine::reset() (generation check).
 */
class CompiledExpression {
public:
    CompiledExpression();                    // empty/invalid handle
    ~CompiledExpression();
    CompiledExpression(CompiledExpression&& other) noexcept;
    CompiledExpression& operator=(CompiledExpression&& other) noexcept;
    CompiledExpression(const CompiledExpression&) = delete;
    CompiledExpression& operator=(const CompiledExpression&) = delete;

    /// Parsed successfully AND not orphaned by an engine reset.
    bool valid() const;
    /// Parse/compile diagnostic when !valid() at compile time.
    const std::string& diagnostic() const;

private:
    friend class GiacEngine;
    struct Impl;
    Impl* _impl;  // null for the empty handle
};

class GiacEngine {
public:
    /// The single engine (and single giac::context) in the system.
    static GiacEngine& instance();

    /// Idempotent. Returns false only if the context could not be created.
    bool begin();

    /// Destroy + rebuild the context. See header contract.
    void reset();

    /// Plain Giac eval of one expression. No implicit simplification.
    MathEngineResult evaluate(const char* expression);

    /// Explicit simplify mode (giac _simplify on the parsed input).
    MathEngineResult simplify(const char* expression);

    /**
     * GIAC-B01: plain evaluate() plus the structural result view. Same
     * semantics as evaluate(); additionally fills `tree` describing the
     * result shape (see EngineNodeKind). hasTree is false when the shape
     * walk hit an internal limit — the textual result still stands.
     */
    StructuredEngineResult evaluateStructured(const char* expression);

    /**
     * Apply one typed algebraic transform to an already-authored expression.
     * The operation is selected by enum, never manufactured as command text.
     */
    StructuredEngineResult transformStructured(
        AlgebraTransform operation, const char* expression);

    /**
     * Return exact Taylor coefficients [c0, ..., c_order] about `center`.
     * Uses Giac's series/Taylor implementation directly; the big-O marker is
     * intentionally omitted because NeoLanguage's established result is the
     * truncated coefficient list.
     */
    StructuredEngineResult taylorStructured(const TaylorRequest& request);

    /**
     * GIAC-E01: execute one typed CalculusApp operation. Input length/tree and
     * result-tree walks are bounded. A valid unevaluated Giac operation keeps
     * status=Ok and sets unevaluated=true so the app can use exact text
     * fallback without invoking another answer engine.
     */
    StructuredCalculusResult evaluateCalculusStructured(
        const CalculusRequest& request);

    /**
     * Independently verify a native tutor's final expression against Giac.
     * Differentiation compares normalized exact expressions. Indefinite
     * integration differentiates (native - Giac), accepting additive
     * constants without relying on formatted strings or doubles.
     */
    CalculusTutorVerification verifyCalculusTutor(
        const CalculusRequest& request,
        const std::string& nativeResultExpression);

    /**
     * GIAC-D01: solve one equation in one Giac call and adapt the returned
     * list/equality shapes into an engine-owned structural solution set.
     * Duplicate roots are removed by exact Giac equality; remaining roots are
     * ordered deterministically. Unsupported value presentation retains
     * status=Ok and rawExactText so the app can show an honest text fallback.
     */
    StructuredSolveResult solveStructured(
        const SolveEquation& equation,
        const std::string& variable,
        SolveDomainPolicy policy = SolveDomainPolicy::RealOnly);

    /**
     * GIAC-D01: solve a simultaneous system as one Giac operation. Solution
     * tuple members are reordered to solveVariables, never map order.
     */
    StructuredSolveResult solveSystemStructured(
        const std::vector<SolveEquation>& equations,
        const std::vector<std::string>& solveVariables,
        SolveDomainPolicy policy = SolveDomainPolicy::RealOnly);

#ifdef NUMOS_GIAC_HOST_HARNESS
    // Host-only structural seam: validates Giac equality/nested-list shapes
    // without exposing giac::gen outside GiacEngine.cpp.
    bool debugStructuredSolveAdapterForms();
#endif

    /**
     * GIAC-B01: assign `name := (valueExpression)` in the engine context.
     * `name` must be a plain identifier ([A-Za-z_][A-Za-z0-9_]*). Used by
     * the Calculation adapter to mirror NumOS variables (A-F, Ans, PreAns)
     * before each evaluation; assignments live until reset().
     */
    MathEngineResult assign(const char* name, const char* valueExpression);

    /**
     * Parse + evaluate once for repeated numeric sampling in `variable`
     * (default "x"). Check .valid() on the returned handle.
     *
     * GIAC-C01 `strictVariables`: when true, the handle is compiled for the
     * Grapher contract — the parsed tree is retained WITHOUT the up-front
     * context eval() (so context assignments like x:=5 or DEG-folded
     * constants can never be captured into the handle), and any free
     * identifier other than `variable` (pi is a constant, not a variable)
     * fails compilation with the offending name in diagnostic(). The
     * default (false) keeps the historical GIAC-A01 behavior: one context
     * eval() up front, unbound symbols tolerated at compile time and
     * rejected per-sample.
     */
    CompiledExpression compileNumeric(const char* expression,
                                      const char* variable = "x",
                                      bool strictVariables = false);

    /**
     * GIAC-C01: parse once for repeated 2-variable sampling (implicit
     * relations / inequality residuals). Always strict: the parsed tree is
     * retained un-eval'd and only `variableA`/`variableB` may appear free.
     */
    CompiledExpression compileNumeric2D(const char* expression,
                                        const char* variableA,
                                        const char* variableB);

    /**
     * Numeric value at x without any string parsing. Returns true and sets
     * `out` for real samples, including signed infinities at poles
     * (discontinuities are preserved as non-finite values). Returns false
     * (out = NaN) for invalid/stale handles and non-real/symbolic residues.
     */
    bool evaluateNumeric(const CompiledExpression& expr, double x,
                         double& out);

    /**
     * GIAC-C01: numeric value at (a, b) for a 2-variable handle. Both
     * variables are substituted simultaneously (never assigned into the
     * shared context). Same result contract as evaluateNumeric().
     */
    bool evaluateNumeric2D(const CompiledExpression& expr, double a, double b,
                           double& out);

    GiacEngine(const GiacEngine&) = delete;
    GiacEngine& operator=(const GiacEngine&) = delete;

private:
    GiacEngine() = default;
    struct State;
    State* _state = nullptr;   // created by begin()
    bool _inCall = false;      // reentrancy rejection
    uint32_t _generation = 0;  // bumped by reset(); stamps compiled handles
};

} // namespace numos
