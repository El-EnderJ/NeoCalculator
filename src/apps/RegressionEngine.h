/**
 * RegressionEngine.h — Regression analysis engine for NumOS.
 *
 * Pure C++ (no LVGL) engine for Least-Squares regression:
 *   - Linear:    y = mx + b
 *   - Quadratic: y = ax^2 + bx + c
 *   - R^2 (coefficient of determination)
 *
 * Part of: NumOS — Data Science & Probability Suite
 */

#pragma once

#include <cstddef>
#include <cstdint>

class RegressionEngine {
public:
    // ── Data point ───────────────────────────────────────────────────────
    struct Point {
        double x;
        double y;
    };

    // ── Regression model ─────────────────────────────────────────────────
    enum class Model : uint8_t {
        LINEAR = 0,     // y = mx + b
        QUADRATIC = 1   // y = ax^2 + bx + c
    };

    // ── Result ───────────────────────────────────────────────────────────
    struct Result {
        bool   ok;          // true if regression succeeded
        Model  model;
        double a, b, c;     // For linear: m=a, b=b (c unused)
                            // For quadratic: a, b, c
        double r2;          // Coefficient of determination
    };

    // ── Interface ────────────────────────────────────────────────────────

    /**
     * Compute linear regression (y = mx + b) via least squares.
     * @param pts   Array of data points
     * @param n     Number of points (must be >= 2)
     * @return      Result with a=m, b=intercept, r2
     */
    static Result fitLinear(const Point* pts, size_t n);

    /**
     * Compute quadratic regression (y = ax^2 + bx + c) via least squares.
     * @param pts   Array of data points
     * @param n     Number of points (must be >= 3)
     * @return      Result with a, b, c, r2
     */
    static Result fitQuadratic(const Point* pts, size_t n);

    /**
     * Evaluate the regression at a given x.
     */
    static double evaluate(const Result& r, double x);
};
