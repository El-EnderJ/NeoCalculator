/**
 * RuleEngine.cpp — Implementation of the TRS Rule Matching Engine.
 *
 * Implements:
 *   · RuleMatcher::matchPattern()     — Structural + basic AC pattern matching
 *   · RuleMatcher::applyCaptures()    — Replacement instantiation
 *   · RuleEngine::applyOneStep()      — Single-redex rewriting strategy
 *   · RuleEngine::applyToFixedPoint() — Iterative application until no change
 *
 * ── Wildcard naming convention ────────────────────────────────────────────────
 *   VariableNode::name is std::string.  Any name beginning with '_' is a
 *   pattern wildcard (e.g. "_a", "_base", "_exp").  All other names are
 *   literal variable references (e.g. "x", "y").
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13A (TRS Infrastructure)
 */

#include "RuleEngine.h"
#include "AstDiff.h"
#include <algorithm>
#include <cassert>

namespace cas {

// ═════════════════════════════════════════════════════════════════════════════
// §1 — rulePhaseName
// ═════════════════════════════════════════════════════════════════════════════

const char* rulePhaseName(RulePhase phase) {
    switch (phase) {
        case RulePhase::Expansion:      return "Expansion";
        case RulePhase::Simplification: return "Simplification";
        case RulePhase::Transposition:  return "Transposition";
        case RulePhase::Reduction:      return "Reduction";
        default:                        return "Unknown";
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// §2 — Internal matchPatternPtr
//
// Primary matching function.  Takes both pattern and subject as NodePtr so
// wildcards can be bound to the original shared_ptr (preserving ownership).
//
// `pool` is required to construct intermediate nodes (e.g. SumNode / ProductNode
// from remaining AC terms) during wildcard binding.
// ═════════════════════════════════════════════════════════════════════════════

// Forward declaration (matchACSumPatternPtr / matchACProductPatternPtr are
// mutually recursive with matchPatternPtr via AC helpers).
static bool matchPatternPtr(const AstNode&   pattern,
                             const NodePtr&   subjectPtr,
                             MatchCaptures&   captures,
                             CasMemoryPool&   pool);

// ─────────────────────────────────────────────────────────────────────────────
// AC Sum matching — tries all permutations and 2-wildcard folding.
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchACSumPattern(const SumNode&   pattern,
                                     const SumNode&   subject,
                                     MatchCaptures&   captures,
                                     CasMemoryPool&   pool)
{
    const NodeList& pTerms = pattern.children;
    const NodeList& sTerms = subject.children;

    // ── Case 1: same arity — try all permutations ─────────────────────────
    if (pTerms.size() == sTerms.size()) {
        std::vector<std::size_t> perm(sTerms.size());
        for (std::size_t i = 0; i < perm.size(); ++i) perm[i] = i;

        do {
            MatchCaptures tryCaps = captures;
            bool ok = true;
            for (std::size_t i = 0; i < pTerms.size(); ++i) {
                if (!pTerms[i] || !sTerms[perm[i]]) { ok = false; break; }
                if (!matchPatternPtr(*pTerms[i], sTerms[perm[i]], tryCaps, pool)) {
                    ok = false; break;
                }
            }
            if (ok) {
                captures = std::move(tryCaps);
                return true;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));

        return false;
    }

    // ── Case 2: pattern has 2 terms, subject has ≥ 2 terms ───────────────
    // Handle patterns like _a + _b against Sum{x, y, z}:
    //   Try each subject term as _a's sole match; bind _b to Sum{remaining}.
    if (pTerms.size() == 2 && sTerms.size() >= 2) {
        for (int swap = 0; swap < 2; ++swap) {
            const NodePtr& pFirst  = pTerms[swap ? 1 : 0];
            const NodePtr& pSecond = pTerms[swap ? 0 : 1];
            if (!pFirst || !pSecond) continue;

            for (std::size_t i = 0; i < sTerms.size(); ++i) {
                if (!sTerms[i]) continue;
                MatchCaptures tryCaps = captures;

                // Try matching pFirst against sTerms[i]
                if (!matchPatternPtr(*pFirst, sTerms[i], tryCaps, pool)) continue;

                // Build NodePtr for remaining terms
                NodeList remaining;
                for (std::size_t j = 0; j < sTerms.size(); ++j) {
                    if (j != i) remaining.push_back(sTerms[j]);
                }
                if (remaining.empty()) continue;

                // Construct the remaining sub-expression as a NodePtr
                NodePtr pSecondSubject;
                if (remaining.size() == 1) {
                    pSecondSubject = remaining[0];
                } else {
                    // Allocate a new SumNode from the pool for the remaining terms
                    pSecondSubject = makeSum(pool, std::move(remaining));
                }

                if (!matchPatternPtr(*pSecond, pSecondSubject, tryCaps, pool)) continue;

                captures = std::move(tryCaps);
                return true;
            }
        }
        return false;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC Product matching (symmetric to AC Sum matching)
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchACProductPattern(const ProductNode& pattern,
                                         const ProductNode& subject,
                                         MatchCaptures&     captures,
                                         CasMemoryPool&     pool)
{
    const NodeList& pTerms = pattern.children;
    const NodeList& sTerms = subject.children;

    // Same arity: try all permutations
    if (pTerms.size() == sTerms.size()) {
        std::vector<std::size_t> perm(sTerms.size());
        for (std::size_t i = 0; i < perm.size(); ++i) perm[i] = i;

        do {
            MatchCaptures tryCaps = captures;
            bool ok = true;
            for (std::size_t i = 0; i < pTerms.size(); ++i) {
                if (!pTerms[i] || !sTerms[perm[i]]) { ok = false; break; }
                if (!matchPatternPtr(*pTerms[i], sTerms[perm[i]], tryCaps, pool)) {
                    ok = false; break;
                }
            }
            if (ok) {
                captures = std::move(tryCaps);
                return true;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));

        return false;
    }

    // 2-wildcard pattern against N-term product
    if (pTerms.size() == 2 && sTerms.size() >= 2) {
        for (int swap = 0; swap < 2; ++swap) {
            const NodePtr& pFirst  = pTerms[swap ? 1 : 0];
            const NodePtr& pSecond = pTerms[swap ? 0 : 1];
            if (!pFirst || !pSecond) continue;

            for (std::size_t i = 0; i < sTerms.size(); ++i) {
                if (!sTerms[i]) continue;
                MatchCaptures tryCaps = captures;

                if (!matchPatternPtr(*pFirst, sTerms[i], tryCaps, pool)) continue;

                NodeList remaining;
                for (std::size_t j = 0; j < sTerms.size(); ++j) {
                    if (j != i) remaining.push_back(sTerms[j]);
                }
                if (remaining.empty()) continue;

                NodePtr pSecondSubject;
                if (remaining.size() == 1) {
                    pSecondSubject = remaining[0];
                } else {
                    // Allocate a new ProductNode from the pool for remaining terms
                    pSecondSubject = makeProduct(pool, std::move(remaining));
                }

                if (!matchPatternPtr(*pSecond, pSecondSubject, tryCaps, pool)) continue;
                captures = std::move(tryCaps);
                return true;
            }
        }
        return false;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// matchACTerms — reserved for future full AC matching extension
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchACTerms(const NodeList& /*patternTerms*/,
                                const NodeList& /*subjectTerms*/,
                                MatchCaptures& /*captures*/,
                                std::vector<std::size_t>& /*matchedSubjectIndices*/)
{
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// matchPatternPtr — core recursive matching with wildcard binding
// ─────────────────────────────────────────────────────────────────────────────

static bool matchPatternPtr(const AstNode&   pattern,
                             const NodePtr&   subjectPtr,
                             MatchCaptures&   captures,
                             CasMemoryPool&   pool)
{
    if (!subjectPtr) return false;
    const AstNode& subject = *subjectPtr;

    // ── Wildcard: VariableNode whose name starts with '_' ─────────────────
    if (pattern.nodeType == NodeType::Variable) {
        const auto& patVar = static_cast<const VariableNode&>(pattern);
        const std::string& varName = patVar.name;

        if (RuleMatcher::isWildcard(varName)) {
            auto it = captures.find(varName);
            if (it != captures.end()) {
                // Linearity constraint: subsequent occurrence must match binding
                return it->second && it->second->equals(subject);
            }
            // First occurrence: bind wildcard to the subject NodePtr
            captures[varName] = subjectPtr;
            return true;
        }

        // Literal variable: subject must be the same variable name
        if (subject.nodeType != NodeType::Variable) return false;
        return varName == static_cast<const VariableNode&>(subject).name;
    }

    // ── ConstantNode — value equality ─────────────────────────────────────
    if (pattern.nodeType == NodeType::Constant) {
        if (subject.nodeType != NodeType::Constant) return false;
        return static_cast<const ConstantNode&>(pattern).value ==
               static_cast<const ConstantNode&>(subject).value;
    }

    // ── NegationNode — recurse into operand ───────────────────────────────
    if (pattern.nodeType == NodeType::Negation) {
        if (subject.nodeType != NodeType::Negation) return false;
        const auto& pn = static_cast<const NegationNode&>(pattern);
        const auto& sn = static_cast<const NegationNode&>(subject);
        if (!pn.operand || !sn.operand) return (pn.operand == sn.operand);
        return matchPatternPtr(*pn.operand, sn.operand, captures, pool);
    }

    // ── SumNode — AC matching ─────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Sum) {
        if (subject.nodeType != NodeType::Sum) return false;
        return RuleMatcher::matchACSumPattern(
            static_cast<const SumNode&>(pattern),
            static_cast<const SumNode&>(subject),
            captures, pool);
    }

    // ── ProductNode — AC matching ─────────────────────────────────────────
    if (pattern.nodeType == NodeType::Product) {
        if (subject.nodeType != NodeType::Product) return false;
        return RuleMatcher::matchACProductPattern(
            static_cast<const ProductNode&>(pattern),
            static_cast<const ProductNode&>(subject),
            captures, pool);
    }

    // ── PowerNode ─────────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Power) {
        if (subject.nodeType != NodeType::Power) return false;
        const auto& pp = static_cast<const PowerNode&>(pattern);
        const auto& sp = static_cast<const PowerNode&>(subject);
        if (!pp.base || !sp.base || !pp.exponent || !sp.exponent) return false;
        MatchCaptures localCaps = captures;
        if (!matchPatternPtr(*pp.base,     sp.base,     localCaps, pool)) return false;
        if (!matchPatternPtr(*pp.exponent, sp.exponent, localCaps, pool)) return false;
        captures = std::move(localCaps);
        return true;
    }

    // ── EquationNode ──────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Equation) {
        if (subject.nodeType != NodeType::Equation) return false;
        const auto& pe = static_cast<const EquationNode&>(pattern);
        const auto& se = static_cast<const EquationNode&>(subject);
        if (!pe.lhs || !se.lhs || !pe.rhs || !se.rhs) return false;
        MatchCaptures localCaps = captures;
        if (!matchPatternPtr(*pe.lhs, se.lhs, localCaps, pool)) return false;
        if (!matchPatternPtr(*pe.rhs, se.rhs, localCaps, pool)) return false;
        captures = std::move(localCaps);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public RuleMatcher::matchPattern (ref-based, no pool, no wildcards)
//
// This overload is provided for callers that have AstNode references and are
// matching patterns that contain no wildcards (e.g. structural equality checks).
// For patterns that include wildcards, use the NodePtr path via tryRulesAt.
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchPattern(const AstNode& pattern,
                                const AstNode& subject,
                                MatchCaptures& captures)
{
    // ── Wildcard ──────────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Variable) {
        const auto& patVar = static_cast<const VariableNode&>(pattern);
        if (isWildcard(patVar.name)) {
            // Wildcards cannot be bound without a NodePtr — check linearity only
            auto it = captures.find(patVar.name);
            if (it != captures.end()) {
                return it->second && it->second->equals(subject);
            }
            // Can't bind without NodePtr: report mismatch so callers use NodePtr path
            return false;
        }
        if (subject.nodeType != NodeType::Variable) return false;
        return patVar.name == static_cast<const VariableNode&>(subject).name;
    }

    if (pattern.nodeType == NodeType::Constant) {
        if (subject.nodeType != NodeType::Constant) return false;
        return static_cast<const ConstantNode&>(pattern).value ==
               static_cast<const ConstantNode&>(subject).value;
    }

    if (pattern.nodeType == NodeType::Negation) {
        if (subject.nodeType != NodeType::Negation) return false;
        const auto& pn = static_cast<const NegationNode&>(pattern);
        const auto& sn = static_cast<const NegationNode&>(subject);
        if (!pn.operand || !sn.operand) return (pn.operand == sn.operand);
        return matchPattern(*pn.operand, *sn.operand, captures);
    }

    if (pattern.nodeType == NodeType::Power) {
        if (subject.nodeType != NodeType::Power) return false;
        const auto& pp = static_cast<const PowerNode&>(pattern);
        const auto& sp = static_cast<const PowerNode&>(subject);
        if (!pp.base || !sp.base || !pp.exponent || !sp.exponent) return false;
        MatchCaptures localCaps = captures;
        if (!matchPattern(*pp.base,     *sp.base,     localCaps)) return false;
        if (!matchPattern(*pp.exponent, *sp.exponent, localCaps)) return false;
        captures = std::move(localCaps);
        return true;
    }

    if (pattern.nodeType == NodeType::Equation) {
        if (subject.nodeType != NodeType::Equation) return false;
        const auto& pe = static_cast<const EquationNode&>(pattern);
        const auto& se = static_cast<const EquationNode&>(subject);
        if (!pe.lhs || !se.lhs || !pe.rhs || !se.rhs) return false;
        MatchCaptures localCaps = captures;
        if (!matchPattern(*pe.lhs, *se.lhs, localCaps)) return false;
        if (!matchPattern(*pe.rhs, *se.rhs, localCaps)) return false;
        captures = std::move(localCaps);
        return true;
    }

    // Sum/Product without pool: fall back to structural equality only
    return pattern.equals(subject);
}

// ═════════════════════════════════════════════════════════════════════════════
// §3 — RuleMatcher::applyCaptures
//
// Recursively walk the replacement template, substituting wildcard
// VariableNodes with their captured sub-trees.
// ═════════════════════════════════════════════════════════════════════════════

NodePtr RuleMatcher::applyCaptures(const AstNode& replacement,
                                    const MatchCaptures& captures,
                                    CasMemoryPool& pool)
{
    // ── Wildcard substitution ─────────────────────────────────────────────
    if (replacement.nodeType == NodeType::Variable) {
        const auto& v = static_cast<const VariableNode&>(replacement);
        if (isWildcard(v.name)) {
            auto it = captures.find(v.name);
            if (it != captures.end()) return it->second;
            // Wildcard not in captures: emit a variable node as-is (defensive)
            return makeVariableNamed(pool, v.name);
        }
        return makeVariableNamed(pool, v.name);
    }

    if (replacement.nodeType == NodeType::Constant) {
        return makeConstant(pool, static_cast<const ConstantNode&>(replacement).value);
    }

    if (replacement.nodeType == NodeType::Negation) {
        const auto& neg = static_cast<const NegationNode&>(replacement);
        if (!neg.operand) return makeConstant(pool, 0.0);
        return makeNegation(pool, applyCaptures(*neg.operand, captures, pool));
    }

    if (replacement.nodeType == NodeType::Sum) {
        const auto& sum = static_cast<const SumNode&>(replacement);
        NodeList kids;
        kids.reserve(sum.children.size());
        for (const auto& c : sum.children) {
            if (c) kids.push_back(applyCaptures(*c, captures, pool));
        }
        return makeSum(pool, std::move(kids));
    }

    if (replacement.nodeType == NodeType::Product) {
        const auto& prod = static_cast<const ProductNode&>(replacement);
        NodeList kids;
        kids.reserve(prod.children.size());
        for (const auto& c : prod.children) {
            if (c) kids.push_back(applyCaptures(*c, captures, pool));
        }
        return makeProduct(pool, std::move(kids));
    }

    if (replacement.nodeType == NodeType::Power) {
        const auto& pw = static_cast<const PowerNode&>(replacement);
        if (!pw.base || !pw.exponent) return makeConstant(pool, 1.0);
        NodePtr b = applyCaptures(*pw.base,     captures, pool);
        NodePtr e = applyCaptures(*pw.exponent, captures, pool);
        return makePower(pool, std::move(b), std::move(e));
    }

    if (replacement.nodeType == NodeType::Equation) {
        const auto& eq = static_cast<const EquationNode&>(replacement);
        if (!eq.lhs || !eq.rhs) return makeConstant(pool, 0.0);
        NodePtr l = applyCaptures(*eq.lhs, captures, pool);
        NodePtr r = applyCaptures(*eq.rhs, captures, pool);
        return makeEquation(pool, std::move(l), std::move(r));
    }

    return nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
// §4 — RuleEngine
// ═════════════════════════════════════════════════════════════════════════════

RuleEngine::RuleEngine(CasMemoryPool& pool)
    : _pool(pool)
{}

void RuleEngine::addRule(RewriteRule rule) {
    _rules.push_back(std::move(rule));
}

void RuleEngine::clearRules() {
    _rules.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// §4.1 — tryRulesAt
//
// Tries every registered rule against `nodePtr` at the current tree position.
// Uses the NodePtr path for proper wildcard binding.
// ─────────────────────────────────────────────────────────────────────────────

NodePtr RuleEngine::tryRulesAt(const AstNode& node,
                                std::string& ruleName,
                                std::string& ruleDesc,
                                RulePhase& phase)
{
    for (const auto& rule : _rules) {
        if (static_cast<uint8_t>(rule.phase) >
            static_cast<uint8_t>(_maxPhase)) {
            continue;
        }

        // ── Custom procedural transform ───────────────────────────────────
        if (rule.customTransform) {
            NodePtr result = rule.customTransform(node, _pool);
            if (result) {
                ruleName = rule.name;
                ruleDesc = rule.description;
                phase    = rule.phase;
                return result;
            }
            continue;
        }

        // ── Structural pattern match (ref-based, no wildcards) ────────────
        // This path handles patterns without wildcards.
        // For patterns with wildcards, rules should use customTransform or
        // the engine will be extended in a future phase.
        if (!rule.pattern || !rule.replacement) continue;

        MatchCaptures captures;
        if (!RuleMatcher::matchPattern(*rule.pattern, node, captures)) continue;

        NodePtr result = RuleMatcher::applyCaptures(*rule.replacement, captures, _pool);
        if (!result) continue;

        ruleName = rule.name;
        ruleDesc = rule.description;
        phase    = rule.phase;
        return result;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// tryRulesAtPtr — variant that takes a NodePtr for wildcard binding
// ─────────────────────────────────────────────────────────────────────────────

static NodePtr tryRulesAtPtr(const NodePtr&           nodePtr,
                              std::vector<RewriteRule>& rules,
                              RulePhase                 maxPhase,
                              CasMemoryPool&            pool,
                              std::string&              ruleName,
                              std::string&              ruleDesc,
                              RulePhase&                phase)
{
    if (!nodePtr) return nullptr;
    const AstNode& node = *nodePtr;

    for (auto& rule : rules) {
        if (static_cast<uint8_t>(rule.phase) >
            static_cast<uint8_t>(maxPhase)) {
            continue;
        }

        // Custom transform
        if (rule.customTransform) {
            NodePtr result = rule.customTransform(node, pool);
            if (result) {
                ruleName = rule.name;
                ruleDesc = rule.description;
                phase    = rule.phase;
                return result;
            }
            continue;
        }

        if (!rule.pattern || !rule.replacement) continue;

        // Use NodePtr-based matching so wildcards are properly bound
        MatchCaptures captures;
        if (!matchPatternPtr(*rule.pattern, nodePtr, captures, pool)) continue;

        NodePtr result = RuleMatcher::applyCaptures(*rule.replacement, captures, pool);
        if (!result) continue;

        ruleName = rule.name;
        ruleDesc = rule.description;
        phase    = rule.phase;
        return result;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// §4.2 — tryRewrite
//
// Recursive depth-first traversal.  Tries children first (innermost-leftmost),
// then tries rules at the current node.
// ─────────────────────────────────────────────────────────────────────────────

NodePtr RuleEngine::tryRewrite(const NodePtr& nodePtr,
                                std::string& ruleName,
                                std::string& ruleDesc,
                                RulePhase& phase)
{
    if (!nodePtr) return nullptr;
    const AstNode& node = *nodePtr;

    // ── Recurse into children first (innermost-leftmost strategy) ─────────

    if (node.nodeType == NodeType::Equation) {
        const auto& eq = static_cast<const EquationNode&>(node);
        if (eq.lhs) {
            NodePtr newLhs = tryRewrite(eq.lhs, ruleName, ruleDesc, phase);
            if (newLhs) return replaceEquationLHS(_pool, eq, std::move(newLhs));
        }
        if (eq.rhs) {
            NodePtr newRhs = tryRewrite(eq.rhs, ruleName, ruleDesc, phase);
            if (newRhs) return replaceEquationRHS(_pool, eq, std::move(newRhs));
        }
    }
    else if (node.nodeType == NodeType::Sum) {
        const auto& sum = static_cast<const SumNode&>(node);
        for (std::size_t i = 0; i < sum.children.size(); ++i) {
            NodePtr nc = tryRewrite(sum.children[i], ruleName, ruleDesc, phase);
            if (nc) return replaceChildInSum(_pool, sum, i, std::move(nc));
        }
    }
    else if (node.nodeType == NodeType::Product) {
        const auto& prod = static_cast<const ProductNode&>(node);
        for (std::size_t i = 0; i < prod.children.size(); ++i) {
            NodePtr nc = tryRewrite(prod.children[i], ruleName, ruleDesc, phase);
            if (nc) return replaceChildInProduct(_pool, prod, i, std::move(nc));
        }
    }
    else if (node.nodeType == NodeType::Power) {
        const auto& pw = static_cast<const PowerNode&>(node);
        if (pw.base) {
            NodePtr nb = tryRewrite(pw.base, ruleName, ruleDesc, phase);
            if (nb) return makePower(_pool, std::move(nb), pw.exponent);
        }
        if (pw.exponent) {
            NodePtr ne = tryRewrite(pw.exponent, ruleName, ruleDesc, phase);
            if (ne) return makePower(_pool, pw.base, std::move(ne));
        }
    }
    else if (node.nodeType == NodeType::Negation) {
        const auto& neg = static_cast<const NegationNode&>(node);
        if (neg.operand) {
            NodePtr no = tryRewrite(neg.operand, ruleName, ruleDesc, phase);
            if (no) return makeNegation(_pool, std::move(no));
        }
    }

    // ── Try rules at the current node via NodePtr path ────────────────────
    return tryRulesAtPtr(nodePtr, _rules, _maxPhase, _pool,
                         ruleName, ruleDesc, phase);
}

// ─────────────────────────────────────────────────────────────────────────────
// §4.3 — applyOneStep
// ─────────────────────────────────────────────────────────────────────────────

RewriteResult RuleEngine::applyOneStep(const NodePtr& root) {
    RewriteResult result;
    result.changed      = false;
    result.newTree      = root;
    result.phase        = RulePhase::Expansion;

    if (!root) return result;

    std::string ruleName, ruleDesc;
    RulePhase   phase;
    NodePtr     newRoot = tryRewrite(root, ruleName, ruleDesc, phase);

    if (newRoot) {
        result.changed      = true;
        result.newTree      = std::move(newRoot);
        result.affectedNode = findChangedNodes(root, result.newTree);
        result.ruleName     = std::move(ruleName);
        result.ruleDesc     = std::move(ruleDesc);
        result.phase        = phase;
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// §4.4 — applyToFixedPoint
// ─────────────────────────────────────────────────────────────────────────────

RuleEngine::SolveResult RuleEngine::applyToFixedPoint(const NodePtr& root,
                                                        std::size_t maxSteps)
{
    SolveResult solveResult;
    solveResult.reachedFixedPoint = false;

    NodePtr current = root;

    for (std::size_t step = 0; step < maxSteps; ++step) {
        RewriteResult r = applyOneStep(current);
        if (!r.changed) {
            solveResult.reachedFixedPoint = true;
            break;
        }
        StepLog log;
        log.ruleName     = std::move(r.ruleName);
        log.ruleDesc     = std::move(r.ruleDesc);
        log.phase        = r.phase;
        log.tree         = r.newTree;
        log.affectedNode = r.affectedNode;
        solveResult.steps.push_back(std::move(log));
        current = r.newTree;
    }

    solveResult.finalTree = current;
    return solveResult;
}

}  // namespace cas
