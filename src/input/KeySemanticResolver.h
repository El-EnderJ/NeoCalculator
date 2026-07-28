#pragma once

#include "KeyboardManager.h"
#include "generated/ProductionKeypadMap.generated.h"

namespace numos::input {

struct ResolvedKey {
    bool dispatch = false;
    KeyCode code = KeyCode::NONE;
    SemanticId semantic = SemanticId::none;
    const char* text = "";
    KeyPlane plane = KeyPlane::Primary;
};

class KeySemanticResolver {
public:
    static ResolvedKey resolve(KeyCode physicalCode,
                               InputContext context,
                               KeyAction action);
    static void reset();

private:
    static std::size_t mappingIndex(KeyCode physicalCode);
    static const KeyPlaneDefinition& definition(InputContext context,
                                                std::size_t index,
                                                KeyPlane plane);
};

} // namespace numos::input
