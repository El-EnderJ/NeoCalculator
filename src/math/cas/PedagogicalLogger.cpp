/**
 * PedagogicalLogger.cpp — Dynamic phrase generation engine.
 *
 * Each SolveAction is mapped to a contextual English phrase that
 * explains the mathematical reasoning.  The phrases reference the
 * actual coefficient values from the ActionContext, producing output
 * like a human tutor would:
 *
 *   "The discriminant D = b^2 - 4ac = (5)^2 - 4*(2)*(-3) = 49"
 *
 * rather than generic:
 *
 *   "Step 3: Compute discriminant"
 *
 * The generated phrase is stored in a CASStep with the appropriate
 * StepKind (Annotation for explanations, Transform for manipulations,
 * Result for final answers, ComplexResult for complex roots).
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 2 (Super Detail Steps)
 */

#include "PedagogicalLogger.h"
#include <cstdio>

namespace cas {

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

    case SolveAction::LINEAR_IDENTIFY_COEFFICIENTS: {
        const CASNumber* a = findVal(ctx, "a");
        const CASNumber* b = findVal(ctx, "b");
        std::string msg = "In the form a" + v + " + b = 0, the coefficients are: ";
        if (a) msg += "a = " + a->toString();
        if (b) msg += ", b = " + b->toString();
        return msg;
    }

    case SolveAction::LINEAR_ISOLATE_VARIABLE:
        return "Moving the constant term to the right side to isolate " + v + ".";

    case SolveAction::LINEAR_DIVIDE_BY_COEFFICIENT: {
        const CASNumber* a = findVal(ctx, "a");
        std::string msg = "Dividing both sides by ";
        if (a) msg += a->toString();
        msg += " to solve for " + v + ".";
        return msg;
    }

    case SolveAction::LINEAR_PRESENT_SOLUTION: {
        const CASNumber* sol = findVal(ctx, "solution");
        return "Solution: " + v + " = " + (sol ? sol->toString() : "?");
    }

    // ── Quadratic ──────────────────────────────────────────────────

    case SolveAction::QUAD_IDENTIFY_COEFFICIENTS: {
        const CASNumber* a = findVal(ctx, "a");
        const CASNumber* b = findVal(ctx, "b");
        const CASNumber* c = findVal(ctx, "c");

        std::string msg = "Reading the standard form a" + v + "^2 + b"
                        + v + " + c = 0, we identify the coefficients:\n";
        if (a) msg += "  a = " + a->toString();
        if (b) msg += ",  b = " + b->toString();
        if (c) msg += ",  c = " + c->toString();
        return msg;
    }

    case SolveAction::QUAD_COMPUTE_DISCRIMINANT: {
        const CASNumber* b  = findVal(ctx, "b");
        const CASNumber* a  = findVal(ctx, "a");
        const CASNumber* c  = findVal(ctx, "c");
        const CASNumber* D  = findVal(ctx, "D");

        std::string msg = "Computing the discriminant D = b^2 - 4ac";
        if (b && a && c) {
            msg += " = (" + b->toString() + ")^2 - 4*("
                 + a->toString() + ")*(" + c->toString() + ")";
        }
        if (D) msg += " = " + D->toString();
        return msg;
    }

    case SolveAction::QUAD_DISCRIMINANT_POSITIVE: {
        const CASNumber* D = findVal(ctx, "D");
        std::string msg = "Since D = ";
        if (D) msg += D->toString();
        msg += " > 0, the equation has two distinct real roots.";
        return msg;
    }

    case SolveAction::QUAD_DISCRIMINANT_ZERO:
        return "Since D = 0, the equation has exactly one repeated real root.";

    case SolveAction::QUAD_DISCRIMINANT_NEGATIVE: {
        const CASNumber* D = findVal(ctx, "D");
        std::string msg = "Since D = ";
        if (D) msg += D->toString();
        msg += " < 0, there are no real solutions. "
               "The roots are complex conjugates.";
        return msg;
    }

    case SolveAction::QUAD_APPLY_FORMULA:
        return "Applying the quadratic formula: " + v
             + " = (-b +/- sqrt(D)) / (2a)";

    case SolveAction::QUAD_COMPUTE_SQRT_DISC: {
        const CASNumber* D    = findVal(ctx, "D");
        const CASNumber* sqD  = findVal(ctx, "sqrtD");
        std::string msg = "Computing sqrt(D)";
        if (D) msg += " = sqrt(" + D->toString() + ")";
        if (sqD) msg += " = " + sqD->toString();
        return msg;
    }

    case SolveAction::QUAD_COMPUTE_ROOTS: {
        const CASNumber* negB = findVal(ctx, "negB");
        const CASNumber* sqD  = findVal(ctx, "sqrtD");
        const CASNumber* twoA = findVal(ctx, "twoA");
        std::string msg = "Substituting into the formula: " + v + " = (";
        if (negB) msg += negB->toString();
        msg += " +/- ";
        if (sqD) msg += sqD->toString();
        msg += ") / ";
        if (twoA) msg += twoA->toString();
        return msg;
    }

    case SolveAction::QUAD_PRESENT_REAL_SOLUTION: {
        const CASNumber* sol = findVal(ctx, "solution");
        char idxBuf[4];
        snprintf(idxBuf, sizeof(idxBuf), "%d", ctx.solutionIndex);

        std::string label;
        if (ctx.solutionIndex == 0) {
            label = "Solution (repeated root)";
        } else {
            label = std::string("Solution ") + v + idxBuf;
        }
        return label + " = " + (sol ? sol->toString() : "?");
    }

    case SolveAction::QUAD_COMPUTE_COMPLEX_PARTS: {
        const CASNumber* re = findVal(ctx, "re");
        const CASNumber* im = findVal(ctx, "im");
        std::string msg = "Computing complex parts:\n";
        msg += "  Real part: -b/(2a) = ";
        if (re) msg += re->toString();
        msg += "\n  Imaginary magnitude: sqrt(|D|)/(2a) = ";
        if (im) msg += im->toString();
        return msg;
    }

    case SolveAction::QUAD_PRESENT_COMPLEX_ROOTS: {
        const CASNumber* re = findVal(ctx, "re");
        const CASNumber* im = findVal(ctx, "im");
        std::string reStr = re ? re->toString() : "?";
        std::string imStr = im ? im->toString() : "?";
        return v + "1 = " + reStr + " + " + imStr + "i,  "
             + v + "2 = " + reStr + " - " + imStr + "i";
    }

    // ── Newton-Raphson ─────────────────────────────────────────────

    case SolveAction::NEWTON_START:
        return "Equation cannot be solved algebraically. "
               "Applying Newton-Raphson numeric method.";

    case SolveAction::NEWTON_CONVERGED: {
        const CASNumber* sol = findVal(ctx, "solution");
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Numeric root found in %d iterations: %s = %s (approx.)",
                 ctx.iterCount, v.c_str(),
                 sol ? sol->toString().c_str() : "?");
        return buf;
    }

    case SolveAction::NEWTON_NO_CONVERGENCE:
        return "Newton-Raphson did not converge to a real root.";

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
    case SolveAction::QUAD_PRESENT_REAL_SOLUTION:
        if (ctx.snapshot) {
            logResult(phrase, *ctx.snapshot, method);
        } else {
            logNote(phrase, method);
        }
        break;

    // ── Complex result steps ────────────────────────────────────────
    case SolveAction::QUAD_PRESENT_COMPLEX_ROOTS:
        logComplex(phrase, method);
        break;

    // ── All other actions are annotations (text-only) ───────────────
    default:
        logNote(phrase, method);
        break;
    }
}

} // namespace cas
