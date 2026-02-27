/**
 * CASStepLogger.h — Step-by-step logging engine for CAS-Lite solvers.
 *
 * Each algebraic manipulation is recorded as a CASStep containing:
 *   · description  — Human-readable explanation (in Spanish)
 *   · snapshot      — SymEquation state after this step
 *   · methodId      — Solver method that generated this step
 *
 * All steps are stored in PSRAM via PSRAMAllocator to free internal
 * SRAM for LVGL and real-time tasks.
 *
 * Part of: NumOS CAS-Lite — Phase C (SingleSolver + Steps)
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "PSRAMAllocator.h"
#include "SymEquation.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// MethodId — Identifies which solver generated a step
// ════════════════════════════════════════════════════════════════════

enum class MethodId : uint8_t {
    General      = 0,   // Normalization, rearrangement, etc.
    Linear       = 1,   // First-degree solver (ax + b = 0)
    Quadratic    = 2,   // Second-degree solver (ax² + bx + c = 0)
    Newton       = 3,   // Numeric fallback (Newton-Raphson)
    Substitution = 4,   // System solver — substitution method
    Reduction    = 5,   // System solver — reduction (elimination) method
    Gauss        = 6,   // System solver — Gaussian elimination (3x3)
};

// ════════════════════════════════════════════════════════════════════
// CASStep — A single algebraic manipulation step
// ════════════════════════════════════════════════════════════════════

struct CASStep {
    std::string  description;   // "Moviendo constantes al lado derecho"
    SymEquation  snapshot;      // Equation state after this step
    MethodId     method;        // Which solver phase generated this

    CASStep()
        : description(), snapshot(), method(MethodId::General) {}

    CASStep(const std::string& desc, const SymEquation& snap, MethodId m)
        : description(desc), snapshot(snap), method(m) {}
};

// PSRAM-allocated step vector
using StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>;

// ════════════════════════════════════════════════════════════════════
// CASStepLogger — Accumulates algebraic steps in PSRAM
// ════════════════════════════════════════════════════════════════════

class CASStepLogger {
public:
    CASStepLogger();

    /// Add a step with description, equation snapshot, and method.
    void log(const std::string& description,
             const SymEquation& snapshot,
             MethodId method = MethodId::General);

    /// Add a step with just a text message (no equation snapshot).
    void logNote(const std::string& note, MethodId method = MethodId::General);

    /// Access all recorded steps.
    const StepVec& steps() const { return _steps; }

    /// Number of recorded steps.
    size_t count() const { return _steps.size(); }

    /// Clear all steps (e.g., before solving a new equation).
    void clear();

    /// Print all steps to a string (for Serial debugging / test output).
    std::string dump() const;

private:
    StepVec _steps;
};

} // namespace cas
