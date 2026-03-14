/**
 * RuleBasedTutor.cpp — Generalized rule-based CAS step engine.
 *
 * Applies five sequential algebraic rules to solve any polynomial equation:
 *   Rule 1 — Expand both sides (normalize polynomials).
 *   Rule 2 — Combine like terms on each side.
 *   Rule 3 — Isolate: move variable terms to LHS, constants to RHS.
 *   Rule 4 — Combine again after isolation.
 *   Rule 5 — Divide both sides by the variable's coefficient.
 *   Fallback — If degree 2, invoke solveQuadraticTutor().
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 12 (True Rule-Based CAS Engine)
 */

#include "RuleBasedTutor.h"
#include "TutorTemplates.h"
#include "SymExprArena.h"
#include "CASNumber.h"
#include <cmath>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════

/// Build a SymEquation where each side is normalized (like terms combined).
static SymEquation expandAndCombine(const SymEquation& eq) {
    SymPoly lhs = eq.lhs;
    SymPoly rhs = eq.rhs;
    lhs.normalize();
    rhs.normalize();
    return SymEquation(lhs, rhs);
}

/// Separate terms: variable terms (power >= 1) go to LHS, constants to RHS.
/// Returns the new equation with all x^k (k>=1) on LHS and constants on RHS.
static SymEquation isolateVariable(const SymEquation& eq, char var) {
    SymPoly varLHS(var);  // variable terms: LHS - RHS variable terms
    SymPoly constRHS(var); // constant terms: RHS - LHS constant terms

    // Collect variable terms from LHS (add) and RHS (subtract)
    for (const auto& t : eq.lhs.terms()) {
        if (!t.isConstant()) {
            varLHS.terms().push_back(t);
        }
    }
    for (const auto& t : eq.rhs.terms()) {
        if (!t.isConstant()) {
            // Move RHS variable term to LHS by negating
            SymTerm negT = t;
            negT.negate();
            varLHS.terms().push_back(negT);
        }
    }

    // Collect constant terms from RHS (add) and LHS (subtract)
    for (const auto& t : eq.rhs.terms()) {
        if (t.isConstant()) {
            constRHS.terms().push_back(t);
        }
    }
    for (const auto& t : eq.lhs.terms()) {
        if (t.isConstant()) {
            // Move LHS constant to RHS by negating
            SymTerm negT = t;
            negT.negate();
            constRHS.terms().push_back(negT);
        }
    }

    varLHS.normalize();
    constRHS.normalize();

    // Ensure empty sides become zero
    if (varLHS.terms().empty()) {
        varLHS = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
    }
    if (constRHS.terms().empty()) {
        constRHS = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
    }

    return SymEquation(varLHS, constRHS);
}

// ════════════════════════════════════════════════════════════════════
// solveEquationStepByStep
// ════════════════════════════════════════════════════════════════════

SolveResult solveEquationStepByStep(const SymEquation& eq, char var,
                                     PedagogicalLogger& log, SymExprArena* arena) {
    SolveResult result;
    result.variable = var;
    result.ok = false;

    // ── Log original equation ────────────────────────────────────────
    log.logAction(SolveAction::PRESENT_ORIGINAL_EQUATION,
                  ActionContext().var(var).snap(&eq), MethodId::General);

    // ── Rule 1: Expand (normalize each side independently) ───────────
    SymEquation step1 = expandAndCombine(eq);
    bool expandChanged = (step1.toString() != eq.toString());
    if (expandChanged) {
        log.logAction(SolveAction::PRE_DISTRIBUTE,
                      ActionContext().var(var).snap(&step1), MethodId::General);
    }

    // ── Rule 2: Combine like terms on each side ──────────────────────
    // (Already done by expandAndCombine above; log if not already logged)
    SymPoly lhs2 = step1.lhs;
    SymPoly rhs2 = step1.rhs;
    lhs2.normalize();
    rhs2.normalize();
    SymEquation step2(lhs2, rhs2);
    bool combineChanged = (step2.toString() != step1.toString());
    if (combineChanged) {
        log.logAction(SolveAction::PRE_COMBINE_TERMS,
                      ActionContext().var(var).snap(&step2), MethodId::General);
    }

    // Check degree for quadratic fallback
    int16_t deg = step2.degree();
    if (deg > 2) {
        result.error = "Equations of degree greater than 2 are not supported";
        return result;
    }

    // ── Quadratic fallback ───────────────────────────────────────────
    if (deg == 2) {
        log.logAction(SolveAction::IDENTIFY_EQUATION_TYPE,
                      ActionContext().var(var).deg(2), MethodId::Quadratic);
        return solveQuadraticTutor(step2, var, log, arena);
    }

    // ── Rule 3: Isolate variable terms on LHS, constants on RHS ─────
    SymEquation step3 = isolateVariable(step2, var);
    bool isolateChanged = (step3.toString() != step2.toString());
    if (isolateChanged) {
        log.logAction(SolveAction::LINEAR_ISOLATE_VARIABLE,
                      ActionContext().var(var).snap(&step3), MethodId::Linear);
    }

    // ── Rule 4: Combine again ────────────────────────────────────────
    SymEquation step4 = expandAndCombine(step3);
    bool combineAgainChanged = (step4.toString() != step3.toString());
    if (combineAgainChanged) {
        log.logAction(SolveAction::PRE_COMBINE_TERMS,
                      ActionContext().var(var).snap(&step4), MethodId::Linear);
    }

    // ── Rule 5: Divide both sides by variable coefficient ───────────
    CASNumber coeff = CASNumber::fromExactVal(step4.lhs.coeffAtExact(1));
    if (coeff.isZero()) {
        result.error = "Variable eliminated during solving; equation may be an identity or contradiction";
        return result;
    }

    log.logAction(SolveAction::IDENTIFY_EQUATION_TYPE,
                  ActionContext().var(var).deg(1), MethodId::Linear);
    log.logAction(SolveAction::LINEAR_IDENTIFY_COEFFICIENTS,
                  ActionContext().var(var).deg(1).withArena(arena)
                      .val("a", coeff)
                      .val("b", CASNumber::fromExactVal(step4.rhs.coeffAtExact(0))),
                  MethodId::Linear);

    // x = constant / coefficient
    CASNumber rhsConst = CASNumber::fromExactVal(step4.rhs.coeffAtExact(0));
    CASNumber solution = CASNumber::div(rhsConst, coeff);

    // Build the final equation: x = solution
    vpam::ExactVal solEV = solution.toExactVal();
    solEV.simplify();
    SymPoly lhsFinal = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1));
    SymPoly rhsFinal = SymPoly::fromConstant(solEV);
    SymEquation stepFinal(lhsFinal, rhsFinal);

    log.logAction(SolveAction::LINEAR_DIVIDE_BY_COEFFICIENT,
                  ActionContext().var(var).withArena(arena).snap(&stepFinal),
                  MethodId::Linear);
    log.logAction(SolveAction::LINEAR_PRESENT_SOLUTION,
                  ActionContext().var(var).withArena(arena)
                      .val("solution", solution).solIdx(0),
                  MethodId::Linear);

    result.solutions[0] = solEV;
    result.count = 1;
    result.ok = true;
    return result;
}

} // namespace cas
