/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif
#include "../math/VariableManager.h"
#include "../utils/ColorUtils.h"

using namespace vpam;

#ifndef ARDUINO
// Native (emulator_pc): Arduino's cooperative yield() does not exist. The four
// call sites below feed the hardware watchdog during long plot/sample loops; the
// single-threaded PC emulator has no watchdog, so a no-op is the correct native
// equivalent. Firmware keeps the real Arduino yield(). None of these sites run in
// the Grapher initial-state path — they only need to compile/link.
static inline void yield() {}
#endif

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
static constexpr uint32_t COL_AXIS      = 0x333333;  // Axis line (dark on white bg)
static constexpr uint32_t COL_GRID_MAIN = 0xE0E0E0;  // Main grid (light grey on white)
static constexpr uint32_t COL_GRID_SUB  = 0xF0F0F0;  // Sub-grid (lighter grey)
static constexpr uint32_t COL_GRAPH_BG  = 0xFFFFFF;  // White graph background
static constexpr uint32_t COL_TB_BG     = 0xF4F4F4;  // Graph toolbar bg
static constexpr uint32_t COL_TB_SEL    = 0x4A90E2;  // Toolbar selected
static constexpr uint32_t COL_TB_TXT    = 0x333333;
static constexpr uint32_t COL_TBL_HDR   = 0xFFB531;  // Table header (NumWorks yellow)
static constexpr uint32_t COL_TBL_SEL   = 0xD6EAFF;

// ── Templates ───────────────────────────────────────────────────────────
struct TemplateEntry {
    const char* text;     // Expression to insert
    const char* display;  // Display label
};

// ── Kandinsky streaming constants ────────────────────────────────────────
static constexpr int INIT_SAMPLE_N = 40;  // Initial samples
static constexpr int ADAPT_DEPTH = 3;     // Adaptive refinement depth
static constexpr float TANGENT_REDRAW_THRESHOLD_PIXELS = 0.25f;
static constexpr float TANGENT_FD_STEP_RATIO = 0.001f;

static inline bool useStipplePixel(int x, int y) {
    return ((x + y) & 1) == 0;
}

static const TemplateEntry TEMPLATES[] = {
    { "y=2*x+3",   "y = 2x + 3  (Linear)"     },
    { "y=x^2-4",   "y = x\xB2 - 4  (Parabola)" },
    { "y=sin(x)",  "y = sin(x)  (Trig)"        },
    { "y=cos(x)",  "y = cos(x)  (Trig)"        },
    { "y=x^3",     "y = x\xB3  (Cubic)"         },
    { "y=1/x",     "y = 1/x  (Hyperbola)"      },
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
    , _graphCanvas(nullptr), _graphBuf(), _graphImgDsc{}
    , _traceDot(nullptr), _traceLineH(nullptr), _traceLineV(nullptr)
    , _tracePill(nullptr), _tracePillDot(nullptr), _tracePillLabel(nullptr)
    , _infoBar(nullptr), _infoLabel(nullptr)
    , _calcMenu(nullptr), _calcMenuIdx(0), _calcMenuOpen(false)
    , _shadingActive(false), _shadingFuncIdx(-1), _shadingX0(0.0f), _shadingX1(0.0f)
    , _tangentActive(false), _tangentFuncIdx(-1), _tangentX(0.0f)
    , _tblTable(nullptr)
    , _tab(Tab::EXPRESSIONS), _focus(Focus::TAB_BAR), _tabIdx(0)
    , _numFuncs(0), _exprIdx(0), _exprMode(ExprMode::LIST)
    , _grMode(GrMode::IDLE), _toolIdx(0)
    , _xMin(-10.0f), _xMax(10.0f), _yMin(-7.0f), _yMax(7.0f)
    , _traceX(0.0f), _traceFn(0), _plotDirty(true)
    , _tblRow(0), _tblStart(-5.0f), _tblStep(1.0f), _tblFuncIdx(0)
    , _poiAsyncTimer(nullptr), _poiAsyncFi(-1)
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
    }
    for (int i = 0; i < 4; ++i) _toolLabels[i] = nullptr;
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) _calcMenuRows[i] = nullptr;
    _tblTable = nullptr;
    _tblHeaderBar = nullptr;
    for (int i = 0; i < TBL_COLS; ++i)     _tblHdrLabels[i] = nullptr;
    for (int i = 0; i < TBL_COLS - 1; ++i) _tblHdrSeps[i]   = nullptr;
    _tplModal = nullptr;
    _tplCount = 0; _tplIdx = 0; _tplOpen = false;
    _tplLoadTimer = nullptr; _tplLoadNext = 0;
    _tplCardW = 0; _tplRowH = 0;
    for (int i = 0; i < 6; ++i) { _tplRows[i] = nullptr; }
    _numPOIs = 0;
    _snappedToPOI = false;
    _snappedPOIIdx = -1;
    _snapEscapeCount = 0;
    _modeBadge = nullptr;
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

    // PSRAMBuffer cleanup handled by destructor; no manual heap_caps_free required.
    if (_graphBuf) {
        _graphBuf.reset();
    }
    // Stop async POI timer if running
    if (_poiAsyncTimer) { lv_timer_delete(_poiAsyncTimer); _poiAsyncTimer = nullptr; }
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
        _graphCanvas = nullptr;
        _traceDot = nullptr; _traceLineH = nullptr; _traceLineV = nullptr;
        _tracePill = nullptr; _tracePillDot = nullptr; _tracePillLabel = nullptr;
        _infoBar = nullptr; _infoLabel = nullptr;
        _calcMenu = nullptr; _calcMenuOpen = false;
        for (int i = 0; i < CALC_MENU_ITEMS; ++i) _calcMenuRows[i] = nullptr;
        _shadingActive = false;
        _tangentActive = false;
        _tblTable = nullptr; _tblHeaderBar = nullptr;
        for (int i = 0; i < TBL_COLS; ++i)     _tblHdrLabels[i] = nullptr;
        for (int i = 0; i < TBL_COLS - 1; ++i) _tblHdrSeps[i]   = nullptr;
        _modeBadge = nullptr;
        for (int i = 0; i < 3; ++i) { _tabLabels[i] = nullptr; _tabPills[i] = nullptr; }
        for (int i = 0; i < MAX_FUNCS; ++i) {
            _exprRows[i] = nullptr; _exprDots[i] = nullptr;
            _tplBtns[i] = nullptr;
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
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
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
                            const lv_font_t* font = &stix_math_18) {
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

        // Plain UI word per tab. montserrat_14 (line_height 16) fits the pill
        // (TAB_H-6 = 22px) cleanly; the serif stix_math_18 (line_height 31)
        // overflowed and clipped the descenders ("Expression" lost its bottom
        // half). Fill the pill width + center on both axes so the label is
        // correctly centred regardless of font metrics.
        _tabLabels[i] = makeLabel(pill, 0, 0, titles[i], COL_TAB_TXT_I,
                                   &lv_font_montserrat_14);
        lv_obj_set_width(_tabLabels[i], tw - 4);
        lv_obj_set_style_text_align(_tabLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(_tabLabels[i]);
    }
}

// ── Notebook-lines callback for static background (NumWorks style) ──────
static void notebookLinesCb(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t* obj = lv_event_get_target_obj(e);
    if (!layer || !obj) return;
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

    // Scrollable expression panel on top — transparent background. Its height is
    // reduced by HINT_H so the bottom strip stays clear for the fixed footer hint
    // (otherwise the scrolled list would slide under/over it).
    static constexpr int HINT_H = 20;
    _panelExpr = makeContainer(_screen, 0, topY, SCREEN_W, panelH - HINT_H);
    lv_obj_set_style_bg_opa(_panelExpr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(_panelExpr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_panelExpr, LV_DIR_VER);
    // Keypad-driven scrolling only: drop momentum/elastic so scroll_to_view snaps
    // cleanly (no overscroll bounce or drift between key presses).
    lv_obj_remove_flag(_panelExpr, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(_panelExpr, LV_OBJ_FLAG_SCROLL_ELASTIC);

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
        lv_obj_set_style_bg_opa(_exprDots[i],
            _funcs[i].visible ? LV_OPA_COVER : LV_OPA_30, LV_PART_MAIN);
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
        int btnW = 80;
        _tplBtns[i] = lv_obj_create(_exprRows[i]);
        lv_obj_set_size(_tplBtns[i], btnW, ROW_H - 6);
        lv_obj_set_style_bg_color(_tplBtns[i], lv_color_hex(0xF2F2F2), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tplBtns[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(_tplBtns[i], 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(_tplBtns[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_tplBtns[i], lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tplBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_tplBtns[i], LV_OBJ_FLAG_SCROLLABLE);
        // montserrat_14 (lh 16) fits this 80x26 button; stix_math_18 (lh 31, and
        // ~90px wide for "Templates") clipped both vertically and horizontally
        // ("Templat"). Fill the button width + center on both axes.
        lv_obj_t* tplBtnLbl = makeLabel(_tplBtns[i], 0, (ROW_H - 6 - 16) / 2,
                                         "Templates", 0x666666, &lv_font_montserrat_14);
        lv_obj_set_width(tplBtnLbl, btnW);
        lv_obj_set_style_text_align(tplBtnLbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_add_flag(_tplBtns[i], LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(_exprRows[i], LV_OBJ_FLAG_HIDDEN);
    }

    // "Add an element" row — dashed-border-style look
    int addY = PAD;   // will be repositioned in refreshExprList
    _addRow = makeContainer(_panelExpr, PAD, addY, SCREEN_W - 2 * PAD, ROW_H, 0xF7F7F7);
    lv_obj_set_style_radius(_addRow, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(_addRow, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_addRow, lv_color_hex(COL_ROW_BRD), LV_PART_MAIN);
    // Plain UI/prose labels with spaces use lv_font_montserrat_14: stix_math_18 (the
    // makeLabel default, used for VPAM-adjacent math) has no U+0020 glyph, so spaced
    // prose renders a tofu box at every space. Real math (VPAM MathCanvas) and the
    // axis/tick labels stay on their STIX/graph path.
    _addLabel = makeLabel(_addRow, (SCREEN_W - 2 * PAD) / 2 - 50, (ROW_H - 14) / 2,
                           "Add an element", COL_ADD_TXT, &lv_font_montserrat_14);

    // "Plot graph" button — blue rounded pill
    _plotBtn = makeContainer(_panelExpr, PAD, 0, (SCREEN_W - 3 * PAD) / 2, ROW_H, COL_BTN_BG);
    lv_obj_set_style_radius(_plotBtn, 10, LV_PART_MAIN);
    makeLabel(_plotBtn, 20, (ROW_H - 14) / 2, "Plot graph", COL_BTN_TXT, &lv_font_montserrat_14);

    // "Display values" button — blue rounded pill
    _tableBtn = makeContainer(_panelExpr, PAD + (SCREEN_W - 3 * PAD) / 2 + PAD, 0,
                              (SCREEN_W - 3 * PAD) / 2, ROW_H, COL_BTN_BG);
    lv_obj_set_style_radius(_tableBtn, 10, LV_PART_MAIN);
    makeLabel(_tableBtn, 10, (ROW_H - 14) / 2, "Display values", COL_BTN_TXT, &lv_font_montserrat_14);

    // Bottom hint — fixed footer on the non-scrollable background strip below the
    // (shortened) list, so it never scrolls with the expressions nor overlaps a
    // row. As a child of _bgExpr it is shown/hidden with the Expressions tab.
    // y derived from HINT_H (not a bare literal) so the reserved strip and the
    // hint position can never drift apart if HINT_H is retuned.
    _exprHint = makeLabel(_bgExpr, PAD, panelH - HINT_H + 2,
                           "ENTER=edit  AC=back", COL_HINT, &lv_font_montserrat_14);

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


// ── Graph tick-label draw callback (DRAW_MAIN_END on _graphCanvas) ────────
// Grid lines and axes are rasterized directly into _graphBuf in replot();
// this callback only adds the numeric tick labels on top of the image.
static void graphTickLabelsCb(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t* obj = lv_event_get_target_obj(e);
    GrapherApp* app = static_cast<GrapherApp*>(lv_event_get_user_data(e));
    if (!layer || !obj || !app) return;

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

    lv_draw_label_dsc_t ldsc;
    lv_draw_label_dsc_init(&ldsc);
    ldsc.color = lv_color_hex(0x666666);
    ldsc.font  = &stix_math_18;

    char buf[16];
    float mainStep = niceStep(xRange, 8);
    int32_t ay = toSY(0.0f);

    // X-axis tick labels
    float start = floorf(xMin / mainStep) * mainStep;
    for (float v = start; v <= xMax; v += mainStep) {
        if (fabsf(v) < mainStep * 0.01f) continue;
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
        if (fabsf(v) < yMainStep * 0.01f) continue;
        int32_t sy = toSY(v);
        if (sy < coords.y1 + 5 || sy > coords.y2 - 5) continue;
        if (fabsf(v - roundf(v)) < 0.001f)
            snprintf(buf, sizeof(buf), "%d", (int)roundf(v));
        else
            snprintf(buf, sizeof(buf), "%.1g", (double)v);
        int32_t ax = toSX(0.0f);
        int32_t lx = (ax >= coords.x1 + 25 && ax <= coords.x2) ? ax - 24 : coords.x1 + 2;
        lv_area_t la = { lx, sy - 5, lx + 22, sy + 7 };
        ldsc.text = buf;
        ldsc.text_local = 1;
        lv_draw_label(layer, &ldsc, &la);
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
    const char* tools[] = { "Auto", "Axes", "Pan", "Trace" };
    int tw = SCREEN_W / 4;
    for (int i = 0; i < 4; ++i) {
        // montserrat_14 (lh 16) fits the 24px toolbar; stix_math_18 (lh 31) clipped
        // here too. Center each tool word in its quarter-width column.
        _toolLabels[i] = makeLabel(_graphToolbar, i * tw, (TOOLBAR_H - 16) / 2,
                                    tools[i], COL_TB_TXT, &lv_font_montserrat_14);
        lv_obj_set_width(_toolLabels[i], tw);
        lv_obj_set_style_text_align(_toolLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    // ── Graph area (below toolbar, above info bar) ──
    int graphY = TOOLBAR_H;
    int graphH = panelH - TOOLBAR_H - INFO_BAR_H;
    _graphArea = makeContainer(_panelGraph, 0, graphY, SCREEN_W, graphH);
    lv_obj_set_style_bg_color(_graphArea, lv_color_hex(COL_GRAPH_BG), LV_PART_MAIN);

    // ── Kandinsky canvas: single RGB565 PSRAM buffer for grid + curves ──
    size_t bufSz = (size_t)GRAPH_CANVAS_W * GRAPH_CANVAS_H * sizeof(uint16_t);
    bool allocOk = _graphBuf.allocate(GRAPH_CANVAS_W * GRAPH_CANVAS_H);
    if (allocOk && _graphBuf) {
        memset(_graphBuf.data(), 0xFF, bufSz);  // Fill white (0xFFFF in RGB565)
#ifdef ARDUINO
        Serial.printf("[GRAPHER] Kandinsky buffer %u bytes in PSRAM, free=%u\n",
                      (unsigned)bufSz,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#else
        Serial.printf("[GRAPHER] Kandinsky buffer %u bytes (native heap)\n",
                      (unsigned)bufSz);
#endif
        // Initialize GraphView with the buffer
        new (&_view) grapher::GraphView(_graphBuf.data(), GRAPH_CANVAS_W, GRAPH_CANVAS_H);
    } else {
        Serial.printf("[GRAPHER] FAIL: Kandinsky PSRAM alloc %u bytes\n", (unsigned)bufSz);
        // ── PSRAM Safety Net: graceful degradation with user-visible warning ──
        lv_obj_t* errLabel = lv_label_create(_graphArea);
        lv_label_set_text(errLabel, LV_SYMBOL_WARNING " ERR: INSUFFICIENT PSRAM\nClose other apps.");
        lv_obj_set_style_text_color(errLabel, lv_color_hex(0xCC0000), LV_PART_MAIN);
        lv_obj_set_style_text_font(errLabel, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_align(errLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(errLabel);
    }

    // Set up the LVGL image descriptor pointing at our raw buffer
    _graphImgDsc.header.w  = GRAPH_CANVAS_W;
    _graphImgDsc.header.h  = GRAPH_CANVAS_H;
    _graphImgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _graphImgDsc.data_size = (uint32_t)bufSz;
    _graphImgDsc.data      = (const uint8_t*)_graphBuf.data();

    // lv_image widget — displays the canvas; tick labels drawn on DRAW_MAIN_END
    _graphCanvas = lv_image_create(_graphArea);
    lv_obj_set_pos(_graphCanvas, 0, 0);
    lv_obj_set_size(_graphCanvas, GRAPH_CANVAS_W, GRAPH_CANVAS_H);
    if (_graphBuf) lv_image_set_src(_graphCanvas, &_graphImgDsc);
    lv_obj_add_event_cb(_graphCanvas, graphTickLabelsCb, LV_EVENT_DRAW_MAIN_END, this);

    // Trace cursor dot
    _traceDot = lv_obj_create(_graphArea);
    lv_obj_set_size(_traceDot, 8, 8);
    lv_obj_set_style_bg_color(_traceDot, lv_color_hex(0x333333), LV_PART_MAIN);
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
    // Plain-prose trace readout ("x: .. y: .." / POI labels, drawTraceCursor) uses
    // lv_font_montserrat_14: stix_math_18 has no U+0020 glyph, so the spaces tofu.
    lv_obj_set_style_text_font(_tracePillLabel, &lv_font_montserrat_14, LV_PART_MAIN);

    // ── Info bar at bottom ──
    _infoBar = makeContainer(_panelGraph, 0, panelH - INFO_BAR_H, SCREEN_W, INFO_BAR_H, COL_TB_BG);
    _infoLabel = makeLabel(_infoBar, PAD, 2, "", 0x333333, &lv_font_montserrat_14);
    // Mode badge on the right side of info bar: "[Trace]" or "[Pan]".
    // montserrat_14 fits the 20px info bar and matches _infoLabel; stix_math_18
    // (lh 31) clipped vertically AND ran "[Trace]" off the right screen edge.
    // Right-align inside a fixed box so both "[Trace]" and "[Pan]" stay on-screen.
    _modeBadge = makeLabel(_infoBar, SCREEN_W - 72, 2, "[Trace]", 0x4A90E2,
                            &lv_font_montserrat_14);
    lv_obj_set_width(_modeBadge, 72 - PAD);
    lv_obj_set_style_text_align(_modeBadge, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
}

// ── Table zebra-stripe draw event (LVGL 9) ──────────────────────────────
static void tblZebraDrawCb(lv_event_t* e) {
    lv_draw_task_t* dt = lv_event_get_draw_task(e);
    if (!dt) return;
    if (lv_draw_task_get_type(dt) != LV_DRAW_TASK_TYPE_FILL) return;
    lv_draw_fill_dsc_t* fill = lv_draw_task_get_fill_dsc(dt);
    if (!fill) return;
    // In LVGL 9 lv_draw_fill_dsc_t begins with lv_draw_dsc_base_t
    lv_draw_dsc_base_t* base = (lv_draw_dsc_base_t*)fill;
    if (base->part != LV_PART_ITEMS) return;
    // id1 = row, id2 = col
    if (base->id1 % 2 == 1) {
        fill->color = lv_color_hex(0xEEF4FF);  // Alternate (odd) rows: light blue
        fill->opa   = LV_OPA_COVER;
    }
}

// ── Table panel ─────────────────────────────────────────────────────────
void GrapherApp::createTablePanel() {
#ifdef ARDUINO
    Serial.printf("[GRAPHER] tablePanel heap=%u\n", (unsigned)esp_get_free_heap_size());
#endif
    int topY = BAR_H + TAB_H;
    int panelH = SCREEN_H - topY;

    // ── Sticky header bar (always visible, not part of scrollable area) ──
    _tblHeaderBar = lv_obj_create(_screen);
    if (!_tblHeaderBar) { Serial.println("[GRAPHER] FAIL _tblHeaderBar"); return; }
    lv_obj_set_pos(_tblHeaderBar, 0, topY);
    lv_obj_set_size(_tblHeaderBar, SCREEN_W, TBL_HDR_H);
    lv_obj_set_style_bg_color(_tblHeaderBar, lv_color_hex(COL_TBL_HDR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tblHeaderBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tblHeaderBar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_side(_tblHeaderBar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tblHeaderBar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_tblHeaderBar, lv_color_hex(0xD0A020), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_tblHeaderBar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tblHeaderBar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_tblHeaderBar, LV_OBJ_FLAG_HIDDEN);

    // Header labels: one per column ("x", "f1(x)", "f2(x)", …). montserrat_14
    // (lh 16) fits the 22px TBL_HDR_H bar; stix_math_18 (lh 31) overflowed and
    // clipped the header text bottoms. rebuildTable() sets each label's text,
    // width (= column width) and x per the active function count, and centers
    // the text within its column; separators are repositioned to match. Only
    // columns 0..cols-1 are shown; the rest stay hidden.
    for (int c = 0; c < TBL_COLS; ++c) {
        _tblHdrLabels[c] = makeLabel(_tblHeaderBar, 0, (TBL_HDR_H - 16) / 2,
                                     "", 0x000000, &lv_font_montserrat_14);
        lv_obj_set_style_text_align(_tblHdrLabels[c], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_add_flag(_tblHdrLabels[c], LV_OBJ_FLAG_HIDDEN);
    }
    // Separators between header columns (up to TBL_COLS-1); shown per column count.
    for (int s = 0; s < TBL_COLS - 1; ++s) {
        _tblHdrSeps[s] = lv_obj_create(_tblHeaderBar);
        lv_obj_set_pos(_tblHdrSeps[s], SCREEN_W / 2, 0);
        lv_obj_set_size(_tblHdrSeps[s], 1, TBL_HDR_H);
        lv_obj_set_style_bg_color(_tblHdrSeps[s], lv_color_hex(0xD0A020), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tblHdrSeps[s], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(_tblHdrSeps[s], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_tblHdrSeps[s], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tblHdrSeps[s], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_tblHdrSeps[s], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_tblHdrSeps[s], LV_OBJ_FLAG_HIDDEN);
    }

    // ── Scrollable data panel (below sticky header) ──
    _panelTable = lv_obj_create(_screen);
    if (!_panelTable) { Serial.println("[GRAPHER] FAIL _panelTable"); return; }
    lv_obj_set_pos(_panelTable, 0, topY + TBL_HDR_H);
    lv_obj_set_size(_panelTable, SCREEN_W, panelH - TBL_HDR_H);
    lv_obj_set_style_bg_color(_panelTable, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_panelTable, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_panelTable, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_panelTable, 0, LV_PART_MAIN);
    // Scrollable — needed for 21+ data rows that exceed the panel height
    lv_obj_add_flag(_panelTable, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_panelTable, LV_DIR_VER);
    lv_obj_add_flag(_panelTable, LV_OBJ_FLAG_HIDDEN);

    // Native lv_table widget (data rows only — no header row in table)
    _tblTable = lv_table_create(_panelTable);
    if (!_tblTable) { Serial.println("[GRAPHER] FAIL _tblTable"); return; }
    lv_obj_set_pos(_tblTable, 0, 0);
    lv_obj_set_width(_tblTable, SCREEN_W);
    lv_table_set_column_count(_tblTable, 2);
    lv_table_set_column_width(_tblTable, 0, SCREEN_W / 2);
    lv_table_set_column_width(_tblTable, 1, SCREEN_W / 2);
    lv_table_set_row_count(_tblTable, TBL_ROWS);

    // Style: white bg, black text, subtle border
    lv_obj_set_style_bg_color(_tblTable, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_text_color(_tblTable, lv_color_hex(0x000000), LV_PART_ITEMS);
    lv_obj_set_style_text_font(_tblTable, &stix_math_18, LV_PART_ITEMS);
    lv_obj_set_style_border_width(_tblTable, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(_tblTable, lv_color_hex(0xE0E0E0), LV_PART_ITEMS);
    lv_obj_set_style_pad_top(_tblTable, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(_tblTable, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(_tblTable, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(_tblTable, 6, LV_PART_ITEMS);

    // Remove main border around table
    lv_obj_set_style_border_width(_tblTable, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_tblTable, 0, LV_PART_MAIN);

    // Zebra stripes via draw task event (LVGL 9)
    lv_obj_add_flag(_tblTable, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(_tblTable, tblZebraDrawCb, LV_EVENT_DRAW_TASK_ADDED, nullptr);

    Serial.println("[GRAPHER] tablePanel Done");
}

// ═══════════════════════════════════════════════════════════════════════
// Tab switching
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::switchTab(Tab t) {
    _tab = t;
    _tabIdx = (int)t;

    // Show/hide panels
    if (_bgExpr)       lv_obj_add_flag(_bgExpr, LV_OBJ_FLAG_HIDDEN);
    if (_panelExpr)    lv_obj_add_flag(_panelExpr, LV_OBJ_FLAG_HIDDEN);
    if (_panelGraph)   lv_obj_add_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
    if (_panelTable)   lv_obj_add_flag(_panelTable, LV_OBJ_FLAG_HIDDEN);
    if (_tblHeaderBar) lv_obj_add_flag(_tblHeaderBar, LV_OBJ_FLAG_HIDDEN);

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
                _grMode = GrMode::TRACE;  // Default to TRACE for immediate interaction
                // Pick first valid function for trace
                _traceFn = -1;
                for (int i = 0; i < _numFuncs; ++i) {
                    if (_funcs[i].valid) { _traceFn = i; break; }
                }
                _traceX = (_xMin + _xMax) / 2.0f;
                _plotDirty = true;
                _snappedToPOI = false;
                _snappedPOIIdx = -1;
                _snapEscapeCount = 0;
            }
        } else {
            lv_obj_remove_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
            _grMode = GrMode::TRACE;
            _traceFn = -1;
            for (int i = 0; i < _numFuncs; ++i) {
                if (_funcs[i].valid) { _traceFn = i; break; }
            }
            _traceX = (_xMin + _xMax) / 2.0f;
            _plotDirty = true;
            _snappedToPOI = false;
            _snappedPOIIdx = -1;
            _snapEscapeCount = 0;
            replot();
            // Pre-compute POIs for snap-to-point
            if (_traceFn >= 0) computePOIs(_traceFn);
            drawTraceCursor();
        }
        updateInfoBar();
        break;
    case Tab::TABLE:
        if (_tblHeaderBar) lv_obj_remove_flag(_tblHeaderBar, LV_OBJ_FLAG_HIDDEN);
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
            // Sync dot opacity with visibility flag
            if (_exprDots[i]) {
                lv_obj_set_style_bg_opa(_exprDots[i],
                    _funcs[i].visible ? LV_OPA_COVER : LV_OPA_30, LV_PART_MAIN);
            }
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

    // Keep the focused item on-screen. Snap with no animation so the list never
    // lags behind the cursor or jitters with momentum/elastic overscroll — the
    // selection used to disappear below the fold once >3 functions were added.
    lv_obj_t* sel = nullptr;
    if (_exprIdx < _numFuncs)            sel = _exprRows[_exprIdx];
    else if (_exprIdx == _numFuncs)      sel = _addRow;
    else if (_exprIdx == _numFuncs + 1)  sel = _plotBtn;
    else if (_exprIdx == _numFuncs + 2)  sel = _tableBtn;
    if (sel) lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
}

void GrapherApp::addFunction() {
    if (_numFuncs >= MAX_FUNCS) return;
    int idx = _numFuncs;
    _funcs[idx] = FuncSlot{};
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

    // Shift down data (use assignment, NOT memcpy — FuncSlot has std::vector)
    for (int i = idx; i < _numFuncs - 1; ++i) {
        _funcs[i] = _funcs[i + 1];
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

        // Validate expression and cache RPN for fast evalAt()
        preCacheFuncRPN(idx);
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
    int16_t astH = vpam::mathObjectHeightPx(rootL, _exprCanvas[idx].normalMetrics(), 2 * PILL_PAD);
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
                // Phase 9F: the '=' of a template is a NodeRelation (OpKind::Eq),
                // NOT a NodeVariable like manual FREE_EQ entry. Without this case it
                // fell through to '+', so "y=2*x+3" serialized as "y+2*x+3": no '='
                // → getExprRHS kept the whole string → unknown var 'y' → never
                // plotted (templates inserted but drew nothing). Emit a real '='.
                case OpKind::Eq:  c = '='; break;
                default:          c = '+'; break;
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
            // Phase 9F: the numeric Evaluator has no 2-arg log, so emit the
            // change-of-base identity   log_b(a) = log(a)/log(b)   (both log10;
            // the ratio is base-independent). Wrapped in outer parens so a
            // surrounding ^/×/÷ binds to the whole quotient, not just log(b).
            // Previously this dropped the base entirely (serialized as plain
            // "log(arg)" → graphed log10 instead of log_b → wrong curve).
            auto* lb = static_cast<const NodeLogBase*>(node);
            auto emitStr = [&](const char* s) {
                for (int j = 0; s[j] && pos < maxLen - 1; ++j) buf[pos++] = s[j];
            };
            auto emitChild = [&](const vpam::MathNode* c) {
                if (!c) return;
                if (c->type() == NodeType::Row)
                    serializeRow(static_cast<const NodeRow*>(c), buf, pos, maxLen);
                else serializeNode(c, buf, pos, maxLen);
            };
            emitStr("(log(");
            emitChild(lb->argument());
            emitStr(")/log(");
            emitChild(lb->base());
            emitStr("))");
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
        } else if (c == 'y' || c == 'Y') {
            r->appendChild(vpam::makeVariable('y'));
        } else if (c == '=') {
            // VPAM has no dedicated equality-operator node; '=' is rendered
            // via NodeOperator with OpKind::Eq (MathClass::REL for TeX spacing).
            r->appendChild(vpam::makeRelation(OpKind::Eq));
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
    makeLabel(card, cardW / 2 - 40, 6, "Templates", 0x333333, &stix_math_18);

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

    // Inline VPAM math previews are unusable in these compact (~20px) rows:
    // MathCanvas vertically centers using a metric that excludes the descender,
    // so every "y = ..." template lost its 'y' tail (it read as "v") and the
    // fraction/trig forms overflowed the row entirely. Render each template as a
    // clean lv_font_montserrat_14 text label of the EXACT string that ENTER
    // inserts (TEMPLATES[i].text): no canvas, no clipping, and the preview now
    // matches what gets inserted. Touches no renderer/MathAST code. (The unused
    // _tplCanvas[i]/_tplAST[i] members stay null-constructed; closeTemplates'
    // destroy()/reset() over all six slots is null-safe.)
    makeLabel(_tplRows[i], 10, (_tplRowH - 14) / 2,
              TEMPLATES[i].text, 0x000000, &lv_font_montserrat_14);

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

// Pre-parse and cache the RPN for function idx to avoid repeated tokenize+parse.
// A bare expression (no '=') is accepted as the implicit function y=<expr>
// (see GraphModel::getExprRHS), so "x"/"2x" compile and plot like y=x/y=2x.
// Only the RHS (after the first '=', or the whole string) is compiled.
void GrapherApp::preCacheFuncRPN(int idx) {
    if (idx < 0 || idx >= _numFuncs) return;
    _funcs[idx].valid = false;

    if (_funcs[idx].len <= 0) return;

    // Delegate to GraphModel to compile and cache the RPN
    _model.preCacheRPN(_funcs[idx]);
}

float GrapherApp::evalAt(int idx, float x) {
    if (idx < 0 || idx >= _numFuncs || !_funcs[idx].valid) return NAN;
    // Delegate to GraphModel for evaluation
    return _model.evalAt(_funcs[idx], x);
}

// ── Adaptive sampling helpers (Phase B: Kandinsky streaming) ──────────────

// Recursively subdivide segment and stream directly to GraphView buffer.
// Instead of storing points in an array, this draws segments in real-time.
// Max recursion depth is ADAPT_DEPTH (3) — safe on ESP32 stack.
// Signature: (app, fi, world_x0, world_y0, world_x1, world_y1, depth)
void GrapherApp::adaptSegStream(int fi,
                                float wx0, float wy0,
                                float wx1, float wy1,
                                int depth, uint32_t color)
{
    static constexpr float ADAPT_THRESHOLD_PX = 2.0f;
    if (depth >= ADAPT_DEPTH) {
        _view.drawFunctionSegment(wx0, wy0, wx1, wy1, color);
        return;
    }

    const float wxMid = (wx0 + wx1) * 0.5f;
    const float wyMid = (float)evalAt(fi, wxMid);
    if (std::isnan(wyMid) || std::isinf(wyMid)) return;

    // Y-only screen-space deviation keeps sampling cheap and catches visible jaggies.
    const int sy0 = _view.worldToScreenY((float)wy0);
    const int sy1 = _view.worldToScreenY((float)wy1);
    const int syMid = _view.worldToScreenY((float)wyMid);
    const int syEst = (sy0 + sy1) / 2;

    if (std::abs(syMid - syEst) > (int)ADAPT_THRESHOLD_PX) {
        adaptSegStream(fi, wx0, wy0, wxMid, wyMid, depth + 1, color);
        adaptSegStream(fi, wxMid, wyMid, wx1, wy1, depth + 1, color);
        return;
    }

    _view.drawFunctionSegment(wx0, wy0, wx1, wy1, color);
}

// Sample function fi adaptively, streaming directly to the GraphView buffer.
// No point array — all drawing happens in real-time.
void GrapherApp::sampleFuncAdaptive(int fi, uint32_t color) {
    const float xRange = (float)(_xMax - _xMin);
    const float yRange = (float)(_yMax - _yMin);
    if (xRange <= 0 || yRange <= 0) return;
    const float step = xRange / (float)INIT_SAMPLE_N;
    float wx0 = (float)_xMin;
    float wy0 = (float)evalAt(fi, wx0);
    bool prevOk = (!std::isnan(wy0) && !std::isinf(wy0));

    for (int i = 1; i <= INIT_SAMPLE_N; ++i) {
        const float wx1 = (i == INIT_SAMPLE_N) ? (float)_xMax : ((float)_xMin + step * (float)i);
        const float wy1 = (float)evalAt(fi, wx1);
        const bool currOk = (!std::isnan(wy1) && !std::isinf(wy1));
        if (prevOk && currOk) {
            adaptSegStream(fi, wx0, wy0, wx1, wy1, 0, color);
        }
        wx0 = wx1;
        wy0 = wy1;
        prevOk = currOk;
        if ((i & 7) == 0) yield();
    }
}

void GrapherApp::replot() {
    if (!_plotDirty || !_graphArea || !_graphCanvas) return;
    _plotDirty = false;

    int areaH = lv_obj_get_height(_graphArea);
    if (areaH < 2 || !_graphBuf) return;  // Layout not yet computed or buffer missing

    // Update GraphView's buffer dimensions and viewport
    _view.setViewport(_xMin, _xMax, _yMin, _yMax);

    // ── Phase B orchestrator: clear -> grid -> stream curves -> push ─────
    _view.clearBuffer();
    _view.drawGrid();
    _view.drawAxes();

    for (int f = 0; f < _numFuncs && f < MAX_FUNCS; ++f) {
        if (!_funcs[f].valid || !_funcs[f].visible) continue;
        sampleFuncAdaptive(f, _funcs[f].color);
    }

    if (_shadingActive) {
        drawIntegralShading(_shadingFuncIdx, _shadingX0, _shadingX1);
    }
    if (_tangentActive) {
        drawTangentOverlay(_tangentFuncIdx, _tangentX);
    }
    for (int i = 0; i < _numPOIs; ++i) {
        if (_pois[i].type == POIType::Intersection) {
            _view.drawIntersectionMarker(_pois[i].x, _pois[i].y, 0xAA00CC);
        }
    }

    lv_image_set_src(_graphCanvas, &_graphImgDsc);
    lv_obj_invalidate(_graphCanvas);
    lv_refr_now(NULL);

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

    float wy = evalAt(_traceFn, _traceX);
    if (std::isnan(wy) || std::isinf(wy)) {
        if (_traceDot)   lv_obj_add_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineH) lv_obj_add_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineV) lv_obj_add_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);
        if (_tracePill)  lv_obj_add_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    float sx = (_traceX - _xMin) / xRange * areaW;
    float sy = (1.0f - (wy - _yMin) / yRange) * areaH;

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

    // Floating pill — show function color + x/y values, or POI label if snapped
    lv_obj_remove_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(_tracePillDot, lv_color_hex(_funcs[_traceFn].color), LV_PART_MAIN);

    char pillBuf[80];
    // Check if we're snapped to a POI
    const char* poiLabel = (_snappedToPOI && _snappedPOIIdx >= 0 && _snappedPOIIdx < _numPOIs)
        ? _pois[_snappedPOIIdx].label
        : nullptr;
    if (poiLabel) {
        snprintf(pillBuf, sizeof(pillBuf), "%s: (%.3f, %.3f)", poiLabel, (double)_traceX, wy);
    } else {
        snprintf(pillBuf, sizeof(pillBuf), "x: %.3f   y: %.3f", (double)_traceX, wy);
    }
    lv_label_set_text(_tracePillLabel, pillBuf);
}

void GrapherApp::updateInfoBar() {
    if (!_infoLabel) return;
    char buf[64];
    if (_grMode == GrMode::TRACE && _traceFn >= 0 && _traceFn < _numFuncs) {
        float y = evalAt(_traceFn, _traceX);
        if (_snappedToPOI && _snappedPOIIdx >= 0 && _snappedPOIIdx < _numPOIs) {
            snprintf(buf, sizeof(buf), "Trace [%s]: x=%.3f  y=%.3f",
                     _pois[_snappedPOIIdx].label, (double)_traceX, (double)y);
        } else {
            snprintf(buf, sizeof(buf), "Trace: x=%.3f  y=%.3f", (double)_traceX, (double)y);
        }
    } else {
        snprintf(buf, sizeof(buf), "x:[%.2g,%.2g] y:[%.2g,%.2g]  ENTER=trace",
                 (double)_xMin, (double)_xMax, (double)_yMin, (double)_yMax);
    }
    lv_label_set_text(_infoLabel, buf);

    // Update mode badge: "[Trace]" in blue or "[Pan]" in gray
    if (_modeBadge) {
        if (_grMode == GrMode::TRACE) {
            lv_label_set_text(_modeBadge, "[Trace]");
            lv_obj_set_style_text_color(_modeBadge, lv_color_hex(0x4A90E2), LV_PART_MAIN);
        } else {
            lv_label_set_text(_modeBadge, "[Pan]");
            lv_obj_set_style_text_color(_modeBadge, lv_color_hex(0x888888), LV_PART_MAIN);
        }
    }
}

void GrapherApp::autoFit() {
    // Preserve current x-range, auto-fit only y-range (with sampling within current viewport).
    const float x0 = _xMin;
    const float x1 = _xMax;
    if (x1 <= x0) {
        return;
    }

    float globalYMin = 1e9f, globalYMax = -1e9f;
    bool hasData = false;

    const int samplePoints = 120;
    for (int f = 0; f < _numFuncs; ++f) {
        if (!_funcs[f].valid || !_funcs[f].visible) continue;
        for (int i = 0; i <= samplePoints; ++i) {
            float t = (float)i / (float)samplePoints;
            float wx = x0 + t * (x1 - x0);
            float wy = evalAt(f, wx);
            if (std::isnan(wy) || std::isinf(wy) || fabsf(wy) > 1e6f) continue;
            if (wy < globalYMin) globalYMin = wy;
            if (wy > globalYMax) globalYMax = wy;
            hasData = true;
        }
    }
    if (hasData) {
        float range = globalYMax - globalYMin;
        if (range < 1e-6f) range = 2.0f;
        float margin = range * 0.10f;
        if (margin < 0.5f) margin = 0.5f;
        _yMin = globalYMin - margin;
        _yMax = globalYMax + margin;
    } else {
        // No visible functions or all out of bounds: keep legacy default logic.
        _yMin = -7.0f;
        _yMax = 7.0f;
    }

    _plotDirty = true;
}

// ── POI computation (roots/extrema/intersections) for snap-to-point ─────
void GrapherApp::appendPOIsForFunction(int fi) {
    if (fi < 0 || fi >= _numFuncs || !_funcs[fi].valid || !_funcs[fi].visible) return;

    const int N = INIT_SAMPLE_N;
    const float step = ((float)_xMax - (float)_xMin) / (float)N;
    const float yDupEps = (((float)_yMax - (float)_yMin) / (float)N) * 0.1f;
    if (step <= 0.0f) return;

    if (_xMin <= 0.0f && _xMax >= 0.0f && _numPOIs < MAX_POIS) {
        const float y0 = (float)evalAt(fi, 0.0f);
        if (!std::isnan(y0) && !std::isinf(y0)) {
            bool dup = false;
            for (int k = 0; k < _numPOIs; ++k) {
                if (fabsf(_pois[k].x) < step * 0.1f && fabsf(_pois[k].y - y0) < yDupEps) { dup = true; break; }
            }
            if (!dup && _numPOIs < MAX_POIS) {
                auto& p = _pois[_numPOIs++];
                p.x = 0.0f;
                p.y = y0;
                p.type = POIType::Intercept;
                strncpy(p.label, "Intercept", sizeof(p.label) - 1);
                p.label[sizeof(p.label) - 1] = '\0';
            }
        }
    }

    float yPrev = (float)evalAt(fi, (float)_xMin);
    float yPrev2 = yPrev;
    for (int i = 1; i <= N && _numPOIs < MAX_POIS - 2; ++i) {
        const float x1 = (i == N) ? (float)_xMax : ((float)_xMin + (float)i * step);
        const float y1 = (float)evalAt(fi, x1);

        if (!std::isnan(yPrev) && !std::isnan(y1) && !std::isinf(yPrev) && !std::isinf(y1)) {
            if (yPrev * y1 < 0.0f && _numPOIs < MAX_POIS) {
                float lo = x1 - step;
                float hi = x1;
                float ylo = yPrev;
                for (int iter = 0; iter < BISECTION_ITER; ++iter) {
                    const float mid = (lo + hi) * 0.5f;
                    const float ym = (float)evalAt(fi, mid);
                    if (std::isnan(ym) || std::isinf(ym)) break;
                    if (ym * ylo < 0.0f) {
                        hi = mid;
                    } else {
                        lo = mid;
                        ylo = ym;
                    }
                }
                const float rx = (lo + hi) * 0.5f;
                bool dup = false;
                for (int k = 0; k < _numPOIs; ++k) {
                    if (fabsf(_pois[k].x - rx) < step * 0.1f) { dup = true; break; }
                }
                if (!dup && _numPOIs < MAX_POIS) {
                    auto& p = _pois[_numPOIs++];
                    p.x = rx; p.y = 0.0f;
                    p.type = POIType::Root;
                    strncpy(p.label, "Root", sizeof(p.label) - 1);
                    p.label[sizeof(p.label) - 1] = '\0';
                }
            }

            if (i >= 2 && !std::isnan(yPrev2) && !std::isinf(yPrev2) && _numPOIs < MAX_POIS) {
                const float xm = x1 - step;
                const bool isMin = (yPrev < yPrev2) && (yPrev < y1);
                const bool isMax = (yPrev > yPrev2) && (yPrev > y1);
                if (isMin || isMax) {
                    bool dup = false;
                    for (int k = 0; k < _numPOIs; ++k) {
                        if (fabsf(_pois[k].x - xm) < step * 0.1f) { dup = true; break; }
                    }
                    if (!dup && _numPOIs < MAX_POIS) {
                        auto& p = _pois[_numPOIs++];
                        p.x = xm;
                        p.y = yPrev;
                        p.type = isMin ? POIType::Min : POIType::Max;
                        strncpy(p.label, isMin ? "Min" : "Max", sizeof(p.label) - 1);
                        p.label[sizeof(p.label) - 1] = '\0';
                    }
                }
            }
        }

        yPrev2 = yPrev;
        yPrev = y1;
        if ((i & 7) == 0) yield();
    }

    for (int j = 0; j < fi && _numPOIs < MAX_POIS; ++j) {
        if (!_funcs[j].valid) continue;

        float prevDiff = (float)evalAt(fi, (float)_xMin) - (float)evalAt(j, (float)_xMin);
        for (int i = 1; i <= N && _numPOIs < MAX_POIS; ++i) {
            const float x1 = (i == N) ? (float)_xMax : ((float)_xMin + (float)i * step);
            const float diff = (float)evalAt(fi, x1) - (float)evalAt(j, x1);
            if (std::isnan(prevDiff) || std::isnan(diff) || std::isinf(prevDiff) || std::isinf(diff)) {
                prevDiff = diff;
                continue;
            }
            if (prevDiff * diff < 0.0f) {
                float lo = x1 - step;
                float hi = x1;
                float dlo = prevDiff;
                for (int iter = 0; iter < BISECTION_ITER; ++iter) {
                    const float mid = (lo + hi) * 0.5f;
                    const float dmid = (float)evalAt(fi, mid) - (float)evalAt(j, mid);
                    if (std::isnan(dmid) || std::isinf(dmid)) break;
                    if (dmid * dlo < 0.0f) {
                        hi = mid;
                    } else {
                        lo = mid;
                        dlo = dmid;
                    }
                }
                const float ix = (lo + hi) * 0.5f;
                const float iy = (float)evalAt(fi, ix);
                bool dup = false;
                for (int k = 0; k < _numPOIs; ++k) {
                    if (fabsf(_pois[k].x - ix) < step * 0.1f) { dup = true; break; }
                }
                if (!dup && !std::isnan(iy) && !std::isinf(iy) && _numPOIs < MAX_POIS) {
                    auto& p = _pois[_numPOIs++];
                    p.x = ix;
                    p.y = iy;
                    p.type = POIType::Intersection;
                    strncpy(p.label, "Intersection", sizeof(p.label) - 1);
                    p.label[sizeof(p.label) - 1] = '\0';
                }
            }
            prevDiff = diff;
            if ((i & 7) == 0) yield();
        }
    }
}

void GrapherApp::computePOIs(int fi) {
    _numPOIs = 0;
    startAsyncPOI(fi);
}

// ── Snap cursor to nearest POI if within threshold pixels ────────────────
void GrapherApp::snapToPOI() {
    // Skip snap entirely during escape mode
    if (_snapEscapeCount > 0) return;

    if (_numPOIs == 0 || _traceFn < 0) {
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        return;
    }
    const float traceY = evalAt(_traceFn, _traceX);
    if (std::isnan(traceY) || std::isinf(traceY)) {
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        return;
    }
    const int traceSx = _view.worldToScreenX(_traceX);
    const int traceSy = _view.worldToScreenY(traceY);
    const float snapPx2 = POI_SNAP_THRESHOLD_PX * POI_SNAP_THRESHOLD_PX;
    float bestDist = snapPx2 + 1.0f;
    int bestIdx = -1;
    for (int i = 0; i < _numPOIs; ++i) {
        const int poiSx = _view.worldToScreenX(_pois[i].x);
        const int poiSy = _view.worldToScreenY(_pois[i].y);
        const float dx = (float)(traceSx - poiSx);
        const float dy = (float)(traceSy - poiSy);
        const float dist = dx * dx + dy * dy;
        if (dist <= snapPx2 && dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0) {
        _traceX = _pois[bestIdx].x;
        _snappedToPOI = true;
        _snappedPOIIdx = bestIdx;
    } else {
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
    }
}

// ── Strict 1:1 locked camera: viewport center is always (traceX, traceY) ─
void GrapherApp::syncViewportToCursor() {
    float xRange = _xMax - _xMin;
    float yRange = _yMax - _yMin;

    // Immediately lock viewport center to the traced point
    _xMin = _traceX - xRange / 2.0f;
    _xMax = _traceX + xRange / 2.0f;

    float traceY = (_traceFn >= 0) ? evalAt(_traceFn, _traceX) : NAN;
    if (!std::isnan(traceY) && !std::isinf(traceY)) {
        _yMin = traceY - yRange / 2.0f;
        _yMax = traceY + yRange / 2.0f;
    }

    _plotDirty = true;
    replot();
    // Recompute POIs for new viewport
    if (_traceFn >= 0) computePOIs(_traceFn);
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

    // Update sticky header: one centered label per column, aligned to the data
    // columns, with a separator between each adjacent pair. Columns past `cols`
    // (and their separators) stay hidden, so 1..MAX_FUNCS functions all read
    // correctly instead of leaving f2(x)+ unlabeled.
    if (_tblHeaderBar) {
        for (int c = 0; c < TBL_COLS; ++c) {
            if (!_tblHdrLabels[c]) continue;
            if (c < cols) {
                char hdrText[20];
                if (c == 0)
                    snprintf(hdrText, sizeof(hdrText), "x");
                else if (numActive > 0)
                    snprintf(hdrText, sizeof(hdrText), "f%d(x)", activeFuncs[c - 1] + 1);
                else
                    snprintf(hdrText, sizeof(hdrText), "f(x)");
                lv_label_set_text(_tblHdrLabels[c], hdrText);
                lv_obj_set_width(_tblHdrLabels[c], colW);
                lv_obj_set_pos(_tblHdrLabels[c], c * colW, (TBL_HDR_H - 16) / 2);
                lv_obj_remove_flag(_tblHdrLabels[c], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(_tblHdrLabels[c], LV_OBJ_FLAG_HIDDEN);
            }
        }
        for (int s = 0; s < TBL_COLS - 1; ++s) {
            if (!_tblHdrSeps[s]) continue;
            if (s < cols - 1) {
                lv_obj_set_pos(_tblHdrSeps[s], (s + 1) * colW, 0);
                lv_obj_remove_flag(_tblHdrSeps[s], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(_tblHdrSeps[s], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Row count: only data rows (no header row in table — sticky header handles it)
    int dataRows = TBL_ROWS;
    lv_table_set_row_count(_tblTable, dataRows);

    // Data rows
    for (int r = 0; r < dataRows; ++r) {
        if ((r & 15) == 0) yield();  // Feed watchdog

        float xVal = _tblStart + (_tblRow + r) * _tblStep;

        // X column
        char xBuf[16];
        snprintf(xBuf, sizeof(xBuf), "%.4g", (double)xVal);
        lv_table_set_cell_value(_tblTable, r, 0, xBuf);

        // Function value columns
        if (numActive > 0) {
            for (int c = 0; c < numActive && c + 1 < cols; ++c) {
                float yVal = evalAt(activeFuncs[c], xVal);
                char yBuf[16];
                if (std::isnan(yVal) || std::isinf(yVal))
                    snprintf(yBuf, sizeof(yBuf), "--");
                else
                    snprintf(yBuf, sizeof(yBuf), "%.6g", (double)yVal);
                lv_table_set_cell_value(_tblTable, r, c + 1, yBuf);
            }
        } else {
            lv_table_set_cell_value(_tblTable, r, 1, "--");
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
    case KeyCode::LEFT:
        // ── Visibility toggle (NumWorks feel) ─────────────────────────
        // LEFT on a function row toggles its visibility flag.
        // The colour dot dims to 30 % opacity when hidden.
        if (_exprIdx < _numFuncs) {
            _funcs[_exprIdx].visible = !_funcs[_exprIdx].visible;
            if (_exprDots[_exprIdx]) {
                lv_obj_set_style_bg_opa(_exprDots[_exprIdx],
                    _funcs[_exprIdx].visible ? LV_OPA_COVER : LV_OPA_30,
                    LV_PART_MAIN);
            }
            // Immediate replot if graph panel exists
            _plotDirty = true;
            if (_panelGraph) replot();
        }
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
        // Both the keypad minus (NEG) and the dedicated (−) sign key (NEGATE)
        // insert a minus; at expression start the parser reads it as unary, so
        // "−x" plots y=−x. NEGATE was previously unhandled here (a no-op).
        case KeyCode::NEG:
        case KeyCode::NEGATE: cur.insertOperator(OpKind::Sub); break;

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
        // Phase 9F: log with explicit base (log_n). Inserts a NodeLogBase (cursor
        // lands in the subscript). Serializes via change-of-base so it graphs.
        case KeyCode::LOG_BASE: if (cursorDepth() < MAX_NEST) cur.insertLogBase(); else changed = false; break;

        // ── Variables ──
        case KeyCode::VAR_X: cur.insertVariable('x'); break;
        case KeyCode::VAR_Y: cur.insertVariable('y'); break;

        // ── Equation sign (FREE_EQ = '=' key) ──
        // VPAM renders '=' via NodeOperator with OpKind::Eq (MathClass::REL for TeX spacing).
        case KeyCode::FREE_EQ: cur.insertVariable('='); break;

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
        case 2: // Pan (was Navigate)
            _focus = Focus::CONTENT;
            _grMode = GrMode::NAVIGATE;
            _plotDirty = true;
            replot();
            updateInfoBar();
            break;
        case 3: // Trace (was Calculate)
            _focus = Focus::CONTENT;
            _grMode = GrMode::TRACE;
            _traceX = (_xMin + _xMax) / 2.0f;
            _snappedToPOI = false;
            _snappedPOIIdx = -1;
            _snapEscapeCount = 0;
            // Pick first valid function
            _traceFn = -1;
            for (int i = 0; i < _numFuncs; ++i) {
                if (_funcs[i].valid) { _traceFn = i; break; }
            }
            // Pre-compute POIs now that we have a target function
            if (_traceFn >= 0) computePOIs(_traceFn);
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
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        _snapEscapeCount = 0;
        _traceFn = -1;
        for (int i = 0; i < _numFuncs; ++i) {
            if (_funcs[i].valid) { _traceFn = i; break; }
        }
        if (_traceFn >= 0) computePOIs(_traceFn);
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
    lv_refr_now(NULL);  // Force synchronous screen update — bypasses LVGL timer delay
}

// ── Graph trace keys ────────────────────────────────────────────────────
void GrapherApp::handleGraphTrace(const KeyEvent& ev) {
    float step = (_xMax - _xMin) / SCREEN_W;  // Pixel-precise cursor movement
    bool didSyncViewport = false;

    switch (ev.code) {
    case KeyCode::LEFT: {
        bool wasSnapped = _snappedToPOI;
        _traceX -= step;
        if (_traceX < _xMin) _traceX = _xMin;  // Clamp to viewport left edge

        if (wasSnapped) {
            // "Escape Force": break snap, ignore POI for next 10 moves
            _snappedToPOI = false;
            _snappedPOIIdx = -1;
            _snapEscapeCount = 10;
        } else if (_snapEscapeCount > 0) {
            --_snapEscapeCount;
        } else {
            snapToPOI();
        }
        syncViewportToCursor();
        didSyncViewport = true;
        break;
    }
    case KeyCode::RIGHT: {
        bool wasSnapped = _snappedToPOI;
        _traceX += step;
        if (_traceX > _xMax) _traceX = _xMax;

        if (wasSnapped) {
            _snappedToPOI = false;
            _snappedPOIIdx = -1;
            _snapEscapeCount = 10;
        } else if (_snapEscapeCount > 0) {
            --_snapEscapeCount;
        } else {
            snapToPOI();
        }
        syncViewportToCursor();
        didSyncViewport = true;
        break;
    }
    case KeyCode::UP:
        // Switch to next function (keep same X)
        for (int i = _traceFn + 1; i < _numFuncs; ++i) {
            if (_funcs[i].valid) { _traceFn = i; break; }
        }
        if (_traceFn >= 0) computePOIs(_traceFn);
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        if (_tangentActive && _traceFn >= 0) {
            _tangentFuncIdx = _traceFn;
            _tangentX = _traceX;
            _plotDirty = true;
            replot();
        }
        break;
    case KeyCode::DOWN:
        // Switch to prev function (keep same X)
        for (int i = _traceFn - 1; i >= 0; --i) {
            if (_funcs[i].valid) { _traceFn = i; break; }
        }
        if (_traceFn >= 0) computePOIs(_traceFn);
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        if (_tangentActive && _traceFn >= 0) {
            _tangentFuncIdx = _traceFn;
            _tangentX = _traceX;
            _plotDirty = true;
            replot();
        }
        break;
    case KeyCode::ENTER:
        // Open floating calculate menu (NumWorks-style)
        openCalcMenu();
        return;
    case KeyCode::AC:
        // Exit trace mode — hide crosshair + pill, return to toolbar
        clearIntegralShading();
        clearTangent();
        if (_traceDot)   lv_obj_add_flag(_traceDot, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineH) lv_obj_add_flag(_traceLineH, LV_OBJ_FLAG_HIDDEN);
        if (_traceLineV) lv_obj_add_flag(_traceLineV, LV_OBJ_FLAG_HIDDEN);
        if (_tracePill)  lv_obj_add_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
        _grMode = GrMode::NAVIGATE;  // AC from trace → pan mode
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        _snapEscapeCount = 0;
        updateInfoBar();
        return;
    default:
        break;
    }
    const float tangentRedrawDx = ((_xMax - _xMin) / SCREEN_W) * TANGENT_REDRAW_THRESHOLD_PIXELS;
    if (!didSyncViewport && _tangentActive &&
        (_tangentFuncIdx != _traceFn || fabsf(_tangentX - _traceX) > tangentRedrawDx)) {
        _tangentFuncIdx = _traceFn;
        _tangentX = _traceX;
        _plotDirty = true;
        replot();
    }
    drawTraceCursor();
    updateInfoBar();
    lv_refr_now(NULL);  // Force synchronous screen update — bypasses LVGL timer delay
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
    "Draw Tangent",
};

void GrapherApp::openCalcMenu() {
    if (_calcMenuOpen || !_graphArea) return;

    _calcMenuOpen = true;
    _calcMenuIdx = 0;

    // Create floating menu container centered on the graph area
    _calcMenu = lv_obj_create(_graphArea);
    if (!_calcMenu) { _calcMenuOpen = false; return; }

    // The graph area is only ~143px tall (SCREEN_H - BAR_H - TAB_H - TOOLBAR_H -
    // INFO_BAR_H). The old 28px/row + 16 padding made a 184px menu that overflowed
    // into the toolbar and info bar and hid the 6th item. 20px rows + 12 padding
    // keep all CALC_MENU_ITEMS on-screen inside the graph area.
    int menuW = 180;
    int menuH = CALC_MENU_ITEMS * 20 + 12;
    int areaW = lv_obj_get_width(_graphArea);
    int areaH = lv_obj_get_height(_graphArea);

    lv_obj_set_size(_calcMenu, menuW, menuH);
    lv_obj_set_pos(_calcMenu, (areaW - menuW) / 2, (areaH - menuH) / 2);
    lv_obj_set_style_bg_color(_calcMenu, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_calcMenu, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(_calcMenu, lv_color_hex(0x4A90E2), LV_PART_MAIN);
    lv_obj_set_style_border_width(_calcMenu, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_calcMenu, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_calcMenu, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(_calcMenu, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_calcMenu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(_calcMenu, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_flex_flow(_calcMenu, LV_FLEX_FLOW_COLUMN);

    for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
        lv_obj_t* row = lv_obj_create(_calcMenu);
        _calcMenuRows[i] = row;
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 20);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(row, lv_color_hex(i == 0 ? COL_BTN_BG : 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, CALC_MENU_LABELS[i]);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_hex(i == 0 ? 0xFFFFFF : 0x333333), LV_PART_MAIN);
        // montserrat_14: every CALC_MENU_LABELS entry contains a space ("Find
        // Root", "Draw Tangent", ...) and stix_math_18 has no U+0020 glyph, so it
        // tofu'd at each space AND clipped vertically in the 24px rows.
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// POI computation: async + magnetic snapping
// ═══════════════════════════════════════════════════════════════════════

void GrapherApp::startAsyncPOI(int fi) {
    // Start POI computation for all functions, one function per timer tick.
    if (fi < 0 || fi >= _numFuncs) fi = 0;

    if (_poiAsyncTimer) {
        lv_timer_delete(_poiAsyncTimer);
        _poiAsyncTimer = nullptr;
    }

    _poiAsyncFi = fi;
    _numPOIs = 0;

    // 10ms slicing keeps UI responsive while progressively adding POIs.
    _poiAsyncTimer = lv_timer_create(poiAsyncTimerCb, 10, (void*)this);
}

void GrapherApp::poiAsyncTimerCb(lv_timer_t* t) {
    GrapherApp* app = (GrapherApp*)lv_timer_get_user_data(t);
    if (!app) return;

    if (app->_poiAsyncFi >= app->_numFuncs || app->_numPOIs >= MAX_POIS) {
        lv_timer_delete(t);
        app->_poiAsyncTimer = nullptr;
        app->_poiAsyncFi = -1;
        app->_plotDirty = true;
        return;
    }

    const int fi = app->_poiAsyncFi;
    app->_poiAsyncFi++;
    if (fi >= 0 && fi < app->_numFuncs && app->_funcs[fi].valid && app->_funcs[fi].visible) {
        app->appendPOIsForFunction(fi);
        app->_plotDirty = true;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Calculate Menu (continued from earlier in file)
// ═══════════════════════════════════════════════════════════════════════

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
        return (double)evalAt(_traceFn, (float)x);
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
            return (double)evalAt(otherFn, (float)x);
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
            _plotDirty = true;
            replot();
        }
        break;
    }
    case 5: { // Draw Tangent
        drawTangentOverlay(_traceFn, _traceX);
        _plotDirty = true;
        replot();
        snprintf(pillBuf, sizeof(pillBuf), "Tangent at x=%.4f", (double)_traceX);
        res.found = true;
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
    if (!_graphBuf || funcIdx < 0 || funcIdx >= _numFuncs || !_funcs[funcIdx].valid)
        return;

    int areaW = GRAPH_CANVAS_W;
    int areaH = GRAPH_CANVAS_H;
    if (areaW < 2 || areaH < 2) return;

    float xRange = (float)(_xMax - _xMin);
    float yRange = (float)(_yMax - _yMin);
    if (xRange <= 0.0f || yRange <= 0.0f) return;

    if (shadeXMax < shadeXMin) {
        float tmp = shadeXMin;
        shadeXMin = shadeXMax;
        shadeXMax = tmp;
    }

    const int sx0 = std::max(0, std::min(areaW - 1, _view.worldToScreenX(shadeXMin)));
    const int sx1 = std::max(0, std::min(areaW - 1, _view.worldToScreenX(shadeXMax)));
    const int left = std::min(sx0, sx1);
    const int right = std::max(sx0, sx1);

    const int yAxisPx = _view.worldToScreenY(0.0f);
    const uint16_t shade565 = utils::rgb888to565(_funcs[funcIdx].color);

    for (int px = left; px <= right; ++px) {
        const float wx = (float)_xMin + ((float)px / (float)areaW) * xRange;
        if (wx < shadeXMin || wx > shadeXMax) continue;

        const float wy = (float)evalAt(funcIdx, wx);
        if (std::isnan(wy) || std::isinf(wy)) continue;

        int sy = _view.worldToScreenY(wy);
        if (sy < 0) sy = 0;
        if (sy >= areaH) sy = areaH - 1;
        int y0 = yAxisPx;
        if (y0 < 0) y0 = 0;
        if (y0 >= areaH) y0 = areaH - 1;

        const int top = std::min(sy, y0);
        const int bot = std::max(sy, y0);
        for (int py = top; py <= bot; ++py) {
            if (!useStipplePixel(px, py)) continue;  // checkerboard stipple
            _graphBuf[py * areaW + px] = shade565;
        }
    }

    _shadingActive = true;
    _shadingFuncIdx = funcIdx;
    _shadingX0 = shadeXMin;
    _shadingX1 = shadeXMax;
}

void GrapherApp::clearIntegralShading() {
    _shadingActive = false;
    _shadingFuncIdx = -1;
    _shadingX0 = 0.0f;
    _shadingX1 = 0.0f;
}

void GrapherApp::drawTangentOverlay(int funcIdx, float xTarget) {
    if (!_graphBuf || funcIdx < 0 || funcIdx >= _numFuncs || !_funcs[funcIdx].valid) return;
    const float yTarget = (float)evalAt(funcIdx, xTarget);
    if (std::isnan(yTarget) || std::isinf(yTarget)) return;

    const float xRange = (float)(_xMax - _xMin);
    const float hPixel = xRange / (float)GRAPH_CANVAS_W;
    const float h = std::max(hPixel, xRange * TANGENT_FD_STEP_RATIO);  // finite-difference step size
    if (h <= 0.0f) return;

    const float yPlus = (float)evalAt(funcIdx, xTarget + h);
    const float yMinus = (float)evalAt(funcIdx, xTarget - h);
    if (std::isnan(yPlus) || std::isinf(yPlus) || std::isnan(yMinus) || std::isinf(yMinus)) return;

    const float derivative = (yPlus - yMinus) / (2.0f * h);  // Central difference
    if (std::isnan(derivative) || std::isinf(derivative)) return;
    const float xLeft = (float)_xMin;
    const float xRight = (float)_xMax;
    const float yLeft = yTarget + derivative * (xLeft - xTarget);
    const float yRight = yTarget + derivative * (xRight - xTarget);
    if (std::isnan(yLeft) || std::isinf(yLeft) || std::isnan(yRight) || std::isinf(yRight)) return;

    _view.drawFunctionSegment(xLeft, yLeft, xRight, yRight, _funcs[funcIdx].color);
    _tangentActive = true;
    _tangentFuncIdx = funcIdx;
    _tangentX = xTarget;
}

void GrapherApp::clearTangent() {
    _tangentActive = false;
    _tangentFuncIdx = -1;
    _tangentX = 0.0f;
}

