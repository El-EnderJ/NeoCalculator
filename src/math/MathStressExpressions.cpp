/*
 * NumOS - Phase 5 math render stress expressions.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MathStressExpressions.h"

#include <utility>

namespace vpam {
namespace {

template <typename... Nodes>
NodePtr row(Nodes&&... nodes) {
    auto out = makeRow();
    auto* r = static_cast<NodeRow*>(out.get());
    (r->appendChild(std::forward<Nodes>(nodes)), ...);
    return out;
}

NodePtr n(const char* value) {
    return makeNumber(value);
}

NodePtr v(char name) {
    return makeVariable(name);
}

NodePtr op(OpKind kind) {
    return makeOperator(kind);
}

NodePtr rel(OpKind kind) {
    return makeRelation(kind);
}

NodePtr pow(NodePtr base, NodePtr exponent) {
    return makePower(std::move(base), std::move(exponent));
}

NodePtr sub(NodePtr base, NodePtr subscript) {
    return makeSubscript(std::move(base), std::move(subscript));
}

NodePtr eps0() {
    return sub(row(v('e')), row(n("0")));
}

NodePtr xSquared() {
    return pow(row(v('x')), row(n("2")));
}

NodePtr monomial4ac() {
    return row(n("4"), v('a'), v('c'));
}

NodePtr bSquaredMinus4ac() {
    return row(
        pow(row(v('b')), row(n("2"))),
        op(OpKind::Sub),
        monomial4ac());
}

NodePtr fraction(NodePtr numerator, NodePtr denominator) {
    return makeFraction(std::move(numerator), std::move(denominator));
}

NodePtr quadraticFormula() {
    return row(
        v('x'),
        rel(OpKind::Eq),
        fraction(
            row(n("-"), v('b'), op(OpKind::PlusMinus),
                makeRoot(bSquaredMinus4ac())),
            row(n("2"), v('a'))));
}

NodePtr gaussLawIntegralBody() {
    return fraction(
        row(v('p'), pow(row(v('r')), row(n("2")))),
        row(eps0()));
}

NodePtr gaussLawDisplay() {
    return row(
        makeDefIntegral(row(n("0")), row(v('R')),
                        gaussLawIntegralBody(), row(v('r'))),
        rel(OpKind::Eq),
        fraction(row(v('Q')), row(eps0())));
}

NodePtr gaussLawText() {
    return gaussLawDisplay();
}

NodePtr deepNestedExponent() {
    return row(
        pow(row(v('x')),
            row(pow(row(v('e')),
                    row(pow(row(v('x')), row(n("2"))))))));
}

NodePtr elasticMatrixBrackets() {
    auto eq1 = row(v('a'), v('x'), op(OpKind::Add), v('b'), v('y'),
                   rel(OpKind::Eq), v('c'));
    auto eq2 = row(v('d'), v('x'), op(OpKind::Sub), v('e'), v('y'),
                   rel(OpKind::Eq), v('f'));
    auto eq3 = row(v('g'), v('x'), op(OpKind::Add), v('h'), v('y'),
                   rel(OpKind::Eq), v('i'));

    auto stackedRows = fraction(
        std::move(eq1),
        fraction(std::move(eq2), std::move(eq3)));

    return row(makeBracket(row(makeBrace(std::move(stackedRows)))));
}

NodePtr nestedRadicals() {
    return row(
        makeRoot(row(
            n("1"), op(OpKind::Add),
            makeRoot(row(
                xSquared(), op(OpKind::Add),
                makeRoot(row(v('y'))))))));
}

NodePtr cauchyIntegralDisplay() {
    return row(
        makeDefIntegral(row(n("0")), row(n("2"), v('p')),
                        fraction(row(v('f'), makeParen(row(v('z')))),
                                 row(v('z'), op(OpKind::Sub), v('a'))),
                        row(v('z'))),
        rel(OpKind::Eq),
        row(n("2"), v('p'), v('i'), v('f'), makeParen(row(v('a')))));
}

NodePtr cauchyIntegralText() {
    return cauchyIntegralDisplay();
}

NodePtr fourLevelFraction() {
    return row(
        fraction(
            row(n("1"), op(OpKind::Add),
                fraction(row(n("1")), row(n("2"), op(OpKind::Add), xSquared()))),
            row(n("3"), op(OpKind::Sub),
                fraction(row(v('a'), op(OpKind::Add), v('b')),
                         row(v('c'), op(OpKind::Sub), v('d'))))));
}

NodePtr summationDisplay() {
    return row(
        makeSummation(row(v('n'), rel(OpKind::Eq), n("1")),
                      row(v('N')),
                      fraction(row(pow(row(v('n')), row(n("2"))), op(OpKind::Add), n("1")),
                               row(n("2"), v('n'), op(OpKind::Add), n("1")))));
}

NodePtr summationText() {
    return summationDisplay();
}

NodePtr productDisplay() {
    return row(
        makeBigOp(BigOpKind::Product,
                  row(v('k'), rel(OpKind::Eq), n("1")),
                  row(v('n')),
                  fraction(row(v('k'), op(OpKind::Add), n("1")),
                           row(v('k')))));
}

NodePtr unionText() {
    return row(
        makeBigOp(BigOpKind::Union,
                  row(v('i'), rel(OpKind::Eq), n("1")),
                  row(v('m')),
                  sub(row(v('A')), row(v('i')))));
}

NodePtr logTrigComposite() {
    return row(
        makeFunction(FuncKind::Sin,
            row(makeLogBase(row(n("2")),
                            row(fraction(row(n("1"), op(OpKind::Add), v('x')),
                                         row(n("1"), op(OpKind::Sub), v('x'))))))),
        op(OpKind::Add),
        makeFunction(FuncKind::Cos, row(pow(row(v('x')), row(n("3"))))));
}

NodePtr determinantBars() {
    return row(
        makeBar(row(
            fraction(row(v('a'), op(OpKind::Add), v('b')),
                     row(v('c'), op(OpKind::Sub), v('d'))),
            op(OpKind::Sub),
            fraction(row(v('e'), op(OpKind::Add), v('f')),
                     row(v('g'), op(OpKind::Sub), v('h'))))));
}

NodePtr braceSystemOnly() {
    return row(
        makeBrace(row(
            fraction(
                row(xSquared(), op(OpKind::Add), pow(row(v('y')), row(n("2"))),
                    rel(OpKind::Eq), n("1")),
                row(v('x'), op(OpKind::Sub), v('y'), rel(OpKind::Eq), n("0"))))));
}

NodePtr bracketedBinomialStack() {
    return row(
        makeBracket(row(
            fraction(row(v('n')),
                     row(v('k'), makeParen(row(v('n'), op(OpKind::Sub), v('k'))))))),
        rel(OpKind::Eq),
        fraction(row(v('n'), v('!')),
                 row(v('k'), v('!'),
                     makeParen(row(v('n'), op(OpKind::Sub), v('k'))), v('!'))));
}

constexpr StressExpressionCase kStressCases[] = {
    {"quadratic_formula", "Quadratic formula", MathStyle::DISPLAY_STYLE, quadraticFormula},
    {"gauss_law_display", "Gauss law display", MathStyle::DISPLAY_STYLE, gaussLawDisplay},
    {"gauss_law_text", "Gauss law text", MathStyle::TEXT, gaussLawText},
    {"deep_nested_exponent", "Deep nested exponent", MathStyle::DISPLAY_STYLE, deepNestedExponent},
    {"elastic_matrix_brackets", "Elastic bracket/brace matrix", MathStyle::DISPLAY_STYLE, elasticMatrixBrackets},
    {"nested_radicals", "Nested radicals", MathStyle::DISPLAY_STYLE, nestedRadicals},
    {"cauchy_integral_display", "Cauchy integral display", MathStyle::DISPLAY_STYLE, cauchyIntegralDisplay},
    {"cauchy_integral_text", "Cauchy integral text", MathStyle::TEXT, cauchyIntegralText},
    {"four_level_fraction", "Four-level fraction", MathStyle::DISPLAY_STYLE, fourLevelFraction},
    {"summation_display", "Summation display", MathStyle::DISPLAY_STYLE, summationDisplay},
    {"summation_text", "Summation text", MathStyle::TEXT, summationText},
    {"product_display", "Product display", MathStyle::DISPLAY_STYLE, productDisplay},
    {"union_text", "Union text", MathStyle::TEXT, unionText},
    {"log_trig_composite", "Log/trig composite", MathStyle::DISPLAY_STYLE, logTrigComposite},
    {"determinant_bars", "Determinant bars", MathStyle::DISPLAY_STYLE, determinantBars},
    {"brace_system", "Brace system", MathStyle::DISPLAY_STYLE, braceSystemOnly},
    {"bracketed_binomial_stack", "Bracketed binomial stack", MathStyle::DISPLAY_STYLE, bracketedBinomialStack},
};

} // namespace

StressExpressionCatalog mathStressExpressionCases() {
    return {kStressCases, sizeof(kStressCases) / sizeof(kStressCases[0])};
}

bool containsScriptLevel(const MathNode* node, uint8_t level) {
    if (!node) return false;
    if (node->scriptLevel() == level) return true;

    for (int i = 0; i < node->childCount(); ++i) {
        if (containsScriptLevel(node->child(i), level)) return true;
    }

    return false;
}

} // namespace vpam
