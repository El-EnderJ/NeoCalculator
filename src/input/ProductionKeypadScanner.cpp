#include "ProductionKeypadScanner.h"

namespace numos::input {

ProductionKeypadScanner::ProductionKeypadScanner(
    const ProductionScannerConfig config)
    : _config(config) {
    if (_config.debounceIntegratorMax == 0) {
        _config.debounceIntegratorMax = 1;
    }
}

void ProductionKeypadScanner::ingestRow(const uint8_t row,
                                        const uint16_t pressedColumns,
                                        const uint32_t nowMs) {
    if (row >= kRows) return;
    for (uint8_t column = 0; column < kColumns; ++column) {
        const std::size_t i = index(row, column);
        auto& key = _states[i];
        const bool raw = (pressedColumns & (1U << column)) != 0;
        key.raw = raw;
        if (key.inhibitUntilReleased) {
            if (!raw) {
                key.inhibitUntilReleased = false;
                key.integrator = 0;
            }
            continue;
        }
        if (raw) {
            if (key.integrator < _config.debounceIntegratorMax) {
                ++key.integrator;
            }
        } else if (key.integrator > 0) {
            --key.integrator;
        }

        if (!key.debounced &&
            key.integrator == _config.debounceIntegratorMax) {
            transition(i, true, nowMs);
        } else if (key.debounced && key.integrator == 0) {
            transition(i, false, nowMs);
        }

        if (!key.debounced || !raw ||
            !kProductionKeypadMap[i].repeatable) {
            continue;
        }
        const uint32_t elapsed = nowMs - key.lastRepeatMs;
        const uint32_t threshold =
            key.repeatStarted ? _config.repeatIntervalMs
                              : _config.repeatInitialDelayMs;
        if (elapsed >= threshold) {
            pushEvent(i, KeyAction::REPEAT);
            key.repeatStarted = true;
            key.lastRepeatMs = nowMs; // No burst catch-up after a stalled frame.
        }
    }
}

void ProductionKeypadScanner::transition(const std::size_t keyIndex,
                                         const bool pressed,
                                         const uint32_t nowMs) {
    auto& key = _states[keyIndex];
    key.debounced = pressed;
    key.repeatStarted = false;
    if (pressed) {
        key.pressedAtMs = nowMs;
        key.lastRepeatMs = nowMs;
        key.suppressRelease = !pushEvent(keyIndex, KeyAction::PRESS);
    } else {
        if (!key.suppressRelease) {
            pushEvent(keyIndex, KeyAction::RELEASE);
        }
        key.suppressRelease = false;
    }
}

bool ProductionKeypadScanner::pushEvent(const std::size_t keyIndex,
                                        const KeyAction action) {
    const auto& mapping = kProductionKeypadMap[keyIndex];
    KeyEvent event{
        mapping.keyCode,
        action,
        mapping.electricalRow,
        mapping.electricalColumn,
    };
    if (_queueCount < kQueueCapacity) {
        _queue[_queueCount++] = event;
        return true;
    }

    ++_overflowCount;
    if (action == KeyAction::REPEAT) return false;

    if (action == KeyAction::RELEASE) {
        // If PRESS was never dispatched, collapse the pair. No logical state
        // was exposed, so fabricating a RELEASE would be incorrect.
        if (removeQueuedPress(keyIndex)) return true;
        if (removeOldestRepeat() || removeUndispatchedPress()) {
            _queue[_queueCount++] = event;
            return true;
        }
        return false;
    }

    // A dropped PRESS is remembered so its later physical release is also
    // suppressed. This prevents release-without-down fabrication.
    return false;
}

void ProductionKeypadScanner::eraseQueueIndex(const std::size_t queueIndex) {
    if (queueIndex >= _queueCount) return;
    for (std::size_t i = queueIndex + 1; i < _queueCount; ++i) {
        _queue[i - 1] = _queue[i];
    }
    --_queueCount;
}

bool ProductionKeypadScanner::removeQueuedPress(
    const std::size_t keyIndex) {
    const int row = static_cast<int>(keyIndex / kColumns);
    const int column = static_cast<int>(keyIndex % kColumns);
    bool removedPress = false;
    for (std::size_t i = 0; i < _queueCount;) {
        const bool sameKey =
            _queue[i].row == row && _queue[i].col == column;
        if (sameKey && (_queue[i].action == KeyAction::PRESS ||
                        _queue[i].action == KeyAction::REPEAT)) {
            removedPress =
                removedPress || _queue[i].action == KeyAction::PRESS;
            eraseQueueIndex(i);
            continue;
        }
        ++i;
    }
    return removedPress;
}

bool ProductionKeypadScanner::removeOldestRepeat() {
    for (std::size_t i = 0; i < _queueCount; ++i) {
        if (_queue[i].action == KeyAction::REPEAT) {
            eraseQueueIndex(i);
            return true;
        }
    }
    return false;
}

bool ProductionKeypadScanner::removeUndispatchedPress() {
    for (std::size_t i = 0; i < _queueCount; ++i) {
        if (_queue[i].action != KeyAction::PRESS) continue;
        const std::size_t keyIndex =
            index(static_cast<uint8_t>(_queue[i].row),
                  static_cast<uint8_t>(_queue[i].col));
        if (!_states[keyIndex].dispatchedDown) {
            _states[keyIndex].suppressRelease = true;
            eraseQueueIndex(i);
            return true;
        }
    }
    return false;
}

bool ProductionKeypadScanner::pollEvent(KeyEvent& event) {
    if (_queueCount == 0) return false;
    event = _queue[0];
    eraseQueueIndex(0);
    const std::size_t keyIndex =
        index(static_cast<uint8_t>(event.row), static_cast<uint8_t>(event.col));
    if (event.action == KeyAction::PRESS) {
        _states[keyIndex].dispatchedDown = true;
    } else if (event.action == KeyAction::RELEASE) {
        _states[keyIndex].dispatchedDown = false;
    }
    return true;
}

void ProductionKeypadScanner::forceReleaseAll(const uint32_t nowMs) {
    (void)nowMs;
    // Transition is a hard input boundary. Discard queued PRESS/REPEAT events
    // from the old application, then emit releases only for presses that were
    // actually dispatched. This prevents both post-transition repeats and
    // release-without-down fabrication.
    _queueCount = 0;
    for (std::size_t i = 0; i < _states.size(); ++i) {
        auto& key = _states[i];
        if (key.dispatchedDown && !key.suppressRelease) {
            pushEvent(i, KeyAction::RELEASE);
        }
        const bool wasActive = key.raw || key.debounced;
        key.raw = false;
        key.debounced = false;
        key.integrator = 0;
        key.repeatStarted = false;
        key.suppressRelease = false;
        key.inhibitUntilReleased = wasActive;
    }
}

void ProductionKeypadScanner::reset() {
    _states = {};
    _queueCount = 0;
    _overflowCount = 0;
}

const ProductionKeyState& ProductionKeypadScanner::state(
    const uint8_t row,
    const uint8_t column) const {
    static constexpr ProductionKeyState kInvalid{};
    if (row >= kRows || column >= kColumns) return kInvalid;
    return _states[index(row, column)];
}

uint16_t ProductionKeypadScanner::activeColumns(const uint8_t row) const {
    if (row >= kRows) return 0;
    uint16_t mask = 0;
    for (uint8_t column = 0; column < kColumns; ++column) {
        if (_states[index(row, column)].debounced) {
            mask |= static_cast<uint16_t>(1U << column);
        }
    }
    return mask;
}

} // namespace numos::input
