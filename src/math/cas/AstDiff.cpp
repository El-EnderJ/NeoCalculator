/**
 * AstDiff.cpp — Implementation of the AST sub-tree diffing engine.
 *
 * Uses pointer identity (NodePtr comparison) to skip unchanged branches in O(1),
 * then descends into children to pinpoint the smallest changed sub-tree.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13B (Algebraic Brain & Smart Diffing)
 */

#include "AstDiff.h"

namespace cas {

NodePtr findChangedNodes(const NodePtr& oldNode, const NodePtr& newNode) {
    // ── Identical pointer: this entire sub-tree is unchanged ─────────────────
    if (oldNode.get() == newNode.get()) return nullptr;

    // ── One side is null: the whole new node is the change ───────────────────
    if (!oldNode || !newNode) return newNode;

    // ── Different node types: the whole new node was replaced ────────────────
    if (oldNode->nodeType != newNode->nodeType) return newNode;

    // ── Same type: descend into children to find the precise changed sub-tree ─

    switch (newNode->nodeType) {

        case NodeType::Negation: {
            const auto& oNeg = static_cast<const NegationNode&>(*oldNode);
            const auto& nNeg = static_cast<const NegationNode&>(*newNode);
            NodePtr inner = findChangedNodes(oNeg.operand, nNeg.operand);
            // If the operand changed, return the pinpointed sub-change;
            // otherwise return newNode (the negation wrapper itself changed).
            return inner ? inner : newNode;
        }

        case NodeType::Sum: {
            const auto& oSum = static_cast<const SumNode&>(*oldNode);
            const auto& nSum = static_cast<const SumNode&>(*newNode);
            // Only descend if arity is the same — structural changes return root.
            if (oSum.children.size() == nSum.children.size()) {
                NodePtr changed;
                int     changedCount = 0;
                for (std::size_t i = 0; i < nSum.children.size(); ++i) {
                    NodePtr diff = findChangedNodes(oSum.children[i], nSum.children[i]);
                    if (diff) {
                        changed = diff;
                        ++changedCount;
                    }
                }
                // Only narrow down when exactly one child changed.
                if (changedCount == 1) return changed;
            }
            return newNode;
        }

        case NodeType::Product: {
            const auto& oProd = static_cast<const ProductNode&>(*oldNode);
            const auto& nProd = static_cast<const ProductNode&>(*newNode);
            if (oProd.children.size() == nProd.children.size()) {
                NodePtr changed;
                int     changedCount = 0;
                for (std::size_t i = 0; i < nProd.children.size(); ++i) {
                    NodePtr diff = findChangedNodes(oProd.children[i], nProd.children[i]);
                    if (diff) {
                        changed = diff;
                        ++changedCount;
                    }
                }
                if (changedCount == 1) return changed;
            }
            return newNode;
        }

        case NodeType::Power: {
            const auto& oPow = static_cast<const PowerNode&>(*oldNode);
            const auto& nPow = static_cast<const PowerNode&>(*newNode);
            NodePtr baseDiff = findChangedNodes(oPow.base,     nPow.base);
            NodePtr expDiff  = findChangedNodes(oPow.exponent, nPow.exponent);
            if (baseDiff && !expDiff) return baseDiff;
            if (expDiff  && !baseDiff) return expDiff;
            return newNode;
        }

        case NodeType::Equation: {
            const auto& oEq = static_cast<const EquationNode&>(*oldNode);
            const auto& nEq = static_cast<const EquationNode&>(*newNode);
            NodePtr lhsDiff = findChangedNodes(oEq.lhs, nEq.lhs);
            NodePtr rhsDiff = findChangedNodes(oEq.rhs, nEq.rhs);
            if (lhsDiff && !rhsDiff) return lhsDiff;
            if (rhsDiff && !lhsDiff) return rhsDiff;
            return newNode;
        }

        default:
            // Leaf nodes (Constant, Variable) with differing values: return newNode.
            return newNode;
    }
}

} // namespace cas
