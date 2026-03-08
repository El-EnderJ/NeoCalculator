/**
 * SymExpr.h — Immutable symbolic expression DAG for Pro-CAS.
 *
 * Phase 2 upgrade: all nodes are now IMMUTABLE after construction.
 * Every node carries a precomputed `_hash` enabling O(1) hash-consing
 * via the ConsTable.  Pointer identity equals structural identity:
 *
 *     if (a == b)  →  same mathematical expression (guaranteed)
 *
 * Node hierarchy (all arena-allocated, no individual free):
 *   SymExpr          — abstract base (immutable, hashed)
 *   ├── SymNum       — exact numeric literal (CASRational + metadata)
 *   ├── SymVar       — variable ('x', 'y', etc.)
 *   ├── SymNeg       — unary negation -(expr)
 *   ├── SymAdd       — n-ary sum:     a + b + c + ...
 *   ├── SymMul       — n-ary product: a · b · c · ...
 *   ├── SymPow       — binary power:  base ^ exponent
 *   └── SymFunc      — named function: sin(arg), cos(arg), ln(arg), ...
 *
 * All nodes carry:
 *   · _hash               → precomputed structural hash (size_t)
 *   · evaluate(varVal)     → double
 *   · clone(arena)         → SymExpr*  (deep copy — bypasses cons)
 *   · toString()           → string    (debug representation)
 *   · containsVar(char)    → bool
 *   · isPolynomial()       → bool
 *   · toSymPoly(char var)  → SymPoly
 *
 * Comparison:
 *   · operator==   → pointer identity only (O(1))
 *   · operator!=   → pointer identity only (O(1))
 *
 * Memory: ALL nodes live in a SymExprArena (PSRAM bump allocator).
 *         No destructors — the arena bulk-frees on reset().
 *         ConsTable deduplicates structurally-identical nodes.
 *
 * Part of: NumOS Pro-CAS — Phase 2 (Immutable DAG & Hash-Consing)
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>       // std::sort for canonical ordering
#include "SymExprArena.h"
#include "SymPoly.h"
#include "CASRational.h"
#include "ConsTable.h"
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
    Exp,       // e^x    — natural exponential
    Abs,       // |x|
    Integral,  // ∫f(x)dx — unevaluated integral
};

const char* symFuncName(SymFuncKind kind);

// ════════════════════════════════════════════════════════════════════
// SymExpr — Abstract base for symbolic expression nodes (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

enum class SymExprType : uint8_t {
    Num,
    Var,
    Neg,
    Add,
    Mul,
    Pow,
    Func,
    Paren,       // Display-only: (expr)
    PlusMinus,   // Display-only: lhs ± rhs
    Subscript,   // Display-only: base_sub
};

class SymExpr {
public:
    const SymExprType type;
    const size_t      _hash;    // Precomputed structural hash

    /// Pointer identity == structural identity (via hash-consing).
    bool operator==(const SymExpr& other) const { return this == &other; }
    bool operator!=(const SymExpr& other) const { return this != &other; }

    /// Numeric evaluation.
    virtual double evaluate(double varVal) const = 0;

    /// Deep-copy this subtree into the given arena (bypasses cons).
    virtual SymExpr* clone(SymExprArena& arena) const = 0;

    /// Human-readable debug string.
    virtual std::string toString() const = 0;

    /// Does this subtree contain variable `v`?
    virtual bool containsVar(char v) const = 0;

    /// Can this entire subtree be represented as a SymPoly?
    virtual bool isPolynomial() const = 0;

    /// Convert to SymPoly (only valid when isPolynomial() returns true).
    SymPoly toSymPoly(char var) const;

protected:
    explicit SymExpr(SymExprType t, size_t hash)
        : type(t), _hash(hash) {}

    ~SymExpr() = default;
};

// ════════════════════════════════════════════════════════════════════
// SymNum — Exact numeric constant (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymNum : public SymExpr {
public:
    const CASRational _coeff;
    const int64_t     _outer;
    const int64_t     _inner;
    const int8_t      _piMul;
    const int8_t      _eMul;

    // ── Construct from CASRational (pure rational, no metadata) ──
    explicit SymNum(const CASRational& c)
        : SymExpr(SymExprType::Num, computeHashStatic(c, 1, 1, 0, 0)),
          _coeff(c), _outer(1), _inner(1), _piMul(0), _eMul(0) {}

    // ── Construct from CASRational + full metadata ──
    SymNum(const CASRational& c, int64_t outer, int64_t inner,
           int8_t piMul, int8_t eMul)
        : SymExpr(SymExprType::Num, computeHashStatic(c, outer, inner, piMul, eMul)),
          _coeff(c), _outer(outer), _inner(inner), _piMul(piMul), _eMul(eMul) {}

    // ── Construct from legacy ExactVal (bridge) ──
    explicit SymNum(const vpam::ExactVal& v)
        : SymExpr(SymExprType::Num,
            computeHashStatic(
                v.ok ? CASRational(v.num, v.den) : CASRational::makeError(),
                v.outer, v.inner, v.piMul, v.eMul)),
          _coeff(v.ok ? CASRational(v.num, v.den) : CASRational::makeError()),
          _outer(v.outer), _inner(v.inner),
          _piMul(v.piMul), _eMul(v.eMul) {}

    // ── Convert back to legacy ExactVal ──
    vpam::ExactVal toExactVal() const {
        vpam::ExactVal ev = _coeff.toExactVal();
        ev.outer = _outer;
        ev.inner = _inner;
        ev.piMul = _piMul;
        ev.eMul  = _eMul;
        return ev;
    }

    // ── Metadata queries ──
    bool hasPi()      const { return _piMul != 0; }
    bool hasE()       const { return _eMul != 0; }
    bool hasRadical() const { return _inner > 1; }
    bool isPureRational() const { return !hasPi() && !hasE() && !hasRadical(); }

    double      evaluate(double) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char) const override { return false; }
    bool        isPolynomial() const override { return true; }

    // ── Static hash computation ──
    static size_t computeHashStatic(const CASRational& c,
                                     int64_t outer, int64_t inner,
                                     int8_t piMul, int8_t eMul)
    {
        size_t h = hashMix(0x01);
        h = hashCombine(h, hashMix(static_cast<size_t>(c.num().toInt64())));
        h = hashCombine(h, hashMix(static_cast<size_t>(c.den().toInt64())));
        h = hashCombine(h, hashMix(static_cast<size_t>(outer)));
        h = hashCombine(h, hashMix(static_cast<size_t>(inner)));
        h = hashCombine(h, hashMix(static_cast<size_t>(piMul)));
        h = hashCombine(h, hashMix(static_cast<size_t>(eMul)));
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymVar — Variable reference (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymVar : public SymExpr {
public:
    const char name;

    explicit SymVar(char n)
        : SymExpr(SymExprType::Var, computeHashStatic(n)), name(n) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override { return name == v; }
    bool        isPolynomial() const override { return true; }

    static size_t computeHashStatic(char n) {
        return hashCombine(hashMix(0x02), hashMix(static_cast<size_t>(n)));
    }
};

// ════════════════════════════════════════════════════════════════════
// SymNeg — Unary negation: -(child)  (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymNeg : public SymExpr {
public:
    SymExpr* const child;

    explicit SymNeg(SymExpr* c)
        : SymExpr(SymExprType::Neg, computeHashStatic(c)), child(c) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;

    static size_t computeHashStatic(const SymExpr* c) {
        return hashCombine(hashMix(0x03), c ? c->_hash : 0);
    }
};

// ════════════════════════════════════════════════════════════════════
// SymAdd — N-ary sum (IMMUTABLE)
//
// Children array is arena-allocated (pointer + count), both const.
// Terms are canonically sorted by hash for commutative dedup.
// ════════════════════════════════════════════════════════════════════

class SymAdd : public SymExpr {
public:
    SymExpr* const* const terms;
    const uint16_t        count;

    SymAdd(SymExpr* const* t, uint16_t n)
        : SymExpr(SymExprType::Add, computeHashStatic(t, n)),
          terms(t), count(n) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;

    static size_t computeHashStatic(SymExpr* const* t, uint16_t n) {
        // Commutative hash: order-independent
        size_t h = hashMix(0x04);
        for (uint16_t i = 0; i < n; ++i) {
            h = hashCombineCommutative(h, t[i] ? t[i]->_hash : 0);
        }
        h = hashCombine(h, hashMix(static_cast<size_t>(n)));
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymMul — N-ary product (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymMul : public SymExpr {
public:
    SymExpr* const* const factors;
    const uint16_t        count;

    SymMul(SymExpr* const* f, uint16_t n)
        : SymExpr(SymExprType::Mul, computeHashStatic(f, n)),
          factors(f), count(n) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;

    static size_t computeHashStatic(SymExpr* const* f, uint16_t n) {
        size_t h = hashMix(0x05);
        for (uint16_t i = 0; i < n; ++i) {
            h = hashCombineCommutative(h, f[i] ? f[i]->_hash : 0);
        }
        h = hashCombine(h, hashMix(static_cast<size_t>(n)));
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymPow — Binary power: base ^ exponent  (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymPow : public SymExpr {
public:
    SymExpr* const base;
    SymExpr* const exponent;

    SymPow(SymExpr* b, SymExpr* e)
        : SymExpr(SymExprType::Pow, computeHashStatic(b, e)),
          base(b), exponent(e) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override;

    static size_t computeHashStatic(const SymExpr* b, const SymExpr* e) {
        size_t h = hashMix(0x06);
        h = hashCombine(h, b ? b->_hash : 0);
        h = hashCombine(h, e ? e->_hash : 0);
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymFunc — Named mathematical function: kind(argument)  (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymFunc : public SymExpr {
public:
    const SymFuncKind kind;
    SymExpr* const    argument;

    SymFunc(SymFuncKind k, SymExpr* arg)
        : SymExpr(SymExprType::Func, computeHashStatic(k, arg)),
          kind(k), argument(arg) {}

    double      evaluate(double varVal) const override;
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override;
    bool        containsVar(char v) const override;
    bool        isPolynomial() const override { return false; }

    static size_t computeHashStatic(SymFuncKind k, const SymExpr* arg) {
        size_t h = hashMix(0x07);
        h = hashCombine(h, hashMix(static_cast<size_t>(k)));
        h = hashCombine(h, arg ? arg->_hash : 0);
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymParen — Display-only parenthesized expression (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymParen : public SymExpr {
public:
    SymExpr* const child;

    explicit SymParen(SymExpr* c)
        : SymExpr(SymExprType::Paren, computeHashStatic(c)), child(c) {}

    double      evaluate(double varVal) const override { return child ? child->evaluate(varVal) : 0.0; }
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override {
        if (!child) return "()";
        std::string inner = child->toString();
        if (inner.size() >= 2 && inner.front() == '(' && inner.back() == ')') {
            return inner;
        }
        return "(" + inner + ")";
    }
    bool        containsVar(char v) const override { return child && child->containsVar(v); }
    bool        isPolynomial() const override { return child && child->isPolynomial(); }

    static size_t computeHashStatic(const SymExpr* c) {
        size_t h = hashMix(0x0A);
        h = hashCombine(h, c ? c->_hash : 0);
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymPlusMinus — Display-only binary ± operator (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymPlusMinus : public SymExpr {
public:
    SymExpr* const lhs;
    SymExpr* const rhs;

    SymPlusMinus(SymExpr* l, SymExpr* r)
        : SymExpr(SymExprType::PlusMinus, computeHashStatic(l, r)),
          lhs(l), rhs(r) {}

    double      evaluate(double varVal) const override { return 0.0; } // Display only
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override { return "(±)"; }
    bool        containsVar(char v) const override { return (lhs && lhs->containsVar(v)) || (rhs && rhs->containsVar(v)); }
    bool        isPolynomial() const override { return false; }

    static size_t computeHashStatic(const SymExpr* l, const SymExpr* r) {
        size_t h = hashMix(0x08);
        h = hashCombine(h, l ? l->_hash : 0);
        h = hashCombine(h, r ? r->_hash : 0);
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// SymSubscript — Display-only subscript (IMMUTABLE)
// ════════════════════════════════════════════════════════════════════

class SymSubscript : public SymExpr {
public:
    SymExpr* const base;
    SymExpr* const subscript;

    SymSubscript(SymExpr* b, SymExpr* sub)
        : SymExpr(SymExprType::Subscript, computeHashStatic(b, sub)),
          base(b), subscript(sub) {}

    double      evaluate(double varVal) const override { return 0.0; } // Display only
    SymExpr*    clone(SymExprArena& arena) const override;
    std::string toString() const override { return "(_)"; }
    bool        containsVar(char v) const override { return (base && base->containsVar(v)) || (subscript && subscript->containsVar(v)); }
    bool        isPolynomial() const override { return false; }

    static size_t computeHashStatic(const SymExpr* b, const SymExpr* sub) {
        size_t h = hashMix(0x09);
        h = hashCombine(h, b ? b->_hash : 0);
        h = hashCombine(h, sub ? sub->_hash : 0);
        return h;
    }
};

// ════════════════════════════════════════════════════════════════════
// Canonical ordering comparator
//
// Defines the total order for commutative children (Add terms,
// Mul factors).  Constants (Num) sort LAST so that the canonical
// form of  x + 1  is always  [x, 1]  never  [1, x].
// Within the same category, sort by hash for deterministic order.
// ════════════════════════════════════════════════════════════════════

inline bool symCanonicalLess(const SymExpr* a, const SymExpr* b) {
    bool aNum = (a->type == SymExprType::Num);
    bool bNum = (b->type == SymExprType::Num);
    if (aNum != bNum) return bNum;  // non-Nums before Nums
    return a->_hash < b->_hash;
}

// ════════════════════════════════════════════════════════════════════
// Cons-based arena helper factories — with hash-consing dedup
//
// All factories route through ConsTable::getOrCreate() to guarantee
// that structurally-identical nodes share the same pointer.
// ════════════════════════════════════════════════════════════════════

/// Create a numeric constant from CASRational (cons'd)
inline SymExpr* symNum(SymExprArena& a, const CASRational& c) {
    auto* node = a.create<SymNum>(c);
    return a.consTable().getOrCreate(node);
}

/// Create a numeric constant from legacy ExactVal (cons'd)
inline SymExpr* symNum(SymExprArena& a, const vpam::ExactVal& v) {
    auto* node = a.create<SymNum>(v);
    return a.consTable().getOrCreate(node);
}

/// Integer constant shorthand (cons'd)
inline SymExpr* symInt(SymExprArena& a, int64_t n) {
    auto* node = a.create<SymNum>(CASRational::fromInt(n));
    return a.consTable().getOrCreate(node);
}

/// Fraction constant shorthand (cons'd)
inline SymExpr* symFrac(SymExprArena& a, int64_t num, int64_t den) {
    auto* node = a.create<SymNum>(CASRational::fromFrac(num, den));
    return a.consTable().getOrCreate(node);
}

/// Variable node (cons'd)
inline SymExpr* symVar(SymExprArena& a, char name) {
    auto* node = a.create<SymVar>(name);
    return a.consTable().getOrCreate(node);
}

/// Negation node (cons'd)
inline SymExpr* symNeg(SymExprArena& a, SymExpr* child) {
    auto* node = a.create<SymNeg>(child);
    return a.consTable().getOrCreate(node);
}

/// Binary addition (2 terms, cons'd with canonical sort)
inline SymExpr* symAdd(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(2 * sizeof(SymExpr*)));
    arr[0] = lhs;
    arr[1] = rhs;
    // Canonical sort: non-Nums before Nums, then by hash
    if (symCanonicalLess(arr[1], arr[0])) std::swap(arr[0], arr[1]);
    auto* node = a.create<SymAdd>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(2));
    return a.consTable().getOrCreate(node);
}

/// Binary addition (2 terms, cons'd preserving insertion order)
inline SymExpr* symAddRaw(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(2 * sizeof(SymExpr*)));
    arr[0] = lhs;
    arr[1] = rhs;
    auto* node = a.create<SymAdd>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(2));
    return a.consTable().getOrCreate(node);
}

/// Ternary addition (3 terms, cons'd with canonical sort)
inline SymExpr* symAdd3(SymExprArena& a, SymExpr* t0, SymExpr* t1, SymExpr* t2) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(3 * sizeof(SymExpr*)));
    arr[0] = t0;
    arr[1] = t1;
    arr[2] = t2;
    std::sort(arr, arr + 3, [](SymExpr* x, SymExpr* y) {
        return symCanonicalLess(x, y);
    });
    auto* node = a.create<SymAdd>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(3));
    return a.consTable().getOrCreate(node);
}

/// Ternary addition (3 terms, cons'd preserving insertion order)
inline SymExpr* symAdd3Raw(SymExprArena& a, SymExpr* t0, SymExpr* t1, SymExpr* t2) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(3 * sizeof(SymExpr*)));
    arr[0] = t0;
    arr[1] = t1;
    arr[2] = t2;
    auto* node = a.create<SymAdd>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(3));
    return a.consTable().getOrCreate(node);
}

/// N-ary addition from pre-allocated array (cons'd with canonical sort)
inline SymExpr* symAddN(SymExprArena& a, SymExpr** arr, uint16_t count) {
    std::sort(arr, arr + count, [](SymExpr* x, SymExpr* y) {
        return symCanonicalLess(x, y);
    });
    auto* node = a.create<SymAdd>(const_cast<SymExpr* const*>(arr), count);
    return a.consTable().getOrCreate(node);
}

/// N-ary addition from pre-allocated array (cons'd preserving insertion order)
inline SymExpr* symAddNRaw(SymExprArena& a, SymExpr** arr, uint16_t count) {
    auto* node = a.create<SymAdd>(const_cast<SymExpr* const*>(arr), count);
    return a.consTable().getOrCreate(node);
}

/// Binary multiplication (2 factors, cons'd with canonical sort)
inline SymExpr* symMul(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(2 * sizeof(SymExpr*)));
    arr[0] = lhs;
    arr[1] = rhs;
    if (symCanonicalLess(arr[1], arr[0])) std::swap(arr[0], arr[1]);
    auto* node = a.create<SymMul>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(2));
    return a.consTable().getOrCreate(node);
}

/// Binary multiplication (2 factors, cons'd preserving insertion order)
inline SymExpr* symMulRaw(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(2 * sizeof(SymExpr*)));
    arr[0] = lhs;
    arr[1] = rhs;
    auto* node = a.create<SymMul>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(2));
    return a.consTable().getOrCreate(node);
}

/// Ternary multiplication (3 factors, cons'd with canonical sort)
inline SymExpr* symMul3(SymExprArena& a, SymExpr* f0, SymExpr* f1, SymExpr* f2) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(3 * sizeof(SymExpr*)));
    arr[0] = f0;
    arr[1] = f1;
    arr[2] = f2;
    std::sort(arr, arr + 3, [](SymExpr* x, SymExpr* y) {
        return symCanonicalLess(x, y);
    });
    auto* node = a.create<SymMul>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(3));
    return a.consTable().getOrCreate(node);
}

/// Ternary multiplication (3 factors, cons'd preserving insertion order)
inline SymExpr* symMul3Raw(SymExprArena& a, SymExpr* f0, SymExpr* f1, SymExpr* f2) {
    auto** arr = static_cast<SymExpr**>(a.allocRaw(3 * sizeof(SymExpr*)));
    arr[0] = f0;
    arr[1] = f1;
    arr[2] = f2;
    auto* node = a.create<SymMul>(const_cast<SymExpr* const*>(arr), static_cast<uint16_t>(3));
    return a.consTable().getOrCreate(node);
}

/// N-ary multiplication from pre-allocated array (cons'd with canonical sort)
inline SymExpr* symMulN(SymExprArena& a, SymExpr** arr, uint16_t count) {
    std::sort(arr, arr + count, [](SymExpr* x, SymExpr* y) {
        return symCanonicalLess(x, y);
    });
    auto* node = a.create<SymMul>(const_cast<SymExpr* const*>(arr), count);
    return a.consTable().getOrCreate(node);
}

/// N-ary multiplication from pre-allocated array (cons'd preserving insertion order)
inline SymExpr* symMulNRaw(SymExprArena& a, SymExpr** arr, uint16_t count) {
    auto* node = a.create<SymMul>(const_cast<SymExpr* const*>(arr), count);
    return a.consTable().getOrCreate(node);
}

/// Power node (cons'd)
inline SymExpr* symPow(SymExprArena& a, SymExpr* base, SymExpr* exp) {
    auto* node = a.create<SymPow>(base, exp);
    return a.consTable().getOrCreate(node);
}

/// Function node (cons'd)
inline SymExpr* symFunc(SymExprArena& a, SymFuncKind kind, SymExpr* arg) {
    auto* node = a.create<SymFunc>(kind, arg);
    return a.consTable().getOrCreate(node);
}

/// Display-only parenthesis node (cons'd)
inline SymExpr* symParen(SymExprArena& a, SymExpr* child) {
    auto* node = a.create<SymParen>(child);
    return a.consTable().getOrCreate(node);
}

/// PlusMinus node (cons'd)
inline SymExpr* symPlusMinus(SymExprArena& a, SymExpr* lhs, SymExpr* rhs) {
    auto* node = a.create<SymPlusMinus>(lhs, rhs);
    return a.consTable().getOrCreate(node);
}

/// Subscript node (cons'd)
inline SymExpr* symSubscript(SymExprArena& a, SymExpr* base, SymExpr* sub) {
    auto* node = a.create<SymSubscript>(base, sub);
    return a.consTable().getOrCreate(node);
}

} // namespace cas
