/**
 * GrapherApp.cpp — Graphing Calculator Module
 * Tabs: Expressions, Graph, Table.
 * Uses GraphState struct.
 */
#include "GrapherApp.h"
#include "ui/Theme.h"
#include <math.h>

/* ── Constants ── */
static const int SW = 320, SH = 240;
static const int TAB_H = 20;

/* ── Colors ── */
static const uint16_t BG       = 0xFFFF;
static const uint16_t CBLK     = 0x0000;
static const uint16_t CTAB_ACT = 0xFEE0; // Active tab
static const uint16_t CTAB_PAS = 0xDEFB; // Passive tab

/* ═══════════════════════  Init  ═══════════════════════ */

GrapherApp::GrapherApp(DisplayDriver &disp, VariableContext &vars)
    : _display(disp), _vars(vars), _shiftActive(false), _redraw(true),
      _currentTab(GrapherTab::EXPRESSIONS),
      _selectedExprIdx(0), _exprEditMode(false)
{
    // Initialize standard view window
    _view.xMin = -10; _view.xMax = 10;
    _view.yMin = -10; _view.yMax = 10;
    
    // Init expressions
    for(int i=0; i<3; i++) _expressions[i] = "";
    _expressions[0] = "x^2"; // Default demo
}

GrapherApp::~GrapherApp() {}

void GrapherApp::begin() {
    _redraw = true;
    _shiftActive = false;
}

/* ═══════════════════════  Logic  ═══════════════════════ */

void GrapherApp::handleKey(const KeyEvent &ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    if (ev.code == KeyCode::SHIFT) { 
        _shiftActive = !_shiftActive; _redraw = true; return; 
    }

    // ── Global Tab Navigation (SHIFT + LEFT/RIGHT) ──
    if (_shiftActive && (ev.code == KeyCode::LEFT || ev.code == KeyCode::RIGHT)) {
        if (ev.code == KeyCode::LEFT) {
            // Cyclical Left
            if (_currentTab == GrapherTab::EXPRESSIONS) _currentTab = GrapherTab::TABLE;
            else if (_currentTab == GrapherTab::GRAPH) _currentTab = GrapherTab::EXPRESSIONS;
            else if (_currentTab == GrapherTab::TABLE) _currentTab = GrapherTab::GRAPH;
        } else {
            // Cyclical Right
            if (_currentTab == GrapherTab::EXPRESSIONS) _currentTab = GrapherTab::GRAPH;
            else if (_currentTab == GrapherTab::GRAPH) _currentTab = GrapherTab::TABLE;
            else _currentTab = GrapherTab::EXPRESSIONS;
        }
        _shiftActive = false; 
        _redraw = true; 
        return;
    }
    
    // ── Tab Specific Handling ──
    switch (_currentTab) {
        case GrapherTab::EXPRESSIONS: handleKeyExpressions(ev); break;
        case GrapherTab::GRAPH:       handleKeyGraph(ev); break;
        case GrapherTab::TABLE:       handleKeyTable(ev); break;
    }
}

void GrapherApp::handleKeyExpressions(const KeyEvent &ev) {
    if (_exprEditMode) {
        if (ev.code == KeyCode::ENTER) { _exprEditMode = false; _redraw = true; return; }
        if (ev.code == KeyCode::DEL) {
             if (_expressions[_selectedExprIdx].length() > 0) 
                 _expressions[_selectedExprIdx].remove(_expressions[_selectedExprIdx].length()-1);
             _redraw = true; return;
        }
        // Basic char input mapping
        char c = 0;
        // Fix for Enum Arithmetic: Cast to int before subtraction
        if (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9) {
             c = '0' + ((int)ev.code - (int)KeyCode::NUM_0);
        }
        else if (ev.code == KeyCode::VAR_X) c = 'x';
        else if (ev.code == KeyCode::ADD) c = '+';
        else if (ev.code == KeyCode::SUB || ev.code == KeyCode::NEG) c = '-';
        else if (ev.code == KeyCode::MUL) c = '*';
        else if (ev.code == KeyCode::DIV) c = '/';
        else if (ev.code == KeyCode::POW) c = '^';
        else if (ev.code == KeyCode::DOT) c = '.';
        
        if (c) { _expressions[_selectedExprIdx] += c; _redraw = true; }
    } else {
        // Navigation list
        if (ev.code == KeyCode::UP) {
            if (_selectedExprIdx > 0) _selectedExprIdx--;
            _redraw = true;
        } else if (ev.code == KeyCode::DOWN) {
            if (_selectedExprIdx < 2) _selectedExprIdx++;
            _redraw = true;
        } else if (ev.code == KeyCode::ENTER) {
            _exprEditMode = true; _redraw = true;
        }
    }
}

void GrapherApp::handleKeyGraph(const KeyEvent &ev) {
    // Zoom / Pan
    double rangeX = _view.xMax - _view.xMin;
    double rangeY = _view.yMax - _view.yMin;
    double stepX = rangeX * 0.1;
    double stepY = rangeY * 0.1;

    switch (ev.code) {
        case KeyCode::ADD: // Zoom In
            _view.xMin += stepX; _view.xMax -= stepX;
            _view.yMin += stepY; _view.yMax -= stepY;
            _redraw = true; break;
        case KeyCode::SUB: // Zoom Out
            _view.xMin -= stepX; _view.xMax += stepX;
            _view.yMin -= stepY; _view.yMax += stepY;
            _redraw = true; break;
        case KeyCode::LEFT:
            _view.xMin -= stepX; _view.xMax -= stepX; _redraw = true; break;
        case KeyCode::RIGHT:
            _view.xMin += stepX; _view.xMax += stepX; _redraw = true; break;
        case KeyCode::UP:
            _view.yMin += stepY; _view.yMax += stepY; _redraw = true; break;
        case KeyCode::DOWN:
            _view.yMin -= stepY; _view.yMax -= stepY; _redraw = true; break;
        default: break;
    }
}

void GrapherApp::handleKeyTable(const KeyEvent &ev) {
    // Scroll table?
}

/* ═══════════════════════  Rendering  ═══════════════════════ */

void GrapherApp::drawTabs() {
    TFT_eSPI &tft = _display.tft();
    int w = SW / 3;
    const char* titles[] = {"Expr", "Graph", "Table"};
    
    for (int i=0; i<3; i++) {
        uint16_t c = (i == (int)_currentTab) ? CTAB_ACT : CTAB_PAS;
        tft.fillRect(i*w, 0, w, TAB_H, c);
        tft.setTextColor(CBLK, c);
        tft.setTextSize(1);
        tft.drawString(titles[i], i*w + 10, 5);
        tft.drawRect(i*w, 0, w, TAB_H, CBLK);
    }
}

void GrapherApp::drawExpressions() {
    TFT_eSPI &tft = _display.tft();
    int y = TAB_H + 10;
    
    for (int i=0; i<3; i++) {
        if (i == _selectedExprIdx) {
            tft.fillRect(0, y, SW, 20, _exprEditMode ? 0xFFE0 : 0xDDD0);
        }
        tft.setTextColor(CBLK);
        String label = "Y" + String(i+1) + "=";
        tft.drawString(label + _expressions[i], 10, y + 5);
        y += 25;
    }
    
    // Instructions
    tft.setTextColor(0x5555);
    tft.drawString("ENTER to edit, SHIFT+ARROWS to tab", 10, SH - 20);
}

void GrapherApp::drawGraph() {
    TFT_eSPI &tft = _display.tft();
    
    // Axes mapping
    auto toScreenX = [&](double val) -> int {
        return (int)((val - _view.xMin) / (_view.xMax - _view.xMin) * SW);
    };
    auto mapY = [&](double val) -> int {
        double norm = (val - _view.yMin) / (_view.yMax - _view.yMin);
        return SH - (int)(norm * (SH - TAB_H));
    };

    // Draw Axes
    int origX = toScreenX(0);
    int origY = mapY(0);
    
    if (origX >= 0 && origX < SW) tft.drawLine(origX, TAB_H, origX, SH, CBLK);
    if (origY >= TAB_H && origY < SH) tft.drawLine(0, origY, SW, origY, CBLK);

    // Plot Y1 (first valid expression)
    if (_expressions[0].length() > 0) {
        Evaluator eval; Tokenizer tok; Parser par;
        // Optimization: Parse ONCE
        TokenizeResult tr = tok.tokenize(_expressions[0]);
        if (!tr.ok) return; // Invalid expression check
        ParseResult pr = par.toRPN(tr.tokens);
        if (!pr.ok) return;

        eval.setAngleMode(AngleMode::RAD); 
        
        int lastY = -1;
        for (int px = 0; px < SW; px += 2) {
            double worldX = _view.xMin + (double)px / SW * (_view.xMax - _view.xMin);
            _vars.setVar('x', worldX); 
            
            EvalResult res = eval.evaluateRPN(pr.outputRPN, _vars);
            
            if (res.ok) {
                int py = mapY(res.value);
                // Clip
                if (py >= TAB_H && py < SH) {
                    if (lastY != -1 && abs(py - lastY) < SH) { // Don't draw asymptotes
                         tft.drawLine(px-2, lastY, px, py, 0xF800); // Red
                    } else {
                         tft.drawPixel(px, py, 0xF800);
                    }
                    lastY = py;
                } else lastY = -1;
            } else lastY = -1;
        }
    }
}

void GrapherApp::drawTable() {
    TFT_eSPI &tft = _display.tft();
    tft.setTextColor(CBLK);
    tft.drawString("Table View (Not Implemented)", 20, 50);
}

// ═════════════════════════════════════════════════
// RENDER (Main implementation)
// ═════════════════════════════════════════════════
void GrapherApp::render() {
    if (!_redraw) return;
    TFT_eSPI &tft = _display.tft();
    tft.fillScreen(BG);
    drawTabs();
    
    switch (_currentTab) {
        case GrapherTab::EXPRESSIONS: drawExpressions(); break;
        case GrapherTab::GRAPH:       drawGraph(); break;
        case GrapherTab::TABLE:       drawTable(); break;
    }
    _redraw = false;
}
