/**
 * SingleSolver.h — Single-variable equation solver for CAS-S3-ULTRA.
 *
 * Solves equations of the form  f(x) = 0  after normalization.
 * Supports:
 *   · Linear    (degree 1):  ax + b = 0  →  x = −b/a
 *   · Quadratic (degree 2):  ax² + bx + c = 0  →  discriminant method
 *   · Fallback  (degree ≥ 3): Newton-Raphson numeric approximation
 *
 * Pilar 1 — CASNumber:
 *   All intermediate arithmetic uses CASNumber (Rational → BigInt → Double)
 *   instead of raw int64_t ExactVal.  Results bridge to ExactVal via
 *   CASNumber::toExactVal() for the rendering pipeline.
 *
 * Pilar 2 — PedagogicalLogger:
 *   Algebraic steps are logged via SolveAction + ActionContext, producing
 *   rich contextual phrases instead of static strings.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase C (SingleSolver + Dynamic Reasoning)
 */

#pragma once

#include <cstdint>
#include <vector>
#include "CASNumber.h"
#include "PedagogicalLogger.h"
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

    /// true if discriminant < 0 → complex conjugate roots
    bool           hasComplexRoots = false;

    /// Real part of complex roots: Re = -b/(2a)
    vpam::ExactVal complexReal;

    /// Imaginary magnitude: |Im| = sqrt(|Δ|)/(2a)
    vpam::ExactVal complexImagMag;

    /// Step-by-step reasoning log (PedagogicalLogger for action-based phrases)
    PedagogicalLogger  steps;
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
    SymEquation normalize(const SymEquation& eq, PedagogicalLogger& log);

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
