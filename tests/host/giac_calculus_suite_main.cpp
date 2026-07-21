// GIAC-E01 focused host harness.
//
// Kept as a second executable because the vendored embedded Giac build has a
// reproducible host-only layout-sensitive crash when this coverage is linked
// into the long-lived GIAC-A01 monolithic harness. Running both executables
// from build-giac-host-harness.sh retains the 135-case regression lane while
// exercising the same production GiacEngine TU and embedded macro profile.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "math/AngleModeRuntime.h"
#include "math/giac/GiacEngine.h"

// Production defines this setting in main.cpp/NativeHal.cpp. SingleSolver is
// part of the shared host link closure even though this suite tests calculus.
bool setting_complex_enabled = true;

size_t hostCurrentRssKb();

using Clock = std::chrono::steady_clock;

static int g_pass = 0;
static int g_fail = 0;

static long long usSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               Clock::now() - start).count();
}

static void check(bool condition, const char* label,
                  const std::string& detail = {}) {
    (condition ? g_pass : g_fail)++;
    std::printf("CALC_CHECK|%s|%s%s%s\n", label,
                condition ? "PASS" : "FAIL",
                detail.empty() ? "" : "|", detail.c_str());
}

int main() {
    using numos::CalculusOperation;
    using numos::CalculusRequest;
    using numos::EngineNodeKind;
    using numos::GiacEngine;
    using numos::MathEngineStatus;
    using numos::StructuredCalculusResult;

    auto& engine = GiacEngine::instance();
    check(engine.begin(), "begin");
    std::printf("CALC_RSS_START|rss_kb=%zu\n", hostCurrentRssKb());

    auto calculus = [&](CalculusOperation operation,
                        const std::string& expression,
                        const char* variable = "x") {
        CalculusRequest request;
        request.operation = operation;
        request.expression = expression;
        request.variable = variable;
        return engine.evaluateCalculusStructured(request);
    };
    auto equivalent = [&](const StructuredCalculusResult& result,
                          const char* expected) {
        if (!result.ok() || result.exactText.empty()) return false;
        const std::string difference =
            "((" + result.exactText + ")-(" + expected + "))";
        const auto normalized = engine.simplify(difference.c_str());
        return normalized.ok() && normalized.exactText == "0";
    };

    engine.reset();
    numos::setAngleMode(vpam::AngleMode::RAD);
    const auto coldStart = Clock::now();
    auto derivative = calculus(CalculusOperation::Differentiate, "x^2");
    std::printf("CALCULUS_COLD|differentiate|x^2|us=%lld\n",
                usSince(coldStart));
    check(derivative.ok() && derivative.hasTree &&
              derivative.tree.kind != EngineNodeKind::Unsupported &&
              equivalent(derivative, "2*x"),
          "differentiate-x2-structured", derivative.exactText);

    auto sinSquare = calculus(
        CalculusOperation::Differentiate, "sin(x)^2");
    check(sinSquare.ok() && sinSquare.hasTree &&
              equivalent(sinSquare, "2*sin(x)*cos(x)"),
          "differentiate-sin-square", sinSquare.exactText);
    auto logarithm = calculus(CalculusOperation::Differentiate, "ln(x)");
    check(logarithm.ok() && equivalent(logarithm, "1/x"),
          "differentiate-ln", logarithm.exactText);
    auto parameter = calculus(
        CalculusOperation::Differentiate, "y*x^2");
    check(parameter.ok() && equivalent(parameter, "2*y*x"),
          "differentiate-symbolic-parameter", parameter.exactText);
    auto constant = calculus(CalculusOperation::Differentiate, "7");
    check(constant.ok() && constant.exactText == "0",
          "differentiate-constant", constant.exactText);
    auto nonSmooth = calculus(
        CalculusOperation::Differentiate, "abs(x)");
    check(nonSmooth.ok() && !nonSmooth.exactText.empty(),
          "differentiate-nonsmooth-honest", nonSmooth.exactText);

    auto polyIntegral = calculus(
        CalculusOperation::IntegrateIndefinite, "x^2");
    check(polyIntegral.ok() && polyIntegral.hasTree &&
              equivalent(polyIntegral, "x^3/3") &&
              !polyIntegral.approximateText.empty(),
          "integrate-x2-exact-and-approx",
          polyIntegral.exactText + "|" + polyIntegral.approximateText);
    auto sinIntegral = calculus(
        CalculusOperation::IntegrateIndefinite, "sin(x)");
    check(sinIntegral.ok() && equivalent(sinIntegral, "-cos(x)"),
          "integrate-sin", sinIntegral.exactText);
    auto inverseIntegral = calculus(
        CalculusOperation::IntegrateIndefinite, "1/x");
    check(inverseIntegral.ok() &&
              inverseIntegral.exactText == "ln(abs(x))",
          "integrate-inverse", inverseIntegral.exactText);
    auto exponentialIntegral = calculus(
        CalculusOperation::IntegrateIndefinite, "exp(x)");
    check(exponentialIntegral.ok() &&
              equivalent(exponentialIntegral, "exp(x)"),
          "integrate-exp", exponentialIntegral.exactText);

    auto unevaluated = calculus(
        CalculusOperation::IntegrateIndefinite, "f(x)");
    check(unevaluated.ok() && unevaluated.unevaluated &&
              !unevaluated.exactText.empty(),
          "valid-unevaluated-is-not-failure",
          unevaluated.exactText + "|" + unevaluated.diagnostic);
    auto infinity = calculus(
        CalculusOperation::Differentiate, "x*infinity");
    check(infinity.ok() &&
              infinity.exactText.find("infinity") != std::string::npos,
          "infinity-preserved", infinity.exactText);
    auto singular = calculus(
        CalculusOperation::IntegrateIndefinite, "1/(x-x)");
    check(!singular.ok() || singular.unevaluated ||
              !singular.exactText.empty(),
          "singular-integral-honest",
          singular.exactText + "|" + singular.diagnostic);
    auto undefined = calculus(
        CalculusOperation::IntegrateIndefinite, "0/0");
    check(undefined.status == MathEngineStatus::Undefined,
          "undefined-output-no-fallback", undefined.diagnostic);

    auto malformed = calculus(
        CalculusOperation::Differentiate, "2+*3");
    check(malformed.status == MathEngineStatus::ParseError,
          "malformed-before-operation", malformed.diagnostic);
    auto reserved = calculus(
        CalculusOperation::Differentiate, "x^2", "sin");
    check(reserved.status == MathEngineStatus::ParseError,
          "reserved-variable-rejected", reserved.diagnostic);
    auto invalidVariable = calculus(
        CalculusOperation::Differentiate, "x^2", "1x");
    check(invalidVariable.status == MathEngineStatus::ParseError,
          "invalid-variable-rejected", invalidVariable.diagnostic);
    const std::string tooLong(2001, 'x');
    auto lengthBound = calculus(
        CalculusOperation::Differentiate, tooLong);
    check(lengthBound.status == MathEngineStatus::Unsupported,
          "serialized-length-bound", lengthBound.diagnostic);
    std::string tooManyNodes = "x";
    for (int i = 0; i < 450; ++i) tooManyNodes += "+x";
    auto nodeBound = calculus(
        CalculusOperation::Differentiate, tooManyNodes);
    check(nodeBound.status == MathEngineStatus::Unsupported,
          "input-node-bound", nodeBound.diagnostic);
    std::string tooDeep = "x";
    for (int i = 0; i < 45; ++i)
        tooDeep = "sin(" + tooDeep + ")";
    auto depthBound = calculus(
        CalculusOperation::Differentiate, tooDeep);
    check(depthBound.status == MathEngineStatus::Unsupported,
          "input-depth-bound", depthBound.diagnostic);

    CalculusRequest derivativeRequest;
    derivativeRequest.operation = CalculusOperation::Differentiate;
    derivativeRequest.expression = "x^2";
    derivativeRequest.variable = "x";
    auto derivativeAgree =
        engine.verifyCalculusTutor(derivativeRequest, "2*x");
    auto derivativeDisagree =
        engine.verifyCalculusTutor(derivativeRequest, "3*x");
    check(derivativeAgree.agreed && !derivativeDisagree.agreed,
          "tutor-derivative-fail-closed",
          derivativeAgree.diagnostic + "|" + derivativeDisagree.diagnostic);

    CalculusRequest integralRequest;
    integralRequest.operation =
        CalculusOperation::IntegrateIndefinite;
    integralRequest.expression = "x^2";
    integralRequest.variable = "x";
    auto integralConstantAgree =
        engine.verifyCalculusTutor(integralRequest, "x^3/3+17");
    auto integralDisagree =
        engine.verifyCalculusTutor(integralRequest, "x^3/2");
    check(integralConstantAgree.agreed && !integralDisagree.agreed,
          "tutor-integral-additive-constant",
          integralConstantAgree.diagnostic + "|" +
              integralDisagree.diagnostic);

    numos::setAngleMode(vpam::AngleMode::RAD);
    auto radDerivative = calculus(
        CalculusOperation::Differentiate, "sin(x)");
    numos::setAngleMode(vpam::AngleMode::DEG);
    auto degDerivative = calculus(
        CalculusOperation::Differentiate, "sin(x)");
    check(radDerivative.ok() && degDerivative.ok() &&
              radDerivative.exactText != degDerivative.exactText,
          "angle-mode-synchronized",
          radDerivative.exactText + " -> " + degDerivative.exactText);
    numos::setAngleMode(vpam::AngleMode::RAD);

    calculus(CalculusOperation::Differentiate, "sin(x)^2");
    const size_t rssBefore = hostCurrentRssKb();
    const auto warmStart = Clock::now();
    int warmOk = 0;
    static constexpr int kWarmCalls = 500;
    for (int i = 0; i < kWarmCalls; ++i) {
        auto result = calculus(
            (i & 1) ? CalculusOperation::Differentiate
                    : CalculusOperation::IntegrateIndefinite,
            (i & 1) ? "sin(x)^2" : "x^2");
        if (result.ok()) ++warmOk;
        if ((i + 1) % 100 == 0) engine.reset();
    }
    const long long warmUs = usSince(warmStart);
    const long long rssDelta =
        static_cast<long long>(hostCurrentRssKb()) -
        static_cast<long long>(rssBefore);
    std::printf(
        "CALCULUS_WARM|n=%d|ok=%d|avg_us=%.1f|rss_delta_kb=%lld\n",
        kWarmCalls, warmOk, static_cast<double>(warmUs) / kWarmCalls,
        rssDelta);
    check(warmOk == kWarmCalls, "repeated-reset-all-ok");
    check(rssDelta < 1024, "repeated-rss-stable");

    std::printf("CALC_RSS_END|rss_kb=%zu\n", hostCurrentRssKb());
    std::printf("CALC_FOOTER|pass=%d|fail=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
