# NumOS — Project Roadmap

> **Última actualización:** Marzo 2026
>
> Registro histórico y plan de futuro de NumOS. Cada fase construye sobre la anterior hasta alcanzar una calculadora científica open-source que rivalice con las mejores del mercado.

---

## Progreso General

| Fase | Descripción | Estado | % |
|:-----|:------------|:------:|:-:|
| **Fase 1** | Cimientos — Math Engine y Drivers | ✅ Completo | 100% |
| **Fase 2** | Natural Display V.P.A.M. y Navegación 2D | ✅ Completo | 100% |
| **Fase 3** | Launcher 3.0, SerialBridge y Documentación | ✅ Completo | 100% |
| **Fase 4** | Migración a LVGL 9.x — HW Bring-Up ESP32-S3 | ✅ Completo | 100% |
| **Fase 5** | CAS-Lite Engine + EquationsApp | ✅ Completo | 100% |
| **CAS Elite** | Pro-CAS: BigNum, DAG, Derivadas, Integrales, CalculusApp unificada, SettingsApp | ✅ **Completo** | 100% |
| **Fase 6** | Apps Científicas Completas | ✅ **Completo** | 100% |
| **Simulaciones** | ParticleLab (30+ materiales), CircuitCore (SPICE), Fluid2D | ✅ **Completo** | 100% |
| **Fase 7** | Matrices + Complejos + Bases | 🔲 Planificado | 0% |
| **Fase 8** | Hardware Final + Conectividad + Scripting | 🔲 Planificado | 0% |

---

## Historial de Hitos

| Fecha | Hito |
|:------|:-----|
| Nov 2025 | Primer cálculo numérico correcto en terminal serie |
| Dic 2025 | Natural Display con fracciones reales apiladas en pantalla TFT |
| Ene 2026 | Launcher 3.0 con 10 apps registradas y scroll vertical |
| Feb 2026 | HW bring-up completo: 6 bugs críticos ESP32-S3 resueltos |
| Feb 2026 | LVGL 9.x operativo — Launcher con iconos en ILI9341 IPS |
| Feb 2026 | Splash screen animado + SerialBridge + LittleFS en producción |
| **Feb 2026** | **CAS-Lite Engine completo: SymPoly · SingleSolver · SystemSolver · 53 tests** |
| **Feb 2026** | **EquationsApp UI: lineal, cuadrática, sistema 2×2 con pasos en PSRAM** |
| **Feb 2026** | **Pro-CAS Engine: CASInt, CASRational, SymExpr DAG, ConsTable, SymDiff (17 reglas), SymIntegrate (Slagle), SymSimplify 8-pass, OmniSolver, SymPolyMulti (resultante)** |
| **Feb 2026** | **CalculusApp: derivadas simbólicas con Natural Display y pasos** |
| **Feb 2026** | **IntegralApp: integrales Slagle (tabla/u-sub/partes), +C, pasos** |
| **Feb 2026** | **Producción activa: RAM 29.0% · Flash 18.5% · tests desactivados** |
| **Mar 2026** | **CalculusApp unificada: derivadas + integrales en app única con pestañas d/dx ↔ ∫dx** |
| **Mar 2026** | **SettingsApp: raíces complejas, precisión decimal, modo angular** |
| **Mar 2026** | **Producción activa: RAM 28.8% · Flash 19.3% · tests desactivados** |
| **Mar 2026** | **ParticleLabApp (ID 15) — Alchemy Update: 30+ materiales, electrónica spark, transiciones de fase, matriz de reacciones, herramienta de línea Bresenham, guardado/carga LittleFS** |
| **Mar 2026** | **Producción activa: RAM 29.7% · Flash 23.2% · 16 apps en launcher** |

---

## Fase 1 — La Base (Completo)

> *Objetivo: Un sistema que evalúa expresiones matemáticas y las muestra en pantalla.*

### Motor Matemático
- [x] **Tokenizer** (`Tokenizer.cpp`): String → lista de `Token` — números, operadores, funciones, paréntesis, variables.
- [x] **Parser Shunting-Yard** (`Parser.cpp`): Genera Notación Polaca Inversa (RPN) para evaluación numérica eficiente.
- [x] **Evaluador RPN** (`Evaluator.cpp`): Calcula el resultado numérico (`double`) con pila. Trigonometría, logarítmos, exponencial, constantes π y e.
- [x] **VariableContext** (`VariableContext.cpp`): Variables `A-Z` y `Ans`. Lectura/escritura O(1). Persistencia LittleFS.
- [x] **StepLogger** (`StepLogger.cpp`): Registro de pasos del parser para depuración.

### Drivers de Bajo Nivel
- [x] **DisplayDriver**: Wrapper TFT_eSPI con doble buffer DMA e inicialización LVGL.
- [x] **KeyMatrix**: Escaneo matricial 6×8 — debounce 20 ms, autorepeat 500 ms, soporte SHIFT/ALPHA.
- [x] **Config.h**: Pinout centralizado para ESP32-S3 N16R8 CAM.

---

## Fase 2 — Natural Display V.P.A.M. (Completo)

> *Objetivo: Las expresiones se renderizan exactamente como en una calculadora física moderna.*

### Árbol de Expresión Visual (AST)
- [x] **ExprNode** (`ExprNode.h`): Árbol dinámico — nodos `TEXT`, `FRACTION`, `ROOT`, `POWER`.
- [x] **Medición recursiva** `measure()`: Cada nodo calcula ancho y alto en píxeles de forma recursiva.
- [x] **Renderizado recursivo** `draw()`: Dibuja cada nodo en posición relativa correcta al bloque padre.

### Navegación 2D y Edición
- [x] **Cursor inteligente**: `RIGHT` al final del numerador salta al denominador. `UP/DOWN` en potencia entra/sale del superíndice.
- [x] **Borrado atómico**: `DEL` destruye estructuras complejas (fracciones, raíces, funciones) como bloque único.

### CalculationApp
- [x] **Historial de 32 entradas** con scroll vertical.
- [x] **Copia de resultados**: `UP` copia resultado o expresión al editor activo.
- [x] **Soporte variables** A-Z + `Ans`. Evaluación completa de expresiones.

---

## Fase 3 — Launcher 3.0 e Integración (Completo)

> *Objetivo: Sistema de apps completo con launcher, navegación y herramientas de debugging.*

### Launcher y SystemApp
- [x] **Grid 3 columnas**: Launcher de iconos inspirado en NumWorks/Casio fx-CG.
- [x] **Scroll vertical**: Menú sincronizado — icono seleccionado siempre visible.
- [x] **10 Apps registradas**: Calculation, Equations, Sequences, Grapher, Regression, Statistics, Table, Probability, Python, Settings.
- [x] **Dispatcher de eventos**: `SystemApp::injectKey()` enruta a MENU o APP activa. `KeyCode::MODE` interceptado — siempre retorna al menú.

### Serial Bridge y Teclado Virtual
- [x] **SerialBridge**: Teclado virtual PC vía Serial Monitor — WASD, z/x/c/h, dígitos, operadores.
- [x] **LvglKeypad**: Adaptador LVGL 9.x `indev` tipo `LV_INDEV_TYPE_KEYPAD`.
- [x] **Heartbeat**: Ping serial cada 5 s, eco inmediato de cada byte recibido.

---

## Fase 4 — LVGL 9.x: Revolución Visual (Completo)

> *Objetivo: Abandonar el renderizado directo y adoptar LVGL para una UI de nivel profesional.*

### HW Bring-Up ESP32-S3 N16R8 CAM

| Fix | Problema | Solución Aplicada |
|:----|:---------|:------------------|
| ① | Crash `Illegal instruction` en boot | `board_build.arduino.memory_type = qio_opi` + `flash_mode = qio` |
| ② | Crash en `TFT_eSPI::begin()` addr `0x10` | `-DUSE_FSPI_PORT` → `SPI_PORT=2` → `REG_SPI_BASE(2)=0x60024000` |
| ③ | Líneas y artefactos en pantalla | `SPI_FREQUENCY=10000000` (10 MHz) |
| ④ | LVGL pantalla siempre negra | `heap_caps_malloc(DMA+8BIT)` — jamás `ps_malloc` para buffers LVGL |
| ⑤ | GPIO 45 BL cortocircuito | `pinMode(45, INPUT)` — pin cableado fijo a 3.3V |
| ⑥ | Serial CDC perdido en boot | `while(!Serial && t0<3000)` + `monitor_rts=0` |

### LVGL
- [x] `lv_conf.h`: `LV_MEM_CUSTOM=1` (PSRAM), `LV_TICK_CUSTOM=1`, `LV_COLOR_DEPTH=16`, fuentes Montserrat 12/14/20.
- [x] **SplashScreen animado**: Fade-in logo + versión con `lv_anim_t` ease_in_out.
- [x] **MainMenu LVGL**: Grid 3×N con scroll, highlight selección, navegación por teclado.
- [x] **LittleFS**: Variables persistentes con `formatOnFail`.

---

## Fase 5 — CAS-Lite Engine + EquationsApp (Completo)

> *Objetivo: Álgebra simbólica nativa con pasos detallados. La primera calculadora open-source ESP32-S3 con CAS propio en PSRAM.*

### CAS-Lite Engine — Álgebra Simbólica

- [x] **PSRAMAllocator\<T\>** (`cas/PSRAMAllocator.h`)
  Allocator STL-compatible que redirige `allocate`/`deallocate` a `ps_malloc`/`ps_free`. Todo el CAS vive en los 8 MB PSRAM OPI, sin presión sobre la RAM interna.

- [x] **SymPoly** (`cas/SymPoly.h/.cpp`)
  Polinomio simbólico en variable única. Coeficientes `Rational` (fracción exacta `p/q`). Operaciones: suma, resta, multiplicación, derivación simbólica, evaluación numérica, normalización.

- [x] **ASTFlattener** (`cas/ASTFlattener.h/.cpp`)
  Recorre el `ExprNode` AST visual y lo convierte en un `SymPoly`. Soporta potencias enteras, constantes π/e, y coeficientes racionales anidados.

- [x] **SingleSolver** (`cas/SingleSolver.h/.cpp`)
  - Grado 1 → `x = -b/a` (solución directa)
  - Grado 2 → fórmula cuadrática analítica con discriminante Δ = b² - 4ac
  - Grado ≥ 3 → Newton-Raphson numérico (semilla adaptativa)
  - Genera pasos detallados: normalización, cálculo Δ, aplicación fórmula, resultados

- [x] **SystemSolver** (`cas/SystemSolver.h/.cpp`)
  Sistemas 2×2 lineales por eliminación gaussiana simbólica. Detecta automáticamente sistemas indeterminados (∞ soluciones) e incompatibles (sin solución).

- [x] **CASStepLogger** (`cas/CASStepLogger.h/.cpp`)
  `StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>`. Tipos: INFO · FORMULA · RESULT · ERROR. `.clear()` libera la memoria PSRAM al salir de la app.

- [x] **SymToAST** (`cas/SymToAST.h/.cpp`)
  Bridge inverso: convierte resultados `Rational` del CAS en nodos `ExprNode` para renderizado Natural Display en la EquationsApp.

### Tests CAS — 53 Unitarios

| Grupo | Tests | Qué valida |
|:------|:-----:|:-----------|
| **Fase A** — Fundamentos | 1–18 | `Rational` (aritmética exacta, simplificación, MCD). `SymPoly` (suma, resta, mul, derivación, evaluación). |
| **Fase B** — ASTFlattener | 19–32 | Conversión AST → SymPoly para polinomios, constantes, potencias, funciones. |
| **Fase C** — SingleSolver | 33–44 | Lineal (1 solución), cuadrática (2 raíces reales, raíz doble, Δ < 0), con pasos. |
| **Fase D** — SystemSolver | 45–53 | Sistema determinado, indeterminado (∞ sol.), incompatible (sin sol.). |

```ini
# platformio.ini — activar tests:
build_flags      = ... -DCAS_RUN_TESTS
build_src_filter = +<*> +<../tests/CASTest.cpp>
```

### EquationsApp

- [x] **UI LVGL nativa** — 4 estados: `SELECT` → `EQ_INPUT` → `RESULT` → `STEPS`
- [x] **Modo 1 — Ecuación (1 var)**: Lineal y cuadrática con discriminante y fórmula cuadrática
- [x] **Modo 2 — Sistema 2×2**: Eliminación gaussiana, detección indeterminado/incompatible
- [x] **Pantalla de pasos**: `KeyCode::SHOW_STEPS` (R2C6) activa pasos desde resultado
- [x] **Natural Display en resultados**: x₁=2, x₂=3 renderizados como expresiones visuales
- [x] **Gestión de memoria**: `end()` llama `.clear()` en `StepVec` — sin fugas en PSRAM
- [x] **Registro en SystemApp**: App id=5, `g_lvglActive=true`, lifecycle correcto

### Build Stats (Producción — tests desactivados)

| Recurso | Usado | Total | % |
|:--------|------:|------:|:-:|
| RAM (data+bss) | 94 512 B | 327 680 B | **28.8%** |
| Flash (program) | 1 263 109 B | 6 553 600 B | **19.3%** |

---

## Fase CAS Elite — Pro-CAS Engine + CalculusApp Unificada (Completo)

> *Objetivo: Evolución CAS-Lite → Pro-CAS. Motor simbólico completo con derivadas, integrales, simplificación multi-pass y resolución de ecuaciones no lineales. Ver [CAS_UPGRADE_ROADMAP.md](CAS_UPGRADE_ROADMAP.md) para el desglose de las 6 fases internas.*

### Pro-CAS Engine — 6 Fases completadas

- [x] **Fase 0**: Research & Planning — Diseño de SymExpr DAG, hash-consing, aritmética bignum
- [x] **Fase 1**: CASInt + CASRational — BigInt híbrido (int64+mbedtls_mpi), fracción overflow-safe
- [x] **Fase 2**: SymExpr DAG + ConsTable + Arena — Árbol inmutable con hash-consing en PSRAM
- [x] **Fase 3**: SymSimplify + SymDiff — Simplificador fixed-point (8 passes), derivación 17 reglas
- [x] **Fase 4**: ASTFlattener v2 + OmniSolver + HybridNewton — MathAST→SymExpr, solver avanzado
- [x] **Fase 5**: SymPolyMulti + SystemSolver NL — Resultante de Sylvester, sistemas no lineales
- [x] **Fase 6A**: CalculusApp — Derivadas simbólicas con Natural Display y pasos detallados
- [x] **Fase 6B**: IntegralApp — Integrales Slagle (tabla/u-sub/partes), +C, ∫, pasos
- [x] **Fase 6B**: SymIntegrate — Slagle heurístico: tabla directa, linealidad, u-sustitución, partes LIATE
- [x] **Fase 6B**: SymExprToAST — Bridge SymExpr → MathAST con `convertIntegral()` (+C)
- [x] **Fase 7**: **CalculusApp unificada** — Derivadas + Integrales en app única con pestañas d/dx ↔ ∫dx
- [x] **Fase 7**: **SettingsApp** — Raíces complejas, precisión decimal, modo angular

### Build Stats (Producción — Marzo 2026)

| Recurso | Usado | Total | % |
|:--------|------:|------:|:-:|
| RAM | 94 512 B | 327 680 B | **28.8%** |
| Flash | 1 263 109 B | 6 553 600 B | **19.3%** |

---

## Fase 6 — Apps Científicas Completas (Planificado)

> *Objetivo: NumOS se convierte en una calculadora científica completa para uso académico real, superando a la Casio fx-991EX en funcionalidades.*

### 6.1 Statistics App
- [ ] Introducción de listas de datos (hasta 200 elementos con scroll)
- [ ] Media aritmética, mediana, moda, rango
- [ ] Varianza y desviación estándar (población σ y muestra s)
- [ ] Histograma y diagrama de caja en pantalla
- [ ] Percentiles y cuartiles Q1/Q2/Q3

### 6.2 Regression App
- [ ] Regresión lineal (a + bx) con coeficiente R² y ecuación de la recta
- [ ] Regresión cuadrática (a + bx + cx²)
- [ ] Regresión logarítmica y exponencial
- [ ] Scatter plot con curva ajustada superpuesta en graficadora

### 6.3 Sequences App
- [ ] Sucesiones aritméticas: primer término, razón, término N, suma parcial SN
- [ ] Sucesiones geométricas: primer término, razón, término N, suma N
- [ ] Verificación automática del tipo (aritmética / geométrica / ninguna)
- [ ] Tabla scrollable de primeros N términos

### 6.4 Probability App
- [ ] Combinaciones nCr y permutaciones nPr
- [ ] Factorial n! (hasta 20!)
- [ ] Distribución binomial: P(X=k), P(X≤k)
- [ ] Distribución normal: densidad, acumulada Φ(z), inversa
- [ ] Distribución de Poisson: P(X=k)

### 6.5 Table App (ampliación de GrapherApp)
- [ ] Tabla x/f(x) con paso configurable (Δx)
- [ ] Scroll vertical de filas, anchura fija de columnas
- [ ] Sincronizada con la función activa en GrapherApp

### 6.6 Settings App ✅ Completo
- [x] Toggle raíces complejas (ON/OFF)
- [x] Selector de precisión decimal (6/8/10/12)
- [x] Visualización de modo angular (informativo)
- [ ] Brillo de pantalla (PWM si BL reconectado a GPIO OUTPUT)
- [ ] Formato numérico: decimal fijo / científico / engineering
- [ ] Reset de fábrica: borrar todas las variables, restaurar configuración
- [ ] Información del sistema: versión firmware, RAM libre, Flash libre

### 6.7 EquationsApp — Ampliaciones
- [ ] Ecuaciones cúbicas (grado 3) con método de Cardano analítico
- [ ] Sistema 3×3 por eliminación gaussiana extendida
- [ ] Visualización de raíces en la graficadora integrada
- [ ] Modo ecuación paramétrica

---

## Fase 7 — Matrices + Complejos + Bases (Planificado)

> *Objetivo: Alcanzar la paridad con el CAS de la HP Prime G2 y la NumWorks.*

### 7.1 Simplificación y Factorización Avanzada
- [ ] Reducción de términos semejantes: 2x + x → 3x
- [ ] Factorización de polinomios: x² - 5x + 6 → (x-2)(x-3)
- [ ] Expansión de productos notables
- [ ] Cancelación en fracciones algebraicas
- [ ] Integral definida numérica de alta precisión (Gauss-Legendre)
- [ ] Visualización: área sombreada bajo la curva en graficadora
- [ ] Series de Taylor / Maclaurin

### 7.2 Matrices
- [ ] Editor de matrices m×n con navegación 2D en pantalla
- [ ] Operaciones: suma, resta, multiplicación, transpuesta
- [ ] Determinante 2×2 y 3×3
- [ ] Inversa por Gauss-Jordan
- [ ] Resolución del sistema Ax = b por matrices
- [ ] Valores propios para matrices 2×2

### 7.3 Números Complejos
- [ ] Modo complejo activable en Settings
- [ ] Entrada forma rectangular (a+bi) y polar (r∠θ)
- [ ] Operaciones básicas: +, -, ×, ÷, conjugado, módulo, argumento
- [ ] Plano de Argand (visualización gráfica en GrapherApp)

### 7.4 Álgebra de Bases y Conversión de Unidades
- [ ] Conversor DEC / HEX / BIN / OCT integrado
- [ ] Operaciones aritméticas en base arbitraria n
- [ ] Conversor de unidades: longitud, masa, temperatura, velocidad, energía

---

## Fase 8 — Hardware Final + Conectividad + Scripting (Planificado)

> *Objetivo: NumOS se convierte en un producto físico completo, portable, autónomo y conectado.*

### 8.1 Teclado Físico — Resolución del Conflicto GPIO
- [ ] Reasignar ROW3 (GPIO 4) y ROW4 (GPIO 5) a GPIOs libres (propuesta: GPIO 15, 16)
- [ ] Actualizar `Config.h` con nuevas asignaciones
- [ ] Integrar `KeyMatrix::scan()` → `LvglKeypad::pushKey()` en tiempo real
- [ ] Test completo de 48 teclas con `HardwareTest.cpp`
- [ ] Overlay visual SHIFT/ALPHA en barra de estado (capa activa)
- [ ] Funciones secundarias (teclas amarillas/rojas) mapeadas a `KeyCode::SHIFT_X`

### 8.2 PCB Propia
- [ ] Esquemático completo en KiCad con ESP32-S3 WROOM integrado
- [ ] Conector FPC/ZIF para pantalla ILI9341
- [ ] Conector JST-PH 2 pines para batería LiPo
- [ ] TP4056 con carga USB-C + boost converter MT3608 (3.7V → 5V)
- [ ] Test points JTAG + SWD para depuración
- [ ] Trazado PCB 2 capas, form factor calculadora (≈85×165mm)

### 8.3 Batería y Gestión de Energía
- [ ] LiPo 1000–2000 mAh (según volumen del chasis)
- [ ] TP4056: carga vía USB-C, indicador LED
- [ ] Monitor nivel batería por ADC (GPIO libre) con conversión al porcentaje
- [ ] Indicador gráfico en barra de estado (icono batería animado)
- [ ] Modo bajo consumo: reducir LVGL refresh rate + bajar frecuencia CPU
- [ ] Deep sleep con wake-up por tecla ON/OFF dedicada

### 8.4 Carcasa 3D
- [ ] Diseño en FreeCAD o Fusion 360 siguiendo dimensiones de `DIMENSIONES_DISEÑO.md`
- [ ] Material: PLA o PETG, colores negro mate / gris
- [ ] Ventana de pantalla con bisel y protección de cristal acrílico
- [ ] Soporte para membrana de teclado o botones táctiles
- [ ] Tapa trasera con tornillos M2 y compartimento batería accesible

### 8.5 Conectividad WiFi
- [ ] **WebBridge**: servidor HTTP embebido en el ESP32-S3 para transferir programas/scripts desde el PC
- [ ] **OTA (Over The Air)**: actualización de firmware sin cables desde el navegador
- [ ] **Sincronización de variables**: backup y restore de `/vars.dat` vía WiFi
- [ ] **NTP**: sincronización de hora para reloj interno y registro de tiempo

### 8.6 Scripting
- [ ] **Lua embebido** (eLua / LittleLua) — lenguaje de scripting para programar la calculadora
- [ ] Editor de scripts en pantalla con cursor 2D y resaltado de sintaxis básico
- [ ] Acceso a todas las funciones matemáticas desde Lua
- [ ] Scripts guardados/cargados desde LittleFS
- [ ] Ejecución por línea (`ENTER`) o bloque completo (`SHIFT+ENTER`)

### 8.7 Funciones Matemáticas Adicionales
- [ ] `log₂(x)`, `logₙ(x, b)` — logaritmo en base arbitraria
- [ ] `ceil`, `floor`, `round`, `frac` — redondeo
- [ ] `mod` — operador módulo para enteros
- [ ] `gcd`, `lcm` — máximo común divisor / mínimo común múltiplo
- [ ] Proyección angular DEG↔RAD↔GRA directa en resultado
- [ ] `Σ` — Sumatorio sobre expresión (límite inferior/superior configurable)
- [ ] `Π` — Productorio
- [ ] `∫` — Integral definida numérica de alta precisión

---

## Visión a Largo Plazo — La Mejor Calculadora Open-Source del Mundo

**NumOS aspira a demostrar que una calculadora open-source sobre hardware de 15 € puede superar en funcionalidades a calculadoras comerciales de 180 €.**

### Comparativa Objetivo

| Calculadora | Precio | CAS | Pasos | Python | Batería | Open Source |
|:------------|:------:|:---:|:-----:|:------:|:-------:|:-----------:|
| **NumOS (objetivo)** | **15 €** | ✅ | ✅ | ✅ | ✅ | ✅ MIT |
| Casio fx-991EX ClassWiz | 20 € | ❌ | ❌ | ❌ | AAA | ❌ |
| NumWorks | 79 € | ✅ | ❌ | ✅ | ✅ | ✅ MIT |
| TI-84 Plus CE | 119 € | ❌ | ❌ | TI-BASIC | AAA | ❌ |
| HP Prime G2 | 179 € | ✅ | ✅ | HP PPL | ✅ | ❌ |

### Diferencial Único de NumOS

1. **Pasos de resolución** — Ninguna calculadora económica muestra los pasos intermedios del CAS tan claramente.
2. **Open source total** — Todo el pipeline matemático, desde el Tokenizer hasta el CAS, es auditable, modificable y educativo.
3. **Hardware a medida** — El proyecto controla todo: desde la PCB hasta el firmware.
4. **Coste 10x inferior** — 15 € de hardware para capacidades de una calculadora de 180 €.
5. **Extensible en minutos** — Añadir una nueva app = ~100 líneas de C++17; nunca toca el core.
6. **Comunidad abierta** — Modelo de contribución libre, no dependiente de ninguna empresa.

---

*NumOS — Open-source scientific calculator OS for ESP32-S3.*
*Cada commit es un paso hacia la mejor calculadora científica del mundo.*

*Última actualización: Marzo 2026*
