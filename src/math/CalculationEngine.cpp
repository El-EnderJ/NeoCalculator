/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

// CalculationEngine.cpp — GIAC-B01 Calculation<->Giac adapter implementation.
// See CalculationEngine.h for the contract. No LVGL, no Arduino, no giac
// types (everything Giac-shaped arrives as EngineResultNode).

#include "CalculationEngine.h"
#include "giac/EngineContracts.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "VariableManager.h"

namespace numos {

// ════════════════════════════════════════════════════════════════════════════
// Small helpers
// ════════════════════════════════════════════════════════════════════════════

namespace {

bool exactValEquals(const vpam::ExactVal& a, const vpam::ExactVal& b) {
    return a.ok == b.ok && a.approximate == b.approximate &&
           a.num == b.num && a.den == b.den &&
           a.outer == b.outer && a.inner == b.inner &&
           a.piMul == b.piMul && a.eMul == b.eMul &&
           a.approxVal == b.approxVal;
}

bool parseInt64(const std::string& s, int64_t& out) {
    if (s.empty()) return false;
    errno = 0;
    char* end = nullptr;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno == ERANGE || !end || *end != '\0') return false;
    out = static_cast<int64_t>(v);
    return true;
}

bool parseDoubleFull(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0';
}

bool mulI64(int64_t a, int64_t b, int64_t& out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(a, b, &out);
#else
    if (a != 0 && (std::abs(a) > INT64_MAX / std::abs(b == 0 ? 1 : b))) return false;
    out = a * b;
    return true;
#endif
}

} // namespace

// ════════════════════════════════════════════════════════════════════════════
// VPAM MathAST -> Giac input text (the controlled Calculation serializer)
// ════════════════════════════════════════════════════════════════════════════

namespace {

// Complexity guard: keeps a single synchronous Giac call bounded by input
// size (there is no safe in-process interruption mechanism — see GIAC-B01
// runtime-safety report).
constexpr int kMaxSerializedNodes = enginecontract::kMaxTreeNodes;
constexpr size_t kMaxSerializedLength = enginecontract::kMaxSourceBytes;

struct Serializer {
    std::string out;
    std::string err;
    int nodes = 0;

    bool fail(const char* m) {
        if (err.empty()) err = m;
        return false;
    }

    bool budget() {
        if (++nodes > kMaxSerializedNodes || out.size() > kMaxSerializedLength)
            return fail("expression too complex");
        return true;
    }

    bool emitSlot(const vpam::MathNode* n, const char* what) {
        if (!n) return fail(what);
        out += '(';
        bool ok = (n->type() == vpam::NodeType::Row)
                      ? emitRow(static_cast<const vpam::NodeRow*>(n))
                      : emitNode(n);
        out += ')';
        return ok;
    }

    bool emitRow(const vpam::NodeRow* row) {
        if (!budget()) return false;
        const auto& kids = row->children();
        bool prevOperand = false;
        bool any = false;
        for (const auto& kid : kids) {
            const vpam::MathNode* n = kid.get();
            if (!n) continue;
            if (n->type() == vpam::NodeType::Empty)
                return fail("incomplete expression");
            if (n->type() == vpam::NodeType::Operator) {
                const auto op = static_cast<const vpam::NodeOperator*>(n)->op();
                if (op != vpam::OpKind::Add && op != vpam::OpKind::Sub &&
                    op != vpam::OpKind::Mul)
                    return fail("unsupported operator");
                if (!prevOperand) {
                    // Unary position (row start or after another operator):
                    // normalize to an explicit (-1)* factor so the emitted
                    // text never relies on Giac's prefix-minus grammar.
                    if (op == vpam::OpKind::Sub) out += "(-1)*";
                    // unary '+' contributes nothing
                    else if (op == vpam::OpKind::Mul)
                        return fail("misplaced operator");
                } else {
                    out += (op == vpam::OpKind::Add) ? '+'
                         : (op == vpam::OpKind::Sub) ? '-'
                                                     : '*';
                    prevOperand = false;
                }
                continue;
            }
            if (prevOperand) out += '*';   // explicit implicit-multiplication
            if (!emitNode(n)) return false;
            prevOperand = true;
            any = true;
        }
        if (!any) return fail("incomplete expression");
        if (!prevOperand) return fail("incomplete expression");
        return true;
    }

    bool emitNumber(const vpam::NodeNumber* n) {
        const std::string& v = n->value();
        if (v.empty()) return fail("incomplete expression");
        int dots = 0, digits = 0;
        for (char c : v) {
            if (c == '.') { ++dots; continue; }
            if (c < '0' || c > '9') return fail("invalid number literal");
            ++digits;
        }
        if (dots > 1 || digits == 0) return fail("invalid number literal");
        if (v.front() == '.') out += '0';
        out += v;
        if (v.back() == '.') out += '0';
        return true;
    }

    bool emitNode(const vpam::MathNode* n) {
        if (!budget()) return false;
        using vpam::NodeType;
        switch (n->type()) {
            case NodeType::Row:
                return emitRow(static_cast<const vpam::NodeRow*>(n));
            case NodeType::Number:
                return emitNumber(static_cast<const vpam::NodeNumber*>(n));
            case NodeType::Fraction: {
                auto* f = static_cast<const vpam::NodeFraction*>(n);
                out += '(';
                if (!emitSlot(f->numerator(), "incomplete fraction")) return false;
                out += '/';
                if (!emitSlot(f->denominator(), "incomplete fraction")) return false;
                out += ')';
                return true;
            }
            case NodeType::Power: {
                auto* p = static_cast<const vpam::NodePower*>(n);
                out += '(';
                if (!emitSlot(p->base(), "incomplete power")) return false;
                out += '^';
                if (!emitSlot(p->exponent(), "incomplete power")) return false;
                out += ')';
                return true;
            }
            case NodeType::Root: {
                auto* r = static_cast<const vpam::NodeRoot*>(n);
                if (r->hasDegree()) {
                    // surd(x, n): principal real nth root, matches the VPAM
                    // radical's meaning (verified in the host harness).
                    out += "surd(";
                    if (!emitSlot(r->radicand(), "incomplete root")) return false;
                    out += ',';
                    if (!emitSlot(r->degree(), "incomplete root")) return false;
                    out += ')';
                } else {
                    out += "sqrt(";
                    if (!emitSlot(r->radicand(), "incomplete root")) return false;
                    out += ')';
                }
                return true;
            }
            case NodeType::Paren: {
                auto* p = static_cast<const vpam::NodeParen*>(n);
                if (p->delimKind() == vpam::DelimKind::Bar) {
                    out += "abs(";
                    if (!emitSlot(p->content(), "incomplete group")) return false;
                    out += ')';
                    return true;
                }
                return emitSlot(p->content(), "incomplete group");
            }
            case NodeType::Function: {
                auto* f = static_cast<const vpam::NodeFunction*>(n);
                const char* name = nullptr;
                switch (f->funcKind()) {
                    case vpam::FuncKind::Sin:    name = "sin";   break;
                    case vpam::FuncKind::Cos:    name = "cos";   break;
                    case vpam::FuncKind::Tan:    name = "tan";   break;
                    case vpam::FuncKind::ArcSin: name = "asin";  break;
                    case vpam::FuncKind::ArcCos: name = "acos";  break;
                    case vpam::FuncKind::ArcTan: name = "atan";  break;
                    case vpam::FuncKind::Ln:     name = "ln";    break;
                    // Giac's log() is the NATURAL log — NumOS log is base 10.
                    case vpam::FuncKind::Log:    name = "log10"; break;
                }
                if (!name) return fail("unsupported function");
                out += name;
                out += '(';
                if (!emitSlot(f->argument(), "incomplete function argument"))
                    return false;
                out += ')';
                return true;
            }
            case NodeType::LogBase: {
                auto* l = static_cast<const vpam::NodeLogBase*>(n);
                // logb(x, b) = log base b of x (verified in the host harness).
                out += "logb(";
                if (!emitSlot(l->argument(), "incomplete logarithm")) return false;
                out += ',';
                if (!emitSlot(l->base(), "incomplete logarithm")) return false;
                out += ')';
                return true;
            }
            case NodeType::Constant: {
                switch (static_cast<const vpam::NodeConstant*>(n)->constKind()) {
                    case vpam::ConstKind::Pi:   out += "pi";     return true;
                    case vpam::ConstKind::E:    out += "exp(1)"; return true;
                    case vpam::ConstKind::Imag: out += "i";      return true;
                }
                return fail("unsupported constant");
            }
            case NodeType::Variable: {
                const char v = static_cast<const vpam::NodeVariable*>(n)->name();
                if (v == vpam::VAR_ANS)    { out += "numos_Ans";    return true; }
                if (v == vpam::VAR_PREANS) { out += "numos_PreAns"; return true; }
                if ((v >= 'A' && v <= 'F') || v == 'x' || v == 'y' || v == 'z') {
                    out += v;
                    return true;
                }
                return fail("unsupported variable");
            }
            case NodeType::DefIntegral: {
                auto* d = static_cast<const vpam::NodeDefIntegral*>(n);
                out += "integrate(";
                if (!emitSlot(d->body(), "incomplete integral")) return false;
                out += ',';
                if (!emitSlot(d->variable(), "incomplete integral")) return false;
                out += ',';
                if (!emitSlot(d->lower(), "incomplete integral")) return false;
                out += ',';
                if (!emitSlot(d->upper(), "incomplete integral")) return false;
                out += ')';
                return true;
            }
            case NodeType::Empty:
                return fail("incomplete expression");
            default:
                // PeriodicDecimal / Summation / Subscript / BigOp / relations:
                // result-only or not-yet-mapped input shapes — typed reject,
                // never ambiguous text.
                return fail("unsupported input element");
        }
    }
};

bool rowIsEffectivelyEmpty(const vpam::MathNode* root) {
    if (!root) return true;
    if (root->type() != vpam::NodeType::Row) return false;
    const auto* row = static_cast<const vpam::NodeRow*>(root);
    for (const auto& kid : row->children())
        if (kid && kid->type() != vpam::NodeType::Empty) return false;
    return true;
}

} // namespace

bool CalculationEngine::serializeForGiac(const vpam::MathNode* root,
                                         std::string& out, std::string& error) {
    out.clear();
    error.clear();
    if (rowIsEffectivelyEmpty(root)) {
        error = "empty expression";
        return false;
    }
    Serializer s;
    const bool ok = (root->type() == vpam::NodeType::Row)
                        ? s.emitRow(static_cast<const vpam::NodeRow*>(root))
                        : s.emitNode(root);
    if (!ok) {
        error = s.err.empty() ? "unsupported expression" : s.err;
        return false;
    }
    out = std::move(s.out);
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// ExactVal -> exact Giac text (variable mirroring)
// ════════════════════════════════════════════════════════════════════════════

std::string CalculationEngine::exactValToGiacText(const vpam::ExactVal& v) {
    if (!v.ok) return "0";
    if (v.approximate) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.17g", v.approxVal);
        return std::string(buf);
    }
    std::string s = "((" + std::to_string(static_cast<long long>(v.num)) +
                    ")/(" + std::to_string(static_cast<long long>(v.den)) + "))";
    if (v.inner > 1) {
        s += "*(" + std::to_string(static_cast<long long>(v.outer)) +
             ")*sqrt(" + std::to_string(static_cast<long long>(v.inner)) + ")";
    }
    if (v.piMul != 0)
        s += "*pi^(" + std::to_string(static_cast<int>(v.piMul)) + ")";
    if (v.eMul != 0)
        s += "*exp(1)^(" + std::to_string(static_cast<int>(v.eMul)) + ")";
    return s;
}

// ════════════════════════════════════════════════════════════════════════════
// Engine result tree -> strict ExactVal mirror (tier 1)
//
// Lossless only: int64 integers, rationals, k*sqrt(n), pi/e powers and their
// rational products. A plain Decimal result maps through ExactVal::fromDouble
// (the value IS the decimal — nothing symbolic is reconstructed from it).
// Anything else returns false so the structured/text tiers take over.
// ════════════════════════════════════════════════════════════════════════════

namespace {

struct ExactAccum {
    int64_t num = 1, den = 1, inner = 1;
    int pi = 0, e = 0;
    bool neg = false;
};

bool accumRationalChild(const EngineResultNode& n, int64_t& num, int64_t& den) {
    if (n.kind == EngineNodeKind::Integer) {
        den = 1;
        return parseInt64(n.text, num);
    }
    if (n.kind == EngineNodeKind::Rational && n.children.size() == 2 &&
        n.children[0].kind == EngineNodeKind::Integer &&
        n.children[1].kind == EngineNodeKind::Integer) {
        return parseInt64(n.children[0].text, num) &&
               parseInt64(n.children[1].text, den) && den != 0;
    }
    return false;
}

bool accumulateFactor(const EngineResultNode& n, ExactAccum& a, int depth) {
    if (depth > 8) return false;
    switch (n.kind) {
        case EngineNodeKind::Integer: {
            int64_t v;
            if (!parseInt64(n.text, v)) return false;
            return mulI64(a.num, v, a.num);
        }
        case EngineNodeKind::Rational: {
            // Numerator may itself be a radical/pi product (giac prints
            // sqrt(2)/2 as a fraction); denominator must be purely rational.
            int64_t dn, dd;
            if (!accumRationalChild(n.children.size() == 2 ? n.children[1]
                                                           : n, dn, dd))
                return false;
            if (n.children.size() != 2 || dn == 0) return false;
            if (!accumulateFactor(n.children[0], a, depth + 1)) return false;
            return mulI64(a.den, dn, a.den) && mulI64(a.num, dd, a.num);
        }
        case EngineNodeKind::Neg:
            if (n.children.size() != 1) return false;
            a.neg = !a.neg;
            return accumulateFactor(n.children[0], a, depth + 1);
        case EngineNodeKind::Mul:
            for (const auto& c : n.children)
                if (!accumulateFactor(c, a, depth + 1)) return false;
            return true;
        case EngineNodeKind::Inv: {
            if (n.children.size() != 1) return false;
            int64_t dn, dd;
            if (!accumRationalChild(n.children[0], dn, dd) || dn == 0)
                return false;
            return mulI64(a.den, dn, a.den) && mulI64(a.num, dd, a.num);
        }
        case EngineNodeKind::Sqrt: {
            if (n.children.size() != 1 ||
                n.children[0].kind != EngineNodeKind::Integer)
                return false;
            int64_t v;
            if (!parseInt64(n.children[0].text, v) || v < 0) return false;
            return mulI64(a.inner, v, a.inner);
        }
        case EngineNodeKind::Pi:
            a.pi += 1;
            return true;
        case EngineNodeKind::EulerE:
            a.e += 1;
            return true;
        case EngineNodeKind::Pow: {
            if (n.children.size() != 2 ||
                n.children[1].kind != EngineNodeKind::Integer)
                return false;
            int64_t k;
            if (!parseInt64(n.children[1].text, k) || k < -127 || k > 127)
                return false;
            if (n.children[0].kind == EngineNodeKind::Pi) {
                a.pi += static_cast<int>(k);
                return true;
            }
            if (n.children[0].kind == EngineNodeKind::EulerE) {
                a.e += static_cast<int>(k);
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

} // namespace

bool CalculationEngine::resultTreeToExactVal(const EngineResultNode& tree,
                                             vpam::ExactVal& out) {
    if (tree.kind == EngineNodeKind::Decimal) {
        double d;
        if (!parseDoubleFull(tree.text, d)) return false;
        out = vpam::ExactVal::fromDouble(d);
        return out.ok;
    }
    ExactAccum a;
    if (!accumulateFactor(tree, a, 0)) return false;
    if (a.den == 0) return false;
    if (a.pi < -128 || a.pi > 127 || a.e < -128 || a.e > 127) return false;
    vpam::ExactVal v;
    v.num = a.neg ? -a.num : a.num;
    v.den = a.den;
    v.outer = 1;
    v.inner = a.inner;
    v.piMul = static_cast<int8_t>(a.pi);
    v.eMul = static_cast<int8_t>(a.e);
    v.ok = true;
    v.simplify();
    v.simplifyRadical();
    out = v;
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Engine result tree -> MathAST (tier 2, Natural Display)
// ════════════════════════════════════════════════════════════════════════════

namespace {

// Precedence contexts for parenthesization decisions.
constexpr int PREC_NONE = 0;   // top level / inside a slot row
constexpr int PREC_ADD = 1;    // term of a sum
constexpr int PREC_MUL = 2;    // factor of a product
constexpr int PREC_POW = 3;    // base of a power

constexpr int kAstMaxDepth = 32;

bool appendConverted(const EngineResultNode& n, vpam::NodeRow* row,
                     int ctxPrec, int depth);

vpam::NodePtr convertToRow(const EngineResultNode& n, int depth, bool& ok) {
    auto rowPtr = vpam::makeRow();
    auto* r = static_cast<vpam::NodeRow*>(rowPtr.get());
    ok = appendConverted(n, r, PREC_NONE, depth);
    return rowPtr;
}

bool appendParenthesized(const EngineResultNode& n, vpam::NodeRow* row,
                         int depth) {
    bool ok = false;
    auto inner = convertToRow(n, depth + 1, ok);
    if (!ok) return false;
    row->appendChild(vpam::makeParen(std::move(inner)));
    return true;
}

bool mapSymbolChar(const std::string& name, char& out) {
    if (name == "numos_Ans")    { out = vpam::VAR_ANS;    return true; }
    if (name == "numos_PreAns") { out = vpam::VAR_PREANS; return true; }
    if (name.size() == 1) {
        const char c = name[0];
        if ((c >= 'A' && c <= 'F') || c == 'x' || c == 'y' || c == 'z') {
            out = c;
            return true;
        }
    }
    return false;
}

bool numberIsNegative(const EngineResultNode& n) {
    return (n.kind == EngineNodeKind::Integer ||
            n.kind == EngineNodeKind::Decimal) &&
           !n.text.empty() && n.text[0] == '-';
}

bool appendNumberText(const std::string& text, vpam::NodeRow* row,
                      int ctxPrec) {
    if (text.empty()) return false;
    const bool negative = text[0] == '-';
    const std::string digits = negative ? text.substr(1) : text;
    if (digits.empty()) return false;
    if (negative && ctxPrec >= PREC_MUL) {
        auto inner = vpam::makeRow();
        auto* r = static_cast<vpam::NodeRow*>(inner.get());
        r->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
        r->appendChild(vpam::makeNumber(digits));
        row->appendChild(vpam::makeParen(std::move(inner)));
        return true;
    }
    if (negative) row->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
    row->appendChild(vpam::makeNumber(digits));
    return true;
}

bool appendConverted(const EngineResultNode& n, vpam::NodeRow* row,
                     int ctxPrec, int depth) {
    if (depth > kAstMaxDepth) return false;
    switch (n.kind) {
        case EngineNodeKind::Integer:
        case EngineNodeKind::Decimal:
            return appendNumberText(n.text, row, ctxPrec);

        case EngineNodeKind::Rational: {
            if (n.children.size() != 2) return false;
            bool okN = false, okD = false;
            auto num = convertToRow(n.children[0], depth + 1, okN);
            auto den = convertToRow(n.children[1], depth + 1, okD);
            if (!okN || !okD) return false;
            row->appendChild(vpam::makeFraction(std::move(num), std::move(den)));
            return true;
        }

        case EngineNodeKind::Symbol: {
            char c;
            if (mapSymbolChar(n.text, c))
                row->appendChild(vpam::makeVariable(c));
            else if (!n.text.empty())
                row->appendChild(vpam::makeSymbol(n.text));
            else
                return false;
            return true;
        }
        case EngineNodeKind::Pi:
            row->appendChild(vpam::makeConstant(vpam::ConstKind::Pi));
            return true;
        case EngineNodeKind::EulerE:
            row->appendChild(vpam::makeConstant(vpam::ConstKind::E));
            return true;
        case EngineNodeKind::ImagUnit:
            row->appendChild(vpam::makeConstant(vpam::ConstKind::Imag));
            return true;

        case EngineNodeKind::Add: {
            if (n.children.empty()) return false;
            if (ctxPrec >= PREC_MUL) return appendParenthesized(n, row, depth);
            bool first = true;
            for (const auto& term : n.children) {
                const EngineResultNode* t = &term;
                bool subtract = false;
                if (t->kind == EngineNodeKind::Neg && t->children.size() == 1) {
                    subtract = true;
                    t = &t->children[0];
                }
                EngineResultNode stripped;   // negative literal: hoist sign
                if (numberIsNegative(*t)) {
                    subtract = !subtract;
                    stripped = *t;
                    stripped.text.erase(0, 1);
                    t = &stripped;
                }
                if (!first)
                    row->appendChild(vpam::makeOperator(
                        subtract ? vpam::OpKind::Sub : vpam::OpKind::Add));
                else if (subtract)
                    row->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
                if (!appendConverted(*t, row, PREC_ADD, depth + 1))
                    return false;
                first = false;
            }
            return true;
        }

        case EngineNodeKind::Neg: {
            if (n.children.size() != 1) return false;
            if (ctxPrec >= PREC_MUL) return appendParenthesized(n, row, depth);
            row->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
            return appendConverted(n.children[0], row, PREC_MUL, depth + 1);
        }

        case EngineNodeKind::Mul: {
            if (n.children.empty()) return false;
            // Split reciprocal factors into a display fraction.
            std::vector<const EngineResultNode*> numF, denF;
            for (const auto& f : n.children) {
                if (f.kind == EngineNodeKind::Inv && f.children.size() == 1)
                    denF.push_back(&f.children[0]);
                else
                    numF.push_back(&f);
            }
            if (!denF.empty()) {
                auto buildProductRow = [&](const std::vector<const EngineResultNode*>& fs,
                                           bool& ok) -> vpam::NodePtr {
                    auto rp = vpam::makeRow();
                    auto* r = static_cast<vpam::NodeRow*>(rp.get());
                    ok = true;
                    if (fs.empty()) {
                        r->appendChild(vpam::makeNumber("1"));
                        return rp;
                    }
                    bool first = true;
                    for (const auto* f : fs) {
                        if (!first)
                            r->appendChild(vpam::makeOperator(vpam::OpKind::Mul));
                        if (!appendConverted(*f, r, PREC_MUL, depth + 1)) {
                            ok = false;
                            return rp;
                        }
                        first = false;
                    }
                    return rp;
                };
                bool okN = false, okD = false;
                auto num = buildProductRow(numF, okN);
                auto den = buildProductRow(denF, okD);
                if (!okN || !okD) return false;
                row->appendChild(
                    vpam::makeFraction(std::move(num), std::move(den)));
                return true;
            }
            bool first = true;
            for (const auto* f : numF) {
                if (!first) row->appendChild(vpam::makeOperator(vpam::OpKind::Mul));
                if (!appendConverted(*f, row, PREC_MUL, depth + 1)) return false;
                first = false;
            }
            return true;
        }

        case EngineNodeKind::Inv: {
            if (n.children.size() != 1) return false;
            auto one = vpam::makeRow();
            static_cast<vpam::NodeRow*>(one.get())->appendChild(
                vpam::makeNumber("1"));
            bool ok = false;
            auto den = convertToRow(n.children[0], depth + 1, ok);
            if (!ok) return false;
            row->appendChild(vpam::makeFraction(std::move(one), std::move(den)));
            return true;
        }

        case EngineNodeKind::Pow: {
            if (n.children.size() != 2) return false;
            bool okB = false, okE = false;
            auto basePtr = vpam::makeRow();
            auto* baseRow = static_cast<vpam::NodeRow*>(basePtr.get());
            okB = appendConverted(n.children[0], baseRow, PREC_POW, depth + 1);
            auto expo = convertToRow(n.children[1], depth + 1, okE);
            if (!okB || !okE) return false;
            row->appendChild(vpam::makePower(std::move(basePtr), std::move(expo)));
            return true;
        }

        case EngineNodeKind::Sqrt: {
            if (n.children.size() != 1) return false;
            bool ok = false;
            auto rad = convertToRow(n.children[0], depth + 1, ok);
            if (!ok) return false;
            row->appendChild(vpam::makeRoot(std::move(rad)));
            return true;
        }

        case EngineNodeKind::Root: {
            if (n.children.size() != 2 ||
                n.children[1].kind != EngineNodeKind::Integer)
                return false;
            bool ok = false;
            auto rad = convertToRow(n.children[0], depth + 1, ok);
            if (!ok) return false;
            auto deg = vpam::makeRow();
            static_cast<vpam::NodeRow*>(deg.get())->appendChild(
                vpam::makeNumber(n.children[1].text));
            row->appendChild(vpam::makeRoot(std::move(rad), std::move(deg)));
            return true;
        }

        case EngineNodeKind::Function: {
            if (n.children.empty()) return false;
            const std::string& f = n.text;
            if (n.children.size() != 1) {
                auto call = vpam::makeCall(f);
                auto* callNode = static_cast<vpam::NodeCall*>(call.get());
                for (const auto& child : n.children) {
                    bool childOk = false;
                    auto arg = convertToRow(child, depth + 1, childOk);
                    if (!childOk) return false;
                    callNode->appendArgument(std::move(arg));
                }
                row->appendChild(std::move(call));
                return true;
            }
            bool ok = false;
            auto arg = convertToRow(n.children[0], depth + 1, ok);
            if (!ok) return false;
            vpam::FuncKind kind;
            if      (f == "sin")   kind = vpam::FuncKind::Sin;
            else if (f == "cos")   kind = vpam::FuncKind::Cos;
            else if (f == "tan")   kind = vpam::FuncKind::Tan;
            else if (f == "asin")  kind = vpam::FuncKind::ArcSin;
            else if (f == "acos")  kind = vpam::FuncKind::ArcCos;
            else if (f == "atan")  kind = vpam::FuncKind::ArcTan;
            else if (f == "ln" || f == "log") kind = vpam::FuncKind::Ln;
            else if (f == "log10") kind = vpam::FuncKind::Log;
            else if (f == "exp") {
                auto e = vpam::makeRow();
                static_cast<vpam::NodeRow*>(e.get())->appendChild(
                    vpam::makeConstant(vpam::ConstKind::E));
                row->appendChild(vpam::makePower(std::move(e), std::move(arg)));
                return true;
            } else if (f == "abs") {
                row->appendChild(
                    vpam::makeParen(std::move(arg), vpam::DelimKind::Bar));
                return true;
            } else {
                auto call = vpam::makeCall(f);
                static_cast<vpam::NodeCall*>(call.get())->appendArgument(
                    std::move(arg));
                row->appendChild(std::move(call));
                return true;
            }
            row->appendChild(vpam::makeFunction(kind, std::move(arg)));
            return true;
        }

        case EngineNodeKind::Equation:
        case EngineNodeKind::Assignment: {
            if (n.children.size() != 2) return false;
            bool lhsOk = false, rhsOk = false;
            auto lhs = convertToRow(n.children[0], depth + 1, lhsOk);
            auto rhs = convertToRow(n.children[1], depth + 1, rhsOk);
            if (!lhsOk || !rhsOk) return false;
            row->appendChild(vpam::makeEquation(
                std::move(lhs), std::move(rhs),
                n.kind == EngineNodeKind::Assignment
                    ? vpam::EquationKind::Assignment
                    : vpam::EquationKind::Equation));
            return true;
        }

        case EngineNodeKind::Complex: {
            if (n.children.size() != 2) return false;
            const EngineResultNode& re = n.children[0];
            const EngineResultNode* im = &n.children[1];
            bool imNeg = false;
            if (im->kind == EngineNodeKind::Neg && im->children.size() == 1) {
                imNeg = true;
                im = &im->children[0];
            }
            EngineResultNode strippedIm;
            if (numberIsNegative(*im)) {
                imNeg = !imNeg;
                strippedIm = *im;
                strippedIm.text.erase(0, 1);
                im = &strippedIm;
            }
            const bool reIsZero =
                re.kind == EngineNodeKind::Integer && re.text == "0";
            if (!reIsZero) {
                if (!appendConverted(re, row, PREC_ADD, depth + 1)) return false;
                row->appendChild(vpam::makeOperator(
                    imNeg ? vpam::OpKind::Sub : vpam::OpKind::Add));
            } else if (imNeg) {
                row->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
            }
            const bool imIsOne =
                im->kind == EngineNodeKind::Integer && im->text == "1";
            if (!imIsOne) {
                if (!appendConverted(*im, row, PREC_MUL, depth + 1)) return false;
                row->appendChild(vpam::makeOperator(vpam::OpKind::Mul));
            }
            row->appendChild(vpam::makeConstant(vpam::ConstKind::Imag));
            return true;
        }

        case EngineNodeKind::PlusInfinity:
            row->appendChild(vpam::makeSpecialValue(
                vpam::SpecialValueKind::PositiveInfinity));
            return true;
        case EngineNodeKind::MinusInfinity:
            row->appendChild(vpam::makeSpecialValue(
                vpam::SpecialValueKind::NegativeInfinity));
            return true;
        case EngineNodeKind::UnsignedInfinity:
            row->appendChild(vpam::makeSpecialValue(
                vpam::SpecialValueKind::UnsignedInfinity));
            return true;
        case EngineNodeKind::Undefined:
            row->appendChild(vpam::makeSpecialValue(
                vpam::SpecialValueKind::Undefined));
            return true;

        case EngineNodeKind::List:
        case EngineNodeKind::Set: {
            auto collection = vpam::makeCollection(
                n.kind == EngineNodeKind::Set ? vpam::CollectionKind::Set
                                              : vpam::CollectionKind::List);
            auto* target = static_cast<vpam::NodeCollection*>(collection.get());
            for (const auto& child : n.children) {
                bool childOk = false;
                auto element = convertToRow(child, depth + 1, childOk);
                if (!childOk) return false;
                target->appendElement(std::move(element));
            }
            row->appendChild(std::move(collection));
            return true;
        }

        case EngineNodeKind::Matrix: {
            if (n.rows == 0 || n.columns == 0 ||
                n.children.size() != static_cast<size_t>(n.rows) * n.columns)
                return false;
            auto matrix = vpam::makeMatrix(n.rows, n.columns);
            auto* target = static_cast<vpam::NodeMatrix*>(matrix.get());
            for (uint8_t r = 0; r < n.rows; ++r) {
                for (uint8_t c = 0; c < n.columns; ++c) {
                    const size_t index = static_cast<size_t>(r) * n.columns + c;
                    bool childOk = false;
                    auto cell = convertToRow(n.children[index], depth + 1, childOk);
                    if (!childOk || !target->setCell(r, c, std::move(cell)))
                        return false;
                }
            }
            row->appendChild(std::move(matrix));
            return true;
        }

        case EngineNodeKind::Interval: {
            if (n.children.size() != 2) return false;
            bool lowerOk = false, upperOk = false;
            auto lower = convertToRow(n.children[0], depth + 1, lowerOk);
            auto upper = convertToRow(n.children[1], depth + 1, upperOk);
            if (!lowerOk || !upperOk) return false;
            const bool leftClosed = n.leftClosed &&
                n.children[0].kind != EngineNodeKind::MinusInfinity &&
                n.children[0].kind != EngineNodeKind::PlusInfinity &&
                n.children[0].kind != EngineNodeKind::UnsignedInfinity;
            const bool rightClosed = n.rightClosed &&
                n.children[1].kind != EngineNodeKind::MinusInfinity &&
                n.children[1].kind != EngineNodeKind::PlusInfinity &&
                n.children[1].kind != EngineNodeKind::UnsignedInfinity;
            row->appendChild(vpam::makeInterval(
                std::move(lower), std::move(upper), leftClosed, rightClosed));
            return true;
        }

        case EngineNodeKind::Piecewise: {
            if (n.children.empty()) return false;
            auto piecewise = vpam::makePiecewise();
            auto* target = static_cast<vpam::NodePiecewise*>(piecewise.get());
            const size_t paired = n.children.size() / 2;
            for (size_t i = 0; i < paired; ++i) {
                bool condOk = false, exprOk = false;
                auto condition = convertToRow(n.children[i * 2], depth + 1, condOk);
                auto expression = convertToRow(n.children[i * 2 + 1], depth + 1,
                                               exprOk);
                if (!condOk || !exprOk ||
                    !target->appendBranch(std::move(expression),
                                          std::move(condition), false))
                    return false;
            }
            if ((n.children.size() & 1U) != 0U) {
                bool exprOk = false;
                auto expression = convertToRow(n.children.back(), depth + 1,
                                               exprOk);
                if (!exprOk || !target->appendBranch(
                        std::move(expression), nullptr, true))
                    return false;
            }
            row->appendChild(std::move(piecewise));
            return true;
        }

        case EngineNodeKind::Unevaluated: {
            if (n.children.size() != 1) return false;
            bool childOk = false;
            auto expression = convertToRow(n.children[0], depth + 1, childOk);
            if (!childOk) return false;
            row->appendChild(vpam::makeUnevaluated(std::move(expression)));
            return true;
        }

        case EngineNodeKind::Unsupported:
        default:
            return false;   // -> text fallback tier
    }
}

} // namespace

vpam::NodePtr CalculationEngine::resultTreeToAST(const EngineResultNode& tree) {
    bool ok = false;
    auto row = convertToRow(tree, 0, ok);
    if (!ok) return nullptr;
    return row;
}

namespace {

bool isElementWiseResult(EngineNodeKind kind) {
    return kind == EngineNodeKind::List || kind == EngineNodeKind::Set ||
           kind == EngineNodeKind::Matrix;
}

ResultReusePolicy reusePolicyFor(const EngineResultNode& tree) {
    switch (tree.kind) {
        case EngineNodeKind::Integer:
        case EngineNodeKind::Decimal:
        case EngineNodeKind::Rational:
        case EngineNodeKind::Symbol:
        case EngineNodeKind::Pi:
        case EngineNodeKind::EulerE:
        case EngineNodeKind::ImagUnit:
        case EngineNodeKind::Add:
        case EngineNodeKind::Neg:
        case EngineNodeKind::Mul:
        case EngineNodeKind::Inv:
        case EngineNodeKind::Pow:
        case EngineNodeKind::Sqrt:
        case EngineNodeKind::Root:
        case EngineNodeKind::Function:
        case EngineNodeKind::Complex:
            return ResultReusePolicy::FullyRoundTrippable;
        case EngineNodeKind::Equation:
        case EngineNodeKind::Assignment:
        case EngineNodeKind::List:
        case EngineNodeKind::Set:
        case EngineNodeKind::Matrix:
        case EngineNodeKind::Interval:
        case EngineNodeKind::Piecewise:
        case EngineNodeKind::Unevaluated:
        case EngineNodeKind::PlusInfinity:
        case EngineNodeKind::MinusInfinity:
        case EngineNodeKind::UnsignedInfinity:
            return ResultReusePolicy::DisplayOnly;
        case EngineNodeKind::Undefined:
        case EngineNodeKind::Unsupported:
            return ResultReusePolicy::NonReusable;
    }
    return ResultReusePolicy::NonReusable;
}

ResultSToDPolicy sToDPolicyFor(const EngineResultNode& tree,
                               bool hasApproximateTree) {
    if (!hasApproximateTree) return ResultSToDPolicy::Unavailable;
    if (isElementWiseResult(tree.kind)) return ResultSToDPolicy::ElementWise;
    switch (tree.kind) {
        case EngineNodeKind::Integer:
        case EngineNodeKind::Decimal:
        case EngineNodeKind::Rational:
        case EngineNodeKind::Symbol:
        case EngineNodeKind::Pi:
        case EngineNodeKind::EulerE:
        case EngineNodeKind::ImagUnit:
        case EngineNodeKind::Add:
        case EngineNodeKind::Neg:
        case EngineNodeKind::Mul:
        case EngineNodeKind::Inv:
        case EngineNodeKind::Pow:
        case EngineNodeKind::Sqrt:
        case EngineNodeKind::Root:
        case EngineNodeKind::Function:
        case EngineNodeKind::Complex:
            return ResultSToDPolicy::Scalar;
        default:
            return ResultSToDPolicy::Unavailable;
    }
}

} // namespace

// ════════════════════════════════════════════════════════════════════════════
// Variable mirroring (NumOS VariableManager -> Giac context)
// ════════════════════════════════════════════════════════════════════════════

ResultReusePolicy CalculationEngine::reusePolicyForResult(
    const EngineResultNode& tree) {
    return reusePolicyFor(tree);
}

ResultSToDPolicy CalculationEngine::sToDPolicyForResult(
    const EngineResultNode& tree, bool hasApproximateTree) {
    return sToDPolicyFor(tree, hasApproximateTree);
}

int CalculationEngine::sessionIndex(char varName) {
    if (varName >= 'A' && varName <= 'F') return varName - 'A';
    if (varName == vpam::VAR_ANS) return 6;
    if (varName == vpam::VAR_PREANS) return 7;
    return -1;
}

const char* CalculationEngine::giacNameFor(char varName) {
    static const char* names[] = {"A", "B", "C", "D", "E", "F",
                                  "numos_Ans", "numos_PreAns"};
    const int idx = sessionIndex(varName);
    return idx >= 0 ? names[idx] : nullptr;
}

void CalculationEngine::syncVariablesToGiac(std::string& diagnostic) {
    static const char kMirrored[] = {'A', 'B', 'C', 'D', 'E', 'F',
                                     vpam::VAR_ANS, vpam::VAR_PREANS};
    auto& vm = vpam::VariableManager::instance();
    auto& eng = GiacEngine::instance();
    for (char c : kMirrored) {
        const vpam::ExactVal val = vm.getVariable(c);
        const int idx = sessionIndex(c);
        const SessionExact& se = _session[idx];
        const std::string text =
            (se.valid && exactValEquals(se.snapshot, val))
                ? se.text
                : exactValToGiacText(val);
        const MathEngineResult r = eng.assign(giacNameFor(c), text.c_str());
        if (!r.ok()) {
            if (!diagnostic.empty()) diagnostic += "\n";
            diagnostic += "variable mirror failed for ";
            diagnostic += vpam::VariableManager::variableLabel(c);
        }
    }
}

void CalculationEngine::noteAnsRotated(const std::string& exactGiacText) {
    auto& vm = vpam::VariableManager::instance();
    SessionExact& ans = _session[sessionIndex(vpam::VAR_ANS)];
    SessionExact& pre = _session[sessionIndex(vpam::VAR_PREANS)];
    // The store rotated old-Ans into PreAns; the session text follows only if
    // it still matches what the store now holds (no silent drift).
    pre.valid = ans.valid && exactValEquals(ans.snapshot, vm.getPreAns());
    pre.text = ans.text;
    pre.snapshot = vm.getPreAns();
    ans.valid = !exactGiacText.empty();
    ans.snapshot = vm.getAns();
    ans.text = exactGiacText;
}

void CalculationEngine::noteVariableStored(char varName) {
    const int idx = sessionIndex(varName);
    if (idx < 0) return;   // x/y/z are not mirrored (free symbols)
    auto& vm = vpam::VariableManager::instance();
    const vpam::ExactVal val = vm.getVariable(varName);
    const SessionExact& ans = _session[sessionIndex(vpam::VAR_ANS)];
    SessionExact& slot = _session[idx];
    if (ans.valid && exactValEquals(ans.snapshot, val)) {
        slot.valid = true;
        slot.snapshot = val;
        slot.text = ans.text;
    } else {
        slot.valid = false;   // sync falls back to exactValToGiacText
    }
}

// ════════════════════════════════════════════════════════════════════════════
// The ENTER pipeline
// ════════════════════════════════════════════════════════════════════════════

CalculationEngine& CalculationEngine::instance() {
    static CalculationEngine engine;
    return engine;
}

CalculationEvaluation CalculationEngine::evaluate(const vpam::MathNode* root) {
    CalculationEvaluation ev;

    std::string serErr;
    if (!serializeForGiac(root, ev.serialized, serErr)) {
        ev.status = MathEngineStatus::ParseError;
        ev.diagnostic = serErr;
        return ev;
    }

    syncVariablesToGiac(ev.diagnostic);

    StructuredEngineResult sr =
        GiacEngine::instance().evaluateStructured(ev.serialized.c_str());
    ev.status = sr.base.status;
    ev.exactText = sr.base.exactText;
    ev.approximateText = sr.base.approximateText;
    ev.fallbackReason = sr.fallbackReason;
    if (!sr.base.diagnostic.empty()) {
        if (!ev.diagnostic.empty()) ev.diagnostic += "\n";
        ev.diagnostic += sr.base.diagnostic;
    }
    if (!ev.ok() && !(ev.status == MathEngineStatus::Undefined && sr.hasTree)) {
        ev.kind = CalcResultKind::None;
        return ev;
    }

    if (sr.hasTree) {
        ev.reusePolicy = reusePolicyForResult(sr.tree);
        ev.sToDPolicy = sToDPolicyForResult(sr.tree, sr.hasApproximateTree);
        if (resultTreeToExactVal(sr.tree, ev.exactVal)) {
            ev.exactValValid = true;
            ev.kind = CalcResultKind::Structured;
            return ev;
        }
        ev.exactAST = resultTreeToAST(sr.tree);
        if (ev.exactAST) {
            ev.exactAST->calculateLayout(vpam::defaultFontMetrics());
            const auto& layout = ev.exactAST->layout();
            const int32_t height = static_cast<int32_t>(layout.ascent) +
                                   layout.descent;
#ifdef NUMOS_GIAC_HOST_HARNESS
            GiacEngine::instance().debugRecordStructuredResultLayout(
                static_cast<uint16_t>(std::max<int32_t>(0, layout.width)),
                static_cast<uint16_t>(std::max<int32_t>(0, height)));
#endif
            if (layout.width > enginecontract::kMaxRenderedWidth ||
                height > enginecontract::kMaxRenderedHeight) {
                ev.exactAST.reset();
                ev.fallbackReason = EngineFallbackReason::RenderedSizeLimit;
                ev.sToDPolicy = ResultSToDPolicy::Unavailable;
                if (!ev.diagnostic.empty()) ev.diagnostic += "\n";
                ev.diagnostic += "structured fallback: rendered_size_limit";
                ev.kind = CalcResultKind::TextFallback;
                return ev;
            }
            if (sr.hasApproximateTree &&
                ev.sToDPolicy != ResultSToDPolicy::Unavailable) {
                ev.approximateAST = resultTreeToAST(sr.approximateTree);
                if (!ev.approximateAST)
                    ev.sToDPolicy = ResultSToDPolicy::Unavailable;
            }
            ev.kind = CalcResultKind::Structured;
            return ev;
        }
    }
    if (ev.fallbackReason == EngineFallbackReason::None)
        ev.fallbackReason = EngineFallbackReason::AstConversionFailed;
    if (!ev.diagnostic.empty()) ev.diagnostic += "\n";
    ev.diagnostic += "structured fallback: ";
    ev.diagnostic += engineFallbackReasonName(ev.fallbackReason);
    ev.kind = CalcResultKind::TextFallback;
    return ev;
}

} // namespace numos
