/**
 * GraphModel.h — Grapher MVC: Model layer
 *
 * Defines CartesianFunction (a single graphable function with its compiled
 * RPN cache) and GraphModel (the evaluation engine).
 *
 * This module knows nothing about pixels, LVGL, or screen coordinates.
 * It is the mathematical core of the Grapher (analogous to Poincaré in
 * NumWorks' architecture).
 */
#pragma once

#include <vector>
#include <cmath>
#include "../math/Tokenizer.h"
#include "../math/Parser.h"
#include "../math/Evaluator.h"
#include "../math/VariableContext.h"

namespace grapher {

/**
 * CartesianFunction — one graphable function slot.
 *
 * Holds the serialised expression text, its compiled RPN cache for fast
 * evaluation, the display colour and validity flag.
 */
struct CartesianFunction {
    char     text[64];   ///< Expression string (e.g. "y=2*x+3")
    int      len;        ///< strlen(text)
    bool     valid;      ///< Expression is parsed and evaluable
    uint32_t color;      ///< Plot colour (RGB888)
    bool     visible;    ///< User visibility toggle

    // Cached RPN — avoids re-tokenising on every evalAt() call
    std::vector<Token> rpn;
    bool               rpnValid;

    CartesianFunction()
        : len(0), valid(false), color(0xFF0000), visible(true), rpnValid(false)
    { text[0] = '\0'; }
};

/**
 * GraphModel — mathematical state and evaluation engine.
 *
 * Owns the Tokenizer/Parser/Evaluator/VariableContext required to compile
 * and evaluate CartesianFunction expressions.  The function array itself
 * lives in GrapherApp (the controller) so the UI can access text/color
 * without going through the model; GrapherApp passes individual
 * CartesianFunction references into the model's methods.
 */
class GraphModel {
public:
    GraphModel() = default;

    /**
     * Compile the RHS of fn.text into fn.rpn.
     * Sets fn.rpnValid and fn.valid accordingly.
     */
    void preCacheRPN(CartesianFunction& fn);

    /**
     * Evaluate fn at x using its cached RPN.
     * Falls back to a full parse if the cache is missing.
     * Returns NAN on any error.
     */
    float evalAt(CartesianFunction& fn, float x);

private:
    Tokenizer      _tok;
    Parser         _par;
    Evaluator      _eval;
    VariableContext _vars;

    /// Return pointer to the RHS of expr (after '='), or nullptr.
    static const char* getExprRHS(const char* expr);
};

} // namespace grapher
