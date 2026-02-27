/**
 * SymExpr.h — Generic symbolic expression tree for Pro-CAS.
 *
 * Represents arbitrary mathematical expressions including transcendental
 * functions (sin, cos, ln, etc.) as an arena-allocated tree. Subsumes
 * the polynomial-only SymPoly for expressions that cannot be flattened
 * to a coefficient map.
 *
 * Node hierarchy (all arena-allocated, no individual free):
 *   SymExpr          — abstract base
 *   ├── SymNum       — exact numeric literal (ExactVal)
 *   ├── SymVar       — variable ('x', 'y', etc.)
 *   ├── SymNeg       — unary negation -(expr)
 *   ├── SymAdd       — n-ary sum:     a + b + c + ...
 *   ├── SymMul       — n-ary product: a · b · c · ...
 *   ├── SymPow       — binary power:  base ^ exponent
 *   └── SymFunc      — named function: sin(arg), cos(arg), ln(arg), ...
 *
 * All nodes carry:
 *   · evaluate(varValues) → double   (numeric evaluation)
 *   · clone(arena) → SymExpr*        (deep copy into arena)
 *   · toString() → string            (debug representation)
 *   · containsVar(char) → bool       (variable presence scan)
 *   · isPolynomial() → bool          (can convert to SymPoly?)
 *
 * Conversion:
 *   · toSymPoly(char var) → SymPoly  (only valid when isPolynomial())
 *
 * Memory: ALL nodes live in a SymExprArena (PSRAM bump allocator).
 *         No destructors needed — the arena bulk-frees on reset().
 *
 * Part of: NumOS Pro-CAS — Phase 1 (Data Structure Overhaul)
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include "SymExprArena.h"
#include "SymPoly.h"        // SymPoly, SymTerm, ExactVal
#include "../ExactVal.h"

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymFunc function kinds — mirrors vpam::FuncKind but CAS-owned
// ════════════════════════════════════════════════════════════════════

enum class SymFuncKind : uint8_t {
    Sin,       // sin(x)
    Cos,       // cos(x)
    Tan,       // tan(x)
    ArcSin,    // arcsin(x)
    ArcCos,    // arccos(x)
    ArcTan,    // arctan(x)
    Ln,        // ln(x)  — natural log
    Log10,     // log(x) — base-10 log
    Exp,       // e^x    — natural exponential (syntactic sugar)
    Abs,       // |x|
};

const char* symFuncName(SymFuncKind kind);

// ════════════════════════════════════════════════════════════════════
// SymExpr — Abstract base for symbolic expression nodes
// ════════════════════════════════════════════════════════════════════

enum class SymExprType : uint8_t {
    Num,
    Var,
    Neg,
    Add,
    Mul,
    Pow,
    Func,
};

class SymExpr {
public:
    const SymExprType type;

    /// Numeric evaluation. `varVal` is the value for the primary variable.
    /// For multi-variable support, extend to a map in the future.
    virtual double evaluate(double varVal) const = 0;

    /// Deep-copy this subtree into the given arena.
    virtual SymExpr* clone(SymExprArena& arena) const = 0;

    /// Human-readable debug string (e.g., "(3*x^2 + sin(x))").
    virtual std::string toString() const = 0;

    /// Does this subtree contain variable `v`?
    virtual bool containsVar(char v) const = 0;

    /// Can this entire subtree be represented as a SymPoly?
    /// True if it uses only Num, Var, Neg, Add, Mul, and Pow with
    /// integer-constant exponents.
    virtual bool isPolynomial() const = 0;

    /// Convert to SymPoly (only valid when isPolynomial() returns true).
    /// Caller provides the expected variable name.
    SymPoly toSymPoly(char var) const;

protected:
    explicit SymExpr(SymExprType t) : type(t) {}

    // No virtual destructor — arena handles deallocation.
    // Prevent accidental delete via protected non-virtual dtor.
    ~SymExpr() = default;
};

// ════════════════════════════════════════════════════════════════════
// SymNum — Exact numeric constant
// ════════════════════════════════════════════════════════════════════

class SymNum : public SymExpr {
public:
    vpam::ExactVal val;

    explicit SymNum(const vpam::ExactVal& v)
        : SymExpr(SymExprType::Num), val(v) {}

    double      evaluate(double) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char) const override { return false; }
    bool        isPolynomial() const override { return true; }
};

// ════════════════════════════════════════════════════════════════════
// SymVar — Variable reference
// ════════════════════════════════════════════════════════════════════

class SymVar : public SymExpr {
public:
    char name;

    explicit SymVar(char n)
        : SymExpr(SymExprType::Var), name(n) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override { return name == v; }
    bool        isPolynomial() const override { return true; }
};

// ════════════════════════════════════════════════════════════════════
// SymNeg — Unary negation: -(child)
// ════════════════════════════════════════════════════════════════════

class SymNeg : public SymExpr {
public:
    SymExpr* child;

    explicit SymNeg(SymExpr* c)
        : SymExpr(SymExprType::Neg), child(c) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;
};

// ════════════════════════════════════════════════════════════════════
// SymAdd — N-ary sum: terms[0] + terms[1] + ... + terms[n-1]
//
// The terms array is arena-allocated (pointer + count).
// ════════════════════════════════════════════════════════════════════

class SymAdd : public SymExpr {
public:
    SymExpr** terms;     // Arena-allocated array of pointers
    uint16_t  count;

    SymAdd(SymExpr** t, uint16_t n)
        : SymExpr(SymExprType::Add), terms(t), count(n) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;
};

// ════════════════════════════════════════════════════════════════════
// SymMul — N-ary product: factors[0] · factors[1] · ... · factors[n-1]
// ════════════════════════════════════════════════════════════════════

class SymMul : public SymExpr {
public:
    SymExpr** factors;   // Arena-allocated array of pointers
    uint16_t  count;

    SymMul(SymExpr** f, uint16_t n)
        : SymExpr(SymExprType::Mul), factors(f), count(n) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;
};

// ════════════════════════════════════════════════════════════════════
// SymPow — Binary power: base ^ exponent
// ════════════════════════════════════════════════════════════════════

class SymPow : public SymExpr {
public:
    SymExpr* base;
    SymExpr* exponent;

    SymPow(SymExpr* b, SymExpr* e)
        : SymExpr(SymExprType::Pow), base(b), exponent(e) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;
};

// ════════════════════════════════════════════════════════════════════
// SymFunc — Named mathematical function: kind(argument)
// ════════════════════════════════════════════════════════════════════

class SymFunc : public SymExpr {
public:
    SymFuncKind kind;
    SymExpr*    argument;

    SymFunc(SymFuncKind k, SymExpr* arg)
        : SymExpr(SymExprType::Func), kind(k), argument(arg) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override { return false; }
};

// ════════════════════════════════════════════════════════════════════
// Arena helper factories — convenient node creation
// ════════════════════════════════════════════════════════════════════

/// Create a numeric constant node
inline SymNum* symNum(SymExprArena& a, const vpam::ExactVal& v) {
    return a.create<SymNum>(v);
}

/// Integer constant shorthand
inline SymNum* symInt(SymExprArena& a, int64_t n) {
    return a.create<SymNum>(vpam::ExactVal::fromInt(n));
}

/// Fraction constant shorthand
inline SymNum* symFrac(SymExprArena& a, int64_t num, int64_t den) {
    return a.create<SymNum>(vpam::ExactVal::fromFrac(num, den));
}

/// Variable node
inline SymVar* symVar(SymExprArena& a, char name) {
    return a.create<SymVar>(name);
}

/// Negation node
inline SymNeg* symNeg(SymExprArena& a, SymExpr* child) {
    return a.create<SymNeg>(child);
}

/// Binary addition (2 terms)
inline SymAdd* symAdd(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(2 * sizeof(SymExpr*)));
    arr[0] = lhs;
    arr[1] = rhs;
    return a.create<SymAdd>(arr, 2);
}

/// Ternary addition (3 terms)
inline SymAdd* symAdd3(SymExprArena& a, SymExpr* t0, SymExpr* t1, SymExpr* t2) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(3 * sizeof(SymExpr*)));
    arr[0] = t0;
    arr[1] = t1;
    arr[2] = t2;
    return a.create<SymAdd>(arr, 3);
}

/// Binary multiplication (2 factors)
inline SymMul* symMul(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(2 * sizeof(SymExpr*)));
    arr[0] = lhs;
    arr[1] = rhs;
    return a.create<SymMul>(arr, 2);
}

/// Ternary multiplication (3 factors)
inline SymMul* symMul3(SymExprArena& a, SymExpr* f0, SymExpr* f1, SymExpr* f2) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(3 * sizeof(SymExpr*)));
    arr[0] = f0;
    arr[1] = f1;
    arr[2] = f2;
    return a.create<SymMul>(arr, 3);
}

/// Power node
inline SymPow* symPow(SymExprArena& a, SymExpr* base, SymExpr* exp) {
    return a.create<SymPow>(base, exp);
}

/// Function node
inline SymFunc* symFunc(SymExprArena& a, SymFuncKind kind, SymExpr* arg) {
    return a.create<SymFunc>(kind, arg);
}

} // namespace cas
