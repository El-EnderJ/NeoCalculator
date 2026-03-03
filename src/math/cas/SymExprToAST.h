/**
 * SymExprToAST.h — Converts SymExpr trees back to MathAST for rendering.
 *
 * This is the Pro-CAS counterpart of SymToAST (which handles SymPoly).
 * It takes a SymExpr tree (output of SymDiff or SymSimplify) and builds
 * a vpam::MathNode tree suitable for rendering via MathCanvas.
 *
 * Mapping:
 *   SymNum   → NodeNumber / NodeFraction / NodeConstant / NodeRoot
 *   SymVar   → NodeVariable
 *   SymNeg   → "−" prefix (or negative sign on number)
 *   SymAdd   → NodeRow with + / − operators between terms
 *   SymMul   → NodeRow with implicit multiplication (juxtaposition)
 *   SymPow   → NodePower
 *   SymFunc  → NodeFunction / NodeLogBase
 *
 * Memory: Uses std::unique_ptr (vpam::NodePtr) — standard heap, not arena.
 *
 * Part of: NumOS Pro-CAS — Phase 3 (SymExpr → MathAST bridge)
 */

#pragma once

#include "SymExpr.h"
#include "../MathAST.h"

namespace cas {

class SymExprToAST {
public:
    /// Convert a SymExpr tree to a MathAST NodePtr.
    /// Returns a NodeRow or single node depending on expression structure.
    static vpam::NodePtr convert(const SymExpr* expr);

    /// Convert an antiderivative SymExpr and append " + C".
    /// Used for displaying integration results.
    static vpam::NodePtr convertIntegral(const SymExpr* antiderivative);

private:
    static vpam::NodePtr convertNum(const SymNum* n);
    static vpam::NodePtr convertVar(const SymVar* v);
    static vpam::NodePtr convertNeg(const SymNeg* n);
    static vpam::NodePtr convertAdd(const SymAdd* a);
    static vpam::NodePtr convertMul(const SymMul* m);
    static vpam::NodePtr convertPow(const SymPow* p);
    static vpam::NodePtr convertFunc(const SymFunc* f);

    /// Helper: wrap a NodePtr in a NodeRow if it isn't already one.
    static vpam::NodePtr ensureRow(vpam::NodePtr node);

    /// Helper: render an ExactVal as AST (reuses SymToAST pattern).
    static vpam::NodePtr renderExactVal(const vpam::ExactVal& val);
};

} // namespace cas
