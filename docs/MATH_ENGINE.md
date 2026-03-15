# NumOS — Math Engine and Pro-CAS Engine

&gt; Complete technical documentation of the NumOS mathematical core.
&gt; Covers the numeric evaluation pipeline (Tokenizer→Parser→Evaluator),
&gt; the ExprNode visual tree, and the complete **Pro-CAS Engine** with exact
&gt; algebraic solving, symbolic differentiation and integration, PSRAM management, and educational step logging.
&gt;
&gt; **Status**: Numeric Engine ✅ · Pro-CAS Engine ✅ · Tests passing ✅

---

## Table of Contents

1. [General Pipeline](#1-general-pipeline)
2. [ExprNode — Visual Tree](#2-exprnode--visual-tree)
3. [Tokenizer](#3-tokenizer)
4. [Parser — Shunting-Yard](#4-parser--shunting-yard)
5. [Evaluator](#5-evaluator)
6. [VariableContext](#6-variablecontext)
7. [Numeric EquationSolver](#7-numeric-equationsolver)
8. [Pro-CAS Engine](#8-pro-cas-engine)
   - 8.1 [Architecture and Modules](#81-architecture-and-modules)
   - 8.2 [PSRAMAllocator](#82-psramallocator)
   - 8.3 [Rational and SymPoly](#83-rational-and-sympoly)
   - 8.4 [ASTFlattener](#84-astflattener)
   - 8.5 [SingleSolver](#85-singlesolver)
   - 8.6 [SystemSolver](#86-systemsolver)
   - 8.7 [CASStepLogger](#87-cassteploggger)
   - 8.8 [SymToAST](#88-symtoast)
   - 8.9 [SymExpr — Symbolic Tree](#89-symexpr--symbolic-tree-representation-phase-3)
   - 8.10 [SymDiff — Symbolic Differentiation](#810-symdiff--symbolic-differentiation-phase-3)
   - 8.11 [SymSimplify — Simplifier](#811-symsimplify--algebraic-simplifier-phase-3)
   - 8.12 [SymExprToAST — Conversion](#812-symexprtoast--conversion-to-mathast-phase-3)
   - 8.13 [CalculusApp — Unified Calculus App](#813-calculusapp--unified-calculus-app)
   - 8.14 [SymIntegrate — Symbolic Integration](#814-symintegrate--symbolic-integration-phase-6b)
9. [CAS Test Suite](#9-cas-test-suite)
10. [Extensibility](#10-extensibility)

---

## 1. General Pipeline

```
 User input (keys / serial)
           │
           ▼
  ┌─────────────────┐
  │    ExprNode     │  ← Visual tree (live editing)
  │  (Visual AST)   │     Fractions, Roots, Powers, Text
  └────────┬────────┘
           │  serialize() → plain string
           ▼
  ┌─────────────────┐
  │   Tokenizer     │  ← "3x^2+5x-2" → [NUM:3, VAR:x, POW, NUM:2, ADD, ...]
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐
  │     Parser      │  ← Shunting-Yard → RPN Queue
  └────────┬────────┘
           │
           ├──────────────────────────────────────────────┐
           ▼                                              ▼
  ┌─────────────────┐                         ┌──────────────────────────┐
  │   Evaluator     │                         │     ASTFlattener         │
  │  (numeric)      │                         │  (CAS-Lite — symbolic)   │
  │  double result  │                         │  ExprNode → SymPoly      │
  └─────────────────┘                         └────────────┬─────────────┘
                                                           │
                                              ┌────────────▼─────────────┐
                                              │   SingleSolver           │
                                              │   SystemSolver           │
                                              │  Exact Result            │
                                              │  (Rational) + steps      │
                                              └────────────┬─────────────┘
                                                           │
                                              ┌────────────▼─────────────┐
                                              │      SymToAST            │
                                              │  Rational → ExprNode     │
                                              │  Natural Display         │
                                              └──────────────────────────┘
```

---

## 2. ExprNode — Visual Tree

`ExprNode` represents the mathematical expression as a tree of visual nodes. It is the bridge between the user​'s editor (CalculationApp, EquationsApp) and the compute engine.

### Node Types

| Type | Description | Visual Example |
|:-----|:------------|:---------------|
| `TEXT` | Number, variable, operator, function | `3`, `x`, `sin`, `+` |
| `FRACTION` | Fraction with numerator and denominator | $\frac&#123;a&#125;&#123;b&#125;$ |
| `ROOT` | Root with index and radicand | $\sqrt[n]&#123;x&#125;$ |
| `POWER` | Base with raised exponent | $x^2$ |
| `PAREN` | Grouping with parentheses | $(a+b)$ |

### Key API

```cpp
// Create text node (number or variable)
ExprNode* n = new ExprNode(ExprNodeType::TEXT, "3.14");

// Fraction tree 2/3:
ExprNode* frac = new ExprNode(ExprNodeType::FRACTION);
frac->children.push_back(new ExprNode(ExprNodeType::TEXT, "2")); // numerator
frac->children.push_back(new ExprNode(ExprNodeType::TEXT, "3")); // denominator

// Serialization → evaluable string
std::string s = frac->serialize();  // "2/3"

// Recursive destruction
delete frac;  // Frees children automatically
```

---

## 3. Tokenizer

**File**: `src/math/Tokenizer.cpp/.h`  
**Input**: `std::string` with mathematical expression  
**Output**: `std::vector<Token>` (24 token types)

### Main Token Types

```
NUM     → numeric literal (double)         "3.14", "1e-5"
VAR     → variable (letter)                x, y, A, B ... Z
ADD / SUB / MUL / DIV / POW               + - * / ^
LPAREN / RPAREN                           ( )
COMMA                                     ,  (arg separator)
SIN / COS / TAN / ASIN / ACOS / ATAN     trigonometric functions
SINH / COSH / TANH                        hyperbolic
SQRT / LOG / LOG10 / EXP / ABS
TOFRAC                                    Ans→fraction
ANS                                       previous result
```

### Features

- Implicit multiplication: `3x` → `[NUM:3, MUL_IMPLICIT, VAR:x]`
- Unary negation: `-x` → `[NEG, VAR:x]`
- Constants: `π` (pi), `e` (Euler) recognized as NUM literals
- Space-tolerant — strip before tokenizing

---

## 4. Parser — Shunting-Yard

**File**: `src/math/Parser.cpp/.h`  
**Input**: `std::vector<Token>`  
**Output**: `std::queue<Token>` (Reverse Polish Notation)

### Shunting-Yard Algorithm

```
For each token t:
  NUM / VAR → output queue
  FUNCTION   → operator stack
  COMMA     → pop until LPAREN (separates args)
  OPERATOR  → pop operators of greater/equal precedence, push t
  LPAREN    → push stack
  RPAREN    → pop until LPAREN

Finally → pop entire stack to output
```

### Precedence Table

| Precedence | Operators |
|:-----------:|:-----------|
| 5 | Unary functions (`sin`, `cos`, `sqrt`, ...) |
| 4 | `^` (power, right associativity) |
| 3 | `*`, `/`, implicit multiplication |
| 2 | `+`, `-` |
| 1 | Parentheses |

---

## 5. Evaluator

**File**: `src/math/Evaluator.cpp/.h`  
**Input**: RPN Queue + `VariableContext`  
**Output**: `double` with result or `NAN` if error

### Process

```cpp
for (Token t : rpnQueue) {
    if (NUM)  → stack.push(t.value)
    if (VAR)  → stack.push(ctx.get(t.name))
    if (OP)   → pop operands, calculate, push result
    if (FUN)  → pop argument, calculate function, push result
}
return stack.top();
```

### Angular Modes

Controlled by `AngleMode { DEG, RAD, GRA }`:

```cpp
double toRad(double a) {
    switch (angleMode) {
        case DEG: return a * M_PI / 180.0;
        case GRA: return a * M_PI / 200.0;   // centesimal degrees
        case RAD: return a;
    }
}
// Applied before sin(), cos(), tan()
// Applied inversely in asin(), acos(), atan() (result → current mode)
```

---

## 6. VariableContext

**File**: `src/math/VariableContext.cpp/.h`

Manages variables `A`–`Z` and `Ans`.

| Function | Description |
|:--------|:------------|
| `set(char var, double val)` | Assign variable |
| `get(char var)` | Get value (0.0 if undefined) |
| `setAns(double v)` | Save last result |
| `getAns()` | Get last result |
| `saveToLittleFS()` | Persist to `/vars.dat` |
| `loadFromLittleFS()` | Load at startup |

---

## 7. Numeric EquationSolver

**File**: `src/math/EquationSolver.cpp/.h`

Solves `f(x) = 0` numerically with Newton-Raphson method.

```
x_{n+1} = x_n - f(x_n) / f'(x_n)

Where f'(x) ≈ (f(x+h) - f(x-h)) / (2h),  h = 1e-7

Stop criterion: |f(x)| < 1e-10  or  max 100 iterations
Test seeds: x₀ ∈ {0, 1, -1, 2, -2, 5, -5, 10}
```

&gt; **Limitation**: Finds one real root. For exact algebra with all roots, use the **Pro-CAS Engine** (Section 8).

---

## 8. Pro-CAS Engine

★ Complete computational algebra motor designed for embedded systems. Evolution of the original CAS-Lite. Includes polynomial equation solving with exact results, symbolic differentiation (17 rules), symbolic integration (Slagle heuristic), multi-pass simplification, and immutable DAG with hash-consing. All CAS memory lives in PSRAM with `PSRAMAllocator`.

### 8.1 Architecture and Modules

```
src/math/cas/
├── CASInt.h              ← Hybrid BigInt: int64 fast-path + mbedtls_mpi
├── CASRational.h/.cpp    ← Exact fraction overflow-safe (auto-GCD)
├── ConsTable.h           ← Hash-consing PSRAM: dedup nodes
├── PSRAMAllocator.h      ← STL-compatible allocator → ps_malloc / ps_free
├── SymExpr.h/.cpp        ← Immutable DAG (hash + weight)
├── SymExprArena.h        ← Bump allocator PSRAM + ConsTable integrated
├── SymPoly.h/.cpp        ← Rational (exact fraction) + SymPoly (polynomial)
├── SymPolyMulti.h/.cpp   ← Multivariable polynomial + Sylvester resultant
├── ASTFlattener.h/.cpp   ← MathAST → SymExpr DAG (hash-consed)
├── SymDiff.h/.cpp        ← Differentiation: 17 rules (chain, product, trig, exp, log)
├── SymIntegrate.h/.cpp   ← Slagle integration: table, linearity, u-sub, parts
├── SymSimplify.h/.cpp    ← Fixed-point simplifier (8 passes, trig/log/exp)
├── SingleSolver.h/.cpp   ← 1-var equation: linear / quadratic / N-R degree N
├── SystemSolver.h/.cpp   ← 2×2 system: Gaussian + NL (resultant)
├── OmniSolver.h/.cpp     ← Analytic variable isolation
├── HybridNewton.h/.cpp   ← Newton-Raphson with symbolic Jacobian
├── CASStepLogger.h/.cpp  ← StepVec PSRAM: INFO / FORMULA / RESULT / ERROR
├── SymToAST.h/.cpp       ← SolveResult → MathAST Natural Display
└── SymExprToAST.h/.cpp   ← SymExpr → MathAST (+C, ∫)
```

**Complete pipeline**:
```
User: "3x+6=0"
  │
  ├─ splitAtEquals() → lhs="3x+6", rhs="0"
  ├─ Parser(lhs) → ExprNode AST left
  ├─ Parser(rhs) → ExprNode AST right
  │
  ├─ ASTFlattener::flatten(lhsNode, rhsNode) → SymPoly (lhs - rhs)
  │    SymPoly: { 1: 3, 0: 6 }   (3x + 6)
  │
  ├─ SingleSolver::solve(poly)
  │    degree 1 → x = -6/3 = -2
  │    Steps: INFO "Linear" / FORMULA "x = -b/a" / RESULT "x = -2"
  │
  └─ SymToAST() → ExprNode "x = -2"  (Natural Display)
```

### 8.2 PSRAMAllocator

STL-compatible allocator that redirects all allocations to the PSRAM heap:

```cpp
template<typename T>
struct PSRAMAllocator {
    using value_type = T;

    T* allocate(std::size_t n) {
        void* ptr = ps_malloc(n * sizeof(T));
        if (!ptr) throw std::bad_alloc();
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t) noexcept {
        ps_free(ptr);
    }
};
```

Types that use it:
- `CoeffMap` in `SymPoly` → `std::map<int, Rational, ..., PSRAMAllocator<...>>`
- `StepVec` in `CASStepLogger` → `std::vector<CASStep, PSRAMAllocator<CASStep>>`

### 8.3 Rational and SymPoly

#### `Rational` — Exact Fraction

```cpp
struct Rational {
    int64_t num, den;  // den always > 0, auto-reduced by GCD

    Rational(int64_t n = 0, int64_t d = 1);  // Normalizes: Rational(6,4) → {3,2}
    double   toDouble() const { return (double)num / den; }
    bool     isInteger() const { return den == 1; }
    bool     isZero()    const { return num == 0; }

    // Exact arithmetic:
    Rational operator+(Rational o) const;
    Rational operator-(Rational o) const;
    Rational operator*(Rational o) const;
    Rational operator/(Rational o) const;  // throws if o.num==0
    bool     operator==(Rational o) const;
};
```

#### `SymPoly` — Symbolic Polynomial

```cpp
using CoeffMap = std::map<int, Rational, std::less<int>,
                          PSRAMAllocator<std::pair<const int, Rational>>>;

struct SymPoly {
    CoeffMap coeffs;   // { degree → Rational }
    char     var;      // Symbolic variable ('x' by default)

    int      degree() const;               // Max degree with nonzero coeff
    Rational coeff(int deg) const;         // Coeff at 'deg' (0 if not exists)
    SymPoly  derivative() const;           // Symbolic derivative
    double   evaluate(double x) const;     // Numeric evaluation (Newton-Raphson)

    SymPoly  operator+(const SymPoly& o) const;
    SymPoly  operator-(const SymPoly& o) const;
    SymPoly  operator*(const Rational& r) const;  // Scalar
};
```

**Example**:
```cpp
// Represents: 3x² - 5x + 2
SymPoly p;
p.coeffs = { {2, {3,1}}, {1, {-5,1}}, {0, {2,1}} };
p.degree();       // 2
p.coeff(1);       // Rational{-5, 1}
p.evaluate(1.0);  // 3 - 5 + 2 = 0.0
```

### 8.4 ASTFlattener

Converts a pair of `ExprNode` (lhs, rhs of equation) into a single `SymPoly` representing `lhs - rhs = 0`.

**Conversion rules (recursive visitation)**:

| ExprNode Node | Action |
|:--------------|:-------|
| `TEXT` number | `SymPoly` degree 0 with numeric value |
| `TEXT` variable (e.g. `x`) | `SymPoly` degree 1, coeff 1 → `{1: 1/1}` |
| `ADD`, `SUB` | Flatten children recursively, add/subtract SymPolys |
| `MUL` (scalar × poly) | Multiply SymPoly by Rational |
| `MUL` (poly × poly) | Polynomial product term by term |
| `POW` (var^n, n integer) | `SymPoly` degree n, coeff 1 |
| `FRACTION` | Exact Rational (num/den) as SymPoly degree 0 |
| `NEG` | Multiply SymPoly by `-1` |

**Example conversion**:
```
AST: ADD(MUL(NUM:3, POW(VAR:x, NUM:2)), SUB(MUL(NUM:5, VAR:x), NUM:2))
Represents: 3x² + 5x - 2

ASTFlattener::visit():
  → ADD of:
     MUL(3, x²) → SymPoly {2: 3}
     SUB(5x, 2) → SymPoly {1: 5, 0: -2}
  → Result: SymPoly {2: 3, 1: 5, 0: -2}  ✓
```

### 8.5 SingleSolver

**File**: `src/math/cas/SingleSolver.h/.cpp`  
**Input**: `SymPoly` (equation set equal to 0)  
**Output**: `SolveResult` with exact `Rational` + `CASStepLogger` with steps

#### Result Structure

```cpp
struct SolveResult {
    enum class Status {
        OK_ONE,     // One real solution
        OK_TWO,     // Two real solutions (quadratic)
        COMPLEX,    // No real solution (discriminant < 0)
        INFINITE,   // Infinite solutions (0 = 0)
        NONE,       // No solution (0 = c, c≠0)
        ERROR       // Internal error
    };
    Status        status;
    Rational      root1, root2;
    CASStepLogger steps;
};
```

#### Logic by Degree

```
Degree 0: coeff₀ = 0? → INFINITE : NONE

Degree 1: ax + b = 0
  x = -b/a   (Exact Rational)
  Steps generated:
    [INFO]    "Linear equation degree 1"
    [FORMULA] "x = -b / a"
    [RESULT]  "x = {value}"

Degree 2: ax² + bx + c = 0
  Δ = b² - 4ac  (calculated as Rational)
  Δ < 0  → COMPLEX; steps: [INFO] "Negative discriminant"
  Δ = 0  → x = -b/(2a);  [FORMULA] "Double root: x = -b/(2a)"
  Δ > 0  → x₁ = (-b+√Δ)/(2a),  x₂ = (-b-√Δ)/(2a)
           √Δ exact if Δ is perfect square, double otherwise
  Steps: [INFO] "Quadratic formula", [FORMULA] "Δ = b²-4ac = {val}",
         [RESULT] "x₁ = {val}", [RESULT] "x₂ = {val}"

Degree ≥ 3: Newton-Raphson numeric
  Seeds: {0, 1, -1, 2, -2}
  Convergence criterion: |f(x)| < 1e-10, max 100 iterations
  Root → Rational{(int64_t)round(root*1e9), 1000000000} (rational approximation)
  Steps: [INFO] "Newton-Raphson degree N", [RESULT] "x ≈ {val}"
```

### 8.6 SystemSolver

**File**: `src/math/cas/SystemSolver.h/.cpp`  
**Input**: 2 equation expressions (strings), secondary variable char  
**Output**: `SystemResult` with exact `Rational` x, y + steps

#### Result

```cpp
struct SystemResult {
    enum class Status { OK, INFINITE, INCONSISTENT, ERROR };
    Status        status;
    Rational      x, y;
    CASStepLogger steps;
};
```

#### Gaussian Elimination 2×2 Algorithm

```
System:
  eq1: a₁·x + b₁·y = c₁
  eq2: a₂·x + b₂·y = c₂

Step 1: Extract coefficients a₁, b₁, c₁, a₂, b₂, c₂
         using ASTFlattener (identification of primary/secondary variable)

Step 2: Elimination of x:
  eq1' = eq1 × a₂   →  (a₁·a₂)x + (b₁·a₂)y = c₁·a₂
  eq2' = eq2 × a₁   →  (a₁·a₂)x + (b₂·a₁)y = c₂·a₁
  eq3  = eq1' - eq2' →  (b₁·a₂ - b₂·a₁)y = c₁·a₂ - c₂·a₁

Step 3: Solve for y:
  D = b₁·a₂ - b₂·a₁   (Exact Rational)
  D = 0 and num≠0 → INCONSISTENT ("Inconsistent system")
  D = 0 and num=0 → INFINITE    ("Dependent system")
  D ≠ 0         → y = (c₁·a₂ - c₂·a₁) / D

Step 4: Substitute y into eq1 to get x:
  x = (c₁ - b₁·y) / a₁

Steps generated (5-7 steps): INFO, FORMULA, INFO, INFO, RESULT×2
```

### 8.7 CASStepLogger

**File**: `src/math/cas/CASStepLogger.h/.cpp`

PSRAM-based log of CAS calculation steps. Displayed to user in steps view of `EquationsApp`.

```cpp
enum class StepType { INFO, FORMULA, RESULT, ERROR };

struct CASStep {
    StepType    type;
    std::string text;  // Step text
};

using StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>;

class CASStepLogger {
public:
    void        add(StepType type, const std::string& text);
    void        clear();                // ← CALL in EquationsApp::end()
    size_t      count() const;
    const CASStep& get(size_t i) const;
};
```

**Step types and rendering colors in EquationsApp**:

| Type | Color | Usage |
|:-----|:------|:----|
| `INFO` | Gray / secondary text | Method description |
| `FORMULA` | Blue | Applied mathematical formula |
| `RESULT` | Green | Result obtained |
| `ERROR` | Red | Error or degenerate case |

**Memory management**:

`SolveResult::steps` and `SystemResult::steps` contain `CASStepLogger` with PSRAM.  
`EquationsApp` stores them as members (`_singleResult`, `_systemResult`).  
In `EquationsApp::end()`:
```cpp
_singleResult.steps.clear();  // ← Without this: PSRAM leak between sessions
_systemResult.steps.clear();
_resultHint = nullptr;        // ← LVGL widget already destroyed, null pointer
```

### 8.8 SymToAST

**File**: `src/math/cas/SymToAST.h/.cpp`

Converts CAS results to `ExprNode` trees for rendering in Natural Display.

```cpp
// Rational → ExprNode
ExprNode* SymToAST::rationalToNode(Rational r) {
    if (r.isInteger()) return textNode(std::to_string(r.num));
    // Fraction: ExprNode FRACTION with num/den as TEXT children
    ExprNode* frac = new ExprNode(ExprNodeType::FRACTION);
    frac->children.push_back(textNode(std::to_string(r.num)));
    frac->children.push_back(textNode(std::to_string(r.den)));
    return frac;
}

// SolveResult → ExprNode "x = value"
ExprNode* SymToAST::solveResultToNode(const SolveResult& r, char varName);

// SystemResult → ExprNode "x = val1, y = val2"
ExprNode* SymToAST::systemResultToNode(const SystemResult& r, char v1, char v2);
```

Output examples:
- `Rational{3, 1}` → `ExprNode TEXT "3"`
- `Rational{1, 2}` → `ExprNode FRACTION [TEXT "1" / TEXT "2"]` → renders $\frac&#123;1&#125;&#123;2&#125;$
- `SolveResult(OK_TWO, -1/3, -2)` → `"x₁ = -1/3, x₂ = -2"`

---

## 8.9 SymExpr — Symbolic Tree (Phase 3)

**Files**: `src/math/cas/SymExpr.h`, `SymExpr.cpp`

Arena-allocated symbolic expression tree for CAS:

| Type | Class | Fields |
|------|-------|--------|
| `Num` | `SymNum` | `ExactVal value` |
| `Var` | `SymVar` | `char name` |
| `Neg` | `SymNeg` | `SymExpr* child` |
| `Add` | `SymAdd` | `SymExpr** terms`, `uint16_t count` |
| `Mul` | `SymMul` | `SymExpr** factors`, `uint16_t count` |
| `Pow` | `SymPow` | `SymExpr* base`, `SymExpr* exponent` |
| `Func` | `SymFunc` | `SymFuncKind kind`, `SymExpr* argument` |

**Key API**:
- `toString()` — readable textual representation
- `evaluate(char var, double val)` — numeric evaluation
- `isPolynomial()` — determines if polynomial
- `toSymPoly(char var)` — converts to SymPoly if polynomial

**SymExprArena** (`SymExprArena.h`):
Bump allocator PSRAM. 64KB blocks, max 4 blocks.
```cpp
arena.create<SymNum>(ExactVal::fromInt(5));
arena.symVar('x');
arena.symPow(x, arena.symInt(2));
arena.reset();  // Frees all
arena.currentUsed();   // bytes in use
arena.blockCount();    // active blocks
```

---

## 8.10 SymDiff — Symbolic Differentiation (Phase 3)

**File**: `src/math/cas/SymDiff.h`, `SymDiff.cpp`

```cpp
static SymExpr* SymDiff::diff(SymExpr* expr, char var, SymExprArena& arena);
```

**Implemented rules**:

| Rule | Input | Result |
|------|-------|--------|
| Constant | `d/dx(c)` | `0` |
| Variable | `d/dx(x)` | `1` |
| Sum | `d/dx(u+v)` | `u' + v'` |
| Product | `d/dx(u·v)` | `u'·v + u·v'` |
| Power | `d/dx(x^n)` | `n·x^(n-1)` |
| Chain | `d/dx(f(g(x)))` | `f'(g(x))·g'(x)` |
| Sine | `d/dx(sin u)` | `cos(u)·u'` |
| Cosine | `d/dx(cos u)` | `-sin(u)·u'` |
| Tangent | `d/dx(tan u)` | `(1/cos²(u))·u'` |
| Exponential | `d/dx(e^u)` | `e^u·u'` |
| Logarithm | `d/dx(ln u)` | `u'/u` |
| Log₁₀ | `d/dx(log₁₀ u)` | `u'/(u·ln(10))` |
| ArcSin | `d/dx(arcsin u)` | `u'/√(1-u²)` |
| ArcCos | `d/dx(arccos u)` | `-u'/√(1-u²)` |
| ArcTan | `d/dx(arctan u)` | `u'/(1+u²)` |

---

## 8.11 SymSimplify — Algebraic Simplifier (Phase 3)

**File**: `src/math/cas/SymSimplify.h`, `SymSimplify.cpp`

```cpp
static SymExpr* SymSimplify::simplify(SymExpr* expr, SymExprArena& arena);
```

**Simplification rules**:
- `0 + x → x`, `x + 0 → x`
- `1 · x → x`, `x · 1 → x`
- `0 · x → 0`
- `x^0 → 1`, `x^1 → x`
- Double negation: `--x → x`
- Constant folding (operations between literals)
- Collapse unit collections (`Add([x]) → x`)

---

## 8.12 SymExprToAST — Conversion to MathAST (Phase 3)

**File**: `src/math/cas/SymExprToAST.h`, `SymExprToAST.cpp`

```cpp
static vpam::NodePtr SymExprToAST::convert(const SymExpr* expr);
```

Converts a `SymExpr` tree (result of diff/simplify) back to
`MathAST` (`NodePtr`) for 2D rendering in `MathCanvas`:

| SymExpr | MathAST |
|---------|---------|
| `SymNum` | `NodeNumber` |
| `SymVar` | `NodeVariable` |
| `SymNeg` | `NodeRow` with `OpKind::Sub` |
| `SymAdd` | `NodeRow` with operators `+/-` |
| `SymMul` | `NodeRow` with operator `×` |
| `SymPow` | `NodePower` |
| `SymFunc` | `NodeFunction` |

---

## 8.13 CalculusApp — Unified Calculus App

**Files**: `src/apps/CalculusApp.h`, `CalculusApp.cpp`

LVGL-native app that unifies symbolic derivatives and integrals in a single interface with tab-based mode switching. Replaces the former separate CalculusApp (derivatives only) and IntegralApp.

### Modes

| Mode | Tab Label | Accent Color | Pipeline |
|------|-----------|:------------:|----------|
| `DERIVATIVE` | "d/dx Derivar" | Orange `#E05500` | SymDiff → SymSimplify → SymExprToAST::convert() |
| `INTEGRAL` | "∫dx Integrar" | Purple `#6A1B9A` | SymIntegrate → SymSimplify → SymExprToAST::convertIntegral() (+C) |

### Complete Pipeline (Derivatives)

```
                   ┌──────────────┐
 Keyboard ──► VPAM │ MathCanvas   │  Visual node (2D)
                   │  (NodeRow)   │
                   └──────┬───────┘
                          │ ASTFlattener
                          ▼
                   ┌──────────────┐
                   │  SymExpr*    │  Symbolic tree (arena)
                   └──────┬───────┘
                          │ SymDiff::diff()
                          ▼
                   ┌──────────────┐
                   │ Raw Deriv    │  Derivative without simplifying
                   └──────┬───────┘
                          │ SymSimplify::simplify()
                          ▼
                   ┌──────────────┐
                   │ Simplified   │  Simplified derivative
                   └──────┬───────┘
                          │ SymExprToAST::convert()
                          ▼
                   ┌──────────────┐
                   │ MathCanvas   │  2D rendering of f'(x)
                   └──────────────┘
```

### Complete Pipeline (Integrals)

```
                   ┌──────────────┐
 Keyboard ──► VPAM │ MathCanvas   │  Visual node (2D)
                   │  (NodeRow)   │
                   └──────┬───────┘
                          │ ASTFlattener
                          ▼
                   ┌──────────────┐
                   │  SymExpr*    │  Symbolic tree (arena)
                   └──────┬───────┘
                          │ SymIntegrate::integrate()
                          ▼
                   ┌──────────────┐
                   │ Raw Integral │  Antiderivative (or nullptr)
                   └──────┬───────┘
                          │ SymSimplify::simplify()
                          ▼
                   ┌──────────────┐
                   │ Simplified   │  Simplified integral
                   └──────┬───────┘
                          │ SymExprToAST::convertIntegral()
                          ▼
                   ┌──────────────┐
                   │ MathCanvas   │  2D rendering: F(x) + C
                   └──────────────┘
```

### App States

| State | Description |
|--------|-------------|
| `EDITING` | Expression input with MathCanvas + cursor |
| `COMPUTING` | Spinner + mode-specific random message while calculating |
| `RESULT` | Shows f(x) and f'(x) or F(x)+C with 2D rendering |
| `STEPS` | Scrollable list of differentiation/integration steps |

### Two Result Branches (Integral mode)

1. **Integral Solved** (`_integralFound = true`):
   - Label: "F(x) ="
   - Uses `SymExprToAST::convertIntegral()` which adds `+ C` automatically
   - Green accent on status bar

2. **Integral Unsolved** (`_integralFound = false`):
   - Label: "∫f(x)dx ="
   - Shows original expression with ∫ symbol
   - Orange accent on status bar

### Supported Functions

All NumOS keyboard functions: `sin`, `cos`, `tan`, `arcsin`, `arccos`,
`arctan`, `ln`, `log`, `√`, `π`, `e`, fractions, powers, parentheses.

### Controls

| Key | Action |
|--------|--------|
| ENTER / = | Compute derivative or integral |
| AC | Clear / go back |
| GRAPH | Toggle mode: d/dx ↔ ∫dx |
| SHIFT+trig | Inverse functions (arcsin, etc.) |
| SHOW_STEPS | View computation steps |
| ↑/↓ | Scroll in steps view |

### Memory Management

- `SymExprArena _arena` as class member (not stack-local)
- `_arena.reset()` at beginning of each computation
- `_arena.reset()` in `end()` to cleanup PSRAM on exit
- `_casSteps.clear()` in `end()` to free StepVec PSRAM
- Result `_resultExpr` is arena-owned — valid until next reset

---

## 8.14 SymIntegrate — Symbolic Integration (Phase 6B)

**File**: `src/math/cas/SymIntegrate.h`, `SymIntegrate.cpp`

```cpp
static const SymExpr* SymIntegrate::integrate(
    SymExprArena& arena, const SymExpr* expr, const char* var);
```

Heuristic symbolic integration engine inspired by Slagle's algorithm.
Returns `nullptr` if unable to find a closed form antiderivative.

**Strategies (in order of application)**:

| # | Strategy | Example |
|---|----------|---------|
| 1 | **Direct table** | ∫sin(x)dx = -cos(x), ∫eˣdx = eˣ, ∫1/x dx = ln(x) |
| 2 | **Linearity** | ∫(af + bg)dx = a∫fdx + b∫gdx |
| 3 | **Powers** | ∫xⁿdx = xⁿ⁺¹/(n+1) for n≠-1 |
| 4 | **u-substitution** | ∫f(g(x))·g'(x)dx → ∫f(u)du with u=g(x) |
| 5 | **Parts (LIATE)** | ∫u·dv = uv - ∫v·du, priority: Log &gt; InvTrig &gt; Alg &gt; Trig &gt; Exp |

**Complete pipeline**:
```
Input: SymExpr* (expression to integrate)
           │
           ▼
  ┌────────────────────┐
  │ Direct table       │  Matches known pattern?
  └────────┬───────────┘
           │ no
           ▼
  ┌────────────────────┐
  │ Linearity          │  Is sum/subtraction? → integrate each term
  └────────┬───────────┘
           │ no
           ▼
  ┌────────────────────┐
  │ u-substitution     │  Has form f(g(x))·g'(x)?
  └────────┬───────────┘
           │ no
           ▼
  ┌────────────────────┐
  │ Parts (LIATE)      │  Assign u, dv by LIATE priority
  └────────┬───────────┘
           │ no
           ▼
       nullptr  (unresolved integral → shown as ∫)
```

---

## 9. CAS Test Suite

**File**: `tests/CASTest.h/.cpp`  
**Enable**: Uncomment `-DCAS_RUN_TESTS` and `+<../tests/CASTest.cpp>` in `platformio.ini`

```
Phase A — SymPoly (12 tests):
  - Rational: arithmetic, normalization (6/4→3/2), division by zero
  - SymPoly: coefficients, degree, sum, subtraction, scalar multiplication
  - SymPoly: polynomial derivative

Phase B — ASTFlattener (15 tests):
  - Conversion of numeric and variable literals
  - Linear expressions: ax+b, with fractions, with negation
  - Quadratic expressions: x²+2x+1, (x+1)(x-1) = x²-1
  - Degenerate cases: pure constant, variable with fractional coeff

Phase C — SingleSolver (16 tests):
  - Degree 0: INFINITE / NONE
  - Degree 1: x = integer, x = fraction, x = negative
  - Degree 2: discriminant 0, >0 (integers), >0 (irrational → double), &lt;0
  - Degree 2: normalized quadratic formula 2x²-5x+2=0 → x=2, x=1/2
  - Steps: verify SingleSolver generates exactly N expected steps

Phase D — SystemSolver (10 tests):
  - System with unique integer solution
  - System with fractional solution
  - Inconsistent system (D=0, num≠0)
  - Dependent system (D=0, num=0)
  - Asymmetric system (zero coefficient in one equation)

Phase E — SymDiff + SymSimplify + SymExprToAST (32 tests):
  - Derivatives of polynomials: constant, linear, quadratic, cubic
  - Trigonometric derivatives: sin, cos, tan + chain rule
  - Derivative of exponential, logarithm, arc functions
  - Product rule: x·sin(x), x²·ln(x)
  - SymSimplify: 0+x→x, 1·x→x, 0·x→0, x^0→1, x^1→x
  - SymExprToAST: correct conversion of node types

Phase F — CalculusStressTest (50 iterations):
  - 25 distinct expressions × 2 repetitions
  - Verify arena.reset() frees memory between iterations
  - Verify arena blocks ≤ 4 (bounded)
  - Verify toString() and SymExprToAST::convert() don't crash

TOTAL: 85+ tests — all passing ✅
```

**Example output** (Serial Monitor, test mode):
```
[CAS TEST] Phase A: SymPoly.............. 12/12 OK
[CAS TEST] Phase B: ASTFlattener......... 15/15 OK
[CAS TEST] Phase C: SingleSolver......... 16/16 OK
[CAS TEST] Phase D: SystemSolver......... 10/10 OK
[CAS TEST] Phase E: SymDiff.............. 32/32 OK
[CAS TEST] Phase F: StressTest........... 50/50 OK
```

---

## 10. Extensibility

*NumOS — Open-source scientific calculator OS for ESP32-S3 N16R8.*
