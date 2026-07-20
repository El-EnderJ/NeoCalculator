/*
 * NumOS NeoLanguage mathematical backend boundary.
 *
 * This header uses only engine-neutral public NumOS result and retained
 * expression contracts. Giac implementation types stay in GiacEngine.cpp.
 */
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "NeoValue.h"
#include "../math/giac/GiacEngine.h"

class NeoEnv;

enum class NeoMathEngine : uint8_t {
    Giac,
    Native
};

enum class NeoMathOperation : uint8_t {
    Evaluate,
    Simplify,
    Expand,
    Factor,
    Differentiate,
    Integrate,
    Solve,
    SolveSystem,
    Taylor,
    CompilePlot,
    EvaluatePlot,
    Count
};

enum class NeoBinaryMathOp : uint8_t {
    Add,
    Subtract,
    Multiply,
    Divide,
    Power
};

struct NeoMathResult {
    numos::MathEngineStatus status = numos::MathEngineStatus::Unsupported;
    NeoValue value;
    std::string diagnostic;
    bool unevaluated = false;

    bool ok() const { return status == numos::MathEngineStatus::Ok; }
};

struct NeoCompiledPlot {
    NeoMathEngine engine = NeoMathEngine::Giac;
    std::string expression;
    std::string variable = "x";
    numos::CompiledExpression giac;
    cas::SymExpr* native = nullptr;
    bool compileAttempted = false;
    std::string diagnostic;
};

class NeoMathBackend {
public:
    NeoMathBackend();

    NeoMathEngine engine() const { return _engine; }
    const char* engineName() const;
    void setEngine(NeoMathEngine engine);
    void resetSession();
    void beginRun();

    NeoMathResult evaluate(const NeoValue& value,
                           cas::SymExprArena& nativeArena);
    NeoMathResult binary(NeoBinaryMathOp operation, const NeoValue& lhs,
                         const NeoValue& rhs,
                         cas::SymExprArena& nativeArena);
    NeoMathResult negate(const NeoValue& value,
                         cas::SymExprArena& nativeArena);
    NeoMathResult function(const std::string& name,
                           const std::vector<NeoValue>& args,
                           cas::SymExprArena& nativeArena);
    NeoMathResult equation(const NeoValue& lhs, const NeoValue& rhs,
                           cas::SymExprArena& nativeArena);

    NeoMathResult simplify(const NeoValue& value,
                           cas::SymExprArena& nativeArena);
    NeoMathResult expand(const NeoValue& value,
                         cas::SymExprArena& nativeArena);
    NeoMathResult factor(const NeoValue& value,
                         cas::SymExprArena& nativeArena);
    NeoMathResult differentiate(const NeoValue& value,
                                const std::string& variable,
                                cas::SymExprArena& nativeArena);
    NeoMathResult integrate(const NeoValue& value,
                            const std::string& variable,
                            cas::SymExprArena& nativeArena);
    NeoMathResult solve(const NeoValue& equationOrResidual,
                        const std::string& variable,
                        cas::SymExprArena& nativeArena);
    NeoMathResult solveSystem(const std::vector<NeoValue>& equations,
                              const std::vector<std::string>& variables,
                              cas::SymExprArena& nativeArena);
    NeoMathResult taylor(const NeoValue& value, const std::string& variable,
                         const NeoValue& center, int order,
                         cas::SymExprArena& nativeArena);

    std::unique_ptr<NeoCompiledPlot> compilePlot(
        const NeoValue& value, const std::string& variable,
        cas::SymExprArena& nativeArena);
    bool evaluatePlot(NeoCompiledPlot& plot, double x, double& out,
                      cas::SymExprArena& nativeArena);

    NeoValue symbol(const std::string& name,
                    cas::SymExprArena& nativeArena);
    NeoValue constant(const std::string& name,
                      cas::SymExprArena& nativeArena);
    bool variableName(const NeoValue& value, std::string& out) const;
    bool numericValue(const NeoValue& value, double& out,
                      cas::SymExprArena& nativeArena);

    uint32_t count(NeoMathEngine engine, NeoMathOperation operation) const;
    numos::MathEngineStatus lastStatus() const { return _lastStatus; }
    uint32_t plotCompileCount() const { return _plotCompileCount; }

private:
    using SymbolMap = std::vector<std::pair<std::string, std::string>>;

    NeoMathEngine _engine = NeoMathEngine::Giac;
    std::vector<std::unique_ptr<numos::EngineResultNode>> _trees;
    std::vector<std::unique_ptr<std::vector<NeoValue>>> _lists;
    std::vector<std::unique_ptr<std::map<std::string, NeoValue>>> _dicts;
    std::array<std::array<uint32_t,
        static_cast<size_t>(NeoMathOperation::Count)>, 2> _counts{};
    numos::MathEngineStatus _lastStatus =
        numos::MathEngineStatus::Unsupported;
    uint32_t _plotCompileCount = 0;

    const numos::EngineResultNode* own(numos::EngineResultNode tree);
    std::vector<NeoValue>* ownList();
    std::map<std::string, NeoValue>* ownDict();
    NeoValue valueFromTree(numos::EngineResultNode tree,
                           bool unevaluated = false);
    NeoMathResult resultFromGiac(numos::StructuredEngineResult result,
                                 const SymbolMap& symbols,
                                 bool unevaluated = false);
    NeoMathResult error(numos::MathEngineStatus status,
                        const std::string& diagnostic);
    void note(NeoMathOperation operation,
              numos::MathEngineStatus status);

    bool serialize(const NeoValue& value, std::string& out,
                   SymbolMap& symbols, std::string& diagnostic) const;
    bool serializeTree(const numos::EngineResultNode& tree, std::string& out,
                       SymbolMap& symbols, int depth, int& budget,
                       std::string& diagnostic) const;
    bool nativeTree(const cas::SymExpr* expr, numos::EngineResultNode& out,
                    int depth, int& budget) const;
    cas::SymExpr* treeToNative(const numos::EngineResultNode& tree,
                               cas::SymExprArena& arena, int depth,
                               int& budget) const;
    cas::SymExpr* toNative(const NeoValue& value,
                           cas::SymExprArena& arena) const;
    void restoreSymbols(numos::EngineResultNode& tree,
                        const SymbolMap& symbols) const;
};
