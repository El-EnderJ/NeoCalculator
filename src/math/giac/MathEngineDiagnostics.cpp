/*
 * GIAC-F01 deterministic ESP32 whole-engine diagnostics and mixed-operation
 * soak. Compiled only by esp32s3_n16r8_mathdiag.
 */

#if defined(NUMOS_MATH_ENGINE_DIAGNOSTICS) && defined(ARDUINO)

#include "MathEngineDiagnostics.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "apps/NeoMathBackend.h"
#include "input/NumosSerialBackend.h"
#include "math/AngleModeRuntime.h"
#include "math/CalculationEngine.h"
#include "math/MathAST.h"
#include "math/cas/SymExprArena.h"
#include "math/giac/GiacEngine.h"

namespace numos {
namespace {

struct HeapSnapshot {
    size_t internalFree = 0;
    size_t internalLargest = 0;
    size_t psramFree = 0;
    size_t psramLargest = 0;
    unsigned stackHighWater = 0;
};

HeapSnapshot heapSnapshot() {
    HeapSnapshot out;
    out.internalFree =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    out.internalLargest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    out.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out.psramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    out.stackHighWater =
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr));
    return out;
}

const char* statusName(MathEngineStatus status) {
    switch (status) {
        case MathEngineStatus::Ok: return "ok";
        case MathEngineStatus::Undefined: return "undefined";
        case MathEngineStatus::ParseError: return "parse_error";
        case MathEngineStatus::EvaluationError: return "evaluation_error";
        case MathEngineStatus::Unsupported: return "unsupported";
        case MathEngineStatus::OutOfMemory: return "out_of_memory";
    }
    return "unknown";
}

struct CaseOutcome {
    bool pass = false;
    MathEngineStatus status = MathEngineStatus::Unsupported;
    const char* kind = "opaque";
    int retainedCompileCount = 0;
};

const char* structuredKind(bool hasTree) {
    return hasTree ? "structured" : "text";
}

template <typename Invoke>
bool runCase(const char* section, const char* name, Invoke&& invoke,
             int warmRuns = 2) {
    HeapSnapshot before = heapSnapshot();
    size_t minimumInternal = before.internalFree;
    size_t minimumPsram = before.psramFree;

    const uint32_t coldStart = micros();
    const CaseOutcome cold = invoke();
    const uint32_t coldUs = micros() - coldStart;
    HeapSnapshot sample = heapSnapshot();
    minimumInternal = std::min(minimumInternal, sample.internalFree);
    minimumPsram = std::min(minimumPsram, sample.psramFree);

    uint64_t warmTotal = 0;
    uint32_t warmMaximum = 0;
    bool warmPass = true;
    for (int i = 0; i < warmRuns; ++i) {
        const uint32_t warmStart = micros();
        const CaseOutcome warm = invoke();
        const uint32_t elapsed = micros() - warmStart;
        warmTotal += elapsed;
        warmMaximum = std::max(warmMaximum, elapsed);
        warmPass = warmPass && warm.pass;
        sample = heapSnapshot();
        minimumInternal = std::min(minimumInternal, sample.internalFree);
        minimumPsram = std::min(minimumPsram, sample.psramFree);
        delay(0);  // WHY: preserve the normal task watchdog contract.
    }
    HeapSnapshot after = heapSnapshot();
    minimumInternal = std::min(minimumInternal, after.internalFree);
    minimumPsram = std::min(minimumPsram, after.psramFree);
    const bool pass = cold.pass && warmPass;
    const uint32_t warmAverage = warmRuns > 0
        ? static_cast<uint32_t>(warmTotal / static_cast<uint64_t>(warmRuns))
        : 0;

    char line[512];
    std::snprintf(
        line, sizeof(line),
        "[MATHDIAG]|section=%s|case=%s|pass=%d|status=%s|cold_us=%u|"
        "warm_avg_us=%u|warm_max_us=%u|int_before=%u|int_min=%u|"
        "int_after=%u|psram_before=%u|psram_min=%u|psram_after=%u|"
        "int_largest=%u|psram_largest=%u|stack_hwm=%u|generation=%u|"
        "kind=%s|retained_compile_count=%d",
        section, name, pass ? 1 : 0, statusName(cold.status),
        static_cast<unsigned>(coldUs), static_cast<unsigned>(warmAverage),
        static_cast<unsigned>(warmMaximum),
        static_cast<unsigned>(before.internalFree),
        static_cast<unsigned>(minimumInternal),
        static_cast<unsigned>(after.internalFree),
        static_cast<unsigned>(before.psramFree),
        static_cast<unsigned>(minimumPsram),
        static_cast<unsigned>(after.psramFree),
        static_cast<unsigned>(after.internalLargest),
        static_cast<unsigned>(after.psramLargest),
        after.stackHighWater,
        static_cast<unsigned>(GiacEngine::instance().generation()),
        cold.kind, cold.retainedCompileCount);
    NUMOS_SERIAL.println(line);
    return pass;
}

vpam::NodePtr rowWith(vpam::NodePtr child) {
    auto row = vpam::makeRow();
    static_cast<vpam::NodeRow*>(row.get())->appendChild(std::move(child));
    return row;
}

vpam::NodePtr fractionSumFixture() {
    auto row = vpam::makeRow();
    auto* children = static_cast<vpam::NodeRow*>(row.get());
    children->appendChild(vpam::makeFraction(
        rowWith(vpam::makeNumber("1")), rowWith(vpam::makeNumber("2"))));
    children->appendChild(vpam::makeOperator(vpam::OpKind::Add));
    children->appendChild(vpam::makeFraction(
        rowWith(vpam::makeNumber("1")), rowWith(vpam::makeNumber("3"))));
    return row;
}

vpam::NodePtr powerFixture() {
    return rowWith(vpam::makePower(
        rowWith(vpam::makeNumber("2")), rowWith(vpam::makeNumber("100"))));
}

vpam::NodePtr sqrtFixture() {
    return rowWith(vpam::makeRoot(rowWith(vpam::makeNumber("2"))));
}

vpam::NodePtr sineFixture() {
    auto fraction = vpam::makeFraction(
        rowWith(vpam::makeConstant(vpam::ConstKind::Pi)),
        rowWith(vpam::makeNumber("6")));
    return rowWith(vpam::makeFunction(
        vpam::FuncKind::Sin, rowWith(std::move(fraction))));
}

CaseOutcome calculationOutcome(const vpam::MathNode* authored,
                               const char* expected) {
    const CalculationEvaluation result =
        CalculationEngine::instance().evaluate(authored);
    const bool textMatches =
        result.exactText.find(expected) != std::string::npos;
    const char* kind = result.kind == CalcResultKind::Structured
        ? "structured"
        : result.kind == CalcResultKind::TextFallback ? "text" : "opaque";
    return {result.ok() && textMatches, result.status, kind, 0};
}

CaseOutcome solveOutcome(const StructuredSolveResult& result,
                         SolutionSetKind expectedKind,
                         size_t expectedGroups = std::numeric_limits<size_t>::max()) {
    const bool groupsMatch =
        expectedGroups == std::numeric_limits<size_t>::max() ||
        result.groups.size() == expectedGroups;
    return {result.ok() && result.setKind == expectedKind && groupsMatch,
            result.status, "structured", 0};
}

NeoValue neoInteger(int64_t value) {
    return NeoValue::makeExact(cas::CASRational::fromInt(value));
}

const char* neoKind(const NeoValue& value) {
    if (value.isOpaqueMath()) return "opaque";
    if (value.isMath() || value.isExact() || value.isList()) return "structured";
    return "text";
}

uint32_t totalNeoCount(const NeoMathBackend& backend,
                       NeoMathEngine engine) {
    uint32_t total = 0;
    for (size_t i = 0;
         i < static_cast<size_t>(NeoMathOperation::Count); ++i) {
        total += backend.count(engine, static_cast<NeoMathOperation>(i));
    }
    return total;
}

} // namespace

void runMathEngineDiagnostics() {
    NUMOS_SERIAL.println(
        "[MATHDIAG]|event=begin|environment=esp32s3_n16r8_mathdiag");
    GiacEngine& engine = GiacEngine::instance();
    engine.reset();
    int passCount = 0;
    int failCount = 0;
    auto record = [&](bool pass) {
        pass ? ++passCount : ++failCount;
    };

    // Calculation: authored MathAST -> CalculationEngine -> Giac.
    {
        auto authored = fractionSumFixture();
        record(runCase("calculation", "fraction_sum",
            [&]() { return calculationOutcome(authored.get(), "5/6"); }));
    }
    {
        auto authored = powerFixture();
        record(runCase("calculation", "power_2_100",
            [&]() {
                return calculationOutcome(
                    authored.get(), "1267650600228229401496703205376");
            }));
    }
    {
        auto authored = sqrtFixture();
        record(runCase("calculation", "sqrt_2",
            [&]() { return calculationOutcome(authored.get(), "sqrt(2)"); }));
    }
    {
        auto authored = sineFixture();
        record(runCase("calculation", "sin_pi_over_6",
            [&]() { return calculationOutcome(authored.get(), "1/2"); }));
        record(runCase("calculation", "repeated_evaluation",
            [&]() { return calculationOutcome(authored.get(), "1/2"); }, 8));
    }

    // Grapher retained expressions, discontinuities, domains, sweeps, grids.
    {
        CompiledExpression retained;
        int compiles = 0;
        record(runCase("grapher", "retained_sin_x", [&]() {
            if (!retained.valid()) {
                retained = engine.compileNumeric("sin(x)", "x", true);
                ++compiles;
            }
            double value = NAN;
            const bool ok = retained.valid() &&
                engine.evaluateNumeric(retained, 0.5, value);
            return CaseOutcome{
                ok && std::fabs(value - std::sin(0.5)) < 1e-12,
                ok ? MathEngineStatus::Ok : MathEngineStatus::EvaluationError,
                "structured", compiles};
        }, 8));
    }
    {
        CompiledExpression retained;
        int compiles = 0;
        record(runCase("grapher", "one_over_x_pole_gap", [&]() {
            if (!retained.valid()) {
                retained = engine.compileNumeric("1/x", "x", true);
                ++compiles;
            }
            double value = NAN;
            const bool evaluated =
                engine.evaluateNumeric(retained, 0.0, value);
            const bool gap = !evaluated || !std::isfinite(value);
            return CaseOutcome{retained.valid() && gap,
                gap ? MathEngineStatus::Ok : MathEngineStatus::EvaluationError,
                "structured", compiles};
        }));
    }
    {
        CompiledExpression retained;
        int compiles = 0;
        record(runCase("grapher", "sqrt_domain_rejection", [&]() {
            if (!retained.valid()) {
                retained = engine.compileNumeric("sqrt(x)", "x", true);
                ++compiles;
            }
            double value = NAN;
            const bool evaluated =
                engine.evaluateNumeric(retained, -1.0, value);
            const bool rejected = !evaluated || !std::isfinite(value);
            return CaseOutcome{retained.valid() && rejected,
                rejected ? MathEngineStatus::Ok
                         : MathEngineStatus::EvaluationError,
                "structured", compiles};
        }));
    }
    {
        CompiledExpression retained;
        int compiles = 0;
        record(runCase("grapher", "explicit_sweep_1000", [&]() {
            if (!retained.valid()) {
                retained = engine.compileNumeric("sin(x)", "x", true);
                ++compiles;
            }
            int valid = 0;
            double checksum = 0.0;
            for (int i = 0; i < 1000; ++i) {
                const double x = -5.0 + 10.0 * i / 999.0;
                double y = NAN;
                if (engine.evaluateNumeric(retained, x, y) &&
                    std::isfinite(y)) {
                    ++valid;
                    checksum += y;
                }
            }
            const bool ok = valid == 1000 && std::fabs(checksum) < 1e-8;
            return CaseOutcome{ok,
                ok ? MathEngineStatus::Ok
                   : MathEngineStatus::EvaluationError,
                "structured", compiles};
        }, 1));
    }
    {
        CompiledExpression retained;
        int compiles = 0;
        record(runCase("grapher", "residual_grid_2d", [&]() {
            if (!retained.valid()) {
                retained = engine.compileNumeric2D(
                    "x^2+y^2-1", "x", "y");
                ++compiles;
            }
            int valid = 0;
            for (int j = 0; j < 32; ++j) {
                const double y = -2.0 + 4.0 * j / 31.0;
                for (int i = 0; i < 32; ++i) {
                    const double x = -2.0 + 4.0 * i / 31.0;
                    double residual = NAN;
                    valid += engine.evaluateNumeric2D(
                        retained, x, y, residual) &&
                        std::isfinite(residual) ? 1 : 0;
                }
            }
            const bool ok = retained.valid() && valid == 1024;
            return CaseOutcome{ok,
                ok ? MathEngineStatus::Ok
                   : MathEngineStatus::EvaluationError,
                "structured", compiles};
        }, 1));
    }
    {
        CompiledExpression retained =
            engine.compileNumeric("sin(x)", "x", true);
        int compiles = 1;
        record(runCase("grapher", "reset_and_recompile", [&]() {
            engine.reset();
            double value = NAN;
            const bool staleRejected =
                !retained.valid() &&
                !engine.evaluateNumeric(retained, 0.25, value);
            retained = engine.compileNumeric("sin(x)", "x", true);
            ++compiles;
            const bool freshWorks = retained.valid() &&
                engine.evaluateNumeric(retained, 0.25, value);
            return CaseOutcome{staleRejected && freshWorks,
                staleRejected && freshWorks ? MathEngineStatus::Ok
                    : MathEngineStatus::EvaluationError,
                "structured", compiles};
        }, 1));
    }

    // Equations: typed one-equation and simultaneous-system boundaries.
    record(runCase("equations", "linear", [&]() {
        return solveOutcome(engine.solveStructured(
            {"2*x+4", "0"}, "x"), SolutionSetKind::Solutions, 1);
    }));
    record(runCase("equations", "quadratic_exact", [&]() {
        return solveOutcome(engine.solveStructured(
            {"x^2", "2"}, "x"), SolutionSetKind::Solutions, 2);
    }));
    record(runCase("equations", "cubic", [&]() {
        return solveOutcome(engine.solveStructured(
            {"x^3-6*x^2+11*x-6", "0"}, "x"),
            SolutionSetKind::Solutions, 3);
    }));
    record(runCase("equations", "transcendental", [&]() {
        return solveOutcome(engine.solveStructured(
            {"exp(x)", "2"}, "x"), SolutionSetKind::Solutions, 1);
    }));
    record(runCase("equations", "system_2x2", [&]() {
        return solveOutcome(engine.solveSystemStructured(
            {{"x+y", "3"}, {"x-y", "1"}}, {"x", "y"}),
            SolutionSetKind::Solutions, 1);
    }));
    record(runCase("equations", "system_3x3", [&]() {
        return solveOutcome(engine.solveSystemStructured(
            {{"x+y+z", "6"}, {"x-y+z", "2"}, {"x+y-z", "0"}},
            {"x", "y", "z"}), SolutionSetKind::Solutions, 1);
    }));
    record(runCase("equations", "contradiction", [&]() {
        return solveOutcome(engine.solveStructured(
            {"1", "0"}, "x"), SolutionSetKind::NoSolution, 0);
    }));
    record(runCase("equations", "identity", [&]() {
        return solveOutcome(engine.solveStructured(
            {"x", "x"}, "x"), SolutionSetKind::AllValues, 0);
    }));

    // Calculus: typed operation enum, structured result, unevaluated honesty.
    auto calculusCase = [&](const char* name, CalculusOperation operation,
                            const char* expression, const char* expected,
                            bool expectUnevaluated = false) {
        record(runCase("calculus", name, [&, operation, expression,
                                          expected, expectUnevaluated]() {
            CalculusRequest request{operation, expression, "x"};
            const StructuredCalculusResult result =
                engine.evaluateCalculusStructured(request);
            const bool textMatches = !expected ||
                result.exactText.find(expected) != std::string::npos;
            return CaseOutcome{
                result.ok() && textMatches &&
                    result.unevaluated == expectUnevaluated,
                result.status, structuredKind(result.hasTree), 0};
        }));
    };
    calculusCase("derivative_x2", CalculusOperation::Differentiate,
                 "x^2", "2*x");
    calculusCase("derivative_sin_squared", CalculusOperation::Differentiate,
                 "sin(x)^2", "2*cos(x)*sin(x)");
    calculusCase("integral_x2", CalculusOperation::IntegrateIndefinite,
                 "x^2", "x^3");
    calculusCase("integral_one_over_x",
                 CalculusOperation::IntegrateIndefinite, "1/x", "ln");
    calculusCase("integral_unevaluated",
                 CalculusOperation::IntegrateIndefinite, "x^x",
                 "integrate", true);

    // NeoLanguage engine-neutral value boundary, including explicit Native.
    cas::SymExprArena nativeArena(96 * 1024);
    NeoMathBackend neo;
    auto prepareNeo = [&]() {
        neo.beginRun();
        nativeArena.reset();
    };
    auto neoCase = [&](const char* name, auto&& operation) {
        record(runCase("neo", name, [&]() {
            prepareNeo();
            const NeoMathResult result = operation();
            return CaseOutcome{result.ok(), result.status,
                               neoKind(result.value),
                               static_cast<int>(neo.plotCompileCount())};
        }));
    };
    neoCase("simplify", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        return neo.simplify(neo.binary(
            NeoBinaryMathOp::Subtract, x, x, nativeArena).value, nativeArena);
    });
    neoCase("expand", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        const NeoMathResult sum = neo.binary(
            NeoBinaryMathOp::Add, x, neoInteger(1), nativeArena);
        return neo.expand(neo.binary(
            NeoBinaryMathOp::Power, sum.value, neoInteger(2),
            nativeArena).value, nativeArena);
    });
    neoCase("differentiate", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        return neo.differentiate(neo.binary(
            NeoBinaryMathOp::Power, x, neoInteger(2),
            nativeArena).value, "x", nativeArena);
    });
    neoCase("integrate", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        return neo.integrate(neo.binary(
            NeoBinaryMathOp::Power, x, neoInteger(2),
            nativeArena).value, "x", nativeArena);
    });
    neoCase("solve", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        const NeoMathResult square = neo.binary(
            NeoBinaryMathOp::Power, x, neoInteger(2), nativeArena);
        return neo.solve(neo.equation(
            square.value, neoInteger(2), nativeArena).value,
            "x", nativeArena);
    });
    neoCase("system", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        const NeoValue y = neo.symbol("y", nativeArena);
        const NeoMathResult sum = neo.binary(
            NeoBinaryMathOp::Add, x, y, nativeArena);
        const NeoMathResult difference = neo.binary(
            NeoBinaryMathOp::Subtract, x, y, nativeArena);
        return neo.solveSystem({
            neo.equation(sum.value, neoInteger(3), nativeArena).value,
            neo.equation(difference.value, neoInteger(1), nativeArena).value},
            {"x", "y"}, nativeArena);
    });
    neoCase("taylor", [&]() {
        const NeoValue x = neo.symbol("x", nativeArena);
        return neo.taylor(neo.function(
            "sin", {x}, nativeArena).value, "x", neoInteger(0), 5,
            nativeArena);
    });
    {
        prepareNeo();
        const NeoValue x = neo.symbol("x", nativeArena);
        const NeoMathResult square = neo.binary(
            NeoBinaryMathOp::Power, x, neoInteger(2), nativeArena);
        std::unique_ptr<NeoCompiledPlot> plot =
            neo.compilePlot(square.value, "x", nativeArena);
        record(runCase("neo", "retained_plot", [&]() {
            double value = NAN;
            const bool ok = plot &&
                neo.evaluatePlot(*plot, 3.0, value, nativeArena);
            return CaseOutcome{ok && std::fabs(value - 9.0) < 1e-12,
                ok ? MathEngineStatus::Ok
                   : MathEngineStatus::EvaluationError,
                "structured", static_cast<int>(neo.plotCompileCount())};
        }, 8));
        record(runCase("neo", "retained_plot_reset", [&]() {
            const uint32_t beforeCompile = neo.plotCompileCount();
            engine.reset();
            double value = NAN;
            const bool ok = plot &&
                neo.evaluatePlot(*plot, 4.0, value, nativeArena);
            return CaseOutcome{
                ok && std::fabs(value - 16.0) < 1e-12 &&
                    neo.plotCompileCount() == beforeCompile + 1,
                ok ? MathEngineStatus::Ok
                   : MathEngineStatus::EvaluationError,
                "structured", static_cast<int>(neo.plotCompileCount())};
        }, 1));
    }
    record(runCase("neo", "explicit_native_no_giac", [&]() {
        prepareNeo();
        neo.setEngine(NeoMathEngine::Native);
        const uint32_t giacBefore = totalNeoCount(neo, NeoMathEngine::Giac);
        const NeoValue x = neo.symbol("x", nativeArena);
        const NeoMathResult result = neo.differentiate(neo.binary(
            NeoBinaryMathOp::Power, x, neoInteger(3),
            nativeArena).value, "x", nativeArena);
        const bool isolated =
            totalNeoCount(neo, NeoMathEngine::Giac) == giacBefore;
        return CaseOutcome{result.ok() && isolated, result.status,
                           neoKind(result.value), 0};
    }));
    record(runCase("neo", "switch_back_giac", [&]() {
        neo.setEngine(NeoMathEngine::Giac);
        prepareNeo();
        const NeoValue x = neo.symbol("x", nativeArena);
        const NeoMathResult result = neo.simplify(x, nativeArena);
        return CaseOutcome{result.ok() &&
                neo.engine() == NeoMathEngine::Giac,
            result.status, neoKind(result.value), 0};
    }));

    // Cross-app invariants plus at least 100 alternating mixed-operation cycles.
    record(runCase("cross_app", "rad_deg_restore", [&]() {
        setAngleMode(vpam::AngleMode::DEG);
        const StructuredCalculusResult derivative =
            engine.evaluateCalculusStructured(
                {CalculusOperation::Differentiate, "sin(x)", "x"});
        const MathEngineResult degreeProbe = engine.evaluate("sin(30)");
        const bool ok = derivative.ok() && degreeProbe.ok() &&
            degreeProbe.exactText == "1/2" && angleModeIsDeg();
        return CaseOutcome{ok, ok ? MathEngineStatus::Ok
                                  : MathEngineStatus::EvaluationError,
                           structuredKind(derivative.hasTree), 0};
    }));
    record(runCase("cross_app", "variable_scoping_isolation", [&]() {
        engine.reset();
        const MathEngineResult assignment = engine.assign("A", "17");
        prepareNeo();
        neo.setEngine(NeoMathEngine::Giac);
        const NeoValue alpha = neo.symbol("alpha", nativeArena);
        const NeoMathResult free = neo.binary(
            NeoBinaryMathOp::Add, alpha, neoInteger(1), nativeArena);
        const MathEngineResult appValue = engine.evaluate("A");
        const MathEngineResult noNeoLeak = engine.evaluate("alpha");
        const bool ok = assignment.ok() && free.ok() && appValue.ok() &&
            appValue.exactText == "17" && noNeoLeak.ok() &&
            noNeoLeak.exactText == "alpha";
        return CaseOutcome{ok, ok ? MathEngineStatus::Ok
                                  : MathEngineStatus::EvaluationError,
                           neoKind(free.value), 0};
    }, 1));
    record(runCase("cross_app", "mixed_cycles_100", [&]() {
        setAngleMode(vpam::AngleMode::RAD);
        CompiledExpression graph =
            engine.compileNumeric("sin(x)", "x", true);
        int graphCompiles = 1;
        bool ok = graph.valid();
        size_t priorInternal = heapSnapshot().internalFree;
        size_t priorPsram = heapSnapshot().psramFree;
        int internalDrops = 0;
        int psramDrops = 0;
        for (int cycle = 0; cycle < 100 && ok; ++cycle) {
            setAngleMode((cycle & 1) ? vpam::AngleMode::DEG
                                     : vpam::AngleMode::RAD);
            switch (cycle % 5) {
                case 0: {
                    const MathEngineResult r = engine.evaluate("1/2+1/3");
                    ok = r.ok() && r.exactText == "5/6";
                    break;
                }
                case 1: {
                    double value = NAN;
                    ok = engine.evaluateNumeric(
                        graph, cycle / 10.0, value) && std::isfinite(value);
                    break;
                }
                case 2: {
                    const StructuredSolveResult r = engine.solveStructured(
                        {"2*x+4", "0"}, "x");
                    ok = r.ok() && r.groups.size() == 1;
                    break;
                }
                case 3: {
                    const StructuredCalculusResult r =
                        engine.evaluateCalculusStructured(
                            {CalculusOperation::Differentiate, "x^2", "x"});
                    ok = r.ok();
                    break;
                }
                case 4: {
                    prepareNeo();
                    neo.setEngine(NeoMathEngine::Giac);
                    const NeoValue x = neo.symbol("x", nativeArena);
                    ok = neo.simplify(x, nativeArena).ok();
                    break;
                }
            }
            if ((cycle + 1) % 25 == 0) {
                engine.reset();
                double stale = NAN;
                ok = ok && !graph.valid() &&
                    !engine.evaluateNumeric(graph, 0.0, stale);
                graph = engine.compileNumeric("sin(x)", "x", true);
                ++graphCompiles;
                ok = ok && graph.valid();
            }
            if ((cycle + 1) % 10 == 0 && cycle >= 19) {
                const HeapSnapshot current = heapSnapshot();
                internalDrops =
                    current.internalFree + 1024 < priorInternal
                    ? internalDrops + 1 : 0;
                psramDrops = current.psramFree + 1024 < priorPsram
                    ? psramDrops + 1 : 0;
                ok = ok && internalDrops < 3 && psramDrops < 3;
                priorInternal = current.internalFree;
                priorPsram = current.psramFree;
            }
            delay(0);
        }
        setAngleMode(vpam::AngleMode::RAD);
        return CaseOutcome{ok, ok ? MathEngineStatus::Ok
                                  : MathEngineStatus::EvaluationError,
                           "structured", graphCompiles};
    }, 0));

    char summary[256];
    const HeapSnapshot finalHeap = heapSnapshot();
    std::snprintf(
        summary, sizeof(summary),
        "[MATHDIAG]|event=summary|pass=%d|fail=%d|generation=%u|"
        "int_free=%u|psram_free=%u|int_largest=%u|psram_largest=%u|"
        "stack_hwm=%u",
        passCount, failCount, static_cast<unsigned>(engine.generation()),
        static_cast<unsigned>(finalHeap.internalFree),
        static_cast<unsigned>(finalHeap.psramFree),
        static_cast<unsigned>(finalHeap.internalLargest),
        static_cast<unsigned>(finalHeap.psramLargest),
        finalHeap.stackHighWater);
    NUMOS_SERIAL.println(summary);
    NUMOS_SERIAL.println("[MATHDIAG]|event=end");
}

} // namespace numos

#endif // NUMOS_MATH_ENGINE_DIAGNOSTICS && ARDUINO

