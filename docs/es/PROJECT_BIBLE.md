# NumOS — Project Bible

&gt; **La documentación maestra del proyecto. Si algo no está aquí, no existe.**
&gt;
&gt; **Plataforma**: ESP32-S3 N16R8 CAM · **UI**: LVGL 9.5
&gt; **Lenguaje**: C++17 · **Pro-CAS Engine**: Producción activa
&gt; **Última actualización**: Marzo 2026

---

## Tabla de Contenidos

1. [Visión General](#1-visión-general)
2. [Arquitectura de Software](#2-arquitectura-de-software)
3. [Pro-CAS Engine — Arquitectura Interna](#3-pro-cas-engine--arquitectura-interna)
4. [Módulos — Inventario Completo](#4-módulos--inventario-completo)
5. [Configuración del Build](#5-configuración-del-build)
6. [Estado Actual (Febrero 2026)](#6-estado-actual-febrero-2026)
7. [Guía de Estilo de Código](#7-guía-de-estilo-de-código)
8. [Cómo Añadir una Nueva App](#8-cómo-añadir-una-nueva-app)
9. [Cómo Añadir una Función Matemática](#9-cómo-añadir-una-función-matemática)
10. [Cómo Ampliar el Pro-CAS](#10-cómo-ampliar-el-pro-cas)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Visión General

**NumOS** es un sistema operativo de calculadora científica y graficadora de código abierto, construido sobre el microcontrolador **ESP32-S3 N16R8 CAM** y la librería gráfica **LVGL 9.x**.

**Objetivo Principal**: Crear la mejor alternativa open-source a calculadoras comerciales como la Casio fx-991EX ClassWiz, la NumWorks, la TI-84 Plus CE y la HP Prime G2. Con pantalla de color 320×240, Natural Display matemático, CAS-Lite propio y arquitectura modular extensible.

### Principios de Diseño

| Principio | Descripción |
|:----------|:------------|
| **Modularidad** | Cada app es un módulo intercambiable con interfaz explícita. Añadir una app no toca el core. |
| **Eficiencia** | C++17 sin heap excesivo. Objetos LVGL en PSRAM, buffers DMA en heap interno. CAS usa `PSRAMAllocator`. |
| **Extensibilidad** | Math Engine: 3 archivos para nueva función. CAS: 1 archivo para nuevo solver. Launcher: 2 líneas. |
| **Fidelidad Visual** | Fracciones reales, raíces √, superíndices genuinos — como en papel. |
| **Transparencia** | El CAS muestra los pasos al usuario. No es una caja negra. Educativo por diseño. |

---

## 2. Arquitectura de Software

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
│  Modo { MENU, APP }                                                    │
│  injectKey(key) → if MODE → returnToMenu()                             │
│                 → else    → activeApp->handleKey(key)                  │
│                                                                        │
│  ┌──────────────┐  ┌──────────────────┐  ┌────────────────────────┐   │
│  │  MainMenu    │  │  CalculationApp  │  │      GrapherApp        │   │
│  │  LVGL Grid   │  │  Natural VPAM    │  │  y=f(x) Zoom/Pan       │   │
│  │  3×N scroll  │  │  Historial 32    │  │  Tabla de valores      │   │
│  └──────────────┘  └──────────────────┘  └────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │              EquationsApp (Pro-CAS UI)                          │  │
│  │  States: SELECT → EQ_INPUT → RESULT → STEPS                     │  │
│  │  Pipeline: MathAST → ASTFlattener → Solver → SymToAST           │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  CalculusApp (Pro-CAS)  — Unificada: d/dx + ∫dx             │  │
│  │  Cambio de modo por pestañas · Derivadas + Integrales         │  │
│  │  17 reglas · Slagle · Steps · +C · Natural Display          │  │
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
│ Evaluator   │   │ SymDiff (17 reglas)    │   │ lvglFlushCb DMA       │
│ ExprNode    │   │ SymIntegrate (Slagle)  │   │ KeyMatrix 5×10         │
│ MathAST     │   │ SymSimplify (8-pass)   │   │ SerialBridge          │
│ VarContext  │   │ OmniSolver / Solvers   │   │ LvglKeypad (indev)    │
│ EqSolver    │   │ CASStepLogger          │   │ LittleFS              │
└─────────────┘   │ SymToAST / SymExprToAST│   └───────────────────────┘
                  └────────────────────────┘
```

### 2.1 main.cpp — Secuencia de Boot

`setup()` en orden estricto:
1. `Serial.begin(115200)` + espera CDC hasta 3 s
2. `g_display.begin()` — init TFT FSPI @10 MHz, reset, fillScreen negro
3. `lv_init()` + `lv_tick_set_cb(millis)`
4. `heap_caps_malloc(MALLOC_CAP_DMA|MALLOC_CAP_8BIT, 6400)` × 2 — **DMA, NO ps_malloc**
5. `g_display.initLvgl(buf1, buf2, 6400)` — registrar flush callback
6. Splash screen animado → pump `lv_timer_handler()` hasta completar + 800 ms
7. `g_app.begin()` — init SystemApp, LittleFS, launcher, apps
8. `g_serial.begin()` — activar SerialBridge

`loop()` en ciclo continuo:
- `lv_timer_handler()` — eventos LVGL, renders, animaciones
- `g_app.update()` — tick de la app activa
- `g_serial.pollEvent()` → `g_app.injectKey()`

### 2.2 SystemApp — Orquestador Central

| Responsabilidad | Descripción |
|:----------------|:------------|
| **Estado global** | Modo angular (DEG/RAD/GRA), app activa, índice selección |
| **Launcher (MENU)** | Grid LVGL 3×N, scroll, highlight selección |
| **App dispatching** | Enruta `KeyEvent`s a la app activa o al launcher |
| **MODE interceptado** | `KeyCode::MODE` siempre → `returnToMenu()` antes de llegar a la app |
| **injectKey()** | API pública — SerialBridge y KeyMatrix usan esta entrada |
| **LittleFS** | Carga `/vars.dat` al arranque |
| **LvglKeypad** | Inicializa indev LVGL tipo `KEYPAD` |

```cpp
enum class Mode { MENU, APP };

// Ciclo de vida de una app:
// 1. Usuario selecciona icono → ENTER
// 2. SystemApp: app->begin(), g_lvglActive=true/false, modo=APP
// 3. update() → app->update()
// 4. injectKey(key):
//    - si key==MODE → returnToMenu(): app->end()+app->begin() reset + modo=MENU
//    - else → app->handleKey(key)
```

### 2.3 Interfaz de Apps

```cpp
void begin();                  // Crear pantalla LVGL, inicializar estado
void end();                    // Destruir pantalla, liberar recursos (inc. PSRAM steps)
void load();                   // App visible (post-transición)
void handleKey(KeyCode key);   // Procesar input del usuario
void update();                 // Tick periódico (~60 fps)
```

**Apps LVGL-nativas**: `CalculationApp`, `GrapherApp`, `EquationsApp`, `CalculusApp`, `SettingsApp`, `MainMenu` → `g_lvglActive = true`.  
**Apps TFT-directas**: pendientes de implementación completa → `g_lvglActive = false`.

### 2.4 EquationsApp — Flujo Interno

```
begin() → setState(SELECT) → showModeSelection()

handleKey(ENTER) en SELECT:
  modo 0 → setState(EQ_INPUT) → showInputScreen(1 var)
  modo 1 → setState(EQ_INPUT) → showInputScreen(2 vars)

handleKey(ENTER) en EQ_INPUT → solveEquations():
  splitAtEquals(expr) → lhs, rhs
  parseToMathAST(lhs), parseToMathAST(rhs)
  ASTFlattener::flatten(lhs, rhs) → SymPoly
  SingleSolver::solve(poly)  ó  SystemSolver::solve(eq1, eq2)
  result → setState(RESULT) → buildResultDisplay()

handleKey(SHOW_STEPS) en RESULT:
  setState(STEPS) → buildStepsDisplay()

handleKey(MODE) [interceptado por SystemApp]:
  returnToMenu() → end() + begin() [reset completo]
```

---

## 3. Pro-CAS Engine — Arquitectura Interna

El Pro-CAS es un motor de álgebra simbólica completo, evolución del CAS-Lite original. Implementa un DAG inmutable con hash-consing, aritmética bignum overflow-safe (`CASInt`/`CASRational`), simplificación multi-pass con punto fijo (8 iteraciones), derivación simbólica (17 reglas), integración simbólica (Slagle heurístico), y resolución de sistemas no lineales via resultante de Sylvester. Todo vive en PSRAM.

### 3.1 Estructura de Módulos

```
src/math/cas/
├── CASInt.h              ← BigInt híbrido: int64 fast-path + mbedtls_mpi
├── CASRational.h/.cpp    ← Fracción exacta overflow-safe (auto-GCD)
├── ConsTable.h           ← Hash-consing PSRAM: dedup de nodos
├── PSRAMAllocator.h      ← Allocator STL → ps_malloc/ps_free
├── SymExpr.h/.cpp        ← DAG inmutable (hash + weight)
├── SymExprArena.h        ← Bump allocator PSRAM + ConsTable
├── SymDiff.h/.cpp        ← Derivación: 17 reglas (cadena, prod, trig, exp, log)
├── SymIntegrate.h/.cpp   ← Integración Slagle: tabla, linealidad, u-sub, partes
├── SymSimplify.h/.cpp    ← Simplificador fixed-point (8 passes, trig/log/exp)
├── SymPoly.h/.cpp        ← Polinomio simbólico univariable (CASRational)
├── SymPolyMulti.h/.cpp   ← Polinomio multivariable + resultante Sylvester
├── ASTFlattener.h/.cpp   ← MathAST (VPAM) → SymExpr DAG
├── SingleSolver.h/.cpp   ← Ecuación 1 var: lineal / cuadrática / N-R
├── SystemSolver.h/.cpp   ← Sistema 2×2: gaussiana + NL (resultante)
├── OmniSolver.h/.cpp     ← Aislamiento analítico de variable
├── HybridNewton.h/.cpp   ← Newton-Raphson con Jacobiana simbólica
├── CASStepLogger.h/.cpp  ← StepVec en PSRAM (INFO/FORMULA/RESULT/ERROR)
├── SymToAST.h/.cpp       ← SolveResult → MathAST Natural Display
└── SymExprToAST.h/.cpp   ← SymExpr → MathAST (+C, ∫)
```

### 3.2 Tipos Fundamentales

#### `Rational`
```cpp
struct Rational {
    int64_t num, den;  // den siempre > 0, simplificado por GCD
    Rational(int64_t n=0, int64_t d=1);  // Normaliza automáticamente
    double toDouble() const;
    bool isInteger() const;
    // Operadores: +, -, *, /, ==, !=
};
```

#### `SymPoly`
```cpp
using CoeffMap = std::map<int, Rational, std::less<int>,
                          PSRAMAllocator<std::pair<const int, Rational>>>;
struct SymPoly {
    CoeffMap coeffs;  // {grado: coeficiente Rational}
    char     var;     // Variable principal ('x' por defecto)

    int      degree() const;
    Rational coeff(int deg) const;
    SymPoly  derivative() const;
    double   evaluate(double x) const;
};
```

#### Resultados
```cpp
struct SolveResult {
    enum Status { OK_ONE, OK_TWO, COMPLEX, INFINITE, NONE, ERROR };
    Status   status;
    Rational root1, root2;
    CASStepLogger steps;  // Pasos en PSRAM
};
struct SystemResult {
    enum Status { OK, INFINITE, INCONSISTENT, ERROR };
    Status   status;
    Rational x, y;
    CASStepLogger steps;
};
```

### 3.3 SingleSolver — Lógica de Resolución

```
Entrada: SymPoly (lhs - rhs = 0)

grado 0: constante → ERROR o INFINITE
grado 1: ax + b = 0  →  x = -b/a
         Pasos: [INFO "Ecuación lineal", FORMULA "x = -b/a", RESULT "x=valor"]

grado 2: ax² + bx + c = 0
         Δ = b² - 4ac
         Δ < 0 → COMPLEX (sin solución real)
         Δ = 0 → raíz doble: x = -b/(2a)
         Δ > 0 → x₁ = (-b+√Δ)/(2a),  x₂ = (-b-√Δ)/(2a)
         Pasos detallan: normalización, cálculo Δ, fórmula aplicada, raíces

grado ≥ 3: Newton-Raphson numérico
           semillas: 0, 1, -1, 2, -2
           converge: |f(x)| < 1e-10, máx 100 iter
```

### 3.4 SystemSolver — Eliminación Gaussiana 2×2

```
eq1: a₁x + b₁y = c₁
eq2: a₂x + b₂y = c₂

eq1' = eq1 × a₂  ;  eq2' = eq2 × a₁
eq3 = eq1' - eq2'  →  (b₁a₂ - b₂a₁)y = (c₁a₂ - c₂a₁)

denominador D = b₁a₂ - b₂a₁
  D = 0 y num≠0 → INCONSISTENT
  D = 0 y num=0 → INFINITE
  D ≠ 0 → y = num/D  ;  sustituir en eq1 para obtener x
```

### 3.5 Gestión de Memoria PSRAM

`CASStepLogger` usa `StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>`.  
`EquationsApp::end()` **debe** llamar:
```cpp
_singleResult.steps.clear();   // Libera StepVec PSRAM
_systemResult.steps.clear();   // Libera StepVec PSRAM
_resultHint = nullptr;         // Nula puntero LVGL (widget ya destruido)
```

Sin esto, la PSRAM acumula allocations entre sesiones de la app.

---

## 4. Módulos — Inventario Completo

### Math Engine

| Módulo | Archivo | Responsabilidad |
|:-------|:--------|:----------------|
| `Tokenizer` | `math/Tokenizer.cpp/.h` | String → lista `Token` (24 tipos) |
| `Parser` | `math/Parser.cpp/.h` | Tokens → RPN Shunting-Yard + AST ExprNode |
| `Evaluator` | `math/Evaluator.cpp/.h` | RPN → `double`. Modos DEG/RAD/GRA. |
| `ExprNode` | `math/ExprNode.h` | Árbol visual: TEXT/FRACTION/ROOT/POWER |
| `VariableContext` | `math/VariableContext.cpp/.h` | Variables A-Z + Ans. LittleFS `/vars.dat` |
| `EquationSolver` | `math/EquationSolver.cpp/.h` | Newton-Raphson numérico general |
| `StepLogger` | `math/StepLogger.cpp/.h` | Log de pasos del parser (debug) |

### CAS-Lite Engine

| Módulo | Archivo | Responsabilidad |
|:-------|:--------|:----------------|
| `PSRAMAllocator<T>` | `math/cas/PSRAMAllocator.h` | STL allocator → `ps_malloc`/`ps_free` |
| `Rational`, `SymPoly` | `math/cas/SymPoly.h/.cpp` | Fracción exacta + polinomio simbólico |
| `ASTFlattener` | `math/cas/ASTFlattener.h/.cpp` | `MathAST` → `SymExpr` DAG |
| `SingleSolver` | `math/cas/SingleSolver.h/.cpp` | Ecuación 1 var (L/Q/N-R) con pasos |
| `SystemSolver` | `math/cas/SystemSolver.h/.cpp` | Sistema 2×2 gaussiana + NL (resultante) |
| `CASStepLogger` | `math/cas/CASStepLogger.h/.cpp` | `StepVec` PSRAM — 4 tipos de paso |
| `SymToAST` | `math/cas/SymToAST.h/.cpp` | `SolveResult` → `MathAST` visual |

### Pro-CAS Engine (extensiones avanzadas)

| Módulo | Archivo | Responsabilidad |
|:-------|:--------|:----------------|
| `CASInt` | `math/cas/CASInt.h` | BigInt híbrido: `int64_t` fast-path + `mbedtls_mpi` |
| `CASRational` | `math/cas/CASRational.h/.cpp` | Fracción exacta overflow-safe (auto-GCD) |
| `ConsTable` | `math/cas/ConsTable.h` | Hash-consing PSRAM: dedup de nodos |
| `SymExpr` | `math/cas/SymExpr.h/.cpp` | DAG inmutable con hash (`_hash`) y peso (`_weight`) |
| `SymExprArena` | `math/cas/SymExprArena.h` | Bump allocator PSRAM + ConsTable integrado |
| `SymDiff` | `math/cas/SymDiff.h/.cpp` | Derivación simbólica: 17 reglas (cadena, producto, trig, exp, log) |
| `SymIntegrate` | `math/cas/SymIntegrate.h/.cpp` | Integración Slagle: tabla, linealidad, u-sub, partes LIATE |
| `SymSimplify` | `math/cas/SymSimplify.h/.cpp` | Simplificador multi-pass (8 iter, fixed-point, trig/log/exp) |
| `SymPolyMulti` | `math/cas/SymPolyMulti.h/.cpp` | Polinomio multivariable + resultante de Sylvester |
| `OmniSolver` | `math/cas/OmniSolver.h/.cpp` | Aislamiento analítico de variable |
| `HybridNewton` | `math/cas/HybridNewton.h/.cpp` | Newton-Raphson con Jacobiana simbólica |
| `SymExprToAST` | `math/cas/SymExprToAST.h/.cpp` | `SymExpr` → MathAST. `convertIntegral()` (+C) |

### Apps

| App | Archivo | Estado | Descripción |
|:----|:--------|:------:|:------------|
| `CalculationApp` | `apps/CalculationApp.cpp/.h` | ✅ | Natural VPAM, historial 32, variables |
| `GrapherApp` | `apps/GrapherApp.cpp/.h` | ✅ | y=f(x), zoom, pan, tabla (UI base) |
| `EquationsApp` | `apps/EquationsApp.cpp/.h` | ✅ | Pro-CAS: 1-var, 2×2, pasos PSRAM |
| `CalculusApp` | `apps/CalculusApp.cpp/.h` | ✅ | Pro-CAS: derivadas (d/dx) + integrales (∫dx) unificadas, pestañas, pasos |
| `SettingsApp` | `apps/SettingsApp.cpp/.h` | ✅ | Configuración: raíces complejas, precisión, modo angular |
| `StatisticsApp` | `apps/StatisticsApp.cpp/.h` | ✅ | Listas de datos, media/mediana/σ/s, histograma |
| `ProbabilityApp` | `apps/ProbabilityApp.cpp/.h` | ✅ | nCr, nPr, n!, binomial, normal, Poisson |
| `RegressionApp` | `apps/RegressionApp.cpp/.h` | ✅ | Regresión lineal/cuadrática/log/exp, R², dispersión |
| `MatricesApp` | `apps/MatricesApp.cpp/.h` | ✅ | Editor m×n, +/−/×/transp., det, inversa, Ax=b |
| `SequencesApp` | `apps/SequencesApp.cpp/.h` | ✅ | Sucesiones aritméticas/geométricas, término N, sumas parciales |
| `PythonApp` | `apps/PythonApp.cpp/.h` | ⚠️ | Placeholder UI (scripting pendiente) |
| `PeriodicTableApp` | `apps/PeriodicTableApp.cpp/.h` | ✅ | Tabla periódica, masa molar, balanceo de ecuaciones |
| `BridgeDesignerApp` | `apps/BridgeDesignerApp.cpp/.h` | ✅ | Simulador de puentes: física Verlet, análisis de estrés |
| `CircuitCoreApp` | `apps/CircuitCoreApp.cpp/.h` | ✅ | Simulador de circuitos SPICE: MNA, 30 componentes |
| `Fluid2DApp` | `apps/Fluid2DApp.cpp/.h` | ✅ | Dinámica de fluidos 2D: Navier-Stokes, vorticidad |
| `ParticleLabApp` | `apps/ParticleLabApp.cpp/.h` | ✅ | Sandbox Powder-Toy: 30+ materiales, electrónica spark, transiciones de fase, guardado/carga |

### UI

| Módulo | Archivo | Descripción |
|:-------|:--------|:------------|
| `MainMenu` | `ui/MainMenu.cpp/.h` | Launcher LVGL grid 3×N scroll |
| `MathRenderer` | `ui/MathRenderer.cpp/.h` | Renderizador 2D MathCanvas |
| `StatusBar` | `ui/StatusBar.cpp/.h` | Barra de estado LVGL |
| `GraphView` | `ui/GraphView.cpp/.h` | Widget graficado GrapherApp |
| `Icons.h` | `ui/Icons.h` | Bitmaps 48×48 de iconos |
| `Theme.h` | `ui/Theme.h` | Paleta colores, fuentes, constantes |

### HAL / Drivers

| Módulo | Archivo | Descripción |
|:-------|:--------|:------------|
| `DisplayDriver` | `display/DisplayDriver.cpp/.h` | TFT_eSPI FSPI + LVGL + flush DMA |
| `KeyMatrix` | `input/KeyMatrix.cpp/.h` | Escaneo 6×8, debounce, autorepeat |
| `SerialBridge` | `input/SerialBridge.cpp/.h` | Inyección teclas desde Serial PC |
| `LvglKeypad` | `input/LvglKeypad.cpp/.h` | Adaptador LVGL indev KEYPAD |
| `KeyCodes.h` | `input/KeyCodes.h` | Enum `KeyCode` — 48 teclas |

### Tests

| Archivo | Estado | Descripción |
|:--------|:------:|:------------|
| `tests/CASTest.h/.cpp` | ✅ | 53 tests CAS (Fases A-D) |
| `tests/HardwareTest.cpp` | ✅ | Test interactivo TFT + teclado físico |
| `tests/TokenizerTest_temp.cpp` | ✅ | Tests del Tokenizer |

---

## 5. Configuración del Build

**Entorno principal**: `esp32s3_n16r8` en `platformio.ini`

### Flags críticos

```ini
board_build.arduino.memory_type = qio_opi   ; Flash QIO + PSRAM OPI — crítico
board_build.flash_mode          = qio
board_upload.flash_size         = 16MB
board_build.partitions          = default_16MB.csv

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DUSE_FSPI_PORT                           ; SPI_PORT=2 — sin esto: crash 0x10
    -DILI9341_DRIVER=1
    -DSPI_FREQUENCY=10000000                  ; 10 MHz — sin esto: artefactos
    -DTFT_MOSI=13 -DTFT_SCLK=12
    -DTFT_CS=10   -DTFT_DC=4  -DTFT_RST=5
    -DTFT_BL=45
    -std=gnu++17

; Tests CAS (descomentar ambas para activar):
; -DCAS_RUN_TESTS
build_src_filter = +<*>
; +<../tests/CASTest.cpp>

monitor_speed   = 115200
monitor_filters = esp32_exception_decoder
monitor_rts     = 0
monitor_dtr     = 0
```

### Por qué `-DUSE_FSPI_PORT` es obligatorio

`REG_SPI_BASE(0) = 0` en ESP32-S3. Sin el flag, `TFT_eSPI::begin_tft_write()` escribe en dirección `0x10` → **Guru Meditation: StoreProhibited**.  
Con el flag: `SPI_PORT=2` → `REG_SPI_BASE(2) = 0x60024000` ✓

### Por qué los buffers LVGL NO pueden estar en PSRAM

El DMA del SPI del ESP32-S3 solo accede a RAM interna. Los buffers en PSRAM producen transferencias de basura sin error explícito — la pantalla permanece negra silenciosamente.

```cpp
// CORRECTO:
buf1 = heap_caps_malloc(6400, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
// INCORRECTO (pantalla negra):
buf1 = ps_malloc(6400);
```

---

## 6. Estado Actual (Febrero 2026)

### En producción

- ✅ Boot estable ESP32-S3 N16R8 CAM — sin panics
- ✅ ILI9341 IPS @10 MHz — sin artefactos
- ✅ LVGL 9.5.0 doble buffer DMA — launcher visible
- ✅ SplashScreen animado
- ✅ SerialBridge — eco teclas, heartbeat 5 s
- ✅ LittleFS — variables persistentes
- ✅ CalculationApp — Natural VPAM, historial 32, A-Z+Ans
- ✅ GrapherApp — y=f(x) zoom/pan
- ✅ **Pro-CAS Engine** — SymExpr DAG, CASInt, CASRational, SymDiff, SymIntegrate, SymSimplify, OmniSolver, SymPolyMulti
- ✅ **EquationsApp** — 4 estados, modos 1-var y 2×2 (lineal + NL), pasos PSRAM
- ✅ **CalculusApp** — Unificada: derivadas simbólicas (17 reglas) + integrales (Slagle), pestañas d/dx ↔ ∫dx, simplificación, pasos
- ✅ **SettingsApp** — Toggle raíces complejas, selector precisión decimal, modo angular
- ✅ **85+ tests CAS** — todos passing (desactivados en producción)

### Build Stats

| Recurso | Usado | Total | % |
|:--------|------:|------:|:-:|
| RAM | 94 512 B | 327 680 B | **28.8%** |
| Flash | 1 263 109 B | 6 553 600 B | **19.3%** |

### Pendiente

- ⏳ 5 apps con lógica: Statistics, Regression, Sequences, Table, Probability
- ⏳ CAS avanzado: integrales definidas, matrices, números complejos
- ⏳ PCB propia, batería, carcasa 3D, WiFi OTA

---

## 7. Guía de Estilo de Código

| Elemento | Convención | Ejemplo |
|:---------|:-----------|:--------|
| Clases | `PascalCase` | `CalculationApp`, `SymPoly` |
| Métodos y funciones | `camelCase` | `handleKey()`, `solveEquations()` |
| Variables de miembro | `_prefijo` | `_screen`, `_singleResult`, `_resultHint` |
| Parámetros y locales | `camelCase` | `expr`, `poly`, `key` |
| Constantes y macros | `UPPER_SNAKE_CASE` | `KEY_ROWS`, `BUF_SIZE` |
| Archivos | `PascalCase.cpp/.h` | `EquationsApp.cpp`, `SingleSolver.h` |
| Lambdas LVGL | Inline `[](lv_event_t* e){ ... }` | Ver `MainMenu.cpp` |
| Includes | Relativos a `src/` | `"math/cas/SymPoly.h"` |

**Reglas generales**:
- Sin `using namespace std;` en cabeceras `.h`.
- Preferir `constexpr`/`const` sobre `#define` para constantes.
- Callbacks LVGL: siempre `static` — objeto vía `lv_event_get_user_data(e)`.
- `lv_obj_t*` miembro: nullar en `end()` para evitar punteros colgantes.
- Liberar recursos en `end()`, no en destructores (objetos long-lived).

---

## 8. Cómo Añadir una Nueva App

```cpp
// 1. Crear src/apps/MiApp.h y MiApp.cpp
class MiApp {
public:
    void begin();                 // Crear pantalla LVGL, init estado
    void end();                   // Destruir pantalla, liberar recursos
    void load();                  // App visible
    void handleKey(KeyCode key);  // Input
    void update();                // Tick ~60fps
};

// 2. En SystemApp.h:
#include "apps/MiApp.h"
MiApp _miApp;

// 3. En SystemApp::begin() — registrar en el launcher:
_apps[N] = { "Mi App", Icons::miIconData, &_miApp, /*lvgl=*/true };

// 4. En ui/Icons.h — añadir bitmap 48×48:
constexpr uint16_t miIconData[] = { /* RGB565 pixels */ };
```

---

## 9. Cómo Añadir una Función Matemática

Para añadir `log₂(x)`:

1. **`Tokenizer.cpp/.h`**: Añadir `LOG2` a `enum class TokenType`. Reconocer `"log2"` en el lexer.

2. **`Parser.cpp`**: Añadir `LOG2` al mapa de funciones con precedencia 5 (unaria).

3. **`Evaluator.cpp`**:
```cpp
case TokenType::LOG2:
    a = stack.top(); stack.pop();
    stack.push(std::log2(a));
    break;
```

4. **`ExprNode.h`** (opcional): Si necesita renderizado especial (subíndice 2 bajo `log`).

5. **CAS-Lite** (opcional): En `ASTFlattener::visitText()`, convertir el nodo a valor numérico equivalente para el análisis polinomial.

---

## 10. Cómo Ampliar el Pro-CAS

### Nuevo solver (ej. Cardano para grado 3)

```cpp
// En SingleSolver.cpp, rama degree==3:
// 1. Reducir a forma deprimida: t³ + pt + q = 0
// 2. Calcular discriminante Δ = -(4p³ + 27q²)
// 3. Δ>0: 3 raíces reales (método trigonométrico)
//    Δ&lt;0: 1 real + 2 complejas
steps.add(StepType::INFO, "Método de Cardano (grado 3)");
```

### Derivadas simbólicas ✅ IMPLEMENTADO

```cpp
// SymDiff.h/.cpp — 17 reglas ya implementadas:
// d/dx(sin(u)) = cos(u) * u'
// d/dx(e^u)   = e^u * u'
// d/dx(ln(u)) = u'/u
// + cadena, producto, cociente, potencia, constante, etc.
// Acceso: SymDiff::differentiate(arena, expr, "x")
```

### Integrales simbólicas ✅ IMPLEMENTADO

```cpp
// SymIntegrate.h/.cpp — Slagle heurístico:
// Estrategias: tabla directa → linealidad → u-sustitución → partes (LIATE)
// Acceso: SymIntegrate::integrate(arena, expr, "x")
// Retorna nullptr si no puede resolver (se muestra como ∫ sin evaluar)
```

### Sistema 3×3

```cpp
// Crear SystemSolver3x3 en cas/SystemSolver.h
// Usar eliminación gaussiana extendida con 3 ecuaciones
// Mismo patrón que SystemSolver (2×2) pero con matriz 3×4 aumentada
```

---

## 11. Troubleshooting

| Síntoma | Causa Probable | Solución |
|:--------|:--------------|:---------|
| `Guru Meditation: Illegal Instruction` en boot | PSRAM OPI no configurado | `memory_type = qio_opi` |
| `Guru Meditation: StoreProhibited` en `TFT_eSPI::begin` | `SPI_PORT=0` → NULL ptr | `-DUSE_FSPI_PORT` en build_flags |
| Pantalla con líneas / artefactos | SPI demasiado rápido | `SPI_FREQUENCY=10000000` |
| Pantalla negra con LVGL activo | Buffers en PSRAM | `heap_caps_malloc(MALLOC_CAP_DMA|MALLOC_CAP_8BIT)` |
| Serial Monitor vacío / board se resetea | DTR/RTS reinicia al conectar | `monitor_rts=0`, `monitor_dtr=0` |
| Output serie perdido en boot | USB CDC no enumerado | `while(!Serial && millis()-t0&lt;3000)` |
| LittleFS error al arrancar | Sin partición o `vars.dat` no existe | `LittleFS.begin(true)` — `formatOnFail=true` |
| Teclado físico no responde | GPIO 4/5 compartidos TFT/teclado | Reasignar ROW3/ROW4 a GPIOs libres |
| EquationsApp resultado incorrecto | ASTFlattener no reconoció nodo | Revisar `ASTFlattener::visit*()` |
| PSRAM crece entre sesiones | `end()` sin `.clear()` en StepVec | Verificar `_singleResult.steps.clear()` en `end()` |
| `ConstKind::Euler` no compila | Enum usa `ConstKind::E` | Usar `ConstKind::E` en `SymToAST.cpp` |

---

*NumOS — Open-source scientific calculator OS for ESP32-S3 N16R8.*
*Documentación maestra — última actualización: Marzo 2026*
