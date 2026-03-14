/**
 * SystemHeuristics.h — Method chooser for 2×2 CAS systems.
 *
 * Analyses two Cas::NodePtr equations and returns the best solving strategy:
 *
 *   ELIMINATION  — opposite variable coefficients found (e.g. +3y and -3y)
 *   SUBSTITUTION — one equation already isolates a variable (y = expr)
 *   EQUATING     — both equations express the same variable (y = … and y = …)
 *
 * Default fallback is SUBSTITUTION.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 15 (System Solver)
 */

#pragma once

#include "PersistentAST.h"

namespace cas {

/// The method the SystemTutor will use to solve the 2x2 system.
enum class SystemSolveMethod : uint8_t {
    SUBSTITUTION = 0,   ///< Isolate a variable in one eq, substitute into other
    ELIMINATION  = 1,   ///< Add/subtract equations to cancel a variable
    EQUATING     = 2,   ///< Both equations already express same variable
};

/**
 * Analyse two parsed EquationNode trees and pick the best solving strategy.
 *
 * @param eq1   First equation (EquationNode).
 * @param eq2   Second equation (EquationNode).
 * @param var1  Primary variable (e.g. 'x').
 * @param var2  Secondary variable (e.g. 'y').
 * @returns     The recommended SystemSolveMethod.
 */
SystemSolveMethod chooseSystemMethod(const NodePtr& eq1,
                                     const NodePtr& eq2,
                                     char var1 = 'x',
                                     char var2 = 'y');

} // namespace cas
