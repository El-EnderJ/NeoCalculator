/**
 * GraphView.h
 * Vista de graficadora 2D simple.
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include "../display/DisplayDriver.h"
#include "../math/Tokenizer.h"
#include "../math/Parser.h"
#include "../math/Evaluator.h"
#include "../math/VariableContext.h"

class GraphView {
public:
    GraphView(DisplayDriver &display);

    void setAngleMode(AngleMode mode) { _angleMode = mode; }

    void setView(double xMin, double xMax, double yMin, double yMax);
    void zoom(double factor); // factor <1: zoom in, >1: zoom out

    // Dibuja la función dada por 'expression' usando la variable x.
    void renderGraph(const String &expression, VariableContext &vars);
    void autoScaleY(const String &expression, VariableContext &vars);

private:
    DisplayDriver &_display;
    Tokenizer _tokenizer;
    Parser _parser;
    Evaluator _evaluator;

    double _xMin;
    double _xMax;
    double _yMin;
    double _yMax;
    AngleMode _angleMode;

    int worldToScreenX(double x) const;
    int worldToScreenY(double y) const;

    void drawGridAndAxes();
};

