#include "KeySemanticResolver.h"

namespace numos::input {

namespace {

constexpr std::size_t kInvalidIndex = kProductionKeypadMap.size();

KeyPlane activePlane(const vpam::KeyboardManager& modifiers) {
    if (modifiers.isShift() && modifiers.isAlpha()) {
        return KeyPlane::ShiftAlpha;
    }
    if (modifiers.isShift()) return KeyPlane::Shift;
    if (modifiers.isAlpha()) return KeyPlane::Alpha;
    return KeyPlane::Primary;
}

bool clearsModifiers(const SemanticId semantic) {
    return semantic == SemanticId::go_home ||
           semantic == SemanticId::deep_sleep_off;
}

} // namespace

std::size_t KeySemanticResolver::mappingIndex(const KeyCode physicalCode) {
    for (std::size_t i = 0; i < kProductionKeypadMap.size(); ++i) {
        if (kProductionKeypadMap[i].keyCode == physicalCode) return i;
    }
    return kInvalidIndex;
}

const KeyPlaneDefinition& KeySemanticResolver::definition(
    const InputContext context,
    const std::size_t index,
    const KeyPlane plane) {
    const auto planeIndex = static_cast<std::size_t>(plane);
    switch (context) {
        case InputContext::Math:
            return kMathPlaneDefinitions[index][planeIndex];
        case InputContext::Code:
            return kCodePlaneDefinitions[index][planeIndex];
        case InputContext::Text:
            return kTextPlaneDefinitions[index][planeIndex];
    }
    return kMathPlaneDefinitions[index][planeIndex];
}

ResolvedKey KeySemanticResolver::resolve(const KeyCode physicalCode,
                                         const InputContext context,
                                         const KeyAction action) {
    auto& modifiers = vpam::KeyboardManager::instance();
    if (action == KeyAction::PRESS) {
        if (physicalCode == KeyCode::SHIFT) {
            modifiers.pressShift();
            return {};
        }
        if (physicalCode == KeyCode::ALPHA) {
            modifiers.pressAlpha();
            return {};
        }
    }

    const std::size_t index = mappingIndex(physicalCode);
    if (index == kInvalidIndex) {
        return {true, physicalCode, SemanticId::none, "",
                KeyPlane::Primary};
    }

    const KeyPlane plane = activePlane(modifiers);
    const auto& resolved = definition(context, index, plane);
    ResolvedKey out{
        true,
        // WHY: an implemented semantic with no legacy KeyCode must remain a
        // semantic-only event. Falling back to the physical primary code would
        // turn unimplemented SHIFT actions into the wrong primary operation
        // (for example OFF into AC or summation into addition).
        resolved.legacyCode,
        resolved.semantic,
        resolved.text,
        plane,
    };

    if (action == KeyAction::PRESS && resolved.consumesModifier) {
        modifiers.consumeForPlane(
            plane == KeyPlane::Shift || plane == KeyPlane::ShiftAlpha,
            plane == KeyPlane::Alpha || plane == KeyPlane::ShiftAlpha);
    }
    if (action == KeyAction::PRESS && clearsModifiers(resolved.semantic)) {
        modifiers.reset();
    }
    return out;
}

void KeySemanticResolver::reset() {
    vpam::KeyboardManager::instance().reset();
}

} // namespace numos::input
