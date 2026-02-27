/**
 * SymToAST.h — Converts CAS-Lite symbolic types back to MathAST for rendering.
 *
 * This is the reverse bridge of ASTFlattener: it takes SymPoly, SymEquation,
 * and ExactVal objects and builds MathAST trees suitable for rendering via
 * MathCanvas in "Natural Display" (fractions, powers, radicals).
 *
 * Used by EquationsApp to render:
 *   · Solution values (e.g., x = 3/2 as a fraction node)
 *   · Step-by-step equation snapshots
 *
 * Part of: NumOS CAS-Lite — Phase E (EquationsApp UI)
 */

#pragma once

#include "../MathAST.h"
#include "SymPoly.h"
#include "SymEquation.h"
#include "../ExactVal.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymToAST — Symbolic → MathAST converter
// ════════════════════════════════════════════════════════════════════

class SymToAST {
public:
    /// Convert an ExactVal to a MathAST node tree.
    /// Integers → NodeNumber
    /// Fractions → NodeFraction with NodeNumber children
    /// Radicals → radical nodes (outer√inner)
    static vpam::NodePtr fromExactVal(const vpam::ExactVal& val);

    /// Convert a SymTerm to a MathAST node tree.
    /// e.g., 3x² → NodeRow{ NodeNumber("3"), NodePower{ NodeVariable('x'), NodeNumber("2") } }
    static vpam::NodePtr fromSymTerm(const SymTerm& term);

    /// Convert a SymPoly to a MathAST NodeRow.
    /// e.g., 2x² + 3x - 5 → NodeRow{ 2x², +, 3x, -, 5 }
    static vpam::NodePtr fromSymPoly(const SymPoly& poly);

    /// Convert a SymEquation to a MathAST NodeRow.
    /// e.g., 2x + 3 = 5 → NodeRow{ 2x, +, 3, =, 5 }
    /// Note: '=' is rendered as a NodeOperator-like element (NodeVariable('=') hack).
    static vpam::NodePtr fromSymEquation(const SymEquation& eq);
};

} // namespace cas
