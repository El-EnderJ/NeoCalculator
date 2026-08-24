#pragma once

#include <cstdint>

#include "../display/ProductionDisplayProfile.h"

namespace numos::settings {

constexpr uint8_t normalizePersistedBrightness(const uint8_t value) {
    if (value == 0) {
        return numos::display::kZeroBrightnessFallbackBacklight;
    }
    return value > numos::display::kMaximumBacklight
        ? numos::display::kMaximumBacklight : value;
}

constexpr uint8_t clampRuntimeBrightness(const int value) {
    return value <= numos::display::kMinimumPersistedBacklight
             ? numos::display::kMinimumPersistedBacklight
         : value >= numos::display::kMaximumBacklight
             ? numos::display::kMaximumBacklight
             : static_cast<uint8_t>(value);
}

struct BrightnessExitDecision {
    uint8_t runtimeBrightness;
    bool persist;
};

// WHY: Settings never permits a black-screen value. This fixed-size state
// machine also makes HOME/RESET/power-loss and deferred writes independently
// testable without involving LVGL or NVS.
class BrightnessSettingSession {
public:
    void begin(const uint8_t visibleBrightness) {
        _entryBrightness = normalizePersistedBrightness(visibleBrightness);
        _runtimeBrightness = _entryBrightness;
        _active = true;
    }

    uint8_t setRuntime(const int value) {
        _runtimeBrightness = clampRuntimeBrightness(value);
        return _runtimeBrightness;
    }

    uint8_t runtimeBrightness() const { return _runtimeBrightness; }
    bool active() const { return _active; }

    BrightnessExitDecision prepareToLeave() {
        if (!_active) return {_runtimeBrightness, false};

        const bool persist = _runtimeBrightness != _entryBrightness;
        _active = false;
        return {_runtimeBrightness, persist};
    }

private:
    uint8_t _entryBrightness =
        numos::display::kSafeDisplayProfile.initialBacklight;
    uint8_t _runtimeBrightness =
        numos::display::kSafeDisplayProfile.initialBacklight;
    bool _active = false;
};

} // namespace numos::settings
