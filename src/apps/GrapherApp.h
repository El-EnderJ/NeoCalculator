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
#include "../math/MathAST.h"
#include "../math/CursorController.h"
#include "../utils/MemoryUtils.h"
#include "../math/MathEvaluator.h"
#include "../math/MathAnalysis.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "GraphModel.h"
#include "../ui/GraphView.h"

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

#ifdef NATIVE_SIM
    // ── Emulator-only debug accessors (GR-14 assert hooks) ────────────────
    // Read-only, allocation-free views of the model state for .numos semantic
    // asserts (assert_graph_slot_kind etc. in NativeHal). Kind tokens follow
    // the classifier contract: explicitY (y=f(x)), explicitX (x=f(y)),
    // implicit, ineqStrict, ineqNonStrict, invalid, empty. Never compiled into
    // firmware (NATIVE_SIM is defined only by [env:emulator_pc]).
    int         debugRelationCount() const { return _numFuncs; }
    const char* debugSlotKind(int i) const;
    bool        debugSlotValid(int i) const;
    const char* debugSlotInvalidReason(int i) const;  // "" when valid; static buf
    const char* debugSlotRelationOp(int i) const;     // eq|lt|gt|le|ge
    const char* debugSlotExprText(int i) const;       // serialized text[64]
    const char* debugTraceMode() const;               // idle|navigate|trace
    int         debugTraceFn() const { return _traceFn; }
    int         debugIntersectionCount() const;       // POIs of type Intersection
    const char* debugAngleMode() const { return _model.debugAngleMode(); }  // deg|rad (evaluator's last-synced mode)

    // ── GIAC-C01 hooks: engine identity, per-slot compiled cache, probes ──
    // debugSlotEvalParam evaluates the slot's single-parameter function
    // (y=f(x): parameter x, result y; x=f(y): parameter y, result x) and
    // debugSlotEvalResidual the implicit residual G(x,y) — both through the
    // SAME GraphModel entry points drawing/trace/POIs use, so the hooks can
    // never observe a different evaluator than the renderer.
    const char* debugGraphEngine() const { return grapher::GraphModel::debugEngineName(); }
    bool  debugSlotCompileOk(int i) const;
    int   debugSlotCompileCount(int i) const;   // -1 in the legacy build
    float debugSlotEvalParam(int i, float t);       // NAN = invalid sample
    float debugSlotEvalResidual(int i, float x, float y);
#endif

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
    static constexpr int MAX_POIS     = 20;   // Max pre-computed points of interest (roots + intersections)
    static constexpr int BISECTION_ITER = 25; // Bisection iterations for root/POI refinement
    static constexpr float POI_SNAP_THRESHOLD_PX = 8.0f; // Magnetic snap radius in screen pixels
    static constexpr int TBL_HDR_H    = 22;   // Sticky table header height
    static constexpr float TRACE_IMPLICIT_STEP_PX = 4.0f; // Implicit trace arc-length per arrow (px)
    static constexpr int   TRACE_NEWTON_ITERS     = 8;    // Corrector iterations onto G(x,y)=0

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

    // How the trace cursor walks the active slot:
    //   ExplicitX  y = f(x): parameter is x  (LEFT/RIGHT step x; fast sampler).
    //   ExplicitY  x = f(y): parameter is y  (LEFT/RIGHT step y; exact f(y)).
    //   Implicit   G(x,y)=0: predictor–corrector along the contour arc-length.
    //   None       slot is not a traceable curve (inequality / invalid).
    enum class TraceKind : uint8_t { None, ExplicitX, ExplicitY, Implicit };

    // ── Per-function slot (MVC: Model data, stored here for UI access) ──
    using FuncSlot = grapher::CartesianFunction;

    enum class POIType : uint8_t {
        Root,
        Min,
        Max,
        Intercept,
        Intersection
    };

    // ── Point of Interest (for snap-to-POI) ──────────────────────────
    struct POI {
        float x, y;
        POIType type;
        char  label[22];   // "Root", "Min", "Max", "Intercept", "Intersection"
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

    // ── Kandinsky canvas constants (compile-time) ────────────────────
    static constexpr int GRAPH_CANVAS_W = SCREEN_W;
    static constexpr int GRAPH_CANVAS_H = SCREEN_H - BAR_H - TAB_H - TOOLBAR_H - INFO_BAR_H;

    // ── Graph panel widgets ──────────────────────────────────────────
    lv_obj_t*       _graphToolbar;
    lv_obj_t*       _toolLabels[4];     // Auto  Axes  Pan  Trace
    lv_obj_t*       _graphArea;         // Plain container for plots
    lv_obj_t*       _graphCanvas;       // lv_image showing the raw pixel canvas
    utils::PSRAMBuffer<uint16_t> _graphBuf;  // PSRAM RGB565 pixel buffer (Kandinsky)
    lv_image_dsc_t  _graphImgDsc;       // LVGL image descriptor for _graphBuf
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
    static constexpr int CALC_MENU_ITEMS = 6;  // +1 for "Draw Tangent"
    static_assert(CALC_MENU_ITEMS == 6, "CALC_MENU_ITEMS must match CALC_MENU_LABELS");
    lv_obj_t*       _calcMenu;          // Floating menu container (nullptr when closed)
    lv_obj_t*       _calcMenuRows[CALC_MENU_ITEMS]; // Menu option labels
    int             _calcMenuIdx;       // Currently highlighted menu item
    bool            _calcMenuOpen;      // Menu is visible
    // Per-item availability for the CURRENT traced curve kind. Root/Min/Max/
    // Integral are single-valued y=f(x) concepts (enabled only for ExplicitX);
    // Intersection needs a 2nd traceable curve; Tangent works for every kind.
    // Disabled items render greyed and are skipped by the menu cursor — so the
    // menu never offers a calculation that would fabricate a wrong result.
    bool            _calcMenuEnabled[CALC_MENU_ITEMS];

    // ── Integral area shading (pixel-buffer based) ───────────────────
    bool            _shadingActive;     // Shading is currently applied to buffer
    int             _shadingFuncIdx;    // Which function is shaded
    float           _shadingX0;         // Shading lower bound (world): x-range, or y-range when _shadingAlongY
    float           _shadingX1;         // Shading upper bound (world)
    bool            _shadingAlongY;     // true ⇒ x=f(y) integral: horizontal strips over a y-range

    // ── Tangent overlay (pixel-buffer based) ─────────────────────────
    bool            _tangentActive;     // Tangent is currently drawn in buffer
    int             _tangentFuncIdx;    // Which function the tangent belongs to
    float           _tangentX;          // Point of tangency (world x)
    float           _tangentY;          // Point of tangency (world y) — implicit/x=f(y)

    // ── Table panel widgets ──────────────────────────────────────────
    static constexpr int TBL_ROWS = 21; // data rows in table
    static constexpr int TBL_COLS = 1 + MAX_FUNCS;  // x + one column per function
    lv_obj_t*       _tblTable;          // native lv_table widget
    lv_obj_t*       _tblHeaderBar;      // Sticky header bar (above scrollable table)
    lv_obj_t*       _tblHdrLabels[TBL_COLS];     // Header labels: "x", "f1(x)", "f2(x)", …
    lv_obj_t*       _tblHdrSeps[TBL_COLS - 1];   // Vertical separators between header columns

    // ── Points of interest (roots, extrema, intersections) ───────────
    POI  _pois[MAX_POIS];
    int  _numPOIs;
    bool _snappedToPOI;     // cursor is currently at a snapped POI
    int  _snappedPOIIdx;    // index of snapped POI in _pois[]
    int  _snapEscapeCount;  // >0: ignore snap for this many more moves

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
    float           _traceX;            // trace cursor X (authoritative with _traceY)
    float           _traceY;            // trace cursor Y (authoritative with _traceX)
    int             _traceFn;           // which function to trace
    bool            _traceValid;        // current trace point is finite & on-curve
    bool            _traceSeeded;       // initial point computed for current fn/mode
    float           _traceHeadX;        // implicit: unit tangent, RIGHT direction (x)
    float           _traceHeadY;        // implicit: unit tangent, RIGHT direction (y)
    bool            _plotDirty;

    // Table state
    int             _tblRow;            // highlighted row
    float           _tblStart;          // first x value
    float           _tblStep;           // x step
    int             _tblFuncIdx;        // which function

    // Eval engine (MVC: Model layer)
    grapher::GraphModel  _model;        // Mathematical evaluation engine
    grapher::GraphView   _view;         // Pixel rendering engine (Kandinsky)
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
    void drawTraceCursor();
    void updateInfoBar();
    void autoFit();
    // Enforce equal aspect (square grid): equalize world-units-per-pixel on x and
    // y so circles render circular. Idempotent; applied at the top of replot().
    void normalizeAspect();

    // Find a traceable curve (valid, visible, relation==Equal — explicit y=f(x),
    // explicit x=f(y), or implicit equation), scanning from `from` in direction
    // `dir` (+1/-1). Returns -1 if none. Inequalities are regions, never traceable.
    int  firstTraceableFunc(int from, int dir) const;

    // Find the first single-valued y=f(x) slot (the only kind auto-traced on Graph
    // tab entry). Implicit/x=f(y) slots are reachable via ENTER and UP/DOWN but do
    // not auto-start trace, so an implicit-only graph still opens in Pan.
    int  firstExplicitXFunc(int from, int dir) const;

    // ── Unified trace cursor (explicit-x / explicit-y / implicit contour) ──────
    TraceKind traceKind(int fi) const;     // classify slot fi for the tracer
    void ensureTraceSeed();                // compute initial on-curve point (lazy)
    bool seedImplicitTrace();              // scan viewport for a G=0 seed + heading
    void traceStep(int dir);               // LEFT(-1)/RIGHT(+1): walk the curve
    bool gradImplicit(int fi, float x, float y, float& gx, float& gy) const; // ∇G
    bool correctOntoCurve(int fi, float& x, float& y) const;  // Newton onto G=0
    void updateImplicitHeading(int fi, float x, float y);     // refresh tangent

    // POI (snap-to-point) helpers
    void preCacheFuncRPN(int idx);
    void computePOIs(int funcIdx);
    void snapToPOI();
    void syncViewportToCursor();  // Camera follow: re-center viewport on trace cursor

    // ── Cross-kind intersection finder (purple POI markers for any curve) ─────
    // residualAt unifies every traceable slot to a single G(x,y)=0 residual:
    //   y=f(x) → y−f(x);  x=f(y) and general implicit → evalImplicit (lhs−rhs).
    // A pair of curves intersects where BOTH residuals vanish; a coarse grid scan
    // seeds a 2×2 Newton (newton2D) that refines the crossing. Pairs that are both
    // y=f(x) keep using the cheaper exact 1-D bisection in appendPOIsForFunction.
    float residualAt(int fi, float x, float y);
    void  appendCrossIntersections(int fi);   // intersections of fi with each j<fi
    void  find2DIntersections(int a, int b);   // grid-seeded 2-D root pairs
    bool  newton2D(int a, int b, float& x, float& y);  // refine onto Ga=Gb=0

    // Async POI computation (sliced over LVGL timer ticks — keeps UI at 60 FPS)
    void startAsyncPOI(int fi);
    static void poiAsyncTimerCb(lv_timer_t* t);

    // ── Tangent overlay ───────────────────────────────────────────────
    void drawTangentOverlay(int funcIdx, float xTarget);          // y=f(x): dy/dx line
    void drawImplicitTangent(int funcIdx, float x0, float y0);    // implicit/x=f(y): ⟂∇G line
    void drawActiveTangent();                                     // redraw by curve kind
    void clearTangent();

    // ── Calculate menu helpers ───────────────────────────────────────
    void openCalcMenu();
    void closeCalcMenu();
    void handleCalcMenu(const KeyEvent& ev);
    void executeCalcOption(int option);
    bool calcItemEnabled(TraceKind k, int item) const;  // is menu option valid for kind k?
    void restyleCalcMenu();                             // apply selected/disabled styling

    // ── Integral shading ─────────────────────────────────────────────
    void drawIntegralShading(int funcIdx, float shadeXMin, float shadeXMax);   // ∫f(x)dx: vertical strips
    void drawIntegralShadingY(int funcIdx, float shadeYMin, float shadeYMax);  // ∫f(y)dy: horizontal strips
    void clearIntegralShading();

    // ── Async POI internal state ──────────────────────────────────────
    lv_timer_t* _poiAsyncTimer;
    int         _poiAsyncFi;

    // ── Table helpers ────────────────────────────────────────────────
    void rebuildTable();

    // ── Math ─────────────────────────────────────────────────────────
    float evalAt(int funcIdx, float x);
    void appendPOIsForFunction(int funcIdx);

    // ── Adaptive sampling (streaming directly to GraphView buffer) ────
    void sampleFuncAdaptive(int fi, uint32_t color);
    void adaptSegStream(int fi,
                        float wx0, float wy0,
                        float wx1, float wy1,
                        int depth, uint32_t color);

    // ── Implicit-equation plotting (marching squares over G(x,y)=0) ───
    void plotImplicit(int fi, uint32_t color);

    // ── Key dispatchers ──────────────────────────────────────────────
    void handleTabBar(const KeyEvent& ev);
    void handleExprList(const KeyEvent& ev);
    void handleExprEdit(const KeyEvent& ev);
    void handleToolbar(const KeyEvent& ev);
    void handleGraphNav(const KeyEvent& ev);
    void handleGraphTrace(const KeyEvent& ev);
    void handleTable(const KeyEvent& ev);
};
