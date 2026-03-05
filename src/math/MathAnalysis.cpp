/**
 * MathAnalysis.cpp — Numerical Analysis Engine
 *
 * Lightweight implementations of root-finding, extremum detection,
 * intersection search, and numerical integration.
 *
 * All algorithms enforce strict iteration limits (MAX_ITER = 100)
 * to guarantee bounded execution time on ESP32-S3, preventing
 * watchdog timer resets.
 */

#include "MathAnalysis.h"
#include <algorithm>

namespace math {

// ════════════════════════════════════════════════════════════════════════════
// findRoot — Bisection Method
//
// Guaranteed convergence when a sign change exists in [xMin, xMax].
// Safer than Newton-Raphson: no derivatives, no division-by-zero risk.
// ════════════════════════════════════════════════════════════════════════════

AnalysisResult findRoot(const EvalFunc& f, double xMin, double xMax,
                        double tol) {
    AnalysisResult res = { false, 0.0, 0.0 };
    if (!f) return res;

    double fMin = f(xMin);
    double fMax = f(xMax);

    // Check for NaN/Inf at boundaries
    if (std::isnan(fMin) || std::isinf(fMin) ||
        std::isnan(fMax) || std::isinf(fMax)) {
        return res;
    }

    // If no sign change, try scanning for a sub-interval with a sign change
    if (fMin * fMax > 0.0) {
        // Scan with 200 sample points to find a sign change
        static constexpr int SCAN_PTS = 200;
        double step = (xMax - xMin) / SCAN_PTS;
        bool found = false;
        double prevX = xMin;
        double prevF = fMin;
        for (int i = 1; i <= SCAN_PTS; ++i) {
            double curX = xMin + i * step;
            double curF = f(curX);
            if (std::isnan(curF) || std::isinf(curF)) {
                prevX = curX;
                prevF = curF;
                continue;
            }
            if (std::isnan(prevF) || std::isinf(prevF)) {
                prevX = curX;
                prevF = curF;
                continue;
            }
            if (prevF * curF <= 0.0) {
                xMin = prevX;
                xMax = curX;
                fMin = prevF;
                fMax = curF;
                found = true;
                break;
            }
            prevX = curX;
            prevF = curF;
        }
        if (!found) return res;
    }

    // Bisection loop
    double a = xMin, b = xMax;
    double fa = fMin;
    for (int iter = 0; iter < MAX_ITER; ++iter) {
        double mid = (a + b) * 0.5;
        double fMid = f(mid);

        if (std::isnan(fMid) || std::isinf(fMid)) {
            // Shrink from the NaN side
            b = mid;
            continue;
        }

        if (std::fabs(fMid) < tol || (b - a) * 0.5 < tol) {
            res.found = true;
            res.x = mid;
            res.y = fMid;
            return res;
        }

        if (fa * fMid < 0.0) {
            b = mid;
        } else {
            a = mid;
            fa = fMid;
        }
    }

    // Return best approximation even if not fully converged
    double mid = (a + b) * 0.5;
    double fMid = f(mid);
    if (!std::isnan(fMid) && std::fabs(fMid) < 0.01) {
        res.found = true;
        res.x = mid;
        res.y = fMid;
    }
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// findExtremum — Coarse scan + Golden-section refinement
//
// Phase 1: Evaluate ~200 points to find the candidate
// Phase 2: Golden-section search to refine
// ════════════════════════════════════════════════════════════════════════════

AnalysisResult findExtremum(const EvalFunc& f, double xMin, double xMax,
                            bool isMax) {
    AnalysisResult res = { false, 0.0, 0.0 };
    if (!f) return res;

    // Phase 1: Coarse scan
    static constexpr int SCAN_PTS = 200;
    double step = (xMax - xMin) / SCAN_PTS;
    double bestX = xMin;
    double bestY = f(xMin);

    if (std::isnan(bestY) || std::isinf(bestY)) {
        bestY = isMax ? -1e30 : 1e30;
    }

    for (int i = 1; i <= SCAN_PTS; ++i) {
        double cx = xMin + i * step;
        double cy = f(cx);
        if (std::isnan(cy) || std::isinf(cy)) continue;

        if ((isMax && cy > bestY) || (!isMax && cy < bestY)) {
            bestX = cx;
            bestY = cy;
            res.found = true;
        }
    }

    if (!res.found) return res;

    // Phase 2: Golden-section refinement around bestX
    static constexpr double PHI = 0.6180339887;  // (√5 - 1) / 2
    double a = std::max(xMin, bestX - step * 5);
    double b = std::min(xMax, bestX + step * 5);

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        if ((b - a) < DEFAULT_TOL) break;

        double x1 = b - PHI * (b - a);
        double x2 = a + PHI * (b - a);
        double f1 = f(x1);
        double f2 = f(x2);

        if (std::isnan(f1) || std::isinf(f1)) { a = x1; continue; }
        if (std::isnan(f2) || std::isinf(f2)) { b = x2; continue; }

        if (isMax) {
            if (f1 > f2) b = x2; else a = x1;
        } else {
            if (f1 < f2) b = x2; else a = x1;
        }
    }

    res.x = (a + b) * 0.5;
    res.y = f(res.x);
    if (std::isnan(res.y) || std::isinf(res.y)) {
        res.found = false;
    }
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// findIntersection — Find where f1(x) = f2(x)
//
// Reduces to root-finding on g(x) = f1(x) - f2(x).
// ════════════════════════════════════════════════════════════════════════════

AnalysisResult findIntersection(const EvalFunc& f1, const EvalFunc& f2,
                                double xMin, double xMax) {
    if (!f1 || !f2) return { false, 0.0, 0.0 };

    auto diff = [&](double x) -> double {
        double y1 = f1(x);
        double y2 = f2(x);
        if (std::isnan(y1) || std::isinf(y1) ||
            std::isnan(y2) || std::isinf(y2)) {
            return NAN;
        }
        return y1 - y2;
    };

    AnalysisResult res = findRoot(diff, xMin, xMax);
    if (res.found) {
        // Store actual y-value from f1 (not the difference)
        res.y = f1(res.x);
    }
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// numericalIntegral — Composite Simpson's 1/3 Rule
//
// ∫[a,b] f(x) dx ≈ (h/3) [f(a) + 4·f(x1) + 2·f(x2) + 4·f(x3) + ... + f(b)]
//
// Uses n=100 sub-intervals (must be even for Simpson's rule).
// ════════════════════════════════════════════════════════════════════════════

AnalysisResult numericalIntegral(const EvalFunc& f, double xMin, double xMax) {
    AnalysisResult res = { false, 0.0, 0.0 };
    if (!f) return res;
    if (xMax <= xMin) return res;

    static constexpr int N = 100;  // Number of sub-intervals (even)
    double h = (xMax - xMin) / N;

    double sum = 0.0;
    double fa = f(xMin);
    double fb = f(xMax);

    if (std::isnan(fa) || std::isinf(fa)) fa = 0.0;
    if (std::isnan(fb) || std::isinf(fb)) fb = 0.0;

    sum = fa + fb;

    for (int i = 1; i < N; ++i) {
        double xi = xMin + i * h;
        double fi = f(xi);
        if (std::isnan(fi) || std::isinf(fi)) fi = 0.0;

        if (i % 2 == 0) {
            sum += 2.0 * fi;
        } else {
            sum += 4.0 * fi;
        }
    }

    double area = sum * h / 3.0;

    res.found = true;
    res.x = xMax;
    res.y = area;
    return res;
}

} // namespace math
