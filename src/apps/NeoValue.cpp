/**
 * NeoValue.cpp — Runtime value implementation for NeoLanguage.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 2 (Interpreter)
 */

#include "NeoValue.h"
#include "NeoUnits.h"
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

NeoValue NeoValue::makeList(std::vector<NeoValue>* list) {
    NeoValue v;
    v._type = Type::List;
    v._list = list;
    return v;
}

NeoValue NeoValue::makeNativeFunction(NeoNativeCallFn fn, void* ctx) {
    NeoValue v;
    v._type      = Type::NativeFunction;
    v._nativeFn  = fn;
    v._nativeCtx = ctx;
    return v;
}

NeoValue NeoValue::makeQuantity(NeoQuantity* q) {
    NeoValue v;
    v._type     = Type::Quantity;
    v._quantity = q;
    return v;
}

NeoValue NeoValue::makeString(const std::string& s) {
    NeoValue v;
    v._type = Type::String;
    v._str  = s;
    return v;
}

NeoValue NeoValue::makeDict(std::map<std::string, NeoValue>* dict) {
    NeoValue v;
    v._type = Type::Dictionary;
    v._dict = dict;
    return v;
}

// ════════════════════════════════════════════════════════════════════
// isTruthy
// ════════════════════════════════════════════════════════════════════

bool NeoValue::isTruthy() const {
    switch (_type) {
        case Type::Null:       return false;
        case Type::Boolean:    return _bool;
        case Type::Number:     return _num != 0.0;
        case Type::Exact:      return !_exact.isZero();
        case Type::Symbolic:   return _sym != nullptr;
        case Type::Function:   return _funcDef != nullptr;
        case Type::List:       return _list != nullptr && !_list->empty();
        case Type::NativeFunction: return _nativeFn != nullptr;
        case Type::Quantity:   return _quantity != nullptr;
        case Type::String:     return !_str.empty();
        case Type::Dictionary: return _dict != nullptr && !_dict->empty();
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
        case Type::Quantity: return _quantity ? _quantity->magnitude : 0.0;
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
// Quantity arithmetic helpers (defined before use in add/sub/mul/div)
// ════════════════════════════════════════════════════════════════════

static NeoValue addQuantity(const NeoQuantity* a, const NeoQuantity* b) {
    if (!a || !b) return NeoValue::makeNull();
    NeoQuantity* out = new NeoQuantity();
    if (!NeoUnits::add(*a, *b, *out)) { delete out; return NeoValue::makeNull(); }
    return NeoValue::makeQuantity(out);
}

static NeoValue subQuantity(const NeoQuantity* a, const NeoQuantity* b) {
    if (!a || !b) return NeoValue::makeNull();
    NeoQuantity* out = new NeoQuantity();
    if (!NeoUnits::sub(*a, *b, *out)) { delete out; return NeoValue::makeNull(); }
    return NeoValue::makeQuantity(out);
}

static NeoValue mulQuantity(const NeoQuantity* a, const NeoQuantity* b) {
    if (!a || !b) return NeoValue::makeNull();
    NeoQuantity* out = new NeoQuantity();
    NeoUnits::mul(*a, *b, *out);
    return NeoValue::makeQuantity(out);
}

static NeoValue divQuantity(const NeoQuantity* a, const NeoQuantity* b) {
    if (!a || !b) return NeoValue::makeNull();
    NeoQuantity* out = new NeoQuantity();
    if (!NeoUnits::div(*a, *b, *out)) { delete out; return NeoValue::makeNull(); }
    return NeoValue::makeQuantity(out);
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

    // ── List + List → concatenation ───────────────────────────────
    if (_type == Type::List && rhs._type == Type::List) {
        auto* result = new std::vector<NeoValue>();
        if (_list) for (const auto& v : *_list) result->push_back(v);
        if (rhs._list) for (const auto& v : *rhs._list) result->push_back(v);
        return makeList(result);
    }

    // ── List + scalar → element-wise add ─────────────────────────
    if (_type == Type::List && rhs.isNumeric()) {
        auto* result = new std::vector<NeoValue>();
        if (_list) for (const auto& v : *_list) result->push_back(v.add(rhs, sa));
        return makeList(result);
    }
    if (isNumeric() && rhs._type == Type::List) {
        auto* result = new std::vector<NeoValue>();
        if (rhs._list) for (const auto& v : *rhs._list) result->push_back(add(v, sa));
        return makeList(result);
    }

    // ── Quantity + Quantity → Quantity ────────────────────────────
    if (_type == Type::Quantity && rhs._type == Type::Quantity)
        return addQuantity(_quantity, rhs._quantity);

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

    // ── List - scalar → element-wise ─────────────────────────────
    if (_type == Type::List && rhs.isNumeric()) {
        auto* result = new std::vector<NeoValue>();
        if (_list) for (const auto& v : *_list) result->push_back(v.sub(rhs, sa));
        return makeList(result);
    }

    // ── Quantity - Quantity ───────────────────────────────────────
    if (_type == Type::Quantity && rhs._type == Type::Quantity)
        return subQuantity(_quantity, rhs._quantity);

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

    // ── List * scalar → element-wise (vectorisation) ─────────────
    if (_type == Type::List && rhs.isNumeric()) {
        auto* result = new std::vector<NeoValue>();
        if (_list) for (const auto& v : *_list) result->push_back(v.mul(rhs, sa));
        return makeList(result);
    }
    if (isNumeric() && rhs._type == Type::List) {
        auto* result = new std::vector<NeoValue>();
        if (rhs._list) for (const auto& v : *rhs._list) result->push_back(mul(v, sa));
        return makeList(result);
    }
    // ── List * List → element-wise (Hadamard) product ────────────
    if (_type == Type::List && rhs._type == Type::List) {
        auto* result = new std::vector<NeoValue>();
        if (_list && rhs._list) {
            size_t len = std::min(_list->size(), rhs._list->size());
            result->reserve(len);
            for (size_t k = 0; k < len; ++k)
                result->push_back((*_list)[k].mul((*rhs._list)[k], sa));
        }
        return makeList(result);
    }

    // ── Quantity * Quantity → combined unit ──────────────────────
    if (_type == Type::Quantity && rhs._type == Type::Quantity)
        return mulQuantity(_quantity, rhs._quantity);

    // ── Quantity * scalar ─────────────────────────────────────────
    if (_type == Type::Quantity && rhs.isNumeric() && _quantity) {
        NeoQuantity* out = new NeoQuantity(*_quantity);
        NeoUnits::scale(*_quantity, rhs.toDouble(), *out);
        return makeQuantity(out);
    }
    if (isNumeric() && rhs._type == Type::Quantity && rhs._quantity) {
        NeoQuantity* out = new NeoQuantity(*rhs._quantity);
        NeoUnits::scale(*rhs._quantity, toDouble(), *out);
        return makeQuantity(out);
    }

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

    // ── Quantity / Quantity → combined unit ──────────────────────
    if (_type == Type::Quantity && rhs._type == Type::Quantity)
        return divQuantity(_quantity, rhs._quantity);

    // ── Quantity / scalar ─────────────────────────────────────────
    if (_type == Type::Quantity && rhs.isNumeric() && _quantity) {
        double d = rhs.toDouble();
        if (d == 0.0) return makeNull();
        NeoQuantity* out = new NeoQuantity(*_quantity);
        NeoUnits::scale(*_quantity, 1.0 / d, *out);
        return makeQuantity(out);
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
    if (_type == Type::String && rhs._type == Type::String) return makeBool(_str == rhs._str);
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
        case Type::List: {
            std::string s = "[";
            if (_list) {
                for (size_t i = 0; i < _list->size(); ++i) {
                    if (i > 0) s += ", ";
                    s += (*_list)[i].toString();
                }
            }
            s += "]";
            return s;
        }
        case Type::NativeFunction:
            return "<native_function>";
        case Type::Quantity: {
            if (_quantity) return NeoUnits::toString(*_quantity);
            return "Quantity(null)";
        }
        case Type::String:
            return _str;
        case Type::Dictionary: {
            std::string s = "{";
            if (_dict) {
                bool first = true;
                for (const auto& kv : *_dict) {
                    if (!first) s += ", ";
                    first = false;
                    s += "\"" + kv.first + "\": " + kv.second.toString();
                }
            }
            s += "}";
            return s;
        }
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════
// Bitwise arithmetic (Phase 6) — operate on integer-truncated values
// ════════════════════════════════════════════════════════════════════

static int64_t toInt64(const NeoValue& v) {
    if (v.isNumber())  return static_cast<int64_t>(v.asNum());
    if (v.isExact())   return v.asExact().toInt64();
    if (v.isBool())    return v.asBool() ? 1 : 0;
    return static_cast<int64_t>(v.toDouble());
}

NeoValue NeoValue::bitwiseAnd(const NeoValue& rhs) const {
    return makeNumber(static_cast<double>(toInt64(*this) & toInt64(rhs)));
}

NeoValue NeoValue::bitwiseOr(const NeoValue& rhs) const {
    return makeNumber(static_cast<double>(toInt64(*this) | toInt64(rhs)));
}

NeoValue NeoValue::bitwiseXor(const NeoValue& rhs) const {
    return makeNumber(static_cast<double>(toInt64(*this) ^ toInt64(rhs)));
}

NeoValue NeoValue::bitwiseNot() const {
    return makeNumber(static_cast<double>(~toInt64(*this)));
}

NeoValue NeoValue::leftShift(const NeoValue& rhs) const {
    int64_t shift = toInt64(rhs);
    if (shift < 0 || shift >= 64) return makeNumber(0.0);
    return makeNumber(static_cast<double>(toInt64(*this) << shift));
}

NeoValue NeoValue::rightShift(const NeoValue& rhs) const {
    int64_t shift = toInt64(rhs);
    if (shift < 0 || shift >= 64) return makeNumber(0.0);
    return makeNumber(static_cast<double>(toInt64(*this) >> shift));
}
