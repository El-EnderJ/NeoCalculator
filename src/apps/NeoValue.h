/**
 * NeoValue.h — Runtime value representation for the NeoLanguage interpreter.
 *
 * NeoValue is a variant-like class that holds one of seven possible types:
 *   Null      — absence of value (None)
 *   Boolean   — true / false
 *   Number    — IEEE-754 double
 *   Exact     — CASRational (exact rational arithmetic)
 *   Symbolic  — cas::SymExpr* (symbolic mathematical expression via Pro-CAS)
 *   Function  — user-defined function (FunctionDefNode* + closure NeoEnv*)
 *   List      — ordered collection of NeoValues (1-D list or row-major matrix)
 *
 * Arithmetic dispatch:
 *   Number  OP Number   → Number  (double arithmetic)
 *   Exact   OP Exact    → Exact   (CASRational arithmetic)
 *   Exact   OP Number   → Number  (promote exact to double)
 *   List    + List      → List    (concatenation)
 *   List    OP scalar   → List    (automatic element-wise / vectorised)
 *   otherwise           → Symbolic (via CAS symAdd/symMul/symSub/symDiv factories)
 *
 * The "Symbolic Twist" (Wolfram Language-like behaviour):
 *   Undefined variables become SymExpr nodes rather than errors.
 *   Any arithmetic involving a Symbolic escalates the result to Symbolic.
 *
 * Arithmetic methods accept a cas::SymExprArena& because Symbolic results
 * require arena-allocated SymExpr nodes.
 *
 * List memory: the underlying std::vector<NeoValue> is heap-allocated with
 * new and shared by pointer copy (reference semantics, like Python lists).
 * Memory is intentionally not freed individually; on the ESP32 target the
 * arena / environment is bulk-reset between program runs.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 4 (Data Structures)
 */
#pragma once

#include <string>
#include <vector>
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
        List,       ///< ordered collection of NeoValues (list / matrix row)
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
        , _list(nullptr)
    {}

    // ── Static factories ──────────────────────────────────────────
    static NeoValue makeNull();
    static NeoValue makeBool(bool b);
    static NeoValue makeNumber(double d);
    static NeoValue makeExact(const cas::CASRational& r);
    static NeoValue makeSymbolic(cas::SymExpr* sym);
    static NeoValue makeFunction(FunctionDefNode* def, NeoEnv* closure);

    /**
     * Create a List value.  NeoValue takes ownership of the vector pointer
     * (heap-allocated with new by the caller).  The pointer is shared by value
     * copies of this NeoValue (reference semantics, like Python lists).
     */
    static NeoValue makeList(std::vector<NeoValue>* list);

    // ── Type queries ──────────────────────────────────────────────
    Type type()       const { return _type; }
    bool isNull()     const { return _type == Type::Null; }
    bool isBool()     const { return _type == Type::Boolean; }
    bool isNumber()   const { return _type == Type::Number; }
    bool isExact()    const { return _type == Type::Exact; }
    bool isSymbolic() const { return _type == Type::Symbolic; }
    bool isFunction() const { return _type == Type::Function; }
    bool isList()     const { return _type == Type::List; }

    /// True for Number or Exact (numerically concrete, non-symbolic).
    bool isNumeric()  const { return _type == Type::Number || _type == Type::Exact; }

    // ── Value accessors (caller ensures correct type) ─────────────
    bool                    asBool()       const { return _bool; }
    double                  asNum()        const { return _num; }
    const cas::CASRational& asExact()      const { return _exact; }
    cas::SymExpr*           asSym()        const { return _sym; }
    FunctionDefNode*        funcDef()      const { return _funcDef; }
    NeoEnv*                 funcClosure()  const { return _funcClosure; }
    /// Returns the list vector pointer (null if not a List type).
    std::vector<NeoValue>*  asList()       const { return _list; }

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
    /// Heap-allocated vector shared by value copies (reference semantics).
    /// nullptr for all non-List types.
    std::vector<NeoValue>* _list;
};
