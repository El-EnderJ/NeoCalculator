/**
 * SymExprTest.h — Header for SymExpr Phase 1 unit tests.
 *
 * Call cas::runSymExprTests() from setup() after Serial is ready.
 * Requires -DCAS_RUN_TESTS build flag.
 */

#pragma once

namespace cas {

/// Run all SymExpr Phase 1 unit tests, printing results to Serial.
void runSymExprTests();

} // namespace cas
