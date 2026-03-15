# NumOS — Motor Matemático y Pro-CAS Engine

> Documentación técnica completa del núcleo matemático de NumOS.
> Cubre el pipeline de evaluación numérica (Tokenizer→Parser→Evaluator),
> el árbol visual ExprNode, y el **Pro-CAS Engine** completo con resolución
> algebraica exacta, derivación e integración simbólicas, gestión en PSRAM y log de pasos educativo.
>
> **Estado**: Motor numérico ✅ · Pro-CAS Engine ✅ · Tests passing ✅

---

## Tabla de Contenidos

1. [Pipeline General](#1-pipeline-general)
2. [ExprNode — Árbol Visual](#2-exprnode--árbol-visual)
3. [Tokenizer](#3-tokenizer)
4. [Parser — Shunting-Yard](#4-parser--shunting-yard)
5. [Evaluator](#5-evaluator)
6. [VariableContext](#6-variablecontext)
7. [EquationSolver Numérico](#7-equationsolver-numérico)
8. [Pro-CAS Engine](#8-pro-cas-engine)
   - 8.1 [Arquitectura y Módulos](#81-arquitectura-y-módulos)
   - 8.2 [PSRAMAllocator](#82-psramallocator)
   - 8.3 [Rational y SymPoly](#83-rational-y-sympoly)
   - 8.4 [ASTFlattener](#84-astflattener)
   - 8.5 [SingleSolver](#85-singlesolver)
   - 8.6 [SystemSolver](#86-systemsolver)
   - 8.7 [CASStepLogger](#87-cassteploggger)
   - 8.8 [SymToAST](#88-symtoast)
   - 8.9 [SymExpr — Árbol simbólico](#89-symexpr--representación-simbólica-de-árbol-phase-3)
   - 8.10 [SymDiff — Derivación simbólica](#810-symdiff--derivación-simbólica-phase-3)
   - 8.11 [SymSimplify — Simplificador](#811-symsimplify--simplificador-algebraico-phase-3)
   - 8.12 [SymExprToAST — Conversión](#812-symexprtoast--conversión-a-mathast-phase-3)
   - 8.13 [CalculusApp — App de Cálculo unificada](#813-calculusapp--app-de-cálculo-unificada)
   - 8.14 [SymIntegrate — Integración simbólica](#814-symintegrate--integración-simbólica-phase-6b)
9. [Suite de Tests CAS](#9-suite-de-tests-cas)
10. [Extensibilidad](#10-extensibilidad)

---

## 1. Pipeline General

```
 Entrada del usuario (teclas / serial)
           │
           ▼
  ┌─────────────────┐
  │    ExprNode     │  ← Árbol visual (edición en vivo)
  │  (AST Visual)   │     Fracciones, Raíces, Potencias, Texto
  └────────┬────────┘
           │  serialize() → string plano
           ▼
  ┌─────────────────┐
  │   Tokenizer     │  ← "3x^2+5x-2" → [NUM:3, VAR:x, POW, NUM:2, ADD, ...]
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐
  │     Parser      │  ← Shunting-Yard → Cola RPN
  └────────┬────────┘
           │
           ├──────────────────────────────────────────────┐
           ▼                                              ▼
  ┌─────────────────┐                         ┌──────────────────────────┐
  │   Evaluator     │                         │     ASTFlattener         │
  │  (numérico)     │                         │  (CAS-Lite — simbólico)  │
  │  double result  │                         │  ExprNode → SymPoly      │
  └─────────────────┘                         └────────────┬─────────────┘
                                                           │
                                              ┌────────────▼─────────────┐
                                              │   SingleSolver           │
                                              │   SystemSolver           │
                                              │  Resultado exacto        │
                                              │  (Rational) + pasos      │
                                              └────────────┬─────────────┘
                                                           │
                                              ┌────────────▼─────────────┐
                                              │      SymToAST            │
                                              │  Rational → ExprNode     │
                                              │  Natural Display         │
                                              └──────────────────────────┘
```

---

## 2. ExprNode — Árbol Visual

`ExprNode` representa la expresión matemática como árbol de nodos visuales. Es el puente entre el editor del usuario (CalculationApp, EquationsApp) y el motor de cálculo.

### Tipos de nodo

| Tipo | Descripción | Ejemplo Visual |
|:-----|:------------|:---------------|
| `TEXT` | Número, variable, operador, función | `3`, `x`, `sin`, `+` |
| `FRACTION` | Fracción con numerador y denominador | $\frac{a}{b}$ |
| `ROOT` | Raíz con índice y radicando | $\sqrt[n]{x}$ |
| `POWER` | Base con exponente elevado | $x^2$ |
| `PAREN` | Agrupación con paréntesis | $(a+b)$ |

### API Clave

```cpp
// Crear nodo texto (número o variable)
ExprNode* n = new ExprNode(ExprNodeType::TEXT, "3.14");

// Árbol fracción 2/3:
ExprNode* frac = new ExprNode(ExprNodeType::FRACTION);
frac->children.push_back(new ExprNode(ExprNodeType::TEXT, "2")); // numerador
frac->children.push_back(new ExprNode(ExprNodeType::TEXT, "3")); // denominador

// Serialización → string evaluable
std::string s = frac->serialize();  // "2/3"

// Destrucción recursiva
delete frac;  // Libera hijos automáticamente
```

---

## 3. Tokenizer

**Archivo**: `src/math/Tokenizer.cpp/.h`  
**Entrada**: `std::string` con expresión matemática  
**Salida**: `std::vector<Token>` (24 tipos de token)

### Tipos de Token principales

```
NUM     → número literal (double)         "3.14", "1e-5"
VAR     → variable (letra)                x, y, A, B ... Z
ADD / SUB / MUL / DIV / POW               + - * / ^
LPAREN / RPAREN                           ( )
COMMA                                     ,  (sep. args)
SIN / COS / TAN / ASIN / ACOS / ATAN     funciones trigonométricas
SINH / COSH / TANH                        hiperbólicas
SQRT / LOG / LOG10 / EXP / ABS
TOFRAC                                    Ans→fracción
ANS                                       resultado anterior
```

### Características

- Multiplicación implícita: `3x` → `[NUM:3, MUL_IMPLICIT, VAR:x]`
- Negación unaria: `-x` → `[NEG, VAR:x]`
- Constantes: `π` (pi), `e` (Euler) reconocidas como literales NUM
- Tolerante a espacios — strip antes del tokenizado

---

## 4. Parser — Shunting-Yard

**Archivo**: `src/math/Parser.cpp/.h`  
**Entrada**: `std::vector<Token>`  
**Salida**: `std::queue<Token>` (Reverse Polish Notation)

### Algoritmo Shunting-Yard

```
Para cada token t:
  NUM / VAR → output queue
  FUNCIÓN   → operator stack
  COMMA     → pop hasta LPAREN (separa args)
  OPERADOR  → pop operadores de mayor/igual precedencia, push t
  LPAREN    → push stack
  RPAREN    → pop hasta LPAREN

Al final → pop todo el stack al output
```

### Tabla de precedencias

| Precedencia | Operadores |
|:-----------:|:-----------|
| 5 | Funciones unarias (`sin`, `cos`, `sqrt`, ...) |
| 4 | `^` (potencia, asociatividad derecha) |
| 3 | `*`, `/`, multiplicación implícita |
| 2 | `+`, `-` |
| 1 | Paréntesis |

---

## 5. Evaluator

**Archivo**: `src/math/Evaluator.cpp/.h`  
**Entrada**: Cola RPN + `VariableContext`  
**Salida**: `double` con resultado o `NAN` si error

### Proceso

```cpp
for (Token t : rpnQueue) {
    if (NUM)  → stack.push(t.value)
    if (VAR)  → stack.push(ctx.get(t.name))
    if (OP)   → pop operandos, calcular, push resultado
    if (FUN)  → pop argumento, calcular función, push resultado
}
return stack.top();
```

### Modos angulares

Controlado por `AngleMode { DEG, RAD, GRA }`:

```cpp
double toRad(double a) {
    switch (angleMode) {
        case DEG: return a * M_PI / 180.0;
        case GRA: return a * M_PI / 200.0;   // grados centesimales
        case RAD: return a;
    }
}
// Aplicado antes de sin(), cos(), tan()
// Aplicado inversamente en asin(), acos(), atan() (resultado → modo actual)
```

---

## 6. VariableContext

**Archivo**: `src/math/VariableContext.cpp/.h`

Gestiona variables `A`–`Z` y `Ans`.

| Función | Descripción |
|:--------|:------------|
| `set(char var, double val)` | Asignar variable |
| `get(char var)` | Obtener valor (0.0 si no definida) |
| `setAns(double v)` | Guardar último resultado |
| `getAns()` | Obtener último resultado |
| `saveToLittleFS()` | Persistir en `/vars.dat` |
| `loadFromLittleFS()` | Cargar al arrancar |

---

## 7. EquationSolver Numérico

**Archivo**: `src/math/EquationSolver.cpp/.h`

Resuelve `f(x) = 0` numéricamente con el método de Newton-Raphson.

```
x_{n+1} = x_n - f(x_n) / f'(x_n)

Donde f'(x) ≈ (f(x+h) - f(x-h)) / (2h),  h = 1e-7

Criterio de parada: |f(x)| < 1e-10  ó  max 100 iteraciones
Semillas probadas: x₀ ∈ {0, 1, -1, 2, -2, 5, -5, 10}
```

> **Limitación**: Encuentra una raíz real. Para álgebra exacta con todas las raíces, usar el **Pro-CAS Engine** (Sección 8).

---

## 8. Pro-CAS Engine

★ Motor de álgebra computacional completo diseñado para embedded. Evolución del CAS-Lite original. Incluye resolución de ecuaciones polinomiales con resultado exacto, derivación simbólica (17 reglas), integración simbólica (Slagle heurístico), simplificación multi-pass, y DAG inmutable con hash-consing. Toda la memoria CAS vive en PSRAM con `PSRAMAllocator`.

### 8.1 Arquitectura y Módulos

```
src/math/cas/
├── CASInt.h              ← BigInt híbrido: int64 fast-path + mbedtls_mpi
├── CASRational.h/.cpp    ← Fracción exacta overflow-safe (auto-GCD)
├── ConsTable.h           ← Hash-consing PSRAM: dedup de nodos
├── PSRAMAllocator.h      ← Allocator STL-compatible → ps_malloc / ps_free
├── SymExpr.h/.cpp        ← DAG inmutable (hash + weight)
├── SymExprArena.h        ← Bump allocator PSRAM + ConsTable integrado
├── SymPoly.h/.cpp        ← Rational (fracción exacta) + SymPoly (polinomio)
├── SymPolyMulti.h/.cpp   ← Polinomio multivariable + resultante Sylvester
├── ASTFlattener.h/.cpp   ← MathAST → SymExpr DAG (hash-consed)
├── SymDiff.h/.cpp        ← Derivación: 17 reglas (cadena, prod, trig, exp, log)
├── SymIntegrate.h/.cpp   ← Integración Slagle: tabla, linealidad, u-sub, partes
├── SymSimplify.h/.cpp    ← Simplificador fixed-point (8 passes, trig/log/exp)
├── SingleSolver.h/.cpp   ← Ecuación 1 var: lineal / cuadrática / N-R grado N
├── SystemSolver.h/.cpp   ← Sistema 2×2: gaussiana + NL (resultante)
├── OmniSolver.h/.cpp     ← Aislamiento analítico de variable
├── HybridNewton.h/.cpp   ← Newton-Raphson con Jacobiana simbólica
├── CASStepLogger.h/.cpp  ← StepVec PSRAM: INFO / FORMULA / RESULT / ERROR
├── SymToAST.h/.cpp       ← SolveResult → MathAST Natural Display
└── SymExprToAST.h/.cpp   ← SymExpr → MathAST (+C, ∫)
```

**Pipeline completo**:
```
Usuario: "3x+6=0"
  │
  ├─ splitAtEquals() → lhs="3x+6", rhs="0"
  ├─ Parser(lhs) → ExprNode AST izquierdo
  ├─ Parser(rhs) → ExprNode AST derecho
  │
  ├─ ASTFlattener::flatten(lhsNode, rhsNode) → SymPoly (lhs - rhs)
  │    SymPoly: { 1: 3, 0: 6 }   (3x + 6)
  │
  ├─ SingleSolver::solve(poly)
  │    grado 1 → x = -6/3 = -2
  │    Steps: INFO "Lineal" / FORMULA "x = -b/a" / RESULT "x = -2"
  │
  └─ SymToAST() → ExprNode "x = -2"  (Natural Display)
```

### 8.2 PSRAMAllocator

Allocator STL-compatible que redirige todas las allocations al heap PSRAM:

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

Tipos que lo usan:
- `CoeffMap` en `SymPoly` → `std::map<int, Rational, ..., PSRAMAllocator<...>>`
- `StepVec` en `CASStepLogger` → `std::vector<CASStep, PSRAMAllocator<CASStep>>`

### 8.3 Rational y SymPoly

#### `Rational` — Fracción exacta

```cpp
struct Rational {
    int64_t num, den;  // den siempre > 0, reducido por GCD automáticamente

    Rational(int64_t n = 0, int64_t d = 1);  // Normaliza: Rational(6,4) → {3,2}
    double   toDouble() const { return (double)num / den; }
    bool     isInteger() const { return den == 1; }
    bool     isZero()    const { return num == 0; }

    // Aritmética exacta:
    Rational operator+(Rational o) const;
    Rational operator-(Rational o) const;
    Rational operator*(Rational o) const;
    Rational operator/(Rational o) const;  // lanza si o.num==0
    bool     operator==(Rational o) const;
};
```

#### `SymPoly` — Polinomio simbólico

```cpp
using CoeffMap = std::map<int, Rational, std::less<int>,
                          PSRAMAllocator<std::pair<const int, Rational>>>;

struct SymPoly {
    CoeffMap coeffs;   // { grado → Rational }
    char     var;      // Variable simbólica ('x' por defecto)

    int      degree() const;               // Grado máximo con coef ≠ 0
    Rational coeff(int deg) const;         // Coef en 'deg' (0 si no existe)
    SymPoly  derivative() const;           // Derivada simbólica
    double   evaluate(double x) const;     // Evaluación numérica (Newton-Raphson)

    SymPoly  operator+(const SymPoly& o) const;
    SymPoly  operator-(const SymPoly& o) const;
    SymPoly  operator*(const Rational& r) const;  // Escalar
};
```

**Ejemplo**:
```cpp
// Representa: 3x² - 5x + 2
SymPoly p;
p.coeffs = { {2, {3,1}}, {1, {-5,1}}, {0, {2,1}} };
p.degree();       // 2
p.coeff(1);       // Rational{-5, 1}
p.evaluate(1.0);  // 3 - 5 + 2 = 0.0
```

### 8.4 ASTFlattener

Convierte un par de `ExprNode` (lhs, rhs de la ecuación) en un único `SymPoly` representando `lhs - rhs = 0`.

**Reglas de conversión (visitación recursiva)**:

| Nodo ExprNode | Acción |
|:--------------|:-------|
| `TEXT` número | `SymPoly` grado 0 con valor numérico |
| `TEXT` variable (ej. `x`) | `SymPoly` grado 1, coef 1 → `{1: 1/1}` |
| `ADD`, `SUB` | Flattear hijos recursivamente, sumar/restar SymPolys |
| `MUL` (escalar × poly) | Multiplicar SymPoly por Rational |
| `MUL` (poly × poly) | Producto polinomial término a término |
| `POW` (var^n, n integer) | `SymPoly` grado n, coef 1 |
| `FRACTION` | Rational exacta (num/den) como SymPoly grado 0 |
| `NEG` | Multiplicar SymPoly por `-1` |

**Ejemplo de conversión**:
```
AST: ADD(MUL(NUM:3, POW(VAR:x, NUM:2)), SUB(MUL(NUM:5, VAR:x), NUM:2))
Representa: 3x² + 5x - 2

ASTFlattener::visit():
  → ADD de:
     MUL(3, x²) → SymPoly {2: 3}
     SUB(5x, 2) → SymPoly {1: 5, 0: -2}
  → Resultado: SymPoly {2: 3, 1: 5, 0: -2}  ✓
```

### 8.5 SingleSolver

**Archivo**: `src/math/cas/SingleSolver.h/.cpp`  
**Entrada**: `SymPoly` (ecuación igualada a 0)  
**Salida**: `SolveResult` con `Rational` exacto + `CASStepLogger` con pasos

#### Estructura de resultado

```cpp
struct SolveResult {
    enum class Status {
        OK_ONE,     // Una solución real
        OK_TWO,     // Dos soluciones reales (cuadrática)
        COMPLEX,    // Sin solución real (discriminante < 0)
        INFINITE,   // Infinitas soluciones (0 = 0)
        NONE,       // Sin solución (0 = c, c≠0)
        ERROR       // Error interno
    };
    Status        status;
    Rational      root1, root2;
    CASStepLogger steps;
};
```

#### Lógica por grado

```
Grado 0: coef₀ = 0? → INFINITE : NONE

Grado 1: ax + b = 0
  x = -b/a   (Rational exacto)
  Steps generados:
    [INFO]    "Ecuación lineal de grado 1"
    [FORMULA] "x = -b / a"
    [RESULT]  "x = {valor}"

Grado 2: ax² + bx + c = 0
  Δ = b² - 4ac  (calculado como Rational)
  Δ < 0  → COMPLEX; steps: [INFO] "Discriminante negativo"
  Δ = 0  → x = -b/(2a);  [FORMULA] "Raíz doble: x = -b/(2a)"
  Δ > 0  → x₁ = (-b+√Δ)/(2a),  x₂ = (-b-√Δ)/(2a)
           √Δ exacta si Δ es cuadrado perfecto, double sinó
  Steps: [INFO] "Fórmula cuadrática", [FORMULA] "Δ = b²-4ac = {val}",
         [RESULT] "x₁ = {val}", [RESULT] "x₂ = {val}"

Grado ≥ 3: Newton-Raphson numérico
  Semillas: {0, 1, -1, 2, -2}
  Criterio convergencia: |f(x)| < 1e-10, máx 100 iteraciones
  Root → Rational{(int64_t)round(root*1e9), 1000000000} (aproximación racional)
  Steps: [INFO] "Newton-Raphson grado N", [RESULT] "x ≈ {val}"
```

### 8.6 SystemSolver

**Archivo**: `src/math/cas/SystemSolver.h/.cpp`  
**Entrada**: 2 expresiones de ecuación (strings), variable secundaria char  
**Salida**: `SystemResult` con `Rational` x, y + pasos

#### Resultado

```cpp
struct SystemResult {
    enum class Status { OK, INFINITE, INCONSISTENT, ERROR };
    Status        status;
    Rational      x, y;
    CASStepLogger steps;
};
```

#### Algoritmo de Gauss 2×2

```
Sistema:
  eq1: a₁·x + b₁·y = c₁
  eq2: a₂·x + b₂·y = c₂

Paso 1: Extraer coeficientes a₁, b₁, c₁, a₂, b₂, c₂
         usando ASTFlattener (identificación de variable principal/secundaria)

Paso 2: Eliminación de x:
  eq1' = eq1 × a₂   →  (a₁·a₂)x + (b₁·a₂)y = c₁·a₂
  eq2' = eq2 × a₁   →  (a₁·a₂)x + (b₂·a₁)y = c₂·a₁
  eq3  = eq1' - eq2' →  (b₁·a₂ - b₂·a₁)y = c₁·a₂ - c₂·a₁

Paso 3: Resolver y:
  D = b₁·a₂ - b₂·a₁   (Rational exacto)
  D = 0 y num≠0 → INCONSISTENT ("Sistema incompatible")
  D = 0 y num=0 → INFINITE    ("Sistema dependiente")
  D ≠ 0         → y = (c₁·a₂ - c₂·a₁) / D

Paso 4: Sustituir y en eq1 para obtener x:
  x = (c₁ - b₁·y) / a₁

Steps generados (5-7 pasos): INFO, FORMULA, INFO, INFO, RESULT×2
```

### 8.7 CASStepLogger

**Archivo**: `src/math/cas/CASStepLogger.h/.cpp`

Log de pasos del CAS con memoria en PSRAM. Se muestra al usuario en la pantalla de pasos de `EquationsApp`.

```cpp
enum class StepType { INFO, FORMULA, RESULT, ERROR };

struct CASStep {
    StepType    type;
    std::string text;  // Texto del paso
};

using StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>;

class CASStepLogger {
public:
    void        add(StepType type, const std::string& text);
    void        clear();                // ← LLAMAR en EquationsApp::end()
    size_t      count() const;
    const CASStep& get(size_t i) const;
};
```

**Tipos de paso y colores de renderizado en EquationsApp**:

| Tipo | Color | Uso |
|:-----|:------|:----|
| `INFO` | Gris / texto secundario | Descripción del método |
| `FORMULA` | Azul | Fórmula matemática aplicada |
| `RESULT` | Verde | Resultado obtenido |
| `ERROR` | Rojo | Error o caso degenerado |

**Gestión de memoria**:

`SolveResult::steps` y `SystemResult::steps` contienen `CASStepLogger` con PSRAM.  
`EquationsApp` los almacena como miembros (`_singleResult`, `_systemResult`).  
En `EquationsApp::end()`:
```cpp
_singleResult.steps.clear();  // ← Sin esto: leak PSRAM entre sesiones
_systemResult.steps.clear();
_resultHint = nullptr;        // ← Widget LVGL ya destruido, nular puntero
```

### 8.8 SymToAST

**Archivo**: `src/math/cas/SymToAST.h/.cpp`

Convierte resultados del CAS a árboles `ExprNode` para renderizado en Natural Display.

```cpp
// Rational → ExprNode
ExprNode* SymToAST::rationalToNode(Rational r) {
    if (r.isInteger()) return textNode(std::to_string(r.num));
    // Fracción: ExprNode FRACTION con num/den como TEXT hijos
    ExprNode* frac = new ExprNode(ExprNodeType::FRACTION);
    frac->children.push_back(textNode(std::to_string(r.num)));
    frac->children.push_back(textNode(std::to_string(r.den)));
    return frac;
}

// SolveResult → ExprNode "x = valor"
ExprNode* SymToAST::solveResultToNode(const SolveResult& r, char varName);

// SystemResult → ExprNode "x = val1, y = val2"
ExprNode* SymToAST::systemResultToNode(const SystemResult& r, char v1, char v2);
```

Ejemplos de output:
- `Rational{3, 1}` → `ExprNode TEXT "3"`
- `Rational{1, 2}` → `ExprNode FRACTION [TEXT "1" / TEXT "2"]` → renders $\frac{1}{2}$
- `SolveResult(OK_TWO, -1/3, -2)` → `"x₁ = -1/3, x₂ = -2"`

---

## 8.9 SymExpr — Representación simbólica de árbol (Phase 3)

**Archivos**: `src/math/cas/SymExpr.h`, `SymExpr.cpp`

Árbol de expresión simbólica arena-allocado para CAS:

| Tipo | Clase | Campos |
|------|-------|--------|
| `Num` | `SymNum` | `ExactVal value` |
| `Var` | `SymVar` | `char name` |
| `Neg` | `SymNeg` | `SymExpr* child` |
| `Add` | `SymAdd` | `SymExpr** terms`, `uint16_t count` |
| `Mul` | `SymMul` | `SymExpr** factors`, `uint16_t count` |
| `Pow` | `SymPow` | `SymExpr* base`, `SymExpr* exponent` |
| `Func` | `SymFunc` | `SymFuncKind kind`, `SymExpr* argument` |

**API clave**:
- `toString()` — representación textual legible
- `evaluate(char var, double val)` — evaluación numérica
- `isPolynomial()` — determina si es polinómica
- `toSymPoly(char var)` — convierte a SymPoly si es polinómica

**SymExprArena** (`SymExprArena.h`):
Bump allocator PSRAM. Bloques de 64KB, máximo 4 bloques.
```cpp
arena.create<SymNum>(ExactVal::fromInt(5));
arena.symVar('x');
arena.symPow(x, arena.symInt(2));
arena.reset();  // Libera todo
arena.currentUsed();   // bytes en uso
arena.blockCount();    // bloques activos
```

---

## 8.10 SymDiff — Derivación simbólica (Phase 3)

**Archivo**: `src/math/cas/SymDiff.h`, `SymDiff.cpp`

```cpp
static SymExpr* SymDiff::diff(SymExpr* expr, char var, SymExprArena& arena);
```

**Reglas implementadas**:

| Regla | Entrada | Resultado |
|-------|---------|-----------|
| Constante | `d/dx(c)` | `0` |
| Variable | `d/dx(x)` | `1` |
| Suma | `d/dx(u+v)` | `u' + v'` |
| Producto | `d/dx(u·v)` | `u'·v + u·v'` |
| Potencia | `d/dx(x^n)` | `n·x^(n-1)` |
| Cadena | `d/dx(f(g(x)))` | `f'(g(x))·g'(x)` |
| Seno | `d/dx(sin u)` | `cos(u)·u'` |
| Coseno | `d/dx(cos u)` | `-sin(u)·u'` |
| Tangente | `d/dx(tan u)` | `(1/cos²(u))·u'` |
| Exponencial | `d/dx(e^u)` | `e^u·u'` |
| Logaritmo | `d/dx(ln u)` | `u'/u` |
| Log₁₀ | `d/dx(log₁₀ u)` | `u'/(u·ln(10))` |
| ArcSin | `d/dx(arcsin u)` | `u'/√(1-u²)` |
| ArcCos | `d/dx(arccos u)` | `-u'/√(1-u²)` |
| ArcTan | `d/dx(arctan u)` | `u'/(1+u²)` |

---

## 8.11 SymSimplify — Simplificador algebraico (Phase 3)

**Archivo**: `src/math/cas/SymSimplify.h`, `SymSimplify.cpp`

```cpp
static SymExpr* SymSimplify::simplify(SymExpr* expr, SymExprArena& arena);
```

**Reglas de simplificación**:
- `0 + x → x`, `x + 0 → x`
- `1 · x → x`, `x · 1 → x`
- `0 · x → 0`
- `x^0 → 1`, `x^1 → x`
- Doble negación: `--x → x`
- Plegado de constantes (operaciones entre literales)
- Colapso de colecciones unitarias (`Add([x]) → x`)

---

## 8.12 SymExprToAST — Conversión a MathAST (Phase 3)

**Archivo**: `src/math/cas/SymExprToAST.h`, `SymExprToAST.cpp`

```cpp
static vpam::NodePtr SymExprToAST::convert(const SymExpr* expr);
```

Convierte un árbol `SymExpr` (resultado de diff/simplify) de vuelta a
`MathAST` (`NodePtr`) para renderizado 2D en `MathCanvas`:

| SymExpr | MathAST |
|---------|---------|
| `SymNum` | `NodeNumber` |
| `SymVar` | `NodeVariable` |
| `SymNeg` | `NodeRow` con `OpKind::Sub` |
| `SymAdd` | `NodeRow` con operadores `+/-` |
| `SymMul` | `NodeRow` con operador `×` |
| `SymPow` | `NodePower` |
| `SymFunc` | `NodeFunction` |

---

## 8.13 CalculusApp — App de Cálculo unificada

**Archivos**: `src/apps/CalculusApp.h`, `CalculusApp.cpp`

App LVGL-native que unifica derivadas e integrales simbólicas en una sola interfaz con cambio de modo por pestañas. Reemplaza las antiguas apps separadas CalculusApp (solo derivadas) e IntegralApp.

### Modos

| Modo | Pestaña | Color acento | Pipeline |
|------|---------|:------------:|----------|
| `DERIVATIVE` | "d/dx Derivar" | Naranja `#E05500` | SymDiff → SymSimplify → SymExprToAST::convert() |
| `INTEGRAL` | "∫dx Integrar" | Púrpura `#6A1B9A` | SymIntegrate → SymSimplify → SymExprToAST::convertIntegral() (+C) |

### Pipeline completo (Derivadas)

```
                   ┌──────────────┐
 Teclado ──► VPAM │ MathCanvas   │  Nodo visual (2D)
                   │  (NodeRow)   │
                   └──────┬───────┘
                          │ ASTFlattener
                          ▼
                   ┌──────────────┐
                   │  SymExpr*    │  Árbol simbólico (arena)
                   └──────┬───────┘
                          │ SymDiff::diff()
                          ▼
                   ┌──────────────┐
                   │ Raw Deriv    │  Derivada sin simplificar
                   └──────┬───────┘
                          │ SymSimplify::simplify()
                          ▼
                   ┌──────────────┐
                   │ Simplified   │  Derivada simplificada
                   └──────┬───────┘
                          │ SymExprToAST::convert()
                          ▼
                   ┌──────────────┐
                   │ MathCanvas   │  Renderizado 2D de f'(x)
                   └──────────────┘
```

### Pipeline completo (Integrales)

```
                   ┌──────────────┐
 Teclado ──► VPAM │ MathCanvas   │  Nodo visual (2D)
                   │  (NodeRow)   │
                   └──────┬───────┘
                          │ ASTFlattener
                          ▼
                   ┌──────────────┐
                   │  SymExpr*    │  Árbol simbólico (arena)
                   └──────┬───────┘
                          │ SymIntegrate::integrate()
                          ▼
                   ┌──────────────┐
                   │ Raw Integral │  Antiderivada (o nullptr)
                   └──────┬───────┘
                          │ SymSimplify::simplify()
                          ▼
                   ┌──────────────┐
                   │ Simplified   │  Integral simplificada
                   └──────┬───────┘
                          │ SymExprToAST::convertIntegral()
                          ▼
                   ┌──────────────┐
                   │ MathCanvas   │  Renderizado 2D: F(x) + C
                   └──────────────┘
```

### Estados de la app

| Estado | Descripción |
|--------|-------------|
| `EDITING` | Entrada de expresión con MathCanvas + cursor |
| `COMPUTING` | Spinner + mensaje aleatorio específico del modo |
| `RESULT` | Muestra f(x) y f'(x) o F(x)+C con renderizado 2D |
| `STEPS` | Lista scrollable de pasos de derivación/integración |

### Dos ramas de resultado (modo Integral)

1. **Integral resuelta** (`_integralFound = true`):
   - Label: "F(x) ="
   - Usa `SymExprToAST::convertIntegral()` que añade `+ C` automáticamente
   - Acento verde en barra de estado

2. **Integral no resuelta** (`_integralFound = false`):
   - Label: "∫f(x)dx ="
   - Muestra expresión original con símbolo ∫
   - Acento naranja en barra de estado

### Funciones soportadas

Todas las funciones del teclado NumOS: `sin`, `cos`, `tan`, `arcsin`, `arccos`,
`arctan`, `ln`, `log`, `√`, `π`, `e`, fracciones, potencias, paréntesis.

### Controles

| Tecla | Acción |
|-------|--------|
| ENTER / = | Calcular derivada o integral |
| AC | Limpiar / volver atrás |
| GRAPH | Cambiar modo: d/dx ↔ ∫dx |
| SHIFT+trig | Funciones inversas (arcsin, etc.) |
| SHOW_STEPS | Ver pasos del cálculo |
| ↑/↓ | Scroll en vista de pasos |

### Gestión de memoria

- `SymExprArena _arena` como miembro de clase (no stack-local)
- `_arena.reset()` al inicio de cada cómputo
- `_arena.reset()` en `end()` para limpiar PSRAM al salir
- `_casSteps.clear()` en `end()` para liberar StepVec PSRAM
- Resultado `_resultExpr` es arena-owned — válido hasta el siguiente reset

---

## 8.14 SymIntegrate — Integración simbólica (Phase 6B)

**Archivo**: `src/math/cas/SymIntegrate.h`, `SymIntegrate.cpp`

```cpp
static const SymExpr* SymIntegrate::integrate(
    SymExprArena& arena, const SymExpr* expr, const char* var);
```

Motor de integración simbólica heurístico inspirado en el algoritmo de Slagle.
Retorna `nullptr` si no puede encontrar una antiderivada cerrada.

**Estrategias (en orden de aplicación)**:

| # | Estrategia | Ejemplo |
|---|-----------|---------|
| 1 | **Tabla directa** | ∫sin(x)dx = -cos(x), ∫eˣdx = eˣ, ∫1/x dx = ln(x) |
| 2 | **Linealidad** | ∫(af + bg)dx = a∫fdx + b∫gdx |
| 3 | **Potencias** | ∫xⁿdx = xⁿ⁺¹/(n+1) para n≠-1 |
| 4 | **u-sustitución** | ∫f(g(x))·g'(x)dx → ∫f(u)du con u=g(x) |
| 5 | **Partes (LIATE)** | ∫u·dv = uv - ∫v·du, prioridad: Log > InvTrig > Alg > Trig > Exp |

**Pipeline completo**:
```
Entrada: SymExpr* (expresión a integrar)
           │
           ▼
  ┌────────────────────┐
  │ Tabla directa      │  ¿Coincide con patrón conocido?
  └────────┬───────────┘
           │ no
           ▼
  ┌────────────────────┐
  │ Linealidad         │  ¿Es suma/resta? → integrar cada término
  └────────┬───────────┘
           │ no
           ▼
  ┌────────────────────┐
  │ u-sustitución      │  ¿Tiene forma f(g(x))·g'(x)?
  └────────┬───────────┘
           │ no
           ▼
  ┌────────────────────┐
  │ Partes (LIATE)     │  Asignar u, dv según prioridad LIATE
  └────────┬───────────┘
           │ no
           ▼
       nullptr  (integral no resuelta → se muestra como ∫)
```

---

## 9. Suite de Tests CAS

**Archivo**: `tests/CASTest.h/.cpp`  
**Activar**: Descomentar `-DCAS_RUN_TESTS` y `+<../tests/CASTest.cpp>` en `platformio.ini`

```
Fase A — SymPoly (12 tests):
  - Rational: aritmética, normalización (6/4→3/2), división por cero
  - SymPoly: coeficientes, grado, suma, resta, multiplicación escalar
  - SymPoly: derivada derivation polynomial

Fase B — ASTFlattener (15 tests):
  - Conversión de literales numéricos y variables
  - Expresiones lineales: ax+b, con fracciones, con negación
  - Expresiones cuadráticas: x²+2x+1, (x+1)(x-1) = x²-1
  - Casos degenerados: constante pura, variable con coef fraccionario

Fase C — SingleSolver (16 tests):
  - Grado 0: INFINITE / NONE
  - Grado 1: x = entero, x = fracción, x = negativo
  - Grado 2: discriminante 0, >0 (enteros), >0 (irracionales → double), &lt;0
  - Grado 2: fórmula cuadrática normalizada 2x²-5x+2=0 → x=2, x=1/2
  - Pasos: verificar que SingleSolver genera exactamente N pasos esperados

Fase D — SystemSolver (10 tests):
  - Sistema con solución entera única
  - Sistema con solución fraccionaria
  - Sistema inconsistente (D=0, num≠0)
  - Sistema dependiente (D=0, num=0)
  - Sistema asimétrico (coeficiente cero en una ecuación)

Fase E — SymDiff + SymSimplify + SymExprToAST (32 tests):
  - Derivadas de polinomios: constante, lineal, cuadrática, cúbica
  - Derivadas trigonométricas: sin, cos, tan + regla de la cadena
  - Derivada de exponencial, logaritmo, arcfunciones
  - Regla del producto: x·sin(x), x²·ln(x)
  - SymSimplify: 0+x→x, 1·x→x, 0·x→0, x^0→1, x^1→x
  - SymExprToAST: conversión correcta de tipos de nodo

Fase F — CalculusStressTest (50 iteraciones):
  - 25 expresiones distintas × 2 repeticiones
  - Verificar arena.reset() libera memoria entre iteraciones
  - Verificar arena blocks ≤ 4 (bounded)
  - Verificar toString() y SymExprToAST::convert() no crashean

TOTAL: 85+ tests — todos passing ✅
```

**Output ejemplo** (Serial Monitor, modo test):
```
[CAS TEST] Fase A: SymPoly.............. 12/12 OK
[CAS TEST] Fase B: ASTFlattener......... 15/15 OK
[CAS TEST] Fase C: SingleSolver......... 16/16 OK
[CAS TEST] Fase D: SystemSolver......... 10/10 OK
[CAS TEST] Fase E: SymDiff.............. 32/32 OK
[CAS TEST] Fase F: StressTest........... 50/50 OK
```

---

*NumOS — Open-source scientific calculator OS for ESP32-S3 N16R8.*
