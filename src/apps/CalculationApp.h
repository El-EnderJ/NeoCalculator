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

// Estructura para guardar cálculos pasados
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
    void render(); // El método maestro de dibujo

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
    bool _histBrowsing; // Si estamos navegando por el historial para editar
    int  _histSelIdx;   // Índice seleccionado
    static const int MAX_HIST = 50;

    /* ── Métricas y Layout ── */
    struct Metrics { int w, above, below; };
    
    // Constantes de diseño (se usan en el cpp)
    static const int HEADER_HEIGHT = 24;
    static const int INPUT_AREA_HEIGHT = 60; 

    /* ── Helpers del Motor Matemático (NO TOCAR LÓGICA) ── */
    void resetInput();
    void insertChar(char c);
    void insertBlock(const String &blk);
    void insertFraction();
    void insertRoot(bool withIndex);
    void insertPower();
    void deleteAtCursor();

    /* ── Movimiento del Cursor ── */
    void cursorLeft();
    void cursorRight();
    void cursorUp();
    void cursorDown();

    /* ── Búsqueda en Árbol ── */
    struct ParentInfo { ExprNode* node; CursorPart part; };
    ParentInfo findParent(ExprNode* head, ExprNode* target);
    ExprNode* lastTextInChain(ExprNode* head);

    /* ── Evaluación ── */
    String flattenChain(ExprNode* head);
    void   evaluate();

    /* ── Rutinas de Dibujo (Visualización Natural) ── */
    void drawHeader();
    void drawHistory();     // Dibuja la lista scrollable
    void drawInputArea();   // Dibuja la línea de edición actual
    
    // Motor recursivo de dibujo
    Metrics measureChain(ExprNode* head, uint8_t size);
    void    drawChain(ExprNode* head, int x, int axisY, bool showCur, uint8_t size);
    void    drawRadical(int x, int axisY, int cW, int cAbove, int cBelow);
};