# NumOS — Project Roadmap

&gt; **Last update:** March 2026
&gt;
&gt; Historical record and future plan for NumOS. Each phase builds upon the previous one until achieving an open-source scientific calculator that rivals the best on the market.

---

## General Progress

| Phase | Description | Status | % |
|:-----|:------------|:------:|:-:|
| **Phase 1** | Foundations — Math Engine and Drivers | ✅ Complete | 100% |
| **Phase 2** | Natural Display V.P.A.M. and 2D Navigation | ✅ Complete | 100% |
| **Phase 3** | Launcher 3.0, SerialBridge and Documentation | ✅ Complete | 100% |
| **Phase 4** | Migration to LVGL 9.x — HW Bring-Up ESP32-S3 | ✅ Complete | 100% |
| **Phase 5** | CAS-Lite Engine + EquationsApp (legacy milestone) | ✅ Complete | 100% |
| **CAS** | CAS-S3 internal milestones: BigNum, DAG, derivatives, integrals, unified CalculusApp, SettingsApp | ✅ **Complete** | 100% |
| **Giac Migration** | Big Switch: GiacBridge integration, UART parser/eval flow, DOUBLEVAL, 64 KB loop stack, real-style defaults | ✅ **Complete** | 100% |
| **Phase 6** | Complete Scientific Apps | ✅ **Complete** | 100% |
| **Phase 7** | Matrices + Complex + Bases | 🔲 Planned | 0% |
| **Phase 8** | Final Hardware + Connectivity + Scripting | 🔲 Planned | 0% |
| **Phase 9** | NeoLanguage — Hybrid Symbolic Programming Language | 🚧 In Progress | 30% |

---

## Milestone History

| Date | Milestone |
|:------|:-----|
| Feb 13 2026 | Project started (initial repo creation) |
| Feb 14 2026 | First tests of screen and initial project structure |
| Feb 2026 | Natural Display and Launcher development (ongoing) |
| Feb 2026 | Complete HW bring-up: 6 critical ESP32-S3 bugs resolved |
| Feb 2026 | LVGL 9.x operational — Launcher with icons on ILI9341 IPS |
| Feb 2026 | Animated splash screen + SerialBridge + LittleFS in production |
| **Feb 2026** | **Complete CAS-Lite Engine: SymPoly · SingleSolver · SystemSolver · 53 tests** |
| **Feb 2026** | **EquationsApp UI: linear, quadratic, 2×2 system with steps in PSRAM** |
| **Feb 2026** | **CAS Engine: CASInt, CASRational, SymExpr DAG, ConsTable, SymDiff (17 rules), SymIntegrate (Slagle), SymSimplify 8-pass, OmniSolver, SymPolyMulti (resultant)** |
| **Feb 2026** | **CalculusApp: symbolic derivatives with Natural Display and steps** |
| **Feb 2026** | **IntegralApp: Slagle integrals (table/u-sub/parts), +C, steps** |
| **Feb 2026** | **Active production: RAM 29.0% · Flash 18.5% · tests disabled** |
| **Mar 2026** | **Unified CalculusApp: derivatives + integrals in a single app with d/dx ↔ ∫dx tabs** |
| **Mar 2026** | **SettingsApp: complex roots toggle, decimal precision, angle mode** |
| **Mar 2026** | **Active production: RAM 28.8% · Flash 19.3% · tests disabled** |
| **Mar 2026** | **StatisticsApp, ProbabilityApp, RegressionApp, MatricesApp, SequencesApp, PythonApp landed via git pull — Phase 6 complete** |
| **Mar 2026** | **Boot crash fix: removed eager begin() calls — all apps lazy-init on first launch** |
| **Mar 2026** | **HOME Hard Reset fix → HOME freeze fix: deferred teardown 250 ms via _pendingTeardownMode in update()** |
| **Mar 2026** | **SettingsApp re-entry null crash fix: _statusBar.destroy() added to end()** |
| **Mar 2026** | **Active production: RAM 29.6% · Flash 20.9% · 11 apps in launcher** |
| **Mar 2026** | **BridgeDesignerApp (ID 12): Verlet physics bridge simulator with stress analysis, 3 materials, truck/car loads — PSRAM-backed 60Hz engine** |
| **Mar 2026** | **ParticleLabApp (ID 15) — Alchemy Update: 30+ materials, spark electronics, phase transitions, reaction matrix, Bresenham line tool, LittleFS save/load** |
| **Mar 2026** | **Active production: RAM 29.7% · Flash 23.2% · 16 apps in launcher** |
| **Mar 2026** | **OpticsLabApp (ID 17): 2D ray-tracing visualiser added — OpticsEngine (ABCD, Snell, paraxial + exact trace)** |
| **Mar 2026** | **NeoLanguageApp (ID 18): NeoLanguage compiler frontend — NeoLexer, NeoAST, NeoParser; two-tab IDE (Editor + Console)** |
| **Apr 2026** | **The Big Switch completed: symbolic backend migrated to Giac C++ through GiacBridge** |
| **Apr 2026** | **Embedded Giac baseline locked: DOUBLEVAL, loop stack 65536, real-style defaults with imaginary support preserved** |
| **Apr 2026** | **UART hardware certification complete for sum, int, solve, simplify command families** |
---

## Phase 1 — The Foundation (Complete)

&gt; *Objective: A system that evaluates mathematical expressions and displays them on screen.*

### Math Engine
- [x] **Tokenizer** (`Tokenizer.cpp`): String → list of `Token` — numbers, operators, functions, parentheses, variables.
- [x] **Shunting-Yard Parser** (`Parser.cpp`): Generates Reverse Polish Notation (RPN) for efficient numerical evaluation.
- [x] **RPN Evaluator** (`Evaluator.cpp`): Calculates the numerical result (`double`) with a stack. Trigonometry, logarithms, exponential, constants π and e.
- [x] **VariableContext** (`VariableContext.cpp`): Variables `A-Z` and `Ans`. O(1) read/write. LittleFS persistence.
- [x] **StepLogger** (`StepLogger.cpp`): Parser step logging for debugging.

### Low-Level Drivers
- [x] **DisplayDriver**: TFT_eSPI wrapper with double DMA buffer and LVGL initialization.
- [x] **KeyMatrix**: 6×8 matrix scanning — 20 ms debounce, 500 ms autorepeat, SHIFT/ALPHA support.
- [x] **Config.h**: Centralized pinout for ESP32-S3 N16R8 CAM.

---

## Phase 2 — Natural Display V.P.A.M. (Complete)

&gt; *Objective: Expressions are rendered exactly like on a modern physical calculator.*

### Visual Expression Tree (AST)
- [x] **ExprNode** (`ExprNode.h`): Dynamic tree — `TEXT`, `FRACTION`, `ROOT`, `POWER` nodes.
- [x] **Recursive measurement** `measure()`: Each node recursively calculates width and height in pixels.
- [x] **Recursive rendering** `draw()`: Draws each node in correct relative position to the parent block.

### 2D Navigation and Editing
- [x] **Smart cursor**: `RIGHT` at the end of the numerator jumps to the denominator. `UP/DOWN` in power enters/exits the superscript.
- [x] **Atomic deletion**: `DEL` destroys complex structures (fractions, roots, functions) as a single block.

### CalculationApp
- [x] **32-entry history** with vertical scroll.
- [x] **Copy results**: `UP` copies result or expression to the active editor.
- [x] **Variable support** A-Z + `Ans`. Full expression evaluation.

---

## Phase 3 — Launcher 3.0 and Integration (Complete)

&gt; *Objective: Complete app system with launcher, navigation, and debugging tools.*

### Launcher and SystemApp
- [x] **3-column grid**: Icon launcher inspired by NumWorks/Casio fx-CG.
- [x] **Vertical scroll**: Synchronized menu — selected icon always visible.
- [x] **10 Registered apps**: Calculation, Equations, Sequences, Grapher, Regression, Statistics, Table, Probability, Python, Settings.
- [x] **Event dispatcher**: `SystemApp::injectKey()` routes to MENU or active APP. `KeyCode::MODE` intercepted — always returns to the menu.

### Serial Bridge and Virtual Keyboard
- [x] **SerialBridge**: PC virtual keyboard via Serial Monitor — WASD, z/x/c/h, digits, operators.
- [x] **LvglKeypad**: LVGL 9.x `indev` adapter of type `LV_INDEV_TYPE_KEYPAD`.
- [x] **Heartbeat**: Serial ping every 5 s, immediate echo of each received byte.

---

## Phase 4 — LVGL 9.x: Visual Revolution (Complete)

&gt; *Objective: Abandon direct rendering and adopt LVGL for a professional-grade UI.*

### HW Bring-Up ESP32-S3 N16R8 CAM

| Fix | Problem | Applied Solution |
|:----|:---------|:------------------|
| ① | Flash/PSRAM boot mode mismatch | DIO ROM header + QIO second stage + `memory_type = qio_opi` |
| ② | Crash in `TFT_eSPI::begin()` addr `0x10` | `-DUSE_FSPI_PORT` → `SPI_PORT=2` → `REG_SPI_BASE(2)=0x60024000` |
| ③ | Lines and artifacts on screen | `SPI_FREQUENCY=10000000` (10 MHz) |
| ④ | LVGL screen always black | `heap_caps_malloc(DMA+8BIT)` — never `ps_malloc` for LVGL buffers |
| ⑤ | GPIO 45 BL short circuit | `pinMode(45, INPUT)` — pin hardwired to 3.3V |
| ⑥ | Serial CDC lost on boot | `while(!Serial && t0&lt;3000)` + `monitor_rts=0` |

### LVGL
- [x] `lv_conf.h`: `LV_MEM_CUSTOM=1` (PSRAM), `LV_TICK_CUSTOM=1`, `LV_COLOR_DEPTH=16`, Montserrat 12/14/20 fonts.
- [x] **Animated SplashScreen**: Fade-in logo + version with `lv_anim_t` ease_in_out.
- [x] **MainMenu LVGL**: 3×N grid with scroll, selection highlight, keyboard navigation.
- [x] **LittleFS**: Persistent variables with `formatOnFail`.

---

## Phase 5 — CAS-Lite Engine + EquationsApp (Complete, legacy milestone)

&gt; *Objective: Native symbolic algebra milestone with detailed steps before the Giac backend migration.*

### CAS-Lite Engine — Symbolic Algebra

- [x] **PSRAMAllocator\&lt;T\&gt;** (`cas/PSRAMAllocator.h`)
  STL-compatible allocator that redirects `allocate`/`deallocate` to `ps_malloc`/`ps_free`. The entire CAS lives in the 8 MB PSRAM OPI, without pressure on internal RAM.

- [x] **SymPoly** (`cas/SymPoly.h/.cpp`)
  Symbolic polynomial in a single variable. `Rational` coefficients (exact fraction `p/q`). Operations: addition, subtraction, multiplication, symbolic derivation, numerical evaluation, normalization.

- [x] **ASTFlattener** (`cas/ASTFlattener.h/.cpp`)
  Traverses the `ExprNode` visual AST and converts it into a `SymPoly`. Supports integer powers, π/e constants, and nested rational coefficients.

- [x] **SingleSolver** (`cas/SingleSolver.h/.cpp`)
  - Degree 1 → `x = -b/a` (direct solution)
  - Degree 2 → analytical quadratic formula with discriminant Δ = b² - 4ac
  - Degree ≥ 3 → Numerical Newton-Raphson (adaptive seed)
  - Generates detailed steps: normalization, Δ calculation, formula application, results

- [x] **SystemSolver** (`cas/SystemSolver.h/.cpp`)
  2×2 linear systems by symbolic Gaussian elimination. Automatically detects indeterminate (∞ solutions) and incompatible (no solution) systems.

- [x] **CASStepLogger** (`cas/CASStepLogger.h/.cpp`)
  `StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>`. Types: INFO · FORMULA · RESULT · ERROR. `.clear()` frees PSRAM memory upon exiting the app.

- [x] **SymToAST** (`cas/SymToAST.h/.cpp`)
  Reverse bridge: converts CAS `Rational` results into `ExprNode` nodes for Natural Display rendering in the EquationsApp.

### CAS Tests — 53 Unit Tests

| Group | Tests | What it validates |
|:------|:-----:|:-----------|
| **Phase A** — Fundamentals | 1–18 | `Rational` (exact arithmetic, simplification, GCD). `SymPoly` (add, sub, mul, diff, eval). |
| **Phase B** — ASTFlattener | 19–32 | AST → SymPoly conversion for polynomials, constants, powers, functions. |
| **Phase C** — SingleSolver | 33–44 | Linear (1 solution), quadratic (2 real roots, double root, Δ &lt; 0), with steps. |
| **Phase D** — SystemSolver | 45–53 | Determinate, indeterminate (∞ sol.), incompatible (no sol.) system. |

```ini
# platformio.ini — activate tests:
build_flags      = ... -DCAS_RUN_TESTS
build_src_filter = +<*> +<../tests/CASTest.cpp>
```

### EquationsApp

- [x] **Native LVGL UI** — 4 states: `SELECT` → `EQ_INPUT` → `RESULT` → `STEPS`
- [x] **Mode 1 — Equation (1 var)**: Linear and quadratic with discriminant and quadratic formula
- [x] **Mode 2 — 2×2 System**: Gaussian elimination, indeterminate/incompatible detection
- [x] **Steps screen**: `KeyCode::SHOW_STEPS` (R2C6) activates steps from result
- [x] **Natural Display in results**: x₁=2, x₂=3 rendered as visual expressions
- [x] **Memory management**: `end()` calls `.clear()` on `StepVec` — no memory leaks in PSRAM
- [x] **SystemApp Registration**: App id=5, `g_lvglActive=true`, correct lifecycle

### Build Stats (Production — tests disabled)

| Resource | Used | Total | % |
|:--------|------:|------:|:-:|
| RAM (data+bss) | 94 512 B | 327 680 B | **28.8%** |
| Flash (program) | 1 263 109 B | 6 553 600 B | **19.3%** |

---

## CAS Phase — CAS Engine + Unified Calculus App (Complete, legacy CAS-S3 milestones)

&gt; *Objective: CAS-Lite → CAS-S3 evolution (historical milestones) before switching the canonical symbolic backend to Giac. See [CAS_UPGRADE_ROADMAP.md](CAS_UPGRADE_ROADMAP.md).*

### CAS Engine — 6 Completed Phases

- [x] **Phase 0**: Research & Planning — SymExpr DAG design, hash-consing, bignum arithmetic
- [x] **Phase 1**: CASInt + CASRational — Hybrid BigInt (int64+mbedtls_mpi), overflow-safe fraction
- [x] **Phase 2**: SymExpr DAG + ConsTable + Arena — Immutable tree with hash-consing in PSRAM
- [x] **Phase 3**: SymSimplify + SymDiff — Fixed-point simplifier (8 passes), 17-rule derivation
- [x] **Phase 4**: ASTFlattener v2 + OmniSolver + HybridNewton — MathAST→SymExpr, advanced solver
- [x] **Phase 5**: SymPolyMulti + SystemSolver NL — Sylvester resultant, non-linear systems
- [x] **Phase 6A**: CalculusApp — Symbolic derivatives with Natural Display and detailed steps
- [x] **Phase 6B**: Unified CalculusApp — Merged derivatives + Slagle integrals (table/u-sub/parts), +C, ∫, steps, tab-based switching
- [x] **Phase 6B**: SymIntegrate — Heuristic Slagle: direct table, linearity, u-substitution, LIATE parts
- [x] **Phase 6B**: SymExprToAST — Bridge SymExpr → MathAST with `convertIntegral()` (+C)
- [x] **Phase 6C**: SettingsApp — Complex roots toggle, decimal precision, angle mode display
- [x] **Phase 7**: **Documentation** — All .md files updated for unified CalculusApp + SettingsApp, build stats, keyboard 5×10
- [x] **Phase 8**: **Scientific Apps (Phase 6)** — StatisticsApp, ProbabilityApp, RegressionApp, MatricesApp, SequencesApp, PythonApp
- [x] **Phase 9**: **Stability** — Boot lazy-init, HOME deferred teardown 250 ms, SettingsApp null crash fix
- [x] **Phase 10**: **Simulation Apps** — CircuitCoreApp (SPICE), Fluid2DApp (Navier-Stokes), ParticleLabApp (Alchemy Update)

## Giac Migration Phase — Symbolic Backend Switch (Complete)

&gt; *Objective: migrate from custom symbolic backend to Giac while preserving NumOS UX and educational workflows.*

- [x] Canonical symbolic backend switched to Giac C++.
- [x] Integration consolidated through `src/math/giac/GiacBridge.cpp`.
- [x] UART command path certified via `src/input/SerialBridge.cpp`.
- [x] Embedded stability baseline completed: `-DDOUBLEVAL`, `-DARDUINO_LOOP_STACK_SIZE=65536`.
- [x] Real-style defaults completed: `complex_mode(false)` and preserved `i^2 = -1` behavior.

### Build Stats (Production — March 2026)

| Resource | Used | Total | % |
|:--------|------:|------:|:-:|
| RAM | 97 192 B | 327 680 B | **29.7%** |
| Flash | 1 518 269 B | 6 553 600 B | **23.2%** |

---

## Phase 6 — Complete Scientific Apps (✅ Complete)

&gt; *Objective: NumOS becomes a complete scientific calculator for real academic use, surpassing the Casio fx-991EX in features.*

### 6.1 Statistics App
- [x] Introduction of data lists (up to 200 elements with scroll)
- [x] Arithmetic mean, median, mode, range
- [x] Variance and standard deviation (population σ and sample s)
- [x] Histogram and box plot on screen
- [x] Percentiles and quartiles Q1/Q2/Q3

### 6.2 Regression App
- [x] Linear regression (a + bx) with R² coefficient and line equation
- [x] Quadratic regression (a + bx + cx²)
- [x] Logarithmic and exponential regression
- [x] Scatter plot with superimposed fitted curve in grapher

### 6.3 Sequences App
- [x] Arithmetic sequences: first term, common difference, Nth term, partial sum SN
- [x] Geometric sequences: first term, common ratio, Nth term, sum N
- [x] Automatic type verification (arithmetic / geometric / neither)
- [x] Scrollable table of first N terms

### 6.4 Probability App
- [x] Combinations nCr and permutations nPr
- [x] Factorial n! (up to 20!)
- [x] Binomial distribution: P(X=k), P(X≤k)
- [x] Normal distribution: density, cumulative Φ(z), inverse
- [x] Poisson distribution: P(X=k)

### 6.5 Table App (GrapherApp expansion)
- [ ] x/f(x) table with configurable step (Δx)
- [ ] Vertical scroll of rows, fixed column width
- [ ] Synchronized with the active function in GrapherApp

### 6.6 Settings App ✅ Complete
- [x] Complex roots toggle (ON/OFF)
- [x] Decimal precision selector (6/8/10/12)
- [x] Angle mode display (informational)
- [ ] Screen brightness (PWM if BL reconnected to GPIO OUTPUT)
- [ ] Number format: fixed decimal / scientific / engineering
- [ ] Factory reset: delete all variables, restore configuration
- [ ] System information: firmware version, free RAM, free Flash

### 6.7 MatricesApp ✅ Complete
- [x] m×n matrix editor with 2D navigation on screen
- [x] Operations: addition, subtraction, multiplication, transpose
- [x] 2×2 and 3×3 determinant
- [x] Inverse by Gauss-Jordan
- [x] Resolution of the Ax = b system by matrices

### 6.8 ParticleLabApp — The Alchemy Update ✅ Complete

&gt; *Powder-Toy-class cellular automata sandbox with 30+ materials, discrete electronics, and phase transitions.*

#### Material Library (31 materials)
- [x] **Earth & Glass**: Sand (&gt;1500°C → Molten Glass), Molten Glass (&lt;1000°C → Glass), Stone (inert, heavy), Glass
- [x] **Organics**: Wood (burns → Smoke), Coal (burns 10× longer than wood), Plant (2% chance to grow near Water)
- [x] **Thermal Extremes**: Lava (1500°C, cools &lt;800°C → Stone), LN2 (Liquid Nitrogen, -196°C, evaporates &gt;-190°C → Gas)
- [x] **Electronics**: Wire (conductive), Heater (sparked → 2000°C), Cooler (sparked → -200°C), C4 (sparked → massive explosion)
- [x] **Advanced Solids**: HEAC (extremely high heat conductor), INSL (heat/electricity insulator, burns), Titan (melts 1668°C, conductive), Iron (melts 1538°C, conductive)
- [x] **Special**: Clone (reads & replicates adjacent material), Smoke (gas, dissipates), Molten Titan

#### Cellular Automata Engine
- [x] LUT-driven material properties (color, density, flammability, state, thermal/electric conductivity, phase temps)
- [x] Spark cycle: conductive materials carry sparks with PF_SPARKED flag, 2-frame propagation, Joule heating
- [x] Reaction matrix: Water+Lava=Stone+Steam, Acid+Iron=Gas, Water+LN2=Ice
- [x] Phase transitions: solid→liquid (melting), liquid→gas (boiling), liquid→solid (cooling)
- [x] Gas diffusion into empty spaces, liquid equalization (aggressive sideways flow)
- [x] Bitwise PF_UPDATED flag prevents double-update per tick

#### UI / QoL
- [x] Material palette overlay (F3): pause + grid selector with D-pad navigation
- [x] Bresenham line tool: hold ENTER and move cursor to draw connected lines
- [x] Brush shapes: Circle, Square, Spray (random fill ~40% density)
- [x] InfoBar HUD: MAT name | Brush | Cursor XY | Particle count | Paused state
- [x] Quick Save (F4) and Quick Load (F5) via LittleFS (`/save.pt`, 76.8 KB grid)
- [x] Cold glow rendering for sub-zero temperatures
- [x] Sparked particle visual overlay (yellow tint)
- [x] Temperature glow (black-body radiation) for hot particles

---

## Phase 7 — Matrices + Complex + Bases (Planned)

&gt; *Objective: Achieve parity with the HP Prime G2 and NumWorks CAS.*

### 7.1 Advanced Simplification and Factorization
- [ ] Reduction of like terms: 2x + x → 3x
- [ ] Polynomial factorization: x² - 5x + 6 → (x-2)(x-3)
- [ ] Expansion of notable products
- [ ] Cancellation in algebraic fractions
- [ ] High-precision numerical definite integral (Gauss-Legendre)
- [ ] Visualization: shaded area under the curve in grapher
- [ ] Taylor / Maclaurin series

### 7.2 Matrices
- [ ] m×n matrix editor with 2D navigation on screen
- [ ] Operations: addition, subtraction, multiplication, transpose
- [ ] 2×2 and 3×3 determinant
- [ ] Inverse by Gauss-Jordan
- [ ] Resolution of the Ax = b system by matrices
- [ ] Eigenvalues for 2×2 matrices

### 7.3 Complex Numbers
- [ ] Complex mode activatable in Settings
- [ ] Rectangular form (a+bi) and polar form (r∠θ) input
- [ ] Basic operations: +, -, ×, ÷, conjugate, modulus, argument
- [ ] Argand plane (graphical visualization in GrapherApp)

### 7.4 Base Algebra and Unit Conversion
- [ ] Integrated DEC / HEX / BIN / OCT converter
- [ ] Arithmetic operations in arbitrary base n
- [ ] Unit converter: length, mass, temperature, speed, energy

---

## Phase 8 — Final Hardware + Connectivity + Scripting (Planned)

&gt; *Objective: NumOS becomes a complete, portable, autonomous, and connected physical product.*

### 8.1 Physical Keyboard — GPIO Conflict Resolution
- [ ] Reassign ROW3 (GPIO 4) and ROW4 (GPIO 5) to free GPIOs (proposal: GPIO 15, 16)
- [ ] Update `Config.h` with new assignments
- [ ] Integrate `KeyMatrix::scan()` → `LvglKeypad::pushKey()` in real time
- [ ] Complete 48-key test with `HardwareTest.cpp`
- [ ] SHIFT/ALPHA visual overlay in status bar (active layer)
- [ ] Secondary functions (yellow/red keys) mapped to `KeyCode::SHIFT_X`

### 8.2 Custom PCB
- [ ] Complete schematic in KiCad with integrated ESP32-S3 WROOM
- [ ] FPC/ZIF connector for ILI9341 screen
- [ ] 2-pin JST-PH connector for LiPo battery
- [ ] TP4056 with USB-C charging + MT3608 boost converter (3.7V → 5V)
- [ ] JTAG + SWD test points for debugging
- [ ] 2-layer PCB layout, calculator form factor (≈85×165mm)

### 8.3 Battery and Power Management
- [ ] 1000–2000 mAh LiPo (depending on chassis volume)
- [ ] TP4056: USB-C charging, LED indicator
- [ ] Battery level monitor by ADC (free GPIO) with percentage conversion
- [ ] Graphical indicator in status bar (animated battery icon)
- [ ] Low power mode: reduce LVGL refresh rate + lower CPU frequency
- [ ] Deep sleep with wake-up by dedicated ON/OFF key

### 8.4 3D Case
- [ ] Design in FreeCAD or Fusion 360 following dimensions in `DIMENSIONES_DISEÑO.md`
- [ ] Material: PLA or PETG, matte black / gray colors
- [ ] Screen window with bezel and acrylic glass protection
- [ ] Support for keyboard membrane or tactile buttons
- [ ] Back cover with M2 screws and accessible battery compartment

### 8.5 WiFi Connectivity
- [ ] **WebBridge**: embedded HTTP server in the ESP32-S3 to transfer programs/scripts from the PC
- [ ] **OTA (Over The Air)**: wireless firmware update from the browser
- [ ] **Variable synchronization**: `/vars.dat` backup and restore via WiFi
- [ ] **NTP**: time synchronization for internal clock and time logging

### 8.6 Scripting
- [ ] **Embedded Lua** (eLua / LittleLua) — scripting language to program the calculator
- [ ] On-screen script editor with 2D cursor and basic syntax highlighting
- [ ] Access to all mathematical functions from Lua
- [ ] Scripts saved to/loaded from LittleFS
- [ ] Execution by line (`ENTER`) or full block (`SHIFT+ENTER`)

### 8.7 Additional Mathematical Functions
- [ ] `log₂(x)`, `logₙ(x, b)` — logarithm in arbitrary base
- [ ] `ceil`, `floor`, `round`, `frac` — rounding
- [ ] `mod` — modulo operator for integers
- [ ] `gcd`, `lcm` — greatest common divisor / least common multiple
- [ ] Direct DEG↔RAD↔GRA angular projection in result
- [ ] `Σ` — Summation over expression (configurable lower/upper limit)
- [ ] `Π` — Product
- [ ] `∫` — High-precision numerical definite integral

---

## Phase 9 — NeoLanguage: Hybrid Symbolic Programming Language (🚧 In Progress)

&gt; *Objective: Give NumOS a native, hybrid programming language that blends Python's clean syntax with Wolfram Language's native symbolic mathematics — running directly on the ESP32-S3.*

### 9.1 Compiler Frontend (Phase 1 — Complete)

**NeoLexer** (`src/apps/NeoLexer.h` / `NeoLexer.cpp`)
- [x] State-machine tokenizer with 40+ token types
- [x] Python-style INDENT / DEDENT generation from indentation levels
- [x] Math-first operators: `+`, `-`, `*`, `/`, `^`, `**` (power), `:=` (delayed assignment)
- [x] Keywords: `def`, `if`, `elif`, `else`, `while`, `for`, `in`, `return`, `and`, `or`, `not`, `True`, `False`, `None`
- [x] String literals with escape sequences
- [x] Single-line comments with `#`
- [x] Line & column tracking in every Token (precise error reporting)
- [x] PSRAM-backed token list (`PSRAMAllocator<Token>`)
- [x] Error tokens with descriptive messages (no crashes)

**NeoAST** (`src/apps/NeoAST.h`)
- [x] `NeoArena` bump allocator using `heap_caps_malloc(MALLOC_CAP_SPIRAM)` (PSRAM) with `std::malloc` fallback for non-Arduino builds
- [x] Complete node hierarchy (13 node kinds): `Number`, `Symbol`, `BinaryOp`, `UnaryOp`, `FunctionCall`, `Assignment`, `If`, `While`, `ForIn`, `FunctionDef`, `Return`, `SymExprWrapper`, `Program`
- [x] `NumberNode`: stores `double` value + exact CASRational (int64 numerator/denominator) + raw text
- [x] `AssignmentNode`: distinguishes `=` (standard) from `:=` (delayed/symbolic, Wolfram-style)
- [x] `SymExprWrapperNode`: CAS integration hook holding a `void* symexpr_ptr` to a CAS `SymExpr` DAG node + string `repr`
- [x] All nodes carry `line` and `col` for error messages

**NeoParser** (`src/apps/NeoParser.h` / `NeoParser.cpp`)
- [x] Recursive descent parser with Pratt expression parser (precedence climbing)
- [x] Symbolic semantics: undefined variables parsed as `SymbolNode` (not errors)
- [x] Dual function-definition syntax: `def f(x): return x^2 + 1` and `f(x) := x^2 + 1`
- [x] Full control flow: `if`/`elif`/`else` chains, `while`, `for x in iterable`
- [x] Panic-mode error recovery: `syncToNextStatement()` skips to next `NEWLINE`/`DEDENT`
- [x] Iterative program-body loop to avoid stack overflow on ESP32
- [x] Correct operator precedences: `^` &gt; `* /` &gt; `+ -` &gt; comparisons &gt; `and`/`or`

**NeoLanguageApp** (`src/apps/NeoLanguageApp.h` / `NeoLanguageApp.cpp`)
- [x] Two-tab LVGL IDE: **Editor** (code textarea) + **Console** (output/errors)
- [x] F5 = compile: tokenize → parse → display AST summary in console
- [x] F1 = insert 4-space tab indent
- [x] MODE = exit (SystemApp handles return-to-menu)
- [x] Dark Catppuccin-Mocha colour palette (editor `#1E1E2E`, console `#11111B`)
- [x] Monospaced `lv_font_unscii_8` for editor and console
- [x] Arena reset on each compile run (no memory accumulation between runs)

### 9.2 Compiler Middle-End (Planned)
- [ ] Semantic analysis: scope checking, type inference, undefined variable detection
- [ ] CAS integration: auto-convert `SymbolNode` subtrees to `SymExpr` DAG nodes
- [ ] Constant folding and algebraic simplification pass
- [ ] Symbolic evaluation: `x := 3`, then `x^2 + 1` → `10`

### 9.3 Compiler Back-End / Interpreter (Planned)
- [ ] Tree-walking interpreter (NeoEval) with variable environment
- [ ] Standard library: `sin`, `cos`, `tan`, `sqrt`, `abs`, `range`, `len`, `print`
- [ ] CAS bridge: call `SymDiff`, `SymIntegrate`, `SymSimplify` from NeoLanguage code
- [ ] Native print → console textarea output
- [ ] LittleFS script storage (load/save `.neo` files)
- [ ] REPL mode in Console tab

### 9.4 Language Extensions (Planned)
- [ ] Matrix literals: `[[1,2],[3,4]]` → `MatrixNode`
- [ ] Unit annotations: `9.8 [m/s^2]`
- [ ] Pattern matching: Wolfram-style `f[x_] := ...`
- [ ] Lambda expressions: `fn := x -> x^2 + 1`

---

# Future Apps:

## 🔐 CipherForge (Complete Cryptography Lab)

### 📝 General Description
**CipherForge** is the expanded successor to CryptoLearn, designed to simultaneously be the best cryptography educational resource available on embedded hardware **and** a functional tool that a cryptographer, security researcher, or CTF competitor can seriously use. Each module operates in two selectable modes:

* **Educational Mode** (`EDU`): Step-by-step animations, inline explanations, visualization of internal structures. To learn.
* **Tool Mode** (`TOOL`): Direct input/output, without pedagogy. To work.

The underlying engine is `CASInt` (Arbitrary Precision BigNum) from NumOS, which enables RSA, factorization, and modular arithmetic with 512–2048-bit real numbers.

---

### 🧩 Module 1: Classic Ciphers

The most comprehensive historical cryptography lab available on a calculator. All ciphers are bidirectional (encrypt/decrypt) and work on text entered by the user.

#### 🔤 Replacement
| Encryptor | Features |
| :--- | :--- |
| **Caesar / ROT-N** | Displacement $k$ configurable (ROT-13 as a special case). Brute force attack with automatic scoring by language frequency (Spanish/English selectable). |
| **Affine** | Clue $(a, b)$ with automatic verification of $\gcd(a, 26) = 1$. Shows the function $E(x) = (ax + b) \mod 26$ and its inverse. |
| **Vigenère / Beaufort / Autoclave** | Three variants of the same common key scheme. Visualization of the Vigenère tableau with the key highlighted letter by letter. |
| **Hill cipher** | Block encryption $\mathbb{Z}_{26}$ using 2×2 or 3×3 matrices. EDU mode shows modulo 26 matrix multiplication step by step, including calculating the modular inverse of the matrix to decipher. Requires engine `CASInt`. |
| **Playfair** | Display of the 5x5 grid built from the key. The peer-to-peer process is animated letter by letter showing which rule applies (same row, same column, rectangle). |
| **Four-Square** | Double grid, bifid polybio. |
| **Polybius** | Grid coordinates with configurable alphabet. |

#### 🔀 Transposition
| Encryptor | Features |
| :--- | :--- |
| **Columnar** | Visual layout of the plain text table with the columns reordered according to the key. TOOL mode allows numerical or alphabetical key. |
| **Rail Fence** | Zigzag animation with $n$ Configurable rails. Displays text diagonals before reading rows. |
| **Route Cipher** | Spiral, snake or diagonal reading of the matrix. |
| **Double Transposition** | Composition of two Columnars with different keys (one of the most resistant of the pre-computational era). |

#### ⚙️ Enigma Machine (Full Simulation)
The jewel of the classic module. Complete simulation of the 3-rotor Enigma Wehrmacht.

* **Simulated Hardware**: 3-rotor selector (I–V), initial position (Grundstellung), adjustment ring (Ringstellung) and plugboard (Steckerbrett) with up to 10 pairs of interchanged letters.
* **Internal Display**: The EDU mode shows the path of the electrical signal letter by letter: input → plugboard → right rotor → central rotor → left rotor → reflector → turn → plugboard → output. Each stage lights up in sequence.
* **Rotor Advance**: Animation of the "double advance" (real mechanical anomaly of the Enigma) with the three rotors visibly rotating.
* **Historical**: Presets of documented historical military configurations (Wehrmacht Feb 1942, Luftwaffe, etc.) to play real decrypted messages.
* **Bidirectional**: Like the real Enigma, the same ciphertext is decrypted using the same configuration (involution).

---

### 🧩 Module 2: Classic Cryptanalysis

Not just encrypt, but **break**. This module turns NumOS into an active analysis tool.

* **Frequency Analysis**: Interactive letter frequency histogram of the ciphertext, superimposed on the standard distribution of the selected language (Spanish, English, French, German). The user drags letters to propose substitutions and the partially deciphered text is updated in real time.

* **Coincidence Index (CI)**: Automatic calculation of the CI of the text. A CI ≈ 0.065 suggests monoalphabetic substitution; CI ≈ 0.038 suggests Vigenère. The visual marker indicates where the text falls on the spectrum.

* **Kasiski Test**: Automatically detects repeated sequences (trigrams or more) in the ciphertext and lists the distances between repetitions. The module factors these distances and suggests the most likely key length for Vigenère.

* **Automatic Vigenère Solver**: With the key length estimated by Kasiski, separates the text into $k$ monoalphabetic flows, applies frequency analysis to each one and proposes the complete key. The user can correct individual letters of the proposed key.

* **XOR Key Recovery**: For repeated key XOR ciphertext (very common scheme in CTF), detects the key period (autocorrelation), extracts each key byte by frequency analysis of the most common byte and displays the recovered key in hex/ASCII.

* **Bigrams and Trigrams**: Frequency of sequences of 2–3 characters compared against language tables. Useful for breaking transposition and confirming substitutions.

* **CTF mode**: Compact view that launches all analyzes in parallel and scores the readability of the result (ratio of recognized words from the embedded dictionary). The candidate with the highest score is automatically highlighted.

---

### 🧩 Module 3: Coding and Representation

Reference tool for conversion and encoding. All in a single multi-base display.

* **Universal Converter**: Enter a value in any base and all others are updated in real time.

| Base | Format |
| :--- | :--- |
| Binary (base 2) | With or without spaces every 8 bits |
| Octal (base 8) | |
| Decimal (base 10) | With thousands separators |
| Hexadecimal (base 16) | Uppercase/lowercase, with or without `0x` |
| Base32 | RFC4648 |
| Base58 | Bitcoin variant (without 0, O, I, l) |
| Base64 | Standard and URL-safe (`+/` vs `-_`) |
| Base85 | ASCII85 variant |
| ASCII/UTF-8 | View of raw bytes in hex |

* **URL Encoding / HTML Entities**: Encodes and decodes special characters in both standards.
* **Morse**: Text ↔ Morse in both directions. Audible playback through the hardware buzzer (if available).
* **Leetspeak / Full ROT-N**: All rotations from 0 to 25 in a single table for ROT-N; useful for CTF.
* **Endianness**: View any 1–8 byte value in little-endian and big-endian simultaneously.

---

### 🧩 Module 4: Number Theory (Mathematical Engine)

The mathematical core of asymmetric cryptography, exposed as a standalone tool. use the engine `CASInt` for arbitrary precision.

#### Modular Operations
* **Modular exponentiation**: $a^b \mod n$ with fast exponentiation (square-and-multiply). EDU mode displays the bit-by-bit algorithm.
* **Modular inverse**: $a^{-1} \mod n$ using the extended Euclid algorithm. Shows the Bezout coefficients $s, t$ such that $as + nt = \gcd(a,n)$.
* **GCD / Extended Euclid**: Table of algorithm steps, row by row.
* **Chinese Remainder Theorem (CRT)**: Given a system $x \equiv a_i \pmod{n_i}$, calculates the unique solution modulo $\prod n_i$. Up to 6 congruencies.

#### Primality and Factorization
* **Miller-Rabin test**: First determine if $n$ is cousin (with $k$ configurable rounds). EDU mode explains witnesses and decay $n-1 = 2^s \cdot d$.
* **Solovay-Strassen test**: Based on the Jacobi symbol. Useful to compare with Miller-Rabin in educational context.
* **Pollard ρ factorization**: For composite numbers up to ~60 bits factorable in seconds on S3. Floyd cycle animation visible.
* **Sieve of Eratosthenes**: Visual and interactive up to $n \leq 10^6$, stored in PSRAM.
* **Euler function φ(n)**: Calculated from factorization.
* **Discrete Logarithm (Baby-Step Giant-Step)**: Solve $g^x \equiv h \pmod{p}$ for $p$ little. Essential to understand the security of DH and ElGamal.
* **Prime Generator**: Generate cousins of $k$ bits (configurable) using Miller-Rabin iterated over random candidates. It uses the hardware noise generator as a source of entropy.

---

### 🧩 Module 5: Modern Symmetric Cryptography

#### 🟦 AES (Full Internal Display)
The star module of symmetric cryptography. It doesn't just encrypt: **opens the AES black box**.

* **Modes**: AES-128 (10 rounds), AES-192 (12) and AES-256 (14).
* **Round by Round Display**: The screen shows the 4x4 byte block of status updated after each operation:
    - **SubBytes**: Substitution using the S-box in $GF(2^8)$. The EDU mode explains the construction of the S-box as an inversion in the finite field plus an affine transformation.
    - **ShiftRows**: The state rows cyclically shift $0, 1, 2, 3$ positions. Animation of the rows moving.
    - **MixColumns**: Multiplication of columns by the circulating matrix in $GF(2^8)$. EDU mode shows multiplication of polynomials modulo $x^4+1$ step by step — the only really complex part of AES.
    - **AddRoundKey**: XOR the state with the round subkey. Displays the key expansion (Key Schedule) as a drop-down tree.
* **Operation Modes**: ECB, CBC, CTR. In ECB mode with input image, visually shows why ECB is insecure (the "ECB penguin" — plaintext patterns survive).
* **Avalanche Effect**: Change 1 bit of the plaintext or key and a counter shows how many bits of the encrypted block have changed per round. Visualization as heatmap of the block.

#### 🔁 Feistel Network (DES Educational)
* **Feistel Structure**: Interactive diagram of the 16-round network. User can "open" each round and see the feature $f$ applied (E expansion, XOR with subkey, S-boxes, P permutation).
* **DES S-boxes**: Interactive tables where the user enters a 6-bit value and traces the path to the 4-bit output.
* **Double and Triple DES**: Demonstration of the Meet-in-the-Middle attack that makes 2DES almost as weak as single DES.

#### 🌊 ChaCha20 / Salsa20
* **Quarter Round**: Animation of the 4 ARX operations (Add, Rotate, XOR) that form the basic block.
* **Keystream**: Visualization of the 512-bit state block evolving in 20 rounds until producing the keystream.
* **AES vs ChaCha20 comparison**: Side-by-side panel showing why ChaCha20 is resistant to timing attacks (without lookup table operations).

---

### 🧩 Module 6: Asymmetric Cryptography

#### 🔑 RSA (Real Step by Step)
use the engine `CASInt` to run RSA with real key sizes (up to 512 bits in demo, full structure for 2048 bits with estimated times).

* **Key Generation**:
    1. Generate two large primes $p, q$ (Miller-Rabin).
    2. Calculate $n = pq$, $\phi(n) = (p-1)(q-1)$.
    3. Choose $e$ (65537 by default) and calculate $d = e^{-1} \mod \phi(n)$ via extended Euclid.
    4. Sample public key pair $(n, e)$ and private $(n, d)$ in hex
* **Encryption/Decryption**: Execute $c = m^e \mod n$ and $m = c^d \mod n$ with real text.
* **RSA Digital Signature**: $s = h^d \mod n$, verification $s^e \mod n = h$.
* **Factorization Attack**: For small keys (<64 bits), cast Pollard ρ live and show how long it takes to factor $n$ — making tangible why small modules are insecure.
* **OAEP Padding**: Visual explanation of why RSA textbook (without padding) is insecure and how OAEP randomizes the message.

#### 🔄 Diffie-Hellman (Key Exchange)
* **Visual Analogy**: The classic animated "color mixing" diagram, showing that the shared secret can be calculated from two different paths.
* **DH Real**: Simultaneously with the analogy, the bottom panel shows the real math: $g^a \mod p$ and $g^b \mod p$ as hex values, and how $(g^a)^b = (g^b)^a = g^{ab} \mod p$.
* **Group Parameters**: Predefined RFC 3526 groups (MODP Group 14, 2048 bits) or custom parameters.
* **Logjam Attack**: Demo of why small or weak groups (512 bits) are vulnerable to discrete logarithm with BSGS.

#### 📐 Elliptic Curves (ECC)
The most advanced module, with real geometric visualization.

* **Display on ℝ**: The curve $y^2 = x^3 + ax + b$ rendered on screen. The user places two points $P$ and $Q$; The app draws the secant line, finds the intersection and reflects to get $P + Q$ geometrically.
* **Addition of Points (Algebraic)**: Parallel panel showing slope formulas $\lambda$ and the coordinates of $P + Q$, evaluated numerically.
* **Curves on $\mathbb{F}_p$**: Switch to switch to modular arithmetic. The curve becomes a discrete set of points. The user can list all the points in the group to $p$ little.
* **Scalar Multiplication $kP$**: Animation of the double-and-add algorithm, showing each intermediate step in the curve.
* **ECDH**: Key exchange demo: Alice chooses $k_A$, Bob chooses $k_B$; the shared secret $k_A \cdot k_B \cdot G = k_B \cdot k_A \cdot G$.
* **Standard Curves**: Predefined parameters of secp256k1 (Bitcoin), P-256 (NIST), Curve25519. Shows the order of the group $n$ and the generator $G$.
* **ECDSA**: Signature generation $(r, s)$ and verification. Explanation of why to reuse the nonce $k$ compromises the private key (the PlayStation 3 bug).

#### 🔏 ElGamal
* Cyclic group encryption: key generation, pair encryption $(c_1, c_2)$, deciphered.
* Direct analogy with DH to see the common structure.

---

### 🧩 Module 7: Hash Functions

#### Functional Implementations
They all produce the correct and verifiable hash of the entered text or file.

| Hash | Length | Note |
| :--- | :--- | :--- |
| MD5 | 128 bit | Marked as "broken" with explanation |
| SHA-1 | 160 bit | Marked as "deprecated" |
| SHA-256 | 256 bit | Current standard |
| SHA-512 | 512 bit | |
| SHA-3/Keccak | 224/256/384/512 bit | Sponge construction |
| BLAKE2s | 256 bit | Optimized for 32 bits, ideal on ESP32-S3 |
| CRC32 | 32 bit | Not cryptographic, but ubiquitous |

#### Views

* **Avalanche Effect**: Enter two texts that differ by 1 bit/character. A 256-bit heatmap shows which bits of the hash change. Counter of bits changed (ideally ~50%).
* **SHA-256 Compression Step by Step**: The 64 rounds of the animated compression function: the 8 working variables $a$–$h$, the sum of messages $W_i$ and the constants $K_i$.
* **Sponge Construction (SHA-3)**: Diagram of the "absorb" and "squeeze" phases of the Keccak construction, with internal permutation $f$ represented as a state transformation.
* **HMAC**: Visualization of the double application of the hash with the keys `ipad`/`opad`. Helpful to understand why just do `hash(clave || mensaje)` is unsafe (length extension attack).

#### Key Derivation
* **PBKDF2**: Configuration of iterations, salt and output length. Shows why the number of iterations makes the dictionary attack expensive.
* **bcrypt (Conceptual)**: Explanation of the cost factor and the underlying Blowfish function. Not fully implemented due to computational cost, but the EDU mode explains the structure.
* **HKDF**: Key extraction and expansion from pseudorandom material.

---

### 🧩 Module 8: Advanced Protocols and Schemes

#### 🤝 Shamir's Shared Secret
One of the most elegant schemes in cryptography, based on polynomial interpolation.

* **Setup**: The user defines the secret $s$, the threshold $k$ and the total number of participants $n$.
* **Visualization**: The app draws the random degree polynomial $k-1$ about $\mathbb{F}_p$, mark the $n$ points (the shares) and shows that with less than $k$ points cannot reconstruct the secret.
* **Reconstruction**: User selects $k$ shares any and interpolates the Lagrange polynomial to recover $f(0) = s$. The Lagrange coefficients are shown numerically.
* **Real Application**: Demo of how to save a private key divided between 3 people so that 2 of them can recover it.

#### 🎲 One-Time Pad (Perfect Secrecy)
* Formal Demo of Shannon's Proof: Given Any Ciphertext $c$, there exists exactly one key that maps it to any possible plaintext of the same length.
* Practical demonstration of why reusing the key breaks the OTP (two-time pad attack with two-message XOR).

#### 🤝 Commitment Schemes
* Hash Commitment: The user "commits" a secret value by publishing its hash, then reveals it. Useful for understanding basic fair coin and ZKP protocols.

#### 🔐 Introduction to Zero-Knowledge Proofs
* **Ali Baba's Cave**: Interactive animation of the most famous ZKP protocol. Shows how Peggy demonstrates knowledge of the secret without revealing it, with multiple rounds to reduce the likelihood of deception ($1/2$ per round).
* **Schnorr Protocol**: Demo of the Schnorr cyclic group identification protocol — the basis of many modern signatures.

---

### 🧩 Module 9: CTF Toolkit

Pure work mode, without pedagogy. Competition tool panel.

| Tool | Function |
| :--- | :--- |
| **XOR Brute Force** | Test 1–4 byte XOR keys, score for readability in English/Spanish |
| **Hex Dump** | Hex+ASCII view of any input, style `xxd` |
| **ROT-N Table** | All 25 rotations on a single screen table |
| **Multi-Decoder Chain** | Applies chain decoder sequences (Base64 → XOR → Hex → ASCII) |
| **String Extractor** | Extract printable ASCII sequences from binary data |
| **Pattern Generator** | Generates cyclic patterns to detect offsets in buffer overflows |
| **Byte Frequency Map** | 256-byte histogram of the input; useful for detecting encryption, compression or random data |
| **Hash Diff** | Compare two hashes and count different bits (Hamming distance) |
| **Rail Fence Solver** | Try all the values of $n$ rails up to 20 and score the result |
| **Columnar Auto-Solver** | Test column permutations guided by bigram analysis |

---

### 🛠️ Technical Implementation

| Component | Detail |
| :--- | :--- |
| **BigNum Engine** | `CASInt` from NumOS: arbitrary precision integers up to ~4096 bits in PSRAM. Module, modular boosting and GCD in hardware. |
| **Entropy** | S3 noise ADC + timer jitter as a source of randomness for prime and nonce generation. |
| **Dual-Core** | Core 0: Heavy cryptographic operations (Miller-Rabin, RSA, Pollard ρ). Core 1: UI, LVGL animations, screen refresh. |
| **Flash Storage** | AES tables (S-box, MixColumns tables in $GF(2^8)$), SHA constants, language frequency dictionary, constants $K_i$ from SHA — all in LittleFS. |
| **PSRAM** | Enigma status, intermediate AES blocks, elliptic curve arrays, operational BigNums. |
| **Sessions** | The current job state (text, parameters, key) is automatically saved to LittleFS when you exit each module to resume the session. |

---

### 🎓 Educational and Professional Value

CipherForge covers the content of three different university courses within a single app:

| Course | Modules covered |
| :--- | :--- |
| Classic Cryptography and History | Modules 1, 2, 3 |
| Mathematical Cryptography (Number Theory) | Module 4 |
| Modern Cryptography (Bachelor/Master) | Modules 5, 6, 7, 8 |
| Practical Security and CTF | Modules 3, 9 |

The **"Show Steps"** mode available in all algebraic modules generates a step report exportable to LittleFS, printable or transferable by USB, which can be used directly as a worksheet in an exam or as a reference in a security audit.

---

## Long-Term Vision — The World's Best Open-Source Calculator

**NumOS aims to demonstrate that a 15 € hardware open-source calculator can surpass the features of 180 € commercial calculators.**

### Objective Comparison

| Calculator | Price | CAS | Steps | Python | Battery | Open Source |
|:------------|:------:|:---:|:-----:|:------:|:-------:|:-----------:|
| **NumOS (objective)** | **15 €** | ✅ | ✅ | ✅ | ✅ | ✅ MIT |
| Casio fx-991EX ClassWiz | 20 € | ❌ | ❌ | ❌ | AAA | ❌ |
| NumWorks | 79 € | ✅ | ❌ | ✅ | ✅ | ✅ MIT |
| TI-84 Plus CE | 119 € | ❌ | ❌ | TI-BASIC | AAA | ❌ |
| HP Prime G2 | 179 € | ✅ | ✅ | HP PPL | ✅ | ❌ |

### NumOS Unique Differential

1. **Resolution steps** — No affordable calculator shows the intermediate steps of the CAS so clearly.
2. **Fully open source** — The entire mathematical pipeline, from Tokenizer to CAS, is auditable, modifiable, and educational.
3. **Custom hardware** — The project controls everything: from the PCB to the firmware.
4. **10x lower cost** — 15 € in hardware for the capabilities of a 180 € calculator.
5. **Extensible in minutes** — Adding a new app = ~100 lines of C++17; never touches the core.
6. **Open community** — Free contribution model, not dependent on any company.

---

*NumOS — Open-source scientific calculator OS for ESP32-S3.*
*Every commit is a step towards the best scientific calculator in the world.*

*Last update: March 2026 — NeoLanguage Phase 1 (compiler frontend) complete*
