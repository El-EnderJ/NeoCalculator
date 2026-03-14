/**
 * SystemHeuristics.cpp — Implementation of the 2×2 system method chooser.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 15 (System Solver)
 */

#include "SystemHeuristics.h"
#include <cmath>
#include <limits>

namespace cas {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static constexpr double kHEps = 1e-12;

/// Extract the coefficient of a single-degree variable term from a node.
/// Returns NaN if the term structure is not recognised.
static double extractCoeff(const NodePtr& node, char var) {
    if (!node) return std::numeric_limits<double>::quiet_NaN();
    const double kNaN = std::numeric_limits<double>::quiet_NaN();

    switch (node->nodeType) {
        case NodeType::Variable: {
            const auto& v = static_cast<const VariableNode&>(*node);
            return (v.name.size() == 1 && v.name[0] == var) ? 1.0 : kNaN;
        }
        case NodeType::Negation: {
            const auto& neg = static_cast<const NegationNode&>(*node);
            double inner = extractCoeff(neg.operand, var);
            return std::isnan(inner) ? kNaN : -inner;
        }
        case NodeType::Product: {
            const auto& prod = static_cast<const ProductNode&>(*node);
            if (prod.children.size() != 2) return kNaN;
            // Try (const, var) and (var, const) orderings
            for (int i = 0; i < 2; ++i) {
                const NodePtr& maybeConst = prod.children[i];
                const NodePtr& maybeVar   = prod.children[1 - i];
                if (maybeConst->nodeType != NodeType::Constant) continue;
                double cv = static_cast<const ConstantNode&>(*maybeConst).value;
                double vcoeff = extractCoeff(maybeVar, var);
                if (!std::isnan(vcoeff)) return cv * vcoeff;
            }
            return kNaN;
        }
        default:
            return kNaN;
    }
}

/// Find the coefficient of `var` (degree-1 term) anywhere in a Sum or standalone.
/// Returns NaN if var does not appear as a linear term, 0.0 if var is absent.
static double findLinearCoeff(const NodePtr& node, char var) {
    if (!node) return 0.0;
    if (node->nodeType == NodeType::Sum) {
        const auto& sum = static_cast<const SumNode&>(*node);
        for (const auto& child : sum.children) {
            double c = extractCoeff(child, var);
            if (!std::isnan(c)) return c;
        }
        return 0.0;   // var not present in this sum
    }
    double c = extractCoeff(node, var);
    return std::isnan(c) ? 0.0 : c;
}

/// True when one side of the equation is exactly a single variable.
static bool sideIsVariable(const NodePtr& node, char var) {
    if (!node) return false;
    if (node->nodeType != NodeType::Variable) return false;
    const auto& v = static_cast<const VariableNode&>(*node);
    return v.name.size() == 1 && v.name[0] == var;
}

/// True when the equation has the form  var = expr  (isolated on LHS or RHS).
static bool isIsolated(const NodePtr& eq, char var) {
    if (!eq || eq->nodeType != NodeType::Equation) return false;
    const auto& e = static_cast<const EquationNode&>(*eq);
    // y = expr  or  expr = y
    return sideIsVariable(e.lhs, var) || sideIsVariable(e.rhs, var);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

SystemSolveMethod chooseSystemMethod(const NodePtr& eq1,
                                      const NodePtr& eq2,
                                      char var1,
                                      char var2) {
    // ── Rule 1: EQUATING ─────────────────────────────────────────────────
    // Both equations isolate the same variable on one side.
    for (char v : {var1, var2}) {
        if (isIsolated(eq1, v) && isIsolated(eq2, v))
            return SystemSolveMethod::EQUATING;
    }

    // ── Rule 2: SUBSTITUTION ──────────────────────────────────────────────
    // At least one equation already has a variable isolated.
    if (isIsolated(eq1, var1) || isIsolated(eq1, var2) ||
        isIsolated(eq2, var1) || isIsolated(eq2, var2))
        return SystemSolveMethod::SUBSTITUTION;

    // ── Rule 3: ELIMINATION ───────────────────────────────────────────────
    // The two equations have opposite coefficients for the same variable
    // (e.g. +3y and -3y, or +x and -x).
    if (eq1 && eq2 &&
        eq1->nodeType == NodeType::Equation &&
        eq2->nodeType == NodeType::Equation) {
        const auto& e1 = static_cast<const EquationNode&>(*eq1);
        const auto& e2 = static_cast<const EquationNode&>(*eq2);

        for (char v : {var1, var2}) {
            double c1 = findLinearCoeff(e1.lhs, v);
            double c2 = findLinearCoeff(e2.lhs, v);
            // Opposite signs AND same magnitude → elimination zeroes the variable
            if (!std::isnan(c1) && !std::isnan(c2) &&
                std::abs(c1) > kHEps && std::abs(c2) > kHEps &&
                std::abs(c1 + c2) < kHEps)
                return SystemSolveMethod::ELIMINATION;
        }
    }

    // ── Rule 4: Fallback ──────────────────────────────────────────────────
    return SystemSolveMethod::SUBSTITUTION;
}

} // namespace cas
