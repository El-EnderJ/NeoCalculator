/**
 * StepLogger.h
 * Registro estructurado de pasos algebraicos / numéricos para Show Steps.
 */

#pragma once

#include <Arduino.h>
#include <vector>

struct Step {
    String description;   // ej. "Restar 5 a ambos lados"
    String equationState; // ej. "2x = 10" o "x1=1.5, f(x1)=..."
};

class StepLogger {
public:
    void clear();
    void addStep(const String &desc, const String &state);

    size_t size() const { return _steps.size(); }
    const Step &get(size_t idx) const { return _steps[idx]; }

private:
    std::vector<Step> _steps;
};

