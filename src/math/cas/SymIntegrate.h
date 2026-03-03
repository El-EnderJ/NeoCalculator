/**
 * SymIntegrate.h — Symbolic indefinite integration engine for Pro-CAS.
 *
 * Provides symbolic integration of SymExpr trees with respect to a
 * single variable.  All output nodes are arena-allocated and cons'd.
 *
 * Integration strategies (tried in order):
 *   1. Table lookup: constants, powers, elementary functions
 *   2. Linearity:    ∫(af + bg) dx = a∫f dx + b∫g dx
 *   3. U-substitution: detect f(g(x))·g'(x) patterns
 *   4. Integration by parts: ∫u dv = uv − ∫v du (LIATE heuristic)
 *   5. Fallback: return unevaluated SymFunc(Integral, expr)
 *
 * All results are passed through SymSimplify::simplify() before return.
 *
 * Memory: ALL output nodes live in the provided SymExprArena.
 *
 * Part of: NumOS Pro-CAS — Phase 6A (Symbolic Integration)
 */

#pragma once

#include "SymExpr.h"
#include "SymExprArena.h"

namespace cas {

class SymIntegrate {
public:
    /// Maximum depth for u-substitution search.
    static constexpr int MAX_USUB_DEPTH = 5;

    /// Maximum depth for integration by parts recursion.
    static constexpr int MAX_PARTS_DEPTH = 3;

    /// Integrate `expr` with respect to variable `var`.
    /// Returns a simplified antiderivative (without + C; caller adds that).
    /// Returns nullptr if no closed-form antiderivative is found —
    /// caller should wrap in SymFunc(Integral, expr) for unevaluated display.
    static SymExpr* integrate(SymExpr* expr, char var, SymExprArena& arena);

private:
    // ── Core recursive integrator (depth-limited) ───────────────────
    static SymExpr* integrateCore(SymExpr* expr, char var,
                                  SymExprArena& arena, int depth);

    // ── Strategy 1: Table lookup (constants, powers, elementary) ────
    static SymExpr* tryTable(SymExpr* expr, char var, SymExprArena& arena);

    // ── Strategy 2: Linearity (split sums, factor constants) ────────
    static SymExpr* tryLinearity(SymExpr* expr, char var,
                                 SymExprArena& arena, int depth);

    // ── Strategy 3: U-substitution ──────────────────────────────────
    static SymExpr* tryUSubstitution(SymExpr* expr, char var,
                                     SymExprArena& arena, int depth);

    // ── Strategy 4: Integration by parts (LIATE heuristic) ──────────
    static SymExpr* tryParts(SymExpr* expr, char var,
                             SymExprArena& arena, int depth);

    // ── Helpers ─────────────────────────────────────────────────────
    /// Count occurrences of variable in expression tree.
    static int countVar(const SymExpr* expr, char var);

    /// Check if two expressions are structurally equal (pointer identity
    /// after simplification — guaranteed by hash-consing).
    static bool exprEqual(const SymExpr* a, const SymExpr* b);

    /// Extract constant coefficient and variable part from a product.
    /// Returns {coeff, rest} where expr == coeff * rest.
    struct CoeffRest {
        SymExpr* coeff;  // constant part (nullptr → 1)
        SymExpr* rest;   // variable-dependent part
    };
    static CoeffRest extractConstFactor(SymExpr* expr, char var,
                                        SymExprArena& arena);

    /// LIATE priority for integration by parts: higher = choose as u.
    static int liatePriority(const SymExpr* expr);

    /// Substitute all occurrences of oldVar with replacement expression.
    static SymExpr* substituteVar(SymExpr* expr, char oldVar,
                                  SymExpr* replacement,
                                  SymExprArena& arena);
};

} // namespace cas
