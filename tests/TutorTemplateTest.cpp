/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * TutorTemplateTest.cpp — Unit tests for the Educational Tutor Engine.
 */

#include "TutorTemplateTest.h"
#include "../src/math/cas/SymPoly.h"
#include "../src/math/cas/SymEquation.h"
#include "../src/math/cas/SymExpr.h"
#include "../src/math/cas/SingleSolver.h"
#include "../src/math/cas/SymExprArena.h"
#include "../src/math/cas/OmniSolver.h"
#include "../src/math/cas/SystemSolver.h"
#include "../src/math/cas/TutorTemplates.h"
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

static const CASStep* findStepByDescription(const StepVec& steps, const char* description) {
    for (const auto& step : steps) {
        if (step.description == description) return &step;
    }
    return nullptr;
}

/// NB-4 helper: solve lhs = 0 through SingleSolver and structurally verify
/// the "Identifying coefficients:" CoeffAssign slots (value + sign each),
/// plus the expected real-root count. Fails if the step is missing, the
/// node type changed, a slot is empty, or a value landed in the wrong slot.
static void checkQuadCoeffStep(const char* label, const SymPoly& lhs,
                               double ea, double eb, double ec,
                               int expectedRoots) {
    SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
    SymExprArena arena;
    SingleSolver solver;
    SolveResult res = solver.solve(eq, 'x', &arena);

    bool ok = res.ok && res.count == expectedRoots;
    const CASStep* step =
        findStepByDescription(res.steps.steps(), "Identifying coefficients:");
    ok = ok && step && step->mathExpr &&
         step->mathExpr->type == SymExprType::CoeffAssign;
    if (ok) {
        const auto* ca = static_cast<const SymCoeffAssign*>(step->mathExpr);
        ok = ca->aVal && ca->bVal && ca->cVal &&
             std::abs(ca->aVal->evaluate(0.0) - ea) < 1e-9 &&
             std::abs(ca->bVal->evaluate(0.0) - eb) < 1e-9 &&
             std::abs(ca->cVal->evaluate(0.0) - ec) < 1e-9;
    }
    check(label, ok);
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
        std::string quadDump = res.steps.dump();
        check("QuadTutor suppresses raw original-equation label",
              quadDump.find("Original equation") == std::string::npos);
        check("QuadTutor suppresses raw normalization noise",
              quadDump.find("Moving and grouping all terms to the left side") == std::string::npos);
        check("QuadTutor uses ASCII root labels",
              quadDump.find("Separating into x1 and x2:") != std::string::npos);
        check("QuadTutor avoids Unicode subscript glyphs in labels",
              quadDump.find("₁") == std::string::npos &&
              quadDump.find("₂") == std::string::npos);

        const auto& quadSteps = res.steps.steps();
        const SymExpr* coeffExpr = nullptr;
        for (const auto& step : quadSteps) {
            if (step.description == "Identifying coefficients:") {
                coeffExpr = step.mathExpr;
                break;
            }
        }
        check("QuadTutor identifies coefficients with math expression", coeffExpr != nullptr);
        // NB-4: the step's math payload is the structured SymCoeffAssign node
        // (renders as "a = 1, b = -5, c = 6"; slot order is fixed by the node
        // itself). Verify each named slot's value and sign structurally —
        // the old substring-position checks asserted a polynomial rendering
        // this step stopped producing when the CoeffAssign node landed.
        check("QuadTutor coefficient step is a CoeffAssign node",
              coeffExpr && coeffExpr->type == SymExprType::CoeffAssign);
        if (coeffExpr && coeffExpr->type == SymExprType::CoeffAssign) {
            const auto* ca = static_cast<const SymCoeffAssign*>(coeffExpr);
            check("QuadTutor coefficient slot a == 1",
                  ca->aVal && std::abs(ca->aVal->evaluate(0.0) - 1.0) < 1e-12);
            check("QuadTutor coefficient slot b == -5",
                  ca->bVal && std::abs(ca->bVal->evaluate(0.0) + 5.0) < 1e-12);
            check("QuadTutor coefficient slot c == 6",
                  ca->cVal && std::abs(ca->cVal->evaluate(0.0) - 6.0) < 1e-12);
        }

        const CASStep* substitutionStep =
            findStepByDescription(quadSteps, "Substituting the coefficient values:");
        check("QuadTutor logs substitution math expression", substitutionStep && substitutionStep->mathExpr);
        if (substitutionStep && substitutionStep->mathExpr &&
            substitutionStep->mathExpr->type == SymExprType::Mul) {
            const auto* mulExpr = static_cast<const SymMul*>(substitutionStep->mathExpr);
            check("QuadTutor substitution keeps fraction structure", mulExpr->count == 2);
            if (mulExpr->count == 2 &&
                mulExpr->factors[0]->type == SymExprType::PlusMinus &&
                mulExpr->factors[1]->type == SymExprType::Pow) {
                const auto* numerator = static_cast<const SymPlusMinus*>(mulExpr->factors[0]);
                const auto* reciprocalDen = static_cast<const SymPow*>(mulExpr->factors[1]);
                check("QuadTutor wraps negated negative b as -(-5)",
                      numerator->lhs->toString() == "(-(-5))");
                check("QuadTutor wraps b squared as (-5)^2",
                      numerator->rhs->type == SymExprType::Pow &&
                      static_cast<const SymPow*>(numerator->rhs)->base->type == SymExprType::Add &&
                      static_cast<const SymAdd*>(static_cast<const SymPow*>(numerator->rhs)->base)->terms[0]->toString() == "((-5)^2)");
                check("QuadTutor separates numeric multiplication in 4ac",
                      numerator->rhs->type == SymExprType::Pow &&
                      static_cast<const SymPow*>(numerator->rhs)->base->type == SymExprType::Add &&
                      static_cast<const SymAdd*>(static_cast<const SymPow*>(numerator->rhs)->base)->terms[1]->toString() == "(-(4 * (1) * (6)))");
                check("QuadTutor separates numeric multiplication in 2a",
                      reciprocalDen->base->toString() == "(2 * (1))");
            }
        }
        
        PRINTLN("  === Tutor Steps for x^2 - 5x + 6 = 0 ===");
        PRINT(res.steps.dump().c_str());
    }

    // ── Test 1b: coefficient-step slots across quadratic shapes (NB-4) ──
    {
        // 2x² + 3x - 4: non-unit leading coefficient
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 2, 1, 2));
        p.terms().push_back(SymTerm::variable('x', 3, 1, 1));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-4)));
        p.normalize();
        checkQuadCoeffStep("CoeffStep 2x²+3x-4 → (2, 3, -4)", p, 2, 3, -4, 2);
    }
    {
        // x² - 9: omitted linear term (b == 0)
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 1, 1, 2));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-9)));
        p.normalize();
        checkQuadCoeffStep("CoeffStep x²-9 → (1, 0, -9)", p, 1, 0, -9, 2);
    }
    {
        // x² + 4x: omitted constant term (c == 0)
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', 1, 1, 2));
        p.terms().push_back(SymTerm::variable('x', 4, 1, 1));
        p.normalize();
        checkQuadCoeffStep("CoeffStep x²+4x → (1, 4, 0)", p, 1, 4, 0, 2);
    }
    {
        // 6 - 5x + x²: input terms in reverse order — extraction must be
        // order-independent and land each value in the right slot.
        SymPoly p('x');
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(6)));
        p.terms().push_back(SymTerm::variable('x', -5, 1, 1));
        p.terms().push_back(SymTerm::variable('x', 1, 1, 2));
        p.normalize();
        checkQuadCoeffStep("CoeffStep 6-5x+x² → (1, -5, 6)", p, 1, -5, 6, 2);
    }
    {
        // -x² + 4: negative leading coefficient
        SymPoly p('x');
        p.terms().push_back(SymTerm::variable('x', -1, 1, 2));
        p.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(4)));
        p.normalize();
        checkQuadCoeffStep("CoeffStep -x²+4 → (-1, 0, 4)", p, -1, 0, 4, 2);
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

    // ── Test 4: ln(x) = 1 (Logarithmic tutor) ──────────────────────
    {
        SymExprArena arena;
        OmniSolver solver;
        SymExpr* lhs = symFunc(arena, SymFuncKind::Ln, symVar(arena, 'x'));
        SymExpr* rhs = symInt(arena, 1);
        OmniResult res = solver.solve(lhs, rhs, 'x', arena);

        check("LogTutor ln(x)=1 ok", res.ok);
        check("LogTutor ln(x)=1 count == 1", res.solutions.size() == 1);
        if (!res.solutions.empty()) {
            check("LogTutor ln(x)=1 root ≈ e",
                  std::abs(res.solutions[0].numeric - std::exp(1.0)) < 0.01);
        }
        check("LogTutor logs conversion step",
              res.steps.dump().find("Converting the logarithmic equation to exponential form") != std::string::npos);
    }

    // ── Test 5: 2·e^x = 4 (Exponential tutor) ──────────────────────
    {
        SymExprArena arena;
        OmniSolver solver;
        SymExpr* lhs = symMul(arena, symInt(arena, 2),
                              symFunc(arena, SymFuncKind::Exp, symVar(arena, 'x')));
        SymExpr* rhs = symInt(arena, 4);
        OmniResult res = solver.solve(lhs, rhs, 'x', arena);

        check("ExpTutor 2e^x=4 ok", res.ok);
        check("ExpTutor 2e^x=4 count == 1", res.solutions.size() == 1);
        if (!res.solutions.empty()) {
            check("ExpTutor 2e^x=4 root ≈ ln(2)",
                  std::abs(res.solutions[0].numeric - std::log(2.0)) < 0.01);
        }
        check("ExpTutor logs logarithm step",
              res.steps.dump().find("Taking logarithms on both sides") != std::string::npos);
    }

    // ── Test 6: sqrt(x+1) = x-1 (Radical tutor + extraneous) ───────
    {
        SymExprArena arena;
        OmniSolver solver;
        SymExpr* lhs = symPow(arena,
                              symAdd(arena, symVar(arena, 'x'), symInt(arena, 1)),
                              symFrac(arena, 1, 2));
        SymExpr* rhs = symAdd(arena, symVar(arena, 'x'), symInt(arena, -1));
        OmniResult res = solver.solve(lhs, rhs, 'x', arena);

        check("RadicalTutor sqrt(x+1)=x-1 ok", res.ok);
        check("RadicalTutor sqrt(x+1)=x-1 count == 1", res.solutions.size() == 1);
        if (!res.solutions.empty()) {
            double numeric = res.solutions[0].isExact
                ? res.solutions[0].exact.toDouble()
                : res.solutions[0].numeric;
            check("RadicalTutor sqrt(x+1)=x-1 root ≈ 3", std::abs(numeric - 3.0) < 0.01);
        }
        std::string radicalDump = res.steps.dump();
        check("RadicalTutor reports extraneous false candidate",
              radicalDump.find("FALSE") != std::string::npos);
        check("RadicalTutor reports true candidate",
              radicalDump.find("TRUE") != std::string::npos);
    }

    {
        SymExprArena arena;
        OmniSolver solver;
        SymExpr* radical = symPow(arena,
                                  symAdd(arena, symVar(arena, 'x'), symInt(arena, 5)),
                                  symFrac(arena, 1, 2));
        SymExpr* lhs = symAdd(arena,
                              symAdd(arena, radical, symVar(arena, 'x')),
                              symInt(arena, -1));
        SymExpr* rhs = symInt(arena, 6);
        OmniResult res = solver.solve(lhs, rhs, 'x', arena);

        check("RadicalTutor sqrt(x+5)+x-1=6 ok", res.ok);
        check("RadicalTutor sqrt(x+5)+x-1=6 count == 1", res.solutions.size() == 1);
        if (!res.solutions.empty()) {
            double numeric = res.solutions[0].isExact
                ? res.solutions[0].exact.toDouble()
                : res.solutions[0].numeric;
            check("RadicalTutor sqrt(x+5)+x-1=6 root ~= 4", std::abs(numeric - 4.0) < 0.01);
        }
    }

    // ── Test 7: 3x² - 7x + 2 = 0 (Stress: Fractional root) ──────
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

    // ── Test 8: x³ - 6x² + 11x - 6 = 0 (Stress: Cubic/Ruffini) ──
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
        if (res.count == 3) {
            bool has1 = false, has2 = false, has3 = false;
            for (uint8_t i = 0; i < res.count; ++i) {
                double v = res.solutions[i].toDouble();
                if (std::abs(v - 1.0) < 0.001) has1 = true;
                if (std::abs(v - 2.0) < 0.001) has2 = true;
                if (std::abs(v - 3.0) < 0.001) has3 = true;
            }
            check("Cubic roots are {1, 2, 3}", has1 && has2 && has3);
        }
        check("Cubic has Ruffini steps", res.steps.count() > 10);
    }

    // ── Test 8b: same cubic, terms pushed in reverse order (NB-5) ──
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-6))); // -6
        lhs.terms().push_back(SymTerm::variable('x', 11, 1, 1));  // 11x
        lhs.terms().push_back(SymTerm::variable('x', -6, 1, 2));  // -6x²
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 3));   // x³
        lhs.normalize();

        SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("Cubic reordered terms ok", res.ok);
        check("Cubic reordered terms count == 3", res.count == 3);
    }

    // ── Test 8c: x³ - 1 = 0 — one real root + complex pair (NB-5) ──
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 3));   // x³
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-1))); // -1
        lhs.normalize();

        SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        check("Cubic x³-1=0 ok", res.ok);
        check("Cubic x³-1=0 one real root", res.count == 1);
        check("Cubic x³-1=0 root ≈ 1",
              res.count >= 1 && std::abs(res.solutions[0].toDouble() - 1.0) < 0.001);
        check("Cubic x³-1=0 flags complex pair", res.hasComplexRoots);
    }

    // ── Test 8d: x³ + x + 11 = 0 — no integer root: honest refusal (NB-5) ──
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 3));   // x³
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 1));   // +x
        lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(11))); // +11
        lhs.normalize();

        SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
        SymExprArena arena;
        PedagogicalLogger tutorLog;
        SolveResult res = solveCubicTutor(eq, 'x', tutorLog, &arena);

        check("Cubic no-integer-root refuses (ok == false)", !res.ok);
        check("Cubic no-integer-root has typed error", !res.error.empty());
        check("Cubic no-integer-root returns no fabricated roots", res.count == 0);
    }

    // ── Test 8e: NB-5 cubic repeated — no lifetime/state leakage ──
    {
        bool stable = true;
        for (int iter = 0; iter < 25 && stable; ++iter) {
            SymPoly lhs('x');
            lhs.terms().push_back(SymTerm::variable('x', 1, 1, 3));
            lhs.terms().push_back(SymTerm::variable('x', -6, 1, 2));
            lhs.terms().push_back(SymTerm::variable('x', 11, 1, 1));
            lhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-6)));
            lhs.normalize();

            SymEquation eq(lhs, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
            SymExprArena arena;
            SingleSolver solver;
            SolveResult res = solver.solve(eq, 'x', &arena);
            stable = res.ok && res.count == 3;
        }
        check("Cubic NB-5 case stable across 25 repeats", stable);
    }

    // ── Test 9: 2x + 3y = 8, x - y = 1 (Stress: 2x2 Cramer) ──────
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

    // ── Test 10: Pre-processing logs set-to-zero and combine-like-terms ─────
    {
        SymPoly lhs('x');
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 2));   // x²
        lhs.terms().push_back(SymTerm::variable('x', 1, 1, 1));   // +x

        SymPoly rhs('x');
        rhs.terms().push_back(SymTerm::variable('x', 6, 1, 1));   // 6x
        rhs.terms().push_back(SymTerm::constant(vpam::ExactVal::fromInt(-6))); // -6

        SymEquation eq(lhs, rhs);
        SymExprArena arena;
        SingleSolver solver;
        SolveResult res = solver.solve(eq, 'x', &arena);

        std::string dump = res.steps.dump();
        check("PreProcess logs set-to-zero step",
              dump.find("Moving all terms to the left to equate to zero") != std::string::npos);
        check("PreProcess logs combine-like-terms step",
              dump.find("Combining like terms") != std::string::npos);
    }

    PRINT("\nTotal Passed: "); PRINTLN(std::to_string(_passed).c_str());
    PRINT("Total Failed: "); PRINTLN(std::to_string(_failed).c_str());
}

} // namespace cas
