/**
 * KeyboardManager.h — Máquina de Estados de Modificadores de Teclado
 *
 * Gestiona los estados globales del teclado de la calculadora:
 *   · MOD_NONE        — Estado normal (sin modificador)
 *   · MOD_SHIFT       — SHIFT activo (una pulsación)
 *   · MOD_ALPHA       — ALPHA activo (una pulsación)
 *   · MOD_SHIFT_LOCK  — SHIFT bloqueado (ALPHA mientras SHIFT activo)
 *   · MOD_ALPHA_LOCK  — ALPHA bloqueado (SHIFT mientras ALPHA activo)
 *   · MOD_STORE       — Modo STO (esperando tecla A-F para guardar)
 *
 * Transiciones estilo Casio/NumWorks:
 *   SHIFT suelto         → SHIFT activo (se desactiva al pulsar cualquier otra tecla)
 *   ALPHA suelto         → ALPHA activo (se desactiva al pulsar cualquier otra tecla)
 *   SHIFT → ALPHA        → ALPHA_LOCK
 *   ALPHA → SHIFT        → SHIFT_LOCK
 *   En LOCK + misma tecla del LOCK → MOD_NONE
 *   STO                  → MOD_STORE (se cancela al ejecutar o con AC/SHIFT)
 *
 * Singleton: KeyboardManager::instance()
 */

#pragma once

#include <cstdint>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// ModifierState — Estado actual del modificador de teclado
// ════════════════════════════════════════════════════════════════════════════
enum class ModifierState : uint8_t {
    MOD_NONE,          ///< Sin modificador
    MOD_SHIFT,         ///< SHIFT activo (una sola acción)
    MOD_ALPHA,         ///< ALPHA activo (una sola acción)
    MOD_SHIFT_LOCK,    ///< SHIFT bloqueado (se mantiene hasta desbloqueo)
    MOD_ALPHA_LOCK,    ///< ALPHA bloqueado (se mantiene hasta desbloqueo)
    MOD_STORE,         ///< Modo STO (esperando variable para guardar)
};

// ════════════════════════════════════════════════════════════════════════════
// KeyboardManager — Singleton global de modificadores de teclado
// ════════════════════════════════════════════════════════════════════════════
class KeyboardManager {
public:
    /// Acceso al singleton
    static KeyboardManager& instance();

    // ── Estado ───────────────────────────────────────────────────────────

    /// Estado actual del modificador
    ModifierState state() const { return _state; }

    /// ¿Está SHIFT activo (simple o lock)?
    bool isShift() const {
        return _state == ModifierState::MOD_SHIFT ||
               _state == ModifierState::MOD_SHIFT_LOCK;
    }

    /// ¿Está ALPHA activo (simple o lock)?
    bool isAlpha() const {
        return _state == ModifierState::MOD_ALPHA ||
               _state == ModifierState::MOD_ALPHA_LOCK;
    }

    /// ¿Está en un lock (SHIFT o ALPHA)?
    bool isLocked() const {
        return _state == ModifierState::MOD_SHIFT_LOCK ||
               _state == ModifierState::MOD_ALPHA_LOCK;
    }

    /// ¿Está en modo STO?
    bool isStore() const { return _state == ModifierState::MOD_STORE; }

    // ── Transiciones ─────────────────────────────────────────────────────

    /**
     * Procesa la pulsación del botón SHIFT.
     * Transiciones:
     *   MOD_NONE   → MOD_SHIFT
     *   MOD_SHIFT  → MOD_NONE
     *   MOD_ALPHA  → MOD_SHIFT_LOCK  (ALPHA estaba activo, SHIFT lo bloquea como shift)
     *   MOD_SHIFT_LOCK → MOD_NONE
     *   MOD_ALPHA_LOCK → MOD_NONE    (desbloquea)
     *   MOD_STORE  → MOD_NONE        (cancela STO)
     */
    void pressShift();

    /**
     * Procesa la pulsación del botón ALPHA.
     * Transiciones:
     *   MOD_NONE   → MOD_ALPHA
     *   MOD_ALPHA  → MOD_NONE
     *   MOD_SHIFT  → MOD_ALPHA_LOCK  (SHIFT estaba activo, ALPHA lo bloquea como alpha)
     *   MOD_ALPHA_LOCK → MOD_NONE
     *   MOD_SHIFT_LOCK → MOD_NONE    (desbloquea)
     *   MOD_STORE  → MOD_NONE        (cancela STO)
     */
    void pressAlpha();

    /**
     * Activa el modo STO.
     * El modo STORE espera que el usuario pulse una tecla de variable
     * (A-F) para ejecutar la asignación.
     */
    void pressStore();

    /**
     * Consume el modificador después de una acción (para modos no-lock).
     * Si el estado es MOD_SHIFT o MOD_ALPHA, vuelve a MOD_NONE.
     * Si es un LOCK, permanece.
     * Si es MOD_STORE, permanece (se cancela explícitamente).
     */
    void consumeModifier();

    /**
     * Fuerza el retorno a MOD_NONE.
     * Usado por AC para resetear todo.
     */
    void reset();

    // ── Texto del indicador para el header ──────────────────────────────

    /**
     * Devuelve una cadena corta para mostrar en el header de la app.
     * Ej: "S", "A", "SL", "AL", "STO", "" (para NONE)
     */
    const char* indicatorText() const;

private:
    KeyboardManager();
    ~KeyboardManager() = default;
    KeyboardManager(const KeyboardManager&) = delete;
    KeyboardManager& operator=(const KeyboardManager&) = delete;

    ModifierState _state;
};

} // namespace vpam
