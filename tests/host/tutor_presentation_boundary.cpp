// SPDX-License-Identifier: GPL-3.0-or-later
// Differential presentation-boundary corpus: no solver or arithmetic replacement.
#include "math/giac/GiacEngine.h"
#include "math/AngleModeRuntime.h"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

bool setting_complex_enabled = true;
namespace {
void signature(const numos::EngineResultNode& n) {
    std::cout << '(' << unsigned(n.kind) << ':' << n.text.size() << ':' << n.text
              << ':' << unsigned(n.fallbackReason) << ':' << unsigned(n.rows)
              << ':' << unsigned(n.columns) << ':' << n.leftClosed << n.rightClosed;
    for (const auto& child : n.children) signature(child);
    std::cout << ')';
}
}
int main() {
    using namespace numos;
    auto& engine = GiacEngine::instance();
    if (!engine.begin()) return 1;
    setAngleMode(vpam::AngleMode::DEG);
    engine.evaluate("x:=7"); engine.evaluate("A:=11"); engine.evaluate("assume(z>0)");
    const auto assumption = engine.evaluate("abs(z)").exactText;
    const auto generation = engine.generation();
    auto graph = engine.compileNumeric("sin(x)", "x", true);
    double before = 0, after = 0;
    if (!engine.evaluateNumeric(graph, 30, before)) return 2;
    struct Case { const char* lhs; const char* rhs; bool accepted; };
    const Case fixed[] = {
        {"x", "1", true}, {"x", "-3", true}, {"x", "(-3-sqrt(41))/4", true},
        {"x", "(-3+sqrt(41))/4", true}, {"x", "i", true}, {"x", "-i", true},
        {"x", "(-3+sqrt(-23))/4", true}, {"x", "sqrt(2)", true},
        {"x/x", "1", true}, {"(x^2-1)/(x-1)", "0", true},
        {"1/(x-1)", "1", true}, {"1/(x^2-1)", "0", true},
        {"1/(x-x)", "0", false}, {"sqrt(x)", "2", false},
        {"log(x)", "1", false}, {"sin(x)", "0", false},
        {"x:=2", "0", false}, {"(x:=2)^2", "0", false},
        {"1/(x:=2)", "0", false}, {"[x,1]", "0", false},
        {"x", "1/0", false}, {"A", "11", false},
        {"x", "1/(sqrt(2)-sqrt(2))", false}
    };
    unsigned count = 0;
    auto check = [&](const std::string& lhs, const std::string& rhs, bool accepted) {
        const auto result = engine.tutorFormula({lhs, rhs});
        std::cout << "FORMULA|" << count++ << '|' << lhs << '=' << rhs << '|'
                  << unsigned(result.base.status) << '|' << result.hasTree << '|';
        signature(result.tree); std::cout << '\n';
        return result.hasTree == accepted;
    };
    for (const auto& test : fixed) if (!check(test.lhs, test.rhs, test.accepted)) return 3;
    // Seeded exact constants and authored variable denominators exercise both
    // routes. The differential oracle compares every typed node, not final roots.
    unsigned seed = 0x7101;
    for (unsigned i = 0; i < 80; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const auto a = std::to_string(1 + seed % 13);
        const auto b = std::to_string(int((seed >> 8) % 31) - 15);
        const auto d = std::to_string(2 + (seed >> 16) % 79);
        if (!check("x", "(" + b + (i % 2 ? "+" : "-") + "sqrt(" + d + "))/" + a, true) ||
            !check("(" + a + "*x+" + b + ")/(x-" + d + ")", "0", true)) return 4;
    }
    if (engine.generation() != generation || !angleModeIsDeg() ||
        engine.evaluate("x").exactText != "7" || engine.evaluate("A").exactText != "11" ||
        engine.evaluate("abs(z)").exactText != assumption || !graph.valid() ||
        !engine.evaluateNumeric(graph, 30, after) || before != after ||
        std::abs(before - 0.5) > 1e-12) return 5;
    graph = {}; engine.reset();
    if (engine.generation() == generation || !check("x", "(-3-sqrt(41))/4", true)) return 6;
    std::cout << "PRESENTATION_BOUNDARY_PASS " << count << '\n';
}
