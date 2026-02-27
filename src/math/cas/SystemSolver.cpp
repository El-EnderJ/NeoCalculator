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
                                     char var1, char var2) {
    SystemResult result;
    result.numVars = 2;
    result.vars[0] = var1;
    result.vars[1] = var2;
    result.ok      = false;

    char vars[3] = { var1, var2, '\0' };

    // Log original system
    result.steps.logNote("Sistema de ecuaciones 2x2:", MethodId::General);
    result.steps.log("  Ec.1: " + eqToString(eq1, vars, 2),
                     eqToSymEquation(eq1, vars, 2), MethodId::General);
    result.steps.log("  Ec.2: " + eqToString(eq2, vars, 2),
                     eqToSymEquation(eq2, vars, 2), MethodId::General);

    // Choose method
    SystemMethod method = analyzeAndChoose(eq1, eq2);
    result.methodUsed = method;

    bool success = false;
    if (method == SystemMethod::Reduction) {
        result.steps.logNote("Metodo seleccionado: Reduccion (igualacion de coeficientes)",
                             MethodId::Reduction);
        success = solveByReduction(eq1, eq2, var1, var2, result);
    } else {
        result.steps.logNote("Metodo seleccionado: Sustitucion",
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
        "Despejando " + std::string(1, vars[isoVar]) +
        " en la Ecuacion " + std::to_string(isoEq + 1), M);

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
        "Sustituyendo en la Ecuacion " + std::to_string(otherEq + 1), M);

    // Check for zero coefficient (no solution or infinite solutions)
    if (newCoeff.isZero()) {
        if (newRhs.isZero()) {
            result.steps.logNote("Sistema dependiente: infinitas soluciones", M);
            result.error = "Sistema dependiente";
        } else {
            result.steps.logNote("Sistema incompatible: sin solucion", M);
            result.error = "Sin solucion (sistema incompatible)";
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
            "Resolviendo para " + std::string(1, vars[otherVar]),
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
            "Sustituyendo " + std::string(1, vars[otherVar]) +
            " para encontrar " + std::string(1, vars[isoVar]),
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
        result.steps.logNote("Ambos coeficientes de " + std::string(1, vars[elimVar]) +
                             " son cero", M);
        result.error = "Coeficientes cero";
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
        "Multiplicando Ec.1 por " + std::to_string(c2.num) +
        (c2.den != 1 ? "/" + std::to_string(c2.den) : "") +
        " para igualar coeficientes de " + std::string(1, vars[elimVar]), M);

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

    result.steps.log("  Ec.1 * " + std::to_string(c2.num) +
                     (c2.den != 1 ? "/" + std::to_string(c2.den) : ""),
                     eqToSymEquation(scaled1, vars, 2), M);

    result.steps.logNote(
        "Multiplicando Ec.2 por " + std::to_string(c1.num) +
        (c1.den != 1 ? "/" + std::to_string(c1.den) : ""), M);

    result.steps.log("  Ec.2 * " + std::to_string(c1.num) +
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
        "Restando ecuaciones para eliminar " + std::string(1, vars[elimVar]),
        eqToSymEquation(combined, vars, 2), M);

    // ── Solve for keepVar ────────────────────────────────────────
    ExactVal keepCoeff = combined.coeffs[keepVar];

    if (keepCoeff.isZero()) {
        if (combined.rhs.isZero()) {
            result.steps.logNote("Sistema dependiente: infinitas soluciones", M);
            result.error = "Sistema dependiente";
        } else {
            result.steps.logNote("Sistema incompatible: sin solucion", M);
            result.error = "Sin solucion (sistema incompatible)";
        }
        return false;
    }

    ExactVal solKeep = exactDiv(combined.rhs, keepCoeff);
    solKeep.simplify();

    {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(vars[keepVar], 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(solKeep);
        result.steps.log(
            "Resolviendo para " + std::string(1, vars[keepVar]),
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
            "Sustituyendo " + std::string(1, vars[keepVar]) +
            " en Ec.1 para encontrar " + std::string(1, vars[elimVar]),
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
    result.steps.logNote("Sistema de ecuaciones 3x3:", MethodId::Gauss);
    result.steps.log("  Ec.1: " + eqToString(eq1, vars, 3),
                     eqToSymEquation(eq1, vars, 3), MethodId::Gauss);
    result.steps.log("  Ec.2: " + eqToString(eq2, vars, 3),
                     eqToSymEquation(eq2, vars, 3), MethodId::Gauss);
    result.steps.log("  Ec.3: " + eqToString(eq3, vars, 3),
                     eqToSymEquation(eq3, vars, 3), MethodId::Gauss);

    result.steps.logNote("Metodo: Eliminacion de Gauss", MethodId::Gauss);

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

    result.steps.logNote("Aplicando eliminacion de Gauss para triangular la matriz", M);

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
                "Pivote cero en columna " + std::string(1, vars[col]) +
                ": sistema singular", M);
            result.error = "Sistema singular (sin solucion unica)";
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
                "Intercambiando Ec." + std::to_string(col + 1) +
                " con Ec." + std::to_string(pivotRow + 1), M);
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
                    "Eliminando " + std::string(1, vars[col]) +
                    " de Ec." + std::to_string(r + 1),
                    eqToSymEquation(rowEq, vars, 3), M);
            }
        }
    }

    // ── Check for singular matrix (zero on diagonal) ─────────────
    for (int i = 0; i < 3; ++i) {
        if (mat[i][i].isZero()) {
            result.steps.logNote("Pivote cero en diagonal: sistema singular", M);
            result.error = "Sistema singular (sin solucion unica)";
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
                "Sustitucion hacia atras: " + std::string(1, vars[r]) +
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

} // namespace cas
