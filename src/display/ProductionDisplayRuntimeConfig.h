#pragma once

#include <stdint.h>

/*
 * Production-only TFT_eSPI configuration seam.
 *
 * PlatformIO binds TFT_eSPI's SPI_FREQUENCY and SPI_READ_FREQUENCY macros to
 * these variables for the WROOM-1U target. TFT_eSPI 2.5.43 evaluates those
 * macros whenever it opens an ESP32 SPI transaction, so a validated update is
 * effective on the next transaction without patching or subclassing the
 * library. CAM targets keep their original compile-time integer macros.
 */
extern uint32_t numos_display_write_spi_hz;
extern uint32_t numos_display_read_spi_hz;
