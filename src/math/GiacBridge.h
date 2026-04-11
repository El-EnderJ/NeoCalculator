#ifndef GIAC_BRIDGE_H
#define GIAC_BRIDGE_H

#ifdef ARDUINO
#include <Arduino.h>
#include "giac/GiacBridge.h"
#endif
#include <string>

class GiacBridge {
public:
    // Compatibility adapter for app-side access.
    static bool begin() { return true; }
    
    // Envía una expresión y devuelve el resultado en texto.
    static std::string evaluate(const std::string& expression) {
#ifdef ARDUINO
        String in(expression.c_str());
        String out = solveWithGiac(in);
        return std::string(out.c_str());
#else
        (void)expression;
        return std::string("Error: Giac available only on Arduino target");
#endif
    }
};

#endif