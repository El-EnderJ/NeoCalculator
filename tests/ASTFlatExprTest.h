/**
 * ASTFlatExprTest.h — Header for ASTFlattener dual-mode Phase 2 tests.
 *
 * Call cas::runASTFlatExprTests() from setup() after Serial is ready.
 * Requires -DCAS_RUN_TESTS build flag.
 */

#pragma once

namespace cas {

/// Run all ASTFlattener dual-mode Phase 2 unit tests.
void runASTFlatExprTests();

} // namespace cas
