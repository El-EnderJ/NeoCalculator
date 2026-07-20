// GIAC-F01 focused cross-app/context sequence.
//
// Drives production engine-neutral seams in the same order that applications
// share them.  This is intentionally a separate process so failures cannot be
// hidden by the larger feature suites.

#include <cmath>
#include <cstdio>
#include <string>

#include "apps/NeoEnv.h"
#include "apps/NeoInterpreter.h"
#include "apps/NeoLexer.h"
#include "apps/NeoMathBackend.h"
#include "apps/NeoParser.h"
#include "math/AngleModeRuntime.h"
#include "math/CalculationEngine.h"
#include "math/MathAST.h"
#include "math/giac/GiacEngine.h"

bool setting_complex_enabled = true;

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const char* name,
           const std::string& detail = {}) {
    std::printf("CROSS_CHECK|%s|%s", name, condition ? "PASS" : "FAIL");
    if (!detail.empty()) std::printf("|%s", detail.c_str());
    std::printf("\n");
    condition ? ++passed : ++failed;
}

NeoValue runNeo(const std::string& source, NeoMathBackend& backend,
                cas::SymExprArena& symArena, bool& hadError) {
    neo::NeoLexer lexer(source);
    const auto tokens = lexer.tokenize();
    NeoArena astArena(64 * 1024);
    NeoParser parser(astArena);
    NeoNode* root = parser.parse(tokens);
    if (!root || parser.hasError() || !lexer.lastError().empty()) {
        hadError = true;
        return NeoValue::makeError(
            parser.hasError() ? parser.lastError() : lexer.lastError());
    }
    NeoInterpreter interpreter(symArena, backend);
    NeoEnv environment;
    NeoValue value = interpreter.eval(root, environment);
    hadError = interpreter.hasError();
    return value;
}

vpam::NodePtr calculationFixture() {
    auto row = vpam::makeRow();
    auto* children = static_cast<vpam::NodeRow*>(row.get());
    children->appendChild(vpam::makeNumber("1"));
    children->appendChild(vpam::makeOperator(vpam::OpKind::Add));
    children->appendChild(vpam::makeNumber("2"));
    return row;
}

} // namespace

int main() {
    auto& engine = numos::GiacEngine::instance();
    auto& calculation = numos::CalculationEngine::instance();
    cas::SymExprArena nativeArena(96 * 1024);
    NeoMathBackend neo;
    bool neoError = false;

    engine.reset();
    numos::setAngleMode(vpam::AngleMode::RAD);

    // 1. Calculation evaluation and assignment.
    auto authored = calculationFixture();
    const auto calc = calculation.evaluate(authored.get());
    check(calc.ok() && calc.exactText == "3",
          "01-calculation-evaluation", calc.exactText);
    const auto assignment = engine.assign("A", "11");
    const auto assigned = engine.evaluate("A");
    check(assignment.ok() && assigned.ok() && assigned.exactText == "11",
          "01-calculation-assignment", assigned.exactText);

    // 2. Grapher-shaped retained expression.
    int graphCompileCount = 1;
    auto graph = engine.compileNumeric("sin(x)", "x", true);
    double graphSample = NAN;
    check(graph.valid() &&
              engine.evaluateNumeric(graph, 0.5, graphSample) &&
              std::fabs(graphSample - std::sin(0.5)) < 1e-12,
          "02-grapher-retained-expression");

    // 3. NeoLanguage local scope and free-symbol preservation.
    neo.beginRun();
    const NeoValue local = runNeo(
        "a = 7\n"
        "def f(a):\n"
        "    return a + 1\n"
        "f(2)\n",
        neo, nativeArena, neoError);
    check(!neoError && local.toString() == "3",
          "03-neo-local-shadowing", local.toString());
    neo.beginRun();
    const NeoValue freeSymbol =
        runNeo("alpha + 1\n", neo, nativeArena, neoError);
    check(!neoError &&
              freeSymbol.toString().find("alpha") != std::string::npos,
          "03-neo-free-symbol", freeSymbol.toString());

    // 4. DEG Calculus differentiation; scoped RAD mode must restore DEG.
    numos::setAngleMode(vpam::AngleMode::DEG);
    numos::CalculusRequest derivativeRequest;
    derivativeRequest.operation = numos::CalculusOperation::Differentiate;
    derivativeRequest.expression = "sin(x)";
    derivativeRequest.variable = "x";
    const auto derivative =
        engine.evaluateCalculusStructured(derivativeRequest);
    const auto degreeProbe = engine.evaluate("sin(30)");
    check(derivative.ok() && degreeProbe.ok() &&
              degreeProbe.exactText == "1/2",
          "04-deg-calculus-restores-angle",
          derivative.exactText + "|" + degreeProbe.exactText);

    // A failing calculus request must not poison the next app.
    derivativeRequest.expression = "2+*3";
    const auto badDerivative =
        engine.evaluateCalculusStructured(derivativeRequest);
    check(!badDerivative.ok(), "04-calculus-failure-is-contained");

    // 5. RAD Equation solving.
    numos::setAngleMode(vpam::AngleMode::RAD);
    numos::SolveEquation equation{"x^2", "4"};
    const auto solved = engine.solveStructured(
        equation, "x", numos::SolveDomainPolicy::RealOnly);
    check(solved.ok() &&
              solved.setKind == numos::SolutionSetKind::Solutions &&
              solved.groups.size() == 2,
          "05-rad-equation-solving",
          std::to_string(solved.groups.size()));

    // 6. Neo retained plot.
    neo.beginRun();
    NeoValue neoX = neo.symbol("x", nativeArena);
    auto neoSquare = neo.binary(
        NeoBinaryMathOp::Power, neoX,
        NeoValue::makeExact(cas::CASRational::fromInt(2)), nativeArena);
    auto neoPlot = neo.compilePlot(neoSquare.value, "x", nativeArena);
    double neoSample = NAN;
    check(neoPlot && neo.evaluatePlot(
              *neoPlot, 3.0, neoSample, nativeArena) &&
              std::fabs(neoSample - 9.0) < 1e-12,
          "06-neo-retained-plot");

    // 7-8. Reset rejects both stale handles. Neo recompiles once; the
    // Grapher-shaped lane performs one deterministic retained-source compile.
    const uint32_t generationBefore = engine.generation();
    const uint32_t neoCompilesBefore = neo.plotCompileCount();
    engine.reset();
    check(engine.generation() == generationBefore + 1 &&
              !graph.valid() &&
              !engine.evaluateNumeric(graph, 0.5, graphSample),
          "07-reset-invalidates-grapher-handle");
    check(neoPlot && neo.evaluatePlot(
              *neoPlot, 4.0, neoSample, nativeArena) &&
              std::fabs(neoSample - 16.0) < 1e-12 &&
              neo.plotCompileCount() == neoCompilesBefore + 1,
          "08-neo-stale-recompiled-once");
    graph = engine.compileNumeric("sin(x)", "x", true);
    ++graphCompileCount;
    check(graphCompileCount == 2 && graph.valid() &&
              engine.evaluateNumeric(graph, 0.5, graphSample),
          "08-grapher-stale-recompiled-once");

    // 9. Calculation remains deterministic after reset.
    const auto repeated = calculation.evaluate(authored.get());
    check(repeated.ok() && repeated.exactText == "3",
          "09-calculation-after-reset", repeated.exactText);

    // 10. Neo Native is session-local and does not alter another app.
    const uint32_t giacNeoBefore = neo.count(
        NeoMathEngine::Giac, NeoMathOperation::Differentiate);
    neo.setEngine(NeoMathEngine::Native);
    neo.beginRun();
    NeoValue nativeX = neo.symbol("x", nativeArena);
    auto nativeSquare = neo.binary(
        NeoBinaryMathOp::Power, nativeX,
        NeoValue::makeExact(cas::CASRational::fromInt(2)), nativeArena);
    const auto nativeDerivative =
        neo.differentiate(nativeSquare.value, "x", nativeArena);
    const auto independentCalculation = calculation.evaluate(authored.get());
    check(nativeDerivative.ok() && independentCalculation.ok() &&
              independentCalculation.exactText == "3" &&
              neo.count(NeoMathEngine::Giac,
                        NeoMathOperation::Differentiate) == giacNeoBefore,
          "10-native-selection-is-session-local");

    neo.setEngine(NeoMathEngine::Giac);
    std::printf(
        "CROSS_FOOTER|pass=%d|fail=%d|generation=%u|graph_compiles=%d|neo_plot_compiles=%u\n",
        passed, failed, static_cast<unsigned>(engine.generation()),
        graphCompileCount, static_cast<unsigned>(neo.plotCompileCount()));
    return failed == 0 ? 0 : 1;
}
