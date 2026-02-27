/**
 * Parser.h
 * Shunting-yard based parser producing RPN from tokens.
 *
 * Precisión:
 *  - Usa los tokens generados por Tokenizer (doble precisión en números).
 *  - Gestiona precedencia: ^ > * / > + - (y soporte futuro para unarios).
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <vector>
#include "Tokenizer.h"

struct ParseResult {
    bool ok = false;
    String errorMessage;
    std::vector<Token> outputRPN; // tokens en notación polaca inversa
};

class Parser {
public:
    Parser() = default;

    // Convierte una lista de tokens en RPN usando shunting-yard.
    ParseResult toRPN(const std::vector<Token> &tokens);

private:
    int precedence(const Token &tok) const;
    bool isRightAssociative(const Token &tok) const;
    bool isOperatorToken(const Token &tok) const;
};

