// SPDX-License-Identifier: GPL-3.0-or-later
// Independent wording review harness: renders one verified graph in all locales.
#include "math/giac/GiacEngine.h"
#include <iostream>
#include <string>
bool setting_complex_enabled = false;
int main(int argc, char** argv) {
    using namespace numos;
    using namespace numos::tutor;
    auto& engine = GiacEngine::instance(); engine.begin();
    Snapshot snapshot;
    snapshot.inputEpoch = 1;
    snapshot.engineGeneration = engine.generation();
    std::string names = "xyz";
    std::vector<SolveEquation> equations;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--complex") { snapshot.complex = true; continue; }
        if (arg.rfind("--variables=", 0) == 0) { names = arg.substr(12); continue; }
        const auto pos = arg.find('='); if (pos == std::string::npos) return 2;
        snapshot.authored.push_back({arg.substr(0, pos), arg.substr(pos + 1)});
        equations.push_back({arg.substr(0, pos), arg.substr(pos + 1)});
    }
    if (equations.empty() || equations.size() > names.size()) return 2;
    for (size_t i = 0; i < equations.size(); ++i) snapshot.variables.emplace_back(1, names[i]);
    const std::vector<std::string> solveNames(snapshot.variables.begin(), snapshot.variables.end());
    const auto domain = snapshot.complex ? SolveDomainPolicy::RealAndComplex : SolveDomainPolicy::RealOnly;
    std::cerr << "phase=ordinary-answer\n";
    const auto answer = equations.size() == 1
        ? engine.solveStructured(equations[0], snapshot.variables[0], domain)
        : engine.solveSystemStructured(equations, solveNames, domain);
    std::cerr << "phase=tutor-construction\n";
    const auto derivation = engine.explainEquations(snapshot, answer);
    std::cerr << "phase=independent-replay\n";
    std::cout << "{\"catalog_valid\":" << validateCatalogs()
              << ",\"replay_verdict\":" << unsigned(engine.verifyDerivation(derivation, derivation.input))
              << ",\"locales\":[";
    std::cerr << "phase=locale-rendering\n";
    for (unsigned i = 0; i < 4; ++i) {
        if (i) std::cout << ',';
        std::cout << replayJson(derivation, Locale(i));
    }
    std::cout << "],\"wording_mutations\":[";
    bool firstMutation = true;
    auto reportMutation = [&](size_t index, const char* kind, size_t parameter, Derivation changed) {
        if (!firstMutation) std::cout << ',';
        firstMutation = false;
        std::cout << "{\"step\":" << index << ",\"kind\":\"" << kind
                  << "\",\"parameter\":" << parameter << ",\"verdict\":"
                  << unsigned(engine.verifyDerivation(changed, derivation.input)) << '}';
    };
    if (derivation.status == Status::Complete) {
        for (size_t i = 0; i < derivation.steps.size(); ++i) {
            auto wrongKey = derivation;
            wrongKey.steps[i].explanation = Message::Unsupported;
            wrongKey.steps[i].verification = Verdict::Verified;
            reportMutation(i, "wrong_key_marked_verified", 0, std::move(wrongKey));
            for (size_t j = 0; j < derivation.steps[i].parameters.size(); ++j) {
                auto wrongValue = derivation;
                wrongValue.steps[i].parameters[j].value = "1000003";
                wrongValue.steps[i].verification = Verdict::Verified;
                reportMutation(i, "wrong_parameter_marked_verified", j, std::move(wrongValue));
                auto wrongType = derivation;
                auto& kind = wrongType.steps[i].parameters[j].kind;
                kind = kind == ParameterKind::Expression ? ParameterKind::Variable : ParameterKind::Expression;
                reportMutation(i, "wrong_parameter_type", j, std::move(wrongType));
            }
        }
    }
    std::cout << "]}\n";
}
