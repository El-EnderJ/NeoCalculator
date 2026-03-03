/**
 * ConsTable.cpp — Hash-consing table implementation.
 *
 * Implements find, insert, getOrCreate, rehash and the structural
 * equality comparator that resolves hash collisions.
 *
 * Part of: NumOS Pro-CAS — Phase 2 (Immutable DAG & Hash-Consing)
 */

#include "ConsTable.h"
#include "SymExpr.h"
#include <cstring>  // memset

namespace cas {

// ════════════════════════════════════════════════════════════════════
// getNodeHash — extract the precomputed _hash from a SymExpr
// ════════════════════════════════════════════════════════════════════

size_t ConsTable::getNodeHash(const SymExpr* node) {
    return node->_hash;
}

// ════════════════════════════════════════════════════════════════════
// structurallyEqual — deep structural comparison
//
// Called only on hash collision.  For a well-distributed hash this
// is extremely rare (<1% of lookups for load factor ≤ 0.7).
// ════════════════════════════════════════════════════════════════════

bool ConsTable::structurallyEqual(const SymExpr* a, const SymExpr* b) {
    if (a == b) return true;                     // Same pointer
    if (a->type != b->type) return false;        // Different type
    if (a->_hash != b->_hash) return false;      // Different hash (fast reject)

    switch (a->type) {
        case SymExprType::Num: {
            const auto* na = static_cast<const SymNum*>(a);
            const auto* nb = static_cast<const SymNum*>(b);
            // Compare all fields that define a SymNum
            return na->_coeff.num().toInt64() == nb->_coeff.num().toInt64()
                && na->_coeff.den().toInt64() == nb->_coeff.den().toInt64()
                && na->_outer == nb->_outer
                && na->_inner == nb->_inner
                && na->_piMul == nb->_piMul
                && na->_eMul  == nb->_eMul;
        }

        case SymExprType::Var: {
            const auto* va = static_cast<const SymVar*>(a);
            const auto* vb = static_cast<const SymVar*>(b);
            return va->name == vb->name;
        }

        case SymExprType::Neg: {
            const auto* na = static_cast<const SymNeg*>(a);
            const auto* nb = static_cast<const SymNeg*>(b);
            return na->child == nb->child;  // Pointer identity (children are already cons'd)
        }

        case SymExprType::Add: {
            const auto* aa = static_cast<const SymAdd*>(a);
            const auto* ab = static_cast<const SymAdd*>(b);
            if (aa->count != ab->count) return false;
            for (uint16_t i = 0; i < aa->count; ++i) {
                if (aa->terms[i] != ab->terms[i]) return false;
            }
            return true;
        }

        case SymExprType::Mul: {
            const auto* ma = static_cast<const SymMul*>(a);
            const auto* mb = static_cast<const SymMul*>(b);
            if (ma->count != mb->count) return false;
            for (uint16_t i = 0; i < ma->count; ++i) {
                if (ma->factors[i] != mb->factors[i]) return false;
            }
            return true;
        }

        case SymExprType::Pow: {
            const auto* pa = static_cast<const SymPow*>(a);
            const auto* pb = static_cast<const SymPow*>(b);
            return pa->base == pb->base && pa->exponent == pb->exponent;
        }

        case SymExprType::Func: {
            const auto* fa = static_cast<const SymFunc*>(a);
            const auto* fb = static_cast<const SymFunc*>(b);
            return fa->kind == fb->kind && fa->argument == fb->argument;
        }

        default:
            return false;
    }
}

// ════════════════════════════════════════════════════════════════════
// find — look up an existing structurally-equal node
// ════════════════════════════════════════════════════════════════════

SymExpr* ConsTable::find(const SymExpr* candidate) const {
    if (!_buckets || _capacity == 0) return nullptr;

    size_t hash = getNodeHash(candidate);
    size_t idx  = bucketIndex(hash);

    // Linear probing
    for (size_t i = 0; i < _capacity; ++i) {
        size_t probe = (idx + i) & (_capacity - 1);
        SymExpr* slot = _buckets[probe];

        if (!slot) return nullptr;  // Empty slot → not found

        if (slot->_hash == hash && structurallyEqual(slot, candidate)) {
            return slot;  // Found existing equivalent
        }
    }

    return nullptr;  // Table full (should never happen with load factor < 0.7)
}

// ════════════════════════════════════════════════════════════════════
// insert — add a new node (caller must ensure no duplicate exists)
// ════════════════════════════════════════════════════════════════════

void ConsTable::insert(SymExpr* node) {
    if (!_buckets) return;

    // Check if rehash needed
    if (static_cast<float>(_size + 1) / static_cast<float>(_capacity)
        > MAX_LOAD_FACTOR)
    {
        rehash();
    }

    size_t hash = getNodeHash(node);
    size_t idx  = bucketIndex(hash);

    // Linear probing to find empty slot
    for (size_t i = 0; i < _capacity; ++i) {
        size_t probe = (idx + i) & (_capacity - 1);
        if (!_buckets[probe]) {
            _buckets[probe] = node;
            ++_size;
            return;
        }
    }
    // Table full — should not happen with proper load factor control
}

// ════════════════════════════════════════════════════════════════════
// getOrCreate — the primary entry point for hash-consing
// ════════════════════════════════════════════════════════════════════

SymExpr* ConsTable::getOrCreate(SymExpr* candidate) {
    if (!candidate) return nullptr;

    // Try to find an existing equivalent node
    SymExpr* existing = find(candidate);
    if (existing) return existing;  // Dedup! Candidate arena memory is wasted but harmless.

    // No equivalent exists — register the candidate
    insert(candidate);
    return candidate;
}

// ════════════════════════════════════════════════════════════════════
// rehash — grow the table when load factor exceeds threshold
// ════════════════════════════════════════════════════════════════════

void ConsTable::rehash() {
    size_t newCap = _capacity * 2;
    if (newCap > MAX_CAPACITY) newCap = MAX_CAPACITY;
    if (newCap <= _capacity) return;  // Already at max

    // Save old state
    SymExpr** oldBuckets  = _buckets;
    size_t    oldCapacity = _capacity;
    size_t    oldSize     = _size;

    // Allocate new buckets
    _buckets  = nullptr;
    _capacity = 0;
    _size     = 0;
    allocBuckets(newCap);

    if (!_buckets) {
        // Allocation failed — restore old state
        _buckets  = oldBuckets;
        _capacity = oldCapacity;
        _size     = oldSize;
        return;
    }

    // Re-insert all entries from old table
    for (size_t i = 0; i < oldCapacity; ++i) {
        if (oldBuckets[i]) {
            insert(oldBuckets[i]);
        }
    }

    // Free old bucket array
#ifdef ARDUINO
    heap_caps_free(oldBuckets);
#else
    std::free(oldBuckets);
#endif
}

} // namespace cas
