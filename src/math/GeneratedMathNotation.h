// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MathAST.h"

namespace numos {
enum class ProductNotation : uint8_t { Explicit, ScalarNatural };

// Read-only, generated scalar mathematics only. Adjacent operands are the
// EXISTING VPAM implicit-product representation (serializeForGiac emits '*').
// No source string, factor order, grouping, or authoritative result is edited.
namespace generatednotation {
enum class Shape : uint8_t { Other, Number, Symbol, Group, Root, Function, Fraction };
struct Factor { Shape shape = Shape::Other; char symbol = 0; };
inline bool knownScalar(char c) {
    return c == 'a' || c == 'b' || c == 'c' || c == 'x' || c == 'y' || c == 'z' ||
           (c >= 'A' && c <= 'F');
}
inline const vpam::MathNode* unwrap(const vpam::MathNode* n, unsigned depth = 0) {
    while (n && n->type() == vpam::NodeType::Row && n->childCount() == 1 && depth++ < 32)
        n = n->child(0);
    return n;
}
// Reject unknown symbols, functions, units and non-scalar node kinds. The
// finite whitelist is a caller contract, not type inference from typography.
inline bool scalarWalk(const vpam::MathNode* n, bool& hasSymbol, unsigned& budget, unsigned depth = 0) {
    using namespace vpam;
    n = unwrap(n);
    if (!n || depth > 32 || !budget) return false;
    --budget;
    switch (n->type()) {
    case NodeType::Variable:
        hasSymbol = true; return knownScalar(static_cast<const NodeVariable*>(n)->name());
    case NodeType::Symbol: {
        const auto& s = static_cast<const NodeSymbol*>(n)->name();
        if (s == "-" || s == "+") return true;
        hasSymbol = true; return s.size() == 1 && knownScalar(s[0]);
    }
    case NodeType::Number: case NodeType::Constant: return true;
    case NodeType::Operator: {
        const auto op = static_cast<const NodeOperator*>(n)->op();
        return op == OpKind::Add || op == OpKind::Sub || op == OpKind::Mul || op == OpKind::Div;
    }
    case NodeType::Row: case NodeType::Paren: case NodeType::Power:
    case NodeType::Fraction: case NodeType::Root: case NodeType::Function:
        for (int i = 0; i < n->childCount(); ++i)
            if (!scalarWalk(n->child(i), hasSymbol, budget, depth + 1)) return false;
        return true;
    default: return false;
    }
}
inline bool scalar(const vpam::MathNode* n, bool& hasSymbol) {
    unsigned budget = 512;
    return scalarWalk(n, hasSymbol, budget);
}
inline Factor classify(const vpam::MathNode* n) {
    using namespace vpam;
    n = unwrap(n);
    if (!n) return {};
    if (n->type() == NodeType::Number) return {Shape::Number, 0};
    if (n->type() == NodeType::Variable) {
        char c = static_cast<const NodeVariable*>(n)->name();
        return knownScalar(c) ? Factor{Shape::Symbol, c} : Factor{};
    }
    if (n->type() == NodeType::Symbol) {
        const auto& s = static_cast<const NodeSymbol*>(n)->name();
        return s.size() == 1 && knownScalar(s[0]) ? Factor{Shape::Symbol, s[0]} : Factor{};
    }
    bool algebraic = false;
    if (!scalar(n, algebraic)) return {};
    if (n->type() == NodeType::Power) {
        auto base = unwrap(n->child(0));
        if (base && (base->type() == NodeType::Variable || base->type() == NodeType::Symbol))
            return classify(base);
        return {};
    }
    if (n->type() == NodeType::Paren && algebraic) {
        auto content = unwrap(n->child(0));
        // A signed right factor must stay visibly multiplied and grouped.
        if (content && content->childCount() &&
            (content->child(0)->type() == NodeType::Operator ||
             (content->child(0)->type() == NodeType::Symbol &&
              static_cast<const NodeSymbol*>(content->child(0))->name() == "-"))) return {};
        return {Shape::Group, 0};
    }
    if (n->type() == NodeType::Fraction) return {Shape::Fraction, 0};
    if (n->type() == NodeType::Root) return {Shape::Root, 0};
    if (n->type() == NodeType::Function) return {Shape::Function, 0};
    return {};
}
inline bool juxtapose(const vpam::MathNode* left, const vpam::MathNode* right) {
    const auto l = classify(left), r = classify(right);
    if (l.shape == Shape::Other || r.shape == Shape::Other ||
        r.shape == Shape::Number || r.shape == Shape::Fraction) return false;
    if (r.shape == Shape::Symbol)
        return l.shape != Shape::Function && (!l.symbol || l.symbol != r.symbol);
    if (r.shape == Shape::Group)
        return l.shape == Shape::Number || l.shape == Shape::Group;
    return r.shape == Shape::Root || r.shape == Shape::Function;
}
// Called during formula construction, never in layout/draw. The source-node
// budget bounds the traversal; exhaustion safely retains explicit operators.
inline void apply(vpam::MathNode* n, unsigned& budget, unsigned depth = 0) {
    using namespace vpam;
    if (!n || !budget || depth > 32) return;
    --budget;
    // Do not opt matrix/vector/collection/call semantics into scalar notation.
    switch (n->type()) {
    case NodeType::Row: case NodeType::Fraction: case NodeType::Power:
    case NodeType::Paren: case NodeType::Root: case NodeType::Function: break;
    default: return;
    }
    for (int i = 0; i < n->childCount(); ++i) apply(n->child(i), budget, depth + 1);
    if (n->type() != NodeType::Row) return;
    auto* row = static_cast<NodeRow*>(n);
    // A leading signed scalar coefficient needs no enclosing parentheses.
    // Preserve right-hand negatives and power-base parentheses. This moves
    // existing nodes, retaining the unary sign and multiplication semantics.
    if (row->childCount() >= 3 && row->child(0)->type() == NodeType::Paren &&
        row->child(1)->type() == NodeType::Operator &&
        static_cast<NodeOperator*>(row->child(1))->op() == OpKind::Mul) {
        auto* contents = row->child(0)->child(0);
        if (contents && contents->type() == NodeType::Row && contents->childCount() == 2 &&
            contents->child(0)->type() == NodeType::Operator &&
            static_cast<NodeOperator*>(contents->child(0))->op() == OpKind::Sub &&
            contents->child(1)->type() == NodeType::Number) {
            auto* inner = static_cast<NodeRow*>(contents);
            auto number = inner->removeChild(1), sign = inner->removeChild(0);
            row->replaceChild(0, std::move(sign));
            row->insertChild(1, std::move(number));
        }
    }
    for (int i = row->childCount() - 2; i > 0; --i) {
        const auto* op = row->child(i);
        if (op->type() != NodeType::Operator ||
            static_cast<const NodeOperator*>(op)->op() != OpKind::Mul ||
            !juxtapose(row->child(i - 1), row->child(i + 1))) continue;
        auto* right = const_cast<MathNode*>(unwrap(row->child(i + 1)));
        if (right->type() == NodeType::Function)
            static_cast<NodeFunction*>(right)->setGeneratedOperatorSpacing(true);
        row->removeChild(i); // semantic multiplication becomes VPAM adjacency
    }
}
} // namespace generatednotation
inline void applyGeneratedProductNotation(vpam::MathNode* root, ProductNotation policy) {
    if (policy == ProductNotation::ScalarNatural) {
        unsigned budget = 512;
        generatednotation::apply(root, budget);
    }
}
} // namespace numos
