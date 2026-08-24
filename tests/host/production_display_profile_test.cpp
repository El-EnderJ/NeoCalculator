#include "display/ProductionDisplayBootPolicy.h"
#include "display/ProductionDisplayClip.h"
#include "display/ProductionDisplayProfile.h"
#include "hardware/BoardProfile.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>

using namespace numos::display;

namespace {
std::size_t gAllocationCount = 0;
}

void* operator new(const std::size_t size) {
    ++gAllocationCount;
    if (void* const memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](const std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* const memory) noexcept {
    std::free(memory);
}

void operator delete[](void* const memory) noexcept {
    std::free(memory);
}

void operator delete(void* const memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* const memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

DisplayCommand parseOk(const char* text) {
    DisplayCommand command{};
    assert(parseDisplayCommand(text, std::strlen(text), command) ==
           CommandParseResult::Ok);
    return command;
}

struct FlushProbe {
    uint16_t* source = nullptr;
    uint32_t sourceCount = 0;
    uint32_t rowCalls = 0;
    uint32_t completionCalls = 0;
    uint32_t firstSourceIndex = UINT32_MAX;
    uint32_t lastSourceExclusive = 0;

    void write(const int32_t x, const int32_t y,
               const uint32_t width, uint16_t* const rowSource) {
        assert(x >= 0 && x < kLogicalDisplayWidth);
        assert(y >= 0 && y < kLogicalDisplayHeight);
        assert(width > 0 &&
               static_cast<uint32_t>(x) + width <= kLogicalDisplayWidth);
        const uint32_t index =
            static_cast<uint32_t>(rowSource - source);
        assert(index < sourceCount);
        assert(index + width <= sourceCount);
        if (firstSourceIndex == UINT32_MAX) firstSourceIndex = index;
        lastSourceExclusive = index + width;
        ++rowCalls;
    }

    void complete() {
        ++completionCalls;
    }
};

void executeAndVerify(const ClippedFlushPlan& plan,
                      const uint32_t expectedRows,
                      const uint32_t expectedFirstSource,
                      const uint32_t expectedLastExclusive) {
    static uint16_t pixels[kLogicalDisplayWidth * kLogicalDisplayHeight]{};
    FlushProbe probe{pixels, plan.sourcePixelCount};
    const std::size_t allocationsBefore = gAllocationCount;
    executeClippedFlush(
        plan,
        pixels,
        [&probe](const int32_t x, const int32_t y,
                 const uint32_t width, uint16_t* const source) {
            probe.write(x, y, width, source);
        },
        [&probe]() { probe.complete(); });
    assert(gAllocationCount == allocationsBefore);
    assert(probe.rowCalls == expectedRows);
    assert(probe.completionCalls == 1);
    if (expectedRows == 0) {
        assert(probe.firstSourceIndex == UINT32_MAX);
    } else {
        assert(probe.firstSourceIndex == expectedFirstSource);
        assert(probe.lastSourceExclusive == expectedLastExclusive);
    }
}

} // namespace

int main() {
    static_assert(kSafeDisplayProfile.rotation == 1);
    static_assert(displayMadctl(kSafeDisplayProfile) == 0x28);
    static_assert(kSafeDisplayProfile.writeSpiHz == 40'000'000U);
    static_assert(kSafeDisplayProfile.xOffset == 0);
    static_assert(kSafeDisplayProfile.yOffset == 0);
    static_assert(kSafeDisplayProfile.maximumBacklight == 192);
    static_assert(displayMadctl(kSafeDisplayProfile) ==
                  numos::hardware::kProductionBoard.display.provisionalMadctl);
    static_assert(logicalDisplayGeometry(1).width == 320);
    static_assert(logicalDisplayGeometry(1).height == 240);
    static_assert(logicalDisplayGeometry(3).width == 320);
    static_assert(logicalDisplayGeometry(3).height == 240);

    assert(validateDisplayProfile(kSafeDisplayProfile) ==
           ProfileValidation::Ok);
    for (const auto& preset : kProductionDisplayPresets) {
        assert(validateDisplayProfile(preset) == ProfileValidation::Ok);
        const auto geometry = logicalDisplayGeometry(preset.rotation);
        assert(geometry.width == kLogicalDisplayWidth);
        assert(geometry.height == kLogicalDisplayHeight);
    }

    // Rotation owns MX/MY/MV. BGR is the only independently variable bit.
    assert(displayMadctl(1, ColorOrder::Rgb) == 0x20);
    assert(displayMadctl(1, ColorOrder::Bgr) == 0x28);
    assert(displayMadctl(3, ColorOrder::Rgb) == 0xE0);
    assert(displayMadctl(3, ColorOrder::Bgr) == 0xE8);
    assert((displayMadctl(1, ColorOrder::Rgb) ^
            displayMadctl(1, ColorOrder::Bgr)) == kMadctlBgr);
    assert((displayMadctl(3, ColorOrder::Rgb) ^
            displayMadctl(3, ColorOrder::Bgr)) == kMadctlBgr);
    uint8_t decodedRotation = 0;
    ColorOrder decodedOrder = ColorOrder::Rgb;
    for (const auto& expected : std::array<std::array<uint8_t, 3>, 4>{{
             {{0x20, 1, 0}}, {{0x28, 1, 1}},
             {{0xE0, 3, 0}}, {{0xE8, 3, 1}}
         }}) {
        assert(decodeSupportedMadctl(
            expected[0], decodedRotation, decodedOrder));
        assert(decodedRotation == expected[1]);
        assert(decodedOrder ==
               (expected[2] ? ColorOrder::Bgr : ColorOrder::Rgb));
    }
    for (const uint8_t invalid :
         std::array<uint8_t, 8>{0x00, 0x08, 0x40, 0x48,
                                0x60, 0x68, 0xA0, 0xA8}) {
        assert(!decodeSupportedMadctl(
            invalid, decodedRotation, decodedOrder));
    }
    for (uint16_t candidate = 0; candidate <= UINT8_MAX; ++candidate) {
        const uint8_t value = static_cast<uint8_t>(candidate);
        const bool expected =
            value == 0x20 || value == 0x28 ||
            value == 0xE0 || value == 0xE8;
        const bool accepted =
            decodeSupportedMadctl(value, decodedRotation, decodedOrder);
        assert(accepted == expected);
        if (accepted) {
            assert(displayMadctl(decodedRotation, decodedOrder) == value);
        }
    }

    auto changedSafe = kSafeDisplayProfile;
    changedSafe.writeSpiHz = 20'000'000U;
    assert(validateDisplayProfile(changedSafe) ==
           ProfileValidation::PresetModified);
    assert(profilesEqual(kSafeDisplayProfile,
                         kProductionDisplayPresets[0]));

    auto custom = kSafeDisplayProfile;
    markProfileCustom(custom);
    custom.xOffset = kMinimumOffset;
    custom.yOffset = kMaximumOffset;
    custom.writeSpiHz = kMaximumSpiHz;
    custom.readSpiHz = kMinimumSpiHz;
    assert(validateDisplayProfile(custom) == ProfileValidation::Ok);
    custom.xOffset = kMinimumOffset - 1;
    assert(validateDisplayProfile(custom) ==
           ProfileValidation::OffsetOutOfRange);
    custom = kSafeDisplayProfile;
    markProfileCustom(custom);
    custom.maximumBacklight = kMaximumBacklight + 1;
    assert(validateDisplayProfile(custom) ==
           ProfileValidation::BacklightOutOfRange);
    custom = kSafeDisplayProfile;
    markProfileCustom(custom);
    custom.writeSpiHz = kMaximumSpiHz + 1;
    assert(validateDisplayProfile(custom) ==
           ProfileValidation::SpiOutOfRange);

    const DisplayProfileRecord record =
        encodeDisplayProfileRecord(kSafeDisplayProfile);
    ProductionDisplayProfile decoded{};
    assert(decodeDisplayProfileRecord(record, decoded));
    assert(profilesEqual(decoded, kSafeDisplayProfile));
    ProductionDisplayProfile resolved{};
    assert(resolveDisplayProfileRecord(&record, true, resolved) ==
           ProfileLoadDecision::Saved);
    assert(profilesEqual(resolved, kSafeDisplayProfile));
    assert(resolveDisplayProfileRecord(nullptr, false, resolved) ==
           ProfileLoadDecision::SafeNoRecord);
    assert(profilesEqual(resolved, kSafeDisplayProfile));
    assert(resolveDisplayProfileRecord(nullptr, true, resolved) ==
           ProfileLoadDecision::SafeInvalidRecord);
    assert(profilesEqual(resolved, kSafeDisplayProfile));

    auto corrupt = record;
    corrupt.bytes[20] ^= 0x55;
    assert(!decodeDisplayProfileRecord(corrupt, decoded));

    // Save/load preserves rotation and color order only; MADCTL is re-derived.
    custom = kSafeDisplayProfile;
    markProfileCustom(custom);
    custom.rotation = 3;
    custom.colorOrder = ColorOrder::Rgb;
    const DisplayProfileRecord coherentRecord =
        encodeDisplayProfileRecord(custom);
    assert(decodeDisplayProfileRecord(coherentRecord, decoded));
    assert(decoded.rotation == 3);
    assert(decoded.colorOrder == ColorOrder::Rgb);
    assert(displayMadctl(decoded) == 0xE0);

    // RTC/NVS recovery policy: power and clean resets never count. Only an
    // armed matching panic/watchdog before launcher readiness increments.
    auto recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 0,
        DisplayResetClass::PowerInterruption, true);
    assert(recovery.decision == ProfileLoadDecision::Saved);
    assert(recovery.failureCount == 0 && !recovery.persistFailureCount);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 0,
        DisplayResetClass::CleanReset, true);
    assert(recovery.decision == ProfileLoadDecision::Saved);
    assert(recovery.failureCount == 0 && !recovery.persistFailureCount);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 1,
        DisplayResetClass::PowerInterruption, true);
    assert(recovery.decision == ProfileLoadDecision::Saved);
    assert(recovery.failureCount == 1 && !recovery.persistFailureCount);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 1,
        DisplayResetClass::CleanReset, true);
    assert(recovery.decision == ProfileLoadDecision::Saved);
    assert(recovery.failureCount == 1 && !recovery.persistFailureCount);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 1,
        DisplayResetClass::Unknown, true);
    assert(recovery.decision == ProfileLoadDecision::Saved);
    assert(recovery.failureCount == 1 && !recovery.persistFailureCount);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 0,
        DisplayResetClass::GenuineFailure, false);
    assert(recovery.failureCount == 0 && recovery.armRtcAttempt);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 0,
        DisplayResetClass::GenuineFailure, true);
    assert(recovery.decision == ProfileLoadDecision::Saved);
    assert(recovery.failureCount == 1 &&
           recovery.persistFailureCount &&
           recovery.armRtcAttempt);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::Saved, 1,
        DisplayResetClass::GenuineFailure, true);
    assert(recovery.decision == ProfileLoadDecision::SafeRollback);
    assert(recovery.failureCount == kDisplayBootFailureThreshold);
    assert(recovery.persistFailureCount && !recovery.armRtcAttempt);
    recovery = planDisplayBootRecovery(
        ProfileLoadDecision::SafeInvalidRecord, 0,
        DisplayResetClass::GenuineFailure, true);
    assert(recovery.decision == ProfileLoadDecision::SafeInvalidRecord);
    assert(!recovery.armRtcAttempt && !recovery.persistFailureCount);

    // Offset/clipping plan: zero, every partial edge, complete clipping,
    // one-pixel and full-screen rectangles. Every execution completes once.
    auto plan = makeClippedFlushPlan(
        {0, 0, 319, 239}, 0, 0, 320, 240);
    assert(plan.visible && plan.destinationX == 0 && plan.destinationY == 0);
    assert(plan.width == 320 && plan.height == 240);
    assert(plan.sourceOffsetPixels == 0 && plan.sourceStride == 320);
    assert(clippedFlushPlanReadsWithinSource(plan));
    executeAndVerify(plan, 240, 0, 320U * 240U);

    plan = makeClippedFlushPlan({0, 10, 19, 19}, -10, 0, 320, 240);
    assert(plan.visible && plan.destinationX == 0 && plan.width == 10);
    assert(plan.sourceOffsetPixels == 10);
    assert(clippedFlushPlanReadsWithinSource(plan));
    executeAndVerify(plan, 10, 10, 200);

    plan = makeClippedFlushPlan({300, 10, 319, 19}, 10, 0, 320, 240);
    assert(plan.visible && plan.destinationX == 310 && plan.width == 10);
    assert(plan.sourceOffsetPixels == 0);
    assert(clippedFlushPlanReadsWithinSource(plan));
    executeAndVerify(plan, 10, 0, 190);

    plan = makeClippedFlushPlan({10, 0, 19, 19}, 0, -5, 320, 240);
    assert(plan.visible && plan.destinationY == 0 && plan.height == 15);
    assert(plan.sourceOffsetPixels == 50);
    assert(clippedFlushPlanReadsWithinSource(plan));
    executeAndVerify(plan, 15, 50, 200);

    plan = makeClippedFlushPlan({10, 225, 19, 239}, 0, 10, 320, 240);
    assert(plan.visible && plan.destinationY == 235 && plan.height == 5);
    assert(plan.sourceOffsetPixels == 0);
    assert(clippedFlushPlanReadsWithinSource(plan));
    executeAndVerify(plan, 5, 0, 50);

    for (const auto& areaAndOffset :
         std::array<std::array<int32_t, 6>, 4>{{
             {{0, 0, 9, 9, -20, 0}},
             {{310, 0, 319, 9, 20, 0}},
             {{0, 0, 9, 9, 0, -20}},
             {{0, 230, 9, 239, 0, 20}}
         }}) {
        plan = makeClippedFlushPlan(
            {areaAndOffset[0], areaAndOffset[1],
             areaAndOffset[2], areaAndOffset[3]},
            static_cast<int16_t>(areaAndOffset[4]),
            static_cast<int16_t>(areaAndOffset[5]),
            320, 240);
        assert(!plan.visible);
        assert(clippedFlushPlanReadsWithinSource(plan));
        executeAndVerify(plan, 0, 0, 0);
    }

    plan = makeClippedFlushPlan({319, 239, 319, 239},
                                0, 0, 320, 240);
    assert(plan.visible && plan.width == 1 && plan.height == 1);
    executeAndVerify(plan, 1, 0, 1);
    assert(resolveDisplayProfileRecord(&corrupt, true, resolved) ==
           ProfileLoadDecision::SafeInvalidRecord);
    assert(profilesEqual(resolved, kSafeDisplayProfile));
    corrupt = record;
    corrupt.bytes[4] = 1;
    const uint32_t crc =
        displayRecordChecksum(corrupt.bytes.data(), 44);
    corrupt.bytes[44] = static_cast<uint8_t>(crc);
    corrupt.bytes[45] = static_cast<uint8_t>(crc >> 8U);
    corrupt.bytes[46] = static_cast<uint8_t>(crc >> 16U);
    corrupt.bytes[47] = static_cast<uint8_t>(crc >> 24U);
    assert(!decodeDisplayProfileRecord(corrupt, decoded));
    corrupt = record;
    corrupt.bytes[8] ^= 0x01; // Unknown schema with an otherwise valid CRC.
    const uint32_t schemaCrc =
        displayRecordChecksum(corrupt.bytes.data(), 44);
    corrupt.bytes[44] = static_cast<uint8_t>(schemaCrc);
    corrupt.bytes[45] = static_cast<uint8_t>(schemaCrc >> 8U);
    corrupt.bytes[46] = static_cast<uint8_t>(schemaCrc >> 16U);
    corrupt.bytes[47] = static_cast<uint8_t>(schemaCrc >> 24U);
    assert(!decodeDisplayProfileRecord(corrupt, decoded));

    assert(parseOk("DISPLAY HELP").kind == DisplayCommandKind::Help);
    assert(parseOk("DISPLAY INFO").kind == DisplayCommandKind::Info);
    assert(parseOk("DISPLAY TEST").kind == DisplayCommandKind::Test);
    assert(parseOk("display profile list").kind ==
           DisplayCommandKind::ProfileList);
    auto command = parseOk("DISPLAY PROFILE SET ROT3-BGR");
    assert(command.kind == DisplayCommandKind::ProfileSet);
    assert(command.profile == ProfileId::Rotate3Bgr);
    command = parseOk("DISPLAY ROTATE 3");
    assert(command.kind == DisplayCommandKind::Rotate &&
           command.first == 3);
    command = parseOk("DISPLAY BGR OFF");
    assert(command.kind == DisplayCommandKind::Bgr &&
           !command.enabled);
    command = parseOk("DISPLAY INVERT ON");
    assert(command.kind == DisplayCommandKind::Invert &&
           command.enabled);
    command = parseOk("DISPLAY OFFSET -32 32");
    assert(command.first == -32 && command.second == 32);
    command = parseOk("DISPLAY SPI 40");
    assert(command.first == 40);
#if defined(NUMOS_PRODUCTION_BRINGUP_SPI_EXPERIMENT_MAX_HZ)
    command = parseOk("DISPLAY SPI 80");
    assert(command.first == 80);
#endif
    command = parseOk("DISPLAY BACKLIGHT 192");
    assert(command.first == 192);
    assert(parseOk("DISPLAY SAVE").kind == DisplayCommandKind::Save);
    assert(parseOk("DISPLAY RESET").kind == DisplayCommandKind::Reset);
    assert(parseOk("DISPLAY SAFE").kind == DisplayCommandKind::Safe);
    command = parseOk("DISPLAY SPI 1");
    assert(command.first == 1);
    command = parseOk("DISPLAY BACKLIGHT 0");
    assert(command.first == 0);

    DisplayCommand rejected{};
    assert(parseDisplayCommand("DISPLAY ROTATE 0", 16, rejected) ==
           CommandParseResult::UnsupportedValue);
    assert(parseDisplayCommand("DISPLAY OFFSET -33 0", 20, rejected) ==
           CommandParseResult::UnsupportedValue);
#if defined(NUMOS_PRODUCTION_BRINGUP_SPI_EXPERIMENT_MAX_HZ)
    assert(parseDisplayCommand("DISPLAY SPI 81", 14, rejected) ==
           CommandParseResult::UnsupportedValue);
#else
    assert(parseDisplayCommand("DISPLAY SPI 41", 14, rejected) ==
           CommandParseResult::UnsupportedValue);
#endif
    assert(parseDisplayCommand("DISPLAY BACKLIGHT 193", 21, rejected) ==
           CommandParseResult::UnsupportedValue);
    assert(parseDisplayCommand("KEYPAD HELP", 11, rejected) ==
           CommandParseResult::NotDisplayCommand);

    char tooLong[kMaximumDisplayCommandLength + 2]{};
    std::memset(tooLong, 'A', sizeof(tooLong));
    assert(parseDisplayCommand(tooLong, sizeof(tooLong), rejected) ==
           CommandParseResult::TooLong);

    // Regression guard: display work must not mutate the established CAM
    // identity/pins or alias the production PCBA contract.
    using numos::hardware::kExistingCamIdentity;
    using numos::hardware::kProductionBoard;
    assert(std::strcmp(kExistingCamIdentity.boardId,
                       "numos-esp32-s3-n16r8-cam") == 0);
    const std::array<int8_t, 6> expectedCamPins = {13, 12, 10, 4, 5, 45};
    assert(kExistingCamIdentity.displayPins == expectedCamPins);
    assert(kProductionBoard.display.chipSelect.gpio !=
           kExistingCamIdentity.displayPins[2]);

    std::cout << "production_display_profile_test: PASS\n";
    return 0;
}
