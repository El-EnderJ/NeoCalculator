#pragma once

#include "ProductionDisplayBootPolicy.h"
#include "ProductionDisplayProfile.h"

namespace numos::display {

// Load a validated explicit record before the TFT owns the SPI bus. Only an
// armed pre-launch panic/watchdog reset counts as a failed display boot.
void prepareProductionDisplayBootProfile();

const ProductionDisplayProfile& activeProductionDisplayProfile();
ProfileLoadDecision productionDisplayLoadDecision();
uint8_t productionDisplayFailureCount();
DisplayResetClass productionDisplayResetClass();

// RAM-only selection. Persistence is deliberately separate and explicit.
bool setActiveProductionDisplayProfile(
    const ProductionDisplayProfile& profile);
void restoreSafeProductionDisplayProfile();

bool saveActiveProductionDisplayProfile();

// Called only once the launcher is usable. It clears the RTC attempt marker;
// NVS is touched only when a prior genuine failure count must be cleared.
void markProductionDisplayBootUsable();

} // namespace numos::display
