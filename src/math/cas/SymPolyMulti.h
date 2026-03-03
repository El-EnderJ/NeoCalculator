/**
 * SymPolyMulti.h — Univariate polynomial view over SymExpr trees.
 *
 * Treats a SymExpr as a polynomial in ONE variable, where coefficients
 * are themselves SymExpr trees (containing the other variable(s)).
 *
 *   Example: x² + x·y + y²  viewed as polynomial in x:
 *     degree=2, coeffs[0]=y², coeffs[1]=y, coeffs[2]=1
 *
 * Also provides:
 *   · substituteVar() — replace a variable by a SymExpr in a tree
 *   · Sylvester resultant computation for 2-equation elimination
 *
 * All nodes are immutable and cons'd through the arena factories.
 *
 * Part of: NumOS Pro-CAS — Phase 5 (Multivariable & Resultant Solver)
 */

#pragma once

#include "SymExpr.h"
#include "SymExprArena.h"
#include "SymSimplify.h"
#include <vector>
#include <cstdint>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// UniPoly — Univariate polynomial with SymExpr* coefficients
//
// coeffs[i] is the coefficient of var^i.
// coeffs[degree] is the leading coefficient (non-null).
// All coeffs are arena-allocated SymExpr trees.
// ════════════════════════════════════════════════════════════════════

struct UniPoly {
    std::vector<SymExpr*> coeffs;  // coeffs[i] = coefficient of var^i
    char                  var;     // The variable this is a polynomial in

    /// Degree of the polynomial (-1 if zero polynomial)
    int degree() const {
        return static_cast<int>(coeffs.size()) - 1;
    }

    /// Is this the zero polynomial?
    bool isZero() const { return coeffs.empty(); }
};

// ════════════════════════════════════════════════════════════════════
// collectAsUniPoly — Extract polynomial structure from SymExpr
//
// Given f(x,y) and var='x', return UniPoly where each coefficient
// is a SymExpr in terms of the remaining variables.
//
// Strategy: walk the expression tree and collect terms by x-degree.
// Handles: SymNum, SymVar, SymNeg, SymAdd, SymMul, SymPow (integer exp).
// Returns empty UniPoly on failure (non-polynomial in var).
// ════════════════════════════════════════════════════════════════════

UniPoly collectAsUniPoly(SymExpr* expr, char var, SymExprArena& arena);

// ════════════════════════════════════════════════════════════════════
// substituteVar — Replace every occurrence of `var` with `replacement`
//
// Returns a new immutable SymExpr tree (cons'd).
// Pure-functional: input tree is never mutated.
// ════════════════════════════════════════════════════════════════════

SymExpr* substituteVar(SymExpr* expr, char var, SymExpr* replacement,
                       SymExprArena& arena);

// ════════════════════════════════════════════════════════════════════
// sylvesterResultant — Compute the resultant of two univariate polys
//
// Given P(v) and Q(v) as UniPoly, builds the Sylvester matrix and
// computes its determinant. The result is a SymExpr tree containing
// only the OTHER variables (v has been eliminated).
//
// Uses Bareiss fraction-free determinant to avoid coefficient explosion.
//
// Returns nullptr on failure (e.g., both polynomials are zero).
// ════════════════════════════════════════════════════════════════════

SymExpr* sylvesterResultant(const UniPoly& P, const UniPoly& Q,
                            SymExprArena& arena);

} // namespace cas
