/**
 * SymPolyMulti.cpp — Univariate polynomial view + Sylvester resultant.
 *
 * Implements:
 *   · collectAsUniPoly()     — extract polynomial coefficients from SymExpr
 *   · substituteVar()        — variable replacement in SymExpr tree
 *   · sylvesterResultant()   — Sylvester matrix determinant (Bareiss)
 *
 * All construction uses immutable cons factories (symAdd, symMul, etc.).
 *
 * Part of: NumOS Pro-CAS — Phase 5 (Multivariable & Resultant Solver)
 */

#include "SymPolyMulti.h"
#include "SymSimplify.h"
#include <algorithm>
#include <cmath>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Internal: addToCoeffMap — accumulate coeff·var^deg into a map
// ════════════════════════════════════════════════════════════════════

/// Representation of a single monomial term: coeff * var^degree
struct MonoTerm {
    SymExpr* coeff;
    int      degree;
};

/// Extract monomial terms from a product node
/// Returns false if the expression is not polynomial in `var`.
static bool extractMonomial(SymExpr* expr, char var, SymExprArena& arena,
                            MonoTerm& out) {
    if (!expr) { out = {nullptr, 0}; return false; }

    switch (expr->type) {
        case SymExprType::Num:
            out = {expr, 0};
            return true;

        case SymExprType::Var: {
            auto* v = static_cast<SymVar*>(expr);
            if (v->name == var) {
                out = {symInt(arena, 1), 1};
            } else {
                out = {expr, 0};
            }
            return true;
        }

        case SymExprType::Neg: {
            auto* neg = static_cast<SymNeg*>(expr);
            MonoTerm inner;
            if (!extractMonomial(neg->child, var, arena, inner))
                return false;
            out = {symNeg(arena, inner.coeff), inner.degree};
            return true;
        }

        case SymExprType::Pow: {
            auto* pw = static_cast<SymPow*>(expr);
            // var^n where n is a positive integer constant
            if (pw->base->type == SymExprType::Var &&
                static_cast<SymVar*>(pw->base)->name == var &&
                pw->exponent->type == SymExprType::Num)
            {
                auto* numExp = static_cast<SymNum*>(pw->exponent);
                if (numExp->_coeff.den() == CASInt(1)) {
                    int64_t e = numExp->_coeff.num().toInt64();
                    if (e >= 0 && e <= 20) {
                        out = {symInt(arena, 1), static_cast<int>(e)};
                        return true;
                    }
                }
            }
            // constant^constant or otherVar^n — treat as degree-0 coeff
            if (!pw->base->containsVar(var) && !pw->exponent->containsVar(var)) {
                out = {expr, 0};
                return true;
            }
            // Can't handle var in exponent or complex base
            return false;
        }

        case SymExprType::Mul: {
            auto* mul = static_cast<SymMul*>(expr);
            // Product of factors: multiply coefficients, sum degrees
            SymExpr* coeffProduct = symInt(arena, 1);
            int totalDeg = 0;

            for (uint16_t i = 0; i < mul->count; ++i) {
                MonoTerm ft;
                if (!extractMonomial(mul->factors[i], var, arena, ft))
                    return false;
                coeffProduct = symMul(arena, coeffProduct, ft.coeff);
                totalDeg += ft.degree;
            }
            out = {coeffProduct, totalDeg};
            return true;
        }

        case SymExprType::Func: {
            // Functions containing var are non-polynomial
            if (expr->containsVar(var)) return false;
            out = {expr, 0};
            return true;
        }

        default:
            return false;
    }
}

// ════════════════════════════════════════════════════════════════════
// collectAsUniPoly — public API
// ════════════════════════════════════════════════════════════════════

UniPoly collectAsUniPoly(SymExpr* expr, char var, SymExprArena& arena) {
    UniPoly result;
    result.var = var;

    if (!expr) return result;

    // Simplify first for canonical form
    expr = SymSimplify::simplify(expr, arena);

    // Collect all additive terms
    std::vector<MonoTerm> terms;

    if (expr->type == SymExprType::Add) {
        auto* add = static_cast<SymAdd*>(expr);
        for (uint16_t i = 0; i < add->count; ++i) {
            MonoTerm t;
            if (!extractMonomial(add->terms[i], var, arena, t)) {
                return UniPoly{};  // Not polynomial
            }
            terms.push_back(t);
        }
    } else {
        MonoTerm t;
        if (!extractMonomial(expr, var, arena, t)) {
            return UniPoly{};
        }
        terms.push_back(t);
    }

    // Find max degree
    int maxDeg = 0;
    for (auto& t : terms) {
        if (t.degree > maxDeg) maxDeg = t.degree;
    }

    if (maxDeg > 20) return UniPoly{};  // Safety cap

    // Accumulate coefficients by degree
    result.coeffs.resize(maxDeg + 1, nullptr);
    for (auto& t : terms) {
        SymExpr* c = SymSimplify::simplify(t.coeff, arena);
        if (!result.coeffs[t.degree]) {
            result.coeffs[t.degree] = c;
        } else {
            result.coeffs[t.degree] = symAdd(arena, result.coeffs[t.degree], c);
            result.coeffs[t.degree] = SymSimplify::simplify(
                result.coeffs[t.degree], arena);
        }
    }

    // Fill null slots with zero
    SymExpr* zero = symInt(arena, 0);
    for (auto& c : result.coeffs) {
        if (!c) c = zero;
    }

    // Trim trailing zeros (but keep at least one)
    while (result.coeffs.size() > 1) {
        SymExpr* lc = result.coeffs.back();
        if (lc->type == SymExprType::Num) {
            auto* num = static_cast<SymNum*>(lc);
            if (num->_coeff.isZero()) {
                result.coeffs.pop_back();
                continue;
            }
        }
        break;
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════
// substituteVar — Replace variable with expression
// ════════════════════════════════════════════════════════════════════

SymExpr* substituteVar(SymExpr* expr, char var, SymExpr* replacement,
                       SymExprArena& arena) {
    if (!expr) return nullptr;

    switch (expr->type) {
        case SymExprType::Num:
            return expr;  // No variables to replace

        case SymExprType::Var: {
            auto* v = static_cast<SymVar*>(expr);
            return (v->name == var) ? replacement : expr;
        }

        case SymExprType::Neg: {
            auto* neg = static_cast<SymNeg*>(expr);
            SymExpr* newChild = substituteVar(neg->child, var, replacement, arena);
            if (newChild == neg->child) return expr;  // No change
            return symNeg(arena, newChild);
        }

        case SymExprType::Add: {
            auto* add = static_cast<SymAdd*>(expr);
            bool changed = false;
            auto** arr = static_cast<SymExpr**>(
                arena.allocRaw(add->count * sizeof(SymExpr*)));
            for (uint16_t i = 0; i < add->count; ++i) {
                arr[i] = substituteVar(add->terms[i], var, replacement, arena);
                if (arr[i] != add->terms[i]) changed = true;
            }
            if (!changed) return expr;
            return symAddN(arena, arr, add->count);
        }

        case SymExprType::Mul: {
            auto* mul = static_cast<SymMul*>(expr);
            bool changed = false;
            auto** arr = static_cast<SymExpr**>(
                arena.allocRaw(mul->count * sizeof(SymExpr*)));
            for (uint16_t i = 0; i < mul->count; ++i) {
                arr[i] = substituteVar(mul->factors[i], var, replacement, arena);
                if (arr[i] != mul->factors[i]) changed = true;
            }
            if (!changed) return expr;
            return symMulN(arena, arr, mul->count);
        }

        case SymExprType::Pow: {
            auto* pw = static_cast<SymPow*>(expr);
            SymExpr* newBase = substituteVar(pw->base, var, replacement, arena);
            SymExpr* newExp  = substituteVar(pw->exponent, var, replacement, arena);
            if (newBase == pw->base && newExp == pw->exponent) return expr;
            return symPow(arena, newBase, newExp);
        }

        case SymExprType::Func: {
            auto* fn = static_cast<SymFunc*>(expr);
            SymExpr* newArg = substituteVar(fn->argument, var, replacement, arena);
            if (newArg == fn->argument) return expr;
            return symFunc(arena, fn->kind, newArg);
        }

        default:
            return expr;
    }
}

// ════════════════════════════════════════════════════════════════════
// Sylvester Resultant — Bareiss fraction-free determinant
//
// Given P(v) of degree m and Q(v) of degree n, the Sylvester matrix
// is (m+n) × (m+n).  The determinant of this matrix is the resultant,
// a polynomial in the remaining variables with v eliminated.
//
// Bareiss algorithm:  fraction-free Gaussian elimination where each
// pivot division is exact (guaranteed by the algebraic structure).
// We use symMul/symAdd/symNeg and simplify at each step.
// ════════════════════════════════════════════════════════════════════

/// Helper: access element of a row-major 2D matrix stored in a flat vector
static inline SymExpr*& matAt(std::vector<SymExpr*>& M, int cols, int r, int c) {
    return M[r * cols + c];
}

SymExpr* sylvesterResultant(const UniPoly& P, const UniPoly& Q,
                            SymExprArena& arena) {
    int m = P.degree();
    int n = Q.degree();
    if (m < 0 || n < 0) return nullptr;
    if (m == 0 && n == 0) {
        // Both are constants — resultant is 1 (or zero if both zero)
        return symInt(arena, 1);
    }

    int N = m + n;  // Matrix dimension
    if (N > 12) return nullptr;  // Safety: cap at degree 6+6

    SymExpr* zero = symInt(arena, 0);

    // ── Build Sylvester matrix ──────────────────────────────────────
    // Layout (N×N):
    //   Rows 0..n-1:  coefficients of P, shifted right by row index
    //   Rows n..N-1:  coefficients of Q, shifted right by row index
    //
    // Row i (i < n): columns j=0..N-1
    //   M[i][j] = P.coeffs[m - (j - i)]  if 0 <= m-(j-i) <= m, else 0
    //
    // Convention: P.coeffs[k] is coefficient of v^k.
    // Sylvester matrix rows use coefficients in descending order:
    //   [a_m, a_{m-1}, ..., a_0, 0, ..., 0]  shifted by row index.

    std::vector<SymExpr*> M(N * N, zero);

    // n rows for P (shifted)
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= m; ++j) {
            int col = i + j;
            if (col < N) {
                // P coefficient in descending order: coeffs[m-j]
                matAt(M, N, i, col) = P.coeffs[m - j] ? P.coeffs[m - j] : zero;
            }
        }
    }

    // m rows for Q (shifted)
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j <= n; ++j) {
            int col = i + j;
            if (col < N) {
                matAt(M, N, n + i, col) = Q.coeffs[n - j] ? Q.coeffs[n - j] : zero;
            }
        }
    }

    // ── Bareiss determinant ─────────────────────────────────────────
    // Fraction-free forward elimination:
    //   M[i][j] = (M[i][j] * M[k][k] - M[i][k] * M[k][j]) / prev_pivot
    //
    // prev_pivot starts as 1 (for k=0).
    // Division is exact by Bareiss' theorem when entries are polynomials.
    //
    // We use symMul/symAdd/symNeg + simplify. "Division" by prev_pivot
    // is done via symMul(expr, symPow(prev_pivot, -1)) + simplify,
    // relying on the simplifier to cancel exactly.

    SymExpr* prevPivot = symInt(arena, 1);

    for (int k = 0; k < N; ++k) {
        // Pivot search: find a row >= k with non-zero M[row][k]
        int pivotRow = -1;
        for (int r = k; r < N; ++r) {
            SymExpr* entry = matAt(M, N, r, k);
            // Check if entry is structurally zero
            if (entry->type == SymExprType::Num) {
                auto* num = static_cast<SymNum*>(entry);
                if (num->_coeff.isZero()) continue;
            }
            pivotRow = r;
            break;
        }

        if (pivotRow < 0) {
            // Singular matrix → resultant is 0
            return zero;
        }

        // Swap rows if needed
        if (pivotRow != k) {
            for (int j = 0; j < N; ++j) {
                std::swap(matAt(M, N, k, j), matAt(M, N, pivotRow, j));
            }
        }

        SymExpr* pivot = matAt(M, N, k, k);

        // Eliminate below pivot
        for (int i = k + 1; i < N; ++i) {
            SymExpr* mik = matAt(M, N, i, k);

            for (int j = k + 1; j < N; ++j) {
                // newVal = (pivot * M[i][j] - mik * M[k][j]) / prevPivot
                SymExpr* term1 = symMul(arena, pivot, matAt(M, N, i, j));
                SymExpr* term2 = symMul(arena, mik, matAt(M, N, k, j));
                SymExpr* numer = symAdd(arena, term1, symNeg(arena, term2));

                // Exact division by prevPivot
                SymExpr* invPrev = symPow(arena, prevPivot, symInt(arena, -1));
                SymExpr* val = symMul(arena, numer, invPrev);
                val = SymSimplify::simplify(val, arena);

                matAt(M, N, i, j) = val;
            }

            // Column k in row i becomes zero (don't need to store)
            matAt(M, N, i, k) = zero;
        }

        prevPivot = pivot;
    }

    // Determinant is M[N-1][N-1] (last diagonal element after Bareiss)
    SymExpr* det = matAt(M, N, N - 1, N - 1);
    return SymSimplify::simplify(det, arena);
}

} // namespace cas
