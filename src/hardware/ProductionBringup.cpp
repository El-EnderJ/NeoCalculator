#include "ProductionBringup.h"

#if defined(ARDUINO) && defined(NUMOS_BOARD_PROD_WROOM1U_N16R8) && \
    NUMOS_BOARD_PROD_WROOM1U_N16R8 && defined(NUMOS_PRODUCTION_BRINGUP)

#include <Arduino.h>
#include <esp_partition.h>
#include <esp_system.h>
#include "BoardProfile.h"
#include "../input/NumosSerialBackend.h"

#ifndef NUMOS_BUILD_REVISION
#define NUMOS_BUILD_REVISION __DATE__ " " __TIME__
#endif

namespace numos::hardware {

namespace {

constexpr uint32_t kMaximumSerialWaitMs = 3000U;
bool g_reportDeliveredToConnectedHost = false;

const char* partitionTypeName(const uint8_t type) {
    return type == ESP_PARTITION_TYPE_APP ? "app" :
           type == ESP_PARTITION_TYPE_DATA ? "data" : "other";
}

void emitProductionBringupReport() {
    constexpr auto& board = kProductionBoard;
    auto& serial = NUMOS_SERIAL;

    serial.println("[PCBA] BEGIN bounded production bring-up report");
    serial.printf("[PCBA] board=%s module=%s revision=%s\n",
                  board.boardId, board.module, NUMOS_BUILD_REVISION);
    serial.printf("[PCBA] reset_reason=%d\n", static_cast<int>(esp_reset_reason()));
    serial.printf("[PCBA] flash expected=%u detected=%u\n",
                  board.expectedFlashBytes, ESP.getFlashChipSize());
    serial.printf("[PCBA] psram expected=%u detected=%u usable_free=%u\n",
                  board.expectedPsramBytes, ESP.getPsramSize(), ESP.getFreePsram());
    serial.printf("[PCBA] usb mode=%d cdc_on_boot=%d pins=%d/%d backend=%s\n",
                  ARDUINO_USB_MODE, ARDUINO_USB_CDC_ON_BOOT,
                  board.usbDataMinus.gpio, board.usbDataPlus.gpio,
                  NUMOS_SERIAL_BACKEND_LABEL);
    serial.printf("[PCBA] display %ux%u spi=%u CS=%d SCLK=%d DC=%d MOSI=%d MISO=%d RST=%d BL=%d\n",
                  board.display.logicalWidth, board.display.logicalHeight,
                  board.display.initialSpiHz,
                  board.display.chipSelect.gpio, board.display.clock.gpio,
                  board.display.dataCommand.gpio, board.display.mosi.gpio,
                  board.display.miso.gpio, board.display.reset.gpio,
                  board.display.backlight.gpio);
    serial.print("[PCBA] matrix electrical_rows=");
    for (const int gpio : board.electricalMatrix.rowOutputs) {
        serial.printf("%d,", gpio);
    }
    serial.print(" electrical_columns=");
    for (const int gpio : board.electricalMatrix.columnInputs) {
        serial.printf("%d,", gpio);
    }
    serial.printf(" mapping_ready=%d scanner_enabled=0\n",
                  board.electricalMatrix.logicalMappingReady);
    serial.printf("[PCBA] capabilities battery_adc=%d regulator_gpio=%d charger_gpio=%d\n",
                  board.capabilities.batteryAdc,
                  board.capabilities.softwareRegulatorControl,
                  board.capabilities.chargerStatusGpios);

    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_ANY,
                           ESP_PARTITION_SUBTYPE_ANY,
                           nullptr);
    unsigned count = 0;
    while (it != nullptr && count < 16U) {
        const esp_partition_t* part = esp_partition_get(it);
        serial.printf("[PCBA] partition[%u] type=%s subtype=0x%02x label=%s offset=0x%06x size=0x%06x\n",
                      count, partitionTypeName(part->type), part->subtype,
                      part->label, part->address, part->size);
        it = esp_partition_next(it);
        ++count;
    }
    if (it != nullptr) {
        esp_partition_iterator_release(it);
        serial.println("[PCBA] partition report truncated at 16 entries");
    }
    serial.println("[PCBA] END report; LCD/key matrix remain physically unvalidated");
}

} // namespace

void waitForProductionBringupSerial() {
    auto& serial = NUMOS_SERIAL;
    const uint32_t startedAt = millis();
    while (!serial && (millis() - startedAt) < kMaximumSerialWaitMs) {
        delay(10);
    }
}

void startProductionBringupReporting() {
    g_reportDeliveredToConnectedHost = false;
    serviceProductionBringupReporting();
}

void serviceProductionBringupReporting() {
    if (g_reportDeliveredToConnectedHost || !NUMOS_SERIAL) {
        return;
    }
    emitProductionBringupReport();
    g_reportDeliveredToConnectedHost = true;
}

} // namespace numos::hardware

#endif
