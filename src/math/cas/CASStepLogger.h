/**
 * CASStepLogger.h — Step-by-step logging engine for CAS solvers.
 *
 * Each algebraic manipulation is recorded as a CASStep containing:
 *   · kind         — StepKind: Transform (with equation), Annotation (text-only),
 *                     Result (final answer), ComplexResult (complex roots)
 *   · description  — Human-readable explanation
 *   · snapshot      — SymEquation state after this step (only for Transform/Result)
 *   · methodId      — Solver method that generated this step
 *   · reason        — Mathematical justification (e.g. "Distributive Property")
 *
 * Deduplication: Steps are hash-fingerprinted.  If the computed hash of a
 * new step matches the previous step's hash, the step is silently discarded.
 * This eliminates the redundant-step bug where the solver re-logs an
 * expression that has not changed (e.g. after a no-op identity rewrite).
 *
 * StepKind eliminates the "0 = 0" display bug: Annotation steps never
 * render an equation, so the default SymEquation is never shown.
 *
 * All steps are stored in PSRAM via PSRAMAllocator to free internal
 * SRAM for LVGL and real-time tasks.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase C+
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "PSRAMAllocator.h"
#include "SymEquation.h"

namespace cas {

// Forward declaration for MathCanvas-ready step expressions
class SymExpr;

// ════════════════════════════════════════════════════════════════════
// MethodId — Identifies which solver generated a step
// ════════════════════════════════════════════════════════════════════

enum class MethodId : uint8_t {
    General      = 0,   // Normalization, rearrangement, etc.
    Linear       = 1,   // First-degree solver (ax + b = 0)
    Quadratic    = 2,   // Second-degree solver (ax² + bx + c = 0)
    Newton       = 3,   // Numeric fallback (Newton-Raphson)
    Substitution = 4,   // System solver — substitution method
    Reduction    = 5,   // System solver — reduction (elimination) method
    Gauss        = 6,   // System solver — Gaussian elimination (3x3)
};

// ════════════════════════════════════════════════════════════════════
// StepKind — Structural classification of a step
// ════════════════════════════════════════════════════════════════════

enum class StepKind : uint8_t {
    Transform     = 0,   ///< Algebraic manipulation — has equation snapshot
    Annotation    = 1,   ///< Text-only note — NO equation snapshot rendered
    Result        = 2,   ///< Final real result — has equation snapshot
    ComplexResult = 3,   ///< Complex root result — description contains text form
};

// ════════════════════════════════════════════════════════════════════
// CASStep — A single algebraic manipulation step
// ════════════════════════════════════════════════════════════════════

struct CASStep {
    std::string  description;   // Human-readable step description (text only, no math notation)
    SymEquation  snapshot;      // Equation state (only meaningful for Transform/Result)
    MethodId     method;        // Which solver phase generated this
    StepKind     kind;          // Structural type of step
    const SymExpr* mathExpr;    // Optional: CAS expression for 2D MathCanvas rendering
                                // Non-owning — SymExprArena manages lifetime
    const SymExpr* highlightExpr; // Optional: sub-expression modified in this step (Smart Highlighter)
                                  // Non-owning — rendered with LV_PALETTE_BLUE / LV_PALETTE_ORANGE accent
    std::string  reason;        // Mathematical justification (e.g. "Power Rule", "Distributive Property")

    CASStep()
        : description(), snapshot(), method(MethodId::General),
          kind(StepKind::Transform), mathExpr(nullptr), highlightExpr(nullptr), reason() {}

    CASStep(const std::string& desc, const SymEquation& snap,
            MethodId m, StepKind k = StepKind::Transform,
            const SymExpr* expr = nullptr,
            const std::string& rsn = "",
            const SymExpr* highlight = nullptr)
        : description(desc), snapshot(snap), method(m), kind(k),
          mathExpr(expr), highlightExpr(highlight), reason(rsn) {}
};

// PSRAM-allocated step vector
using StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>;

// ════════════════════════════════════════════════════════════════════
// CASStepLogger — Accumulates algebraic steps in PSRAM
// ════════════════════════════════════════════════════════════════════

class CASStepLogger {
public:
    CASStepLogger();

    /// Add a transform step with description, equation snapshot, and method.
    /// Duplicate steps (same hash as previous) are silently discarded.
    void log(const std::string& description,
             const SymEquation& snapshot,
             MethodId method = MethodId::General,
             const std::string& reason = "");

    /// Add an annotation step (text-only, NO equation rendered).
    void logNote(const std::string& note, MethodId method = MethodId::General);

    /// Add an annotation step with an attached SymExpr for 2D MathCanvas rendering.
    /// The SymExpr* is non-owning — the arena must outlive the step display.
    void logExpr(const std::string& desc, const SymExpr* expr,
                 MethodId method = MethodId::General,
                 const std::string& reason = "");

    /// Add an annotation step with a SymExpr for rendering AND a highlighted
    /// sub-expression (Smart Highlighter).  The highlighted node will be
    /// displayed in an accent colour (LV_PALETTE_BLUE / LV_PALETTE_ORANGE)
    /// in the step viewer so students can see exactly what changed.
    /// Both SymExpr pointers are non-owning — the arena must outlive display.
    void logWithHighlight(const std::string& desc, const SymExpr* expr,
                          const SymExpr* highlightExpr,
                          MethodId method = MethodId::General,
                          const std::string& reason = "");

    /// Add a result step (final answer with equation snapshot).
    void logResult(const std::string& description,
                   const SymEquation& snapshot,
                   MethodId method = MethodId::General,
                   const std::string& reason = "");

    /// Add a result step with MathCanvas expression (no snapshot needed).
    void logResultExpr(const std::string& desc, const SymExpr* expr,
                       MethodId method = MethodId::General,
                       const std::string& reason = "");

    /// Add a complex result step (text description only, no SymEquation).
    void logComplex(const std::string& description,
                    MethodId method = MethodId::Quadratic);

    /// Copy a step preserving its original StepKind.
    /// Used by OmniSolver to relay steps from SingleSolver without losing
    /// the kind (Transform, Annotation, Result, ComplexResult).
    void copyStep(const CASStep& step);

    /// Access all recorded steps.
    const StepVec& steps() const { return _steps; }

    /// Number of recorded steps.
    size_t count() const { return _steps.size(); }

    /// Clear all steps (e.g., before solving a new equation).
    void clear();

    /// Print all steps to a string (for Serial debugging / test output).
    std::string dump() const;

private:
    StepVec _steps;
    size_t  _prevHash;   // Hash of the most recently accepted step (for dedup)

    /// Compute a fingerprint hash for a step's mathematical content.
    /// Uses the equation snapshot string so that equivalent states hash equally.
    static size_t computeStepHash(const SymEquation& snapshot,
                                  const SymExpr* expr,
                                  StepKind kind);

    /// Returns true if the step should be discarded (duplicate or trivial).
    bool isDuplicate(size_t hash, StepKind kind) const;
};

} // namespace cas
