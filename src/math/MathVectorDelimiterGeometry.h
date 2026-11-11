#pragma once

#include <algorithm>
#include <cstdint>

namespace vpam {

// Compact-font policy for synthetic braces/bars; genuine STIX ()[] bypass it.
// Coordinates describe a half-open ink slot, including rounded stroke caps.
struct VectorDelimiterGeometry {
    int16_t stroke;
    int16_t radius;
    int16_t span;
    int16_t width;
    int16_t firstY;
    int16_t lastY;
    int16_t shoulder;
    int16_t cuspHalf;
};

inline VectorDelimiterGeometry vectorDelimiterGeometry(
        int16_t height, int16_t em, uint32_t codepoint) {
    const int16_t stroke = std::max<int16_t>(1, std::min<int16_t>(
        std::max<int16_t>(2, (em + 4) / 8), std::max<int16_t>(1, height - 1)));
    const int16_t radius = (stroke + 1) / 2;
    const int16_t span = (codepoint == '{' || codepoint == '}')
        ? std::max<int16_t>(4, std::min<int16_t>(height / 5, em))
        : 0;
    const int16_t firstY = std::min<int16_t>(radius, std::max<int16_t>(0, height / 2));
    const int16_t lastY = std::max<int16_t>(firstY, height - 1 - radius);
    const int16_t usable = lastY - firstY;
    // WHY: reserve the same height-dependent bulge that draw submits, plus
    // both rounded caps. Using the absent assembly glyph's width clips braces.
    return {stroke, radius, span, static_cast<int16_t>(span + 2 * radius + 1),
            firstY, lastY, static_cast<int16_t>(usable / 5),
            static_cast<int16_t>(usable / 18)};
}

} // namespace vpam
