/**
 * MathEvaluator.h — Evaluador Simbólico + Decimal del AST
 *
 * Fase 4+5 del Motor V.P.A.M.
 *
 * Recorre recursivamente el AST (MathAST.h) y produce un resultado
 * exacto (fracción irreducible a/b + radical c√d, múltiplos de π/e)
 * o decimal (double / alta precisión).
 *
 * Capacidades:
 *   · Evaluación recursiva por tipo de nodo (incluye Function, LogBase, Constant)
 *   · Aritmética exacta de fracciones (GCD/LCM)
 *   · Simplificación de radicales (√12 → 2√3)
 *   · Multiplicadores de π y e en ExactVal (CAS-level)
 *   · Funciones trigonométricas: sin, cos, tan, arcsin, arccos, arctan
 *   · Funciones logarítmicas: ln, log, log_base
 *   · Modo ángulo: DEG / RAD
 *   · 3 estados S⇔D: Simbólico → Periódico → Extendido
 *   · División Larga Procedural para decimales de alta precisión
 *   · Constantes π/e con hasta 1000 dígitos (Constants.h / PROGMEM)
 *   · Errores: "Math ERROR" para dominios inválidos, etc.
 *
 * Representación interna:
 *   ExactVal modela: (num/den) * outer√inner * π^piMul * e^eMul
 *   Cuando piMul=0 y eMul=0, sin constantes especiales.
 *   Ejemplo: 3π → num=3, den=1, piMul=1
 *   Ejemplo: e²/6 → num=1, den=6, eMul=2
 *
 * Generación de AST resultado:
 *   resultToAST()          → Simbólico (Estado 1)
 *   resultToPeriodicAST()  → Periódico con overline (Estado 2)
 *   resultToExtendedAST()  → Decimal extendido (Estado 3)
 *   resultToDecimalAST()   → Decimal simple (legacy, 10 dígitos)
 *
 * Dependencias: MathAST.h, Constants.h (C++ estándar, sin LVGL).
 */

#pragma once

#include "MathAST.h"
#include "ExactVal.h"
#include "Constants.h"
#include <cstdint>
#include <string>
#include <cmath>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// AngleMode — Modo de ángulo para funciones trigonométricas
// ════════════════════════════════════════════════════════════════════════════
enum class AngleMode : uint8_t {
    DEG,   ///< Grados (por defecto)
    RAD,   ///< Radianes
};

/// Variable global de modo de ángulo (accesible desde cualquier módulo)
extern AngleMode g_angleMode;

// ════════════════════════════════════════════════════════════════════════════
// ResultMode — Estado de visualización del resultado (3-state S⇔D)
// ════════════════════════════════════════════════════════════════════════════
enum class ResultMode : uint8_t {
    Symbolic,   ///< Estado 1: Fracción/radical/π/e simbólico
    Periodic,   ///< Estado 2: Decimal periódico o ~12 dígitos para irracionales
    Extended,   ///< Estado 3: Decimal extendido con scroll (200-500 chars)
};

// ════════════════════════════════════════════════════════════════════════════
// MathEvaluator — Evaluador recursivo del AST
// ════════════════════════════════════════════════════════════════════════════
class MathEvaluator {
public:
    MathEvaluator() = default;

    // ── Evaluación principal ─────────────────────────────────────────────

    /**
     * Evalúa recursivamente el AST completo.
     * @param root  Nodo raíz (normalmente un NodeRow).
     * @return      Resultado exacto o error.
     */
    ExactVal evaluate(const MathNode* root) const;

    // ── Generación de AST resultado (3 estados S⇔D) ─────────────────────

    /**
     * Estado 1 — Simbólico: fracción/radical/π/e.
     */
    static NodePtr resultToAST(const ExactVal& val);

    /**
     * Estado 2 — Periódico: 0.̄3̄ para racionales, ~12 dígitos para irracionales.
     * Genera NodePeriodicDecimal para fracciones con período.
     */
    static NodePtr resultToPeriodicAST(const ExactVal& val);

    /**
     * Estado 3 — Extendido: decimal largo (200-500 dígitos) con scroll.
     * @param maxDigits  Número de dígitos decimales a generar.
     */
    static NodePtr resultToExtendedAST(const ExactVal& val, int maxDigits = 200);

    /**
     * Legacy — Decimal simple (~10 dígitos significativos).
     */
    static NodePtr resultToDecimalAST(const ExactVal& val, int precision = 10);

    /**
     * Genera un AST de error: NodeRow con "Math ERROR".
     */
    static NodePtr errorToAST(const std::string& msg);

    // ── Factorización en primos ──────────────────────────────────────────

    /**
     * Descompone un entero positivo en sus factores primos.
     * Devuelve un AST con formato: p₁^a₁ × p₂^a₂ × ...
     * Si el valor no es un entero positivo > 1, devuelve un AST de error.
     */
    static NodePtr factorizeToAST(const ExactVal& val);

private:
    // ── Evaluación por tipo de nodo ──────────────────────────────────────
    ExactVal evalRow(const NodeRow* row) const;
    ExactVal evalNumber(const NodeNumber* node) const;
    ExactVal evalFraction(const NodeFraction* node) const;
    ExactVal evalPower(const NodePower* node) const;
    ExactVal evalRoot(const NodeRoot* node) const;
    ExactVal evalParen(const NodeParen* node) const;
    ExactVal evalFunction(const NodeFunction* node) const;
    ExactVal evalLogBase(const NodeLogBase* node) const;
    ExactVal evalConstant(const NodeConstant* node) const;
    ExactVal evalVariable(const NodeVariable* node) const;

    /**
     * Evalúa una secuencia de términos y operadores con precedencia.
     * Implementa: multiplicación antes que suma/resta.
     * @param terms  Valores evaluados de los tokens.
     * @param ops    Operadores entre los términos.
     */
    ExactVal evalWithPrecedence(
        const std::vector<ExactVal>& terms,
        const std::vector<OpKind>& ops) const;

    // ── Helpers trigonométricos ──────────────────────────────────────────
    static double degToRad(double deg);
    static double radToDeg(double rad);
};

// ════════════════════════════════════════════════════════════════════════════
// Utilidades matemáticas
// ════════════════════════════════════════════════════════════════════════════

/// Máximo común divisor (siempre positivo)
int64_t gcd(int64_t a, int64_t b);

/// Mínimo común múltiplo
int64_t lcm(int64_t a, int64_t b);

/// ¿Es un cuadrado perfecto?
bool isPerfectSquare(int64_t n);

/// Raíz cuadrada entera (floor)
int64_t isqrt(int64_t n);

} // namespace vpam
