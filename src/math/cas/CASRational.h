/**
 * CASRational.h — Overflow-safe exact rational arithmetic built on CASInt.
 *
 * Represents a fraction  num / den  where:
 *   · den > 0  always (sign lives in num)
 *   · gcd(|num|, den) == 1  always (auto-normalized after every operation)
 *   · Zero is represented as  0 / 1
 *
 * Overflow safety:
 *   All intermediate products route through CASInt::mul / CASInt::add,
 *   which auto-promote to mbedtls_mpi on overflow.  The normalize() step
 *   demotes back to int64_t whenever the GCD-reduced result fits.
 *
 * Cross-reduction strategy (minimizes BigInt promotions):
 *   · add/sub:  compute lcm(den_a, den_b) via  den_a / gcd * den_b
 *               to keep intermediate products as small as possible.
 *   · mul:      cross-reduce  gcd(num_a, den_b) and gcd(num_b, den_a)
 *               before multiplying — avoids most unnecessary promotions.
 *   · div:      same as mul with inverted second operand.
 *
 * Bridge:
 *   toExactVal() converts back to the legacy vpam::ExactVal format
 *   so the existing rendering pipeline (MathCanvas, SymExprToAST,
 *   CalculationApp) continues to work unchanged.
 *
 * Part of: NumOS Elite OmniCAS — Phase 1 (BigInt Hybrid Engine)
 */

#pragma once

#include "CASInt.h"
#include "../ExactVal.h"   // vpam::ExactVal — bridge for legacy rendering

namespace cas {

class CASRational {
public:
    // ════════════════════════════════════════════════════════════════
    // Construction
    // ════════════════════════════════════════════════════════════════

    /// Default: 0/1
    CASRational() : _num(0), _den(1) {}

    /// From integer: n/1
    explicit CASRational(int64_t n) : _num(n), _den(1) {}

    /// From numerator / denominator (auto-normalized)
    CASRational(int64_t n, int64_t d)
        : _num(n), _den(d)
    {
        normalize();
    }

    /// From two CASInts (auto-normalized)
    CASRational(const CASInt& n, const CASInt& d)
        : _num(n), _den(d)
    {
        normalize();
    }

    /// Named factories
    static CASRational fromInt(int64_t n) { return CASRational(n); }

    static CASRational fromFrac(int64_t n, int64_t d) {
        return CASRational(n, d);
    }

    static CASRational zero() { return CASRational(); }
    static CASRational one()  { return CASRational(1); }

    static CASRational makeError() {
        CASRational r;
        r._num = CASInt::fromInt(0);
        r._den = CASInt::fromInt(0);  // den == 0 signals error
        return r;
    }

    // ════════════════════════════════════════════════════════════════
    // Queries
    // ════════════════════════════════════════════════════════════════

    bool isZero()    const { return !hasError() && _num.isZero(); }
    bool isOne()     const { return !hasError() && _num.isOne() && _den.isOne(); }
    bool isInteger() const { return !hasError() && _den.isOne(); }
    bool hasError()  const { return _den.isZero() || _num.hasError() || _den.hasError(); }

    bool isNegative() const { return !hasError() && _num.isNegative(); }
    bool isPositive() const { return !hasError() && !_num.isZero() && !_num.isNegative(); }

    const CASInt& num() const { return _num; }
    const CASInt& den() const { return _den; }

    // ════════════════════════════════════════════════════════════════
    // Conversion
    // ════════════════════════════════════════════════════════════════

    /// Approximate as double (always valid).
    double toDouble() const {
        if (hasError()) return 0.0;
        return _num.toDouble() / _den.toDouble();
    }

    /// Extract integer value (only meaningful when isInteger()).
    int64_t toInt64() const {
        if (hasError()) return 0;
        if (!_den.isOne()) return 0;
        return _num.toInt64();
    }

    /// Bridge to legacy ExactVal for the rendering pipeline.
    /// Maps pure rationals.  Constants (π, e, radicals) are NOT handled
    /// here — those live as SymNum metadata fields during Phase 1.
    vpam::ExactVal toExactVal() const {
        if (hasError()) return vpam::ExactVal::makeError("CASRational error");
        // Both num and den should be demoted to int64 for ExactVal
        int64_t n = _num.toInt64();
        int64_t d = _den.toInt64();
        // Guard: if either CASInt is promoted beyond int64 range,
        // fall back to double → fraction approximation.
        if (_num.isPromoted() || _den.isPromoted()) {
            return vpam::ExactVal::fromDouble(toDouble());
        }
        return vpam::ExactVal::fromFrac(n, d);
    }

    // ════════════════════════════════════════════════════════════════
    // Arithmetic — static methods returning new CASRational
    // ════════════════════════════════════════════════════════════════

    // ── Addition: a/b + c/d ─────────────────────────────────────────
    //
    //   g  = gcd(b, d)
    //   b' = b / g          (exact integer division)
    //   d' = d / g
    //   result_num = a * d' + c * b'
    //   result_den = b' * d          (= lcm(b,d))
    //   → normalize
    //
    // Cross-reducing via g keeps intermediates ~half the size vs a*d+c*b.

    static CASRational add(const CASRational& lhs, const CASRational& rhs) {
        if (lhs.hasError() || rhs.hasError()) return makeError();
        if (lhs.isZero()) return rhs;
        if (rhs.isZero()) return lhs;

        CASInt g = CASInt::gcd(lhs._den, rhs._den);
        if (g.hasError()) return makeError();

        CASInt lhsDenReduced = CASInt::div(lhs._den, g);  // b' = b/g
        CASInt rhsDenReduced = CASInt::div(rhs._den, g);   // d' = d/g

        // num = a * d' + c * b'
        CASInt term1 = CASInt::mul(lhs._num, rhsDenReduced);
        CASInt term2 = CASInt::mul(rhs._num, lhsDenReduced);
        CASInt newNum = CASInt::add(term1, term2);

        // den = b' * d  (= b/g * d)
        CASInt newDen = CASInt::mul(lhsDenReduced, rhs._den);

        if (newNum.hasError() || newDen.hasError()) return makeError();
        return CASRational(newNum, newDen);
    }

    // ── Subtraction: a/b - c/d ──────────────────────────────────────

    static CASRational sub(const CASRational& lhs, const CASRational& rhs) {
        if (lhs.hasError() || rhs.hasError()) return makeError();
        if (rhs.isZero()) return lhs;

        CASInt g = CASInt::gcd(lhs._den, rhs._den);
        if (g.hasError()) return makeError();

        CASInt lhsDenReduced = CASInt::div(lhs._den, g);
        CASInt rhsDenReduced = CASInt::div(rhs._den, g);

        CASInt term1 = CASInt::mul(lhs._num, rhsDenReduced);
        CASInt term2 = CASInt::mul(rhs._num, lhsDenReduced);
        CASInt newNum = CASInt::sub(term1, term2);

        CASInt newDen = CASInt::mul(lhsDenReduced, rhs._den);

        if (newNum.hasError() || newDen.hasError()) return makeError();
        return CASRational(newNum, newDen);
    }

    // ── Multiplication: (a/b) * (c/d) ──────────────────────────────
    //
    //   Cross-reduce to keep intermediates small:
    //     g1 = gcd(a, d)   →  a' = a/g1,  d' = d/g1
    //     g2 = gcd(c, b)   →  c' = c/g2,  b' = b/g2
    //     result = (a' * c') / (b' * d')

    static CASRational mul(const CASRational& lhs, const CASRational& rhs) {
        if (lhs.hasError() || rhs.hasError()) return makeError();
        if (lhs.isZero() || rhs.isZero()) return zero();

        // Cross-reduce
        CASInt g1 = CASInt::gcd(lhs._num, rhs._den);
        CASInt g2 = CASInt::gcd(rhs._num, lhs._den);
        if (g1.hasError() || g2.hasError()) return makeError();

        CASInt a = CASInt::div(lhs._num, g1);
        CASInt d = CASInt::div(rhs._den, g1);
        CASInt c = CASInt::div(rhs._num, g2);
        CASInt b = CASInt::div(lhs._den, g2);

        CASInt newNum = CASInt::mul(a, c);
        CASInt newDen = CASInt::mul(b, d);

        if (newNum.hasError() || newDen.hasError()) return makeError();
        return CASRational(newNum, newDen);
    }

    // ── Division: (a/b) / (c/d) = (a/b) * (d/c) ───────────────────

    static CASRational div(const CASRational& lhs, const CASRational& rhs) {
        if (lhs.hasError() || rhs.hasError()) return makeError();
        if (rhs.isZero()) return makeError();  // Division by zero

        // Invert rhs and multiply
        CASRational rhsInv;
        rhsInv._num = rhs._den;
        rhsInv._den = rhs._num;
        // Fix sign: if rhs was negative, _den is now negative
        if (rhsInv._den.isNegative()) {
            rhsInv._num = CASInt::neg(rhsInv._num);
            rhsInv._den = CASInt::neg(rhsInv._den);
        }
        return mul(lhs, rhsInv);
    }

    // ── Negation: -(a/b) = (-a)/b ──────────────────────────────────

    static CASRational neg(const CASRational& v) {
        if (v.hasError()) return makeError();
        if (v.isZero()) return zero();
        CASInt newNum = CASInt::neg(v._num);
        if (newNum.hasError()) return makeError();
        CASRational r;
        r._num = newNum;
        r._den = v._den;
        return r;  // Already normalized (only sign changed)
    }

    // ── Absolute Value ──────────────────────────────────────────────

    static CASRational absVal(const CASRational& v) {
        if (v.hasError()) return makeError();
        CASInt newNum = CASInt::absVal(v._num);
        if (newNum.hasError()) return makeError();
        CASRational r;
        r._num = newNum;
        r._den = v._den;
        return r;
    }

    // ── Integer Power: (a/b)^n ──────────────────────────────────────
    //
    //   n > 0:  num^n / den^n
    //   n == 0: 1
    //   n < 0:  den^|n| / num^|n|   (requires num ≠ 0)
    //
    // Uses binary exponentiation to minimize multiplications.

    static CASRational pow(const CASRational& base, int32_t exp) {
        if (base.hasError()) return makeError();

        if (exp == 0) return one();
        if (exp == 1) return base;

        bool negExp = (exp < 0);
        uint32_t e = negExp
            ? static_cast<uint32_t>(-(static_cast<int64_t>(exp)))
            : static_cast<uint32_t>(exp);

        if (negExp && base.isZero()) return makeError();  // 0^(-n) undefined

        // Binary exponentiation
        CASRational result = one();
        CASRational b = base;

        while (e > 0) {
            if (e & 1) {
                result = mul(result, b);
                if (result.hasError()) return makeError();
            }
            e >>= 1;
            if (e > 0) {
                b = mul(b, b);
                if (b.hasError()) return makeError();
            }
        }

        if (negExp) {
            // Invert: swap num and den, fix sign
            if (result.isZero()) return makeError();
            CASInt tmpNum = result._den;
            CASInt tmpDen = result._num;
            if (tmpDen.isNegative()) {
                tmpNum = CASInt::neg(tmpNum);
                tmpDen = CASInt::neg(tmpDen);
            }
            result._num = tmpNum;
            result._den = tmpDen;
        }

        return result;
    }

    // ════════════════════════════════════════════════════════════════
    // Comparison
    // ════════════════════════════════════════════════════════════════

    /// Returns -1, 0, or +1.
    /// Compares a/b vs c/d  as  a*d  vs  c*b  (cross-multiply).
    static int cmp(const CASRational& lhs, const CASRational& rhs) {
        if (lhs.hasError() || rhs.hasError()) return 0;
        // Both denominators are positive, so cross-multiply preserves order.
        CASInt lhsCross = CASInt::mul(lhs._num, rhs._den);
        CASInt rhsCross = CASInt::mul(rhs._num, lhs._den);
        return CASInt::cmp(lhsCross, rhsCross);
    }

    bool operator==(const CASRational& o) const { return cmp(*this, o) == 0; }
    bool operator!=(const CASRational& o) const { return cmp(*this, o) != 0; }
    bool operator< (const CASRational& o) const { return cmp(*this, o) <  0; }
    bool operator<=(const CASRational& o) const { return cmp(*this, o) <= 0; }
    bool operator> (const CASRational& o) const { return cmp(*this, o) >  0; }
    bool operator>=(const CASRational& o) const { return cmp(*this, o) >= 0; }

    // ════════════════════════════════════════════════════════════════
    // Debug
    // ════════════════════════════════════════════════════════════════

    std::string toString() const {
        if (hasError()) return "<err>";
        if (_den.isOne()) return _num.toString();
        return _num.toString() + "/" + _den.toString();
    }

private:
    CASInt _num;   // Numerator (sign lives here)
    CASInt _den;   // Denominator (always > 0 after normalize)

    // ════════════════════════════════════════════════════════════════
    // Core invariant enforcer — called after every construction
    // and arithmetic operation.
    //
    //  1. den == 0  → error state (preserved, no further action)
    //  2. num == 0  → canonical zero: 0/1
    //  3. Sign: if den < 0, flip signs of both num and den
    //  4. GCD reduce: g = gcd(|num|, den);  num /= g;  den /= g
    //
    // Because CASInt::gcd auto-promotes on overflow, this will never
    // silently corrupt the fraction through wraparound.
    // ════════════════════════════════════════════════════════════════

    void normalize() {
        // Error propagation
        if (_num.hasError() || _den.hasError()) {
            _den = CASInt::fromInt(0);
            return;
        }
        // Division by zero → error
        if (_den.isZero()) return;

        // Zero numerator → canonical 0/1
        if (_num.isZero()) {
            _den = CASInt::fromInt(1);
            return;
        }

        // Sign: ensure denominator is positive
        if (_den.isNegative()) {
            _num = CASInt::neg(_num);
            _den = CASInt::neg(_den);
            if (_num.hasError() || _den.hasError()) {
                _den = CASInt::fromInt(0);
                return;
            }
        }

        // GCD reduction
        CASInt g = CASInt::gcd(_num, _den);
        if (g.hasError() || g.isZero()) return;  // Should not happen

        if (!g.isOne()) {
            _num = CASInt::div(_num, g);
            _den = CASInt::div(_den, g);
            if (_num.hasError() || _den.hasError()) {
                _den = CASInt::fromInt(0);
                return;
            }
        }
    }
};

} // namespace cas
