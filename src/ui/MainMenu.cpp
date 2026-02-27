/**
 * MainMenu.cpp
 * Launcher NumOS — Pixel-Perfect NumWorks High-Fidelity
 *
 * Feature summary:
 *  1. Montserrat 14 everywhere, full-opacity anti-aliased text
 *  2. Dot-grid background #D1D1D1 on #F5F5F5, 1 px dots, 20 px spacing
 *  3. Cards: shadow_width 12, shadow_spread 2; focus → blue/orange glow
 *  4. Focus anim: 1.05× scale via lv_anim_path_overshoot (250 ms)
 *  5. Geometric vector icons per app (concentric shapes, NumWorks palette)
 *  6. ENTER/OK → _launchCb(app_id) → SystemApp::launchApp()
 *
 * Visual layout (320 × 240):
 *   ┌──────────────────────────────────────┐
 *   │  rad      APPLICATIONS     battery   │  24 px (#FF9900)
 *   ├──────────────────────────────────────┤
 *   │  ┌────┐  ┌────┐  ┌────┐             │
 *   │  │ ◎  │  │ ∿  │  │ ≡  │             │  Row 0
 *   │  │Calc│  │Grph│  │Eqns│             │
 *   │  └────┘  └────┘  └────┘             │
 *   │  ┌────┐  ┌────┐  ┌────┐             │
 *   │  │ ⊞  │  │ ↺  │  │ ∞  │             │  Row 1
 *   │  │Stat│  │Regr│  │Sequ│             │
 *   │  └────┘  └────┘  └────┘             │
 *   │         ... scroll ↕ ...             │
 *   └──────────────────────────────────────┘
 */

#include "MainMenu.h"
#include "../display/DisplayDriver.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Layout constants
// ═══════════════════════════════════════════════════════════════════════════════
static constexpr int SCREEN_W      = 320;
static constexpr int SCREEN_H      = 240;
static constexpr int STATUS_BAR_H  = 24;
static constexpr int GRID_H        = SCREEN_H - STATUS_BAR_H;   // 216 px

static constexpr int GRID_PAD      = 8;
static constexpr int CARD_GAP_COL  = 6;
static constexpr int CARD_GAP_ROW  = 6;
static constexpr int CARD_RADIUS   = 8;
static constexpr int ICON_SIZE     = 44;
static constexpr int ICON_RADIUS   = 10;
static constexpr int CARD_PAD      = 6;
static constexpr int ROW_H         = 78;

// ═══════════════════════════════════════════════════════════════════════════════
// NumWorks colour palette
// ═══════════════════════════════════════════════════════════════════════════════
static constexpr uint32_t COL_STATUS_BAR   = 0xFF9900;
static constexpr uint32_t COL_STATUS_TEXT  = 0xFFFFFF;
static constexpr uint32_t COL_BG           = 0xF5F5F5;
static constexpr uint32_t COL_CARD_BG      = 0xFFFFFF;
static constexpr uint32_t COL_FOCUS_BORDER = 0x0074D9;
static constexpr uint32_t COL_LABEL_TEXT   = 0x333333;
static constexpr uint32_t COL_DOT_GRID     = 0xD1D1D1;

// ═══════════════════════════════════════════════════════════════════════════════
// App registry — {id, name, primary colour, lighter accent}
// ═══════════════════════════════════════════════════════════════════════════════
const MainMenu::AppEntry MainMenu::APPS[] = {
    //  id   name            primary     light accent
    {  0, "Calculation",  0xFF8000,   0xFFBB66 },   // Orange
    {  1, "Grapher",      0x50B849,   0x8ED888 },   // Green
    {  2, "Equations",    0x1565C0,   0x5E9CE0 },   // Blue
    {  3, "Statistics",   0xE91E63,   0xF06898 },   // Pink
    {  4, "Regression",   0xBF360C,   0xE07040 },   // Brown-red
    {  5, "Sequences",    0x7B1FA2,   0xAB60D0 },   // Purple
    {  6, "Python",       0xF57F17,   0xFAAF50 },   // Amber
    {  7, "Probability",  0x00897B,   0x40BBA8 },   // Teal
    {  8, "Solver",       0x283593,   0x5C6BC0 },   // Navy
    {  9, "Settings",     0x546E7A,   0x8AA4B0 },   // Blue-grey
};
const int MainMenu::APP_COUNT =
    sizeof(MainMenu::APPS) / sizeof(MainMenu::APPS[0]);

// ═══════════════════════════════════════════════════════════════════════════════
// Transition properties (animated with overshoot on focus)
// ═══════════════════════════════════════════════════════════════════════════════
static const lv_style_prop_t _transProps[] = {
    LV_STYLE_BORDER_WIDTH,
    LV_STYLE_BORDER_COLOR,
    LV_STYLE_BORDER_OPA,
    LV_STYLE_SHADOW_WIDTH,
    LV_STYLE_SHADOW_OPA,
    LV_STYLE_SHADOW_COLOR,
    LV_STYLE_SHADOW_SPREAD,
    LV_STYLE_TRANSFORM_SCALE_X,
    LV_STYLE_TRANSFORM_SCALE_Y,
    LV_STYLE_PROP_INV
};

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════════

MainMenu::MainMenu(DisplayDriver& display) : _display(display) {}

MainMenu::~MainMenu() {
    if (_group)  { lv_group_delete(_group);  _group  = nullptr; }
    if (_screen) { lv_obj_delete(_screen);   _screen = nullptr; }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::setLaunchCallback(std::function<void(int)> cb) {
    _launchCb = std::move(cb);
}

void MainMenu::load() {
    if (_screen) {
        lv_scr_load(_screen);
        lv_group_set_default(_group);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// create()
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::create() {
    initStyles();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, SCREEN_W, SCREEN_H);
    lv_obj_add_style(_screen, &_styleScreen, LV_PART_MAIN);
    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _group = lv_group_create();

    buildStatusBar();
    buildGrid();

    if (_grid) lv_group_focus_obj(_grid);
}

// ═══════════════════════════════════════════════════════════════════════════════
// buildStatusBar() — 24 px orange bar, Montserrat 14, anti-aliased
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::buildStatusBar() {
    lv_obj_t* bar = lv_obj_create(_screen);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_W, STATUS_BAR_H);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(bar, lv_color_hex(COL_STATUS_BAR), 0);
    lv_obj_set_style_bg_opa(bar,   LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar,   0, 0);

    // Left: angle mode
    lv_obj_t* modeLabel = lv_label_create(bar);
    lv_label_set_text(modeLabel, "rad");
    lv_obj_set_style_text_font(modeLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(modeLabel, lv_color_hex(COL_STATUS_TEXT), 0);
    lv_obj_set_style_text_opa(modeLabel, LV_OPA_COVER, 0);
    lv_obj_align(modeLabel, LV_ALIGN_LEFT_MID, 8, 0);

    // Centre: title
    lv_obj_t* title = lv_label_create(bar);
    lv_label_set_text(title, "APPLICATIONS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_STATUS_TEXT), 0);
    lv_obj_set_style_text_opa(title, LV_OPA_COVER, 0);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Right: battery
    lv_obj_t* batt = lv_label_create(bar);
    lv_label_set_text(batt, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_font(batt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(batt, lv_color_hex(COL_STATUS_TEXT), 0);
    lv_obj_set_style_text_opa(batt, LV_OPA_COVER, 0);
    lv_obj_align(batt, LV_ALIGN_RIGHT_MID, -8, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// buildGrid()
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::buildGrid() {
    static lv_coord_t col_dsc[] = {
        LV_PCT(33), LV_PCT(33), LV_PCT(34),
        LV_GRID_TEMPLATE_LAST
    };
    static lv_coord_t row_dsc[] = {
        ROW_H, ROW_H, ROW_H, ROW_H,
        LV_GRID_TEMPLATE_LAST
    };

    _grid = lv_obj_create(_screen);
    lv_obj_set_pos(_grid, 0, STATUS_BAR_H);
    lv_obj_set_size(_grid, SCREEN_W, GRID_H);
    lv_obj_set_scroll_dir(_grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_grid, LV_SCROLLBAR_MODE_AUTO);

    // Background
    lv_obj_set_style_bg_color(_grid, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(_grid,   LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_grid, 0, 0);
    lv_obj_set_style_radius(_grid, 0, 0);
    lv_obj_set_style_pad_all(_grid,    GRID_PAD, 0);
    lv_obj_set_style_pad_row(_grid,    CARD_GAP_ROW, 0);
    lv_obj_set_style_pad_column(_grid, CARD_GAP_COL, 0);

    // Scrollbar styling
    lv_obj_set_style_bg_color(_grid, lv_color_hex(0xBBBBBB), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(_grid,   LV_OPA_70,               LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(_grid,   2,                        LV_PART_SCROLLBAR);
    lv_obj_set_style_width(_grid,    3,                        LV_PART_SCROLLBAR);

    lv_obj_set_layout(_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(_grid, col_dsc, row_dsc);

    lv_gridnav_add(_grid, static_cast<lv_gridnav_ctrl_t>(
        LV_GRIDNAV_CTRL_ROLLOVER | LV_GRIDNAV_CTRL_SCROLL_FIRST));

    lv_group_add_obj(_group, _grid);

    // Dot-grid background callback
    lv_obj_add_event_cb(_grid, onGridDraw, LV_EVENT_DRAW_MAIN_END, nullptr);

    for (int i = 0; i < APP_COUNT; ++i) {
        buildCard(_grid, APPS[i], i % 3, i / 3);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// buildCard()
// ═══════════════════════════════════════════════════════════════════════════════

lv_obj_t* MainMenu::buildCard(lv_obj_t* parent,
                               const AppEntry& app,
                               int col, int row)
{
    lv_obj_t* card = lv_obj_create(parent);

    lv_obj_set_grid_cell(card,
        LV_GRID_ALIGN_STRETCH, col, 1,
        LV_GRID_ALIGN_STRETCH, row, 1);

    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);

    lv_obj_add_style(card, &_styleCard,        LV_PART_MAIN);
    lv_obj_add_style(card, &_styleCardFocused, LV_STATE_FOCUSED);
    lv_obj_add_style(card, &_styleCardPressed, LV_STATE_PRESSED);

    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_user_data(card,
        reinterpret_cast<void*>(static_cast<intptr_t>(app.id)));
    lv_obj_add_event_cb(card, onCardEvent, LV_EVENT_CLICKED, this);

    // ── Geometric vector icon ───────────────────────────────────────────
    createAppIcon(card, app);

    // ── App name (Montserrat 14, anti-aliased) ──────────────────────────
    lv_obj_t* name = lv_label_create(card);
    lv_label_set_text(name, app.name);
    lv_obj_add_style(name, &_styleAppName, LV_PART_MAIN);
    lv_obj_set_style_pad_top(name, 4, 0);

    return card;
}

// ═══════════════════════════════════════════════════════════════════════════════
// createAppIcon() — Geometric vector icon per app
//
// Each icon is a coloured rounded box with LVGL draw primitives drawn
// via LV_EVENT_DRAW_MAIN_END: concentric circles, bars, curves, etc.
// Stores the app ID in user_data so the draw callback can branch per app.
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::createAppIcon(lv_obj_t* parent, const AppEntry& app) {
    lv_obj_t* iconBox = lv_obj_create(parent);
    lv_obj_set_size(iconBox, ICON_SIZE, ICON_SIZE);
    lv_obj_clear_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(iconBox, &_styleIconBox, LV_PART_MAIN);

    // Per-app primary colour
    lv_obj_set_style_bg_color(iconBox, lv_color_hex(app.color), 0);

    // Colour-tinted shadow for depth
    lv_obj_set_style_shadow_color(iconBox,  lv_color_hex(app.color), 0);
    lv_obj_set_style_shadow_width(iconBox,  10, 0);
    lv_obj_set_style_shadow_opa(iconBox,    LV_OPA_30, 0);
    lv_obj_set_style_shadow_spread(iconBox, 1, 0);
    lv_obj_set_style_shadow_ofs_y(iconBox,  2, 0);

    // Store both colours packed: primary in lower 32 bits via user_data
    // We use app.id to branch drawing and app.colorLight in the callback
    lv_obj_set_user_data(iconBox,
        reinterpret_cast<void*>(static_cast<intptr_t>(app.id)));

    // Register custom draw callback for geometric shapes
    lv_obj_add_event_cb(iconBox, onIconDraw, LV_EVENT_DRAW_MAIN_END,
        reinterpret_cast<void*>(static_cast<uintptr_t>(app.colorLight)));
}

// ═══════════════════════════════════════════════════════════════════════════════
// onIconDraw() — Draws geometric vector shapes per app id
//
// Shapes are drawn in white / light-accent on the coloured background.
// This creates a clean, "printed" look matching NumWorks style.
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::onIconDraw(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t*   obj   = lv_event_get_target_obj(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);

    int cx = (a.x1 + a.x2) / 2;   // Centre X
    int cy = (a.y1 + a.y2) / 2;   // Centre Y

    int appId = static_cast<int>(
        reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj)));
    uint32_t lightCol = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));

    lv_color_t white = lv_color_hex(0xFFFFFF);
    lv_color_t light = lv_color_hex(lightCol);

    // Helper: draw a filled circle (arc 0–360)
    auto drawCircle = [&](int x, int y, int r, lv_color_t col, lv_opa_t opa) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color      = col;
        arc.width      = r;
        arc.center.x   = x;
        arc.center.y   = y;
        arc.radius     = r;
        arc.start_angle = 0;
        arc.end_angle   = 360;
        arc.rounded    = 0;
        arc.opa        = opa;
        lv_draw_arc(layer, &arc);
    };

    // Helper: draw a small filled rect
    auto drawRect = [&](int x1, int y1, int x2, int y2, int radius,
                        lv_color_t col, lv_opa_t opa) {
        lv_draw_rect_dsc_t r;
        lv_draw_rect_dsc_init(&r);
        r.bg_color = col;
        r.bg_opa   = opa;
        r.radius   = radius;
        lv_area_t area = {(int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2};
        lv_draw_rect(layer, &r, &area);
    };

    // Helper: draw a line segment
    auto drawLine = [&](int x1, int y1, int x2, int y2, int w,
                        lv_color_t col, lv_opa_t opa) {
        lv_draw_line_dsc_t l;
        lv_draw_line_dsc_init(&l);
        l.color       = col;
        l.width       = w;
        l.opa         = opa;
        l.round_start = 1;
        l.round_end   = 1;
        l.p1.x = x1;  l.p1.y = y1;
        l.p2.x = x2;  l.p2.y = y2;
        lv_draw_line(layer, &l);
    };

    switch (appId) {
        case 0: {
            // Calculation: "1+2" style — three horizontal bars (abstract keypad)
            int bw = 22, bh = 3;
            int sx = cx - bw / 2;
            drawRect(sx, cy - 8, sx + bw, cy - 8 + bh, 1, white, LV_OPA_COVER);
            drawRect(sx, cy - 1, sx + bw, cy - 1 + bh, 1, light, LV_OPA_80);
            drawRect(sx, cy + 6, sx + bw, cy + 6 + bh, 1, white, LV_OPA_COVER);
            // Plus sign
            drawLine(cx + 6, cy - 10, cx + 6, cy - 3, 2, white, LV_OPA_COVER);
            drawLine(cx + 3, cy - 7,  cx + 9, cy - 7, 2, white, LV_OPA_COVER);
            break;
        }
        case 1: {
            // Grapher: sine-wave approximation with line segments
            int baseY = cy + 2;
            drawLine(a.x1 + 6,  baseY,     cx - 4,  cy - 10, 2, white, LV_OPA_COVER);
            drawLine(cx - 4,    cy - 10,   cx + 4,  cy + 10, 2, white, LV_OPA_COVER);
            drawLine(cx + 4,    cy + 10,   a.x2 - 6, baseY,  2, white, LV_OPA_COVER);
            // X-axis
            drawLine(a.x1 + 5, baseY, a.x2 - 5, baseY, 1, light, LV_OPA_60);
            break;
        }
        case 2: {
            // Equations: "x=" symbol — variable + equals sign
            drawRect(cx - 10, cy - 6, cx - 4, cy + 6, 2, white, LV_OPA_COVER);
            drawLine(cx,  cy - 3, cx + 10, cy - 3, 2, white, LV_OPA_COVER);
            drawLine(cx,  cy + 3, cx + 10, cy + 3, 2, white, LV_OPA_COVER);
            break;
        }
        case 3: {
            // Statistics: three vertical bars (bar chart)
            int bw = 5;
            drawRect(cx - 10, cy + 8, cx - 10 + bw, cy - 2,  2, white, LV_OPA_COVER);
            drawRect(cx - 2,  cy + 8, cx - 2 + bw,  cy - 10, 2, light, LV_OPA_90);
            drawRect(cx + 6,  cy + 8, cx + 6 + bw,  cy - 5,  2, white, LV_OPA_COVER);
            break;
        }
        case 4: {
            // Regression: scatter dots + trend line
            drawCircle(cx - 8, cy + 4, 2, white, LV_OPA_COVER);
            drawCircle(cx - 3, cy - 1, 2, white, LV_OPA_COVER);
            drawCircle(cx + 4, cy - 5, 2, white, LV_OPA_COVER);
            drawCircle(cx + 9, cy - 9, 2, white, LV_OPA_COVER);
            // Trend line
            drawLine(cx - 12, cy + 8, cx + 12, cy - 10, 2, light, LV_OPA_70);
            break;
        }
        case 5: {
            // Sequences: concentric circles (rings = progression)
            drawCircle(cx, cy, 14, light, LV_OPA_40);
            drawCircle(cx, cy, 9,  white, LV_OPA_60);
            drawCircle(cx, cy, 4,  white, LV_OPA_COVER);
            break;
        }
        case 6: {
            // Python: ">_" prompt — two lines
            drawLine(cx - 8, cy - 6, cx,     cy,     2, white, LV_OPA_COVER);
            drawLine(cx,     cy,     cx - 8, cy + 6, 2, white, LV_OPA_COVER);
            drawLine(cx + 2, cy + 6, cx + 10,cy + 6, 2, light, LV_OPA_90);
            break;
        }
        case 7: {
            // Probability: bell-curve (Gaussian approximation)
            drawLine(a.x1 + 6,  cy + 8, cx - 6, cy - 2,  2, light, LV_OPA_70);
            drawLine(cx - 6,    cy - 2, cx,     cy - 12, 2, white, LV_OPA_COVER);
            drawLine(cx,        cy - 12,cx + 6, cy - 2,  2, white, LV_OPA_COVER);
            drawLine(cx + 6,    cy - 2, a.x2 - 6, cy + 8,2, light, LV_OPA_70);
            // Base line
            drawLine(a.x1 + 5, cy + 8, a.x2 - 5, cy + 8, 1, white, LV_OPA_40);
            break;
        }
        case 8: {
            // Solver: checkmark / tick inside rounded frame
            drawRect(cx - 11, cy - 11, cx + 11, cy + 11, 4, light, LV_OPA_30);
            drawLine(cx - 6,  cy,     cx - 2, cy + 5, 3, white, LV_OPA_COVER);
            drawLine(cx - 2,  cy + 5, cx + 7, cy - 6, 3, white, LV_OPA_COVER);
            break;
        }
        case 9: {
            // Settings: gear — concentric ring + centre dot
            drawCircle(cx, cy, 12, light, LV_OPA_40);
            drawCircle(cx, cy, 8,  white, LV_OPA_50);
            drawCircle(cx, cy, 4,  white, LV_OPA_COVER);
            // Four notch lines (simplified gear teeth)
            drawLine(cx, cy - 14, cx, cy - 10, 2, white, LV_OPA_80);
            drawLine(cx, cy + 10, cx, cy + 14, 2, white, LV_OPA_80);
            drawLine(cx - 14, cy, cx - 10, cy, 2, white, LV_OPA_80);
            drawLine(cx + 10, cy, cx + 14, cy, 2, white, LV_OPA_80);
            break;
        }
        default:
            // Fallback: simple centred dot
            drawCircle(cx, cy, 6, white, LV_OPA_COVER);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// initStyles() — All styles + overshoot transition
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::initStyles() {

    // ── Transition: 250 ms overshoot (elastic pop) ──────────────────────
    lv_style_transition_dsc_init(&_transCard,
        _transProps,
        lv_anim_path_overshoot,
        250,          // duration ms
        0,            // delay
        nullptr);

    // ── Screen ──────────────────────────────────────────────────────────
    lv_style_init(&_styleScreen);
    lv_style_set_bg_color(&_styleScreen, lv_color_hex(COL_BG));
    lv_style_set_bg_opa(&_styleScreen,   LV_OPA_COVER);
    lv_style_set_border_width(&_styleScreen, 0);
    lv_style_set_pad_all(&_styleScreen, 0);
    lv_style_set_radius(&_styleScreen, 0);

    // ── Card (normal) ───────────────────────────────────────────────────
    lv_style_init(&_styleCard);
    lv_style_set_bg_color(&_styleCard,   lv_color_hex(COL_CARD_BG));
    lv_style_set_bg_opa(&_styleCard,     LV_OPA_COVER);
    lv_style_set_radius(&_styleCard,     CARD_RADIUS);
    lv_style_set_pad_all(&_styleCard,    CARD_PAD);
    lv_style_set_pad_row(&_styleCard,    0);
    // Reserve border space (invisible at rest)
    lv_style_set_border_width(&_styleCard, 2);
    lv_style_set_border_color(&_styleCard, lv_color_hex(COL_CARD_BG));
    lv_style_set_border_opa(&_styleCard,   LV_OPA_TRANSP);
    // Diffuse shadow (req #3: shadow_width 12, spread 2)
    lv_style_set_shadow_width(&_styleCard,  12);
    lv_style_set_shadow_opa(&_styleCard,    LV_OPA_10);
    lv_style_set_shadow_color(&_styleCard,  lv_color_hex(0x000000));
    lv_style_set_shadow_ofs_y(&_styleCard,  3);
    lv_style_set_shadow_spread(&_styleCard, 2);
    // Base scale 100 % (256)
    lv_style_set_transform_scale_x(&_styleCard, 256);
    lv_style_set_transform_scale_y(&_styleCard, 256);
    // Transition → overshoot animates BOTH in and out
    lv_style_set_transition(&_styleCard, &_transCard);

    // ── Card (focused): blue glow + 1.05× ──────────────────────────────
    lv_style_init(&_styleCardFocused);
    lv_style_set_border_width(&_styleCardFocused, 2);
    lv_style_set_border_color(&_styleCardFocused, lv_color_hex(COL_FOCUS_BORDER));
    lv_style_set_border_opa(&_styleCardFocused,   LV_OPA_COVER);
    // Intense blue shadow (req #3)
    lv_style_set_shadow_width(&_styleCardFocused,  20);
    lv_style_set_shadow_opa(&_styleCardFocused,    LV_OPA_40);
    lv_style_set_shadow_color(&_styleCardFocused,  lv_color_hex(COL_FOCUS_BORDER));
    lv_style_set_shadow_spread(&_styleCardFocused, 3);
    // Scale 1.05× (req #4: 256 × 1.05 = 268.8 ≈ 269)
    lv_style_set_transform_scale_x(&_styleCardFocused, 269);
    lv_style_set_transform_scale_y(&_styleCardFocused, 269);

    // ── Card (pressed) ──────────────────────────────────────────────────
    lv_style_init(&_styleCardPressed);
    lv_style_set_bg_color(&_styleCardPressed, lv_color_hex(0xE8E8E8));
    // Scale 97 % (256 × 0.97 ≈ 248) — satisfying click feel
    lv_style_set_transform_scale_x(&_styleCardPressed, 248);
    lv_style_set_transform_scale_y(&_styleCardPressed, 248);

    // ── Icon box ────────────────────────────────────────────────────────
    lv_style_init(&_styleIconBox);
    lv_style_set_bg_opa(&_styleIconBox,      LV_OPA_COVER);
    lv_style_set_radius(&_styleIconBox,      ICON_RADIUS);
    lv_style_set_border_width(&_styleIconBox, 0);
    lv_style_set_pad_all(&_styleIconBox,     0);

    // ── App name (Montserrat 14, full-opa anti-aliased text) ────────────
    lv_style_init(&_styleAppName);
    lv_style_set_text_font(&_styleAppName,  &lv_font_montserrat_14);
    lv_style_set_text_color(&_styleAppName, lv_color_hex(COL_LABEL_TEXT));
    lv_style_set_text_opa(&_styleAppName,   LV_OPA_COVER);
    lv_style_set_text_align(&_styleAppName, LV_TEXT_ALIGN_CENTER);
    lv_style_set_bg_opa(&_styleAppName,     LV_OPA_TRANSP);
    lv_style_set_border_width(&_styleAppName, 0);
    lv_style_set_pad_all(&_styleAppName,    0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// onGridDraw() — Dot-grid pattern: 1 px #D1D1D1 dots, 20 px spacing
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::onGridDraw(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t*   obj   = lv_event_get_target_obj(e);

    lv_area_t objArea;
    lv_obj_get_coords(obj, &objArea);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(COL_DOT_GRID);
    dsc.bg_opa   = LV_OPA_COVER;
    dsc.radius   = LV_RADIUS_CIRCLE;

    static constexpr int DOT_SPACING = 20;
    static constexpr int DOT_SIZE    = 1;     // 1 px subtle dots

    lv_coord_t scrollY = lv_obj_get_scroll_y(obj);
    int x0 = objArea.x1;
    int y0 = objArea.y1;
    int contentH = GRID_H + 120;

    for (int dy = DOT_SPACING / 2; dy < contentH; dy += DOT_SPACING) {
        int screenY = y0 + dy - scrollY;
        if (screenY + DOT_SIZE < objArea.y1 || screenY > objArea.y2) continue;

        for (int dx = DOT_SPACING / 2; dx < SCREEN_W; dx += DOT_SPACING) {
            lv_area_t dot;
            dot.x1 = x0 + dx;
            dot.y1 = screenY;
            dot.x2 = dot.x1 + DOT_SIZE;
            dot.y2 = dot.y1 + DOT_SIZE;
            lv_draw_rect(layer, &dsc, &dot);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// onCardEvent — ENTER/OK → _launchCb(app_id) → SystemApp::launchApp()
// ═══════════════════════════════════════════════════════════════════════════════

void MainMenu::onCardEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_obj_t* card = lv_event_get_target_obj(e);
    auto* self     = static_cast<MainMenu*>(lv_event_get_user_data(e));
    int appId      = static_cast<int>(
                         reinterpret_cast<intptr_t>(lv_obj_get_user_data(card)));

    if (self && self->_launchCb) {
        self->_launchCb(appId);
    }
}
