/**
 * OmniSolverTest.h — Header for Phase 4 unit tests.
 *
 * Covers OmniSolver (classification + dispatch), HybridNewton
 * (Newton-Raphson with exact symbolic Jacobian), and the
 * algebraic inverse path.
 *
 * Call cas::runOmniSolverTests() from setup() after Serial is ready.
 * Requires -DCAS_RUN_TESTS build flag.
 */

#pragma once

namespace cas {

/// Run all Phase 4 unit tests (OmniSolver, HybridNewton, Inverse).
void runOmniSolverTests();

} // namespace cas
