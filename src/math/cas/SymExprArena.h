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

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

namespace cas {

// ════════════════════════════════════════════════════════════════════
// SymExprArena — Zero-fragmentation bump allocator for PSRAM
// ════════════════════════════════════════════════════════════════════

class SymExprArena {
public:
    static constexpr size_t DEFAULT_BLOCK_SIZE = 65536;  // 64 KB
    static constexpr size_t MAX_BLOCKS         = 4;      // 256 KB max total
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

    // ── Reset (bulk free) ───────────────────────────────────────────

    /// Reset the arena — all previously allocated nodes become invalid.
    /// Does NOT free the underlying blocks (reuse for next solve cycle).
    void reset() {
        // Keep only the first block, release extras
        for (size_t i = 1; i < _numBlocks; ++i) {
            freeBlock(_blocks[i]);
            _blocks[i] = nullptr;
        }
        if (_numBlocks > 1) _numBlocks = 1;
        _offset = 0;
    }

    // ── Stats ───────────────────────────────────────────────────────

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

    void freeAll() {
        for (size_t i = 0; i < _numBlocks; ++i) {
            freeBlock(_blocks[i]);
            _blocks[i] = nullptr;
        }
        _numBlocks = 0;
        _offset = 0;
    }
};

} // namespace cas
