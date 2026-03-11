/**
 * NeoValue.cpp — Runtime value implementation for NeoLanguage.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 2 (Interpreter)
 */

#include "NeoValue.h"
#include <cstdio>
#include <cstring>

// ════════════════════════════════════════════════════════════════════
// Static factories
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::makeNull() {
    return NeoValue();  // default ctor creates Null
}

NeoValue NeoValue::makeBool(bool b) {
    NeoValue v;
    v._type = Type::Boolean;
    v._bool = b;
    return v;
}

NeoValue NeoValue::makeNumber(double d) {
    NeoValue v;
    v._type = Type::Number;
    v._num  = d;
    return v;
}

NeoValue NeoValue::makeExact(const cas::CASRational& r) {
    NeoValue v;
    v._type  = Type::Exact;
    v._exact = r;
    return v;
}

NeoValue NeoValue::makeSymbolic(cas::SymExpr* sym) {
    NeoValue v;
    v._type = Type::Symbolic;
    v._sym  = sym;
    return v;
}

NeoValue NeoValue::makeFunction(FunctionDefNode* def, NeoEnv* closure) {
    NeoValue v;
    v._type         = Type::Function;
    v._funcDef      = def;
    v._funcClosure  = closure;
    return v;
}

// ════════════════════════════════════════════════════════════════════
// isTruthy
// ════════════════════════════════════════════════════════════════════

bool NeoValue::isTruthy() const {
    switch (_type) {
        case Type::Null:     return false;
        case Type::Boolean:  return _bool;
        case Type::Number:   return _num != 0.0;
        case Type::Exact:    return !_exact.isZero();
        case Type::Symbolic: return _sym != nullptr;
        case Type::Function: return _funcDef != nullptr;
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════
// toDouble — approximate numeric value
// ════════════════════════════════════════════════════════════════════

double NeoValue::toDouble() const {
    switch (_type) {
        case Type::Number:   return _num;
        case Type::Exact:    return _exact.toDouble();
        case Type::Boolean:  return _bool ? 1.0 : 0.0;
        case Type::Symbolic: return _sym ? _sym->evaluate(0.0) : 0.0;
        default:             return 0.0;
    }
}

// ════════════════════════════════════════════════════════════════════
// toSymExpr — coerce to SymExpr* for symbolic arithmetic
// ════════════════════════════════════════════════════════════════════

cas::SymExpr* NeoValue::toSymExpr(cas::SymExprArena& sa) const {
    using namespace cas;
    switch (_type) {
        case Type::Symbolic:
            return _sym;
        case Type::Number: {
            // Convert to a CASRational-backed SymNum for exact ints,
            // or a double-approximated rational for floats.
            double d = _num;
            double intPart;
            if (std::modf(d, &intPart) == 0.0 &&
                intPart >= -9.0e18 && intPart <= 9.0e18) {
                return symInt(sa, static_cast<int64_t>(intPart));
            }
            // Approximate as a fraction (limited precision)
            return symNum(sa, CASRational::fromFrac(
                static_cast<int64_t>(d * 1000000), 1000000));
        }
        case Type::Exact:
            return symNum(sa, _exact);
        case Type::Boolean:
            return symInt(sa, _bool ? 1 : 0);
        default:
            return nullptr;
    }
}

// ════════════════════════════════════════════════════════════════════
// add
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::add(const NeoValue& rhs, cas::SymExprArena& sa) const {
    using namespace cas;

    // ── Both Numbers → double arithmetic ─────────────────────────
    if (_type == Type::Number && rhs._type == Type::Number)
        return makeNumber(_num + rhs._num);

    // ── Both Exact → exact rational arithmetic ────────────────────
    if (_type == Type::Exact && rhs._type == Type::Exact)
        return makeExact(CASRational::add(_exact, rhs._exact));

    // ── Exact OP Number → double ──────────────────────────────────
    if (isNumeric() && rhs.isNumeric())
        return makeNumber(toDouble() + rhs.toDouble());

    // ── At least one Symbolic → symbolic addition ─────────────────
    SymExpr* lSym = toSymExpr(sa);
    SymExpr* rSym = rhs.toSymExpr(sa);
    if (!lSym || !rSym) return makeNull();
    return makeSymbolic(symAddRaw(sa, lSym, rSym));
}

// ════════════════════════════════════════════════════════════════════
// sub
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::sub(const NeoValue& rhs, cas::SymExprArena& sa) const {
    using namespace cas;

    if (_type == Type::Number && rhs._type == Type::Number)
        return makeNumber(_num - rhs._num);

    if (_type == Type::Exact && rhs._type == Type::Exact)
        return makeExact(CASRational::sub(_exact, rhs._exact));

    if (isNumeric() && rhs.isNumeric())
        return makeNumber(toDouble() - rhs.toDouble());

    // Symbolic: lhs + (-rhs)
    SymExpr* lSym = toSymExpr(sa);
    SymExpr* rSym = rhs.toSymExpr(sa);
    if (!lSym || !rSym) return makeNull();
    SymExpr* negRhs = symNeg(sa, rSym);
    return makeSymbolic(symAddRaw(sa, lSym, negRhs));
}

// ════════════════════════════════════════════════════════════════════
// mul
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::mul(const NeoValue& rhs, cas::SymExprArena& sa) const {
    using namespace cas;

    if (_type == Type::Number && rhs._type == Type::Number)
        return makeNumber(_num * rhs._num);

    if (_type == Type::Exact && rhs._type == Type::Exact)
        return makeExact(CASRational::mul(_exact, rhs._exact));

    if (isNumeric() && rhs.isNumeric())
        return makeNumber(toDouble() * rhs.toDouble());

    SymExpr* lSym = toSymExpr(sa);
    SymExpr* rSym = rhs.toSymExpr(sa);
    if (!lSym || !rSym) return makeNull();
    return makeSymbolic(symMulRaw(sa, lSym, rSym));
}

// ════════════════════════════════════════════════════════════════════
// div
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::div(const NeoValue& rhs, cas::SymExprArena& sa) const {
    using namespace cas;

    if (_type == Type::Number && rhs._type == Type::Number) {
        if (rhs._num == 0.0) return makeNumber(std::numeric_limits<double>::quiet_NaN());
        return makeNumber(_num / rhs._num);
    }

    if (_type == Type::Exact && rhs._type == Type::Exact) {
        if (rhs._exact.isZero())
            return makeNumber(std::numeric_limits<double>::quiet_NaN());
        return makeExact(CASRational::div(_exact, rhs._exact));
    }

    if (isNumeric() && rhs.isNumeric()) {
        double d = rhs.toDouble();
        if (d == 0.0) return makeNumber(std::numeric_limits<double>::quiet_NaN());
        return makeNumber(toDouble() / d);
    }

    // Symbolic: lhs * (rhs ^ -1)
    SymExpr* lSym = toSymExpr(sa);
    SymExpr* rSym = rhs.toSymExpr(sa);
    if (!lSym || !rSym) return makeNull();
    SymExpr* negOne  = symInt(sa, -1);
    SymExpr* invRhs  = symPow(sa, rSym, negOne);
    return makeSymbolic(symMulRaw(sa, lSym, invRhs));
}

// ════════════════════════════════════════════════════════════════════
// pow
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::pow(const NeoValue& rhs, cas::SymExprArena& sa) const {
    using namespace cas;

    if (_type == Type::Number && rhs._type == Type::Number)
        return makeNumber(std::pow(_num, rhs._num));

    if (_type == Type::Exact && rhs._type == Type::Exact && rhs._exact.isInteger()) {
        int32_t exp = static_cast<int32_t>(rhs._exact.toInt64());
        return makeExact(CASRational::pow(_exact, exp));
    }

    if (isNumeric() && rhs.isNumeric())
        return makeNumber(std::pow(toDouble(), rhs.toDouble()));

    SymExpr* lSym = toSymExpr(sa);
    SymExpr* rSym = rhs.toSymExpr(sa);
    if (!lSym || !rSym) return makeNull();
    return makeSymbolic(symPow(sa, lSym, rSym));
}

// ════════════════════════════════════════════════════════════════════
// neg
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::neg(cas::SymExprArena& sa) const {
    using namespace cas;

    if (_type == Type::Number)  return makeNumber(-_num);
    if (_type == Type::Exact)   return makeExact(CASRational::neg(_exact));
    if (_type == Type::Boolean) return makeNumber(_bool ? -1.0 : 0.0);

    SymExpr* s = toSymExpr(sa);
    if (!s) return makeNull();
    return makeSymbolic(symNeg(sa, s));
}

// ════════════════════════════════════════════════════════════════════
// Comparison helpers
// ════════════════════════════════════════════════════════════════════

NeoValue NeoValue::opEq(const NeoValue& rhs) const {
    if (_type == Type::Null && rhs._type == Type::Null) return makeBool(true);
    if (_type != rhs._type) return makeBool(false);
    switch (_type) {
        case Type::Boolean:  return makeBool(_bool == rhs._bool);
        case Type::Number:   return makeBool(_num  == rhs._num);
        case Type::Exact:    return makeBool(_exact == rhs._exact);
        case Type::Symbolic: return makeBool(_sym  == rhs._sym);
        default:             return makeBool(false);
    }
}

NeoValue NeoValue::opNe(const NeoValue& rhs) const {
    NeoValue eq = opEq(rhs);
    return makeBool(!eq.asBool());
}

NeoValue NeoValue::opLt(const NeoValue& rhs) const {
    if (isNumeric() && rhs.isNumeric())
        return makeBool(toDouble() < rhs.toDouble());
    return makeBool(false);
}

NeoValue NeoValue::opLe(const NeoValue& rhs) const {
    if (isNumeric() && rhs.isNumeric())
        return makeBool(toDouble() <= rhs.toDouble());
    return makeBool(false);
}

NeoValue NeoValue::opGt(const NeoValue& rhs) const {
    if (isNumeric() && rhs.isNumeric())
        return makeBool(toDouble() > rhs.toDouble());
    return makeBool(false);
}

NeoValue NeoValue::opGe(const NeoValue& rhs) const {
    if (isNumeric() && rhs.isNumeric())
        return makeBool(toDouble() >= rhs.toDouble());
    return makeBool(false);
}

// ════════════════════════════════════════════════════════════════════
// toString
// ════════════════════════════════════════════════════════════════════

std::string NeoValue::toString() const {
    char buf[64];
    switch (_type) {
        case Type::Null:
            return "None";
        case Type::Boolean:
            return _bool ? "True" : "False";
        case Type::Number: {
            // Show as integer if value is a whole number
            double intPart;
            if (std::modf(_num, &intPart) == 0.0 &&
                intPart >= -1e15 && intPart <= 1e15) {
                std::snprintf(buf, sizeof(buf), "%.0f", intPart);
            } else {
                std::snprintf(buf, sizeof(buf), "%.10g", _num);
            }
            return std::string(buf);
        }
        case Type::Exact: {
            if (_exact.hasError()) return "Rational(error)";
            if (_exact.isInteger()) {
                std::snprintf(buf, sizeof(buf), "%lld",
                    static_cast<long long>(_exact.toInt64()));
                return std::string(buf);
            }
            // num/den
            std::string s = std::to_string(_exact.num().toInt64());
            s += "/";
            s += std::to_string(_exact.den().toInt64());
            return s;
        }
        case Type::Symbolic:
            return _sym ? _sym->toString() : "Symbolic(null)";
        case Type::Function: {
            if (_funcDef) {
                return "<function " + _funcDef->name + ">";
            }
            return "<function>";
        }
    }
    return "?";
}
