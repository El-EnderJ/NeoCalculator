/**
 * MatricesApp.h — Linear algebra matrix application for NumOS.
 *
 * LVGL-native app with two views:
 *   View 0 — Manager: List of matrices (MatA, MatB, MatC) + operations
 *   View 1 — Editor:  lv_table matrix editor with +/- row/col
 *
 * Supports: Add, Subtract, Multiply, Determinant, Inverse.
 * Max 5×5 matrices. Error handling for singular/dimension mismatches.
 *
 * Part of: NumOS — Linear Algebra Suite
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "MatrixEngine.h"

class MatricesApp {
public:
    MatricesApp();
    ~MatricesApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Constants ────────────────────────────────────────────────────────
    static constexpr int SCREEN_W  = 320;
    static constexpr int SCREEN_H  = 240;
    static constexpr int NUM_MATS  = 3;   // MatA, MatB, MatC
    static constexpr int MAX_DIM   = MatrixEngine::MAX_DIM;

    // ── Views ────────────────────────────────────────────────────────────
    enum class View : uint8_t {
        MANAGER = 0,    // Matrix list + operations
        EDITOR  = 1,    // Matrix table editor
        RESULT  = 2     // Operation result display
    };

    // ── Operations ───────────────────────────────────────────────────────
    enum class Op : uint8_t {
        NONE = 0,
        ADD, SUB, MUL, DET, INV
    };

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // Manager view
    lv_obj_t*       _managerPanel;
    lv_obj_t*       _matBtns[NUM_MATS];     // Matrix selection buttons
    lv_obj_t*       _matLabels[NUM_MATS];    // Button labels
    lv_obj_t*       _matDimLabels[NUM_MATS]; // Dimension labels (e.g. "2×3")
    lv_obj_t*       _opBtns[5];              // Operation buttons
    lv_obj_t*       _opLabels[5];            // Operation labels
    lv_obj_t*       _managerHint;

    // Editor view
    lv_obj_t*       _editorPanel;
    lv_obj_t*       _editorTitle;
    lv_obj_t*       _editorTable;
    lv_obj_t*       _editorHint;
    lv_obj_t*       _dimLabel;               // "3×3" dimension indicator

    // Result view
    lv_obj_t*       _resultPanel;
    lv_obj_t*       _resultTitle;
    lv_obj_t*       _resultTable;
    lv_obj_t*       _resultScalarLabel;

    // Error message overlay
    lv_obj_t*       _errorLabel;

    // ── State ────────────────────────────────────────────────────────────
    View            _view;
    int             _selectedMat;    // 0=A, 1=B, 2=C in manager
    int             _selectedItem;   // Current selected item (mat or op)
    int             _editingMat;     // Which matrix we're editing (0-2)

    // Editor state
    int             _editRow;
    int             _editCol;
    bool            _editing;
    char            _editBuf[16];
    int             _editLen;

    // Operation state
    Op              _pendingOp;
    int             _opFirstMat;     // First operand matrix index
    bool            _pickingSecond;  // Waiting for second operand

    // Matrix storage
    MatrixEngine::Matrix _matrices[NUM_MATS];

    // Result storage
    MatrixEngine::Result _lastResult;

    // ── Builders ─────────────────────────────────────────────────────────
    void createManagerView();
    void createEditorView();
    void createResultView();

    // ── View switching ───────────────────────────────────────────────────
    void showManager();
    void showEditor(int matIdx);
    void showResult();

    // ── Manager helpers ──────────────────────────────────────────────────
    void updateManagerStyles();
    void updateDimLabels();

    // ── Editor helpers ───────────────────────────────────────────────────
    void refreshEditor();
    void highlightEditorCell();
    void startCellEdit();
    void finishCellEdit();
    void cancelCellEdit();
    void addRow();
    void removeRow();
    void addCol();
    void removeCol();

    // ── Operations ───────────────────────────────────────────────────────
    void executeOp(Op op, int matA, int matB = -1);
    void showError(const char* msg);
    void hideError();

    // ── Result helpers ───────────────────────────────────────────────────
    void displayMatrixResult();
    void displayScalarResult(double val);

    // ── Key handlers per view ────────────────────────────────────────────
    void handleKeyManager(const KeyEvent& ev);
    void handleKeyEditor(const KeyEvent& ev);
    void handleKeyResult(const KeyEvent& ev);
};
