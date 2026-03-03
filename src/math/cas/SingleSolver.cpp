/**
 * SingleSolver.cpp — Implementation of the single-variable equation solver.
 *
 * Solver pipeline:
 *   1. Normalize to f(x) = 0 via moveAllToLHS()
 *   2. Determine degree of f(x)
 *   3. Dispatch to linear / quadratic / Newton-Raphson
 *   4. Return solutions + step log
 *
 * Step descriptions are in Spanish per project requirements.
 *
 * Part of: NumOS CAS-Lite — Phase C
 */

#include "SingleSolver.h"
#include <cmath>
#include <cstdlib>

namespace cas {

using vpam::ExactVal;

// ────────────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────────────

SingleSolver::SingleSolver() {}

// ────────────────────────────────────────────────────────────────────
// solve — Main entry point
// ────────────────────────────────────────────────────────────────────

SolveResult SingleSolver::solve(const SymEquation& eq, char variable) {
    SolveResult result;
    result.variable = variable;
    result.ok       = false;

    // Step 0 — Log the original equation
    result.steps.log("Ecuacion original", eq, MethodId::General);

    // Step 1 — Normalize: move everything to LHS = 0
    SymEquation normalized = normalize(eq, result.steps);

    // Step 2 — Determine degree
    int16_t deg = normalized.lhs.degree();

    if (deg <= 0) {
        // Constant equation (e.g., 5 = 0 → no variable terms)
        ExactVal constVal = normalized.lhs.coeffAtExact(0);
        if (constVal.isZero()) {
            result.steps.logNote("Identidad: 0 = 0 (infinitas soluciones)", MethodId::General);
            result.ok    = true;
            result.count = 0;  // Identity — all values are solutions
        } else {
            result.steps.logNote("Contradiccion: " + normalized.lhs.toString() + " != 0 (sin solucion)",
                                 MethodId::General);
            result.ok    = false;
            result.count = 0;
            result.error = "Sin solucion (contradiccion)";
        }
        return result;
    }

    if (deg == 1) {
        // Linear
        result.steps.logNote("Ecuacion lineal detectada (grado 1)", MethodId::Linear);
        if (solveLinear(normalized, variable, result)) {
            result.ok = true;
        }
    } else if (deg == 2) {
        // Quadratic
        result.steps.logNote("Ecuacion cuadratica detectada (grado 2)", MethodId::Quadratic);
        if (solveQuadratic(normalized, variable, result)) {
            result.ok = true;
        }
    } else {
        // Higher degree → Newton-Raphson fallback
        result.steps.logNote(
            "Grado " + std::to_string(deg) + " detectado. Usando aproximacion numerica (Newton-Raphson)",
            MethodId::Newton);
        if (solveNewton(normalized, variable, result)) {
            result.ok      = true;
            result.numeric = true;
        }
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════
// normalize — Move all terms to LHS, log the step
// ════════════════════════════════════════════════════════════════════

SymEquation SingleSolver::normalize(const SymEquation& eq, CASStepLogger& log) {
    SymEquation norm = eq.moveAllToLHS();
    log.log("Mover todos los terminos al lado izquierdo e igualar a cero", norm, MethodId::General);
    return norm;
}

// ════════════════════════════════════════════════════════════════════
// solveLinear — ax + b = 0  →  x = -b / a
// ════════════════════════════════════════════════════════════════════

bool SingleSolver::solveLinear(const SymEquation& eq, char var, SolveResult& result) {
    const SymPoly& f = eq.lhs;   // f(x) = ax + b = 0

    ExactVal a = f.coeffAtExact(1);   // coefficient of x
    ExactVal b = f.coeffAtExact(0);   // constant term

    // Should not happen, but guard
    if (a.isZero()) {
        result.error = "Coeficiente principal es cero";
        result.steps.logNote("Error: coeficiente de " + std::string(1, var) + " es cero", MethodId::Linear);
        return false;
    }

    // Step: Isolate the constant
    // ax + b = 0   →   ax = -b
    {
        ExactVal negB = vpam::exactNeg(b);
        SymPoly lhsStep = SymPoly::fromTerm(SymTerm::variable(var, a.num, a.den, 1));
        SymPoly rhsStep = SymPoly::fromConstant(negB);
        SymEquation step(lhsStep, rhsStep);

        std::string desc = "Mover termino independiente al lado derecho";
        if (!b.isZero()) {
            desc += ": " + std::string(1, var);
            // Show what we're isolating
        }
        result.steps.log(desc, step, MethodId::Linear);
    }

    // Step: Divide by coefficient of x
    // ax = -b   →   x = -b/a
    ExactVal solution = vpam::exactDiv(vpam::exactNeg(b), a);
    solution.simplify();

    {
        SymPoly lhsSol = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
        SymPoly rhsSol = SymPoly::fromConstant(solution);
        SymEquation stepEq(lhsSol, rhsSol);
        result.steps.log("Dividir ambos lados por el coeficiente de " + std::string(1, var),
                         stepEq, MethodId::Linear);
    }

    result.solutions[0] = solution;
    result.count         = 1;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solveQuadratic — ax² + bx + c = 0
// ════════════════════════════════════════════════════════════════════

bool SingleSolver::solveQuadratic(const SymEquation& eq, char var, SolveResult& result) {
    const SymPoly& f = eq.lhs;

    ExactVal a = f.coeffAtExact(2);   // x² coefficient
    ExactVal b = f.coeffAtExact(1);   // x  coefficient
    ExactVal c = f.coeffAtExact(0);   // constant

    if (a.isZero()) {
        result.error = "Coeficiente cuadratico es cero";
        result.steps.logNote("Error: coeficiente de " + std::string(1, var) + "^2 es cero",
                             MethodId::Quadratic);
        return false;
    }

    // Log the coefficients
    {
        std::string msg = "Identificar coeficientes: a=" + std::to_string(a.num);
        if (a.den != 1) msg += "/" + std::to_string(a.den);
        msg += ", b=" + std::to_string(b.num);
        if (b.den != 1) msg += "/" + std::to_string(b.den);
        msg += ", c=" + std::to_string(c.num);
        if (c.den != 1) msg += "/" + std::to_string(c.den);
        result.steps.logNote(msg, MethodId::Quadratic);
    }

    // Compute discriminant Δ = b² - 4ac
    ExactVal b2   = vpam::exactMul(b, b);                           // b²
    ExactVal four = ExactVal::fromInt(4);
    ExactVal ac4  = vpam::exactMul(four, vpam::exactMul(a, c));     // 4ac
    ExactVal disc = vpam::exactSub(b2, ac4);                        // Δ = b² - 4ac
    disc.simplify();

    // Log the discriminant
    {
        std::string msg = "Calcular discriminante: D = b^2 - 4ac = " +
                          std::to_string(disc.num);
        if (disc.den != 1) msg += "/" + std::to_string(disc.den);
        result.steps.logNote(msg, MethodId::Quadratic);
    }

    // Check discriminant sign (exact — no floating-point comparison)
    // For rational disc: sign = sign(num) * sign(den) (den always > 0 after simplify)
    bool discNegative = (disc.num < 0 && disc.den > 0) || (disc.num > 0 && disc.den < 0);
    bool discZero     = disc.isZero();

    if (discNegative) {
        // No real solutions
        result.steps.logNote("Discriminante negativo: no hay soluciones reales", MethodId::Quadratic);
        result.count = 0;
        result.ok    = true;
        return true;
    }

    // Compute -b
    ExactVal negB = vpam::exactNeg(b);

    // Compute 2a
    ExactVal twoA = vpam::exactMul(ExactVal::fromInt(2), a);

    result.steps.logNote("Aplicar formula cuadratica: x = (-b +/- sqrt(D)) / 2a", MethodId::Quadratic);

    if (discZero) {
        // One repeated root: x = -b / (2a)
        ExactVal root = vpam::exactDiv(negB, twoA);
        root.simplify();

        result.steps.logNote("Discriminante = 0: una solucion repetida", MethodId::Quadratic);

        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(root);
        result.steps.log("Solucion unica", SymEquation(lhs, rhs), MethodId::Quadratic);

        result.solutions[0] = root;
        result.count         = 1;
        return true;
    }

    // Two distinct real roots: x = (-b ± √Δ) / (2a)
    ExactVal sqrtDisc = vpam::exactSqrt(disc);

    // x₁ = (-b + √Δ) / (2a)
    ExactVal num1  = vpam::exactAdd(negB, sqrtDisc);
    ExactVal root1 = vpam::exactDiv(num1, twoA);
    root1.simplify();

    // x₂ = (-b - √Δ) / (2a)
    ExactVal num2  = vpam::exactSub(negB, sqrtDisc);
    ExactVal root2 = vpam::exactDiv(num2, twoA);
    root2.simplify();

    // Log solutions
    {
        SymPoly lhs  = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
        SymPoly rhs1 = SymPoly::fromConstant(root1);
        result.steps.log("Primera solucion: " + std::string(1, var) + "1",
                         SymEquation(lhs, rhs1), MethodId::Quadratic);

        SymPoly rhs2 = SymPoly::fromConstant(root2);
        result.steps.log("Segunda solucion: " + std::string(1, var) + "2",
                         SymEquation(lhs, rhs2), MethodId::Quadratic);
    }

    result.solutions[0] = root1;
    result.solutions[1] = root2;
    result.count         = 2;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solveNewton — Newton-Raphson numeric fallback
// ════════════════════════════════════════════════════════════════════

static constexpr int    NEWTON_MAX_ITER  = 50;
static constexpr double NEWTON_TOL       = 1e-12;
static constexpr double NEWTON_DX        = 1e-9;     // For numeric derivative

bool SingleSolver::solveNewton(const SymEquation& eq, char var, SolveResult& result) {
    const SymPoly& f = eq.lhs;

    // Try several initial guesses
    double guesses[] = {1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 10.0, -10.0};
    int found = 0;

    for (double x0 : guesses) {
        if (found >= 2) break;

        double x = x0;
        bool converged = false;

        for (int i = 0; i < NEWTON_MAX_ITER; ++i) {
            double fx  = evalPoly(f, var, x);
            double fpx = evalDerivative(f, var, x);

            if (std::abs(fpx) < 1e-15) break;  // Derivative too small

            double xNew = x - fx / fpx;

            if (std::abs(xNew - x) < NEWTON_TOL) {
                converged = true;
                x = xNew;
                break;
            }
            x = xNew;
        }

        if (!converged) continue;

        // Check if f(x) ≈ 0
        double fx = evalPoly(f, var, x);
        if (std::abs(fx) > 1e-8) continue;

        // Avoid duplicates
        bool duplicate = false;
        for (int j = 0; j < found; ++j) {
            if (std::abs(result.solutions[j].toDouble() - x) < NEWTON_TOL * 100) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        result.solutions[found] = ExactVal::fromDouble(x);
        result.solutions[found].simplify();
        found++;
    }

    result.count = found;

    if (found == 0) {
        result.error = "Newton-Raphson no convergio";
        result.steps.logNote("Newton-Raphson: no se encontraron raices reales", MethodId::Newton);
        return false;
    }

    // Log the solutions
    for (int i = 0; i < found; ++i) {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(result.solutions[i]);
        std::string label = "Solucion numerica " + std::string(1, var) +
                            std::to_string(i + 1) + " (aprox.)";
        result.steps.log(label, SymEquation(lhs, rhs), MethodId::Newton);
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════
// evalPoly — Evaluate f(x) at a double value using Horner's method
// ════════════════════════════════════════════════════════════════════

double SingleSolver::evalPoly(const SymPoly& poly, char var, double x) {
    double result = 0.0;
    for (const auto& term : poly.terms()) {
        double coeff = term.coeff.toDouble();
        if (term.isConstant()) {
            result += coeff;
        } else if (term.var == var) {
            result += coeff * std::pow(x, static_cast<double>(term.power));
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════
// evalDerivative — Numeric derivative via central difference
// ════════════════════════════════════════════════════════════════════

double SingleSolver::evalDerivative(const SymPoly& poly, char var, double x) {
    double h = NEWTON_DX;
    double fPlus  = evalPoly(poly, var, x + h);
    double fMinus = evalPoly(poly, var, x - h);
    return (fPlus - fMinus) / (2.0 * h);
}

} // namespace cas
