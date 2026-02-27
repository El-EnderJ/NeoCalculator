/**
 * ASTFlatExprTest.cpp — Phase 2 unit tests: ASTFlattener dual-mode output.
 *
 * Tests the flattenToExpr() path and the automatic poly→SymExpr fallback
 * when transcendental nodes are encountered.
 *
 * Convention:
 *   · Build MathAST trees using vpam::makeXxx() factories.
 *   · Set an arena on the flattener to enable the SymExpr path.
 *   · Verify exprTree is non-null, transcendental flag, and evaluate().
 *
 * Part of: NumOS Pro-CAS — Phase 2 (ASTFlattener Dual-Mode)
 */

#include "ASTFlatExprTest.h"
#include "../src/math/cas/ASTFlattener.h"
#include "../src/math/cas/SymExpr.h"
#include "../src/math/cas/SymExprArena.h"
#include "../src/math/MathAST.h"

#include <cmath>
#include <cstdio>

// ── Platform-agnostic print macros ──────────────────────────────────
#ifdef ARDUINO
  #include <Arduino.h>
  #define AFE_PRINT(...)   Serial.printf(__VA_ARGS__)
  #define AFE_PRINTLN(s)   Serial.println(s)
#else
  #define AFE_PRINT(...)   printf(__VA_ARGS__)
  #define AFE_PRINTLN(s)   printf("%s\n", s)
#endif

namespace cas {

static int _afePass = 0;
static int _afeFail = 0;

static void check(const char* name, bool cond) {
    if (cond) {
        _afePass++;
    } else {
        _afeFail++;
        AFE_PRINT("  FAIL: %s\n", name);
    }
}

static bool approx(double a, double b, double tol = 1e-9) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
    return std::fabs(a - b) < tol;
}

// ════════════════════════════════════════════════════════════════════
// Helper: build common AST patterns
// ════════════════════════════════════════════════════════════════════

/// Build a Row containing: [children...]
/// Caller appends children manually via the returned raw pointer.
static vpam::NodeRow* makeRowRaw(vpam::NodePtr& owner) {
    owner = vpam::makeRow();
    return static_cast<vpam::NodeRow*>(owner.get());
}

// ════════════════════════════════════════════════════════════════════
// Test suite
// ════════════════════════════════════════════════════════════════════

void runASTFlatExprTests() {
    _afePass = 0;
    _afeFail = 0;

    AFE_PRINTLN("\n══════════ ASTFlatExpr Phase 2 Tests ══════════");

    SymExprArena arena;

    // ── Test 1: sin(x) → SymFunc(Sin, SymVar('x')) ──────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto sinNode = vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x'));

        auto r = flat.flatten(sinNode.get());
        check("T1 sin(x) ok", r.ok);
        check("T1 sin(x) transcendental", r.transcendental);
        check("T1 sin(x) exprTree != null", r.exprTree != nullptr);

        if (r.exprTree) {
            check("T1 sin(x) type == Func",
                  r.exprTree->type == SymExprType::Func);

            double val = r.exprTree->evaluate(M_PI / 2.0);
            check("T1 sin(π/2) ≈ 1.0", approx(val, 1.0));

            AFE_PRINT("  sin(x) → %s\n", r.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 2: cos(x) ──────────────────────────────────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto cosNode = vpam::makeFunction(
            vpam::FuncKind::Cos, vpam::makeVariable('x'));

        auto r = flat.flatten(cosNode.get());
        check("T2 cos(x) ok", r.ok);
        check("T2 cos(x) transcendental", r.transcendental);

        if (r.exprTree) {
            double val = r.exprTree->evaluate(0.0);
            check("T2 cos(0) ≈ 1.0", approx(val, 1.0));
        }

        arena.reset();
    }

    // ── Test 3: sin(x) + 1 → SymAdd([SymFunc(Sin,x), SymNum(1)]) ─
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        vpam::NodePtr row;
        auto* r = makeRowRaw(row);
        r->appendChild(vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x')));
        r->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        r->appendChild(vpam::makeNumber("1"));

        auto res = flat.flatten(row.get());
        check("T3 sin(x)+1 ok", res.ok);
        check("T3 sin(x)+1 transcendental", res.transcendental);
        check("T3 sin(x)+1 exprTree != null", res.exprTree != nullptr);

        if (res.exprTree) {
            check("T3 sin(x)+1 type == Add",
                  res.exprTree->type == SymExprType::Add);

            // sin(0) + 1 = 1
            check("T3 eval(0) ≈ 1.0",
                  approx(res.exprTree->evaluate(0.0), 1.0));

            // sin(π/2) + 1 = 2
            check("T3 eval(π/2) ≈ 2.0",
                  approx(res.exprTree->evaluate(M_PI / 2.0), 2.0));

            AFE_PRINT("  sin(x)+1 → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 4: e^(2x) → SymPow(e, SymMul(2, x)) ──────────────
    // AST: Power(base=Constant(e), exp=Row[Number("2"), Variable('x')])
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto expRow = vpam::makeRow();
        auto* er = static_cast<vpam::NodeRow*>(expRow.get());
        er->appendChild(vpam::makeNumber("2"));
        er->appendChild(vpam::makeVariable('x'));

        auto pw = vpam::makePower(
            vpam::makeConstant(vpam::ConstKind::E),
            std::move(expRow));

        auto res = flat.flatten(pw.get());
        check("T4 e^(2x) ok", res.ok);
        check("T4 e^(2x) transcendental", res.transcendental);
        check("T4 e^(2x) exprTree != null", res.exprTree != nullptr);

        if (res.exprTree) {
            // e^(2·0) = 1
            check("T4 eval(0) ≈ 1.0",
                  approx(res.exprTree->evaluate(0.0), 1.0));

            // e^(2·1) = e² ≈ 7.389
            check("T4 eval(1) ≈ e²",
                  approx(res.exprTree->evaluate(1.0), std::exp(2.0)));

            AFE_PRINT("  e^(2x) → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 5: ln(x²) → SymFunc(Ln, SymPow(x, 2)) ────────────
    // AST: Function(Ln, argument=Power(Variable('x'), Number("2")))
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto inner = vpam::makePower(
            vpam::makeVariable('x'),
            vpam::makeNumber("2"));
        auto lnNode = vpam::makeFunction(
            vpam::FuncKind::Ln, std::move(inner));

        auto res = flat.flatten(lnNode.get());
        check("T5 ln(x²) ok", res.ok);
        check("T5 ln(x²) transcendental", res.transcendental);

        if (res.exprTree) {
            // ln(e²) = 2·ln(e) = 2
            double val = res.exprTree->evaluate(M_E);
            check("T5 ln(e²) ≈ 2.0", approx(val, 2.0));

            AFE_PRINT("  ln(x²) → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 6: x / sin(x) → SymMul(x, SymPow(sin(x), -1)) ────
    // AST: Fraction(num=Variable('x'), den=Function(Sin, Variable('x')))
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto frac = vpam::makeFraction(
            vpam::makeVariable('x'),
            vpam::makeFunction(
                vpam::FuncKind::Sin, vpam::makeVariable('x')));

        auto res = flat.flatten(frac.get());
        check("T6 x/sin(x) ok", res.ok);
        check("T6 x/sin(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // x/sin(x) at x→0 ≈ 1.0 (L'Hôpital limit; use small x)
            double val = res.exprTree->evaluate(0.001);
            check("T6 eval(0.001) ≈ 1.0", approx(val, 1.0, 1e-3));

            AFE_PRINT("  x/sin(x) → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 7: 3x² + sin(x) (mixed polynomial + transcendental) ─
    // AST: Row[ Number("3"), Power(x, 2), Op(+), Function(Sin, x) ]
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeNumber("3"));
        rp->appendChild(vpam::makePower(
            vpam::makeVariable('x'), vpam::makeNumber("2")));
        rp->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rp->appendChild(vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x')));

        auto res = flat.flatten(row.get());
        check("T7 3x²+sin(x) ok", res.ok);
        check("T7 3x²+sin(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // 3·0² + sin(0) = 0
            check("T7 eval(0) ≈ 0",
                  approx(res.exprTree->evaluate(0.0), 0.0));

            // 3·1² + sin(1) ≈ 3.841
            double expected = 3.0 + std::sin(1.0);
            check("T7 eval(1) ≈ 3+sin(1)",
                  approx(res.exprTree->evaluate(1.0), expected));

            AFE_PRINT("  3x²+sin(x) → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 8: √x → SymPow(x, 1/2) ────────────────────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto rootNode = vpam::makeRoot(vpam::makeVariable('x'));

        auto res = flat.flatten(rootNode.get());
        check("T8 √x ok", res.ok);
        check("T8 √x transcendental", res.transcendental);

        if (res.exprTree) {
            check("T8 √x type == Pow",
                  res.exprTree->type == SymExprType::Pow);

            // √4 = 2
            check("T8 eval(4) ≈ 2.0",
                  approx(res.exprTree->evaluate(4.0), 2.0));

            // √9 = 3
            check("T8 eval(9) ≈ 3.0",
                  approx(res.exprTree->evaluate(9.0), 3.0));

            AFE_PRINT("  √x → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 9: log₂(x) → ln(x)/ln(2) ─────────────────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto logNode = vpam::makeLogBase(
            vpam::makeNumber("2"),
            vpam::makeVariable('x'));

        auto res = flat.flatten(logNode.get());
        check("T9 log₂(x) ok", res.ok);
        check("T9 log₂(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // log₂(8) = 3
            check("T9 log₂(8) ≈ 3.0",
                  approx(res.exprTree->evaluate(8.0), 3.0));

            // log₂(1) = 0
            check("T9 log₂(1) ≈ 0.0",
                  approx(res.exprTree->evaluate(1.0), 0.0));

            AFE_PRINT("  log₂(x) → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 10: tan(x) ─────────────────────────────────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto tanNode = vpam::makeFunction(
            vpam::FuncKind::Tan, vpam::makeVariable('x'));

        auto res = flat.flatten(tanNode.get());
        check("T10 tan(x) ok", res.ok);
        check("T10 tan(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // tan(π/4) = 1
            check("T10 tan(π/4) ≈ 1.0",
                  approx(res.exprTree->evaluate(M_PI / 4.0), 1.0));
        }

        arena.reset();
    }

    // ── Test 11: x^x (variable base AND exponent) ───────────────
    // AST: Power(Variable('x'), Variable('x'))
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto pw = vpam::makePower(
            vpam::makeVariable('x'),
            vpam::makeVariable('x'));

        auto res = flat.flatten(pw.get());
        check("T11 x^x ok", res.ok);
        check("T11 x^x transcendental", res.transcendental);

        if (res.exprTree) {
            // 2^2 = 4
            check("T11 eval(2) ≈ 4.0",
                  approx(res.exprTree->evaluate(2.0), 4.0));

            // 3^3 = 27
            check("T11 eval(3) ≈ 27.0",
                  approx(res.exprTree->evaluate(3.0), 27.0));
        }

        arena.reset();
    }

    // ── Test 12: Preserve polynomial path (no arena on flattener) ─
    {
        ASTFlattener flat;
        // Deliberately NOT calling flat.setArena()

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeNumber("3"));
        rp->appendChild(vpam::makeVariable('x'));
        rp->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rp->appendChild(vpam::makeNumber("5"));

        auto res = flat.flatten(row.get());
        check("T12 3x+5 poly ok", res.ok);
        check("T12 3x+5 NOT transcendental", !res.transcendental);
        check("T12 3x+5 exprTree == null", res.exprTree == nullptr);
        check("T12 3x+5 x coeff == 3", res.poly.coeffAt(1).num == 3);
        check("T12 3x+5 const == 5", res.poly.coeffAt(0).num == 5);
    }

    // ── Test 13: Preserve polynomial path (sin without arena) ───
    {
        ASTFlattener flat;
        // No arena → old behavior: needsNumeric

        auto sinNode = vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x'));

        auto res = flat.flatten(sinNode.get());
        check("T13 sin(x) no arena: !ok", !res.ok);
        check("T13 sin(x) no arena: transcendental", res.transcendental);
        check("T13 sin(x) no arena: exprTree == null",
              res.exprTree == nullptr);
    }

    // ── Test 14: Equation: sin(x) = 0 (transcendental equation) ─
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto lhs = vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x'));
        auto rhs = vpam::makeNumber("0");

        auto res = flat.flattenEquation(lhs.get(), rhs.get());
        check("T14 sin(x)=0 ok", res.ok);
        check("T14 sin(x)=0 transcendental", res.transcendental);
        check("T14 sin(x)=0 lhsExpr != null", res.lhsExpr != nullptr);
        check("T14 sin(x)=0 rhsExpr != null", res.rhsExpr != nullptr);

        if (res.lhsExpr) {
            // sin(π) ≈ 0
            check("T14 lhs eval(π) ≈ 0",
                  approx(res.lhsExpr->evaluate(M_PI), 0.0, 1e-9));
        }
        if (res.rhsExpr) {
            // rhs is constant 0
            check("T14 rhs eval(any) ≈ 0",
                  approx(res.rhsExpr->evaluate(42.0), 0.0));
        }

        arena.reset();
    }

    // ── Test 15: Equation: x² + 1 = cos(x) (mixed equation) ────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        // LHS: x² + 1
        vpam::NodePtr lhsRow;
        auto* lr = makeRowRaw(lhsRow);
        lr->appendChild(vpam::makePower(
            vpam::makeVariable('x'), vpam::makeNumber("2")));
        lr->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        lr->appendChild(vpam::makeNumber("1"));

        // RHS: cos(x)
        auto rhsNode = vpam::makeFunction(
            vpam::FuncKind::Cos, vpam::makeVariable('x'));

        auto res = flat.flattenEquation(lhsRow.get(), rhsNode.get());
        check("T15 x²+1=cos(x) ok", res.ok);
        check("T15 x²+1=cos(x) transcendental", res.transcendental);
        check("T15 lhsExpr != null", res.lhsExpr != nullptr);
        check("T15 rhsExpr != null", res.rhsExpr != nullptr);

        if (res.lhsExpr && res.rhsExpr) {
            // At x=0: lhs=1, rhs=cos(0)=1
            double lv = res.lhsExpr->evaluate(0.0);
            double rv = res.rhsExpr->evaluate(0.0);
            check("T15 lhs(0) ≈ 1", approx(lv, 1.0));
            check("T15 rhs(0) ≈ 1", approx(rv, 1.0));
        }

        arena.reset();
    }

    // ── Test 16: -sin(x) (unary negation of transcendental) ─────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeOperator(vpam::OpKind::Sub));  // unary -
        rp->appendChild(vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x')));

        auto res = flat.flatten(row.get());
        check("T16 -sin(x) ok", res.ok);
        check("T16 -sin(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // -sin(π/2) = -1
            check("T16 eval(π/2) ≈ -1",
                  approx(res.exprTree->evaluate(M_PI / 2.0), -1.0));
        }

        arena.reset();
    }

    // ── Test 17: 2·sin(x)·cos(x) (product of transcendentals) ──
    // AST: Row[ Number("2"), Function(Sin,x), Function(Cos,x) ]
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeNumber("2"));
        rp->appendChild(vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x')));
        rp->appendChild(vpam::makeFunction(
            vpam::FuncKind::Cos, vpam::makeVariable('x')));

        auto res = flat.flatten(row.get());
        check("T17 2sin(x)cos(x) ok", res.ok);
        check("T17 2sin(x)cos(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // 2sin(π/4)cos(π/4) = 2·(√2/2)·(√2/2) = 1
            double val = res.exprTree->evaluate(M_PI / 4.0);
            check("T17 eval(π/4) ≈ 1.0", approx(val, 1.0));

            AFE_PRINT("  2sin(x)cos(x) → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 18: ³√x (cube root) → x^(1/3) ─────────────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto rootNode = vpam::makeRoot(
            vpam::makeVariable('x'),
            vpam::makeNumber("3"));  // cube root

        auto res = flat.flatten(rootNode.get());
        check("T18 ³√x ok", res.ok);
        check("T18 ³√x transcendental", res.transcendental);

        if (res.exprTree) {
            // ³√8 = 2
            check("T18 eval(8) ≈ 2.0",
                  approx(res.exprTree->evaluate(8.0), 2.0));

            // ³√27 = 3
            check("T18 eval(27) ≈ 3.0",
                  approx(res.exprTree->evaluate(27.0), 3.0));

            AFE_PRINT("  ³√x → %s\n",
                      res.exprTree->toString().c_str());
        }

        arena.reset();
    }

    // ── Test 19: flattenToExpr on pure polynomial 2x+3 ──────────
    // Verify that flattenToExpr works even for non-transcendental
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeNumber("2"));
        rp->appendChild(vpam::makeVariable('x'));
        rp->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rp->appendChild(vpam::makeNumber("3"));

        SymExpr* expr = flat.flattenToExpr(row.get());
        check("T19 flattenToExpr(2x+3) != null", expr != nullptr);

        if (expr) {
            // 2·5 + 3 = 13
            check("T19 eval(5) ≈ 13",
                  approx(expr->evaluate(5.0), 13.0));

            // 2·0 + 3 = 3
            check("T19 eval(0) ≈ 3",
                  approx(expr->evaluate(0.0), 3.0));
        }

        arena.reset();
    }

    // ── Test 20: arctan(x) ──────────────────────────────────────
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        auto atanNode = vpam::makeFunction(
            vpam::FuncKind::ArcTan, vpam::makeVariable('x'));

        auto res = flat.flatten(atanNode.get());
        check("T20 arctan(x) ok", res.ok);
        check("T20 arctan(x) transcendental", res.transcendental);

        if (res.exprTree) {
            // arctan(1) = π/4
            check("T20 arctan(1) ≈ π/4",
                  approx(res.exprTree->evaluate(1.0), M_PI / 4.0));
        }

        arena.reset();
    }

    // ── Test 21: sin(x) − x (subtraction with transcendental) ──
    {
        ASTFlattener flat;
        flat.setArena(&arena);

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x')));
        rp->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
        rp->appendChild(vpam::makeVariable('x'));

        auto res = flat.flatten(row.get());
        check("T21 sin(x)-x ok", res.ok);
        check("T21 sin(x)-x transcendental", res.transcendental);

        if (res.exprTree) {
            // sin(0) - 0 = 0
            check("T21 eval(0) ≈ 0",
                  approx(res.exprTree->evaluate(0.0), 0.0));

            // sin(1) - 1 ≈ -0.1585
            double expected = std::sin(1.0) - 1.0;
            check("T21 eval(1) ≈ sin(1)-1",
                  approx(res.exprTree->evaluate(1.0), expected));
        }

        arena.reset();
    }

    // ── Test 22: Polynomial path preserved with arena set ───────
    // Pure polynomial (no transcendentals) should still use poly path
    {
        ASTFlattener flat;
        flat.setArena(&arena);  // Arena IS set

        vpam::NodePtr row;
        auto* rp = makeRowRaw(row);
        rp->appendChild(vpam::makeNumber("5"));
        rp->appendChild(vpam::makeVariable('x'));
        rp->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
        rp->appendChild(vpam::makeNumber("7"));

        auto res = flat.flatten(row.get());
        check("T22 5x-7 poly ok", res.ok);
        check("T22 5x-7 NOT transcendental", !res.transcendental);
        check("T22 5x-7 exprTree == null", res.exprTree == nullptr);
        check("T22 5x-7 x coeff == 5", res.poly.coeffAt(1).num == 5);
        check("T22 5x-7 const == -7", res.poly.coeffAt(0).num == -7);

        arena.reset();
    }

    // ── Summary ─────────────────────────────────────────────────
    AFE_PRINT("\n── ASTFlatExpr Phase 2: %d passed, %d failed ──\n",
              _afePass, _afeFail);
    if (_afeFail > 0) {
        AFE_PRINTLN("*** SOME TESTS FAILED ***");
    } else {
        AFE_PRINTLN("All tests PASSED!");
    }
}

} // namespace cas
