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
static constexpr uint32_t COL_ROW_INV   = 0xCC3333;  // Invalid pill border (red)

// Compose the user-visible refusal string for an invalid slot ("one-sided
// relation", "unknown: xy", ...). Canonical strings frozen by the classifier
// contract; scripts assert them by substring.
static void formatInvalidReason(const grapher::CartesianFunction& f, char* out, size_t n) {
    if (f.invalidReason == grapher::InvalidReason::UnknownIdent && f.reasonDetail[0])
        snprintf(out, n, "unknown: %s", f.reasonDetail);
    else
        snprintf(out, n, "%s", grapher::invalidReasonText(f.invalidReason));
}
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
    , _shadingActive(false), _shadingFuncIdx(-1), _shadingX0(0.0f), _shadingX1(0.0f), _shadingAlongY(false)
    , _tangentActive(false), _tangentFuncIdx(-1), _tangentX(0.0f), _tangentY(0.0f)
    , _tblTable(nullptr)
    , _tab(Tab::EXPRESSIONS), _focus(Focus::TAB_BAR), _tabIdx(0)
    , _numFuncs(0), _exprIdx(0), _exprMode(ExprMode::LIST)
    , _grMode(GrMode::IDLE), _toolIdx(0)
    , _xMin(-10.0f), _xMax(10.0f), _yMin(-7.0f), _yMax(7.0f)
    , _traceX(0.0f), _traceY(0.0f), _traceFn(0)
    , _traceValid(false), _traceSeeded(false), _traceHeadX(0.0f), _traceHeadY(0.0f)
    , _plotDirty(true)
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
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) { _calcMenuRows[i] = nullptr; _calcMenuEnabled[i] = false; }
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
    // Single shared step for BOTH axes (same one the grid rasterizer uses), so
    // tick labels sit on grid lines and, with the equal-aspect viewport, do not
    // pile up — previously the Y labels used a denser 6-tick step and overlapped.
    const float mainStep = grapher::GraphView::squareGridStep(xRange / (float)aW);
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

    // Y-axis tick labels — SAME step as X (square grid → labels on grid lines).
    const float yMainStep = mainStep;
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
            if (_panelGraph) {
                lv_obj_remove_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
                // Default to TRACE on ANY traceable curve — y=f(x), x=f(y), or a
                // general implicit equation. Only a graph with no traceable curve
                // (inequality / region only) opens in PAN. The camera then locks
                // onto the control point for every kind (syncViewportToCursor).
                _traceFn = firstTraceableFunc(0, +1);
                _grMode  = (_traceFn >= 0) ? GrMode::TRACE : GrMode::NAVIGATE;
                _traceX = (_xMin + _xMax) / 2.0f;
                _traceY = (_yMin + _yMax) / 2.0f;
                _traceSeeded = false;
                _traceHeadX = 0.0f; _traceHeadY = 0.0f;
                _plotDirty = true;
                _snappedToPOI = false;
                _snappedPOIIdx = -1;
                _snapEscapeCount = 0;
                // Force the LVGL layout pass NOW so _graphArea has real dimensions,
                // then plot the curve immediately. Without this the first entry to
                // the Graph tab — including via the "Plot graph" button — shows an
                // empty graph (just axes) until the user presses a key, because
                // replot() bails while lv_obj_get_height(_graphArea) is still 0.
                lv_obj_update_layout(_panelGraph);
                replot();
                // y=f(x) keeps its long-standing deferred cursor/POIs (drawn on the
                // first trace key, so the legacy goldens are untouched). x=f(y) and
                // implicit curves seed an on-curve point and centre the camera on it
                // NOW, so the graph opens in a centred Trace instead of Pan.
                if (_grMode == GrMode::TRACE && traceKind(_traceFn) != TraceKind::ExplicitX) {
                    ensureTraceSeed();
                    if (_traceValid) syncViewportToCursor();
                    drawTraceCursor();
                }
            }
        } else {
            lv_obj_remove_flag(_panelGraph, LV_OBJ_FLAG_HIDDEN);
            // See lazy-create branch above: auto-trace ANY traceable curve.
            _traceFn = firstTraceableFunc(0, +1);
            _grMode  = (_traceFn >= 0) ? GrMode::TRACE : GrMode::NAVIGATE;
            _traceX = (_xMin + _xMax) / 2.0f;
            _traceY = (_yMin + _yMax) / 2.0f;
            _traceSeeded = false;
            _traceHeadX = 0.0f; _traceHeadY = 0.0f;
            _plotDirty = true;
            _snappedToPOI = false;
            _snappedPOIIdx = -1;
            _snapEscapeCount = 0;
            replot();
            if (_grMode == GrMode::TRACE) {
                ensureTraceSeed();
                // x=f(y) / implicit lock the camera on the seed; y=f(x) keeps the
                // current view and just precomputes POIs (camera follows on step).
                if (traceKind(_traceFn) != TraceKind::ExplicitX && _traceValid)
                    syncViewportToCursor();
                else
                    computePOIs(_traceFn);
                drawTraceCursor();
            }
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
            // Invalid committed slots (len>0, rejected by the classifier) show a
            // red border so a refused expression is VISIBLY different from a
            // plotted one. Valid and empty slots keep the legacy gray border, so
            // every existing golden (valid slots only) stays byte-identical.
            const bool invalid = _funcs[i].len > 0 && !_funcs[i].valid;
            lv_obj_set_style_bg_color(_exprRows[i], lv_color_hex(COL_ROW_BG), LV_PART_MAIN);
            lv_obj_set_style_border_color(_exprRows[i],
                lv_color_hex(invalid ? COL_ROW_INV : COL_ROW_BRD), LV_PART_MAIN);
            lv_obj_set_style_border_width(_exprRows[i], invalid ? 2 : 1, LV_PART_MAIN);
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

    // Selected-row refusal hint: when the focused slot was rejected, the hint
    // bar says WHY ("Invalid: one-sided relation", "Invalid: unknown: xy") so
    // the refusal is explicit, not a mysteriously blank plot. On valid/other
    // rows the hint reverts to the LIST default. Only in LIST mode (the editor
    // owns the hint while editing).
    if (_exprHint && _exprMode == ExprMode::LIST) {
        if (_exprIdx < _numFuncs && _funcs[_exprIdx].len > 0 && !_funcs[_exprIdx].valid) {
            char reason[32];
            formatInvalidReason(_funcs[_exprIdx], reason, sizeof(reason));
            char hint[64];
            snprintf(hint, sizeof(hint), "Invalid: %s  ENTER=edit", reason);
            lv_label_set_text(_exprHint, hint);
        } else {
            lv_label_set_text(_exprHint, "ENTER=edit  AC=back");
        }
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

    // Shift down data (move assignment, NOT memcpy — FuncSlot has
    // std::vector members and, since GIAC-C01, move-only compiled handles)
    for (int i = idx; i < _numFuncs - 1; ++i) {
        _funcs[i] = std::move(_funcs[i + 1]);
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
// s_serializeUnsupported is set when serialization meets an AST node the
// numeric pipeline cannot express (DefIntegral, Summation, Subscript, BigOp,
// PeriodicDecimal). Previously such nodes were silently DROPPED, so the slot
// compiled a different expression than the one on screen. The flag turns that
// into a visible "unsupported node" rejection (classifier contract R14).
static bool s_serializeUnsupported = false;
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
            break;
        default:
            // AST node with no textual form in the numeric pipeline — flag it
            // instead of silently dropping it (would plot a DIFFERENT function
            // than the one displayed).
            s_serializeUnsupported = true;
            break;
    }
}

void GrapherApp::syncASTtoText(int idx) {
    if (idx < 0 || idx >= MAX_FUNCS) return;
    FuncSlot& f = _funcs[idx];

    // Serialize into an oversized scratch first (classifier contract R13): the
    // slot's text[] holds at most 63 chars, and the old direct-serialize
    // silently TRUNCATED longer expressions — which then compiled and plotted a
    // wrong curve. Overflow now records a TooLong verdict that preCacheRPN
    // turns into a visible "expression too long" rejection.
    char scratch[128];
    int pos = 0;
    s_serializeUnsupported = false;
    if (_exprASTRow[idx]) {
        serializeRow(_exprASTRow[idx], scratch, pos, (int)sizeof(scratch) - 1);
    }
    scratch[pos] = '\0';

    f.serializeVerdict = grapher::InvalidReason::None;
    if (s_serializeUnsupported) {
        f.serializeVerdict = grapher::InvalidReason::Unsupported;
    } else if (pos > 63) {
        f.serializeVerdict = grapher::InvalidReason::TooLong;
    }

    // Copy (possibly clamped) text for display/diagnostics; when a verdict is
    // set the text is never classified or compiled (preCacheRPN early-outs).
    int n = pos > 63 ? 63 : pos;
    memcpy(f.text, scratch, (size_t)n);
    f.text[n] = '\0';
    f.len = n;
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

    // Refusal telemetry: one greppable line per rejected commit, so scripted
    // runs (and users watching the serial log) see WHY a slot will not plot.
    if (!_funcs[idx].valid) {
        char reason[32];
        formatInvalidReason(_funcs[idx], reason, sizeof(reason));
        Serial.printf("[GRAPH] invalid slot=%d reason=%s\n", idx, reason);
    }
}

float GrapherApp::evalAt(int idx, float x) {
    if (idx < 0 || idx >= _numFuncs || !_funcs[idx].valid) return NAN;
    // Delegate to GraphModel for evaluation
    return _model.evalAt(_funcs[idx], x);
}

int GrapherApp::firstTraceableFunc(int from, int dir) const {
    for (int i = from; i >= 0 && i < _numFuncs; i += dir) {
        if (_funcs[i].isTraceable() && _funcs[i].visible) return i;
    }
    return -1;
}

int GrapherApp::firstExplicitXFunc(int from, int dir) const {
    for (int i = from; i >= 0 && i < _numFuncs; i += dir) {
        if (_funcs[i].isExplicit() && _funcs[i].visible) return i;
    }
    return -1;
}

// ── Unified trace cursor ───────────────────────────────────────────────────
//
// Three curve kinds share one cursor (_traceX,_traceY) + validity flag:
//   ExplicitX  y=f(x): LEFT/RIGHT step x by one pixel; y = f(x). Locked camera
//                      + POI snap (unchanged legacy behaviour).
//   ExplicitY  x=f(y): LEFT/RIGHT step y by one pixel; x = f(y) exactly.
//   Implicit   G=0   : LEFT/RIGHT advance along the contour by a fixed arc-length
//                      (predictor along the tangent ⟂∇G, Newton corrector back
//                      onto G=0). A persistent heading keeps RIGHT/LEFT consistent
//                      around closed curves.
// ExplicitY/Implicit keep the viewport fixed and only pan when the cursor nears
// an edge, so a circle stays still while the cursor walks around it.

GrapherApp::TraceKind GrapherApp::traceKind(int fi) const {
    if (fi < 0 || fi >= _numFuncs) return TraceKind::None;
    const FuncSlot& f = _funcs[fi];
    if (!f.isTraceable() || !f.visible) return TraceKind::None;
    if (!f.implicit)  return TraceKind::ExplicitX;   // y = f(x)
    if (f.explicitY)  return TraceKind::ExplicitY;   // x = f(y)
    return TraceKind::Implicit;                      // general G(x,y) = 0
}

// Finite-difference gradient ∇G = (∂G/∂x, ∂G/∂y) at (x,y), central difference
// over ~one pixel. Returns false if any sample lands on a domain hole (NaN).
bool GrapherApp::gradImplicit(int fi, float x, float y, float& gx, float& gy) const {
    const int   bw = _view.bufW() > 1 ? _view.bufW() : SCREEN_W;
    const int   bh = _view.bufH() > 1 ? _view.bufH() : GRAPH_CANVAS_H;
    float hx = (_xMax - _xMin) / (float)bw;
    float hy = (_yMax - _yMin) / (float)bh;
    if (!(hx > 0.0f)) hx = 1e-4f;
    if (!(hy > 0.0f)) hy = 1e-4f;
    grapher::GraphModel& m = const_cast<grapher::GraphModel&>(_model);
    FuncSlot&   f = const_cast<FuncSlot&>(_funcs[fi]);
    float gpx = m.evalImplicit(f, x + hx, y);
    float gnx = m.evalImplicit(f, x - hx, y);
    float gpy = m.evalImplicit(f, x, y + hy);
    float gny = m.evalImplicit(f, x, y - hy);
    if (std::isnan(gpx) || std::isnan(gnx) || std::isnan(gpy) || std::isnan(gny))
        return false;
    gx = (gpx - gnx) / (2.0f * hx);
    gy = (gpy - gny) / (2.0f * hy);
    if (std::isnan(gx) || std::isnan(gy) || std::isinf(gx) || std::isinf(gy))
        return false;
    return true;
}

// Newton corrector: project (x,y) onto the nearest point of G(x,y)=0 along the
// gradient. Converges quadratically near the curve. Returns true when the final
// residual distance |G|/|∇G| is sub-pixel-small; false on divergence/domain holes.
bool GrapherApp::correctOntoCurve(int fi, float& x, float& y) const {
    const float scale = _xMax - _xMin;
    grapher::GraphModel& m = const_cast<grapher::GraphModel&>(_model);
    FuncSlot&   f = const_cast<FuncSlot&>(_funcs[fi]);
    for (int it = 0; it < TRACE_NEWTON_ITERS; ++it) {
        float g = m.evalImplicit(f, x, y);
        if (std::isnan(g) || std::isinf(g)) return false;
        float gx, gy;
        if (!gradImplicit(fi, x, y, gx, gy)) return false;
        float gm2 = gx * gx + gy * gy;
        if (gm2 < 1e-20f) return false;          // singular gradient
        float dx = g * gx / gm2;
        float dy = g * gy / gm2;
        x -= dx;
        y -= dy;
        if (std::sqrt(dx * dx + dy * dy) < scale * 1e-4f) break;
    }
    float g = m.evalImplicit(f, x, y);
    if (std::isnan(g) || std::isinf(g)) return false;
    float gx, gy;
    if (!gradImplicit(fi, x, y, gx, gy)) return false;
    float gm = std::sqrt(gx * gx + gy * gy);
    if (gm < 1e-12f) return false;
    return (std::fabs(g) / gm) < scale * 1e-3f;  // within ~0.1% of viewport width
}

// Recompute the unit tangent (⟂∇G) at (x,y) and orient it consistently: align
// with the previous heading so RIGHT keeps circling the same way; on the first
// call (no heading yet) prefer the +x direction so RIGHT initially moves right.
void GrapherApp::updateImplicitHeading(int fi, float x, float y) {
    float gx, gy;
    if (!gradImplicit(fi, x, y, gx, gy)) return;
    float tx = gy, ty = -gx;                     // tangent ⟂ gradient
    float m = std::sqrt(tx * tx + ty * ty);
    if (m < 1e-12f) return;
    tx /= m; ty /= m;
    const bool havePrev = !(_traceHeadX == 0.0f && _traceHeadY == 0.0f);
    const float ref = havePrev ? (tx * _traceHeadX + ty * _traceHeadY) : tx;
    if (ref < 0.0f) { tx = -tx; ty = -ty; }
    _traceHeadX = tx;
    _traceHeadY = ty;
}

// Scan the viewport on a coarse grid for a G sign change, pick the crossing
// nearest the screen centre, refine it with the Newton corrector, and seed the
// cursor + heading there. Returns false if the contour is not in view.
bool GrapherApp::seedImplicitTrace() {
    const int bw = _view.bufW();
    const int bh = _view.bufH();
    if (bw < 2 || bh < 2) return false;

    constexpr int GX = 48, GY = 36;              // seed-scan grid resolution
    std::vector<float> g((size_t)(GX + 1) * (GY + 1));
    auto pxAt = [&](int i) { return (int)((float)i / (float)GX * (float)(bw - 1)); };
    auto pyAt = [&](int j) { return (int)((float)j / (float)GY * (float)(bh - 1)); };

    grapher::GraphModel& m = _model;
    for (int j = 0; j <= GY; ++j) {
        float wy = _view.screenToWorldY(pyAt(j));
        for (int i = 0; i <= GX; ++i) {
            float wx = _view.screenToWorldX(pxAt(i));
            g[(size_t)j * (GX + 1) + i] = m.evalImplicit(_funcs[_traceFn], wx, wy);
        }
    }

    const float cxp = (float)bw * 0.5f, cyp = (float)bh * 0.5f;
    bool found = false;
    float best = 1e30f, bx = 0.0f, by = 0.0f;
    auto consider = [&](float va, float vb, int xa, int ya, int xb, int yb) {
        if (std::isnan(va) || std::isnan(vb)) return;
        if ((va <= 0.0f) == (vb <= 0.0f)) return;       // no sign change on this edge
        float t = va / (va - vb);
        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
        float sx = (float)xa + t * (float)(xb - xa);
        float sy = (float)ya + t * (float)(yb - ya);
        float dx = sx - cxp, dy = sy - cyp;
        float d = dx * dx + dy * dy;
        if (d < best) {
            best = d;
            bx = _view.screenToWorldX((int)(sx + 0.5f));
            by = _view.screenToWorldY((int)(sy + 0.5f));
            found = true;
        }
    };
    for (int j = 0; j <= GY; ++j) {
        for (int i = 0; i <= GX; ++i) {
            float v = g[(size_t)j * (GX + 1) + i];
            if (i < GX)
                consider(v, g[(size_t)j * (GX + 1) + (i + 1)], pxAt(i), pyAt(j), pxAt(i + 1), pyAt(j));
            if (j < GY)
                consider(v, g[(size_t)(j + 1) * (GX + 1) + i], pxAt(i), pyAt(j), pxAt(i), pyAt(j + 1));
        }
    }
    if (!found) return false;

    float x = bx, y = by;
    if (!correctOntoCurve(_traceFn, x, y)) { x = bx; y = by; }
    _traceX = x;
    _traceY = y;
    _traceHeadX = 0.0f;                            // reset → prefer +x heading
    _traceHeadY = 0.0f;
    updateImplicitHeading(_traceFn, x, y);
    return true;
}

// Compute the initial on-curve point for the active slot/kind (idempotent until
// reset). Sets _traceValid; for ExplicitX/Y the parameter (_traceX or _traceY)
// is supplied by the caller and the dependent coordinate is filled in here.
void GrapherApp::ensureTraceSeed() {
    if (_traceSeeded) return;
    _traceSeeded = true;
    _traceValid  = false;
    switch (traceKind(_traceFn)) {
    case TraceKind::ExplicitX: {
        float yv = evalAt(_traceFn, _traceX);
        _traceValid = !(std::isnan(yv) || std::isinf(yv));
        // Never store a non-finite coordinate: a NaN _traceY poisons every
        // consumer that centres the viewport on the cursor (zoom-while-tracing
        // y=1/x → NaN _yMin/_yMax → blank canvas, GRBUG-001). Keep the view
        // centre as a finite fallback when f is off-domain at this x.
        _traceY = _traceValid ? yv : (_yMin + _yMax) * 0.5f;
        break;
    }
    case TraceKind::ExplicitY: {
        float xv = _model.evalAtY(_funcs[_traceFn], _traceY);
        _traceValid = !(std::isnan(xv) || std::isinf(xv));
        _traceX = _traceValid ? xv : (_xMin + _xMax) * 0.5f;   // see ExplicitX note
        break;
    }
    case TraceKind::Implicit:
        _traceValid = seedImplicitTrace();
        break;
    default:
        break;
    }
}

// Walk the active curve one step. dir = -1 (LEFT) / +1 (RIGHT).
void GrapherApp::traceStep(int dir) {
    ensureTraceSeed();
    switch (traceKind(_traceFn)) {
    case TraceKind::ExplicitX: {
        // Legacy y=f(x): pixel-precise x move, POI snap, locked camera.
        const float step = (_xMax - _xMin) / SCREEN_W;
        const bool wasSnapped = _snappedToPOI;
        _traceX += (float)dir * step;
        if (_traceX < _xMin) _traceX = _xMin;
        if (_traceX > _xMax) _traceX = _xMax;
        if (wasSnapped) {
            _snappedToPOI = false; _snappedPOIIdx = -1; _snapEscapeCount = 10;
        } else if (_snapEscapeCount > 0) {
            --_snapEscapeCount;
        } else {
            snapToPOI();
        }
        syncViewportToCursor();
        float yv = evalAt(_traceFn, _traceX);
        _traceValid = !(std::isnan(yv) || std::isinf(yv));
        // Off-domain step (pole/domain hole): keep _traceY finite so a later
        // zoom-about-the-cursor cannot poison the viewport (GRBUG-001).
        _traceY = _traceValid ? yv : (_yMin + _yMax) * 0.5f;
        break;
    }
    case TraceKind::ExplicitY: {
        // x=f(y): pixel-precise y move along the curve, exact x = f(y).
        const float stepY = (_yMax - _yMin) / SCREEN_H;
        _traceY += (float)dir * stepY;
        float xv = _model.evalAtY(_funcs[_traceFn], _traceY);
        _traceValid = !(std::isnan(xv) || std::isinf(xv));
        _traceX = _traceValid ? xv : (_xMin + _xMax) * 0.5f;   // see ExplicitX note
        if (_traceValid) syncViewportToCursor();   // lock camera on the control point
        break;
    }
    case TraceKind::Implicit: {
        if (!_traceValid) {                       // (re)seed if we lost the curve
            _traceSeeded = false;
            ensureTraceSeed();
            if (!_traceValid) break;
        }
        const float pxw = (_xMax - _xMin) / SCREEN_W;
        const float h = TRACE_IMPLICIT_STEP_PX * pxw;     // arc-length per press
        float nx = _traceX + (float)dir * _traceHeadX * h;
        float ny = _traceY + (float)dir * _traceHeadY * h;
        if (correctOntoCurve(_traceFn, nx, ny)) {
            updateImplicitHeading(_traceFn, nx, ny);
            _traceX = nx; _traceY = ny; _traceValid = true;
            syncViewportToCursor();                // lock camera on the control point
        }
        break;
    }
    default:
        break;
    }
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
    // Discontinuity guard: a segment whose endpoints span more than 3× the visible
    // y-range is almost certainly straddling an asymptote (tan, 1/x, log near 0)
    // rather than a steep-but-continuous arc. Recursion isolates the jump into a
    // tiny sub-segment which this test then drops, so the asymptote no longer
    // smears as a solid vertical line while the real branches on either side draw.
    const float yJump = (_yMax - _yMin) * 3.0f;
    if (depth >= ADAPT_DEPTH) {
        if (std::fabs(wy1 - wy0) <= yJump)
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

    if (std::fabs(wy1 - wy0) <= yJump)
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

    // GRBUG-001: a sample landing ON a pole / domain edge (y=1/x at x=0, the
    // viewport centre) evaluates non-finite, and both adjacent coarse segments
    // used to be dropped whole — visibly amputating the steep near-pole branch
    // (a gap up to 2·step wide). On an ok↔non-finite transition, bisect toward
    // the edge to find the last representable on-curve point and draw the
    // finite-side extension, clamping the divergent end to just outside the
    // view so the branch runs off-screen like a real asymptote. The clamp also
    // keeps screen coordinates small (no megapixel Bresenham walks). This never
    // BRIDGES the pole — each side extends independently — so tan()-style
    // asymptotes still cannot smear into vertical connector lines.
    auto extendToEdge = [&](float xa, float ya, float xb) {
        float lo = xa, ylo = ya, hi = xb;
        for (int k = 0; k < 12; ++k) {
            const float mid = (lo + hi) * 0.5f;
            const float ym  = (float)evalAt(fi, mid);
            if (!std::isnan(ym) && !std::isinf(ym)) { lo = mid; ylo = ym; }
            else                                    { hi = mid; }
        }
        float yEnd = ylo;
        if (yEnd > _yMax + yRange) yEnd = _yMax + yRange;
        if (yEnd < _yMin - yRange) yEnd = _yMin - yRange;
        _view.drawFunctionSegment(xa, ya, lo, yEnd, color);
    };

    for (int i = 1; i <= INIT_SAMPLE_N; ++i) {
        const float wx1 = (i == INIT_SAMPLE_N) ? (float)_xMax : ((float)_xMin + step * (float)i);
        const float wy1 = (float)evalAt(fi, wx1);
        const bool currOk = (!std::isnan(wy1) && !std::isinf(wy1));
        if (prevOk && currOk) {
            adaptSegStream(fi, wx0, wy0, wx1, wy1, 0, color);
        } else if (prevOk && !currOk) {
            extendToEdge(wx0, wy0, wx1);      // branch ends at a pole/domain edge
        } else if (!prevOk && currOk) {
            extendToEdge(wx1, wy1, wx0);      // branch begins at a pole/domain edge
        }
        wx0 = wx1;
        wy0 = wy1;
        prevOk = currOk;
        if ((i & 7) == 0) yield();
    }
}

// ── Implicit-equation plotting (marching squares) ─────────────────────────
//
// For an implicit relation F(x,y)=0 (stored as G(x,y)=lhs-rhs), sample G on a
// coarse pixel grid over the viewport, then run marching squares per cell to
// draw the G=0 contour. This handles multi-valued curves the single-valued
// adaptive sampler cannot: x=y^2 (sideways parabola), x^2+y^2=1 (circle),
// y=x^2+y^2 (a circle), etc. Cells touching a NaN corner (domain holes) are
// skipped so discontinuities don't smear.
void GrapherApp::plotImplicit(int fi, uint32_t color) {
    const int W = GRAPH_CANVAS_W;
    const int H = GRAPH_CANVAS_H;
    if (W < 2 || H < 2) return;

    // Grid step in pixels: smaller = smoother contour, more evals. 3px keeps
    // curves visually clean while bounding work to ~(W/3)·(H/3) evaluations.
    constexpr int CELL = 3;
    const int nx = W / CELL + 1;
    const int ny = H / CELL + 1;

    // Sample G at each grid node (node n maps to pixel n*CELL, clamped to edge).
    std::vector<float> g((size_t)nx * (size_t)ny);
    auto pxAt = [&](int i) { return (i == nx - 1) ? (W - 1) : (i * CELL); };
    auto pyAt = [&](int j) { return (j == ny - 1) ? (H - 1) : (j * CELL); };

    for (int j = 0; j < ny; ++j) {
        const float wy = _view.screenToWorldY(pyAt(j));
        for (int i = 0; i < nx; ++i) {
            const float wx = _view.screenToWorldX(pxAt(i));
            g[(size_t)j * nx + i] = _model.evalImplicit(_funcs[fi], wx, wy);
        }
        if ((j & 3) == 0) yield();
    }

    // Linear-interpolate the zero crossing on an edge between two node values.
    auto cross = [](float va, float vb, int a, int b) -> int {
        float denom = va - vb;
        float t = (denom != 0.0f) ? (va / denom) : 0.5f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return a + (int)((float)(b - a) * t + 0.5f);
    };

    // ── Inequality region shading setup ──────────────────────────────────
    // For an inequality slot we shade the solution half (G≷0) with a 50 %
    // stipple of a lightened function colour, then draw the same G=0 boundary
    // on top. Whole-inside cells fill from the grid corners (no extra evals);
    // boundary cells feather per-pixel so the shade meets the curve crisply.
    const grapher::Relation rel = _funcs[fi].relation;
    const bool inequality = _funcs[fi].isInequality();
    uint32_t tint = color;
    if (inequality) {
        const uint32_t r = (color >> 16) & 0xFF, gc = (color >> 8) & 0xFF, b = color & 0xFF;
        auto mix = [](uint32_t c) { return (uint32_t)(c + (255 - c) * 55 / 100); };  // 55% → white
        tint = (mix(r) << 16) | (mix(gc) << 8) | mix(b);
    }

    for (int j = 0; j + 1 < ny; ++j) {
        for (int i = 0; i + 1 < nx; ++i) {
            const float v00 = g[(size_t)j * nx + i];           // top-left  (A)
            const float v10 = g[(size_t)j * nx + i + 1];       // top-right (B)
            const float v11 = g[(size_t)(j + 1) * nx + i + 1]; // bot-right (C)
            const float v01 = g[(size_t)(j + 1) * nx + i];     // bot-left  (D)

            const int x0 = pxAt(i),     x1 = pxAt(i + 1);
            const int y0 = pyAt(j),     y1 = pyAt(j + 1);

            if (inequality) {
                const int inCount = (grapher::regionHolds(rel, v00) ? 1 : 0) +
                                    (grapher::regionHolds(rel, v10) ? 1 : 0) +
                                    (grapher::regionHolds(rel, v11) ? 1 : 0) +
                                    (grapher::regionHolds(rel, v01) ? 1 : 0);
                const bool anyNan = std::isnan(v00) || std::isnan(v10) ||
                                    std::isnan(v11) || std::isnan(v01);
                if (inCount == 4 && !anyNan) {
                    _view.fillRectStipple(x0, y0, x1, y1, tint);
                } else if (inCount > 0 || anyNan) {
                    // Mixed / domain-edge cell: test each pixel of the cell.
                    for (int py = y0; py <= y1; ++py) {
                        const float wy = _view.screenToWorldY(py);
                        for (int px = x0; px <= x1; ++px) {
                            const float wx = _view.screenToWorldX(px);
                            if (grapher::regionHolds(rel, _model.evalImplicit(_funcs[fi], wx, wy)))
                                _view.plotPixelStipple(px, py, tint);
                        }
                    }
                }
            }

            // Boundary (G=0 contour) — skip cells touching a domain hole so
            // discontinuities don't smear.
            if (std::isnan(v00) || std::isnan(v10) ||
                std::isnan(v11) || std::isnan(v01)) continue;

            // Inside = G < 0. Build the 4-bit marching-squares case index.
            const int idx = (v00 < 0.0f ? 1 : 0) | (v10 < 0.0f ? 2 : 0) |
                            (v11 < 0.0f ? 4 : 0) | (v01 < 0.0f ? 8 : 0);
            if (idx == 0 || idx == 15) continue;  // wholly in/out → no contour

            // Edge crossing points (only computed where signs differ).
            const int tX = cross(v00, v10, x0, x1), tY = y0;   // top    edge A-B
            const int rX = x1, rY = cross(v10, v11, y0, y1);   // right  edge B-C
            const int bX = cross(v01, v11, x0, x1), bY = y1;   // bottom edge D-C
            const int lX = x0, lY = cross(v00, v01, y0, y1);   // left   edge A-D

            switch (idx) {
                case 1: case 14: _view.drawSegmentPx(tX, tY, lX, lY, color); break; // T-L
                case 2: case 13: _view.drawSegmentPx(tX, tY, rX, rY, color); break; // T-R
                case 3: case 12: _view.drawSegmentPx(lX, lY, rX, rY, color); break; // L-R
                case 4: case 11: _view.drawSegmentPx(rX, rY, bX, bY, color); break; // R-B
                case 6: case 9:  _view.drawSegmentPx(tX, tY, bX, bY, color); break; // T-B
                case 7: case 8:  _view.drawSegmentPx(lX, lY, bX, bY, color); break; // L-B
                case 5:  // saddle: A,C inside
                    _view.drawSegmentPx(tX, tY, lX, lY, color);
                    _view.drawSegmentPx(rX, rY, bX, bY, color);
                    break;
                case 10: // saddle: B,D inside
                    _view.drawSegmentPx(tX, tY, rX, rY, color);
                    _view.drawSegmentPx(lX, lY, bX, bY, color);
                    break;
                default: break;
            }
        }
        if ((j & 3) == 0) yield();
    }
}

// Equal-aspect ("square grid") enforcement. The curve is rasterised into a
// GRAPH_CANVAS_W x GRAPH_CANVAS_H (320 x 143) pixel buffer; squareness means the
// world span per pixel is identical on both axes: (xMax-xMin)/W == (yMax-yMin)/H.
// We equalize to the LARGER units-per-pixel — i.e. EXPAND the axis that is more
// zoomed-in — about the current centre, so nothing that was visible is cropped.
// Idempotent: on an already-square viewport every value is unchanged. Calling it
// once at the top of replot() makes EVERY render square (default view, pan, zoom,
// autofit, zoom-box, trace recenter) and keeps the canonical _xMin.._yMax the
// single source of truth for the renderer, trace cursor, POI markers and infobar.
void GrapherApp::normalizeAspect() {
    const float W = static_cast<float>(GRAPH_CANVAS_W);
    const float H = static_cast<float>(GRAPH_CANVAS_H);
    const float xRange = _xMax - _xMin;
    const float yRange = _yMax - _yMin;
    // Guard degenerate/corrupt viewports (zero/negative span, NaN, inf): leave
    // the viewport untouched rather than produce NaN/inf bounds. Not reachable
    // from current call sites, but keeps the helper self-contained & robust.
    if (!std::isfinite(xRange) || !std::isfinite(yRange) ||
        xRange <= 0.0f || yRange <= 0.0f || W <= 0.0f || H <= 0.0f) return;

    const float uppX = xRange / W;
    const float uppY = yRange / H;
    const float upp  = (uppX > uppY) ? uppX : uppY;   // larger units-per-pixel
    const float cx = (_xMin + _xMax) * 0.5f;
    const float cy = (_yMin + _yMax) * 0.5f;
    const float halfX = upp * W * 0.5f;
    const float halfY = upp * H * 0.5f;
    _xMin = cx - halfX; _xMax = cx + halfX;
    _yMin = cy - halfY; _yMax = cy + halfY;
}

void GrapherApp::replot() {
    if (!_plotDirty || !_graphArea || !_graphCanvas) return;
    _plotDirty = false;

    // Equal aspect: square the canonical viewport BEFORE it reaches the renderer
    // (and the trace/POI consumers, which read the same _xMin.._yMax). Done above
    // the layout-readiness gate below so the canonical viewport is kept square on
    // EVERY replot intent, even the early-out frame before the buffer is sized.
    normalizeAspect();

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
        if (_funcs[f].implicit) {
            plotImplicit(f, _funcs[f].color);
        } else {
            sampleFuncAdaptive(f, _funcs[f].color);
        }
    }

    if (_shadingActive) {
        if (_shadingAlongY) drawIntegralShadingY(_shadingFuncIdx, _shadingX0, _shadingX1);
        else                drawIntegralShading(_shadingFuncIdx, _shadingX0, _shadingX1);
    }
    drawActiveTangent();
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
    const TraceKind kind = (_grMode == GrMode::TRACE) ? traceKind(_traceFn)
                                                      : TraceKind::None;
    // Resolve the on-curve point for the active kind. ExplicitX recomputes y from
    // x (pixel-identical to the legacy path); the others use the authoritative
    // (_traceX,_traceY) maintained by the seed/step logic.
    if (kind != TraceKind::None) ensureTraceSeed();
    float wx = _traceX, wy = _traceY;
    bool  ok  = (kind != TraceKind::None);
    if (kind == TraceKind::ExplicitX) {
        wx = _traceX;
        wy = evalAt(_traceFn, _traceX);
    } else if (kind != TraceKind::None) {
        ok = _traceValid;
    }
    if (!ok || std::isnan(wx) || std::isinf(wx) || std::isnan(wy) || std::isinf(wy)) {
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

    float sx = (wx - _xMin) / xRange * areaW;
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
        snprintf(pillBuf, sizeof(pillBuf), "%s: (%.3f, %.3f)", poiLabel, (double)wx, (double)wy);
    } else {
        snprintf(pillBuf, sizeof(pillBuf), "x: %.3f   y: %.3f", (double)wx, (double)wy);
    }
    lv_label_set_text(_tracePillLabel, pillBuf);
}

void GrapherApp::updateInfoBar() {
    if (!_infoLabel) return;
    char buf[64];
    if (_grMode == GrMode::TRACE && _traceFn >= 0 && _traceFn < _numFuncs &&
        _funcs[_traceFn].isExplicit()) {
        float y = evalAt(_traceFn, _traceX);
        if (std::isnan(y) || std::isinf(y)) {
            // Off-domain (e.g. tan asymptote, log of negative): never show "nan".
            snprintf(buf, sizeof(buf), "Trace: x=%.3f  y=---  (off domain)", (double)_traceX);
        } else if (_snappedToPOI && _snappedPOIIdx >= 0 && _snappedPOIIdx < _numPOIs) {
            snprintf(buf, sizeof(buf), "Trace [%s]: x=%.3f  y=%.3f",
                     _pois[_snappedPOIIdx].label, (double)_traceX, (double)y);
        } else {
            snprintf(buf, sizeof(buf), "Trace: x=%.3f  y=%.3f", (double)_traceX, (double)y);
        }
    } else if (_grMode == GrMode::TRACE && traceKind(_traceFn) == TraceKind::ExplicitY) {
        // x = f(y): report the exact (x,y) on the curve, parametrised by y.
        ensureTraceSeed();
        if (!_traceValid)
            snprintf(buf, sizeof(buf), "Trace: y=%.3f  x=---  (off domain)", (double)_traceY);
        else
            snprintf(buf, sizeof(buf), "Trace: x=%.3f  y=%.3f", (double)_traceX, (double)_traceY);
    } else if (_grMode == GrMode::TRACE && traceKind(_traceFn) == TraceKind::Implicit) {
        // General implicit contour: report the point we are sitting on.
        ensureTraceSeed();
        if (!_traceValid)
            snprintf(buf, sizeof(buf), "Trace: curve not in view");
        else
            snprintf(buf, sizeof(buf), "Trace: x=%.3f  y=%.3f", (double)_traceX, (double)_traceY);
    } else if (_grMode == GrMode::TRACE) {
        // Defensive: trace somehow active with no traceable target.
        snprintf(buf, sizeof(buf), "Trace n/a: inequality");
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
            // GRBUG-005: accept an exact zero at the right sample too — a
            // crossing landing exactly on a sample point (y=x ∩ y=-x at x=0 on
            // the centred grid) gives diff==0, which the strict `< 0` product
            // test used to skip.
            if (prevDiff * diff < 0.0f || (diff == 0.0f && prevDiff != 0.0f)) {
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
                // GRBUG-005: dedupe ONLY against other Intersection POIs. Both
                // lines of y=x ∩ y=-x carry a root POI at x≈0, so the origin
                // crossing was always discarded as a "duplicate" of a ROOT.
                bool dup = false;
                for (int k = 0; k < _numPOIs; ++k) {
                    if (_pois[k].type != POIType::Intersection) continue;
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

    // Cross-kind intersections (line∩circle, parabola∩curve, contour∩contour, …)
    // for every pair involving fi that the 1-D loop above could not handle.
    appendCrossIntersections(fi);
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

// ── Strict 1:1 locked camera: viewport centre is always the control point ─
// Every traceable kind keeps the cursor pinned to screen centre as it walks the
// curve (NumWorks-style). y=f(x) recomputes y from x (legacy, pixel-identical);
// x=f(y) and implicit contours centre on the authoritative (_traceX,_traceY),
// which is what lets a circle/parabola stay centred instead of drifting to an edge.
void GrapherApp::syncViewportToCursor() {
    float xRange = _xMax - _xMin;
    float yRange = _yMax - _yMin;

    float cx = _traceX;
    float cy = _traceY;
    if (traceKind(_traceFn) == TraceKind::ExplicitX) {
        // Legacy path: x is authoritative, y is recomputed; keep the current y
        // centre if the function is off-domain at this x (NaN), exactly as before.
        float traceY = (_traceFn >= 0) ? evalAt(_traceFn, _traceX) : NAN;
        cx = _traceX;
        cy = (!std::isnan(traceY) && !std::isinf(traceY))
                 ? traceY
                 : (_yMin + _yMax) * 0.5f;
    }
    // Defence in depth vs GRBUG-001: never let a non-finite cursor coordinate
    // (off-domain seed/step) recentre the viewport onto NaN — that blanks every
    // later replot. Keep the respective current centre instead.
    if (!std::isfinite(cx)) cx = (_xMin + _xMax) * 0.5f;
    if (!std::isfinite(cy)) cy = (_yMin + _yMax) * 0.5f;

    _xMin = cx - xRange / 2.0f;
    _xMax = cx + xRange / 2.0f;
    _yMin = cy - yRange / 2.0f;
    _yMax = cy + yRange / 2.0f;

    _plotDirty = true;
    replot();
    // Recompute POIs for new viewport
    if (_traceFn >= 0) computePOIs(_traceFn);
}

// ── Cross-kind intersection markers ─────────────────────────────────────────
//
// The legacy intersection finder (appendPOIsForFunction) only compares y=f(x)
// against y=f(x) by sampling evalAt. To mark where a line meets a circle, an
// x=f(y) parabola meets a curve, or two implicit contours cross, we treat every
// traceable slot as a residual G(x,y)=0 and look for points where two residuals
// vanish at once.

// Unified residual: 0 exactly on the curve, signed off it. NAN on a domain hole
// or for a non-traceable slot (inequality region).
float GrapherApp::residualAt(int fi, float x, float y) {
    if (fi < 0 || fi >= _numFuncs) return NAN;
    FuncSlot& f = _funcs[fi];
    if (!f.isTraceable() || !f.visible) return NAN;
    if (!f.implicit) {                       // y = f(x)
        float fx = evalAt(fi, x);
        if (std::isnan(fx) || std::isinf(fx)) return NAN;
        return y - fx;
    }
    return _model.evalImplicit(f, x, y);     // x = f(y) and general implicit
}

// 2-D Newton on F(x,y) = (Ga, Gb) = (0, 0). Jacobian by central difference over
// ~one pixel. Returns true when both residuals are sub-pixel-small at the end.
bool GrapherApp::newton2D(int a, int b, float& x, float& y) {
    const int bw = _view.bufW() > 1 ? _view.bufW() : SCREEN_W;
    const int bh = _view.bufH() > 1 ? _view.bufH() : GRAPH_CANVAS_H;
    float hx = (_xMax - _xMin) / (float)bw;
    float hy = (_yMax - _yMin) / (float)bh;
    if (!(hx > 0.0f)) hx = 1e-4f;
    if (!(hy > 0.0f)) hy = 1e-4f;
    const float scale = _xMax - _xMin;
    for (int it = 0; it < 16; ++it) {
        float Fa = residualAt(a, x, y), Fb = residualAt(b, x, y);
        if (std::isnan(Fa) || std::isnan(Fb) || std::isinf(Fa) || std::isinf(Fb)) return false;
        float ax1 = residualAt(a, x + hx, y), ax0 = residualAt(a, x - hx, y);
        float ay1 = residualAt(a, x, y + hy), ay0 = residualAt(a, x, y - hy);
        float bx1 = residualAt(b, x + hx, y), bx0 = residualAt(b, x - hx, y);
        float by1 = residualAt(b, x, y + hy), by0 = residualAt(b, x, y - hy);
        if (std::isnan(ax1) || std::isnan(ax0) || std::isnan(ay1) || std::isnan(ay0) ||
            std::isnan(bx1) || std::isnan(bx0) || std::isnan(by1) || std::isnan(by0))
            return false;
        const float j11 = (ax1 - ax0) / (2.0f * hx);   // ∂Ga/∂x
        const float j12 = (ay1 - ay0) / (2.0f * hy);   // ∂Ga/∂y
        const float j21 = (bx1 - bx0) / (2.0f * hx);   // ∂Gb/∂x
        const float j22 = (by1 - by0) / (2.0f * hy);   // ∂Gb/∂y
        const float det = j11 * j22 - j12 * j21;
        if (std::fabs(det) < 1e-20f) return false;      // parallel curves / singular
        const float dx = (-Fa * j22 + Fb * j12) / det;
        const float dy = (-Fb * j11 + Fa * j21) / det;
        x += dx;
        y += dy;
        if (std::sqrt(dx * dx + dy * dy) < scale * 1e-4f) break;
    }
    float Fa = residualAt(a, x, y), Fb = residualAt(b, x, y);
    if (std::isnan(Fa) || std::isnan(Fb)) return false;
    const float tol = (std::fabs(_xMax - _xMin) + std::fabs(_yMax - _yMin)) * 1e-3f;
    return std::fabs(Fa) < tol && std::fabs(Fb) < tol;
}

// Grid-seeded search for points where curves a and b cross. A cell whose four
// corners straddle zero for BOTH residuals brackets a crossing; refine with
// newton2D and add a unique in-view Intersection POI.
void GrapherApp::find2DIntersections(int a, int b) {
    constexpr int GX = 32, GY = 24;
    const float xr = _xMax - _xMin, yr = _yMax - _yMin;
    if (xr <= 0.0f || yr <= 0.0f) return;

    std::vector<float> ga((size_t)(GX + 1) * (GY + 1));
    std::vector<float> gb((size_t)(GX + 1) * (GY + 1));
    for (int j = 0; j <= GY; ++j) {
        const float wy = _yMin + (float)j / (float)GY * yr;
        for (int i = 0; i <= GX; ++i) {
            const float wx = _xMin + (float)i / (float)GX * xr;
            ga[(size_t)j * (GX + 1) + i] = residualAt(a, wx, wy);
            gb[(size_t)j * (GX + 1) + i] = residualAt(b, wx, wy);
        }
        if ((j & 3) == 0) yield();
    }

    auto straddles = [](float c0, float c1, float c2, float c3) -> bool {
        if (std::isnan(c0) || std::isnan(c1) || std::isnan(c2) || std::isnan(c3)) return false;
        const bool anyNeg = c0 < 0.0f || c1 < 0.0f || c2 < 0.0f || c3 < 0.0f;
        const bool anyPos = c0 > 0.0f || c1 > 0.0f || c2 > 0.0f || c3 > 0.0f;
        return anyNeg && anyPos;
    };

    const float dupPx = 6.0f;   // markers closer than this (screen px) are the same point
    for (int j = 0; j < GY && _numPOIs < MAX_POIS; ++j) {
        for (int i = 0; i < GX && _numPOIs < MAX_POIS; ++i) {
            const size_t i00 = (size_t)j * (GX + 1) + i;
            const float a00 = ga[i00], a10 = ga[i00 + 1];
            const float a01 = ga[i00 + (GX + 1)], a11 = ga[i00 + (GX + 1) + 1];
            const float b00 = gb[i00], b10 = gb[i00 + 1];
            const float b01 = gb[i00 + (GX + 1)], b11 = gb[i00 + (GX + 1) + 1];
            if (!straddles(a00, a10, a01, a11)) continue;
            if (!straddles(b00, b10, b01, b11)) continue;

            float x = _xMin + ((float)i + 0.5f) / (float)GX * xr;
            float y = _yMin + ((float)j + 0.5f) / (float)GY * yr;
            if (!newton2D(a, b, x, y)) continue;
            if (x < _xMin || x > _xMax || y < _yMin || y > _yMax) continue;

            const int sx = _view.worldToScreenX(x);
            const int sy = _view.worldToScreenY(y);
            bool dup = false;
            for (int k = 0; k < _numPOIs; ++k) {
                const float ddx = (float)(sx - _view.worldToScreenX(_pois[k].x));
                const float ddy = (float)(sy - _view.worldToScreenY(_pois[k].y));
                if (ddx * ddx + ddy * ddy < dupPx * dupPx) { dup = true; break; }
            }
            if (dup) continue;
            auto& p = _pois[_numPOIs++];
            p.x = x; p.y = y;
            p.type = POIType::Intersection;
            strncpy(p.label, "Intersection", sizeof(p.label) - 1);
            p.label[sizeof(p.label) - 1] = '\0';
        }
        if ((j & 3) == 0) yield();
    }
}

// Intersections of fi with each earlier slot j<fi, for any pair that is NOT both
// y=f(x) (those keep the exact 1-D path in appendPOIsForFunction). Every unordered
// pair is visited once because appendPOIsForFunction runs for every fi in turn.
void GrapherApp::appendCrossIntersections(int fi) {
    if (fi < 0 || fi >= _numFuncs) return;
    if (!_funcs[fi].isTraceable() || !_funcs[fi].visible) return;
    const TraceKind ki = traceKind(fi);
    for (int j = 0; j < fi && _numPOIs < MAX_POIS; ++j) {
        if (!_funcs[j].isTraceable() || !_funcs[j].visible) continue;
        const TraceKind kj = traceKind(j);
        if (ki == TraceKind::ExplicitX && kj == TraceKind::ExplicitX) continue;  // legacy 1-D
        find2DIntersections(fi, j);
    }
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

        // ── Inequality operators (Grapher inecuaciones) ──
        // Inserted as a plain NodeVariable carrying the '<'/'>' glyph; serializeNode
        // emits the char verbatim, GraphModel::preCacheRPN splits on it and shades
        // the solution region. e.g. "x^2+y^2<1", "y>x^2".
        case KeyCode::LESS:    cur.insertVariable('<'); break;
        case KeyCode::GREATER: cur.insertVariable('>'); break;

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
        case 3: { // Trace (was Calculate)
            // Trace any curve — y=f(x), x=f(y), or an implicit equation. Only a
            // graph with no traceable curve (inequality / region only) refuses,
            // instead of tracing an invisible y=0 line (no "y=nan").
            int tf = firstTraceableFunc(0, +1);
            if (tf < 0) {
                if (_infoLabel)
                    lv_label_set_text(_infoLabel, "Trace n/a: inequality region only");
                break;
            }
            _focus = Focus::CONTENT;
            _grMode = GrMode::TRACE;
            _traceFn = tf;
            _traceX = (_xMin + _xMax) / 2.0f;
            _traceY = (_yMin + _yMax) / 2.0f;
            _traceSeeded = false;
            _traceHeadX = 0.0f; _traceHeadY = 0.0f;
            _snappedToPOI = false;
            _snappedPOIIdx = -1;
            _snapEscapeCount = 0;
            ensureTraceSeed();
            // x=f(y) / implicit lock the camera on the seed; y=f(x) keeps the view.
            if (traceKind(_traceFn) != TraceKind::ExplicitX && _traceValid)
                syncViewportToCursor();
            else
                computePOIs(_traceFn);
            drawTraceCursor();
            updateInfoBar();
            break;
        }
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
        // Enter Trace on any traceable curve — explicit y=f(x), explicit x=f(y),
        // or a general implicit equation. Only an inequality-only graph has no
        // traceable curve; there, stay in Pan and say so.
        int tf = firstTraceableFunc(0, +1);
        if (tf < 0) {
            if (_infoLabel)
                lv_label_set_text(_infoLabel, "Trace n/a: inequality region only");
            return;
        }
        _grMode = GrMode::TRACE;
        _traceFn = tf;
        _traceX = (_xMin + _xMax) / 2.0f;
        _traceY = (_yMin + _yMax) / 2.0f;
        _traceSeeded = false;
        _traceHeadX = 0.0f; _traceHeadY = 0.0f;
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        _snapEscapeCount = 0;
        // POIs for snap (roots/extrema, y=f(x) only) + intersection markers (every
        // curve kind, via the cross-kind finder). Snap itself stays ExplicitX-only.
        ensureTraceSeed();
        // x=f(y) / implicit lock the camera on the seed; y=f(x) keeps the view.
        if (traceKind(_traceFn) != TraceKind::ExplicitX && _traceValid)
            syncViewportToCursor();
        else
            computePOIs(_traceFn);
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
    bool didSyncViewport = false;

    switch (ev.code) {
    case KeyCode::LEFT:
        traceStep(-1);
        didSyncViewport = true;   // traceStep owns viewport sync / redraw
        break;
    case KeyCode::RIGHT:
        traceStep(+1);
        didSyncViewport = true;
        break;
    case KeyCode::UP: {
        // Switch to next traceable curve (explicit y=f(x), x=f(y), or implicit);
        // re-seed for the new slot. Keep the same X/Y where the kind allows.
        int nf = firstTraceableFunc(_traceFn + 1, +1);
        if (nf >= 0 && nf != _traceFn) {
            _traceFn = nf;
            _traceSeeded = false;
            _traceHeadX = 0.0f; _traceHeadY = 0.0f;
        }
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        ensureTraceSeed();
        // x=f(y) / implicit: centre the camera on the new seed. y=f(x): keep the
        // view and just (re)compute POIs (camera follows on the next step).
        if (traceKind(_traceFn) != TraceKind::ExplicitX && _traceValid)
            syncViewportToCursor();
        else
            computePOIs(_traceFn);
        if (_tangentActive && traceKind(_traceFn) != TraceKind::None) {
            // Carry an active tangent onto the newly selected curve at its seed point.
            _tangentFuncIdx = _traceFn;
            _tangentX = _traceX;
            _tangentY = _traceY;
            _plotDirty = true;
            replot();
        }
        break;
    }
    case KeyCode::DOWN: {
        // Switch to previous traceable curve; re-seed for the new slot.
        int pf = firstTraceableFunc(_traceFn - 1, -1);
        if (pf >= 0 && pf != _traceFn) {
            _traceFn = pf;
            _traceSeeded = false;
            _traceHeadX = 0.0f; _traceHeadY = 0.0f;
        }
        _snappedToPOI = false;
        _snappedPOIIdx = -1;
        ensureTraceSeed();
        // x=f(y) / implicit: centre the camera on the new seed. y=f(x): keep the
        // view and just (re)compute POIs (camera follows on the next step).
        if (traceKind(_traceFn) != TraceKind::ExplicitX && _traceValid)
            syncViewportToCursor();
        else
            computePOIs(_traceFn);
        if (_tangentActive && traceKind(_traceFn) != TraceKind::None) {
            // Carry an active tangent onto the newly selected curve at its seed point.
            _tangentFuncIdx = _traceFn;
            _tangentX = _traceX;
            _tangentY = _traceY;
            _plotDirty = true;
            replot();
        }
        break;
    }
    case KeyCode::ADD:
    case KeyCode::ZOOM:
    case KeyCode::SUB: {
        // Zoom in (ZOOM/+) or out (-) by 1.5×, keeping the control point centred —
        // so you can zoom while tracing any curve kind without losing the cursor.
        // Guard against a non-finite cursor coordinate (off-domain trace, e.g.
        // y=1/x at x=0): centring on NaN poisons _xMin.._yMax and blanks the
        // canvas on every later replot (GRBUG-001). Fall back to the view centre.
        const float f = (ev.code == KeyCode::SUB) ? 1.5f : (1.0f / 1.5f);
        const float nxr = (_xMax - _xMin) * f;
        const float nyr = (_yMax - _yMin) * f;
        const float czx = std::isfinite(_traceX) ? _traceX : (_xMin + _xMax) * 0.5f;
        const float czy = std::isfinite(_traceY) ? _traceY : (_yMin + _yMax) * 0.5f;
        _xMin = czx - nxr * 0.5f; _xMax = czx + nxr * 0.5f;
        _yMin = czy - nyr * 0.5f; _yMax = czy + nyr * 0.5f;
        if (traceKind(_traceFn) != TraceKind::None) {
            syncViewportToCursor();   // re-lock on the cursor at the new zoom
        } else {
            _plotDirty = true; replot();
        }
        drawTraceCursor();
        updateInfoBar();
        lv_refr_now(NULL);
        return;
    }
    case KeyCode::ENTER:
        // Calculate menu opens on every traceable curve. openCalcMenu greys the
        // options that don't apply to the current kind (e.g. Root/Integral on an
        // implicit contour), so non-y=f(x) curves get Intersection + Tangent.
        if (traceKind(_traceFn) != TraceKind::None) openCalcMenu();
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
        _tangentY = _traceY;
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

// Which Calculate-menu options make sense for a given traced curve kind. The
// goal-critical rule: never enable an option that would produce a fake/wrong
// answer on a curve it isn't defined for. Root/Min/Max/Integral are single-valued
// y=f(x) concepts. Intersection needs a second traceable curve. Tangent is the
// only option that works for every traceable kind (via ∇G for implicit/x=f(y)).
bool GrapherApp::calcItemEnabled(TraceKind k, int item) const {
    switch (item) {
    case 0: case 1: case 2: case 4:          // Root, Min, Max, Integral
        // Single-valued in one variable: y=f(x) analyses over x; x=f(y) analyses
        // over y (root = where x=0, min/max = leftmost/rightmost turning point,
        // integral = ∫f(y)dy). A general implicit contour is multivalued → disabled.
        return k == TraceKind::ExplicitX || k == TraceKind::ExplicitY;
    case 3: {                                 // Intersection — need ≥2 traceable curves
        int n = 0;
        for (int i = 0; i < _numFuncs; ++i)
            if (_funcs[i].isTraceable() && _funcs[i].visible && ++n >= 2) return true;
        return false;
    }
    case 5:                                   // Tangent — any traceable curve
        return k != TraceKind::None;
    default:
        return false;
    }
}

// Apply the three visual states to every menu row: disabled (grey, skipped),
// selected (blue fill, white text), or idle (white, dark text).
void GrapherApp::restyleCalcMenu() {
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
        if (!_calcMenuRows[i]) continue;
        const bool en  = _calcMenuEnabled[i];
        const bool sel = en && (i == _calcMenuIdx);
        lv_obj_set_style_bg_color(_calcMenuRows[i],
            lv_color_hex(sel ? COL_BTN_BG : 0xFFFFFF), LV_PART_MAIN);
        lv_obj_t* lbl = lv_obj_get_child(_calcMenuRows[i], 0);
        if (lbl) {
            uint32_t txt = sel ? 0xFFFFFF : (en ? 0x333333 : 0xBBBBBB);
            lv_obj_set_style_text_color(lbl, lv_color_hex(txt), LV_PART_MAIN);
        }
    }
}

void GrapherApp::openCalcMenu() {
    if (_calcMenuOpen || !_graphArea) return;

    // GRBUG-002: POIs (roots/extrema/intersections) used to be seeded only by
    // trace NAVIGATION, so Find Intersection invoked right after graphing (no
    // arrow pressed yet) searched an empty POI set → "No intersection found".
    // Seed them SYNCHRONOUSLY here: menu-open is a modal pause, the sweep is
    // bounded (≤ MAX_FUNCS slots × fixed sampling), and doing it inline (not
    // via the async timer) guarantees the set is ready even if ENTER lands on
    // "Find Intersection" on the very next frame. Flows that never open this
    // menu are untouched (pixel-identical goldens).
    if (_poiAsyncTimer) {
        lv_timer_delete(_poiAsyncTimer);
        _poiAsyncTimer = nullptr;
        _poiAsyncFi = -1;
    }
    _numPOIs = 0;
    for (int i = 0; i < _numFuncs; ++i) {
        if (_funcs[i].valid && _funcs[i].visible) appendPOIsForFunction(i);
    }

    _calcMenuOpen = true;

    // Compute per-kind availability and start the cursor on the first enabled item
    // (Tangent is always enabled, so a valid selection always exists).
    const TraceKind kind = traceKind(_traceFn);
    _calcMenuIdx = -1;
    for (int i = 0; i < CALC_MENU_ITEMS; ++i) {
        _calcMenuEnabled[i] = calcItemEnabled(kind, i);
        if (_calcMenuEnabled[i] && _calcMenuIdx < 0) _calcMenuIdx = i;
    }
    if (_calcMenuIdx < 0) _calcMenuIdx = 0;

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
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, CALC_MENU_LABELS[i]);
        lv_obj_center(lbl);
        // montserrat_14: every CALC_MENU_LABELS entry contains a space ("Find
        // Root", "Draw Tangent", ...) and stix_math_18 has no U+0020 glyph, so it
        // tofu'd at each space AND clipped vertically in the 24px rows.
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    }

    // Paint selected/idle/disabled states now that all rows exist.
    restyleCalcMenu();
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
        // Paint the freshly-computed intersection markers immediately. GrapherApp
        // has no per-frame update(), so replot() only runs on key events — without
        // this, purple markers would not appear until the next keypress. Only the
        // Intersection POI type is rendered, so single-curve traces are unaffected.
        if (app->_tab == Tab::GRAPH) {
            app->_plotDirty = true;
            app->replot();
            if (app->_grMode == GrMode::TRACE) app->drawTraceCursor();
        }
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
    case KeyCode::UP: {
        // Move to the previous ENABLED row (skip greyed-out options).
        for (int i = _calcMenuIdx - 1; i >= 0; --i) {
            if (_calcMenuEnabled[i]) { _calcMenuIdx = i; break; }
        }
        restyleCalcMenu();
        break;
    }
    case KeyCode::DOWN: {
        // Move to the next ENABLED row (skip greyed-out options).
        for (int i = _calcMenuIdx + 1; i < CALC_MENU_ITEMS; ++i) {
            if (_calcMenuEnabled[i]) { _calcMenuIdx = i; break; }
        }
        restyleCalcMenu();
        break;
    }
    case KeyCode::ENTER:
        if (_calcMenuIdx >= 0 && _calcMenuIdx < CALC_MENU_ITEMS && _calcMenuEnabled[_calcMenuIdx])
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
    // Never run an option that isn't valid for the current curve kind (the menu
    // greys these, but guard here so the math layer can't fabricate a result).
    if (option < 0 || option >= CALC_MENU_ITEMS || !_calcMenuEnabled[option]) return;

    // Analyse the active slot as a single-variable function. y=f(x) parametrises by
    // x; x=f(y) parametrises by y (the curve is single-valued in y). `evalFunc(t)`
    // returns the dependent coordinate; `tMin/tMax` is the visible range of the
    // independent coordinate. After solving, map the result back to a screen point.
    const TraceKind kind = traceKind(_traceFn);
    const bool alongY = (kind == TraceKind::ExplicitY);
    auto evalFunc = [this, alongY](double t) -> double {
        return alongY ? (double)_model.evalAtY(_funcs[_traceFn], (float)t)
                      : (double)evalAt(_traceFn, (float)t);
    };
    const double tMin = alongY ? (double)_yMin : (double)_xMin;
    const double tMax = alongY ? (double)_yMax : (double)_xMax;

    math::AnalysisResult res = { false, 0.0, 0.0 };
    char pillBuf[80];
    // Point-finding options (Root/Min/Max) move the cursor onto (px,py).
    bool  movePoint = false;
    float px = 0.0f, py = 0.0f;

    switch (option) {
    case 0: { // Find Root — where the function value is 0 (y=0 for f(x); x=0 for f(y)).
        res = math::findRoot(evalFunc, tMin, tMax);
        if (res.found) {
            if (alongY) { px = 0.0f;            py = (float)res.x; }  // x=f(y): x=0 at y=res.x
            else        { px = (float)res.x;    py = 0.0f;        }  // y=f(x): y=0 at x=res.x
            movePoint = true;
            snprintf(pillBuf, sizeof(pillBuf), "Root: x=%.4f y=%.4f", (double)px, (double)py);
        }
        break;
    }
    case 1:   // Find Minimum (leftmost turning point for x=f(y))
    case 2: { // Find Maximum (rightmost turning point for x=f(y))
        const bool findMax = (option == 2);
        res = math::findExtremum(evalFunc, tMin, tMax, findMax);
        if (res.found) {
            // res.x = independent coord at the extremum; res.y = the extreme value.
            if (alongY) { px = (float)res.y;    py = (float)res.x; }
            else        { px = (float)res.x;    py = (float)res.y; }
            movePoint = true;
            snprintf(pillBuf, sizeof(pillBuf), "%s: x=%.4f y=%.4f",
                     findMax ? "Max" : "Min", (double)px, (double)py);
        }
        break;
    }
    case 3: { // Find Intersection — jump to the nearest precomputed crossing.
        // The cross-kind 2-D finder already seeded every intersection as a purple
        // POI marker (line∩circle, parabola∩curve, contour∩contour, …); snap the
        // cursor to the closest one instead of re-deriving via a y=f(x)-only solver.
        int best = -1;
        float bestd = 1e30f;
        for (int i = 0; i < _numPOIs; ++i) {
            if (_pois[i].type != POIType::Intersection) continue;
            const float ddx = _pois[i].x - _traceX, ddy = _pois[i].y - _traceY;
            const float d = ddx * ddx + ddy * ddy;
            if (d < bestd) { bestd = d; best = i; }
        }
        if (best < 0) {
            if (_tracePillLabel) lv_label_set_text(_tracePillLabel, "No intersection found");
            if (_tracePill) lv_obj_remove_flag(_tracePill, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        _traceX = _pois[best].x;
        _traceY = _pois[best].y;
        _traceValid = true;
        if (traceKind(_traceFn) == TraceKind::Implicit)
            updateImplicitHeading(_traceFn, _traceX, _traceY);
        if (traceKind(_traceFn) != TraceKind::ExplicitX)
            syncViewportToCursor();   // keep the control point centred on the jump
        res.found = true; res.x = (double)_traceX; res.y = (double)_traceY;
        snprintf(pillBuf, sizeof(pillBuf), "Intersect: x=%.4f y=%.4f",
                 (double)_traceX, (double)_traceY);
        break;
    }
    case 4: { // Calculate Integral — ∫f(x)dx for y=f(x); ∫f(y)dy for x=f(y).
        res = math::numericalIntegral(evalFunc, tMin, tMax);
        if (res.found) {
            if (alongY) {
                snprintf(pillBuf, sizeof(pillBuf), "Area = %.4f (dy)", res.y);
                drawIntegralShadingY(_traceFn, _yMin, _yMax);   // horizontal strips
            } else {
                snprintf(pillBuf, sizeof(pillBuf), "Area = %.4f", res.y);
                drawIntegralShading(_traceFn, _xMin, _xMax);     // vertical strips
            }
            _plotDirty = true;
            replot();
        }
        break;
    }
    case 5: { // Draw Tangent — slope line for y=f(x); ⟂∇G line for implicit / x=f(y).
        if (traceKind(_traceFn) == TraceKind::ExplicitX) {
            drawTangentOverlay(_traceFn, _traceX);
            snprintf(pillBuf, sizeof(pillBuf), "Tangent at x=%.4f", (double)_traceX);
        } else {
            drawImplicitTangent(_traceFn, _traceX, _traceY);
            snprintf(pillBuf, sizeof(pillBuf), "Tangent at (%.3f, %.3f)",
                     (double)_traceX, (double)_traceY);
        }
        _plotDirty = true;
        replot();
        res.found = true;
        break;
    }
    default:
        return;
    }

    // Move the cursor onto a found Root/Min/Max point. Intersection set its own
    // point in-case; Integral/Tangent annotate the current point (no teleport).
    if (movePoint) {
        if (alongY) {
            _traceY = py; _traceX = px; _traceValid = true;
            syncViewportToCursor();   // lock the camera on the result, on-curve
        } else {
            _traceX = px;
            if (_traceX < _xMin) _traceX = _xMin;
            if (_traceX > _xMax) _traceX = _xMax;
        }
    }

    if (res.found) {
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
    _shadingAlongY = false;
}

// Horizontal-strip shading for an x=f(y) integral: for each pixel row in the
// shaded y-range, fill from the y-axis (x=0) out to x=f(y). Mirror of
// drawIntegralShading with the x/y roles swapped.
void GrapherApp::drawIntegralShadingY(int funcIdx, float shadeYMin, float shadeYMax) {
    if (!_graphBuf || funcIdx < 0 || funcIdx >= _numFuncs || !_funcs[funcIdx].valid)
        return;

    int areaW = GRAPH_CANVAS_W;
    int areaH = GRAPH_CANVAS_H;
    if (areaW < 2 || areaH < 2) return;

    float xRange = (float)(_xMax - _xMin);
    float yRange = (float)(_yMax - _yMin);
    if (xRange <= 0.0f || yRange <= 0.0f) return;

    if (shadeYMax < shadeYMin) { float t = shadeYMin; shadeYMin = shadeYMax; shadeYMax = t; }

    const int sy0 = std::max(0, std::min(areaH - 1, _view.worldToScreenY(shadeYMin)));
    const int sy1 = std::max(0, std::min(areaH - 1, _view.worldToScreenY(shadeYMax)));
    const int top = std::min(sy0, sy1);
    const int bot = std::max(sy0, sy1);

    const int xAxisPx = _view.worldToScreenX(0.0f);
    const uint16_t shade565 = utils::rgb888to565(_funcs[funcIdx].color);

    for (int py = top; py <= bot; ++py) {
        const float wy = (float)_yMax - ((float)py / (float)areaH) * yRange;
        if (wy < shadeYMin || wy > shadeYMax) continue;

        const float wx = _model.evalAtY(_funcs[funcIdx], wy);
        if (std::isnan(wx) || std::isinf(wx)) continue;

        int sx = _view.worldToScreenX(wx);
        if (sx < 0) sx = 0;
        if (sx >= areaW) sx = areaW - 1;
        int x0 = xAxisPx;
        if (x0 < 0) x0 = 0;
        if (x0 >= areaW) x0 = areaW - 1;

        const int left = std::min(sx, x0);
        const int right = std::max(sx, x0);
        for (int px = left; px <= right; ++px) {
            if (!useStipplePixel(px, py)) continue;  // checkerboard stipple
            _graphBuf[py * areaW + px] = shade565;
        }
    }

    _shadingActive = true;
    _shadingFuncIdx = funcIdx;
    _shadingX0 = shadeYMin;
    _shadingX1 = shadeYMax;
    _shadingAlongY = true;
}

void GrapherApp::clearIntegralShading() {
    _shadingActive = false;
    _shadingFuncIdx = -1;
    _shadingX0 = 0.0f;
    _shadingX1 = 0.0f;
    _shadingAlongY = false;
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
    _tangentY = yTarget;
}

// Tangent to a general curve at (x0,y0): the line ⟂ to ∇G. Works for circles,
// sideways parabolas, x=f(y) and any other contour the y=f(x) slope line cannot
// represent (vertical or multi-valued tangents). Drawn as a segment long enough
// to span the viewport; drawFunctionSegment clips it to the buffer.
void GrapherApp::drawImplicitTangent(int funcIdx, float x0, float y0) {
    if (!_graphBuf || funcIdx < 0 || funcIdx >= _numFuncs || !_funcs[funcIdx].valid) return;
    float gx, gy;
    if (!gradImplicit(funcIdx, x0, y0, gx, gy)) return;
    float tx = gy, ty = -gx;                       // tangent ⟂ gradient
    const float m = std::sqrt(tx * tx + ty * ty);
    if (m < 1e-9f) return;                          // singular point (∇G = 0)
    tx /= m; ty /= m;
    const float T = (_xMax - _xMin) + (_yMax - _yMin);  // overshoots the viewport
    _view.drawFunctionSegment(x0 - T * tx, y0 - T * ty,
                              x0 + T * tx, y0 + T * ty, _funcs[funcIdx].color);
    _tangentActive = true;
    _tangentFuncIdx = funcIdx;
    _tangentX = x0;
    _tangentY = y0;
}

// Redraw the active tangent the right way for its curve kind (called from replot).
void GrapherApp::drawActiveTangent() {
    if (!_tangentActive) return;
    if (traceKind(_tangentFuncIdx) == TraceKind::ExplicitX)
        drawTangentOverlay(_tangentFuncIdx, _tangentX);
    else
        drawImplicitTangent(_tangentFuncIdx, _tangentX, _tangentY);
}

void GrapherApp::clearTangent() {
    _tangentActive = false;
    _tangentFuncIdx = -1;
    _tangentX = 0.0f;
    _tangentY = 0.0f;
}


#ifdef NATIVE_SIM
// ═══════════════════════════════════════════════════════════════════════
// Emulator-only debug accessors (GR-14 assert hooks — see GrapherApp.h)
// ═══════════════════════════════════════════════════════════════════════

const char* GrapherApp::debugSlotKind(int i) const {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return "empty";
    const FuncSlot& f = _funcs[i];
    if (f.len == 0)  return "empty";
    if (!f.valid)    return "invalid";
    if (!f.implicit) return "explicitY";                       // y = f(x)
    switch (f.relation) {
        case grapher::Relation::Less:
        case grapher::Relation::Greater:      return "ineqStrict";
        case grapher::Relation::LessEqual:
        case grapher::Relation::GreaterEqual: return "ineqNonStrict";
        default: break;
    }
    if (f.explicitY) return "explicitX";                       // x = f(y)
    return "implicit";
}

bool GrapherApp::debugSlotValid(int i) const {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return false;
    return _funcs[i].valid;
}

const char* GrapherApp::debugSlotInvalidReason(int i) const {
    static char buf[32];
    buf[0] = '\0';
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return buf;
    const FuncSlot& f = _funcs[i];
    if (f.valid || f.len == 0) return buf;
    formatInvalidReason(f, buf, sizeof(buf));
    return buf;
}

const char* GrapherApp::debugSlotRelationOp(int i) const {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return "";
    switch (_funcs[i].relation) {
        case grapher::Relation::Less:         return "lt";
        case grapher::Relation::Greater:      return "gt";
        case grapher::Relation::LessEqual:    return "le";
        case grapher::Relation::GreaterEqual: return "ge";
        default:                              return "eq";
    }
}

const char* GrapherApp::debugSlotExprText(int i) const {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return "";
    return _funcs[i].text;
}

const char* GrapherApp::debugTraceMode() const {
    switch (_grMode) {
        case GrMode::TRACE:    return "trace";
        case GrMode::NAVIGATE: return "navigate";
        default:               return "idle";
    }
}

// GR-14 append-only hook (GRBUG-002/005 regression guard): how many of the
// currently computed POIs are curve∩curve intersections. Deterministic after
// openCalcMenu()'s synchronous seed / executeCalcOption(3).
int GrapherApp::debugIntersectionCount() const {
    int n = 0;
    for (int i = 0; i < _numPOIs; ++i)
        if (_pois[i].type == POIType::Intersection) ++n;
    return n;
}

// ── GIAC-C01 hooks (see GrapherApp.h) ─────────────────────────────────────

bool GrapherApp::debugSlotCompileOk(int i) const {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return false;
    return grapher::GraphModel::debugSlotCompiledOk(_funcs[i]);
}

int GrapherApp::debugSlotCompileCount(int i) const {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return 0;
    return grapher::GraphModel::debugSlotCompileCount(_funcs[i]);
}

float GrapherApp::debugSlotEvalParam(int i, float t) {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return NAN;
    if (_funcs[i].explicitY) return _model.evalAtY(_funcs[i], t);
    return _model.evalAt(_funcs[i], t);
}

float GrapherApp::debugSlotEvalResidual(int i, float x, float y) {
    if (i < 0 || i >= MAX_FUNCS || i >= _numFuncs) return NAN;
    return _model.evalImplicit(_funcs[i], x, y);
}
#endif  // NATIVE_SIM
