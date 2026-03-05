/**
 * ProbEngine.h — Probability computation engine for NumOS.
 *
 * Implements Normal Distribution:
 *   - PDF (Probability Density Function)
 *   - CDF (Cumulative Distribution Function) via Abramowitz & Stegun ERF
 *
 * Pure C++ — no LVGL or Arduino dependencies.
 */

#pragma once

#include <cmath>

class ProbEngine {
public:
    /// Normal distribution PDF: f(x) = (1/(σ√(2π))) · e^(-½((x-μ)/σ)²)
    static double normalPDF(double x, double mu, double sigma);

    /// Normal distribution CDF: P(X ≤ x) using ERF approximation
    static double normalCDF(double x, double mu, double sigma);

    /// Error function approximation (Abramowitz & Stegun, formula 7.1.26)
    static double erf_approx(double x);

    /// Generate PDF curve points for plotting
    /// Fills yOut[0..nPoints-1] with PDF values from xMin to xMax
    static void generatePDFCurve(double mu, double sigma,
                                  double xMin, double xMax,
                                  double* yOut, int nPoints);
};
