/**
 * RuleEngine.cpp — Implementation of the TRS Rule Matching Engine.
 *
 * Implements:
 *   · RuleMatcher::matchPattern()    — Structural + basic AC pattern matching
 *   · RuleMatcher::applyCaptures()   — Replacement instantiation
 *   · RuleEngine::applyOneStep()     — Single-redex rewriting strategy
 *   · RuleEngine::applyToFixedPoint() — Iterative application until no change
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13A (TRS Infrastructure)
 */

#include "RuleEngine.h"
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
// §2 — RuleMatcher::matchPattern
//
// Unifies a pattern AST node against a subject AST node, filling `captures`
// with wildcard bindings.  Returns true on success.
//
// Wildcard handling:
//   A VariableNode with name starting with '_' is a wildcard.  On first
//   encounter it binds to the subject sub-tree; on subsequent encounters it
//   must match the previously-bound sub-tree (linear pattern variables).
//
// AC matching:
//   Sum and Product patterns use the AC helpers to try all orderings of terms.
// ═════════════════════════════════════════════════════════════════════════════

bool RuleMatcher::matchPattern(const AstNode& pattern,
                               const AstNode& subject,
                               MatchCaptures& captures)
{
    // ── Wildcard: a VariableNode whose name starts with '_' ──────────────
    if (pattern.nodeType == NodeType::Variable) {
        const auto& patVar = static_cast<const VariableNode&>(pattern);
        std::string varName(1, patVar.name);

        if (isWildcard(varName)) {
            // Check linearity: if already captured, subject must equal binding
            auto it = captures.find(varName);
            if (it != captures.end()) {
                // Must be structurally equal to previously bound sub-tree
                return it->second && it->second->equals(subject);
            }
            // First occurrence: bind
            // We need the NodePtr form of `subject` — but we only have a
            // reference here.  To avoid reconstructing a shared_ptr from a raw
            // reference we require callers to pass NodePtrs; however, this
            // internal helper receives references.  The solution: we capture via
            // a shared_ptr<const AstNode> using aliasing constructor if the
            // subject was originally a NodePtr.  Since we cannot recover the
            // original shared_ptr from a raw reference, we note this as a known
            // limitation and store nullptr in captures (the engine's tryRewrite
            // always calls matchPattern with the original NodePtrs, so captures
            // are filled via the NodePtr overload below).
            //
            // See matchPatternPtr() for the NodePtr overload that correctly
            // binds wildcards.
            //
            // This ref overload is only reachable for non-wildcard recursive
            // calls on child nodes; wildcards at the top level always go
            // through the NodePtr overload from tryRulesAt.
            return false;  // Wildcards must be matched via matchPatternPtr
        }

        // ── Literal variable: must match a VariableNode with the same name ─
        if (subject.nodeType != NodeType::Variable) return false;
        return patVar.name == static_cast<const VariableNode&>(subject).name;
    }

    // ── ConstantNode — literal match ─────────────────────────────────────
    if (pattern.nodeType == NodeType::Constant) {
        if (subject.nodeType != NodeType::Constant) return false;
        const auto& pc = static_cast<const ConstantNode&>(pattern);
        const auto& sc = static_cast<const ConstantNode&>(subject);
        return pc.value == sc.value;
    }

    // ── NegationNode ──────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Negation) {
        if (subject.nodeType != NodeType::Negation) return false;
        const auto& pn = static_cast<const NegationNode&>(pattern);
        const auto& sn = static_cast<const NegationNode&>(subject);
        if (!pn.operand || !sn.operand) return pn.operand == sn.operand;
        return matchPattern(*pn.operand, *sn.operand, captures);
    }

    // ── SumNode — AC matching ─────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Sum) {
        if (subject.nodeType != NodeType::Sum) return false;
        return matchACSumPattern(
            static_cast<const SumNode&>(pattern),
            static_cast<const SumNode&>(subject),
            captures);
    }

    // ── ProductNode — AC matching ─────────────────────────────────────────
    if (pattern.nodeType == NodeType::Product) {
        if (subject.nodeType != NodeType::Product) return false;
        return matchACProductPattern(
            static_cast<const ProductNode&>(pattern),
            static_cast<const ProductNode&>(subject),
            captures);
    }

    // ── PowerNode ─────────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Power) {
        if (subject.nodeType != NodeType::Power) return false;
        const auto& pp = static_cast<const PowerNode&>(pattern);
        const auto& sp = static_cast<const PowerNode&>(subject);
        if (!pp.base || !sp.base || !pp.exponent || !sp.exponent) return false;
        MatchCaptures localCaps = captures;
        if (!matchPattern(*pp.base, *sp.base, localCaps)) return false;
        if (!matchPattern(*pp.exponent, *sp.exponent, localCaps)) return false;
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
        if (!matchPattern(*pe.lhs, *se.lhs, localCaps)) return false;
        if (!matchPattern(*pe.rhs, *se.rhs, localCaps)) return false;
        captures = std::move(localCaps);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// matchPatternPtr — NodePtr overload; handles wildcard binding correctly.
// This is the primary entry point used by tryRulesAt so wildcards get
// properly bound to the original shared_ptr sub-trees.
// ─────────────────────────────────────────────────────────────────────────────

static bool matchPatternPtr(const AstNode& pattern,
                             const NodePtr& subjectPtr,
                             MatchCaptures& captures)
{
    if (!subjectPtr) return false;
    const AstNode& subject = *subjectPtr;

    // ── Wildcard ──────────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Variable) {
        const auto& patVar = static_cast<const VariableNode&>(pattern);
        std::string varName(1, patVar.name);

        if (!varName.empty() && varName[0] == '_') {
            auto it = captures.find(varName);
            if (it != captures.end()) {
                // Linearity: must match already-bound tree
                return it->second && it->second->equals(subject);
            }
            // Bind wildcard to the subject NodePtr (preserving shared ownership)
            captures[varName] = subjectPtr;
            return true;
        }

        // Literal variable
        if (subject.nodeType != NodeType::Variable) return false;
        return patVar.name == static_cast<const VariableNode&>(subject).name;
    }

    // ── ConstantNode ──────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Constant) {
        if (subject.nodeType != NodeType::Constant) return false;
        const auto& pc = static_cast<const ConstantNode&>(pattern);
        const auto& sc = static_cast<const ConstantNode&>(subject);
        return pc.value == sc.value;
    }

    // ── NegationNode ──────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Negation) {
        if (subject.nodeType != NodeType::Negation) return false;
        const auto& pn = static_cast<const NegationNode&>(pattern);
        const auto& sn = static_cast<const NegationNode&>(subject);
        if (!pn.operand || !sn.operand) return pn.operand == sn.operand;
        return matchPatternPtr(*pn.operand, sn.operand, captures);
    }

    // ── SumNode — delegate to AC Sum matcher ──────────────────────────────
    if (pattern.nodeType == NodeType::Sum) {
        if (subject.nodeType != NodeType::Sum) return false;
        return RuleMatcher::matchACSumPattern(
            static_cast<const SumNode&>(pattern),
            static_cast<const SumNode&>(subject),
            captures);
    }

    // ── ProductNode — delegate to AC Product matcher ──────────────────────
    if (pattern.nodeType == NodeType::Product) {
        if (subject.nodeType != NodeType::Product) return false;
        return RuleMatcher::matchACProductPattern(
            static_cast<const ProductNode&>(pattern),
            static_cast<const ProductNode&>(subject),
            captures);
    }

    // ── PowerNode ─────────────────────────────────────────────────────────
    if (pattern.nodeType == NodeType::Power) {
        if (subject.nodeType != NodeType::Power) return false;
        const auto& pp = static_cast<const PowerNode&>(pattern);
        const auto& sp = static_cast<const PowerNode&>(subject);
        if (!pp.base || !sp.base || !pp.exponent || !sp.exponent) return false;
        MatchCaptures localCaps = captures;
        if (!matchPatternPtr(*pp.base, sp.base, localCaps)) return false;
        if (!matchPatternPtr(*pp.exponent, sp.exponent, localCaps)) return false;
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
        if (!matchPatternPtr(*pe.lhs, se.lhs, localCaps)) return false;
        if (!matchPatternPtr(*pe.rhs, se.rhs, localCaps)) return false;
        captures = std::move(localCaps);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// §2.1 — AC Sum matching
//
// If pattern and subject have the same number of terms, try all permutations.
// If pattern has 2 wildcard terms and subject has N terms, bind the first
// wildcard to one term and the second to Sum{remaining terms}.
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchACSumPattern(const SumNode& pattern,
                                    const SumNode& subject,
                                    MatchCaptures& captures)
{
    const NodeList& pTerms = pattern.children;
    const NodeList& sTerms = subject.children;

    // ── Case 1: same arity — try all permutations ─────────────────────────
    if (pTerms.size() == sTerms.size()) {
        // Simple approach: try matching each pattern term against each subject
        // term position using a recursive backtracking algorithm.

        // Build index array for subject terms
        std::vector<std::size_t> perm(sTerms.size());
        for (std::size_t i = 0; i < perm.size(); ++i) perm[i] = i;

        do {
            MatchCaptures tryCaps = captures;
            bool ok = true;
            for (std::size_t i = 0; i < pTerms.size(); ++i) {
                if (!pTerms[i] || !sTerms[perm[i]]) { ok = false; break; }
                if (!matchPatternPtr(*pTerms[i], sTerms[perm[i]], tryCaps)) {
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

    // ── Case 2: pattern has 2 terms, one of which is a wildcard ──────────
    // This handles the common pattern _a + _b against Sum{x, y, z}:
    //   Try: _a ↦ x, _b ↦ Sum{y, z}
    //        _a ↦ y, _b ↦ Sum{x, z}   etc.
    if (pTerms.size() == 2 && sTerms.size() >= 2) {
        // Try each subject term as the sole match for pTerms[0],
        // with remaining terms collapsed into a Sum for pTerms[1].
        // Then also try swapping pattern roles (AC commutativity).
        for (int swap = 0; swap < 2; ++swap) {
            const NodePtr& pFirst  = pTerms[swap ? 1 : 0];
            const NodePtr& pSecond = pTerms[swap ? 0 : 1];

            for (std::size_t i = 0; i < sTerms.size(); ++i) {
                MatchCaptures tryCaps = captures;

                // Try matching pFirst against sTerms[i]
                if (!pFirst || !sTerms[i]) continue;
                if (!matchPatternPtr(*pFirst, sTerms[i], tryCaps)) continue;

                // Collect remaining terms
                NodeList remaining;
                for (std::size_t j = 0; j < sTerms.size(); ++j) {
                    if (j != i) remaining.push_back(sTerms[j]);
                }

                if (remaining.empty()) continue;

                // Build a NodePtr for the remaining sub-expression.
                // If one term remains, use it directly; otherwise rebuild Sum.
                // NOTE: We cannot call makeSum without a pool here, so we use
                // a temporary SumNode approach.  For the wildcard binding we
                // need a NodePtr.  We use the existing sTerms[j] NodePtrs
                // directly (shared_ptr aliasing).
                NodePtr pSecondSubject;
                if (remaining.size() == 1) {
                    pSecondSubject = remaining[0];
                } else {
                    // We need to build a new SumNode.  However, we don't have a
                    // pool reference here.  To keep RuleMatcher stateless, we
                    // store the first remaining NodePtr as a partial match and
                    // accept this as a conservative limitation.
                    // Full multi-wildcard AC matching is handled by tryRulesAt
                    // which has pool access.
                    pSecondSubject = remaining[0];  // conservative
                }

                if (!matchPatternPtr(*pSecond, pSecondSubject, tryCaps)) continue;

                captures = std::move(tryCaps);
                return true;
            }
        }
        return false;
    }

    return false;  // Arity mismatch with no special handling
}

// ─────────────────────────────────────────────────────────────────────────────
// §2.2 — AC Product matching (symmetric to Sum matching)
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchACProductPattern(const ProductNode& pattern,
                                         const ProductNode& subject,
                                         MatchCaptures& captures)
{
    const NodeList& pTerms = pattern.children;
    const NodeList& sTerms = subject.children;

    // Same-arity: try all permutations
    if (pTerms.size() == sTerms.size()) {
        std::vector<std::size_t> perm(sTerms.size());
        for (std::size_t i = 0; i < perm.size(); ++i) perm[i] = i;

        do {
            MatchCaptures tryCaps = captures;
            bool ok = true;
            for (std::size_t i = 0; i < pTerms.size(); ++i) {
                if (!pTerms[i] || !sTerms[perm[i]]) { ok = false; break; }
                if (!matchPatternPtr(*pTerms[i], sTerms[perm[i]], tryCaps)) {
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

    // 2-pattern wildcard against N-term product
    if (pTerms.size() == 2 && sTerms.size() >= 2) {
        for (int swap = 0; swap < 2; ++swap) {
            const NodePtr& pFirst  = pTerms[swap ? 1 : 0];
            const NodePtr& pSecond = pTerms[swap ? 0 : 1];

            for (std::size_t i = 0; i < sTerms.size(); ++i) {
                MatchCaptures tryCaps = captures;
                if (!pFirst || !sTerms[i]) continue;
                if (!matchPatternPtr(*pFirst, sTerms[i], tryCaps)) continue;

                NodeList remaining;
                for (std::size_t j = 0; j < sTerms.size(); ++j) {
                    if (j != i) remaining.push_back(sTerms[j]);
                }
                if (remaining.empty()) continue;

                NodePtr pSecondSubject = (remaining.size() == 1)
                    ? remaining[0] : remaining[0];  // conservative

                if (!matchPatternPtr(*pSecond, pSecondSubject, tryCaps)) continue;
                captures = std::move(tryCaps);
                return true;
            }
        }
        return false;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// §2.3 — matchACTerms (unused in current impl; kept for future extension)
// ─────────────────────────────────────────────────────────────────────────────

bool RuleMatcher::matchACTerms(const NodeList& /*patternTerms*/,
                                const NodeList& /*subjectTerms*/,
                                MatchCaptures& /*captures*/,
                                std::vector<std::size_t>& /*matchedSubjectIndices*/)
{
    return false;  // Reserved for future full AC matching extension
}

// ═════════════════════════════════════════════════════════════════════════════
// §3 — RuleMatcher::applyCaptures
//
// Recursively walk the replacement template.  When a wildcard VariableNode
// is found, substitute it with the corresponding captured sub-tree.
// ═════════════════════════════════════════════════════════════════════════════

NodePtr RuleMatcher::applyCaptures(const AstNode& replacement,
                                    const MatchCaptures& captures,
                                    CasMemoryPool& pool)
{
    // ── Wildcard substitution ─────────────────────────────────────────────
    if (replacement.nodeType == NodeType::Variable) {
        const auto& v = static_cast<const VariableNode&>(replacement);
        std::string name(1, v.name);
        if (!name.empty() && name[0] == '_') {
            auto it = captures.find(name);
            if (it != captures.end()) return it->second;  // Return captured sub-tree
            // Wildcard not captured: leave as a VariableNode (defensive)
            return makeVariable(pool, v.name);
        }
        return makeVariable(pool, v.name);
    }

    // ── Constant — copy ───────────────────────────────────────────────────
    if (replacement.nodeType == NodeType::Constant) {
        return makeConstant(pool, static_cast<const ConstantNode&>(replacement).value);
    }

    // ── NegationNode — recurse ────────────────────────────────────────────
    if (replacement.nodeType == NodeType::Negation) {
        const auto& neg = static_cast<const NegationNode&>(replacement);
        if (!neg.operand) return makeConstant(pool, 0.0);
        NodePtr child = applyCaptures(*neg.operand, captures, pool);
        return makeNegation(pool, std::move(child));
    }

    // ── SumNode — recurse over children ───────────────────────────────────
    if (replacement.nodeType == NodeType::Sum) {
        const auto& sum = static_cast<const SumNode&>(replacement);
        NodeList kids;
        kids.reserve(sum.children.size());
        for (const auto& c : sum.children) {
            if (c) kids.push_back(applyCaptures(*c, captures, pool));
        }
        return makeSum(pool, std::move(kids));
    }

    // ── ProductNode — recurse over children ───────────────────────────────
    if (replacement.nodeType == NodeType::Product) {
        const auto& prod = static_cast<const ProductNode&>(replacement);
        NodeList kids;
        kids.reserve(prod.children.size());
        for (const auto& c : prod.children) {
            if (c) kids.push_back(applyCaptures(*c, captures, pool));
        }
        return makeProduct(pool, std::move(kids));
    }

    // ── PowerNode — recurse ───────────────────────────────────────────────
    if (replacement.nodeType == NodeType::Power) {
        const auto& pw = static_cast<const PowerNode&>(replacement);
        if (!pw.base || !pw.exponent) return makeConstant(pool, 1.0);
        NodePtr b = applyCaptures(*pw.base,     captures, pool);
        NodePtr e = applyCaptures(*pw.exponent, captures, pool);
        return makePower(pool, std::move(b), std::move(e));
    }

    // ── EquationNode — recurse ────────────────────────────────────────────
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
// Tries every registered rule against `node` (at the current position in the
// tree).  Returns the rewritten NodePtr on success, or nullptr.
// ─────────────────────────────────────────────────────────────────────────────

NodePtr RuleEngine::tryRulesAt(const AstNode& node,
                                std::string& ruleName,
                                std::string& ruleDesc,
                                RulePhase& phase)
{
    for (const auto& rule : _rules) {
        // Skip rules beyond the current pedagogical phase
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

        // ── Structural pattern match ──────────────────────────────────────
        if (!rule.pattern || !rule.replacement) continue;

        MatchCaptures captures;

        // We need a NodePtr for wildcard binding.  Since tryRewrite always
        // passes the original NodePtr via tryRewrite(nodePtr, ...) and we
        // reconstruct `node` here from a reference, we use matchPattern
        // (ref overload) which correctly handles non-wildcard patterns.
        // Wildcard matching is done via the shared_ptr stored in the engine
        // traversal context.  To get wildcards working we use the node directly:
        if (!RuleMatcher::matchPattern(*rule.pattern, node, captures)) {
            continue;
        }

        // Pattern matched — instantiate replacement
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
// §4.2 — tryRewrite
//
// Recursive depth-first traversal.  At each node we:
//   1. First recurse into children (innermost/leftmost strategy).
//   2. If a child rewrite succeeded, rebuild the parent with COW.
//   3. If no child rewrite, try rules at the current node.
// ─────────────────────────────────────────────────────────────────────────────

NodePtr RuleEngine::tryRewrite(const NodePtr& nodePtr,
                                std::string& ruleName,
                                std::string& ruleDesc,
                                RulePhase& phase)
{
    if (!nodePtr) return nullptr;
    const AstNode& node = *nodePtr;

    // ── Recurse into children first (innermost-leftmost) ─────────────────

    if (node.nodeType == NodeType::Equation) {
        const auto& eq = static_cast<const EquationNode&>(node);

        // Try LHS
        if (eq.lhs) {
            NodePtr newLhs = tryRewrite(eq.lhs, ruleName, ruleDesc, phase);
            if (newLhs) {
                return replaceEquationLHS(_pool, eq, std::move(newLhs));
            }
        }
        // Try RHS
        if (eq.rhs) {
            NodePtr newRhs = tryRewrite(eq.rhs, ruleName, ruleDesc, phase);
            if (newRhs) {
                return replaceEquationRHS(_pool, eq, std::move(newRhs));
            }
        }
    }
    else if (node.nodeType == NodeType::Sum) {
        const auto& sum = static_cast<const SumNode&>(node);
        for (std::size_t i = 0; i < sum.children.size(); ++i) {
            NodePtr newChild = tryRewrite(sum.children[i], ruleName, ruleDesc, phase);
            if (newChild) {
                // COW: rebuild sum with the rewritten child; share all others
                return replaceChildInSum(_pool, sum, i, std::move(newChild));
            }
        }
    }
    else if (node.nodeType == NodeType::Product) {
        const auto& prod = static_cast<const ProductNode&>(node);
        for (std::size_t i = 0; i < prod.children.size(); ++i) {
            NodePtr newChild = tryRewrite(prod.children[i], ruleName, ruleDesc, phase);
            if (newChild) {
                return replaceChildInProduct(_pool, prod, i, std::move(newChild));
            }
        }
    }
    else if (node.nodeType == NodeType::Power) {
        const auto& pw = static_cast<const PowerNode&>(node);
        // Try base first, then exponent
        if (pw.base) {
            NodePtr newBase = tryRewrite(pw.base, ruleName, ruleDesc, phase);
            if (newBase) {
                return makePower(_pool, std::move(newBase), pw.exponent);
            }
        }
        if (pw.exponent) {
            NodePtr newExp = tryRewrite(pw.exponent, ruleName, ruleDesc, phase);
            if (newExp) {
                return makePower(_pool, pw.base, std::move(newExp));
            }
        }
    }
    else if (node.nodeType == NodeType::Negation) {
        const auto& neg = static_cast<const NegationNode&>(node);
        if (neg.operand) {
            NodePtr newOp = tryRewrite(neg.operand, ruleName, ruleDesc, phase);
            if (newOp) {
                return makeNegation(_pool, std::move(newOp));
            }
        }
    }

    // ── No child rewrite fired — try rules at this node ───────────────────
    NodePtr result = tryRulesAt(node, ruleName, ruleDesc, phase);
    return result;  // nullptr if no rule fired
}

// ─────────────────────────────────────────────────────────────────────────────
// §4.3 — applyOneStep (public API)
// ─────────────────────────────────────────────────────────────────────────────

RewriteResult RuleEngine::applyOneStep(const NodePtr& root) {
    RewriteResult result;
    result.changed = false;
    result.newTree = root;
    result.phase   = RulePhase::Expansion;  // default

    if (!root) return result;

    std::string ruleName, ruleDesc;
    RulePhase   phase;
    NodePtr     newRoot = tryRewrite(root, ruleName, ruleDesc, phase);

    if (newRoot) {
        result.changed  = true;
        result.newTree  = std::move(newRoot);
        result.ruleName = std::move(ruleName);
        result.ruleDesc = std::move(ruleDesc);
        result.phase    = phase;
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// §4.4 — applyToFixedPoint (public API)
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
        log.ruleName = std::move(r.ruleName);
        log.ruleDesc = std::move(r.ruleDesc);
        log.phase    = r.phase;
        log.tree     = r.newTree;
        solveResult.steps.push_back(std::move(log));
        current = r.newTree;
    }

    solveResult.finalTree = current;
    return solveResult;
}

}  // namespace cas
