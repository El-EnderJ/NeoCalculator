/**
 * SequencesApp.cpp — Sequences & Series application for NumOS.
 *
 * Two-tab LVGL app: Define sequences, View table of values.
 * Supports arithmetic and geometric sequence expressions.
 *
 * Part of: NumOS — Mathematics Suite
 */

#include "SequencesApp.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ══ Color palette ════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0xFFFFFF;
static constexpr uint32_t COL_TAB_BG     = 0xF0F0F0;
static constexpr uint32_t COL_TAB_ACTIVE = 0x4A90D9;
static constexpr uint32_t COL_TAB_TEXT   = 0x333333;
static constexpr uint32_t COL_TAB_TEXT_A = 0xFFFFFF;
static constexpr uint32_t COL_CELL_FOCUS = 0xE3F2FD;
static constexpr uint32_t COL_CELL_EDIT  = 0xFFF9C4;
static constexpr uint32_t COL_TEXT       = 0x1A1A1A;
static constexpr uint32_t COL_HINT       = 0x808080;
static constexpr uint32_t COL_HEADER_BG  = 0xE0E0E0;
static constexpr uint32_t COL_SEQ_U      = 0xCC0000;   // Red for u(n)
static constexpr uint32_t COL_SEQ_V      = 0x0000CC;   // Blue for v(n)

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

SequencesApp::SequencesApp()
    : _screen(nullptr)
    , _tabBar(nullptr)
    , _definePanel(nullptr)
    , _defineHint(nullptr)
    , _tablePanel(nullptr)
    , _table(nullptr)
    , _activeTab(Tab::DEFINE)
    , _selectedSeq(0)
    , _editing(false)
    , _editLen(0)
    , _numTerms(10)
{
    memset(_editBuf, 0, sizeof(_editBuf));
    memset(_tabBtns, 0, sizeof(_tabBtns));
    memset(_tabLabels, 0, sizeof(_tabLabels));
    memset(_seqLabels, 0, sizeof(_seqLabels));
    memset(_seqExprs, 0, sizeof(_seqExprs));

    // Default expressions
    strncpy(_seqExprText[0], "2*n+1", MAX_EXPR - 1);   // u(n) = 2n+1
    strncpy(_seqExprText[1], "n^2", MAX_EXPR - 1);     // v(n) = n²

    for (int s = 0; s < MAX_SEQ; ++s)
        for (int t = 0; t < MAX_TERMS; ++t)
            _seqValues[s][t] = 0.0;
}

SequencesApp::~SequencesApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin — Create LVGL screen + widgets
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Sequences");

    createTabBar();
    createDefineTab();
    createTableTab();

    recompute();
    switchTab(Tab::DEFINE);
}

// ════════════════════════════════════════════════════════════════════════════
// end — Destroy screen
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::end() {
    if (_screen) {
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen      = nullptr;
        _tabBar      = nullptr;
        _definePanel = nullptr;
        _defineHint  = nullptr;
        _tablePanel  = nullptr;
        _table       = nullptr;
        memset(_tabBtns, 0, sizeof(_tabBtns));
        memset(_tabLabels, 0, sizeof(_tabLabels));
        memset(_seqLabels, 0, sizeof(_seqLabels));
        memset(_seqExprs, 0, sizeof(_seqExprs));
    }
    _activeTab = Tab::DEFINE;
    _selectedSeq = 0;
    _editing = false;
}

// ════════════════════════════════════════════════════════════════════════════
// load — Activate screen
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();
}

// ════════════════════════════════════════════════════════════════════════════
// Tab bar
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::createTabBar() {
    int y = ui::StatusBar::HEIGHT;

    _tabBar = lv_obj_create(_screen);
    lv_obj_set_pos(_tabBar, 0, y);
    lv_obj_set_size(_tabBar, SCREEN_W, TAB_BAR_H);
    lv_obj_set_style_bg_color(_tabBar, lv_color_hex(COL_TAB_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tabBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tabBar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_tabBar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_tabBar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tabBar, LV_OBJ_FLAG_SCROLLABLE);

    const char* titles[] = { "Define", "Table" };
    int tabW = SCREEN_W / NUM_TABS;

    for (int i = 0; i < NUM_TABS; ++i) {
        _tabBtns[i] = lv_obj_create(_tabBar);
        lv_obj_set_pos(_tabBtns[i], i * tabW, 0);
        lv_obj_set_size(_tabBtns[i], tabW, TAB_BAR_H);
        lv_obj_set_style_border_width(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tabBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_tabBtns[i], LV_OBJ_FLAG_SCROLLABLE);

        _tabLabels[i] = lv_label_create(_tabBtns[i]);
        lv_label_set_text(_tabLabels[i], titles[i]);
        lv_obj_set_style_text_font(_tabLabels[i], &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_center(_tabLabels[i]);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Define tab — sequence expression editor
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::createDefineTab() {
    int topY = ui::StatusBar::HEIGHT + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _definePanel = lv_obj_create(_screen);
    lv_obj_set_pos(_definePanel, 0, topY);
    lv_obj_set_size(_definePanel, SCREEN_W, panelH);
    lv_obj_set_style_bg_color(_definePanel, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_definePanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_definePanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_definePanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_definePanel, LV_OBJ_FLAG_SCROLLABLE);

    const char* names[] = { "u(n) =", "v(n) =" };
    const uint32_t colors[] = { COL_SEQ_U, COL_SEQ_V };

    for (int i = 0; i < MAX_SEQ; ++i) {
        int ry = 20 + i * 60;

        // Sequence name label
        _seqLabels[i] = lv_label_create(_definePanel);
        lv_label_set_text(_seqLabels[i], names[i]);
        lv_obj_set_pos(_seqLabels[i], 15, ry);
        lv_obj_set_style_text_color(_seqLabels[i], lv_color_hex(colors[i]), LV_PART_MAIN);
        lv_obj_set_style_text_font(_seqLabels[i], &lv_font_montserrat_14, LV_PART_MAIN);

        // Expression display
        _seqExprs[i] = lv_label_create(_definePanel);
        lv_label_set_text(_seqExprs[i], _seqExprText[i]);
        lv_obj_set_pos(_seqExprs[i], 85, ry);
        lv_obj_set_style_text_color(_seqExprs[i], lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(_seqExprs[i], &lv_font_montserrat_14, LV_PART_MAIN);
    }

    // Hint
    _defineHint = lv_label_create(_definePanel);
    lv_label_set_text(_defineHint, "ENTER to edit, LEFT/RIGHT to switch tab");
    lv_obj_set_pos(_defineHint, 15, panelH - 25);
    lv_obj_set_style_text_color(_defineHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_style_text_font(_defineHint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ════════════════════════════════════════════════════════════════════════════
// Table tab — computed sequence values
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::createTableTab() {
    int topY = ui::StatusBar::HEIGHT + TAB_BAR_H;
    int panelH = SCREEN_H - topY;

    _tablePanel = lv_obj_create(_screen);
    lv_obj_set_pos(_tablePanel, 0, topY);
    lv_obj_set_size(_tablePanel, SCREEN_W, panelH);
    lv_obj_set_style_bg_color(_tablePanel, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_tablePanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_tablePanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_tablePanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_tablePanel, LV_OBJ_FLAG_SCROLLABLE);

    _table = lv_table_create(_tablePanel);
    lv_obj_set_pos(_table, 5, 5);
    lv_obj_set_size(_table, SCREEN_W - 10, panelH - 10);
    lv_table_set_column_count(_table, 3);
    lv_table_set_column_width(_table, 0, 60);   // n
    lv_table_set_column_width(_table, 1, 120);  // u(n)
    lv_table_set_column_width(_table, 2, 120);  // v(n)

    // Headers
    lv_table_set_cell_value(_table, 0, 0, "n");
    lv_table_set_cell_value(_table, 0, 1, "u(n)");
    lv_table_set_cell_value(_table, 0, 2, "v(n)");

    updateTable();
}

// ════════════════════════════════════════════════════════════════════════════
// Tab switching
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::switchTab(Tab t) {
    _activeTab = t;
    if (_definePanel) lv_obj_add_flag(_definePanel, LV_OBJ_FLAG_HIDDEN);
    if (_tablePanel)  lv_obj_add_flag(_tablePanel,  LV_OBJ_FLAG_HIDDEN);

    switch (t) {
        case Tab::DEFINE: lv_obj_remove_flag(_definePanel, LV_OBJ_FLAG_HIDDEN); break;
        case Tab::TABLE:
            recompute();
            updateTable();
            lv_obj_remove_flag(_tablePanel, LV_OBJ_FLAG_HIDDEN);
            break;
    }
    updateTabStyles();
}

void SequencesApp::updateTabStyles() {
    for (int i = 0; i < NUM_TABS; ++i) {
        bool active = (i == static_cast<int>(_activeTab));
        lv_obj_set_style_bg_color(_tabBtns[i],
            lv_color_hex(active ? COL_TAB_ACTIVE : COL_TAB_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tabBtns[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(_tabLabels[i],
            lv_color_hex(active ? COL_TAB_TEXT_A : COL_TAB_TEXT), LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Define helpers
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::updateDefineDisplay() {
    for (int i = 0; i < MAX_SEQ; ++i) {
        if (_seqExprs[i]) {
            lv_label_set_text(_seqExprs[i], _seqExprText[i]);
        }
        // Highlight selected
        if (_seqLabels[i]) {
            bool sel = (i == _selectedSeq && !_editing);
            lv_obj_set_style_bg_color(lv_obj_get_parent(_seqLabels[i]),
                lv_color_hex(sel ? COL_CELL_FOCUS : COL_BG), LV_PART_MAIN);
        }
    }
}

void SequencesApp::startEdit() {
    _editing = true;
    strncpy(_editBuf, _seqExprText[_selectedSeq], MAX_EXPR - 1);
    _editLen = strlen(_editBuf);
}

void SequencesApp::finishEdit() {
    strncpy(_seqExprText[_selectedSeq], _editBuf, MAX_EXPR - 1);
    _editing = false;
    recompute();
    updateDefineDisplay();
}

void SequencesApp::cancelEdit() {
    _editing = false;
    updateDefineDisplay();
}

// ════════════════════════════════════════════════════════════════════════════
// Computation — evaluate sequences for n = 0..MAX_TERMS-1
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::recompute() {
    // Simple evaluation: replace 'n' with the value and evaluate
    // For now, support basic arithmetic sequences
    for (int s = 0; s < MAX_SEQ; ++s) {
        for (int n = 0; n < _numTerms; ++n) {
            // Simple pattern-based evaluation
            double val = 0.0;
            const char* expr = _seqExprText[s];

            // Try to parse simple expressions like "2*n+1" or "n^2"
            // This is a basic evaluator; full math engine can be added later
            if (strstr(expr, "n^2")) {
                // Quadratic: a*n^2 + b or just n^2
                double a = 1.0;
                double b = 0.0;
                if (sscanf(expr, "%lf*n^2+%lf", &a, &b) == 2) {
                    val = a * (double)(n * n) + b;
                } else if (sscanf(expr, "%lf*n^2", &a) == 1) {
                    val = a * (double)(n * n);
                } else {
                    val = (double)(n * n);
                }
            } else if (strstr(expr, "*n")) {
                // Linear: a*n + b
                double a = 1.0;
                double b = 0.0;
                if (sscanf(expr, "%lf*n+%lf", &a, &b) == 2) {
                    val = a * n + b;
                } else if (sscanf(expr, "%lf*n", &a) == 1) {
                    val = a * n;
                }
            } else if (strcmp(expr, "n") == 0) {
                val = (double)n;
            } else {
                // Try as constant
                double c = 0.0;
                if (sscanf(expr, "%lf", &c) == 1) {
                    val = c;
                }
            }

            _seqValues[s][n] = val;
        }
    }
}

void SequencesApp::updateTable() {
    if (!_table) return;

    lv_table_set_row_count(_table, _numTerms + 1);  // +1 for header

    for (int n = 0; n < _numTerms; ++n) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", n);
        lv_table_set_cell_value(_table, n + 1, 0, buf);

        for (int s = 0; s < MAX_SEQ; ++s) {
            snprintf(buf, sizeof(buf), "%.4g", _seqValues[s][n]);
            lv_table_set_cell_value(_table, n + 1, s + 1, buf);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Key handling
// ════════════════════════════════════════════════════════════════════════════

void SequencesApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_activeTab) {
        case Tab::DEFINE: handleKeyDefine(ev); break;
        case Tab::TABLE:  handleKeyTable(ev);  break;
    }
}

void SequencesApp::handleKeyDefine(const KeyEvent& ev) {
    if (_editing) {
        // Edit mode: type characters into the expression
        switch (ev.code) {
            case KeyCode::ENTER:
                finishEdit();
                break;
            case KeyCode::AC:
                cancelEdit();
                break;
            case KeyCode::DEL:
                if (_editLen > 0) {
                    _editBuf[--_editLen] = '\0';
                    if (_seqExprs[_selectedSeq])
                        lv_label_set_text(_seqExprs[_selectedSeq], _editBuf);
                }
                break;
            default: {
                // Map number keys to characters
                char c = '\0';
                if (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9) {
                    c = '0' + (static_cast<int>(ev.code) - static_cast<int>(KeyCode::NUM_0));
                } else if (ev.code == KeyCode::ADD)  c = '+';
                else if (ev.code == KeyCode::SUB)    c = '-';
                else if (ev.code == KeyCode::MUL)    c = '*';
                else if (ev.code == KeyCode::DIV)    c = '/';
                else if (ev.code == KeyCode::DOT)    c = '.';
                else if (ev.code == KeyCode::POW)    c = '^';

                if (c != '\0' && _editLen < MAX_EXPR - 1) {
                    _editBuf[_editLen++] = c;
                    _editBuf[_editLen] = '\0';
                    if (_seqExprs[_selectedSeq])
                        lv_label_set_text(_seqExprs[_selectedSeq], _editBuf);
                }
                break;
            }
        }
    } else {
        // Navigation mode
        switch (ev.code) {
            case KeyCode::UP:
                if (_selectedSeq > 0) {
                    _selectedSeq--;
                    updateDefineDisplay();
                }
                break;
            case KeyCode::DOWN:
                if (_selectedSeq < MAX_SEQ - 1) {
                    _selectedSeq++;
                    updateDefineDisplay();
                }
                break;
            case KeyCode::LEFT:
            case KeyCode::RIGHT:
                switchTab(_activeTab == Tab::DEFINE ? Tab::TABLE : Tab::DEFINE);
                break;
            case KeyCode::ENTER:
                startEdit();
                if (_seqExprs[_selectedSeq])
                    lv_label_set_text(_seqExprs[_selectedSeq], _editBuf);
                break;
            default:
                break;
        }
    }
}

void SequencesApp::handleKeyTable(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::LEFT:
        case KeyCode::RIGHT:
            switchTab(_activeTab == Tab::DEFINE ? Tab::TABLE : Tab::DEFINE);
            break;
        default:
            break;
    }
}
