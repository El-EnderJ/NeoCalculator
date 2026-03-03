/**
 * SymPoly.h — Symbolic Polynomial & Monomial types for CAS-Lite.
 *
 * SymTerm  — A single monomial: coeff · var^power
 *            (e.g., 3x², -½x⁻¹, 7)
 *
 * SymPoly  — An ordered sum of SymTerms with automatic normalization.
 *            All internal vectors use PSRAMAllocator.
 *
 * Design decisions:
 *   · Coefficient is vpam::ExactVal — preserves exact fractions/radicals.
 *   · int16_t exponent allows negative powers (e.g., 2/x = 2·x⁻¹).
 *   · Single variable per polynomial (univariate CAS-Lite assumption).
 *   · normalize() sorts descending by power, combines like terms,
 *     removes zero coefficients, and GCD-simplifies every coefficient.
 *
 * Part of: NumOS CAS-Lite — Phase A (Algebraic Foundations)
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "PSRAMAllocator.h"
#include "CASRational.h"    // CASRational, CASInt
#include "../ExactVal.h"    // vpam::ExactVal (legacy bridge)

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymTerm — A single monomial: coeff · var^power
// ════════════════════════════════════════════════════════════════════

struct SymTerm {
    CASRational    coeff;     // Overflow-safe exact rational coefficient
    char           var;       // Variable name ('x','y','z','\0' for constant)
    int16_t        power;     // Exponent (negative = denominator, e.g., x⁻¹)

    // ── Constructors ──────────────────────────────────────────────────
    SymTerm();
    SymTerm(CASRational c, char v, int16_t p);
    
    // Legacy bridge: construct from ExactVal (transition helper)
    SymTerm(vpam::ExactVal c, char v, int16_t p);

    // Constant term (no variable): e.g., SymTerm::constant(5)
    static SymTerm constant(CASRational c);
    static SymTerm constant(vpam::ExactVal c);  // legacy bridge
    
    // Simple variable term: e.g., SymTerm::variable('x', 3, 2) = 3x²
    static SymTerm variable(char v, int64_t coeffNum, int64_t coeffDen = 1, int16_t p = 1);

    // ── Queries ─────────────────────────────────────────────────────
    bool isConstant() const;    // var == '\0' || power == 0
    bool isZero()     const;    // coeff.isZero()
    bool isLikeTerm(const SymTerm& other) const;  // same var & power

    // ── Scalar operations (in-place) ────────────────────────────────
    void negate();              // coeff = -coeff
    void simplifyCoeff();       // GCD-reduce the coefficient

    // ── Display ─────────────────────────────────────────────────────
    std::string toString() const;
};

// Convenience type alias for PSRAM-allocated term vector
using TermVec = std::vector<SymTerm, PSRAMAllocator<SymTerm>>;


// ════════════════════════════════════════════════════════════════════
// SymPoly — Symbolic univariate polynomial (sum of monomials)
// ════════════════════════════════════════════════════════════════════

class SymPoly {
public:
    // ── Constructors ────────────────────────────────────────────────
    SymPoly();
    explicit SymPoly(char variable);
    SymPoly(const SymPoly& other);
    SymPoly(SymPoly&& other) noexcept;
    SymPoly& operator=(const SymPoly& other);
    SymPoly& operator=(SymPoly&& other) noexcept;

    // ── Factory helpers ─────────────────────────────────────────────
    // Create from a single constant: e.g., SymPoly::fromConstant(CASRational(5))
    static SymPoly fromConstant(CASRational c);
    static SymPoly fromConstant(vpam::ExactVal c);  // legacy bridge

    // Create from a single term
    static SymPoly fromTerm(const SymTerm& t);

    // ── Core normalization ──────────────────────────────────────────
    // Sort by descending power, combine like terms, remove zeros,
    // GCD-simplify all coefficients.
    void normalize();

    // ── Accessors ───────────────────────────────────────────────────
    const TermVec& terms() const { return _terms; }
    TermVec&       terms()       { return _terms; }
    char           var()   const { return _var; }
    void           setVar(char v) { _var = v; }

    bool isEmpty()    const { return _terms.empty(); }
    bool isZero()     const;   // All terms zero (or empty after normalize)
    bool isConstant() const;   // Only constant terms remain
    int16_t degree()  const;   // Highest power (0 if constant/empty)

    // Return the coefficient for a specific power (CASRational::zero() if absent)
    CASRational coeffAt(int16_t power) const;

    // Legacy bridge: return ExactVal coefficient for a specific power
    vpam::ExactVal coeffAtExact(int16_t power) const;

    // ── Arithmetic (return new SymPoly) ─────────────────────────────
    SymPoly add(const SymPoly& rhs) const;
    SymPoly sub(const SymPoly& rhs) const;
    SymPoly negate() const;

    // Multiply every term by a CASRational scalar
    SymPoly mulScalar(const CASRational& scalar) const;

    // Legacy bridge: multiply by ExactVal scalar
    SymPoly mulScalar(const vpam::ExactVal& scalar) const;

    // Multiply two polynomials (FOIL / distribution)
    SymPoly mul(const SymPoly& rhs) const;

    // Divide every term by a CASRational scalar
    SymPoly divScalar(const CASRational& scalar) const;

    // Legacy bridge: divide by ExactVal scalar
    SymPoly divScalar(const vpam::ExactVal& scalar) const;

    // ── Display ─────────────────────────────────────────────────────
    std::string toString() const;

private:
    TermVec _terms;  // PSRAM-allocated vector of monomials
    char    _var;    // Primary variable ('x' by default)
};

} // namespace cas
