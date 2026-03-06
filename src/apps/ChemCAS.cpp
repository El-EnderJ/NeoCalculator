/**
 * ChemCAS.cpp — Chemistry CAS implementation for NumOS.
 *
 * Molar mass parser + equation balancer with exact rational arithmetic.
 */

#include "ChemCAS.h"
#include "ChemDatabase.h"
#include <cmath>
#include <cstdio>
#include <cctype>
#include <climits>

namespace chem {

// ════════════════════════════════════════════════════════════════════════════
// GCD / LCM helpers
// ════════════════════════════════════════════════════════════════════════════

int32_t gcd(int32_t a, int32_t b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { int32_t t = b; b = a % b; a = t; }
    return a ? a : 1;
}

int32_t lcm(int32_t a, int32_t b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (a == 0 || b == 0) return 0;
    return (a / gcd(a, b)) * b;
}

// ════════════════════════════════════════════════════════════════════════════
// RationalNumber implementation
// ════════════════════════════════════════════════════════════════════════════

void RationalNumber::simplify() {
    if (den < 0) { num = -num; den = -den; }
    if (num == 0) { den = 1; return; }
    int32_t g = gcd(num < 0 ? -num : num, den);
    num /= g;
    den /= g;
}

RationalNumber RationalNumber::operator+(const RationalNumber& o) const {
    RationalNumber r(num * o.den + o.num * den, den * o.den);
    r.simplify();
    return r;
}

RationalNumber RationalNumber::operator-(const RationalNumber& o) const {
    RationalNumber r(num * o.den - o.num * den, den * o.den);
    r.simplify();
    return r;
}

RationalNumber RationalNumber::operator*(const RationalNumber& o) const {
    RationalNumber r(num * o.num, den * o.den);
    r.simplify();
    return r;
}

RationalNumber RationalNumber::operator/(const RationalNumber& o) const {
    RationalNumber r(num * o.den, den * o.num);
    r.simplify();
    return r;
}

bool RationalNumber::operator==(const RationalNumber& o) const {
    return num * o.den == o.num * den;
}

// ════════════════════════════════════════════════════════════════════════════
// Element lookup helper
// ════════════════════════════════════════════════════════════════════════════

static int findElementBySymbol(const char* sym, int len) {
    for (int i = 0; i < 118; ++i) {
        const char* es = ELEMENTS[i].symbol;
        int elen = static_cast<int>(strlen(es));
        if (elen == len && strncmp(es, sym, len) == 0) {
            return i;  // index into ELEMENTS
        }
    }
    return -1;
}

// ════════════════════════════════════════════════════════════════════════════
// Molar Mass Parser (recursive descent)
// ════════════════════════════════════════════════════════════════════════════

// Forward declaration
static float parseGroup(const char* s, int& pos, int len);

static int parseNumber(const char* s, int& pos, int len) {
    if (pos >= len || !isdigit((unsigned char)s[pos])) return 1;
    int n = 0;
    while (pos < len && isdigit((unsigned char)s[pos])) {
        int digit = s[pos] - '0';
        if (n > (INT32_MAX - digit) / 10) return n > 0 ? n : 1; // overflow guard
        n = n * 10 + digit;
        ++pos;
    }
    return n > 0 ? n : 1;
}

// Parse an element symbol (uppercase + optional lowercase)
static float parseElement(const char* s, int& pos, int len) {
    if (pos >= len || !isupper((unsigned char)s[pos])) return -1.0f;
    int start = pos;
    ++pos;
    while (pos < len && islower((unsigned char)s[pos])) ++pos;

    int symLen = pos - start;
    if (symLen > 3) return -1.0f;

    int idx = findElementBySymbol(s + start, symLen);
    if (idx < 0) return -1.0f;

    int count = parseNumber(s, pos, len);
    return ELEMENTS[idx].mass * count;
}

static float parseGroup(const char* s, int& pos, int len) {
    float total = 0.0f;
    while (pos < len) {
        if (s[pos] == '(') {
            ++pos;
            float inner = parseGroup(s, pos, len);
            if (inner < 0.0f) return -1.0f;
            if (pos < len && s[pos] == ')') ++pos;
            else return -1.0f;
            int count = parseNumber(s, pos, len);
            total += inner * count;
        } else if (s[pos] == ')') {
            break;
        } else if (s[pos] == '*' || s[pos] == '.') {
            // Hydrate notation: CuSO4*5H2O
            ++pos;
            int coeff = parseNumber(s, pos, len);
            float rest = parseGroup(s, pos, len);
            if (rest < 0.0f) return -1.0f;
            total += coeff * rest;
        } else if (isupper((unsigned char)s[pos])) {
            float m = parseElement(s, pos, len);
            if (m < 0.0f) return -1.0f;
            total += m;
        } else {
            break;
        }
    }
    return total;
}

float parseMolarMass(const char* formula) {
    if (!formula || formula[0] == '\0') return -1.0f;
    int len = static_cast<int>(strlen(formula));
    int pos = 0;

    // Handle leading coefficient (e.g., "2H2O")
    int coeff = 1;
    if (isdigit((unsigned char)formula[0]) && len > 1) {
        bool hasUpper = false;
        for (int i = 1; i < len; ++i) {
            if (isupper((unsigned char)formula[i])) { hasUpper = true; break; }
        }
        if (hasUpper) {
            coeff = parseNumber(formula, pos, len);
        }
    }

    float mass = parseGroup(formula, pos, len);
    if (mass < 0.0f) return -1.0f;
    return mass * coeff;
}

// ════════════════════════════════════════════════════════════════════════════
// Equation Balancer
// ════════════════════════════════════════════════════════════════════════════

// Parse a formula into element counts: elemIndex[] → count
struct FormulaComposition {
    int elemIdx[MAX_UNIQUE_ELEMS];
    int count[MAX_UNIQUE_ELEMS];
    int numElems;

    FormulaComposition() : numElems(0) {
        memset(elemIdx, 0, sizeof(elemIdx));
        memset(count, 0, sizeof(count));
    }

    void addElement(int idx, int cnt) {
        for (int i = 0; i < numElems; ++i) {
            if (elemIdx[i] == idx) {
                count[i] += cnt;
                return;
            }
        }
        if (numElems < MAX_UNIQUE_ELEMS) {
            elemIdx[numElems] = idx;
            count[numElems] = cnt;
            ++numElems;
        }
    }
};

// Forward
static bool parseFormulaComp(const char* s, int& pos, int len,
                              FormulaComposition& comp, int multiplier);

static bool parseCompGroup(const char* s, int& pos, int len,
                            FormulaComposition& comp, int multiplier) {
    while (pos < len) {
        if (s[pos] == '(') {
            ++pos;
            FormulaComposition inner;
            if (!parseCompGroup(s, pos, len, inner, 1)) return false;
            if (pos < len && s[pos] == ')') ++pos;
            else return false;
            int cnt = parseNumber(s, pos, len);
            for (int i = 0; i < inner.numElems; ++i) {
                comp.addElement(inner.elemIdx[i], inner.count[i] * cnt * multiplier);
            }
        } else if (s[pos] == ')') {
            break;
        } else if (isupper((unsigned char)s[pos])) {
            int start = pos;
            ++pos;
            while (pos < len && islower((unsigned char)s[pos])) ++pos;
            int symLen = pos - start;
            int idx = findElementBySymbol(s + start, symLen);
            if (idx < 0) return false;
            int cnt = parseNumber(s, pos, len);
            comp.addElement(idx, cnt * multiplier);
        } else {
            break;
        }
    }
    return true;
}

// Skip whitespace helper
static void skipWS(const char* s, int& pos, int len) {
    while (pos < len && s[pos] == ' ') ++pos;
}

bool balanceEquation(const char* equation, char* result) {
    if (!equation || !result) return false;

    int len = static_cast<int>(strlen(equation));
    if (len == 0 || len >= MAX_FORMULA_LEN * 2) return false;

    // ── Step 1: Split into molecules ─────────────────────────────────────
    // Find '=' or '->' separator
    int eqPos = -1;
    bool usesArrow = false;
    for (int i = 0; i < len - 1; ++i) {
        if (equation[i] == '=') { eqPos = i; break; }
        if (equation[i] == '-' && equation[i+1] == '>') {
            eqPos = i; usesArrow = true; break;
        }
    }
    if (eqPos < 0) return false;

    // Parse left side
    char formulas[MAX_MOLECULES][MAX_FORMULA_LEN];
    int formulaCount = 0;
    int sideStart[MAX_MOLECULES]; // which side: 0=left, 1=right
    memset(formulas, 0, sizeof(formulas));

    // Parse left side molecules (separated by '+')
    {
        int p = 0;
        while (p < eqPos && formulaCount < MAX_MOLECULES) {
            skipWS(equation, p, eqPos);
            int fStart = p;
            while (p < eqPos && equation[p] != '+') ++p;
            int fEnd = p;
            // Trim trailing space
            while (fEnd > fStart && equation[fEnd-1] == ' ') --fEnd;
            if (fEnd > fStart) {
                int fLen = fEnd - fStart;
                if (fLen >= MAX_FORMULA_LEN) return false;
                memcpy(formulas[formulaCount], equation + fStart, fLen);
                formulas[formulaCount][fLen] = '\0';
                sideStart[formulaCount] = 0;
                ++formulaCount;
            }
            if (p < eqPos && equation[p] == '+') ++p;
        }
    }

    int rightStart = usesArrow ? eqPos + 2 : eqPos + 1;

    // Parse right side molecules
    {
        int p = rightStart;
        while (p < len && formulaCount < MAX_MOLECULES) {
            skipWS(equation, p, len);
            int fStart = p;
            while (p < len && equation[p] != '+') ++p;
            int fEnd = p;
            while (fEnd > fStart && equation[fEnd-1] == ' ') --fEnd;
            if (fEnd > fStart) {
                int fLen = fEnd - fStart;
                if (fLen >= MAX_FORMULA_LEN) return false;
                memcpy(formulas[formulaCount], equation + fStart, fLen);
                formulas[formulaCount][fLen] = '\0';
                sideStart[formulaCount] = 1;
                ++formulaCount;
            }
            if (p < len && equation[p] == '+') ++p;
        }
    }

    if (formulaCount < 2) return false;

    // ── Step 2: Parse each molecule's composition ────────────────────────
    FormulaComposition comps[MAX_MOLECULES];
    for (int m = 0; m < formulaCount; ++m) {
        int p = 0;
        int fLen = static_cast<int>(strlen(formulas[m]));
        if (!parseCompGroup(formulas[m], p, fLen, comps[m], 1)) return false;
    }

    // ── Step 3: Build unique element list ────────────────────────────────
    int uniqueElems[MAX_UNIQUE_ELEMS];
    int numUnique = 0;

    for (int m = 0; m < formulaCount; ++m) {
        for (int e = 0; e < comps[m].numElems; ++e) {
            int idx = comps[m].elemIdx[e];
            bool found = false;
            for (int u = 0; u < numUnique; ++u) {
                if (uniqueElems[u] == idx) { found = true; break; }
            }
            if (!found && numUnique < MAX_UNIQUE_ELEMS) {
                uniqueElems[numUnique++] = idx;
            }
        }
    }

    // Safety: prevent CPU hang on oversized matrices
    if (numUnique > 10 || formulaCount > 10) return false;

    // ── Step 4: Build composition matrix (rational) ──────────────────────
    // Matrix: numUnique rows × formulaCount columns
    // Left-side molecules get positive counts, right-side get negative
    RationalNumber matrix[MAX_UNIQUE_ELEMS][MAX_MOLECULES + 1]; // augmented

    for (int r = 0; r < numUnique; ++r) {
        for (int c = 0; c < formulaCount; ++c) {
            int elemIdx = uniqueElems[r];
            int cnt = 0;
            for (int e = 0; e < comps[c].numElems; ++e) {
                if (comps[c].elemIdx[e] == elemIdx) {
                    cnt = comps[c].count[e];
                    break;
                }
            }
            // Left side = positive, right side = negative
            int sign = (sideStart[c] == 0) ? 1 : -1;
            matrix[r][c] = RationalNumber(sign * cnt);
        }
    }

    int rows = numUnique;
    int cols = formulaCount;

    // ── Step 5: Gauss-Jordan elimination ─────────────────────────────────
    int pivotCol[MAX_UNIQUE_ELEMS];
    memset(pivotCol, -1, sizeof(pivotCol));

    int pivotRow = 0;
    for (int c = 0; c < cols && pivotRow < rows; ++c) {
        // Find pivot
        int pr = -1;
        for (int r = pivotRow; r < rows; ++r) {
            if (!matrix[r][c].isZero()) { pr = r; break; }
        }
        if (pr < 0) continue;

        // Swap rows
        if (pr != pivotRow) {
            for (int j = 0; j < cols; ++j) {
                RationalNumber tmp = matrix[pivotRow][j];
                matrix[pivotRow][j] = matrix[pr][j];
                matrix[pr][j] = tmp;
            }
        }

        pivotCol[pivotRow] = c;

        // Scale pivot row
        RationalNumber pivot = matrix[pivotRow][c];
        for (int j = 0; j < cols; ++j) {
            matrix[pivotRow][j] = matrix[pivotRow][j] / pivot;
        }

        // Eliminate column
        for (int r = 0; r < rows; ++r) {
            if (r == pivotRow) continue;
            if (matrix[r][c].isZero()) continue;
            RationalNumber factor = matrix[r][c];
            for (int j = 0; j < cols; ++j) {
                matrix[r][j] = matrix[r][j] - factor * matrix[pivotRow][j];
            }
        }

        ++pivotRow;
    }

    // ── Step 6: Extract nullspace vector ─────────────────────────────────
    // The free variable is the last column (or first column not a pivot)
    int freeCol = -1;
    for (int c = cols - 1; c >= 0; --c) {
        bool isPivot = false;
        for (int r = 0; r < pivotRow; ++r) {
            if (pivotCol[r] == c) { isPivot = true; break; }
        }
        if (!isPivot) { freeCol = c; break; }
    }

    if (freeCol < 0) return false; // No solution

    // Set free variable = 1, solve for others
    RationalNumber solution[MAX_MOLECULES];
    solution[freeCol] = RationalNumber(1);

    for (int r = pivotRow - 1; r >= 0; --r) {
        int pc = pivotCol[r];
        if (pc < 0) continue;
        RationalNumber val(0);
        for (int c = 0; c < cols; ++c) {
            if (c == pc) continue;
            val = val - matrix[r][c] * solution[c];
        }
        solution[pc] = val;
    }

    // ── Step 7: Scale to smallest positive integers ──────────────────────
    // Make all positive (flip signs if needed)
    bool allNeg = true;
    for (int c = 0; c < formulaCount; ++c) {
        if (solution[c].num > 0) { allNeg = false; break; }
    }
    if (allNeg) {
        for (int c = 0; c < formulaCount; ++c) {
            solution[c].num = -solution[c].num;
        }
    }

    // Find LCM of denominators
    int32_t denom_lcm = 1;
    for (int c = 0; c < formulaCount; ++c) {
        solution[c].simplify();
        if (solution[c].den != 0) {
            denom_lcm = lcm(denom_lcm, solution[c].den);
        }
    }

    // Scale to integers (use 64-bit intermediate to prevent overflow)
    int32_t coeffs[MAX_MOLECULES];
    for (int c = 0; c < formulaCount; ++c) {
        int64_t val = static_cast<int64_t>(solution[c].num)
                    * (denom_lcm / solution[c].den);
        if (val < 0) val = -val;
        if (val == 0 || val > INT32_MAX) return false;
        coeffs[c] = static_cast<int32_t>(val);
    }

    // Reduce by GCD of all coefficients
    int32_t allGcd = coeffs[0];
    for (int c = 1; c < formulaCount; ++c) {
        allGcd = gcd(allGcd, coeffs[c]);
    }
    for (int c = 0; c < formulaCount; ++c) {
        coeffs[c] /= allGcd;
    }

    // ── Step 8: Build result string ──────────────────────────────────────
    result[0] = '\0';
    int rPos = 0;
    bool firstLeft = true, firstRight = true;

    for (int c = 0; c < formulaCount; ++c) {
        if (sideStart[c] == 1 && firstRight) {
            // Add separator
            rPos += snprintf(result + rPos, MAX_RESULT_LEN - rPos, " = ");
            firstRight = false;
        } else if (sideStart[c] == 0 && !firstLeft) {
            rPos += snprintf(result + rPos, MAX_RESULT_LEN - rPos, " + ");
        } else if (sideStart[c] == 1 && !firstRight) {
            rPos += snprintf(result + rPos, MAX_RESULT_LEN - rPos, " + ");
        }

        if (sideStart[c] == 0) firstLeft = false;

        if (coeffs[c] != 1) {
            rPos += snprintf(result + rPos, MAX_RESULT_LEN - rPos, "%d", (int)coeffs[c]);
        }
        rPos += snprintf(result + rPos, MAX_RESULT_LEN - rPos, "%s", formulas[c]);

        if (rPos >= MAX_RESULT_LEN - 1) break;
    }

    return true;
}

} // namespace chem
