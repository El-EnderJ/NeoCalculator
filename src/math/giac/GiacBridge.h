// GiacBridge.h
#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
#endif

// Public API: evaluate a textual expression using Giac and return result as Arduino String.
// Input should be clean text (no trailing '\r' or '\n').
// The implementation already wraps execution in try/catch and returns "Error: ..."
// messages instead of throwing, so callers from LVGL tasks remain protected.
String solveWithGiac(String input);
