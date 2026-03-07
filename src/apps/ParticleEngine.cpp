/**
 * ParticleEngine.cpp
 * Cellular automata engine — physics, thermodynamics, material interactions.
 */

#include "ParticleEngine.h"

#ifdef ARDUINO
#include <esp_heap_caps.h>
#include <stdlib.h>
#else
#include <cstdlib>
#endif

// ═══════════════════════════════════════════════════════════════════
// Material LUT (static, effectively in Flash on ESP32)
// ═══════════════════════════════════════════════════════════════════
//                                     color               dens  flam  state           therm  melt   boil
const MaterialProps ParticleEngine::MAT_LUT[(int)MatType::MAT_COUNT] = {
    /* EMPTY     */ { RGB565(  8,  8, 12),   0,   0, MatState::GAS,     0,   INT16_MAX, INT16_MAX },
    /* WALL      */ { RGB565(128,128,128), 255,   0, MatState::SOLID,  80,   INT16_MAX, INT16_MAX },
    /* SAND      */ { RGB565(210,180, 80), 200,   0, MatState::POWDER, 40,   1700,      INT16_MAX },
    /* WATER     */ { RGB565( 30, 90,220), 100,   0, MatState::LIQUID, 60,   INT16_MIN, 100       },
    /* WOOD      */ { RGB565(120, 80, 40), 220, 180, MatState::SOLID,  20,   INT16_MAX, INT16_MAX },
    /* FIRE      */ { RGB565(255,100, 10),   5, 255, MatState::GAS,   200,   INT16_MAX, INT16_MAX },
    /* OIL       */ { RGB565( 60, 40, 20),  80, 230, MatState::LIQUID, 30,   INT16_MIN, 250       },
    /* STEAM     */ { RGB565(200,210,230),  10,   0, MatState::GAS,    50,   INT16_MIN, INT16_MIN },
    /* ICE       */ { RGB565(180,220,255), 180,   0, MatState::SOLID,  90,   0,         INT16_MAX },
    /* SALT      */ { RGB565(240,240,240), 190,   0, MatState::POWDER, 30,   801,       INT16_MAX },
    /* GUNPOWDER */ { RGB565( 60, 60, 60), 195, 255, MatState::POWDER, 10,   INT16_MAX, INT16_MAX },
    /* ACID      */ { RGB565( 80,255, 40),  90,   0, MatState::LIQUID, 40,   INT16_MIN, 300       },
};

const char* const ParticleEngine::MAT_NAMES[(int)MatType::MAT_COUNT] = {
    "Empty", "Wall", "Sand", "Water", "Wood", "Fire",
    "Oil", "Steam", "Ice", "Salt", "Gunpowder", "Acid"
};

// ═══════════════════════════════════════════════════════════════════
// Simple pseudo-random (fast, no library dependency)
// ═══════════════════════════════════════════════════════════════════
static uint32_t s_rng = 12345;
static inline uint32_t fastRand() {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}
static inline int randInt(int maxExcl) { return (int)(fastRand() % (uint32_t)maxExcl); }
static inline bool randBool() { return (fastRand() & 1) != 0; }

// ═══════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════
ParticleEngine::ParticleEngine()
    : _grid(nullptr), _frameCount(0)
{
    memset(_chunkActive, 1, sizeof(_chunkActive));
    memset(_chunkDirty, 0, sizeof(_chunkDirty));
}

ParticleEngine::~ParticleEngine() {
    destroy();
}

// ═══════════════════════════════════════════════════════════════════
// init / destroy / clear
// ═══════════════════════════════════════════════════════════════════
bool ParticleEngine::init() {
    if (_grid) return true;

#ifdef ARDUINO
    _grid = (Particle*)heap_caps_malloc(PG_SIZE * sizeof(Particle), MALLOC_CAP_SPIRAM);
#else
    _grid = new (std::nothrow) Particle[PG_SIZE];
#endif

    if (!_grid) return false;
    clear();
    return true;
}

void ParticleEngine::destroy() {
    if (!_grid) return;
#ifdef ARDUINO
    heap_caps_free(_grid);
#else
    delete[] _grid;
#endif
    _grid = nullptr;
}

void ParticleEngine::clear() {
    if (!_grid) return;
    for (int i = 0; i < PG_SIZE; ++i) {
        _grid[i].type  = (uint8_t)MatType::EMPTY;
        _grid[i].temp  = 22;   // ambient 22°C
        _grid[i].flags = 0;
    }
    memset(_chunkActive, 1, sizeof(_chunkActive));
    memset(_chunkDirty, 0, sizeof(_chunkDirty));
    _frameCount = 0;
}

// ═══════════════════════════════════════════════════════════════════
// Particle placement
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::placeParticle(int x, int y, MatType mat) {
    if (!_grid || !inBounds(x, y)) return;
    int idx = PG_IX(x, y);
    // Wall is indestructible — cannot overwrite
    if (_grid[idx].type == (uint8_t)MatType::WALL && mat != MatType::EMPTY) return;
    _grid[idx].type  = (uint8_t)mat;
    _grid[idx].temp  = (mat == MatType::FIRE) ? 600 :
                       (mat == MatType::ICE)  ? -10 :
                       (mat == MatType::STEAM) ? 110 : 22;
    _grid[idx].flags = 0;
    if (mat == MatType::FIRE) _grid[idx].flags = 60 + randInt(40); // life=60-99
    markChunkDirty(x, y);
}

void ParticleEngine::placeBrush(int cx, int cy, int radius, bool circle, MatType mat) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (circle && dx * dx + dy * dy > radius * radius) continue;
            placeParticle(cx + dx, cy + dy, mat);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Accessors
// ═══════════════════════════════════════════════════════════════════
const Particle& ParticleEngine::getParticle(int x, int y) const {
    static const Particle empty = { (uint8_t)MatType::WALL, 22, 0 };
    if (!_grid || !inBounds(x, y)) return empty;
    return _grid[PG_IX(x, y)];
}

const MaterialProps& ParticleEngine::getMatProps(uint8_t type) {
    if (type >= (uint8_t)MatType::MAT_COUNT) type = 0;
    return MAT_LUT[type];
}

const char* ParticleEngine::getMatName(uint8_t type) {
    if (type >= (uint8_t)MatType::MAT_COUNT) return "?";
    return MAT_NAMES[type];
}

// ═══════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════
bool ParticleEngine::isEmpty(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return _grid[PG_IX(x, y)].type == (uint8_t)MatType::EMPTY;
}

bool ParticleEngine::canDisplace(int x, int y, uint8_t myDensity) const {
    if (!inBounds(x, y)) return false;
    uint8_t otherType = _grid[PG_IX(x, y)].type;
    if (otherType == (uint8_t)MatType::EMPTY) return true;
    if (otherType == (uint8_t)MatType::WALL) return false;
    return MAT_LUT[otherType].density < myDensity;
}

void ParticleEngine::swapParticles(int x1, int y1, int x2, int y2) {
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;
    int i1 = PG_IX(x1, y1);
    int i2 = PG_IX(x2, y2);
    Particle tmp = _grid[i1];
    _grid[i1] = _grid[i2];
    _grid[i2] = tmp;
    markChunkDirty(x1, y1);
    markChunkDirty(x2, y2);
}

void ParticleEngine::markChunkDirty(int x, int y) {
    int cx = x / CHUNK_W;
    int cy = y / CHUNK_H;
    if (cx >= 0 && cx < CHUNKS_X && cy >= 0 && cy < CHUNKS_Y) {
        _chunkDirty[cy * CHUNKS_X + cx] = 1;
    }
}

void ParticleEngine::igniteNeighbors(int x, int y, int16_t heatOut) {
    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int nx = x + dx[d], ny = y + dy[d];
        if (!inBounds(nx, ny)) continue;
        Particle& nb = _grid[PG_IX(nx, ny)];
        // Transfer heat
        nb.temp = (int16_t)(nb.temp + (heatOut >> 2));
        // Ignite flammable materials
        uint8_t flam = MAT_LUT[nb.type].flammability;
        if (flam > 0 && nb.temp > 200 && randInt(256) < flam) {
            nb.type  = (uint8_t)MatType::FIRE;
            nb.flags = 40 + randInt(60);
            markChunkDirty(nx, ny);
        }
    }
}

void ParticleEngine::explode(int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            int nx = cx + dx, ny = cy + dy;
            if (!inBounds(nx, ny)) continue;
            Particle& p = _grid[PG_IX(nx, ny)];
            if (p.type == (uint8_t)MatType::WALL) continue;
            if (randInt(3) == 0) {
                p.type  = (uint8_t)MatType::FIRE;
                p.temp  = 800;
                p.flags = 20 + randInt(30);
            } else {
                p.type  = (uint8_t)MatType::EMPTY;
                p.temp  = 400;
                p.flags = 0;
            }
            markChunkDirty(nx, ny);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Main tick
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::tick() {
    if (!_grid) return;
    _frameCount++;

    // Clear updated flags
    for (int i = 0; i < PG_SIZE; ++i) {
        _grid[i].flags &= 0xFE;  // clear bit 0 (updated flag)
    }

    // Copy chunk activity from last frame's dirty map
    memcpy(_chunkActive, _chunkDirty, sizeof(_chunkDirty));
    memset(_chunkDirty, 0, sizeof(_chunkDirty));

    // Heat conduction every other frame for performance
    if ((_frameCount & 1) == 0) {
        conductHeat();
    }

    // Determine scan direction: alternate L-R / R-L each frame
    bool scanLR = (_frameCount & 1) == 0;

    // Iterate bottom-to-top for falling particles
    for (int y = PG_H - 1; y >= 0; --y) {
        // Check chunk activity for this row
        int chunkY = y / CHUNK_H;

        if (scanLR) {
            for (int x = 0; x < PG_W; ++x) {
                int chunkX = x / CHUNK_W;
                if (!_chunkActive[chunkY * CHUNKS_X + chunkX]) continue;
                updateParticle(x, y);
            }
        } else {
            for (int x = PG_W - 1; x >= 0; --x) {
                int chunkX = x / CHUNK_W;
                if (!_chunkActive[chunkY * CHUNKS_X + chunkX]) continue;
                updateParticle(x, y);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Per-particle update dispatcher
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateParticle(int x, int y) {
    int idx = PG_IX(x, y);
    Particle& p = _grid[idx];

    if (p.type == (uint8_t)MatType::EMPTY || p.type == (uint8_t)MatType::WALL) return;
    if (p.flags & 1) return; // already updated this frame

    p.flags |= 1; // mark updated

    // ── State transitions based on temperature ──
    const MaterialProps& props = MAT_LUT[p.type];

    // Melting: solid/powder → liquid
    if ((props.state == MatState::SOLID || props.state == MatState::POWDER) &&
        props.meltPoint != INT16_MAX && p.temp > props.meltPoint) {
        // Ice → Water, Salt → liquid (just disappear for simplicity)
        if (p.type == (uint8_t)MatType::ICE) {
            p.type = (uint8_t)MatType::WATER;
            p.temp = 1;
        } else if (p.type == (uint8_t)MatType::SAND && p.temp > 1700) {
            // Sand melts to glass (becomes wall-like)
            p.type = (uint8_t)MatType::WALL;
            p.temp = 1700;
        }
        markChunkDirty(x, y);
        return;
    }

    // Boiling: liquid → gas
    if (props.state == MatState::LIQUID && props.boilPoint != INT16_MAX &&
        props.boilPoint != INT16_MIN && p.temp > props.boilPoint) {
        if (p.type == (uint8_t)MatType::WATER) {
            p.type = (uint8_t)MatType::STEAM;
            p.temp = 101;
        } else if (p.type == (uint8_t)MatType::OIL) {
            p.type = (uint8_t)MatType::FIRE;
            p.flags = 40 + randInt(40);
            p.temp = 300;
        } else if (p.type == (uint8_t)MatType::ACID) {
            p.type = (uint8_t)MatType::STEAM;
            p.temp = 120;
        }
        markChunkDirty(x, y);
        return;
    }

    // Steam condensation: gas → liquid when < 90C
    if (p.type == (uint8_t)MatType::STEAM && p.temp < 90) {
        p.type = (uint8_t)MatType::WATER;
        p.temp = 89;
        markChunkDirty(x, y);
        return;
    }

    // ── Material-specific behavior ──
    switch ((MatType)p.type) {
        case MatType::SAND:
        case MatType::SALT:
        case MatType::GUNPOWDER:
            updatePowder(x, y, p);
            break;
        case MatType::WATER:
        case MatType::OIL:
            updateLiquid(x, y, p);
            break;
        case MatType::FIRE:
            updateFire(x, y, p);
            break;
        case MatType::STEAM:
            updateGas(x, y, p);
            break;
        case MatType::ACID:
            updateAcid(x, y, p);
            break;
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Powder physics (sand, salt, gunpowder)
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updatePowder(int x, int y, Particle& p) {
    uint8_t dens = MAT_LUT[p.type].density;
    int dgy = (gy >= 0) ? 1 : -1;  // gravity direction

    // Gunpowder: check for ignition
    if (p.type == (uint8_t)MatType::GUNPOWDER) {
        if (p.temp > 100) {
            explode(x, y, 3 + randInt(3));
            return;
        }
    }

    // Salt: dissolve in water
    if (p.type == (uint8_t)MatType::SALT) {
        static const int dx4[] = {-1, 1, 0, 0};
        static const int dy4[] = {0, 0, -1, 1};
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (inBounds(nx, ny) && _grid[PG_IX(nx, ny)].type == (uint8_t)MatType::WATER) {
                if (randInt(20) == 0) {
                    p.type = (uint8_t)MatType::EMPTY;
                    p.flags = 0;
                    markChunkDirty(x, y);
                    return;
                }
            }
        }
    }

    // Try to move down
    int ny = y + dgy;
    if (canDisplace(x, ny, dens)) {
        swapParticles(x, y, x, ny);
        return;
    }

    // Try diagonal (random order to prevent bias)
    int dir = randBool() ? 1 : -1;
    if (canDisplace(x + dir, ny, dens)) {
        swapParticles(x, y, x + dir, ny);
        return;
    }
    if (canDisplace(x - dir, ny, dens)) {
        swapParticles(x, y, x - dir, ny);
        return;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Liquid physics (water, oil)
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateLiquid(int x, int y, Particle& p) {
    uint8_t dens = MAT_LUT[p.type].density;
    int dgy = (gy >= 0) ? 1 : -1;

    // Try down
    int ny = y + dgy;
    if (canDisplace(x, ny, dens)) {
        swapParticles(x, y, x, ny);
        return;
    }

    // Try diagonal down
    int dir = randBool() ? 1 : -1;
    if (canDisplace(x + dir, ny, dens)) {
        swapParticles(x, y, x + dir, ny);
        return;
    }
    if (canDisplace(x - dir, ny, dens)) {
        swapParticles(x, y, x - dir, ny);
        return;
    }

    // Flow horizontally
    int spread = 2 + randInt(3);
    for (int s = 1; s <= spread; ++s) {
        if (canDisplace(x + dir * s, y, dens)) {
            swapParticles(x, y, x + dir * s, y);
            return;
        }
        if (canDisplace(x - dir * s, y, dens)) {
            swapParticles(x, y, x - dir * s, y);
            return;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Gas physics (steam)
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateGas(int x, int y, Particle& p) {
    int dgy = (gy >= 0) ? -1 : 1;  // rises opposite to gravity

    // Try up
    int ny = y + dgy;
    if (isEmpty(x, ny)) {
        swapParticles(x, y, x, ny);
        return;
    }

    // Try diagonal up
    int dir = randBool() ? 1 : -1;
    if (isEmpty(x + dir, ny)) {
        swapParticles(x, y, x + dir, ny);
        return;
    }
    if (isEmpty(x - dir, ny)) {
        swapParticles(x, y, x - dir, ny);
        return;
    }

    // Drift horizontally
    if (randInt(3) == 0) {
        if (isEmpty(x + dir, y)) {
            swapParticles(x, y, x + dir, y);
            return;
        }
        if (isEmpty(x - dir, y)) {
            swapParticles(x, y, x - dir, y);
        }
    }

    // Steam slowly cools
    if (p.temp > 22) p.temp--;
}

// ═══════════════════════════════════════════════════════════════════
// Fire behavior
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateFire(int x, int y, Particle& p) {
    // Decrease life (stored in flags bits 1-7)
    uint8_t life = p.flags >> 1;
    if (life == 0) {
        // Fire dies out — leave smoke/steam or empty
        if (randInt(3) == 0) {
            p.type = (uint8_t)MatType::STEAM;
            p.temp = 100;
            p.flags = 0;
        } else {
            p.type = (uint8_t)MatType::EMPTY;
            p.temp = 80;
            p.flags = 0;
        }
        markChunkDirty(x, y);
        return;
    }
    // Decrement life
    p.flags = (uint8_t)(((life - 1) << 1) | (p.flags & 1));

    // Fire transfers heat to neighbors and can ignite them
    igniteNeighbors(x, y, p.temp);

    // Fire rises
    int dgy = (gy >= 0) ? -1 : 1;
    int ny = y + dgy;
    if (isEmpty(x, ny)) {
        swapParticles(x, y, x, ny);
        return;
    }

    // Drift
    int dir = randBool() ? 1 : -1;
    if (isEmpty(x + dir, ny)) {
        swapParticles(x, y, x + dir, ny);
        return;
    }
    if (isEmpty(x - dir, ny)) {
        swapParticles(x, y, x - dir, ny);
        return;
    }

    // Slight chance of lateral movement
    if (randInt(4) == 0 && isEmpty(x + dir, y)) {
        swapParticles(x, y, x + dir, y);
    }

    // Fire temp fluctuates
    p.temp = (int16_t)(400 + randInt(400));

    // Water extinguishes fire
    static const int dx4[] = {-1, 1, 0, 0};
    static const int dy4[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int nx = x + dx4[d], nny = y + dy4[d];
        if (inBounds(nx, nny) && _grid[PG_IX(nx, nny)].type == (uint8_t)MatType::WATER) {
            p.type  = (uint8_t)MatType::STEAM;
            p.temp  = 100;
            p.flags = 0;
            markChunkDirty(x, y);
            return;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Acid behavior
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateAcid(int x, int y, Particle& p) {
    uint8_t dens = MAT_LUT[p.type].density;

    // Check neighbors — dissolve solids
    static const int dx4[] = {-1, 1, 0, 0};
    static const int dy4[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int nx = x + dx4[d], ny = y + dy4[d];
        if (!inBounds(nx, ny)) continue;
        Particle& nb = _grid[PG_IX(nx, ny)];
        MatState nbState = MAT_LUT[nb.type].state;

        // Dissolve solids and powders (except wall)
        if (nb.type != (uint8_t)MatType::EMPTY &&
            nb.type != (uint8_t)MatType::WALL &&
            nb.type != (uint8_t)MatType::ACID &&
            (nbState == MatState::SOLID || nbState == MatState::POWDER)) {
            if (randInt(8) == 0) {
                nb.type  = (uint8_t)MatType::EMPTY;
                nb.temp  = p.temp;
                nb.flags = 0;
                markChunkDirty(nx, ny);
                // Acid also consumes itself slowly
                if (randInt(4) == 0) {
                    p.type  = (uint8_t)MatType::EMPTY;
                    p.flags = 0;
                    markChunkDirty(x, y);
                    return;
                }
            }
        }
    }

    // Acid flows like a liquid
    updateLiquid(x, y, p);
}

// ═══════════════════════════════════════════════════════════════════
// Heat conduction (Von Neumann neighborhood)
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::conductHeat() {
    // We only do a simple averaging pass — no temp buffer needed
    // since we do it in-place with partial averaging
    static const int dx4[] = {-1, 1, 0, 0};
    static const int dy4[] = {0, 0, -1, 1};

    for (int y = 0; y < PG_H; ++y) {
        int chunkY = y / CHUNK_H;
        for (int x = 0; x < PG_W; ++x) {
            int chunkX = x / CHUNK_W;
            if (!_chunkActive[chunkY * CHUNKS_X + chunkX]) continue;

            int idx = PG_IX(x, y);
            Particle& p = _grid[idx];
            if (p.type == (uint8_t)MatType::EMPTY) continue;

            uint8_t cond = MAT_LUT[p.type].thermalCond;
            if (cond == 0) continue;

            int32_t sum = (int32_t)p.temp * 4;
            int count = 4;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d], ny = y + dy4[d];
                if (inBounds(nx, ny)) {
                    sum += _grid[PG_IX(nx, ny)].temp;
                    count++;
                } else {
                    sum += 22; // ambient at boundary
                    count++;
                }
            }
            int16_t avgTemp = (int16_t)(sum / count);
            // Blend toward average based on conductivity
            int16_t diff = (int16_t)(avgTemp - p.temp);
            p.temp = (int16_t)(p.temp + (diff * cond) / 512);

            // Ambient cooling
            if (p.temp > 22 && p.type != (uint8_t)MatType::FIRE) {
                p.temp--;
            } else if (p.temp < 22 && p.type != (uint8_t)MatType::ICE) {
                p.temp++;
            }
        }
    }
}
