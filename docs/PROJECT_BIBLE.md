# NumOS — Project Bible

> **The master documentation of the project. If something is not here, it does not exist.**
>
> **Platform**: ESP32-S3 N16R8 CAM · **UI**: LVGL 9.5
> **Language**: C++17 · **Pro-CAS Engine**: Active Production
> **Last Update**: April 2026

---

## Table of Contents

1. [Vision](#1-vision)
2. [Software Architecture](#2-software-architecture)
3. [Pro-CAS Engine — Internal Architecture](#3-pro-cas-engine--internal-architecture)
4. [Modules — Complete Inventory](#4-modules--complete-inventory)
5. [Build Configuration](#5-build-configuration)
6. [Current State (April 2026)](#6-current-state-april-2026)
7. [Code Style Guide](#7-code-style-guide)
8. [How to Add a New App](#8-how-to-add-a-new-app)
9. [How to Add a Math Function](#9-how-to-add-a-math-function)
10. [How to Extend the Pro-CAS](#10-how-to-extend-the-pro-cas)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Vision

**NumOS** is an open-source scientific calculator and graphing operating system, built on the **ESP32-S3 N16R8 CAM** microcontroller and the **LVGL 9.x** graphics library.

**Main Goal**: Create the best open-source alternative to commercial calculators like the Casio fx-991EX ClassWiz, NumWorks, TI-84 Plus CE, and HP Prime G2. With 320×240 color display, mathematical Natural Display, proprietary CAS-Lite, and extensible modular architecture.

### Design Principles

| Principle | Description |
|:----------|:------------|
| **Modularity** | Each app is an interchangeable module with explicit interface. Adding an app does not touch the core. |
| **Efficiency** | C++17 without excessive heap. LVGL objects in PSRAM, DMA buffers in internal heap. CAS uses `PSRAMAllocator`. |
| **Extensibility** | Math Engine: 3 files for new function. CAS: 1 file for new solver. Launcher: 2 lines. |
| **Visual Fidelity** | Real fractions, roots √, genuine superscripts — like on paper. |
| **Transparency** | The CAS shows steps to the user. It is not a black box. Educational by design. |

---

## 2. Software Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                          main.cpp (setup/loop)                         │
│  PSRAM → TFT init → lv_init → DMA bufs → Splash → g_app.begin        │
│  loop(): lv_timer_handler · g_app.update · g_serial.pollEvent          │
└──────────────────────────────┬─────────────────────────────────────────┘
                               │
┌──────────────────────────────▼─────────────────────────────────────────┐
│                       SystemApp (Dispatcher)                           │
│                                                                        │
│  Mode { MENU, APP }                                                    │
│  injectKey(key) → if MODE → returnToMenu()                             │
│                 → else    → activeApp->handleKey(key)                  │
│                                                                        │
│  ┌──────────────┐  ┌──────────────────┐  ┌────────────────────────┐   │
│  │  MainMenu    │  │  CalculationApp  │  │      GrapherApp        │   │
│  │  LVGL Grid   │  │  Natural VPAM    │  │  y=f(x) Zoom/Pan       │   │
│  │  3×N scroll  │  │  History 32      │  │  Values Table          │   │
│  └──────────────┘  └──────────────────┘  └────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │              EquationsApp (Pro-CAS UI)                          │  │
│  │  States: SELECT → EQ_INPUT → RESULT → STEPS                     │  │
│  │  Pipeline: MathAST → ASTFlattener → Solver → SymToAST           │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  CalculusApp (Pro-CAS)  — Unified: d/dx + ∫dx             │  │
│  │  Tab-based mode switch · Derivatives + Integrals          │  │
│  │  17 rules · Slagle · Steps · +C · Natural Display          │  │
│  └──────────────────────────────────────────────────────────────┘  │
│  [ SettingsApp · Sequences · Statistics · Regression ]         │
│  [ Table · Probability · Python (placeholder) ]                │
└────────────────────────────────────────────────────────────────────────┘
       │                       │                        │
       ▼                       ▼                        ▼
┌─────────────┐   ┌────────────────────────┐   ┌───────────────────────┐
│ Math Engine │   │ Pro-CAS Engine ★       │   │    Hardware Layer     │
│             │   │                        │   │                       │
│ Tokenizer   │   │ CASInt / CASRational   │   │ DisplayDriver         │
│ Parser      │   │ SymExpr DAG (consed)   │   │ (TFT_eSPI FSPI)       │
│ Evaluator   │   │ SymDiff (17 rules)     │   │ lvglFlushCb DMA       │
│ ExprNode    │   │ SymIntegrate (Slagle)  │   │ KeyMatrix 5×10         │
│ MathAST     │   │ SymSimplify (8-pass)   │   │ SerialBridge          │
│ VarContext  │   │ OmniSolver / Solvers   │   │ LvglKeypad (indev)    │
│ EqSolver    │   │ CASStepLogger          │   │ LittleFS              │
└─────────────┘   │ SymToAST / SymExprToAST│   └───────────────────────┘
                  └────────────────────────┘
```

### 2.1 main.cpp — Boot Sequence

`setup()` in strict order:
1. `Serial.begin(115200)` + wait CDC up to 3 s
2. `g_display.begin()` — init TFT FSPI @10 MHz, reset, fillScreen black
3. `lv_init()` + `lv_tick_set_cb(millis)`
4. `heap_caps_malloc(MALLOC_CAP_DMA|MALLOC_CAP_8BIT, 6400)` × 2 — **DMA, NO ps_malloc**
5. `g_display.initLvgl(buf1, buf2, 6400)` — register flush callback
6. Animated splash screen → pump `lv_timer_handler()` until completion + 800 ms
7. `g_app.begin()` — init SystemApp, LittleFS, launcher, apps
8. `g_serial.begin()` — activate SerialBridge

`loop()` in continuous cycle:
- `lv_timer_handler()` — LVGL events, renders, animations
- `g_app.update()` — tick of active app
- `g_serial.pollEvent()` → `g_app.injectKey()`

### 2.2 SystemApp — Central Orchestrator

| Responsibility | Description |
|:----------------|:------------|
| **Global state** | Angular mode (DEG/RAD/GRA), active app, selection index |
| **Launcher (MENU)** | LVGL grid 3×N, scroll, highlight selection |
| **App dispatching** | Routes `KeyEvent`s to active app or launcher |
| **MODE intercepted** | `KeyCode::MODE` always → `returnToMenu()` before reaching app |
| **injectKey()** | Public API — SerialBridge and KeyMatrix use this entry |
| **LittleFS** | Loads `/vars.dat` at startup |
| **LvglKeypad** | Initializes LVGL indev type `KEYPAD` |

```cpp
enum class Mode { MENU, APP_CALCULATION, APP_GRAPHER, APP_EQUATIONS,
                  APP_CALCULUS, APP_SETTINGS, APP_STATISTICS,
                  APP_PROBABILITY, APP_REGRESSION, APP_MATRICES,
                  APP_PYTHON, APP_SEQUENCES };

// Lazy-init lifecycle of an app:
// 1. Boot: all apps are new'd (no LVGL work) — cheap.
// 2. User selects icon → ENTER → launchApp():
//    app->load() calls if (!_screen) begin()  [lazy LVGL creation]
//    g_lvglActive = true, _mode = APP_*
// 3. update() → app->update()
// 4. injectKey(key==MODE) → returnToMenu() [DEFERRED TEARDOWN]:
//    a) _mainMenu.load()          ← starts 200 ms FADE_IN animation
//    b) _pendingTeardownMode = _mode, _teardownStartMs = millis()
//    c) _mode = MENU  — returns immediately (no end() yet!)
// 5. 250 ms later inside update():
//    app->end() is called — animation has already completed, safe to delete
// This 250 ms gap prevents use-after-free corruption of the LVGL
// animation object list that caused an infinite loop in lv_timer_handler().
```

### 2.3 Apps Interface

```cpp
void begin();                  // Create LVGL screen, initialize state (lazy: called by load())
void end();                    // Destroy screen, free resources (called by deferred teardown)
void load();                   // Make app visible: calls begin() if needed, then loads screen
void handleKey(KeyCode key);   // Process user input
void update();                 // Periodic tick (~60 fps)
```

**Critical rule for `end()`**: must call `_statusBar.destroy()` *before* `lv_obj_delete(_screen)`.  
This prevents `StatusBar::create()` from misfiring its `if (_bar) return` guard on dangling pointers  
when the app is reopened. Every app (`CalculationApp`, `GrapherApp`, `EquationsApp`, `CalculusApp`,  
`SettingsApp`, `StatisticsApp`, `ProbabilityApp`, `RegressionApp`, `MatricesApp`, `SequencesApp`,  
`PythonApp`) follows this pattern.

**LVGL-native apps**: All current apps → `g_lvglActive = true`.

### 2.4 EquationsApp — Internal Flow

```
begin() → setState(SELECT) → showModeSelection()

handleKey(ENTER) in SELECT:
  mode 0 → setState(EQ_INPUT) → showInputScreen(1 var)
  mode 1 → setState(EQ_INPUT) → showInputScreen(2 vars)

handleKey(ENTER) in EQ_INPUT → solveEquations():
  splitAtEquals(expr) → lhs, rhs
  parseToMathAST(lhs), parseToMathAST(rhs)
  ASTFlattener::flatten(lhs, rhs) → SymPoly
  SingleSolver::solve(poly)  or  SystemSolver::solve(eq1, eq2)
  result → setState(RESULT) → buildResultDisplay()

handleKey(SHOW_STEPS) in RESULT:
  setState(STEPS) → buildStepsDisplay()

handleKey(MODE) [intercepted by SystemApp]:
  returnToMenu() → end() + begin() [complete reset]
```

---

## 3. Pro-CAS Engine — Internal Architecture

The Pro-CAS is a complete symbolic algebra engine, evolution of the original CAS-Lite. It implements an immutable DAG with hash-consing, overflow-safe bignum arithmetic (`CASInt`/`CASRational`), multi-pass simplification with fixed point (8 iterations), symbolic differentiation (17 rules), symbolic integration (Slagle heuristic), and non-linear system solving via Sylvester resultant. Everything lives in PSRAM.

### 3.1 Module Structure

```
src/math/cas/
├── CASInt.h              ← Hybrid BigInt: int64 fast-path + mbedtls_mpi
├── CASRational.h/.cpp    ← Exact fraction overflow-safe (auto-GCD)
├── ConsTable.h           ← Hash-consing PSRAM: node dedup
├── PSRAMAllocator.h      ← STL Allocator → ps_malloc/ps_free
├── SymExpr.h/.cpp        ← Immutable DAG (hash + weight)
├── SymExprArena.h        ← Bump allocator PSRAM + integrated ConsTable
├── SymDiff.h/.cpp        ← Differentiation: 17 rules (chain, product, trig, exp, log)
├── SymIntegrate.h/.cpp   ← Slagle Integration: table, linearity, u-sub, parts
├── SymSimplify.h/.cpp    ← Fixed-point simplifier (8 passes, trig/log/exp)
├── SymPoly.h/.cpp        ← Univariate symbolic polynomial (CASRational)
├── SymPolyMulti.h/.cpp   ← Multivariate polynomial + Sylvester resultant
├── ASTFlattener.h/.cpp   ← MathAST (VPAM) → SymExpr DAG
├── SingleSolver.h/.cpp   ← 1-var equation: linear / quadratic / N-R
├── SystemSolver.h/.cpp   ← 2×2 system: Gaussian + NL (resultant)
├── OmniSolver.h/.cpp     ← Analytic variable isolation
├── HybridNewton.h/.cpp   ← Newton-Raphson with symbolic Jacobian
├── CASStepLogger.h/.cpp  ← StepVec in PSRAM (INFO/FORMULA/RESULT/ERROR)
├── SymToAST.h/.cpp       ← SolveResult → Natural Display MathAST
└── SymExprToAST.h/.cpp   ← SymExpr → MathAST (+C, ∫)
```

### 3.2 Fundamental Types

#### `Rational`
```cpp
struct Rational {
    int64_t num, den;  // den always > 0, simplified by GCD
    Rational(int64_t n=0, int64_t d=1);  // Auto-normalizes
    double toDouble() const;
    bool isInteger() const;
    // Operators: +, -, *, /, ==, !=
};
```

#### `SymPoly`
```cpp
using CoeffMap = std::map<int, Rational, std::less<int>,
                          PSRAMAllocator<std::pair<const int, Rational>>>;
struct SymPoly {
    CoeffMap coeffs;  // {degree: Rational coefficient}
    char     var;     // Main variable ('x' by default)

    int      degree() const;
    Rational coeff(int deg) const;
    SymPoly  derivative() const;
    double   evaluate(double x) const;
};
```

#### Results
```cpp
struct SolveResult {
    enum Status { OK_ONE, OK_TWO, COMPLEX, INFINITE, NONE, ERROR };
    Status   status;
    Rational root1, root2;
    CASStepLogger steps;  // Steps in PSRAM
};
struct SystemResult {
    enum Status { OK, INFINITE, INCONSISTENT, ERROR };
    Status   status;
    Rational x, y;
    CASStepLogger steps;
};
```

### 3.3 SingleSolver — Solving Logic

```
Input: SymPoly (lhs - rhs = 0)

degree 0: constant → ERROR or INFINITE
degree 1: ax + b = 0  →  x = -b/a
         Steps: [INFO "Linear equation", FORMULA "x = -b/a", RESULT "x=value"]

degree 2: ax² + bx + c = 0
         Δ = b² - 4ac
         Δ < 0 → COMPLEX (no real solution)
         Δ = 0 → double root: x = -b/(2a)
         Δ > 0 → x₁ = (-b+√Δ)/(2a),  x₂ = (-b-√Δ)/(2a)
         Steps detail: normalization, Δ calculation, applied formula, roots

degree ≥ 3: Newton-Raphson numerical
           seeds: 0, 1, -1, 2, -2
           converges: |f(x)| < 1e-10, max 100 iter
```

### 3.4 SystemSolver — Gaussian Elimination 2×2

```
eq1: a₁x + b₁y = c₁
eq2: a₂x + b₂y = c₂

eq1' = eq1 × a₂  ;  eq2' = eq2 × a₁
eq3 = eq1' - eq2'  →  (b₁a₂ - b₂a₁)y = (c₁a₂ - c₂a₁)

denominator D = b₁a₂ - b₂a₁
  D = 0 and num≠0 → INCONSISTENT
  D = 0 and num=0 → INFINITE
  D ≠ 0 → y = num/D  ;  substitute in eq1 to get x
```

### 3.5 PSRAM Memory Management

`CASStepLogger` uses `StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>`.  
`EquationsApp::end()` **must** call:
```cpp
_singleResult.steps.clear();   // Frees StepVec PSRAM
_systemResult.steps.clear();   // Frees StepVec PSRAM
_resultHint = nullptr;         // Null LVGL pointer (widget already destroyed)
```

Without this, PSRAM accumulates allocations between app sessions.

---

## 4. Modules — Complete Inventory

### Math Engine

| Module | File | Responsibility |
|:-------|:-----|:----------------|
| `Tokenizer` | `math/Tokenizer.cpp/.h` | String → list of `Token` (24 types) |
| `Parser` | `math/Parser.cpp/.h` | Tokens → RPN Shunting-Yard + AST ExprNode |
| `Evaluator` | `math/Evaluator.cpp/.h` | RPN → `double`. Modes DEG/RAD/GRA. |
| `ExprNode` | `math/ExprNode.h` | Visual tree: TEXT/FRACTION/ROOT/POWER |
| `VariableContext` | `math/VariableContext.cpp/.h` | Variables A-Z + Ans. LittleFS `/vars.dat` |
| `EquationSolver` | `math/EquationSolver.cpp/.h` | General Newton-Raphson numerical |
| `StepLogger` | `math/StepLogger.cpp/.h` | Log parser steps (debug) |

### CAS-Lite Engine

| Module | File | Responsibility |
|:-------|:-----|:----------------|
| `PSRAMAllocator<T>` | `math/cas/PSRAMAllocator.h` | STL allocator → `ps_malloc`/`ps_free` |
| `Rational`, `SymPoly` | `math/cas/SymPoly.h/.cpp` | Exact fraction + symbolic polynomial |
| `ASTFlattener` | `math/cas/ASTFlattener.h/.cpp` | `MathAST` → `SymExpr` DAG |
| `SingleSolver` | `math/cas/SingleSolver.h/.cpp` | 1-var equation (L/Q/N-R) with steps |
| `SystemSolver` | `math/cas/SystemSolver.h/.cpp` | 2×2 system Gaussian + NL (resultant) |
| `CASStepLogger` | `math/cas/CASStepLogger.h/.cpp` | `StepVec` PSRAM — 4 step types |
| `SymToAST` | `math/cas/SymToAST.h/.cpp` | `SolveResult` → visual `MathAST` |

### Pro-CAS Engine (advanced extensions)

| Module | File | Responsibility |
|:-------|:-----|:----------------|
| `CASInt` | `math/cas/CASInt.h` | Hybrid BigInt: `int64_t` fast-path + `mbedtls_mpi` |
| `CASRational` | `math/cas/CASRational.h/.cpp` | Overflow-safe exact fraction (auto-GCD) |
| `ConsTable` | `math/cas/ConsTable.h` | Hash-consing PSRAM: node dedup |
| `SymExpr` | `math/cas/SymExpr.h/.cpp` | Immutable DAG with hash (`_hash`) and weight (`_weight`) |
| `SymExprArena` | `math/cas/SymExprArena.h` | Bump allocator PSRAM + integrated ConsTable |
| `SymDiff` | `math/cas/SymDiff.h/.cpp` | Symbolic differentiation: 17 rules (chain, product, trig, exp, log) |
| `SymIntegrate` | `math/cas/SymIntegrate.h/.cpp` | Slagle Integration: table, linearity, u-sub, parts LIATE |
| `SymSimplify` | `math/cas/SymSimplify.h/.cpp` | Multi-pass simplifier (8 iterations, fixed-point, trig/log/exp) |
| `SymPolyMulti` | `math/cas/SymPolyMulti.h/.cpp` | Multivariate polynomial + Sylvester resultant |
| `OmniSolver` | `math/cas/OmniSolver.h/.cpp` | Analytic variable isolation |
| `HybridNewton` | `math/cas/HybridNewton.h/.cpp` | Newton-Raphson with symbolic Jacobian |
| `SymExprToAST` | `math/cas/SymExprToAST.h/.cpp` | `SymExpr` → MathAST. `convertIntegral()` (+C) |

### Apps

| App | File | Status | Description |
|:----|:-----|:------:|:------------|
| `CalculationApp` | `apps/CalculationApp.cpp/.h` | ✅ | Natural VPAM, history 32, A-Z+Ans variables |
| `GrapherApp` | `apps/GrapherApp.cpp/.h` | ✅ | y=f(x), zoom, pan, expression list, table |
| `EquationsApp` | `apps/EquationsApp.cpp/.h` | ✅ | Pro-CAS: 1-var, 2×2 (linear+NL), PSRAM steps |
| `CalculusApp` | `apps/CalculusApp.cpp/.h` | ✅ | Pro-CAS: symbolic d/dx (17 rules) + ∯dx (Slagle), tabs, +C, steps |
| `SettingsApp` | `apps/SettingsApp.cpp/.h` | ✅ | Complex roots toggle, decimal precision, angle mode |
| `StatisticsApp` | `apps/StatisticsApp.cpp/.h` | ✅ | Data lists, mean/median/σ/s, histogram UI |
| `ProbabilityApp` | `apps/ProbabilityApp.cpp/.h` | ✅ | nCr, nPr, n!, binomial, normal, Poisson distributions |
| `RegressionApp` | `apps/RegressionApp.cpp/.h` | ✅ | Linear/quadratic/log/exp regression, R², scatter plot |
| `MatricesApp` | `apps/MatricesApp.cpp/.h` | ✅ | m×n editor, +/−/×/transp., det, inverse, Ax=b |
| `SequencesApp` | `apps/SequencesApp.cpp/.h` | ✅ | Arithmetic/geometric sequences, Nth term, partial sums |
| `PythonApp` | `apps/PythonApp.cpp/.h` | ⚠️ | Placeholder UI (Lua/MicroPython scripting — Phase 8) |

### UI

| Module | File | Description |
|:-------|:-----|:------------|
| `MainMenu` | `ui/MainMenu.cpp/.h` | LVGL launcher grid 3×N scroll |
| `MathRenderer` | `ui/MathRenderer.cpp/.h` | 2D Renderer MathCanvas |
| `StatusBar` | `ui/StatusBar.cpp/.h` | LVGL status bar |
| `GraphView` | `ui/GraphView.cpp/.h` | GrapherApp graph widget |
| `Icons.h` | `ui/Icons.h` | Icon bitmaps 48×48 |
| `Theme.h` | `ui/Theme.h` | Color palette, fonts, constants |

### HAL / Drivers

| Module | File | Description |
|:-------|:-----|:------------|
| `DisplayDriver` | `display/DisplayDriver.cpp/.h` | TFT_eSPI FSPI + LVGL + DMA flush |
| `KeyMatrix` | `input/KeyMatrix.cpp/.h` | 6×8 scan, debounce, autorepeat |
| `SerialBridge` | `input/SerialBridge.cpp/.h` | Key injection from Serial PC |
| `LvglKeypad` | `input/LvglKeypad.cpp/.h` | LVGL indev adapter KEYPAD |
| `KeyCodes.h` | `input/KeyCodes.h` | `KeyCode` enum — 48 keys |

### Tests

| File | Status | Description |
|:-----|:------:|:------------|
| `tests/CASTest.h/.cpp` | ✅ | 53 CAS tests (Phases A-D) |
| `tests/HardwareTest.cpp` | ✅ | Interactive test TFT + physical keyboard |
| `tests/TokenizerTest_temp.cpp` | ✅ | Tokenizer tests |

---

## 5. Build Configuration

**Main environment**: `esp32s3_n16r8` in `platformio.ini`

### Critical flags

```ini
board_build.arduino.memory_type = qio_opi   ; Flash QIO + PSRAM OPI — critical
board_build.flash_mode          = qio
board_upload.flash_size         = 16MB
board_build.partitions          = default_16MB.csv

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DUSE_FSPI_PORT                           ; SPI_PORT=2 — without: crash 0x10
    -DILI9341_DRIVER=1
    -DSPI_FREQUENCY=10000000                  ; 10 MHz — without: artifacts
    -DTFT_MOSI=13 -DTFT_SCLK=12
    -DTFT_CS=10   -DTFT_DC=4  -DTFT_RST=5
    -DTFT_BL=45
    -std=gnu++17

; CAS Tests (uncomment both to enable):
; -DCAS_RUN_TESTS
build_src_filter = +<*>
; +<../tests/CASTest.cpp>

monitor_speed   = 115200
monitor_filters = esp32_exception_decoder
monitor_rts     = 0
monitor_dtr     = 0
```

### Why `-DUSE_FSPI_PORT` is mandatory

`REG_SPI_BASE(0) = 0` on ESP32-S3. Without the flag, `TFT_eSPI::begin_tft_write()` writes to address `0x10` → **Guru Meditation: StoreProhibited**.  
With the flag: `SPI_PORT=2` → `REG_SPI_BASE(2) = 0x60024000` ✓

### Why LVGL buffers CANNOT be in PSRAM

The ESP32-S3 SPI DMA only accesses internal RAM. Buffers in PSRAM produce garbage transfers without explicit error — the screen stays silently black.

```cpp
// CORRECT:
buf1 = heap_caps_malloc(6400, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
// INCORRECT (black screen):
buf1 = ps_malloc(6400);
```

---

## 6. Current State (April 2026)

### In production

- ✅ Stable boot ESP32-S3 N16R8 CAM — no panics, lazy-init (no begin() at boot)
- ✅ ILI9341 IPS @10 MHz — no artifacts
- ✅ LVGL 9.5.0 double DMA buffer — launcher visible
- ✅ Animated SplashScreen
- ✅ SerialBridge — key echo, 5 s heartbeat
- ✅ LittleFS — persistent variables (proactive /vars.dat creation on first boot)
- ✅ CalculationApp — Natural VPAM, history 32, A-Z+Ans
- ✅ GrapherApp — y=f(x) zoom/pan, expression list, values table
- ✅ **Pro-CAS Engine** — SymExpr DAG, CASInt, CASRational, SymDiff, SymIntegrate, SymSimplify, OmniSolver, SymPolyMulti
- ✅ **EquationsApp** — 4 states, modes 1-var and 2×2 (linear + NL), PSRAM steps
- ✅ **CalculusApp** — Unified: symbolic derivatives (17 rules) + integrals (Slagle), tab-based d/dx ↔ ∯dx mode switching, simplification, steps
- ✅ **SettingsApp** — Complex roots toggle, decimal precision selector, angle mode
- ✅ **StatisticsApp** — Data lists, descriptive statistics (μ, σ, median, mode), histogram UI
- ✅ **ProbabilityApp** — nCr, nPr, n!, binomial, normal (PDF/CDF/inverse), Poisson
- ✅ **RegressionApp** — Linear/quadratic/log/exp regression, R², scatter plot
- ✅ **MatricesApp** — m×n editor, +/−/×/transpose, det (2×2, 3×3), inverse, Ax=b
- ✅ **SequencesApp** — Arithmetic/geometric sequences, Nth term, partial sums Sn
- ⚠️ **PythonApp** — Placeholder UI present; scripting engine pending Phase 8
- ✅ **Deferred teardown** — HOME key triggers 250 ms deferred end() to let FADE_IN animation complete safely
- ✅ **85+ CAS tests** — all passing (disabled in production)

### Build Stats

| Resource | Used | Total | % |
|:---------|-----:|------:|:-:|
| RAM | 97 040 B | 327 680 B | **29.6%** |
| Flash | 1 370 157 B | 6 553 600 B | **20.9%** |

### Pending

- ⏳ PythonApp scripting engine (Lua/MicroPython — Phase 8)
- ⏳ Table App (GrapherApp x/f(x) expansion)
- ⏳ Advanced CAS: definite integrals, complex numbers
- ⏳ Custom PCB, battery, 3D case, WiFi OTA

---

## 7. Code Style Guide

| Element | Convention | Example |
|:--------|:-----------|:--------|
| Classes | `PascalCase` | `CalculationApp`, `SymPoly` |
| Methods and functions | `camelCase` | `handleKey()`, `solveEquations()` |
| Member variables | `_prefix` | `_screen`, `_singleResult`, `_resultHint` |
| Parameters and locals | `camelCase` | `expr`, `poly`, `key` |
| Constants and macros | `UPPER_SNAKE_CASE` | `KEY_ROWS`, `BUF_SIZE` |
| Files | `PascalCase.cpp/.h` | `EquationsApp.cpp`, `SingleSolver.h` |
| LVGL lambdas | Inline `[](lv_event_t* e){ ... }` | See `MainMenu.cpp` |
| Includes | Relative to `src/` | `"math/cas/SymPoly.h"` |

**General rules**:
- No `using namespace std;` in header files `.h`.
- Prefer `constexpr`/`const` over `#define` for constants.
- LVGL callbacks: always `static` — object via `lv_event_get_user_data(e)`.
- `lv_obj_t*` member: null in `end()` to avoid dangling pointers.
- Free resources in `end()`, not in destructors (long-lived objects).

---

## 8. How to Add a New App

```cpp
// 1. Create src/apps/MyApp.h and MyApp.cpp
class MyApp {
public:
    void begin();                 // Create LVGL screen, init state
    void end();                   // Destroy screen, free resources
    void load();                  // App visible
    void handleKey(KeyCode key);  // Input
    void update();                // Tick ~60fps
};

// 2. In SystemApp.h:
#include "apps/MyApp.h"
MyApp _myApp;

// 3. In SystemApp::begin() — register in launcher:
_apps[N] = { "My App", Icons::myIconData, &_myApp, /*lvgl=*/true };

// 4. In ui/Icons.h — add 48×48 bitmap:
constexpr uint16_t myIconData[] = { /* RGB565 pixels */ };
```

---

## 9. How to Add a Math Function

To add `log₂(x)`:

1. **`Tokenizer.cpp/.h`**: Add `LOG2` to `enum class TokenType`. Recognize `"log2"` in lexer.

2. **`Parser.cpp`**: Add `LOG2` to functions map with precedence 5 (unary).

3. **`Evaluator.cpp`**:
```cpp
case TokenType::LOG2:
    a = stack.top(); stack.pop();
    stack.push(std::log2(a));
    break;
```

4. **`ExprNode.h`** (optional): If special rendering needed (subscript 2 under `log`).

5. **CAS-Lite** (optional): In `ASTFlattener::visitText()`, convert node to equivalent numerical value for polynomial analysis.

---

## 10. How to Extend the Pro-CAS

### New solver (ex. Cardano for degree 3)

```cpp
// In SingleSolver.cpp, branch degree==3:
// 1. Reduce to depressed form: t³ + pt + q = 0
// 2. Calculate discriminant Δ = -(4p³ + 27q²)
// 3. Δ>0: 3 real roots (trigonometric method)
//    Δ<0: 1 real + 2 complex
steps.add(StepType::INFO, "Cardano's Method (degree 3)");
```

### Symbolic Derivatives ✅ IMPLEMENTED

```cpp
// SymDiff.h/.cpp — 17 rules already implemented:
// d/dx(sin(u)) = cos(u) * u'
// d/dx(e^u)   = e^u * u'
// d/dx(ln(u)) = u'/u
// + chain, product, quotient, power, constant, etc.
// Access: SymDiff::differentiate(arena, expr, "x")
```

### Symbolic Integrals ✅ IMPLEMENTED

```cpp
// SymIntegrate.h/.cpp — Slagle heuristic:
// Strategies: direct table → linearity → u-substitution → parts (LIATE)
// Access: SymIntegrate::integrate(arena, expr, "x")
// Returns nullptr if cannot resolve (displayed as ∫ unevaluated)
```

### 3×3 System

```cpp
// Create SystemSolver3x3 in cas/SystemSolver.h
// Use extended Gaussian elimination with 3 equations
// Same pattern as SystemSolver (2×2) but with 3×4 augmented matrix
```

---

## 11. Troubleshooting

| Symptom | Probable Cause | Solution |
|:--------|:---------|:---------|
| `Guru Meditation: Illegal Instruction` on boot | PSRAM OPI not configured | `memory_type = qio_opi` |
| `Guru Meditation: StoreProhibited` in `TFT_eSPI::begin` | `SPI_PORT=0` → NULL ptr | `-DUSE_FSPI_PORT` in build_flags |
| Screen with lines / artifacts | SPI too fast | `SPI_FREQUENCY=10000000` |
| Black screen with LVGL active | Buffers in PSRAM | `heap_caps_malloc(MALLOC_CAP_DMA\|MALLOC_CAP_8BIT)` |
| Empty Serial Monitor / board resets | DTR/RTS resets on connect | `monitor_rts=0`, `monitor_dtr=0` |
| Serial output lost on boot | USB CDC not enumerated | `while(!Serial && millis()-t0<3000)` |
| LittleFS error on startup | No partition or `vars.dat` not exists | `LittleFS.begin(true)` — `formatOnFail=true` |
| Physical keyboard not responding | GPIO 4/5 shared TFT/keyboard | Reassign ROW3/ROW4 to free GPIOs |
| EquationsApp incorrect result | ASTFlattener didn't recognize node | Review `ASTFlattener::visit*()` |
| PSRAM grows between sessions | `end()` without `.clear()` in StepVec | Verify `_singleResult.steps.clear()` in `end()` |
| `ConstKind::Euler` doesn't compile | Enum uses `ConstKind::E` | Use `ConstKind::E` in `SymToAST.cpp` |
| App re-entry crash (NULL dereference in StatusBar) | `end()` missing `_statusBar.destroy()` | Add `_statusBar.destroy()` before `lv_obj_delete(_screen)` in every `end()` |
| HOME key freeze / no heartbeat (infinite loop in LVGL) | `lv_obj_delete` or `lv_obj_delete_async` called while FADE_IN animation holds screen reference | Use deferred teardown: `returnToMenu()` only records `_pendingTeardownMode`; `end()` called 250 ms later in `update()` |
| Hard Reset (Guru Meditation) on HOME key | Sync `lv_obj_delete` during live FADE_IN animation — same as above | Same fix: deferred teardown in `SystemApp` |

---

*NumOS — Open-source scientific calculator OS for ESP32-S3 N16R8.*
*Master documentation — last update: April 2026*
