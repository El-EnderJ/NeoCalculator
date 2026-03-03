/**
 * SymDiffTest.h — Header for Phase 3 unit tests.
 *
 * Covers SymDiff (symbolic differentiation), SymSimplify (algebraic
 * simplifier), and SymExprToAST (SymExpr → MathAST conversion).
 *
 * Call cas::runSymDiffTests() from setup() after Serial is ready.
 * Requires -DCAS_RUN_TESTS build flag.
 */

#pragma once

namespace cas {

/// Run all Phase 3 unit tests (SymDiff, SymSimplify, SymExprToAST).
void runSymDiffTests();

} // namespace cas
