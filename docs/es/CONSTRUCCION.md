# 🔨 NumOS — Guía de Construcción y Test de Hardware

&gt; Esta guía cubre todo el proceso de ensamblado: conexionado, firmware, impresión 3D y verificación de hardware. Está actualizada para la plataforma **ESP32-S3 N16R8 CAM** con pantalla **ILI9341 IPS 3.2"**.

---

## 1. Lista de Materiales (BOM)

| Cantidad | Componente | Especificación | Notas |
|:--------:|:-----------|:--------------|:------|
| 1 | ESP32-S3 N16R8 CAM | 16 MB Flash QIO + 8 MB PSRAM OPI | Módulo con antena integrada |
| 1 | Pantalla TFT IPS | 3.2" ILI9341 320×240 SPI | Panel IPS (no TN) — invertDisplay=true |
| 48 | Pulsadores táctiles | 6×6 mm, altura 5 mm | Con capuchón impreso 3D |
| 1 | Protoboard / PCB | 400 puntos o más | O PCB custom (Fase 5) |
| — | Cables jumper | Macho-Macho y Macho-Hembra | 10–15 cm para SPI display |
| 1 | Cable USB-C | Para programación y alimentación | USB-C (S3 tiene USB-C nativo) |
| 1 | *(Opcional)* Batería 18650 | Li-Ion 3.7V ~2000 mAh | |
| 1 | *(Opcional)* Módulo TP4056 | Con protección de batería | |
| 1 | *(Opcional)* Boost MT3608 | 3.7V → 5V, hasta 2A | Para alimentar ESP32 por VIN |

---

## 2. Diagrama de Conexionado Completo

### 2.1 Pantalla ILI9341 → ESP32-S3 N16R8 CAM

```
ESP32-S3          ILI9341 3.2" TFT
  3.3V  ──────────  VCC
  GND   ──────────  GND
  G13   ──────────  SDI / MOSI
  G12   ──────────  SCK
  G10   ──────────  CS
  G4    ──────────  DC / RS
  G5    ──────────  RST
  3.3V  ──────────  LED / BL   ← Cableado directo a 3.3V (sin transistor)
  (NC)             SDO / MISO  ← No conectado (sin touch)
```

&gt; **Nota BL**: El pin de backlight está cableado directamente a 3.3V en esta build. En el código, GPIO 45 se configura como `INPUT` (alta impedancia). Si en el futuro se quiere control de brillo por PWM, usar un transistor MOSFET N-canal entre GND del LED y GND, con la gate controlada por GPIO.

### 2.2 Matriz de Teclado 6×8 → ESP32-S3

```
ESP32-S3  Señal    Dirección
  G1    ─  R0    ─  INPUT_PULLUP
  G2    ─  R1    ─  INPUT_PULLUP
  G3    ─  R2    ─  INPUT_PULLUP
  G4    ─  R3    ─  INPUT_PULLUP  ⚠️ Conflicto con TFT DC
  G5    ─  R4    ─  INPUT_PULLUP  ⚠️ Conflicto con TFT RST
  G6    ─  R5    ─  INPUT_PULLUP

  G38   ─  C0    ─  OUTPUT
  G39   ─  C1    ─  OUTPUT
  G40   ─  C2    ─  OUTPUT
  G41   ─  C3    ─  OUTPUT
  G42   ─  C4    ─  OUTPUT
  G47   ─  C5    ─  OUTPUT
  G48   ─  C6    ─  OUTPUT
  G21   ─  C7    ─  OUTPUT
```

&gt; ⚠️ **Conflicto de pines R3/R4**: Los GPIO 4 y 5 están asignados tanto al TFT (DC y RST) como a las filas R3 y R4 del teclado. Esto no puede funcionar simultáneamente. **Antes de soldar el teclado**, actualiza `Config.h` para asignar R3 y R4 a pines libres (ej. GPIO 7, 8, o cualquier otro no utilizado).

---

## 3. Proceso de Flasheo del Firmware

### Prerrequisitos
- [PlatformIO](https://platformio.org/install/ide?install=vscode) instalado como extensión de VS Code.
- Driver USB del ESP32-S3 (en Windows 11+, el driver CDC viene incluido).
- Cable USB-C.

### Pasos

```bash
# 1. Clonar el repositorio
git clone https://github.com/tu-usuario/numOS.git
cd numOS

# 2. Compilar solo (sin flashear)
C:\Python314\Scripts\pio.exe run -e esp32s3_n16r8

# 3. Compilar y flashear
C:\Python314\Scripts\pio.exe run -e esp32s3_n16r8 --target upload

# 4. Abrir Serial Monitor
C:\Python314\Scripts\pio.exe device monitor
```

O desde la interfaz de VS Code: barra inferior → ícono de Upload (→) o Monitor (🔌).

### Salida esperada en Serial Monitor tras un buen flash

```
=== NumOS Boot ===
[PSRAM] 7680 KB libres
[TFT] OK
[BOOT] OK — Escribe 'w' y pulsa Enter para probar.
[BOOT] Si ves este mensaje, Serial TX funciona.
[SerialBridge] PC keyboard bridge active.
[SerialBridge] Controles (minusculas!):
  w/a/s/d = Flechas    z = OK/Enter
  ...
[HB] 5s uptime | heap=245760
```

---

## 4. Test de Hardware

### 4.1 Test de Pantalla

El splash screen animado valida automáticamente:
- ✅ TFT_eSPI inicializa sin crash
- ✅ LVGL renderiza correctamente (si el logo "NumOS" aparece con fade-in)
- ✅ Flush DMA funciona (si los píxeles llegan a la pantalla)

Si la pantalla se queda **negra** tras el flash:
1. Verifica que ves output en Serial Monitor (si no → problema de Serial CDC, ver sección 5).
2. Revisa que los pines MOSI/SCK/CS/DC/RST estén bien cableados.
3. Confirma que el SPI es FSPI: `-DUSE_FSPI_PORT` debe estar en `build_flags`.
4. Comprueba que los buffers DMA son `heap_caps_malloc(MALLOC_CAP_DMA)` y NO `ps_malloc`.

### 4.2 Test del Teclado (Firmware dedicado)

Para verificar la matriz física antes de integrar con NumOS:

1. En `platformio.ini`, cambia temporalmente:
   ```ini
   build_src_filter = +<HardwareTest.cpp>
   ```
2. Flashea.
3. La pantalla mostrará la fila y columna de cada tecla que presiones.
4. Comprueba las 48 teclas una por una.
5. Si una tecla no responde:
   - Mide continuidad con multímetro entre el punto de unión fila-tecla-columna.
   - Verifica que el pulsador hace contacto (prueba con una punta).
   - Revisa los jumpers de esa fila/columna específicos.

### 4.3 Test de SerialBridge

Con el firmware de producción cargado:

1. Abre Serial Monitor (115 200 baud, `monitor_rts=0`).
2. Espera a ver `[HB]` — confirma que el loop corre y Serial TX funciona.
3. Escribe `w` y pulsa Enter → deberías ver:
   ```
   [SB] RX: 'w' (0x77)
   [Key] PC Input: 'UP' -> Action: UP
   ```
4. Esto confirma que el ESP32-S3 recibe datos por Serial RX.
5. Navega el launcher con `w`/`a`/`s`/`d` y entra a apps con `z`.

---

## 5. Solución de Problemas Comunes

### Serial Monitor no muestra nada

| Causa | Solución |
|:------|:---------|
| DTR/RTS reseteando la placa al abrir el Monitor | Añadir `monitor_rts=0` y `monitor_dtr=0` en `platformio.ini` |
| USB CDC no enumerado aún | El código tiene `while(!Serial && millis()-t0 < 3000)` — espera 3 s tras reset |
| Velocidad incorrecta | Confirmar 115 200 baud en Monitor |
| Driver no instalado | Instalar driver Zadig (Windows) o usar Windows 11 que lo incluye de serie |

### Pantalla negra después del flash

| Causa | Solución |
|:------|:---------|
| PSRAM OPI no configurado | `board_build.arduino.memory_type = qio_opi` |
| SPI_PORT=0 → crash | `-DUSE_FSPI_PORT` en `build_flags` |
| Buffers en PSRAM | Cambiar `ps_malloc` por `heap_caps_malloc(MALLOC_CAP_DMA\|MALLOC_CAP_8BIT)` |
| Pin BL en OUTPUT LOW | `pinMode(45, INPUT)` — nunca `OUTPUT` |
| SPI muy rápido | `SPI_FREQUENCY=10000000` (10 MHz) |

### ESP32-S3 no entra en modo flash

Mantener `BOOT` (GPIO 0) pulsado → pulsar `RESET` → soltar `BOOT`. Libera GPIO 0 antes del siguiente arranque.

---

## 6. Impresión 3D del Chasis

### Parámetros Recomendados (Impresora Ender CR6 SE u similar)

| Parámetro | Valor |
|:----------|:------|
| **Material** | PLA+ o PETG |
| **Altura de capa** | 0.2 mm (carcasa), 0.12 mm (teclas — más detalle) |
| **Relleno** | 20% carcasa, 100% teclas |
| **Velocidad** | 50 mm/s carcasa, 30 mm/s teclas |
| **Soportes** | Solo en vanos de puertos USB/SD si no se diseñan con puentes |
| **Perímetros** | 3 (para paredes robustas) |

### Tolerancias para Teclas

- Teclas de 6.0 mm × 6.0 mm → hueco en carcasa de **6.4 mm × 6.4 mm** (tolerancia 0.4 mm por lado).
- Imprime 4-5 teclas de prueba antes de imprimir la carcasa completa.
- Altura recomendada del vástago: **3 mm** + altura del capuchón impreso.
- Usa pulsadores táctiles de **6×6 mm, altura total 7 mm** (5 mm cuerpo + 2 mm recorrido).

### Consejos de Diseño

- Inclinar la zona de pantalla **10-15°** respecto a la base mejora la ergonomía visual.
- Prever **insertos roscados M3×4** en las 4 esquinas para cierre seguro sin pegamento.
- Dejar hueco de **12 mm × 8 mm** para el conector USB-C del ESP32-S3.
- Añadir pilares de soporte internos para el módulo ESP32-S3 (no tiene agujeros de montaje estándar — usar snap-fit o clips laterales).
- Si incluyes batería 18650: agujero de **18.5 mm diámetro × 70 mm longitud**, con tapa deslizante trasera fijada con tornillo M3.

---

## 7. Recomendaciones de PCB (Fase 5)

Para la versión final del hardware, en lugar de protoboard:

1. **Diseño con KiCad**: Esquemático y layout, exportar Gerbers.
2. **2 capas**: Top (señales) + Bottom (plano de tierra continuo).
3. **Condensadores de desacoplo**: 100 nF cerámico por píh de alimentación del ESP32-S3.
4. **Conector de cámara**: El módulo ESP32-S3 CAM tiene conector de cámara OV2640 — se puede usar o recubrir.
5. **Conector de programación**: Mantener acceso al USB-C o añadir pad de programación UART.
6. **Protección ESD**: TVS diodo en la línea USB-C.
7. **Fabricación**: JLCPCB, PCBWay o similar (2L, 1.6 mm FR4, 1 oz cobre).

---

*Última actualización: Febrero 2026*


Este documento detalla el proceso de construcción de la Calculadora Graficadora ESP32.

## 1. Diagrama de Conexionado (Wiring)

### MCU: ESP32 DevKit V1 (38 pines)
La alimentación principal entra por el puerto USB (5V) o por el pin VIN (5V-9V) si usas batería externa + booster.

### Pantalla TFT 3.2" SPI (ILI9341)

| Pin TFT | Pin ESP32 | Función | Notas |
| :--- | :--- | :--- | :--- |
| VCC | 3V3 | Alimentación | Usar la salida de 3.3V del ESP32 |
| GND | GND | Tierra | Común con ESP32 |
| CS | GPIO 5 | Chip Select | |
| RESET | GPIO 17 | Reset | |
| DC/RS | GPIO 16 | Data/Command | |
| SDI/MOSI | GPIO 23 | Data In | SPI MOSI |
| SCK | GPIO 18 | Clock | SPI SCK |
| LED | GPIO 4 | Backlight | Se recomienda usar transistor (NPN o MOSFET) |
| SDO/MISO | GPIO 19 | Data Out | Opcional (para lectura/touch) |

### Matriz de Teclado 6x8

La matriz consta de **6 filas** (entradas con pull-up interno) y **8 columnas** (salidas).
**NO SE REQUIEREN RESISTENCIAS EXTERNAS** para las filas, ya que se activan las pull-ups internas del ESP32 (`INPUT_PULLUP`).

#### Mapeo de Pines

| Fila (Input) | GPIO ESP32 | | Columna (Output) | GPIO ESP32 |
| :--- | :--- | :--- | :--- | :--- |
| **R0** | GPIO 32 | | **C0** | GPIO 13 |
| **R1** | GPIO 33 | | **C1** | GPIO 19 |
| **R2** | GPIO 25 | | **C2** | GPIO 21 |
| **R3** | GPIO 26 | | **C3** | GPIO 22 |
| **R4** | GPIO 27 | | **C4** | GPIO 0 (*) |
| **R5** | GPIO 14 | | **C5** | GPIO 2 (*) |
| | | | **C6** | GPIO 12 (*) |
| | | | **C7** | GPIO 15 (*) |

*&gt; **ATENCIÓN**: Las columnas C4, C5, C6 y C7 (GPIO 0, 2, 12, 15) son pines de "strapping" que determinan el modo de arranque del ESP32. **NO MANTENER PULSADAS** teclas de estas columnas durante el encendido o reset, o el ESP32 podría entrar en modo bootloader/fallo.*

## 2. Instrucciones de Impresión 3D (Ender CR6 SE)

Para una carcasa robusta y teclas funcionales:

### Parámetros Generales
*   **Material**: PLA o PETG.
*   **Altura de capa**: 0.2mm (0.12mm para las teclas para mejor detalle).
*   **Relleno**: 20% para carcasa, 100% para teclas.
*   **Soportes**: Solo necesarios en huecos de puertos si no se diseñan con puentes.

### Tolerancias para Botones
*   Diseña los botones con una tolerancia de **0.4mm** respecto al hueco de la carcasa.
    *   *Ejemplo*: Si el botón mide 6.0mm x 6.0mm, el hueco en la carcasa debe ser de 6.4mm x 6.4mm.
    *   Esto asegura que las teclas no se atasquen y tengan un recorrido suave.
*   Usa **pulsadores táctiles de 6x6mm** de altura adecuada (ej. 5mm + altura de tecla impresa).
*   Imprime una **prueba de ajuste** con 4-5 teclas antes de imprimir la carcasa completa.

## 3. Test de Hardware

Para verificar soldaduras y conexiones, compila y sube el código de prueba `src/HardwareTest.cpp` (renómbralo temporalmente a `main.cpp` o ajusta `platformio.ini` para usarlo).

Este test mostrará en la pantalla TFT cada tecla que presiones (Fila, Columna y Código).
Si una tecla no responde:
1.  Revisa el cableado de esa fila/columna específica.
2.  Verifica con multímetro la continuidad.
3.  Asegúrate de que el diodo (si usaste) esté en la dirección correcta (cátodo a columna).

## 4. Batería (Opcional)

Si añades batería Li-Ion (3.7V):
*   Usa un módulo **TP4056** para carga.
*   Usa un módulo **Boost (MT3608)** para elevar a 5V y alimentar por el pin 5V (VIN), o alimenta directo a 3.3V (menos recomendado por estabilidad).
*   **Monitor de batería**: Conecta un divisor de tensión (ej. 100k + 100k) desde V_BAT a un pin analógico (ej. GPIO 35) para leer el nivel. (Actualmente no implementado en software).
