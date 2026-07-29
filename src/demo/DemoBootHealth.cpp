#include "DemoBootHealth.h"

#include <cstddef>
#include <cstring>

#if NUMOS_PRODUCTION_DEMO_PROFILE && defined(ARDUINO)
#include <esp_attr.h>
#include <esp_system.h>
#endif

namespace numos::demo {

namespace {

uint32_t fnv1a(const uint8_t* data, const std::size_t size) {
    uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

bool isWatchdog(const ResetClass reset) {
    return reset == ResetClass::InterruptWatchdog ||
           reset == ResetClass::TaskWatchdog ||
           reset == ResetClass::OtherWatchdog;
}

bool isPowerOrOperatorReset(const ResetClass reset) {
    return reset == ResetClass::PowerInterruption ||
           reset == ResetClass::External ||
           reset == ResetClass::Software ||
           reset == ResetClass::DeepSleep;
}

void seal(BootHealthRecord& record) {
    record.magic = kBootHealthMagic;
    record.magicInverse = ~kBootHealthMagic;
    record.checksum = 0;
    record.checksum = bootHealthChecksum(record);
}

} // namespace

uint32_t bootHealthChecksum(const BootHealthRecord& record) {
    BootHealthRecord copy = record;
    copy.checksum = 0;
    return fnv1a(reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
}

bool bootHealthRecordValid(const BootHealthRecord& record) {
    return record.magic == kBootHealthMagic &&
           record.magicInverse == ~kBootHealthMagic &&
           record.checksum == bootHealthChecksum(record);
}

BootTransition evaluateBootTransition(const BootHealthRecord* previous,
                                      const ResetClass currentReset) {
    BootTransition transition{};
    const bool valid = previous && bootHealthRecordValid(*previous);
    if (valid) {
        transition.next = *previous;
    }

    if (!valid || isPowerOrOperatorReset(currentReset)) {
        transition.next.consecutiveFailures = 0;
    } else if (isWatchdog(currentReset)) {
        if (transition.next.consecutiveFailures < UINT8_MAX) {
            ++transition.next.consecutiveFailures;
        }
        transition.next.lastFailure = FailureCode::Watchdog;
        transition.countedFailure = true;
    } else if (currentReset == ResetClass::Panic &&
               transition.next.phase != BootPhase::LauncherReady) {
        if (transition.next.consecutiveFailures < UINT8_MAX) {
            ++transition.next.consecutiveFailures;
        }
        transition.next.lastFailure = FailureCode::PreLauncherCrash;
        transition.countedFailure = true;
    }

    ++transition.next.session;
    transition.next.lastReset = currentReset;
    transition.next.phase = BootPhase::Starting;
    transition.safeMode =
        transition.next.userSafeMode ||
        transition.next.consecutiveFailures >= kSafeModeThreshold;
    seal(transition.next);
    return transition;
}

const char* resetClassName(const ResetClass reset) {
    switch (reset) {
        case ResetClass::PowerInterruption: return "power";
        case ResetClass::External: return "external";
        case ResetClass::Software: return "software";
        case ResetClass::DeepSleep: return "deep-sleep";
        case ResetClass::Panic: return "panic";
        case ResetClass::InterruptWatchdog: return "int-wdt";
        case ResetClass::TaskWatchdog: return "task-wdt";
        case ResetClass::OtherWatchdog: return "wdt";
        case ResetClass::Unknown: return "unknown";
    }
    return "unknown";
}

const char* failureCodeName(const FailureCode failure) {
    switch (failure) {
        case FailureCode::None: return "none";
        case FailureCode::PreLauncherCrash: return "pre-launcher-crash";
        case FailureCode::Watchdog: return "watchdog";
        case FailureCode::AppInitialization: return "app-init";
        case FailureCode::FilesystemMount: return "filesystem-mount";
        case FailureCode::PersistentState: return "persistent-state";
    }
    return "unknown";
}

#if NUMOS_PRODUCTION_DEMO_PROFILE

namespace {

#if defined(ARDUINO)
RTC_NOINIT_ATTR BootHealthRecord g_rtcBootHealth;
RTC_NOINIT_ATTR uint8_t g_rtcLastAppMode;
#else
BootHealthRecord g_rtcBootHealth;
uint8_t g_rtcLastAppMode = 0;
#endif

bool g_safeMode = false;

ResetClass platformResetClass() {
#if defined(ARDUINO)
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:
        case ESP_RST_BROWNOUT: return ResetClass::PowerInterruption;
        case ESP_RST_EXT: return ResetClass::External;
        case ESP_RST_SW:
        case ESP_RST_SDIO: return ResetClass::Software;
        case ESP_RST_DEEPSLEEP: return ResetClass::DeepSleep;
        case ESP_RST_PANIC: return ResetClass::Panic;
        case ESP_RST_INT_WDT: return ResetClass::InterruptWatchdog;
        case ESP_RST_TASK_WDT: return ResetClass::TaskWatchdog;
        case ESP_RST_WDT: return ResetClass::OtherWatchdog;
        case ESP_RST_UNKNOWN: return ResetClass::Unknown;
    }
#endif
    return ResetClass::Unknown;
}

void resealRuntimeRecord() {
    seal(g_rtcBootHealth);
}

} // namespace

void initializeBootHealth() {
    const bool previousRecordValid = bootHealthRecordValid(g_rtcBootHealth);
    const BootTransition transition =
        evaluateBootTransition(&g_rtcBootHealth, platformResetClass());
    g_rtcBootHealth = transition.next;
    g_safeMode = transition.safeMode;
    if (!previousRecordValid) {
        g_rtcLastAppMode = 0;
    }
}

bool safeModeActive() {
    return g_safeMode;
}

void acknowledgeLauncherReady() {
    g_rtcBootHealth.phase = BootPhase::LauncherReady;
    if (!g_safeMode) {
        g_rtcBootHealth.consecutiveFailures = 0;
    }
    resealRuntimeRecord();
}

void recordAppMode(const uint8_t mode) {
    g_rtcLastAppMode = mode;
}

void recordFailure(const FailureCode failure) {
    g_rtcBootHealth.lastFailure = failure;
    resealRuntimeRecord();
}

void requestSafeMode() {
    g_rtcBootHealth.userSafeMode = true;
    g_safeMode = true;
    resealRuntimeRecord();
}

void clearSafeMode() {
    g_rtcBootHealth.userSafeMode = false;
    g_rtcBootHealth.consecutiveFailures = 0;
    g_rtcBootHealth.lastFailure = FailureCode::None;
    g_safeMode = false;
    resealRuntimeRecord();
}

const BootHealthRecord& bootHealthRecord() {
    return g_rtcBootHealth;
}

uint8_t lastAppMode() {
    return g_rtcLastAppMode;
}

#endif

} // namespace numos::demo
