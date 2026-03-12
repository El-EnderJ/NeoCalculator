/**
 * NeoAST.h — Abstract Syntax Tree for the NeoLanguage compiler frontend.
 *
 * Provides:
 *   · NeoArena  — PSRAM-backed bump allocator (linked-list of blocks)
 *   · NeoNode   — base AST node with kind/line/col
 *   · Concrete node types for all language constructs
 *
 * All AST nodes MUST be allocated via NeoArena::make<T>(...).
 * No new/delete usage — the arena bulk-frees on reset().
 *
 * PSRAM strategy: on Arduino/ESP32-S3, blocks are heap_caps_malloc'd
 * from SPIRAM; on native/PC builds, falls back to std::malloc.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <new>          // placement new
#include <utility>      // std::forward

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

#include "../math/cas/PSRAMAllocator.h"

// ════════════════════════════════════════════════════════════════════
// NeoArena — PSRAM bump allocator with linked-list of blocks
// ════════════════════════════════════════════════════════════════════

/**
 * NeoArena is a region-based allocator.
 *
 * Memory is carved out sequentially from large blocks allocated on PSRAM.
 * When a block is exhausted a new one is chained. reset() returns all
 * memory in O(1) without running any destructors.
 *
 * NOT thread-safe (single-threaded Arduino usage).
 */
class NeoArena {
public:
    // Default block size — 32 KB fits comfortably in PSRAM
    static constexpr size_t DEFAULT_BLOCK = 32 * 1024;

    explicit NeoArena(size_t blockSize = DEFAULT_BLOCK)
        : _blockSize(blockSize), _head(nullptr), _cur(nullptr), _end(nullptr)
    {}

    // Non-copyable
    NeoArena(const NeoArena&)            = delete;
    NeoArena& operator=(const NeoArena&) = delete;

    ~NeoArena() { freeAll(); }

    // ── allocate(bytes) ───────────────────────────────────────────
    /**
     * Allocate `bytes` bytes, pointer-aligned to alignof(max_align_t).
     * Returns nullptr only if allocation of a new block fails.
     */
    void* allocate(size_t bytes) {
        // Align up to max_align_t boundary
        bytes = alignUp(bytes);

        if (_cur + bytes > _end) {
            if (!newBlock(bytes)) return nullptr;
        }

        void* ptr = _cur;
        _cur += bytes;
        return ptr;
    }

    // ── make<T>(args...) ─────────────────────────────────────────
    /**
     * Construct a T in arena memory via placement new.
     * Returns nullptr if allocation fails.
     */
    template<typename T, typename... Args>
    T* make(Args&&... args) {
        void* mem = allocate(sizeof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    // ── reset() ──────────────────────────────────────────────────
    /**
     * Release all allocations in O(1).
     * Does NOT run destructors — nodes must be trivially destructible
     * or callers must manually destruct before reset().
     */
    void reset() {
        freeAll();
        _head = nullptr;
        _cur  = nullptr;
        _end  = nullptr;
    }

    // Total bytes allocated across all blocks (for diagnostics)
    size_t totalAllocated() const {
        size_t total = 0;
        Block* b = _head;
        while (b) { total += b->capacity; b = b->next; }
        return total;
    }

private:
    // ── Internal block header ─────────────────────────────────────
    struct Block {
        Block*  next;
        size_t  capacity;
        // data follows immediately in memory
    };

    size_t _blockSize;
    Block* _head;
    uint8_t* _cur;
    uint8_t* _end;

    // ── Align up to max_align_t ───────────────────────────────────
    static size_t alignUp(size_t n) {
        constexpr size_t align = alignof(std::max_align_t);
        return (n + align - 1) & ~(align - 1);
    }

    // ── Allocate a new block large enough for `minBytes` ─────────
    bool newBlock(size_t minBytes) {
        size_t cap = _blockSize;
        if (minBytes + sizeof(Block) > cap)
            cap = alignUp(minBytes + sizeof(Block));

        void* raw = nullptr;
#ifdef ARDUINO
        raw = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!raw)
            raw = heap_caps_malloc(cap, MALLOC_CAP_8BIT);
#else
        raw = std::malloc(cap);
#endif
        if (!raw) return false;

        Block* b = static_cast<Block*>(raw);
        b->next     = _head;
        b->capacity = cap;
        _head = b;

        // Data region starts right after the header
        _cur = reinterpret_cast<uint8_t*>(b) + alignUp(sizeof(Block));
        _end = reinterpret_cast<uint8_t*>(b) + cap;
        return true;
    }

    // ── Free all blocks ───────────────────────────────────────────
    void freeAll() {
        Block* b = _head;
        while (b) {
            Block* next = b->next;
#ifdef ARDUINO
            heap_caps_free(b);
#else
            std::free(b);
#endif
            b = next;
        }
    }
};

// ════════════════════════════════════════════════════════════════════
// NodeKind — discriminated union tag for all AST node types
// ════════════════════════════════════════════════════════════════════

enum class NodeKind : uint8_t {
    Number,
    Symbol,
    String,         ///< string literal (Phase 6)
    BinaryOp,
    UnaryOp,
    FunctionCall,
    Assignment,
    If,
    While,
    FunctionDef,
    SymExprWrapper,
    Return,
    Program,
    ForIn,
    IndexOp,        ///< list / matrix indexing: expr[i] or expr[i, j]
    DictLiteral,    ///< dictionary literal: { "key": expr, ... }
    TryExcept,      ///< try/except block (Phase 6)
    Import,         ///< import module [as alias] / from module import name,... (Phase 8)
};

// ════════════════════════════════════════════════════════════════════
// NeoNode — abstract base for all AST nodes
// ════════════════════════════════════════════════════════════════════

/**
 * All AST nodes inherit from NeoNode.
 * Fields: kind (discriminator), line and column of source origin.
 * No virtual destructor — lifetime managed by NeoArena.
 */
struct NeoNode {
    NodeKind kind;
    int      line;
    int      col;

    NeoNode(NodeKind k, int l, int c) : kind(k), line(l), col(c) {}
};

// ════════════════════════════════════════════════════════════════════
// Convenience alias: PSRAM-backed vector of NeoNode*
// ════════════════════════════════════════════════════════════════════

using NeoNodeVec = std::vector<NeoNode*, cas::PSRAMAllocator<NeoNode*>>;
using StringVec  = std::vector<std::string, cas::PSRAMAllocator<std::string>>;

// ════════════════════════════════════════════════════════════════════
// NumberNode — numeric literal
// ════════════════════════════════════════════════════════════════════

/**
 * Represents an integer or floating-point literal.
 * raw_text preserves the original source text for re-printing.
 * num/den provide exact CASRational representation when the value
 * is an integer or simple fraction (den == 0 means "not rational").
 */
struct NumberNode : NeoNode {
    double   value;       ///< Parsed double value
    std::string raw_text; ///< Original source token text
    int64_t  num;         ///< CASRational numerator (exact, when applicable)
    int64_t  den;         ///< CASRational denominator (0 = not exact rational)

    NumberNode(double v, const std::string& raw, int l, int c)
        : NeoNode(NodeKind::Number, l, c)
        , value(v), raw_text(raw)
        , num(0), den(0)
    {}
};

// ════════════════════════════════════════════════════════════════════
// SymbolNode — variable / identifier reference
// ════════════════════════════════════════════════════════════════════

struct SymbolNode : NeoNode {
    std::string name;

    SymbolNode(const std::string& n, int l, int c)
        : NeoNode(NodeKind::Symbol, l, c), name(n) {}
};

// ════════════════════════════════════════════════════════════════════
// StringNode — string literal value (Phase 6)
// ════════════════════════════════════════════════════════════════════

struct StringNode : NeoNode {
    std::string value; ///< The string content (escape sequences already resolved)

    StringNode(const std::string& v, int l, int c)
        : NeoNode(NodeKind::String, l, c), value(v) {}
};

// ════════════════════════════════════════════════════════════════════
// BinaryOpNode — infix binary operation
// ════════════════════════════════════════════════════════════════════

struct BinaryOpNode : NeoNode {
    enum class OpKind : uint8_t {
        Add, Sub, Mul, Div, Pow,
        Eq, Ne, Lt, Le, Gt, Ge,
        And, Or,
        BitAnd, BitOr, BitXor, LShift, RShift  ///< Bitwise operators (Phase 6)
    };

    OpKind   op;
    NeoNode* left;
    NeoNode* right;

    BinaryOpNode(OpKind o, NeoNode* l, NeoNode* r, int line, int col)
        : NeoNode(NodeKind::BinaryOp, line, col)
        , op(o), left(l), right(r) {}
};

// ════════════════════════════════════════════════════════════════════
// UnaryOpNode — prefix unary operation
// ════════════════════════════════════════════════════════════════════

struct UnaryOpNode : NeoNode {
    enum class OpKind : uint8_t { Neg, Not, BitNot };  ///< BitNot = ~ (Phase 6)

    OpKind   op;
    NeoNode* operand;

    UnaryOpNode(OpKind o, NeoNode* operand, int line, int col)
        : NeoNode(NodeKind::UnaryOp, line, col)
        , op(o), operand(operand) {}
};

// ════════════════════════════════════════════════════════════════════
// FunctionCallNode — f(arg1, arg2, ...)
// ════════════════════════════════════════════════════════════════════

struct FunctionCallNode : NeoNode {
    std::string name;
    std::vector<NeoNode*, cas::PSRAMAllocator<NeoNode*>> args;

    FunctionCallNode(const std::string& n, int line, int col)
        : NeoNode(NodeKind::FunctionCall, line, col), name(n) {}
};

// ════════════════════════════════════════════════════════════════════
// AssignmentNode — x = expr  or  x := expr (delayed/symbolic)
// ════════════════════════════════════════════════════════════════════

struct AssignmentNode : NeoNode {
    std::string target;
    NeoNode*    value;
    bool        is_delayed; ///< true for :=, false for =

    AssignmentNode(const std::string& t, NeoNode* v, bool delayed, int line, int col)
        : NeoNode(NodeKind::Assignment, line, col)
        , target(t), value(v), is_delayed(delayed) {}
};

// ════════════════════════════════════════════════════════════════════
// IfNode — if / elif / else
// ════════════════════════════════════════════════════════════════════

struct IfNode : NeoNode {
    NeoNode*  condition;
    NeoNodeVec then_body;
    NeoNodeVec else_body; ///< May contain a single IfNode for elif chains

    IfNode(NeoNode* cond, int line, int col)
        : NeoNode(NodeKind::If, line, col)
        , condition(cond) {}
};

// ════════════════════════════════════════════════════════════════════
// WhileNode — while loop
// ════════════════════════════════════════════════════════════════════

struct WhileNode : NeoNode {
    NeoNode*  condition;
    NeoNodeVec body;

    WhileNode(NeoNode* cond, int line, int col)
        : NeoNode(NodeKind::While, line, col)
        , condition(cond) {}
};

// ════════════════════════════════════════════════════════════════════
// ForInNode — for var in iterable
// ════════════════════════════════════════════════════════════════════

struct ForInNode : NeoNode {
    std::string var;
    NeoNode*    iterable;
    NeoNodeVec  body;

    ForInNode(const std::string& v, NeoNode* iter, int line, int col)
        : NeoNode(NodeKind::ForIn, line, col)
        , var(v), iterable(iter) {}
};

// ════════════════════════════════════════════════════════════════════
// FunctionDefNode — def f(params): body  or  f(params) := expr
// ════════════════════════════════════════════════════════════════════

struct FunctionDefNode : NeoNode {
    std::string name;
    StringVec   params;
    NeoNodeVec  body;
    bool        is_one_liner; ///< true for f(x) := expr syntax

    FunctionDefNode(const std::string& n, bool one_liner, int line, int col)
        : NeoNode(NodeKind::FunctionDef, line, col)
        , name(n), is_one_liner(one_liner) {}
};

// ════════════════════════════════════════════════════════════════════
// ReturnNode — return [value]
// ════════════════════════════════════════════════════════════════════

struct ReturnNode : NeoNode {
    NeoNode* value; ///< May be nullptr for bare `return`

    ReturnNode(NeoNode* v, int line, int col)
        : NeoNode(NodeKind::Return, line, col), value(v) {}
};

// ════════════════════════════════════════════════════════════════════
// SymExprWrapperNode — wraps a Pro-CAS SymExpr* (forward-declared)
// ════════════════════════════════════════════════════════════════════

/**
 * Avoids circular inclusion of SymExpr.h inside NeoAST.h.
 * The actual SymExpr pointer is stored as void* and must be cast
 * to SymExpr* by code that includes both headers.
 */
struct SymExprWrapperNode : NeoNode {
    void*       symexpr_ptr; ///< cast to SymExpr* when needed
    std::string repr;        ///< human-readable string representation

    SymExprWrapperNode(void* ptr, const std::string& r, int line, int col)
        : NeoNode(NodeKind::SymExprWrapper, line, col)
        , symexpr_ptr(ptr), repr(r) {}
};

// ════════════════════════════════════════════════════════════════════
// ProgramNode — top-level list of statements
// ════════════════════════════════════════════════════════════════════

struct ProgramNode : NeoNode {
    NeoNodeVec statements;

    ProgramNode(int line, int col)
        : NeoNode(NodeKind::Program, line, col) {}
};

// ════════════════════════════════════════════════════════════════════
// IndexOpNode — subscript / index access: target[i] or target[i, j]
// ════════════════════════════════════════════════════════════════════

/**
 * Represents a subscript operation on a list or matrix.
 *
 * Examples:
 *   L[0]       → target = L,  indices = [0]
 *   M[1, 2]    → target = M,  indices = [1, 2]
 *   L[i + 1]   → target = L,  indices = [i+1]
 */
struct IndexOpNode : NeoNode {
    NeoNode*  target;   ///< The collection being indexed
    NeoNodeVec indices; ///< One index per dimension (1 for lists, 2 for matrices)

    IndexOpNode(NeoNode* t, int line, int col)
        : NeoNode(NodeKind::IndexOp, line, col), target(t) {}
};

// ════════════════════════════════════════════════════════════════════
// DictLiteralNode — dictionary literal: { "key": value, ... }
// ════════════════════════════════════════════════════════════════════

/**
 * Represents a dictionary (hash map) literal.
 *
 * Keys are stored as raw strings (identifier or string literal text).
 * Values are unevaluated AST nodes.
 *
 * Examples:
 *   {"power": 500, "unit": "W"}
 *   {x: 1, y: 2}
 */
struct DictLiteralNode : NeoNode {
    StringVec  keys;   ///< Raw key strings
    NeoNodeVec values; ///< Value expressions (parallel to keys)

    DictLiteralNode(int l, int c)
        : NeoNode(NodeKind::DictLiteral, l, c) {}
};

// ════════════════════════════════════════════════════════════════════
// TryExceptNode — try/except exception handling block (Phase 6)
// ════════════════════════════════════════════════════════════════════

/**
 * Represents a try/except block.
 *
 * Syntax:
 *   try:
 *       <try_body>
 *   except [Type] as <var>:
 *       <except_body>
 *
 * On runtime error in try_body, execution jumps to except_body
 * with the error message bound to <var> (if given).
 */
struct TryExceptNode : NeoNode {
    NeoNodeVec  try_body;    ///< Statements in the try block
    std::string except_var;  ///< Variable name for the exception (may be empty)
    NeoNodeVec  except_body; ///< Statements in the except block

    TryExceptNode(int l, int c)
        : NeoNode(NodeKind::TryExcept, l, c) {}
};

// ════════════════════════════════════════════════════════════════════
// ImportNode — import module [as alias]  or  from module import a, b
// (Phase 8: Module System)
// ════════════════════════════════════════════════════════════════════

/**
 * Represents a module import statement.
 *
 * Three forms:
 *   import finance                  → is_from=false, alias=""
 *   import finance as fin           → is_from=false, alias="fin"
 *   from math import pi, sin        → is_from=true,  names=[pi, sin]
 */
struct ImportNode : NeoNode {
    std::string module;    ///< Module name to import (e.g. "finance", "math")
    std::string alias;     ///< Alias for namespace binding (empty = use module name)
    StringVec   names;     ///< Names to import for 'from X import a, b' (empty = import all)
    bool        is_from;   ///< true = from X import ..., false = import X [as Y]

    ImportNode(const std::string& mod, int l, int c)
        : NeoNode(NodeKind::Import, l, c)
        , module(mod), is_from(false) {}
};
