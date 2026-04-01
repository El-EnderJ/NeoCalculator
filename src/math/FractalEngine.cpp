#include "FractalEngine.h"
#include <cmath>
#include <new>

// ── Simple RGB565 macro ──
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

namespace {
constexpr int kAbortCheckInterval = 64;
constexpr float kRebaseThresholdSq = 64.0f;

struct LutEntry {
    float s;
    float c;
};

#ifdef ARDUINO
#include <esp_attr.h>
static DRAM_ATTR LutEntry gTrigLut[512];
#else
static LutEntry gTrigLut[512];
#endif

static bool gTrigLutInit = false;

inline void ensureTrigLut() {
    if (gTrigLutInit) return;
    constexpr float twoPi = 6.28318530717958647692f;
    for (int i = 0; i < 512; ++i) {
        float a = twoPi * (float)i / 512.0f;
        gTrigLut[i].s = sinf(a);
        gTrigLut[i].c = cosf(a);
    }
    gTrigLutInit = true;
}

inline void sinCosLut(float angle, float& s, float& c) {
    constexpr float twoPi = 6.28318530717958647692f;
    float norm = angle / twoPi;
    norm -= floorf(norm);
    int idx = (int)(norm * 512.0f) & 511;
    s = gTrigLut[idx].s;
    c = gTrigLut[idx].c;
}
}

void FractalEngine::renderMandelbrot(uint16_t* buffer, int width, int height, 
                                     float centerX, float centerY, float zoom, 
                                     int maxIter, int step, bool invertY) {
    ReferenceOrbit* orbit = new (std::nothrow) ReferenceOrbit();
    if (!orbit) return;

    buildReferenceOrbit(*orbit, (double)centerX, (double)centerY, maxIter);
    bool rebaseRequired = false;
    renderMandelbrotPerturbationStrip(
        buffer, width, height, centerX, centerY, zoom, maxIter,
        0, height, *orbit, nullptr, &rebaseRequired, step, invertY
    );
    
    delete orbit;
}

void FractalEngine::buildReferenceOrbit(ReferenceOrbit& orbit, double cRe, double cIm, int maxIter) {
    orbit.cRe = cRe;
    orbit.cIm = cIm;
    orbit.count = 0;

    double zRe = 0.0;
    double zIm = 0.0;
    int limit = maxIter;
    if (limit > ReferenceOrbit::MAX_POINTS) {
        limit = ReferenceOrbit::MAX_POINTS;
    }

    for (int i = 0; i < limit; ++i) {
        orbit.points[i].re = zRe;
        orbit.points[i].im = zIm;
        orbit.count++;

        double nextRe = zRe * zRe - zIm * zIm + cRe;
        double nextIm = 2.0 * zRe * zIm + cIm;
        zRe = nextRe;
        zIm = nextIm;

        if ((zRe * zRe + zIm * zIm) > 16.0) {
            break;
        }
    }
}

void FractalEngine::renderMandelbrotPerturbationStrip(
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
    int step,
    bool invertY
) {
    float aspect = (float)width / (float)height;
    float windowW = 3.0f / zoom;
    float windowH = windowW / aspect;
    float xMin = centerX - windowW * 0.5f;
    float yMin = centerY - windowH * 0.5f;
    float dx = windowW / (float)width;
    float dy = windowH / (float)height;
    int checkCounter = 0;
    int orbitCount = orbit.count;
    if (orbitCount <= 0) {
        return;
    }
    int effectiveIter = maxIter < orbitCount ? maxIter : orbitCount;

    // Use a fast local SRAM line buffer to prevent PSRAM single-write bottlenecks
    uint16_t rowBuffer[320];

    for (int py = yStart; py < yEnd; py += step) {
        float cy = yMin + py * dy;
        
        // Horizontal pixel processing
        for (int px = 0; px < width; px += step) {
            if (abortRequested && (++checkCounter >= kAbortCheckInterval)) {
                checkCounter = 0;
                if (*abortRequested) {
                    return;
                }
            }

            float cx = xMin + px * dx;
            float dcRe = cx - (float)orbit.cRe;
            float dcIm = cy - (float)orbit.cIm;
            float dzRe = 0.0f;
            float dzIm = 0.0f;
            int iter = 0;
            bool escaped = false;

            while (iter < effectiveIter) {
                float zr = (float)orbit.points[iter].re;
                float zi = (float)orbit.points[iter].im;

                float twoZDzRe = 2.0f * (zr * dzRe - zi * dzIm);
                float twoZDzIm = 2.0f * (zr * dzIm + zi * dzRe);
                float dz2Re = dzRe * dzRe - dzIm * dzIm;
                float dz2Im = 2.0f * dzRe * dzIm;

                dzRe = twoZDzRe + dz2Re + dcRe;
                dzIm = twoZDzIm + dz2Im + dcIm;

                float absSq = dzRe * dzRe + dzIm * dzIm;
                if (rebaseRequired && absSq > kRebaseThresholdSq) {
                    *rebaseRequired = true;
                }

                // Optimization: Branch hint for escape condition
                float pixelRe = zr + dzRe;
                float pixelIm = zi + dzIm;
                if (__builtin_expect((pixelRe * pixelRe + pixelIm * pixelIm) > 4.0f, 0)) {
                    escaped = true;
                    break;
                }
                ++iter;
            }
            if (!escaped && iter >= effectiveIter) {
                iter = maxIter;
            }
            
            uint16_t color = mapColor(iter, maxIter);
            
            // Replicate horizontally in local buffer
            for (int dxP = 0; dxP < step && (px + dxP) < width; ++dxP) {
                rowBuffer[px + dxP] = color;
            }
        }
        
        // Blocky replication vertically and fast commit to PSRAM
        for (int stepY = 0; stepY < step && (py + stepY) < yEnd; ++stepY) {
            int targetY = py + stepY;
            int screenY = invertY ? (height - 1 - targetY) : targetY;
            memcpy(&buffer[screenY * width], rowBuffer, width * sizeof(uint16_t));
        }
    }
}

void FractalEngine::renderMandelbulbSliceStrip(
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
    int step,
    bool invertY
) {
    ensureTrigLut();

    float aspect = (float)width / (float)height;
    float windowW = 3.0f / zoom;
    float windowH = windowW / aspect;
    float xMin = centerX - windowW * 0.5f;
    float yMin = centerY - windowH * 0.5f;
    float dx = windowW / (float)width;
    float dy = windowH / (float)height;
    int checkCounter = 0;

    // Use a fast local SRAM line buffer
    uint16_t rowBuffer[320];

    for (int py = yStart; py < yEnd; py += step) {
        float y0 = yMin + py * dy;
        for (int px = 0; px < width; px += step) {
            if (abortRequested && (++checkCounter >= kAbortCheckInterval)) {
                checkCounter = 0;
                if (*abortRequested) return;
            }

            float x0 = xMin + px * dx;
            float z0 = sliceZ;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            int iter = 0;

            while (iter < maxIter) {
                float r2 = x * x + y * y + z * z;
                if (__builtin_expect(r2 > 4.0f, 0)) break;
                float r = sqrtf(r2);
                if (__builtin_expect(r < 1e-6f, 0)) {
                    x = x0;
                    y = y0;
                    z = z0;
                    ++iter;
                    continue;
                }

                float theta = acosf(z / r);
                float phi = atan2f(y, x);
                float rn = powf(r, (float)power);
                float pTheta = (float)power * theta;
                float pPhi = (float)power * phi;
                float sinTheta, cosTheta, sinPhi, cosPhi;
                sinCosLut(pTheta, sinTheta, cosTheta);
                sinCosLut(pPhi, sinPhi, cosPhi);

                x = rn * sinTheta * cosPhi + x0;
                y = rn * sinTheta * sinPhi + y0;
                z = rn * cosTheta + z0;
                ++iter;
            }

            uint16_t color = mapColor(iter, maxIter);
            
            // Replicate horizontally in local buffer
            for (int dxP = 0; dxP < step && (px + dxP) < width; ++dxP) {
                rowBuffer[px + dxP] = color;
            }
        }
        
        // Blocky replication vertically and fast commit to PSRAM
        for (int stepY = 0; stepY < step && (py + stepY) < yEnd; ++stepY) {
            int targetY = py + stepY;
            int screenY = invertY ? (height - 1 - targetY) : targetY;
            memcpy(&buffer[screenY * width], rowBuffer, width * sizeof(uint16_t));
        }
    }
}

uint16_t FractalEngine::mapColor(int iter, int maxIter) {
    if (iter == maxIter) return 0x0000; // Black for interior

    // Simple smooth coloring: continuous base offset combined with some modulo
    // We create a basic multi-gradient palette
    float t = (float)iter / (float)maxIter;
    
    // Create a psychedelic cyan/blue/white/black gradient like typical Mandelbrot sets
    uint8_t r = (uint8_t)(9.0f * (1.0f - t) * t * t * t * 255.0f);
    uint8_t g = (uint8_t)(15.0f * (1.0f - t) * (1.0f - t) * t * t * 255.0f);
    uint8_t b = (uint8_t)(8.5f * (1.0f - t) * (1.0f - t) * (1.0f - t) * t * 255.0f);

    return rgb565(r, g, b);
}
