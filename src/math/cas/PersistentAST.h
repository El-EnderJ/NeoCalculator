/**
 * PersistentAST.h — Immutable (Persistent) AST for the CAS Rule Engine.
 *
 * Phase 13A: Persistent (Immutable) AST Architecture for the TRS Engine.
 *
 * Every node in this hierarchy is IMMUTABLE after construction — its children,
 * coefficients, and variable names never change.  This is the foundational
 * property that enables:
 *
 *   · Safe sharing: multiple parent nodes can point to the same child.
 *   · Historical tracking: old tree roots remain valid across rewrites.
 *   · Copy-on-Write (COW): a rewrite that changes node N produces a new parent
 *     sharing all unmodified siblings of N; only the path from root to N is
 *     re-created.
 *
 * ── Node Hierarchy ────────────────────────────────────────────────────────
 *
 *   AstNode              — Abstract base; immutable; carries NodeType tag.
 *   ├── ConstantNode     — Numeric constant, e.g. 3, -½, 2.71828…
 *   ├── VariableNode     — Symbolic variable, e.g. 'x', 'y', 't'
 *   ├── NegationNode     — Unary negation: −child
 *   ├── SumNode          — N-ary sum: a + b + c + …   (children.size() ≥ 2)
 *   ├── ProductNode      — N-ary product: a · b · c … (children.size() ≥ 2)
 *   ├── PowerNode        — Binary exponentiation: base ^ exponent
 *   └── EquationNode     — LHS = RHS (the top-level problem node)
 *
 * ── Ownership Model (COW via std::shared_ptr) ─────────────────────────────
 *
 *   NodePtr = std::shared_ptr<const AstNode>
 *
 *   Rationale: shared_ptr provides automatic reference counting so sibling
 *   sub-trees remain alive as long as any version of the tree references them.
 *   Copying a NodePtr is O(1) and does NOT duplicate the sub-tree.
 *
 *   When the RuleEngine rewrites sub-tree T to T':
 *     1. A new parent P' is constructed containing T' and all original
 *        siblings of T (shared_ptr copies — no duplication of sub-trees).
 *     2. A new path of parent nodes is built up to the root (R').
 *     3. The old root R remains valid — all NodePtrs held by the StepLogger
 *        keep the old tree alive until explicitly released.
 *
 *   ── Allocation: all nodes are allocated from a CasMemoryPool via
 *   std::allocate_shared, placing both the AstNode payload and the
 *   shared_ptr control block in the PSRAM monotonic buffer.
 *
 * ── Factory Functions ─────────────────────────────────────────────────────
 *
 *   makeConstant(pool, value)                 → NodePtr
 *   makeVariable(pool, name)                  → NodePtr
 *   makeNegation(pool, child)                 → NodePtr
 *   makeSum(pool, children)                   → NodePtr
 *   makeProduct(pool, children)               → NodePtr
 *   makePower(pool, base, exponent)           → NodePtr
 *   makeEquation(pool, lhs, rhs)              → NodePtr
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13A (TRS Infrastructure)
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>       // std::shared_ptr, std::allocate_shared
#include <utility>      // std::move
#include "CasMemory.h"

namespace cas {

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
class AstNode;

/// The canonical smart-pointer type for tree nodes.
/// Shared ownership enables O(1) structural sharing (COW).
using NodePtr = std::shared_ptr<const AstNode>;

/// Ordered list of child NodePtrs (used by SumNode, ProductNode).
using NodeList = std::vector<NodePtr>;

// ═════════════════════════════════════════════════════════════════════════════
// §1 — NodeType discriminator
// ═════════════════════════════════════════════════════════════════════════════

enum class NodeType : uint8_t {
    Constant,   ///< Numeric literal
    Variable,   ///< Symbolic variable name
    Negation,   ///< Unary minus
    Sum,        ///< N-ary addition
    Product,    ///< N-ary multiplication
    Power,      ///< Binary exponentiation (base ^ exp)
    Equation,   ///< Top-level equation: LHS = RHS
};

// ═════════════════════════════════════════════════════════════════════════════
// §2 — AstNode — Abstract immutable base
// ═════════════════════════════════════════════════════════════════════════════

class AstNode {
public:
    const NodeType nodeType;

    // ── Polymorphic interface ─────────────────────────────────────────────

    /// Human-readable infix string (debugging / step display).
    virtual std::string toString() const = 0;

    /// Does this subtree reference variable `v`?
    virtual bool containsVar(char v) const = 0;

    /// Numeric evaluation; `varMap` maps variable names to double values.
    /// Returns NaN if a variable is not found in the map.
    virtual double evaluate(char var, double varVal) const = 0;

    /// Structural equality (deep comparison — not pointer identity).
    virtual bool equals(const AstNode& other) const = 0;

    // ── Helpers ───────────────────────────────────────────────────────────

    /// Convenience: is this a ConstantNode?
    bool isConstant()  const { return nodeType == NodeType::Constant; }
    /// Convenience: is this a VariableNode?
    bool isVariable()  const { return nodeType == NodeType::Variable; }
    /// Convenience: is this a NegationNode?
    bool isNegation()  const { return nodeType == NodeType::Negation; }
    /// Convenience: is this a SumNode?
    bool isSum()       const { return nodeType == NodeType::Sum; }
    /// Convenience: is this a ProductNode?
    bool isProduct()   const { return nodeType == NodeType::Product; }
    /// Convenience: is this a PowerNode?
    bool isPower()     const { return nodeType == NodeType::Power; }
    /// Convenience: is this an EquationNode?
    bool isEquation()  const { return nodeType == NodeType::Equation; }

protected:
    explicit AstNode(NodeType t) noexcept : nodeType(t) {}
    // Immutable nodes — no copy, no move, no public destructor.
    // The shared_ptr deleter (from allocate_shared) handles cleanup.
    virtual ~AstNode() = default;

    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
    AstNode(AstNode&&) = delete;
    AstNode& operator=(AstNode&&) = delete;
};

// ═════════════════════════════════════════════════════════════════════════════
// §3 — ConstantNode — Exact rational literal
//
// Every constant is stored as a reduced fraction (numerator / denominator).
// The denominator is always positive (sign is carried by the numerator).
// GCD reduction is applied automatically in the constructor.
//
// For backward compatibility the `value` field holds the double-precision
// floating-point approximation (numerator / denominator).  All existing
// AlgebraicRules arithmetic still operates on `value`; the new rational
// fields are used by CasToVpam for clean fraction rendering.
// ═════════════════════════════════════════════════════════════════════════════

class ConstantNode final : public AstNode {
public:
    const int32_t numerator;    ///< Reduced numerator   (may be negative)
    const int32_t denominator;  ///< Reduced denominator (always > 0)
    const double  value;        ///< Floating-point approximation (num/den)

    /// Construct from an exact rational.  GCD reduction is applied.
    ConstantNode(int32_t num, int32_t den) noexcept;

    /// Convenience: construct from a double (integer values → exact rational).
    explicit ConstantNode(double v) noexcept;

    std::string toString() const override;
    bool containsVar(char /*v*/) const override { return false; }
    double evaluate(char /*var*/, double /*varVal*/) const override { return value; }
    bool equals(const AstNode& other) const override;

    /// True when the constant is an exact integer (denominator == 1).
    bool isInteger() const noexcept { return denominator == 1; }

    /// True when this is an exact (non-integer) rational fraction.
    bool isFraction() const noexcept { return denominator != 1; }
};

// ═════════════════════════════════════════════════════════════════════════════
// §4 — VariableNode — Symbolic variable
// ═════════════════════════════════════════════════════════════════════════════

class VariableNode final : public AstNode {
public:
    /// Variable or wildcard name.
    /// Regular variables are single chars, e.g. "x", "y", "t".
    /// Wildcard pattern variables start with '_', e.g. "_a", "_base", "_exp".
    const std::string name;

    explicit VariableNode(std::string n)
        : AstNode(NodeType::Variable), name(std::move(n)) {}

    std::string toString() const override;
    bool containsVar(char v) const override {
        return name.size() == 1 && name[0] == v;
    }
    double evaluate(char var, double varVal) const override {
        return (name.size() == 1 && name[0] == var)
            ? varVal
            : std::numeric_limits<double>::quiet_NaN();
    }
    bool equals(const AstNode& other) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// §5 — NegationNode — Unary negation: −child
// ═════════════════════════════════════════════════════════════════════════════

class NegationNode final : public AstNode {
public:
    const NodePtr operand;  ///< The expression being negated. Non-null.

    explicit NegationNode(NodePtr op)
        : AstNode(NodeType::Negation), operand(std::move(op)) {}

    std::string toString() const override;
    bool containsVar(char v) const override { return operand && operand->containsVar(v); }
    double evaluate(char var, double varVal) const override {
        return operand ? -operand->evaluate(var, varVal) : 0.0;
    }
    bool equals(const AstNode& other) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// §6 — SumNode — N-ary addition: a + b + c + …
//
// Invariants (enforced by makeSum factory):
//   · children.size() >= 2
//   · No child is itself a SumNode (flattened representation).
// ═════════════════════════════════════════════════════════════════════════════

class SumNode final : public AstNode {
public:
    const NodeList children;  ///< Addends (at least 2). Shared with other trees.

    explicit SumNode(NodeList kids)
        : AstNode(NodeType::Sum), children(std::move(kids)) {}

    std::string toString() const override;
    bool containsVar(char v) const override;
    double evaluate(char var, double varVal) const override;
    bool equals(const AstNode& other) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// §7 — ProductNode — N-ary multiplication: a · b · c · …
//
// Invariants (enforced by makeProduct factory):
//   · children.size() >= 2
//   · No child is itself a ProductNode (flattened representation).
// ═════════════════════════════════════════════════════════════════════════════

class ProductNode final : public AstNode {
public:
    const NodeList children;  ///< Factors (at least 2). Shared with other trees.

    explicit ProductNode(NodeList kids)
        : AstNode(NodeType::Product), children(std::move(kids)) {}

    std::string toString() const override;
    bool containsVar(char v) const override;
    double evaluate(char var, double varVal) const override;
    bool equals(const AstNode& other) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// §8 — PowerNode — Binary exponentiation: base ^ exponent
// ═════════════════════════════════════════════════════════════════════════════

class PowerNode final : public AstNode {
public:
    const NodePtr base;      ///< The base expression. Non-null.
    const NodePtr exponent;  ///< The exponent expression. Non-null.

    PowerNode(NodePtr b, NodePtr e)
        : AstNode(NodeType::Power), base(std::move(b)), exponent(std::move(e)) {}

    std::string toString() const override;
    bool containsVar(char v) const override {
        return (base && base->containsVar(v)) ||
               (exponent && exponent->containsVar(v));
    }
    double evaluate(char var, double varVal) const override {
        double b = base     ? base->evaluate(var, varVal)     : 1.0;
        double e = exponent ? exponent->evaluate(var, varVal) : 1.0;
        return std::pow(b, e);
    }
    bool equals(const AstNode& other) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// §9 — EquationNode — Top-level equation: LHS = RHS
//
// This is the root node of an algebraic problem handed to the RuleEngine.
// The engine produces a sequence of EquationNode roots — one per rewrite step.
// ═════════════════════════════════════════════════════════════════════════════

class EquationNode final : public AstNode {
public:
    const NodePtr lhs;  ///< Left-hand side of the equation.
    const NodePtr rhs;  ///< Right-hand side of the equation.

    EquationNode(NodePtr l, NodePtr r)
        : AstNode(NodeType::Equation), lhs(std::move(l)), rhs(std::move(r)) {}

    std::string toString() const override;
    bool containsVar(char v) const override {
        return (lhs && lhs->containsVar(v)) ||
               (rhs && rhs->containsVar(v));
    }
    double evaluate(char /*var*/, double /*varVal*/) const override {
        // Equations are not numerically evaluated; return NaN.
        return std::numeric_limits<double>::quiet_NaN();
    }
    bool equals(const AstNode& other) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// §10 — Factory functions
//
// All factories allocate from the supplied CasMemoryPool so that both the
// node payload and the shared_ptr control block reside in PSRAM.
//
// ── Copy-on-Write (COW) pattern ───────────────────────────────────────────
//
//   To rewrite sub-tree T rooted at position P in tree R:
//     1. Build the replacement T'.
//     2. Build a new P' = rebuildXxx(pool, P, T') sharing siblings.
//     3. Build a new R' by following the path from P to root, rebuilding each
//        ancestor with the updated child pointer.  All other NodePtrs are
//        shared_ptr copies (O(1), no duplication).
//
// ─────────────────────────────────────────────────────────────────────────────

/// Create a numeric constant node from a double.
/// If the value is a whole number the rational is exact (denominator = 1).
NodePtr makeConstant(CasMemoryPool& pool, double value);

/// Create an exact rational constant, automatically GCD-reduced.
/// Precondition: den != 0.
NodePtr makeRational(CasMemoryPool& pool, int32_t num, int32_t den);

/// Create a variable reference node for a regular single-character variable.
NodePtr makeVariable(CasMemoryPool& pool, char name);

/// Create a variable reference node with a full string name.
/// Used for multi-character wildcard identifiers like "_a", "_base".
NodePtr makeVariableNamed(CasMemoryPool& pool, std::string name);

/// Create a unary negation node.
/// If `operand` is already a NegationNode, returns its inner child
/// (double-negation elimination).
NodePtr makeNegation(CasMemoryPool& pool, NodePtr operand);

/// Create an N-ary sum node.
/// Flattens nested SumNodes so the children list is always flat.
/// If `children.size() == 1`, returns the single child directly (no wrapper).
NodePtr makeSum(CasMemoryPool& pool, NodeList children);

/// Create an N-ary product node.
/// Flattens nested ProductNodes so the children list is always flat.
/// If `children.size() == 1`, returns the single child directly (no wrapper).
NodePtr makeProduct(CasMemoryPool& pool, NodeList children);

/// Create a power node.
NodePtr makePower(CasMemoryPool& pool, NodePtr base, NodePtr exponent);

/// Create an equation node (top-level LHS = RHS).
NodePtr makeEquation(CasMemoryPool& pool, NodePtr lhs, NodePtr rhs);

// ── COW helpers ──────────────────────────────────────────────────────────────

/// Return a new SumNode that is identical to `sum` except the child at `index`
/// is replaced by `newChild`.  All other children are shared (no copy).
/// Precondition: sum->isSum() && index < sum.children.size().
NodePtr replaceChildInSum(CasMemoryPool& pool,
                          const SumNode& sum,
                          std::size_t index,
                          NodePtr newChild);

/// Return a new ProductNode that is identical to `prod` except the child at
/// `index` is replaced by `newChild`.  All other children are shared.
NodePtr replaceChildInProduct(CasMemoryPool& pool,
                              const ProductNode& prod,
                              std::size_t index,
                              NodePtr newChild);

/// Return a new EquationNode with lhs replaced.
NodePtr replaceEquationLHS(CasMemoryPool& pool,
                           const EquationNode& eq,
                           NodePtr newLhs);

/// Return a new EquationNode with rhs replaced.
NodePtr replaceEquationRHS(CasMemoryPool& pool,
                           const EquationNode& eq,
                           NodePtr newRhs);

}  // namespace cas
