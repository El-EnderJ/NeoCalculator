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
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"
#include "input/SerialBridge.h"
#include "input/LvglKeypad.h"
#include "SystemApp.h"
#include "ui/SplashScreen.h"

// CAS tests (enable via -DCAS_RUN_TESTS in platformio.ini)
#ifdef CAS_RUN_TESTS
  #include "../tests/CASTest.h"
  #include "../tests/SymExprTest.h"
  #include "../tests/ASTFlatExprTest.h"
#endif

// ---- Objetos globales ----
static KeyMatrix     g_keypad;
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

    // USB CDC nativo del S3: esperar a que el host enumere el puerto.
    // Sin esto, Serial.print() se pierde y el Monitor no conecta.
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) { delay(10); }
    delay(100);  // Margen extra para que el driver del host se estabilice

    Serial.println("\n=== NumOS Boot ===");

    // -- CAS-Lite Phase A Tests (if enabled) --
#ifdef CAS_RUN_TESTS
    cas::runCASTests();
    cas::runSymExprTests();
    cas::runASTFlatExprTests();
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

    // -- 4. Draw buffers (HEAP INTERNO DMA — NO PSRAM) --
    static constexpr uint32_t BUF_LINES = 10;
    static constexpr uint32_t BUF_BYTES = 320 * BUF_LINES * 2; // 6400 bytes

    lvBuf1 = heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    lvBuf2 = heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

    if (!lvBuf1 || !lvBuf2) {
        Serial.println("[BOOT] BUFFER FAIL! HALT.");
        while (1) delay(1000);
    }

    // -- 5. Display LVGL --
    g_display.initLvgl(lvBuf1, lvBuf2, BUF_BYTES);

    if (!lv_display_get_default()) {
        Serial.println("[BOOT] NO DISPLAY! HALT.");
        while (1) delay(1000);
    }

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

    Serial.println("[BOOT] OK — Escribe 'w' y pulsa Enter para probar.");
    Serial.println("[BOOT] Si ves este mensaje, Serial TX funciona.");
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

