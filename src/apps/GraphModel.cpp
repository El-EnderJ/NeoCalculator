/**
 * GraphModel.cpp — Grapher MVC: Model implementation
 */
#include "GraphModel.h"
#include <cstring>
#include <cmath>

namespace grapher {

// ── Helpers ────────────────────────────────────────────────────────────────

const char* GraphModel::getExprRHS(const char* text) {
    if (!text) return nullptr;
    const char* eq = strchr(text, '=');
    if (!eq) return nullptr;
    const char* rhs = eq + 1;
    while (*rhs == ' ' || *rhs == '\t') ++rhs;
    if (!*rhs) return nullptr;
    return rhs;
}

// ── preCacheRPN ────────────────────────────────────────────────────────────

void GraphModel::preCacheRPN(CartesianFunction& fn) {
    fn.rpnValid = false;
    fn.rpn.clear();
    fn.valid = false;

    if (fn.len <= 0) return;

    const char* rhs = getExprRHS(fn.text);
    if (!rhs) return;

    TokenizeResult tr = _tok.tokenize(rhs);
    if (!tr.ok) return;

    ParseResult pr = _par.toRPN(tr.tokens);
    if (!pr.ok) return;

    fn.rpn = pr.outputRPN;
    fn.rpnValid = true;
    fn.valid = true;
}

// ── evalAt ─────────────────────────────────────────────────────────────────

float GraphModel::evalAt(CartesianFunction& fn, float x) {
    if (!fn.valid) return NAN;

    _vars.setVar('x', (double)x);

    // Fast path: use pre-compiled RPN
    if (fn.rpnValid && !fn.rpn.empty()) {
        EvalResult er = _eval.evaluateRPN(fn.rpn, _vars);
        return er.ok ? (float)er.value : NAN;
    }

    // Fallback: full parse (shouldn't normally be reached after preCacheRPN)
    const char* rhs = getExprRHS(fn.text);
    if (!rhs) return NAN;

    TokenizeResult tr = _tok.tokenize(rhs);
    if (!tr.ok) return NAN;
    ParseResult pr = _par.toRPN(tr.tokens);
    if (!pr.ok) return NAN;
    EvalResult er = _eval.evaluateRPN(pr.outputRPN, _vars);
    return er.ok ? (float)er.value : NAN;
}

} // namespace grapher
