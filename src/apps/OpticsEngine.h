/**
 * OpticsEngine.h
 * OpticsLab Physics Core — Dual-engine optical simulator for NumOS.
 *
 * Engine A: Exact Ray Tracing with vector Snell's Law + TIR detection.
 * Engine B: Paraxial Analysis via ABCD (ray transfer) matrices.
 *           Computes EFL, BFL, magnification, and principal planes.
 *
 * Thick-lens model: principal planes (H1, H2) are computed explicitly.
 * No thin-lens approximations are used.
 *
 * Sign convention for radii (standard optics):
 *   R > 0  → centre of curvature to the RIGHT of the surface.
 *   R < 0  → centre of curvature to the LEFT of the surface.
 */

#pragma once

#include <cmath>
#include <cstring>
#include <cstdint>

// ── Physical infinity placeholder ──────────────────────────────────────────
static constexpr float OPT_INF = 1e9f;   // "flat" surface (R → ∞)

// ───────────────────────────────────────────────────────────────────────────
// 2-D Vector
// ───────────────────────────────────────────────────────────────────────────
struct Vector2D {
    float x = 0.0f, y = 0.0f;

    Vector2D() = default;
    Vector2D(float x_, float y_) : x(x_), y(y_) {}

    Vector2D operator+(const Vector2D& o) const { return {x + o.x, y + o.y}; }
    Vector2D operator-(const Vector2D& o) const { return {x - o.x, y - o.y}; }
    Vector2D operator*(float s)           const { return {x * s, y * s}; }

    float dot(const Vector2D& o)   const { return x * o.x + y * o.y; }
    float lengthSq()               const { return x * x + y * y; }
    float length()                 const { return sqrtf(lengthSq()); }

    Vector2D normalized() const {
        float len = length();
        if (len < 1e-12f) return {1.0f, 0.0f};
        return {x / len, y / len};
    }
};

// ───────────────────────────────────────────────────────────────────────────
// Ray
// ───────────────────────────────────────────────────────────────────────────
struct Ray {
    Vector2D origin;
    Vector2D direction;  ///< unit vector (mostly +x for left-to-right optics)
    float    wavelength; ///< in mm  (e.g. 5.876e-4 for sodium-D)
    float    n_current;  ///< refractive index of current medium
    bool     tir;        ///< true if TIR occurred at last surface
};

// ───────────────────────────────────────────────────────────────────────────
// ABCD Ray-Transfer Matrix  (y-column convention: [y, u])
//
// Propagation (distance d):   [[1, d], [0, 1]]
// Refraction (n1→n2, R):      [[1, 0], [-(n2-n1)/(n2*R), n1/n2]]
// ───────────────────────────────────────────────────────────────────────────
struct ABCDMatrix {
    float A = 1.0f, B = 0.0f;
    float C = 0.0f, D = 1.0f;

    ABCDMatrix() = default;
    ABCDMatrix(float a, float b, float c, float d)
        : A(a), B(b), C(c), D(d) {}

    /// Left-multiply: result = (*this) * m  (light hits *this first, then m)
    ABCDMatrix operator*(const ABCDMatrix& m) const {
        return {
            A * m.A + B * m.C,  A * m.B + B * m.D,
            C * m.A + D * m.C,  C * m.B + D * m.D
        };
    }
};

// ───────────────────────────────────────────────────────────────────────────
// Traced ray segment (one straight-line piece of a ray's path)
// ───────────────────────────────────────────────────────────────────────────
struct RaySegment {
    Vector2D start, end;
    bool tir;  ///< true if this segment ends in TIR
};

// ───────────────────────────────────────────────────────────────────────────
// Paraxial telemetry
// ───────────────────────────────────────────────────────────────────────────
struct ParaxialData {
    float efl;           ///< Effective Focal Length (mm)
    float bfl;           ///< Back Focal Length from last lens rear surface (mm)
    float ffl;           ///< Front Focal Length from first lens front surface (mm)
    float magnification; ///< Lateral magnification (object→image)
    float H1_abs;        ///< Front principal plane, absolute X position (mm)
    float H2_abs;        ///< Rear principal plane, absolute X position (mm)
    bool  valid;         ///< false if system is singular (C ≈ 0)
};

// ───────────────────────────────────────────────────────────────────────────
// Optical Element
// ───────────────────────────────────────────────────────────────────────────
enum class ElementType : uint8_t {
    LENS,    ///< refractive thick lens (two spherical surfaces)
    SCREEN,  ///< image screen (marker only — no optical effect)
    OBJECT,  ///< object/source marker
};

struct OpticalElement {
    ElementType type       = ElementType::LENS;
    float posX             = 80.0f;  ///< position along optical axis (mm)
    // ── Lens parameters ───────────────────────────────────────────────
    float R1               = 40.0f;  ///< surface-1 radius of curvature (mm)
    float R2               = -40.0f; ///< surface-2 radius of curvature (mm)
    float thickness        = 5.0f;   ///< centre thickness (mm)
    float n                = 1.5f;   ///< refractive index
    float halfHeight       = 14.0f;  ///< aperture half-height (mm)

    // ── Factory helpers ───────────────────────────────────────────────
    static OpticalElement makeLens(float x, float r1, float r2,
                                   float d, float idx, float h = 14.0f) {
        OpticalElement e;
        e.type = ElementType::LENS;
        e.posX = x; e.R1 = r1; e.R2 = r2;
        e.thickness = d; e.n = idx; e.halfHeight = h;
        return e;
    }
    static OpticalElement makeScreen(float x) {
        OpticalElement e;
        e.type = ElementType::SCREEN; e.posX = x;
        e.R1 = e.R2 = OPT_INF; e.thickness = 0; e.n = 1; e.halfHeight = 20.0f;
        return e;
    }
    static OpticalElement makeObject(float x) {
        OpticalElement e;
        e.type = ElementType::OBJECT; e.posX = x;
        e.R1 = e.R2 = OPT_INF; e.thickness = 0; e.n = 1; e.halfHeight = 10.0f;
        return e;
    }

    // ── Paraxial ABCD matrix for a thick lens ─────────────────────────
    // Returns the composite M = M_refr2 × M_prop × M_refr1  (all in air).
    ABCDMatrix getABCDMatrix() const;

    // ── Thin-lens EFL (lensmaker's eq.) — useful for quick reference ──
    float focalLength() const;

    // ── Thick-lens principal-plane offsets ────────────────────────────
    // h1 = offset from front surface (positive = inside lens, rightward)
    // h2 = offset from rear  surface (negative = inside lens, leftward)
    void getPrincipalPlanes(float& h1, float& h2) const;
};

// ───────────────────────────────────────────────────────────────────────────
// Engine constants
// ───────────────────────────────────────────────────────────────────────────
static constexpr int OPT_MAX_ELEMENTS = 8;
static constexpr int OPT_MAX_RAYS     = 7;
static constexpr int OPT_MAX_SEGMENTS = 32;

// ───────────────────────────────────────────────────────────────────────────
// One complete traced ray (sequence of straight segments)
// ───────────────────────────────────────────────────────────────────────────
struct TracedRay {
    RaySegment segments[OPT_MAX_SEGMENTS];
    int   count;
    bool  tir;        ///< true if ray was terminated by TIR
};

// ───────────────────────────────────────────────────────────────────────────
// OpticsEngine
// ───────────────────────────────────────────────────────────────────────────
class OpticsEngine {
public:
    OpticsEngine();

    // ── Scene management ─────────────────────────────────────────────
    void resetScene();
    void addElement(const OpticalElement& e);

    int            elementCount() const { return _elemCount; }
    OpticalElement& element(int i)      { return _elements[i]; }
    const OpticalElement& element(int i) const { return _elements[i]; }

    void  setObjectX(float x)     { _objectX = x; }
    float getObjectX()      const { return _objectX; }
    void  setObjectHeight(float h){ _objectHeight = h; }
    float getObjectHeight() const { return _objectHeight; }

    // ── Engine A: Exact Ray Tracing ───────────────────────────────────
    void traceRays();
    int  tracedRayCount()                 const { return _rayCount; }
    const TracedRay& tracedRay(int i)     const { return _rays[i]; }

    // ── Engine B: Paraxial Analysis ───────────────────────────────────
    void computeParaxial();
    const ParaxialData& paraxial()        const { return _paraxial; }

    // ── Scene bounds ──────────────────────────────────────────────────
    float sceneMinX() const { return _sceneMinX; }
    float sceneMaxX() const { return _sceneMaxX; }

private:
    OpticalElement _elements[OPT_MAX_ELEMENTS];
    int            _elemCount = 0;

    TracedRay      _rays[OPT_MAX_RAYS];
    int            _rayCount  = 0;

    ParaxialData   _paraxial  = {};

    float _objectX      = 0.0f;
    float _objectHeight = 8.0f;
    float _sceneMinX    = -40.0f;
    float _sceneMaxX    = 240.0f;

    // ── Internal helpers ──────────────────────────────────────────────
    void traceOneRay(const Ray& initial, TracedRay& out);

    /// Intersect ray with the vertical plane at x = planeX.
    bool intersectPlane(const Ray& ray, float planeX,
                        Vector2D& hitPt, float& t) const;

    /// Apply vector Snell's law at an interface.
    /// n_hat must point INTO the incident medium (away from surface).
    /// Returns false if Total Internal Reflection occurs.
    bool snellRefract(const Vector2D& vIn, const Vector2D& n_hat,
                      float n1, float n2, Vector2D& vOut) const;

    /// Sort _elements by ascending posX.
    void sortElements();

    /// Compute a safe surface normal pointing into the incident medium.
    /// R = radius of curvature (standard sign convention).
    /// y_hit = intersection height.
    /// fromLeft = true if light is travelling left→right.
    Vector2D surfaceNormal(float R, float y_hit, bool fromLeft) const;
};
