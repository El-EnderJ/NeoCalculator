/**
 * NeoModules.cpp — Module loader implementation for NeoLanguage Phase 8.
 *
 * Each module is a named collection of NativeFunction NeoValues.
 * Native callables forward to the same C++ implementations used by the
 * global built-in dispatcher in NeoStdLib / NeoInterpreter, ensuring
 * that `import finance as fin` gives exactly the same functions as the
 * global `tvm_pv()`.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 8 (Ecosystem)
 */

#include "NeoModules.h"
#include "NeoFinance.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════
// Static helper callbacks (capture-free — decay to function pointer)
// ════════════════════════════════════════════════════════════════════

// ── Math: unary double → double (fn passed via ctx) ──────────────

typedef double (*UnaryMathFn)(double);

static NeoValue neoModUnaryMath(const std::vector<NeoValue>& args,
                                void* ctx,
                                cas::SymExprArena&)
{
    auto fn = reinterpret_cast<UnaryMathFn>(ctx);
    if (args.empty()) return NeoValue::makeNumber(0);
    return NeoValue::makeNumber(fn(args[0].toDouble()));
}

static NeoValue neoModAtan2(const std::vector<NeoValue>& args,
                             void*, cas::SymExprArena&)
{
    if (args.size() < 2) return NeoValue::makeNumber(0);
    return NeoValue::makeNumber(std::atan2(args[0].toDouble(), args[1].toDouble()));
}

static NeoValue neoModPow(const std::vector<NeoValue>& args,
                           void*, cas::SymExprArena&)
{
    if (args.size() < 2) return NeoValue::makeNumber(0);
    return NeoValue::makeNumber(std::pow(args[0].toDouble(), args[1].toDouble()));
}

// ── Finance ──────────────────────────────────────────────────────

static NeoValue neoModTvmPV(const std::vector<NeoValue>& a,
                              void*, cas::SymExprArena&)
{
    if (a.size() < 3) return NeoValue::makeNumber(0);
    double rate = a[0].toDouble(), n = a[1].toDouble(), pmt = a[2].toDouble();
    double fv   = a.size() >= 4 ? a[3].toDouble() : 0.0;
    return NeoValue::makeNumber(NeoFinance::solvePV(rate, n, pmt, fv));
}

static NeoValue neoModTvmFV(const std::vector<NeoValue>& a,
                              void*, cas::SymExprArena&)
{
    if (a.size() < 3) return NeoValue::makeNumber(0);
    double rate = a[0].toDouble(), n = a[1].toDouble(), pmt = a[2].toDouble();
    double pv   = a.size() >= 4 ? a[3].toDouble() : 0.0;
    return NeoValue::makeNumber(NeoFinance::solveFV(rate, n, pmt, pv));
}

static NeoValue neoModTvmPMT(const std::vector<NeoValue>& a,
                               void*, cas::SymExprArena&)
{
    if (a.size() < 3) return NeoValue::makeNumber(0);
    double rate = a[0].toDouble(), n = a[1].toDouble(), pv = a[2].toDouble();
    double fv   = a.size() >= 4 ? a[3].toDouble() : 0.0;
    return NeoValue::makeNumber(NeoFinance::solvePMT(rate, n, pv, fv));
}

static NeoValue neoModTvmN(const std::vector<NeoValue>& a,
                             void*, cas::SymExprArena&)
{
    if (a.size() < 3) return NeoValue::makeNumber(0);
    double rate = a[0].toDouble(), pmt = a[1].toDouble(), pv = a[2].toDouble();
    double fv   = a.size() >= 4 ? a[3].toDouble() : 0.0;
    return NeoValue::makeNumber(NeoFinance::solveN(rate, pmt, pv, fv));
}

// ── Electronics ──────────────────────────────────────────────────

static NeoValue neoModBitGet(const std::vector<NeoValue>& a,
                               void*, cas::SymExprArena&)
{
    if (a.size() < 2) return NeoValue::makeNumber(0);
    uint32_t val = static_cast<uint32_t>(a[0].toDouble());
    int bit = static_cast<int>(a[1].toDouble());
    return NeoValue::makeNumber((val >> bit) & 1u);
}

static NeoValue neoModBitSet(const std::vector<NeoValue>& a,
                               void*, cas::SymExprArena&)
{
    if (a.size() < 2) return NeoValue::makeNumber(0);
    uint32_t val = static_cast<uint32_t>(a[0].toDouble());
    int bit = static_cast<int>(a[1].toDouble());
    return NeoValue::makeNumber(static_cast<double>(val | (1u << bit)));
}

static NeoValue neoModBitClear(const std::vector<NeoValue>& a,
                                 void*, cas::SymExprArena&)
{
    if (a.size() < 2) return NeoValue::makeNumber(0);
    uint32_t val = static_cast<uint32_t>(a[0].toDouble());
    int bit = static_cast<int>(a[1].toDouble());
    return NeoValue::makeNumber(static_cast<double>(val & ~(1u << bit)));
}

static NeoValue neoModToBin(const std::vector<NeoValue>& a,
                              void*, cas::SymExprArena&)
{
    if (a.empty()) return NeoValue::makeString("0b0");
    uint32_t val = static_cast<uint32_t>(a[0].toDouble());
    if (val == 0) return NeoValue::makeString("0b0");
    std::string bits = "0b";
    bool leading = true;
    for (int i = 31; i >= 0; --i) {
        bool b = (val >> i) & 1u;
        if (b) leading = false;
        if (!leading) bits += (b ? '1' : '0');
    }
    return NeoValue::makeString(bits);
}

static NeoValue neoModToHex(const std::vector<NeoValue>& a,
                              void*, cas::SymExprArena&)
{
    if (a.empty()) return NeoValue::makeString("0x0");
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%X",
        static_cast<unsigned int>(a[0].toDouble()));
    return NeoValue::makeString(std::string(buf));
}

// ── Stats ────────────────────────────────────────────────────────

static NeoValue neoModMean(const std::vector<NeoValue>& a,
                             void*, cas::SymExprArena&)
{
    if (a.empty() || !a[0].isList() || !a[0].asList() || a[0].asList()->empty())
        return NeoValue::makeNumber(0);
    const auto& lst = *a[0].asList();
    double s = 0.0;
    for (const auto& v : lst) s += v.toDouble();
    return NeoValue::makeNumber(s / static_cast<double>(lst.size()));
}

static NeoValue neoModStddev(const std::vector<NeoValue>& a,
                               void*, cas::SymExprArena&)
{
    if (a.empty() || !a[0].isList() || !a[0].asList() || a[0].asList()->size() < 2)
        return NeoValue::makeNumber(0);
    const auto& lst = *a[0].asList();
    double s = 0.0;
    for (const auto& v : lst) s += v.toDouble();
    double mean = s / static_cast<double>(lst.size());
    double sq = 0.0;
    for (const auto& v : lst) { double d = v.toDouble() - mean; sq += d * d; }
    return NeoValue::makeNumber(std::sqrt(sq / static_cast<double>(lst.size() - 1)));
}

static NeoValue neoModVariance(const std::vector<NeoValue>& a,
                                 void*, cas::SymExprArena&)
{
    if (a.empty() || !a[0].isList() || !a[0].asList() || a[0].asList()->size() < 2)
        return NeoValue::makeNumber(0);
    const auto& lst = *a[0].asList();
    double s = 0.0;
    for (const auto& v : lst) s += v.toDouble();
    double mean = s / static_cast<double>(lst.size());
    double sq = 0.0;
    for (const auto& v : lst) { double d = v.toDouble() - mean; sq += d * d; }
    return NeoValue::makeNumber(sq / static_cast<double>(lst.size() - 1));
}

// ── Signal: Cooley-Tukey Radix-2 FFT helper ──────────────────────

static void fftInPlace(std::vector<double>& re, std::vector<double>& im, bool inverse)
{
    size_t M = re.size();
    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < M; ++i) {
        size_t bit = M >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    // Butterfly stages
    double sign = inverse ? 1.0 : -1.0;
    for (size_t len = 2; len <= M; len <<= 1) {
        double ang  = sign * 2.0 * M_PI / static_cast<double>(len);
        double wRe  = std::cos(ang), wIm = std::sin(ang);
        for (size_t i = 0; i < M; i += len) {
            double curRe = 1.0, curIm = 0.0;
            for (size_t k = 0; k < len / 2; ++k) {
                double uRe = re[i + k], uIm = im[i + k];
                double tRe = curRe * re[i + k + len/2] - curIm * im[i + k + len/2];
                double tIm = curRe * im[i + k + len/2] + curIm * re[i + k + len/2];
                re[i + k]           = uRe + tRe;  im[i + k]           = uIm + tIm;
                re[i + k + len / 2] = uRe - tRe;  im[i + k + len / 2] = uIm - tIm;
                double tmp = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = tmp;
            }
        }
    }
    if (inverse) {
        double invN = 1.0 / static_cast<double>(M);
        for (size_t i = 0; i < M; ++i) { re[i] *= invN; im[i] *= invN; }
    }
}

static NeoValue neoModFFT(const std::vector<NeoValue>& a,
                            void* ctx, cas::SymExprArena&)
{
    bool inverse = (ctx != nullptr); // ctx=nullptr → forward, ctx=(void*)1 → inverse
    if (a.empty() || !a[0].isList() || !a[0].asList())
        return NeoValue::makeList(new std::vector<NeoValue>());
    const auto& in = *a[0].asList();
    size_t N = in.size();
    size_t M = 1;
    while (M < N) M <<= 1;

    std::vector<double> re(M, 0.0), im(M, 0.0);
    for (size_t i = 0; i < N; ++i) {
        if (in[i].isList() && in[i].asList() && in[i].asList()->size() >= 2) {
            re[i] = (*in[i].asList())[0].toDouble();
            im[i] = (*in[i].asList())[1].toDouble();
        } else {
            re[i] = in[i].toDouble();
        }
    }
    fftInPlace(re, im, inverse);

    auto* out_lst = new std::vector<NeoValue>();
    out_lst->reserve(M);
    for (size_t i = 0; i < M; ++i) {
        auto* pair = new std::vector<NeoValue>();
        pair->push_back(NeoValue::makeNumber(re[i]));
        pair->push_back(NeoValue::makeNumber(im[i]));
        out_lst->push_back(NeoValue::makeList(pair));
    }
    return NeoValue::makeList(out_lst);
}

static NeoValue neoModAbsSpectrum(const std::vector<NeoValue>& a,
                                    void*, cas::SymExprArena&)
{
    if (a.empty() || !a[0].isList() || !a[0].asList())
        return NeoValue::makeList(new std::vector<NeoValue>());
    const auto& in = *a[0].asList();
    auto* out_lst = new std::vector<NeoValue>();
    out_lst->reserve(in.size());
    for (const auto& v : in) {
        if (v.isList() && v.asList() && v.asList()->size() >= 2) {
            double r = (*v.asList())[0].toDouble();
            double im2 = (*v.asList())[1].toDouble();
            out_lst->push_back(NeoValue::makeNumber(std::sqrt(r * r + im2 * im2)));
        } else {
            out_lst->push_back(NeoValue::makeNumber(std::fabs(v.toDouble())));
        }
    }
    return NeoValue::makeList(out_lst);
}

// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

bool NeoModules::exists(const std::string& name)
{
    return name == "math"
        || name == "finance"
        || name == "electronics"
        || name == "stats"
        || name == "signal";
}

std::vector<std::string> NeoModules::availableModules()
{
    return { "math", "finance", "electronics", "stats", "signal" };
}

bool NeoModules::loadIntoEnv(const std::string& name,
                              NeoEnv& env,
                              cas::SymExprArena& sa)
{
    std::map<std::string, NeoValue> exports;
    if (!buildModule(name, exports, sa)) return false;
    for (auto& kv : exports) env.define(kv.first, kv.second);
    return true;
}

bool NeoModules::loadAsDict(const std::string& name,
                             NeoValue& dict,
                             cas::SymExprArena& sa)
{
    std::map<std::string, NeoValue> exports;
    if (!buildModule(name, exports, sa)) return false;
    auto* d = new std::map<std::string, NeoValue>(std::move(exports));
    dict = NeoValue::makeDict(d);
    return true;
}

bool NeoModules::buildModule(const std::string& name,
                              std::map<std::string, NeoValue>& out,
                              cas::SymExprArena& sa)
{
    if (name == "math")         { buildMath(out, sa);         return true; }
    if (name == "finance")      { buildFinance(out, sa);      return true; }
    if (name == "electronics")  { buildElectronics(out, sa);  return true; }
    if (name == "stats")        { buildStats(out, sa);        return true; }
    if (name == "signal")       { buildSignal(out, sa);       return true; }
    return false;
}

// ════════════════════════════════════════════════════════════════════
// Module builders
// ════════════════════════════════════════════════════════════════════

void NeoModules::buildMath(std::map<std::string, NeoValue>& out,
                            cas::SymExprArena& /*sa*/)
{
    out["pi"]  = NeoValue::makeNumber(M_PI);
    out["e"]   = NeoValue::makeNumber(M_E);
    out["phi"] = NeoValue::makeNumber(1.6180339887498948482);
    out["inf"] = NeoValue::makeNumber(std::numeric_limits<double>::infinity());

    // Pass the C standard-library function pointer through ctx
    out["sin"]   = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::sin)));
    out["cos"]   = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::cos)));
    out["tan"]   = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::tan)));
    out["asin"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::asin)));
    out["acos"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::acos)));
    out["atan"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::atan)));
    out["sqrt"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::sqrt)));
    out["exp"]   = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::exp)));
    out["log"]   = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::log)));
    out["log10"] = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::log10)));
    out["log2"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::log2)));
    out["abs"]   = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::fabs)));
    out["floor"] = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::floor)));
    out["ceil"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::ceil)));
    out["sinh"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::sinh)));
    out["cosh"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::cosh)));
    out["tanh"]  = NeoValue::makeNativeFunction(neoModUnaryMath, reinterpret_cast<void*>(static_cast<UnaryMathFn>(std::tanh)));
    out["atan2"] = NeoValue::makeNativeFunction(neoModAtan2, nullptr);
    out["pow"]   = NeoValue::makeNativeFunction(neoModPow,   nullptr);
}

void NeoModules::buildFinance(std::map<std::string, NeoValue>& out,
                               cas::SymExprArena& /*sa*/)
{
    out["tvm_pv"]  = NeoValue::makeNativeFunction(neoModTvmPV,  nullptr);
    out["tvm_fv"]  = NeoValue::makeNativeFunction(neoModTvmFV,  nullptr);
    out["tvm_pmt"] = NeoValue::makeNativeFunction(neoModTvmPMT, nullptr);
    out["tvm_n"]   = NeoValue::makeNativeFunction(neoModTvmN,   nullptr);
}

void NeoModules::buildElectronics(std::map<std::string, NeoValue>& out,
                                   cas::SymExprArena& /*sa*/)
{
    out["bit_get"]   = NeoValue::makeNativeFunction(neoModBitGet,   nullptr);
    out["bit_set"]   = NeoValue::makeNativeFunction(neoModBitSet,   nullptr);
    out["bit_clear"] = NeoValue::makeNativeFunction(neoModBitClear, nullptr);
    out["to_bin"]    = NeoValue::makeNativeFunction(neoModToBin,    nullptr);
    out["to_hex"]    = NeoValue::makeNativeFunction(neoModToHex,    nullptr);
}

void NeoModules::buildStats(std::map<std::string, NeoValue>& out,
                             cas::SymExprArena& /*sa*/)
{
    out["mean"]     = NeoValue::makeNativeFunction(neoModMean,     nullptr);
    out["stddev"]   = NeoValue::makeNativeFunction(neoModStddev,   nullptr);
    out["variance"] = NeoValue::makeNativeFunction(neoModVariance, nullptr);
}

void NeoModules::buildSignal(std::map<std::string, NeoValue>& out,
                              cas::SymExprArena& /*sa*/)
{
    // ctx=nullptr → forward FFT; ctx=(void*)1 → inverse FFT
    out["fft"]          = NeoValue::makeNativeFunction(neoModFFT,         nullptr);
    out["ifft"]         = NeoValue::makeNativeFunction(neoModFFT,         reinterpret_cast<void*>(1));
    out["abs_spectrum"] = NeoValue::makeNativeFunction(neoModAbsSpectrum, nullptr);
}
