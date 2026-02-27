/**
 * SymPoly.cpp — Implementation of SymTerm & SymPoly for CAS-Lite.
 *
 * All operations keep coefficients GCD-simplified via ExactVal::simplify()
 * after every arithmetic step to prevent integer overflow during
 * algebraic manipulations.
 *
 * Part of: NumOS CAS-Lite — Phase A (Algebraic Foundations)
 */

#include "SymPoly.h"
#include <algorithm>
#include <cmath>

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymTerm Implementation
// ════════════════════════════════════════════════════════════════════

SymTerm::SymTerm()
    : coeff(vpam::ExactVal::fromInt(0)), var('\0'), power(0) {}

SymTerm::SymTerm(vpam::ExactVal c, char v, int16_t p)
    : coeff(c), var(v), power(p) {
    simplifyCoeff();
}

SymTerm SymTerm::constant(vpam::ExactVal c) {
    return SymTerm(c, '\0', 0);
}

SymTerm SymTerm::variable(char v, int64_t coeffNum, int64_t coeffDen, int16_t p) {
    return SymTerm(vpam::ExactVal::fromFrac(coeffNum, coeffDen), v, p);
}

bool SymTerm::isConstant() const {
    return var == '\0' || power == 0;
}

bool SymTerm::isZero() const {
    return coeff.isZero();
}

bool SymTerm::isLikeTerm(const SymTerm& other) const {
    // Both constants → like terms
    if (isConstant() && other.isConstant()) return true;
    // Same variable and same power
    return var == other.var && power == other.power;
}

void SymTerm::negate() {
    coeff = vpam::exactNeg(coeff);
    simplifyCoeff();
}

void SymTerm::simplifyCoeff() {
    coeff.simplify();
    coeff.simplifyRadical();
}

std::string SymTerm::toString() const {
    std::string result;

    // Handle zero
    if (coeff.isZero()) return "0";

    // Determine sign and absolute coefficient
    bool negative = coeff.num < 0;
    int64_t absNum = negative ? -coeff.num : coeff.num;
    int64_t den = coeff.den;

    // Coefficient part
    bool isOne = (absNum == 1 && den == 1 && coeff.outer == 1 &&
                  coeff.inner == 1 && coeff.piMul == 0 && coeff.eMul == 0);

    if (negative) result += "-";

    // For constants or non-unit coefficients, print the number
    if (isConstant() || !isOne) {
        if (den == 1) {
            if (coeff.outer != 1 || coeff.inner != 1) {
                // Radical form
                if (coeff.outer != 1)
                    result += std::to_string(absNum * coeff.outer);
                else
                    result += std::to_string(absNum);
                if (coeff.inner > 1)
                    result += "√" + std::to_string(coeff.inner);
            } else {
                result += std::to_string(absNum);
            }
        } else {
            // Fraction
            result += "(" + std::to_string(absNum) + "/" + std::to_string(den) + ")";
        }
    }

    // Variable and power part
    if (!isConstant()) {
        result += var;
        if (power != 1 && power != 0) {
            result += "^";
            if (power < 0) {
                result += "(" + std::to_string(power) + ")";
            } else {
                result += std::to_string(power);
            }
        }
    }

    return result.empty() ? "0" : result;
}


// ════════════════════════════════════════════════════════════════════
// SymPoly Implementation
// ════════════════════════════════════════════════════════════════════

SymPoly::SymPoly()
    : _terms(), _var('x') {}

SymPoly::SymPoly(char variable)
    : _terms(), _var(variable) {}

SymPoly::SymPoly(const SymPoly& other)
    : _terms(other._terms), _var(other._var) {}

SymPoly::SymPoly(SymPoly&& other) noexcept
    : _terms(std::move(other._terms)), _var(other._var) {}

SymPoly& SymPoly::operator=(const SymPoly& other) {
    if (this != &other) {
        _terms = other._terms;
        _var   = other._var;
    }
    return *this;
}

SymPoly& SymPoly::operator=(SymPoly&& other) noexcept {
    if (this != &other) {
        _terms = std::move(other._terms);
        _var   = other._var;
    }
    return *this;
}

// ── Factory helpers ────────────────────────────────────────────────

SymPoly SymPoly::fromConstant(vpam::ExactVal c) {
    SymPoly p;
    p._terms.push_back(SymTerm::constant(c));
    p.normalize();
    return p;
}

SymPoly SymPoly::fromTerm(const SymTerm& t) {
    SymPoly p;
    if (t.var != '\0') p._var = t.var;
    p._terms.push_back(t);
    p.normalize();
    return p;
}

// ── Core normalization ─────────────────────────────────────────────

void SymPoly::normalize() {
    // Step 1: GCD-simplify every coefficient
    for (auto& t : _terms) {
        t.simplifyCoeff();
    }

    // Step 2: Canonicalize constant terms (power=0 or var='\0')
    // If a term has a variable but power==0, treat as constant
    for (auto& t : _terms) {
        if (t.power == 0 && t.var != '\0') {
            t.var = '\0';
        }
    }

    // Step 3: Sort by power descending
    std::sort(_terms.begin(), _terms.end(),
              [](const SymTerm& a, const SymTerm& b) {
                  // Constants (power=0, var='\0') always go last
                  int16_t pa = (a.var == '\0') ? static_cast<int16_t>(0) : a.power;
                  int16_t pb = (b.var == '\0') ? static_cast<int16_t>(0) : b.power;
                  return pa > pb;
              });

    // Step 4: Combine like terms
    TermVec merged;
    merged.reserve(_terms.size());

    for (size_t i = 0; i < _terms.size(); ) {
        SymTerm acc = _terms[i];
        size_t j = i + 1;
        while (j < _terms.size() && acc.isLikeTerm(_terms[j])) {
            // Add coefficients
            acc.coeff = vpam::exactAdd(acc.coeff, _terms[j].coeff);
            acc.simplifyCoeff();  // GCD after each add to prevent overflow
            ++j;
        }
        merged.push_back(acc);
        i = j;
    }

    // Step 5: Remove zero-coefficient terms
    _terms.clear();
    for (auto& t : merged) {
        if (!t.isZero()) {
            _terms.push_back(t);
        }
    }
}

// ── Queries ────────────────────────────────────────────────────────

bool SymPoly::isZero() const {
    if (_terms.empty()) return true;
    for (auto& t : _terms) {
        if (!t.isZero()) return false;
    }
    return true;
}

bool SymPoly::isConstant() const {
    if (_terms.empty()) return true;
    for (auto& t : _terms) {
        if (!t.isConstant()) return false;
    }
    return true;
}

int16_t SymPoly::degree() const {
    if (_terms.empty()) return 0;
    int16_t maxPow = 0;
    for (auto& t : _terms) {
        if (!t.isConstant() && t.power > maxPow) {
            maxPow = t.power;
        }
    }
    return maxPow;
}

vpam::ExactVal SymPoly::coeffAt(int16_t power) const {
    for (auto& t : _terms) {
        // Match: same power, and handle constants correctly
        if (power == 0 && t.isConstant()) return t.coeff;
        if (!t.isConstant() && t.power == power) return t.coeff;
    }
    return vpam::ExactVal::fromInt(0);
}

// ── Arithmetic ─────────────────────────────────────────────────────

SymPoly SymPoly::add(const SymPoly& rhs) const {
    SymPoly result(_var);
    result._terms.reserve(_terms.size() + rhs._terms.size());

    // Copy all terms from both polynomials
    for (auto& t : _terms) {
        result._terms.push_back(t);
    }
    for (auto& t : rhs._terms) {
        SymTerm copy = t;
        // If the RHS term has a variable and ours is set, adopt ours
        if (copy.var != '\0' && _var != '\0') {
            copy.var = _var;
        }
        result._terms.push_back(copy);
    }

    result.normalize();
    return result;
}

SymPoly SymPoly::sub(const SymPoly& rhs) const {
    // a - b = a + (-b)
    return add(rhs.negate());
}

SymPoly SymPoly::negate() const {
    SymPoly result(_var);
    result._terms.reserve(_terms.size());

    for (auto& t : _terms) {
        SymTerm neg = t;
        neg.negate();
        result._terms.push_back(neg);
    }

    // No need to re-normalize — structure is preserved, only signs flipped
    return result;
}

SymPoly SymPoly::mulScalar(const vpam::ExactVal& scalar) const {
    if (scalar.isZero()) {
        return SymPoly(_var);  // zero polynomial
    }

    SymPoly result(_var);
    result._terms.reserve(_terms.size());

    for (auto& t : _terms) {
        SymTerm scaled = t;
        scaled.coeff = vpam::exactMul(scaled.coeff, scalar);
        scaled.simplifyCoeff();  // GCD after multiply
        result._terms.push_back(scaled);
    }

    result.normalize();
    return result;
}

SymPoly SymPoly::mul(const SymPoly& rhs) const {
    // (a₁ + a₂ + ...) × (b₁ + b₂ + ...) = Σᵢ Σⱼ aᵢ·bⱼ
    // Each pair of terms: coeff₁·coeff₂ · var^(p₁+p₂)
    if (isZero() || rhs.isZero()) {
        return SymPoly(_var);
    }

    SymPoly result(_var);
    result._terms.reserve(_terms.size() * rhs._terms.size());

    for (auto& a : _terms) {
        for (auto& b : rhs._terms) {
            SymTerm product;
            product.coeff = vpam::exactMul(a.coeff, b.coeff);
            product.simplifyCoeff();  // GCD after multiply

            // Determine variable and power
            if (a.isConstant() && b.isConstant()) {
                product.var   = '\0';
                product.power = 0;
            } else if (a.isConstant()) {
                product.var   = b.var;
                product.power = b.power;
            } else if (b.isConstant()) {
                product.var   = a.var;
                product.power = a.power;
            } else {
                // Both have variables — add exponents
                product.var   = a.var != '\0' ? a.var : b.var;
                product.power = a.power + b.power;
            }

            result._terms.push_back(product);
        }
    }

    result.normalize();
    return result;
}

SymPoly SymPoly::divScalar(const vpam::ExactVal& scalar) const {
    // Division by zero → return error polynomial (empty with flag)
    if (scalar.isZero()) {
        return SymPoly(_var);  // returns zero/empty poly
    }

    SymPoly result(_var);
    result._terms.reserve(_terms.size());

    // Distribute: (ax^n + bx^m + ...) / k = (a/k)x^n + (b/k)x^m + ...
    for (auto& t : _terms) {
        SymTerm divided = t;
        divided.coeff = vpam::exactDiv(divided.coeff, scalar);
        divided.simplifyCoeff();  // GCD after division
        result._terms.push_back(divided);
    }

    result.normalize();
    return result;
}

// ── Display ────────────────────────────────────────────────────────

std::string SymPoly::toString() const {
    if (_terms.empty()) return "0";

    std::string result;

    for (size_t i = 0; i < _terms.size(); ++i) {
        const auto& t = _terms[i];
        if (t.isZero()) continue;

        std::string termStr = t.toString();
        if (termStr.empty() || termStr == "0") continue;

        if (i == 0) {
            // First term: print as-is (includes sign if negative)
            result += termStr;
        } else {
            // Subsequent terms: add + or - separator
            if (t.coeff.num < 0) {
                // Term already has minus sign from toString()
                result += " " + termStr;
            } else {
                result += " + " + termStr;
            }
        }
    }

    return result.empty() ? "0" : result;
}

} // namespace cas
