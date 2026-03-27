/**
 * DisplayDriver.cpp
 * Driver TFT con bridge para LVGL 9.x.
 */

#include "DisplayDriver.h"
#include "../ui/Theme.h"
#include <esp_heap_caps.h>
#include <cstring>
#include <cstdint>

// Diagnostic and strict-sync helpers — enable during debugging.
#define DISPLAY_DRIVER_DIAG
#define DISPLAY_DRIVER_STRICT_SYNC

DisplayDriver::DisplayDriver() : _tft(), _sprite(&_tft), _useSprite(false), _lvDisp(nullptr) {
}

DisplayDriver::~DisplayDriver() {
    if (_dmaStagingAlloc) {
        heap_caps_free(_dmaStagingAlloc);
        _dmaStagingAlloc = nullptr;
        _dmaStagingBuf = nullptr;
        _dmaStagingBufBytes = 0;
    } else if (_dmaStagingBuf) {
        heap_caps_free(_dmaStagingBuf);
        _dmaStagingBuf = nullptr;
        _dmaStagingBufBytes = 0;
    }
}

void DisplayDriver::begin() {
    Serial.println("[TFT] Initializing...");

    // BL pin: prefer leaving as INPUT initially; LVGL init will drive it HIGH.
    pinMode(TFT_BL, INPUT);

    // Reset physical panel
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH); delay(50);
    digitalWrite(TFT_RST, LOW);  delay(100);
    digitalWrite(TFT_RST, HIGH); delay(200);

    _tft.init();
    _tft.setRotation(SCREEN_ROTATION);
    _tft.invertDisplay(true); // IPS panels

    // Clear GRAM
    _tft.fillScreen(0x0000);

    // By default, don't assume DMA until staging is allocated in initLvgl
    _dmaEnabled = false;

    _useSprite = false;
    Serial.println("[TFT] OK");
}

void DisplayDriver::initLvgl(void* buf1, void* buf2, uint32_t bufBytes) {
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

    lv_display_set_buffers(_lvDisp,
                           buf1, buf2,
                           bufBytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Prepare aligned staging buffer (overallocate and align to 32)
    _dmaStagingBufBytes = bufBytes;
    if (!_dmaStagingAlloc && bufBytes > 0) {
        _dmaStagingAlloc = heap_caps_malloc(_dmaStagingBufBytes + 31,
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (_dmaStagingAlloc) {
            uintptr_t raw = reinterpret_cast<uintptr_t>(_dmaStagingAlloc);
            uintptr_t aligned = (raw + 31) & ~((uintptr_t)31);
            _dmaStagingBuf = reinterpret_cast<uint16_t*>(aligned);
            Serial.printf("[LVGL] DMA staging raw=%p aligned=%p bytes=%u\n",
                          _dmaStagingAlloc, _dmaStagingBuf, (unsigned)_dmaStagingBufBytes);
        } else {
            Serial.println("[LVGL] WARNING: fallo asignacion DMA staging buffer");
        }
    }

    Serial.printf("[DisplayDriver] Staging Buffer: %s\n",
                  _dmaStagingBuf ? "OK" : "FAILED");
    Serial.printf("[DisplayDriver] Staging Buffer bytes=%u (appx %u lines)\n",
                  (unsigned)_dmaStagingBufBytes,
                  (unsigned)(_dmaStagingBufBytes / (SCREEN_WIDTH * 2)));

    _dmaEnabled = (_dmaStagingBuf != nullptr);
    Serial.printf("[DisplayDriver] DMA enabled: %s\n",
                  _dmaEnabled ? "YES" : "NO");

    // Try to initialise the TFT_eSPI DMA engine (do not let SPI driver
    // control CS; we use startWrite()/endWrite() in the flush path).
    if (_dmaStagingBuf) {
        bool dmaInitOk = _tft.initDMA(false);
        Serial.printf("[LVGL] TFT initDMA() returned: %s\n", dmaInitOk ? "OK" : "FAILED/ALREADY");
    }

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    // Set HIGH then toggle LOW briefly to detect active-low backlight hardware.
    digitalWrite(TFT_BL, HIGH);
    delay(50);
    digitalWrite(TFT_BL, LOW);
    delay(50);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("[TFT] Backlight set HIGH (initLvgl) - toggled for diag");
    // Quick hardware self-test: fill white then clear, to verify panel responds.
#ifdef DISPLAY_DRIVER_DIAG
    _tft.fillScreen(0xFFFF); // white
    delay(150);
    _tft.fillScreen(0x0000); // revert to black before LVGL draws
    Serial.println("[TFT] Self-test fill done");
#endif
#endif

    Serial.printf("[LVGL] Buffers asignados: %u bytes cada uno\n", (unsigned)bufBytes);
}

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
    const uint32_t pxCount = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
    uint16_t* src = reinterpret_cast<uint16_t*>(pxMap);

#ifdef DISPLAY_DRIVER_DIAG
    Serial.printf("A area=%d,%d..%d,%d px=%u pxMap=%p dmaPending=%d\n",
                  area->x1, area->y1, area->x2, area->y2, pxCount, pxMap, (int)self->_dmaPending);
#endif

    // If a DMA was pending from the previous flush, wait and close it.
    if (self->_dmaPending) {
#ifdef DISPLAY_DRIVER_DIAG
        Serial.println("B dmaWait(prevPending=1)");
#endif
        self->_tft.dmaWait();
        self->_tft.endWrite();
        self->_dmaPending = false;
#ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
#endif
    }

    self->_tft.startWrite();
    self->_tft.setAddrWindow(area->x1, area->y1, w, h);
#ifdef DISPLAY_DRIVER_DIAG
    Serial.printf("C startWrite win=%d,%d %dx%d\n", area->x1, area->y1, w, h);
#endif

    // PRIORITY: Staging (S) path — copy to our aligned internal buffer and DMA from it.
    if (self->_dmaEnabled && self->_dmaStagingBuf &&
        self->_dmaStagingBufBytes >= pxCount * sizeof(uint16_t)) {
#ifdef DISPLAY_DRIVER_DIAG
        Serial.printf("E staging cp=%u -> %p\n", (unsigned)(pxCount * 2), (void*)self->_dmaStagingBuf);
#endif
        memcpy(self->_dmaStagingBuf, src, pxCount * sizeof(uint16_t));
#ifdef DISPLAY_DRIVER_STRICT_SYNC
        self->_tft.pushPixelsDMA(self->_dmaStagingBuf, pxCount);
        // Wait until DMA finishes before releasing CS
        self->_tft.dmaWait();
        self->_tft.endWrite();
        self->_dmaPending = false;
#ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
#endif
        lv_display_flush_ready(disp);
        return;
#else
        self->_tft.pushPixelsDMA(self->_dmaStagingBuf, pxCount);
        self->_dmaPending = true;
        lv_display_flush_ready(disp);
        return;
#endif
    }

    // If staging isn't available, try direct DMA from pxMap (last resort before CPU copy).
    if (self->_dmaEnabled && esp_ptr_dma_capable(pxMap)) {
#ifdef DISPLAY_DRIVER_DIAG
        Serial.printf("D directDMA src=%p px=%u\n", src, pxCount);
#endif
#ifdef DISPLAY_DRIVER_STRICT_SYNC
        self->_tft.pushPixelsDMA(src, pxCount);
        self->_tft.dmaWait();
        self->_tft.endWrite();
        self->_dmaPending = false;
#ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
#endif
        lv_display_flush_ready(disp);
        return;
#else
        self->_tft.pushPixelsDMA(src, pxCount);
        self->_dmaPending = true;
        lv_display_flush_ready(disp);
        return;
#endif
    }

    // Fallback: blocking CPU transfer
#ifdef DISPLAY_DRIVER_DIAG
    Serial.printf("F blocking px=%u\n", (unsigned)pxCount);
#endif
    self->_tft.pushColors(src, pxCount, true);
    self->_tft.endWrite();
#ifdef DISPLAY_DRIVER_DIAG
    Serial.println("G endWrite");
#endif
    lv_display_flush_ready(disp);
}

void DisplayDriver::pushFrame() {
    if (_useSprite) _sprite.pushSprite(0, 0);
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

int DisplayDriver::width() { return _tft.width(); }
int DisplayDriver::height() { return _tft.height(); }

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
    if (_useSprite) { _sprite.setCursor(x, y); _sprite.print(text); }
    else { _tft.setCursor(x, y); _tft.print(text); }
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
