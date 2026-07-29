#pragma once

#include <cstdint>

#ifndef NUMOS_PRODUCTION_DEMO_PROFILE
#define NUMOS_PRODUCTION_DEMO_PROFILE 0
#endif

namespace numos::demo {

inline constexpr uint32_t kBootHealthMagic = 0x31484D4EU; // "NMH1"
inline constexpr uint8_t kSafeModeThreshold = 3;

enum class ResetClass : uint8_t {
    PowerInterruption,
    External,
    Software,
    DeepSleep,
    Panic,
    InterruptWatchdog,
    TaskWatchdog,
    OtherWatchdog,
    Unknown,
};

enum class BootPhase : uint8_t {
    Starting,
    LauncherReady,
};

enum class FailureCode : uint8_t {
    None,
    PreLauncherCrash,
    Watchdog,
    AppInitialization,
    FilesystemMount,
    PersistentState,
};

struct BootHealthRecord {
    uint32_t magic = kBootHealthMagic;
    uint32_t magicInverse = ~kBootHealthMagic;
    uint32_t session = 0;
    uint8_t consecutiveFailures = 0;
    ResetClass lastReset = ResetClass::Unknown;
    BootPhase phase = BootPhase::Starting;
    FailureCode lastFailure = FailureCode::None;
    bool userSafeMode = false;
    uint32_t checksum = 0;
};

struct BootTransition {
    BootHealthRecord next{};
    bool safeMode = false;
    bool countedFailure = false;
};

uint32_t bootHealthChecksum(const BootHealthRecord& record);
bool bootHealthRecordValid(const BootHealthRecord& record);
BootTransition evaluateBootTransition(const BootHealthRecord* previous,
                                      ResetClass currentReset);
const char* resetClassName(ResetClass reset);
const char* failureCodeName(FailureCode failure);

#if NUMOS_PRODUCTION_DEMO_PROFILE
void initializeBootHealth();
bool safeModeActive();
void acknowledgeLauncherReady();
void recordAppMode(uint8_t mode);
void recordFailure(FailureCode failure);
void requestSafeMode();
void clearSafeMode();
const BootHealthRecord& bootHealthRecord();
uint8_t lastAppMode();
#else
inline void initializeBootHealth() {}
inline bool safeModeActive() { return false; }
inline void acknowledgeLauncherReady() {}
inline void recordAppMode(uint8_t) {}
inline void recordFailure(FailureCode) {}
inline void requestSafeMode() {}
inline void clearSafeMode() {}
inline uint8_t lastAppMode() { return 0; }
#endif

} // namespace numos::demo
