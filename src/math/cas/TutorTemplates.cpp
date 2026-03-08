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
#include "OmniSolver.h"
#include "SymExpr.h"
#include "SymExprArena.h"
#include "SymSimplify.h"

#include <cmath>
#include <vector>

namespace cas {

using vpam::ExactVal;

static constexpr int64_t RADICAL_NUMERATOR = 1;
static constexpr int64_t RADICAL_DENOMINATOR = 2;
static constexpr double SOLUTION_TOLERANCE = 1e-6;

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

static bool isNegativeNumericExpr(const SymExpr* expr) {
    if (!expr) return false;
    if (expr->type == SymExprType::Num) {
        return static_cast<const SymNum*>(expr)->toExactVal().toDouble() < 0.0;
    }
    return expr->type == SymExprType::Neg;
}

static bool needsSubstitutionParens(const SymExpr* expr) {
    if (!expr) return false;
    if (expr->type == SymExprType::Paren) return false;
    if (isNegativeNumericExpr(expr)) return true;

    switch (expr->type) {
        case SymExprType::Num:
        case SymExprType::Var:
        case SymExprType::Subscript:
            return false;
        default:
            return true;
    }
}

static SymExpr* wrapForSubstitution(SymExpr* val, SymExprArena& arena) {
    return needsSubstitutionParens(val) ? symParen(arena, val) : val;
}

static SymExpr* wrapNumericFactorForProduct(SymExpr* val, SymExprArena& arena) {
    if (!val) return val;
    return (val->type == SymExprType::Paren) ? val : symParen(arena, val);
}

static void appendSteps(CASStepLogger& dst, const CASStepLogger& src) {
    for (const auto& step : src.steps()) {
        dst.copyStep(step);
    }
}

static bool isSameEquation(const SymEquation& a, const SymEquation& b) {
    return a.toString() == b.toString();
}

static bool hasLikeTerms(const SymPoly& poly) {
    for (size_t i = 0; i < poly.terms().size(); ++i) {
        const SymTerm& lhs = poly.terms()[i];
        for (size_t j = i + 1; j < poly.terms().size(); ++j) {
            if (lhs.isLikeTerm(poly.terms()[j])) return true;
        }
    }
    return false;
}

static SymEquation moveAllToLHSRaw(const SymEquation& eq) {
    SymPoly rawLhs = eq.lhs;
    SymPoly negRhs = eq.rhs.negate();
    rawLhs.terms().reserve(rawLhs.terms().size() + negRhs.terms().size());
    for (const auto& term : negRhs.terms()) {
        rawLhs.terms().push_back(term);
    }
    return SymEquation(rawLhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
}

static void logTutorSetup(PedagogicalLogger& log, const SymEquation& original,
                          const SymEquation& normalized, char var, int degree) {
    log.logAction(SolveAction::PRESENT_ORIGINAL_EQUATION,
                  ActionContext().var(var).snap(&original), MethodId::General);
    if (!isSameEquation(original, normalized)) {
        log.logAction(SolveAction::NORMALIZE_TO_STANDARD_FORM,
                      ActionContext().var(var).snap(&normalized), MethodId::General);
    }
    log.logAction(SolveAction::IDENTIFY_EQUATION_TYPE,
                  ActionContext().var(var).deg(degree), MethodId::General);
}

static void logTutorEntry(PedagogicalLogger& log, const SymEquation& original,
                          const SymEquation& normalized, char var, int degree) {
    if (log.count() == 0) {
        logTutorSetup(log, original, normalized, var, degree);
    } else {
        log.logAction(SolveAction::IDENTIFY_EQUATION_TYPE,
                      ActionContext().var(var).deg(degree), MethodId::General);
    }
}

static SymExpr* buildSignedMonomialExpr(SymExprArena& arena,
                                        const CASNumber& coeff,
                                        SymExpr* factor,
                                        bool showUnitCoeff = true) {
    CASNumber absCoeff = CASNumber::abs(coeff);
    SymExpr* term = nullptr;

    if (!factor) {
        term = symFromCAS(arena, absCoeff);
    } else if (!showUnitCoeff && absCoeff.isOne()) {
        term = factor;
    } else {
        term = symMulRaw(arena, symFromCAS(arena, absCoeff), factor);
    }

    return coeff.isNegative() ? symNeg(arena, term) : term;
}

static SymExpr* buildQuadraticDisplayExpr(SymExprArena& arena,
                                          char var,
                                          const CASNumber& a,
                                          const CASNumber& b,
                                          const CASNumber& c) {
    SymExpr* x = symVar(arena, var);
    SymExpr* x2 = symPow(arena, x, symInt(arena, 2));

    std::vector<SymExpr*> terms;
    terms.reserve(3);

    if (!a.isZero()) terms.push_back(buildSignedMonomialExpr(arena, a, x2));
    if (!b.isZero()) terms.push_back(buildSignedMonomialExpr(arena, b, x));
    if (!c.isZero() || terms.empty()) terms.push_back(buildSignedMonomialExpr(arena, c, nullptr));

    SymExpr* expr = terms.front();
    for (size_t i = 1; i < terms.size(); ++i) {
        expr = symAddRaw(arena, expr, terms[i]);
    }
    return expr;
}

static SymExpr* makeRawFractionExpr(SymExprArena& arena, SymExpr* numerator, SymExpr* denominator) {
    return symMulRaw(arena, numerator, symPow(arena, denominator, symInt(arena, -1)));
}

void preProcessEquationTutor(SymEquation& eq, PedagogicalLogger& log) {
    SymEquation rawMoved = moveAllToLHSRaw(eq);
    SymEquation normalized = eq.moveAllToLHS();

    bool needsSetToZero = !eq.rhs.isZero();
    bool needsCombine = hasLikeTerms(rawMoved.lhs);

    if (!needsSetToZero && !needsCombine) {
        return;
    }

    char var = eq.var();
    log.logAction(SolveAction::PRESENT_ORIGINAL_EQUATION,
                  ActionContext().var(var).snap(&eq), MethodId::General);

    if (needsSetToZero) {
        log.logAction(SolveAction::PRE_SET_TO_ZERO,
                      ActionContext().var(var).snap(&rawMoved), MethodId::General);
    }

    if (needsCombine) {
        log.logAction(SolveAction::PRE_COMBINE_TERMS,
                      ActionContext().var(var).snap(&normalized), MethodId::General);
    }

    eq = normalized;
}

static bool isNumericExpr(const SymExpr* expr, char var) {
    return expr && !expr->containsVar(var);
}

static bool isHalfPower(const SymExpr* expr, SymExpr*& inner) {
    if (!expr || expr->type != SymExprType::Pow) return false;
    auto* pow = static_cast<const SymPow*>(expr);
    if (pow->exponent->type != SymExprType::Num) return false;
    auto* num = static_cast<const SymNum*>(pow->exponent);
    if (!num->isPureRational()) return false;
    vpam::ExactVal ev = num->toExactVal();
    if (ev.num == RADICAL_NUMERATOR && ev.den == RADICAL_DENOMINATOR) {
        inner = pow->base;
        return true;
    }
    return false;
}

static bool extractRadicalSide(SymExpr* expr,
                               SymExpr*& radicalExpr,
                               SymExpr*& radicalInner, SymExpr*& otherTerms) {
    radicalExpr = nullptr;
    radicalInner = nullptr;
    otherTerms = nullptr;

    SymExpr* inner = nullptr;
    if (isHalfPower(expr, inner)) {
        radicalExpr = expr;
        radicalInner = inner;
        otherTerms = nullptr;
        return true;
    }

    if (expr && expr->type == SymExprType::Add) {
        auto* add = static_cast<SymAdd*>(expr);
        SymExpr* foundRad = nullptr;
        SymExpr* foundInner = nullptr;
        std::vector<SymExpr*> rest;
        for (uint16_t i = 0; i < add->count; ++i) {
            SymExpr* term = add->terms[i];
            SymExpr* termInner = nullptr;
            if (!foundRad && isHalfPower(term, termInner)) {
                foundRad = term;
                foundInner = termInner;
            } else {
                rest.push_back(term);
            }
        }
        if (foundRad && !rest.empty()) {
            radicalExpr = foundRad;
            radicalInner = foundInner;
            if (rest.size() == 1) {
                otherTerms = rest[0];
            }
            return true;
        }
    }

    return false;
}

static bool isLogFunc(SymExpr* expr, SymFuncKind& kind, SymExpr*& arg) {
    if (!expr || expr->type != SymExprType::Func) return false;
    auto* fn = static_cast<SymFunc*>(expr);
    if (fn->kind != SymFuncKind::Ln && fn->kind != SymFuncKind::Log10) return false;
    kind = fn->kind;
    arg = fn->argument;
    return true;
}

static bool collectLogArgs(SymExpr* expr, SymFuncKind& kind, std::vector<SymExpr*>& args) {
    SymExpr* arg = nullptr;
    SymFuncKind currentKind = SymFuncKind::Ln;
    if (isLogFunc(expr, currentKind, arg)) {
        if (args.empty()) {
            kind = currentKind;
        } else if (kind != currentKind) {
            return false;
        }
        args.push_back(arg);
        return true;
    }

    if (!expr || expr->type != SymExprType::Add) return false;
    auto* add = static_cast<SymAdd*>(expr);
    for (uint16_t i = 0; i < add->count; ++i) {
        if (!collectLogArgs(add->terms[i], kind, args)) {
            return false;
        }
    }
    return !args.empty();
}

static SymExpr* multiplyArgs(SymExprArena& arena, const std::vector<SymExpr*>& args) {
    if (args.empty()) return symInt(arena, 1);
    SymExpr* out = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        out = symMul(arena, out, args[i]);
    }
    return out;
}

static SymExpr* makeExponentialValue(SymExprArena& arena, SymFuncKind kind, SymExpr* rhs) {
    if (kind == SymFuncKind::Ln) {
        return symFunc(arena, SymFuncKind::Exp, rhs);
    }
    return symPow(arena, symInt(arena, 10), rhs);
}

static bool isExponentialForm(SymExpr* expr, char var, SymExpr*& scale,
                              SymExpr*& base, SymExpr*& exponent, bool& naturalBase) {
    scale = nullptr;
    base = nullptr;
    exponent = nullptr;
    naturalBase = false;

    if (!expr) return false;

    if (expr->type == SymExprType::Func) {
        auto* fn = static_cast<SymFunc*>(expr);
        if (fn->kind == SymFuncKind::Exp) {
            base = nullptr;
            exponent = fn->argument;
            naturalBase = true;
            return true;
        }
    }

    if (expr->type == SymExprType::Pow) {
        auto* pw = static_cast<SymPow*>(expr);
        if (!pw->base->containsVar(var) && pw->exponent->containsVar(var)) {
            base = pw->base;
            exponent = pw->exponent;
            naturalBase = false;
            return true;
        }
    }

    if (expr->type == SymExprType::Mul) {
        auto* mul = static_cast<SymMul*>(expr);
        for (uint16_t i = 0; i < mul->count; ++i) {
            SymExpr* factor = mul->factors[i];
            SymExpr* localScale = nullptr;
            SymExpr* localBase = nullptr;
            SymExpr* localExponent = nullptr;
            bool localNatural = false;
            if (isExponentialForm(factor, var, localScale, localBase, localExponent, localNatural)) {
                for (uint16_t j = 0; j < mul->count; ++j) {
                    if (j == i) continue;
                    if (mul->factors[j]->containsVar(var)) return false;
                }
                if (mul->count == 2) {
                    scale = (i == 0) ? mul->factors[1] : mul->factors[0];
                }
                base = localBase;
                exponent = localExponent;
                naturalBase = localNatural;
                return true;
            }
        }
    }

    return false;
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

    SymEquation normalized = eq.moveAllToLHS();
    logTutorEntry(log, eq, normalized, var, 2);

    const SymPoly& f = normalized.lhs;
    
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
    ActionContext coeffCtx = ctx;
    if (arena) {
        coeffCtx.expr(buildQuadraticDisplayExpr(*arena, var, a, b, c));
    }
    log.logAction(SolveAction::QUAD_IDENTIFY_COEFFICIENTS, coeffCtx, MethodId::Quadratic);
    
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
        SymExpr* fourAC = symMul3Raw(*arena, symInt(*arena, 4), aVar, cVar);
        // b² - 4ac
        SymExpr* discrim = symAddRaw(*arena, b2, symNeg(*arena, fourAC));
        // √(b² - 4ac)
        SymExpr* sqrtD = symPow(*arena, discrim, symFrac(*arena, 1, 2));
        // -b ± √(b² - 4ac)
        SymExpr* num = symPlusMinus(*arena, negB, sqrtD);
        // 2a
        SymExpr* den = symMulRaw(*arena, symInt(*arena, 2), aVar);
        // Fraction
        SymExpr* frac = makeRawFractionExpr(*arena, num, den);
        
        log.logAction(SolveAction::QUAD_SHOW_GENERAL_FORMULA, ctx.expr(frac), MethodId::Quadratic);
    }
    
    // ── 3. Substitute values ─────────────────────────────────────────
    {
        SymExpr* aNum = symFromCAS(*arena, a);
        SymExpr* bNum = symFromCAS(*arena, b);
        SymExpr* cNum = symFromCAS(*arena, c);
        SymExpr* aSub = wrapForSubstitution(aNum, *arena);
        SymExpr* bSub = wrapForSubstitution(bNum, *arena);
        SymExpr* cSub = wrapForSubstitution(cNum, *arena);
        SymExpr* aProd = wrapNumericFactorForProduct(aSub, *arena);
        SymExpr* cProd = wrapNumericFactorForProduct(cSub, *arena);
        
        SymExpr* negB = symNeg(*arena, bSub);
        SymExpr* b2 = symPow(*arena, bSub, symInt(*arena, 2));
        SymExpr* fourAC = symMul3Raw(*arena, symInt(*arena, 4), aProd, cProd);
        SymExpr* discrim = symAddRaw(*arena, b2, symNeg(*arena, fourAC));
        SymExpr* sqrtD = symPow(*arena, discrim, symFrac(*arena, 1, 2));
        SymExpr* num = symPlusMinus(*arena, negB, sqrtD);
        SymExpr* den = symMulRaw(*arena, symInt(*arena, 2), aProd);
        SymExpr* frac = makeRawFractionExpr(*arena, num, den);
        
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
        SymExpr* frac    = makeRawFractionExpr(*arena, num, denSym);
        
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
        SymExpr* frac    = makeRawFractionExpr(*arena, num, denSym);
        
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
        SymExpr* num1 = symAddRaw(*arena, symFromCAS(*arena, negBVal), symFromCAS(*arena, sqrtDVal));
        SymExpr* root1Expr = makeRawFractionExpr(*arena, num1, denSym);
        
        // x₂ = (-b - √D) / 2a
        SymExpr* num2 = symAddRaw(*arena, symFromCAS(*arena, negBVal),
                                  symNeg(*arena, symFromCAS(*arena, sqrtDVal)));
        SymExpr* root2Expr = makeRawFractionExpr(*arena, num2, denSym);
        
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

    SymEquation normalized = eq.moveAllToLHS();
    logTutorEntry(log, eq, normalized, var, 1);

    const SymPoly& f = normalized.lhs;
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

    SymEquation normalized = eq.moveAllToLHS();
    logTutorEntry(log, eq, normalized, var, 3);

    const SymPoly& f = normalized.lhs;
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
    SymExpr* poly = buildQuadraticDisplayExpr(*arena, var, qA, qB, qC);
    
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

static bool valuesMatch(SymExpr* lhs, SymExpr* rhs, double x) {
    double lhsVal = lhs->evaluate(x);
    double rhsVal = rhs->evaluate(x);
    if (!std::isfinite(lhsVal) || !std::isfinite(rhsVal)) return false;
    return std::abs(lhsVal - rhsVal) < SOLUTION_TOLERANCE;
}

bool solveLogarithmicTutor(SymExpr* lhs, SymExpr* rhs, char var,
                           SymExprArena& arena, OmniResult& result) {
    result = OmniResult();
    result.variable = var;

    SymFuncKind kind = SymFuncKind::Ln;
    std::vector<SymExpr*> args;
    SymExpr* otherSide = nullptr;

    if (collectLogArgs(lhs, kind, args) && isNumericExpr(rhs, var)) {
        otherSide = rhs;
    } else {
        args.clear();
        if (!collectLogArgs(rhs, kind, args) || !isNumericExpr(lhs, var)) {
            return false;
        }
        otherSide = lhs;
    }

    PedagogicalLogger log;
    result.classification = EquationClass::SimpleInverse;

    log.logNote(
        std::string("This is a logarithmic equation. The base is ") +
        ((kind == SymFuncKind::Ln) ? "e" : "10") +
        ", and we identify the argument before solving.",
        MethodId::General);

    SymExpr* combinedArg = (args.size() == 1) ? args[0] : multiplyArgs(arena, args);
    if (args.size() > 1) {
        log.logAction(
            SolveAction::LOG_APPLY_PROPERTIES,
            ActionContext().var(var).withArena(&arena)
                .expr(symFunc(arena, kind, combinedArg)),
            MethodId::General);
    }

    SymExpr* transformedRhs = nullptr;
    SymFuncKind rhsKind = SymFuncKind::Ln;
    SymExpr* rhsArg = nullptr;
    if (isLogFunc(otherSide, rhsKind, rhsArg) && rhsKind == kind) {
        transformedRhs = rhsArg;
    } else {
        transformedRhs = makeExponentialValue(arena, kind, otherSide);
    }

    log.logAction(
        SolveAction::LOG_CONVERT_TO_EXPONENTIAL,
        ActionContext().var(var).withArena(&arena)
            .expr(symAdd(arena, combinedArg, symNeg(arena, transformedRhs))),
        MethodId::General);

    OmniSolver solver;
    OmniResult nested = solver.solve(combinedArg, transformedRhs, var, arena);
    if (!nested.ok) {
        return false;
    }

    appendSteps(log, nested.steps);
    for (const auto& candidate : nested.solutions) {
        double x = candidate.isExact ? candidate.exact.toDouble() : candidate.numeric;
        if (valuesMatch(lhs, rhs, x)) {
            result.solutions.push_back(candidate);
        }
    }
    appendSteps(result.steps, log);
    result.ok = !result.solutions.empty();
    if (!result.ok && !nested.solutions.empty()) {
        result.error = "No solution satisfies the original logarithmic equation";
    }
    result.hasComplexRoots = nested.hasComplexRoots;
    result.complexReal = nested.complexReal;
    result.complexImagMag = nested.complexImagMag;
    return true;
}

bool solveExponentialTutor(SymExpr* lhs, SymExpr* rhs, char var,
                           SymExprArena& arena, OmniResult& result) {
    result = OmniResult();
    result.variable = var;

    SymExpr* expSide = nullptr;
    SymExpr* otherSide = nullptr;
    SymExpr* offset = nullptr;
    SymExpr* scale = nullptr;
    SymExpr* base = nullptr;
    SymExpr* exponent = nullptr;
    bool naturalBase = false;

    auto trySide = [&](SymExpr* candidate, SymExpr* opposite) -> bool {
        SymExpr* localScale = nullptr;
        SymExpr* localBase = nullptr;
        SymExpr* localExponent = nullptr;
        bool localNatural = false;

        if (isExponentialForm(candidate, var, localScale, localBase, localExponent, localNatural)) {
            expSide = candidate;
            otherSide = opposite;
            scale = localScale;
            base = localBase;
            exponent = localExponent;
            naturalBase = localNatural;
            offset = nullptr;
            return true;
        }

        if (candidate && candidate->type == SymExprType::Add) {
            auto* add = static_cast<SymAdd*>(candidate);
            if (add->count == 2) {
                for (uint16_t i = 0; i < add->count; ++i) {
                    if (isExponentialForm(add->terms[i], var, localScale, localBase,
                                          localExponent, localNatural)) {
                        SymExpr* otherTerm = add->terms[1 - i];
                        if (otherTerm->containsVar(var)) return false;
                        expSide = add->terms[i];
                        otherSide = opposite;
                        scale = localScale;
                        base = localBase;
                        exponent = localExponent;
                        naturalBase = localNatural;
                        offset = otherTerm;
                        return true;
                    }
                }
            }
        }

        return false;
    };

    if (!trySide(lhs, rhs) && !trySide(rhs, lhs)) {
        return false;
    }

    PedagogicalLogger log;
    result.classification = EquationClass::SimpleInverse;

    SymExpr* isolatedRhs = otherSide;
    if (offset) {
        isolatedRhs = SymSimplify::simplify(symAdd(arena, otherSide, symNeg(arena, offset)), arena);
    }
    if (scale) {
        isolatedRhs = SymSimplify::simplify(
            symMul(arena, isolatedRhs, symPow(arena, scale, symInt(arena, -1))), arena);
    }

    SymExpr* targetRhs = nullptr;
    bool matchedBases = false;
    if (naturalBase && isolatedRhs->type == SymExprType::Func &&
        static_cast<SymFunc*>(isolatedRhs)->kind == SymFuncKind::Exp) {
        targetRhs = static_cast<SymFunc*>(isolatedRhs)->argument;
        matchedBases = true;
    } else if (!naturalBase && isolatedRhs->type == SymExprType::Pow) {
        auto* rhsPow = static_cast<SymPow*>(isolatedRhs);
        if (rhsPow->base == base) {
            targetRhs = rhsPow->exponent;
            matchedBases = true;
        }
    }

    if (matchedBases) {
        log.logAction(
            SolveAction::EXP_EQUAL_BASES,
            ActionContext().var(var).withArena(&arena)
                .expr(symAdd(arena, exponent, symNeg(arena, targetRhs))),
            MethodId::General);
    } else {
        // `isolatedRhs` is guaranteed constant here, so evaluating at x = 0 is
        // simply a convenient way to obtain its numeric value for the log domain check.
        if (isNumericExpr(isolatedRhs, var) && isolatedRhs->evaluate(0.0) <= 0.0) {
            return false;
        }
        targetRhs = naturalBase
            ? symFunc(arena, SymFuncKind::Ln, isolatedRhs)
            : SymSimplify::simplify(
                  symMul(arena,
                         symFunc(arena, SymFuncKind::Ln, isolatedRhs),
                         symPow(arena, symFunc(arena, SymFuncKind::Ln, base), symInt(arena, -1))),
                  arena);
        log.logAction(
            SolveAction::EXP_LOG_BOTH_SIDES,
            ActionContext().var(var).withArena(&arena)
                .expr(targetRhs),
            MethodId::General);
    }

    OmniSolver solver;
    OmniResult nested = solver.solve(exponent, targetRhs, var, arena);
    if (!nested.ok) {
        return false;
    }

    appendSteps(log, nested.steps);
    appendSteps(result.steps, log);
    result.solutions = nested.solutions;
    result.ok = true;
    return true;
}

bool solveRadicalTutor(SymExpr* lhs, SymExpr* rhs, char var,
                       SymExprArena& arena, OmniResult& result) {
    result = OmniResult();
    result.variable = var;

    SymExpr* radicalExpr = nullptr;
    SymExpr* radicalInner = nullptr;
    SymExpr* extraTerms = nullptr;
    SymExpr* otherSide = nullptr;

    if (extractRadicalSide(lhs, radicalExpr, radicalInner, extraTerms)) {
        otherSide = rhs;
    } else if (extractRadicalSide(rhs, radicalExpr, radicalInner, extraTerms)) {
        otherSide = lhs;
    } else {
        return false;
    }

    PedagogicalLogger log;
    result.classification = EquationClass::MixedTranscendental;

    SymExpr* isolatedRhs = otherSide;
    if (extraTerms) {
        isolatedRhs = SymSimplify::simplify(symAdd(arena, otherSide, symNeg(arena, extraTerms)), arena);
    }

    log.logAction(
        SolveAction::RADICAL_ISOLATE,
        ActionContext().var(var).withArena(&arena)
            .expr(symAdd(arena, radicalExpr, symNeg(arena, isolatedRhs))),
        MethodId::General);

    SymExpr* squaredRhs = SymSimplify::simplify(
        symPow(arena, isolatedRhs, symInt(arena, 2)), arena);
    log.logAction(
        SolveAction::RADICAL_SQUARE_BOTH_SIDES,
        ActionContext().var(var).withArena(&arena)
            .expr(symAdd(arena, radicalInner, symNeg(arena, squaredRhs))),
        MethodId::General);

    OmniSolver solver;
    OmniResult nested = solver.solve(radicalInner, squaredRhs, var, arena);
    if (!nested.ok) {
        return false;
    }

    appendSteps(log, nested.steps);

    for (const auto& candidate : nested.solutions) {
        double x = candidate.isExact ? candidate.exact.toDouble() : candidate.numeric;
        double lhsVal = lhs->evaluate(x);
        double rhsVal = rhs->evaluate(x);
        bool valid = valuesMatch(lhs, rhs, x);

        log.logAction(
            SolveAction::RADICAL_CHECK_EXTRANEOUS,
            ActionContext().var(var).withArena(&arena)
                .val("candidate", CASNumber::fromDouble(x))
                .val("lhs", CASNumber::fromDouble(lhsVal))
                .val("rhs", CASNumber::fromDouble(rhsVal))
                .val("valid", CASNumber::fromInt(valid ? 1 : 0)),
            MethodId::General);

        if (valid) {
            result.solutions.push_back(candidate);
        }
    }

    appendSteps(result.steps, log);

    if (result.solutions.empty()) {
        result.error = "All candidates were extraneous";
        result.ok = false;
        return true;
    }

    result.ok = true;
    return true;
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
