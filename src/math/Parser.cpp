/**
 * Parser.cpp
 * Basic shunting-yard implementation.
 *
 * NOTA: El manejo completo de unarios, funciones y ecuaciones
 * se ampliará en fases posteriores del MathEngine.
 */

#include "Parser.h"

#define CHECK_STACK(sp, n, msg) if ((sp) < (n)) { res.ok=false; res.errorMessage=msg; return res; }

int Parser::precedence(const Token &tok) const {
    if (tok.type != TokenType::Operator) return 0;
    switch (tok.op) {
        case OperatorKind::Plus:
        case OperatorKind::Minus:
            return 1;
        case OperatorKind::Mul:
        case OperatorKind::Div:
            return 2;
        case OperatorKind::Pow:
            return 3;
        default:
            return 0;
    }
}

bool Parser::isRightAssociative(const Token &tok) const {
    // Solo la potencia es asociativa a la derecha por ahora
    if (tok.type == TokenType::Operator && tok.op == OperatorKind::Pow) {
        return true;
    }
    return false;
}

bool Parser::isOperatorToken(const Token &tok) const {
    return tok.type == TokenType::Operator;
}

ParseResult Parser::toRPN(const std::vector<Token> &tokens) {
    ParseResult res;
    res.ok = false;
    res.outputRPN.clear();
    res.errorMessage = "";

    std::vector<Token> opStack;

    int parenBalance = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token &tok = tokens[i];

        if (tok.type == TokenType::End) {
            break;
        }

        switch (tok.type) {
            case TokenType::Number:
            case TokenType::Identifier:
                // valores van directo a la salida
                res.outputRPN.push_back(tok);
                break;

            case TokenType::Function:
                // Las funciones se comportan como operadores con alta precedencia,
                // pero se manejarán mejor cuando se añada la gestión explícita de argumentos.
                opStack.push_back(tok);
                break;

            case TokenType::Comma:
                // Separador de argumentos: sacar operadores hasta encontrar '('
                while (!opStack.empty() && opStack.back().type != TokenType::LParen) {
                    res.outputRPN.push_back(opStack.back());
                    opStack.pop_back();
                }
                if (opStack.empty()) {
                    res.errorMessage = "Separador de argumentos inesperado";
                    return res;
                }
                break;

             case TokenType::Operator: {
                 // Operadores binarios por ahora
                 while (!opStack.empty() && isOperatorToken(opStack.back())) {
                     const Token &top = opStack.back();
                     int precTop = precedence(top);
                     int precCur = precedence(tok);
                     if ((isRightAssociative(tok) && precCur < precTop) ||
                         (!isRightAssociative(tok) && precCur <= precTop)) {
                         res.outputRPN.push_back(top);
                         opStack.pop_back();
                     } else {
                         break;
                     }
                 }
                 opStack.push_back(tok);
                 break;
             }

            case TokenType::LParen:
                parenBalance++;
                opStack.push_back(tok);
                break;

            case TokenType::RParen: {
                parenBalance--;
                if (parenBalance < 0) {
                    res.errorMessage = "Falta paréntesis de apertura";
                    return res;
                }
                while (!opStack.empty() && opStack.back().type != TokenType::LParen) {
                    res.outputRPN.push_back(opStack.back());
                    opStack.pop_back();
                }
                if (opStack.empty()) {
                    res.errorMessage = "Falta paréntesis de apertura";
                    return res;
                }
                opStack.pop_back(); // quitar '('

                // Si en la cima ahora hay una Function, también va a la salida
                if (!opStack.empty() && opStack.back().type == TokenType::Function) {
                    res.outputRPN.push_back(opStack.back());
                    opStack.pop_back();
                }
                break;
            }

            case TokenType::Equal:
                // De momento dejamos '=' en la salida directamente, el EquationSolver
                // decidirá cómo partir LHS/RHS en modo Free Equation.
                res.outputRPN.push_back(tok);
                break;

            default:
                res.errorMessage = "Token inesperado en el parser";
                return res;
        }
    }

    if (parenBalance > 0) {
        res.errorMessage = "Falta paréntesis de cierre";
        return res;
    }

    // Vaciar la pila de operadores
    while (!opStack.empty()) {
        if (opStack.back().type == TokenType::LParen ||
            opStack.back().type == TokenType::RParen) {
            res.errorMessage = "Falta paréntesis de cierre";
            return res;
        }
        res.outputRPN.push_back(opStack.back());
        opStack.pop_back();
    }

    res.ok = true;
    return res;
}

