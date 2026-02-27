/**
 * KeyboardManager.cpp — Implementación de la máquina de estados de modificadores
 */

#include "KeyboardManager.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Singleton
// ════════════════════════════════════════════════════════════════════════════

KeyboardManager& KeyboardManager::instance() {
    static KeyboardManager inst;
    return inst;
}

KeyboardManager::KeyboardManager()
    : _state(ModifierState::MOD_NONE)
{
}

// ════════════════════════════════════════════════════════════════════════════
// pressShift — Transiciones al pulsar SHIFT
// ════════════════════════════════════════════════════════════════════════════

void KeyboardManager::pressShift() {
    switch (_state) {
        case ModifierState::MOD_NONE:
            _state = ModifierState::MOD_SHIFT;
            break;
        case ModifierState::MOD_SHIFT:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_ALPHA:
            // ALPHA activo + SHIFT → SHIFT_LOCK
            _state = ModifierState::MOD_SHIFT_LOCK;
            break;
        case ModifierState::MOD_SHIFT_LOCK:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_ALPHA_LOCK:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_STORE:
            _state = ModifierState::MOD_NONE;
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// pressAlpha — Transiciones al pulsar ALPHA
// ════════════════════════════════════════════════════════════════════════════

void KeyboardManager::pressAlpha() {
    switch (_state) {
        case ModifierState::MOD_NONE:
            _state = ModifierState::MOD_ALPHA;
            break;
        case ModifierState::MOD_ALPHA:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_SHIFT:
            // SHIFT activo + ALPHA → ALPHA_LOCK
            _state = ModifierState::MOD_ALPHA_LOCK;
            break;
        case ModifierState::MOD_ALPHA_LOCK:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_SHIFT_LOCK:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_STORE:
            _state = ModifierState::MOD_NONE;
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// pressStore — Activar modo STO
// ════════════════════════════════════════════════════════════════════════════

void KeyboardManager::pressStore() {
    _state = ModifierState::MOD_STORE;
}

// ════════════════════════════════════════════════════════════════════════════
// consumeModifier — Consume el modificador para modos de una sola acción
// ════════════════════════════════════════════════════════════════════════════

void KeyboardManager::consumeModifier() {
    switch (_state) {
        case ModifierState::MOD_SHIFT:
        case ModifierState::MOD_ALPHA:
            _state = ModifierState::MOD_NONE;
            break;
        case ModifierState::MOD_SHIFT_LOCK:
        case ModifierState::MOD_ALPHA_LOCK:
            // Lock permanece activo
            break;
        case ModifierState::MOD_STORE:
            // STO se cancela explícitamente, no por consumo normal
            break;
        case ModifierState::MOD_NONE:
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// reset — Fuerza MOD_NONE
// ════════════════════════════════════════════════════════════════════════════

void KeyboardManager::reset() {
    _state = ModifierState::MOD_NONE;
}

// ════════════════════════════════════════════════════════════════════════════
// indicatorText — Texto corto para el indicador del header
// ════════════════════════════════════════════════════════════════════════════

const char* KeyboardManager::indicatorText() const {
    switch (_state) {
        case ModifierState::MOD_NONE:        return "";
        case ModifierState::MOD_SHIFT:       return "S";
        case ModifierState::MOD_ALPHA:       return "A";
        case ModifierState::MOD_SHIFT_LOCK:  return "S-LOCK";
        case ModifierState::MOD_ALPHA_LOCK:  return "A-LOCK";
        case ModifierState::MOD_STORE:       return "STO";
    }
    return "";
}

} // namespace vpam
