/**
 * NeoValue.h — Runtime value representation for the NeoLanguage interpreter.
 *
 * NeoValue is a variant-like class that holds one of six possible types:
 *   Null      — absence of value (None)
 *   Boolean   — true / false
 *   Number    — IEEE-754 double
 *   Exact     — CASRational (exact rational arithmetic)
 *   Symbolic  — cas::SymExpr* (symbolic mathematical expression via Pro-CAS)
 *   Function  — user-defined function (FunctionDefNode* + closure NeoEnv*)
 *
 * Arithmetic dispatch:
 *   Number  OP Number   → Number  (double arithmetic)
 *   Exact   OP Exact    → Exact   (CASRational arithmetic)
 *   Exact   OP Number   → Number  (promote exact to double)
 *   otherwise           → Symbolic (via CAS symAdd/symMul/symSub/symDiv factories)
 *
 * The "Symbolic Twist" (Wolfram Language-like behaviour):
 *   Undefined variables become SymExpr nodes rather than errors.
 *   Any arithmetic involving a Symbolic escalates the result to Symbolic.
 *
 * Arithmetic methods accept a cas::SymExprArena& because Symbolic results
 * require arena-allocated SymExpr nodes.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 2 (Interpreter)
 */
#pragma once

#include <string>
#include <cstdint>
#include <cmath>
#include "../math/cas/SymExprArena.h"
#include "../math/cas/SymExpr.h"
#include "../math/cas/CASRational.h"
#include "NeoAST.h"

// Forward declaration to break circular dependency:
// NeoValue holds NeoEnv* (closure), NeoEnv holds NeoValue (bindings).
class NeoEnv;

// ════════════════════════════════════════════════════════════════════
// NeoValue
// ════════════════════════════════════════════════════════════════════

class NeoValue {
public:
    // ── Value kind discriminator ──────────────────────────────────
    enum class Type : uint8_t {
        Null,
        Boolean,
        Number,
        Exact,
        Symbolic,
        Function,
    };

    // ── Default: Null ─────────────────────────────────────────────
    NeoValue()
        : _type(Type::Null)
        , _bool(false)
        , _num(0.0)
        , _exact()
        , _sym(nullptr)
        , _funcDef(nullptr)
        , _funcClosure(nullptr)
    {}

    // ── Static factories ──────────────────────────────────────────
    static NeoValue makeNull();
    static NeoValue makeBool(bool b);
    static NeoValue makeNumber(double d);
    static NeoValue makeExact(const cas::CASRational& r);
    static NeoValue makeSymbolic(cas::SymExpr* sym);
    static NeoValue makeFunction(FunctionDefNode* def, NeoEnv* closure);

    // ── Type queries ──────────────────────────────────────────────
    Type type()       const { return _type; }
    bool isNull()     const { return _type == Type::Null; }
    bool isBool()     const { return _type == Type::Boolean; }
    bool isNumber()   const { return _type == Type::Number; }
    bool isExact()    const { return _type == Type::Exact; }
    bool isSymbolic() const { return _type == Type::Symbolic; }
    bool isFunction() const { return _type == Type::Function; }

    /// True for Number or Exact (numerically concrete, non-symbolic).
    bool isNumeric()  const { return _type == Type::Number || _type == Type::Exact; }

    // ── Value accessors (caller ensures correct type) ─────────────
    bool                    asBool()       const { return _bool; }
    double                  asNum()        const { return _num; }
    const cas::CASRational& asExact()      const { return _exact; }
    cas::SymExpr*           asSym()        const { return _sym; }
    FunctionDefNode*        funcDef()      const { return _funcDef; }
    NeoEnv*                 funcClosure()  const { return _funcClosure; }

    // ── Truthiness (Python-style) ─────────────────────────────────
    /// Null → false; Boolean → value; Number → non-zero; others → true.
    bool isTruthy() const;

    // ── Arithmetic — require arena for possible Symbolic creation ─
    NeoValue add(const NeoValue& rhs, cas::SymExprArena& sa) const;
    NeoValue sub(const NeoValue& rhs, cas::SymExprArena& sa) const;
    NeoValue mul(const NeoValue& rhs, cas::SymExprArena& sa) const;
    NeoValue div(const NeoValue& rhs, cas::SymExprArena& sa) const;
    NeoValue pow(const NeoValue& rhs, cas::SymExprArena& sa) const;
    NeoValue neg(cas::SymExprArena& sa) const;

    // ── Comparison — return Boolean NeoValues ─────────────────────
    NeoValue opEq(const NeoValue& rhs) const;
    NeoValue opNe(const NeoValue& rhs) const;
    NeoValue opLt(const NeoValue& rhs) const;
    NeoValue opLe(const NeoValue& rhs) const;
    NeoValue opGt(const NeoValue& rhs) const;
    NeoValue opGe(const NeoValue& rhs) const;

    // ── Display ───────────────────────────────────────────────────
    std::string toString() const;

    // ── Helpers ───────────────────────────────────────────────────
    /// Promote this value to a SymExpr* for use in symbolic arithmetic.
    /// Returns nullptr if the value cannot be represented symbolically.
    cas::SymExpr* toSymExpr(cas::SymExprArena& sa) const;

    /// Approximate numeric value as double (Number → as-is, Exact → toDouble(),
    /// Symbolic → evaluate(0), others → 0).
    double toDouble() const;

private:
    Type             _type;
    bool             _bool;
    double           _num;
    cas::CASRational _exact;
    cas::SymExpr*    _sym;
    FunctionDefNode* _funcDef;
    NeoEnv*          _funcClosure;
};
