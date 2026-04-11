// GiacAlloc.cpp
// Optional global operator new/delete override to prefer PSRAM for Giac allocations.
// This file is only enabled when building with NUMOS_USE_GIAC and BOARD_HAS_PSRAM.

#include <new>
#include <cstdlib>
#include <cstddef>

#if defined(NUMOS_USE_GIAC) && defined(ARDUINO) && defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>

static void* giac_alloc_psram(std::size_t sz) {
    if (sz == 0) sz = 1;
    void* p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(sz, MALLOC_CAP_8BIT); // fallback to any 8-bit heap
    return p;
}

static void* giac_alloc_psram_aligned(std::size_t alignment, std::size_t sz) {
    if (sz == 0) sz = 1;
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    void* p = heap_caps_aligned_alloc(alignment, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_aligned_alloc(alignment, sz, MALLOC_CAP_8BIT);
    return p;
}

void* operator new(std::size_t sz) {
    void* p = giac_alloc_psram(sz);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    if (!p) return;
    free(p);
}

void* operator new[](std::size_t sz) {
    void* p = giac_alloc_psram(sz);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    if (!p) return;
    free(p);
}

// sized delete (C++14)
void operator delete(void* p, std::size_t) noexcept { if (p) free(p); }
void operator delete[](void* p, std::size_t) noexcept { if (p) free(p); }

// aligned allocation (C++17)
void* operator new(std::size_t sz, std::align_val_t al) {
    void* p = giac_alloc_psram_aligned(static_cast<std::size_t>(al), sz);
    if (!p) throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t sz, std::align_val_t al) {
    void* p = giac_alloc_psram_aligned(static_cast<std::size_t>(al), sz);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p, std::align_val_t) noexcept {
    if (p) free(p);
}

void operator delete[](void* p, std::align_val_t) noexcept {
    if (p) free(p);
}

void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    if (p) free(p);
}

void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    if (p) free(p);
}

void* operator new(std::size_t sz, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(sz);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t sz, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](sz);
    } catch (...) {
        return nullptr;
    }
}

void* operator new(std::size_t sz, std::align_val_t al, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(sz, al);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t sz, std::align_val_t al, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](sz, al);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    if (p) free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    if (p) free(p);
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    if (p) free(p);
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    if (p) free(p);
}

#endif // NUMOS_USE_GIAC && ARDUINO && BOARD_HAS_PSRAM
