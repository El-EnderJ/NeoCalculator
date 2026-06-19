#pragma once

#include <cstddef>

#include "MathAST.h"

namespace vpam {

struct MathRenderVisualCase {
    const char* id;
    const char* label;
    MathStyle style;
    NodePtr (*build)();
};

const MathRenderVisualCase* mathRenderVisualCases();
std::size_t mathRenderVisualCaseCount();

} // namespace vpam
