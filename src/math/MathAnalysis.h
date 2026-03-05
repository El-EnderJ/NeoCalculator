/**
 * MathAnalysis.h — Numerical Analysis Engine
 *
 * Lightweight numerical methods for the Grapher's "Calculate" menu:
 *   · findRoot()         — Bisection method to find f(x) = 0
 *   · findExtremum()     — Find local minimum or maximum
 *   · findIntersection() — Find where f1(x) = f2(x)
 *   · numericalIntegral()— Simpson's rule for area under curve
 *
 * All algorithms use strict iteration limits to guarantee bounded
 * execution time on ESP32, preventing watchdog resets.
 *
 * Dependencies: C++ standard only (no LVGL, no Arduino).
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <functional>

namespace math {

/// Result of a numerical analysis operation
struct AnalysisResult {
    bool   found;    ///< true if the algorithm converged
    double x;        ///< x-coordinate of the result
    double y;        ///< y-coordinate (f(x) at result)
};

/// Type alias for a single-variable evaluator function
using EvalFunc = std::function<double(double)>;

/// Maximum iterations for all iterative algorithms (prevents infinite loops)
static constexpr int MAX_ITER = 100;

/// Default tolerance for convergence
static constexpr double DEFAULT_TOL = 1e-5;

/**
 * Find a root of f(x) = 0 in [xMin, xMax] using the Bisection Method.
 *
 * The bisection method is chosen over Newton-Raphson because:
 *   1. It never requires computing derivatives.
 *   2. It never diverges (no division by near-zero derivatives).
 *   3. Guaranteed convergence if a sign change exists.
 *
 * @param f      Function to evaluate
 * @param xMin   Left bound of the search interval
 * @param xMax   Right bound of the search interval
 * @param tol    Convergence tolerance (default: 1e-5)
 * @return       AnalysisResult with found=true if a root was located
 */
AnalysisResult findRoot(const EvalFunc& f, double xMin, double xMax,
                        double tol = DEFAULT_TOL);

/**
 * Find a local extremum (minimum or maximum) of f(x) in [xMin, xMax].
 *
 * Uses a two-phase approach:
 *   1. Coarse scan: evaluate f at ~200 evenly-spaced points
 *   2. Fine refinement: golden-section search around the best candidate
 *
 * @param f      Function to evaluate
 * @param xMin   Left bound
 * @param xMax   Right bound
 * @param isMax  true = find maximum, false = find minimum
 * @return       AnalysisResult with the extremum's (x, y)
 */
AnalysisResult findExtremum(const EvalFunc& f, double xMin, double xMax,
                            bool isMax);

/**
 * Find an intersection point where f1(x) = f2(x) in [xMin, xMax].
 *
 * Internally computes g(x) = f1(x) - f2(x) and applies findRoot on g.
 *
 * @param f1     First function
 * @param f2     Second function
 * @param xMin   Left bound
 * @param xMax   Right bound
 * @return       AnalysisResult with the intersection point
 */
AnalysisResult findIntersection(const EvalFunc& f1, const EvalFunc& f2,
                                double xMin, double xMax);

/**
 * Compute the definite integral ∫[xMin,xMax] f(x) dx using Simpson's Rule.
 *
 * Uses composite Simpson's 1/3 rule with n=100 subintervals.
 * Returns the area and sets y = area in the result.
 *
 * @param f      Function to integrate
 * @param xMin   Lower integration bound
 * @param xMax   Upper integration bound
 * @return       AnalysisResult with found=true, x=xMax, y=area
 */
AnalysisResult numericalIntegral(const EvalFunc& f, double xMin, double xMax);

} // namespace math
