#pragma once

#include "MathAST.h"
#include "MathEvaluator.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace numos {

enum class CalculationFormat : uint8_t {
    Standard, Decimal, Periodic, Extended, Scientific, Engineering,
    MixedFraction, PrimeFactors, Polar, Exponential, Fixed,
    Radians, Degrees, Gradians, OutputUnit, Uncertainty, UncertaintyStandard,
    UncertaintyRelative, UncertaintyExpanded, UncertaintyParenthetical, Count
};

inline const char* calculationFormatLabel(CalculationFormat format, bool spanish) {
    static constexpr const char* labels[][2] = {
        {"Standard (exact)", "Normal (exacto)"},
        {"Decimal", "Decimal"}, {"Recurring decimal", "Decimal periódico"},
        {"Extended decimal", "Decimal extendido"},
        {"Scientific (SCI)", "Científica (SCI)"}, {"Engineering (ENG)", "Ingeniería (ENG)"},
        {"Mixed fraction", "Fracción mixta"}, {"Prime factors", "Factores primos"},
        {"Polar / phasor", "Polar / fasor"}, {"Exponential form", "Forma exponencial"},
        {"Fixed decimals (FIX)", "Decimales fijos (FIX)"},
        {"Radians (RAD)", "Radianes (RAD)"}, {"Degrees (DEG)", "Grados (DEG)"},
        {"Gradians (GRA)", "Centesimales (GRA)"},
        {"Output unit", "Unidad de salida"}, {"Uncertainty", "Incertidumbre"},
        {"Standard u (k=1)", "Estándar u (k=1)"}, {"Relative u_r", "Relativa u_r"},
        {"Expanded U (k=2)", "Expandida U (k=2)"}, {"Parenthetic digits", "Notación parentética"}
    };
    const unsigned i = static_cast<unsigned>(format);
    return i < static_cast<unsigned>(CalculationFormat::Count) ? labels[i][spanish ? 1 : 0] : "";
}

inline bool numericComplex(const vpam::MathNode* node, bool& imaginary,
                           unsigned& budget, unsigned depth = 0) {
    if (!node || !budget-- || depth > 32) return false;
    using namespace vpam;
    switch (node->type()) {
        case NodeType::Constant:
            imaginary |= static_cast<const NodeConstant*>(node)->constKind() == ConstKind::Imag;
            return true;
        case NodeType::Number: case NodeType::Operator: return true;
        case NodeType::Row: case NodeType::Paren: case NodeType::Root:
        case NodeType::Power: case NodeType::Fraction: case NodeType::Function:
            for (int i = 0; i < node->childCount(); ++i)
                if (!numericComplex(node->child(i), imaginary, budget, depth + 1)) return false;
            return true;
        default: return false;
    }
}

// Numeric literal metadata, not a formula parser. The source is the engine's
// canonical scalar/typed numeric companion; never the authored expression.
// No floating point conversion: scientific exponents must survive 10^1000.
struct DecimalParts {
    bool negative = false;
    std::string digits;
    int exponent = 0; // normalized scientific exponent
};

inline bool decimalParts(const std::string& text, DecimalParts& out) {
    out = {};
    size_t i = 0;
    if (text.empty()) return false;
    if (text[i] == '-' || text[i] == '+') out.negative = text[i++] == '-';
    bool dot = false, any = false;
    int beforeDot = 0;
    for (; i < text.size() && text[i] != 'e' && text[i] != 'E'; ++i) {
        const char c = text[i];
        if (c == '.' && !dot) { dot = true; continue; }
        if (c < '0' || c > '9') return false;
        any = true;
        out.digits += c;
        if (!dot) ++beforeDot;
    }
    if (!any) return false;
    int exp = 0, sign = 1;
    if (i < text.size()) {
        ++i;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            sign = text[i++] == '-' ? -1 : 1;
        if (i == text.size()) return false;
        for (; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9' || exp > 100000) return false;
            exp = exp * 10 + text[i] - '0';
        }
    }
    const size_t first = out.digits.find_first_not_of('0');
    if (first == std::string::npos) { out = {}; out.digits = "0"; return true; }
    out.exponent = sign * exp + beforeDot - static_cast<int>(first) - 1;
    out.digits.erase(0, first);
    while (out.digits.size() > 1 && out.digits.back() == '0') out.digits.pop_back();
    return true;
}

inline int engineeringExponent(int scientificExponent) {
    // WHY: integer division truncates toward zero, but -1 must map to -3.
    const int remainder = ((scientificExponent % 3) + 3) % 3;
    return scientificExponent - remainder;
}

// Decimal rounding at a requested place, including carry and negative values.
// Bounded output: 256 integer digits plus at most 9 decimal places. Numbers
// smaller than the requested precision round without expanding 10^(-1000).
inline bool fixedDecimal(const std::string& source, unsigned places, std::string& out) {
    DecimalParts value;
    if (places > 9 || !decimalParts(source, value) || value.exponent > 255) return false;
    const int keep = value.exponent + 1 + static_cast<int>(places);
    std::string rounded;
    bool up = false;
    if (keep <= 0) {
        rounded = "0";
        up = keep == 0 && value.digits[0] >= '5';
    } else {
        rounded = value.digits.substr(0, keep);
        if (static_cast<int>(rounded.size()) < keep) rounded.append(keep-rounded.size(), '0');
        up = keep < static_cast<int>(value.digits.size()) && value.digits[keep] >= '5';
    }
    if (up) {
        size_t i = rounded.size();
        while (i && rounded[i-1] == '9') rounded[--i] = '0';
        if (i) ++rounded[i-1]; else rounded.insert(0, 1, '1');
    }
    const bool zero = rounded.find_first_not_of('0') == std::string::npos;
    if (rounded.size() <= places) rounded.insert(0, places + 1 - rounded.size(), '0');
    if (places) rounded.insert(rounded.size()-places, 1, '.');
    if (value.negative && !zero) rounded.insert(0, 1, '-');
    out = std::move(rounded);
    return true;
}

// Small dimensional annotation, not evaluation. It walks authored operators
// with their existing precedence; ordinary numbers never acquire an angle
// unit merely because their value happens to equal an inverse-trig result.
inline int angleDimension(const vpam::MathNode* node, unsigned depth = 0) {
    constexpr int unknown = 100;
    if (!node || depth > 40) return unknown;
    using namespace vpam;
    if (node->type() == NodeType::Paren) return angleDimension(node->child(0), depth+1);
    if (node->type() == NodeType::Fraction) {
        const int a = angleDimension(node->child(0), depth+1);
        const int b = angleDimension(node->child(1), depth+1);
        return a == unknown || b == unknown ? unknown : a-b;
    }
    if (node->type() == NodeType::Row) {
        int sum = unknown, term = 0;
        bool operand = false, divide = false;
        for (int i = 0; i < node->childCount(); ++i) {
            const auto* child = node->child(i);
            if (child->type() == NodeType::Operator) {
                const auto op = static_cast<const NodeOperator*>(child)->op();
                if (op == OpKind::Add || op == OpKind::Sub) {
                    if (!operand) continue; // unary sign
                    if (sum != unknown && sum != term) return unknown;
                    sum = term; term = 0; operand = false; divide = false;
                } else if (op == OpKind::Mul || op == OpKind::Div) divide = op == OpKind::Div;
                else return unknown;
            } else {
                const int dimension = angleDimension(child, depth+1);
                if (dimension == unknown) return unknown;
                term += divide ? -dimension : dimension;
                divide = false; operand = true;
            }
        }
        return !operand || (sum != unknown && sum != term) ? unknown : term;
    }
    if (node->type() == NodeType::Function) {
        const auto kind = static_cast<const NodeFunction*>(node)->funcKind();
        return kind == FuncKind::ArcSin || kind == FuncKind::ArcCos || kind == FuncKind::ArcTan ? 1 : 0;
    }
    if (node->type() == NodeType::Number || node->type() == NodeType::Constant ||
        node->type() == NodeType::Variable) return 0;
    // Avoid inventing units for powers, roots, collections and pending slots.
    return unknown;
}
inline bool authoredAngle(const vpam::MathNode* node) { return angleDimension(node) == 1; }

inline vpam::NodePtr powerOfTenFormat(const std::string& numeric,
                                     bool engineering, int shift = 0) {
    DecimalParts value;
    if (!decimalParts(numeric, value) || shift < -6 || shift > 6) return nullptr;
    const int exponent = value.digits == "0" ? 0 :
        (engineering ? engineeringExponent(value.exponent) + 3 * shift : value.exponent);
    const int point = value.exponent - exponent + 1;
    std::string mantissa = value.digits;
    if (point <= 0) mantissa = "0." + std::string(-point, '0') + mantissa;
    else if (point < static_cast<int>(mantissa.size())) mantissa.insert(point, 1, '.');
    else mantissa.append(point - mantissa.size(), '0');
    auto result = vpam::makeRow();
    auto* row = static_cast<vpam::NodeRow*>(result.get());
    if (value.negative) row->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
    row->appendChild(vpam::makeNumber(mantissa));
    row->appendChild(vpam::makeOperator(vpam::OpKind::Mul));
    auto power = vpam::makeRow();
    auto* expRow = static_cast<vpam::NodeRow*>(power.get());
    if (exponent < 0) expRow->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
    expRow->appendChild(vpam::makeNumber(std::to_string(exponent < 0 ? -exponent : exponent)));
    row->appendChild(vpam::makePower(vpam::makeNumber("10"), std::move(power)));
    return result;
}

inline vpam::NodePtr mixedFractionFormat(const vpam::ExactVal& value) {
    if (!value.ok || !value.isRational() || value.approximate || value.den <= 1 ||
        value.num == std::numeric_limits<int64_t>::min()) return nullptr;
    const int64_t magnitude = value.num < 0 ? -value.num : value.num;
    if (magnitude < value.den || magnitude % value.den == 0) return nullptr;
    auto result = vpam::makeRow();
    auto* row = static_cast<vpam::NodeRow*>(result.get());
    if (value.num < 0) row->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
    row->appendChild(vpam::makeNumber(std::to_string(magnitude / value.den)));
    row->appendChild(vpam::makeFraction(
        vpam::makeNumber(std::to_string(magnitude % value.den)),
        vpam::makeNumber(std::to_string(value.den))));
    // Display convention only. Reuse/Ans always uses the canonical result,
    // never serializes this juxtaposition as multiplication.
    return result;
}

} // namespace numos
