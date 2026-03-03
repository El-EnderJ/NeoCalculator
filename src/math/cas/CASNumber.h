/**
 * CASNumber.h — Unified numeric type for CAS-S3-ULTRA engine.
 *
 * Manages three precision states automatically:
 *
 *   Form::Rational   (num/den via CASRational)
 *     Hot path — exact integer and fraction arithmetic.  CASRational
 *     internally uses CASInt, which auto-promotes to mbedtls_mpi BigInt
 *     on 64-bit overflow.  Handles 10^100 without loss.
 *
 *   Form::Radical    (coeff * sqrt(radicand))
 *     Exact irrational — the quadratic formula's discriminant path.
 *     The coefficient is CASRational (overflow-safe), the radicand is
 *     CASInt.  Square factors are extracted automatically so that
 *     sqrt(72) = 6*sqrt(2), not left unsimplified.
 *
 *   Form::Floating   (IEEE 754 double)
 *     Fallback for transcendentals, mixed-radical addition, and
 *     any operation that cannot be represented exactly.
 *
 * Bridge:
 *   toExactVal() converts to the legacy vpam::ExactVal format for the
 *   existing rendering pipeline (MathCanvas, SymExprToAST, SymToAST).
 *   When CASInt is promoted (BigInt), falls back to approximate double.
 *
 * Memory:
 *   sizeof(CASNumber) ≈ 80 bytes on ESP32-S3 (CASRational + CASInt +
 *   double + Form tag + string).  Passed by value in hot loops but
 *   by const-ref in solver APIs.
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 1 (Infinite Precision)
 */

#pragma once

#include "CASInt.h"
#include "CASRational.h"
#include "../ExactVal.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>   // std::pair

namespace cas {

class CASNumber {
public:
    // ════════════════════════════════════════════════════════════════
    // Form — The precision state of this number
    // ════════════════════════════════════════════════════════════════

    enum class Form : uint8_t {
        Rational,   ///< Exact fraction: num/den (CASRational, auto-BigInt)
        Radical,    ///< Exact irrational: coeff * sqrt(radicand)
        Floating,   ///< IEEE 754 double (transcendentals, fallback)
        Error       ///< Invalid / error state
    };

    // ════════════════════════════════════════════════════════════════
    // Construction
    // ════════════════════════════════════════════════════════════════

    CASNumber() : _form(Form::Rational), _coeff(), _radicand(1), _dbl(0.0) {}

    static CASNumber fromInt(int64_t n);
    static CASNumber fromFrac(int64_t n, int64_t d);
    static CASNumber fromRational(const CASRational& q);
    static CASNumber fromDouble(double v);
    static CASNumber fromRadical(const CASRational& coeff, const CASInt& radicand);
    static CASNumber fromExactVal(const vpam::ExactVal& ev);
    static CASNumber makeError(const std::string& msg);

    // ════════════════════════════════════════════════════════════════
    // Queries
    // ════════════════════════════════════════════════════════════════

    Form form() const { return _form; }

    bool isExact()    const { return _form == Form::Rational || _form == Form::Radical; }
    bool isRational() const { return _form == Form::Rational; }
    bool isRadical()  const { return _form == Form::Radical; }
    bool isFloating() const { return _form == Form::Floating; }
    bool isError()    const { return _form == Form::Error; }

    bool isZero() const;
    bool isNegative() const;
    bool isOne() const;

    /// Access the rational coefficient (Rational and Radical forms).
    const CASRational& coeff() const { return _coeff; }

    /// Access the radicand (Radical form; 1 otherwise).
    const CASInt& radicand() const { return _radicand; }

    /// Access the double value (Floating form; 0.0 otherwise).
    double doubleVal() const { return _dbl; }

    /// Access the error message (Error form; empty otherwise).
    const std::string& errorMsg() const { return _error; }

    // ════════════════════════════════════════════════════════════════
    // Conversion
    // ════════════════════════════════════════════════════════════════

    /// Convert to double (always valid, may lose precision).
    double toDouble() const;

    /// Bridge to legacy vpam::ExactVal for the rendering pipeline.
    vpam::ExactVal toExactVal() const;

    /// Human-readable display string (for step logging).
    std::string toString() const;

    // ════════════════════════════════════════════════════════════════
    // Arithmetic — static functions, return new CASNumber by value.
    //
    // Precision rules:
    //   Rational ⊕ Rational  → Rational   (CASRational, BigInt on overflow)
    //   Rational × Radical   → Radical    (scale coefficient)
    //   Radical  × Radical   → simplified  (extract square factors)
    //   sqrt(Rational)       → Rational if perfect square, else Radical
    //   Anything + Error     → Error
    //   Incompatible forms   → Floating (double fallback)
    // ════════════════════════════════════════════════════════════════

    static CASNumber add(const CASNumber& a, const CASNumber& b);
    static CASNumber sub(const CASNumber& a, const CASNumber& b);
    static CASNumber mul(const CASNumber& a, const CASNumber& b);
    static CASNumber div(const CASNumber& a, const CASNumber& b);
    static CASNumber neg(const CASNumber& a);
    static CASNumber abs(const CASNumber& a);
    static CASNumber sqrt(const CASNumber& a);
    static CASNumber pow(const CASNumber& base, int exp);

private:
    Form        _form;
    CASRational _coeff;      // Coefficient (Rational and Radical forms)
    CASInt      _radicand;   // Radicand > 1 for Radical form (default 1)
    double      _dbl;        // Value for Floating form
    std::string _error;      // Message for Error form

    // ── Internal helpers ────────────────────────────────────────────

    /// Extract largest perfect square factor: n = k² * m.
    /// Returns {k, m} where m is square-free.
    struct SquareFactors { CASInt k; CASInt m; };
    static SquareFactors extractSquareFactor(const CASInt& n);

    /// Simplify coeff * sqrt(radicand) by extracting square factors.
    static CASNumber simplifyRadical(const CASRational& coeff, const CASInt& radicand);
};

} // namespace cas
