/**
 * SystemSolver.h — Systems of Linear Equations solver for CAS-Lite.
 *
 * Supports 2x2 and 3x3 systems with exact arithmetic (ExactVal).
 *
 * 2x2 Solving Methods:
 *   · Substitution — Isolate a variable (preferring ±1 coefficients)
 *   · Reduction    — Multiply & add/subtract to eliminate a variable
 *   · Heuristic engine selects the best method automatically
 *
 * 3x3 Solving Method:
 *   · Gaussian Elimination — Forward elimination + back substitution
 *
 * Every algebraic step is logged in Spanish via CASStepLogger.
 *
 * Part of: NumOS CAS-Lite — Phase D (SystemSolver)
 */

#pragma once

#include <cstdint>
#include <string>
#include "CASStepLogger.h"
#include "SymEquation.h"
#include "SymPoly.h"
#include "../ExactVal.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// LinEq — A single linear equation: coeffs[0]*v0 + coeffs[1]*v1 [+ coeffs[2]*v2] = rhs
// ════════════════════════════════════════════════════════════════════

struct LinEq {
    vpam::ExactVal coeffs[3];   // Variable coefficients (index 0,1,2)
    vpam::ExactVal rhs;         // Right-hand side constant

    LinEq() : rhs(vpam::ExactVal::fromInt(0)) {
        coeffs[0] = vpam::ExactVal::fromInt(0);
        coeffs[1] = vpam::ExactVal::fromInt(0);
        coeffs[2] = vpam::ExactVal::fromInt(0);
    }

    /// Factory for 2-variable equation: a*v0 + b*v1 = c
    static LinEq from2(int64_t a, int64_t b, int64_t c) {
        LinEq eq;
        eq.coeffs[0] = vpam::ExactVal::fromInt(a);
        eq.coeffs[1] = vpam::ExactVal::fromInt(b);
        eq.rhs       = vpam::ExactVal::fromInt(c);
        return eq;
    }

    /// Factory for 2-variable equation with ExactVal
    static LinEq from2v(vpam::ExactVal a, vpam::ExactVal b, vpam::ExactVal c) {
        LinEq eq;
        eq.coeffs[0] = a;
        eq.coeffs[1] = b;
        eq.rhs       = c;
        return eq;
    }

    /// Factory for 3-variable equation: a*v0 + b*v1 + c*v2 = d
    static LinEq from3(int64_t a, int64_t b, int64_t c, int64_t d) {
        LinEq eq;
        eq.coeffs[0] = vpam::ExactVal::fromInt(a);
        eq.coeffs[1] = vpam::ExactVal::fromInt(b);
        eq.coeffs[2] = vpam::ExactVal::fromInt(c);
        eq.rhs       = vpam::ExactVal::fromInt(d);
        return eq;
    }
};

// ════════════════════════════════════════════════════════════════════
// SystemMethod — Which algorithm was selected / used
// ════════════════════════════════════════════════════════════════════

enum class SystemMethod : uint8_t {
    Substitution = 0,
    Reduction    = 1,
    Gauss        = 2,
};

// ════════════════════════════════════════════════════════════════════
// SystemResult — Return value from SystemSolver
// ════════════════════════════════════════════════════════════════════

struct SystemResult {
    vpam::ExactVal solutions[3];   // Values for each variable
    char           vars[3];        // Variable names ('x','y','z')
    uint8_t        numVars  = 0;   // 2 or 3
    bool           ok       = false;
    std::string    error;
    SystemMethod   methodUsed = SystemMethod::Substitution;
    CASStepLogger  steps;
};

// ════════════════════════════════════════════════════════════════════
// SystemSolver — Systems of linear equations solver
// ════════════════════════════════════════════════════════════════════

class SystemSolver {
public:
    SystemSolver();

    /// Solve a 2×2 system of linear equations.
    /// Variables default to x, y.
    SystemResult solve2x2(const LinEq& eq1, const LinEq& eq2,
                          char var1 = 'x', char var2 = 'y');

    /// Solve a 3×3 system via Gaussian Elimination.
    /// Variables default to x, y, z.
    SystemResult solve3x3(const LinEq& eq1, const LinEq& eq2, const LinEq& eq3,
                          char var1 = 'x', char var2 = 'y', char var3 = 'z');

    /// Analyze a 2×2 system and choose the best method.
    /// Exposed for testing.
    SystemMethod analyzeAndChoose(const LinEq& eq1, const LinEq& eq2);

private:
    // ── 2x2 solver methods ──────────────────────────────────────────

    bool solveBySubstitution(const LinEq& eq1, const LinEq& eq2,
                             char v1, char v2, SystemResult& result);

    bool solveByReduction(const LinEq& eq1, const LinEq& eq2,
                          char v1, char v2, SystemResult& result);

    // ── 3x3 solver ─────────────────────────────────────────────────

    bool solveByGauss(const LinEq& eq1, const LinEq& eq2, const LinEq& eq3,
                      char v1, char v2, char v3, SystemResult& result);

    // ── Helpers ─────────────────────────────────────────────────────

    /// Format a LinEq as a human-readable string: "2x + y = 5"
    std::string eqToString(const LinEq& eq, const char* vars, int numVars);

    /// Build a SymEquation snapshot from a LinEq (for step logger).
    SymEquation eqToSymEquation(const LinEq& eq, const char* vars, int numVars);

    /// Check if an ExactVal is ±1 (integer)
    static bool isUnitCoeff(const vpam::ExactVal& v);

    /// Check if b is a multiple of a (a divides b evenly)
    static bool isDivisor(const vpam::ExactVal& a, const vpam::ExactVal& b);
};

} // namespace cas
