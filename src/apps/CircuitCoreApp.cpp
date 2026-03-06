/**
 * CircuitCoreApp.cpp — Real-time SPICE-like Circuit Simulator for NumOS.
 *
 * LVGL-native app: grid-based circuit editor with MNA solver.
 * Custom draw via LV_EVENT_DRAW_MAIN. Component arrays in PSRAM.
 */

#include "CircuitCoreApp.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
  #include <esp_heap_caps.h>
  #include <LittleFS.h>
#endif

// ── Color palette ───────────────────────────────────────────────────────────
static constexpr uint32_t COL_BG          = 0x0D1117;
static constexpr uint32_t COL_GRID_LINE   = 0x1A2332;
static constexpr uint32_t COL_GRID_DOT    = 0x2A3342;
static constexpr uint32_t COL_CURSOR      = 0xFFD700;
static constexpr uint32_t COL_TOOLBAR_BG  = 0x161B22;
static constexpr uint32_t COL_TOOL_ACTIVE = 0x1F6FEB;
static constexpr uint32_t COL_TOOL_TEXT   = 0xE0E0E0;
static constexpr uint32_t COL_TOOL_DIM    = 0x606060;
static constexpr uint32_t COL_INFO_BG     = 0x161B22;
static constexpr uint32_t COL_INFO_TEXT   = 0xA0A0A0;
static constexpr uint32_t COL_RUN_ON      = 0x3FB950;
static constexpr uint32_t COL_RUN_OFF     = 0xFF4444;

// ── Toolbar labels ──────────────────────────────────────────────────────────
static const char* TOOL_LABELS[] = {
    "RES", "VCC", "GND", "LED", "MCU", "WIRE", "POT", "BTN", "CAP", "DIO", "RUN"
};
static constexpr int NUM_TOOLS = 11;

// ── Toolbar tooltip descriptions ────────────────────────────────────────────
const char* CircuitCoreApp::TOOL_TOOLTIPS[] = {
    "RESISTOR [R] - Adjust value with ENTER",
    "VOLTAGE SOURCE [V] - 5V DC supply",
    "GROUND [G] - 0V reference node",
    "LED [L] - Light-emitting diode",
    "MCU [M] - Microcontroller (Lua)",
    "WIRE [W] - Connect two nodes",
    "POTENTIOMETER [P] - Adjustable resistor (UP/DOWN)",
    "PUSH-BUTTON [B] - Toggle with F4",
    "CAPACITOR [C] - RC transient analysis",
    "DIODE [D] - Rectifier diode (0.7V)",
    "RUN/STOP - Toggle simulation"
};

// ══ Constructor / Destructor ═════════════════════════════════════════════════

CircuitCoreApp::CircuitCoreApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _toolbarObj(nullptr)
    , _infoLabel(nullptr)
    , _simTimer(nullptr)
    , _components(nullptr)
    , _compCount(0)
    , _currentTool(Tool::RES)
    , _toolbarIdx(0)
    , _focusArea(FocusArea::GRID)
    , _cursorX(GRID_SNAP * 5)   // start near center
    , _cursorY(GRID_SNAP * 4)
    , _simRunning(false)
    , _frameCount(0)
    , _nextNodeId(1)
    , _nextVsIdx(0)
    , _scopeWriteIdx(0)
    , _probeNode(-1)
    , _scopeActive(false)
{
    _infoBuf[0] = '\0';
    for (int i = 0; i < static_cast<int>(Tool::TOOL_COUNT); ++i)
        _toolBtns[i] = nullptr;
    memset(_scopeBuffer, 0, sizeof(_scopeBuffer));
}

CircuitCoreApp::~CircuitCoreApp() {
    end();
}

// ══ Lifecycle ════════════════════════════════════════════════════════════════

void CircuitCoreApp::begin() {
    if (_screen) return;

    // ── Allocate component array in PSRAM ──
#ifdef ARDUINO
    _components = (CircuitComponent**)heap_caps_malloc(
        MAX_COMPONENTS * sizeof(CircuitComponent*), MALLOC_CAP_SPIRAM);
#else
    _components = new CircuitComponent*[MAX_COMPONENTS];
#endif
    if (!_components) return;
    memset(_components, 0, MAX_COMPONENTS * sizeof(CircuitComponent*));
    _compCount = 0;

    // ── Allocate MNA matrices ──
    if (!_mna.allocate()) return;

    // ── Initialize Lua VM (MCU component set later when one is placed) ──
    _luaVM.init(&_mna, nullptr);

    // ── Create LVGL screen ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── Status bar ──
    _statusBar.create(_screen);
    _statusBar.setTitle("Circuit Core");

    // ── Build UI ──
    createUI();

    // ── Reset state ──
    _currentTool = Tool::RES;
    _toolbarIdx = 0;
    _focusArea = FocusArea::GRID;
    _cursorX = GRID_SNAP * 5;
    _cursorY = GRID_SNAP * 4;
    _simRunning = false;
    _frameCount = 0;
    _nextNodeId = 1;
    _nextVsIdx = 0;
    _scopeWriteIdx = 0;
    _probeNode = -1;
    _scopeActive = false;
    memset(_scopeBuffer, 0, sizeof(_scopeBuffer));

    // ── Try to auto-load last circuit ──
    autoLoad();

    updateInfoBar();
    updateToolbarHighlight();
}

void CircuitCoreApp::end() {
    // ── Auto-save circuit before cleanup ──
    autoSave();

    // ── Stop simulation timer ──
    if (_simTimer) {
        lv_timer_delete(_simTimer);
        _simTimer = nullptr;
    }

    // ── Shut down Lua VM ──
    _luaVM.shutdown();

    // ── Free MNA matrices ──
    _mna.deallocate();

    // ── Free components ──
    if (_components) {
        for (int i = 0; i < _compCount; ++i) {
            delete _components[i];
            _components[i] = nullptr;
        }
#ifdef ARDUINO
        heap_caps_free(_components);
#else
        delete[] _components;
#endif
        _components = nullptr;
    }
    _compCount = 0;

    // ── Destroy LVGL objects ──
    _statusBar.resetPointers();
    _drawObj = nullptr;
    _toolbarObj = nullptr;
    _infoLabel = nullptr;
    for (int i = 0; i < static_cast<int>(Tool::TOOL_COUNT); ++i)
        _toolBtns[i] = nullptr;

    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
    }
}

void CircuitCoreApp::load() {
    if (!_screen) begin();
    if (_screen) {
        lv_screen_load_anim(_screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
}

// ══ UI Construction ═════════════════════════════════════════════════════════

void CircuitCoreApp::createUI() {
    // ── Grid draw area ──
    _drawObj = lv_obj_create(_screen);
    lv_obj_remove_style_all(_drawObj);
    lv_obj_set_pos(_drawObj, 0, GRID_Y);
    lv_obj_set_size(_drawObj, GRID_W, GRID_H);
    lv_obj_set_style_bg_color(_drawObj, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_drawObj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_drawObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_drawObj, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Store 'this' for the draw callback
    lv_obj_set_user_data(_drawObj, this);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN, this);

    // ── Toolbar ──
    createToolbar();

    // ── Info bar ──
    createInfoBar();
}

void CircuitCoreApp::createToolbar() {
    _toolbarObj = lv_obj_create(_screen);
    lv_obj_remove_style_all(_toolbarObj);
    lv_obj_set_pos(_toolbarObj, 0, TOOLBAR_Y);
    lv_obj_set_size(_toolbarObj, SCREEN_W, TOOLBAR_H);
    lv_obj_set_style_bg_color(_toolbarObj, lv_color_hex(COL_TOOLBAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_toolbarObj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_toolbarObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_toolbarObj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Create tool buttons (compact to fit all 11 tools)
    int btnW = SCREEN_W / NUM_TOOLS;
    for (int i = 0; i < NUM_TOOLS; ++i) {
        lv_obj_t* btn = lv_obj_create(_toolbarObj);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, i * btnW, 0);
        lv_obj_set_size(btn, btnW, TOOLBAR_H);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_TOOLBAR_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, TOOL_LABELS[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TOOL_DIM), LV_PART_MAIN);
        lv_obj_center(lbl);

        _toolBtns[i] = btn;
    }
}

void CircuitCoreApp::createInfoBar() {
    _infoLabel = lv_label_create(_screen);
    lv_obj_set_pos(_infoLabel, 4, SCREEN_H - INFOBAR_H + 2);
    lv_obj_set_size(_infoLabel, SCREEN_W - 8, INFOBAR_H);
    lv_obj_set_style_bg_color(_infoLabel, lv_color_hex(COL_INFO_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_infoLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_INFO_TEXT), LV_PART_MAIN);
    lv_label_set_text(_infoLabel, "Ready");
}

void CircuitCoreApp::updateInfoBar() {
    if (!_infoLabel) return;

    const char* toolName = TOOL_LABELS[static_cast<int>(_currentTool)];
    int gx = _cursorX / GRID_SNAP;
    int gy = _cursorY / GRID_SNAP;

    if (_focusArea == FocusArea::TOOLBAR) {
        // Show tooltip for focused toolbar button
        snprintf(_infoBuf, sizeof(_infoBuf), "%s", TOOL_TOOLTIPS[_toolbarIdx]);
    } else if (_simRunning) {
        snprintf(_infoBuf, sizeof(_infoBuf), "SIM [%s] Cursor(%d,%d) Comps:%d",
                 toolName, gx, gy, _compCount);
    } else {
        snprintf(_infoBuf, sizeof(_infoBuf), "EDIT [%s] Cursor(%d,%d) Comps:%d",
                 toolName, gx, gy, _compCount);
    }
    lv_label_set_text(_infoLabel, _infoBuf);
}

void CircuitCoreApp::updateToolbarHighlight() {
    for (int i = 0; i < NUM_TOOLS; ++i) {
        if (!_toolBtns[i]) continue;

        bool isActive = (i == static_cast<int>(_currentTool));
        bool isFocused = (_focusArea == FocusArea::TOOLBAR && i == _toolbarIdx);

        uint32_t bgCol = COL_TOOLBAR_BG;
        if (isFocused) bgCol = COL_TOOL_ACTIVE;
        else if (isActive) bgCol = 0x1A3050;

        lv_obj_set_style_bg_color(_toolBtns[i], lv_color_hex(bgCol), LV_PART_MAIN);

        // Update label color
        lv_obj_t* lbl = lv_obj_get_child(_toolBtns[i], 0);
        if (lbl) {
            uint32_t txtCol = COL_TOOL_DIM;
            if (isActive || isFocused) txtCol = COL_TOOL_TEXT;

            // Special color for RUN button
            if (i == static_cast<int>(Tool::RUN)) {
                txtCol = _simRunning ? COL_RUN_ON : COL_RUN_OFF;
            }

            lv_obj_set_style_text_color(lbl, lv_color_hex(txtCol), LV_PART_MAIN);
        }
    }
}

// ══ Input Handling ══════════════════════════════════════════════════════════

void CircuitCoreApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_focusArea) {
        case FocusArea::GRID:
            handleKeyGrid(ev);
            break;
        case FocusArea::TOOLBAR:
            handleKeyToolbar(ev);
            break;
    }
}

void CircuitCoreApp::handleKeyGrid(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
            if (_simRunning) {
                // In SIM_MODE, UP adjusts potentiometer under cursor
                adjustPotAt(_cursorX, _cursorY, true);
            } else {
                if (_cursorY >= GRID_SNAP)
                    _cursorY -= GRID_SNAP;
            }
            break;
        case KeyCode::DOWN:
            if (_simRunning) {
                // In SIM_MODE, DOWN adjusts potentiometer under cursor
                adjustPotAt(_cursorX, _cursorY, false);
            } else {
                if (_cursorY < GRID_H - GRID_SNAP)
                    _cursorY += GRID_SNAP;
            }
            break;
        case KeyCode::LEFT:
            if (_cursorX >= GRID_SNAP)
                _cursorX -= GRID_SNAP;
            break;
        case KeyCode::RIGHT:
            if (_cursorX < GRID_W - GRID_SNAP)
                _cursorX += GRID_SNAP;
            break;
        case KeyCode::ENTER:
            if (_simRunning) {
                // In SIM_MODE, ENTER probes a node for oscilloscope
                CircuitComponent* comp = findComponentAt(_cursorX, _cursorY);
                if (comp) {
                    _probeNode = comp->nodeA();
                    _scopeActive = true;
                    _scopeWriteIdx = 0;
                    memset(_scopeBuffer, 0, sizeof(_scopeBuffer));
                }
            } else {
                placeComponent();
            }
            break;
        case KeyCode::DEL:
            deleteComponentAt(_cursorX, _cursorY);
            break;
        case KeyCode::F4:
            // Toggle push-button under cursor
            toggleButtonAt(_cursorX, _cursorY);
            break;
        case KeyCode::AC:
            // Escape to toolbar
            _focusArea = FocusArea::TOOLBAR;
            _toolbarIdx = static_cast<int>(_currentTool);
            updateToolbarHighlight();
            break;
        default:
            break;
    }

    updateInfoBar();
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

void CircuitCoreApp::handleKeyToolbar(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::LEFT:
            if (_toolbarIdx > 0) _toolbarIdx--;
            updateToolbarHighlight();
            break;
        case KeyCode::RIGHT:
            if (_toolbarIdx < NUM_TOOLS - 1) _toolbarIdx++;
            updateToolbarHighlight();
            break;
        case KeyCode::ENTER: {
            Tool selected = static_cast<Tool>(_toolbarIdx);
            if (selected == Tool::RUN) {
                // Toggle simulation
                if (_simRunning) stopSimulation();
                else startSimulation();
            } else {
                _currentTool = selected;
                _focusArea = FocusArea::GRID;
            }
            updateToolbarHighlight();
            updateInfoBar();
            break;
        }
        case KeyCode::DOWN:
            // Go back to grid
            _focusArea = FocusArea::GRID;
            updateToolbarHighlight();
            break;
        case KeyCode::AC:
            // AC on toolbar: SystemApp handles MODE for return to menu.
            // This case intentionally does nothing — SystemApp intercepts it.
            break;
        default:
            break;
    }
}

// ══ Component Management ════════════════════════════════════════════════════

void CircuitCoreApp::placeComponent() {
    if (_compCount >= MAX_COMPONENTS) return;
    if (_simRunning) return;  // Can't place during simulation

    // Check if something already exists at this position
    if (findComponentAt(_cursorX, _cursorY)) return;

    CircuitComponent* comp = nullptr;

    switch (_currentTool) {
        case Tool::RES:
            comp = new Resistor(_cursorX, _cursorY, 1000.0f);
            comp->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            break;
        case Tool::VCC: {
            auto* vs = new VoltageSource(_cursorX, _cursorY, 5.0f);
            vs->setVsIndex(_nextVsIdx++);
            vs->setNodes(_nextNodeId, 0);  // positive to new node, negative to GND
            _nextNodeId++;
            comp = vs;
            break;
        }
        case Tool::GND:
            comp = new GroundNode(_cursorX, _cursorY);
            // Ground connects nearby nodes to node 0
            break;
        case Tool::LED: {
            auto* led = new LEDComponent(_cursorX, _cursorY, 2.0f);
            led->setVsIndex(_nextVsIdx++);
            led->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            comp = led;
            break;
        }
        case Tool::MCU: {
            auto* mcu = new MCUComponent(_cursorX, _cursorY);
            for (int i = 0; i < MCUComponent::PIN_COUNT; ++i) {
                mcu->setPinNode(i, _nextNodeId++);
            }
            comp = mcu;
            break;
        }
        case Tool::WIRE:
            comp = new WireComponent(_cursorX, _cursorY);
            comp->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            break;
        case Tool::POT:
            comp = new Potentiometer(_cursorX, _cursorY, 10000.0f);
            comp->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            break;
        case Tool::BTN:
            comp = new PushButton(_cursorX, _cursorY);
            comp->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            break;
        case Tool::CAP:
            comp = new Capacitor(_cursorX, _cursorY, 100e-6f);
            comp->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            break;
        case Tool::DIODE:
            comp = new Diode(_cursorX, _cursorY, 0.7f);
            comp->setNodes(_nextNodeId, _nextNodeId + 1);
            _nextNodeId += 2;
            break;
        case Tool::RUN:
            // Not a placeable component
            return;
        default:
            return;
    }

    if (comp) {
        _components[_compCount++] = comp;
        updateInfoBar();
        if (_drawObj) lv_obj_invalidate(_drawObj);
    }
}

void CircuitCoreApp::deleteComponentAt(int gx, int gy) {
    if (_simRunning) return;

    for (int i = 0; i < _compCount; ++i) {
        if (_components[i] &&
            _components[i]->gridX() == gx &&
            _components[i]->gridY() == gy)
        {
            delete _components[i];
            // Shift remaining
            for (int j = i; j < _compCount - 1; ++j)
                _components[j] = _components[j + 1];
            _components[--_compCount] = nullptr;
            updateInfoBar();
            if (_drawObj) lv_obj_invalidate(_drawObj);
            return;
        }
    }
}

CircuitComponent* CircuitCoreApp::findComponentAt(int gx, int gy) const {
    for (int i = 0; i < _compCount; ++i) {
        if (_components[i] &&
            _components[i]->gridX() == gx &&
            _components[i]->gridY() == gy)
            return _components[i];
    }
    return nullptr;
}

// ══ Simulation ══════════════════════════════════════════════════════════════

void CircuitCoreApp::startSimulation() {
    if (_simRunning) return;

    _simRunning = true;
    _frameCount = 0;

    // Create 60Hz timer (MNA runs at 30Hz via frameCount % 2)
    _simTimer = lv_timer_create(onSimTimer, 16, this);  // ~60 Hz

    updateInfoBar();
    updateToolbarHighlight();
}

void CircuitCoreApp::stopSimulation() {
    _simRunning = false;

    if (_simTimer) {
        lv_timer_delete(_simTimer);
        _simTimer = nullptr;
    }

    _luaVM.reset();
    updateInfoBar();
    updateToolbarHighlight();
}

void CircuitCoreApp::runMnaTick() {
    // Reset Union-Find
    _mna.ufReset();

    // Count voltage sources needed
    int vsCount = 0;
    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        CompType t = _components[i]->type();
        if (t == CompType::VOLTAGE_SOURCE) vsCount++;
        if (t == CompType::LED) vsCount++;  // LED uses a VS when ON
    }
    _mna.setVsCount(vsCount);

    // Phase 1: Stamp wires (Union-Find merge)
    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        if (_components[i]->type() == CompType::WIRE ||
            _components[i]->type() == CompType::GROUND) {
            _components[i]->stampMatrix(_mna);
        }
    }

    // Phase 2: Clear and re-stamp matrices
    _mna.stampClear();

    // Phase 3: Stamp all non-wire components (includes new types)
    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        CompType t = _components[i]->type();
        if (t != CompType::WIRE && t != CompType::GROUND) {
            _components[i]->stampMatrix(_mna);
        }
    }

    // Phase 4: Solve
    bool ok = _mna.solve();

    // Phase 5: Update component state from solution
    if (ok) {
        for (int i = 0; i < _compCount; ++i) {
            if (_components[i])
                _components[i]->updateFromSolution(_mna);
        }
    }

    // Phase 6: Update oscilloscope
    updateScope();

    // Phase 7: Run Lua VM tick
    _luaVM.tick();

    // Invalidate draw area
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

// ══ Timer Callback ══════════════════════════════════════════════════════════

void CircuitCoreApp::onSimTimer(lv_timer_t* timer) {
    auto* app = static_cast<CircuitCoreApp*>(lv_timer_get_user_data(timer));
    if (!app || !app->_simRunning) return;

    app->_frameCount++;

    // MNA runs at 30Hz (every other frame at 60Hz)
    if (app->_frameCount % 2 == 0) {
        app->runMnaTick();
    }
}

// ══ Custom Draw Callback ════════════════════════════════════════════════════

void CircuitCoreApp::onDraw(lv_event_t* e) {
    auto* app = static_cast<CircuitCoreApp*>(lv_event_get_user_data(e));
    if (!app) return;

    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t* layer = lv_event_get_layer(e);

    int objX = lv_obj_get_x(obj);
    int objY = lv_obj_get_y(obj);

    // ── Draw grid dots ──
    lv_draw_rect_dsc_t dotDsc;
    lv_draw_rect_dsc_init(&dotDsc);
    dotDsc.bg_color = lv_color_hex(COL_GRID_DOT);
    dotDsc.bg_opa   = LV_OPA_COVER;
    dotDsc.radius   = 1;

    for (int gx = 0; gx < GRID_W; gx += GRID_SNAP) {
        for (int gy = 0; gy < GRID_H; gy += GRID_SNAP) {
            lv_area_t dot;
            dot.x1 = objX + gx - 1;
            dot.y1 = objY + gy - 1;
            dot.x2 = objX + gx + 1;
            dot.y2 = objY + gy + 1;
            lv_draw_rect(layer, &dotDsc, &dot);
        }
    }

    // ── Draw components ──
    for (int i = 0; i < app->_compCount; ++i) {
        if (app->_components[i]) {
            app->_components[i]->draw(layer, objX, objY);
        }
    }

    // ── Draw current animation dots (SIM_MODE) ──
    if (app->_simRunning) {
        app->drawCurrentDots(layer, objX, objY);
    }

    // ── Draw node voltage labels (SIM_MODE) ──
    if (app->_simRunning) {
        app->drawNodeLabels(layer, objX, objY);
    }

    // ── Draw cursor crosshair ──
    if (app->_focusArea == FocusArea::GRID) {
        lv_draw_line_dsc_t curDsc;
        lv_draw_line_dsc_init(&curDsc);
        curDsc.color = lv_color_hex(COL_CURSOR);
        curDsc.width = 1;

        int cx = objX + app->_cursorX;
        int cy = objY + app->_cursorY;

        // Horizontal crosshair
        curDsc.p1.x = cx - 8; curDsc.p1.y = cy;
        curDsc.p2.x = cx + 8; curDsc.p2.y = cy;
        lv_draw_line(layer, &curDsc);

        // Vertical crosshair
        curDsc.p1.x = cx; curDsc.p1.y = cy - 8;
        curDsc.p2.x = cx; curDsc.p2.y = cy + 8;
        lv_draw_line(layer, &curDsc);
    }

    // ── Draw oscilloscope (SIM_MODE) ──
    if (app->_scopeActive && app->_simRunning) {
        app->drawScope(layer, objX, objY);
    }
}

// ══ Oscilloscope ════════════════════════════════════════════════════════════

void CircuitCoreApp::updateScope() {
    if (!_scopeActive || _probeNode < 0) return;

    float v = _mna.nodeVoltage(_probeNode);
    _scopeBuffer[_scopeWriteIdx] = v;
    _scopeWriteIdx = (_scopeWriteIdx + 1) % SCOPE_SAMPLES;
}

void CircuitCoreApp::drawScope(lv_layer_t* layer, int objX, int objY) {
    // Position: bottom-right corner of grid area
    int scopeX = objX + GRID_W - SCOPE_W - 4;
    int scopeY = objY + GRID_H - SCOPE_H - 4;

    // Background
    lv_draw_rect_dsc_t bgDsc;
    lv_draw_rect_dsc_init(&bgDsc);
    bgDsc.bg_color = lv_color_hex(0x0A0A0A);
    bgDsc.bg_opa   = LV_OPA_80;
    bgDsc.border_color = lv_color_hex(0x3FB950);
    bgDsc.border_width = 1;
    bgDsc.radius = 2;

    lv_area_t bgArea;
    bgArea.x1 = scopeX;
    bgArea.y1 = scopeY;
    bgArea.x2 = scopeX + SCOPE_W;
    bgArea.y2 = scopeY + SCOPE_H;
    lv_draw_rect(layer, &bgDsc, &bgArea);

    // Find voltage range for auto-scaling
    static constexpr float SCOPE_MIN_RANGE = 0.1f;  // minimum displayable voltage range
    float vMin = 1e9f, vMax = -1e9f;
    for (int i = 0; i < SCOPE_SAMPLES; ++i) {
        if (_scopeBuffer[i] < vMin) vMin = _scopeBuffer[i];
        if (_scopeBuffer[i] > vMax) vMax = _scopeBuffer[i];
    }
    float vRange = vMax - vMin;
    if (vRange < SCOPE_MIN_RANGE) { vRange = SCOPE_MIN_RANGE; vMin = vMax - SCOPE_MIN_RANGE / 2.0f; }

    // Draw waveform
    lv_draw_line_dsc_t lineDsc;
    lv_draw_line_dsc_init(&lineDsc);
    lineDsc.color = lv_color_hex(0x3FB950);  // green waveform
    lineDsc.width = 1;

    for (int i = 0; i < SCOPE_SAMPLES - 1; ++i) {
        int idx0 = (_scopeWriteIdx + i) % SCOPE_SAMPLES;
        int idx1 = (_scopeWriteIdx + i + 1) % SCOPE_SAMPLES;

        float norm0 = (_scopeBuffer[idx0] - vMin) / vRange;
        float norm1 = (_scopeBuffer[idx1] - vMin) / vRange;

        int y0 = scopeY + SCOPE_H - 2 - (int)(norm0 * (SCOPE_H - 4));
        int y1 = scopeY + SCOPE_H - 2 - (int)(norm1 * (SCOPE_H - 4));

        lineDsc.p1.x = scopeX + i;
        lineDsc.p1.y = y0;
        lineDsc.p2.x = scopeX + i + 1;
        lineDsc.p2.y = y1;
        lv_draw_line(layer, &lineDsc);
    }

    // Probe label
    char probeBuf[24];
    snprintf(probeBuf, sizeof(probeBuf), "N%d", _probeNode);
    lv_draw_label_dsc_t labDsc;
    lv_draw_label_dsc_init(&labDsc);
    labDsc.color = lv_color_hex(0x3FB950);
    labDsc.font  = &lv_font_unscii_8;
    labDsc.text  = probeBuf;

    lv_area_t labArea;
    labArea.x1 = scopeX + 2;
    labArea.y1 = scopeY + 1;
    labArea.x2 = scopeX + 40;
    labArea.y2 = scopeY + 10;
    lv_draw_label(layer, &labDsc, &labArea);
}

// ══ Node Voltage Labels (SIM_MODE) ══════════════════════════════════════════

void CircuitCoreApp::drawNodeLabels(lv_layer_t* layer, int objX, int objY) {
    // Show floating voltage labels near the cursor
    int cursorPxX = objX + _cursorX;
    int cursorPxY = objY + _cursorY;

    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;

        int compPxX = objX + _components[i]->gridX();
        int compPxY = objY + _components[i]->gridY();

        // Only show for components within 2 grid cells of cursor
        int dx = abs(compPxX - cursorPxX);
        int dy = abs(compPxY - cursorPxY);
        if (dx > GRID_SNAP * 2 || dy > GRID_SNAP * 2) continue;

        int nodeA = _components[i]->nodeA();
        if (nodeA <= 0) continue;

        float voltage = _mna.nodeVoltage(nodeA);

        char voltageBuf[20];
        snprintf(voltageBuf, sizeof(voltageBuf), "V%d:%.2fV", nodeA, (double)voltage);

        lv_draw_label_dsc_t labDsc;
        lv_draw_label_dsc_init(&labDsc);
        labDsc.color = lv_color_hex(0xFFD700);  // gold
        labDsc.font  = &lv_font_unscii_8;
        labDsc.text  = voltageBuf;

        lv_area_t labArea;
        labArea.x1 = compPxX + 12;
        labArea.y1 = compPxY - 14;
        labArea.x2 = compPxX + 70;
        labArea.y2 = compPxY - 4;
        lv_draw_label(layer, &labDsc, &labArea);
    }
}

// ══ Current Animation Dots ══════════════════════════════════════════════════

void CircuitCoreApp::drawCurrentDots(lv_layer_t* layer, int objX, int objY) {
    // Animated dots along components with current > 1mA
    uint32_t animPhase = _frameCount % 8;  // 8-frame animation cycle

    lv_draw_rect_dsc_t dotDsc;
    lv_draw_rect_dsc_init(&dotDsc);
    dotDsc.bg_color = lv_color_hex(0xFFFF00);  // bright yellow dots
    dotDsc.bg_opa   = LV_OPA_COVER;
    dotDsc.radius   = 2;

    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        CompType t = _components[i]->type();

        // Determine current through component
        float current = 0.0f;
        if (t == CompType::LED) {
            current = static_cast<LEDComponent*>(_components[i])->current();
        } else if (t == CompType::DIODE) {
            current = static_cast<Diode*>(_components[i])->current();
        } else if (t == CompType::RESISTOR || t == CompType::POTENTIOMETER) {
            // Estimate current from node voltages
            float vA = _mna.nodeVoltage(_components[i]->nodeA());
            float vB = _mna.nodeVoltage(_components[i]->nodeB());
            float r = (t == CompType::RESISTOR)
                ? static_cast<Resistor*>(_components[i])->resistance()
                : static_cast<Potentiometer*>(_components[i])->resistance();
            if (r > 0.0f) current = fabsf(vA - vB) / r;
        }

        // Only show dots if current > 1mA
        if (current < 0.001f) continue;

        int cx = objX + _components[i]->gridX();
        int cy = objY + _components[i]->gridY();

        // Draw 2-3 moving dots along the component's horizontal extent
        for (int d = 0; d < 3; ++d) {
            int dotX = cx - 16 + (int)((animPhase + d * 3) % 8) * 4;
            lv_area_t dot;
            dot.x1 = dotX - 1;
            dot.y1 = cy - 1;
            dot.x2 = dotX + 1;
            dot.y2 = cy + 1;
            lv_draw_rect(layer, &dotDsc, &dot);
        }
    }
}

// ══ Component Interaction ═══════════════════════════════════════════════════

void CircuitCoreApp::toggleButtonAt(int gx, int gy) {
    CircuitComponent* comp = findComponentAt(gx, gy);
    if (comp && comp->type() == CompType::PUSH_BUTTON) {
        static_cast<PushButton*>(comp)->toggle();
        if (_drawObj) lv_obj_invalidate(_drawObj);
    }
}

void CircuitCoreApp::adjustPotAt(int gx, int gy, bool up) {
    CircuitComponent* comp = findComponentAt(gx, gy);
    if (comp && comp->type() == CompType::POTENTIOMETER) {
        auto* pot = static_cast<Potentiometer*>(comp);
        if (up) pot->adjustUp();
        else    pot->adjustDown();
        if (_drawObj) lv_obj_invalidate(_drawObj);
    }
}

// ══ Persistence (LittleFS) ══════════════════════════════════════════════════

void CircuitCoreApp::saveCircuit(const char* filename) {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;

    // Ensure /circuits/ directory exists
    if (!LittleFS.exists("/circuits")) {
        LittleFS.mkdir("/circuits");
    }

    char path[64];
    snprintf(path, sizeof(path), "/circuits/%s", filename);

    File f = LittleFS.open(path, "w");
    if (!f) return;

    // Write header: component count
    f.printf("%d\n", _compCount);

    // Write each component: type,gridX,gridY,nodeA,nodeB,extra...
    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        CircuitComponent* c = _components[i];
        int typeVal = static_cast<int>(c->type());
        f.printf("%d,%d,%d,%d,%d", typeVal, c->gridX(), c->gridY(),
                 c->nodeA(), c->nodeB());

        // Type-specific data
        switch (c->type()) {
            case CompType::RESISTOR:
                f.printf(",%.6f", (double)static_cast<Resistor*>(c)->resistance());
                break;
            case CompType::VOLTAGE_SOURCE:
                f.printf(",%.6f,%d",
                    (double)static_cast<VoltageSource*>(c)->voltage(),
                    static_cast<VoltageSource*>(c)->vsIndex());
                break;
            case CompType::LED:
                f.printf(",%.6f,%d",
                    (double)static_cast<LEDComponent*>(c)->forwardVoltage(),
                    static_cast<LEDComponent*>(c)->vsIndex());
                break;
            case CompType::POTENTIOMETER:
                f.printf(",%.6f", (double)static_cast<Potentiometer*>(c)->resistance());
                break;
            case CompType::PUSH_BUTTON:
                f.printf(",%d", static_cast<PushButton*>(c)->isPressed() ? 1 : 0);
                break;
            case CompType::CAPACITOR:
                f.printf(",%.9f", (double)static_cast<Capacitor*>(c)->capacitance());
                break;
            case CompType::DIODE:
                // No extra data needed
                break;
            default:
                break;
        }
        f.println();
    }

    // Save editor state
    f.printf("STATE,%d,%d,%d,%d\n", _nextNodeId, _nextVsIdx, _cursorX, _cursorY);

    f.close();
#else
    (void)filename;
#endif
}

void CircuitCoreApp::loadCircuit(const char* filename) {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;

    char path[64];
    snprintf(path, sizeof(path), "/circuits/%s", filename);

    File f = LittleFS.open(path, "r");
    if (!f) return;

    // Clear existing components
    for (int i = 0; i < _compCount; ++i) {
        delete _components[i];
        _components[i] = nullptr;
    }
    _compCount = 0;
    _nextNodeId = 1;
    _nextVsIdx = 0;

    // Read header
    String line = f.readStringUntil('\n');
    int totalComps = line.toInt();

    for (int idx = 0; idx < totalComps && _compCount < MAX_COMPONENTS; ++idx) {
        line = f.readStringUntil('\n');
        if (line.startsWith("STATE")) break;

        // Parse: type,gridX,gridY,nodeA,nodeB[,extra...]
        int typeVal, gx, gy, nA, nB;
        int parsed = sscanf(line.c_str(), "%d,%d,%d,%d,%d", &typeVal, &gx, &gy, &nA, &nB);
        if (parsed < 5) continue;

        CircuitComponent* comp = nullptr;
        CompType ctype = static_cast<CompType>(typeVal);

        switch (ctype) {
            case CompType::RESISTOR: {
                float r = 1000.0f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &r);
                comp = new Resistor(gx, gy, r);
                break;
            }
            case CompType::VOLTAGE_SOURCE: {
                float v = 5.0f; int vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f,%d", &v, &vi);
                auto* vs = new VoltageSource(gx, gy, v);
                vs->setVsIndex(vi);
                comp = vs;
                break;
            }
            case CompType::LED: {
                float vf = 2.0f; int vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f,%d", &vf, &vi);
                auto* led = new LEDComponent(gx, gy, vf);
                led->setVsIndex(vi);
                comp = led;
                break;
            }
            case CompType::WIRE:
                comp = new WireComponent(gx, gy);
                break;
            case CompType::GROUND:
                comp = new GroundNode(gx, gy);
                break;
            case CompType::MCU: {
                comp = new MCUComponent(gx, gy);
                break;
            }
            case CompType::POTENTIOMETER: {
                float r = 10000.0f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &r);
                comp = new Potentiometer(gx, gy, r);
                break;
            }
            case CompType::PUSH_BUTTON: {
                int pressed = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &pressed);
                auto* btn = new PushButton(gx, gy);
                btn->setPressed(pressed != 0);
                comp = btn;
                break;
            }
            case CompType::CAPACITOR: {
                float c = 100e-6f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &c);
                comp = new Capacitor(gx, gy, c);
                break;
            }
            case CompType::DIODE:
                comp = new Diode(gx, gy, 0.7f);
                break;
            default:
                continue;
        }

        if (comp) {
            comp->setNodes(nA, nB);
            _components[_compCount++] = comp;
        }
    }

    // Try to read STATE line
    while (f.available()) {
        line = f.readStringUntil('\n');
        if (line.startsWith("STATE")) {
            sscanf(line.c_str(), "STATE,%d,%d,%d,%d",
                   &_nextNodeId, &_nextVsIdx, &_cursorX, &_cursorY);
            break;
        }
    }

    f.close();
    updateInfoBar();
    if (_drawObj) lv_obj_invalidate(_drawObj);
#else
    (void)filename;
#endif
}

void CircuitCoreApp::autoSave() {
    if (_compCount > 0) {
        saveCircuit("autosave.dat");
    }
}

void CircuitCoreApp::autoLoad() {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;
    if (LittleFS.exists("/circuits/autosave.dat")) {
        loadCircuit("autosave.dat");
    }
#endif
}
