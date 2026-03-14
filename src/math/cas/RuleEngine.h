/**
 * RuleEngine.h — Term Rewriting System (TRS) core engine.
 *
 * Phase 13A: Rule-Based CAS Engine Infrastructure.
 *
 * This module provides the three key components of the TRS:
 *
 *   1. RewriteRule — Declarative description of an algebraic transformation.
 *   2. RuleMatcher  — Structural pattern matching with basic AC (Associative-
 *                     Commutative) matching for Sum and Product nodes.
 *   3. RuleEngine   — Orchestrator; holds a rule registry and applies one
 *                     transformation per step (Single-Redex strategy).
 *
 * ── RewriteRule ───────────────────────────────────────────────────────────
 *
 * A RewriteRule is a pair of AST patterns:
 *
 *   pattern     — The LHS: describes what the rule recognises.
 *   replacement — The RHS: describes what it rewrites to.
 *
 * Patterns may contain wildcard VariableNodes whose names begin with '_':
 *   _a, _b, _c, …   — match any single sub-tree and are captured.
 * Variables whose name does NOT begin with '_' (e.g. 'x') are treated as
 * literal variable references and only match that exact symbol.
 *
 * Additional metadata:
 *   name        — Human-readable rule name ("Distributive Property", …).
 *   description — Longer explanation for the step log.
 *   phase       — Pedagogical phase (see RulePhase enum).
 *
 * ── RuleMatcher ──────────────────────────────────────────────────────────
 *
 * matchPattern(pattern, node, captures) attempts to unify `pattern` against
 * `node`, filling the `captures` map with wildcard → sub-tree bindings.
 *
 * AC matching for Sum and Product:
 *   If the pattern is Sum{_a, _b} and the subject is Sum{x, y, z}, the
 *   matcher tries all partitions {_a ↦ x, _b ↦ Sum{y,z}} etc., returning
 *   on the first successful unification.  This handles commutativity and
 *   basic associativity without requiring canonicalisation.
 *
 * applyCaptures(replacement, captures, pool) instantiates the replacement
 * pattern by substituting all wildcard occurrences with the captured
 * sub-trees, producing the rewritten node in `pool`.
 *
 * ── RuleEngine ────────────────────────────────────────────────────────────
 *
 * Single-Redex strategy: on each call to applyOneStep(), the engine scans
 * the tree in a depth-first, left-to-right order and applies the FIRST rule
 * that matches at the FIRST matching sub-tree.  This mirrors how a human
 * algebraist works: identify and fix one thing at a time.
 *
 * Usage:
 *   RuleEngine engine(pool);
 *   engine.addRule(distributiveRule);
 *   engine.addRule(combineConstantsRule);
 *
 *   NodePtr current = makeEquation(pool, lhs, rhs);
 *   while (true) {
 *       auto result = engine.applyOneStep(current);
 *       if (!result.changed) break;
 *       stepLogger.record(result.ruleName, result.newTree);
 *       current = result.newTree;
 *   }
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13A (TRS Infrastructure)
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "PersistentAST.h"
#include "CasMemory.h"

namespace cas {

// ═════════════════════════════════════════════════════════════════════════════
// §1 — RulePhase — Pedagogical sequencing
//
// Rules are tagged with the teaching phase they belong to.  The engine can
// be configured to apply only rules up to a given phase, enabling a phased
// tutorial presentation:
//
//   Expansion      — Expand brackets, distribute multiplication over addition.
//   Simplification — Collect like terms, evaluate constant sub-expressions.
//   Transposition  — Move terms across the equals sign (add/subtract both sides).
//   Reduction      — Divide/multiply both sides to isolate the variable.
// ═════════════════════════════════════════════════════════════════════════════

enum class RulePhase : uint8_t {
    Expansion      = 0,   ///< Expand brackets, apply distributive law.
    Simplification = 1,   ///< Combine like terms, fold constants.
    Transposition  = 2,   ///< Transpose terms across the equals sign.
    Reduction      = 3,   ///< Divide / multiply to reduce to solved form.
};

// ─────────────────────────────────────────────────────────────────────────────
// Convenience: convert RulePhase to human-readable string.
// ─────────────────────────────────────────────────────────────────────────────
const char* rulePhaseName(RulePhase phase);

// ═════════════════════════════════════════════════════════════════════════════
// §2 — MatchCaptures
//
// A wildcard capture map: maps wildcard names (e.g. "_a", "_b") to the
// sub-trees they were bound to during pattern matching.
// ═════════════════════════════════════════════════════════════════════════════

/// Map from wildcard name to bound sub-tree.
using MatchCaptures = std::unordered_map<std::string, NodePtr>;

// ═════════════════════════════════════════════════════════════════════════════
// §3 — RewriteRule
//
// A declarative algebraic transformation rule.
//
// Memory ownership:
//   The `pattern` and `replacement` NodePtrs are owned by the RuleEngine
//   (or the caller who adds them), which keeps them alive for the duration
//   of the solve session.  They are allocated from the same CasMemoryPool
//   as the working tree, or from a separate "rule pool" that outlives the
//   session pool.
// ═════════════════════════════════════════════════════════════════════════════

struct RewriteRule {
    // ── Identity ──────────────────────────────────────────────────────────
    std::string name;         ///< Short identifier, e.g. "combine_constants"
    std::string description;  ///< Human-readable step log text,
                              ///  e.g. "Combine constant terms"

    // ── Phase metadata ─────────────────────────────────────────────────────
    RulePhase phase;          ///< Teaching phase this rule belongs to.

    // ── Pattern ────────────────────────────────────────────────────────────
    /// LHS pattern.  VariableNodes whose name starts with '_' are wildcards.
    NodePtr pattern;

    /// RHS replacement template.  Wildcard names refer back to captures from
    /// pattern matching.  All other nodes are literal.
    NodePtr replacement;

    // ── Optional custom matcher / transformer ─────────────────────────────
    //
    // For rules that are easier to express procedurally (e.g. "evaluate any
    // constant sub-expression") a custom transformer can be supplied instead
    // of (or in addition to) a structural pattern.
    //
    // Signature: (node, pool) → new NodePtr, or nullptr if rule doesn't apply.
    //
    using CustomTransform = std::function<NodePtr(const AstNode&, CasMemoryPool&)>;
    CustomTransform customTransform;   ///< Optional; nullptr means use pattern matching.

    // ── Construction helpers ───────────────────────────────────────────────

    /// Construct from structural pattern / replacement.
    RewriteRule(std::string n, std::string desc, RulePhase ph,
                NodePtr pat, NodePtr repl)
        : name(std::move(n))
        , description(std::move(desc))
        , phase(ph)
        , pattern(std::move(pat))
        , replacement(std::move(repl))
        , customTransform(nullptr)
    {}

    /// Construct from a custom procedural transform.
    RewriteRule(std::string n, std::string desc, RulePhase ph,
                CustomTransform transform)
        : name(std::move(n))
        , description(std::move(desc))
        , phase(ph)
        , pattern(nullptr)
        , replacement(nullptr)
        , customTransform(std::move(transform))
    {}
};

// ═════════════════════════════════════════════════════════════════════════════
// §4 — RuleMatcher
//
// Implements structural pattern matching with basic AC support.
//
// Wildcard convention:
//   A VariableNode whose `name` begins with '_' (e.g. '_a', '_b') is a
//   pattern wildcard.  It matches any sub-tree and is recorded in `captures`.
//   Multiple occurrences of the same wildcard must match the same sub-tree
//   (linearity constraint).
//
// AC matching for Sum and Product:
//   If the pattern is Sum{_a, _b} and the subject has more terms, the
//   matcher tries to unify _a with each term and _b with the Sum of the
//   remaining terms.  This covers both commutativity (any ordering) and
//   basic associativity (grouping of leftovers).
// ═════════════════════════════════════════════════════════════════════════════

class RuleMatcher {
public:
    // ── Pattern Matching ─────────────────────────────────────────────────

    /// Attempt to unify `pattern` against `subject`.
    ///
    /// On success: returns true and fills `captures` with wildcard bindings.
    /// On failure: returns false; `captures` may be partially filled (discard).
    ///
    /// Thread-safety: stateless; safe to call from multiple contexts if the
    /// pool is not shared across threads.
    static bool matchPattern(const AstNode& pattern,
                             const AstNode& subject,
                             MatchCaptures& captures);

    // ── Replacement Instantiation ─────────────────────────────────────────

    /// Instantiate `replacement` by substituting all wildcard VariableNodes
    /// with their captured sub-trees.
    ///
    /// Wildcards not found in `captures` are left as VariableNodes (defensive).
    /// Returns nullptr if `replacement` is nullptr.
    static NodePtr applyCaptures(const AstNode& replacement,
                                 const MatchCaptures& captures,
                                 CasMemoryPool& pool);

    // ── AC matching helpers ───────────────────────────────────────────────

    /// AC matching for SumNode patterns against SumNode subjects.
    static bool matchACSumPattern(const SumNode& pattern,
                                  const SumNode& subject,
                                  MatchCaptures& captures);

    /// AC matching for ProductNode patterns against ProductNode subjects.
    static bool matchACProductPattern(const ProductNode& pattern,
                                      const ProductNode& subject,
                                      MatchCaptures& captures);

    /// Helper: try to match `patternTerms` against `subjectTerms` in any order.
    /// `remaining` receives the indices of unmatched subject terms.
    static bool matchACTerms(const NodeList& patternTerms,
                             const NodeList& subjectTerms,
                             MatchCaptures& captures,
                             std::vector<std::size_t>& matchedSubjectIndices);

    /// Helper: does `name` denote a wildcard (starts with '_')?
    static bool isWildcard(const std::string& name) {
        return !name.empty() && name[0] == '_';
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// §5 — RewriteResult
//
// Returned by RuleEngine::applyOneStep(); bundles the outcome of one step.
// ═════════════════════════════════════════════════════════════════════════════

struct RewriteResult {
    bool    changed;      ///< False if no rule fired (fixed point reached).
    NodePtr newTree;      ///< The new root after rewriting (shares sub-trees with old).
    std::string ruleName; ///< Name of the rule that was applied.
    std::string ruleDesc; ///< Human-readable description for the step log.
    RulePhase   phase;    ///< Phase of the applied rule.
};

// ═════════════════════════════════════════════════════════════════════════════
// §6 — RuleEngine
//
// Orchestrator: holds the rule registry and applies rules using the
// Single-Redex strategy (one transformation per step, depth-first,
// left-to-right, first-matching-rule wins).
// ═════════════════════════════════════════════════════════════════════════════

class RuleEngine {
public:
    // ── Construction ─────────────────────────────────────────────────────

    /// `pool` must outlive the RuleEngine.  All new nodes created during
    /// rewriting are allocated from this pool.
    explicit RuleEngine(CasMemoryPool& pool);

    // Non-copyable, non-movable (holds reference to pool)
    RuleEngine(const RuleEngine&) = delete;
    RuleEngine& operator=(const RuleEngine&) = delete;

    // ── Rule registry ─────────────────────────────────────────────────────

    /// Add a rule to the registry.  Rules are tried in insertion order.
    void addRule(RewriteRule rule);

    /// Remove all rules (e.g. before reconfiguring for a new phase).
    void clearRules();

    /// Number of registered rules.
    std::size_t ruleCount() const { return _rules.size(); }

    /// Maximum pedagogical phase to apply.  Rules with phase > maxPhase
    /// are silently skipped.  Default: Reduction (all phases enabled).
    void setMaxPhase(RulePhase maxPhase) { _maxPhase = maxPhase; }
    RulePhase maxPhase() const { return _maxPhase; }

    // ── Single-Redex application ──────────────────────────────────────────

    /// Attempt to apply ONE rule to ONE sub-tree of `root`.
    ///
    /// Strategy:
    ///   1. Depth-first, left-to-right traversal of `root`.
    ///   2. At each node, try all registered rules (in order) until one fires.
    ///   3. On the first match: rebuild the path from that node to the root
    ///      using COW helpers, producing a new root that shares all unchanged
    ///      sub-trees with the old root.
    ///   4. Return RewriteResult{changed=true, newTree=newRoot, …}.
    ///   5. If no rule fires anywhere in the tree: return {changed=false}.
    ///
    /// The old `root` NodePtr remains valid (persistent AST guarantee).
    RewriteResult applyOneStep(const NodePtr& root);

    /// Apply steps until no rule fires (fixed point) or `maxSteps` is reached.
    /// Returns the final tree and a log of each step taken.
    struct StepLog {
        std::string ruleName;
        std::string ruleDesc;
        RulePhase   phase;
        NodePtr     tree;     ///< Tree state AFTER this step (shared with history)
    };
    struct SolveResult {
        NodePtr               finalTree;
        std::vector<StepLog>  steps;
        bool                  reachedFixedPoint;  ///< True if stopped naturally
    };
    SolveResult applyToFixedPoint(const NodePtr& root, std::size_t maxSteps = 100);

private:
    CasMemoryPool&          _pool;
    std::vector<RewriteRule> _rules;
    RulePhase               _maxPhase = RulePhase::Reduction;

    // ── Internal traversal ────────────────────────────────────────────────

    /// Recursive depth-first traversal.  Tries to rewrite at `node` first
    /// (innermost/leftmost strategy), then recurses into children.
    ///
    /// Returns a new NodePtr if a rewrite occurred, or nullptr if no rule fired.
    /// `ruleName`, `ruleDesc`, `phase` are set on success.
    NodePtr tryRewrite(const NodePtr& node,
                       std::string& ruleName,
                       std::string& ruleDesc,
                       RulePhase& phase);

    /// Try every rule against `node` at the current position.
    /// Returns a rewritten NodePtr on success, nullptr otherwise.
    NodePtr tryRulesAt(const AstNode& node,
                       std::string& ruleName,
                       std::string& ruleDesc,
                       RulePhase& phase);
};

}  // namespace cas
