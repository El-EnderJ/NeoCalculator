/**
 * ASTFlattener.h — Translator: MathAST → SymPoly / SymEquation
 *
 * Converts the visual V.P.A.M. expression tree (MathNode) into the
 * symbolic polynomial representation used by the CAS-Lite engine.
 *
 * Supported node types:
 *   NodeRow           Sequence with operator precedence + implicit ×
 *   NodeNumber        Integer or decimal → ExactVal fraction
 *   NodeVariable      x, y, z, A-F → SymTerm variable
 *   NodeOperator      +, −, × (handled within Row processing)
 *   NodeFraction      num/den → divScalar if den is constant
 *   NodePower         base^exp → polynomial if integer exponent
 *   NodeParen         (content) → recurse
 *   NodeConstant      π, e → ExactVal with piMul/eMul
 *
 * Transcendental nodes (sin, cos, ln, log, √ of variable):
 *   These cannot be represented as polynomials. The flattener returns
 *   a FlattenResult with `transcendental = true`, signalling the caller
 *   to fall back to numeric Newton-Raphson solving.
 *
 * Equation mode:
 *   flattenEquation(lhsRoot, rhsRoot) → SymEquation from two ASTs.
 *   The EquationsApp UI provides separate editors for each side.
 *
 * Part of: NumOS Pro-CAS — Phase B (AST Bridge)
 */

#pragma once

#include <string>
#include "SymPoly.h"
#include "SymEquation.h"
#include "SymExpr.h"
#include "../MathAST.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// FlattenResult — Output of flattening a single expression
// ════════════════════════════════════════════════════════════════════

struct FlattenResult {
    SymPoly     poly;             // The resulting polynomial (poly path)
    SymExpr*    exprTree = nullptr; // SymExpr tree (transcendental path)
    bool        ok = false;       // true if conversion succeeded
    std::string error;            // Error message if !ok
    bool        transcendental = false;  // true if expression has sin/cos/ln/√x etc.

    // Factory helpers
    static FlattenResult success(const SymPoly& p) {
        FlattenResult r;
        r.poly = p;
        r.ok   = true;
        return r;
    }

    static FlattenResult fail(const std::string& msg) {
        FlattenResult r;
        r.ok    = false;
        r.error = msg;
        return r;
    }

    static FlattenResult needsNumeric(const std::string& reason) {
        FlattenResult r;
        r.ok             = false;
        r.transcendental = true;
        r.error          = reason;
        return r;
    }

    /// Successfully built a SymExpr tree (transcendental expression).
    static FlattenResult transcendentalSuccess(SymExpr* expr) {
        FlattenResult r;
        r.exprTree       = expr;
        r.ok             = true;
        r.transcendental = true;
        return r;
    }
};

// ════════════════════════════════════════════════════════════════════
// EquationFlatResult — Output of flattening a full equation
// ════════════════════════════════════════════════════════════════════

struct EquationFlatResult {
    SymEquation eq;               // Polynomial equation (poly path)
    SymExpr*    lhsExpr = nullptr; // LHS SymExpr tree (transcendental path)
    SymExpr*    rhsExpr = nullptr; // RHS SymExpr tree (transcendental path)
    bool        ok = false;
    std::string error;
    bool        transcendental = false;
};

// ════════════════════════════════════════════════════════════════════
// ASTFlattener — Recursive translator: MathNode* → SymPoly
// ════════════════════════════════════════════════════════════════════

class ASTFlattener {
public:
    ASTFlattener();

    /// Set the primary variable for the output polynomials (default: 'x')
    void setVariable(char v) { _var = v; }

    /// Set the arena for SymExpr allocations (enables transcendental path).
    /// If nullptr, transcendental nodes fall back to needsNumeric().
    void setArena(SymExprArena* arena) { _arena = arena; }

    // ── Single expression ───────────────────────────────────────────
    /// Flatten a MathNode tree into a SymPoly.
    /// Returns FlattenResult with ok=true on success.
    FlattenResult flatten(const vpam::MathNode* root);

    // ── Equation (two-tree) ─────────────────────────────────────────
    /// Flatten LHS and RHS AST roots into a SymEquation.
    EquationFlatResult flattenEquation(const vpam::MathNode* lhsRoot,
                                       const vpam::MathNode* rhsRoot);

    // ── SymExpr path (dual-mode) ────────────────────────────────────
    /// Flatten any MathNode tree into a SymExpr tree (arena-allocated).
    /// Works for both polynomial and transcendental expressions.
    /// Requires setArena() to have been called first.
    SymExpr* flattenToExpr(const vpam::MathNode* node);

private:
    char _var;                  // Primary variable for polynomial output
    SymExprArena* _arena = nullptr;  // Arena for SymExpr allocations

    // ── Recursive handlers per node type (polynomial path) ──────────
    FlattenResult flattenNode(const vpam::MathNode* node);
    FlattenResult flattenRow(const vpam::NodeRow* row);
    FlattenResult flattenNumber(const vpam::NodeNumber* node);
    FlattenResult flattenVariable(const vpam::NodeVariable* node);
    FlattenResult flattenFraction(const vpam::NodeFraction* node);
    FlattenResult flattenPower(const vpam::NodePower* node);
    FlattenResult flattenParen(const vpam::NodeParen* node);
    FlattenResult flattenConstant(const vpam::NodeConstant* node);
    FlattenResult flattenRoot(const vpam::NodeRoot* node);

    // ── SymExpr recursive handlers (transcendental path) ────────────
    SymExpr* flattenRowToExpr(const vpam::NodeRow* row);
    SymExpr* flattenFractionToExpr(const vpam::NodeFraction* node);
    SymExpr* flattenPowerToExpr(const vpam::NodePower* node);
    SymExpr* flattenRootToExpr(const vpam::NodeRoot* node);
    SymExpr* flattenFunctionToExpr(const vpam::NodeFunction* node);
    SymExpr* flattenLogBaseToExpr(const vpam::NodeLogBase* node);

    // ── Number parsing ──────────────────────────────────────────────
    /// Parse a numeric string ("42", "3.14") into an ExactVal fraction.
    vpam::ExactVal parseNumberString(const std::string& str);

    // ── Operator precedence within a Row ────────────────────────────
    /// Apply precedence: × before +/−.
    FlattenResult applyPrecedence(
        const std::vector<SymPoly>& terms,
        const std::vector<vpam::OpKind>& ops);
};

} // namespace cas
