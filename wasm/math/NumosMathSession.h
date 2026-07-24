/*
 * WASM-MATH-01 — bounded, instance-scoped, headless-only facade over GiacEngine.
 *
 * No Giac type crosses this boundary.  The JSON returned by this class is
 * serialized directly from EngineResultNode, never from rendered text.
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "math/giac/GiacEngine.h"

namespace numos {

class NumosMathSession {
public:
    NumosMathSession();
    ~NumosMathSession();

    std::string create();
    std::string initialize();
    std::string evaluate(const char* expression, bool approximate);
    std::string simplify(const char* expression);
    std::string calculus(const char* expression, const char* variable,
                         bool integrate);
    std::string solve(const char* lhsLines, const char* rhsLines,
                      const char* variablesCsv, bool complexDomain);
    std::string compile1D(const char* expression, const char* variable);
    std::string compile2D(const char* expression, const char* variableA,
                          const char* variableB);
    std::string evaluate1D(const char* handle, double value);
    std::string evaluateMany1D(const char* handle, const double* values,
                               int count);
    std::string evaluate2D(const char* handle, double x, double y);
    std::string evaluateGrid2D(const char* handle, double xMin, double xMax,
                               double yMin, double yMax, int width, int height);
    std::string disposeHandle(const char* handle);
    std::string setAngleMode(const char* mode);
    std::string getAngleMode() const;
    std::string setVariable(const char* name, const char* expression);
    std::string removeVariable(const char* name);
    std::string clearVariables();
    std::string reset();
    std::string diagnostics() const;
    std::string shutdown();

private:
    enum class Lifecycle : uint8_t { New, Created, Initialized, Shutdown };
    enum class HandleState : uint8_t { Live, Disposed };
    struct HandleEntry {
        uint32_t generation = 0;
        uint32_t serial = 0;
        uint8_t dimensions = 0;
        HandleState state = HandleState::Live;
        CompiledExpression expression;
    };

    GiacEngine& _engine;
    Lifecycle _lifecycle = Lifecycle::New;
    std::map<std::string, std::string> _variables;
    std::map<uint32_t, HandleEntry> _handles;
    uint32_t _handleGeneration = 1;
    uint32_t _nextHandleSerial = 1;
    double _lastOperationMs = 0.0;
    std::string _lastDiagnostic;

    std::string requireReady() const;
    std::string validateExpression(const char* expression,
                                   bool allowEquation = true) const;
    std::string validateIdentifier(const char* identifier,
                                   bool storedVariable) const;
    std::string rebuildContext(bool bumpHandleGeneration);
    std::string makeHandleId(uint32_t generation, uint32_t serial) const;
    bool parseHandleId(const char* id, uint32_t& generation,
                       uint32_t& serial) const;
    HandleEntry* findHandle(const char* id, uint8_t dimensions,
                            std::string& error);
};

} // namespace numos
