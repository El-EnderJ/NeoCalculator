/**
 * ParticleEngine.h
 * Cellular automata engine for ParticleLabApp (App ID 15).
 * Implements falling-sand physics, thermodynamics, material interactions,
 * electronics (spark cycle), phase transitions, and 30+ materials.
 *
 * "The Alchemy Update" — Powder-Toy-class physics sandbox.
 */

#pragma once

#include <stdint.h>
#include <string.h>

// ── Grid dimensions ──
static constexpr int PG_W     = 160;
static constexpr int PG_H     = 120;
static constexpr int PG_SIZE  = PG_W * PG_H;

// ── Chunk system for skip-logic ──
static constexpr int CHUNK_W  = 10;
static constexpr int CHUNK_H  = 10;
static constexpr int CHUNKS_X = PG_W / CHUNK_W;   // 16
static constexpr int CHUNKS_Y = PG_H / CHUNK_H;   // 12
static constexpr int NUM_CHUNKS = CHUNKS_X * CHUNKS_Y; // 192

// ── Particle flags ──
static constexpr uint8_t PF_UPDATED = 0x01;  // bit 0: updated this frame
static constexpr uint8_t PF_SPARKED = 0x02;  // bit 1: carrying a spark

// ── Particle struct: exactly 4 bytes ──
#pragma pack(push, 1)
struct Particle {
    uint8_t type;      // Material ID (MatType)
    int16_t temp;      // Temperature in Celsius (e.g., 22 = 22C)
    uint8_t flags;     // Bits: 0=updated_this_frame, 1=sparked, 2-7=life/extra
};
#pragma pack(pop)

static_assert(sizeof(Particle) == 4, "Particle struct must be exactly 4 bytes");

// ── Material IDs ──
enum class MatType : uint8_t {
    EMPTY          = 0,
    WALL           = 1,
    SAND           = 2,
    WATER          = 3,
    WOOD           = 4,
    FIRE           = 5,
    OIL            = 6,
    STEAM          = 7,
    ICE            = 8,
    SALT           = 9,
    GUNPOWDER      = 10,
    ACID           = 11,
    // ── New materials (Alchemy Update) ──
    MOLTEN_GLASS   = 12,
    GLASS          = 13,
    STONE          = 14,
    COAL           = 15,
    PLANT          = 16,
    LAVA           = 17,
    LN2            = 18,
    GAS_MAT        = 19,
    WIRE           = 20,
    HEATER         = 21,
    COOLER         = 22,
    C4             = 23,
    HEAC           = 24,
    INSL           = 25,
    TITAN          = 26,
    MOLTEN_TITAN   = 27,
    SMOKE          = 28,
    CLONE          = 29,
    IRON           = 30,
    MAT_COUNT      = 31
};

// ── Material states ──
enum class MatState : uint8_t {
    SOLID  = 0,
    POWDER = 1,
    LIQUID = 2,
    GAS    = 3
};

// ── Material properties LUT entry ──
struct MaterialProps {
    uint16_t color;         // RGB565
    uint8_t  density;       // Higher = heavier (0=gas, 255=heaviest solid)
    uint8_t  flammability;  // 0 = non-flammable, 255 = extremely flammable
    MatState state;
    uint8_t  thermalCond;   // 0-255 thermal conductivity
    int16_t  meltPoint;     // Temperature at which solid->liquid (INT16_MAX if never)
    int16_t  boilPoint;     // Temperature at which liquid->gas (INT16_MAX if never)
    uint8_t  electricCond;  // 0 = insulator, 1 = conducts sparks
};

// ── Helper: RGB888 to RGB565 ──
static constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ── Grid index macro ──
inline int PG_IX(int x, int y) { return y * PG_W + x; }

// ── Brush shape enum ──
enum class BrushShape : uint8_t {
    CIRCLE = 0,
    SQUARE = 1,
    SPRAY  = 2,
    SHAPE_COUNT = 3
};

// ── The particle engine ──
class ParticleEngine {
public:
    ParticleEngine();
    ~ParticleEngine();

    /// Allocate grid in PSRAM. Returns false on allocation failure.
    bool init();

    /// Free grid memory.
    void destroy();

    /// Clear entire grid to EMPTY at ambient temperature.
    void clear();

    /// Run one full physics tick.
    void tick();

    /// Place a particle at (x,y) with given material.
    void placeParticle(int x, int y, MatType mat);

    /// Place particles using brush shape.
    void placeBrush(int cx, int cy, int radius, BrushShape shape, MatType mat);

    /// Draw a Bresenham line of particles between two points.
    void placeLine(int x0, int y0, int x1, int y1, int radius, BrushShape shape, MatType mat);

    /// Get particle at (x,y).
    const Particle& getParticle(int x, int y) const;

    /// Get material properties for a type.
    static const MaterialProps& getMatProps(uint8_t type);

    /// Get material name string.
    static const char* getMatName(uint8_t type);

    /// Check if a material is electrically conductive.
    static bool isConductive(uint8_t type);

    /// Count active (non-empty) particles.
    int countParticles() const;

    /// Direct grid access for rendering.
    const Particle* grid() const { return _grid; }
    Particle* grid() { return _grid; }

    /// Gravity vector (default: gx=0, gy=1 = down).
    int gx = 0;
    int gy = 1;

    /// Frame counter (used for alternating scan direction).
    uint32_t frameCount() const { return _frameCount; }

    /// Serialize grid to a byte buffer (returns grid size in bytes).
    int serialize(uint8_t* buf, int bufSize) const;

    /// Deserialize grid from a byte buffer.
    bool deserialize(const uint8_t* buf, int bufSize);

private:
    Particle* _grid;
    uint8_t   _chunkActive[NUM_CHUNKS]; // 1 if chunk had activity last frame
    uint8_t   _chunkDirty[NUM_CHUNKS];  // 1 if chunk has activity this frame
    uint32_t  _frameCount;

    // ── Physics sub-steps ──
    void updateParticle(int x, int y);
    void updatePowder(int x, int y, Particle& p);
    void updateLiquid(int x, int y, Particle& p);
    void updateGas(int x, int y, Particle& p);
    void updateFire(int x, int y, Particle& p);
    void updateAcid(int x, int y, Particle& p);
    void updatePlant(int x, int y, Particle& p);
    void updateClone(int x, int y, Particle& p);
    void updateSpark(int x, int y, Particle& p);

    // ── Thermodynamics ──
    void conductHeat();

    // ── Reactions ──
    void checkReactions(int x, int y);

    // ── Helpers ──
    bool inBounds(int x, int y) const { return x >= 0 && x < PG_W && y >= 0 && y < PG_H; }
    bool isEmpty(int x, int y) const;
    bool canDisplace(int x, int y, uint8_t myDensity) const;
    void swapParticles(int x1, int y1, int x2, int y2);
    void markChunkDirty(int x, int y);
    void igniteNeighbors(int x, int y, int16_t heatOut);
    void explode(int cx, int cy, int radius);

    // ── Material LUT (static, in Flash) ──
    static const MaterialProps MAT_LUT[(int)MatType::MAT_COUNT];
    static const char* const   MAT_NAMES[(int)MatType::MAT_COUNT];
};
