/**
 * CasMemory.h — PSRAM-backed PMR memory pool for the CAS Rule Engine.
 *
 * Phase 13A: Memory Management for the Term Rewriting System (TRS).
 *
 * This module implements the architectural recommendation for ESP32-S3 embedded
 * systems: a C++17 Polymorphic Memory Resource (std::pmr) session allocator
 * backed by a single large PSRAM slab, preventing heap fragmentation during
 * intensive rule-rewriting sessions.
 *
 * ── Design & Memory Ownership Model ─────────────────────────────────────────
 *
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  PSRAM buffer  (256 KB, heap_caps_malloc SPIRAM on ESP32)        │
 *   │  ┌──────────────────────────────────────────────────────────┐   │
 *   │  │  MonotonicBufferResource (bump-pointer, O(1) alloc)      │   │
 *   │  │  ┌──────────────────────────────────────────────────┐    │   │
 *   │  │  │  PolymorphicAllocator<byte>                       │    │   │
 *   │  │  │  └─ used by std::allocate_shared<AstNode>(...)    │    │   │
 *   │  │  └──────────────────────────────────────────────────┘    │   │
 *   │  └──────────────────────────────────────────────────────────┘   │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 *   1. CasMemoryPool OWNS the raw PSRAM buffer (RAII via destructor).
 *   2. All AstNodes and their shared_ptr control blocks are allocated
 *      from this pool via std::allocate_shared + polymorphic_allocator.
 *   3. MonotonicBufferResource::deallocate() is a no-op — individual frees
 *      are silently ignored; the entire buffer is reclaimed in O(1) by
 *      calling reset() or destroying the pool.
 *
 * ── Lifecycle ────────────────────────────────────────────────────────────────
 *
 *   // At session start (e.g. user taps "Solve"):
 *   CasMemoryPool pool;                     // allocates 256 KB PSRAM slab
 *   auto alloc = pool.allocator();          // lightweight handle, no alloc
 *
 *   // During rewriting:
 *   auto node = makeConstant(alloc, 3.14);  // O(1) bump allocation
 *
 *   // IMPORTANT: drop all NodePtrs BEFORE reset/destruction:
 *   nodePtr.reset();                        // decrement shared_ptr refcount
 *   pool.reset();                           // bulk-free, O(1), no fragmentation
 *   // (or just let pool go out of scope — destructor calls reset)
 *
 * ── Platform abstraction ─────────────────────────────────────────────────────
 *
 *   On ESP32-S3:  heap_caps_malloc(MALLOC_CAP_SPIRAM) → PSRAM slab.
 *   On PC/native: std::malloc → heap slab (NATIVE_SIM / no ARDUINO macro).
 *
 *   If the compiler ships <memory_resource> (GCC 9+, Clang 9+, MSVC 2017+),
 *   std::pmr types are used directly.  Older toolchains (e.g. Arduino-ESP32
 *   with GCC 8.x) use the drop-in cas::pmr fallback defined in this header.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13A (TRS Infrastructure)
 */

#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>
#include <memory>      // std::shared_ptr, std::allocate_shared

#ifdef ARDUINO
#  include <esp_heap_caps.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Detect std::pmr availability (C++17 + library support)
// GCC ≥ 9, Clang ≥ 9, MSVC ≥ 2017 all ship <memory_resource>.
// Arduino-ESP32 GCC 8.x does NOT.  We detect at compile time.
// ─────────────────────────────────────────────────────────────────────────────
#if __has_include(<memory_resource>)
#  include <memory_resource>
#  define CAS_HAS_PMR 1
#else
#  define CAS_HAS_PMR 0
#endif

namespace cas {

// ═════════════════════════════════════════════════════════════════════════════
// §1 — cas::pmr shim: exposes std::pmr when available, otherwise provides a
//      self-contained monotonic-buffer + polymorphic-allocator implementation
//      that is API-compatible with std::pmr for our use-case.
// ═════════════════════════════════════════════════════════════════════════════

namespace pmr {

#if CAS_HAS_PMR

// ── When the standard library provides <memory_resource>, alias directly. ──
using memory_resource          = std::pmr::memory_resource;
using monotonic_buffer_resource = std::pmr::monotonic_buffer_resource;

template <typename T = std::byte>
using polymorphic_allocator    = std::pmr::polymorphic_allocator<T>;

#else  // ---------- Fallback for GCC 8 / Arduino-ESP32 older toolchains ------

// ── memory_resource — abstract base ─────────────────────────────────────────
class memory_resource {
public:
    virtual ~memory_resource() = default;

    void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
        return do_allocate(bytes, alignment);
    }
    void deallocate(void* p, std::size_t bytes,
                    std::size_t alignment = alignof(std::max_align_t)) noexcept {
        do_deallocate(p, bytes, alignment);
    }
    bool is_equal(const memory_resource& other) const noexcept {
        return do_is_equal(other);
    }

    bool operator==(const memory_resource& other) const noexcept { return is_equal(other); }
    bool operator!=(const memory_resource& other) const noexcept { return !is_equal(other); }

protected:
    virtual void* do_allocate(std::size_t bytes, std::size_t alignment) = 0;
    virtual void  do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept = 0;
    virtual bool  do_is_equal(const memory_resource& other) const noexcept = 0;
};

// ── monotonic_buffer_resource — bump-pointer allocator, no-op dealloc ───────
//
// Allocates from a caller-supplied buffer by bumping a pointer.  Deallocation
// is a no-op; the entire buffer is reclaimed by destroying this resource or
// calling the owner's reset().  Requests that overflow the buffer fall through
// to an upstream memory_resource (default: operator new).
class monotonic_buffer_resource : public memory_resource {
public:
    // Construct over an existing buffer; upstream is used on overflow.
    monotonic_buffer_resource(void* buffer, std::size_t size,
                               memory_resource* upstream = nullptr)
        : _buffer(static_cast<char*>(buffer))
        , _size(size)
        , _offset(0)
        , _upstream(upstream)
    {}

    // Construct without a pre-allocated buffer — all allocations go to upstream.
    explicit monotonic_buffer_resource(memory_resource* upstream = nullptr)
        : _buffer(nullptr), _size(0), _offset(0), _upstream(upstream) {}

    // Non-copyable, non-movable (owns logical state of the bump pointer)
    monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;
    monotonic_buffer_resource& operator=(const monotonic_buffer_resource&) = delete;

    // Reset the bump pointer to the start of the buffer.
    // Does NOT free upstream allocations made on overflow.
    void release() noexcept { _offset = 0; }

    memory_resource* upstream_resource() const noexcept { return _upstream; }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        // Align the current offset
        std::size_t aligned = (_offset + alignment - 1) & ~(alignment - 1);
        if (_buffer && aligned + bytes <= _size) {
            void* ptr = _buffer + aligned;
            _offset = aligned + bytes;
            return ptr;
        }
        // Overflow: fall back to upstream (operator new if nullptr)
        if (_upstream) {
            return _upstream->allocate(bytes, alignment);
        }
        return ::operator new(bytes);
    }

    void do_deallocate(void* /*p*/, std::size_t /*bytes*/,
                       std::size_t /*alignment*/) noexcept override {
        // Intentional no-op: memory is reclaimed by release() / pool destructor.
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    char*         _buffer;
    std::size_t   _size;
    std::size_t   _offset;
    memory_resource* _upstream;
};

// ── polymorphic_allocator<T> — STL allocator wrapping a memory_resource ─────
template <typename T = std::byte>
class polymorphic_allocator {
public:
    using value_type = T;

    explicit polymorphic_allocator(memory_resource* r) noexcept : _resource(r) {}

    // Rebind copy-constructor (required by std::allocate_shared)
    template <typename U>
    polymorphic_allocator(const polymorphic_allocator<U>& other) noexcept
        : _resource(other.resource()) {}

    memory_resource* resource() const noexcept { return _resource; }

    T* allocate(std::size_t n) {
        return static_cast<T*>(
            _resource->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept {
        _resource->deallocate(p, n * sizeof(T), alignof(T));
    }

    // Required by STL: returns a rebound allocator for a different type
    template <typename U>
    polymorphic_allocator<U> rebind_alloc() const noexcept {
        return polymorphic_allocator<U>(_resource);
    }

    bool operator==(const polymorphic_allocator& other) const noexcept {
        return _resource == other._resource;
    }
    bool operator!=(const polymorphic_allocator& other) const noexcept {
        return _resource != other._resource;
    }

private:
    memory_resource* _resource;
};

#endif  // CAS_HAS_PMR

}  // namespace pmr

// ═════════════════════════════════════════════════════════════════════════════
// §2 — CasMemoryPool
//
// RAII owner of a PSRAM slab + monotonic_buffer_resource session allocator.
// One pool instance per solve-session: create before solving, destroy/reset
// after all NodePtrs have been dropped.
// ═════════════════════════════════════════════════════════════════════════════

class CasMemoryPool {
public:
    /// Default pool size: 256 KB.  Sufficient for ~4 000 AST nodes of 64 bytes
    /// each (control block + payload) during a typical algebraic rewriting session.
    static constexpr std::size_t DEFAULT_SIZE = 256 * 1024;  // 256 KB

    // ── Construction ────────────────────────────────────────────────────────

    /// Allocate a PSRAM slab (ESP32) or heap slab (PC) of `bufferSize` bytes
    /// and initialise the monotonic buffer resource over it.
    explicit CasMemoryPool(std::size_t bufferSize = DEFAULT_SIZE)
        : _bufSize(bufferSize)
        , _buffer(allocPSRAM(bufferSize))
        , _mbr(_buffer, bufferSize)
    {}

    // Non-copyable, non-movable — owns the PSRAM slab
    CasMemoryPool(const CasMemoryPool&) = delete;
    CasMemoryPool& operator=(const CasMemoryPool&) = delete;
    CasMemoryPool(CasMemoryPool&&) = delete;
    CasMemoryPool& operator=(CasMemoryPool&&) = delete;

    ~CasMemoryPool() {
        reset();
        freePSRAM(_buffer);
        _buffer = nullptr;
    }

    // ── Allocator access ────────────────────────────────────────────────────

    /// Returns a polymorphic_allocator<std::byte> backed by this pool.
    /// Lightweight handle — cheap to copy, no allocation.
    pmr::polymorphic_allocator<std::byte> allocator() noexcept {
        return pmr::polymorphic_allocator<std::byte>(&_mbr);
    }

    /// Direct access to the underlying memory_resource (for advanced use).
    pmr::memory_resource* resource() noexcept { return &_mbr; }

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /// Reset the bump pointer to the start of the buffer.
    ///
    /// ⚠ SAFETY CONTRACT: All NodePtrs (std::shared_ptr<const AstNode>)
    ///   allocated from this pool MUST be reset/destroyed before calling
    ///   reset().  Surviving shared_ptrs will point to freed memory.
    void reset() noexcept {
        _mbr.release();
    }

    // ── Statistics ───────────────────────────────────────────────────────────

    /// Total capacity of the backing buffer in bytes.
    std::size_t capacity() const noexcept { return _bufSize; }

    /// True if the backing buffer was successfully allocated.
    bool valid() const noexcept { return _buffer != nullptr; }

private:
    std::size_t               _bufSize;
    void*                     _buffer;
    pmr::monotonic_buffer_resource _mbr;

    // ── PSRAM platform helpers ───────────────────────────────────────────────

    static void* allocPSRAM(std::size_t size) {
#ifdef ARDUINO
        // Prefer 8-byte-capable PSRAM; fall back to any available memory.
        void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) {
            p = heap_caps_malloc(size, MALLOC_CAP_8BIT);
        }
        return p;
#else
        return std::malloc(size);
#endif
    }

    static void freePSRAM(void* p) noexcept {
        if (!p) return;
#ifdef ARDUINO
        heap_caps_free(p);
#else
        std::free(p);
#endif
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// §3 — Convenience helper: allocate a shared_ptr<T> from a CasMemoryPool
//
// Usage:
//   auto node = cas::allocateNode<ConstantNode>(pool, 3.14);
//
// Both the T object and the shared_ptr control block are allocated from the
// pool's monotonic buffer — zero heap fragmentation.
// ═════════════════════════════════════════════════════════════════════════════

template <typename T, typename... Args>
std::shared_ptr<T> allocateNode(CasMemoryPool& pool, Args&&... args) {
    pmr::polymorphic_allocator<T> alloc(pool.resource());
    return std::allocate_shared<T>(alloc, std::forward<Args>(args)...);
}

}  // namespace cas
