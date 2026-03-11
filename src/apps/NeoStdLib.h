/**
 * NeoStdLib.h — Standard Library for NeoLanguage Phase 3.
 *
 * Provides built-in functions that expose the NumOS CAS/graphics power
 * to NeoLanguage programs. All function names are lowercase for easy
 * calculator-keyboard entry.
 *
 * Built-in functions:
 *
 *   Symbolic Calculus & Algebra:
 *     diff(expr, var)       — symbolic differentiation (uses SymDiff)
 *     integrate(expr, var)  — symbolic indefinite integral (uses SymIntegrate)
 *     solve(expr, var)      — find roots of expr=0 (uses OmniSolver)
 *     simplify(expr)        — algebraic simplification (uses SymSimplify)
 *     expand(expr)          — polynomial expansion (distribute rule)
 *
 *   High-Level Graphics:
 *     plot(expr, xmin, xmax) — enter Graphics Mode and render function plot.
 *                              Evaluates via SymExpr::evaluate() — decoupled
 *                              from GrapherApp's string evaluator so upgrades
 *                              to GrapherApp do not affect NeoLanguage.
 *     clear_plot()           — clear the current plot overlay.
 *
 *   System & Utilities:
 *     print(args...)        — send text to the console (via host callback).
 *     type(val)             — display and return the type of a value.
 *     vars()                — list all variables in the global environment.
 *
 * Host Callbacks:
 *   NeoStdLib is pure C++ with no LVGL/Arduino dependency.  It drives
 *   I/O through a NeoHostCallbacks struct supplied by the host app
 *   (NeoLanguageApp).  This decouples the standard library from the UI
 *   and makes it testable on any platform.
 *
 * Usage:
 *   1. NeoLanguageApp fills a NeoHostCallbacks struct with its own lambdas.
 *   2. NeoInterpreter owns a NeoStdLib and calls setCallbacks().
 *   3. evalBuiltin() delegates to NeoStdLib::callBuiltin() after
 *      checking its own math primitives (sin, cos, etc.).
 *
 * File extension note: NeoLanguage source files use the .nl extension.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 3 (Standard Library)
 */
#pragma once

#include <string>
#include <vector>
#include "../math/cas/SymExprArena.h"
#include "../math/cas/SymExpr.h"
#include "NeoValue.h"
#include "NeoEnv.h"

// ════════════════════════════════════════════════════════════════════
// NeoPlotRequest — data produced by plot() for the host to render
// ════════════════════════════════════════════════════════════════════

/**
 * Represents a pending plot request produced when the NeoLanguage
 * `plot(expr, xmin, xmax)` built-in is called.
 *
 * The host application (NeoLanguageApp) reads this struct and renders
 * the plot using whatever rendering technology it prefers.  The SymExpr
 * pointer is evaluated directly via SymExpr::evaluate(x), which makes
 * the rendering completely independent of GrapherApp's string evaluator:
 * upgrading GrapherApp does not require any changes to NeoStdLib.
 */
struct NeoPlotRequest {
    cas::SymExpr* expr    = nullptr;  ///< Arena-owned expression to evaluate
    double        xMin    = -10.0;    ///< Left boundary of the view
    double        xMax    =  10.0;    ///< Right boundary of the view
    bool          pending = false;    ///< true if a plot was requested
};

// ════════════════════════════════════════════════════════════════════
// NeoHostCallbacks — I/O interface between stdlib and the host app
// ════════════════════════════════════════════════════════════════════

/**
 * Raw function-pointer callbacks supplied by the host application.
 * Using raw pointers (not std::function) avoids heap overhead on ESP32.
 * The `userdata` pointer is forwarded to every callback so the host
 * can capture its own context without a global variable.
 */
struct NeoHostCallbacks {
    /// print(text): send a string to the console output area.
    void (*onPrint)(const char* text, void* ud);

    /// msg_box(title, content): display a modal message dialog.
    void (*onMsgBox)(const char* title, const char* content, void* ud);

    /// input_num(prompt): request a numeric value from the user.
    /// Implementations may be synchronous (blocking until the user
    /// submits a value) or asynchronous (returning a sentinel and
    /// re-running the program later).  For non-interactive targets,
    /// returning 0.0 is acceptable.
    double (*onInputNum)(const char* prompt, void* ud);

    /// Opaque context pointer forwarded to every callback.
    void* userdata;
};

// ════════════════════════════════════════════════════════════════════
// NeoStdLib — Standard Library dispatcher
// ════════════════════════════════════════════════════════════════════

class NeoStdLib {
public:
    NeoStdLib();

    // ── Host integration ──────────────────────────────────────────
    /**
     * Supply host callbacks (print, msg_box, input_num).
     * Must be called before any program is run.
     */
    void setCallbacks(const NeoHostCallbacks& cbs) { _cbs = cbs; }

    /**
     * Provide access to the most recent plot request.
     * The host reads this after each runCode() to decide whether to
     * open Graphics Mode.
     */
    const NeoPlotRequest& plotRequest() const { return _plotReq; }

    /** Clear the pending plot request (host calls after consuming it). */
    void clearPlotRequest() { _plotReq = NeoPlotRequest{}; }

    // ── Built-in dispatch ─────────────────────────────────────────
    /**
     * Try to dispatch a built-in function call.
     *
     * @param name     Function name (lowercase).
     * @param args     Evaluated arguments.
     * @param result   [out] Set to the return value if handled.
     * @param env      Current execution environment (for vars()).
     * @param symArena SymExpr arena for CAS operations.
     * @return true and sets `result` if the name is a known built-in;
     *         false if the name is not handled here (caller continues).
     */
    bool callBuiltin(const std::string&          name,
                     const std::vector<NeoValue>& args,
                     NeoValue&                    result,
                     NeoEnv&                      env,
                     cas::SymExprArena&           symArena);

private:
    NeoHostCallbacks _cbs;
    NeoPlotRequest   _plotReq;

    // ── Per-function implementations ──────────────────────────────

    /// diff(expr, var) — symbolic differentiation
    bool callDiff      (const std::vector<NeoValue>& args, NeoValue& result,
                        cas::SymExprArena& sa);

    /// integrate(expr, var) — symbolic integration
    bool callIntegrate (const std::vector<NeoValue>& args, NeoValue& result,
                        cas::SymExprArena& sa);

    /// solve(expr, var) — find roots expr=0
    bool callSolve     (const std::vector<NeoValue>& args, NeoValue& result,
                        cas::SymExprArena& sa);

    /// simplify(expr) — algebraic simplification
    bool callSimplify  (const std::vector<NeoValue>& args, NeoValue& result,
                        cas::SymExprArena& sa);

    /// expand(expr) — polynomial expansion (distribute rule)
    bool callExpand    (const std::vector<NeoValue>& args, NeoValue& result,
                        cas::SymExprArena& sa);

    /// plot(expr, xmin, xmax) — queue a graphics-mode plot request
    bool callPlot      (const std::vector<NeoValue>& args, NeoValue& result,
                        cas::SymExprArena& sa);

    /// clear_plot() — discard any pending plot request
    bool callClearPlot (NeoValue& result);

    /// print(args...) — send text to console via callback
    bool callPrint     (const std::vector<NeoValue>& args, NeoValue& result);

    /// type(val) — display type name and return ordinal
    bool callType      (const std::vector<NeoValue>& args, NeoValue& result);

    /// vars() — list all bindings in the global environment
    bool callVars      (const std::vector<NeoValue>& args, NeoValue& result,
                        NeoEnv& env);

    /// input_num(prompt) — request numeric input via UI hook
    bool callInputNum  (const std::vector<NeoValue>& args, NeoValue& result);

    /// msg_box(title, content) — display modal dialog via UI hook
    bool callMsgBox    (const std::vector<NeoValue>& args, NeoValue& result);

    // ── Helpers ───────────────────────────────────────────────────

    /// Extract a SymExpr* from a NeoValue (Symbolic or numeric literal).
    /// Numeric literals are wrapped in a SymNum node.
    static cas::SymExpr* toSym(const NeoValue& v, cas::SymExprArena& sa);

    /// Extract the variable character from a NeoValue.
    /// Accepts Symbolic (SymVar) or a Symbol that was looked up → Symbolic.
    /// Returns 'x' if extraction fails.
    static char toVarChar(const NeoValue& v, cas::SymExprArena& sa);

    /// Send a string to the host print callback (no-op if not set).
    void hostPrint(const char* text);
};
