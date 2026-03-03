/**
 * CASStepLogger.cpp — Implementation of step-by-step CAS logger.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase C+
 */

#include "CASStepLogger.h"

namespace cas {

// ────────────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────────────

CASStepLogger::CASStepLogger()
    : _steps() {}

// ────────────────────────────────────────────────────────────────────
// log — Record a transform step with equation snapshot
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::log(const std::string& description,
                        const SymEquation& snapshot,
                        MethodId method) {
    _steps.emplace_back(description, snapshot, method, StepKind::Transform);
}

// ────────────────────────────────────────────────────────────────────
// logNote — Record an annotation step (text-only, no equation)
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logNote(const std::string& note, MethodId method) {
    _steps.emplace_back(note, SymEquation(), method, StepKind::Annotation);
}

// ────────────────────────────────────────────────────────────────────
// logResult — Record a final result step with equation snapshot
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logResult(const std::string& description,
                              const SymEquation& snapshot,
                              MethodId method) {
    _steps.emplace_back(description, snapshot, method, StepKind::Result);
}

// ────────────────────────────────────────────────────────────────────
// logComplex — Record a complex root result (text-only)
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logComplex(const std::string& description,
                               MethodId method) {
    _steps.emplace_back(description, SymEquation(), method, StepKind::ComplexResult);
}

// ────────────────────────────────────────────────────────────────────
// copyStep — Copy a step preserving its original StepKind
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::copyStep(const CASStep& step) {
    _steps.emplace_back(step.description, step.snapshot, step.method, step.kind);
}

// ────────────────────────────────────────────────────────────────────
// clear
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::clear() {
    _steps.clear();
}

// ────────────────────────────────────────────────────────────────────
// dump — Returns all steps as multi-line string
// ────────────────────────────────────────────────────────────────────

std::string CASStepLogger::dump() const {
    std::string out;
    int i = 1;
    for (const auto& step : _steps) {
        out += "Step " + std::to_string(i++) + ": " + step.description + "\n";
        // Only show equation for Transform and Result steps
        if (step.kind == StepKind::Transform || step.kind == StepKind::Result) {
            std::string eqStr = step.snapshot.toString();
            if (!eqStr.empty() && eqStr != "0 = 0") {
                out += "  => " + eqStr + "\n";
            }
        }
    }
    return out;
}

} // namespace cas
