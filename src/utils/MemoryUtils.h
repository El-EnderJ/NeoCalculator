#pragma once

#include <cstddef>
#include <stdexcept>
#include <esp_heap_caps.h>

namespace utils {

template<typename T>
class PSRAMBuffer {
public:
    PSRAMBuffer() = default;

    explicit PSRAMBuffer(size_t count) {
        allocate(count);
    }

    PSRAMBuffer(const PSRAMBuffer&) = delete;
    PSRAMBuffer& operator=(const PSRAMBuffer&) = delete;

    PSRAMBuffer(PSRAMBuffer&& other) noexcept
        : _data(other._data), _count(other._count) {
        other._data = nullptr;
        other._count = 0;
    }

    PSRAMBuffer& operator=(PSRAMBuffer&& other) noexcept {
        if (this != &other) {
            reset();
            _data = other._data;
            _count = other._count;
            other._data = nullptr;
            other._count = 0;
        }
        return *this;
    }

    ~PSRAMBuffer() {
        reset();
    }

    bool allocate(size_t count) {
        reset();
        if (count == 0) {
            return true;
        }
        _data = reinterpret_cast<T*>(heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!_data) {
            _count = 0;
            return false;
        }
        _count = count;
        return true;
    }

    void reset() {
        if (_data) {
            heap_caps_free(_data);
            _data = nullptr;
            _count = 0;
        }
    }

    T* data() noexcept { return _data; }
    const T* data() const noexcept { return _data; }

    size_t size() const noexcept { return _count; }

    T& operator[](size_t idx) {
        if (idx >= _count) {
            throw std::out_of_range("PSRAMBuffer index out of range");
        }
        return _data[idx];
    }

    const T& operator[](size_t idx) const {
        if (idx >= _count) {
            // undefined behavior in embedded builds; we keep safe fallback.
            return _data[0];
        }
        return _data[idx];
    }

    explicit operator bool() const noexcept {
        return _data != nullptr;
    }

private:
    T*     _data  = nullptr;
    size_t _count = 0;
};

} // namespace utils
