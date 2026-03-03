# 🔨 NumOS — Hardware Build and Test Guide

> This guide covers the entire assembly process: wiring, firmware flashing, 3D printing, and hardware verification. It is updated for the **ESP32-S3 N16R8 CAM** platform with a **3.2" ILI9341 IPS** display.

---

## 1. Bill of Materials (BOM)

| Quantity | Component | Specification | Notes |
|:--------:|:----------|:--------------|:------|
| 1 | ESP32-S3 N16R8 CAM | 16 MB Flash QIO + 8 MB PSRAM OPI | Module with integrated antenna |
| 1 | IPS TFT Display | 3.2" ILI9341 320×240 SPI | IPS panel (not TN) — invertDisplay=true |
| 48 | Tactile push buttons | 6×6 mm, 5 mm height | For 3D printed keycaps |
| 1 | Breadboard / PCB | 400 points or more | Or custom PCB (Phase 5) |
| — | Jumper wires | Male-to-Male and Male-to-Female | 10–15 cm for SPI display |
| 1 | USB-C cable | For programming and power | S3 has native USB-C |
| 1 | *(Optional)* 18650 Battery | Li-Ion 3.7V ~2000 mAh | |
| 1 | *(Optional)* TP4056 Module | With battery protection | |
| 1 | *(Optional)* MT3608 Boost | 3.7V → 5V, up to 2A | To power ESP32 via VIN |

---

## 2. Complete Wiring Diagram

### 2.1 ILI9341 Display → ESP32-S3 N16R8 CAM

```
ESP32-S3          ILI9341 3.2" TFT
  3.3V  ──────────  VCC
  GND   ──────────  GND
  G13   ──────────  SDI / MOSI
  G12   ──────────  SCK
  G10   ──────────  CS
  G4    ──────────  DC / RS
  G5    ──────────  RST
  3.3V  ──────────  LED / BL   ← Wired directly to 3.3V (no transistor)
  (NC)             SDO / MISO  ← Not connected (no touch support)
```

> **Note on BL**: The backlight pin is wired directly to 3.3V in this build. In code, GPIO 45 is set as `INPUT` (high impedance). If PWM brightness control is desired in the future, use an N-channel logic-level MOSFET between the LED GND and system GND, with the gate controlled by a GPIO.

### 2.2 6×8 Key Matrix → ESP32-S3

```
ESP32-S3  Signal    Direction
  G1    ─  R0    ─  INPUT_PULLUP
  G2    ─  R1    ─  INPUT_PULLUP
  G3    ─  R2    ─  INPUT_PULLUP
  G4    ─  R3    ─  INPUT_PULLUP  ⚠️ Conflict with TFT DC
  G5    ─  R4    ─  INPUT_PULLUP  ⚠️ Conflict with TFT RST
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

> ✅ **GPIO 4/5 conflict — RESOLVED (2026-03-02)**: GPIO 4 (`TFT_DC`) and GPIO 5 (`TFT_RST`) are no longer assigned to keyboard columns. The new `Keyboard` driver (Phase 7, `src/drivers/Keyboard.h`) uses GPIO 6, 7, 8 for the three currently wired columns. Safe to solder to these pins.

---

## 3. Firmware Flashing Process

### Prerequisites
- [PlatformIO](https://platformio.org/install/ide?install=vscode) installed as a VS Code extension.
- ESP32-S3 USB driver (Windows 11+ includes the CDC driver by default).
- USB-C cable.

### Steps

```bash
# 1. Clone the repository
git clone https://github.com/your-username/numOS.git
cd numOS

# 2. Build only (without flashing)
C:\Python314\Scripts\pio.exe run -e esp32s3_n16r8

# 3. Build and flash
C:\Python314\Scripts\pio.exe run -e esp32s3_n16r8 --target upload

# 4. Open Serial Monitor
C:\Python314\Scripts\pio.exe device monitor
```

Alternatively, from the VS Code UI: bottom toolbar → Upload (→) or Monitor (🔌) icon.

### Expected Serial Monitor Output Upon Successful Flash

```
=== NumOS Boot ===
[PSRAM] 7680 KB free
[TFT] OK
[BOOT] OK — Type 'w' and press Enter to test.
[BOOT] If you see this message, Serial TX works.
[SerialBridge] PC keyboard bridge active.
[SerialBridge] Controls (lowercase!):
  w/a/s/d = D-pad arrows    z = OK/Enter
  ...
[HB] 5s uptime | heap=245760
```

---

## 4. Hardware Verification

### 4.1 Display Test

The animated splash screen automatically validates:
- ✅ TFT_eSPI initialized without crashing
- ✅ LVGL correctly renders (fade-in "NumOS" logo appears)
- ✅ DMA flushing works (pixels are delivered to the screen)

If the screen remains **black** after flashing:
1. Ensure there is output in the Serial Monitor (if not → CDC Serial problem, refer to section 5).
2. Check that the MOSI/SCK/CS/DC/RST jumpers are securely and correctly wired.
3. Confirm FSPI usage: `-DUSE_FSPI_PORT` must be present in `build_flags`.
4. Verify that DMA buffers use `heap_caps_malloc(MALLOC_CAP_DMA)` and NOT `ps_malloc`.

### 4.2 Keyboard Test (Dedicated Firmware)

To verify the physical matrix prior to full NumOS integration:

1. In `platformio.ini`, temporarily modify the build filter:
   ```ini
   build_src_filter = +<HardwareTest.cpp>
   ```
2. Flash the firmware.
3. The screen will output the row and column of any pressed key.
4. Test all 48 keys individually.
5. If a key doesn't register:
   - Use a multimeter to check continuity at the row-to-key-to-column junctions.
   - Ensure the physical button creates contact when pressed.
   - Verify the jumpers associated with that specific row/column.

### 4.3 SerialBridge Test

With production firmware running:

1. Open the Serial Monitor (115,200 baud, `monitor_rts=0`).
2. Wait for `[HB]` — confirms the main loop is running and Serial TX is functional.
3. Type `w` and press Enter → you should observe:
   ```
   [SB] RX: 'w' (0x77)
   [Key] PC Input: 'UP' -> Action: UP
   ```
4. This confirms the ESP32-S3 receives packets via Serial RX.
5. Navigate the launcher using `w`/`a`/`s`/`d` and enter apps by pressing `z`.

---

## 5. Typical Troubleshooting

### Serial Monitor Displays Nothing

| Cause | Solution |
|:------|:---------|
| DTR/RTS resetting board upon Monitor opening | Include `monitor_rts=0` and `monitor_dtr=0` in `platformio.ini` |
| USB CDC not yet enumerated | Code utilizes `while(!Serial && millis()-t0 < 3000)` — waits 3s after reset |
| Incorrect baud rate | Ensure Monitor is set to 115,200 baud |
| Driver missing | Install Windows Zadig driver (Windows 11 includes it out of the box) |

### Screen Stays Black After Flash

| Cause | Solution |
|:------|:---------|
| PSRAM OPI improperly configured | Ensure `board_build.arduino.memory_type = qio_opi` is set |
| SPI_PORT=0 → null-pointer crash | Set `-DUSE_FSPI_PORT` inside `build_flags` |
| LVGL buffers allocated in PSRAM | Alter `ps_malloc` to `heap_caps_malloc(MALLOC_CAP_DMA\|MALLOC_CAP_8BIT)` |
| BL pin driven to OUTPUT LOW | Adjust to `pinMode(45, INPUT)` — avoid `OUTPUT` status |
| SPI frequency set too high | Lower to `SPI_FREQUENCY=10000000` (10 MHz) |

### ESP32-S3 Cannot Enter Flash Mode

Press and hold the `BOOT` button (GPIO 0) → press `RESET` → release `BOOT`. Unhold GPIO 0 entirely before the next boot cycle.

---

## 6. 3D Printed Enclosure Specifications

### Recommended Settings (e.g., Ender CR6 SE or similar)

| Parameter | Value |
|:----------|:------|
| **Material** | PLA+ or PETG |
| **Layer Height** | 0.2 mm (case), 0.12 mm (keycaps for detail) |
| **Infill** | 20% case, 100% keycaps |
| **Print Speed** | 50 mm/s case, 30 mm/s keycaps |
| **Supports** | Only on USB/SD port overhangs if not designed for bridging |
| **Perimeters** | 3 perimeters (dense/sturdy walls) |

### Button Array Tolerances

- 6.0 mm × 6.0 mm buttons → print a **6.4 mm × 6.4 mm** housing cavity (0.4 mm lateral tolerance).
- Print a sample test batch of 4-5 keys before committing to the full structure.
- Stem height suggestion: **3 mm** + printed cap height.
- Leverage tactile switches spec'd at **6×6 mm, 7 mm total height** (5 mm block + 2 mm stem travel).

### Design Recommendations

- Angling the screen housing **10-15°** backwards vastly improves ergonomic visibility.
- Include **M3×4 threaded heat-set inserts** in the 4 corners for tight closure without adhesives.
- Prepare a cutout of **12 mm × 8 mm** specifically for the ESP32-S3's USB-C physical housing.
- Add holding pillars or snap-fit clips for the ESP32-S3 board (it lacks native M2/M3 mounting holes).
- For a 18650 cell: model an **18.5 mm diameter × 70 mm length** battery tube with a removable rear hatch latched by an M3 bolt.

---

## 7. PCB Fabrication Notes (Phase 5)

For finalizing hardware beyond breadboard status:

1. **KiCad Designing**: Map out a standard 2-layer schematic and layout, generating proper Gerber files.
2. **Layering Strategy**: Top layer (signal tracing) + Bottom layer (continuous ground plane/pour).
3. **Decoupling Capacitors**: Assign a 100 nF ceramic capacitor directly near every ESP32-S3 VCC input header.
4. **Camera Ribbon Connector**: The specific CAM module model houses an OV2640 flex ribbon slot — it can be physically accommodated or buried.
5. **Programming Header**: Verify physical clearance for the USB-C plug, or implement dedicated UART flash pads.
6. **ESD Security**: Apply a TVS diode array adjacent to USB lines.
7. **Production Sourcing**: JLCPCB, PCBWay, or equivalent (Standard config: 2L, 1.6 mm FR4 thickness, 1 oz copper weight).

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

*> **ATENCIÓN**: Las columnas C4, C5, C6 y C7 (GPIO 0, 2, 12, 15) son pines de "strapping" que determinan el modo de arranque del ESP32. **NO MANTENER PULSADAS** teclas de estas columnas durante el encendido o reset, o el ESP32 podría entrar en modo bootloader/fallo.*

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
