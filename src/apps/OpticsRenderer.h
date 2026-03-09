/**
 * OpticsRenderer.h
 * OpticsLab — Custom canvas drawing functions for LVGL RGB565 buffer.
 *
 * All functions operate on a raw uint16_t* buffer (RGB565, row-major).
 * Buffer coordinates start at the TOP of the draw area (below the StatusBar).
 *
 * Drawing layout in buffer coordinates:
 *   Rows   0 … VIEWPORT_H-1  : ray-tracing viewport (156 px)
 *   Rows   VIEWPORT_H … BUF_H-1 : telemetry bar   ( 60 px)
 *
 * Colour helpers use 24-bit → 16-bit RGB565 conversion.
 */

#pragma once

#include <cstdint>
#include <cmath>

// ── Buffer / layout constants ───────────────────────────────────────────────
static constexpr int OPT_BUF_W      = 320;
static constexpr int OPT_BUF_H      = 216;  // SCREEN_H(240) - STATUS_H(24)
static constexpr int OPT_VIEWPORT_H = 156;  // 180px total - 24px status bar
static constexpr int OPT_TELEMETRY_H = 60;
static constexpr int OPT_AXIS_ROW   = OPT_VIEWPORT_H / 2;  // 78 — optical axis

// ── Colour palette ──────────────────────────────────────────────────────────
// All as RGB888 → converted at use time
static constexpr uint32_t OPT_COL_BG        = 0x000000;  // deep black
static constexpr uint32_t OPT_COL_AXIS      = 0x404040;  // dark grey
static constexpr uint32_t OPT_COL_RAY_NORM  = 0xFFD700;  // golden yellow
static constexpr uint32_t OPT_COL_RAY_TIR   = 0xFF2020;  // TIR red
static constexpr uint32_t OPT_COL_LENS_FILL = 0x003C3C;  // dark cyan fill
static constexpr uint32_t OPT_COL_LENS_EDGE = 0x00CCCC;  // ice-blue edge
static constexpr uint32_t OPT_COL_SCREEN    = 0x505050;  // dim grey screen
static constexpr uint32_t OPT_COL_OBJECT    = 0x00FF80;  // green arrow
static constexpr uint32_t OPT_COL_SELECTED  = 0xFFFFFF;  // white highlight
static constexpr uint32_t OPT_COL_HPLANE    = 0xFF8800;  // orange (H1,H2)
static constexpr uint32_t OPT_COL_TEXT      = 0xCCCCCC;
static constexpr uint32_t OPT_COL_TELE_BG   = 0x0A0A0A;

// ───────────────────────────────────────────────────────────────────────────
// Colour helpers
// ───────────────────────────────────────────────────────────────────────────
inline uint16_t optRGB(uint32_t rgb888) {
    uint8_t r = (rgb888 >> 16) & 0xFF;
    uint8_t g = (rgb888 >>  8) & 0xFF;
    uint8_t b =  rgb888        & 0xFF;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

inline uint16_t optRGB(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/// Blend two RGB565 colours with alpha (0–255).
inline uint16_t optBlend(uint16_t bg, uint16_t fg, uint8_t alpha) {
    if (alpha == 0)   return bg;
    if (alpha == 255) return fg;
    uint32_t rb_bg = bg & 0xF81F, g_bg = bg & 0x07E0;
    uint32_t rb_fg = fg & 0xF81F, g_fg = fg & 0x07E0;
    uint8_t ia = 255 - alpha;
    uint32_t rb = (rb_bg * ia + rb_fg * alpha) >> 8;
    uint32_t g  = (g_bg  * ia + g_fg  * alpha) >> 8;
    return (uint16_t)((rb & 0xF81F) | (g & 0x07E0));
}

// ───────────────────────────────────────────────────────────────────────────
// Pixel-level drawing
// ───────────────────────────────────────────────────────────────────────────

/// Safe pixel write (bounds-checked).
inline void optPutPixel(uint16_t* buf, int x, int y, uint16_t col) {
    if ((unsigned)x < (unsigned)OPT_BUF_W &&
        (unsigned)y < (unsigned)OPT_BUF_H) {
        buf[y * OPT_BUF_W + x] = col;
    }
}

/// Pixel write with alpha blend.
inline void optPutPixelA(uint16_t* buf, int x, int y, uint16_t col, uint8_t alpha) {
    if ((unsigned)x < (unsigned)OPT_BUF_W &&
        (unsigned)y < (unsigned)OPT_BUF_H) {
        int idx = y * OPT_BUF_W + x;
        buf[idx] = optBlend(buf[idx], col, alpha);
    }
}

// ───────────────────────────────────────────────────────────────────────────
// Bresenham line
// ───────────────────────────────────────────────────────────────────────────
inline void optDrawLine(uint16_t* buf, int x0, int y0, int x1, int y1,
                         uint16_t col) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        optPutPixel(buf, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/// Dotted line (every other pixel).
inline void optDrawDotted(uint16_t* buf, int x0, int y0, int x1, int y1,
                           uint16_t col, int period = 8) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int step = 0;
    for (;;) {
        if (step % period < (period / 2)) optPutPixel(buf, x0, y0, col);
        ++step;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/// Horizontal fill strip (clipped to buffer bounds).
inline void optHLine(uint16_t* buf, int y, int x0, int x1, uint16_t col) {
    if ((unsigned)y >= (unsigned)OPT_BUF_H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= OPT_BUF_W) x1 = OPT_BUF_W - 1;
    uint16_t* row = buf + y * OPT_BUF_W;
    for (int x = x0; x <= x1; ++x) row[x] = col;
}

/// Vertical line.
inline void optVLine(uint16_t* buf, int x, int y0, int y1, uint16_t col) {
    if ((unsigned)x >= (unsigned)OPT_BUF_W) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= OPT_BUF_H) y1 = OPT_BUF_H - 1;
    for (int y = y0; y <= y1; ++y) buf[y * OPT_BUF_W + x] = col;
}

// ───────────────────────────────────────────────────────────────────────────
// Fill a rectangular region.
// ───────────────────────────────────────────────────────────────────────────
inline void optFillRect(uint16_t* buf,
                         int x, int y, int w, int h, uint16_t col) {
    int x1 = x + w - 1, y1 = y + h - 1;
    if (x1 < 0 || x >= OPT_BUF_W || y1 < 0 || y >= OPT_BUF_H) return;
    if (x  < 0)          x  = 0;
    if (y  < 0)          y  = 0;
    if (x1 >= OPT_BUF_W) x1 = OPT_BUF_W - 1;
    if (y1 >= OPT_BUF_H) y1 = OPT_BUF_H - 1;
    for (int row = y; row <= y1; ++row) {
        uint16_t* p = buf + row * OPT_BUF_W + x;
        for (int col_x = x; col_x <= x1; ++col_x) *p++ = col;
    }
}

// ───────────────────────────────────────────────────────────────────────────
// Draw a thick lens at pixel position xPx (centre column), spanning rows
// yTop … yBot in the viewport.
//
// The lens profile is drawn as two arcs (approximated by filling a
// region between two circular arcs with a translucent fill).
// ───────────────────────────────────────────────────────────────────────────
inline void optDrawLens(uint16_t* buf,
                         int xPx, int yAxis,  // axis row in buffer
                         float R1_mm, float R2_mm,
                         float halfH_mm, float scale_y,  // mm → px
                         float thickness_px,
                         bool selected) {
    uint16_t edgeCol = optRGB(OPT_COL_LENS_EDGE);
    uint16_t fillCol = optRGB(OPT_COL_LENS_FILL);
    if (selected) edgeCol = optRGB(OPT_COL_SELECTED);

    int aperPx = (int)(halfH_mm * scale_y);
    int thickPx = (int)thickness_px;
    if (thickPx < 2) thickPx = 2;

    // For each row in the aperture, compute x-extent of the lens
    for (int dy = -aperPx; dy <= aperPx; ++dy) {
        int row = yAxis + dy;
        if (row < 0 || row >= OPT_BUF_H) continue;
        float y_mm = (float)dy / scale_y;

        // Sag of front surface (x shifts right for biconvex R1 > 0)
        float sag1 = 0.0f;
        if (fabsf(R1_mm) > 0.01f && fabsf(y_mm) < fabsf(R1_mm)) {
            float under = R1_mm * R1_mm - y_mm * y_mm;
            if (under >= 0.0f)
                sag1 = R1_mm - (R1_mm > 0 ? 1 : -1) * sqrtf(under);
        }
        // Sag of rear surface
        float sag2 = 0.0f;
        if (fabsf(R2_mm) > 0.01f && fabsf(y_mm) < fabsf(R2_mm)) {
            float under = R2_mm * R2_mm - y_mm * y_mm;
            if (under >= 0.0f)
                sag2 = R2_mm - (R2_mm > 0 ? 1 : -1) * sqrtf(under);
        }

        // pixel x range
        // Front surface at xPx + sag1 * scale_x (scale_x ≈ same unit here)
        // We use scale_y as approx for scale_x — caller should pass matched scale
        // The centre of the lens body runs from xPx to xPx + thickPx
        int x_front = xPx + (int)(sag1 * scale_y * 0.5f); // approximate scale
        int x_rear  = xPx + thickPx + (int)(sag2 * scale_y * 0.5f);
        if (x_front > x_rear) { int t = x_front; x_front = x_rear; x_rear = t; }

        // Fill lens body
        for (int col_x = x_front; col_x <= x_rear; ++col_x) {
            if ((unsigned)col_x < (unsigned)OPT_BUF_W) {
                int idx = row * OPT_BUF_W + col_x;
                buf[idx] = optBlend(buf[idx], fillCol, 120);
            }
        }

        // Edge lines
        optPutPixel(buf, x_front, row, edgeCol);
        optPutPixel(buf, x_rear,  row, edgeCol);
    }

    // Draw aperture tick marks at the top and bottom
    optPutPixel(buf, xPx,           yAxis - aperPx, edgeCol);
    optPutPixel(buf, xPx,           yAxis + aperPx, edgeCol);
    optPutPixel(buf, xPx + thickPx, yAxis - aperPx, edgeCol);
    optPutPixel(buf, xPx + thickPx, yAxis + aperPx, edgeCol);
}

// ───────────────────────────────────────────────────────────────────────────
// Draw an object arrow (upward-pointing, centred on axis).
// ───────────────────────────────────────────────────────────────────────────
inline void optDrawObjectArrow(uint16_t* buf,
                                int xPx, int yAxis,
                                float objH_mm, float scale_y,
                                bool selected) {
    uint16_t col = selected ? optRGB(OPT_COL_SELECTED) : optRGB(OPT_COL_OBJECT);
    int tipPx = yAxis - (int)(objH_mm * scale_y);
    // Shaft
    optVLine(buf, xPx, tipPx, yAxis, col);
    // Arrowhead (3 px wide at tip)
    optPutPixel(buf, xPx - 1, tipPx + 1, col);
    optPutPixel(buf, xPx + 1, tipPx + 1, col);
    optPutPixel(buf, xPx - 2, tipPx + 2, col);
    optPutPixel(buf, xPx + 2, tipPx + 2, col);
}

// ───────────────────────────────────────────────────────────────────────────
// Draw a screen (dashed vertical line).
// ───────────────────────────────────────────────────────────────────────────
inline void optDrawScreen(uint16_t* buf,
                           int xPx, int yAxis,
                           float halfH_mm, float scale_y,
                           bool selected) {
    uint16_t col = selected ? optRGB(OPT_COL_SELECTED) : optRGB(OPT_COL_SCREEN);
    int halfPx = (int)(halfH_mm * scale_y);
    for (int dy = -halfPx; dy <= halfPx; ++dy) {
        if (abs(dy) % 4 < 2)
            optPutPixel(buf, xPx, yAxis + dy, col);
    }
}
