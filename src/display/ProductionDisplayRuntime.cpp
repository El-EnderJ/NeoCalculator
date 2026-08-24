#include "ProductionDisplayRuntime.h"
#include "ProductionDisplayRuntimeConfig.h"

#if defined(ARDUINO) && defined(NUMOS_BOARD_PROD_WROOM1U_N16R8) && \
    NUMOS_BOARD_PROD_WROOM1U_N16R8

#include <Preferences.h>
#include <esp_attr.h>
#include <esp_system.h>

#if NUMOS_PRODUCTION_DEMO_PROFILE
#include "../demo/DemoBootHealth.h"
#endif

namespace numos::display {

namespace {

constexpr const char* kPreferencesNamespace = "numosdisp";
constexpr const char* kProfileKey = "profile";
constexpr const char* kFailureCountKey = "failures";
constexpr uint32_t kRtcAttemptMagic = 0x44535041U; // "APSD"

struct DisplayRtcAttempt {
    uint32_t magic;
    uint32_t recordIdentity;
    uint32_t recordIdentityInverse;
};

RTC_NOINIT_ATTR DisplayRtcAttempt g_rtcAttempt;

ProductionDisplayProfile g_activeProfile = kSafeDisplayProfile;
ProfileLoadDecision g_loadDecision = ProfileLoadDecision::SafeNoRecord;
DisplayResetClass g_resetClass = DisplayResetClass::Unknown;
uint8_t g_failureCount = 0;
bool g_prepared = false;

void publishBusFrequencies() {
    numos_display_write_spi_hz = g_activeProfile.writeSpiHz;
    numos_display_read_spi_hz = g_activeProfile.readSpiHz;
}

DisplayResetClass classifyReset(const esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
        case ESP_RST_BROWNOUT:
            return DisplayResetClass::PowerInterruption;
        case ESP_RST_EXT:
        case ESP_RST_SW:
        case ESP_RST_DEEPSLEEP:
        case ESP_RST_SDIO:
            return DisplayResetClass::CleanReset;
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return DisplayResetClass::GenuineFailure;
        case ESP_RST_UNKNOWN:
            return DisplayResetClass::Unknown;
    }
    return DisplayResetClass::Unknown;
}

bool rtcAttemptMatches(const uint32_t recordIdentity) {
    return g_rtcAttempt.magic == kRtcAttemptMagic &&
           g_rtcAttempt.recordIdentity == recordIdentity &&
           g_rtcAttempt.recordIdentityInverse == ~recordIdentity;
}

void clearRtcAttempt() {
    g_rtcAttempt = {};
}

void armRtcAttempt(const uint32_t recordIdentity) {
    g_rtcAttempt = {
        kRtcAttemptMagic,
        recordIdentity,
        ~recordIdentity
    };
}

} // namespace

void prepareProductionDisplayBootProfile() {
    if (g_prepared) return;
    g_prepared = true;

#if NUMOS_PRODUCTION_DEMO_PROFILE
    if (numos::demo::safeModeActive()) {
        // General boot-health safe mode is independent from the display
        // quarantine counter. Optional UI state is ignored, while its NVS
        // evidence is left untouched for diagnostics.
        g_activeProfile = kSafeDisplayProfile;
        g_loadDecision = ProfileLoadDecision::SafeRollback;
        g_resetClass = classifyReset(esp_reset_reason());
        clearRtcAttempt();
        publishBusFrequencies();
        return;
    }
#endif

    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) {
        clearRtcAttempt();
        restoreSafeProductionDisplayProfile();
        g_loadDecision = ProfileLoadDecision::SafeNoRecord;
        return;
    }

    g_resetClass = classifyReset(esp_reset_reason());
    g_failureCount =
        preferences.getUChar(kFailureCountKey, 0);
    DisplayProfileRecord record{};
    const std::size_t recordLength =
        preferences.getBytesLength(kProfileKey);
    const bool recordPresent = recordLength != 0;
    const bool readableRecord =
        recordLength == record.bytes.size() &&
        preferences.getBytes(kProfileKey, record.bytes.data(),
                             record.bytes.size()) == record.bytes.size();

    const ProfileLoadDecision recordDecision =
        resolveDisplayProfileRecord(
            readableRecord ? &record : nullptr,
            recordPresent,
            g_activeProfile);
    const uint32_t recordIdentity = readableRecord
        ? displayRecordChecksum(record.bytes.data(), record.bytes.size())
        : 0;
    const bool matchingAttempt =
        readableRecord && rtcAttemptMatches(recordIdentity);
    clearRtcAttempt();

    const DisplayBootRecoveryPlan recovery =
        planDisplayBootRecovery(
            recordDecision,
            g_failureCount,
            g_resetClass,
            matchingAttempt);
    g_loadDecision = recovery.decision;
    g_failureCount = recovery.failureCount;
    if (recovery.persistFailureCount) {
        preferences.putUChar(kFailureCountKey, g_failureCount);
    }
    if (g_loadDecision != ProfileLoadDecision::Saved) {
        g_activeProfile = kSafeDisplayProfile;
    }
    preferences.end();
    if (recovery.armRtcAttempt) {
        // WHY: RTC slow memory survives panic/watchdog/software reset without
        // consuming NVS endurance. A power interruption loses or invalidates
        // this marker and therefore never increments the failure counter.
        armRtcAttempt(recordIdentity);
    }
    publishBusFrequencies();
}

const ProductionDisplayProfile& activeProductionDisplayProfile() {
    return g_activeProfile;
}

ProfileLoadDecision productionDisplayLoadDecision() {
    return g_loadDecision;
}

uint8_t productionDisplayFailureCount() {
    return g_failureCount;
}

DisplayResetClass productionDisplayResetClass() {
    return g_resetClass;
}

bool setActiveProductionDisplayProfile(
    const ProductionDisplayProfile& profile) {
    if (validateDisplayProfile(profile) != ProfileValidation::Ok) {
        return false;
    }
    g_activeProfile = profile;
    publishBusFrequencies();
    return true;
}

void restoreSafeProductionDisplayProfile() {
    g_activeProfile = kSafeDisplayProfile;
    publishBusFrequencies();
}

bool saveActiveProductionDisplayProfile() {
    // WHY: The opt-in BOARD A performance build may exercise the ESP32-S3
    // peripheral's 80 MHz endpoint, but GPIO-matrix routing is only guaranteed
    // through 40 MHz. Never allow an experimental rate to survive reboot.
    if (usesUnvalidatedSpiRate(g_activeProfile) ||
        validateDisplayProfile(g_activeProfile) != ProfileValidation::Ok) {
        return false;
    }
    const DisplayProfileRecord record =
        encodeDisplayProfileRecord(g_activeProfile);
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) return false;
    const bool stored =
        preferences.putBytes(kProfileKey, record.bytes.data(),
                             record.bytes.size()) == record.bytes.size();
    if (stored) {
        if (g_failureCount != 0) {
            preferences.putUChar(kFailureCountKey, 0);
        }
        clearRtcAttempt();
        g_failureCount = 0;
        g_loadDecision = ProfileLoadDecision::Saved;
    }
    preferences.end();
    return stored;
}

void markProductionDisplayBootUsable() {
    clearRtcAttempt();
    if (g_loadDecision != ProfileLoadDecision::Saved ||
        g_failureCount == 0) {
        return;
    }
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) return;
    preferences.putUChar(kFailureCountKey, 0);
    preferences.end();
    g_failureCount = 0;
}

} // namespace numos::display

#elif !defined(ARDUINO)

namespace numos::display {

void prepareProductionDisplayBootProfile() {}
const ProductionDisplayProfile& activeProductionDisplayProfile() {
    return kSafeDisplayProfile;
}
ProfileLoadDecision productionDisplayLoadDecision() {
    return ProfileLoadDecision::SafeNoRecord;
}
uint8_t productionDisplayFailureCount() { return 0; }
DisplayResetClass productionDisplayResetClass() {
    return DisplayResetClass::Unknown;
}
bool setActiveProductionDisplayProfile(
    const ProductionDisplayProfile& profile) {
    return validateDisplayProfile(profile) == ProfileValidation::Ok;
}
void restoreSafeProductionDisplayProfile() {}
bool saveActiveProductionDisplayProfile() { return false; }
void markProductionDisplayBootUsable() {}

} // namespace numos::display

#endif
