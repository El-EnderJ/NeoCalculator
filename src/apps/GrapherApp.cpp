/**
 * GrapherApp.cpp — Grapher 2.0 (100 % LVGL 9)
 *
 * Everything built from lv_obj + lv_label + lv_line,
 * since LV_USE_CANVAS / LV_USE_TABLE / LV_USE_TABVIEW = 0.
 */
#include "GrapherApp.h"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <esp_heap_caps.h>
#include "../math/VariableManager.h"

using namespace vpam;

// ── Layout colours (NumWorks-inspired palette) ─────────────────────────
static constexpr uint32_t COL_BG        = 0xF5F5F5;  // Soft gray background (NumWorks 'fun')
static constexpr uint32_t COL_TAB_BG    = 0xFFB531;  // Yellow tab strip (NumWorks)
static constexpr uint32_t COL_TAB_ACT   = 0xFFFFFF;  // Active tab pill
static constexpr uint32_t COL_TAB_TXT_I = 0xFFF3D6;  // Inactive tab text (light on yellow)
static constexpr uint32_t COL_TAB_TXT_A = 0x000000;  // Active tab text (black)
static constexpr uint32_t COL_ROW_BG    = 0xFFFFFF;  // Expr pill background (white)
static constexpr uint32_t COL_ROW_BRD   = 0xD1D1D1;  // Expr pill border (gray)
static constexpr uint32_t COL_ROW_SEL   = 0x4A90E2;  // Selected pill (blue)
static constexpr uint32_t COL_HINT      = 0x999999;
static constexpr uint32_t COL_ADD_TXT   = 0x888888;  // "Add element" muted
static constexpr uint32_t COL_BTN_BG    = 0x4A90E2;  // Action buttons (blue)
static constexpr uint32_t COL_BTN_TXT   = 0xFFFFFF;
static constexpr uint32_t COL_AXIS      = 0x555555;  // Dark gray axis (NumWorks)
static constexpr uint32_t COL_GRID_MAIN = 0xD1D1D1;  // Main grid (integer units)
static constexpr uint32_t COL_GRID_SUB  = 0xEBEBEB;  // Sub-grid (subdivisions)
static constexpr uint32_t COL_GRAPH_BG  = 0xFDFDFD;  // Off-white graph background
static constexpr uint32_t COL_TB_BG     = 0xF4F4F4;  // Graph toolbar bg
static constexpr uint32_t COL_TB_SEL    = 0x4A90E2;  // Toolbar selected
static constexpr uint32_t COL_TB_TXT    = 0x333333;
static constexpr uint32_t COL_TBL_HDR   = 0xF4F4F4;
static constexpr uint32_t COL_TBL_SEL   = 0xD6EAFF;

// ── Graph plotting constants ────────────────────────────────────────────
static constexpr int GRAPH_MAX_PTS = 320;   // One point per pixel column

// ── Templates ───────────────────────────────────────────────────────────
struct TemplateEntry {
    const char* text;     // Expression to insert
    const char* display;  // Display label
};
static const TemplateEntry TEMPLATES[] = {
    { "2*x+3",     "y = 2x + 3  (Linear)"     },
    { "x^2-4",     "y = x\xB2 - 4  (Parabola)" },
    { "sin(x)",    "y = sin(x)  (Trig)"        },
    { "cos(x)",    "y = cos(x)  (Trig)"        },
    { "x^3",       "y = x\xB3  (Cubic)"         },
    { "1/x",       "y = 1/x  (Hyperbola)"      },
};
static constexpr int NUM_TEMPLATES = sizeof(TEMPLATES) / sizeof(TEMPLATES[0]);

// ═══════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════

GrapherApp::GrapherApp()
    : _screen(nullptr)
    , _tabBar(nullptr)
    , _bgExpr(nullptr)
    , _panelExpr(nullptr), _panelGraph(nullptr), _panelTable(nullptr)
    , _addRow(nullptr), _addLabel(nullptr)
    , _exprHint(nullptr), _plotBtn(nullptr), _tableBtn(nullptr)
    , _graphToolbar(nullptr), _graphArea(nullptr)
    , _axisLineX(nullptr), _axisLineY(nullptr)
    , _traceDot(nullptr), _traceLineH(nullptr), _traceLineV(nullptr)
    , _tracePill(nullptr), _tracePillDot(nullptr), _tracePillLabel(nullptr)
    , _infoBar(nullptr), _infoLabel(nullptr)
    , _calcMenu(nullptr), _calcMenuIdx(0), _calcMenuOpen(false)
    , _shadingCount(0), _shadingActive(false)
    , _tblTable(nullptr)
    , _tab(Tab::EXPRESSIONS), _focus(Focus::TAB_BAR), _tabIdx(0)
    , _numFuncs(0), _exprIdx(0), _exprMode(ExprMode::LIST)
    , _grMode(GrMode::IDLE), _toolIdx(0)
    , _xMin(-10.0f), _xMax(10.0f), _yMin(-7.0f), _yMax(7.0f)
    , _traceX(0.0f), _traceFn(0), _plotDirty(true)
    , _tblRow(0), _tblStart(-5.0f), _tblStep(1.0f), _tblFuncIdx(0)
{
    for (int i = 0; i < 3; ++i) { _tabLabels[i] = nullptr; _tabPills[i] = nullptr; }
    for (int i = 0; i < MAX_FUNCS; ++i) {
        _exprRows[i] = nullptr;
        _exprDots[i] = nullptr;
        _exprASTRow[i] = nullptr;
        _tplBtns[i] = nullptr;
        _funcs[i].text[0] = '\0';
        _funcs[i].len = 0;
        _funcs[i].valid = false;
        _funcs[i].color = FUNC_COLORS[i];
        _funcLines[i] = nullptr;
        _funcPts[i] = nullptr;
        _funcPtCount[i] = 0;
    }
    for (int i = 0; i < 4; ++i) _toolLabels[i] = nullptr;
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) _calcMenuRows[i] = nullptr;
    for (int i = 0; i < 320; ++i) _shadingLines[i] = nullptr;
    _tblTable = nullptr;
    _tplModal = nullptr;
    _tplCount = 0; _tplIdx = 0; _tplOpen = false;
    _tplLoadTimer = nullptr; _tplLoadNext = 0;
    _tplCardW = 0; _tplRowH = 0;
    for (int i = 0; i < 6; ++i) { _tplRows[i] = nullptr; }
}

GrapherApp::~GrapherApp() { end(); }

// ═══════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::begin() {
    if (_screen) return;
    Serial.println("[GRAPHER] begin() start");
    createUI();
    Serial.println("[GRAPHER] createUI done");
    _tab = Tab::EXPRESSIONS;
    _focus = Focus::TAB_BAR;
    _tabIdx = 0;
    switchTab(_tab);
    Serial.println("[GRAPHER] begin() done");
}

void GrapherApp::end() {
    // ── Stop lazy loading timer ──
    if (_tplLoadTimer) {
        lv_timer_delete(_tplLoadTimer);
        _tplLoadTimer = nullptr;
    }
    // ── Destroy VPAM canvases before deleting the screen ──
    for (int i = 0; i < MAX_FUNCS; ++i) {
        _exprCanvas[i].destroy();
        _exprAST[i].reset();
        _exprASTRow[i] = nullptr;
    }
    for (int i = 0; i < 6; ++i) {
        _tplCanvas[i].destroy();
        _tplAST[i].reset();
    }

    for (int i = 0; i < MAX_FUNCS; ++i) {
        if (_funcPts[i]) { heap_caps_free(_funcPts[i]); _funcPts[i] = nullptr; }
        _funcPtCount[i] = 0;
    }
    _statusBar.destroy();
    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
        _tabBar = nullptr;
        _bgExpr = nullptr;
        _panelExpr = nullptr;
        _panelGraph = nullptr;
        _panelTable = nullptr;
        _addRow = nullptr; _addLabel = nullptr;
        _exprHint = nullptr; _plotBtn = nullptr; _tableBtn = nullptr;
        _graphToolbar = nullptr; _graphArea = nullptr;
        _axisLineX = nullptr; _axisLineY = nullptr;
        _traceDot = nullptr; _traceLineH = nullptr; _traceLineV = nullptr;
        _tracePill = nullptr; _tracePillDot = nullptr; _tracePillLabel = nullptr;
        _infoBar = nullptr; _infoLabel = nullptr;
        _calcMenu = nullptr; _calcMenuOpen = false;
        for (int i = 0; i < CALC_MENU_ITEMS; ++i) _calcMenuRows[i] = nullptr;
        for (int i = 0; i < 320; ++i) _shadingLines[i] = nullptr;
        _shadingCount = 0; _shadingActive = false;
        _tblTable = nullptr;
        for (int i = 0; i < 3; ++i) { _tabLabels[i] = nullptr; _tabPills[i] = nullptr; }
        for (int i = 0; i < MAX_FUNCS; ++i) {
            _exprRows[i] = nullptr; _exprDots[i] = nullptr;
            _tplBtns[i] = nullptr;
            _funcLines[i] = nullptr;
        }
        for (int i = 0; i < 4; ++i) _toolLabels[i] = nullptr;
        _tplModal = nullptr; _tplOpen = false;
        for (int i = 0; i < 6; ++i) { _tplRows[i] = nullptr; }
    }
    _tab = Tab::EXPRESSIONS;
    _focus = Focus::TAB_BAR;
    _numFuncs = 0;
    _exprIdx = 0;
    _exprMode = ExprMode::LIST;
}

void GrapherApp::load() {
    Serial.println("[GRAPHER] load() enter");
    if (!_screen) begin();
    Serial.println("[GRAPHER] lv_screen_load...");
    lv_screen_load(_screen);
    Serial.println("[GRAPHER] screen loaded OK");
    _statusBar.update();
    Serial.println("[GRAPHER] load() done");
}

// ═══════════════════════════════════════════════════════════════════════
// UI creation helpers
// ═══════════════════════════════════════════════════════════════════════

static lv_obj_t* makeContainer(lv_obj_t* parent, int x, int y, int w, int h,
                                uint32_t bg = COL_BG, bool scroll = false) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
    if (!scroll) lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y,
                            const char* txt, uint32_t col = 0x000000,
                            const lv_font_t* font = &lv_font_montserrat_14) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, x, y);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(col), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    return lbl;
}

void GrapherApp::createUI() {
    Serial.println("[GRAPHER] screen...");
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Grapher");
    _statusBar.setBatteryLevel(100);

    Serial.println("[GRAPHER] tabBar...");
    createTabBar();
    Serial.println("[GRAPHER] exprPanel...");
    createExpressionsPanel();
    // Graph panel is created lazily on first tab switch
    Serial.println("[GRAPHER] tablePanel...");
    createTablePanel();
    Serial.println("[GRAPHER] panels OK");
}

// ── Tab bar (NumWorks yellow strip with rounded active pill) ─────────────
void GrapherApp::createTabBar() {
    int y = BAR_H;
    _tabBar = makeContainer(_screen, 0, y, SCREEN_W, TAB_H, COL_TAB_BG);

    const char* titles[] = { "Expressions", "Graph", "Table" };
    int tw = SCREEN_W / 3;
    for (int i = 0; i < 3; ++i) {
        // Create a rounded pill bg for each tab label
        lv_obj_t* pill = lv_obj_create(_tabBar);
        lv_obj_set_size(pill, tw - 4, TAB_H - 6);
        lv_obj_set_pos(pill, i * tw + 2, 3);
        lv_obj_set_style_bg_color(pill, lv_color_hex(COL_TAB_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(pill, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(pill, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(pill, 0, LV_PART_MAIN);
        lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
        _tabPills[i] = pill;

        _tabLabels[i] = makeLabel(pill, (tw - 4) / 2 - 30, (TAB_H - 6 - 12) / 2,
                                   titles[i], COL_TAB_TXT_I, &lv_font_montserrat_12);
        lv_obj_set_style_text_align(_tabLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

// ── Notebook-lines callback for static background (NumWorks style) ──────
static void notebookLinesCb(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t* obj = lv_event_get_target_obj(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xEAEAEA);
    dsc.opa   = LV_OPA_COVER;
    dsc.width = 1;

    static constexpr int LINE_STEP = 40;
    for (int32_t y = coords.y1 + LINE_STEP; y < coords.y2; y += LINE_STEP) {
        dsc.p1.x = coords.x1;  dsc.p1.y = y;
        dsc.p2.x = coords.x2;  dsc.p2.y = y;
        lv_draw_line(layer, &dsc);
    }
}

// ── Expressions panel ───────────────────────────────────────────────────
void GrapherApp::createExpressionsPanel() {
    int topY = BAR_H + TAB_H;
    int panelH = SCREEN_H - topY;

    // Static background layer — non-scrollable, with notebook lines
    _bgExpr = makeContainer(_screen, 0, topY, SCREEN_W, panelH, 0xF9F9F9);
    lv_obj_remove_flag(_bgExpr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_bgExpr, notebookLinesCb, LV_EVENT_DRAW_MAIN_BEGIN, nullptr);

    // Scrollable expression panel on top — transparent background
    _panelExpr = makeContainer(_screen, 0, topY, SCREEN_W, panelH);
    lv_obj_set_style_bg_opa(_panelExpr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(_panelExpr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_panelExpr, LV_DIR_VER);

    // Function rows — white pills with thin gray border (NumWorks style)
    for (int i = 0; i < MAX_FUNCS; ++i) {
        int ry = PAD + i * (ROW_H + ROW_GAP);
        _exprRows[i] = makeContainer(_panelExpr, PAD, ry, SCREEN_W - 2 * PAD, ROW_H, COL_ROW_BG);
        lv_obj_set_height(_exprRows[i], LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(_exprRows[i], ROW_H, LV_PART_MAIN);
        lv_obj_set_style_radius(_exprRows[i], PILL_RADIUS, LV_PART_MAIN);
        lv_obj_set_style_border_width(_exprRows[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_exprRows[i], lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
        // Shadow for NumWorks 'fun' look
        lv_obj_set_style_shadow_width(_exprRows[i], 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(_exprRows[i], 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(_exprRows[i], 20, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(_exprRows[i], lv_color_black(), LV_PART_MAIN);
        // Flex centering for pill contents
        lv_obj_set_layout(_exprRows[i], LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(_exprRows[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(_exprRows[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(_exprRows[i], PILL_PAD, LV_PART_MAIN);
        lv_obj_set_style_pad_column(_exprRows[i], 4, LV_PART_MAIN);

        // Colour dot
        _exprDots[i] = lv_obj_create(_exprRows[i]);
        lv_obj_set_size(_exprDots[i], 10, 10);
        lv_obj_set_style_bg_color(_exprDots[i], lv_color_hex(_funcs[i].color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_exprDots[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(_exprDots[i], 5, LV_PART_MAIN);
        lv_obj_set_style_border_width(_exprDots[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_exprDots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_min_width(_exprDots[i], 10, LV_PART_MAIN);

        // VPAM MathCanvas — renders the expression AST
        _exprCanvas[i].create(_exprRows[i]);
        lv_obj_set_size(_exprCanvas[i].obj(), LV_SIZE_CONTENT, ROW_H - 4);
        lv_obj_set_style_min_width(_exprCanvas[i].obj(), 80, LV_PART_MAIN);
        lv_obj_set_flex_grow(_exprCanvas[i].obj(), 1);

        // Templates button — shown only when expression is empty
        int btnW = 70;
        _tplBtns[i] = lv_obj_create(_exprRows[i]);
        lv_obj_set_size(_tplBtns[i], btnW, ROW_H - 6);
        lv_obj_set_style_bg_color(_tplBtns[i], lv_color_hex(0xF2F2F2), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tplBtns[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(_tplBtns[i], 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(_tplBtns[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_tplBtns[i], lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tplBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_tplBtns[i], LV_OBJ_FLAG_SCROLLABLE);
        makeLabel(_tplBtns[i], 6, (ROW_H - 6 - 12) / 2, "Templates", 0x666666, &lv_font_montserrat_12);
        lv_obj_add_flag(_tplBtns[i], LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(_exprRows[i], LV_OBJ_FLAG_HIDDEN);
    }

    // "Add an element" row — dashed-border-style look
    int addY = PAD;   // will be repositioned in refreshExprList
    _addRow = makeContainer(_panelExpr, PAD, addY, SCREEN_W - 2 * PAD, ROW_H, 0xF7F7F7);
    lv_obj_set_style_radius(_addRow, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(_addRow, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_addRow, lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
    _addLabel = makeLabel(_addRow, (SCREEN_W - 2 * PAD) / 2 - 50, (ROW_H - 14) / 2,
                           "Add an element", COL_ADD_TXT);

    // "Plot graph" button — blue rounded pill
    _plotBtn = makeContainer(_panelExpr, PAD, 0, (SCREEN_W - 3 * PAD) / 2, ROW_H, COL_BTN_BG);
    lv_obj_set_style_radius(_plotBtn, 10, LV_PART_MAIN);
    makeLabel(_plotBtn, 20, (ROW_H - 14) / 2, "Plot graph", COL_BTN_TXT);

    // "Display values" button — blue rounded pill
    _tableBtn = makeContainer(_panelExpr, PAD + (SCREEN_W - 3 * PAD) / 2 + PAD, 0,
                              (SCREEN_W - 3 * PAD) / 2, ROW_H, COL_BTN_BG);
    lv_obj_set_style_radius(_tableBtn, 10, LV_PART_MAIN);
    makeLabel(_tableBtn, 10, (ROW_H - 14) / 2, "Display values", COL_BTN_TXT);

    // Bottom hint
    _exprHint = makeLabel(_panelExpr, PAD, SCREEN_H - BAR_H - TAB_H - 20,
                           "ENTER=edit  AC=back", COL_HINT, &lv_font_montserrat_12);

    refreshExprList();
}

// ── Nice step for grid lines (1, 2, 5 × 10^n) ─────────────────────────
static float niceStep(float range, int maxTicks) {
    float rough = range / maxTicks;
    float mag = powf(10.0f, floorf(log10f(rough)));
    float norm = rough / mag;
    float nice;
    if (norm < 1.5f)      nice = 1.0f;
    else if (norm < 3.5f) nice = 2.0f;
    else if (norm < 7.5f) nice = 5.0f;
    else                  nice = 10.0f;
    return nice * mag;
}

// ── Graph grid + axis draw callback ─────────────────────────────────────
static void graphGridDrawCb(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t* obj = lv_event_get_target_obj(e);
    GrapherApp* app = static_cast<GrapherApp*>(lv_event_get_user_data(e));

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t aW = coords.x2 - coords.x1;
    int32_t aH = coords.y2 - coords.y1;
    if (aW < 2 || aH < 2) return;

    float xMin, xMax, yMin, yMax;
    app->getViewport(xMin, xMax, yMin, yMax);
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    if (xRange <= 0 || yRange <= 0) return;

    auto toSX = [&](float wx) -> int32_t { return coords.x1 + (int32_t)((wx - xMin) / xRange * aW); };
    auto toSY = [&](float wy) -> int32_t { return coords.y1 + (int32_t)((1.0f - (wy - yMin) / yRange) * aH); };

    // ── Sub-grid lines ──
    float mainStep = niceStep(xRange, 8);
    float subStep  = mainStep / 5.0f;
    {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_hex(COL_GRID_SUB);
        dsc.opa   = LV_OPA_COVER;
        dsc.width = 1;

        // Vertical sub-grid
        float start = floorf(xMin / subStep) * subStep;
        for (float v = start; v <= xMax; v += subStep) {
            int32_t sx = toSX(v);
            if (sx < coords.x1 || sx > coords.x2) continue;
            dsc.p1.x = sx; dsc.p1.y = coords.y1;
            dsc.p2.x = sx; dsc.p2.y = coords.y2;
            lv_draw_line(layer, &dsc);
        }
        // Horizontal sub-grid
        float yMainStep = niceStep(yRange, 6);
        float ySubStep  = yMainStep / 5.0f;
        start = floorf(yMin / ySubStep) * ySubStep;
        for (float v = start; v <= yMax; v += ySubStep) {
            int32_t sy = toSY(v);
            if (sy < coords.y1 || sy > coords.y2) continue;
            dsc.p1.x = coords.x1; dsc.p1.y = sy;
            dsc.p2.x = coords.x2; dsc.p2.y = sy;
            lv_draw_line(layer, &dsc);
        }
    }

    // ── Main grid lines ──
    {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_hex(COL_GRID_MAIN);
        dsc.opa   = LV_OPA_COVER;
        dsc.width = 1;

        // Vertical main grid
        float start = floorf(xMin / mainStep) * mainStep;
        for (float v = start; v <= xMax; v += mainStep) {
            int32_t sx = toSX(v);
            if (sx < coords.x1 || sx > coords.x2) continue;
            dsc.p1.x = sx; dsc.p1.y = coords.y1;
            dsc.p2.x = sx; dsc.p2.y = coords.y2;
            lv_draw_line(layer, &dsc);
        }
        // Horizontal main grid
        float yMainStep = niceStep(yRange, 6);
        start = floorf(yMin / yMainStep) * yMainStep;
        for (float v = start; v <= yMax; v += yMainStep) {
            int32_t sy = toSY(v);
            if (sy < coords.y1 || sy > coords.y2) continue;
            dsc.p1.x = coords.x1; dsc.p1.y = sy;
            dsc.p2.x = coords.x2; dsc.p2.y = sy;
            lv_draw_line(layer, &dsc);
        }
    }

    // ── Axes (on top of grid) ──
    {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_hex(COL_AXIS);
        dsc.opa   = LV_OPA_COVER;
        dsc.width = 1;

        // X axis (y=0)
        int32_t ay = toSY(0);
        if (ay >= coords.y1 && ay <= coords.y2) {
            dsc.p1.x = coords.x1; dsc.p1.y = ay;
            dsc.p2.x = coords.x2; dsc.p2.y = ay;
            lv_draw_line(layer, &dsc);
        }
        // Y axis (x=0)
        int32_t ax = toSX(0);
        if (ax >= coords.x1 && ax <= coords.x2) {
            dsc.p1.x = ax; dsc.p1.y = coords.y1;
            dsc.p2.x = ax; dsc.p2.y = coords.y2;
            lv_draw_line(layer, &dsc);
        }
    }

    // ── Tick labels ──
    {
        lv_draw_label_dsc_t ldsc;
        lv_draw_label_dsc_init(&ldsc);
        ldsc.color = lv_color_hex(0x888888);
        ldsc.font  = &lv_font_montserrat_10;

        char buf[16];
        int32_t ay = toSY(0);  // Y axis screen position

        // X-axis tick labels
        float start = floorf(xMin / mainStep) * mainStep;
        for (float v = start; v <= xMax; v += mainStep) {
            if (fabsf(v) < mainStep * 0.01f) continue; // skip 0
            int32_t sx = toSX(v);
            if (sx < coords.x1 + 5 || sx > coords.x2 - 15) continue;

            if (fabsf(v - roundf(v)) < 0.001f)
                snprintf(buf, sizeof(buf), "%d", (int)roundf(v));
            else
                snprintf(buf, sizeof(buf), "%.1g", (double)v);

            int32_t ly = (ay >= coords.y1 && ay <= coords.y2 - 12) ? ay + 2 : coords.y2 - 12;
            lv_area_t la = { sx - 10, ly, sx + 20, ly + 12 };
            ldsc.text = buf;
            ldsc.text_local = 1;
            lv_draw_label(layer, &ldsc, &la);
        }

        // Y-axis tick labels
        float yMainStep = niceStep(yRange, 6);
        start = floorf(yMin / yMainStep) * yMainStep;
        for (float v = start; v <= yMax; v += yMainStep) {
            if (fabsf(v) < yMainStep * 0.01f) continue; // skip 0
            int32_t sy = toSY(v);
            if (sy < coords.y1 + 5 || sy > coords.y2 - 5) continue;

            if (fabsf(v - roundf(v)) < 0.001f)
                snprintf(buf, sizeof(buf), "%d", (int)roundf(v));
            else
                snprintf(buf, sizeof(buf), "%.1g", (double)v);

            int32_t ax = toSX(0);
            int32_t lx = (ax >= coords.x1 + 25 && ax <= coords.x2) ? ax - 24 : coords.x1 + 2;
            lv_area_t la = { lx, sy - 5, lx + 22, sy + 7 };
            ldsc.text = buf;
            ldsc.text_local = 1;
            lv_draw_label(layer, &ldsc, &la);
        }
    }
}

// ── Graph panel ─────────────────────────────────────────────────────────
void GrapherApp::createGraphPanel() {
    int topY = BAR_H + TAB_H;
    int panelH = SCREEN_H - topY;
    _panelGraph = makeContainer(_screen, 0, topY, SCREEN_W, panelH);
    lv_obj_add_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);

    // ── Toolbar at top of panel ──
    _graphToolbar = makeContainer(_panelGraph, 0, 0, SCREEN_W, TOOLBAR_H, COL_TB_BG);
    const char* tools[] = { "Auto", "Axes", "Navigate", "Calculate" };
    int tw = SCREEN_W / 4;
    for (int i = 0; i < 4; ++i) {
        _toolLabels[i] = makeLabel(_graphToolbar, i * tw + 4, (TOOLBAR_H - 12) / 2,
                                    tools[i], COL_TB_TXT, &lv_font_montserrat_12);
    }

    // ── Graph area (below toolbar, above info bar) ──
    int graphY = TOOLBAR_H;
    int graphH = panelH - TOOLBAR_H - INFO_BAR_H;
    _graphArea = makeContainer(_panelGraph, 0, graphY, SCREEN_W, graphH);
    lv_obj_set_style_bg_color(_graphArea, lv_color_hex(COL_GRAPH_BG), LV_PART_MAIN);

    // Grid + axes drawn via custom callback (uses viewport from GrapherApp)
    lv_obj_add_event_cb(_graphArea, graphGridDrawCb, LV_EVENT_DRAW_MAIN_BEGIN, this);

    // Hidden axis lv_line objects kept for compatibility (grid callback draws them)
    _axisLineX = lv_line_create(_graphArea);
    lv_obj_add_flag(_axisLineX, LV_OBJ_FLAG_HIDDEN);
    _axisLineY = lv_line_create(_graphArea);
    lv_obj_add_flag(_axisLineY, LV_OBJ_FLAG_HIDDEN);

    // Function lv_line objects — allocate point buffers in PSRAM (8MB free)
    for (int i = 0; i < MAX_FUNCS; ++i) {
        size_t ptsSz = GRAPH_MAX_PTS * sizeof(lv_point_precise_t);
        _funcPts[i] = (lv_point_precise_t*)heap_caps_malloc(ptsSz, MALLOC_CAP_SPIRAM);
        if (_funcPts[i]) {
            memset(_funcPts[i], 0, ptsSz);
        } else {
            Serial.printf("[GRAPHER] FAIL: PSRAM alloc funcPts[%d] (%u bytes)\n", i, (unsigned)ptsSz);
        }

        _funcLines[i] = lv_line_create(_graphArea);
        lv_obj_set_style_line_color(_funcLines[i], lv_color_hex(_funcs[i].color), LV_PART_MAIN);
        lv_obj_set_style_line_width(_funcLines[i], 2, LV_PART_MAIN);
        lv_obj_add_flag(_funcLines[i], LV_OBJ_FLAG_HIDDEN);

        // Set valid dummy points so lv_line never has stale internal data
        if (_funcPts[i]) {
            lv_line_set_points(_funcLines[i], _funcPts[i], 2);
        }
        _funcPtCount[i] = 0;
    }
    Serial.printf("[GRAPHER] funcPts allocated in PSRAM, free=%u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Trace cursor dot
    _traceDot = lv_obj_create(_graphArea);
    lv_obj_set_size(_traceDot, 8, 8);
    lv_obj_set_style_bg_color(_traceDot, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_traceDot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(_traceDot, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(_traceDot, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_traceDot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);

    // Crosshair lines (horizontal + vertical)
    _traceLineH = lv_line_create(_graphArea);
    lv_obj_set_style_line_color(_traceLineH, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_line_width(_traceLineH, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(_traceLineH, LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);
    _traceHPts[0] = {0, 0}; _traceHPts[1] = {0, 0};
    lv_line_set_points(_traceLineH, _traceHPts, 2);

    _traceLineV = lv_line_create(_graphArea);
    lv_obj_set_style_line_color(_traceLineV, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_line_width(_traceLineV, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(_traceLineV, LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);
    _traceVPts[0] = {0, 0}; _traceVPts[1] = {0, 0};
    lv_line_set_points(_traceLineV, _traceVPts, 2);

    // Floating bottom pill for trace info
    _tracePill = lv_obj_create(_graphArea);
    lv_obj_set_size(_tracePill, 200, 26);
    lv_obj_set_align(_tracePill, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_translate_y(_tracePill, -6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_tracePill, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tracePill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(_tracePill, 13, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tracePill, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_tracePill, lv_color_hex(0xD1D1D1), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_tracePill, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_spread(_tracePill, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(_tracePill, 30, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(_tracePill, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_left(_tracePill, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_tracePill, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_tracePill, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_tracePill, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tracePill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);

    // Color dot inside pill
    _tracePillDot = lv_obj_create(_tracePill);
    lv_obj_set_size(_tracePillDot, 10, 10);
    lv_obj_set_pos(_tracePillDot, 0, 7);
    lv_obj_set_style_bg_color(_tracePillDot, lv_color_hex(0xCC0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tracePillDot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(_tracePillDot, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tracePillDot, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tracePillDot, LV_OBJ_FLAG_SCROLLABLE);

    // Label inside pill
    _tracePillLabel = lv_label_create(_tracePill);
    lv_obj_set_pos(_tracePillLabel, 14, 5);
    lv_label_set_text(_tracePillLabel, "");
    lv_obj_set_style_text_color(_tracePillLabel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(_tracePillLabel, &lv_font_montserrat_12, LV_PART_MAIN);

    // ── Info bar at bottom ──
    _infoBar = makeContainer(_panelGraph, 0, panelH - INFO_BAR_H, SCREEN_W, INFO_BAR_H, COL_TB_BG);
    _infoLabel = makeLabel(_infoBar, PAD, 2, "", 0x000000, &lv_font_montserrat_12);
}

// ── Table panel ─────────────────────────────────────────────────────────
void GrapherApp::createTablePanel() {
    Serial.printf("[GRAPHER] tablePanel heap=%u\n", (unsigned)esp_get_free_heap_size());
    int topY = BAR_H + TAB_H;
    int panelH = SCREEN_H - topY;

    _panelTable = lv_obj_create(_screen);
    if (!_panelTable) { Serial.println("[GRAPHER] FAIL _panelTable"); return; }
    lv_obj_set_pos(_panelTable, 0, topY);
    lv_obj_set_size(_panelTable, SCREEN_W, panelH);
    lv_obj_set_style_bg_color(_panelTable, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_panelTable, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_panelTable, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_panelTable, 0, LV_PART_MAIN);
    // Scrollable — needed for 21+ data rows that exceed the panel height
    lv_obj_add_flag(_panelTable, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_panelTable, LV_DIR_VER);
    lv_obj_add_flag(_panelTable, LV_OBJ_FLAG_HIDDEN);

    // Native lv_table widget
    _tblTable = lv_table_create(_panelTable);
    if (!_tblTable) { Serial.println("[GRAPHER] FAIL _tblTable"); return; }
    lv_obj_set_pos(_tblTable, 0, 0);
    lv_obj_set_width(_tblTable, SCREEN_W);
    lv_table_set_column_count(_tblTable, 2);
    lv_table_set_column_width(_tblTable, 0, SCREEN_W / 2);
    lv_table_set_column_width(_tblTable, 1, SCREEN_W / 2);
    lv_table_set_row_count(_tblTable, 1);
    lv_table_set_cell_value(_tblTable, 0, 0, "x");
    lv_table_set_cell_value(_tblTable, 0, 1, "y");

    // Style: header row (row 0) — yellow NumWorks bg, black text
    lv_obj_set_style_bg_color(_tblTable, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_text_color(_tblTable, lv_color_hex(0x000000), LV_PART_ITEMS);
    lv_obj_set_style_text_font(_tblTable, &lv_font_montserrat_12, LV_PART_ITEMS);
    lv_obj_set_style_border_width(_tblTable, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(_tblTable, lv_color_hex(0xE0E0E0), LV_PART_ITEMS);
    lv_obj_set_style_pad_top(_tblTable, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(_tblTable, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(_tblTable, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(_tblTable, 6, LV_PART_ITEMS);

    // Remove main border around table
    lv_obj_set_style_border_width(_tblTable, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_tblTable, 0, LV_PART_MAIN);

    Serial.println("[GRAPHER] tablePanel Done");
}

// ═══════════════════════════════════════════════════════════════════════
// Tab switching
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::switchTab(Tab t) {
    _tab = t;
    _tabIdx = (int)t;

    // Show/hide panels
    if (_bgExpr)     lv_obj_add_flag(_bgExpr, LV_OBJ_FLAG_HIDDEN);
    if (_panelExpr)  lv_obj_add_flag(_panelExpr, LV_OBJ_FLAG_HIDDEN);
    if (_panelGraph) lv_obj_add_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
    if (_panelTable) lv_obj_add_flag(_panelTable, LV_OBJ_FLAG_HIDDEN);

    switch (t) {
    case Tab::EXPRESSIONS:
        lv_obj_remove_flag(_bgExpr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(_panelExpr, LV_OBJ_FLAG_HIDDEN);
        refreshExprList();
        break;
    case Tab::GRAPH:
        if (!_panelGraph) {
            Serial.println("[GRAPHER] graphPanel (lazy)...");
            createGraphPanel();
            Serial.println("[GRAPHER] graphPanel done");
            // First creation: don't replot yet — wait for layout pass
            if (_panelGraph) {
                lv_obj_remove_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
                _grMode = GrMode::NAVIGATE;  // Default to NAVIGATE for immediate pan/zoom
                _plotDirty = true;  // Will be picked up on next interaction
            }
        } else {
            lv_obj_remove_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
            _grMode = GrMode::NAVIGATE;
            _plotDirty = true;
            replot();
        }
        break;
    case Tab::TABLE:
        lv_obj_remove_flag(_panelTable, LV_OBJ_FLAG_HIDDEN);
        // Pick first valid function for table
        _tblFuncIdx = -1;
        for (int i = 0; i < _numFuncs; ++i) {
            if (_funcs[i].valid) { _tblFuncIdx = i; break; }
        }
        rebuildTable();
        break;
    }
    refreshTabBar();
}

void GrapherApp::refreshTabBar() {
    for (int i = 0; i < 3; ++i) {
        bool active = (i == _tabIdx);
        // Active tab pill: white bg, opaque. Inactive: transparent
        lv_obj_set_style_bg_opa(_tabPills[i],
            active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_tabPills[i],
            lv_color_hex(COL_TAB_ACT), LV_PART_MAIN);
        lv_obj_set_style_text_color(_tabLabels[i],
            lv_color_hex(active ? COL_TAB_TXT_A : COL_TAB_TXT_I), LV_PART_MAIN);
    }
    // Focus indicator: blue outline on focused tab pill
    if (_focus == Focus::TAB_BAR) {
        lv_obj_set_style_border_width(_tabPills[_tabIdx], 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(_tabPills[_tabIdx],
            lv_color_hex(COL_TB_SEL), LV_PART_MAIN);
    }
    for (int i = 0; i < 3; ++i) {
        if (i != _tabIdx || _focus != Focus::TAB_BAR) {
            lv_obj_set_style_border_width(_tabPills[i], 0, LV_PART_MAIN);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Expression list management
// ═══════════════════════════════════════════════════════════════════════

int GrapherApp::exprItemCount() const {
    // Items: _numFuncs rows + 1 Add + 2 buttons (Plot, Table)
    return _numFuncs + 3;
}

void GrapherApp::refreshExprList() {
    // Show/hide function rows and compute dynamic positions
    int curY = PAD;
    for (int i = 0; i < MAX_FUNCS; ++i) {
        if (i < _numFuncs) {
            lv_obj_remove_flag(_exprRows[i], LV_OBJ_FLAG_HIDDEN);
            // Refresh VPAM canvas rendering (also resizes canvas)
            refreshVPAMExpr(i);
            lv_obj_set_pos(_exprRows[i], PAD, curY);
            // Force layout update to get actual height
            lv_obj_update_layout(_exprRows[i]);
            int pillH = lv_obj_get_height(_exprRows[i]);
            if (pillH < ROW_H) pillH = ROW_H;
            curY += pillH + ROW_GAP;
        } else {
            lv_obj_add_flag(_exprRows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Reposition Add row below last function
    lv_obj_set_pos(_addRow, PAD, curY);

    // Reposition buttons
    int btnY = curY + ROW_H + ROW_GAP + 4;
    int halfW = (SCREEN_W - 3 * PAD) / 2;
    lv_obj_set_pos(_plotBtn, PAD, btnY);
    lv_obj_set_pos(_tableBtn, PAD + halfW + PAD, btnY);

    // Hide Add if max functions reached
    if (_numFuncs >= MAX_FUNCS) {
        lv_obj_add_flag(_addRow, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(_addRow, LV_OBJ_FLAG_HIDDEN);
    }

    refreshExprFocus();
    refreshTemplateButtons();
}

void GrapherApp::refreshExprFocus() {
    for (int i = 0; i < MAX_FUNCS; ++i) {
        if (i < _numFuncs) {
            lv_obj_set_style_bg_color(_exprRows[i], lv_color_hex(COL_ROW_BG), LV_PART_MAIN);
            lv_obj_set_style_border_color(_exprRows[i], lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
            lv_obj_set_style_border_width(_exprRows[i], 1, LV_PART_MAIN);
        }
    }
    lv_obj_set_style_bg_color(_addRow, lv_color_hex(0xF7F7F7), LV_PART_MAIN);
    lv_obj_set_style_border_color(_addRow, lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_plotBtn, lv_color_hex(COL_BTN_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_tableBtn, lv_color_hex(COL_BTN_BG), LV_PART_MAIN);

    if (_focus != Focus::CONTENT) return;

    // Selected item: blue bg + white text (NumWorks style)
    if (_exprIdx < _numFuncs) {
        lv_obj_set_style_bg_color(_exprRows[_exprIdx], lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
        lv_obj_set_style_border_color(_exprRows[_exprIdx], lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
        lv_obj_set_style_border_width(_exprRows[_exprIdx], 2, LV_PART_MAIN);
    } else if (_exprIdx == _numFuncs) {
        // Add row
        lv_obj_set_style_bg_color(_addRow, lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
        lv_obj_set_style_border_color(_addRow, lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
    } else if (_exprIdx == _numFuncs + 1) {
        lv_obj_set_style_bg_color(_plotBtn, lv_color_hex(0x3A7BD5), LV_PART_MAIN);
    } else if (_exprIdx == _numFuncs + 2) {
        lv_obj_set_style_bg_color(_tableBtn, lv_color_hex(0x3A7BD5), LV_PART_MAIN);
    }
}

void GrapherApp::addFunction() {
    if (_numFuncs >= MAX_FUNCS) return;
    int idx = _numFuncs;
    _funcs[idx].text[0] = '\0';
    _funcs[idx].len = 0;
    _funcs[idx].valid = false;
    _funcs[idx].color = FUNC_COLORS[idx];
    initSlotAST(idx);
    _numFuncs++;
    _exprIdx = idx;
    refreshExprList();
    startEditing(_exprIdx);
}

void GrapherApp::removeFunction(int idx) {
    if (idx < 0 || idx >= _numFuncs) return;

    // Destroy VPAM resources for this slot
    _exprCanvas[idx].stopCursorBlink();

    // Shift down data
    for (int i = idx; i < _numFuncs - 1; ++i) {
        memcpy(&_funcs[i], &_funcs[i + 1], sizeof(FuncSlot));
        _exprAST[i] = std::move(_exprAST[i + 1]);
        _exprASTRow[i] = _exprAST[i] ? static_cast<NodeRow*>(_exprAST[i].get()) : nullptr;
        _exprCursor[i] = _exprCursor[i + 1];
        // Rebind cursor to new AST owner
        if (_exprASTRow[i]) {
            _exprCursor[i].init(_exprASTRow[i]);
        }
        // Rebind canvas
        _exprCanvas[i].setExpression(_exprASTRow[i],
            (_exprMode == ExprMode::EDITING && _exprIdx == i) ? &_exprCursor[i] : nullptr);
    }
    // Clear last slot
    int last = _numFuncs - 1;
    _exprAST[last].reset();
    _exprASTRow[last] = nullptr;

    _numFuncs--;
    if (_exprIdx >= _numFuncs && _exprIdx > 0) _exprIdx--;
    _plotDirty = true;
    refreshExprList();
}

void GrapherApp::startEditing(int idx) {
    if (idx < 0 || idx >= _numFuncs) return;
    _exprMode = ExprMode::EDITING;
    _exprIdx = idx;

    // Initialize AST if not yet created
    if (!_exprASTRow[idx]) {
        initSlotAST(idx);
    }

    // Bind MathCanvas with cursor for active editing
    _exprCanvas[idx].setExpression(_exprASTRow[idx], &_exprCursor[idx]);
    _exprCanvas[idx].startCursorBlink();
    refreshVPAMExpr(idx);

    lv_label_set_text(_exprHint, "Type expression  ENTER=done  DEL=backspace");
    refreshExprFocus();
    refreshTemplateButtons();
}

void GrapherApp::stopEditing() {
    _exprMode = ExprMode::LIST;
    int idx = _exprIdx;
    if (idx >= 0 && idx < _numFuncs) {
        _exprCanvas[idx].stopCursorBlink();
        // Show expression without cursor
        _exprCanvas[idx].setExpression(_exprASTRow[idx], nullptr);
        refreshVPAMExpr(idx);

        // Sync AST → text for evaluation pipeline
        syncASTtoText(idx);

        // Validate expression
        if (_funcs[idx].len > 0) {
            TokenizeResult tr = _tokenizer.tokenize(_funcs[idx].text);
            if (tr.ok) {
                ParseResult pr = _parser.toRPN(tr.tokens);
                _funcs[idx].valid = pr.ok;
            } else {
                _funcs[idx].valid = false;
            }
        } else {
            _funcs[idx].valid = false;
        }
    }
    _plotDirty = true;
    lv_label_set_text(_exprHint, "ENTER=edit  AC=back");
    refreshExprList();
}

// ═══════════════════════════════════════════════════════════════════════
// VPAM helpers
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::initSlotAST(int idx) {
    _exprAST[idx] = vpam::makeRow();
    _exprASTRow[idx] = static_cast<NodeRow*>(_exprAST[idx].get());
    _exprASTRow[idx]->appendChild(vpam::makeEmpty());
    _exprCursor[idx].init(_exprASTRow[idx]);
    _exprCanvas[idx].setExpression(_exprASTRow[idx], nullptr);
}

void GrapherApp::refreshVPAMExpr(int idx) {
    if (idx < 0 || idx >= MAX_FUNCS || !_exprASTRow[idx]) return;
    _exprASTRow[idx]->calculateLayout(_exprCanvas[idx].normalMetrics());
    // Resize canvas to fit AST content so the pill stretches
    const auto& rootL = _exprASTRow[idx]->layout();
    int16_t astH = static_cast<int16_t>(rootL.ascent + rootL.descent + 2 * PILL_PAD); // padding top+bottom
    int16_t minH = ROW_H - 2 * PILL_PAD;
    if (astH < minH) astH = minH;
    lv_obj_set_height(_exprCanvas[idx].obj(), astH);
    // Force parent pill to recalculate its content-based height
    if (_exprRows[idx]) lv_obj_update_layout(_exprRows[idx]);
    _exprCanvas[idx].invalidate();
}

// ── AST → text serialization for the Tokenizer/Parser/Evaluator pipeline ──
static void serializeNode(const vpam::MathNode* node, char* buf, int& pos, int maxLen);

static void serializeRow(const vpam::NodeRow* row, char* buf, int& pos, int maxLen) {
    if (!row) return;
    for (int i = 0; i < row->childCount(); ++i) {
        serializeNode(row->child(i), buf, pos, maxLen);
    }
}

static void serializeNode(const vpam::MathNode* node, char* buf, int& pos, int maxLen) {
    if (!node || pos >= maxLen - 1) return;
    switch (node->type()) {
        case NodeType::Number: {
            auto* n = static_cast<const NodeNumber*>(node);
            for (char c : n->value()) {
                if (pos < maxLen - 1) buf[pos++] = c;
            }
            break;
        }
        case NodeType::Operator: {
            auto* op = static_cast<const NodeOperator*>(node);
            char c = '+';
            switch (op->op()) {
                case OpKind::Add: c = '+'; break;
                case OpKind::Sub: c = '-'; break;
                case OpKind::Mul: c = '*'; break;
            }
            if (pos < maxLen - 1) buf[pos++] = c;
            break;
        }
        case NodeType::Variable: {
            auto* v = static_cast<const NodeVariable*>(node);
            if (pos < maxLen - 1) buf[pos++] = v->name();
            break;
        }
        case NodeType::Constant: {
            auto* c = static_cast<const NodeConstant*>(node);
            if (c->constKind() == ConstKind::Pi) {
                // Use text representation for pi
                const char* s = "pi";
                for (int j = 0; s[j] && pos < maxLen - 1; ++j) buf[pos++] = s[j];
            } else {
                if (pos < maxLen - 1) buf[pos++] = 'e';
            }
            break;
        }
        case NodeType::Fraction: {
            auto* f = static_cast<const NodeFraction*>(node);
            if (pos < maxLen - 1) buf[pos++] = '(';
            if (f->numerator()) {
                if (f->numerator()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(f->numerator()), buf, pos, maxLen);
                else serializeNode(f->numerator(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            if (pos < maxLen - 1) buf[pos++] = '/';
            if (pos < maxLen - 1) buf[pos++] = '(';
            if (f->denominator()) {
                if (f->denominator()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(f->denominator()), buf, pos, maxLen);
                else serializeNode(f->denominator(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            break;
        }
        case NodeType::Power: {
            auto* p = static_cast<const NodePower*>(node);
            if (p->base()) {
                if (p->base()->type() == NodeType::Row) {
                    if (pos < maxLen - 1) buf[pos++] = '(';
                    serializeRow(static_cast<const NodeRow*>(p->base()), buf, pos, maxLen);
                    if (pos < maxLen - 1) buf[pos++] = ')';
                } else {
                    serializeNode(p->base(), buf, pos, maxLen);
                }
            }
            if (pos < maxLen - 1) buf[pos++] = '^';
            if (pos < maxLen - 1) buf[pos++] = '(';
            if (p->exponent()) {
                if (p->exponent()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(p->exponent()), buf, pos, maxLen);
                else serializeNode(p->exponent(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            break;
        }
        case NodeType::Root: {
            auto* r = static_cast<const NodeRoot*>(node);
            const char* s = "sqrt(";
            for (int j = 0; s[j] && pos < maxLen - 1; ++j) buf[pos++] = s[j];
            if (r->radicand()) {
                if (r->radicand()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(r->radicand()), buf, pos, maxLen);
                else serializeNode(r->radicand(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            break;
        }
        case NodeType::Paren: {
            auto* p = static_cast<const NodeParen*>(node);
            if (pos < maxLen - 1) buf[pos++] = '(';
            if (p->content()) {
                if (p->content()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(p->content()), buf, pos, maxLen);
                else serializeNode(p->content(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            break;
        }
        case NodeType::Function: {
            auto* fn = static_cast<const NodeFunction*>(node);
            const char* name = fn->label();
            for (int j = 0; name[j] && pos < maxLen - 1; ++j) buf[pos++] = name[j];
            if (pos < maxLen - 1) buf[pos++] = '(';
            if (fn->argument()) {
                if (fn->argument()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(fn->argument()), buf, pos, maxLen);
                else serializeNode(fn->argument(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            break;
        }
        case NodeType::LogBase: {
            auto* lb = static_cast<const NodeLogBase*>(node);
            const char* s = "log(";
            for (int j = 0; s[j] && pos < maxLen - 1; ++j) buf[pos++] = s[j];
            if (lb->argument()) {
                if (lb->argument()->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(lb->argument()), buf, pos, maxLen);
                else serializeNode(lb->argument(), buf, pos, maxLen);
            }
            if (pos < maxLen - 1) buf[pos++] = ')';
            break;
        }
        case NodeType::Row: {
            serializeRow(static_cast<const NodeRow*>(node), buf, pos, maxLen);
            break;
        }
        case NodeType::Empty:
        default:
            break;
    }
}

void GrapherApp::syncASTtoText(int idx) {
    if (idx < 0 || idx >= MAX_FUNCS) return;
    FuncSlot& f = _funcs[idx];
    int pos = 0;
    if (_exprASTRow[idx]) {
        serializeRow(_exprASTRow[idx], f.text, pos, 63);
    }
    f.text[pos] = '\0';
    f.len = pos;
}

vpam::NodePtr GrapherApp::buildTemplateAST(const char* text) {
    auto row = vpam::makeRow();
    auto* r = static_cast<NodeRow*>(row.get());

    for (int i = 0; text[i]; ++i) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            // Extend previous number or create new
            if (r->childCount() > 0) {
                auto* last = r->child(r->childCount() - 1);
                if (last && last->type() == NodeType::Number) {
                    static_cast<NodeNumber*>(last)->appendChar(c);
                    continue;
                }
            }
            r->appendChild(vpam::makeNumber(std::string(1, c)));
        } else if (c == '.') {
            if (r->childCount() > 0) {
                auto* last = r->child(r->childCount() - 1);
                if (last && last->type() == NodeType::Number) {
                    static_cast<NodeNumber*>(last)->appendChar(c);
                    continue;
                }
            }
            r->appendChild(vpam::makeNumber(std::string(1, c)));
        } else if (c == '+') {
            r->appendChild(vpam::makeOperator(OpKind::Add));
        } else if (c == '-') {
            r->appendChild(vpam::makeOperator(OpKind::Sub));
        } else if (c == '*') {
            r->appendChild(vpam::makeOperator(OpKind::Mul));
        } else if (c == 'x' || c == 'X') {
            r->appendChild(vpam::makeVariable('x'));
        } else if (c == '^') {
            // Simple power: take next char(s) as exponent
            auto expRow = vpam::makeRow();
            auto* er = static_cast<NodeRow*>(expRow.get());
            // Take the previous node as base if available
            NodePtr base;
            if (r->childCount() > 0) {
                base = r->removeChild(r->childCount() - 1);
            } else {
                base = vpam::makeEmpty();
            }
            // Gather exponent characters
            ++i;
            while (text[i] && text[i] != '+' && text[i] != '-' && text[i] != '*'
                   && text[i] != '/' && text[i] != ')') {
                if (text[i] >= '0' && text[i] <= '9') {
                    if (er->childCount() > 0) {
                        auto* last = er->child(er->childCount() - 1);
                        if (last && last->type() == NodeType::Number) {
                            static_cast<NodeNumber*>(last)->appendChar(text[i]);
                            ++i; continue;
                        }
                    }
                    er->appendChild(vpam::makeNumber(std::string(1, text[i])));
                } else if (text[i] == 'x') {
                    er->appendChild(vpam::makeVariable('x'));
                } else {
                    break;
                }
                ++i;
            }
            --i; // Back up for loop increment
            if (er->isEmpty()) er->appendChild(vpam::makeEmpty());
            // Wrap base in a row if needed
            auto baseRow = vpam::makeRow();
            static_cast<NodeRow*>(baseRow.get())->appendChild(std::move(base));
            r->appendChild(vpam::makePower(std::move(baseRow), std::move(expRow)));
        } else if (c == '/') {
            // Fraction: previous node as numerator, next as denominator
            NodePtr num;
            if (r->childCount() > 0) {
                num = r->removeChild(r->childCount() - 1);
            } else {
                num = vpam::makeEmpty();
            }
            auto numRow = vpam::makeRow();
            static_cast<NodeRow*>(numRow.get())->appendChild(std::move(num));
            auto denRow = vpam::makeRow();
            auto* dr = static_cast<NodeRow*>(denRow.get());
            ++i;
            while (text[i] && text[i] != '+' && text[i] != '-'
                   && text[i] != '*' && text[i] != ')') {
                if (text[i] >= '0' && text[i] <= '9') {
                    if (dr->childCount() > 0) {
                        auto* last = dr->child(dr->childCount() - 1);
                        if (last && last->type() == NodeType::Number) {
                            static_cast<NodeNumber*>(last)->appendChar(text[i]);
                            ++i; continue;
                        }
                    }
                    dr->appendChild(vpam::makeNumber(std::string(1, text[i])));
                } else if (text[i] == 'x') {
                    dr->appendChild(vpam::makeVariable('x'));
                } else {
                    break;
                }
                ++i;
            }
            --i;
            if (dr->isEmpty()) dr->appendChild(vpam::makeEmpty());
            r->appendChild(vpam::makeFraction(std::move(numRow), std::move(denRow)));
        } else if (c == 's' && text[i+1] == 'i' && text[i+2] == 'n' && text[i+3] == '(') {
            i += 3; // skip "in("
            auto argRow = vpam::makeRow();
            auto* ar = static_cast<NodeRow*>(argRow.get());
            ++i;
            int depth = 1;
            while (text[i] && depth > 0) {
                if (text[i] == '(') depth++;
                else if (text[i] == ')') { depth--; if (depth == 0) break; }
                if (text[i] >= '0' && text[i] <= '9') {
                    if (ar->childCount() > 0) {
                        auto* last = ar->child(ar->childCount() - 1);
                        if (last && last->type() == NodeType::Number) {
                            static_cast<NodeNumber*>(last)->appendChar(text[i]);
                            ++i; continue;
                        }
                    }
                    ar->appendChild(vpam::makeNumber(std::string(1, text[i])));
                } else if (text[i] == 'x') {
                    ar->appendChild(vpam::makeVariable('x'));
                }
                ++i;
            }
            if (ar->isEmpty()) ar->appendChild(vpam::makeVariable('x'));
            r->appendChild(vpam::makeFunction(FuncKind::Sin, std::move(argRow)));
        } else if (c == 'c' && text[i+1] == 'o' && text[i+2] == 's' && text[i+3] == '(') {
            i += 3;
            auto argRow = vpam::makeRow();
            auto* ar = static_cast<NodeRow*>(argRow.get());
            ++i;
            int depth = 1;
            while (text[i] && depth > 0) {
                if (text[i] == '(') depth++;
                else if (text[i] == ')') { depth--; if (depth == 0) break; }
                if (text[i] >= '0' && text[i] <= '9') {
                    ar->appendChild(vpam::makeNumber(std::string(1, text[i])));
                } else if (text[i] == 'x') {
                    ar->appendChild(vpam::makeVariable('x'));
                }
                ++i;
            }
            if (ar->isEmpty()) ar->appendChild(vpam::makeVariable('x'));
            r->appendChild(vpam::makeFunction(FuncKind::Cos, std::move(argRow)));
        } else if (c == '(' || c == ')') {
            // Skip bare parentheses in template text
        }
    }

    if (r->isEmpty()) r->appendChild(vpam::makeEmpty());
    return row;
}

// ═══════════════════════════════════════════════════════════════════════
// Templates
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::refreshTemplateButtons() {
    for (int i = 0; i < MAX_FUNCS; ++i) {
        if (!_tplBtns[i]) continue;
        // Show Templates button only for visible rows with empty AST
        bool isEmpty = false;
        if (i < _numFuncs && _exprASTRow[i]) {
            isEmpty = (_exprASTRow[i]->childCount() == 1
                    && _exprASTRow[i]->child(0)->type() == NodeType::Empty);
        } else if (i < _numFuncs && !_exprASTRow[i]) {
            isEmpty = true;
        }
        if (isEmpty) {
            lv_obj_remove_flag(_tplBtns[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_tplBtns[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void GrapherApp::showTemplates() {
    if (_tplModal) return;  // Already open

    // Semi-transparent overlay
    _tplModal = lv_obj_create(_screen);
    lv_obj_set_size(_tplModal, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_tplModal, 0, 0);
    lv_obj_set_style_bg_color(_tplModal, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tplModal, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tplModal, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_tplModal, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tplModal, LV_OBJ_FLAG_SCROLLABLE);

    // Card in center
    int cardW = 260, cardH = 180;
    int cx = (SCREEN_W - cardW) / 2, cy = (SCREEN_H - cardH) / 2;
    lv_obj_t* card = makeContainer(_tplModal, cx, cy, cardW, cardH, COL_BG);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);

    // Title
    makeLabel(card, cardW / 2 - 40, 6, "Templates", 0x333333, &lv_font_montserrat_14);

    // Create template row containers (UI shells) immediately
    _tplCount = NUM_TEMPLATES;
    int rowH = 22, startY = 30;
    _tplCardW = cardW;
    _tplRowH = rowH;
    for (int i = 0; i < _tplCount && i < 6; ++i) {
        _tplRows[i] = makeContainer(card, 8, startY + i * (rowH + 2),
                                     cardW - 16, rowH, COL_BG);
        lv_obj_set_style_radius(_tplRows[i], 6, LV_PART_MAIN);
    }

    _tplIdx = 0;
    _tplOpen = true;

    // Highlight first row
    if (_tplCount > 0) {
        lv_obj_set_style_bg_color(_tplRows[0], lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tplRows[0], LV_OPA_COVER, LV_PART_MAIN);
    }

    // Lazy load: start a timer that loads one template AST per tick
    _tplLoadNext = 0;
    _tplLoadTimer = lv_timer_create(tplLoadTimerCb, TPL_LOAD_INTERVAL_MS, this);
}

void GrapherApp::tplLoadTimerCb(lv_timer_t* t) {
    auto* self = static_cast<GrapherApp*>(lv_timer_get_user_data(t));
    self->loadNextTemplate();
}

void GrapherApp::loadNextTemplate() {
    if (_tplLoadNext >= _tplCount || _tplLoadNext >= 6 || !_tplOpen) {
        // All templates loaded or modal closed — stop timer
        if (_tplLoadTimer) {
            lv_timer_delete(_tplLoadTimer);
            _tplLoadTimer = nullptr;
        }
        return;
    }

    int i = _tplLoadNext;

    // Build AST for template preview
    _tplAST[i] = buildTemplateAST(TEMPLATES[i].text);
    auto* tplRow = static_cast<NodeRow*>(_tplAST[i].get());

    // Create VPAM canvas for live preview (read-only, no cursor)
    _tplCanvas[i].create(_tplRows[i]);
    lv_obj_set_size(_tplCanvas[i].obj(), _tplCardW - 32, _tplRowH - 2);
    lv_obj_set_pos(_tplCanvas[i].obj(), 4, 1);
    _tplCanvas[i].setExpression(tplRow, nullptr);
    tplRow->calculateLayout(_tplCanvas[i].normalMetrics());
    _tplCanvas[i].invalidate();

    _tplLoadNext++;
}

void GrapherApp::closeTemplates() {
    // Stop lazy loading timer if still running
    if (_tplLoadTimer) {
        lv_timer_delete(_tplLoadTimer);
        _tplLoadTimer = nullptr;
    }
    // Destroy VPAM canvases before deleting modal
    for (int i = 0; i < 6; ++i) {
        _tplCanvas[i].destroy();
        _tplAST[i].reset();
    }
    if (_tplModal) {
        lv_obj_delete(_tplModal);
        _tplModal = nullptr;
    }
    _tplOpen = false;
    for (int i = 0; i < 6; ++i) { _tplRows[i] = nullptr; }
}

void GrapherApp::handleTemplates(const KeyEvent& ev) {
    switch (ev.code) {
    case KeyCode::UP:
        if (_tplIdx > 0) {
            lv_obj_set_style_bg_color(_tplRows[_tplIdx], lv_color_hex(COL_BG), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tplRows[_tplIdx], LV_OPA_COVER, LV_PART_MAIN);
            _tplIdx--;
            lv_obj_set_style_bg_color(_tplRows[_tplIdx], lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tplRows[_tplIdx], LV_OPA_COVER, LV_PART_MAIN);
        }
        break;
    case KeyCode::DOWN:
        if (_tplIdx < _tplCount - 1) {
            lv_obj_set_style_bg_color(_tplRows[_tplIdx], lv_color_hex(COL_BG), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tplRows[_tplIdx], LV_OPA_COVER, LV_PART_MAIN);
            _tplIdx++;
            lv_obj_set_style_bg_color(_tplRows[_tplIdx], lv_color_hex(COL_ROW_SEL), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tplRows[_tplIdx], LV_OPA_COVER, LV_PART_MAIN);
        }
        break;
    case KeyCode::ENTER: {
        // Insert template: build AST from template text into the slot
        int idx = _exprIdx;
        if (idx >= 0 && idx < _numFuncs && _tplIdx >= 0 && _tplIdx < _tplCount) {
            const char* src = TEMPLATES[_tplIdx].text;
            // Build real VPAM AST from template
            _exprAST[idx] = buildTemplateAST(src);
            _exprASTRow[idx] = static_cast<NodeRow*>(_exprAST[idx].get());
            _exprCursor[idx].init(_exprASTRow[idx]);
            _exprCanvas[idx].setExpression(_exprASTRow[idx], nullptr);
            refreshVPAMExpr(idx);
            // Sync to text for evaluation
            syncASTtoText(idx);
        }
        closeTemplates();
        stopEditing();
        break;
    }
    case KeyCode::AC:
        closeTemplates();
        break;
    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Graph plotting
// ═══════════════════════════════════════════════════════════════════════

double GrapherApp::evalAt(int idx, double x) {
    if (idx < 0 || idx >= _numFuncs || !_funcs[idx].valid) return NAN;
    _vars.setVar('x', x);
    TokenizeResult tr = _tokenizer.tokenize(_funcs[idx].text);
    if (!tr.ok) return NAN;
    ParseResult pr = _parser.toRPN(tr.tokens);
    if (!pr.ok) return NAN;
    EvalResult er = _evaluator.evaluateRPN(pr.outputRPN, _vars);
    return er.ok ? er.value : NAN;
}

void GrapherApp::replot() {
    if (!_plotDirty || !_graphArea) return;
    _plotDirty = false;

    int areaW = SCREEN_W;
    int areaH = lv_obj_get_height(_graphArea);
    if (areaH < 2) return;  // Layout not yet computed — skip

    float xRange = _xMax - _xMin;
    float yRange = _yMax - _yMin;

    auto worldToX = [&](float wx) -> float { return (wx - _xMin) / xRange * areaW; };
    auto worldToY = [&](float wy) -> float { return (1.0f - (wy - _yMin) / yRange) * areaH; };

    // Force redraw of grid + axes via the custom draw callback
    lv_obj_invalidate(_graphArea);

    // ── Plot each function ──
    for (int f = 0; f < MAX_FUNCS; ++f) {
        // Skip if no line object or no point buffer
        if (!_funcLines[f] || !_funcPts[f]) continue;

        if (f >= _numFuncs || !_funcs[f].valid) {
            lv_obj_add_flag(_funcLines[f], LV_OBJ_FLAG_HIDDEN);
            _funcPtCount[f] = 0;
            continue;
        }
        lv_obj_remove_flag(_funcLines[f], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_line_color(_funcLines[f], lv_color_hex(_funcs[f].color), LV_PART_MAIN);

        int count = 0;
        float prevY = NAN;
        for (int px = 0; px < areaW && count < GRAPH_MAX_PTS; ++px) {
            if ((px & 63) == 0) yield();  // Feed watchdog
            float wx = _xMin + (float)px / areaW * xRange;
            double wy = evalAt(f, wx);
            if (std::isnan(wy) || std::isinf(wy)) {
                prevY = NAN;
                continue;
            }
            float sy = worldToY((float)wy);
            // Clip Y to graph area with margin
            if (sy < -areaH || sy > 2 * areaH) {
                prevY = NAN;
                continue;
            }
            // Discontinuity check: huge jump → skip segment
            if (std::isnan(prevY) || fabsf(sy - prevY) > areaH) {
                // Start new segment — still add point so line resumes
            }
            _funcPts[f][count].x = px;
            _funcPts[f][count].y = (int)sy;
            count++;
            prevY = sy;
        }
        _funcPtCount[f] = count;
        if (_funcPts[f] && count >= 2) {
            lv_line_set_points(_funcLines[f], _funcPts[f], count);
        } else {
            lv_obj_add_flag(_funcLines[f], LV_OBJ_FLAG_HIDDEN);
        }
    }

    updateInfoBar();
}

void GrapherApp::drawTraceCursor() {
    if (_grMode != GrMode::TRACE || _traceFn < 0 || _traceFn >= _numFuncs ||
        !_funcs[_traceFn].valid) {
        if (_traceDot)   lv_obj_add_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineH) lv_obj_add_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineV) lv_obj_add_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);
        if (_tracePill)  lv_obj_add_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int areaW = SCREEN_W;
    int areaH = lv_obj_get_height(_graphArea);
    float xRange = _xMax - _xMin;
    float yRange = _yMax - _yMin;

    double wy = evalAt(_traceFn, _traceX);
    if (std::isnan(wy) || std::isinf(wy)) {
        if (_traceDot)   lv_obj_add_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineH) lv_obj_add_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineV) lv_obj_add_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);
        if (_tracePill)  lv_obj_add_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    float sx = (_traceX - _xMin) / xRange * areaW;
    float sy = (1.0f - ((float)wy - _yMin) / yRange) * areaH;

    // Trace dot
    lv_obj_remove_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(_traceDot, (int)sx - 4, (int)sy - 4);
    lv_obj_set_style_bg_color(_traceDot, lv_color_hex(_funcs[_traceFn].color), LV_PART_MAIN);

    // Crosshair horizontal line
    _traceHPts[0] = {0, (lv_value_precise_t)(int)sy};
    _traceHPts[1] = {(lv_value_precise_t)areaW, (lv_value_precise_t)(int)sy};
    lv_line_set_points(_traceLineH, _traceHPts, 2);
    lv_obj_remove_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);

    // Crosshair vertical line
    _traceVPts[0] = {(lv_value_precise_t)(int)sx, 0};
    _traceVPts[1] = {(lv_value_precise_t)(int)sx, (lv_value_precise_t)areaH};
    lv_line_set_points(_traceLineV, _traceVPts, 2);
    lv_obj_remove_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);

    // Floating pill — show function color + x/y values
    lv_obj_remove_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(_tracePillDot, lv_color_hex(_funcs[_traceFn].color), LV_PART_MAIN);
    char pillBuf[64];
    snprintf(pillBuf, sizeof(pillBuf), "x: %.3f   y: %.3f", (double)_traceX, wy);
    lv_label_set_text(_tracePillLabel, pillBuf);
}

void GrapherApp::updateInfoBar() {
    if (!_infoLabel) return;
    char buf[64];
    if (_grMode == GrMode::TRACE && _traceFn >= 0 && _traceFn < _numFuncs) {
        double y = evalAt(_traceFn, _traceX);
        snprintf(buf, sizeof(buf), "Trace: x=%.3f  y=%.3f", (double)_traceX, y);
    } else {
        snprintf(buf, sizeof(buf), "x:[%.2g,%.2g] y:[%.2g,%.2g]  ENTER=trace",
                 (double)_xMin, (double)_xMax, (double)_yMin, (double)_yMax);
    }
    lv_label_set_text(_infoLabel, buf);
}

void GrapherApp::autoFit() {
    _xMin = -10; _xMax = 10;
    _yMin = -7;  _yMax = 7;
    // Try to fit based on active functions
    float globalYMin = 1e9f, globalYMax = -1e9f;
    bool hasData = false;
    for (int f = 0; f < _numFuncs; ++f) {
        if (!_funcs[f].valid) continue;
        for (int px = 0; px < SCREEN_W; px += 4) {
            float wx = _xMin + (float)px / SCREEN_W * (_xMax - _xMin);
            double wy = evalAt(f, wx);
            if (!std::isnan(wy) && !std::isinf(wy) && fabs(wy) < 1e6) {
                if ((float)wy < globalYMin) globalYMin = (float)wy;
                if ((float)wy > globalYMax) globalYMax = (float)wy;
                hasData = true;
            }
        }
    }
    if (hasData) {
        float margin = (globalYMax - globalYMin) * 0.15f;
        if (margin < 1.0f) margin = 1.0f;
        _yMin = globalYMin - margin;
        _yMax = globalYMax + margin;
    }
    _plotDirty = true;
}

void GrapherApp::refreshToolbar() {
    if (!_graphToolbar) return;
    for (int i = 0; i < 4; ++i) {
        bool sel = (_focus == Focus::TOOLBAR && i == _toolIdx);
        lv_obj_set_style_text_color(_toolLabels[i],
            lv_color_hex(sel ? COL_BTN_TXT : COL_TB_TXT), LV_PART_MAIN);
    }
    // Background highlight for selected tool
    lv_obj_set_style_bg_color(_graphToolbar,
        lv_color_hex(_focus == Focus::TOOLBAR ? 0xD0D8E0 : COL_TB_BG), LV_PART_MAIN);
}

// ═══════════════════════════════════════════════════════════════════════
// Table
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::rebuildTable() {
    if (!_tblTable) return;

    // Count active (valid) functions
    int activeFuncs[MAX_FUNCS];
    int numActive = 0;
    for (int i = 0; i < _numFuncs; ++i) {
        if (_funcs[i].valid) {
            activeFuncs[numActive++] = i;
        }
    }

    // Determine column count: 1 (x) + numActive (f1, f2, ...)
    int cols = 1 + (numActive > 0 ? numActive : 1);
    if (cols > TBL_COLS) cols = TBL_COLS;
    lv_table_set_column_count(_tblTable, cols);

    // Distribute column widths
    int colW = SCREEN_W / cols;
    for (int c = 0; c < cols; ++c) {
        lv_table_set_column_width(_tblTable, c, colW);
    }

    // Row count: 1 header + TBL_ROWS data rows
    int dataRows = TBL_ROWS;
    lv_table_set_row_count(_tblTable, 1 + dataRows);

    // Header row
    lv_table_set_cell_value(_tblTable, 0, 0, "x");
    if (numActive > 0) {
        char hdr[16];
        for (int c = 0; c < numActive && c + 1 < cols; ++c) {
            snprintf(hdr, sizeof(hdr), "f%d(x)", activeFuncs[c] + 1);
            lv_table_set_cell_value(_tblTable, 0, c + 1, hdr);
        }
    } else {
        lv_table_set_cell_value(_tblTable, 0, 1, "y");
    }

    // Data rows
    for (int r = 0; r < dataRows; ++r) {
        if ((r & 15) == 0) yield();  // Feed watchdog

        float xVal = _tblStart + (_tblRow + r) * _tblStep;

        // X column
        char xBuf[16];
        snprintf(xBuf, sizeof(xBuf), "%.4g", (double)xVal);
        lv_table_set_cell_value(_tblTable, r + 1, 0, xBuf);

        // Function value columns
        if (numActive > 0) {
            for (int c = 0; c < numActive && c + 1 < cols; ++c) {
                double yVal = evalAt(activeFuncs[c], xVal);
                char yBuf[16];
                if (std::isnan(yVal) || std::isinf(yVal))
                    snprintf(yBuf, sizeof(yBuf), "--");
                else
                    snprintf(yBuf, sizeof(yBuf), "%.6g", yVal);
                lv_table_set_cell_value(_tblTable, r + 1, c + 1, yBuf);
            }
        } else {
            lv_table_set_cell_value(_tblTable, r + 1, 1, "--");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Key handling — main dispatcher
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // Templates modal intercept — all keys go to modal when open
    if (_tplOpen) {
        handleTemplates(ev);
        return;
    }

    // Calculate menu intercept — all keys go to menu when open
    if (_calcMenuOpen) {
        handleCalcMenu(ev);
        return;
    }

    // Global shortcuts: GRAPH → Graph tab, TABLE → Table tab
    if (ev.code == KeyCode::GRAPH && _tab != Tab::GRAPH) {
        _focus = Focus::CONTENT;
        switchTab(Tab::GRAPH);
        return;
    }
    if (ev.code == KeyCode::TABLE && _tab != Tab::TABLE) {
        _focus = Focus::CONTENT;
        switchTab(Tab::TABLE);
        return;
    }

    // Focus-based dispatch
    switch (_focus) {
    case Focus::TAB_BAR:
        handleTabBar(ev);
        break;
    case Focus::TOOLBAR:
        handleToolbar(ev);
        break;
    case Focus::CONTENT:
        switch (_tab) {
        case Tab::EXPRESSIONS:
            if (_exprMode == ExprMode::EDITING) handleExprEdit(ev);
            else handleExprList(ev);
            break;
        case Tab::GRAPH:
            if (_grMode == GrMode::TRACE) handleGraphTrace(ev);
            else handleGraphNav(ev);  // IDLE + NAVIGATE both use pan/zoom
            break;
        case Tab::TABLE:
            handleTable(ev);
            break;
        }
        break;
    }
}

// ── Tab bar keys ────────────────────────────────────────────────────────
void GrapherApp::handleTabBar(const KeyEvent& ev) {
    switch (ev.code) {
    case KeyCode::LEFT:
        if (_tabIdx > 0) { _tabIdx--; switchTab((Tab)_tabIdx); }
        break;
    case KeyCode::RIGHT:
        if (_tabIdx < 2) { _tabIdx++; switchTab((Tab)_tabIdx); }
        break;
    case KeyCode::DOWN:
    case KeyCode::ENTER:
        // Move focus into content (or toolbar for graph tab)
        if (_tab == Tab::GRAPH) {
            _focus = Focus::TOOLBAR;
            _toolIdx = 0;
            refreshToolbar();
        } else {
            _focus = Focus::CONTENT;
            if (_tab == Tab::EXPRESSIONS) refreshExprFocus();
        }
        refreshTabBar();
        break;
    case KeyCode::AC:
        // If not on Expressions, go to Expressions first
        if (_tab != Tab::EXPRESSIONS) {
            _tabIdx = 0;
            switchTab(Tab::EXPRESSIONS);
        }
        // If already on Expressions, SystemApp handles exit via atTabLevel()+isOnExpressions()
        break;
    default:
        break;
    }
}

// ── Expression list keys ────────────────────────────────────────────────
void GrapherApp::handleExprList(const KeyEvent& ev) {
    int maxIdx = exprItemCount() - 1;
    switch (ev.code) {
    case KeyCode::UP:
        if (_exprIdx > 0) _exprIdx--;
        else { _focus = Focus::TAB_BAR; refreshTabBar(); }
        refreshExprFocus();
        break;
    case KeyCode::DOWN:
        if (_exprIdx < maxIdx) _exprIdx++;
        refreshExprFocus();
        break;
    case KeyCode::ENTER:
        if (_exprIdx < _numFuncs) {
            startEditing(_exprIdx);
        } else if (_exprIdx == _numFuncs) {
            addFunction();
        } else if (_exprIdx == _numFuncs + 1) {
            // Plot graph
            _focus = Focus::CONTENT;
            switchTab(Tab::GRAPH);
        } else if (_exprIdx == _numFuncs + 2) {
            // Display values
            _focus = Focus::CONTENT;
            switchTab(Tab::TABLE);
        }
        break;
    case KeyCode::DEL:
        if (_exprIdx < _numFuncs) {
            removeFunction(_exprIdx);
        }
        break;
    case KeyCode::AC:
        _focus = Focus::TAB_BAR;
        refreshTabBar();
        refreshExprFocus();
        break;
    default:
        break;
    }
}

// ── Expression edit keys (VPAM CursorController) ────────────────────────
void GrapherApp::handleExprEdit(const KeyEvent& ev) {
    int idx = _exprIdx;
    if (idx < 0 || idx >= _numFuncs || !_exprASTRow[idx]) return;

    auto& cur = _exprCursor[idx];

    if (ev.code == KeyCode::ENTER) {
        stopEditing();
        return;
    }
    if (ev.code == KeyCode::AC) {
        // Clear entire expression: reset AST
        initSlotAST(idx);
        _exprCanvas[idx].setExpression(_exprASTRow[idx], &_exprCursor[idx]);
        _exprCanvas[idx].startCursorBlink();
        refreshVPAMExpr(idx);
        refreshTemplateButtons();
        return;
    }

    // RIGHT key when expression is empty → open Templates
    if (ev.code == KeyCode::RIGHT) {
        // Check if AST is empty (only contains a single NodeEmpty)
        bool isEmpty = (_exprASTRow[idx]->childCount() == 1
                     && _exprASTRow[idx]->child(0)->type() == NodeType::Empty);
        if (isEmpty) {
            showTemplates();
            return;
        }
    }

    bool changed = true;

    // Compute current nesting depth to prevent excessively deep ASTs
    auto cursorDepth = [&]() -> int {
        int d = 0;
        const vpam::MathNode* n = cur.cursor().row;
        while (n && n->parent()) { n = n->parent(); ++d; }
        return d;
    };
    static constexpr int MAX_NEST = 8;

    switch (ev.code) {
        // ── Digits ──
        case KeyCode::NUM_0: cur.insertDigit('0'); break;
        case KeyCode::NUM_1: cur.insertDigit('1'); break;
        case KeyCode::NUM_2: cur.insertDigit('2'); break;
        case KeyCode::NUM_3: cur.insertDigit('3'); break;
        case KeyCode::NUM_4: cur.insertDigit('4'); break;
        case KeyCode::NUM_5: cur.insertDigit('5'); break;
        case KeyCode::NUM_6: cur.insertDigit('6'); break;
        case KeyCode::NUM_7: cur.insertDigit('7'); break;
        case KeyCode::NUM_8: cur.insertDigit('8'); break;
        case KeyCode::NUM_9: cur.insertDigit('9'); break;
        case KeyCode::DOT:   cur.insertDigit('.'); break;

        // ── Operators ──
        case KeyCode::ADD: cur.insertOperator(OpKind::Add); break;
        case KeyCode::SUB: cur.insertOperator(OpKind::Sub); break;
        case KeyCode::MUL: cur.insertOperator(OpKind::Mul); break;
        case KeyCode::NEG: cur.insertOperator(OpKind::Sub); break;

        // ── VPAM structures (depth-limited) ──
        case KeyCode::DIV:    if (cursorDepth() < MAX_NEST) cur.insertFraction(); else changed = false; break;
        case KeyCode::POW:    if (cursorDepth() < MAX_NEST) cur.insertPower();    else changed = false; break;
        case KeyCode::SQRT:   if (cursorDepth() < MAX_NEST) cur.insertRoot();     else changed = false; break;
        case KeyCode::LPAREN: if (cursorDepth() < MAX_NEST) cur.insertParen();    else changed = false; break;

        // ── Functions (depth-limited) ──
        case KeyCode::SIN: if (cursorDepth() < MAX_NEST) cur.insertFunction(FuncKind::Sin); else changed = false; break;
        case KeyCode::COS: if (cursorDepth() < MAX_NEST) cur.insertFunction(FuncKind::Cos); else changed = false; break;
        case KeyCode::TAN: if (cursorDepth() < MAX_NEST) cur.insertFunction(FuncKind::Tan); else changed = false; break;
        case KeyCode::LN:  if (cursorDepth() < MAX_NEST) cur.insertFunction(FuncKind::Ln);  else changed = false; break;
        case KeyCode::LOG: if (cursorDepth() < MAX_NEST) cur.insertFunction(FuncKind::Log); else changed = false; break;

        // ── Variables ──
        case KeyCode::VAR_X: cur.insertVariable('x'); break;

        // ── Constants ──
        case KeyCode::CONST_PI: cur.insertConstant(ConstKind::Pi); break;
        case KeyCode::CONST_E:  cur.insertConstant(ConstKind::E);  break;

        // ── Navigation ──
        case KeyCode::LEFT:  cur.moveLeft();  break;
        case KeyCode::RIGHT: cur.moveRight(); break;
        case KeyCode::UP:    cur.moveUp();    break;
        case KeyCode::DOWN:  cur.moveDown();  break;

        // ── Backspace ──
        case KeyCode::DEL: cur.backspace(); break;

        default:
            changed = false;
            break;
    }

    if (changed) {
        _exprCanvas[idx].resetCursorBlink();
        refreshVPAMExpr(idx);
        refreshTemplateButtons();
    }
}

// ── Toolbar keys ────────────────────────────────────────────────────────
void GrapherApp::handleToolbar(const KeyEvent& ev) {
    switch (ev.code) {
    case KeyCode::LEFT:
        if (_toolIdx > 0) _toolIdx--;
        refreshToolbar();
        break;
    case KeyCode::RIGHT:
        if (_toolIdx < 3) _toolIdx++;
        refreshToolbar();
        break;
    case KeyCode::UP:
    case KeyCode::AC:
        _focus = Focus::TAB_BAR;
        refreshTabBar();
        refreshToolbar();
        break;
    case KeyCode::DOWN:
    case KeyCode::ENTER:
        // Activate selected tool
        switch ((int)_toolIdx) {
        case 0: // Auto
            autoFit();
            replot();
            break;
        case 1: // Axes — toggle grid/axes visibility (simple toggle)
            break;
        case 2: // Navigate
            _focus = Focus::CONTENT;
            _grMode = GrMode::NAVIGATE;
            _plotDirty = true;
            replot();
            updateInfoBar();
            break;
        case 3: // Calculate (Trace)
            _focus = Focus::CONTENT;
            _grMode = GrMode::TRACE;
            _traceX = (_xMin + _xMax) / 2.0f;
            // Pick first valid function
            _traceFn = -1;
            for (int i = 0; i < _numFuncs; ++i) {
                if (_funcs[i].valid) { _traceFn = i; break; }
            }
            drawTraceCursor();
            updateInfoBar();
            break;
        }
        break;
    default:
        break;
    }
}

// ── Graph navigate keys ─────────────────────────────────────────────────
void GrapherApp::handleGraphNav(const KeyEvent& ev) {
    float dx = (_xMax - _xMin) * 0.1f;
    float dy = (_yMax - _yMin) * 0.1f;

    switch (ev.code) {
    case KeyCode::LEFT:   _xMin -= dx; _xMax -= dx; _plotDirty = true; break;
    case KeyCode::RIGHT:  _xMin += dx; _xMax += dx; _plotDirty = true; break;
    case KeyCode::UP:     _yMin += dy; _yMax += dy; _plotDirty = true; break;
    case KeyCode::DOWN:   _yMin -= dy; _yMax -= dy; _plotDirty = true; break;
    case KeyCode::ADD:
    case KeyCode::ZOOM: {
        // Zoom in by factor 1.5: new_range = range / 1.5 → half = range / 3
        float cx = (_xMin + _xMax) / 2.0f, cy = (_yMin + _yMax) / 2.0f;
        float sx = (_xMax - _xMin) / 3.0f, sy = (_yMax - _yMin) / 3.0f;
        _xMin = cx - sx; _xMax = cx + sx;
        _yMin = cy - sy; _yMax = cy + sy;
        _plotDirty = true;
        break;
    }
    case KeyCode::SUB: {
        // Zoom out by factor 1.5: new_range = range * 1.5 → half = range * 0.75
        float cx = (_xMin + _xMax) / 2.0f, cy = (_yMin + _yMax) / 2.0f;
        float sx = (_xMax - _xMin) * 0.75f, sy = (_yMax - _yMin) * 0.75f;
        _xMin = cx - sx; _xMax = cx + sx;
        _yMin = cy - sy; _yMax = cy + sy;
        _plotDirty = true;
        break;
    }
    case KeyCode::ENTER: {
        // Toggle Trace Mode
        _grMode = GrMode::TRACE;
        _traceX = (_xMin + _xMax) / 2.0f;
        _traceFn = -1;
        for (int i = 0; i < _numFuncs; ++i) {
            if (_funcs[i].valid) { _traceFn = i; break; }
        }
        drawTraceCursor();
        updateInfoBar();
        return;
    }
    case KeyCode::AC:
        _grMode = GrMode::IDLE;
        _focus = Focus::TOOLBAR;
        refreshToolbar();
        updateInfoBar();
        break;
    default:
        break;
    }
    if (_plotDirty) replot();
}

// ── Graph trace keys ────────────────────────────────────────────────────
void GrapherApp::handleGraphTrace(const KeyEvent& ev) {
    float step = (_xMax - _xMin) / SCREEN_W;  // Pixel-precise cursor movement

    switch (ev.code) {
    case KeyCode::LEFT:
        _traceX -= step;
        if (_traceX < _xMin) _traceX = _xMin;
        break;
    case KeyCode::RIGHT:
        _traceX += step;
        if (_traceX > _xMax) _traceX = _xMax;
        break;
    case KeyCode::UP:
        // Switch to next function (keep same X)
        for (int i = _traceFn + 1; i < _numFuncs; ++i) {
            if (_funcs[i].valid) { _traceFn = i; break; }
        }
        break;
    case KeyCode::DOWN:
        // Switch to prev function (keep same X)
        for (int i = _traceFn - 1; i >= 0; --i) {
            if (_funcs[i].valid) { _traceFn = i; break; }
        }
        break;
    case KeyCode::ENTER:
        // Open floating calculate menu (NumWorks-style)
        openCalcMenu();
        return;
    case KeyCode::AC:
        // Exit trace mode — hide crosshair + pill
        clearIntegralShading();
        _grMode = GrMode::NAVIGATE;
        if (_traceDot)   lv_obj_add_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineH) lv_obj_add_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineV) lv_obj_add_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);
        if (_tracePill)  lv_obj_add_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
        _grMode = GrMode::IDLE;
        _focus = Focus::TOOLBAR;
        refreshToolbar();
        updateInfoBar();
        return;
    default:
        break;
    }
    drawTraceCursor();
    updateInfoBar();
}

// ── Table keys ──────────────────────────────────────────────────────────
void GrapherApp::handleTable(const KeyEvent& ev) {
    switch (ev.code) {
    case KeyCode::UP:
        if (_tblRow > 0) {
            _tblRow--;
        } else {
            _tblStart -= _tblStep;
        }
        rebuildTable();
        break;
    case KeyCode::DOWN:
        _tblRow++;
        rebuildTable();
        break;
    case KeyCode::LEFT:
        // Decrease step size
        if (_tblStep > 0.1f) {
            _tblStep /= 2.0f;
            rebuildTable();
        }
        break;
    case KeyCode::RIGHT:
        // Increase step size
        if (_tblStep < 10.0f) {
            _tblStep *= 2.0f;
            rebuildTable();
        }
        break;
    case KeyCode::AC:
        _focus = Focus::TAB_BAR;
        refreshTabBar();
        break;
    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Calculate Menu — Floating overlay (NumWorks-style)
// ═══════════════════════════════════════════════════════════════════════

static const char* CALC_MENU_LABELS[] = {
    "Find Root",
    "Find Minimum",
    "Find Maximum",
    "Find Intersection",
    "Calculate Integral",
};

void GrapherApp::openCalcMenu() {
    if (_calcMenuOpen || !_graphArea) return;

    _calcMenuOpen = true;
    _calcMenuIdx = 0;

    // Create floating menu container centered on the graph area
    _calcMenu = lv_obj_create(_graphArea);
    if (!_calcMenu) { _calcMenuOpen = false; return; }

    int menuW = 180;
    int menuH = CALC_MENU_ITEMS * 28 + 16;  // 28px per row + padding
    int areaW = lv_obj_get_width(_graphArea);
    int areaH = lv_obj_get_height(_graphArea);

    lv_obj_set_size(_calcMenu, menuW, menuH);
    lv_obj_set_pos(_calcMenu, (areaW - menuW) / 2, (areaH - menuH) / 2);
    lv_obj_set_style_bg_color(_calcMenu, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_calcMenu, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(_calcMenu, lv_color_hex(0x4A90E2), LV_PART_MAIN);
    lv_obj_set_style_border_width(_calcMenu, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_calcMenu, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_calcMenu, 8, LV_PART_MAIN);
    lv_obj_remove_flag(_calcMenu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(_calcMenu, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_flex_flow(_calcMenu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(_calcMenu, LV_FLEX_ALIGN_START, LV_PART_MAIN);

    // Create menu item labels
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
        _calcMenuRows[i] = lv_obj_create(_calcMenu);
        if (!_calcMenuRows[i]) continue;
        lv_obj_set_size(_calcMenuRows[i], menuW - 16, 26);
        lv_obj_set_style_radius(_calcMenuRows[i], 4, LV_PART_MAIN);
        lv_obj_set_style_pad_left(_calcMenuRows[i], 6, LV_PART_MAIN);
        lv_obj_set_style_pad_top(_calcMenuRows[i], 3, LV_PART_MAIN);
        lv_obj_set_style_border_width(_calcMenuRows[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_calcMenuRows[i], LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(_calcMenuRows[i]);
        if (lbl) {
            lv_label_set_text(lbl, CALC_MENU_LABELS[i]);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        }

        bool sel = (i == _calcMenuIdx);
        lv_obj_set_style_bg_color(_calcMenuRows[i],
            lv_color_hex(sel ? COL_BTN_BG : 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_calcMenuRows[i], LV_OPA_COVER, LV_PART_MAIN);
        if (lbl) {
            lv_obj_set_style_text_color(lbl,
                lv_color_hex(sel ? 0xFFFFFF : 0x333333), LV_PART_MAIN);
        }
    }
}

void GrapherApp::closeCalcMenu() {
    if (!_calcMenuOpen) return;
    _calcMenuOpen = false;

    // Delete the floating menu and all its children
    if (_calcMenu) {
        lv_obj_delete(_calcMenu);
        _calcMenu = nullptr;
    }
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
        _calcMenuRows[i] = nullptr;
    }
}

void GrapherApp::handleCalcMenu(const KeyEvent& ev) {
    switch (ev.code) {
    case KeyCode::UP:
        if (_calcMenuIdx > 0) {
            _calcMenuIdx--;
            // Refresh selection styling
            for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
                if (!_calcMenuRows[i]) continue;
                bool sel = (i == _calcMenuIdx);
                lv_obj_set_style_bg_color(_calcMenuRows[i],
                    lv_color_hex(sel ? COL_BTN_BG : 0xFFFFFF), LV_PART_MAIN);
                lv_obj_t* lbl = lv_obj_get_child(_calcMenuRows[i], 0);
                if (lbl) {
                    lv_obj_set_style_text_color(lbl,
                        lv_color_hex(sel ? 0xFFFFFF : 0x333333), LV_PART_MAIN);
                }
            }
        }
        break;
    case KeyCode::DOWN:
        if (_calcMenuIdx < CALC_MENU_ITEMS - 1) {
            _calcMenuIdx++;
            for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
                if (!_calcMenuRows[i]) continue;
                bool sel = (i == _calcMenuIdx);
                lv_obj_set_style_bg_color(_calcMenuRows[i],
                    lv_color_hex(sel ? COL_BTN_BG : 0xFFFFFF), LV_PART_MAIN);
                lv_obj_t* lbl = lv_obj_get_child(_calcMenuRows[i], 0);
                if (lbl) {
                    lv_obj_set_style_text_color(lbl,
                        lv_color_hex(sel ? 0xFFFFFF : 0x333333), LV_PART_MAIN);
                }
            }
        }
        break;
    case KeyCode::ENTER:
        executeCalcOption(_calcMenuIdx);
        closeCalcMenu();
        break;
    case KeyCode::AC:
    case KeyCode::LEFT:
        closeCalcMenu();
        break;
    default:
        break;
    }
}

void GrapherApp::executeCalcOption(int option) {
    if (_traceFn < 0 || _traceFn >= _numFuncs || !_funcs[_traceFn].valid) return;

    // Build evaluator lambda for the current traced function
    auto evalFunc = [this](double x) -> double {
        return evalAt(_traceFn, x);
    };

    math::AnalysisResult res = { false, 0.0, 0.0 };
    char pillBuf[80];

    switch (option) {
    case 0: { // Find Root
        res = math::findRoot(evalFunc, (double)_xMin, (double)_xMax);
        if (res.found) {
            snprintf(pillBuf, sizeof(pillBuf), "Root: x=%.4f y=%.6f", res.x, res.y);
        }
        break;
    }
    case 1: { // Find Minimum
        res = math::findExtremum(evalFunc, (double)_xMin, (double)_xMax, false);
        if (res.found) {
            snprintf(pillBuf, sizeof(pillBuf), "Min: x=%.4f y=%.4f", res.x, res.y);
        }
        break;
    }
    case 2: { // Find Maximum
        res = math::findExtremum(evalFunc, (double)_xMin, (double)_xMax, true);
        if (res.found) {
            snprintf(pillBuf, sizeof(pillBuf), "Max: x=%.4f y=%.4f", res.x, res.y);
        }
        break;
    }
    case 3: { // Find Intersection
        // Find the second valid function to intersect with
        int otherFn = -1;
        for (int i = 0; i < _numFuncs; ++i) {
            if (i != _traceFn && _funcs[i].valid) { otherFn = i; break; }
        }
        if (otherFn < 0) {
            if (_tracePillLabel)
                lv_label_set_text(_tracePillLabel, "No second function");
            return;
        }
        auto evalFunc2 = [this, otherFn](double x) -> double {
            return evalAt(otherFn, x);
        };
        res = math::findIntersection(evalFunc, evalFunc2,
                                     (double)_xMin, (double)_xMax);
        if (res.found) {
            snprintf(pillBuf, sizeof(pillBuf), "Intersect: x=%.4f y=%.4f", res.x, res.y);
        }
        break;
    }
    case 4: { // Calculate Integral
        res = math::numericalIntegral(evalFunc, (double)_xMin, (double)_xMax);
        if (res.found) {
            snprintf(pillBuf, sizeof(pillBuf), "Area = %.4f", res.y);
            // Draw area shading
            drawIntegralShading(_traceFn, _xMin, _xMax);
        }
        break;
    }
    default:
        return;
    }

    if (res.found) {
        // Move trace cursor to the result
        if (option != 4) {
            _traceX = static_cast<float>(res.x);
            if (_traceX < _xMin) _traceX = _xMin;
            if (_traceX > _xMax) _traceX = _xMax;
        }
        drawTraceCursor();

        // Update pill with special text
        if (_tracePillLabel) {
            lv_label_set_text(_tracePillLabel, pillBuf);
        }
        if (_tracePill) {
            lv_obj_remove_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (_tracePillLabel)
            lv_label_set_text(_tracePillLabel, "Not found in range");
        if (_tracePill)
            lv_obj_remove_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
    }
    updateInfoBar();
}

// ═══════════════════════════════════════════════════════════════════════
// Integral Area Shading
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::drawIntegralShading(int funcIdx, float shadeXMin, float shadeXMax) {
    clearIntegralShading();
    if (!_graphArea || funcIdx < 0 || funcIdx >= _numFuncs || !_funcs[funcIdx].valid)
        return;

    int areaW = lv_obj_get_width(_graphArea);
    int areaH = lv_obj_get_height(_graphArea);
    if (areaW < 2 || areaH < 2) return;

    float xRange = _xMax - _xMin;
    float yRange = _yMax - _yMin;
    if (xRange <= 0.0f || yRange <= 0.0f) return;

    // Find the Y pixel of the x-axis (y=0)
    float yAxisPx = (1.0f - (0.0f - _yMin) / yRange) * areaH;

    int count = 0;
    uint32_t funcColor = _funcs[funcIdx].color;

    for (int px = 0; px < areaW && count < 320; ++px) {
        float wx = _xMin + (float)px / areaW * xRange;
        if (wx < shadeXMin || wx > shadeXMax) continue;

        double wy = evalAt(funcIdx, wx);
        if (std::isnan(wy) || std::isinf(wy)) continue;

        float sy = (1.0f - ((float)wy - _yMin) / yRange) * areaH;

        // Clip to visible area
        if (sy < 0) sy = 0;
        if (sy > areaH) sy = static_cast<float>(areaH);

        float y0 = yAxisPx;
        if (y0 < 0) y0 = 0;
        if (y0 > areaH) y0 = static_cast<float>(areaH);

        // Draw vertical line from axis to function value
        float lineTop = std::min(sy, y0);
        float lineBot = std::max(sy, y0);
        if (lineBot - lineTop < 1.0f) continue;

        // Create a small lv_obj as a vertical shading line
        lv_obj_t* line = lv_obj_create(_graphArea);
        if (!line) break;  // Allocation failed — abort cleanly (Phase 5)

        lv_obj_set_size(line, 1, (int)(lineBot - lineTop));
        lv_obj_set_pos(line, px, (int)lineTop);
        lv_obj_set_style_bg_color(line, lv_color_hex(funcColor), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_30, LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(line, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);
        lv_obj_remove_flag(line, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

        _shadingLines[count] = line;
        count++;
    }
    _shadingCount = count;
    _shadingActive = true;
}

void GrapherApp::clearIntegralShading() {
    if (!_shadingActive) return;
    for (int i = 0; i < _shadingCount; ++i) {
        if (_shadingLines[i]) {
            lv_obj_delete(_shadingLines[i]);
            _shadingLines[i] = nullptr;
        }
    }
    _shadingCount = 0;
    _shadingActive = false;
}
