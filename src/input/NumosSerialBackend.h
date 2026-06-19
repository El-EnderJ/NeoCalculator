#pragma once

#ifdef ARDUINO
#include <Arduino.h>

#ifndef NUMOS_SERIAL_BACKEND_USB_CDC
#define NUMOS_SERIAL_BACKEND_USB_CDC 0
#endif

#ifndef NUMOS_SERIAL_BACKEND_UART0
#define NUMOS_SERIAL_BACKEND_UART0 (!NUMOS_SERIAL_BACKEND_USB_CDC)
#endif

#if NUMOS_SERIAL_BACKEND_UART0 && NUMOS_SERIAL_BACKEND_USB_CDC
#error "Select only one NumOS serial backend"
#endif

#if NUMOS_SERIAL_BACKEND_USB_CDC
#define NUMOS_SERIAL Serial
#define NUMOS_SERIAL_BACKEND_LABEL "USB CDC Mode"
#elif NUMOS_SERIAL_BACKEND_UART0
// WHY: On ESP32-S3 Arduino, enabling USB CDC on boot makes Serial the USB
// endpoint and exposes UART0 as Serial0. The CH343 board uses UART0.
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#define NUMOS_SERIAL Serial0
#else
#define NUMOS_SERIAL Serial
#endif
#define NUMOS_SERIAL_BACKEND_LABEL "UART0/CH343 Mode"
#else
#define NUMOS_SERIAL Serial
#define NUMOS_SERIAL_BACKEND_LABEL "Default Serial Mode"
#endif

#endif
