/**
 * Tokenizer.h
 * Lexical analyzer for calculator expressions.
 *
 * Features:
 *  - Uses double precision for numeric literals.
 *  - Accepts `.1` and normaliza a `0.1`.
 *  - Interprets aislado `.` como `0` cuando va seguido de un operador.
 *  - Inserta multiplicación implícita: 2x, 2(x+1), (x+1)(x-1).
 *  - Distingue variables/identificadores (A..Z, x, y1, etc.) y funciones (ident seguidos de `(`).
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <vector>

enum class TokenType : uint8_t {
    End = 0,
    Number,
    Identifier,   // variable (x, A..Z, etc.)
    Function,     // function name (sin, cos, etc.)
    Operator,
    LParen,
    RParen,
    Comma,
    Equal
};

enum class OperatorKind : uint8_t {
    Plus = 0,
    Minus,
    Mul,
    Div,
    Pow
};

struct Token {
    TokenType type = TokenType::End;
    String    text;   // raw text (for identifiers/operators)
    double    value = 0.0;  // for Number
    OperatorKind op = OperatorKind::Plus; // for Operator
};

struct TokenizeResult {
    bool ok = false;
    String errorMessage;
    std::vector<Token> tokens;
};

class Tokenizer {
public:
    Tokenizer() = default;

    // Tokeniza una expresión completa. Agrega un token final de tipo End.
    TokenizeResult tokenize(const String &input);

private:
    String _input;
    int _len = 0;
    int _pos = 0;

    bool isAtEnd() const { return _pos >= _len; }
    char peek(int offset = 0) const;
    char peekNextNonSpace() const;
    char advance();
    void skipSpaces();

    bool isIdentStart(char c) const;
    bool isIdentPart(char c) const;

    bool readNumber(Token &outTok);
    bool readIdentifierOrFunction(Token &outTok);
    bool readOperatorOrSymbol(Token &outTok, String &error);

    bool isValueLike(const Token &t) const;
    bool isValueStartChar(char c) const;

    void appendImplicitMulIfNeeded(std::vector<Token> &out, const Token &next);
};

