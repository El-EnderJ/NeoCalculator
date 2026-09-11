// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>
#if defined(ARDUINO) && !defined(NATIVE_SIM)
#include <esp_heap_caps.h>
#endif
namespace numos::tutor {
struct AllocationStats {
    size_t live = 0, peak = 0, allocations = 0;
};
// The engine is single-threaded and non-reentrant. These are counters, not an
// arena; each allocation is freed with its vector, independently of a view.
inline AllocationStats traceAllocations;
inline bool traceConstructionActive = false;
inline size_t traceConstructionBaseline = 0;
struct ConstructionBudget {
    ConstructionBudget() {
        traceConstructionBaseline = traceAllocations.live;
        traceConstructionActive = true;
    }
    ~ConstructionBudget() {
        traceConstructionActive = false;
    }
};
template <class T> struct TraceAllocator {
    using value_type = T;
    TraceAllocator() noexcept = default;
    template <class U> TraceAllocator(const TraceAllocator<U> &) noexcept {}
    T *allocate(size_t n) {
        if (n > 65536 / sizeof(T))
            throw std::bad_alloc();
        const size_t bytes = n * sizeof(T);
        // Separate retained (64 KB) and transient vector-payload (128 KB) budgets.
        if (traceConstructionActive &&
            traceAllocations.live - traceConstructionBaseline + bytes > 128 * 1024)
            throw std::bad_alloc();
#if defined(ARDUINO) && !defined(NATIVE_SIM)
        // WHY: on-demand structural storage belongs in PSRAM; do not compete with
        // the independent 64 KB LVGL pool or reserve a large global arena.
        void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        void *p = std::malloc(bytes);
#endif
        if (!p)
            throw std::bad_alloc();
        traceAllocations.live += bytes;
        if (traceAllocations.live > traceAllocations.peak)
            traceAllocations.peak = traceAllocations.live;
        ++traceAllocations.allocations;
        return static_cast<T *>(p);
    }
    void deallocate(T *p, size_t n) noexcept {
        traceAllocations.live -= n * sizeof(T);
#if defined(ARDUINO) && !defined(NATIVE_SIM)
        heap_caps_free(p);
#else
        std::free(p);
#endif
    }
    template <class U> bool operator==(const TraceAllocator<U> &) const noexcept {
        return true;
    }
    template <class U> bool operator!=(const TraceAllocator<U> &) const noexcept {
        return false;
    }
};
template <class T> using Vector = std::vector<T, TraceAllocator<T>>;
} // namespace numos::tutor
