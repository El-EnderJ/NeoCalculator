/*
 * NumOS logical modifier state.
 *
 * SHIFT and ALPHA are independent three-state machines. Keeping them
 * independent is what makes the Revision-C SHIFT+ALPHA plane representable.
 */
#pragma once

#include <cstdint>

namespace vpam {

enum class ModifierPhase : uint8_t {
    Off,
    Once,
    Locked,
};

// Compatibility summary for existing status/debug callers.
enum class ModifierState : uint8_t {
    MOD_NONE,
    MOD_SHIFT,
    MOD_ALPHA,
    MOD_SHIFT_LOCK,
    MOD_ALPHA_LOCK,
    MOD_STORE,
    MOD_SHIFT_ALPHA,
};

class KeyboardManager {
public:
    static KeyboardManager& instance();

    ModifierState state() const;
    ModifierPhase shiftPhase() const { return _shift; }
    ModifierPhase alphaPhase() const { return _alpha; }

    bool isShift() const { return _shift != ModifierPhase::Off; }
    bool isAlpha() const { return _alpha != ModifierPhase::Off; }
    bool isLocked() const {
        return _shift == ModifierPhase::Locked ||
               _alpha == ModifierPhase::Locked;
    }
    bool isStore() const { return _store; }

    void pressShift();
    void longPressShift();
    void pressAlpha();
    void longPressAlpha();
    void pressStore();

    // Compatibility behavior: consume all active one-shot modifiers.
    void consumeModifier();

    // Revision-C behavior: consume only one-shot modifiers used by the plane.
    void consumeForPlane(bool usesShift, bool usesAlpha);

    void reset();
    const char* indicatorText() const;

private:
    KeyboardManager();
    ~KeyboardManager() = default;
    KeyboardManager(const KeyboardManager&) = delete;
    KeyboardManager& operator=(const KeyboardManager&) = delete;

    ModifierPhase _shift;
    ModifierPhase _alpha;
    bool _store;
};

} // namespace vpam
