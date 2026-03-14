/**
 * CasToVpam.cpp — PersistentAST → VPAM MathAST converter.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13C (TRS→Display Bridge)
 */

#include "CasToVpam.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace cas {

// Helper: explicit aliases for vpam factories to avoid name clash with
// the same-named cas factories in PersistentAST.h
static vpam::NodePtr vRow()          { return vpam::makeRow(); }
static vpam::NodePtr vNumber(const std::string& s) { return vpam::makeNumber(s); }
static vpam::NodePtr vOp(vpam::OpKind k)           { return vpam::makeOperator(k); }
static vpam::NodePtr vVar(char c)    { return vpam::makeVariable(c); }
static vpam::NodePtr vPow(vpam::NodePtr b, vpam::NodePtr e)
    { return vpam::makePower(std::move(b), std::move(e)); }

/// Values whose absolute value is below this threshold are rendered as
/// integers (no decimal point) to keep the display clean.
static constexpr double MAX_INTEGER_DISPLAY = 1e9;

// ─────────────────────────────────────────────────────────────────────────────
// Public entry points
// ─────────────────────────────────────────────────────────────────────────────

vpam::NodePtr CasToVpam::convert(const NodePtr& node) {
    return convertNode(node, nullptr);
}

vpam::NodePtr CasToVpam::convert(const NodePtr& node,
                                  const NodePtr& highlight,
                                  const vpam::MathNode** outHighlight) {
    Context ctx;
    ctx.highlightTarget = highlight ? highlight.get() : nullptr;
    ctx.found           = nullptr;

    vpam::NodePtr result = convertNode(node, &ctx);

    if (outHighlight) *outHighlight = ctx.found;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

void CasToVpam::trackHighlight(const AstNode* casNode,
                                const vpam::MathNode* vpamNode,
                                Context* ctx) {
    if (!ctx || ctx->found) return;
    if (casNode == ctx->highlightTarget) ctx->found = vpamNode;
}

vpam::NodePtr CasToVpam::convertNode(const NodePtr& node, Context* ctx) {
    if (!node) return vRow();

    switch (node->nodeType) {
        case NodeType::Constant:
            return convertConstant(static_cast<const ConstantNode*>(node.get()));
        case NodeType::Variable:
            return convertVariable(static_cast<const VariableNode*>(node.get()));
        case NodeType::Negation:
            return convertNegation(static_cast<const NegationNode*>(node.get()), ctx);
        case NodeType::Sum:
            return convertSum(static_cast<const SumNode*>(node.get()), ctx);
        case NodeType::Product:
            return convertProduct(static_cast<const ProductNode*>(node.get()), ctx);
        case NodeType::Power:
            return convertPower(static_cast<const PowerNode*>(node.get()), ctx);
        case NodeType::Equation:
            return convertEquation(static_cast<const EquationNode*>(node.get()), ctx);
        default:
            return vRow();
    }
}

// ── ConstantNode → NodeNumber or NodeFraction ─────────────────────────────────

vpam::NodePtr CasToVpam::convertConstant(const ConstantNode* n) {
    // Exact rational fraction: render as a vertical fraction bar
    if (n->isFraction() && n->denominator > 1) {
        char numBuf[24], denBuf[24];
        snprintf(numBuf, sizeof(numBuf), "%d", n->numerator);
        snprintf(denBuf, sizeof(denBuf), "%d", n->denominator);
        auto numRow = vRow();
        static_cast<vpam::NodeRow*>(numRow.get())->appendChild(vNumber(numBuf));
        auto denRow = vRow();
        static_cast<vpam::NodeRow*>(denRow.get())->appendChild(vNumber(denBuf));
        return vpam::makeFraction(std::move(numRow), std::move(denRow));
    }
    // Integer or fallback double
    char buf[32];
    double v = n->value;
    if (v == std::floor(v) && std::fabs(v) < MAX_INTEGER_DISPLAY) {
        snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    } else {
        snprintf(buf, sizeof(buf), "%g", v);
    }
    return vNumber(buf);
}

// ── VariableNode → NodeVariable ───────────────────────────────────────────────

vpam::NodePtr CasToVpam::convertVariable(const VariableNode* v) {
    char ch = v->name.empty() ? '?' : v->name[0];
    return vVar(ch);
}

// ── NegationNode → NodeRow(-child) ────────────────────────────────────────────

vpam::NodePtr CasToVpam::convertNegation(const NegationNode* neg, Context* ctx) {
    auto row  = vRow();
    auto* r   = static_cast<vpam::NodeRow*>(row.get());
    trackHighlight(neg, r, ctx);

    r->appendChild(vOp(vpam::OpKind::Sub));
    r->appendChild(convertNode(neg->operand, ctx));
    return row;
}

// ── SumNode → NodeRow(a [+/-] b …) ───────────────────────────────────────────

vpam::NodePtr CasToVpam::convertSum(const SumNode* s, Context* ctx) {
    auto row = vRow();
    auto* r  = static_cast<vpam::NodeRow*>(row.get());
    trackHighlight(s, r, ctx);

    for (std::size_t i = 0; i < s->children.size(); ++i) {
        const auto& child = s->children[i];
        if (i == 0) {
            r->appendChild(convertNode(child, ctx));
        } else {
            if (child->isNegation()) {
                const auto* neg = static_cast<const NegationNode*>(child.get());
                r->appendChild(vOp(vpam::OpKind::Sub));
                r->appendChild(convertNode(neg->operand, ctx));
            } else {
                r->appendChild(vOp(vpam::OpKind::Add));
                r->appendChild(convertNode(child, ctx));
            }
        }
    }
    return row;
}

// ── ProductNode → NodeRow(a b …) ──────────────────────────────────────────────

vpam::NodePtr CasToVpam::convertProduct(const ProductNode* p, Context* ctx) {
    auto row = vRow();
    auto* r  = static_cast<vpam::NodeRow*>(row.get());
    trackHighlight(p, r, ctx);

    for (std::size_t i = 0; i < p->children.size(); ++i) {
        if (i > 0) {
            const auto& prev = p->children[i - 1];
            const auto& curr = p->children[i];
            if (prev->isConstant() && curr->isConstant()) {
                r->appendChild(vOp(vpam::OpKind::Mul));
            }
        }
        r->appendChild(convertNode(p->children[i], ctx));
    }
    return row;
}

// ── PowerNode → NodePower ──────────────────────────────────────────────────────

vpam::NodePtr CasToVpam::convertPower(const PowerNode* pw, Context* ctx) {
    auto base = convertNode(pw->base, ctx);
    auto exp  = convertNode(pw->exponent, ctx);
    auto pwr  = vPow(std::move(base), std::move(exp));
    trackHighlight(pw, pwr.get(), ctx);
    return pwr;
}

// ── EquationNode → NodeRow(lhs = rhs) ─────────────────────────────────────────

vpam::NodePtr CasToVpam::convertEquation(const EquationNode* eq, Context* ctx) {
    auto row = vRow();
    auto* r  = static_cast<vpam::NodeRow*>(row.get());
    trackHighlight(eq, r, ctx);

    r->appendChild(convertNode(eq->lhs, ctx));
    r->appendChild(vVar('='));
    r->appendChild(convertNode(eq->rhs, ctx));
    return row;
}

} // namespace cas
