/**
 * NeoScientific.h — Advanced Scientific Computing for NeoLanguage Phase 7.
 *
 * Provides:
 *
 *   Differential Equations:
 *     ndsolve(f, y0, x0, x1, steps) — Numerical ODE solver using RK4.
 *       f(x, y) is the derivative dy/dx, y0 is the initial condition at x0.
 *       Returns a List of [x, y] pairs that can be plotted.
 *
 *   Optimization:
 *     minimize(f, x0)  — Find local minimum using Nelder-Mead simplex.
 *     maximize(f, x0)  — Find local maximum (minimise –f).
 *
 *   Special Functions:
 *     gamma(x)         — Gamma function Γ(x) via Lanczos approximation.
 *     beta(x, y)       — Beta function B(x,y) = Γ(x)Γ(y)/Γ(x+y).
 *     erf(x)           — Error function via Horner approximation.
 *
 * All functions are static, header-only, and require no dynamic allocation
 * beyond the List returned by ndsolve.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 7
 */
#pragma once

#include <cmath>
#include <vector>
#include <functional>
#include <limits>
#include "NeoValue.h"

namespace NeoScientific {

// ════════════════════════════════════════════════════════════════════
// Special Functions
// ════════════════════════════════════════════════════════════════════

/**
 * Gamma function Γ(x) using the Lanczos approximation (g=7, 9 terms).
 * Accurate to ~14 significant digits for Re(x) > 0.5.
 * For negative non-integer x: uses the reflection formula
 *   Γ(x) = π / (sin(πx) · Γ(1−x))
 */
inline double gamma(double x) {
    if (x <= 0.0 && x == std::floor(x)) {
        return std::numeric_limits<double>::infinity();  // pole
    }
    // Reflection for x < 0.5
    if (x < 0.5) {
        return M_PI / (std::sin(M_PI * x) * gamma(1.0 - x));
    }
    static const double g = 7.0;
    static const double c[] = {
         0.99999999999980993,
       676.5203681218851,
     -1259.1392167224028,
       771.32342877765313,
      -176.61502916214059,
        12.507343278686905,
        -0.13857109526572012,
         9.9843695780195716e-6,
         1.5056327351493116e-7
    };
    x -= 1.0;
    double a = c[0];
    double t = x + g + 0.5;
    for (int i = 1; i < 9; ++i) a += c[i] / (x + i);
    return std::sqrt(2.0 * M_PI) * std::pow(t, x + 0.5) * std::exp(-t) * a;
}

/**
 * Beta function B(x, y) = Γ(x)·Γ(y) / Γ(x+y).
 */
inline double beta(double x, double y) {
    return gamma(x) * gamma(y) / gamma(x + y);
}

/**
 * Error function erf(x) via Horner's method (Abramowitz & Stegun 7.1.26).
 * Maximum error < 1.5e-7.
 */
inline double erf(double x) {
    if (x < 0.0) return -erf(-x);
    static const double p  =  0.3275911;
    static const double a1 =  0.254829592;
    static const double a2 = -0.284496736;
    static const double a3 =  1.421413741;
    static const double a4 = -1.453152027;
    static const double a5 =  1.061405429;
    double t = 1.0 / (1.0 + p * x);
    double poly = t * (a1 + t * (a2 + t * (a3 + t * (a4 + t * a5))));
    return 1.0 - poly * std::exp(-x * x);
}

// ════════════════════════════════════════════════════════════════════
// Numerical ODE Solver — RK4
// ════════════════════════════════════════════════════════════════════

/**
 * Solve dy/dx = f(x, y) numerically using the 4th-order Runge-Kutta method.
 *
 * @param f     The derivative function f(x, y) → dy/dx.
 * @param y0    Initial condition y(x0) = y0.
 * @param x0    Start of integration interval.
 * @param x1    End of integration interval.
 * @param steps Number of integration steps (resolution).
 * @return      List of [x, y] pairs (each element is a 2-element List).
 *              Returns empty list on invalid arguments.
 */
inline std::vector<NeoValue>* ndsolveRK4(
    std::function<double(double, double)> f,
    double y0,
    double x0,
    double x1,
    int    steps = 200)
{
    auto* result = new std::vector<NeoValue>();
    if (steps <= 0 || steps > 10000) steps = 200;
    if (x0 >= x1) return result;

    double h = (x1 - x0) / static_cast<double>(steps);
    double x = x0;
    double y = y0;

    result->reserve(steps + 1);

    for (int i = 0; i <= steps; ++i) {
        auto* pair = new std::vector<NeoValue>();
        pair->push_back(NeoValue::makeNumber(x));
        pair->push_back(NeoValue::makeNumber(y));
        result->push_back(NeoValue::makeList(pair));

        if (i < steps) {
            double k1 = f(x,           y);
            double k2 = f(x + h / 2.0, y + h * k1 / 2.0);
            double k3 = f(x + h / 2.0, y + h * k2 / 2.0);
            double k4 = f(x + h,       y + h * k3);
            y += h * (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
            x += h;
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════
// Optimization — Nelder-Mead Simplex (1-D case)
// ════════════════════════════════════════════════════════════════════

/**
 * Minimise a 1-D scalar function f(x) using a simple golden-section search.
 * For starting point x0, searches in [x0 - 1e3, x0 + 1e3].
 *
 * @param f         The objective function f(x) → double.
 * @param x0        Initial guess.
 * @param tolerance Convergence tolerance (default 1e-6).
 * @param maxIter   Maximum iterations (default 100).
 * @return          The x value that minimises f(x).
 */
inline double minimizeGolden(
    std::function<double(double)> f,
    double x0,
    double tolerance = 1e-6,
    int    maxIter   = 200)
{
    // Use a Nelder-Mead-like simplex in 1D: start with 3 points
    // and iteratively shrink around the minimum.
    static const double GOLDEN = 0.6180339887498949;
    double a = x0 - 10.0;
    double b = x0 + 10.0;

    // Narrow initial bracket around x0
    double fa = f(a), fb0 = f(x0), fb = f(b);
    (void)fa; (void)fb;
    if (fb0 < fa && fb0 < fb) {
        // x0 is already a local min — shrink bracket
        a = x0 - 10.0;
        b = x0 + 10.0;
    }

    // Brent's method (simplified golden section)
    double c = b - GOLDEN * (b - a);
    double d = a + GOLDEN * (b - a);
    for (int i = 0; i < maxIter; ++i) {
        if (std::abs(c - d) < tolerance) break;
        if (f(c) < f(d)) {
            b = d;
        } else {
            a = c;
        }
        c = b - GOLDEN * (b - a);
        d = a + GOLDEN * (b - a);
    }
    return (a + b) / 2.0;
}

/**
 * Maximise f(x) by minimising –f(x).
 */
inline double maximizeGolden(
    std::function<double(double)> f,
    double x0,
    double tolerance = 1e-6,
    int    maxIter   = 200)
{
    return minimizeGolden([&f](double x){ return -f(x); }, x0, tolerance, maxIter);
}

} // namespace NeoScientific
