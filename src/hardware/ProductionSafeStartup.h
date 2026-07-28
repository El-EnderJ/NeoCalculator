#pragma once

namespace numos::hardware {

// Apply only the production outputs whose boot-safe level is electrically
// confirmed. Matrix, USB, BOOT, and strap pins are deliberately untouched.
void applyProductionSafeStartup();

} // namespace numos::hardware
