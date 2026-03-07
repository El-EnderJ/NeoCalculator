/**
 * Evaluator.cpp
 */

#include "Evaluator.h"
#include <math.h>
#include <cmath>

Evaluator::Evaluator()
    : _angleMode(AngleMode::RAD) {}

bool Evaluator::push(double stack[], size_t &sp, double v) {
    if (sp >= STACK_MAX) {
        return false;
    }
    stack[sp++] = v;
    return true;
}

bool Evaluator::pop(double stack[], size_t &sp, double &out) {
    if (sp == 0) return false;
    out = stack[--sp];
    return true;
}

double Evaluator::toRadians(double x) const {
    if (_angleMode == AngleMode::RAD) return x;
    const double pi = 3.14159265358979323846;
    return x * (pi / 180.0);
}

bool Evaluator::resolveIdentifier(const String &name, VariableContext &vars, double &out, String &err) {
    // Constantes especiales
    if (name.equalsIgnoreCase("PI")) {
        out = 3.14159265358979323846;
        return true;
    }
    if (name.equalsIgnoreCase("E")) {
        out = 2.71828182845904523536;
        return true;
    }
    if (name.equalsIgnoreCase("ANS")) {
        out = vars.getAns();
        return true;
    }

    if (name.length() == 1) {
        char c = name.charAt(0);
        double val = 0.0;
        if (vars.getVariable(c, val)) {
            out = val;
            return true;
        }
        err = "Variable no definida: ";
        err += c;
        return false;
    }

    err = "Variable no definida: ";
    err += name;
    return false;
}

EvalResult Evaluator::evaluateRPN(const std::vector<Token> &rpn, VariableContext &vars) {
    EvalResult res;
    res.ok = false;
    res.errorMessage = "";
    res.value = 0.0;

    double stack[STACK_MAX];
    size_t sp = 0;

    for (size_t i = 0; i < rpn.size(); ++i) {
        const Token &t = rpn[i];
        if (t.type == TokenType::End) break;

        switch (t.type) {
            case TokenType::Number:
                if (!push(stack, sp, t.value)) {
                    res.errorMessage = "Error: pila llena";
                    return res;
                }
                break;

            case TokenType::Identifier: {
                double val = 0.0;
                String err;
                if (!resolveIdentifier(t.text, vars, val, err)) {
                    res.errorMessage = err;
                    return res;
                }
                if (!push(stack, sp, val)) {
                    res.errorMessage = "Error: pila llena";
                    return res;
                }
                break;
            }

            case TokenType::Function: {
                double a = 0.0;
                if (!pop(stack, sp, a)) {
                    res.errorMessage = "Operador sin operando previo";
                    return res;
                }

                double val = 0.0;
                String name = t.text;
                name.toLowerCase();

                if (name == "sin") {
                    val = sin(toRadians(a));
                } else if (name == "cos") {
                    val = cos(toRadians(a));
                } else if (name == "tan") {
                    {
                        double c = cos(toRadians(a));
                        if (fabs(c) < 1e-15) {
                            res.errorMessage = "Error: Dominio";
                            return res;
                        }
                    }
                    val = tan(toRadians(a));
                } else if (name == "sqrt") {
                    if (a < 0.0) {
                        res.errorMessage = "Error: Dominio";
                        return res;
                    }
                    val = sqrt(a);
                } else if (name == "log") {
                    if (a <= 0.0) {
                        res.errorMessage = "Error: Dominio";
                        return res;
                    }
                    val = log10(a);
                } else if (name == "ln") {
                    if (a <= 0.0) {
                        res.errorMessage = "Error: Dominio";
                        return res;
                    }
                    val = log(a);
                } else if (name == "abs") {
                    val = fabs(a);
                } else {
                    res.errorMessage = "Función no soportada: " + t.text;
                    return res;
                }

                // FPU sanitization: reject NaN / Inf produced by any function
                if (!std::isfinite(val)) {
                    res.errorMessage = "Error: Resultado no finito";
                    return res;
                }

                if (!push(stack, sp, val)) {
                    res.errorMessage = "Error: pila llena";
                    return res;
                }
                break;
            }

            case TokenType::Operator: {
                double b = 0.0;
                double a = 0.0;
                if (!pop(stack, sp, b) || !pop(stack, sp, a)) {
                    res.errorMessage = "Operador sin operando previo";
                    return res;
                }

                double val = 0.0;
                switch (t.op) {
                    case OperatorKind::Plus:
                        val = a + b;
                        break;
                    case OperatorKind::Minus:
                        val = a - b;
                        break;
                    case OperatorKind::Mul:
                        val = a * b;
                        break;
                    case OperatorKind::Div:
                        if (b == 0.0) {
                            res.errorMessage = "Error: Div por 0";
                            return res;
                        }
                        val = a / b;
                        break;
                    case OperatorKind::Pow:
                        // Manejo básico de dominio: 0^neg y raíces pares de negativos
                        if (a == 0.0 && b < 0.0) {
                            res.errorMessage = "Error: Div por 0";
                            return res;
                        }
                        val = pow(a, b);
                        break;
                    default:
                        res.errorMessage = "Operador desconocido";
                        return res;
                }

                // FPU sanitization: reject NaN / Inf from any operator
                if (!std::isfinite(val)) {
                    res.errorMessage = "Error: Resultado no finito";
                    return res;
                }

                if (!push(stack, sp, val)) {
                    res.errorMessage = "Error: pila llena";
                    return res;
                }
                break;
            }

            case TokenType::Equal:
                // Por ahora '=' se ignora en el evaluador simple. El EquationSolver
                // se encargará de ecuaciones LHS=RHS en modo Free Equation.
                break;

            default:
                res.errorMessage = "Token inesperado en evaluador";
                return res;
        }
    }

    if (sp != 1) {
        res.errorMessage = "Operador sin operando previo";
        return res;
    }

    // Final FPU sanitization: ensure the result is a valid finite number
    if (!std::isfinite(stack[0])) {
        res.errorMessage = "Error: Resultado no finito";
        return res;
    }

    res.value = stack[0];
    res.ok = true;
    return res;
}

