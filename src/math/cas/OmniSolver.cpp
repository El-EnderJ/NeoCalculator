/**
 * OmniSolver.cpp — Universal single-variable equation solver.
 *
 * Pipeline:
 *   1. Normalize: lhs - rhs → f(x) = 0
 *   2. Classify:  polynomial / simpleInverse / mixedTranscendental
 *   3. Dispatch:  SingleSolver / AlgebraicIsolator / HybridNewton
 *   4. Log every decision to CASStepLogger
 *
 * Part of: NumOS Pro-CAS — Phase 4 (Omni-Solver)
 */

#include "OmniSolver.h"
#include "SymDiff.h"
#include "SymSimplify.h"
#include "TutorTemplates.h"

#include <cmath>
#include <cstdio>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Classification name helper
// ════════════════════════════════════════════════════════════════════

const char* equationClassName(EquationClass c) {
    switch (c) {
        case EquationClass::Polynomial:          return "Polynomial";
        case EquationClass::SimpleInverse:       return "Simple inverse";
        case EquationClass::MixedTranscendental: return "Mixed transcendental";
        default: return "Unknown";
    }
}

// ════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════

OmniSolver::OmniSolver() {}

// ════════════════════════════════════════════════════════════════════
// classify — Determine equation type
// ════════════════════════════════════════════════════════════════════

EquationClass OmniSolver::classify(SymExpr* f, char var) {
    if (!f) return EquationClass::MixedTranscendental;

    // 1. If entire expression is polynomial, use polynomial path
    if (f->isPolynomial()) return EquationClass::Polynomial;

    // 2. Check for simple inverse pattern:
    //    The expression is of the form  Func(arg) + const  or  Func(arg) - const
    //    where arg is a simple variable reference and const is a pure number.

    // Strip Add(Func(...), Num) pattern → simple inverse
    if (f->type == SymExprType::Add) {
        auto* add = static_cast<SymAdd*>(f);
        if (add->count == 2) {
            // Look for Func(var) + const  or  const + Func(var)
            SymExpr* funcPart = nullptr;
            SymExpr* constPart = nullptr;

            for (uint16_t i = 0; i < 2; ++i) {
                if (add->terms[i]->type == SymExprType::Num ||
                    (add->terms[i]->type == SymExprType::Neg &&
                     static_cast<SymNeg*>(add->terms[i])->child->type == SymExprType::Num))
                {
                    constPart = add->terms[i];
                } else {
                    funcPart = add->terms[i];
                }
            }

            if (funcPart && constPart) {
                if (extractSimpleFunc(funcPart, var) != nullptr)
                    return EquationClass::SimpleInverse;
                if (extractSimplePow(funcPart, var) != nullptr)
                    return EquationClass::SimpleInverse;
            }
        }
    }

    // Bare function: Func(var) = 0  → inverse (though c = 0)
    if (extractSimpleFunc(f, var) != nullptr)
        return EquationClass::SimpleInverse;

    // 3. Fallback: mixed transcendental
    return EquationClass::MixedTranscendental;
}

// ════════════════════════════════════════════════════════════════════
// extractSimpleFunc — Pattern: Func(var_only)
// ════════════════════════════════════════════════════════════════════

SymFunc* OmniSolver::extractSimpleFunc(SymExpr* expr, char var) {
    if (!expr || expr->type != SymExprType::Func) return nullptr;
    auto* func = static_cast<SymFunc*>(expr);

    // Argument must be a simple variable or constant-scaled variable
    SymExpr* arg = func->argument;
    if (!arg) return nullptr;

    // Direct variable: sin(x), ln(x), etc.
    if (arg->type == SymExprType::Var &&
        static_cast<SymVar*>(arg)->name == var)
    {
        return func;
    }

    // Scaled variable: sin(2x) → still invertible
    if (arg->type == SymExprType::Mul) {
        auto* mul = static_cast<SymMul*>(arg);
        bool hasVar = false, allOtherConst = true;
        for (uint16_t i = 0; i < mul->count; ++i) {
            if (mul->factors[i]->containsVar(var)) {
                if (mul->factors[i]->type == SymExprType::Var)
                    hasVar = true;
                else
                    allOtherConst = false;
            }
        }
        if (hasVar && allOtherConst) return func;
    }

    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// extractSimplePow — Pattern: x^const or const^x (simple power)
// ════════════════════════════════════════════════════════════════════

SymPow* OmniSolver::extractSimplePow(SymExpr* expr, char var) {
    if (!expr || expr->type != SymExprType::Pow) return nullptr;
    auto* pow = static_cast<SymPow*>(expr);

    // x^n where n is constant → polynomial should have caught it,
    //   but this handles non-integer exponents like x^(1/2)
    if (pow->base->type == SymExprType::Var &&
        static_cast<SymVar*>(pow->base)->name == var &&
        !pow->exponent->containsVar(var))
    {
        return pow;
    }

    // a^x where a is constant → exponential, invertible via log
    if (!pow->base->containsVar(var) &&
        pow->exponent->type == SymExprType::Var &&
        static_cast<SymVar*>(pow->exponent)->name == var)
    {
        return pow;
    }

    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// applyInverse — Symbolic inverse function application
// ════════════════════════════════════════════════════════════════════

SymExpr* OmniSolver::applyInverse(SymFuncKind kind, SymExpr* val,
                                   SymExprArena& arena) {
    switch (kind) {
        case SymFuncKind::Sin:
            return symFunc(arena, SymFuncKind::ArcSin, val);
        case SymFuncKind::Cos:
            return symFunc(arena, SymFuncKind::ArcCos, val);
        case SymFuncKind::Tan:
            return symFunc(arena, SymFuncKind::ArcTan, val);
        case SymFuncKind::ArcSin:
            return symFunc(arena, SymFuncKind::Sin, val);
        case SymFuncKind::ArcCos:
            return symFunc(arena, SymFuncKind::Cos, val);
        case SymFuncKind::ArcTan:
            return symFunc(arena, SymFuncKind::Tan, val);
        case SymFuncKind::Ln:
            // ln(x) = c  → x = e^c
            return symFunc(arena, SymFuncKind::Exp, val);
        case SymFuncKind::Exp:
            // e^x = c  → x = ln(c)
            return symFunc(arena, SymFuncKind::Ln, val);
        case SymFuncKind::Log10: {
            // log10(x) = c → x = 10^c
            return symPow(arena, symInt(arena, 10), val);
        }
        default:
            return nullptr;  // Not invertible
    }
}

// ════════════════════════════════════════════════════════════════════
// solve — Main entry: lhs = rhs
// ════════════════════════════════════════════════════════════════════

OmniResult OmniSolver::solve(SymExpr* lhs, SymExpr* rhs, char var,
                              SymExprArena& arena) {
    OmniResult tutorResult;
    if (solveLogarithmicTutor(lhs, rhs, var, arena, tutorResult) ||
        solveExponentialTutor(lhs, rhs, var, arena, tutorResult) ||
        solveRadicalTutor(lhs, rhs, var, arena, tutorResult))
    {
        return tutorResult;
    }

    // Normalize to f(x) = 0: f(x) = lhs - rhs
    SymExpr* f;
    if (rhs->type == SymExprType::Num &&
        static_cast<SymNum*>(rhs)->_coeff.isZero())
    {
        // Already in f(x) = 0 form
        f = lhs;
    } else {
        // f(x) = lhs - rhs = lhs + (-rhs)
        f = symAdd(arena, lhs, symNeg(arena, rhs));
        f = SymSimplify::simplify(f, arena);
    }

    return solveExpr(f, var, arena);
}

// ════════════════════════════════════════════════════════════════════
// solveExpr — Solve f(x) = 0
// ════════════════════════════════════════════════════════════════════

OmniResult OmniSolver::solveExpr(SymExpr* f, char var, SymExprArena& arena) {
    OmniResult result;
    result.variable = var;

    if (!f) {
        result.error = "Null expression";
        return result;
    }

    // Step 0: Simplify the expression first (Phase 3 multi-pass)
    f = SymSimplify::simplify(f, arena);

    // Step 1: Try analytic isolation (var appears exactly once)
    if (countVar(f, var) == 1) {
        result.steps.logNote(
            "Variable appears exactly once: attempting analytic isolation.",
            MethodId::General);
        if (solveAnalytic(f, var, arena, result))
            return result;
    }

    // Step 2: Classify
    EquationClass cls = classify(f, var);
    result.classification = cls;

    // Step 3: Dispatch
    bool solved = false;
    switch (cls) {
        case EquationClass::Polynomial:
            solved = solvePolynomial(f, var, arena, result);
            break;
        case EquationClass::SimpleInverse:
            solved = solveInverse(f, var, arena, result);
            break;
        case EquationClass::MixedTranscendental:
            solved = solveNewton(f, var, arena, result);
            break;
    }

    result.ok = solved;
    return result;
}

// ════════════════════════════════════════════════════════════════════
// solvePolynomial — Convert to SymPoly, delegate to SingleSolver
// ════════════════════════════════════════════════════════════════════

bool OmniSolver::solvePolynomial(SymExpr* f, char var, SymExprArena& arena,
                                  OmniResult& result) {
    // Convert SymExpr (known polynomial) to SymPoly
    SymPoly poly = f->toSymPoly(var);

    // Build a SymEquation: poly = 0
    SymEquation eq(poly, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));

    // Delegate to SingleSolver (pass arena for VPAM math rendering)
    SingleSolver solver;
    SolveResult sres = solver.solve(eq, var, &arena);

    // Copy steps preserving StepKind (Transform, Annotation, Result, etc.)
    for (const auto& step : sres.steps.steps()) {
        result.steps.copyStep(step);
    }

    if (!sres.ok) {
        result.error = sres.error;
        return false;
    }

    // Propagate complex roots from SingleSolver
    if (sres.hasComplexRoots) {
        result.hasComplexRoots = true;
        result.complexReal     = sres.complexReal;
        result.complexImagMag  = sres.complexImagMag;
        // ok=true but no real solutions — display will show complex roots
        return true;
    }

    // Handle count==0 with ok==true (identity: 0=0)
    if (sres.count == 0) {
        // No real solutions and no complex roots — identity or degenerate
        return true;
    }

    // Convert SolveResult solutions to OmniSolution
    for (uint8_t i = 0; i < sres.count; ++i) {
        OmniSolution sol;
        sol.exact   = sres.solutions[i];
        sol.numeric = sres.solutions[i].toDouble();
        sol.isExact = !sres.numeric;
        result.solutions.push_back(sol);
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════
// solveInverse — f(x) = c → x = f⁻¹(c)
//
// Patterns handled:
//   Func(x) + const = 0  →  Func(x) = -const  →  x = Func⁻¹(-const)
//   Func(k·x) + const    →  k·x = Func⁻¹(-const)  →  x = Func⁻¹(-const)/k
//   x^n + const = 0      →  x = ⁿ√(-const)          (for appropriate n)
//   a^x + const = 0      →  x = log_a(-const)
// ════════════════════════════════════════════════════════════════════

bool OmniSolver::solveInverse(SymExpr* f, char var, SymExprArena& arena,
                               OmniResult& result) {
    // Try to decompose f into func_part + const_part = 0
    SymExpr* funcPart  = nullptr;
    SymExpr* constPart = nullptr;

    if (f->type == SymExprType::Add) {
        auto* add = static_cast<SymAdd*>(f);
        if (add->count == 2) {
            for (uint16_t i = 0; i < 2; ++i) {
                SymExpr* term = add->terms[i];
                bool isConst = false;
                if (term->type == SymExprType::Num) isConst = true;
                if (term->type == SymExprType::Neg &&
                    static_cast<SymNeg*>(term)->child->type == SymExprType::Num)
                    isConst = true;

                if (isConst) constPart = term;
                else funcPart = term;
            }
        }
    }

    // Bare function: Func(x) = 0 → constPart = 0
    if (!funcPart && !constPart) {
        funcPart = f;
        constPart = symInt(arena, 0);
    }

    if (!funcPart) {
        // Can't decompose — fall back to Newton
        result.steps.logNote(
            "Inverse: could not decompose expression, using Newton-Raphson",
            MethodId::General);
        return solveNewton(f, var, arena, result);
    }

    // The constant to invert: func(x) = -constPart
    SymExpr* rhs = symNeg(arena, constPart);
    rhs = SymSimplify::simplify(rhs, arena);

    // ── Case A: Func(arg) pattern ──
    SymFunc* func = extractSimpleFunc(funcPart, var);
    if (func) {
        SymExpr* arg = func->argument;

        result.steps.logNote(
            "Inverse: " + std::string(symFuncName(func->kind)) +
            "(arg) = " + rhs->toString(),
            MethodId::General);

        // Apply inverse: arg = Func⁻¹(rhs)
        SymExpr* inverted = applyInverse(func->kind, rhs, arena);
        if (!inverted) {
            result.steps.logNote(
                "Inverse: function not invertible, using Newton-Raphson",
                MethodId::Newton);
            return solveNewton(f, var, arena, result);
        }

        inverted = SymSimplify::simplify(inverted, arena);

        // If arg is just the variable, we're done
        if (arg->type == SymExprType::Var &&
            static_cast<SymVar*>(arg)->name == var)
        {
            OmniSolution sol;
            sol.numeric  = inverted->evaluate(0.0);
            sol.exact    = vpam::ExactVal::fromDouble(sol.numeric);
            sol.exact.simplify();
            sol.isExact  = false;  // transcendental result
            sol.symbolic = inverted;
            result.solutions.push_back(sol);

            result.steps.logNote(
                "Solution: " + std::string(1, var) + " = " + inverted->toString(),
                MethodId::General);

            return true;
        }

        // If arg is k·x, solve k·x = inverted → x = inverted/k
        if (arg->type == SymExprType::Mul) {
            auto* mul = static_cast<SymMul*>(arg);
            SymExpr* scale = nullptr;
            for (uint16_t i = 0; i < mul->count; ++i) {
                if (!mul->factors[i]->containsVar(var)) {
                    scale = mul->factors[i];
                }
            }
            if (scale) {
                // x = inverted / scale
                SymExpr* sol_expr = symMul(arena,
                    inverted,
                    symPow(arena, scale, symInt(arena, -1)));
                sol_expr = SymSimplify::simplify(sol_expr, arena);

                OmniSolution sol;
                sol.numeric  = sol_expr->evaluate(0.0);
                sol.exact    = vpam::ExactVal::fromDouble(sol.numeric);
                sol.exact.simplify();
                sol.isExact  = false;
                sol.symbolic = sol_expr;
                result.solutions.push_back(sol);

                result.steps.logNote(
                    "Solution: " + std::string(1, var) + " = " + sol_expr->toString(),
                    MethodId::General);
                return true;
            }
        }

        // Fallback
        return solveNewton(f, var, arena, result);
    }

    // ── Case B: x^n pattern ──
    SymPow* pw = extractSimplePow(funcPart, var);
    if (pw) {
        // x^n = rhs
        if (pw->base->type == SymExprType::Var &&
            static_cast<SymVar*>(pw->base)->name == var &&
            !pw->exponent->containsVar(var))
        {
            // x = rhs^(1/n)
            SymExpr* recipExp = symPow(arena, pw->exponent, symInt(arena, -1));
            recipExp = SymSimplify::simplify(recipExp, arena);
            SymExpr* sol_expr = symPow(arena, rhs, recipExp);
            sol_expr = SymSimplify::simplify(sol_expr, arena);

            double numericVal = sol_expr->evaluate(0.0);

            result.steps.logNote(
                "Inverse power: " + std::string(1, var) + "\xE2\x81\xBF = " + rhs->toString() +
                " \xE2\x86\x92 " + std::string(1, var) + " = " + sol_expr->toString(),
                MethodId::General);

            OmniSolution sol;
            sol.numeric  = numericVal;
            sol.exact    = vpam::ExactVal::fromDouble(numericVal);
            sol.exact.simplify();
            sol.isExact  = false;
            sol.symbolic = sol_expr;
            result.solutions.push_back(sol);

            // Check for second root if exponent is even integer
            if (pw->exponent->type == SymExprType::Num) {
                const auto* eSn = static_cast<const SymNum*>(pw->exponent);
                if (eSn->isPureRational() && eSn->_coeff.isInteger()
                    && eSn->_coeff.toInt64() % 2 == 0 && numericVal != 0.0) {
                    OmniSolution sol2;
                    sol2.numeric  = -numericVal;
                    sol2.exact    = vpam::ExactVal::fromDouble(-numericVal);
                    sol2.exact.simplify();
                    sol2.isExact  = false;
                    sol2.symbolic = symNeg(arena, sol_expr);
                    result.solutions.push_back(sol2);

                    result.steps.logNote(
                        "Second solution (even exponent): " +
                        std::string(1, var) + " = " + sol2.symbolic->toString(),
                        MethodId::General);
                }
            }

            return true;
        }

        // a^x = rhs → x = ln(rhs) / ln(a)
        if (!pw->base->containsVar(var) &&
            pw->exponent->containsVar(var))
        {
            SymExpr* lnRhs  = symFunc(arena, SymFuncKind::Ln, rhs);
            SymExpr* lnBase = symFunc(arena, SymFuncKind::Ln, pw->base);
            SymExpr* sol_expr = symMul(arena,
                lnRhs,
                symPow(arena, lnBase, symInt(arena, -1)));
            sol_expr = SymSimplify::simplify(sol_expr, arena);

            result.steps.logNote(
                "Exponential inverse: a\xCB\xA3 = c \xE2\x86\x92 x = ln(c)/ln(a)",
                MethodId::General);

            OmniSolution sol;
            sol.numeric  = sol_expr->evaluate(0.0);
            sol.exact    = vpam::ExactVal::fromDouble(sol.numeric);
            sol.exact.simplify();
            sol.isExact  = false;
            sol.symbolic = sol_expr;
            result.solutions.push_back(sol);

            result.steps.logNote(
                "Solution: " + std::string(1, var) + " = " + sol_expr->toString(),
                MethodId::General);
            return true;
        }
    }

    // Fallback to Newton
    return solveNewton(f, var, arena, result);
}

// ════════════════════════════════════════════════════════════════════
// solveNewton — HybridNewton fallback
// ════════════════════════════════════════════════════════════════════

bool OmniSolver::solveNewton(SymExpr* f, char var, SymExprArena& arena,
                              OmniResult& result) {
    result.steps.logNote(
        "No symbolic solution found. Switching to numerical approximation "
        "via Newton-Raphson method.",
        MethodId::Newton);

    result.steps.logNote(
        "Using 16 initial seeds distributed across [-100, 100] "
        "to find all real roots.",
        MethodId::Newton);

    NewtonResult nr = HybridNewton::solve(f, var, arena,
                                           &result.steps, 8);

    if (!nr.ok) {
        result.error = nr.error;
        return false;
    }

    for (const auto& root : nr.roots) {
        OmniSolution sol;
        sol.exact   = root.exact;
        sol.numeric = root.value;
        sol.isExact = false;  // Newton roots are approximate
        result.solutions.push_back(sol);
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════
// countVar — Count occurrences of variable in expression tree
// ════════════════════════════════════════════════════════════════════

int OmniSolver::countVar(const SymExpr* expr, char var) {
    if (!expr) return 0;
    switch (expr->type) {
        case SymExprType::Num:
            return 0;
        case SymExprType::Var:
            return (static_cast<const SymVar*>(expr)->name == var) ? 1 : 0;
        case SymExprType::Neg:
            return countVar(static_cast<const SymNeg*>(expr)->child, var);
        case SymExprType::Add: {
            auto* add = static_cast<const SymAdd*>(expr);
            int total = 0;
            for (uint16_t i = 0; i < add->count; ++i)
                total += countVar(add->terms[i], var);
            return total;
        }
        case SymExprType::Mul: {
            auto* mul = static_cast<const SymMul*>(expr);
            int total = 0;
            for (uint16_t i = 0; i < mul->count; ++i)
                total += countVar(mul->factors[i], var);
            return total;
        }
        case SymExprType::Pow: {
            auto* pw = static_cast<const SymPow*>(expr);
            return countVar(pw->base, var) + countVar(pw->exponent, var);
        }
        case SymExprType::Func:
            return countVar(static_cast<const SymFunc*>(expr)->argument, var);
        default:
            return 0;
    }
}

// ════════════════════════════════════════════════════════════════════
// isolateVar — Recursive inverse peeling
//
// Given  expr = rhs  where var appears exactly once in expr,
// peel one layer at a time until we reach Var(var), returning rhs.
//
// Examples:
//   Add(x, 3) = 0     →  x = 0 - 3        →  x = -3
//   Mul(2, x) = 0     →  x = 0 / 2        →  x = 0
//   Pow(x, 3) = 8     →  x = 8^(1/3)      →  x = 2
//   ln(x) = 1         →  x = e^1           →  x = e
//   sin(2x+1) = 0.5   →  2x+1 = arcsin(0.5) →  x = ...
// ════════════════════════════════════════════════════════════════════

SymExpr* OmniSolver::isolateVar(SymExpr* expr, SymExpr* rhs, char var,
                                 SymExprArena& arena, CASStepLogger& log,
                                 int depth) {
    if (!expr || depth > 20) return nullptr;

    // ── Base case: we've reached the variable itself ──
    if (expr->type == SymExprType::Var) {
        auto* v = static_cast<SymVar*>(expr);
        if (v->name == var) return rhs;
        return nullptr;  // wrong variable
    }

    // ── Neg(inner) = rhs  →  inner = -rhs ──
    if (expr->type == SymExprType::Neg) {
        auto* neg = static_cast<SymNeg*>(expr);
        log.logNote("\xE2\x88\x92(f) = rhs  \xE2\x86\x92  f = \xE2\x88\x92(rhs)", MethodId::General);
        SymExpr* newRhs = symNeg(arena, rhs);
        return isolateVar(neg->child, newRhs, var, arena, log, depth + 1);
    }

    // ── Add(terms...) = rhs  →  move non-var terms to rhs ──
    if (expr->type == SymExprType::Add) {
        auto* add = static_cast<SymAdd*>(expr);

        // Find which term contains the variable
        int varIdx = -1;
        for (uint16_t i = 0; i < add->count; ++i) {
            if (countVar(add->terms[i], var) > 0) {
                if (varIdx != -1) return nullptr;  // var in multiple terms
                varIdx = static_cast<int>(i);
            }
        }
        if (varIdx < 0) return nullptr;

        // rhs = rhs - (sum of non-var terms)
        log.logNote("Isolate sum: move constant terms to the other side",
                    MethodId::General);
        SymExpr* newRhs = rhs;
        for (uint16_t i = 0; i < add->count; ++i) {
            if (static_cast<int>(i) != varIdx) {
                newRhs = symAdd(arena, newRhs, symNeg(arena, add->terms[i]));
            }
        }
        newRhs = SymSimplify::simplify(newRhs, arena);
        return isolateVar(add->terms[varIdx], newRhs, var, arena, log, depth + 1);
    }

    // ── Mul(factors...) = rhs  →  divide rhs by non-var factors ──
    if (expr->type == SymExprType::Mul) {
        auto* mul = static_cast<SymMul*>(expr);

        int varIdx = -1;
        for (uint16_t i = 0; i < mul->count; ++i) {
            if (countVar(mul->factors[i], var) > 0) {
                if (varIdx != -1) return nullptr;  // var in multiple factors
                varIdx = static_cast<int>(i);
            }
        }
        if (varIdx < 0) return nullptr;

        // rhs = rhs / (product of non-var factors)
        log.logNote("Isolate product: divide by constant factors",
                    MethodId::General);
        SymExpr* newRhs = rhs;
        for (uint16_t i = 0; i < mul->count; ++i) {
            if (static_cast<int>(i) != varIdx) {
                // rhs / factor = rhs * factor^(-1)
                SymExpr* inv = symPow(arena, mul->factors[i], symInt(arena, -1));
                newRhs = symMul(arena, newRhs, inv);
            }
        }
        newRhs = SymSimplify::simplify(newRhs, arena);
        return isolateVar(mul->factors[varIdx], newRhs, var, arena, log, depth + 1);
    }

    // ── Pow(base, exp) = rhs ──
    if (expr->type == SymExprType::Pow) {
        auto* pw = static_cast<SymPow*>(expr);
        bool baseHas = countVar(pw->base, var) > 0;
        bool expHas  = countVar(pw->exponent, var) > 0;

        if (baseHas && !expHas) {
            // base^exp = rhs  →  base = rhs^(1/exp)
            log.logNote("base\xE2\x81\xBF = rhs  \xE2\x86\x92  base = rhs\xC2\xB9\xE2\x81\x8F\xE2\x81\xBF", MethodId::General);
            SymExpr* invExp = symPow(arena, pw->exponent, symInt(arena, -1));
            SymExpr* newRhs = symPow(arena, rhs, invExp);
            newRhs = SymSimplify::simplify(newRhs, arena);
            return isolateVar(pw->base, newRhs, var, arena, log, depth + 1);
        }

        if (expHas && !baseHas) {
            // a^exp = rhs  →  exp = ln(rhs) / ln(a)
            log.logNote("a\xCB\xA3 = rhs  \xE2\x86\x92  f(x) = ln(rhs)/ln(a)", MethodId::General);
            SymExpr* lnRhs  = symFunc(arena, SymFuncKind::Ln, rhs);
            SymExpr* lnBase = symFunc(arena, SymFuncKind::Ln, pw->base);
            SymExpr* invLnB = symPow(arena, lnBase, symInt(arena, -1));
            SymExpr* newRhs = symMul(arena, lnRhs, invLnB);
            newRhs = SymSimplify::simplify(newRhs, arena);
            return isolateVar(pw->exponent, newRhs, var, arena, log, depth + 1);
        }

        return nullptr;  // var in both base and exponent — can't isolate
    }

    // ── Func(kind, arg) = rhs  →  arg = inverse(kind, rhs) ──
    if (expr->type == SymExprType::Func) {
        auto* fn = static_cast<SymFunc*>(expr);
        SymExpr* inv = applyInverse(fn->kind, rhs, arena);
        if (!inv) {
            log.logNote("Non-invertible function: " +
                        std::string(symFuncName(fn->kind)), MethodId::General);
            return nullptr;
        }
        log.logNote(std::string(symFuncName(fn->kind)) +
                    "(f) = rhs  \xE2\x86\x92  f = " + inv->toString(),
                    MethodId::General);
        SymExpr* newRhs = SymSimplify::simplify(inv, arena);
        return isolateVar(fn->argument, newRhs, var, arena, log, depth + 1);
    }

    return nullptr;  // Unknown node type — bail out
}

// ════════════════════════════════════════════════════════════════════
// solveAnalytic — Analytic isolation entry point
//
// If the variable appears exactly once in f, recursively peel
// the expression tree to isolate var, starting from f(x) = 0.
// ════════════════════════════════════════════════════════════════════

bool OmniSolver::solveAnalytic(SymExpr* f, char var, SymExprArena& arena,
                                OmniResult& result) {
    result.steps.logNote("Attempting analytic isolation", MethodId::General);

    // Start: f(x) = 0  →  isolate var with rhs = 0
    SymExpr* zero = symInt(arena, 0);
    SymExpr* sol  = isolateVar(f, zero, var, arena, result.steps, 0);

    if (!sol) {
        result.steps.logNote("Analytic isolation not achieved",
                             MethodId::General);
        return false;
    }

    // Final simplification pass
    sol = SymSimplify::simplify(sol, arena);

    result.steps.logNote(
        "Analytic solution: " + std::string(1, var) + " = " + sol->toString(),
        MethodId::General);

    // Build OmniSolution
    OmniSolution omniSol;
    omniSol.symbolic = sol;
    omniSol.numeric  = sol->evaluate(0.0);  // Evaluate (var-free expression)

    // Try to extract exact CASRational if result is a pure number
    if (sol->type == SymExprType::Num) {
        auto* num = static_cast<SymNum*>(sol);
        omniSol.exact   = vpam::ExactVal::fromFrac(
            num->_coeff.num().toInt64(), num->_coeff.den().toInt64());
        omniSol.isExact = true;
    } else if (sol->type == SymExprType::Neg &&
               static_cast<SymNeg*>(sol)->child->type == SymExprType::Num) {
        auto* inner = static_cast<SymNum*>(static_cast<SymNeg*>(sol)->child);
        omniSol.exact   = vpam::ExactVal::fromFrac(
            -inner->_coeff.num().toInt64(), inner->_coeff.den().toInt64());
        omniSol.isExact = true;
    } else {
        // Symbolic result (e.g., arcsin(1/2), e^3, ln(5)/ln(2))
        omniSol.isExact = false;
    }

    result.solutions.push_back(omniSol);
    result.ok = true;
    return true;
}

} // namespace cas
