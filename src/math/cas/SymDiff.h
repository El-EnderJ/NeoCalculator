/**
 * SymDiff.h — Symbolic differentiation engine for Pro-CAS.
 *
 * Provides recursive differentiation of SymExpr trees with respect
 * to a single variable. All output nodes are arena-allocated.
 *
 * Differentiation rules implemented:
 *   d/dx(c)          = 0              (constant)
 *   d/dx(x)          = 1              (variable)
 *   d/dx(-u)         = -(du)          (negation)
 *   d/dx(u + v + ..) = du + dv + ..   (sum rule)
 *   d/dx(u · v)      = u·dv + du·v    (product rule, generalised)
 *   d/dx(u^n)        = n·u^(n-1)·du   (power rule, constant exp)
 *   d/dx(u^v)        = u^v·(v'·ln(u) + v·u'/u)  (general power)
 *   d/dx(sin(u))     = cos(u)·du      (chain rule)
 *   d/dx(cos(u))     = -sin(u)·du
 *   d/dx(tan(u))     = (1/cos²(u))·du
 *   d/dx(ln(u))      = du/u
 *   d/dx(exp(u))     = exp(u)·du
 *   d/dx(log10(u))   = du/(u·ln(10))
 *   d/dx(arcsin(u))  = du/√(1-u²)
 *   d/dx(arccos(u))  = -du/√(1-u²)
 *   d/dx(arctan(u))  = du/(1+u²)
 *
 * Memory: ALL output nodes are allocated in the provided SymExprArena.
 *
 * Part of: NumOS Pro-CAS — Phase 3 (Symbolic Differentiation)
 */

#pragma once

#include "SymExpr.h"
#include "SymExprArena.h"

namespace cas {

class SymDiff {
public:
    /// Differentiate `expr` with respect to variable `var`.
    /// Returns an arena-allocated SymExpr tree representing the derivative.
    /// Returns nullptr on error (unsupported node type, etc.).
    static SymExpr* diff(SymExpr* expr, char var, SymExprArena& arena);

private:
    // Recursive handler per node type
    static SymExpr* diffNum(SymNum* n, char var, SymExprArena& arena);
    static SymExpr* diffVar(SymVar* v, char var, SymExprArena& arena);
    static SymExpr* diffNeg(SymNeg* n, char var, SymExprArena& arena);
    static SymExpr* diffAdd(SymAdd* a, char var, SymExprArena& arena);
    static SymExpr* diffMul(SymMul* m, char var, SymExprArena& arena);
    static SymExpr* diffPow(SymPow* p, char var, SymExprArena& arena);
    static SymExpr* diffFunc(SymFunc* f, char var, SymExprArena& arena);
};

} // namespace cas
