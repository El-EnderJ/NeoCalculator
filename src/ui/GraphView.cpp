/**
 * GraphView.cpp
 */

#include "GraphView.h"
#include <float.h>

GraphView::GraphView(DisplayDriver &display)
    : _display(display),
      _xMin(-10.0), _xMax(10.0),
      _yMin(-10.0), _yMax(10.0),
      _angleMode(AngleMode::DEG) {}

void GraphView::setView(double xMin, double xMax, double yMin, double yMax) {
    _xMin = xMin;
    _xMax = xMax;
    _yMin = yMin;
    _yMax = yMax;
}

void GraphView::zoom(double factor) {
    double xRange = _xMax - _xMin;
    double yRange = _yMax - _yMin;
    double xCenter = (_xMin + _xMax) / 2.0;
    double yCenter = (_yMin + _yMax) / 2.0;

    double newXHalf = (xRange * factor) / 2.0;
    double newYHalf = (yRange * factor) / 2.0;

    _xMin = xCenter - newXHalf;
    _xMax = xCenter + newXHalf;
    _yMin = yCenter - newYHalf;
    _yMax = yCenter + newYHalf;
}

void GraphView::autoScaleY(const String &expression, VariableContext &vars) {
    // 1. Tokenize & Parse expression once
    _evaluator.setAngleMode(_angleMode);
    
    // Check if expression contains 'x'
    if (expression.indexOf('x') < 0 && expression.indexOf('X') < 0) {
        return; // Constant or invalid
    }

    TokenizeResult tokRes = _tokenizer.tokenize(expression);
    if (!tokRes.ok) return;

    ParseResult parseRes = _parser.toRPN(tokRes.tokens);
    if (!parseRes.ok) return;

    // 2. Sample 30 points
    double minVal = DBL_MAX;
    double maxVal = -DBL_MAX;
    int samples = 30;
    double step = (_xMax - _xMin) / (samples - 1);
    
    bool validPoints = false;
    
    // Save original X value
    double originalX = 0;
    vars.getVariable('x', originalX);

    for (int i = 0; i < samples; ++i) {
        double x = _xMin + i * step;
        vars.setVariable('x', x);
        
        EvalResult res = _evaluator.evaluateRPN(parseRes.outputRPN, vars);
        if (res.ok && !isnan(res.value) && !isinf(res.value)) {
            if (res.value < minVal) minVal = res.value;
            if (res.value > maxVal) maxVal = res.value;
            validPoints = true;
        }
    }
    
    // Restore X
    vars.setVariable('x', originalX);

    if (validPoints) {
        // Apply margin
        double range = maxVal - minVal;
        if (range < 1e-9) range = 2.0; // Flat line case
        
        double margin = range * 0.1;
        _yMin = minVal - margin;
        _yMax = maxVal + margin;
    }
}

void GraphView::renderGraph(const String &expression, VariableContext &vars) {
    _display.clear(TFT_BLACK);
    
    // Prepare expression
    _evaluator.setAngleMode(_angleMode);
    TokenizeResult tokRes = _tokenizer.tokenize(expression);
    if (!tokRes.ok) {
        _display.setTextColor(TFT_RED);
        _display.drawText(0, 0, "Err Tok");
        return;
    }
    ParseResult parseRes = _parser.toRPN(tokRes.tokens);
    if (!parseRes.ok) {
        _display.setTextColor(TFT_RED);
        _display.drawText(0, 0, "Err Pars");
        return;
    }

    drawGridAndAxes();

    // Plot
    _display.tft().startWrite(); 
    // ^ Assuming TFT_eSPI, startWrite/endWrite optimizes SPI transaction
    // But DisplayDriver wraps it. We can use drawPixel, or access tft() if public.
    // DisplayDriver has tft() accessor.

    int screenW = _display.width();
    int screenH = _display.height();
    
    double originalX = 0;
    vars.getVariable('x', originalX);

    int prevPx = -1, prevPy = -1;

    for (int px = 0; px < screenW; ++px) {
        // Screen X -> World X
        double xNormal = (double)px / (double)(screenW - 1);
        double wx = _xMin + xNormal * (_xMax - _xMin);

        vars.setVariable('x', wx);
        EvalResult res = _evaluator.evaluateRPN(parseRes.outputRPN, vars);
        
        if (res.ok && !isnan(res.value) && !isinf(res.value)) {
            int py = worldToScreenY(res.value);
            
            // Clip drawing to screen
            // Simple line drawing between points
            if (prevPx >= 0) {
                 // Check if jump is too big (asymptote detection simple version)
                 if (abs(py - prevPy) < screenH) {
                     _display.drawLine(prevPx, prevPy, px, py, TFT_YELLOW);
                 }
            }
            // Point (optional, line is better)
            // _display.drawPixel(px, py, TFT_YELLOW);

            prevPx = px;
            prevPy = py;
        } else {
            prevPx = -1; // Gap in graph
        }
    }
    
    _display.tft().endWrite();
    vars.setVariable('x', originalX);
    
    // Draw Info
    _display.setTextColor(TFT_WHITE, TFT_BLACK);
    _display.setTextSize(1);
    _display.drawText(2, 2, "Y=");
    _display.drawText(20, 2, expression);
}

void GraphView::drawGridAndAxes() {
    int w = _display.width();
    int h = _display.height();

    // Axes
    // X Axis (y=0)
    if (_yMin <= 0 && _yMax >= 0) {
        int y0 = worldToScreenY(0.0);
        _display.drawLine(0, y0, w, y0, TFT_DARKGREY);
    }
    // Y Axis (x=0)
    if (_xMin <= 0 && _xMax >= 0) {
        int x0 = worldToScreenX(0.0);
        _display.drawLine(x0, 0, x0, h, TFT_DARKGREY);
    }
}

int GraphView::worldToScreenX(double x) const {
    double ratio = (x - _xMin) / (_xMax - _xMin);
    return (int)(ratio * (_display.width() - 1));
}

int GraphView::worldToScreenY(double y) const {
    // Invert Y because screen Y grows downwards
    double ratio = (y - _yMin) / (_yMax - _yMin);
    return (int)((1.0 - ratio) * (_display.height() - 1));
}
