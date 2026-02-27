<div align="center">

<br>

# ◼ NumOS

### Open-Source Scientific Graphing Calculator OS

**ESP32-S3 N16R8 · ILI9341 IPS 320×240 · LVGL 9.x · CAS-Lite Engine · Natural Display V.P.A.M.**

<br>

[![PlatformIO](https://img.shields.io/badge/PlatformIO-espressif32_6.12-orange?logo=platformio&logoColor=white)](https://platformio.org/)
[![LVGL](https://img.shields.io/badge/LVGL-9.5.0-blue?logo=c&logoColor=white)](https://lvgl.io/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-N16R8-red?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Framework-Arduino-teal?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue?logo=cplusplus&logoColor=white)](https://en.cppreference.com/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Status](https://img.shields.io/badge/Status-CAS--Lite%20Production-brightgreen)](#estado-del-proyecto)
[![RAM](https://img.shields.io/badge/RAM-29%25%20%E2%80%94%2094.9%20KB-informational)](#build-stats)
[![Flash](https://img.shields.io/badge/Flash-17.1%25%20%E2%80%94%201.07%20MB-informational)](#build-stats)

<br>

> *Una alternativa open-source real a las calculadoras comerciales.*
> *Inspirada en NumWorks · TI-84 Plus · HP Prime G2 — construida desde cero en C++17.*

<br>

</div>

---

## Tabla de Contenidos

1. [¿Qué es NumOS?](#qué-es-numos)
2. [Características Destacadas](#características-destacadas)
3. [Arquitectura del Sistema](#arquitectura-del-sistema)
4. [CAS-Lite Engine](#cas-lite-engine)
5. [Hardware](#hardware)
6. [Inicio Rápido](#inicio-rápido)
7. [Manual de Usuario — EquationsApp](#manual-de-usuario--equationsapp)
8. [Estructura del Proyecto](#estructura-del-proyecto)
9. [Build Stats](#build-stats)
10. [Fixes Críticos de Hardware](#fixes-críticos-de-hardware)
11. [Estado del Proyecto](#estado-del-proyecto)
12. [Stack Tecnológico](#stack-tecnológico)
13. [Comparativa con Calculadoras Comerciales](#comparativa-con-calculadoras-comerciales)
14. [Documentación](#documentación)
15. [Contribuir](#contribuir)

---

## ¿Qué es NumOS?

**NumOS** es un sistema operativo de calculadora científica y graficadora de código abierto, construido sobre el microcontrolador **ESP32-S3 N16R8** (16 MB Flash QIO + 8 MB PSRAM OPI). Es un proyecto que aspira a convertirse en la mejor calculadora open-source del mundo, rivalizando con la Casio fx-991EX ClassWiz, la NumWorks, la TI-84 Plus CE y la HP Prime G2.

**NumOS incorpora:**

- **Motor CAS-Lite propio** — Álgebra simbólica nativa: polinomios simbólicos con aritmética exacta `Rational`, resolución cuadrática analítica con pasos detallados, sistemas 2×2 con eliminación gaussiana simbólica, todo en PSRAM con allocator personalizado STL-compatible.
- **Natural Display V.P.A.M.** — Las fórmulas se renderizan como en papel: fracciones apiladas reales, raíces con símbolo √, superíndices genuinos, navegación 2D con cursor inteligente estructural.
- **Interfaz moderna LVGL 9.x** — Transiciones fluidas, splash screen animado, launcher estilo NumWorks con iconos y grid 3×N, apps con estados múltiples y ciclo de vida limpio.
- **Motor matemático propio** — Pipeline completo: Tokenizador → Parser Shunting-Yard → Evaluador RPN + AST visual, implementado en C++17 desde cero.
- **Arquitectura de Apps modular** — Cada aplicación es un módulo intercambiable con ciclo de vida explícito (`begin/end/load/handleKey`), gestionado por `SystemApp`.

---

## Características Destacadas

| Característica | Descripción |
|:---------------|:------------|
| **CAS-Lite Engine** | Resolución analítica de ecuaciones polinomiales con pasos simbólicos detallados en PSRAM |
| **EquationsApp** | Resuelve lineales, cuadráticas y sistemas 2×2 con discriminante, fórmula cuadrática y eliminación gaussiana |
| **Natural Display** | Fracciones reales, raíces, potencias, cursores 2D — renderizado matemático como en papel |
| **Graficadora y=f(x)** | Plotter en tiempo real con zoom, pan y tabla de valores |
| **53 Tests Unitarios CAS** | Suite completa de tests para el CAS-Lite, activable/desactivable vía flag de compilación |
| **PSRAMAllocator** | CAS usa `PSRAMAllocator<T>` para aislar uso de memoria en los 8 MB PSRAM OPI |
| **Variables A-Z + Ans** | Persistencia en LittleFS — 216 bytes en `/vars.dat` |
| **SerialBridge** | Control completo de la calculadora desde PC vía Serial Monitor sin hardware físico |
| **SerialBridge Debug** | Eco inmediato de bytes, heartbeat cada 5 s, buffer circular de 8 eventos |

---

## Arquitectura del Sistema

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           ESP32-S3 N16R8                                 │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                         main.cpp                                  │  │
│  │   setup(): PSRAM → TFT → LVGL → Splash → SystemApp → Serial      │  │
│  │   loop():  lv_timer_handler() · app.update() · serial.poll()     │  │
│  └──────────────────────────┬────────────────────────────────────────┘  │
│                             │                                            │
│  ┌──────────────────────────▼────────────────────────────────────────┐  │
│  │                    SystemApp  (Dispatcher)                        │  │
│  │                                                                   │  │
│  │  ┌──────────┐  ┌──────────────────┐  ┌────────────────────────┐  │  │
│  │  │ MainMenu │  │  CalculationApp  │  │      GrapherApp        │  │  │
│  │  │  LVGL    │  │  Natural VPAM    │  │  y=f(x)·Zoom·Pan       │  │  │
│  │  │  Grid    │  │  Historial 32    │  │  Tabla de valores      │  │  │
│  │  └──────────┘  └──────────────────┘  └────────────────────────┘  │  │
│  │  ┌──────────────────────────────────────────────────────────────┐ │  │
│  │  │              EquationsApp  ★ CAS-Lite                        │ │  │
│  │  │       Lineal · Cuadrática · Sistema 2×2                      │ │  │
│  │  │       Discriminante · Fórmula cuadrática · Gauss             │ │  │
│  │  │       Pasos detallados en PSRAM · Natural Display            │ │  │
│  │  └──────────────────────────────────────────────────────────────┘ │  │
│  │  [ Sequences · Statistics · Regression · Table · Probability ]    │  │
│  │  [ Settings · Python (placeholder) ]                              │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌──────────────────────────┐  ┌─────────────────────────────────────┐  │
│  │      Math Engine         │  │       CAS-Lite Engine  ★ NUEVO     │  │
│  │                          │  │                                     │  │
│  │  Tokenizer               │  │  PSRAMAllocator<T>                  │  │
│  │  Parser (Shunting-Yard)  │  │  SymPoly (polinomios simbólicos)    │  │
│  │  Evaluator (RPN)         │  │  ASTFlattener (AST → SymPoly)       │  │
│  │  ExprNode (AST visual)   │  │  SingleSolver (lineal/cuadrática)   │  │
│  │  VariableContext A-Z     │  │  SystemSolver (2×2 gaussiana)       │  │
│  │  EquationSolver (N-R)    │  │  CASStepLogger (steps en PSRAM)     │  │
│  └──────────────────────────┘  │  SymToAST (CAS → Natural Display)   │  │
│                                └─────────────────────────────────────┘  │
│  ┌──────────────────────────┐  ┌─────────────────────────────────────┐  │
│  │     Display Layer        │  │          Input Layer                │  │
│  │                          │  │                                     │  │
│  │  DisplayDriver           │  │  KeyMatrix 6×8 (48 teclas)          │  │
│  │  (TFT_eSPI FSPI)         │  │  SerialBridge (teclado virtual PC)  │  │
│  │  LVGL flush DMA          │  │  LvglKeypad (LVGL indev)            │  │
│  │  ILI9341 @ 10 MHz        │  │  LittleFS (vars persistentes)       │  │
│  └──────────────────────────┘  └─────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
                                     │ SPI @ 10 MHz
                          ┌──────────▼──────────┐
                          │   ILI9341 IPS 3.2"  │
                          │   320×240 · 16 bpp  │
                          └─────────────────────┘
```

---

## CAS-Lite Engine

El **CAS-Lite** (Computer Algebra System Lite) es el motor de álgebra simbólica propio de NumOS. Fue diseñado con tres principios: funcionar completamente en PSRAM para no afectar la RAM interna, producir pasos legibles por el usuario, e integrarse con el Natural Display existente a través del bridge `SymToAST`.

### Pipeline CAS

```
Entrada del usuario (EquationsApp):
  "x^2 - 5x + 6 = 0"
           │
           ▼
  ┌──────────────────┐
  │   Math Engine    │  ExprNode AST visual
  │  (Parser+Tokens) │  ─────────────────►  [x², -5x, +6]  |  [0]
  └──────────────────┘
           │
           ▼
  ┌──────────────────┐
  │  ASTFlattener    │  MathAST → SymPoly
  │                  │  ─────────────────►  SymPoly{ x²:1, x:-5, 1:6 }
  └──────────────────┘
           │
           ▼
  ┌──────────────────┐
  │  SingleSolver    │  grado 2 → fórmula cuadrática analítica
  │                  │  ─────────────────►  x₁=2, x₂=3  +  pasos PSRAM
  └──────────────────┘
           │
           ▼
  ┌──────────────────┐
  │    SymToAST      │  SolveResult → ExprNode
  │                  │  ─────────────────►  Natural Display:  x₁ = 2
  └──────────────────┘
           │
           ▼
  EquationsApp renderiza resultado + pantalla de pasos detallados
```

### Componentes CAS

| Módulo | Archivo | Responsabilidad |
|:-------|:--------|:----------------|
| `PSRAMAllocator<T>` | `cas/PSRAMAllocator.h` | STL allocator que redirige `allocate`/`deallocate` a `ps_malloc`/`ps_free`. Aísla toda la memoria CAS en los 8 MB PSRAM OPI. |
| `SymPoly` | `cas/SymPoly.h/.cpp` | Polinomio simbólico en variable única. Coeficientes `Rational` (fracción exacta `p/q`). Soporta suma, resta, multiplicación, derivación, evaluación numérica. |
| `ASTFlattener` | `cas/ASTFlattener.h/.cpp` | Recorre el `ExprNode` AST visual y lo convierte en un `SymPoly`. Detecta grado, variable principal, constantes π y e. |
| `SingleSolver` | `cas/SingleSolver.h/.cpp` | Resuelve ecuaciones polinomiales: grado 1 → lineal directa; grado 2 → fórmula cuadrática analítica con discriminante; grado ≥ 3 → Newton-Raphson numérico. Genera `CASStep` detallados. |
| `SystemSolver` | `cas/SystemSolver.h/.cpp` | Resuelve sistemas 2×2 lineales por eliminación gaussiana simbólica. Detecta sistemas indeterminados e incompatibles. |
| `CASStepLogger` | `cas/CASStepLogger.h/.cpp` | Acumula pasos en `StepVec` (`std::vector<CASStep, PSRAMAllocator>`). Cada `CASStep` tiene tipo (INFO / FORMULA / RESULT / ERROR) y texto. `.clear()` libera PSRAM. |
| `SymToAST` | `cas/SymToAST.h/.cpp` | Bridge inverso: convierte `SolveResult` (raíces `Rational`) en `ExprNode` para renderizado Natural Display en la `EquationsApp`. |

### Tests CAS — 53 unitarios

| Fase | Tests | Cobertura |
|:-----|:-----:|:----------|
| **A — Fundamentos** | 1–18 | `Rational`: suma, resta, mul, div, simplificación. `SymPoly`: aritmética, derivación, normalización. |
| **B — ASTFlattener** | 19–32 | Conversión AST→SymPoly para polinomios simples, constantes, funciones trig, potencias. |
| **C — SingleSolver** | 33–44 | Lineal (1 solución), cuadrática (2 raíces reales, raíz doble, discriminante negativo), pasos. |
| **D — SystemSolver** | 45–53 | Sistema 2×2 determinado, sistema indeterminado (infinitas soluciones), sistema incompatible. |

```ini
# platformio.ini — activar tests:
build_flags    = ... -DCAS_RUN_TESTS
build_src_filter = +<*> +<../tests/CASTest.cpp>
```

---

## Hardware

| Componente | Especificación |
|:-----------|:--------------|
| **MCU** | ESP32-S3 N16R8 CAM — Dual-core Xtensa LX7 @ 240 MHz |
| **Flash** | 16 MB QIO (`default_16MB.csv`) |
| **PSRAM** | 8 MB OPI (`qio_opi` — crítico para evitar boot panic) |
| **Pantalla** | ILI9341 IPS TFT 3.2" — 320×240 px — SPI @ 10 MHz |
| **Bus SPI** | FSPI (SPI2): MOSI=13, SCLK=12, CS=10, DC=4, RST=5 |
| **Backlight** | GPIO 45 — cableado fijo a 3.3V (`pinMode(45, INPUT)`) |
| **Teclado** | Matriz 6×8 = 48 teclas — Filas: GPIO 1–6 · Cols: GPIO 38–42, 47, 48, 21 |
| **Almacenamiento** | LittleFS en partición dedicada — variables A-Z persistentes |
| **USB** | USB-CDC nativo del S3 — 115 200 baud |

### Pinout Completo

#### Pantalla ILI9341

| Señal | GPIO | Notas |
|:------|:----:|:------|
| MOSI | 13 | FSPI Data In |
| SCLK | 12 | FSPI Clock |
| CS | 10 | Chip Select (activo LOW) |
| DC | **4** | Data/Command ⚠ Conflicto ROW 3 |
| RST | **5** | Reset ⚠ Conflicto ROW 4 |
| BL | 45 | Cableado fijo a 3.3V — siempre INPUT |

#### Teclado Matricial 6×8

| Fila | GPIO | Columna | GPIO |
|:-----|:----:|:--------|:----:|
| ROW 0 | 1 | COL 0 | 38 |
| ROW 1 | 2 | COL 1 | 39 |
| ROW 2 | 3 | COL 2 | 40 |
| ROW 3 | **4** ⚠ TFT DC | COL 3 | 41 |
| ROW 4 | **5** ⚠ TFT RST | COL 4 | 42 |
| ROW 5 | 6 | COL 5 | 47 |
| — | — | COL 6 | 48 |
| — | — | COL 7 | 21 |

> ⚠️ **Conflicto crítico**: GPIO 4 (TFT_DC) y GPIO 5 (TFT_RST) coinciden con ROW3/ROW4. **Reasignar estas filas del teclado a GPIOs libres antes de soldar.**

---

## Inicio Rápido

### Requisitos

- [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) (extensión VS Code)
- Drivers USB ESP32-S3 (no necesita driver externo en Windows 11+)
- Python 3.x (PlatformIO lo instala automáticamente)

### Compilar y Flashear

```bash
git clone https://github.com/tu-usuario/numOS.git
cd numOS

# Solo compilar
pio run -e esp32s3_n16r8

# Compilar y flashear al ESP32-S3
pio run -e esp32s3_n16r8 --target upload

# Abrir monitor serial (115 200 baud)
pio device monitor
```

### Control por teclado serial (SerialBridge)

Con el Serial Monitor abierto, escribe caracteres para controlar la calculadora:

| Tecla | Acción | | Tecla | Acción |
|:-----:|:-------|-|:-----:|:-------|
| `w` | ↑ Arriba | | `z` | ENTER / Confirmar |
| `s` | ↓ Abajo | | `x` | DEL / Borrar |
| `a` | ← Izquierda | | `c` | AC / Limpiar |
| `d` | → Derecha | | `h` | MODE / Volver al menú |
| `0`–`9` | Dígitos | | `+-*/^.()` | Operadores |
| `S` | SHIFT | | `r` | √ SQRT |
| `t` | sin | | `g` | GRAPH |
| `e` | `=` (ecuación) | | `R` | SHOW STEPS (pasos) |

> **Nota**: `s` minúscula = DOWN; `S` mayúscula = SHIFT. Desactivar CapsLock antes de usar.

---

## Manual de Usuario — EquationsApp

La **EquationsApp** resuelve ecuaciones polinomiales de una variable y sistemas de 2 ecuaciones lineales con 2 incógnitas, mostrando los pasos de resolución completos usando el motor CAS-Lite.

### Acceso

1. Desde el Launcher, selecciona **Equations** con ↑↓ y pulsa ENTER.
2. Aparece la pantalla de selección de modo.

### Modo 1: Ecuación de una variable

1. Selecciona **Equation (1 var)** con ↑↓ y pulsa ENTER.
2. Se abre el editor. Escribe tu ecuación con el signo `=`:
   - `x^2 - 5x + 6 = 0`  →  x₁=2, x₂=3
   - `2x + 3 = 7`  →  x=2
   - `x^2 = -1`  →  sin solución real (Δ < 0)
3. Pulsa **ENTER** para resolver.
4. La pantalla de resultados muestra:
   - **Lineal**: una solución `x = valor`
   - **Cuadrática**: discriminante Δ y hasta 2 soluciones `x₁`, `x₂`
   - **Sin solución real**: mensaje de discriminante negativo
5. Pulsa **SHOW STEPS** (`R`) para ver los pasos detallados:
   - Ecuación normalizada
   - Valor del discriminante Δ = b² - 4ac
   - Fórmula cuadrática aplicada
   - Raíces calculadas
6. Pulsa **MODE** (`h`) para volver al menú principal.

### Modo 2: Sistema 2×2

1. Selecciona **System (2×2)** y pulsa ENTER.
2. Se muestran dos campos: **Eq 1** y **Eq 2**.
   - Escribe la primera ecuación en `x` e `y`, pulsa ENTER.
   - Escribe la segunda ecuación, pulsa ENTER.
   - Ejemplo: `2x + y = 5` / `x - y = 1`  →  x=2, y=1
3. Pulsa **ENTER** para resolver. Muestra `x = valor, y = valor`.
4. Pulsa **SHOW STEPS** para ver la eliminación gaussiana completa.
5. Pulsa **MODE** para volver.

### Teclas en EquationsApp

| Tecla | Acción |
|:------|:-------|
| ↑ ↓ ← → | Navegación en selección / cursor en editor |
| ENTER | Confirmar modo / Resolver ecuación |
| DEL | Borrar carácter |
| AC | Limpiar campo |
| SHOW STEPS | Ver pasos detallados (desde pantalla resultado) |
| MODE | Volver al menú principal |

---

## Estructura del Proyecto

```
numOS/
├── src/
│   ├── main.cpp                      # Punto de entrada Arduino (setup/loop)
│   ├── SystemApp.cpp/.h              # Orquestador central y launcher LVGL
│   ├── Config.h                      # Pinout global ESP32-S3
│   ├── lv_conf.h                     # Configuración LVGL 9.x
│   ├── HardwareTest.cpp              # Test interactivo de teclado (inline)
│   ├── apps/
│   │   ├── CalculationApp.cpp/.h     # Calculadora Natural V.P.A.M.
│   │   ├── GrapherApp.cpp/.h         # Graficadora y=f(x)
│   │   └── EquationsApp.cpp/.h       # ★ NUEVO: CAS-Lite — Solver ecuaciones
│   ├── display/
│   │   └── DisplayDriver.cpp/.h      # TFT_eSPI FSPI + LVGL init + DMA flush
│   ├── input/
│   │   ├── KeyCodes.h                # Enum KeyCode (48 teclas)
│   │   ├── KeyMatrix.cpp/.h          # Driver hardware 6×8 con debounce
│   │   ├── SerialBridge.cpp/.h       # Teclado virtual vía Serial
│   │   └── LvglKeypad.cpp/.h         # Adaptador LVGL indev keypad
│   ├── math/
│   │   ├── Tokenizer.cpp/.h          # Analizador léxico
│   │   ├── Parser.cpp/.h             # Shunting-Yard → RPN / AST visual
│   │   ├── Evaluator.cpp/.h          # Evaluador numérico RPN
│   │   ├── ExprNode.h                # Árbol de expresión (Natural Display)
│   │   ├── EquationSolver.cpp/.h     # Newton-Raphson numérico
│   │   ├── VariableContext.cpp/.h    # Variables A-Z + Ans
│   │   ├── StepLogger.cpp/.h         # Logger de pasos del parser
│   │   └── cas/                      # ★ CAS-Lite Engine completo
│   │       ├── PSRAMAllocator.h      # STL allocator para PSRAM OPI
│   │       ├── SymPoly.h/.cpp        # Polinomio simbólico (Rational)
│   │       ├── ASTFlattener.h/.cpp   # ExprNode AST → SymPoly
│   │       ├── SingleSolver.h/.cpp   # Solver lineal + cuadrático analítico
│   │       ├── SystemSolver.h/.cpp   # Sistema 2×2 eliminación gaussiana
│   │       ├── CASStepLogger.h/.cpp  # Steps en PSRAM (StepVec)
│   │       └── SymToAST.h/.cpp       # SolveResult → ExprNode visual
│   └── ui/
│       ├── MainMenu.cpp/.h           # Launcher LVGL grid 3×N
│       ├── GraphView.cpp/.h          # Widget de graficado
│       ├── Icons.h                   # Bitmaps de iconos de apps
│       └── Theme.h                   # Paleta de colores y constantes UI
├── tests/
│   ├── CASTest.h/.cpp                # ★ 53 tests unitarios CAS-Lite
│   ├── HardwareTest.cpp              # Test TFT + teclado físico
│   └── TokenizerTest_temp.cpp        # Test Tokenizer
├── docs/
│   ├── ROADMAP.md                    # Historial de fases + plan futuro
│   ├── PROJECT_BIBLE.md              # Arquitectura maestra del software
│   ├── MATH_ENGINE.md                # Motor matemático y CAS-Lite en detalle
│   ├── HARDWARE.md                   # Pinout, wiring y bring-up ESP32-S3
│   ├── CONSTRUCCION.md               # Guía de montaje físico
│   └── DIMENSIONES_DISEÑO.md         # Especificaciones 3D del chasis
├── platformio.ini                    # Configuración PlatformIO
├── wokwi.toml                        # Simulador Wokwi (opcional)
└── diagram.json                      # Diagrama de circuito Wokwi
```

---

## Build Stats

> Compilado con `pio run -e esp32s3_n16r8` en modo **producción** (tests CAS desactivados)

| Recurso | Usado | Total | Porcentaje |
|:--------|------:|------:|:----------:|
| **RAM** (data + bss) | 94 920 B | 327 680 B | **29.0 %** |
| **Flash** (program storage) | 1 118 121 B | 6 553 600 B | **17.1 %** |

**Flash ahorrado vs modo tests:** −39 444 B al desactivar `-DCAS_RUN_TESTS`.

Para activar o desactivar los tests CAS, editar `platformio.ini`:

```ini
; ---- Modo producción (defecto) ----
; -DCAS_RUN_TESTS          ← comentado

; ---- Modo tests — descomentar estas dos líneas ----
; -DCAS_RUN_TESTS
; +<../tests/CASTest.cpp>  ← en build_src_filter
```

---

## Fixes Críticos de Hardware

Problemas descubiertos y resueltos durante el bring-up. **Esenciales** para cualquier fork o montaje nuevo:

| # | Problema | Síntoma | Solución |
|:-:|:---------|:--------|:---------|
| **①** | Flash OPI Panic | Boot → `Guru Meditation: Illegal instruction` | `memory_type = qio_opi` + `flash_mode = qio` |
| **②** | SPI StoreProhibited | Crash en `TFT_eSPI::begin()` dirección `0x10` | `-DUSE_FSPI_PORT` → `SPI_PORT=2` → `REG_SPI_BASE(2)=0x60024000` |
| **③** | Ruido de pantalla | Líneas horizontales y artefactos visuales | Reducir SPI a 10 MHz: `-DSPI_FREQUENCY=10000000` |
| **④** | Pantalla negra LVGL | `lv_timer_handler()` invoca flush pero no aparece imagen | Buffers con `heap_caps_malloc(MALLOC_CAP_DMA|MALLOC_CAP_8BIT)` — **jamás** `ps_malloc` |
| **⑤** | GPIO 45 BL cortocircuito | Pantalla deja de responder al inicializar backlight | `pinMode(45, INPUT)` — el pin está cableado fijo a 3.3V |
| **⑥** | Serial CDC perdido | Output invisible en Serial Monitor al conectar | `while(!Serial && millis()-t0 < 3000)` + `monitor_rts=0` en platformio.ini |

---

## Estado del Proyecto

| Fase | Descripción | Estado |
|:-----|:------------|:------:|
| **Fase 1** | Math Engine — Tokenizer, Parser Shunting-Yard, Evaluador RPN, ExprNode, VariableContext | ✅ Completo |
| **Fase 2** | Natural Display V.P.A.M. — fracciones, raíces, potencias, cursor 2D inteligente | ✅ Completo |
| **Fase 3** | Launcher 3.0, SerialBridge, CalculationApp historial, GrapherApp zoom/pan | ✅ Completo |
| **Fase 4** | LVGL 9.x — HW bring-up ESP32-S3, DMA, splash screen animado, launcer iconos | ✅ Completo |
| **Fase 5** | CAS-Lite Engine (SymPoly, SingleSolver, SystemSolver, 53 tests) + EquationsApp UI | ✅ **Completo** |
| **Fase 6** | Statistics, Regression, Sequences, Probability, Settings App | 🔲 Planificado |
| **Fase 7** | CAS avanzado: derivadas, integrales, matrices, números complejos, base conversions | 🔲 Planificado |
| **Fase 8** | Teclado físico, PCB propia, batería recargable, carcasa 3D, WiFi OTA | 🔲 Planificado |

---

## Stack Tecnológico

| Capa | Tecnología | Versión |
|:-----|:-----------|:-------:|
| **MCU Framework** | Arduino sobre ESP-IDF 5.x | PlatformIO espressif32 6.12.0 |
| **UI / Graphics** | LVGL | 9.5.0 |
| **TFT Driver** | TFT_eSPI | 2.5.43 |
| **Filesystem** | LittleFS | ESP-IDF built-in |
| **Language** | C++17 | lambdas, `std::function`, `std::unique_ptr` |
| **CAS Memory** | PSRAMAllocator STL custom | PSRAM OPI 8 MB |
| **Build System** | PlatformIO | 6.12.0 |
| **Simulation** | Wokwi | wokwi.toml |

---

## Comparativa con Calculadoras Comerciales

| Característica | **NumOS** | NumWorks | TI-84 Plus CE | HP Prime G2 |
|:---------------|:---------:|:--------:|:-------------:|:-----------:|
| Open Source | ✅ MIT | ✅ MIT | ❌ | ❌ |
| Natural Display | ✅ | ✅ | ✅ | ✅ |
| CAS Simbólico | ✅ Lite | ✅ SymPy | ❌ | ✅ |
| Pasos de resolución | ✅ | ❌ | ❌ | ✅ |
| Graficadora color | ✅ | ✅ | ✅ | ✅ |
| Multi-función gráfica | 🔲 | ✅ | ✅ | ✅ |
| Estadística y Regresión | 🔲 | ✅ | ✅ | ✅ |
| Matrices | 🔲 | ✅ | ✅ | ✅ |
| Números complejos | 🔲 | ✅ | ✅ | ✅ |
| Scripting / Python | 🔲 | ✅ | ✅ TI-BASIC | ✅ HP PPL |
| WiFi / Conectividad | 🔲 | ✅ | ❌ | ❌ |
| Batería recargable | 🔲 | ✅ | ❌ | ✅ |
| Coste hardware estimado | **~15 €** | 79 € | 119 € | 179 € |
| Plataforma | ESP32-S3 | STM32F730 | Zilog eZ80 | ARM Cortex-A7 |

> 🏆 NumOS ya supera a la TI-84 en capacidades CAS y coste, y está en camino de igualar la NumWorks.

---

## Documentación

| Documento | Descripción |
|:----------|:------------|
| [ROADMAP.md](docs/ROADMAP.md) | Historial completo de fases, hitos y plan detallado de futuro |
| [PROJECT_BIBLE.md](docs/PROJECT_BIBLE.md) | Arquitectura maestra, módulos, convenciones de código y guías de desarrollo |
| [MATH_ENGINE.md](docs/MATH_ENGINE.md) | Motor matemático + CAS-Lite: diseño, algoritmos, pipeline y ejemplos |
| [HARDWARE.md](docs/HARDWARE.md) | Pinout ESP32-S3, wiring completo, bugs críticos y notas de bring-up |
| [CONSTRUCCION.md](docs/CONSTRUCCION.md) | Guía de montaje físico, impresión 3D y test de hardware |
| [DIMENSIONES_DISEÑO.md](docs/DIMENSIONES_DISEÑO.md) | Especificaciones dimensionales para el chasis 3D |

---

## Contribuir

NumOS es un proyecto open-source que aspira a crecer en comunidad. ¡Las contribuciones son bienvenidas!

1. Haz un **fork** del repositorio.
2. Crea una rama: `git checkout -b feature/nombre-descriptivo`
3. Sigue las [convenciones de código](docs/PROJECT_BIBLE.md#guia-de-estilo) del proyecto.
4. Verifica que el build pasa: `pio run -e esp32s3_n16r8`
5. Si añades lógica matemática, incluye tests en `tests/`.
6. Abre un **Pull Request** con descripción clara del cambio.

### Áreas donde más ayuda hace falta

| Módulo | Descripción |
|:-------|:------------|
| **Statistics App** | Media, mediana, moda, desviación estándar, listas de datos |
| **Regression App** | Regresión lineal/cuadrática con coeficiente R² |
| **Sequences App** | Sucesiones aritméticas y geométricas, término N, suma parcial |
| **Settings App** | Modo angular DEG/RAD/GRA, brillo, reset de fábrica |
| **CAS avanzado** | Derivadas simbólicas, integrales definidas, simplificación |
| **Matrices** | Editor de matrices, determinante, inversa, multiplicación |
| **Teclado físico** | Resolver conflicto GPIO 4/5, integrar KeyMatrix → LvglKeypad |
| **PCB propia** | Esquema KiCad con ESP32-S3 integrado + cargador TP4056 |

---

## Licencia

Este proyecto está bajo la licencia **MIT**. Ver [LICENSE](LICENSE) para más detalles.

---

<div align="center">

*Hecho con ❤️ y mucho C++17*

**NumOS — La mejor calculadora científica open-source para ESP32-S3**

*Última actualización: Febrero 2026*

</div>
