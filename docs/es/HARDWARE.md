# NumOS — Hardware Reference

> **Plataforma**: ESP32-S3 N16R8 CAM · ILI9341 IPS 3.2" · Teclado 6×8
> **Build Stats (Feb 2026)**: RAM 29.0% (94 920 B / 327 680 B) · Flash 17.1% (1 118 121 B / 6 553 600 B)
>
> Referencia completa de hardware para NumOS. Cubre pinout, wiring, conflictos GPIO conocidos, bugs críticos resueltos, notas de bring-up y gestión de memoria (CAS-Lite PSRAM).
>
> **Última actualización**: Febrero 2026

---

## 1. Microcontrolador — ESP32-S3 N16R8 CAM

### Especificaciones Clave

| Parámetro | Valor |
|:----------|:------|
| **MCU** | ESP32-S3 (dual-core Xtensa LX7 @ 240 MHz) |
| **Flash** | 16 MB — Quad SPI (QIO) |
| **PSRAM** | 8 MB — Octal SPI (OPI) |
| **USB** | USB 1.1 Full-Speed nativo (CDC + JTAG) |
| **GPIO** | 45 pines (sin ADC solo-entrada como en ESP32 clásico) |
| **SPI** | SPI0 (Flash), SPI1 (interno), SPI2/FSPI (**disponible**), SPI3/GSPI |

### Configuración de memoria (CRÍTICA en PlatformIO)

```ini
board_build.arduino.memory_type = qio_opi   ; Flash QIO + PSRAM OPI
board_build.flash_mode          = qio        ; Modo de Flash
board_upload.flash_size         = 16MB
board_build.partitions          = default_16MB.csv
```

> ⚠️ **Si no se especifica `qio_opi`**, el ESP-IDF intenta inicializar la PSRAM en modo SPI estándar y el arranque termina en `Guru Meditation: Illegal Instruction` inmediatamente.

---

## 2. Pantalla — ILI9341 IPS TFT 3.2"

### Especificaciones

| Parámetro | Valor |
|:----------|:------|
| **Controlador** | ILI9341 |
| **Tipo de panel** | IPS (ángulos de visión amplios, sin distorsión de color) |
| **Resolución** | 320 × 240 px (landscape) |
| **Color** | 16 bpp (RGB565) |
| **Interface** | SPI (4 hilos: MOSI, SCLK, CS, DC) |
| **Frecuencia SPI** | 10 MHz (reducida para estabilidad en protoboard) |
| **Bus SPI** | FSPI (SPI_PORT=2) — CRÍTICO: ver Fix ② |
| **Alimentación** | 3.3V lógica, BL hardwired a 3.3V |

### Pinout ESP32-S3 ↔ ILI9341

| Señal TFT | GPIO ESP32-S3 | Notas |
|:----------|:-------------:|:------|
| VCC | 3V3 | Alimentación lógica 3.3V |
| GND | GND | Tierra común |
| MOSI / SDI | **13** | SPI Data In |
| SCLK / SCK | **12** | SPI Clock |
| CS | **10** | Chip Select (activo LOW) |
| DC / RS | **4** | Data/Command selector |
| RST | **5** | Reset (activo LOW, pulso 50 ms) |
| BL / LED | **45** → 3.3V | Backlight — **cableado fijo a 3.3V** |
| MISO / SDO | — | No conectado (sin touch) |

> ⚠️ **GPIO 45 (BL)**: En esta build, el pin de backlight está físicamente conectado a 3.3V. El código **siempre** hace `pinMode(45, INPUT)` para que el pin sea alto-impedancia. Si se pusiera en `OUTPUT LOW`, cortocircuitaría la alimentación y la pantalla dejaría de funcionar.

> 🚨 **CONFLICTO CRÍTICO — GPIO 4 y GPIO 5**:
> - `GPIO 4` = `TFT_DC` (Data/Command del display) = `ROW3` (fila 3 del teclado físico)
> - `GPIO 5` = `TFT_RST` (Reset del display) = `ROW4` (fila 4 del teclado físico)
>
> **Consecuencia**: Conectar el teclado físico sin resolver este conflicto producirá:
> 1. Pulsaciones de teclas que modifican DC/RST del display → corrupción visual o reset no deseado de la pantalla.
> 2. La inicialización del display puede activar filas del teclado involuntariamente.
>
> **Resolución propuesta antes de soldar**:
> - Reasignar `ROW3` a **GPIO 15** y `ROW4` a **GPIO 16** (libres y compatibles con matrix scan).
> - Actualizar `Config.h`: `constexpr int KEY_ROWS[] = {21, 47, 48, 15, 16, 38};`
> - Verificar que GPIO 15/16 no están en uso por PSRAM OPI (en N16R8 los GPIOs OPI son 35–37 internos — 15/16 son libres).
> - **No soldar el teclado físico** hasta confirmar la nueva asignación en Wokwi con SerialBridge.

### Inicialización TFT (secuencia exacta en código)

```cpp
_tft.init();                    // FSPI @ 10 MHz
_tft.invertDisplay(true);       // NECESARIO para panel IPS — sin esto los colores son invertidos
_tft.setRotation(1);            // Landscape: 320 ancho × 240 alto
pinMode(PIN_TFT_BL, INPUT);     // BL hardwired a 3.3V — nunca OUTPUT
_tft.fillScreen(0x0000);        // Limpiar GRAM con negro
```

### Gestión de Memoria — CAS-Lite y PSRAM

El **CAS-Lite Engine** (EquationsApp) almacena todos los datos simbólicos en PSRAM:
- `SymPoly::CoeffMap` → `std::map` con `PSRAMAllocator` → `ps_malloc`
- `CASStepLogger::StepVec` → `std::vector` con `PSRAMAllocator` → `ps_malloc`
- Los buffers de LVGL y DMA permanecen en RAM interna (`MALLOC_CAP_DMA | MALLOC_CAP_8BIT`)

> ✅ **El CAS-Lite NO interfiere con los buffers de display.** La PSRAM es compartida por:
> - El heap de PSRAM general (Arduino `ps_malloc`)
> - El CAS-Lite Engine (via `PSRAMAllocator`)
> Los buffers DMA del ILI9341 usan **exclusivamente RAM interna** — separación garantizada.

---

### La razón de `USE_FSPI_PORT` (Fix Crítico ②)

El ESP-IDF define `REG_SPI_BASE(i)` así:
```c
// esp-idf/components/soc/esp32s3/include/soc/soc.h
#define REG_SPI_BASE(i) (((i)>1) ? (DR_REG_SPI2_BASE + (i-2)*0x1000) : 0)
```

Si `SPI_PORT = 0` (default de TFT_eSPI para FSPI), entonces `REG_SPI_BASE(0) = 0`. Cualquier acceso a registros SPI se convierte en una escritura a la dirección `0x10` → **StoreProhibited / Guru Meditation**.

La flag `-DUSE_FSPI_PORT` fuerza `SPI_PORT = 2` en TFT_eSPI:
```
REG_SPI_BASE(2) = DR_REG_SPI2_BASE = 0x60024000  ✓
```

**Sin esta flag, la placa crashea en `TFT_eSPI::begin_tft_write()` siempre.**

---

## 3. Buffers de Render LVGL (Fix Crítico ④)

```cpp
// ✅ CORRECTO — heap interno, DMA-capable
void* buf1 = heap_caps_malloc(6400, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
void* buf2 = heap_caps_malloc(6400, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

// ❌ INCORRECTO — PSRAM no es DMA-capable para SPI en ESP32-S3
void* buf1 = ps_malloc(6400);  // Silenciosamente falla: pantalla negra
```

Los 6 400 bytes corresponden a **10 líneas × 320 px × 2 bytes (RGB565)**. Con `LV_DISPLAY_RENDER_MODE_PARTIAL`, LVGL renderiza en trozos de 10 líneas hasta completar la pantalla.

---

## 4. Teclado — Matriz 6×8

### Especificaciones

| Parámetro | Valor |
|:----------|:------|
| **Configuración** | 6 filas (entrada) × 8 columnas (salida) = 48 teclas |
| **Filas** | INPUT_PULLUP — no necesitan resistencias externas |
| **Columnas** | OUTPUT, puestas en LOW durante escaneo |
| **Debounce** | 20 ms por firmware |
| **Autorepeat** | Delay 500 ms, rate 80 ms |

### Pinout del Teclado (ESP32-S3)

| Señal | GPIO | Dirección | Nota |
|:------|:----:|:---------:|:-----|
| **R0** | 1 | INPUT_PULLUP | Fila 0 (superior) |
| **R1** | 2 | INPUT_PULLUP | Fila 1 |
| **R2** | 3 | INPUT_PULLUP | Fila 2 |
| **R3** | 4 | INPUT_PULLUP | ⚠️ Coincide con TFT DC |
| **R4** | 5 | INPUT_PULLUP | ⚠️ Coincide con TFT RST |
| **R5** | 6 | INPUT_PULLUP | Fila 5 (inferior) |
| **C0** | 38 | OUTPUT | Columna 0 |
| **C1** | 39 | OUTPUT | Columna 1 |
| **C2** | 40 | OUTPUT | Columna 2 |
| **C3** | 41 | OUTPUT | Columna 3 |
| **C4** | 42 | OUTPUT | Columna 4 |
| **C5** | 47 | OUTPUT | Columna 5 |
| **C6** | 48 | OUTPUT | Columna 6 |
| **C7** | 21 | OUTPUT | Columna 7 |

> ⚠️ **Nota crítica**: GPIO 4 y 5 están en uso por el TFT (DC y RST). Antes de conectar el teclado físico, reasignar R3 y R4 a pines libres y actualizar `Config.h`.

### Layout de Teclas

#### Fila R0 — Modos y Softkeys
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| SHIFT | ALPHA | MODE | SETUP | F1 | F2 | F3 | F4 |

#### Fila R1 — Control Global y Navegación
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| ON | AC | DEL | FREE_EQ | LEFT | UP | DOWN | RIGHT |

#### Fila R2 — Graficadora y Funciones Avanzadas
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| X | Y= | TABLE | GRAPH | ZOOM | TRACE | STEPS | SOLVE |

#### Fila R3 — Fila Superior Numérica + Operadores
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 7 | 8 | 9 | ( | ) | ÷ | ^ | √ |

#### Fila R4 — Fila Media Numérica + Trig
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 4 | 5 | 6 | × | − | sin | cos | tan |

#### Fila R5 — Fila Inferior Numérica + Enter
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 1 | 2 | 3 | + | (−) | 0 | . | ENTER |

### Algoritmo de Escaneo

```cpp
// En KeyMatrix::scan() — ejecutado cada 5 ms
for (int col = 0; col < KEY_COLS; col++) {
    digitalWrite(colPins[col], LOW);   // Activar columna
    delayMicroseconds(2);              // Dejamos estabilizar el nivel
    for (int row = 0; row < KEY_ROWS; row++) {
        bool pressed = (digitalRead(rowPins[row]) == LOW);
        // Procesar debounce y autorepeat...
    }
    digitalWrite(colPins[col], HIGH);  // Desactivar columna
}
```

---

## 5. Teclado Virtual (SerialBridge)

Para pruebas sin teclado físico, la calculadora puede controlarse desde el Serial Monitor del PC:

```
[SerialBridge] Controles:
  w/a/s/d    = Flechas (↑←↓→)
  z          = ENTER / Confirmar
  x          = DEL / Borrar
  c          = AC / Limpiar todo
  h          = HOME / Mode
  0-9        = Dígitos
  +-*/^.()   = Operadores
  S          = SHIFT (mayúscula)
  f          = FRACCIÓN (SHIFT+DIV)
  r          = √ SQRT
  R          = ⁿ√ nth-ROOT (SHIFT+SQRT)
  g          = GRAPH
  t          = SIN
  y          = VAR_Y
```

> **Importante**: `s` minúscula = DOWN. `S` mayúscula = SHIFT. El monitor envía `\r\n` al pulsar Enter, que es **ignorado** (usa `z` para ENTER). Asegúrate de tener `monitor_rts=0` y `monitor_dtr=0` en platformio.ini.

---

## 6. USB Serial — Procedimiento de Conexión

El ESP32-S3 usa su USB CDC nativo. Después de flashear:

1. **Espera ~3 segundos** tras el upload.
2. Abre el Serial Monitor con `monitor_rts=0`.
3. Deberías ver:
   ```
   === NumOS Boot ===
   [PSRAM] 7680 KB libres
   [TFT] OK
   [BOOT] OK — Escribe 'w' y pulsa Enter para probar.
   [HB] 5s uptime | heap=XXXXX
   ```
4. Si ves `[HB]` periódico, el Serial TX funciona.
5. Si ves `[SB] RX: 'w' (0x77)` al escribir, el Serial RX también funciona.

---

## 7. Batería (Opcional)

Para uso portable:

| Componente | Especificación |
|:-----------|:--------------|
| **Celda** | Li-Ion 18650 (3.7V nominal) |
| **Cargador** | TP4056 con protección de batería |
| **Regulador** | Boost MT3608: 3.7V → 5V para alimentar por VIN |
| **Monitor** | Divisor resistivo 100kΩ+100kΩ → GPIO analógico (no implementado aún) |

> La implementación del monitor de batería en software está pendiente (Fase 5).

---

## 8. Resumen de Fixes Críticos

| # | Fix | Síntoma sin el fix | Solución |
|:-:|:----|:-------------------|:---------|
| ① | Flash OPI | `Illegal Instruction` al boot | `memory_type = qio_opi` |
| ② | `USE_FSPI_PORT` | `StoreProhibited` en `TFT_eSPI::begin` | `-DUSE_FSPI_PORT` en build_flags |
| ③ | SPI 10 MHz | Líneas/ruido display en protoboard | `SPI_FREQUENCY=10000000` |
| ④ | DMA buffers | Pantalla negra, flush no pinta | `heap_caps_malloc(MALLOC_CAP_DMA\|MALLOC_CAP_8BIT)` |
| ⑤ | GPIO 45 INPUT | Pantalla muere al encender BL | `pinMode(45, INPUT)` — nunca `OUTPUT LOW` |
| ⑥ | Serial CDC wait | Output perdido, monitor desconectado | `while(!Serial)` + `monitor_rts=0` / `monitor_dtr=0` |


Este documento describe el wiring recomendado y la matriz de teclas para la calculadora basada en ESP32 con pantalla TFT de 3.2" (ILI9341).

## MCU

- **Placa**: ESP32 DevKit V1 (38 pines).
- **Alimentación**:
  - Entrada típica por USB (5V).
  - El regulador del DevKit genera 3.3V para la lógica.
  - Opcional: batería Li‑Ion + módulo de carga (p.ej. TP4056) y booster a 5V si se desea portabilidad.

## Pantalla TFT 3.2" (ILI9341, SPI)

Conexión recomendada (coincide con `src/Config.h` y debe alinearse con la configuración de `TFT_eSPI`):

| ESP32        | TFT ILI9341 | Descripción                          |
|--------------|-------------|--------------------------------------|
| 3V3          | VCC         | Alimentación lógica 3.3V            |
| GND          | GND         | Tierra común                         |
| GPIO23       | SDI/MOSI    | Datos SPI hacia el TFT               |
| GPIO18       | SCK         | Reloj SPI                            |
| GPIO5        | CS          | Chip Select de la pantalla           |
| GPIO16       | DC/RS       | Data/Command                         |
| GPIO17       | RST         | Reset de la pantalla                 |
| GPIO4 (PWM)  | LED/BL      | Backlight (vía transistor + R serie) |
| (opcional)19 | SDO/MISO    | Lectura SPI desde el TFT             |

> Nota: El backlight se recomienda controlar mediante un transistor NPN o MOSFET canal N, con resistencia en la base/puerta y, si es necesario, resistencia limitadora de corriente en serie con el LED.

## Matriz de teclas 6x8

La calculadora utiliza una matriz de **6 filas x 8 columnas** (48 teclas).  
Las columnas se configuran como **salidas**, y las filas como **entradas con `INPUT_PULLUP` interno**. Se han evitado los GPIO 34, 35, 36 y 39 (solo entrada y sin pull-up) para no requerir resistencias externas.

### Pines ESP32 ↔ Matriz

| Fila/Columna | GPIO ESP32 | Dirección | Comentario                                                                 |
|--------------|-----------:|----------|---------------------------------------------------------------------------|
| R0           | GPIO32     | Entrada  | Fila 0 (superior), con `INPUT_PULLUP` interno                             |
| R1           | GPIO33     | Entrada  | Fila 1, con `INPUT_PULLUP` interno                                        |
| R2           | GPIO25     | Entrada  | Fila 2, con `INPUT_PULLUP` interno                                        |
| R3           | GPIO26     | Entrada  | Fila 3, con `INPUT_PULLUP` interno                                        |
| R4           | GPIO27     | Entrada  | Fila 4, con `INPUT_PULLUP` interno                                        |
| R5           | GPIO14     | Entrada  | Fila 5 (inferior), con `INPUT_PULLUP` interno                             |
| C0           | GPIO13     | Salida   | Columna 0                                                                 |
| C1           | GPIO19     | Salida   | Columna 1                                                                 |
| C2           | GPIO21     | Salida   | Columna 2                                                                 |
| C3           | GPIO22     | Salida   | Columna 3                                                                 |
| C4           | GPIO0      | Salida   | Columna 4 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |
| C5           | GPIO2      | Salida   | Columna 5 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |
| C6           | GPIO12     | Salida   | Columna 6 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |
| C7           | GPIO15     | Salida   | Columna 7 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |

### Layout lógico de teclas

La disposición se inspira en calculadoras TI‑84/Casio FX, aprovechando teclas de `SHIFT` y `ALPHA` para segundas funciones.

#### Fila R0 (superior) – Modos y softkeys

| Columna | Tecla   |
|---------|---------|
| C0      | SHIFT   |
| C1      | ALPHA   |
| C2      | MODE    |
| C3      | SETUP   |
| C4      | F1      |
| C5      | F2      |
| C6      | F3      |
| C7      | F4      |

#### Fila R1 – Control global y navegación

| Columna | Tecla                 |
|---------|-----------------------|
| C0      | ON                    |
| C1      | AC (Clear All)       |
| C2      | DEL                   |
| C3      | Free Eq (toggle)     |
| C4      | LEFT                  |
| C5      | UP                    |
| C6      | DOWN                  |
| C7      | RIGHT / F5           |

#### Fila R2 – Graficadora y solver

| Columna | Tecla          |
|---------|----------------|
| C0      | X (variable)   |
| C1      | Y=/f(x)        |
| C2      | TABLE          |
| C3      | GRAPH          |
| C4      | ZOOM           |
| C5      | TRACE          |
| C6      | SHOW STEPS     |
| C7      | SOLVE          |

#### Fila R3 – Paréntesis y operaciones altas

| Columna | Tecla                 |
|---------|-----------------------|
| C0      | 7                     |
| C1      | 8                     |
| C2      | 9                     |
| C3      | (                     |
| C4      | )                     |
| C5      | ÷                     |
| C6      | ^ (potencia)         |
| C7      | √ (SHIFT: x²)        |

#### Fila R4 – Operaciones y trigonometría

| Columna | Tecla   |
|---------|---------|
| C0      | 4       |
| C1      | 5       |
| C2      | 6       |
| C3      | ×       |
| C4      | −       |
| C5      | sin     |
| C6      | cos     |
| C7      | tan     |

#### Fila R5 (inferior) – Numérico base

| Columna | Tecla        |
|---------|--------------|
| C0      | 1            |
| C1      | 2            |
| C2      | 3            |
| C3      | +            |
| C4      | (−) cambio signo |
| C5      | 0            |
| C6      | .            |
| C7      | ENTER (=)    |

### Escaneo de la matriz

Estrategia de escaneo recomendada:

1. Configurar todas las columnas a estado HIGH (o HIGH‑Z cuando no estén activas).
2. Para cada columna C_i:
   - Poner C_i en LOW.
   - Leer el estado de todas las filas R_j (con `INPUT_PULLUP` interno).
   - Si una fila lee LOW, la tecla (R_j, C_i) está pulsada.
   - Restaurar C_i a HIGH antes de pasar a la siguiente.

Con esto se evitan conflictos y se puede implementar debounce y autorepetición en firmware.  
Si en el futuro se requiere soporte de múltiples teclas simultáneas complejas, se pueden añadir diodos por tecla para evitar “ghosting”.

## Consideraciones de carcasa 3D

- Pantalla inclinada aproximadamente **10–15°** respecto a la base para mejor ergonomía visual.
- Uso de pulsadores táctiles de **6x6mm** con capuchones impresos para un tacto similar al de calculadoras comerciales.
- Dejar espacio suficiente entre filas/columnas de teclas para una escritura cómoda (similar a TI‑84/FX‑570).
- Prever volumen interno para la PCB de la matriz de teclas, la placa ESP32 y, si se usa, una batería Li‑Ion.

