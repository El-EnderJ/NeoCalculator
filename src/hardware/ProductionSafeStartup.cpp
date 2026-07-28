#include "ProductionSafeStartup.h"

#if defined(ARDUINO) && defined(NUMOS_BOARD_PROD_WROOM1U_N16R8) && \
    NUMOS_BOARD_PROD_WROOM1U_N16R8

#include <Arduino.h>
#include "BoardProfile.h"

namespace numos::hardware {

void applyProductionSafeStartup() {
    constexpr auto& display = kProductionBoard.display;

    // WHY: Assert inactive levels before switching to OUTPUT so CS and the
    // transistor-driven backlight cannot glitch during framework startup.
    digitalWrite(display.chipSelect.gpio, HIGH);
    pinMode(display.chipSelect.gpio, OUTPUT);

    digitalWrite(display.backlight.gpio, LOW);
    pinMode(display.backlight.gpio, OUTPUT);

    digitalWrite(display.reset.gpio, HIGH);
    pinMode(display.reset.gpio, OUTPUT);

    // ILI9341 SPI mode 0 idles SCLK low. MOSI/DC remain untouched until the
    // hardware SPI/display driver takes ownership.
    digitalWrite(display.clock.gpio, LOW);
    pinMode(display.clock.gpio, OUTPUT);
}

} // namespace numos::hardware

#endif
