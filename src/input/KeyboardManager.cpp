#include "KeyboardManager.h"

namespace vpam {

KeyboardManager& KeyboardManager::instance() {
    static KeyboardManager instance;
    return instance;
}

KeyboardManager::KeyboardManager()
    : _shift(ModifierPhase::Off)
    , _alpha(ModifierPhase::Off)
    , _store(false) {}

namespace {

ModifierPhase nextShortPress(const ModifierPhase phase) {
    switch (phase) {
        case ModifierPhase::Off:    return ModifierPhase::Once;
        case ModifierPhase::Once:   return ModifierPhase::Locked;
        case ModifierPhase::Locked: return ModifierPhase::Off;
    }
    return ModifierPhase::Off;
}

ModifierPhase nextLongPress(const ModifierPhase phase) {
    return phase == ModifierPhase::Locked
               ? ModifierPhase::Off
               : ModifierPhase::Locked;
}

} // namespace

void KeyboardManager::pressShift() {
    _store = false;
    _shift = nextShortPress(_shift);
}

void KeyboardManager::longPressShift() {
    _store = false;
    _shift = nextLongPress(_shift);
}

void KeyboardManager::pressAlpha() {
    _store = false;
    _alpha = nextShortPress(_alpha);
}

void KeyboardManager::longPressAlpha() {
    _store = false;
    _alpha = nextLongPress(_alpha);
}

void KeyboardManager::pressStore() {
    _store = true;
    _shift = ModifierPhase::Off;
    _alpha = ModifierPhase::Off;
}

void KeyboardManager::consumeModifier() {
    consumeForPlane(true, true);
}

void KeyboardManager::consumeForPlane(const bool usesShift,
                                      const bool usesAlpha) {
    if (usesShift && _shift == ModifierPhase::Once) {
        _shift = ModifierPhase::Off;
    }
    if (usesAlpha && _alpha == ModifierPhase::Once) {
        _alpha = ModifierPhase::Off;
    }
}

void KeyboardManager::reset() {
    _shift = ModifierPhase::Off;
    _alpha = ModifierPhase::Off;
    _store = false;
}

ModifierState KeyboardManager::state() const {
    if (_store) return ModifierState::MOD_STORE;
    if (isShift() && isAlpha()) return ModifierState::MOD_SHIFT_ALPHA;
    if (_shift == ModifierPhase::Locked) return ModifierState::MOD_SHIFT_LOCK;
    if (_alpha == ModifierPhase::Locked) return ModifierState::MOD_ALPHA_LOCK;
    if (_shift == ModifierPhase::Once) return ModifierState::MOD_SHIFT;
    if (_alpha == ModifierPhase::Once) return ModifierState::MOD_ALPHA;
    return ModifierState::MOD_NONE;
}

const char* KeyboardManager::indicatorText() const {
    if (_store) return "STO";
    if (isShift() && isAlpha()) {
        if (_shift == ModifierPhase::Locked &&
            _alpha == ModifierPhase::Locked) {
            return "S+A-LOCK";
        }
        return "S+A";
    }
    switch (state()) {
        case ModifierState::MOD_NONE:        return "";
        case ModifierState::MOD_SHIFT:       return "S";
        case ModifierState::MOD_ALPHA:       return "A";
        case ModifierState::MOD_SHIFT_LOCK:  return "S-LOCK";
        case ModifierState::MOD_ALPHA_LOCK:  return "A-LOCK";
        case ModifierState::MOD_STORE:       return "STO";
        case ModifierState::MOD_SHIFT_ALPHA: return "S+A";
    }
    return "";
}

} // namespace vpam
