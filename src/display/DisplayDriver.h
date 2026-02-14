/**
 * DisplayDriver.h
 * Wrapper for TFT_eSPI to handle the ILI9341 display.
 */

#pragma once

#include <Arduino.h>
// Añade estas líneas antes de incluir TFT_eSPI.h
#ifndef TFT_WIDTH
  #define TFT_WIDTH 240
#endif
#ifndef TFT_HEIGHT
  #define TFT_HEIGHT 320
#endif
#include <TFT_eSPI.h>
#include "../Config.h" // Relative include to src/Config.h

class DisplayDriver {
public:
    DisplayDriver();

    void begin();
    
    // Basic drawing pass-through
    void clear(uint16_t color);
    void setTextColor(uint16_t color, uint16_t bg);
    void setTextColor(uint16_t color);
    void setTextSize(uint8_t size);
    void drawText(int16_t x, int16_t y, const String &text);

    // Smooth Font support (VLW)
    void loadFont(const uint8_t* fontArray);
    void unloadFont();
    
    int width();
    int height();
    
    // Direct access if needed for advanced graphics (graphs)
    TFT_eSPI& tft() { return _tft; }
    // Sprite access for advanced drawing
    TFT_eSprite& sprite() { return _sprite; }

    // Flush back-buffer to screen
    void pushFrame(); 

    // Drawing primitives (redirected to Sprite)
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

private:
    TFT_eSPI _tft;
    TFT_eSprite _sprite;
    bool _useSprite;
};
