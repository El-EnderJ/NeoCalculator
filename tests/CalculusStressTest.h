/**
 * CalculusStressTest.h — Phase 6 memory stress test.
 *
 * 50 symbolic differentiations in a loop, verifying that arena.reset()
 * properly releases memory and no PSRAM leaks occur.
 *
 * Call cas::runCalculusStressTest() from setup() with -DCAS_RUN_TESTS.
 */

#pragma once

namespace cas {

/// Run 50-iteration differentiation stress test with memory tracking.
void runCalculusStressTest();

} // namespace cas
