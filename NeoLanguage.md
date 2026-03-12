# NeoLanguage — Official Language Bible

> **File extension:** `.nl` (NeoLanguage)
> **Version:** Phase 7 — Differential Equations, Optimization, Data Persistence, NeoGUI & Physics Constants
> **Target platform:** NeoCalculator / NumOS (ESP32-S3)

---

## Quick-Start Cheat Sheet
*(If you already know Python, you can start using NeoLanguage in 30 seconds.)*

```nl
# Arithmetic & variables
x = 3
y = x^2 + 2*x + 1          # x^2 = 9+6+1 = 16

# Lists and matrices
v  = [1, 2, 3]              # 1-D list
v2 = v * 2                  # => [2, 4, 6]  (vectorised)
M  = [[1, 2], [3, 4]]       # 2-D matrix
M[0]                        # => [1, 2]
M[1, 0]                     # => 3

# Symbolic math (Wolfram-style)
diff(x^3, x)                # => (3*(x^2))
integrate(sin(x), x)        # => -cos(x)
solve(x^2 - 4, x)           # prints: x = 2  and  x = -2

# Numeric evaluation
n(pi)                       # => 3.141592654
n(sqrt(2))                  # => 1.414213562

# Constants
pi, e, phi                  # pi=3.1415…, e=2.7182…, phi=1.6180…

# Loops and functions
for i in range(5):
    print(i)                # 0 1 2 3 4

def square(n): return n*n
map(square, [1,2,3,4])      # => [1, 4, 9, 16]

# Plot
plot(sin(x), -6.28, 6.28)
```

---

## Table of Contents

1. [Core Philosophy](#1-core-philosophy)
2. [Syntax Guide](#2-syntax-guide)
3. [Data Types](#3-data-types)
4. [Operators](#4-operators)
5. [Control Flow](#5-control-flow)
6. [Functions](#6-functions)
7. [Linear Algebra: Lists & Matrices](#7-linear-algebra-lists--matrices)
8. [Symbolic vs Numeric](#8-symbolic-vs-numeric)
9. [Standard Library Reference](#9-standard-library-reference)
10. [Functional Tools](#10-functional-tools)
11. [**Data Science**](#11-data-science)
12. [**Physics & Units**](#12-physics--units)
13. [**Advanced Calculus**](#13-advanced-calculus)
14. [Code Examples](#14-code-examples)
15. [**Differential Equations**](#15-differential-equations)
16. [**Optimization**](#16-optimization)
17. [**Data Persistence: JSON & CSV**](#17-data-persistence-json--csv)
18. [**NeoGUI: Building Custom Interfaces**](#18-neogui-building-custom-interfaces)
19. [**Physics Constants Database**](#19-physics-constants-database)
20. [**Master Engineering Example**](#20-master-engineering-example)
21. [**Modules and Namespaces**](#21-modules-and-namespaces)
22. [**Signal Processing**](#22-signal-processing)
23. [**Advanced Linear Algebra**](#23-advanced-linear-algebra)
24. [**Performance Profiling**](#24-performance-profiling)
25. [Keyboard Reference](#25-keyboard-reference)
26. [Known Limitations](#26-known-limitations)

---

## 1. Core Philosophy

NeoLanguage is a **Symbolic-Imperative hybrid** designed for a scientific calculator.
It blends two powerful ideas:

| Concept | Inspiration | Behaviour |
|---------|-------------|-----------|
| Imperative scripting | Python | Variables, loops, conditionals, functions |
| Symbolic mathematics | Wolfram Language | Undefined variables become symbolic expressions |
| Data structures | NumPy / MATLAB | Lists, matrices with vectorised arithmetic |

### The Symbolic Twist

In most languages, using an undefined variable is an error.  In NeoLanguage it
is the foundation of the CAS engine:

```nl
# x is not yet defined, so NeoLanguage treats it symbolically
expr = x^2 + 2*x + 1
diff(expr, x)      # => (2*x + 2)  (exact symbolic derivative)
```

### Design Principles

* **Lowercase everything** — all built-in functions use lowercase names, ideal
  for calculator keyboards.
* **No surprises** — the language never silently converts types; it escalates to
  symbolic where needed.
* **Safety first** — infinite loops are capped at 10 000 iterations; the CAS
  arena is soft-reset when memory exceeds 70 %.
* **Decoupled graphics** — `plot()` uses the CAS evaluator directly, not the
  Grapher app string parser.  Upgrading GrapherApp never breaks NeoLanguage
  programs.

---

## 2. Syntax Guide

### 2.1 Indentation

NeoLanguage uses **Python-style significant indentation**.  Indent with 4 spaces
(press **F1** on the calculator).  Mixed tabs/spaces are not supported.

```nl
if x > 0:
    print("positive")
else:
    print("non-positive")
```

### 2.2 Comments

Lines starting with `#` are comments.

```nl
# This is a comment
x = 5  # inline comment
```

### 2.3 Variable Assignment

| Syntax | Meaning |
|--------|---------|
| `x = 3` | Immediate assignment: evaluate RHS now |
| `f := x^2 + 1` | Delayed assignment: store as-is (Wolfram-style) |

### 2.4 Statements

Each non-empty line is a statement.  The last expression in a block is its
return value.  In the REPL (console), non-`None` results are prefixed with
`=> `.

---

## 3. Data Types

| Type | Description | Example |
|------|-------------|---------|
| `None` | Absence of value | `x = None` |
| `bool` | Boolean | `True`, `False` |
| `number` | IEEE-754 double | `3.14`, `-0.5` |
| `exact` | Exact rational (CASRational) | `1/3` (from integer ops) |
| `symbolic` | CAS expression tree (SymExpr) | `sin(x)`, `x^2 + 1` |
| `function` | User-defined function | `def f(x): return x*x` |
| `list` | Ordered collection of values | `[1, 2, 3]`, `[[1,2],[3,4]]` |
| `native_function` | Built-in C++ callable | `regress(...)` return value |
| `quantity` | Physical quantity with units | `unit(5, "m")` |

### Type Arithmetic Escalation

```
number    OP number    → number    (double arithmetic)
exact     OP exact     → exact     (rational arithmetic)
exact     OP number    → number    (promote to double)
any       OP symbolic  → symbolic  (CAS factory)
list      + list       → list      (concatenation)
list      OP scalar    → list      (element-wise vectorisation)
quantity  OP quantity  → quantity  (dimensional arithmetic + unit checking)
quantity  OP scalar    → quantity  (scale magnitude)
```

---

## 4. Operators

### Arithmetic

| Operator | Operation |
|----------|-----------|
| `+` | Addition / List concatenation |
| `-` | Subtraction / Negation / List element-wise subtract |
| `*` | Multiplication / List scalar multiply |
| `/` | Division |
| `^` or `**` | Exponentiation |

### Comparison

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<`, `<=` | Less, less-or-equal |
| `>`, `>=` | Greater, greater-or-equal |

### Logical

| Operator | Meaning |
|----------|---------|
| `and` | Logical AND (short-circuit) |
| `or` | Logical OR (short-circuit) |
| `not` | Logical NOT |

### Assignment

| Operator | Meaning |
|----------|---------|
| `=` | Immediate assignment |
| `:=` | Delayed (symbolic) assignment |

### Subscript

| Syntax | Meaning |
|--------|---------|
| `L[i]` | Index element `i` of list `L` (0-based) |
| `M[r, c]` | Element at row `r`, column `c` of matrix `M` |
| `M[r]` | Row `r` of matrix `M` (returns a list) |
| `L[-1]` | Last element (negative indexing) |

---

## 5. Control Flow

### if / elif / else

```nl
x = 7
if x > 10:
    print("big")
elif x > 5:
    print("medium")
else:
    print("small")
```

### while

```nl
n = 1
while n < 100:
    n = n * 2
print(n)   # => 128
```

### for-in / range

```nl
total = 0
for i in range(5):
    total = total + i
print(total)   # => 10
```

`range(n)` generates integers 0 … n−1.  Maximum 10 000 iterations.

### for-in over a list

```nl
items = [10, 20, 30]
for v in items:
    print(v)
# prints 10, 20, 30
```

---

## 6. Functions

### Defining Functions

```nl
def square(x):
    return x * x

square(7)   # => 49
```

### One-liner Syntax

```nl
cube(x) := x^3
cube(3)   # => 27
```

### Closures

Functions capture their enclosing scope:

```nl
a = 10
def addA(x):
    return x + a

addA(5)   # => 15
```

---

## 7. Linear Algebra: Lists & Matrices

### 7.1 Creating Lists

```nl
v = [1, 2, 3, 4, 5]       # literal list
z = zeros(5)               # [0, 0, 0, 0, 0]
o = ones(3)                # [1, 1, 1]
```

### 7.2 List Operations

| Operation | Example | Result |
|-----------|---------|--------|
| Concatenation | `[1,2] + [3,4]` | `[1, 2, 3, 4]` |
| Scalar multiply | `[1,2,3] * 2` | `[2, 4, 6]` |
| Scalar add | `[1,2,3] + 10` | `[11, 12, 13]` |
| Element-wise mul | `[1,2,3] * [4,5,6]` | `[4, 10, 18]` |
| Index | `v[0]` | first element |
| Length | `len(v)` | number of elements |
| Sum | `sum(v)` | sum of all elements |

### 7.3 Creating Matrices

A matrix is a **list of row lists**:

```nl
A = [[1, 2, 3],
     [4, 5, 6],
     [7, 8, 9]]

A[0]       # => [1, 2, 3]   (first row)
A[1, 2]    # => 6           (row 1, col 2)
```

### 7.4 Matrix Arithmetic

```nl
A = [[1, 2], [3, 4]]
B = [[5, 6], [7, 8]]

# Element-wise operations (vectorised rows):
# A + B: [  A[0]+B[0],  A[1]+B[1]  ]
#       = [[6,8],[10,12]]

# Scalar multiply:
A_times_2 = [[2, 4], [6, 8]]
```

### 7.5 Dot Product

```nl
a = [1, 2, 3]
b = [4, 5, 6]
dot(a, b)    # => 1*4 + 2*5 + 3*6 = 32
```

### 7.6 Solving a 2×2 System via Cramer's Rule

```nl
# Solve:  2x + 3y = 8
#          x - 2y = -3
def cramer2(a11, a12, a21, a22, b1, b2):
    det_A = a11*a22 - a12*a21
    x = (b1*a22 - b2*a12) / det_A
    y = (a11*b2 - a21*b1) / det_A
    return [x, y]

sol = cramer2(2, 3, 1, -2, 8, -3)
print("x =", sol[0], "  y =", sol[1])
# x = 1   y = 2
```

---

## 8. Symbolic vs Numeric

### 8.1 Why sqrt(8) Returns 2*sqrt(2)

NeoLanguage uses a CAS (Computer Algebra System) for symbolic computations.
When you type:

```nl
simplify(sqrt(8))
```

The CAS recognises that `√8 = √(4·2) = 2√2` — an **exact** symbolic
simplification.  This is the *exact* answer, not a floating-point approximation.

To get a decimal value, use `n()`:

```nl
n(sqrt(8))   # => 2.828427125
```

### 8.2 Constants

| Name | Value | Description |
|------|-------|-------------|
| `pi` | 3.14159265… | Circle constant |
| `e` | 2.71828182… | Euler's number |
| `phi` | 1.61803398… | Golden ratio (1+√5)/2 |
| `inf` | ∞ | Positive infinity |

### 8.3 The n() Numeric Evaluator

`n(expr)` converts any expression or constant to its numeric double value.

```nl
n(pi)         # => 3.141592654
n(exp(1))     # => 2.718281828
n(sqrt(2))    # => 1.414213562
n(sin(pi/6))  # => 0.5
```

`n(expr, digits)` — `digits` hints at desired precision (1–15); the actual
precision is always IEEE-754 double (~15 significant digits).

```nl
n(pi, 15)     # => 3.14159265358979
```

### 8.4 Keeping Expressions Symbolic

If you want to manipulate a formula symbolically and only evaluate at the end:

```nl
expr = x^3 - 3*x + 2
dp   = diff(expr, x)     # => (3*(x^2) - 3)  — still symbolic
n(dp)                    # evaluates at x=0 → -3
```

---

## 9. Standard Library Reference

All built-in function names are **lowercase**.

### 9.1 Symbolic Calculus & Algebra

---

#### `diff(expr, var)`

Compute the symbolic derivative of `expr` with respect to `var`.

```nl
diff(x^3, x)          # => (3 * (x^2))
diff(sin(x) * x, x)   # => (sin(x) + (x * cos(x)))
```

---

#### `integrate(expr, var)`

Compute the symbolic indefinite integral of `expr` with respect to `var`.

```nl
integrate(x^2, x)     # => ((x^3) / 3)
integrate(cos(x), x)  # => sin(x)
```

Returns an unevaluated `∫` expression if no closed-form antiderivative is found.

---

#### `solve(expr, var)`

Find the roots of `expr = 0` with respect to `var`.

```nl
solve(x^2 - 4, x)    # prints: x = 2  and  x = -2
solve(2*x + 6, x)    # prints: x = -3
```

---

#### `simplify(expr)`

Apply algebraic simplification rules.

```nl
simplify(sin(x)^2 + cos(x)^2)   # => 1
simplify(ln(exp(x)))             # => x
```

---

#### `expand(expr)`

Expand products and powers using the distributive law.

```nl
expand((x + 1)^2)   # => (x^2 + 2*x + 1)
```

---

### 9.2 Numeric Evaluation

---

#### `n(expr)` / `n(expr, digits)`

Force numeric evaluation of any expression.

```nl
n(pi)         # => 3.141592654
n(sqrt(2))    # => 1.414213562
n(sin(pi))    # => 1.2246e-16 (≈ 0)
```

---

### 9.3 Math Primitives

#### Trigonometry

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `asin(x)` / `arcsin(x)` | Arcsine |
| `acos(x)` / `arccos(x)` | Arccosine |
| `atan(x)` / `arctan(x)` | Arctangent |
| `atan2(y, x)` | Two-argument arctangent |

#### Hyperbolic

| Function | Description |
|----------|-------------|
| `sinh(x)` | Hyperbolic sine |
| `cosh(x)` | Hyperbolic cosine |
| `tanh(x)` | Hyperbolic tangent |
| `asinh(x)` / `arcsinh(x)` | Inverse hyperbolic sine |
| `acosh(x)` / `arccosh(x)` | Inverse hyperbolic cosine |
| `atanh(x)` / `arctanh(x)` | Inverse hyperbolic tangent |

#### Exponential & Logarithms

| Function | Description |
|----------|-------------|
| `exp(x)` | e^x |
| `ln(x)` / `log(x)` | Natural logarithm |
| `log10(x)` | Base-10 logarithm |
| `log2(x)` | Base-2 logarithm |

#### Roots & Rounding

| Function | Description |
|----------|-------------|
| `sqrt(x)` | Square root |
| `cbrt(x)` | Cube root |
| `abs(x)` | Absolute value |
| `floor(x)` | Round down |
| `ceil(x)` | Round up |
| `round(x)` / `round(x, d)` | Round to `d` decimal places |
| `sign(x)` | Sign: −1, 0, or +1 |
| `max(a, b, ...)` | Maximum |
| `min(a, b, ...)` | Minimum |

---

### 9.4 List Operations

| Function | Description |
|----------|-------------|
| `len(list)` | Number of elements |
| `sum(list)` | Sum of all elements |
| `dot(a, b)` | Dot product of two numeric lists |
| `zeros(n)` | List of `n` zeros |
| `ones(n)` | List of `n` ones |

---

### 9.5 High-Level Graphics

---

#### `plot(expr, xmin, xmax)`

Enter **Graphics Mode** and render the function `expr` over `[xmin, xmax]`.

```nl
plot(sin(x), -6.28, 6.28)
plot(x^2 - 4, -5, 5)
plot(exp(-x^2), -3, 3)
```

---

#### `clear_plot()`

Discard any pending plot request.

---

### 9.6 System & Utilities

---

#### `print(args...)`

Send values to the console output area.

```nl
print("Hello, world!")
print("x =", x, "y =", y)
```

---

#### `type(val)`

Print the type name of `val` and return its ordinal (0–6).

| Ordinal | Type name |
|---------|-----------|
| 0 | None |
| 1 | bool |
| 2 | number |
| 3 | exact |
| 4 | symbolic |
| 5 | function |
| 6 | list |
| 7 | native_function |
| 8 | quantity |

---

#### `vars()`

List all variables defined in the current scope.

---

#### `input_num("prompt")`

Request a numeric value from the user (UI hook).

---

#### `msg_box("title", "content")`

Display a message in the console.

---

### 9.7 Statistics (Phase 5)

| Function | Description |
|----------|-------------|
| `mean(list)` | Arithmetic mean |
| `median(list)` | Median value |
| `stddev(list)` | Sample standard deviation |
| `variance(list)` | Sample variance |
| `sort(list)` | Sorted copy (ascending) |
| `regress(x, y, model)` | Regression; returns callable function |
| `pdf_normal(x, mu, sigma)` | Gaussian probability density |
| `cdf_normal(x, mu, sigma)` | Gaussian cumulative distribution |
| `factorial(n)` | n! |
| `ncr(n, r)` | Binomial coefficient C(n,r) |
| `npr(n, r)` | Permutations P(n,r) |

### 9.8 Units (Phase 5)

| Function | Description |
|----------|-------------|
| `unit(value, "unit")` | Create a physical quantity |
| `convert(quantity, "unit")` | Convert to a different unit |

### 9.9 Advanced Calculus (Phase 5)

| Function | Description |
|----------|-------------|
| `limit(expr, var, point)` | Numerical limit approximation |
| `taylor(expr, var, point, order)` | Taylor polynomial coefficients |
| `sigma(expr, var, start, end)` | Numerical summation |
| `table(func, start, end, step)` | Sample a function into a list |

---

## 10. Functional Tools

### 10.1 `map(func, list)`

Apply a function to every element of a list and return a new list.

```nl
double(x) := x * 2
map(double, [1, 2, 3, 4])   # => [2, 4, 6, 8]

# With def syntax:
def square(x):
    return x * x
map(square, [1, 2, 3, 4, 5])   # => [1, 4, 9, 16, 25]
```

### 10.2 `filter(func, list)`

Keep only elements of a list for which `func` returns a truthy value.

```nl
positive(x) := x > 0
filter(positive, [-3, -1, 0, 2, 5])   # => [2, 5]
```

### 10.3 `range(n)` / `range(start, stop)`

Generate integer ranges for `for` loops.

```nl
for i in range(5):          # i = 0, 1, 2, 3, 4
    print(i)

for i in range(2, 7):       # i = 0..4  (stop - start = 5 iterations)
    print(i)
```

> **Note:** `range(start, stop)` currently iterates `stop - start` times,
> starting from 0.  The loop variable still starts at 0 in this version.

---

## 11. Data Science

NeoLanguage Phase 5 introduces a professional statistics engine for data analysis
on sensor data, experimental measurements, and any list of numbers.

### 11.1 Descriptive Statistics

All functions operate on a `list` value.

| Function | Description | Example |
|----------|-------------|---------|
| `mean(list)` | Arithmetic mean | `mean([1,2,3,4,5])` → `3` |
| `median(list)` | Median value | `median([1,3,2])` → `2` |
| `stddev(list)` | Sample standard deviation | `stddev([2,4,4,4,5,5,7,9])` → `2` |
| `variance(list)` | Sample variance | `variance([1,2,3])` → `1` |
| `sort(list)` | Sorted copy (ascending) | `sort([3,1,2])` → `[1, 2, 3]` |

```nl
data = [23, 45, 12, 67, 34, 89, 22, 55]
print("mean    =", mean(data))
print("median  =", median(data))
print("std dev =", stddev(data))
print("sorted  =", sort(data))
```

### 11.2 Regression Analysis

`regress(x_list, y_list, model)` fits a curve to your data and returns a
**callable function** (not global variables like TI-Basic).

#### Supported Models

| Model | Formula | Use when |
|-------|---------|----------|
| `"linear"` | `a + b·x` | Data grows steadily |
| `"quad"` | `a + b·x + c·x²` | Parabolic trend |
| `"exp"` | `a·eˢ(b·x)` | Exponential growth/decay |
| `"log"` | `a + b·ln(x)` | Logarithmic saturation |

#### The Killer Feature: Regression Returns a Function

```nl
days   = [1, 2, 3, 4, 5, 6, 7]
growth = [2.1, 4.2, 8.0, 16.3, 31.5, 64.0, 127.8]

# Fit an exponential model
f = regress(days, growth, "exp")

# Use f like any function to predict
print("Day 10 prediction:", f(10))
print("Day 14 prediction:", f(14))

# Generate a table of predicted values
predictions = table(f, 1, 14, 1)
print("Prediction table:", predictions)
```

The console output will show: `[regress] exp: a=1.053  b=0.693  R2=0.9999`

#### Evaluating Fit Quality (R²)

The R² value (printed by `regress`) measures goodness of fit (0–1, higher is better):
- R² > 0.99: excellent fit
- R² > 0.95: good fit
- R² < 0.8:  consider a different model

### 11.3 Probability Distributions

#### Normal Distribution

```nl
# Gaussian PDF and CDF
mu    = 0.0
sigma = 1.0

print("P(x=0)   =", pdf_normal(0, mu, sigma))   # => 0.3989
print("P(x<1.0) =", cdf_normal(1, mu, sigma))   # => 0.8413
print("P(x<-1)  =", cdf_normal(-1, mu, sigma))  # => 0.1587

# 95% confidence interval: P(-1.96 < X < 1.96) ≈ 0.95
lower = cdf_normal(-1.96, 0, 1)
upper = cdf_normal( 1.96, 0, 1)
print("95% CI coverage =", upper - lower)  # => 0.95
```

#### Combinatorics

```nl
print("10! =", factorial(10))       # => 3628800
print("C(10,3) =", ncr(10, 3))      # => 120
print("P(10,3) =", npr(10, 3))      # => 720
```

### 11.4 Complete Data Science Example

```nl
# Temperature sensor data (°C) over 8 hours
hours = [0, 1, 2, 3, 4, 5, 6, 7]
temps = [20.1, 22.3, 25.8, 30.2, 35.1, 38.9, 40.1, 39.5]

# Descriptive stats
print("Mean temp:", mean(temps))
print("Std dev:", stddev(temps))
print("Range:", sort(temps)[0], "to", sort(temps)[7])

# Fit quadratic model (temperature peaks then drops)
f = regress(hours, temps, "quad")

# Predict temperature at hour 3.5
print("Predicted at 3.5h:", f(3.5))

# Find peak temperature by sampling
best_h = 0
best_t = 0.0
for i in range(70):
    h = i / 10.0
    t = f(h)
    if t > best_t:
        best_t = t
        best_h = h
print("Estimated peak:", best_t, "at hour", best_h)
```

---

## 12. Physics & Units

NeoLanguage Phase 5 adds **dimensional analysis** — the ability to attach
physical units to numbers so that the interpreter tracks dimensions and
performs automatic conversions, just like Wolfram Alpha.

### 12.1 Creating Quantities

```nl
mass = unit(70, "kg")        # 70 kilograms
dist = unit(100, "m")        # 100 metres
time = unit(9.58, "s")       # 9.58 seconds (100m world record)

print(mass)   # => 70 kg
print(dist)   # => 100 m
```

### 12.2 Unit Arithmetic

#### Addition / Subtraction — Auto-convert to First Operand's Unit

```nl
a = unit(5, "m")
b = unit(200, "cm")
c = a + b               # Auto-converts 200cm → 2m
print(c)                # => 7 m

d = unit(1, "km") - unit(300, "m")
print(d)                # => 0.7 km
```

#### Multiplication / Division — Units Combine

```nl
speed = unit(100, "m") / unit(9.58, "s")
print(speed)            # => 10.438 m/s

force = unit(70, "kg") * unit(9.81, "m/s2")
print(force)            # => 686.7 N

energy = unit(686.7, "N") * unit(10, "m")
print(energy)           # => 6867 J
```

#### Scalar Multiplication

```nl
v = unit(5, "m/s")
print(v * 2)            # => 10 m/s
print(3.6 * v)          # => 18 m/s  (still m/s)
```

### 12.3 Unit Conversion

```nl
speed_kmh = convert(speed, "km/h")
print(speed_kmh)           # => 37.58 km/h

speed_mph = convert(speed, "mph")
print(speed_mph)           # => 23.36 mph

temp_c = unit(100, "C")    # 100 degrees Celsius
temp_k = convert(temp_c, "K")
print(temp_k)              # => 373.15 K

temp_f = convert(temp_c, "F")
print(temp_f)              # => 212 F
```

### 12.4 Supported Units

#### Length
| Unit | Full name | SI factor |
|------|-----------|-----------|
| `m` | metre | 1 |
| `cm` | centimetre | 0.01 |
| `mm` | millimetre | 0.001 |
| `km` | kilometre | 1000 |
| `in` | inch | 0.0254 |
| `ft` | foot | 0.3048 |
| `yd` | yard | 0.9144 |
| `mi` | mile | 1609.344 |

#### Mass
| Unit | Full name | SI factor |
|------|-----------|-----------|
| `kg` | kilogram | 1 |
| `g` | gram | 0.001 |
| `lb` | pound | 0.45359 |
| `oz` | ounce | 0.02835 |

#### Time
| Unit | Full name | SI factor |
|------|-----------|-----------|
| `s` | second | 1 |
| `ms` | millisecond | 0.001 |
| `min` | minute | 60 |
| `h` | hour | 3600 |
| `d` | day | 86400 |

#### Force / Energy / Power
| Unit | Quantity | SI equivalent |
|------|----------|---------------|
| `N` | Newton (force) | kg·m/s² |
| `J` | Joule (energy) | kg·m²/s² |
| `W` | Watt (power) | kg·m²/s³ |
| `kJ`, `MJ` | kilo/megajoule | 1000/1000000 J |
| `kWh` | kilowatt-hour | 3.6×10⁶ J |
| `cal`, `kcal` | calorie / kilocalorie | 4.184 / 4184 J |

#### Pressure
| Unit | SI equivalent |
|------|---------------|
| `Pa` | 1 kg/(m·s²) |
| `bar` | 100,000 Pa |
| `atm` | 101,325 Pa |
| `psi` | 6894.76 Pa |

#### Temperature
| Unit | Conversion |
|------|-----------|
| `K` | Kelvin (SI base) |
| `C` | Celsius: K = °C + 273.15 |
| `F` | Fahrenheit: K = (°F + 459.67) × 5/9 |

### 12.5 Physics Script: Kinetic Energy with Units

```nl
# Kinetic energy: KE = 1/2 * m * v^2

mass = unit(1200, "kg")          # car mass
vel  = unit(27.78, "m/s")        # 100 km/h in m/s

# KE = 0.5 * mass * vel * vel
KE = mass * vel * vel * 0.5
print("Kinetic energy:", KE)      # => 462xxxxxxx J (approx 462 kJ)

# Convert to kJ for readability
KE_kJ = convert(KE, "kJ")
print("Kinetic energy:", KE_kJ)   # => 462.1 kJ
```

### 12.6 Physics Script: Free-Fall Distance

```nl
g    = unit(9.81, "m/s2")        # gravitational acceleration
t    = unit(3.0, "s")            # time

# d = 1/2 * g * t^2
# (note: unit arithmetic on t^2 requires two multiplications)
t2   = t * t                     # => Quantity with m/s2 units combined
dist = g * t2 * 0.5
print("Fall distance:", dist)     # => 44.145 m
```

---

## 13. Advanced Calculus

Phase 5 adds professional CAS functions for limits, Taylor series, and
numerical summation.

### 13.1 `limit(expr, var, point)`

Compute the numerical limit of `expr` as `var` → `point` using Richardson
extrapolation (two-sided).

```nl
# lim(x→0) sin(x)/x = 1
print(limit(sin(x)/x, x, 0))     # => 1.0

# lim(x→2) (x^2 - 4)/(x - 2) = 4
print(limit((x^2 - 4)/(x - 2), x, 2))   # => 4.0

# lim(x→∞) can be approximated with a large point
print(limit(1/x, x, 1e6))        # => ~0.000001
```

### 13.2 `taylor(expr, var, point, order)`

Generate the Taylor polynomial coefficients of `expr` around `point`.
Returns a **list of coefficients** `[c₀, c₁, c₂, …, cₙ]` where the
polynomial is `c₀ + c₁(x−a) + c₂(x−a)² + …`.

```nl
# Taylor series of sin(x) around x=0, order 5
coeffs = taylor(sin(x), x, 0, 5)
print("sin(x) Taylor coefficients:", coeffs)
# => [0, 1, 0, -0.1667, 0, 0.00833]
# Polynomial: 0 + x + 0 - x^3/6 + 0 + x^5/120

# Taylor series of e^x around x=0, order 4
e_coeffs = taylor(exp(x), x, 0, 4)
print("e^x Taylor coefficients:", e_coeffs)
# => [1, 1, 0.5, 0.1667, 0.0417]
```

**Evaluating the Taylor polynomial at a point:**

```nl
coeffs = taylor(sin(x), x, 0, 5)
# Evaluate at x = pi/6 (0.5236)
pt = 0.5236
approx = 0.0
for k in range(6):
    power = 1.0
    for j in range(k):
        power = power * pt
    approx = approx + coeffs[k] * power
print("sin(pi/6) Taylor approx:", approx)   # ≈ 0.5
print("sin(pi/6) exact:", sin(0.5236))       # ≈ 0.5
```

### 13.3 `sigma(expr, var, start, end)`

Numerical summation: Σ expr from var=start to var=end.

```nl
# Sum 1 + 2 + 3 + ... + 100 = 5050
print(sigma(x, x, 1, 100))          # => 5050

# Sum of squares: 1^2 + 2^2 + ... + 10^2 = 385
print(sigma(x^2, x, 1, 10))         # => 385

# Approximate pi via Leibniz series: pi/4 = 1 - 1/3 + 1/5 - ...
# sigma((-1)^x / (2*x+1), x, 0, 1000) * 4 ≈ pi
# (Use the formula manually since (-1)^x needs careful handling)
```

### 13.4 `table(func, start, end, step)`

Sample a function (especially a regression model) over a range and return
a list of `[x, y]` pairs.

```nl
# Regression model prediction table
days = [1, 2, 3, 4, 5]
vals = [2, 4, 8, 16, 32]
f = regress(days, vals, "exp")

# Generate predictions for days 1–10
tbl = table(f, 1, 10, 1)
for i in range(len(tbl)):
    row = tbl[i]
    print("day", row[0], ":", row[1])
```

---

## 14. Code Examples

### 14.1 Hello World

```nl
print("Hello, NeoWorld!")
```

### 14.2 Fibonacci Sequence

```nl
def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

for i in range(10):
    print(fib(i))
```

### 14.3 Symbolic Differentiation

```nl
p = x^4 - 3*x^2 + 2
dp  = diff(p, x)
ddp = diff(dp, x)
print("p =", p)
print("p' =", dp)
print("p'' =", ddp)
```

### 14.4 Solving an Equation

```nl
poly = x^3 - 6*x^2 + 11*x - 6
solve(poly, x)
```

### 14.5 Solving a Linear System (Cramer's Rule)

```nl
# Solve:  2x + 3y = 8
#          x - 2y = -3
def cramer2(a11, a12, a21, a22, b1, b2):
    det_A = a11*a22 - a12*a21
    x = (b1*a22 - b2*a12) / det_A
    y = (a11*b2 - a21*b1) / det_A
    return [x, y]

sol = cramer2(2, 3, 1, -2, 8, -3)
print("x =", sol[0], "  y =", sol[1])
```

### 14.6 Vector Operations

```nl
u = [1, 0, 0]
v = [0, 1, 0]

# Dot product
dot_uv = dot(u, v)    # => 0

# Scale and add
w = u * 3 + v * 4     # => [3, 4, 0]
print("w =", w)
print("len(w) =", len(w))
```

### 14.7 Taylor Expansion of sin(x)

```nl
# Approximation: sin(x) ≈ x - x^3/6 + x^5/120
taylor = x - x^3 / 6 + x^5 / 120
print("Taylor poly:", taylor)

# Compare symbolically
d1 = diff(taylor, x)
d2 = diff(d1, x)
d3 = diff(d2, x)
print("3rd derivative:", simplify(d3))

# Plot both
plot(sin(x), -4, 4)
plot(taylor, -4, 4)
```

### 14.8 Matrix Determinant (2x2)

```nl
def det2(M):
    return M[0, 0] * M[1, 1] - M[0, 1] * M[1, 0]

A = [[3, 7], [1, -4]]
print("det(A) =", det2(A))   # => 3*(-4) - 7*1 = -19
```

### 14.9 Newton's Method (manual)

```nl
# Solve sqrt(2) using Newton iterations for x^2 - 2 = 0
x0 = 1.0
for i in range(20):
    fx  = x0^2 - 2
    dfx = 2 * x0
    x0  = x0 - fx / dfx
print("sqrt(2) =", x0)
print("n(sqrt(2)) =", n(sqrt(2)))
```

### 14.10 Map and Filter Example

```nl
def square(x):
    return x * x

nums = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
squares = map(square, nums)
print("Squares:", squares)
# => [1, 4, 9, 16, 25, 36, 49, 64, 81, 100]

# Keep only squares > 25
big(x) := x > 25
big_squares = filter(big, squares)
print("Big squares:", big_squares)
# => [36, 49, 64, 81, 100]
```

### 14.11 Numeric Constants Demo

```nl
print("pi  =", n(pi))           # 3.141592654
print("e   =", n(e))            # 2.718281828
print("phi =", n(phi))          # 1.618033989
print("tau =", n(pi * 2))       # 6.283185307
```

### 14.12 Sensor Data Regression & Plot (Phase 5)

```nl
# Simulate sensor data: exponential growth
days   = [1, 2, 3, 4, 5, 6, 7, 8]
counts = [3, 6, 12, 23, 47, 94, 189, 378]

# Fit exponential model
f = regress(days, counts, "exp")

# Predict future values
print("Day 10:", f(10))
print("Day 14:", f(14))

# Plot regression curve
plot(f(x), 0, 15)
```

### 14.13 Physics: Projectile Motion (Phase 5)

```nl
# Projectile motion with units
g    = unit(9.81, "m/s2")
v0   = unit(20.0, "m/s")    # initial velocity
ang  = 0.7854               # 45 degrees in radians

v0x  = v0 * cos(ang)        # horizontal component (m/s)
v0y  = v0 * sin(ang)        # vertical component (m/s)

# Time of flight: T = 2*v0y/g
# (manual: g is Quantity, v0y is Quantity)
v0y_num = v0y.magnitude      # get SI value
g_num   = g.magnitude
T = 2 * v0y_num / g_num

# Range: R = v0x * T
R = v0x.magnitude * T

print("Time of flight:", T, "s")
print("Range:", R, "m")
```

### 14.14 Statistical Analysis of Sensor Data (Phase 5)

```nl
# Temperature readings over 20 samples
temps = [22.1, 22.5, 23.0, 22.8, 23.2, 23.5, 24.0, 23.8,
         24.2, 24.5, 24.8, 25.0, 25.2, 25.0, 24.8, 24.5,
         24.2, 24.0, 23.8, 23.5]

print("Mean     =", mean(temps))
print("Std Dev  =", stddev(temps))
print("Median   =", median(temps))
print("Min/Max  =", sort(temps)[0], "/", sort(temps)[19])

# Test for normal distribution
mu    = mean(temps)
sigma = stddev(temps)
p_within_1sigma = cdf_normal(mu + sigma, mu, sigma) -
                  cdf_normal(mu - sigma, mu, sigma)
print("P within 1σ:", p_within_1sigma)   # Should be ~0.68 for normal data
```

---

## 15. Differential Equations

NeoLanguage Phase 7 provides a numerical ODE solver using the 4th-order Runge-Kutta (RK4) method.

### `ndsolve(f, y0, x0, x1, steps=200)` — Numerical ODE Solver

Solve the first-order ODE **dy/dx = f(x, y)** numerically.

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | Function `(x, y) → number` | The derivative function |
| `y0` | Number | Initial condition: y(x0) |
| `x0` | Number | Start of integration interval |
| `x1` | Number | End of integration interval |
| `steps` | Number (optional) | Number of RK4 steps (default 200, max 10000) |

**Returns:** A `List` of `[x, y]` pairs that can be passed directly to `plot()`.

```nl
# Example: Exponential decay  dy/dx = -0.5*y,  y(0) = 10
def decay(x, y): return -0.5 * y
sol = ndsolve(decay, 10.0, 0, 8, 400)

# sol is a list of [x, y] pairs
print("y(8) ≈", sol[400][1])    # ≈ 0.67

# Plotting ndsolve results — plot the sampled y values
xs = map(def(p): return p[0], sol)
ys = map(def(p): return p[1], sol)
```

**Solving a second-order ODE** — reduce to a system using substitution:
```nl
# Harmonic oscillator: y'' + y = 0  →  y' = v,  v' = -y
# State: [y, v], solve two coupled 1st-order ODEs
def osc_y(x, y): return y   # dy/dx = v  (passed as second state)
# In NeoLanguage, model as one equation per call, or use the
# trick of calling ndsolve twice with the coupled equations.
```

### Special Mathematical Functions

| Function | Description |
|----------|-------------|
| `gamma(x)` | Gamma function Γ(x); generalises factorial: Γ(n) = (n-1)! for integer n |
| `beta(x, y)` | Beta function B(x,y) = Γ(x)·Γ(y)/Γ(x+y) |
| `erf(x)` | Error function; used in probability and heat-transfer problems |

```nl
# Stirling: n! ≈ gamma(n+1)
print(gamma(6))        # ≈ 120  (= 5!)
print(gamma(0.5))      # ≈ √π ≈ 1.7724539

# Error function (probability within ±z sigmas of mean)
p = erf(1.0 / sqrt(2))    # ≈ 0.6827 → 68.27% within ±1σ
print("P(-σ < X < σ) =", p)

# Beta distribution normalisation constant
print(beta(2, 3))      # ≈ 0.0833
```

---

## 16. Optimization

Find local minima and maxima of scalar functions using a golden-section search.

### `minimize(f, x0)` and `maximize(f, x0)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | Function `(x) → number` | The objective function to optimise |
| `x0` | Number | Initial guess for the optimum |

**Returns:** The x-value of the local optimum near `x0`.

> **Algorithm:** Golden-section search within [x0−10, x0+10].  
> For global optima, call multiple times with different starting points.

```nl
# Minimise f(x) = (x - 3)^2 + 1
def parabola(x): return (x - 3)^2 + 1
xmin = minimize(parabola, 0)
print("Minimum at x =", xmin)     # ≈ 3.0
print("f(xmin) =", parabola(xmin)) # ≈ 1.0

# Maximise the sinc function near x = 0
def sinc(x):
    if x == 0: return 1.0
    return sin(x) / x
xmax = maximize(sinc, 0.1)
print("Max sinc at x =", xmax)    # ≈ 0.0 (global max = 1 at x=0)

# Efficiency curve maximisation
def efficiency(x):
    # Quadratic efficiency model (peaks near x = 5)
    return -(x - 5)^2 + 25
xopt = maximize(efficiency, 3)
print("Peak efficiency at x =", xopt, " = ", efficiency(xopt), "%")
```

---

## 17. Data Persistence: JSON & CSV

NeoLanguage Phase 7 provides built-in functions for reading and writing files
on the device's LittleFS flash storage.

### File I/O

```nl
# Writing a file
h = open("/data.txt", "w")
write(h, "Temperature: 23.5\n")
write(h, "Humidity: 65\n")
close(h)

# Reading it back
h = open("/data.txt", "r")
content = read(h)
close(h)
print(content)
```

| Function | Description |
|----------|-------------|
| `open(path, mode)` | Open file. `mode`: `"r"` read, `"w"` write, `"a"` append. Returns handle (≥0) or -1 on error. |
| `read(handle)` | Read entire file as a String. |
| `write(handle, str)` | Write string to file. Returns bytes written. |
| `close(handle)` | Close the file. |

### JSON Serialization

```nl
# Encode a dictionary to JSON
data = {"sensor": "LM35", "temp": 23.5, "unit": "°C"}
json_str = json_encode(data)
print(json_str)     # {"sensor":"LM35","temp":23.5,"unit":"°C"}

# Write JSON to flash
h = open("/sensor.json", "w")
write(h, json_str)
close(h)

# Read and decode
h = open("/sensor.json", "r")
raw = read(h)
close(h)
restored = json_decode(raw)
print(restored["temp"])   # 23.5
```

| Function | Description |
|----------|-------------|
| `json_encode(val)` | Serialize Dict/List/Number/String/Bool/Null → JSON string |
| `json_decode(str)` | Parse JSON string → NeoValue |

### CSV Data Exchange

```nl
# Create a measurement matrix
measurements = [
    ["time", "voltage", "current"],
    [0.0, 3.30, 0.012],
    [0.1, 3.28, 0.011],
    [0.2, 3.25, 0.013]
]

# Export to CSV (readable by Excel / MATLAB)
ok = export_csv(measurements, "/measurements.csv")
print("Saved:", ok)

# Import CSV
data = import_csv("/measurements.csv")
print("Rows:", len(data))
print("Header:", data[0])   # ["time", "voltage", "current"]
```

| Function | Description |
|----------|-------------|
| `export_csv(matrix, path)` | Write List-of-Lists to CSV file. Returns `true` on success. |
| `import_csv(path)` | Read CSV file. Returns List-of-Lists (numbers auto-parsed). |

---

## 18. NeoGUI: Building Custom Interfaces

NeoGUI allows NeoLanguage scripts to build interactive LVGL dashboards
that remain active while the script runs.

### Component Functions

| Function | Description |
|----------|-------------|
| `gui_label(text)` | Add a static text label |
| `gui_button(label, "callback")` | Add a button; calls `callback()` when clicked |
| `gui_slider(min, max, "callback")` | Add a slider; calls `callback(value)` on change |
| `gui_input(label)` | Add a text input field with placeholder text |
| `gui_clear()` | Remove all GUI components |
| `gui_show()` | Make the GUI screen visible |

### Layout

Components are stacked vertically in a scrollable flex container.
Call `gui_clear()` + component functions + `gui_show()` to rebuild the interface.

### Example: Temperature Dashboard

```nl
# Define callback functions
def on_heat(v):
    print("Heater toggled:", v)

def on_setpoint(v):
    print("Setpoint:", v, "°C")

def on_plot_btn(v):
    # Solve ODE for thermal dynamics and show result
    def dT(t, T): return -0.1 * (T - 20)
    sol = ndsolve(dT, 100.0, 0, 60, 300)
    print("T(60s) =", sol[300][1])

# Build the GUI
gui_clear()
gui_label("=== Temperature Controller ===")
gui_slider(15, 80, "on_setpoint")
gui_button("Toggle Heater", "on_heat")
gui_button("Plot Thermal Decay", "on_plot_btn")
gui_show()
```

> **Note:** Callback names are strings matching NeoLanguage function names
> defined in the same script. The host app dispatches GUI events back to
> the interpreter between evaluation cycles.

---

## 19. Physics Constants Database

The `const(name)` function returns the value of fundamental physical and
mathematical constants in SI units.

```nl
# Speed of light
c = const("c")          # 2.99792458e8 m/s

# Boltzmann constant — thermal energy at room temperature
kT = const("k_B") * 293.15
print("kT =", kT, "J")  # ≈ 4.04e-21 J

# Planck constant
h = const("h")          # 6.62607015e-34 J·s

# Avogadro
NA = const("N_A")       # 6.02214076e23 mol⁻¹

# Gravitational constant
G = const("G")          # 6.6743e-11 m³/(kg·s²)
```

Use `const_desc(name)` to print the description and unit:
```nl
print(const_desc("m_e"))    # m_e = 9.10938e-31 kg — Electron rest mass
print(const_desc("alpha"))  # alpha = 7.29735e-3  — Fine-structure constant
```

### Available Constants (selected)

| Name | Value | Description |
|------|-------|-------------|
| `c` | 2.997 924 58×10⁸ m/s | Speed of light |
| `h` | 6.626 070 15×10⁻³⁴ J·s | Planck constant |
| `hbar` | 1.054 571 817×10⁻³⁴ J·s | Reduced Planck constant |
| `k_B` | 1.380 649×10⁻²³ J/K | Boltzmann constant |
| `N_A` | 6.022 140 76×10²³ mol⁻¹ | Avogadro constant |
| `G` | 6.674 30×10⁻¹¹ m³/(kg·s²) | Gravitational constant |
| `g` | 9.806 65 m/s² | Standard gravity |
| `e` / `q_e` | 1.602 176 634×10⁻¹⁹ C | Elementary charge |
| `m_e` | 9.109 383 70×10⁻³¹ kg | Electron mass |
| `m_p` | 1.672 621 924×10⁻²⁷ kg | Proton mass |
| `alpha` | 7.297 352 57×10⁻³ | Fine-structure constant |
| `R` | 8.314 462 618 J/(mol·K) | Molar gas constant |
| `F` | 96 485.332 12 C/mol | Faraday constant |
| `sigma_SB` | 5.670 374 419×10⁻⁸ W/(m²·K⁴) | Stefan–Boltzmann constant |
| `AU` | 1.495 978 707×10¹¹ m | Astronomical unit |
| `eV` | 1.602 176 634×10⁻¹⁹ J | Electron-volt |

> Full list: 54 constants. See `NeoPhysics.h` for the complete table.

---

## 20. Master Engineering Example

This example demonstrates a complete engineering workflow:
1. Load sensor data from CSV
2. Fit a regression model
3. Solve a related differential equation
4. Display results in a custom GUI

```nl
# ─── Step 1: Load CSV measurement data ───
data = import_csv("/RC_discharge.csv")
# Format: [[t0, V0], [t1, V1], ...]
t_vals = map(def(row): return row[0], data)
v_vals = map(def(row): return row[1], data)

# ─── Step 2: Fit exponential regression ───
model = regress(t_vals, v_vals, "exponential")
# model is a callable: model(t) → fitted voltage

# ─── Step 3: Solve the RC discharge ODE ───
# dV/dt = -V / (R*C),  R=10kΩ, C=100µF
R = 10000.0
C = 0.0001
def dV(t, V): return -V / (R * C)

V0 = v_vals[0]   # Initial voltage from data
sol = ndsolve(dV, V0, 0, 5, 500)

# ─── Step 4: Print key results ───
tau = R * C
print("RC time constant τ =", tau, "s")
print("V(τ) analytical =", V0 / n(e))
print("V(τ) numerical  =", sol[int(100 * tau)][1])

# ─── Step 5: Build a GUI dashboard ───
def on_plot(v):
    print("Plotting", len(sol), "ODE points")
    # In a real script, trigger a plot overlay here

def on_info(v):
    print(const_desc("e"))
    print("tau =", tau, "s,  V0 =", V0, "V")

gui_clear()
gui_label("RC Discharge Analyser")
gui_label("V0 = " + str(V0) + " V,  τ = " + str(tau) + " s")
gui_button("Plot ODE Solution", "on_plot")
gui_button("Show Constants", "on_info")
gui_show()
```

---

## 21. Modules and Namespaces

NeoLanguage Phase 8 introduces a **module import system** to keep the global
namespace clean and load only the functions you need.

### Available Modules

| Module | Contents |
|--------|----------|
| `math` | Constants (`pi`, `e`, `phi`) + math functions (`sin`, `cos`, `sqrt`, …) |
| `finance` | TVM solvers: `tvm_pv`, `tvm_fv`, `tvm_pmt`, `tvm_n` |
| `electronics` | Bitwise tools: `bit_get`, `bit_set`, `bit_clear`, `to_bin`, `to_hex` |
| `stats` | Statistical functions: `mean`, `stddev`, `variance` |
| `signal` | FFT tools: `fft`, `ifft`, `abs_spectrum` |

### Import Syntax

**`import X`** — load all exports of module `X` into the current scope:

```nl
import finance
pv = tvm_pv(5, 12, -500)       # finance functions now directly available
print(pv)
```

**`import X as Y`** — load module `X` as a dictionary named `Y`:

```nl
import finance as fin
pv = fin["tvm_pv"](5, 12, -500)
print(pv)
```

**`from X import a, b`** — import only specific names:

```nl
from math import pi, sin
print(sin(pi / 6))              # => 0.5
```

### Practical Example

```nl
# Clean namespace: only load what you need
from math import pi, sin, cos
import stats as s

# Generate a signal
N = 32
signal = []
for i in range(N):
    signal = signal + [sin(2 * pi * i / N)]

# Statistics on the signal
print(s["mean"](signal))    # => ~0.0
print(s["stddev"](signal))  # => ~0.707
```

---

## 22. Signal Processing

NeoLanguage provides a **Fast Fourier Transform** (Cooley-Tukey radix-2) for
frequency analysis of sampled signals.

### Functions

| Function | Description |
|----------|-------------|
| `fft(list)` | Forward FFT. Returns a list of `[re, im]` pairs. Pads input to next power-of-2. |
| `ifft(list)` | Inverse FFT. Accepts `[re, im]` pairs or real numbers. |
| `abs_spectrum(fft_result)` | Returns magnitude spectrum from `fft()` output. |

### Complete Example: Noisy Sine Wave → Frequency Spectrum

```nl
from math import pi, sin
import signal as sig

# 1. Generate a noisy sine wave at frequency f0 = 3 Hz
#    Sampled at Fs = 32 samples (one period)
N  = 32
f0 = 3
x  = []
for i in range(N):
    noise = (i * 7 + 3) / 1000.0   # simple deterministic noise
    x = x + [sin(2 * pi * f0 * i / N) + noise]

# 2. Compute the FFT
F = fft(x)

# 3. Extract magnitude spectrum
mag = abs_spectrum(F)

# 4. Find the dominant frequency bin
peak = 0
peak_bin = 0
for i in range(N / 2):     # Only look at first half (Nyquist)
    if mag[i] > peak:
        peak = mag[i]
        peak_bin = i

print("Dominant bin:", peak_bin)   # Should print 3
print("Magnitude:", peak)

# 5. Plot the spectrum using the GUI
plot_data = []
for i in range(N / 2):
    plot_data = plot_data + [mag[i]]
```

> **Note:** `fft()` pads the input to the next power-of-2 automatically.
> The output length equals the padded size. For best frequency resolution,
> use input lengths that are already powers of 2 (e.g. 32, 64, 128, 256).

---

## 23. Advanced Linear Algebra

| Function | Description |
|----------|-------------|
| `det(matrix)` | Determinant of a square matrix (Gaussian elimination with partial pivoting). |
| `inv(matrix)` | Inverse of a square matrix (Gauss-Jordan). Returns `None` if singular. |
| `eigen(matrix)` | Eigenvalues and eigenvectors via power iteration. Returns a dictionary `{"values": [...], "vectors": [...]}`. |

### Example: Eigendecomposition

```nl
# 2×2 rotation-scale matrix
A = [[3, 1], [0, 2]]

d = det(A)          # => 6
print("det =", d)

Ainv = inv(A)       # => [[0.333, -0.167], [0, 0.5]]
print("inv =", Ainv)

e = eigen(A)
print("eigenvalues:", e["values"])    # => [3.0, 2.0]
print("eigenvectors:", e["vectors"])
```

---

## 24. Performance Profiling

`time_it(func, args...)` measures the wall-clock time in **milliseconds** to
execute a NeoLanguage function on the ESP32-S3.

```nl
def heavy(n):
    s = 0
    for i in range(n):
        s = s + i
    return s

ms = time_it(heavy, 5000)
print("Elapsed:", ms, "ms")
```

This lets you compare algorithm variants directly on the device:

```nl
# Compare iterative vs recursive sum
iter_ms  = time_it(iterative_sum, 1000)
recur_ms = time_it(recursive_sum, 1000)
print("Iterative:", iter_ms, "ms")
print("Recursive:", recur_ms, "ms")
```

---

## 25. Keyboard Reference

| Key | Editor Action |
|-----|---------------|
| **F5** | Run program |
| **F1** | Insert 4 spaces (indent) |
| **F2** | Save to flash (`/neolang.nl`) |
| **F3** | Load from flash |
| **F4** | Insert `#` (comment) |
| **MODE** | Exit to main menu |
| **DEL** | Delete character |
| **ENTER** | New line |
| **AC** | Return focus to tab bar |
| **←/→/↑/↓** | Cursor navigation |

### Letter Keys

| Key | Character |
|-----|-----------|
| ALPHA A–F | `a`–`f` |
| X / Y | `x` / `y` |
| SIN | `s` |
| COS | `c` |
| TAN | `t` |
| LN | `n` |
| LOG | `l` |
| π | `p` |
| e | `e` |
| √ | `r` |
| ANS | `i` |

---

## 26. Known Limitations

| Limitation | Detail |
|------------|--------|
| **Single-char variable names in CAS** | Symbolic variables (`SymVar`) store one character. `alpha` and `apple` both become `a` in the CAS. Use single-letter names (`x`, `y`, `z`, `t`, `n`) for symbolic math. |
| **No string type** | NeoLanguage currently has no `string` data type. Text is only used in `print()`, `input_num()`, and `msg_box()` calls. Unit strings are passed as-is. |
| **`input_num` is non-blocking** | Returns 0 immediately; a modal UI hook is planned for a future release. |
| **Loop limit** | `while` and `for` loops are capped at 10 000 iterations to prevent watchdog resets on embedded hardware. |
| **Recursion depth** | Deep recursion may overflow the ESP32 call stack. Iterative solutions are preferred for large inputs. |
| **Plot is momentary** | The plot overlay is dismissed on the next keypress. There is no zoom or trace mode in the NeoLanguage plot overlay (use GrapherApp for those features). |
| **range(start, stop)** | Currently iterates `stop - start` times starting from 0. Full Python-style range with configurable start is planned. |
| **Matrix multiplication** | Full matrix-matrix multiply (dot product) is not yet built in; use `dot()` for vectors or implement with `map`/`for` loops. |
| **`table()` and user functions** | `table()` currently only evaluates `native_function` (regression) callables. For user-defined functions, use `map()` over a generated list instead. |
| **limit() is numerical only** | `limit()` uses Richardson extrapolation — it cannot compute symbolic limits (e.g. limits involving infinities algebraically). Use `n()` + manual evaluation for simple cases. |
| **taylor() coefficient precision** | Taylor coefficients are computed via numerical differentiation. For high-order terms (k > 6), floating-point cancellation may reduce accuracy. |
| **Regression model persistence** | Regression models returned by `regress()` are in-memory only. They cannot be saved to flash directly. Save the coefficients as variables instead. |
| **Unit compound expressions** | Complex compound unit strings like `"kg*m/s^2"` are not yet parsed. Use named units (`"N"`, `"J"`) or sequential arithmetic. |

---

*NeoLanguage — Part of the NeoCalculator / NumOS project.*
