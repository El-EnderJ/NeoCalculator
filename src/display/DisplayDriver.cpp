/**
 * DisplayDriver.cpp
 * Updated for Anti-Flickering using TFT_eSprite.
 */

#include "DisplayDriver.h"
#include "../ui/Theme.h"

DisplayDriver::DisplayDriver() : _tft(), _sprite(&_tft), _useSprite(false) {
}

void DisplayDriver::begin() {
    delay(100); 
    _tft.init();
    _tft.setRotation(1); 
    _tft.fillScreen(TFT_BLACK);

    // Initialize Sprite strategy:
    // User requested "Banded Sprites" to save RAM.
    // We will NOT allocate a full screen sprite here.
    // Individual components will create short-lived sprites.
    _useSprite = false;
    _tft.fillScreen(COLOR_BACKGROUND);
}

void DisplayDriver::pushFrame() {
    if (_useSprite) {
        _sprite.pushSprite(0, 0);
    }
}

void DisplayDriver::clear(uint16_t color) {
    if (_useSprite) _sprite.fillSprite(color);
    else _tft.fillScreen(color);
}

void DisplayDriver::setTextColor(uint16_t color, uint16_t bg) {
    if (_useSprite) _sprite.setTextColor(color, bg);
    else _tft.setTextColor(color, bg);
}

void DisplayDriver::setTextColor(uint16_t color) {
    if (_useSprite) _sprite.setTextColor(color);
    else _tft.setTextColor(color);
}

int DisplayDriver::width() {
    return _tft.width(); 
}

int DisplayDriver::height() {
    return _tft.height();
}

void DisplayDriver::setTextSize(uint8_t size) {
    if (_useSprite) _sprite.setTextSize(size);
    else _tft.setTextSize(size);
}

void DisplayDriver::loadFont(const uint8_t* fontArray) {
    _tft.loadFont(fontArray);
    _sprite.loadFont(fontArray);
}

void DisplayDriver::unloadFont() {
    _tft.unloadFont();
    _sprite.unloadFont();
}

void DisplayDriver::drawText(int16_t x, int16_t y, const String &text) {
    if (_useSprite) {
        _sprite.setCursor(x, y);
        _sprite.print(text);
    } else {
        _tft.setCursor(x, y);
        _tft.print(text);
    }
}

void DisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (_useSprite) _sprite.drawPixel(x, y, color);
    else _tft.drawPixel(x, y, color);
}

void DisplayDriver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (_useSprite) _sprite.drawLine(x0, y0, x1, y1, color);
    else _tft.drawLine(x0, y0, x1, y1, color);
}

void DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (_useSprite) _sprite.fillRect(x, y, w, h, color);
    else _tft.fillRect(x, y, w, h, color);
}

void DisplayDriver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (_useSprite) _sprite.drawRect(x, y, w, h, color);
    else _tft.drawRect(x, y, w, h, color);
}
