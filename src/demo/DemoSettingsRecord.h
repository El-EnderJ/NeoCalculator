#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace numos::demo {

inline constexpr uint32_t kSettingsMagic = 0x3254534EU; // "NST2"
inline constexpr uint8_t kSettingsFormatVersion = 2;
inline constexpr std::size_t kSettingsRecordSize = 16;

struct DecodedSettings {
    bool angleValid = false;
    bool angleDeg = false;
    bool complexValid = false;
    bool complexEnabled = false;
    bool educationValid = false;
    bool educationEnabled = false;
    bool precisionValid = false;
    uint8_t precision = 10;
};

inline uint32_t settingsRecordChecksum(const uint8_t* data,
                                       const std::size_t length) {
    uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

inline bool settingsPrecisionValid(const uint8_t precision) {
    return precision == 6 || precision == 8 ||
           precision == 10 || precision == 12;
}

inline std::array<uint8_t, kSettingsRecordSize> encodeSettingsRecord(
    const bool angleDeg, const bool complexEnabled,
    const bool educationEnabled, const uint8_t precision) {
    std::array<uint8_t, kSettingsRecordSize> record{};
    std::memcpy(record.data(), &kSettingsMagic, sizeof(kSettingsMagic));
    record[4] = kSettingsFormatVersion;
    record[5] = angleDeg ? 1 : 0;
    record[6] = complexEnabled ? 1 : 0;
    record[7] = educationEnabled ? 1 : 0;
    record[8] = settingsPrecisionValid(precision) ? precision : 10;
    const uint32_t checksum = settingsRecordChecksum(record.data(), 12);
    std::memcpy(record.data() + 12, &checksum, sizeof(checksum));
    return record;
}

inline bool decodeSettingsRecord(const uint8_t* data, const std::size_t length,
                                 DecodedSettings& decoded) {
    decoded = {};
    if (!data || length != kSettingsRecordSize) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, data, sizeof(magic));
    if (magic != kSettingsMagic || data[4] != kSettingsFormatVersion)
        return false;
    uint32_t storedChecksum = 0;
    std::memcpy(&storedChecksum, data + 12, sizeof(storedChecksum));
    if (storedChecksum != settingsRecordChecksum(data, 12)) return false;

    if (data[5] <= 1) {
        decoded.angleValid = true;
        decoded.angleDeg = data[5] != 0;
    }
    if (data[6] <= 1) {
        decoded.complexValid = true;
        decoded.complexEnabled = data[6] != 0;
    }
    if (data[7] <= 1) {
        decoded.educationValid = true;
        decoded.educationEnabled = data[7] != 0;
    }
    if (settingsPrecisionValid(data[8])) {
        decoded.precisionValid = true;
        decoded.precision = data[8];
    }
    return true;
}

} // namespace numos::demo
