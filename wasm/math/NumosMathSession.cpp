#include "NumosMathSession.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>

#include "math/AngleModeRuntime.h"
#include "math/giac/EngineContracts.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
#include <malloc.h>
#endif

namespace numos {
namespace {

using Clock = std::chrono::steady_clock;
constexpr unsigned kSchemaVersion = 1;
constexpr int kMaxBatchValues = 10000;
constexpr int kMaxGridWidth = 256;
constexpr int kMaxGridHeight = 256;
constexpr int kMaxGridCells = 65536;
constexpr int kMaxSystemEquations = 4;
constexpr int kMaxVariables = 6;
constexpr std::size_t kMaxDiagnosticBytes = 512;
constexpr std::size_t kMaxDisplayBytes = 512;

std::string jsonEscape(std::string_view input, std::size_t limit) {
    std::string out;
    const std::size_t size = std::min(input.size(), limit);
    out.reserve(size + 16);
    for (std::size_t i = 0; i < size; ++i) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    if (input.size() > limit) out += "...";
    return out;
}

const char* nodeKindName(EngineNodeKind kind) {
    switch (kind) {
        case EngineNodeKind::Integer: return "integer";
        case EngineNodeKind::Decimal: return "decimal";
        case EngineNodeKind::Rational: return "rational";
        case EngineNodeKind::Symbol: return "symbol";
        case EngineNodeKind::Add: return "sum";
        case EngineNodeKind::Neg: return "negative";
        case EngineNodeKind::Mul: return "product";
        case EngineNodeKind::Inv: return "inverse";
        case EngineNodeKind::Pow: return "power";
        case EngineNodeKind::Sqrt: return "sqrt";
        case EngineNodeKind::Root: return "root";
        case EngineNodeKind::Function: return "function";
        case EngineNodeKind::Pi: return "pi";
        case EngineNodeKind::EulerE: return "e";
        case EngineNodeKind::ImagUnit: return "imaginary_unit";
        case EngineNodeKind::PlusInfinity: return "positive_infinity";
        case EngineNodeKind::MinusInfinity: return "negative_infinity";
        case EngineNodeKind::UnsignedInfinity: return "unsigned_infinity";
        case EngineNodeKind::Equation: return "equation";
        case EngineNodeKind::Assignment: return "assignment";
        case EngineNodeKind::List: return "list";
        case EngineNodeKind::Set: return "set";
        case EngineNodeKind::Matrix: return "matrix";
        case EngineNodeKind::Interval: return "interval";
        case EngineNodeKind::Piecewise: return "piecewise";
        case EngineNodeKind::Complex: return "complex";
        case EngineNodeKind::Unevaluated: return "unevaluated_call";
        case EngineNodeKind::Undefined: return "undefined";
        case EngineNodeKind::Unsupported: return "opaque";
    }
    return "opaque";
}

void appendNodeJson(std::ostringstream& out, const EngineResultNode& node,
                    int depth = 0) {
    if (depth > enginecontract::kMaxResultDepth) {
        out << "{\"kind\":\"opaque\",\"reason\":\"depth_limit\"}";
        return;
    }
    out << "{\"kind\":\"" << nodeKindName(node.kind) << '"';
    switch (node.kind) {
        case EngineNodeKind::Integer:
        case EngineNodeKind::Decimal:
        case EngineNodeKind::Symbol:
            out << ",\"value\":\""
                << jsonEscape(node.text, kMaxDisplayBytes) << '"';
            break;
        case EngineNodeKind::Function:
            out << ",\"name\":\""
                << jsonEscape(node.text, 64) << '"';
            break;
        case EngineNodeKind::Matrix:
            out << ",\"rows\":" << static_cast<unsigned>(node.rows)
                << ",\"columns\":" << static_cast<unsigned>(node.columns);
            break;
        case EngineNodeKind::Interval:
            out << ",\"leftClosed\":" << (node.leftClosed ? "true" : "false")
                << ",\"rightClosed\":" << (node.rightClosed ? "true" : "false");
            break;
        case EngineNodeKind::Unsupported:
            out << ",\"reason\":\""
                << engineFallbackReasonName(node.fallbackReason) << '"';
            if (!node.text.empty())
                out << ",\"displayText\":\""
                    << jsonEscape(node.text, kMaxDisplayBytes) << '"';
            break;
        default:
            break;
    }
    if (!node.children.empty()) {
        out << ",\"children\":[";
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (i) out << ',';
            appendNodeJson(out, node.children[i], depth + 1);
        }
        out << ']';
    }
    out << '}';
}

std::string errorJson(const char* code, std::string_view message) {
    std::ostringstream out;
    out << "{\"schemaVersion\":" << kSchemaVersion
        << ",\"ok\":false,\"error\":{\"code\":\"" << code
        << "\",\"message\":\"" << jsonEscape(message, kMaxDiagnosticBytes)
        << "\"}}";
    return out.str();
}

std::string okJson(std::string_view body = {}) {
    std::string out = "{\"schemaVersion\":1,\"ok\":true";
    if (!body.empty()) {
        out += ',';
        out += body;
    }
    out += '}';
    return out;
}

const char* statusCode(MathEngineStatus status) {
    switch (status) {
        case MathEngineStatus::ParseError: return "PARSE_ERROR";
        case MathEngineStatus::EvaluationError: return "EVALUATION_ERROR";
        case MathEngineStatus::Unsupported: return "UNSUPPORTED_RESULT";
        case MathEngineStatus::OutOfMemory: return "INTERNAL_ERROR";
        case MathEngineStatus::Undefined:
        case MathEngineStatus::Ok: return nullptr;
    }
    return "INTERNAL_ERROR";
}

std::string structuredJson(const StructuredEngineResult& result,
                           bool approximate, double elapsedMs) {
    if (const char* code = statusCode(result.base.status))
        return errorJson(code, result.base.diagnostic);
    const EngineResultNode* tree = nullptr;
    if (approximate && result.hasApproximateTree)
        tree = &result.approximateTree;
    else if (result.hasTree)
        tree = &result.tree;
    if (!tree) return errorJson("UNSUPPORTED_RESULT",
                                "Giac result has no bounded structured tree");
    std::ostringstream out;
    out << "\"result\":";
    appendNodeJson(out, *tree);
    out << ",\"exact\":";
    appendNodeJson(out, result.tree);
    if (result.hasApproximateTree) {
        out << ",\"approximate\":";
        appendNodeJson(out, result.approximateTree);
    } else {
        out << ",\"approximate\":null";
    }
    out << ",\"displayText\":\""
        << jsonEscape(approximate && !result.base.approximateText.empty()
                          ? result.base.approximateText
                          : result.base.exactText,
                      kMaxDisplayBytes)
        << "\",\"operationMs\":" << std::fixed << std::setprecision(3)
        << elapsedMs;
    return okJson(out.str());
}

std::string calculusJson(const StructuredCalculusResult& result,
                         double elapsedMs) {
    if (const char* code = statusCode(result.status))
        return errorJson(code, result.diagnostic);
    if (!result.hasTree)
        return errorJson("UNSUPPORTED_RESULT",
                         "Giac calculus result has no structured tree");
    std::ostringstream out;
    out << "\"result\":";
    appendNodeJson(out, result.tree);
    out << ",\"approximate\":null,\"unevaluated\":"
        << (result.unevaluated ? "true" : "false")
        << ",\"displayText\":\""
        << jsonEscape(result.exactText, kMaxDisplayBytes)
        << "\",\"operationMs\":" << std::fixed << std::setprecision(3)
        << elapsedMs;
    return okJson(out.str());
}

std::string numberJson(double value, bool valid) {
    if (!valid || std::isnan(value))
        return "{\"kind\":\"non_finite\",\"value\":\"nan\"}";
    if (std::isinf(value))
        return value > 0
            ? "{\"kind\":\"non_finite\",\"value\":\"positive_infinity\"}"
            : "{\"kind\":\"non_finite\",\"value\":\"negative_infinity\"}";
    std::ostringstream out;
    out << "{\"kind\":\"finite\",\"value\":"
        << std::setprecision(17) << value << '}';
    return out.str();
}

std::vector<std::string> split(const char* text, char delimiter) {
    std::vector<std::string> result;
    if (!text) return result;
    std::string_view input(text);
    std::size_t begin = 0;
    while (begin <= input.size()) {
        const std::size_t end = input.find(delimiter, begin);
        result.emplace_back(input.substr(begin,
            end == std::string_view::npos ? input.size() - begin : end - begin));
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return result;
}

bool isAllowedFunction(std::string_view name) {
    static const std::set<std::string_view> allowed = {
        "abs", "acos", "asin", "atan", "ceil", "cos", "cosh", "diff",
        "exp", "expand", "factor", "floor", "integrate", "ln", "log",
        "log10", "logb", "matrix", "piecewise", "regroup", "simplify",
        "sin", "sinh", "sqrt", "surd", "tan", "tanh"
    };
    return allowed.find(name) != allowed.end();
}

bool isReservedIdentifier(std::string_view name) {
    static const std::set<std::string_view> reserved = {
        "Ans", "PreAns", "archive", "assume", "cd", "debug", "delete",
        "erase", "eval", "evalc", "exec", "giac", "help", "input", "kill",
        "numos_Ans", "numos_PreAns", "open", "purge", "read", "restart",
        "shell", "system", "write"
    };
    return reserved.find(name) != reserved.end();
}

} // namespace

NumosMathSession::NumosMathSession() : _engine(GiacEngine::instance()) {}

NumosMathSession::~NumosMathSession() {
    if (_lifecycle != Lifecycle::Shutdown) shutdown();
}

std::string NumosMathSession::create() {
    if (_lifecycle == Lifecycle::Initialized)
        return errorJson("INVALID_REQUEST", "session is already initialized");
    _lifecycle = Lifecycle::Created;
    return okJson("\"state\":\"created\"");
}

std::string NumosMathSession::initialize() {
    if (_lifecycle == Lifecycle::New)
        return errorJson("INVALID_REQUEST", "create must be called first");
    if (_lifecycle == Lifecycle::Shutdown)
        return errorJson("SHUTDOWN", "session has been shut down");
    if (!_engine.begin())
        return errorJson("INTERNAL_ERROR", "Giac context initialization failed");
    _lifecycle = Lifecycle::Initialized;
    return okJson("\"state\":\"ready\"");
}

std::string NumosMathSession::requireReady() const {
    if (_lifecycle == Lifecycle::Shutdown)
        return errorJson("SHUTDOWN", "session has been shut down");
    if (_lifecycle != Lifecycle::Initialized)
        return errorJson("NOT_INITIALIZED", "session is not initialized");
    return {};
}

std::string NumosMathSession::validateIdentifier(
    const char* identifier, bool storedVariable) const {
    if (!identifier ||
        !enginecontract::isPlainIdentifier(identifier, 31) ||
        isReservedIdentifier(identifier))
        return errorJson("INVALID_IDENTIFIER", "invalid or reserved identifier");
    const std::string_view name(identifier);
    static const std::set<std::string_view> giacReserved = {
        "pi", "e", "i", "oo", "undef", "solve", "csolve", "fsolve",
        "sin", "cos", "tan", "asin", "acos", "atan", "ln", "log",
        "log10", "sqrt", "surd", "exp", "abs", "diff", "integrate"
    };
    if (giacReserved.count(name))
        return errorJson("INVALID_IDENTIFIER", "invalid or reserved identifier");
    if (storedVariable &&
        !(name.size() == 1 && name.front() >= 'A' && name.front() <= 'F'))
        return errorJson("INVALID_IDENTIFIER",
                         "stored variables are limited to A through F");
    return {};
}

std::string NumosMathSession::validateExpression(
    const char* expression, bool allowEquation) const {
    if (!expression || !*expression)
        return errorJson("INVALID_REQUEST", "expression is empty");
    const std::size_t length = std::strlen(expression);
    if (length > enginecontract::kMaxSourceBytes)
        return errorJson("SOURCE_TOO_LONG", "expression exceeds source limit");
    int depth = 0;
    int nodes = 0;
    for (std::size_t i = 0; i < length;) {
        const unsigned char c = static_cast<unsigned char>(expression[i]);
        if (c < 0x20 || c == ';' || c == '\\' || c == '"' || c == '\'' ||
            (c == ':' && i + 1 < length && expression[i + 1] == '='))
            return errorJson("INVALID_REQUEST",
                             "expression contains a forbidden token");
        if (c == '(' || c == '[' || c == '{') {
            if (++depth > enginecontract::kMaxTreeDepth)
                return errorJson("DEPTH_LIMIT", "expression nesting is too deep");
            ++nodes;
            ++i;
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (--depth < 0)
                return errorJson("PARSE_ERROR", "unbalanced delimiters");
            ++i;
            continue;
        }
        if (!allowEquation && c == '=')
            return errorJson("INVALID_REQUEST", "equation is not valid here");
        if (std::isalpha(c) || c == '_') {
            const std::size_t begin = i++;
            while (i < length) {
                const unsigned char next =
                    static_cast<unsigned char>(expression[i]);
                if (!std::isalnum(next) && next != '_') break;
                ++i;
            }
            const std::string_view name(expression + begin, i - begin);
            std::size_t next = i;
            while (next < length &&
                   std::isspace(static_cast<unsigned char>(expression[next])))
                ++next;
            if (isReservedIdentifier(name))
                return errorJson("INVALID_REQUEST",
                                 "expression uses a forbidden identifier");
            if (next < length && expression[next] == '(' &&
                !isAllowedFunction(name))
                return errorJson("INVALID_REQUEST",
                                 "function is not in the controlled input contract");
            ++nodes;
        } else if (std::isdigit(c) ||
                   (c == '.' && i + 1 < length &&
                    std::isdigit(static_cast<unsigned char>(expression[i + 1])))) {
            ++i;
            while (i < length) {
                const unsigned char next =
                    static_cast<unsigned char>(expression[i]);
                if (!std::isdigit(next) && next != '.') break;
                ++i;
            }
            ++nodes;
        } else if (std::strchr("+-*/^=,<>!", c)) {
            ++nodes;
            ++i;
        } else if (std::isspace(c)) {
            ++i;
        } else {
            return errorJson("INVALID_REQUEST",
                             "expression contains an unsupported character");
        }
        if (nodes > enginecontract::kMaxTreeNodes)
            return errorJson("NODE_LIMIT", "expression exceeds node limit");
    }
    if (depth != 0) return errorJson("PARSE_ERROR", "unbalanced delimiters");
    return {};
}

std::string NumosMathSession::evaluate(const char* expression,
                                       bool approximate) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateExpression(expression); !error.empty()) return error;
    const auto start = Clock::now();
    const StructuredEngineResult result =
        _engine.evaluateStructured(expression);
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    _lastDiagnostic = result.base.diagnostic.substr(0, kMaxDiagnosticBytes);
    return structuredJson(result, approximate, _lastOperationMs);
}

std::string NumosMathSession::simplify(const char* expression) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateExpression(expression); !error.empty()) return error;
    const auto start = Clock::now();
    const auto result =
        _engine.transformStructured(AlgebraTransform::Simplify, expression);
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    _lastDiagnostic = result.base.diagnostic.substr(0, kMaxDiagnosticBytes);
    return structuredJson(result, false, _lastOperationMs);
}

std::string NumosMathSession::calculus(const char* expression,
                                       const char* variable, bool integrate) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateExpression(expression); !error.empty()) return error;
    if (auto error = validateIdentifier(variable, false); !error.empty())
        return error;
    CalculusRequest request;
    request.expression = expression;
    request.variable = variable;
    request.operation = integrate ? CalculusOperation::IntegrateIndefinite
                                  : CalculusOperation::Differentiate;
    const auto start = Clock::now();
    const auto result = _engine.evaluateCalculusStructured(request);
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    _lastDiagnostic = result.diagnostic.substr(0, kMaxDiagnosticBytes);
    return calculusJson(result, _lastOperationMs);
}

std::string NumosMathSession::solve(const char* lhsLines, const char* rhsLines,
                                    const char* variablesCsv,
                                    bool complexDomain) {
    if (auto error = requireReady(); !error.empty()) return error;
    const auto lhs = split(lhsLines, '\n');
    const auto rhs = split(rhsLines, '\n');
    const auto variables = split(variablesCsv, ',');
    if (lhs.empty() || lhs.size() != rhs.size() ||
        lhs.size() > kMaxSystemEquations || variables.empty() ||
        variables.size() > kMaxVariables)
        return errorJson("INVALID_REQUEST", "invalid bounded solve request");
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (auto error = validateExpression(lhs[i].c_str(), false);
            !error.empty()) return error;
        if (auto error = validateExpression(rhs[i].c_str(), false);
            !error.empty()) return error;
    }
    for (const auto& variable : variables)
        if (auto error = validateIdentifier(variable.c_str(), false);
            !error.empty()) return error;
    std::vector<SolveEquation> equations;
    for (std::size_t i = 0; i < lhs.size(); ++i)
        equations.push_back({lhs[i], rhs[i]});
    const SolveDomainPolicy policy =
        complexDomain ? SolveDomainPolicy::RealAndComplex
                      : SolveDomainPolicy::RealOnly;
    const auto start = Clock::now();
    const auto result = equations.size() == 1 && variables.size() == 1
        ? _engine.solveStructured(equations.front(), variables.front(), policy)
        : _engine.solveSystemStructured(equations, variables, policy);
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    _lastDiagnostic = result.diagnostic.substr(0, kMaxDiagnosticBytes);
    if (const char* code = statusCode(result.status))
        return errorJson(code, result.diagnostic);
    std::ostringstream out;
    const char* setKind = "unsupported";
    switch (result.setKind) {
        case SolutionSetKind::Solutions: setKind = "solutions"; break;
        case SolutionSetKind::NoSolution: setKind = "no_solution"; break;
        case SolutionSetKind::AllValues: setKind = "all_values"; break;
        case SolutionSetKind::Unsupported: break;
    }
    out << "\"result\":{\"kind\":\"solution_set\",\"setKind\":\""
        << setKind << "\",\"groups\":[";
    for (std::size_t i = 0; i < result.groups.size(); ++i) {
        if (i) out << ',';
        out << "{\"values\":[";
        for (std::size_t j = 0; j < result.groups[i].values.size(); ++j) {
            if (j) out << ',';
            const auto& solution = result.groups[i].values[j];
            out << "{\"variable\":\"" << jsonEscape(solution.variable, 31)
                << "\",\"value\":";
            appendNodeJson(out, solution.exactValue);
            out << '}';
        }
        out << "]}";
    }
    out << "]},\"operationMs\":" << std::fixed << std::setprecision(3)
        << _lastOperationMs;
    return okJson(out.str());
}

std::string NumosMathSession::makeHandleId(uint32_t generation,
                                           uint32_t serial) const {
    std::ostringstream out;
    out << 'h' << generation << '-' << serial;
    return out.str();
}

bool NumosMathSession::parseHandleId(const char* id, uint32_t& generation,
                                     uint32_t& serial) const {
    if (!id || id[0] != 'h') return false;
    char tail = 0;
    return std::sscanf(id, "h%u-%u%c", &generation, &serial, &tail) == 2;
}

NumosMathSession::HandleEntry* NumosMathSession::findHandle(
    const char* id, uint8_t dimensions, std::string& error) {
    uint32_t generation = 0, serial = 0;
    if (!parseHandleId(id, generation, serial)) {
        error = errorJson("STALE_HANDLE", "invalid opaque handle");
        return nullptr;
    }
    if (generation != _handleGeneration) {
        error = errorJson("STALE_HANDLE", "handle generation is stale");
        return nullptr;
    }
    const auto found = _handles.find(serial);
    if (found == _handles.end()) {
        error = errorJson("STALE_HANDLE", "unknown opaque handle");
        return nullptr;
    }
    if (found->second.state == HandleState::Disposed) {
        error = errorJson("DISPOSED_HANDLE", "handle has been disposed");
        return nullptr;
    }
    if (found->second.dimensions != dimensions) {
        error = errorJson("INVALID_REQUEST", "handle dimension mismatch");
        return nullptr;
    }
    if (!found->second.expression.valid()) {
        error = errorJson("STALE_HANDLE", "native retained expression is stale");
        return nullptr;
    }
    return &found->second;
}

std::string NumosMathSession::compile1D(const char* expression,
                                        const char* variable) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateExpression(expression); !error.empty()) return error;
    if (auto error = validateIdentifier(variable, false); !error.empty())
        return error;
    const auto start = Clock::now();
    CompiledExpression compiled =
        _engine.compileNumeric(expression, variable, false);
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (!compiled.valid())
        return errorJson("PARSE_ERROR", compiled.diagnostic());
    const uint32_t serial = _nextHandleSerial++;
    HandleEntry entry;
    entry.generation = _handleGeneration;
    entry.serial = serial;
    entry.dimensions = 1;
    entry.expression = std::move(compiled);
    _handles.emplace(serial, std::move(entry));
    std::ostringstream body;
    body << "\"handle\":\""
         << makeHandleId(_handleGeneration, serial)
         << "\",\"dimensions\":1,\"operationMs\":" << std::fixed
         << std::setprecision(3) << _lastOperationMs;
    return okJson(body.str());
}

std::string NumosMathSession::compile2D(const char* expression,
                                        const char* variableA,
                                        const char* variableB) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateExpression(expression); !error.empty()) return error;
    if (auto error = validateIdentifier(variableA, false); !error.empty())
        return error;
    if (auto error = validateIdentifier(variableB, false); !error.empty())
        return error;
    if (std::strcmp(variableA, variableB) == 0)
        return errorJson("INVALID_REQUEST", "2D variables must be distinct");
    const auto start = Clock::now();
    CompiledExpression compiled =
        _engine.compileNumeric2D(expression, variableA, variableB);
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (!compiled.valid())
        return errorJson("PARSE_ERROR", compiled.diagnostic());
    const uint32_t serial = _nextHandleSerial++;
    HandleEntry entry;
    entry.generation = _handleGeneration;
    entry.serial = serial;
    entry.dimensions = 2;
    entry.expression = std::move(compiled);
    _handles.emplace(serial, std::move(entry));
    std::ostringstream body;
    body << "\"handle\":\""
         << makeHandleId(_handleGeneration, serial)
         << "\",\"dimensions\":2,\"operationMs\":" << std::fixed
         << std::setprecision(3) << _lastOperationMs;
    return okJson(body.str());
}

std::string NumosMathSession::evaluate1D(const char* handle, double value) {
    if (auto error = requireReady(); !error.empty()) return error;
    std::string error;
    HandleEntry* entry = findHandle(handle, 1, error);
    if (!entry) return error;
    double result = std::numeric_limits<double>::quiet_NaN();
    const bool valid = _engine.evaluateNumeric(entry->expression, value, result);
    return okJson("\"value\":" + numberJson(result, valid));
}

std::string NumosMathSession::evaluateMany1D(
    const char* handle, const double* values, int count) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (!values || count < 0 || count > kMaxBatchValues)
        return errorJson("INVALID_REQUEST", "batch exceeds bounded size");
    std::string error;
    HandleEntry* entry = findHandle(handle, 1, error);
    if (!entry) return error;
    const auto start = Clock::now();
    std::ostringstream body;
    body << "\"values\":[";
    for (int i = 0; i < count; ++i) {
        if (i) body << ',';
        double result = std::numeric_limits<double>::quiet_NaN();
        const bool valid =
            _engine.evaluateNumeric(entry->expression, values[i], result);
        body << numberJson(result, valid);
    }
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    body << "],\"operationMs\":" << std::fixed << std::setprecision(3)
         << _lastOperationMs;
    return okJson(body.str());
}

std::string NumosMathSession::evaluate2D(const char* handle, double x,
                                         double y) {
    if (auto error = requireReady(); !error.empty()) return error;
    std::string error;
    HandleEntry* entry = findHandle(handle, 2, error);
    if (!entry) return error;
    double result = std::numeric_limits<double>::quiet_NaN();
    const bool valid =
        _engine.evaluateNumeric2D(entry->expression, x, y, result);
    return okJson("\"value\":" + numberJson(result, valid));
}

std::string NumosMathSession::evaluateGrid2D(
    const char* handle, double xMin, double xMax, double yMin, double yMax,
    int width, int height) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (width < 1 || height < 1 || width > kMaxGridWidth ||
        height > kMaxGridHeight || width * height > kMaxGridCells ||
        !std::isfinite(xMin) || !std::isfinite(xMax) ||
        !std::isfinite(yMin) || !std::isfinite(yMax) ||
        xMin > xMax || yMin > yMax)
        return errorJson("INVALID_REQUEST", "grid exceeds bounded dimensions");
    std::string error;
    HandleEntry* entry = findHandle(handle, 2, error);
    if (!entry) return error;
    const auto start = Clock::now();
    std::ostringstream body;
    body << "\"width\":" << width << ",\"height\":" << height
         << ",\"order\":\"row-major-y-then-x\",\"values\":[";
    for (int row = 0; row < height; ++row) {
        const double y = height == 1 ? yMin :
            yMin + (yMax - yMin) * row / static_cast<double>(height - 1);
        for (int column = 0; column < width; ++column) {
            if (row || column) body << ',';
            const double x = width == 1 ? xMin :
                xMin + (xMax - xMin) * column /
                    static_cast<double>(width - 1);
            double result = std::numeric_limits<double>::quiet_NaN();
            const bool valid =
                _engine.evaluateNumeric2D(entry->expression, x, y, result);
            body << numberJson(result, valid);
        }
    }
    _lastOperationMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    body << "],\"operationMs\":" << std::fixed << std::setprecision(3)
         << _lastOperationMs;
    return okJson(body.str());
}

std::string NumosMathSession::disposeHandle(const char* handle) {
    if (auto error = requireReady(); !error.empty()) return error;
    std::string error;
    uint32_t generation = 0, serial = 0;
    if (!parseHandleId(handle, generation, serial) ||
        generation != _handleGeneration)
        return errorJson("STALE_HANDLE", "handle generation is stale");
    const auto found = _handles.find(serial);
    if (found == _handles.end())
        return errorJson("STALE_HANDLE", "unknown opaque handle");
    if (found->second.state == HandleState::Disposed)
        return errorJson("DISPOSED_HANDLE", "handle has been disposed");
    found->second.expression = CompiledExpression();
    found->second.state = HandleState::Disposed;
    return okJson("\"disposed\":true");
}

std::string NumosMathSession::rebuildContext(bool bumpHandleGeneration) {
    _handles.clear();
    if (bumpHandleGeneration) ++_handleGeneration;
    _engine.reset();
    for (const auto& [name, value] : _variables) {
        const auto assigned = _engine.assign(name.c_str(), value.c_str());
        if (!assigned.ok())
            return errorJson("INTERNAL_ERROR",
                             "could not restore controlled variable");
    }
    return {};
}

std::string NumosMathSession::setAngleMode(const char* mode) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (!mode || (std::strcmp(mode, "radian") != 0 &&
                  std::strcmp(mode, "degree") != 0))
        return errorJson("INVALID_ANGLE_MODE",
                         "angle mode must be radian or degree");
    const auto target = std::strcmp(mode, "degree") == 0
        ? vpam::AngleMode::DEG : vpam::AngleMode::RAD;
    if (numos::angleMode() != target) {
        numos::setAngleMode(target);
        if (auto error = rebuildContext(true); !error.empty()) return error;
    }
    return getAngleMode();
}

std::string NumosMathSession::getAngleMode() const {
    if (auto error = requireReady(); !error.empty()) return error;
    return okJson(numos::angleModeIsDeg()
        ? "\"mode\":\"degree\"" : "\"mode\":\"radian\"");
}

std::string NumosMathSession::setVariable(const char* name,
                                          const char* expression) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateIdentifier(name, true); !error.empty()) return error;
    if (auto error = validateExpression(expression, false); !error.empty())
        return error;
    const auto value = _engine.evaluateStructured(expression);
    if (const char* code = statusCode(value.base.status))
        return errorJson(code, value.base.diagnostic);
    if (!value.hasTree || value.tree.kind == EngineNodeKind::Undefined ||
        value.tree.kind == EngineNodeKind::Equation ||
        value.tree.kind == EngineNodeKind::Assignment ||
        value.tree.kind == EngineNodeKind::List ||
        value.tree.kind == EngineNodeKind::Set ||
        value.tree.kind == EngineNodeKind::Matrix ||
        value.tree.kind == EngineNodeKind::Interval ||
        value.tree.kind == EngineNodeKind::Piecewise ||
        value.tree.kind == EngineNodeKind::Unsupported)
        return errorJson("UNSUPPORTED_RESULT",
                         "variable value must be an exact scalar expression");
    _variables[name] = value.base.exactText;
    if (auto error = rebuildContext(true); !error.empty()) return error;
    std::ostringstream body;
    body << "\"variable\":{\"name\":\"" << jsonEscape(name, 31)
         << "\",\"value\":";
    appendNodeJson(body, value.tree);
    body << '}';
    return okJson(body.str());
}

std::string NumosMathSession::removeVariable(const char* name) {
    if (auto error = requireReady(); !error.empty()) return error;
    if (auto error = validateIdentifier(name, true); !error.empty()) return error;
    const bool removed = _variables.erase(name) != 0;
    if (removed)
        if (auto error = rebuildContext(true); !error.empty()) return error;
    return okJson(removed ? "\"removed\":true" : "\"removed\":false");
}

std::string NumosMathSession::clearVariables() {
    if (auto error = requireReady(); !error.empty()) return error;
    _variables.clear();
    if (auto error = rebuildContext(true); !error.empty()) return error;
    return okJson("\"cleared\":true");
}

std::string NumosMathSession::reset() {
    if (auto error = requireReady(); !error.empty()) return error;
    _variables.clear();
    _lastDiagnostic.clear();
    _lastOperationMs = 0.0;
    if (auto error = rebuildContext(true); !error.empty()) return error;
    return okJson("\"reset\":true");
}

std::string NumosMathSession::diagnostics() const {
    if (auto error = requireReady(); !error.empty()) return error;
    std::ostringstream body;
    body << "\"diagnostics\":{\"angleMode\":\""
         << (numos::angleModeIsDeg() ? "degree" : "radian")
         << "\",\"handleGeneration\":" << _handleGeneration
         << ",\"liveHandles\":";
    std::size_t live = 0;
    for (const auto& [serial, handle] : _handles)
        if (handle.state == HandleState::Live) ++live;
    body << live << ",\"variables\":[";
    bool first = true;
    for (const auto& [name, value] : _variables) {
        if (!first) body << ',';
        first = false;
        body << "{\"name\":\"" << jsonEscape(name, 31)
             << "\",\"exact\":\"" << jsonEscape(value, kMaxDisplayBytes)
             << "\"}";
    }
    body << "],\"lastOperationMs\":" << std::fixed << std::setprecision(3)
         << _lastOperationMs
         << ",\"timeBudget\":\"soft-observation-only\"";
#ifdef __EMSCRIPTEN__
    const struct mallinfo memory = mallinfo();
    body << ",\"heapBytes\":" << emscripten_get_heap_size()
         << ",\"usedHeapBytes\":" << memory.uordblks;
#endif
#ifdef NATIVE_SIM
    const auto giac = _engine.runtimeDiagnostics();
    body << ",\"giac\":{\"contextsCreated\":" << giac.contextsCreated
         << ",\"contextsDestroyed\":" << giac.contextsDestroyed
         << ",\"activeContexts\":" << giac.activeContexts
         << ",\"retainedCompiles\":" << giac.retainedCompiles
         << ",\"numericSamples\":" << giac.numericSamples
         << ",\"liveRetainedHandles\":" << giac.liveRetainedHandles
         << ",\"generation\":" << giac.generation << '}';
#endif
    body << '}';
    return okJson(body.str());
}

std::string NumosMathSession::shutdown() {
    if (_lifecycle == Lifecycle::Shutdown)
        return errorJson("SHUTDOWN", "session has already been shut down");
    _handles.clear();
    _variables.clear();
    _lastDiagnostic.clear();
    _lastOperationMs = 0.0;
#ifdef NATIVE_SIM
    _engine.shutdown();
    const auto giac = _engine.runtimeDiagnostics();
#endif
    ++_handleGeneration;
    _lifecycle = Lifecycle::Shutdown;
    std::ostringstream body;
    body << "\"state\":\"shutdown\"";
#ifdef NATIVE_SIM
    body << ",\"activeContexts\":" << giac.activeContexts;
#endif
    return okJson(body.str());
}

} // namespace numos
