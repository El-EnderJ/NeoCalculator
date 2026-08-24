#include "ProductionDisplayProfile.h"
#include "ProductionDisplayRuntimeConfig.h"

#include <cctype>
#include <cstring>
#include <limits>

#if !defined(ARDUINO) || \
    (defined(NUMOS_BOARD_PROD_WROOM1U_N16R8) && \
     NUMOS_BOARD_PROD_WROOM1U_N16R8)

uint32_t numos_display_write_spi_hz =
    numos::display::kSafeDisplayProfile.writeSpiHz;
uint32_t numos_display_read_spi_hz =
    numos::display::kSafeDisplayProfile.readSpiHz;

namespace numos::display {

namespace {

constexpr uint8_t kRecordReserved = 0;

bool equalsIgnoreCase(const char* left, const std::size_t leftLength,
                      const char* right) {
    const std::size_t rightLength = std::strlen(right);
    if (leftLength != rightLength) return false;
    for (std::size_t index = 0; index < leftLength; ++index) {
        const unsigned char a = static_cast<unsigned char>(left[index]);
        const unsigned char b = static_cast<unsigned char>(right[index]);
        if (std::toupper(a) != std::toupper(b)) return false;
    }
    return true;
}

uint16_t readU16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8U);
}

uint32_t readU32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8U) |
           (static_cast<uint32_t>(bytes[2]) << 16U) |
           (static_cast<uint32_t>(bytes[3]) << 24U);
}

void writeU16(uint8_t* bytes, const uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8U);
}

void writeU32(uint8_t* bytes, const uint32_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8U);
    bytes[2] = static_cast<uint8_t>(value >> 16U);
    bytes[3] = static_cast<uint8_t>(value >> 24U);
}

bool parseInteger(const char* text, const std::size_t length, int32_t& value) {
    if (length == 0) return false;
    std::size_t cursor = 0;
    bool negative = false;
    if (text[cursor] == '-' || text[cursor] == '+') {
        negative = text[cursor] == '-';
        if (++cursor == length) return false;
    }
    int64_t parsed = 0;
    for (; cursor < length; ++cursor) {
        if (text[cursor] < '0' || text[cursor] > '9') return false;
        parsed = parsed * 10 + (text[cursor] - '0');
        const int64_t limit = negative
            ? -(static_cast<int64_t>(std::numeric_limits<int32_t>::min()))
            : std::numeric_limits<int32_t>::max();
        if (parsed > limit) return false;
    }
    value = negative ? static_cast<int32_t>(-parsed)
                     : static_cast<int32_t>(parsed);
    return true;
}

struct Token {
    const char* data = nullptr;
    std::size_t length = 0;
};

CommandParseResult tokenize(const char* text, std::size_t length,
                            std::array<Token, 5>& tokens,
                            std::size_t& tokenCount) {
    if (text == nullptr) return CommandParseResult::NotDisplayCommand;
    if (length > kMaximumDisplayCommandLength) {
        return CommandParseResult::TooLong;
    }
    while (length > 0 &&
           (text[length - 1] == '\r' || text[length - 1] == '\n' ||
            text[length - 1] == ' ' || text[length - 1] == '\t')) {
        --length;
    }
    std::size_t cursor = 0;
    tokenCount = 0;
    while (cursor < length) {
        while (cursor < length &&
               (text[cursor] == ' ' || text[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor == length) break;
        if (tokenCount == tokens.size()) {
            return CommandParseResult::TooManyTokens;
        }
        const std::size_t start = cursor;
        while (cursor < length &&
               text[cursor] != ' ' && text[cursor] != '\t') {
            ++cursor;
        }
        tokens[tokenCount++] = {text + start, cursor - start};
    }
    if (tokenCount == 0 ||
        !equalsIgnoreCase(tokens[0].data, tokens[0].length, "DISPLAY")) {
        return CommandParseResult::NotDisplayCommand;
    }
    return CommandParseResult::Ok;
}

bool tokenEquals(const Token& token, const char* value) {
    return equalsIgnoreCase(token.data, token.length, value);
}

CommandParseResult requireCount(const std::size_t actual,
                                const std::size_t expected) {
    if (actual < expected) return CommandParseResult::MissingArgument;
    if (actual > expected) return CommandParseResult::UnexpectedArgument;
    return CommandParseResult::Ok;
}

} // namespace

const char* profileIdentifier(const ProfileId identifier) {
    switch (identifier) {
        case ProfileId::Safe: return "SAFE";
        case ProfileId::Rotate3Bgr: return "ROT3-BGR";
        case ProfileId::Rotate1Rgb: return "ROT1-RGB";
        case ProfileId::Rotate1BgrInverted: return "ROT1-BGR-INV";
        case ProfileId::Custom: return "CUSTOM";
    }
    return "UNKNOWN";
}

const ProductionDisplayProfile* findPreset(const ProfileId identifier) {
    for (const auto& preset : kProductionDisplayPresets) {
        if (preset.identifier == identifier) return &preset;
    }
    return nullptr;
}

bool parseProfileIdentifier(const char* text, const std::size_t length,
                            ProfileId& identifier) {
    for (const auto& preset : kProductionDisplayPresets) {
        if (equalsIgnoreCase(text, length,
                             profileIdentifier(preset.identifier))) {
            identifier = preset.identifier;
            return true;
        }
    }
    return false;
}

bool profilesEqual(const ProductionDisplayProfile& left,
                   const ProductionDisplayProfile& right) {
    return left.identifier == right.identifier &&
           left.rotation == right.rotation &&
           left.colorOrder == right.colorOrder &&
           left.inverted == right.inverted &&
           left.xOffset == right.xOffset &&
           left.yOffset == right.yOffset &&
           left.writeSpiHz == right.writeSpiHz &&
           left.readSpiHz == right.readSpiHz &&
           left.resetLowMs == right.resetLowMs &&
           left.resetRecoveryMs == right.resetRecoveryMs &&
           left.initialBacklight == right.initialBacklight &&
           left.maximumBacklight == right.maximumBacklight;
}

ProfileValidation validateDisplayProfile(
    const ProductionDisplayProfile& profile) {
    if (profile.identifier != ProfileId::Safe &&
        profile.identifier != ProfileId::Rotate3Bgr &&
        profile.identifier != ProfileId::Rotate1Rgb &&
        profile.identifier != ProfileId::Rotate1BgrInverted &&
        profile.identifier != ProfileId::Custom) {
        return ProfileValidation::UnknownIdentifier;
    }
    if (profile.rotation != 1 && profile.rotation != 3) {
        return ProfileValidation::UnsupportedRotation;
    }
    if (profile.colorOrder != ColorOrder::Rgb &&
        profile.colorOrder != ColorOrder::Bgr) {
        return ProfileValidation::InvalidColorOrder;
    }
    if (profile.xOffset < kMinimumOffset ||
        profile.xOffset > kMaximumOffset ||
        profile.yOffset < kMinimumOffset ||
        profile.yOffset > kMaximumOffset) {
        return ProfileValidation::OffsetOutOfRange;
    }
    if (profile.writeSpiHz < kMinimumSpiHz ||
        profile.writeSpiHz > kMaximumSpiHz ||
        profile.readSpiHz < kMinimumSpiHz ||
        profile.readSpiHz > kMaximumSpiHz) {
        return ProfileValidation::SpiOutOfRange;
    }
    if ((profile.writeSpiHz % 1'000'000U) != 0 ||
        (profile.readSpiHz % 1'000'000U) != 0) {
        return ProfileValidation::SpiNotWholeMHz;
    }
    if (profile.resetLowMs < 1 || profile.resetLowMs > 200 ||
        profile.resetRecoveryMs < 5 || profile.resetRecoveryMs > 500) {
        return ProfileValidation::ResetTimingOutOfRange;
    }
    if (profile.maximumBacklight > kMaximumBacklight ||
        profile.initialBacklight > profile.maximumBacklight) {
        return ProfileValidation::BacklightOutOfRange;
    }
    const ProductionDisplayProfile* preset = findPreset(profile.identifier);
    if (preset != nullptr && !profilesEqual(profile, *preset)) {
        return ProfileValidation::PresetModified;
    }
    return ProfileValidation::Ok;
}

const char* profileValidationName(const ProfileValidation validation) {
    switch (validation) {
        case ProfileValidation::Ok: return "ok";
        case ProfileValidation::UnknownIdentifier: return "unknown-identifier";
        case ProfileValidation::UnsupportedRotation: return "unsupported-rotation";
        case ProfileValidation::InvalidColorOrder: return "invalid-color-order";
        case ProfileValidation::OffsetOutOfRange: return "offset-out-of-range";
        case ProfileValidation::SpiOutOfRange: return "spi-out-of-range";
        case ProfileValidation::SpiNotWholeMHz: return "spi-not-whole-mhz";
        case ProfileValidation::ResetTimingOutOfRange: return "reset-timing-out-of-range";
        case ProfileValidation::BacklightOutOfRange: return "backlight-out-of-range";
        case ProfileValidation::PresetModified: return "preset-modified";
    }
    return "unknown";
}

void markProfileCustom(ProductionDisplayProfile& profile) {
    profile.identifier = ProfileId::Custom;
}

bool decodeSupportedMadctl(const uint8_t madctl,
                           uint8_t& rotation,
                           ColorOrder& colorOrder) {
    constexpr uint8_t supported[] = {
        kMadctlRotation1,
        static_cast<uint8_t>(kMadctlRotation1 | kMadctlBgr),
        kMadctlRotation3,
        static_cast<uint8_t>(kMadctlRotation3 | kMadctlBgr)
    };
    for (const uint8_t candidate : supported) {
        if (candidate != madctl) continue;
        rotation = (candidate & kMadctlRotation3) == kMadctlRotation3 ? 3 : 1;
        colorOrder =
            (candidate & kMadctlBgr) != 0 ? ColorOrder::Bgr : ColorOrder::Rgb;
        return true;
    }
    return false;
}

uint32_t displayRecordChecksum(const uint8_t* data,
                               const std::size_t length) {
    uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask =
                static_cast<uint32_t>(-static_cast<int32_t>(crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

DisplayProfileRecord encodeDisplayProfileRecord(
    const ProductionDisplayProfile& profile) {
    DisplayProfileRecord record{};
    auto& bytes = record.bytes;
    writeU32(bytes.data() + 0, kDisplayRecordMagic);
    writeU16(bytes.data() + 4, kDisplayRecordVersion);
    writeU16(bytes.data() + 6,
             static_cast<uint16_t>(kDisplayRecordSize));
    writeU32(bytes.data() + 8, kDisplayProfileSchemaTag);
    bytes[12] = static_cast<uint8_t>(profile.identifier);
    bytes[13] = profile.rotation;
    bytes[14] = static_cast<uint8_t>(profile.colorOrder);
    bytes[15] = profile.inverted ? 1U : 0U;
    bytes[16] = kRecordReserved;
    bytes[17] = kRecordReserved;
    writeU16(bytes.data() + 18,
             static_cast<uint16_t>(profile.xOffset));
    writeU16(bytes.data() + 20,
             static_cast<uint16_t>(profile.yOffset));
    writeU32(bytes.data() + 22, profile.writeSpiHz);
    writeU32(bytes.data() + 26, profile.readSpiHz);
    writeU16(bytes.data() + 30, profile.resetLowMs);
    writeU16(bytes.data() + 32, profile.resetRecoveryMs);
    bytes[34] = profile.initialBacklight;
    bytes[35] = profile.maximumBacklight;
    for (std::size_t index = 36; index < 44; ++index) {
        bytes[index] = kRecordReserved;
    }
    writeU32(bytes.data() + 44,
             displayRecordChecksum(bytes.data(), 44));
    return record;
}

bool decodeDisplayProfileRecord(const DisplayProfileRecord& record,
                                ProductionDisplayProfile& profile) {
    const auto& bytes = record.bytes;
    if (readU32(bytes.data() + 0) != kDisplayRecordMagic ||
        readU16(bytes.data() + 4) != kDisplayRecordVersion ||
        readU16(bytes.data() + 6) != kDisplayRecordSize ||
        readU32(bytes.data() + 8) != kDisplayProfileSchemaTag ||
        bytes[16] != kRecordReserved ||
        bytes[17] != kRecordReserved ||
        readU32(bytes.data() + 44) !=
            displayRecordChecksum(bytes.data(), 44)) {
        return false;
    }
    for (std::size_t index = 36; index < 44; ++index) {
        if (bytes[index] != kRecordReserved) return false;
    }
    ProductionDisplayProfile decoded{
        static_cast<ProfileId>(bytes[12]),
        bytes[13],
        static_cast<ColorOrder>(bytes[14]),
        bytes[15] != 0,
        static_cast<int16_t>(readU16(bytes.data() + 18)),
        static_cast<int16_t>(readU16(bytes.data() + 20)),
        readU32(bytes.data() + 22),
        readU32(bytes.data() + 26),
        readU16(bytes.data() + 30),
        readU16(bytes.data() + 32),
        bytes[34],
        bytes[35]
    };
    if (bytes[15] > 1 ||
        validateDisplayProfile(decoded) != ProfileValidation::Ok) {
        return false;
    }
    profile = decoded;
    return true;
}

ProfileLoadDecision resolveDisplayProfileRecord(
    const DisplayProfileRecord* record,
    const bool recordPresent,
    ProductionDisplayProfile& profile) {
    profile = kSafeDisplayProfile;
    if (record == nullptr) {
        return recordPresent
            ? ProfileLoadDecision::SafeInvalidRecord
            : ProfileLoadDecision::SafeNoRecord;
    }
    ProductionDisplayProfile decoded{};
    if (!decodeDisplayProfileRecord(*record, decoded)) {
        return ProfileLoadDecision::SafeInvalidRecord;
    }
    profile = decoded;
    return ProfileLoadDecision::Saved;
}

const char* profileLoadDecisionName(const ProfileLoadDecision decision) {
    switch (decision) {
        case ProfileLoadDecision::Saved: return "saved";
        case ProfileLoadDecision::SafeNoRecord: return "safe-no-record";
        case ProfileLoadDecision::SafeInvalidRecord: return "safe-invalid-record";
        case ProfileLoadDecision::SafeRollback: return "safe-rollback";
    }
    return "unknown";
}

CommandParseResult parseDisplayCommand(const char* text,
                                       const std::size_t length,
                                       DisplayCommand& command) {
    std::array<Token, 5> tokens{};
    std::size_t tokenCount = 0;
    const CommandParseResult tokenResult =
        tokenize(text, length, tokens, tokenCount);
    if (tokenResult != CommandParseResult::Ok) return tokenResult;
    if (tokenCount < 2) return CommandParseResult::MissingArgument;

    auto noArguments = [&](const DisplayCommandKind kind) {
        const CommandParseResult count = requireCount(tokenCount, 2);
        if (count == CommandParseResult::Ok) command.kind = kind;
        return count;
    };

    if (tokenEquals(tokens[1], "HELP")) {
        return noArguments(DisplayCommandKind::Help);
    }
    if (tokenEquals(tokens[1], "INFO")) {
        return noArguments(DisplayCommandKind::Info);
    }
    if (tokenEquals(tokens[1], "TEST")) {
        return noArguments(DisplayCommandKind::Test);
    }
    if (tokenEquals(tokens[1], "SAVE")) {
        return noArguments(DisplayCommandKind::Save);
    }
    if (tokenEquals(tokens[1], "RESET")) {
        return noArguments(DisplayCommandKind::Reset);
    }
    if (tokenEquals(tokens[1], "SAFE")) {
        return noArguments(DisplayCommandKind::Safe);
    }
    if (tokenEquals(tokens[1], "PROFILE")) {
        if (tokenCount < 3) return CommandParseResult::MissingArgument;
        if (tokenEquals(tokens[2], "LIST")) {
            const auto count = requireCount(tokenCount, 3);
            if (count == CommandParseResult::Ok) {
                command.kind = DisplayCommandKind::ProfileList;
            }
            return count;
        }
        if (!tokenEquals(tokens[2], "SET")) {
            return CommandParseResult::UnknownCommand;
        }
        const auto count = requireCount(tokenCount, 4);
        if (count != CommandParseResult::Ok) return count;
        ProfileId identifier = ProfileId::Safe;
        if (!parseProfileIdentifier(tokens[3].data, tokens[3].length,
                                    identifier)) {
            return CommandParseResult::UnsupportedValue;
        }
        command.kind = DisplayCommandKind::ProfileSet;
        command.profile = identifier;
        return CommandParseResult::Ok;
    }

    if (tokenEquals(tokens[1], "ROTATE")) {
        const auto count = requireCount(tokenCount, 3);
        if (count != CommandParseResult::Ok) return count;
        int32_t rotation = 0;
        if (!parseInteger(tokens[2].data, tokens[2].length, rotation)) {
            return CommandParseResult::InvalidInteger;
        }
        if (rotation != 1 && rotation != 3) {
            return CommandParseResult::UnsupportedValue;
        }
        command.kind = DisplayCommandKind::Rotate;
        command.first = rotation;
        return CommandParseResult::Ok;
    }

    if (tokenEquals(tokens[1], "BGR") ||
        tokenEquals(tokens[1], "INVERT")) {
        const auto count = requireCount(tokenCount, 3);
        if (count != CommandParseResult::Ok) return count;
        if (tokenEquals(tokens[2], "ON")) command.enabled = true;
        else if (tokenEquals(tokens[2], "OFF")) command.enabled = false;
        else return CommandParseResult::UnsupportedValue;
        command.kind = tokenEquals(tokens[1], "BGR")
            ? DisplayCommandKind::Bgr
            : DisplayCommandKind::Invert;
        return CommandParseResult::Ok;
    }

    if (tokenEquals(tokens[1], "OFFSET")) {
        const auto count = requireCount(tokenCount, 4);
        if (count != CommandParseResult::Ok) return count;
        int32_t x = 0;
        int32_t y = 0;
        if (!parseInteger(tokens[2].data, tokens[2].length, x) ||
            !parseInteger(tokens[3].data, tokens[3].length, y)) {
            return CommandParseResult::InvalidInteger;
        }
        if (x < kMinimumOffset || x > kMaximumOffset ||
            y < kMinimumOffset || y > kMaximumOffset) {
            return CommandParseResult::UnsupportedValue;
        }
        command.kind = DisplayCommandKind::Offset;
        command.first = x;
        command.second = y;
        return CommandParseResult::Ok;
    }

    if (tokenEquals(tokens[1], "SPI")) {
        const auto count = requireCount(tokenCount, 3);
        if (count != CommandParseResult::Ok) return count;
        int32_t mhz = 0;
        if (!parseInteger(tokens[2].data, tokens[2].length, mhz)) {
            return CommandParseResult::InvalidInteger;
        }
        if (mhz < static_cast<int32_t>(kMinimumSpiHz / 1'000'000U) ||
            mhz > static_cast<int32_t>(kMaximumSpiHz / 1'000'000U)) {
            return CommandParseResult::UnsupportedValue;
        }
        command.kind = DisplayCommandKind::Spi;
        command.first = mhz;
        return CommandParseResult::Ok;
    }

    if (tokenEquals(tokens[1], "BACKLIGHT")) {
        const auto count = requireCount(tokenCount, 3);
        if (count != CommandParseResult::Ok) return count;
        int32_t level = 0;
        if (!parseInteger(tokens[2].data, tokens[2].length, level)) {
            return CommandParseResult::InvalidInteger;
        }
        if (level < 0 || level > kMaximumBacklight) {
            return CommandParseResult::UnsupportedValue;
        }
        command.kind = DisplayCommandKind::Backlight;
        command.first = level;
        return CommandParseResult::Ok;
    }

    return CommandParseResult::UnknownCommand;
}

const char* commandParseResultName(const CommandParseResult result) {
    switch (result) {
        case CommandParseResult::Ok: return "ok";
        case CommandParseResult::NotDisplayCommand: return "not-display-command";
        case CommandParseResult::TooLong: return "too-long";
        case CommandParseResult::TooManyTokens: return "too-many-tokens";
        case CommandParseResult::MissingArgument: return "missing-argument";
        case CommandParseResult::UnexpectedArgument: return "unexpected-argument";
        case CommandParseResult::UnknownCommand: return "unknown-command";
        case CommandParseResult::UnsupportedValue: return "unsupported-value";
        case CommandParseResult::InvalidInteger: return "invalid-integer";
    }
    return "unknown";
}

} // namespace numos::display

#endif
