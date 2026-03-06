/**
 * BridgeDesignerApp.cpp — Bridge Structural Simulator for NumOS.
 *
 * LVGL-native app: grid-based bridge editor with Verlet physics engine.
 * Nodes and beams in PSRAM; custom draw via LV_EVENT_DRAW_MAIN.
 */

#include "BridgeDesignerApp.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

// ══ Color palette (dark engineering theme) ═══════════════════════════════════
static constexpr uint32_t COL_BG          = 0x0D1117;
static constexpr uint32_t COL_GRID        = 0x1A2332;
static constexpr uint32_t COL_NODE        = 0x58A6FF;
static constexpr uint32_t COL_NODE_FIXED  = 0xFF4444;
static constexpr uint32_t COL_NODE_DECK   = 0x3FB950;
static constexpr uint32_t COL_CURSOR      = 0xFFD700;
static constexpr uint32_t COL_TOOLBAR_BG  = 0x161B22;
static constexpr uint32_t COL_TOOL_ACTIVE = 0x1F6FEB;
static constexpr uint32_t COL_TEXT        = 0xE0E0E0;
static constexpr uint32_t COL_TEXT_DIM    = 0x808080;
static constexpr uint32_t COL_TRUCK       = 0xFF6B00;
static constexpr uint32_t COL_CAR         = 0x58A6FF;
static constexpr uint32_t COL_PREVIEW     = 0x4488FF;

// ══ Layout constants ═════════════════════════════════════════════════════════
static constexpr int TOOLBAR_Y  = 24;   // below StatusBar
static constexpr int TOOLBAR_H  = 18;
static constexpr int DRAW_Y     = 42;   // below toolbar
static constexpr int DRAW_H     = 198;  // rest of 240px screen
static constexpr int INFO_H     = 14;   // info label overlay height

// Tool names for toolbar
static const char* TOOL_NAMES[] = { "WOOD", "STEEL", "CABLE", "DEL" };
static constexpr int TOOL_COUNT = 4;

// ══ Constructor / Destructor ═════════════════════════════════════════════════

BridgeDesignerApp::BridgeDesignerApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _toolbarObj(nullptr)
    , _infoLabel(nullptr)
    , _physicsTimer(nullptr)
    , _nodes(nullptr)
    , _nodeCount(0)
    , _beams(nullptr)
    , _beamCount(0)
    , _appMode(AppMode::EDIT_MODE)
    , _currentTool(ToolType::TOOL_WOOD)
    , _cursorX(160), _cursorY(160)
    , _selectedNode(-1)
    , _startBeamNode(-1)
    , _viewOffX(0.0f), _viewOffY(0.0f)
{
    _infoBuf[0] = '\0';
    for (int i = 0; i < MAX_VEHICLES; ++i) {
        _vehicles[i].pos    = 0.0f;
        _vehicles[i].mass   = 0.0f;
        _vehicles[i].active = false;
        _vehicles[i].type   = VehicleType::TRUCK;
    }
}

BridgeDesignerApp::~BridgeDesignerApp() {
    end();
}

// ══ Lifecycle ════════════════════════════════════════════════════════════════

void BridgeDesignerApp::begin() {
    if (_screen) return;

    // Allocate node/beam arrays in PSRAM (or heap on desktop)
#ifdef ARDUINO
    _nodes = (BridgeNode*)heap_caps_malloc(MAX_NODES * sizeof(BridgeNode), MALLOC_CAP_SPIRAM);
    _beams = (BridgeBeam*)heap_caps_malloc(MAX_BEAMS * sizeof(BridgeBeam), MALLOC_CAP_SPIRAM);
#else
    _nodes = new BridgeNode[MAX_NODES];
    _beams = new BridgeBeam[MAX_BEAMS];
#endif

    if (!_nodes || !_beams) return;  // allocation failure guard
    memset(_nodes, 0, MAX_NODES * sizeof(BridgeNode));
    memset(_beams, 0, MAX_BEAMS * sizeof(BridgeBeam));
    _nodeCount = 0;
    _beamCount = 0;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Bridge Designer");

    createUI();

    // Default anchor points: left and right fixed deck nodes
    addNode(60.0f,  160.0f, true, true);
    addNode(260.0f, 160.0f, true, true);

    _cursorX = 160;
    _cursorY = 160;
    _selectedNode  = -1;
    _startBeamNode = -1;
    _appMode       = AppMode::EDIT_MODE;
    _currentTool   = ToolType::TOOL_WOOD;
}

void BridgeDesignerApp::end() {
    if (_screen) {
        if (_physicsTimer) {
            lv_timer_delete(_physicsTimer);
            _physicsTimer = nullptr;
        }
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen     = nullptr;
        _statusBar.resetPointers();

        _drawObj    = nullptr;
        _toolbarObj = nullptr;
        _infoLabel  = nullptr;

        // Free PSRAM/heap arrays
#ifdef ARDUINO
        if (_nodes) { heap_caps_free(_nodes); _nodes = nullptr; }
        if (_beams) { heap_caps_free(_beams); _beams = nullptr; }
#else
        delete[] _nodes; _nodes = nullptr;
        delete[] _beams; _beams = nullptr;
#endif

        _nodeCount     = 0;
        _beamCount     = 0;
        _selectedNode  = -1;
        _startBeamNode = -1;
        _appMode       = AppMode::EDIT_MODE;
    }
}

void BridgeDesignerApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Bridge Designer");
    _statusBar.update();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ══ UI Construction ══════════════════════════════════════════════════════════

void BridgeDesignerApp::createUI() {
    createToolbar();

    // Custom-drawn bridge area
    _drawObj = lv_obj_create(_screen);
    lv_obj_set_size(_drawObj, SCREEN_W, DRAW_H);
    lv_obj_set_pos(_drawObj, 0, DRAW_Y);
    lv_obj_set_style_bg_opa(_drawObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_drawObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_drawObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_drawObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(_drawObj, this);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Info label (bottom overlay)
    _infoLabel = lv_label_create(_screen);
    lv_obj_set_pos(_infoLabel, 4, SCREEN_H - INFO_H - 2);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    lv_label_set_text(_infoLabel, "Arrows:move  ENTER:place  EXE:sim");
}

void BridgeDesignerApp::createToolbar() {
    _toolbarObj = lv_obj_create(_screen);
    lv_obj_set_size(_toolbarObj, SCREEN_W, TOOLBAR_H);
    lv_obj_set_pos(_toolbarObj, 0, TOOLBAR_Y);
    lv_obj_set_style_bg_color(_toolbarObj, lv_color_hex(COL_TOOLBAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_toolbarObj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_toolbarObj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_toolbarObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_toolbarObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_toolbarObj, LV_OBJ_FLAG_SCROLLABLE);

    int tabW = SCREEN_W / TOOL_COUNT;
    for (int i = 0; i < TOOL_COUNT; ++i) {
        lv_obj_t* btn = lv_obj_create(_toolbarObj);
        lv_obj_set_size(btn, tabW, TOOLBAR_H);
        lv_obj_set_pos(btn, i * tabW, 0);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        bool active = (i == static_cast<int>(_currentTool));
        lv_obj_set_style_bg_color(btn, lv_color_hex(active ? COL_TOOL_ACTIVE : COL_TOOLBAR_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, TOOL_NAMES[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_center(lbl);
    }
}

/** Update toolbar highlight to reflect _currentTool. */
static void updateToolbarHighlight(lv_obj_t* toolbarObj, int activeTool) {
    if (!toolbarObj) return;
    uint32_t childCount = lv_obj_get_child_count(toolbarObj);
    for (uint32_t i = 0; i < childCount && i < TOOL_COUNT; ++i) {
        lv_obj_t* btn = lv_obj_get_child(toolbarObj, (int32_t)i);
        bool active = ((int)i == activeTool);
        lv_obj_set_style_bg_color(btn, lv_color_hex(active ? COL_TOOL_ACTIVE : COL_TOOLBAR_BG), LV_PART_MAIN);
    }
}

// ══ Custom Draw Callback ═════════════════════════════════════════════════════

void BridgeDesignerApp::onDraw(lv_event_t* e) {
    BridgeDesignerApp* app = static_cast<BridgeDesignerApp*>(lv_event_get_user_data(e));
    if (!app || !app->_screen) return;

    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer || !obj) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int ox = coords.x1;
    int oy = coords.y1;

    // ── Background ───────────────────────────────────────────────────────
    {
        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = lv_color_hex(COL_BG);
        bg.bg_opa   = LV_OPA_COVER;
        bg.radius   = 0;
        lv_draw_rect(layer, &bg, &coords);
    }

    // ── Grid dots (edit mode only) ───────────────────────────────────────
    if (app->_appMode == AppMode::EDIT_MODE) {
        lv_draw_rect_dsc_t dotDsc;
        lv_draw_rect_dsc_init(&dotDsc);
        dotDsc.bg_color = lv_color_hex(COL_GRID);
        dotDsc.bg_opa   = LV_OPA_COVER;
        dotDsc.radius   = 0;

        for (int gx = 0; gx < SCREEN_W; gx += GRID_SNAP) {
            for (int gy = 0; gy < DRAW_H; gy += GRID_SNAP) {
                lv_area_t dot = {
                    (int32_t)(ox + gx), (int32_t)(oy + gy),
                    (int32_t)(ox + gx), (int32_t)(oy + gy)
                };
                lv_draw_rect(layer, &dotDsc, &dot);
            }
        }
    }

    // ── Beams ────────────────────────────────────────────────────────────
    for (int i = 0; i < app->_beamCount; ++i) {
        const BridgeBeam& bm = app->_beams[i];
        if (bm.a < 0 || bm.a >= app->_nodeCount) continue;
        if (bm.b < 0 || bm.b >= app->_nodeCount) continue;

        const BridgeNode& na = app->_nodes[bm.a];
        const BridgeNode& nb = app->_nodes[bm.b];

        lv_draw_line_dsc_t lineDsc;
        lv_draw_line_dsc_init(&lineDsc);
        lineDsc.width = 2;

        if (bm.broken) {
            lineDsc.color = lv_color_hex(COL_TEXT_DIM);
            lineDsc.opa   = LV_OPA_40;
        } else {
            lineDsc.color = lerpColor(bm.stress);
            lineDsc.opa   = LV_OPA_COVER;
        }

        lineDsc.p1.x = ox + (int32_t)na.x;
        lineDsc.p1.y = oy + (int32_t)na.y;
        lineDsc.p2.x = ox + (int32_t)nb.x;
        lineDsc.p2.y = oy + (int32_t)nb.y;
        lv_draw_line(layer, &lineDsc);
    }

    // ── Beam creation preview ────────────────────────────────────────────
    if (app->_appMode == AppMode::EDIT_MODE && app->_startBeamNode >= 0 &&
        app->_startBeamNode < app->_nodeCount) {
        const BridgeNode& sn = app->_nodes[app->_startBeamNode];
        lv_draw_line_dsc_t prevDsc;
        lv_draw_line_dsc_init(&prevDsc);
        prevDsc.color = lv_color_hex(COL_PREVIEW);
        prevDsc.opa   = LV_OPA_60;
        prevDsc.width = 1;
        prevDsc.p1.x  = ox + (int32_t)sn.x;
        prevDsc.p1.y  = oy + (int32_t)sn.y;
        prevDsc.p2.x  = ox + app->_cursorX;
        prevDsc.p2.y  = oy + app->_cursorY;
        lv_draw_line(layer, &prevDsc);
    }

    // ── Nodes ────────────────────────────────────────────────────────────
    for (int i = 0; i < app->_nodeCount; ++i) {
        const BridgeNode& n = app->_nodes[i];
        lv_draw_rect_dsc_t nDsc;
        lv_draw_rect_dsc_init(&nDsc);
        nDsc.bg_opa = LV_OPA_COVER;

        int32_t px = ox + (int32_t)n.x;
        int32_t py = oy + (int32_t)n.y;

        if (n.fixed) {
            // Fixed nodes: red squares 6×6
            nDsc.bg_color = lv_color_hex(COL_NODE_FIXED);
            nDsc.radius   = 0;
            lv_area_t sq = { px - 3, py - 3, px + 3, py + 3 };
            lv_draw_rect(layer, &nDsc, &sq);
        } else {
            // Normal nodes: blue or green circles radius 4
            nDsc.bg_color = lv_color_hex(n.is_deck ? COL_NODE_DECK : COL_NODE);
            nDsc.radius   = LV_RADIUS_CIRCLE;
            lv_area_t circ = { px - 4, py - 4, px + 4, py + 4 };
            lv_draw_rect(layer, &nDsc, &circ);
        }

        // Highlight selected node
        if (i == app->_selectedNode) {
            lv_draw_rect_dsc_t selDsc;
            lv_draw_rect_dsc_init(&selDsc);
            selDsc.bg_opa      = LV_OPA_TRANSP;
            selDsc.border_color = lv_color_hex(COL_CURSOR);
            selDsc.border_width = 1;
            selDsc.border_opa   = LV_OPA_COVER;
            selDsc.radius       = LV_RADIUS_CIRCLE;
            lv_area_t selArea = { px - 6, py - 6, px + 6, py + 6 };
            lv_draw_rect(layer, &selDsc, &selArea);
        }
    }

    // ── Cursor crosshair (edit mode) ─────────────────────────────────────
    if (app->_appMode == AppMode::EDIT_MODE) {
        int32_t cx = ox + app->_cursorX;
        int32_t cy = oy + app->_cursorY;

        lv_draw_line_dsc_t crDsc;
        lv_draw_line_dsc_init(&crDsc);
        crDsc.color = lv_color_hex(COL_CURSOR);
        crDsc.opa   = LV_OPA_COVER;
        crDsc.width = 1;

        // Horizontal crosshair
        crDsc.p1.x = cx - 6;  crDsc.p1.y = cy;
        crDsc.p2.x = cx + 6;  crDsc.p2.y = cy;
        lv_draw_line(layer, &crDsc);

        // Vertical crosshair
        crDsc.p1.x = cx;      crDsc.p1.y = cy - 6;
        crDsc.p2.x = cx;      crDsc.p2.y = cy + 6;
        lv_draw_line(layer, &crDsc);
    }

    // ── Vehicles (sim mode) ──────────────────────────────────────────────
    if (app->_appMode == AppMode::SIM_MODE) {
        for (int v = 0; v < MAX_VEHICLES; ++v) {
            const Vehicle& veh = app->_vehicles[v];
            if (!veh.active) continue;

            // Interpolate Y position from deck nodes
            int deckA = -1, deckB = -1;
            float t = 0.0f;
            app->findClosestDeckNodes(veh.pos, deckA, deckB, t);
            if (deckA < 0) continue;

            float vy;
            if (deckB >= 0 && deckB < app->_nodeCount) {
                vy = app->_nodes[deckA].y * (1.0f - t) + app->_nodes[deckB].y * t;
            } else {
                vy = app->_nodes[deckA].y;
            }

            int32_t vx = ox + (int32_t)veh.pos;
            int32_t vyI = oy + (int32_t)vy;

            lv_draw_rect_dsc_t vDsc;
            lv_draw_rect_dsc_init(&vDsc);
            vDsc.bg_opa = LV_OPA_COVER;
            vDsc.radius = 1;

            if (veh.type == VehicleType::TRUCK) {
                // Truck body: 20×12 orange
                vDsc.bg_color = lv_color_hex(COL_TRUCK);
                lv_area_t body = { vx - 10, vyI - 14, vx + 10, vyI - 2 };
                lv_draw_rect(layer, &vDsc, &body);
                // Wheels
                vDsc.bg_color = lv_color_hex(0x222222);
                vDsc.radius   = LV_RADIUS_CIRCLE;
                lv_area_t w1 = { vx - 7, vyI - 3, vx - 3, vyI + 1 };
                lv_area_t w2 = { vx + 3, vyI - 3, vx + 7, vyI + 1 };
                lv_draw_rect(layer, &vDsc, &w1);
                lv_draw_rect(layer, &vDsc, &w2);
            } else {
                // Car body: 14×8 blue
                vDsc.bg_color = lv_color_hex(COL_CAR);
                lv_area_t body = { vx - 7, vyI - 10, vx + 7, vyI - 2 };
                lv_draw_rect(layer, &vDsc, &body);
                // Wheels
                vDsc.bg_color = lv_color_hex(0x222222);
                vDsc.radius   = LV_RADIUS_CIRCLE;
                lv_area_t w1 = { vx - 5, vyI - 3, vx - 2, vyI };
                lv_area_t w2 = { vx + 2, vyI - 3, vx + 5, vyI };
                lv_draw_rect(layer, &vDsc, &w1);
                lv_draw_rect(layer, &vDsc, &w2);
            }
        }
    }
}

// ══ Physics Timer Callback ═══════════════════════════════════════════════════

void BridgeDesignerApp::onPhysicsTimer(lv_timer_t* timer) {
    BridgeDesignerApp* app = static_cast<BridgeDesignerApp*>(lv_timer_get_user_data(timer));
    if (!app || app->_appMode != AppMode::SIM_MODE) return;
    app->physicsStep();
}

// ══ Physics Engine ═══════════════════════════════════════════════════════════

void BridgeDesignerApp::physicsStep() {
    applyGravity();
    for (int i = 0; i < ITER_COUNT; ++i) {
        satisfyConstraints();
    }
    updateVehicle(_vehicles[0]);
    updateVehicle(_vehicles[1]);
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

void BridgeDesignerApp::applyGravity() {
    for (int i = 0; i < _nodeCount; ++i) {
        BridgeNode& n = _nodes[i];
        if (n.fixed) continue;

        float vx = n.x - n.ox;
        float vy = n.y - n.oy;
        n.ox = n.x;
        n.oy = n.y;
        n.x += vx;
        n.y += vy + GRAVITY * DT * DT * 50.0f;

        // NaN/Inf guard — reset simulation if physics diverges
        if (!std::isfinite(n.x) || !std::isfinite(n.y)) {
            resetSimulation();
            return;
        }
        // Clamp to draw area bounds
        if (n.y > DRAW_H - 2) { n.y = DRAW_H - 2; n.oy = n.y; }
        if (n.x < 2)          { n.x = 2;           n.ox = n.x; }
        if (n.x > SCREEN_W - 2) { n.x = SCREEN_W - 2; n.ox = n.x; }
    }
}

void BridgeDesignerApp::satisfyConstraints() {
    for (int i = 0; i < _beamCount; ++i) {
        BridgeBeam& bm = _beams[i];
        if (bm.broken) continue;

        BridgeNode& na = _nodes[bm.a];
        BridgeNode& nb = _nodes[bm.b];

        float dx = nb.x - na.x;
        float dy = nb.y - na.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < 0.001f) dist = 0.001f;

        // Cable: no compression — skip if shorter than rest length
        if (bm.material == MAT_CABLE && dist < bm.restLen) continue;

        float stress = fabsf(dist - bm.restLen) / bm.restLen;
        bm.stress = std::min(stress, 1.0f);

        float elasticity = MAT_ELASTICITY[bm.material];
        float diff = (dist - bm.restLen) / dist * elasticity;

        float corrX = dx * diff * 0.5f;
        float corrY = dy * diff * 0.5f;

        if (!na.fixed) { na.x += corrX; na.y += corrY; }
        if (!nb.fixed) { nb.x -= corrX; nb.y -= corrY; }

        // Break check
        if (bm.stress > MAT_BREAK_POINT[bm.material]) {
            bm.broken = true;
        }
    }
}

void BridgeDesignerApp::updateVehicle(Vehicle& v) {
    if (!v.active) return;

    v.pos += 30.0f * DT;

    // Apply weight to nearest deck nodes
    int deckA = -1, deckB = -1;
    float t = 0.0f;
    findClosestDeckNodes(v.pos, deckA, deckB, t);

    float force = v.mass * GRAVITY * DT * DT * 50.0f;

    if (deckA >= 0 && deckA < _nodeCount && !_nodes[deckA].fixed) {
        _nodes[deckA].y += force * (1.0f - t);
    }
    if (deckB >= 0 && deckB < _nodeCount && !_nodes[deckB].fixed) {
        _nodes[deckB].y += force * t;
    }

    // Deactivate if past all deck nodes
    float maxX = 0.0f;
    for (int i = 0; i < _nodeCount; ++i) {
        if (_nodes[i].is_deck && _nodes[i].x > maxX) maxX = _nodes[i].x;
    }
    if (v.pos > maxX + 20.0f) {
        v.active = false;
    }
}

// ══ Deck Interpolation ══════════════════════════════════════════════════════

void BridgeDesignerApp::findClosestDeckNodes(float pos, int& a, int& b, float& t) const {
    a = -1;
    b = -1;
    t = 0.0f;

    // Collect deck nodes sorted by X (simple scan for small counts)
    int bestL = -1, bestR = -1;
    float bestLx = -1e9f, bestRx = 1e9f;

    for (int i = 0; i < _nodeCount; ++i) {
        if (!_nodes[i].is_deck) continue;
        float nx = _nodes[i].x;
        if (nx <= pos && nx > bestLx) { bestLx = nx; bestL = i; }
        if (nx > pos && nx < bestRx)  { bestRx = nx; bestR = i; }
    }

    if (bestL >= 0 && bestR >= 0) {
        a = bestL;
        b = bestR;
        float span = bestRx - bestLx;
        t = (span > 0.001f) ? (pos - bestLx) / span : 0.0f;
    } else if (bestL >= 0) {
        a = bestL;
    } else if (bestR >= 0) {
        a = bestR;
    }
}

// ══ Editing Helpers ══════════════════════════════════════════════════════════

void BridgeDesignerApp::addNode(float x, float y, bool fixed, bool isDeck) {
    if (_nodeCount >= MAX_NODES) {
        if (_infoLabel) lv_label_set_text(_infoLabel, "Node limit!");
        return;
    }
    BridgeNode& n = _nodes[_nodeCount];
    n.x  = x;   n.y  = y;
    n.ox = x;   n.oy = y;
    n.fixed   = fixed;
    n.is_deck = isDeck;
    _nodeCount++;
}

void BridgeDesignerApp::addBeam(int a, int b, uint8_t material) {
    if (_beamCount >= MAX_BEAMS) {
        if (_infoLabel) lv_label_set_text(_infoLabel, "Beam limit!");
        return;
    }
    if (a == b || a < 0 || b < 0) return;

    // Avoid duplicate beams
    for (int i = 0; i < _beamCount; ++i) {
        if ((_beams[i].a == a && _beams[i].b == b) ||
            (_beams[i].a == b && _beams[i].b == a)) return;
    }

    float dx = _nodes[b].x - _nodes[a].x;
    float dy = _nodes[b].y - _nodes[a].y;
    float len = sqrtf(dx * dx + dy * dy);

    BridgeBeam& bm = _beams[_beamCount];
    bm.a        = a;
    bm.b        = b;
    bm.restLen  = len;
    bm.material = material;
    bm.stress   = 0.0f;
    bm.broken   = false;
    _beamCount++;
}

void BridgeDesignerApp::deleteNodeOrBeam() {
    if (_selectedNode < 0 || _selectedNode >= _nodeCount) return;

    // Remove all beams connected to this node
    int dst = 0;
    for (int i = 0; i < _beamCount; ++i) {
        if (_beams[i].a == _selectedNode || _beams[i].b == _selectedNode) continue;
        // Reindex: shift indices above deleted node
        BridgeBeam bm = _beams[i];
        if (bm.a > _selectedNode) bm.a--;
        if (bm.b > _selectedNode) bm.b--;
        _beams[dst++] = bm;
    }
    _beamCount = dst;

    // Remove the node by shifting array
    for (int i = _selectedNode; i < _nodeCount - 1; ++i) {
        _nodes[i] = _nodes[i + 1];
    }
    _nodeCount--;
    _selectedNode  = -1;
    _startBeamNode = -1;
}

int BridgeDesignerApp::findNodeAt(int x, int y) const {
    constexpr int SNAP_DIST = 8;
    for (int i = 0; i < _nodeCount; ++i) {
        float dx = _nodes[i].x - (float)x;
        float dy = _nodes[i].y - (float)y;
        if (dx * dx + dy * dy <= SNAP_DIST * SNAP_DIST) return i;
    }
    return -1;
}

// ══ Simulation Lifecycle ════════════════════════════════════════════════════

void BridgeDesignerApp::resetSimulation() {
    // Restore original positions
    for (int i = 0; i < _nodeCount; ++i) {
        _nodes[i].ox = _nodes[i].x;
        _nodes[i].oy = _nodes[i].y;
    }
    // Reset beam stress
    for (int i = 0; i < _beamCount; ++i) {
        _beams[i].stress = 0.0f;
        _beams[i].broken = false;
    }
    // Deactivate vehicles
    for (int i = 0; i < MAX_VEHICLES; ++i) {
        _vehicles[i].active = false;
    }
}

// ══ Color Helper ═════════════════════════════════════════════════════════════

lv_color_t BridgeDesignerApp::lerpColor(float stress) {
    float s = std::min(std::max(stress, 0.0f), 1.0f);
    uint8_t r = (uint8_t)(s * 255.0f);
    uint8_t g = (uint8_t)((1.0f - s) * 255.0f);
    return lv_color_make(r, g, 0);
}

// ══ Input Handling ═══════════════════════════════════════════════════════════

void BridgeDesignerApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_appMode) {
        case AppMode::EDIT_MODE: handleKeyEdit(ev); break;
        case AppMode::SIM_MODE:  handleKeySim(ev);  break;
    }
}

void BridgeDesignerApp::handleKeyEdit(const KeyEvent& ev) {
    auto& km = vpam::KeyboardManager::instance();
    bool shift = km.isShift();

    switch (ev.code) {
        // ── Cursor movement ──────────────────────────────────────────────
        case KeyCode::UP:
            if (shift && _selectedNode >= 0) {
                _nodes[_selectedNode].is_deck = !_nodes[_selectedNode].is_deck;
                snprintf(_infoBuf, sizeof(_infoBuf), "Deck: %s",
                         _nodes[_selectedNode].is_deck ? "ON" : "OFF");
                if (_infoLabel) lv_label_set_text(_infoLabel, _infoBuf);
            } else {
                _cursorY = std::max(_cursorY - GRID_SNAP, 0);
            }
            break;

        case KeyCode::DOWN:
            if (shift && _selectedNode >= 0) {
                _nodes[_selectedNode].fixed = !_nodes[_selectedNode].fixed;
                snprintf(_infoBuf, sizeof(_infoBuf), "Fixed: %s",
                         _nodes[_selectedNode].fixed ? "ON" : "OFF");
                if (_infoLabel) lv_label_set_text(_infoLabel, _infoBuf);
            } else {
                _cursorY = std::min(_cursorY + GRID_SNAP, DRAW_H - GRID_SNAP);
            }
            break;

        case KeyCode::LEFT:
            _cursorX = std::max(_cursorX - GRID_SNAP, 0);
            break;

        case KeyCode::RIGHT:
            _cursorX = std::min(_cursorX + GRID_SNAP, SCREEN_W - GRID_SNAP);
            break;

        // ── Place / connect / select ─────────────────────────────────────
        case KeyCode::ENTER: {
            int nodeHere = findNodeAt(_cursorX, _cursorY);
            if (nodeHere < 0) {
                // No node at cursor — place a new one
                addNode((float)_cursorX, (float)_cursorY, false, false);
                int newIdx = _nodeCount - 1;

                // If we were in beam-creation mode, finish the beam
                if (_startBeamNode >= 0) {
                    uint8_t mat = static_cast<uint8_t>(_currentTool);
                    if (mat > MAT_CABLE) mat = MAT_WOOD;
                    addBeam(_startBeamNode, newIdx, mat);
                    _startBeamNode = -1;
                    if (_infoLabel) lv_label_set_text(_infoLabel, "Beam placed");
                } else {
                    if (_infoLabel) lv_label_set_text(_infoLabel, "Node placed");
                }
                _selectedNode = newIdx;
            } else {
                // Node exists at cursor
                if (_startBeamNode < 0) {
                    // Start beam creation from this node
                    _startBeamNode = nodeHere;
                    _selectedNode  = nodeHere;
                    if (_infoLabel) lv_label_set_text(_infoLabel, "Beam start — select end node");
                } else if (_startBeamNode != nodeHere) {
                    // Finish beam
                    uint8_t mat = static_cast<uint8_t>(_currentTool);
                    if (mat > MAT_CABLE) mat = MAT_WOOD;
                    addBeam(_startBeamNode, nodeHere, mat);
                    _startBeamNode = -1;
                    if (_infoLabel) lv_label_set_text(_infoLabel, "Beam placed");
                } else {
                    // Same node — cancel beam creation, just select
                    _startBeamNode = -1;
                    _selectedNode  = nodeHere;
                    if (_infoLabel) lv_label_set_text(_infoLabel, "Selected");
                }
            }
            break;
        }

        // ── Delete ───────────────────────────────────────────────────────
        case KeyCode::DEL:
            if (_startBeamNode >= 0) {
                _startBeamNode = -1;
                if (_infoLabel) lv_label_set_text(_infoLabel, "Cancelled");
            } else if (_selectedNode >= 0) {
                deleteNodeOrBeam();
                if (_infoLabel) lv_label_set_text(_infoLabel, "Deleted");
            }
            break;

        // ── Tool selection (F1-F4) ───────────────────────────────────────
        case KeyCode::F1:
            _currentTool = ToolType::TOOL_WOOD;
            updateToolbarHighlight(_toolbarObj, 0);
            if (_infoLabel) lv_label_set_text(_infoLabel, "Tool: WOOD");
            break;
        case KeyCode::F2:
            _currentTool = ToolType::TOOL_STEEL;
            updateToolbarHighlight(_toolbarObj, 1);
            if (_infoLabel) lv_label_set_text(_infoLabel, "Tool: STEEL");
            break;
        case KeyCode::F3:
            _currentTool = ToolType::TOOL_CABLE;
            updateToolbarHighlight(_toolbarObj, 2);
            if (_infoLabel) lv_label_set_text(_infoLabel, "Tool: CABLE");
            break;
        case KeyCode::F4:
            _currentTool = ToolType::TOOL_DELETE;
            updateToolbarHighlight(_toolbarObj, 3);
            if (_infoLabel) lv_label_set_text(_infoLabel, "Tool: DELETE");
            break;

        // ── Start simulation (SOLVE / EXE key) ──────────────────────────
        case KeyCode::SOLVE: {
            _appMode = AppMode::SIM_MODE;
            _selectedNode  = -1;
            _startBeamNode = -1;

            // Launch truck
            _vehicles[0].pos    = 0.0f;
            _vehicles[0].mass   = 5.0f;
            _vehicles[0].active = true;
            _vehicles[0].type   = VehicleType::TRUCK;

            // Find leftmost deck node as starting X
            float minX = (float)SCREEN_W;
            for (int i = 0; i < _nodeCount; ++i) {
                if (_nodes[i].is_deck && _nodes[i].x < minX) minX = _nodes[i].x;
            }
            _vehicles[0].pos = minX;

            // Start physics timer (~60 Hz)
            if (!_physicsTimer) {
                _physicsTimer = lv_timer_create(onPhysicsTimer, 16, this);
            }
            if (_infoLabel) lv_label_set_text(_infoLabel, "SIM  F1:truck  F2:car  AC:stop");
            break;
        }

        // ── Cancel beam creation ─────────────────────────────────────────
        case KeyCode::AC:
            _startBeamNode = -1;
            _selectedNode  = -1;
            if (_infoLabel) lv_label_set_text(_infoLabel, "Arrows:move  ENTER:place  EXE:sim");
            break;

        default:
            break;
    }

    // Redraw after any edit action
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

void BridgeDesignerApp::handleKeySim(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::AC:
            // Stop simulation, return to edit mode
            if (_physicsTimer) {
                lv_timer_delete(_physicsTimer);
                _physicsTimer = nullptr;
            }
            _appMode = AppMode::EDIT_MODE;
            resetSimulation();
            if (_infoLabel) lv_label_set_text(_infoLabel, "Sim stopped — edit mode");
            if (_drawObj) lv_obj_invalidate(_drawObj);
            break;

        case KeyCode::F1: {
            // Launch truck
            float minX = (float)SCREEN_W;
            for (int i = 0; i < _nodeCount; ++i) {
                if (_nodes[i].is_deck && _nodes[i].x < minX) minX = _nodes[i].x;
            }
            _vehicles[0].pos    = minX;
            _vehicles[0].mass   = 5.0f;
            _vehicles[0].active = true;
            _vehicles[0].type   = VehicleType::TRUCK;
            if (_infoLabel) lv_label_set_text(_infoLabel, "Truck launched!");
            break;
        }

        case KeyCode::F2: {
            // Launch car (lighter)
            float minX = (float)SCREEN_W;
            for (int i = 0; i < _nodeCount; ++i) {
                if (_nodes[i].is_deck && _nodes[i].x < minX) minX = _nodes[i].x;
            }
            _vehicles[1].pos    = minX;
            _vehicles[1].mass   = 2.0f;
            _vehicles[1].active = true;
            _vehicles[1].type   = VehicleType::CAR;
            if (_infoLabel) lv_label_set_text(_infoLabel, "Car launched!");
            break;
        }

        default:
            break;
    }
}
