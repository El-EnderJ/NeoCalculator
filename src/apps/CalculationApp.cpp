/**
 * CalculationApp.cpp — Natural V.P.A.M. Engine
 * Supports POWER (superscript), fractions, roots.
 * Using DisplayDriver for drawing.
 */
#include "CalculationApp.h"
#include "ui/Theme.h"
#include <math.h>

/* ── Layout ── */
static const int SW = 320, SH = 240;
static const int BAR_H = 24;
// Metrics for standard font
static const int RAD_HOOK = 3, RAD_RISE = 5; 
static const int RAD_LEAD = RAD_HOOK+RAD_RISE+2; 
static const int RAD_VPAD = 3;             
static const int POW_LIFT = 6;             

/* ── Colours ── */
static const uint16_t BG   = 0xFFFF;
static const uint16_t CBLK = 0x0000;

/* ═══════════════════════  Constructor / Init  ═══════════════════════ */

CalculationApp::CalculationApp(DisplayDriver &disp, VariableContext &vars)
    : _display(disp), _tokenizer(), _parser(), _evaluator(), _vars(vars),
      _angleMode(AngleMode::DEG), _root(nullptr), _cursor(),
      _shiftActive(false), _redraw(true), _histBrowsing(false), _histSelIdx(-1)
{ resetInput(); }

CalculationApp::~CalculationApp() { ExprNode::freeChain(_root); }

void CalculationApp::begin() {
    _history.clear(); _histBrowsing = false; _histSelIdx = -1;
    resetInput(); _redraw = true;
}

void CalculationApp::resetInput() {
    ExprNode::freeChain(_root);
    _root = ExprNode::newText("");
    _cursor.node = _root; _cursor.offset = 0; _cursor.part = CursorPart::MAIN;
}

/* ═══════════════════════  Tree helpers  ═══════════════════════ */

CalculationApp::ParentInfo CalculationApp::findParent(ExprNode* head, ExprNode* tgt) {
    for (ExprNode* n = head; n; n = n->next) {
        if (n->type == NodeType::FRACTION) {
            for (ExprNode* c = n->numerator; c; c = c->next) if (c == tgt) return {n, CursorPart::FRAC_NUM};
            for (ExprNode* c = n->denominator; c; c = c->next) if (c == tgt) return {n, CursorPart::FRAC_DEN};
            ParentInfo r = findParent(n->numerator, tgt); if (r.node) return r;
            r = findParent(n->denominator, tgt); if (r.node) return r;
        } else if (n->type == NodeType::ROOT) {
            if (n->rootIndex) { for (ExprNode* c = n->rootIndex; c; c = c->next) if (c == tgt) return {n, CursorPart::ROOT_INDEX}; ParentInfo r = findParent(n->rootIndex, tgt); if (r.node) return r; }
            for (ExprNode* c = n->radicand; c; c = c->next) if (c == tgt) return {n, CursorPart::ROOT_RADICAND};
            ParentInfo r = findParent(n->radicand, tgt); if (r.node) return r;
        } else if (n->type == NodeType::POWER) {
            for (ExprNode* c = n->exponent; c; c = c->next) if (c == tgt) return {n, CursorPart::POWER_EXP};
            ParentInfo r = findParent(n->exponent, tgt); if (r.node) return r;
        }
    }
    return {nullptr, CursorPart::MAIN};
}

ExprNode* CalculationApp::lastTextInChain(ExprNode* head) {
    ExprNode* last = nullptr;
    for (ExprNode* n = head; n; n = n->next)
        if (n->type == NodeType::TEXT) last = n;
    return last;
}

/* ═══════════════════════  Node manipulation  ═══════════════════════ */

void CalculationApp::insertChar(char c) {
    if (c == '^') { insertPower(); return; } 
    ExprNode* tn = _cursor.node;
    if (!tn || tn->type != NodeType::TEXT) return;
    String pre = tn->text.substring(0, _cursor.offset);
    String post = tn->text.substring(_cursor.offset);
    tn->text = pre + String(c) + post;
    _cursor.offset++; _redraw = true;
}

void CalculationApp::insertBlock(const String &blk) {
    ExprNode* tn = _cursor.node;
    if (!tn || tn->type != NodeType::TEXT) return;
    String after = tn->text.substring(_cursor.offset);
    tn->text = tn->text.substring(0, _cursor.offset);
    ExprNode* b = ExprNode::newText(blk, true);
    ExprNode* a = ExprNode::newText(after);
    a->next = tn->next; b->next = a; tn->next = b;
    _cursor.node = a; _cursor.offset = 0; _redraw = true;
}

void CalculationApp::insertFraction() {
    ExprNode* tn = _cursor.node;
    if (!tn || tn->type != NodeType::TEXT) return;
    String before = tn->text.substring(0, _cursor.offset);
    String after  = tn->text.substring(_cursor.offset);
    int ns = before.length();
    while (ns > 0 && (isDigit(before[ns-1]) || before[ns-1] == '.')) ns--;
    String numStr = before.substring(ns);
    tn->text = before.substring(0, ns);
    ExprNode* frac = ExprNode::newFraction();
    if (numStr.length() > 0) frac->numerator->text = numStr;
    ExprNode* aft = ExprNode::newText(after);
    aft->next = tn->next; frac->next = aft; tn->next = frac;
    if (numStr.length() > 0) {
        _cursor.node = frac->denominator; _cursor.offset = 0; _cursor.part = CursorPart::FRAC_DEN;
    } else {
        _cursor.node = frac->numerator; _cursor.offset = 0; _cursor.part = CursorPart::FRAC_NUM;
    }
    _redraw = true;
}

void CalculationApp::insertRoot(bool withIndex) {
    ExprNode* tn = _cursor.node;
    if (!tn || tn->type != NodeType::TEXT) return;
    String after = tn->text.substring(_cursor.offset);
    tn->text = tn->text.substring(0, _cursor.offset);
    ExprNode* r = ExprNode::newRoot(withIndex);
    ExprNode* a = ExprNode::newText(after);
    a->next = tn->next; r->next = a; tn->next = r;
    if (withIndex) {
        _cursor.node = r->rootIndex; _cursor.offset = 0; _cursor.part = CursorPart::ROOT_INDEX;
    } else {
        _cursor.node = r->radicand; _cursor.offset = 0; _cursor.part = CursorPart::ROOT_RADICAND;
    }
    _redraw = true;
}

void CalculationApp::insertPower() {
    ExprNode* tn = _cursor.node;
    if (!tn || tn->type != NodeType::TEXT) return;
    String after = tn->text.substring(_cursor.offset);
    tn->text = tn->text.substring(0, _cursor.offset);
    ExprNode* p = ExprNode::newPower();
    ExprNode* a = ExprNode::newText(after);
    a->next = tn->next; p->next = a; tn->next = p;
    _cursor.node = p->exponent; _cursor.offset = 0; _cursor.part = CursorPart::POWER_EXP;
    _redraw = true;
}

void CalculationApp::deleteAtCursor() {
    ExprNode* tn = _cursor.node;
    if (!tn || tn->type != NodeType::TEXT) return;
    if (_cursor.offset > 0) {
        if (tn->isBlock) { 
            ParentInfo pi = findParent(_root, tn);
            ExprNode* chain = _root;
            if (pi.node) {
                 if (pi.part == CursorPart::FRAC_NUM) chain = pi.node->numerator;
                 else if (pi.part == CursorPart::FRAC_DEN) chain = pi.node->denominator;
                 else if (pi.part == CursorPart::ROOT_INDEX) chain = pi.node->rootIndex;
                 else if (pi.part == CursorPart::ROOT_RADICAND) chain = pi.node->radicand;
                 else if (pi.part == CursorPart::POWER_EXP) chain = pi.node->exponent;
            }
            ExprNode* prev = nullptr; for (ExprNode* s = chain; s && s != tn; s = s->next) prev = s;
            if (prev) { prev->next = tn->next; } else {
                ExprNode* rep = ExprNode::newText(""); rep->next = tn->next;
                if (pi.node) {
                    if (pi.part == CursorPart::FRAC_NUM) pi.node->numerator = rep;
                    else if (pi.part == CursorPart::FRAC_DEN) pi.node->denominator = rep;
                    else if (pi.part == CursorPart::ROOT_INDEX) pi.node->rootIndex = rep;
                    else if (pi.part == CursorPart::ROOT_RADICAND) pi.node->radicand = rep;
                    else if (pi.part == CursorPart::POWER_EXP) pi.node->exponent = rep;
                } else _root = rep;
                _cursor.node = rep; _cursor.offset = 0;
            }
            tn->next = nullptr; delete tn;
        } else {
            tn->text = tn->text.substring(0, _cursor.offset-1) + tn->text.substring(_cursor.offset);
            _cursor.offset--;
        }
        _redraw = true; return;
    }
    // offset == 0
    if (_cursor.part != CursorPart::MAIN) return; 
    ExprNode* prev = nullptr; ExprNode* scan = _root;
    while (scan && scan != tn) { prev = scan; scan = scan->next; }
    if (!prev) return;
    
    if (prev->type == NodeType::TEXT) {
        if (prev->isBlock) {
             _cursor.offset = 0; ExprNode* rep = ExprNode::newText(tn->text); rep->next = tn->next;
             ExprNode *pp = nullptr, *s2 = _root; while(s2 && s2 != prev) { pp = s2; s2 = s2->next; }
             if (pp) pp->next = rep; else _root = rep;
             prev->next = nullptr; delete prev; tn->next = nullptr; delete tn;
             _cursor.node = rep;
        } else {
             _cursor.offset = prev->text.length();
             prev->text += tn->text; prev->next = tn->next;
             tn->next = nullptr; delete tn; _cursor.node = prev;
        }
    } else { 
        String flat = "";
        if (prev->type == NodeType::FRACTION) flat = flattenChain(prev->numerator); 
        else if (prev->type == NodeType::ROOT) flat = flattenChain(prev->radicand);
        else if (prev->type == NodeType::POWER) flat = flattenChain(prev->exponent); 
        
        ExprNode *pp = nullptr; ExprNode *s2 = _root;
        while (s2 && s2 != prev) { pp = s2; s2 = s2->next; }
        ExprNode* rep = ExprNode::newText(flat + tn->text);
        rep->next = tn->next;
        if (pp) pp->next = rep; else _root = rep;
        
        if (prev->type == NodeType::FRACTION) { prev->numerator = nullptr; prev->denominator = nullptr; }
        else if (prev->type == NodeType::ROOT) { prev->rootIndex = nullptr; prev->radicand = nullptr; }
        else if (prev->type == NodeType::POWER) { prev->exponent = nullptr; }
        
        prev->next = nullptr; ExprNode::freeChain(prev);
        tn->next = nullptr; delete tn;
        _cursor.node = rep; _cursor.offset = flat.length(); _cursor.part = CursorPart::MAIN;
    }
    _redraw = true;
}

/* ═══════════════════════  Cursor movement  ═══════════════════════ */

void CalculationApp::cursorLeft() {
    ExprNode* tn = _cursor.node; if (!tn) return;
    if (_cursor.offset > 0) {
        _cursor.offset = tn->isBlock ? 0 : _cursor.offset - 1;
        _redraw = true; return;
    }
    // offset == 0
    ParentInfo pi = findParent(_root, tn);
    ExprNode* chain = _root;
    if (pi.node) {
        if (pi.part == CursorPart::FRAC_NUM) chain = pi.node->numerator;
        else if (pi.part == CursorPart::FRAC_DEN) chain = pi.node->denominator;
        else if (pi.part == CursorPart::ROOT_INDEX) chain = pi.node->rootIndex;
        else if (pi.part == CursorPart::ROOT_RADICAND) chain = pi.node->radicand;
        else if (pi.part == CursorPart::POWER_EXP) chain = pi.node->exponent;
    }
    ExprNode* prev = nullptr;
    for (ExprNode* s = chain; s && s != tn; s = s->next) prev = s;
    if (prev) {
        if (prev->type == NodeType::TEXT) { _cursor.node = prev; _cursor.offset = prev->text.length(); }
        else if (prev->type == NodeType::FRACTION) {
            ExprNode* lt = lastTextInChain(prev->denominator);
            if (lt) { _cursor.node = lt; _cursor.offset = lt->text.length(); _cursor.part = CursorPart::FRAC_DEN; }
        } else if (prev->type == NodeType::ROOT) {
            ExprNode* lt = lastTextInChain(prev->radicand);
            if (lt) { _cursor.node = lt; _cursor.offset = lt->text.length(); _cursor.part = CursorPart::ROOT_RADICAND; }
        } else if (prev->type == NodeType::POWER) {
             ExprNode* lt = lastTextInChain(prev->exponent);
             if (lt) { _cursor.node = lt; _cursor.offset = lt->text.length(); _cursor.part = CursorPart::POWER_EXP; }
        }
    } else if (pi.node) {
        if (pi.part == CursorPart::FRAC_DEN) {
             ExprNode* lt = lastTextInChain(pi.node->numerator);
             if (lt) { _cursor.node = lt; _cursor.offset = lt->text.length(); _cursor.part = CursorPart::FRAC_NUM; }
        } else if (pi.part == CursorPart::ROOT_RADICAND && pi.node->rootIndex) {
             ExprNode* lt = lastTextInChain(pi.node->rootIndex);
             if (lt) { _cursor.node = lt; _cursor.offset = lt->text.length(); _cursor.part = CursorPart::ROOT_INDEX; }
        } else {
             ParentInfo pp = findParent(_root, pi.node);
             ExprNode* outer = _root;
             if (pp.node) {
                 if (pp.part == CursorPart::FRAC_NUM) outer = pp.node->numerator;
                 else if (pp.part == CursorPart::FRAC_DEN) outer = pp.node->denominator;
                 else if (pp.part == CursorPart::ROOT_RADICAND) outer = pp.node->radicand;
                 else if (pp.part == CursorPart::ROOT_INDEX) outer = pp.node->rootIndex;
                 else if (pp.part == CursorPart::POWER_EXP) outer = pp.node->exponent;
             }
             ExprNode* pb = nullptr; for(ExprNode* s=outer; s && s!=pi.node; s=s->next) pb=s;
             if (pb && pb->type == NodeType::TEXT) { _cursor.node = pb; _cursor.offset = pb->text.length(); _cursor.part = pp.node ? pp.part : CursorPart::MAIN; }
        }
    }
    _redraw = true;
}

void CalculationApp::cursorRight() {
    ExprNode* tn = _cursor.node; if (!tn) return;
    if (tn->type == NodeType::TEXT && _cursor.offset < (int)tn->text.length()) {
        _cursor.offset = tn->isBlock ? tn->text.length() : _cursor.offset + 1;
        _redraw = true; return;
    }
    ExprNode* nx = tn->next;
    if (nx) {
        if (nx->type == NodeType::TEXT) { _cursor.node = nx; _cursor.offset = 0; }
        else if (nx->type == NodeType::FRACTION) { _cursor.node = nx->numerator; _cursor.offset = 0; _cursor.part = CursorPart::FRAC_NUM; }
        else if (nx->type == NodeType::ROOT) {
            if (nx->rootIndex) { _cursor.node = nx->rootIndex; _cursor.offset = 0; _cursor.part = CursorPart::ROOT_INDEX; }
            else { _cursor.node = nx->radicand; _cursor.offset = 0; _cursor.part = CursorPart::ROOT_RADICAND; }
        } else if (nx->type == NodeType::POWER) {
             _cursor.node = nx->exponent; _cursor.offset = 0; _cursor.part = CursorPart::POWER_EXP;
        }
    } else {
        ParentInfo pi = findParent(_root, tn);
        if (pi.node) {
            if (pi.part == CursorPart::FRAC_NUM) { _cursor.node = pi.node->denominator; _cursor.offset = 0; _cursor.part = CursorPart::FRAC_DEN; }
            else if (pi.part == CursorPart::ROOT_INDEX) { _cursor.node = pi.node->radicand; _cursor.offset = 0; _cursor.part = CursorPart::ROOT_RADICAND; }
            else { 
                ExprNode* after = pi.node->next;
                if (after && after->type == NodeType::TEXT) { 
                    _cursor.node = after; _cursor.offset = 0; 
                    ParentInfo gp = findParent(_root, pi.node);
                    _cursor.part = gp.node ? gp.part : CursorPart::MAIN;
                }
            }
        }
    }
    _redraw = true;
}

void CalculationApp::cursorUp() {
    ParentInfo pi = findParent(_root, _cursor.node);
    if (pi.node && pi.part == CursorPart::FRAC_DEN) {
        _cursor.node = pi.node->numerator;
        _cursor.offset = min(_cursor.offset, (int)pi.node->numerator->text.length());
        _cursor.part = CursorPart::FRAC_NUM; _redraw = true;
    } else if (pi.node && pi.part == CursorPart::ROOT_RADICAND && pi.node->rootIndex) {
        _cursor.node = pi.node->rootIndex;
        _cursor.offset = min(_cursor.offset, (int)pi.node->rootIndex->text.length());
        _cursor.part = CursorPart::ROOT_INDEX; _redraw = true;
    } else {
        if (!_histBrowsing && !_history.empty()) {
            _histBrowsing = true; _histSelIdx = (int)_history.size() - 1; _redraw = true;
        } else if (_histBrowsing && _histSelIdx > 0) {
            _histSelIdx--; _redraw = true;
        }
    }
}

void CalculationApp::cursorDown() {
    ParentInfo pi = findParent(_root, _cursor.node);
    if (pi.node && pi.part == CursorPart::FRAC_NUM) {
        _cursor.node = pi.node->denominator;
        _cursor.offset = min(_cursor.offset, (int)pi.node->denominator->text.length());
        _cursor.part = CursorPart::FRAC_DEN; _redraw = true;
    } else if (pi.node && pi.part == CursorPart::ROOT_INDEX) {
        _cursor.node = pi.node->radicand;
        _cursor.offset = min(_cursor.offset, (int)pi.node->radicand->text.length());
        _cursor.part = CursorPart::ROOT_RADICAND; _redraw = true;
    } else if (pi.node && pi.part == CursorPart::POWER_EXP) {
        ExprNode* after = pi.node->next; 
        if (after && after->type == NodeType::TEXT) {
             _cursor.node = after; _cursor.offset = 0;
             ParentInfo gp = findParent(_root, pi.node);
             _cursor.part = gp.node ? gp.part : CursorPart::MAIN;
             _redraw = true;
        }
    } else if (_histBrowsing) {
        if (_histSelIdx < (int)_history.size() - 1) _histSelIdx++;
        else { _histBrowsing = false; _histSelIdx = -1; }
        _redraw = true;
    }
}

/* ═══════════════════════  Flatten / Evaluate  ═══════════════════════ */

String CalculationApp::flattenChain(ExprNode* head) {
    String r = "";
    for (ExprNode* n = head; n; n = n->next) {
        if (n->type == NodeType::TEXT) r += n->text;
        else if (n->type == NodeType::FRACTION) r += "((" + flattenChain(n->numerator) + ")/(" + flattenChain(n->denominator) + "))";
        else if (n->type == NodeType::ROOT) {
            if (n->rootIndex) r += "((" + flattenChain(n->radicand) + ")^(1/(" + flattenChain(n->rootIndex) + ")))";
            else r += "sqrt(" + flattenChain(n->radicand) + ")";
        } else if (n->type == NodeType::POWER) {
            r += "^(" + flattenChain(n->exponent) + ")";
        }
    }
    return r;
}

void CalculationApp::evaluate() {
    String flat = flattenChain(_root);
    if (flat.length() == 0) return;
    _evaluator.setAngleMode(_angleMode);
    TokenizeResult tok = _tokenizer.tokenize(flat);
    if (!tok.ok) { _history.push_back({flat, tok.errorMessage, true}); resetInput(); _redraw = true; return; }
    ParseResult parse = _parser.toRPN(tok.tokens);
    if (!parse.ok) { _history.push_back({flat, parse.errorMessage, true}); resetInput(); _redraw = true; return; }
    EvalResult res = _evaluator.evaluateRPN(parse.outputRPN, _vars);
    if (!res.ok) { _history.push_back({flat, res.errorMessage, true}); }
    else {
        _vars.setAns(res.value);
        String rs;
        double v = res.value;
        if (v == (int64_t)v && fabs(v) < 1e12) rs = String((int64_t)v);
        else { rs = String(v, 10); while (rs.endsWith("0") && !rs.endsWith(".0")) rs.remove(rs.length()-1); if (rs.endsWith(".")) rs.remove(rs.length()-1); }
        _history.push_back({flat, rs, false});
    }
    while ((int)_history.size() > MAX_HIST) _history.erase(_history.begin());
    resetInput(); _histBrowsing = false; _histSelIdx = -1; _redraw = true;
}

/* ═══════════════════════  Key handler  ═══════════════════════ */

void CalculationApp::handleKey(const KeyEvent &ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;
    if (_histBrowsing) {
        if (ev.code == KeyCode::ENTER) {
            if (_histSelIdx >= 0 && _histSelIdx < (int)_history.size()) {
                resetInput(); _root->text = _history[_histSelIdx].expression;
                _cursor.node = _root; _cursor.offset = _root->text.length(); _cursor.part = CursorPart::MAIN;
            }
            _histBrowsing = false; _histSelIdx = -1; _redraw = true; return;
        }
        if (ev.code != KeyCode::UP && ev.code != KeyCode::DOWN && ev.code != KeyCode::AC) {
            _histBrowsing = false; _histSelIdx = -1; _redraw = true;
        }
    }

    switch (ev.code) {
        case KeyCode::SHIFT: _shiftActive = !_shiftActive; _redraw = true; return;
        case KeyCode::LEFT:  cursorLeft();  return;
        case KeyCode::RIGHT: cursorRight(); return;
        case KeyCode::UP:    cursorUp();    return;
        case KeyCode::DOWN:  cursorDown();  return;
        case KeyCode::ENTER: evaluate();    return;
        case KeyCode::DEL:   deleteAtCursor(); return;
        case KeyCode::AC:    resetInput(); _history.clear(); _histBrowsing = false; _redraw = true; return; // Clear history on AC?

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
        case KeyCode::DOT:   insertChar('.'); break;

        case KeyCode::ADD: insertChar('+'); break;
        case KeyCode::SUB: insertChar('-'); break;
        case KeyCode::MUL: insertChar('*'); break;
        case KeyCode::POW: insertPower(); break; 
        case KeyCode::NEG: insertChar('-'); break;
        case KeyCode::LPAREN: insertChar('('); break;
        case KeyCode::RPAREN: insertChar(')'); break;
        case KeyCode::VAR_X: insertChar('x'); break;
        case KeyCode::VAR_Y: insertChar('y'); break;

        case KeyCode::DIV:
            if (_shiftActive) { insertFraction(); _shiftActive = false; }
            else insertChar('/');
            break;

        case KeyCode::SQRT:
            if (_shiftActive) { insertRoot(true); _shiftActive = false; }
            else insertRoot(false);
            break;

        case KeyCode::SIN: insertBlock("sin("); break;
        case KeyCode::COS: insertBlock("cos("); break;
        case KeyCode::TAN: insertBlock("tan("); break;

        default: break;
    }
    _shiftActive = false;
}

/* ═══════════════════════  Measuring  ═══════════════════════ */

CalculationApp::Metrics CalculationApp::measureChain(ExprNode* head, uint8_t size) {
    int cw = (size >= 2) ? 12 : 6;
    int ch = (size >= 2) ? 16 : 8;
    Metrics t = {0, ch/2, ch/2}; // Baseline centered
    
    for (ExprNode* n = head; n; n = n->next) {
        if (n->type == NodeType::TEXT) {
            t.w += n->text.length() * cw;
        } else if (n->type == NodeType::FRACTION) {
            Metrics nm = measureChain(n->numerator, size);     
            Metrics dm = measureChain(n->denominator, size);
            int fw = max(nm.w, dm.w) + 4 + (size*2);
            t.w += fw;
            t.above = max(t.above, 2 + nm.above + nm.below);
            t.below = max(t.below, 2 + dm.above + dm.below);
        } else if (n->type == NodeType::ROOT) {
            Metrics rm = measureChain(n->radicand, size);
            int hook = (size >= 2) ? 3 : 2;
            int rise = (size >= 2) ? 5 : 3;
            int lead = hook + rise + 2;
            int totalW = lead + rm.w + 4;
            if (n->rootIndex) {
                 Metrics im = measureChain(n->rootIndex, 1); 
                 totalW = max(totalW, lead + rm.w + im.w); 
            }
            t.w += totalW;
            t.above = max(t.above, rm.above + 4);
            t.below = max(t.below, rm.below); 
        } else if (n->type == NodeType::POWER) {
             uint8_t expSize = (size > 1) ? size - 1 : 1;
             Metrics em = measureChain(n->exponent, expSize); 
             t.w += em.w; 
             int lift = (size >= 2) ? 6 : 3;
             t.above = max(t.above, em.above + em.below + lift); 
        }
    }
    return t;
}

/* ═══════════════════════  Drawing  ═══════════════════════ */

void CalculationApp::drawRadical(int x, int axisY, int cW, int cAbove, int cBelow, bool hasIdx) {
    TFT_eSPI &tft = _display.tft();
    int top = axisY - cAbove - 2; 
    int bottom = axisY + cBelow;
    int hook = 3, rise = 5; 
    int ax = x, ay = axisY;
    int bx = x + hook, by = bottom;
    int cx = bx + rise, cy = top;
    tft.drawLine(ax, ay, bx, by, CBLK);
    tft.drawLine(bx, by, cx, cy, CBLK);
    tft.drawLine(bx+1, by, cx+1, cy, CBLK); 
    int vincR = cx + cW + 4;
    tft.fillRect(cx, top, vincR - cx, 2, CBLK);
    tft.drawLine(vincR, top, vincR, top + 4, CBLK);
}

void CalculationApp::drawChain(ExprNode* head, int x, int axisY, bool showCur, uint8_t size) {
    TFT_eSPI &tft = _display.tft();
    
    // Per user request: use setTextSize directly.
    tft.setTextSize(size);
    tft.setTextColor(CBLK, BG);

    int cw = (size >= 2) ? 12 : 6;
    int ch = (size >= 2) ? 16 : 8;

    for (ExprNode* n = head; n; n = n->next) {
        if (n->type == NodeType::TEXT) {
            int ty = axisY - ch/2;
            for (int i=0; i<(int)n->text.length(); i++) {
                 if (showCur && _cursor.node == n && _cursor.offset == i) tft.fillRect(x, ty, 2, ch, CBLK);
                 char b[2]={n->text.charAt(i),0}; tft.drawString(b, x, ty); x+=cw;
            }
            if (showCur && _cursor.node == n && _cursor.offset == (int)n->text.length()) tft.fillRect(x, ty, 2, ch, CBLK);
            
        } else if (n->type == NodeType::FRACTION) {
            Metrics nm = measureChain(n->numerator, size);
            Metrics dm = measureChain(n->denominator, size);
            int fw = max(nm.w, dm.w) + 4 + (size*2);
            tft.fillRect(x, axisY, fw, size, CBLK); 
            
            int numAx = axisY - 2 - nm.below;
            int numX  = x + (fw - nm.w) / 2;
            drawChain(n->numerator, numX, numAx, showCur, size);
            
            int denAx = axisY + 4 + dm.above;
            int denX  = x + (fw - dm.w) / 2;
            drawChain(n->denominator, denX, denAx, showCur, size);
            x += fw;
            
        } else if (n->type == NodeType::ROOT) {
            Metrics rm = measureChain(n->radicand, size);
            int lead = 10; 
            drawRadical(x, axisY, rm.w, rm.above, rm.below, n->rootIndex != nullptr);
            if (n->rootIndex) {
                 int idxX = x;
                 int idxAx = axisY - rm.above - 2;
                 drawChain(n->rootIndex, idxX, idxAx, showCur, 1);
            }
            drawChain(n->radicand, x + lead, axisY, showCur, size);
            x += lead + rm.w + 4;
            
        } else if (n->type == NodeType::POWER) {
            uint8_t expSize = (size > 1) ? size - 1 : 1;
            Metrics em = measureChain(n->exponent, expSize);
            int lift = (size >= 2) ? 6 : 3;
            int expAx = axisY - em.above/2 - lift;
            drawChain(n->exponent, x, expAx, showCur, expSize);
            x += em.w;
        }
    }
}

void CalculationApp::drawStatusBar() {
    TFT_eSPI &tft = _display.tft();
    tft.fillRect(0, 0, SW, BAR_H, 0xFFE0); 
    tft.setTextColor(CBLK, 0xFFE0);
    tft.setTextSize(1);
    tft.drawString("Calculation", 5, 5);
}

void CalculationApp::drawHistory() {
    TFT_eSPI &tft = _display.tft();
    if (_histBrowsing) {
        int y = BAR_H + 5;
        for (int i=0; i<(int)_history.size(); i++) {
             uint16_t c = (i == _histSelIdx) ? 0xF800 : CBLK;
             tft.setTextColor(c, BG);
             tft.drawString(_history[i].expression + " = " + _history[i].result, 10, y);
             y += 10;
        }
    }
}

void CalculationApp::drawInputArea() {
    int startY = BAR_H + 40; 
    drawChain(_root, 10, startY + 50, true, 2);
}

// ═════════════════════════════════════════════════
// RENDER (Main implementation)
// ═════════════════════════════════════════════════
void CalculationApp::render() {
    if (!_redraw) return;
    TFT_eSPI &tft = _display.tft();
    tft.fillScreen(BG);
    drawStatusBar();
    drawHistory();
    drawInputArea();
    _redraw = false;
}
