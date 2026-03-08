/**
 * SystemSolver.cpp — Implementation of the systems-of-linear-equations solver.
 *
 * Pipeline for 2x2:
 *   1. analyzeAndChoose() — score Substitution vs Reduction
 *   2. Dispatch to solveBySubstitution() or solveByReduction()
 *   3. Log every step in Spanish
 *
 * Pipeline for 3x3:
 *   1. Forward elimination (Gaussian) with pivot swapping
 *   2. Back substitution
 *   3. Log every step in Spanish
 *
 * All arithmetic uses ExactVal for exact fractions/radicals.
 *
 * Part of: NumOS CAS-Lite — Phase D
 */

#include "SystemSolver.h"
#include "SymPolyMulti.h"
#include "OmniSolver.h"
#include "SymSimplify.h"
#include "TutorTemplates.h"
#include <cmath>
#include <cstdlib>

namespace cas {

using vpam::ExactVal;
using vpam::exactAdd;
using vpam::exactSub;
using vpam::exactMul;
using vpam::exactDiv;
using vpam::exactNeg;

// ────────────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────────────

SystemSolver::SystemSolver() {}

// ════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════

bool SystemSolver::isUnitCoeff(const ExactVal& v) {
    return v.ok && v.den == 1 && (v.num == 1 || v.num == -1) &&
           v.inner == 1 && v.piMul == 0 && v.eMul == 0;
}

bool SystemSolver::isDivisor(const ExactVal& a, const ExactVal& b) {
    // Check if a divides b evenly (for integer coefficients)
    if (!a.isRational() || !b.isRational()) return false;
    if (a.isZero()) return false;
    // a = aN/aD, b = bN/bD
    // b/a = (bN*aD) / (bD*aN) — integer if bN*aD % (bD*aN) == 0
    int64_t num = b.num * a.den;
    int64_t den = b.den * a.num;
    if (den == 0) return false;
    return (num % den) == 0;
}

std::string SystemSolver::eqToString(const LinEq& eq, const char* vars, int numVars) {
    std::string result;

    for (int i = 0; i < numVars; ++i) {
        if (eq.coeffs[i].isZero()) continue;

        int64_t n = eq.coeffs[i].num;
        int64_t d = eq.coeffs[i].den;
        int64_t absN = (n < 0) ? -n : n;

        if (result.empty()) {
            // First term
            if (n < 0) result += "-";
            if (absN != 1 || d != 1) {
                result += std::to_string(absN);
                if (d != 1) result += "/" + std::to_string(d);
            }
            result += vars[i];
        } else {
            // Subsequent terms
            if (n < 0) {
                result += " - ";
            } else {
                result += " + ";
            }
            if (absN != 1 || d != 1) {
                result += std::to_string(absN);
                if (d != 1) result += "/" + std::to_string(d);
            }
            result += vars[i];
        }
    }

    if (result.empty()) result = "0";

    result += " = ";
    if (eq.rhs.den == 1) {
        result += std::to_string(eq.rhs.num);
    } else {
        result += std::to_string(eq.rhs.num) + "/" + std::to_string(eq.rhs.den);
    }

    return result;
}

SymEquation SystemSolver::eqToSymEquation(const LinEq& eq, const char* vars, int numVars) {
    SymPoly lhs;
    for (int i = 0; i < numVars; ++i) {
        if (!eq.coeffs[i].isZero()) {
            lhs.terms().push_back(SymTerm(eq.coeffs[i], vars[i], 1));
        }
    }
    // Don't normalize — preserve variable insertion order for display
    SymPoly rhs = SymPoly::fromConstant(eq.rhs);
    return SymEquation(lhs, rhs);
}

// ════════════════════════════════════════════════════════════════════
// analyzeAndChoose — Heuristic engine for 2x2 method selection
// ════════════════════════════════════════════════════════════════════

SystemMethod SystemSolver::analyzeAndChoose(const LinEq& eq1, const LinEq& eq2) {
    int reductionScore    = 0;
    int substitutionScore = 0;

    for (int i = 0; i < 2; ++i) {
        const ExactVal& c1 = eq1.coeffs[i];
        const ExactVal& c2 = eq2.coeffs[i];

        // ── Reduction scoring ────────────────────────────────────
        // Same absolute value → just add or subtract (no multiplication)
        if (c1.isRational() && c2.isRational() && c1.den == c2.den) {
            if (c1.num == c2.num)  reductionScore += 10;   // e.g., 3y and 3y → subtract
            if (c1.num == -c2.num) reductionScore += 10;   // e.g., 3y and -3y → add
        }

        // One is a multiple of the other → one multiplication
        if (isDivisor(c1, c2) || isDivisor(c2, c1)) {
            reductionScore += 5;
        }

        // ── Substitution scoring ─────────────────────────────────
        // Coefficient of ±1 → easy to isolate
        if (isUnitCoeff(c1)) substitutionScore += 8;
        if (isUnitCoeff(c2)) substitutionScore += 8;
    }

    // Default to Substitution if tied or if no clear advantage
    if (reductionScore > substitutionScore) {
        return SystemMethod::Reduction;
    }
    return SystemMethod::Substitution;
}

// ════════════════════════════════════════════════════════════════════
// solve2x2 — Main entry for 2×2 systems
// ════════════════════════════════════════════════════════════════════

SystemResult SystemSolver::solve2x2(const LinEq& eq1, const LinEq& eq2,
                                     char var1, char var2, SymExprArena* arena) {
    if (arena) {
        return solveSystem2x2Tutor(eq1, eq2, var1, var2, arena);
    }
    
    SystemResult result;
    result.numVars = 2;
    result.vars[0] = var1;
    result.vars[1] = var2;
    result.ok      = false;

    char vars[3] = { var1, var2, '\0' };

    // Log original system
    result.steps.logNote("System of equations 2x2:", MethodId::General);
    result.steps.log("  Eq.1: " + eqToString(eq1, vars, 2),
                     eqToSymEquation(eq1, vars, 2), MethodId::General);
    result.steps.log("  Eq.2: " + eqToString(eq2, vars, 2),
                     eqToSymEquation(eq2, vars, 2), MethodId::General);

    // Choose method
    SystemMethod method = analyzeAndChoose(eq1, eq2);
    result.methodUsed = method;

    bool success = false;
    if (method == SystemMethod::Reduction) {
        result.steps.logNote("Method selected: Reduction (coefficient matching)",
                             MethodId::Reduction);
        success = solveByReduction(eq1, eq2, var1, var2, result);
    } else {
        result.steps.logNote("Method selected: Substitution",
                             MethodId::Substitution);
        success = solveBySubstitution(eq1, eq2, var1, var2, result);
    }

    result.ok = success;
    return result;
}

// ════════════════════════════════════════════════════════════════════
// solveBySubstitution — Isolate one variable, substitute into other
// ════════════════════════════════════════════════════════════════════

bool SystemSolver::solveBySubstitution(const LinEq& eq1, const LinEq& eq2,
                                       char v1, char v2, SystemResult& result) {
    const MethodId M = MethodId::Substitution;
    char vars[3] = { v1, v2, '\0' };

    // ── Step 1: Choose which equation and variable to isolate ────
    // Prefer the coefficient closest to ±1
    int isoEq  = 0;   // Index of equation to isolate from (0 or 1)
    int isoVar = 0;   // Index of variable to isolate (0 or 1)

    const LinEq* eqs[2] = { &eq1, &eq2 };

    // Scan for ±1 coefficients
    bool found = false;
    for (int e = 0; e < 2 && !found; ++e) {
        for (int v = 0; v < 2 && !found; ++v) {
            if (isUnitCoeff(eqs[e]->coeffs[v])) {
                isoEq  = e;
                isoVar = v;
                found  = true;
            }
        }
    }

    int otherEq  = 1 - isoEq;
    int otherVar = 1 - isoVar;

    const LinEq& iEq = *eqs[isoEq];
    const LinEq& oEq = *eqs[otherEq];

    // ── Step 2: Isolate variable ─────────────────────────────────
    // From iEq: coeffs[isoVar]*var[isoVar] + coeffs[otherVar]*var[otherVar] = rhs
    // var[isoVar] = (rhs - coeffs[otherVar]*var[otherVar]) / coeffs[isoVar]

    result.steps.logNote(
        "Isolating " + std::string(1, vars[isoVar]) +
        " in Equation " + std::to_string(isoEq + 1), M);

    // ── Step 3: Substitute into the other equation ───────────────
    // oEq: coeffs[isoVar]*var[isoVar] + coeffs[otherVar]*var[otherVar] = rhs
    // Replace var[isoVar]:
    //   factor = oEq.coeffs[isoVar] / iEq.coeffs[isoVar]
    //   newCoeff = oEq.coeffs[otherVar] - factor * iEq.coeffs[otherVar]
    //   newRhs   = oEq.rhs - factor * iEq.rhs

    ExactVal factor   = exactDiv(oEq.coeffs[isoVar], iEq.coeffs[isoVar]);
    factor.simplify();

    ExactVal newCoeff = exactSub(oEq.coeffs[otherVar],
                                 exactMul(factor, iEq.coeffs[otherVar]));
    newCoeff.simplify();

    ExactVal newRhs   = exactSub(oEq.rhs, exactMul(factor, iEq.rhs));
    newRhs.simplify();

    result.steps.logNote(
        "Substituting into Equation " + std::to_string(otherEq + 1), M);

    // Check for zero coefficient (no solution or infinite solutions)
    if (newCoeff.isZero()) {
        if (newRhs.isZero()) {
            result.steps.logNote("Dependent system: infinite solutions", M);
            result.error = "Dependent system";
        } else {
            result.steps.logNote("Incompatible system: no solution", M);
            result.error = "No solution (incompatible system)";
        }
        return false;
    }

    // ── Step 4: Solve for the other variable ─────────────────────
    ExactVal solOther = exactDiv(newRhs, newCoeff);
    solOther.simplify();

    {
        // Log the reduced single-variable equation
        LinEq reduced;
        reduced.coeffs[otherVar] = newCoeff;
        reduced.rhs = newRhs;
        result.steps.log(
            "Solving for " + std::string(1, vars[otherVar]),
            eqToSymEquation(reduced, vars, 2), M);
    }

    // Log the solution
    {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(vars[otherVar], 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(solOther);
        result.steps.log(
            std::string(1, vars[otherVar]) + " = " +
            std::to_string(solOther.num) +
            (solOther.den != 1 ? "/" + std::to_string(solOther.den) : ""),
            SymEquation(lhs, rhs), M);
    }

    // ── Step 5: Back-substitute to find the isolated variable ────
    // var[isoVar] = (rhs - coeffs[otherVar]*solOther) / coeffs[isoVar]
    ExactVal solIso = exactDiv(
        exactSub(iEq.rhs, exactMul(iEq.coeffs[otherVar], solOther)),
        iEq.coeffs[isoVar]);
    solIso.simplify();

    {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(vars[isoVar], 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(solIso);
        result.steps.log(
            "Substituting " + std::string(1, vars[otherVar]) +
            " to find " + std::string(1, vars[isoVar]),
            SymEquation(lhs, rhs), M);
    }

    // Store solutions in correct order (index 0 = var1, index 1 = var2)
    result.solutions[isoVar]  = solIso;
    result.solutions[otherVar] = solOther;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solveByReduction — Multiply & combine to eliminate a variable
// ════════════════════════════════════════════════════════════════════

bool SystemSolver::solveByReduction(const LinEq& eq1, const LinEq& eq2,
                                     char v1, char v2, SystemResult& result) {
    const MethodId M = MethodId::Reduction;
    char vars[3] = { v1, v2, '\0' };

    // ── Choose which variable to eliminate ───────────────────────
    // Prefer the variable whose coefficients are already matched or one divides the other
    int elimVar = 0;  // Default: eliminate first variable

    // Check if second variable is easier to eliminate
    {
        int score0 = 0, score1 = 0;
        // Variable 0
        if (eq1.coeffs[0].num == eq2.coeffs[0].num &&
            eq1.coeffs[0].den == eq2.coeffs[0].den) score0 += 10;
        if (eq1.coeffs[0].num == -eq2.coeffs[0].num &&
            eq1.coeffs[0].den == eq2.coeffs[0].den) score0 += 10;
        if (isDivisor(eq1.coeffs[0], eq2.coeffs[0]) ||
            isDivisor(eq2.coeffs[0], eq1.coeffs[0])) score0 += 5;

        // Variable 1
        if (eq1.coeffs[1].num == eq2.coeffs[1].num &&
            eq1.coeffs[1].den == eq2.coeffs[1].den) score1 += 10;
        if (eq1.coeffs[1].num == -eq2.coeffs[1].num &&
            eq1.coeffs[1].den == eq2.coeffs[1].den) score1 += 10;
        if (isDivisor(eq1.coeffs[1], eq2.coeffs[1]) ||
            isDivisor(eq2.coeffs[1], eq1.coeffs[1])) score1 += 5;

        if (score1 > score0) elimVar = 1;
    }

    int keepVar = 1 - elimVar;

    // ── Compute multipliers to match coefficients ────────────────
    // To eliminate var[elimVar]:
    //   Multiply eq1 by c2 = eq2.coeffs[elimVar]
    //   Multiply eq2 by c1 = eq1.coeffs[elimVar]
    //   Subtract: c2*eq1 - c1*eq2 → eliminates var[elimVar]

    ExactVal c1 = eq1.coeffs[elimVar];
    ExactVal c2 = eq2.coeffs[elimVar];

    // Check for zero coefficients
    if (c1.isZero() && c2.isZero()) {
        result.steps.logNote("Both coefficients of " + std::string(1, vars[elimVar]) +
                             " are zero", M);
        result.error = "Zero coefficients";
        return false;
    }

    // If one is zero, no need to eliminate — the other equation already lacks this variable
    if (c1.isZero()) {
        // eq1 already has no var[elimVar], just solve
        ExactVal solKeep = exactDiv(eq1.rhs, eq1.coeffs[keepVar]);
        solKeep.simplify();
        ExactVal solElim = exactDiv(
            exactSub(eq2.rhs, exactMul(eq2.coeffs[keepVar], solKeep)),
            eq2.coeffs[elimVar]);
        solElim.simplify();
        result.solutions[keepVar] = solKeep;
        result.solutions[elimVar] = solElim;
        return true;
    }
    if (c2.isZero()) {
        ExactVal solKeep = exactDiv(eq2.rhs, eq2.coeffs[keepVar]);
        solKeep.simplify();
        ExactVal solElim = exactDiv(
            exactSub(eq1.rhs, exactMul(eq1.coeffs[keepVar], solKeep)),
            eq1.coeffs[elimVar]);
        solElim.simplify();
        result.solutions[keepVar] = solKeep;
        result.solutions[elimVar] = solElim;
        return true;
    }

    // Log multiplication steps
    result.steps.logNote(
        "Multiplying Eq.1 by " + std::to_string(c2.num) +
        (c2.den != 1 ? "/" + std::to_string(c2.den) : "") +
        " to match coefficients of " + std::string(1, vars[elimVar]), M);

    // Build scaled equations for logging
    LinEq scaled1, scaled2;
    for (int i = 0; i < 2; ++i) {
        scaled1.coeffs[i] = exactMul(c2, eq1.coeffs[i]);
        scaled1.coeffs[i].simplify();
        scaled2.coeffs[i] = exactMul(c1, eq2.coeffs[i]);
        scaled2.coeffs[i].simplify();
    }
    scaled1.rhs = exactMul(c2, eq1.rhs);
    scaled1.rhs.simplify();
    scaled2.rhs = exactMul(c1, eq2.rhs);
    scaled2.rhs.simplify();

    result.steps.log("  Eq.1 * " + std::to_string(c2.num) +
                     (c2.den != 1 ? "/" + std::to_string(c2.den) : ""),
                     eqToSymEquation(scaled1, vars, 2), M);

    result.steps.logNote(
        "Multiplying Eq.2 by " + std::to_string(c1.num) +
        (c1.den != 1 ? "/" + std::to_string(c1.den) : ""), M);

    result.steps.log("  Eq.2 * " + std::to_string(c1.num) +
                     (c1.den != 1 ? "/" + std::to_string(c1.den) : ""),
                     eqToSymEquation(scaled2, vars, 2), M);

    // ── Subtract: c2*eq1 - c1*eq2 → eliminates var[elimVar] ─────
    LinEq combined;
    for (int i = 0; i < 2; ++i) {
        combined.coeffs[i] = exactSub(scaled1.coeffs[i], scaled2.coeffs[i]);
        combined.coeffs[i].simplify();
    }
    combined.rhs = exactSub(scaled1.rhs, scaled2.rhs);
    combined.rhs.simplify();

    result.steps.log(
        "Subtracting equations to eliminate " + std::string(1, vars[elimVar]),
        eqToSymEquation(combined, vars, 2), M);

    // ── Solve for keepVar ────────────────────────────────────────
    ExactVal keepCoeff = combined.coeffs[keepVar];

    if (keepCoeff.isZero()) {
        if (combined.rhs.isZero()) {
            result.steps.logNote("Dependent system: infinite solutions", M);
            result.error = "Dependent system";
        } else {
            result.steps.logNote("Incompatible system: no solution", M);
            result.error = "No solution (incompatible system)";
        }
        return false;
    }

    ExactVal solKeep = exactDiv(combined.rhs, keepCoeff);
    solKeep.simplify();

    {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(vars[keepVar], 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(solKeep);
        result.steps.log(
            "Solving for " + std::string(1, vars[keepVar]),
            SymEquation(lhs, rhs), M);
    }

    // ── Back-substitute into eq1 to find elimVar ─────────────────
    ExactVal solElim = exactDiv(
        exactSub(eq1.rhs, exactMul(eq1.coeffs[keepVar], solKeep)),
        eq1.coeffs[elimVar]);
    solElim.simplify();

    {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(vars[elimVar], 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(solElim);
        result.steps.log(
            "Substituting " + std::string(1, vars[keepVar]) +
            " in Eq.1 to find " + std::string(1, vars[elimVar]),
            SymEquation(lhs, rhs), M);
    }

    result.solutions[keepVar] = solKeep;
    result.solutions[elimVar] = solElim;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solve3x3 — Main entry for 3×3 systems
// ════════════════════════════════════════════════════════════════════

SystemResult SystemSolver::solve3x3(const LinEq& eq1, const LinEq& eq2,
                                     const LinEq& eq3,
                                     char var1, char var2, char var3) {
    SystemResult result;
    result.numVars = 3;
    result.vars[0] = var1;
    result.vars[1] = var2;
    result.vars[2] = var3;
    result.methodUsed = SystemMethod::Gauss;
    result.ok = false;

    char vars[4] = { var1, var2, var3, '\0' };

    // Log original system
    result.steps.logNote("System of equations 3x3:", MethodId::Gauss);
    result.steps.log("  Eq.1: " + eqToString(eq1, vars, 3),
                     eqToSymEquation(eq1, vars, 3), MethodId::Gauss);
    result.steps.log("  Eq.2: " + eqToString(eq2, vars, 3),
                     eqToSymEquation(eq2, vars, 3), MethodId::Gauss);
    result.steps.log("  Eq.3: " + eqToString(eq3, vars, 3),
                     eqToSymEquation(eq3, vars, 3), MethodId::Gauss);

    result.steps.logNote("Method: Gaussian elimination", MethodId::Gauss);

    result.ok = solveByGauss(eq1, eq2, eq3, var1, var2, var3, result);
    return result;
}

// ════════════════════════════════════════════════════════════════════
// solveByGauss — Gaussian Elimination with partial pivoting
// ════════════════════════════════════════════════════════════════════

bool SystemSolver::solveByGauss(const LinEq& eq1, const LinEq& eq2,
                                 const LinEq& eq3,
                                 char v1, char v2, char v3,
                                 SystemResult& result) {
    const MethodId M = MethodId::Gauss;
    char vars[4] = { v1, v2, v3, '\0' };

    // Build augmented matrix M[row][col], col 3 = RHS
    ExactVal mat[3][4];
    const LinEq* eqs[3] = { &eq1, &eq2, &eq3 };
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            mat[r][c] = eqs[r]->coeffs[c];
        }
        mat[r][3] = eqs[r]->rhs;
    }

    result.steps.logNote("Applying Gaussian elimination to triangulate the matrix", M);

    // ── Forward elimination ──────────────────────────────────────
    for (int col = 0; col < 3; ++col) {
        // Find pivot: non-zero in column col, starting from row col
        int pivotRow = -1;
        for (int r = col; r < 3; ++r) {
            if (!mat[r][col].isZero()) {
                pivotRow = r;
                break;
            }
        }

        if (pivotRow < 0) {
            result.steps.logNote(
                "Zero pivot in column " + std::string(1, vars[col]) +
                ": singular system", M);
            result.error = "Singular system (no unique solution)";
            return false;
        }

        // Swap rows if needed
        if (pivotRow != col) {
            for (int j = 0; j < 4; ++j) {
                ExactVal tmp = mat[col][j];
                mat[col][j] = mat[pivotRow][j];
                mat[pivotRow][j] = tmp;
            }
            result.steps.logNote(
                "Swapping Eq." + std::to_string(col + 1) +
                " with Eq." + std::to_string(pivotRow + 1), M);
        }

        // Eliminate below pivot
        for (int r = col + 1; r < 3; ++r) {
            if (mat[r][col].isZero()) continue;

            ExactVal factor = exactDiv(mat[r][col], mat[col][col]);
            factor.simplify();

            for (int j = col; j < 4; ++j) {
                mat[r][j] = exactSub(mat[r][j], exactMul(factor, mat[col][j]));
                mat[r][j].simplify();
            }

            // Log the elimination step
            {
                LinEq rowEq;
                for (int c2 = 0; c2 < 3; ++c2) rowEq.coeffs[c2] = mat[r][c2];
                rowEq.rhs = mat[r][3];

                result.steps.log(
                    "Eliminating " + std::string(1, vars[col]) +
                    " from Eq." + std::to_string(r + 1),
                    eqToSymEquation(rowEq, vars, 3), M);
            }
        }
    }

    // ── Check for singular matrix (zero on diagonal) ─────────────
    for (int i = 0; i < 3; ++i) {
        if (mat[i][i].isZero()) {
            result.steps.logNote("Zero pivot on diagonal: singular system", M);
            result.error = "Singular system (no unique solution)";
            return false;
        }
    }

    // ── Back substitution ────────────────────────────────────────
    ExactVal sol[3];

    for (int r = 2; r >= 0; --r) {
        ExactVal sum = mat[r][3];  // RHS
        for (int j = r + 1; j < 3; ++j) {
            sum = exactSub(sum, exactMul(mat[r][j], sol[j]));
        }
        sol[r] = exactDiv(sum, mat[r][r]);
        sol[r].simplify();

        // Log each back-substitution solution
        {
            SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(vars[r], 1, 1, 1));
            SymPoly rhs = SymPoly::fromConstant(sol[r]);
            result.steps.log(
                "Back substitution: " + std::string(1, vars[r]) +
                " = " + std::to_string(sol[r].num) +
                (sol[r].den != 1 ? "/" + std::to_string(sol[r].den) : ""),
                SymEquation(lhs, rhs), M);
        }
    }

    result.solutions[0] = sol[0];
    result.solutions[1] = sol[1];
    result.solutions[2] = sol[2];
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solveNonlinear2x2 — Sylvester resultant elimination for 2×2 systems
//
// Pipeline:
//   1. View f1, f2 as polynomials in var2 (y)
//   2. Compute Sylvester resultant → R(var1) = 0
//   3. Solve R(var1) via OmniSolver
//   4. Back-substitute each var1 solution → solve for var2
// ════════════════════════════════════════════════════════════════════

NLSystemResult SystemSolver::solveNonlinear2x2(SymExpr* f1, SymExpr* f2,
                                                char var1, char var2,
                                                SymExprArena& arena) {
    NLSystemResult result;
    result.var1 = var1;
    result.var2 = var2;
    result.numVars = 2;
    result.methodUsed = SystemMethod::Resultant;

    if (!f1 || !f2) {
        result.error = "Null equation";
        return result;
    }

    // ── Step 1: Simplify both equations ────────────────────────────
    f1 = SymSimplify::simplify(f1, arena);
    f2 = SymSimplify::simplify(f2, arena);

    result.steps.logNote(
        "Eliminating " + std::string(1, var2) +
        " by constructing the Sylvester matrix from both equations.",
        MethodId::General);
    result.steps.logNote(
        "Eq1: " + f1->toString() + " = 0", MethodId::General);
    result.steps.logNote(
        "Eq2: " + f2->toString() + " = 0", MethodId::General);

    // ── Step 2: Collect as univariate polynomials in var2 ──────────
    UniPoly P = collectAsUniPoly(f1, var2, arena);
    UniPoly Q = collectAsUniPoly(f2, var2, arena);

    if (P.isZero() || Q.isZero()) {
        result.error = "Could not extract polynomial structure in " +
                       std::string(1, var2);
        return result;
    }

    result.steps.logNote(
        "Eq1 is degree " + std::to_string(P.degree()) +
        " in " + std::string(1, var2) + ".",
        MethodId::General);
    result.steps.logNote(
        "Eq2 is degree " + std::to_string(Q.degree()) +
        " in " + std::string(1, var2) + ".",
        MethodId::General);

    // Degree safety check
    int totalDeg = P.degree() + Q.degree();
    if (totalDeg > 12) {
        result.error = "Total degree > 12: system too complex";
        return result;
    }

    // ── Step 3: Compute Sylvester resultant ────────────────────────
    result.steps.logNote(
        "Constructing the " + std::to_string(totalDeg) + "\u00D7" +
        std::to_string(totalDeg) +
        " Sylvester matrix to compute the resultant.",
        MethodId::General);

    SymExpr* R = sylvesterResultant(P, Q, arena);

    if (!R) {
        result.error = "Error computing the resultant";
        return result;
    }

    R = SymSimplify::simplify(R, arena);

    result.steps.logNote(
        "The resultant eliminates " + std::string(1, var2) +
        ", giving R(" + std::string(1, var1) + ") = " + R->toString() + ".",
        MethodId::General);

    // Check if resultant is zero (equations are not independent)
    if (R->type == SymExprType::Num) {
        auto* num = static_cast<SymNum*>(R);
        if (num->_coeff.isZero()) {
            result.error = "Resultant identically zero: equations are not independent";
            return result;
        }
    }

    // ── Step 4: Solve R(var1) = 0 ──────────────────────────────────
    result.steps.logNote(
        "Solving R(" + std::string(1, var1) + ") = 0 for " +
        std::string(1, var1) + ".",
        MethodId::General);

    // Check that R actually contains var1
    if (!R->containsVar(var1)) {
        // R is a constant — if non-zero, system is inconsistent
        result.error = "Incompatible system (constant non-zero resultant)";
        return result;
    }

    OmniSolver omni;
    SymExpr* zeroExpr = symInt(arena, 0);
    OmniResult omniRes = omni.solve(R, zeroExpr, var1, arena);

    if (!omniRes.ok || omniRes.solutions.empty()) {
        result.error = "No solutions found for " +
                       std::string(1, var1);
        return result;
    }

    result.steps.logNote(
        "Found " + std::to_string(omniRes.solutions.size()) +
        " value(s) for " + std::string(1, var1) + ".",
        MethodId::General);

    // ── Step 5: Back-substitution ──────────────────────────────────
    // For each solution of var1, substitute into f1 → solve for var2
    for (const auto& xSol : omniRes.solutions) {
        // Build the substitution value as a SymExpr
        SymExpr* xVal = nullptr;
        if (xSol.symbolic) {
            xVal = xSol.symbolic;
        } else if (xSol.isExact) {
            xVal = symNum(arena, xSol.exact);
        } else {
            // Use numeric approximation as a SymNum
            xVal = symNum(arena, vpam::ExactVal::fromDouble(xSol.numeric));
        }

        result.steps.logNote(
            "Back-substituting " + std::string(1, var1) + " = " +
            xVal->toString() + " into Eq1 to find " +
            std::string(1, var2) + ".", MethodId::General);

        // Substitute var1 = xVal into f1 → get equation in var2 only
        SymExpr* f1_sub = substituteVar(f1, var1, xVal, arena);
        f1_sub = SymSimplify::simplify(f1_sub, arena);

        result.steps.logNote(
            "After substitution: " + f1_sub->toString() + " = 0.",
            MethodId::General);

        // Solve the substituted equation for var2
        if (!f1_sub->containsVar(var2)) {
            // f1_sub is a constant — check if it's zero
            if (f1_sub->type == SymExprType::Num) {
                auto* num = static_cast<SymNum*>(f1_sub);
                if (num->_coeff.isZero()) {
                    // Identity 0=0 → var2 is free (skip or note)
                    result.steps.logNote(
                        "Identity 0=0: " + std::string(1, var2) +
                        " is free", MethodId::General);
                }
            }
            // Try with f2 instead
            SymExpr* f2_sub = substituteVar(f2, var1, xVal, arena);
            f2_sub = SymSimplify::simplify(f2_sub, arena);
            if (f2_sub->containsVar(var2)) {
                f1_sub = f2_sub;
            } else {
                continue;  // Can't solve for var2 — skip this x solution
            }
        }

        OmniResult yRes = omni.solve(f1_sub, zeroExpr, var2, arena);

        if (!yRes.ok || yRes.solutions.empty()) {
            // Try with f2
            SymExpr* f2_sub = substituteVar(f2, var1, xVal, arena);
            f2_sub = SymSimplify::simplify(f2_sub, arena);
            if (f2_sub->containsVar(var2)) {
                yRes = omni.solve(f2_sub, zeroExpr, var2, arena);
            }
        }

        if (!yRes.ok) continue;

        // Build NLSolution pairs
        for (const auto& ySol : yRes.solutions) {
            NLSolution pair;
            pair.exprX = xVal;
            pair.exprY = ySol.symbolic;
            pair.numX  = xSol.numeric;
            pair.numY  = ySol.numeric;
            pair.isExact = xSol.isExact && ySol.isExact;

            if (ySol.symbolic) {
                pair.exprY = ySol.symbolic;
            } else if (ySol.isExact) {
                pair.exprY = symNum(arena, ySol.exact);
            } else {
                pair.exprY = symNum(arena, vpam::ExactVal::fromDouble(ySol.numeric));
            }

            result.steps.logNote(
                "\u2714 Solution: (" + std::string(1, var1) + ", " +
                std::string(1, var2) + ") = (" +
                pair.exprX->toString() + ", " +
                pair.exprY->toString() + ").", MethodId::General);

            result.solutions.push_back(pair);
        }
    }

    result.ok = !result.solutions.empty();
    if (!result.ok) {
        result.error = "No valid solutions after back-substitution";
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════
// solveNonlinear3x3 — Cascaded Sylvester elimination for 3×3 systems
//
// Pipeline:
//   1. Eliminate var3 between (f1, f2) → R12(var1, var2) = 0
//   2. Eliminate var3 between (f1, f3) → R13(var1, var2) = 0
//   3. Solve {R12=0, R13=0} via solveNonlinear2x2
//   4. Back-substitute each (var1, var2) → solve f1 for var3
// ════════════════════════════════════════════════════════════════════

NLSystemResult SystemSolver::solveNonlinear3x3(
    SymExpr* f1, SymExpr* f2, SymExpr* f3,
    char var1, char var2, char var3,
    SymExprArena& arena)
{
    NLSystemResult result;
    result.var1     = var1;
    result.var2     = var2;
    result.var3     = var3;
    result.numVars  = 3;
    result.methodUsed = SystemMethod::Resultant;

    if (!f1 || !f2 || !f3) {
        result.error = "Null equation";
        return result;
    }

    // ── Step 1: Simplify all three equations ───────────────────────
    f1 = SymSimplify::simplify(f1, arena);
    f2 = SymSimplify::simplify(f2, arena);
    f3 = SymSimplify::simplify(f3, arena);

    result.steps.logNote(
        "Nonlinear system 3\u00D73: eliminating " + std::string(1, var3) +
        " by cascaded Sylvester resultants.", MethodId::General);
    result.steps.logNote(
        "Eq1: " + f1->toString() + " = 0", MethodId::General);
    result.steps.logNote(
        "Eq2: " + f2->toString() + " = 0", MethodId::General);
    result.steps.logNote(
        "Eq3: " + f3->toString() + " = 0", MethodId::General);

    // ── Step 2: Collect as univariate in var3 ──────────────────────
    UniPoly P1 = collectAsUniPoly(f1, var3, arena);
    UniPoly P2 = collectAsUniPoly(f2, var3, arena);
    UniPoly P3 = collectAsUniPoly(f3, var3, arena);

    if (P1.isZero() || P2.isZero() || P3.isZero()) {
        result.error = "Could not extract polynomial structure in " +
                       std::string(1, var3);
        return result;
    }

    // Degree safety
    int d12 = P1.degree() + P2.degree();
    int d13 = P1.degree() + P3.degree();
    if (d12 > 12 || d13 > 12) {
        result.error = "Total degree too high for Sylvester (" +
                       std::to_string(d12) + ", " + std::to_string(d13) + ")";
        return result;
    }

    // ── Step 3: Resultant R12 = Res(f1, f2, var3) ─────────────────
    result.steps.logNote(
        "Constructing " + std::to_string(d12) + "\u00D7" +
        std::to_string(d12) +
        " Sylvester matrix for Eq1 and Eq2.", MethodId::General);

    SymExpr* R12 = sylvesterResultant(P1, P2, arena);
    if (!R12) {
        result.error = "Error computing the resultant R12";
        return result;
    }
    R12 = SymSimplify::simplify(R12, arena);

    result.steps.logNote(
        "R12(" + std::string(1, var1) + "," + std::string(1, var2) +
        ") = " + R12->toString(), MethodId::General);

    // ── Step 4: Resultant R13 = Res(f1, f3, var3) ─────────────────
    result.steps.logNote(
        "Constructing " + std::to_string(d13) + "\u00D7" +
        std::to_string(d13) +
        " Sylvester matrix for Eq1 and Eq3.", MethodId::General);

    SymExpr* R13 = sylvesterResultant(P1, P3, arena);
    if (!R13) {
        result.error = "Error computing the resultant R13";
        return result;
    }
    R13 = SymSimplify::simplify(R13, arena);

    result.steps.logNote(
        "R13(" + std::string(1, var1) + "," + std::string(1, var2) +
        ") = " + R13->toString(), MethodId::General);

    // ── Step 5: Solve {R12=0, R13=0} as 2×2 nonlinear ─────────────
    result.steps.logNote(
        "Solving the reduced 2\u00D72 system {R12=0, R13=0} for (" +
        std::string(1, var1) + ", " + std::string(1, var2) + ").",
        MethodId::General);

    NLSystemResult sub2x2 = solveNonlinear2x2(R12, R13, var1, var2, arena);

    if (!sub2x2.ok || sub2x2.solutions.empty()) {
        result.error = sub2x2.error.empty()
            ? "No solutions for the reduced 2\u00D72 system"
            : sub2x2.error;
        return result;
    }

    result.steps.logNote(
        "Found " + std::to_string(sub2x2.solutions.size()) +
        " candidate(s) for (" + std::string(1, var1) + ", " +
        std::string(1, var2) + ").", MethodId::General);

    // ── Step 6: Back-substitute to find var3 ───────────────────────
    OmniSolver omni;
    SymExpr* zeroExpr = symInt(arena, 0);

    for (const auto& sol2 : sub2x2.solutions) {
        SymExpr* xVal = sol2.exprX;
        SymExpr* yVal = sol2.exprY;
        if (!xVal || !yVal) continue;

        result.steps.logNote(
            "Back-substituting " + std::string(1, var1) + " = " +
            xVal->toString() + ", " + std::string(1, var2) + " = " +
            yVal->toString() + " into Eq1 to find " +
            std::string(1, var3) + ".", MethodId::General);

        // Substitute both var1 and var2 into f1
        SymExpr* f1_sub = substituteVar(f1, var1, xVal, arena);
        f1_sub = substituteVar(f1_sub, var2, yVal, arena);
        f1_sub = SymSimplify::simplify(f1_sub, arena);

        // Check if f1_sub still contains var3
        if (!f1_sub->containsVar(var3)) {
            // Try f2 instead
            SymExpr* f2_sub = substituteVar(f2, var1, xVal, arena);
            f2_sub = substituteVar(f2_sub, var2, yVal, arena);
            f2_sub = SymSimplify::simplify(f2_sub, arena);
            if (f2_sub->containsVar(var3)) {
                f1_sub = f2_sub;
            } else {
                // Try f3
                SymExpr* f3_sub = substituteVar(f3, var1, xVal, arena);
                f3_sub = substituteVar(f3_sub, var2, yVal, arena);
                f3_sub = SymSimplify::simplify(f3_sub, arena);
                if (f3_sub->containsVar(var3)) {
                    f1_sub = f3_sub;
                } else {
                    continue;  // Can't solve for var3
                }
            }
        }

        OmniResult zRes = omni.solve(f1_sub, zeroExpr, var3, arena);

        if (!zRes.ok || zRes.solutions.empty()) {
            // Try f2 or f3 as fallback
            SymExpr* f2_sub = substituteVar(f2, var1, xVal, arena);
            f2_sub = substituteVar(f2_sub, var2, yVal, arena);
            f2_sub = SymSimplify::simplify(f2_sub, arena);
            if (f2_sub->containsVar(var3)) {
                zRes = omni.solve(f2_sub, zeroExpr, var3, arena);
            }
        }

        if (!zRes.ok) continue;

        // Build NLSolution triplets
        for (const auto& zSol : zRes.solutions) {
            NLSolution triplet;
            triplet.exprX = xVal;
            triplet.exprY = yVal;
            triplet.numX  = sol2.numX;
            triplet.numY  = sol2.numY;
            triplet.numZ  = zSol.numeric;
            triplet.isExact = sol2.isExact && zSol.isExact;

            if (zSol.symbolic) {
                triplet.exprZ = zSol.symbolic;
            } else if (zSol.isExact) {
                triplet.exprZ = symNum(arena, zSol.exact);
            } else {
                triplet.exprZ = symNum(arena,
                    vpam::ExactVal::fromDouble(zSol.numeric));
            }

            result.steps.logNote(
                "\u2714 Solution: (" + std::string(1, var1) + ", " +
                std::string(1, var2) + ", " + std::string(1, var3) +
                ") = (" + triplet.exprX->toString() + ", " +
                triplet.exprY->toString() + ", " +
                triplet.exprZ->toString() + ").", MethodId::General);

            result.solutions.push_back(triplet);
        }
    }

    result.ok = !result.solutions.empty();
    if (!result.ok) {
        result.error = "No valid solutions after back-substitution";
    }

    return result;
}

} // namespace cas
