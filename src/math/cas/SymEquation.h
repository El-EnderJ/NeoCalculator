/**
 * SymEquation.h — Symbolic equation: LHS = RHS
 *
 * Wraps two SymPoly instances representing the left-hand side and
 * right-hand side of an equation. Provides convenience methods for
 * the solver phases:
 *   · moveAllToLHS() — rearranges to (LHS - RHS = 0)
 *   · swap()         — exchange LHS ↔ RHS
 *   · isLinear()     — checks if equation is degree 1 after moveAllToLHS
 *   · isQuadratic()  — checks if equation is degree 2
 *
 * Part of: NumOS CAS-Lite — Phase A (Algebraic Foundations)
 */

#pragma once

#include "SymPoly.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymEquation — LHS = RHS
// ════════════════════════════════════════════════════════════════════

class SymEquation {
public:
    SymPoly lhs;
    SymPoly rhs;

    // ── Constructors ────────────────────────────────────────────────
    SymEquation() : lhs(), rhs() {}

    SymEquation(const SymPoly& left, const SymPoly& right)
        : lhs(left), rhs(right) {}

    SymEquation(SymPoly&& left, SymPoly&& right)
        : lhs(std::move(left)), rhs(std::move(right)) {}

    // ── Manipulation ─────────────────────────────────────────────────

    /// Move everything to LHS: returns (LHS - RHS = 0)
    /// After this call, rhs is zero.
    SymEquation moveAllToLHS() const {
        SymPoly combined = lhs.sub(rhs);
        combined.normalize();
        return SymEquation(combined, SymPoly::fromConstant(vpam::ExactVal::fromInt(0)));
    }

    /// Swap LHS and RHS
    void swap() {
        std::swap(lhs, rhs);
    }

    /// Negate both sides: -LHS = -RHS
    SymEquation negateBoth() const {
        return SymEquation(lhs.negate(), rhs.negate());
    }

    /// Multiply both sides by a scalar
    SymEquation mulBothScalar(const vpam::ExactVal& scalar) const {
        return SymEquation(lhs.mulScalar(scalar), rhs.mulScalar(scalar));
    }

    /// Divide both sides by a scalar (distribution)
    SymEquation divBothScalar(const vpam::ExactVal& scalar) const {
        return SymEquation(lhs.divScalar(scalar), rhs.divScalar(scalar));
    }

    // ── Queries ─────────────────────────────────────────────────────

    /// Primary variable (from LHS, or RHS if LHS is constant-only)
    char var() const {
        if (!lhs.isConstant()) return lhs.var();
        return rhs.var();
    }

    /// Degree of the equation (after conceptual moveAllToLHS)
    int16_t degree() const {
        // Quick check without actually creating a new polynomial
        int16_t dl = lhs.degree();
        int16_t dr = rhs.degree();
        return (dl > dr) ? dl : dr;
    }

    bool isLinear()    const { return degree() == 1; }
    bool isQuadratic() const { return degree() == 2; }

    // ── Display ─────────────────────────────────────────────────────

    std::string toString() const {
        return lhs.toString() + " = " + rhs.toString();
    }
};

} // namespace cas
