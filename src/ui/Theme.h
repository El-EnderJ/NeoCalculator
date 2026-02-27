/**
 * Theme.h
 * NumWorks-inspired color palette and styling.
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include <cstdint>
#endif

// Colors (RGB565)
// White/Cream background
static const uint16_t COLOR_BACKGROUND = 0xFFFF; // Pure White 
// static const uint16_t COLOR_BACKGROUND = 0xFFFE; // Slightly off-white if needed

// NumWorks Orange/Gold
// Hex: #FFB531 -> RGB565 approx
static const uint16_t COLOR_PRIMARY = 0xFD20; 

// Top Bar / Header Background usually same as Primary or distinct
static const uint16_t COLOR_HEADER_BG = COLOR_PRIMARY; 
static const uint16_t COLOR_HEADER_TEXT = 0x0000;

// Text Colors
static const uint16_t COLOR_TEXT = 0x0000; // Black
static const uint16_t COLOR_TEXT_WHITE = 0xFFFF; // For dark backgrounds
static const uint16_t COLOR_TEXT_LIGHT = 0x9CD3; // Light Grey
static const uint16_t COLOR_TEXT_DARK = 0x3333; // Dark Grey

// Accents
static const uint16_t COLOR_ACCENT = 0xE71C; // Light grey for borders/grid
static const uint16_t COLOR_SELECTION = 0xDEDB; // Light grey selection

// Fonts
// Using built-in GLCD/Fonts for now, aliases for logic
// 1 = GLCD (Tiny), 2 = Small, 4 = Medium, 6 = Large Number
static const uint8_t FONT_SMALL = 1;
static const uint8_t FONT_REGULAR = 2; // Or 2
static const uint8_t FONT_HEADER = 2;  // Or 4
static const uint8_t FONT_LARGE = 4;
