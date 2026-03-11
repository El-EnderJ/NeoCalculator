# NeoLanguage — Official Language Bible

> **File extension:** `.nl` (NeoLanguage)  
> **Version:** Phase 3 — Standard Library Release  
> **Target platform:** NeoCalculator / NumOS (ESP32-S3)

---

## Table of Contents

1. [Core Philosophy](#1-core-philosophy)
2. [Syntax Guide](#2-syntax-guide)
3. [Data Types](#3-data-types)
4. [Operators](#4-operators)
5. [Control Flow](#5-control-flow)
6. [Functions](#6-functions)
7. [Standard Library Reference](#7-standard-library-reference)
8. [Code Examples](#8-code-examples)
9. [Keyboard Reference](#9-keyboard-reference)
10. [Known Limitations](#10-known-limitations)

---

## 1. Core Philosophy

NeoLanguage is a **Symbolic-Imperative hybrid** designed for a scientific calculator.
It blends two powerful ideas:

| Concept | Inspiration | Behaviour |
|---------|-------------|-----------|
| Imperative scripting | Python | Variables, loops, conditionals, functions |
| Symbolic mathematics | Wolfram Language | Undefined variables become symbolic expressions |

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

### Type Arithmetic Escalation

```
number  OP number   → number    (double arithmetic)
exact   OP exact    → exact     (rational arithmetic)
exact   OP number   → number    (promote to double)
any     OP symbolic → symbolic  (CAS factory)
```

---

## 4. Operators

### Arithmetic

| Operator | Operation |
|----------|-----------|
| `+` | Addition |
| `-` | Subtraction / Negation |
| `*` | Multiplication |
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

## 7. Standard Library Reference

All built-in function names are **lowercase**.

### 7.1 Symbolic Calculus & Algebra

---

#### `diff(expr, var)`

Compute the symbolic derivative of `expr` with respect to `var`.

```nl
diff(x^3, x)          # => (3 * (x^2))
diff(sin(x) * x, x)   # => (sin(x) + (x * cos(x)))
```

Internally uses `SymDiff::diff()` followed by `SymSimplify::simplify()`.

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

Uses `OmniSolver` which automatically selects between exact polynomial solving
and Newton-Raphson numerical approximation.  All roots are printed to the
console; the return value is the first root.

---

#### `simplify(expr)`

Apply algebraic simplification rules (constant folding, like-term collection,
Pythagorean identities, log/exp cancellation, etc.).

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

`expand()` calls the same simplifier as `simplify()` — the distribute rule
(G8) is included in every simplification pass.

---

### 7.2 High-Level Graphics

---

#### `plot(expr, xmin, xmax)`

Enter **Graphics Mode** and render the function `expr` over `[xmin, xmax]`.

```nl
plot(sin(x), -6.28, 6.28)
plot(x^2 - 4, -5, 5)
plot(exp(-x^2), -3, 3)   # Gaussian bell curve
```

**How it works (decoupled architecture):**

1. `expr` is evaluated symbolically by the interpreter producing a `SymExpr*`
   node.
2. NeoLanguage stores a `NeoPlotRequest { expr*, xMin, xMax }`.
3. After the program finishes, NeoLanguageApp creates a full-screen graphics
   overlay.
4. The overlay evaluates `expr->evaluate(x)` directly for each screen pixel —
   **no string re-parsing**.
5. Press any key to return to the IDE.

Because the plot uses `SymExpr::evaluate()` from the CAS layer (not
GrapherApp's string evaluator), **upgrading GrapherApp has zero effect on
NeoLanguage programs**.

---

#### `clear_plot()`

Discard any pending plot request queued by `plot()`.

---

### 7.3 System & Utilities

---

#### `print(args...)`

Send values to the console output area.  Multiple arguments are joined with
spaces and followed by a newline.

```nl
print("Hello, world!")
print("x =", x, "y =", y)
```

---

#### `type(val)`

Print the type name of `val` and return its ordinal (0–5).

```nl
type(3.14)      # prints: type: number  => 2
type(x)         # prints: type: symbolic => 4
type(True)      # prints: type: bool     => 1
```

| Ordinal | Type name |
|---------|-----------|
| 0 | None |
| 1 | bool |
| 2 | number |
| 3 | exact |
| 4 | symbolic |
| 5 | function |

---

#### `vars()`

List all variables defined in the current scope (and parent scopes).

```nl
a = 1
b = 2
vars()
# prints:
#   a = 1
#   b = 2
```

---

#### `input_num("prompt")`

Request a numeric value from the user (UI hook).

```nl
r = input_num("Enter radius:")
area = 3.14159 * r^2
print("Area =", area)
```

> **Note:** In the current release, `input_num` returns `0` because the
> calculator keyboard is not yet modal-blocked during execution.  A future
> release will display an LVGL dialog and wait for input.

---

#### `msg_box("title", "content")`

Display a message in the console (future: LVGL modal dialog).

```nl
msg_box("Result", "Calculation complete")
```

---

### 7.4 Math Primitives (always available)

These are implemented directly in the interpreter (no CAS overhead):

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `asin(x)` / `arcsin(x)` | Arcsine |
| `acos(x)` / `arccos(x)` | Arccosine |
| `atan(x)` / `arctan(x)` | Arctangent |
| `ln(x)` / `log(x)` | Natural logarithm |
| `log10(x)` | Base-10 logarithm |
| `exp(x)` | e^x |
| `abs(x)` | Absolute value |
| `sqrt(x)` | Square root (= x^(1/2)) |
| `range(n)` | Integer range 0…n−1 (for loops) |
| `range(a, b)` | Integer range a…b−1 |

---

## 8. Code Examples

### 8.1 Hello World

```nl
print("Hello, NeoWorld!")
```

### 8.2 Fibonacci Sequence

```nl
def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

for i in range(10):
    print(fib(i))
```

### 8.3 Symbolic Differentiation

```nl
# Define a polynomial
p = x^4 - 3*x^2 + 2

# Differentiate twice
dp  = diff(p, x)
ddp = diff(dp, x)

print("p =", p)
print("p' =", dp)
print("p'' =", ddp)
```

### 8.4 Solving an Equation

```nl
# Find roots of x^3 - 6x^2 + 11x - 6 = 0
poly = x^3 - 6*x^2 + 11*x - 6
solve(poly, x)
```

### 8.5 Taylor Series (manual, degree 5)

```nl
# Approximate sin(x) around x=0
# Taylor series: x - x^3/6 + x^5/120
taylor = x - x^3 / 6 + x^5 / 120
simplified = simplify(taylor)
plot(simplified, -4, 4)
plot(sin(x), -4, 4)
```

### 8.6 Function Plotting

```nl
# Gaussian bell curve
plot(exp(-x^2), -3, 3)

# Damped oscillation
plot(sin(x) * exp(-x / 5), 0, 30)

# Lemniscate-like
plot(sin(2*x) * cos(x), 0, 6.28)
```

### 8.7 Numerical Integration Check

```nl
# Antiderivative of x*sin(x)
antideriv = integrate(x * sin(x), x)
print("Antiderivative:", antideriv)

# Verify by differentiating
check = diff(antideriv, x)
check = simplify(check)
print("d/dx(antideriv) =", check)
```

### 8.8 Newton's Method (manual)

```nl
# Solve sqrt(2) using Newton iterations for x^2 - 2 = 0
x0 = 1.0
for i in range(20):
    fx  = x0^2 - 2
    dfx = 2 * x0
    x0 = x0 - fx / dfx
print("sqrt(2) =", x0)
```

### 8.9 Symbolic Taylor Series (using diff)

```nl
# Compute Taylor coefficients of e^x at x=0
# e^x = 1 + x + x^2/2! + x^3/3! + ...
def factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)

f = exp(x)
coeff0 = diff(f, x)   # Note: needs x=0 evaluation — symbolic demo only
print("d/dx exp(x) =", simplify(coeff0))
```

---

## 9. Keyboard Reference

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

## 10. Known Limitations

| Limitation | Detail |
|------------|--------|
| **Single-char variable names in CAS** | Symbolic variables (`SymVar`) store one character. `alpha` and `apple` both become `a` in the CAS. Use single-letter names (`x`, `y`, `z`, `t`, `n`) for symbolic math. |
| **No string type** | NeoLanguage currently has no `string` data type. Text is only used in `print()`, `input_num()`, and `msg_box()` calls. |
| **`input_num` is non-blocking** | Returns 0 immediately; a modal UI hook is planned for a future release. |
| **Loop limit** | `while` and `for` loops are capped at 10 000 iterations to prevent watchdog resets on embedded hardware. |
| **Recursion depth** | Deep recursion may overflow the ESP32 call stack. Iterative solutions are preferred for large inputs. |
| **Plot is momentary** | The plot overlay is dismissed on the next keypress. There is no zoom or trace mode in the NeoLanguage plot overlay (use GrapherApp for those features). |

---

*NeoLanguage — Part of the NeoCalculator / NumOS project.*
