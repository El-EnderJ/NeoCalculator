/**
 * MatricesApp.cpp — Linear algebra matrix application for NumOS.
 *
 * Two-view LVGL app: Matrix Manager + Matrix Editor.
 * Supports: Add, Subtract, Multiply, Determinant, Inverse.
 *
 * Part of: NumOS — Linear Algebra Suite
 */

#include "MatricesApp.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ══ Color palette ════════════════════════════════════════════════════════
static constexpr uint32_t COL_BG         = 0xFFFFFF;
static constexpr uint32_t COL_TEXT       = 0x1A1A1A;
static constexpr uint32_t COL_HINT       = 0x808080;
static constexpr uint32_t COL_HEADER_BG  = 0xE0E0E0;
static constexpr uint32_t COL_SEL        = 0xFFB500;   // NumWorks yellow
static constexpr uint32_t COL_SEL_TEXT   = 0x1A1A1A;
static constexpr uint32_t COL_UNSEL      = 0xF0F0F0;
static constexpr uint32_t COL_BTN_OP     = 0x4A90D9;
static constexpr uint32_t COL_BTN_OP_TXT = 0xFFFFFF;
static constexpr uint32_t COL_BORDER     = 0xD0D0D0;
static constexpr uint32_t COL_ERROR_BG   = 0xFFEBEE;
static constexpr uint32_t COL_ERROR_TXT  = 0xC62828;
static constexpr uint32_t COL_RES_TITLE  = 0x1565C0;

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

MatricesApp::MatricesApp()
    : _screen(nullptr)
    , _managerPanel(nullptr)
    , _managerHint(nullptr)
    , _editorPanel(nullptr)
    , _editorTitle(nullptr)
    , _editorTable(nullptr)
    , _editorHint(nullptr)
    , _dimLabel(nullptr)
    , _resultPanel(nullptr)
    , _resultTitle(nullptr)
    , _resultTable(nullptr)
    , _resultScalarLabel(nullptr)
    , _errorLabel(nullptr)
    , _view(View::MANAGER)
    , _selectedMat(0)
    , _selectedItem(0)
    , _editingMat(0)
    , _editRow(0)
    , _editCol(0)
    , _editing(false)
    , _editLen(0)
    , _pendingOp(Op::NONE)
    , _opFirstMat(-1)
    , _pickingSecond(false)
{
    memset(_editBuf, 0, sizeof(_editBuf));
    memset(_matBtns, 0, sizeof(_matBtns));
    memset(_matLabels, 0, sizeof(_matLabels));
    memset(_matDimLabels, 0, sizeof(_matDimLabels));
    memset(_opBtns, 0, sizeof(_opBtns));
    memset(_opLabels, 0, sizeof(_opLabels));

    // Initialize default matrices (2×2)
    for (int i = 0; i < NUM_MATS; ++i) {
        _matrices[i] = MatrixEngine::Matrix(2, 2);
    }
}

MatricesApp::~MatricesApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin — Create LVGL screen
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Matrices");

    createManagerView();
    createEditorView();
    createResultView();

    showManager();
}

// ════════════════════════════════════════════════════════════════════════════
// end — Destroy screen
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::end() {
    if (_screen) {
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen       = nullptr;
        _managerPanel = nullptr;
        _managerHint  = nullptr;
        _editorPanel  = nullptr;
        _editorTitle  = nullptr;
        _editorTable  = nullptr;
        _editorHint   = nullptr;
        _dimLabel     = nullptr;
        _resultPanel  = nullptr;
        _resultTitle  = nullptr;
        _resultTable  = nullptr;
        _resultScalarLabel = nullptr;
        _errorLabel   = nullptr;
        memset(_matBtns, 0, sizeof(_matBtns));
        memset(_matLabels, 0, sizeof(_matLabels));
        memset(_matDimLabels, 0, sizeof(_matDimLabels));
        memset(_opBtns, 0, sizeof(_opBtns));
        memset(_opLabels, 0, sizeof(_opLabels));
    }
}

// ════════════════════════════════════════════════════════════════════════════
// load — Activate the screen
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Matrices");
    _statusBar.update();
    _view = View::MANAGER;
    _selectedItem = 0;
    _pickingSecond = false;
    _pendingOp = Op::NONE;
    showManager();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ════════════════════════════════════════════════════════════════════════════
// createManagerView — Matrix list + operation buttons
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::createManagerView() {
    int topY = ui::StatusBar::HEIGHT + 1;
    int panelH = SCREEN_H - topY;

    _managerPanel = lv_obj_create(_screen);
    lv_obj_set_size(_managerPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_managerPanel, 0, topY);
    lv_obj_set_style_bg_opa(_managerPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_managerPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_managerPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_managerPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_managerPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Section title: Matrices
    lv_obj_t* secTitle = lv_label_create(_managerPanel);
    lv_label_set_text(secTitle, "Matrices");
    lv_obj_set_style_text_font(secTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(secTitle, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(secTitle, 12, 6);

    // Matrix buttons (3 matrices)
    const char* matNames[NUM_MATS] = { "MatA", "MatB", "MatC" };
    int btnW = 90, btnH = 50;
    int startX = 12;
    int btnY = 28;

    for (int i = 0; i < NUM_MATS; ++i) {
        _matBtns[i] = lv_obj_create(_managerPanel);
        lv_obj_set_size(_matBtns[i], btnW, btnH);
        lv_obj_set_pos(_matBtns[i], startX + i * (btnW + 10), btnY);
        lv_obj_set_style_radius(_matBtns[i], 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(_matBtns[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_matBtns[i], lv_color_hex(COL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_pad_all(_matBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_matBtns[i], LV_OBJ_FLAG_SCROLLABLE);

        _matLabels[i] = lv_label_create(_matBtns[i]);
        lv_label_set_text(_matLabels[i], matNames[i]);
        lv_obj_set_style_text_font(_matLabels[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(_matLabels[i], LV_ALIGN_TOP_MID, 0, 6);

        _matDimLabels[i] = lv_label_create(_matBtns[i]);
        lv_obj_set_style_text_font(_matDimLabels[i], &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(_matDimLabels[i], lv_color_hex(COL_HINT), LV_PART_MAIN);
        lv_obj_align(_matDimLabels[i], LV_ALIGN_BOTTOM_MID, 0, -6);
    }
    updateDimLabels();

    // Section title: Operations
    lv_obj_t* opTitle = lv_label_create(_managerPanel);
    lv_label_set_text(opTitle, "Operations");
    lv_obj_set_style_text_font(opTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(opTitle, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(opTitle, 12, btnY + btnH + 14);

    // Operation buttons
    const char* opNames[5] = { "A+B", "A-B", "A\xC3\x97""B", "det", "A\xE2\x81\xBB\xC2\xB9" };
    int opBtnW = 52, opBtnH = 32;
    int opY = btnY + btnH + 36;

    for (int i = 0; i < 5; ++i) {
        _opBtns[i] = lv_obj_create(_managerPanel);
        lv_obj_set_size(_opBtns[i], opBtnW, opBtnH);
        lv_obj_set_pos(_opBtns[i], 12 + i * (opBtnW + 6), opY);
        lv_obj_set_style_bg_color(_opBtns[i], lv_color_hex(COL_BTN_OP), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_opBtns[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(_opBtns[i], 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(_opBtns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_opBtns[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_opBtns[i], LV_OBJ_FLAG_SCROLLABLE);

        _opLabels[i] = lv_label_create(_opBtns[i]);
        lv_label_set_text(_opLabels[i], opNames[i]);
        lv_obj_set_style_text_font(_opLabels[i], &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(_opLabels[i], lv_color_hex(COL_BTN_OP_TXT), LV_PART_MAIN);
        lv_obj_align(_opLabels[i], LV_ALIGN_CENTER, 0, 0);
    }

    // Hint
    _managerHint = lv_label_create(_managerPanel);
    lv_label_set_text(_managerHint,
        LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Select  ENTER Open/Run  " LV_SYMBOL_UP LV_SYMBOL_DOWN " Row");
    lv_obj_set_style_text_font(_managerHint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_managerHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_managerHint, 6, panelH - 18);

    // Error label (initially hidden)
    _errorLabel = lv_label_create(_managerPanel);
    lv_obj_set_style_text_font(_errorLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_errorLabel, lv_color_hex(COL_ERROR_TXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_errorLabel, lv_color_hex(COL_ERROR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_errorLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_errorLabel, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(_errorLabel, 4, LV_PART_MAIN);
    lv_obj_set_pos(_errorLabel, 12, panelH - 40);
    lv_label_set_text(_errorLabel, "");
    lv_obj_add_flag(_errorLabel, LV_OBJ_FLAG_HIDDEN);
}

// ════════════════════════════════════════════════════════════════════════════
// createEditorView — Matrix table editor
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::createEditorView() {
    int topY = ui::StatusBar::HEIGHT + 1;
    int panelH = SCREEN_H - topY;

    _editorPanel = lv_obj_create(_screen);
    lv_obj_set_size(_editorPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_editorPanel, 0, topY);
    lv_obj_set_style_bg_opa(_editorPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_editorPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_editorPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_editorPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_editorPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    _editorTitle = lv_label_create(_editorPanel);
    lv_label_set_text(_editorTitle, "Edit MatA");
    lv_obj_set_style_text_font(_editorTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_editorTitle, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(_editorTitle, 8, 4);

    // Dimension label
    _dimLabel = lv_label_create(_editorPanel);
    lv_label_set_text(_dimLabel, "2\xC3\x97""2");
    lv_obj_set_style_text_font(_dimLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_dimLabel, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_dimLabel, SCREEN_W - 60, 6);

    // Table
    _editorTable = lv_table_create(_editorPanel);
    lv_obj_set_pos(_editorTable, 8, 26);
    lv_obj_set_style_text_font(_editorTable, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_border_width(_editorTable, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_editorTable, lv_color_hex(COL_BORDER), LV_PART_MAIN);

    // Hint
    _editorHint = lv_label_create(_editorPanel);
    lv_label_set_text(_editorHint,
        "F1 +Row  F2 -Row  F3 +Col  F4 -Col  AC Back");
    lv_obj_set_style_text_font(_editorHint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(_editorHint, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_editorHint, 6, panelH - 18);
}

// ════════════════════════════════════════════════════════════════════════════
// createResultView — Operation result display
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::createResultView() {
    int topY = ui::StatusBar::HEIGHT + 1;
    int panelH = SCREEN_H - topY;

    _resultPanel = lv_obj_create(_screen);
    lv_obj_set_size(_resultPanel, SCREEN_W, panelH);
    lv_obj_set_pos(_resultPanel, 0, topY);
    lv_obj_set_style_bg_opa(_resultPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_resultPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_resultPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_resultPanel, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    _resultTitle = lv_label_create(_resultPanel);
    lv_label_set_text(_resultTitle, "Result");
    lv_obj_set_style_text_font(_resultTitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultTitle, lv_color_hex(COL_RES_TITLE), LV_PART_MAIN);
    lv_obj_set_pos(_resultTitle, 8, 4);

    // Result table
    _resultTable = lv_table_create(_resultPanel);
    lv_obj_set_pos(_resultTable, 8, 26);
    lv_obj_set_style_text_font(_resultTable, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_border_width(_resultTable, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_resultTable, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_add_flag(_resultTable, LV_OBJ_FLAG_HIDDEN);

    // Scalar result label
    _resultScalarLabel = lv_label_create(_resultPanel);
    lv_label_set_text(_resultScalarLabel, "");
    lv_obj_set_style_text_font(_resultScalarLabel, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(_resultScalarLabel, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(_resultScalarLabel, 20, 60);
    lv_obj_add_flag(_resultScalarLabel, LV_OBJ_FLAG_HIDDEN);
}

// ════════════════════════════════════════════════════════════════════════════
// showManager — Switch to manager view
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::showManager() {
    _view = View::MANAGER;
    _editing = false;
    _pickingSecond = false;
    hideError();

    if (_managerPanel) lv_obj_remove_flag(_managerPanel, LV_OBJ_FLAG_HIDDEN);
    if (_editorPanel)  lv_obj_add_flag(_editorPanel,  LV_OBJ_FLAG_HIDDEN);
    if (_resultPanel)  lv_obj_add_flag(_resultPanel,  LV_OBJ_FLAG_HIDDEN);

    updateDimLabels();
    updateManagerStyles();
}

// ════════════════════════════════════════════════════════════════════════════
// showEditor — Switch to editor view for a specific matrix
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::showEditor(int matIdx) {
    if (matIdx < 0 || matIdx >= NUM_MATS) return;

    _view = View::EDITOR;
    _editingMat = matIdx;
    _editRow = 0;
    _editCol = 0;
    _editing = false;

    if (_managerPanel) lv_obj_add_flag(_managerPanel, LV_OBJ_FLAG_HIDDEN);
    if (_editorPanel)  lv_obj_remove_flag(_editorPanel, LV_OBJ_FLAG_HIDDEN);
    if (_resultPanel)  lv_obj_add_flag(_resultPanel,  LV_OBJ_FLAG_HIDDEN);

    // Update title
    char title[32];
    const char* names[] = { "MatA", "MatB", "MatC" };
    snprintf(title, sizeof(title), "Edit %s", names[matIdx]);
    if (_editorTitle) lv_label_set_text(_editorTitle, title);

    refreshEditor();
}

// ════════════════════════════════════════════════════════════════════════════
// showResult — Switch to result view
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::showResult() {
    _view = View::RESULT;

    if (_managerPanel) lv_obj_add_flag(_managerPanel, LV_OBJ_FLAG_HIDDEN);
    if (_editorPanel)  lv_obj_add_flag(_editorPanel,  LV_OBJ_FLAG_HIDDEN);
    if (_resultPanel)  lv_obj_remove_flag(_resultPanel, LV_OBJ_FLAG_HIDDEN);
}

// ════════════════════════════════════════════════════════════════════════════
// updateManagerStyles — Highlight selected item
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::updateManagerStyles() {
    // Items 0-2: matrices, items 3-7: operations
    for (int i = 0; i < NUM_MATS; ++i) {
        if (!_matBtns[i]) continue;
        bool sel = (_selectedItem == i);
        lv_obj_set_style_bg_color(_matBtns[i],
            lv_color_hex(sel ? COL_SEL : COL_UNSEL), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_matBtns[i], LV_OPA_COVER, LV_PART_MAIN);
    }
    for (int i = 0; i < 5; ++i) {
        if (!_opBtns[i]) continue;
        bool sel = (_selectedItem == NUM_MATS + i);
        if (sel) {
            lv_obj_set_style_bg_color(_opBtns[i], lv_color_hex(COL_SEL), LV_PART_MAIN);
            lv_obj_set_style_text_color(_opLabels[i], lv_color_hex(COL_SEL_TEXT), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_opBtns[i], lv_color_hex(COL_BTN_OP), LV_PART_MAIN);
            lv_obj_set_style_text_color(_opLabels[i], lv_color_hex(COL_BTN_OP_TXT), LV_PART_MAIN);
        }
    }

    // Update hint when picking second operand
    if (_pickingSecond && _managerHint) {
        lv_label_set_text(_managerHint, "Select second matrix (A/B/C) then ENTER");
    } else if (_managerHint) {
        lv_label_set_text(_managerHint,
            LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Select  ENTER Open/Run  " LV_SYMBOL_UP LV_SYMBOL_DOWN " Row");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// updateDimLabels — Refresh dimension labels in manager
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::updateDimLabels() {
    char buf[16];
    for (int i = 0; i < NUM_MATS; ++i) {
        if (_matDimLabels[i]) {
            snprintf(buf, sizeof(buf), "%d\xC3\x97%d",
                     _matrices[i].rows, _matrices[i].cols);
            lv_label_set_text(_matDimLabels[i], buf);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// refreshEditor — Update editor table from matrix data
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::refreshEditor() {
    if (!_editorTable) return;

    auto& mat = _matrices[_editingMat];
    int rows = mat.rows;
    int cols = mat.cols;

    lv_table_set_row_count(_editorTable, rows);
    lv_table_set_column_count(_editorTable, cols);

    // Set column widths (evenly distributed)
    int colW = (SCREEN_W - 24) / cols;
    if (colW > 60) colW = 60;
    for (int c = 0; c < cols; ++c) {
        lv_table_set_column_width(_editorTable, c, colW);
    }
    lv_obj_set_size(_editorTable, colW * cols + 4, LV_SIZE_CONTENT);

    // Fill cells
    char buf[16];
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            snprintf(buf, sizeof(buf), "%.4g", mat.data[r][c]);
            lv_table_set_cell_value(_editorTable, r, c, buf);
        }
    }

    // Update dimension label
    if (_dimLabel) {
        snprintf(buf, sizeof(buf), "%d\xC3\x97%d", rows, cols);
        lv_label_set_text(_dimLabel, buf);
    }

    highlightEditorCell();
}

// ════════════════════════════════════════════════════════════════════════════
// highlightEditorCell — Visual indicator for focused cell
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::highlightEditorCell() {
    if (!_editorTable) return;
    lv_table_set_selected_cell(_editorTable, _editRow, _editCol);
}

// ════════════════════════════════════════════════════════════════════════════
// Cell editing
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::startCellEdit() {
    _editing = true;
    _editLen = 0;
    _editBuf[0] = '\0';

    auto& mat = _matrices[_editingMat];
    double val = mat.data[_editRow][_editCol];
    if (val != 0.0) {
        snprintf(_editBuf, sizeof(_editBuf), "%.4g", val);
        _editLen = strlen(_editBuf);
    }

    char display[20];
    snprintf(display, sizeof(display), ">%s_", _editBuf);
    lv_table_set_cell_value(_editorTable, _editRow, _editCol, display);
}

void MatricesApp::finishCellEdit() {
    if (!_editing) return;
    _editing = false;

    double val = 0.0;
    if (_editLen > 0) {
        val = atof(_editBuf);
    }
    _matrices[_editingMat].data[_editRow][_editCol] = val;
    refreshEditor();
}

void MatricesApp::cancelCellEdit() {
    _editing = false;
    refreshEditor();
}

// ════════════════════════════════════════════════════════════════════════════
// Dimension adjustment
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::addRow() {
    auto& mat = _matrices[_editingMat];
    if (mat.rows >= MAX_DIM) return;
    // New row initialized to 0 (already zeroed in Matrix constructor)
    for (int c = 0; c < mat.cols; ++c)
        mat.data[mat.rows][c] = 0.0;
    mat.rows++;
    refreshEditor();
}

void MatricesApp::removeRow() {
    auto& mat = _matrices[_editingMat];
    if (mat.rows <= 1) return;
    mat.rows--;
    if (_editRow >= mat.rows) _editRow = mat.rows - 1;
    refreshEditor();
}

void MatricesApp::addCol() {
    auto& mat = _matrices[_editingMat];
    if (mat.cols >= MAX_DIM) return;
    for (int r = 0; r < mat.rows; ++r)
        mat.data[r][mat.cols] = 0.0;
    mat.cols++;
    refreshEditor();
}

void MatricesApp::removeCol() {
    auto& mat = _matrices[_editingMat];
    if (mat.cols <= 1) return;
    mat.cols--;
    if (_editCol >= mat.cols) _editCol = mat.cols - 1;
    refreshEditor();
}

// ════════════════════════════════════════════════════════════════════════════
// executeOp — Run matrix operation
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::executeOp(Op op, int matA, int matB) {
    hideError();

    switch (op) {
        case Op::ADD:
            if (matB < 0 || matB >= NUM_MATS) return;
            _lastResult = MatrixEngine::add(_matrices[matA], _matrices[matB]);
            break;
        case Op::SUB:
            if (matB < 0 || matB >= NUM_MATS) return;
            _lastResult = MatrixEngine::subtract(_matrices[matA], _matrices[matB]);
            break;
        case Op::MUL:
            if (matB < 0 || matB >= NUM_MATS) return;
            _lastResult = MatrixEngine::multiply(_matrices[matA], _matrices[matB]);
            break;
        case Op::DET:
            _lastResult = MatrixEngine::determinant(_matrices[matA]);
            break;
        case Op::INV:
            _lastResult = MatrixEngine::inverse(_matrices[matA]);
            break;
        default:
            return;
    }

    if (!_lastResult.ok) {
        showError(_lastResult.error ? _lastResult.error : "Error");
        return;
    }

    if (op == Op::DET) {
        displayScalarResult(_lastResult.scalar);
    } else {
        displayMatrixResult();
    }
    showResult();
}

// ════════════════════════════════════════════════════════════════════════════
// showError / hideError — Error messages
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::showError(const char* msg) {
    if (_errorLabel) {
        lv_label_set_text(_errorLabel, msg);
        lv_obj_remove_flag(_errorLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

void MatricesApp::hideError() {
    if (_errorLabel) {
        lv_obj_add_flag(_errorLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// displayMatrixResult — Show result matrix in table
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::displayMatrixResult() {
    if (!_resultTable) return;

    auto& mat = _lastResult.mat;
    lv_table_set_row_count(_resultTable, mat.rows);
    lv_table_set_column_count(_resultTable, mat.cols);

    int colW = (SCREEN_W - 24) / mat.cols;
    if (colW > 60) colW = 60;
    for (int c = 0; c < mat.cols; ++c)
        lv_table_set_column_width(_resultTable, c, colW);

    char buf[16];
    for (int r = 0; r < mat.rows; ++r) {
        for (int c = 0; c < mat.cols; ++c) {
            snprintf(buf, sizeof(buf), "%.4g", mat.data[r][c]);
            lv_table_set_cell_value(_resultTable, r, c, buf);
        }
    }

    lv_obj_remove_flag(_resultTable, LV_OBJ_FLAG_HIDDEN);
    if (_resultScalarLabel) lv_obj_add_flag(_resultScalarLabel, LV_OBJ_FLAG_HIDDEN);
}

// ════════════════════════════════════════════════════════════════════════════
// displayScalarResult — Show determinant value
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::displayScalarResult(double val) {
    if (_resultTable) lv_obj_add_flag(_resultTable, LV_OBJ_FLAG_HIDDEN);

    if (_resultScalarLabel) {
        char buf[48];
        snprintf(buf, sizeof(buf), "det = %.6g", val);
        lv_label_set_text(_resultScalarLabel, buf);
        lv_obj_remove_flag(_resultScalarLabel, LV_OBJ_FLAG_HIDDEN);
    }

    if (_resultTitle) {
        lv_label_set_text(_resultTitle, "Determinant");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKey — Route to active view handler
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_view) {
        case View::MANAGER: handleKeyManager(ev); break;
        case View::EDITOR:  handleKeyEditor(ev);  break;
        case View::RESULT:  handleKeyResult(ev);   break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyManager — Matrix list + operation selection
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::handleKeyManager(const KeyEvent& ev) {
    hideError();

    int totalItems = NUM_MATS + 5;  // 3 matrices + 5 operations

    if (_pickingSecond) {
        // Navigate matrices only
        if (ev.code == KeyCode::LEFT) {
            if (_selectedItem > 0) { --_selectedItem; updateManagerStyles(); }
            return;
        }
        if (ev.code == KeyCode::RIGHT) {
            if (_selectedItem < NUM_MATS - 1) { ++_selectedItem; updateManagerStyles(); }
            return;
        }
        if (ev.code == KeyCode::ENTER) {
            if (_selectedItem < NUM_MATS) {
                if (_pendingOp == Op::DET || _pendingOp == Op::INV) {
                    // Single-operand: selected matrix is the operand
                    executeOp(_pendingOp, _selectedItem);
                } else if (_opFirstMat < 0) {
                    // Binary op: first operand chosen, now pick second
                    _opFirstMat = _selectedItem;
                    if (_managerHint)
                        lv_label_set_text(_managerHint, "Select SECOND matrix, then ENTER");
                    updateManagerStyles();
                    return;
                } else {
                    // Binary op: second operand chosen, execute
                    executeOp(_pendingOp, _opFirstMat, _selectedItem);
                }
                _pickingSecond = false;
                _pendingOp = Op::NONE;
                _opFirstMat = -1;
            }
            return;
        }
        if (ev.code == KeyCode::AC) {
            _pickingSecond = false;
            _pendingOp = Op::NONE;
            _opFirstMat = -1;
            updateManagerStyles();
            return;
        }
        return;
    }

    switch (ev.code) {
        case KeyCode::LEFT:
            if (_selectedItem > 0) {
                --_selectedItem;
                updateManagerStyles();
            }
            break;
        case KeyCode::RIGHT:
            if (_selectedItem < totalItems - 1) {
                ++_selectedItem;
                updateManagerStyles();
            }
            break;
        case KeyCode::UP:
            // Move from operations row to matrices row
            if (_selectedItem >= NUM_MATS) {
                _selectedItem = (_selectedItem - NUM_MATS) % NUM_MATS;
                if (_selectedItem >= NUM_MATS) _selectedItem = 0;
                updateManagerStyles();
            }
            break;
        case KeyCode::DOWN:
            // Move from matrices row to operations row
            if (_selectedItem < NUM_MATS) {
                _selectedItem = NUM_MATS + _selectedItem;
                if (_selectedItem >= totalItems) _selectedItem = totalItems - 1;
                updateManagerStyles();
            }
            break;
        case KeyCode::ENTER:
            if (_selectedItem < NUM_MATS) {
                // Open matrix editor
                showEditor(_selectedItem);
            } else {
                // Execute operation
                int opIdx = _selectedItem - NUM_MATS;
                Op ops[] = { Op::ADD, Op::SUB, Op::MUL, Op::DET, Op::INV };
                Op op = ops[opIdx];

                if (op == Op::DET || op == Op::INV) {
                    // Single operand: let user pick which matrix
                    _pendingOp = op;
                    _opFirstMat = -1;
                    _selectedItem = 0;  // Highlight MatA
                    _pickingSecond = true;
                    if (_managerHint) {
                        const char* opName = (op == Op::DET) ? "det" : "inverse";
                        char hint[80];
                        snprintf(hint, sizeof(hint), "Select matrix for %s, then ENTER", opName);
                        lv_label_set_text(_managerHint, hint);
                    }
                    updateManagerStyles();
                } else {
                    // Two operand: pick first, then second
                    _pendingOp = op;
                    _opFirstMat = -1;
                    _selectedItem = 0;
                    _pickingSecond = true;
                    if (_managerHint) {
                        lv_label_set_text(_managerHint, "Select FIRST matrix, then ENTER");
                    }
                    updateManagerStyles();
                }
            }
            break;
        default:
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyEditor — Matrix cell editing
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::handleKeyEditor(const KeyEvent& ev) {
    auto& mat = _matrices[_editingMat];

    if (_editing) {
        // Numeric input
        bool isDigit = (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9);
        if (isDigit && _editLen < 14) {
            _editBuf[_editLen++] = '0' + (static_cast<int>(ev.code) - static_cast<int>(KeyCode::NUM_0));
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_editorTable, _editRow, _editCol, display);
            return;
        }
        if (ev.code == KeyCode::DOT && _editLen < 14) {
            _editBuf[_editLen++] = '.';
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_editorTable, _editRow, _editCol, display);
            return;
        }
        if (ev.code == KeyCode::SUB && _editLen == 0) {
            _editBuf[_editLen++] = '-';
            _editBuf[_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_editorTable, _editRow, _editCol, display);
            return;
        }
        if (ev.code == KeyCode::DEL && _editLen > 0) {
            _editBuf[--_editLen] = '\0';
            char display[20];
            snprintf(display, sizeof(display), ">%s_", _editBuf);
            lv_table_set_cell_value(_editorTable, _editRow, _editCol, display);
            return;
        }
        if (ev.code == KeyCode::ENTER) {
            finishCellEdit();
            return;
        }
        if (ev.code == KeyCode::AC) {
            cancelCellEdit();
            return;
        }
        return;
    }

    // Navigation / dimension control
    switch (ev.code) {
        case KeyCode::UP:
            if (_editRow > 0) { --_editRow; highlightEditorCell(); }
            break;
        case KeyCode::DOWN:
            if (_editRow < mat.rows - 1) { ++_editRow; highlightEditorCell(); }
            break;
        case KeyCode::LEFT:
            if (_editCol > 0) { --_editCol; highlightEditorCell(); }
            break;
        case KeyCode::RIGHT:
            if (_editCol < mat.cols - 1) { ++_editCol; highlightEditorCell(); }
            break;
        case KeyCode::ENTER:
            startCellEdit();
            break;
        case KeyCode::F1:
            addRow();
            break;
        case KeyCode::F2:
            removeRow();
            break;
        case KeyCode::F3:
            addCol();
            break;
        case KeyCode::F4:
            removeCol();
            break;
        case KeyCode::AC:
            // Return to manager
            showManager();
            break;
        default:
            // Direct digit input starts editing
            if (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9) {
                startCellEdit();
                handleKeyEditor(ev);
            }
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleKeyResult — Result view (read-only, AC to go back)
// ════════════════════════════════════════════════════════════════════════════

void MatricesApp::handleKeyResult(const KeyEvent& ev) {
    if (ev.code == KeyCode::AC || ev.code == KeyCode::ENTER) {
        showManager();
    }
}
