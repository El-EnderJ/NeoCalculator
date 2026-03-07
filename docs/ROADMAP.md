# NumOS — Project Roadmap

> **Last update:** April 2026
>
> Historical record and future plan for NumOS. Each phase builds upon the previous one until achieving an open-source scientific calculator that rivals the best on the market.

---

## General Progress

| Phase | Description | Status | % |
|:-----|:------------|:------:|:-:|
| **Phase 1** | Foundations — Math Engine and Drivers | ✅ Complete | 100% |
| **Phase 2** | Natural Display V.P.A.M. and 2D Navigation | ✅ Complete | 100% |
| **Phase 3** | Launcher 3.0, SerialBridge and Documentation | ✅ Complete | 100% |
| **Phase 4** | Migration to LVGL 9.x — HW Bring-Up ESP32-S3 | ✅ Complete | 100% |
| **Phase 5** | CAS-Lite Engine + EquationsApp | ✅ Complete | 100% |
| **CAS Elite** | Pro-CAS: BigNum, DAG, Derivatives, Integrals, Unified CalculusApp, SettingsApp | ✅ **Complete** | 100% |
| **Phase 6** | Complete Scientific Apps | ✅ **Complete** | 100% |
| **Phase 7** | Matrices + Complex + Bases | 🔲 Planned | 0% |
| **Phase 8** | Final Hardware + Connectivity + Scripting | 🔲 Planned | 0% |

---

## Milestone History

| Date | Milestone |
|:------|:-----|
| Nov 2025 | First correct numerical calculation in serial terminal |
| Dec 2025 | Natural Display with stacked real fractions on TFT screen |
| Jan 2026 | Launcher 3.0 with 10 registered apps and vertical scroll |
| Feb 2026 | Complete HW bring-up: 6 critical ESP32-S3 bugs resolved |
| Feb 2026 | LVGL 9.x operational — Launcher with icons on ILI9341 IPS |
| Feb 2026 | Animated splash screen + SerialBridge + LittleFS in production |
| **Feb 2026** | **Complete CAS-Lite Engine: SymPoly · SingleSolver · SystemSolver · 53 tests** |
| **Feb 2026** | **EquationsApp UI: linear, quadratic, 2×2 system with steps in PSRAM** |
| **Feb 2026** | **Pro-CAS Engine: CASInt, CASRational, SymExpr DAG, ConsTable, SymDiff (17 rules), SymIntegrate (Slagle), SymSimplify 8-pass, OmniSolver, SymPolyMulti (resultant)** |
| **Feb 2026** | **CalculusApp: symbolic derivatives with Natural Display and steps** |
| **Feb 2026** | **IntegralApp: Slagle integrals (table/u-sub/parts), +C, steps** |
| **Feb 2026** | **Active production: RAM 29.0% · Flash 18.5% · tests disabled** |
| **Mar 2026** | **CalculusApp unificada: derivadas + integrales en app única con pestañas d/dx ↔ ∫dx** |
| **Mar 2026** | **SettingsApp: raíces complejas, precisión decimal, modo angular** |
| **Mar 2026** | **Active production: RAM 28.8% · Flash 19.3% · tests disabled** || **Apr 2026** | **StatisticsApp, ProbabilityApp, RegressionApp, MatricesApp, SequencesApp, PythonApp landed via git pull — Phase 6 complete** |
| **Apr 2026** | **Boot crash fix: removed eager begin() calls — all apps lazy-init on first launch** |
| **Apr 2026** | **HOME Hard Reset fix → HOME freeze fix: deferred teardown 250 ms via _pendingTeardownMode in update()** |
| **Apr 2026** | **SettingsApp re-entry null crash fix: _statusBar.destroy() added to end()** |
| **Apr 2026** | **Active production: RAM 29.6% · Flash 20.9% · 11 apps in launcher** |
| **Mar 2026** | **BridgeDesignerApp (ID 12): Verlet physics bridge simulator with stress analysis, 3 materials, truck/car loads — PSRAM-backed 60Hz engine** |
| **Mar 2026** | **ParticleLabApp (ID 15) — Alchemy Update: 30+ materials, spark electronics, phase transitions, reaction matrix, Bresenham line tool, LittleFS save/load** |
| **Mar 2026** | **Active production: RAM 29.7% · Flash 23.2% · 16 apps in launcher** |
---

## Phase 1 — The Foundation (Complete)

> *Objective: A system that evaluates mathematical expressions and displays them on screen.*

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

> *Objective: Expressions are rendered exactly like on a modern physical calculator.*

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

> *Objective: Complete app system with launcher, navigation, and debugging tools.*

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

> *Objective: Abandon direct rendering and adopt LVGL for a professional-grade UI.*

### HW Bring-Up ESP32-S3 N16R8 CAM

| Fix | Problem | Applied Solution |
|:----|:---------|:------------------|
| ① | `Illegal instruction` crash on boot | `board_build.arduino.memory_type = qio_opi` + `flash_mode = qio` |
| ② | Crash in `TFT_eSPI::begin()` addr `0x10` | `-DUSE_FSPI_PORT` → `SPI_PORT=2` → `REG_SPI_BASE(2)=0x60024000` |
| ③ | Lines and artifacts on screen | `SPI_FREQUENCY=10000000` (10 MHz) |
| ④ | LVGL screen always black | `heap_caps_malloc(DMA+8BIT)` — never `ps_malloc` for LVGL buffers |
| ⑤ | GPIO 45 BL short circuit | `pinMode(45, INPUT)` — pin hardwired to 3.3V |
| ⑥ | Serial CDC lost on boot | `while(!Serial && t0<3000)` + `monitor_rts=0` |

### LVGL
- [x] `lv_conf.h`: `LV_MEM_CUSTOM=1` (PSRAM), `LV_TICK_CUSTOM=1`, `LV_COLOR_DEPTH=16`, Montserrat 12/14/20 fonts.
- [x] **Animated SplashScreen**: Fade-in logo + version with `lv_anim_t` ease_in_out.
- [x] **MainMenu LVGL**: 3×N grid with scroll, selection highlight, keyboard navigation.
- [x] **LittleFS**: Persistent variables with `formatOnFail`.

---

## Phase 5 — CAS-Lite Engine + EquationsApp (Complete)

> *Objective: Native symbolic algebra with detailed steps. The first open-source ESP32-S3 calculator with its own CAS in PSRAM.*

### CAS-Lite Engine — Symbolic Algebra

- [x] **PSRAMAllocator\<T\>** (`cas/PSRAMAllocator.h`)
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
| **Phase C** — SingleSolver | 33–44 | Linear (1 solution), quadratic (2 real roots, double root, Δ < 0), with steps. |
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

## CAS Elite Phase — Pro-CAS Engine + Unified Calculus App (Complete)

> *Objective: CAS-Lite → Pro-CAS evolution. Full symbolic engine with derivatives, integrals, multi-pass simplification, and non-linear equation solving. See [CAS_UPGRADE_ROADMAP.md](CAS_UPGRADE_ROADMAP.md) for the breakdown of the 6 internal phases.*

### Pro-CAS Engine — 6 Completed Phases

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

### Build Stats (Production — March 2026)

| Resource | Used | Total | % |
|:--------|------:|------:|:-:|
| RAM | 97 192 B | 327 680 B | **29.7%** |
| Flash | 1 518 269 B | 6 553 600 B | **23.2%** |

---

## Phase 6 — Complete Scientific Apps (✅ Complete)

> *Objective: NumOS becomes a complete scientific calculator for real academic use, surpassing the Casio fx-991EX in features.*

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

> *Powder-Toy-class cellular automata sandbox with 30+ materials, discrete electronics, and phase transitions.*

#### Material Library (31 materials)
- [x] **Earth & Glass**: Sand (>1500°C → Molten Glass), Molten Glass (<1000°C → Glass), Stone (inert, heavy), Glass
- [x] **Organics**: Wood (burns → Smoke), Coal (burns 10× longer than wood), Plant (2% chance to grow near Water)
- [x] **Thermal Extremes**: Lava (1500°C, cools <800°C → Stone), LN2 (Liquid Nitrogen, -196°C, evaporates >-190°C → Gas)
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

> *Objective: Achieve parity with the HP Prime G2 and NumWorks CAS.*

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

> *Objective: NumOS becomes a complete, portable, autonomous, and connected physical product.*

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

*Last update: March 2026*
