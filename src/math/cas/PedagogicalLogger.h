/**
 * PedagogicalLogger.h — Dynamic reasoning engine for step-by-step display.
 *
 * Instead of the solver hardcoding phrases like "Step 1: Identify coefficients",
 * it emits a SolveAction (e.g. ACTION_IDENTIFY_COEFFICIENTS) together with an
 * ActionContext that carries the live values.  The PedagogicalLogger constructs
 * rich, contextual English phrases that explain *why* each step happens:
 *
 *   "Reading the standard form ax² + bx + c = 0, we identify:
 *    a = 2, b = 5, c = -3"
 *
 *   "Computing the discriminant: D = b² − 4ac = (5)² − 4·(2)·(−3) = 49"
 *
 *   "Since D = 49 > 0, the equation has two distinct real roots."
 *
 * Each logged action creates a CASStep (via the parent CASStepLogger) with
 * the generated phrase as description and the appropriate StepKind:
 *   - Transform   → algebraic manipulation (with equation snapshot)
 *   - Annotation   → contextual explanation (text only)
 *   - Result       → final answer (with equation snapshot)
 *   - ComplexResult → complex root text
 *
 * The ActionContext uses CASNumber (Pilar 1) for all numeric values,
 * ensuring BigInt values display correctly.
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 2 (Super Detail Steps)
 */

#pragma once

#include "CASStepLogger.h"
#include "CASNumber.h"
#include <string>

namespace cas { class SymExprArena; }  // forward declaration

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SolveAction — Semantic actions emitted by solvers
// ════════════════════════════════════════════════════════════════════

enum class SolveAction : uint8_t {
    // ── General (all solvers) ──────────────────────────────────────
    PRESENT_ORIGINAL_EQUATION,      ///< Log the starting equation
    NORMALIZE_TO_STANDARD_FORM,     ///< Rearrange to f(x) = 0
    IDENTIFY_EQUATION_TYPE,         ///< "Quadratic", "Linear", etc.

    // ── Linear solver ──────────────────────────────────────────────
    LINEAR_IDENTIFY_COEFFICIENTS,   ///< Identify a and b in ax + b = 0
    LINEAR_ISOLATE_VARIABLE,        ///< Move constant to RHS
    LINEAR_DIVIDE_BY_COEFFICIENT,   ///< Divide both sides by a
    LINEAR_PRESENT_SOLUTION,        ///< x = -b/a

    // ── Quadratic solver ───────────────────────────────────────────
    QUAD_IDENTIFY_COEFFICIENTS,     ///< Identify a, b, c in ax²+bx+c=0
    QUAD_COMPUTE_DISCRIMINANT,      ///< D = b² - 4ac (show computation)
    QUAD_DISCRIMINANT_POSITIVE,     ///< D > 0 → two real roots
    QUAD_DISCRIMINANT_ZERO,         ///< D = 0 → one repeated root
    QUAD_DISCRIMINANT_NEGATIVE,     ///< D < 0 → complex conjugate roots
    QUAD_APPLY_FORMULA,             ///< State the quadratic formula
    QUAD_COMPUTE_SQRT_DISC,         ///< Compute sqrt(D)
    QUAD_COMPUTE_ROOTS,             ///< Calculate the two roots
    QUAD_PRESENT_REAL_SOLUTION,     ///< Final real root value
    QUAD_COMPUTE_COMPLEX_PARTS,     ///< Real and imaginary components
    QUAD_PRESENT_COMPLEX_ROOTS,     ///< x = re ± im·i

    // ── Newton-Raphson ─────────────────────────────────────────────
    NEWTON_START,                   ///< Explain numeric fallback
    NEWTON_CONVERGED,               ///< Found a root
    NEWTON_NO_CONVERGENCE,          ///< Failed to find roots
};

// ════════════════════════════════════════════════════════════════════
// ActionContext — Dynamic parameters for phrase generation
// ════════════════════════════════════════════════════════════════════

struct ActionContext {
    static constexpr int MAX_VALUES = 6;

    char         variable = 'x';             ///< Variable being solved for
    CASNumber    values[MAX_VALUES];         ///< Numeric parameters
    const char*  labels[MAX_VALUES] = {};    ///< Labels ("a", "b", "c", "D", ...)
    int          numValues = 0;              ///< How many values/labels are set

    const SymEquation* snapshot = nullptr;   ///< Optional equation state
    int          degree = 0;                 ///< Equation degree (for type identification)
    int          solutionIndex = 0;          ///< Which solution (1, 2, ...)
    int          iterCount = 0;              ///< Newton iteration count
    SymExprArena* arena = nullptr;           ///< Arena for building SymExpr math chunks

    // ── Builder API for clean solver code ──────────────────────────

    ActionContext& var(char v) { variable = v; return *this; }
    ActionContext& deg(int d) { degree = d; return *this; }
    ActionContext& solIdx(int i) { solutionIndex = i; return *this; }
    ActionContext& iters(int n) { iterCount = n; return *this; }
    ActionContext& snap(const SymEquation* eq) { snapshot = eq; return *this; }
    ActionContext& withArena(SymExprArena* a) { arena = a; return *this; }

    ActionContext& val(const char* label, const CASNumber& v) {
        if (numValues < MAX_VALUES) {
            labels[numValues] = label;
            values[numValues] = v;
            ++numValues;
        }
        return *this;
    }
};

// ════════════════════════════════════════════════════════════════════
// PedagogicalLogger — Extends CASStepLogger with action-based logging
// ════════════════════════════════════════════════════════════════════

class PedagogicalLogger : public CASStepLogger {
public:
    PedagogicalLogger() = default;

    /// Log a semantic action with context.  The logger builds a contextual
    /// English phrase and creates the appropriate CASStep.
    void logAction(SolveAction action, const ActionContext& ctx,
                   MethodId method = MethodId::General);

private:
    /// Build a rich, contextual English phrase for the given action.
    std::string buildPhrase(SolveAction action, const ActionContext& ctx) const;

    /// Helper: find a value by label in the context.  Returns nullptr if not found.
    const CASNumber* findVal(const ActionContext& ctx, const char* label) const;
};

} // namespace cas
