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
