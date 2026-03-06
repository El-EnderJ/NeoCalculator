/**
 * PythonEngine.cpp — MicroPython Wrapper / Mock Engine
 *
 * Simulates basic Python execution for the embedded IDE.
 * Handles: print(), for loops, variable assignment, math expressions.
 * Allocates its working heap from PSRAM on ESP32-S3.
 */

#include "PythonEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <cstring>

#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

// ── Simple variable storage for the mock interpreter ──
static constexpr int MAX_VARS = 32;
static struct { char name[32]; double value; bool used; } s_vars[MAX_VARS];
static int s_varCount = 0;

static void resetVars() {
    s_varCount = 0;
    for (int i = 0; i < MAX_VARS; ++i) s_vars[i].used = false;
}

static double* findVar(const char* name) {
    for (int i = 0; i < s_varCount; ++i) {
        if (s_vars[i].used && strcmp(s_vars[i].name, name) == 0)
            return &s_vars[i].value;
    }
    return nullptr;
}

static bool setVar(const char* name, double value) {
    // Update existing
    for (int i = 0; i < s_varCount; ++i) {
        if (s_vars[i].used && strcmp(s_vars[i].name, name) == 0) {
            s_vars[i].value = value;
            return true;
        }
    }
    // Create new
    if (s_varCount < MAX_VARS) {
        s_vars[s_varCount].used = true;
        strncpy(s_vars[s_varCount].name, name, 31);
        s_vars[s_varCount].name[31] = '\0';
        s_vars[s_varCount].value = value;
        s_varCount++;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

PythonEngine::PythonEngine()
    : _outPos(0), _errPos(0), _heap(nullptr), _initialized(false)
{
    _outBuf[0] = '\0';
    _errBuf[0] = '\0';
}

PythonEngine::~PythonEngine() {
    deinit();
}

void PythonEngine::init() {
    if (_initialized) return;

#ifdef ARDUINO
    _heap = heap_caps_malloc(HEAP_SIZE, MALLOC_CAP_SPIRAM);
    if (!_heap) {
        // Fallback: try smaller allocation
        _heap = heap_caps_malloc(256 * 1024, MALLOC_CAP_SPIRAM);
    }
#else
    _heap = malloc(HEAP_SIZE);
#endif

    resetVars();
    _initialized = true;
}

void PythonEngine::deinit() {
    if (!_initialized) return;

    if (_heap) {
#ifdef ARDUINO
        free(_heap);
#else
        free(_heap);
#endif
        _heap = nullptr;
    }
    resetVars();
    _initialized = false;
}

void PythonEngine::clearOutput() {
    _outPos = 0;
    _outBuf[0] = '\0';
    _errPos = 0;
    _errBuf[0] = '\0';
}

void PythonEngine::appendOut(const char* text) {
    int len = strlen(text);
    int space = OUT_BUF_SIZE - _outPos - 1;
    if (space <= 0) return;
    int n = (len < space) ? len : space;
    memcpy(_outBuf + _outPos, text, n);
    _outPos += n;
    _outBuf[_outPos] = '\0';
}

void PythonEngine::appendErr(const char* text) {
    int len = strlen(text);
    int space = ERR_BUF_SIZE - _errPos - 1;
    if (space <= 0) return;
    int n = (len < space) ? len : space;
    memcpy(_errBuf + _errPos, text, n);
    _errPos += n;
    _errBuf[_errPos] = '\0';
}

bool PythonEngine::isIdentifier(const char* s, int len) {
    if (len <= 0) return false;
    if (!isalpha((unsigned char)s[0]) && s[0] != '_') return false;
    for (int i = 1; i < len; ++i) {
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Simple expression evaluator
// ═══════════════════════════════════════════════════════════════

bool PythonEngine::evalExpr(const char* expr, double& result) {
    // Skip whitespace
    while (*expr && isspace((unsigned char)*expr)) expr++;
    if (!*expr) return false;

    // Try to parse as a number
    char* end = nullptr;
    double val = strtod(expr, &end);
    if (end != expr && (*end == '\0' || isspace((unsigned char)*end))) {
        result = val;
        return true;
    }

    // Try to find as a variable
    int len = strlen(expr);
    // Trim trailing whitespace
    while (len > 0 && isspace((unsigned char)expr[len - 1])) len--;

    char trimmed[64];
    if (len >= (int)sizeof(trimmed)) len = sizeof(trimmed) - 1;
    memcpy(trimmed, expr, len);
    trimmed[len] = '\0';

    // Check for simple math: a + b, a - b, a * b, a / b, a ** b
    // Find the last operator outside parentheses
    for (int op = 0; op < 4; ++op) {
        char opChar = "+-*/"[op];
        int depth = 0;
        for (int i = len - 1; i > 0; --i) {
            if (trimmed[i] == ')') depth++;
            if (trimmed[i] == '(') depth--;
            if (depth == 0 && trimmed[i] == opChar) {
                // Skip ** (power)
                if (opChar == '*' && i > 0 && trimmed[i - 1] == '*') continue;
                char left[64], right[64];
                memcpy(left, trimmed, i);
                left[i] = '\0';
                strcpy(right, trimmed + i + 1);
                double lv, rv;
                if (evalExpr(left, lv) && evalExpr(right, rv)) {
                    switch (opChar) {
                        case '+': result = lv + rv; return true;
                        case '-': result = lv - rv; return true;
                        case '*': result = lv * rv; return true;
                        case '/':
                            if (rv == 0) { appendErr("ZeroDivisionError\n"); return false; }
                            result = lv / rv; return true;
                    }
                }
            }
        }
    }

    // Power operator **
    {
        const char* pp = strstr(trimmed, "**");
        if (pp) {
            int idx = pp - trimmed;
            char left[64], right[64];
            memcpy(left, trimmed, idx);
            left[idx] = '\0';
            strcpy(right, trimmed + idx + 2);
            double lv, rv;
            if (evalExpr(left, lv) && evalExpr(right, rv)) {
                result = pow(lv, rv);
                return true;
            }
        }
    }

    // Built-in functions
    if (strncmp(trimmed, "abs(", 4) == 0 && trimmed[len - 1] == ')') {
        char inner[64];
        memcpy(inner, trimmed + 4, len - 5);
        inner[len - 5] = '\0';
        double v;
        if (evalExpr(inner, v)) { result = fabs(v); return true; }
    }
    if (strncmp(trimmed, "int(", 4) == 0 && trimmed[len - 1] == ')') {
        char inner[64];
        memcpy(inner, trimmed + 4, len - 5);
        inner[len - 5] = '\0';
        double v;
        if (evalExpr(inner, v)) { result = (double)(int)v; return true; }
    }

    // math.sqrt(), math.sin(), math.cos(), math.pi, math.e
    if (strcmp(trimmed, "math.pi") == 0) { result = M_PI; return true; }
    if (strcmp(trimmed, "math.e") == 0) { result = M_E; return true; }
    if (strncmp(trimmed, "math.sqrt(", 10) == 0 && trimmed[len - 1] == ')') {
        char inner[64];
        memcpy(inner, trimmed + 10, len - 11);
        inner[len - 11] = '\0';
        double v;
        if (evalExpr(inner, v)) { result = sqrt(v); return true; }
    }
    if (strncmp(trimmed, "math.sin(", 9) == 0 && trimmed[len - 1] == ')') {
        char inner[64];
        memcpy(inner, trimmed + 9, len - 10);
        inner[len - 10] = '\0';
        double v;
        if (evalExpr(inner, v)) { result = sin(v); return true; }
    }
    if (strncmp(trimmed, "math.cos(", 9) == 0 && trimmed[len - 1] == ')') {
        char inner[64];
        memcpy(inner, trimmed + 9, len - 10);
        inner[len - 10] = '\0';
        double v;
        if (evalExpr(inner, v)) { result = cos(v); return true; }
    }

    // Variable lookup
    double* vp = findVar(trimmed);
    if (vp) { result = *vp; return true; }

    // String literal (for print)
    if (len >= 2 && ((trimmed[0] == '"' && trimmed[len - 1] == '"') ||
                     (trimmed[0] == '\'' && trimmed[len - 1] == '\''))) {
        // Not a numeric expression
        return false;
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════
// Line interpreter
// ═══════════════════════════════════════════════════════════════

bool PythonEngine::interpretLine(const char* line) {
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line)) line++;
    if (!*line || line[0] == '#') return true;  // Empty line or comment

    // import statement (just acknowledge)
    if (strncmp(line, "import ", 7) == 0) return true;
    if (strncmp(line, "from ", 5) == 0) return true;

    // print(...)
    if (strncmp(line, "print(", 6) == 0) {
        int len = strlen(line);
        if (line[len - 1] != ')') {
            appendErr("SyntaxError: missing closing parenthesis\n");
            return false;
        }
        // Extract content inside print(...)
        char content[256];
        int clen = len - 7;  // minus "print(" and ")"
        if (clen < 0) clen = 0;
        if (clen >= (int)sizeof(content)) clen = sizeof(content) - 1;
        memcpy(content, line + 6, clen);
        content[clen] = '\0';

        // Check if it's a string literal
        int cl = strlen(content);
        if (cl >= 2 && ((content[0] == '"' && content[cl - 1] == '"') ||
                        (content[0] == '\'' && content[cl - 1] == '\''))) {
            char str[256];
            memcpy(str, content + 1, cl - 2);
            str[cl - 2] = '\0';
            appendOut(str);
            appendOut("\n");
            return true;
        }

        // Try format string: f"..." or f'...'
        if (cl >= 3 && content[0] == 'f' &&
            ((content[1] == '"' && content[cl - 1] == '"') ||
             (content[1] == '\'' && content[cl - 1] == '\''))) {
            // Simple f-string: just output the template
            char str[256];
            memcpy(str, content + 2, cl - 3);
            str[cl - 3] = '\0';
            // Replace {var} with variable values
            char output[256];
            int oi = 0;
            for (int i = 0; str[i] && oi < 254; ++i) {
                if (str[i] == '{') {
                    int j = i + 1;
                    while (str[j] && str[j] != '}') j++;
                    if (str[j] == '}') {
                        char varName[64];
                        int vl = j - i - 1;
                        if (vl >= (int)sizeof(varName)) vl = sizeof(varName) - 1;
                        memcpy(varName, str + i + 1, vl);
                        varName[vl] = '\0';
                        double val;
                        if (evalExpr(varName, val)) {
                            char numBuf[32];
                            if (val == (int)val)
                                snprintf(numBuf, sizeof(numBuf), "%d", (int)val);
                            else
                                snprintf(numBuf, sizeof(numBuf), "%.6g", val);
                            int nl = strlen(numBuf);
                            for (int k = 0; k < nl && oi < 254; ++k)
                                output[oi++] = numBuf[k];
                        } else {
                            // Can't evaluate - put placeholder
                            output[oi++] = '?';
                        }
                        i = j;  // skip past '}'
                        continue;
                    }
                }
                output[oi++] = str[i];
            }
            output[oi] = '\0';
            appendOut(output);
            appendOut("\n");
            return true;
        }

        // Try to evaluate as expression
        double val;
        if (evalExpr(content, val)) {
            char buf[64];
            if (val == (int)val)
                snprintf(buf, sizeof(buf), "%d", (int)val);
            else
                snprintf(buf, sizeof(buf), "%.6g", val);
            appendOut(buf);
            appendOut("\n");
            return true;
        }

        // Fall back: just print the content
        appendOut(content);
        appendOut("\n");
        return true;
    }

    // Variable assignment: x = expr
    {
        const char* eq = strchr(line, '=');
        if (eq && eq != line && *(eq - 1) != '!' && *(eq - 1) != '<' &&
            *(eq - 1) != '>' && *(eq + 1) != '=') {
            char varName[64];
            int nLen = eq - line;
            // Trim trailing whitespace from variable name
            while (nLen > 0 && isspace((unsigned char)line[nLen - 1])) nLen--;
            if (nLen > 0 && nLen < (int)sizeof(varName)) {
                memcpy(varName, line, nLen);
                varName[nLen] = '\0';
                if (isIdentifier(varName, nLen)) {
                    const char* valStr = eq + 1;
                    while (*valStr && isspace((unsigned char)*valStr)) valStr++;

                    // Check if value is a string
                    int vl = strlen(valStr);
                    if (vl >= 2 && ((valStr[0] == '"' && valStr[vl - 1] == '"') ||
                                    (valStr[0] == '\'' && valStr[vl - 1] == '\''))) {
                        // String variable - store as 0 but acknowledge
                        setVar(varName, 0);
                        return true;
                    }

                    double val;
                    if (evalExpr(valStr, val)) {
                        setVar(varName, val);
                        return true;
                    }
                    // Assignment with unknown expression - still set to 0
                    setVar(varName, 0);
                    return true;
                }
            }
        }
    }

    // def/class (just acknowledge)
    if (strncmp(line, "def ", 4) == 0) return true;
    if (strncmp(line, "class ", 6) == 0) return true;
    if (strncmp(line, "if ", 3) == 0 || strncmp(line, "elif ", 5) == 0 ||
        strncmp(line, "else:", 5) == 0) return true;
    if (strncmp(line, "while ", 6) == 0) return true;
    if (strncmp(line, "return", 6) == 0) return true;
    if (strncmp(line, "pass", 4) == 0) return true;
    if (strncmp(line, "break", 5) == 0) return true;
    if (strncmp(line, "continue", 8) == 0) return true;

    // for i in range(n): — execute the loop
    if (strncmp(line, "for ", 4) == 0) return true;  // Handled at block level

    return true;  // Unknown line - accept silently
}

// ═══════════════════════════════════════════════════════════════
// Main execution entry point
// ═══════════════════════════════════════════════════════════════

bool PythonEngine::execute(const char* code) {
    if (!_initialized) {
        appendErr("RuntimeError: Engine not initialized\n");
        return false;
    }

    clearOutput();
    resetVars();

    if (!code || !*code) return true;

    // Split into lines and interpret
    // Handle for loops with simple range()
    char lineBuf[512];
    const char* p = code;
    bool inForLoop = false;
    char forVar[32] = {};
    int forStart = 0, forEnd = 0, forStep = 1;
    char forBody[1024] = {};
    int forBodyLen = 0;
    int indentLevel = 0;

    while (*p) {
        // Read one line
        int i = 0;
        while (*p && *p != '\n' && i < (int)sizeof(lineBuf) - 1) {
            lineBuf[i++] = *p++;
        }
        lineBuf[i] = '\0';
        if (*p == '\n') p++;

        // Count leading spaces
        int indent = 0;
        const char* lp = lineBuf;
        while (*lp == ' ') { indent++; lp++; }

        if (inForLoop) {
            if (indent > indentLevel || (*lp && indent == indentLevel && (*lp == ' ' || *lp == '\t'))) {
                // Part of the for loop body
                if (forBodyLen + i + 2 < (int)sizeof(forBody)) {
                    if (forBodyLen > 0) forBody[forBodyLen++] = '\n';
                    strcpy(forBody + forBodyLen, lp);
                    forBodyLen += strlen(lp);
                }
                continue;
            } else {
                // End of for loop — execute it
                for (int idx = forStart; idx < forEnd; idx += forStep) {
                    setVar(forVar, (double)idx);
                    // Execute body lines
                    char bodyLine[512];
                    const char* bp = forBody;
                    while (*bp) {
                        int bi = 0;
                        while (*bp && *bp != '\n' && bi < (int)sizeof(bodyLine) - 1) {
                            bodyLine[bi++] = *bp++;
                        }
                        bodyLine[bi] = '\0';
                        if (*bp == '\n') bp++;
                        // Skip leading whitespace
                        const char* blp = bodyLine;
                        while (*blp == ' ' || *blp == '\t') blp++;
                        if (!interpretLine(blp)) return false;
                    }
                }
                inForLoop = false;
                forBodyLen = 0;
                // Fall through to process current line
            }
        }

        // Skip empty lines
        if (!*lp) continue;

        // Check for "for VAR in range(...):"
        if (strncmp(lp, "for ", 4) == 0) {
            // Parse: for VAR in range(N): or for VAR in range(start, end):
            char varN[32];
            int n1 = 0, n2 = 0;
            if (sscanf(lp, "for %31s in range(%d, %d):", varN, &n1, &n2) == 3) {
                strncpy(forVar, varN, sizeof(forVar) - 1);
                forStart = n1;
                forEnd = n2;
                forStep = 1;
                inForLoop = true;
                indentLevel = indent;
                forBodyLen = 0;
                forBody[0] = '\0';
                continue;
            }
            if (sscanf(lp, "for %31s in range(%d):", varN, &n1) == 2) {
                strncpy(forVar, varN, sizeof(forVar) - 1);
                forStart = 0;
                forEnd = n1;
                forStep = 1;
                inForLoop = true;
                indentLevel = indent;
                forBodyLen = 0;
                forBody[0] = '\0';
                continue;
            }
        }

        if (!interpretLine(lp)) return false;
    }

    // Execute remaining for loop body if file ends while in loop
    if (inForLoop) {
        for (int idx = forStart; idx < forEnd; idx += forStep) {
            setVar(forVar, (double)idx);
            char bodyLine[512];
            const char* bp = forBody;
            while (*bp) {
                int bi = 0;
                while (*bp && *bp != '\n' && bi < (int)sizeof(bodyLine) - 1) {
                    bodyLine[bi++] = *bp++;
                }
                bodyLine[bi] = '\0';
                if (*bp == '\n') bp++;
                const char* blp = bodyLine;
                while (*blp == ' ' || *blp == '\t') blp++;
                if (!interpretLine(blp)) return false;
            }
        }
    }

    return true;
}
