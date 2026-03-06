/**
 * RegressionEngine.cpp — Least-Squares regression engine.
 *
 * Linear:    Solves 2×2 normal equations.
 * Quadratic: Solves 3×3 normal equations via Cramer's rule.
 * R^2:       1 - SS_res / SS_tot
 *
 * Division-by-zero guards included throughout.
 */

#include "RegressionEngine.h"
#include <cmath>

// ════════════════════════════════════════════════════════════════════════════
// fitLinear — y = mx + b
// ════════════════════════════════════════════════════════════════════════════

RegressionEngine::Result RegressionEngine::fitLinear(const Point* pts, size_t n) {
    Result res = { false, Model::LINEAR, 0, 0, 0, 0 };
    if (!pts || n < 2) return res;

    double sumX  = 0, sumY  = 0;
    double sumXY = 0, sumX2 = 0;

    for (size_t i = 0; i < n; ++i) {
        sumX  += pts[i].x;
        sumY  += pts[i].y;
        sumXY += pts[i].x * pts[i].y;
        sumX2 += pts[i].x * pts[i].x;
    }

    double N = static_cast<double>(n);
    double denom = N * sumX2 - sumX * sumX;

    // Guard: if all X values are identical, the slope is undefined
    if (std::fabs(denom) < 1e-15) return res;

    double m = (N * sumXY - sumX * sumY) / denom;
    double b = (sumY - m * sumX) / N;

    // R^2 = 1 - SS_res / SS_tot
    double meanY  = sumY / N;
    double ssTot  = 0, ssRes = 0;
    for (size_t i = 0; i < n; ++i) {
        double yHat = m * pts[i].x + b;
        ssRes += (pts[i].y - yHat) * (pts[i].y - yHat);
        ssTot += (pts[i].y - meanY) * (pts[i].y - meanY);
    }

    double r2 = (std::fabs(ssTot) < 1e-15) ? 1.0 : (1.0 - ssRes / ssTot);

    res.ok    = true;
    res.a     = m;
    res.b     = b;
    res.c     = 0;
    res.r2    = r2;
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// fitQuadratic — y = ax^2 + bx + c
// Normal equations (3×3 system, Cramer's rule)
// ════════════════════════════════════════════════════════════════════════════

RegressionEngine::Result RegressionEngine::fitQuadratic(const Point* pts, size_t n) {
    Result res = { false, Model::QUADRATIC, 0, 0, 0, 0 };
    if (!pts || n < 3) return res;

    // Accumulate sums for normal equations
    double S0 = static_cast<double>(n);  // Σ1
    double S1 = 0, S2 = 0, S3 = 0, S4 = 0;
    double T0 = 0, T1 = 0, T2 = 0;

    for (size_t i = 0; i < n; ++i) {
        double x  = pts[i].x;
        double x2 = x * x;
        S1 += x;
        S2 += x2;
        S3 += x2 * x;
        S4 += x2 * x2;
        T0 += pts[i].y;
        T1 += x * pts[i].y;
        T2 += x2 * pts[i].y;
    }

    // Normal equations matrix:
    // | S4  S3  S2 | | a |   | T2 |
    // | S3  S2  S1 | | b | = | T1 |
    // | S2  S1  S0 | | c |   | T0 |
    //
    // Solve via Cramer's rule

    // Determinant of the 3×3 coefficient matrix
    double D = S4 * (S2 * S0 - S1 * S1)
             - S3 * (S3 * S0 - S1 * S2)
             + S2 * (S3 * S1 - S2 * S2);

    if (std::fabs(D) < 1e-15) return res;

    double Da = T2 * (S2 * S0 - S1 * S1)
              - S3 * (T1 * S0 - S1 * T0)
              + S2 * (T1 * S1 - S2 * T0);

    double Db = S4 * (T1 * S0 - S1 * T0)
              - T2 * (S3 * S0 - S1 * S2)
              + S2 * (S3 * T0 - T1 * S2);

    double Dc = S4 * (S2 * T0 - T1 * S1)
              - S3 * (S3 * T0 - T1 * S2)
              + T2 * (S3 * S1 - S2 * S2);

    double a = Da / D;
    double b = Db / D;
    double c = Dc / D;

    // R^2
    double meanY = T0 / S0;
    double ssTot = 0, ssRes = 0;
    for (size_t i = 0; i < n; ++i) {
        double yHat = a * pts[i].x * pts[i].x + b * pts[i].x + c;
        ssRes += (pts[i].y - yHat) * (pts[i].y - yHat);
        ssTot += (pts[i].y - meanY) * (pts[i].y - meanY);
    }

    double r2 = (std::fabs(ssTot) < 1e-15) ? 1.0 : (1.0 - ssRes / ssTot);

    res.ok = true;
    res.a  = a;
    res.b  = b;
    res.c  = c;
    res.r2 = r2;
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// evaluate — Evaluate regression at a point
// ════════════════════════════════════════════════════════════════════════════

double RegressionEngine::evaluate(const Result& r, double x) {
    if (r.model == Model::LINEAR) {
        return r.a * x + r.b;
    }
    return r.a * x * x + r.b * x + r.c;
}
