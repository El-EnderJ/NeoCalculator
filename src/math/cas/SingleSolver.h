/**
 * SingleSolver.h — Single-variable equation solver for CAS-Lite.
 *
 * Solves equations of the form  f(x) = 0  after normalization.
 * Supports:
 *   · Linear    (degree 1):  ax + b = 0  →  x = −b/a
 *   · Quadratic (degree 2):  ax² + bx + c = 0  →  discriminant method
 *   · Fallback  (degree ≥ 3): Newton-Raphson numeric approximation
 *
 * Every algebraic manipulation is logged to a CASStepLogger for
 * step-by-step display.
 *
 * Part of: NumOS CAS-Lite — Phase C (SingleSolver + Steps)
 */

#pragma once

#include <cstdint>
#include <vector>
#include "CASStepLogger.h"
#include "SymEquation.h"
#include "SymPoly.h"
#include "PSRAMAllocator.h"
#include "../ExactVal.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SolveResult — Return value from SingleSolver::solve()
// ════════════════════════════════════════════════════════════════════

struct SolveResult {
    /// Up to 2 exact solutions (linear → 1, quadratic → 0/1/2)
    vpam::ExactVal solutions[2];

    /// How many solutions were found (0..2)
    uint8_t        count    = 0;

    /// true if solve completed without error
    bool           ok       = false;

    /// Human-readable error message (empty if ok)
    std::string    error;

    /// Variable that was solved for
    char           variable = 'x';

    /// true if numeric fallback (Newton) was used
    bool           numeric  = false;

    /// Reference to the step logger used (owned by caller)
    CASStepLogger  steps;
};

// ════════════════════════════════════════════════════════════════════
// SingleSolver — Single-variable equation solver
// ════════════════════════════════════════════════════════════════════

class SingleSolver {
public:
    SingleSolver();

    /// Solve an equation for the given variable.
    /// Returns SolveResult with solutions, steps, and status.
    SolveResult solve(const SymEquation& eq, char variable = 'x');

private:
    // ── Internal solver methods ─────────────────────────────────────

    /// Normalize equation to f(x) = 0 form via moveAllToLHS().
    SymEquation normalize(const SymEquation& eq, CASStepLogger& log);

    /// Solve linear: ax + b = 0  →  x = -b/a
    bool solveLinear(const SymEquation& eq, char var, SolveResult& result);

    /// Solve quadratic: ax² + bx + c = 0  →  discriminant method
    bool solveQuadratic(const SymEquation& eq, char var, SolveResult& result);

    /// Newton-Raphson fallback for degree ≥ 3
    bool solveNewton(const SymEquation& eq, char var, SolveResult& result);

    // ── Helper ──────────────────────────────────────────────────────

    /// Evaluate polynomial f(x) at a given double value
    double evalPoly(const SymPoly& poly, char var, double x);

    /// Evaluate derivative f'(x) at a given double value (numeric diff)
    double evalDerivative(const SymPoly& poly, char var, double x);
};

} // namespace cas
