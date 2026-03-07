/**
 * ParticleLabApp.cpp
 * Falling-sand particle simulation — LVGL integration, rendering, input.
 */

#include "ParticleLabApp.h"

#ifdef ARDUINO
#include <esp_heap_caps.h>
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
    , _brushCircle(true)
    , _selectedMat(0)
    , _drawing(false)
    , _thermoMode(false)
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

    // Allocate render buffer (320×240 RGB565 = 153,600 bytes)
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
// updateInfoLabel()
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::updateInfoLabel() {
    static const char* brushNames[] = { "1px", "3px", "5px" };
    const char* matName = ParticleEngine::getMatName(MAT_PALETTE[_selectedMat]);
    const char* shape = _brushCircle ? "O" : "#";

    if (_thermoMode) {
        const Particle& p = _engine.getParticle(_cursorX, _cursorY);
        const char* pName = ParticleEngine::getMatName(p.type);
        snprintf(_infoBuf, sizeof(_infoBuf),
                 "THERMO: %s %dC @(%d,%d)", pName, (int)p.temp, _cursorX, _cursorY);
    } else {
        snprintf(_infoBuf, sizeof(_infoBuf),
                 "%s|B:%s%s|F:%lu",
                 matName, brushNames[_brushRadius], shape,
                 (unsigned long)_engine.frameCount());
    }
    lv_label_set_text(_infoLabel, _infoBuf);
}

// ═══════════════════════════════════════════════════════════════════
// handleKey() — Input handling
// ═══════════════════════════════════════════════════════════════════
void ParticleLabApp::handleKey(const KeyEvent& ev) {
    if (!_screen) return;
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // Cursor speed (1 normally, 4 with rapid repeat)
    int speed = (ev.action == KeyAction::REPEAT) ? 3 : 1;

    switch (ev.code) {
        case KeyCode::UP:
            _cursorY -= speed;
            if (_cursorY < 0) _cursorY = 0;
            break;
        case KeyCode::DOWN:
            _cursorY += speed;
            if (_cursorY >= PG_H) _cursorY = PG_H - 1;
            break;
        case KeyCode::LEFT:
            _cursorX -= speed;
            if (_cursorX < 0) _cursorX = 0;
            break;
        case KeyCode::RIGHT:
            _cursorX += speed;
            if (_cursorX >= PG_W) _cursorX = PG_W - 1;
            break;

        // ENTER: Draw/Emit selected material
        case KeyCode::ENTER:
            _drawing = true;
            _engine.placeBrush(_cursorX, _cursorY, _brushRadius, _brushCircle,
                               (MatType)MAT_PALETTE[_selectedMat]);
            break;

        // EXE: Thermometer tool
        case KeyCode::EXE:
            _thermoMode = !_thermoMode;
            break;

        // DEL: Erase (place EMPTY)
        case KeyCode::DEL:
            _engine.placeBrush(_cursorX, _cursorY, _brushRadius + 1, _brushCircle,
                               MatType::EMPTY);
            break;

        // F1: Toggle brush size (0→1→2→0)
        case KeyCode::F1:
            _brushRadius = (_brushRadius + 1) % 3;
            break;

        // F2: Toggle brush shape (circle/square)
        case KeyCode::F2:
            _brushCircle = !_brushCircle;
            break;

        // F3: Previous material
        case KeyCode::F3:
            _selectedMat = (_selectedMat - 1 + MAT_PALETTE_COUNT) % MAT_PALETTE_COUNT;
            break;

        // F4: Next material
        case KeyCode::F4:
            _selectedMat = (_selectedMat + 1) % MAT_PALETTE_COUNT;
            break;

        // F5: Clear simulation
        case KeyCode::F5:
            _engine.clear();
            break;

        // Number keys: quick material select (1-9 → materials 0-8)
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

        default:
            break;
    }

    updateInfoLabel();
    if (_drawObj) lv_obj_invalidate(_drawObj);
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

    // Blend: Red → Yellow → White
    uint8_t gr, gg, gb;
    if (glow < 500) {
        // Red glow
        gr = 255;
        gg = (uint8_t)(glow * 200 / 500);
        gb = 0;
    } else {
        // Yellow → White
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

            // Add slight variation for natural look (sand, salt)
            if (p.type == (uint8_t)MatType::SAND ||
                p.type == (uint8_t)MatType::SALT ||
                p.type == (uint8_t)MatType::GUNPOWDER) {
                // Use position-based hash for consistent variation
                uint8_t hash = (uint8_t)((px * 7 + py * 13) & 0x0F);
                uint8_t r = (color >> 11) & 0x1F;
                uint8_t g = (color >> 5) & 0x3F;
                uint8_t b = color & 0x1F;
                r = (uint8_t)(r > 2 ? r - (hash & 3) : r);
                g = (uint8_t)(g > 3 ? g - (hash & 3) : g);
                _renderBuf[sy * SCREEN_W + sx]           = (r << 11) | (g << 5) | b;
                _renderBuf[sy * SCREEN_W + sx + 1]       = (r << 11) | (g << 5) | b;
                _renderBuf[(sy + 1) * SCREEN_W + sx]     = (r << 11) | (g << 5) | b;
                _renderBuf[(sy + 1) * SCREEN_W + sx + 1] = (r << 11) | (g << 5) | b;
                continue;
            }

            // Fire: flickering variation
            if (p.type == (uint8_t)MatType::FIRE) {
                uint8_t life = p.flags >> 1;
                // More intense at start, dimmer as it dies
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
                if (_brushCircle) {
                    int d2 = dx * dx + dy * dy;
                    onBorder = (d2 >= (rad - 1) * (rad - 1) && d2 <= rad * rad);
                } else {
                    onBorder = (abs(dx) == rad || abs(dy) == rad);
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

        // Material swatches
        int swatchW = SCREEN_W / MAT_PALETTE_COUNT;
        for (int i = 0; i < MAT_PALETTE_COUNT; ++i) {
            const MaterialProps& mp = ParticleEngine::getMatProps(MAT_PALETTE[i]);
            int sx = i * swatchW + 2;
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
                // Top and bottom borders
                for (int x = sx - 1; x <= sx + sw && x < SCREEN_W; ++x) {
                    if (x >= 0 && sy - 1 >= 0)
                        _renderBuf[(sy - 1) * SCREEN_W + x] = selColor;
                    if (x >= 0 && sy + sh < SCREEN_H)
                        _renderBuf[(sy + sh) * SCREEN_W + x] = selColor;
                }
                // Left and right borders
                for (int y = sy - 1; y <= sy + sh && y < SCREEN_H; ++y) {
                    if (y >= 0 && sx - 1 >= 0)
                        _renderBuf[y * SCREEN_W + (sx - 1)] = selColor;
                    if (y >= 0 && sx + sw < SCREEN_W)
                        _renderBuf[y * SCREEN_W + (sx + sw)] = selColor;
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

    // Run physics tick
    app->_engine.tick();

    // Update info label periodically
    app->updateInfoLabel();

    // Trigger redraw
    if (app->_drawObj) lv_obj_invalidate(app->_drawObj);
}
