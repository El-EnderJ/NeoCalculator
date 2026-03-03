/**
 * SymDiff.cpp — Symbolic differentiation of SymExpr trees.
 *
 * Implements all differentiation rules recursively.
 * All output nodes are arena-allocated (zero individual frees).
 *
 * Part of: NumOS Pro-CAS — Phase 3 (Symbolic Differentiation)
 */

#include "SymDiff.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diff(SymExpr* expr, char var, SymExprArena& arena) {
    if (!expr) return nullptr;

    switch (expr->type) {
        case SymExprType::Num:
            return diffNum(static_cast<SymNum*>(expr), var, arena);
        case SymExprType::Var:
            return diffVar(static_cast<SymVar*>(expr), var, arena);
        case SymExprType::Neg:
            return diffNeg(static_cast<SymNeg*>(expr), var, arena);
        case SymExprType::Add:
            return diffAdd(static_cast<SymAdd*>(expr), var, arena);
        case SymExprType::Mul:
            return diffMul(static_cast<SymMul*>(expr), var, arena);
        case SymExprType::Pow:
            return diffPow(static_cast<SymPow*>(expr), var, arena);
        case SymExprType::Func:
            return diffFunc(static_cast<SymFunc*>(expr), var, arena);
        default:
            return nullptr;
    }
}

// ════════════════════════════════════════════════════════════════════
// d/dx(constant) = 0
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffNum(SymNum*, char, SymExprArena& arena) {
    return symInt(arena, 0);
}

// ════════════════════════════════════════════════════════════════════
// d/dx(x) = 1,  d/dx(y) = 0
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffVar(SymVar* v, char var, SymExprArena& arena) {
    return symInt(arena, (v->name == var) ? 1 : 0);
}

// ════════════════════════════════════════════════════════════════════
// d/dx(-u) = -(du)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffNeg(SymNeg* n, char var, SymExprArena& arena) {
    SymExpr* du = diff(n->child, var, arena);
    if (!du) return nullptr;
    return symNeg(arena, du);
}

// ════════════════════════════════════════════════════════════════════
// d/dx(a + b + c + ...) = da + db + dc + ...
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffAdd(SymAdd* a, char var, SymExprArena& arena) {
    auto** arr = static_cast<SymExpr**>(
        arena.allocRaw(a->count * sizeof(SymExpr*)));
    if (!arr) return nullptr;

    for (uint16_t i = 0; i < a->count; ++i) {
        arr[i] = diff(const_cast<SymExpr*>(a->terms[i]), var, arena);
        if (!arr[i]) return nullptr;
    }
    return symAddN(arena, arr, a->count);
}

// ════════════════════════════════════════════════════════════════════
// d/dx(u · v) = u·dv + du·v   (generalized product rule)
//
// For n-ary: d/dx(f₁·f₂·...·fₙ) = Σᵢ (f₁·...·fᵢ'·...·fₙ)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffMul(SymMul* m, char var, SymExprArena& arena) {
    uint16_t n = m->count;

    // Build n sum-terms: for each i, product of all factors
    // except factor[i] is replaced by its derivative
    auto** sumTerms = static_cast<SymExpr**>(
        arena.allocRaw(n * sizeof(SymExpr*)));
    if (!sumTerms) return nullptr;

    for (uint16_t i = 0; i < n; ++i) {
        // Build product: factors[0]·...·diff(factors[i])·...·factors[n-1]
        auto** prodArr = static_cast<SymExpr**>(
            arena.allocRaw(n * sizeof(SymExpr*)));
        if (!prodArr) return nullptr;

        for (uint16_t j = 0; j < n; ++j) {
            if (j == i) {
                prodArr[j] = diff(const_cast<SymExpr*>(m->factors[j]), var, arena);
                if (!prodArr[j]) return nullptr;
            } else {
                prodArr[j] = const_cast<SymExpr*>(m->factors[j]);
            }
        }
        sumTerms[i] = symMulN(arena, prodArr, n);
    }

    return symAddN(arena, sumTerms, n);
}

// ════════════════════════════════════════════════════════════════════
// d/dx(u^v)
//
// Case 1: v is constant w.r.t. var → n·u^(n-1)·du  (power rule)
// Case 2: u is constant w.r.t. var → u^v·ln(u)·dv  (exponential)
// Case 3: general → u^v · (dv·ln(u) + v·du/u)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffPow(SymPow* p, char var, SymExprArena& arena) {
    bool baseHasVar = p->base->containsVar(var);
    bool expHasVar  = p->exponent->containsVar(var);

    if (!baseHasVar && !expHasVar) {
        // Constant^Constant → derivative is 0
        return symInt(arena, 0);
    }

    if (!expHasVar) {
        // ── Power rule: d/dx(u^n) = n · u^(n-1) · du ──
        SymExpr* du = diff(p->base, var, arena);
        if (!du) return nullptr;

        // n-1
        SymExpr* nMinus1 = symAdd(arena,
            p->exponent,
            symInt(arena, -1));

        // n · u^(n-1) · du
        return symMul3(arena,
            p->exponent,
            symPow(arena, p->base, nMinus1),
            du);
    }

    if (!baseHasVar) {
        // ── Exponential rule: d/dx(a^v) = a^v · ln(a) · dv ──
        SymExpr* dv = diff(p->exponent, var, arena);
        if (!dv) return nullptr;

        return symMul3(arena,
            p,  // a^v (reuse existing node)
            symFunc(arena, SymFuncKind::Ln, p->base),
            dv);
    }

    // ── General case: d/dx(u^v) = u^v · (dv·ln(u) + v·du/u) ──
    SymExpr* du = diff(p->base, var, arena);
    SymExpr* dv = diff(p->exponent, var, arena);
    if (!du || !dv) return nullptr;

    // dv · ln(u)
    SymExpr* term1 = symMul(arena,
        dv,
        symFunc(arena, SymFuncKind::Ln, p->base));

    // v · du / u  =  v · du · u^(-1)
    SymExpr* term2 = symMul3(arena,
        p->exponent,
        du,
        symPow(arena, p->base, symInt(arena, -1)));

    // u^v · (term1 + term2)
    return symMul(arena,
        p,
        symAdd(arena, term1, term2));
}

// ════════════════════════════════════════════════════════════════════
// d/dx(f(u)) = f'(u) · du   (chain rule)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymDiff::diffFunc(SymFunc* f, char var, SymExprArena& arena) {
    SymExpr* du = diff(f->argument, var, arena);
    if (!du) return nullptr;

    SymExpr* u = f->argument;
    SymExpr* innerDeriv = nullptr;

    switch (f->kind) {
        // ── d/dx(sin(u)) = cos(u) ──
        case SymFuncKind::Sin:
            innerDeriv = symFunc(arena, SymFuncKind::Cos, u);
            break;

        // ── d/dx(cos(u)) = -sin(u) ──
        case SymFuncKind::Cos:
            innerDeriv = symNeg(arena,
                symFunc(arena, SymFuncKind::Sin, u));
            break;

        // ── d/dx(tan(u)) = 1 / cos²(u) = cos(u)^(-2) ──
        case SymFuncKind::Tan:
            innerDeriv = symPow(arena,
                symFunc(arena, SymFuncKind::Cos, u),
                symInt(arena, -2));
            break;

        // ── d/dx(ln(u)) = 1/u = u^(-1) ──
        case SymFuncKind::Ln:
            innerDeriv = symPow(arena, u, symInt(arena, -1));
            break;

        // ── d/dx(exp(u)) = exp(u) ──
        case SymFuncKind::Exp:
            innerDeriv = symFunc(arena, SymFuncKind::Exp, u);
            break;

        // ── d/dx(log10(u)) = 1 / (u · ln(10)) ──
        case SymFuncKind::Log10: {
            // 1 / (u · ln(10))
            SymExpr* ln10 = symFunc(arena, SymFuncKind::Ln,
                symInt(arena, 10));
            innerDeriv = symPow(arena,
                symMul(arena, u, ln10),
                symInt(arena, -1));
            break;
        }

        // ── d/dx(arcsin(u)) = 1 / √(1 - u²) ──
        case SymFuncKind::ArcSin: {
            // (1 - u²)^(-1/2)
            SymExpr* oneMinusU2 = symAdd(arena,
                symInt(arena, 1),
                symNeg(arena, symPow(arena, u, symInt(arena, 2))));
            innerDeriv = symPow(arena, oneMinusU2, symFrac(arena, -1, 2));
            break;
        }

        // ── d/dx(arccos(u)) = -1 / √(1 - u²) ──
        case SymFuncKind::ArcCos: {
            SymExpr* oneMinusU2 = symAdd(arena,
                symInt(arena, 1),
                symNeg(arena, symPow(arena, u, symInt(arena, 2))));
            innerDeriv = symNeg(arena,
                symPow(arena, oneMinusU2, symFrac(arena, -1, 2)));
            break;
        }

        // ── d/dx(arctan(u)) = 1 / (1 + u²) ──
        case SymFuncKind::ArcTan: {
            SymExpr* onePlusU2 = symAdd(arena,
                symInt(arena, 1),
                symPow(arena, u, symInt(arena, 2)));
            innerDeriv = symPow(arena, onePlusU2, symInt(arena, -1));
            break;
        }

        // ── |u| — not differentiable in general, return sign(u)·du ──
        case SymFuncKind::Abs:
        default:
            return nullptr;  // Not supported
    }

    if (!innerDeriv) return nullptr;

    // Chain rule: f'(u) · du
    return symMul(arena, innerDeriv, du);
}

} // namespace cas
