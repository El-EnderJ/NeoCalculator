/**
 * SymDiffTest.cpp — Phase 3 unit tests.
 *
 * Tests:
 *   A) SymDiff — symbolic differentiation (17 tests)
 *   B) SymSimplify — algebraic simplifier (10 tests)
 *   C) SymExprToAST — SymExpr → MathAST conversion (5 tests)
 *
 * Total: 32 tests
 *
 * Convention:
 *   · Build SymExpr trees with arena factory helpers
 *   · Differentiate, simplify, then verify via evaluate()
 *   · Verify AST conversion produces correct node types
 *
 * Part of: NumOS Pro-CAS — Phase 3 (Differentiation & Simplifier)
 */

#include "SymDiffTest.h"
#include "../src/math/cas/SymExpr.h"
#include "../src/math/cas/SymExprArena.h"
#include "../src/math/cas/SymDiff.h"
#include "../src/math/cas/SymSimplify.h"
#include "../src/math/cas/SymExprToAST.h"
#include "../src/math/MathAST.h"

#include <cmath>
#include <cstdio>
#include <string>

// ── Platform-agnostic print macros ──────────────────────────────────
#ifdef ARDUINO
  #include <Arduino.h>
  #define SD_PRINT(...)   Serial.printf(__VA_ARGS__)
  #define SD_PRINTLN(s)   Serial.println(s)
#else
  #define SD_PRINT(...)   printf(__VA_ARGS__)
  #define SD_PRINTLN(s)   printf("%s\n", s)
#endif

namespace cas {

static int _sdPass = 0;
static int _sdFail = 0;

static void check(const char* name, bool cond) {
    if (cond) {
        _sdPass++;
    } else {
        _sdFail++;
        SD_PRINT("  FAIL: %s\n", name);
    }
}

static bool approx(double a, double b, double tol = 1e-6) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
    return std::fabs(a - b) < tol;
}

// ════════════════════════════════════════════════════════════════════
// A) SymDiff Tests
// ════════════════════════════════════════════════════════════════════

// Helper: differentiate, simplify, and evaluate at x = val
static double diffEval(SymExpr* expr, char var, SymExprArena& arena, double val) {
    SymExpr* d = SymDiff::diff(expr, var, arena);
    if (!d) return NAN;
    SymExpr* s = SymSimplify::simplify(d, arena);
    return s->evaluate(val);
}

void runSymDiffTests() {
    _sdPass = 0;
    _sdFail = 0;

    SD_PRINTLN("\n══════════ Phase 3: SymDiff/Simplify/ToAST Tests ══════════");

    SymExprArena arena;

    // ────────────────────────────────────────────────────────────────
    // A1: d/dx(5) = 0
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symInt(arena, 5);
        SymExpr* d = SymDiff::diff(expr, 'x', arena);
        check("A1 d/dx(5) != null", d != nullptr);
        if (d) check("A1 d/dx(5) = 0", approx(d->evaluate(1.0), 0.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A2: d/dx(x) = 1
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symVar(arena, 'x');
        SymExpr* d = SymDiff::diff(expr, 'x', arena);
        check("A2 d/dx(x) != null", d != nullptr);
        if (d) check("A2 d/dx(x) = 1", approx(d->evaluate(3.0), 1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A3: d/dx(y) = 0 (independent variable)
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symVar(arena, 'y');
        SymExpr* d = SymDiff::diff(expr, 'x', arena);
        check("A3 d/dx(y) != null", d != nullptr);
        if (d) check("A3 d/dx(y) = 0", approx(d->evaluate(5.0), 0.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A4: d/dx(-x) = -1
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symNeg(arena, symVar(arena, 'x'));
        double dv = diffEval(expr, 'x', arena, 2.0);
        check("A4 d/dx(-x) = -1", approx(dv, -1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A5: d/dx(x + 3) = 1
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symAdd(arena, symVar(arena, 'x'), symInt(arena, 3));
        double dv = diffEval(expr, 'x', arena, 7.0);
        check("A5 d/dx(x+3) = 1", approx(dv, 1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A6: d/dx(x²) = 2x  → at x=3: 6
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symPow(arena, x, symInt(arena, 2));
        double dv = diffEval(expr, 'x', arena, 3.0);
        check("A6 d/dx(x²) at x=3 = 6", approx(dv, 6.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A7: d/dx(x³) = 3x²  → at x=2: 12
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symPow(arena, x, symInt(arena, 3));
        double dv = diffEval(expr, 'x', arena, 2.0);
        check("A7 d/dx(x³) at x=2 = 12", approx(dv, 12.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A8: d/dx(3x²) = 6x  → at x=4: 24
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symMul(arena,
            symInt(arena, 3),
            symPow(arena, x, symInt(arena, 2)));
        double dv = diffEval(expr, 'x', arena, 4.0);
        check("A8 d/dx(3x²) at x=4 = 24", approx(dv, 24.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A9: d/dx(sin(x)) = cos(x)  → at x=0: 1
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Sin, x);
        double dv = diffEval(expr, 'x', arena, 0.0);
        check("A9 d/dx(sin(x)) at x=0 = 1", approx(dv, 1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A10: d/dx(cos(x)) = -sin(x)  → at x=π/2: -1
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Cos, x);
        double dv = diffEval(expr, 'x', arena, M_PI / 2.0);
        check("A10 d/dx(cos(x)) at π/2 = -1", approx(dv, -1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A11: d/dx(ln(x)) = 1/x  → at x=2: 0.5
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Ln, x);
        double dv = diffEval(expr, 'x', arena, 2.0);
        check("A11 d/dx(ln(x)) at x=2 = 0.5", approx(dv, 0.5));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A12: d/dx(e^x) = e^x  → at x=1: e
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Exp, x);
        double dv = diffEval(expr, 'x', arena, 1.0);
        check("A12 d/dx(e^x) at x=1 = e", approx(dv, M_E));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A13: d/dx(tan(x)) = sec²(x) = 1/cos²(x)  → at x=0: 1
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Tan, x);
        double dv = diffEval(expr, 'x', arena, 0.0);
        check("A13 d/dx(tan(x)) at x=0 = 1", approx(dv, 1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A14: d/dx(sin(x²)) = 2x·cos(x²)  → chain rule
    //      at x=1: 2·cos(1) ≈ 1.0806
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Sin,
            symPow(arena, x, symInt(arena, 2)));
        double dv = diffEval(expr, 'x', arena, 1.0);
        double expected = 2.0 * cos(1.0);
        check("A14 d/dx(sin(x²)) at x=1", approx(dv, expected));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A15: d/dx(x·ln(x)) = ln(x) + 1  → product rule
    //      at x=e: ln(e)+1 = 2
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symMul(arena,
            x,
            symFunc(arena, SymFuncKind::Ln, x));
        double dv = diffEval(expr, 'x', arena, M_E);
        check("A15 d/dx(x·ln(x)) at x=e = 2", approx(dv, 2.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A16: d/dx(2^x) = 2^x · ln(2)  → exponential rule
    //      at x=3: 8 · ln(2) ≈ 5.5452
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symPow(arena, symInt(arena, 2), x);
        double dv = diffEval(expr, 'x', arena, 3.0);
        double expected = 8.0 * log(2.0);
        check("A16 d/dx(2^x) at x=3", approx(dv, expected));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A17: d/dx(x^x) = x^x · (ln(x) + 1)  → general power
    //      at x=2: 4·(ln(2)+1) ≈ 6.7726
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symPow(arena, x, x);
        double dv = diffEval(expr, 'x', arena, 2.0);
        double expected = 4.0 * (log(2.0) + 1.0);
        check("A17 d/dx(x^x) at x=2", approx(dv, expected));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A18: d/dx(arcsin(x)) = 1/√(1-x²)  → at x=0: 1
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::ArcSin, x);
        double dv = diffEval(expr, 'x', arena, 0.0);
        check("A18 d/dx(arcsin(x)) at x=0 = 1", approx(dv, 1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A19: d/dx(arctan(x)) = 1/(1+x²)  → at x=1: 0.5
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::ArcTan, x);
        double dv = diffEval(expr, 'x', arena, 1.0);
        check("A19 d/dx(arctan(x)) at x=1 = 0.5", approx(dv, 0.5));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // A20: d/dx(log10(x)) = 1/(x·ln(10))  → at x=10: 1/(10·ln(10))
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symFunc(arena, SymFuncKind::Log10, x);
        double dv = diffEval(expr, 'x', arena, 10.0);
        double expected = 1.0 / (10.0 * log(10.0));
        check("A20 d/dx(log10(x)) at x=10", approx(dv, expected));
        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // B) SymSimplify Tests
    // ════════════════════════════════════════════════════════════════

    // ────────────────────────────────────────────────────────────────
    // B1: 0 + x → x
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symAdd(arena,
            symInt(arena, 0),
            symVar(arena, 'x'));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B1 0+x → x type == Var", s->type == SymExprType::Var);
        check("B1 0+x → x eval", approx(s->evaluate(7.0), 7.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B2: x + 0 → x
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symAdd(arena,
            symVar(arena, 'x'),
            symInt(arena, 0));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B2 x+0 → x type == Var", s->type == SymExprType::Var);
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B3: 1 · x → x
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symMul(arena,
            symInt(arena, 1),
            symVar(arena, 'x'));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B3 1·x → x type == Var", s->type == SymExprType::Var);
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B4: 0 · x → 0
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symMul(arena,
            symInt(arena, 0),
            symVar(arena, 'x'));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B4 0·x → 0 type == Num", s->type == SymExprType::Num);
        check("B4 0·x → 0 eval", approx(s->evaluate(99.0), 0.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B5: x^1 → x
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symPow(arena,
            symVar(arena, 'x'),
            symInt(arena, 1));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B5 x^1 → x type == Var", s->type == SymExprType::Var);
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B6: x^0 → 1
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symPow(arena,
            symVar(arena, 'x'),
            symInt(arena, 0));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B6 x^0 → 1 type == Num", s->type == SymExprType::Num);
        check("B6 x^0 → 1 eval", approx(s->evaluate(42.0), 1.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B7: --x → x
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symNeg(arena, symNeg(arena, symVar(arena, 'x')));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B7 --x → x type == Var", s->type == SymExprType::Var);
        check("B7 --x → x eval", approx(s->evaluate(5.0), 5.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B8: -(0) → 0
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symNeg(arena, symInt(arena, 0));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B8 -(0) → 0 type == Num", s->type == SymExprType::Num);
        check("B8 -(0) → 0 eval", approx(s->evaluate(0.0), 0.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B9: constant folding: 3 + 5 → 8
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symAdd(arena, symInt(arena, 3), symInt(arena, 5));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B9 3+5 → 8 type == Num", s->type == SymExprType::Num);
        check("B9 3+5 → 8 eval", approx(s->evaluate(0.0), 8.0));
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // B10: constant folding mul: 2 · 4 → 8
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symMul(arena, symInt(arena, 2), symInt(arena, 4));
        SymExpr* s = SymSimplify::simplify(expr, arena);
        check("B10 2·4 → 8 type == Num", s->type == SymExprType::Num);
        check("B10 2·4 → 8 eval", approx(s->evaluate(0.0), 8.0));
        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // B11: diff + simplify combined: d/dx(x²) simplified
    //      Raw: Add(Mul(x,1), Mul(1,x)) → simplify → Add(x, x)
    //      At x=3 → 6
    // ════════════════════════════════════════════════════════════════
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symPow(arena, x, symInt(arena, 2));
        SymExpr* d = SymDiff::diff(expr, 'x', arena);
        check("B11 raw diff != null", d != nullptr);
        if (d) {
            SymExpr* s = SymSimplify::simplify(d, arena);
            check("B11 simplified d/dx(x²) at x=3", approx(s->evaluate(3.0), 6.0));
            SD_PRINT("  B11 simplified: %s\n", s->toString().c_str());
        }
        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // C) SymExprToAST Tests
    // ════════════════════════════════════════════════════════════════

    // ────────────────────────────────────────────────────────────────
    // C1: SymNum(5) → NodeNumber "5"
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symInt(arena, 5);
        auto node = SymExprToAST::convert(expr);
        check("C1 num→AST != null", node != nullptr);
        if (node) {
            check("C1 num→AST type == Number",
                  node->type() == vpam::NodeType::Number);
            if (node->type() == vpam::NodeType::Number) {
                auto* nn = static_cast<vpam::NodeNumber*>(node.get());
                check("C1 num→AST value == '5'", nn->value() == "5");
            }
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C2: SymVar('x') → NodeVariable 'x'
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symVar(arena, 'x');
        auto node = SymExprToAST::convert(expr);
        check("C2 var→AST != null", node != nullptr);
        if (node) {
            check("C2 var→AST type == Variable",
                  node->type() == vpam::NodeType::Variable);
            if (node->type() == vpam::NodeType::Variable) {
                auto* vn = static_cast<vpam::NodeVariable*>(node.get());
                check("C2 var→AST name == 'x'", vn->name() == 'x');
            }
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C3: SymFrac(1,2) → NodeFraction
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symFrac(arena, 1, 2);
        auto node = SymExprToAST::convert(expr);
        check("C3 frac→AST != null", node != nullptr);
        if (node) {
            check("C3 frac→AST type == Fraction",
                  node->type() == vpam::NodeType::Fraction);
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C4: SymFunc(Sin, x) → NodeFunction(Sin, ...)
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symFunc(arena, SymFuncKind::Sin, symVar(arena, 'x'));
        auto node = SymExprToAST::convert(expr);
        check("C4 sin→AST != null", node != nullptr);
        if (node) {
            check("C4 sin→AST type == Function",
                  node->type() == vpam::NodeType::Function);
            if (node->type() == vpam::NodeType::Function) {
                auto* fn = static_cast<vpam::NodeFunction*>(node.get());
                check("C4 sin→AST kind == Sin",
                      fn->funcKind() == vpam::FuncKind::Sin);
            }
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C5: SymPow(x, 1/2) → NodeRoot (square root)
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symPow(arena, symVar(arena, 'x'), symFrac(arena, 1, 2));
        auto node = SymExprToAST::convert(expr);
        check("C5 x^(1/2)→AST != null", node != nullptr);
        if (node) {
            check("C5 x^(1/2)→AST type == Root",
                  node->type() == vpam::NodeType::Root);
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C6: SymPow(x, 2) → NodePower
    // ────────────────────────────────────────────────────────────────
    {
        auto* expr = symPow(arena, symVar(arena, 'x'), symInt(arena, 2));
        auto node = SymExprToAST::convert(expr);
        check("C6 x²→AST != null", node != nullptr);
        if (node) {
            check("C6 x²→AST type == Power",
                  node->type() == vpam::NodeType::Power);
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C7: Mul(x, x^(-1)) → Fraction (x / x)
    // ────────────────────────────────────────────────────────────────
    {
        auto* x1 = symVar(arena, 'x');
        auto* x2 = symVar(arena, 'x');
        auto* expr = symMul(arena,
            x1,
            symPow(arena, x2, symInt(arena, -1)));
        auto node = SymExprToAST::convert(expr);
        check("C7 x·x^(-1)→AST != null", node != nullptr);
        if (node) {
            check("C7 x·x^(-1)→AST type == Fraction",
                  node->type() == vpam::NodeType::Fraction);
        }
        arena.reset();
    }

    // ────────────────────────────────────────────────────────────────
    // C8: Full pipeline: diff(x²) → simplify → toAST
    // ────────────────────────────────────────────────────────────────
    {
        auto* x = symVar(arena, 'x');
        auto* expr = symPow(arena, x, symInt(arena, 2));
        SymExpr* d = SymDiff::diff(expr, 'x', arena);
        check("C8 diff != null", d != nullptr);
        if (d) {
            SymExpr* s = SymSimplify::simplify(d, arena);
            auto node = SymExprToAST::convert(s);
            check("C8 toAST != null", node != nullptr);
            SD_PRINT("  C8 pipeline: x² → diff → simplify → AST OK\n");
        }
        arena.reset();
    }

    // ════════════════════════════════════════════════════════════════
    // Summary
    // ════════════════════════════════════════════════════════════════

    SD_PRINT("\n  Phase 3 results: %d PASS, %d FAIL (total %d)\n",
             _sdPass, _sdFail, _sdPass + _sdFail);
    if (_sdFail == 0)
        SD_PRINTLN("  ✔ ALL PHASE 3 TESTS PASSED");
    else
        SD_PRINTLN("  ✘ SOME TESTS FAILED");

    SD_PRINTLN("══════════════════════════════════════════════════\n");
}

} // namespace cas
