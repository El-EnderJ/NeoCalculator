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

#if NUMOS_BOARD_PROD_WROOM1U_N16R8

#include "../hardware/BoardProfile.h"
#include "../input/KeySemanticResolver.h"

#if !defined(NUMOS_PRODUCTION_KEYPAD_MAPPING_READY) || \
    !NUMOS_PRODUCTION_KEYPAD_MAPPING_READY
#error "Production keyboard requires the generated and validated mapping"
#endif

namespace {

bool elapsedAtLeast(const uint32_t now,
                    const uint32_t then,
                    const uint32_t duration) {
    return static_cast<uint32_t>(now - then) >= duration;
}

bool deadlineReached(const uint32_t now, const uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

int levelFor(const numos::hardware::ActiveLevel level) {
    return level == numos::hardware::ActiveLevel::Low ? LOW : HIGH;
}

} // namespace

void Keyboard::driveAllRowsInactive() {
    const auto& matrix =
        numos::hardware::kProductionBoard.electricalMatrix;
    const int inactive = levelFor(matrix.inactiveRowLevel);
    for (const int gpio : matrix.rowOutputs) {
        digitalWrite(gpio, inactive);
    }
}

void Keyboard::begin() {
    const auto& matrix =
        numos::hardware::kProductionBoard.electricalMatrix;
    const int inactive = levelFor(matrix.inactiveRowLevel);

    // WHY: preload every output latch before output enable so no row can emit
    // an active-low glitch while GPIO ownership transfers to the scanner.
    for (const int gpio : matrix.rowOutputs) {
        digitalWrite(gpio, inactive);
    }
    for (const int gpio : matrix.rowOutputs) {
        pinMode(gpio, OUTPUT);
        digitalWrite(gpio, inactive);
    }
    for (const int gpio : matrix.columnInputs) {
        pinMode(gpio, INPUT_PULLUP);
    }

    _productionScanner.reset();
    _currentRow = 0;
    _scanPhase = ScanPhase::WaitingToSelect;
    _nextSelectUs = micros();
    _scanStartedUs = _nextSelectUs;
    _phaseStartedUs = _nextSelectUs;
    _initialized = true;
    _enabled = true;
}

void Keyboard::selectCurrentRow() {
    const auto& matrix =
        numos::hardware::kProductionBoard.electricalMatrix;
    driveAllRowsInactive();
    const uint8_t pinIndex = matrix.rowOrder[_currentRow];
    digitalWrite(matrix.rowOutputs[pinIndex],
                 levelFor(matrix.selectedRowLevel));
}

uint16_t Keyboard::sampleColumns() const {
    const auto& matrix =
        numos::hardware::kProductionBoard.electricalMatrix;
    const int pressed = levelFor(matrix.pressedColumnLevel);
    uint16_t mask = 0;
    for (uint8_t logicalColumn = 0; logicalColumn < COLS; ++logicalColumn) {
        const uint8_t pinIndex = matrix.columnOrder[logicalColumn];
        if (digitalRead(matrix.columnInputs[pinIndex]) == pressed) {
            mask |= static_cast<uint16_t>(1U << logicalColumn);
        }
    }
    return mask;
}

void Keyboard::update() {
    if (!_initialized || !_enabled) return;
    const auto& matrix =
        numos::hardware::kProductionBoard.electricalMatrix;
    const uint32_t nowUs = micros();

    if (_scanPhase == ScanPhase::WaitingToSelect) {
        if (!deadlineReached(nowUs, _nextSelectUs)) return;
        if (_currentRow == 0) _scanStartedUs = nowUs;
        selectCurrentRow();
        _phaseStartedUs = nowUs;
        _scanPhase = ScanPhase::Settling;
        return;
    }

    if (!elapsedAtLeast(nowUs, _phaseStartedUs,
                        matrix.settlingDurationUs)) {
        return;
    }

    const uint16_t pressedColumns = sampleColumns();
    driveAllRowsInactive();
    _productionScanner.ingestRow(
        _currentRow, pressedColumns, millis());

    ++_currentRow;
    if (_currentRow == ROWS) {
        _currentRow = 0;
        _nextSelectUs = _scanStartedUs + matrix.fullScanIntervalUs;
        if (deadlineReached(nowUs, _nextSelectUs)) {
            _nextSelectUs = nowUs;
        }
    } else {
        _nextSelectUs = nowUs;
    }
    _scanPhase = ScanPhase::WaitingToSelect;
}

bool Keyboard::pollEvent(KeyEvent& event) {
    return _productionScanner.pollEvent(event);
}

void Keyboard::setEnabled(const bool enabled) {
    if (!_initialized || _enabled == enabled) return;
    if (!enabled) {
        driveAllRowsInactive();
        _productionScanner.forceReleaseAll(millis());
        numos::input::KeySemanticResolver::reset();
        _enabled = false;
        return;
    }
    driveAllRowsInactive();
    _currentRow = 0;
    _scanPhase = ScanPhase::WaitingToSelect;
    _nextSelectUs = micros();
    _enabled = true;
}

void Keyboard::forceReleaseAll() {
    if (!_initialized) return;
    driveAllRowsInactive();
    _productionScanner.forceReleaseAll(millis());
    numos::input::KeySemanticResolver::reset();
}

bool Keyboard::initialized() const { return _initialized; }
bool Keyboard::enabled() const { return _enabled; }
bool Keyboard::rowSelected() const {
    return _initialized && _enabled && _scanPhase == ScanPhase::Settling;
}
uint32_t Keyboard::overflowCount() const {
    return _productionScanner.overflowCount();
}

const numos::input::ProductionKeyState& Keyboard::diagnosticState(
    const uint8_t row,
    const uint8_t column) const {
    return _productionScanner.state(row, column);
}

uint16_t Keyboard::diagnosticActiveColumns(const uint8_t row) const {
    return _productionScanner.activeColumns(row);
}

#else

// ── Keymap 5×10 ───────────────────────────────────────────────────────────────
//
// Diseño visual de las 15 teclas ACTUALMENTE CABLEADAS (cols 0-2):
// (C0/C1 reasignados de GPIO 4/5 → GPIO 6/7 para evitar conflicto con TFT_DC/RST)
//
//  Col →   C0 (GPIO 6)  C1 (GPIO 7)  C2 (GPIO 8)  C3…C9 (no cableadas)
//  R0 (GPIO  1)   7           8           9
//  R1 (GPIO  2)   4           5           6
//  R2 (GPIO 41)   1           2           3
//  R3 (GPIO 42)   0          AC         ENTER
//  R4 (GPIO 40)   +           -           ×
//
// Full 5×10 planned layout (C3-C9):
//   R0 top row:  SHIFT ALPHA MODE SETUP F1 F2 F3 F4 F5 EXE
//   Top row function keys F1-F5 mapped to C4-C8.
//   Physical '<' key → EXE (Execute/Solve) at C9.
//   Physical Enter → ENTER (Place/Select) remains at R3C2.
//
const KeyCode Keyboard::_map[Keyboard::ROWS][Keyboard::COLS] = {
    // C0          C1          C2          C3          C4
    // C5          C6          C7          C8          C9
    { KeyCode::NUM_7, KeyCode::NUM_8, KeyCode::NUM_9,
      KeyCode::SETUP, KeyCode::F1,    KeyCode::F2,
      KeyCode::F3,    KeyCode::F4,    KeyCode::F5,    KeyCode::EXE  },  // Row 0

    { KeyCode::NUM_4, KeyCode::NUM_5, KeyCode::NUM_6,
      KeyCode::NONE,  KeyCode::LEFT,  KeyCode::UP,
      KeyCode::DOWN,  KeyCode::RIGHT, KeyCode::NONE,  KeyCode::NONE },  // Row 1

    { KeyCode::NUM_1, KeyCode::NUM_2, KeyCode::NUM_3,
      KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE,
      KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE },  // Row 2

    { KeyCode::NUM_0, KeyCode::AC,     KeyCode::ENTER,
      KeyCode::SHIFT, KeyCode::ALPHA,  KeyCode::MODE,
      KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE },  // Row 3

    { KeyCode::ADD,   KeyCode::SUB,   KeyCode::MUL,
      KeyCode::DIV,   KeyCode::DEL,   KeyCode::NONE,
      KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE,  KeyCode::NONE },  // Row 4
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

void Keyboard::setEnabled(bool) {}
void Keyboard::forceReleaseAll() {}
bool Keyboard::initialized() const { return true; }
bool Keyboard::enabled() const { return CONNECTED_COLS > 0; }
bool Keyboard::rowSelected() const { return false; }
uint32_t Keyboard::overflowCount() const { return 0; }

#endif
