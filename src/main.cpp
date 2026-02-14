/**
 * main.cpp
 * Entry point for the ESP32 graphing/scientific calculator.
 *
 * Initializes display, keypad, serial bridge, and SystemApp.
 * The loop polls both the physical keypad (via SystemApp::update)
 * and the PC serial bridge (via SerialBridge::pollEvent).
 */

#include <Arduino.h>
#include "Config.h"
#include "input/KeyMatrix.h"
#include "input/SerialBridge.h"
#include "display/DisplayDriver.h"
#include "SystemApp.h"

static KeyMatrix g_keypad;
static DisplayDriver g_display;
static SystemApp g_app(g_display, g_keypad);
static SerialBridge g_serial;

void setup() {
    Serial.begin(115200);
    delay(500); // Un respiro para el hardware virtual
    // FORZAR RESET MANUAL
    pinMode(17, OUTPUT);
    digitalWrite(17, LOW);
    delay(20);
    digitalWrite(17, HIGH); // Sacamos a la pantalla del estado de reset
    delay(20);
    Serial.println("--- INICIO DE DIAGNÓSTICO ---");

    Serial.println("Paso 1: Iniciando Pantalla...");
    g_display.begin();
    Serial.println("Paso 1 completado: Pantalla OK");

    Serial.println("Paso 2: Iniciando Teclado...");
    g_keypad.begin();
    Serial.println("Paso 2 completado: Teclado OK");

    Serial.println("Paso 3: Iniciando Aplicación...");
    g_app.begin();
    Serial.println("Paso 3 completado: APP OK");

    // ── PC Serial Bridge ──
    g_serial.begin();

    Serial.println("--- SISTEMA LISTO ---");
}

void loop() {
    // 1. Physical keypad + normal app update
    g_app.update();

    // 2. PC Serial Bridge: read serial input & inject events
    KeyEvent serialEv;
    while (g_serial.pollEvent(serialEv)) {
        g_app.injectKey(serialEv);
    }

    delay(KEY_SCAN_INTERVAL_MS);
}
