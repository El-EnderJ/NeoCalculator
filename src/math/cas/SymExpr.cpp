/**
 * SymExpr.cpp — Implementation of generic symbolic expression tree.
 *
 * Implements evaluate(), clone(), toString(), containsVar(),
 * isPolynomial(), and toSymPoly() for all 7 concrete SymExpr types.
 *
 * Part of: NumOS Pro-CAS — Phase 1 (Data Structure Overhaul)
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
        case SymFuncKind::Abs:    return "abs";
        default:                  return "?";
    }
}

// ════════════════════════════════════════════════════════════════════
// Helper: double to compact string
// ════════════════════════════════════════════════════════════════════

static std::string dblToStr(double v) {
    // Integers print without decimal
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
// SymNum — Exact numeric constant
// ════════════════════════════════════════════════════════════════════

double SymNum::evaluate(double) const {
    return val.toDouble();
}

SymExpr* SymNum::clone(SymExprArena& arena) const {
    return arena.create<SymNum>(val);
}

std::string SymNum::toString() const {
    if (val.hasPi()) {
        if (val.num == 1 && val.den == 1) return "pi";
        if (val.num == -1 && val.den == 1) return "-pi";
        char buf[48];
        snprintf(buf, sizeof(buf), "(%lld/%lld)*pi",
                 (long long)val.num, (long long)val.den);
        return buf;
    }
    if (val.hasE()) {
        if (val.num == 1 && val.den == 1) return "e";
        if (val.num == -1 && val.den == 1) return "-e";
        char buf[48];
        snprintf(buf, sizeof(buf), "(%lld/%lld)*e",
                 (long long)val.num, (long long)val.den);
        return buf;
    }
    if (val.hasRadical()) {
        char buf[64];
        if (val.outer == 1 && val.num == 1 && val.den == 1) {
            snprintf(buf, sizeof(buf), "sqrt(%lld)", (long long)val.inner);
        } else {
            snprintf(buf, sizeof(buf), "(%lld/%lld)*%lld*sqrt(%lld)",
                     (long long)val.num, (long long)val.den,
                     (long long)val.outer, (long long)val.inner);
        }
        return buf;
    }
    if (val.den == 1) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lld", (long long)val.num);
        return buf;
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "(%lld/%lld)",
             (long long)val.num, (long long)val.den);
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
    return arena.create<SymAdd>(arr, count);
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
    return arena.create<SymMul>(arr, count);
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
    // Polynomial only if:
    // 1. Base is polynomial
    // 2. Exponent is a non-negative integer constant
    if (!base->isPolynomial()) return false;
    if (exponent->type != SymExprType::Num) return false;

    const auto& ev = static_cast<const SymNum*>(exponent)->val;
    if (!ev.isInteger()) return false;
    if (ev.num < 0) return false;       // Negative exponents → rational, not polynomial
    if (ev.num > 10) return false;       // Cap to avoid combinatorial explosion
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
// toSymPoly() — Convert polynomial SymExpr tree → SymPoly
//
// Recursive descent: each node returns a SymPoly, combined via
// SymPoly arithmetic. Only valid when isPolynomial() == true.
// ════════════════════════════════════════════════════════════════════

static SymPoly exprToPolyImpl(const SymExpr* expr, char var) {
    switch (expr->type) {
        // ── SymNum → constant SymPoly ───────────────────────────────
        case SymExprType::Num: {
            const auto* n = static_cast<const SymNum*>(expr);
            return SymPoly::fromConstant(n->val);
        }

        // ── SymVar → 1·var^1 ────────────────────────────────────────
        case SymExprType::Var: {
            const auto* v = static_cast<const SymVar*>(expr);
            SymPoly p(var);
            if (v->name == var) {
                p.terms().push_back(
                    SymTerm::variable(var, 1, 1, 1));  // 1·var^1
            } else {
                // Different variable — treat as unknown constant
                // (univariate limitation: only primary var is symbolic)
                p.terms().push_back(
                    SymTerm::variable(v->name, 1, 1, 1));
            }
            p.normalize();
            return p;
        }

        // ── SymNeg → negate the sub-polynomial ──────────────────────
        case SymExprType::Neg: {
            const auto* neg = static_cast<const SymNeg*>(expr);
            return exprToPolyImpl(neg->child, var).negate();
        }

        // ── SymAdd → sum of sub-polynomials ─────────────────────────
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

        // ── SymMul → product of sub-polynomials ─────────────────────
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

        // ── SymPow → repeated multiplication ────────────────────────
        case SymExprType::Pow: {
            const auto* pw = static_cast<const SymPow*>(expr);
            SymPoly basePoly = exprToPolyImpl(pw->base, var);
            // Exponent must be non-negative integer (guaranteed by isPolynomial)
            const auto* expNum = static_cast<const SymNum*>(pw->exponent);
            int16_t exp = static_cast<int16_t>(expNum->val.num);
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

        // ── SymFunc → should not reach here if isPolynomial() ───────
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
