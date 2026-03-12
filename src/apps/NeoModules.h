/**
 * NeoModules.h — Module system for NeoLanguage Phase 8.
 *
 * Provides lazy-loaded named modules so that library functions are
 * brought into the execution environment only when explicitly imported.
 * This keeps the global namespace clean and saves PSRAM on the ESP32-S3.
 *
 * Available modules:
 *   math       — mathematical constants and functions (pi, e, sin, cos, …)
 *   finance    — time-value-of-money functions (tvm_pv, tvm_fv, …)
 *   electronics — bitwise helpers and signal-related utilities
 *   stats      — statistical functions (mean, stddev, variance, …)
 *   signal     — FFT / IFFT and spectral analysis
 *
 * Usage from NeoLanguage:
 *   import finance                   → binds tvm_pv, tvm_fv, … into global env
 *   import finance as fin            → binds a dict 'fin' with those functions
 *   from math import pi, sin         → binds only pi and sin into global env
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 8 (Ecosystem)
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include "NeoValue.h"
#include "NeoEnv.h"

// ════════════════════════════════════════════════════════════════════
// NeoModules — module loader
// ════════════════════════════════════════════════════════════════════

class NeoModules {
public:
    /**
     * Load all exports of a named module into `env`.
     *
     * @param name    Module name (e.g. "finance", "math", "signal").
     * @param env     Environment to define names into.
     * @param sa      SymExpr arena (forwarded to native callables).
     * @return true if the module was found and loaded; false if unknown.
     */
    static bool loadIntoEnv(const std::string& name,
                             NeoEnv& env,
                             cas::SymExprArena& sa);

    /**
     * Build a Dictionary NeoValue containing all exports of a named module.
     * Used for `import X as Y` — the returned dictionary is bound as `Y`.
     *
     * @param name    Module name.
     * @param dict    [out] Receives a NeoValue::Dictionary on success.
     * @param sa      SymExpr arena.
     * @return true if the module was found; false otherwise.
     */
    static bool loadAsDict(const std::string& name,
                            NeoValue& dict,
                            cas::SymExprArena& sa);

    /**
     * Check whether a module with the given name exists.
     */
    static bool exists(const std::string& name);

    /**
     * Return a list of all available module names.
     */
    static std::vector<std::string> availableModules();

private:
    // ── Per-module builders ───────────────────────────────────────

    /// Populate `out` with the math module's exports (constants + functions).
    static void buildMath      (std::map<std::string, NeoValue>& out, cas::SymExprArena& sa);
    /// Populate `out` with the finance module's exports (TVM functions).
    static void buildFinance   (std::map<std::string, NeoValue>& out, cas::SymExprArena& sa);
    /// Populate `out` with the electronics/bitwise module's exports.
    static void buildElectronics(std::map<std::string, NeoValue>& out, cas::SymExprArena& sa);
    /// Populate `out` with the stats module's exports.
    static void buildStats     (std::map<std::string, NeoValue>& out, cas::SymExprArena& sa);
    /// Populate `out` with the signal module's exports (FFT / IFFT).
    static void buildSignal    (std::map<std::string, NeoValue>& out, cas::SymExprArena& sa);

    /// Dispatch: call the right builder for `name` and fill `out`.
    /// Returns false if `name` is unknown.
    static bool buildModule(const std::string& name,
                            std::map<std::string, NeoValue>& out,
                            cas::SymExprArena& sa);
};
