/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

// GiacDiagnostics.cpp — see header. Firmware-only, flag-gated boot probe
// for the production GiacEngine seam. Uses the PUBLIC engine API only, so
// the numbers reflect exactly what Calculation/Grapher migrations will get.

#if defined(NUMOS_GIAC_DIAGNOSTICS) && defined(ARDUINO)

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <cmath>
#include <cstdio>

#include "math/giac/GiacDiagnostics.h"
#include "math/giac/GiacEngine.h"
#include "input/NumosSerialBackend.h"

namespace numos {

namespace {

struct HeapSnap {
    size_t intFree, intLarge, psFree, psLarge;
    unsigned stackHW;
};

HeapSnap snapHeap() {
    HeapSnap s;
    s.intFree  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s.intLarge = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    s.psFree   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s.psLarge  = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    s.stackHW  = (unsigned)uxTaskGetStackHighWaterMark(nullptr);
    return s;
}

void printHeap(const char* tag, const HeapSnap& s) {
    char line[160];
    snprintf(line, sizeof(line),
             "[GIACDIAG] heap %s int=%u/%u psram=%u/%u stack=%u", tag,
             (unsigned)s.intFree, (unsigned)s.intLarge, (unsigned)s.psFree,
             (unsigned)s.psLarge, s.stackHW);
    NUMOS_SERIAL.println(line);
}

// Compact representative suite: one probe per engine capability class.
struct SuiteCase {
    const char* expr;
    bool simplifyMode;
    MathEngineStatus want;
    const char* contains;  // nullptr = status check only
};

const SuiteCase kSuite[] = {
    {"2+2", false, MathEngineStatus::Ok, "4"},
    {"1/2+1/3", false, MathEngineStatus::Ok, "5/6"},
    {"2^100", false, MathEngineStatus::Ok, "1267650600228229401496703205376"},
    {"x-2*x", true, MathEngineStatus::Ok, "-x"},
    {"factor(x^3-6*x^2+11*x-6)", false, MathEngineStatus::Ok,
     "(x-1)*(x-2)*(x-3)"},
    {"solve(x^2-2=0,x)", false, MathEngineStatus::Ok, "sqrt(2)"},
    {"diff(sin(x)^2,x)", false, MathEngineStatus::Ok, "2*cos(x)*sin(x)"},
    {"integrate(x^2,x)", false, MathEngineStatus::Ok, "x^3"},
    {"sin(pi/6)", false, MathEngineStatus::Ok, "1/2"},
    {"2+*3", false, MathEngineStatus::ParseError, nullptr},
    {"1/0", false, MathEngineStatus::Ok, "oo"},
};

} // namespace

void runGiacDiagnostics() {
    NUMOS_SERIAL.println("[GIACDIAG] begin (NUMOS_GIAC_DIAGNOSTICS build)");
    GiacEngine& eng = GiacEngine::instance();
    HeapSnap before = snapHeap();
    printHeap("before-init", before);

    uint32_t t0 = micros();
    bool ok = eng.begin();
    uint32_t initUs = micros() - t0;
    printHeap("after-init", snapHeap());

    t0 = micros();
    MathEngineResult first = eng.evaluate("2+2");
    uint32_t firstUs = micros() - t0;

    // warm timing: mid-weight symbolic call x50
    eng.evaluate("diff(sin(x)^2,x)");
    t0 = micros();
    const int kWarm = 50;
    for (int i = 0; i < kWarm; ++i) eng.evaluate("diff(sin(x)^2,x)");
    uint32_t warmAvgUs = (micros() - t0) / kWarm;

    char line[160];
    snprintf(line, sizeof(line),
             "[GIACDIAG] timing init_us=%u first_eval_us=%u warm_avg_us=%u "
             "init_ok=%d first_ok=%d",
             (unsigned)initUs, (unsigned)firstUs, (unsigned)warmAvgUs,
             ok ? 1 : 0, first.ok() ? 1 : 0);
    NUMOS_SERIAL.println(line);

    // 1000-sample retained-expression sweep (Grapher-shaped)
    {
        CompiledExpression ce = eng.compileNumeric("sin(x)");
        int good = 0;
        double sum = 0;
        t0 = micros();
        for (int i = 0; i < 1000; ++i) {
            double x = -5.0 + 10.0 * i / 999.0;
            double y;
            if (eng.evaluateNumeric(ce, x, y) && std::isfinite(y)) {
                ++good;
                sum += y;
            }
        }
        uint32_t sweepUs = micros() - t0;
        snprintf(line, sizeof(line),
                 "[GIACDIAG] sweep sin(x) n=1000 total_us=%u per_point_us=%u "
                 "good=%d checksum=%.3g",
                 (unsigned)sweepUs, (unsigned)(sweepUs / 1000), good, sum);
        NUMOS_SERIAL.println(line);
    }

    // GIAC-C01: Grapher graph-path probes — strict 1-D compile+sweep and a
    // representative 2-D residual grid, through the SAME engine calls
    // GraphModel uses in normal builds. Compile time reported separately: a
    // successful compile is not a frame-time claim.
    {
        t0 = micros();
        CompiledExpression fx = eng.compileNumeric("x^2-2", "x", true);
        uint32_t c1Us = micros() - t0;
        int good = 0;
        double sum = 0;
        t0 = micros();
        for (int i = 0; i < 1000; ++i) {
            double x = -5.0 + 10.0 * i / 999.0;
            double y;
            if (eng.evaluateNumeric(fx, x, y) && std::isfinite(y)) {
                ++good;
                sum += y;
            }
        }
        uint32_t sweepUs = micros() - t0;
        snprintf(line, sizeof(line),
                 "[GIACDIAG] graph1d x^2-2 compile_us=%u n=1000 total_us=%u "
                 "per_point_us=%u good=%d checksum=%.3g",
                 (unsigned)c1Us, (unsigned)sweepUs,
                 (unsigned)(sweepUs / 1000), good, sum);
        NUMOS_SERIAL.println(line);

        t0 = micros();
        CompiledExpression g = eng.compileNumeric2D("(x^2+y^2)-(1)", "x", "y");
        uint32_t c2Us = micros() - t0;
        good = 0;
        sum = 0;
        const int side = 32;   // 1024-sample residual grid
        t0 = micros();
        for (int j = 0; j < side; ++j) {
            double y = -2.0 + 4.0 * j / (side - 1);
            for (int i = 0; i < side; ++i) {
                double x = -2.0 + 4.0 * i / (side - 1);
                double r;
                if (eng.evaluateNumeric2D(g, x, y, r) && std::isfinite(r)) {
                    ++good;
                    sum += r;
                }
            }
        }
        uint32_t gridUs = micros() - t0;
        snprintf(line, sizeof(line),
                 "[GIACDIAG] graph2d circle compile_us=%u n=%d total_us=%u "
                 "per_point_us=%u good=%d checksum=%.3g",
                 (unsigned)c2Us, side * side, (unsigned)gridUs,
                 (unsigned)(gridUs / (side * side)), good, sum);
        NUMOS_SERIAL.println(line);
    }

    // compact suite
    int pass = 0, fail = 0;
    for (const SuiteCase& c : kSuite) {
        MathEngineResult r =
            c.simplifyMode ? eng.simplify(c.expr) : eng.evaluate(c.expr);
        bool casePass = (r.status == c.want) &&
                        (!c.contains ||
                         r.exactText.find(c.contains) != std::string::npos);
        (casePass ? pass : fail)++;
        if (!casePass) {
            snprintf(line, sizeof(line), "[GIACDIAG] FAIL %s -> %s", c.expr,
                     r.ok() ? r.exactText.c_str() : r.diagnostic.c_str());
            NUMOS_SERIAL.println(line);
        }
    }

    HeapSnap after = snapHeap();
    printHeap("after-suite", after);
    snprintf(line, sizeof(line),
             "[GIACDIAG] summary pass=%d fail=%d psram_used_by_run=%d "
             "int_used_by_run=%d",
             pass, fail, (int)((long)before.psFree - (long)after.psFree),
             (int)((long)before.intFree - (long)after.intFree));
    NUMOS_SERIAL.println(line);
    NUMOS_SERIAL.println("[GIACDIAG] end");
}

} // namespace numos

#endif // NUMOS_GIAC_DIAGNOSTICS && ARDUINO
