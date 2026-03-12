/**
 * NeoStdLib.cpp — Standard Library implementation for NeoLanguage Phase 3.
 *
 * See NeoStdLib.h for the public API and design rationale.
 *
 * CAS usage:
 *   diff      → cas::SymDiff::diff()
 *   integrate → cas::SymIntegrate::integrate()
 *   solve     → cas::OmniSolver::solve()
 *   simplify  → cas::SymSimplify::simplify()
 *   expand    → cas::SymSimplify::simplify() (includes G8: distribute)
 *
 * Plot usage:
 *   plot() stores a NeoPlotRequest with the SymExpr* pointer.
 *   The host (NeoLanguageApp) reads plotRequest() after execution and
 *   renders the curve using SymExpr::evaluate(x) directly — this makes
 *   the plot completely independent of GrapherApp's string evaluator.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 3 (Standard Library)
 */

#include "NeoStdLib.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

#include "../math/cas/SymDiff.h"
#include "../math/cas/SymIntegrate.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/OmniSolver.h"

// ════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════

NeoStdLib::NeoStdLib()
    : _cbs{}
    , _plotReq{}
{}

// ════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════

cas::SymExpr* NeoStdLib::toSym(const NeoValue& v, cas::SymExprArena& sa) {
    if (v.isSymbolic()) return v.asSym();
    if (v.isExact()) {
        return cas::symNum(sa, v.asExact());
    }
    if (v.isNumeric()) {
        double d = v.toDouble();
        long long i = static_cast<long long>(d);
        if (static_cast<double>(i) == d) {
            return cas::symFrac(sa, i, 1);
        }
        // Approximate non-integer as a rational with this denominator.
        // Provides ~6 decimal digits of precision, sufficient for CAS display.
        static constexpr long long RATIONAL_APPROX_DENOM = 1000000LL;
        long long num = static_cast<long long>(d * (double)RATIONAL_APPROX_DENOM);
        return cas::symFrac(sa, num, RATIONAL_APPROX_DENOM);
    }
    return nullptr;
}

char NeoStdLib::toVarChar(const NeoValue& v, cas::SymExprArena& /*sa*/) {
    if (!v.isSymbolic()) return 'x';
    cas::SymExpr* sym = v.asSym();
    if (!sym) return 'x';
    if (sym->type != cas::SymExprType::Var) return 'x';
    return static_cast<cas::SymVar*>(sym)->name;
}

void NeoStdLib::hostPrint(const char* text) {
    if (_cbs.onPrint) _cbs.onPrint(text, _cbs.userdata);
}

// ════════════════════════════════════════════════════════════════════
// Main dispatch
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callBuiltin(const std::string&          name,
                             const std::vector<NeoValue>& args,
                             NeoValue&                    result,
                             NeoEnv&                      env,
                             cas::SymExprArena&           sa)
{
    // ── Symbolic calculus ──────────────────────────────────────────
    if (name == "diff")      return callDiff     (args, result, sa);
    if (name == "integrate") return callIntegrate(args, result, sa);
    if (name == "solve")     return callSolve    (args, result, sa);
    if (name == "simplify")  return callSimplify (args, result, sa);
    if (name == "expand")    return callExpand   (args, result, sa);

    // ── Graphics ───────────────────────────────────────────────────
    if (name == "plot")       return callPlot      (args, result, sa);
    if (name == "clear_plot") return callClearPlot (result);

    // ── Utilities ──────────────────────────────────────────────────
    if (name == "print")     return callPrint    (args, result);
    if (name == "type")      return callType     (args, result);
    if (name == "vars")      return callVars     (args, result, env);
    if (name == "input_num") return callInputNum (args, result);
    if (name == "msg_box")   return callMsgBox   (args, result);

    return false;  // Not a stdlib built-in
}

// ════════════════════════════════════════════════════════════════════
// diff(expr, var)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callDiff(const std::vector<NeoValue>& args,
                          NeoValue& result,
                          cas::SymExprArena& sa)
{
    if (args.size() < 2) {
        hostPrint("[diff] Usage: diff(expr, var)\n");
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* expr = toSym(args[0], sa);
    char          var  = toVarChar(args[1], sa);

    if (!expr) {
        hostPrint("[diff] First argument must be a symbolic expression.\n");
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* d = cas::SymDiff::diff(expr, var, sa);
    if (!d) {
        hostPrint("[diff] Differentiation failed.\n");
        result = NeoValue::makeNull();
        return true;
    }

    // Simplify the derivative
    cas::SymExpr* simplified = cas::SymSimplify::simplify(d, sa);
    result = NeoValue::makeSymbolic(simplified ? simplified : d);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// integrate(expr, var)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callIntegrate(const std::vector<NeoValue>& args,
                               NeoValue& result,
                               cas::SymExprArena& sa)
{
    if (args.size() < 2) {
        hostPrint("[integrate] Usage: integrate(expr, var)\n");
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* expr = toSym(args[0], sa);
    char          var  = toVarChar(args[1], sa);

    if (!expr) {
        hostPrint("[integrate] First argument must be a symbolic expression.\n");
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* iexpr = cas::SymIntegrate::integrate(expr, var, sa);
    if (!iexpr) {
        // Return as unevaluated integral
        iexpr = cas::symFunc(sa, cas::SymFuncKind::Integral, expr);
        if (!iexpr) {
            result = NeoValue::makeNull();
            return true;
        }
    }
    result = NeoValue::makeSymbolic(iexpr);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// solve(expr, var)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callSolve(const std::vector<NeoValue>& args,
                           NeoValue& result,
                           cas::SymExprArena& sa)
{
    if (args.size() < 2) {
        hostPrint("[solve] Usage: solve(expr, var)\n");
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* lhs = toSym(args[0], sa);
    char          var = toVarChar(args[1], sa);

    if (!lhs) {
        hostPrint("[solve] First argument must be a symbolic expression.\n");
        result = NeoValue::makeNull();
        return true;
    }

    // RHS = 0
    cas::SymExpr* zero = cas::symFrac(sa, 0, 1);

    cas::OmniSolver solver;
    cas::OmniResult res = solver.solve(lhs, zero, var, sa);

    if (!res.ok || res.solutions.empty()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[solve] %s\n",
                      res.error.empty() ? "No solution found." : res.error.c_str());
        hostPrint(buf);
        result = NeoValue::makeNull();
        return true;
    }

    // Return the first solution
    const cas::OmniSolution& sol = res.solutions[0];
    if (sol.symbolic) {
        result = NeoValue::makeSymbolic(sol.symbolic);
    } else {
        result = NeoValue::makeNumber(sol.numeric);
    }

    // Print all solutions to console for visibility
    for (size_t i = 0; i < res.solutions.size(); ++i) {
        char buf[128];
        const cas::OmniSolution& s = res.solutions[i];
        if (s.symbolic) {
            std::snprintf(buf, sizeof(buf), "%c = %s\n", var,
                          s.symbolic->toString().c_str());
        } else {
            std::snprintf(buf, sizeof(buf), "%c = %.10g\n", var, s.numeric);
        }
        hostPrint(buf);
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════
// simplify(expr)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callSimplify(const std::vector<NeoValue>& args,
                              NeoValue& result,
                              cas::SymExprArena& sa)
{
    if (args.empty()) {
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* expr = toSym(args[0], sa);
    if (!expr) {
        result = args[0];  // pass through non-symbolic values
        return true;
    }

    cas::SymExpr* simplified = cas::SymSimplify::simplify(expr, sa);
    result = NeoValue::makeSymbolic(simplified ? simplified : expr);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// expand(expr)
// ════════════════════════════════════════════════════════════════════
// Polynomial expansion uses SymSimplify which includes the G8 Distribute
// rule.  After simplification, fully-expanded expressions are returned.

bool NeoStdLib::callExpand(const std::vector<NeoValue>& args,
                            NeoValue& result,
                            cas::SymExprArena& sa)
{
    // expand() applies the distribute rule (G8 in SymSimplify)
    // SymSimplify::simplify() already runs all rule groups including G8,
    // so calling it is equivalent to a full expansion pass.
    return callSimplify(args, result, sa);
}

// ════════════════════════════════════════════════════════════════════
// plot(expr, xmin, xmax)
// ════════════════════════════════════════════════════════════════════
// Stores a NeoPlotRequest which the host (NeoLanguageApp) consumes after
// execution to enter Graphics Mode.  The SymExpr* is evaluated directly
// via SymExpr::evaluate(x) — completely decoupled from GrapherApp's
// string-based evaluation pipeline.

bool NeoStdLib::callPlot(const std::vector<NeoValue>& args,
                          NeoValue& result,
                          cas::SymExprArena& sa)
{
    if (args.empty()) {
        hostPrint("[plot] Usage: plot(expr, xmin, xmax)\n");
        result = NeoValue::makeNull();
        return true;
    }

    cas::SymExpr* expr = toSym(args[0], sa);
    if (!expr) {
        hostPrint("[plot] First argument must be a symbolic expression.\n");
        result = NeoValue::makeNull();
        return true;
    }

    double xMin = (args.size() >= 2) ? args[1].toDouble() : -10.0;
    double xMax = (args.size() >= 3) ? args[2].toDouble() :  10.0;
    if (xMin >= xMax) xMax = xMin + 20.0;

    _plotReq.expr    = expr;
    _plotReq.xMin    = xMin;
    _plotReq.xMax    = xMax;
    _plotReq.pending = true;

    char buf[128];
    std::snprintf(buf, sizeof(buf), "[plot] %s  [%.4g, %.4g]\n",
                  expr->toString().c_str(), xMin, xMax);
    hostPrint(buf);

    result = NeoValue::makeNull();
    return true;
}

// ════════════════════════════════════════════════════════════════════
// clear_plot()
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callClearPlot(NeoValue& result) {
    _plotReq = NeoPlotRequest{};
    result   = NeoValue::makeNull();
    return true;
}

// ════════════════════════════════════════════════════════════════════
// print(args...)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callPrint(const std::vector<NeoValue>& args,
                           NeoValue& result)
{
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) hostPrint(" ");
        std::string s = args[i].toString();
        hostPrint(s.c_str());
    }
    hostPrint("\n");
    result = NeoValue::makeNull();
    return true;
}

// ════════════════════════════════════════════════════════════════════
// type(val)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callType(const std::vector<NeoValue>& args,
                          NeoValue& result)
{
    if (args.empty()) {
        result = NeoValue::makeNull();
        return true;
    }

    const char* typeName = nullptr;
    int         ordinal  = 0;
    switch (args[0].type()) {
        case NeoValue::Type::Null:     typeName = "None";     ordinal = 0; break;
        case NeoValue::Type::Boolean:  typeName = "bool";     ordinal = 1; break;
        case NeoValue::Type::Number:   typeName = "number";   ordinal = 2; break;
        case NeoValue::Type::Exact:    typeName = "exact";    ordinal = 3; break;
        case NeoValue::Type::Symbolic: typeName = "symbolic"; ordinal = 4; break;
        case NeoValue::Type::Function: typeName = "function"; ordinal = 5; break;
        case NeoValue::Type::List:     typeName = "list";     ordinal = 6; break;
        default:                       typeName = "unknown";  ordinal = -1; break;
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "type: %s\n", typeName);
    hostPrint(buf);

    result = NeoValue::makeNumber(static_cast<double>(ordinal));
    return true;
}

// ════════════════════════════════════════════════════════════════════
// vars()
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callVars(const std::vector<NeoValue>& /*args*/,
                          NeoValue& result,
                          NeoEnv& env)
{
    hostPrint("--- variables ---\n");

    // Walk all scopes from current up to global
    int count = 0;
    NeoEnv* scope = &env;
    while (scope) {
        scope->forEach([&](const std::string& name, const NeoValue& val) {
            char buf[256];
            std::string vs = val.toString();
            if (vs.size() > 40) vs = vs.substr(0, 40) + "...";
            std::snprintf(buf, sizeof(buf), "  %s = %s\n",
                          name.c_str(), vs.c_str());
            hostPrint(buf);
            ++count;
        });
        scope = scope->parent();
    }

    if (count == 0) hostPrint("  (no variables defined)\n");
    hostPrint("-----------------\n");

    result = NeoValue::makeNull();
    return true;
}

// ════════════════════════════════════════════════════════════════════
// input_num(prompt)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callInputNum(const std::vector<NeoValue>& args,
                              NeoValue& result)
{
    const char* prompt = "Enter a number:";
    std::string promptStorage;
    if (!args.empty()) {
        promptStorage = args[0].toString();
        prompt = promptStorage.c_str();
    }

    double val = 0.0;
    if (_cbs.onInputNum) {
        val = _cbs.onInputNum(prompt, _cbs.userdata);
    } else {
        hostPrint("[input_num] No UI hook set — returning 0\n");
    }

    result = NeoValue::makeNumber(val);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// msg_box(title, content)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callMsgBox(const std::vector<NeoValue>& args,
                            NeoValue& result)
{
    std::string title   = (args.size() >= 1) ? args[0].toString() : "Message";
    std::string content = (args.size() >= 2) ? args[1].toString() : "";

    if (_cbs.onMsgBox) {
        _cbs.onMsgBox(title.c_str(), content.c_str(), _cbs.userdata);
    } else {
        // Fallback: print to console
        char buf[512];
        std::snprintf(buf, sizeof(buf), "[%s] %s\n",
                      title.c_str(), content.c_str());
        hostPrint(buf);
    }

    result = NeoValue::makeNull();
    return true;
}
