/**
 * NeoLanguageApp.h — NeoLanguage IDE app for NumOS.
 *
 * Two-tab IDE:
 *   · EDITOR  — monospace code textarea (keyboard input)
 *   · CONSOLE — read-only output / error display
 *
 * Key bindings:
 *   F5   = run the current program
 *   F1   = insert 4 spaces (tab-indent)
 *   F2   = save editor content to flash (/neolang.nl)
 *   F3   = load editor content from flash (/neolang.nl)
 *   MODE = exit app (caller SystemApp handles return-to-menu)
 *
 * Phase 3 additions:
 *   · NeoStdLib integration (diff, integrate, solve, plot, etc.)
 *   · plot() triggers Graphics Mode overlay using SymExpr::evaluate
 *     directly, decoupled from GrapherApp's string evaluator
 *   · print() appends directly to the console textarea
 *   · Persistence: editor content saved/loaded via LittleFS (/neolang.nl)
 *   · Memory protection: SymExprArena soft-reset if >70% used
 *
 * Follows the same LVGL/lifecycle patterns as PythonApp.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 3 (Standard Library)
 */
#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "NeoAST.h"
#include "NeoLexer.h"
#include "NeoParser.h"
#include "NeoValue.h"
#include "NeoEnv.h"
#include "NeoInterpreter.h"
#include "NeoStdLib.h"
#include "../math/cas/SymExprArena.h"

// ════════════════════════════════════════════════════════════════════
// NeoLanguageApp
// ════════════════════════════════════════════════════════════════════

class NeoLanguageApp {
public:
    NeoLanguageApp();
    ~NeoLanguageApp();

    // Standard NumOS app lifecycle
    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // Layout constants
    static constexpr int SCREEN_W         = 320;
    static constexpr int SCREEN_H         = 240;
    static constexpr int BAR_H            = 25;
    static constexpr int TAB_H            = 28;
    static constexpr int CONTENT_Y        = BAR_H + TAB_H;
    static constexpr int CONTENT_H        = SCREEN_H - CONTENT_Y;
    static constexpr int MAX_RESULT_CHARS = 200;
    static constexpr int PLOT_W           = SCREEN_W;
    static constexpr int PLOT_H           = SCREEN_H - BAR_H - 20;
    static constexpr int PLOT_HINT_H      = 20;
    static constexpr int PLOT_SAMPLE_PTS  = 160; ///< Sampling points for y-range auto-detection
    static constexpr float ARENA_RESET_THRESHOLD = 0.70f;

    // Colour palette
    static constexpr uint32_t COL_BG         = 0x1E1E2E;
    static constexpr uint32_t COL_TAB_BG     = 0x313244;
    static constexpr uint32_t COL_TAB_ACT    = 0x89B4FA;
    static constexpr uint32_t COL_TAB_TXT_A  = 0x1E1E2E;
    static constexpr uint32_t COL_TAB_TXT_I  = 0xCDD6F4;
    static constexpr uint32_t COL_EDITOR_BG  = 0x1E1E2E;
    static constexpr uint32_t COL_EDITOR_TXT = 0xCDD6F4;
    static constexpr uint32_t COL_CONSOLE_BG = 0x11111B;
    static constexpr uint32_t COL_CONSOLE_TXT= 0xA6E3A1;
    static constexpr uint32_t COL_PLOT_BG    = 0x11111B;
    static constexpr uint32_t COL_PLOT_AXIS  = 0x585B70;
    static constexpr uint32_t COL_PLOT_LINE  = 0x89B4FA;

    enum class Tab   : uint8_t { EDITOR = 0, CONSOLE = 1 };
    enum class Focus : uint8_t { TAB_BAR, CONTENT };

    // LVGL objects
    lv_obj_t*      _screen;
    lv_obj_t*      _tabBar;
    lv_obj_t*      _tabPills[2];
    lv_obj_t*      _tabLabels[2];
    lv_obj_t*      _panelEditor;
    lv_obj_t*      _editor;
    lv_obj_t*      _panelConsole;
    lv_obj_t*      _console;
    ui::StatusBar  _statusBar;

    // Graphics Mode overlay
    lv_obj_t*      _plotOverlay;
    lv_obj_t*      _plotCanvas;
    lv_obj_t*      _plotHintLabel;
    lv_draw_buf_t  _plotDrawBuf;
    uint8_t*       _plotBuf;
    bool           _plotMode;

    // Application state
    Tab            _activeTab;
    Focus          _focus;
    int            _tabIdx;

    // Compiler components
    NeoArena          _arena;
    cas::SymExprArena _symArena;

    // UI helpers
    void createUI();
    void createTabBar();
    void createEditorPanel();
    void createConsolePanel();

    // Tab management
    void switchTab(Tab t);
    void refreshTabBar();

    // Key dispatch
    void handleTabBarKey(const KeyEvent& ev);
    void handleEditorKey(const KeyEvent& ev);
    void handleConsoleKey(const KeyEvent& ev);

    // Console helpers
    void appendConsole(const char* text);
    void clearConsole();

    // Graphics Mode
    void showPlot(const NeoPlotRequest& req);
    void hidePlot();
    void renderPlot(cas::SymExpr* expr, double xMin, double xMax);

    // Persistence
    void saveToFlash();
    void loadFromFlash();

    // Host callback builder
    NeoHostCallbacks buildHostCallbacks();

    // Compiler integration
    void runCode();
    void dumpNode(NeoNode* node, std::string& out, int depth);
    void insertChar(char c);
};
