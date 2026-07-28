#include "hardware/BoardProfile.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace numos::hardware;

int main() {
    const auto& board = kProductionBoard;

    assert(std::strcmp(board.boardId,
                       "numos-esp32-s3-wroom-1u-n16r8") == 0);
    assert(std::strcmp(board.module,
                       "ESP32-S3-WROOM-1U-N16R8") == 0);
    assert(board.expectedFlashBytes == 16U * 1024U * 1024U);
    assert(board.expectedPsramBytes == 8U * 1024U * 1024U);

    assert(board.display.logicalWidth == 320);
    assert(board.display.logicalHeight == 240);
    assert(board.display.chipSelect.gpio == 38);
    assert(board.display.clock.gpio == 39);
    assert(board.display.dataCommand.gpio == 40);
    assert(board.display.mosi.gpio == 41);
    assert(board.display.miso.gpio == 42);
    assert(board.display.reset.gpio == 1);
    assert(board.display.reset.activeLevel == ActiveLevel::Low);
    assert(board.display.backlight.gpio == 2);
    assert(board.display.backlight.activeLevel == ActiveLevel::High);
    assert(board.display.initialSpiHz == 10'000'000U);
    assert(!board.display.misoRequiredForBasicRendering);
    assert(!board.display.teConnected);

    constexpr std::array<int8_t, 5> expectedRows = {9, 21, 47, 48, 11};
    constexpr std::array<int8_t, 10> expectedColumns =
        {4, 5, 6, 7, 15, 16, 17, 18, 8, 10};
    assert(board.electricalMatrix.rowOutputs == expectedRows);
    assert(board.electricalMatrix.columnInputs == expectedColumns);
    constexpr std::array<uint8_t, 5> expectedRowOrder = {0, 1, 2, 3, 4};
    constexpr std::array<uint8_t, 10> expectedColumnOrder =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    assert(board.electricalMatrix.rowOrder == expectedRowOrder);
    assert(board.electricalMatrix.columnOrder == expectedColumnOrder);
    assert(board.electricalMatrix.inactiveRowLevel == ActiveLevel::High);
    assert(board.electricalMatrix.selectedRowLevel == ActiveLevel::Low);
    assert(board.electricalMatrix.pressedColumnLevel == ActiveLevel::Low);
    assert(board.electricalMatrix.columnsUseInternalPullups);
    assert(board.electricalMatrix.perKeyDiodesFitted);
    assert(board.electricalMatrix.fullScanIntervalUs == 5'000U);
    assert(board.electricalMatrix.settlingDurationUs == 10U);
    assert(board.electricalMatrix.logicalMappingReady);

    assert(board.usbDataMinus.gpio == 19);
    assert(board.usbDataPlus.gpio == 20);
    assert(board.boot.gpio == 0);
    assert(board.uart0TxTestPoint.gpio == 43);
    assert(board.uart0RxTestPoint.gpio == 44);
    assert(board.otherStraps[0].gpio == 3);
    assert(board.otherStraps[1].gpio == 45);
    assert(board.otherStraps[2].gpio == 46);

    assert(!board.capabilities.batteryAdc);
    assert(!board.capabilities.softwareRegulatorControl);
    assert(!board.capabilities.chargerStatusGpios);
    assert(std::strcmp(board.boardId, kExistingCamIdentity.boardId) != 0);

    std::cout << "production_hardware_profile_test: PASS\n";
    return 0;
}
