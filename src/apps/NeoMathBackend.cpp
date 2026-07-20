#include "NeoMathBackend.h"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "../math/cas/OmniSolver.h"
#include "../math/cas/SymDiff.h"
#include "../math/cas/SymIntegrate.h"
#include "../math/cas/SymSimplify.h"
#include "../math/cas/SystemSolver.h"
#include "../math/giac/EngineContracts.h"

namespace {

constexpr int kNeoMathDepth = numos::enginecontract::kMaxTreeDepth;
constexpr int kNeoMathNodes = numos::enginecontract::kMaxTreeNodes;
constexpr size_t kNeoMathSource = numos::enginecontract::kMaxSourceBytes;

bool validIdentifier(const std::string& name) {
    return numos::enginecontract::isPlainIdentifier(name, 63);
}

bool parseInt64(const std::string& text, int64_t& out) {
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno == ERANGE || end != text.c_str() + text.size()) return false;
    out = static_cast<int64_t>(value);
    return true;
}

bool containsUnsupported(const numos::EngineResultNode& tree) {
    if (tree.kind == numos::EngineNodeKind::Unsupported) return true;
    for (const auto& child : tree.children)
        if (containsUnsupported(child)) return true;
    return false;
}

cas::SymFuncKind nativeFunctionKind(const std::string& name, bool& ok) {
    ok = true;
    if (name == "sin") return cas::SymFuncKind::Sin;
    if (name == "cos") return cas::SymFuncKind::Cos;
    if (name == "tan") return cas::SymFuncKind::Tan;
    if (name == "asin" || name == "arcsin") return cas::SymFuncKind::ArcSin;
    if (name == "acos" || name == "arccos") return cas::SymFuncKind::ArcCos;
    if (name == "atan" || name == "arctan") return cas::SymFuncKind::ArcTan;
    if (name == "ln" || name == "log") return cas::SymFuncKind::Ln;
    if (name == "log10") return cas::SymFuncKind::Log10;
    if (name == "exp") return cas::SymFuncKind::Exp;
    if (name == "abs") return cas::SymFuncKind::Abs;
    if (name == "integrate") return cas::SymFuncKind::Integral;
    ok = false;
    return cas::SymFuncKind::Sin;
}

} // namespace

NeoMathBackend::NeoMathBackend() = default;

const char* NeoMathBackend::engineName() const {
    return _engine == NeoMathEngine::Giac ? "giac" : "native";
}

void NeoMathBackend::setEngine(NeoMathEngine engine) {
    _engine = engine;
}

void NeoMathBackend::resetSession() {
    _engine = NeoMathEngine::Giac;
    _trees.clear();
    _lists.clear();
    _dicts.clear();
    _counts = {};
    _lastStatus = numos::MathEngineStatus::Unsupported;
    _plotCompileCount = 0;
}

void NeoMathBackend::beginRun() {
    // Values and environments are run-local in the current interpreter.
    _trees.clear();
    _lists.clear();
    _dicts.clear();
    _lastStatus = numos::MathEngineStatus::Unsupported;
}

uint32_t NeoMathBackend::count(NeoMathEngine engine,
                               NeoMathOperation operation) const {
    return _counts[static_cast<size_t>(engine)]
                  [static_cast<size_t>(operation)];
}

std::vector<NeoValue>* NeoMathBackend::ownList() {
    std::unique_ptr<std::vector<NeoValue>> list(
        new (std::nothrow) std::vector<NeoValue>());
    if (!list) return nullptr;
    auto* pointer = list.get();
    _lists.push_back(std::move(list));
    return pointer;
}

std::map<std::string, NeoValue>* NeoMathBackend::ownDict() {
    std::unique_ptr<std::map<std::string, NeoValue>> dict(
        new (std::nothrow) std::map<std::string, NeoValue>());
    if (!dict) return nullptr;
    auto* pointer = dict.get();
    _dicts.push_back(std::move(dict));
    return pointer;
}

void NeoMathBackend::note(NeoMathOperation operation,
                          numos::MathEngineStatus status) {
    ++_counts[static_cast<size_t>(_engine)][static_cast<size_t>(operation)];
    _lastStatus = status;
}

NeoMathResult NeoMathBackend::error(numos::MathEngineStatus status,
                                    const std::string& diagnostic) {
    _lastStatus = status;
    NeoMathResult result;
    result.status = status;
    result.diagnostic = diagnostic;
    result.value = NeoValue::makeError(diagnostic);
    return result;
}

const numos::EngineResultNode* NeoMathBackend::own(
    numos::EngineResultNode tree) {
    std::unique_ptr<numos::EngineResultNode> owned(
        new (std::nothrow) numos::EngineResultNode(std::move(tree)));
    if (!owned) return nullptr;
    const numos::EngineResultNode* ptr = owned.get();
    _trees.push_back(std::move(owned));
    return ptr;
}

NeoValue NeoMathBackend::valueFromTree(numos::EngineResultNode tree,
                                       bool unevaluated) {
    using numos::EngineNodeKind;
    if (tree.kind == EngineNodeKind::Integer) {
        int64_t value = 0;
        if (parseInt64(tree.text, value))
            return NeoValue::makeExact(cas::CASRational::fromInt(value));
    }
    if (tree.kind == EngineNodeKind::Decimal) {
        char* end = nullptr;
        const double value = std::strtod(tree.text.c_str(), &end);
        if (end == tree.text.c_str() + tree.text.size())
            return NeoValue::makeNumber(value);
    }
    if (tree.kind == EngineNodeKind::Rational &&
        tree.children.size() == 2) {
        int64_t numerator = 0;
        int64_t denominator = 0;
        if (tree.children[0].kind == EngineNodeKind::Integer &&
            tree.children[1].kind == EngineNodeKind::Integer &&
            parseInt64(tree.children[0].text, numerator) &&
            parseInt64(tree.children[1].text, denominator) &&
            denominator != 0) {
            return NeoValue::makeExact(
                cas::CASRational::fromFrac(numerator, denominator));
        }
    }
    if (tree.kind == EngineNodeKind::List) {
        const numos::EngineResultNode* root = own(std::move(tree));
        if (!root) return NeoValue::makeError("Neo math result allocation failed");
        auto* list = ownList();
        if (!list) return NeoValue::makeError("Neo list allocation failed");
        list->reserve(root->children.size());
        for (const auto& child : root->children) {
            numos::EngineResultNode copy = child;
            list->push_back(valueFromTree(std::move(copy), false));
        }
        return NeoValue::makeList(list);
    }
    const bool opaque = containsUnsupported(tree);
    const numos::EngineResultNode* ptr = own(std::move(tree));
    return ptr ? NeoValue::makeMath(ptr, unevaluated, opaque)
               : NeoValue::makeError("Neo math result allocation failed");
}

void NeoMathBackend::restoreSymbols(numos::EngineResultNode& tree,
                                    const SymbolMap& symbols) const {
    if (tree.kind == numos::EngineNodeKind::Symbol) {
        for (const auto& symbol : symbols) {
            if (tree.text == symbol.second) {
                tree.text = symbol.first;
                break;
            }
        }
    }
    for (auto& child : tree.children) restoreSymbols(child, symbols);
}

NeoMathResult NeoMathBackend::resultFromGiac(
    numos::StructuredEngineResult result, const SymbolMap& symbols,
    bool unevaluated) {
    NeoMathResult out;
    out.status = result.base.status;
    out.diagnostic = result.base.diagnostic;
    out.unevaluated = unevaluated;
    if (!result.base.ok()) {
        out.value = NeoValue::makeError(out.diagnostic);
        return out;
    }
    if (!result.hasTree) {
        numos::EngineResultNode opaque;
        opaque.kind = numos::EngineNodeKind::Unsupported;
        opaque.text = result.base.exactText;
        out.value = valueFromTree(std::move(opaque), unevaluated);
        return out;
    }
    restoreSymbols(result.tree, symbols);
    out.value = valueFromTree(std::move(result.tree), unevaluated);
    return out;
}

bool NeoMathBackend::serializeTree(const numos::EngineResultNode& tree,
                                   std::string& out, SymbolMap& symbols,
                                   int depth, int& budget,
                                   std::string& diagnostic) const {
    using numos::EngineNodeKind;
    if (depth > kNeoMathDepth || --budget < 0) {
        diagnostic = "Neo math expression exceeds structural limits";
        return false;
    }
    auto child = [&](size_t index, std::string& target) {
        if (index >= tree.children.size()) {
            diagnostic = "malformed Neo math expression";
            return false;
        }
        return serializeTree(tree.children[index], target, symbols,
                             depth + 1, budget, diagnostic);
    };
    switch (tree.kind) {
        case EngineNodeKind::Integer:
        case EngineNodeKind::Decimal:
            out = tree.text;
            return !out.empty();
        case EngineNodeKind::Symbol: {
            if (!validIdentifier(tree.text)) {
                diagnostic = "invalid Neo symbol: " + tree.text;
                return false;
            }
            for (const auto& symbol : symbols) {
                if (symbol.first == tree.text) {
                    out = symbol.second;
                    return true;
                }
            }
            const std::string mapped =
                "numos_neo_s" + std::to_string(symbols.size());
            symbols.emplace_back(tree.text, mapped);
            out = mapped;
            return true;
        }
        case EngineNodeKind::Pi: out = "pi"; return true;
        case EngineNodeKind::EulerE: out = "exp(1)"; return true;
        case EngineNodeKind::ImagUnit: out = "i"; return true;
        case EngineNodeKind::PlusInfinity: out = "+infinity"; return true;
        case EngineNodeKind::MinusInfinity: out = "-infinity"; return true;
        case EngineNodeKind::UnsignedInfinity: out = "infinity"; return true;
        case EngineNodeKind::Rational:
        case EngineNodeKind::Pow:
        case EngineNodeKind::Equation:
        case EngineNodeKind::Complex:
        case EngineNodeKind::Root: {
            std::string a, b;
            if (!child(0, a) || !child(1, b)) return false;
            if (tree.kind == EngineNodeKind::Rational)
                out = "((" + a + ")/(" + b + "))";
            else if (tree.kind == EngineNodeKind::Pow)
                out = "((" + a + ")^(" + b + "))";
            else if (tree.kind == EngineNodeKind::Equation)
                out = "((" + a + ")=(" + b + "))";
            else if (tree.kind == EngineNodeKind::Complex)
                out = "((" + a + ")+(" + b + ")*i)";
            else
                out = "root(" + a + "," + b + ")";
            return true;
        }
        case EngineNodeKind::Neg:
        case EngineNodeKind::Inv:
        case EngineNodeKind::Sqrt: {
            std::string a;
            if (!child(0, a)) return false;
            if (tree.kind == EngineNodeKind::Neg) out = "(-(" + a + "))";
            else if (tree.kind == EngineNodeKind::Inv) out = "(1/(" + a + "))";
            else out = "sqrt(" + a + ")";
            return true;
        }
        case EngineNodeKind::Add:
        case EngineNodeKind::Mul:
        case EngineNodeKind::List: {
            out = tree.kind == EngineNodeKind::List ? "[" : "(";
            const char* separator =
                tree.kind == EngineNodeKind::Add ? "+" :
                tree.kind == EngineNodeKind::Mul ? "*" : ",";
            for (size_t i = 0; i < tree.children.size(); ++i) {
                std::string part;
                if (!child(i, part)) return false;
                if (i) out += separator;
                out += "(" + part + ")";
            }
            out += tree.kind == EngineNodeKind::List ? "]" : ")";
            return true;
        }
        case EngineNodeKind::Function: {
            if (!validIdentifier(tree.text)) {
                diagnostic = "invalid Neo function name";
                return false;
            }
            out = tree.text + "(";
            for (size_t i = 0; i < tree.children.size(); ++i) {
                std::string part;
                if (!child(i, part)) return false;
                if (i) out += ",";
                out += part;
            }
            out += ")";
            return true;
        }
        case EngineNodeKind::Unsupported:
            diagnostic = "opaque Neo math value cannot be re-evaluated";
            return false;
    }
    diagnostic = "unsupported Neo math value";
    return false;
}

bool NeoMathBackend::nativeTree(const cas::SymExpr* expr,
                                numos::EngineResultNode& out,
                                int depth, int& budget) const {
    using numos::EngineNodeKind;
    if (!expr || depth > kNeoMathDepth || --budget < 0) return false;
    switch (expr->type) {
        case cas::SymExprType::Num: {
            const auto* number = static_cast<const cas::SymNum*>(expr);
            auto rationalNode = [&]() {
                numos::EngineResultNode node;
                if (number->_coeff.isInteger()) {
                    node.kind = EngineNodeKind::Integer;
                    node.text = number->_coeff.num().toString();
                } else {
                    node.kind = EngineNodeKind::Rational;
                    node.children.resize(2);
                    node.children[0].kind = EngineNodeKind::Integer;
                    node.children[0].text =
                        number->_coeff.num().toString();
                    node.children[1].kind = EngineNodeKind::Integer;
                    node.children[1].text =
                        number->_coeff.den().toString();
                }
                return node;
            };
            if (number->isPureRational()) {
                out = rationalNode();
                return true;
            }
            out.kind = EngineNodeKind::Mul;
            out.children.push_back(rationalNode());
            if (number->_outer != 1) {
                numos::EngineResultNode outer;
                outer.kind = EngineNodeKind::Integer;
                outer.text = std::to_string(number->_outer);
                out.children.push_back(std::move(outer));
            }
            if (number->_inner > 1) {
                numos::EngineResultNode radical;
                radical.kind = EngineNodeKind::Sqrt;
                radical.children.resize(1);
                radical.children[0].kind = EngineNodeKind::Integer;
                radical.children[0].text =
                    std::to_string(number->_inner);
                out.children.push_back(std::move(radical));
            }
            auto appendConstant = [&](EngineNodeKind kind, int exponent) {
                if (!exponent) return;
                numos::EngineResultNode constant;
                constant.kind = kind;
                if (exponent == 1) {
                    out.children.push_back(std::move(constant));
                } else {
                    numos::EngineResultNode power;
                    power.kind = EngineNodeKind::Pow;
                    power.children.resize(2);
                    power.children[0] = std::move(constant);
                    power.children[1].kind = EngineNodeKind::Integer;
                    power.children[1].text = std::to_string(exponent);
                    out.children.push_back(std::move(power));
                }
            };
            appendConstant(EngineNodeKind::Pi, number->_piMul);
            appendConstant(EngineNodeKind::EulerE, number->_eMul);
            if (out.children.size() == 1) {
                numos::EngineResultNode sole =
                    std::move(out.children.front());
                out = std::move(sole);
            }
            return true;
        }
        case cas::SymExprType::Var:
            out.kind = EngineNodeKind::Symbol;
            out.text.assign(1, static_cast<const cas::SymVar*>(expr)->name);
            return true;
        case cas::SymExprType::Neg:
        case cas::SymExprType::Paren: {
            out.kind = expr->type == cas::SymExprType::Neg
                           ? EngineNodeKind::Neg : EngineNodeKind::Function;
            const cas::SymExpr* child =
                expr->type == cas::SymExprType::Neg
                    ? static_cast<const cas::SymNeg*>(expr)->child
                    : static_cast<const cas::SymParen*>(expr)->child;
            if (expr->type == cas::SymExprType::Paren) out.text = "identity";
            out.children.resize(1);
            return nativeTree(child, out.children[0], depth + 1, budget);
        }
        case cas::SymExprType::Add: {
            const auto* add = static_cast<const cas::SymAdd*>(expr);
            out.kind = EngineNodeKind::Add;
            out.children.resize(add->count);
            for (uint16_t i = 0; i < add->count; ++i)
                if (!nativeTree(add->terms[i], out.children[i],
                                depth + 1, budget)) return false;
            return true;
        }
        case cas::SymExprType::Mul: {
            const auto* mul = static_cast<const cas::SymMul*>(expr);
            out.kind = EngineNodeKind::Mul;
            out.children.resize(mul->count);
            for (uint16_t i = 0; i < mul->count; ++i)
                if (!nativeTree(mul->factors[i], out.children[i],
                                depth + 1, budget)) return false;
            return true;
        }
        case cas::SymExprType::Pow: {
            const auto* power = static_cast<const cas::SymPow*>(expr);
            out.kind = EngineNodeKind::Pow;
            out.children.resize(2);
            return nativeTree(power->base, out.children[0], depth + 1, budget) &&
                   nativeTree(power->exponent, out.children[1],
                              depth + 1, budget);
        }
        case cas::SymExprType::Func: {
            const auto* function = static_cast<const cas::SymFunc*>(expr);
            out.kind = EngineNodeKind::Function;
            out.text = cas::symFuncName(function->kind);
            out.children.resize(1);
            return nativeTree(function->argument, out.children[0],
                              depth + 1, budget);
        }
        default:
            out.kind = EngineNodeKind::Unsupported;
            out.text = expr->toString();
            return true;
    }
}

bool NeoMathBackend::serialize(const NeoValue& value, std::string& out,
                               SymbolMap& symbols,
                               std::string& diagnostic) const {
    int budget = kNeoMathNodes;
    if (value.isMath() && value.asMath())
        return serializeTree(*value.asMath(), out, symbols, 0, budget,
                             diagnostic) && out.size() <= kNeoMathSource;
    if (value.isSymbolic() && value.asSym()) {
        numos::EngineResultNode tree;
        if (!nativeTree(value.asSym(), tree, 0, budget)) {
            diagnostic = "native symbolic expression exceeds limits";
            return false;
        }
        budget = kNeoMathNodes;
        return serializeTree(tree, out, symbols, 0, budget, diagnostic) &&
               out.size() <= kNeoMathSource;
    }
    if (value.isExact()) {
        out = value.asExact().toString();
        return true;
    }
    if (value.isNumber()) {
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.17g", value.asNum());
        out = buffer;
        return true;
    }
    if (value.isBool()) {
        out = value.asBool() ? "1" : "0";
        return true;
    }
    diagnostic = "Neo value is not mathematical";
    return false;
}

cas::SymExpr* NeoMathBackend::treeToNative(
    const numos::EngineResultNode& tree, cas::SymExprArena& arena,
    int depth, int& budget) const {
    using numos::EngineNodeKind;
    if (depth > kNeoMathDepth || --budget < 0) return nullptr;
    switch (tree.kind) {
        case EngineNodeKind::Integer: {
            int64_t value = 0;
            return parseInt64(tree.text, value) ? cas::symInt(arena, value)
                                                : nullptr;
        }
        case EngineNodeKind::Decimal: {
            char* end = nullptr;
            const double d = std::strtod(tree.text.c_str(), &end);
            if (end != tree.text.c_str() + tree.text.size()) return nullptr;
            return cas::symNum(arena, cas::CASRational::fromFrac(
                static_cast<int64_t>(d * 1000000.0), 1000000));
        }
        case EngineNodeKind::Rational: {
            if (tree.children.size() != 2) return nullptr;
            int64_t numerator = 0, denominator = 0;
            if (!parseInt64(tree.children[0].text, numerator) ||
                !parseInt64(tree.children[1].text, denominator))
                return nullptr;
            return cas::symFrac(arena, numerator, denominator);
        }
        case EngineNodeKind::Symbol:
            return tree.text.size() == 1
                       ? cas::symVar(arena, tree.text.front()) : nullptr;
        case EngineNodeKind::Pi: {
            vpam::ExactVal value = vpam::ExactVal::fromInt(1);
            value.piMul = 1;
            return cas::symNum(arena, value);
        }
        case EngineNodeKind::EulerE: {
            vpam::ExactVal value = vpam::ExactVal::fromInt(1);
            value.eMul = 1;
            return cas::symNum(arena, value);
        }
        case EngineNodeKind::Neg:
            return tree.children.size() == 1
                       ? cas::symNeg(arena, treeToNative(tree.children[0], arena,
                                                        depth + 1, budget))
                       : nullptr;
        case EngineNodeKind::Add: {
            cas::SymExpr* result = cas::symInt(arena, 0);
            for (const auto& child : tree.children) {
                cas::SymExpr* next =
                    treeToNative(child, arena, depth + 1, budget);
                if (!next) return nullptr;
                result = cas::symAddRaw(arena, result, next);
            }
            return result;
        }
        case EngineNodeKind::Mul: {
            cas::SymExpr* result = cas::symInt(arena, 1);
            for (const auto& child : tree.children) {
                cas::SymExpr* next =
                    treeToNative(child, arena, depth + 1, budget);
                if (!next) return nullptr;
                result = cas::symMulRaw(arena, result, next);
            }
            return result;
        }
        case EngineNodeKind::Inv:
            if (tree.children.size() == 1)
                return cas::symPow(arena,
                    treeToNative(tree.children[0], arena, depth + 1, budget),
                    cas::symInt(arena, -1));
            return nullptr;
        case EngineNodeKind::Pow:
            if (tree.children.size() == 2)
                return cas::symPow(
                    arena,
                    treeToNative(tree.children[0], arena, depth + 1, budget),
                    treeToNative(tree.children[1], arena, depth + 1, budget));
            return nullptr;
        case EngineNodeKind::Sqrt:
            if (tree.children.size() == 1)
                return cas::symPow(
                    arena,
                    treeToNative(tree.children[0], arena, depth + 1, budget),
                    cas::symFrac(arena, 1, 2));
            return nullptr;
        case EngineNodeKind::Function: {
            if (tree.children.size() != 1) return nullptr;
            bool ok = false;
            const cas::SymFuncKind kind = nativeFunctionKind(tree.text, ok);
            return ok ? cas::symFunc(
                            arena, kind,
                            treeToNative(tree.children[0], arena,
                                         depth + 1, budget))
                      : nullptr;
        }
        case EngineNodeKind::Equation:
            if (tree.children.size() != 2) return nullptr;
            return cas::symAddRaw(
                arena,
                treeToNative(tree.children[0], arena, depth + 1, budget),
                cas::symNeg(
                    arena,
                    treeToNative(tree.children[1], arena, depth + 1, budget)));
        default:
            return nullptr;
    }
}

cas::SymExpr* NeoMathBackend::toNative(const NeoValue& value,
                                       cas::SymExprArena& arena) const {
    if (value.isSymbolic()) return value.asSym();
    if (value.isExact()) return cas::symNum(arena, value.asExact());
    if (value.isNumber()) {
        const double d = value.asNum();
        double integer = 0.0;
        if (std::modf(d, &integer) == 0.0 &&
            integer >= static_cast<double>(INT64_MIN) &&
            integer <= static_cast<double>(INT64_MAX))
            return cas::symInt(arena, static_cast<int64_t>(integer));
        return cas::symNum(arena, cas::CASRational::fromFrac(
            static_cast<int64_t>(d * 1000000.0), 1000000));
    }
    if (value.isMath() && value.asMath()) {
        int budget = kNeoMathNodes;
        return treeToNative(*value.asMath(), arena, 0, budget);
    }
    return nullptr;
}

NeoValue NeoMathBackend::symbol(const std::string& name,
                                cas::SymExprArena& nativeArena) {
    if (!validIdentifier(name))
        return NeoValue::makeError("invalid Neo symbol");
    if (_engine == NeoMathEngine::Native) {
        // Native SymExpr intentionally retains its historical single-letter
        // variable contract.
        return NeoValue::makeSymbolic(
            cas::symVar(nativeArena, name.empty() ? 'x' : name.front()));
    }
    numos::EngineResultNode tree;
    tree.kind = numos::EngineNodeKind::Symbol;
    tree.text = name;
    return valueFromTree(std::move(tree));
}

NeoValue NeoMathBackend::constant(const std::string& name,
                                  cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        if (name == "pi")
            return NeoValue::makeNumber(3.14159265358979323846);
        if (name == "e")
            return NeoValue::makeNumber(2.71828182845904523536);
        if (name == "infinity")
            return NeoValue::makeNumber(
                std::numeric_limits<double>::infinity());
        return NeoValue::makeError("unknown mathematical constant");
    }
    numos::EngineResultNode tree;
    if (name == "pi") tree.kind = numos::EngineNodeKind::Pi;
    else if (name == "e") tree.kind = numos::EngineNodeKind::EulerE;
    else if (name == "infinity")
        tree.kind = numos::EngineNodeKind::PlusInfinity;
    else return NeoValue::makeError("unknown mathematical constant");
    return valueFromTree(std::move(tree));
}

bool NeoMathBackend::variableName(const NeoValue& value,
                                  std::string& out) const {
    if (value.isMath() && value.asMath() &&
        value.asMath()->kind == numos::EngineNodeKind::Symbol) {
        out = value.asMath()->text;
        return validIdentifier(out);
    }
    if (value.isSymbolic() && value.asSym() &&
        value.asSym()->type == cas::SymExprType::Var) {
        out.assign(1, static_cast<cas::SymVar*>(value.asSym())->name);
        return true;
    }
    return false;
}

NeoMathResult NeoMathBackend::binary(
    NeoBinaryMathOp operation, const NeoValue& lhs, const NeoValue& rhs,
    cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        NeoValue left = lhs;
        NeoValue right = rhs;
        if (left.isMath()) {
            cas::SymExpr* converted = toNative(left, nativeArena);
            if (!converted) {
                note(NeoMathOperation::Evaluate,
                     numos::MathEngineStatus::Unsupported);
                return error(numos::MathEngineStatus::Unsupported,
                             "value cannot be converted to native CAS");
            }
            left = NeoValue::makeSymbolic(converted);
        }
        if (right.isMath()) {
            cas::SymExpr* converted = toNative(right, nativeArena);
            if (!converted) {
                note(NeoMathOperation::Evaluate,
                     numos::MathEngineStatus::Unsupported);
                return error(numos::MathEngineStatus::Unsupported,
                             "value cannot be converted to native CAS");
            }
            right = NeoValue::makeSymbolic(converted);
        }
        NeoValue value;
        switch (operation) {
            case NeoBinaryMathOp::Add: value = left.add(right, nativeArena); break;
            case NeoBinaryMathOp::Subtract: value = left.sub(right, nativeArena); break;
            case NeoBinaryMathOp::Multiply: value = left.mul(right, nativeArena); break;
            case NeoBinaryMathOp::Divide: value = left.div(right, nativeArena); break;
            case NeoBinaryMathOp::Power: value = left.pow(right, nativeArena); break;
        }
        NeoMathResult result;
        result.status = numos::MathEngineStatus::Ok;
        if (value.isSymbolic()) {
            numos::EngineResultNode tree;
            int budget = kNeoMathNodes;
            if (!nativeTree(value.asSym(), tree, 0, budget))
                return error(numos::MathEngineStatus::Unsupported,
                             "native CAS result exceeds limits");
            result.value = valueFromTree(std::move(tree));
        } else {
            result.value = value;
        }
        note(NeoMathOperation::Evaluate, result.status);
        return result;
    }

    std::string left, right, diagnostic;
    SymbolMap symbols;
    if (!serialize(lhs, left, symbols, diagnostic) ||
        !serialize(rhs, right, symbols, diagnostic)) {
        note(NeoMathOperation::Evaluate,
             numos::MathEngineStatus::Unsupported);
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    }
    const char* op =
        operation == NeoBinaryMathOp::Add ? "+" :
        operation == NeoBinaryMathOp::Subtract ? "-" :
        operation == NeoBinaryMathOp::Multiply ? "*" :
        operation == NeoBinaryMathOp::Divide ? "/" : "^";
    const std::string expression =
        "((" + left + ")" + op + "(" + right + "))";
    auto result = resultFromGiac(
        numos::GiacEngine::instance().evaluateStructured(expression.c_str()),
        symbols);
    note(NeoMathOperation::Evaluate, result.status);
    return result;
}

NeoMathResult NeoMathBackend::negate(const NeoValue& value,
                                     cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native && !value.isMath()) {
        NeoMathResult result;
        result.status = numos::MathEngineStatus::Ok;
        NeoValue native = value.neg(nativeArena);
        if (native.isSymbolic()) {
            numos::EngineResultNode tree;
            int budget = kNeoMathNodes;
            if (!nativeTree(native.asSym(), tree, 0, budget))
                return error(numos::MathEngineStatus::Unsupported,
                             "native CAS result exceeds limits");
            result.value = valueFromTree(std::move(tree));
        } else result.value = native;
        note(NeoMathOperation::Evaluate, result.status);
        return result;
    }
    return binary(NeoBinaryMathOp::Multiply,
                  NeoValue::makeExact(cas::CASRational::fromInt(-1)),
                  value, nativeArena);
}

NeoMathResult NeoMathBackend::function(
    const std::string& name, const std::vector<NeoValue>& args,
    cas::SymExprArena& nativeArena) {
    if (!validIdentifier(name) || args.empty())
        return error(numos::MathEngineStatus::ParseError,
                     "invalid Neo mathematical function call");
    if (_engine == NeoMathEngine::Native) {
        if (args.size() != 1)
            return error(numos::MathEngineStatus::Unsupported,
                         "native CAS supports unary symbolic functions here");
        cas::SymExpr* argument = toNative(args.front(), nativeArena);
        bool ok = false;
        const cas::SymFuncKind kind = nativeFunctionKind(name, ok);
        if (!argument || !ok)
            return error(numos::MathEngineStatus::Unsupported,
                         "function unsupported by native CAS");
        numos::EngineResultNode tree;
        int budget = kNeoMathNodes;
        cas::SymExpr* expression = cas::symFunc(nativeArena, kind, argument);
        if (!nativeTree(expression, tree, 0, budget))
            return error(numos::MathEngineStatus::Unsupported,
                         "native CAS result exceeds limits");
        NeoMathResult result;
        result.status = numos::MathEngineStatus::Ok;
        result.value = valueFromTree(std::move(tree));
        note(NeoMathOperation::Evaluate, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression = name + "(";
    std::string diagnostic;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string arg;
        if (!serialize(args[i], arg, symbols, diagnostic)) {
            note(NeoMathOperation::Evaluate,
                 numos::MathEngineStatus::Unsupported);
            return error(numos::MathEngineStatus::Unsupported, diagnostic);
        }
        if (i) expression += ",";
        expression += arg;
    }
    expression += ")";
    auto result = resultFromGiac(
        numos::GiacEngine::instance().evaluateStructured(expression.c_str()),
        symbols);
    note(NeoMathOperation::Evaluate, result.status);
    return result;
}

NeoMathResult NeoMathBackend::equation(const NeoValue& lhs,
                                       const NeoValue& rhs,
                                       cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native)
        return binary(NeoBinaryMathOp::Subtract, lhs, rhs, nativeArena);
    SymbolMap symbols;
    std::string left, right, diagnostic;
    if (!serialize(lhs, left, symbols, diagnostic) ||
        !serialize(rhs, right, symbols, diagnostic))
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    numos::EngineResultNode tree;
    tree.kind = numos::EngineNodeKind::Equation;
    tree.children.resize(2);
    auto parsedLeft = numos::GiacEngine::instance().evaluateStructured(
        left.c_str());
    auto parsedRight = numos::GiacEngine::instance().evaluateStructured(
        right.c_str());
    if (!parsedLeft.base.ok() || !parsedLeft.hasTree ||
        !parsedRight.base.ok() || !parsedRight.hasTree)
        return error(numos::MathEngineStatus::ParseError,
                     "unable to construct Neo equation");
    tree.children[0] = std::move(parsedLeft.tree);
    tree.children[1] = std::move(parsedRight.tree);
    restoreSymbols(tree, symbols);
    NeoMathResult result;
    result.status = numos::MathEngineStatus::Ok;
    result.value = valueFromTree(std::move(tree));
    note(NeoMathOperation::Evaluate, result.status);
    return result;
}

NeoMathResult NeoMathBackend::evaluate(const NeoValue& value,
                                       cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        NeoMathResult result;
        result.status = numos::MathEngineStatus::Ok;
        result.value = value;
        note(NeoMathOperation::Evaluate, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression, diagnostic;
    if (!serialize(value, expression, symbols, diagnostic)) {
        note(NeoMathOperation::Evaluate,
             numos::MathEngineStatus::Unsupported);
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    }
    auto result = resultFromGiac(
        numos::GiacEngine::instance().evaluateStructured(expression.c_str()),
        symbols);
    note(NeoMathOperation::Evaluate, result.status);
    return result;
}

NeoMathResult NeoMathBackend::simplify(const NeoValue& value,
                                       cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        cas::SymExpr* input = toNative(value, nativeArena);
        cas::SymExpr* output = input
            ? cas::SymSimplify::simplify(input, nativeArena) : nullptr;
        if (!output) return error(numos::MathEngineStatus::EvaluationError,
                                  "native simplification failed");
        numos::EngineResultNode tree;
        int budget = kNeoMathNodes;
        if (!nativeTree(output, tree, 0, budget))
            return error(numos::MathEngineStatus::Unsupported,
                         "native result exceeds limits");
        NeoMathResult result{numos::MathEngineStatus::Ok,
                             valueFromTree(std::move(tree)), {}, false};
        note(NeoMathOperation::Simplify, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression, diagnostic;
    if (!serialize(value, expression, symbols, diagnostic)) {
        note(NeoMathOperation::Simplify,
             numos::MathEngineStatus::Unsupported);
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    }
    auto result = resultFromGiac(
        numos::GiacEngine::instance().transformStructured(
            numos::AlgebraTransform::Simplify, expression.c_str()), symbols);
    note(NeoMathOperation::Simplify, result.status);
    return result;
}

NeoMathResult NeoMathBackend::expand(const NeoValue& value,
                                     cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        NeoMathResult result = simplify(value, nativeArena);
        ++_counts[static_cast<size_t>(NeoMathEngine::Native)]
                 [static_cast<size_t>(NeoMathOperation::Expand)];
        return result;
    }
    SymbolMap symbols;
    std::string expression, diagnostic;
    if (!serialize(value, expression, symbols, diagnostic)) {
        note(NeoMathOperation::Expand, numos::MathEngineStatus::Unsupported);
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    }
    auto result = resultFromGiac(
        numos::GiacEngine::instance().transformStructured(
            numos::AlgebraTransform::Expand, expression.c_str()), symbols);
    note(NeoMathOperation::Expand, result.status);
    return result;
}

NeoMathResult NeoMathBackend::factor(const NeoValue& value,
                                     cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native)
        return error(numos::MathEngineStatus::Unsupported,
                     "factor is not exposed by the native Neo backend");
    SymbolMap symbols;
    std::string expression, diagnostic;
    if (!serialize(value, expression, symbols, diagnostic)) {
        note(NeoMathOperation::Factor, numos::MathEngineStatus::Unsupported);
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    }
    auto result = resultFromGiac(
        numos::GiacEngine::instance().transformStructured(
            numos::AlgebraTransform::Factor, expression.c_str()), symbols);
    note(NeoMathOperation::Factor, result.status);
    return result;
}

NeoMathResult NeoMathBackend::differentiate(
    const NeoValue& value, const std::string& variable,
    cas::SymExprArena& nativeArena) {
    if (!validIdentifier(variable))
        return error(numos::MathEngineStatus::ParseError,
                     "invalid differentiation variable");
    if (_engine == NeoMathEngine::Native) {
        if (variable.size() != 1)
            return error(numos::MathEngineStatus::Unsupported,
                         "native CAS variables are single-letter");
        cas::SymExpr* input = toNative(value, nativeArena);
        cas::SymExpr* output = input
            ? cas::SymDiff::diff(input, variable.front(), nativeArena)
            : nullptr;
        if (output) output = cas::SymSimplify::simplify(output, nativeArena);
        if (!output) return error(numos::MathEngineStatus::EvaluationError,
                                  "native differentiation failed");
        numos::EngineResultNode tree;
        int budget = kNeoMathNodes;
        if (!nativeTree(output, tree, 0, budget))
            return error(numos::MathEngineStatus::Unsupported,
                         "native result exceeds limits");
        NeoMathResult result{numos::MathEngineStatus::Ok,
                             valueFromTree(std::move(tree)), {}, false};
        note(NeoMathOperation::Differentiate, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression, diagnostic, mappedVariable;
    if (!serialize(value, expression, symbols, diagnostic))
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    numos::EngineResultNode variableTree;
    variableTree.kind = numos::EngineNodeKind::Symbol;
    variableTree.text = variable;
    int budget = kNeoMathNodes;
    if (!serializeTree(variableTree, mappedVariable, symbols, 0, budget,
                       diagnostic))
        return error(numos::MathEngineStatus::ParseError, diagnostic);
    numos::CalculusRequest request;
    request.operation = numos::CalculusOperation::Differentiate;
    request.expression = expression;
    request.variable = mappedVariable;
    auto giac = numos::GiacEngine::instance().evaluateCalculusStructured(request);
    NeoMathResult result;
    result.status = giac.status;
    result.diagnostic = giac.diagnostic;
    result.unevaluated = giac.unevaluated;
    if (giac.ok()) {
        if (giac.hasTree) {
            restoreSymbols(giac.tree, symbols);
            result.value = valueFromTree(std::move(giac.tree), giac.unevaluated);
        } else {
            numos::EngineResultNode opaque;
            opaque.kind = numos::EngineNodeKind::Unsupported;
            opaque.text = giac.exactText;
            result.value = valueFromTree(std::move(opaque), giac.unevaluated);
        }
    } else result.value = NeoValue::makeError(result.diagnostic);
    note(NeoMathOperation::Differentiate, result.status);
    return result;
}

NeoMathResult NeoMathBackend::integrate(
    const NeoValue& value, const std::string& variable,
    cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        if (variable.size() != 1)
            return error(numos::MathEngineStatus::Unsupported,
                         "native CAS variables are single-letter");
        cas::SymExpr* input = toNative(value, nativeArena);
        cas::SymExpr* output = input
            ? cas::SymIntegrate::integrate(input, variable.front(), nativeArena)
            : nullptr;
        bool unevaluated = false;
        if (!output && input) {
            output = cas::symFunc(nativeArena, cas::SymFuncKind::Integral, input);
            unevaluated = true;
        }
        if (!output) return error(numos::MathEngineStatus::EvaluationError,
                                  "native integration failed");
        numos::EngineResultNode tree;
        int budget = kNeoMathNodes;
        if (!nativeTree(output, tree, 0, budget))
            return error(numos::MathEngineStatus::Unsupported,
                         "native result exceeds limits");
        NeoMathResult result{numos::MathEngineStatus::Ok,
                             valueFromTree(std::move(tree), unevaluated), {},
                             unevaluated};
        note(NeoMathOperation::Integrate, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression, diagnostic, mappedVariable;
    if (!serialize(value, expression, symbols, diagnostic))
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    numos::EngineResultNode variableTree;
    variableTree.kind = numos::EngineNodeKind::Symbol;
    variableTree.text = variable;
    int budget = kNeoMathNodes;
    if (!serializeTree(variableTree, mappedVariable, symbols, 0, budget,
                       diagnostic))
        return error(numos::MathEngineStatus::ParseError, diagnostic);
    numos::CalculusRequest request;
    request.operation = numos::CalculusOperation::IntegrateIndefinite;
    request.expression = expression;
    request.variable = mappedVariable;
    auto giac = numos::GiacEngine::instance().evaluateCalculusStructured(request);
    NeoMathResult result;
    result.status = giac.status;
    result.diagnostic = giac.diagnostic;
    result.unevaluated = giac.unevaluated;
    if (giac.ok()) {
        if (giac.hasTree) {
            restoreSymbols(giac.tree, symbols);
            result.value = valueFromTree(std::move(giac.tree), giac.unevaluated);
        } else {
            numos::EngineResultNode opaque;
            opaque.kind = numos::EngineNodeKind::Unsupported;
            opaque.text = giac.exactText;
            result.value = valueFromTree(std::move(opaque), giac.unevaluated);
        }
    } else result.value = NeoValue::makeError(result.diagnostic);
    note(NeoMathOperation::Integrate, result.status);
    return result;
}

NeoMathResult NeoMathBackend::solve(
    const NeoValue& equationOrResidual, const std::string& variable,
    cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        if (variable.size() != 1)
            return error(numos::MathEngineStatus::Unsupported,
                         "native CAS variables are single-letter");
        cas::SymExpr* lhs = toNative(equationOrResidual, nativeArena);
        if (!lhs) return error(numos::MathEngineStatus::Unsupported,
                               "value cannot be solved by native CAS");
        cas::OmniSolver solver;
        cas::OmniResult native = solver.solve(
            lhs, cas::symInt(nativeArena, 0), variable.front(), nativeArena);
        if (!native.ok)
            return error(numos::MathEngineStatus::EvaluationError,
                         native.error);
        auto* values = ownList();
        if (!values)
            return error(numos::MathEngineStatus::OutOfMemory,
                         "Neo list allocation failed");
        for (const auto& solution : native.solutions) {
            if (solution.symbolic) {
                numos::EngineResultNode tree;
                int budget = kNeoMathNodes;
                if (nativeTree(solution.symbolic, tree, 0, budget))
                    values->push_back(valueFromTree(std::move(tree)));
            } else {
                values->push_back(NeoValue::makeNumber(solution.numeric));
            }
        }
        NeoMathResult result{numos::MathEngineStatus::Ok,
                             NeoValue::makeList(values), {}, false};
        note(NeoMathOperation::Solve, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression, diagnostic, mappedVariable;
    if (!serialize(equationOrResidual, expression, symbols, diagnostic))
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    numos::EngineResultNode variableTree;
    variableTree.kind = numos::EngineNodeKind::Symbol;
    variableTree.text = variable;
    int budget = kNeoMathNodes;
    if (!serializeTree(variableTree, mappedVariable, symbols, 0, budget,
                       diagnostic))
        return error(numos::MathEngineStatus::ParseError, diagnostic);
    numos::SolveEquation equation;
    if (equationOrResidual.isMath() && equationOrResidual.asMath() &&
        equationOrResidual.asMath()->kind == numos::EngineNodeKind::Equation &&
        equationOrResidual.asMath()->children.size() == 2) {
        SymbolMap equationSymbols;
        if (!serializeTree(equationOrResidual.asMath()->children[0],
                           equation.lhs, equationSymbols, 0, budget,
                           diagnostic) ||
            !serializeTree(equationOrResidual.asMath()->children[1],
                           equation.rhs, equationSymbols, 0, budget,
                           diagnostic))
            return error(numos::MathEngineStatus::Unsupported, diagnostic);
        symbols = equationSymbols;
        mappedVariable.clear();
        budget = kNeoMathNodes;
        if (!serializeTree(variableTree, mappedVariable, symbols, 0, budget,
                           diagnostic))
            return error(numos::MathEngineStatus::ParseError, diagnostic);
    } else {
        equation.lhs = expression;
        equation.rhs = "0";
    }
    auto giac = numos::GiacEngine::instance().solveStructured(
        equation, mappedVariable, numos::SolveDomainPolicy::RealAndComplex);
    NeoMathResult result;
    result.status = giac.status;
    result.diagnostic = giac.diagnostic;
    auto* roots = ownList();
    if (!roots) return error(numos::MathEngineStatus::OutOfMemory,
                             "Neo solution allocation failed");
    if (giac.ok()) {
        if (giac.setKind == numos::SolutionSetKind::AllValues) {
            numos::EngineResultNode all;
            all.kind = numos::EngineNodeKind::Symbol;
            all.text = "all_values";
            roots->push_back(valueFromTree(std::move(all)));
        } else {
            for (auto& group : giac.groups) {
                if (group.values.empty()) continue;
                restoreSymbols(group.values[0].exactValue, symbols);
                roots->push_back(
                    valueFromTree(std::move(group.values[0].exactValue)));
            }
        }
        result.value = NeoValue::makeList(roots);
    } else {
        result.value = NeoValue::makeError(result.diagnostic);
    }
    note(NeoMathOperation::Solve, result.status);
    return result;
}

NeoMathResult NeoMathBackend::solveSystem(
    const std::vector<NeoValue>& equations,
    const std::vector<std::string>& variables,
    cas::SymExprArena& nativeArena) {
    if (_engine == NeoMathEngine::Native) {
        if (equations.size() != variables.size() ||
            (equations.size() != 2 && equations.size() != 3))
            return error(numos::MathEngineStatus::Unsupported,
                         "native systems support only 2x2 and 3x3");
        std::vector<cas::SymExpr*> nativeEquations;
        std::vector<char> nativeVariables;
        for (size_t i = 0; i < equations.size(); ++i) {
            cas::SymExpr* expression = toNative(equations[i], nativeArena);
            if (!expression || variables[i].size() != 1)
                return error(numos::MathEngineStatus::Unsupported,
                             "system cannot be converted to native CAS");
            nativeEquations.push_back(expression);
            nativeVariables.push_back(variables[i].front());
        }
        cas::SystemSolver solver;
        cas::NLSystemResult native =
            equations.size() == 2
                ? solver.solveNonlinear2x2(
                      nativeEquations[0], nativeEquations[1],
                      nativeVariables[0], nativeVariables[1], nativeArena)
                : solver.solveNonlinear3x3(
                      nativeEquations[0], nativeEquations[1],
                      nativeEquations[2], nativeVariables[0],
                      nativeVariables[1], nativeVariables[2], nativeArena);
        if (!native.ok)
            return error(numos::MathEngineStatus::EvaluationError,
                         native.error);
        auto* groups = ownList();
        if (!groups) return error(numos::MathEngineStatus::OutOfMemory,
                                  "Neo system allocation failed");
        for (const auto& solution : native.solutions) {
            auto* dict = ownDict();
            if (!dict) return error(numos::MathEngineStatus::OutOfMemory,
                                    "Neo system allocation failed");
            const cas::SymExpr* exact[3] = {
                solution.exprX, solution.exprY, solution.exprZ};
            const double approximate[3] = {
                solution.numX, solution.numY, solution.numZ};
            for (size_t i = 0; i < variables.size(); ++i) {
                if (exact[i]) {
                    numos::EngineResultNode tree;
                    int treeBudget = kNeoMathNodes;
                    nativeTree(exact[i], tree, 0, treeBudget);
                    (*dict)[variables[i]] = valueFromTree(std::move(tree));
                } else {
                    (*dict)[variables[i]] =
                        NeoValue::makeNumber(approximate[i]);
                }
            }
            groups->push_back(NeoValue::makeDict(dict));
        }
        NeoMathResult result{numos::MathEngineStatus::Ok,
                             NeoValue::makeList(groups), {}, false};
        note(NeoMathOperation::SolveSystem, result.status);
        return result;
    }

    if (equations.empty() || equations.size() != variables.size() ||
        equations.size() > 8)
        return error(numos::MathEngineStatus::ParseError,
                     "invalid Neo system dimensions");
    SymbolMap symbols;
    std::vector<numos::SolveEquation> requests;
    std::vector<std::string> mappedVariables;
    std::string diagnostic;
    int budget = kNeoMathNodes;
    for (const NeoValue& value : equations) {
        numos::SolveEquation equation;
        if (value.isMath() && value.asMath() &&
            value.asMath()->kind == numos::EngineNodeKind::Equation &&
            value.asMath()->children.size() == 2) {
            if (!serializeTree(value.asMath()->children[0], equation.lhs,
                               symbols, 0, budget, diagnostic) ||
                !serializeTree(value.asMath()->children[1], equation.rhs,
                               symbols, 0, budget, diagnostic))
                return error(numos::MathEngineStatus::Unsupported, diagnostic);
        } else {
            if (!serialize(value, equation.lhs, symbols, diagnostic))
                return error(numos::MathEngineStatus::Unsupported, diagnostic);
            equation.rhs = "0";
        }
        requests.push_back(std::move(equation));
    }
    for (const std::string& variable : variables) {
        numos::EngineResultNode tree;
        tree.kind = numos::EngineNodeKind::Symbol;
        tree.text = variable;
        std::string mapped;
        budget = kNeoMathNodes;
        if (!serializeTree(tree, mapped, symbols, 0, budget, diagnostic))
            return error(numos::MathEngineStatus::ParseError, diagnostic);
        mappedVariables.push_back(mapped);
    }
    auto giac = numos::GiacEngine::instance().solveSystemStructured(
        requests, mappedVariables, numos::SolveDomainPolicy::RealAndComplex);
    NeoMathResult result;
    result.status = giac.status;
    result.diagnostic = giac.diagnostic;
    if (!giac.ok()) {
        result.value = NeoValue::makeError(result.diagnostic);
        note(NeoMathOperation::SolveSystem, result.status);
        return result;
    }
    auto* groups = ownList();
    if (!groups) return error(numos::MathEngineStatus::OutOfMemory,
                              "Neo system allocation failed");
    for (auto& solutionGroup : giac.groups) {
        auto* dict = ownDict();
        if (!dict) return error(numos::MathEngineStatus::OutOfMemory,
                                "Neo system allocation failed");
        for (size_t i = 0;
             i < solutionGroup.values.size() && i < variables.size(); ++i) {
            auto& solution = solutionGroup.values[i];
            restoreSymbols(solution.exactValue, symbols);
            (*dict)[variables[i]] =
                valueFromTree(std::move(solution.exactValue));
        }
        groups->push_back(NeoValue::makeDict(dict));
    }
    result.value = NeoValue::makeList(groups);
    note(NeoMathOperation::SolveSystem, result.status);
    return result;
}

NeoMathResult NeoMathBackend::taylor(
    const NeoValue& value, const std::string& variable,
    const NeoValue& center, int order, cas::SymExprArena& nativeArena) {
    if (order < 0 || order > 32)
        return error(numos::MathEngineStatus::Unsupported,
                     "Taylor order must be between 0 and 32");
    if (_engine == NeoMathEngine::Native) {
        if (variable.size() != 1)
            return error(numos::MathEngineStatus::Unsupported,
                         "native CAS variables are single-letter");
        cas::SymExpr* expression = toNative(value, nativeArena);
        if (!expression)
            return error(numos::MathEngineStatus::Unsupported,
                         "Taylor expression cannot be converted");
        const double point = center.toDouble();
        auto* coefficients = ownList();
        if (!coefficients)
            return error(numos::MathEngineStatus::OutOfMemory,
                         "Neo Taylor allocation failed");
        cas::SymExpr* derivative = expression;
        double factorial = 1.0;
        for (int i = 0; i <= order; ++i) {
            if (i > 0) {
                derivative = cas::SymDiff::diff(
                    derivative, variable.front(), nativeArena);
                if (derivative)
                    derivative = cas::SymSimplify::simplify(
                        derivative, nativeArena);
                factorial *= static_cast<double>(i);
            }
            const double coefficient =
                derivative ? derivative->evaluate(point) / factorial : NAN;
            coefficients->push_back(NeoValue::makeNumber(
                std::isfinite(coefficient) ? coefficient : 0.0));
        }
        NeoMathResult result{numos::MathEngineStatus::Ok,
                             NeoValue::makeList(coefficients), {}, false};
        note(NeoMathOperation::Taylor, result.status);
        return result;
    }
    SymbolMap symbols;
    std::string expression, centerText, diagnostic, mappedVariable;
    if (!serialize(value, expression, symbols, diagnostic) ||
        !serialize(center, centerText, symbols, diagnostic))
        return error(numos::MathEngineStatus::Unsupported, diagnostic);
    numos::EngineResultNode variableTree;
    variableTree.kind = numos::EngineNodeKind::Symbol;
    variableTree.text = variable;
    int budget = kNeoMathNodes;
    if (!serializeTree(variableTree, mappedVariable, symbols, 0, budget,
                       diagnostic))
        return error(numos::MathEngineStatus::ParseError, diagnostic);
    numos::TaylorRequest request;
    request.expression = expression;
    request.variable = mappedVariable;
    request.center = centerText;
    request.order = order;
    auto result = resultFromGiac(
        numos::GiacEngine::instance().taylorStructured(request), symbols);
    note(NeoMathOperation::Taylor, result.status);
    return result;
}

std::unique_ptr<NeoCompiledPlot> NeoMathBackend::compilePlot(
    const NeoValue& value, const std::string& variable,
    cas::SymExprArena& nativeArena) {
    std::unique_ptr<NeoCompiledPlot> plot(
        new (std::nothrow) NeoCompiledPlot());
    if (!plot) return plot;
    plot->engine = _engine;
    plot->variable = variable;
    plot->compileAttempted = true;
    ++_plotCompileCount;
    if (_engine == NeoMathEngine::Native) {
        plot->native = toNative(value, nativeArena);
        if (!plot->native) plot->diagnostic =
            "plot expression cannot be converted to native CAS";
        note(NeoMathOperation::CompilePlot,
             plot->native ? numos::MathEngineStatus::Ok
                          : numos::MathEngineStatus::Unsupported);
        return plot;
    }
    SymbolMap symbols;
    std::string diagnostic;
    if (!serialize(value, plot->expression, symbols, diagnostic)) {
        plot->diagnostic = diagnostic;
        note(NeoMathOperation::CompilePlot,
             numos::MathEngineStatus::Unsupported);
        return plot;
    }
    std::string mappedVariable;
    for (const auto& symbol : symbols)
        if (symbol.first == variable) mappedVariable = symbol.second;
    if (mappedVariable.empty()) {
        numos::EngineResultNode variableTree;
        variableTree.kind = numos::EngineNodeKind::Symbol;
        variableTree.text = variable;
        int budget = kNeoMathNodes;
        serializeTree(variableTree, mappedVariable, symbols, 0, budget,
                      diagnostic);
    }
    plot->variable = mappedVariable;
    plot->giac = numos::GiacEngine::instance().compileNumeric(
        plot->expression.c_str(), plot->variable.c_str(), true);
    plot->diagnostic = plot->giac.diagnostic();
    note(NeoMathOperation::CompilePlot,
         plot->giac.valid() ? numos::MathEngineStatus::Ok
                            : numos::MathEngineStatus::ParseError);
    return plot;
}

bool NeoMathBackend::evaluatePlot(NeoCompiledPlot& plot, double x,
                                  double& out,
                                  cas::SymExprArena& nativeArena) {
    out = NAN;
    if (plot.engine != _engine) return false;
    if (plot.engine == NeoMathEngine::Native) {
        const double value = plot.native ? plot.native->evaluate(x) : NAN;
        const bool ok = std::isfinite(value);
        if (ok) out = value;
        note(NeoMathOperation::EvaluatePlot,
             ok ? numos::MathEngineStatus::Ok
                : numos::MathEngineStatus::Undefined);
        return ok;
    }
    if (!plot.giac.valid() && !plot.expression.empty()) {
        plot.giac = numos::GiacEngine::instance().compileNumeric(
            plot.expression.c_str(), plot.variable.c_str(), true);
        ++_plotCompileCount;
    }
    const bool ok = numos::GiacEngine::instance().evaluateNumeric(
        plot.giac, x, out) && std::isfinite(out);
    if (!ok) out = NAN;
    note(NeoMathOperation::EvaluatePlot,
         ok ? numos::MathEngineStatus::Ok
            : numos::MathEngineStatus::Undefined);
    return ok;
}

bool NeoMathBackend::numericValue(const NeoValue& value, double& out,
                                  cas::SymExprArena& nativeArena) {
    if (value.isNumber() || value.isExact() || value.isBool()) {
        out = value.toDouble();
        return std::isfinite(out);
    }
    if (_engine == NeoMathEngine::Native) {
        cas::SymExpr* expression = toNative(value, nativeArena);
        out = expression ? expression->evaluate(0.0) : NAN;
        return std::isfinite(out);
    }
    auto plot = compilePlot(value, "numos_neo_numeric_probe", nativeArena);
    return plot && evaluatePlot(*plot, 0.0, out, nativeArena);
}
