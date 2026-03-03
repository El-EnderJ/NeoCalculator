/**
 * SymExprArena.h — Bump-pointer arena allocator for SymExpr nodes.
 *
 * Allocates a contiguous PSRAM block (default 64 KB) on construction
 * and hands out memory by bumping an offset pointer. No individual
 * deallocation — call reset() to reclaim all memory at once.
 *
 * This eliminates PSRAM fragmentation from many small SymExpr node
 * allocations (~24–48 bytes each) during CAS operations.
 *
 * On native/PC builds (NATIVE_SIM), uses standard malloc.
 *
 * Lifecycle:
 *   EquationsApp::begin()  → arena = SymExprArena(65536);
 *   solve()                → arena.alloc<SymAdd>(...);  // O(1) bump
 *   EquationsApp::end()    → arena.reset();  // or destructor
 *
 * Part of: NumOS Pro-CAS — Phase 1 (Data Structure Overhaul)
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <new>           // placement new
#include <type_traits>   // std::aligned_storage, alignment_of
#include <utility>       // std::forward
#include <vector>
#include "ConsTable.h"    // Hash-consing dedup table

#ifdef ARDUINO
  #include <esp_heap_caps.h>
  #include <mbedtls/bignum.h>   // mbedtls_mpi_free for BigInt cleanup
#endif

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymExprArena — Zero-fragmentation bump allocator for PSRAM
// ════════════════════════════════════════════════════════════════════

class SymExprArena {
public:
    static constexpr size_t DEFAULT_BLOCK_SIZE = 65536;  // 64 KB
    static constexpr size_t MAX_BLOCKS         = 16;     // 1 MB max total
    static constexpr size_t ALIGNMENT          = alignof(std::max_align_t);

    // ── Construction / Destruction ──────────────────────────────────

    explicit SymExprArena(size_t blockSize = DEFAULT_BLOCK_SIZE)
        : _blockSize(blockSize), _numBlocks(0), _offset(0)
    {
        for (size_t i = 0; i < MAX_BLOCKS; ++i)
            _blocks[i] = nullptr;
        allocateBlock();
    }

    ~SymExprArena() {
        freeAll();
    }

    // Non-copyable, non-movable
    SymExprArena(const SymExprArena&) = delete;
    SymExprArena& operator=(const SymExprArena&) = delete;
    SymExprArena(SymExprArena&&) = delete;
    SymExprArena& operator=(SymExprArena&&) = delete;

    // ── Allocation ──────────────────────────────────────────────────

    /// Allocate raw memory of `size` bytes, aligned to ALIGNMENT.
    void* allocRaw(size_t size) {
        // Align the offset
        size_t aligned = (_offset + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        if (aligned + size > _blockSize) {
            // Current block exhausted — allocate a new one
            if (!allocateBlock()) return nullptr;  // Out of blocks
            aligned = 0;
        }
        void* ptr = static_cast<uint8_t*>(_blocks[_numBlocks - 1]) + aligned;
        _offset = aligned + size;
        return ptr;
    }

    /// Allocate and construct a T in the arena.
    /// Usage: arena.create<SymNum>(ExactVal::fromInt(5))
    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocRaw(sizeof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    // ── BigInt registry ─────────────────────────────────────────────

    /// Register an mbedtls_mpi pointer so the arena frees it on reset.
    /// CASInt calls this when promoting to BigInt.
    void registerBigInt([[maybe_unused]] void* mpi) {
#ifdef ARDUINO
        _bigIntRegistry.push_back(static_cast<mbedtls_mpi*>(mpi));
#endif
    }

    // ── ConsTable accessor ────────────────────────────────────────

    /// Access the companion hash-consing table.
    ConsTable& consTable() { return _consTable; }
    const ConsTable& consTable() const { return _consTable; }

    // ── Reset (bulk free) ─────────────────────────────────────────

    /// Reset the arena — all previously allocated nodes become invalid.
    /// Clears the ConsTable, frees BigInt mpis, then reclaims arena blocks.
    void reset() {
        // 1. Clear the hash-consing table
        _consTable.clear();

        // 2. Free all registered mbedtls_mpi allocations
        freeBigInts();

        // 3. Keep only the first block, release extras
        for (size_t i = 1; i < _numBlocks; ++i) {
            freeBlock(_blocks[i]);
            _blocks[i] = nullptr;
        }
        if (_numBlocks > 1) _numBlocks = 1;
        _offset = 0;
    }

    // ── Stats ───────────────────────────────────────────────────────

    /// Snapshot of arena memory usage (for debugging / step display)
    struct ArenaStats {
        size_t totalAllocated;    ///< Total bytes allocated (blocks * blockSize)
        size_t currentBlockUsed;  ///< Bytes used in current (last) block
        size_t numBlocks;         ///< Number of PSRAM blocks in use
        size_t consTableSize;     ///< Entries in the hash-consing table
        size_t consTableCap;      ///< Capacity of the hash-consing table
        size_t bigIntCount;       ///< Number of BigInt promotions tracked

        /// Approximate total bytes actually used (all blocks).
        /// Counts full blocks (except last) + current offset in last block.
        size_t estimatedUsed(size_t blockSize) const {
            if (numBlocks == 0) return 0;
            return (numBlocks - 1) * blockSize + currentBlockUsed;
        }
    };

    /// Get a snapshot of current arena statistics.
    ArenaStats stats() const {
        ArenaStats s;
        s.totalAllocated   = _numBlocks * _blockSize;
        s.currentBlockUsed = _offset;
        s.numBlocks        = _numBlocks;
        s.consTableSize    = _consTable.size();
        s.consTableCap     = _consTable.capacity();
        s.bigIntCount      = 0;
#ifdef ARDUINO
        s.bigIntCount      = _bigIntRegistry.size();
#endif
        return s;
    }

    /// Total bytes allocated from PSRAM for this arena
    size_t totalAllocated() const { return _numBlocks * _blockSize; }

    /// Bytes used in current block
    size_t currentUsed() const { return _offset; }

    /// Number of blocks currently allocated
    size_t blockCount() const { return _numBlocks; }

private:
    size_t _blockSize;
    void*  _blocks[MAX_BLOCKS];
    size_t _numBlocks;
    size_t _offset;      // offset within current (last) block

    ConsTable _consTable;  // Hash-consing dedup table (Phase 2)

#ifdef ARDUINO
    std::vector<mbedtls_mpi*> _bigIntRegistry;  // BigInt lifecycle tracking
#endif

    bool allocateBlock() {
        if (_numBlocks >= MAX_BLOCKS) return false;

        void* block = nullptr;
#ifdef ARDUINO
        block = heap_caps_malloc(_blockSize,
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!block) {
            block = heap_caps_malloc(_blockSize, MALLOC_CAP_8BIT);
        }
#else
        block = std::malloc(_blockSize);
#endif
        if (!block) return false;

        _blocks[_numBlocks++] = block;
        _offset = 0;
        return true;
    }

    void freeBlock(void* block) {
        if (!block) return;
#ifdef ARDUINO
        heap_caps_free(block);
#else
        std::free(block);
#endif
    }

    /// Free all registered BigInt mpis.
    void freeBigInts() {
#ifdef ARDUINO
        for (auto* mpi : _bigIntRegistry) {
            if (mpi) mbedtls_mpi_free(mpi);
        }
        _bigIntRegistry.clear();
#endif
    }

    void freeAll() {
        _consTable.clear();
        freeBigInts();
        for (size_t i = 0; i < _numBlocks; ++i) {
            freeBlock(_blocks[i]);
            _blocks[i] = nullptr;
        }
        _numBlocks = 0;
        _offset = 0;
    }
};

} // namespace cas
