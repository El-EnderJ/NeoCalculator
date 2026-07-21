#if defined(NUMOS_MATH_VISUAL_VERIFY) || !defined(ARDUINO)

#include "MathRenderVisualCases.h"

#include <utility>

namespace vpam {
namespace {

template <typename... Nodes>
NodePtr row(Nodes&&... nodes) {
    auto r = makeRow();
    auto* rr = static_cast<NodeRow*>(r.get());
    (rr->appendChild(std::forward<Nodes>(nodes)), ...);
    return r;
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

NodePtr frac(NodePtr numerator, NodePtr denominator) {
    return makeFraction(std::move(numerator), std::move(denominator));
}

NodePtr xSquared() {
    return pow(row(v('x')), row(n("2")));
}

NodePtr buildTwoSquared() {
    return row(pow(row(n("2")), row(n("2"))));
}

NodePtr buildXSquared() {
    return row(xSquared());
}

NodePtr buildXPowerTen() {
    return row(pow(row(v('x')), row(n("10"))));
}

NodePtr buildParenXPlusOneSquared() {
    return row(pow(makeParen(row(v('x'), op(OpKind::Add), n("1"))),
                   row(n("2"))));
}

NodePtr buildMixedRowFractionPower() {
    return row(n("1"), op(OpKind::Add),
               frac(row(n("2")), row(n("3"))),
               op(OpKind::Add),
               xSquared());
}

NodePtr buildPhotoTwoPlusTwoOverTwo() {
    return row(n("2"), op(OpKind::Add),
               frac(row(n("2")), row(n("2"))));
}

NodePtr buildTextPlusFraction() {
    return row(v('a'), op(OpKind::Add),
               makeParen(frac(row(v('b')), row(v('c')))));
}

NodePtr buildFractionPlusPower() {
    return row(makeParen(frac(row(n("1")), row(n("2")))),
               op(OpKind::Add),
               xSquared());
}

NodePtr buildFractionBaseSquared() {
    return row(pow(makeParen(frac(row(n("2")), row(n("3")))),
                   row(n("2"))));
}

NodePtr buildTwoPowerHalf() {
    return row(pow(row(n("2")),
                   row(frac(row(n("1")), row(n("2"))))));
}

NodePtr buildMixedFractionGroupSquared() {
    return row(pow(makeParen(row(n("1"), op(OpKind::Add),
                             frac(row(n("2")), row(n("3"))))),
                   row(n("2"))));
}

NodePtr buildQuarterNestedFraction() {
    return row(frac(row(frac(row(n("1")), row(n("2")))),
                    row(frac(row(n("3")), row(n("4"))))));
}

NodePtr buildPoweredNumeratorFraction() {
    return row(frac(row(xSquared()), row(n("3"))));
}

NodePtr buildPoweredDenominatorFraction() {
    return row(frac(row(n("2")), row(xSquared())));
}

NodePtr buildNestedFraction() {
    return row(frac(row(n("1"), op(OpKind::Add),
                    frac(row(n("1")), row(n("2")))),
                    row(v('x'), op(OpKind::Add), n("3"))));
}

NodePtr buildRootQuadratic() {
    return row(makeRoot(row(xSquared(), op(OpKind::Add), n("1"))));
}

NodePtr buildRootNextToText() {
    return row(n("1"), op(OpKind::Add),
               makeRoot(row(n("2"))),
               op(OpKind::Add),
               v('x'));
}

NodePtr buildNegativePeriodicDecimal() {
    return row(makePeriodicDecimal("0", "1", "6", true));
}

NodePtr buildLongPeriodicPrefix() {
    return row(makePeriodicDecimal("0", "12345678901234567890", "6", false));
}

NodePtr buildSummationLimits() {
    return row(makeSummation(row(v('n'), rel(OpKind::Eq), n("1")),
                             row(n("10")),
                             row(pow(row(v('n')), row(n("2"))),
                                 op(OpKind::Add), n("1"))));
}

NodePtr buildWideScroll() {
    return row(n("1"), op(OpKind::Add), frac(row(n("2")), row(n("3"))),
               op(OpKind::Add), xSquared(),
               op(OpKind::Add), makeRoot(row(n("2"))),
               op(OpKind::Add), frac(row(n("1"), op(OpKind::Add), n("1")),
                                     row(v('x'), op(OpKind::Add), n("3"))),
               op(OpKind::Add), makePeriodicDecimal("0", "12345678901234567890", "6"),
               op(OpKind::Add), xSquared());
}

NodePtr assignment(const char* name, NodePtr value) {
    return makeEquation(row(makeSymbol(name)), std::move(value),
                        EquationKind::Assignment);
}

NodePtr buildResultSolutionShort() {
    auto list = makeCollection(CollectionKind::List);
    auto* c = static_cast<NodeCollection*>(list.get());
    c->appendElement(row(assignment("x", row(n("1")))));
    c->appendElement(row(assignment("x", row(n("2")))));
    return row(std::move(list));
}

NodePtr buildResultSolutionLong() {
    auto list = makeCollection(CollectionKind::List);
    auto* c = static_cast<NodeCollection*>(list.get());
    for (int i = 1; i <= 8; ++i)
        c->appendElement(row(assignment("x", row(n(std::to_string(i).c_str())))));
    return row(std::move(list));
}

NodePtr buildResultNestedFractionList() {
    auto list = makeCollection(CollectionKind::List);
    auto* c = static_cast<NodeCollection*>(list.get());
    c->appendElement(row(frac(row(n("1")), row(n("2")))));
    auto numerator = row(n("1"), op(OpKind::Add),
                         frac(row(n("2")), row(n("3"))));
    auto denominator = row(v('x'), op(OpKind::Add), n("1"));
    c->appendElement(row(frac(std::move(numerator),
                              std::move(denominator))));
    return row(std::move(list));
}

NodePtr buildResultEquationList() {
    auto list = makeCollection(CollectionKind::List);
    auto* c = static_cast<NodeCollection*>(list.get());
    c->appendElement(row(makeEquation(row(v('x')), row(n("2")))));
    c->appendElement(row(makeEquation(row(v('y')), row(frac(row(n("1")), row(n("3")))))));
    return row(std::move(list));
}

NodePtr buildResultComplexValues() {
    return row(n("2"), op(OpKind::Add), n("3"), op(OpKind::Mul),
               makeConstant(ConstKind::Imag), op(OpKind::Add),
               makeParen(row(n("2"), op(OpKind::Sub), makeRoot(row(n("3"))),
                             op(OpKind::Mul), makeConstant(ConstKind::Imag))));
}

NodePtr matrixCase(uint8_t size) {
    auto matrix = makeMatrix(size, size);
    auto* m = static_cast<NodeMatrix*>(matrix.get());
    for (uint8_t r = 0; r < size; ++r)
        for (uint8_t c = 0; c < size; ++c)
            m->setCell(r, c, row(n(std::to_string(r * size + c + 1).c_str())));
    return row(std::move(matrix));
}

NodePtr buildResultMatrix2() { return matrixCase(2); }
NodePtr buildResultMatrix3() { return matrixCase(3); }

NodePtr buildResultIntervals() {
    return row(makeInterval(row(n("0")), row(n("1")), false, false),
               op(OpKind::Add),
               makeInterval(row(n("1")), row(n("2")), true, false),
               op(OpKind::Add),
               makeInterval(row(makeSpecialValue(SpecialValueKind::NegativeInfinity)),
                            row(n("3")), false, true));
}

NodePtr buildResultPiecewise() {
    auto piece = makePiecewise();
    auto* p = static_cast<NodePiecewise*>(piece.get());
    p->appendBranch(row(op(OpKind::Sub), v('x')),
                    row(v('x'), rel(OpKind::Lt), n("0")), false);
    p->appendBranch(row(n("0")), row(v('x'), rel(OpKind::Eq), n("0")), false);
    p->appendBranch(row(v('x')), nullptr, true);
    return row(std::move(piece));
}

NodePtr buildResultNestedPiecewiseMatrix() {
    auto piece = makePiecewise();
    auto* p = static_cast<NodePiecewise*>(piece.get());
    p->appendBranch(matrixCase(2), row(v('x'), rel(OpKind::Ge), n("0")), false);
    p->appendBranch(row(makeSpecialValue(SpecialValueKind::Undefined)), nullptr, true);
    return row(std::move(piece));
}

NodePtr buildResultUndefinedUnevaluated() {
    auto call = makeCall("integrate");
    auto* fn = static_cast<NodeCall*>(call.get());
    fn->appendArgument(row(makeSymbol("f"), makeParen(row(v('x')))));
    fn->appendArgument(row(v('x')));
    return row(makeSpecialValue(SpecialValueKind::Undefined),
               op(OpKind::Add), makeUnevaluated(std::move(call)));
}

static constexpr MathRenderVisualCase kCases[] = {
    { "power_2_squared", "2^2", MathStyle::TEXT, buildTwoSquared },
    { "power_x_squared", "x^2", MathStyle::TEXT, buildXSquared },
    { "power_x_ten", "x^10", MathStyle::TEXT, buildXPowerTen },
    { "power_group_x_plus_1_squared", "(x+1)^2", MathStyle::TEXT, buildParenXPlusOneSquared },
    { "mixed_row_fraction_power", "1 + 2/3 + x^2", MathStyle::TEXT, buildMixedRowFractionPower },
    { "power_fraction_base_squared", "(2/3)^2", MathStyle::TEXT, buildFractionBaseSquared },
    { "power_two_half", "2^(1/2)", MathStyle::TEXT, buildTwoPowerHalf },
    { "power_mixed_fraction_group_squared", "(1 + 2/3)^2", MathStyle::TEXT, buildMixedFractionGroupSquared },
    { "nested_fraction_quarters", "(1/2)/(3/4)", MathStyle::TEXT, buildQuarterNestedFraction },
    { "fraction_powered_numerator", "(x^2)/3", MathStyle::TEXT, buildPoweredNumeratorFraction },
    { "fraction_powered_denominator", "2/(x^2)", MathStyle::TEXT, buildPoweredDenominatorFraction },
    { "photo_2_plus_2_over_2", "2 + 2/2", MathStyle::TEXT, buildPhotoTwoPlusTwoOverTwo },
    { "text_plus_fraction", "a + (b/c)", MathStyle::TEXT, buildTextPlusFraction },
    { "fraction_plus_power", "(1/2) + x^2", MathStyle::TEXT, buildFractionPlusPower },
    { "nested_fraction", "(1 + 1/2) / (x + 3)", MathStyle::TEXT, buildNestedFraction },
    { "root_quadratic", "sqrt(x^2 + 1)", MathStyle::DISPLAY_STYLE, buildRootQuadratic },
    { "root_next_to_text", "1 + sqrt(2) + x", MathStyle::DISPLAY_STYLE, buildRootNextToText },
    { "negative_periodic_decimal", "-0.1 overline(6)", MathStyle::DISPLAY_STYLE, buildNegativePeriodicDecimal },
    { "long_periodic_prefix", "0.12345678901234567890 overline(6)", MathStyle::DISPLAY_STYLE, buildLongPeriodicPrefix },
    { "summation_limits", "sum n=1..10 (n^2+1)", MathStyle::DISPLAY_STYLE, buildSummationLimits },
    { "wide_scroll", "wide horizontal scroll case", MathStyle::DISPLAY_STYLE, buildWideScroll },
    { "result_solution_short", "structured short solution list", MathStyle::DISPLAY_STYLE, buildResultSolutionShort },
    { "result_solution_long", "structured long solution list", MathStyle::DISPLAY_STYLE, buildResultSolutionLong },
    { "result_nested_fraction_list", "nested fractions in list", MathStyle::DISPLAY_STYLE, buildResultNestedFractionList },
    { "result_equation_list", "semantic equation list", MathStyle::DISPLAY_STYLE, buildResultEquationList },
    { "result_complex_values", "exact complex values", MathStyle::DISPLAY_STYLE, buildResultComplexValues },
    { "result_matrix_2x2", "2 by 2 matrix", MathStyle::DISPLAY_STYLE, buildResultMatrix2 },
    { "result_matrix_3x3", "3 by 3 matrix", MathStyle::DISPLAY_STYLE, buildResultMatrix3 },
    { "result_intervals", "open closed and infinite intervals", MathStyle::DISPLAY_STYLE, buildResultIntervals },
    { "result_piecewise", "three branch piecewise", MathStyle::DISPLAY_STYLE, buildResultPiecewise },
    { "result_nested_piecewise_matrix", "matrix nested in piecewise", MathStyle::DISPLAY_STYLE, buildResultNestedPiecewiseMatrix },
    { "result_undefined_unevaluated", "undefined and unevaluated", MathStyle::DISPLAY_STYLE, buildResultUndefinedUnevaluated },
};

} // namespace

const MathRenderVisualCase* mathRenderVisualCases() {
    return kCases;
}

std::size_t mathRenderVisualCaseCount() {
    return sizeof(kCases) / sizeof(kCases[0]);
}

} // namespace vpam

#endif // defined(NUMOS_MATH_VISUAL_VERIFY) || !defined(ARDUINO)
