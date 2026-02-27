/**
 * CASTest.cpp — Unit tests for CAS-Lite Phase A, B & C.
 *
 * Phase A: SymTerm, SymPoly (normalize, add, sub, negate, mulScalar, divScalar),
 *          SymEquation, negative powers (x⁻¹), PSRAM allocator.
 * Phase B: mulPoly (FOIL), ASTFlattener (MathAST → SymPoly/SymEquation),
 *          transcendental detection, fraction handling.
 * Phase C: CASStepLogger, SingleSolver (linear, quadratic, Newton-Raphson).
 *
 * Usage: Call cas::runCASTests() from main.cpp setup() after Serial is ready.
 *        Results are printed to Serial. Remove the call for production builds.
 */

#include "CASTest.h"
#include "../src/math/cas/SymPoly.h"
#include "../src/math/cas/SymEquation.h"
#include "../src/math/cas/ASTFlattener.h"
#include "../src/math/cas/CASStepLogger.h"
#include "../src/math/cas/SingleSolver.h"
#include "../src/math/cas/SystemSolver.h"
#include "../src/math/MathAST.h"
#include <cmath>

#ifdef ARDUINO
  #include <Arduino.h>
  #include <esp_heap_caps.h>
  #define PRINT(x) Serial.print(x)
  #define PRINTLN(x) Serial.println(x)
#else
  #include <cstdio>
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

void runCASTests() {
    _passed = 0;
    _failed = 0;

    PRINTLN("\n════════════════════════════════════════════");
    PRINTLN("  CAS-Lite Phase A — Unit Tests");
    PRINTLN("════════════════════════════════════════════");

#ifdef ARDUINO
    size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    PRINT("PSRAM free before: ");
    PRINTLN(String(psramBefore).c_str());
#endif

    // ── Test 1: SymTerm construction ──────────────────────────────
    {
        auto t = SymTerm::variable('x', 3, 1, 2);  // 3x²
        check("SymTerm(3x²) coeff.num", t.coeff.num == 3);
        check("SymTerm(3x²) coeff.den", t.coeff.den == 1);
        check("SymTerm(3x²) power", t.power == 2);
        check("SymTerm(3x²) not constant", !t.isConstant());
    }

    // ── Test 2: SymTerm constant ──────────────────────────────────
    {
        auto t = SymTerm::constant(vpam::ExactVal::fromInt(-5));
        check("SymTerm(-5) is constant", t.isConstant());
        check("SymTerm(-5) coeff.num", t.coeff.num == -5);
    }

    // ── Test 3: SymTerm fraction coefficient ──────────────────────
    {
        auto t = SymTerm::variable('x', 2, 3, 1);  // (2/3)x
        check("SymTerm(2/3 x) coeff.num", t.coeff.num == 2);
        check("SymTerm(2/3 x) coeff.den", t.coeff.den == 3);
    }

    // ── Test 4: SymTerm negative power (2/x = 2·x⁻¹) ────────────
    {
        auto t = SymTerm::variable('x', 2, 1, -1);  // 2x⁻¹
        check("SymTerm(2x^-1) power", t.power == -1);
        check("SymTerm(2x^-1) not constant", !t.isConstant());
    }

    // ── Test 5: SymTerm like terms ───────────────────────────────
    {
        auto a = SymTerm::variable('x', 3, 1, 2);  // 3x²
        auto b = SymTerm::variable('x', 5, 1, 2);  // 5x²
        auto c = SymTerm::variable('x', 2, 1, 1);  // 2x
        check("3x² and 5x² are like terms", a.isLikeTerm(b));
        check("3x² and 2x are NOT like terms", !a.isLikeTerm(c));
    }

    // ── Test 6: SymPoly creation & normalize ─────────────────────
    // Build 3x² + 2x - 5
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 2, 1, 1));   // 2x
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-5)));  // -5
        p.terms().push_back(SymTerm::variable('x', 3, 1, 2));   // 3x²
        p.normalize();

        check("Poly(3x²+2x-5) has 3 terms", p.terms().size() == 3);
        check("Poly degree == 2", p.degree() == 2);
        // After sort: first term should be 3x²
        check("First term power == 2", p.terms()[0].power == 2);
        check("First term coeff == 3", p.terms()[0].coeff.num == 3);
        // Second term: 2x
        check("Second term power == 1", p.terms()[1].power == 1);
        check("Second term coeff == 2", p.terms()[1].coeff.num == 2);
        // Third term: -5 (constant)
        check("Third term is constant", p.terms()[2].isConstant());
        check("Third term coeff == -5", p.terms()[2].coeff.num == -5);

        PRINT("  toString: ");
        PRINTLN(p.toString().c_str());
    }

    // ── Test 7: Combine like terms ───────────────────────────────
    // 3x² + 5x² should become 8x²
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 3, 1, 2));  // 3x²
        p.terms().push_back(SymTerm::variable('x', 5, 1, 2));  // 5x²
        p.normalize();

        check("3x²+5x² → 1 term", p.terms().size() == 1);
        check("Combined coeff == 8", p.terms()[0].coeff.num == 8);
    }

    // ── Test 8: Remove zero terms ────────────────────────────────
    // 3x² - 3x² should become 0 (empty)
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 3, 1, 2));   // 3x²
        p.terms().push_back(SymTerm::variable('x', -3, 1, 2));  // -3x²
        p.normalize();

        check("3x²-3x² → zero polynomial", p.isZero());
    }

    // ── Test 9: Polynomial subtraction ───────────────────────────
    // (3x² + 2x - 5) - (x² - 3x + 2) = 2x² + 5x - 7
    {
        SymPoly a('x');
        a.terms().push_back(SymTerm::variable('x', 3, 1, 2));   // 3x²
        a.terms().push_back(SymTerm::variable('x', 2, 1, 1));   // 2x
        a.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-5)));
        a.normalize();

        SymPoly b('x');
        b.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        b.terms().push_back(SymTerm::variable('x', -3, 1, 1));  // -3x
        b.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(2)));
        b.normalize();

        SymPoly diff = a.sub(b);

        check("(3x²+2x-5)-(x²-3x+2) has 3 terms", diff.terms().size() == 3);
        // Expected: 2x² + 5x - 7
        check("Result x² coeff == 2", diff.coeffAt(2).num == 2);
        check("Result x  coeff == 5", diff.coeffAt(1).num == 5);
        check("Result const   == -7", diff.coeffAt(0).num == -7);

        PRINT("  a - b = ");
        PRINTLN(diff.toString().c_str());
    }

    // ── Test 10: Polynomial addition ─────────────────────────────
    // (2x + 3) + (5x - 1) = 7x + 2
    {
        SymPoly a('x');
        a.terms().push_back(SymTerm::variable('x', 2, 1, 1));
        a.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(3)));
        a.normalize();

        SymPoly b('x');
        b.terms().push_back(SymTerm::variable('x', 5, 1, 1));
        b.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-1)));
        b.normalize();

        SymPoly sum = a.add(b);
        check("(2x+3)+(5x-1) x coeff == 7", sum.coeffAt(1).num == 7);
        check("(2x+3)+(5x-1) const   == 2", sum.coeffAt(0).num == 2);
    }

    // ── Test 11: mulScalar ───────────────────────────────────────
    // (2x + 3) * 4 = 8x + 12
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 2, 1, 1));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(3)));
        p.normalize();

        SymPoly result = p.mulScalar(vpam::ExactVal::fromInt(4));
        check("(2x+3)*4 → x coeff == 8", result.coeffAt(1).num == 8);
        check("(2x+3)*4 → const == 12",  result.coeffAt(0).num == 12);
    }

    // ── Test 12: divScalar (distribution) ────────────────────────
    // (6x² + 4x - 2) / 2 = 3x² + 2x - 1
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 6, 1, 2));
        p.terms().push_back(SymTerm::variable('x', 4, 1, 1));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-2)));
        p.normalize();

        SymPoly result = p.divScalar(vpam::ExactVal::fromInt(2));
        check("(6x²+4x-2)/2 → x² coeff == 3", result.coeffAt(2).num == 3);
        check("(6x²+4x-2)/2 → x  coeff == 2", result.coeffAt(1).num == 2);
        check("(6x²+4x-2)/2 → const == -1",   result.coeffAt(0).num == -1);

        PRINT("  (6x²+4x-2)/2 = ");
        PRINTLN(result.toString().c_str());
    }

    // ── Test 13: Fraction coefficient (GCD simplification) ───────
    // (4/6)x should simplify to (2/3)x
    {
        auto t = SymTerm(vpam::ExactVal::fromFrac(4, 6), 'x', 1);
        check("4/6 simplifies to num=2", t.coeff.num == 2);
        check("4/6 simplifies to den=3", t.coeff.den == 3);
    }

    // ── Test 14: SymEquation ─────────────────────────────────────
    // 3x + 2 = 5x - 4  →  moveAllToLHS → -2x + 6 = 0
    {
        SymPoly left('x');
        left.terms().push_back(SymTerm::variable('x', 3, 1, 1));
        left.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(2)));
        left.normalize();

        SymPoly right('x');
        right.terms().push_back(SymTerm::variable('x', 5, 1, 1));
        right.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-4)));
        right.normalize();

        SymEquation eq(left, right);
        PRINT("  Equation: ");
        PRINTLN(eq.toString().c_str());

        SymEquation unified = eq.moveAllToLHS();
        check("moveAllToLHS → RHS is zero", unified.rhs.isZero());
        // LHS should be -2x + 6
        check("LHS x coeff == -2", unified.lhs.coeffAt(1).num == -2);
        check("LHS const   ==  6", unified.lhs.coeffAt(0).num == 6);

        PRINT("  Unified: ");
        PRINTLN(unified.toString().c_str());

        check("Equation is linear", unified.isLinear());
    }

    // ── Test 15: Negate polynomial ───────────────────────────────
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 3, 1, 2));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-7)));
        p.normalize();

        SymPoly neg = p.negate();
        check("negate(3x²-7) → x² coeff == -3", neg.coeffAt(2).num == -3);
        check("negate(3x²-7) → const == 7",     neg.coeffAt(0).num == 7);
    }

    // ── Test 16: Negative power (denominator variable) ───────────
    // 2/x = 2·x⁻¹  →  polynomial with power=-1
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 2, 1, -1));  // 2x⁻¹
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(1)));
        p.normalize();

        check("2x⁻¹+1 has 2 terms", p.terms().size() == 2);
        check("coeffAt(-1) == 2", p.coeffAt(-1).num == 2);

        PRINT("  2/x + 1 = ");
        PRINTLN(p.toString().c_str());
    }

    // ── Test 17: divScalar with fraction ─────────────────────────
    // (3x + 6) / 3 = x + 2
    {
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 3, 1, 1));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(6)));
        p.normalize();

        SymPoly result = p.divScalar(vpam::ExactVal::fromInt(3));
        check("(3x+6)/3 → x coeff == 1", result.coeffAt(1).num == 1);
        check("(3x+6)/3 → const == 2",   result.coeffAt(0).num == 2);
    }

    // ── Test 18: SymEquation divBothScalar ───────────────────────
    // 4x = 8  → ÷4 → x = 2
    {
        SymPoly left = SymPoly::fromTerm(SymTerm::variable('x', 4, 1, 1));
        SymPoly right = SymPoly::fromConstant(vpam::ExactVal::fromInt(8));

        SymEquation eq(left, right);
        SymEquation simplified = eq.divBothScalar(vpam::ExactVal::fromInt(4));

        check("4x=8 ÷4 → LHS x coeff == 1", simplified.lhs.coeffAt(1).num == 1);
        check("4x=8 ÷4 → RHS const == 2",   simplified.rhs.coeffAt(0).num == 2);

        PRINT("  4x=8 → ");
        PRINTLN(simplified.toString().c_str());
    }

    // ════════════════════════════════════════════════════════════════════
    // Phase B Tests: mulPoly + ASTFlattener
    // ════════════════════════════════════════════════════════════════════
    PRINTLN("\n════════════════════════════════════════════");
    PRINTLN("  CAS-Lite Phase B — Unit Tests");
    PRINTLN("════════════════════════════════════════════");

    // ── Test 19: Polynomial multiplication (FOIL) ─────────────────
    // (x + 2)(x - 3) = x² - x - 6
    {
        SymPoly a('x');
        a.terms().push_back(SymTerm::variable('x', 1, 1, 1));  // x
        a.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(2)));  // +2
        a.normalize();

        SymPoly b('x');
        b.terms().push_back(SymTerm::variable('x', 1, 1, 1));  // x
        b.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-3))); // -3
        b.normalize();

        SymPoly product = a.mul(b);
        check("(x+2)(x-3) has 3 terms", product.terms().size() == 3);
        check("(x+2)(x-3) x² coeff == 1", product.coeffAt(2).num == 1);
        check("(x+2)(x-3) x  coeff == -1", product.coeffAt(1).num == -1);
        check("(x+2)(x-3) const == -6",    product.coeffAt(0).num == -6);

        PRINT("  (x+2)(x-3) = ");
        PRINTLN(product.toString().c_str());
    }

    // ── Test 20: mulPoly constant × polynomial ────────────────────
    // 3 × (x + 1) = 3x + 3
    {
        SymPoly a = SymPoly::fromConstant(vpam::ExactVal::fromInt(3));

        SymPoly b('x');
        b.terms().push_back(SymTerm::variable('x', 1, 1, 1));
        b.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(1)));
        b.normalize();

        SymPoly product = a.mul(b);
        check("3×(x+1) x coeff == 3", product.coeffAt(1).num == 3);
        check("3×(x+1) const == 3",  product.coeffAt(0).num == 3);
    }

    // ── Test 21: ASTFlattener — simple number ────────────────────
    {
        ASTFlattener flat;
        auto numNode = vpam::makeNumber("42");
        auto r = flat.flatten(numNode.get());
        check("flatten(42) ok", r.ok);
        check("flatten(42) is constant", r.poly.isConstant());
        check("flatten(42) coeff == 42", r.poly.coeffAt(0).num == 42);
    }

    // ── Test 22: ASTFlattener — decimal becomes fraction ────────
    {
        ASTFlattener flat;
        auto numNode = vpam::makeNumber("0.5");
        auto r = flat.flatten(numNode.get());
        check("flatten(0.5) ok", r.ok);
        check("flatten(0.5) num == 1", r.poly.coeffAt(0).num == 1);
        check("flatten(0.5) den == 2", r.poly.coeffAt(0).den == 2);
    }

    // ── Test 23: ASTFlattener — variable ───────────────────────
    {
        ASTFlattener flat;
        auto varNode = vpam::makeVariable('x');
        auto r = flat.flatten(varNode.get());
        check("flatten(x) ok", r.ok);
        check("flatten(x) degree == 1", r.poly.degree() == 1);
        check("flatten(x) x coeff == 1", r.poly.coeffAt(1).num == 1);
    }

    // ── Test 24: ASTFlattener — 3x + 5 (Row with implicit mul) ────
    // AST: Row[ Number("3"), Variable('x'), Operator(+), Number("5") ]
    {
        ASTFlattener flat;
        auto row = vpam::makeRow();
        auto* rowPtr = static_cast<vpam::NodeRow*>(row.get());
        rowPtr->appendChild(vpam::makeNumber("3"));
        rowPtr->appendChild(vpam::makeVariable('x'));
        rowPtr->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rowPtr->appendChild(vpam::makeNumber("5"));

        auto r = flat.flatten(row.get());
        check("flatten(3x+5) ok", r.ok);
        check("flatten(3x+5) x coeff == 3", r.poly.coeffAt(1).num == 3);
        check("flatten(3x+5) const == 5",   r.poly.coeffAt(0).num == 5);

        PRINT("  flatten(3x+5) = ");
        PRINTLN(r.poly.toString().c_str());
    }

    // ── Test 25: ASTFlattener — Equation: 3x+5 = 2x+10 ───────────
    {
        ASTFlattener flat;

        // LHS: 3x + 5
        auto lhs = vpam::makeRow();
        auto* lhsRow = static_cast<vpam::NodeRow*>(lhs.get());
        lhsRow->appendChild(vpam::makeNumber("3"));
        lhsRow->appendChild(vpam::makeVariable('x'));
        lhsRow->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        lhsRow->appendChild(vpam::makeNumber("5"));

        // RHS: 2x + 10
        auto rhs = vpam::makeRow();
        auto* rhsRow = static_cast<vpam::NodeRow*>(rhs.get());
        rhsRow->appendChild(vpam::makeNumber("2"));
        rhsRow->appendChild(vpam::makeVariable('x'));
        rhsRow->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rhsRow->appendChild(vpam::makeNumber("10"));

        auto eqr = flat.flattenEquation(lhs.get(), rhs.get());
        check("flattenEq(3x+5=2x+10) ok", eqr.ok);
        check("LHS x coeff == 3", eqr.eq.lhs.coeffAt(1).num == 3);
        check("LHS const == 5",   eqr.eq.lhs.coeffAt(0).num == 5);
        check("RHS x coeff == 2", eqr.eq.rhs.coeffAt(1).num == 2);
        check("RHS const == 10",  eqr.eq.rhs.coeffAt(0).num == 10);

        PRINT("  Equation: ");
        PRINTLN(eqr.eq.toString().c_str());

        // Also test moveAllToLHS: x - 5 = 0
        auto unified = eqr.eq.moveAllToLHS();
        check("unified x coeff == 1",  unified.lhs.coeffAt(1).num == 1);
        check("unified const == -5",   unified.lhs.coeffAt(0).num == -5);

        PRINT("  moveAllToLHS: ");
        PRINTLN(unified.toString().c_str());
    }

    // ── Test 26: ASTFlattener — FOIL: (x+2)(x-3) ───────────────
    // AST: Row[ Paren(Row[x, +, 2]), Paren(Row[x, -, 3]) ]
    // Implicit multiplication between adjacent parenthesized groups
    {
        ASTFlattener flat;

        // (x + 2)
        auto innerA = vpam::makeRow();
        auto* rA = static_cast<vpam::NodeRow*>(innerA.get());
        rA->appendChild(vpam::makeVariable('x'));
        rA->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rA->appendChild(vpam::makeNumber("2"));
        auto parenA = vpam::makeParen(std::move(innerA));

        // (x - 3)
        auto innerB = vpam::makeRow();
        auto* rB = static_cast<vpam::NodeRow*>(innerB.get());
        rB->appendChild(vpam::makeVariable('x'));
        rB->appendChild(vpam::makeOperator(vpam::OpKind::Sub));
        rB->appendChild(vpam::makeNumber("3"));
        auto parenB = vpam::makeParen(std::move(innerB));

        // Row[ parenA, parenB ]
        auto row = vpam::makeRow();
        auto* rowPtr = static_cast<vpam::NodeRow*>(row.get());
        rowPtr->appendChild(std::move(parenA));
        rowPtr->appendChild(std::move(parenB));

        auto r = flat.flatten(row.get());
        check("FOIL (x+2)(x-3) ok", r.ok);
        check("FOIL x² coeff == 1", r.poly.coeffAt(2).num == 1);
        check("FOIL x  coeff == -1", r.poly.coeffAt(1).num == -1);
        check("FOIL const == -6",    r.poly.coeffAt(0).num == -6);

        PRINT("  FOIL (x+2)(x-3) = ");
        PRINTLN(r.poly.toString().c_str());
    }

    // ── Test 27: ASTFlattener — Power: x^3 ─────────────────────
    {
        ASTFlattener flat;
        // Power(base=Variable('x'), exp=Number("3"))
        auto pw = vpam::makePower(
            vpam::makeVariable('x'),
            vpam::makeNumber("3"));

        auto r = flat.flatten(pw.get());
        check("flatten(x^3) ok", r.ok);
        check("flatten(x^3) degree == 3", r.poly.degree() == 3);
        check("flatten(x^3) x³ coeff == 1", r.poly.coeffAt(3).num == 1);
    }

    // ── Test 28: ASTFlattener — Fraction with constant denom ─────
    // (6x + 4) / 2 = 3x + 2
    {
        ASTFlattener flat;

        auto numRow = vpam::makeRow();
        auto* nr = static_cast<vpam::NodeRow*>(numRow.get());
        nr->appendChild(vpam::makeNumber("6"));
        nr->appendChild(vpam::makeVariable('x'));
        nr->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        nr->appendChild(vpam::makeNumber("4"));

        auto denNum = vpam::makeNumber("2");

        auto frac = vpam::makeFraction(std::move(numRow), std::move(denNum));
        auto r = flat.flatten(frac.get());
        check("flatten((6x+4)/2) ok", r.ok);
        check("(6x+4)/2 x coeff == 3", r.poly.coeffAt(1).num == 3);
        check("(6x+4)/2 const == 2",   r.poly.coeffAt(0).num == 2);

        PRINT("  (6x+4)/2 = ");
        PRINTLN(r.poly.toString().c_str());
    }

    // ── Test 29: ASTFlattener — Transcendental detection ─────────
    {
        ASTFlattener flat;
        auto sinNode = vpam::makeFunction(
            vpam::FuncKind::Sin, vpam::makeVariable('x'));

        auto r = flat.flatten(sinNode.get());
        check("flatten(sin(x)) NOT ok", !r.ok);
        check("flatten(sin(x)) transcendental", r.transcendental);
    }

    // ── Test 30: ASTFlattener — Constants (π, e) ───────────────
    {
        ASTFlattener flat;
        auto piNode = vpam::makeConstant(vpam::ConstKind::Pi);
        auto r = flat.flatten(piNode.get());
        check("flatten(π) ok", r.ok);
        check("flatten(π) is constant", r.poly.isConstant());
        check("flatten(π) piMul == 1", r.poly.coeffAt(0).piMul == 1);
    }

    // ── Test 31: ASTFlattener — Unary minus: -3x + 7 ───────────
    {
        ASTFlattener flat;
        auto row = vpam::makeRow();
        auto* rowPtr = static_cast<vpam::NodeRow*>(row.get());
        rowPtr->appendChild(vpam::makeOperator(vpam::OpKind::Sub)); // unary -
        rowPtr->appendChild(vpam::makeNumber("3"));
        rowPtr->appendChild(vpam::makeVariable('x'));
        rowPtr->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        rowPtr->appendChild(vpam::makeNumber("7"));

        auto r = flat.flatten(row.get());
        check("flatten(-3x+7) ok", r.ok);
        check("flatten(-3x+7) x coeff == -3", r.poly.coeffAt(1).num == -3);
        check("flatten(-3x+7) const == 7",    r.poly.coeffAt(0).num == 7);

        PRINT("  flatten(-3x+7) = ");
        PRINTLN(r.poly.toString().c_str());
    }

    // ── Test 32: ASTFlattener — Power expansion: (x+1)^2 = x²+2x+1
    {
        ASTFlattener flat;

        auto inner = vpam::makeRow();
        auto* ir = static_cast<vpam::NodeRow*>(inner.get());
        ir->appendChild(vpam::makeVariable('x'));
        ir->appendChild(vpam::makeOperator(vpam::OpKind::Add));
        ir->appendChild(vpam::makeNumber("1"));
        auto paren = vpam::makeParen(std::move(inner));

        auto pw = vpam::makePower(std::move(paren), vpam::makeNumber("2"));
        auto r = flat.flatten(pw.get());
        check("(x+1)^2 ok", r.ok);
        check("(x+1)^2 x² coeff == 1", r.poly.coeffAt(2).num == 1);
        check("(x+1)^2 x  coeff == 2", r.poly.coeffAt(1).num == 2);
        check("(x+1)^2 const == 1",    r.poly.coeffAt(0).num == 1);

        PRINT("  (x+1)^2 = ");
        PRINTLN(r.poly.toString().c_str());
    }

    // ════════════════════════════════════════════════════════════════
    //  Phase C — CASStepLogger + SingleSolver
    // ════════════════════════════════════════════════════════════════

    PRINTLN("\n────────────────────────────────────────────");
    PRINTLN("  Phase C — CASStepLogger + SingleSolver");
    PRINTLN("────────────────────────────────────────────");

    // ── Test 33: CASStepLogger — basic logging ────────────────────
    {
        CASStepLogger logger;
        SymPoly lhs = SymPoly::fromTerm(SymTerm::variable('x', 2, 1, 1));
        SymEquation snap(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
        logger.log("Paso de prueba", snap, MethodId::General);
        logger.logNote("Nota de prueba", MethodId::Linear);
        check("StepLogger count == 2", logger.count() == 2);
        check("StepLogger step[0] desc", logger.steps()[0].description == "Paso de prueba");
        check("StepLogger step[1] method == Linear",
              logger.steps()[1].method == MethodId::Linear);

        std::string dumped = logger.dump();
        check("StepLogger dump not empty", dumped.size() > 0);
        PRINT("  dump:\n");
        PRINT(dumped.c_str());
    }

    // ── Test 34: CASStepLogger — clear ────────────────────────────
    {
        CASStepLogger logger;
        logger.logNote("Temp");
        check("StepLogger before clear == 1", logger.count() == 1);
        logger.clear();
        check("StepLogger after clear == 0", logger.count() == 0);
    }

    // ── Test 35: SingleSolver — Linear: 5x - 10 = 0 → x = 2 ─────
    {
        // Build: 5x - 10 = 0
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 5, 1, 1));   // 5x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-10)));  // -10
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Linear 5x-10=0 ok", res.ok);
        check("Linear 5x-10=0 count == 1", res.count == 1);
        check("Linear 5x-10=0 x=2 (num)", res.solutions[0].num == 2);
        check("Linear 5x-10=0 x=2 (den)", res.solutions[0].den == 1);
        check("Linear 5x-10=0 not numeric", !res.numeric);
        check("Linear 5x-10=0 has steps", res.steps.count() >= 3);

        PRINTLN("  === Steps for 5x - 10 = 0 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 36: SingleSolver — Linear: 3x + 9 = 0 → x = -3 ─────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 3, 1, 1));   // 3x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(9)));  // +9
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Linear 3x+9=0 ok", res.ok);
        check("Linear 3x+9=0 count == 1", res.count == 1);
        check("Linear 3x+9=0 x=-3 (num)", res.solutions[0].num == -3);
        check("Linear 3x+9=0 x=-3 (den)", res.solutions[0].den == 1);
    }

    // ── Test 37: SingleSolver — Linear with fraction: 2x - 1 = 0 → x = 1/2
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 2, 1, 1));   // 2x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-1))); // -1
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Linear 2x-1=0 ok", res.ok);
        check("Linear 2x-1=0 x=1/2 (num)", res.solutions[0].num == 1);
        check("Linear 2x-1=0 x=1/2 (den)", res.solutions[0].den == 2);
    }

    // ── Test 38: SingleSolver — Quadratic: x² - 5x + 6 = 0 → x=2, x=3
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::variable('x', -5, 1, 1));  // -5x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(6)));  // +6
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Quad x²-5x+6=0 ok", res.ok);
        check("Quad x²-5x+6=0 count == 2", res.count == 2);
        check("Quad x²-5x+6=0 not numeric", !res.numeric);

        // Solutions should be 2 and 3 (order may vary based on formula)
        double s1 = res.solutions[0].toDouble();
        double s2 = res.solutions[1].toDouble();
        double lo = (s1 < s2) ? s1 : s2;
        double hi = (s1 < s2) ? s2 : s1;
        check("Quad x²-5x+6=0 root lo ≈ 2", std::abs(lo - 2.0) < 0.001);
        check("Quad x²-5x+6=0 root hi ≈ 3", std::abs(hi - 3.0) < 0.001);
        check("Quad x²-5x+6=0 has steps",    res.steps.count() >= 5);

        PRINTLN("  === Steps for x^2 - 5x + 6 = 0 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 39: SingleSolver — Quadratic: x² - 4 = 0 → x=±2 ────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-4))); // -4
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Quad x²-4=0 ok", res.ok);
        check("Quad x²-4=0 count == 2", res.count == 2);

        double s1 = res.solutions[0].toDouble();
        double s2 = res.solutions[1].toDouble();
        double lo = (s1 < s2) ? s1 : s2;
        double hi = (s1 < s2) ? s2 : s1;
        check("Quad x²-4=0 root lo ≈ -2", std::abs(lo - (-2.0)) < 0.001);
        check("Quad x²-4=0 root hi ≈  2", std::abs(hi - 2.0) < 0.001);
    }

    // ── Test 40: SingleSolver — Quadratic: x² + 1 = 0 → no real ──
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(1)));  // +1
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Quad x²+1=0 ok (no real)", res.ok);
        check("Quad x²+1=0 count == 0",   res.count == 0);

        PRINTLN("  === Steps for x^2 + 1 = 0 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 41: SingleSolver — Quadratic repeated root: x² - 4x + 4 = 0 → x = 2
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::variable('x', -4, 1, 1));  // -4x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(4)));  // +4
        lhs.normalize();

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("Quad x²-4x+4=0 ok", res.ok);
        check("Quad x²-4x+4=0 count == 1", res.count == 1);
        check("Quad x²-4x+4=0 root ≈ 2",
              std::abs(res.solutions[0].toDouble() - 2.0) < 0.001);
    }

    // ── Test 42: SingleSolver — Linear with RHS: 2x + 3 = 7 → x = 2
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 2, 1, 1));   // 2x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(3)));  // +3

        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(7));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("2x+3=7 ok", res.ok);
        check("2x+3=7 count == 1", res.count == 1);
        check("2x+3=7 x=2", res.solutions[0].num == 2 && res.solutions[0].den == 1);

        PRINTLN("  === Steps for 2x + 3 = 7 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 43: SingleSolver — Identity: 0 = 0 ──────────────────
    {
        SymPoly lhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("0=0 ok (identity)", res.ok);
        check("0=0 count == 0 (infinite solutions)", res.count == 0);
    }

    // ── Test 44: SingleSolver — Contradiction: 5 = 0 ─────────────
    {
        SymPoly lhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(5));
        SymPoly rhs = SymPoly::fromConstant(vpam::ExactVal::fromInt(0));
        SymEquation eq(lhs, rhs);

        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x');

        check("5=0 not ok (contradiction)", !res.ok);
        check("5=0 count == 0", res.count == 0);
    }

    // ════════════════════════════════════════════════════════════════
    //  Phase D — SystemSolver (2x2 + 3x3)
    // ════════════════════════════════════════════════════════════════

    PRINTLN("\n────────────────────────────────────────────");
    PRINTLN("  Phase D — SystemSolver");
    PRINTLN("────────────────────────────────────────────");

    // ── Test 45: analyzeAndChoose — ±1 coeff → Substitution ──────
    {
        // {x - y = 1, 2x + y = 5} → x has coeff 1 → Substitution
        SystemSolver sys;
        LinEq e1 = LinEq::from2(1, -1, 1);
        LinEq e2 = LinEq::from2(2, 1, 5);
        SystemMethod m = sys.analyzeAndChoose(e1, e2);
        check("Heuristic: ±1 coeff → Substitution",
              m == SystemMethod::Substitution);
    }

    // ── Test 46: analyzeAndChoose — same coeff → Reduction ───────
    {
        // {2x + 3y = 8, 4x + 3y = 10} → 3y same → Reduction
        SystemSolver sys;
        LinEq e1 = LinEq::from2(2, 3, 8);
        LinEq e2 = LinEq::from2(4, 3, 10);
        SystemMethod m = sys.analyzeAndChoose(e1, e2);
        check("Heuristic: same coeff → Reduction",
              m == SystemMethod::Reduction);
    }

    // ── Test 47: 2x2 Substitution — {2x + y = 5, x - y = 1} → x=2, y=1
    {
        SystemSolver sys;
        LinEq e1 = LinEq::from2(2, 1, 5);
        LinEq e2 = LinEq::from2(1, -1, 1);
        SystemResult res = sys.solve2x2(e1, e2, 'x', 'y');

        check("2x2 sub {2x+y=5,x-y=1} ok", res.ok);
        check("2x2 sub x=2",
              res.solutions[0].num == 2 && res.solutions[0].den == 1);
        check("2x2 sub y=1",
              res.solutions[1].num == 1 && res.solutions[1].den == 1);
        check("2x2 sub method == Substitution",
              res.methodUsed == SystemMethod::Substitution);
        check("2x2 sub has steps", res.steps.count() >= 5);

        PRINTLN("  === Steps for {2x+y=5, x-y=1} ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 48: 2x2 Reduction — {2x + 3y = 8, 4x + 3y = 10} → x=1, y=2
    {
        SystemSolver sys;
        LinEq e1 = LinEq::from2(2, 3, 8);
        LinEq e2 = LinEq::from2(4, 3, 10);
        SystemResult res = sys.solve2x2(e1, e2, 'x', 'y');

        check("2x2 red {2x+3y=8,4x+3y=10} ok", res.ok);
        check("2x2 red x=1",
              res.solutions[0].num == 1 && res.solutions[0].den == 1);
        check("2x2 red y=2",
              res.solutions[1].num == 2 && res.solutions[1].den == 1);
        check("2x2 red method == Reduction",
              res.methodUsed == SystemMethod::Reduction);

        PRINTLN("  === Steps for {2x+3y=8, 4x+3y=10} ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 49: 2x2 Incompatible — {x + y = 1, x + y = 3} → no solution
    {
        SystemSolver sys;
        LinEq e1 = LinEq::from2(1, 1, 1);
        LinEq e2 = LinEq::from2(1, 1, 3);
        SystemResult res = sys.solve2x2(e1, e2, 'x', 'y');

        check("2x2 incompatible not ok", !res.ok);
        check("2x2 incompatible has error", res.error.size() > 0);
    }

    // ── Test 50: 2x2 Fraction result — {3x + 2y = 7, x - y = 1} → x=9/5, y=4/5
    {
        // 3x + 2y = 7   (1)
        // x  - y  = 1   (2) → x = 1 + y
        // Sub: 3(1+y) + 2y = 7 → 3 + 3y + 2y = 7 → 5y = 4 → y = 4/5
        // x = 1 + 4/5 = 9/5
        SystemSolver sys;
        LinEq e1 = LinEq::from2(3, 2, 7);
        LinEq e2 = LinEq::from2(1, -1, 1);
        SystemResult res = sys.solve2x2(e1, e2, 'x', 'y');

        check("2x2 frac ok", res.ok);
        double sx = res.solutions[0].toDouble();
        double sy = res.solutions[1].toDouble();
        check("2x2 frac x ≈ 9/5", std::abs(sx - 1.8) < 0.001);
        check("2x2 frac y ≈ 4/5", std::abs(sy - 0.8) < 0.001);
    }

    // ── Test 51: 3x3 Gauss — {x+y+z=6, 2x-y+z=3, x+2y-z=2} → x=1,y=2,z=3
    {
        SystemSolver sys;
        LinEq e1 = LinEq::from3(1, 1, 1, 6);
        LinEq e2 = LinEq::from3(2, -1, 1, 3);
        LinEq e3 = LinEq::from3(1, 2, -1, 2);
        SystemResult res = sys.solve3x3(e1, e2, e3, 'x', 'y', 'z');

        check("3x3 Gauss {x+y+z=6,...} ok", res.ok);
        check("3x3 Gauss x=1",
              res.solutions[0].num == 1 && res.solutions[0].den == 1);
        check("3x3 Gauss y=2",
              res.solutions[1].num == 2 && res.solutions[1].den == 1);
        check("3x3 Gauss z=3",
              res.solutions[2].num == 3 && res.solutions[2].den == 1);
        check("3x3 Gauss method == Gauss",
              res.methodUsed == SystemMethod::Gauss);
        check("3x3 Gauss has steps", res.steps.count() >= 8);

        PRINTLN("  === Steps for 3x3 {x+y+z=6, 2x-y+z=3, x+2y-z=2} ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 52: 3x3 Singular — dependent system ────────────────
    {
        // Eq3 = Eq1 + Eq2 → singular
        SystemSolver sys;
        LinEq e1 = LinEq::from3(1, 1, 0, 3);
        LinEq e2 = LinEq::from3(0, 1, 1, 4);
        LinEq e3 = LinEq::from3(1, 2, 1, 7);   // = e1 + e2
        SystemResult res = sys.solve3x3(e1, e2, e3, 'x', 'y', 'z');

        check("3x3 singular not ok", !res.ok);
        check("3x3 singular has error", res.error.size() > 0);
    }

    // ── Test 53: 3x3 with pivot swap needed ─────────────────────
    {
        // First row has zero in first column → needs row swap
        // 0x + 2y + z = 5
        // 3x +  y - z = 2
        // x  -  y + 2z = 3
        // After swap R1↔R2 (or R1↔R3):
        // Solving: from 3x+y-z=2, x-y+2z=3, 2y+z=5
        // Let's verify: x-y+2z=3 (1'), 3x+y-z=2 (2'), 2y+z=5 (3')
        // From (1'): x = 3+y-2z
        // Sub into (2'): 3(3+y-2z)+y-z=2 → 9+3y-6z+y-z=2 → 4y-7z=-7 (4)
        // From (3'): y = (5-z)/2
        // Sub into (4): 4*(5-z)/2 -7z=-7 → 2(5-z)-7z=-7 → 10-2z-7z=-7 → -9z=-17 → z=17/9
        // y = (5-17/9)/2 = (45/9-17/9)/2 = (28/9)/2 = 14/9
        // x = 3 + 14/9 - 2*17/9 = 3 + 14/9 - 34/9 = 3 - 20/9 = 27/9 - 20/9 = 7/9
        SystemSolver sys;
        LinEq e1 = LinEq::from3(0, 2, 1, 5);
        LinEq e2 = LinEq::from3(3, 1, -1, 2);
        LinEq e3 = LinEq::from3(1, -1, 2, 3);
        SystemResult res = sys.solve3x3(e1, e2, e3, 'x', 'y', 'z');

        check("3x3 pivot swap ok", res.ok);
        double rx = res.solutions[0].toDouble();
        double ry = res.solutions[1].toDouble();
        double rz = res.solutions[2].toDouble();
        check("3x3 pivot x ≈ 7/9", std::abs(rx - 7.0/9.0) < 0.001);
        check("3x3 pivot y ≈ 14/9", std::abs(ry - 14.0/9.0) < 0.001);
        check("3x3 pivot z ≈ 17/9", std::abs(rz - 17.0/9.0) < 0.001);
    }

#ifdef ARDUINO
    size_t psramAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    PRINT("PSRAM free after:  ");
    PRINTLN(String(psramAfter).c_str());
    PRINT("PSRAM used by tests: ");
    PRINTLN(String(psramBefore - psramAfter).c_str());
#endif

    PRINTLN("════════════════════════════════════════════");
    PRINT("  Results: ");
    char buf[40];
    snprintf(buf, sizeof(buf), "%d passed, %d failed", _passed, _failed);
    PRINTLN(buf);
    PRINTLN("════════════════════════════════════════════");
}

} // namespace cas
