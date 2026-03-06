/**
 * PythonApp.h — MicroPython IDE (LVGL 9 Native)
 *
 * NumWorks-inspired 3-tab Python IDE:
 *   · Scripts  — LittleFS file manager (list, new, run, delete)
 *   · Editor   — Monospace code textarea with autocomplete
 *   · Console  — Read-only output terminal (green on black)
 *
 * Uses PythonEngine for simulated Python execution.
 */
#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "PythonEngine.h"

class PythonApp {
public:
    PythonApp();
    ~PythonApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Layout constants ─────────────────────────────────────────
    static constexpr int SCREEN_W     = 320;
    static constexpr int SCREEN_H     = 240;
    static constexpr int BAR_H        = 25;
    static constexpr int TAB_H        = 28;
    static constexpr int BTN_BAR_H    = 28;
    static constexpr int CONTENT_Y    = BAR_H + TAB_H;
    static constexpr int CONTENT_H    = SCREEN_H - CONTENT_Y;
    static constexpr int MAX_SCRIPTS  = 16;
    static constexpr int MAX_FILENAME = 32;
    static constexpr int MAX_CODE_LEN = 2048;

    // Autocomplete keywords
    static constexpr int NUM_KEYWORDS = 14;
    static const char* KEYWORDS[NUM_KEYWORDS];

    // ── Enums ────────────────────────────────────────────────────
    enum class Tab   : uint8_t { SCRIPTS, EDITOR, CONSOLE };
    enum class Focus : uint8_t { TAB_BAR, CONTENT };

    // ── UI Objects ───────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // Tab bar
    lv_obj_t*       _tabBar;
    lv_obj_t*       _tabPills[3];
    lv_obj_t*       _tabLabels[3];

    // Scripts panel
    lv_obj_t*       _panelScripts;
    lv_obj_t*       _scriptList;
    lv_obj_t*       _scriptRows[MAX_SCRIPTS];
    lv_obj_t*       _scriptLabels[MAX_SCRIPTS];
    int             _scriptCount;
    int             _scriptIdx;
    char            _scriptNames[MAX_SCRIPTS][MAX_FILENAME];

    // Bottom button bar for scripts
    lv_obj_t*       _btnBarScripts;
    lv_obj_t*       _btnNew;
    lv_obj_t*       _btnRun;
    lv_obj_t*       _btnDelete;
    int             _btnIdx;        // focused button (0=New, 1=Run, 2=Delete)
    bool            _btnFocus;      // true = focus is on button bar

    // Editor panel
    lv_obj_t*       _panelEditor;
    lv_obj_t*       _editorTA;      // lv_textarea for code editing
    char            _editFilename[MAX_FILENAME];
    char            _editCode[MAX_CODE_LEN];

    // Autocomplete popup
    lv_obj_t*       _acPopup;
    lv_obj_t*       _acRows[8];
    lv_obj_t*       _acLabels[8];
    int             _acCount;
    int             _acIdx;
    bool            _acOpen;
    char            _acPrefix[32];

    // Console panel
    lv_obj_t*       _panelConsole;
    lv_obj_t*       _consoleTA;     // lv_textarea read-only

    // ── State ────────────────────────────────────────────────────
    Tab             _tab;
    Focus           _focus;
    int             _tabIdx;
    PythonEngine    _engine;

    // ── UI creation ──────────────────────────────────────────────
    void createUI();
    void createTabBar();
    void createScriptsPanel();
    void createEditorPanel();
    void createConsolePanel();

    // ── Tab switching ────────────────────────────────────────────
    void switchTab(Tab t);
    void refreshTabBar();

    // ── Scripts helpers ──────────────────────────────────────────
    void refreshScriptList();
    void refreshScriptFocus();
    void loadScriptsList();
    void createNewScript();
    void deleteScript(int idx);
    void openScriptInEditor(int idx);
    void runScript(int idx);
    void refreshBtnBar();

    // ── Editor helpers ───────────────────────────────────────────
    void loadIntoEditor(const char* filename);
    void saveCurrentScript();
    void insertChar(char c);
    void handleEditorKey(const KeyEvent& ev);
    void checkAutocomplete();
    void applyAutocomplete();
    void closeAutocomplete();

    // ── Console helpers ──────────────────────────────────────────
    void appendConsole(const char* text);
    void clearConsole();

    // ── File system ──────────────────────────────────────────────
    bool saveScript(const char* filename, const char* content);
    bool loadScript(const char* filename, char* buffer, int bufSize);
    void ensureDefaultScripts();

    // ── Key dispatchers ──────────────────────────────────────────
    void handleTabBarKey(const KeyEvent& ev);
    void handleScriptsKey(const KeyEvent& ev);
    void handleConsoleKey(const KeyEvent& ev);
};
