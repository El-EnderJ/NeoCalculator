#include "DemoDiagnostics.h"

#if NUMOS_PRODUCTION_DEMO_PROFILE && defined(ARDUINO)

#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "../SystemApp.h"
#include "../display/DisplayDriver.h"
#include "../display/ProductionDisplayRuntime.h"
#include "../drivers/Keyboard.h"
#include "../input/KeyboardManager.h"
#include "../input/NumosSerialBackend.h"
#include "../input/generated/ProductionKeypadMap.generated.h"
#include "../math/VariableManager.h"
#include "DemoBootHealth.h"
#include "DemoLiveness.h"

namespace numos::demo {

namespace {

bool equalsIgnoreCase(const char* left, const char* right) {
    if (!left || !right) return false;
    while (*left && *right) {
        char a = *left++;
        char b = *right++;
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 32);
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 32);
        if (a != b) return false;
    }
    return *left == '\0' && *right == '\0';
}

struct LvglObjectCount {
    uint16_t count = 0;
    bool truncated = false;
};

LvglObjectCount countActiveLvglObjects() {
    // WHY: diagnostic-only fixed traversal avoids recursion and heap use.
    std::array<lv_obj_t*, 96> pending{};
    std::size_t head = 0;
    std::size_t tail = 0;
    if (lv_screen_active()) pending[tail++] = lv_screen_active();

    LvglObjectCount result{};
    while (head < tail) {
        lv_obj_t* object = pending[head++];
        ++result.count;
        const uint32_t children = lv_obj_get_child_count(object);
        for (uint32_t index = 0; index < children; ++index) {
            if (tail >= pending.size()) {
                result.truncated = true;
                return result;
            }
            pending[tail++] = lv_obj_get_child(object, index);
        }
    }
    return result;
}

} // namespace

DemoDiagnostics::DemoDiagnostics(Keyboard& keyboard, DisplayDriver& display,
                                 SystemApp& app)
    : _keyboard(keyboard), _display(display), _app(app) {}

bool DemoDiagnostics::confirmedCommand(const char* line,
                                       const char* command) const {
    char expected[80] = {};
    const int written =
        snprintf(expected, sizeof(expected), "%s CONFIRM", command);
    return written > 0 && static_cast<std::size_t>(written) < sizeof(expected) &&
           equalsIgnoreCase(line, expected);
}

void DemoDiagnostics::printHelp() const {
    NUMOS_SERIAL.println(
        "[DIAG] DEMO INFO | DEMO KEYPAD RAW ON|OFF|STATUS | "
        "DEMO DISPLAY SAFE CONFIRM | DEMO FACTORY RESET CONFIRM");
    NUMOS_SERIAL.println(
        "[DIAG] DEMO SAFE ON CONFIRM | DEMO CLEAR SAFE CONFIRM | "
        "DEMO FS FORMAT CONFIRM | DEMO REBOOT CONFIRM | "
        "DEMO SOAK START CONFIRM|STOP|STATUS");
}

void DemoDiagnostics::inject(const KeyCode code,
                             const KeyAction action) {
    KeyEvent event{};
    event.code = code;
    event.action = action;
    event.row = -1;
    event.col = -1;
    _app.injectKey(event);
}

void DemoDiagnostics::printSoakStatus() const {
    const LvglObjectCount lvgl = countActiveLvglObjects();
    NUMOS_SERIAL.printf(
        "[SOAK] active=%u iterations=%u elapsed_ms=%u heap=%u->%u "
        "min_heap=%u psram=%u->%u min_psram=%u overflow=%u retained=%u "
        "lvgl_objects=%u lvgl_truncated=%u last_step=%s reset=%s\n",
        _soakActive, static_cast<unsigned>(_soakIterations),
        static_cast<unsigned>(_soakStartedMs
            ? millis() - _soakStartedMs : 0),
        static_cast<unsigned>(_soakHeapBefore),
        static_cast<unsigned>(ESP.getFreeHeap()),
        static_cast<unsigned>(ESP.getMinFreeHeap()),
        static_cast<unsigned>(_soakPsramBefore),
        static_cast<unsigned>(ESP.getFreePsram()),
        static_cast<unsigned>(
            heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned>(_keyboard.overflowCount()),
        static_cast<unsigned>(_app.demoRetainedExpressionCount()),
        static_cast<unsigned>(lvgl.count), lvgl.truncated,
        _soakLastStep, resetClassName(bootHealthRecord().lastReset));
}

void DemoDiagnostics::serviceSoak() {
    if (!_soakActive || millis() - _soakLastStepMs < 350U) return;
    _soakLastStepMs = millis();

    switch (_soakStep) {
        case 0:
            _soakLastStep = "calculation-enter";
            _app.launchApp(0);
            break;
        case 1:
            _soakLastStep = "calculation-eval-clear-repeat";
            inject(KeyCode::NUM_1);
            inject(KeyCode::ADD);
            inject(KeyCode::NUM_1);
            inject(KeyCode::ENTER);
            inject(KeyCode::AC);
            inject(KeyCode::RIGHT, KeyAction::REPEAT);
            break;
        case 2:
            _soakLastStep = "calculation-force-release";
            _app.demoReturnToLauncher();
            break;
        case 3:
            _soakLastStep = "grapher-enter";
            _app.launchApp(1);
            break;
        case 4:
            _soakLastStep = "grapher-create";
            inject(KeyCode::DOWN);
            inject(KeyCode::ENTER);
            inject(KeyCode::VAR_Y);
            inject(KeyCode::FREE_EQ);
            inject(KeyCode::VAR_X);
            inject(KeyCode::POW);
            inject(KeyCode::NUM_2);
            inject(KeyCode::ENTER);
            break;
        case 5:
            _soakLastStep = "grapher-destroy";
            _app.demoReturnToLauncher();
            break;
        case 6:
            _soakLastStep = "settings-lifecycle";
            _app.launchApp(10);
            break;
        case 7:
            _soakLastStep = "modifier-cycle-release";
            inject(KeyCode::SHIFT);
            inject(KeyCode::ALPHA);
            _app.demoReturnToLauncher();
            ++_soakIterations;
            if ((_soakIterations % 10U) == 0U) {
                _display.restoreSafeProductionDisplayProfile();
                printSoakStatus();
            }
            if (_soakIterations >= 100U) {
                _soakActive = false;
                _soakLastStep = "bounded-complete";
                printSoakStatus();
            }
            break;
    }
    _soakStep = static_cast<uint8_t>((_soakStep + 1U) % 8U);
}

void DemoDiagnostics::printInfo() const {
    const auto& health = bootHealthRecord();
    const auto& profile =
        numos::display::activeProductionDisplayProfile();
    NUMOS_SERIAL.printf(
        "[DIAG] commit=%s env=%s board=numos-esp32-s3-wroom-1u-n16r8\n",
        NUMOS_BUILD_COMMIT, NUMOS_BUILD_ENVIRONMENT);
    NUMOS_SERIAL.printf(
        "[DIAG] reset=%s safe=%u failures=%u phase=%u last_failure=%s "
        "last_app=%u launcher_ms=%u\n",
        resetClassName(health.lastReset), safeModeActive(),
        health.consecutiveFailures, static_cast<unsigned>(health.phase),
        failureCodeName(health.lastFailure), lastAppMode(),
        static_cast<unsigned>(_launcherReadyMs));
    NUMOS_SERIAL.printf(
        "[DIAG] flash=%u psram_found=%u psram_free=%u "
        "profile=%s profile_source=%s display_failures=%u\n",
        static_cast<unsigned>(ESP.getFlashChipSize()), psramFound(),
        static_cast<unsigned>(ESP.getFreePsram()),
        numos::display::profileIdentifier(profile.identifier),
        numos::display::profileLoadDecisionName(
            numos::display::productionDisplayLoadDecision()),
        numos::display::productionDisplayFailureCount());
    NUMOS_SERIAL.printf(
        "[DIAG] keypad_layout_sha=%s overflow=%u raw=%u fs_mounted=%u "
        "ui_wdt=%u giac_wdt_suspended=%u modifier=%s\n",
        numos::input::kProductionLayoutSha256,
        static_cast<unsigned>(_keyboard.overflowCount()), _rawKeypad,
        _app.filesystemMounted(), uiLoopWatchdogEnabled(),
        giacWatchdogCoverageSuspended(),
        vpam::KeyboardManager::instance().indicatorText());
}

bool DemoDiagnostics::handleLine(const char* line) {
    if (!line || !*line) return false;
    if (equalsIgnoreCase(line, "DEMO HELP")) {
        printHelp();
        return true;
    }
    if (equalsIgnoreCase(line, "DEMO INFO")) {
        printInfo();
        return true;
    }
    if (equalsIgnoreCase(line, "DEMO KEYPAD RAW ON")) {
        _rawKeypad = true;
        _lastRaw.fill(false);
        _lastDebounced.fill(false);
        NUMOS_SERIAL.println("[DIAG] keypad_raw=on");
        return true;
    }
    if (equalsIgnoreCase(line, "DEMO KEYPAD RAW OFF")) {
        _rawKeypad = false;
        NUMOS_SERIAL.println("[DIAG] keypad_raw=off");
        return true;
    }
    if (equalsIgnoreCase(line, "DEMO KEYPAD RAW STATUS")) {
        NUMOS_SERIAL.printf("[DIAG] keypad_raw=%u overflow=%u\n",
                            _rawKeypad, _keyboard.overflowCount());
        return true;
    }
    if (confirmedCommand(line, "DEMO SOAK START")) {
        _soakActive = true;
        _soakStep = 0;
        _soakIterations = 0;
        _soakStartedMs = millis();
        _soakLastStepMs = millis();
        _soakHeapBefore = ESP.getFreeHeap();
        _soakPsramBefore = ESP.getFreePsram();
        _soakLastStep = "started";
        NUMOS_SERIAL.println(
            "[SOAK] started=1 bounded_iterations=100 autorun=0");
        return true;
    }
    if (equalsIgnoreCase(line, "DEMO SOAK STOP")) {
        _soakActive = false;
        _app.demoReturnToLauncher();
        _soakLastStep = "operator-stop";
        printSoakStatus();
        return true;
    }
    if (equalsIgnoreCase(line, "DEMO SOAK STATUS")) {
        printSoakStatus();
        return true;
    }
    if (confirmedCommand(line, "DEMO DISPLAY SAFE")) {
        _display.restoreSafeProductionDisplayProfile();
        NUMOS_SERIAL.println(
            "[DIAG] display_safe=restored persistence=unchanged");
        return true;
    }
    if (confirmedCommand(line, "DEMO SAFE ON")) {
        requestSafeMode();
        NUMOS_SERIAL.println("[DIAG] safe_mode=requested reboot_required=1");
        return true;
    }
    if (confirmedCommand(line, "DEMO CLEAR SAFE")) {
        clearSafeMode();
        NUMOS_SERIAL.println("[DIAG] safe_mode=cleared reboot_required=1");
        return true;
    }
    if (confirmedCommand(line, "DEMO FACTORY RESET")) {
        if (!_app.filesystemMounted()) {
            NUMOS_SERIAL.println(
                "[DIAG] factory_reset=refused reason=filesystem-unmounted");
            return true;
        }
        bool ok = true;
        for (const char* path : {"/vars.dat", "/vars.tmp", "/vars.bad",
                                 "/settings.dat", "/settings.tmp",
                                 "/settings.bad"}) {
            if (LittleFS.exists(path) && !LittleFS.remove(path)) ok = false;
        }
        vpam::VariableManager::instance().resetAll();
        clearSafeMode();
        _display.restoreSafeProductionDisplayProfile();
        NUMOS_SERIAL.printf(
            "[DIAG] factory_reset=%s scope=demo-state reboot_required=1\n",
            ok ? "ok" : "partial");
        return true;
    }
    if (confirmedCommand(line, "DEMO FS FORMAT")) {
        if (!safeModeActive()) {
            NUMOS_SERIAL.println(
                "[DIAG] fs_format=refused reason=safe-mode-required");
            return true;
        }
        const bool formatted = LittleFS.format();
        NUMOS_SERIAL.printf("[DIAG] fs_format=%s reboot_required=1\n",
                            formatted ? "ok" : "failed");
        return true;
    }
    if (confirmedCommand(line, "DEMO REBOOT")) {
        NUMOS_SERIAL.println("[DIAG] reboot=operator-confirmed");
        NUMOS_SERIAL.flush();
        esp_restart();
        return true;
    }
    if ((line[0] == 'D' || line[0] == 'd') &&
        (line[1] == 'E' || line[1] == 'e') &&
        (line[2] == 'M' || line[2] == 'm') &&
        (line[3] == 'O' || line[3] == 'o')) {
        NUMOS_SERIAL.println(
            "[DIAG] error=unknown-or-unconfirmed command; use DEMO HELP");
        return true;
    }
    return false;
}

void DemoDiagnostics::service() {
    serviceSoak();
    if (!_rawKeypad || !_keyboard.initialized()) return;
    for (uint8_t row = 0; row < 5; ++row) {
        for (uint8_t column = 0; column < 10; ++column) {
            const std::size_t index =
                static_cast<std::size_t>(row) * 10U + column;
            const auto& state = _keyboard.diagnosticState(row, column);
            if (state.raw != _lastRaw[index] ||
                state.debounced != _lastDebounced[index]) {
                NUMOS_SERIAL.printf(
                    "[KEYPAD-RAW] row=%u col=%u raw=%u debounced=%u\n",
                    row, column, state.raw, state.debounced);
                _lastRaw[index] = state.raw;
                _lastDebounced[index] = state.debounced;
            }
        }
    }
}

} // namespace numos::demo

#endif
