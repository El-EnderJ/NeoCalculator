#include "ProductionBringup.h"

#if defined(ARDUINO) && defined(NUMOS_BOARD_PROD_WROOM1U_N16R8) && \
    NUMOS_BOARD_PROD_WROOM1U_N16R8 && defined(NUMOS_PRODUCTION_BRINGUP)

#include <Arduino.h>
#include <cstring>
#include <esp_partition.h>
#include <esp_system.h>
#include "BoardProfile.h"
#include "../display/DisplayDriver.h"
#include "../display/ProductionDisplayProfile.h"
#include "../display/ProductionDisplayRuntime.h"
#include "../drivers/Keyboard.h"
#include "../input/NumosSerialBackend.h"
#include "../input/generated/ProductionKeypadMap.generated.h"
#include "../SystemApp.h"

#ifndef NUMOS_BUILD_REVISION
#define NUMOS_BUILD_REVISION __DATE__ " " __TIME__
#endif

namespace numos::hardware {

namespace {

constexpr uint32_t kMaximumSerialWaitMs = 3000U;
bool g_reportDeliveredToConnectedHost = false;
bool g_rawKeypadDiagnostic = false;
char g_bringupCommand[80]{};
uint8_t g_bringupCommandLength = 0;
bool g_bringupCommandOverflow = false;
bool g_lastRaw[50]{};
bool g_lastDebounced[50]{};

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

void emitActivePositions(const Keyboard& keyboard) {
    auto& serial = NUMOS_SERIAL;
    serial.print("[KEYPAD-RAW] active=");
    bool any = false;
    for (uint8_t row = 0; row < 5; ++row) {
        const uint16_t mask = keyboard.diagnosticActiveColumns(row);
        for (uint8_t column = 0; column < 10; ++column) {
            if ((mask & (1U << column)) == 0) continue;
            serial.printf("%sR%uC%u", any ? "," : "", row, column);
            any = true;
        }
    }
    if (!any) serial.print("none");
    serial.println();
}

void emitRawRecord(const Keyboard& keyboard,
                   const uint8_t row,
                   const uint8_t column,
                   const char* transition) {
    constexpr auto& board = kProductionBoard;
    const std::size_t index = static_cast<std::size_t>(row) * 10U + column;
    const auto& mapping = numos::input::kProductionKeypadMap[index];
    const auto& state = keyboard.diagnosticState(row, column);
    const int rowGpio =
        board.electricalMatrix.rowOutputs[
            board.electricalMatrix.rowOrder[row]];
    const int columnGpio =
        board.electricalMatrix.columnInputs[
            board.electricalMatrix.columnOrder[column]];
    NUMOS_SERIAL.printf(
        "[KEYPAD-RAW] eR=%u eC=%u row_gpio=%d col_gpio=%d SW%u "
        "visual=r%uc%u pcb_um=(%ld,%ld) rotation_deg=%d "
        "keycode=%u label=\"%s\" raw=%u debounced=%u "
        "transition=%s integrator=%u overflow=%lu\n",
        row, column, rowGpio, columnGpio, mapping.switchNumber,
        mapping.visualRow, mapping.visualColumn,
        static_cast<long>(mapping.pcbXUm),
        static_cast<long>(mapping.pcbYUm),
        static_cast<int>(mapping.rotationDegrees),
        static_cast<unsigned>(mapping.keyCode), mapping.primaryLabel,
        state.raw, state.debounced, transition, state.integrator,
        static_cast<unsigned long>(keyboard.overflowCount()));
}

bool commandEquals(const char* expected) {
    return strcmp(g_bringupCommand, expected) == 0;
}

void emitDisplayProfile(const char* prefix,
                        const numos::display::ProductionDisplayProfile& profile,
                        const uint8_t backlight) {
    NUMOS_SERIAL.printf(
        "%s id=%s rotation=%u madctl=0x%02X order=%s invert=%u "
        "offset=(%d,%d) write_mhz=%u read_mhz=%u reset_ms=%u/%u "
        "backlight=%u initial=%u max=%u\n",
        prefix, numos::display::profileIdentifier(profile.identifier),
        profile.rotation, numos::display::displayMadctl(profile),
        profile.colorOrder == numos::display::ColorOrder::Bgr ? "BGR" : "RGB",
        profile.inverted, profile.xOffset, profile.yOffset,
        profile.writeSpiHz / 1'000'000U,
        profile.readSpiHz / 1'000'000U,
        profile.resetLowMs, profile.resetRecoveryMs,
        backlight, profile.initialBacklight, profile.maximumBacklight);
}

void emitDisplayHelp() {
    NUMOS_SERIAL.println("[DISPLAY] commands:");
    NUMOS_SERIAL.println("[DISPLAY] DISPLAY HELP | DISPLAY INFO | DISPLAY TEST");
    NUMOS_SERIAL.println(
        "[DISPLAY] DISPLAY PROFILE LIST | DISPLAY PROFILE SET <id>");
    NUMOS_SERIAL.println(
        "[DISPLAY] DISPLAY ROTATE 1|3 | DISPLAY BGR ON|OFF | "
        "DISPLAY INVERT ON|OFF");
    NUMOS_SERIAL.println(
        "[DISPLAY] DISPLAY OFFSET <-32..32> <-32..32> | "
        "DISPLAY SPI <1..40>");
    NUMOS_SERIAL.println(
        "[DISPLAY] DISPLAY BACKLIGHT <0..192> | DISPLAY SAVE | "
        "DISPLAY RESET | DISPLAY SAFE");
    NUMOS_SERIAL.println(
        "[DISPLAY] SAVE is explicit; invalid DISPLAY input restores SAFE");
}

void emitDisplayInfo(const DisplayDriver& display) {
    const auto& profile =
        numos::display::activeProductionDisplayProfile();
    emitDisplayProfile("[DISPLAY] active", profile,
                       display.backlightLevel());
    NUMOS_SERIAL.printf(
        "[DISPLAY] source=%s driver=TFT_eSPI-2.5.43 "
        "failures=%u/%u reset=%s "
        "runtime=rotation,madctl-rgb-bgr,inversion,offset,spi,backlight "
        "compile_time=controller,pins,bus,geometry,pixel-format\n",
        numos::display::profileLoadDecisionName(
            numos::display::productionDisplayLoadDecision()),
        numos::display::productionDisplayFailureCount(),
        numos::display::kDisplayBootFailureThreshold,
        numos::display::displayResetClassName(
            numos::display::productionDisplayResetClass()));
}

bool applyDisplayCandidate(
    DisplayDriver& display,
    const numos::display::ProductionDisplayProfile& candidate,
    const bool resetController) {
    const auto validation =
        numos::display::validateDisplayProfile(candidate);
    if (validation != numos::display::ProfileValidation::Ok ||
        !display.applyProductionDisplayProfile(candidate, resetController)) {
        display.restoreSafeProductionDisplayProfile();
        NUMOS_SERIAL.printf(
            "[DISPLAY] ERROR apply=%s; immutable SAFE restored\n",
            numos::display::profileValidationName(validation));
        return false;
    }
    emitDisplayProfile("[DISPLAY] OK", candidate,
                       display.backlightLevel());
    return true;
}

void processDisplayCommand(const numos::display::DisplayCommand& command,
                           DisplayDriver& display,
                           SystemApp& app) {
    using numos::display::ColorOrder;
    using numos::display::DisplayCommandKind;
    using numos::display::ProductionDisplayProfile;
    auto candidate =
        numos::display::activeProductionDisplayProfile();

    switch (command.kind) {
        case DisplayCommandKind::Help:
            emitDisplayHelp();
            return;
        case DisplayCommandKind::Info:
            emitDisplayInfo(display);
            return;
        case DisplayCommandKind::Test:
            NUMOS_SERIAL.println(
                "[DISPLAY] TEST BEGIN bounded; launcher resumes on completion");
            display.runBoundedProductionDisplayDiagnostic();
            app.returnToLauncherAfterDiagnostic();
            NUMOS_SERIAL.println("[DISPLAY] TEST END launcher restored");
            return;
        case DisplayCommandKind::ProfileList:
            for (const auto& preset :
                 numos::display::kProductionDisplayPresets) {
                emitDisplayProfile("[DISPLAY] preset", preset,
                                   preset.initialBacklight);
            }
            return;
        case DisplayCommandKind::ProfileSet: {
            const ProductionDisplayProfile* preset =
                numos::display::findPreset(command.profile);
            if (preset == nullptr ||
                !applyDisplayCandidate(display, *preset, true)) {
                display.restoreSafeProductionDisplayProfile();
            }
            return;
        }
        case DisplayCommandKind::Rotate:
            numos::display::markProfileCustom(candidate);
            candidate.rotation = static_cast<uint8_t>(command.first);
            (void)applyDisplayCandidate(display, candidate, false);
            return;
        case DisplayCommandKind::Bgr:
            numos::display::markProfileCustom(candidate);
            candidate.colorOrder =
                command.enabled ? ColorOrder::Bgr : ColorOrder::Rgb;
            (void)applyDisplayCandidate(display, candidate, false);
            return;
        case DisplayCommandKind::Invert:
            numos::display::markProfileCustom(candidate);
            candidate.inverted = command.enabled;
            (void)applyDisplayCandidate(display, candidate, false);
            return;
        case DisplayCommandKind::Offset:
            numos::display::markProfileCustom(candidate);
            candidate.xOffset = static_cast<int16_t>(command.first);
            candidate.yOffset = static_cast<int16_t>(command.second);
            (void)applyDisplayCandidate(display, candidate, false);
            return;
        case DisplayCommandKind::Spi:
            numos::display::markProfileCustom(candidate);
            candidate.writeSpiHz =
                static_cast<uint32_t>(command.first) * 1'000'000U;
            candidate.readSpiHz = candidate.writeSpiHz;
            (void)applyDisplayCandidate(display, candidate, false);
            return;
        case DisplayCommandKind::Backlight:
            numos::display::markProfileCustom(candidate);
            candidate.initialBacklight =
                static_cast<uint8_t>(command.first);
            (void)applyDisplayCandidate(display, candidate, false);
            return;
        case DisplayCommandKind::Save:
            if (numos::display::saveActiveProductionDisplayProfile()) {
                NUMOS_SERIAL.println(
                    "[DISPLAY] SAVE OK version=2 crc32=valid failures=0");
            } else {
                display.restoreSafeProductionDisplayProfile();
                NUMOS_SERIAL.println(
                    "[DISPLAY] SAVE ERROR; immutable SAFE restored");
            }
            return;
        case DisplayCommandKind::Reset:
            if (!display.applyProductionDisplayProfile(candidate, true)) {
                display.restoreSafeProductionDisplayProfile();
                NUMOS_SERIAL.println(
                    "[DISPLAY] RESET ERROR; immutable SAFE restored");
            } else {
                NUMOS_SERIAL.println(
                    "[DISPLAY] RESET OK active profile reapplied");
            }
            return;
        case DisplayCommandKind::Safe:
            display.restoreSafeProductionDisplayProfile();
            NUMOS_SERIAL.println(
                "[DISPLAY] SAFE OK immutable profile restored; not auto-saved");
            return;
    }
}

void processBringupCommand(DisplayDriver& display, SystemApp& app) {
    auto& serial = NUMOS_SERIAL;
    numos::display::DisplayCommand displayCommand{};
    const auto displayResult = numos::display::parseDisplayCommand(
        g_bringupCommand, g_bringupCommandLength, displayCommand);
    if (displayResult == numos::display::CommandParseResult::Ok) {
        processDisplayCommand(displayCommand, display, app);
        return;
    }
    if (displayResult !=
        numos::display::CommandParseResult::NotDisplayCommand) {
        display.restoreSafeProductionDisplayProfile();
        serial.printf(
            "[DISPLAY] ERROR parse=%s; immutable SAFE restored\n",
            numos::display::commandParseResultName(displayResult));
        return;
    }

    if (commandEquals("KEYPAD RAW ON")) {
        g_rawKeypadDiagnostic = true;
        for (std::size_t i = 0; i < 50; ++i) {
            g_lastRaw[i] = false;
            g_lastDebounced[i] = false;
        }
        serial.println(
            "[KEYPAD-RAW] enabled; press visual keys top-left to bottom-right");
    } else if (commandEquals("KEYPAD RAW OFF")) {
        g_rawKeypadDiagnostic = false;
        serial.println("[KEYPAD-RAW] disabled; mapped input remains enabled");
    } else if (commandEquals("KEYPAD RAW STATUS")) {
        serial.printf("[KEYPAD-RAW] enabled=%u layout_sha=%s\n",
                      g_rawKeypadDiagnostic,
                      numos::input::kProductionLayoutSha256);
    } else if (commandEquals("KEYPAD HELP")) {
        serial.println(
            "[KEYPAD] commands: KEYPAD RAW ON | KEYPAD RAW OFF | "
            "KEYPAD RAW STATUS");
    }
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

void serviceProductionBringupCommands(Keyboard& keyboard,
                                      DisplayDriver& display,
                                      SystemApp& app) {
    auto& serial = NUMOS_SERIAL;
    while (serial.available() > 0) {
        const int value = serial.read();
        if (value < 0) break;
        const char ch = static_cast<char>(value);
        if (ch == '\r' || ch == '\n') {
            if (g_bringupCommandOverflow) {
                display.restoreSafeProductionDisplayProfile();
                serial.println(
                    "[DISPLAY] ERROR parse=too-long; immutable SAFE restored");
                g_bringupCommandOverflow = false;
                g_bringupCommandLength = 0;
            } else if (g_bringupCommandLength > 0) {
                g_bringupCommand[g_bringupCommandLength] = '\0';
                processBringupCommand(display, app);
                g_bringupCommandLength = 0;
            }
            continue;
        }
        if (g_bringupCommandOverflow) continue;
        if (g_bringupCommandLength + 1U >=
            sizeof(g_bringupCommand)) {
            g_bringupCommandOverflow = true;
            g_bringupCommandLength = 0;
            continue;
        }
        g_bringupCommand[g_bringupCommandLength++] =
            (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 32) : ch;
    }

    if (!g_rawKeypadDiagnostic || !keyboard.initialized()) return;
    bool changed = false;
    for (uint8_t row = 0; row < 5; ++row) {
        for (uint8_t column = 0; column < 10; ++column) {
            const std::size_t index =
                static_cast<std::size_t>(row) * 10U + column;
            const auto& state = keyboard.diagnosticState(row, column);
            if (state.raw != g_lastRaw[index]) {
                emitRawRecord(keyboard, row, column,
                              state.raw ? "raw-down" : "raw-up");
                g_lastRaw[index] = state.raw;
                changed = true;
            }
            if (state.debounced != g_lastDebounced[index]) {
                emitRawRecord(keyboard, row, column,
                              state.debounced ? "down" : "up");
                g_lastDebounced[index] = state.debounced;
                changed = true;
            }
        }
    }
    if (changed) emitActivePositions(keyboard);
}

} // namespace numos::hardware

#endif
