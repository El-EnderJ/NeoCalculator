/**
 * NeoLanguageApp.cpp — NeoLanguage IDE implementation.
 *
 * Follows the same LVGL patterns as PythonApp.cpp:
 *   · makePanel / makeLabel static helpers
 *   · switchTab hides/shows panels
 *   · handleKey dispatches by focus region
 *
 * F5 runs the compiler frontend and shows results in the console tab.
 * MODE exits the app (caller SystemApp returns to menu).
 * F1 inserts 4 spaces (tab-indent).
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
 */

#include "NeoLanguageApp.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>

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
    , _activeTab(Tab::EDITOR)
    , _focus(Focus::TAB_BAR)
    , _tabIdx(0)
    , _arena(32 * 1024)
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
    switchTab(_activeTab);
}

void NeoLanguageApp::end() {
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
        // ── Function-key letter shortcuts ─────────────────────────
        case KeyCode::F2:      insertChar(':'); break;
        case KeyCode::F3:      insertChar(' '); break;
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
// runCode — tokenize + parse + display summary
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
    appendConsole("[NeoLang] Compiling...\n");

    // ── Tokenise ─────────────────────────────────────────────────
    std::string source(src);
    neo::NeoLexer lexer(source);
    auto tokens = lexer.tokenize();

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Tokens: %zu\n", tokens.size());
    appendConsole(buf);

    if (!lexer.lastError().empty()) {
        appendConsole("[Lex ERROR] ");
        appendConsole(lexer.lastError().c_str());
        appendConsole("\n");
    }

    // Print first few tokens for debugging
    int shown = 0;
    for (const auto& tok : tokens) {
        if (shown++ >= 10) { appendConsole("  ...\n"); break; }
        if (tok.type == NeoTokType::END_OF_FILE) break;
        std::snprintf(buf, sizeof(buf), "  [%d] '%s'\n",
                      static_cast<int>(tok.type), tok.value.c_str());
        appendConsole(buf);
    }

    // ── Parse ─────────────────────────────────────────────────────
    _arena.reset();
    NeoParser parser(_arena);
    NeoNode* root = parser.parse(tokens);

    if (parser.hasError()) {
        appendConsole("[Parse ERROR] ");
        appendConsole(parser.lastError().c_str());
        appendConsole("\n");
    } else {
        appendConsole("[Parse OK]\n");
    }

    // ── AST summary ───────────────────────────────────────────────
    if (root) {
        std::string summary;
        summary.reserve(512);
        dumpNode(root, summary, 0);
        appendConsole(summary.c_str());
    }

    // Switch to console so the user sees results
    _focus = Focus::CONTENT;
    switchTab(Tab::CONSOLE);
}
