/**
 * Tokenizer.cpp
 */

#include "Tokenizer.h"
#include <ctype.h>

char Tokenizer::peek(int offset) const {
    int idx = _pos + offset;
    if (idx < 0 || idx >= _len) return '\0';
    return _input.charAt(idx);
}

char Tokenizer::peekNextNonSpace() const {
    int i = _pos + 1;
    while (i < _len) {
        char c = _input.charAt(i);
        if (!isspace(static_cast<unsigned char>(c))) {
            return c;
        }
        ++i;
    }
    return '\0';
}

char Tokenizer::advance() {
    if (_pos >= _len) return '\0';
    return _input.charAt(_pos++);
}

void Tokenizer::skipSpaces() {
    while (!isAtEnd()) {
        char c = peek();
        if (!isspace(static_cast<unsigned char>(c))) break;
        ++_pos;
    }
}

bool Tokenizer::isIdentStart(char c) const {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool Tokenizer::isIdentPart(char c) const {
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

bool Tokenizer::isValueLike(const Token &t) const {
    return t.type == TokenType::Number ||
           t.type == TokenType::Identifier ||
           t.type == TokenType::RParen;
}

bool Tokenizer::isValueStartChar(char c) const {
    return isdigit(static_cast<unsigned char>(c)) ||
           c == '.' ||
           isIdentStart(c) ||
           c == '(';
}

bool Tokenizer::readNumber(Token &outTok) {
    // Special handling for leading '.'
    int start = _pos;
    String numStr;

    char c = peek();
    if (c == '.') {
        char nextNonSpace = peekNextNonSpace();
        if (isdigit(static_cast<unsigned char>(nextNonSpace))) {
            // Case ".1" (with or without space): treat as "0.1"
            numStr = "0.";
            // consume '.'
            advance();
            // consume optional spaces directly after '.'
            while (!isAtEnd() && isspace(static_cast<unsigned char>(peek()))) {
                advance();
            }
            // consume digits
            while (!isAtEnd() && isdigit(static_cast<unsigned char>(peek()))) {
                numStr += advance();
            }
        } else {
            // Isolated '.' → treat as 0
            advance(); // consume '.'
            outTok.type = TokenType::Number;
            outTok.text = "0";
            outTok.value = 0.0;
            return true;
        }
    } else {
        // Starts with digit
        while (!isAtEnd() && isdigit(static_cast<unsigned char>(peek()))) {
            numStr += advance();
        }
        // optional decimal part
        if (!isAtEnd() && peek() == '.') {
            numStr += advance();
            while (!isAtEnd() && isdigit(static_cast<unsigned char>(peek()))) {
                numStr += advance();
            }
        }
    }

    // Optional exponent part (e.g. 1e-3)
    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        numStr += advance(); // 'e' or 'E'
        if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
            numStr += advance();
        }
        bool hasExpDigits = false;
        while (!isAtEnd() && isdigit(static_cast<unsigned char>(peek()))) {
            hasExpDigits = true;
            numStr += advance();
        }
        if (!hasExpDigits) {
            // roll back exponent if invalid
            // for simplicidad aquí aceptamos como está; el parser/evaluador podría
            // tratarlo como error más adelante si se desea.
        }
    }

    if (numStr.length() == 0) {
        // No se leyó nada; no debería ocurrir
        return false;
    }

    outTok.type = TokenType::Number;
    outTok.text = numStr;
    outTok.value = numStr.toDouble(); // double precision
    return true;
}

bool Tokenizer::readIdentifierOrFunction(Token &outTok) {
    String ident;
    while (!isAtEnd() && isIdentPart(peek())) {
        ident += advance();
    }

    if (ident.length() == 0) return false;

    outTok.text = ident;

    // Look ahead: if next non-space char is '(', treat as function
    int savedPos = _pos;
    skipSpaces();
    char c = peek();
    _pos = savedPos; // restore, tokenizer no consume '(' aquí

    if (c == '(') {
        outTok.type = TokenType::Function;
    } else {
        outTok.type = TokenType::Identifier;
    }
    return true;
}

bool Tokenizer::readOperatorOrSymbol(Token &outTok, String &error) {
    char c = advance();
    switch (c) {
        case '+':
            outTok.type = TokenType::Operator;
            outTok.op = OperatorKind::Plus;
            outTok.text = "+";
            return true;
        case '-':
            outTok.type = TokenType::Operator;
            outTok.op = OperatorKind::Minus;
            outTok.text = "-";
            return true;
        case '*':
            outTok.type = TokenType::Operator;
            outTok.op = OperatorKind::Mul;
            outTok.text = "*";
            return true;
        case '/':
            outTok.type = TokenType::Operator;
            outTok.op = OperatorKind::Div;
            outTok.text = "/";
            return true;
        case '^':
            outTok.type = TokenType::Operator;
            outTok.op = OperatorKind::Pow;
            outTok.text = "^";
            return true;
        case '(':
            outTok.type = TokenType::LParen;
            outTok.text = "(";
            return true;
        case ')':
            outTok.type = TokenType::RParen;
            outTok.text = ")";
            return true;
        case ',':
            outTok.type = TokenType::Comma;
            outTok.text = ",";
            return true;
        case '=':
            outTok.type = TokenType::Equal;
            outTok.text = "=";
            return true;
        default:
            error = "Símbolo no reconocido: '";
            error += c;
            error += "'";
            return false;
    }
}

void Tokenizer::appendImplicitMulIfNeeded(std::vector<Token> &out, const Token &next) {
    if (out.empty()) {
        out.push_back(next);
        return;
    }

    const Token &prev = out.back();
    bool leftVal = isValueLike(prev) && prev.type != TokenType::Function;
    bool rightVal =
        next.type == TokenType::Number ||
        next.type == TokenType::Identifier ||
        next.type == TokenType::LParen;

    if (leftVal && rightVal) {
        Token mulTok;
        mulTok.type = TokenType::Operator;
        mulTok.op = OperatorKind::Mul;
        mulTok.text = "*";
        out.push_back(mulTok);
    }

    out.push_back(next);
}

TokenizeResult Tokenizer::tokenize(const String &input) {
    TokenizeResult result;
    result.ok = false;
    result.tokens.clear();
    result.errorMessage = "";

    _input = input;
    _len = _input.length();
    _pos = 0;

    while (!isAtEnd()) {
        skipSpaces();
        if (isAtEnd()) break;

        char c = peek();
        Token tok;

        if (isdigit(static_cast<unsigned char>(c)) || c == '.') {
            if (!readNumber(tok)) {
                result.errorMessage = "Error al leer número";
                return result;
            }
            appendImplicitMulIfNeeded(result.tokens, tok);
        } else if (isIdentStart(c)) {
            if (!readIdentifierOrFunction(tok)) {
                result.errorMessage = "Error al leer identificador";
                return result;
            }
            appendImplicitMulIfNeeded(result.tokens, tok);
        } else {
            String err;
            if (!readOperatorOrSymbol(tok, err)) {
                result.errorMessage = err;
                return result;
            }
            // Para operadores/paréntesis/igualdad no se aplica multiplicación implícita antes de saber el token
            appendImplicitMulIfNeeded(result.tokens, tok);
        }
    }

    // Append End token
    Token endTok;
    endTok.type = TokenType::End;
    endTok.text = "";
    endTok.value = 0.0;
    result.tokens.push_back(endTok);

    result.ok = true;
    return result;
}

