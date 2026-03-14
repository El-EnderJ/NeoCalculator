/**
 * CasToVpam.h — Converts PersistentAST (cas::NodePtr) to VPAM MathAST.
 *
 * Part of the "Smart Highlighter" bridge:  the TutorApp builds a VPAM tree
 * from a CAS equation snapshot and, optionally, records which VPAM node
 * corresponds to the CAS-engine's `affectedNode` so that MathCanvas can
 * render it in a highlight colour.
 *
 * Mapping
 * ───────
 *   cas::ConstantNode  → vpam::NodeNumber  (with sign handling)
 *   cas::VariableNode  → vpam::NodeVariable (single-char)
 *   cas::NegationNode  → NodeOperator(Sub) + child in a NodeRow
 *   cas::SumNode       → NodeRow with NodeOperator(Add/Sub) between terms
 *   cas::ProductNode   → NodeRow with implicit multiplication (no operator)
 *   cas::PowerNode     → NodePower
 *   cas::EquationNode  → NodeRow:  lhs  NodeVariable('=')  rhs
 *
 * Highlight tracking
 * ──────────────────
 * Call the two-argument overload of `convert()` and pass the CAS node whose
 * sub-tree should be highlighted.  The function does a single DFS pass; when
 * it encounters the exact pointer, it records the corresponding freshly-built
 * vpam::MathNode* in `*outHighlight`.  Pass that pointer to
 * `MathCanvas::setHighlightNode()`.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13C (TRS→Display Bridge)
 */

#pragma once

#include "PersistentAST.h"
#include "../MathAST.h"

namespace cas {

class CasToVpam {
public:
    /// Convert a CAS equation/expression tree to a VPAM NodePtr.
    static vpam::NodePtr convert(const NodePtr& node);

    /**
     * Convert with highlight tracking.
     * @param node         Root of the CAS tree to convert.
     * @param highlight    CAS node whose VPAM counterpart should be tracked.
     * @param outHighlight Receives a raw pointer to the matching VPAM node
     *                     (owned by the returned vpam::NodePtr tree).
     *                     Set to nullptr if @p highlight is not found.
     */
    static vpam::NodePtr convert(const NodePtr& node,
                                 const NodePtr& highlight,
                                 const vpam::MathNode** outHighlight);

private:
    // Internal state threaded through the recursive conversion to track highlight.
    struct Context {
        const AstNode*          highlightTarget = nullptr;  ///< CAS node to match (raw)
        const vpam::MathNode*   found           = nullptr;  ///< Matching VPAM node
    };

    static vpam::NodePtr convertNode(const NodePtr& node, Context* ctx);
    static vpam::NodePtr convertConstant(const ConstantNode* n);
    static vpam::NodePtr convertVariable(const VariableNode* v);
    static vpam::NodePtr convertNegation(const NegationNode* neg, Context* ctx);
    static vpam::NodePtr convertSum(const SumNode* s, Context* ctx);
    static vpam::NodePtr convertProduct(const ProductNode* p, Context* ctx);
    static vpam::NodePtr convertPower(const PowerNode* pw, Context* ctx);
    static vpam::NodePtr convertEquation(const EquationNode* eq, Context* ctx);

    /// Helper: if this is the highlight target, record the VPAM node pointer.
    static void trackHighlight(const AstNode* casNode,
                               const vpam::MathNode* vpamNode,
                               Context* ctx);
};

} // namespace cas
