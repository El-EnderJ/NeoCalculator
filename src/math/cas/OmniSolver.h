/**
 * OmniSolver.h — Universal single-variable equation solver.
 *
 * Takes a SymExpr-based equation (LHS = RHS, or f(x) = 0) and
 * dispatches to the optimal solving strategy:
 *
 *   1. Polynomial path  — if isPolynomial(), convert to SymPoly and
 *                          delegate to SingleSolver (exact linear/quadratic).
 *   2. Inverse path     — if equation is of the form f(x) = c where f
 *                          is a single invertible function, apply the
 *                          inverse analytically (e.g., ln(x) = 1 → x = e).
 *   3. Hybrid Newton    — fallback for mixed transcendental equations.
 *                          Uses exact symbolic derivative (SymDiff::diff)
 *                          for the Jacobian.
 *
 * Classification enum:
 *   · Polynomial           — pure polynomial (no transcendentals)
 *   · SimpleInverse        — f(x) = c, one invertible layer
 *   · MixedTranscendental  — everything else (Newton-Raphson)
 *
 * Step logging:
 *   All classification decisions and solving steps are recorded in
 *   the CASStepLogger for step-by-step display.
 *
 * Memory:
 *   All SymExpr nodes live in the provided SymExprArena.
 *   Results use ExactVal (for polynomial/inverse) or double (for Newton).
 *
 * Part of: NumOS Pro-CAS — Phase 4 (Omni-Solver)
 */

#pragma once

#include "SymExpr.h"
#include "SymExprArena.h"
#include "SymEquation.h"
#include "CASStepLogger.h"
#include "HybridNewton.h"
#include "SingleSolver.h"
#include "../ExactVal.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// EquationClass — Classification of the equation type
// ════════════════════════════════════════════════════════════════════

enum class EquationClass : uint8_t {
    Polynomial,          ///< Pure polynomial — delegate to SingleSolver
    SimpleInverse,       ///< f(x) = c — apply inverse function
    MixedTranscendental, ///< General transcendental — HybridNewton
};

const char* equationClassName(EquationClass c);

// ════════════════════════════════════════════════════════════════════
// OmniSolution — A single solution from OmniSolver
// ════════════════════════════════════════════════════════════════════

struct OmniSolution {
    vpam::ExactVal exact;      ///< Exact value (valid for poly/inverse)
    double         numeric = 0.0; ///< Numeric value (always available)
    bool           isExact = false; ///< true if exact is meaningful
    SymExpr*       symbolic = nullptr; ///< Symbolic expression for result (arena)
};

// ════════════════════════════════════════════════════════════════════
// OmniResult — Return value from OmniSolver::solve()
// ════════════════════════════════════════════════════════════════════

struct OmniResult {
    std::vector<OmniSolution> solutions;
    EquationClass  classification = EquationClass::MixedTranscendental;
    bool           ok    = false;
    std::string    error;
    char           variable = 'x';
    CASStepLogger  steps;

    /// Complex root data (from quadratic with negative discriminant)
    bool           hasComplexRoots = false;
    vpam::ExactVal complexReal;       ///< Real part: -b/(2a)
    vpam::ExactVal complexImagMag;    ///< |Im|: sqrt(|D|)/(2a)
};

// ════════════════════════════════════════════════════════════════════
// OmniSolver — Universal equation solver
// ════════════════════════════════════════════════════════════════════

class OmniSolver {
public:
    OmniSolver();

    /// Solve an equation given as two SymExpr trees: lhs = rhs.
    /// Normalizes to f(x) = 0 internally.
    ///
    /// @param lhs   Left-hand side expression (arena-allocated).
    /// @param rhs   Right-hand side expression (arena-allocated).
    /// @param var   Variable to solve for (default: 'x').
    /// @param arena Arena for all intermediate allocations.
    /// @return OmniResult with solutions, classification, and steps.
    OmniResult solve(SymExpr* lhs, SymExpr* rhs, char var,
                     SymExprArena& arena);

    /// Solve f(x) = 0 (single expression form).
    OmniResult solveExpr(SymExpr* f, char var, SymExprArena& arena);

    /// Classify an expression (f(x) = 0 form).
    static EquationClass classify(SymExpr* f, char var);

private:
    // ── Strategy dispatchers ────────────────────────────────────────

    /// Polynomial path: convert to SymPoly, delegate to SingleSolver.
    bool solvePolynomial(SymExpr* f, char var, SymExprArena& arena,
                         OmniResult& result);

    /// Analytic isolation: recursive peeling when var appears once.
    /// Returns true if solved analytically with exact CASRational.
    bool solveAnalytic(SymExpr* f, char var, SymExprArena& arena,
                       OmniResult& result);

    /// Inverse path: f(x) = c → x = f⁻¹(c).
    bool solveInverse(SymExpr* f, char var, SymExprArena& arena,
                      OmniResult& result);

    /// Hybrid Newton fallback.
    bool solveNewton(SymExpr* f, char var, SymExprArena& arena,
                     OmniResult& result);

    // ── Inverse analysis ────────────────────────────────────────────

    /// Check if `expr` has the form Func(var) with no other var usage.
    /// Returns the SymFunc if yes, nullptr if no.
    static SymFunc* extractSimpleFunc(SymExpr* expr, char var);

    /// Check if `expr` is Pow(var, const) or Pow(const, var).
    /// Used for simple inverse on power forms.
    static SymPow* extractSimplePow(SymExpr* expr, char var);

    /// Apply the inverse of a function symbolically.
    /// Returns the inverted SymExpr, or nullptr if not invertible.
    static SymExpr* applyInverse(SymFuncKind kind, SymExpr* val,
                                  SymExprArena& arena);

    // ── Analytic isolation helpers ──────────────────────────────────

    /// Count how many times variable `var` appears in the tree.
    static int countVar(const SymExpr* expr, char var);

    /// Recursive isolation step: given  expr = rhs, isolate `var`.
    /// Returns the solved expression for `var`, or nullptr on failure.
    static SymExpr* isolateVar(SymExpr* expr, SymExpr* rhs, char var,
                               SymExprArena& arena, CASStepLogger& log,
                               int depth);
};

} // namespace cas
