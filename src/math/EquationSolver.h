/**
 * EquationSolver.h
 * Modo "Free Equation": resuelve LHS = RHS en términos de x.
 *
 * Estrategia:
 *  - Tokenizar y separar en LHS / RHS.
 *  - Construir f(x) = LHS - RHS.
 *  - Intentar detectar polinomios sencillos con coeficientes enteros y aplicar Ruffini.
 *  - En caso contrario, usar Newton-Raphson numérico sobre f(x).
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include "Tokenizer.h"
#include "Parser.h"
#include "Evaluator.h"
#include "VariableContext.h"
#include "StepLogger.h"

struct SolveResult {
    bool ok = false;
    String errorMessage;
    std::vector<double> roots;
};

class EquationSolver {
public:
    EquationSolver();

    void setAngleMode(AngleMode mode) { _angleMode = mode; }

    // Solve 2x2 System:
    // a1*x + b1*y = c1
    // a2*x + b2*y = c2
    struct System2x2 {
        double a1, b1, c1;
        double a2, b2, c2;
    };
    
    // Solve 3x3 System:
    // a1*x + b1*y + c1*z = d1
    // ...
    struct System3x3 {
        double a1, b1, c1, d1;
        double a2, b2, c2, d2;
        double a3, b3, c3, d3;
    };

    struct SystemResult {
        bool ok = false;
        String errorMessage;
        double x = 0, y = 0, z = 0;
        bool infiniteSolutions = false;
        bool noSolution = false;
    };

    SolveResult solveFreeEquation(const String &expr,
                                  VariableContext &vars,
                                  StepLogger *logger = nullptr);

    SystemResult solveSystem2x2(const System2x2 &sys, StepLogger *logger = nullptr);
    SystemResult solveSystem3x3(const System3x3 &sys, StepLogger *logger = nullptr);

private:
    Tokenizer _tokenizer;
    Parser _parser;
    Evaluator _evaluator;
    AngleMode _angleMode;

    // Helper for Polynomials
    struct PolyCoefs {
        double a=0, b=0, c=0, d=0, e=0; // Up to quartic: ax^4 + bx^3 + cx^2 + dx + e
        int degree = 0;
    };
    
    bool extractPolynomial(const std::vector<Token> &rpn, VariableContext &vars, PolyCoefs &outPoly);

    // Specific Solvers
    void solveLinear(double m, double n, SolveResult &out); // mx + n = 0
    void solveQuadratic(double a, double b, double c, SolveResult &out, StepLogger *logger);
    void solveCubic(double a, double b, double c, double d, SolveResult &out, StepLogger *logger);
    void solveBiquadratic(double a, double c, double e, SolveResult &out, StepLogger *logger); // ax^4 + cx^2 + e = 0

    // ... existing private methods if needed, or replace/refactor ...
    // Construye tokens de f(x) = LHS - RHS a partir de la lista tokenizada original.
    bool buildDifferenceTokens(const std::vector<Token> &tokens,
                               std::vector<Token> &outFx,
                               String &errorMessage,
                               StepLogger *logger,
                               const String &originalExpr);

    // Newton-Raphson numérico sobre f(x) dado en RPN.
    SolveResult solveWithNewton(const std::vector<Token> &rpn,
                                VariableContext &vars,
                                StepLogger *logger);
};

