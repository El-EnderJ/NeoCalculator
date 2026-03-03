/**
 * src/drivers/Keyboard.cpp
 * ──────────────────────────────────────────────────────────────────────────────
 * Implementación del driver de matriz 5×10 para ESP32-S3.
 *
 * CÓMO EXTENDER EL KEYMAP:
 *   1. Aumenta CONNECTED_COLS en Keyboard.h (máx. 10).
 *   2. Rellena las posiciones NONE del array _map con los KeyCodes correctos.
 *   3. Reconecta los nuevos pines en el PCB.  No hay que tocar nada más.
 *
 * ──────────────────────────────────────────────────────────────────────────────
 */

#include "Keyboard.h"

// ── Keymap 5×10 ───────────────────────────────────────────────────────────────
//
// Diseño visual de las 15 teclas ACTUALMENTE CABLEADAS (cols 0-2):
// (C0/C1 reasignados de GPIO 4/5 → GPIO 6/7 para evitar conflicto con TFT_DC/RST)
//
//  Col →   C0 (GPIO 6)  C1 (GPIO 7)  C2 (GPIO 8)  C3…C9 (no cableadas)
//  R0 (GPIO  1)   7           8           9
//  R1 (GPIO  2)   4           5           6
//  R2 (GPIO 41)   1           2           3
//  R3 (GPIO 42)   0          AC          EXE
//  R4 (GPIO 40)   +           -           ×
//
// Los slots C3…C9 están marcados con KeyCode::NONE.
// Para el layout completo previsto (bloques de funciones, trigonometría, etc.),
// sustituir NONE con los KeyCodes correspondientes al conectar el hardware.
//
const KeyCode Keyboard::_map[Keyboard::ROWS][Keyboard::COLS] = {
    // C0          C1          C2          C3          C4
    // C5          C6          C7          C8          C9
    { KeyCode::NUM_7, KeyCode::NUM_8, KeyCode::NUM_9,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE, KeyCode::NONE },  // Row 0

    { KeyCode::NUM_4, KeyCode::NUM_5, KeyCode::NUM_6,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE, KeyCode::NONE },  // Row 1

    { KeyCode::NUM_1, KeyCode::NUM_2, KeyCode::NUM_3,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE, KeyCode::NONE },  // Row 2

    { KeyCode::NUM_0, KeyCode::AC,     KeyCode::ENTER,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE, KeyCode::NONE },  // Row 3

    { KeyCode::ADD,   KeyCode::SUB,   KeyCode::MUL,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE,
      KeyCode::NONE, KeyCode::NONE, KeyCode::NONE, KeyCode::NONE },  // Row 4
};

// ── begin() ──────────────────────────────────────────────────────────────────

void Keyboard::begin() {
    // Filas: OUTPUT, idle en HIGH (ninguna fila activa).
    for (int r = 0; r < ROWS; ++r) {
        pinMode(_rowPins[r], OUTPUT);
        digitalWrite(_rowPins[r], HIGH);
    }

    // Columnas: INPUT_PULLUP.  Solo configuramos las que están cableadas para
    // evitar que pines flotantes generen eventos espurios.
    for (int c = 0; c < CONNECTED_COLS; ++c) {
        pinMode(_colPins[c], INPUT_PULLUP);
    }

    // Limpia el estado completo de la matriz.
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            _rawState[r][c] = false;
            _debState[r][c] = false;
            _debTimer[r][c] = 0;
            _arTimer[r][c]  = 0;
        }
    }

    _qHead = 0;
    _qTail = 0;
    _lastScanMs = millis();
}

// ── update() ─────────────────────────────────────────────────────────────────

void Keyboard::update() {
    uint32_t now = millis();
    if (now - _lastScanMs < SCAN_INTERVAL_MS) return;
    _lastScanMs = now;
    doScan();
}

// ── pollEvent() ──────────────────────────────────────────────────────────────

bool Keyboard::pollEvent(KeyEvent& outEvent) {
    if (_qHead == _qTail) return false;   // Cola vacía
    outEvent = _queue[_qHead];
    _qHead = (_qHead + 1) & (QUEUE_SIZE - 1);
    return true;
}

// ── doScan() — núcleo del driver ─────────────────────────────────────────────

void Keyboard::doScan() {
    uint32_t now = millis();

    for (int r = 0; r < ROWS; ++r) {
        // Activa la fila poniéndola a LOW.
        digitalWrite(_rowPins[r], LOW);

        // Pequeño delay para que los pines se estabilicen.
        // delayMicroseconds(10) es suficiente para ESP32-S3.
        delayMicroseconds(10);

        // Lee solo las columnas físicamente cableadas.
        for (int c = 0; c < CONNECTED_COLS; ++c) {
            bool rawNow = (digitalRead(_colPins[c]) == LOW);  // LOW = pulsada

            // ── Máquina de estados de debounce por celda ─────────────────
            if (rawNow != _rawState[r][c]) {
                // El estado físico cambió: reinicia el temporizador.
                _rawState[r][c] = rawNow;
                _debTimer[r][c]  = now;
            } else if ((now - _debTimer[r][c]) >= DEBOUNCE_MS) {
                // Estado estable durante DEBOUNCE_MS ms: confirma el cambio.
                if (rawNow != _debState[r][c]) {
                    _debState[r][c] = rawNow;

                    KeyCode kc = _map[r][c];
                    if (kc != KeyCode::NONE) {
                        KeyAction action = rawNow ? KeyAction::PRESS
                                                  : KeyAction::RELEASE;
                        pushEvent({ kc, action, r, c });

                        if (rawNow) {
                            // Inicia el temporizador de auto-repetición.
                            _arTimer[r][c] = now;
                        }
                    }
                }
            }

            // ── Auto-repetición (solo si sigue pulsada) ──────────────────
            if (_debState[r][c] && _rawState[r][c]) {
                uint32_t elapsed = now - _arTimer[r][c];
                // Primera repetición: espera AUTOREPEAT_DELAY_MS.
                // Siguientes: cada AUTOREPEAT_RATE_MS.
                uint32_t threshold = (_arTimer[r][c] == _debTimer[r][c])
                                     ? AUTOREPEAT_DELAY_MS
                                     : AUTOREPEAT_RATE_MS;

                if (elapsed >= threshold) {
                    KeyCode kc = _map[r][c];
                    if (kc != KeyCode::NONE) {
                        pushEvent({ kc, KeyAction::REPEAT, r, c });
                    }
                    _arTimer[r][c] = now;
                }
            }
        }

        // Desactiva la fila: vuelve a HIGH.
        digitalWrite(_rowPins[r], HIGH);
    }
}

// ── pushEvent() ──────────────────────────────────────────────────────────────

void Keyboard::pushEvent(const KeyEvent& ev) {
    int nextTail = (_qTail + 1) & (QUEUE_SIZE - 1);
    if (nextTail == _qHead) return;   // Cola llena: descarta el evento silenciosamente.
    _queue[_qTail] = ev;
    _qTail = nextTail;
}
