#pragma once

// PhysicalKeyRoute — map a logical KeyCode to the production matrix key that
// produces it, so test harnesses (serial bridge, emulator scripts) can inject
// input through the same path as a pressed key: scanner event queue ->
// KeySemanticResolver -> application. The search uses the generated Math
// planes: a key whose electrical KeyCode matches first, then the Primary,
// Shift, Alpha and Shift+Alpha planes by resolved legacy code.

#include <cstdint>

#include "KeyCodes.h"
#include "generated/ProductionKeypadMap.generated.h"

namespace numos::input {

struct PhysicalKeyRoute {
    bool found = false;
    bool shift = false;    // SHIFT must be latched before the key
    bool alpha = false;    // ALPHA must be latched before the key
    uint8_t row = 0;       // electrical row
    uint8_t col = 0;       // electrical column
};

inline PhysicalKeyRoute physicalKeyAt(const std::size_t index, const bool shift,
                                      const bool alpha) {
    PhysicalKeyRoute route;
    route.found = true;
    route.shift = shift;
    route.alpha = alpha;
    route.row = kProductionKeypadMap[index].electricalRow;
    route.col = kProductionKeypadMap[index].electricalColumn;
    return route;
}

inline PhysicalKeyRoute physicalRouteFor(KeyCode logical) {
    // Logical-only aliases. The production keypad has HOME (no MODE key) and a
    // single fraction-template key (no Ans-capturing DIV key).
    if (logical == KeyCode::MODE) logical = KeyCode::HOME;
    if (logical == KeyCode::DIV) logical = KeyCode::FRAC;
    if (logical == KeyCode::NONE) return {};

    for (std::size_t i = 0; i < kProductionKeypadMap.size(); ++i) {
        if (kProductionKeypadMap[i].keyCode == logical) return physicalKeyAt(i, false, false);
    }
    static constexpr KeyPlane kPlanes[] = {
        KeyPlane::Primary, KeyPlane::Shift, KeyPlane::Alpha, KeyPlane::ShiftAlpha};
    for (const KeyPlane plane : kPlanes) {
        const auto planeIndex = static_cast<std::size_t>(plane);
        for (std::size_t i = 0; i < kMathPlaneDefinitions.size(); ++i) {
            if (kMathPlaneDefinitions[i][planeIndex].legacyCode != logical) continue;
            return physicalKeyAt(i,
                                 plane == KeyPlane::Shift || plane == KeyPlane::ShiftAlpha,
                                 plane == KeyPlane::Alpha || plane == KeyPlane::ShiftAlpha);
        }
    }
    return {};
}

// Electrical KeyCode of a matrix position, or NONE when it does not exist.
inline KeyCode physicalKeyCodeAt(const int row, const int col) {
    for (const auto& key : kProductionKeypadMap) {
        if (key.electricalRow == row && key.electricalColumn == col) return key.keyCode;
    }
    return KeyCode::NONE;
}

} // namespace numos::input
