// User_Setup.h — Project-specific TFT_eSPI configuration for Calculadora
// This file is intentionally minimal; platformio build_flags also set many defines.

#ifndef USER_SETUP_H
#define USER_SETUP_H

// Driver selection
#define ILI9341_DRIVER

// Pinout (match platformio.ini -DTFT_xxx)
#define TFT_MISO -1
#define TFT_MOSI 13
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   4
#define TFT_RST  5
#define TFT_BL   45

// Display resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// SPI settings (elevated for blocking mode performance)
#define TFT_SPI_MODE 0
#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 40000000

// Pixel order / inversion
#define TFT_RGB_ORDER TFT_BGR
// #define TFT_INVERSION_ON

// ESP32-S3: force FSPI port to avoid REG_SPI_BASE(0)==0 crash
#define USE_FSPI_PORT

#endif // USER_SETUP_H
