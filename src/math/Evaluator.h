/**
 * Evaluator.h
 * RPN evaluator for calculator expressions, using double precision.
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <vector>
#include "Tokenizer.h"
#include "VariableContext.h"

enum class AngleMode : uint8_t {
    RAD = 0,
    DEG = 1
};

struct EvalResult {
    bool ok = false;
    String errorMessage;
    double value = 0.0;
};

class Evaluator {
public:
    Evaluator();

    void setAngleMode(AngleMode mode) { _angleMode = mode; }

    EvalResult evaluateRPN(const std::vector<Token> &rpn, VariableContext &vars);

private:
    static constexpr size_t STACK_MAX = 64;

    AngleMode _angleMode;

    bool push(double stack[], size_t &sp, double v);
    bool pop(double stack[], size_t &sp, double &out);

    bool resolveIdentifier(const String &name, VariableContext &vars, double &out, String &err);
    double toRadians(double x) const;
};

