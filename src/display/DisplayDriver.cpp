/**
 * DisplayDriver.cpp
 * Driver TFT con bridge para LVGL 9.x.
 *
 * Flujo LVGL:
 *   1. main.cpp llama display.begin() → inicia el panel SPI.
 *   2. main.cpp asigna buffers en PSRAM y llama display.initLvgl().
 *   3. LVGL renderiza regiones "sucias" en el buffer y llama lvglFlushCb().
 *   4. lvglFlushCb() transfiere los píxeles al GRAM via TFT_eSPI::pushColors().
 *
 * Las apps heredadas (CalculationApp, GrapherApp) siguen escribiendo
 * directamente a _display.tft() cuando LVGL está en pausa (ver main.cpp).
 */

#include "DisplayDriver.h"
#include "../ui/Theme.h"

DisplayDriver::DisplayDriver() : _tft(), _sprite(&_tft), _useSprite(false), _lvDisp(nullptr) {
}

void DisplayDriver::begin() {
    Serial.println("[TFT] Initializing...");

    // BL está conectado físicamente a 3.3V → NO tocar GPIO 45.
    // Si lo configuramos como OUTPUT LOW crearíamos un cortocircuito.
    // Dejamos el pin como INPUT (alta impedancia) para no interferir.
    pinMode(TFT_BL, INPUT);

    // Reset físico del panel ILI9341
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH); delay(50);
    digitalWrite(TFT_RST, LOW);  delay(100);
    digitalWrite(TFT_RST, HIGH); delay(200);

    // TFT_eSPI maneja SPI internamente (USE_FSPI_PORT → SPI_PORT=2)
    _tft.init();
    _tft.setRotation(SCREEN_ROTATION);
    _tft.invertDisplay(true);   // Panel IPS: invertir colores

    // Limpiar GRAM con negro puro (0x0000) → elimina ruido IPS residual
    _tft.fillScreen(0x0000);

    // ── DMA ─────────────────────────────────────────────────────────────
    // TFT_eSPI pushPixelsDMA crashea en ESP32-S3 (StoreProhibited en
    // EXCVADDR=0x30).  Dejamos DMA desactivado; pushColors ya usa el
    // FIFO SPI hardware, y a 40 MHz + 32 KB buffers el rendimiento
    // es suficiente para scroll fluido sin tearing.
    _dmaEnabled = false;

    _useSprite = false;
    Serial.println("[TFT] OK");
}

// ═══════════════════════════════════════════════════════════════════════════
// LVGL Bridge
// ═══════════════════════════════════════════════════════════════════════════

void DisplayDriver::initLvgl(void* buf1, void* buf2, uint32_t bufBytes) {
    // Crea el display LVGL con las dimensiones lógicas post-rotación.
    _lvDisp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!_lvDisp) {
        Serial.println("[LVGL] ERROR: lv_display_create() retornó NULL!");
        return;
    }
    Serial.printf("[LVGL] Display creado: %ux%u (ptr=%p)\n",
                  SCREEN_WIDTH, SCREEN_HEIGHT, _lvDisp);

    lv_display_set_user_data(_lvDisp, this);
    lv_display_set_flush_cb(_lvDisp, lvglFlushCb);
    Serial.println("[LVGL] Flush callback registrado");

    // Asigna los buffers de render (ya alojados en PSRAM desde main.cpp).
    // LV_DISPLAY_RENDER_MODE_PARTIAL: solo las "dirty regions" se renderizan.
    lv_display_set_buffers(_lvDisp,
                           buf1, buf2,
                           bufBytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.printf("[LVGL] Buffers asignados: %u bytes cada uno\n", (unsigned)bufBytes);
}

/**
 * lvglFlushCb — Envía una región de píxeles RGB565 al GRAM del panel TFT.
 *
 * Invocado por el sistema de rendering de LVGL al final de cada ciclo de
 * dibujado. Los píxeles están en formato RGB565 (16 bpp) en pxMap.
 * Usamos startWrite/pushColors/endWrite para la transferencia SPI en bloque.
 */
/**
 * lvglFlushCb — Envía píxeles al panel vía DMA no-bloqueante.
 *
 * Flujo con doble buffer:
 *  1. Si hay un DMA anterior en vuelo, esperar a que termine (dmaWait)
 *     y cerrar la transacción SPI previa (endWrite).
 *  2. Abrir nueva transacción (startWrite), fijar ventana GRAM.
 *  3. Lanzar pushPixelsDMA — copia píxeles al buffer DMA interno de
 *     TFT_eSPI (con byte-swap) y arranca la transferencia GDMA.
 *  4. Llamar flush_ready INMEDIATAMENTE: LVGL puede renderizar la
 *     siguiente dirty-area al OTRO buffer mientras el DMA envía éste.
 *
 * Fallback: si DMA no está habilitado, usa pushColors bloqueante.
 */
void DisplayDriver::lvglFlushCb(lv_display_t* disp,
                                const lv_area_t* area,
                                uint8_t* pxMap) {
    DisplayDriver* self = static_cast<DisplayDriver*>(lv_display_get_user_data(disp));
    if (!self) {
        lv_display_flush_ready(disp);
        return;
    }

    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);

    if (self->_dmaEnabled) {
        // ── Ruta DMA no-bloqueante ──────────────────────────────────
        // Completar el DMA anterior (si lo hay)
        if (self->_dmaPending) {
            self->_tft.dmaWait();
            self->_tft.endWrite();
            self->_dmaPending = false;
        }

        self->_tft.startWrite();
        self->_tft.setAddrWindow(area->x1, area->y1, w, h);
        self->_tft.pushPixelsDMA(reinterpret_cast<uint16_t*>(pxMap),
                                 static_cast<uint32_t>(w * h));
        self->_dmaPending = true;

        // LVGL puede usar el otro buffer mientras el DMA corre.
        lv_display_flush_ready(disp);
    } else {
        // ── Ruta bloqueante (fallback) ─────────────────────────────
        self->_tft.startWrite();
        self->_tft.setAddrWindow(area->x1, area->y1, w, h);
        self->_tft.pushColors(reinterpret_cast<uint16_t*>(pxMap),
                              static_cast<uint32_t>(w * h),
                              true);   // swap RGB565 LE→BE
        self->_tft.endWrite();
        lv_display_flush_ready(disp);
    }
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
