/*
 * NumOS production hardware contract.
 *
 * This header is deliberately Arduino-free so the same constexpr contract can
 * be compiled by host tests. GPIO numbers describe electrical PCB nets, not a
 * visual keypad layout.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace numos::hardware {

enum class Direction : uint8_t {
    Output,
    Input,
    Bidirectional,
    Reserved,
    HardwareOnly,
    NotConnected
};

enum class ActiveLevel : uint8_t {
    None,
    Low,
    High
};

enum class Pull : uint8_t {
    None,
    Up10k,
    Down10k,
    InternalUpWhenScanning
};

struct Signal {
    const char* role;
    int8_t gpio;
    Direction direction;
    ActiveLevel activeLevel;
    Pull pull;
};

struct DisplayProfile {
    uint16_t logicalWidth;
    uint16_t logicalHeight;
    uint32_t initialSpiHz;
    Signal chipSelect;
    Signal clock;
    Signal dataCommand;
    Signal mosi;
    Signal miso;
    Signal reset;
    Signal backlight;
    bool misoRequiredForBasicRendering;
    bool teConnected;
    uint8_t provisionalMadctl;
    bool provisionalRgb565;
};

struct MatrixElectricalProfile {
    std::array<int8_t, 5> rowOutputs;
    std::array<int8_t, 10> columnInputs;
    ActiveLevel selectedRowLevel;
    ActiveLevel inactiveRowLevel;
    ActiveLevel pressedColumnLevel;
    bool columnsUseInternalPullups;
    bool perKeyDiodesFitted;
    bool logicalMappingReady;
};

struct CapabilityProfile {
    bool batteryAdc;
    bool softwareRegulatorControl;
    bool chargerStatusGpios;
};

struct ProductionBoardProfile {
    const char* boardId;
    const char* module;
    uint32_t expectedFlashBytes;
    uint32_t expectedPsramBytes;
    DisplayProfile display;
    MatrixElectricalProfile electricalMatrix;
    Signal usbDataMinus;
    Signal usbDataPlus;
    Signal boot;
    Signal resetEnable;
    Signal uart0TxTestPoint;
    Signal uart0RxTestPoint;
    std::array<Signal, 3> otherStraps;
    std::array<int8_t, 6> deliberatelyUnassigned;
    CapabilityProfile capabilities;
};

inline constexpr ProductionBoardProfile kProductionBoard = {
    "numos-esp32-s3-wroom-1u-n16r8",
    "ESP32-S3-WROOM-1U-N16R8",
    16U * 1024U * 1024U,
    8U * 1024U * 1024U,
    {
        320,
        240,
        10'000'000U,
        {"LCD CS", 38, Direction::Output, ActiveLevel::Low, Pull::None},
        {"LCD SCLK", 39, Direction::Output, ActiveLevel::None, Pull::None},
        {"LCD DC", 40, Direction::Output, ActiveLevel::None, Pull::None},
        {"LCD MOSI/SDI", 41, Direction::Output, ActiveLevel::None, Pull::None},
        {"LCD MISO/SDO", 42, Direction::Input, ActiveLevel::None, Pull::None},
        {"LCD reset", 1, Direction::Output, ActiveLevel::Low, Pull::None},
        {"LCD backlight", 2, Direction::Output, ActiveLevel::High, Pull::None},
        false,
        false,
        0x28,
        true
    },
    {
        {9, 21, 47, 48, 11},
        {4, 5, 6, 7, 15, 16, 17, 18, 8, 10},
        ActiveLevel::Low,
        ActiveLevel::High,
        ActiveLevel::Low,
        true,
        true,
        false
    },
    {"native USB D-", 19, Direction::Reserved, ActiveLevel::None, Pull::None},
    {"native USB D+", 20, Direction::Reserved, ActiveLevel::None, Pull::None},
    {"BOOT K2/TP6", 0, Direction::Reserved, ActiveLevel::Low, Pull::None},
    {"RESET/EN K1/TP5", -1, Direction::HardwareOnly, ActiveLevel::Low, Pull::None},
    {"UART0 TX TP14", 43, Direction::Output, ActiveLevel::None, Pull::None},
    {"UART0 RX TP15", 44, Direction::Input, ActiveLevel::None, Pull::None},
    {{
        {"strap GPIO3", 3, Direction::Reserved, ActiveLevel::None, Pull::Up10k},
        {"strap GPIO45", 45, Direction::Reserved, ActiveLevel::None, Pull::Up10k},
        {"strap GPIO46", 46, Direction::Reserved, ActiveLevel::None, Pull::Down10k}
    }},
    {12, 13, 14, 35, 36, 37},
    {false, false, false}
};

// This is identity-only metadata for a regression guard. The authoritative CAM
// contract remains in its existing board manifest and Config.h branch.
struct ExistingCamIdentity {
    const char* boardId;
    std::array<int8_t, 6> displayPins;
};

inline constexpr ExistingCamIdentity kExistingCamIdentity = {
    "numos-esp32-s3-n16r8-cam",
    {13, 12, 10, 4, 5, 45}
};

constexpr bool isValidEsp32S3Gpio(const int gpio) {
    return (gpio >= 0 && gpio <= 21) || (gpio >= 26 && gpio <= 48);
}

template <std::size_t N>
constexpr bool contains(const std::array<int8_t, N>& values, const int gpio) {
    for (const int value : values) {
        if (value == gpio) {
            return true;
        }
    }
    return false;
}

template <std::size_t N>
constexpr bool allValid(const std::array<int8_t, N>& values) {
    for (const int value : values) {
        if (!isValidEsp32S3Gpio(value)) {
            return false;
        }
    }
    return true;
}

template <std::size_t N>
constexpr bool allUnique(const std::array<int8_t, N>& values) {
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            if (values[i] == values[j]) {
                return false;
            }
        }
    }
    return true;
}

inline constexpr std::array<int8_t, 22> kProductionNormalPeripheralGpios = {
    38, 39, 40, 41, 42, 1, 2,
    9, 21, 47, 48, 11,
    4, 5, 6, 7, 15, 16, 17, 18, 8, 10
};

static_assert(kProductionBoard.electricalMatrix.rowOutputs.size() == 5);
static_assert(kProductionBoard.electricalMatrix.columnInputs.size() == 10);
static_assert(allValid(kProductionNormalPeripheralGpios),
              "Production profile contains an invalid ESP32-S3 GPIO");
static_assert(allUnique(kProductionNormalPeripheralGpios),
              "Production display/matrix GPIO allocation contains a collision");
static_assert(!contains(kProductionNormalPeripheralGpios, 19) &&
              !contains(kProductionNormalPeripheralGpios, 20),
              "Native USB GPIO19/GPIO20 must remain reserved");
static_assert(!contains(kProductionNormalPeripheralGpios, 0) &&
              !contains(kProductionNormalPeripheralGpios, 3) &&
              !contains(kProductionNormalPeripheralGpios, 45) &&
              !contains(kProductionNormalPeripheralGpios, 46),
              "BOOT/strap pins must not be normal peripherals");
static_assert(!contains(kProductionNormalPeripheralGpios, 12) &&
              !contains(kProductionNormalPeripheralGpios, 13) &&
              !contains(kProductionNormalPeripheralGpios, 14) &&
              !contains(kProductionNormalPeripheralGpios, 35) &&
              !contains(kProductionNormalPeripheralGpios, 36) &&
              !contains(kProductionNormalPeripheralGpios, 37),
              "Milestone-reserved unconnected GPIOs must remain unassigned");
static_assert(!kProductionBoard.electricalMatrix.logicalMappingReady);
static_assert(!kProductionBoard.capabilities.batteryAdc);
static_assert(!kProductionBoard.capabilities.softwareRegulatorControl);
static_assert(!kProductionBoard.capabilities.chargerStatusGpios);
static_assert(kProductionBoard.display.initialSpiHz <= 10'000'000U);
static_assert(kProductionBoard.display.chipSelect.gpio !=
              kExistingCamIdentity.displayPins[2],
              "Production profile must not inherit the CAM display contract");

} // namespace numos::hardware
