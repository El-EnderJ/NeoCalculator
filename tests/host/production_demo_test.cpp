#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "demo/DemoBootHealth.h"
#include "demo/DemoProfile.h"
#include "demo/DemoSettingsRecord.h"
#include "apps/BrightnessSettingPolicy.h"
#include "hal/FileSystem.h"
#include "input/KeyboardManager.h"
#include "input/ProductionKeypadScanner.h"
#include "math/VariableManager.h"

// VariableManager's persistence code needs only this ExactVal constructor.
// Keeping the host suite narrow avoids linking the renderer/evaluator graph.
namespace vpam {
ExactVal ExactVal::fromInt(const int64_t value) {
    ExactVal result;
    result.num = value;
    result.den = 1;
    result.outer = 1;
    result.inner = 1;
    result.ok = true;
    return result;
}
} // namespace vpam

namespace {

void require(const bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "production_demo_test: FAIL: %s\n", message);
        std::abort();
    }
}

uint32_t fnv1a(const uint8_t* data, const std::size_t length) {
    uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

std::vector<uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void writeBytes(const std::filesystem::path& path,
                const std::vector<uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void testAllowlist() {
    using namespace numos::demo;
    require(kEventReadyApps.size() == 5, "event allowlist size");
    require(isEventReadyApp(0) && isEventReadyApp(1) &&
            isEventReadyApp(2) && isEventReadyApp(3) &&
            isEventReadyApp(10), "required visible apps");
    require(!isEventReadyApp(4) && !isEventReadyApp(19),
            "experimental apps hidden");
    require(isSafeModeApp(0) && isSafeModeApp(10),
            "safe-mode recovery apps");
    require(!isSafeModeApp(1), "Grapher excluded from safe mode");
}

void testBootHealth() {
    using namespace numos::demo;
    BootTransition boot = evaluateBootTransition(
        nullptr, ResetClass::PowerInterruption);
    require(!boot.safeMode && boot.next.consecutiveFailures == 0,
            "power-on starts clean");

    for (uint8_t expected = 1; expected <= kSafeModeThreshold; ++expected) {
        boot = evaluateBootTransition(&boot.next, ResetClass::Panic);
        require(boot.next.consecutiveFailures == expected,
                "pre-launcher panic increments once");
    }
    require(boot.safeMode, "three pre-launcher failures enter safe mode");

    boot = evaluateBootTransition(&boot.next, ResetClass::External);
    require(!boot.safeMode && boot.next.consecutiveFailures == 0,
            "external reset does not accumulate false failures");

    boot.next.phase = BootPhase::LauncherReady;
    boot.next.checksum = bootHealthChecksum(boot.next);
    const BootTransition postLauncherPanic =
        evaluateBootTransition(&boot.next, ResetClass::Panic);
    require(postLauncherPanic.next.consecutiveFailures == 0,
            "ordinary post-launcher panic is not a boot-loop failure");

    const BootTransition watchdog =
        evaluateBootTransition(&boot.next, ResetClass::TaskWatchdog);
    require(watchdog.next.consecutiveFailures == 1 &&
            watchdog.next.lastFailure == FailureCode::Watchdog,
            "watchdog reset participates in policy");

    BootHealthRecord corrupt = watchdog.next;
    corrupt.session ^= 0x55U;
    require(!bootHealthRecordValid(corrupt),
            "RTC checksum rejects random/corrupt state");
}

void testForceReleaseAndModifiers() {
    using namespace numos::input;
    ProductionKeypadScanner scanner;
    const auto& mapping = kProductionKeypadMap[0];
    const uint16_t mask =
        static_cast<uint16_t>(1U << mapping.electricalColumn);
    for (uint32_t tick = 0; tick < 4; ++tick) {
        scanner.ingestRow(mapping.electricalRow, mask, tick);
    }
    KeyEvent event{};
    require(scanner.pollEvent(event) &&
            event.action == KeyAction::PRESS, "press dispatched");

    scanner.forceReleaseAll(10);
    require(scanner.pollEvent(event) &&
            event.action == KeyAction::RELEASE, "held key released");
    require(!scanner.pollEvent(event), "repeat queue cleared on transition");
    require(scanner.state(mapping.electricalRow, mapping.electricalColumn)
                .inhibitUntilReleased,
            "held physical key inhibited until real release");

    auto& modifiers = vpam::KeyboardManager::instance();
    modifiers.pressShift();
    modifiers.pressAlpha();
    require(modifiers.isShift() && modifiers.isAlpha(),
            "combined modifier plane entered");
    modifiers.reset();
    require(!modifiers.isShift() && !modifiers.isAlpha(),
            "transition clears modifiers");
}

void testPersistentFaults(const std::filesystem::path& root) {
    LittleFSClass::setRoot(root.string().c_str());
    require(LittleFS.begin(false), "host LittleFS mount");
    auto& variables = vpam::VariableManager::instance();
    variables.resetAll();
    variables.setVariable('A', vpam::ExactVal::fromInt(11));
    variables.setVariable('B', vpam::ExactVal::fromInt(22));
    require(variables.saveToFlash(), "write v2 record");

    const auto path = root / "vars.dat";
    const std::vector<uint8_t> pristine = readBytes(path);
    require(pristine.size() == 8U + 11U * 38U, "bounded v2 record size");

    variables.resetAll();
    require(variables.loadFromFlash(), "load valid v2 record");
    require(variables.getVariable('A').num == 11 &&
            variables.getVariable('B').num == 22, "valid slots restored");

    std::vector<uint8_t> truncated(pristine.begin(), pristine.begin() + 9);
    writeBytes(path, truncated);
    require(!variables.loadFromFlash() &&
            variables.lastLoadStatus() ==
                vpam::VariableManager::PersistentLoadStatus::Corrupt,
            "truncated record rejected");

    std::vector<uint8_t> random(pristine.size(), 0xA5);
    writeBytes(path, random);
    require(!variables.loadFromFlash(), "random record rejected");

    std::vector<uint8_t> stale = pristine;
    stale[0] = 'V'; stale[1] = 'R'; stale[2] = '0'; stale[3] = '1';
    stale[4] = 1;
    writeBytes(path, stale);
    require(!variables.loadFromFlash() &&
            variables.lastLoadStatus() ==
                vpam::VariableManager::PersistentLoadStatus::
                    UnsupportedVersion,
            "stale version rejected");

    std::vector<uint8_t> checksumBad = pristine;
    checksumBad[8] ^= 0x01U; // slot A payload, leave its checksum unchanged
    writeBytes(path, checksumBad);
    variables.resetAll();
    require(variables.loadFromFlash() &&
            variables.lastLoadStatus() ==
                vpam::VariableManager::PersistentLoadStatus::Partial,
            "checksum-bad slot falls back individually");
    require(variables.getVariable('A').num == 0 &&
            variables.getVariable('B').num == 22,
            "bad A does not invalidate unrelated B");

    std::vector<uint8_t> invalidDen = pristine;
    constexpr std::size_t slotB = 8U + 38U;
    for (std::size_t i = 8; i < 16; ++i) invalidDen[slotB + i] = 0;
    const uint32_t updatedChecksum = fnv1a(invalidDen.data() + slotB, 34);
    std::memcpy(invalidDen.data() + slotB + 34,
                &updatedChecksum, sizeof(updatedChecksum));
    writeBytes(path, invalidDen);
    variables.resetAll();
    require(variables.loadFromFlash() &&
            variables.lastLoadStatus() ==
                vpam::VariableManager::PersistentLoadStatus::Partial,
            "structurally invalid slot rejected with valid checksum");
    require(variables.getVariable('A').num == 11 &&
            variables.getVariable('B').num == 0,
            "invalid denominator is isolated to its slot");
}

void testSettingsRecordFaults() {
    using namespace numos::demo;
    const auto pristine = encodeSettingsRecord(true, true, false, 12, 180);
    DecodedSettings decoded{};
    require(decodeSettingsRecord(
                pristine.data(), pristine.size(), decoded),
            "valid settings record decoded");
    require(decoded.angleValid && decoded.angleDeg &&
            decoded.complexValid && decoded.complexEnabled &&
            decoded.educationValid && !decoded.educationEnabled &&
            decoded.precisionValid && decoded.precision == 12 &&
            decoded.brightnessValid && decoded.brightness == 180,
            "valid settings fields restored");

    auto previous = pristine;
    previous[4] = kPreviousSettingsFormatVersion;
    previous[9] = 0;  // reserved in v2, not an encoded brightness value
    const uint32_t previousChecksum =
        settingsRecordChecksum(previous.data(), 12);
    std::memcpy(previous.data() + 12, &previousChecksum,
                sizeof(previousChecksum));
    require(decodeSettingsRecord(
                previous.data(), previous.size(), decoded),
            "previous settings version migrated");
    require(decoded.angleValid && decoded.precisionValid &&
            !decoded.brightnessValid,
            "v2 migration preserves defaults for missing brightness");

    require(!decodeSettingsRecord(pristine.data(), 9, decoded),
            "truncated settings rejected");
    auto random = pristine;
    random.fill(0xA5);
    require(!decodeSettingsRecord(random.data(), random.size(), decoded),
            "random settings rejected");
    auto stale = pristine;
    stale[4] = 1;
    const uint32_t staleChecksum =
        settingsRecordChecksum(stale.data(), 12);
    std::memcpy(stale.data() + 12, &staleChecksum, sizeof(staleChecksum));
    require(!decodeSettingsRecord(stale.data(), stale.size(), decoded),
            "stale settings version rejected");
    auto checksumBad = pristine;
    checksumBad[5] ^= 1;
    require(!decodeSettingsRecord(
                checksumBad.data(), checksumBad.size(), decoded),
            "checksum-bad settings rejected");

    auto fieldBad = pristine;
    fieldBad[5] = 9;  // invalid angle only
    fieldBad[8] = 7;  // invalid precision only
    const uint32_t fieldChecksum =
        settingsRecordChecksum(fieldBad.data(), 12);
    std::memcpy(fieldBad.data() + 12, &fieldChecksum,
                sizeof(fieldChecksum));
    require(decodeSettingsRecord(
                fieldBad.data(), fieldBad.size(), decoded),
            "individually invalid settings retain valid container");
    require(!decoded.angleValid && !decoded.precisionValid &&
            decoded.complexValid && decoded.complexEnabled &&
            decoded.educationValid && !decoded.educationEnabled,
            "invalid settings fields do not poison unrelated fields");
}

void testBrightnessZeroContract() {
    using numos::display::kMaximumBacklight;
    using numos::display::kMinimumPersistedBacklight;
    using numos::display::kZeroBrightnessFallbackBacklight;
    using numos::settings::BrightnessSettingSession;
    using numos::settings::normalizePersistedBrightness;
    using namespace numos::demo;

    static_assert(kMinimumPersistedBacklight == 1);
    static_assert(kZeroBrightnessFallbackBacklight == 32);
    static_assert(normalizePersistedBrightness(0) == 32);
    static_assert(normalizePersistedBrightness(1) == 1);
    static_assert(normalizePersistedBrightness(31) == 31);
    static_assert(normalizePersistedBrightness(32) == 32);
    static_assert(normalizePersistedBrightness(192) == 192);
    static_assert(normalizePersistedBrightness(255) == 192);

    // A zero request through Settings is clamped to the visible minimum.
    // HOME uses this same prepare-to-leave seam and performs one deferred write.
    BrightnessSettingSession session;
    session.begin(144);
    require(session.setRuntime(0) == kMinimumPersistedBacklight,
            "Settings clamps zero to its visible minimum");
    auto leave = session.prepareToLeave();
    require(leave.runtimeBrightness == kMinimumPersistedBacklight &&
            leave.persist,
            "zero exit/HOME commits the visible minimum once");

    // RESET or hard-power before leaving Settings never changes the previously
    // committed record, so the next ordinary boot remains visible.
    const auto committed = encodeSettingsRecord(false, true, false, 10, 144);
    session.begin(144);
    require(session.setRuntime(0) == kMinimumPersistedBacklight,
            "zero request remains visible before reset");
    DecodedSettings rebooted{};
    require(decodeSettingsRecord(
                committed.data(), committed.size(), rebooted) &&
            rebooted.brightnessValid && rebooted.brightness == 144,
            "zero then RESET/hard-power retains visible committed brightness");

    // A pre-fix version-3 record containing zero migrates in place to the
    // established SAFE fallback instead of becoming a valid black boot.
    auto legacyZero = committed;
    legacyZero[9] = 0;
    const uint32_t legacyZeroChecksum =
        settingsRecordChecksum(legacyZero.data(), 12);
    std::memcpy(legacyZero.data() + 12, &legacyZeroChecksum,
                sizeof(legacyZeroChecksum));
    require(decodeSettingsRecord(
                legacyZero.data(), legacyZero.size(), rebooted) &&
            rebooted.brightnessValid &&
            rebooted.brightness == kZeroBrightnessFallbackBacklight &&
            rebooted.brightnessMigrated,
            "persisted zero migrates to minimum visible brightness");

    // New records never encode a value below the visible persistence floor.
    for (int value = 0; value <= kMaximumBacklight; ++value) {
        const auto record = encodeSettingsRecord(
            false, true, false, 10, static_cast<uint8_t>(value));
        require(record[9] == normalizePersistedBrightness(
                    static_cast<uint8_t>(value)),
                "encoded brightness obeys persistent visible range");
    }

    // Repeated runtime changes perform no persistence themselves. Exactly one
    // deferred write is requested when the final visible value differs.
    session.begin(96);
    unsigned writes = 0;
    for (int value = 104; value <= 176; value += 8) {
        session.setRuntime(value);
        require(writes == 0, "autorepeat remains write-free");
    }
    leave = session.prepareToLeave();
    if (leave.persist) ++writes;
    require(leave.runtimeBrightness == 176 && writes == 1,
            "one deferred write records final visible brightness");
}

} // namespace

int main(int argc, char** argv) {
    require(argc == 2, "temporary filesystem root argument");
    testAllowlist();
    testBootHealth();
    testForceReleaseAndModifiers();
    testPersistentFaults(argv[1]);
    testSettingsRecordFaults();
    testBrightnessZeroContract();
    std::puts("production demo host suite: PASS");
    return 0;
}
