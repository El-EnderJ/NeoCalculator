#pragma once

#include "ProductionDisplayProfile.h"

#include <cstdint>

namespace numos::display {

inline constexpr uint8_t kDisplayBootFailureThreshold = 2;

enum class DisplayResetClass : uint8_t {
    PowerInterruption,
    CleanReset,
    GenuineFailure,
    Unknown
};

constexpr const char* displayResetClassName(
    const DisplayResetClass resetClass) {
    switch (resetClass) {
        case DisplayResetClass::PowerInterruption:
            return "power-interruption";
        case DisplayResetClass::CleanReset:
            return "clean-reset";
        case DisplayResetClass::GenuineFailure:
            return "panic-or-watchdog";
        case DisplayResetClass::Unknown:
            return "unknown";
    }
    return "unknown";
}

struct DisplayBootRecoveryPlan {
    ProfileLoadDecision decision;
    uint8_t failureCount;
    bool persistFailureCount;
    bool armRtcAttempt;
};

constexpr DisplayBootRecoveryPlan planDisplayBootRecovery(
    const ProfileLoadDecision recordDecision,
    const uint8_t storedFailureCount,
    const DisplayResetClass resetClass,
    const bool matchingRtcAttemptArmed) {
    if (recordDecision != ProfileLoadDecision::Saved) {
        return {recordDecision, storedFailureCount, false, false};
    }

    uint8_t failures = storedFailureCount;
    bool persist = false;
    if (resetClass == DisplayResetClass::GenuineFailure &&
        matchingRtcAttemptArmed &&
        failures < kDisplayBootFailureThreshold) {
        ++failures;
        persist = true;
    }

    if (failures >= kDisplayBootFailureThreshold) {
        return {
            ProfileLoadDecision::SafeRollback,
            failures,
            persist,
            false
        };
    }
    return {
        ProfileLoadDecision::Saved,
        failures,
        persist,
        true
    };
}

} // namespace numos::display
