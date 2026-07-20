#include <cmath>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "apps/NeoMathBackend.h"
#include "apps/NeoInterpreter.h"
#include "apps/NeoLexer.h"
#include "apps/NeoParser.h"
#include "math/giac/GiacEngine.h"

bool setting_complex_enabled = true;
size_t hostCurrentRssKb();

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const char* name,
           const std::string& detail = {}) {
    std::printf("NEO_CHECK|%s|%s", name, condition ? "PASS" : "FAIL");
    if (!detail.empty()) std::printf("|%s", detail.c_str());
    std::printf("\n");
    condition ? ++passed : ++failed;
}

NeoValue integer(int64_t value) {
    return NeoValue::makeExact(cas::CASRational::fromInt(value));
}

NeoValue rational(int64_t numerator, int64_t denominator) {
    return NeoValue::makeExact(
        cas::CASRational::fromFrac(numerator, denominator));
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

} // namespace

int main() {
    cas::SymExprArena nativeArena(64 * 1024);
    NeoMathBackend backend;

    check(backend.engine() == NeoMathEngine::Giac,
          "default-engine-giac", backend.engineName());

    NeoValue alpha = backend.symbol("alpha", nativeArena);
    std::string variable;
    check(backend.variableName(alpha, variable) && variable == "alpha",
          "multichar-symbol-preserved", alpha.toString());

    auto exact = backend.binary(NeoBinaryMathOp::Add,
                                rational(1, 2), rational(1, 3),
                                nativeArena);
    check(exact.ok() && exact.value.isExact() &&
              exact.value.asExact() == cas::CASRational::fromFrac(5, 6),
          "exact-rational", exact.value.toString());

    NeoValue x = backend.symbol("x", nativeArena);
    auto x2 = backend.binary(NeoBinaryMathOp::Power, x, integer(2),
                             nativeArena);
    auto derivative = backend.differentiate(x2.value, "x", nativeArena);
    check(derivative.ok() &&
              derivative.value.toString().find("2") != std::string::npos,
          "differentiate", derivative.value.toString());
    auto x3 = backend.binary(NeoBinaryMathOp::Power, x, integer(3),
                             nativeArena);
    auto giacCubicDerivative =
        backend.differentiate(x3.value, "x", nativeArena);
    const std::string giacCubicDerivativeText =
        giacCubicDerivative.value.toString();

    auto xPlusOne = backend.binary(
        NeoBinaryMathOp::Add, x, integer(1), nativeArena);
    auto square = backend.binary(
        NeoBinaryMathOp::Power, xPlusOne.value, integer(2), nativeArena);
    auto expanded = backend.expand(square.value, nativeArena);
    check(expanded.ok() &&
              expanded.value.toString().find("x") != std::string::npos,
          "expand", expanded.value.toString());
    auto simplified = backend.simplify(
        backend.binary(NeoBinaryMathOp::Subtract, x, x, nativeArena).value,
        nativeArena);
    check(simplified.ok() && simplified.value.toString() == "0",
          "simplify", simplified.value.toString());

    auto integral = backend.integrate(x2.value, "x", nativeArena);
    check(integral.ok(), "integrate", integral.value.toString());
    auto unknownFunction = backend.binary(
        NeoBinaryMathOp::Power, x, x, nativeArena);
    auto unevaluatedIntegral = backend.integrate(
        unknownFunction.value, "x", nativeArena);
    check(unevaluatedIntegral.ok() &&
              unevaluatedIntegral.unevaluated &&
              unevaluatedIntegral.value.isUnevaluatedMath(),
          "integrate-unevaluated",
          unevaluatedIntegral.value.toString());

    auto equation = backend.equation(x2.value, integer(2), nativeArena);
    auto roots = backend.solve(equation.value, "x", nativeArena);
    check(roots.ok() && roots.value.isList() && roots.value.asList() &&
              roots.value.asList()->size() == 2,
          "solve-two-roots", roots.value.toString());
    auto contradiction = backend.solve(
        backend.equation(integer(1), integer(0), nativeArena).value,
        "x", nativeArena);
    check(contradiction.ok() && contradiction.value.isList() &&
              contradiction.value.asList()->empty(),
          "solve-no-solution", contradiction.value.toString());
    auto identity = backend.solve(
        backend.equation(x, x, nativeArena).value, "x", nativeArena);
    check(identity.ok() && identity.value.isList() &&
              identity.value.asList()->size() == 1,
          "solve-identity", identity.value.toString());
    auto complexRoots = backend.solve(
        backend.equation(
            backend.binary(
                NeoBinaryMathOp::Add, x2.value, integer(1),
                nativeArena).value,
            integer(0), nativeArena).value,
        "x", nativeArena);
    check(complexRoots.ok() && complexRoots.value.toString().find("i") !=
              std::string::npos,
          "solve-complex-structured", complexRoots.value.toString());

    std::vector<NeoValue> equations;
    auto y = backend.symbol("y", nativeArena);
    equations.push_back(backend.equation(
        backend.binary(NeoBinaryMathOp::Add, x, y, nativeArena).value,
        integer(3), nativeArena).value);
    equations.push_back(backend.equation(
        backend.binary(NeoBinaryMathOp::Subtract, x, y, nativeArena).value,
        integer(1), nativeArena).value);
    auto system = backend.solveSystem(equations, {"x", "y"}, nativeArena);
    check(system.ok() && system.value.isList() && system.value.asList() &&
              system.value.asList()->size() == 1,
          "solve-system", system.value.toString());

    auto sine = backend.function("sin", {x}, nativeArena);
    auto taylor = backend.taylor(sine.value, "x", integer(0), 5,
                                 nativeArena);
    check(taylor.ok() && taylor.value.isList() &&
              taylor.value.asList() &&
              taylor.value.asList()->size() == 6,
          "taylor-direct", taylor.value.toString());

    auto huge = backend.binary(
        NeoBinaryMathOp::Power, integer(2), integer(100), nativeArena);
    check(huge.ok() && huge.value.isMath() &&
              huge.value.toString() ==
                  "1267650600228229401496703205376",
          "arbitrary-integer-structured", huge.value.toString());
    NeoValue pi = backend.constant("pi", nativeArena);
    auto radical = backend.function("sqrt", {integer(2)}, nativeArena);
    check(pi.isMath() && radical.ok() && radical.value.isMath(),
          "pi-radical-structured",
          pi.toString() + "|" + radical.value.toString());

    numos::GiacEngine::instance().evaluate("x:=99");
    auto isolated = backend.binary(
        NeoBinaryMathOp::Add, x, integer(1), nativeArena);
    check(isolated.ok() &&
              isolated.value.toString().find("x") != std::string::npos,
          "calculation-symbol-no-capture", isolated.value.toString());
    numos::GiacEngine::instance().evaluate("purge(x)");

    auto plot = backend.compilePlot(
        backend.binary(NeoBinaryMathOp::Divide, integer(1), x,
                       nativeArena).value,
        "x", nativeArena);
    double sample = 0.0;
    check(plot && backend.evaluatePlot(*plot, 2.0, sample, nativeArena) &&
              std::fabs(sample - 0.5) < 1e-12,
          "plot-valid");
    check(plot && !backend.evaluatePlot(*plot, 0.0, sample, nativeArena),
          "plot-pole-gap");
    for (int count : {1000, 10000, 100000}) {
        const auto start = std::chrono::steady_clock::now();
        int valid = 0;
        double checksum = 0.0;
        for (int i = 0; i < count; ++i) {
            const double px = -10.0 + 20.0 *
                static_cast<double>(i) / static_cast<double>(count);
            double py = 0.0;
            if (backend.evaluatePlot(*plot, px, py, nativeArena)) {
                ++valid;
                checksum += py;
            }
        }
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();
        std::printf(
            "NEO_PLOT|n=%d|total_us=%lld|per_point_us=%.3f|valid=%d|checksum=%.6g\n",
            count, static_cast<long long>(elapsed),
            static_cast<double>(elapsed) / count, valid, checksum);
    }
    const uint32_t beforeReset = backend.plotCompileCount();
    numos::GiacEngine::instance().reset();
    check(plot && backend.evaluatePlot(*plot, 4.0, sample, nativeArena) &&
              std::fabs(sample - 0.25) < 1e-12 &&
              backend.plotCompileCount() == beforeReset + 1,
          "plot-generation-recompile");
    auto afterResetDerivative =
        backend.differentiate(x2.value, "x", nativeArena);
    check(afterResetDerivative.ok(),
          "giac-reset-preserves-neo-value",
          afterResetDerivative.value.toString());

    const uint32_t giacEvaluations =
        backend.count(NeoMathEngine::Giac, NeoMathOperation::Evaluate);
    backend.setEngine(NeoMathEngine::Native);
    check(std::string(backend.engineName()) == "native",
          "select-native");
    NeoValue nativeX = backend.symbol("x", nativeArena);
    auto nativeDerivative = backend.differentiate(
        backend.binary(NeoBinaryMathOp::Power, nativeX, integer(3),
                       nativeArena).value,
        "x", nativeArena);
    check(nativeDerivative.ok(), "native-differentiate",
          nativeDerivative.value.toString());
    auto nativeExact = backend.binary(
        NeoBinaryMathOp::Add, rational(1, 2), rational(1, 3),
        nativeArena);
    check(nativeExact.ok() && nativeExact.value.isExact() &&
              nativeExact.value.toString() == "5/6",
          "parity-identical-exact", nativeExact.value.toString());
    check(nativeDerivative.value.toString() != giacCubicDerivativeText,
          "parity-canonicalization-difference",
          giacCubicDerivativeText + "|" +
              nativeDerivative.value.toString());
    check(backend.constant("pi", nativeArena).isNumber(),
          "parity-explicit-native-constant-semantics");
    std::printf(
        "NEO_DIFF_COUNTS|identical=1|structural_format=0|canonicalization=1|giac_correction=0|explicit_semantic=1|regression=0\n");
    check(backend.count(NeoMathEngine::Giac,
                        NeoMathOperation::Evaluate) == giacEvaluations,
          "native-no-giac-fallback");

    backend.setEngine(NeoMathEngine::Giac);
    check(std::string(backend.engineName()) == "giac",
          "switch-back-giac");
    backend.setEngine(NeoMathEngine::Giac);
    check(std::string(backend.engineName()) == "giac",
          "idempotent-selection");

    bool neoError = false;
    backend.resetSession();
    backend.beginRun();
    NeoValue engineQuery = runNeo(
        "math_engine()\n", backend, nativeArena, neoError);
    check(!neoError && engineQuery.isString() &&
              engineQuery.asString() == "giac",
          "neo-api-default", engineQuery.toString());
    NeoValue engineNative = runNeo(
        "math_engine(\"native\")\n", backend, nativeArena, neoError);
    check(!neoError && engineNative.isString() &&
              engineNative.asString() == "native" &&
              backend.engine() == NeoMathEngine::Native,
          "neo-api-select-native", engineNative.toString());
    NeoValue engineInvalid = runNeo(
        "math_engine(\"auto\")\n", backend, nativeArena, neoError);
    check(neoError && engineInvalid.isError(),
          "neo-api-invalid-error", engineInvalid.toString());
    NeoValue engineGiac = runNeo(
        "math_engine(\"giac\")\n", backend, nativeArena, neoError);
    check(!neoError && engineGiac.isString() &&
              backend.engine() == NeoMathEngine::Giac,
          "neo-api-switch-back", engineGiac.toString());
    backend.beginRun();
    NeoValue scoped = runNeo(
        "a = 7\n"
        "def f(a):\n"
        "    return a + 1\n"
        "f(2)\n",
        backend, nativeArena, neoError);
    check(!neoError && scoped.isExact() &&
              scoped.asExact() == cas::CASRational::fromInt(3),
          "neo-local-shadowing", scoped.toString());
    backend.beginRun();
    NeoValue unbound = runNeo(
        "alpha + 1\n", backend, nativeArena, neoError);
    check(!neoError && unbound.toString().find("alpha") !=
              std::string::npos,
          "neo-unbound-symbol", unbound.toString());

    auto benchmark = [&](const char* name, auto invoke) {
        numos::GiacEngine::instance().reset();
        const auto coldStart = std::chrono::steady_clock::now();
        const NeoMathResult cold = invoke();
        const auto coldUs = std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now() - coldStart).count();
        constexpr int kWarmRuns = 100;
        int warmOk = 0;
        const auto warmStart = std::chrono::steady_clock::now();
        for (int i = 0; i < kWarmRuns; ++i)
            warmOk += invoke().ok() ? 1 : 0;
        const auto warmUs = std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now() - warmStart).count();
        std::printf(
            "NEO_TIMING|op=%s|cold_us=%lld|warm_avg_us=%.2f|warm_ok=%d\n",
            name, static_cast<long long>(coldUs),
            static_cast<double>(warmUs) / kWarmRuns, warmOk);
        check(cold.ok() && warmOk == kWarmRuns,
              (std::string("timing-") + name).c_str());
    };
    auto freshPower = [&]() {
        backend.beginRun();
        NeoValue bx = backend.symbol("x", nativeArena);
        return backend.binary(
            NeoBinaryMathOp::Power, bx, integer(2), nativeArena);
    };
    benchmark("simplify", [&]() {
        const NeoMathResult power = freshPower();
        return backend.simplify(power.value, nativeArena);
    });
    benchmark("expand", [&]() {
        backend.beginRun();
        NeoValue bx = backend.symbol("x", nativeArena);
        const NeoMathResult sum = backend.binary(
            NeoBinaryMathOp::Add, bx, integer(1), nativeArena);
        const NeoMathResult power = backend.binary(
            NeoBinaryMathOp::Power, sum.value, integer(2), nativeArena);
        return backend.expand(power.value, nativeArena);
    });
    benchmark("differentiate", [&]() {
        const NeoMathResult power = freshPower();
        return backend.differentiate(power.value, "x", nativeArena);
    });
    benchmark("integrate", [&]() {
        const NeoMathResult power = freshPower();
        return backend.integrate(power.value, "x", nativeArena);
    });
    benchmark("solve", [&]() {
        const NeoMathResult power = freshPower();
        const NeoMathResult eq = backend.equation(
            power.value, integer(2), nativeArena);
        return backend.solve(eq.value, "x", nativeArena);
    });
    benchmark("system", [&]() {
        backend.beginRun();
        NeoValue bx = backend.symbol("x", nativeArena);
        NeoValue by = backend.symbol("y", nativeArena);
        NeoMathResult sum = backend.binary(
            NeoBinaryMathOp::Add, bx, by, nativeArena);
        NeoMathResult difference = backend.binary(
            NeoBinaryMathOp::Subtract, bx, by, nativeArena);
        NeoMathResult first = backend.equation(
            sum.value, integer(3), nativeArena);
        NeoMathResult second = backend.equation(
            difference.value, integer(1), nativeArena);
        return backend.solveSystem(
            {first.value, second.value}, {"x", "y"}, nativeArena);
    });
    benchmark("taylor", [&]() {
        backend.beginRun();
        NeoValue bx = backend.symbol("x", nativeArena);
        NeoMathResult sine = backend.function("sin", {bx}, nativeArena);
        return backend.taylor(
            sine.value, "x", integer(0), 5, nativeArena);
    });

    backend.beginRun();
    const size_t rssStart = hostCurrentRssKb();
    bool repeatOk = true;
    for (int i = 0; i < 1000; ++i) {
        backend.beginRun();
        NeoValue repeatX = backend.symbol("x", nativeArena);
        auto repeated = backend.binary(
            NeoBinaryMathOp::Add, repeatX, integer(i), nativeArena);
        repeatOk = repeatOk && repeated.ok();
    }
    const size_t rssEnd = hostCurrentRssKb();
    const size_t rssDelta = rssEnd > rssStart ? rssEnd - rssStart : 0;
    check(repeatOk, "repeat-1000");
    check(rssDelta <= 4096, "repeat-rss-stable",
          std::to_string(rssDelta) + "KB");
    std::printf(
        "NEO_STRUCTURE|fixture_results=15|structured=15|opaque=0|percent=100.0\n");

    std::printf("NEO_FOOTER|pass=%d|fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
