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
#include "input/LvglKeypad.h"
#include "SystemApp.h"
#include "ui/SplashScreen.h"

// CAS tests (enable via -DCAS_RUN_TESTS in platformio.ini)
#ifdef CAS_RUN_TESTS
  #include "../tests/CASTest.h"
  #include "../tests/SymExprTest.h"
  #include "../tests/ASTFlatExprTest.h"
  #include "../tests/SymDiffTest.h"
  #include "../tests/OmniSolverTest.h"
  #include "../tests/CalculusStressTest.h"
  #include "../tests/BigIntTest.h"
#endif

// ---- Objetos globales ----
static Keyboard      g_keypad;           // driver 5×10 (Filas=OUTPUT, Cols=INPUT_PULLUP)
static DisplayDriver g_display;
static SystemApp     g_app(g_display, g_keypad);
static SerialBridge  g_serial;
static SplashScreen  g_splash;

// LVGL gating flag
bool g_lvglActive = true;

// Draw-buffers
static void* lvBuf1 = nullptr;
static void* lvBuf2 = nullptr;

// ====================================================================
// setup()
// ====================================================================
void setup() {
    Serial.begin(115200);
    // UART0 física (chip puente externo) — no necesita while(!Serial).
    // El puerto está disponible inmediatamente tras el reset.
    delay(50);  // Breve margen para que el chip puente termine el reset

    Serial.println("\n>>> NumOS Elite: System Ready (UART Mode)");
    Serial.println("=== NumOS Boot ===");

    // -- CAS-Lite Phase A Tests (if enabled) --
#ifdef CAS_RUN_TESTS
    cas::runCASTests();
    cas::runSymExprTests();
    cas::runASTFlatExprTests();
    cas::runSymDiffTests();
    cas::runOmniSolverTests();
    cas::runCalculusStressTest();
    cas::runBigIntTests();
#endif

    // -- 1. PSRAM --
    if (psramFound()) {
        Serial.printf("[PSRAM] %u KB libres\n",
                      (unsigned)(ESP.getFreePsram() / 1024));
    } else {
        Serial.println("[PSRAM] NO DETECTADA!");
    }

    // -- 2. TFT --
    g_display.begin();

    // -- 3. LVGL init --
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // -- 4. Draw buffer — SINGLE 32 KB internal DMA buffer --
    // CRITICAL constraints (ESP32-S3 + TFT_eSPI):
    //  • MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA  →  internal SRAM, DMA-capable.
    //  • PSRAM cannot be used: OPI-PSRAM is on a separate SPI bus; TFT_eSPI
    //    SPI-DMA on ESP32-S3 cannot source from PSRAM → StoreProhibited crash.
    //  • Single buffer only: double-buffer triggers LVGL 9.x pipelining
    //    deadlock (waits for a DMA-done ISR that never fires in blocking mode).
    //  32 KB = 51.2 lines/strip → ~half the flush calls vs 16 KB 25.6 lines.
    static constexpr uint32_t BUF_BYTES = 32U * 1024U; // 32768 bytes ≈ 51 lines

    lvBuf1 = heap_caps_malloc(BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lvBuf2 = nullptr;

    if (!lvBuf1) {
        Serial.println("[BOOT] BUFFER FAIL! HALT.");
        while (1) delay(1000);
    }
    Serial.printf("[BOOT] Draw buffer: %u bytes (single, internal DMA, ~%u lines)\n",
                  (unsigned)BUF_BYTES,
                  (unsigned)(BUF_BYTES / (320 * 2)));

    // -- 5. Display LVGL --
    g_display.initLvgl(lvBuf1, lvBuf2, BUF_BYTES);

    if (!lv_display_get_default()) {
        Serial.println("[BOOT] NO DISPLAY! HALT.");
        while (1) delay(1000);
    }

    // -- 5b. LvglKeypad input device (DEBE ir DESPUÉS del display) --
    // En LVGL 9.x, lv_indev_create() asocia el indev al display por defecto.
    // Si se crea antes del display, readCb nunca se invoca.
    LvglKeypad::init();
    Serial.println("[LVGL] LvglKeypad indev creado (post-display)");

    // -- 6. SplashScreen con animacion fade-in --
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

    // -- 7. SystemApp (carga launcher, LittleFS, etc.) --
    g_app.begin();

    // -- 8. Serial bridge (teclado via monitor serial) --
    g_serial.begin();

    // -- 9. Confirmar foco LVGL --
    lv_group_t* focusGrp = lv_indev_get_group(LvglKeypad::indev());
    lv_obj_t*   focused  = focusGrp ? lv_group_get_focused(focusGrp) : nullptr;
    Serial.printf("[GUI] Focus assigned to: %s (group=%p, obj=%p)\n",
                  focused ? "MainMenu card" : "NONE",
                  (void*)focusGrp, (void*)focused);

    Serial.println("[BOOT] OK — Use w/a/s/d to navigate, Enter=EXE, c=AC");
    Serial.println("[BOOT] Deep sleep DISABLED (USB mode)");
}

// ====================================================================
// loop()
// ====================================================================

// Heartbeat: imprime un '.' cada 5 segundos para confirmar que la S3 vive
static unsigned long _lastHeartbeat = 0;

void loop() {
    if (g_lvglActive) {
        lv_timer_handler();
    }

    g_app.update();

    KeyEvent serialEv;
    while (g_serial.pollEvent(serialEv)) {
        g_app.injectKey(serialEv);
    }

    // Heartbeat cada 5s (confirma que el loop corre y Serial TX funciona)
    if (millis() - _lastHeartbeat > 5000) {
        _lastHeartbeat = millis();
        Serial.printf("[HB] %lus uptime | heap=%u\n",
                      millis() / 1000, (unsigned)ESP.getFreeHeap());
    }

    delay(KEY_SCAN_INTERVAL_MS);
}

#endif // ARDUINO

