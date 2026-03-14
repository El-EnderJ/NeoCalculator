/**
 * SystemTutor.cpp — Step-by-step 2×2 CAS system orchestrator.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 15 (System Solver)
 */

#include "SystemTutor.h"
#include "AlgebraicRules.h"
#include <cmath>

namespace cas {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

NodePtr SystemTutor::substituteVar(const NodePtr& node, char target,
                                    const NodePtr& replacement,
                                    CasMemoryPool& pool) {
    if (!node) return node;

    switch (node->nodeType) {
        case NodeType::Variable: {
            const auto& v = static_cast<const VariableNode&>(*node);
            if (v.name.size() == 1 && v.name[0] == target)
                return replacement;
            return node;
        }
        case NodeType::Constant:
            return node;

        case NodeType::Negation: {
            const auto& neg = static_cast<const NegationNode&>(*node);
            NodePtr inner = substituteVar(neg.operand, target, replacement, pool);
            if (inner == neg.operand) return node;
            return makeNegation(pool, inner);
        }
        case NodeType::Sum: {
            const auto& sum = static_cast<const SumNode&>(*node);
            NodeList kids;
            kids.reserve(sum.children.size());
            bool changed = false;
            for (const auto& c : sum.children) {
                NodePtr nc = substituteVar(c, target, replacement, pool);
                if (nc != c) changed = true;
                kids.push_back(nc);
            }
            if (!changed) return node;
            return makeSum(pool, std::move(kids));
        }
        case NodeType::Product: {
            const auto& prod = static_cast<const ProductNode&>(*node);
            NodeList kids;
            kids.reserve(prod.children.size());
            bool changed = false;
            for (const auto& c : prod.children) {
                NodePtr nc = substituteVar(c, target, replacement, pool);
                if (nc != c) changed = true;
                kids.push_back(nc);
            }
            if (!changed) return node;
            return makeProduct(pool, std::move(kids));
        }
        case NodeType::Power: {
            const auto& pw = static_cast<const PowerNode&>(*node);
            NodePtr base = substituteVar(pw.base, target, replacement, pool);
            NodePtr exp  = substituteVar(pw.exponent, target, replacement, pool);
            if (base == pw.base && exp == pw.exponent) return node;
            return makePower(pool, base, exp);
        }
        case NodeType::Equation: {
            const auto& eq = static_cast<const EquationNode&>(*node);
            NodePtr lhs = substituteVar(eq.lhs, target, replacement, pool);
            NodePtr rhs = substituteVar(eq.rhs, target, replacement, pool);
            if (lhs == eq.lhs && rhs == eq.rhs) return node;
            return makeEquation(pool, lhs, rhs);
        }
        default:
            return node;
    }
}

NodePtr SystemTutor::getIsolatedRHS(const NodePtr& eq, char var) {
    if (!eq || eq->nodeType != NodeType::Equation) return nullptr;
    const auto& e = static_cast<const EquationNode&>(*eq);
    // Form: var = expr
    if (e.lhs && e.lhs->nodeType == NodeType::Variable) {
        const auto& v = static_cast<const VariableNode&>(*e.lhs);
        if (v.name.size() == 1 && v.name[0] == var) return e.rhs;
    }
    // Form: expr = var
    if (e.rhs && e.rhs->nodeType == NodeType::Variable) {
        const auto& v = static_cast<const VariableNode&>(*e.rhs);
        if (v.name.size() == 1 && v.name[0] == var) return e.lhs;
    }
    return nullptr;
}

void SystemTutor::appendEngineSteps(const RuleEngine::SolveResult& result,
                                     std::vector<SystemTutorStep>& out,
                                     bool isEq1) {
    for (const auto& s : result.steps) {
        SystemTutorStep st;
        st.ruleName = s.ruleName;
        st.ruleDesc = s.ruleDesc;
        if (isEq1) {
            st.eq1Tree = s.tree;
        } else {
            st.eq2Tree = s.tree;
        }
        out.push_back(std::move(st));
    }
}

NodePtr SystemTutor::runEngine(const NodePtr& eq, CasMemoryPool& pool,
                                char var,
                                std::vector<SystemTutorStep>& out) {
    RuleEngine engine(pool);
    for (auto& rule : makeAlgebraicRules(pool, var))
        engine.addRule(std::move(rule));

    auto result = engine.applyToFixedPoint(eq, 80);
    checkNonLinearHandover(result, var);
    appendEngineSteps(result, out, true);
    return result.finalTree;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

SystemTutorResult SystemTutor::solveSystem(const NodePtr& eq1,
                                            const NodePtr& eq2,
                                            CasMemoryPool& pool,
                                            char var1, char var2) {
    if (!eq1 || !eq2) {
        SystemTutorResult r;
        r.error = "Invalid equations";
        return r;
    }

    SystemSolveMethod method = chooseSystemMethod(eq1, eq2, var1, var2);

    switch (method) {
        case SystemSolveMethod::ELIMINATION:
            return byElimination(eq1, eq2, pool, var1, var2);
        case SystemSolveMethod::EQUATING:
            return byEquating(eq1, eq2, pool, var1, var2);
        case SystemSolveMethod::SUBSTITUTION:
        default:
            return bySubstitution(eq1, eq2, pool, var1, var2);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SUBSTITUTION
// ─────────────────────────────────────────────────────────────────────────────

SystemTutorResult SystemTutor::bySubstitution(const NodePtr& eq1,
                                               const NodePtr& eq2,
                                               CasMemoryPool& pool,
                                               char var1, char var2) {
    SystemTutorResult res;
    res.methodUsed = SystemSolveMethod::SUBSTITUTION;

    // ── Step 0: announce method ─────────────────────────────────────────
    {
        SystemTutorStep st;
        st.ruleName = "MethodChoice";
        st.ruleDesc = "Method Chosen: Substitution";
        st.eq1Tree  = eq1;
        st.eq2Tree  = eq2;
        res.steps.push_back(std::move(st));
    }

    // ── Step 1: isolate var2 in eq1 ─────────────────────────────────────
    // Try to use eq1 as-is if already isolated; otherwise run RuleEngine
    NodePtr isolatedExpr = getIsolatedRHS(eq1, var2);
    NodePtr workingEq1 = eq1;

    if (!isolatedExpr) {
        // Run RuleEngine on eq1 with var2 as the target variable
        RuleEngine eng1(pool);
        for (auto& rule : makeAlgebraicRules(pool, var2))
            eng1.addRule(std::move(rule));
        auto r1 = eng1.applyToFixedPoint(eq1, 80);
        appendEngineSteps(r1, res.steps, true);
        workingEq1 = r1.finalTree;
        isolatedExpr = getIsolatedRHS(workingEq1, var2);
    }

    if (!isolatedExpr) {
        res.error = "Could not isolate variable in first equation";
        return res;
    }

    // ── Step 2: substitute into eq2 ─────────────────────────────────────
    NodePtr substituted = substituteVar(eq2, var2, isolatedExpr, pool);
    {
        SystemTutorStep st;
        st.ruleName = "Substitution";
        st.ruleDesc = std::string("Substitute ") + var2 +
                      " into equation 2";
        st.eq1Tree  = workingEq1;
        st.eq2Tree  = substituted;
        res.steps.push_back(std::move(st));
    }

    // ── Step 3: solve substituted eq for var1 ───────────────────────────
    RuleEngine eng2(pool);
    for (auto& rule : makeAlgebraicRules(pool, var1))
        eng2.addRule(std::move(rule));
    auto r2 = eng2.applyToFixedPoint(substituted, 80);
    checkNonLinearHandover(r2, var1);
    appendEngineSteps(r2, res.steps, false);
    NodePtr solvedVar1 = r2.finalTree;

    NodePtr val1 = getIsolatedRHS(solvedVar1, var1);

    // ── Step 4: back-substitute to find var2 ────────────────────────────
    if (val1) {
        NodePtr backSub = substituteVar(workingEq1, var1, val1, pool);
        {
            SystemTutorStep st;
            st.ruleName = "BackSubstitution";
            st.ruleDesc = std::string("Substitute ") + var1 +
                          " back into isolated equation to find " + var2;
            st.eq1Tree  = backSub;
            res.steps.push_back(std::move(st));
        }

        RuleEngine eng3(pool);
        for (auto& rule : makeAlgebraicRules(pool, var2))
            eng3.addRule(std::move(rule));
        auto r3 = eng3.applyToFixedPoint(backSub, 80);
        appendEngineSteps(r3, res.steps, true);
    }

    res.ok = true;
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// ELIMINATION
// ─────────────────────────────────────────────────────────────────────────────

SystemTutorResult SystemTutor::byElimination(const NodePtr& eq1,
                                              const NodePtr& eq2,
                                              CasMemoryPool& pool,
                                              char var1, char var2) {
    SystemTutorResult res;
    res.methodUsed = SystemSolveMethod::ELIMINATION;

    if (eq1->nodeType != NodeType::Equation ||
        eq2->nodeType != NodeType::Equation) {
        res.error = "Not equation nodes";
        return res;
    }

    const auto& e1 = static_cast<const EquationNode&>(*eq1);
    const auto& e2 = static_cast<const EquationNode&>(*eq2);

    // ── Step 0: announce method ─────────────────────────────────────────
    {
        SystemTutorStep st;
        st.ruleName = "MethodChoice";
        st.ruleDesc = "Method Chosen: Elimination (adding equations)";
        st.eq1Tree  = eq1;
        st.eq2Tree  = eq2;
        res.steps.push_back(std::move(st));
    }

    // ── Step 1: form combined equation: LHS1+LHS2 = RHS1+RHS2 ──────────
    NodePtr combinedLhs = makeSum(pool, {e1.lhs, e2.lhs});
    NodePtr combinedRhs = makeSum(pool, {e1.rhs, e2.rhs});
    NodePtr combined    = makeEquation(pool, combinedLhs, combinedRhs);

    {
        SystemTutorStep st;
        st.ruleName = "AddEquations";
        st.ruleDesc = "Add both equations: (LHS1 + LHS2) = (RHS1 + RHS2)";
        st.eq1Tree  = combined;
        res.steps.push_back(std::move(st));
    }

    // ── Step 2: determine which variable was eliminated ──────────────────
    // The surviving variable is the one whose terms didn't cancel.
    // We solve the combined eq first for var1; if that fails, try var2.
    char solveFor = var1;
    {
        // Quick check: if combined doesn't contain var1, solve for var2
        if (!combined->containsVar(var1)) solveFor = var2;
    }

    // ── Step 3: simplify & solve the combined equation ───────────────────
    RuleEngine eng1(pool);
    for (auto& rule : makeAlgebraicRules(pool, solveFor))
        eng1.addRule(std::move(rule));
    auto r1 = eng1.applyToFixedPoint(combined, 80);
    checkNonLinearHandover(r1, solveFor);
    appendEngineSteps(r1, res.steps, true);

    NodePtr solvedFirst = r1.finalTree;
    NodePtr val1 = getIsolatedRHS(solvedFirst, solveFor);

    // ── Step 4: back-substitute into simpler original equation ───────────
    if (val1) {
        char backVar = (solveFor == var1) ? var2 : var1;
        NodePtr backSub = substituteVar(eq1, solveFor, val1, pool);
        {
            SystemTutorStep st;
            st.ruleName = "BackSubstitution";
            st.ruleDesc = std::string("Substitute ") + solveFor +
                          " back into equation 1 to find " + backVar;
            st.eq1Tree  = backSub;
            res.steps.push_back(std::move(st));
        }

        RuleEngine eng2(pool);
        for (auto& rule : makeAlgebraicRules(pool, backVar))
            eng2.addRule(std::move(rule));
        auto r2 = eng2.applyToFixedPoint(backSub, 80);
        appendEngineSteps(r2, res.steps, true);
    }

    res.ok = true;
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// EQUATING
// ─────────────────────────────────────────────────────────────────────────────

SystemTutorResult SystemTutor::byEquating(const NodePtr& eq1,
                                           const NodePtr& eq2,
                                           CasMemoryPool& pool,
                                           char var1, char var2) {
    SystemTutorResult res;
    res.methodUsed = SystemSolveMethod::EQUATING;

    // ── Step 0: announce method ─────────────────────────────────────────
    {
        SystemTutorStep st;
        st.ruleName = "MethodChoice";
        st.ruleDesc = "Method Chosen: Equating (Igualación)";
        st.eq1Tree  = eq1;
        st.eq2Tree  = eq2;
        res.steps.push_back(std::move(st));
    }

    // Find the shared isolated variable and its two RHS expressions
    char isolVar = 0;
    NodePtr rhs1, rhs2;
    for (char v : {var1, var2}) {
        NodePtr r1 = getIsolatedRHS(eq1, v);
        NodePtr r2 = getIsolatedRHS(eq2, v);
        if (r1 && r2) { isolVar = v; rhs1 = r1; rhs2 = r2; break; }
    }

    if (!isolVar) {
        // Fall back to substitution
        return bySubstitution(eq1, eq2, pool, var1, var2);
    }

    // ── Step 1: equate the two RHS expressions ──────────────────────────
    char solveFor = (isolVar == var1) ? var2 : var1;
    NodePtr equated = makeEquation(pool, rhs1, rhs2);
    {
        SystemTutorStep st;
        st.ruleName = "Equate";
        st.ruleDesc = std::string("Equate both expressions for ") + isolVar +
                      " and solve for " + solveFor;
        st.eq1Tree  = equated;
        res.steps.push_back(std::move(st));
    }

    // ── Step 2: solve the equated expression for the other variable ──────
    RuleEngine eng(pool);
    for (auto& rule : makeAlgebraicRules(pool, solveFor))
        eng.addRule(std::move(rule));
    auto r1 = eng.applyToFixedPoint(equated, 80);
    checkNonLinearHandover(r1, solveFor);
    appendEngineSteps(r1, res.steps, true);

    NodePtr solvedVal = getIsolatedRHS(r1.finalTree, solveFor);

    // ── Step 3: back-substitute to find the isolated variable ─────────────
    if (solvedVal) {
        NodePtr backSub = substituteVar(eq1, solveFor, solvedVal, pool);
        {
            SystemTutorStep st;
            st.ruleName = "BackSubstitution";
            st.ruleDesc = std::string("Substitute ") + solveFor +
                          " back to find " + isolVar;
            st.eq1Tree  = backSub;
            res.steps.push_back(std::move(st));
        }

        RuleEngine eng2(pool);
        for (auto& rule : makeAlgebraicRules(pool, isolVar))
            eng2.addRule(std::move(rule));
        auto r2 = eng2.applyToFixedPoint(backSub, 80);
        appendEngineSteps(r2, res.steps, true);
    }

    res.ok = true;
    return res;
}

} // namespace cas
