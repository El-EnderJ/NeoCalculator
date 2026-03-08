/**
 * SymExprToAST.cpp — Converts SymExpr trees back to MathAST for rendering.
 *
 * Maps each SymExpr node type to the corresponding vpam::MathNode subtree.
 * Uses a structural box model where each node produces a properly sized
 * and baseline-aligned MathNode tree:
 *
 *   · Fractions center numerator/denominator based on the widest element
 *     and draw the vinculum on the math baseline.
 *   · Radicals dynamically scale the "V" hook and overline to match the
 *     radicand's height and width.
 *   · Powers correctly raise exponents and shrink their font metrics.
 *   · Nested structures (e.g. fraction inside an exponent) are handled
 *     recursively with proper alignment propagation.
 *
 * All layout computation is delegated to MathNode::calculateLayout(),
 * which recursively sizes each sub-tree using FontMetrics.
 *
 * Part of: NumOS Pro-CAS — Phase 3 (SymExpr → MathAST bridge)
 */

#include "SymExprToAST.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace vpam;

namespace {

/// Append all children of a NodeRow src into dest (flat merge).
/// If src is not a Row, append as-is.
void appendFlat(NodeRow* dest, NodePtr src) {
    if (!src) return;
    if (src->type() == NodeType::Row) {
        auto* srcRow = static_cast<NodeRow*>(src.get());
        while (srcRow->childCount() > 0)
            dest->appendChild(srcRow->removeChild(0));
    } else {
        dest->appendChild(std::move(src));
    }
}

} // anonymous namespace

namespace cas {

// ════════════════════════════════════════════════════════════════════
// renderExactVal — renders an ExactVal as MathAST (mirrors SymToAST)
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::renderExactVal(const ExactVal& val) {
    if (!val.ok)      return makeNumber("ERR");
    if (val.isZero()) return makeNumber("0");

    int64_t absNum = (val.num < 0) ? -val.num : val.num;
    bool negative  = (val.num < 0);

    // Helper: build the absolute part
    NodePtr absNode;

    if (val.isInteger()) {
        absNode = makeNumber(std::to_string(absNum));
    }
    else if (val.isRational()) {
        auto numRow = makeRow();
        static_cast<NodeRow*>(numRow.get())
            ->appendChild(makeNumber(std::to_string(absNum)));
        auto denRow = makeRow();
        static_cast<NodeRow*>(denRow.get())
            ->appendChild(makeNumber(std::to_string(val.den)));
        absNode = makeFraction(std::move(numRow), std::move(denRow));
    }
    else if (val.hasRadical()) {
        NodePtr radical = makeRoot(makeNumber(std::to_string(val.inner)));
        int64_t scalarNum = absNum * ((val.outer < 0) ? -val.outer : val.outer);
        int64_t scalarDen = val.den;

        if (scalarNum == 1 && scalarDen == 1) {
            absNode = std::move(radical);
        } else {
            auto row = makeRow();
            auto* r = static_cast<NodeRow*>(row.get());
            if (scalarDen == 1) {
                r->appendChild(makeNumber(std::to_string(scalarNum)));
            } else {
                auto nRow = makeRow();
                static_cast<NodeRow*>(nRow.get())
                    ->appendChild(makeNumber(std::to_string(scalarNum)));
                auto dRow = makeRow();
                static_cast<NodeRow*>(dRow.get())
                    ->appendChild(makeNumber(std::to_string(scalarDen)));
                r->appendChild(makeFraction(std::move(nRow), std::move(dRow)));
            }
            r->appendChild(std::move(radical));
            absNode = std::move(row);
        }
    }
    else if (val.hasPi()) {
        auto row = makeRow();
        auto* r = static_cast<NodeRow*>(row.get());
        if (!(absNum == 1 && val.den == 1)) {
            if (val.den == 1) {
                r->appendChild(makeNumber(std::to_string(absNum)));
            } else {
                auto nRow = makeRow();
                static_cast<NodeRow*>(nRow.get())
                    ->appendChild(makeNumber(std::to_string(absNum)));
                auto dRow = makeRow();
                static_cast<NodeRow*>(dRow.get())
                    ->appendChild(makeNumber(std::to_string(val.den)));
                r->appendChild(makeFraction(std::move(nRow), std::move(dRow)));
            }
        }
        r->appendChild(makeConstant(ConstKind::Pi));
        absNode = std::move(row);
    }
    else if (val.hasE()) {
        auto row = makeRow();
        auto* r = static_cast<NodeRow*>(row.get());
        if (!(absNum == 1 && val.den == 1)) {
            if (val.den == 1) {
                r->appendChild(makeNumber(std::to_string(absNum)));
            } else {
                auto nRow = makeRow();
                static_cast<NodeRow*>(nRow.get())
                    ->appendChild(makeNumber(std::to_string(absNum)));
                auto dRow = makeRow();
                static_cast<NodeRow*>(dRow.get())
                    ->appendChild(makeNumber(std::to_string(val.den)));
                r->appendChild(makeFraction(std::move(nRow), std::move(dRow)));
            }
        }
        r->appendChild(makeConstant(ConstKind::E));
        absNode = std::move(row);
    }
    else {
        // Fallback → decimal string
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6g", val.toDouble());
        absNode = makeNumber(buf);
    }

    // Add negative sign if needed
    if (negative) {
        auto row = makeRow();
        auto* r = static_cast<NodeRow*>(row.get());
        r->appendChild(makeOperator(OpKind::Sub));
        appendFlat(r, std::move(absNode));
        return row;
    }
    return absNode;
}

// ════════════════════════════════════════════════════════════════════
// ensureRow — wrap node in a Row if it isn't already one
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::ensureRow(NodePtr node) {
    if (!node) return makeRow();
    if (node->type() == NodeType::Row) return node;
    auto row = makeRow();
    static_cast<NodeRow*>(row.get())->appendChild(std::move(node));
    return row;
}

// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convert(const SymExpr* expr) {
    if (!expr) return makeNumber("?");

    switch (expr->type) {
        case SymExprType::Num:
            return convertNum(static_cast<const SymNum*>(expr));
        case SymExprType::Var:
            return convertVar(static_cast<const SymVar*>(expr));
        case SymExprType::Neg:
            return convertNeg(static_cast<const SymNeg*>(expr));
        case SymExprType::Add:
            return convertAdd(static_cast<const SymAdd*>(expr));
        case SymExprType::Mul:
            return convertMul(static_cast<const SymMul*>(expr));
        case SymExprType::Pow:
            return convertPow(static_cast<const SymPow*>(expr));
        case SymExprType::Func:
            return convertFunc(static_cast<const SymFunc*>(expr));
        case SymExprType::PlusMinus:
            return convertPlusMinus(static_cast<const SymPlusMinus*>(expr));
        case SymExprType::Subscript:
            return convertSubscript(static_cast<const SymSubscript*>(expr));
        default:
            return makeNumber("?");
    }
}

// ════════════════════════════════════════════════════════════════════
// convertIntegral — render antiderivative with " + C"
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertIntegral(const SymExpr* antiderivative) {
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());

    // Render the antiderivative
    NodePtr body = convert(antiderivative);
    appendFlat(r, std::move(body));

    // Append " + C"
    r->appendChild(makeOperator(OpKind::Add));
    r->appendChild(makeVariable('C'));

    return row;
}

// ════════════════════════════════════════════════════════════════════
// convertNum — SymNum → NodeNumber / NodeFraction / NodeConstant
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertNum(const SymNum* n) {
    return renderExactVal(n->toExactVal());
}

// ════════════════════════════════════════════════════════════════════
// convertVar — SymVar → NodeVariable
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertVar(const SymVar* v) {
    return makeVariable(v->name);
}

// ════════════════════════════════════════════════════════════════════
// convertNeg — SymNeg → "−" prefix + child
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertNeg(const SymNeg* n) {
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());
    r->appendChild(makeOperator(OpKind::Sub));

    NodePtr child = convert(n->child);

    // If child is an Add, wrap in parens: -(a+b) → −(a+b)
    if (n->child && n->child->type == SymExprType::Add) {
        r->appendChild(makeParen(ensureRow(std::move(child))));
    } else {
        appendFlat(r, std::move(child));
    }

    return row;
}

// ════════════════════════════════════════════════════════════════════
// convertAdd — SymAdd → NodeRow with + / − operators
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertAdd(const SymAdd* a) {
    if (a->count == 0) return makeNumber("0");
    if (a->count == 1) return convert(a->terms[0]);

    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());

    for (uint16_t i = 0; i < a->count; ++i) {
        const SymExpr* term = a->terms[i];

        // Check if this term is a SymNeg — render as " − child" instead of " + −child"
        if (i > 0 && term->type == SymExprType::Neg) {
            r->appendChild(makeOperator(OpKind::Sub));
            appendFlat(r, convert(static_cast<const SymNeg*>(term)->child));
        } else {
            if (i > 0) r->appendChild(makeOperator(OpKind::Add));
            appendFlat(r, convert(term));
        }
    }

    return row;
}

// ════════════════════════════════════════════════════════════════════
// convertMul — SymMul → NodeRow (implicit multiplication / fractions)
//
// Rendering strategy:
//   · Negative-power factors → denominator of a fraction
//   · Number factors → coefficient (rendered first)
//   · Other factors → juxtaposed (implicit ×)
//   · If there's exactly 1 numerator factor and it's numeric, and
//     there are denominator factors, render as fraction.
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertMul(const SymMul* m) {
    if (m->count == 0) return makeNumber("1");
    if (m->count == 1) return convert(m->factors[0]);

    // Separate numerator and denominator factors
    std::vector<const SymExpr*> numFactors;
    std::vector<const SymExpr*> denFactors;

    for (uint16_t i = 0; i < m->count; ++i) {
        const SymExpr* f = m->factors[i];
        // Check for factor^(-n) pattern → move to denominator
        if (f->type == SymExprType::Pow) {
            auto* pow = static_cast<const SymPow*>(f);
            if (pow->exponent->type == SymExprType::Num) {
                auto ev = static_cast<const SymNum*>(pow->exponent)->toExactVal();
                if (ev.isInteger() && ev.num < 0) {
                    // Move to denominator with positive exponent
                    denFactors.push_back(f);
                    continue;
                }
                if (ev.isRational() && ev.num < 0) {
                    denFactors.push_back(f);
                    continue;
                }
            }
        }
        numFactors.push_back(f);
    }

    // If we have denominator factors, render as fraction
    if (!denFactors.empty()) {
        // Build numerator
        NodePtr numNode;
        if (numFactors.empty()) {
            numNode = makeNumber("1");
        } else if (numFactors.size() == 1) {
            numNode = convert(numFactors[0]);
        } else {
            auto numRow = makeRow();
            auto* nr = static_cast<NodeRow*>(numRow.get());
            // Display order: coefficients (numbers) first
            for (auto* f : numFactors) {
                if (f->type == SymExprType::Num) appendFlat(nr, convert(f));
            }
            for (auto* f : numFactors) {
                if (f->type != SymExprType::Num) appendFlat(nr, convert(f));
            }
            numNode = std::move(numRow);
        }

        // Build denominator (negate the exponents)
        NodePtr denNode;
        if (denFactors.size() == 1) {
            auto* pow = static_cast<const SymPow*>(denFactors[0]);
            auto ev = static_cast<const SymNum*>(pow->exponent)->toExactVal();
            auto posExp = vpam::exactNeg(ev);
            if (posExp.isInteger() && posExp.num == 1) {
                // x^(-1) → just x in denominator
                denNode = convert(pow->base);
            } else {
                // x^(-n) → x^n in denominator
                denNode = convert(pow->base);
                auto expRow = ensureRow(renderExactVal(posExp));
                denNode = makePower(ensureRow(std::move(denNode)), std::move(expRow));
            }
        } else {
            auto denRow = makeRow();
            auto* dr = static_cast<NodeRow*>(denRow.get());
            for (auto* df : denFactors) {
                auto* pow = static_cast<const SymPow*>(df);
                auto ev = static_cast<const SymNum*>(pow->exponent)->toExactVal();
                auto posExp = vpam::exactNeg(ev);
                if (posExp.isInteger() && posExp.num == 1) {
                    appendFlat(dr, convert(pow->base));
                } else {
                    auto baseNode = convert(pow->base);
                    auto expRow = ensureRow(renderExactVal(posExp));
                    dr->appendChild(makePower(
                        ensureRow(std::move(baseNode)), std::move(expRow)));
                }
            }
            denNode = std::move(denRow);
        }

        return makeFraction(
            ensureRow(std::move(numNode)),
            ensureRow(std::move(denNode)));
    }

    // No denominator → simple juxtaposition (coefficients first for display)
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());
    // Render numeric coefficients first, then non-numeric factors
    for (uint16_t i = 0; i < m->count; ++i) {
        if (m->factors[i]->type == SymExprType::Num)
            appendFlat(r, convert(m->factors[i]));
    }
    for (uint16_t i = 0; i < m->count; ++i) {
        if (m->factors[i]->type != SymExprType::Num)
            appendFlat(r, convert(m->factors[i]));
    }
    return row;
}

// ════════════════════════════════════════════════════════════════════
// convertPow — SymPow → NodePower or NodeRoot
//
// Special case: exponent = 1/2 → √(base)
//              exponent = 1/n → ⁿ√(base)
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertPow(const SymPow* p) {
    // Check for root form: base^(1/n)
    if (p->exponent->type == SymExprType::Num) {
        auto ev = static_cast<const SymNum*>(p->exponent)->toExactVal();
        if (ev.isRational() && ev.num == 1 && ev.den > 1
            && ev.inner == 1 && ev.piMul == 0 && ev.eMul == 0)
        {
            NodePtr radicand = ensureRow(convert(p->base));
            if (ev.den == 2) {
                return makeRoot(std::move(radicand));  // √base
            } else {
                auto degRow = makeRow();
                static_cast<NodeRow*>(degRow.get())
                    ->appendChild(makeNumber(std::to_string(ev.den)));
                return makeRoot(std::move(radicand), std::move(degRow));
            }
        }
    }

    // Normal power — structural box model:
    // The base is wrapped in a Row to establish a self-contained layout box.
    // The exponent is wrapped similarly; the Power node raises it by
    // EXP_RAISE (3/5 of base ascent) and applies superscript font scaling.
    // For compound bases (Add, Mul with >1 factor), wrap in parentheses
    // to maintain mathematical clarity at any nesting depth.
    NodePtr baseNode;
    if (p->base->type == SymExprType::Add ||
        (p->base->type == SymExprType::Mul &&
         static_cast<const SymMul*>(p->base)->count > 1)) {
        baseNode = makeParen(ensureRow(convert(p->base)));
    } else {
        baseNode = ensureRow(convert(p->base));
    }
    NodePtr expNode  = ensureRow(convert(p->exponent));
    return makePower(std::move(baseNode), std::move(expNode));
}

// ════════════════════════════════════════════════════════════════════
// convertFunc — SymFunc → NodeFunction / NodeLogBase
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertFunc(const SymFunc* f) {
    NodePtr argNode = ensureRow(convert(f->argument));

    switch (f->kind) {
        case SymFuncKind::Sin:
            return makeFunction(FuncKind::Sin, std::move(argNode));
        case SymFuncKind::Cos:
            return makeFunction(FuncKind::Cos, std::move(argNode));
        case SymFuncKind::Tan:
            return makeFunction(FuncKind::Tan, std::move(argNode));
        case SymFuncKind::ArcSin:
            return makeFunction(FuncKind::ArcSin, std::move(argNode));
        case SymFuncKind::ArcCos:
            return makeFunction(FuncKind::ArcCos, std::move(argNode));
        case SymFuncKind::ArcTan:
            return makeFunction(FuncKind::ArcTan, std::move(argNode));
        case SymFuncKind::Ln:
            return makeFunction(FuncKind::Ln, std::move(argNode));
        case SymFuncKind::Exp: {
            // exp(u) → e^u (NodePower with e base)
            auto baseRow = makeRow();
            static_cast<NodeRow*>(baseRow.get())
                ->appendChild(makeConstant(ConstKind::E));
            return makePower(std::move(baseRow), std::move(argNode));
        }
        case SymFuncKind::Log10:
            return makeFunction(FuncKind::Log, std::move(argNode));
        case SymFuncKind::Abs: {
            // |u| → rendered with pipe delimiters wrapping the argument
            // in a parenthesized row for correct baseline alignment.
            auto row = makeRow();
            auto* r = static_cast<NodeRow*>(row.get());
            r->appendChild(makeNumber("|"));
            r->appendChild(makeParen(std::move(argNode)));
            r->appendChild(makeNumber("|"));
            return row;
        }
        case SymFuncKind::Integral: {
            // Integral f(x) dx — safe ASCII fallback (Montserrat lacks ∫)
            auto row = makeRow();
            auto* r = static_cast<NodeRow*>(row.get());
            r->appendChild(makeNumber("S"));       // safe integral symbol
            r->appendChild(makeNumber("("));
            appendFlat(r, std::move(argNode));
            r->appendChild(makeNumber(")"));
            r->appendChild(makeNumber("dx"));
            return row;
        }
        default:
            return makeNumber("?");
    }
}

// ════════════════════════════════════════════════════════════════════
// convertPlusMinus — SymPlusMinus → NodeRow with ± operator
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertPlusMinus(const SymPlusMinus* pm) {
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());
    
    // We don't prepend "0 ±" if LHS is empty. We assume the tutor builds it cleanly.
    if (pm->lhs) {
        appendFlat(r, convert(pm->lhs));
    }
    
    r->appendChild(makeOperator(OpKind::PlusMinus));
    
    // If rhs is negative, parens are usually needed, but tutor logic should handle it.
    if (pm->rhs) {
        appendFlat(r, convert(pm->rhs));
    }
    
    return row;
}

// ════════════════════════════════════════════════════════════════════
// convertSubscript — SymSubscript → NodeSubscript
// ════════════════════════════════════════════════════════════════════

NodePtr SymExprToAST::convertSubscript(const SymSubscript* sub) {
    // Both base and subscript need to be single nodes OR wrapped in Row
    NodePtr baseNode = ensureRow(convert(sub->base));
    NodePtr subNode  = ensureRow(convert(sub->subscript));
    return makeSubscript(std::move(baseNode), std::move(subNode));
}

} // namespace cas
