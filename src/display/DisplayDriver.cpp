/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
#include "ProductionDisplayClip.h"
#include "ProductionDisplayRuntime.h"
#include "ProductionDisplayRuntimeConfig.h"
#endif

// Diagnostic and strict-sync helpers — enable during debugging.
// Uncomment to enable verbose display diagnostics during development.
// #define DISPLAY_DRIVER_DIAG
#define DISPLAY_DRIVER_STRICT_SYNC

DisplayDriver::DisplayDriver() : _tft(), _sprite(&_tft), _useSprite(false), _lvDisp(nullptr) {
}

DisplayDriver::~DisplayDriver() {
    if (_dmaStagingAlloc) {
        heap_caps_free(_dmaStagingAlloc);
        _dmaStagingAlloc = nullptr;
    }
    _dmaStagingBuf = nullptr;
    _dmaStagingBufBytes = 0;
}

void DisplayDriver::begin() {
    Serial.println("[TFT] Initializing...");

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    numos::display::prepareProductionDisplayBootProfile();
    const auto& productionProfile =
        numos::display::activeProductionDisplayProfile();
    Serial.printf("[TFT] profile=%s source=%s write=%u read=%u\n",
                  numos::display::profileIdentifier(
                      productionProfile.identifier),
                  numos::display::profileLoadDecisionName(
                      numos::display::productionDisplayLoadDecision()),
                  productionProfile.writeSpiHz,
                  productionProfile.readSpiHz);
#endif

    // Production BL is an active-high NPN drive and must never float or flash.
#ifdef TFT_BL
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    digitalWrite(TFT_BL, LOW);
    pinMode(TFT_BL, OUTPUT);
#else
    // Existing CAM behavior remains unchanged pending production validation.
    pinMode(TFT_BL, INPUT);
#endif
#else
    (void)0;
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // WHY: Reassert the early safe states at the display ownership boundary.
    digitalWrite(TFT_CS, HIGH);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_SCLK, LOW);
    pinMode(TFT_SCLK, OUTPUT);
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

    // Manual hardware reset after the safe-startup inactive-high interval.
#ifdef TFT_RST
#if TFT_RST >= 0
    pinMode(TFT_RST, OUTPUT);
    Serial.printf("[TFT] Performing manual hardware reset on pin %d\n", (int)TFT_RST);
    // Ensure known high then pulse low briefly
    digitalWrite(TFT_RST, HIGH);
    delay(10);
    digitalWrite(TFT_RST, LOW);
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    delay(productionProfile.resetLowMs);
    digitalWrite(TFT_RST, HIGH);
    delay(productionProfile.resetRecoveryMs);
#else
    delay(150); // Aggressive reset: hold low for 150ms
    digitalWrite(TFT_RST, HIGH);
    delay(150); // hold high for 150ms before init
#endif
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
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    (void)configureProductionController(productionProfile);
#else
    _tft.setRotation(SCREEN_ROTATION);
    _tft.invertDisplay(true); // colors inverted
#endif

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

#if defined(NUMOS_DISPLAY_PERF_TELEMETRY)
    lv_display_add_event_cb(
        _lvDisp, lvglPerfEventCb, LV_EVENT_ALL, this);
    Serial.printf("[DISPLAY-PERF] telemetry=on refresh_period_ms=%u window_ms=2000\n",
                  static_cast<unsigned>(LV_DEF_REFR_PERIOD));
#endif

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
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // The GRAM is black before the bounded PWM level is enabled. This avoids a
    // full-brightness flash while retaining enough light for first bring-up.
    const auto& profile =
        numos::display::activeProductionDisplayProfile();
    setBacklightLevel(profile.initialBacklight);
    Serial.printf("[TFT] Production backlight level=%u/%u\n",
                  (unsigned)profile.initialBacklight,
                  (unsigned)profile.maximumBacklight);
#else
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
#endif

    Serial.printf("[LVGL] Buffers asignados: %u bytes cada uno\n", (unsigned)bufBytes);
}

#if NUMOS_BOARD_PROD_WROOM1U_N16R8

void DisplayDriver::forceBacklightOff() {
    analogWrite(TFT_BL, 0);
    digitalWrite(TFT_BL, LOW);
    pinMode(TFT_BL, OUTPUT);
    _backlightLevel = 0;
}

void DisplayDriver::setBacklightLevel(uint8_t level) {
    const uint8_t maximum =
        numos::display::activeProductionDisplayProfile().maximumBacklight;
    const uint8_t bounded =
        level > maximum ? maximum : level;
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, bounded);
    _backlightLevel = bounded;
}

void DisplayDriver::resetProductionController(
    const numos::display::ProductionDisplayProfile& profile) {
    digitalWrite(TFT_RST, HIGH);
    pinMode(TFT_RST, OUTPUT);
    delay(10);
    digitalWrite(TFT_RST, LOW);
    delay(profile.resetLowMs);
    digitalWrite(TFT_RST, HIGH);
    delay(profile.resetRecoveryMs);
    _tft.init();
}

bool DisplayDriver::configureProductionController(
    const numos::display::ProductionDisplayProfile& profile) {
    // WHY: rotation is the sole axis/geometry authority. TFT_eSPI first writes
    // its rotation and updates its private logical dimensions. NumOS then
    // applies offsets and writes a final MADCTL derived only from that rotation
    // plus the independently validated color-order bit.
    _tft.setRotation(profile.rotation);
    const numos::display::DisplayGeometry geometry =
        numos::display::logicalDisplayGeometry(profile.rotation);
    if (_tft.width() != geometry.width ||
        _tft.height() != geometry.height) {
        return false;
    }
    _xOffset = profile.xOffset;
    _yOffset = profile.yOffset;
    const uint8_t madctl = numos::display::displayMadctl(profile);
    _tft.startWrite();
    _tft.writecommand(0x36);
    _tft.writedata(madctl);
    _tft.endWrite();
    _tft.invertDisplay(profile.inverted);
    return true;
}

void DisplayDriver::invalidateLvglFrame() {
    if (_lvDisp != nullptr && lv_screen_active() != nullptr) {
        lv_obj_invalidate(lv_screen_active());
    }
}

bool DisplayDriver::applyProductionDisplayProfile(
    const numos::display::ProductionDisplayProfile& profile,
    const bool resetController) {
    if (numos::display::validateDisplayProfile(profile) !=
        numos::display::ProfileValidation::Ok) {
        return false;
    }
    forceBacklightOff();
    if (!numos::display::setActiveProductionDisplayProfile(profile)) {
        return false;
    }
    if (resetController) {
        resetProductionController(profile);
    }
    if (!configureProductionController(profile)) return false;
    _tft.fillScreen(TFT_BLACK);
    setBacklightLevel(profile.initialBacklight);
    invalidateLvglFrame();
    return true;
}

void DisplayDriver::restoreSafeProductionDisplayProfile() {
    numos::display::restoreSafeProductionDisplayProfile();
    (void)applyProductionDisplayProfile(
        numos::display::kSafeDisplayProfile, true);
}

void DisplayDriver::runBoundedProductionDisplayDiagnostic() {
    // Explicit bring-up entry point only; no normal boot path calls it. All
    // loops are fixed by the 320x240 production geometry and allocate no heap.
    const auto& profile =
        numos::display::activeProductionDisplayProfile();
    const auto tx = [&](const int16_t x) {
        return static_cast<int16_t>(x + _xOffset);
    };
    const auto ty = [&](const int16_t y) {
        return static_cast<int16_t>(y + _yOffset);
    };

    forceBacklightOff();
    _tft.fillScreen(TFT_BLACK);
    delay(150);

    constexpr uint8_t levels[] = {
        0,
        32,
        96,
        numos::display::kMaximumBacklight
    };
    constexpr const char* levelNames[] = {
        "off", "low", "normal", "bounded-high"
    };
    std::size_t levelIndex = 0;
    for (const uint8_t level : levels) {
        const uint8_t bounded = level > profile.maximumBacklight
            ? profile.maximumBacklight : level;
        Serial.printf("[DISPLAY] TEST backlight=%s level=%u\n",
                      levelNames[levelIndex++], bounded);
        setBacklightLevel(bounded);
        delay(200);
    }

    constexpr uint16_t colors[] = {
        TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE
    };
    for (const uint16_t color : colors) {
        _tft.fillScreen(color);
        delay(350);
    }

    // Color order, one-pixel physical edges, labelled corners/directions, grid.
    _tft.fillScreen(TFT_BLACK);
    _tft.drawRect(tx(0), ty(0), SCREEN_WIDTH, SCREEN_HEIGHT, TFT_WHITE);
    for (int16_t x = 0; x < SCREEN_WIDTH; x += 40) {
        _tft.drawFastVLine(tx(x), ty(0), SCREEN_HEIGHT, TFT_DARKGREY);
    }
    for (int16_t y = 0; y < SCREEN_HEIGHT; y += 40) {
        _tft.drawFastHLine(tx(0), ty(y), SCREEN_WIDTH, TFT_DARKGREY);
    }
    _tft.fillRect(tx(110), ty(42), 32, 24, TFT_RED);
    _tft.fillRect(tx(144), ty(42), 32, 24, TFT_GREEN);
    _tft.fillRect(tx(178), ty(42), 32, 24, TFT_BLUE);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("TL (0,0)", tx(2), ty(2), 1);
    _tft.setTextDatum(TR_DATUM);
    _tft.drawString("TR (319,0)", tx(317), ty(2), 1);
    _tft.setTextDatum(BL_DATUM);
    _tft.drawString("BL (0,239)", tx(2), ty(237), 1);
    _tft.setTextDatum(BR_DATUM);
    _tft.drawString("BR (319,239)", tx(317), ty(237), 1);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString("TOP", tx(160), ty(12), 2);
    _tft.setTextDatum(BC_DATUM);
    _tft.drawString("BOTTOM", tx(160), ty(228), 2);
    _tft.setTextDatum(ML_DATUM);
    _tft.drawString("LEFT", tx(3), ty(120), 2);
    _tft.setTextDatum(MR_DATUM);
    _tft.drawString("RIGHT", tx(317), ty(120), 2);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("R", tx(122), ty(69), 1);
    _tft.drawString("G", tx(156), ty(69), 1);
    _tft.drawString("B", tx(190), ty(69), 1);
    delay(1400);

    // Independent horizontal and vertical RGB565 gradients.
    _tft.fillScreen(TFT_BLACK);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.drawString("HORIZONTAL GRADIENT", tx(6), ty(4), 1);
    for (int16_t x = 0; x < SCREEN_WIDTH; ++x) {
        const uint8_t red = static_cast<uint8_t>(
            (x * 31) / (SCREEN_WIDTH - 1));
        const uint8_t blue = static_cast<uint8_t>(31 - red);
        const uint16_t color =
            static_cast<uint16_t>((red << 11U) | blue);
        _tft.drawFastVLine(tx(x), ty(18), 82, color);
    }
    _tft.drawString("VERTICAL GRADIENT", tx(6), ty(106), 1);
    for (int16_t y = 0; y < 120; ++y) {
        const uint8_t green = static_cast<uint8_t>((y * 63) / 119);
        const uint16_t color = static_cast<uint16_t>(green << 5U);
        _tft.drawFastHLine(tx(0), ty(120 + y), SCREEN_WIDTH, color);
    }
    delay(1400);

    // Baseline rules and the smallest loaded bitmap glyphs.
    _tft.fillScreen(TFT_BLACK);
    _tft.setTextDatum(BL_DATUM);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    constexpr int16_t baselines[] = {40, 78, 116, 154, 192, 224};
    for (const int16_t baseline : baselines) {
        _tft.drawFastHLine(tx(4), ty(baseline), 312, TFT_CYAN);
    }
    _tft.drawString("Baseline 40: Agjpq 0123456789", tx(6), ty(40), 1);
    _tft.drawString("Baseline 78: +-*/=()[]{}", tx(6), ty(78), 2);
    _tft.drawString("Baseline 116: RGB rgb 1px", tx(6), ty(116), 1);
    _tft.drawString("Baseline 154: small glyphs .,:;!|", tx(6), ty(154), 1);
    _tft.drawString("Baseline 192: XYZ xyz", tx(6), ty(192), 2);
    _tft.drawString("Baseline 224: NumOS Z320IT008-D", tx(6), ty(224), 1);
    delay(1400);

    _tft.fillScreen(TFT_BLACK);
    setBacklightLevel(profile.initialBacklight);
    invalidateLvglFrame();
}

#endif

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

#if defined(NUMOS_DISPLAY_PERF_TELEMETRY)
    const uint32_t perfFlushStartUs = micros();
    const auto finishFlush = [self, disp, pxCount, perfFlushStartUs]() {
        self->recordDisplayPerfFlush(pxCount, perfFlushStartUs);
        lv_display_flush_ready(disp);
    };
#else
    const auto finishFlush = [disp]() { lv_display_flush_ready(disp); };
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    if (self->_xOffset != 0 || self->_yOffset != 0) {
        const numos::display::ClippedFlushPlan plan =
            numos::display::makeClippedFlushPlan(
                {area->x1, area->y1, area->x2, area->y2},
                self->_xOffset, self->_yOffset,
                SCREEN_WIDTH, SCREEN_HEIGHT);
        numos::display::executeClippedFlush(
            plan,
            src,
            [self](const int32_t x, const int32_t y,
                   const uint32_t width, uint16_t* const rowSource) {
                self->_tft.startWrite();
                self->_tft.setAddrWindow(x, y, width, 1);
                self->_tft.pushColors(rowSource, width, true);
                self->_tft.endWrite();
            },
            [finishFlush]() {
                digitalWrite(TFT_CS, HIGH);
                finishFlush();
            });
        return;
    }
#endif

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
        finishFlush();
        return;
    #else
        memcpy(self->_dmaStagingBuf, src, pxCount * sizeof(uint16_t));

        // Force synchronous blocking transfer (CPU) to avoid DMA corruption.
        self->_tft.pushColors(self->_dmaStagingBuf, pxCount, true);
        self->_tft.endWrite();
        // Release forced CS after transfer
        digitalWrite(TFT_CS, HIGH);
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.println("[DisplayDriver] Write complete");
    #endif
    #endif
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
    #endif
        finishFlush();
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
        finishFlush();
        return;
    #else
        // Use blocking write for reliability
        self->_tft.pushColors(src, pxCount, true);
        self->_tft.endWrite();
        // Release forced CS after transfer
        digitalWrite(TFT_CS, HIGH);
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.println("[DisplayDriver] Write complete");
    #endif
    #endif
    #ifdef DISPLAY_DRIVER_DIAG
        Serial.println("G endWrite");
    #endif
        finishFlush();
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
    finishFlush();
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
    finishFlush();
}

#if defined(NUMOS_DISPLAY_PERF_TELEMETRY)
void DisplayDriver::recordDisplayPerfFlush(const uint32_t pixelCount,
                                           const uint32_t startUs) {
    const uint32_t elapsedUs = micros() - startUs;
    ++_perfFlushCount;
    _perfPixelCount += pixelCount;
    _perfFlushTotalUs += elapsedUs;
    if (elapsedUs > _perfFlushMaxUs) _perfFlushMaxUs = elapsedUs;
}

void DisplayDriver::lvglPerfEventCb(lv_event_t* event) {
    auto* self = static_cast<DisplayDriver*>(lv_event_get_user_data(event));
    if (!self) return;

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_REFR_START) {
        self->_perfRefreshStartUs = micros();
        if (self->_perfWindowStartUs == 0) {
            self->_perfWindowStartUs = self->_perfRefreshStartUs;
        }
        return;
    }
    if (code != LV_EVENT_REFR_READY || self->_perfRefreshStartUs == 0) return;

    const uint32_t nowUs = micros();
    const uint32_t refreshUs = nowUs - self->_perfRefreshStartUs;
    self->_perfRefreshStartUs = 0;
    ++self->_perfFrameCount;
    self->_perfRefreshTotalUs += refreshUs;
    if (refreshUs > self->_perfRefreshMaxUs) {
        self->_perfRefreshMaxUs = refreshUs;
    }

    const uint32_t windowUs = nowUs - self->_perfWindowStartUs;
    if (windowUs < 2'000'000U) return;

    const uint32_t frames = self->_perfFrameCount;
    const uint32_t flushes = self->_perfFlushCount;
    const uint32_t fpsTimesTen = windowUs == 0
        ? 0
        : static_cast<uint32_t>(
              (static_cast<uint64_t>(frames) * 10'000'000ULL) / windowUs);
    Serial.printf(
        "[DISPLAY-PERF] fps_x10=%u frames=%u refresh_us=%u/%u "
        "flush_us=%u/%u flushes_per_frame_x10=%u pixels_per_frame=%u\n",
        static_cast<unsigned>(fpsTimesTen),
        static_cast<unsigned>(frames),
        static_cast<unsigned>(frames ? self->_perfRefreshTotalUs / frames : 0),
        static_cast<unsigned>(self->_perfRefreshMaxUs),
        static_cast<unsigned>(flushes ? self->_perfFlushTotalUs / flushes : 0),
        static_cast<unsigned>(self->_perfFlushMaxUs),
        static_cast<unsigned>(frames ? (flushes * 10U) / frames : 0),
        static_cast<unsigned>(frames ? self->_perfPixelCount / frames : 0));

    self->_perfWindowStartUs = nowUs;
    self->_perfFrameCount = 0;
    self->_perfFlushCount = 0;
    self->_perfPixelCount = 0;
    self->_perfRefreshTotalUs = 0;
    self->_perfRefreshMaxUs = 0;
    self->_perfFlushTotalUs = 0;
    self->_perfFlushMaxUs = 0;
}
#endif

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
