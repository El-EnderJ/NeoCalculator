/**
 * SymIntegrate.cpp — Symbolic indefinite integration engine.
 *
 * Implements integrate() with a strategy cascade:
 *   1. Table lookup  — ∫c dx, ∫x^n dx, ∫sin(x) dx, etc.
 *   2. Linearity     — ∫(af+bg) dx = a∫f + b∫g
 *   3. U-substitution— ∫f(g(x))·g'(x) dx via chain rule detection
 *   4. Parts         — ∫u dv = uv − ∫v du (LIATE heuristic, depth-limited)
 *
 * All results are simplified via SymSimplify before return.
 * Uses Phase 2 cons'd factories throughout.
 *
 * Part of: NumOS Pro-CAS — Phase 6A (Symbolic Integration)
 */

#include "SymIntegrate.h"
#include "SymDiff.h"
#include "SymSimplify.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::integrate(SymExpr* expr, char var,
                                 SymExprArena& arena) {
    if (!expr) return nullptr;

    // Simplify input first for canonical form
    SymExpr* simplified = SymSimplify::simplify(expr, arena);

    SymExpr* result = integrateCore(simplified, var, arena, 0);
    if (!result) return nullptr;

    // Final simplification pass on result
    return SymSimplify::simplify(result, arena);
}

// ════════════════════════════════════════════════════════════════════
// Core recursive integrator — tries strategies in order
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::integrateCore(SymExpr* expr, char var,
                                     SymExprArena& arena, int depth) {
    if (!expr) return nullptr;
    if (depth > MAX_USUB_DEPTH) return nullptr;  // depth safety

    // Strategy 1: Table lookup
    SymExpr* result = tryTable(expr, var, arena);
    if (result) return result;

    // Strategy 2: Linearity (sums and constant factors)
    result = tryLinearity(expr, var, arena, depth);
    if (result) return result;

    // Strategy 3: U-substitution
    result = tryUSubstitution(expr, var, arena, depth);
    if (result) return result;

    // Strategy 4: Integration by parts
    result = tryParts(expr, var, arena, depth);
    if (result) return result;

    // All strategies failed
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// Strategy 1: Table Lookup
//
// Handles: constants, x, x^n, 1/x, sin(x), cos(x), exp(x),
//          tan(x), 1/cos²(x), 1/sin²(x), arcsin(x), arccos(x),
//          arctan(x), ln(x)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::tryTable(SymExpr* expr, char var,
                                SymExprArena& arena) {
    // ── Constant (no variable) → c·x ──
    if (countVar(expr, var) == 0) {
        return symMul(arena, expr, symVar(arena, var));
    }

    // ── Bare variable x → x²/2 ──
    if (expr->type == SymExprType::Var) {
        auto* v = static_cast<SymVar*>(expr);
        if (v->name == var) {
            // ∫x dx = x²/2
            return symMul(arena,
                symFrac(arena, 1, 2),
                symPow(arena, symVar(arena, var), symInt(arena, 2)));
        }
        return nullptr;  // different variable — constant
    }

    // ── Power: x^n ──
    if (expr->type == SymExprType::Pow) {
        auto* pw = static_cast<SymPow*>(expr);

        // Check base == var and exponent is constant
        if (pw->base->type == SymExprType::Var &&
            static_cast<SymVar*>(pw->base)->name == var &&
            countVar(pw->exponent, var) == 0)
        {
            // Check for n == -1 → ln|x|
            if (pw->exponent->type == SymExprType::Num) {
                auto* numExp = static_cast<SymNum*>(pw->exponent);
                if (numExp->isPureRational() &&
                    numExp->_coeff == CASRational::fromInt(-1))
                {
                    // ∫x^(-1) dx = ln|x|
                    return symFunc(arena, SymFuncKind::Ln,
                        symFunc(arena, SymFuncKind::Abs,
                            symVar(arena, var)));
                }
            }

            // General: ∫x^n dx = x^(n+1)/(n+1)
            SymExpr* nPlusOne = symAdd(arena, pw->exponent, symInt(arena, 1));
            nPlusOne = SymSimplify::simplify(nPlusOne, arena);

            // x^(n+1) / (n+1) = x^(n+1) · (n+1)^(-1)
            return symMul(arena,
                symPow(arena, symVar(arena, var), nPlusOne),
                symPow(arena, nPlusOne, symInt(arena, -1)));
        }

        // ── e^x (rendered as Pow with base e) — shouldn't appear here
        //    since we use SymFunc(Exp, x), but handle just in case
    }

    // ── Negation: -(f) → -(∫f dx) ──
    if (expr->type == SymExprType::Neg) {
        // Handled by linearity, but simple case here
        return nullptr;
    }

    // ── Function applications ──
    if (expr->type == SymExprType::Func) {
        auto* f = static_cast<SymFunc*>(expr);

        // Only handle f(x) where argument == the variable
        if (f->argument->type == SymExprType::Var &&
            static_cast<SymVar*>(f->argument)->name == var)
        {
            switch (f->kind) {
                case SymFuncKind::Sin:
                    // ∫sin(x) dx = -cos(x)
                    return symNeg(arena,
                        symFunc(arena, SymFuncKind::Cos,
                            symVar(arena, var)));

                case SymFuncKind::Cos:
                    // ∫cos(x) dx = sin(x)
                    return symFunc(arena, SymFuncKind::Sin,
                        symVar(arena, var));

                case SymFuncKind::Exp:
                    // ∫e^x dx = e^x
                    return symFunc(arena, SymFuncKind::Exp,
                        symVar(arena, var));

                case SymFuncKind::Tan:
                    // ∫tan(x) dx = -ln|cos(x)|
                    return symNeg(arena,
                        symFunc(arena, SymFuncKind::Ln,
                            symFunc(arena, SymFuncKind::Abs,
                                symFunc(arena, SymFuncKind::Cos,
                                    symVar(arena, var)))));

                case SymFuncKind::Ln:
                    // ∫ln(x) dx = x·ln(x) - x
                    return symAdd(arena,
                        symMul(arena,
                            symVar(arena, var),
                            symFunc(arena, SymFuncKind::Ln,
                                symVar(arena, var))),
                        symNeg(arena, symVar(arena, var)));

                case SymFuncKind::ArcSin:
                    // ∫arcsin(x) dx = x·arcsin(x) + √(1-x²)
                    return symAdd(arena,
                        symMul(arena,
                            symVar(arena, var),
                            symFunc(arena, SymFuncKind::ArcSin,
                                symVar(arena, var))),
                        symPow(arena,
                            symAdd(arena,
                                symInt(arena, 1),
                                symNeg(arena,
                                    symPow(arena,
                                        symVar(arena, var),
                                        symInt(arena, 2)))),
                            symFrac(arena, 1, 2)));

                case SymFuncKind::ArcCos:
                    // ∫arccos(x) dx = x·arccos(x) - √(1-x²)
                    return symAdd(arena,
                        symMul(arena,
                            symVar(arena, var),
                            symFunc(arena, SymFuncKind::ArcCos,
                                symVar(arena, var))),
                        symNeg(arena,
                            symPow(arena,
                                symAdd(arena,
                                    symInt(arena, 1),
                                    symNeg(arena,
                                        symPow(arena,
                                            symVar(arena, var),
                                            symInt(arena, 2)))),
                                symFrac(arena, 1, 2))));

                case SymFuncKind::ArcTan:
                    // ∫arctan(x) dx = x·arctan(x) - ½·ln(1+x²)
                    return symAdd(arena,
                        symMul(arena,
                            symVar(arena, var),
                            symFunc(arena, SymFuncKind::ArcTan,
                                symVar(arena, var))),
                        symNeg(arena,
                            symMul(arena,
                                symFrac(arena, 1, 2),
                                symFunc(arena, SymFuncKind::Ln,
                                    symAdd(arena,
                                        symInt(arena, 1),
                                        symPow(arena,
                                            symVar(arena, var),
                                            symInt(arena, 2)))))));

                default:
                    return nullptr;
            }
        }
    }

    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// Strategy 2: Linearity
//
// ∫(a + b + c + ...) dx = ∫a dx + ∫b dx + ...
// ∫(c · f(x)) dx = c · ∫f(x) dx   (c constant)
// ∫(-f) dx = -(∫f dx)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::tryLinearity(SymExpr* expr, char var,
                                    SymExprArena& arena, int depth) {
    // ── Sum: integrate each term ──
    if (expr->type == SymExprType::Add) {
        auto* add = static_cast<SymAdd*>(expr);
        auto** results = static_cast<SymExpr**>(
            arena.allocRaw(add->count * sizeof(SymExpr*)));
        if (!results) return nullptr;

        for (uint16_t i = 0; i < add->count; ++i) {
            results[i] = integrateCore(
                const_cast<SymExpr*>(add->terms[i]), var, arena, depth);
            if (!results[i]) return nullptr;  // one term failed → fail all
        }
        return symAddN(arena, results, add->count);
    }

    // ── Negation: ∫(-f) dx = -(∫f dx) ──
    if (expr->type == SymExprType::Neg) {
        auto* neg = static_cast<SymNeg*>(expr);
        SymExpr* inner = integrateCore(neg->child, var, arena, depth);
        if (!inner) return nullptr;
        return symNeg(arena, inner);
    }

    // ── Product with constant factor: c·f(x) → c · ∫f(x) dx ──
    if (expr->type == SymExprType::Mul) {
        CoeffRest cr = extractConstFactor(expr, var, arena);
        if (cr.coeff && cr.rest) {
            SymExpr* innerInt = integrateCore(cr.rest, var, arena, depth);
            if (innerInt) {
                return symMul(arena, cr.coeff, innerInt);
            }
        }
        // Fall through — not a simple constant factor product
    }

    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// Strategy 3: U-Substitution
//
// Detects patterns: f(g(x)) · g'(x)
// If the integrand is a product where one factor is a composite
// function f(g(x)) and another factor matches g'(x) (after simplify),
// then ∫f(g(x))·g'(x) dx = F(g(x))  where F' = f.
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::tryUSubstitution(SymExpr* expr, char var,
                                        SymExprArena& arena, int depth) {
    if (depth >= MAX_USUB_DEPTH) return nullptr;

    // We need a product with a function composition
    if (expr->type != SymExprType::Mul) return nullptr;

    auto* mul = static_cast<SymMul*>(expr);
    if (mul->count < 2) return nullptr;

    // Look for a SymFunc factor — that's our f(g(x))
    for (uint16_t i = 0; i < mul->count; ++i) {
        SymExpr* factor_i = const_cast<SymExpr*>(mul->factors[i]);

        // Candidate inner function g(x)
        SymExpr* g = nullptr;
        SymFuncKind outerKind;

        if (factor_i->type == SymExprType::Func) {
            auto* func = static_cast<SymFunc*>(factor_i);
            if (countVar(func->argument, var) > 0) {
                g = func->argument;
                outerKind = func->kind;
            }
        }

        if (!g) continue;

        // Compute g'(x)
        SymExpr* gPrime = SymDiff::diff(g, var, arena);
        if (!gPrime) continue;
        gPrime = SymSimplify::simplify(gPrime, arena);

        // Build the "remaining" factors (everything except factor_i)
        SymExpr* remaining = nullptr;
        if (mul->count == 2) {
            remaining = const_cast<SymExpr*>(mul->factors[1 - i]);
        } else {
            auto** remArr = static_cast<SymExpr**>(
                arena.allocRaw((mul->count - 1) * sizeof(SymExpr*)));
            uint16_t ri = 0;
            for (uint16_t j = 0; j < mul->count; ++j) {
                if (j != i)
                    remArr[ri++] = const_cast<SymExpr*>(mul->factors[j]);
            }
            remaining = symMulN(arena, remArr, ri);
        }
        remaining = SymSimplify::simplify(remaining, arena);

        // Check: does remaining == g'(x)?
        if (exprEqual(remaining, gPrime)) {
            // ∫f(g(x))·g'(x) dx = F(g(x)) where F is antiderivative of f
            // Use a temporary variable 'u' to integrate f(u)
            // Then substitute g(x) back for u
            SymExpr* fOfU = symFunc(arena, outerKind,
                symVar(arena, 'u'));

            SymExpr* FofU = integrateCore(fOfU, 'u', arena, depth + 1);
            if (!FofU) continue;

            // Substitute u → g(x) via clone-and-replace
            SymExpr* result = substituteVar(FofU, 'u', g, arena);
            return result;
        }

        // Check: does remaining == c · g'(x) for some constant c?
        //        i.e., remaining / g'(x) is constant
        // Build remaining · g'(x)^(-1) and simplify
        SymExpr* ratio = symMul(arena, remaining,
            symPow(arena, gPrime, symInt(arena, -1)));
        ratio = SymSimplify::simplify(ratio, arena);

        if (countVar(ratio, var) == 0) {
            // remaining = ratio · g'(x), so ∫f(g)·(c·g') dx = c·F(g)
            SymExpr* fOfU = symFunc(arena, outerKind,
                symVar(arena, 'u'));
            SymExpr* FofU = integrateCore(fOfU, 'u', arena, depth + 1);
            if (!FofU) continue;

            SymExpr* result = substituteVar(FofU, 'u', g, arena);
            return symMul(arena, ratio, result);
        }
    }

    // ── Also try u-sub for x^n patterns like ∫2x·(x²+1)^3 dx ──
    // Look for a SymPow factor with a composite base
    for (uint16_t i = 0; i < mul->count; ++i) {
        SymExpr* factor_i = const_cast<SymExpr*>(mul->factors[i]);

        if (factor_i->type != SymExprType::Pow) continue;
        auto* pw = static_cast<SymPow*>(factor_i);

        // Need: base contains var, exponent is constant
        if (countVar(pw->base, var) == 0) continue;
        if (countVar(pw->exponent, var) != 0) continue;

        SymExpr* g = pw->base;
        SymExpr* n = pw->exponent;

        // Compute g'(x)
        SymExpr* gPrime = SymDiff::diff(g, var, arena);
        if (!gPrime) continue;
        gPrime = SymSimplify::simplify(gPrime, arena);

        // Build remaining factors
        SymExpr* remaining = nullptr;
        if (mul->count == 2) {
            remaining = const_cast<SymExpr*>(mul->factors[1 - i]);
        } else {
            auto** remArr = static_cast<SymExpr**>(
                arena.allocRaw((mul->count - 1) * sizeof(SymExpr*)));
            uint16_t ri = 0;
            for (uint16_t j = 0; j < mul->count; ++j) {
                if (j != i)
                    remArr[ri++] = const_cast<SymExpr*>(mul->factors[j]);
            }
            remaining = symMulN(arena, remArr, ri);
        }
        remaining = SymSimplify::simplify(remaining, arena);

        // Check g'(x) match (exact or up to constant)
        SymExpr* ratio = symMul(arena, remaining,
            symPow(arena, gPrime, symInt(arena, -1)));
        ratio = SymSimplify::simplify(ratio, arena);

        if (countVar(ratio, var) == 0) {
            // ∫c · g(x)^n · g'(x) dx = c · g(x)^(n+1)/(n+1)
            // Check n == -1 case
            bool isNegOne = false;
            if (n->type == SymExprType::Num) {
                auto* nNum = static_cast<SymNum*>(n);
                if (nNum->isPureRational() &&
                    nNum->_coeff == CASRational::fromInt(-1)) {
                    isNegOne = true;
                }
            }

            SymExpr* antideriv;
            if (isNegOne) {
                // ∫g^(-1)·g' dx = ln|g|
                antideriv = symFunc(arena, SymFuncKind::Ln,
                    symFunc(arena, SymFuncKind::Abs, g));
            } else {
                SymExpr* nPlusOne = SymSimplify::simplify(
                    symAdd(arena, n, symInt(arena, 1)), arena);
                antideriv = symMul(arena,
                    symPow(arena, g, nPlusOne),
                    symPow(arena, nPlusOne, symInt(arena, -1)));
            }

            return symMul(arena, ratio, antideriv);
        }
    }

    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// Strategy 4: Integration by Parts (LIATE heuristic)
//
// ∫u dv = uv − ∫v du
// Choose u via LIATE: Logarithmic > Inverse trig > Algebraic >
//                     Trigonometric > Exponential
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::tryParts(SymExpr* expr, char var,
                                SymExprArena& arena, int depth) {
    if (depth >= MAX_PARTS_DEPTH) return nullptr;

    // Need a product of at least 2 factors
    if (expr->type != SymExprType::Mul) return nullptr;
    auto* mul = static_cast<SymMul*>(expr);
    if (mul->count < 2) return nullptr;

    // Split into two groups: u candidate and dv candidate
    // Try each factor as u (highest LIATE priority first)
    int bestPri = -1;
    int bestIdx = -1;

    for (uint16_t i = 0; i < mul->count; ++i) {
        int pri = liatePriority(mul->factors[i]);
        if (pri > bestPri) {
            bestPri = pri;
            bestIdx = i;
        }
    }

    if (bestIdx < 0) return nullptr;

    // u = factor with highest LIATE priority
    SymExpr* u = const_cast<SymExpr*>(mul->factors[bestIdx]);

    // dv = product of remaining factors
    SymExpr* dv;
    if (mul->count == 2) {
        dv = const_cast<SymExpr*>(mul->factors[1 - bestIdx]);
    } else {
        auto** dvArr = static_cast<SymExpr**>(
            arena.allocRaw((mul->count - 1) * sizeof(SymExpr*)));
        uint16_t di = 0;
        for (uint16_t j = 0; j < mul->count; ++j) {
            if (j != (uint16_t)bestIdx)
                dvArr[di++] = const_cast<SymExpr*>(mul->factors[j]);
        }
        dv = symMulN(arena, dvArr, di);
    }

    // Compute v = ∫dv dx  (must succeed for parts to work)
    SymExpr* v = integrateCore(dv, var, arena, depth + 1);
    if (!v) return nullptr;
    v = SymSimplify::simplify(v, arena);

    // Compute du = d/dx(u)
    SymExpr* du = SymDiff::diff(u, var, arena);
    if (!du) return nullptr;
    du = SymSimplify::simplify(du, arena);

    // ∫v·du dx  (the remaining integral)
    SymExpr* vdu = SymSimplify::simplify(symMul(arena, v, du), arena);
    SymExpr* intVdu = integrateCore(vdu, var, arena, depth + 1);
    if (!intVdu) return nullptr;

    // Result: u·v − ∫v·du dx
    SymExpr* uv = symMul(arena, u, v);
    return symAdd(arena, uv, symNeg(arena, intVdu));
}

// ════════════════════════════════════════════════════════════════════
// Helper: countVar — count variable occurrences
// ════════════════════════════════════════════════════════════════════

int SymIntegrate::countVar(const SymExpr* expr, char var) {
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
            return countVar(
                static_cast<const SymFunc*>(expr)->argument, var);
        default:
            return 0;
    }
}

// ════════════════════════════════════════════════════════════════════
// Helper: exprEqual — structural equality via pointer identity
// (hash-consing guarantees this after simplification)
// ════════════════════════════════════════════════════════════════════

bool SymIntegrate::exprEqual(const SymExpr* a, const SymExpr* b) {
    return a == b;
}

// ════════════════════════════════════════════════════════════════════
// Helper: extractConstFactor
//
// Given a Mul, separate constant factors from variable-dependent.
// ════════════════════════════════════════════════════════════════════

SymIntegrate::CoeffRest
SymIntegrate::extractConstFactor(SymExpr* expr, char var,
                                 SymExprArena& arena) {
    CoeffRest cr = { nullptr, nullptr };

    if (expr->type != SymExprType::Mul) {
        cr.rest = expr;
        return cr;
    }

    auto* mul = static_cast<SymMul*>(expr);

    // Separate constant and variable-dependent factors
    SymExpr** constArr = static_cast<SymExpr**>(
        arena.allocRaw(mul->count * sizeof(SymExpr*)));
    SymExpr** varArr = static_cast<SymExpr**>(
        arena.allocRaw(mul->count * sizeof(SymExpr*)));
    uint16_t cCount = 0, vCount = 0;

    for (uint16_t i = 0; i < mul->count; ++i) {
        if (countVar(mul->factors[i], var) == 0) {
            constArr[cCount++] = const_cast<SymExpr*>(mul->factors[i]);
        } else {
            varArr[vCount++] = const_cast<SymExpr*>(mul->factors[i]);
        }
    }

    if (cCount == 0 || vCount == 0) {
        // No separation possible — treat entire expression as rest
        cr.rest = expr;
        return cr;
    }

    // Build coefficient and remainder
    cr.coeff = (cCount == 1) ? constArr[0]
                             : symMulN(arena, constArr, cCount);
    cr.rest  = (vCount == 1) ? varArr[0]
                             : symMulN(arena, varArr, vCount);
    return cr;
}

// ════════════════════════════════════════════════════════════════════
// Helper: liatePriority — LIATE heuristic
//
//   L (Logarithmic)       = 50
//   I (Inverse trig)      = 40
//   A (Algebraic/poly)    = 30
//   T (Trigonometric)     = 20
//   E (Exponential)       = 10
// ════════════════════════════════════════════════════════════════════

int SymIntegrate::liatePriority(const SymExpr* expr) {
    if (!expr) return 0;

    switch (expr->type) {
        case SymExprType::Func: {
            auto* f = static_cast<const SymFunc*>(expr);
            switch (f->kind) {
                case SymFuncKind::Ln:
                case SymFuncKind::Log10:
                    return 50;  // L — Logarithmic
                case SymFuncKind::ArcSin:
                case SymFuncKind::ArcCos:
                case SymFuncKind::ArcTan:
                    return 40;  // I — Inverse trig
                case SymFuncKind::Sin:
                case SymFuncKind::Cos:
                case SymFuncKind::Tan:
                    return 20;  // T — Trigonometric
                case SymFuncKind::Exp:
                    return 10;  // E — Exponential
                default:
                    return 0;
            }
        }
        case SymExprType::Var:
        case SymExprType::Pow:
            return 30;  // A — Algebraic
        case SymExprType::Num:
            return 0;   // Constants — never choose as u
        default:
            return 15;  // Default for compound expressions
    }
}

// ════════════════════════════════════════════════════════════════════
// Helper: substituteVar — replace all occurrences of variable with expr
// ════════════════════════════════════════════════════════════════════

SymExpr* SymIntegrate::substituteVar(SymExpr* expr, char oldVar,
                                     SymExpr* replacement,
                                     SymExprArena& arena) {
    if (!expr) return nullptr;

    switch (expr->type) {
        case SymExprType::Num:
            return expr;  // Constants unchanged

        case SymExprType::Var: {
            auto* v = static_cast<SymVar*>(expr);
            return (v->name == oldVar) ? replacement : expr;
        }

        case SymExprType::Neg: {
            auto* neg = static_cast<SymNeg*>(expr);
            SymExpr* inner = substituteVar(neg->child, oldVar,
                                           replacement, arena);
            return symNeg(arena, inner);
        }

        case SymExprType::Add: {
            auto* add = static_cast<SymAdd*>(expr);
            auto** arr = static_cast<SymExpr**>(
                arena.allocRaw(add->count * sizeof(SymExpr*)));
            for (uint16_t i = 0; i < add->count; ++i) {
                arr[i] = substituteVar(
                    const_cast<SymExpr*>(add->terms[i]),
                    oldVar, replacement, arena);
            }
            return symAddN(arena, arr, add->count);
        }

        case SymExprType::Mul: {
            auto* mul = static_cast<SymMul*>(expr);
            auto** arr = static_cast<SymExpr**>(
                arena.allocRaw(mul->count * sizeof(SymExpr*)));
            for (uint16_t i = 0; i < mul->count; ++i) {
                arr[i] = substituteVar(
                    const_cast<SymExpr*>(mul->factors[i]),
                    oldVar, replacement, arena);
            }
            return symMulN(arena, arr, mul->count);
        }

        case SymExprType::Pow: {
            auto* pw = static_cast<SymPow*>(expr);
            SymExpr* b = substituteVar(pw->base, oldVar,
                                       replacement, arena);
            SymExpr* e = substituteVar(pw->exponent, oldVar,
                                       replacement, arena);
            return symPow(arena, b, e);
        }

        case SymExprType::Func: {
            auto* f = static_cast<SymFunc*>(expr);
            SymExpr* arg = substituteVar(f->argument, oldVar,
                                         replacement, arena);
            return symFunc(arena, f->kind, arg);
        }

        default:
            return expr;
    }
}

} // namespace cas
