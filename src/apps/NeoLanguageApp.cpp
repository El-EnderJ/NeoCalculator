/**
 * NeoLanguageApp.cpp — NeoLanguage IDE implementation.
 *
 * Follows the same LVGL patterns as PythonApp.cpp:
 *   · makePanel / makeLabel static helpers
 *   · switchTab hides/shows panels
 *   · handleKey dispatches by focus region
 *
 * Phase 3 additions:
 *   · NeoStdLib integration (diff, integrate, solve, plot, etc.)
 *   · Graphics Mode overlay (plot overlay using SymExpr::evaluate directly)
 *   · print() callback routes to console textarea
 *   · Persistence: F2 save / F3 load from /neolang.nl
 *   · SymExprArena soft-reset at >70% usage
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 3 (Standard Library)
 */

#include "NeoLanguageApp.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <string>

#ifdef ARDUINO
  #include <LittleFS.h>
  #include <esp_heap_caps.h>
#else
  #include "../hal/FileSystem.h"
#endif

// ── LVGL helper — create a plain container ───────────────────────
static lv_obj_t* makePanel(lv_obj_t* parent,
                            int x, int y, int w, int h,
                            uint32_t bg = 0x1E1E2E,
                            bool scroll = false) {
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

// ── LVGL helper — create a label ────────────────────────────────
static lv_obj_t* makeLabel(lv_obj_t* parent,
                            int x, int y,
                            const char* text,
                            uint32_t color,
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

NeoLanguageApp::NeoLanguageApp()
    : _screen(nullptr)
    , _tabBar(nullptr)
    , _panelEditor(nullptr), _editor(nullptr)
    , _panelConsole(nullptr), _console(nullptr)
    , _plotOverlay(nullptr), _plotCanvas(nullptr), _plotHintLabel(nullptr)
    , _plotDrawBuf{}, _plotBuf(nullptr), _plotMode(false)
    , _activeTab(Tab::EDITOR)
    , _focus(Focus::TAB_BAR)
    , _tabIdx(0)
    , _arena(32 * 1024)
    , _symArena(32 * 1024)
{
    for (int i = 0; i < 2; ++i) {
        _tabPills[i]  = nullptr;
        _tabLabels[i] = nullptr;
    }
}

NeoLanguageApp::~NeoLanguageApp() { end(); }

// ═════════════════════════════════════════════════════════════════
// Lifecycle
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::begin() {
    if (_screen) return;
    createUI();
    _activeTab = Tab::EDITOR;
    _focus     = Focus::TAB_BAR;
    _tabIdx    = 0;
    _plotMode  = false;
    switchTab(_activeTab);
    loadFromFlash();  // Restore last editor session
}

void NeoLanguageApp::end() {
    hidePlot();  // Dismiss any active plot overlay
    _statusBar.destroy();
    if (_screen) {
        lv_obj_delete(_screen);
        _screen       = nullptr;
        _tabBar       = nullptr;
        _panelEditor  = nullptr;  _editor  = nullptr;
        _panelConsole = nullptr;  _console = nullptr;
        for (int i = 0; i < 2; ++i) {
            _tabPills[i]  = nullptr;
            _tabLabels[i] = nullptr;
        }
    }
    _arena.reset();
    _activeTab = Tab::EDITOR;
    _focus     = Focus::TAB_BAR;
}

void NeoLanguageApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();
}

// ═════════════════════════════════════════════════════════════════
// UI Creation
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::createUI() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("NeoLang");
    _statusBar.setBatteryLevel(100);

    createTabBar();
    createEditorPanel();
    createConsolePanel();
}

void NeoLanguageApp::createTabBar() {
    _tabBar = makePanel(_screen, 0, BAR_H, SCREEN_W, TAB_H, COL_TAB_BG);

    const char* titles[] = { "Editor", "Console" };
    int tw = SCREEN_W / 2;

    for (int i = 0; i < 2; ++i) {
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

        // Centre the label text in the pill.
        // Each char in lv_font_montserrat_12 is ~7 px wide; pill width is (tw-4).
        // Using 7 px/char as a reasonable estimate — exact centering would require
        // lv_txt_get_size() which is only available after full LVGL init.
        int charW = 7;
        int labelX = ((tw - 4) - static_cast<int>(strlen(titles[i])) * charW) / 2;
        _tabLabels[i] = makeLabel(pill, labelX, (TAB_H - 6 - 12) / 2,
                                   titles[i], COL_TAB_TXT_I,
                                   &lv_font_montserrat_12);
        lv_obj_set_style_text_align(_tabLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

void NeoLanguageApp::createEditorPanel() {
    _panelEditor = makePanel(_screen, 0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_EDITOR_BG);

    _editor = lv_textarea_create(_panelEditor);
    lv_obj_set_pos(_editor, 0, 0);
    lv_obj_set_size(_editor, SCREEN_W, CONTENT_H);
    lv_textarea_set_placeholder_text(_editor, "# NeoLang code here...\n# F5 = run  MODE = exit");
    lv_obj_set_style_bg_color(_editor, lv_color_hex(COL_EDITOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(_editor, lv_color_hex(COL_EDITOR_TXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(_editor, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_border_width(_editor, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_editor, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_editor, 4, LV_PART_MAIN);
    // Cursor: white block
    lv_obj_set_style_bg_color(_editor, lv_color_hex(0xCDD6F4), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(_editor, LV_OPA_COVER, LV_PART_CURSOR);
}

void NeoLanguageApp::createConsolePanel() {
    _panelConsole = makePanel(_screen, 0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_CONSOLE_BG);

    _console = lv_textarea_create(_panelConsole);
    lv_obj_set_pos(_console, 0, 0);
    lv_obj_set_size(_console, SCREEN_W, CONTENT_H);
    lv_textarea_set_text(_console, "NeoLang v0.1 ready.\nF5 to compile.\n");
    lv_obj_set_style_bg_color(_console, lv_color_hex(COL_CONSOLE_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(_console, lv_color_hex(COL_CONSOLE_TXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(_console, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_border_width(_console, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_console, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_console, 4, LV_PART_MAIN);
    // Read-only: no click / cursor interaction
    lv_textarea_set_cursor_click_pos(_console, false);
    lv_obj_remove_flag(_console, LV_OBJ_FLAG_CLICKABLE);
}

// ═════════════════════════════════════════════════════════════════
// Tab management
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::switchTab(Tab t) {
    _activeTab = t;

    // Hide both panels first
    if (_panelEditor)  lv_obj_add_flag(_panelEditor,  LV_OBJ_FLAG_HIDDEN);
    if (_panelConsole) lv_obj_add_flag(_panelConsole, LV_OBJ_FLAG_HIDDEN);

    switch (t) {
        case Tab::EDITOR:
            if (_panelEditor) lv_obj_remove_flag(_panelEditor, LV_OBJ_FLAG_HIDDEN);
            break;
        case Tab::CONSOLE:
            if (_panelConsole) lv_obj_remove_flag(_panelConsole, LV_OBJ_FLAG_HIDDEN);
            break;
    }
    refreshTabBar();
}

void NeoLanguageApp::refreshTabBar() {
    for (int i = 0; i < 2; ++i) {
        bool active  = (i == static_cast<int>(_activeTab));
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
// Key handling
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::handleKey(const KeyEvent& ev) {
    // Only act on PRESS or REPEAT events
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // Graphics Mode: any key dismisses the plot overlay
    if (_plotMode) {
        hidePlot();
        return;
    }

    // Global: MODE exits the app
    if (ev.code == KeyCode::MODE) {
        end();
        return;
    }

    // Global: F5 runs the compiler (from any tab / focus)
    if (ev.code == KeyCode::F5) {
        runCode();
        return;
    }

    if (_focus == Focus::TAB_BAR) {
        handleTabBarKey(ev);
    } else {
        if (_activeTab == Tab::EDITOR)
            handleEditorKey(ev);
        else
            handleConsoleKey(ev);
    }
}

void NeoLanguageApp::handleTabBarKey(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::LEFT:
            if (_tabIdx > 0) { _tabIdx--; refreshTabBar(); }
            break;
        case KeyCode::RIGHT:
            if (_tabIdx < 1) { _tabIdx++; refreshTabBar(); }
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

void NeoLanguageApp::handleEditorKey(const KeyEvent& ev) {
    switch (ev.code) {
        // ── Navigation ────────────────────────────────────────────
        case KeyCode::UP:
            if (_editor) lv_textarea_cursor_up(_editor);
            break;
        case KeyCode::DOWN:
            if (_editor) lv_textarea_cursor_down(_editor);
            break;
        case KeyCode::LEFT:
            if (_editor) lv_textarea_cursor_left(_editor);
            break;
        case KeyCode::RIGHT:
            if (_editor) lv_textarea_cursor_right(_editor);
            break;
        case KeyCode::AC:
            // Return focus to tab bar
            _focus = Focus::TAB_BAR;
            refreshTabBar();
            break;
        // ── Editing ───────────────────────────────────────────────
        case KeyCode::DEL:
            if (_editor) lv_textarea_delete_char(_editor);
            break;
        case KeyCode::ENTER:
            if (_editor) lv_textarea_add_char(_editor, '\n');
            break;
        // F1 = insert 4 spaces (tab-indent)
        case KeyCode::F1:
            if (_editor) lv_textarea_add_text(_editor, "    ");
            break;
        // ── Number keys ───────────────────────────────────────────
        case KeyCode::NUM_0: insertChar('0'); break;
        case KeyCode::NUM_1: insertChar('1'); break;
        case KeyCode::NUM_2: insertChar('2'); break;
        case KeyCode::NUM_3: insertChar('3'); break;
        case KeyCode::NUM_4: insertChar('4'); break;
        case KeyCode::NUM_5: insertChar('5'); break;
        case KeyCode::NUM_6: insertChar('6'); break;
        case KeyCode::NUM_7: insertChar('7'); break;
        case KeyCode::NUM_8: insertChar('8'); break;
        case KeyCode::NUM_9: insertChar('9'); break;
        // ── Operator / symbol keys ────────────────────────────────
        case KeyCode::DOT:     insertChar('.'); break;
        case KeyCode::ADD:     insertChar('+'); break;
        case KeyCode::SUB:     insertChar('-'); break;
        case KeyCode::MUL:     insertChar('*'); break;
        case KeyCode::DIV:     insertChar('/'); break;
        case KeyCode::LPAREN:  insertChar('('); break;
        case KeyCode::RPAREN:  insertChar(')'); break;
        case KeyCode::FREE_EQ: insertChar('='); break;
        case KeyCode::POW:     insertChar('^'); break;
        case KeyCode::NEG:     insertChar('-'); break;
        // ── Alpha letter keys ─────────────────────────────────────
        case KeyCode::ALPHA_A: insertChar('a'); break;
        case KeyCode::ALPHA_B: insertChar('b'); break;
        case KeyCode::ALPHA_C: insertChar('c'); break;
        case KeyCode::ALPHA_D: insertChar('d'); break;
        case KeyCode::ALPHA_E: insertChar('e'); break;
        case KeyCode::ALPHA_F: insertChar('f'); break;
        case KeyCode::VAR_X:   insertChar('x'); break;
        case KeyCode::VAR_Y:   insertChar('y'); break;
        // ── Function-key shortcuts ────────────────────────────────
        case KeyCode::F2:      saveToFlash();  break;  // F2 = save to flash
        case KeyCode::F3:      loadFromFlash(); break; // F3 = load from flash
        case KeyCode::F4:      insertChar('#'); break;
        // ── Trig keys mapped to common identifier letters ─────────
        case KeyCode::SIN:      insertChar('s'); break;
        case KeyCode::COS:      insertChar('c'); break;
        case KeyCode::TAN:      insertChar('t'); break;
        case KeyCode::LN:       insertChar('n'); break;
        case KeyCode::LOG:      insertChar('l'); break;
        case KeyCode::CONST_PI: insertChar('p'); break;
        case KeyCode::CONST_E:  insertChar('e'); break;
        case KeyCode::SQRT:     insertChar('r'); break;
        case KeyCode::ANS:      insertChar('i'); break;
        default: break;
    }
}

void NeoLanguageApp::handleConsoleKey(const KeyEvent& ev) {
    switch (ev.code) {
        case KeyCode::AC:
        case KeyCode::UP:
            // Return focus to tab bar when pressing AC or navigating up from top
            _focus = Focus::TAB_BAR;
            refreshTabBar();
            break;
        case KeyCode::DEL:
            clearConsole();
            appendConsole("NeoLang v0.1 ready.\nF5 to compile.\n");
            break;
        default:
            break;
    }
}

// ═════════════════════════════════════════════════════════════════
// Console helpers
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::appendConsole(const char* text) {
    if (_console) lv_textarea_add_text(_console, text);
}

void NeoLanguageApp::clearConsole() {
    if (_console) lv_textarea_set_text(_console, "");
}

// ═════════════════════════════════════════════════════════════════
// insertChar — add a character to the editor textarea
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::insertChar(char c) {
    if (_editor) lv_textarea_add_char(_editor, c);
}

// ═════════════════════════════════════════════════════════════════
// dumpNode — produce a readable AST summary (depth-limited)
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::dumpNode(NeoNode* node, std::string& out, int depth) {
    // Hard limit to prevent long output / stack use
    if (!node || depth > 6) return;

    // Indentation: 2 spaces per level
    for (int i = 0; i < depth; ++i) out += "  ";

    char buf[128];
    switch (node->kind) {
        case NodeKind::Program: {
            auto* p = static_cast<ProgramNode*>(node);
            std::snprintf(buf, sizeof(buf), "Program (%zu stmts)\n",
                          p->statements.size());
            out += buf;
            // Only dump first 8 statements to keep output manageable
            int shown = 0;
            for (auto* s : p->statements) {
                if (shown++ >= 8) { out += "  ...\n"; break; }
                dumpNode(s, out, depth + 1);
            }
            break;
        }
        case NodeKind::Number: {
            auto* n = static_cast<NumberNode*>(node);
            std::snprintf(buf, sizeof(buf), "Number(%s)\n", n->raw_text.c_str());
            out += buf;
            break;
        }
        case NodeKind::Symbol: {
            auto* s = static_cast<SymbolNode*>(node);
            std::snprintf(buf, sizeof(buf), "Symbol(%s)\n", s->name.c_str());
            out += buf;
            break;
        }
        case NodeKind::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node);
            const char* ops[] = {"+","-","*","/","^","==","!=","<","<=",">",">=","and","or"};
            int oi = static_cast<int>(b->op);
            std::snprintf(buf, sizeof(buf), "BinOp(%s)\n",
                          (oi < 13) ? ops[oi] : "?");
            out += buf;
            dumpNode(b->left,  out, depth + 1);
            dumpNode(b->right, out, depth + 1);
            break;
        }
        case NodeKind::UnaryOp: {
            auto* u = static_cast<UnaryOpNode*>(node);
            out += (u->op == UnaryOpNode::OpKind::Neg) ? "UnaryOp(-)\n" : "UnaryOp(not)\n";
            dumpNode(u->operand, out, depth + 1);
            break;
        }
        case NodeKind::FunctionCall: {
            auto* f = static_cast<FunctionCallNode*>(node);
            std::snprintf(buf, sizeof(buf), "Call(%s, %zu args)\n",
                          f->name.c_str(), f->args.size());
            out += buf;
            for (auto* a : f->args) dumpNode(a, out, depth + 1);
            break;
        }
        case NodeKind::Assignment: {
            auto* a = static_cast<AssignmentNode*>(node);
            std::snprintf(buf, sizeof(buf), "Assign(%s %s expr)\n",
                          a->target.c_str(), a->is_delayed ? ":=" : "=");
            out += buf;
            dumpNode(a->value, out, depth + 1);
            break;
        }
        case NodeKind::If: {
            auto* n = static_cast<IfNode*>(node);
            std::snprintf(buf, sizeof(buf),
                          "If(then=%zu, else=%zu)\n",
                          n->then_body.size(), n->else_body.size());
            out += buf;
            dumpNode(n->condition, out, depth + 1);
            break;
        }
        case NodeKind::While: {
            auto* n = static_cast<WhileNode*>(node);
            std::snprintf(buf, sizeof(buf), "While(body=%zu)\n", n->body.size());
            out += buf;
            dumpNode(n->condition, out, depth + 1);
            break;
        }
        case NodeKind::ForIn: {
            auto* n = static_cast<ForInNode*>(node);
            std::snprintf(buf, sizeof(buf), "ForIn(%s, body=%zu)\n",
                          n->var.c_str(), n->body.size());
            out += buf;
            dumpNode(n->iterable, out, depth + 1);
            break;
        }
        case NodeKind::FunctionDef: {
            auto* f = static_cast<FunctionDefNode*>(node);
            std::snprintf(buf, sizeof(buf), "FuncDef(%s, params=%zu%s)\n",
                          f->name.c_str(), f->params.size(),
                          f->is_one_liner ? ", one-liner" : "");
            out += buf;
            break;
        }
        case NodeKind::Return: {
            out += "Return\n";
            auto* r = static_cast<ReturnNode*>(node);
            if (r->value) dumpNode(r->value, out, depth + 1);
            break;
        }
        case NodeKind::SymExprWrapper: {
            auto* w = static_cast<SymExprWrapperNode*>(node);
            std::snprintf(buf, sizeof(buf), "SymExpr(%s)\n", w->repr.c_str());
            out += buf;
            break;
        }
        default:
            out += "UnknownNode\n";
            break;
    }
}

// ═════════════════════════════════════════════════════════════════
// buildHostCallbacks
// ═════════════════════════════════════════════════════════════════

NeoHostCallbacks NeoLanguageApp::buildHostCallbacks() {
    NeoHostCallbacks cbs{};
    cbs.userdata = this;

    // print() → append to console textarea
    cbs.onPrint = [](const char* text, void* ud) {
        auto* app = static_cast<NeoLanguageApp*>(ud);
        app->appendConsole(text);
    };

    // msg_box() → print to console (full dialog would require LVGL msgbox)
    cbs.onMsgBox = [](const char* title, const char* content, void* ud) {
        auto* app = static_cast<NeoLanguageApp*>(ud);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "[%s] %s\n", title, content);
        app->appendConsole(buf);
    };

    // input_num() — not yet interactive; returns 0 and prints a notice
    cbs.onInputNum = [](const char* prompt, void* ud) -> double {
        auto* app = static_cast<NeoLanguageApp*>(ud);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[input_num] '%s' = 0 (not interactive)\n", prompt);
        app->appendConsole(buf);
        return 0.0;
    };

    return cbs;
}

// ═════════════════════════════════════════════════════════════════
// runCode — tokenize + parse + interpret
// ═════════════════════════════════════════════════════════════════

void NeoLanguageApp::runCode() {
    if (!_editor) return;

    const char* src = lv_textarea_get_text(_editor);
    if (!src || src[0] == '\0') {
        switchTab(Tab::CONSOLE);
        clearConsole();
        appendConsole("[NeoLang] Empty editor.\n");
        return;
    }

    clearConsole();
    appendConsole("[NeoLang] Running...\n");

    // ── Memory protection: soft-reset SymExprArena if >70% used ──
    {
        auto st = _symArena.stats();
        size_t capacity = _symArena.totalAllocated();
        // estimatedUsed() needs the block size used during arena construction
        // (32 KB, matching the NeoLanguageApp constructor: SymExprArena(32*1024))
        static constexpr size_t SYM_ARENA_BLOCK_SIZE = 32 * 1024;
        size_t used = st.estimatedUsed(SYM_ARENA_BLOCK_SIZE);
        if (capacity > 0 && (float)used / (float)capacity > ARENA_RESET_THRESHOLD) {
            _symArena.reset();
            appendConsole("[SYM] Arena soft-reset (>70% used)\n");
        }
    }

    // ── Tokenise ─────────────────────────────────────────────────
    std::string source(src);
    neo::NeoLexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.lastError().empty()) {
        appendConsole("[Lex ERROR] ");
        appendConsole(lexer.lastError().c_str());
        appendConsole("\n");
    }

    // ── Parse ─────────────────────────────────────────────────────
    _arena.reset();
    NeoParser parser(_arena);
    NeoNode* root = parser.parse(tokens);

    if (parser.hasError()) {
        appendConsole("[Parse ERROR] ");
        appendConsole(parser.lastError().c_str());
        appendConsole("\n");
        _focus = Focus::CONTENT;
        switchTab(Tab::CONSOLE);
        return;
    }

    // ── Interpret ─────────────────────────────────────────────────
    _symArena.reset();
    NeoInterpreter interp(_symArena);

    // Connect host callbacks (print, msg_box, input_num)
    interp.setHostCallbacks(buildHostCallbacks());

    NeoEnv globalEnv;

    if (root) {
        auto* prog = static_cast<ProgramNode*>(root);
        char buf[128];

        // Evaluate each top-level statement and display its result
        for (NeoNode* stmt : prog->statements) {
            NeoValue val = interp.eval(stmt, globalEnv);

            if (interp.hasError()) {
                appendConsole("[ERROR] ");
                appendConsole(interp.lastError().c_str());
                appendConsole("\n");
                interp.clearErrors();
                continue;
            }

            // Show non-Null results (but skip print output — it's already
            // been sent to the console via the onPrint callback)
            if (!val.isNull()) {
                std::string s = val.toString();
                // Limit output length per result
                if (s.size() > MAX_RESULT_CHARS)
                    s = s.substr(0, MAX_RESULT_CHARS) + "...";
                appendConsole("=> ");
                appendConsole(s.c_str());
                appendConsole("\n");
            }
        }

        std::snprintf(buf, sizeof(buf), "[Done]\n");
        appendConsole(buf);
    }

    // Switch to console so the user sees results
    _focus = Focus::CONTENT;
    switchTab(Tab::CONSOLE);

    // ── Graphics Mode: check for pending plot request ──────────────
    const NeoPlotRequest& req = interp.plotRequest();
    if (req.pending && req.expr) {
        showPlot(req);
        interp.clearPlotRequest();
    }
}

// ═════════════════════════════════════════════════════════════════
// Graphics Mode — plot overlay
// ═════════════════════════════════════════════════════════════════
//
// Design note: the plot uses SymExpr::evaluate(x) directly, NOT
// GrapherApp's string-based evaluator.  This fully decouples
// NeoLanguage from GrapherApp: upgrading GrapherApp requires no
// changes here.

void NeoLanguageApp::showPlot(const NeoPlotRequest& req) {
    if (!_screen || _plotMode) return;

    // ── Allocate pixel buffer ────────────────────────────────────
    const int BUF_SIZE = PLOT_W * PLOT_H * 2;  // RGB565, 2 bytes/pixel
#ifdef ARDUINO
    _plotBuf = (uint8_t*)heap_caps_malloc(BUF_SIZE, MALLOC_CAP_SPIRAM);
#else
    _plotBuf = (uint8_t*)malloc(BUF_SIZE);
#endif
    if (!_plotBuf) {
        appendConsole("[plot] Not enough memory for plot buffer.\n");
        return;
    }
    memset(_plotBuf, 0, BUF_SIZE);

    // ── Create overlay container ─────────────────────────────────
    _plotOverlay = lv_obj_create(_screen);
    lv_obj_set_pos(_plotOverlay, 0, BAR_H);
    lv_obj_set_size(_plotOverlay, PLOT_W, PLOT_H + PLOT_HINT_H);
    lv_obj_set_style_bg_color(_plotOverlay, lv_color_hex(COL_PLOT_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_plotOverlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_plotOverlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_plotOverlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_plotOverlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_plotOverlay, LV_OBJ_FLAG_SCROLLABLE);

    // ── Image object backed by our pixel buffer ──────────────────
    static lv_image_dsc_t imgDsc;
    memset(&imgDsc, 0, sizeof(imgDsc));
    imgDsc.header.w  = PLOT_W;
    imgDsc.header.h  = PLOT_H;
    imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    imgDsc.data_size = (uint32_t)BUF_SIZE;
    imgDsc.data      = _plotBuf;

    _plotCanvas = lv_image_create(_plotOverlay);
    lv_obj_set_pos(_plotCanvas, 0, 0);
    lv_obj_set_size(_plotCanvas, PLOT_W, PLOT_H);
    lv_image_set_src(_plotCanvas, &imgDsc);

    // ── Hint label ───────────────────────────────────────────────
    _plotHintLabel = lv_label_create(_plotOverlay);
    lv_obj_set_pos(_plotHintLabel, 0, PLOT_H + 2);
    lv_obj_set_size(_plotHintLabel, PLOT_W, PLOT_HINT_H);
    lv_label_set_text(_plotHintLabel, "Press any key to return");
    lv_obj_set_style_text_color(_plotHintLabel, lv_color_hex(COL_TAB_TXT_I), LV_PART_MAIN);
    lv_obj_set_style_text_font(_plotHintLabel, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_align(_plotHintLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // ── Render the plot ──────────────────────────────────────────
    renderPlot(req.expr, req.xMin, req.xMax);

    // Refresh the image widget so LVGL re-reads the pixel buffer
    lv_image_set_src(_plotCanvas, &imgDsc);
    lv_obj_invalidate(_plotCanvas);

    _plotMode = true;
}

void NeoLanguageApp::hidePlot() {
    if (!_plotMode) return;
    _plotMode = false;

    if (_plotOverlay) {
        lv_obj_delete(_plotOverlay);
        _plotOverlay    = nullptr;
        _plotCanvas     = nullptr;
        _plotHintLabel  = nullptr;
    }
    if (_plotBuf) {
#ifdef ARDUINO
        heap_caps_free(_plotBuf);
#else
        free(_plotBuf);
#endif
        _plotBuf = nullptr;
    }
}

// Render a function expressed as a SymExpr* into the pixel buffer.
//
// Key design: uses SymExpr::evaluate(x) directly — independent of any
// string-evaluator or GrapherApp internal.  If GrapherApp is upgraded,
// this function does not need to change.

void NeoLanguageApp::renderPlot(cas::SymExpr* expr, double xMin, double xMax) {
    if (!_plotBuf || !expr) return;

    const int W = PLOT_W;
    const int H = PLOT_H;

    // ── Compute y range by sampling ──────────────────────────────
    double yMin =  1e30, yMax = -1e30;
    for (int i = 0; i <= PLOT_SAMPLE_PTS; ++i) {
        double x = xMin + (xMax - xMin) * i / PLOT_SAMPLE_PTS;
        double y = expr->evaluate(x);
        if (std::isnan(y) || std::isinf(y)) continue;
        if (y < yMin) yMin = y;
        if (y > yMax) yMax = y;
    }
    // Ensure a usable range
    if (yMax <= yMin) { yMin -= 5.0; yMax += 5.0; }
    double yRange = yMax - yMin;
    double xRange = xMax - xMin;

    // ── RGB565 pixel helpers ──────────────────────────────────────
    auto setPixel = [&](int px, int py, uint32_t rgb) {
        if (px < 0 || px >= W || py < 0 || py >= H) return;
        uint16_t r = (rgb >> 16) & 0xFF;
        uint16_t g = (rgb >>  8) & 0xFF;
        uint16_t b = (rgb      ) & 0xFF;
        uint16_t c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        int idx = (py * W + px) * 2;
        _plotBuf[idx]     = (uint8_t)(c >> 8);
        _plotBuf[idx + 1] = (uint8_t)(c & 0xFF);
    };

    // ── Draw background ───────────────────────────────────────────
    for (int py = 0; py < H; ++py)
        for (int px = 0; px < W; ++px)
            setPixel(px, py, COL_PLOT_BG);

    // ── Draw axes ────────────────────────────────────────────────
    // X-axis (y=0 line)
    if (yMin <= 0.0 && yMax >= 0.0) {
        int yZero = (int)((1.0 - (0.0 - yMin) / yRange) * H);
        for (int px = 0; px < W; ++px) setPixel(px, yZero, COL_PLOT_AXIS);
    }
    // Y-axis (x=0 line)
    if (xMin <= 0.0 && xMax >= 0.0) {
        int xZero = (int)((-xMin) / xRange * W);
        for (int py = 0; py < H; ++py) setPixel(xZero, py, COL_PLOT_AXIS);
    }

    // ── Draw curve ───────────────────────────────────────────────
    double prevY = std::numeric_limits<double>::quiet_NaN();
    int    prevPy = 0;

    for (int px = 0; px < W; ++px) {
        double x  = xMin + (double)px / W * xRange;
        double y  = expr->evaluate(x);

        if (std::isnan(y) || std::isinf(y)) {
            prevY = std::numeric_limits<double>::quiet_NaN();
            continue;
        }

        int py = (int)((1.0 - (y - yMin) / yRange) * H);

        // Connect with previous pixel using vertical line segment
        if (!std::isnan(prevY) && std::abs(py - prevPy) < H) {
            int y0 = (py < prevPy) ? py : prevPy;
            int y1 = (py > prevPy) ? py : prevPy;
            for (int vy = y0; vy <= y1; ++vy)
                setPixel(px, vy, COL_PLOT_LINE);
        } else {
            setPixel(px, py, COL_PLOT_LINE);
        }

        prevY  = y;
        prevPy = py;
    }

    // ── Axis labels ───────────────────────────────────────────────
    // (Omitted in this lightweight renderer to avoid font dependency.
    //  Axis values are shown as part of the hint text if needed.)
}

// ═════════════════════════════════════════════════════════════════
// Persistence — save/load editor content via LittleFS
// ═════════════════════════════════════════════════════════════════

static constexpr const char* NEOLANG_FILE = "/neolang.nl";

void NeoLanguageApp::saveToFlash() {
    if (!_editor) return;
    const char* src = lv_textarea_get_text(_editor);
    if (!src) return;

#ifdef ARDUINO
    if (!LittleFS.begin(true)) {
        appendConsole("[save] LittleFS unavailable.\n");
        return;
    }
    File f = LittleFS.open(NEOLANG_FILE, "w");
#else
    File f = LittleFS.open(NEOLANG_FILE, "w");
#endif
    if (!f) {
        appendConsole("[save] Could not open file.\n");
        return;
    }
    size_t len = strlen(src);
    f.write((const uint8_t*)src, len);
    f.close();

    char buf[64];
    std::snprintf(buf, sizeof(buf), "[save] %zu bytes to %s\n", len, NEOLANG_FILE);
    appendConsole(buf);
}

void NeoLanguageApp::loadFromFlash() {
#ifdef ARDUINO
    if (!LittleFS.begin(true)) return;
    if (!LittleFS.exists(NEOLANG_FILE)) return;
    File f = LittleFS.open(NEOLANG_FILE, "r");
#else
    if (!LittleFS.exists(NEOLANG_FILE)) return;
    File f = LittleFS.open(NEOLANG_FILE, "r");
#endif
    if (!f) return;

    // Read up to 4 KB — use heap to avoid stack pressure on ESP32
    static constexpr size_t MAX_LOAD = 4096;
#ifdef ARDUINO
    char* buf = (char*)heap_caps_malloc(MAX_LOAD + 1, MALLOC_CAP_SPIRAM);
#else
    char* buf = (char*)malloc(MAX_LOAD + 1);
#endif
    if (!buf) { f.close(); return; }

    size_t bytesRead = f.read((uint8_t*)buf, MAX_LOAD);
    f.close();

    if (bytesRead > 0 && _editor) {
        buf[bytesRead] = '\0';
        lv_textarea_set_text(_editor, buf);
    }

#ifdef ARDUINO
    heap_caps_free(buf);
#else
    free(buf);
#endif
}
