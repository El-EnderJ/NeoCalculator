/**
 * PythonApp.cpp — MicroPython IDE (100% LVGL 9)
 *
 * Three-tab Python IDE: Scripts, Editor, Console.
 * Uses PythonEngine for simulated execution, LittleFS for persistence.
 */

#include "PythonApp.h"
#include <cstring>
#include <cstdio>
#include <cctype>

#ifdef ARDUINO
#include <FS.h>
#include <LittleFS.h>
#endif

// ── Layout colours ──────────────────────────────────────────────
static constexpr uint32_t COL_BG        = 0xF5F5F5;
static constexpr uint32_t COL_TAB_BG    = 0xF57F17;  // Amber (Python)
static constexpr uint32_t COL_TAB_ACT   = 0xFFFFFF;
static constexpr uint32_t COL_TAB_TXT_I = 0xFFF3D6;
static constexpr uint32_t COL_TAB_TXT_A = 0x000000;
static constexpr uint32_t COL_ROW_BG    = 0xFFFFFF;
static constexpr uint32_t COL_ROW_SEL   = 0x4A90E2;
static constexpr uint32_t COL_BTN_BG    = 0x4A90E2;
static constexpr uint32_t COL_BTN_TXT   = 0xFFFFFF;
static constexpr uint32_t COL_EDITOR_BG = 0x1E1E1E;
static constexpr uint32_t COL_EDITOR_TXT= 0xD4D4D4;
static constexpr uint32_t COL_CONSOLE_BG= 0x000000;
static constexpr uint32_t COL_CONSOLE_TXT=0x00FF00;
static constexpr uint32_t COL_AC_BG     = 0x2D2D2D;
static constexpr uint32_t COL_AC_SEL    = 0x264F78;

// Autocomplete keywords
const char* PythonApp::KEYWORDS[NUM_KEYWORDS] = {
    "import", "from", "def", "class", "print",
    "for", "while", "if", "elif", "else",
    "return", "range", "math", "True"
};

// ── Helper to create a container ─────────────────────────────────
static lv_obj_t* makePanel(lv_obj_t* parent, int x, int y, int w, int h,
                            uint32_t bg = COL_BG, bool scroll = false) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    if (!scroll) lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y,
                            const char* text, uint32_t color,
                            const lv_font_t* font = &lv_font_montserrat_12) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    return lbl;
}

// ═════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═════════════════════════════════════════════════════════════════

PythonApp::PythonApp()
    : _screen(nullptr)
    , _tabBar(nullptr)
    , _panelScripts(nullptr), _scriptList(nullptr)
    , _scriptCount(0), _scriptIdx(0)
    , _btnBarScripts(nullptr), _btnNew(nullptr), _btnRun(nullptr), _btnDelete(nullptr)
    , _btnIdx(0), _btnFocus(false)
    , _panelEditor(nullptr), _editorTA(nullptr)
    , _acPopup(nullptr), _acCount(0), _acIdx(0), _acOpen(false)
    , _panelConsole(nullptr), _consoleTA(nullptr)
    , _tab(Tab::SCRIPTS), _focus(Focus::TAB_BAR), _tabIdx(0)
{
    _editFilename[0] = '\0';
    _editCode[0] = '\0';
    _acPrefix[0] = '\0';
    for (int i = 0; i < 3; ++i) { _tabPills[i] = nullptr; _tabLabels[i] = nullptr; }
    for (int i = 0; i < MAX_SCRIPTS; ++i) {
        _scriptRows[i] = nullptr;
        _scriptLabels[i] = nullptr;
        _scriptNames[i][0] = '\0';
    }
    for (int i = 0; i < 8; ++i) { _acRows[i] = nullptr; _acLabels[i] = nullptr; }
}

PythonApp::~PythonApp() { end(); }

// ═════════════════════════════════════════════════════════════════
// Lifecycle
// ═════════════════════════════════════════════════════════════════

void PythonApp::begin() {
    if (_screen) return;
    _engine.init();
    ensureDefaultScripts();
    createUI();
    _tab = Tab::SCRIPTS;
    _focus = Focus::TAB_BAR;
    _tabIdx = 0;
    switchTab(_tab);
}

void PythonApp::end() {
    _engine.deinit();
    _statusBar.destroy();
    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
        _tabBar = nullptr;
        for (int i = 0; i < 3; ++i) { _tabPills[i] = nullptr; _tabLabels[i] = nullptr; }
        _panelScripts = nullptr; _scriptList = nullptr;
        for (int i = 0; i < MAX_SCRIPTS; ++i) {
            _scriptRows[i] = nullptr; _scriptLabels[i] = nullptr;
        }
        _btnBarScripts = nullptr; _btnNew = nullptr; _btnRun = nullptr; _btnDelete = nullptr;
        _panelEditor = nullptr; _editorTA = nullptr;
        _acPopup = nullptr; _acOpen = false;
        for (int i = 0; i < 8; ++i) { _acRows[i] = nullptr; _acLabels[i] = nullptr; }
        _panelConsole = nullptr; _consoleTA = nullptr;
    }
    _tab = Tab::SCRIPTS;
    _focus = Focus::TAB_BAR;
    _scriptCount = 0;
    _scriptIdx = 0;
}

void PythonApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();
}

// ═════════════════════════════════════════════════════════════════
// UI Creation
// ═════════════════════════════════════════════════════════════════

void PythonApp::createUI() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Python");
    _statusBar.setBatteryLevel(100);

    createTabBar();
    createScriptsPanel();
    createEditorPanel();
    createConsolePanel();
}

void PythonApp::createTabBar() {
    int y = BAR_H;
    _tabBar = makePanel(_screen, 0, y, SCREEN_W, TAB_H, COL_TAB_BG);

    const char* titles[] = { "Scripts", "Editor", "Console" };
    int tw = SCREEN_W / 3;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t* pill = lv_obj_create(_tabBar);
        lv_obj_set_size(pill, tw - 4, TAB_H - 6);
        lv_obj_set_pos(pill, i * tw + 2, 3);
        lv_obj_set_style_bg_color(pill, lv_color_hex(COL_TAB_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(pill, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(pill, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(pill, 0, LV_PART_MAIN);
        lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
        _tabPills[i] = pill;

        _tabLabels[i] = makeLabel(pill, (tw - 4) / 2 - 24, (TAB_H - 6 - 12) / 2,
                                   titles[i], COL_TAB_TXT_I, &lv_font_montserrat_12);
        lv_obj_set_style_text_align(_tabLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

void PythonApp::createScriptsPanel() {
    int contentH = CONTENT_H - BTN_BAR_H;
    _panelScripts = makePanel(_screen, 0, CONTENT_Y, SCREEN_W, contentH, COL_BG, true);
    lv_obj_set_scroll_dir(_panelScripts, LV_DIR_VER);

    // Script rows will be created dynamically in refreshScriptList()
    loadScriptsList();
    refreshScriptList();

    // Bottom button bar
    _btnBarScripts = makePanel(_screen, 0, SCREEN_H - BTN_BAR_H, SCREEN_W, BTN_BAR_H, 0xEEEEEE);

    int bw = (SCREEN_W - 4 * 6) / 3;  // 3 buttons with padding
    _btnNew = makePanel(_btnBarScripts, 6, 3, bw, BTN_BAR_H - 6, COL_BTN_BG);
    lv_obj_set_style_radius(_btnNew, 4, LV_PART_MAIN);
    makeLabel(_btnNew, bw / 2 - 12, (BTN_BAR_H - 6 - 12) / 2, "New", COL_BTN_TXT, &lv_font_montserrat_12);

    _btnRun = makePanel(_btnBarScripts, 6 + bw + 6, 3, bw, BTN_BAR_H - 6, COL_BTN_BG);
    lv_obj_set_style_radius(_btnRun, 4, LV_PART_MAIN);
    makeLabel(_btnRun, bw / 2 - 12, (BTN_BAR_H - 6 - 12) / 2, "Run", COL_BTN_TXT, &lv_font_montserrat_12);

    _btnDelete = makePanel(_btnBarScripts, 6 + (bw + 6) * 2, 3, bw, BTN_BAR_H - 6, 0xCC3333);
    lv_obj_set_style_radius(_btnDelete, 4, LV_PART_MAIN);
    makeLabel(_btnDelete, bw / 2 - 18, (BTN_BAR_H - 6 - 12) / 2, "Delete", COL_BTN_TXT, &lv_font_montserrat_12);

    _btnIdx = 0;
    _btnFocus = false;
    refreshBtnBar();
}

void PythonApp::createEditorPanel() {
    _panelEditor = makePanel(_screen, 0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_EDITOR_BG);

    _editorTA = lv_textarea_create(_panelEditor);
    lv_obj_set_pos(_editorTA, 0, 0);
    lv_obj_set_size(_editorTA, SCREEN_W, CONTENT_H);
    lv_textarea_set_placeholder_text(_editorTA, "# Write Python code here...");
    lv_obj_set_style_bg_color(_editorTA, lv_color_hex(COL_EDITOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(_editorTA, lv_color_hex(COL_EDITOR_TXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(_editorTA, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_border_width(_editorTA, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_editorTA, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_editorTA, 4, LV_PART_MAIN);
    // Cursor style
    lv_obj_set_style_bg_color(_editorTA, lv_color_hex(0xFFFFFF), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(_editorTA, LV_OPA_COVER, LV_PART_CURSOR);

    // Autocomplete popup (hidden by default)
    _acPopup = makePanel(_screen, 10, CONTENT_Y + 20, 120, 0, COL_AC_BG);
    lv_obj_set_style_radius(_acPopup, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(_acPopup, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_acPopup, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_add_flag(_acPopup, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 8; ++i) {
        _acRows[i] = makePanel(_acPopup, 0, i * 16, 120, 16, COL_AC_BG);
        _acLabels[i] = makeLabel(_acRows[i], 4, 2, "", 0xCCCCCC, &lv_font_unscii_8);
        lv_obj_add_flag(_acRows[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void PythonApp::createConsolePanel() {
    _panelConsole = makePanel(_screen, 0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_CONSOLE_BG);

    _consoleTA = lv_textarea_create(_panelConsole);
    lv_obj_set_pos(_consoleTA, 0, 0);
    lv_obj_set_size(_consoleTA, SCREEN_W, CONTENT_H);
    lv_textarea_set_text(_consoleTA, ">>> Python Console\n");
    lv_obj_set_style_bg_color(_consoleTA, lv_color_hex(COL_CONSOLE_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(_consoleTA, lv_color_hex(COL_CONSOLE_TXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(_consoleTA, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_border_width(_consoleTA, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_consoleTA, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_consoleTA, 4, LV_PART_MAIN);
    lv_textarea_set_cursor_click_pos(_consoleTA, false);
    // Make read-only: no text entry via LVGL
    lv_obj_remove_flag(_consoleTA, LV_OBJ_FLAG_CLICKABLE);
}

// ═════════════════════════════════════════════════════════════════
// Tab switching
// ═════════════════════════════════════════════════════════════════

void PythonApp::switchTab(Tab t) {
    _tab = t;
    // Hide all panels
    if (_panelScripts)   lv_obj_add_flag(_panelScripts, LV_OBJ_FLAG_HIDDEN);
    if (_btnBarScripts)  lv_obj_add_flag(_btnBarScripts, LV_OBJ_FLAG_HIDDEN);
    if (_panelEditor)    lv_obj_add_flag(_panelEditor, LV_OBJ_FLAG_HIDDEN);
    if (_acPopup)        lv_obj_add_flag(_acPopup, LV_OBJ_FLAG_HIDDEN);
    if (_panelConsole)   lv_obj_add_flag(_panelConsole, LV_OBJ_FLAG_HIDDEN);

    switch (t) {
        case Tab::SCRIPTS:
            lv_obj_remove_flag(_panelScripts, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(_btnBarScripts, LV_OBJ_FLAG_HIDDEN);
            refreshScriptList();
            break;
        case Tab::EDITOR:
            lv_obj_remove_flag(_panelEditor, LV_OBJ_FLAG_HIDDEN);
            break;
        case Tab::CONSOLE:
            lv_obj_remove_flag(_panelConsole, LV_OBJ_FLAG_HIDDEN);
            break;
    }
    refreshTabBar();
}

void PythonApp::refreshTabBar() {
    for (int i = 0; i < 3; ++i) {
        bool active = (i == (int)_tab);
        bool focused = (_focus == Focus::TAB_BAR && _tabIdx == i);

        if (active) {
            lv_obj_set_style_bg_color(_tabPills[i], lv_color_hex(COL_TAB_ACT), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tabPills[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(_tabLabels[i], lv_color_hex(COL_TAB_TXT_A), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(_tabPills[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(_tabLabels[i], lv_color_hex(COL_TAB_TXT_I), LV_PART_MAIN);
        }

        if (focused) {
            lv_obj_set_style_border_width(_tabPills[i], 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(_tabPills[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(_tabPills[i], 0, LV_PART_MAIN);
        }
    }
}

// ═════════════════════════════════════════════════════════════════
// Scripts panel helpers
// ═════════════════════════════════════════════════════════════════

void PythonApp::loadScriptsList() {
    _scriptCount = 0;

#ifdef ARDUINO
    File dir = LittleFS.open("/py");
    if (!dir || !dir.isDirectory()) {
        // Directory doesn't exist yet
        return;
    }
    File f = dir.openNextFile();
    while (f && _scriptCount < MAX_SCRIPTS) {
        const char* name = f.name();
        if (name) {
            // Store just the filename (without path)
            const char* slash = strrchr(name, '/');
            const char* base = slash ? slash + 1 : name;
            strncpy(_scriptNames[_scriptCount], base, MAX_FILENAME - 1);
            _scriptNames[_scriptCount][MAX_FILENAME - 1] = '\0';
            _scriptCount++;
        }
        f = dir.openNextFile();
    }
#else
    // Desktop simulation: add a fake entry
    strcpy(_scriptNames[0], "hello.py");
    _scriptCount = 1;
#endif
}

void PythonApp::refreshScriptList() {
    if (!_panelScripts) return;

    // Remove old rows
    for (int i = 0; i < MAX_SCRIPTS; ++i) {
        if (_scriptRows[i]) {
            lv_obj_delete(_scriptRows[i]);
            _scriptRows[i] = nullptr;
            _scriptLabels[i] = nullptr;
        }
    }

    // Create rows for each script
    static constexpr int ROW_H = 28;
    static constexpr int ROW_GAP = 2;
    static constexpr int PAD = 6;

    for (int i = 0; i < _scriptCount; ++i) {
        int ry = PAD + i * (ROW_H + ROW_GAP);
        _scriptRows[i] = makePanel(_panelScripts, PAD, ry, SCREEN_W - 2 * PAD, ROW_H, COL_ROW_BG);
        lv_obj_set_style_radius(_scriptRows[i], 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(_scriptRows[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_scriptRows[i], lv_color_hex(0xD1D1D1), LV_PART_MAIN);

        // Python file icon (emoji-like "py" tag)
        makeLabel(_scriptRows[i], 6, (ROW_H - 12) / 2, "py", 0xF57F17, &lv_font_montserrat_12);

        _scriptLabels[i] = makeLabel(_scriptRows[i], 28, (ROW_H - 12) / 2,
                                      _scriptNames[i], 0x333333, &lv_font_montserrat_12);
    }

    if (_scriptCount == 0) {
        makeLabel(_panelScripts, SCREEN_W / 2 - 50, CONTENT_H / 2 - 20,
                  "No scripts found", 0x999999, &lv_font_montserrat_12);
    }

    refreshScriptFocus();
}

void PythonApp::refreshScriptFocus() {
    for (int i = 0; i < _scriptCount; ++i) {
        if (!_scriptRows[i]) continue;
        bool sel = (!_btnFocus && i == _scriptIdx && _focus == Focus::CONTENT);
        lv_obj_set_style_bg_color(_scriptRows[i],
            lv_color_hex(sel ? COL_ROW_SEL : COL_ROW_BG), LV_PART_MAIN);
        if (_scriptLabels[i]) {
            lv_obj_set_style_text_color(_scriptLabels[i],
                lv_color_hex(sel ? 0xFFFFFF : 0x333333), LV_PART_MAIN);
        }
    }
    refreshBtnBar();
}

void PythonApp::refreshBtnBar() {
    if (!_btnNew || !_btnRun || !_btnDelete) return;
    lv_obj_t* btns[] = { _btnNew, _btnRun, _btnDelete };
    uint32_t colors[] = { COL_BTN_BG, COL_BTN_BG, 0xCC3333 };

    for (int i = 0; i < 3; ++i) {
        bool sel = (_btnFocus && i == _btnIdx && _focus == Focus::CONTENT);
        if (sel) {
            lv_obj_set_style_border_width(btns[i], 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(btns[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(btns[i], 0, LV_PART_MAIN);
        }
    }
}

void PythonApp::createNewScript() {
    if (_scriptCount >= MAX_SCRIPTS) return;

    // Generate a unique name
    char name[MAX_FILENAME];
    for (int n = 1; n < 100; ++n) {
        snprintf(name, sizeof(name), "script%d.py", n);
        bool exists = false;
        for (int i = 0; i < _scriptCount; ++i) {
            if (strcmp(_scriptNames[i], name) == 0) { exists = true; break; }
        }
        if (!exists) break;
    }

    // Create empty script
    saveScript(name, "# New script\nprint('Hello!')\n");

    // Reload and open in editor
    loadScriptsList();
    refreshScriptList();

    // Find the new script and open it
    for (int i = 0; i < _scriptCount; ++i) {
        if (strcmp(_scriptNames[i], name) == 0) {
            openScriptInEditor(i);
            return;
        }
    }
}

void PythonApp::deleteScript(int idx) {
    if (idx < 0 || idx >= _scriptCount) return;

#ifdef ARDUINO
    char path[64];
    snprintf(path, sizeof(path), "/py/%s", _scriptNames[idx]);
    LittleFS.remove(path);
#endif

    loadScriptsList();
    if (_scriptIdx >= _scriptCount && _scriptCount > 0)
        _scriptIdx = _scriptCount - 1;
    refreshScriptList();
}

void PythonApp::openScriptInEditor(int idx) {
    if (idx < 0 || idx >= _scriptCount) return;
    strncpy(_editFilename, _scriptNames[idx], MAX_FILENAME - 1);
    loadIntoEditor(_editFilename);
    switchTab(Tab::EDITOR);
    _focus = Focus::CONTENT;
}

void PythonApp::runScript(int idx) {
    if (idx < 0 || idx >= _scriptCount) return;

    char code[MAX_CODE_LEN];
    if (!loadScript(_scriptNames[idx], code, MAX_CODE_LEN)) {
        clearConsole();
        appendConsole("Error: Could not load script\n");
        switchTab(Tab::CONSOLE);
        return;
    }

    clearConsole();
    appendConsole(">>> Running ");
    appendConsole(_scriptNames[idx]);
    appendConsole("...\n");

    bool ok = _engine.execute(code);
    const char* out = _engine.getOutput();
    if (out && out[0]) {
        appendConsole(out);
    }
    if (!ok) {
        const char* err = _engine.getError();
        if (err && err[0]) {
            appendConsole(err);
        }
    }
    appendConsole("\n>>> Done.\n");

    switchTab(Tab::CONSOLE);
    _focus = Focus::CONTENT;
}

// ═════════════════════════════════════════════════════════════════
// Editor helpers
// ═════════════════════════════════════════════════════════════════

void PythonApp::loadIntoEditor(const char* filename) {
    _editCode[0] = '\0';
    loadScript(filename, _editCode, MAX_CODE_LEN);
    if (_editorTA) {
        lv_textarea_set_text(_editorTA, _editCode);
    }
}

void PythonApp::saveCurrentScript() {
    if (_editFilename[0] == '\0') return;
    if (_editorTA) {
        const char* text = lv_textarea_get_text(_editorTA);
        if (text) {
            saveScript(_editFilename, text);
        }
    }
}

void PythonApp::insertChar(char c) {
    if (!_editorTA) return;
    char buf[2] = { c, '\0' };
    lv_textarea_add_text(_editorTA, buf);
}

void PythonApp::checkAutocomplete() {
    if (!_editorTA) return;

    const char* text = lv_textarea_get_text(_editorTA);
    uint32_t curPos = lv_textarea_get_cursor_pos(_editorTA);
    if (!text || curPos == 0) { closeAutocomplete(); return; }

    // Extract the current word being typed
    int wordStart = curPos - 1;
    while (wordStart >= 0 && (isalpha((unsigned char)text[wordStart]) || text[wordStart] == '_')) {
        wordStart--;
    }
    wordStart++;

    int wordLen = curPos - wordStart;
    if (wordLen < 2) { closeAutocomplete(); return; }

    char prefix[32];
    if (wordLen >= (int)sizeof(prefix)) wordLen = sizeof(prefix) - 1;
    memcpy(prefix, text + wordStart, wordLen);
    prefix[wordLen] = '\0';
    strncpy(_acPrefix, prefix, sizeof(_acPrefix) - 1);

    // Find matching keywords
    _acCount = 0;
    for (int i = 0; i < NUM_KEYWORDS && _acCount < 8; ++i) {
        if (strncmp(KEYWORDS[i], prefix, wordLen) == 0 && strcmp(KEYWORDS[i], prefix) != 0) {
            if (_acRows[_acCount] && _acLabels[_acCount]) {
                lv_label_set_text(_acLabels[_acCount], KEYWORDS[i]);
                lv_obj_remove_flag(_acRows[_acCount], LV_OBJ_FLAG_HIDDEN);
            }
            _acCount++;
        }
    }

    if (_acCount == 0) {
        closeAutocomplete();
        return;
    }

    // Hide unused rows
    for (int i = _acCount; i < 8; ++i) {
        if (_acRows[i]) lv_obj_add_flag(_acRows[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Size and show popup
    _acIdx = 0;
    int popH = _acCount * 16;
    lv_obj_set_size(_acPopup, 120, popH);
    lv_obj_remove_flag(_acPopup, LV_OBJ_FLAG_HIDDEN);
    _acOpen = true;

    // Highlight first item
    for (int i = 0; i < _acCount; ++i) {
        if (_acRows[i]) {
            lv_obj_set_style_bg_color(_acRows[i],
                lv_color_hex(i == 0 ? COL_AC_SEL : COL_AC_BG), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_acRows[i], LV_OPA_COVER, LV_PART_MAIN);
        }
    }
}

void PythonApp::applyAutocomplete() {
    if (!_acOpen || _acIdx < 0 || _acIdx >= _acCount) return;
    if (!_acLabels[_acIdx]) return;

    const char* keyword = lv_label_get_text(_acLabels[_acIdx]);
    if (!keyword) return;

    // Delete the prefix that was already typed
    int prefixLen = strlen(_acPrefix);
    for (int i = 0; i < prefixLen; ++i) {
        lv_textarea_delete_char(_editorTA);
    }

    // Insert the full keyword
    lv_textarea_add_text(_editorTA, keyword);

    closeAutocomplete();
}

void PythonApp::closeAutocomplete() {
    if (_acPopup) lv_obj_add_flag(_acPopup, LV_OBJ_FLAG_HIDDEN);
    _acOpen = false;
    _acCount = 0;
    _acIdx = 0;
}

// ═════════════════════════════════════════════════════════════════
// Console helpers
// ═════════════════════════════════════════════════════════════════

void PythonApp::appendConsole(const char* text) {
    if (!_consoleTA || !text) return;
    lv_textarea_add_text(_consoleTA, text);
}

void PythonApp::clearConsole() {
    if (!_consoleTA) return;
    lv_textarea_set_text(_consoleTA, "");
}

// ═════════════════════════════════════════════════════════════════
// File System (LittleFS)
// ═════════════════════════════════════════════════════════════════

bool PythonApp::saveScript(const char* filename, const char* content) {
#ifdef ARDUINO
    // Ensure /py directory exists
    if (!LittleFS.exists("/py")) {
        LittleFS.mkdir("/py");
    }
    char path[64];
    snprintf(path, sizeof(path), "/py/%s", filename);
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    f.print(content);
    f.close();
    return true;
#else
    (void)filename; (void)content;
    return true;
#endif
}

bool PythonApp::loadScript(const char* filename, char* buffer, int bufSize) {
#ifdef ARDUINO
    char path[64];
    snprintf(path, sizeof(path), "/py/%s", filename);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    int len = f.readBytes(buffer, bufSize - 1);
    buffer[len] = '\0';
    f.close();
    return true;
#else
    (void)filename;
    strncpy(buffer, "# hello.py\nfor i in range(5):\n    print(i)\n", bufSize - 1);
    buffer[bufSize - 1] = '\0';
    return true;
#endif
}

void PythonApp::ensureDefaultScripts() {
#ifdef ARDUINO
    if (!LittleFS.exists("/py")) {
        LittleFS.mkdir("/py");
    }
    if (!LittleFS.exists("/py/hello.py")) {
        saveScript("hello.py",
            "# Hello World\n"
            "# A simple Python example\n"
            "\n"
            "for i in range(5):\n"
            "    print(i)\n"
            "\n"
            "print('Hello NumOS!')\n"
        );
    }
#endif
}

// ═════════════════════════════════════════════════════════════════
// Key handling
// ═════════════════════════════════════════════════════════════════

void PythonApp::handleKey(const KeyEvent& ev) {
    // Autocomplete takes priority in editor
    if (_acOpen && _tab == Tab::EDITOR) {
        if (ev.code == KeyCode::DOWN) {
            _acIdx = (_acIdx + 1) % _acCount;
            for (int i = 0; i < _acCount; ++i) {
                if (_acRows[i]) {
                    lv_obj_set_style_bg_color(_acRows[i],
                        lv_color_hex(i == _acIdx ? COL_AC_SEL : COL_AC_BG), LV_PART_MAIN);
                }
            }
            return;
        }
        if (ev.code == KeyCode::UP) {
            _acIdx = (_acIdx - 1 + _acCount) % _acCount;
            for (int i = 0; i < _acCount; ++i) {
                if (_acRows[i]) {
                    lv_obj_set_style_bg_color(_acRows[i],
                        lv_color_hex(i == _acIdx ? COL_AC_SEL : COL_AC_BG), LV_PART_MAIN);
                }
            }
            return;
        }
        if (ev.code == KeyCode::ENTER) {
            applyAutocomplete();
            return;
        }
        if (ev.code == KeyCode::AC || ev.code == KeyCode::DEL) {
            closeAutocomplete();
            // Fall through to handle the key normally
        }
    }

    if (_focus == Focus::TAB_BAR) {
        handleTabBarKey(ev);
    } else {
        switch (_tab) {
            case Tab::SCRIPTS:  handleScriptsKey(ev); break;
            case Tab::EDITOR:   handleEditorKey(ev);  break;
            case Tab::CONSOLE:  handleConsoleKey(ev);  break;
        }
    }
}

void PythonApp::handleTabBarKey(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::LEFT:
            if (_tabIdx > 0) { _tabIdx--; refreshTabBar(); }
            break;
        case KeyCode::RIGHT:
            if (_tabIdx < 2) { _tabIdx++; refreshTabBar(); }
            break;
        case KeyCode::ENTER:
        case KeyCode::DOWN:
            switchTab(static_cast<Tab>(_tabIdx));
            _focus = Focus::CONTENT;
            refreshTabBar();
            break;
        default:
            break;
    }
}

void PythonApp::handleScriptsKey(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::UP:
            if (_btnFocus) {
                _btnFocus = false;
                refreshScriptFocus();
            } else if (_scriptIdx > 0) {
                _scriptIdx--;
                refreshScriptFocus();
            } else {
                _focus = Focus::TAB_BAR;
                refreshTabBar();
                refreshScriptFocus();
            }
            break;
        case KeyCode::DOWN:
            if (!_btnFocus && _scriptIdx < _scriptCount - 1) {
                _scriptIdx++;
                refreshScriptFocus();
            } else if (!_btnFocus) {
                _btnFocus = true;
                _btnIdx = 0;
                refreshScriptFocus();
            }
            break;
        case KeyCode::LEFT:
            if (_btnFocus && _btnIdx > 0) {
                _btnIdx--;
                refreshBtnBar();
            }
            break;
        case KeyCode::RIGHT:
            if (_btnFocus && _btnIdx < 2) {
                _btnIdx++;
                refreshBtnBar();
            }
            break;
        case KeyCode::ENTER:
            if (_btnFocus) {
                switch (_btnIdx) {
                    case 0: createNewScript(); break;
                    case 1: runScript(_scriptIdx); break;
                    case 2: deleteScript(_scriptIdx); break;
                }
            } else if (_scriptCount > 0) {
                openScriptInEditor(_scriptIdx);
            }
            break;
        case KeyCode::AC:
            _focus = Focus::TAB_BAR;
            _btnFocus = false;
            refreshTabBar();
            refreshScriptFocus();
            break;
        default:
            break;
    }
}

void PythonApp::handleEditorKey(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::AC:
            // Save and go back to tab bar
            saveCurrentScript();
            closeAutocomplete();
            _focus = Focus::TAB_BAR;
            refreshTabBar();
            break;
        case KeyCode::LEFT:
            if (_editorTA) lv_textarea_cursor_left(_editorTA);
            break;
        case KeyCode::RIGHT:
            if (_editorTA) lv_textarea_cursor_right(_editorTA);
            break;
        case KeyCode::UP:
            if (_editorTA) lv_textarea_cursor_up(_editorTA);
            break;
        case KeyCode::DOWN:
            if (_editorTA) lv_textarea_cursor_down(_editorTA);
            break;
        case KeyCode::DEL:
            if (_editorTA) lv_textarea_delete_char(_editorTA);
            closeAutocomplete();
            break;
        case KeyCode::ENTER:
            if (_editorTA) lv_textarea_add_char(_editorTA, '\n');
            closeAutocomplete();
            break;
        // Number keys
        case KeyCode::NUM_0: insertChar('0'); checkAutocomplete(); break;
        case KeyCode::NUM_1: insertChar('1'); checkAutocomplete(); break;
        case KeyCode::NUM_2: insertChar('2'); checkAutocomplete(); break;
        case KeyCode::NUM_3: insertChar('3'); checkAutocomplete(); break;
        case KeyCode::NUM_4: insertChar('4'); checkAutocomplete(); break;
        case KeyCode::NUM_5: insertChar('5'); checkAutocomplete(); break;
        case KeyCode::NUM_6: insertChar('6'); checkAutocomplete(); break;
        case KeyCode::NUM_7: insertChar('7'); checkAutocomplete(); break;
        case KeyCode::NUM_8: insertChar('8'); checkAutocomplete(); break;
        case KeyCode::NUM_9: insertChar('9'); checkAutocomplete(); break;
        // Operator / symbol keys
        case KeyCode::DOT: insertChar('.'); closeAutocomplete(); break;
        case KeyCode::ADD: insertChar('+'); closeAutocomplete(); break;
        case KeyCode::SUB: insertChar('-'); closeAutocomplete(); break;
        case KeyCode::MUL: insertChar('*'); closeAutocomplete(); break;
        case KeyCode::DIV: insertChar('/'); closeAutocomplete(); break;
        case KeyCode::LPAREN: insertChar('('); closeAutocomplete(); break;
        case KeyCode::RPAREN: insertChar(')'); closeAutocomplete(); break;
        case KeyCode::FREE_EQ: insertChar('='); closeAutocomplete(); break;
        case KeyCode::NEG: insertChar('_'); checkAutocomplete(); break;
        case KeyCode::POW: insertChar('^'); closeAutocomplete(); break;
        // Alpha letter keys (physical ALPHA keys)
        case KeyCode::ALPHA_A: insertChar('a'); checkAutocomplete(); break;
        case KeyCode::ALPHA_B: insertChar('b'); checkAutocomplete(); break;
        case KeyCode::ALPHA_C: insertChar('c'); checkAutocomplete(); break;
        case KeyCode::ALPHA_D: insertChar('d'); checkAutocomplete(); break;
        case KeyCode::ALPHA_E: insertChar('e'); checkAutocomplete(); break;
        case KeyCode::ALPHA_F: insertChar('f'); checkAutocomplete(); break;
        // Variable keys mapped to letters
        case KeyCode::VAR_X: insertChar('x'); checkAutocomplete(); break;
        case KeyCode::VAR_Y: insertChar('y'); checkAutocomplete(); break;
        // Function keys mapped to common Python chars via F1-F4
        case KeyCode::F1: insertChar(':'); closeAutocomplete(); break;
        case KeyCode::F2: insertChar(' '); closeAutocomplete(); break;
        case KeyCode::F3: insertChar('\''); closeAutocomplete(); break;
        case KeyCode::F4: insertChar('#'); closeAutocomplete(); break;
        // Trig keys reused for common letters
        case KeyCode::SIN: insertChar('s'); checkAutocomplete(); break;
        case KeyCode::COS: insertChar('c'); checkAutocomplete(); break;
        case KeyCode::TAN: insertChar('t'); checkAutocomplete(); break;
        case KeyCode::LN:  insertChar('n'); checkAutocomplete(); break;
        case KeyCode::LOG: insertChar('l'); checkAutocomplete(); break;
        case KeyCode::CONST_PI: insertChar('p'); checkAutocomplete(); break;
        case KeyCode::CONST_E: insertChar('e'); checkAutocomplete(); break;
        case KeyCode::SQRT: insertChar('r'); checkAutocomplete(); break;
        case KeyCode::ANS:  insertChar('i'); checkAutocomplete(); break;
        default:
            break;
    }
}

void PythonApp::handleConsoleKey(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::AC:
        case KeyCode::UP:
            _focus = Focus::TAB_BAR;
            refreshTabBar();
            break;
        default:
            break;
    }
}
