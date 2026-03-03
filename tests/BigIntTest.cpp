/**
 * BigIntTest.cpp — Phase 1 unit tests: CASInt hybrid BigInt + CASRational.
 *
 * Test suite (30 tests):
 *   A)  CASInt fast-path arithmetic (8 tests)
 *   B)  CASInt overflow / promotion / demotion (6 tests, ESP32-only promo)
 *   C)  CASInt large numbers: factorial, prime powers, GCD (6 tests)
 *   D)  CASRational arithmetic & normalisation (7 tests)
 *   E)  CASRational bridge, pow, error propagation (3 tests)
 *
 * Part of: NumOS Elite OmniCAS — Phase 1 (BigInt Hybrid Engine)
 */

#include "BigIntTest.h"
#include "../src/math/cas/CASInt.h"
#include "../src/math/cas/CASRational.h"

#include <cmath>
#include <cstdio>

#ifdef ARDUINO
  #include <Arduino.h>
  #define BI_PRINT(...)  Serial.printf(__VA_ARGS__)
#else
  #define BI_PRINT(...)  printf(__VA_ARGS__)
#endif

namespace cas {

// ── Test helpers ────────────────────────────────────────────────────

static int _biPass = 0;
static int _biFail = 0;

static void check(const char* name, bool cond) {
    if (cond) {
        _biPass++;
    } else {
        _biFail++;
        BI_PRINT("  [FAIL] %s\n", name);
    }
}

static bool approxEq(double a, double b, double tol = 1e-9) {
    return std::fabs(a - b) <= tol;
}

// ════════════════════════════════════════════════════════════════════
// A — CASInt fast-path arithmetic (int64 only)
// ════════════════════════════════════════════════════════════════════

static void testCASIntBasic() {
    // T1: Default construction is zero
    CASInt z;
    check("T01 CASInt default = 0", z.isZero() && !z.hasError());

    // T2: fromInt + toInt64
    CASInt a = CASInt::fromInt(42);
    check("T02 CASInt fromInt(42)", a.toInt64() == 42);

    // T3: add
    CASInt b = CASInt::fromInt(100);
    CASInt c = CASInt::add(a, b);
    check("T03 CASInt add(42,100)=142", c.toInt64() == 142);

    // T4: sub
    CASInt d = CASInt::sub(b, a);
    check("T04 CASInt sub(100,42)=58", d.toInt64() == 58);

    // T5: mul
    CASInt e = CASInt::mul(a, CASInt::fromInt(3));
    check("T05 CASInt mul(42,3)=126", e.toInt64() == 126);

    // T6: div
    CASInt f = CASInt::div(b, CASInt::fromInt(4));
    check("T06 CASInt div(100,4)=25", f.toInt64() == 25);

    // T7: neg
    CASInt g = CASInt::neg(a);
    check("T07 CASInt neg(42)=-42", g.toInt64() == -42 && g.isNegative());

    // T8: absVal
    CASInt h = CASInt::absVal(g);
    check("T08 CASInt abs(-42)=42", h.toInt64() == 42 && !h.isNegative());
}

// ════════════════════════════════════════════════════════════════════
// B — CASInt overflow detection, promotion, demotion
// ════════════════════════════════════════════════════════════════════

static void testCASIntOverflow() {
    // T9: Division by zero → error
    CASInt z;
    CASInt err = CASInt::div(CASInt::fromInt(5), z);
    check("T09 CASInt div-by-zero = error", err.hasError());

    // T10: Add of error propagates
    CASInt ok  = CASInt::fromInt(1);
    CASInt err2 = CASInt::add(err, ok);
    check("T10 CASInt error propagation", err2.hasError());

    // T11: INT64_MAX + 1 triggers overflow
    CASInt maxVal = CASInt::fromInt(INT64_MAX);
    CASInt one    = CASInt::fromInt(1);
    CASInt overflowed = CASInt::add(maxVal, one);
#if CAS_HAS_BIGINT
    // On ESP32: should promote to BigInt and succeed
    check("T11 INT64_MAX+1 promoted", overflowed.isPromoted() && !overflowed.hasError());
#else
    // On native (no BigInt): returns error
    check("T11 INT64_MAX+1 error (native)", overflowed.hasError());
#endif

    // T12: INT64_MIN - 1 triggers underflow
    CASInt minVal   = CASInt::fromInt(INT64_MIN);
    CASInt minUnder = CASInt::sub(minVal, one);
#if CAS_HAS_BIGINT
    check("T12 INT64_MIN-1 promoted", minUnder.isPromoted() && !minUnder.hasError());
#else
    check("T12 INT64_MIN-1 error (native)", minUnder.hasError());
#endif

    // T13: Large mul overflow
    CASInt large = CASInt::fromInt(INT64_MAX / 2 + 1);
    CASInt mulOver = CASInt::mul(large, CASInt::fromInt(3));
#if CAS_HAS_BIGINT
    check("T13 large mul promoted", mulOver.isPromoted() && !mulOver.hasError());
#else
    check("T13 large mul error (native)", mulOver.hasError());
#endif

    // T14: Result that fits back in int64 demotes after add
    // Add two small numbers via BigInt path (if available) → stays small
    CASInt s1 = CASInt::fromInt(1000000000LL);
    CASInt s2 = CASInt::fromInt(2000000000LL);
    CASInt s3 = CASInt::add(s1, s2);
    // Result 3e9 fits in int64
    check("T14 small add stays int64", s3.toInt64() == 3000000000LL && !s3.isPromoted());
}

// ════════════════════════════════════════════════════════════════════
// C — CASInt: factorials, prime powers, GCD
// ════════════════════════════════════════════════════════════════════

static CASInt factorial(int n) {
    CASInt result = CASInt::fromInt(1);
    for (int i = 2; i <= n; ++i) {
        result = CASInt::mul(result, CASInt::fromInt(i));
    }
    return result;
}

static void testCASIntLarge() {
    // T15: 10! = 3628800
    CASInt f10 = factorial(10);
    check("T15 10! = 3628800", f10.toInt64() == 3628800LL && !f10.hasError());

    // T16: 20! = 2432902008176640000 (fits in int64, 63 bits)
    CASInt f20 = factorial(20);
    check("T16 20! = 2432902008176640000", f20.toInt64() == 2432902008176640000LL && !f20.hasError());

    // T17: 25! overflows int64 (> 2^63)
    CASInt f25 = factorial(25);
#if CAS_HAS_BIGINT
    // Promoted: exact, not error
    check("T17 25! promoted and correct double",
          f25.isPromoted() && !f25.hasError() &&
          approxEq(f25.toDouble(), 15511210043330985984000000.0, 1e10));
#else
    check("T17 25! error (native fallback)", f25.hasError());
#endif

    // T18: GCD(48, 18) = 6
    CASInt gcd1 = CASInt::gcd(CASInt::fromInt(48), CASInt::fromInt(18));
    check("T18 gcd(48,18)=6", gcd1.toInt64() == 6);

    // T19: GCD(100, 75) = 25
    CASInt gcd2 = CASInt::gcd(CASInt::fromInt(100), CASInt::fromInt(75));
    check("T19 gcd(100,75)=25", gcd2.toInt64() == 25);

    // T20: 2^40 = 1099511627776 (fits in int64)
    CASInt pow2_40 = CASInt::fromInt(1);
    CASInt two     = CASInt::fromInt(2);
    for (int i = 0; i < 40; ++i)
        pow2_40 = CASInt::mul(pow2_40, two);
    check("T20 2^40 = 1099511627776", pow2_40.toInt64() == 1099511627776LL && !pow2_40.hasError());
}

// ════════════════════════════════════════════════════════════════════
// D — CASRational arithmetic & normalisation
// ════════════════════════════════════════════════════════════════════

static void testCASRationalArith() {
    // T21: fromFrac normalises: 6/4 → 3/2
    CASRational r1 = CASRational::fromFrac(6, 4);
    check("T21 6/4 normalises to 3/2",
          r1.num().toInt64() == 3 && r1.den().toInt64() == 2);

    // T22: negative denominator → den stays positive: 3/-4 → -3/4
    CASRational r2 = CASRational::fromFrac(3, -4);
    check("T22 3/-4 → num=-3 den=4",
          r2.num().toInt64() == -3 && r2.den().toInt64() == 4);

    // T23: add 1/3 + 1/6 = 3/6 = 1/2
    CASRational a = CASRational::fromFrac(1, 3);
    CASRational b = CASRational::fromFrac(1, 6);
    CASRational sum = CASRational::add(a, b);
    check("T23 1/3 + 1/6 = 1/2",
          sum.num().toInt64() == 1 && sum.den().toInt64() == 2);

    // T24: sub 3/4 - 1/4 = 1/2
    CASRational c = CASRational::fromFrac(3, 4);
    CASRational d = CASRational::fromFrac(1, 4);
    CASRational diff = CASRational::sub(c, d);
    check("T24 3/4 - 1/4 = 1/2",
          diff.num().toInt64() == 1 && diff.den().toInt64() == 2);

    // T25: mul 2/3 * 3/4 = 6/12 = 1/2
    CASRational e = CASRational::fromFrac(2, 3);
    CASRational f = CASRational::fromFrac(3, 4);
    CASRational prod = CASRational::mul(e, f);
    check("T25 2/3 * 3/4 = 1/2",
          prod.num().toInt64() == 1 && prod.den().toInt64() == 2);

    // T26: div 5/6 / 5/3 = 5/6 * 3/5 = 15/30 = 1/2
    CASRational g = CASRational::fromFrac(5, 6);
    CASRational h = CASRational::fromFrac(5, 3);
    CASRational quot = CASRational::div(g, h);
    check("T26 (5/6)/(5/3) = 1/2",
          quot.num().toInt64() == 1 && quot.den().toInt64() == 2);

    // T27: isZero / isOne
    CASRational zero = CASRational::zero();
    CASRational one  = CASRational::one();
    check("T27 zero/one predicates",
          zero.isZero() && !zero.isOne() && one.isOne() && !one.isZero());
}

// ════════════════════════════════════════════════════════════════════
// E — CASRational: bridge toExactVal, pow, error propagation
// ════════════════════════════════════════════════════════════════════

static void testCASRationalSpecial() {
    // T28: toExactVal bridge round-trip
    CASRational r = CASRational::fromFrac(3, 7);
    vpam::ExactVal ev = r.toExactVal();
    check("T28 toExactVal 3/7 bridge",
          ev.ok && ev.num == 3 && ev.den == 7);

    // T29: pow(2/3, 3) = 8/27
    CASRational base = CASRational::fromFrac(2, 3);
    CASRational p3   = CASRational::pow(base, 3);
    check("T29 (2/3)^3 = 8/27",
          p3.num().toInt64() == 8 && p3.den().toInt64() == 27 && !p3.hasError());

    // T30: error propagation through arithmetic
    CASRational err = CASRational::makeError();
    CASRational ok  = CASRational::fromFrac(1, 2);
    CASRational r2  = CASRational::add(err, ok);
    CASRational r3  = CASRational::mul(ok, err);
    check("T30 error propagates through add/mul",
          r2.hasError() && r3.hasError());
}

// ════════════════════════════════════════════════════════════════════
// Public runner
// ════════════════════════════════════════════════════════════════════

void runBigIntTests() {
    _biPass = 0;
    _biFail = 0;

    BI_PRINT("\n--- Phase 1: BigInt Hybrid Engine Tests ---\n");

    BI_PRINT(" A) CASInt fast-path\n");
    testCASIntBasic();

    BI_PRINT(" B) CASInt overflow/promotion\n");
    testCASIntOverflow();

    BI_PRINT(" C) CASInt large numbers\n");
    testCASIntLarge();

    BI_PRINT(" D) CASRational arithmetic\n");
    testCASRationalArith();

    BI_PRINT(" E) CASRational bridge/pow/error\n");
    testCASRationalSpecial();

    BI_PRINT("--- BigIntTest: %d/%d passed ---\n\n",
             _biPass, _biPass + _biFail);
}

} // namespace cas
