/**
 * TutorTemplates.cpp — Educational step-by-step solver templates.
 *
 * This file implements the "Tutor Engine" for the NumOS CAS-S3-ULTRA.
 * It intercept equations before they are sent to the compact solvers
 * and elaborates a full, human-like step-by-step resolution.
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 2 (Super Detail Steps)
 */

#include "TutorTemplates.h"
#include "CASNumber.h"
#include "SymExpr.h"
#include "SymExprArena.h"

namespace cas {

using vpam::ExactVal;

// ════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════

static SymExpr* symFromCAS(SymExprArena& arena, const CASNumber& n) {
    ExactVal ev = n.toExactVal();
    if (ev.den != 1) {
        SymExpr* num = symInt(arena, ev.num);
        SymExpr* den = symInt(arena, ev.den);
        return symMul(arena, num, symPow(arena, den, symInt(arena, -1)));
    }
    return symNum(arena, ev);
}

// ════════════════════════════════════════════════════════════════════
// solveQuadraticTutor
// ════════════════════════════════════════════════════════════════════
// Elaborates:
// 1. Identify equation type
// 2. Identify coefficients a, b, c
// 3. Show generic formula: x = (-b ± √(b² - 4ac)) / 2a
// 4. Substitute values:    x = (-(...) ± √(...² - 4(...)(...))) / 2(...)
// 5. Simplify radical:     x = (... ± √(...)) / ...
// 6. Compute √D:           x = (... ± ...) / ...
// 7. Separate roots:       x₁ = ... , x₂ = ...
// 8. Simplify roots:       results
// ════════════════════════════════════════════════════════════════════

SolveResult solveQuadraticTutor(const SymEquation& eq, char var,
                                PedagogicalLogger& log, SymExprArena* arena) {
    SolveResult result;
    result.variable = var;
    result.ok = false;
    
    // We already know it's degree 2 (SingleSolver determines this)
    const SymPoly& f = eq.lhs;
    
    // ── Extract exact coefficients ──────────────────────────────────
    CASNumber a = CASNumber::fromExactVal(f.coeffAtExact(2));
    CASNumber b = CASNumber::fromExactVal(f.coeffAtExact(1));
    CASNumber c = CASNumber::fromExactVal(f.coeffAtExact(0));
    
    if (a.isZero()) {
        result.error = "Quadratic coefficient is zero";
        return result;
    }
    
    // ── Set up the tracking context ─────────────────────────────────
    ActionContext ctx;
    ctx.var(var).deg(2).withArena(arena);
    ctx.val("a", a).val("b", b).val("c", c);
    
    // ── 1. Type & Coefficients (using preexisting actions) ──────────
    // IDENTIFY_EQUATION_TYPE is logged by SingleSolver before calling us.
    log.logAction(SolveAction::QUAD_IDENTIFY_COEFFICIENTS, ctx, MethodId::Quadratic);
    
    if (!arena) {
        // Fallback if no arena provided (shouldn't happen in visual mode)
        return result;
    }
    
    // ── 2. Generic Formula: x = (-b ± √(b² - 4ac)) / (2a) ───────────
    {
        SymExpr* bVar = symVar(*arena, 'b');
        SymExpr* aVar = symVar(*arena, 'a');
        SymExpr* cVar = symVar(*arena, 'c');
        
        // -b
        SymExpr* negB = symNeg(*arena, bVar);
        // b²
        SymExpr* b2 = symPow(*arena, bVar, symInt(*arena, 2));
        // 4ac
        SymExpr* fourAC = symMul3(*arena, symInt(*arena, 4), aVar, cVar);
        // b² - 4ac
        SymExpr* discrim = symAdd(*arena, b2, symNeg(*arena, fourAC));
        // √(b² - 4ac)
        SymExpr* sqrtD = symPow(*arena, discrim, symFrac(*arena, 1, 2));
        // -b ± √(b² - 4ac)
        SymExpr* num = symPlusMinus(*arena, negB, sqrtD);
        // 2a
        SymExpr* den = symMul(*arena, symInt(*arena, 2), aVar);
        // Fraction
        SymExpr* frac = symMul(*arena, num, symPow(*arena, den, symInt(*arena, -1)));
        
        log.logAction(SolveAction::QUAD_SHOW_GENERAL_FORMULA, ctx.expr(frac), MethodId::Quadratic);
    }
    
    // ── 3. Substitute values ─────────────────────────────────────────
    {
        SymExpr* aNum = symFromCAS(*arena, a);
        SymExpr* bNum = symFromCAS(*arena, b);
        SymExpr* cNum = symFromCAS(*arena, c);
        
        SymExpr* negB = symNeg(*arena, bNum);
        SymExpr* b2 = symPow(*arena, bNum, symInt(*arena, 2));
        SymExpr* fourAC = symMul3(*arena, symInt(*arena, 4), aNum, cNum);
        SymExpr* discrim = symAdd(*arena, b2, symNeg(*arena, fourAC));
        SymExpr* sqrtD = symPow(*arena, discrim, symFrac(*arena, 1, 2));
        SymExpr* num = symPlusMinus(*arena, negB, sqrtD);
        SymExpr* den = symMul(*arena, symInt(*arena, 2), aNum);
        SymExpr* frac = symMul(*arena, num, symPow(*arena, den, symInt(*arena, -1)));
        
        log.logAction(SolveAction::QUAD_SUBSTITUTE_VALUES, ctx.expr(frac), MethodId::Quadratic);
    }
    
    // ── Pre-compute actual values for next steps ──────────────────────
    CASNumber b2Val = CASNumber::mul(b, b);
    CASNumber ac4Val = CASNumber::mul(CASNumber::fromInt(4), CASNumber::mul(a, c));
    CASNumber discVal = CASNumber::sub(b2Val, ac4Val);
    CASNumber twoAVal = CASNumber::mul(CASNumber::fromInt(2), a);
    CASNumber negBVal = CASNumber::neg(b);
    
    ctx.val("D", discVal).val("twoA", twoAVal).val("negB", negBVal);
    
    // ── 4. Simplify under radical ─────────────────────────────────────
    {
        // (-b ± √(D)) / 2a
        SymExpr* negBSym = symFromCAS(*arena, negBVal);
        SymExpr* dSym    = symFromCAS(*arena, discVal);
        SymExpr* sqrtDSym = symPow(*arena, dSym, symFrac(*arena, 1, 2));
        SymExpr* num     = symPlusMinus(*arena, negBSym, sqrtDSym);
        SymExpr* denSym  = symFromCAS(*arena, twoAVal);
        SymExpr* frac    = symMul(*arena, num, symPow(*arena, denSym, symInt(*arena, -1)));
        
        log.logAction(SolveAction::QUAD_SIMPLIFY_UNDER_RADICAL, ctx.expr(frac), MethodId::Quadratic);
    }
    
    // ── Complex check (D < 0) ─────────────────────────────────────────
    if (discVal.isNegative()) {
        log.logAction(SolveAction::QUAD_DISCRIMINANT_NEGATIVE, ctx, MethodId::Quadratic);
        
        // Complex fallback — skip detailed separation for now, rely on SingleSolver standard response
        result.hasComplexRoots = true;
        CASNumber absDisc = CASNumber::neg(discVal);
        CASNumber sqrtAbsDisc = CASNumber::sqrt(absDisc);
        CASNumber realPart = CASNumber::div(negBVal, twoAVal);
        CASNumber imagMag  = CASNumber::div(sqrtAbsDisc, twoAVal);
        imagMag = CASNumber::abs(imagMag);
        
        result.complexReal = realPart.toExactVal();
        result.complexReal.simplify();
        result.complexImagMag = imagMag.toExactVal();
        result.complexImagMag.simplify();
        result.count = 0;
        result.ok = true;
        
        log.logAction(SolveAction::QUAD_COMPUTE_COMPLEX_PARTS, 
            ActionContext().var(var).withArena(arena).val("re", realPart).val("im", imagMag), 
            MethodId::Quadratic);
        log.logAction(SolveAction::QUAD_PRESENT_COMPLEX_ROOTS, 
            ActionContext().var(var).withArena(arena).val("re", realPart).val("im", imagMag), 
            MethodId::Quadratic);
        
        return result;
    }
    
    // ── D >= 0 ────────────────────────────────────────────────────────
    CASNumber sqrtDVal = CASNumber::sqrt(discVal);
    ctx.val("sqrtD", sqrtDVal);
    
    if (discVal.isZero()) {
        log.logAction(SolveAction::QUAD_DISCRIMINANT_ZERO, ctx, MethodId::Quadratic);
    } else {
        log.logAction(SolveAction::QUAD_DISCRIMINANT_POSITIVE, ctx, MethodId::Quadratic);
    }
    
    // ── 5. Compute √D ─────────────────────────────────────────────────
    if (!discVal.isZero()) {
        SymExpr* negBSym = symFromCAS(*arena, negBVal);
        SymExpr* sqrtDNum = symFromCAS(*arena, sqrtDVal);
        SymExpr* num     = symPlusMinus(*arena, negBSym, sqrtDNum);
        SymExpr* denSym  = symFromCAS(*arena, twoAVal);
        SymExpr* frac    = symMul(*arena, num, symPow(*arena, denSym, symInt(*arena, -1)));
        
        log.logAction(SolveAction::QUAD_COMPUTE_SQRT_VALUE, ctx.expr(frac), MethodId::Quadratic);
    }
    
    // ── 6. Separate roots if D > 0 ────────────────────────────────────
    if (discVal.isZero()) {
        // Only one root
        CASNumber root = CASNumber::div(negBVal, twoAVal);
        vpam::ExactVal rootEV = root.toExactVal();
        rootEV.simplify();
        
        log.logAction(SolveAction::QUAD_PRESENT_REAL_SOLUTION, 
            ActionContext().var(var).withArena(arena).val("solution", root).solIdx(0), 
            MethodId::Quadratic);
            
        result.solutions[0] = rootEV;
        result.count = 1;
        result.ok = true;
    } else {
        // Two roots to separate
        SymExpr* denSym = symFromCAS(*arena, twoAVal);
        
        // x₁ = (-b + √D) / 2a
        SymExpr* num1 = symAdd(*arena, symFromCAS(*arena, negBVal), symFromCAS(*arena, sqrtDVal));
        SymExpr* root1Expr = symMul(*arena, num1, symPow(*arena, denSym, symInt(*arena, -1)));
        
        // x₂ = (-b - √D) / 2a
        SymExpr* num2 = symAdd(*arena, symFromCAS(*arena, negBVal), symNeg(*arena, symFromCAS(*arena, sqrtDVal)));
        SymExpr* root2Expr = symMul(*arena, num2, symPow(*arena, denSym, symInt(*arena, -1)));
        
        // Emulate equations: x₁ = ... , x₂ = ...
        SymExpr* xVar = symVar(*arena, var);
        SymExpr* x1 = symSubscript(*arena, xVar, symInt(*arena, 1));
        SymExpr* x2 = symSubscript(*arena, xVar, symInt(*arena, 2));
        
        // Combine them in a row for visual display
        // "x₁ = frac1"
        // Wait, PedagogicalLogger logExpr doesn't support Equations natively, it does expressions.
        // We can just log them as expressions. The user sees "Separating into x₁ and x₂:"
        // and then the canvas shows the two fractions.
        
        // To show "x₁ =" we can abuse SymPlusMinus or let the UI handle it. 
        // Actually, we can just show the fractions. 
        // Let's create an Add node with both just so they appear nicely separated?
        // Let's just log them separately.
        
        log.logAction(SolveAction::QUAD_SEPARATE_ROOTS, ctx.expr(root1Expr), MethodId::Quadratic);
        
        CASNumber r1Val = CASNumber::div(CASNumber::add(negBVal, sqrtDVal), twoAVal);
        vpam::ExactVal r1EV = r1Val.toExactVal();
        r1EV.simplify();
        
        log.logAction(SolveAction::QUAD_SIMPLIFY_ROOT, 
            ActionContext().var(var).withArena(arena).val("solution", r1Val).solIdx(1), 
            MethodId::Quadratic);
        log.logAction(SolveAction::QUAD_PRESENT_REAL_SOLUTION, 
            ActionContext().var(var).withArena(arena).val("solution", r1Val).solIdx(1), 
            MethodId::Quadratic);
            
        log.logAction(SolveAction::QUAD_SEPARATE_ROOTS, ctx.expr(root2Expr), MethodId::Quadratic);
        
        CASNumber r2Val = CASNumber::div(CASNumber::sub(negBVal, sqrtDVal), twoAVal);
        vpam::ExactVal r2EV = r2Val.toExactVal();
        r2EV.simplify();
        
        log.logAction(SolveAction::QUAD_SIMPLIFY_ROOT, 
            ActionContext().var(var).withArena(arena).val("solution", r2Val).solIdx(2), 
            MethodId::Quadratic);
        log.logAction(SolveAction::QUAD_PRESENT_REAL_SOLUTION, 
            ActionContext().var(var).withArena(arena).val("solution", r2Val).solIdx(2), 
            MethodId::Quadratic);
            
        result.solutions[0] = r1EV;
        result.solutions[1] = r2EV;
        result.count = 2;
        result.ok = true;
    }
    
    return result;
}

// ════════════════════════════════════════════════════════════════════
// solveLinearTutor
// ════════════════════════════════════════════════════════════════════

SolveResult solveLinearTutor(const SymEquation& eq, char var,
                             PedagogicalLogger& log, SymExprArena* arena) {
    SolveResult result;
    result.variable = var;
    result.ok = false;
    
    const SymPoly& f = eq.lhs;
    CASNumber a = CASNumber::fromExactVal(f.coeffAtExact(1));
    CASNumber b = CASNumber::fromExactVal(f.coeffAtExact(0));
    
    if (a.isZero()) {
        result.error = "Leading coefficient is zero";
        return result;
    }
    
    ActionContext ctx;
    ctx.var(var).deg(1).withArena(arena);
    ctx.val("a", a).val("b", b);
    
    log.logAction(SolveAction::LINEAR_IDENTIFY_COEFFICIENTS, ctx, MethodId::Linear);
    
    if (!arena) return result;
    
    // Isolate variable: ax = -b
    CASNumber negB = CASNumber::neg(b);
    {
        ExactVal aEV = a.toExactVal();
        ExactVal negBEV = negB.toExactVal();
        SymPoly lhsStep = SymPoly::fromTerm(SymTerm::variable(var, aEV.num, aEV.den, 1));
        SymPoly rhsStep = SymPoly::fromConstant(negBEV);
        SymEquation step(lhsStep, rhsStep);
        log.logAction(SolveAction::LINEAR_ISOLATE_VARIABLE, 
            ActionContext().var(var).withArena(arena).snap(&step), MethodId::Linear);
    }
    
    // Divide both sides by a
    CASNumber root = CASNumber::div(negB, a);
    {
        ExactVal rootEV = root.toExactVal();
        rootEV.simplify();
        SymPoly lhsStep = SymPoly::fromTerm(SymTerm::variable(var, 1, 1, 1)); // x
        SymPoly rhsStep = SymPoly::fromConstant(rootEV);
        SymEquation step(lhsStep, rhsStep);
        log.logAction(SolveAction::LINEAR_DIVIDE_BY_COEFFICIENT, 
            ActionContext().var(var).withArena(arena).snap(&step), MethodId::Linear);
            
        // Final solution
        log.logAction(SolveAction::LINEAR_PRESENT_SOLUTION,
            ActionContext().var(var).withArena(arena).val("solution", root).solIdx(0),
            MethodId::Linear);
            
        result.solutions[0] = rootEV;
        result.count = 1;
        result.ok = true;
    }
    
    return result;
}

// ════════════════════════════════════════════════════════════════════
// solveCubicTutor
// ════════════════════════════════════════════════════════════════════

SolveResult solveCubicTutor(const SymEquation& eq, char var,
                            PedagogicalLogger& log, SymExprArena* arena) {
    SolveResult result;
    result.variable = var;
    result.ok = false;
    
    // We already know it's degree 3 (SingleSolver classification)
    const SymPoly& f = eq.lhs;
    CASNumber a = CASNumber::fromExactVal(f.coeffAtExact(3));
    CASNumber b = CASNumber::fromExactVal(f.coeffAtExact(2));
    CASNumber c = CASNumber::fromExactVal(f.coeffAtExact(1));
    CASNumber d = CASNumber::fromExactVal(f.coeffAtExact(0));
    
    if (a.isZero()) {
        result.error = "Leading coefficient is zero";
        return result;
    }
    
    ActionContext ctx;
    ctx.var(var).deg(3).withArena(arena);
    
    log.logAction(SolveAction::IDENTIFY_EQUATION_TYPE, ctx, MethodId::Newton); // We use Newton or general? General fallback to Tutor
    
    // We try to find an integer root in [-10, 10]
    bool foundObj = false;
    CASNumber rootR;
    CASNumber qA, qB, qC; // Coefficients of residual quadratic

    for (int testR = -10; testR <= 10; ++testR) {
        if (testR == 0 && !d.isZero()) continue; // Quick skip if d!=0
        
        CASNumber r = CASNumber::fromInt(testR);
        log.logAction(SolveAction::CUBIC_TRY_ROOT, 
            ActionContext().var(var).withArena(arena).val("root", r), MethodId::General);
            
        // Ruffini
        // q2 = a
        // q1 = b + q2*r
        // q0 = c + q1*r
        // rem = d + q0*r
        
        CASNumber q2 = a;
        CASNumber q1 = CASNumber::add(b, CASNumber::mul(q2, r));
        CASNumber q0 = CASNumber::add(c, CASNumber::mul(q1, r));
        CASNumber rem = CASNumber::add(d, CASNumber::mul(q0, r));
        
        if (rem.isZero()) {
            rootR = r;
            qA = q2;
            qB = q1;
            qC = q0;
            foundObj = true;
            break;
        }
    }
    
    if (!foundObj) {
        // Fallback to Newton
        log.logAction(SolveAction::NEWTON_START, ctx, MethodId::Newton);
        result.error = "No integer roots found in [-10, 10], fallback required";
        return result;
    }
    
    log.logAction(SolveAction::CUBIC_ROOT_FOUND, 
        ActionContext().var(var).withArena(arena).val("root", rootR), MethodId::General);
        
    if (!arena) return result;
    
    // Build synthetic division step (we just log the Ruffini quadratic)
    // Residual: qA x² + qB x + qC = 0
    SymExpr* xVar = symVar(*arena, var);
    SymExpr* qaSym = symFromCAS(*arena, qA);
    SymExpr* qbSym = symFromCAS(*arena, qB);
    SymExpr* qcSym = symFromCAS(*arena, qC);
    
    SymExpr* ax2 = symMul(*arena, qaSym, symPow(*arena, xVar, symInt(*arena, 2)));
    SymExpr* bx  = symMul(*arena, qbSym, xVar);
    SymExpr* poly = symAdd3(*arena, ax2, bx, qcSym);
    
    log.logAction(SolveAction::CUBIC_RESULTING_QUADRATIC, 
        ActionContext().var(var).withArena(arena).expr(poly), MethodId::General);
        
    // Solve residual quadratic using solveQuadraticTutor
    vpam::ExactVal qaEV = qA.toExactVal();
    vpam::ExactVal qbEV = qB.toExactVal();
    vpam::ExactVal qcEV = qC.toExactVal();
    SymPoly residualPoly(var);
    if (!qaEV.isZero()) residualPoly.terms().push_back(SymTerm::variable(var, qaEV.num, qaEV.den, 2));
    if (!qbEV.isZero()) residualPoly.terms().push_back(SymTerm::variable(var, qbEV.num, qbEV.den, 1));
    if (!qcEV.isZero()) residualPoly.terms().push_back(SymTerm::constant(qcEV));
    residualPoly.normalize();
    
    SymEquation residualEq(residualPoly, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
    
    SolveResult quadRes = solveQuadraticTutor(residualEq, var, log, arena);
    
    // Merge results
    result.solutions[0] = rootR.toExactVal();
    result.solutions[0].simplify();
    result.count = 1;
    
    if (quadRes.ok && quadRes.count > 0) {
        if (quadRes.count == 1) {
            result.solutions[1] = quadRes.solutions[0];
            result.count = 2;
        } else if (quadRes.count == 2) {
            result.solutions[1] = quadRes.solutions[0];
            result.solutions[2] = quadRes.solutions[1];
            result.count = 3;
        }
    }
    
    result.ok = true;
    result.numeric = false;
    result.hasComplexRoots = quadRes.hasComplexRoots;
    if (result.hasComplexRoots) {
        result.complexReal = quadRes.complexReal;
        result.complexImagMag = quadRes.complexImagMag;
    }
    
    return result;
}

// ════════════════════════════════════════════════════════════════════
// solveSystem2x2Tutor
// ════════════════════════════════════════════════════════════════════

SystemResult solveSystem2x2Tutor(const LinEq& eq1, const LinEq& eq2,
                                 char var1, char var2, SymExprArena* arena) {
    SystemResult result;
    result.numVars = 2;
    result.vars[0] = var1;
    result.vars[1] = var2;
    result.ok = false;
    
    // a*x + b*y = c
    // d*x + e*y = f
    CASNumber a = cas::CASNumber::fromExactVal(eq1.coeffs[0]);
    CASNumber b = cas::CASNumber::fromExactVal(eq1.coeffs[1]);
    CASNumber c = cas::CASNumber::fromExactVal(eq1.rhs);
    
    CASNumber d = cas::CASNumber::fromExactVal(eq2.coeffs[0]);
    CASNumber e = cas::CASNumber::fromExactVal(eq2.coeffs[1]);
    CASNumber f = cas::CASNumber::fromExactVal(eq2.rhs);
    
    // Calculate determinants
    // D = a*e - b*d
    CASNumber D = CASNumber::sub(CASNumber::mul(a, e), CASNumber::mul(b, d));
    // Dx = c*e - b*f
    CASNumber Dx = CASNumber::sub(CASNumber::mul(c, e), CASNumber::mul(b, f));
    // Dy = a*f - c*d
    CASNumber Dy = CASNumber::sub(CASNumber::mul(a, f), CASNumber::mul(c, d));
    
    // We need a PedagogicalLogger to generate phrases and math chunks.
    PedagogicalLogger pLog;
    
    if (D.isZero()) {
        if (Dx.isZero() && Dy.isZero()) {
            result.error = "Dependent system (infinite solutions)";
            pLog.logNote("D = 0 and Dx=0, Dy=0. System is dependent.", MethodId::Substitution);
        } else {
            result.error = "Incompatible system (no solution)";
            pLog.logNote("D = 0 but Dx, Dy are non-zero. System is incompatible.", MethodId::Substitution);
        }
        for (const auto& step : pLog.steps()) result.steps.copyStep(step);
        return result;
    }
    
    if (!arena) return result;
    
    // --- Step: Calculate D ---
    {
        SymExpr* dVar = symVar(*arena, 'D');
        SymExpr* aSym = symFromCAS(*arena, a);
        SymExpr* eSym = symFromCAS(*arena, e);
        SymExpr* bSym = symFromCAS(*arena, b);
        SymExpr* dSym = symFromCAS(*arena, d);
        
        // a*e - b*d
        SymExpr* valD = symFromCAS(*arena, D);
        
        // We log D = D_val
        SymExpr* ex = symAdd(*arena, dVar, symNeg(*arena, valD));
        // Note: we can't show =, we just show D value
        pLog.logAction(SolveAction::SYSTEM_CRAMER_DETERMINANT, 
            ActionContext().withArena(arena).expr(valD), MethodId::General);
    }
    
    // --- Step: Calculate Dx, Dy ---
    {
        SymExpr* valDx = symFromCAS(*arena, Dx);
        pLog.logAction(SolveAction::SYSTEM_CRAMER_DX_DY, 
            ActionContext().withArena(arena).expr(valDx), MethodId::General);
            
        SymExpr* valDy = symFromCAS(*arena, Dy);
        // We reuse the same action text or just show it
        pLog.logAction(SolveAction::SYSTEM_CRAMER_DX_DY, 
            ActionContext().withArena(arena).expr(valDy), MethodId::General);
    }
    
    // --- Step: Solution x, y ---
    CASNumber solX = CASNumber::div(Dx, D);
    CASNumber solY = CASNumber::div(Dy, D);
    
    {
        SymExpr* valX = symFromCAS(*arena, solX);
        pLog.logAction(SolveAction::SYSTEM_CRAMER_SOLUTION, 
            ActionContext().withArena(arena).expr(valX), MethodId::General);
            
        SymExpr* valY = symFromCAS(*arena, solY);
        pLog.logAction(SolveAction::SYSTEM_CRAMER_SOLUTION, 
            ActionContext().withArena(arena).expr(valY), MethodId::General);
    }
    
    result.solutions[0] = solX.toExactVal();
    result.solutions[0].simplify();
    result.solutions[1] = solY.toExactVal();
    result.solutions[1].simplify();
    result.ok = true;
    
    for (const auto& step : pLog.steps()) {
        result.steps.copyStep(step);
    }
    
    return result;
}

} // namespace cas
