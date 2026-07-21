/*
 * MATH-HW-UX-01 hardware-acceptance timing probe.
 *
 * Compiled only by the dedicated acceptance environment. Production builds
 * reduce this type to a zero-cost no-op. The probe deliberately records no
 * expression or result text; a stable FNV-1a hash lets the acceptance runner
 * distinguish expected result payloads without leaking user mathematics.
 */
#pragma once

#if defined(NUMOS_HW_UX_ACCEPTANCE) && defined(ARDUINO)

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "input/NumosSerialBackend.h"

namespace numos {

inline uint32_t hwUxHash(const char* text) {
    uint32_t hash = 2166136261u;
    if (!text) return hash;
    while (*text) {
        hash ^= static_cast<uint8_t>(*text++);
        hash *= 16777619u;
    }
    return hash;
}

class HwUxProbe {
public:
    HwUxProbe(const char* app, const char* action)
        : _app(app), _action(action), _startUs(micros()),
          _intBefore(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          _psBefore(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
          _stackBefore(uxTaskGetStackHighWaterMark(nullptr)) {}

    ~HwUxProbe() {
        if (!_finished) finish("aborted", "none", "unknown", 0u);
    }

    void finish(const char* status, const char* kind, const char* backend,
                uint32_t resultHash) {
        if (_finished) return;
        const uint32_t resultReadyUs = micros() - _startUs;

        // WHY: the hardware display path is synchronous (DMA is disabled), so
        // returning from lv_refr_now() means the first invalidated result pixels
        // have physically traversed the SPI path to the ILI9341.
        lv_refr_now(nullptr);
        const uint32_t firstPaintUs = micros() - _startUs;

        const size_t intAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t psAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const unsigned stackAfter =
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr));
        const unsigned stackMin = static_cast<unsigned>(
            _stackBefore < stackAfter ? _stackBefore : stackAfter);

        NUMOS_SERIAL.printf(
            "[HWUX] app=%s action=%s backend=%s status=%s kind=%s "
            "result_us=%u first_paint_us=%u dint=%d dpsram=%d "
            "stack_min=%u hash=%08x\n",
            _app, _action, backend, status, kind,
            static_cast<unsigned>(resultReadyUs),
            static_cast<unsigned>(firstPaintUs),
            static_cast<int32_t>(intAfter) - static_cast<int32_t>(_intBefore),
            static_cast<int32_t>(psAfter) - static_cast<int32_t>(_psBefore),
            stackMin, static_cast<unsigned>(resultHash));
        _finished = true;
    }

private:
    const char* _app;
    const char* _action;
    uint32_t _startUs;
    size_t _intBefore;
    size_t _psBefore;
    UBaseType_t _stackBefore;
    bool _finished = false;
};

} // namespace numos

#else

namespace numos {
inline unsigned hwUxHash(const char*) { return 0u; }
class HwUxProbe {
public:
    HwUxProbe(const char*, const char*) {}
    void finish(const char*, const char*, const char*, unsigned) {}
};
} // namespace numos

#endif
