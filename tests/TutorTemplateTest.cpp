/**
 * TutorTemplateTest.cpp — Unit tests for the Educational Tutor Engine.
 */

#include "TutorTemplateTest.h"
#include "../src/math/cas/SymPoly.h"
#include "../src/math/cas/SymEquation.h"
#include "../src/math/cas/SingleSolver.h"
#include "../src/math/cas/SymExprArena.h"
#include <cmath>

#ifdef ARDUINO
  #include <Arduino.h>
  #define PRINT(x) Serial.print(x)
  #define PRINTLN(x) Serial.println(x)
#else
  #include <cstdio>
  #include <string>
  #define PRINT(x) printf("%s", std::string(x).c_str())
  #define PRINTLN(x) printf("%s\n", std::string(x).c_str())
#endif

namespace cas {

static int _passed = 0;
static int _failed = 0;

static void check(const char* name, bool condition) {
    if (condition) {
        _passed++;
        PRINT("[PASS] ");
    } else {
        _failed++;
        PRINT("[FAIL] ");
    }
    PRINTLN(name);
}

void runTutorTests() {
    _passed = 0;
    _failed = 0;

    PRINTLN("\n════════════════════════════════════════════");
    PRINTLN("  Tutor Engine — Unit Tests");
    PRINTLN("════════════════════════════════════════════");

    // ── Test 1: x² - 5x + 6 = 0 (Two real roots: 3, 2) ───────────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::variable('x', -5, 1, 1));  // -5x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(6))); // +6
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("QuadTutor x²-5x+6=0 ok", res.ok);
        check("QuadTutor x²-5x+6=0 count == 2", res.count == 2);
        
        double s1 = res.solutions[0].toDouble();
        double s2 = res.solutions[1].toDouble();
        double lo = (s1 < s2) ? s1 : s2;
        double hi = (s1 < s2) ? s2 : s1;
        check("QuadTutor x²-5x+6=0 root lo ≈ 2", std::abs(lo - 2.0) < 0.001);
        check("QuadTutor x²-5x+6=0 root hi ≈ 3", std::abs(hi - 3.0) < 0.001);
        
        // The tutor generates many more steps than the compact solver (~10 steps)
        check("QuadTutor x²-5x+6=0 has >= 8 steps", res.steps.count() >= 8);
        
        PRINTLN("  === Tutor Steps for x^2 - 5x + 6 = 0 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 2: x² - 6x + 9 = 0 (One real root: 3) ───────────────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::variable('x', -6, 1, 1));  // -6x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(9))); // +9
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("QuadTutor x²-6x+9=0 ok", res.ok);
        check("QuadTutor x²-6x+9=0 count == 1", res.count == 1);
        check("QuadTutor x²-6x+9=0 root ≈ 3", std::abs(res.solutions[0].toDouble() - 3.0) < 0.001);
        check("QuadTutor x²-6x+9=0 has >= 6 steps", res.steps.count() >= 6);
    }

    // ── Test 3: x² + 1 = 0 (Complex roots) ───────────────────────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(1))); // +1
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("QuadTutor x²+1=0 ok", res.ok);
        check("QuadTutor x²+1=0 count == 0", res.count == 0);
        check("QuadTutor hasComplexRoots", res.hasComplexRoots);
        
        PRINTLN("  === Tutor Steps for x^2 + 1 = 0 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 4: 3x² - 7x + 2 = 0 (Stress: Fractional root) ──────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 3, 1, 2));   // 3x²
        lhs.terms().push_back(SymTerm::variable('x', -7, 1, 1));  // -7x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(2))); // +2
        lhs.normalize();

        SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("Stress Quad 3x²-7x+2=0 ok", res.ok);
        check("Stress Quad count == 2", res.count == 2);
        // Roots: 2 and 1/3
        double r1 = res.solutions[0].toDouble();
        double r2 = res.solutions[1].toDouble();
        bool found2 = (std::abs(r1-2.0)<0.01 || std::abs(r2-2.0)<0.01);
        bool foundThird = (std::abs(r1-0.333)<0.01 || std::abs(r2-0.333)<0.01);
        check("Stress Quad found 2", found2);
        check("Stress Quad found 1/3", foundThird);
    }

    // ── Test 5: x³ - 6x² + 11x - 6 = 0 (Stress: Cubic/Ruffini) ──
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 3));   // x³
        lhs.terms().push_back(SymTerm::variable('x', -6, 1, 2));  // -6x²
        lhs.terms().push_back(SymTerm::variable('x', 11, 1, 1));  // 11x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-6))); // -6
        lhs.normalize();

        SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("Stress Cubic x³-6x²+11x-6=0 ok", res.ok);
        check("Stress Cubic count == 3", res.count == 3);
        // Roots: 1, 2, 3
        check("Cubic has Ruffini steps", res.steps.count() > 10);
    }

    // ── Test 6: 2x + 3y = 8, x - y = 1 (Stress: 2x2 Cramer) ──────
    {
        LinEq eq1 = LinEq::from2(2, 3, 8);
        LinEq eq2 = LinEq::from2(1, -1, 1);
        
        SymExprArena arena;
        SystemSolver solver;
        SystemResult res = solver.solve2x2(eq1, eq2, 'x', 'y', &arena);

        check("Stress 2x2 ok", res.ok);
        check("Stress 2x2 x ≈ 2.2", std::abs(res.solutions[0].toDouble() - 2.2) < 0.01);
        check("Stress 2x2 y ≈ 1.2", std::abs(res.solutions[1].toDouble() - 1.2) < 0.01);
        check("Stress 2x2 has Cramer steps", res.steps.count() >= 4);
    }

    PRINT("\nTotal Passed: "); PRINTLN(std::to_string(_passed).c_str());
    PRINT("Total Failed: "); PRINTLN(std::to_string(_failed).c_str());
}

} // namespace cas
