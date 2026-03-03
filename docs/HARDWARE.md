# NumOS — Hardware Reference

> **Platform**: ESP32-S3 N16R8 CAM · ILI9341 IPS 3.2" · 5×10 Keyboard
> **Build Stats (Mar 2026)**: RAM 28.8% (94 512 B / 327 680 B) · Flash 19.3% (1 263 109 B / 6 553 600 B)
>
> Complete hardware reference for NumOS. Covers pinout, wiring, known GPIO conflicts, critical bugs resolved, bring-up notes, and memory management (CAS-Lite PSRAM).
>
> **Last updated**: March 2026

---

## 1. Microcontroller — ESP32-S3 N16R8 CAM

### Key Specifications

| Parameter | Value |
|:----------|:------|
| **MCU** | ESP32-S3 (dual-core Xtensa LX7 @ 240 MHz) |
| **Flash** | 16 MB — Quad SPI (QIO) |
| **PSRAM** | 8 MB — Octal SPI (OPI) |
| **USB** | Native USB 1.1 Full-Speed (CDC + JTAG) |
| **GPIO** | 45 pins (no ADC-only inputs like classic ESP32) |
| **SPI** | SPI0 (Flash), SPI1 (internal), SPI2/FSPI (**available**), SPI3/GSPI |

### Memory Configuration (CRITICAL in PlatformIO)

```ini
board_build.arduino.memory_type = qio_opi   ; Flash QIO + PSRAM OPI
board_build.flash_mode          = qio        ; Flash mode
board_upload.flash_size         = 16MB
board_build.partitions          = default_16MB.csv
```

> ⚠️ **If `qio_opi` is not specified**, ESP-IDF attempts to initialize PSRAM in standard SPI mode and boot ends with `Guru Meditation: Illegal Instruction` immediately.

---

## 2. Display — ILI9341 IPS TFT 3.2"

### Specifications

| Parameter | Value |
|:----------|:------|
| **Controller** | ILI9341 |
| **Panel type** | IPS (wide viewing angles, no color distortion) |
| **Resolution** | 320 × 240 px (landscape) |
| **Color** | 16 bpp (RGB565) |
| **Interface** | SPI (4 wires: MOSI, SCLK, CS, DC) |
| **SPI Frequency** | 10 MHz (`-DSPI_FREQUENCY=10000000`) — reduced from 40 MHz to avoid artifacts on breadboard |
| **SPI Bus** | FSPI (SPI_PORT=2) — CRITICAL: see Fix ② |
| **Power** | 3.3V logic, BL hardwired to 3.3V |

### ESP32-S3 ↔ ILI9341 Pinout

| TFT Signal | ESP32-S3 GPIO | Notes |
|:-----------|:-------------:|:------|
| VCC | 3V3 | 3.3V logic power |
| GND | GND | Common ground |
| MOSI / SDI | **13** | SPI Data In |
| SCLK / SCK | **12** | SPI Clock |
| CS | **10** | Chip Select (active LOW) |
| DC / RS | **4** | Data/Command selector |
| RST | **5** | Reset (active LOW, 50 ms pulse) |
| BL / LED | **45** → 3.3V | Backlight — **hardwired to 3.3V** |
| MISO / SDO | — | Not connected (no touch) |

> ✅ **GPIO 45 (BL)**: In this build, the backlight pin is physically connected to 3.3V. Code **always** does `pinMode(45, INPUT)` to make the pin high-impedance. If set to `OUTPUT LOW`, it would short the power and the display would stop working.

> ✅ **GPIO 4/5 conflict — RESOLVED (2026-03-02)**:
> Previously, `GPIO 4` (`TFT_DC`) and `GPIO 5` (`TFT_RST`) were also listed as keyboard column pins C0/C1,
> which would have caused display corruption on any keypress. This has been fixed in `src/drivers/Keyboard.h`:
> - **C0** reassigned from GPIO 4 → **GPIO 6** (free)
> - **C1** reassigned from GPIO 5 → **GPIO 7** (free)
> - **C2** reassigned from GPIO 6 → **GPIO 8** (free)
>
> The three currently wired columns use GPIOs 6, 7, 8 — none of which conflict with TFT, PSRAM or row pins.

### TFT Initialization (exact sequence in code)

```cpp
_tft.init();                    // FSPI @ 10 MHz
_tft.invertDisplay(true);       // REQUIRED for IPS panel — without this colors are inverted
_tft.setRotation(1);            // Landscape: 320 wide × 240 tall
pinMode(PIN_TFT_BL, INPUT);     // BL hardwired to 3.3V — never OUTPUT
_tft.fillScreen(0x0000);        // Clear GRAM with black
```

### Memory Management — CAS-Lite and PSRAM

The **CAS-Lite Engine** (EquationsApp) stores all symbolic data in PSRAM:
- `SymPoly::CoeffMap` → `std::map` with `PSRAMAllocator` → `ps_malloc`
- `CASStepLogger::StepVec` → `std::vector` with `PSRAMAllocator` → `ps_malloc`
- LVGL and DMA buffers remain in internal RAM (`MALLOC_CAP_DMA | MALLOC_CAP_8BIT`)

> ✅ **CAS-Lite does NOT interfere with display buffers.** PSRAM is shared by:
> - General PSRAM heap (Arduino `ps_malloc`)
> - CAS-Lite Engine (via `PSRAMAllocator`)
> Display DMA buffers use **exclusively internal RAM** — separation guaranteed.

---

### Why `USE_FSPI_PORT` (Critical Fix ②)

ESP-IDF defines `REG_SPI_BASE(i)` as:
```c
// esp-idf/components/soc/esp32s3/include/soc/soc.h
#define REG_SPI_BASE(i) (((i)>1) ? (DR_REG_SPI2_BASE + (i-2)*0x1000) : 0)
```

If `SPI_PORT = 0` (TFT_eSPI default for FSPI), then `REG_SPI_BASE(0) = 0`. Any SPI register access becomes a write to address `0x10` → **StoreProhibited / Guru Meditation**.

The flag `-DUSE_FSPI_PORT` forces `SPI_PORT = 2` in TFT_eSPI:
```
REG_SPI_BASE(2) = DR_REG_SPI2_BASE = 0x60024000  ✓
```

**Without this flag, the board always crashes in `TFT_eSPI::begin_tft_write()`.**

---

## 3. LVGL Render Buffers (Critical Fix ④)

```cpp
// ✅ CORRECT — 32 KB single buffer, internal SRAM, DMA-capable
void* buf1 = heap_caps_malloc(32 * 1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
void* buf2 = nullptr;   // Single buffer ONLY — see note below

// ❌ WRONG — PSRAM is not DMA-capable for SPI on ESP32-S3
void* buf1 = ps_malloc(32 * 1024);  // StoreProhibited crash on first flush

// ❌ WRONG — double buffer causes LVGL 9.x pipelining deadlock
void* buf2 = heap_caps_malloc(32 * 1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
```

**32,768 bytes = ~51.2 scanlines × 320 px × 2 bytes (RGB565).** With `LV_DISPLAY_RENDER_MODE_PARTIAL`, LVGL renders in ≈51-line strips → ~5 `pushColors()` calls per full frame (vs ~25 with the old 6,400-byte buffer). Halving strip count roughly halves the "curtain" artifact visible on fast scroll.

> ⚠️ **Single buffer only.** LVGL 9.x pipelining (`lv_display_set_flush_cb`) expects a DMA-done interrupt to release the buffer. TFT_eSPI uses blocking `pushColors()` — no ISR fires — so LVGL deadlocks waiting for a buffer-free signal if `buf2 != nullptr`.

---

## 4. Keyboard — 5×10 Matrix

### Specifications

| Parameter | Value |
|:----------|:------|
| **Configuration** | 5 rows (output) × 10 columns (input) = 50 keys max |
| **Currently wired** | 5 rows × 3 columns = 15 keys |
| **Rows** | OUTPUT, toggled LOW during scan |
| **Columns** | INPUT_PULLUP — no external resistors needed |
| **Debounce** | 20 ms in firmware |
| **Autorepeat** | 500 ms delay, 80 ms rate |

### Keyboard Pinout — Current Wiring (5 rows × 3 columns)

> The physical PCB has 5 row traces and 10 column traces planned. Only 3 columns are wired in the current prototype. Full 5 × 10 matrix support is implemented in firmware; increase `KBD_CONNECTED_COLS` in `Config.h` when more columns are soldered.

| Signal | GPIO | Direction | Note |
|:-------|:----:|:---------:|:-----|
| **R0** | **1** | OUTPUT (active LOW) | Row 0 — drives LOW during scan |
| **R1** | **2** | OUTPUT (active LOW) | Row 1 |
| **R2** | **41** | OUTPUT (active LOW) | Row 2 |
| **R3** | **42** | OUTPUT (active LOW) | Row 3 |
| **R4** | **40** | OUTPUT (active LOW) | Row 4 (bottom) |
| **C0** | **6** | INPUT_PULLUP | Column 0 — wired ✅ |
| **C1** | **7** | INPUT_PULLUP | Column 1 — wired ✅ |
| **C2** | **8** | INPUT_PULLUP | Column 2 — wired ✅ |
| C3–C9 | 3,15,16,17,18,21,47 | INPUT_PULLUP | Not wired yet — defined in `Config.h` |

> ✅ **GPIO 4/5 conflict resolved**: The new `Keyboard` driver (Phase 7) uses GPIOs 6, 7, 8 for the three currently wired columns. GPIO 4 and 5 are exclusively reserved for `TFT_DC` and `TFT_RST`.

### Key Layout

#### Row R0 — Modes and Softkeys
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| SHIFT | ALPHA | MODE | SETUP | F1 | F2 | F3 | F4 |

#### Row R1 — Global Control and Navigation
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| ON | AC | DEL | FREE_EQ | LEFT | UP | DOWN | RIGHT |

#### Row R2 — Grapher and Advanced Functions
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| X | Y= | TABLE | GRAPH | ZOOM | TRACE | STEPS | SOLVE |

#### Row R3 — Upper Numeric + Operators
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 7 | 8 | 9 | ( | ) | ÷ | ^ | √ |

#### Row R4 — Middle Numeric + Trig
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 4 | 5 | 6 | × | − | sin | cos | tan |

#### Row R5 — Lower Numeric + Enter
| C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 1 | 2 | 3 | + | (−) | 0 | . | ENTER |

### Scanning Algorithm

```cpp
// In KeyMatrix::scan() — executed every 5 ms
for (int col = 0; col < KEY_COLS; col++) {
    digitalWrite(colPins[col], LOW);   // Activate column
    delayMicroseconds(2);              // Let level stabilize
    for (int row = 0; row < KEY_ROWS; row++) {
        bool pressed = (digitalRead(rowPins[row]) == LOW);
        // Process debounce and autorepeat...
    }
    digitalWrite(colPins[col], HIGH);  // Deactivate column
}
```

---

## 5. Virtual Keyboard (SerialBridge)

For testing without physical keyboard, the calculator can be controlled from PC Serial Monitor:

```
[SerialBridge] Controls:
  w/a/s/d    = Arrow keys (↑←↓→)
  z          = ENTER / Confirm
  x          = DEL / Delete
  c          = AC / Clear All
  h          = HOME / Mode
  0-9        = Digits
  +-*/^.()   = Operators
  S          = SHIFT (uppercase)
  f          = FRACTION (SHIFT+DIV)
  r          = √ SQRT
  R          = ⁿ√ nth-ROOT (SHIFT+SQRT)
  g          = GRAPH
  t          = SIN
  y          = VAR_Y
```

> **Important**: `s` lowercase = DOWN. `S` uppercase = SHIFT. Monitor sends `\r\n` on Enter, which is **ignored** (use `z` for ENTER). Make sure you have `monitor_rts=0` and `monitor_dtr=0` in platformio.ini.

---

## 6. USB Serial — Connection Procedure

ESP32-S3 uses its native USB CDC. After flashing:

1. **Wait ~3 seconds** after upload.
2. Open Serial Monitor with `monitor_rts=0`.
3. You should see:
   ```
   === NumOS Boot ===
   [PSRAM] 7680 KB free
   [TFT] OK
   [BOOT] OK — Type 'w' and press Enter to test.
   [HB] 5s uptime | heap=XXXXX
   ```
4. If you see periodic `[HB]`, Serial TX works.
5. If you see `[SB] RX: 'w' (0x77)` when typing, Serial RX also works.

---

## 7. Optional Battery

For portable use:

| Component | Specification |
|:----------|:--------------|
| **Cell** | Li-Ion 18650 (3.7V nominal) |
| **Charger** | TP4056 with battery protection |
| **Booster** | MT3608: 3.7V → 5V to power via VIN |
| **Monitor** | Resistive divider 100kΩ+100kΩ → analog GPIO (not yet implemented) |

> Battery monitoring implementation in software is pending (Phase 5).

---

## 8. Summary of Critical Fixes

| # | Fix | Symptom without fix | Solution |
|:-:|:----|:-------------------|:---------|
| ① | Flash OPI | `Illegal Instruction` at boot | `memory_type = qio_opi` |
| ② | `USE_FSPI_PORT` | `StoreProhibited` in `TFT_eSPI::begin` | `-DUSE_FSPI_PORT` in build_flags |
| ③ | SPI 10 MHz | Display lines/noise on breadboard | `SPI_FREQUENCY=10000000` |
| ④ | DMA buffers | Black screen, flush doesn't paint | `heap_caps_malloc(MALLOC_CAP_DMA\|MALLOC_CAP_8BIT)` |
| ⑤ | GPIO 45 INPUT | Display dies on BL power-up | `pinMode(45, INPUT)` — never `OUTPUT LOW` |
| ⑥ | Serial CDC wait | Lost output, monitor disconnects | `while(!Serial)` + `monitor_rts=0` / `monitor_dtr=0` |

---

*Chassis 3D printing guide reference: [DIMENSIONES_DISEÑO.md](DIMENSIONES_DISEÑO.md)*

