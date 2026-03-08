/**
 * SingleSolver.cpp — Implementation of the single-variable equation solver.
 *
 * CAS-S3-ULTRA engine integration:
 *
 *   Pilar 1 (CASNumber):
 *     All intermediate arithmetic uses CASNumber — exact fractions via
 *     CASRational (with CASInt BigInt auto-promotion on overflow),
 *     exact radicals for sqrt(D), and double fallback for irrationals.
 *     Results bridge to vpam::ExactVal only at the end for rendering.
 *
 *   Pilar 2 (PedagogicalLogger):
 *     Every step is logged via SolveAction + ActionContext.  The logger
 *     generates rich contextual phrases explaining *why* each computation
 *     happens, not just *what* is being done.
 *
 *   Pilar 3 (SymExprArena):
 *     SymPoly terms use PSRAMAllocator; step vectors live in PSRAM.
 *     The arena + ConsTable deduplication is active for any SymExpr
 *     operations in the OmniSolver layer above.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase C (SingleSolver + Dynamic Reasoning)
 */

#include "SingleSolver.h"
#include "TutorTemplates.h"
#include "CASNumber.h"
#include "PedagogicalLogger.h"
#include "../../Config.h"
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

SolveResult SingleSolver::solve(const SymEquation& eq, char variable,
                                SymExprArena* arena) {
    SolveResult result;
    result.variable = variable;
    result.ok       = false;

    if (arena) {
        SymEquation normalized = eq.moveAllToLHS();
        int16_t deg = normalized.lhs.degree();

        if (deg == 1) {
            return solveLinearTutor(eq, variable, result.steps, arena);
        }
        if (deg == 2) {
            return solveQuadraticTutor(eq, variable, result.steps, arena);
        }
        if (deg == 3) {
            return solveCubicTutor(eq, variable, result.steps, arena);
        }
    }

    // Step 0 — Log the original equation with VPAM snapshot
    result.steps.log("Original equation", eq, MethodId::General);

    // Step 1 — Normalize: move everything to LHS = 0
    SymEquation normalized = normalize(eq, result.steps);

    // Step 2 — Determine degree
    int16_t deg = normalized.lhs.degree();

    if (deg <= 0) {
        // Constant equation (e.g., 5 = 0 → no variable terms)
        ExactVal constVal = normalized.lhs.coeffAtExact(0);
        if (constVal.isZero()) {
            result.steps.logNote("Identity: 0 = 0 (infinite solutions)", MethodId::General);
            result.ok    = true;
            result.count = 0;
        } else {
            result.steps.logNote("Contradiction: " + normalized.lhs.toString() + " != 0 (no solution)",
                                 MethodId::General);
            result.ok    = false;
            result.count = 0;
            result.error = "No solution (contradiction)";
        }
        return result;
    }

    // Step 3 — Identify equation type (action-based)
    result.steps.logAction(
        SolveAction::IDENTIFY_EQUATION_TYPE,
        ActionContext().var(variable).deg(deg),
        MethodId::General);

    if (deg == 1) {
        if (solveLinear(normalized, variable, result, arena)) {
            result.ok = true;
        }
    } else if (deg == 2) {
        if (solveQuadratic(normalized, variable, result, arena)) {
            result.ok = true;
        }
    } else {
        result.steps.logAction(
            SolveAction::NEWTON_START,
            ActionContext().var(variable).deg(deg),
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

SymEquation SingleSolver::normalize(const SymEquation& eq, PedagogicalLogger& log) {
    SymEquation norm = eq.moveAllToLHS();
    // Single merged step: normalize + show result
    log.log("Moving and grouping all terms to the left side", norm, MethodId::General);
    return norm;
}

// ════════════════════════════════════════════════════════════════════
// solveLinear — ax + b = 0  →  x = -b/a
//
// Uses CASNumber for overflow-safe arithmetic.
// Uses PedagogicalLogger for contextual step descriptions.
// ════════════════════════════════════════════════════════════════════

bool SingleSolver::solveLinear(const SymEquation& eq, char var, SolveResult& result,
                               SymExprArena* arena) {
    if (arena) {
        result = solveLinearTutor(eq, var, result.steps, arena);
        return result.ok;
    }

    const SymPoly& f = eq.lhs;

    // Extract coefficients as CASNumber (from ExactVal bridge)
    CASNumber a = CASNumber::fromExactVal(f.coeffAtExact(1));
    CASNumber b = CASNumber::fromExactVal(f.coeffAtExact(0));

    if (a.isZero()) {
        result.error = "Leading coefficient is zero";
        result.steps.logNote("Error: coefficient of " + std::string(1, var) + " is zero",
                             MethodId::Linear);
        return false;
    }

    // Log coefficient identification
    result.steps.logAction(
        SolveAction::LINEAR_IDENTIFY_COEFFICIENTS,
        ActionContext().var(var).withArena(arena).val("a", a).val("b", b),
        MethodId::Linear);

    // Step: Isolate variable term — ax = -b
    CASNumber negB = CASNumber::neg(b);
    {
        ExactVal aEV = a.toExactVal();
        ExactVal negBEV = negB.toExactVal();
        SymPoly lhsStep = SymPoly::fromTerm(SymTerm::variable(var, aEV.num, aEV.den, 1));
        SymPoly rhsStep = SymPoly::fromConstant(negBEV);
        SymEquation step(lhsStep, rhsStep);

        result.steps.logAction(
            SolveAction::LINEAR_ISOLATE_VARIABLE,
            ActionContext().var(var).withArena(arena).snap(&step),
            MethodId::Linear);
    }

    // Step: Divide by coefficient — x = -b/a
    CASNumber solution = CASNumber::div(negB, a);
    ExactVal solEV = solution.toExactVal();
    solEV.simplify();

    {
        SymPoly lhsSol = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
        SymPoly rhsSol = SymPoly::fromConstant(solEV);
        SymEquation stepEq(lhsSol, rhsSol);

        result.steps.logAction(
            SolveAction::LINEAR_DIVIDE_BY_COEFFICIENT,
            ActionContext().var(var).withArena(arena).val("a", a).snap(&stepEq),
            MethodId::Linear);

        result.steps.logAction(
            SolveAction::LINEAR_PRESENT_SOLUTION,
            ActionContext().var(var).withArena(arena).val("solution", solution).snap(&stepEq),
            MethodId::Linear);
    }

    result.solutions[0] = solEV;
    result.count         = 1;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solveQuadratic — ax² + bx + c = 0
//
// ★ SHOWCASE for all three CAS-S3-ULTRA pillars:
//
//   Pilar 1 (CASNumber): All arithmetic uses CASNumber.  Discriminant
//     computation is exact even for large coefficients.  sqrt(D) returns
//     a Radical when D is not a perfect square, preserving exact form
//     like 3*sqrt(7) instead of ≈7.937.
//
//   Pilar 2 (PedagogicalLogger): Each step emits an ACTION with context,
//     generating phrases like "Computing the discriminant: D = b^2 - 4ac =
//     (5)^2 - 4*(2)*(-3) = 49" instead of "Compute discriminant: D = 49".
//
//   Pilar 3 (SymExprArena): SymPoly terms and step vectors live in PSRAM.
// ════════════════════════════════════════════════════════════════════

bool SingleSolver::solveQuadratic(const SymEquation& eq, char var, SolveResult& result,
                                  SymExprArena* arena) {
    // Tutor mode: full step-by-step when arena is available
    if (arena) {
        result = solveQuadraticTutor(eq, var, result.steps, arena);
        return result.ok;
    }

    const SymPoly& f = eq.lhs;

    // ── Extract coefficients via CASNumber ──────────────────────────
    CASNumber a = CASNumber::fromExactVal(f.coeffAtExact(2));
    CASNumber b = CASNumber::fromExactVal(f.coeffAtExact(1));
    CASNumber c = CASNumber::fromExactVal(f.coeffAtExact(0));

    if (a.isZero()) {
        result.error = "Quadratic coefficient is zero";
        result.steps.logNote("Error: coefficient of " + std::string(1, var) + "^2 is zero",
                             MethodId::Quadratic);
        return false;
    }

    // ── ACTION: Identify coefficients ──────────────────────────────
    result.steps.logAction(
        SolveAction::QUAD_IDENTIFY_COEFFICIENTS,
        ActionContext().var(var).withArena(arena)
            .val("a", a).val("b", b).val("c", c),
        MethodId::Quadratic);

    // ── Compute discriminant D = b² - 4ac ──────────────────────────
    CASNumber b2   = CASNumber::mul(b, b);                              // b²
    CASNumber four = CASNumber::fromInt(4);
    CASNumber ac4  = CASNumber::mul(four, CASNumber::mul(a, c));        // 4ac
    CASNumber disc = CASNumber::sub(b2, ac4);                           // D = b² - 4ac

    // ── Evaluate discriminant sign (exact) ─────────────────────────
    bool discNegative = disc.isNegative();
    bool discZero     = disc.isZero();

    // ════════════════════════════════════════════════════════════════
    // CASE 1: Discriminant < 0 — Complex conjugate roots
    // ════════════════════════════════════════════════════════════════

    if (discNegative) {
        // Show discriminant computation (only for D < 0)
        result.steps.logAction(
            SolveAction::QUAD_COMPUTE_DISCRIMINANT,
            ActionContext().var(var).withArena(arena)
                .val("a", a).val("b", b).val("c", c).val("D", disc),
            MethodId::Quadratic);

        result.steps.logAction(
            SolveAction::QUAD_DISCRIMINANT_NEGATIVE,
            ActionContext().var(var).withArena(arena).val("D", disc),
            MethodId::Quadratic);

        // ── Complex toggle: if disabled, stop here ─────────────────
        if (!setting_complex_enabled) {
            result.count = 0;
            result.ok    = false;
            result.error = "No real solutions";
            return false;   // signal "not solved" → propagates error
        }

        // 2a
        CASNumber twoA = CASNumber::mul(CASNumber::fromInt(2), a);

        // Real part: Re = -b / (2a)
        CASNumber negB     = CASNumber::neg(b);
        CASNumber realPart = CASNumber::div(negB, twoA);

        // |D| = -D (since D < 0)
        CASNumber absDisc = CASNumber::neg(disc);

        // Imaginary magnitude: |Im| = sqrt(|D|) / (2a)
        CASNumber sqrtAbsDisc = CASNumber::sqrt(absDisc);
        CASNumber imagMag     = CASNumber::div(sqrtAbsDisc, twoA);

        // Ensure imagMag is positive (take absolute value)
        imagMag = CASNumber::abs(imagMag);

        // ── ACTION: Show complex parts ─────────────────────────────
        result.steps.logAction(
            SolveAction::QUAD_COMPUTE_COMPLEX_PARTS,
            ActionContext().var(var).withArena(arena).val("re", realPart).val("im", imagMag),
            MethodId::Quadratic);

        // ── ACTION: Present complex roots ──────────────────────
        result.steps.logAction(
            SolveAction::QUAD_PRESENT_COMPLEX_ROOTS,
            ActionContext().var(var).withArena(arena).val("re", realPart).val("im", imagMag),
            MethodId::Quadratic);

        // Store in result
        result.hasComplexRoots = true;
        result.complexReal     = realPart.toExactVal();
        result.complexReal.simplify();
        result.complexImagMag  = imagMag.toExactVal();
        result.complexImagMag.simplify();
        result.count           = 0;
        result.ok              = true;

        return true;
    }

    // ════════════════════════════════════════════════════════════════
    // CASE 2 & 3: D ≥ 0 — Show formula with values substituted
    //
    // New flow: skip intermediate discriminant/sqrt steps.
    // Emit ONE formula step with actual numerical values, then solutions.
    // ════════════════════════════════════════════════════════════════

    CASNumber negB = CASNumber::neg(b);
    CASNumber twoA = CASNumber::mul(CASNumber::fromInt(2), a);

    if (discZero) {
        // D = 0 → repeated root: x = -b / (2a)
        CASNumber root = CASNumber::div(negB, twoA);
        ExactVal rootEV = root.toExactVal();
        rootEV.simplify();

        // Show formula with values
        result.steps.logAction(
            SolveAction::QUAD_APPLY_FORMULA,
            ActionContext().var(var).withArena(arena)
                .val("D", disc).val("negB", negB).val("twoA", twoA),
            MethodId::Quadratic);

        // Present the single root
        result.steps.logAction(
            SolveAction::QUAD_PRESENT_REAL_SOLUTION,
            ActionContext().var(var).withArena(arena).val("solution", root)
                          .solIdx(0),
            MethodId::Quadratic);

        result.solutions[0] = rootEV;
        result.count         = 1;
        return true;
    }

    // D > 0 → two distinct real roots
    CASNumber sqrtDisc = CASNumber::sqrt(disc);

    // Show formula with values substituted
    result.steps.logAction(
        SolveAction::QUAD_APPLY_FORMULA,
        ActionContext().var(var).withArena(arena)
            .val("D", disc).val("negB", negB).val("sqrtD", sqrtDisc).val("twoA", twoA),
        MethodId::Quadratic);

    // ── x₁ = (-b + sqrt(D)) / (2a) ────────────────────────────────
    CASNumber num1  = CASNumber::add(negB, sqrtDisc);
    CASNumber root1 = CASNumber::div(num1, twoA);
    ExactVal root1EV = root1.toExactVal();
    root1EV.simplify();

    // ── x₂ = (-b - sqrt(D)) / (2a) ────────────────────────────────
    CASNumber num2  = CASNumber::sub(negB, sqrtDisc);
    CASNumber root2 = CASNumber::div(num2, twoA);
    ExactVal root2EV = root2.toExactVal();
    root2EV.simplify();

    // ── Present solutions with MathCanvas (no snapshots) ───────────
    result.steps.logAction(
        SolveAction::QUAD_PRESENT_REAL_SOLUTION,
        ActionContext().var(var).withArena(arena).val("solution", root1)
                      .solIdx(1),
        MethodId::Quadratic);

    result.steps.logAction(
        SolveAction::QUAD_PRESENT_REAL_SOLUTION,
        ActionContext().var(var).withArena(arena).val("solution", root2)
                      .solIdx(2),
        MethodId::Quadratic);

    result.solutions[0] = root1EV;
    result.solutions[1] = root2EV;
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
        result.error = "Newton-Raphson did not converge";
        result.steps.logNote("Newton-Raphson: no real roots found", MethodId::Newton);
        return false;
    }

    // Log the solutions
    for (int i = 0; i < found; ++i) {
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
        SymPoly rhs = SymPoly::fromConstant(result.solutions[i]);
        std::string label = "Numeric solution " + std::string(1, var) +
                            std::to_string(i + 1) + " (approx.)";
        result.steps.logResult(label, SymEquation(lhs, rhs), MethodId::Newton);
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
