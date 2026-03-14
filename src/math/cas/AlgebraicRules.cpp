/**
 * AlgebraicRules.cpp — Implementation of the 10-rule pedagogical rule library.
 *
 * ── Design notes ─────────────────────────────────────────────────────────────
 *
 * All rules use the `customTransform` code path rather than structural
 * pattern / replacement templates.  This allows complex matching logic
 * (multi-term sums, coefficient extraction, equation-level operations) that
 * cannot be expressed cleanly with wildcard patterns.
 *
 * Rules are ordered so that higher-priority phases appear first in the returned
 * vector; when the RuleEngine iterates the list, expansion fires before
 * simplification, which fires before transposition, etc.
 *
 * Memory: every new node is created through the factory functions in
 * PersistentAST.h (makeConstant, makeSum, …), which allocate from the
 * CasMemoryPool supplied at construction time, satisfying the Phase 13A
 * requirement that all CAS allocations use the PSRAM pmr pool.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13B (Algebraic Brain & Smart Diffing)
 */

#include "AlgebraicRules.h"
#include <cmath>
#include <limits>
#include <sstream>

namespace cas {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers — not exposed in the header
// ─────────────────────────────────────────────────────────────────────────────

// Named epsilon used throughout for floating-point near-equality tests.
static constexpr double kEps = 1e-12;

/// True if |a - b| < kEps.
static bool isApprox(double a, double b) { return std::abs(a - b) < kEps; }

/// Return the numeric value of a node if it is purely numeric, else NaN.
/// Handles both ConstantNode(v) and NegationNode(ConstantNode(v)).
static double getNumericValue(const NodePtr& node) {
    if (!node) return std::numeric_limits<double>::quiet_NaN();
    if (node->nodeType == NodeType::Constant)
        return static_cast<const ConstantNode&>(*node).value;
    if (node->nodeType == NodeType::Negation) {
        const auto& neg = static_cast<const NegationNode&>(*node);
        if (neg.operand && neg.operand->nodeType == NodeType::Constant)
            return -static_cast<const ConstantNode&>(*neg.operand).value;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

static bool isNumeric(const NodePtr& node) {
    return !std::isnan(getNumericValue(node));
}

/// Format a double exponent as a short string: 2.0 → "2", 0.5 → "0.5".
static std::string fmtExp(double e) {
    double intpart;
    if (std::modf(e, &intpart) == 0.0) {
        std::ostringstream oss;
        oss << static_cast<long long>(intpart);
        return oss.str();
    }
    std::ostringstream oss;
    oss << e;
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// TermExtract — coefficient + canonical "key" for CombineLikeTerms
// ─────────────────────────────────────────────────────────────────────────────

struct TermExtract {
    double  coeff;   ///< Numeric coefficient (NaN if unrecognised).
    std::string key; ///< Canonical key: "" = unrecognised, "__k" = pure const,
                     ///  or varName+expStr (e.g. "x1", "y2") for variable terms.
    NodePtr base;    ///< The non-coefficient part (for reconstruction).
};

/// Extract coefficient + canonical key from a sum-child node.
static TermExtract extractTerm(const NodePtr& node);

static TermExtract extractTerm(const NodePtr& node) {
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    if (!node) return {kNaN, "", nullptr};

    switch (node->nodeType) {

        case NodeType::Constant:
            return {static_cast<const ConstantNode&>(*node).value, "__k", nullptr};

        case NodeType::Variable: {
            const std::string& name = static_cast<const VariableNode&>(*node).name;
            // Wildcards are not algebraic terms
            if (!name.empty() && name[0] == '_') return {kNaN, "", nullptr};
            return {1.0, name + "1", node};
        }

        case NodeType::Power: {
            const auto& pw = static_cast<const PowerNode&>(*node);
            if (!pw.base || !pw.exponent) return {kNaN, "", nullptr};
            if (pw.base->nodeType != NodeType::Variable) return {kNaN, "", nullptr};
            const std::string& name = static_cast<const VariableNode&>(*pw.base).name;
            if (!name.empty() && name[0] == '_')  return {kNaN, "", nullptr};
            double expVal = getNumericValue(pw.exponent);
            if (std::isnan(expVal)) return {kNaN, "", nullptr};
            return {1.0, name + fmtExp(expVal), node};
        }

        case NodeType::Product: {
            const auto& prod = static_cast<const ProductNode&>(*node);
            if (prod.children.size() != 2) return {kNaN, "", nullptr};
            // Try both orderings: (const, var-part) and (var-part, const)
            for (int i = 0; i < 2; ++i) {
                const NodePtr& maybeConst = prod.children[i];
                const NodePtr& maybeVar   = prod.children[1 - i];
                double cv = getNumericValue(maybeConst);
                if (std::isnan(cv)) continue;
                TermExtract ve = extractTerm(maybeVar);
                if (!std::isnan(ve.coeff))
                    return {cv * ve.coeff, ve.key, ve.base};
            }
            return {kNaN, "", nullptr};
        }

        case NodeType::Negation: {
            const auto& neg = static_cast<const NegationNode&>(*node);
            TermExtract inner = extractTerm(neg.operand);
            if (!std::isnan(inner.coeff))
                return {-inner.coeff, inner.key, inner.base};
            return {kNaN, "", nullptr};
        }

        default:
            return {kNaN, "", nullptr};
    }
}

/// Reconstruct a term from a coefficient and a base NodePtr.
///   coeff=2, base=x   →  Product(2, x)
///   coeff=1, base=x   →  x
///   coeff=-1, base=x  →  Neg(x)
///   coeff=0, base=x   →  Constant(0)
///   key=="__k"         →  Constant(coeff)
static NodePtr buildTerm(CasMemoryPool& pool,
                         double coeff, const std::string& key,
                         const NodePtr& base) {
    if (key == "__k")
        return makeConstant(pool, coeff);
    if (!base) return makeConstant(pool, 0.0);

    const double eps = kEps;
    if (std::abs(coeff) < eps)
        return makeConstant(pool, 0.0);
    if (isApprox(coeff, 1.0))
        return base;
    if (isApprox(coeff, -1.0))
        return makeNegation(pool, base);
    if (coeff < 0.0)
        return makeNegation(pool,
               makeProduct(pool, {makeConstant(pool, -coeff), base}));
    return makeProduct(pool, {makeConstant(pool, coeff), base});
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 1 — DoubleNegation:  -(-x) → x
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeDoubleNegationRule() {
    return RewriteRule{
        "DoubleNegation",
        "A double negative cancels out: -(-x) = x",
        RulePhase::Expansion,
        [](const AstNode& node, CasMemoryPool& /*pool*/) -> NodePtr {
            if (node.nodeType != NodeType::Negation) return nullptr;
            const auto& outer = static_cast<const NegationNode&>(node);
            if (!outer.operand ||
                outer.operand->nodeType != NodeType::Negation) return nullptr;
            const auto& inner = static_cast<const NegationNode&>(*outer.operand);
            return inner.operand;
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 2 — DistributiveProperty:  a(b + c) → ab + ac
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeDistributivePropertyRule() {
    return RewriteRule{
        "DistributiveProperty",
        "Distribute multiplication over addition: a(b + c) = ab + ac",
        RulePhase::Expansion,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Product) return nullptr;
            const auto& prod = static_cast<const ProductNode&>(node);

            // Find the first (leftmost) SumNode child
            std::size_t sumIdx = prod.children.size();
            for (std::size_t i = 0; i < prod.children.size(); ++i) {
                if (prod.children[i] &&
                    prod.children[i]->nodeType == NodeType::Sum) {
                    sumIdx = i;
                    break;
                }
            }
            if (sumIdx == prod.children.size()) return nullptr;

            // Collect all other factors
            NodeList otherFactors;
            otherFactors.reserve(prod.children.size() - 1);
            for (std::size_t i = 0; i < prod.children.size(); ++i) {
                if (i != sumIdx) otherFactors.push_back(prod.children[i]);
            }

            const auto& sumNode =
                static_cast<const SumNode&>(*prod.children[sumIdx]);

            // Build a new sum: each term multiplied by all other factors
            NodeList newTerms;
            newTerms.reserve(sumNode.children.size());
            for (const auto& term : sumNode.children) {
                NodeList factors;
                factors.reserve(1 + otherFactors.size());
                factors.push_back(term);
                for (const auto& f : otherFactors) factors.push_back(f);
                newTerms.push_back(makeProduct(pool, std::move(factors)));
            }
            return makeSum(pool, std::move(newTerms));
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 3 — PowerExpansion:  (ab)^c → a^c · b^c
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makePowerExpansionRule() {
    return RewriteRule{
        "PowerExpansion",
        "Expand power over a product: (ab)^c = a^c * b^c",
        RulePhase::Expansion,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Power) return nullptr;
            const auto& pw = static_cast<const PowerNode&>(node);
            if (!pw.base || pw.base->nodeType != NodeType::Product) return nullptr;

            const auto& baseProd = static_cast<const ProductNode&>(*pw.base);
            NodeList newFactors;
            newFactors.reserve(baseProd.children.size());
            for (const auto& factor : baseProd.children)
                newFactors.push_back(makePower(pool, factor, pw.exponent));
            return makeProduct(pool, std::move(newFactors));
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 4 — CombineConstants:  5 + 3 → 8  and  2 * 3 → 6
//
// Folds ALL purely-numeric children of a SumNode or ProductNode into a single
// constant.  Handles both ConstantNode(v) and NegationNode(ConstantNode(v)).
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeCombineConstantsRule() {
    return RewriteRule{
        "CombineConstants",
        "Evaluate and combine constant terms",
        RulePhase::Simplification,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {

            // Neg(Const(v)) → Const(-v)  (e.g. -(-3) → 3, or negate literal)
            if (node.nodeType == NodeType::Negation) {
                const auto& neg = static_cast<const NegationNode&>(node);
                if (neg.operand && neg.operand->nodeType == NodeType::Constant) {
                    double v = static_cast<const ConstantNode&>(*neg.operand).value;
                    return makeConstant(pool, -v);
                }
                return nullptr;
            }

            if (node.nodeType == NodeType::Sum) {
                const auto& sum = static_cast<const SumNode&>(node);
                double  acc       = 0.0;
                int     numCount  = 0;
                NodeList nonNum;
                for (const auto& child : sum.children) {
                    double v = getNumericValue(child);
                    if (!std::isnan(v)) { acc += v; ++numCount; }
                    else                { nonNum.push_back(child); }
                }
                if (numCount < 2) return nullptr; // nothing to fold
                nonNum.push_back(makeConstant(pool, acc));
                return makeSum(pool, std::move(nonNum));
            }

            if (node.nodeType == NodeType::Product) {
                const auto& prod = static_cast<const ProductNode&>(node);
                double  acc       = 1.0;
                int     numCount  = 0;
                NodeList nonNum;
                for (const auto& child : prod.children) {
                    double v = getNumericValue(child);
                    if (!std::isnan(v)) { acc *= v; ++numCount; }
                    else                { nonNum.push_back(child); }
                }
                if (numCount < 2) return nullptr;
                // zero product short-circuit
                if (std::abs(acc) < kEps) return makeConstant(pool, 0.0);
                nonNum.push_back(makeConstant(pool, acc));
                return makeProduct(pool, std::move(nonNum));
            }

            return nullptr;
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 5 — ConstantPowerEval:  c^n → result  (both operands numeric)
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeConstantPowerEvalRule() {
    return RewriteRule{
        "ConstantPowerEval",
        "Evaluate a power of constants: c^n = result",
        RulePhase::Simplification,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Power) return nullptr;
            const auto& pw = static_cast<const PowerNode&>(node);
            double base = getNumericValue(pw.base);
            double exp  = getNumericValue(pw.exponent);
            if (std::isnan(base) || std::isnan(exp)) return nullptr;
            return makeConstant(pool, std::pow(base, exp));
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 6 — CombineLikeTerms:  2x + 4x → 6x
//
// Groups terms in a SumNode by their canonical key (variable name + exponent).
// Pure-constant terms (key == "__k") are skipped — handled by CombineConstants.
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeCombineLikeTermsRule() {
    return RewriteRule{
        "CombineLikeTerms",
        "Combine like terms by adding their coefficients",
        RulePhase::Simplification,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Sum) return nullptr;
            const auto& sum = static_cast<const SumNode&>(node);
            if (sum.children.size() < 2) return nullptr;

            std::vector<TermExtract> terms;
            terms.reserve(sum.children.size());
            for (const auto& child : sum.children)
                terms.push_back(extractTerm(child));

            std::vector<bool> used(sum.children.size(), false);
            NodeList newKids;
            bool anyChange = false;

            for (std::size_t i = 0; i < sum.children.size(); ++i) {
                if (used[i]) continue;
                const TermExtract& ti = terms[i];

                // Skip unrecognised terms or pure constants (CombineConstants handles those)
                if (std::isnan(ti.coeff) || ti.key.empty() || ti.key == "__k") {
                    newKids.push_back(sum.children[i]);
                    continue;
                }

                double combinedCoeff = ti.coeff;
                bool   found         = false;

                for (std::size_t j = i + 1; j < sum.children.size(); ++j) {
                    if (used[j]) continue;
                    const TermExtract& tj = terms[j];
                    if (!std::isnan(tj.coeff) && tj.key == ti.key) {
                        combinedCoeff += tj.coeff;
                        used[j] = true;
                        found   = true;
                    }
                }

                if (!found) {
                    newKids.push_back(sum.children[i]);
                    continue;
                }

                anyChange = true;
                newKids.push_back(buildTerm(pool, combinedCoeff, ti.key, ti.base));
            }

            if (!anyChange) return nullptr;
            if (newKids.empty()) return makeConstant(pool, 0.0);
            if (newKids.size() == 1) return newKids[0];
            return makeSum(pool, std::move(newKids));
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 7 — IdentityElimination:  x + 0 → x,  x * 1 → x,  x * 0 → 0
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeIdentityEliminationRule() {
    return RewriteRule{
        "IdentityElimination",
        "Remove additive identity (x + 0 = x) and multiplicative identity (x * 1 = x)",
        RulePhase::Simplification,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {

            if (node.nodeType == NodeType::Sum) {
                const auto& sum = static_cast<const SumNode&>(node);
                NodeList nonZero;
                bool     hasZero = false;
                for (const auto& child : sum.children) {
                    double v = getNumericValue(child);
                    if (!std::isnan(v) && v == 0.0) { hasZero = true; continue; }
                    nonZero.push_back(child);
                }
                if (!hasZero) return nullptr;
                if (nonZero.empty())    return makeConstant(pool, 0.0);
                if (nonZero.size() == 1) return nonZero[0];
                return makeSum(pool, std::move(nonZero));
            }

            if (node.nodeType == NodeType::Product) {
                const auto& prod = static_cast<const ProductNode&>(node);
                NodeList nonOne;
                bool     hasOne = false;
                for (const auto& child : prod.children) {
                    double v = getNumericValue(child);
                    if (!std::isnan(v)) {
                        if (v == 0.0) return makeConstant(pool, 0.0); // x * 0 = 0
                        if (v == 1.0) { hasOne = true; continue; }    // x * 1 = x
                    }
                    nonOne.push_back(child);
                }
                if (!hasOne) return nullptr;
                if (nonOne.empty())    return makeConstant(pool, 1.0);
                if (nonOne.size() == 1) return nonOne[0];
                return makeProduct(pool, std::move(nonOne));
            }

            return nullptr;
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 8 — MoveConstantsToRHS  (Transposition — pedagogical intermediate step)
//
// Pattern: Equation where LHS is a Sum that contains a non-zero constant term.
// Action:  Subtract that constant from both sides, generating the intermediate
//          working form before simplification.
//
//   x + 3 = 7  →  x + 3 + (-3) = 7 + (-3)
//
// On the next step(s), CombineConstants / IdentityElimination will simplify.
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeMoveConstantsToRHSRule() {
    return RewriteRule{
        "MoveConstantsToRHS",
        "Subtract the constant from both sides to isolate the variable term",
        RulePhase::Transposition,
        [](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Equation) return nullptr;
            const auto& eq = static_cast<const EquationNode&>(node);
            if (!eq.lhs || !eq.rhs) return nullptr;
            if (eq.lhs->nodeType != NodeType::Sum) return nullptr;

            const auto& lhsSum = static_cast<const SumNode&>(*eq.lhs);

            // Find the first non-zero purely-numeric child in LHS
            int    constIdx  = -1;
            double constVal  = 0.0;
            for (int i = 0; i < static_cast<int>(lhsSum.children.size()); ++i) {
                double v = getNumericValue(lhsSum.children[i]);
                if (!std::isnan(v) && v != 0.0) {
                    constIdx = i;
                    constVal = v;
                    break;
                }
            }
            if (constIdx < 0) return nullptr;

            // The "add to both sides" value is the negation of constVal
            NodePtr adjustment = makeConstant(pool, -constVal);

            // New LHS: append adjustment
            NodeList newLhsKids;
            newLhsKids.reserve(lhsSum.children.size() + 1);
            for (const auto& c : lhsSum.children) newLhsKids.push_back(c);
            newLhsKids.push_back(adjustment);
            NodePtr newLhs = makeSum(pool, std::move(newLhsKids));

            // New RHS: append adjustment (may be Sum or scalar)
            NodePtr newRhs;
            if (eq.rhs->nodeType == NodeType::Sum) {
                const auto& rhsSum = static_cast<const SumNode&>(*eq.rhs);
                NodeList rhsKids;
                rhsKids.reserve(rhsSum.children.size() + 1);
                for (const auto& c : rhsSum.children) rhsKids.push_back(c);
                rhsKids.push_back(adjustment);
                newRhs = makeSum(pool, std::move(rhsKids));
            } else {
                newRhs = makeSum(pool, {eq.rhs, adjustment});
            }

            return makeEquation(pool, newLhs, newRhs);
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 9 — MoveVariablesToLHS  (Transposition)
//
// Pattern: Equation where RHS contains a recognisable variable term (coeff * varⁿ).
// Action:  Subtract that term from both sides (intermediate working step).
//
//   5 = 2x + 1  →  5 + (-2x) = 2x + 1 + (-2x)
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeMoveVariablesToLHSRule(char var) {
    return RewriteRule{
        "MoveVariablesToLHS",
        "Move the variable term to the left-hand side by subtracting from both sides",
        RulePhase::Transposition,
        [var](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Equation) return nullptr;
            const auto& eq = static_cast<const EquationNode&>(node);
            if (!eq.lhs || !eq.rhs) return nullptr;
            if (!eq.rhs->containsVar(var)) return nullptr;
            if (eq.rhs->nodeType != NodeType::Sum) return nullptr;

            const auto& rhsSum = static_cast<const SumNode&>(*eq.rhs);

            // Find the first variable term in RHS that contains `var`
            int varIdx = -1;
            for (int i = 0; i < static_cast<int>(rhsSum.children.size()); ++i) {
                if (rhsSum.children[i] && rhsSum.children[i]->containsVar(var)) {
                    varIdx = i;
                    break;
                }
            }
            if (varIdx < 0) return nullptr;

            // Adjustment = Neg(varTerm)
            NodePtr varTerm    = rhsSum.children[varIdx];
            NodePtr adjustment = makeNegation(pool, varTerm);

            // New LHS: lhs + adjustment
            NodePtr newLhs;
            if (eq.lhs->nodeType == NodeType::Sum) {
                const auto& lhsSum = static_cast<const SumNode&>(*eq.lhs);
                NodeList kids;
                kids.reserve(lhsSum.children.size() + 1);
                for (const auto& c : lhsSum.children) kids.push_back(c);
                kids.push_back(adjustment);
                newLhs = makeSum(pool, std::move(kids));
            } else {
                newLhs = makeSum(pool, {eq.lhs, adjustment});
            }

            // New RHS: rhs + adjustment
            NodeList rhsKids;
            rhsKids.reserve(rhsSum.children.size() + 1);
            for (const auto& c : rhsSum.children) rhsKids.push_back(c);
            rhsKids.push_back(adjustment);
            NodePtr newRhs = makeSum(pool, std::move(rhsKids));

            return makeEquation(pool, newLhs, newRhs);
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 10 — DivideByCoefficient:  A·x = B  →  x = B/A
//
// Fires when LHS is exactly a scalar multiple of the target variable:
//   · Variable(var)                         — coefficient = 1  (no-op, skip)
//   · Product(Const(A), Variable(var))      — coefficient = A
//   · Negation(Variable(var))               — coefficient = -1
//   · Negation(Product(Const(A), Var(var))) — coefficient = -A
// ─────────────────────────────────────────────────────────────────────────────
static RewriteRule makeDivideByCoefficientRule(char var) {
    return RewriteRule{
        "DivideByCoefficient",
        "Divide both sides by the coefficient to solve for the variable",
        RulePhase::Reduction,
        [var](const AstNode& node, CasMemoryPool& pool) -> NodePtr {
            if (node.nodeType != NodeType::Equation) return nullptr;
            const auto& eq = static_cast<const EquationNode&>(node);
            if (!eq.lhs || !eq.rhs) return nullptr;

            // Extract coefficient from LHS using extractTerm
            TermExtract te = extractTerm(eq.lhs);
            if (std::isnan(te.coeff)) return nullptr;

            // Check that the base is exactly var^1
            std::string expectedKey = std::string(1, var) + "1";
            if (te.key != expectedKey) return nullptr;

            // coeff == 1 means LHS is already just var — nothing to do
            if (isApprox(te.coeff, 1.0)) return nullptr;
            if (std::abs(te.coeff) < kEps) return nullptr; // degenerate

            // Build new RHS = old_RHS / coeff
            NodePtr varNode = makeVariable(pool, var);
            NodePtr newRhs;
            double rhsNum = getNumericValue(eq.rhs);
            if (!std::isnan(rhsNum)) {
                // RHS is a known constant: evaluate directly
                newRhs = makeConstant(pool, rhsNum / te.coeff);
            } else if (isApprox(te.coeff, -1.0)) {
                // coeff == -1: new RHS = -old_RHS
                newRhs = makeNegation(pool, eq.rhs);
            } else {
                // General case: multiply RHS by 1/coeff
                newRhs = makeProduct(pool, {makeConstant(pool, 1.0 / te.coeff), eq.rhs});
            }
            return makeEquation(pool, varNode, newRhs);
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Public factory
// ─────────────────────────────────────────────────────────────────────────────

std::vector<RewriteRule> makeAlgebraicRules(CasMemoryPool& /*pool*/, char var) {
    std::vector<RewriteRule> rules;
    rules.reserve(10);

    // Phase 1 — Expansion & Cleanup (applied first)
    rules.push_back(makeDoubleNegationRule());
    rules.push_back(makeDistributivePropertyRule());
    rules.push_back(makePowerExpansionRule());

    // Phase 2 — Simplification
    rules.push_back(makeCombineConstantsRule());
    rules.push_back(makeConstantPowerEvalRule());
    rules.push_back(makeCombineLikeTermsRule());
    rules.push_back(makeIdentityEliminationRule());

    // Phase 3 — Transposition
    rules.push_back(makeMoveConstantsToRHSRule());
    rules.push_back(makeMoveVariablesToLHSRule(var));

    // Phase 4 — Reduction
    rules.push_back(makeDivideByCoefficientRule(var));

    return rules;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkNonLinearHandover — Degree detection & quadratic handover
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/// Returns the highest power of `var` that appears in `node`.
/// Returns 0 if the node is constant, 1 for linear, 2 for quadratic, etc.
static int maxDegree(const NodePtr& node, char var) {
    if (!node) return 0;
    switch (node->nodeType) {
        case NodeType::Constant:  return 0;
        case NodeType::Variable: {
            const auto* v = static_cast<const VariableNode*>(node.get());
            return (v->name.size() == 1 && v->name[0] == var) ? 1 : 0;
        }
        case NodeType::Negation: {
            const auto* n = static_cast<const NegationNode*>(node.get());
            return maxDegree(n->operand, var);
        }
        case NodeType::Sum: {
            const auto* s = static_cast<const SumNode*>(node.get());
            int d = 0;
            for (const auto& c : s->children) d = std::max(d, maxDegree(c, var));
            return d;
        }
        case NodeType::Product: {
            const auto* p = static_cast<const ProductNode*>(node.get());
            int d = 0;
            for (const auto& c : p->children) d += maxDegree(c, var);
            return d;
        }
        case NodeType::Power: {
            const auto* pw = static_cast<const PowerNode*>(node.get());
            // Only handle constant exponents
            if (pw->exponent && pw->exponent->isConstant()) {
                double exp = static_cast<const ConstantNode*>(pw->exponent.get())->value;
                return static_cast<int>(maxDegree(pw->base, var) * exp);
            }
            return maxDegree(pw->base, var);
        }
        case NodeType::Equation: {
            const auto* eq = static_cast<const EquationNode*>(node.get());
            return std::max(maxDegree(eq->lhs, var), maxDegree(eq->rhs, var));
        }
        default:
            return 0;
    }
}

/// True if the equation is in the "solved" form:  var = constant.
static bool isSolved(const NodePtr& tree, char var) {
    if (!tree || !tree->isEquation()) return false;
    const auto* eq = static_cast<const EquationNode*>(tree.get());
    // LHS must be exactly the variable
    if (!eq->lhs || !eq->lhs->isVariable()) return false;
    const auto* v = static_cast<const VariableNode*>(eq->lhs.get());
    return v->name.size() == 1 && v->name[0] == var
           && eq->rhs && !eq->rhs->containsVar(var);
}

} // anonymous namespace

void checkNonLinearHandover(RuleEngine::SolveResult& result, char var) {
    // Already solved — nothing to do.
    if (isSolved(result.finalTree, var)) return;

    if (!result.finalTree || !result.finalTree->isEquation()) return;
    const auto* eq = static_cast<const EquationNode*>(result.finalTree.get());

    int degree = maxDegree(eq->lhs, var);
    if (degree != 2) return;

    // Append a special handover step.
    RuleEngine::StepLog handover;
    handover.ruleName     = RULE_NONLINEAR_HANDOVER;
    handover.ruleDesc     = "Equation is quadratic. Transitioning to Quadratic Formula.";
    handover.phase        = RulePhase::Reduction;
    handover.tree         = result.finalTree;   // share the final tree
    handover.affectedNode = result.finalTree;   // highlight the whole equation

    result.steps.push_back(std::move(handover));
}
} // namespace cas
