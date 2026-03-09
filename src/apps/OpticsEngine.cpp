/**
 * OpticsEngine.cpp
 * OpticsLab Physics Core — Dual-engine implementation.
 *
 * Engine A: Exact 2D ray-tracing with vector Snell's Law.
 *   – Uses the approximate planar-intersection / spherical-normal method:
 *     the ray is intersected with the vertical plane at the lens position,
 *     and the surface normal is computed from the spherical geometry at that
 *     height.  This is exact for paraxial rays and gives good qualitative
 *     results for moderate apertures (f/3 and slower).
 *
 * Engine B: Paraxial ABCD matrix chain.
 *   – Produces EFL, BFL, magnification, and principal-plane positions.
 */

#include "OpticsEngine.h"
#include <algorithm>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════════════
// OpticalElement helpers
// ═══════════════════════════════════════════════════════════════════════════

ABCDMatrix OpticalElement::getABCDMatrix() const {
    if (type != ElementType::LENS) return ABCDMatrix();  // identity

    // ── Refraction matrix at surface 1 (air → glass) ─────────────────
    // M_r1 = [[1, 0], [-(n2-n1)/(n2*R1), n1/n2]]
    // with n1=1, n2=n, radius=R1
    ABCDMatrix M_r1;
    if (fabsf(R1) < 1.0f) {
        // Essentially flat surface — identity refraction
        M_r1 = ABCDMatrix(1, 0, 0, 1.0f / n);
    } else {
        float C1 = -(n - 1.0f) / (n * R1);
        M_r1 = ABCDMatrix(1.0f, 0.0f, C1, 1.0f / n);
    }

    // ── Propagation through glass (thickness d) ───────────────────────
    // M_d = [[1, d], [0, 1]]
    ABCDMatrix M_d(1.0f, thickness, 0.0f, 1.0f);

    // ── Refraction matrix at surface 2 (glass → air) ─────────────────
    // M_r2 = [[1, 0], [-(n2-n1)/(n2*R2), n1/n2]]
    // with n1=n, n2=1, radius=R2
    ABCDMatrix M_r2;
    if (fabsf(R2) < 1.0f) {
        M_r2 = ABCDMatrix(1, 0, 0, n);
    } else {
        float C2 = -(1.0f - n) / (1.0f * R2);   //  = (n-1)/R2
        M_r2 = ABCDMatrix(1.0f, 0.0f, C2, n);
    }

    // ── Composite thick-lens matrix M = M_r2 * M_d * M_r1 ─────────────
    // (light first hits M_r1, then propagates M_d, then exits M_r2)
    return M_r2 * (M_d * M_r1);
}

float OpticalElement::focalLength() const {
    if (type != ElementType::LENS) return OPT_INF;
    float inv_f = 0.0f;
    if (fabsf(R1) > 0.01f) inv_f += 1.0f / R1;
    if (fabsf(R2) > 0.01f) inv_f -= 1.0f / R2;
    inv_f *= (n - 1.0f);
    // Thick-lens correction
    if (fabsf(R1) > 0.01f && fabsf(R2) > 0.01f) {
        inv_f += (n - 1.0f) * (n - 1.0f) * thickness / (n * R1 * R2);
    }
    if (fabsf(inv_f) < 1e-9f) return OPT_INF;
    return 1.0f / inv_f;
}

void OpticalElement::getPrincipalPlanes(float& h1, float& h2) const {
    ABCDMatrix M = getABCDMatrix();
    if (fabsf(M.C) < 1e-12f) { h1 = h2 = 0.0f; return; }
    // h1 from front surface (positive = into lens)
    // h2 from rear  surface (negative = into lens)
    // EFL = -1/C
    // H1 offset from front surface = (D-1)/C   (positive = rightward)
    // H2 offset from rear  surface = (1-A)/C   (negative = leftward)
    h1 = (M.D - 1.0f) / M.C;
    h2 = (1.0f - M.A) / M.C;
}

// ═══════════════════════════════════════════════════════════════════════════
// OpticsEngine
// ═══════════════════════════════════════════════════════════════════════════

OpticsEngine::OpticsEngine() {
    resetScene();
}

void OpticsEngine::resetScene() {
    _elemCount   = 0;
    _rayCount    = 0;
    _objectX     = 0.0f;
    _objectHeight = 8.0f;
    _sceneMinX   = -40.0f;
    _sceneMaxX   = 240.0f;
    memset(&_paraxial, 0, sizeof(_paraxial));
}

void OpticsEngine::addElement(const OpticalElement& e) {
    if (_elemCount >= OPT_MAX_ELEMENTS) return;
    _elements[_elemCount++] = e;
    sortElements();
}

void OpticsEngine::sortElements() {
    // Simple insertion sort (small array)
    for (int i = 1; i < _elemCount; ++i) {
        OpticalElement key = _elements[i];
        int j = i - 1;
        while (j >= 0 && _elements[j].posX > key.posX) {
            _elements[j + 1] = _elements[j];
            j--;
        }
        _elements[j + 1] = key;
    }
}

// ───────────────────────────────────────────────────────────────────────────
// Surface normal helper
//
// Returns the surface normal at height y_hit, pointing INTO the incident
// medium (i.e., pointing away from the refracting surface toward the
// incoming ray).
//
//   fromLeft = true  : incident medium is on the LEFT  (n_hat → points left)
//   fromLeft = false : incident medium is on the RIGHT (n_hat → points right)
// ───────────────────────────────────────────────────────────────────────────
Vector2D OpticsEngine::surfaceNormal(float R, float y_hit, bool fromLeft) const {
    if (fabsf(R) > 1e6f) {
        // Flat surface
        return fromLeft ? Vector2D(-1.0f, 0.0f) : Vector2D(1.0f, 0.0f);
    }
    // Approximate outward sphere normal at surface point (0, y_hit) with
    // centre of curvature at (R, 0) → direction = (0 - R, y_hit - 0)
    //                                             = (-R, y_hit)
    Vector2D n(-R, y_hit);
    n = n.normalized();
    // Ensure it points into the incident medium
    if (fromLeft  && n.x > 0.0f) { n.x = -n.x; n.y = -n.y; }
    if (!fromLeft && n.x < 0.0f) { n.x = -n.x; n.y = -n.y; }
    return n;
}

// ───────────────────────────────────────────────────────────────────────────
// Vector form of Snell's law:
//   v_out = η · v_in + (η · cosθ_i − cosθ_t) · n_hat
// where:
//   n_hat  = surface normal pointing INTO the incident medium
//   cosθ_i = −(v_in · n_hat)  (positive: ray heads into the surface)
//   η      = n1 / n2
//
// Returns false on Total Internal Reflection.
// ───────────────────────────────────────────────────────────────────────────
bool OpticsEngine::snellRefract(const Vector2D& vIn,
                                 const Vector2D& n_hat,
                                 float n1, float n2,
                                 Vector2D& vOut) const {
    float cos_i = -(vIn.dot(n_hat));   // should be > 0
    if (cos_i < 0.0f) cos_i = -cos_i; // safety clamp

    float eta     = n1 / n2;
    float sin2_t  = eta * eta * (1.0f - cos_i * cos_i);
    if (sin2_t >= 1.0f) return false;  // TIR

    float cos_t = sqrtf(1.0f - sin2_t);
    // v_out = η·v_in + (η·cosθ_i − cosθ_t)·n_hat
    vOut.x = eta * vIn.x + (eta * cos_i - cos_t) * n_hat.x;
    vOut.y = eta * vIn.y + (eta * cos_i - cos_t) * n_hat.y;
    // Renormalize to guard against fp drift
    vOut = vOut.normalized();
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Intersect ray with vertical plane at x = planeX.
// Returns false if the ray is nearly parallel to the plane (dx ≈ 0).
// ───────────────────────────────────────────────────────────────────────────
bool OpticsEngine::intersectPlane(const Ray& ray, float planeX,
                                   Vector2D& hitPt, float& t) const {
    float dx = ray.direction.x;
    if (fabsf(dx) < 1e-9f) return false;
    t = (planeX - ray.origin.x) / dx;
    if (t < 0.0f) return false;   // behind ray
    hitPt.x = ray.origin.x + ray.direction.x * t;
    hitPt.y = ray.origin.y + ray.direction.y * t;
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Trace a single ray through the entire element sequence.
// ───────────────────────────────────────────────────────────────────────────
void OpticsEngine::traceOneRay(const Ray& initial, TracedRay& out) {
    out.count = 0;
    out.tir   = false;

    Ray cur = initial;

    // Scene end boundary
    float xEnd = _sceneMaxX + 10.0f;

    for (int ei = 0; ei < _elemCount && out.count < OPT_MAX_SEGMENTS - 1; ++ei) {
        const OpticalElement& el = _elements[ei];

        if (el.type == ElementType::OBJECT) continue;  // skip object markers
        if (el.posX <= cur.origin.x + 1e-3f) continue; // element behind ray

        if (el.type == ElementType::SCREEN) {
            // Screen: just terminate segment at the screen plane
            Vector2D hitPt; float t;
            if (intersectPlane(cur, el.posX, hitPt, t)) {
                RaySegment& seg = out.segments[out.count++];
                seg.start = cur.origin;
                seg.end   = hitPt;
                seg.tir   = false;
                cur.origin = hitPt;
            }
            break; // stop at screen
        }

        // ── Lens element: two refractive surfaces ─────────────────────
        if (el.type != ElementType::LENS) continue;

        float xS1 = el.posX;                 // front surface
        float xS2 = el.posX + el.thickness;  // rear surface

        // ── Intersect front surface ───────────────────────────────────
        Vector2D hit1; float t1;
        if (!intersectPlane(cur, xS1, hit1, t1)) continue;
        if (fabsf(hit1.y) > el.halfHeight) {
            // Ray misses aperture — continue past the lens
            continue;
        }

        // Segment from current position to front surface
        {
            RaySegment& seg = out.segments[out.count++];
            seg.start = cur.origin;
            seg.end   = hit1;
            seg.tir   = false;
        }
        if (out.count >= OPT_MAX_SEGMENTS - 1) break;

        // Apply refraction at front surface (air → glass)
        Vector2D n1_hat = surfaceNormal(el.R1, hit1.y, /*fromLeft=*/true);
        Vector2D dir_refr;
        if (!snellRefract(cur.direction, n1_hat, cur.n_current, el.n, dir_refr)) {
            // TIR at front surface (unusual but handle it)
            RaySegment& seg = out.segments[out.count++];
            seg.start = hit1;
            seg.end   = hit1 + cur.direction * 0.1f;
            seg.tir   = true;
            out.tir   = true;
            return;
        }

        // Inside glass: propagate to rear surface
        Ray glassRay;
        glassRay.origin    = hit1;
        glassRay.direction = dir_refr;
        glassRay.n_current = el.n;
        glassRay.tir       = false;

        Vector2D hit2; float t2;
        if (!intersectPlane(glassRay, xS2, hit2, t2)) {
            // Shouldn't happen for reasonable ray angles
            cur.origin = hit1;
            continue;
        }

        // Check aperture at rear surface too
        if (fabsf(hit2.y) > el.halfHeight * 1.1f) {
            // Ray lost inside glass
            cur.origin = hit1;
            continue;
        }

        // Segment inside glass
        {
            RaySegment& seg = out.segments[out.count++];
            seg.start = hit1;
            seg.end   = hit2;
            seg.tir   = false;
        }
        if (out.count >= OPT_MAX_SEGMENTS - 1) break;

        // Apply refraction at rear surface (glass → air)
        // Normal points INTO glass (incident medium), i.e., points LEFT
        Vector2D n2_hat = surfaceNormal(el.R2, hit2.y, /*fromLeft=*/true);
        // For rear surface the light comes from the LEFT (from inside glass)
        // n2_hat should point leftward (into glass)
        // surfaceNormal(R2, y, fromLeft=true) gives a normal pointing left ✓

        Vector2D dir_exit;
        if (!snellRefract(glassRay.direction, n2_hat, el.n, 1.0f, dir_exit)) {
            // TIR at rear surface
            {
                RaySegment& seg = out.segments[out.count++];
                seg.start = hit2;
                // Reflect inside (simple: reverse x)
                Vector2D reflDir(-glassRay.direction.x, glassRay.direction.y);
                seg.end = hit2 + reflDir * 30.0f;
                seg.tir = true;
            }
            out.tir = true;
            return;
        }

        cur.origin    = hit2;
        cur.direction = dir_exit;
        cur.n_current = 1.0f;
    }

    // Final segment: ray continues to scene boundary
    if (out.count < OPT_MAX_SEGMENTS) {
        Vector2D hitEnd; float te;
        if (intersectPlane(cur, xEnd, hitEnd, te)) {
            RaySegment& seg = out.segments[out.count++];
            seg.start = cur.origin;
            seg.end   = hitEnd;
            seg.tir   = false;
        }
    }
}

// ───────────────────────────────────────────────────────────────────────────
// Trace all rays in the fan (parallel rays from -halfH to +halfH plus
// marginal and chief rays).  Called every time parameters change.
// ───────────────────────────────────────────────────────────────────────────
void OpticsEngine::traceRays() {
    _rayCount = 0;

    // Determine aperture heights to use for the fan.
    // Use the first lens element's halfHeight as reference.
    float aperH = 12.0f;
    for (int i = 0; i < _elemCount; ++i) {
        if (_elements[i].type == ElementType::LENS) {
            aperH = _elements[i].halfHeight;
            break;
        }
    }

    static constexpr int NUM_RAYS = OPT_MAX_RAYS;  // 7
    for (int k = 0; k < NUM_RAYS && _rayCount < OPT_MAX_RAYS; ++k) {
        float h;
        if (NUM_RAYS == 1) {
            h = 0.0f;
        } else {
            h = aperH * (2.0f * k / (NUM_RAYS - 1) - 1.0f);
        }

        Ray r;
        r.origin    = { _objectX - 5.0f, h };  // slightly left of object
        r.direction = { 1.0f, 0.0f };           // parallel to optical axis
        r.wavelength = 5.876e-4f;               // sodium-D (mm)
        r.n_current  = 1.0f;
        r.tir        = false;

        TracedRay& tr = _rays[_rayCount++];
        tr.count = 0;
        tr.tir   = false;
        traceOneRay(r, tr);
    }
}

// ───────────────────────────────────────────────────────────────────────────
// Compute composite ABCD matrix and derive paraxial telemetry.
// ───────────────────────────────────────────────────────────────────────────
void OpticsEngine::computeParaxial() {
    _paraxial = {};
    _paraxial.valid = false;

    // Collect lens elements sorted by position
    ABCDMatrix composite;    // starts as identity
    float firstLensX = 0.0f, lastLensRearX = 0.0f;
    bool  hasLens = false;
    float prevX   = _objectX; // propagate from object position

    for (int i = 0; i < _elemCount; ++i) {
        const OpticalElement& el = _elements[i];
        if (el.type != ElementType::LENS) continue;

        // Free-space propagation from prevX to el.posX (front surface)
        float gap = el.posX - prevX;
        if (gap > 0.0f) {
            ABCDMatrix Mprop(1.0f, gap, 0.0f, 1.0f);
            composite = Mprop * composite;
        }

        // Apply lens matrix
        composite = el.getABCDMatrix() * composite;

        if (!hasLens) { firstLensX = el.posX; hasLens = true; }
        lastLensRearX = el.posX + el.thickness;
        prevX = lastLensRearX;
    }

    if (!hasLens) return;

    float C = composite.C;
    if (fabsf(C) < 1e-12f) return; // singular (afocal system)

    float A = composite.A;
    float D = composite.D;

    float efl = -1.0f / C;
    float bfl = -A / C;        // from last lens rear surface to back focal point
    float ffl =  D / C;        // from first lens front surface to front focal point

    // Principal plane absolute positions
    // H2 offset from last lens rear surface = (1-A)/C
    float h2_offset = (1.0f - A) / C;
    // H1 offset from first lens front surface = (D-1)/C
    float h1_offset = (D - 1.0f) / C;

    float H2_abs = lastLensRearX + h2_offset;
    float H1_abs = firstLensX    + h1_offset;

    // Magnification: object at _objectX, image via 1/v - 1/u = 1/EFL
    float u = _objectX - H1_abs;  // signed distance from H1 to object
    float mag = 0.0f;
    if (fabsf(u - efl) > 0.01f) {
        float v = efl * u / (u - efl);
        mag = (fabsf(u) > 0.01f) ? (v / u) : 0.0f;
    }

    _paraxial.efl          = efl;
    _paraxial.bfl          = bfl;
    _paraxial.ffl          = ffl;
    _paraxial.magnification = mag;
    _paraxial.H1_abs       = H1_abs;
    _paraxial.H2_abs       = H2_abs;
    _paraxial.valid        = true;
}
