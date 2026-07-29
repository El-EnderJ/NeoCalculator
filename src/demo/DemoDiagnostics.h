#pragma once

#include <array>
#include <cstdint>

#include "../Config.h"
#include "../input/KeyCodes.h"

class Keyboard;
class DisplayDriver;
class SystemApp;

namespace numos::demo {

#if NUMOS_PRODUCTION_DEMO_PROFILE
class DemoDiagnostics {
public:
    DemoDiagnostics(Keyboard& keyboard, DisplayDriver& display,
                    SystemApp& app);

    bool handleLine(const char* line);
    void service();
    void setLauncherReadyMs(uint32_t value) { _launcherReadyMs = value; }

private:
    Keyboard& _keyboard;
    DisplayDriver& _display;
    SystemApp& _app;
    bool _rawKeypad = false;
    bool _soakActive = false;
    uint8_t _soakStep = 0;
    uint32_t _soakIterations = 0;
    uint32_t _soakStartedMs = 0;
    uint32_t _soakLastStepMs = 0;
    uint32_t _soakHeapBefore = 0;
    uint32_t _soakPsramBefore = 0;
    const char* _soakLastStep = "idle";
    uint32_t _launcherReadyMs = 0;
    std::array<bool, 50> _lastRaw{};
    std::array<bool, 50> _lastDebounced{};

    void printInfo() const;
    void printHelp() const;
    void printSoakStatus() const;
    void serviceSoak();
    void inject(KeyCode code, KeyAction action = KeyAction::PRESS);
    bool confirmedCommand(const char* line, const char* command) const;
};
#endif

} // namespace numos::demo
