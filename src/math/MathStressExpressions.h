/*
 * NumOS - Phase 5 math render stress expressions.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "MathAST.h"

namespace vpam {

struct StressExpressionCase {
    const char* id;
    const char* label;
    MathStyle   style;
    NodePtr   (*build)();
};

struct StressExpressionCatalog {
    const StressExpressionCase* items;
    std::size_t count;
};

StressExpressionCatalog mathStressExpressionCases();

bool containsScriptLevel(const MathNode* node, uint8_t level);

} // namespace vpam
