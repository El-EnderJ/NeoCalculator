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
#include <vector>
#include "../math/Tokenizer.h"
#include "../math/Parser.h"
#include "../math/Evaluator.h"
#include "../math/VariableContext.h"
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../math/MathEvaluator.h"
#include "../math/MathAnalysis.h"
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
    static constexpr int PILL_RADIUS  = 6;    // NumWorks pill corner radius
    static constexpr int PILL_PAD     = 5;    // Internal padding (all sides)
    static constexpr int TPL_LOAD_INTERVAL_MS = 30;  // Lazy template load interval
    static constexpr int MAX_POIS     = 10;   // Max pre-computed points of interest
    static constexpr int INIT_SAMPLE_N= 40;   // Initial coarse-grid sample count
    static constexpr int ADAPT_DEPTH  = 3;    // Adaptive subdivision depth
    static constexpr float ADAPT_THRESHOLD_PX = 2.0f;  // Pixel error threshold for subdivision
    static constexpr float POI_SNAP_THRESHOLD_PX = 5.0f; // Magnetic snap radius in screen pixels
    static constexpr int BISECTION_ITER = 25; // Bisection iterations for root/POI refinement
    static constexpr int TBL_HDR_H    = 22;   // Sticky table header height

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

    // ── Point of Interest (for snap-to-POI) ──────────────────────────
    struct POI {
        float x, y;
        char  label[16];   // "Root", "Min", "Max", "Intercept"
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
    lv_timer_t*     _tplLoadTimer;      // Lazy loader for template ASTs
    int             _tplLoadNext;       // Next template index to load
    int             _tplCardW;          // Card width (cached for lazy loader)
    int             _tplRowH;           // Row height (cached for lazy loader)

    // ── Graph panel widgets ──────────────────────────────────────────
    lv_obj_t*       _graphToolbar;
    lv_obj_t*       _toolLabels[4];     // Auto  Axes  Pan  Trace
    lv_obj_t*       _graphArea;         // Plain container for plots
    lv_obj_t*       _axisLineX;         // lv_line for X axis
    lv_obj_t*       _axisLineY;         // lv_line for Y axis
    lv_obj_t*       _funcLines[MAX_FUNCS]; // lv_line per function
    lv_point_precise_t* _funcPts[MAX_FUNCS]; // Point arrays (heap)
    int             _funcPtCount[MAX_FUNCS]; // Valid point count per func
    lv_obj_t*       _traceDot;          // Small circle for trace cursor
    lv_obj_t*       _traceLineH;        // Crosshair horizontal lv_line
    lv_obj_t*       _traceLineV;        // Crosshair vertical lv_line
    lv_point_precise_t _traceHPts[2];   // Crosshair H points
    lv_point_precise_t _traceVPts[2];   // Crosshair V points
    lv_obj_t*       _tracePill;         // Floating bottom pill container
    lv_obj_t*       _tracePillDot;      // Color dot in pill
    lv_obj_t*       _tracePillLabel;    // "x: ... y: ..." label in pill
    lv_obj_t*       _infoBar;           // bottom bar
    lv_obj_t*       _infoLabel;
    lv_obj_t*       _modeBadge;         // Mode indicator "[Trace]" / "[Pan]"

    // ── Calculate menu (floating overlay) ────────────────────────────
    static constexpr int CALC_MENU_ITEMS = 5;
    lv_obj_t*       _calcMenu;          // Floating menu container (nullptr when closed)
    lv_obj_t*       _calcMenuRows[CALC_MENU_ITEMS]; // Menu option labels
    int             _calcMenuIdx;       // Currently highlighted menu item
    bool            _calcMenuOpen;      // Menu is visible

    // ── Integral area shading ────────────────────────────────────────
    lv_obj_t*       _shadingLines[320]; // Vertical lines for area shading (max 320px)
    int             _shadingCount;      // Number of active shading lines
    bool            _shadingActive;     // Shading is currently displayed

    // ── Table panel widgets ──────────────────────────────────────────
    lv_obj_t*       _tblTable;          // native lv_table widget
    lv_obj_t*       _tblHeaderBar;      // Sticky header bar (above scrollable table)
    lv_obj_t*       _tblHdrLabels[2];   // Header label widgets: "x" and "f(x)"
    static constexpr int TBL_ROWS = 21; // data rows in table
    static constexpr int TBL_COLS = 1 + MAX_FUNCS;  // x + one column per function

    // ── Cached RPN per function (eliminates re-parsing in evalAt) ────
    std::vector<Token>  _cachedRPN[MAX_FUNCS];
    bool                _rpnCacheValid[MAX_FUNCS];

    // ── Points of interest (roots, extrema) for snap-to-POI ─────────
    POI  _pois[MAX_POIS];
    int  _numPOIs;

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
    static void tplLoadTimerCb(lv_timer_t* t);  // Lazy template AST loader callback
    void loadNextTemplate();                      // Load one template AST per timer tick

    // ── Graph helpers ────────────────────────────────────────────────
    void refreshToolbar();
    void replot();
    void drawAxes();
    void plotFunc(int idx);
    void drawTraceCursor();
    void updateInfoBar();
    void autoFit();

    // Adaptive sampling: replaces pixel-by-pixel plotFunc
    int  sampleFuncAdaptive(int fi, int areaW, int areaH);
    static void adaptSeg(GrapherApp* app, int fi,
                         float xMin, float xRange,
                         float yMin, float yRange,
                         float areaW, float areaH,
                         float wx0, float sy0,
                         float wx1, float sy1,
                         int depth,
                         lv_point_precise_t* pts, int& n, int maxN);

    // POI (snap-to-point) helpers
    void preCacheFuncRPN(int idx);
    void computePOIs(int funcIdx);
    void snapToPOI();

    // ── Calculate menu helpers ───────────────────────────────────────
    void openCalcMenu();
    void closeCalcMenu();
    void handleCalcMenu(const KeyEvent& ev);
    void executeCalcOption(int option);

    // ── Integral shading ─────────────────────────────────────────────
    void drawIntegralShading(int funcIdx, float shadeXMin, float shadeXMax);
    void clearIntegralShading();

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
