/**
 * DisplayDriver.h
 * Wrapper sobre TFT_eSPI con bridge para LVGL 9.x
 *
 * Responsabilidades:
 *  1. Inicializar el panel TFT (ST7789/ILI9341) vía TFT_eSPI.
 *  2. Exponer un flush_cb estático que LVGL invoca para transferir
 *     píxeles renderizados al GRAM de la pantalla.
 *  3. Mantener las primitivas de dibujo heredadas (used by legacy apps).
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif

#ifndef TFT_WIDTH
  #define TFT_WIDTH 240
#endif
#ifndef TFT_HEIGHT
  #define TFT_HEIGHT 320
#endif

#ifdef ARDUINO
#include <TFT_eSPI.h>
#endif
#include <lvgl.h>
#include "../Config.h"

class DisplayDriver {
public:
    DisplayDriver();

    /** Inicializa el panel TFT. Llamar ANTES de lv_init(). */
    void begin();

    // ── LVGL Bridge ─────────────────────────────────────────────────────
    /**
     * Registra el display LVGL.
     * Crea el lv_display_t, asigna los buffers que se le pasan (deben
     * estar ya alojados en PSRAM desde main.cpp) y conecta flush_cb.
     *
     * @param buf1     Puntero al primer buffer de render (PSRAM)
     * @param buf2     Puntero al segundo buffer (doble buffer, PSRAM)
     * @param bufBytes Tamaño en bytes de CADA buffer
     */
    void initLvgl(void* buf1, void* buf2, uint32_t bufBytes);

    /** Callback de flush que LVGL llama para enviar píxeles al GRAM. */
    static void lvglFlushCb(lv_display_t* disp,
                            const lv_area_t* area,
                            uint8_t* pxMap);

    // ── Primitivas heredadas (apps legacy, solo ESP32) ─────────────────
    #ifdef ARDUINO
    void clear(uint16_t color);
    void setTextColor(uint16_t color, uint16_t bg);
    void setTextColor(uint16_t color);
    void setTextSize(uint8_t size);
    void drawText(int16_t x, int16_t y, const String &text);

    void loadFont(const uint8_t* fontArray);
    void unloadFont();

    int width();
    int height();

    TFT_eSPI& tft()         { return _tft;    }
    TFT_eSprite& sprite()   { return _sprite; }

    void pushFrame();

    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    #endif

private:
    #ifdef ARDUINO
    TFT_eSPI    _tft;
    TFT_eSprite _sprite;
    bool        _useSprite;
    #endif

    /** Puntero al display LVGL creado por initLvgl() */
    lv_display_t* _lvDisp = nullptr;
};
