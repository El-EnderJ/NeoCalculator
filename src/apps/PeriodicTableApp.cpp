/**
 * PeriodicTableApp.cpp — Interactive Periodic Table for NumOS.
 *
 * LVGL-native app: custom-drawn grid, detail panel, molar mass, balancer.
 */

#include "PeriodicTableApp.h"
#include "ChemCAS.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// ══ Color palette ════════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0x1A1A2E;  // Dark blue-black
static constexpr uint32_t COL_PANEL_BG   = 0x16213E;  // Slightly lighter
static constexpr uint32_t COL_CELL_BG    = 0x2D2D44;  // Default cell
static constexpr uint32_t COL_TEXT       = 0xE0E0E0;  // Light grey text
static constexpr uint32_t COL_TEXT_DIM   = 0x808080;
static constexpr uint32_t COL_CURSOR     = 0xFFD700;  // Gold highlight
static constexpr uint32_t COL_TAB_ACTIVE = 0x4A90D9;  // Active tab
static constexpr uint32_t COL_TAB_IDLE   = 0x333355;  // Inactive tab
static constexpr uint32_t COL_INPUT_BG   = 0x2A2A40;
static constexpr uint32_t COL_RESULT_OK  = 0x4CAF50;  // Green result
static constexpr uint32_t COL_RESULT_ERR = 0xFF5252;  // Red error
static constexpr uint32_t COL_HINT       = 0x666688;
static constexpr uint32_t COL_GRID_LINE  = 0x3A3A55;

// Category colors (bright, distinct for the periodic table)
static constexpr uint32_t CAT_COLORS[] = {
    0x5BC0EB,  // NonMetal       - sky blue
    0xA855F7,  // NobleGas       - purple
    0xFF6B6B,  // AlkaliMetal    - red
    0xFFA07A,  // AlkalineEarth  - salmon
    0x20B2AA,  // Metalloid      - teal
    0x98FB98,  // Halogen        - pale green
    0xFFD93D,  // TransitionMetal - yellow
    0x6BCB77,  // PostTransition  - green
    0xFF8FA3,  // Lanthanide     - pink
    0xC084FC,  // Actinide       - light purple
    0x888888,  // Unknown        - grey
};

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

PeriodicTableApp::PeriodicTableApp()
    : _screen(nullptr)
    , _gridObj(nullptr), _detailPanel(nullptr)
    , _detSymbol(nullptr), _detName(nullptr), _detNumber(nullptr)
    , _detMass(nullptr), _detConfig(nullptr), _detEN(nullptr)
    , _molarContainer(nullptr), _molarInput(nullptr)
    , _molarResult(nullptr), _molarHint(nullptr)
    , _balContainer(nullptr), _balInput(nullptr)
    , _balResult(nullptr), _balHint(nullptr)
    , _currentTab(Tab::TABLE)
    , _cursorAtomicNumber(1)
    , _molarLen(0), _balLen(0)
{
    for (int i = 0; i < 3; ++i) { _tabBtns[i] = nullptr; _tabLabels[i] = nullptr; }
    _molarBuf[0] = '\0';
    _balBuf[0] = '\0';
}

PeriodicTableApp::~PeriodicTableApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Periodic Table");

    createUI();
}

void PeriodicTableApp::end() {
    if (_screen) {
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen = nullptr;
        _statusBar.resetPointers();
        // Nullify all widget pointers
        _gridObj = nullptr;
        _detailPanel = nullptr;
        _detSymbol = nullptr;
        _detName = nullptr;
        _detNumber = nullptr;
        _detMass = nullptr;
        _detConfig = nullptr;
        _detEN = nullptr;
        _molarContainer = nullptr;
        _molarInput = nullptr;
        _molarResult = nullptr;
        _molarHint = nullptr;
        _balContainer = nullptr;
        _balInput = nullptr;
        _balResult = nullptr;
        _balHint = nullptr;
        for (int i = 0; i < 3; ++i) {
            _tabBtns[i] = nullptr;
            _tabLabels[i] = nullptr;
        }
    }
}

void PeriodicTableApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Periodic Table");
    _statusBar.update();
    _currentTab = Tab::TABLE;
    _cursorAtomicNumber = 1;
    switchTab(Tab::TABLE);
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ════════════════════════════════════════════════════════════════════════════
// UI Construction
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::createUI() {
    createTabBar();
    createTableTab();
    createMolarTab();
    createBalanceTab();
    switchTab(Tab::TABLE);
}

void PeriodicTableApp::createTabBar() {
    int barH = ui::StatusBar::HEIGHT + 1;
    const char* tabNames[] = {"Table", "Molar", "Balance"};

    int tabW = SCREEN_W / 3;
    for (int i = 0; i < 3; ++i) {
        _tabBtns[i] = lv_obj_create(_screen);
        lv_obj_set_size(_tabBtns[i], tabW, 20);
        lv_obj_set_pos(_tabBtns[i], i * tabW, barH);
        lv_obj_set_style_bg_color(_tabBtns[i], lv_color_hex(COL_TAB_IDLE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tabBtns[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_tabBtns[i], LV_OBJ_FLAG_SCROLLABLE);

        _tabLabels[i] = lv_label_create(_tabBtns[i]);
        lv_label_set_text(_tabLabels[i], tabNames[i]);
        lv_obj_set_style_text_font(_tabLabels[i], &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(_tabLabels[i], lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_center(_tabLabels[i]);
    }
}

void PeriodicTableApp::createTableTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + 20;  // after status bar + tab bar
    int gridH = GRID_TOTAL_H + 4;  // grid area with margin
    int detailY = topY + gridH;

    // ── Custom-drawn grid object ─────────────────────────────────────────
    _gridObj = lv_obj_create(_screen);
    lv_obj_set_size(_gridObj, SCREEN_W, gridH);
    lv_obj_set_pos(_gridObj, 0, topY);
    lv_obj_set_style_bg_opa(_gridObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_gridObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_gridObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_gridObj, LV_OBJ_FLAG_SCROLLABLE);

    // Store 'this' pointer for the draw callback
    lv_obj_set_user_data(_gridObj, this);
    lv_obj_add_event_cb(_gridObj, onGridDraw, LV_EVENT_DRAW_MAIN, this);

    // ── Detail panel ─────────────────────────────────────────────────────
    _detailPanel = lv_obj_create(_screen);
    lv_obj_set_size(_detailPanel, SCREEN_W, SCREEN_H - detailY);
    lv_obj_set_pos(_detailPanel, 0, detailY);
    lv_obj_set_style_bg_color(_detailPanel, lv_color_hex(COL_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_detailPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_detailPanel, lv_color_hex(COL_GRID_LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_detailPanel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(_detailPanel, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(_detailPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_detailPanel, 4, LV_PART_MAIN);
    lv_obj_remove_flag(_detailPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Giant symbol (left side)
    _detSymbol = lv_label_create(_detailPanel);
    lv_obj_set_style_text_font(_detSymbol, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(_detSymbol, lv_color_hex(COL_CURSOR), LV_PART_MAIN);
    lv_obj_set_pos(_detSymbol, 8, 4);

    // Atomic number (above symbol)
    _detNumber = lv_label_create(_detailPanel);
    lv_obj_set_style_text_font(_detNumber, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_detNumber, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_pos(_detNumber, 50, 4);

    // Full name
    _detName = lv_label_create(_detailPanel);
    lv_obj_set_style_text_font(_detName, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_detName, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(_detName, 50, 18);

    // Mass
    _detMass = lv_label_create(_detailPanel);
    lv_obj_set_style_text_font(_detMass, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_detMass, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(_detMass, 50, 36);

    // Electron config
    _detConfig = lv_label_create(_detailPanel);
    lv_obj_set_style_text_font(_detConfig, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_detConfig, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_pos(_detConfig, 50, 48);

    // Electronegativity
    _detEN = lv_label_create(_detailPanel);
    lv_obj_set_style_text_font(_detEN, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_detEN, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_pos(_detEN, 200, 36);

    updateFocusedElement();
}

void PeriodicTableApp::createMolarTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + 20;

    _molarContainer = lv_obj_create(_screen);
    lv_obj_set_size(_molarContainer, SCREEN_W, SCREEN_H - topY);
    lv_obj_set_pos(_molarContainer, 0, topY);
    lv_obj_set_style_bg_color(_molarContainer, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_molarContainer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_molarContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_molarContainer, 8, LV_PART_MAIN);
    lv_obj_remove_flag(_molarContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(_molarContainer);
    lv_label_set_text(title, "Molar Mass Calculator");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 4);

    // "Formula:" label
    lv_obj_t* fLabel = lv_label_create(_molarContainer);
    lv_label_set_text(fLabel, "Formula:");
    lv_obj_set_style_text_font(fLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(fLabel, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_pos(fLabel, 0, 30);

    // Input display
    _molarInput = lv_label_create(_molarContainer);
    lv_label_set_text(_molarInput, "_");
    lv_obj_set_style_text_font(_molarInput, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(_molarInput, lv_color_hex(COL_CURSOR), LV_PART_MAIN);
    lv_obj_set_pos(_molarInput, 0, 44);
    lv_obj_set_width(_molarInput, SCREEN_W - 24);

    // Result
    _molarResult = lv_label_create(_molarContainer);
    lv_label_set_text(_molarResult, "");
    lv_obj_set_style_text_font(_molarResult, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_molarResult, lv_color_hex(COL_RESULT_OK), LV_PART_MAIN);
    lv_obj_set_pos(_molarResult, 0, 80);
    lv_obj_set_width(_molarResult, SCREEN_W - 24);

    // Hint
    _molarHint = lv_label_create(_molarContainer);
    lv_label_set_text(_molarHint,
        "A-F keys=letters  NUM=digits  ()  ENTER=calc");
    lv_obj_set_style_text_font(_molarHint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_molarHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_molarHint, 0, SCREEN_H - topY - 30);
}

void PeriodicTableApp::createBalanceTab() {
    int topY = ui::StatusBar::HEIGHT + 1 + 20;

    _balContainer = lv_obj_create(_screen);
    lv_obj_set_size(_balContainer, SCREEN_W, SCREEN_H - topY);
    lv_obj_set_pos(_balContainer, 0, topY);
    lv_obj_set_style_bg_color(_balContainer, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_balContainer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_balContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_balContainer, 8, LV_PART_MAIN);
    lv_obj_remove_flag(_balContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(_balContainer);
    lv_label_set_text(title, "Equation Balancer");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 4);

    // "Equation:" label
    lv_obj_t* eLabel = lv_label_create(_balContainer);
    lv_label_set_text(eLabel, "Equation:");
    lv_obj_set_style_text_font(eLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(eLabel, lv_color_hex(COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_pos(eLabel, 0, 30);

    // Input display
    _balInput = lv_label_create(_balContainer);
    lv_label_set_text(_balInput, "_");
    lv_obj_set_style_text_font(_balInput, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_balInput, lv_color_hex(COL_CURSOR), LV_PART_MAIN);
    lv_obj_set_pos(_balInput, 0, 44);
    lv_obj_set_width(_balInput, SCREEN_W - 24);

    // Result
    _balResult = lv_label_create(_balContainer);
    lv_label_set_text(_balResult, "");
    lv_obj_set_style_text_font(_balResult, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_balResult, lv_color_hex(COL_RESULT_OK), LV_PART_MAIN);
    lv_obj_set_pos(_balResult, 0, 80);
    lv_obj_set_width(_balResult, SCREEN_W - 24);

    // Hint
    _balHint = lv_label_create(_balContainer);
    lv_label_set_text(_balHint,
        "A-F=letters NUM=digits +=plus ==equals ENTER=balance");
    lv_obj_set_style_text_font(_balHint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_balHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_balHint, 0, SCREEN_H - topY - 30);
}

// ════════════════════════════════════════════════════════════════════════════
// Tab switching
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::switchTab(Tab tab) {
    _currentTab = tab;
    updateTabHighlight();

    bool showTable = (tab == Tab::TABLE);
    bool showMolar = (tab == Tab::MOLAR);
    bool showBal   = (tab == Tab::BALANCE);

    if (_gridObj)        lv_obj_set_style_opa(_gridObj, showTable ? LV_OPA_COVER : LV_OPA_0, LV_PART_MAIN);
    if (_detailPanel)    lv_obj_set_style_opa(_detailPanel, showTable ? LV_OPA_COVER : LV_OPA_0, LV_PART_MAIN);
    if (_molarContainer) lv_obj_set_style_opa(_molarContainer, showMolar ? LV_OPA_COVER : LV_OPA_0, LV_PART_MAIN);
    if (_balContainer)   lv_obj_set_style_opa(_balContainer, showBal ? LV_OPA_COVER : LV_OPA_0, LV_PART_MAIN);

    // Show/hide by moving off-screen vs on-screen
    if (_gridObj) {
        if (showTable) {
            int topY = ui::StatusBar::HEIGHT + 1 + 20;
            lv_obj_set_pos(_gridObj, 0, topY);
            int gridH = GRID_TOTAL_H + 4;
            lv_obj_set_pos(_detailPanel, 0, topY + gridH);
        } else {
            lv_obj_set_pos(_gridObj, -400, 0);
            lv_obj_set_pos(_detailPanel, -400, 0);
        }
    }
    if (_molarContainer) {
        int topY = ui::StatusBar::HEIGHT + 1 + 20;
        lv_obj_set_pos(_molarContainer, showMolar ? 0 : -400, topY);
    }
    if (_balContainer) {
        int topY = ui::StatusBar::HEIGHT + 1 + 20;
        lv_obj_set_pos(_balContainer, showBal ? 0 : -400, topY);
    }

    if (showTable) {
        updateFocusedElement();
        if (_gridObj) lv_obj_invalidate(_gridObj);
    }
}

void PeriodicTableApp::updateTabHighlight() {
    for (int i = 0; i < 3; ++i) {
        if (!_tabBtns[i]) continue;
        bool active = (i == static_cast<int>(_currentTab));
        lv_obj_set_style_bg_color(_tabBtns[i],
            lv_color_hex(active ? COL_TAB_ACTIVE : COL_TAB_IDLE), LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Detail panel update
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::updateFocusedElement() {
    if (_cursorAtomicNumber < 1 || _cursorAtomicNumber > 118) return;
    const chem::ElementData& el = chem::ELEMENTS[_cursorAtomicNumber - 1];

    char buf[64];

    if (_detSymbol) lv_label_set_text(_detSymbol, el.symbol);

    if (_detNumber) {
        snprintf(buf, sizeof(buf), "#%d", el.atomicNumber);
        lv_label_set_text(_detNumber, buf);
    }

    if (_detName) lv_label_set_text(_detName, el.name);

    if (_detMass) {
        snprintf(buf, sizeof(buf), "Mass: %.3f g/mol", (double)el.mass);
        lv_label_set_text(_detMass, buf);
    }

    if (_detConfig) {
        snprintf(buf, sizeof(buf), "e-: %s", el.electronConfig);
        lv_label_set_text(_detConfig, buf);
    }

    if (_detEN) {
        if (el.electronegativity > 0.0f) {
            snprintf(buf, sizeof(buf), "EN: %.2f", (double)el.electronegativity);
        } else {
            snprintf(buf, sizeof(buf), "EN: N/A");
        }
        lv_label_set_text(_detEN, buf);
    }

    // Color the symbol with category color
    if (_detSymbol) {
        lv_obj_set_style_text_color(_detSymbol,
            lv_color_hex(categoryColor(el.category)), LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Table navigation using NAV_LUT
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::navigateTable(KeyCode dir) {
    if (_cursorAtomicNumber < 1 || _cursorAtomicNumber > 118) return;

    const uint8_t* nav = chem::NAV_LUT[_cursorAtomicNumber - 1];
    uint8_t next = 0;

    switch (dir) {
        case KeyCode::UP:    next = nav[0]; break;
        case KeyCode::DOWN:  next = nav[1]; break;
        case KeyCode::LEFT:  next = nav[2]; break;
        case KeyCode::RIGHT: next = nav[3]; break;
        default: return;
    }

    if (next >= 1 && next <= 118) {
        _cursorAtomicNumber = next;
        updateFocusedElement();
        if (_gridObj) lv_obj_invalidate(_gridObj);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Custom Draw Callback — Periodic Table Grid
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::onGridDraw(lv_event_t* e) {
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    PeriodicTableApp* app = static_cast<PeriodicTableApp*>(lv_event_get_user_data(e));
    if (!app) return;

    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer) return;

    // Get object coordinates
    lv_area_t objArea;
    lv_obj_get_coords(obj, &objArea);

    int ox = objArea.x1 + GRID_X0;
    int oy = objArea.y1 + GRID_Y0;

    // Draw each cell
    for (int row = 0; row < GRID_ROWS; ++row) {
        for (int col = 0; col < GRID_COLS; ++col) {
            int8_t atomNum = chem::TABLE_LAYOUT[row][col];
            if (atomNum < 1) continue;

            int cx = ox + col * CELL_W;
            int cy = oy + row * CELL_H;

            const chem::ElementData& el = chem::ELEMENTS[atomNum - 1];
            bool isCursor = (atomNum == app->_cursorAtomicNumber);

            // Cell background
            lv_area_t cellArea = {
                (int32_t)cx, (int32_t)cy,
                (int32_t)(cx + CELL_W - 1), (int32_t)(cy + CELL_H - 1)
            };

            lv_draw_rect_dsc_t rectDsc;
            lv_draw_rect_dsc_init(&rectDsc);

            if (isCursor) {
                rectDsc.bg_color = lv_color_hex(COL_CURSOR);
                rectDsc.bg_opa = LV_OPA_COVER;
                rectDsc.border_color = lv_color_hex(0xFFFFFF);
                rectDsc.border_width = 1;
                rectDsc.border_opa = LV_OPA_COVER;
            } else {
                rectDsc.bg_color = lv_color_hex(categoryColor(el.category));
                rectDsc.bg_opa = LV_OPA_60;
                rectDsc.border_color = lv_color_hex(COL_GRID_LINE);
                rectDsc.border_width = 1;
                rectDsc.border_opa = LV_OPA_40;
            }
            rectDsc.radius = 1;
            lv_draw_rect(layer, &rectDsc, &cellArea);

            // Element symbol text
            lv_draw_label_dsc_t labelDsc;
            lv_draw_label_dsc_init(&labelDsc);
            labelDsc.color = isCursor ? lv_color_hex(0x000000) : lv_color_hex(COL_TEXT);
            labelDsc.font = &lv_font_montserrat_10;
            labelDsc.align = LV_TEXT_ALIGN_CENTER;
            labelDsc.opa = LV_OPA_COVER;
            labelDsc.text = el.symbol;

            // Center the symbol in the cell
            lv_area_t textArea = {
                (int32_t)cx, (int32_t)(cy + 1),
                (int32_t)(cx + CELL_W - 1), (int32_t)(cy + CELL_H - 1)
            };
            lv_draw_label(layer, &labelDsc, &textArea);
        }
    }

    // Draw gap/separator between main table and lanthanides/actinides
    // Row 7 is empty — draw a small gap indicator
    {
        int gapY = oy + 7 * CELL_H + CELL_H / 2 - 1;
        lv_area_t gapArea = {
            (int32_t)(ox + 2 * CELL_W), (int32_t)gapY,
            (int32_t)(ox + 16 * CELL_W), (int32_t)(gapY + 1)
        };
        lv_draw_rect_dsc_t gapDsc;
        lv_draw_rect_dsc_init(&gapDsc);
        gapDsc.bg_color = lv_color_hex(COL_GRID_LINE);
        gapDsc.bg_opa = LV_OPA_30;
        lv_draw_rect(layer, &gapDsc, &gapArea);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Category color mapping
// ════════════════════════════════════════════════════════════════════════════

uint32_t PeriodicTableApp::categoryColor(chem::ChemCategory cat) {
    int idx = static_cast<int>(cat);
    if (idx >= 0 && idx < static_cast<int>(sizeof(CAT_COLORS) / sizeof(CAT_COLORS[0]))) {
        return CAT_COLORS[idx];
    }
    return 0x888888;
}

// ════════════════════════════════════════════════════════════════════════════
// Text input helpers
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::appendChar(char* buf, int& len, int maxLen, char c) {
    if (len < maxLen - 1) {
        buf[len++] = c;
        buf[len] = '\0';
    }
}

void PeriodicTableApp::deleteChar(char* buf, int& len) {
    if (len > 0) {
        buf[--len] = '\0';
    }
}

char PeriodicTableApp::keyToChar(const KeyEvent& ev) {
    auto& km = vpam::KeyboardManager::instance();

    // Alpha keys produce uppercase letters
    if (km.isAlpha()) {
        switch (ev.code) {
            case KeyCode::ALPHA_A: return 'A';
            case KeyCode::ALPHA_B: return 'B';
            case KeyCode::ALPHA_C: return 'C';
            case KeyCode::ALPHA_D: return 'D';
            case KeyCode::ALPHA_E: return 'E';
            case KeyCode::ALPHA_F: return 'F';
            default: break;
        }
    }

    // Direct alpha keys (without modifier) for chemistry input
    switch (ev.code) {
        case KeyCode::ALPHA_A: return 'A';
        case KeyCode::ALPHA_B: return 'B';
        case KeyCode::ALPHA_C: return 'C';
        case KeyCode::ALPHA_D: return 'D';
        case KeyCode::ALPHA_E: return 'E';
        case KeyCode::ALPHA_F: return 'F';
        default: break;
    }

    // Shift + alpha for lowercase
    if (km.isShift()) {
        switch (ev.code) {
            case KeyCode::ALPHA_A: return 'a';
            case KeyCode::ALPHA_B: return 'b';
            case KeyCode::ALPHA_C: return 'c';
            case KeyCode::ALPHA_D: return 'd';
            case KeyCode::ALPHA_E: return 'e';
            case KeyCode::ALPHA_F: return 'f';
            default: break;
        }
    }

    // Numbers
    switch (ev.code) {
        case KeyCode::NUM_0: return '0';
        case KeyCode::NUM_1: return '1';
        case KeyCode::NUM_2: return '2';
        case KeyCode::NUM_3: return '3';
        case KeyCode::NUM_4: return '4';
        case KeyCode::NUM_5: return '5';
        case KeyCode::NUM_6: return '6';
        case KeyCode::NUM_7: return '7';
        case KeyCode::NUM_8: return '8';
        case KeyCode::NUM_9: return '9';
        default: break;
    }

    // Special characters for chemistry
    switch (ev.code) {
        case KeyCode::LPAREN:  return '(';
        case KeyCode::RPAREN:  return ')';
        case KeyCode::ADD:     return '+';
        case KeyCode::MUL:     return '*';
        case KeyCode::DOT:     return '.';
        default: break;
    }

    return '\0';
}

// ════════════════════════════════════════════════════════════════════════════
// Key handling
// ════════════════════════════════════════════════════════════════════════════

void PeriodicTableApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // F1/F2/F3 switch tabs (or SHIFT+LEFT/RIGHT)
    auto& km = vpam::KeyboardManager::instance();

    if (ev.code == KeyCode::F1) { switchTab(Tab::TABLE); return; }
    if (ev.code == KeyCode::F2) { switchTab(Tab::MOLAR); return; }
    if (ev.code == KeyCode::F3) { switchTab(Tab::BALANCE); return; }

    // Tab cycling with SHIFT+LEFT/RIGHT
    if (km.isShift()) {
        if (ev.code == KeyCode::LEFT) {
            int t = (static_cast<int>(_currentTab) + 2) % 3;
            switchTab(static_cast<Tab>(t));
            return;
        }
        if (ev.code == KeyCode::RIGHT) {
            int t = (static_cast<int>(_currentTab) + 1) % 3;
            switchTab(static_cast<Tab>(t));
            return;
        }
    }

    switch (_currentTab) {
        case Tab::TABLE:   handleKeyTable(ev); break;
        case Tab::MOLAR:   handleKeyMolar(ev); break;
        case Tab::BALANCE: handleKeyBalance(ev); break;
    }
}

void PeriodicTableApp::handleKeyTable(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
        case KeyCode::DOWN:
        case KeyCode::LEFT:
        case KeyCode::RIGHT:
            navigateTable(ev.code);
            break;
        default:
            break;
    }
}

void PeriodicTableApp::handleKeyMolar(const KeyEvent& ev) {
    if (ev.code == KeyCode::DEL || ev.code == KeyCode::AC) {
        if (ev.code == KeyCode::AC) {
            _molarLen = 0;
            _molarBuf[0] = '\0';
            if (_molarResult) lv_label_set_text(_molarResult, "");
        } else {
            deleteChar(_molarBuf, _molarLen);
        }
        // Update display
        if (_molarInput) {
            char display[68];
            snprintf(display, sizeof(display), "%s_", _molarBuf);
            lv_label_set_text(_molarInput, display);
        }
        return;
    }

    if (ev.code == KeyCode::ENTER) {
        // Calculate molar mass
        if (_molarLen > 0) {
            float mass = chem::parseMolarMass(_molarBuf);
            char buf[64];
            if (mass > 0.0f) {
                snprintf(buf, sizeof(buf), "M = %.3f g/mol", (double)mass);
                if (_molarResult) {
                    lv_obj_set_style_text_color(_molarResult,
                        lv_color_hex(COL_RESULT_OK), LV_PART_MAIN);
                    lv_label_set_text(_molarResult, buf);
                }
            } else {
                if (_molarResult) {
                    lv_obj_set_style_text_color(_molarResult,
                        lv_color_hex(COL_RESULT_ERR), LV_PART_MAIN);
                    lv_label_set_text(_molarResult, "Error: Invalid formula");
                }
            }
        }
        return;
    }

    // Character input
    char c = keyToChar(ev);
    if (c != '\0') {
        appendChar(_molarBuf, _molarLen, static_cast<int>(sizeof(_molarBuf)), c);
        if (_molarInput) {
            char display[68];
            snprintf(display, sizeof(display), "%s_", _molarBuf);
            lv_label_set_text(_molarInput, display);
        }
    }
}

void PeriodicTableApp::handleKeyBalance(const KeyEvent& ev) {
    if (ev.code == KeyCode::DEL || ev.code == KeyCode::AC) {
        if (ev.code == KeyCode::AC) {
            _balLen = 0;
            _balBuf[0] = '\0';
            if (_balResult) lv_label_set_text(_balResult, "");
        } else {
            deleteChar(_balBuf, _balLen);
        }
        if (_balInput) {
            char display[132];
            snprintf(display, sizeof(display), "%s_", _balBuf);
            lv_label_set_text(_balInput, display);
        }
        return;
    }

    if (ev.code == KeyCode::ENTER) {
        if (_balLen > 0) {
            char result[chem::MAX_RESULT_LEN];
            if (chem::balanceEquation(_balBuf, result)) {
                if (_balResult) {
                    lv_obj_set_style_text_color(_balResult,
                        lv_color_hex(COL_RESULT_OK), LV_PART_MAIN);
                    lv_label_set_text(_balResult, result);
                }
            } else {
                if (_balResult) {
                    lv_obj_set_style_text_color(_balResult,
                        lv_color_hex(COL_RESULT_ERR), LV_PART_MAIN);
                    lv_label_set_text(_balResult, "Error: Cannot balance");
                }
            }
        }
        return;
    }

    // Special: '=' sign for equation separator
    if (ev.code == KeyCode::SOLVE) {
        appendChar(_balBuf, _balLen, static_cast<int>(sizeof(_balBuf)), '=');
        if (_balInput) {
            char display[132];
            snprintf(display, sizeof(display), "%s_", _balBuf);
            lv_label_set_text(_balInput, display);
        }
        return;
    }

    // Character input
    char c = keyToChar(ev);
    // In balance mode, also allow '=' via the SETUP/EXE key mapping
    if (c == '*') c = ' ';  // don't allow * in equations, use space
    if (c != '\0') {
        appendChar(_balBuf, _balLen, static_cast<int>(sizeof(_balBuf)), c);
        if (_balInput) {
            char display[132];
            snprintf(display, sizeof(display), "%s_", _balBuf);
            lv_label_set_text(_balInput, display);
        }
    }
}
