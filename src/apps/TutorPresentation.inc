// SPDX-License-Identifier: GPL-3.0-or-later
// Private to EquationsApp.cpp. Builds existing VPAM nodes during page changes;
// it adds no parser, arithmetic engine, renderer geometry or hot-path allocation.
namespace tutorview {
using namespace vpam;
using namespace numos;
using namespace numos::tutor;

NodePtr rowOf(NodePtr a, NodePtr b = nullptr, NodePtr c = nullptr) {
    auto result = makeRow(); auto* row = static_cast<NodeRow*>(result.get());
    row->appendChild(std::move(a)); row->appendChild(std::move(b)); row->appendChild(std::move(c));
    return result;
}
NodePtr relation(NodePtr left, NodePtr right, OpKind op = OpKind::Eq) {
    return rowOf(std::move(left), makeRelation(op), std::move(right));
}
bool signedInteger(const NodePtr& node) {
    return node && node->type() == NodeType::Number &&
           !static_cast<const NodeNumber*>(node.get())->value().empty() &&
           static_cast<const NodeNumber*>(node.get())->value()[0] == '-';
}
NodePtr factor(NodePtr node) {
    // WHY: a negative factor needs parentheses, even though a standalone signed
    // value does not. Compound rows also preserve their grouping in products.
    if (signedInteger(node) || (node && node->type() == NodeType::Row && node->childCount() > 1))
        return makeParen(std::move(node));
    return node;
}
NodePtr multiply(NodePtr left, NodePtr right, ProductNotation notation = ProductNotation::ScalarNatural) {
    auto result = rowOf(factor(std::move(left)), makeOperator(OpKind::Mul), factor(std::move(right)));
    applyGeneratedProductNotation(result.get(), notation);
    return result;
}
NodePtr multiplyThree(NodePtr a, NodePtr b, NodePtr c, ProductNotation notation = ProductNotation::ScalarNatural) {
    auto product = multiply(std::move(a), std::move(b), ProductNotation::Explicit);
    auto* row = static_cast<NodeRow*>(product.get());
    row->appendChild(makeOperator(OpKind::Mul)); row->appendChild(factor(std::move(c)));
    applyGeneratedProductNotation(product.get(), notation);
    return product;
}
NodePtr square(NodePtr base) {
    return makePower(factor(std::move(base)), makeNumber("2"));
}
void compactScalarSigns(NodePtr& node, unsigned depth = 0) {
    if (!node || depth > Limits::depth || node->type() != NodeType::Row) return;
    auto* row = static_cast<NodeRow*>(node.get());
    // WHY: unary signs are ordinary math atoms, not binary operations. The
    // existing Symbol node supplies the same STIX glyph with measured geometry
    // and ORD spacing. This local display conversion leaves every parenthesis
    // and every binary minus intact; it changes no renderer/layout formula.
    if (row->childCount() && row->child(0)->type() == NodeType::Operator) {
        const auto sign = static_cast<NodeOperator*>(row->child(0))->op();
        if (sign == OpKind::Sub || sign == OpKind::Add) {
            row->replaceChild(0, makeSymbol(sign == OpKind::Sub ? "-" : "+"));
        }
    }
}
void authoredSigns(MathNode* node,unsigned depth=0) {
    if(!node||depth>Limits::depth+8)return;
    if(node->type()==NodeType::Row) {
        auto* row=static_cast<NodeRow*>(node);
        for(int i=0;i<row->childCount();++i) {
            if((i==0||isEquality(row->child(i-1)))&&row->child(i)->type()==NodeType::Operator) {
                const auto sign=static_cast<NodeOperator*>(row->child(i))->op();
                if(sign==OpKind::Sub||sign==OpKind::Add)row->replaceChild(i,makeSymbol(sign==OpKind::Sub?"-":"+"));
            }
        }
    }
    for(int i=0;i<node->childCount();++i)authoredSigns(node->child(i),depth+1);
}
NodePtr converted(const EngineResultNode& n, unsigned depth = 0, ProductNotation notation = ProductNotation::ScalarNatural) {
    if (depth > Limits::depth) return nullptr;
    if (n.kind == EngineNodeKind::Integer || n.kind == EngineNodeKind::Decimal) {
        if (!n.text.empty() && n.text[0] == '-')
            return rowOf(makeSymbol("-"), makeNumber(n.text.substr(1)));
        return makeNumber(n.text);
    }
    if (n.kind == EngineNodeKind::Equation && n.children.size() == 2) {
        auto left = converted(n.children[0], depth + 1, notation), right = converted(n.children[1], depth + 1, notation);
        if (!left || !right) return nullptr;
        return relation(std::move(left), std::move(right));
    }
    if (n.kind == EngineNodeKind::Rational && n.children.size() == 2) {
        auto numerator = converted(n.children[0], depth + 1, notation), denominator = converted(n.children[1], depth + 1, notation);
        if (!numerator || !denominator) return nullptr;
        return makeFraction(std::move(numerator), std::move(denominator));
    }
    auto value = resultFormula(n, 0, notation);
    compactScalarSigns(value);
    return value;
}
NodePtr typedTerm(const std::string& source, unsigned& calls) {
    if (++calls > 16) throw std::bad_alloc();
    auto value = GiacEngine::instance().tutorFormula({source, "0"});
    if (!value.hasTree || value.tree.kind != EngineNodeKind::Equation || value.tree.children.size() != 2)
        throw std::bad_alloc();
    auto result = converted(value.tree.children[0]);
    // WHY: constructors accept empty slots. Fail before composing a formula so
    // a recoverable boundary allocation failure cannot publish missing math.
    if (!result) throw std::bad_alloc();
    return result;
}
NodePtr symbol(const char* value) { return makeSymbol(value); }
NodePtr variable(const std::string& value) {
    return value.size() == 1 ? makeVariable(value[0]) : makeSymbol(value);
}
NodePtr equation(const Equation& e, unsigned& calls, ProductNotation notation = ProductNotation::ScalarNatural) {
    if (++calls > 16) return nullptr;
    auto value = GiacEngine::instance().tutorFormula(e);
    return value.hasTree ? converted(value.tree, 0, notation) : nullptr;
}
// Semantic display role, never a user identifier or a Giac context binding.
enum class Quantity : uint8_t { Discriminant };
NodePtr quantity(Quantity) { return makeSymbol("\xCE\x94"); } // U+0394, Greek capital Delta
NodePtr rowName(unsigned n) {
    return makeSubscript(makeVariable('E'), makeNumber(std::to_string(n + 1)));
}
unsigned nodeCount(const MathNode* node, unsigned depth = 0) {
    if (!node) return 0;
    if (depth > Limits::depth + 8) return 1024;
    unsigned count = 1;
    for (int i = 0; i < node->childCount(); ++i) count += nodeCount(node->child(i), depth + 1);
    return count;
}

NodePtr build(const Derivation& d, FormulaRef ref, unsigned& calls) {
    if (ref.step >= d.steps.size()) return nullptr;
    const auto& step = d.steps[ref.step];
    if (step.verification != Verdict::Verified) return nullptr;
    const auto& name = d.input.variables[0];
    if (ref.kind == FormulaKind::Equation) {
        if (ref.state >= d.states.size() || ref.branch >= d.states[ref.state].branches.size()) return nullptr;
        const auto& branch = d.states[ref.state].branches[ref.branch];
        return ref.row < branch.equations.size() ? equation(branch.equations[ref.row], calls) : nullptr;
    }
    if (ref.kind == FormulaKind::Authored)
        return ref.row < d.input.authored.size() ? equation(d.input.authored[ref.row], calls, ProductNotation::Explicit) : nullptr;
    if (ref.kind == FormulaKind::Conditions) {
        if (ref.state >= d.states.size()) return nullptr;
        auto list = makeRow(); auto* row = static_cast<NodeRow*>(list.get());
        for (const auto& condition : d.states[ref.state].conditions) {
            auto term = typedTerm(condition.nonzero, calls);
            if (!term) return nullptr;
            if (row->childCount()) row->appendChild(makeSymbol("; "));
            row->appendChild(relation(std::move(term), makeNumber("0"), OpKind::Ne));
        }
        return list;
    }
    if (ref.kind == FormulaKind::Operand) {
        const bool subtract = std::string_view(messageKey(step.explanation)).find("equation.subtract") == 0;
        return typedTerm(subtract ? "-(" + step.operand + ")" : step.operand, calls);
    }
    if (ref.kind == FormulaKind::RowOperation) {
        if (step.rule == Rule::RowSwap)
            return rowOf(rowName(step.row), symbol("\xE2\x86\x94"), rowName(step.otherRow));
        NodePtr right;
        if (step.rule == Rule::RowScale)
            right = makeFraction(rowName(step.row), factor(typedTerm(step.operand, calls)));
        else {
            // The notation uses the exact signed row multiplier declared by the
            // checked operation, with parentheses preserving negative factors.
            const bool subtract = std::string_view(messageKey(step.explanation)).find("system.subtract") == 0;
            const auto amount = subtract ? "-(" + step.operand + ")" : step.operand;
            NodePtr term;
            if (step.explanation == Message::RowAddUnit || step.explanation == Message::RowSubtractUnit)
                term = rowName(step.otherRow);
            else term = multiply(typedTerm(amount, calls), rowName(step.otherRow), ProductNotation::Explicit);
            right = rowOf(rowName(step.row), makeOperator(subtract ? OpKind::Sub : OpKind::Add), std::move(term));
        }
        return rowOf(rowName(step.row), symbol("\xE2\x86\x90"), std::move(right));
    }
    if (step.rule != Rule::QuadraticFormula || step.auxiliaries.size() != 4) return nullptr;
    const auto& aux = step.auxiliaries;
    if (ref.kind == FormulaKind::CoefficientEquation) {
        // The verifier identifies these exact coefficients with lhs-rhs.
        // Display that checked polynomial with zero on the right, including
        // when the preceding state was a pure square such as x^2=-1.
        auto polynomial = typedTerm("(" + aux[0] + ")*(" + name + ")^2+(" +
                                    aux[1] + ")*(" + name + ")+(" + aux[2] + ")", calls);
        return relation(std::move(polynomial), makeNumber("0"));
    }
    if (ref.kind == FormulaKind::StandardQuadratic) {
        auto left = rowOf(multiply(makeVariable('a'), square(variable(name))), makeOperator(OpKind::Add),
                          multiply(makeVariable('b'), variable(name)));
        static_cast<NodeRow*>(left.get())->appendChild(makeOperator(OpKind::Add));
        static_cast<NodeRow*>(left.get())->appendChild(makeVariable('c'));
        return relation(std::move(left), makeNumber("0"));
    }
    if (ref.kind == FormulaKind::Coefficients) {
        auto row = makeRow(); auto* r = static_cast<NodeRow*>(row.get());
        for (unsigned i = 0; i < 3; ++i) {
            auto value = typedTerm(aux[i], calls); if (!value) return nullptr;
            if (i) r->appendChild(symbol(", "));
            r->appendChild(relation(makeVariable(char('a' + i)), std::move(value)));
        }
        return row;
    }
    if (ref.kind == FormulaKind::DiscriminantDefinition)
        return relation(quantity(Quantity::Discriminant), rowOf(square(makeVariable('b')), makeOperator(OpKind::Sub),
                            multiplyThree(makeNumber("4"), makeVariable('a'), makeVariable('c'))));
    if (ref.kind == FormulaKind::DiscriminantValues) {
        auto expression = rowOf(square(typedTerm(aux[1], calls)), makeOperator(OpKind::Sub),
                            multiplyThree(makeNumber("4"), typedTerm(aux[0], calls), typedTerm(aux[2], calls), ProductNotation::Explicit));
        auto result = relation(quantity(Quantity::Discriminant), std::move(expression));
        auto* r = static_cast<NodeRow*>(result.get());
        r->appendChild(makeRelation(OpKind::Eq)); r->appendChild(typedTerm(aux[3], calls));
        return result;
    }
    if (ref.kind == FormulaKind::GeneralFormula) {
        auto numerator = rowOf(makeSymbol("-"), makeVariable('b'), makeOperator(OpKind::PlusMinus));
        static_cast<NodeRow*>(numerator.get())->appendChild(makeRoot(quantity(Quantity::Discriminant)));
        return relation(variable(name), makeFraction(std::move(numerator), multiply(makeNumber("2"), makeVariable('a'))));
    }
    if (ref.kind == FormulaKind::SubstitutedFormula) {
        // Exact constant arithmetic is evaluated by the existing boundary.
        // This is checked coefficient substitution, never a second root solver.
        auto minusB = typedTerm("-(" + aux[1] + ")", calls);
        auto denominator = typedTerm("2*(" + aux[0] + ")", calls);
        auto discriminant = typedTerm(aux[3], calls);
        if (!minusB || !denominator || !discriminant) return nullptr;
        auto numerator = rowOf(std::move(minusB), makeOperator(OpKind::PlusMinus), makeRoot(std::move(discriminant)));
        return relation(variable(name), makeFraction(std::move(numerator), std::move(denominator)));
    }
    return nullptr;
}
} // namespace tutorview
