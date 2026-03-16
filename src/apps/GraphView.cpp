/**
 * GraphView.cpp — Grapher MVC: View / pixel-engine implementation
 *
 * All methods operate directly on the RGB565 buffer supplied via init().
 * This module contains NO LVGL widget creation or destruction — it only
 * reads/writes the raw pixel buffer.
 */
#include "GraphView.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace grapher {

// ── Layout colours (must match GrapherApp.cpp palette) ─────────────────
static constexpr uint32_t COL_GRID_SUB  = 0xF0F0F0;
static constexpr uint32_t COL_GRID_MAIN = 0xE0E0E0;
static constexpr uint32_t COL_AXIS      = 0x333333;

// ── Nice grid step: 1, 2, 5 × 10^n ────────────────────────────────────
static float niceStep(float range, int maxTicks) {
    if (range <= 0.0f || maxTicks <= 0) return 1.0f;
    float rough = range / (float)maxTicks;
    float mag   = powf(10.0f, floorf(log10f(rough)));
    float norm  = rough / mag;
    float nice;
    if      (norm < 1.5f) nice = 1.0f;
    else if (norm < 3.5f) nice = 2.0f;
    else if (norm < 7.5f) nice = 5.0f;
    else                  nice = 10.0f;
    return nice * mag;
}

// ═══════════════════════════════════════════════════════════════════════
// Static pixel helpers
// ═══════════════════════════════════════════════════════════════════════

uint16_t GraphView::rgb888to565(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void GraphView::fastDrawLine(uint16_t* buf, int bufW, int bufH,
                             int x0, int y0, int x1, int y1, uint16_t color)
{
    // Trivial rejection: both endpoints outside same edge
    if ((x0 < 0 && x1 < 0) || (x0 >= bufW && x1 >= bufW) ||
        (y0 < 0 && y1 < 0) || (y0 >= bufH && y1 >= bufH)) return;

    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    if ((unsigned)x0 < (unsigned)bufW && (unsigned)x1 < (unsigned)bufW &&
        (unsigned)y0 < (unsigned)bufH && (unsigned)y1 < (unsigned)bufH) {
        // Fast path: no per-pixel bounds check needed
        for (;;) {
            buf[y0 * bufW + x0] = color;
            if (x0 == x1 && y0 == y1) break;
            int e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    } else {
        // Clipping path
        for (;;) {
            if ((unsigned)x0 < (unsigned)bufW && (unsigned)y0 < (unsigned)bufH)
                buf[y0 * bufW + x0] = color;
            if (x0 == x1 && y0 == y1) break;
            int e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Initialisation
// ═══════════════════════════════════════════════════════════════════════

void GraphView::init(uint16_t* buf, int w, int h) {
    _buf = buf;
    _w   = w;
    _h   = h;
}

// ═══════════════════════════════════════════════════════════════════════
// Buffer-level operations
// ═══════════════════════════════════════════════════════════════════════

void GraphView::clearWhite() {
    if (!_buf || _w <= 0 || _h <= 0) return;
    // 0xFF in every byte → 0xFFFF per RGB565 pixel = white
    memset(_buf, 0xFF, (size_t)_w * _h * sizeof(uint16_t));
}

void GraphView::drawGridAndAxes(float xMin, float xMax, float yMin, float yMax) {
    if (!_buf || _w <= 0 || _h <= 0) return;
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    if (xRange <= 0 || yRange <= 0) return;

    auto toSX = [&](float wx) -> int { return (int)((wx - xMin) / xRange * _w); };
    auto toSY = [&](float wy) -> int { return (int)((1.0f - (wy - yMin) / yRange) * _h); };

    // ── Sub-grid ────────────────────────────────────────────────────────
    {
        uint16_t col = rgb888to565(COL_GRID_SUB);
        float mainStep = niceStep(xRange, 8);
        float subStep  = mainStep / 5.0f;

        float start = floorf(xMin / subStep) * subStep;
        for (float v = start; v <= xMax; v += subStep) {
            int sx = toSX(v);
            fastDrawLine(_buf, _w, _h, sx, 0, sx, _h - 1, col);
        }
        float yMainStep = niceStep(yRange, 6);
        float ySubStep  = yMainStep / 5.0f;
        start = floorf(yMin / ySubStep) * ySubStep;
        for (float v = start; v <= yMax; v += ySubStep) {
            int sy = toSY(v);
            fastDrawLine(_buf, _w, _h, 0, sy, _w - 1, sy, col);
        }
    }

    // ── Main grid ───────────────────────────────────────────────────────
    {
        uint16_t col = rgb888to565(COL_GRID_MAIN);
        float mainStep = niceStep(xRange, 8);
        float start = floorf(xMin / mainStep) * mainStep;
        for (float v = start; v <= xMax; v += mainStep) {
            int sx = toSX(v);
            fastDrawLine(_buf, _w, _h, sx, 0, sx, _h - 1, col);
        }
        float yMainStep = niceStep(yRange, 6);
        start = floorf(yMin / yMainStep) * yMainStep;
        for (float v = start; v <= yMax; v += yMainStep) {
            int sy = toSY(v);
            fastDrawLine(_buf, _w, _h, 0, sy, _w - 1, sy, col);
        }
    }

    // ── Axes ────────────────────────────────────────────────────────────
    {
        uint16_t col = rgb888to565(COL_AXIS);
        // X-axis (y=0)
        int ay = toSY(0.0f);
        fastDrawLine(_buf, _w, _h, 0, ay, _w - 1, ay, col);
        // Y-axis (x=0)
        int ax = toSX(0.0f);
        fastDrawLine(_buf, _w, _h, ax, 0, ax, _h - 1, col);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Adaptive sampling
// ═══════════════════════════════════════════════════════════════════════

void GraphView::adaptSeg(const std::function<float(float)>& fn,
                         float xMin, float xRange,
                         float yMin, float yRange,
                         float wx0, float sy0,
                         float wx1, float sy1,
                         int depth, PlotPt* pts, int& n, int maxN)
{
    if (depth <= 0 || n >= maxN - 1) return;
    float mwx = (wx0 + wx1) * 0.5f;
    float mwy = fn(mwx);
    if (std::isnan(mwy) || std::isinf(mwy)) return;
    float msy = (1.0f - (mwy - yMin) / yRange) * _h;
    float interpSy = (sy0 + sy1) * 0.5f;
    if (fabsf(msy - interpSy) > ADAPT_THRESH) {
        float msx = (mwx - xMin) / xRange * _w;
        adaptSeg(fn, xMin, xRange, yMin, yRange,
                 wx0, sy0, mwx, msy, depth - 1, pts, n, maxN);
        if (n < maxN) {
            pts[n++] = { (int16_t)(int)msx, (int16_t)(int)msy };
        }
        adaptSeg(fn, xMin, xRange, yMin, yRange,
                 mwx, msy, wx1, sy1, depth - 1, pts, n, maxN);
    }
}

int GraphView::sampleAdaptive(const std::function<float(float)>& fn,
                              float xMin, float xMax,
                              float yMin, float yMax,
                              PlotPt* pts, int maxN)
{
    if (!_buf || _w <= 0 || _h <= 0 || !pts || maxN < 2) return 0;
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    if (xRange <= 0 || yRange <= 0) return 0;

    // Coarse sample grid
    struct CoarsePt { float wx, sy, sx; bool ok; };
    static CoarsePt coarse[INIT_SAMPLE_N + 1];

    float step = xRange / INIT_SAMPLE_N;
    for (int i = 0; i <= INIT_SAMPLE_N; ++i) {
        float wx = xMin + i * step;
        float wy = fn(wx);
        bool ok = !std::isnan(wy) && !std::isinf(wy);
        float sy = 0, sx = (wx - xMin) / xRange * _w;
        if (ok) {
            sy = (1.0f - (wy - yMin) / yRange) * _h;
            if (sy < -(float)_h || sy > 2.0f * _h) ok = false;
        }
        coarse[i] = { wx, sy, sx, ok };
    }

    int n = 0;
    bool prevOk = false;
    for (int i = 0; i <= INIT_SAMPLE_N && n < maxN; ++i) {
        if (!coarse[i].ok) { prevOk = false; continue; }
        if (!prevOk) {
            pts[n++] = { (int16_t)(int)coarse[i].sx, (int16_t)(int)coarse[i].sy };
        } else {
            adaptSeg(fn, xMin, xRange, yMin, yRange,
                     coarse[i - 1].wx, coarse[i - 1].sy,
                     coarse[i].wx,     coarse[i].sy,
                     ADAPT_DEPTH, pts, n, maxN);
            if (n < maxN) {
                pts[n++] = { (int16_t)(int)coarse[i].sx, (int16_t)(int)coarse[i].sy };
            }
        }
        prevOk = true;
    }
    return n;
}

// ═══════════════════════════════════════════════════════════════════════
// Curve rendering
// ═══════════════════════════════════════════════════════════════════════

void GraphView::drawCurve(const PlotPt* pts, int count, uint32_t color) {
    if (!_buf || !pts || count < 2) return;
    uint16_t col = rgb888to565(color);
    for (int k = 1; k < count; ++k) {
        fastDrawLine(_buf, _w, _h,
                     pts[k-1].x, pts[k-1].y,
                     pts[k].x,   pts[k].y, col);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Integral area shading (Kandinsky stipple)
// ═══════════════════════════════════════════════════════════════════════

void GraphView::drawAreaUnderCurve(const std::function<float(float)>& fn,
                                   float x0, float x1,
                                   float xMin, float xMax,
                                   float yMin, float yMax,
                                   uint32_t color)
{
    if (!_buf || _w <= 0 || _h <= 0) return;
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    if (xRange <= 0 || yRange <= 0) return;

    // Shade colour in RGB565
    uint16_t col = rgb888to565(color);

    // Y-axis pixel (y=0 in world)
    int yAxisPx = (int)((1.0f - (0.0f - yMin) / yRange) * _h);

    for (int px = 0; px < _w; ++px) {
        float wx = xMin + (float)px / _w * xRange;
        if (wx < x0 || wx > x1) continue;

        float wy = fn(wx);
        if (std::isnan(wy) || std::isinf(wy)) continue;

        int sy = (int)((1.0f - (wy - yMin) / yRange) * _h);

        // Clamp to buffer
        int yA = std::max(0, std::min(yAxisPx, _h - 1));
        int yB = std::max(0, std::min(sy, _h - 1));
        if (yA > yB) std::swap(yA, yB);
        if (yA == yB) continue;

        // Stipple: every other pixel in a checkerboard pattern for ~50 % opacity
        for (int py = yA; py <= yB; ++py) {
            if (((px + py) & 1) == 0) {
                _buf[py * _w + px] = col;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Tangent line drawing
// ═══════════════════════════════════════════════════════════════════════

void GraphView::drawTangent(const std::function<float(float)>& fn, float xTarget,
                            float xMin, float xMax, float yMin, float yMax,
                            uint32_t color)
{
    if (!_buf || _w <= 0 || _h <= 0) return;
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    if (xRange <= 0 || yRange <= 0) return;

    // Numerical derivative: central difference
    float h = std::max(fabsf(xTarget) * 1e-4f, 1e-5f);
    float yPlus  = fn(xTarget + h);
    float yMinus = fn(xTarget - h);
    if (std::isnan(yPlus) || std::isnan(yMinus) ||
        std::isinf(yPlus) || std::isinf(yMinus)) return;

    float slope  = (yPlus - yMinus) / (2.0f * h);
    float yAtX   = fn(xTarget);
    if (std::isnan(yAtX) || std::isinf(yAtX)) return;

    // Tangent line: y = yAtX + slope * (wx - xTarget)
    // Evaluate at the left and right viewport edges
    float yLeft  = yAtX + slope * (xMin - xTarget);
    float yRight = yAtX + slope * (xMax - xTarget);

    // World → screen
    int sx0 = 0;
    int sy0 = (int)((1.0f - (yLeft  - yMin) / yRange) * _h);
    int sx1 = _w - 1;
    int sy1 = (int)((1.0f - (yRight - yMin) / yRange) * _h);

    uint16_t col = rgb888to565(color);
    fastDrawLine(_buf, _w, _h, sx0, sy0, sx1, sy1, col);
}

} // namespace grapher
