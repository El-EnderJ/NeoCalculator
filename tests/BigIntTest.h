/**
 * BigIntTest.h — Phase 1 unit tests: CASInt + CASRational.
 *
 * Call cas::runBigIntTests() from setup() after Serial is ready.
 * Gated by -DCAS_RUN_TESTS.
 */

#pragma once

namespace cas {

/// Run all Phase 1 BigInt/Rational unit tests, printing results to Serial.
void runBigIntTests();

} // namespace cas
