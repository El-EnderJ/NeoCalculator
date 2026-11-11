// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MathAST.h"

namespace vpam {

struct AtomBoundary {
    MathClass left;
    MathClass right;
};

// Static presentation boundaries; only Rows are transparent lists. A compound
// slot (especially an exponent) NEVER supplies context to its parent's list.
inline AtomBoundary atomBoundary(const MathNode* node) {
    for (;;) {
        if (node->type() == NodeType::Power) {
            node = static_cast<const NodePower*>(node)->base();
        } else if (node->type() == NodeType::Subscript) {
            node = static_cast<const NodeSubscript*>(node)->base();
        } else if (node->type() == NodeType::Row) {
            // A multi-atom script nucleus is boxed, not a spliced outer list.
            if (node->childCount() != 1) return {MathClass::ORD, MathClass::ORD};
            node = node->child(0);
        } else if (node->type() == NodeType::Paren) {
            // WHY: the automatically sized, visible group is TeX INNER. Its
            // internal signs cannot turn an operator in the outer row unary.
            return {MathClass::INNER, MathClass::INNER};
        } else if (node->type() == NodeType::Function || node->type() == NodeType::LogBase) {
            if (node->type() == NodeType::Function &&
                static_cast<const NodeFunction*>(node)->funcKind() == FuncKind::Factorial)
                return {MathClass::ORD, MathClass::ORD};
            // Function label faces left as OP; its owned argument completes an
            // operand on the right. Shared for authored and generated formulas.
            return {MathClass::OP, MathClass::ORD};
        } else {
            return {node->leftMathClass(), node->rightMathClass()};
        }
    }
}

// Three linear passes, O(1) state per existing row nesting level. No row-sized
// scratch buffer, heap allocation, neighbour searches, or 64-element cutoff.
// Private friendship permits changing only cached layout, never the operator
// kind, children, ownership, or serialization.
struct RowLayout {
    static void measure(NodeRow& row, const FontMetrics& fm) {
        row.setScriptLevel(fm.scriptLevel);
        for (const auto& child : row.children()) {
            if (child->type() == NodeType::Row)
                measure(static_cast<NodeRow&>(*child), fm);
            else child->calculateLayout(fm);
        }
    }

    static bool demotesFollowingBinary(MathClass previous) {
        return previous == MathClass::BINARY || previous == MathClass::OP ||
               previous == MathClass::REL || previous == MathClass::OPEN ||
               previous == MathClass::PUNCT;
    }
    static bool demotesPreviousBinary(MathClass next) {
        return next == MathClass::REL || next == MathClass::CLOSE || next == MathClass::PUNCT;
    }
    static void ordinary(MathNode& node) {
        node._layout.effectiveLeft = node._layout.effectiveRight = MathClass::ORD;
    }
    static void classify(NodeRow& row, MathNode*& previous) {
        for (const auto& child : row.children()) {
            if (child->type() == NodeType::Row) {
                classify(static_cast<NodeRow&>(*child), previous);
                continue;
            }
            auto& l = child->_layout;
            const auto boundary = atomBoundary(child.get());
            l.effectiveLeft = boundary.left;
            l.effectiveRight = boundary.right;
            if (child->type() == NodeType::Empty) continue;
            // WHY: exactly TeX's ordered first pass. In a+-=, '-' is demoted
            // by '+' before '=' is seen; a look-ahead shortcut changes this.
            if (l.effectiveLeft == MathClass::BINARY &&
                (!previous || demotesFollowingBinary(previous->_layout.effectiveRight)))
                ordinary(*child);
            else if (demotesPreviousBinary(l.effectiveLeft) && previous &&
                     previous->_layout.effectiveRight == MathClass::BINARY)
                ordinary(*previous);
            previous = child.get();
        }
    }
    static void place(NodeRow& row, const FontMetrics& fm, const MathNode*& previous) {
        auto& l = row._layout;
        l = {};
        if (row.isEmpty()) {
            l.ascent = l.inkAscent = fm.ascent;
            l.descent = l.inkDescent = fm.descent;
            return;
        }
        bool first = true;
        bool firstAtom = true;
        int32_t width = 0;
        for (const auto& child : row.children()) {
            const auto* prior = previous;
            auto& cl = child->_layout;
            if (child->type() == NodeType::Row) {
                place(static_cast<NodeRow&>(*child), fm, previous);
            } else {
                cl.spaceBefore = 0;
                if (child->type() != NodeType::Empty) {
                    if (previous) cl.spaceBefore = interAtomSpacingPx(
                        previous->layout().effectiveRight, cl.effectiveLeft, fm.style, fm.emSize);
                    previous = child.get();
                }
            }
            // The first incoming gap belongs to the containing row. It is
            // factored out of a transparent Row exactly once, even when nested.
            if (first) l.spaceBefore = cl.spaceBefore;
            else width += cl.spaceBefore;
            cl.rowX = fm.coordinate(width);
            width += cl.width;
            l.ascent = std::max(l.ascent, cl.ascent);
            l.descent = std::max(l.descent, cl.descent);
            l.inkAscent = std::max(l.inkAscent, layoutInkAscentPx(cl));
            l.inkDescent = std::max(l.inkDescent, layoutInkDescentPx(cl));
            if (previous != prior) {
                if (firstAtom) l.effectiveLeft = cl.effectiveLeft;
                l.effectiveRight = cl.effectiveRight;
                firstAtom = false;
            }
            first = false;
        }
        l.width = fm.coordinate(width);
    }
    static void calculate(NodeRow& row, const FontMetrics& fm) {
        measure(row, fm);
        MathNode* previous = nullptr;
        classify(row, previous);
        if (previous && previous->_layout.effectiveRight == MathClass::BINARY) ordinary(*previous);
        const MathNode* placedPrevious = nullptr;
        place(row, fm, placedPrevious);
    }
};

} // namespace vpam
