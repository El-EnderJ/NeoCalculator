#include "GrapherApp.h"
#include "../input/KeyCodes.h"

// Colors (NumWorks Grapher - Clean Gray Theme)
#define COL_TAB_BAR     0x4208 // Dark Gray
#define COL_TAB_ACTIVE  0xFFFF // White
#define COL_TAB_TEXT_A  0x0000 // Black (Active)
#define COL_TAB_TEXT_I  0xFFFF // White (Inactive)
#define COL_BG          0xFFFF // White
#define COL_AXIS        0x0000 // Black
#define COL_GRID        0xE71C // Light Gray
#define COL_EXPR_SEL    0xE71C // Selection Light Gray

using namespace std;

GrapherApp::GrapherApp(DisplayDriver &disp, VariableContext &vars)
    : _display(disp), _vars(vars), _evaluator(), _parser(), _tokenizer(),
      _currentTab(GrapherTab::EXPRESSIONS), _selectedExprIdx(0), _exprEditMode(false),
      _shiftActive(false), _redraw(true),
      _offsetX(0.0f), _offsetY(0.0f), _scaleX(20.0f), _scaleY(20.0f) // Pixels per unit
{
    // Init Expressions
    for(int i=0; i<3; i++) {
        _expressions[i].enabled = false;
        _expressions[i].expression = "";
        // NumWorks Colors: Red, Blue, Green
        if (i==0) _expressions[i].color = 0xF800; // Red
        if (i==1) _expressions[i].color = 0x001F; // Blue
        if (i==2) _expressions[i].color = 0x07E0; // Green
    }
}

GrapherApp::~GrapherApp() {}

void GrapherApp::begin() {
    _redraw = true;
    _display.clear(COL_BG);
}

void GrapherApp::handleKey(const KeyEvent &ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    _redraw = true;

    // Tab Navigation (Back key or Mode)
    // NOTE: In NumWorks, usually UP goes to tabs. 
    // Here we'll use KEY_BACK to cycle or focus tabs for simplicity
    if (ev.code == KeyCode::MODE) { // Assuming there's a MODE key or similar
        // Cycle tabs
        int t = (int)_currentTab;
        t = (t + 1) % 3;
        _currentTab = (GrapherTab)t;
        return;
    }

    switch(_currentTab) {
        case GrapherTab::EXPRESSIONS: handleKeyExpressions(ev); break;
        case GrapherTab::GRAPH:       handleKeyGraph(ev); break;
        case GrapherTab::TABLE:       handleKeyTable(ev); break;
    }
}

void GrapherApp::handleKeyExpressions(const KeyEvent &ev) {
    if (_exprEditMode) {
        // Typing mode
        if (ev.code == KeyCode::ENTER) {
            _exprEditMode = false;
            // Parse/Validate?
            return;
        }
        if (ev.code == KeyCode::DEL) {
            String &s = _expressions[_selectedExprIdx].expression;
            if (s.length() > 0) s.remove(s.length()-1);
        }
        // Insert Char
        // Use a mini mapper or assume a helper exists. 
        // For brevity, replicating char map logic:
        char c = 0;
        switch (ev.code) {
            case KeyCode::NUM_0: c='0'; break;
            case KeyCode::NUM_1: c='1'; break;
            case KeyCode::NUM_2: c='2'; break;
            case KeyCode::NUM_3: c='3'; break;
            case KeyCode::NUM_4: c='4'; break;
            case KeyCode::NUM_5: c='5'; break;
            case KeyCode::NUM_6: c='6'; break;
            case KeyCode::NUM_7: c='7'; break;
            case KeyCode::NUM_8: c='8'; break;
            case KeyCode::NUM_9: c='9'; break;
            case KeyCode::VAR_X: c='x'; break;
            case KeyCode::ADD:   c='+'; break;
            case KeyCode::SUB:   c='-'; break;
            case KeyCode::MUL:   c='*'; break;
            case KeyCode::DIV:   c='/'; break;
            case KeyCode::POW:   c='^'; break;
            default: break;
        }
        if (c) _expressions[_selectedExprIdx].expression += c;
        
        // Enable if not empty
        if (_expressions[_selectedExprIdx].expression.length() > 0)
             _expressions[_selectedExprIdx].enabled = true;

    } else {
        // Navigation Mode
        if (ev.code == KeyCode::DOWN) {
            _selectedExprIdx = (_selectedExprIdx + 1) % 3;
        }
        if (ev.code == KeyCode::UP) {
            _selectedExprIdx = (_selectedExprIdx + 2) % 3; // -1 wrapping
        }
        if (ev.code == KeyCode::ENTER || ev.code == KeyCode::RIGHT) {
            _exprEditMode = true;
        }
        if (ev.code == KeyCode::RIGHT) {
            // Go to Graph?
            _currentTab = GrapherTab::GRAPH;
        }
    }
}

void GrapherApp::handleKeyGraph(const KeyEvent &ev) {
    // Pan & Zoom
    float step = 10.0f / _scaleX; // move 10 pixels worth
    
    if (ev.code == KeyCode::LEFT)  _offsetX -= step;
    if (ev.code == KeyCode::RIGHT) _offsetX += step;
    if (ev.code == KeyCode::UP)    _offsetY += step;
    if (ev.code == KeyCode::DOWN)  _offsetY -= step;
    
    if (ev.code == KeyCode::ADD) { _scaleX *= 1.2f; _scaleY *= 1.2f; }
    if (ev.code == KeyCode::SUB) { _scaleX /= 1.2f; _scaleY /= 1.2f; }
    
    // Go back to expressions
    if (ev.code == KeyCode::DEL || ev.code == KeyCode::AC) {
        _currentTab = GrapherTab::EXPRESSIONS;
    }
}

void GrapherApp::handleKeyTable(const KeyEvent &ev) {
    // Placeholder
      if (ev.code == KeyCode::DEL) _currentTab = GrapherTab::GRAPH;
}

/* ─────────────────────────────────────────────────────────────────────────────
   RENDERING
   ───────────────────────────────────────────────────────────────────────────── */

void GrapherApp::render() {
    if (!_redraw) return;
    _redraw = false;

    drawTabs();

    switch(_currentTab) {
        case GrapherTab::EXPRESSIONS: drawExpressions(); break;
        case GrapherTab::GRAPH:       drawGraph(); break;
        case GrapherTab::TABLE:       drawTable(); break;
    }
}

void GrapherApp::drawTabs() {
    int w = _display.width();
    int h = 30; // Tab height
    int tabW = w / 3;

    // Background Bar
    _display.fillRect(0, 0, w, h, COL_TAB_BAR);

    const char* titles[] = {"Expressions", "Graph", "Table"};
    
    for (int i=0; i<3; i++) {
        bool active = ((int)_currentTab == i);
        int x = i * tabW;
        
        if (active) {
            _display.fillRect(x, 0, tabW, h, COL_TAB_ACTIVE);
            _display.setTextColor(COL_TAB_TEXT_A, COL_TAB_ACTIVE);
        } else {
            _display.setTextColor(COL_TAB_TEXT_I, COL_TAB_BAR);
        }
        
        // Draw Text Centered
        _display.setTextSize(1);
        String t = titles[i];
        int tw = t.length() * 6;
        _display.drawText(x + (tabW - tw)/2, 10, t);
        
        // Separator
        if (i < 2) _display.drawLine(x + tabW, 5, x + tabW, h-5, 0xFFFF);
    }
}

void GrapherApp::drawExpressions() {
    int w = _display.width();
    int h = _display.height();
    int yStart = 35;
    
    _display.fillRect(0, yStart, w, h-yStart, COL_BG);

    for (int i=0; i<3; i++) {
        int y = yStart + i * 40;
        bool selected = (i == _selectedExprIdx);
        
        if (selected) {
            _display.fillRect(0, y, w, 40, COL_EXPR_SEL);
        }

        // Colored Dot
        _display.fillRect(10, y+10, 10, 20, _expressions[i].color); 
        // We could verify this is a circle if we had drawCircle

        // Text "f(x) ="
        _display.setTextSize(2);
        _display.setTextColor(0x0000, selected ? COL_EXPR_SEL : COL_BG);
        
        String label = (i==0 ? "f(x)=" : (i==1 ? "g(x)=" : "h(x)="));
        _display.drawText(30, y+12, label + _expressions[i].expression);
        
        if (selected && _exprEditMode) {
             // Blinking cursor visual could go here
             _display.drawText(w-20, y+12, "_");
        }
    }
}

double GrapherApp::evaluateFunction(int funcIdx, double x) {
    if (!_expressions[funcIdx].enabled) return 0;
    // Set variable "x"
    _vars.setVar('x', x);
    // Parse & Eval
    // Note: Creating parser/tokenizer every pixel is slow, 
    // but optimized for code simplicity here.
    // Ideally we cache the AST.
    
    // Simple eval wrapper:
    String expr = _expressions[funcIdx].expression;
    if (expr.length() == 0) return 0;
    
    // We assume the Evaluator can handle "x" variable
    // We need to re-parse the string each time or keep ASTs.
    // Given the class structure, we'll re-parse (slow but safe).
    TokenizeResult tokRes = _tokenizer.tokenize(expr);
    if (!tokRes.ok) return 0;

    ParseResult parseRes = _parser.toRPN(tokRes.tokens);
    if (!parseRes.ok) return 0;

    EvalResult evalRes = _evaluator.evaluateRPN(parseRes.outputRPN, _vars);
    if (evalRes.ok) {
        return evalRes.value;
    }
    return 0;
}

void GrapherApp::drawGraph() {
    int w = _display.width();
    int h = _display.height();
    int top = 30; // Tab Bar offset

    _display.fillRect(0, top, w, h-top, COL_BG);

    // Center in pixels
    int cx = w / 2;
    int cy = top + (h - top) / 2;

    // Shift by offset (convert offset units to pixels)
    // _offsetX is center position in World Units
    // We want (0,0) to be at (cx, cy) if _offsetX=0
    
    // Pixel = CenterPixel + (World - Offset) * Scale
    auto worldToScreenX = [&](float val) -> int {
        return cx + (val - _offsetX) * _scaleX;
    };
    auto worldToScreenY = [&](float val) -> int {
        return cy - (val - _offsetY) * _scaleY; // Y inverted on screen
    };
    auto screenToWorldX = [&](int px) -> float {
        return _offsetX + (px - cx) / _scaleX;
    };

    // Draw Axes
    int axisX_Screen = worldToScreenY(0);
    int axisY_Screen = worldToScreenX(0);

    if (axisX_Screen >= top && axisX_Screen < h) 
        _display.drawLine(0, axisX_Screen, w, axisX_Screen, COL_AXIS); // X-Axis
    
    if (axisY_Screen >= 0 && axisY_Screen < w) 
        _display.drawLine(axisY_Screen, top, axisY_Screen, h, COL_AXIS); // Y-Axis

    // Plot Functions
    for (int i=0; i<3; i++) {
        if (!_expressions[i].enabled) continue;
        
        uint16_t col = _expressions[i].color;
        int lastPy = -1;
        
        // Scan X pixels
        for (int px = 0; px < w; px += 2) { // Step 2 for speed
            float worldX = screenToWorldX(px);
            double worldY = evaluateFunction(i, worldX);
            
            // Check for NaN or Inf
            if (isnan(worldY) || isinf(worldY)) {
                lastPy = -1;
                continue;
            }

            int py = worldToScreenY((float)worldY);
            
            // Clip
            if (py < top) py = top - 1; 
            if (py >= h) py = h;

            if (lastPy != -1 && abs(py - lastPy) < h) {
                _display.drawLine(px-2, lastPy, px, py, col);
            }
            lastPy = py;
        }
    }
}

void GrapherApp::drawTable() {
    int w = _display.width();
    int h = _display.height();
    _display.fillRect(0, 30, w, h-30, COL_BG);
    _display.setTextColor(0x0000);
    _display.drawText(20, 60, "Table View (TODO)");
}
