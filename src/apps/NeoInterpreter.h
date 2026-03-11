/**
 * NeoInterpreter.h — AST evaluation engine for NeoLanguage.
 *
 * Walks an AST produced by NeoParser and evaluates it in a NeoEnv
 * environment.  Supports:
 *   · Literal values (numbers, booleans, None)
 *   · Arithmetic and comparison expressions
 *   · Variable lookup and assignment (= and :=)
 *   · Function definitions and calls (def / f(x):= syntax)
 *   · Control flow: if/elif/else, while, for-in
 *   · Return statements
 *   · Built-in mathematical functions (sin, cos, sqrt, etc.)
 *   · Standard library functions via NeoStdLib (diff, integrate, plot, …)
 *
 * Wolfram-Language-like symbolic behaviour:
 *   Undefined variables do NOT cause errors.  They are returned as
 *   Symbolic NeoValues wrapping a cas::SymExpr* (SymVar node).
 *   Any arithmetic involving a Symbolic value escalates the result to
 *   Symbolic using the Pro-CAS factories (symAdd, symMul, etc.).
 *
 * Return propagation:
 *   Because ESP32 projects typically disable C++ exceptions,
 *   return values are propagated via a _returnFlag / _returnValue
 *   pair.  After each statement eval, callers check returnPending()
 *   and exit their statement list early.
 *
 * Standard Library integration:
 *   The interpreter embeds a NeoStdLib instance.  After checking its
 *   own math primitives (sin, cos, etc.), evalBuiltin() delegates to
 *   NeoStdLib::callBuiltin() for CAS-level and utility functions.
 *   Host I/O callbacks (print, plot, input_num, msg_box) are forwarded
 *   from the host app via setHostCallbacks().
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 3 (Standard Library)
 */
#pragma once

#include <string>
#include <vector>
#include "../math/cas/SymExprArena.h"
#include "../math/cas/SymExpr.h"
#include "NeoAST.h"
#include "NeoValue.h"
#include "NeoEnv.h"
#include "NeoStdLib.h"

// ════════════════════════════════════════════════════════════════════
// NeoInterpreter
// ════════════════════════════════════════════════════════════════════

class NeoInterpreter {
public:
    /**
     * Construct with a SymExprArena used for all Symbolic node creation.
     * The arena must outlive the interpreter.
     */
    explicit NeoInterpreter(cas::SymExprArena& symArena);

    // ── Standard Library / Host integration ───────────────────────
    /**
     * Forward host I/O callbacks to the embedded NeoStdLib.
     * Must be called before running any program that uses print(),
     * plot(), input_num(), or msg_box().
     */
    void setHostCallbacks(const NeoHostCallbacks& cbs) {
        _stdlib.setCallbacks(cbs);
    }

    /**
     * Read the most recent plot request produced by a plot() call.
     * The host should check this after each runCode() to decide whether
     * to enter Graphics Mode.
     */
    const NeoPlotRequest& plotRequest() const { return _stdlib.plotRequest(); }

    /** Clear the pending plot request (host calls after consuming it). */
    void clearPlotRequest() { _stdlib.clearPlotRequest(); }

    // ── Main entry point ──────────────────────────────────────────
    /**
     * Evaluate `node` in the given environment.
     * Returns the NeoValue result of the evaluation.
     * For ProgramNode, returns the last non-Null statement result.
     * For statements with no value (if, while, def), returns Null.
     */
    NeoValue eval(NeoNode* node, NeoEnv& env);

    // ── Error state ───────────────────────────────────────────────
    bool               hasError()   const { return _hasError; }
    const std::string& lastError()  const { return _lastError; }
    int                errorCount() const { return _errorCount; }
    void               clearErrors();

    // ── Return propagation ────────────────────────────────────────
    /** True if a `return` statement has been evaluated and not consumed. */
    bool returnPending() const { return _returnFlag; }

    /** Consume the pending return value (clears _returnFlag). */
    NeoValue takeReturn();

private:
    cas::SymExprArena& _symArena;
    NeoStdLib          _stdlib;   ///< Standard Library (CAS + graphics + I/O)

    // ── Error tracking ────────────────────────────────────────────
    bool        _hasError;
    std::string _lastError;
    int         _errorCount;

    // ── Return propagation state ──────────────────────────────────
    bool     _returnFlag;
    NeoValue _returnValue;

    // ── Node evaluators (one per NodeKind) ────────────────────────
    NeoValue evalProgram     (ProgramNode*      node, NeoEnv& env);
    NeoValue evalNumber      (NumberNode*       node);
    NeoValue evalSymbol      (SymbolNode*       node, NeoEnv& env);
    NeoValue evalBinaryOp    (BinaryOpNode*     node, NeoEnv& env);
    NeoValue evalUnaryOp     (UnaryOpNode*      node, NeoEnv& env);
    NeoValue evalFunctionCall(FunctionCallNode* node, NeoEnv& env);
    NeoValue evalAssignment  (AssignmentNode*   node, NeoEnv& env);
    NeoValue evalIf          (IfNode*           node, NeoEnv& env);
    NeoValue evalWhile       (WhileNode*        node, NeoEnv& env);
    NeoValue evalForIn       (ForInNode*        node, NeoEnv& env);
    NeoValue evalFunctionDef (FunctionDefNode*  node, NeoEnv& env);
    NeoValue evalReturn      (ReturnNode*       node, NeoEnv& env);

    // ── Built-in function dispatch ────────────────────────────────
    /**
     * Try to evaluate a call to a known built-in function.
     * Checks math primitives (sin, cos, etc.) first, then delegates
     * to NeoStdLib for CAS-level and utility functions.
     * Returns true and sets `result` if handled; false otherwise.
     */
    bool evalBuiltin(const std::string& name,
                     const std::vector<NeoValue>& args,
                     NeoValue& result,
                     NeoEnv& env);

    // ── Helpers ───────────────────────────────────────────────────
    void recordError(const std::string& msg, int line, int col);

    /** Execute a list of statements; stops early on return. */
    NeoValue evalBlock(const NeoNodeVec& stmts, NeoEnv& env);
};
