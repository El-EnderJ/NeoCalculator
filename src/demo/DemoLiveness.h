#pragma once

#include "../Config.h"

namespace numos::demo {

#if NUMOS_PRODUCTION_DEMO_PROFILE
void enableUiLoopWatchdog();
void noteUiLoopProgress();
void suspendUiLoopWatchdogForGiac();
void resumeUiLoopWatchdogAfterGiac();
bool uiLoopWatchdogEnabled();
bool giacWatchdogCoverageSuspended();
#else
inline void enableUiLoopWatchdog() {}
inline void noteUiLoopProgress() {}
inline void suspendUiLoopWatchdogForGiac() {}
inline void resumeUiLoopWatchdogAfterGiac() {}
inline bool uiLoopWatchdogEnabled() { return false; }
inline bool giacWatchdogCoverageSuspended() { return false; }
#endif

} // namespace numos::demo
