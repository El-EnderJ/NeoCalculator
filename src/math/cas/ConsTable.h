/**
 * ConsTable.h — Hash-consing deduplication table for SymExpr DAG.
 *
 * Ensures structural uniqueness: if two SymExpr nodes are semantically
 * identical (same type, same children/values), only one physical copy
 * exists in the arena.  This enables O(1) equality via pointer identity:
 *
 *     if (a == b)  →  structurally identical (guaranteed by cons)
 *
 * Design:
 *   · Open-addressing hash map (Robin Hood probing) backed by PSRAM.
 *   · Keys are the precomputed `_hash` stored in every SymExpr node.
 *   · On collision, full structural equality is checked.
 *   · Load factor ≤ 0.70 → automatic rehash (double capacity).
 *   · Lifecycle tied to SymExprArena — `reset()` clears both.
 *
 * Memory: Each bucket = 1 pointer (4 bytes on ESP32).
 *         Default 4096 buckets → 16 KB initial.
 *         Max capacity scales to ~32K entries → 128 KB.
 *
 * Part of: NumOS Pro-CAS — Phase 2 (Immutable DAG & Hash-Consing)
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>   // memset

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#else
  #include <cstdlib>
#endif

namespace cas {

// Forward declaration — full definition in SymExpr.h
class SymExpr;

// ════════════════════════════════════════════════════════════════════
// Hash utilities — splitmix64 finalizer for high-quality mixing
// ════════════════════════════════════════════════════════════════════

/// SplitMix64 finalizer — excellent avalanche, fast on 32-bit too.
inline size_t hashMix(size_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

/// Combine two hashes (order-dependent).
inline size_t hashCombine(size_t seed, size_t value) {
    // Boost-style combine adapted for 64-bit
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 12) + (seed >> 4);
    return seed;
}

/// Combine two hashes (order-independent / commutative).
/// Used for SymAdd terms and SymMul factors so that
/// a + b and b + a hash identically.
inline size_t hashCombineCommutative(size_t a, size_t b) {
    return a ^ b;  // XOR is commutative and associative
}

// ════════════════════════════════════════════════════════════════════
// ConsTable — Open-addressing hash-consing table
// ════════════════════════════════════════════════════════════════════

class ConsTable {
public:
    static constexpr size_t INITIAL_CAPACITY = 4096;
    static constexpr size_t MAX_CAPACITY     = 32768;
    static constexpr float  MAX_LOAD_FACTOR  = 0.70f;

    // ── Construction / Destruction ──────────────────────────────────

    ConsTable()
        : _buckets(nullptr), _capacity(0), _size(0)
    {
        allocBuckets(INITIAL_CAPACITY);
    }

    ~ConsTable() {
        freeBuckets();
    }

    // Non-copyable
    ConsTable(const ConsTable&) = delete;
    ConsTable& operator=(const ConsTable&) = delete;

    // ── Core API ────────────────────────────────────────────────────

    /**
     * Look up a candidate node by its precomputed hash.
     * If a structurally-equal node already exists, return it.
     * Otherwise return nullptr (the candidate should be kept).
     *
     * @param candidate  Arena-allocated node with `_hash` already set.
     * @return  Existing equivalent node, or nullptr if candidate is new.
     *
     * Usage pattern in cons factories:
     *   SymExpr* existing = table.find(candidate);
     *   if (existing) return existing;   // dedup — candidate is wasted arena space
     *   table.insert(candidate);
     *   return candidate;
     */
    SymExpr* find(const SymExpr* candidate) const;

    /**
     * Insert a node into the table.  Caller must ensure the node is
     * not already present (call find() first).
     */
    void insert(SymExpr* node);

    /**
     * Combined find-or-insert.  The primary entry point.
     * Returns the canonical (unique) pointer for this expression.
     *
     * If an equivalent node exists → returns the existing one.
     * If not → inserts `candidate` and returns it.
     *
     * Note: on dedup, the candidate's arena memory is "wasted" (a few
     * dozen bytes).  This is acceptable for bump allocators.
     */
    SymExpr* getOrCreate(SymExpr* candidate);

    /**
     * Clear all entries.  Called by SymExprArena::reset().
     * Does NOT free the bucket array itself — allows reuse.
     */
    void clear() {
        if (_buckets && _capacity > 0) {
            std::memset(_buckets, 0, _capacity * sizeof(SymExpr*));
        }
        _size = 0;
    }

    // ── Stats ───────────────────────────────────────────────────────

    size_t size()     const { return _size; }
    size_t capacity() const { return _capacity; }

    float loadFactor() const {
        return _capacity > 0
            ? static_cast<float>(_size) / static_cast<float>(_capacity)
            : 0.0f;
    }

private:
    SymExpr** _buckets;
    size_t    _capacity;
    size_t    _size;

    // ── Internal helpers ────────────────────────────────────────────

    /// Get the precomputed hash from a SymExpr node.
    /// Defined out-of-line after SymExpr.h is included (see bottom).
    static size_t getNodeHash(const SymExpr* node);

    /// Structural equality check between two SymExpr nodes.
    /// Required to resolve hash collisions.
    static bool structurallyEqual(const SymExpr* a, const SymExpr* b);

    /// Bucket index from hash.
    size_t bucketIndex(size_t hash) const {
        return hash & (_capacity - 1);  // capacity is always power-of-2
    }

    /// Grow the table when load factor exceeds threshold.
    void rehash();

    /// Allocate bucket array.
    void allocBuckets(size_t cap) {
        size_t bytes = cap * sizeof(SymExpr*);
#ifdef ARDUINO
        _buckets = static_cast<SymExpr**>(
            heap_caps_calloc(cap, sizeof(SymExpr*),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!_buckets) {
            _buckets = static_cast<SymExpr**>(
                heap_caps_calloc(cap, sizeof(SymExpr*), MALLOC_CAP_8BIT));
        }
#else
        _buckets = static_cast<SymExpr**>(std::calloc(cap, sizeof(SymExpr*)));
#endif
        _capacity = _buckets ? cap : 0;
        (void)bytes;
    }

    /// Free the bucket array.
    void freeBuckets() {
        if (!_buckets) return;
#ifdef ARDUINO
        heap_caps_free(_buckets);
#else
        std::free(_buckets);
#endif
        _buckets  = nullptr;
        _capacity = 0;
        _size     = 0;
    }
};

} // namespace cas
