/**
 * SystemTutor.h — Step-by-step 2×2 CAS system orchestrator.
 *
 * Coordinates multiple RuleEngine sessions to solve a 2×2 system of equations
 * (linear or quadratic) and produces a rich step-by-step log that the UI can
 * display as pedagogical MathCanvas widgets.
 *
 * Supported Methods (chosen automatically by SystemHeuristics):
 *   · SUBSTITUTION — isolate a variable in eq1, substitute into eq2, back-sub
 *   · ELIMINATION  — add both equations to cancel one variable, then back-sub
 *   · EQUATING     — equate the isolated expressions and solve
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 15 (System Solver)
 */

#pragma once

#include <string>
#include <vector>
#include "PersistentAST.h"
#include "CasMemory.h"
#include "RuleEngine.h"
#include "SystemHeuristics.h"

namespace cas {

// ─────────────────────────────────────────────────────────────────────────────
// SystemTutorStep — A single step in the system-solve log
// ─────────────────────────────────────────────────────────────────────────────

struct SystemTutorStep {
    std::string ruleName;    ///< Short name (e.g. "Substitution", "Eliminate-y")
    std::string ruleDesc;    ///< Human-readable description shown in the UI
    NodePtr     eq1Tree;     ///< State of equation 1 at this step (may be nullptr)
    NodePtr     eq2Tree;     ///< State of equation 2 at this step (may be nullptr)
};

// ─────────────────────────────────────────────────────────────────────────────
// SystemTutorResult — Output of SystemTutor::solveSystem()
// ─────────────────────────────────────────────────────────────────────────────

struct SystemTutorResult {
    std::vector<SystemTutorStep> steps;
    SystemSolveMethod            methodUsed = SystemSolveMethod::SUBSTITUTION;
    bool                         ok         = false;
    std::string                  error;
};

// ─────────────────────────────────────────────────────────────────────────────
// SystemTutor — The orchestrator
// ─────────────────────────────────────────────────────────────────────────────

class SystemTutor {
public:
    /**
     * Solve a 2×2 system step-by-step.
     *
     * @param eq1   First equation (EquationNode NodePtr).
     * @param eq2   Second equation (EquationNode NodePtr).
     * @param pool  Memory pool for all intermediate AST allocations.
     * @param var1  First variable (default 'x').
     * @param var2  Second variable (default 'y').
     * @returns     SystemTutorResult with all steps.
     */
    static SystemTutorResult solveSystem(const NodePtr& eq1,
                                         const NodePtr& eq2,
                                         CasMemoryPool&  pool,
                                         char var1 = 'x',
                                         char var2 = 'y');

private:
    // ── Method implementations ────────────────────────────────────────────

    static SystemTutorResult bySubstitution(const NodePtr& eq1,
                                             const NodePtr& eq2,
                                             CasMemoryPool&  pool,
                                             char var1, char var2);

    static SystemTutorResult byElimination(const NodePtr& eq1,
                                            const NodePtr& eq2,
                                            CasMemoryPool&  pool,
                                            char var1, char var2);

    static SystemTutorResult byEquating(const NodePtr& eq1,
                                         const NodePtr& eq2,
                                         CasMemoryPool&  pool,
                                         char var1, char var2);

    // ── Helpers ───────────────────────────────────────────────────────────

    /// Run the algebraic RuleEngine on `eq` until fixed-point, appending
    /// each step to `out`.  Returns the final tree.
    static NodePtr runEngine(const NodePtr& eq, CasMemoryPool& pool,
                             char var,
                             std::vector<SystemTutorStep>& out);

    /// Replace every VariableNode(target) in `node` with `replacement`.
    static NodePtr substituteVar(const NodePtr& node, char target,
                                  const NodePtr& replacement,
                                  CasMemoryPool& pool);

    /// Append all steps from a RuleEngine::SolveResult, tagging eq2 as nullptr.
    static void appendEngineSteps(const RuleEngine::SolveResult& result,
                                   std::vector<SystemTutorStep>& out,
                                   bool isEq1);

    /// Get the isolated RHS expression from an equation of the form `var = expr`.
    /// Returns nullptr if the equation is not in that form.
    static NodePtr getIsolatedRHS(const NodePtr& eq, char var);
};

} // namespace cas
