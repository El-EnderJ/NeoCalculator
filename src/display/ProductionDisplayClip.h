#pragma once

#include <cstdint>

namespace numos::display {

struct FlushArea {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
};

struct ClippedFlushPlan {
    bool visible;
    int32_t destinationX;
    int32_t destinationY;
    uint32_t width;
    uint32_t height;
    uint32_t sourceStride;
    uint32_t sourceOffsetPixels;
    uint32_t sourcePixelCount;
};

constexpr ClippedFlushPlan makeClippedFlushPlan(
    const FlushArea area,
    const int16_t xOffset,
    const int16_t yOffset,
    const int32_t displayWidth,
    const int32_t displayHeight) {
    const int32_t sourceWidth = area.x2 - area.x1 + 1;
    const int32_t sourceHeight = area.y2 - area.y1 + 1;
    if (sourceWidth <= 0 || sourceHeight <= 0 ||
        displayWidth <= 0 || displayHeight <= 0) {
        return {false, 0, 0, 0, 0, 0, 0, 0};
    }

    const int32_t translatedX = area.x1 + xOffset;
    const int32_t translatedY = area.y1 + yOffset;
    const int32_t translatedRight = translatedX + sourceWidth;
    const int32_t translatedBottom = translatedY + sourceHeight;
    const int32_t clippedX0 = translatedX < 0 ? 0 : translatedX;
    const int32_t clippedY0 = translatedY < 0 ? 0 : translatedY;
    const int32_t clippedX1 =
        translatedRight > displayWidth ? displayWidth : translatedRight;
    const int32_t clippedY1 =
        translatedBottom > displayHeight ? displayHeight : translatedBottom;
    const uint32_t sourcePixels =
        static_cast<uint32_t>(sourceWidth) *
        static_cast<uint32_t>(sourceHeight);

    if (clippedX0 >= clippedX1 || clippedY0 >= clippedY1) {
        return {
            false, clippedX0, clippedY0, 0, 0,
            static_cast<uint32_t>(sourceWidth), 0, sourcePixels
        };
    }

    const uint32_t sourceColumn =
        static_cast<uint32_t>(clippedX0 - translatedX);
    const uint32_t sourceRow =
        static_cast<uint32_t>(clippedY0 - translatedY);
    return {
        true,
        clippedX0,
        clippedY0,
        static_cast<uint32_t>(clippedX1 - clippedX0),
        static_cast<uint32_t>(clippedY1 - clippedY0),
        static_cast<uint32_t>(sourceWidth),
        sourceRow * static_cast<uint32_t>(sourceWidth) + sourceColumn,
        sourcePixels
    };
}

template <typename Pixel, typename RowWriter, typename Completion>
void executeClippedFlush(const ClippedFlushPlan& plan,
                         Pixel* const source,
                         RowWriter&& writeRow,
                         Completion&& complete) {
    // WHY: the writer is a template callback, not std::function, so this path
    // has no allocation or runtime-polymorphism cost. Completion is structurally
    // called exactly once, including for a completely clipped rectangle.
    auto& writer = writeRow;
    auto& completion = complete;
    if (plan.visible) {
        for (uint32_t row = 0; row < plan.height; ++row) {
            Pixel* const rowSource =
                source + plan.sourceOffsetPixels + row * plan.sourceStride;
            writer(
                plan.destinationX,
                plan.destinationY + static_cast<int32_t>(row),
                plan.width,
                rowSource);
        }
    }
    completion();
}

constexpr bool clippedFlushPlanReadsWithinSource(
    const ClippedFlushPlan& plan) {
    if (!plan.visible) return true;
    const uint32_t finalExclusive =
        plan.sourceOffsetPixels +
        (plan.height - 1U) * plan.sourceStride +
        plan.width;
    return finalExclusive <= plan.sourcePixelCount;
}

} // namespace numos::display
