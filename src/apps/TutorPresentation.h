// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../math/tutor/TeachingPlan.h"

namespace tutorview {
enum class FormulaKind : uint8_t { Equation, Authored, Conditions, StandardQuadratic,
    Coefficients, DiscriminantDefinition, DiscriminantValues, GeneralFormula,
    SubstitutedFormula, Operand, RowOperation, CoefficientEquation };
// Every displayed mathematical object names its immutable checked source.
struct FormulaRef {
    FormulaKind kind = FormulaKind::Equation;
    uint16_t state = 0, step = 0;
    uint8_t branch = 0, row = 0;
};
} // namespace tutorview
