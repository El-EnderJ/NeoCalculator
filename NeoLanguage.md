# NeoLanguage — Official Language Bible

> **File extension:** `.nl` (NeoLanguage)
> **Version:** Phase 4 — Data Structures, Lists & Matrices, Complex Library
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
11. [Code Examples](#11-code-examples)
12. [Keyboard Reference](#12-keyboard-reference)
13. [Known Limitations](#13-known-limitations)

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

### Type Arithmetic Escalation

```
number  OP number   → number    (double arithmetic)
exact   OP exact    → exact     (rational arithmetic)
exact   OP number   → number    (promote to double)
any     OP symbolic → symbolic  (CAS factory)
list    + list      → list      (concatenation)
list    OP scalar   → list      (element-wise vectorisation)
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

## 11. Code Examples

### 11.1 Hello World

```nl
print("Hello, NeoWorld!")
```

### 11.2 Fibonacci Sequence

```nl
def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

for i in range(10):
    print(fib(i))
```

### 11.3 Symbolic Differentiation

```nl
p = x^4 - 3*x^2 + 2
dp  = diff(p, x)
ddp = diff(dp, x)
print("p =", p)
print("p' =", dp)
print("p'' =", ddp)
```

### 11.4 Solving an Equation

```nl
poly = x^3 - 6*x^2 + 11*x - 6
solve(poly, x)
```

### 11.5 Solving a Linear System (Cramer's Rule)

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

### 11.6 Vector Operations

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

### 11.7 Taylor Expansion of sin(x)

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

### 11.8 Matrix Determinant (2x2)

```nl
def det2(M):
    return M[0, 0] * M[1, 1] - M[0, 1] * M[1, 0]

A = [[3, 7], [1, -4]]
print("det(A) =", det2(A))   # => 3*(-4) - 7*1 = -19
```

### 11.9 Newton's Method (manual)

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

### 11.10 Map and Filter Example

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

### 11.11 Numeric Constants Demo

```nl
print("pi  =", n(pi))           # 3.141592654
print("e   =", n(e))            # 2.718281828
print("phi =", n(phi))          # 1.618033989
print("tau =", n(pi * 2))       # 6.283185307
```

---

## 12. Keyboard Reference

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

## 13. Known Limitations

| Limitation | Detail |
|------------|--------|
| **Single-char variable names in CAS** | Symbolic variables (`SymVar`) store one character. `alpha` and `apple` both become `a` in the CAS. Use single-letter names (`x`, `y`, `z`, `t`, `n`) for symbolic math. |
| **No string type** | NeoLanguage currently has no `string` data type. Text is only used in `print()`, `input_num()`, and `msg_box()` calls. |
| **`input_num` is non-blocking** | Returns 0 immediately; a modal UI hook is planned for a future release. |
| **Loop limit** | `while` and `for` loops are capped at 10 000 iterations to prevent watchdog resets on embedded hardware. |
| **Recursion depth** | Deep recursion may overflow the ESP32 call stack. Iterative solutions are preferred for large inputs. |
| **Plot is momentary** | The plot overlay is dismissed on the next keypress. There is no zoom or trace mode in the NeoLanguage plot overlay (use GrapherApp for those features). |
| **range(start, stop)** | Currently iterates `stop - start` times starting from 0. Full Python-style range with configurable start is planned. |
| **Matrix multiplication** | Full matrix-matrix multiply (dot product) is not yet built in; use `dot()` for vectors or implement with `map`/`for` loops. |

---

*NeoLanguage — Part of the NeoCalculator / NumOS project.*
