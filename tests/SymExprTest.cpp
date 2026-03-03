/**
 * SymExprTest.cpp — Unit tests for SymExpr Phase 1.
 *
 * Tests:
 *   Arena  (4): allocation, reset, multi-block, stats
 *   SymNum (3): integer, fraction, pi/e constants
 *   SymVar (2): containsVar, evaluate
 *   SymNeg (2): evaluate negation, isPolynomial propagation
 *   SymAdd (3): binary sum eval, n-ary sum, toString
 *   SymMul (3): binary product eval, n-ary product, toString
 *   SymPow (3): integer power eval, isPolynomial checks, negative exp
 *   SymFunc(3): sin eval, cos eval, isPolynomial = false
 *   Clone  (2): deep copy correctness, clone independence
 *   toSymPoly (4): constant, linear, quadratic, cubic via pow+mul
 *   Mixed  (2): polynomial + transcendental detection, evaluate complex expr
 *
 * Total: 31 tests
 *
 * Usage: Call cas::runSymExprTests() from main.cpp when CAS_RUN_TESTS defined.
 */

#include "SymExprTest.h"
#include "../src/math/cas/SymExpr.h"
#include <cmath>
#include <cstdlib>   // abs on integer

#ifdef ARDUINO
  #include <Arduino.h>
  #include <esp_heap_caps.h>
  #define SE_PRINT(x)   Serial.print(x)
  #define SE_PRINTLN(x) Serial.println(x)
#else
  #include <cstdio>
  #define SE_PRINT(x)   printf("%s", std::string(x).c_str())
  #define SE_PRINTLN(x) printf("%s\n", std::string(x).c_str())
#endif

namespace cas {

static int _sePassed = 0;
static int _seFailed = 0;

static void seCheck(const char* name, bool condition) {
    if (condition) {
        _sePassed++;
        SE_PRINT("[PASS] ");
    } else {
        _seFailed++;
        SE_PRINT("[FAIL] ");
    }
    SE_PRINTLN(name);
}

/// Check double equality with tolerance
static bool approx(double a, double b, double tol = 1e-9) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
    return std::fabs(a - b) < tol;
}

void runSymExprTests() {
    _sePassed = 0;
    _seFailed = 0;

    SE_PRINTLN("\n════════════════════════════════════════════");
    SE_PRINTLN("  SymExpr Phase 1 — Unit Tests");
    SE_PRINTLN("════════════════════════════════════════════");

#ifdef ARDUINO
    size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    SE_PRINT("PSRAM free before: ");
    SE_PRINTLN(String(psramBefore).c_str());
#endif

    // ══════════════════════════════════════════════════════════════
    // Arena tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── Arena ──────────────────────────────────");
    {
        SymExprArena arena(1024);  // Small block for testing
        seCheck("Arena: blockCount == 1 after init",
                arena.blockCount() == 1);
        seCheck("Arena: totalAllocated == 1024",
                arena.totalAllocated() == 1024);

        // Allocate several nodes
        auto* n1 = arena.create<SymNum>(vpam::ExactVal::fromInt(42));
        auto* n2 = arena.create<SymVar>('x');
        seCheck("Arena: nodes not null",
                n1 != nullptr && n2 != nullptr);

        // Reset and verify
        arena.reset();
        seCheck("Arena: currentUsed == 0 after reset",
                arena.currentUsed() == 0);
    }

    // ══════════════════════════════════════════════════════════════
    // SymNum tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymNum ─────────────────────────────────");
    {
        SymExprArena arena;

        auto* n = symInt(arena, 7);
        seCheck("SymNum(7) evaluate", approx(n->evaluate(0), 7.0));

        auto* f = symFrac(arena, 1, 3);
        seCheck("SymNum(1/3) evaluate", approx(f->evaluate(0), 1.0/3.0));

        auto* pi = arena.create<SymNum>(vpam::ExactVal::fromPi(1, 1));
        seCheck("SymNum(pi) evaluate", approx(pi->evaluate(0), M_PI));
    }

    // ══════════════════════════════════════════════════════════════
    // SymVar tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymVar ─────────────────────────────────");
    {
        SymExprArena arena;

        auto* x = symVar(arena, 'x');
        seCheck("SymVar('x') evaluate(5)", approx(x->evaluate(5.0), 5.0));
        seCheck("SymVar('x') containsVar('x')", x->containsVar('x'));
    }

    // ══════════════════════════════════════════════════════════════
    // SymNeg tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymNeg ─────────────────────────────────");
    {
        SymExprArena arena;

        auto* neg = symNeg(arena, symInt(arena, 5));
        seCheck("SymNeg(5) evaluate == -5", approx(neg->evaluate(0), -5.0));
        seCheck("SymNeg(5) isPolynomial", neg->isPolynomial());
    }

    // ══════════════════════════════════════════════════════════════
    // SymAdd tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymAdd ─────────────────────────────────");
    {
        SymExprArena arena;

        // 3 + 4 = 7
        auto* sum = symAdd(arena, symInt(arena, 3), symInt(arena, 4));
        seCheck("SymAdd(3,4) evaluate == 7", approx(sum->evaluate(0), 7.0));

        // x + 2 + 5 = x + 7
        auto* sum3 = symAdd3(arena,
                             symVar(arena, 'x'),
                             symInt(arena, 2),
                             symInt(arena, 5));
        seCheck("SymAdd(x,2,5) evaluate(10) == 17",
                approx(sum3->evaluate(10.0), 17.0));
        seCheck("SymAdd toString contains '+'",
                sum3->toString().find('+') != std::string::npos);
    }

    // ══════════════════════════════════════════════════════════════
    // SymMul tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymMul ─────────────────────────────────");
    {
        SymExprArena arena;

        // 3 * 4 = 12
        auto* prod = symMul(arena, symInt(arena, 3), symInt(arena, 4));
        seCheck("SymMul(3,4) evaluate == 12", approx(prod->evaluate(0), 12.0));

        // 2 * x * 5 = 10x
        auto* prod3 = symMul3(arena,
                              symInt(arena, 2),
                              symVar(arena, 'x'),
                              symInt(arena, 5));
        seCheck("SymMul(2,x,5) evaluate(3) == 30",
                approx(prod3->evaluate(3.0), 30.0));
        seCheck("SymMul toString contains '*'",
                prod3->toString().find('*') != std::string::npos);
    }

    // ══════════════════════════════════════════════════════════════
    // SymPow tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymPow ─────────────────────────────────");
    {
        SymExprArena arena;

        // x^3 at x=2 → 8
        auto* pw = symPow(arena, symVar(arena, 'x'), symInt(arena, 3));
        seCheck("SymPow(x,3) evaluate(2) == 8",
                approx(pw->evaluate(2.0), 8.0));
        seCheck("SymPow(x,3) isPolynomial", pw->isPolynomial());

        // x^(-1) → NOT polynomial
        auto* pwNeg = symPow(arena, symVar(arena, 'x'), symInt(arena, -1));
        seCheck("SymPow(x,-1) NOT isPolynomial", !pwNeg->isPolynomial());
    }

    // ══════════════════════════════════════════════════════════════
    // SymFunc tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── SymFunc ────────────────────────────────");
    {
        SymExprArena arena;

        // sin(0) = 0
        auto* s = symFunc(arena, SymFuncKind::Sin, symInt(arena, 0));
        seCheck("sin(0) == 0", approx(s->evaluate(0), 0.0));

        // cos(0) = 1
        auto* c = symFunc(arena, SymFuncKind::Cos, symInt(arena, 0));
        seCheck("cos(0) == 1", approx(c->evaluate(0), 1.0));

        // sin(x) is NOT polynomial
        auto* sx = symFunc(arena, SymFuncKind::Sin, symVar(arena, 'x'));
        seCheck("sin(x) NOT isPolynomial", !sx->isPolynomial());
    }

    // ══════════════════════════════════════════════════════════════
    // Clone tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── Clone ──────────────────────────────────");
    {
        SymExprArena arena;

        // Build 3*x + sin(x)
        auto* expr = symAdd(arena,
            symMul(arena, symInt(arena, 3), symVar(arena, 'x')),
            symFunc(arena, SymFuncKind::Sin, symVar(arena, 'x')));

        // Clone into same arena
        auto* copy = expr->clone(arena);
        seCheck("Clone evaluate matches original",
                approx(expr->evaluate(1.0), copy->evaluate(1.0)));

        // They should be distinct objects
        seCheck("Clone is distinct pointer", expr != copy);
    }

    // ══════════════════════════════════════════════════════════════
    // toSymPoly tests
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── toSymPoly ──────────────────────────────");
    {
        SymExprArena arena;

        // Constant: 5 → SymPoly degree 0, coeff 5
        {
            auto* expr = symInt(arena, 5);
            auto poly = expr->toSymPoly('x');
            seCheck("toSymPoly(5): isConstant",
                    poly.isConstant());
            auto c = poly.coeffAtExact(0);
            seCheck("toSymPoly(5): coeff(0) == 5",
                    c.num == 5 && c.den == 1);
        }

        // Linear: 3x + 2
        {
            auto* expr = symAdd(arena,
                symMul(arena, symInt(arena, 3), symVar(arena, 'x')),
                symInt(arena, 2));
            auto poly = expr->toSymPoly('x');
            seCheck("toSymPoly(3x+2): degree == 1",
                    poly.degree() == 1);
            auto a = poly.coeffAtExact(1);
            auto b = poly.coeffAtExact(0);
            seCheck("toSymPoly(3x+2): coeffs correct",
                    a.num == 3 && a.den == 1 &&
                    b.num == 2 && b.den == 1);
        }

        // Quadratic: x^2 - 4
        {
            auto* expr = symAdd(arena,
                symPow(arena, symVar(arena, 'x'), symInt(arena, 2)),
                symNeg(arena, symInt(arena, 4)));
            auto poly = expr->toSymPoly('x');
            seCheck("toSymPoly(x^2-4): degree == 2",
                    poly.degree() == 2);
        }

        // Cubic via multiply: (x+1)(x-1)(x+2) = x³+2x²-x-2
        {
            // (x+1)
            auto* xp1 = symAdd(arena, symVar(arena, 'x'), symInt(arena, 1));
            // (x-1)
            auto* xm1 = symAdd(arena, symVar(arena, 'x'),
                                symNeg(arena, symInt(arena, 1)));
            // (x+2)
            auto* xp2 = symAdd(arena, symVar(arena, 'x'), symInt(arena, 2));
            // (x+1)(x-1)(x+2)
            auto* expr = symMul3(arena, xp1, xm1, xp2);

            seCheck("(x+1)(x-1)(x+2) isPolynomial", expr->isPolynomial());
            auto poly = expr->toSymPoly('x');
            seCheck("(x+1)(x-1)(x+2) degree == 3",
                    poly.degree() == 3);
            // Evaluate at x=0: should be (1)(-1)(2) = -2
            seCheck("(x+1)(x-1)(x+2) eval(0) == -2",
                    approx(expr->evaluate(0.0), -2.0));
        }
    }

    // ══════════════════════════════════════════════════════════════
    // Mixed polynomial + transcendental detection
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n── Mixed ──────────────────────────────────");
    {
        SymExprArena arena;

        // 3x^2 + sin(x) — NOT polynomial
        auto* mixed = symAdd(arena,
            symMul(arena, symInt(arena, 3),
                   symPow(arena, symVar(arena, 'x'), symInt(arena, 2))),
            symFunc(arena, SymFuncKind::Sin, symVar(arena, 'x')));
        seCheck("3x^2+sin(x) NOT isPolynomial", !mixed->isPolynomial());

        // Evaluate at x=pi/2: 3*(pi/2)^2 + sin(pi/2) = 3*(pi^2/4) + 1
        double expected = 3.0 * (M_PI/2.0) * (M_PI/2.0) + 1.0;
        seCheck("3x^2+sin(x) evaluate(pi/2)",
                approx(mixed->evaluate(M_PI/2.0), expected));
    }

    // ══════════════════════════════════════════════════════════════
    // Summary
    // ══════════════════════════════════════════════════════════════
    SE_PRINTLN("\n════════════════════════════════════════════");
    SE_PRINT("  SymExpr Tests: ");

    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d PASSED",
             _sePassed, _sePassed + _seFailed);
    SE_PRINTLN(buf);

    if (_seFailed > 0) {
        snprintf(buf, sizeof(buf), "  *** %d FAILED ***", _seFailed);
        SE_PRINTLN(buf);
    } else {
        SE_PRINTLN("  ALL TESTS PASSED");
    }
    SE_PRINTLN("════════════════════════════════════════════\n");

#ifdef ARDUINO
    size_t psramAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    SE_PRINT("PSRAM free after: ");
    SE_PRINTLN(String(psramAfter).c_str());
    SE_PRINT("PSRAM delta: ");
    SE_PRINTLN(String((int32_t)(psramAfter - psramBefore)).c_str());
#endif
}

} // namespace cas
