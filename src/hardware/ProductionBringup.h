#pragma once

namespace numos::hardware {

// Bring-up only: wait no more than three seconds for initial CDC enumeration.
// Normal production firmware does not call or compile this wait.
void waitForProductionBringupSerial();

// Emit the bounded report immediately if a host is ready, otherwise arm a
// one-shot late-connection report. Neither function probes the LCD, drives the
// matrix, starts radios, or blocks.
void startProductionBringupReporting();
void serviceProductionBringupReporting();

} // namespace numos::hardware
