/**
 * VariableManager.h — Gestor Global de Variables (Singleton)
 *
 * Almacena variables algebraicas como ExactVal para preservar el estado
 * simbólico exacto entre evaluaciones, apps y reinicios.
 *
 * Variables soportadas:
 *   · Ans     — Último resultado (char code: '#')
 *   · PreAns  — Penúltimo resultado (char code: '$')
 *   · A-F     — Variables de usuario generales
 *   · x, y, z — Variables de función/gráfica
 *
 * Persistencia:
 *   · saveToFlash()  — Serializa a /vars.dat en LittleFS
 *   · loadFromFlash() — Carga desde /vars.dat al arrancar
 *
 * Forma de acceso:
 *   VariableManager::instance().getVariable('x')
 *   VariableManager::instance().setVariable('A', val)
 *   VariableManager::instance().updateAns(newVal)
 *
 * Dependencias: MathEvaluator.h (ExactVal), LittleFS (ESP32).
 */

#pragma once

#include "ExactVal.h"
#include <array>
#include <cstdint>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Códigos especiales de variable
// ════════════════════════════════════════════════════════════════════════════
static constexpr char VAR_ANS     = '#';   ///< Ans (último resultado)
static constexpr char VAR_PREANS  = '$';   ///< PreAns (penúltimo resultado)

// ════════════════════════════════════════════════════════════════════════════
// VariableManager — Singleton global de variables
// ════════════════════════════════════════════════════════════════════════════
class VariableManager {
public:
    /// Acceso al singleton
    static VariableManager& instance();

    // ── Variables ────────────────────────────────────────────────────────

    /**
     * Obtiene el valor de una variable.
     * @param name  Identificador: 'A'-'F', 'x','y','z', '#' (Ans), '$' (PreAns)
     * @return      ExactVal almacenado (0 por defecto si no existe).
     */
    ExactVal getVariable(char name) const;

    /**
     * Asigna un valor a una variable.
     * @param name  Identificador ('A'-'F', 'x','y','z', '#', '$')
     * @param val   ExactVal a guardar.
     */
    void setVariable(char name, const ExactVal& val);

    /**
     * Actualiza Ans: el valor actual de Ans pasa a PreAns,
     * y newVal se convierte en el nuevo Ans.
     * @param newVal  Nuevo resultado a almacenar como Ans.
     */
    void updateAns(const ExactVal& newVal);

    /**
     * Obtiene el valor de Ans directamente.
     */
    ExactVal getAns() const { return getVariable(VAR_ANS); }

    /**
     * Obtiene el valor de PreAns directamente.
     */
    ExactVal getPreAns() const { return getVariable(VAR_PREANS); }

    // ── Persistencia (LittleFS) ──────────────────────────────────────────

    /**
     * Serializa todas las variables a /vars.dat en LittleFS.
     * @return true si se guardó correctamente.
     */
    bool saveToFlash();

    /**
     * Carga las variables desde /vars.dat en LittleFS.
     * @return true si se cargó correctamente.
     */
    bool loadFromFlash();

    // ── Utilidades ───────────────────────────────────────────────────────

    /**
     * Reinicia todas las variables a 0.
     */
    void resetAll();

    /**
     * Convierte un char de variable a texto legible.
     * '#' → "Ans", '$' → "PreAns", 'x' → "x", 'A' → "A", etc.
     */
    static const char* variableLabel(char name);

    /**
     * ¿Es un identificador de variable válido?
     */
    static bool isValidName(char name);

private:
    VariableManager();
    ~VariableManager() = default;

    // No copiable / no movible
    VariableManager(const VariableManager&) = delete;
    VariableManager& operator=(const VariableManager&) = delete;

    // ── Almacenamiento ──────────────────────────────────────────────────

    /// Índice de variable → slot en el array
    /// Mapeo: A=0, B=1, C=2, D=3, E=4, F=5, x=6, y=7, z=8, Ans=9, PreAns=10
    static constexpr int NUM_VARS = 11;

    /// Convierte char → índice interno (-1 si inválido)
    static int nameToIndex(char name);

    /// Variables almacenadas
    std::array<ExactVal, NUM_VARS> _vars;

    // ── Ruta del archivo de persistencia ─────────────────────────────────
    static constexpr const char* FLASH_PATH = "/vars.dat";

    // ── Serialización ────────────────────────────────────────────────────

    /// Formato binario: header(4 bytes "VR01") + count(1 byte) + [ExactVal serializado]*count
    /// Cada ExactVal serializado: num(8) + den(8) + outer(8) + inner(8) + piMul(1) + eMul(1) = 34 bytes
    static constexpr uint32_t MAGIC = 0x56523031; // "VR01"
    static constexpr int EXACTVAL_SIZE = 34;       // Bytes por ExactVal serializado

    void serializeExactVal(uint8_t* buf, const ExactVal& val) const;
    ExactVal deserializeExactVal(const uint8_t* buf) const;
};

} // namespace vpam
