/**
 * DisplayDriver.cpp
 * Driver TFT con bridge para LVGL 9.x.
 */

#include "DisplayDriver.h"
#include "../ui/Theme.h"
#include <esp_heap_caps.h>
#include <cstring>
#include <cstdint>
#include <SPI.h>

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
#ifdef TFT_BL
    pinMode(TFT_BL, INPUT);
#else
    (void)0;
#endif

    // Print compile-time pin mapping so user can verify wiring.
#ifdef TFT_MOSI
    Serial.printf("[TFT PINS] MOSI=%d ", (int)TFT_MOSI);
#else
    Serial.print("[TFT PINS] MOSI=<undef> ");
#endif
#ifdef TFT_SCLK
    Serial.printf("SCLK=%d ", (int)TFT_SCLK);
#else
    Serial.print("SCLK=<undef> ");
#endif
#ifdef TFT_CS
    Serial.printf("CS=%d ", (int)TFT_CS);
#else
    Serial.print("CS=<undef> ");
#endif
#ifdef TFT_DC
    Serial.printf("DC=%d ", (int)TFT_DC);
#else
    Serial.print("DC=<undef> ");
#endif
#ifdef TFT_RST
    Serial.printf("RST=%d\n", (int)TFT_RST);
#else
    Serial.println("RST=<undef>");
#endif

    // Manual hardware reset: pulse RST low for 10ms, then high for 150ms
#ifdef TFT_RST
#if TFT_RST >= 0
    pinMode(TFT_RST, OUTPUT);
    Serial.printf("[TFT] Performing manual hardware reset on pin %d\n", (int)TFT_RST);
    // Ensure known high then pulse low briefly
    digitalWrite(TFT_RST, HIGH);
    delay(10);
    digitalWrite(TFT_RST, LOW);
    delay(150); // Aggressive reset: hold low for 150ms
    digitalWrite(TFT_RST, HIGH);
    delay(150); // hold high for 150ms before init
#else
    Serial.println("[TFT] TFT_RST defined as -1; skipping manual reset");
#endif
#else
    Serial.println("[TFT] TFT_RST not defined; skipping manual reset");
#endif

    _tft.init();
    // Reverted to simple init logic that previously worked with display_test_patterns.

#ifdef DISPLAY_DRIVER_DIAG
    // Diagnostic: attempt to read controller ID using available readcommand API.
    {
        uint32_t id = 0;
        id = _tft.readcommand32(0x04); // Try READ_DISPLAY_ID (0x04)
        if (id == 0 || id == 0xFFFFFFFF) {
            id = _tft.readcommand32(0xD3); // Try alternative READ_ID (0xD3)
        }
        Serial.printf("[TFT] readcmd ID = 0x%08X (%u)\n", (unsigned)id, (unsigned)id);
    }
#endif
    _tft.setRotation(SCREEN_ROTATION);
    _tft.invertDisplay(true); // colors inverted

    // Ensure CS and DC pins are correctly configured for manual control
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(TFT_DC, OUTPUT);

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

    // Diagnostic color stress test disabled: colors verified in hardware.
#if 0
    // Diagnostic color stress test: paint red then blue to verify the
    // TFT SPI driver and controller respond to simple blocking writes.
    // This runs after the TFT init (called in begin()) and before LVGL
    // takes ownership of the display pipeline.
    Serial.println("[LVGL] Running red/blue screen test...");
    _tft.invertDisplay(true);
    _tft.fillScreen(TFT_RED);
    delay(1000);
    _tft.fillScreen(TFT_BLUE);
    delay(1000);
    Serial.printf("[LVGL] TFT SPI mode macro value: %d\n", (int)TFT_SPI_MODE);
#endif

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

    // Force DMA off for diagnostic testing
    _dmaEnabled = false;
    Serial.printf("[DisplayDriver] DMA enabled: FORCE %s\n",
                  _dmaEnabled ? "YES" : "NO");

    // Skip TFT DMA initialization for diagnostic testing
    /*
    if (_dmaStagingBuf) {
        bool dmaInitOk = _tft.initDMA(false);
        Serial.printf("[LVGL] TFT initDMA() returned: %s\n", dmaInitOk ? "OK" : "FAILED/ALREADY");
    }
    */

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    // Force hold HIGH unequivocally to prevent blanking
    Serial.println("[TFT] Backlight explicitly set HIGH (initLvgl)");
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
        // Ensure CS is in a known idle state after reclaiming the previous DMA.
        digitalWrite(TFT_CS, HIGH);
    }

        // Force CS LOW for the upcoming transfer and hold it low until we call endWrite().
        digitalWrite(TFT_CS, LOW);

        self->_tft.startWrite();
    self->_tft.setAddrWindow(area->x1, area->y1, w, h);
#ifdef DISPLAY_DRIVER_DIAG
    Serial.printf("C startWrite win=%d,%d %dx%d\n", area->x1, area->y1, w, h);
#endif

        // PRIORITY: Staging (S) path — copy to our aligned internal buffer and write from it (blocking).
        if (self->_dmaEnabled && self->_dmaStagingBuf &&
        self->_dmaStagingBufBytes >= pxCount * sizeof(uint16_t)) {
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.printf("E staging cp=%u -> %p\n", (unsigned)(pxCount * 2), (void*)self->_dmaStagingBuf);
    #endif
    #if 0
        memcpy(self->_dmaStagingBuf, src, pxCount * sizeof(uint16_t));
        // Start DMA-based transfer and keep CS asserted until DMA completes
        Serial.println("[DisplayDriver] Starting DMA pushPixelsDMA...");
        self->_tft.pushPixelsDMA(self->_dmaStagingBuf, pxCount);
        self->_dmaPending = true;
        // Leave CS low and return immediately — dmaWait()/endWrite() will be
        // invoked at the start of the next flush when _dmaPending is detected.
        lv_display_flush_ready(disp);
        return;
    #else
        memcpy(self->_dmaStagingBuf, src, pxCount * sizeof(uint16_t));

        // Force synchronous blocking transfer (CPU) to avoid DMA corruption.
        self->_tft.pushColors(self->_dmaStagingBuf, pxCount, true);
        self->_tft.endWrite();
        // Release forced CS after transfer
        digitalWrite(TFT_CS, HIGH);
        Serial.println("[DisplayDriver] Write complete");
    #endif
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
    #endif
        lv_display_flush_ready(disp);
        return;
        }

    // If staging isn't available, try direct DMA from pxMap (last resort before CPU copy).
        if (self->_dmaEnabled && esp_ptr_dma_capable(pxMap)) {
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.printf("D directDMA src=%p px=%u\n", src, pxCount);
    #endif
    #if 0
        // Start DMA from source buffer (pxMap is DMA-capable per check above)
        Serial.println("[DisplayDriver] Starting DMA pushPixelsDMA(src)...");
        self->_tft.pushPixelsDMA(src, pxCount);
        self->_dmaPending = true;
        lv_display_flush_ready(disp);
        return;
    #else
        // Use blocking write for reliability
        self->_tft.pushColors(src, pxCount, true);
        self->_tft.endWrite();
        // Release forced CS after transfer
        digitalWrite(TFT_CS, HIGH);
        Serial.println("[DisplayDriver] Write complete");
    #endif
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
    #endif
        lv_display_flush_ready(disp);
        return;
        }

    // Fallback: blocking CPU transfer
#ifdef DISPLAY_DRIVER_DIAG
    Serial.printf("F blocking px=%u\n", (unsigned)pxCount);
#endif
#if 0
    // Start DMA from source buffer
    self->_tft.pushPixelsDMA(src, pxCount);
    self->_dmaPending = true;
    lv_display_flush_ready(disp);
    return;
#else
    self->_tft.pushColors(src, pxCount, true);
    self->_tft.endWrite();
    // Release forced CS after transfer
    digitalWrite(TFT_CS, HIGH);
#endif
#ifdef DISPLAY_DRIVER_DIAG
    Serial.println("[DisplayDriver] Write complete");
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
