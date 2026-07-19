// giac_engine_suite_main.cpp — GIAC-A01 host harness for the PRODUCTION
// numos::GiacEngine seam (src/math/giac/GiacEngine.h). Successor of the
// GIAC-FEAS-01 spike harness; the experimental adapter is gone and this
// drives the same TU the firmware and emulator link.
//
// Build + run: scripts/build-giac-host-harness.sh
//
// Output is line-oriented:
//   EVAL|<expr>|status=Ok|us=123|<exact>|approx=<...>
//   CHECK|<label>|PASS
//   SWEEP|<expr>|n=1000|total_ms=..|per_point_us=..|nonfinite=..|rejected=..
//   FOOTER|pass=..|fail=..

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <chrono>

#include "math/giac/GiacEngine.h"
#include "math/AngleModeRuntime.h"
#include "math/CalculationEngine.h"
#include "math/MathAST.h"
#include "math/VariableManager.h"

// Defined in host_rss_probe.cpp — its own TU because <windows.h> cannot
// coexist with NumOS headers (INPUT/TokenType collisions).
size_t hostCurrentRssKb();
static size_t currentRssKb() { return hostCurrentRssKb(); }

using Clock = std::chrono::steady_clock;
static long long usSince(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               Clock::now() - t0).count();
}

static int g_pass = 0, g_fail = 0;

static const char* statusName(numos::MathEngineStatus s) {
    switch (s) {
        case numos::MathEngineStatus::Ok:              return "Ok";
        case numos::MathEngineStatus::Undefined:       return "Undefined";
        case numos::MathEngineStatus::ParseError:      return "ParseError";
        case numos::MathEngineStatus::EvaluationError: return "EvaluationError";
        case numos::MathEngineStatus::Unsupported:     return "Unsupported";
        case numos::MathEngineStatus::OutOfMemory:     return "OutOfMemory";
    }
    return "?";
}

static void check(bool cond, const char* label, const std::string& detail = "") {
    (cond ? g_pass : g_fail)++;
    printf("CHECK|%s|%s%s%s\n", label, cond ? "PASS" : "FAIL",
           detail.empty() ? "" : "|", detail.c_str());
}

// Run one expression through the engine in evaluate or simplify mode and
// assert the status (and optionally an exact-text substring).
static void runCase(const char* expr, bool simplifyMode,
                    numos::MathEngineStatus want,
                    const char* expectContains = nullptr) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    auto t0 = Clock::now();
    numos::MathEngineResult r =
        simplifyMode ? eng.simplify(expr) : eng.evaluate(expr);
    long long us = usSince(t0);
    bool pass = (r.status == want);
    if (pass && expectContains &&
        r.exactText.find(expectContains) == std::string::npos)
        pass = false;
    (pass ? g_pass : g_fail)++;
    printf("EVAL|%s%s|status=%s|us=%lld|%s|approx=%s%s\n",
           simplifyMode ? "simplify:" : "", expr, statusName(r.status), us,
           r.ok() ? r.exactText.c_str() : r.diagnostic.c_str(),
           r.approximateText.c_str(), pass ? "" : "|UNEXPECTED");
}

static void sweep(const char* expr, double x0, double x1, int n) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    numos::CompiledExpression ce = eng.compileNumeric(expr);
    if (!ce.valid()) {
        printf("SWEEP|%s|COMPILE_FAIL|%s\n", expr, ce.diagnostic().c_str());
        g_fail++;
        return;
    }
    int nonfinite = 0, rejected = 0;
    double sum = 0;
    auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
        double x = x0 + (x1 - x0) * i / (n - 1);
        double y;
        if (!eng.evaluateNumeric(ce, x, y)) { ++rejected; continue; }
        if (std::isnan(y) || std::isinf(y)) ++nonfinite;
        else sum += y;
    }
    long long us = usSince(t0);
    printf("SWEEP|%s|n=%d|total_ms=%.1f|per_point_us=%.2f|nonfinite=%d|rejected=%d|checksum=%.6g\n",
           expr, n, us / 1000.0, (double)us / n, nonfinite, rejected, sum);
    g_pass++;
}

// GIAC-C01: Grapher-shaped strict 1-D sweep (raw parse retained; per-sample
// substitution only) with COMPILE timing, and the 2-D residual grid sweep
// used by implicit contours / inequality regions.
static void sweepStrict(const char* expr, const char* var, double x0, double x1,
                        int n) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    auto tc = Clock::now();
    numos::CompiledExpression ce = eng.compileNumeric(expr, var, true);
    long long compileUs = usSince(tc);
    if (!ce.valid()) {
        printf("SWEEPS|%s|COMPILE_FAIL|%s\n", expr, ce.diagnostic().c_str());
        g_fail++;
        return;
    }
    int rejected = 0;
    double sum = 0;
    auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
        double x = x0 + (x1 - x0) * i / (n - 1);
        double y;
        if (!eng.evaluateNumeric(ce, x, y) || std::isnan(y) || std::isinf(y)) {
            ++rejected;
            continue;
        }
        sum += y;
    }
    long long us = usSince(t0);
    printf("SWEEPS|%s|compile_us=%lld|n=%d|total_ms=%.1f|per_point_us=%.2f|rejected=%d|checksum=%.6g\n",
           expr, compileUs, n, us / 1000.0, (double)us / n, rejected, sum);
    g_pass++;
}

static void sweep2D(const char* expr, double lo, double hi, int side) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    auto tc = Clock::now();
    numos::CompiledExpression ce = eng.compileNumeric2D(expr, "x", "y");
    long long compileUs = usSince(tc);
    if (!ce.valid()) {
        printf("SWEEP2D|%s|COMPILE_FAIL|%s\n", expr, ce.diagnostic().c_str());
        g_fail++;
        return;
    }
    const int n = side * side;
    int rejected = 0;
    double sum = 0;
    auto t0 = Clock::now();
    for (int j = 0; j < side; ++j) {
        double y = lo + (hi - lo) * j / (side - 1);
        for (int i = 0; i < side; ++i) {
            double x = lo + (hi - lo) * i / (side - 1);
            double g;
            if (!eng.evaluateNumeric2D(ce, x, y, g) || std::isnan(g) ||
                std::isinf(g)) {
                ++rejected;
                continue;
            }
            sum += g;
        }
    }
    long long us = usSince(t0);
    printf("SWEEP2D|%s|compile_us=%lld|n=%d|total_ms=%.1f|per_point_us=%.2f|rejected=%d|checksum=%.6g\n",
           expr, compileUs, n, us / 1000.0, (double)us / n, rejected, sum);
    g_pass++;
}

int main() {
    using numos::GiacEngine;
    using numos::MathEngineStatus;
    GiacEngine& eng = GiacEngine::instance();

    printf("RSS_START|rss_kb=%zu\n", currentRssKb());

    // --- lifecycle: initialization exactly once, begin() idempotent ---
    auto t0 = Clock::now();
    bool first = eng.begin();
    long long initUs = usSince(t0);
    bool second = eng.begin();
    printf("INIT|first=%d|second=%d|us=%lld\n", first ? 1 : 0, second ? 1 : 0,
           initUs);
    check(first && second, "begin-idempotent");

    t0 = Clock::now();
    numos::MathEngineResult firstEval = eng.evaluate("2+2");
    printf("FIRST_EVAL|status=%s|us=%lld|%s\n", statusName(firstEval.status),
           usSince(t0), firstEval.exactText.c_str());
    check(firstEval.ok() && firstEval.exactText == "4", "first-eval");

    // --- the 24-expression representative suite (GIAC-FEAS-01 lineage) ---
    // Arithmetic
    runCase("2+2", false, MathEngineStatus::Ok, "4");
    runCase("1/2+1/3", false, MathEngineStatus::Ok, "5/6");
    runCase("2^100", false, MathEngineStatus::Ok,
            "1267650600228229401496703205376");
    // Symbolic. Documented semantics: plain evaluate does NOT collect like
    // terms; explicit simplify mode does.
    runCase("x-2*x", false, MathEngineStatus::Ok, "x-2*x");
    runCase("x-2*x", true, MathEngineStatus::Ok, "-x");
    runCase("regroup(x-2*x)", false, MathEngineStatus::Ok, "-x");
    runCase("simplify((x^2-1)/(x-1))", false, MathEngineStatus::Ok, "x+1");
    runCase("(x^2-1)/(x-1)", true, MathEngineStatus::Ok, "x+1");
    runCase("factor(x^3-6*x^2+11*x-6)", false, MathEngineStatus::Ok,
            "(x-1)*(x-2)*(x-3)");
    // Equations
    runCase("solve(x^2-2=0,x)", false, MathEngineStatus::Ok, "sqrt(2)");
    runCase("solve(ln(x)=1,x)", false, MathEngineStatus::Ok, "exp(1)");
    // Calculus
    runCase("diff(sin(x)^2,x)", false, MathEngineStatus::Ok, "2*cos(x)*sin(x)");
    runCase("integrate(x^2,x)", false, MathEngineStatus::Ok, "x^3");
    // Constants / exactness
    runCase("pi", false, MathEngineStatus::Ok, "pi");
    runCase("sqrt(2)", false, MathEngineStatus::Ok, "sqrt(2)");
    runCase("sin(pi/6)", false, MathEngineStatus::Ok, "1/2");
    // Grapher-oriented
    runCase("simplify(1/x)", false, MathEngineStatus::Ok, "1/x");
    runCase("diff(sin(x),x)", false, MathEngineStatus::Ok, "cos(x)");
    // Failure semantics, reported honestly
    runCase("2+*3", false, MathEngineStatus::ParseError);
    runCase("1/0", false, MathEngineStatus::Ok, "oo");          // Giac infinity
    runCase("0/0", false, MathEngineStatus::Undefined);
    runCase("thisfunctiondoesnotexist(4)", false, MathEngineStatus::Ok,
            "thisfunctiondoesnotexist(4)");                     // symbolic echo
    runCase("idivis(x)", false, MathEngineStatus::EvaluationError);
    runCase("", false, MathEngineStatus::ParseError);

    // approximate companion text
    {
        numos::MathEngineResult r = eng.evaluate("pi");
        check(r.ok() && r.approximateText.rfind("3.14", 0) == 0,
              "pi-approximate-text", r.approximateText);
    }

    // --- variable assignment, persistence, clearing ---
    check(eng.evaluate("giacvar:=5").ok(), "var-assign");
    {
        numos::MathEngineResult r = eng.evaluate("giacvar");
        check(r.ok() && r.exactText == "5", "var-persists", r.exactText);
    }
    check(eng.evaluate("purge(giacvar)").ok(), "var-purge");
    {
        numos::MathEngineResult r = eng.evaluate("giacvar");
        check(r.ok() && r.exactText == "giacvar", "var-cleared", r.exactText);
    }

    // --- reset() forgets engine-side state ---
    check(eng.evaluate("resetvar:=7").ok(), "reset-var-assign");
    eng.reset();
    {
        numos::MathEngineResult r = eng.evaluate("resetvar");
        check(r.ok() && r.exactText == "resetvar", "reset-clears-vars",
              r.exactText);
    }

    // --- DEG/RAD synchronization with AngleModeRuntime ---
    numos::setAngleMode(vpam::AngleMode::DEG);
    {
        numos::MathEngineResult r = eng.evaluate("sin(30)");
        check(r.ok() && r.exactText == "1/2", "deg-sin30", r.exactText);
    }
    numos::setAngleMode(vpam::AngleMode::RAD);
    {
        numos::MathEngineResult r = eng.evaluate("sin(30)");
        check(r.ok() && r.exactText != "1/2", "rad-sin30-differs", r.exactText);
        numos::MathEngineResult r2 = eng.evaluate("sin(pi/6)");
        check(r2.ok() && r2.exactText == "1/2", "rad-sin-pi6", r2.exactText);
    }

    // --- repeated warm evaluation + memory stability (>= 1000 evals) ---
    {
        eng.evaluate("diff(sin(x)^2,x)");  // warm
        size_t rssBefore = currentRssKb();
        auto tw = Clock::now();
        const int N = 1000;
        int okCount = 0;
        for (int i = 0; i < N; ++i)
            if (eng.evaluate("diff(sin(x)^2,x)").ok()) ++okCount;
        long long us = usSince(tw);
        size_t rssAfter = currentRssKb();
        long long deltaKb = (long long)rssAfter - (long long)rssBefore;
        printf("WARM|diff(sin(x)^2,x)|n=%d|ok=%d|avg_us=%.1f|rss_delta_kb=%lld\n",
               N, okCount, (double)us / N, deltaKb);
        check(okCount == N, "warm-1000-all-ok");
        check(deltaKb < 1024, "warm-1000-rss-stable");
    }

    // --- compiled expressions: two retained simultaneously, interleaved ---
    {
        numos::CompiledExpression cSin = eng.compileNumeric("sin(x)");
        numos::CompiledExpression cPoly = eng.compileNumeric("x^2-2");
        check(cSin.valid() && cPoly.valid(), "compile-two-simultaneous");
        double a = 0, b = 0, c = 0, d = 0;
        bool ok = eng.evaluateNumeric(cSin, 0.0, a) &&
                  eng.evaluateNumeric(cPoly, 2.0, b) &&
                  eng.evaluateNumeric(cSin, M_PI / 2.0, c) &&
                  eng.evaluateNumeric(cPoly, -3.0, d);
        check(ok && std::fabs(a) < 1e-12 && std::fabs(b - 2.0) < 1e-12 &&
                  std::fabs(c - 1.0) < 1e-12 && std::fabs(d - 7.0) < 1e-12,
              "compile-interleaved-values");

        // pole behavior: 1/x at exactly 0 -> unsigned infinity -> rejected
        numos::CompiledExpression cInv = eng.compileNumeric("1/x");
        double pole = 0;
        check(cInv.valid() && !eng.evaluateNumeric(cInv, 0.0, pole),
              "pole-rejected-safely");
        // non-numeric residue: unresolved second symbol
        numos::CompiledExpression cXY = eng.compileNumeric("x+unboundsym");
        double resid = 0;
        check(cXY.valid() && !eng.evaluateNumeric(cXY, 1.0, resid),
              "symbolic-residue-rejected");

        // invalid/empty handles
        numos::CompiledExpression empty;
        double dummy = 0;
        check(!empty.valid() && !eng.evaluateNumeric(empty, 1.0, dummy),
              "empty-handle-rejected");
        numos::CompiledExpression bad = eng.compileNumeric("2+*3");
        check(!bad.valid() && !eng.evaluateNumeric(bad, 1.0, dummy),
              "parse-fail-handle-rejected", bad.diagnostic());

        // engine reset orphans previously compiled handles — no stale context
        check(cSin.valid(), "handle-valid-pre-reset");
        eng.reset();
        check(!cSin.valid() && !eng.evaluateNumeric(cSin, 1.0, dummy),
              "handle-invalid-post-reset");
        numos::CompiledExpression cSin2 = eng.compileNumeric("sin(x)");
        double post = 0;
        check(cSin2.valid() && eng.evaluateNumeric(cSin2, 0.0, post) &&
                  std::fabs(post) < 1e-12,
              "recompile-after-reset");
    }

    // --- Grapher sweeps: 1,000 and 10,000 points, no per-point parsing ---
    sweep("sin(x)", -5.0, 5.0, 1000);
    sweep("1/x", -5.0, 5.0, 1000);
    sweep("x^2-2", -5.0, 5.0, 1000);
    sweep("sin(x)", -5.0, 5.0, 10000);
    sweep("1/x", -5.0, 5.0, 10000);
    sweep("x^2-2", -5.0, 5.0, 10000);

    // ═══════════════════════════════════════════════════════════════════
    // GIAC-B01: CalculationEngine adapter (VPAM serializer + result bridge)
    // ═══════════════════════════════════════════════════════════════════
    {
        using namespace vpam;
        numos::CalculationEngine& calc = numos::CalculationEngine::instance();
        numos::setAngleMode(vpam::AngleMode::RAD);

        auto rowPtr = []() { return makeRow(); };
        auto asRow = [](NodePtr& p) { return static_cast<NodeRow*>(p.get()); };
        auto numRow = [&](const char* v) {
            auto r = makeRow();
            static_cast<NodeRow*>(r.get())->appendChild(makeNumber(v));
            return r;
        };

        // -- serializer basics --------------------------------------------
        {
            auto r = rowPtr();
            asRow(r)->appendChild(makeNumber("2"));
            asRow(r)->appendChild(makeOperator(OpKind::Add));
            asRow(r)->appendChild(makeNumber("2"));
            std::string out, err;
            check(numos::CalculationEngine::serializeForGiac(r.get(), out, err) &&
                      out == "2+2",
                  "b01-ser-2plus2", out);
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.isInteger() &&
                      ev.exactVal.num == 4,
                  "b01-eval-2plus2", ev.exactText);
        }
        {   // 1/2 + 1/3 == exact 5/6, tier 1
            auto r = rowPtr();
            asRow(r)->appendChild(makeFraction(numRow("1"), numRow("2")));
            asRow(r)->appendChild(makeOperator(OpKind::Add));
            asRow(r)->appendChild(makeFraction(numRow("1"), numRow("3")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.isRational() &&
                      ev.exactVal.num == 5 && ev.exactVal.den == 6,
                  "b01-eval-frac-5-6", ev.exactText);
        }
        {   // unary minus normalization: [-,3^2] -> -9 ; [2,*,-,3] -> -6
            auto r = rowPtr();
            asRow(r)->appendChild(makeOperator(OpKind::Sub));
            asRow(r)->appendChild(makePower(numRow("3"), numRow("2")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.num == -9,
                  "b01-eval-unary-minus-pow", ev.exactText);
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeNumber("2"));
            asRow(r2)->appendChild(makeOperator(OpKind::Mul));
            asRow(r2)->appendChild(makeOperator(OpKind::Sub));
            asRow(r2)->appendChild(makeNumber("3"));
            auto ev2 = calc.evaluate(r2.get());
            check(ev2.ok() && ev2.exactValValid && ev2.exactVal.num == -6,
                  "b01-eval-mul-unary-minus", ev2.exactText);
        }
        {   // General roots: Giac keeps surd(8,3) EXACT as 8^(1/3) (no auto
            // radical collapse under plain evaluate) — presented through the
            // structured tier as a degree-3 NodeRoot.
            auto r = rowPtr();
            asRow(r)->appendChild(makeRoot(numRow("8"), numRow("3")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && !ev.exactValValid && ev.exactAST != nullptr &&
                      ev.exactText == "8^(1/3)",
                  "b01-eval-cuberoot-8-exact", ev.exactText);
        }
        {   // logb argument order: log_2(8) == 3
            auto r = rowPtr();
            asRow(r)->appendChild(makeLogBase(numRow("2"), numRow("8")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.num == 3,
                  "b01-eval-logbase-2-8", ev.exactText);
        }
        {   // NumOS log == base 10 (Giac log is ln!)
            auto r = rowPtr();
            asRow(r)->appendChild(makeFunction(FuncKind::Log, numRow("100")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.num == 2,
                  "b01-eval-log10-100", ev.exactText);
        }
        {   // pi and e stay exact (tier 1 pi/e multipliers)
            auto r = rowPtr();
            asRow(r)->appendChild(makeConstant(ConstKind::Pi));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.piMul == 1,
                  "b01-eval-pi-exact", ev.exactText);
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeConstant(ConstKind::E));
            auto ev2 = calc.evaluate(r2.get());
            check(ev2.ok() && ev2.exactValValid && ev2.exactVal.eMul == 1,
                  "b01-eval-e-exact", ev2.exactText);
        }
        {   // sqrt(2) exact radical; sin(pi/6) == 1/2
            auto r = rowPtr();
            asRow(r)->appendChild(makeRoot(numRow("2")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.inner == 2 &&
                      ev.exactVal.num == 1 && ev.exactVal.den == 1,
                  "b01-eval-sqrt2-exact", ev.exactText);
            auto arg = rowPtr();
            asRow(arg)->appendChild(makeConstant(ConstKind::Pi));
            asRow(arg)->appendChild(makeOperator(OpKind::Mul));
            asRow(arg)->appendChild(makeFraction(numRow("1"), numRow("6")));
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeFunction(FuncKind::Sin, std::move(arg)));
            auto ev2 = calc.evaluate(r2.get());
            check(ev2.ok() && ev2.exactValValid && ev2.exactVal.num == 1 &&
                      ev2.exactVal.den == 2,
                  "b01-eval-sin-pi6", ev2.exactText);
        }
        {   // 2^100: beyond int64 -> structured AST tier (never lossy)
            auto r = rowPtr();
            asRow(r)->appendChild(makePower(numRow("2"), numRow("100")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && !ev.exactValValid && ev.exactAST != nullptr &&
                      ev.exactText == "1267650600228229401496703205376",
                  "b01-eval-2pow100-structured", ev.exactText);
        }
        {   // free symbols: x-2*x stays x-2*x under plain evaluate
            auto r = rowPtr();
            asRow(r)->appendChild(makeVariable('x'));
            asRow(r)->appendChild(makeOperator(OpKind::Sub));
            asRow(r)->appendChild(makeNumber("2"));
            asRow(r)->appendChild(makeOperator(OpKind::Mul));
            asRow(r)->appendChild(makeVariable('x'));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && !ev.exactValValid && ev.exactAST != nullptr &&
                      ev.exactText == "x-2*x",
                  "b01-eval-free-symbol", ev.exactText);
        }
        {   // 1/0 -> Giac infinity, honest text fallback tier
            auto r = rowPtr();
            asRow(r)->appendChild(makeFraction(numRow("1"), numRow("0")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() &&
                      ev.kind == numos::CalcResultKind::TextFallback &&
                      ev.exactText == "oo",
                  "b01-eval-one-over-zero-oo", ev.exactText);
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeFraction(numRow("0"), numRow("0")));
            auto ev2 = calc.evaluate(r2.get());
            check(!ev2.ok() &&
                      ev2.status == numos::MathEngineStatus::Undefined,
                  "b01-eval-zero-over-zero-undef", ev2.diagnostic);
        }
        {   // incomplete input: typed reject, never ambiguous text
            auto r = rowPtr();
            asRow(r)->appendChild(makeNumber("2"));
            asRow(r)->appendChild(makeOperator(OpKind::Add));
            asRow(r)->appendChild(makeEmpty());
            auto ev = calc.evaluate(r.get());
            check(!ev.ok() &&
                      ev.status == numos::MathEngineStatus::ParseError,
                  "b01-serialize-incomplete-typed", ev.diagnostic);
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeSummation(numRow("1"), numRow("5"),
                                                 numRow("2")));
            auto ev2 = calc.evaluate(r2.get());
            check(!ev2.ok() &&
                      ev2.status == numos::MathEngineStatus::ParseError,
                  "b01-serialize-unsupported-typed", ev2.diagnostic);
        }
        {   // sqrt(2)/2-shaped result mirrors exactly ((1/2)*sqrt(2))
            numos::StructuredEngineResult sr =
                eng.evaluateStructured("sin(pi/4)");
            vpam::ExactVal v;
            check(sr.base.ok() && sr.hasTree &&
                      numos::CalculationEngine::resultTreeToExactVal(sr.tree, v) &&
                      v.inner == 2 && v.num == 1 && v.den == 2,
                  "b01-mirror-sqrt2-over-2", sr.base.exactText);
        }
        {   // solve/factor/diff/integrate keep their Giac meaning through
            // the structured bridge (lists fall back honestly)
            numos::StructuredEngineResult sr =
                eng.evaluateStructured("solve(x^2-2=0,x)");
            check(sr.base.ok() && sr.hasTree &&
                      sr.tree.kind == numos::EngineNodeKind::List &&
                      numos::CalculationEngine::resultTreeToAST(sr.tree) == nullptr,
                  "b01-solve-list-fallback", sr.base.exactText);
            numos::StructuredEngineResult sf =
                eng.evaluateStructured("factor(x^3-6*x^2+11*x-6)");
            check(sf.base.ok() && sf.hasTree &&
                      numos::CalculationEngine::resultTreeToAST(sf.tree) != nullptr,
                  "b01-factor-structured", sf.base.exactText);
            numos::StructuredEngineResult sd =
                eng.evaluateStructured("diff(sin(x)^2,x)");
            check(sd.base.ok() && sd.hasTree &&
                      numos::CalculationEngine::resultTreeToAST(sd.tree) != nullptr,
                  "b01-diff-structured", sd.base.exactText);
            numos::StructuredEngineResult si =
                eng.evaluateStructured("integrate(x^2,x)");
            check(si.base.ok() && si.hasTree &&
                      numos::CalculationEngine::resultTreeToAST(si.tree) != nullptr,
                  "b01-integrate-structured", si.base.exactText);
        }
        {   // decimal input maps through fromDouble like the legacy engine
            auto r = rowPtr();
            asRow(r)->appendChild(makeNumber("0.5"));
            asRow(r)->appendChild(makeOperator(OpKind::Add));
            asRow(r)->appendChild(makeNumber("0.25"));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.isRational() &&
                      ev.exactVal.num == 3 && ev.exactVal.den == 4,
                  "b01-eval-decimal-three-quarters", ev.exactText);
        }
        {   // Ans mirroring: exact chain through numos_Ans
            auto& vm = vpam::VariableManager::instance();
            vm.updateAns(vpam::ExactVal::fromInt(4));
            calc.noteAnsRotated("4");
            auto r = rowPtr();
            asRow(r)->appendChild(makeVariable(vpam::VAR_ANS));
            asRow(r)->appendChild(makeOperator(OpKind::Add));
            asRow(r)->appendChild(makeNumber("1"));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.num == 5,
                  "b01-ans-chain-5", ev.exactText);

            // beyond-int64 Ans: session-exact text keeps the chain exact
            vpam::ExactVal approx;
            approx.ok = true; approx.approximate = true;
            approx.approxVal = 1.2676506002282294e30;
            vm.updateAns(approx);
            calc.noteAnsRotated("1267650600228229401496703205376");
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeVariable(vpam::VAR_ANS));
            asRow(r2)->appendChild(makeOperator(OpKind::Mul));
            asRow(r2)->appendChild(makeNumber("2"));
            auto ev2 = calc.evaluate(r2.get());
            check(ev2.ok() &&
                      ev2.exactText == "2535301200456458802993406410752",
                  "b01-ans-exact-bigint-chain", ev2.exactText);
        }
        {   // STO round-trip incl. potential name collisions (A..F, E!)
            auto& vm = vpam::VariableManager::instance();
            vm.updateAns(vpam::ExactVal::fromFrac(5, 6));
            calc.noteAnsRotated("5/6");
            for (char c : {'A', 'E', 'F'}) {
                vm.setVariable(c, vm.getAns());
                calc.noteVariableStored(c);
                auto r = rowPtr();
                asRow(r)->appendChild(makeVariable(c));
                asRow(r)->appendChild(makeOperator(OpKind::Mul));
                asRow(r)->appendChild(makeNumber("6"));
                auto ev = calc.evaluate(r.get());
                check(ev.ok() && ev.exactValValid && ev.exactVal.num == 5,
                      (std::string("b01-sto-roundtrip-") + c).c_str(),
                      ev.exactText);
            }
        }
        {   // DEG/RAD through the adapter (AngleModeRuntime is the truth)
            numos::setAngleMode(vpam::AngleMode::DEG);
            auto r = rowPtr();
            asRow(r)->appendChild(makeFunction(FuncKind::Sin, numRow("30")));
            auto ev = calc.evaluate(r.get());
            check(ev.ok() && ev.exactValValid && ev.exactVal.num == 1 &&
                      ev.exactVal.den == 2,
                  "b01-deg-sin30", ev.exactText);
            numos::setAngleMode(vpam::AngleMode::RAD);
            auto r2 = rowPtr();
            asRow(r2)->appendChild(makeFunction(FuncKind::Sin, numRow("30")));
            auto ev2 = calc.evaluate(r2.get());
            check(ev2.ok() && !ev2.exactValValid &&
                      ev2.exactText == "sin(30)",
                  "b01-rad-sin30-symbolic", ev2.exactText);
        }
        {   // ExactVal -> Giac text round-trip ((1/2)*sqrt(2)): value-equal
            // to sqrt(2)/2 (print forms may differ; compare the difference).
            vpam::ExactVal v;
            v.num = 1; v.den = 2; v.outer = 1; v.inner = 2; v.ok = true;
            std::string t = numos::CalculationEngine::exactValToGiacText(v);
            numos::MathEngineResult r =
                eng.simplify(("(" + t + ")-(sqrt(2)/2)").c_str());
            check(r.ok() && r.exactText == "0",
                  "b01-exactval-text-roundtrip", t + " diff -> " + r.exactText);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // GIAC-C01: Grapher retained-expression contract (strict 1-D + 2-D)
    // ═══════════════════════════════════════════════════════════════════
    {
        numos::setAngleMode(vpam::AngleMode::RAD);

        // strict compile: unknown identifiers are an HONEST compile failure
        // that names the offender (the Grapher's "unknown: <ident>" surface).
        numos::CompiledExpression bad =
            eng.compileNumeric("x+unboundsym", "x", true);
        check(!bad.valid() &&
                  bad.diagnostic().find("unboundsym") != std::string::npos,
              "c01-strict-unknown-ident-named", bad.diagnostic());

        // strict compile ignores context assignments: a stored value can
        // never capture the sampling variable (x:=7 must NOT bake into f).
        check(eng.evaluate("x:=7").ok(), "c01-ctx-x-assign");
        numos::CompiledExpression fx = eng.compileNumeric("x^2", "x", true);
        double v = 0;
        check(fx.valid() && eng.evaluateNumeric(fx, 3.0, v) &&
                  std::fabs(v - 9.0) < 1e-12,
              "c01-strict-no-context-capture", std::to_string(v));
        check(eng.evaluate("purge(x)").ok(), "c01-ctx-x-purge");

        // strict compile rejects OTHER assigned variables too (a:=5, "a*x"):
        // Grapher slots may only reference their sampling variables.
        check(eng.evaluate("giaca:=5").ok(), "c01-ctx-a-assign");
        numos::CompiledExpression ax = eng.compileNumeric("giaca*x", "x", true);
        check(!ax.valid() &&
                  ax.diagnostic().find("giaca") != std::string::npos,
              "c01-strict-assigned-var-rejected", ax.diagnostic());
        check(eng.evaluate("purge(giaca)").ok(), "c01-ctx-a-purge");

        // DEG/RAD is applied at EVALUATION time on strict handles — no
        // recompile needed after a mode flip (the Grapher relies on this).
        numos::CompiledExpression fsin = eng.compileNumeric("sin(x)", "x", true);
        double dv = 0, rv = 0;
        numos::setAngleMode(vpam::AngleMode::DEG);
        bool okDeg = fsin.valid() && eng.evaluateNumeric(fsin, 30.0, dv);
        numos::setAngleMode(vpam::AngleMode::RAD);
        bool okRad = eng.evaluateNumeric(fsin, 30.0, rv);
        check(okDeg && okRad && std::fabs(dv - 0.5) < 1e-12 &&
                  std::fabs(rv - (-0.98803162409286183)) < 1e-9,
              "c01-strict-angle-mode-eval-time",
              std::to_string(dv) + " / " + std::to_string(rv));

        // 2-D residual handle: circle x^2+y^2-1.
        numos::CompiledExpression circ =
            eng.compileNumeric2D("(x^2+y^2)-(1)", "x", "y");
        double g0 = 0, g1 = 0, g2 = 0;
        check(circ.valid() &&
                  eng.evaluateNumeric2D(circ, 1.0, 0.0, g0) &&
                  eng.evaluateNumeric2D(circ, 0.0, 0.0, g1) &&
                  eng.evaluateNumeric2D(circ, 2.0, 2.0, g2) &&
                  std::fabs(g0) < 1e-12 && std::fabs(g1 + 1.0) < 1e-12 &&
                  std::fabs(g2 - 7.0) < 1e-12,
              "c01-2d-circle-residual");

        // 2-D strictness + arity guards.
        numos::CompiledExpression zbad =
            eng.compileNumeric2D("x*y+zvar", "x", "y");
        check(!zbad.valid() &&
                  zbad.diagnostic().find("zvar") != std::string::npos,
              "c01-2d-unknown-ident-named", zbad.diagnostic());
        double dummy = 0;
        check(!eng.evaluateNumeric(circ, 1.0, dummy),
              "c01-2d-handle-rejects-1d-eval");
        check(fx.valid() && !eng.evaluateNumeric2D(fx, 1.0, 1.0, dummy),
              "c01-1d-handle-rejects-2d-eval");

        // reset() orphans 2-D handles exactly like 1-D ones.
        eng.reset();
        check(!circ.valid() && !eng.evaluateNumeric2D(circ, 1.0, 0.0, dummy),
              "c01-2d-handle-invalid-post-reset");
        numos::CompiledExpression circ2 =
            eng.compileNumeric2D("(x^2+y^2)-(1)", "x", "y");
        double gpost = 0;
        check(circ2.valid() && eng.evaluateNumeric2D(circ2, 1.0, 0.0, gpost) &&
                  std::fabs(gpost) < 1e-12,
              "c01-2d-recompile-after-reset");
    }

    // ── GIAC-C01 benchmarks: explicit 1-D (strict), 2-D residual grids,
    // invalid-domain-heavy functions, and RSS drift across replots ──────────
    sweepStrict("x^2-2", "x", -5.0, 5.0, 1000);
    sweepStrict("sin(x)", "x", -5.0, 5.0, 1000);
    sweepStrict("x^2-2", "x", -5.0, 5.0, 10000);
    sweepStrict("sin(x)", "x", -5.0, 5.0, 10000);
    sweepStrict("x^2-2", "x", -5.0, 5.0, 100000);
    sweepStrict("sin(x)", "x", -5.0, 5.0, 100000);
    // invalid-domain-heavy: half the sweep range is outside the domain.
    sweepStrict("sqrt(x)", "x", -5.0, 5.0, 10000);
    sweepStrict("ln(x)", "x", -5.0, 5.0, 10000);
    // implicit 2-D residual grids: 1k, 10k, ~100k samples.
    sweep2D("(x^2+y^2)-(1)", -2.0, 2.0, 32);     //  1,024
    sweep2D("(x^2+y^2)-(1)", -2.0, 2.0, 100);    // 10,000
    sweep2D("(x^2+y^2)-(1)", -2.0, 2.0, 316);    // 99,856
    sweep2D("(sin(x)*cos(y))-((x*y)/(4))", -3.0, 3.0, 100);

    // Repeated replots with two retained handles: recompile + resample 50
    // times and require a bounded RSS drift (no unbounded growth).
    {
        size_t rssBefore = currentRssKb();
        double sink = 0;
        for (int rep = 0; rep < 50; ++rep) {
            numos::CompiledExpression f =
                eng.compileNumeric("sin(x)+x^2", "x", true);
            numos::CompiledExpression g =
                eng.compileNumeric2D("(x^2+y^2)-(4)", "x", "y");
            for (int i = 0; i < 500; ++i) {
                double x = -5.0 + 10.0 * i / 499.0, y;
                if (eng.evaluateNumeric(f, x, y)) sink += y;
                if (eng.evaluateNumeric2D(g, x, -x, y)) sink += y;
            }
        }
        size_t rssAfter = currentRssKb();
        long long deltaKb = (long long)rssAfter - (long long)rssBefore;
        printf("REPLOT|reps=50|samples_per_rep=1000|rss_delta_kb=%lld|checksum=%.6g\n",
               deltaKb, sink);
        check(deltaKb < 1024, "c01-replot-rss-stable");
    }

    printf("RSS_END|rss_kb=%zu\n", currentRssKb());
    printf("FOOTER|pass=%d|fail=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
