/**
 * CASNumber.cpp — Implementation of the unified CAS numeric type.
 *
 * Arithmetic follows strict precision promotion:
 *   Rational ⊕ Rational → Rational (overflow-safe via CASInt BigInt)
 *   Rational × Radical  → Radical  (scale coefficient)
 *   Radical  × Radical  → simplified (extract square factors from product)
 *   sqrt(Rational)      → Rational if perfect square, else Radical
 *   Incompatible forms  → Floating (double fallback)
 *
 * The extractSquareFactor() helper ensures radicals are always simplified:
 *   sqrt(72)  →  6·sqrt(2)     (72 = 36·2)
 *   sqrt(50)  →  5·sqrt(2)     (50 = 25·2)
 *   sqrt(49)  →  7             (perfect square)
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 1 (Infinite Precision)
 */

#include "CASNumber.h"
#include <cmath>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════

CASNumber CASNumber::fromInt(int64_t n) {
    CASNumber r;
    r._form  = Form::Rational;
    r._coeff = CASRational::fromInt(n);
    return r;
}

CASNumber CASNumber::fromFrac(int64_t n, int64_t d) {
    CASNumber r;
    r._form  = Form::Rational;
    r._coeff = CASRational::fromFrac(n, d);
    return r;
}

CASNumber CASNumber::fromRational(const CASRational& q) {
    CASNumber r;
    r._form  = Form::Rational;
    r._coeff = q;
    return r;
}

CASNumber CASNumber::fromDouble(double v) {
    CASNumber r;
    r._form = Form::Floating;
    r._dbl  = v;
    return r;
}

CASNumber CASNumber::fromRadical(const CASRational& coeff, const CASInt& radicand) {
    // Special cases: radicand == 0 → 0, radicand == 1 → rational
    if (radicand.isZero()) return fromInt(0);
    if (radicand.isOne())  return fromRational(coeff);

    CASNumber r;
    r._form     = Form::Radical;
    r._coeff    = coeff;
    r._radicand = radicand;
    return r;
}

CASNumber CASNumber::fromExactVal(const vpam::ExactVal& ev) {
    if (!ev.ok) return makeError(ev.error);
    if (ev.approximate) return fromDouble(ev.approxVal);

    if (ev.inner > 1) {
        // ExactVal format: (num/den) * outer * sqrt(inner)
        // CASNumber radical: coeff * sqrt(radicand)
        // coeff = (num * outer) / den
        CASInt numTimesOuter = CASInt::mul(CASInt::fromInt(ev.num),
                                           CASInt::fromInt(ev.outer));
        CASRational coeff(numTimesOuter, CASInt::fromInt(ev.den));
        return fromRadical(coeff, CASInt::fromInt(ev.inner));
    }

    return fromRational(CASRational::fromFrac(ev.num, ev.den));
}

CASNumber CASNumber::makeError(const std::string& msg) {
    CASNumber r;
    r._form  = Form::Error;
    r._error = msg;
    return r;
}

// ════════════════════════════════════════════════════════════════════
// Queries
// ════════════════════════════════════════════════════════════════════

bool CASNumber::isZero() const {
    switch (_form) {
        case Form::Rational: return _coeff.isZero();
        case Form::Radical:  return _coeff.isZero();
        case Form::Floating: return _dbl == 0.0;
        case Form::Error:    return false;
    }
    return false;
}

bool CASNumber::isNegative() const {
    switch (_form) {
        case Form::Rational: return _coeff.isNegative();
        case Form::Radical:  return _coeff.isNegative();
        case Form::Floating: return _dbl < 0.0;
        case Form::Error:    return false;
    }
    return false;
}

bool CASNumber::isOne() const {
    if (_form != Form::Rational) return false;
    return _coeff.isOne();
}

// ════════════════════════════════════════════════════════════════════
// Conversion
// ════════════════════════════════════════════════════════════════════

double CASNumber::toDouble() const {
    switch (_form) {
        case Form::Rational: return _coeff.toDouble();
        case Form::Radical:  return _coeff.toDouble() * std::sqrt(_radicand.toDouble());
        case Form::Floating: return _dbl;
        case Form::Error:    return 0.0;
    }
    return 0.0;
}

vpam::ExactVal CASNumber::toExactVal() const {
    switch (_form) {
        case Form::Rational:
            return _coeff.toExactVal();

        case Form::Radical: {
            vpam::ExactVal ev;
            // CASNumber radical: coeff * sqrt(radicand)
            // ExactVal format:   (num/den) * outer * sqrt(inner)
            // Map: num = coeff.num, den = coeff.den, outer = 1, inner = radicand
            if (_coeff.num().isPromoted() || _coeff.den().isPromoted()
                || _radicand.isPromoted())
            {
                // BigInt overflow → approximate double
                ev.approximate = true;
                ev.approxVal   = toDouble();
                ev.num = 0; ev.den = 1; ev.outer = 1; ev.inner = 1;
            } else {
                ev.num   = _coeff.num().toInt64();
                ev.den   = _coeff.den().toInt64();
                ev.outer = 1;
                ev.inner = _radicand.toInt64();
            }
            return ev;
        }

        case Form::Floating: {
            vpam::ExactVal ev;
            ev.approximate = true;
            ev.approxVal   = _dbl;
            ev.num = 0; ev.den = 1; ev.outer = 1; ev.inner = 1;
            return ev;
        }

        case Form::Error:
            return vpam::ExactVal::makeError(_error);
    }
    return vpam::ExactVal::makeError("unknown form");
}

std::string CASNumber::toString() const {
    switch (_form) {
        case Form::Rational: {
            int64_t n = _coeff.num().toInt64();
            int64_t d = _coeff.den().toInt64();
            if (_coeff.num().isPromoted() || _coeff.den().isPromoted()) {
                // BigInt: use scientific notation
                char buf[32];
                snprintf(buf, sizeof(buf), "%.10g", toDouble());
                return buf;
            }
            if (d == 1) return std::to_string(n);
            return std::to_string(n) + "/" + std::to_string(d);
        }

        case Form::Radical: {
            int64_t n     = _coeff.num().toInt64();
            int64_t d     = _coeff.den().toInt64();
            int64_t inner = _radicand.toInt64();

            if (_coeff.num().isPromoted() || _coeff.den().isPromoted()
                || _radicand.isPromoted())
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.10g", toDouble());
                return buf;
            }

            std::string s;
            if (n == 1 && d == 1) {
                s = "sqrt(" + std::to_string(inner) + ")";
            } else if (n == -1 && d == 1) {
                s = "-sqrt(" + std::to_string(inner) + ")";
            } else if (d == 1) {
                s = std::to_string(n) + "*sqrt(" + std::to_string(inner) + ")";
            } else {
                s = std::to_string(n) + "/" + std::to_string(d)
                  + "*sqrt(" + std::to_string(inner) + ")";
            }
            return s;
        }

        case Form::Floating: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.10g", _dbl);
            return buf;
        }

        case Form::Error:
            return "Error: " + _error;
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════
// extractSquareFactor — n = k² · m, returns {k, m} (m square-free)
//
// Trial division by small primes.  For BigInt-promoted values, returns
// {1, n} (no factoring attempted — stays exact, just unsimplified).
// ════════════════════════════════════════════════════════════════════

CASNumber::SquareFactors CASNumber::extractSquareFactor(const CASInt& n) {
    if (n.isZero()) return { CASInt::fromInt(0), CASInt::fromInt(0) };

    // BigInt or negative → don't attempt factoring
    if (n.isPromoted()) return { CASInt::fromInt(1), n };

    int64_t val = n.toInt64();
    if (val < 0) val = -val;   // work with absolute value

    int64_t k = 1;    // extracted square root
    int64_t m = val;   // remaining square-free part

    // Trial division: check p² | m
    for (int64_t p = 2; p * p <= m; ) {
        if (m % (p * p) == 0) {
            k *= p;
            m /= (p * p);
        } else {
            p = (p == 2) ? 3 : p + 2;
        }
    }

    return { CASInt::fromInt(k), CASInt::fromInt(m) };
}

// ════════════════════════════════════════════════════════════════════
// simplifyRadical — coeff * sqrt(radicand), extract perfect squares
//
//   coeff * sqrt(k² · m) = (coeff · k) * sqrt(m)
//   If m == 1, the result is purely Rational.
// ════════════════════════════════════════════════════════════════════

CASNumber CASNumber::simplifyRadical(const CASRational& coeff, const CASInt& radicand) {
    if (radicand.isZero()) return fromInt(0);
    if (radicand.isOne())  return fromRational(coeff);

    auto sf = extractSquareFactor(radicand);
    // coeff * sqrt(k² · m) = (coeff * k) * sqrt(m)
    CASRational newCoeff = CASRational::mul(coeff, CASRational(sf.k, CASInt::fromInt(1)));

    if (sf.m.isOne()) return fromRational(newCoeff);
    return fromRadical(newCoeff, sf.m);
}

// ════════════════════════════════════════════════════════════════════
// Arithmetic
// ════════════════════════════════════════════════════════════════════

// ── Addition ────────────────────────────────────────────────────────

CASNumber CASNumber::add(const CASNumber& a, const CASNumber& b) {
    if (a.isError()) return a;
    if (b.isError()) return b;

    // Rational + Rational → Rational
    if (a.isRational() && b.isRational()) {
        return fromRational(CASRational::add(a._coeff, b._coeff));
    }

    // Radical + Radical with same radicand → combine coefficients
    if (a.isRadical() && b.isRadical()) {
        if (CASInt::cmp(a._radicand, b._radicand) == 0) {
            CASRational sum = CASRational::add(a._coeff, b._coeff);
            if (sum.isZero()) return fromInt(0);
            return fromRadical(sum, a._radicand);
        }
    }

    // Fallback: double
    return fromDouble(a.toDouble() + b.toDouble());
}

// ── Subtraction ─────────────────────────────────────────────────────

CASNumber CASNumber::sub(const CASNumber& a, const CASNumber& b) {
    return add(a, neg(b));
}

// ── Multiplication ──────────────────────────────────────────────────

CASNumber CASNumber::mul(const CASNumber& a, const CASNumber& b) {
    if (a.isError()) return a;
    if (b.isError()) return b;

    // Zero short-circuit
    if (a.isZero() || b.isZero()) return fromInt(0);

    // Rational × Rational → Rational
    if (a.isRational() && b.isRational()) {
        return fromRational(CASRational::mul(a._coeff, b._coeff));
    }

    // Rational × Radical → Radical (scale coefficient)
    if (a.isRational() && b.isRadical()) {
        return fromRadical(CASRational::mul(a._coeff, b._coeff), b._radicand);
    }
    if (a.isRadical() && b.isRational()) {
        return fromRadical(CASRational::mul(a._coeff, b._coeff), a._radicand);
    }

    // Radical × Radical → simplify(coeff_a * coeff_b, rad_a * rad_b)
    //   sqrt(A) * sqrt(B) = sqrt(A*B), then extract squares
    if (a.isRadical() && b.isRadical()) {
        CASRational coeffProd = CASRational::mul(a._coeff, b._coeff);
        CASInt      radProd   = CASInt::mul(a._radicand, b._radicand);
        return simplifyRadical(coeffProd, radProd);
    }

    // Fallback: double
    return fromDouble(a.toDouble() * b.toDouble());
}

// ── Division ────────────────────────────────────────────────────────

CASNumber CASNumber::div(const CASNumber& a, const CASNumber& b) {
    if (a.isError()) return a;
    if (b.isError()) return b;
    if (b.isZero()) return makeError("Division by zero");

    // Rational / Rational → Rational
    if (a.isRational() && b.isRational()) {
        return fromRational(CASRational::div(a._coeff, b._coeff));
    }

    // Radical / Rational → Radical (scale coefficient)
    if (a.isRadical() && b.isRational()) {
        return fromRadical(CASRational::div(a._coeff, b._coeff), a._radicand);
    }

    // Rational / Radical → rationalize:
    //   a / (c·sqrt(r)) = a / (c·r) · sqrt(r)
    if (a.isRational() && b.isRadical()) {
        // Denominator full value: c * r  (as rational)
        CASRational rAsRat(b._radicand, CASInt::fromInt(1));
        CASRational denomFull = CASRational::mul(b._coeff, rAsRat);
        CASRational newCoeff  = CASRational::div(a._coeff, denomFull);
        return fromRadical(newCoeff, b._radicand);
    }

    // Radical / Radical with same radicand → Rational
    if (a.isRadical() && b.isRadical()) {
        if (CASInt::cmp(a._radicand, b._radicand) == 0) {
            return fromRational(CASRational::div(a._coeff, b._coeff));
        }
    }

    // Fallback: double
    return fromDouble(a.toDouble() / b.toDouble());
}

// ── Negation ────────────────────────────────────────────────────────

CASNumber CASNumber::neg(const CASNumber& a) {
    if (a.isError()) return a;

    CASNumber r = a;
    switch (r._form) {
        case Form::Rational:
        case Form::Radical:
            r._coeff = CASRational::neg(r._coeff);
            break;
        case Form::Floating:
            r._dbl = -r._dbl;
            break;
        default:
            break;
    }
    return r;
}

// ── Absolute value ──────────────────────────────────────────────────

CASNumber CASNumber::abs(const CASNumber& a) {
    if (a.isNegative()) return neg(a);
    return a;
}

// ── Square root ─────────────────────────────────────────────────────
//
// sqrt(p/q) — rationalized as sqrt(p·q) / q:
//   p·q = k² · m   →   sqrt(p·q) = k · sqrt(m)
//   Result: (k/q) · sqrt(m)
//   If m == 1, the result is purely Rational.

CASNumber CASNumber::sqrt(const CASNumber& a) {
    if (a.isError()) return a;
    if (a.isNegative()) return makeError("Square root of negative number");
    if (a.isZero()) return fromInt(0);

    if (a.isRational()) {
        CASInt p = a._coeff.num();   // numerator (>= 0 since !isNegative)
        CASInt q = a._coeff.den();   // denominator (always > 0)

        // Rationalize: sqrt(p/q) = sqrt(p·q) / q
        CASInt pq = CASInt::mul(p, q);
        auto sf = extractSquareFactor(pq);

        // Result: (k / q) · sqrt(m)
        CASRational resultCoeff(sf.k, q);

        if (sf.m.isOne()) {
            return fromRational(resultCoeff);
        }
        return fromRadical(resultCoeff, sf.m);
    }

    // Floating or Radical → double fallback
    double v = a.toDouble();
    if (v < 0.0) return makeError("Square root of negative number");
    return fromDouble(std::sqrt(v));
}

// ── Exponentiation (integer exponent) ───────────────────────────────

CASNumber CASNumber::pow(const CASNumber& base, int exp) {
    if (base.isError()) return base;
    if (exp == 0) return fromInt(1);
    if (exp == 1) return base;
    if (exp < 0) return div(fromInt(1), pow(base, -exp));

    // Rational base: use CASRational::pow (fast binary exponentiation)
    if (base.isRational()) {
        return fromRational(CASRational::pow(base._coeff, exp));
    }

    // General: repeated multiplication (small exponents expected)
    CASNumber result = base;
    for (int i = 1; i < exp && i < 100; ++i) {
        result = mul(result, base);
    }
    return result;
}

} // namespace cas
