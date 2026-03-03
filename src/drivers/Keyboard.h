/**
 * src/drivers/Keyboard.h
 * ──────────────────────────────────────────────────────────────────────────────
 * Driver para matriz de teclado 5×10 (ESP32-S3 N16R8)
 *
 * POLARIDAD DE ESCANEO (distinta del KeyMatrix legacy):
 *   Filas  → OUTPUT  — se ponen a LOW de una en una durante el escaneo.
 *   Columnas → INPUT_PULLUP — se leen; LOW = tecla pulsada.
 *
 * COLUMNAS FÍSICAMENTE CABLEADAS:
 *   Actualmente solo las primeras 3 (GPIO 6, 7, 8).
 *   Para activar el hardware completo (5×10) basta con cambiar
 *   CONNECTED_COLS de 3 a 10 — el keymap ya tiene todos los slots.
 *
 * ✅ CONFLICTO DE PINES RESUELTO (2026-03-02):
 *   C0/C1 han sido reasignados de GPIO 4/5 (TFT_DC/RST) a GPIO 6/7.
 *   Los 3 pines actualmente cableados (GPIO 6, 7, 8) no conflictúan
 *   con ninguna señal del bus TFT ni con los pines de fila.
 *
 * API pública — compatible drop-in con la clase KeyMatrix:
 *   begin()       – Configura pines.
 *   update()      – Llamar en cada iteración del loop; no bloquea.
 *   pollEvent()   – Saca el siguiente evento de la cola (FIFO).
 *
 * ──────────────────────────────────────────────────────────────────────────────
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "../hal/ArduinoCompat.h"
#endif

#include "../input/KeyCodes.h"   // KeyCode, KeyAction, KeyEvent

class Keyboard {
public:
    Keyboard() = default;

    /// Configura pines y limpia el estado interno.
    void begin();

    /// Avanza el scanner; llámalo en cada loop(). No bloquea.
    void update();

    /// Devuelve true y rellena outEvent si hay un evento pendiente.
    bool pollEvent(KeyEvent& outEvent);

    // ── Dimensiones — públicas para código que las necesite ──────────────
    static constexpr int ROWS = 5;
    static constexpr int COLS = 10;

    /**
     * Número de columnas físicamente cableadas.
     * Solo se escanean estas columnas; el resto se ignora para evitar
     * falsas pulsaciones en pines flotantes.
     *
     * ⚠ PUESTO A 0: No hay botones soldados todavía.
     *   Sin switches físicos, las filas (LOW) se acoplan directamente a las
     *   columnas (INPUT_PULLUP) → todas las teclas leen como "pulsadas".
     *   Pon CONNECTED_COLS = 3 cuando sueldes los primeros 15 botones.
     *
     * 🔧 Para ampliar al layout 5×10 completo: cambia este valor a 10.
     */
    static constexpr int CONNECTED_COLS = 0;  // ← 0 = escaneo desactivado

private:
    // ── GPIOs ──────────────────────────────────────────────────────────────
    // Filas (OUTPUT) — se ponen a LOW de una en una.
    const int _rowPins[ROWS] = { 1, 2, 41, 42, 40 };

    // Columnas (INPUT_PULLUP) — solo se leen las CONNECTED_COLS primeras.
    // El orden físico es el orden de izquierda a derecha en el PCB.
    // C0/C1 reasignados de GPIO 4/5 → GPIO 6/7 para evitar conflicto con TFT_DC/RST.
    const int _colPins[COLS] = { 6, 7, 8, 3, 15, 16, 17, 18, 21, 47 };

    // ── Timings ────────────────────────────────────────────────────────────
    static constexpr uint16_t SCAN_INTERVAL_MS    =  5;   // ms entre escaneos
    static constexpr uint16_t DEBOUNCE_MS         = 20;   // ms de anti-rebote
    static constexpr uint16_t AUTOREPEAT_DELAY_MS = 500;  // ms antes del autorepeat
    static constexpr uint16_t AUTOREPEAT_RATE_MS  =  80;  // ms entre REPEATs

    // ── Mapa lógico completo 5×10 ──────────────────────────────────────────
    // Slots sin cablear = KeyCode::NONE.  Añade más en Keyboard.cpp cuando
    // conectes columnas nuevas: simplemente sustituye NONE por el KeyCode.
    static const KeyCode _map[ROWS][COLS];

    // ── Estado por posición (ROWS × COLS) ─────────────────────────────────
    bool     _rawState[ROWS][COLS] {};       // físico: leído en el scan actual
    bool     _debState[ROWS][COLS] {};       // lógico: tras debounce confirmado
    uint32_t _debTimer[ROWS][COLS] {};       // millis() del último cambio raw
    uint32_t _arTimer[ROWS][COLS]  {};       // millis() del último PRESS/REPEAT

    // ── Timestamp del último scan ──────────────────────────────────────────
    uint32_t _lastScanMs = 0;

    // ── Cola circular de eventos ───────────────────────────────────────────
    static constexpr int QUEUE_SIZE = 16;    // Potencia de 2 — facilita wrap
    KeyEvent _queue[QUEUE_SIZE];
    int      _qHead = 0;                     // índice de lectura
    int      _qTail = 0;                     // índice de escritura

    // ── Helpers ───────────────────────────────────────────────────────────
    void doScan();                           // Escaneo físico + debounce
    void pushEvent(const KeyEvent& ev);      // Inserta en la cola (descarta si llena)
};
