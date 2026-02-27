/**
 * ExactVal.h — Valor Exacto: (num/den) * outer√inner * π^piMul * e^eMul
 *
 * Extraído de MathEvaluator.h para evitar dependencias circulares.
 * Representa el resultado simbólico exacto de una evaluación matemática.
 *
 * Usado por: MathEvaluator, VariableManager, CalculationApp, etc.
 */

#pragma once

#include <cstdint>
#include <string>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// ExactVal — Valor exacto: (num/den) * outer√inner
//
// Representación unificada para enteros, fracciones, radicales y
// combinaciones. Cuando inner==1 y outer==1, es una fracción pura.
// Cuando inner>1, contiene un radical simplificado.
// ════════════════════════════════════════════════════════════════════════════
struct ExactVal {
    int64_t num   = 0;    ///< Numerador de la parte racional
    int64_t den   = 1;    ///< Denominador (siempre > 0)
    int64_t outer = 1;    ///< Coeficiente fuera del radical (outer√inner)
    int64_t inner = 1;    ///< Radicando simplificado (1 = sin radical)

    int8_t piMul  = 0;    ///< Multiplicador de π (ej. 3π → piMul=1, num=3)
    int8_t eMul   = 0;    ///< Multiplicador de e (ej. e² → eMul=2)

    bool ok    = true;    ///< false si hubo error de evaluación
    std::string error;    ///< Mensaje de error ("Math ERROR", etc.)

    // ── Constructores de conveniencia ────────────────────────────────────

    /// Valor racional puro: n/d
    static ExactVal fromFrac(int64_t n, int64_t d);

    /// Entero puro
    static ExactVal fromInt(int64_t n);

    /// Decimal (convierte a fracción con precisión limitada)
    static ExactVal fromDouble(double v);

    /// Radical simplificado: outer√inner
    static ExactVal fromRadical(int64_t outer, int64_t inner);

    /// Múltiplo de π: (n/d)*π^mul
    static ExactVal fromPi(int64_t n, int64_t d, int8_t mul = 1);

    /// Múltiplo de e: (n/d)*e^mul
    static ExactVal fromE(int64_t n, int64_t d, int8_t mul = 1);

    /// Error
    static ExactVal makeError(const std::string& msg);

    // ── Consultas ────────────────────────────────────────────────────────

    /// ¿Es una fracción pura (sin radical, sin π, sin e)?
    bool isRational() const { return ok && inner == 1 && piMul == 0 && eMul == 0; }

    /// ¿Es un entero (fracción con den=1, sin radical, sin constantes)?
    bool isInteger() const { return ok && inner == 1 && den == 1 && piMul == 0 && eMul == 0; }

    /// ¿Tiene parte radical?
    bool hasRadical() const { return ok && inner > 1; }

    /// ¿Tiene multiplicador de π?
    bool hasPi() const { return ok && piMul != 0; }

    /// ¿Tiene multiplicador de e?
    bool hasE() const { return ok && eMul != 0; }

    /// ¿Tiene alguna constante (π o e)?
    bool hasConstant() const { return hasPi() || hasE(); }

    /// ¿Es exactamente cero?
    bool isZero() const { return ok && num == 0 && (inner == 1 || outer == 0); }

    /// ¿Es un resultado irracional (π, e, radical no perfecto)?
    bool isIrrational() const { return ok && (hasRadical() || hasPi() || hasE()); }

    /// Convierte a double
    double toDouble() const;

    // ── Simplificación ───────────────────────────────────────────────────

    /// Reduce la fracción (divide num y den por su GCD, signo en num)
    void simplify();

    /// Simplifica el radical: extrae factores cuadrados perfectos
    void simplifyRadical();
};

// ════════════════════════════════════════════════════════════════════════════
// Aritmética de ExactVal
// ════════════════════════════════════════════════════════════════════════════

/// Suma de dos ExactVal (sólo racionales puros; si tienen radicales,
/// se recurre a aproximación decimal → fracción)
ExactVal exactAdd(const ExactVal& a, const ExactVal& b);

/// Resta
ExactVal exactSub(const ExactVal& a, const ExactVal& b);

/// Multiplicación
ExactVal exactMul(const ExactVal& a, const ExactVal& b);

/// División
ExactVal exactDiv(const ExactVal& a, const ExactVal& b);

/// Negación
ExactVal exactNeg(const ExactVal& a);

/// Potencia entera (exponente entero)
ExactVal exactPow(const ExactVal& base, const ExactVal& exp);

/// Raíz cuadrada exacta
ExactVal exactSqrt(const ExactVal& a);

} // namespace vpam
