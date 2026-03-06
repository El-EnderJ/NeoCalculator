/**
 * ChemCAS.h — Chemistry Computer Algebra System for NumOS.
 *
 * Standalone, lightweight engine for:
 *   1. Molar mass parsing: "Ca(OH)2", "CuSO4*5H2O", "C6H12O6"
 *   2. Chemical equation balancing using rational Gauss-Jordan elimination
 *
 * Uses the Flash-resident ChemDatabase for element masses.
 * Does NOT use the existing double-based MatrixEngine (avoids FP errors).
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace chem {

// ── Maximum limits ──────────────────────────────────────────────────────────
static constexpr int MAX_FORMULA_LEN   = 64;
static constexpr int MAX_MOLECULES     = 8;    // max molecules in equation
static constexpr int MAX_UNIQUE_ELEMS  = 16;   // max distinct elements
static constexpr int MAX_RESULT_LEN    = 128;  // result string buffer

// ════════════════════════════════════════════════════════════════════════════
// RationalNumber — Exact arithmetic to avoid floating-point errors
// ════════════════════════════════════════════════════════════════════════════

struct RationalNumber {
    int32_t num;
    int32_t den;

    constexpr RationalNumber() : num(0), den(1) {}
    constexpr RationalNumber(int32_t n, int32_t d = 1) : num(n), den(d) {
        // Simplify in non-constexpr contexts only — see simplify()
    }

    void simplify();
    bool isZero() const { return num == 0; }

    RationalNumber operator+(const RationalNumber& o) const;
    RationalNumber operator-(const RationalNumber& o) const;
    RationalNumber operator*(const RationalNumber& o) const;
    RationalNumber operator/(const RationalNumber& o) const;
    RationalNumber operator-() const { return {-num, den}; }
    bool operator==(const RationalNumber& o) const;
    bool operator!=(const RationalNumber& o) const { return !(*this == o); }
};

int32_t gcd(int32_t a, int32_t b);
int32_t lcm(int32_t a, int32_t b);

// ════════════════════════════════════════════════════════════════════════════
// Molar Mass Calculator
// ════════════════════════════════════════════════════════════════════════════

/**
 * Parse a chemical formula and return its molar mass.
 * Supports:
 *   - Simple: "H2O", "NaCl", "C6H12O6"
 *   - Parentheses: "Ca(OH)2", "Mg(NO3)2"
 *   - Nested: "Ca3(PO4)2"
 *   - Hydrates: "CuSO4*5H2O" (star = coefficient multiplier)
 *
 * Returns molar mass in g/mol, or -1.0f on parse error.
 */
float parseMolarMass(const char* formula);

// ════════════════════════════════════════════════════════════════════════════
// Equation Balancer
// ════════════════════════════════════════════════════════════════════════════

/**
 * Balance a chemical equation string.
 *
 * Input format:  "H2 + O2 = H2O"  (spaces optional, '=' or '->' separator)
 * Output format: "2H2 + O2 = 2H2O"
 *
 * @param equation  Input unbalanced equation
 * @param result    Output buffer (at least MAX_RESULT_LEN chars)
 * @return true on success, false on parse/balance error
 */
bool balanceEquation(const char* equation, char* result);

} // namespace chem
