/**
 * CircuitCoreApp.cpp — Real-time SPICE-like Circuit Simulator for NumOS.
 *
 * LVGL-native app: grid-based circuit editor with MNA solver.
 * Custom draw via LV_EVENT_DRAW_MAIN. Component arrays in PSRAM.
 */

#include "CircuitCoreApp.h"
#include "ComponentFactory.h"
#include "LogicGates.h"
#include "PowerSystems.h"
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

// ── Toolbar labels (per page, 10 tools each) ────────────────────────────────
static const char* PAGE0_LABELS[] = {
    "RES", "VCC", "GND", "LED", "MCU", "WIRE", "POT", "BTN", "CAP", "DIO"
};
static const char* PAGE1_LABELS[] = {
    "LDR", "TMP", "FLX", "FSR", "NPN", "PNP", "MOS", "OPA", "BUZ", "7SG"
};
static const char* PAGE2_LABELS[] = {
    "AND", "OR", "NOT", "NAN", "XOR", "1V5", "3V", "9V", "MTR", "RUN"
};
static const char** PAGE_LABELS[] = { PAGE0_LABELS, PAGE1_LABELS, PAGE2_LABELS };
static constexpr int PAGE_SIZES[] = { 10, 10, 10 };

// ── Toolbar tooltip descriptions (removed static array, using factory) ──────
// Tooltips now come from ComponentFactory::tooltip() and are mapped dynamically.

// ══ Constructor / Destructor ═════════════════════════════════════════════════

CircuitCoreApp::CircuitCoreApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _toolbarObj(nullptr)
    , _infoLabel(nullptr)
    , _pageLabel(nullptr)
    , _simTimer(nullptr)
    , _components(nullptr)
    , _compCount(0)
    , _currentTool(Tool::RES)
    , _toolbarIdx(0)
    , _toolbarPage(0)
    , _focusArea(FocusArea::GRID)
    , _cursorX(GRID_SNAP * 5)   // start near center
    , _cursorY(GRID_SNAP * 4)
    , _simRunning(false)
    , _frameCount(0)
    , _nextNodeId(1)
    , _nextVsIdx(0)
    , _sensorSliderValue(0.5f)
    , _scopeWriteIdx(0)
    , _probeNode(-1)
    , _probeNode2(-1)
    , _scopeActive(false)
    , _scopeTimebase(1)
    , _meterNodeA(-1)
    , _meterNodeB(-1)
    , _meterActive(false)
{
    _infoBuf[0] = '\0';
    for (int i = 0; i < TOOLS_PER_PAGE; ++i)
        _toolBtns[i] = nullptr;
    memset(_scopeBuffer, 0, sizeof(_scopeBuffer));
    memset(_scopeBuffer2, 0, sizeof(_scopeBuffer2));
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
    _toolbarPage = 0;
    _focusArea = FocusArea::GRID;
    _cursorX = GRID_SNAP * 5;
    _cursorY = GRID_SNAP * 4;
    _simRunning = false;
    _frameCount = 0;
    _nextNodeId = 1;
    _nextVsIdx = 0;
    _sensorSliderValue = 0.5f;
    _scopeWriteIdx = 0;
    _probeNode = -1;
    _probeNode2 = -1;
    _scopeActive = false;
    _scopeTimebase = 1;
    _meterNodeA = -1;
    _meterNodeB = -1;
    _meterActive = false;
    memset(_scopeBuffer, 0, sizeof(_scopeBuffer));
    memset(_scopeBuffer2, 0, sizeof(_scopeBuffer2));

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
    _pageLabel = nullptr;
    for (int i = 0; i < TOOLS_PER_PAGE; ++i)
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

    // Page indicator label (shows "1/3", "2/3", "3/3")
    _pageLabel = lv_label_create(_toolbarObj);
    lv_obj_set_style_text_font(_pageLabel, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_text_color(_pageLabel, lv_color_hex(0xFFD700), LV_PART_MAIN);

    // Build toolbar for initial page
    rebuildToolbarPage();
}

void CircuitCoreApp::rebuildToolbarPage() {
    // Delete existing tool buttons
    for (int i = 0; i < TOOLS_PER_PAGE; ++i) {
        if (_toolBtns[i]) {
            lv_obj_delete(_toolBtns[i]);
            _toolBtns[i] = nullptr;
        }
    }

    int numTools = PAGE_SIZES[_toolbarPage];
    // Reserve space for page indicator (12px on right)
    int availW = SCREEN_W - 16;
    int btnW = availW / numTools;

    const char** labels = PAGE_LABELS[_toolbarPage];

    for (int i = 0; i < numTools; ++i) {
        lv_obj_t* btn = lv_obj_create(_toolbarObj);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, i * btnW, 0);
        lv_obj_set_size(btn, btnW, TOOLBAR_H);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_TOOLBAR_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TOOL_DIM), LV_PART_MAIN);
        lv_obj_center(lbl);

        _toolBtns[i] = btn;
    }

    // Update page indicator
    if (_pageLabel) {
        char pageBuf[8];
        snprintf(pageBuf, sizeof(pageBuf), "%d/%d", _toolbarPage + 1, NUM_PAGES);
        lv_label_set_text(_pageLabel, pageBuf);
        lv_obj_set_pos(_pageLabel, SCREEN_W - 16, 4);
    }

    updateToolbarHighlight();
}

CircuitCoreApp::Tool CircuitCoreApp::pageToolAt(int page, int idx) const {
    // Map (page, idx) to Tool enum
    int toolIdx = page * TOOLS_PER_PAGE + idx;
    if (toolIdx >= static_cast<int>(Tool::TOOL_COUNT)) return Tool::RES;
    return static_cast<Tool>(toolIdx);
}

int CircuitCoreApp::toolsOnPage(int page) const {
    return PAGE_SIZES[page];
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

    Tool currentPageTool = pageToolAt(_toolbarPage, _toolbarIdx);
    int gx = _cursorX / GRID_SNAP;
    int gy = _cursorY / GRID_SNAP;

    if (_focusArea == FocusArea::TOOLBAR) {
        // Show tooltip for focused toolbar tool
        const char* labels = PAGE_LABELS[_toolbarPage][_toolbarIdx];
        // Determine CompType or special tool
        if (currentPageTool == Tool::RUN) {
            snprintf(_infoBuf, sizeof(_infoBuf), "RUN/STOP - Toggle simulation");
        } else if (currentPageTool == Tool::MULTI) {
            snprintf(_infoBuf, sizeof(_infoBuf), "MULTIMETER - Probe V/I/R");
        } else {
            snprintf(_infoBuf, sizeof(_infoBuf), "P%d [%s] Select to place",
                     _toolbarPage + 1, labels);
        }
    } else if (_simRunning) {
        const char* label = PAGE_LABELS[_toolbarPage][_toolbarIdx < toolsOnPage(_toolbarPage)
            ? _toolbarIdx : 0];
        snprintf(_infoBuf, sizeof(_infoBuf), "SIM [%s] (%d,%d) C:%d",
                 label, gx, gy, _compCount);
    } else {
        const char* label = PAGE_LABELS[_toolbarPage][_toolbarIdx < toolsOnPage(_toolbarPage)
            ? _toolbarIdx : 0];
        snprintf(_infoBuf, sizeof(_infoBuf), "EDIT [%s] (%d,%d) C:%d",
                 label, gx, gy, _compCount);
    }
    lv_label_set_text(_infoLabel, _infoBuf);
}

void CircuitCoreApp::updateToolbarHighlight() {
    int numTools = toolsOnPage(_toolbarPage);
    int currentToolOnPage = static_cast<int>(_currentTool) - _toolbarPage * TOOLS_PER_PAGE;

    for (int i = 0; i < TOOLS_PER_PAGE; ++i) {
        if (!_toolBtns[i]) continue;
        if (i >= numTools) continue;

        bool isActive = (i == currentToolOnPage && currentToolOnPage >= 0);
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

            // Special color for RUN button (page 2, last item)
            Tool thisTool = pageToolAt(_toolbarPage, i);
            if (thisTool == Tool::RUN) {
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
                // In SIM_MODE, UP adjusts potentiometer/sensor under cursor
                adjustPotAt(_cursorX, _cursorY, true);
                adjustSensorAt(_cursorX, _cursorY, 0.1f);
                // Timebase adjustment for scope
                if (_scopeActive && _scopeTimebase < 8) _scopeTimebase++;
            } else {
                if (_cursorY >= GRID_SNAP)
                    _cursorY -= GRID_SNAP;
            }
            break;
        case KeyCode::DOWN:
            if (_simRunning) {
                // In SIM_MODE, DOWN adjusts potentiometer/sensor under cursor
                adjustPotAt(_cursorX, _cursorY, false);
                adjustSensorAt(_cursorX, _cursorY, -0.1f);
                // Timebase adjustment for scope
                if (_scopeActive && _scopeTimebase > 1) _scopeTimebase--;
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
                if (_currentTool == Tool::MULTI) {
                    // Multimeter probe mode
                    probeMultimeter(_cursorX, _cursorY);
                } else {
                    // Probe a node for oscilloscope (Ch1 with ENTER, Ch2 with F4+ENTER)
                    CircuitComponent* comp = findComponentAt(_cursorX, _cursorY);
                    if (comp) {
                        if (_probeNode < 0) {
                            _probeNode = comp->nodeA();
                            _scopeActive = true;
                            _scopeWriteIdx = 0;
                            memset(_scopeBuffer, 0, sizeof(_scopeBuffer));
                        } else if (_probeNode2 < 0) {
                            // Second press: assign channel 2
                            _probeNode2 = comp->nodeA();
                            memset(_scopeBuffer2, 0, sizeof(_scopeBuffer2));
                        } else {
                            // Third press: reset both channels
                            _probeNode = comp->nodeA();
                            _probeNode2 = -1;
                            _scopeWriteIdx = 0;
                            memset(_scopeBuffer, 0, sizeof(_scopeBuffer));
                            memset(_scopeBuffer2, 0, sizeof(_scopeBuffer2));
                        }
                    }
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
    int numTools = toolsOnPage(_toolbarPage);

    switch (ev.code) {
        case KeyCode::LEFT:
            if (_toolbarIdx > 0) {
                _toolbarIdx--;
            } else if (_toolbarPage > 0) {
                // Page left
                _toolbarPage--;
                _toolbarIdx = toolsOnPage(_toolbarPage) - 1;
                rebuildToolbarPage();
            }
            updateToolbarHighlight();
            break;
        case KeyCode::RIGHT:
            if (_toolbarIdx < numTools - 1) {
                _toolbarIdx++;
            } else if (_toolbarPage < NUM_PAGES - 1) {
                // Page right
                _toolbarPage++;
                _toolbarIdx = 0;
                rebuildToolbarPage();
            }
            updateToolbarHighlight();
            break;
        case KeyCode::ENTER: {
            Tool selected = pageToolAt(_toolbarPage, _toolbarIdx);
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

    // Map Tool to CompType for factory creation
    CompType ctype;
    bool isSpecial = false;

    switch (_currentTool) {
        case Tool::RES:       ctype = CompType::RESISTOR; break;
        case Tool::VCC:       ctype = CompType::VOLTAGE_SOURCE; break;
        case Tool::GND:       ctype = CompType::GROUND; break;
        case Tool::LED:       ctype = CompType::LED; break;
        case Tool::MCU:       ctype = CompType::MCU; break;
        case Tool::WIRE:      ctype = CompType::WIRE; break;
        case Tool::POT:       ctype = CompType::POTENTIOMETER; break;
        case Tool::BTN:       ctype = CompType::PUSH_BUTTON; break;
        case Tool::CAP:       ctype = CompType::CAPACITOR; break;
        case Tool::DIODE:     ctype = CompType::DIODE; break;
        case Tool::LDR_T:     ctype = CompType::LDR; break;
        case Tool::THERM:     ctype = CompType::THERMISTOR; break;
        case Tool::FLEX:      ctype = CompType::FLEX_SENSOR; break;
        case Tool::FSR_T:     ctype = CompType::FSR; break;
        case Tool::NPN:       ctype = CompType::NPN_BJT; break;
        case Tool::PNP:       ctype = CompType::PNP_BJT; break;
        case Tool::NMOS_T:    ctype = CompType::NMOS; break;
        case Tool::OPAMP:     ctype = CompType::OP_AMP; break;
        case Tool::BUZZER_T:  ctype = CompType::BUZZER; break;
        case Tool::SEG7:      ctype = CompType::SEVEN_SEG; break;
        case Tool::AND_T:     ctype = CompType::AND_GATE; break;
        case Tool::OR_T:      ctype = CompType::OR_GATE; break;
        case Tool::NOT_T:     ctype = CompType::NOT_GATE; break;
        case Tool::NAND_T:    ctype = CompType::NAND_GATE; break;
        case Tool::XOR_T:     ctype = CompType::XOR_GATE; break;
        case Tool::BAT_AA:    ctype = CompType::BATTERY_AA; break;
        case Tool::BAT_COIN:  ctype = CompType::BATTERY_COIN; break;
        case Tool::BAT_9V:    ctype = CompType::BATTERY_9V; break;
        case Tool::MULTI:     isSpecial = true; break;  // not a placeable component
        case Tool::RUN:       return;  // not a placeable component
        default:              return;
    }

    if (isSpecial) return;

    // Use factory to create the component
    CircuitComponent* comp = ComponentFactory::create(ctype, _cursorX, _cursorY);
    if (!comp) return;

    // Assign nodes based on component type
    if (ctype == CompType::GROUND) {
        // Ground connects to node 0
    } else if (ctype == CompType::MCU) {
        auto* mcu = static_cast<MCUComponent*>(comp);
        for (int i = 0; i < MCUComponent::PIN_COUNT; ++i) {
            mcu->setPinNode(i, _nextNodeId++);
        }
    } else {
        // Standard 2-terminal component
        comp->setNodes(_nextNodeId, _nextNodeId + 1);
        _nextNodeId += 2;

        // Handle 3rd terminal for transistors/op-amps
        int extra = ComponentFactory::extraNodes(ctype);
        if (extra > 0) {
            if (ctype == CompType::NPN_BJT)
                static_cast<NpnBjt*>(comp)->setNodeC(_nextNodeId++);
            else if (ctype == CompType::PNP_BJT)
                static_cast<PnpBjt*>(comp)->setNodeC(_nextNodeId++);
            else if (ctype == CompType::NMOS)
                static_cast<NmosFet*>(comp)->setNodeD(_nextNodeId++);
            else if (ctype == CompType::OP_AMP)
                static_cast<OpAmp*>(comp)->setNodeInN(_nextNodeId++);
        }

        // Handle logic gate output node
        if (ComponentFactory::isLogicGate(ctype)) {
            auto* gate = static_cast<LogicGate*>(comp);
            gate->setNodeOut(_nextNodeId++);
            gate->setVsIndex(_nextVsIdx++);
        }
    }

    // Assign voltage source index if needed
    if (ComponentFactory::needsVsIndex(ctype) && !ComponentFactory::isLogicGate(ctype)) {
        if (ctype == CompType::VOLTAGE_SOURCE)
            static_cast<VoltageSource*>(comp)->setVsIndex(_nextVsIdx++);
        else if (ctype == CompType::LED)
            static_cast<LEDComponent*>(comp)->setVsIndex(_nextVsIdx++);
        else if (ctype == CompType::OP_AMP)
            static_cast<OpAmp*>(comp)->setVsIndex(_nextVsIdx++);
        else if (ctype == CompType::BATTERY_AA)
            static_cast<BatteryAA*>(comp)->setVsIndex(_nextVsIdx++);
        else if (ctype == CompType::BATTERY_COIN)
            static_cast<BatteryCoin*>(comp)->setVsIndex(_nextVsIdx++);
        else if (ctype == CompType::BATTERY_9V)
            static_cast<Battery9V*>(comp)->setVsIndex(_nextVsIdx++);
    }

    _components[_compCount++] = comp;
    updateInfoBar();
    if (_drawObj) lv_obj_invalidate(_drawObj);
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
        if (t == CompType::OP_AMP) vsCount++;
        if (t == CompType::BATTERY_AA || t == CompType::BATTERY_COIN ||
            t == CompType::BATTERY_9V) vsCount++;
        if (ComponentFactory::isLogicGate(t)) vsCount++;
    }
    _mna.setVsCount(vsCount);

    // Phase 1: Stamp wires (Union-Find merge)
    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        if (ComponentFactory::isWirePhase(_components[i]->type())) {
            _components[i]->stampMatrix(_mna);
        }
    }

    // Phase 2: Clear and re-stamp matrices
    _mna.stampClear();

    // Phase 3: Stamp all non-wire components
    for (int i = 0; i < _compCount; ++i) {
        if (!_components[i]) continue;
        if (!ComponentFactory::isWirePhase(_components[i]->type())) {
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

    // ── Draw multimeter (SIM_MODE) ──
    if (app->_meterActive && app->_simRunning) {
        app->drawMultimeter(layer, objX, objY);
    }
}

// ══ Oscilloscope ════════════════════════════════════════════════════════════

void CircuitCoreApp::updateScope() {
    if (!_scopeActive || _probeNode < 0) return;

    // Only sample based on timebase divisor
    if ((_frameCount / 2) % _scopeTimebase != 0) return;

    // Channel 1
    float v = _mna.nodeVoltage(_probeNode);
    _scopeBuffer[_scopeWriteIdx] = v;

    // Channel 2 (if active)
    if (_probeNode2 >= 0) {
        float v2 = _mna.nodeVoltage(_probeNode2);
        _scopeBuffer2[_scopeWriteIdx] = v2;
    }

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

    // Find voltage range for auto-scaling (across both channels)
    static constexpr float SCOPE_MIN_RANGE = 0.1f;
    float vMin = 1e9f, vMax = -1e9f;
    for (int i = 0; i < SCOPE_SAMPLES; ++i) {
        if (_scopeBuffer[i] < vMin) vMin = _scopeBuffer[i];
        if (_scopeBuffer[i] > vMax) vMax = _scopeBuffer[i];
        if (_probeNode2 >= 0) {
            if (_scopeBuffer2[i] < vMin) vMin = _scopeBuffer2[i];
            if (_scopeBuffer2[i] > vMax) vMax = _scopeBuffer2[i];
        }
    }
    float vRange = vMax - vMin;
    if (vRange < SCOPE_MIN_RANGE) { vRange = SCOPE_MIN_RANGE; vMin = vMax - SCOPE_MIN_RANGE / 2.0f; }

    // Draw channel 1 waveform (green)
    lv_draw_line_dsc_t lineDsc;
    lv_draw_line_dsc_init(&lineDsc);
    lineDsc.color = lv_color_hex(0x3FB950);
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

    // Draw channel 2 waveform (cyan) if active
    if (_probeNode2 >= 0) {
        lineDsc.color = lv_color_hex(0x00BFFF);
        for (int i = 0; i < SCOPE_SAMPLES - 1; ++i) {
            int idx0 = (_scopeWriteIdx + i) % SCOPE_SAMPLES;
            int idx1 = (_scopeWriteIdx + i + 1) % SCOPE_SAMPLES;

            float norm0 = (_scopeBuffer2[idx0] - vMin) / vRange;
            float norm1 = (_scopeBuffer2[idx1] - vMin) / vRange;

            int y0 = scopeY + SCOPE_H - 2 - (int)(norm0 * (SCOPE_H - 4));
            int y1 = scopeY + SCOPE_H - 2 - (int)(norm1 * (SCOPE_H - 4));

            lineDsc.p1.x = scopeX + i;
            lineDsc.p1.y = y0;
            lineDsc.p2.x = scopeX + i + 1;
            lineDsc.p2.y = y1;
            lv_draw_line(layer, &lineDsc);
        }
    }

    // Probe labels
    char probeBuf[32];
    snprintf(probeBuf, sizeof(probeBuf), "Ch1:N%d T:%dx", _probeNode, _scopeTimebase);
    lv_draw_label_dsc_t labDsc;
    lv_draw_label_dsc_init(&labDsc);
    labDsc.color = lv_color_hex(0x3FB950);
    labDsc.font  = &lv_font_unscii_8;
    labDsc.text  = probeBuf;

    lv_area_t labArea;
    labArea.x1 = scopeX + 2;
    labArea.y1 = scopeY + 1;
    labArea.x2 = scopeX + SCOPE_W - 2;
    labArea.y2 = scopeY + 10;
    lv_draw_label(layer, &labDsc, &labArea);

    if (_probeNode2 >= 0) {
        char probeBuf2[16];
        snprintf(probeBuf2, sizeof(probeBuf2), "Ch2:N%d", _probeNode2);
        labDsc.color = lv_color_hex(0x00BFFF);
        labDsc.text  = probeBuf2;
        labArea.y1 = scopeY + SCOPE_H - 10;
        labArea.y2 = scopeY + SCOPE_H - 1;
        lv_draw_label(layer, &labDsc, &labArea);
    }
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
        } else if (t == CompType::RESISTOR || t == CompType::POTENTIOMETER ||
                   t == CompType::LDR || t == CompType::THERMISTOR ||
                   t == CompType::FLEX_SENSOR || t == CompType::FSR) {
            // All variable-resistance sensors use V/R for current estimate
            float vA = _mna.nodeVoltage(_components[i]->nodeA());
            float vB = _mna.nodeVoltage(_components[i]->nodeB());
            float r = 1000.0f;  // default
            if (t == CompType::RESISTOR)
                r = static_cast<Resistor*>(_components[i])->resistance();
            else if (t == CompType::POTENTIOMETER)
                r = static_cast<Potentiometer*>(_components[i])->resistance();
            else if (t == CompType::LDR)
                r = static_cast<LDR*>(_components[i])->resistance();
            else if (t == CompType::THERMISTOR)
                r = static_cast<Thermistor*>(_components[i])->resistance();
            else if (t == CompType::FLEX_SENSOR)
                r = static_cast<FlexSensor*>(_components[i])->resistance();
            else if (t == CompType::FSR)
                r = static_cast<FSRComponent*>(_components[i])->resistance();
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

void CircuitCoreApp::adjustSensorAt(int gx, int gy, float delta) {
    CircuitComponent* comp = findComponentAt(gx, gy);
    if (!comp) return;

    _sensorSliderValue += delta;
    if (_sensorSliderValue < 0.0f) _sensorSliderValue = 0.0f;
    if (_sensorSliderValue > 1.0f) _sensorSliderValue = 1.0f;

    switch (comp->type()) {
        case CompType::LDR:
            static_cast<LDR*>(comp)->setLightLevel(_sensorSliderValue);
            break;
        case CompType::THERMISTOR:
            // Map 0-1 → -20°C to 100°C
            static_cast<Thermistor*>(comp)->setTemperature(-20.0f + 120.0f * _sensorSliderValue);
            break;
        case CompType::FLEX_SENSOR:
            static_cast<FlexSensor*>(comp)->setBendLevel(_sensorSliderValue);
            break;
        case CompType::FSR:
            static_cast<FSRComponent*>(comp)->setForceLevel(_sensorSliderValue);
            break;
        default:
            return;  // Not a sensor
    }
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

// ══ Multimeter ══════════════════════════════════════════════════════════════

void CircuitCoreApp::probeMultimeter(int gx, int gy) {
    CircuitComponent* comp = findComponentAt(gx, gy);
    if (!comp) return;

    if (_meterNodeA < 0) {
        // First probe
        _meterNodeA = comp->nodeA();
        _meterActive = true;
    } else if (_meterNodeB < 0) {
        // Second probe
        _meterNodeB = comp->nodeA();
    } else {
        // Reset
        _meterNodeA = comp->nodeA();
        _meterNodeB = -1;
    }
    if (_drawObj) lv_obj_invalidate(_drawObj);
}

void CircuitCoreApp::drawMultimeter(lv_layer_t* layer, int objX, int objY) {
    // Position: top-right corner
    int mX = objX + GRID_W - 90;
    int mY = objY + 4;

    lv_draw_rect_dsc_t bgDsc;
    lv_draw_rect_dsc_init(&bgDsc);
    bgDsc.bg_color = lv_color_hex(0x0A0A0A);
    bgDsc.bg_opa   = LV_OPA_90;
    bgDsc.border_color = lv_color_hex(0xFFD700);
    bgDsc.border_width = 1;
    bgDsc.radius = 3;

    lv_area_t bg;
    bg.x1 = mX; bg.y1 = mY;
    bg.x2 = mX + 86; bg.y2 = mY + 36;
    lv_draw_rect(layer, &bgDsc, &bg);

    // Compute readings
    float vA = (_meterNodeA >= 0) ? _mna.nodeVoltage(_meterNodeA) : 0.0f;
    float vB = (_meterNodeB >= 0) ? _mna.nodeVoltage(_meterNodeB) : 0.0f;
    float voltage = vA - vB;

    char line1[32], line2[32];
    snprintf(line1, sizeof(line1), "V: %.3fV", (double)voltage);
    if (_meterNodeB >= 0) {
        snprintf(line2, sizeof(line2), "A:%d B:%d", _meterNodeA, _meterNodeB);
    } else {
        snprintf(line2, sizeof(line2), "Probe A: N%d", _meterNodeA);
    }

    lv_draw_label_dsc_t labDsc;
    lv_draw_label_dsc_init(&labDsc);
    labDsc.color = lv_color_hex(0xFFD700);
    labDsc.font  = &lv_font_unscii_8;

    labDsc.text = line1;
    lv_area_t la;
    la.x1 = mX + 4; la.y1 = mY + 3;
    la.x2 = mX + 82; la.y2 = mY + 15;
    lv_draw_label(layer, &labDsc, &la);

    labDsc.text = line2;
    labDsc.color = lv_color_hex(0xA0A0A0);
    la.y1 = mY + 18; la.y2 = mY + 30;
    lv_draw_label(layer, &labDsc, &la);
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
                break;
            // New sensor components (save current sensor levels)
            case CompType::LDR:
                f.printf(",%.6f", (double)static_cast<LDR*>(c)->lightLevel());
                break;
            case CompType::THERMISTOR:
                f.printf(",%.6f", (double)static_cast<Thermistor*>(c)->temperature());
                break;
            case CompType::FLEX_SENSOR:
                f.printf(",%.6f", (double)static_cast<FlexSensor*>(c)->bendLevel());
                break;
            case CompType::FSR:
                f.printf(",%.6f", (double)static_cast<FSRComponent*>(c)->forceLevel());
                break;
            // Transistors (save 3rd node)
            case CompType::NPN_BJT:
                f.printf(",%d", static_cast<NpnBjt*>(c)->nodeC());
                break;
            case CompType::PNP_BJT:
                f.printf(",%d", static_cast<PnpBjt*>(c)->nodeC());
                break;
            case CompType::NMOS:
                f.printf(",%d", static_cast<NmosFet*>(c)->nodeD());
                break;
            case CompType::OP_AMP:
                f.printf(",%d,%d",
                    static_cast<OpAmp*>(c)->nodeInN(),
                    static_cast<OpAmp*>(c)->vsIndex());
                break;
            // Logic gates (save output node and vs index)
            case CompType::AND_GATE:
            case CompType::OR_GATE:
            case CompType::NOT_GATE:
            case CompType::NAND_GATE:
            case CompType::XOR_GATE:
                f.printf(",%d,%d",
                    static_cast<LogicGate*>(c)->nodeOut(),
                    static_cast<LogicGate*>(c)->vsIndex());
                break;
            // Batteries (save vs index)
            case CompType::BATTERY_AA:
                f.printf(",%d", static_cast<BatteryAA*>(c)->vsIndex());
                break;
            case CompType::BATTERY_COIN:
                f.printf(",%d", static_cast<BatteryCoin*>(c)->vsIndex());
                break;
            case CompType::BATTERY_9V:
                f.printf(",%d", static_cast<Battery9V*>(c)->vsIndex());
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
            case CompType::MCU:
                comp = new MCUComponent(gx, gy);
                break;
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
            // New sensor components
            case CompType::LDR: {
                float lvl = 0.0f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &lvl);
                auto* ldr = new LDR(gx, gy);
                ldr->setLightLevel(lvl);
                comp = ldr;
                break;
            }
            case CompType::THERMISTOR: {
                float tmp = 25.0f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &tmp);
                auto* therm = new Thermistor(gx, gy);
                therm->setTemperature(tmp);
                comp = therm;
                break;
            }
            case CompType::FLEX_SENSOR: {
                float bend = 0.0f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &bend);
                auto* flex = new FlexSensor(gx, gy);
                flex->setBendLevel(bend);
                comp = flex;
                break;
            }
            case CompType::FSR: {
                float force = 0.0f;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%f", &force);
                auto* fsr = new FSRComponent(gx, gy);
                fsr->setForceLevel(force);
                comp = fsr;
                break;
            }
            // Transistors
            case CompType::NPN_BJT: {
                int nc = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &nc);
                auto* npn = new NpnBjt(gx, gy);
                npn->setNodeC(nc);
                comp = npn;
                break;
            }
            case CompType::PNP_BJT: {
                int nc = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &nc);
                auto* pnp = new PnpBjt(gx, gy);
                pnp->setNodeC(nc);
                comp = pnp;
                break;
            }
            case CompType::NMOS: {
                int nd = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &nd);
                auto* mos = new NmosFet(gx, gy);
                mos->setNodeD(nd);
                comp = mos;
                break;
            }
            case CompType::OP_AMP: {
                int nin = 0, vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d,%d", &nin, &vi);
                auto* opa = new OpAmp(gx, gy);
                opa->setNodeInN(nin);
                opa->setVsIndex(vi);
                comp = opa;
                break;
            }
            // Logic gates
            case CompType::AND_GATE:
            case CompType::OR_GATE:
            case CompType::NOT_GATE:
            case CompType::NAND_GATE:
            case CompType::XOR_GATE: {
                int nOut = 0, vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d,%d", &nOut, &vi);
                comp = ComponentFactory::create(ctype, gx, gy);
                if (comp) {
                    auto* gate = static_cast<LogicGate*>(comp);
                    gate->setNodeOut(nOut);
                    gate->setVsIndex(vi);
                }
                break;
            }
            // Batteries
            case CompType::BATTERY_AA: {
                int vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &vi);
                auto* bat = new BatteryAA(gx, gy);
                bat->setVsIndex(vi);
                comp = bat;
                break;
            }
            case CompType::BATTERY_COIN: {
                int vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &vi);
                auto* bat = new BatteryCoin(gx, gy);
                bat->setVsIndex(vi);
                comp = bat;
                break;
            }
            case CompType::BATTERY_9V: {
                int vi = 0;
                sscanf(line.c_str(), "%*d,%*d,%*d,%*d,%*d,%d", &vi);
                auto* bat = new Battery9V(gx, gy);
                bat->setVsIndex(vi);
                comp = bat;
                break;
            }
            // Simple components (buzzer, 7-seg) - factory defaults
            case CompType::BUZZER:
            case CompType::SEVEN_SEG:
                comp = ComponentFactory::create(ctype, gx, gy);
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
