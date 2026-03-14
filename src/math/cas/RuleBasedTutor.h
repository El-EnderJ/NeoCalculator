/**
 * RuleBasedTutor.h — Generalized rule-based CAS step engine.
 *
 * Implements solveEquationStepByStep() which solves ANY polynomial
 * equation by sequentially applying five algebraic rules:
 *
 *   Rule 1 — Expand:          Expand both sides (normalize polynomials).
 *   Rule 2 — Combine:         Combine like terms on each side.
 *   Rule 3 — Isolate:         Move variable terms to LHS, constants to RHS.
 *   Rule 4 — Combine Again:   Re-simplify both sides after isolation.
 *   Rule 5 — Solve:           Divide both sides by the variable coefficient.
 *   Fallback — Quadratic:     If degree 2 after simplification, invoke
 *                              solveQuadraticTutor().
 *
 * This ensures dynamic, correct steps for equations such as:
 *   3x - 2 = x + 6  →  2x = 8  →  x = 4
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 12 (True Rule-Based CAS Engine)
 */

#pragma once

#include "SymEquation.h"
#include "PedagogicalLogger.h"
#include "SingleSolver.h"

namespace cas {

class SymExprArena;

/// Generalized rule-based solver: applies five sequential algebraic rules
/// to solve any first- or second-degree polynomial equation with logging.
///
/// For degree-1 equations: expands, combines, isolates, and divides.
/// For degree-2 equations: falls back to solveQuadraticTutor after reduction.
///
/// @param eq     The equation to solve.
/// @param var    The variable to isolate (e.g. 'x').
/// @param log    Pedagogical logger for step-by-step output.
/// @param arena  Memory arena for CAS expressions (may be nullptr).
/// @return       SolveResult with solution(s) and success flag.
SolveResult solveEquationStepByStep(
    const SymEquation& eq, char var,
    PedagogicalLogger& log, SymExprArena* arena);

} // namespace cas
