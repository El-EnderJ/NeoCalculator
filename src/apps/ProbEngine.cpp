/**
 * ProbEngine.cpp — Probability computation engine for NumOS.
 *
 * Normal distribution PDF/CDF using Abramowitz & Stegun ERF approximation.
 */

#include "ProbEngine.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ════════════════════════════════════════════════════════════════════════════
// normalPDF — Probability Density Function of Normal Distribution
// ════════════════════════════════════════════════════════════════════════════

double ProbEngine::normalPDF(double x, double mu, double sigma) {
    if (sigma <= 0.0) return 0.0;
    double z = (x - mu) / sigma;
    return (1.0 / (sigma * std::sqrt(2.0 * M_PI))) * std::exp(-0.5 * z * z);
}

// ════════════════════════════════════════════════════════════════════════════
// erf_approx — Abramowitz & Stegun approximation (formula 7.1.26)
//   Maximum error: |ε(x)| ≤ 1.5 × 10⁻⁷
// ════════════════════════════════════════════════════════════════════════════

double ProbEngine::erf_approx(double x) {
    // Constants from Abramowitz & Stegun
    static const double a1 =  0.254829592;
    static const double a2 = -0.284496736;
    static const double a3 =  1.421413741;
    static const double a4 = -1.453152027;
    static const double a5 =  1.061405429;
    static const double p  =  0.3275911;

    // Save the sign of x
    int sign = 1;
    if (x < 0) sign = -1;
    x = std::fabs(x);

    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);

    return sign * y;
}

// ════════════════════════════════════════════════════════════════════════════
// normalCDF — Cumulative Distribution Function via ERF
//   Φ(x) = 0.5 · [1 + erf((x - μ) / (σ · √2))]
// ════════════════════════════════════════════════════════════════════════════

double ProbEngine::normalCDF(double x, double mu, double sigma) {
    if (sigma <= 0.0) return (x >= mu) ? 1.0 : 0.0;
    double z = (x - mu) / (sigma * std::sqrt(2.0));
    return 0.5 * (1.0 + erf_approx(z));
}

// ════════════════════════════════════════════════════════════════════════════
// generatePDFCurve — Fill array with PDF values for plotting
// ════════════════════════════════════════════════════════════════════════════

void ProbEngine::generatePDFCurve(double mu, double sigma,
                                    double xMin, double xMax,
                                    double* yOut, int nPoints) {
    if (nPoints < 2 || sigma <= 0.0) return;
    double step = (xMax - xMin) / (nPoints - 1);
    for (int i = 0; i < nPoints; ++i) {
        double x = xMin + i * step;
        yOut[i] = normalPDF(x, mu, sigma);
    }
}
