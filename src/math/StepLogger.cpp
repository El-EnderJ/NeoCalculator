/**
 * StepLogger.cpp
 */

#include "StepLogger.h"

void StepLogger::clear() {
    _steps.clear();
}

void StepLogger::addStep(const String &desc, const String &state) {
    Step s;
    s.description = desc;
    s.equationState = state;
    _steps.push_back(s);
}

