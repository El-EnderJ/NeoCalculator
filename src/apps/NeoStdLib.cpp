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
#include <algorithm>
#include <limits>

#include "../math/cas/SymDiff.h"
#include "../math/cas/SymIntegrate.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/OmniSolver.h"
#include "../math/cas/SystemSolver.h"
#include "NeoFinance.h"
#include "NeoBitwise.h"
#include "NeoScientific.h"
#include "NeoPhysics.h"
#include "NeoIO.h"
#include "NeoGUI.h"

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
    if (name == "println")   return callPrint    (args, result);
    if (name == "type")      return callType     (args, result);
    if (name == "vars")      return callVars     (args, result, env);
    if (name == "input_num") return callInputNum (args, result);
    if (name == "msg_box")   return callMsgBox   (args, result);

    // ── int(x) — integer conversion ───────────────────────────────
    if (name == "int") {
        if (args.empty()) { result = NeoValue::makeNumber(0); return true; }
        result = NeoValue::makeNumber(std::floor(args[0].toDouble()));
        return true;
    }
    // ── str(x) — string conversion ────────────────────────────────
    if (name == "str") {
        if (args.empty()) { result = NeoValue::makeString(""); return true; }
        result = NeoValue::makeString(args[0].toString());
        return true;
    }
    // ── keys(dict) — return list of dictionary keys ───────────────
    if (name == "keys") {
        if (args.size() == 1 && args[0].isDict() && args[0].asDict()) {
            auto* lst = new std::vector<NeoValue>();
            for (const auto& kv : *args[0].asDict())
                lst->push_back(NeoValue::makeString(kv.first));
            result = NeoValue::makeList(lst);
        } else {
            result = NeoValue::makeList(new std::vector<NeoValue>());
        }
        return true;
    }
    // ── values(dict) — return list of dictionary values ───────────
    if (name == "values") {
        if (args.size() == 1 && args[0].isDict() && args[0].asDict()) {
            auto* lst = new std::vector<NeoValue>();
            for (const auto& kv : *args[0].asDict())
                lst->push_back(kv.second);
            result = NeoValue::makeList(lst);
        } else {
            result = NeoValue::makeList(new std::vector<NeoValue>());
        }
        return true;
    }
    // ── has_key(dict, key) — check if dictionary has a key ────────
    if (name == "has_key") {
        if (args.size() == 2 && args[0].isDict() && args[0].asDict()) {
            std::string key = args[1].isString() ? args[1].asString() : args[1].toString();
            result = NeoValue::makeBool(args[0].asDict()->count(key) > 0);
        } else {
            result = NeoValue::makeBool(false);
        }
        return true;
    }

    // ── Phase 5: Statistics ────────────────────────────────────────
    if (name == "mean")        return callMean      (args, result);
    if (name == "median")      return callMedian    (args, result);
    if (name == "stddev")      return callStddev    (args, result);
    if (name == "variance")    return callVariance  (args, result);
    if (name == "sort")        return callSort      (args, result);
    if (name == "regress")     return callRegress   (args, result);
    if (name == "pdf_normal")  return callPdfNormal (args, result);
    if (name == "cdf_normal")  return callCdfNormal (args, result);
    if (name == "factorial")   return callFactorial (args, result);
    if (name == "ncr")         return callNcr       (args, result);
    if (name == "npr")         return callNpr       (args, result);

    // ── Phase 5: Units ─────────────────────────────────────────────
    if (name == "unit")        return callUnit      (args, result);
    if (name == "convert")     return callConvert   (args, result);

    // ── Phase 5: Advanced Calculus ────────────────────────────────
    if (name == "limit")       return callLimit     (args, result, sa);
    if (name == "taylor")      return callTaylor    (args, result, sa);
    if (name == "sum_expr" || name == "sigma")
                               return callSumExpr   (args, result, sa);
    if (name == "table")       return callTable     (args, result, env, sa);

    // ── Phase 6: Financial Math (TVM) ─────────────────────────────
    if (name == "tvm_pv")      return callTvmPV     (args, result);
    if (name == "tvm_fv")      return callTvmFV     (args, result);
    if (name == "tvm_pmt")     return callTvmPMT    (args, result);
    if (name == "tvm_n")       return callTvmN      (args, result);
    if (name == "tvm_ir")      return callTvmIR     (args, result);
    if (name == "amort_table") return callAmortTable(args, result);

    // ── Phase 6: Bitwise helpers ──────────────────────────────────
    if (name == "bit_get")     return callBitGet    (args, result);
    if (name == "bit_set")     return callBitSet    (args, result);
    if (name == "bit_clear")   return callBitClear  (args, result);
    if (name == "bit_toggle")  return callBitToggle (args, result);
    if (name == "bit_count")   return callBitCount  (args, result);
    if (name == "to_bin")      return callToBin     (args, result);
    if (name == "to_hex")      return callToHex     (args, result);

    // ── Phase 7: ODEs & Optimization ─────────────────────────────
    // (ndsolve/minimize/maximize are handled in NeoInterpreter::evalBuiltin
    //  because they need to call user-defined NeoLanguage functions)
    if (name == "gamma")       return callGamma     (args, result);
    if (name == "beta")        return callBeta      (args, result);
    if (name == "erf")         return callErf       (args, result);

    // ── Phase 7: File I/O ─────────────────────────────────────────
    if (name == "open")        return callOpen      (args, result);
    if (name == "read")        return callRead      (args, result);
    if (name == "write")       return callWrite     (args, result);
    if (name == "close")       return callClose     (args, result);
    if (name == "json_encode") return callJsonEncode(args, result);
    if (name == "json_decode") return callJsonDecode(args, result);
    if (name == "export_csv")  return callExportCsv (args, result);
    if (name == "import_csv")  return callImportCsv (args, result);

    // ── Phase 7: Physics Constants ────────────────────────────────
    if (name == "const")       return callConst     (args, result);
    if (name == "const_desc")  return callConstDesc (args, result);

    // ── Phase 7: NeoGUI ──────────────────────────────────────────
    if (name == "gui_label")   return callGuiLabel  (args, result);
    if (name == "gui_button")  return callGuiButton (args, result);
    if (name == "gui_slider")  return callGuiSlider (args, result);
    if (name == "gui_input")   return callGuiInput  (args, result);
    if (name == "gui_clear")   return callGuiClear  (args, result);
    if (name == "gui_show")    return callGuiShow   (args, result);

    // ── Phase 8: Signal Processing ───────────────────────────────
    if (name == "fft")          return callFFT         (args, result);
    if (name == "ifft")         return callIFFT        (args, result);
    if (name == "abs_spectrum") return callAbsSpectrum (args, result);

    // ── Phase 8: Advanced Linear Algebra ─────────────────────────
    if (name == "det")          return callDet  (args, result);
    if (name == "inv")          return callInv  (args, result);
    if (name == "eigen")        return callEigen(args, result);

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
// solve(expr, var)  OR  solve([eq1, eq2], [x, y])
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callSolve(const std::vector<NeoValue>& args,
                           NeoValue& result,
                           cas::SymExprArena& sa)
{
    if (args.size() < 2) {
        hostPrint("[solve] Usage: solve(expr, var) or solve([eq1,eq2,...], [x,y,...])\n");
        result = NeoValue::makeNull();
        return true;
    }

    // ── Multi-variable solver: solve([eq1, eq2], [x, y]) ──────────
    if (args[0].isList() && args[1].isList()
        && args[0].asList() && args[1].asList()) {

        const auto& eqs  = *args[0].asList();
        const auto& vars = *args[1].asList();
        size_t n = eqs.size();

        if (n == 0 || n != vars.size()) {
            hostPrint("[solve] Equation count must match variable count.\n");
            result = NeoValue::makeNull();
            return true;
        }

        // Extract symbolic residuals (each eq should be a Symbolic)
        std::vector<cas::SymExpr*> exprs;
        for (const NeoValue& eq : eqs) {
            if (!eq.isSymbolic()) {
                hostPrint("[solve] Each equation must be symbolic (use '==' with symbolic vars).\n");
                result = NeoValue::makeNull();
                return true;
            }
            exprs.push_back(eq.asSym());
        }

        // Extract variable chars
        std::vector<char> varChars;
        for (const NeoValue& v : vars) varChars.push_back(toVarChar(v, sa));

        if (n > 3) {
            hostPrint("[solve] Systems larger than 3×3 not yet supported.\n");
            result = NeoValue::makeNull();
            return true;
        }

        cas::SystemSolver sys;

        // ── 2-variable system ────────────────────────────────────
        if (n == 2) {
            cas::NLSystemResult res = sys.solveNonlinear2x2(
                exprs[0], exprs[1], varChars[0], varChars[1], sa);

            if (!res.ok || res.solutions.empty()) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "[solve] %s\n",
                    res.error.empty() ? "No solution found." : res.error.c_str());
                hostPrint(buf);
                result = NeoValue::makeNull();
                return true;
            }

            // Build result dictionary: {x: val, y: val}
            auto* dict = new std::map<std::string, NeoValue>();
            const cas::NLSolution& sol = res.solutions[0];
            std::string v1(1, varChars[0]), v2(1, varChars[1]);
            if (sol.exprX)
                (*dict)[v1] = NeoValue::makeSymbolic(sol.exprX);
            else
                (*dict)[v1] = NeoValue::makeNumber(sol.numX);
            if (sol.exprY)
                (*dict)[v2] = NeoValue::makeSymbolic(sol.exprY);
            else
                (*dict)[v2] = NeoValue::makeNumber(sol.numY);

            // Print solutions
            for (const auto& s : res.solutions) {
                char buf[256];
                if (s.exprX)
                    std::snprintf(buf, sizeof(buf), "%c = %s,  %c = %s\n",
                        varChars[0], s.exprX->toString().c_str(),
                        varChars[1], s.exprY ? s.exprY->toString().c_str() : "?");
                else
                    std::snprintf(buf, sizeof(buf), "%c = %.10g,  %c = %.10g\n",
                        varChars[0], s.numX, varChars[1], s.numY);
                hostPrint(buf);
            }
            result = NeoValue::makeDict(dict);
            return true;
        }

        // ── 3-variable system ────────────────────────────────────
        if (n == 3) {
            cas::NLSystemResult res = sys.solveNonlinear3x3(
                exprs[0], exprs[1], exprs[2],
                varChars[0], varChars[1], varChars[2], sa);

            if (!res.ok || res.solutions.empty()) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "[solve] %s\n",
                    res.error.empty() ? "No solution found." : res.error.c_str());
                hostPrint(buf);
                result = NeoValue::makeNull();
                return true;
            }

            auto* dict = new std::map<std::string, NeoValue>();
            const cas::NLSolution& sol = res.solutions[0];
            std::string v1(1, varChars[0]), v2(1, varChars[1]), v3(1, varChars[2]);
            (*dict)[v1] = sol.exprX ? NeoValue::makeSymbolic(sol.exprX) : NeoValue::makeNumber(sol.numX);
            (*dict)[v2] = sol.exprY ? NeoValue::makeSymbolic(sol.exprY) : NeoValue::makeNumber(sol.numY);
            (*dict)[v3] = sol.exprZ ? NeoValue::makeSymbolic(sol.exprZ) : NeoValue::makeNumber(sol.numZ);

            char buf[256];
            std::snprintf(buf, sizeof(buf), "%c = %.10g,  %c = %.10g,  %c = %.10g\n",
                varChars[0], sol.numX, varChars[1], sol.numY, varChars[2], sol.numZ);
            hostPrint(buf);
            result = NeoValue::makeDict(dict);
            return true;
        }
    }

    // ── Single-variable solver: solve(expr, var) ──────────────────
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
        case NeoValue::Type::Null:            typeName = "None";           ordinal = 0; break;
        case NeoValue::Type::Boolean:         typeName = "bool";           ordinal = 1; break;
        case NeoValue::Type::Number:          typeName = "number";         ordinal = 2; break;
        case NeoValue::Type::Exact:           typeName = "exact";          ordinal = 3; break;
        case NeoValue::Type::Symbolic:        typeName = "symbolic";       ordinal = 4; break;
        case NeoValue::Type::Function:        typeName = "function";       ordinal = 5; break;
        case NeoValue::Type::List:            typeName = "list";           ordinal = 6; break;
        case NeoValue::Type::NativeFunction:  typeName = "native_function";ordinal = 7; break;
        case NeoValue::Type::Quantity:        typeName = "quantity";       ordinal = 8; break;
        default:                              typeName = "unknown";        ordinal = -1; break;
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

// ════════════════════════════════════════════════════════════════════
// ── PHASE 5: STATISTICS ──────────────────────────────────────────
// ════════════════════════════════════════════════════════════════════

static const std::vector<NeoValue>* expectList(const std::vector<NeoValue>& args,
                                                size_t idx = 0) {
    if (args.size() <= idx) return nullptr;
    if (!args[idx].isList() || !args[idx].asList()) return nullptr;
    return args[idx].asList();
}

bool NeoStdLib::callMean(const std::vector<NeoValue>& args, NeoValue& result) {
    const auto* lst = expectList(args);
    if (!lst) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::mean(*lst));
    return true;
}

bool NeoStdLib::callMedian(const std::vector<NeoValue>& args, NeoValue& result) {
    const auto* lst = expectList(args);
    if (!lst) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::median(*lst));
    return true;
}

bool NeoStdLib::callStddev(const std::vector<NeoValue>& args, NeoValue& result) {
    const auto* lst = expectList(args);
    if (!lst) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::stddev(*lst));
    return true;
}

bool NeoStdLib::callVariance(const std::vector<NeoValue>& args, NeoValue& result) {
    const auto* lst = expectList(args);
    if (!lst) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::variance(*lst));
    return true;
}

bool NeoStdLib::callSort(const std::vector<NeoValue>& args, NeoValue& result) {
    const auto* lst = expectList(args);
    if (!lst) { result = NeoValue::makeNull(); return true; }
    std::vector<double> sorted = NeoStats::sortedCopy(*lst);
    auto* out = new std::vector<NeoValue>();
    out->reserve(sorted.size());
    for (double d : sorted) out->push_back(NeoValue::makeNumber(d));
    result = NeoValue::makeList(out);
    return true;
}

static NeoValue regressionPredict(const std::vector<NeoValue>& args,
                                   void* ctx,
                                   cas::SymExprArena& /*sa*/) {
    auto* model = static_cast<NeoRegModel*>(ctx);
    if (!model || args.empty()) return NeoValue::makeNull();
    double x = args[0].toDouble();
    return NeoValue::makeNumber(model->predict(x));
}

bool NeoStdLib::callRegress(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 3) {
        hostPrint("[regress] Usage: regress(x_list, y_list, model)\n");
        result = NeoValue::makeNull();
        return true;
    }
    const auto* xList = expectList(args, 0);
    const auto* yList = expectList(args, 1);
    if (!xList || !yList) {
        hostPrint("[regress] First two arguments must be lists.\n");
        result = NeoValue::makeNull();
        return true;
    }

    std::string modelStr = args[2].toString();
    if (modelStr.size() >= 2 && modelStr.front() == '"')
        modelStr = modelStr.substr(1, modelStr.size() - 2);

    NeoRegModel::Kind kind = NeoRegModel::Kind::Linear;
    if (modelStr == "quad")        kind = NeoRegModel::Kind::Quad;
    else if (modelStr == "exp")    kind = NeoRegModel::Kind::Exp;
    else if (modelStr == "log")    kind = NeoRegModel::Kind::Log;
    else if (modelStr != "linear") {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "[regress] Unknown model '%s'. Use: linear, quad, exp, log\n",
            modelStr.c_str());
        hostPrint(buf);
        result = NeoValue::makeNull();
        return true;
    }

    auto* model = new NeoRegModel();
    if (!NeoStats::regress(*xList, *yList, kind, *model)) {
        hostPrint("[regress] Regression failed (insufficient/invalid data).\n");
        delete model;
        result = NeoValue::makeNull();
        return true;
    }

    {
        char buf[256];
        const char* kindStr =
            (kind == NeoRegModel::Kind::Linear) ? "linear" :
            (kind == NeoRegModel::Kind::Quad)   ? "quad"   :
            (kind == NeoRegModel::Kind::Exp)     ? "exp"    : "log";
        if (kind == NeoRegModel::Kind::Quad) {
            std::snprintf(buf, sizeof(buf),
                "[regress] %s: a=%.6g  b=%.6g  c=%.6g  R2=%.4f\n",
                kindStr, model->a, model->b, model->c, model->r2);
        } else {
            std::snprintf(buf, sizeof(buf),
                "[regress] %s: a=%.6g  b=%.6g  R2=%.4f\n",
                kindStr, model->a, model->b, model->r2);
        }
        hostPrint(buf);
    }

    result = NeoValue::makeNativeFunction(regressionPredict, model);
    return true;
}

bool NeoStdLib::callPdfNormal(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 3) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(
        NeoStats::pdfNormal(args[0].toDouble(), args[1].toDouble(), args[2].toDouble()));
    return true;
}

bool NeoStdLib::callCdfNormal(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 3) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(
        NeoStats::cdfNormal(args[0].toDouble(), args[1].toDouble(), args[2].toDouble()));
    return true;
}

bool NeoStdLib::callFactorial(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::factorial(
        static_cast<int>(args[0].toDouble())));
    return true;
}

bool NeoStdLib::callNcr(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::ncr(
        static_cast<int>(args[0].toDouble()),
        static_cast<int>(args[1].toDouble())));
    return true;
}

bool NeoStdLib::callNpr(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoStats::npr(
        static_cast<int>(args[0].toDouble()),
        static_cast<int>(args[1].toDouble())));
    return true;
}

// ════════════════════════════════════════════════════════════════════
// ── PHASE 5: UNITS ───────────────────────────────────────────────
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callUnit(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) {
        hostPrint("[unit] Usage: unit(value, \"unit_str\")\n");
        result = NeoValue::makeNull();
        return true;
    }
    double value = args[0].toDouble();
    std::string unitStr = args[1].toString();
    if (unitStr.size() >= 2 && unitStr.front() == '"')
        unitStr = unitStr.substr(1, unitStr.size() - 2);

    auto* q = new NeoQuantity();
    if (!NeoUnits::makeQuantity(value, unitStr, *q)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[unit] Unknown unit: '%s'\n", unitStr.c_str());
        hostPrint(buf);
        delete q;
        result = NeoValue::makeNull();
        return true;
    }
    result = NeoValue::makeQuantity(q);
    return true;
}

bool NeoStdLib::callConvert(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) {
        hostPrint("[convert] Usage: convert(quantity, \"unit_str\")\n");
        result = NeoValue::makeNull();
        return true;
    }
    if (!args[0].isQuantity() || !args[0].asQuantity()) {
        hostPrint("[convert] First argument must be a Quantity.\n");
        result = NeoValue::makeNull();
        return true;
    }
    std::string targetStr = args[1].toString();
    if (targetStr.size() >= 2 && targetStr.front() == '"')
        targetStr = targetStr.substr(1, targetStr.size() - 2);

    auto* out = new NeoQuantity();
    if (!NeoUnits::convert(*args[0].asQuantity(), targetStr, *out)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "[convert] Cannot convert to '%s'\n", targetStr.c_str());
        hostPrint(buf);
        delete out;
        result = NeoValue::makeNull();
        return true;
    }
    result = NeoValue::makeQuantity(out);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// ── PHASE 5: ADVANCED CALCULUS ───────────────────────────────────
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callLimit(const std::vector<NeoValue>& args,
                           NeoValue& result,
                           cas::SymExprArena& sa)
{
    if (args.size() < 3) {
        hostPrint("[limit] Usage: limit(expr, var, point)\n");
        result = NeoValue::makeNull();
        return true;
    }
    cas::SymExpr* expr = toSym(args[0], sa);
    if (!expr) { result = args[0]; return true; }
    char   var = toVarChar(args[1], sa);
    double pt  = args[2].toDouble();

    static constexpr double H = 1e-4;
    double f1p = expr->evaluate(pt + H);
    double f1m = expr->evaluate(pt - H);
    double f2p = expr->evaluate(pt + H * 0.5);
    double f2m = expr->evaluate(pt - H * 0.5);

    double lim1 = (f1p + f1m) * 0.5;
    double lim2 = (f2p + f2m) * 0.5;
    double lim  = (4.0 * lim2 - lim1) / 3.0;

    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "[limit] lim(%c->%.6g) = %.10g\n", var, pt, lim);
    hostPrint(buf);

    result = NeoValue::makeNumber(lim);
    return true;
}

bool NeoStdLib::callTaylor(const std::vector<NeoValue>& args,
                            NeoValue& result,
                            cas::SymExprArena& sa)
{
    if (args.size() < 4) {
        hostPrint("[taylor] Usage: taylor(expr, var, point, order)\n");
        result = NeoValue::makeNull();
        return true;
    }
    cas::SymExpr* expr = toSym(args[0], sa);
    if (!expr) { result = args[0]; return true; }
    char   var   = toVarChar(args[1], sa);
    double point = args[2].toDouble();
    int    order = static_cast<int>(args[3].toDouble());
    if (order < 0) order = 0;
    if (order > 10) order = 10;

    // Build polynomial as a NeoValue list of coefficients [c0, c1, ..., cn]
    // where the polynomial = sum c_k * (x - point)^k
    auto* coeffs = new std::vector<NeoValue>();
    coeffs->reserve(static_cast<size_t>(order + 1));

    cas::SymExpr* currentDeriv = expr;
    double factorial = 1.0;

    for (int k = 0; k <= order; ++k) {
        if (k > 0) {
            cas::SymExpr* d = cas::SymDiff::diff(currentDeriv, var, sa);
            if (!d) { coeffs->push_back(NeoValue::makeNumber(0.0)); continue; }
            currentDeriv = cas::SymSimplify::simplify(d, sa);
            if (!currentDeriv) currentDeriv = d;
            factorial *= static_cast<double>(k);
        }
        double coef = currentDeriv->evaluate(point) / factorial;
        coeffs->push_back(NeoValue::makeNumber(
            (std::isnan(coef) || std::isinf(coef)) ? 0.0 : coef));
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "[taylor] order-%d Taylor coefficients around %c=%g\n",
        order, var, point);
    hostPrint(buf);

    result = NeoValue::makeList(coeffs);
    return true;
}

bool NeoStdLib::callSumExpr(const std::vector<NeoValue>& args,
                              NeoValue& result,
                              cas::SymExprArena& sa)
{
    if (args.size() < 4) {
        hostPrint("[sigma] Usage: sigma(expr, var, start, end)\n");
        result = NeoValue::makeNull();
        return true;
    }
    cas::SymExpr* expr = toSym(args[0], sa);
    char   var   = toVarChar(args[1], sa);
    int    start = static_cast<int>(args[2].toDouble());
    int    end   = static_cast<int>(args[3].toDouble());
    if (end - start > 10000) end = start + 10000;

    double total = 0.0;
    for (int i = start; i <= end; ++i) {
        if (expr) total += expr->evaluate(static_cast<double>(i));
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "[sigma] sum(%c=%d..%d) = %.10g\n", var, start, end, total);
    hostPrint(buf);

    result = NeoValue::makeNumber(total);
    return true;
}

bool NeoStdLib::callTable(const std::vector<NeoValue>& args,
                           NeoValue& result,
                           NeoEnv& /*env*/,
                           cas::SymExprArena& sa)
{
    if (args.size() < 4) {
        hostPrint("[table] Usage: table(func, start, end, step)\n");
        result = NeoValue::makeNull();
        return true;
    }
    const NeoValue& funcVal = args[0];
    double xstart = args[1].toDouble();
    double xend   = args[2].toDouble();
    double step   = args[3].toDouble();

    if (step == 0.0 || std::fabs((xend - xstart) / step) > 10001.0) {
        hostPrint("[table] Step too small or range too large.\n");
        result = NeoValue::makeNull();
        return true;
    }

    auto* out = new std::vector<NeoValue>();
    int maxPts = static_cast<int>(std::fabs((xend - xstart) / step)) + 2;
    if (maxPts > 10002) maxPts = 10002;
    out->reserve(static_cast<size_t>(maxPts));

    for (double x = xstart;
         (step > 0 ? x <= xend + step * 0.5 : x >= xend + step * 0.5);
         x += step)
    {
        double y = std::numeric_limits<double>::quiet_NaN();
        if (funcVal.isNativeFunction() && funcVal.nativeFn()) {
            std::vector<NeoValue> callArgs = { NeoValue::makeNumber(x) };
            NeoValue r = funcVal.nativeFn()(callArgs, funcVal.nativeCtx(), sa);
            y = r.toDouble();
        }
        auto* row = new std::vector<NeoValue>();
        row->push_back(NeoValue::makeNumber(x));
        row->push_back(NeoValue::makeNumber(y));
        out->push_back(NeoValue::makeList(row));
    }

    result = NeoValue::makeList(out);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 6: Financial Math — TVM Engine
// ════════════════════════════════════════════════════════════════════

// Helper to extract double arg with default value
static double tvmArg(const std::vector<NeoValue>& args, size_t idx, double def = 0.0) {
    if (idx < args.size()) return args[idx].toDouble();
    return def;
}

bool NeoStdLib::callTvmPV(const std::vector<NeoValue>& args, NeoValue& result) {
    // tvm_pv(rate_pct, n, pmt, fv=0)
    if (args.size() < 3) { hostPrint("[tvm_pv] Usage: tvm_pv(rate, n, pmt, fv=0)\n"); result = NeoValue::makeNull(); return true; }
    double r = tvmArg(args,0), n = tvmArg(args,1), pmt = tvmArg(args,2), fv = tvmArg(args,3,0);
    result = NeoValue::makeNumber(NeoFinance::solvePV(r, n, pmt, fv));
    return true;
}

bool NeoStdLib::callTvmFV(const std::vector<NeoValue>& args, NeoValue& result) {
    // tvm_fv(rate_pct, n, pmt, pv=0)
    if (args.size() < 3) { hostPrint("[tvm_fv] Usage: tvm_fv(rate, n, pmt, pv=0)\n"); result = NeoValue::makeNull(); return true; }
    double r = tvmArg(args,0), n = tvmArg(args,1), pmt = tvmArg(args,2), pv = tvmArg(args,3,0);
    result = NeoValue::makeNumber(NeoFinance::solveFV(r, n, pmt, pv));
    return true;
}

bool NeoStdLib::callTvmPMT(const std::vector<NeoValue>& args, NeoValue& result) {
    // tvm_pmt(rate_pct, n, pv, fv=0)
    if (args.size() < 3) { hostPrint("[tvm_pmt] Usage: tvm_pmt(rate, n, pv, fv=0)\n"); result = NeoValue::makeNull(); return true; }
    double r = tvmArg(args,0), n = tvmArg(args,1), pv = tvmArg(args,2), fv = tvmArg(args,3,0);
    result = NeoValue::makeNumber(NeoFinance::solvePMT(r, n, pv, fv));
    return true;
}

bool NeoStdLib::callTvmN(const std::vector<NeoValue>& args, NeoValue& result) {
    // tvm_n(rate_pct, pmt, pv, fv=0)
    if (args.size() < 3) { hostPrint("[tvm_n] Usage: tvm_n(rate, pmt, pv, fv=0)\n"); result = NeoValue::makeNull(); return true; }
    double r = tvmArg(args,0), pmt = tvmArg(args,1), pv = tvmArg(args,2), fv = tvmArg(args,3,0);
    result = NeoValue::makeNumber(NeoFinance::solveN(r, pmt, pv, fv));
    return true;
}

bool NeoStdLib::callTvmIR(const std::vector<NeoValue>& args, NeoValue& result) {
    // tvm_ir(n, pmt, pv, fv=0)
    if (args.size() < 3) { hostPrint("[tvm_ir] Usage: tvm_ir(n, pmt, pv, fv=0)\n"); result = NeoValue::makeNull(); return true; }
    double n = tvmArg(args,0), pmt = tvmArg(args,1), pv = tvmArg(args,2), fv = tvmArg(args,3,0);
    result = NeoValue::makeNumber(NeoFinance::solveIR(n, pmt, pv, fv));
    return true;
}

bool NeoStdLib::callAmortTable(const std::vector<NeoValue>& args, NeoValue& result) {
    // amort_table(principal, rate_pct, periods)
    if (args.size() < 3) { hostPrint("[amort_table] Usage: amort_table(principal, rate_pct, periods)\n"); result = NeoValue::makeNull(); return true; }
    double principal = tvmArg(args,0);
    double rate_pct  = tvmArg(args,1);
    int    periods   = static_cast<int>(tvmArg(args,2));
    if (periods <= 0 || periods > 600) { hostPrint("[amort_table] periods must be 1-600\n"); result = NeoValue::makeNull(); return true; }
    result = NeoFinance::amortTable(principal, rate_pct, periods);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 6: Bitwise Helpers
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callBitGet(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { result = NeoValue::makeNumber(0); return true; }
    result = NeoBitwise::bitGet(args[0], args[1]);
    return true;
}

bool NeoStdLib::callBitSet(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { result = NeoValue::makeNumber(0); return true; }
    result = NeoBitwise::bitSet(args[0], args[1]);
    return true;
}

bool NeoStdLib::callBitClear(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { result = NeoValue::makeNumber(0); return true; }
    result = NeoBitwise::bitClear(args[0], args[1]);
    return true;
}

bool NeoStdLib::callBitToggle(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { result = NeoValue::makeNumber(0); return true; }
    result = NeoBitwise::bitToggle(args[0], args[1]);
    return true;
}

bool NeoStdLib::callBitCount(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { result = NeoValue::makeNumber(0); return true; }
    result = NeoBitwise::bitCount(args[0]);
    return true;
}

bool NeoStdLib::callToBin(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { result = NeoValue::makeString("0b0"); return true; }
    result = NeoBitwise::toBin(args[0]);
    return true;
}

bool NeoStdLib::callToHex(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { result = NeoValue::makeString("0x0"); return true; }
    result = NeoBitwise::toHex(args[0]);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 7: Special Functions (NeoScientific)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callGamma(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[gamma] Usage: gamma(x)\n"); result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoScientific::gamma(args[0].toDouble()));
    return true;
}

bool NeoStdLib::callBeta(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { hostPrint("[beta] Usage: beta(x, y)\n"); result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoScientific::beta(args[0].toDouble(), args[1].toDouble()));
    return true;
}

bool NeoStdLib::callErf(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[erf] Usage: erf(x)\n"); result = NeoValue::makeNull(); return true; }
    result = NeoValue::makeNumber(NeoScientific::erf(args[0].toDouble()));
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 7: File I/O (NeoIO)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callOpen(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { hostPrint("[open] Usage: open(path, mode)\n"); result = NeoValue::makeNumber(-1); return true; }
    std::string path = args[0].isString() ? args[0].asString() : args[0].toString();
    std::string mode = args[1].isString() ? args[1].asString() : args[1].toString();
    int h = NeoIO::openFile(path.c_str(), mode.c_str());
    result = NeoValue::makeNumber(static_cast<double>(h));
    return true;
}

bool NeoStdLib::callRead(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[read] Usage: read(handle)\n"); result = NeoValue::makeString(""); return true; }
    int h = static_cast<int>(args[0].toDouble());
    result = NeoValue::makeString(NeoIO::readFile(h));
    return true;
}

bool NeoStdLib::callWrite(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { hostPrint("[write] Usage: write(handle, str)\n"); result = NeoValue::makeNumber(-1); return true; }
    int h = static_cast<int>(args[0].toDouble());
    std::string text = args[1].isString() ? args[1].asString() : args[1].toString();
    int n = NeoIO::writeFile(h, text.c_str());
    result = NeoValue::makeNumber(static_cast<double>(n));
    return true;
}

bool NeoStdLib::callClose(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[close] Usage: close(handle)\n"); result = NeoValue::makeNull(); return true; }
    int h = static_cast<int>(args[0].toDouble());
    NeoIO::closeFile(h);
    result = NeoValue::makeNull();
    return true;
}

bool NeoStdLib::callJsonEncode(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { result = NeoValue::makeString("null"); return true; }
    result = NeoValue::makeString(NeoIO::jsonEncode(args[0]));
    return true;
}

bool NeoStdLib::callJsonDecode(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { result = NeoValue::makeNull(); return true; }
    std::string s = args[0].isString() ? args[0].asString() : args[0].toString();
    result = NeoIO::jsonDecode(s);
    return true;
}

bool NeoStdLib::callExportCsv(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { hostPrint("[export_csv] Usage: export_csv(matrix, path)\n"); result = NeoValue::makeBool(false); return true; }
    std::string path = args[1].isString() ? args[1].asString() : args[1].toString();
    bool ok = NeoIO::exportCsv(args[0], path.c_str());
    result = NeoValue::makeBool(ok);
    return true;
}

bool NeoStdLib::callImportCsv(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[import_csv] Usage: import_csv(path)\n"); result = NeoValue::makeNull(); return true; }
    std::string path = args[0].isString() ? args[0].asString() : args[0].toString();
    result = NeoIO::importCsv(path.c_str());
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 7: Physics Constants (NeoPhysics)
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callConst(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[const] Usage: const(\"name\")\n"); result = NeoValue::makeNull(); return true; }
    std::string name = args[0].isString() ? args[0].asString() : args[0].toString();
    result = NeoPhysics::get(name.c_str());
    if (result.isNull()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[const] Unknown constant: '%s'\n", name.c_str());
        hostPrint(buf);
    }
    return true;
}

bool NeoStdLib::callConstDesc(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[const_desc] Usage: const_desc(\"name\")\n"); result = NeoValue::makeString(""); return true; }
    std::string name = args[0].isString() ? args[0].asString() : args[0].toString();
    std::string desc = NeoPhysics::describe(name.c_str());
    result = NeoValue::makeString(desc);
    if (desc.empty()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[const_desc] Unknown constant: '%s'\n", name.c_str());
        hostPrint(buf);
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 7: NeoGUI
// ════════════════════════════════════════════════════════════════════

bool NeoStdLib::callGuiLabel(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) { hostPrint("[gui_label] Usage: gui_label(text)\n"); result = NeoValue::makeNull(); return true; }
    std::string text = args[0].isString() ? args[0].asString() : args[0].toString();
    NeoGUI::addLabel(text);
    result = NeoValue::makeNull();
    return true;
}

bool NeoStdLib::callGuiButton(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 2) { hostPrint("[gui_button] Usage: gui_button(label, callback)\n"); result = NeoValue::makeNull(); return true; }
    std::string label    = args[0].isString() ? args[0].asString() : args[0].toString();
    std::string callback = args[1].isString() ? args[1].asString() : args[1].toString();
    NeoGUI::addButton(label, callback);
    result = NeoValue::makeNull();
    return true;
}

bool NeoStdLib::callGuiSlider(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.size() < 3) { hostPrint("[gui_slider] Usage: gui_slider(min, max, callback)\n"); result = NeoValue::makeNull(); return true; }
    double      minv     = args[0].toDouble();
    double      maxv     = args[1].toDouble();
    std::string callback = args[2].isString() ? args[2].asString() : args[2].toString();
    NeoGUI::addSlider(minv, maxv, callback);
    result = NeoValue::makeNull();
    return true;
}

bool NeoStdLib::callGuiInput(const std::vector<NeoValue>& args, NeoValue& result) {
    std::string label = args.empty() ? "Input" :
                        (args[0].isString() ? args[0].asString() : args[0].toString());
    NeoGUI::addInput(label);
    result = NeoValue::makeNull();
    return true;
}

bool NeoStdLib::callGuiClear(const std::vector<NeoValue>& /*args*/, NeoValue& result) {
    NeoGUI::clearComponents();
    result = NeoValue::makeNull();
    return true;
}

bool NeoStdLib::callGuiShow(const std::vector<NeoValue>& /*args*/, NeoValue& result) {
    // The host polls NeoGUI::isDirty() and calls NeoGUI::flush() to render.
    // gui_show() just ensures the dirty flag is set.
    NeoGUI::guiDirty() = true;
    hostPrint("[gui] GUI queued for display.\n");
    result = NeoValue::makeNull();
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 8: Signal Processing — FFT / IFFT
// ════════════════════════════════════════════════════════════════════

// ── Internal Cooley-Tukey helper ──────────────────────────────────

static void neostdlib_fftInPlace(std::vector<double>& re,
                                  std::vector<double>& im,
                                  bool inverse)
{
    size_t M = re.size();
    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < M; ++i) {
        size_t bit = M >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    // Butterfly stages
    double sign = inverse ? 1.0 : -1.0;
    for (size_t len = 2; len <= M; len <<= 1) {
        double ang  = sign * 2.0 * M_PI / static_cast<double>(len);
        double wRe  = std::cos(ang), wIm = std::sin(ang);
        for (size_t i = 0; i < M; i += len) {
            double curRe = 1.0, curIm = 0.0;
            for (size_t k = 0; k < len / 2; ++k) {
                double uRe = re[i + k], uIm = im[i + k];
                double tRe = curRe * re[i + k + len/2] - curIm * im[i + k + len/2];
                double tIm = curRe * im[i + k + len/2] + curIm * re[i + k + len/2];
                re[i + k]           = uRe + tRe;  im[i + k]           = uIm + tIm;
                re[i + k + len / 2] = uRe - tRe;  im[i + k + len / 2] = uIm - tIm;
                double tmp = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = tmp;
            }
        }
    }
    if (inverse) {
        double invN = 1.0 / static_cast<double>(M);
        for (size_t i = 0; i < M; ++i) { re[i] *= invN; im[i] *= invN; }
    }
}

bool NeoStdLib::callFFT(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty() || !args[0].isList() || !args[0].asList()) {
        hostPrint("[fft] Usage: fft(list)\n");
        result = NeoValue::makeList(new std::vector<NeoValue>());
        return true;
    }
    const auto& in = *args[0].asList();
    size_t N = in.size();
    size_t M = 1;
    while (M < N) M <<= 1;

    std::vector<double> re(M, 0.0), im(M, 0.0);
    for (size_t i = 0; i < N; ++i) {
        if (in[i].isList() && in[i].asList() && in[i].asList()->size() >= 2) {
            re[i] = (*in[i].asList())[0].toDouble();
            im[i] = (*in[i].asList())[1].toDouble();
        } else {
            re[i] = in[i].toDouble();
        }
    }
    neostdlib_fftInPlace(re, im, false);

    auto* out_lst = new std::vector<NeoValue>();
    out_lst->reserve(M);
    for (size_t i = 0; i < M; ++i) {
        auto* pair = new std::vector<NeoValue>();
        pair->push_back(NeoValue::makeNumber(re[i]));
        pair->push_back(NeoValue::makeNumber(im[i]));
        out_lst->push_back(NeoValue::makeList(pair));
    }
    result = NeoValue::makeList(out_lst);
    return true;
}

bool NeoStdLib::callIFFT(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty() || !args[0].isList() || !args[0].asList()) {
        hostPrint("[ifft] Usage: ifft(fft_result)\n");
        result = NeoValue::makeList(new std::vector<NeoValue>());
        return true;
    }
    const auto& in = *args[0].asList();
    size_t N = in.size();
    size_t M = 1;
    while (M < N) M <<= 1;

    std::vector<double> re(M, 0.0), im(M, 0.0);
    for (size_t i = 0; i < N; ++i) {
        if (in[i].isList() && in[i].asList() && in[i].asList()->size() >= 2) {
            re[i] = (*in[i].asList())[0].toDouble();
            im[i] = (*in[i].asList())[1].toDouble();
        } else {
            re[i] = in[i].toDouble();
        }
    }
    neostdlib_fftInPlace(re, im, true);

    auto* out_lst = new std::vector<NeoValue>();
    out_lst->reserve(M);
    for (size_t i = 0; i < M; ++i) {
        auto* pair = new std::vector<NeoValue>();
        pair->push_back(NeoValue::makeNumber(re[i]));
        pair->push_back(NeoValue::makeNumber(im[i]));
        out_lst->push_back(NeoValue::makeList(pair));
    }
    result = NeoValue::makeList(out_lst);
    return true;
}

bool NeoStdLib::callAbsSpectrum(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty() || !args[0].isList() || !args[0].asList()) {
        result = NeoValue::makeList(new std::vector<NeoValue>());
        return true;
    }
    const auto& in = *args[0].asList();
    auto* out_lst = new std::vector<NeoValue>();
    out_lst->reserve(in.size());
    for (const auto& v : in) {
        if (v.isList() && v.asList() && v.asList()->size() >= 2) {
            double r  = (*v.asList())[0].toDouble();
            double im_part = (*v.asList())[1].toDouble();
            out_lst->push_back(NeoValue::makeNumber(std::sqrt(r * r + im_part * im_part)));
        } else {
            out_lst->push_back(NeoValue::makeNumber(std::fabs(v.toDouble())));
        }
    }
    result = NeoValue::makeList(out_lst);
    return true;
}

// ════════════════════════════════════════════════════════════════════
// Phase 8: Advanced Linear Algebra — det, inv, eigen
// ════════════════════════════════════════════════════════════════════

// ── Extract matrix from NeoValue (list of lists) ─────────────────

static bool extractMatrix(const NeoValue& v,
                           std::vector<std::vector<double>>& M,
                           int& rows, int& cols)
{
    if (!v.isList() || !v.asList()) return false;
    const auto& outer = *v.asList();
    rows = static_cast<int>(outer.size());
    if (rows == 0) return false;
    // If first element is a plain number → treat as 1×n row vector
    if (!outer[0].isList()) {
        cols = rows;
        rows = 1;
        M.resize(1, std::vector<double>(static_cast<size_t>(cols), 0.0));
        for (int j = 0; j < cols; ++j) M[0][j] = outer[j].toDouble();
        return true;
    }
    // List of lists
    cols = static_cast<int>(outer[0].asList() ? outer[0].asList()->size() : 0);
    if (cols == 0) return false;
    M.resize(static_cast<size_t>(rows),
             std::vector<double>(static_cast<size_t>(cols), 0.0));
    for (int i = 0; i < rows; ++i) {
        if (!outer[i].isList() || !outer[i].asList()) continue;
        const auto& row = *outer[i].asList();
        for (int j = 0; j < cols && j < static_cast<int>(row.size()); ++j) {
            M[static_cast<size_t>(i)][static_cast<size_t>(j)] = row[j].toDouble();
        }
    }
    return true;
}

static NeoValue matrixToNeoValue(const std::vector<std::vector<double>>& M) {
    auto* outer = new std::vector<NeoValue>();
    for (const auto& row : M) {
        auto* r = new std::vector<NeoValue>();
        for (double v : row) r->push_back(NeoValue::makeNumber(v));
        outer->push_back(NeoValue::makeList(r));
    }
    return NeoValue::makeList(outer);
}

// ── Determinant via Gaussian elimination ─────────────────────────

bool NeoStdLib::callDet(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) {
        hostPrint("[det] Usage: det(matrix)\n");
        result = NeoValue::makeNumber(0);
        return true;
    }
    std::vector<std::vector<double>> M;
    int rows, cols;
    if (!extractMatrix(args[0], M, rows, cols) || rows != cols) {
        hostPrint("[det] Matrix must be square.\n");
        result = NeoValue::makeNumber(0);
        return true;
    }
    int n = rows;
    double det = 1.0;
    // Forward elimination with partial pivoting
    for (int col = 0; col < n; ++col) {
        // Find pivot
        int pivotRow = col;
        double maxVal = std::fabs(M[static_cast<size_t>(col)][static_cast<size_t>(col)]);
        for (int r = col + 1; r < n; ++r) {
            double v = std::fabs(M[static_cast<size_t>(r)][static_cast<size_t>(col)]);
            if (v > maxVal) { maxVal = v; pivotRow = r; }
        }
        if (maxVal < 1e-14) { result = NeoValue::makeNumber(0.0); return true; }
        if (pivotRow != col) {
            std::swap(M[static_cast<size_t>(col)], M[static_cast<size_t>(pivotRow)]);
            det = -det; // Row swap flips sign
        }
        double pivot = M[static_cast<size_t>(col)][static_cast<size_t>(col)];
        det *= pivot;
        for (int r = col + 1; r < n; ++r) {
            double factor = M[static_cast<size_t>(r)][static_cast<size_t>(col)] / pivot;
            for (int c = col; c < n; ++c) {
                M[static_cast<size_t>(r)][static_cast<size_t>(c)]
                    -= factor * M[static_cast<size_t>(col)][static_cast<size_t>(c)];
            }
        }
    }
    result = NeoValue::makeNumber(det);
    return true;
}

// ── Inverse via Gauss-Jordan on augmented [A | I] ────────────────

bool NeoStdLib::callInv(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) {
        hostPrint("[inv] Usage: inv(matrix)\n");
        result = NeoValue::makeNull();
        return true;
    }
    std::vector<std::vector<double>> A;
    int rows, cols;
    if (!extractMatrix(args[0], A, rows, cols) || rows != cols) {
        hostPrint("[inv] Matrix must be square.\n");
        result = NeoValue::makeNull();
        return true;
    }
    int n = rows;
    // Build augmented matrix [A | I]
    std::vector<std::vector<double>> aug(static_cast<size_t>(n),
        std::vector<double>(static_cast<size_t>(2 * n), 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            aug[static_cast<size_t>(i)][static_cast<size_t>(j)] = A[static_cast<size_t>(i)][static_cast<size_t>(j)];
        aug[static_cast<size_t>(i)][static_cast<size_t>(n + i)] = 1.0; // Identity on right
    }
    // Forward pass
    for (int col = 0; col < n; ++col) {
        // Partial pivot
        int pivotRow = col;
        double maxVal = std::fabs(aug[static_cast<size_t>(col)][static_cast<size_t>(col)]);
        for (int r = col + 1; r < n; ++r) {
            double v = std::fabs(aug[static_cast<size_t>(r)][static_cast<size_t>(col)]);
            if (v > maxVal) { maxVal = v; pivotRow = r; }
        }
        if (maxVal < 1e-14) {
            hostPrint("[inv] Matrix is singular.\n");
            result = NeoValue::makeNull();
            return true;
        }
        if (pivotRow != col)
            std::swap(aug[static_cast<size_t>(col)], aug[static_cast<size_t>(pivotRow)]);
        double pivot = aug[static_cast<size_t>(col)][static_cast<size_t>(col)];
        for (int c = 0; c < 2 * n; ++c)
            aug[static_cast<size_t>(col)][static_cast<size_t>(c)] /= pivot;
        for (int r = 0; r < n; ++r) {
            if (r == col) continue;
            double factor = aug[static_cast<size_t>(r)][static_cast<size_t>(col)];
            for (int c = 0; c < 2 * n; ++c)
                aug[static_cast<size_t>(r)][static_cast<size_t>(c)]
                    -= factor * aug[static_cast<size_t>(col)][static_cast<size_t>(c)];
        }
    }
    // Extract inverse from right half
    std::vector<std::vector<double>> inv(static_cast<size_t>(n),
        std::vector<double>(static_cast<size_t>(n), 0.0));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            inv[static_cast<size_t>(i)][static_cast<size_t>(j)]
                = aug[static_cast<size_t>(i)][static_cast<size_t>(n + j)];
    result = matrixToNeoValue(inv);
    return true;
}

// ── Eigenvalues via power iteration (dominant) + deflation ───────

bool NeoStdLib::callEigen(const std::vector<NeoValue>& args, NeoValue& result) {
    if (args.empty()) {
        hostPrint("[eigen] Usage: eigen(matrix)\n");
        result = NeoValue::makeNull();
        return true;
    }
    std::vector<std::vector<double>> A;
    int rows, cols;
    if (!extractMatrix(args[0], A, rows, cols) || rows != cols) {
        hostPrint("[eigen] Matrix must be square.\n");
        result = NeoValue::makeNull();
        return true;
    }
    int n = rows;

    // Power iteration: find the dominant eigenvalue and eigenvector
    // Returns a dict: {"values": [λ1], "vectors": [[v1]]}
    // For simplicity and memory safety on ESP32 we compute only the
    // dominant eigenvalue via power iteration and Rayleigh quotient.
    static constexpr int MAX_ITER = 500;
    static constexpr double TOL   = 1e-9;

    // Build result structures
    auto* eigenvals = new std::vector<NeoValue>();
    auto* eigenvecs = new std::vector<NeoValue>();

    std::vector<std::vector<double>> B = A; // Working copy for deflation

    for (int eigIdx = 0; eigIdx < n && eigIdx < 4; ++eigIdx) {
        // Power iteration on B
        std::vector<double> v(static_cast<size_t>(n), 0.0);
        v[static_cast<size_t>(eigIdx % n)] = 1.0; // Start vector
        double lambda = 0.0;

        for (int iter = 0; iter < MAX_ITER; ++iter) {
            // w = B * v
            std::vector<double> w(static_cast<size_t>(n), 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    w[static_cast<size_t>(i)] += B[static_cast<size_t>(i)][static_cast<size_t>(j)]
                                               * v[static_cast<size_t>(j)];
            // Normalise
            double norm = 0.0;
            for (double x : w) norm += x * x;
            norm = std::sqrt(norm);
            if (norm < 1e-15) break;
            // Rayleigh quotient: λ = vᵀ * B * v / vᵀ * v
            double newLambda = 0.0;
            for (int i = 0; i < n; ++i) newLambda += v[static_cast<size_t>(i)] * w[static_cast<size_t>(i)];
            for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = w[static_cast<size_t>(i)] / norm;
            if (std::fabs(newLambda - lambda) < TOL && iter > 10) { lambda = newLambda; break; }
            lambda = newLambda;
        }

        eigenvals->push_back(NeoValue::makeNumber(lambda));

        // Store eigenvector as a list
        auto* vec = new std::vector<NeoValue>();
        for (int i = 0; i < n; ++i) vec->push_back(NeoValue::makeNumber(v[static_cast<size_t>(i)]));
        eigenvecs->push_back(NeoValue::makeList(vec));

        // Deflate: B = B - λ * v * vᵀ  (removes the dominant eigenvalue)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                B[static_cast<size_t>(i)][static_cast<size_t>(j)]
                    -= lambda * v[static_cast<size_t>(i)] * v[static_cast<size_t>(j)];
    }

    // Return {"values": [λ...], "vectors": [[v1], [v2], ...]}
    auto* dict = new std::map<std::string, NeoValue>();
    (*dict)["values"]  = NeoValue::makeList(eigenvals);
    (*dict)["vectors"] = NeoValue::makeList(eigenvecs);
    result = NeoValue::makeDict(dict);
    return true;
}
