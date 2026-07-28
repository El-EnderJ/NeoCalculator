#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "KeyCodes.h"
#include "generated/ProductionKeypadMap.generated.h"

namespace numos::input {

struct ProductionScannerConfig {
    uint8_t debounceIntegratorMax = 4;
    uint32_t repeatInitialDelayMs = 500;
    uint32_t repeatIntervalMs = 80;
};

struct ProductionKeyState {
    bool raw = false;
    bool debounced = false;
    bool dispatchedDown = false;
    bool suppressRelease = false;
    bool inhibitUntilReleased = false;
    bool repeatStarted = false;
    uint8_t integrator = 0;
    uint32_t pressedAtMs = 0;
    uint32_t lastRepeatMs = 0;
};

class ProductionKeypadScanner {
public:
    static constexpr std::size_t kRows = 5;
    static constexpr std::size_t kColumns = 10;
    static constexpr std::size_t kKeyCount = 50;
    // A full 50-key down burst fits without losing a PRESS. This is bounded
    // static DRAM, but leaves enough headroom to preserve releases while the
    // UI drains the queue.
    static constexpr std::size_t kQueueCapacity = 64;

    explicit ProductionKeypadScanner(
        ProductionScannerConfig config = ProductionScannerConfig{});

    // One call supplies all ten samples for one electrical row. Bit c set
    // means the switch at (row,c) is electrically pressed.
    void ingestRow(uint8_t row, uint16_t pressedColumns, uint32_t nowMs);

    bool pollEvent(KeyEvent& event);
    void forceReleaseAll(uint32_t nowMs);
    void reset();

    const ProductionKeyState& state(uint8_t row, uint8_t column) const;
    uint16_t activeColumns(uint8_t row) const;
    uint32_t overflowCount() const { return _overflowCount; }
    std::size_t queuedEventCount() const { return _queueCount; }
    const ProductionScannerConfig& config() const { return _config; }

private:
    static constexpr std::size_t index(uint8_t row, uint8_t column) {
        return static_cast<std::size_t>(row) * kColumns + column;
    }

    void transition(std::size_t keyIndex, bool pressed, uint32_t nowMs);
    bool pushEvent(std::size_t keyIndex, KeyAction action);
    void eraseQueueIndex(std::size_t queueIndex);
    bool removeQueuedPress(std::size_t keyIndex);
    bool removeOldestRepeat();
    bool removeUndispatchedPress();

    ProductionScannerConfig _config;
    std::array<ProductionKeyState, kKeyCount> _states{};
    std::array<KeyEvent, kQueueCapacity> _queue{};
    std::size_t _queueCount = 0;
    uint32_t _overflowCount = 0;
};

} // namespace numos::input
