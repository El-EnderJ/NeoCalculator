#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace numos::display {

inline constexpr uint32_t kMinimumSpiHz = 1'000'000U;
inline constexpr uint32_t kMaximumSpiHz = 40'000'000U;
inline constexpr int16_t kMinimumOffset = -32;
inline constexpr int16_t kMaximumOffset = 32;
inline constexpr uint8_t kMaximumBacklight = 192;
inline constexpr uint8_t kMadctlBgr = 0x08;
inline constexpr uint8_t kMadctlRotation1 = 0x20;
inline constexpr uint8_t kMadctlRotation3 = 0xE0;
inline constexpr uint16_t kLogicalDisplayWidth = 320;
inline constexpr uint16_t kLogicalDisplayHeight = 240;

enum class ProfileId : uint8_t {
    Safe = 0,
    Rotate3Bgr = 1,
    Rotate1Rgb = 2,
    Rotate1BgrInverted = 3,
    Custom = 0x7F
};

enum class ColorOrder : uint8_t {
    Rgb = 0,
    Bgr = 1
};

struct ProductionDisplayProfile {
    ProfileId identifier;
    uint8_t rotation;
    ColorOrder colorOrder;
    bool inverted;
    int16_t xOffset;
    int16_t yOffset;
    uint32_t writeSpiHz;
    uint32_t readSpiHz;
    uint16_t resetLowMs;
    uint16_t resetRecoveryMs;
    uint8_t initialBacklight;
    uint8_t maximumBacklight;
};

inline constexpr ProductionDisplayProfile kSafeDisplayProfile = {
    ProfileId::Safe,
    1,
    ColorOrder::Bgr,
    false,
    0,
    0,
    10'000'000U,
    10'000'000U,
    10,
    120,
    96,
    192
};

inline constexpr std::array<ProductionDisplayProfile, 4>
    kProductionDisplayPresets = {{
        kSafeDisplayProfile,
        {
            ProfileId::Rotate3Bgr, 3, ColorOrder::Bgr, false,
            0, 0, 10'000'000U, 10'000'000U, 10, 120, 96, 192
        },
        {
            ProfileId::Rotate1Rgb, 1, ColorOrder::Rgb, false,
            0, 0, 10'000'000U, 10'000'000U, 10, 120, 96, 192
        },
        {
            ProfileId::Rotate1BgrInverted, 1, ColorOrder::Bgr, true,
            0, 0, 10'000'000U, 10'000'000U, 10, 120, 96, 192
        }
    }};

struct DisplayGeometry {
    uint16_t width;
    uint16_t height;
};

constexpr DisplayGeometry logicalDisplayGeometry(const uint8_t rotation) {
    return (rotation == 1 || rotation == 3)
        ? DisplayGeometry{kLogicalDisplayWidth, kLogicalDisplayHeight}
        : DisplayGeometry{0, 0};
}

constexpr uint8_t displayMadctl(const uint8_t rotation,
                                const ColorOrder colorOrder) {
    const uint8_t axisBits =
        rotation == 1 ? kMadctlRotation1 :
        rotation == 3 ? kMadctlRotation3 : 0;
    return static_cast<uint8_t>(
        axisBits | (colorOrder == ColorOrder::Bgr ? kMadctlBgr : 0));
}

constexpr uint8_t displayMadctl(const ProductionDisplayProfile& profile) {
    return displayMadctl(profile.rotation, profile.colorOrder);
}

bool decodeSupportedMadctl(uint8_t madctl, uint8_t& rotation,
                           ColorOrder& colorOrder);

enum class ProfileValidation : uint8_t {
    Ok,
    UnknownIdentifier,
    UnsupportedRotation,
    InvalidColorOrder,
    OffsetOutOfRange,
    SpiOutOfRange,
    SpiNotWholeMHz,
    ResetTimingOutOfRange,
    BacklightOutOfRange,
    PresetModified
};

const char* profileIdentifier(ProfileId identifier);
const ProductionDisplayProfile* findPreset(ProfileId identifier);
bool parseProfileIdentifier(const char* text, std::size_t length,
                            ProfileId& identifier);
ProfileValidation validateDisplayProfile(
    const ProductionDisplayProfile& profile);
const char* profileValidationName(ProfileValidation validation);
bool profilesEqual(const ProductionDisplayProfile& left,
                   const ProductionDisplayProfile& right);
void markProfileCustom(ProductionDisplayProfile& profile);

inline constexpr uint32_t kDisplayRecordMagic = 0x3250444EU; // "NDP2" LE
inline constexpr uint16_t kDisplayRecordVersion = 2;
inline constexpr uint32_t kDisplayProfileSchemaTag = 0x5A320002U;
inline constexpr std::size_t kDisplayRecordSize = 48;

struct DisplayProfileRecord {
    std::array<uint8_t, kDisplayRecordSize> bytes{};
};

uint32_t displayRecordChecksum(const uint8_t* data, std::size_t length);
DisplayProfileRecord encodeDisplayProfileRecord(
    const ProductionDisplayProfile& profile);
bool decodeDisplayProfileRecord(const DisplayProfileRecord& record,
                                ProductionDisplayProfile& profile);

enum class ProfileLoadDecision : uint8_t {
    Saved,
    SafeNoRecord,
    SafeInvalidRecord,
    SafeRollback
};

ProfileLoadDecision resolveDisplayProfileRecord(
    const DisplayProfileRecord* record,
    bool recordPresent,
    ProductionDisplayProfile& profile);
const char* profileLoadDecisionName(ProfileLoadDecision decision);

enum class DisplayCommandKind : uint8_t {
    Help,
    Info,
    Test,
    ProfileList,
    ProfileSet,
    Rotate,
    Bgr,
    Invert,
    Offset,
    Spi,
    Backlight,
    Save,
    Reset,
    Safe
};

struct DisplayCommand {
    DisplayCommandKind kind = DisplayCommandKind::Help;
    ProfileId profile = ProfileId::Safe;
    int32_t first = 0;
    int32_t second = 0;
    bool enabled = false;
};

enum class CommandParseResult : uint8_t {
    Ok,
    NotDisplayCommand,
    TooLong,
    TooManyTokens,
    MissingArgument,
    UnexpectedArgument,
    UnknownCommand,
    UnsupportedValue,
    InvalidInteger
};

inline constexpr std::size_t kMaximumDisplayCommandLength = 79;

CommandParseResult parseDisplayCommand(const char* text, std::size_t length,
                                       DisplayCommand& command);
const char* commandParseResultName(CommandParseResult result);

} // namespace numos::display
