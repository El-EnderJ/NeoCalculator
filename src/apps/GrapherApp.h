/**
 * GrapherApp.h — Grapher 2.0 (100 % LVGL 9)
 *
 * NumWorks-inspired graphing calculator with 3 tabs:
 *   · Expressions — add/edit up to 6 functions
 *   · Graph       — real-time plotting with toolbar
 *   · Table       — dynamic value table
 *
 * Focus hierarchy: TAB_BAR → TOOLBAR → CONTENT
 * KEY_BACK (AC) reverses focus chain; MODE returns to menu.
 */
#pragma once

#include <lvgl.h>
#include "../math/Tokenizer.h"
#include "../math/Parser.h"
#include "../math/Evaluator.h"
#include "../math/VariableContext.h"
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/MathEvaluator.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class GrapherApp {
public:
    GrapherApp();
    ~GrapherApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive()        const { return _screen != nullptr; }
    bool atTabLevel()      const { return _focus == Focus::TAB_BAR; }
    bool isOnExpressions() const { return _tab == Tab::EXPRESSIONS; }

    // Viewport access for grid draw callback
    void getViewport(float& xMin, float& xMax, float& yMin, float& yMax) const {
        xMin = _xMin; xMax = _xMax; yMin = _yMin; yMax = _yMax;
    }

private:
    // ── Constants ────────────────────────────────────────────────────
    static constexpr int MAX_FUNCS    = 6;
    static constexpr int SCREEN_W     = 320;
    static constexpr int SCREEN_H     = 240;
    static constexpr int BAR_H        = 25;   // StatusBar::HEIGHT + 1
    static constexpr int TAB_H        = 28;   // Tab header strip
    static constexpr int TOOLBAR_H    = 24;   // Graph toolbar strip
    static constexpr int INFO_BAR_H   = 20;   // Bottom info bar (x= y=)
    static constexpr int PAD          = 6;
    static constexpr int ROW_H        = 32;   // Expression row height
    static constexpr int ROW_GAP      = 2;

    // Function colours (NumWorks palette)
    static constexpr uint32_t FUNC_COLORS[MAX_FUNCS] = {
        0xCC0000,  // Red
        0x0000CC,  // Blue
        0x009900,  // Green
        0xCC6600,  // Orange
        0x6600CC,  // Purple
        0x009999,  // Teal
    };

    // ── Enums (declared before any member that uses them) ────────────
    enum class Tab      : uint8_t { EXPRESSIONS, GRAPH, TABLE };
    enum class Focus    : uint8_t { TAB_BAR, TOOLBAR, CONTENT };
    enum class ExprMode : uint8_t { LIST, EDITING };
    enum class GrMode   : uint8_t { IDLE, NAVIGATE, TRACE };

    // ── Per-function slot ────────────────────────────────────────────
    struct FuncSlot {
        char     text[64];   // expression string
        int      len;        // strlen(text)
        bool     valid;      // has parseable content
        uint32_t color;
    };

    // ── LVGL root ────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // ── Manual tab bar ───────────────────────────────────────────────
    lv_obj_t*       _tabBar;
    lv_obj_t*       _tabPills[3];       // Rounded pill backgrounds
    lv_obj_t*       _tabLabels[3];

    // ── Three content panels (mutually exclusive visibility) ─────────
    lv_obj_t*       _bgExpr;            // Static background behind exprPanel
    lv_obj_t*       _panelExpr;
    lv_obj_t*       _panelGraph;
    lv_obj_t*       _panelTable;

    // ── Expressions panel widgets ────────────────────────────────────
    lv_obj_t*       _exprRows[MAX_FUNCS];
    lv_obj_t*       _exprDots[MAX_FUNCS];
    vpam::MathCanvas _exprCanvas[MAX_FUNCS];       // VPAM canvas per slot
    vpam::NodePtr    _exprAST[MAX_FUNCS];           // Owning AST root
    vpam::NodeRow*   _exprASTRow[MAX_FUNCS];        // Raw pointer to AST row
    vpam::CursorController _exprCursor[MAX_FUNCS];  // Cursor per slot
    lv_obj_t*       _addRow;
    lv_obj_t*       _addLabel;
    lv_obj_t*       _exprHint;
    lv_obj_t*       _plotBtn;           // "Plot graph"
    lv_obj_t*       _tableBtn;          // "Display values"
    lv_obj_t*       _tplBtns[MAX_FUNCS]; // Per-row "Templates" buttons

    // ── Templates modal ──────────────────────────────────────────────
    lv_obj_t*       _tplModal;          // Modal overlay
    lv_obj_t*       _tplRows[6];        // Up to 6 template option rows
    vpam::MathCanvas _tplCanvas[6];     // VPAM preview per template
    vpam::NodePtr    _tplAST[6];        // Owning AST for template preview
    int             _tplCount;          // Number of templates shown
    int             _tplIdx;            // Focused template index
    bool            _tplOpen;           // Modal is open

    // ── Graph panel widgets ──────────────────────────────────────────
    lv_obj_t*       _graphToolbar;
    lv_obj_t*       _toolLabels[4];     // Auto  Axes  Navigate  Calculate
    lv_obj_t*       _graphArea;         // Plain container for plots
    lv_obj_t*       _axisLineX;         // lv_line for X axis
    lv_obj_t*       _axisLineY;         // lv_line for Y axis
    lv_obj_t*       _funcLines[MAX_FUNCS]; // lv_line per function
    lv_point_precise_t* _funcPts[MAX_FUNCS]; // Point arrays (heap)
    int             _funcPtCount[MAX_FUNCS]; // Valid point count per func
    lv_obj_t*       _traceDot;          // Small circle for trace cursor
    lv_obj_t*       _infoBar;           // bottom bar
    lv_obj_t*       _infoLabel;

    // ── Table panel widgets ──────────────────────────────────────────
    lv_obj_t*       _tblBodyLabel;      // single label for all table rows
    static constexpr int TBL_ROWS = 8;  // rows visible on screen
    static constexpr int TBL_COLS = 2;  // x, y=f(x)

    // ── State ────────────────────────────────────────────────────────
    Tab             _tab;
    Focus           _focus;
    int             _tabIdx;            // highlighted tab (0-2)

    // Expression state
    FuncSlot        _funcs[MAX_FUNCS];
    int             _numFuncs;
    int             _exprIdx;           // focused row
    ExprMode        _exprMode;

    // Graph state
    GrMode          _grMode;
    int             _toolIdx;           // focused toolbar item (0-3)
    float           _xMin, _xMax, _yMin, _yMax;
    float           _traceX;            // trace cursor X
    int             _traceFn;           // which function to trace
    bool            _plotDirty;

    // Table state
    int             _tblRow;            // highlighted row
    float           _tblStart;          // first x value
    float           _tblStep;           // x step
    int             _tblFuncIdx;        // which function

    // Eval engine
    Tokenizer       _tokenizer;
    Parser          _parser;
    Evaluator       _evaluator;
    VariableContext  _vars;
    vpam::MathEvaluator _vpamEval;

    // ── UI creation ──────────────────────────────────────────────────
    void createUI();
    void createTabBar();
    void createExpressionsPanel();
    void createGraphPanel();
    void createTablePanel();

    // ── Tab switching ────────────────────────────────────────────────
    void switchTab(Tab t);
    void refreshTabBar();

    // ── Expression helpers ───────────────────────────────────────────
    void refreshExprList();
    void refreshExprFocus();
    void addFunction();
    void removeFunction(int idx);
    void startEditing(int idx);
    void stopEditing();
    void refreshVPAMExpr(int idx);
    void syncASTtoText(int idx);
    void initSlotAST(int idx);
    static vpam::NodePtr buildTemplateAST(const char* text);
    int  exprItemCount() const;  // numFuncs + 1(Add) + 2(Plot/Table)
    void showTemplates();
    void closeTemplates();
    void handleTemplates(const KeyEvent& ev);
    void refreshTemplateButtons();

    // ── Graph helpers ────────────────────────────────────────────────
    void refreshToolbar();
    void replot();
    void drawAxes();
    void plotFunc(int idx);
    void drawTraceCursor();
    void updateInfoBar();
    void autoFit();

    // ── Table helpers ────────────────────────────────────────────────
    void rebuildTable();

    // ── Math ─────────────────────────────────────────────────────────
    double evalAt(int funcIdx, double x);

    // ── Key dispatchers ──────────────────────────────────────────────
    void handleTabBar(const KeyEvent& ev);
    void handleExprList(const KeyEvent& ev);
    void handleExprEdit(const KeyEvent& ev);
    void handleToolbar(const KeyEvent& ev);
    void handleGraphNav(const KeyEvent& ev);
    void handleGraphTrace(const KeyEvent& ev);
    void handleTable(const KeyEvent& ev);
};
