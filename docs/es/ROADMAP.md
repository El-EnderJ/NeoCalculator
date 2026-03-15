# NumOS  Project Roadmap

&gt; **ltima actualizacin:** Marzo 2026
&gt;
&gt; Registro histrico y plan de futuro de NumOS. Cada fase construye sobre la anterior hasta alcanzar una calculadora cientfica open-source que rivalice con las mejores del mercado.

---

## Progreso General

| Fase | Descripcin | Estado | % |
|:-----|:------------|:------:|:-:|
| **Fase 1** | Cimientos  Math Engine y Drivers | ? Completo | 100% |
| **Fase 2** | Natural Display V.P.A.M. y Navegacin 2D | ? Completo | 100% |
| **Fase 3** | Launcher 3.0, SerialBridge y Documentacin | ? Completo | 100% |
| **Fase 4** | Migracin a LVGL 9.x  HW Bring-Up ESP32-S3 | ? Completo | 100% |
| **Fase 5** | CAS-Lite Engine + EquationsApp | ? Completo | 100% |
| **CAS Elite** | Pro-CAS: BigNum, DAG, Derivadas, Integrales, CalculusApp unificada, SettingsApp | ? **Completo** | 100% |
| **Fase 6** | Apps Cientficas Completas | ? **Completo** | 100% |
| **Simulaciones** | ParticleLab (30+ materiales), CircuitCore (SPICE), Fluid2D | ? **Completo** | 100% |
| **Fase 7** | Matrices + Complejos + Bases | ?? Planificado | 0% |
| **Fase 8** | Hardware Final + Conectividad + Scripting | ?? Planificado | 0% |

---

## Historial de Hitos

| Fecha | Hito |
|:------|:-----|
| Nov 2025 | Primer clculo numrico correcto en terminal serie |
| Dic 2025 | Natural Display con fracciones reales apiladas en pantalla TFT |
| Ene 2026 | Launcher 3.0 con 10 apps registradas y scroll vertical |
| Feb 2026 | HW bring-up completo: 6 bugs crticos ESP32-S3 resueltos |
| Feb 2026 | LVGL 9.x operativo  Launcher con iconos en ILI9341 IPS |
| Feb 2026 | Splash screen animado + SerialBridge + LittleFS en produccin |
| **Feb 2026** | **CAS-Lite Engine completo: SymPoly  SingleSolver  SystemSolver  53 tests** |
| **Feb 2026** | **EquationsApp UI: lineal, cuadrtica, sistema 22 con pasos en PSRAM** |
| **Feb 2026** | **Pro-CAS Engine: CASInt, CASRational, SymExpr DAG, ConsTable, SymDiff (17 reglas), SymIntegrate (Slagle), SymSimplify 8-pass, OmniSolver, SymPolyMulti (resultante)** |
| **Feb 2026** | **CalculusApp: derivadas simblicas con Natural Display y pasos** |
| **Feb 2026** | **IntegralApp: integrales Slagle (tabla/u-sub/partes), +C, pasos** |
| **Feb 2026** | **Produccin activa: RAM 29.0%  Flash 18.5%  tests desactivados** |
| **Mar 2026** | **CalculusApp unificada: derivadas + integrales en app nica con pestaas d/dx ? ?dx** |
| **Mar 2026** | **SettingsApp: races complejas, precisin decimal, modo angular** |
| **Mar 2026** | **Produccin activa: RAM 28.8%  Flash 19.3%  tests desactivados** |
| **Mar 2026** | **ParticleLabApp (ID 15)  Alchemy Update: 30+ materiales, electrnica spark, transiciones de fase, matriz de reacciones, herramienta de lnea Bresenham, guardado/carga LittleFS** |
| **Mar 2026** | **Produccin activa: RAM 29.7%  Flash 23.2%  16 apps en launcher** |

---

## Fase 1  La Base (Completo)

&gt; *Objetivo: Un sistema que evala expresiones matemticas y las muestra en pantalla.*

### Motor Matemtico
- [x] **Tokenizer** (`Tokenizer.cpp`): String ? lista de `Token`  nmeros, operadores, funciones, parntesis, variables.
- [x] **Parser Shunting-Yard** (`Parser.cpp`): Genera Notacin Polaca Inversa (RPN) para evaluacin numrica eficiente.
- [x] **Evaluador RPN** (`Evaluator.cpp`): Calcula el resultado numrico (`double`) con pila. Trigonometra, logartmos, exponencial, constantes p y e.
- [x] **VariableContext** (`VariableContext.cpp`): Variables `A-Z` y `Ans`. Lectura/escritura O(1). Persistencia LittleFS.
- [x] **StepLogger** (`StepLogger.cpp`): Registro de pasos del parser para depuracin.

### Drivers de Bajo Nivel
- [x] **DisplayDriver**: Wrapper TFT_eSPI con doble buffer DMA e inicializacin LVGL.
- [x] **KeyMatrix**: Escaneo matricial 68  debounce 20 ms, autorepeat 500 ms, soporte SHIFT/ALPHA.
- [x] **Config.h**: Pinout centralizado para ESP32-S3 N16R8 CAM.

---

## Fase 2  Natural Display V.P.A.M. (Completo)

&gt; *Objetivo: Las expresiones se renderizan exactamente como en una calculadora fsica moderna.*

### rbol de Expresin Visual (AST)
- [x] **ExprNode** (`ExprNode.h`): rbol dinmico  nodos `TEXT`, `FRACTION`, `ROOT`, `POWER`.
- [x] **Medicin recursiva** `measure()`: Cada nodo calcula ancho y alto en pxeles de forma recursiva.
- [x] **Renderizado recursivo** `draw()`: Dibuja cada nodo en posicin relativa correcta al bloque padre.

### Navegacin 2D y Edicin
- [x] **Cursor inteligente**: `RIGHT` al final del numerador salta al denominador. `UP/DOWN` en potencia entra/sale del superndice.
- [x] **Borrado atmico**: `DEL` destruye estructuras complejas (fracciones, races, funciones) como bloque nico.

### CalculationApp
- [x] **Historial de 32 entradas** con scroll vertical.
- [x] **Copia de resultados**: `UP` copia resultado o expresin al editor activo.
- [x] **Soporte variables** A-Z + `Ans`. Evaluacin completa de expresiones.

---

## Fase 3  Launcher 3.0 e Integracin (Completo)

&gt; *Objetivo: Sistema de apps completo con launcher, navegacin y herramientas de debugging.*

### Launcher y SystemApp
- [x] **Grid 3 columnas**: Launcher de iconos inspirado en NumWorks/Casio fx-CG.
- [x] **Scroll vertical**: Men sincronizado  icono seleccionado siempre visible.
- [x] **10 Apps registradas**: Calculation, Equations, Sequences, Grapher, Regression, Statistics, Table, Probability, Python, Settings.
- [x] **Dispatcher de eventos**: `SystemApp::injectKey()` enruta a MENU o APP activa. `KeyCode::MODE` interceptado  siempre retorna al men.

### Serial Bridge y Teclado Virtual
- [x] **SerialBridge**: Teclado virtual PC va Serial Monitor  WASD, z/x/c/h, dgitos, operadores.
- [x] **LvglKeypad**: Adaptador LVGL 9.x `indev` tipo `LV_INDEV_TYPE_KEYPAD`.
- [x] **Heartbeat**: Ping serial cada 5 s, eco inmediato de cada byte recibido.

---

## Fase 4  LVGL 9.x: Revolucin Visual (Completo)

&gt; *Objetivo: Abandonar el renderizado directo y adoptar LVGL para una UI de nivel profesional.*

### HW Bring-Up ESP32-S3 N16R8 CAM

| Fix | Problema | Solucin Aplicada |
|:----|:---------|:------------------|
| ? | Crash `Illegal instruction` en boot | `board_build.arduino.memory_type = qio_opi` + `flash_mode = qio` |
| ? | Crash en `TFT_eSPI::begin()` addr `0x10` | `-DUSE_FSPI_PORT` ? `SPI_PORT=2` ? `REG_SPI_BASE(2)=0x60024000` |
| ? | Lneas y artefactos en pantalla | `SPI_FREQUENCY=10000000` (10 MHz) |
| ? | LVGL pantalla siempre negra | `heap_caps_malloc(DMA+8BIT)`  jams `ps_malloc` para buffers LVGL |
| ? | GPIO 45 BL cortocircuito | `pinMode(45, INPUT)`  pin cableado fijo a 3.3V |
| ? | Serial CDC perdido en boot | `while(!Serial && t0&lt;3000)` + `monitor_rts=0` |

### LVGL
- [x] `lv_conf.h`: `LV_MEM_CUSTOM=1` (PSRAM), `LV_TICK_CUSTOM=1`, `LV_COLOR_DEPTH=16`, fuentes Montserrat 12/14/20.
- [x] **SplashScreen animado**: Fade-in logo + versin con `lv_anim_t` ease_in_out.
- [x] **MainMenu LVGL**: Grid 3N con scroll, highlight seleccin, navegacin por teclado.
- [x] **LittleFS**: Variables persistentes con `formatOnFail`.

---

## Fase 5  CAS-Lite Engine + EquationsApp (Completo)

&gt; *Objetivo: lgebra simblica nativa con pasos detallados. La primera calculadora open-source ESP32-S3 con CAS propio en PSRAM.*

### CAS-Lite Engine  lgebra Simblica

- [x] **PSRAMAllocator\&lt;T\&gt;** (`cas/PSRAMAllocator.h`)
  Allocator STL-compatible que redirige `allocate`/`deallocate` a `ps_malloc`/`ps_free`. Todo el CAS vive en los 8 MB PSRAM OPI, sin presin sobre la RAM interna.

- [x] **SymPoly** (`cas/SymPoly.h/.cpp`)
  Polinomio simblico en variable nica. Coeficientes `Rational` (fraccin exacta `p/q`). Operaciones: suma, resta, multiplicacin, derivacin simblica, evaluacin numrica, normalizacin.

- [x] **ASTFlattener** (`cas/ASTFlattener.h/.cpp`)
  Recorre el `ExprNode` AST visual y lo convierte en un `SymPoly`. Soporta potencias enteras, constantes p/e, y coeficientes racionales anidados.

- [x] **SingleSolver** (`cas/SingleSolver.h/.cpp`)
  - Grado 1 ? `x = -b/a` (solucin directa)
  - Grado 2 ? frmula cuadrtica analtica con discriminante ? = b - 4ac
  - Grado = 3 ? Newton-Raphson numrico (semilla adaptativa)
  - Genera pasos detallados: normalizacin, clculo ?, aplicacin frmula, resultados

- [x] **SystemSolver** (`cas/SystemSolver.h/.cpp`)
  Sistemas 22 lineales por eliminacin gaussiana simblica. Detecta automticamente sistemas indeterminados (8 soluciones) e incompatibles (sin solucin).

- [x] **CASStepLogger** (`cas/CASStepLogger.h/.cpp`)
  `StepVec = std::vector<CASStep, PSRAMAllocator<CASStep>>`. Tipos: INFO  FORMULA  RESULT  ERROR. `.clear()` libera la memoria PSRAM al salir de la app.

- [x] **SymToAST** (`cas/SymToAST.h/.cpp`)
  Bridge inverso: convierte resultados `Rational` del CAS en nodos `ExprNode` para renderizado Natural Display en la EquationsApp.

### Tests CAS  53 Unitarios

| Grupo | Tests | Qu valida |
|:------|:-----:|:-----------|
| **Fase A**  Fundamentos | 118 | `Rational` (aritmtica exacta, simplificacin, MCD). `SymPoly` (suma, resta, mul, derivacin, evaluacin). |
| **Fase B**  ASTFlattener | 1932 | Conversin AST ? SymPoly para polinomios, constantes, potencias, funciones. |
| **Fase C**  SingleSolver | 3344 | Lineal (1 solucin), cuadrtica (2 races reales, raz doble, ? &lt; 0), con pasos. |
| **Fase D**  SystemSolver | 4553 | Sistema determinado, indeterminado (8 sol.), incompatible (sin sol.). |

```ini
# platformio.ini  activar tests:
build_flags      = ... -DCAS_RUN_TESTS
build_src_filter = +<*> +<../tests/CASTest.cpp>
```

### EquationsApp

- [x] **UI LVGL nativa**  4 estados: `SELECT` ? `EQ_INPUT` ? `RESULT` ? `STEPS`
- [x] **Modo 1  Ecuacin (1 var)**: Lineal y cuadrtica con discriminante y frmula cuadrtica
- [x] **Modo 2  Sistema 22**: Eliminacin gaussiana, deteccin indeterminado/incompatible
- [x] **Pantalla de pasos**: `KeyCode::SHOW_STEPS` (R2C6) activa pasos desde resultado
- [x] **Natural Display en resultados**: x1=2, x2=3 renderizados como expresiones visuales
- [x] **Gestin de memoria**: `end()` llama `.clear()` en `StepVec`  sin fugas en PSRAM
- [x] **Registro en SystemApp**: App id=5, `g_lvglActive=true`, lifecycle correcto

### Build Stats (Produccin  tests desactivados)

| Recurso | Usado | Total | % |
|:--------|------:|------:|:-:|
| RAM (data+bss) | 94 512 B | 327 680 B | **28.8%** |
| Flash (program) | 1 263 109 B | 6 553 600 B | **19.3%** |

---

## Fase CAS Elite  Pro-CAS Engine + CalculusApp Unificada (Completo)

&gt; *Objetivo: Evolucin CAS-Lite ? Pro-CAS. Motor simblico completo con derivadas, integrales, simplificacin multi-pass y resolucin de ecuaciones no lineales. Ver [CAS_UPGRADE_ROADMAP.md](CAS_UPGRADE_ROADMAP.md) para el desglose de las 6 fases internas.*

### Pro-CAS Engine  6 Fases completadas

- [x] **Fase 0**: Research & Planning  Diseo de SymExpr DAG, hash-consing, aritmtica bignum
- [x] **Fase 1**: CASInt + CASRational  BigInt hbrido (int64+mbedtls_mpi), fraccin overflow-safe
- [x] **Fase 2**: SymExpr DAG + ConsTable + Arena  rbol inmutable con hash-consing en PSRAM
- [x] **Fase 3**: SymSimplify + SymDiff  Simplificador fixed-point (8 passes), derivacin 17 reglas
- [x] **Fase 4**: ASTFlattener v2 + OmniSolver + HybridNewton  MathAST?SymExpr, solver avanzado
- [x] **Fase 5**: SymPolyMulti + SystemSolver NL  Resultante de Sylvester, sistemas no lineales
- [x] **Fase 6A**: CalculusApp  Derivadas simblicas con Natural Display y pasos detallados
- [x] **Fase 6B**: IntegralApp  Integrales Slagle (tabla/u-sub/partes), +C, ?, pasos
- [x] **Fase 6B**: SymIntegrate  Slagle heurstico: tabla directa, linealidad, u-sustitucin, partes LIATE
- [x] **Fase 6B**: SymExprToAST  Bridge SymExpr ? MathAST con `convertIntegral()` (+C)
- [x] **Fase 7**: **CalculusApp unificada**  Derivadas + Integrales en app nica con pestaas d/dx ? ?dx
- [x] **Fase 7**: **SettingsApp**  Races complejas, precisin decimal, modo angular

### Build Stats (Produccin  Marzo 2026)

| Recurso | Usado | Total | % |
|:--------|------:|------:|:-:|
| RAM | 94 512 B | 327 680 B | **28.8%** |
| Flash | 1 263 109 B | 6 553 600 B | **19.3%** |

---

## Fase 6  Apps Cientficas Completas (Planificado)

&gt; *Objetivo: NumOS se convierte en una calculadora cientfica completa para uso acadmico real, superando a la Casio fx-991EX en funcionalidades.*

### 6.1 Statistics App
- [ ] Introduccin de listas de datos (hasta 200 elementos con scroll)
- [ ] Media aritmtica, mediana, moda, rango
- [ ] Varianza y desviacin estndar (poblacin s y muestra s)
- [ ] Histograma y diagrama de caja en pantalla
- [ ] Percentiles y cuartiles Q1/Q2/Q3

### 6.2 Regression App
- [ ] Regresin lineal (a + bx) con coeficiente R y ecuacin de la recta
- [ ] Regresin cuadrtica (a + bx + cx)
- [ ] Regresin logartmica y exponencial
- [ ] Scatter plot con curva ajustada superpuesta en graficadora

### 6.3 Sequences App
- [ ] Sucesiones aritmticas: primer trmino, razn, trmino N, suma parcial SN
- [ ] Sucesiones geomtricas: primer trmino, razn, trmino N, suma N
- [ ] Verificacin automtica del tipo (aritmtica / geomtrica / ninguna)
- [ ] Tabla scrollable de primeros N trminos

### 6.4 Probability App
- [ ] Combinaciones nCr y permutaciones nPr
- [ ] Factorial n! (hasta 20!)
- [ ] Distribucin binomial: P(X=k), P(X=k)
- [ ] Distribucin normal: densidad, acumulada F(z), inversa
- [ ] Distribucin de Poisson: P(X=k)

### 6.5 Table App (ampliacin de GrapherApp)
- [ ] Tabla x/f(x) con paso configurable (?x)
- [ ] Scroll vertical de filas, anchura fija de columnas
- [ ] Sincronizada con la funcin activa en GrapherApp

### 6.6 Settings App ? Completo
- [x] Toggle races complejas (ON/OFF)
- [x] Selector de precisin decimal (6/8/10/12)
- [x] Visualizacin de modo angular (informativo)
- [ ] Brillo de pantalla (PWM si BL reconectado a GPIO OUTPUT)
- [ ] Formato numrico: decimal fijo / cientfico / engineering
- [ ] Reset de fbrica: borrar todas las variables, restaurar configuracin
- [ ] Informacin del sistema: versin firmware, RAM libre, Flash libre

### 6.7 EquationsApp  Ampliaciones
- [ ] Ecuaciones cbicas (grado 3) con mtodo de Cardano analtico
- [ ] Sistema 33 por eliminacin gaussiana extendida
- [ ] Visualizacin de races en la graficadora integrada
- [ ] Modo ecuacin paramtrica

---

## Fase 7  Matrices + Complejos + Bases (Planificado)

&gt; *Objetivo: Alcanzar la paridad con el CAS de la HP Prime G2 y la NumWorks.*

### 7.1 Simplificacin y Factorizacin Avanzada
- [ ] Reduccin de trminos semejantes: 2x + x ? 3x
- [ ] Factorizacin de polinomios: x - 5x + 6 ? (x-2)(x-3)
- [ ] Expansin de productos notables
- [ ] Cancelacin en fracciones algebraicas
- [ ] Integral definida numrica de alta precisin (Gauss-Legendre)
- [ ] Visualizacin: rea sombreada bajo la curva en graficadora
- [ ] Series de Taylor / Maclaurin

### 7.2 Matrices
- [ ] Editor de matrices mn con navegacin 2D en pantalla
- [ ] Operaciones: suma, resta, multiplicacin, transpuesta
- [ ] Determinante 22 y 33
- [ ] Inversa por Gauss-Jordan
- [ ] Resolucin del sistema Ax = b por matrices
- [ ] Valores propios para matrices 22

### 7.3 Nmeros Complejos
- [ ] Modo complejo activable en Settings
- [ ] Entrada forma rectangular (a+bi) y polar (r??)
- [ ] Operaciones bsicas: +, -, , , conjugado, mdulo, argumento
- [ ] Plano de Argand (visualizacin grfica en GrapherApp)

### 7.4 lgebra de Bases y Conversin de Unidades
- [ ] Conversor DEC / HEX / BIN / OCT integrado
- [ ] Operaciones aritmticas en base arbitraria n
- [ ] Conversor de unidades: longitud, masa, temperatura, velocidad, energa

---

## Fase 8  Hardware Final + Conectividad + Scripting (Planificado)

&gt; *Objetivo: NumOS se convierte en un producto fsico completo, portable, autnomo y conectado.*

### 8.1 Teclado Fsico  Resolucin del Conflicto GPIO
- [ ] Reasignar ROW3 (GPIO 4) y ROW4 (GPIO 5) a GPIOs libres (propuesta: GPIO 15, 16)
- [ ] Actualizar `Config.h` con nuevas asignaciones
- [ ] Integrar `KeyMatrix::scan()` ? `LvglKeypad::pushKey()` en tiempo real
- [ ] Test completo de 48 teclas con `HardwareTest.cpp`
- [ ] Overlay visual SHIFT/ALPHA en barra de estado (capa activa)
- [ ] Funciones secundarias (teclas amarillas/rojas) mapeadas a `KeyCode::SHIFT_X`

### 8.2 PCB Propia
- [ ] Esquemtico completo en KiCad con ESP32-S3 WROOM integrado
- [ ] Conector FPC/ZIF para pantalla ILI9341
- [ ] Conector JST-PH 2 pines para batera LiPo
- [ ] TP4056 con carga USB-C + boost converter MT3608 (3.7V ? 5V)
- [ ] Test points JTAG + SWD para depuracin
- [ ] Trazado PCB 2 capas, form factor calculadora (85165mm)

### 8.3 Batera y Gestin de Energa
- [ ] LiPo 10002000 mAh (segn volumen del chasis)
- [ ] TP4056: carga va USB-C, indicador LED
- [ ] Monitor nivel batera por ADC (GPIO libre) con conversin al porcentaje
- [ ] Indicador grfico en barra de estado (icono batera animado)
- [ ] Modo bajo consumo: reducir LVGL refresh rate + bajar frecuencia CPU
- [ ] Deep sleep con wake-up por tecla ON/OFF dedicada

### 8.4 Carcasa 3D
- [ ] Diseo en FreeCAD o Fusion 360 siguiendo dimensiones de `DIMENSIONES_DISEO.md`
- [ ] Material: PLA o PETG, colores negro mate / gris
- [ ] Ventana de pantalla con bisel y proteccin de cristal acrlico
- [ ] Soporte para membrana de teclado o botones tctiles
- [ ] Tapa trasera con tornillos M2 y compartimento batera accesible

### 8.5 Conectividad WiFi
- [ ] **WebBridge**: servidor HTTP embebido en el ESP32-S3 para transferir programas/scripts desde el PC
- [ ] **OTA (Over The Air)**: actualizacin de firmware sin cables desde el navegador
- [ ] **Sincronizacin de variables**: backup y restore de `/vars.dat` va WiFi
- [ ] **NTP**: sincronizacin de hora para reloj interno y registro de tiempo

### 8.6 Scripting
- [ ] **Lua embebido** (eLua / LittleLua)  lenguaje de scripting para programar la calculadora
- [ ] Editor de scripts en pantalla con cursor 2D y resaltado de sintaxis bsico
- [ ] Acceso a todas las funciones matemticas desde Lua
- [ ] Scripts guardados/cargados desde LittleFS
- [ ] Ejecucin por lnea (`ENTER`) o bloque completo (`SHIFT+ENTER`)

### 8.7 Funciones Matemticas Adicionales
- [ ] `log2(x)`, `log?(x, b)`  logaritmo en base arbitraria
- [ ] `ceil`, `floor`, `round`, `frac`  redondeo
- [ ] `mod`  operador mdulo para enteros
- [ ] `gcd`, `lcm`  mximo comn divisor / mnimo comn mltiplo
- [ ] Proyeccin angular DEG?RAD?GRA directa en resultado
- [ ] `S`  Sumatorio sobre expresin (lmite inferior/superior configurable)
- [ ] `?`  Productorio
- [ ] `?`  Integral definida numrica de alta precisin

---

## Visin a Largo Plazo  La Mejor Calculadora Open-Source del Mundo

**NumOS aspira a demostrar que una calculadora open-source sobre hardware de 15  puede superar en funcionalidades a calculadoras comerciales de 180 .**

### Comparativa Objetivo

| Calculadora | Precio | CAS | Pasos | Python | Batera | Open Source |
|:------------|:------:|:---:|:-----:|:------:|:-------:|:-----------:|
| **NumOS (objetivo)** | **15 ** | ? | ? | ? | ? | ? MIT |
| Casio fx-991EX ClassWiz | 20  | ? | ? | ? | AAA | ? |
| NumWorks | 79  | ? | ? | ? | ? | ? MIT |
| TI-84 Plus CE | 119  | ? | ? | TI-BASIC | AAA | ? |
| HP Prime G2 | 179  | ? | ? | HP PPL | ? | ? |

### Diferencial nico de NumOS

1. **Pasos de resolucin**  Ninguna calculadora econmica muestra los pasos intermedios del CAS tan claramente.
2. **Open source total**  Todo el pipeline matemtico, desde el Tokenizer hasta el CAS, es auditable, modificable y educativo.
3. **Hardware a medida**  El proyecto controla todo: desde la PCB hasta el firmware.
4. **Coste 10x inferior**  15  de hardware para capacidades de una calculadora de 180 .
5. **Extensible en minutos**  Aadir una nueva app = ~100 lneas de C++17; nunca toca el core.
6. **Comunidad abierta**  Modelo de contribucin libre, no dependiente de ninguna empresa.

---

*NumOS  Open-source scientific calculator OS for ESP32-S3.*
*Cada commit es un paso hacia la mejor calculadora cientfica del mundo.*

*ltima actualizacin: Marzo 2026*
