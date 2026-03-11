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
    if (name == "True"  || name == "true")  return NeoValue::makeBool(true);
    if (name == "False" || name == "false") return NeoValue::makeBool(false);
    if (name == "None"  || name == "none")  return NeoValue::makeNull();
    if (name == "inf"   || name == "Inf" || name == "Infinity")
        return NeoValue::makeNumber(std::numeric_limits<double>::infinity());

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
        case BinaryOpNode::OpKind::Eq:  return lv.opEq(rv);
        case BinaryOpNode::OpKind::Ne:  return lv.opNe(rv);
        case BinaryOpNode::OpKind::Lt:  return lv.opLt(rv);
        case BinaryOpNode::OpKind::Le:  return lv.opLe(rv);
        case BinaryOpNode::OpKind::Gt:  return lv.opGt(rv);
        case BinaryOpNode::OpKind::Ge:  return lv.opGe(rv);
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

    if (!funcVal->isFunction()) {
        recordError(node->name + " is not a function", node->line, node->col);
        return NeoValue::makeNull();
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

    // ── range(n) or range(start, stop) ─────────────────────────
    // Returns a NeoValue::Number representing the count (lightweight
    // support: used by for-in to generate an integer range).
    // The actual iteration is handled in evalForIn.
    if (name == "range") {
        // Return a special "range" marker value — evalForIn handles it
        // by detecting Function type with nullptr def.
        if (args.size() == 1 && args[0].isNumeric()) {
            result = args[0];  // just pass the count through
            return true;
        }
        if (args.size() == 2 && args[0].isNumeric() && args[1].isNumeric()) {
            // For range(start, stop), we can't easily return both values
            // as a single NeoValue in the current design.
            // Return stop - start as a proxy (evalForIn special-cases range).
            result = NeoValue::makeNumber(args[1].toDouble() - args[0].toDouble());
            return true;
        }
        return false;
    }

    // ── print / println and type: delegate to NeoStdLib for callback support ─
    // (These were previously handled here as no-ops; NeoStdLib now routes
    //  them through the host print callback so output appears in the console.)

    // ── All remaining built-ins: delegate to NeoStdLib ───────────
    // This handles: diff, integrate, solve, simplify, expand,
    //               plot, clear_plot, print, println, type,
    //               vars, input_num, msg_box
    if (_stdlib.callBuiltin(name, args, result, env, _symArena)) return true;

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
