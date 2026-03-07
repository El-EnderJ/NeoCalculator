/**
 * ParticleLabApp.cpp
 * Falling-sand particle simulation — LVGL integration, rendering, input.
 *
 * "The Alchemy Update" — 30+ materials, electronics, phase transitions,
 * professional UI overlay, Bresenham line tool, save/load.
 */

#include "ParticleLabApp.h"

#ifdef ARDUINO
#include <esp_heap_caps.h>
#include <LittleFS.h>
#endif

// ═══════════════════════════════════════════════════════════════════
// Material palette (selectable materials in toolbar order)
// ═══════════════════════════════════════════════════════════════════
const uint8_t ParticleLabApp::MAT_PALETTE[MAT_PALETTE_COUNT] = {
    (uint8_t)MatType::SAND,
    (uint8_t)MatType::WATER,
    (uint8_t)MatType::WALL,
    (uint8_t)MatType::WOOD,
    (uint8_t)MatType::FIRE,
    (uint8_t)MatType::OIL,
    (uint8_t)MatType::STEAM,
    (uint8_t)MatType::ICE,
    (uint8_t)MatType::SALT,
    (uint8_t)MatType::GUNPOWDER,
    (uint8_t)MatType::ACID,
    (uint8_t)MatType::STONE,
    (uint8_t)MatType::GLASS,
    (uint8_t)MatType::COAL,
    (uint8_t)MatType::PLANT,
    (uint8_t)MatType::LAVA,
    (uint8_t)MatType::LN2,
    (uint8_t)MatType::IRON,
    (uint8_t)MatType::WIRE,
    (uint8_t)MatType::HEATER,
    (uint8_t)MatType::COOLER,
    (uint8_t)MatType::C4,
    (uint8_t)MatType::HEAC,
    (uint8_t)MatType::INSL,
    (uint8_t)MatType::TITAN,
    (uint8_t)MatType::SMOKE,
    (uint8_t)MatType::CLONE,
    (uint8_t)MatType::EMPTY,   // Eraser as last entry
};

// ═══════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════
ParticleLabApp::ParticleLabApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _infoLabel(nullptr)
    , _simTimer(nullptr)
    , _renderBuf(nullptr)
    , _cursorX(PG_W / 2)
    , _cursorY(PG_H / 2)
    , _brushRadius(0)
    , _brushShape(BrushShape::CIRCLE)
    , _selectedMat(0)
    , _drawing(false)
    , _drawStartX(PG_W / 2)
    , _drawStartY(PG_H / 2)
    , _thermoMode(false)
    , _paletteOpen(false)
    , _paletteCurX(0)
    , _paletteCurY(0)
    , _paused(false)
{
    _infoBuf[0] = '\0';
    memset(&_imgDsc, 0, sizeof(_imgDsc));
}

ParticleLabApp::~ParticleLabApp() {
    end();
}

// ═══════════════════════════════════════════════════════════════════
// begin() — Allocate memory, create LVGL screen
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::begin() {
    if (_screen) return;

    // Initialize particle engine
    if (!_engine.init()) return;

    // Allocate render buffer (320x240 RGB565 = 153,600 bytes)
    const size_t rbSize = SCREEN_W * SCREEN_H * sizeof(uint16_t);
#ifdef ARDUINO
    _renderBuf = (uint16_t*)heap_caps_malloc(rbSize, MALLOC_CAP_SPIRAM);
#else
    _renderBuf = new (std::nothrow) uint16_t[SCREEN_W * SCREEN_H];
#endif
    if (!_renderBuf) {
        _engine.destroy();
        return;
    }
    memset(_renderBuf, 0, rbSize);

    // Setup LVGL image descriptor
    _imgDsc.header.w  = SCREEN_W;
    _imgDsc.header.h  = SCREEN_H;
    _imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _imgDsc.data_size = rbSize;
    _imgDsc.data      = (const uint8_t*)_renderBuf;

    // Create LVGL screen
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Particle Lab");

    createUI();

    _cursorX = PG_W / 2;
    _cursorY = PG_H / 2;
    _drawing = false;
    _thermoMode = false;
    _paletteOpen = false;
    _paused = false;
}

// ═══════════════════════════════════════════════════════════════════
// end() — Cleanup
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::end() {
    if (_screen) {
        if (_simTimer) {
            lv_timer_delete(_simTimer);
            _simTimer = nullptr;
        }
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen    = nullptr;
        _statusBar.resetPointers();
        _drawObj   = nullptr;
        _infoLabel = nullptr;

        _engine.destroy();

#ifdef ARDUINO
        if (_renderBuf) { heap_caps_free(_renderBuf); _renderBuf = nullptr; }
#else
        delete[] _renderBuf; _renderBuf = nullptr;
#endif
    }
}

// ═══════════════════════════════════════════════════════════════════
// load() — Display screen
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Particle Lab");
    _statusBar.update();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ═══════════════════════════════════════════════════════════════════
// createUI() — Draw area and toolbar
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::createUI() {
    // Draw area (full screen below status bar)
    _drawObj = lv_obj_create(_screen);
    lv_obj_set_size(_drawObj, SCREEN_W, DRAW_H);
    lv_obj_set_pos(_drawObj, 0, DRAW_Y);
    lv_obj_set_style_bg_opa(_drawObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_drawObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_drawObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_drawObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(_drawObj, this);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Info label
    _infoLabel = lv_label_create(_screen);
    lv_obj_set_pos(_infoLabel, 4, SCREEN_H - INFO_H - 2);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    updateInfoLabel();

    // Simulation timer (~33ms = 30 FPS physics)
    _simTimer = lv_timer_create(onSimTimer, 33, this);
}

// ═══════════════════════════════════════════════════════════════════
// updateInfoLabel() — HUD info bar
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::updateInfoLabel() {
    static const char* shapeNames[] = { "O", "#", "~" };  // circle, square, spray
    static const char* brushNames[] = { "1px", "3px", "5px" };
    const char* matName = ParticleEngine::getMatName(MAT_PALETTE[_selectedMat]);
    const char* shape = shapeNames[(int)_brushShape];

    if (_thermoMode) {
        const Particle& p = _engine.getParticle(_cursorX, _cursorY);
        const char* pName = ParticleEngine::getMatName(p.type);
        bool sparked = (p.flags & PF_SPARKED) != 0;
        snprintf(_infoBuf, sizeof(_infoBuf),
                 "%s%s %dC @(%d,%d)",
                 sparked ? "(SPK)" : "", pName, (int)p.temp, _cursorX, _cursorY);
    } else {
        snprintf(_infoBuf, sizeof(_infoBuf),
                 "MAT:%s|B:%s%s|%d,%d|P:%d%s",
                 matName, brushNames[_brushRadius], shape,
                 _cursorX, _cursorY,
                 _engine.countParticles(),
                 _paused ? "|PAUSED" : "");
    }
    lv_label_set_text(_infoLabel, _infoBuf);
}

// ═══════════════════════════════════════════════════════════════════
// handleKey() — Input handling
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::handleKey(const KeyEvent& ev) {
    if (!_screen) return;
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // ── Palette overlay mode ──
    if (_paletteOpen) {
        switch (ev.code) {
            case KeyCode::UP:
                _paletteCurY = (_paletteCurY - 1 + PAL_ROWS) % PAL_ROWS;
                break;
            case KeyCode::DOWN:
                _paletteCurY = (_paletteCurY + 1) % PAL_ROWS;
                break;
            case KeyCode::LEFT:
                _paletteCurX = (_paletteCurX - 1 + PAL_COLS) % PAL_COLS;
                break;
            case KeyCode::RIGHT:
                _paletteCurX = (_paletteCurX + 1) % PAL_COLS;
                break;
            case KeyCode::ENTER: {
                int idx = _paletteCurY * PAL_COLS + _paletteCurX;
                if (idx < MAT_PALETTE_COUNT) {
                    _selectedMat = idx;
                }
                _paletteOpen = false;
                _paused = false;
                break;
            }
            case KeyCode::F3:
                _paletteOpen = false;
                _paused = false;
                break;
            default:
                break;
        }
        updateInfoLabel();
        if (_drawObj) lv_obj_invalidate(_drawObj);
        return;
    }

    // Cursor speed (1 normally, 3 with rapid repeat)
    int speed = (ev.action == KeyAction::REPEAT) ? 3 : 1;

    switch (ev.code) {
        case KeyCode::UP:
            _cursorY -= speed;
            if (_cursorY < 0) _cursorY = 0;
            if (_drawing) {
                _engine.placeLine(_drawStartX, _drawStartY, _cursorX, _cursorY,
                                  _brushRadius, _brushShape, (MatType)MAT_PALETTE[_selectedMat]);
                _drawStartX = _cursorX;
                _drawStartY = _cursorY;
            }
            break;
        case KeyCode::DOWN:
            _cursorY += speed;
            if (_cursorY >= PG_H) _cursorY = PG_H - 1;
            if (_drawing) {
                _engine.placeLine(_drawStartX, _drawStartY, _cursorX, _cursorY,
                                  _brushRadius, _brushShape, (MatType)MAT_PALETTE[_selectedMat]);
                _drawStartX = _cursorX;
                _drawStartY = _cursorY;
            }
            break;
        case KeyCode::LEFT:
            _cursorX -= speed;
            if (_cursorX < 0) _cursorX = 0;
            if (_drawing) {
                _engine.placeLine(_drawStartX, _drawStartY, _cursorX, _cursorY,
                                  _brushRadius, _brushShape, (MatType)MAT_PALETTE[_selectedMat]);
                _drawStartX = _cursorX;
                _drawStartY = _cursorY;
            }
            break;
        case KeyCode::RIGHT:
            _cursorX += speed;
            if (_cursorX >= PG_W) _cursorX = PG_W - 1;
            if (_drawing) {
                _engine.placeLine(_drawStartX, _drawStartY, _cursorX, _cursorY,
                                  _brushRadius, _brushShape, (MatType)MAT_PALETTE[_selectedMat]);
                _drawStartX = _cursorX;
                _drawStartY = _cursorY;
            }
            break;

        // ENTER: Draw/Emit selected material (Bresenham line when moving)
        case KeyCode::ENTER:
            if (!_drawing) {
                _drawing = true;
                _drawStartX = _cursorX;
                _drawStartY = _cursorY;
            }
            _engine.placeBrush(_cursorX, _cursorY, _brushRadius, _brushShape,
                               (MatType)MAT_PALETTE[_selectedMat]);
            break;

        // EXE: Thermometer tool
        case KeyCode::EXE:
            _thermoMode = !_thermoMode;
            break;

        // DEL: Erase (place EMPTY)
        case KeyCode::DEL:
            _engine.placeBrush(_cursorX, _cursorY, _brushRadius + 1, _brushShape,
                               MatType::EMPTY);
            break;

        // F1: Toggle brush size (0->1->2->0)
        case KeyCode::F1:
            _brushRadius = (_brushRadius + 1) % 3;
            break;

        // F2: Toggle brush shape (circle->square->spray->circle)
        case KeyCode::F2:
            _brushShape = (BrushShape)(((int)_brushShape + 1) % (int)BrushShape::SHAPE_COUNT);
            break;

        // F3: Material palette overlay
        case KeyCode::F3:
            _paletteOpen = !_paletteOpen;
            _paused = _paletteOpen;
            _paletteCurX = _selectedMat % PAL_COLS;
            _paletteCurY = _selectedMat / PAL_COLS;
            break;

        // F4: Quick Save
        case KeyCode::F4:
            saveSandbox();
            break;

        // F5: Quick Load
        case KeyCode::F5:
            loadSandbox();
            break;

        // Number keys: quick material select (1-9 -> materials 0-8)
        case KeyCode::NUM_1: _selectedMat = 0;  break;
        case KeyCode::NUM_2: _selectedMat = 1;  break;
        case KeyCode::NUM_3: _selectedMat = 2;  break;
        case KeyCode::NUM_4: _selectedMat = 3;  break;
        case KeyCode::NUM_5: _selectedMat = 4;  break;
        case KeyCode::NUM_6: _selectedMat = 5;  break;
        case KeyCode::NUM_7: _selectedMat = 6;  break;
        case KeyCode::NUM_8: _selectedMat = 7;  break;
        case KeyCode::NUM_9: _selectedMat = 8;  break;
        case KeyCode::NUM_0: _selectedMat = (_selectedMat + 1) % MAT_PALETTE_COUNT; break;

        // SOLVE key: clear simulation
        case KeyCode::SOLVE:
            _engine.clear();
            break;

        default:
            break;
    }

    // Reset drawing state if ENTER not held
    if (ev.code != KeyCode::ENTER && ev.code != KeyCode::UP &&
        ev.code != KeyCode::DOWN && ev.code != KeyCode::LEFT &&
        ev.code != KeyCode::RIGHT) {
        _drawing = false;
    }

    updateInfoLabel();
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

// ═══════════════════════════════════════════════════════════════════
// Save / Load sandbox to LittleFS
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::saveSandbox() {
#ifdef ARDUINO
    const int gridBytes = PG_SIZE * (int)sizeof(Particle);
    uint8_t* buf = (uint8_t*)heap_caps_malloc(gridBytes, MALLOC_CAP_SPIRAM);
    if (!buf) return;

    int written = _engine.serialize(buf, gridBytes);
    if (written > 0) {
        File f = LittleFS.open("/save.pt", "w");
        if (f) {
            f.write(buf, written);
            f.close();
        }
    }
    heap_caps_free(buf);
#endif
}

void ParticleLabApp::loadSandbox() {
#ifdef ARDUINO
    const int gridBytes = PG_SIZE * (int)sizeof(Particle);
    if (!LittleFS.exists("/save.pt")) return;

    File f = LittleFS.open("/save.pt", "r");
    if (!f) return;

    uint8_t* buf = (uint8_t*)heap_caps_malloc(gridBytes, MALLOC_CAP_SPIRAM);
    if (!buf) { f.close(); return; }

    int bytesRead = (int)f.read(buf, gridBytes);
    f.close();

    if (bytesRead == gridBytes) {
        _engine.deserialize(buf, gridBytes);
    }
    heap_caps_free(buf);
#endif
}

// ═══════════════════════════════════════════════════════════════════
// Rendering — Fill _renderBuf with 2x upscaled particles
// ═══════════════════════════════════════════════════════════════════

// Blend base color with temperature glow (black-body radiation)
uint16_t ParticleLabApp::getTempGlowColor(uint16_t baseColor, int16_t temp) {
    if (temp <= 500) return baseColor;

    // Extract base RGB565 components
    uint8_t r = (baseColor >> 11) & 0x1F;
    uint8_t g = (baseColor >> 5)  & 0x3F;
    uint8_t b = baseColor & 0x1F;

    // Scale to 8-bit
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    // Black-body radiation approximation
    int glow = (temp - 500);
    if (glow > 1000) glow = 1000;

    // Blend: Red -> Yellow -> White
    uint8_t gr, gg, gb;
    if (glow < 500) {
        // Red glow
        gr = 255;
        gg = (uint8_t)(glow * 200 / 500);
        gb = 0;
    } else {
        // Yellow -> White
        gr = 255;
        gg = 200 + (uint8_t)((glow - 500) * 55 / 500);
        gb = (uint8_t)((glow - 500) * 255 / 500);
    }

    int alpha = glow * 200 / 1000;
    if (alpha > 200) alpha = 200;

    r = (uint8_t)((r * (255 - alpha) + gr * alpha) / 255);
    g = (uint8_t)((g * (255 - alpha) + gg * alpha) / 255);
    b = (uint8_t)((b * (255 - alpha) + gb * alpha) / 255);

    return RGB565(r, g, b);
}

void ParticleLabApp::renderToBuffer() {
    if (!_renderBuf || !_engine.grid()) return;

    const Particle* grid = _engine.grid();

    // Fill status bar area black
    for (int y = 0; y < STATUS_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            _renderBuf[y * SCREEN_W + x] = 0x0000;
        }
    }

    // Render particles with 2x upscale
    for (int py = 0; py < PG_H; ++py) {
        int sy = STATUS_H + py * 2;  // screen Y (after status bar)
        if (sy + 1 >= SCREEN_H) break;

        for (int px = 0; px < PG_W; ++px) {
            int sx = px * 2;  // screen X

            const Particle& p = grid[PG_IX(px, py)];
            const MaterialProps& props = ParticleEngine::getMatProps(p.type);

            // Get color with temperature glow
            uint16_t color = getTempGlowColor(props.color, p.temp);

            // Cold glow for sub-zero temperatures
            if (p.temp < -50) {
                int coldGlow = (-50 - p.temp);
                if (coldGlow > 200) coldGlow = 200;
                uint8_t r = (color >> 11) & 0x1F;
                uint8_t g = (color >> 5) & 0x3F;
                uint8_t b = color & 0x1F;
                r = (uint8_t)((r << 3) | (r >> 2));
                g = (uint8_t)((g << 2) | (g >> 4));
                b = (uint8_t)((b << 3) | (b >> 2));
                int alpha = coldGlow * 150 / 200;
                r = (uint8_t)((r * (255 - alpha) + 100 * alpha) / 255);
                g = (uint8_t)((g * (255 - alpha) + 180 * alpha) / 255);
                b = (uint8_t)((b * (255 - alpha) + 255 * alpha) / 255);
                color = RGB565(r, g, b);
            }

            // Sparked particles: yellow tint overlay
            if (p.flags & PF_SPARKED) {
                uint8_t r = (color >> 11) & 0x1F;
                uint8_t g = (color >> 5) & 0x3F;
                uint8_t b = color & 0x1F;
                r = (r > 20) ? 31 : (uint8_t)(r + 11);
                g = (g > 50) ? 63 : (uint8_t)(g + 13);
                b = (b > 4) ? (uint8_t)(b - 4) : 0;
                color = (r << 11) | (g << 5) | b;
            }

            // Add slight variation for natural look (powders)
            if (p.type == (uint8_t)MatType::SAND ||
                p.type == (uint8_t)MatType::SALT ||
                p.type == (uint8_t)MatType::GUNPOWDER) {
                uint8_t hash = (uint8_t)((px * 7 + py * 13) & 0x0F);
                uint8_t r = (color >> 11) & 0x1F;
                uint8_t g = (color >> 5) & 0x3F;
                uint8_t b = color & 0x1F;
                r = (uint8_t)(r > 2 ? r - (hash & 3) : r);
                g = (uint8_t)(g > 3 ? g - (hash & 3) : g);
                color = (r << 11) | (g << 5) | b;
            }

            // Fire: flickering variation
            if (p.type == (uint8_t)MatType::FIRE) {
                uint8_t life = p.flags >> 1;
                uint8_t r = (uint8_t)(31);
                uint8_t g = (uint8_t)(8 + (life & 0x1F));
                if (g > 63) g = 63;
                uint8_t b = (uint8_t)(life > 30 ? 4 : 0);
                color = (r << 11) | (g << 5) | b;
            }

            // Write 2x2 pixel block
            _renderBuf[sy * SCREEN_W + sx]           = color;
            _renderBuf[sy * SCREEN_W + sx + 1]       = color;
            _renderBuf[(sy + 1) * SCREEN_W + sx]     = color;
            _renderBuf[(sy + 1) * SCREEN_W + sx + 1] = color;
        }
    }

    // Draw cursor overlay
    {
        int cx = _cursorX * 2;
        int cy = STATUS_H + _cursorY * 2;
        int rad = (_brushRadius + 1) * 2;

        // Cursor color: gold for drawing, cyan for thermo mode
        uint16_t curColor = _thermoMode ? RGB565(0, 255, 255) : RGB565(255, 215, 0);

        // Draw cursor outline
        for (int dy = -rad; dy <= rad; ++dy) {
            for (int dx = -rad; dx <= rad; ++dx) {
                int sx = cx + dx, sy = cy + dy;
                if (sx < 0 || sx >= SCREEN_W || sy < STATUS_H || sy >= SCREEN_H) continue;

                bool onBorder;
                if (_brushShape == BrushShape::SQUARE) {
                    onBorder = (abs(dx) == rad || abs(dy) == rad);
                } else {
                    int d2 = dx * dx + dy * dy;
                    onBorder = (d2 >= (rad - 1) * (rad - 1) && d2 <= rad * rad);
                }

                if (onBorder) {
                    _renderBuf[sy * SCREEN_W + sx] = curColor;
                }
            }
        }
    }

    // Draw toolbar at bottom
    {
        int toolY = SCREEN_H - TOOLBAR_H;
        // Toolbar background
        for (int y = toolY; y < SCREEN_H - INFO_H; ++y) {
            for (int x = 0; x < SCREEN_W; ++x) {
                _renderBuf[y * SCREEN_W + x] = RGB565(0x1A, 0x1A, 0x22);
            }
        }

        // Show a window of materials around the selected one
        int visibleCount = 11;
        int halfVis = visibleCount / 2;
        int startIdx = _selectedMat - halfVis;
        if (startIdx < 0) startIdx = 0;
        if (startIdx + visibleCount > MAT_PALETTE_COUNT) startIdx = MAT_PALETTE_COUNT - visibleCount;
        if (startIdx < 0) startIdx = 0;

        int swatchW = SCREEN_W / visibleCount;
        for (int vi = 0; vi < visibleCount && (startIdx + vi) < MAT_PALETTE_COUNT; ++vi) {
            int i = startIdx + vi;
            const MaterialProps& mp = ParticleEngine::getMatProps(MAT_PALETTE[i]);
            int sx = vi * swatchW + 2;
            int sy = toolY + 2;
            int sw = swatchW - 4;
            int sh = TOOLBAR_H - INFO_H - 4;
            if (sh < 2) sh = 2;

            for (int y = sy; y < sy + sh && y < SCREEN_H; ++y) {
                for (int x = sx; x < sx + sw && x < SCREEN_W; ++x) {
                    _renderBuf[y * SCREEN_W + x] = mp.color;
                }
            }

            // Highlight selected material
            if (i == _selectedMat) {
                uint16_t selColor = RGB565(0x40, 0xFF, 0x40);
                for (int x = sx - 1; x <= sx + sw && x < SCREEN_W; ++x) {
                    if (x >= 0 && sy - 1 >= 0)
                        _renderBuf[(sy - 1) * SCREEN_W + x] = selColor;
                    if (x >= 0 && sy + sh < SCREEN_H)
                        _renderBuf[(sy + sh) * SCREEN_W + x] = selColor;
                }
                for (int y = sy - 1; y <= sy + sh && y < SCREEN_H; ++y) {
                    if (y >= 0 && sx - 1 >= 0)
                        _renderBuf[y * SCREEN_W + (sx - 1)] = selColor;
                    if (y >= 0 && sx + sw < SCREEN_W)
                        _renderBuf[y * SCREEN_W + (sx + sw)] = selColor;
                }
            }
        }
    }

    // Draw palette overlay if open
    if (_paletteOpen) {
        renderPaletteOverlay();
    }
}

// ═══════════════════════════════════════════════════════════════════
// Palette overlay rendering
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::renderPaletteOverlay() {
    // Semi-transparent dark overlay
    for (int y = STATUS_H; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            uint16_t c = _renderBuf[y * SCREEN_W + x];
            uint8_t r = ((c >> 11) & 0x1F) >> 1;
            uint8_t g = ((c >> 5) & 0x3F) >> 1;
            uint8_t b = (c & 0x1F) >> 1;
            _renderBuf[y * SCREEN_W + x] = (r << 11) | (g << 5) | b;
        }
    }

    // Grid of material swatches
    int gridW = PAL_COLS * 40;
    int gridH = PAL_ROWS * 40;
    int startX = (SCREEN_W - gridW) / 2;
    int startY = STATUS_H + (DRAW_H - gridH) / 2;

    for (int row = 0; row < PAL_ROWS; ++row) {
        for (int col = 0; col < PAL_COLS; ++col) {
            int idx = row * PAL_COLS + col;
            if (idx >= MAT_PALETTE_COUNT) continue;

            const MaterialProps& mp = ParticleEngine::getMatProps(MAT_PALETTE[idx]);
            int sx = startX + col * 40 + 2;
            int sy = startY + row * 40 + 2;
            int sw = 36;
            int sh = 36;

            // Fill swatch
            for (int y = sy; y < sy + sh && y < SCREEN_H; ++y) {
                for (int x = sx; x < sx + sw && x < SCREEN_W; ++x) {
                    if (x >= 0 && y >= 0) {
                        _renderBuf[y * SCREEN_W + x] = mp.color;
                    }
                }
            }

            // Highlight cursor position in palette
            if (col == _paletteCurX && row == _paletteCurY) {
                uint16_t hlColor = RGB565(255, 255, 0);
                for (int x = sx - 2; x < sx + sw + 2 && x < SCREEN_W; ++x) {
                    if (x >= 0) {
                        if (sy - 2 >= 0) _renderBuf[(sy - 2) * SCREEN_W + x] = hlColor;
                        if (sy + sh + 1 < SCREEN_H) _renderBuf[(sy + sh + 1) * SCREEN_W + x] = hlColor;
                    }
                }
                for (int y = sy - 2; y < sy + sh + 2 && y < SCREEN_H; ++y) {
                    if (y >= 0) {
                        if (sx - 2 >= 0) _renderBuf[y * SCREEN_W + (sx - 2)] = hlColor;
                        if (sx + sw + 1 < SCREEN_W) _renderBuf[y * SCREEN_W + (sx + sw + 1)] = hlColor;
                    }
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// LVGL Draw callback — blit _renderBuf as image
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::onDraw(lv_event_t* e) {
    ParticleLabApp* app = static_cast<ParticleLabApp*>(lv_event_get_user_data(e));
    if (!app || !app->_screen || !app->_renderBuf) return;

    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer) return;

    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    // Render particles to buffer
    app->renderToBuffer();

    // Draw the image
    lv_draw_image_dsc_t dsc;
    lv_draw_image_dsc_init(&dsc);
    dsc.src = &app->_imgDsc;

    lv_area_t area = {
        coords.x1, coords.y1,
        (int16_t)(coords.x1 + SCREEN_W - 1),
        (int16_t)(coords.y1 + DRAW_H - 1)
    };
    lv_draw_image(layer, &dsc, &area);
}

// ═══════════════════════════════════════════════════════════════════
// Simulation timer callback
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::onSimTimer(lv_timer_t* timer) {
    ParticleLabApp* app = static_cast<ParticleLabApp*>(lv_timer_get_user_data(timer));
    if (!app || !app->_screen) return;

    // Run physics tick (skip if paused)
    if (!app->_paused) {
        app->_engine.tick();
    }

    // Update info label periodically
    app->updateInfoLabel();

    // Trigger redraw
    if (app->_drawObj) lv_obj_invalidate(app->_drawObj);
}
