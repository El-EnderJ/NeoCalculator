/**
 * CASStepLogger.cpp — Implementation of step-by-step CAS logger.
 *
 * Part of: NumOS CAS-Lite — Phase C
 */

#include "CASStepLogger.h"

namespace cas {

// ────────────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────────────

CASStepLogger::CASStepLogger()
    : _steps() {}

// ────────────────────────────────────────────────────────────────────
// log — Record a solver step with equation snapshot
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::log(const std::string& description,
                        const SymEquation& snapshot,
                        MethodId method) {
    _steps.emplace_back(description, snapshot, method);
}

// ────────────────────────────────────────────────────────────────────
// logNote — Record a text-only step (empty equation)
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logNote(const std::string& note, MethodId method) {
    _steps.emplace_back(note, SymEquation(), method);
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
        out += "Paso " + std::to_string(i++) + ": " + step.description + "\n";
        std::string eqStr = step.snapshot.toString();
        if (!eqStr.empty() && eqStr != "0 = 0") {
            out += "  => " + eqStr + "\n";
        }
    }
    return out;
}

} // namespace cas
