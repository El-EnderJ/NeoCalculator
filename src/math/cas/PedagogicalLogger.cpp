/**
 * PedagogicalLogger.cpp — Dynamic phrase generation engine.
 *
 * Each SolveAction is mapped to a contextual English phrase that
 * explains the mathematical reasoning, plus an optional SymExpr tree
 * for 2D MathCanvas rendering via VPAM.
 *
 * Text descriptions contain ONLY explanatory text — no math notation.
 * Each step with a math expression emits a SINGLE CASStep containing
 * BOTH the text description AND a SymExpr* tree for 2D MathCanvas
 * rendering (fractions, superscripts, radicals, etc.).
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 2 (Super Detail Steps)
 */

#include "PedagogicalLogger.h"
#include "SymExpr.h"
#include "SymExprArena.h"
#include <cstdio>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// symFromCAS — Build a SymExpr node from a CASNumber using the arena
// ════════════════════════════════════════════════════════════════════

static SymExpr* symFromCAS(SymExprArena& arena, const CASNumber& n) {
    return symNum(arena, n.toExactVal());
}

// ════════════════════════════════════════════════════════════════════
// findVal — Look up a value by label in the context
// ════════════════════════════════════════════════════════════════════

const CASNumber* PedagogicalLogger::findVal(const ActionContext& ctx,
                                             const char* label) const {
    for (int i = 0; i < ctx.numValues; ++i) {
        if (ctx.labels[i] && std::string(ctx.labels[i]) == label) {
            return &ctx.values[i];
        }
    }
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// buildPhrase — Generate contextual English phrase for each action
// ════════════════════════════════════════════════════════════════════

std::string PedagogicalLogger::buildPhrase(SolveAction action,
                                            const ActionContext& ctx) const {
    std::string v(1, ctx.variable);   // Variable name as string

    switch (action) {

    // ── General ────────────────────────────────────────────────────

    case SolveAction::PRESENT_ORIGINAL_EQUATION:
        return "We start with the equation:";

    case SolveAction::NORMALIZE_TO_STANDARD_FORM:
        return "Rearranging into standard form: move all terms to the left "
               "side and set equal to zero.";

    case SolveAction::IDENTIFY_EQUATION_TYPE: {
        if (ctx.degree == 1)
            return "This is a linear equation (degree 1) in " + v + ".";
        if (ctx.degree == 2)
            return "This is a quadratic equation (degree 2) in " + v + ".";
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "This is a degree-%d equation in %s. Using numeric approximation.",
                 ctx.degree, v.c_str());
        return buf;
    }

    // ── Linear ─────────────────────────────────────────────────────

    case SolveAction::LINEAR_IDENTIFY_COEFFICIENTS:
        return "Identifying coefficients:";

    case SolveAction::LINEAR_ISOLATE_VARIABLE:
        return "Moving the constant term to the right side to isolate " + v + ".";

    case SolveAction::LINEAR_DIVIDE_BY_COEFFICIENT:
        return "Dividing both sides by the coefficient:";

    case SolveAction::LINEAR_PRESENT_SOLUTION:
        return "Solution:";

    // ── Quadratic ──────────────────────────────────────────────────

    case SolveAction::QUAD_IDENTIFY_COEFFICIENTS:
        return "Identifying coefficients:";

    case SolveAction::QUAD_COMPUTE_DISCRIMINANT:
        return "Computing the discriminant:";

    case SolveAction::QUAD_DISCRIMINANT_POSITIVE:
        return "Discriminant is positive: two distinct real roots.";

    case SolveAction::QUAD_DISCRIMINANT_ZERO:
        return "Discriminant is zero: one repeated real root.";

    case SolveAction::QUAD_DISCRIMINANT_NEGATIVE:
        return "Discriminant is negative: complex conjugate roots.";

    case SolveAction::QUAD_APPLY_FORMULA: {
        const CASNumber* D = findVal(ctx, "D");
        if (D && D->isZero())
            return "Discriminant is zero, one repeated root:";
        return "Applying the quadratic formula:";
    }

    case SolveAction::QUAD_COMPUTE_SQRT_DISC:
        return "Computing square root of discriminant:";

    case SolveAction::QUAD_COMPUTE_ROOTS:
        return "Substituting into the formula:";

    case SolveAction::QUAD_PRESENT_REAL_SOLUTION: {
        const CASNumber* sol = findVal(ctx, "solution");
        char idxBuf[4];
        snprintf(idxBuf, sizeof(idxBuf), "%d", ctx.solutionIndex);

        std::string label;
        if (ctx.solutionIndex == 0) {
            label = "Solution (repeated root):";
        } else {
            label = std::string("Solution ") + v + idxBuf + ":";
        }
        // Value rendered via MathCanvas (logExpr), not in text
        return label;
    }

    case SolveAction::QUAD_COMPUTE_COMPLEX_PARTS:
        return "Computing complex parts:";

    case SolveAction::QUAD_PRESENT_COMPLEX_ROOTS:
        return "Complex conjugate roots:";

    // ── Newton-Raphson ─────────────────────────────────────────────

    case SolveAction::NEWTON_START:
        return "Equation cannot be solved algebraically. "
               "Applying Newton-Raphson numeric method.";

    case SolveAction::NEWTON_CONVERGED: {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "Numeric root found in %d iterations (approx.).",
                 ctx.iterCount);
        return buf;
    }

    case SolveAction::NEWTON_NO_CONVERGENCE:
        return "Newton-Raphson did not converge to a real root.";

    // ── Tutor: Expanded Quadratic ──────────────────────────────────

    case SolveAction::QUAD_SHOW_GENERAL_FORMULA:
        return "The quadratic formula states:";

    case SolveAction::QUAD_SUBSTITUTE_VALUES:
        return "Substituting the coefficient values:";

    case SolveAction::QUAD_SIMPLIFY_UNDER_RADICAL:
        return "Simplifying under the radical:";

    case SolveAction::QUAD_COMPUTE_SQRT_VALUE:
        return "Computing the square root:";

    case SolveAction::QUAD_SEPARATE_ROOTS: {
        std::string label = "Separating into " + v;
        // Use subscript numbers in text as Unicode
        label += "\xe2\x82\x81 and " + v + "\xe2\x82\x82:";  // ₁ and ₂
        return label;
    }

    case SolveAction::QUAD_SIMPLIFY_ROOT: {
        char idxBuf[4];
        snprintf(idxBuf, sizeof(idxBuf), "%d", ctx.solutionIndex);
        return std::string("Simplifying ") + v + idxBuf + ":";
    }

    // ── Tutor: Cubic (Ruffini) ──────────────────────────────────────

    case SolveAction::CUBIC_TRY_ROOT: {
        const CASNumber* r = findVal(ctx, "root");
        if (r) {
            return "Testing " + v + " = " + r->toString() + " as a potential root...";
        }
        return "Testing a rational root candidate...";
    }

    case SolveAction::CUBIC_ROOT_FOUND: {
        const CASNumber* r = findVal(ctx, "root");
        if (r) {
            return "P(" + r->toString() + ") = 0, so " + v +
                   "\xe2\x82\x81 = " + r->toString();
        }
        return "Root found by rational root test.";
    }

    case SolveAction::CUBIC_SYNTHETIC_DIVISION:
        return "Performing synthetic (Ruffini) division:";

    case SolveAction::CUBIC_RESULTING_QUADRATIC:
        return "The resulting quadratic factor is:";

    // ── Tutor: System 2x2 (Cramer) ──────────────────────────────────
    case SolveAction::SYSTEM_SHOW_MATRIX:
        return "Writing the system in matrix form:";

    case SolveAction::SYSTEM_CRAMER_DETERMINANT:
        return "Calculating the main determinant D:";

    case SolveAction::SYSTEM_CRAMER_DX_DY:
        return "Calculating determinants Dx and Dy:";

    case SolveAction::SYSTEM_CRAMER_SOLUTION:
        return "Finding values for x and y:";

    } // switch

    return "Unknown action";
}

// ════════════════════════════════════════════════════════════════════
// logAction — Build phrase and create the appropriate CASStep
// ════════════════════════════════════════════════════════════════════

void PedagogicalLogger::logAction(SolveAction action,
                                   const ActionContext& ctx,
                                   MethodId method) {
    std::string phrase = buildPhrase(action, ctx);
    SymExprArena* ar = ctx.arena;

    // Determine StepKind based on action type
    switch (action) {

    // ── Transform steps (algebraic manipulation with equation snapshot) ──
    case SolveAction::NORMALIZE_TO_STANDARD_FORM:
    case SolveAction::LINEAR_ISOLATE_VARIABLE:
    case SolveAction::LINEAR_DIVIDE_BY_COEFFICIENT:
        if (ctx.snapshot) {
            log(phrase, *ctx.snapshot, method);
        } else {
            logNote(phrase, method);
        }
        break;

    // ── Result steps (final answer with equation snapshot) ──────────
    case SolveAction::LINEAR_PRESENT_SOLUTION:
        if (ctx.snapshot) {
            logResult(phrase, *ctx.snapshot, method);
        } else {
            logNote(phrase, method);
        }
        break;

    // ── Quadratic solution: render value via MathCanvas ─────────────
    case SolveAction::QUAD_PRESENT_REAL_SOLUTION:
        if (ar) {
            const CASNumber* sol = findVal(ctx, "solution");
            if (sol) {
                SymExpr* solExpr = symFromCAS(*ar, *sol);
                logResultExpr(phrase, solExpr, method);
            } else {
                logNote(phrase, method);
            }
        } else if (ctx.snapshot) {
            logResult(phrase, *ctx.snapshot, method);
        } else {
            logNote(phrase, method);
        }
        break;

    // ── Complex result steps ────────────────────────────────────────
    case SolveAction::QUAD_PRESENT_COMPLEX_ROOTS:
        logNote(phrase, method);
        // Complex root math is rendered as two MathCanvas widgets
        // directly in buildStepsDisplay() using OmniResult data.
        break;

    // ── Quadratic formula: emit text + fraction with actual values ─
    case SolveAction::QUAD_APPLY_FORMULA:
        if (ar) {
            const CASNumber* negBVal = findVal(ctx, "negB");
            const CASNumber* sqDVal  = findVal(ctx, "sqrtD");
            const CASNumber* twoAVal = findVal(ctx, "twoA");
            if (negBVal && twoAVal) {
                SymExpr* negBSym = symFromCAS(*ar, *negBVal);
                SymExpr* twoASym = symFromCAS(*ar, *twoAVal);
                if (sqDVal && !sqDVal->isZero()) {
                    // D > 0: show (-b + sqrt(D)) / 2a with actual values
                    SymExpr* sqDSym = symFromCAS(*ar, *sqDVal);
                    // Bypass canonical sort: negB first, then sqrtD
                    auto** arr = static_cast<SymExpr**>(
                        ar->allocRaw(2 * sizeof(SymExpr*)));
                    arr[0] = negBSym;
                    arr[1] = sqDSym;
                    SymExpr* num = ar->create<SymAdd>(
                        const_cast<SymExpr* const*>(arr), uint16_t(2));
                    SymExpr* frac = symMul(*ar, num,
                        symPow(*ar, twoASym, symInt(*ar, -1)));
                    logExpr(phrase, frac, method);
                } else {
                    // D = 0: show -b / (2a) with actual values
                    SymExpr* frac = symMul(*ar, negBSym,
                        symPow(*ar, twoASym, symInt(*ar, -1)));
                    logExpr(phrase, frac, method);
                }
            } else {
                logNote(phrase, method);
            }
        } else {
            logNote(phrase, method);
        }
        break;

    // ── Discriminant computation: emit text + D = b^2 - 4ac ──────
    case SolveAction::QUAD_COMPUTE_DISCRIMINANT: {
        if (ar) {
            const CASNumber* b  = findVal(ctx, "b");
            const CASNumber* a  = findVal(ctx, "a");
            const CASNumber* c  = findVal(ctx, "c");
            const CASNumber* D  = findVal(ctx, "D");
            if (b && a && c && D) {
                SymExpr* bSym = symFromCAS(*ar, *b);

                // b^2
                SymExpr* b2 = symPow(*ar, bSym, symInt(*ar, 2));
                // Compute 4ac as a single numeric value (avoids "342" display)
                CASNumber ac4_val = CASNumber::mul(CASNumber::fromInt(4),
                                      CASNumber::mul(*a, *c));
                SymExpr* fourACSym = symFromCAS(*ar, ac4_val);
                // b^2 - 4ac — bypass canonical sort to preserve visual order
                auto** arr = static_cast<SymExpr**>(ar->allocRaw(2 * sizeof(SymExpr*)));
                arr[0] = b2;                       // b² first
                arr[1] = symNeg(*ar, fourACSym);   // -4ac (as computed value) second
                SymExpr* discExpr = ar->create<SymAdd>(const_cast<SymExpr* const*>(arr), uint16_t(2));

                // Single step: text + MathCanvas expression
                logExpr(phrase, discExpr, method);
            } else {
                logNote(phrase, method);
            }
        } else {
            logNote(phrase, method);
        }
        break;
    }

    // ── sqrt(D) computation: emit text + sqrt(D) expression ──────
    case SolveAction::QUAD_COMPUTE_SQRT_DISC: {
        if (ar) {
            const CASNumber* D   = findVal(ctx, "D");
            const CASNumber* sqD = findVal(ctx, "sqrtD");
            if (D && sqD) {
                SymExpr* dSym  = symFromCAS(*ar, *D);
                // Show: sqrt(D_value) as D^(1/2)
                SymExpr* sqrtExpr = symPow(*ar, dSym, symFrac(*ar, 1, 2));
                // Single step: text + MathCanvas expression
                logExpr(phrase, sqrtExpr, method);
            } else {
                logNote(phrase, method);
            }
        } else {
            logNote(phrase, method);
        }
        break;
    }

    // ── Root substitution: emit text + x = (negB ± sqrtD) / 2a ──
    case SolveAction::QUAD_COMPUTE_ROOTS: {
        if (ar) {
            const CASNumber* negB = findVal(ctx, "negB");
            const CASNumber* sqD  = findVal(ctx, "sqrtD");
            const CASNumber* twoA = findVal(ctx, "twoA");
            if (negB && sqD && twoA) {
                SymExpr* negBSym = symFromCAS(*ar, *negB);
                SymExpr* sqDSym  = symFromCAS(*ar, *sqD);
                SymExpr* twoASym = symFromCAS(*ar, *twoA);

                // Build: (negB + sqrtD) / twoA   (for x1)
                SymExpr* num = symAdd(*ar, negBSym, sqDSym);
                SymExpr* frac = symMul(*ar, num,
                    symPow(*ar, twoASym, symInt(*ar, -1)));
                // Single step: text + MathCanvas expression
                logExpr(phrase, frac, method);
            } else {
                logNote(phrase, method);
            }
        } else {
            logNote(phrase, method);
        }
        break;
    }

    // ── Complex parts: emit text + re, im values via MathCanvas ──
    case SolveAction::QUAD_COMPUTE_COMPLEX_PARTS: {
        if (ar) {
            const CASNumber* re = findVal(ctx, "re");
            const CASNumber* im = findVal(ctx, "im");
            if (re) {
                SymExpr* reSym = symFromCAS(*ar, *re);
                logExpr(phrase, reSym, method);
            }
            if (im) {
                SymExpr* imSym = symFromCAS(*ar, *im);
                logExpr("Imaginary magnitude:", imSym, method);
            }
            if (!re && !im) logNote(phrase, method);
        } else {
            logNote(phrase, method);
        }
        break;
    }

    // ── Identify coefficients: emit text + the standard form ──────
    case SolveAction::QUAD_IDENTIFY_COEFFICIENTS: {
        if (ar) {
            const CASNumber* a = findVal(ctx, "a");
            const CASNumber* b = findVal(ctx, "b");
            const CASNumber* c = findVal(ctx, "c");
            if (a && b && c) {
                // Build: a*x^2 + b*x + c = 0 with actual values
                SymExpr* x = symVar(*ar, ctx.variable);
                SymExpr* aSym = symFromCAS(*ar, *a);
                SymExpr* bSym = symFromCAS(*ar, *b);
                SymExpr* cSym = symFromCAS(*ar, *c);

                // a*x^2
                SymExpr* ax2 = symMul(*ar, aSym, symPow(*ar, x, symInt(*ar, 2)));
                // b*x
                SymExpr* bx = symMul(*ar, bSym, x);
                // a*x^2 + b*x + c
                SymExpr* poly = symAdd3(*ar, ax2, bx, cSym);
                // Single step: text + MathCanvas expression
                logExpr(phrase, poly, method);
            } else {
                logNote(phrase, method);
            }
        } else {
            logNote(phrase, method);
        }
        break;
    }

    // ── Tutor: all new tutor actions are annotations ────────────────
    case SolveAction::QUAD_SHOW_GENERAL_FORMULA:
    case SolveAction::QUAD_SUBSTITUTE_VALUES:
    case SolveAction::QUAD_SIMPLIFY_UNDER_RADICAL:
    case SolveAction::QUAD_COMPUTE_SQRT_VALUE:
    case SolveAction::QUAD_SEPARATE_ROOTS:
    case SolveAction::QUAD_SIMPLIFY_ROOT:
    case SolveAction::CUBIC_TRY_ROOT:
    case SolveAction::CUBIC_ROOT_FOUND:
    case SolveAction::CUBIC_SYNTHETIC_DIVISION:
    case SolveAction::CUBIC_RESULTING_QUADRATIC:
    case SolveAction::SYSTEM_SHOW_MATRIX:
    case SolveAction::SYSTEM_CRAMER_DETERMINANT:
    case SolveAction::SYSTEM_CRAMER_DX_DY:
    case SolveAction::SYSTEM_CRAMER_SOLUTION:
        // These use SymExpr math chunks built by the TutorTemplate
        if (ctx.customExpr) {
            logExpr(phrase, ctx.customExpr, method);
        } else {
            logNote(phrase, method);
        }
        break;

    // ── All other actions are annotations (text-only) ───────────────
    default:
        logNote(phrase, method);
        break;
    }
}

} // namespace cas
