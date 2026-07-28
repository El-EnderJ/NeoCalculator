#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "../hal/ArduinoCompat.h"
#endif

#include "../Config.h"
#include "../input/KeyCodes.h"

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
#include "../input/ProductionKeypadScanner.h"
#endif

class Keyboard {
public:
    Keyboard() = default;

    void begin();
    void update();
    bool pollEvent(KeyEvent& outEvent);

    void setEnabled(bool enabled);
    void forceReleaseAll();
    bool initialized() const;
    bool enabled() const;
    bool rowSelected() const;
    uint32_t overflowCount() const;

    static constexpr int ROWS = 5;
    static constexpr int COLS = 10;
    static constexpr int CONNECTED_COLS = 0; // Legacy CAM hardware remains off.

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    const numos::input::ProductionKeyState& diagnosticState(
        uint8_t row, uint8_t column) const;
    uint16_t diagnosticActiveColumns(uint8_t row) const;

private:
    enum class ScanPhase : uint8_t {
        WaitingToSelect,
        Settling,
    };

    void driveAllRowsInactive();
    void selectCurrentRow();
    uint16_t sampleColumns() const;

    numos::input::ProductionKeypadScanner _productionScanner;
    ScanPhase _scanPhase = ScanPhase::WaitingToSelect;
    uint8_t _currentRow = 0;
    uint32_t _scanStartedUs = 0;
    uint32_t _phaseStartedUs = 0;
    uint32_t _nextSelectUs = 0;
    bool _initialized = false;
    bool _enabled = false;
#else
private:
    const int _rowPins[ROWS] = {1, 2, 41, 42, 40};
    const int _colPins[COLS] = {6, 7, 8, 3, 15, 16, 17, 18, 21, 47};

    static constexpr uint16_t SCAN_INTERVAL_MS = 5;
    static constexpr uint16_t DEBOUNCE_MS = 20;
    static constexpr uint16_t AUTOREPEAT_DELAY_MS = 500;
    static constexpr uint16_t AUTOREPEAT_RATE_MS = 80;
    static const KeyCode _map[ROWS][COLS];

    bool _rawState[ROWS][COLS]{};
    bool _debState[ROWS][COLS]{};
    uint32_t _debTimer[ROWS][COLS]{};
    uint32_t _arTimer[ROWS][COLS]{};
    uint32_t _lastScanMs = 0;

    static constexpr int QUEUE_SIZE = 16;
    KeyEvent _queue[QUEUE_SIZE]{};
    int _qHead = 0;
    int _qTail = 0;

    void doScan();
    void pushEvent(const KeyEvent& ev);
#endif
};
