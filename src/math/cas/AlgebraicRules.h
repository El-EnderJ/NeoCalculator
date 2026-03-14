/**
 * AlgebraicRules.h — Pedagogical rule dictionary for the CAS algebraic brain.
 *
 * Provides a factory function that builds the four-phase rule library used by
 * the RuleEngine to solve linear and quadratic equations step-by-step in a
 * manner modelled on Photomath / Symbolab quality pedagogy.
 *
 * ── Rule phases ───────────────────────────────────────────────────────────────
 *
 *   PHASE 1 — Expansion & Cleanup
 *     1.  DoubleNegation          -(-x) → x
 *     2.  DistributiveProperty    a(b + c) → ab + ac
 *     3.  PowerExpansion          (ab)^c → a^c · b^c
 *
 *   PHASE 2 — Simplification (sides treated independently)
 *     4.  CombineConstants        5 + 3 → 8  (Sum and Product)
 *     5.  ConstantPowerEval       2^3 → 8    (both operands constant)
 *     6.  CombineLikeTerms        2x + 4x → 6x  (coeff × varⁿ grouping)
 *     7.  IdentityElimination     x + 0 → x,  x * 1 → x,  x * 0 → 0
 *
 *   PHASE 3 — Transposition (the "human move")
 *     8.  MoveConstantsToRHS      x + c = RHS  →  x + c + (-c) = RHS + (-c)
 *                                 (generates the intermediate working step)
 *     9.  MoveVariablesToLHS      LHS = A·var + rest  →  LHS + (-A·var) = …
 *
 *   PHASE 4 — Reduction
 *    10.  DivideByCoefficient     A·x = B  →  x = B/A
 *
 * ── Memory ───────────────────────────────────────────────────────────────────
 *   All nodes produced by the rules are allocated from the CasMemoryPool
 *   passed to makeAlgebraicRules().  The rules themselves capture closures
 *   over `var` but hold no raw pointers; everything is NodePtr (shared_ptr).
 *
 * ── Usage ────────────────────────────────────────────────────────────────────
 *   CasMemoryPool pool;
 *   RuleEngine engine(pool);
 *   for (auto& r : makeAlgebraicRules(pool, 'x'))
 *       engine.addRule(std::move(r));
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13B (Algebraic Brain & Smart Diffing)
 */

#pragma once

#include "RuleEngine.h"

namespace cas {

/**
 * Build the complete algebraic rule set for solving linear / quadratic
 * equations.  Rules are ordered by phase (Phase 1 first, Phase 4 last) so
 * that the RuleEngine's first-matching-rule-wins strategy naturally applies
 * expansion and simplification before transposition and reduction.
 *
 * @param pool  Memory pool used to allocate any pattern/replacement nodes that
 *              are needed for structural pattern rules.  (Most rules here use
 *              custom transforms and don't require pre-built patterns.)
 * @param var   The variable to solve for (default 'x').  Affects Phase 3 and
 *              Phase 4 rules (MoveVariablesToLHS, DivideByCoefficient).
 * @returns     Vector of 10 RewriteRule objects ready to be registered with a
 *              RuleEngine via addRule().
 */
std::vector<RewriteRule> makeAlgebraicRules(CasMemoryPool& pool, char var = 'x');

/**
 * Inspect the final SolveResult and, if the equation is unsolved and its LHS
 * is a degree-2 polynomial in `var`, append a special "handover" StepLog
 * entry whose `ruleDesc` directs the UI to invoke the quadratic formula.
 *
 * Behaviour
 * ─────────
 *   · If `result.finalTree` is already solved (`x = c`), this is a no-op.
 *   · If the LHS is degree 2, a StepLog entry with:
 *       ruleName = "NonLinearHandover"
 *       ruleDesc = "Equation is quadratic. Transitioning to Quadratic Formula."
 *       phase    = RulePhase::Reduction
 *     is appended and `result.reachedFixedPoint` is left unchanged.
 *
 * @param result  The SolveResult from RuleEngine::applyToFixedPoint().
 * @param var     The variable being solved for.
 */
void checkNonLinearHandover(RuleEngine::SolveResult& result, char var);

/// Rule name used in the NonLinearHandover StepLog entry.
constexpr const char* RULE_NONLINEAR_HANDOVER = "NonLinearHandover";

} // namespace cas
