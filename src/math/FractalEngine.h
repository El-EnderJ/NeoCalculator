#pragma once

#include <cstdint>

class FractalEngine {
public:
    struct ReferenceOrbitPoint {
        double re;
        double im;
    };

    struct ReferenceOrbit {
        static constexpr int MAX_POINTS = 2048;
        ReferenceOrbitPoint points[MAX_POINTS];
        int count = 0;
        double cRe = 0.0;
        double cIm = 0.0;
    };

    /**
     * Renders a Mandelbrot fractal to a 1D pixel buffer.
     * Uses perturbation with a high precision reference orbit and float32 deltas.
     * @param buffer 1D RGB565 pixel buffer (height * width size)
     * @param width  Buffer width in pixels
     * @param height Buffer height in pixels
     * @param centerX Center X on complex plane
     * @param centerY Center Y on complex plane
     * @param zoom Zoom level (larger = more zoomed in)
     * @param maxIter Maximum iterations for escape time
     * @param invertY Whether to invert the Y axis (default true for standard graphics coordinates)
     */
    static void renderMandelbrot(uint16_t* buffer, int width, int height, 
                                 float centerX, float centerY, float zoom, 
                                 int maxIter, bool invertY = true);

    /**
     * Builds the high precision reference orbit for perturbation.
     */
    static void buildReferenceOrbit(ReferenceOrbit& orbit, double cRe, double cIm, int maxIter);

    /**
     * Renders a strip [yStart, yEnd) with perturbation.
     * Checks abortRequested every N pixels and signals rebaseRequired when delta diverges.
     */
    static void renderMandelbrotPerturbationStrip(
        uint16_t* buffer,
        int width,
        int height,
        float centerX,
        float centerY,
        float zoom,
        int maxIter,
        int yStart,
        int yEnd,
        ReferenceOrbit& orbit,
        volatile bool* abortRequested,
        volatile bool* rebaseRequired,
        bool invertY = true
    );

    /**
     * Renders a Mandelbulb 2D slice strip [yStart, yEnd).
     */
    static void renderMandelbulbSliceStrip(
        uint16_t* buffer,
        int width,
        int height,
        float centerX,
        float centerY,
        float sliceZ,
        float zoom,
        int maxIter,
        int power,
        int yStart,
        int yEnd,
        volatile bool* abortRequested,
        bool invertY = true
    );
    
    /**
     * Maps an iteration count to an RGB565 color.
     */
    static uint16_t mapColor(int iter, int maxIter);
};
