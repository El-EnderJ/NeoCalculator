/**
 * OmniSolverTest.cpp — Phase 4 unit tests.
 *
 * Tests:
 *   A) Classification — verify equation classification (3 tests)
 *   B) Polynomial path — x²-2=0 → x=±√2 via SingleSolver (2 tests)
 *   C) Inverse path — ln(x)=1 → x=e via algebraic inverse (2 tests)
 *   D) HybridNewton — e^x+x=5 → numeric root via exact Jacobian (3 tests)
 *   E) Integration — OmniSolver end-to-end (3 tests)
 *
 * Total: 13 tests
 *
 * Convention:
 *   · Build SymExpr trees with arena factory helpers
 *   · Solve, then verify solutions numerically
 *   · arena.reset() between test groups
 *
 * Part of: NumOS Pro-CAS — Phase 4 (Omni-Solver)
 */

#include "OmniSolverTest.h"
#include "../src/math/cas/SymExpr.h"
#include "../src/math/cas/SymExprArena.h"
#include "../src/math/cas/OmniSolver.h"
#include "../src/math/cas/HybridNewton.h"

#include <cmath>
#include <cstdio>
#include <string>

// ── Platform-agnostic print macros ──────────────────────────────────
#ifdef ARDUINO
  #include <Arduino.h>
  #define OS_PRINT(...)   Serial.printf(__VA_ARGS__)
  #define OS_PRINTLN(s)   Serial.println(s)
#else
  #define OS_PRINT(...)   printf(__VA_ARGS__)
  #define OS_PRINTLN(s)   printf("%s\n", s)
#endif

namespace cas {

static int _osPass = 0;
static int _osFail = 0;

static void check(const char* name, bool cond) {
    if (cond) {
        _osPass++;
    } else {
        _osFail++;
        OS_PRINT("  FAIL: %s\n", name);
    }
}

static bool approx(double a, double b, double tol = 1e-6) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
    return std::fabs(a - b) < tol;
}

// Helper: check if a solution vector contains a value near `target`
static bool hasSolutionNear(const OmniResult& r, double target, double tol = 1e-6) {
    for (const auto& sol : r.solutions) {
        if (std::fabs(sol.numeric - target) < tol) return true;
    }
    return false;
}

void runOmniSolverTests() {
    _osPass = 0;
    _osFail = 0;

    OS_PRINTLN("\n══════════ Phase 4: OmniSolver/HybridNewton Tests ══════════");

    SymExprArena arena;

    // ════════════════════════════════════════════════════════════════
    // A) Classification tests
    // ════════════════════════════════════════════════════════════════
    OS_PRINTLN("\n── A) Classification ──");

    // A1: x² - 2 → Polynomial
    {
        // x^2 + (-2) = x^2 - 2
        auto* expr = symAdd(arena,
            symPow(arena, symVar(arena, 'x'), symInt(arena, 2)),
            symInt(arena, -2));
        EquationClass cls = OmniSolver::classify(expr, 'x');
        check("A1 x^2-2 → Polynomial", cls == EquationClass::Polynomial);
        arena.reset();
    }

    // A2: ln(x) + (-1) → SimpleInverse
    {
        auto* expr = symAdd(arena,
            symFunc(arena, SymFuncKind::Ln, symVar(arena, 'x')),
            symInt(arena, -1));
        EquationClass cls = OmniSolver::classify(expr, 'x');
        check("A2 ln(x)-1 → SimpleInverse", cls == EquationClass::SimpleInverse);
        arena.reset();
    }

    // A3: e^x + x + (-5) → MixedTranscendental
    {
        auto* expr = symAdd3(arena,
            symFunc(arena, SymFuncKind::Exp, symVar(arena, 'x')),
            symVar(arena, 'x'),
            symInt(arena, -5));
        EquationClass cls = OmniSolver::classify(expr, 'x');
        check("A3 e^x+x-5 → MixedTranscendental",
              cls == EquationClass::MixedTranscendental);
        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // B) Polynomial path: x² - 2 = 0 → x = ±√2
    // ════════════════════════════════════════════════════════════════
    OS_PRINTLN("\n── B) Polynomial: x^2 - 2 = 0 ──");

    {
        // f(x) = x^2 - 2   (solve f(x) = 0)
        auto* f = symAdd(arena,
            symPow(arena, symVar(arena, 'x'), symInt(arena, 2)),
            symInt(arena, -2));

        OmniSolver solver;
        OmniResult r = solver.solveExpr(f, 'x', arena);

        check("B1 polynomial solve ok", r.ok);
        check("B2 two solutions", r.solutions.size() == 2);

        if (r.solutions.size() == 2) {
            double sqrt2 = std::sqrt(2.0);
            bool hasPos = hasSolutionNear(r, sqrt2);
            bool hasNeg = hasSolutionNear(r, -sqrt2);
            check("B3 has +sqrt(2)", hasPos);
            check("B4 has -sqrt(2)", hasNeg);
        }

        // Log steps
        OS_PRINT("  Classification: %s\n", equationClassName(r.classification));
        OS_PRINT("  Solutions: %d\n", (int)r.solutions.size());
        for (size_t i = 0; i < r.solutions.size(); ++i) {
            OS_PRINT("    x%d = %.10g\n", (int)(i+1), r.solutions[i].numeric);
        }

        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // C) Inverse path: ln(x) = 1 → x = e
    // ════════════════════════════════════════════════════════════════
    OS_PRINTLN("\n── C) Inverse: ln(x) = 1 ──");

    {
        // Equation: ln(x) = 1  →  normalize to ln(x) - 1 = 0
        auto* lhs = symFunc(arena, SymFuncKind::Ln, symVar(arena, 'x'));
        auto* rhs = symInt(arena, 1);

        OmniSolver solver;
        OmniResult r = solver.solve(lhs, rhs, 'x', arena);

        check("C1 inverse solve ok", r.ok);
        check("C2 classification = SimpleInverse",
              r.classification == EquationClass::SimpleInverse);

        if (!r.solutions.empty()) {
            double expected = std::exp(1.0);  // e ≈ 2.71828
            check("C3 solution ≈ e",
                  approx(r.solutions[0].numeric, expected, 1e-8));
            OS_PRINT("  x = %.10g (expected %.10g)\n",
                     r.solutions[0].numeric, expected);
        } else {
            check("C3 solution ≈ e (no solutions found)", false);
        }

        // Log steps
        OS_PRINT("  Classification: %s\n", equationClassName(r.classification));
        for (const auto& step : r.steps.steps()) {
            OS_PRINT("  Step: %s\n", step.description.c_str());
        }

        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // D) HybridNewton direct: e^x + x - 5 = 0
    // ════════════════════════════════════════════════════════════════
    OS_PRINTLN("\n── D) HybridNewton: e^x + x - 5 = 0 ──");

    {
        // f(x) = e^x + x - 5
        auto* f = symAdd3(arena,
            symFunc(arena, SymFuncKind::Exp, symVar(arena, 'x')),
            symVar(arena, 'x'),
            symInt(arena, -5));

        CASStepLogger log;
        NewtonResult nr = HybridNewton::solve(f, 'x', arena, &log, 4);

        check("D1 HybridNewton ok", nr.ok);
        check("D2 at least 1 root", !nr.roots.empty());

        if (!nr.roots.empty()) {
            // Verify: e^x + x = 5 at the root
            double root = nr.roots[0].value;
            double residual = std::exp(root) + root - 5.0;
            check("D3 |f(root)| < 1e-8", std::fabs(residual) < 1e-8);
            OS_PRINT("  root = %.12g, |f(root)| = %.2e, iters = %d\n",
                     root, std::fabs(residual), nr.roots[0].iterations);
        }

        // Print logged steps
        for (const auto& step : log.steps()) {
            OS_PRINT("  Step: %s\n", step.description.c_str());
        }

        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // E) OmniSolver end-to-end: e^x + x = 5 (via solve(lhs, rhs))
    // ════════════════════════════════════════════════════════════════
    OS_PRINTLN("\n── E) OmniSolver E2E: e^x + x = 5 ──");

    {
        // lhs = e^x + x, rhs = 5
        auto* lhs = symAdd(arena,
            symFunc(arena, SymFuncKind::Exp, symVar(arena, 'x')),
            symVar(arena, 'x'));
        auto* rhs = symInt(arena, 5);

        OmniSolver solver;
        OmniResult r = solver.solve(lhs, rhs, 'x', arena);

        check("E1 OmniSolver e^x+x=5 ok", r.ok);
        check("E2 classification = MixedTranscendental",
              r.classification == EquationClass::MixedTranscendental);

        if (!r.solutions.empty()) {
            double root = r.solutions[0].numeric;
            double residual = std::exp(root) + root - 5.0;
            check("E3 |f(root)| < 1e-8", std::fabs(residual) < 1e-8);
            OS_PRINT("  x = %.12g, residual = %.2e\n",
                     root, std::fabs(residual));
        } else {
            check("E3 solution found", false);
        }

        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // Summary
    // ════════════════════════════════════════════════════════════════
    OS_PRINT("\n══════════ Phase 4 Results: %d passed, %d failed ══════════\n\n",
             _osPass, _osFail);
}

} // namespace cas
