/**
 * NeoStats.cpp — Professional Statistics & Regression engine for NeoLanguage.
 *
 * See NeoStats.h for the public API.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 5 (Data Science)
 */

#include "NeoStats.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ════════════════════════════════════════════════════════════════════
// NeoRegModel::predict
// ════════════════════════════════════════════════════════════════════

double NeoRegModel::predict(double x) const {
    switch (kind) {
        case Kind::Linear: return a + b * x;
        case Kind::Quad:   return a + b * x + c * x * x;
        case Kind::Exp:    return a * std::exp(b * x);
        case Kind::Log:    return (x > 0.0) ? a + b * std::log(x) : std::numeric_limits<double>::quiet_NaN();
    }
    return 0.0;
}

// ════════════════════════════════════════════════════════════════════
// Helpers — extract doubles from a NeoValue list
// ════════════════════════════════════════════════════════════════════

static std::vector<double> toDoubles(const std::vector<NeoValue>& list) {
    std::vector<double> out;
    out.reserve(list.size());
    for (const auto& v : list) out.push_back(v.toDouble());
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Descriptive statistics
// ════════════════════════════════════════════════════════════════════

double NeoStats::mean(const std::vector<NeoValue>& list) {
    if (list.empty()) return std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    for (const auto& v : list) s += v.toDouble();
    return s / static_cast<double>(list.size());
}

double NeoStats::median(const std::vector<NeoValue>& list) {
    if (list.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::vector<double> sorted = sortedCopy(list);
    size_t n = sorted.size();
    if (n % 2 == 1) {
        return sorted[n / 2];
    } else {
        return (sorted[n / 2 - 1] + sorted[n / 2]) * 0.5;
    }
}

double NeoStats::variance(const std::vector<NeoValue>& list) {
    if (list.size() <= 1) return 0.0;
    double m = mean(list);
    double s = 0.0;
    for (const auto& v : list) {
        double d = v.toDouble() - m;
        s += d * d;
    }
    return s / static_cast<double>(list.size() - 1);
}

double NeoStats::stddev(const std::vector<NeoValue>& list) {
    return std::sqrt(variance(list));
}

std::vector<double> NeoStats::sortedCopy(const std::vector<NeoValue>& list) {
    std::vector<double> out = toDoubles(list);
    std::sort(out.begin(), out.end());
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Regression helpers
// ════════════════════════════════════════════════════════════════════

double NeoStats::computeR2(const std::vector<double>& ys,
                            const std::vector<double>& pred) {
    if (ys.empty()) return 0.0;
    double yMean = 0.0;
    for (double y : ys) yMean += y;
    yMean /= static_cast<double>(ys.size());

    double ssTot = 0.0, ssRes = 0.0;
    for (size_t i = 0; i < ys.size(); ++i) {
        double diff = ys[i] - yMean;
        ssTot += diff * diff;
        double r = ys[i] - pred[i];
        ssRes += r * r;
    }
    if (ssTot < 1e-15) return 1.0;
    return 1.0 - ssRes / ssTot;
}

bool NeoStats::solve2x2(double a00, double a01,
                         double a10, double a11,
                         double b0,  double b1,
                         double& x0, double& x1) {
    double det = a00 * a11 - a01 * a10;
    if (std::fabs(det) < 1e-15) return false;
    x0 = (b0 * a11 - b1 * a01) / det;
    x1 = (a00 * b1 - a10 * b0) / det;
    return true;
}

bool NeoStats::solve3x3(double a[3][3], double b[3], double x[3]) {
    // Gaussian elimination with partial pivoting
    double aug[3][4];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) aug[i][j] = a[i][j];
        aug[i][3] = b[i];
    }
    for (int col = 0; col < 3; ++col) {
        // Find pivot
        int pivot = col;
        for (int row = col + 1; row < 3; ++row) {
            if (std::fabs(aug[row][col]) > std::fabs(aug[pivot][col])) pivot = row;
        }
        // Swap rows
        for (int j = 0; j <= 3; ++j) {
            double tmp = aug[col][j];
            aug[col][j] = aug[pivot][j];
            aug[pivot][j] = tmp;
        }
        if (std::fabs(aug[col][col]) < 1e-15) return false;
        // Eliminate
        for (int row = col + 1; row < 3; ++row) {
            double factor = aug[row][col] / aug[col][col];
            for (int j = col; j <= 3; ++j)
                aug[row][j] -= factor * aug[col][j];
        }
    }
    // Back-substitution
    for (int i = 2; i >= 0; --i) {
        x[i] = aug[i][3];
        for (int j = i + 1; j < 3; ++j) x[i] -= aug[i][j] * x[j];
        x[i] /= aug[i][i];
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════
// regressLinear — OLS: a + b·x
// ════════════════════════════════════════════════════════════════════

bool NeoStats::regressLinear(const std::vector<double>& xs,
                              const std::vector<double>& ys,
                              NeoRegModel& out) {
    size_t n = xs.size();
    if (n < 2) return false;

    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (size_t i = 0; i < n; ++i) {
        sumX  += xs[i];
        sumY  += ys[i];
        sumXY += xs[i] * ys[i];
        sumX2 += xs[i] * xs[i];
    }
    double dn = static_cast<double>(n);
    double denom = dn * sumX2 - sumX * sumX;
    if (std::fabs(denom) < 1e-15) return false;

    out.kind = NeoRegModel::Kind::Linear;
    out.b = (dn * sumXY - sumX * sumY) / denom;
    out.a = (sumY - out.b * sumX) / dn;
    out.c = 0.0;

    std::vector<double> pred;
    pred.reserve(n);
    for (size_t i = 0; i < n; ++i) pred.push_back(out.predict(xs[i]));
    out.r2 = computeR2(ys, pred);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// regressQuad — OLS: a + b·x + c·x²
// ════════════════════════════════════════════════════════════════════

bool NeoStats::regressQuad(const std::vector<double>& xs,
                            const std::vector<double>& ys,
                            NeoRegModel& out) {
    size_t n = xs.size();
    if (n < 3) return false;

    double s0 = n, s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    double t0 = 0, t1 = 0, t2 = 0;
    for (size_t i = 0; i < n; ++i) {
        double xi = xs[i], yi = ys[i];
        double xi2 = xi * xi;
        s1 += xi; s2 += xi2; s3 += xi2 * xi; s4 += xi2 * xi2;
        t0 += yi; t1 += xi * yi; t2 += xi2 * yi;
    }
    double A[3][3] = { {s0, s1, s2}, {s1, s2, s3}, {s2, s3, s4} };
    double b[3]    = { t0, t1, t2 };
    double x[3]    = { 0, 0, 0 };
    if (!solve3x3(A, b, x)) return false;

    out.kind = NeoRegModel::Kind::Quad;
    out.a = x[0]; out.b = x[1]; out.c = x[2];

    std::vector<double> pred;
    pred.reserve(n);
    for (size_t i = 0; i < n; ++i) pred.push_back(out.predict(xs[i]));
    out.r2 = computeR2(ys, pred);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// regressExp — a·e^(b·x)  → linearise: ln(y) = ln(a) + b·x
// ════════════════════════════════════════════════════════════════════

bool NeoStats::regressExp(const std::vector<double>& xs,
                           const std::vector<double>& ys,
                           NeoRegModel& out) {
    size_t n = xs.size();
    if (n < 2) return false;

    // Need all y > 0 for linearisation
    std::vector<double> lys;
    lys.reserve(n);
    for (double y : ys) {
        if (y <= 0.0) return false;
        lys.push_back(std::log(y));
    }

    NeoRegModel linModel;
    // Re-use linear fit on (xs, lys)
    std::vector<NeoValue> xVals, lyVals;
    xVals.reserve(n); lyVals.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        xVals.push_back(NeoValue::makeNumber(xs[i]));
        lyVals.push_back(NeoValue::makeNumber(lys[i]));
    }
    if (!regressLinear(xs, lys, linModel)) return false;

    out.kind = NeoRegModel::Kind::Exp;
    out.a    = std::exp(linModel.a);
    out.b    = linModel.b;
    out.c    = 0.0;

    std::vector<double> pred;
    pred.reserve(n);
    for (size_t i = 0; i < n; ++i) pred.push_back(out.predict(xs[i]));
    out.r2 = computeR2(ys, pred);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// regressLog — a + b·ln(x)  → linearise: y = a + b·ln(x)
// ════════════════════════════════════════════════════════════════════

bool NeoStats::regressLog(const std::vector<double>& xs,
                           const std::vector<double>& ys,
                           NeoRegModel& out) {
    size_t n = xs.size();
    if (n < 2) return false;

    std::vector<double> lxs;
    lxs.reserve(n);
    for (double x : xs) {
        if (x <= 0.0) return false;
        lxs.push_back(std::log(x));
    }

    NeoRegModel linModel;
    if (!regressLinear(lxs, ys, linModel)) return false;

    out.kind = NeoRegModel::Kind::Log;
    out.a    = linModel.a;
    out.b    = linModel.b;
    out.c    = 0.0;

    std::vector<double> pred;
    pred.reserve(n);
    for (size_t i = 0; i < n; ++i) pred.push_back(out.predict(xs[i]));
    out.r2 = computeR2(ys, pred);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// regress — public dispatcher
// ════════════════════════════════════════════════════════════════════

bool NeoStats::regress(const std::vector<NeoValue>& x_data,
                       const std::vector<NeoValue>& y_data,
                       NeoRegModel::Kind            kind,
                       NeoRegModel&                 out) {
    if (x_data.size() != y_data.size() || x_data.empty()) return false;

    std::vector<double> xs = toDoubles(x_data);
    std::vector<double> ys = toDoubles(y_data);

    switch (kind) {
        case NeoRegModel::Kind::Linear: return regressLinear(xs, ys, out);
        case NeoRegModel::Kind::Quad:   return regressQuad  (xs, ys, out);
        case NeoRegModel::Kind::Exp:    return regressExp   (xs, ys, out);
        case NeoRegModel::Kind::Log:    return regressLog   (xs, ys, out);
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════
// Probability distributions
// ════════════════════════════════════════════════════════════════════

double NeoStats::pdfNormal(double x, double mu, double sigma) {
    if (sigma <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    double z = (x - mu) / sigma;
    return std::exp(-0.5 * z * z) / (sigma * std::sqrt(2.0 * M_PI));
}

double NeoStats::cdfNormal(double x, double mu, double sigma) {
    if (sigma <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    double z = (x - mu) / (sigma * std::sqrt(2.0));
    return 0.5 * (1.0 + std::erf(z));
}

// ════════════════════════════════════════════════════════════════════
// Combinatorics
// ════════════════════════════════════════════════════════════════════

double NeoStats::factorial(int n) {
    if (n < 0)   return std::numeric_limits<double>::quiet_NaN();
    if (n == 0)  return 1.0;
    if (n > 170) return std::numeric_limits<double>::infinity();
    double result = 1.0;
    for (int i = 2; i <= n; ++i) result *= static_cast<double>(i);
    return result;
}

double NeoStats::ncr(int n, int r) {
    if (r < 0 || r > n) return 0.0;
    if (r == 0 || r == n) return 1.0;
    // Use the smaller of r and n-r to minimise computation
    if (r > n - r) r = n - r;
    double result = 1.0;
    for (int i = 0; i < r; ++i) {
        result *= static_cast<double>(n - i);
        result /= static_cast<double>(i + 1);
    }
    return std::round(result);
}

double NeoStats::npr(int n, int r) {
    if (r < 0 || r > n) return 0.0;
    double result = 1.0;
    for (int i = n; i > n - r; --i) result *= static_cast<double>(i);
    return result;
}
