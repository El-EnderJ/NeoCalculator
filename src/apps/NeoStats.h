/**
 * NeoStats.h — Professional Statistics & Regression engine for NeoLanguage.
 *
 * Provides descriptive statistics, regression models, and probability
 * distribution functions that operate on NeoLanguage List values.
 *
 * Statistics functions:
 *   mean(list)           — arithmetic mean
 *   median(list)         — median value
 *   stddev(list)         — sample standard deviation
 *   variance(list)       — sample variance
 *   sort(list)           — sorted copy of a list (ascending)
 *
 * Regression:
 *   regress(x, y, model) — fit a model to (x, y) data; returns a Function.
 *     model = "linear"   → a + b·x
 *     model = "quad"     → a + b·x + c·x²
 *     model = "exp"      → a·e^(b·x)
 *     model = "log"      → a + b·ln(x)
 *
 * Probability:
 *   pdf_normal(x, mu, sigma) — Gaussian PDF
 *   cdf_normal(x, mu, sigma) — Gaussian CDF (approximated via erf)
 *   factorial(n)             — n!  (returns exact double for n ≤ 170)
 *   ncr(n, r)                — binomial coefficient C(n,r)
 *   npr(n, r)                — permutations P(n,r)
 *
 * Design notes:
 *   · Regression returns a NeoValue of type NativeFunction so the caller
 *     can immediately write f = regress(x, y, "linear") and then f(10).
 *   · All computations use IEEE-754 double precision — suitable for the
 *     data sizes typical on an ESP32 calculator.
 *   · NeoRegModel is allocated with `new` and shared via a raw pointer
 *     inside a NativeFunction NeoValue (reference semantics, no RAII).
 *     Memory is bulk-reset between program runs (like NeoValue::List).
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 5 (Data Science)
 */
#pragma once

#include <cstddef>
#include <vector>
#include "NeoValue.h"

// ════════════════════════════════════════════════════════════════════
// NeoRegModel — regression model coefficients & predictor
// ════════════════════════════════════════════════════════════════════

/**
 * Holds the coefficients for one of four supported regression models
 * and can evaluate predictions at any x.
 */
struct NeoRegModel {
    enum class Kind : uint8_t { Linear, Quad, Exp, Log };

    Kind   kind = Kind::Linear;
    double a    = 0.0;   ///< intercept / scale coefficient
    double b    = 0.0;   ///< linear / exponent coefficient
    double c    = 0.0;   ///< quadratic coefficient (quad model only)
    double r2   = 0.0;   ///< coefficient of determination (fit quality)

    /// Predict y at the given x using the stored model.
    double predict(double x) const;
};

// ════════════════════════════════════════════════════════════════════
// NeoStats — static helper class
// ════════════════════════════════════════════════════════════════════

class NeoStats {
public:
    // ── Descriptive statistics ────────────────────────────────────

    /// Arithmetic mean of a list.  Returns NaN if list is empty.
    static double mean    (const std::vector<NeoValue>& list);

    /// Median of a list (sorts a copy).  Returns NaN if empty.
    static double median  (const std::vector<NeoValue>& list);

    /// Sample standard deviation (N−1 denominator).  Returns 0 if ≤1 element.
    static double stddev  (const std::vector<NeoValue>& list);

    /// Sample variance (N−1 denominator).
    static double variance(const std::vector<NeoValue>& list);

    /// Sort a copy of a list ascending; returns the sorted copy.
    static std::vector<double> sortedCopy(const std::vector<NeoValue>& list);

    // ── Regression ────────────────────────────────────────────────

    /**
     * Fit a regression model to (x_data, y_data).
     * @param x_data   List of x-values.
     * @param y_data   List of y-values (same length as x_data).
     * @param kind     Model type.
     * @param out      [out] Filled on success.
     * @return true if fit succeeded; false if data is insufficient.
     */
    static bool regress(const std::vector<NeoValue>& x_data,
                        const std::vector<NeoValue>& y_data,
                        NeoRegModel::Kind            kind,
                        NeoRegModel&                 out);

    // ── Probability ───────────────────────────────────────────────

    /// Gaussian PDF: (1 / (sigma * sqrt(2π))) * exp(-0.5 * ((x-mu)/sigma)^2)
    static double pdfNormal(double x, double mu, double sigma);

    /// Gaussian CDF: 0.5 * (1 + erf((x - mu) / (sigma * sqrt(2))))
    static double cdfNormal(double x, double mu, double sigma);

    /// n! for integer n ≥ 0.  Returns 1 for n=0; +inf for n > 170.
    static double factorial(int n);

    /// Binomial coefficient C(n,r) = n! / (r! * (n-r)!)
    static double ncr(int n, int r);

    /// Permutations P(n,r) = n! / (n-r)!
    static double npr(int n, int r);

private:
    NeoStats() = delete;

    // ── Regression helpers ────────────────────────────────────────
    static bool regressLinear(const std::vector<double>& xs,
                              const std::vector<double>& ys,
                              NeoRegModel& out);
    static bool regressQuad  (const std::vector<double>& xs,
                              const std::vector<double>& ys,
                              NeoRegModel& out);
    static bool regressExp   (const std::vector<double>& xs,
                              const std::vector<double>& ys,
                              NeoRegModel& out);
    static bool regressLog   (const std::vector<double>& xs,
                              const std::vector<double>& ys,
                              NeoRegModel& out);

    /// Compute R² from actual ys and predicted ys.
    static double computeR2(const std::vector<double>& ys,
                             const std::vector<double>& pred);

    /// Solve a 2×2 linear system  A·v = b  via Cramer's rule.
    /// Returns false if the determinant is (near) zero.
    static bool solve2x2(double a00, double a01,
                         double a10, double a11,
                         double b0,  double b1,
                         double& x0, double& x1);

    /// Solve a 3×3 linear system  A·v = b  via Gaussian elimination.
    static bool solve3x3(double a[3][3], double b[3], double x[3]);
};
