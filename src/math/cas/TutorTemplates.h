/**
 * TutorTemplates.h — Educational step-by-step solver templates.
 *
 * This module extends the CAS solvers by generating rich, educational
 * step-by-step solutions that emulate human reasoning. Instead of skipping
 * directly to the roots, it builds intermediate MathAST trees using the
 * SymExpr factories to show standard formulas, substitutions, and
 * gradual simplifications.
 *
 * Part of: NumOS CAS-S3-ULTRA — Pilar 2 (Super Detail Steps)
 */

#pragma once

#include "SymEquation.h"
#include "PedagogicalLogger.h"
#include "SingleSolver.h"
#include "SystemSolver.h"

namespace cas {
class SymExprArena;
class SymExpr;
struct OmniResult;

/// Pre-process an equation into standard polynomial form while logging
/// pedagogical normalization steps when they are needed.
void preProcessEquationTutor(SymEquation& eq, PedagogicalLogger& log);

/// Quadratic Tutor: Generates step-by-step educational steps for ax² + bx + c = 0
/// Emits steps showing the generic quadratic formula, substitution of coefficients,
/// simplification under the radical, separation of roots, and final values.
SolveResult solveQuadraticTutor(
    const SymEquation& eq, char var,
    PedagogicalLogger& log, SymExprArena* arena);

/// Linear Tutor: generates educational steps for ax + b = 0
SolveResult solveLinearTutor(
    const SymEquation& eq, char var,
    PedagogicalLogger& log, SymExprArena* arena);

/// Cubic Tutor: generates Ruffini steps + residual quadratic
SolveResult solveCubicTutor(
    const SymEquation& eq, char var,
    PedagogicalLogger& log, SymExprArena* arena);

/// Logarithmic Tutor: combines compatible logs, converts to exponential form,
/// and solves the resulting equation in tutor mode.
bool solveLogarithmicTutor(
    SymExpr* lhs, SymExpr* rhs, char var,
    SymExprArena& arena, OmniResult& result);

/// Exponential Tutor: isolates a^(f(x)) and solves by equal bases or logs.
bool solveExponentialTutor(
    SymExpr* lhs, SymExpr* rhs, char var,
    SymExprArena& arena, OmniResult& result);

/// Radical Tutor: isolates sqrt(f(x)), squares both sides, solves recursively,
/// and checks candidates against the original equation.
bool solveRadicalTutor(
    SymExpr* lhs, SymExpr* rhs, char var,
    SymExprArena& arena, OmniResult& result);

/// System 2x2 Tutor: Cramer's rule step-by-step
SystemResult solveSystem2x2Tutor(
    const LinEq& eq1, const LinEq& eq2,
    char var1, char var2, SymExprArena* arena);

} // namespace cas
