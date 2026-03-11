/**
 * NeoLanguageApp.h — NeoLanguage IDE app for NumOS.
 *
 * Two-tab IDE:
 *   · EDITOR  — monospace code textarea (keyboard input)
 *   · CONSOLE — read-only output / error display
 *
 * F5 = compile & show AST summary in the console.
 * MODE = exit app (caller SystemApp handles return-to-menu).
 * Tab key (F1) = insert 4 spaces.
 *
 * Follows the same LVGL/lifecycle patterns as PythonApp.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
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
#include "../math/cas/SymExprArena.h"

// ════════════════════════════════════════════════════════════════════
// NeoLanguageApp
// ════════════════════════════════════════════════════════════════════

class NeoLanguageApp {
public:
    NeoLanguageApp();
    ~NeoLanguageApp();

    // ── Standard NumOS app lifecycle ─────────────────────────────
    void begin();           ///< Create LVGL screen & UI
    void end();             ///< Destroy LVGL screen & reset state
    void load();            ///< Animate screen into view
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Layout constants ─────────────────────────────────────────
    static constexpr int SCREEN_W         = 320;
    static constexpr int SCREEN_H         = 240;
    static constexpr int BAR_H            = 25;   ///< StatusBar height
    static constexpr int TAB_H            = 28;   ///< Tab bar height
    static constexpr int CONTENT_Y        = BAR_H + TAB_H;
    static constexpr int CONTENT_H        = SCREEN_H - CONTENT_Y;
    static constexpr int MAX_RESULT_CHARS = 200;  ///< Per-result output truncation limit

    // ── Colour palette ────────────────────────────────────────────
    static constexpr uint32_t COL_BG        = 0x1E1E2E; ///< Dark background
    static constexpr uint32_t COL_TAB_BG    = 0x313244; ///< Tab bar bg
    static constexpr uint32_t COL_TAB_ACT   = 0x89B4FA; ///< Active tab pill
    static constexpr uint32_t COL_TAB_TXT_A = 0x1E1E2E; ///< Active tab text
    static constexpr uint32_t COL_TAB_TXT_I = 0xCDD6F4; ///< Inactive tab text
    static constexpr uint32_t COL_EDITOR_BG = 0x1E1E2E;
    static constexpr uint32_t COL_EDITOR_TXT= 0xCDD6F4;
    static constexpr uint32_t COL_CONSOLE_BG= 0x11111B;
    static constexpr uint32_t COL_CONSOLE_TXT=0xA6E3A1; ///< Green terminal text

    // ── Enum: active tab ─────────────────────────────────────────
    enum class Tab : uint8_t { EDITOR = 0, CONSOLE = 1 };

    // ── Enum: focus region ───────────────────────────────────────
    enum class Focus : uint8_t { TAB_BAR, CONTENT };

    // ── LVGL objects ─────────────────────────────────────────────
    lv_obj_t*      _screen;

    // Tab bar (2 pills)
    lv_obj_t*      _tabBar;
    lv_obj_t*      _tabPills[2];
    lv_obj_t*      _tabLabels[2];

    // Editor panel
    lv_obj_t*      _panelEditor;
    lv_obj_t*      _editor;         ///< lv_textarea for code input

    // Console panel
    lv_obj_t*      _panelConsole;
    lv_obj_t*      _console;        ///< lv_textarea for output (read-only)

    // Status bar (title + battery)
    ui::StatusBar  _statusBar;

    // ── Application state ────────────────────────────────────────
    Tab            _activeTab;
    Focus          _focus;
    int            _tabIdx;         ///< Tab-bar cursor (0=EDITOR, 1=CONSOLE)

    // ── Compiler components ───────────────────────────────────────
    NeoArena         _arena;      ///< AST arena (reset on each run)
    cas::SymExprArena _symArena;  ///< SymExpr arena for symbolic evaluation

    // ── UI helpers ────────────────────────────────────────────────
    void createUI();
    void createTabBar();
    void createEditorPanel();
    void createConsolePanel();

    // ── Tab management ────────────────────────────────────────────
    void switchTab(Tab t);
    void refreshTabBar();

    // ── Key dispatch ─────────────────────────────────────────────
    void handleTabBarKey(const KeyEvent& ev);
    void handleEditorKey(const KeyEvent& ev);
    void handleConsoleKey(const KeyEvent& ev);

    // ── Console helpers ───────────────────────────────────────────
    void appendConsole(const char* text);
    void clearConsole();

    // ── Compiler integration ──────────────────────────────────────
    /**
     * Tokenize + parse the editor content, then print a summary
     * (token count, node count, errors) to the console textarea.
     */
    void runCode();

    /**
     * Recursively walk the AST and append a human-readable summary
     * to `out`.  Depth-limited to avoid stack overflow.
     */
    void dumpNode(NeoNode* node, std::string& out, int depth);

    // ── Character insertion ───────────────────────────────────────
    void insertChar(char c);
};
