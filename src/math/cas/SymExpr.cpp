/**
 * SymExpr.cpp — Implementation of immutable symbolic expression DAG.
 *
 * Implements evaluate(), clone(), toString(), containsVar(),
 * isPolynomial(), and toSymPoly() for all concrete SymExpr types.
 *
 * Phase 2: all members are const.  clone() bypasses ConsTable
 * (creates a deep copy without dedup — useful for cross-arena cloning).
 *
 * Part of: NumOS Pro-CAS — Phase 2 (Immutable DAG & Hash-Consing)
 */

#include "SymExpr.h"
#include <cstdio>    // snprintf
#include <cstring>   // strlen

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymFuncKind name table
// ════════════════════════════════════════════════════════════════════

const char* symFuncName(SymFuncKind kind) {
    switch (kind) {
        case SymFuncKind::Sin:    return "sin";
        case SymFuncKind::Cos:    return "cos";
        case SymFuncKind::Tan:    return "tan";
        case SymFuncKind::ArcSin: return "arcsin";
        case SymFuncKind::ArcCos: return "arccos";
        case SymFuncKind::ArcTan: return "arctan";
        case SymFuncKind::Ln:     return "ln";
        case SymFuncKind::Log10:  return "log";
        case SymFuncKind::Exp:    return "exp";
        case SymFuncKind::Abs:      return "abs";
        case SymFuncKind::Integral: return "integral";
        default:                    return "?";
    }
}

// ════════════════════════════════════════════════════════════════════
// Helper: double to compact string
// ════════════════════════════════════════════════════════════════════

static std::string dblToStr(double v) {
    if (v == static_cast<int64_t>(v) && std::abs(v) < 1e15) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lld", (long long)static_cast<int64_t>(v));
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

// ════════════════════════════════════════════════════════════════════
// SymNum — Exact numeric constant (CASRational + metadata)
// ════════════════════════════════════════════════════════════════════

double SymNum::evaluate(double) const {
    vpam::ExactVal ev = toExactVal();
    return ev.toDouble();
}

SymExpr* SymNum::clone(SymExprArena& arena) const {
    return arena.create<SymNum>(_coeff, _outer, _inner, _piMul, _eMul);
}

std::string SymNum::toString() const {
    vpam::ExactVal ev = toExactVal();

    if (ev.hasPi()) {
        if (ev.num == 1 && ev.den == 1) return "pi";
        if (ev.num == -1 && ev.den == 1) return "-pi";
        char buf[48];
        snprintf(buf, sizeof(buf), "(%lld/%lld)*pi",
                 (long long)ev.num, (long long)ev.den);
        return buf;
    }
    if (ev.hasE()) {
        if (ev.num == 1 && ev.den == 1) return "e";
        if (ev.num == -1 && ev.den == 1) return "-e";
        char buf[48];
        snprintf(buf, sizeof(buf), "(%lld/%lld)*e",
                 (long long)ev.num, (long long)ev.den);
        return buf;
    }
    if (ev.hasRadical()) {
        char buf[64];
        if (ev.outer == 1 && ev.num == 1 && ev.den == 1) {
            snprintf(buf, sizeof(buf), "sqrt(%lld)", (long long)ev.inner);
        } else {
            snprintf(buf, sizeof(buf), "(%lld/%lld)*%lld*sqrt(%lld)",
                     (long long)ev.num, (long long)ev.den,
                     (long long)ev.outer, (long long)ev.inner);
        }
        return buf;
    }
    if (ev.den == 1) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lld", (long long)ev.num);
        return buf;
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "(%lld/%lld)",
             (long long)ev.num, (long long)ev.den);
    return buf;
}

// ════════════════════════════════════════════════════════════════════
// SymVar — Variable reference
// ════════════════════════════════════════════════════════════════════

double SymVar::evaluate(double varVal) const {
    return varVal;
}

SymExpr* SymVar::clone(SymExprArena& arena) const {
    return arena.create<SymVar>(name);
}

std::string SymVar::toString() const {
    return std::string(1, name);
}

// ════════════════════════════════════════════════════════════════════
// SymNeg — Unary negation
// ════════════════════════════════════════════════════════════════════

double SymNeg::evaluate(double varVal) const {
    return -child->evaluate(varVal);
}

SymExpr* SymNeg::clone(SymExprArena& arena) const {
    return arena.create<SymNeg>(child->clone(arena));
}

std::string SymNeg::toString() const {
    return "(-" + child->toString() + ")";
}

bool SymNeg::containsVar(char v) const {
    return child->containsVar(v);
}

bool SymNeg::isPolynomial() const {
    return child->isPolynomial();
}

// ════════════════════════════════════════════════════════════════════
// SymAdd — N-ary sum
// ════════════════════════════════════════════════════════════════════

double SymAdd::evaluate(double varVal) const {
    double sum = 0.0;
    for (uint16_t i = 0; i < count; ++i)
        sum += terms[i]->evaluate(varVal);
    return sum;
}

SymExpr* SymAdd::clone(SymExprArena& arena) const {
    auto** arr = static_cast<SymExpr**>(
        arena.allocRaw(count * sizeof(SymExpr*)));
    for (uint16_t i = 0; i < count; ++i)
        arr[i] = terms[i]->clone(arena);
    return arena.create<SymAdd>(const_cast<SymExpr* const*>(arr), count);
}

std::string SymAdd::toString() const {
    if (count == 0) return "0";
    std::string s = "(";
    for (uint16_t i = 0; i < count; ++i) {
        if (i > 0) s += " + ";
        s += terms[i]->toString();
    }
    s += ")";
    return s;
}

bool SymAdd::containsVar(char v) const {
    for (uint16_t i = 0; i < count; ++i)
        if (terms[i]->containsVar(v)) return true;
    return false;
}

bool SymAdd::isPolynomial() const {
    for (uint16_t i = 0; i < count; ++i)
        if (!terms[i]->isPolynomial()) return false;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// SymMul — N-ary product
// ════════════════════════════════════════════════════════════════════

double SymMul::evaluate(double varVal) const {
    double prod = 1.0;
    for (uint16_t i = 0; i < count; ++i)
        prod *= factors[i]->evaluate(varVal);
    return prod;
}

SymExpr* SymMul::clone(SymExprArena& arena) const {
    auto** arr = static_cast<SymExpr**>(
        arena.allocRaw(count * sizeof(SymExpr*)));
    for (uint16_t i = 0; i < count; ++i)
        arr[i] = factors[i]->clone(arena);
    return arena.create<SymMul>(const_cast<SymExpr* const*>(arr), count);
}

std::string SymMul::toString() const {
    if (count == 0) return "1";
    std::string s = "(";
    for (uint16_t i = 0; i < count; ++i) {
        if (i > 0) s += " * ";
        s += factors[i]->toString();
    }
    s += ")";
    return s;
}

bool SymMul::containsVar(char v) const {
    for (uint16_t i = 0; i < count; ++i)
        if (factors[i]->containsVar(v)) return true;
    return false;
}

bool SymMul::isPolynomial() const {
    for (uint16_t i = 0; i < count; ++i)
        if (!factors[i]->isPolynomial()) return false;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// SymPow — Binary power: base ^ exponent
// ════════════════════════════════════════════════════════════════════

double SymPow::evaluate(double varVal) const {
    double b = base->evaluate(varVal);
    double e = exponent->evaluate(varVal);
    return std::pow(b, e);
}

SymExpr* SymPow::clone(SymExprArena& arena) const {
    return arena.create<SymPow>(
        base->clone(arena), exponent->clone(arena));
}

std::string SymPow::toString() const {
    return "(" + base->toString() + "^" + exponent->toString() + ")";
}

bool SymPow::containsVar(char v) const {
    return base->containsVar(v) || exponent->containsVar(v);
}

bool SymPow::isPolynomial() const {
    if (!base->isPolynomial()) return false;
    if (exponent->type != SymExprType::Num) return false;

    const auto* sn = static_cast<const SymNum*>(exponent);
    if (!sn->isPureRational() || !sn->_coeff.isInteger()) return false;
    int64_t exp = sn->_coeff.toInt64();
    if (exp < 0) return false;
    if (exp > 10) return false;
    return true;
}

// ════════════════════════════════════════════════════════════════════
// SymFunc — Named function: kind(argument)
// ════════════════════════════════════════════════════════════════════

double SymFunc::evaluate(double varVal) const {
    double a = argument->evaluate(varVal);
    switch (kind) {
        case SymFuncKind::Sin:    return std::sin(a);
        case SymFuncKind::Cos:    return std::cos(a);
        case SymFuncKind::Tan:    return std::tan(a);
        case SymFuncKind::ArcSin: return std::asin(a);
        case SymFuncKind::ArcCos: return std::acos(a);
        case SymFuncKind::ArcTan: return std::atan(a);
        case SymFuncKind::Ln:     return std::log(a);
        case SymFuncKind::Log10:  return std::log10(a);
        case SymFuncKind::Exp:    return std::exp(a);
        case SymFuncKind::Abs:    return std::fabs(a);
        default:                  return 0.0;
    }
}

SymExpr* SymFunc::clone(SymExprArena& arena) const {
    return arena.create<SymFunc>(kind, argument->clone(arena));
}

std::string SymFunc::toString() const {
    return std::string(symFuncName(kind)) + "(" + argument->toString() + ")";
}

bool SymFunc::containsVar(char v) const {
    return argument->containsVar(v);
}

// SymFunc::isPolynomial() → always false (defined inline in header)

// ════════════════════════════════════════════════════════════════════
// SymParen, SymPlusMinus & SymSubscript (Display-only)
// ════════════════════════════════════════════════════════════════════

SymExpr* SymParen::clone(SymExprArena& arena) const {
    return arena.create<SymParen>(child ? child->clone(arena) : nullptr);
}

SymExpr* SymPlusMinus::clone(SymExprArena& arena) const {
    return arena.create<SymPlusMinus>(lhs->clone(arena), rhs->clone(arena));
}

SymExpr* SymSubscript::clone(SymExprArena& arena) const {
    return arena.create<SymSubscript>(base->clone(arena), subscript->clone(arena));
}

// ════════════════════════════════════════════════════════════════════
// toSymPoly() — Convert polynomial SymExpr tree → SymPoly
// ════════════════════════════════════════════════════════════════════

static SymPoly exprToPolyImpl(const SymExpr* expr, char var) {
    switch (expr->type) {
        case SymExprType::Num: {
            const auto* n = static_cast<const SymNum*>(expr);
            return SymPoly::fromConstant(n->toExactVal());
        }

        case SymExprType::Var: {
            const auto* v = static_cast<const SymVar*>(expr);
            SymPoly p(var);
            if (v->name == var) {
                p.terms().push_back(
                    SymTerm::variable(var, 1, 1, 1));
            } else {
                p.terms().push_back(
                    SymTerm::variable(v->name, 1, 1, 1));
            }
            p.normalize();
            return p;
        }

        case SymExprType::Neg: {
            const auto* neg = static_cast<const SymNeg*>(expr);
            return exprToPolyImpl(neg->child, var).negate();
        }

        case SymExprType::Add: {
            const auto* add = static_cast<const SymAdd*>(expr);
            if (add->count == 0)
                return SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
            SymPoly result = exprToPolyImpl(add->terms[0], var);
            for (uint16_t i = 1; i < add->count; ++i)
                result = result.add(exprToPolyImpl(add->terms[i], var));
            result.normalize();
            return result;
        }

        case SymExprType::Mul: {
            const auto* mul = static_cast<const SymMul*>(expr);
            if (mul->count == 0)
                return SymPoly::fromConstant(vpam::ExactVal::fromInt(1));
            SymPoly result = exprToPolyImpl(mul->factors[0], var);
            for (uint16_t i = 1; i < mul->count; ++i)
                result = result.mul(exprToPolyImpl(mul->factors[i], var));
            result.normalize();
            return result;
        }

        case SymExprType::Pow: {
            const auto* pw = static_cast<const SymPow*>(expr);
            SymPoly basePoly = exprToPolyImpl(pw->base, var);
            const auto* expNum = static_cast<const SymNum*>(pw->exponent);
            int16_t exp = static_cast<int16_t>(expNum->_coeff.toInt64());
            if (exp == 0)
                return SymPoly::fromConstant(vpam::ExactVal::fromInt(1));
            if (exp == 1)
                return basePoly;
            SymPoly result = basePoly;
            for (int16_t i = 1; i < exp; ++i)
                result = result.mul(basePoly);
            result.normalize();
            return result;
        }

        case SymExprType::Paren:
            return exprToPolyImpl(static_cast<const SymParen*>(expr)->child, var);
        case SymExprType::PlusMinus:
        case SymExprType::Subscript:
        case SymExprType::Func:
        default:
            return SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
    }
}

SymPoly SymExpr::toSymPoly(char var) const {
    SymPoly p = exprToPolyImpl(this, var);
    p.normalize();
    return p;
}

} // namespace cas
