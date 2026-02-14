/**
 * EquationSolver.cpp
 */

#include "EquationSolver.h"
#include <math.h>
#include <stdlib.h> // for abs

EquationSolver::EquationSolver()
    : _tokenizer(), _parser(), _evaluator(), _angleMode(AngleMode::DEG) {}

SolveResult EquationSolver::solveFreeEquation(const String &expr,
                                              VariableContext &vars,
                                              StepLogger *logger) {
    SolveResult result;
    if (logger) logger->clear();

    // 1. Parse LHS = RHS
    int eqPos = expr.indexOf('=');
    if (eqPos < 0) {
        result.errorMessage = "Falta '=' en la ecuacion";
        return result;
    }

    // Reuse parsing logic to get tokens
    TokenizeResult tokRes = _tokenizer.tokenize(expr);
    if (!tokRes.ok) {
        result.errorMessage = tokRes.errorMessage;
        return result;
    }

    std::vector<Token> rpnFx;
    if (!buildDifferenceTokens(tokRes.tokens, rpnFx, result.errorMessage, logger, expr)) {
        return result;
    }

    // 2. Try to extract Polynomial Coefficients
    // This is a heuristic. It attempts to simplify the RPN into a polynomial form ax^n + ...
    PolyCoefs poly;
    if (extractPolynomial(rpnFx, vars, poly)) {
        if (poly.degree == 1) {
            solveLinear(poly.d, poly.e, result); // dx + e = 0
            return result;
        } else if (poly.degree == 2) {
            solveQuadratic(poly.c, poly.d, poly.e, result, logger);
            return result;
        } else if (poly.degree == 3) {
            solveCubic(poly.b, poly.c, poly.d, poly.e, result, logger);
            return result;
        } else if (poly.degree == 4) {
             // Check for Biquadratic: ax^4 + cx^2 + e = 0 (b=0, d=0)
             if (abs(poly.b) < 1e-9 && abs(poly.d) < 1e-9) {
                 solveBiquadratic(poly.a, poly.c, poly.e, result, logger);
                 return result;
             }
        }
    }

    // 3. Fallback: Numerical Newton-Raphson
    if (logger) logger->addStep("No es un polinomio simple detectado.", "Usando metodo numerico");
    return solveWithNewton(rpnFx, vars, logger);
}

// --- Solvers Specíficos ---

void EquationSolver::solveLinear(double m, double n, SolveResult &out) {
    // mx + n = 0 -> x = -n/m
    out.ok = true;
    if (abs(m) < 1e-12) {
        if (abs(n) < 1e-12) {
            out.errorMessage = "Infinitas Soluciones"; // 0=0
            out.ok = false; 
        } else {
            out.errorMessage = "Sin Solucion"; // 0=5
            out.ok = false;
        }
    } else {
        out.roots.push_back(-n / m);
    }
}

void EquationSolver::solveQuadratic(double a, double b, double c, SolveResult &out, StepLogger *logger) {
    // ax^2 + bx + c = 0
    if (logger) {
        String s = String(a) + "x^2 + " + String(b) + "x + " + String(c) + " = 0";
        logger->addStep("Ecuacion Cuadratica", s);
    }

    double disc = b*b - 4*a*c;
    out.ok = true;

    if (disc < 0) {
        out.errorMessage = "Raices complejas"; // Simple calc only real
        // out.ok = false; 
        // Or return empty roots for "No real solution"
    } else if (abs(disc) < 1e-12) {
        double x = -b / (2*a);
        out.roots.push_back(x);
        if (logger) logger->addStep("Discriminante 0", "Raiz unica: " + String(x));
    } else {
        double sqrtD = sqrt(disc);
        double x1 = (-b + sqrtD) / (2*a);
        double x2 = (-b - sqrtD) / (2*a);
        out.roots.push_back(x1);
        out.roots.push_back(x2);
        if (logger) logger->addStep("Discriminante > 0", "x1=" + String(x1) + ", x2=" + String(x2));
    }
}

void EquationSolver::solveBiquadratic(double a, double c, double e, SolveResult &out, StepLogger *logger) {
    // ax^4 + cx^2 + e = 0. let u = x^2. -> au^2 + cu + e = 0
    if (logger) {
        logger->addStep("Biquadrada", "Cambio variable u = x^2");
    }

    SolveResult quadRes;
    solveQuadratic(a, c, e, quadRes, logger);

    if (!quadRes.ok || quadRes.roots.empty()) {
        out.ok = true; // No real roots is a valid result state
        out.errorMessage = "Sin sol. real (u<0)";
        return;
    }

    out.ok = true;
    for (double u : quadRes.roots) {
        if (u < 0) continue; // x^2 = negative -> complex
        if (abs(u) < 1e-12) {
            out.roots.push_back(0);
        } else {
            double sq = sqrt(u);
            out.roots.push_back(sq);
            out.roots.push_back(-sq);
        }
    }
    
    if (logger) {
        String s = "Raices finales: ";
        for (double r : out.roots) s += String(r) + " ";
        logger->addStep("Deshacer cambio u=x^2", s);
    }
}

void EquationSolver::solveCubic(double a, double b, double c, double d, SolveResult &out, StepLogger *logger) {
    // ax^3 + bx^2 + cx + d = 0
    // Try integer roots (Ruffini) divisors of d/a
    if (logger) logger->addStep("Cubica detected", "Buscando raiz entera (Ruffini)");

    // Simple heuristic: check integers from -10 to 10.
    // Real implementation should check divisors of 'd'.
    double foundRoot = NAN;
    bool found = false;

    // Range to search for integer root
    for (int r = -20; r <= 20; ++r) {
        if (r == 0 && abs(d) > 1e-9) continue; // x=0 only if d=0
        
        // Evaluate P(r)
        double val = a*r*r*r + b*r*r + c*r + d;
        if (abs(val) < 1e-7) {
            foundRoot = (double)r;
            found = true;
            break;
        }
    }

    if (!found) {
        // Fallback to Newton for all roots or Cardan (too complex?)
        // Let's just use Newton for one solution currently if no integer found.
        if (logger) logger->addStep("Ruffini fallo", "Usando metodo numerico");
        // TODO: Could call solveWithNewton here, but we need to reconstruct the RPN? 
        // Logic gap - simplification: just return "Use Graph" or specific Newton
        out.errorMessage = "Raiz entera no hallada";
        // out.ok = false;
        return; 
    }

    out.ok = true;
    out.roots.push_back(foundRoot);
    if (logger) logger->addStep("Encontrada Raiz", "x1 = " + String(foundRoot));

    // Synthetic division (Ruffini) to get Quadratic
    // (ax^3 + bx^2 + cx + d) / (x - r) = qx^2 + rx + s
    // newA = a
    // newB = b + r*a
    // newC = c + r*newB
    double newA = a;
    double newB = b + foundRoot * newA;
    double newC = c + foundRoot * newB;
    // Remainder should be ~0

    // Solve resulting quadratic
    SolveResult qRes;
    solveQuadratic(newA, newB, newC, qRes, logger);
    if (qRes.ok) {
        for (double v : qRes.roots) {
            out.roots.push_back(v);
        }
    }
}

// --- Linear Systems ---

EquationSolver::SystemResult EquationSolver::solveSystem2x2(const System2x2 &sys, StepLogger *logger) {
    // Cramer's Rule
    // D = a1*b2 - a2*b1
    double D = sys.a1 * sys.b2 - sys.a2 * sys.b1;
    double Dx = sys.c1 * sys.b2 - sys.c2 * sys.b1;
    double Dy = sys.a1 * sys.c2 - sys.a2 * sys.c1;

    SystemResult res;
    
    if (abs(D) < 1e-12) {
        if (abs(Dx) < 1e-12 && abs(Dy) < 1e-12) {
            res.infiniteSolutions = true;
            res.errorMessage = "Infinitas Sol";
        } else {
            res.noSolution = true;
            res.errorMessage = "Sin Solucion";
        }
        res.ok = false;
    } else {
        res.x = Dx / D;
        res.y = Dy / D;
        res.ok = true;
        
        if (logger) {
            logger->clear();
            logger->addStep("Metodo Cramer", "Det=" + String(D));
            logger->addStep("Solucion", "x=" + String(res.x) + ", y=" + String(res.y));
        }
    }
    return res;
}

EquationSolver::SystemResult EquationSolver::solveSystem3x3(const System3x3 &sys, StepLogger *logger) {
    // Cramer's for 3x3
    // Det(A)
    double D = sys.a1*(sys.b2*sys.c3 - sys.b3*sys.c2) -
               sys.b1*(sys.a2*sys.c3 - sys.a3*sys.c2) +
               sys.c1*(sys.a2*sys.b3 - sys.a3*sys.b2);

    SystemResult res;
    if (abs(D) < 1e-12) {
        res.ok = false;
        res.errorMessage = "Det=0 (Sin/Inf Sol)";
        return res;
    }

    // Dx replace col 1 with d
    double Dx = sys.d1*(sys.b2*sys.c3 - sys.b3*sys.c2) -
                sys.b1*(sys.d2*sys.c3 - sys.d3*sys.c2) +
                sys.c1*(sys.d2*sys.b3 - sys.d3*sys.b2);

    // Dy replace col 2 with d
    double Dy = sys.a1*(sys.d2*sys.c3 - sys.d3*sys.c2) -
                sys.d1*(sys.a2*sys.c3 - sys.a3*sys.c2) +
                sys.c1*(sys.a2*sys.d3 - sys.a3*sys.d2);
    
    // Dz replace col 3 with d
    double Dz = sys.a1*(sys.b2*sys.d3 - sys.b3*sys.d2) -
                sys.b1*(sys.a2*sys.d3 - sys.a3*sys.d2) +
                sys.d1*(sys.a2*sys.b3 - sys.a3*sys.b2);

    res.x = Dx / D;
    res.y = Dy / D;
    res.z = Dz / D;
    res.ok = true;
    
    if (logger) {
       logger->clear();
       logger->addStep("Metodo Cramer 3x3", "Det=" + String(D));
    }

    return res;
}

// --- Internal Utilities ---

bool EquationSolver::buildDifferenceTokens(const std::vector<Token> &tokens,
                                           std::vector<Token> &outFx,
                                           String &errorMessage,
                                           StepLogger *logger,
                                           const String &originalExpr) {
    // Implementation to convert LHS=RHS to LHS - (RHS)
    // Find =
    int splitIdx = -1;
    for (int i=0; i<(int)tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Equal) {
            splitIdx = i;
            break;
        }
    }
    if (splitIdx == -1) return false;

    // LHS
    for (int i=0; i<splitIdx; ++i) outFx.push_back(tokens[i]);
    
    // Minus
    Token minusTok; 
    minusTok.type = TokenType::Operator; 
    minusTok.op = OperatorKind::Minus; 
    minusTok.text = "-";
    outFx.push_back(minusTok);

    // ( RHS )
    Token lp; lp.type = TokenType::LParen; lp.text="(";
    outFx.push_back(lp);

    for (int i=splitIdx+1; i<(int)tokens.size(); ++i) {
        if (tokens[i].type != TokenType::End) {
             outFx.push_back(tokens[i]);
        }
    }
    Token rp; rp.type = TokenType::RParen; rp.text=")";
    outFx.push_back(rp);

    // End
    Token end; end.type = TokenType::End;
    outFx.push_back(end);

    return true;
}

// Heuristic Polynomial Extractor
// Very simplified: assumes fully expanded form like "2x^2 + 5x - 3" or similar arithmetic construction
// Actually constructing a CAS to normalize to a polynomial is hard. 
// We will simply test the tree by evaluating at specific points!
// If f(x) is polynomial degree N, then (N+1) points define it.
// We can solve for coefficients using minimal points, then verify with one more.

bool EquationSolver::extractPolynomial(const std::vector<Token> &rpn, VariableContext &vars, PolyCoefs &outPoly) {
    // We check for degrees 0 to 4.
    // Evaluate f(x) at x = -2, -1, 0, 1, 2
    // Use Lagrange Interpolation or Regression to find coeffs?
    // Actually simpler: 
    // f(0) = e
    // f(1) = a+b+c+d+e
    // f(-1) = a-b+c-d+e
    // ...
    // Let's implement this "Black Box" identification.
    
    _evaluator.setAngleMode(_angleMode);
    
    double y[5]; // x = -2, -1, 0, 1, 2
    double xVal[] = {-2, -1, 0, 1, 2};
    
    // Save x
    double originalX = 0; vars.getVariable('x', originalX);

    ParseResult pr; 
    pr.outputRPN = rpn; // copy?

    for (int i=0; i<5; ++i) {
        vars.setVariable('x', xVal[i]);
        EvalResult r = _evaluator.evaluateRPN(rpn, vars);
        if (!r.ok) {
             vars.setVariable('x', originalX);
             return false; 
        }
        y[i] = r.value;
    }
    vars.setVariable('x', originalX);

    // Calculate Finite Differences or Solve System?
    // Let's try to fit Linear first
    // y = d*x + e
    // Check if consistent slope
    double slope1 = y[3] - y[2]; // f(1)-f(0)
    double slope2 = y[2] - y[1]; // f(0)-f(-1)
    if (abs(slope1 - slope2) < 1e-9) {
        outPoly.degree = 1;
        outPoly.d = slope1;
        outPoly.e = y[2]; // f(0)
        return true;
    }

    // Quadratic: c*x^2 + d*x + e
    // 2nd diff const
    double d2_1 = slope1 - slope2;
    double slope3 = y[1] - y[0];
    double d2_2 = slope2 - slope3;
    if (abs(d2_1 - d2_2) < 1e-9) {
        outPoly.degree = 2;
        // 2c = second diff
        outPoly.c = d2_1 / 2.0;
        outPoly.e = y[2]; 
        // f(1) = c + d + e -> d = f(1) - c - e
        outPoly.d = y[3] - outPoly.c - outPoly.e;
        return true;
    }

    // Cubic
    // 3rd diff const
    double d3_1 = d2_1 - d2_2;
    // Need more points for robust check? 5 pts is enough for degree 4.
    // ... (This logic can expand for cubic/quartic).
    // Let's implement generic polynomial fit for degree 4?
    // Or just finish here for Quad/Linear which is most common "Auto" solve.
    // The requirement says "Cubic Ruffini" and "Biquadratic".
    
    // Check Biquadratic symmetry f(x) = f(-x)
    // f(1)=f(-1), f(2)=f(-2)
    if (abs(y[3] - y[1]) < 1e-9 && abs(y[4] - y[0]) < 1e-9) {
        // Even function. likely ax^4 + cx^2 + e
        // y = A(x^2)^2 + B(x^2) + C
        // Let Z = x^2. Points: Z=0->y[2], Z=1->y[3], Z=4->y[4]
        // Fit quadratic to (0, y0), (1, y1), (4, y4)
        double Y0 = y[2];
        double Y1 = y[3];
        double Y4 = y[4];
        
        // C = Y0
        // A(1) + B(1) + Y0 = Y1  -> A+B = Y1-Y0
        // A(16) + B(4) + Y0 = Y4 -> 16A+4B = Y4-Y0 -> 4A+B = (Y4-Y0)/4
        
        double eq1 = Y1 - Y0;
        double eq2 = (Y4 - Y0) / 4.0;
        
        // (4A+B) - (A+B) = 3A
        double val3A = eq2 - eq1;
        double A = val3A / 3.0;
        double B = eq1 - A;
        
        outPoly.degree = 4;
        outPoly.a = A;
        outPoly.c = B;
        outPoly.e = Y0;
        // Verify?
        if (abs(A) < 1e-9) outPoly.degree = 2; // degenerates
        
        return true;
    }

    // Check Cubic
    // Fit degree 3 to 4 points (-1,0,1,2)
    // Coeffs usually integers in school problems.
    // Solve system for a,b,c,d?
    // f(0) = d
    // f(1) = a+b+c+d
    // f(-1) = -a+b-c+d
    // f(2) = 8a+4b+2c+d
    
    double D = y[2];
    double F1 = y[3];
    double Fm1 = y[1];
    double F2 = y[4];
    
    // b+d = (F1 + Fm1)/2
    double b_plus_d = (F1 + Fm1)/2.0;
    double B = b_plus_d - D;
    
    // a+c = F1 - (b+d)
    double a_plus_c = F1 - b_plus_d;
    
    // 8a+4b+2c+d = F2
    // 6a + 2(a+c) + 4b + d = F2 ?? no
    // 8a + 2c = F2 - 4B - D
    // 2(a+c) + 6a = ...
    double rhs = F2 - 4*B - D;
    double six_a = rhs - 2*a_plus_c;
    double A = six_a / 6.0;
    double C = a_plus_c - A;

    // Check with x=-2 (y[0]) ?
    // -8a + 4b - 2c + d = y[0] ?
    double check = -8*A + 4*B - 2*C + D;
    if (abs(check - y[0]) < 1e-9) {
        outPoly.degree = 3;
        outPoly.a = A;
        outPoly.b = B;
        outPoly.c = C;
        outPoly.e = D; // d in formula
        return true;
    }

    return false;
}

SolveResult EquationSolver::solveWithNewton(const std::vector<Token> &rpn,
                                            VariableContext &vars,
                                            StepLogger *logger) {
    // Basic Newton-Raphson implementation
    // x_new = x_old - f(x)/f'(x)
    // f'(x) approx (f(x+h) - f(x))/h
    
    SolveResult res;
    res.ok = false;
    
    double x = 1.0; // Initial guess
    // Try to get a better guess?
    if (vars.getAns() != 0) x = vars.getAns(); 

    double originalX = 0; vars.getVariable('x', originalX);

    for (int i=0; i<20; ++i) {
        vars.setVariable('x', x);
        EvalResult r = _evaluator.evaluateRPN(rpn, vars);
        if (!r.ok) { vars.setVariable('x', originalX); res.errorMessage="Eval Err"; return res; }
        double fx = r.value;
        
        if (abs(fx) < 1e-9) {
            vars.setVariable('x', originalX);
            res.ok = true;
            res.roots.push_back(x);
             if (logger) logger->addStep("Newton Raphson", "Raiz aprox: " + String(x));
            return res;
        }

        // Derivative
        double h = 1e-5;
        vars.setVariable('x', x+h);
        EvalResult r2 = _evaluator.evaluateRPN(rpn, vars);
        double fxh = r2.value;
        double df = (fxh - fx) / h;
        
        if (abs(df) < 1e-12) {
             x += 1.0; // Jump
        } else {
             x = x - fx/df;
        }
    }
    
    vars.setVariable('x', originalX);
    res.errorMessage = "No converge";
    return res;
}
