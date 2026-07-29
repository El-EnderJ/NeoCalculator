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
 * main.cpp  --  NumOS Entry Point (ESP32-S3 N16R8 + LVGL 9.x)
 *
 * Este archivo solo se compila en el entorno ESP32 (framework Arduino).
 * Para simulacion nativa en PC, ver src/hal/NativeHal.cpp.
 */

#ifdef ARDUINO

#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "Config.h"

// ── Global CAS settings ─────────────────────────────────────────────
bool setting_complex_enabled  = true;
int  setting_decimal_precision = 10;
bool setting_edu_steps = false;
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"   // legacy driver — no instanciado; conservado por si acaso
#include "drivers/Keyboard.h"  // nuevo driver 5×10
#include "input/SerialBridge.h"
#include "input/NumosSerialBackend.h"
#include "input/LvglKeypad.h"
#include "SystemApp.h"
#include "ui/SplashScreen.h"
#include "utils/MemProbe.h"

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
#include "hardware/ProductionSafeStartup.h"
#include "display/ProductionDisplayRuntime.h"
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8 && defined(NUMOS_PRODUCTION_BRINGUP)
#include "hardware/ProductionBringup.h"
#endif

#if NUMOS_PRODUCTION_DEMO_PROFILE
#include "demo/DemoBootHealth.h"
#include "demo/DemoDiagnostics.h"
#include "demo/DemoLiveness.h"
#endif

#define Serial NUMOS_SERIAL

#ifdef NUMOS_MATH_STRESS_DIAGNOSTICS
#include "math/MathRenderStressDiagnostics.h"
#endif

#ifdef NUMOS_STIX_DIAGNOSTICS
#include "ui/StixGlyphGallery.h"
#include "fonts/StixMathFont.h"
#endif

// GIAC-A01 engine diagnostics (enable via -DNUMOS_GIAC_DIAGNOSTICS,
// pio run -e esp32s3_n16r8_giacdiag). No-op in normal firmware.
#ifdef NUMOS_GIAC_DIAGNOSTICS
#include "math/giac/GiacDiagnostics.h"
#endif

// GIAC-F01 whole-engine diagnostics (opt-in firmware only).
#ifdef NUMOS_MATH_ENGINE_DIAGNOSTICS
#include "math/giac/MathEngineDiagnostics.h"
#endif

// CAS tests (enable via -DCAS_RUN_TESTS in platformio.ini)
#ifdef CAS_RUN_TESTS
  #include "../tests/CASTest.h"
  #include "../tests/SymExprTest.h"
  #include "../tests/ASTFlatExprTest.h"
  #include "../tests/SymDiffTest.h"
  #include "../tests/OmniSolverTest.h"
  #include "../tests/CalculusStressTest.h"
  #include "../tests/BigIntTest.h"
  #include "../tests/TutorTemplateTest.h"
#endif

// ---- Objetos globales ----
static Keyboard      g_keypad;           // driver 5×10 (Filas=OUTPUT, Cols=INPUT_PULLUP)
static DisplayDriver g_display;
static SystemApp     g_app(g_display, g_keypad);
static SerialBridge  g_serial;
static SplashScreen  g_splash;
#if NUMOS_PRODUCTION_DEMO_PROFILE
static uint32_t g_demoLauncherReadyMs = 0;
static numos::demo::DemoDiagnostics g_demoDiagnostics(
    g_keypad, g_display, g_app);

static bool handleDemoDiagnosticLine(const char* line, void* context) {
    auto* diagnostics =
        static_cast<numos::demo::DemoDiagnostics*>(context);
    return diagnostics && diagnostics->handleLine(line);
}
#endif

// LVGL gating flag
bool g_lvglActive = true;

// Draw-buffers
static void* lvBuf1 = nullptr;
static void* lvBuf2 = nullptr;

// ====================================================================
// setup()
// ====================================================================
void setup() {
#if NUMOS_PRODUCTION_DEMO_PROFILE
    const uint32_t demoSetupEntryMs = millis();
#endif
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // Earliest framework-controlled point: CS high, BL off, reset inactive,
    // mode-0 SCLK idle. Matrix, USB, BOOT, and strap pins remain untouched.
    numos::hardware::applyProductionSafeStartup();
#endif

#if NUMOS_PRODUCTION_DEMO_PROFILE
    numos::demo::initializeBootHealth();
#endif

    Serial.begin(115200);
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // Normal production boot never waits for a USB host. Only the explicitly
    // selected bring-up build gets a bounded enumeration window; its report
    // service also handles a host that connects after setup() has completed.
#if defined(NUMOS_PRODUCTION_BRINGUP)
    numos::hardware::waitForProductionBringupSerial();
#endif
#else
    // Preserve the separately validated CAM UART/USB startup contract exactly.
#if NUMOS_SERIAL_BACKEND_USB_CDC
    const uint32_t serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart) < 3000U) {
        delay(10);
    }
#else
    delay(50);  // UART bridge reset margin when Serial is routed to UART0.
#endif
#endif

    Serial.print("\n>>> NumOS: System Ready (");
    Serial.print(NUMOS_SERIAL_BACKEND_LABEL);
    Serial.println(")");
    Serial.println("=== NumOS Boot ===");
#if NUMOS_PRODUCTION_DEMO_PROFILE
    {
        const auto& health = numos::demo::bootHealthRecord();
        Serial.printf(
            "[BOOT] profile=demo commit=%s reset=%s failures=%u safe=%u\n",
            NUMOS_BUILD_COMMIT,
            numos::demo::resetClassName(health.lastReset),
            health.consecutiveFailures,
            numos::demo::safeModeActive());
    }
#endif

    // -- CAS-Lite Phase A Tests (if enabled) --
#ifdef CAS_RUN_TESTS
    cas::runCASTests();
    cas::runSymExprTests();
    cas::runASTFlatExprTests();
    cas::runSymDiffTests();
    cas::runOmniSolverTests();
    cas::runCalculusStressTest();
    cas::runBigIntTests();
    cas::runTutorTests();
#endif

    // -- GIAC-A01 engine diagnostics (opt-in build only) --
#ifdef NUMOS_GIAC_DIAGNOSTICS
    numos::runGiacDiagnostics();
#endif

#ifdef NUMOS_MATH_ENGINE_DIAGNOSTICS
    numos::runMathEngineDiagnostics();
#endif

    // -- 1. PSRAM --
    if (psramFound()) {
        Serial.printf("[PSRAM] %u KB libres\n",
                      (unsigned)(ESP.getFreePsram() / 1024));
    } else {
        Serial.println("[PSRAM] NO DETECTADA!");
    }

#if NUMOS_BOARD_PROD_WROOM1U_N16R8 && defined(NUMOS_PRODUCTION_BRINGUP)
    numos::hardware::startProductionBringupReporting();
#endif

    // -- 2. TFT --
    g_display.begin();

    // -- 3. LVGL init --
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // -- 4. Draw buffer — attempt DOUBLE 32 KB internal DMA buffers --
    // CRITICAL constraints (ESP32-S3 + TFT_eSPI):
    //  • MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA  →  internal SRAM, DMA-capable.
    //  • PSRAM cannot be used: OPI-PSRAM is on a separate SPI bus; TFT_eSPI
    //    SPI-DMA on ESP32-S3 cannot source from PSRAM → StoreProhibited crash.
    //  • Single buffer only: double-buffer triggers LVGL 9.x pipelining
    //    deadlock (waits for a DMA-done ISR that never fires in blocking mode).
    //  32 KB = 51.2 lines/strip → ~half the flush calls vs 16 KB 25.6 lines.
    static constexpr uint32_t BUF_BYTES = 32U * 1024U; // 32768 bytes ≈ 51 lines

    // Use single-buffer mode for isolation testing (staging DMA)
    lvBuf1 = heap_caps_malloc(BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lvBuf2 = nullptr;

    if (!lvBuf1) {
        Serial.println("[BOOT] BUFFER FAIL! HALT.");
        while (1) delay(1000);
    }
    Serial.printf("[BOOT] Draw buffer: %u bytes (single internal DMA, lvBuf1=%p, ~%u lines)\n",
                  (unsigned)BUF_BYTES, lvBuf1,
                  (unsigned)(BUF_BYTES / (320 * 2)));

    // -- 5. Display LVGL --
    g_display.initLvgl(lvBuf1, lvBuf2, BUF_BYTES);

#if NUMOS_BOARD_PROD_WROOM1U_N16R8 && \
    defined(NUMOS_PRODUCTION_BRINGUP_DISPLAY_AUTORUN)
    // Never enabled by either checked-in production environment. This bounded
    // seam is available for an explicitly instrumented first-board build.
    g_display.runBoundedProductionDisplayDiagnostic();
#endif

    if (!lv_display_get_default()) {
        Serial.println("[BOOT] NO DISPLAY! HALT.");
        while (1) delay(1000);
    }

    // -- 5b. LvglKeypad input device (DEBE ir DESPUÉS del display) --
    // En LVGL 9.x, lv_indev_create() asocia el indev al display por defecto.
    // Si se crea antes del display, readCb nunca se invoca.
    LvglKeypad::init();
    Serial.println("[LVGL] LvglKeypad indev creado (post-display)");

#ifdef NUMOS_MATH_STRESS_DIAGNOSTICS
    vpam::runMathRenderStressDiagnostics();
#endif

    // -- 6. SplashScreen con animacion fade-in --
#if !NUMOS_PRODUCTION_DEMO_PROFILE
    volatile bool splashDone = false;
    g_splash.create();
    g_splash.show([&splashDone]() { splashDone = true; });

    // Pump LVGL mientras dura la animacion del splash
    while (!splashDone) {
        lv_timer_handler();
        delay(5);
    }
    // Breve pausa extra para que se aprecie el splash completo
    uint32_t holdEnd = millis() + 800;
    while (millis() < holdEnd) {
        lv_timer_handler();
        delay(5);
    }
#else
    Serial.println("[BOOT] demo splash=skipped");
#endif

    #ifdef NUMOS_STIX_DIAGNOSTICS
    // -- 6b. STIX Two Math validation (glyph coverage + baseline check) --
    const bool stixDiagOk = ui::runStixGlyphAlignmentDiagnostics(&stix_math_18);
    Serial.printf("[STIX] Alignment diagnostics: %s\n", stixDiagOk ? "PASS" : "WARN");

    // Show the required glyph gallery briefly on real hardware.
    ui::showStixGlyphGallery(1800);
    #endif

    // -- 7. SystemApp (carga launcher, LittleFS, etc.) --
    g_app.begin();

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // Deliberate GPIO ownership point. Safe startup leaves every matrix pin
    // untouched until the complete generated mapping is compiled.
    g_keypad.begin();
#endif

    // -- 7b. Splash teardown (MT-03) --
    // Lifetime order: g_app.begin() already called _mainMenu.load(), which
    // starts a 200 ms FADE_IN screen animation (MainMenu.cpp). While that
    // animation runs, the splash screen is still referenced by the render
    // pipeline — deleting it now would reproduce the active-screen
    // use-after-free hang that forced deferred app teardown (SystemApp.cpp).
    // So: pump LVGL past the fade first, THEN delete. Reclaims the splash's
    // ~1 KB (+3 objects) from the fixed 64 KB LVGL pool (audit §4.1).
    {
        const uint32_t fadeEnd = millis() + 250;   // > 200 ms menu fade
        while (millis() < fadeEnd) {
            lv_timer_handler();
            delay(5);
        }
    }
#if !NUMOS_PRODUCTION_DEMO_PROFILE
    g_splash.destroy();
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // A saved profile is considered bootable only after the launcher has
    // rendered and the splash/transition lifetime is complete.
    numos::display::markProductionDisplayBootUsable();
#endif

#if NUMOS_PRODUCTION_DEMO_PROFILE
    lv_timer_handler();
    g_demoLauncherReadyMs = millis() - demoSetupEntryMs;
    g_demoDiagnostics.setLauncherReadyMs(g_demoLauncherReadyMs);
    numos::demo::acknowledgeLauncherReady();
    Serial.printf("[BOOT] launcher-usable-ms=%u\n",
                  (unsigned)g_demoLauncherReadyMs);
#endif

    // -- 8. Serial bridge (teclado via monitor serial) --
    g_serial.begin();
#if NUMOS_PRODUCTION_DEMO_PROFILE
    g_serial.setLineHandler(handleDemoDiagnosticLine, &g_demoDiagnostics);
#endif

    // -- 9. Confirmar foco LVGL --
    lv_group_t* focusGrp = lv_indev_get_group(LvglKeypad::indev());
    lv_obj_t*   focused  = focusGrp ? lv_group_get_focused(focusGrp) : nullptr;
    Serial.printf("[GUI] Focus assigned to: %s (group=%p, obj=%p)\n",
                  focused ? "MainMenu card" : "NONE",
                  (void*)focusGrp, (void*)focused);

    Serial.println("[BOOT] OK — Use w/a/s/d to navigate, Enter=EXE, c=AC");
    Serial.println("[BOOT] Deep sleep DISABLED (serial monitor mode)");

    // MT-01: boot steady-state probe (menu loaded, splash freed, apps constructed).
    NUMOS_MEM_PROBE("boot");

#if NUMOS_PRODUCTION_DEMO_PROFILE
    numos::demo::enableUiLoopWatchdog();
#endif
}

// ====================================================================
// loop()
// ====================================================================

// Heartbeat: imprime un '.' cada 5 segundos para confirmar que la S3 vive
static unsigned long _lastHeartbeat = 0;

void loop() {
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    // WHY: the production scanner is serviced before any potentially long
    // LVGL, serial, or CAS work. A row-select phase yields immediately, and
    // the next loop iteration samples and restores that row before other
    // subsystems run. Giac stalls therefore pause scanning with every row
    // inactive instead of stretching an active-low phase.
    g_keypad.update();
    if (g_keypad.rowSelected()) {
        return;
    }
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8 && defined(NUMOS_PRODUCTION_BRINGUP)
    // O(1), non-blocking readiness check. If CDC was absent during setup(),
    // the bounded report is emitted once when a host later enumerates.
    numos::hardware::serviceProductionBringupReporting();
#endif

    if (g_lvglActive) {
        lv_timer_handler();
    }

    g_app.update();

#if NUMOS_PRODUCTION_DEMO_PROFILE
    g_demoDiagnostics.service();
#endif

#if NUMOS_BOARD_PROD_WROOM1U_N16R8 && defined(NUMOS_PRODUCTION_BRINGUP)
    numos::hardware::serviceProductionBringupCommands(
        g_keypad, g_display, g_app);
#endif

    KeyEvent serialEv;
    while (g_serial.pollEvent(serialEv)) {
        g_app.injectKey(serialEv);
    }

#if NUMOS_PRODUCTION_DEMO_PROFILE
    numos::demo::noteUiLoopProgress();
#endif

    // Heartbeat cada 5s (confirma que el loop corre y Serial TX funciona).
    // MT-01: the old internal-only "[HB] heap=" line is replaced by the full
    // memory probe (internal + PSRAM + largest block + LVGL pool + stack HW).
    if (millis() - _lastHeartbeat > 5000) {
        _lastHeartbeat = millis();
#if NUMOS_MEM_PROBE_ENABLE
        char hbTag[24];
        snprintf(hbTag, sizeof(hbTag), "hb %lus", millis() / 1000);
        NUMOS_MEM_PROBE(hbTag);
#else
        Serial.printf("[HB] %lus uptime | heap=%u\n",
                      millis() / 1000, (unsigned)ESP.getFreeHeap());
#endif
    }

#if !NUMOS_BOARD_PROD_WROOM1U_N16R8
    // The CAM driver retains its historical polling cadence. Production uses
    // the timestamped phased scanner and must revisit it without delay().
    delay(KEY_SCAN_INTERVAL_MS);
#endif
}

#endif // ARDUINO

