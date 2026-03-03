/**
 * MathEvaluator.cpp — Evaluador Simbólico + Decimal del AST
 *
 * Fase 4+5 del Motor V.P.A.M.
 *
 * Implementación completa:
 *   · Recorrido recursivo del AST con evaluación por tipo de nodo
 *   · Aritmética exacta de fracciones (GCD/LCM)
 *   · Simplificación de radicales (factor cuadrado perfecto)
 *   · Multiplicadores de π/e (CAS-level)
 *   · Funciones trigonométricas (DEG/RAD)
 *   · Funciones logarítmicas (ln, log, log_base)
 *   · Precedencia de operadores (* antes de +/−)
 *   · 3 estados S⇔D: Simbólico, Periódico, Extendido
 *   · División Larga Procedural (alta precisión sin double)
 *   · Constantes π/e con hasta 1000 dígitos (PROGMEM)
 *   · Manejo de errores ("Math ERROR")
 */

#include "MathEvaluator.h"
#include "VariableManager.h"
#include "Constants.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <limits>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Variable global de modo de ángulo
// ════════════════════════════════════════════════════════════════════════════
AngleMode g_angleMode = AngleMode::RAD;

// ════════════════════════════════════════════════════════════════════════════
// Utilidades matemáticas
// ════════════════════════════════════════════════════════════════════════════

int64_t gcd(int64_t a, int64_t b) {
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;
    while (b) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int64_t lcm(int64_t a, int64_t b) {
    if (a == 0 || b == 0) return 0;
    int64_t g = gcd(a, b);
    return (a / g) * b;   // evita overflow parcial
}

bool isPerfectSquare(int64_t n) {
    if (n < 0) return false;
    if (n == 0) return true;
    int64_t s = isqrt(n);
    return s * s == n;
}

int64_t isqrt(int64_t n) {
    if (n <= 0) return 0;
    int64_t x = static_cast<int64_t>(std::sqrt(static_cast<double>(n)));
    // Ajustar por errores de redondeo
    while (x * x > n) --x;
    while ((x + 1) * (x + 1) <= n) ++x;
    return x;
}

// ════════════════════════════════════════════════════════════════════════════
// ExactVal — Constructores de conveniencia
// ════════════════════════════════════════════════════════════════════════════

ExactVal ExactVal::fromFrac(int64_t n, int64_t d) {
    ExactVal v;
    v.num = n; v.den = d; v.outer = 1; v.inner = 1;
    v.simplify();
    return v;
}

ExactVal ExactVal::fromInt(int64_t n) {
    return fromFrac(n, 1);
}

ExactVal ExactVal::fromDouble(double val) {
    // NaN / Inf guard
    if (std::isnan(val) || std::isinf(val)) {
        return makeError("Math ERROR");
    }

    // If the absolute value exceeds safe int64_t range (~9.2e18),
    // store as an approximate double — prevents undefined behaviour
    // from static_cast<int64_t> on values like 1e100.
    if (std::abs(val) >= 9.0e18) {
        ExactVal v;
        v.num = 0; v.den = 1; v.outer = 1; v.inner = 1;
        v.approximate = true;
        v.approxVal   = val;
        v.ok = true;
        return v;
    }

    // If it's an exact integer, use that
    if (val == std::floor(val) && std::abs(val) < 1e15) {
        return fromInt(static_cast<int64_t>(val));
    }

    // Convert decimal to fraction with denominator as power of 10
    // Limit to 10 decimal digits to avoid overflow
    double abs_val = std::abs(val);
    int64_t sign = (val < 0) ? -1 : 1;

    // Find how many decimal digits (max 10)
    int decimals = 0;
    double temp = abs_val;
    int64_t denom = 1;
    while (decimals < 10 && std::abs(temp - std::round(temp)) > 1e-12) {
        temp *= 10.0;
        denom *= 10;
        decimals++;
        // Safety: if denom * 10 would overflow or temp > int64_t range, stop
        if (denom > 1000000000LL || temp >= 9.0e18) break;
    }

    int64_t numer = static_cast<int64_t>(std::round(temp)) * sign;
    return fromFrac(numer, denom);
}

ExactVal ExactVal::fromRadical(int64_t out, int64_t inn) {
    ExactVal v;
    v.num = 1; v.den = 1; v.outer = out; v.inner = inn;
    v.ok = true;
    v.simplifyRadical();
    return v;
}

ExactVal ExactVal::fromPi(int64_t n, int64_t d, int8_t mul) {
    ExactVal v;
    v.num = n; v.den = d; v.outer = 1; v.inner = 1;
    v.piMul = mul; v.eMul = 0;
    v.simplify();
    return v;
}

ExactVal ExactVal::fromE(int64_t n, int64_t d, int8_t mul) {
    ExactVal v;
    v.num = n; v.den = d; v.outer = 1; v.inner = 1;
    v.piMul = 0; v.eMul = mul;
    v.simplify();
    return v;
}

ExactVal ExactVal::makeError(const std::string& msg) {
    ExactVal v;
    v.ok = false;
    v.error = msg;
    return v;
}

// ════════════════════════════════════════════════════════════════════════════
// ExactVal — Conversión y simplificación
// ════════════════════════════════════════════════════════════════════════════

double ExactVal::toDouble() const {
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    if (approximate) return approxVal;
    double rational = static_cast<double>(num) / static_cast<double>(den);
    double radPart = (inner <= 1) ? static_cast<double>(outer)
                                  : outer * std::sqrt(static_cast<double>(inner));
    double result = rational * radPart;
    if (piMul != 0) result *= std::pow(M_PI, static_cast<double>(piMul));
    if (eMul != 0)  result *= std::pow(M_E, static_cast<double>(eMul));
    return result;
}

void ExactVal::simplify() {
    if (!ok || den == 0) return;

    // Mantener denominador positivo
    if (den < 0) {
        num = -num;
        den = -den;
    }

    // Reducir fracción
    int64_t g = gcd(num, den);
    if (g > 1) {
        num /= g;
        den /= g;
    }
}

void ExactVal::simplifyRadical() {
    if (!ok || inner <= 1) return;

    // Si inner es negativo, error
    if (inner < 0) {
        ok = false;
        error = "Math ERROR";
        return;
    }

    // Extraer factores cuadrados perfectos del inner
    // √(a²·b) = a·√b
    int64_t extractedFactor = 1;
    int64_t remaining = inner;

    // Probar con factores pequeños primero
    for (int64_t f = 2; f * f <= remaining; ++f) {
        while (remaining % (f * f) == 0) {
            extractedFactor *= f;
            remaining /= (f * f);
        }
    }

    outer *= extractedFactor;
    inner = remaining;

    // Si inner se redujo a 1, ya no hay radical
    // Incorporar outer a la parte racional
    if (inner == 1) {
        num *= outer;
        outer = 1;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Aritmética de ExactVal
// ════════════════════════════════════════════════════════════════════════════

/// Helper: verifica que ambos están ok
static inline bool checkOk(const ExactVal& a, const ExactVal& b) {
    return a.ok && b.ok;
}

ExactVal exactAdd(const ExactVal& a, const ExactVal& b) {
    if (!checkOk(a, b))
        return ExactVal::makeError(a.ok ? b.error : a.error);

    // If either operand is an approximate large double, use double arithmetic
    if (a.approximate || b.approximate)
        return ExactVal::fromDouble(a.toDouble() + b.toDouble());

    // Si ambos son racionales puros (sin radical, sin constantes)
    if (a.isRational() && b.isRational()) {
        // a.num/a.den + b.num/b.den
        int64_t d = lcm(a.den, b.den);
        int64_t n = a.num * (d / a.den) + b.num * (d / b.den);
        return ExactVal::fromFrac(n, d);
    }

    // Si ambos tienen exactamente la misma constante (ej. 2π + 3π = 5π)
    if (a.piMul == b.piMul && a.eMul == b.eMul && a.inner == b.inner) {
        if (a.isRational() || (a.inner == b.inner)) {
            int64_t d = lcm(a.den, b.den);
            int64_t na = a.num * a.outer * (d / a.den);
            int64_t nb = b.num * b.outer * (d / b.den);
            ExactVal result;
            result.num = na + nb;
            result.den = d;
            result.outer = 1;
            result.inner = a.inner;
            result.piMul = a.piMul;
            result.eMul = a.eMul;
            result.simplify();
            result.simplifyRadical();
            return result;
        }
    }

    // Si ambos tienen el mismo radical (ej. 2√3 + 5√3 = 7√3)
    if (a.hasRadical() && b.hasRadical() && a.inner == b.inner) {
        // (a.num/a.den * a.outer + b.num/b.den * b.outer) √inner
        int64_t d = lcm(a.den, b.den);
        int64_t combinedOuter = a.outer * a.num * (d / a.den) +
                                b.outer * b.num * (d / b.den);
        ExactVal result;
        result.num = combinedOuter;
        result.den = d;
        result.outer = 1;
        result.inner = a.inner;
        result.simplify();
        result.simplifyRadical();
        return result;
    }

    // Caso general: recurrir a decimal → fracción
    double sum = a.toDouble() + b.toDouble();
    return ExactVal::fromDouble(sum);
}

ExactVal exactSub(const ExactVal& a, const ExactVal& b) {
    ExactVal negB = exactNeg(b);
    return exactAdd(a, negB);
}

ExactVal exactMul(const ExactVal& a, const ExactVal& b) {
    if (!checkOk(a, b))
        return ExactVal::makeError(a.ok ? b.error : a.error);

    // If either operand is an approximate large double, use double arithmetic
    if (a.approximate || b.approximate)
        return ExactVal::fromDouble(a.toDouble() * b.toDouble());

    ExactVal result;
    result.num   = a.num * b.num;
    result.den   = a.den * b.den;
    result.outer = a.outer * b.outer;

    // Multiplicar radicales: √a · √b = √(a·b)
    if (a.inner > 1 && b.inner > 1) {
        result.inner = a.inner * b.inner;
    } else if (a.inner > 1) {
        result.inner = a.inner;
    } else {
        result.inner = b.inner;
    }

    // Propagar multiplicadores de constantes (π^a * π^b = π^(a+b))
    result.piMul = static_cast<int8_t>(a.piMul + b.piMul);
    result.eMul  = static_cast<int8_t>(a.eMul + b.eMul);

    result.simplify();
    result.simplifyRadical();
    return result;
}

ExactVal exactDiv(const ExactVal& a, const ExactVal& b) {
    if (!checkOk(a, b))
        return ExactVal::makeError(a.ok ? b.error : a.error);

    // División por cero
    if (b.isZero()) {
        return ExactVal::makeError("Math ERROR");
    }

    // If either operand is an approximate large double, use double arithmetic
    if (a.approximate || b.approximate)
        return ExactVal::fromDouble(a.toDouble() / b.toDouble());

    // Invertir b: (b.num/b.den * b.outer√b.inner)⁻¹
    // Si b es racional: invertir → den/num
    if (b.isRational()) {
        ExactVal invB;
        invB.num   = b.den;
        invB.den   = b.num;
        invB.outer = 1;
        invB.inner = 1;
        if (invB.den < 0) { invB.num = -invB.num; invB.den = -invB.den; }
        return exactMul(a, invB);
    }

    // Caso general con radicales: usar double → fracción
    double dv = a.toDouble() / b.toDouble();
    return ExactVal::fromDouble(dv);
}

ExactVal exactNeg(const ExactVal& a) {
    if (!a.ok) return a;
    if (a.approximate) return ExactVal::fromDouble(-a.approxVal);
    ExactVal result = a;
    result.num = -result.num;
    return result;
}

ExactVal exactPow(const ExactVal& base, const ExactVal& exp) {
    if (!checkOk(base, exp))
        return ExactVal::makeError(base.ok ? exp.error : base.error);

    // Solo soportamos exponentes enteros para resultado exacto
    if (!exp.isInteger()) {
        // Exponente no entero: usar decimal
        double result = std::pow(base.toDouble(), exp.toDouble());
        if (std::isnan(result) || std::isinf(result)) {
            return ExactVal::makeError("Math ERROR");
        }
        return ExactVal::fromDouble(result);
    }

    int64_t e = exp.num;  // exponente entero (puede ser negativo)

    // Caso especial: exp = 0
    if (e == 0) {
        if (base.isZero()) return ExactVal::makeError("Math ERROR"); // 0^0
        return ExactVal::fromInt(1);
    }

    // Base 0 con exp positivo
    if (base.isZero()) {
        if (e > 0) return ExactVal::fromInt(0);
        return ExactVal::makeError("Math ERROR");   // 0^(neg)
    }

    // Solo racionales (sin radical) para exactitud
    if (!base.isRational()) {
        double result = std::pow(base.toDouble(), static_cast<double>(e));
        if (std::isnan(result) || std::isinf(result)) {
            return ExactVal::makeError("Math ERROR");
        }
        return ExactVal::fromDouble(result);
    }

    // Exponente negativo: invertir base
    bool negExp = (e < 0);
    int64_t absE = negExp ? -e : e;

    // Calcular potencia de la fracción
    // Limitar para evitar overflow
    if (absE > 30) {
        double result = std::pow(base.toDouble(), static_cast<double>(e));
        if (std::isnan(result) || std::isinf(result)) {
            return ExactVal::makeError("Math ERROR");
        }
        return ExactVal::fromDouble(result);
    }

    int64_t rn = 1, rd = 1;
    int64_t bn = base.num, bd = base.den;
    if (negExp) { std::swap(bn, bd); if (bd < 0) { bn = -bn; bd = -bd; } }

    for (int64_t i = 0; i < absE; ++i) {
        rn *= bn;
        rd *= bd;
        // Simplificar parcialmente para evitar overflow
        int64_t g = gcd(rn, rd);
        if (g > 1) { rn /= g; rd /= g; }
    }

    return ExactVal::fromFrac(rn, rd);
}

ExactVal exactSqrt(const ExactVal& a) {
    if (!a.ok) return a;

    double val = a.toDouble();
    if (val < 0.0) {
        return ExactVal::makeError("Math ERROR");
    }

    // Si es racional, intentar raíz exacta de num y den
    if (a.isRational()) {
        int64_t absNum = (a.num < 0) ? -a.num : a.num;
        int64_t absDen = a.den;

        if (a.num < 0) {
            return ExactVal::makeError("Math ERROR");
        }

        // Intentar raíz exacta del numerador y denominador
        if (isPerfectSquare(absNum) && isPerfectSquare(absDen)) {
            return ExactVal::fromFrac(isqrt(absNum), isqrt(absDen));
        }

        // Simplificar radical: √(num/den)
        // = √num / √den
        // Si den es cuadrado perfecto: outer√num / sqrt(den)
        if (isPerfectSquare(absDen)) {
            ExactVal result;
            result.num = 1;
            result.den = isqrt(absDen);
            result.outer = 1;
            result.inner = absNum;
            result.simplifyRadical();
            return result;
        }

        // Caso general: racionalizar denominador
        // √(a/b) = √(ab) / b
        ExactVal result;
        result.num = 1;
        result.den = absDen;
        result.outer = 1;
        result.inner = absNum * absDen;
        result.simplifyRadical();
        return result;
    }

    // Si ya tiene radical → resultado decimal
    return ExactVal::fromDouble(std::sqrt(val));
}

// ════════════════════════════════════════════════════════════════════════════
// MathEvaluator — Evaluación recursiva
// ════════════════════════════════════════════════════════════════════════════

ExactVal MathEvaluator::evaluate(const MathNode* root) const {
    if (!root) return ExactVal::makeError("Math ERROR");

    switch (root->type()) {
        case NodeType::Row:      return evalRow(static_cast<const NodeRow*>(root));
        case NodeType::Number:   return evalNumber(static_cast<const NodeNumber*>(root));
        case NodeType::Fraction: return evalFraction(static_cast<const NodeFraction*>(root));
        case NodeType::Power:    return evalPower(static_cast<const NodePower*>(root));
        case NodeType::Root:     return evalRoot(static_cast<const NodeRoot*>(root));
        case NodeType::Paren:    return evalParen(static_cast<const NodeParen*>(root));
        case NodeType::Function: return evalFunction(static_cast<const NodeFunction*>(root));
        case NodeType::LogBase:  return evalLogBase(static_cast<const NodeLogBase*>(root));
        case NodeType::Constant: return evalConstant(static_cast<const NodeConstant*>(root));
        case NodeType::Variable: return evalVariable(static_cast<const NodeVariable*>(root));
        case NodeType::Empty:    return ExactVal::makeError("Math ERROR");
        case NodeType::Operator: return ExactVal::makeError("Math ERROR");
        default:                 return ExactVal::makeError("Math ERROR");
    }
}

// ────────────────────────────────────────────────────────────────────────────
// evalRow — Procesa secuencia de hijos con precedencia de operadores
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalRow(const NodeRow* row) const {
    if (!row || row->isEmpty()) {
        return ExactVal::makeError("Math ERROR");
    }

    const auto& kids = row->children();
    int n = static_cast<int>(kids.size());

    // Si solo hay un hijo, evaluarlo directamente
    if (n == 1) {
        return evaluate(kids[0].get());
    }

    // Separar en términos y operadores
    // Un "término" puede ser: Number, Fraction, Power, Root, Paren
    // Un operador: NodeOperator
    std::vector<ExactVal> terms;
    std::vector<OpKind> ops;

    // Manejar negación implícita: si el primer hijo es un operador Sub,
    // tratar como negación del siguiente término
    int i = 0;

    // Verificar si empieza con un operador Sub (negación unaria)
    if (kids[i]->type() == NodeType::Operator) {
        auto* opNode = static_cast<const NodeOperator*>(kids[i].get());
        if (opNode->op() == OpKind::Sub) {
            // Siguiente término negado
            i++;
            if (i >= n) return ExactVal::makeError("Math ERROR");
            ExactVal firstTerm = evaluate(kids[i].get());
            if (!firstTerm.ok) return firstTerm;
            terms.push_back(exactNeg(firstTerm));
            i++;
        } else {
            return ExactVal::makeError("Math ERROR");
        }
    } else {
        // Primer término normal
        ExactVal firstTerm = evaluate(kids[i].get());
        if (!firstTerm.ok) return firstTerm;
        terms.push_back(firstTerm);
        i++;
    }

    // Resto: alternancia operador-término
    while (i < n) {
        // Esperar un operador
        if (kids[i]->type() != NodeType::Operator) {
            // Multiplicación implícita (ej. "2(3)" → 2 × 3)
            // Tratamos yuxtaposición como multiplicación
            ops.push_back(OpKind::Mul);
            ExactVal term = evaluate(kids[i].get());
            if (!term.ok) return term;
            terms.push_back(term);
            i++;
            continue;
        }

        auto* opNode = static_cast<const NodeOperator*>(kids[i].get());
        ops.push_back(opNode->op());
        i++;

        // Esperar un término
        if (i >= n) return ExactVal::makeError("Math ERROR");

        ExactVal term = evaluate(kids[i].get());
        if (!term.ok) return term;
        terms.push_back(term);
        i++;
    }

    // Evaluar con precedencia
    return evalWithPrecedence(terms, ops);
}

// ────────────────────────────────────────────────────────────────────────────
// evalWithPrecedence — Multiplicación antes que +/−
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalWithPrecedence(
    const std::vector<ExactVal>& terms,
    const std::vector<OpKind>& ops) const
{
    if (terms.empty()) return ExactVal::makeError("Math ERROR");
    if (terms.size() == 1) return terms[0];

    // Paso 1: procesar multiplicaciones (mayor precedencia)
    std::vector<ExactVal> addTerms;
    std::vector<OpKind> addOps;

    ExactVal current = terms[0];
    for (size_t i = 0; i < ops.size(); ++i) {
        if (ops[i] == OpKind::Mul) {
            current = exactMul(current, terms[i + 1]);
            if (!current.ok) return current;
        } else {
            addTerms.push_back(current);
            addOps.push_back(ops[i]);
            current = terms[i + 1];
        }
    }
    addTerms.push_back(current);

    // Paso 2: procesar sumas y restas (izquierda a derecha)
    ExactVal result = addTerms[0];
    for (size_t i = 0; i < addOps.size(); ++i) {
        if (addOps[i] == OpKind::Add) {
            result = exactAdd(result, addTerms[i + 1]);
        } else { // Sub
            result = exactSub(result, addTerms[i + 1]);
        }
        if (!result.ok) return result;
    }

    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// evalNumber — Parsear literal numérico
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalNumber(const NodeNumber* node) const {
    if (!node || node->value().empty()) {
        return ExactVal::makeError("Math ERROR");
    }

    const std::string& val = node->value();

    // Buscar punto decimal
    auto dotPos = val.find('.');
    if (dotPos == std::string::npos) {
        // Entero
        int64_t n = 0;
        bool negative = false;
        size_t start = 0;
        if (!val.empty() && val[0] == '-') {
            negative = true;
            start = 1;
        }
        for (size_t i = start; i < val.size(); ++i) {
            if (val[i] < '0' || val[i] > '9') {
                return ExactVal::makeError("Math ERROR");
            }
            n = n * 10 + (val[i] - '0');
        }
        return ExactVal::fromInt(negative ? -n : n);
    }

    // Decimal: convertir a fracción
    // "3.14" → 314/100
    std::string intPart = val.substr(0, dotPos);
    std::string decPart = val.substr(dotPos + 1);

    if (decPart.empty()) {
        // "3." → tratar como "3"
        int64_t n = 0;
        for (char c : intPart) {
            if (c == '-') continue;
            n = n * 10 + (c - '0');
        }
        if (!intPart.empty() && intPart[0] == '-') n = -n;
        return ExactVal::fromInt(n);
    }

    int64_t denominator = 1;
    for (size_t i = 0; i < decPart.size(); ++i) {
        denominator *= 10;
    }

    int64_t whole = 0;
    bool negative = false;
    if (!intPart.empty()) {
        size_t s = 0;
        if (intPart[0] == '-') { negative = true; s = 1; }
        for (size_t i = s; i < intPart.size(); ++i) {
            whole = whole * 10 + (intPart[i] - '0');
        }
    }

    int64_t decVal = 0;
    for (char c : decPart) {
        if (c < '0' || c > '9') return ExactVal::makeError("Math ERROR");
        decVal = decVal * 10 + (c - '0');
    }

    int64_t numerator = whole * denominator + decVal;
    if (negative) numerator = -numerator;

    return ExactVal::fromFrac(numerator, denominator);
}

// ────────────────────────────────────────────────────────────────────────────
// evalFraction — Numerador / Denominador
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalFraction(const NodeFraction* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    ExactVal num = evaluate(node->numerator());
    if (!num.ok) return num;

    ExactVal den = evaluate(node->denominator());
    if (!den.ok) return den;

    return exactDiv(num, den);
}

// ────────────────────────────────────────────────────────────────────────────
// evalPower — Base^Exponente
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalPower(const NodePower* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    ExactVal base = evaluate(node->base());
    if (!base.ok) return base;

    ExactVal exp = evaluate(node->exponent());
    if (!exp.ok) return exp;

    return exactPow(base, exp);
}

// ────────────────────────────────────────────────────────────────────────────
// evalRoot — √(radicand) o ⁿ√(radicand)
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalRoot(const NodeRoot* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    ExactVal radicand = evaluate(node->radicand());
    if (!radicand.ok) return radicand;

    if (node->hasDegree()) {
        // ⁿ√x = x^(1/n)
        ExactVal degree = evaluate(node->degree());
        if (!degree.ok) return degree;

        if (degree.isZero()) return ExactVal::makeError("Math ERROR");

        // Para grado 2, usar exactSqrt
        if (degree.isInteger() && degree.num == 2) {
            return exactSqrt(radicand);
        }

        // Caso general: x^(1/n) → decimal
        double base = radicand.toDouble();
        double n = degree.toDouble();
        if (base < 0.0) {
            // Raíces impares de negativos
            int64_t ni = degree.isInteger() ? degree.num : 0;
            if (ni != 0 && (ni % 2 != 0)) {
                double result = -std::pow(-base, 1.0 / n);
                return ExactVal::fromDouble(result);
            }
            return ExactVal::makeError("Math ERROR");
        }
        double result = std::pow(base, 1.0 / n);
        if (std::isnan(result) || std::isinf(result)) {
            return ExactVal::makeError("Math ERROR");
        }
        return ExactVal::fromDouble(result);
    }

    // Raíz cuadrada
    return exactSqrt(radicand);
}

// ────────────────────────────────────────────────────────────────────────────
// evalParen — (contenido)
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalParen(const NodeParen* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");
    return evaluate(node->content());
}

// ────────────────────────────────────────────────────────────────────────────
// evalFunction — sin(x), cos(x), tan(x), arcsin(x), arccos(x), arctan(x),
//                ln(x), log(x)
//
// Respeta g_angleMode para trig directas (DEG→conversión a RAD interna).
// Trig inversas devuelven en el modo actual.
// Simplificaciones simbólicas: sin(0)=0, cos(0)=1, ln(1)=0, ln(e)=1, etc.
// ────────────────────────────────────────────────────────────────────────────

double MathEvaluator::degToRad(double deg) {
    return deg * M_PI / 180.0;
}

double MathEvaluator::radToDeg(double rad) {
    return rad * 180.0 / M_PI;
}

ExactVal MathEvaluator::evalFunction(const NodeFunction* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    ExactVal arg = evaluate(node->argument());
    if (!arg.ok) return arg;

    double x = arg.toDouble();
    FuncKind kind = node->funcKind();

    switch (kind) {
        case FuncKind::Sin: {
            // Simplificación simbólica: sin(0) = 0
            if (arg.isZero()) return ExactVal::fromInt(0);
            double input = (g_angleMode == AngleMode::DEG) ? degToRad(x) : x;
            double r = std::sin(input);
            // Intentar resultado exacto para ángulos notables
            if (std::abs(r) < 1e-15) return ExactVal::fromInt(0);
            if (std::abs(r - 1.0) < 1e-15) return ExactVal::fromInt(1);
            if (std::abs(r + 1.0) < 1e-15) return ExactVal::fromInt(-1);
            if (std::abs(r - 0.5) < 1e-15) return ExactVal::fromFrac(1, 2);
            if (std::abs(r + 0.5) < 1e-15) return ExactVal::fromFrac(-1, 2);
            return ExactVal::fromDouble(r);
        }
        case FuncKind::Cos: {
            if (arg.isZero()) return ExactVal::fromInt(1);
            double input = (g_angleMode == AngleMode::DEG) ? degToRad(x) : x;
            double r = std::cos(input);
            if (std::abs(r) < 1e-15) return ExactVal::fromInt(0);
            if (std::abs(r - 1.0) < 1e-15) return ExactVal::fromInt(1);
            if (std::abs(r + 1.0) < 1e-15) return ExactVal::fromInt(-1);
            if (std::abs(r - 0.5) < 1e-15) return ExactVal::fromFrac(1, 2);
            if (std::abs(r + 0.5) < 1e-15) return ExactVal::fromFrac(-1, 2);
            return ExactVal::fromDouble(r);
        }
        case FuncKind::Tan: {
            if (arg.isZero()) return ExactVal::fromInt(0);
            double input = (g_angleMode == AngleMode::DEG) ? degToRad(x) : x;
            double cosVal = std::cos(input);
            if (std::abs(cosVal) < 1e-15) return ExactVal::makeError("Math ERROR");
            double r = std::tan(input);
            if (std::abs(r) < 1e-15) return ExactVal::fromInt(0);
            if (std::abs(r - 1.0) < 1e-15) return ExactVal::fromInt(1);
            if (std::abs(r + 1.0) < 1e-15) return ExactVal::fromInt(-1);
            return ExactVal::fromDouble(r);
        }
        case FuncKind::ArcSin: {
            if (x < -1.0 || x > 1.0) return ExactVal::makeError("Math ERROR");
            double r = std::asin(x);
            if (g_angleMode == AngleMode::DEG) r = radToDeg(r);
            if (std::abs(r) < 1e-15) return ExactVal::fromInt(0);
            return ExactVal::fromDouble(r);
        }
        case FuncKind::ArcCos: {
            if (x < -1.0 || x > 1.0) return ExactVal::makeError("Math ERROR");
            double r = std::acos(x);
            if (g_angleMode == AngleMode::DEG) r = radToDeg(r);
            return ExactVal::fromDouble(r);
        }
        case FuncKind::ArcTan: {
            double r = std::atan(x);
            if (g_angleMode == AngleMode::DEG) r = radToDeg(r);
            if (std::abs(r) < 1e-15) return ExactVal::fromInt(0);
            return ExactVal::fromDouble(r);
        }
        case FuncKind::Ln: {
            // ln(1) = 0
            if (arg.isRational() && arg.num == 1 && arg.den == 1)
                return ExactVal::fromInt(0);
            // ln(e) = 1 (si arg es exactamente e^1)
            if (arg.hasE() && arg.eMul == 1 && arg.num == 1 && arg.den == 1 && !arg.hasRadical() && !arg.hasPi())
                return ExactVal::fromInt(1);
            // ln(e^n) = n
            if (arg.hasE() && arg.num == 1 && arg.den == 1 && !arg.hasRadical() && !arg.hasPi())
                return ExactVal::fromInt(arg.eMul);
            if (x <= 0.0) return ExactVal::makeError("Math ERROR");
            return ExactVal::fromDouble(std::log(x));
        }
        case FuncKind::Log: {
            // log10(1) = 0
            if (arg.isRational() && arg.num == 1 && arg.den == 1)
                return ExactVal::fromInt(0);
            // log10(10) = 1
            if (arg.isInteger() && arg.num == 10)
                return ExactVal::fromInt(1);
            // log10(10^n) — check powers of 10
            if (arg.isInteger() && arg.num > 0) {
                int64_t v = arg.num;
                int exp = 0;
                while (v > 1 && v % 10 == 0) { v /= 10; exp++; }
                if (v == 1 && exp > 0) return ExactVal::fromInt(exp);
            }
            if (x <= 0.0) return ExactVal::makeError("Math ERROR");
            return ExactVal::fromDouble(std::log10(x));
        }
    }
    return ExactVal::makeError("Math ERROR");
}

// ────────────────────────────────────────────────────────────────────────────
// evalLogBase — log_n(x) = ln(x) / ln(n)
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalLogBase(const NodeLogBase* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    ExactVal base = evaluate(node->base());
    if (!base.ok) return base;

    ExactVal arg = evaluate(node->argument());
    if (!arg.ok) return arg;

    double b = base.toDouble();
    double x = arg.toDouble();

    if (b <= 0.0 || b == 1.0 || x <= 0.0)
        return ExactVal::makeError("Math ERROR");

    // Simplificaciones: log_b(1) = 0, log_b(b) = 1
    if (arg.isRational() && arg.num == 1 && arg.den == 1)
        return ExactVal::fromInt(0);

    // log_b(b) = 1 (ambos enteros iguales)
    if (base.isInteger() && arg.isInteger() && base.num == arg.num)
        return ExactVal::fromInt(1);

    // log_b(b^n) — check if arg is an integer power of base
    if (base.isInteger() && arg.isInteger() && base.num > 1) {
        int64_t v = arg.num;
        int exp = 0;
        while (v > 1 && v % base.num == 0) { v /= base.num; exp++; }
        if (v == 1 && exp > 0) return ExactVal::fromInt(exp);
    }

    double result = std::log(x) / std::log(b);
    if (std::isnan(result) || std::isinf(result))
        return ExactVal::makeError("Math ERROR");
    return ExactVal::fromDouble(result);
}

// ────────────────────────────────────────────────────────────────────────────
// evalConstant — π, e como valores simbólicos
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalConstant(const NodeConstant* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    switch (node->constKind()) {
        case ConstKind::Pi:
            return ExactVal::fromPi(1, 1, 1);   // 1 * π
        case ConstKind::E:
            return ExactVal::fromE(1, 1, 1);     // 1 * e
    }
    return ExactVal::makeError("Math ERROR");
}

// ────────────────────────────────────────────────────────────────────────────
// evalVariable — Obtiene el valor de una variable del VariableManager
// ────────────────────────────────────────────────────────────────────────────

ExactVal MathEvaluator::evalVariable(const NodeVariable* node) const {
    if (!node) return ExactVal::makeError("Math ERROR");

    char name = node->name();
    if (!VariableManager::isValidName(name)) {
        return ExactVal::makeError("Math ERROR");
    }

    return VariableManager::instance().getVariable(name);
}

// ════════════════════════════════════════════════════════════════════════════
// Generación de AST resultado
// ════════════════════════════════════════════════════════════════════════════

// ── Helper: genera NodeNumber desde un int64_t ──
static NodePtr numNode(int64_t val) {
    std::string s;
    if (val < 0) {
        s = std::to_string(-val);
    } else {
        s = std::to_string(val);
    }
    return makeNumber(s);
}

// ── Helper: añade multiplicador de constante (π^n o e^n) al row ──
static void appendConstantSymbol(NodeRow* rowPtr, int8_t piMul, int8_t eMul) {
    if (piMul != 0) {
        rowPtr->appendChild(makeConstant(ConstKind::Pi));
        if (piMul > 1) {
            auto powNode = makePower(
                makeConstant(ConstKind::Pi),
                makeNumber(std::to_string(piMul))
            );
            // Reemplazar: quitar el π que acabamos de añadir y poner el Power
            rowPtr->removeChild(rowPtr->childCount() - 1);
            rowPtr->appendChild(std::move(powNode));
        }
    }
    if (eMul != 0) {
        rowPtr->appendChild(makeConstant(ConstKind::E));
        if (eMul > 1) {
            auto powNode = makePower(
                makeConstant(ConstKind::E),
                makeNumber(std::to_string(eMul))
            );
            rowPtr->removeChild(rowPtr->childCount() - 1);
            rowPtr->appendChild(std::move(powNode));
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Estado 1 — Simbólico: fracción/radical/π/e
// ════════════════════════════════════════════════════════════════════════════

NodePtr MathEvaluator::resultToAST(const ExactVal& val) {
    if (!val.ok) {
        return errorToAST(val.error);
    }

    // ── Approximate (large) values: display as scientific notation ──
    if (val.approximate) {
        auto row = makeRow();
        auto* rowPtr = static_cast<NodeRow*>(row.get());
        double d = val.approxVal;
        if (d < 0) {
            rowPtr->appendChild(makeOperator(OpKind::Sub));
            d = -d;
        }
        // Format in scientific notation: e.g. "1×10" with exponent
        if (d == 0.0) {
            rowPtr->appendChild(makeNumber("0"));
        } else {
            int exponent = static_cast<int>(std::floor(std::log10(d)));
            double mantissa = d / std::pow(10.0, exponent);
            // Round mantissa to 10 significant digits
            char mbuf[32];
            snprintf(mbuf, sizeof(mbuf), "%.10g", mantissa);
            // Remove trailing zeros after decimal
            std::string ms(mbuf);

            if (std::abs(mantissa - 1.0) < 1e-12) {
                // Pure power of 10: show "10^exp"
            } else {
                rowPtr->appendChild(makeNumber(ms));
                rowPtr->appendChild(makeOperator(OpKind::Mul));
            }
            // 10^exponent as power node
            auto base10 = makeNumber("10");
            auto expRow = makeRow();
            static_cast<NodeRow*>(expRow.get())->appendChild(
                makeNumber(std::to_string(exponent))
            );
            rowPtr->appendChild(makePower(std::move(base10), std::move(expRow)));
        }
        return row;
    }

    auto row = makeRow();
    auto* rowPtr = static_cast<NodeRow*>(row.get());

    bool negative = false;

    // Determinar signo
    int64_t num = val.num;
    int64_t den = val.den;
    int64_t outer = val.outer;
    int64_t inner = val.inner;
    int8_t piMul = val.piMul;
    int8_t eMul = val.eMul;

    // Normalizar signo
    int signs = 0;
    if (num < 0) { num = -num; signs++; }
    if (outer < 0) { outer = -outer; signs++; }
    negative = (signs % 2 != 0);

    if (negative) {
        rowPtr->appendChild(makeOperator(OpKind::Sub));
    }

    bool hasConst = (piMul != 0 || eMul != 0);

    // Caso: sin radical
    if (inner <= 1) {
        if (den == 1) {
            // Entero (o entero × constante)
            if (!hasConst || num != 1) {
                rowPtr->appendChild(makeNumber(std::to_string(num)));
            }
            if (hasConst) {
                appendConstantSymbol(rowPtr, piMul, eMul);
            }
        } else {
            // Fracción (o fracción × constante)
            if (hasConst) {
                // (num/den) × π/e → mostrar como fracción con constante
                // Numerador: num × π (o num si num=1 → solo π)
                auto numRow = makeRow();
                auto* numRowPtr = static_cast<NodeRow*>(numRow.get());
                if (num != 1) {
                    numRowPtr->appendChild(makeNumber(std::to_string(num)));
                }
                appendConstantSymbol(numRowPtr, piMul, eMul);

                auto fracNode = makeFraction(
                    std::move(numRow),
                    makeNumber(std::to_string(den))
                );
                rowPtr->appendChild(std::move(fracNode));
            } else {
                auto fracNode = makeFraction(
                    makeNumber(std::to_string(num)),
                    makeNumber(std::to_string(den))
                );
                rowPtr->appendChild(std::move(fracNode));
            }
        }
    } else {
        // Con radical
        int64_t coeff = num * outer;

        if (den == 1) {
            if (coeff != 1) {
                rowPtr->appendChild(makeNumber(std::to_string(coeff)));
            }
            auto radContent = makeRow();
            static_cast<NodeRow*>(radContent.get())->appendChild(
                makeNumber(std::to_string(inner))
            );
            rowPtr->appendChild(makeRoot(std::move(radContent)));
            if (hasConst) appendConstantSymbol(rowPtr, piMul, eMul);
        } else {
            auto numRow = makeRow();
            auto* numRowPtr = static_cast<NodeRow*>(numRow.get());
            if (coeff != 1) {
                numRowPtr->appendChild(makeNumber(std::to_string(coeff)));
            }
            auto radContent = makeRow();
            static_cast<NodeRow*>(radContent.get())->appendChild(
                makeNumber(std::to_string(inner))
            );
            numRowPtr->appendChild(makeRoot(std::move(radContent)));
            if (hasConst) appendConstantSymbol(numRowPtr, piMul, eMul);

            auto fracNode = makeFraction(
                std::move(numRow),
                makeNumber(std::to_string(den))
            );
            rowPtr->appendChild(std::move(fracNode));
        }
    }

    return row;
}

// ════════════════════════════════════════════════════════════════════════════
// Estado 2 — Periódico: decimal periódico con overline, o ~12 dígitos
//            para irracionales (π, e, radicales)
// ════════════════════════════════════════════════════════════════════════════

NodePtr MathEvaluator::resultToPeriodicAST(const ExactVal& val) {
    if (!val.ok) return errorToAST(val.error);

    // Approximate values: delegate to symbolic (scientific notation)
    if (val.approximate) return resultToAST(val);

    auto row = makeRow();
    auto* rowPtr = static_cast<NodeRow*>(row.get());

    // Irracionales (π, e, radicales): mostrar ~12 dígitos
    if (val.isIrrational()) {
        double d = val.toDouble();
        bool neg = (d < 0);
        if (neg) d = -d;

        // Para múltiplos de π o e que son simples, usar constantes de alta precisión
        if (val.hasPi() && !val.hasE() && !val.hasRadical() &&
            val.inner <= 1 && val.piMul == 1) {
            // (num/den) * π → multiplicar los dígitos de π por la fracción
            // Para simplicidad, usar double con 12 dígitos
            std::ostringstream oss;
            oss << std::setprecision(12) << d;
            std::string numStr = oss.str();
            if (neg) rowPtr->appendChild(makeOperator(OpKind::Sub));
            rowPtr->appendChild(makeNumber(numStr));
            return row;
        }
        if (val.hasE() && !val.hasPi() && !val.hasRadical() &&
            val.inner <= 1 && val.eMul == 1) {
            std::ostringstream oss;
            oss << std::setprecision(12) << d;
            std::string numStr = oss.str();
            if (neg) rowPtr->appendChild(makeOperator(OpKind::Sub));
            rowPtr->appendChild(makeNumber(numStr));
            return row;
        }

        // Caso general irracional: 12 dígitos significativos
        if (neg) rowPtr->appendChild(makeOperator(OpKind::Sub));
        std::ostringstream oss;
        oss << std::setprecision(12) << d;
        rowPtr->appendChild(makeNumber(oss.str()));
        return row;
    }

    // Racionales: usar División Larga Procedural para detectar período
    int64_t num = val.num;
    int64_t den = val.den;
    int64_t outer = val.outer;

    // Incorporar outer al numerador
    num *= outer;

    // Si es entero, mostrar directamente
    if (den == 1 || num % den == 0) {
        int64_t intVal = num / den;
        if (intVal < 0) {
            rowPtr->appendChild(makeOperator(OpKind::Sub));
            intVal = -intVal;
        }
        rowPtr->appendChild(makeNumber(std::to_string(intVal)));
        return row;
    }

    // División Larga con detección de período
    LongDivResult ld = longDivision(num, den, 500);

    if (!ld.repeat.empty()) {
        // Decimal periódico → usar NodePeriodicDecimal con overline
        rowPtr->appendChild(makePeriodicDecimal(
            ld.intPart, ld.nonRepeat, ld.repeat, ld.negative));
    } else if (ld.terminating) {
        // Decimal terminante → mostrar completo
        std::string full;
        if (ld.negative) full += "-";
        full += ld.intPart;
        if (!ld.nonRepeat.empty()) {
            full += "." + ld.nonRepeat;
        }
        rowPtr->appendChild(makeNumber(full));
    } else {
        // No se detectó período (raro) → mostrar dígitos generados
        std::string full;
        if (ld.negative) full += "-";
        full += ld.intPart + "." + ld.nonRepeat;
        rowPtr->appendChild(makeNumber(full));
    }

    return row;
}

// ════════════════════════════════════════════════════════════════════════════
// Estado 3 — Extendido: decimal largo con scroll (200-500 dígitos)
// ════════════════════════════════════════════════════════════════════════════

NodePtr MathEvaluator::resultToExtendedAST(const ExactVal& val, int maxDigits) {
    if (!val.ok) return errorToAST(val.error);

    // Approximate values: delegate to symbolic (scientific notation)
    if (val.approximate) return resultToAST(val);

    auto row = makeRow();
    auto* rowPtr = static_cast<NodeRow*>(row.get());

    // Irracionales con constantes: usar dígitos de alta precisión
    if (val.hasPi() && !val.hasE() && !val.hasRadical() && val.piMul == 1) {
        // (num/den) × π
        // Para π puro (num=1, den=1): usar PI_DEC directamente
        if (val.num == 1 && val.den == 1) {
            bool neg = false;  // ya normalizado
            std::string digits = getPiDigits(maxDigits, neg);
            rowPtr->appendChild(makeNumber(digits));
            return row;
        }
        if (val.num == -1 && val.den == 1) {
            rowPtr->appendChild(makeOperator(OpKind::Sub));
            std::string digits = getPiDigits(maxDigits, false);
            rowPtr->appendChild(makeNumber(digits));
            return row;
        }
    }

    if (val.hasE() && !val.hasPi() && !val.hasRadical() && val.eMul == 1) {
        if (val.num == 1 && val.den == 1) {
            std::string digits = getEDigits(maxDigits, false);
            rowPtr->appendChild(makeNumber(digits));
            return row;
        }
        if (val.num == -1 && val.den == 1) {
            rowPtr->appendChild(makeOperator(OpKind::Sub));
            std::string digits = getEDigits(maxDigits, false);
            rowPtr->appendChild(makeNumber(digits));
            return row;
        }
    }

    // Racionales: División Larga extendida
    if (val.isRational() || (!val.hasConstant() && !val.hasRadical())) {
        int64_t num = val.num * val.outer;
        int64_t den = val.den;
        bool neg = (num < 0) != (den < 0);
        if (num < 0) num = -num;
        if (den < 0) den = -den;

        std::string digits = longDivisionExtended(num, den, maxDigits, neg);
        rowPtr->appendChild(makeNumber(digits));
        return row;
    }

    // Caso general (irracionales complejos): usar double con máxima precisión
    double d = val.toDouble();
    bool neg = (d < 0);
    if (neg) {
        d = -d;
        rowPtr->appendChild(makeOperator(OpKind::Sub));
    }
    std::ostringstream oss;
    oss << std::setprecision(15) << d;
    rowPtr->appendChild(makeNumber(oss.str()));

    return row;
}

NodePtr MathEvaluator::resultToDecimalAST(const ExactVal& val, int precision) {
    if (!val.ok) {
        return errorToAST(val.error);
    }

    // Approximate values: delegate to symbolic (scientific notation)
    if (val.approximate) {
        return resultToAST(val);
    }

    double d = val.toDouble();

    auto row = makeRow();
    auto* rowPtr = static_cast<NodeRow*>(row.get());

    // Formatear con la precisión solicitada
    std::ostringstream oss;

    // Si es un entero exacto, mostrarlo sin decimales
    if (d == std::floor(d) && std::abs(d) < 1e15) {
        int64_t intVal = static_cast<int64_t>(d);
        if (intVal < 0) {
            rowPtr->appendChild(makeOperator(OpKind::Sub));
            intVal = -intVal;
        }
        rowPtr->appendChild(makeNumber(std::to_string(intVal)));
    } else {
        if (d < 0) {
            rowPtr->appendChild(makeOperator(OpKind::Sub));
            d = -d;
        }

        oss << std::setprecision(precision) << d;
        std::string numStr = oss.str();

        // Eliminar ceros trailing innecesarios
        if (numStr.find('.') != std::string::npos) {
            size_t last = numStr.find_last_not_of('0');
            if (last != std::string::npos && numStr[last] == '.') {
                numStr = numStr.substr(0, last);  // quitar el punto también
            } else if (last != std::string::npos) {
                numStr = numStr.substr(0, last + 1);
            }
        }

        rowPtr->appendChild(makeNumber(numStr));
    }

    return row;
}

NodePtr MathEvaluator::errorToAST(const std::string& msg) {
    auto row = makeRow();
    auto* rowPtr = static_cast<NodeRow*>(row.get());

    // Usamos un NodeNumber con el texto del error
    // (MathRenderer dibujará esto como texto)
    std::string errMsg = msg.empty() ? "Math ERROR" : msg;
    rowPtr->appendChild(makeNumber(errMsg));

    return row;
}

// ════════════════════════════════════════════════════════════════════════════
// factorizeToAST() — Descompone n en primos y genera el AST visual
//
// Ejemplo: 360 → 2³ × 3² × 5
// El AST usa NodePower para exponentes > 1 y NodeOperator(Mul) entre factores.
// ════════════════════════════════════════════════════════════════════════════

NodePtr MathEvaluator::factorizeToAST(const ExactVal& val) {
    // Validar: debe ser un entero positivo > 1 sin radical ni constantes
    if (!val.ok || !val.isInteger() || val.num < 2) {
        return errorToAST("FACT ERROR");
    }

    int64_t n = val.num;

    // ── Factorización por trial division ─────────────────────────────
    struct PrimeFactor {
        int64_t prime;
        int     exponent;
    };
    std::vector<PrimeFactor> factors;

    // Dividir por 2
    {
        int count = 0;
        while (n % 2 == 0) { n /= 2; ++count; }
        if (count > 0) factors.push_back({2, count});
    }

    // Dividir por impares desde 3
    for (int64_t d = 3; d * d <= n; d += 2) {
        int count = 0;
        while (n % d == 0) { n /= d; ++count; }
        if (count > 0) factors.push_back({d, count});
    }

    // Si queda un primo > sqrt(n_original)
    if (n > 1) {
        factors.push_back({n, 1});
    }

    // ── Generar AST: p₁^a₁ × p₂^a₂ × ... ──────────────────────────
    auto row = makeRow();
    auto* rowPtr = static_cast<NodeRow*>(row.get());

    for (size_t i = 0; i < factors.size(); ++i) {
        if (i > 0) {
            rowPtr->appendChild(makeOperator(OpKind::Mul));
        }

        auto& f = factors[i];
        std::string primeStr = std::to_string(f.prime);

        if (f.exponent == 1) {
            // Solo el primo: 3, 5, 7...
            rowPtr->appendChild(makeNumber(primeStr));
        } else {
            // Primo con exponente: 2³, 3²...
            // Construir NodePower con base = prime, exp = exponent
            auto baseRow = makeRow();
            static_cast<NodeRow*>(baseRow.get())->appendChild(makeNumber(primeStr));

            auto expRow = makeRow();
            static_cast<NodeRow*>(expRow.get())->appendChild(
                makeNumber(std::to_string(f.exponent)));

            rowPtr->appendChild(makePower(std::move(baseRow), std::move(expRow)));
        }
    }

    return row;
}

} // namespace vpam
