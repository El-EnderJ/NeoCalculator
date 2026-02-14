/**
 * CalculationApp.h — Natural V.P.A.M. calculator.
 * Fractions, square/nth roots, history with orange selection.
 */
#pragma once
#include <Arduino.h>
#include <vector>
#include "display/DisplayDriver.h"
#include "input/KeyMatrix.h"
#include "math/ExprNode.h"
#include "math/Tokenizer.h"
#include "math/Parser.h"
#include "math/Evaluator.h"
#include "math/VariableContext.h"

struct HistoryEntry {
    String expression;
    String result;
    bool   isError;
};

class CalculationApp {
public:
    CalculationApp(DisplayDriver &disp, VariableContext &vars);
    ~CalculationApp();

    void begin();
    void handleKey(const KeyEvent &ev);
    void render(); 

    void setAngleMode(AngleMode m) { _angleMode = m; }
    AngleMode angleMode() const    { return _angleMode; }

private:
    DisplayDriver  &_display;
    Tokenizer       _tokenizer;
    Parser          _parser;
    Evaluator       _evaluator;
    VariableContext &_vars;
    AngleMode       _angleMode;

    /* ── Input tree ── */
    ExprNode* _root;
    CursorPos _cursor;
    bool      _shiftActive;
    bool      _redraw;

    /* ── History ── */
    std::vector<HistoryEntry> _history;
    bool _histBrowsing;
    int  _histSelIdx;
    static const int MAX_HIST = 30;

    /* ── Metrics struct ── */
    struct Metrics { int w, above, below; };

    /* ── Node helpers ── */
    void resetInput();
    void insertChar(char c);
    void insertBlock(const String &blk);
    void insertFraction();
    void insertRoot(bool withIndex);
    void insertPower();
    void deleteAtCursor();

    /* ── Cursor ── */
    void cursorLeft();
    void cursorRight();
    void cursorUp();
    void cursorDown();

    /* ── Tree search ── */
    struct ParentInfo { ExprNode* node; CursorPart part; };
    ParentInfo findParent(ExprNode* head, ExprNode* target);
    ExprNode*  lastTextInChain(ExprNode* head);

    /* ── Evaluate ── */
    String flattenChain(ExprNode* head);
    void   evaluate();

    /* ── Rendering ── */
    void drawStatusBar();
    void drawHistory();
    void drawInputArea();
    Metrics measureChain(ExprNode* head, uint8_t size = 2);
    void    drawChain(ExprNode* head, int x, int axisY, bool showCur, uint8_t size = 2);
    void    drawRadical(int x, int axisY, int cW, int cAbove, int cBelow, bool hasIdx);
};
