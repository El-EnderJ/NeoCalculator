/**
 * HybridNewton.cpp — Newton-Raphson with exact symbolic derivative.
 *
 * Key difference from SingleSolver::solveNewton():
 *   Old: numeric central-difference derivative  (h = 1e-9)
 *   New: exact symbolic derivative via SymDiff::diff(), then evaluate()
 *
 * This eliminates numerical derivative error and improves convergence
 * reliability, especially for stiff functions like e^(2x) + x.
 *
 * Strategy:
 *   1. Compute f'(x) symbolically (once, reused for all guesses)
 *   2. Simplify f'(x) to reduce evaluation cost
 *   3. Launch Newton from 16 diversified initial seeds
 *   4. De-duplicate converged roots (|r1 - r2| < DEDUP_TOL)
 *   5. Verify each root: |f(root)| < VERIFY_TOL
 *
 * Part of: NumOS Pro-CAS — Phase 4 (Omni-Solver)
 */

#include "HybridNewton.h"
#include "SymDiff.h"
#include "SymSimplify.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// makeGuesses — Diversified initial seeds for Newton iterations
// ════════════════════════════════════════════════════════════════════

void HybridNewton::makeGuesses(double* out, int& count, int /*maxRoots*/) {
    // Spread across a wide range to catch roots in different regions.
    // Includes small, medium, and large magnitudes, both signs.
    static const double seeds[] = {
         0.0,   0.5,  -0.5,   1.0,  -1.0,   2.0,  -2.0,
         5.0,  -5.0,  10.0, -10.0,   0.1,  -0.1,  20.0,
       -20.0, 100.0
    };
    count = static_cast<int>(sizeof(seeds) / sizeof(seeds[0]));
    for (int i = 0; i < count; ++i) out[i] = seeds[i];
}

// ════════════════════════════════════════════════════════════════════
// runFromGuess — Single Newton iteration from x0
// ════════════════════════════════════════════════════════════════════

bool HybridNewton::runFromGuess(SymExpr* f, SymExpr* df,
                                 double x0, NewtonRoot& out) {
    double x = x0;

    for (int i = 0; i < MAX_ITER; ++i) {
        double fx  = f->evaluate(x);
        double fpx = df->evaluate(x);

        // Guard: if derivative is too small, this guess is stuck
        if (std::abs(fpx) < DERIV_FLOOR) return false;

        // Guard: if f(x) is NaN/Inf, bail
        if (std::isnan(fx) || std::isinf(fx)) return false;

        double dx = fx / fpx;

        // Damping: clamp step size to avoid wild jumps
        if (dx > DAMPING_CLAMP) dx = DAMPING_CLAMP;
        if (dx < -DAMPING_CLAMP) dx = -DAMPING_CLAMP;

        double xNew = x - dx;

        // Guard: NaN/Inf result
        if (std::isnan(xNew) || std::isinf(xNew)) return false;

        if (std::abs(dx) < TOLERANCE) {
            // Converged — verify the root
            double fRoot = f->evaluate(xNew);
            out.value      = xNew;
            out.iterations = i + 1;
            out.verified   = (std::abs(fRoot) < VERIFY_TOL);
            out.exact      = vpam::ExactVal::fromDouble(xNew);
            out.exact.simplify();
            return out.verified;
        }

        x = xNew;
    }

    return false;  // Did not converge in MAX_ITER
}

// ════════════════════════════════════════════════════════════════════
// solve — Public API
// ════════════════════════════════════════════════════════════════════

NewtonResult HybridNewton::solve(SymExpr* f, char var, SymExprArena& arena,
                                  CASStepLogger* log, int maxRoots) {
    NewtonResult result;

    if (!f) {
        result.error = "Null expression";
        return result;
    }

    // 1. Compute the exact symbolic derivative f'(x)
    SymExpr* df = SymDiff::diff(f, var, arena);
    if (!df) {
        result.error = "Could not differentiate expression (unsupported function)";
        if (log) log->logNote(
            "Newton-Raphson: Error computing symbolic derivative",
            MethodId::Newton);
        return result;
    }

    // 2. Simplify the derivative to reduce evaluation cost
    df = SymSimplify::simplify(df, arena);

    if (log) {
        log->logNote(
            "Exact symbolic derivative: f'(" + std::string(1, var) + ") = " +
            df->toString(),
            MethodId::Newton);
    }

    // 3. Generate diversified initial guesses
    double guesses[16];
    int numGuesses = 0;
    makeGuesses(guesses, numGuesses, maxRoots);

    // 4. Run Newton from each guess, collect verified roots
    for (int i = 0; i < numGuesses; ++i) {
        if (static_cast<int>(result.roots.size()) >= maxRoots) break;

        NewtonRoot root;
        if (!runFromGuess(f, df, guesses[i], root)) continue;
        if (!root.verified) continue;

        // De-duplicate: check against all previously found roots
        bool dup = false;
        for (const auto& prev : result.roots) {
            if (std::abs(prev.value - root.value) < DEDUP_TOL) {
                dup = true;
                break;
            }
        }
        if (dup) continue;

        result.roots.push_back(root);
    }

    result.ok = !result.roots.empty();

    if (!result.ok) {
        result.error = "Newton-Raphson did not converge for any initial point";
        if (log) log->logNote(
            "Newton-Raphson: no real roots found",
            MethodId::Newton);
    } else {
        // Sort roots by value for consistent output
        std::sort(result.roots.begin(), result.roots.end(),
                  [](const NewtonRoot& a, const NewtonRoot& b) {
                      return a.value < b.value;
                  });

        if (log) {
            for (size_t i = 0; i < result.roots.size(); ++i) {
                char buf[120];
                snprintf(buf, sizeof(buf),
                    "Root %c%d = %.12g (converged in %d iterations, verified)",
                    var,
                    static_cast<int>(i + 1),
                    result.roots[i].value,
                    result.roots[i].iterations);
                log->logNote(buf, MethodId::Newton);
            }
        }
    }

    return result;
}

} // namespace cas
