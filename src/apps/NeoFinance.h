/**
 * NeoFinance.h — Time Value of Money (TVM) engine for NeoLanguage Phase 6.
 *
 * Implements the five TVM variables that every financial calculator needs:
 *
 *   N   — number of compounding periods
 *   I/Y — interest rate per period (as a percentage, e.g. 5 means 5%)
 *   PV  — present value
 *   PMT — payment per period (ordinary annuity)
 *   FV  — future value
 *
 * Sign convention (Cash Flow Positive):
 *   Money received is positive (+); money paid is negative (–).
 *
 * TVM equation (standard financial formula):
 *   PV*(1+r)^n + PMT*[(1+r)^n - 1]/r + FV = 0
 *   where r = I_Y / 100.
 *
 * All functions accept the four known variables and solve for the fifth.
 *
 * Additional:
 *   amort_table(principal, rate_pct, periods)
 *     Returns a List of Lists: [[period, payment, interest, principal, balance], ...]
 *     Useful for mortgage / loan amortisation schedules.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 6 (Financial Math)
 */
#pragma once

#include <cmath>
#include <limits>
#include "NeoValue.h"

namespace NeoFinance {

// ════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════

/// Compute (1+r)^n efficiently.
inline double compound(double r, double n) {
    if (r == 0.0) return 1.0;
    return std::pow(1.0 + r, n);
}

/// Present value annuity factor: [(1+r)^n - 1] / r
/// Returns n when r == 0 (limiting case).
inline double pvAnnuityFactor(double r, double n) {
    if (r == 0.0) return n;
    return (compound(r, n) - 1.0) / r;
}

// ════════════════════════════════════════════════════════════════════
// Core TVM solvers
// ════════════════════════════════════════════════════════════════════

/**
 * Solve for Present Value.
 * @param rate_pct  Interest rate per period as a PERCENTAGE (e.g. 5 = 5%).
 * @param n         Number of periods.
 * @param pmt       Payment per period (annuity).
 * @param fv        Future value.
 * @return Present value (positive = cash inflow).
 */
inline double solvePV(double rate_pct, double n, double pmt, double fv) {
    double r = rate_pct / 100.0;
    double cf = compound(r, n);
    if (r == 0.0) return -(pmt * n + fv);
    return -(pmt * pvAnnuityFactor(r, n) + fv) / cf;
}

/**
 * Solve for Future Value.
 */
inline double solveFV(double rate_pct, double n, double pmt, double pv) {
    double r = rate_pct / 100.0;
    double cf = compound(r, n);
    if (r == 0.0) return -(pv + pmt * n);
    return -(pv * cf + pmt * pvAnnuityFactor(r, n));
}

/**
 * Solve for Payment per period.
 */
inline double solvePMT(double rate_pct, double n, double pv, double fv) {
    double r = rate_pct / 100.0;
    double cf = compound(r, n);
    if (r == 0.0) {
        if (n == 0.0) return 0.0;
        return -(pv + fv) / n;
    }
    double pva = pvAnnuityFactor(r, n);
    return -(pv * cf + fv) / pva;
}

/**
 * Solve for Number of periods.
 * Uses the analytical formula; returns 0 if pmt+pv*r = 0.
 */
inline double solveN(double rate_pct, double pmt, double pv, double fv) {
    double r = rate_pct / 100.0;
    if (r == 0.0) {
        if (pmt == 0.0) return 0.0;
        return -(pv + fv) / pmt;
    }
    double num = -fv * r + pmt;
    double den =  pv * r + pmt;
    if (den == 0.0) return 0.0;
    return std::log(num / den) / std::log(1.0 + r);
}

/**
 * Solve for Interest Rate per period (% per period).
 * Uses Newton-Raphson iteration (no closed-form solution).
 * Returns NaN on non-convergence.
 */
inline double solveIR(double n, double pmt, double pv, double fv) {
    // Initial guess
    double r = (pmt != 0.0) ? (pmt / (pv == 0.0 ? 1.0 : -pv)) : 0.1;
    if (!std::isfinite(r) || r <= -1.0) r = 0.05;

    static constexpr int    MAX_ITER = 200;
    static constexpr double TOL      = 1e-9;

    for (int i = 0; i < MAX_ITER; ++i) {
        double cf   = compound(r, n);
        double pva  = pvAnnuityFactor(r, n);
        double f    = pv * cf + pmt * pva + fv;

        // Derivative df/dr
        double dcf  = n * std::pow(1.0 + r, n - 1.0);
        double dpva = (r == 0.0) ? 0.0
                                 : (dcf / r - (cf - 1.0) / (r * r));
        double df   = pv * dcf + pmt * dpva;

        if (df == 0.0) break;
        double delta = f / df;
        r -= delta;
        if (r <= -1.0) r = -0.9999; // clamp to avoid log(0)
        if (std::fabs(delta) < TOL) return r * 100.0;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// ════════════════════════════════════════════════════════════════════
// Amortisation schedule
// ════════════════════════════════════════════════════════════════════

/**
 * Build a full amortisation table.
 *
 * @param principal  Loan amount (positive).
 * @param rate_pct   Annual interest rate as a PERCENTAGE (e.g. 5 = 5%).
 *                   Divided by 12 internally for monthly periods.
 * @param periods    Total number of monthly periods.
 * @return           NeoValue List of rows, each row = List of 5 Numbers:
 *                   [period, payment, interest_portion, principal_portion, balance]
 */
inline NeoValue amortTable(double principal, double rate_pct, int periods) {
    auto* rows = new std::vector<NeoValue>();
    rows->reserve(static_cast<size_t>(periods));

    double r = (rate_pct / 100.0) / 12.0;  // monthly rate
    double pmt = (r == 0.0)
        ? principal / periods
        : principal * r * std::pow(1.0 + r, periods)
          / (std::pow(1.0 + r, periods) - 1.0);

    double balance = principal;

    for (int p = 1; p <= periods; ++p) {
        double interest   = balance * r;
        double principal_p = pmt - interest;
        balance          -= principal_p;
        if (p == periods) balance = 0.0; // eliminate floating-point residual

        auto* row = new std::vector<NeoValue>();
        row->push_back(NeoValue::makeNumber(static_cast<double>(p)));
        row->push_back(NeoValue::makeNumber(pmt));
        row->push_back(NeoValue::makeNumber(interest));
        row->push_back(NeoValue::makeNumber(principal_p));
        row->push_back(NeoValue::makeNumber(balance));
        rows->push_back(NeoValue::makeList(row));
    }
    return NeoValue::makeList(rows);
}

} // namespace NeoFinance
