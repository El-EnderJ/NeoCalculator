/**
 * GraphView.h — Grapher MVC: View layer (Kandinsky pixel engine)
 *
 * Operates on a raw RGB565 pixel buffer (the "Kandinsky" buffer allocated
 * in PSRAM).  Provides all graph rendering: grid, axes, function curves,
 * integral area shading, and tangent-line overlay.
 *
 * This module knows nothing about LVGL widgets, AST nodes, or expression
 * text.  It receives data as float arrays / std::function lambdas and
 * writes pixels directly (analogous to Escher/Kandinsky in NumWorks).
 */
#pragma once

#include <cstdint>
#include <cmath>
#include <functional>

namespace grapher {

/// Lightweight integer screen point (saves stack vs lv_point_precise_t).
struct PlotPt { int16_t x, y; };

/**
 * GraphView — pixel-level graph rendering engine.
 *
 * Usage pattern:
 *   1. Call init() once after the PSRAM buffer is allocated.
 *   2. Call clearWhite() + drawGridAndAxes() + drawCurve() each replot.
 *   3. Call drawAreaUnderCurve() / drawTangent() for overlays.
 *   4. Push _buf to LVGL via lv_image_set_src().
 */
class GraphView {
public:
    /// Maximum number of adaptive plot points per function.
    static constexpr int MAX_PLOT_PTS  = 512;
    /// Initial coarse-grid samples before adaptive refinement.
    static constexpr int INIT_SAMPLE_N = 40;
    /// Max recursive subdivision depth for adaptive sampling.
    static constexpr int ADAPT_DEPTH   = 3;
    /// Pixel error threshold that triggers subdivision.
    static constexpr float ADAPT_THRESH = 2.0f;

    /**
     * Bind the view to an RGB565 buffer.
     * Must be called before any draw method.
     */
    void init(uint16_t* buf, int w, int h);

    /// Fill the entire buffer with white (0xFFFF RGB565).
    void clearWhite();

    /**
     * Draw sub-grid, main grid and coordinate axes into the buffer.
     * @param xMin/xMax/yMin/yMax  World-space viewport extents.
     */
    void drawGridAndAxes(float xMin, float xMax, float yMin, float yMax);

    /**
     * Adaptively sample fn over [xMin, xMax] into pts[].
     * Returns the number of valid points written (≤ maxN).
     */
    int sampleAdaptive(const std::function<float(float)>& fn,
                       float xMin, float xMax, float yMin, float yMax,
                       PlotPt* pts, int maxN);

    /// Draw a polyline through pts[0..count-1] in the given RGB888 colour.
    void drawCurve(const PlotPt* pts, int count, uint32_t color);

    /**
     * Shade the area between f(x) and y=0 over [x0, x1].
     *
     * Uses a checkerboard stipple pattern to simulate 50 % opacity,
     * since the RGB565 buffer has no alpha channel.
     *
     * @param fn      Function to shade under (or over for negative regions)
     * @param x0,x1   Shading X bounds in world space
     * @param color   Fill colour (RGB888) — stippled at ~50 % opacity
     */
    void drawAreaUnderCurve(const std::function<float(float)>& fn,
                            float x0, float x1,
                            float xMin, float xMax, float yMin, float yMax,
                            uint32_t color);

    /**
     * Draw the tangent line to fn at xTarget across the full viewport.
     *
     * Computes the numerical derivative using a central-difference
     * formula (h = max(|xTarget|·1e-4, 1e-5)) and draws the resulting
     * straight line from the left to the right edge of the buffer.
     *
     * @param fn        Function to differentiate
     * @param xTarget   Point of tangency in world space
     * @param color     Line colour (RGB888)
     */
    void drawTangent(const std::function<float(float)>& fn, float xTarget,
                     float xMin, float xMax, float yMin, float yMax,
                     uint32_t color);

    // ── Static low-level helpers (public so GrapherApp can call them) ──

    /// Convert a packed RGB888 value to RGB565.
    static uint16_t rgb888to565(uint32_t rgb);

    /**
     * Bresenham line rasteriser with trivial-rejection clipping.
     * Writes directly into buf[].  Thread-unsafe (must hold LVGL mutex).
     */
    static void fastDrawLine(uint16_t* buf, int bufW, int bufH,
                             int x0, int y0, int x1, int y1, uint16_t color);

private:
    uint16_t* _buf = nullptr;
    int _w = 0, _h = 0;

    /// Recursive adaptive subdivision helper (called by sampleAdaptive).
    void adaptSeg(const std::function<float(float)>& fn,
                  float xMin, float xRange, float yMin, float yRange,
                  float wx0, float sy0, float wx1, float sy1,
                  int depth, PlotPt* pts, int& n, int maxN);
};

} // namespace grapher
