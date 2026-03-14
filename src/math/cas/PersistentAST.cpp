/**
 * PersistentAST.cpp — Implementation of the immutable AST node hierarchy.
 *
 * Provides:
 *   · toString() for all node types (infix notation for readability)
 *   · equals() deep structural comparison
 *   · Factory functions that enforce invariants and COW helpers
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13A (TRS Infrastructure)
 */

#include "PersistentAST.h"
#include <sstream>
#include <cmath>
#include <limits>

namespace cas {

// ─────────────────────────────────────────────────────────────────────────────
// ConstantNode
// ─────────────────────────────────────────────────────────────────────────────

/// Compile-time-friendly GCD (Euclidean, works on positive values).
static int32_t gcd32(int32_t a, int32_t b) noexcept {
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;
    while (b) { int32_t t = b; b = a % b; a = t; }
    return (a == 0) ? 1 : a;
}

ConstantNode::ConstantNode(int32_t num, int32_t den) noexcept
    : AstNode(NodeType::Constant)
    , numerator([&] {
        if (den == 0) return num; // degenerate guard
        int32_t g = gcd32(num, den);
        // Normalise sign: denominator must be positive
        return (den < 0) ? -(num / g) : (num / g);
    }())
    , denominator([&] {
        if (den == 0) return int32_t(1);
        int32_t g = gcd32(num, den);
        int32_t d = (den < 0) ? -(den / g) : (den / g);
        return (d == 0) ? int32_t(1) : d;
    }())
    , value(static_cast<double>(numerator) / static_cast<double>(denominator))
{}

ConstantNode::ConstantNode(double v) noexcept
    : AstNode(NodeType::Constant)
    , numerator([&]() -> int32_t {
        double intpart;
        if (std::modf(v, &intpart) == 0.0 && std::fabs(v) < 2.0e9)
            return static_cast<int32_t>(intpart);
        return 0; // non-integer double: stored with denominator=0 (not a clean rational)
    }())
    , denominator([&]() -> int32_t {
        double intpart;
        if (std::modf(v, &intpart) == 0.0 && std::fabs(v) < 2.0e9)
            return 1;
        return 0; // non-integer double: denominator=0 signals "not a clean rational"
    }())
    , value(v)
{}

std::string ConstantNode::toString() const {
    // Exact rational fraction: show as "num/den"
    if (denominator > 1) {
        std::ostringstream oss;
        oss << numerator << "/" << denominator;
        return oss.str();
    }
    // Integer stored exactly
    if (denominator == 1) {
        std::ostringstream oss;
        oss << numerator;
        return oss.str();
    }
    // Fallback: non-clean double
    double intpart;
    if (std::modf(value, &intpart) == 0.0 && std::abs(value) < 1e15) {
        std::ostringstream oss;
        oss << static_cast<long long>(intpart);
        return oss.str();
    }
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

bool ConstantNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Constant) return false;
    const auto& o = static_cast<const ConstantNode&>(other);
    // Compare by rational fields when both are clean rationals (denominator > 0).
    // For non-integer doubles (denominator == 0) fall back to double comparison.
    if (denominator > 0 && o.denominator > 0)
        return numerator == o.numerator && denominator == o.denominator;
    // At least one operand is a non-clean double: compare values.
    return value == o.value;
}

// ─────────────────────────────────────────────────────────────────────────────
// VariableNode
// ─────────────────────────────────────────────────────────────────────────────

std::string VariableNode::toString() const {
    return name;
}

bool VariableNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Variable) return false;
    return name == static_cast<const VariableNode&>(other).name;
}

// ─────────────────────────────────────────────────────────────────────────────
// NegationNode
// ─────────────────────────────────────────────────────────────────────────────

std::string NegationNode::toString() const {
    if (!operand) return "-(null)";
    // Parenthesise compound operands for clarity
    if (operand->isSum() || operand->isProduct()) {
        return "-(" + operand->toString() + ")";
    }
    return "-" + operand->toString();
}

bool NegationNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Negation) return false;
    const auto& o = static_cast<const NegationNode&>(other);
    if (!operand && !o.operand) return true;
    if (!operand || !o.operand) return false;
    return operand->equals(*o.operand);
}

// ─────────────────────────────────────────────────────────────────────────────
// SumNode
// ─────────────────────────────────────────────────────────────────────────────

bool SumNode::containsVar(char v) const {
    for (const auto& child : children) {
        if (child && child->containsVar(v)) return true;
    }
    return false;
}

double SumNode::evaluate(char var, double varVal) const {
    double result = 0.0;
    for (const auto& child : children) {
        if (child) result += child->evaluate(var, varVal);
    }
    return result;
}

std::string SumNode::toString() const {
    std::string s;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i]) continue;
        std::string part = children[i]->toString();
        if (i == 0) {
            s = part;
        } else {
            // If part starts with '-', omit the '+' separator
            if (!part.empty() && part[0] == '-') {
                s += part;
            } else {
                s += " + " + part;
            }
        }
    }
    return s;
}

bool SumNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Sum) return false;
    const auto& o = static_cast<const SumNode&>(other);
    if (children.size() != o.children.size()) return false;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i] && !o.children[i]) continue;
        if (!children[i] || !o.children[i]) return false;
        if (!children[i]->equals(*o.children[i])) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ProductNode
// ─────────────────────────────────────────────────────────────────────────────

bool ProductNode::containsVar(char v) const {
    for (const auto& child : children) {
        if (child && child->containsVar(v)) return true;
    }
    return false;
}

double ProductNode::evaluate(char var, double varVal) const {
    double result = 1.0;
    for (const auto& child : children) {
        if (child) result *= child->evaluate(var, varVal);
    }
    return result;
}

std::string ProductNode::toString() const {
    std::string s;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i]) continue;
        // Parenthesise sums inside products
        std::string part;
        if (children[i]->isSum() || children[i]->isNegation()) {
            part = "(" + children[i]->toString() + ")";
        } else {
            part = children[i]->toString();
        }
        if (i == 0) {
            s = part;
        } else {
            s += " * " + part;
        }
    }
    return s;
}

bool ProductNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Product) return false;
    const auto& o = static_cast<const ProductNode&>(other);
    if (children.size() != o.children.size()) return false;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i] && !o.children[i]) continue;
        if (!children[i] || !o.children[i]) return false;
        if (!children[i]->equals(*o.children[i])) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// PowerNode
// ─────────────────────────────────────────────────────────────────────────────

std::string PowerNode::toString() const {
    std::string b = base     ? base->toString()     : "null";
    std::string e = exponent ? exponent->toString() : "null";
    // Parenthesise compound sub-expressions
    if (base && (base->isSum() || base->isProduct() || base->isNegation())) {
        b = "(" + b + ")";
    }
    if (exponent && (exponent->isSum() || exponent->isProduct())) {
        e = "(" + e + ")";
    }
    return b + "^" + e;
}

bool PowerNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Power) return false;
    const auto& o = static_cast<const PowerNode&>(other);
    if (!base && !o.base && !exponent && !o.exponent) return true;
    if (!base || !o.base || !exponent || !o.exponent) return false;
    return base->equals(*o.base) && exponent->equals(*o.exponent);
}

// ─────────────────────────────────────────────────────────────────────────────
// EquationNode
// ─────────────────────────────────────────────────────────────────────────────

std::string EquationNode::toString() const {
    std::string l = lhs ? lhs->toString() : "null";
    std::string r = rhs ? rhs->toString() : "null";
    return l + " = " + r;
}

bool EquationNode::equals(const AstNode& other) const {
    if (other.nodeType != NodeType::Equation) return false;
    const auto& o = static_cast<const EquationNode&>(other);
    if (!lhs && !o.lhs && !rhs && !o.rhs) return true;
    if (!lhs || !o.lhs || !rhs || !o.rhs) return false;
    return lhs->equals(*o.lhs) && rhs->equals(*o.rhs);
}

// ═════════════════════════════════════════════════════════════════════════════
// Factory functions
// ═════════════════════════════════════════════════════════════════════════════

NodePtr makeConstant(CasMemoryPool& pool, double value) {
    return allocateNode<ConstantNode>(pool, value);
}

NodePtr makeRational(CasMemoryPool& pool, int32_t num, int32_t den) {
    return allocateNode<ConstantNode>(pool, num, den);
}

NodePtr makeVariable(CasMemoryPool& pool, char name) {
    return allocateNode<VariableNode>(pool, std::string(1, name));
}

NodePtr makeVariableNamed(CasMemoryPool& pool, std::string name) {
    return allocateNode<VariableNode>(pool, std::move(name));
}

NodePtr makeNegation(CasMemoryPool& pool, NodePtr operand) {
    // Eliminate double negation: -(-x) → x
    if (operand && operand->isNegation()) {
        return static_cast<const NegationNode&>(*operand).operand;
    }
    return allocateNode<NegationNode>(pool, std::move(operand));
}

NodePtr makeSum(CasMemoryPool& pool, NodeList children) {
    // Flatten nested SumNodes into a single level
    NodeList flat;
    flat.reserve(children.size());
    for (auto& child : children) {
        if (child && child->isSum()) {
            const auto& s = static_cast<const SumNode&>(*child);
            for (const auto& grandchild : s.children) {
                flat.push_back(grandchild);
            }
        } else {
            flat.push_back(std::move(child));
        }
    }
    if (flat.size() == 1) return flat[0];
    if (flat.empty())     return makeConstant(pool, 0.0);
    return allocateNode<SumNode>(pool, std::move(flat));
}

NodePtr makeProduct(CasMemoryPool& pool, NodeList children) {
    // Flatten nested ProductNodes into a single level
    NodeList flat;
    flat.reserve(children.size());
    for (auto& child : children) {
        if (child && child->isProduct()) {
            const auto& p = static_cast<const ProductNode&>(*child);
            for (const auto& grandchild : p.children) {
                flat.push_back(grandchild);
            }
        } else {
            flat.push_back(std::move(child));
        }
    }
    if (flat.size() == 1) return flat[0];
    if (flat.empty())     return makeConstant(pool, 1.0);
    return allocateNode<ProductNode>(pool, std::move(flat));
}

NodePtr makePower(CasMemoryPool& pool, NodePtr base, NodePtr exponent) {
    return allocateNode<PowerNode>(pool, std::move(base), std::move(exponent));
}

NodePtr makeEquation(CasMemoryPool& pool, NodePtr lhs, NodePtr rhs) {
    return allocateNode<EquationNode>(pool, std::move(lhs), std::move(rhs));
}

// ─────────────────────────────────────────────────────────────────────────────
// COW helpers — rebuild a node with one child replaced, sharing all others
// ─────────────────────────────────────────────────────────────────────────────

NodePtr replaceChildInSum(CasMemoryPool& pool,
                          const SumNode& sum,
                          std::size_t index,
                          NodePtr newChild) {
    // Build a new child list: copy all shared_ptrs except the replaced one
    NodeList kids;
    kids.reserve(sum.children.size());
    for (std::size_t i = 0; i < sum.children.size(); ++i) {
        kids.push_back(i == index ? newChild : sum.children[i]);
    }
    return allocateNode<SumNode>(pool, std::move(kids));
}

NodePtr replaceChildInProduct(CasMemoryPool& pool,
                              const ProductNode& prod,
                              std::size_t index,
                              NodePtr newChild) {
    NodeList kids;
    kids.reserve(prod.children.size());
    for (std::size_t i = 0; i < prod.children.size(); ++i) {
        kids.push_back(i == index ? newChild : prod.children[i]);
    }
    return allocateNode<ProductNode>(pool, std::move(kids));
}

NodePtr replaceEquationLHS(CasMemoryPool& pool,
                           const EquationNode& eq,
                           NodePtr newLhs) {
    return allocateNode<EquationNode>(pool, std::move(newLhs), eq.rhs);
}

NodePtr replaceEquationRHS(CasMemoryPool& pool,
                           const EquationNode& eq,
                           NodePtr newRhs) {
    return allocateNode<EquationNode>(pool, eq.lhs, std::move(newRhs));
}

}  // namespace cas
