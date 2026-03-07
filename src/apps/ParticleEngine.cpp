/**
 * ParticleEngine.cpp
 * Cellular automata engine — physics, thermodynamics, material interactions,
 * electronics (spark cycle), 30+ materials, reaction matrix.
 *
 * "The Alchemy Update" — Powder-Toy-class physics sandbox.
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
//                                       color               dens  flam  state           therm  melt       boil       elec
const MaterialProps ParticleEngine::MAT_LUT[(int)MatType::MAT_COUNT] = {
    /* EMPTY        */ { RGB565(  8,  8, 12),   0,   0, MatState::GAS,     0, INT16_MAX, INT16_MAX, 0 },
    /* WALL         */ { RGB565(128,128,128), 255,   0, MatState::SOLID,  80, INT16_MAX, INT16_MAX, 0 },
    /* SAND         */ { RGB565(210,180, 80), 200,   0, MatState::POWDER, 40, 1500,      INT16_MAX, 0 },
    /* WATER        */ { RGB565( 30, 90,220), 100,   0, MatState::LIQUID, 60, INT16_MIN, 100,       0 },
    /* WOOD         */ { RGB565(120, 80, 40), 220, 180, MatState::SOLID,  20, INT16_MAX, INT16_MAX, 0 },
    /* FIRE         */ { RGB565(255,100, 10),   5, 255, MatState::GAS,   200, INT16_MAX, INT16_MAX, 0 },
    /* OIL          */ { RGB565( 60, 40, 20),  80, 230, MatState::LIQUID, 30, INT16_MIN, 250,       0 },
    /* STEAM        */ { RGB565(200,210,230),  10,   0, MatState::GAS,    50, INT16_MIN, INT16_MIN, 0 },
    /* ICE          */ { RGB565(180,220,255), 180,   0, MatState::SOLID,  90, 0,         INT16_MAX, 0 },
    /* SALT         */ { RGB565(240,240,240), 190,   0, MatState::POWDER, 30, 801,       INT16_MAX, 0 },
    /* GUNPOWDER    */ { RGB565( 60, 60, 60), 195, 255, MatState::POWDER, 10, INT16_MAX, INT16_MAX, 0 },
    /* ACID         */ { RGB565( 80,255, 40),  90,   0, MatState::LIQUID, 40, INT16_MIN, 300,       0 },
    // ── New materials (Alchemy Update) ──
    /* MOLTEN_GLASS */ { RGB565(255,140, 30), 210,   0, MatState::LIQUID,120, INT16_MIN, INT16_MAX, 0 },
    /* GLASS        */ { RGB565(180,210,230), 220,   0, MatState::SOLID,  70, 1000,      INT16_MAX, 0 },
    /* STONE        */ { RGB565(140,140,140), 240,   0, MatState::SOLID,  60, INT16_MAX, INT16_MAX, 0 },
    /* COAL         */ { RGB565( 40, 35, 30), 210,  40, MatState::SOLID,  30, INT16_MAX, INT16_MAX, 0 },
    /* PLANT        */ { RGB565( 30,160, 40), 180,  90, MatState::SOLID,  15, INT16_MAX, INT16_MAX, 0 },
    /* LAVA         */ { RGB565(255, 60, 10), 230,   0, MatState::LIQUID,150, INT16_MIN, INT16_MAX, 0 },
    /* LN2          */ { RGB565(100,180,255),  85,   0, MatState::LIQUID,120, INT16_MIN,-190,       0 },
    /* GAS_MAT      */ { RGB565(160,180,160),   8,   0, MatState::GAS,    20, INT16_MIN, INT16_MIN, 0 },
    /* WIRE         */ { RGB565(180,120, 50), 230,   0, MatState::SOLID,  70, 1085,      INT16_MAX, 1 },
    /* HEATER       */ { RGB565(200, 50, 50), 230,   0, MatState::SOLID,  80, INT16_MAX, INT16_MAX, 1 },
    /* COOLER       */ { RGB565( 50, 80,200), 230,   0, MatState::SOLID,  80, INT16_MAX, INT16_MAX, 1 },
    /* C4           */ { RGB565(200,200,150), 220,   0, MatState::SOLID,  10, INT16_MAX, INT16_MAX, 1 },
    /* HEAC         */ { RGB565(190,200,210), 230,   0, MatState::SOLID, 250, INT16_MAX, INT16_MAX, 0 },
    /* INSL         */ { RGB565(230,220,200), 210, 120, MatState::SOLID,   0, INT16_MAX, INT16_MAX, 0 },
    /* TITAN        */ { RGB565(180,190,200), 240,   0, MatState::SOLID, 140, 1668,      INT16_MAX, 1 },
    /* MOLTEN_TITAN */ { RGB565(255,180,100), 235,   0, MatState::LIQUID,160, INT16_MIN, INT16_MAX, 0 },
    /* SMOKE        */ { RGB565( 80, 80, 80),   6,   0, MatState::GAS,    10, INT16_MIN, INT16_MIN, 0 },
    /* CLONE        */ { RGB565(180, 50,220), 255,   0, MatState::SOLID,   0, INT16_MAX, INT16_MAX, 0 },
    /* IRON         */ { RGB565(160,160,170), 235,   0, MatState::SOLID, 100, 1538,      INT16_MAX, 1 },
};

const char* const ParticleEngine::MAT_NAMES[(int)MatType::MAT_COUNT] = {
    "Empty",   "Wall",    "Sand",    "Water",   "Wood",    "Fire",
    "Oil",     "Steam",   "Ice",     "Salt",    "Gunpowder","Acid",
    "M.Glass", "Glass",   "Stone",   "Coal",    "Plant",   "Lava",
    "LN2",     "Gas",     "Wire",    "Heater",  "Cooler",  "C4",
    "HEAC",    "INSL",    "Titan",   "M.Titan", "Smoke",   "Clone",
    "Iron"
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
static inline int randInt(int maxExcl) {
    if (maxExcl <= 0) return 0;
    return (int)(fastRand() % (uint32_t)maxExcl);
}
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
        _grid[i].temp  = 22;   // ambient 22C
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
    // Wall and Clone are indestructible — cannot overwrite
    if ((_grid[idx].type == (uint8_t)MatType::WALL ||
         _grid[idx].type == (uint8_t)MatType::CLONE) && mat != MatType::EMPTY) return;

    _grid[idx].type  = (uint8_t)mat;
    _grid[idx].flags = 0;

    // Set spawn temperature for each material
    switch (mat) {
        case MatType::FIRE:      _grid[idx].temp = 600; _grid[idx].flags = 60 + randInt(40); break;
        case MatType::ICE:       _grid[idx].temp = -10; break;
        case MatType::STEAM:     _grid[idx].temp = 110; break;
        case MatType::LAVA:      _grid[idx].temp = 1500; break;
        case MatType::LN2:       _grid[idx].temp = -196; break;
        case MatType::MOLTEN_GLASS: _grid[idx].temp = 1200; break;
        case MatType::MOLTEN_TITAN: _grid[idx].temp = 1700; break;
        case MatType::COAL:      _grid[idx].temp = 22; break;
        default:                 _grid[idx].temp = 22; break;
    }
    markChunkDirty(x, y);
}

void ParticleEngine::placeBrush(int cx, int cy, int radius, BrushShape shape, MatType mat) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (shape == BrushShape::CIRCLE && dx * dx + dy * dy > radius * radius) continue;
            if (shape == BrushShape::SPRAY) {
                // Spray: ~40% fill rate within radius
                if (dx * dx + dy * dy > radius * radius) continue;
                if (randInt(100) > 40) continue;
            }
            placeParticle(cx + dx, cy + dy, mat);
        }
    }
}

void ParticleEngine::placeLine(int x0, int y0, int x1, int y1, int radius, BrushShape shape, MatType mat) {
    // Bresenham line algorithm
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    dx = (dx < 0) ? -dx : dx;
    dy = (dy < 0) ? -dy : dy;

    int err = dx - dy;
    int cx = x0, cy = y0;

    for (int step = 0; step < MAX_LINE_STEPS; ++step) {
        placeBrush(cx, cy, radius, shape, mat);
        if (cx == x1 && cy == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
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

bool ParticleEngine::isConductive(uint8_t type) {
    if (type >= (uint8_t)MatType::MAT_COUNT) return false;
    return MAT_LUT[type].electricCond != 0;
}

int ParticleEngine::countParticles() const {
    if (!_grid) return 0;
    int count = 0;
    for (int i = 0; i < PG_SIZE; ++i) {
        if (_grid[i].type != (uint8_t)MatType::EMPTY) count++;
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════════
// Serialization (for save/load)
// ═══════════════════════════════════════════════════════════════════
int ParticleEngine::serialize(uint8_t* buf, int bufSize) const {
    int needed = PG_SIZE * (int)sizeof(Particle);
    if (!_grid || bufSize < needed) return 0;
    memcpy(buf, _grid, (size_t)needed);
    return needed;
}

bool ParticleEngine::deserialize(const uint8_t* buf, int bufSize) {
    int needed = PG_SIZE * (int)sizeof(Particle);
    if (!_grid || bufSize < needed) return false;
    memcpy(_grid, buf, (size_t)needed);
    memset(_chunkActive, 1, sizeof(_chunkActive));
    memset(_chunkDirty, 0, sizeof(_chunkDirty));
    _frameCount = 0;
    return true;
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
    if (otherType == (uint8_t)MatType::CLONE) return false;
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
        if (nb.temp > MAX_TEMP) nb.temp = MAX_TEMP;
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
            if (p.type == (uint8_t)MatType::CLONE) continue;
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
// Reaction Matrix — inline checks for material pair interactions
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::checkReactions(int x, int y) {
    if (!inBounds(x, y)) return;
    Particle& p = _grid[PG_IX(x, y)];
    if (p.type == (uint8_t)MatType::EMPTY) return;

    static const int dx4[] = {-1, 1, 0, 0};
    static const int dy4[] = {0, 0, -1, 1};

    for (int d = 0; d < 4; ++d) {
        int nx = x + dx4[d], ny = y + dy4[d];
        if (!inBounds(nx, ny)) continue;
        Particle& nb = _grid[PG_IX(nx, ny)];
        if (nb.type == (uint8_t)MatType::EMPTY) continue;

        uint8_t t1 = p.type, t2 = nb.type;

        // WATER + LAVA → STONE + STEAM
        if ((t1 == (uint8_t)MatType::WATER && t2 == (uint8_t)MatType::LAVA) ||
            (t1 == (uint8_t)MatType::LAVA  && t2 == (uint8_t)MatType::WATER)) {
            if (t1 == (uint8_t)MatType::WATER) {
                p.type = (uint8_t)MatType::STEAM;
                p.temp = 100;
                p.flags = 0;
                nb.type = (uint8_t)MatType::STONE;
                nb.temp = 600;
                nb.flags = 0;
            } else {
                p.type = (uint8_t)MatType::STONE;
                p.temp = 600;
                p.flags = 0;
                nb.type = (uint8_t)MatType::STEAM;
                nb.temp = 100;
                nb.flags = 0;
            }
            markChunkDirty(x, y);
            markChunkDirty(nx, ny);
            return;
        }

        // ACID + IRON → GAS (acid dissolves iron)
        if (t1 == (uint8_t)MatType::ACID && t2 == (uint8_t)MatType::IRON) {
            if (randInt(6) == 0) {
                p.type = (uint8_t)MatType::GAS_MAT;
                p.temp = 40;
                p.flags = 0;
                nb.type = (uint8_t)MatType::EMPTY;
                nb.flags = 0;
                markChunkDirty(x, y);
                markChunkDirty(nx, ny);
                return;
            }
        }

        // ACID + TITAN → GAS (slow, titanium is resistant)
        if (t1 == (uint8_t)MatType::ACID && t2 == (uint8_t)MatType::TITAN) {
            if (randInt(30) == 0) {
                p.type = (uint8_t)MatType::GAS_MAT;
                p.temp = 40;
                p.flags = 0;
                nb.type = (uint8_t)MatType::EMPTY;
                nb.flags = 0;
                markChunkDirty(x, y);
                markChunkDirty(nx, ny);
                return;
            }
        }

        // WATER + LN2 → ICE (water freezes near liquid nitrogen)
        if (t1 == (uint8_t)MatType::WATER && t2 == (uint8_t)MatType::LN2) {
            if (randInt(4) == 0) {
                p.type = (uint8_t)MatType::ICE;
                p.temp = -40;
                p.flags = 0;
                markChunkDirty(x, y);
                return;
            }
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
        _grid[i].flags &= (uint8_t)~PF_UPDATED;  // clear bit 0
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
    if (p.flags & PF_UPDATED) return; // already updated this frame

    p.flags |= PF_UPDATED; // mark updated

    // ── Spark processing (before other updates) ──
    if (p.flags & PF_SPARKED) {
        updateSpark(x, y, p);
        return;
    }

    // ── Check reactions with neighbors ──
    if ((_frameCount & 3) == 0) {
        checkReactions(x, y);
        // Re-read since reaction may have changed type
        if (p.type == (uint8_t)MatType::EMPTY) return;
    }

    // ── State transitions based on temperature ──
    const MaterialProps& props = MAT_LUT[p.type];

    // Melting: solid/powder -> liquid
    if ((props.state == MatState::SOLID || props.state == MatState::POWDER) &&
        props.meltPoint != INT16_MAX && p.temp > props.meltPoint) {
        if (p.type == (uint8_t)MatType::ICE) {
            p.type = (uint8_t)MatType::WATER;
            p.temp = 1;
        } else if (p.type == (uint8_t)MatType::SAND) {
            p.type = (uint8_t)MatType::MOLTEN_GLASS;
            p.temp = 1500;
        } else if (p.type == (uint8_t)MatType::GLASS) {
            p.type = (uint8_t)MatType::MOLTEN_GLASS;
            p.temp = 1100;
        } else if (p.type == (uint8_t)MatType::IRON) {
            p.type = (uint8_t)MatType::LAVA;
            p.temp = 1540;
        } else if (p.type == (uint8_t)MatType::TITAN) {
            p.type = (uint8_t)MatType::MOLTEN_TITAN;
            p.temp = 1670;
        } else if (p.type == (uint8_t)MatType::WIRE) {
            p.type = (uint8_t)MatType::LAVA;
            p.temp = 1100;
        }
        markChunkDirty(x, y);
        return;
    }

    // Boiling: liquid -> gas
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
        } else if (p.type == (uint8_t)MatType::LN2) {
            p.type = (uint8_t)MatType::GAS_MAT;
            p.temp = -188;
        }
        markChunkDirty(x, y);
        return;
    }

    // MOLTEN_GLASS cools to GLASS below 1000C
    if (p.type == (uint8_t)MatType::MOLTEN_GLASS && p.temp < 1000) {
        p.type = (uint8_t)MatType::GLASS;
        p.temp = 999;
        markChunkDirty(x, y);
        return;
    }

    // LAVA cools to STONE below 800C
    if (p.type == (uint8_t)MatType::LAVA && p.temp < 800) {
        p.type = (uint8_t)MatType::STONE;
        p.temp = 799;
        markChunkDirty(x, y);
        return;
    }

    // MOLTEN_TITAN cools to TITAN below 1600C
    if (p.type == (uint8_t)MatType::MOLTEN_TITAN && p.temp < 1600) {
        p.type = (uint8_t)MatType::TITAN;
        p.temp = 1599;
        markChunkDirty(x, y);
        return;
    }

    // Steam condensation: gas -> liquid when < 90C
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
        case MatType::LAVA:
        case MatType::LN2:
        case MatType::MOLTEN_GLASS:
        case MatType::MOLTEN_TITAN:
            updateLiquid(x, y, p);
            break;
        case MatType::FIRE:
            updateFire(x, y, p);
            break;
        case MatType::STEAM:
        case MatType::GAS_MAT:
        case MatType::SMOKE:
            updateGas(x, y, p);
            break;
        case MatType::ACID:
            updateAcid(x, y, p);
            break;
        case MatType::PLANT:
            updatePlant(x, y, p);
            break;
        case MatType::CLONE:
            updateClone(x, y, p);
            break;
        case MatType::COAL:
            // Coal burns slowly when hot
            if (p.temp > 300) {
                p.type = (uint8_t)MatType::FIRE;
                p.flags = 120 + randInt(60);  // 10x longer than wood
                p.temp = 600;
                markChunkDirty(x, y);
            }
            break;
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Spark update — electronics system
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateSpark(int x, int y, Particle& p) {
    // Spark timer stored in bits 2-3: 0-3 frames
    uint8_t sparkTimer = (p.flags >> 2) & 0x03;

    // Joule heating: spark generates heat in the conductor
    p.temp = (int16_t)(p.temp + JOULE_HEAT);
    if (p.temp > MAX_TEMP) p.temp = MAX_TEMP;

    if (sparkTimer == 0) {
        // Spark expires — propagate to conductive neighbors, clear sparked flag
        p.flags &= (uint8_t)~PF_SPARKED;
        p.flags &= 0x03; // clear extra bits

        // Special reactions for HEATER, COOLER, C4
        if (p.type == (uint8_t)MatType::HEATER) {
            p.temp = 2000;
            markChunkDirty(x, y);
        } else if (p.type == (uint8_t)MatType::COOLER) {
            p.temp = -200;
            markChunkDirty(x, y);
        } else if (p.type == (uint8_t)MatType::C4) {
            explode(x, y, 6 + randInt(4));
            return;
        }

        // Propagate spark to conductive neighbors that are not recently sparked
        static const int dx4[] = {-1, 1, 0, 0};
        static const int dy4[] = {0, 0, -1, 1};
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (!inBounds(nx, ny)) continue;
            Particle& nb = _grid[PG_IX(nx, ny)];
            // Only spark conductive, non-sparked, non-insulator neighbors
            if (nb.type == (uint8_t)MatType::INSL) continue;
            if (isConductive(nb.type) && !(nb.flags & PF_SPARKED)) {
                nb.flags |= PF_SPARKED;
                nb.flags = (uint8_t)((nb.flags & 0x03) | (2 << 2)); // set timer=2
                markChunkDirty(nx, ny);
            }
        }
    } else {
        // Count down spark timer
        sparkTimer--;
        p.flags = (uint8_t)((p.flags & 0x03) | (sparkTimer << 2));
    }
    markChunkDirty(x, y);
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
// Liquid physics (water, oil, lava, LN2, molten glass/titan)
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

    // Equalization: if liquid above, try to move sideways more aggressively
    bool liquidAbove = false;
    if (inBounds(x, y - 1)) {
        uint8_t aboveType = _grid[PG_IX(x, y - 1)].type;
        MatState aboveState = MAT_LUT[aboveType].state;
        if (aboveState == MatState::LIQUID) liquidAbove = true;
    }

    int spread = liquidAbove ? (4 + randInt(3)) : (2 + randInt(3));
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
// Gas physics (steam, smoke, generic gas)
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

    // Diffuse: randomly try to move to any empty neighbor
    if (randInt(3) == 0) {
        int rd = randInt(4);
        static const int ddx[] = {-1, 1, 0, 0};
        static const int ddy[] = {0, 0, -1, 1};
        int nnx = x + ddx[rd], nny = y + ddy[rd];
        if (isEmpty(nnx, nny)) {
            swapParticles(x, y, nnx, nny);
            return;
        }
    }

    // Gas cools toward ambient
    if (p.temp > 22) p.temp--;
    else if (p.temp < 22) p.temp++;

    // Smoke/gas dissipates over time
    if (p.type == (uint8_t)MatType::SMOKE || p.type == (uint8_t)MatType::GAS_MAT) {
        if (randInt(200) == 0) {
            p.type = (uint8_t)MatType::EMPTY;
            p.flags = 0;
            markChunkDirty(x, y);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Fire behavior
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateFire(int x, int y, Particle& p) {
    // Decrease life (stored in flags bits 1-7)
    uint8_t life = p.flags >> 1;
    if (life == 0) {
        // Fire dies out — leave smoke or empty
        if (randInt(3) == 0) {
            p.type = (uint8_t)MatType::SMOKE;
            p.temp = 80;
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

        // Dissolve solids and powders (except wall, clone)
        if (nb.type != (uint8_t)MatType::EMPTY &&
            nb.type != (uint8_t)MatType::WALL &&
            nb.type != (uint8_t)MatType::ACID &&
            nb.type != (uint8_t)MatType::CLONE &&
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
// Plant behavior — grows near water
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updatePlant(int x, int y, Particle& p) {
    static const int dx4[] = {-1, 1, 0, 0};
    static const int dy4[] = {0, 0, -1, 1};

    // Check for water neighbor — 2% chance per frame to grow upward
    bool nearWater = false;
    for (int d = 0; d < 4; ++d) {
        int nx = x + dx4[d], ny = y + dy4[d];
        if (inBounds(nx, ny) && _grid[PG_IX(nx, ny)].type == (uint8_t)MatType::WATER) {
            nearWater = true;
            break;
        }
    }

    if (nearWater && randInt(50) == 0) {  // ~2% chance
        // Try to grow upward
        int uy = y - 1;
        if (inBounds(x, uy) && _grid[PG_IX(x, uy)].type == (uint8_t)MatType::EMPTY) {
            _grid[PG_IX(x, uy)].type = (uint8_t)MatType::PLANT;
            _grid[PG_IX(x, uy)].temp = 22;
            _grid[PG_IX(x, uy)].flags = 0;
            markChunkDirty(x, uy);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Clone behavior — reads and replicates adjacent materials
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::updateClone(int x, int y, Particle& p) {
    // Clone source stored in bits 2-7 of flags (0 = not yet programmed)
    uint8_t cloneSrc = (p.flags >> 2) & 0x3F;

    static const int dx4[] = {-1, 1, 0, 0};
    static const int dy4[] = {0, 0, -1, 1};

    // If not programmed, read first non-clone, non-empty neighbor
    if (cloneSrc == 0) {
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (!inBounds(nx, ny)) continue;
            uint8_t nt = _grid[PG_IX(nx, ny)].type;
            if (nt != (uint8_t)MatType::EMPTY && nt != (uint8_t)MatType::CLONE &&
                nt != (uint8_t)MatType::WALL) {
                cloneSrc = nt;
                p.flags = (uint8_t)((p.flags & 0x03) | (cloneSrc << 2));
                break;
            }
        }
        return;
    }

    // Emit cloned material into empty adjacent spaces
    if (randInt(4) == 0) {
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (inBounds(nx, ny) && _grid[PG_IX(nx, ny)].type == (uint8_t)MatType::EMPTY) {
                placeParticle(nx, ny, (MatType)cloneSrc);
                markChunkDirty(nx, ny);
                break;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Heat conduction (Von Neumann neighborhood)
// ═══════════════════════════════════════════════════════════════════
void ParticleEngine::conductHeat() {
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

            // INSL blocks all heat conduction
            if (p.type == (uint8_t)MatType::INSL) continue;

            uint8_t cond = MAT_LUT[p.type].thermalCond;
            if (cond == 0) continue;

            int32_t sum = (int32_t)p.temp * 4;
            int count = 4;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d], ny = y + dy4[d];
                if (inBounds(nx, ny)) {
                    Particle& nb = _grid[PG_IX(nx, ny)];
                    // Don't conduct through INSL
                    if (nb.type == (uint8_t)MatType::INSL) {
                        sum += p.temp; // treat as self (no transfer)
                    } else {
                        sum += nb.temp;
                    }
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

            // Ambient cooling (except things that maintain temperature)
            if (p.temp > 22 && p.type != (uint8_t)MatType::FIRE) {
                p.temp--;
            } else if (p.temp < 22) {
                p.temp++;
            }

            // Clamp temperature
            if (p.temp > MAX_TEMP) p.temp = MAX_TEMP;
            if (p.temp < MIN_TEMP) p.temp = MIN_TEMP;
        }
    }
}
