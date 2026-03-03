/**
 * HybridNewton.h — Newton-Raphson solver with exact symbolic Jacobian.
 *
 * Uses SymDiff::diff() to compute the exact derivative f'(x) once,
 * then evaluates f(x) and f'(x) numerically at each iteration:
 *
 *   x_{n+1} = x_n - f(x_n) / f'(x_n)
 *
 * Multi-guess strategy:
 *   Launches Newton from several initial seeds spread across a
 *   configurable search window. Converged roots are de-duplicated
 *   and verified (|f(root)| < tolerance).
 *
 * All intermediate SymExpr nodes (the derivative tree) live in the
 * caller-provided SymExprArena.
 *
 * Part of: NumOS Pro-CAS — Phase 4 (Omni-Solver)
 */

#pragma once

#include "SymExpr.h"
#include "SymExprArena.h"
#include "CASStepLogger.h"
#include "../ExactVal.h"

#include <cstdint>
#include <vector>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// NewtonRoot — A single root found by Newton-Raphson
// ════════════════════════════════════════════════════════════════════

struct NewtonRoot {
    double       value      = 0.0;  ///< Root value (double precision)
    vpam::ExactVal exact;           ///< Best ExactVal representation
    int          iterations = 0;    ///< Iterations to converge
    bool         verified   = false;///< |f(root)| < verify tolerance
};

// ════════════════════════════════════════════════════════════════════
// NewtonResult — Return value from HybridNewton::solve()
// ════════════════════════════════════════════════════════════════════

struct NewtonResult {
    std::vector<NewtonRoot> roots;  ///< De-duplicated roots found
    bool        ok    = false;      ///< At least one root found
    std::string error;              ///< Error message if !ok
};

// ════════════════════════════════════════════════════════════════════
// HybridNewton — Multi-guess Newton-Raphson with exact Jacobian
// ════════════════════════════════════════════════════════════════════

class HybridNewton {
public:
    /// Solve f(x) = 0 where `f` is a SymExpr tree.
    ///
    /// @param f       Expression representing f(x) (should equal zero).
    /// @param var     Variable to solve for (e.g. 'x').
    /// @param arena   Arena for derivative allocation.
    /// @param log     Step logger for decision logging.
    /// @param maxRoots Maximum number of roots to search for (default: 4).
    /// @return NewtonResult with de-duplicated verified roots.
    static NewtonResult solve(SymExpr* f, char var, SymExprArena& arena,
                              CASStepLogger* log = nullptr,
                              int maxRoots = 4);

    // ── Tuning constants ────────────────────────────────────────────
    static constexpr int    MAX_ITER       = 60;      ///< Max iterations per guess
    static constexpr double TOLERANCE      = 1e-12;   ///< Convergence tolerance |dx|
    static constexpr double VERIFY_TOL     = 1e-8;    ///< |f(root)| verification
    static constexpr double DEDUP_TOL      = 1e-8;    ///< Root de-duplication distance
    static constexpr double DERIV_FLOOR    = 1e-15;   ///< Minimum |f'(x)| to avoid div/0
    static constexpr double DAMPING_CLAMP  = 100.0;   ///< Max Newton step size

private:
    /// Run Newton from a single initial guess.
    /// Returns true if converged, writing root into `out`.
    static bool runFromGuess(SymExpr* f, SymExpr* df,
                             double x0, NewtonRoot& out);

    /// Generate initial guesses spread across search window.
    static void makeGuesses(double* out, int& count, int maxRoots);
};

} // namespace cas
