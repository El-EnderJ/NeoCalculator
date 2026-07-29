#include "DemoLiveness.h"

#if NUMOS_PRODUCTION_DEMO_PROFILE && defined(ARDUINO)
#include <Arduino.h>
#endif

namespace numos::demo {

#if NUMOS_PRODUCTION_DEMO_PROFILE
namespace {
bool g_enabled = false;
uint8_t g_giacSuspendDepth = 0;
}

void enableUiLoopWatchdog() {
#if defined(ARDUINO)
    if (!g_enabled) {
        enableLoopWDT();
        g_enabled = true;
    }
#endif
}

void noteUiLoopProgress() {
#if defined(ARDUINO)
    // WHY: this call occurs only after LVGL and SystemApp both returned.
    // Feeding here proves one complete UI/event-loop progression.
    if (g_enabled && g_giacSuspendDepth == 0) feedLoopWDT();
#endif
}

void suspendUiLoopWatchdogForGiac() {
#if defined(ARDUINO)
    if (g_giacSuspendDepth++ == 0 && g_enabled) {
        // The pinned Giac build has cooperative flags but no independently
        // validated timeout driver. Leaving TWDT armed here would reset valid
        // long calculations and would be a dishonest cancellation claim.
        disableLoopWDT();
    }
#endif
}

void resumeUiLoopWatchdogAfterGiac() {
#if defined(ARDUINO)
    if (g_giacSuspendDepth == 0) return;
    --g_giacSuspendDepth;
    if (g_giacSuspendDepth == 0 && g_enabled) {
        enableLoopWDT();
        feedLoopWDT();
    }
#endif
}

bool uiLoopWatchdogEnabled() {
    return g_enabled;
}

bool giacWatchdogCoverageSuspended() {
    return g_giacSuspendDepth != 0;
}
#endif

} // namespace numos::demo
