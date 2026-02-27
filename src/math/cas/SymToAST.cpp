/**
 * SymToAST.cpp — Converts CAS-Lite symbolic types back to MathAST.
 *
 * Implements the reverse bridge: ExactVal / SymTerm / SymPoly / SymEquation
 * → MathAST node trees suitable for rendering via MathCanvas.
 *
 * Part of: NumOS CAS-Lite — Phase E (EquationsApp UI)
 */

#include "SymToAST.h"
#include <cstdio>      // snprintf
#include <cstdlib>     // abs / llabs
#include <string>

using namespace vpam;

namespace {

// ────────────────────────────────────────────────────────────────────
// Internal helpers — render absolute values (sign handled by callers)
// ────────────────────────────────────────────────────────────────────

/// Render |val| as a MathAST node (always non-negative).
NodePtr renderExactValAbs(const ExactVal& val) {
    if (!val.ok)     return makeNumber("ERR");
    if (val.isZero()) return makeNumber("0");

    int64_t absNum = (val.num < 0) ? -val.num : val.num;

    // ── Pure integer ────────────────────────────────────────────────
    if (val.isInteger()) {
        return makeNumber(std::to_string(absNum));
    }

    // ── Pure fraction ───────────────────────────────────────────────
    if (val.isRational()) {
        auto numRow = makeRow();
        static_cast<NodeRow*>(numRow.get())
            ->appendChild(makeNumber(std::to_string(absNum)));
        auto denRow = makeRow();
        static_cast<NodeRow*>(denRow.get())
            ->appendChild(makeNumber(std::to_string(val.den)));
        return makeFraction(std::move(numRow), std::move(denRow));
    }

    // ── Radical: (absNum/den) · outer · √inner ──────────────────────
    if (val.hasRadical()) {
        NodePtr radical = makeRoot(makeNumber(std::to_string(val.inner)));

        int64_t scalarNum = absNum * ((val.outer < 0) ? -val.outer : val.outer);
        int64_t scalarDen = val.den;

        if (scalarNum == 1 && scalarDen == 1) {
            return radical;                          // just √inner
        }
        auto row = makeRow();
        auto* r = static_cast<NodeRow*>(row.get());

        if (scalarDen == 1) {
            // integer · √inner
            r->appendChild(makeNumber(std::to_string(scalarNum)));
        } else {
            // fraction · √inner
            auto nRow = makeRow();
            static_cast<NodeRow*>(nRow.get())
                ->appendChild(makeNumber(std::to_string(scalarNum)));
            auto dRow = makeRow();
            static_cast<NodeRow*>(dRow.get())
                ->appendChild(makeNumber(std::to_string(scalarDen)));
            r->appendChild(makeFraction(std::move(nRow), std::move(dRow)));
        }
        r->appendChild(std::move(radical));
        return row;
    }

    // ── Pi ──────────────────────────────────────────────────────────
    if (val.hasPi()) {
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
        return row;
    }

    // ── e ───────────────────────────────────────────────────────────
    if (val.hasE()) {
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
        return row;
    }

    // ── Fallback → decimal string ───────────────────────────────────
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6g", val.toDouble());
    return makeNumber(buf);
}

/// Render |term| (positive coefficient, with variable and power).
NodePtr renderTermAbs(const cas::SymTerm& term) {
    if (term.isZero()) return makeNumber("0");

    // Constant term → just the absolute value
    if (term.isConstant()) {
        return renderExactValAbs(term.coeff);
    }

    ExactVal absCoeff = term.coeff;
    if (absCoeff.num < 0) absCoeff.num = -absCoeff.num;

    bool coeffIsOne = absCoeff.isInteger() && absCoeff.num == 1;

    // Build variable node (with power if > 1)
    NodePtr varNode;
    if (term.power == 1) {
        varNode = makeVariable(term.var);
    } else if (term.power > 1) {
        auto expRow = makeRow();
        static_cast<NodeRow*>(expRow.get())
            ->appendChild(makeNumber(std::to_string(term.power)));
        auto baseRow = makeRow();
        static_cast<NodeRow*>(baseRow.get())
            ->appendChild(makeVariable(term.var));
        varNode = makePower(std::move(baseRow), std::move(expRow));
    } else if (term.power == 0) {
        // x⁰ = 1 → just the coefficient
        return renderExactValAbs(absCoeff);
    } else {
        // Negative power: render as fraction  coeff / var^|p|
        NodePtr denomVar;
        if (term.power == -1) {
            denomVar = makeVariable(term.var);
        } else {
            auto expRow = makeRow();
            static_cast<NodeRow*>(expRow.get())
                ->appendChild(makeNumber(std::to_string(-term.power)));
            auto baseRow = makeRow();
            static_cast<NodeRow*>(baseRow.get())
                ->appendChild(makeVariable(term.var));
            denomVar = makePower(std::move(baseRow), std::move(expRow));
        }
        // numerator part
        auto numRow = makeRow();
        static_cast<NodeRow*>(numRow.get())
            ->appendChild(coeffIsOne ? makeNumber("1")
                                     : renderExactValAbs(absCoeff));
        auto denRow = makeRow();
        static_cast<NodeRow*>(denRow.get())
            ->appendChild(std::move(denomVar));
        return makeFraction(std::move(numRow), std::move(denRow));
    }

    // If coefficient is 1, skip it (just "x" instead of "1x")
    if (coeffIsOne) {
        return varNode;
    }

    // coefficient · variable
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());
    r->appendChild(renderExactValAbs(absCoeff));
    r->appendChild(std::move(varNode));
    return row;
}

/// Helper: append all children of srcRow into destRow (flattening).
/// If src is not a NodeRow, append it as-is.
void appendFlat(NodeRow* dest, NodePtr src) {
    if (src->type() == NodeType::Row) {
        auto* srcRow = static_cast<NodeRow*>(src.get());
        // Move children one by one
        while (srcRow->childCount() > 0) {
            dest->appendChild(srcRow->removeChild(0));
        }
    } else {
        dest->appendChild(std::move(src));
    }
}

} // anonymous namespace

namespace cas {

// ════════════════════════════════════════════════════════════════════
// fromExactVal — ExactVal → MathAST
// ════════════════════════════════════════════════════════════════════

NodePtr SymToAST::fromExactVal(const ExactVal& val) {
    if (!val.ok) return makeNumber("ERR");

    bool negative = (val.num < 0);

    if (negative) {
        auto row = makeRow();
        auto* r  = static_cast<NodeRow*>(row.get());
        r->appendChild(makeOperator(OpKind::Sub));
        r->appendChild(renderExactValAbs(val));
        return row;
    }
    return renderExactValAbs(val);
}

// ════════════════════════════════════════════════════════════════════
// fromSymTerm — SymTerm → MathAST
// ════════════════════════════════════════════════════════════════════

NodePtr SymToAST::fromSymTerm(const SymTerm& term) {
    if (term.isZero()) return makeNumber("0");

    bool negative = (term.coeff.num < 0);

    if (negative) {
        auto row = makeRow();
        auto* r  = static_cast<NodeRow*>(row.get());
        r->appendChild(makeOperator(OpKind::Sub));
        r->appendChild(renderTermAbs(term));
        return row;
    }
    return renderTermAbs(term);
}

// ════════════════════════════════════════════════════════════════════
// fromSymPoly — SymPoly → MathAST (NodeRow)
// ════════════════════════════════════════════════════════════════════

NodePtr SymToAST::fromSymPoly(const SymPoly& poly) {
    if (poly.isEmpty() || poly.isZero()) return makeNumber("0");

    auto row = makeRow();
    auto* r  = static_cast<NodeRow*>(row.get());
    bool first = true;

    for (const auto& t : poly.terms()) {
        if (t.isZero()) continue;

        bool neg = (t.coeff.num < 0);

        if (first) {
            // First term: include negative sign if needed
            if (neg) {
                r->appendChild(makeOperator(OpKind::Sub));
            }
            appendFlat(r, renderTermAbs(t));
            first = false;
        } else {
            // Subsequent terms: +/− operator then |term|
            r->appendChild(makeOperator(neg ? OpKind::Sub : OpKind::Add));
            appendFlat(r, renderTermAbs(t));
        }
    }

    if (first) return makeNumber("0");   // all terms were zero
    return row;
}

// ════════════════════════════════════════════════════════════════════
// fromSymEquation — SymEquation → MathAST (LHS = RHS as NodeRow)
// ════════════════════════════════════════════════════════════════════

NodePtr SymToAST::fromSymEquation(const SymEquation& eq) {
    auto row = makeRow();
    auto* r  = static_cast<NodeRow*>(row.get());

    // LHS
    appendFlat(r, fromSymPoly(eq.lhs));

    // = sign (rendered via NodeVariable('=') — display-only)
    r->appendChild(makeVariable('='));

    // RHS
    appendFlat(r, fromSymPoly(eq.rhs));

    return row;
}

} // namespace cas
