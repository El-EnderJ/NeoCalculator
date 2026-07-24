#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include "NumosMathSession.h"

namespace {
std::unique_ptr<numos::NumosMathSession> session;
std::string response;

const char* store(std::string value) {
    response = std::move(value);
    return response.c_str();
}

const char* missing() {
    return store("{\"schemaVersion\":1,\"ok\":false,\"error\":{"
                 "\"code\":\"NOT_INITIALIZED\","
                 "\"message\":\"session has not been created\"}}");
}
}

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* numos_math_create() {
    if (session) return store("{\"schemaVersion\":1,\"ok\":false,\"error\":{"
        "\"code\":\"INVALID_REQUEST\",\"message\":\"session already exists\"}}");
    session = std::make_unique<numos::NumosMathSession>();
    return store(session->create());
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_initialize() {
    return session ? store(session->initialize()) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_evaluate(
    const char* expression, int approximate) {
    return session ? store(session->evaluate(expression, approximate != 0))
                   : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_simplify(const char* expression) {
    return session ? store(session->simplify(expression)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_calculus(
    const char* expression, const char* variable, int integrate) {
    return session
        ? store(session->calculus(expression, variable, integrate != 0))
        : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_solve(
    const char* lhs, const char* rhs, const char* variables, int complexDomain) {
    return session
        ? store(session->solve(lhs, rhs, variables, complexDomain != 0))
        : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_compile_1d(
    const char* expression, const char* variable) {
    return session ? store(session->compile1D(expression, variable)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_compile_2d(
    const char* expression, const char* variableA, const char* variableB) {
    return session
        ? store(session->compile2D(expression, variableA, variableB))
        : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_evaluate_1d(
    const char* handle, double value) {
    return session ? store(session->evaluate1D(handle, value)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_evaluate_many_1d(
    const char* handle, const double* values, int count) {
    return session
        ? store(session->evaluateMany1D(handle, values, count)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_evaluate_2d(
    const char* handle, const char* x, const char* y) {
    return session
        ? store(session->evaluate2D(handle, std::strtod(x, nullptr),
                                   std::strtod(y, nullptr)))
        : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_evaluate_grid_2d(
    const char* handle, const char* xMin, const char* xMax,
    const char* yMin, const char* yMax,
    int width, int height) {
    return session
        ? store(session->evaluateGrid2D(
              handle, std::strtod(xMin, nullptr), std::strtod(xMax, nullptr),
              std::strtod(yMin, nullptr), std::strtod(yMax, nullptr),
              width, height))
        : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_dispose_handle(const char* handle) {
    return session ? store(session->disposeHandle(handle)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_set_angle_mode(const char* mode) {
    return session ? store(session->setAngleMode(mode)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_get_angle_mode() {
    return session ? store(session->getAngleMode()) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_set_variable(
    const char* name, const char* expression) {
    return session ? store(session->setVariable(name, expression)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_remove_variable(const char* name) {
    return session ? store(session->removeVariable(name)) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_clear_variables() {
    return session ? store(session->clearVariables()) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_reset() {
    return session ? store(session->reset()) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_diagnostics() {
    return session ? store(session->diagnostics()) : missing();
}

EMSCRIPTEN_KEEPALIVE const char* numos_math_shutdown() {
    if (!session) return missing();
    std::string result = session->shutdown();
    session.reset();
    return store(std::move(result));
}

}
