/**
 * ASTFlattener.cpp — Implementation of MathAST → SymPoly translator.
 *
 * Mirrors the evaluation logic of MathEvaluator::evalRow() but produces
 * symbolic SymPoly polynomials instead of numeric ExactVal scalars.
 *
 * Key differences from MathEvaluator:
 *   · Variables are preserved as symbolic terms (SymTerm with var+power)
 *   · Multiplication of polynomials uses FOIL distribution (SymPoly::mul)
 *   · Transcendental functions (sin, cos, ln, log) are rejected with a flag
 *   · NodeFraction with constant denominator → divScalar distribution
 *
 * Part of: NumOS Pro-CAS — Phase B (AST Bridge)
 */

#include "ASTFlattener.h"
#include <cstdlib>
#include <cstring>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════

ASTFlattener::ASTFlattener() : _var('x') {}

// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flatten(const vpam::MathNode* root) {
    if (!root) return FlattenResult::fail("Null expression");
    return flattenNode(root);
}

EquationFlatResult ASTFlattener::flattenEquation(
    const vpam::MathNode* lhsRoot,
    const vpam::MathNode* rhsRoot)
{
    EquationFlatResult result;

    if (!lhsRoot || !rhsRoot) {
        result.ok    = false;
        result.error = "Null equation side";
        return result;
    }

    auto lf = flattenNode(lhsRoot);
    auto rf = flattenNode(rhsRoot);

    // ── Transcendental path: build SymExpr for both sides ───────────
    if ((lf.transcendental || rf.transcendental) && _arena) {
        result.lhsExpr = lf.exprTree ? lf.exprTree : flattenToExpr(lhsRoot);
        result.rhsExpr = rf.exprTree ? rf.exprTree : flattenToExpr(rhsRoot);

        if (result.lhsExpr && result.rhsExpr) {
            result.ok             = true;
            result.transcendental = true;
        } else {
            result.ok    = false;
            result.error = "Failed to build SymExpr for equation";
        }
        return result;
    }

    // ── Error check ────────────────────────────────────────────────
    if (!lf.ok) {
        result.ok             = false;
        result.error          = "LHS: " + lf.error;
        result.transcendental = lf.transcendental;
        return result;
    }

    if (!rf.ok) {
        result.ok             = false;
        result.error          = "RHS: " + rf.error;
        result.transcendental = rf.transcendental;
        return result;
    }

    // ── Polynomial path (unchanged) ───────────────────────────────
    result.ok = true;
    result.eq = SymEquation(lf.poly, rf.poly);
    return result;
}

// ════════════════════════════════════════════════════════════════════
// Node dispatcher
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenNode(const vpam::MathNode* node) {
    if (!node) return FlattenResult::fail("Null node");

    using vpam::NodeType;

    switch (node->type()) {
        case NodeType::Row:
            return flattenRow(
                static_cast<const vpam::NodeRow*>(node));

        case NodeType::Number:
            return flattenNumber(
                static_cast<const vpam::NodeNumber*>(node));

        case NodeType::Variable:
            return flattenVariable(
                static_cast<const vpam::NodeVariable*>(node));

        case NodeType::Operator:
            // Operators are handled within flattenRow; standalone = error
            return FlattenResult::fail("Unexpected standalone operator");

        case NodeType::Fraction:
            return flattenFraction(
                static_cast<const vpam::NodeFraction*>(node));

        case NodeType::Power:
            return flattenPower(
                static_cast<const vpam::NodePower*>(node));

        case NodeType::Paren:
            return flattenParen(
                static_cast<const vpam::NodeParen*>(node));

        case NodeType::Constant:
            return flattenConstant(
                static_cast<const vpam::NodeConstant*>(node));

        case NodeType::Empty:
            return FlattenResult::fail("Incomplete expression (empty slot)");

        // ── Transcendental nodes: build SymExpr if arena available ──
        case NodeType::Function: {
            if (_arena) {
                SymExpr* expr = flattenFunctionToExpr(
                    static_cast<const vpam::NodeFunction*>(node));
                if (expr) return FlattenResult::transcendentalSuccess(expr);
            }
            return FlattenResult::needsNumeric(
                "Transcendental function (use numeric solver)");
        }

        case NodeType::LogBase: {
            if (_arena) {
                SymExpr* expr = flattenLogBaseToExpr(
                    static_cast<const vpam::NodeLogBase*>(node));
                if (expr) return FlattenResult::transcendentalSuccess(expr);
            }
            return FlattenResult::needsNumeric(
                "Logarithm (use numeric solver)");
        }

        case NodeType::Root: {
            // √(constant) is OK → ExactVal radical
            // √(variable expression) → transcendental
            auto* root = static_cast<const vpam::NodeRoot*>(node);
            return flattenRoot(root);
        }

        case NodeType::PeriodicDecimal:
            return FlattenResult::fail(
                "Periodic decimal not supported in CAS input");

        default:
            return FlattenResult::fail("Unknown node type");
    }
}

// ════════════════════════════════════════════════════════════════════
// flattenRow — Process horizontal sequence with precedence
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenRow(const vpam::NodeRow* row) {
    if (!row || row->isEmpty()) {
        return FlattenResult::fail("Empty row");
    }

    const auto& kids = row->children();
    int n = static_cast<int>(kids.size());

    // Single child → flatten directly
    if (n == 1) {
        return flattenNode(kids[0].get());
    }

    // Parse into terms (SymPoly) and operators (OpKind)
    std::vector<SymPoly> terms;
    std::vector<vpam::OpKind> ops;

    int i = 0;

    // Handle leading unary minus: -expr = negate(first_term)
    if (kids[i]->type() == vpam::NodeType::Operator) {
        auto* opNode = static_cast<const vpam::NodeOperator*>(kids[i].get());
        if (opNode->op() == vpam::OpKind::Sub) {
            i++;
            if (i >= n) return FlattenResult::fail("Unary minus with no operand");
            auto first = flattenNode(kids[i].get());
            if (first.transcendental && _arena) {
                SymExpr* expr = flattenRowToExpr(row);
                if (expr) return FlattenResult::transcendentalSuccess(expr);
                return FlattenResult::fail("SymExpr conversion failed");
            }
            if (!first.ok) return first;
            terms.push_back(first.poly.negate());
            i++;
        } else {
            return FlattenResult::fail("Unexpected leading operator");
        }
    } else {
        // Normal first term
        auto first = flattenNode(kids[i].get());
        if (first.transcendental && _arena) {
            SymExpr* expr = flattenRowToExpr(row);
            if (expr) return FlattenResult::transcendentalSuccess(expr);
            return FlattenResult::fail("SymExpr conversion failed");
        }
        if (!first.ok) return first;
        terms.push_back(first.poly);
        i++;
    }

    // Remaining: alternate operator — term (with implicit multiplication)
    while (i < n) {
        if (kids[i]->type() == vpam::NodeType::Operator) {
            // Explicit operator
            auto* opNode = static_cast<const vpam::NodeOperator*>(kids[i].get());
            ops.push_back(opNode->op());
            i++;

            if (i >= n) return FlattenResult::fail("Operator with no right operand");

            auto term = flattenNode(kids[i].get());
            if (term.transcendental && _arena) {
                SymExpr* expr = flattenRowToExpr(row);
                if (expr) return FlattenResult::transcendentalSuccess(expr);
                return FlattenResult::fail("SymExpr conversion failed");
            }
            if (!term.ok) return term;
            terms.push_back(term.poly);
            i++;
        } else {
            // Implicit multiplication (adjacent non-operator nodes)
            // e.g., 3x, 2(x+1), (x+1)(x-2)
            ops.push_back(vpam::OpKind::Mul);

            auto term = flattenNode(kids[i].get());
            if (term.transcendental && _arena) {
                SymExpr* expr = flattenRowToExpr(row);
                if (expr) return FlattenResult::transcendentalSuccess(expr);
                return FlattenResult::fail("SymExpr conversion failed");
            }
            if (!term.ok) return term;
            terms.push_back(term.poly);
            i++;
        }
    }

    // Apply operator precedence: × before +/−
    return applyPrecedence(terms, ops);
}

// ════════════════════════════════════════════════════════════════════
// applyPrecedence — Mul > {Add, Sub}
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::applyPrecedence(
    const std::vector<SymPoly>& terms,
    const std::vector<vpam::OpKind>& ops)
{
    if (terms.empty()) return FlattenResult::fail("No terms");
    if (terms.size() == 1) return FlattenResult::success(terms[0]);

    // Pass 1: Collapse multiplications (higher precedence)
    std::vector<SymPoly> addTerms;
    std::vector<vpam::OpKind> addOps;

    SymPoly current = terms[0];
    for (size_t i = 0; i < ops.size(); ++i) {
        if (ops[i] == vpam::OpKind::Mul) {
            // Polynomial × Polynomial (FOIL distribution)
            current = current.mul(terms[i + 1]);
        } else {
            addTerms.push_back(current);
            addOps.push_back(ops[i]);
            current = terms[i + 1];
        }
    }
    addTerms.push_back(current);

    // Pass 2: Collapse additions and subtractions (left to right)
    SymPoly result = addTerms[0];
    for (size_t i = 0; i < addOps.size(); ++i) {
        if (addOps[i] == vpam::OpKind::Add) {
            result = result.add(addTerms[i + 1]);
        } else { // Sub
            result = result.sub(addTerms[i + 1]);
        }
    }

    return FlattenResult::success(result);
}

// ════════════════════════════════════════════════════════════════════
// flattenNumber — Numeric literal → constant SymPoly
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenNumber(const vpam::NodeNumber* node) {
    if (!node || node->value().empty()) {
        return FlattenResult::fail("Empty number");
    }

    vpam::ExactVal val = parseNumberString(node->value());
    if (!val.ok) {
        return FlattenResult::fail("Invalid number: " + node->value());
    }

    return FlattenResult::success(
        SymPoly::fromConstant(val));
}

// ════════════════════════════════════════════════════════════════════
// flattenVariable — Variable node → single-term SymPoly
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenVariable(const vpam::NodeVariable* node) {
    if (!node) return FlattenResult::fail("Null variable");

    char name = node->name();

    // Ans and PreAns are not algebraic variables
    if (name == '#' || name == '$') {
        return FlattenResult::fail("Ans/PreAns cannot be used in equations");
    }

    // Update flattener's primary variable if this is the first variable seen
    // For single-variable polynomials, all variables should match
    SymTerm t = SymTerm::variable(name, 1, 1, 1);  // 1·var^1

    SymPoly p(name);
    p.terms().push_back(t);
    p.normalize();
    return FlattenResult::success(p);
}

// ════════════════════════════════════════════════════════════════════
// flattenFraction — Fraction node → polynomial division
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenFraction(const vpam::NodeFraction* node) {
    if (!node) return FlattenResult::fail("Null fraction");

    auto numResult = flattenNode(node->numerator());
    // If numerator is transcendental, build SymExpr for entire fraction
    if (numResult.transcendental && _arena) {
        SymExpr* expr = flattenFractionToExpr(node);
        if (expr) return FlattenResult::transcendentalSuccess(expr);
        return FlattenResult::fail("SymExpr fraction conversion failed");
    }
    if (!numResult.ok) return numResult;

    auto denResult = flattenNode(node->denominator());
    // If denominator is transcendental, build SymExpr for entire fraction
    if (denResult.transcendental && _arena) {
        SymExpr* expr = flattenFractionToExpr(node);
        if (expr) return FlattenResult::transcendentalSuccess(expr);
        return FlattenResult::fail("SymExpr fraction conversion failed");
    }
    if (!denResult.ok) return denResult;

    const SymPoly& num = numResult.poly;
    const SymPoly& den = denResult.poly;

    // Case 1: Denominator is a constant → distribute division
    if (den.isConstant()) {
        vpam::ExactVal denVal = den.coeffAt(0);
        if (denVal.isZero()) {
            return FlattenResult::fail("Division by zero");
        }
        return FlattenResult::success(num.divScalar(denVal));
    }

    // Case 2: Denominator is a single monomial (e.g., just 'x' or '2x')
    // Convert to negative power: p(x) / (c·x^n) = (1/c)·p(x)·x^(-n)
    if (den.terms().size() == 1 && !den.terms()[0].isConstant()) {
        const SymTerm& denTerm = den.terms()[0];
        // Create the reciprocal: (1/coeff) · var^(-power)
        vpam::ExactVal recipCoeff = vpam::exactDiv(
            vpam::ExactVal::fromInt(1), denTerm.coeff);

        // Multiply numerator by (1/coeff)
        SymPoly scaled = num.mulScalar(recipCoeff);

        // Now multiply each term's power by -denTerm.power
        // This is equivalent to multiplying by var^(-denTerm.power)
        SymPoly recipVar(denTerm.var);
        recipVar.terms().push_back(
            SymTerm(vpam::ExactVal::fromInt(1), denTerm.var,
                    static_cast<int16_t>(-denTerm.power)));
        recipVar.normalize();

        return FlattenResult::success(scaled.mul(recipVar));
    }

    // Case 3: Complex variable denominator → build SymExpr if arena available
    if (_arena) {
        SymExpr* expr = flattenFractionToExpr(node);
        if (expr) return FlattenResult::transcendentalSuccess(expr);
    }
    return FlattenResult::needsNumeric(
        "Rational expression with variable denominator");
}

// ════════════════════════════════════════════════════════════════════
// flattenPower — Power node → polynomial if integer exponent
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenPower(const vpam::NodePower* node) {
    if (!node) return FlattenResult::fail("Null power");

    auto baseResult = flattenNode(node->base());
    // If base is transcendental but we have arena, build SymExpr for whole power
    if (baseResult.transcendental && _arena) {
        SymExpr* expr = flattenPowerToExpr(node);
        if (expr) return FlattenResult::transcendentalSuccess(expr);
        return FlattenResult::fail("SymExpr power conversion failed");
    }
    if (!baseResult.ok) return baseResult;

    auto expResult = flattenNode(node->exponent());
    // If exponent is transcendental, build SymExpr
    if (expResult.transcendental && _arena) {
        SymExpr* expr = flattenPowerToExpr(node);
        if (expr) return FlattenResult::transcendentalSuccess(expr);
        return FlattenResult::fail("SymExpr power conversion failed");
    }
    if (!expResult.ok) return expResult;

    const SymPoly& basePoly = baseResult.poly;
    const SymPoly& expPoly  = expResult.poly;

    // Exponent must be a constant integer for polynomial representation
    if (!expPoly.isConstant()) {
        if (_arena) {
            SymExpr* expr = flattenPowerToExpr(node);
            if (expr) return FlattenResult::transcendentalSuccess(expr);
        }
        return FlattenResult::needsNumeric(
            "Variable exponent (not a polynomial)");
    }

    vpam::ExactVal expVal = expPoly.coeffAt(0);

    // Must be a rational number with den=1 (integer)
    if (expVal.den != 1 || expVal.hasRadical() || expVal.hasConstant()) {
        if (_arena) {
            SymExpr* expr = flattenPowerToExpr(node);
            if (expr) return FlattenResult::transcendentalSuccess(expr);
        }
        // Fractional exponent (e.g., x^(1/2) = √x) → transcendental
        return FlattenResult::needsNumeric(
            "Fractional/irrational exponent");
    }

    int64_t exp = expVal.num;

    // Sanity: limit exponent range to prevent combinatorial explosion
    if (exp > 10 || exp < -10) {
        return FlattenResult::fail("Exponent too large for symbolic expansion");
    }

    // Handle base = single constant → compute ExactVal power directly
    if (basePoly.isConstant()) {
        vpam::ExactVal baseVal = basePoly.coeffAt(0);
        vpam::ExactVal result = vpam::exactPow(baseVal, expVal);
        if (!result.ok) {
            return FlattenResult::fail("Power computation error");
        }
        return FlattenResult::success(SymPoly::fromConstant(result));
    }

    // Handle negative exponent on polynomial → not directly polynomial
    if (exp < 0) {
        // Single-term base with negative exp → adjust power
        if (basePoly.terms().size() == 1) {
            const SymTerm& bt = basePoly.terms()[0];
            SymTerm result;
            result.coeff = vpam::exactPow(bt.coeff, expVal);
            result.var   = bt.var;
            result.power = static_cast<int16_t>(bt.power * exp);
            result.simplifyCoeff();
            return FlattenResult::success(SymPoly::fromTerm(result));
        }
        if (_arena) {
            SymExpr* expr = flattenPowerToExpr(node);
            if (expr) return FlattenResult::transcendentalSuccess(expr);
        }
        return FlattenResult::needsNumeric(
            "Negative exponent on multi-term polynomial");
    }

    // exp == 0 → 1
    if (exp == 0) {
        return FlattenResult::success(
            SymPoly::fromConstant(vpam::ExactVal::fromInt(1)));
    }

    // exp == 1 → base itself
    if (exp == 1) {
        return FlattenResult::success(basePoly);
    }

    // Positive integer exponent: repeated multiplication
    // (a + b)^n = (a + b) × (a + b) × ... (n times)
    SymPoly result = basePoly;
    for (int64_t k = 1; k < exp; ++k) {
        result = result.mul(basePoly);
    }

    return FlattenResult::success(result);
}

// ════════════════════════════════════════════════════════════════════
// flattenRoot — Radical node
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenRoot(const vpam::NodeRoot* node) {
    if (!node) return FlattenResult::fail("Null root");

    // Flatten the radicand
    auto radResult = flattenNode(node->radicand());
    // If radicand is transcendental, build SymExpr for entire root
    if (radResult.transcendental && _arena) {
        SymExpr* expr = flattenRootToExpr(node);
        if (expr) return FlattenResult::transcendentalSuccess(expr);
        return FlattenResult::fail("SymExpr root conversion failed");
    }
    if (!radResult.ok) return radResult;

    const SymPoly& radPoly = radResult.poly;

    // Only constants under √ can be represented as ExactVal radical
    if (!radPoly.isConstant()) {
        if (_arena) {
            SymExpr* expr = flattenRootToExpr(node);
            if (expr) return FlattenResult::transcendentalSuccess(expr);
        }
        return FlattenResult::needsNumeric(
            "Square root of variable expression");
    }

    vpam::ExactVal radVal = radPoly.coeffAt(0);

    // For now, only handle square root (no degree or degree=2)
    if (node->hasDegree()) {
        // Check if degree is 2 (square root)
        auto degResult = flattenNode(node->degree());
        if (!degResult.ok || !degResult.poly.isConstant()) {
            if (_arena) {
                SymExpr* expr = flattenRootToExpr(node);
                if (expr) return FlattenResult::transcendentalSuccess(expr);
            }
            return FlattenResult::needsNumeric(
                "Non-constant root degree");
        }
        vpam::ExactVal degVal = degResult.poly.coeffAt(0);
        if (degVal.num != 2 || degVal.den != 1) {
            if (_arena) {
                SymExpr* expr = flattenRootToExpr(node);
                if (expr) return FlattenResult::transcendentalSuccess(expr);
            }
            return FlattenResult::needsNumeric(
                "Nth root (n≠2) of constant");
        }
    }

    // Compute √(constant)
    vpam::ExactVal result = vpam::exactSqrt(radVal);
    if (!result.ok) {
        return FlattenResult::fail("Square root error");
    }

    return FlattenResult::success(SymPoly::fromConstant(result));
}

// ════════════════════════════════════════════════════════════════════
// flattenParen — Parenthesized expression → recurse
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenParen(const vpam::NodeParen* node) {
    if (!node) return FlattenResult::fail("Null paren");
    return flattenNode(node->content());
}

// ════════════════════════════════════════════════════════════════════
// flattenConstant — π, e → ExactVal with piMul/eMul
// ════════════════════════════════════════════════════════════════════

FlattenResult ASTFlattener::flattenConstant(const vpam::NodeConstant* node) {
    if (!node) return FlattenResult::fail("Null constant");

    vpam::ExactVal val;

    switch (node->constKind()) {
        case vpam::ConstKind::Pi:
            val = vpam::ExactVal::fromPi(1, 1, 1);  // 1 × π
            break;
        case vpam::ConstKind::E:
            val = vpam::ExactVal::fromE(1, 1, 1);    // 1 × e
            break;
        default:
            return FlattenResult::fail("Unknown constant kind");
    }

    return FlattenResult::success(SymPoly::fromConstant(val));
}

// ════════════════════════════════════════════════════════════════════
// parseNumberString — "42" → ExactVal(42/1), "3.14" → ExactVal(157/50)
// ════════════════════════════════════════════════════════════════════

vpam::ExactVal ASTFlattener::parseNumberString(const std::string& str) {
    if (str.empty()) return vpam::ExactVal::makeError("Empty number");

    auto dotPos = str.find('.');
    if (dotPos == std::string::npos) {
        // Integer
        int64_t n = 0;
        bool negative = false;
        size_t start = 0;
        if (str[0] == '-') { negative = true; start = 1; }
        for (size_t i = start; i < str.size(); ++i) {
            if (str[i] < '0' || str[i] > '9') {
                return vpam::ExactVal::makeError("Invalid digit");
            }
            n = n * 10 + (str[i] - '0');
        }
        return vpam::ExactVal::fromInt(negative ? -n : n);
    }

    // Decimal: "3.14" → 314 / 100 → simplify → 157/50
    std::string intPart = str.substr(0, dotPos);
    std::string decPart = str.substr(dotPos + 1);

    if (decPart.empty()) {
        // "3." → treat as "3"
        int64_t n = 0;
        bool negative = false;
        size_t start = 0;
        if (!intPart.empty() && intPart[0] == '-') { negative = true; start = 1; }
        for (size_t i = start; i < intPart.size(); ++i) {
            n = n * 10 + (intPart[i] - '0');
        }
        return vpam::ExactVal::fromInt(negative ? -n : n);
    }

    int64_t denominator = 1;
    for (size_t i = 0; i < decPart.size(); ++i) {
        denominator *= 10;
    }

    int64_t whole = 0;
    bool negative = false;
    if (!intPart.empty()) {
        size_t s = 0;
        if (intPart[0] == '-') { negative = true; s = 1; }
        for (size_t i = s; i < intPart.size(); ++i) {
            whole = whole * 10 + (intPart[i] - '0');
        }
    }

    int64_t decVal = 0;
    for (size_t i = 0; i < decPart.size(); ++i) {
        if (decPart[i] < '0' || decPart[i] > '9') {
            return vpam::ExactVal::makeError("Invalid decimal digit");
        }
        decVal = decVal * 10 + (decPart[i] - '0');
    }

    int64_t numerator = whole * denominator + decVal;
    if (negative) numerator = -numerator;

    auto result = vpam::ExactVal::fromFrac(numerator, denominator);
    result.simplify();  // GCD reduction: 314/100 → 157/50
    return result;
}

// ════════════════════════════════════════════════════════════════════
//  SymExpr path — Dual-mode methods (Phase 2)
// ════════════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────────────
// flattenToExpr — Master dispatcher: MathNode → SymExpr*
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenToExpr(const vpam::MathNode* node) {
    if (!node || !_arena) return nullptr;

    using vpam::NodeType;
    switch (node->type()) {
        case NodeType::Row:
            return flattenRowToExpr(
                static_cast<const vpam::NodeRow*>(node));

        case NodeType::Number: {
            auto* num = static_cast<const vpam::NodeNumber*>(node);
            if (num->value().empty()) return nullptr;
            vpam::ExactVal val = parseNumberString(num->value());
            if (!val.ok) return nullptr;
            return symNum(*_arena, val);
        }

        case NodeType::Variable: {
            auto* var = static_cast<const vpam::NodeVariable*>(node);
            char name = var->name();
            if (name == '#' || name == '$') return nullptr;  // Ans/PreAns
            return symVar(*_arena, name);
        }

        case NodeType::Operator:
            return nullptr;  // Standalone operator = error

        case NodeType::Fraction:
            return flattenFractionToExpr(
                static_cast<const vpam::NodeFraction*>(node));

        case NodeType::Power:
            return flattenPowerToExpr(
                static_cast<const vpam::NodePower*>(node));

        case NodeType::Root:
            return flattenRootToExpr(
                static_cast<const vpam::NodeRoot*>(node));

        case NodeType::Paren: {
            auto* paren = static_cast<const vpam::NodeParen*>(node);
            return flattenToExpr(paren->content());
        }

        case NodeType::Constant: {
            auto* c = static_cast<const vpam::NodeConstant*>(node);
            vpam::ExactVal val;
            if (c->constKind() == vpam::ConstKind::Pi)
                val = vpam::ExactVal::fromPi(1, 1, 1);
            else
                val = vpam::ExactVal::fromE(1, 1, 1);
            return symNum(*_arena, val);
        }

        case NodeType::Function:
            return flattenFunctionToExpr(
                static_cast<const vpam::NodeFunction*>(node));

        case NodeType::LogBase:
            return flattenLogBaseToExpr(
                static_cast<const vpam::NodeLogBase*>(node));

        case NodeType::Empty:
        case NodeType::PeriodicDecimal:
        default:
            return nullptr;
    }
}

// ────────────────────────────────────────────────────────────────────
// flattenRowToExpr — Row with precedence → SymExpr*
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenRowToExpr(const vpam::NodeRow* row) {
    if (!row || row->isEmpty() || !_arena) return nullptr;

    const auto& kids = row->children();
    int n = static_cast<int>(kids.size());

    if (n == 1) return flattenToExpr(kids[0].get());

    // Parse into terms and operators
    std::vector<SymExpr*> terms;
    std::vector<vpam::OpKind> ops;
    int i = 0;

    // Handle leading unary minus
    bool leadingNeg = false;
    if (kids[i]->type() == vpam::NodeType::Operator) {
        auto* opNode = static_cast<const vpam::NodeOperator*>(kids[i].get());
        if (opNode->op() == vpam::OpKind::Sub) {
            leadingNeg = true;
            i++;
        } else {
            return nullptr;
        }
    }

    if (i >= n) return nullptr;

    SymExpr* first = flattenToExpr(kids[i].get());
    if (!first) return nullptr;
    if (leadingNeg) first = symNeg(*_arena, first);
    terms.push_back(first);
    i++;

    while (i < n) {
        if (kids[i]->type() == vpam::NodeType::Operator) {
            auto* opNode = static_cast<const vpam::NodeOperator*>(kids[i].get());
            ops.push_back(opNode->op());
            i++;
            if (i >= n) return nullptr;
        } else {
            // Implicit multiplication
            ops.push_back(vpam::OpKind::Mul);
        }

        SymExpr* term = flattenToExpr(kids[i].get());
        if (!term) return nullptr;
        terms.push_back(term);
        i++;
    }

    // ── Pass 1: Collapse multiplications (higher precedence) ────
    std::vector<SymExpr*> addTerms;
    std::vector<vpam::OpKind> addOps;

    SymExpr* current = terms[0];
    for (size_t j = 0; j < ops.size(); ++j) {
        if (ops[j] == vpam::OpKind::Mul) {
            current = symMul(*_arena, current, terms[j + 1]);
        } else {
            addTerms.push_back(current);
            addOps.push_back(ops[j]);
            current = terms[j + 1];
        }
    }
    addTerms.push_back(current);

    // ── Pass 2: Collapse additions/subtractions ─────────────────
    if (addTerms.size() == 1) return addTerms[0];

    // Collect all terms, wrapping subtracted ones in SymNeg
    std::vector<SymExpr*> finalTerms;
    finalTerms.push_back(addTerms[0]);
    for (size_t j = 0; j < addOps.size(); ++j) {
        if (addOps[j] == vpam::OpKind::Sub) {
            finalTerms.push_back(symNeg(*_arena, addTerms[j + 1]));
        } else {
            finalTerms.push_back(addTerms[j + 1]);
        }
    }

    // Build n-ary SymAdd
    uint16_t cnt = static_cast<uint16_t>(finalTerms.size());
    auto** arr = static_cast<SymExpr**>(
        _arena->allocRaw(cnt * sizeof(SymExpr*)));
    if (!arr) return nullptr;
    for (uint16_t k = 0; k < cnt; ++k) arr[k] = finalTerms[k];
    return _arena->create<SymAdd>(arr, cnt);
}

// ────────────────────────────────────────────────────────────────────
// flattenFunctionToExpr — NodeFunction → SymFunc
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenFunctionToExpr(
    const vpam::NodeFunction* node)
{
    if (!node || !_arena) return nullptr;

    SymExpr* arg = flattenToExpr(node->argument());
    if (!arg) return nullptr;

    // Map vpam::FuncKind → cas::SymFuncKind
    SymFuncKind kind;
    switch (node->funcKind()) {
        case vpam::FuncKind::Sin:    kind = SymFuncKind::Sin;    break;
        case vpam::FuncKind::Cos:    kind = SymFuncKind::Cos;    break;
        case vpam::FuncKind::Tan:    kind = SymFuncKind::Tan;    break;
        case vpam::FuncKind::ArcSin: kind = SymFuncKind::ArcSin; break;
        case vpam::FuncKind::ArcCos: kind = SymFuncKind::ArcCos; break;
        case vpam::FuncKind::ArcTan: kind = SymFuncKind::ArcTan; break;
        case vpam::FuncKind::Ln:     kind = SymFuncKind::Ln;     break;
        case vpam::FuncKind::Log:    kind = SymFuncKind::Log10;  break;
        default: return nullptr;
    }

    return symFunc(*_arena, kind, arg);
}

// ────────────────────────────────────────────────────────────────────
// flattenLogBaseToExpr — NodeLogBase → ln(arg) / ln(base)
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenLogBaseToExpr(
    const vpam::NodeLogBase* node)
{
    if (!node || !_arena) return nullptr;

    SymExpr* arg = flattenToExpr(node->argument());
    if (!arg) return nullptr;

    SymExpr* base = flattenToExpr(node->base());
    if (!base) return nullptr;

    // log_b(x) = ln(x) / ln(b) = ln(x) · ln(b)^(-1)
    SymExpr* lnArg  = symFunc(*_arena, SymFuncKind::Ln, arg);
    SymExpr* lnBase = symFunc(*_arena, SymFuncKind::Ln, base);
    SymExpr* invLnBase = symPow(*_arena, lnBase, symInt(*_arena, -1));

    return symMul(*_arena, lnArg, invLnBase);
}

// ────────────────────────────────────────────────────────────────────
// flattenRootToExpr — NodeRoot → SymPow(radicand, 1/n)
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenRootToExpr(const vpam::NodeRoot* node) {
    if (!node || !_arena) return nullptr;

    SymExpr* rad = flattenToExpr(node->radicand());
    if (!rad) return nullptr;

    SymExpr* exp;
    if (node->hasDegree()) {
        SymExpr* deg = flattenToExpr(node->degree());
        if (!deg) return nullptr;
        // x^(1/n) = x^(n^(-1))
        exp = symPow(*_arena, deg, symInt(*_arena, -1));
    } else {
        // Square root: x^(1/2)
        exp = symFrac(*_arena, 1, 2);
    }

    return symPow(*_arena, rad, exp);
}

// ────────────────────────────────────────────────────────────────────
// flattenPowerToExpr — NodePower → SymPow(base, exp)
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenPowerToExpr(const vpam::NodePower* node) {
    if (!node || !_arena) return nullptr;

    SymExpr* base = flattenToExpr(node->base());
    if (!base) return nullptr;

    SymExpr* exp = flattenToExpr(node->exponent());
    if (!exp) return nullptr;

    return symPow(*_arena, base, exp);
}

// ────────────────────────────────────────────────────────────────────
// flattenFractionToExpr — NodeFraction → SymMul(num, SymPow(den, -1))
// ────────────────────────────────────────────────────────────────────

SymExpr* ASTFlattener::flattenFractionToExpr(
    const vpam::NodeFraction* node)
{
    if (!node || !_arena) return nullptr;

    SymExpr* num = flattenToExpr(node->numerator());
    if (!num) return nullptr;

    SymExpr* den = flattenToExpr(node->denominator());
    if (!den) return nullptr;

    // a/b = a · b^(-1)
    SymExpr* invDen = symPow(*_arena, den, symInt(*_arena, -1));
    return symMul(*_arena, num, invDen);
}

} // namespace cas
