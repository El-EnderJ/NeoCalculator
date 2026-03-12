/**
 * NeoInterpreter.cpp — AST evaluation engine for NeoLanguage.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 2 (Interpreter)
 */

#include "NeoInterpreter.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
#include "NeoScientific.h"

#ifdef ARDUINO
#  include <Arduino.h>
#else
#  include <chrono>
#endif

// Maximum iterations for while loops and for-in ranges.
// Guards against infinite loops on the embedded target.
static constexpr int NEO_MAX_ITER = 10000;

// ════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════

NeoInterpreter::NeoInterpreter(cas::SymExprArena& symArena)
    : _symArena(symArena)
    , _hasError(false)
    , _lastError()
    , _errorCount(0)
    , _returnFlag(false)
    , _returnValue()
{}

// ════════════════════════════════════════════════════════════════════
// Error helpers
// ════════════════════════════════════════════════════════════════════

void NeoInterpreter::recordError(const std::string& msg, int line, int col) {
    _hasError = true;
    ++_errorCount;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[line %d:%d] %s", line, col, msg.c_str());
    _lastError = std::string(buf);
}

void NeoInterpreter::clearErrors() {
    _hasError    = false;
    _lastError.clear();
    _errorCount  = 0;
    _returnFlag  = false;
    _returnValue = NeoValue();
}

// ════════════════════════════════════════════════════════════════════
// takeReturn
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::takeReturn() {
    _returnFlag = false;
    NeoValue v  = _returnValue;
    _returnValue = NeoValue();
    return v;
}

// ════════════════════════════════════════════════════════════════════
// eval — dispatch by node kind
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::eval(NeoNode* node, NeoEnv& env) {
    if (!node) return NeoValue::makeNull();

    switch (node->kind) {
        case NodeKind::Program:
            return evalProgram(static_cast<ProgramNode*>(node), env);
        case NodeKind::Number:
            return evalNumber(static_cast<NumberNode*>(node));
        case NodeKind::String:
            return evalString(static_cast<StringNode*>(node));
        case NodeKind::Symbol:
            return evalSymbol(static_cast<SymbolNode*>(node), env);
        case NodeKind::BinaryOp:
            return evalBinaryOp(static_cast<BinaryOpNode*>(node), env);
        case NodeKind::UnaryOp:
            return evalUnaryOp(static_cast<UnaryOpNode*>(node), env);
        case NodeKind::FunctionCall:
            return evalFunctionCall(static_cast<FunctionCallNode*>(node), env);
        case NodeKind::Assignment:
            return evalAssignment(static_cast<AssignmentNode*>(node), env);
        case NodeKind::If:
            return evalIf(static_cast<IfNode*>(node), env);
        case NodeKind::While:
            return evalWhile(static_cast<WhileNode*>(node), env);
        case NodeKind::ForIn:
            return evalForIn(static_cast<ForInNode*>(node), env);
        case NodeKind::FunctionDef:
            return evalFunctionDef(static_cast<FunctionDefNode*>(node), env);
        case NodeKind::Return:
            return evalReturn(static_cast<ReturnNode*>(node), env);
        case NodeKind::IndexOp:
            return evalIndexOp(static_cast<IndexOpNode*>(node), env);
        case NodeKind::DictLiteral:
            return evalDictLiteral(static_cast<DictLiteralNode*>(node), env);
        case NodeKind::TryExcept:
            return evalTryExcept(static_cast<TryExceptNode*>(node), env);
        case NodeKind::Import:
            return evalImport(static_cast<ImportNode*>(node), env);
        case NodeKind::SymExprWrapper: {
            // Already-computed symbolic node — return it as-is
            auto* w = static_cast<SymExprWrapperNode*>(node);
            auto* sym = static_cast<cas::SymExpr*>(w->symexpr_ptr);
            if (sym) return NeoValue::makeSymbolic(sym);
            return NeoValue::makeNull();
        }
        default:
            recordError("Unknown AST node kind", node->line, node->col);
            return NeoValue::makeNull();
    }
}

// ════════════════════════════════════════════════════════════════════
// evalProgram
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalProgram(ProgramNode* node, NeoEnv& env) {
    return evalBlock(node->statements, env);
}

// ════════════════════════════════════════════════════════════════════
// evalBlock — run statements, stop on return
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalBlock(const NeoNodeVec& stmts, NeoEnv& env) {
    NeoValue last = NeoValue::makeNull();
    for (NeoNode* stmt : stmts) {
        last = eval(stmt, env);
        if (_returnFlag) return last;
    }
    return last;
}

// ════════════════════════════════════════════════════════════════════
// evalNumber
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalNumber(NumberNode* node) {
    // Prefer exact rational when available
    if (node->den != 0) {
        return NeoValue::makeExact(cas::CASRational(node->num, node->den));
    }
    // Otherwise use double
    return NeoValue::makeNumber(node->value);
}

// ════════════════════════════════════════════════════════════════════
// evalString — string literal (Phase 6)
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalString(StringNode* node) {
    return NeoValue::makeString(node->value);
}

// ════════════════════════════════════════════════════════════════════
// evalSymbol — look up variable, or return a symbolic SymExpr
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalSymbol(SymbolNode* node, NeoEnv& env) {
    // Check for well-known constants first
    const std::string& name = node->name;
    if (name == "pi" || name == "Pi" || name == "PI") {
        return NeoValue::makeNumber(M_PI);
    }
    if (name == "e" || name == "E") {
        return NeoValue::makeNumber(M_E);
    }
    if (name == "phi" || name == "PHI" || name == "golden") {
        // Golden ratio φ = (1 + √5) / 2
        return NeoValue::makeNumber(1.6180339887498948482);
    }
    if (name == "inf"   || name == "Inf" || name == "Infinity")
        return NeoValue::makeNumber(std::numeric_limits<double>::infinity());
    if (name == "True"  || name == "true")  return NeoValue::makeBool(true);
    if (name == "False" || name == "false") return NeoValue::makeBool(false);
    if (name == "None"  || name == "none")  return NeoValue::makeNull();

    // Look up in environment
    const NeoValue* val = env.lookup(name);
    if (val) return *val;

    // Undefined variable → Wolfram-style: return as a symbolic SymVar.
    // NOTE: SymVar stores a single char. Single-letter names (x, y, z)
    // map perfectly. Multi-char names use only the first character; this
    // is a known limitation — 'alpha' and 'apple' both become 'a'.
    // Future work: add a multi-char SymNamedVar node to the CAS.
    char varChar = name.empty() ? 'x' : name[0];
    cas::SymExpr* symv = cas::symVar(_symArena, varChar);
    return NeoValue::makeSymbolic(symv);
}

// ════════════════════════════════════════════════════════════════════
// evalBinaryOp
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalBinaryOp(BinaryOpNode* node, NeoEnv& env) {
    // Short-circuit logical operators
    if (node->op == BinaryOpNode::OpKind::And) {
        NeoValue lv = eval(node->left, env);
        if (!lv.isTruthy()) return lv;
        return eval(node->right, env);
    }
    if (node->op == BinaryOpNode::OpKind::Or) {
        NeoValue lv = eval(node->left, env);
        if (lv.isTruthy()) return lv;
        return eval(node->right, env);
    }

    NeoValue lv = eval(node->left,  env);
    if (_returnFlag) return lv;
    NeoValue rv = eval(node->right, env);
    if (_returnFlag) return rv;

    switch (node->op) {
        case BinaryOpNode::OpKind::Add: return lv.add(rv, _symArena);
        case BinaryOpNode::OpKind::Sub: return lv.sub(rv, _symArena);
        case BinaryOpNode::OpKind::Mul: return lv.mul(rv, _symArena);
        case BinaryOpNode::OpKind::Div: return lv.div(rv, _symArena);
        case BinaryOpNode::OpKind::Pow: return lv.pow(rv, _symArena);
        case BinaryOpNode::OpKind::Eq:
            // When at least one side is symbolic, return lhs-rhs as a symbolic
            // residual (used by solve() for multi-variable system detection).
            if (lv.isSymbolic() || rv.isSymbolic())
                return lv.sub(rv, _symArena);
            return lv.opEq(rv);
        case BinaryOpNode::OpKind::Ne:  return lv.opNe(rv);
        case BinaryOpNode::OpKind::Lt:  return lv.opLt(rv);
        case BinaryOpNode::OpKind::Le:  return lv.opLe(rv);
        case BinaryOpNode::OpKind::Gt:  return lv.opGt(rv);
        case BinaryOpNode::OpKind::Ge:  return lv.opGe(rv);
        // ── Bitwise operators (Phase 6) ───────────────────────────
        case BinaryOpNode::OpKind::BitAnd: return lv.bitwiseAnd(rv);
        case BinaryOpNode::OpKind::BitOr:  return lv.bitwiseOr(rv);
        case BinaryOpNode::OpKind::BitXor: return lv.bitwiseXor(rv);
        case BinaryOpNode::OpKind::LShift: return lv.leftShift(rv);
        case BinaryOpNode::OpKind::RShift: return lv.rightShift(rv);
        default:
            recordError("Unknown binary operator", node->line, node->col);
            return NeoValue::makeNull();
    }
}

// ════════════════════════════════════════════════════════════════════
// evalUnaryOp
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalUnaryOp(UnaryOpNode* node, NeoEnv& env) {
    NeoValue v = eval(node->operand, env);
    if (_returnFlag) return v;

    switch (node->op) {
        case UnaryOpNode::OpKind::Neg:
            return v.neg(_symArena);
        case UnaryOpNode::OpKind::Not:
            return NeoValue::makeBool(!v.isTruthy());
        case UnaryOpNode::OpKind::BitNot:
            return v.bitwiseNot();
        default:
            recordError("Unknown unary operator", node->line, node->col);
            return NeoValue::makeNull();
    }
}

// ════════════════════════════════════════════════════════════════════
// evalFunctionCall
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalFunctionCall(FunctionCallNode* node, NeoEnv& env) {
    // Evaluate arguments
    std::vector<NeoValue> args;
    args.reserve(node->args.size());
    for (NeoNode* argNode : node->args) {
        args.push_back(eval(argNode, env));
        if (_returnFlag) return NeoValue::makeNull();
    }

    // Try built-in functions first
    NeoValue result;
    if (evalBuiltin(node->name, args, result, env)) return result;

    // Look up user-defined function in environment
    const NeoValue* funcVal = env.lookup(node->name);
    if (!funcVal) {
        // Undefined function called on symbolic arguments → return symbolic
        if (!args.empty() && args[0].isSymbolic()) {
            // Build a generic symbolic call (use first arg as proxy)
            return args[0];  // best-effort: return argument unchanged
        }
        recordError("Undefined function: " + node->name, node->line, node->col);
        return NeoValue::makeNull();
    }

    if (!funcVal->isFunction() && !funcVal->isNativeFunction()) {
        recordError(node->name + " is not a function", node->line, node->col);
        return NeoValue::makeNull();
    }

    // ── NativeFunction path ───────────────────────────────────────
    if (funcVal->isNativeFunction()) {
        if (!funcVal->nativeFn()) return NeoValue::makeNull();
        return funcVal->nativeFn()(args, funcVal->nativeCtx(), _symArena);
    }

    FunctionDefNode* def     = funcVal->funcDef();
    NeoEnv*          closure = funcVal->funcClosure();

    if (!def) return NeoValue::makeNull();

    // Check arity
    if (args.size() != def->params.size()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "%s() expects %zu argument(s), got %zu",
            node->name.c_str(), def->params.size(), args.size());
        recordError(std::string(buf), node->line, node->col);
        return NeoValue::makeNull();
    }

    // Create a new scope chained from the function's closure
    NeoEnv callEnv(closure);
    for (size_t i = 0; i < def->params.size(); ++i) {
        callEnv.define(def->params[i], args[i]);
    }

    // Execute body
    NeoValue retVal = evalBlock(def->body, callEnv);
    if (_returnFlag) {
        retVal = takeReturn();
    }
    return retVal;
}

// ════════════════════════════════════════════════════════════════════
// evalBuiltin — built-in mathematical functions
// ════════════════════════════════════════════════════════════════════

bool NeoInterpreter::evalBuiltin(const std::string& name,
                                   const std::vector<NeoValue>& args,
                                   NeoValue& result,
                                   NeoEnv& env) {
    using namespace cas;

    // ── Single-argument math functions ───────────────────────────
    auto unary = [&](SymFuncKind kind, double (*fn)(double)) -> bool {
        if (args.size() != 1) return false;
        const NeoValue& a = args[0];
        if (a.isNumeric()) {
            result = NeoValue::makeNumber(fn(a.toDouble()));
        } else if (a.isSymbolic() && a.asSym()) {
            result = NeoValue::makeSymbolic(symFunc(_symArena, kind, a.asSym()));
        } else {
            result = NeoValue::makeNumber(fn(a.toDouble()));
        }
        return true;
    };

    if (name == "sin")    return unary(SymFuncKind::Sin,    std::sin);
    if (name == "cos")    return unary(SymFuncKind::Cos,    std::cos);
    if (name == "tan")    return unary(SymFuncKind::Tan,    std::tan);
    if (name == "asin" || name == "arcsin")
        return unary(SymFuncKind::ArcSin, std::asin);
    if (name == "acos" || name == "arccos")
        return unary(SymFuncKind::ArcCos, std::acos);
    if (name == "atan" || name == "arctan")
        return unary(SymFuncKind::ArcTan, std::atan);
    if (name == "ln" || name == "log")
        return unary(SymFuncKind::Ln,  std::log);
    if (name == "log10")  return unary(SymFuncKind::Log10, std::log10);
    if (name == "exp")    return unary(SymFuncKind::Exp,   std::exp);
    if (name == "abs")    return unary(SymFuncKind::Abs,   std::fabs);

    // ── Hyperbolic functions ──────────────────────────────────────
    if (name == "sinh") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::sinh(args[0].toDouble()));
        return true;
    }
    if (name == "cosh") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::cosh(args[0].toDouble()));
        return true;
    }
    if (name == "tanh") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::tanh(args[0].toDouble()));
        return true;
    }
    if (name == "asinh" || name == "arcsinh") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::asinh(args[0].toDouble()));
        return true;
    }
    if (name == "acosh" || name == "arccosh") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::acosh(args[0].toDouble()));
        return true;
    }
    if (name == "atanh" || name == "arctanh") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::atanh(args[0].toDouble()));
        return true;
    }
    if (name == "atan2") {
        if (args.size() != 2) return false;
        result = NeoValue::makeNumber(std::atan2(args[0].toDouble(), args[1].toDouble()));
        return true;
    }
    if (name == "floor") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::floor(args[0].toDouble()));
        return true;
    }
    if (name == "ceil") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::ceil(args[0].toDouble()));
        return true;
    }
    if (name == "round") {
        if (args.size() < 1) return false;
        double val = args[0].toDouble();
        if (args.size() >= 2) {
            // round(x, digits)
            double factor = std::pow(10.0, args[1].toDouble());
            result = NeoValue::makeNumber(std::round(val * factor) / factor);
        } else {
            result = NeoValue::makeNumber(std::round(val));
        }
        return true;
    }
    if (name == "cbrt") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::cbrt(args[0].toDouble()));
        return true;
    }
    if (name == "log2") {
        if (args.size() != 1) return false;
        result = NeoValue::makeNumber(std::log2(args[0].toDouble()));
        return true;
    }
    // sign(x) — signum function
    if (name == "sign") {
        if (args.size() != 1) return false;
        double v = args[0].toDouble();
        result = NeoValue::makeNumber(v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0));
        return true;
    }
    // max(a, b) / min(a, b)
    if (name == "max") {
        if (args.size() < 1) return false;
        double m = args[0].toDouble();
        for (size_t k = 1; k < args.size(); ++k)
            if (args[k].toDouble() > m) m = args[k].toDouble();
        result = NeoValue::makeNumber(m);
        return true;
    }
    if (name == "min") {
        if (args.size() < 1) return false;
        double m = args[0].toDouble();
        for (size_t k = 1; k < args.size(); ++k)
            if (args[k].toDouble() < m) m = args[k].toDouble();
        result = NeoValue::makeNumber(m);
        return true;
    }

    if (name == "sqrt") {
        if (args.size() != 1) return false;
        const NeoValue& a = args[0];
        // sqrt(x) == x ^ (1/2)
        if (a.isNumeric()) {
            result = NeoValue::makeNumber(std::sqrt(a.toDouble()));
        } else if (a.isSymbolic() && a.asSym()) {
            SymExpr* half = symFrac(_symArena, 1, 2);
            result = NeoValue::makeSymbolic(symPow(_symArena, a.asSym(), half));
        } else {
            result = NeoValue::makeNumber(std::sqrt(a.toDouble()));
        }
        return true;
    }

    // ── n(expr) — numeric evaluator ──────────────────────────────
    // n(expr) or n(expr, digits): forces numeric evaluation.
    // For symbolic expressions: calls evaluate(0.0).
    // For numeric values: returns the value as-is (possible formatting by digits).
    if (name == "n") {
        if (args.empty()) { result = NeoValue::makeNull(); return true; }
        const NeoValue& a = args[0];
        // digits parameter (currently used only for display rounding)
        int digits = (args.size() >= 2) ? static_cast<int>(args[1].toDouble()) : 10;
        if (digits < 1) digits = 1;
        if (digits > 15) digits = 15;
        double numVal = a.toDouble();
        result = NeoValue::makeNumber(numVal);
        return true;
    }

    // ── List operations ───────────────────────────────────────────

    // __list__(elements...) — create a List from arguments
    if (name == "__list__") {
        auto* lst = new std::vector<NeoValue>(args.begin(), args.end());
        result = NeoValue::makeList(lst);
        return true;
    }

    // len(list_or_string) — length of a list
    if (name == "len") {
        if (args.size() != 1) { result = NeoValue::makeNumber(0); return true; }
        const NeoValue& a = args[0];
        if (a.isList() && a.asList()) {
            result = NeoValue::makeNumber(static_cast<double>(a.asList()->size()));
        } else {
            result = NeoValue::makeNumber(0);
        }
        return true;
    }

    // zeros(n) — list of n zeros
    if (name == "zeros") {
        int n = args.empty() ? 0 : static_cast<int>(args[0].toDouble());
        if (n < 0) n = 0;
        if (n > NEO_MAX_ITER) n = NEO_MAX_ITER;
        auto* lst = new std::vector<NeoValue>(static_cast<size_t>(n), NeoValue::makeNumber(0));
        result = NeoValue::makeList(lst);
        return true;
    }

    // ones(n) — list of n ones
    if (name == "ones") {
        int n = args.empty() ? 0 : static_cast<int>(args[0].toDouble());
        if (n < 0) n = 0;
        if (n > NEO_MAX_ITER) n = NEO_MAX_ITER;
        auto* lst = new std::vector<NeoValue>(static_cast<size_t>(n), NeoValue::makeNumber(1));
        result = NeoValue::makeList(lst);
        return true;
    }

    // sum(list) — sum of list elements
    if (name == "sum") {
        if (args.size() == 1 && args[0].isList() && args[0].asList()) {
            double s = 0.0;
            for (const auto& v : *args[0].asList()) s += v.toDouble();
            result = NeoValue::makeNumber(s);
            return true;
        }
        if (!args.empty() && args[0].isNumeric()) {
            double s = 0.0;
            for (const auto& v : args) s += v.toDouble();
            result = NeoValue::makeNumber(s);
            return true;
        }
        result = NeoValue::makeNumber(0);
        return true;
    }

    // dot(a, b) — dot product of two lists
    if (name == "dot") {
        if (args.size() == 2 && args[0].isList() && args[1].isList()
            && args[0].asList() && args[1].asList()) {
            const auto& la = *args[0].asList();
            const auto& lb = *args[1].asList();
            size_t len = std::min(la.size(), lb.size());
            double s = 0.0;
            for (size_t k = 0; k < len; ++k) s += la[k].toDouble() * lb[k].toDouble();
            result = NeoValue::makeNumber(s);
            return true;
        }
        result = NeoValue::makeNumber(0);
        return true;
    }

    // map(func, list) — apply func to each element of list
    // Note: func must be a user-defined NeoLanguage function stored in env.
    if (name == "map") {
        if (args.size() < 2) { result = NeoValue::makeNull(); return true; }
        const NeoValue& funcVal = args[0];
        const NeoValue& listVal = args[1];
        if (!funcVal.isFunction() || !listVal.isList() || !listVal.asList()) {
            result = NeoValue::makeNull();
            return true;
        }
        FunctionDefNode* def     = funcVal.funcDef();
        NeoEnv*          closure = funcVal.funcClosure();
        if (!def || def->params.empty()) { result = NeoValue::makeNull(); return true; }
        auto* out = new std::vector<NeoValue>();
        out->reserve(listVal.asList()->size());
        for (const NeoValue& elem : *listVal.asList()) {
            NeoEnv callEnv(closure);
            callEnv.define(def->params[0], elem);
            NeoValue r = evalBlock(def->body, callEnv);
            if (_returnFlag) { r = takeReturn(); }
            out->push_back(r);
        }
        result = NeoValue::makeList(out);
        return true;
    }

    // filter(func, list) — keep elements where func returns truthy
    if (name == "filter") {
        if (args.size() < 2) { result = NeoValue::makeNull(); return true; }
        const NeoValue& funcVal = args[0];
        const NeoValue& listVal = args[1];
        if (!funcVal.isFunction() || !listVal.isList() || !listVal.asList()) {
            result = NeoValue::makeNull();
            return true;
        }
        FunctionDefNode* def     = funcVal.funcDef();
        NeoEnv*          closure = funcVal.funcClosure();
        if (!def || def->params.empty()) { result = NeoValue::makeNull(); return true; }
        auto* out = new std::vector<NeoValue>();
        for (const NeoValue& elem : *listVal.asList()) {
            NeoEnv callEnv(closure);
            callEnv.define(def->params[0], elem);
            NeoValue r = evalBlock(def->body, callEnv);
            if (_returnFlag) { r = takeReturn(); }
            if (r.isTruthy()) out->push_back(elem);
        }
        result = NeoValue::makeList(out);
        return true;
    }

    // ── range(n) or range(start, stop) ─────────────────────────
    // Returns a NeoValue::Number representing the count (lightweight
    // support: used by for-in to generate an integer range).
    // The actual iteration is handled in evalForIn.
    if (name == "range") {
        if (args.size() == 1 && args[0].isNumeric()) {
            result = args[0];  // just pass the count through
            return true;
        }
        if (args.size() == 2 && args[0].isNumeric() && args[1].isNumeric()) {
            result = NeoValue::makeNumber(args[1].toDouble() - args[0].toDouble());
            return true;
        }
        return false;
    }

    // ── print / println and type: delegate to NeoStdLib for callback support ─

    // ── Phase 7: ndsolve(f, y0, x0, x1, steps) — RK4 ODE solver ──
    if (name == "ndsolve") {
        if (args.size() < 4) {
            result = NeoValue::makeNull(); return true;
        }
        const NeoValue& funcVal = args[0];
        double y0    = args[1].toDouble();
        double x0    = args[2].toDouble();
        double x1    = args[3].toDouble();
        int    steps = args.size() >= 5 ? static_cast<int>(args[4].toDouble()) : 200;

        // Build a C++ lambda that evaluates the NeoLanguage/native function
        std::function<double(double, double)> f;

        if (funcVal.isFunction() && funcVal.funcDef()) {
            FunctionDefNode* def     = funcVal.funcDef();
            NeoEnv*          closure = funcVal.funcClosure();
            if (!def || def->params.size() < 2) {
                _stdlib.callBuiltin("print", {NeoValue::makeString("[ndsolve] Function must accept 2 params f(x, y)")}, result, env, _symArena);
                result = NeoValue::makeNull(); return true;
            }
            f = [this, def, closure](double x, double y) -> double {
                NeoEnv callEnv(closure);
                callEnv.define(def->params[0], NeoValue::makeNumber(x));
                callEnv.define(def->params[1], NeoValue::makeNumber(y));
                NeoValue r = evalBlock(def->body, callEnv);
                if (_returnFlag) r = takeReturn();
                return r.toDouble();
            };
        } else if (funcVal.isNativeFunction() && funcVal.nativeFn()) {
            f = [&funcVal, this](double x, double y) -> double {
                std::vector<NeoValue> a = { NeoValue::makeNumber(x),
                                            NeoValue::makeNumber(y) };
                return funcVal.nativeFn()(a, funcVal.nativeCtx(), _symArena).toDouble();
            };
        } else if (funcVal.isSymbolic() && funcVal.asSym()) {
            // Treat symbolic f(x, y) as f(x) — evaluate sym at x only
            cas::SymExpr* sym = funcVal.asSym();
            f = [sym](double x, double /*y*/) -> double {
                return sym->evaluate(x);
            };
        } else {
            _stdlib.callBuiltin("print", {NeoValue::makeString("[ndsolve] First argument must be a function f(x, y)")}, result, env, _symArena);
            result = NeoValue::makeNull(); return true;
        }

        auto* pts = NeoScientific::ndsolveRK4(f, y0, x0, x1, steps);
        result = NeoValue::makeList(pts);
        return true;
    }

    // ── Phase 7: minimize(f, x0) — golden-section minimisation ───
    if (name == "minimize" || name == "maximize") {
        if (args.size() < 2) {
            result = NeoValue::makeNull(); return true;
        }
        const NeoValue& funcVal = args[0];
        double x0 = args[1].toDouble();
        bool   doMax = (name == "maximize");

        std::function<double(double)> f;

        if (funcVal.isFunction() && funcVal.funcDef()) {
            FunctionDefNode* def     = funcVal.funcDef();
            NeoEnv*          closure = funcVal.funcClosure();
            if (!def || def->params.empty()) {
                _stdlib.callBuiltin("print", {NeoValue::makeString("[minimize/maximize] Function must accept 1 param")}, result, env, _symArena);
                result = NeoValue::makeNull(); return true;
            }
            f = [this, def, closure](double x) -> double {
                NeoEnv callEnv(closure);
                callEnv.define(def->params[0], NeoValue::makeNumber(x));
                NeoValue r = evalBlock(def->body, callEnv);
                if (_returnFlag) r = takeReturn();
                return r.toDouble();
            };
        } else if (funcVal.isNativeFunction() && funcVal.nativeFn()) {
            f = [&funcVal, this](double x) -> double {
                std::vector<NeoValue> a = { NeoValue::makeNumber(x) };
                return funcVal.nativeFn()(a, funcVal.nativeCtx(), _symArena).toDouble();
            };
        } else if (funcVal.isSymbolic() && funcVal.asSym()) {
            cas::SymExpr* sym = funcVal.asSym();
            f = [sym](double x) -> double { return sym->evaluate(x); };
        } else {
            _stdlib.callBuiltin("print", {NeoValue::makeString("[minimize/maximize] First argument must be a function")}, result, env, _symArena);
            result = NeoValue::makeNull(); return true;
        }

        double xOpt = doMax ? NeoScientific::maximizeGolden(f, x0)
                            : NeoScientific::minimizeGolden(f, x0);
        result = NeoValue::makeNumber(xOpt);
        return true;
    }

    // ── All remaining built-ins: delegate to NeoStdLib ───────────
    // This handles: diff, integrate, solve, simplify, expand,
    //               plot, clear_plot, print, println, type,
    //               vars, input_num, msg_box
    if (_stdlib.callBuiltin(name, args, result, env, _symArena)) return true;

    // ── Phase 8: time_it(func, args...) — performance profiling ──
    // Measures the time in milliseconds to execute a NeoLanguage function.
    if (name == "time_it") {
        if (args.empty() || (!args[0].isFunction() && !args[0].isNativeFunction())) {
            result = NeoValue::makeNumber(0);
            return true;
        }
        // Collect extra args to pass to the function
        std::vector<NeoValue> callArgs(args.begin() + 1, args.end());

#ifdef ARDUINO
        unsigned long t0 = millis();
#else
        auto t0 = std::chrono::steady_clock::now();
#endif
        // Invoke the function
        if (args[0].isFunction() && args[0].funcDef()) {
            FunctionDefNode* def     = args[0].funcDef();
            NeoEnv*          closure = args[0].funcClosure();
            if (def && def->params.size() == callArgs.size()) {
                NeoEnv callEnv(closure);
                for (size_t i = 0; i < def->params.size(); ++i)
                    callEnv.define(def->params[i], callArgs[i]);
                NeoValue r = evalBlock(def->body, callEnv);
                if (_returnFlag) takeReturn();
                (void)r;
            }
        } else if (args[0].isNativeFunction() && args[0].nativeFn()) {
            NeoValue r = args[0].nativeFn()(callArgs, args[0].nativeCtx(), _symArena);
            (void)r;
        }
#ifdef ARDUINO
        unsigned long elapsed = millis() - t0;
        result = NeoValue::makeNumber(static_cast<double>(elapsed));
#else
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        result = NeoValue::makeNumber(ms);
#endif
        return true;
    }

    return false;  // Not a built-in
}

// ════════════════════════════════════════════════════════════════════
// evalAssignment
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalAssignment(AssignmentNode* node, NeoEnv& env) {
    NeoValue val = eval(node->value, env);
    if (_returnFlag) return val;

    if (node->is_delayed) {
        // := (delayed assignment): store as symbolic if RHS is symbolic
        // or convert to symbolic if the name looks like a function def.
        env.define(node->target, val);
    } else {
        // = (immediate assignment)
        env.assign(node->target, val);
    }
    return val;
}

// ════════════════════════════════════════════════════════════════════
// evalIf
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalIf(IfNode* node, NeoEnv& env) {
    NeoValue cond = eval(node->condition, env);
    if (_returnFlag) return cond;

    if (cond.isTruthy()) {
        NeoEnv thenEnv(&env);
        return evalBlock(node->then_body, thenEnv);
    } else if (!node->else_body.empty()) {
        NeoEnv elseEnv(&env);
        return evalBlock(node->else_body, elseEnv);
    }
    return NeoValue::makeNull();
}

// ════════════════════════════════════════════════════════════════════
// evalWhile
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalWhile(WhileNode* node, NeoEnv& env) {
    static constexpr int MAX_ITER = NEO_MAX_ITER;
    NeoValue last = NeoValue::makeNull();

    for (int i = 0; i < MAX_ITER; ++i) {
        NeoValue cond = eval(node->condition, env);
        if (_returnFlag) return cond;
        if (!cond.isTruthy()) break;

        NeoEnv bodyEnv(&env);
        last = evalBlock(node->body, bodyEnv);
        if (_returnFlag) return last;
    }
    return last;
}

// ════════════════════════════════════════════════════════════════════
// evalForIn
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalForIn(ForInNode* node, NeoEnv& env) {
    NeoValue iterVal = eval(node->iterable, env);
    if (_returnFlag) return iterVal;

    NeoValue last = NeoValue::makeNull();

    // ── for i in list: ─────────────────────────────────────────────
    if (iterVal.isList() && iterVal.asList()) {
        const auto& lst = *iterVal.asList();
        int iter = 0;
        for (const NeoValue& elem : lst) {
            if (++iter > NEO_MAX_ITER) break;
            NeoEnv bodyEnv(&env);
            bodyEnv.define(node->var, elem);
            last = evalBlock(node->body, bodyEnv);
            if (_returnFlag) return last;
        }
        return last;
    }

    // Support for-in over a numeric range (the primary use case).
    // For `for i in range(n)` the parser produces a FunctionCall,
    // which evalFunctionCall simplifies to a Number n.
    // We iterate i = 0 .. n-1.
    if (iterVal.isNumeric()) {
        int64_t count = static_cast<int64_t>(iterVal.toDouble());
        if (count > NEO_MAX_ITER) count = NEO_MAX_ITER;
        for (int64_t i = 0; i < count; ++i) {
            NeoEnv bodyEnv(&env);
            bodyEnv.define(node->var,
                NeoValue::makeExact(cas::CASRational::fromInt(i)));
            last = evalBlock(node->body, bodyEnv);
            if (_returnFlag) return last;
        }
        return last;
    }

    // Symbolic iterable — we can't iterate meaningfully; skip.
    return NeoValue::makeNull();
}

// ════════════════════════════════════════════════════════════════════
// evalFunctionDef
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalFunctionDef(FunctionDefNode* node, NeoEnv& env) {
    // Capture the current environment as the closure
    NeoValue funcVal = NeoValue::makeFunction(node, &env);
    env.define(node->name, funcVal);
    return funcVal;
}

// ════════════════════════════════════════════════════════════════════
// evalReturn
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalReturn(ReturnNode* node, NeoEnv& env) {
    NeoValue val = node->value ? eval(node->value, env) : NeoValue::makeNull();
    _returnFlag  = true;
    _returnValue = val;
    return val;
}

// ════════════════════════════════════════════════════════════════════
// evalIndexOp — subscript / index access: target[i] or target[i, j]
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalIndexOp(IndexOpNode* node, NeoEnv& env) {
    NeoValue target = eval(node->target, env);
    if (_returnFlag) return target;

    if (node->indices.empty()) {
        recordError("Index expression requires at least one index", node->line, node->col);
        return NeoValue::makeNull();
    }

    // Evaluate first index
    NeoValue idx0 = eval(node->indices[0], env);
    if (_returnFlag) return idx0;

    // ── Dictionary indexing (Phase 6) ─────────────────────────────
    if (target.isDict() && target.asDict()) {
        // Index must be a string key
        std::string key;
        if (idx0.isString()) {
            key = idx0.asString();
        } else if (idx0.isNumeric()) {
            // Allow numeric keys by converting to string
            key = std::to_string(static_cast<int64_t>(idx0.toDouble()));
        } else {
            // Use toString() for any other key type
            key = idx0.toString();
        }
        auto it = target.asDict()->find(key);
        if (it == target.asDict()->end()) {
            recordError("Dictionary key not found: " + key, node->line, node->col);
            return NeoValue::makeNull();
        }
        return it->second;
    }

    // ── List / 1-D indexing ────────────────────────────────────────
    if (target.isList() && target.asList()) {
        const auto& lst = *target.asList();
        int64_t i = static_cast<int64_t>(idx0.toDouble());
        if (i < 0) i += static_cast<int64_t>(lst.size());  // negative indexing
        if (i < 0 || i >= static_cast<int64_t>(lst.size())) {
            char buf[80];
            std::snprintf(buf, sizeof(buf),
                "List index %lld out of range (size %zu)",
                static_cast<long long>(i), lst.size());
            recordError(buf, node->line, node->col);
            return NeoValue::makeNull();
        }

        // ── Matrix row access: M[r] returns a row (List of Lists) ──
        if (node->indices.size() == 1) {
            return lst[static_cast<size_t>(i)];
        }

        // ── Matrix element: M[r, c] ────────────────────────────────
        NeoValue row = lst[static_cast<size_t>(i)];
        if (row.isList() && row.asList()) {
            NeoValue idx1 = eval(node->indices[1], env);
            if (_returnFlag) return idx1;
            const auto& rowLst = *row.asList();
            int64_t j = static_cast<int64_t>(idx1.toDouble());
            if (j < 0) j += static_cast<int64_t>(rowLst.size());
            if (j < 0 || j >= static_cast<int64_t>(rowLst.size())) {
                recordError("Matrix column index out of range", node->line, node->col);
                return NeoValue::makeNull();
            }
            return rowLst[static_cast<size_t>(j)];
        }
        return NeoValue::makeNull();
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Value of type '%s' is not subscriptable",
                  target.isDict()     ? "dict" :
                  target.isList()     ? "list" :
                  target.isNumeric()  ? "number" :
                  target.isSymbolic() ? "symbolic" :
                  target.isString()   ? "string" : "unknown");
    recordError(buf, node->line, node->col);
    return NeoValue::makeNull();
}

// ════════════════════════════════════════════════════════════════════
// evalDictLiteral — { "key": expr, ... }  (Phase 6)
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalDictLiteral(DictLiteralNode* node, NeoEnv& env) {
    auto* dict = new std::map<std::string, NeoValue>();
    for (size_t i = 0; i < node->keys.size() && i < node->values.size(); ++i) {
        NeoValue val = eval(node->values[i], env);
        if (_returnFlag) { delete dict; return val; }
        (*dict)[node->keys[i]] = val;
    }
    return NeoValue::makeDict(dict);
}

// ════════════════════════════════════════════════════════════════════
// evalTryExcept — try/except exception handling  (Phase 6)
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalTryExcept(TryExceptNode* node, NeoEnv& env) {
    // Save error state
    bool   savedHasError  = _hasError;
    int    savedErrCount  = _errorCount;
    std::string savedErr  = _lastError;

    // Reset errors so we can detect new ones inside try block
    _hasError    = false;
    _errorCount  = 0;
    _lastError.clear();

    NeoEnv tryEnv(&env);
    NeoValue result = evalBlock(node->try_body, tryEnv);

    if (_hasError || _returnFlag) {
        // An error occurred in the try block — execute except block
        std::string errorMsg = _lastError;

        // Restore pre-try error state (we've "handled" the try error)
        _hasError   = savedHasError;
        _errorCount = savedErrCount;
        _lastError  = savedErr;
        _returnFlag = false;  // clear return flag too

        NeoEnv exceptEnv(&env);
        if (!node->except_var.empty()) {
            // Bind the error message as a string to the exception variable
            exceptEnv.define(node->except_var, NeoValue::makeString(errorMsg));
        }
        result = evalBlock(node->except_body, exceptEnv);
    } else {
        // No error — restore original state, discarding any new non-error state
        _hasError   = savedHasError;
        _errorCount = savedErrCount;
        _lastError  = savedErr;
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════
// evalImport — import / from...import  (Phase 8: Module System)
// ════════════════════════════════════════════════════════════════════

NeoValue NeoInterpreter::evalImport(ImportNode* node, NeoEnv& env) {
    if (!node) return NeoValue::makeNull();

    if (!NeoModules::exists(node->module)) {
        // Unknown module — report error but don't abort execution
        NeoValue dummy;
        std::string msg = "[import] Unknown module: '" + node->module + "'";
        _stdlib.callBuiltin("print", {NeoValue::makeString(msg)}, dummy, env, _symArena);
        return NeoValue::makeNull();
    }

    if (node->is_from) {
        // from X import a, b, c  — bind named exports into current env
        std::map<std::string, NeoValue> exports;
        if (node->names.empty()) {
            // from X import  (without names) → import all
            NeoModules::loadIntoEnv(node->module, env, _symArena);
        } else {
            // Build full module, then selectively bind only requested names
            NeoValue dict;
            if (NeoModules::loadAsDict(node->module, dict, _symArena)
                && dict.isDict() && dict.asDict()) {
                for (const std::string& name : node->names) {
                    auto it = dict.asDict()->find(name);
                    if (it != dict.asDict()->end()) {
                        env.define(name, it->second);
                    } else {
                        NeoValue dummy;
                        std::string msg = "[import] '" + name
                                        + "' not found in module '" + node->module + "'";
                        _stdlib.callBuiltin("print", {NeoValue::makeString(msg)},
                                            dummy, env, _symArena);
                    }
                }
            }
        }
    } else if (!node->alias.empty()) {
        // import X as Y  — bind entire module as a dictionary named Y
        NeoValue dict;
        if (NeoModules::loadAsDict(node->module, dict, _symArena)) {
            env.define(node->alias, dict);
        }
    } else {
        // import X  — bind all exports directly into current env
        NeoModules::loadIntoEnv(node->module, env, _symArena);
    }

    return NeoValue::makeNull();
}
