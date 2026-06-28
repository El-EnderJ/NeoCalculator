/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * GraphModel.cpp — Grapher MVC: Model implementation
 */
#include "GraphModel.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

namespace grapher {

// ── Helpers ────────────────────────────────────────────────────────────────

// The numeric Parser (Parser.cpp) is binary-only — it has no unary-minus rule,
// so a function whose rhs starts with a sign ("-x", "-2x", "sin(-x)") fails to
// compile and never plots. Rewrite such a leading/parenthesised unary +/- into
// a parser-safe binary form by inserting a '0' operand ("-2x" -> "0-2x"). Only
// positions where "0<sign>" provably preserves both value AND precedence are
// rewritten: expression start, right after '(' , or right after '=' (after '='
// the rhs sign is effectively leading). Signs following a value or a *, /, ^ or
// another sign are left untouched, so this can never change the value of an
// already well-formed expression — it only rescues ones that were invalid.
static std::string normalizeLeadingUnary(const char* rhs) {
    std::string out;
    char prevSig = '\0';
    for (const char* p = rhs; *p; ++p) {
        char c = *p;
        if (c == ' ' || c == '\t') { out += c; continue; }
        if ((c == '-' || c == '+') &&
            (prevSig == '\0' || prevSig == '(' || prevSig == '=')) {
            out += '0';
        }
        out += c;
        prevSig = c;
    }
    return out;
}

const char* GraphModel::getExprRHS(const char* text) {
    if (!text) return nullptr;
    // An explicit "lhs = rhs" plots the rhs; a bare expression (no '=') is
    // treated as the implicit function y = <expr>, so "x" plots as y=x and
    // "2x" as y=2x. The whole (leading-trimmed) string becomes the rhs.
    const char* eq = strchr(text, '=');
    const char* rhs = eq ? eq + 1 : text;
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

    std::string expr = normalizeLeadingUnary(rhs);
    TokenizeResult tr = _tok.tokenize(expr.c_str());
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

    std::string expr = normalizeLeadingUnary(rhs);
    TokenizeResult tr = _tok.tokenize(expr.c_str());
    if (!tr.ok) return NAN;
    ParseResult pr = _par.toRPN(tr.tokens);
    if (!pr.ok) return NAN;
    EvalResult er = _eval.evaluateRPN(pr.outputRPN, _vars);
    return er.ok ? (float)er.value : NAN;
}

// ── Difference function: f1(x) - f2(x) ────────────────────────────────────

float GraphModel::diffAt(CartesianFunction& fn1, CartesianFunction& fn2, float x) {
    float y1 = evalAt(fn1, x);
    float y2 = evalAt(fn2, x);
    if (std::isnan(y1) || std::isnan(y2)) return NAN;
    return y1 - y2;
}

// ── Bisection root finder ──────────────────────────────────────────────────

float GraphModel::bisect(CartesianFunction& fn1, CartesianFunction& fn2,
                         float a, float b, int maxIter) {
    float fa = diffAt(fn1, fn2, a);
    float fb = diffAt(fn1, fn2, b);

    // Check if signs differ and are non-nan
    if (std::isnan(fa) || std::isnan(fb)) return NAN;
    if ((fa > 0.0f && fb > 0.0f) || (fa < 0.0f && fb < 0.0f)) return NAN;

    float tolerance = 1e-5f;

    for (int i = 0; i < maxIter; ++i) {
        float c = (a + b) / 2.0f;
        float fc = diffAt(fn1, fn2, c);

        if (std::isnan(fc)) return NAN;
        if (std::abs(fc) < tolerance) return c;

        if ((fa > 0.0f && fc < 0.0f) || (fa < 0.0f && fc > 0.0f)) {
            b = c;
            fb = fc;
        } else {
            a = c;
            fa = fc;
        }

        if (std::abs(b - a) < tolerance) return c;
    }

    return (a + b) / 2.0f;
}

// ── Find intersections ─────────────────────────────────────────────────────

std::vector<PointOfInterest> GraphModel::findIntersections(
    CartesianFunction& fn1,
    CartesianFunction& fn2,
    float xMin, float xMax,
    int maxPoints,
    int maxIterations)
{
    std::vector<PointOfInterest> results;

    if (!fn1.valid || !fn2.valid) return results;
    if (xMin >= xMax) return results;

    // Adaptive sampling: look for sign changes in f1(x) - f2(x)
    int samples = 100;  // Initial grid for sign change detection
    float step = (xMax - xMin) / samples;

    float prevX = xMin;
    float prevDiff = diffAt(fn1, fn2, prevX);

    for (int i = 1; i <= samples && (int)results.size() < maxPoints; ++i) {
        float currX = xMin + i * step;
        float currDiff = diffAt(fn1, fn2, currX);

        // Skip NaN values
        if (std::isnan(prevDiff) || std::isnan(currDiff)) {
            prevX = currX;
            prevDiff = currDiff;
            continue;
        }

        // Check for sign change (intersection point exists here)
        if ((prevDiff > 0.0f && currDiff < 0.0f) || (prevDiff < 0.0f && currDiff > 0.0f)) {
            // Refine with bisection
            float xIntersect = bisect(fn1, fn2, prevX, currX, maxIterations);

            if (!std::isnan(xIntersect)) {
                float yIntersect = evalAt(fn1, xIntersect);

                // Avoid duplicates: check if this root is far from previous ones
                bool isDuplicate = false;
                for (const auto& poi : results) {
                    if (std::abs(poi.x - xIntersect) < 0.01f) {
                        isDuplicate = true;
                        break;
                    }
                }

                if (!isDuplicate) {
                    PointOfInterest poi;
                    poi.x = xIntersect;
                    poi.y = yIntersect;
                    snprintf(poi.label, sizeof(poi.label), "Intersection");
                    results.push_back(poi);
                }
            }
        }

        prevX = currX;
        prevDiff = currDiff;
    }

    return results;
}

} // namespace grapher

