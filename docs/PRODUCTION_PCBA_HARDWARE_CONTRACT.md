# NumOS Production PCBA Hardware Contract

Status: **build-confirmed / physically unvalidated** until an incoming PCBA is
flashed and measured. This document establishes the software target for
PROD-PCBA-ENV-01. It does not claim that NumOS boots, that the panel orientation
is correct, or that any switch scans correctly on the delivered hardware.

## Evidence labels

- **schematic-confirmed**: traced directly in the final KiCad schematic and/or
  PCB netlist.
- **PCBWay-test-confirmed**: reported in PCBWay's final reply/test report and
  corroborated by its test photographs.
- **build-confirmed**: enforced by the checked-in profile, manifest, tests, or a
  successful toolchain build.
- **physically unvalidated**: not yet reproduced by NumOS maintainers on an
  incoming production PCBA.

Every hardware row below remains physically unvalidated even where PCBWay
reported a test pass.

## Source priority and audit

The keypad conflict order is:

1. `L-1L1W1027609A.kicad_sch` and `L-1L1W1027609A.kicad_pcb`;
2. final PCB switch positions and designators;
3. `hardware/keyboard/neocalculator-v1-final-5x10-revision-c-canonical.json`;
4. existing NumOS KeyCodes and dispatcher;
5. production BoardProfile;
6. prototype assumptions.

| Material | Direct audit result |
|---|---|
| Final KiCad schematic | U2 pad/net assignments, strap networks, FPC display signals, K1/K2, TP14/TP15, power ICs, and matrix nets inspected. |
| Final KiCad PCB | U2 pads 1–41 and their routed nets inspected; PCB outline is approximately 76 × 156 mm. |
| Final BOM | U2 is `ESP32-S3-WROOM-1U-N16R8`; U1 TP4056, U3 DW01HA, U4 RT6150A, Q1 8205A, Q2 S8050; D1–D50 are fitted 1N4148WS parts. |
| Assembly/schematic drawings | One-page final schematic and PCB drawings inspected. |
| Manufacturing specification | Two-layer 1.6 mm FR-4, 76 × 156 mm, lead-free HASL, 90 Ω USB differential routing. |
| PCBWay reply/test report | Exact module, keypad electrical arrays, LCD GPIOs, native USB programming, panel and backlight tests inspected. |
| `key_jul21d.ino`, `LCD_jul21e.ino`, `LCD-backlight_jul21f.ino` | **Unavailable in the supplied folders and accessible PCBWay email attachments.** No source was reconstructed. Only values independently corroborated by KiCad and the final PCBWay reply/report are used. |
| Existing repository | CAM board manifest, `platformio.ini`, pin abstractions, display/backlight and keypad HALs, Serial backend, partition table, provisioning script, all diagnostic environments, and hardware documentation audited. |

No final KiCad net conflicts with the authoritative contract. The missing
sketch-source audit is a remaining evidence limitation, not permission to reuse
prototype firmware.

## Authoritative board identity

| Property | Production value | Status |
|---|---|---|
| Board ID | `numos-esp32-s3-wroom-1u-n16r8` | build-confirmed |
| Module | ESP32-S3-WROOM-1U-N16R8 | schematic-confirmed; PCBWay-test-confirmed |
| Flash | 16 MB external, QIO runtime at 80 MHz | schematic-confirmed; build-confirmed |
| PSRAM | 8 MB OPI at 80 MHz | schematic-confirmed; build-confirmed |
| CPU | ESP32-S3, 240 MHz supported | build-confirmed |
| USB | ESP32-S3 native USB Serial/JTAG, GPIO19/GPIO20 | schematic-confirmed; PCBWay-test-confirmed |
| UART bridge | None | schematic-confirmed |
| Display | Z320IT008-D, ILI9341, 3.2-inch, 320 × 240 | schematic-confirmed; PCBWay-test-confirmed |
| Electrical keypad | 5 outputs × 10 inputs, 50 fitted per-key diodes | schematic-confirmed; PCBWay-test-confirmed |

The CAM target remains the repository's existing default workflow. Its CH343,
camera-board pins, UART upload assumptions, ROM/header provisioning checks, and
40 MHz panel profile are not inherited by this target.

## Complete GPIO and signal evidence table

“Electrical row/column” describes a schematic matrix coordinate. It does not
describe a visual key position and conveys no `KeyCode`.

| Role | GPIO | Electrical direction | Active level / idle | Source evidence | Confidence | Physical confirmation required |
|---|---:|---|---|---|---|---|
| LCD reset | 1 | output | active low; inactive high | KiCad U2.39 → IO1 → FPC15; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| LCD backlight NPN drive | 2 | output | active high; boot low/off | KiCad U2.38 → IO2 → Q2; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Strap pull-up | 3 | reserved input/strap | external 10 kΩ pull-up | KiCad U2.15 → R9 | schematic-confirmed; build-confirmed | Yes |
| Electrical column input 0 | 4 | input | internal pull-up when future scanner runs; pressed low | KiCad U2.4; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 1 | 5 | input | pull-up; pressed low | KiCad U2.5; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 2 | 6 | input | pull-up; pressed low | KiCad U2.6; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 3 | 7 | input | pull-up; pressed low | KiCad U2.7; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 8 | 8 | input | pull-up; pressed low | KiCad U2.12; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical row output 0 | 9 | output when enabled | inactive high; selected low | KiCad U2.17; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 9 | 10 | input | pull-up; pressed low | KiCad U2.18; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical row output 4 | 11 | output when enabled | inactive high; selected low | KiCad U2.19; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Deliberately unassigned | 12 | no connection | untouched | KiCad U2.20 NC | schematic-confirmed; build-confirmed | Yes |
| Deliberately unassigned | 13 | no connection | untouched | KiCad U2.21 NC | schematic-confirmed; build-confirmed | Yes |
| Deliberately unassigned | 14 | no connection | untouched | KiCad U2.22 NC | schematic-confirmed; build-confirmed | Yes |
| Electrical column input 4 | 15 | input | pull-up; pressed low | KiCad U2.8; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 5 | 16 | input | pull-up; pressed low | KiCad U2.9; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 6 | 17 | input | pull-up; pressed low | KiCad U2.10; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical column input 7 | 18 | input | pull-up; pressed low | KiCad U2.11; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Native USB D− | 19 | reserved bidirectional USB | USB peripheral owned | KiCad U2.13 → USB_D− | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Native USB D+ | 20 | reserved bidirectional USB | USB peripheral owned | KiCad U2.14 → USB_D+ | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical row output 1 | 21 | output when enabled | inactive high; selected low | KiCad U2.23; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Deliberately unassigned | 35 | no connection | untouched | KiCad U2.28 NC | schematic-confirmed; build-confirmed | Yes |
| Deliberately unassigned | 36 | no connection | untouched | KiCad U2.29 NC | schematic-confirmed; build-confirmed | Yes |
| Deliberately unassigned | 37 | no connection | untouched | KiCad U2.30 NC | schematic-confirmed; build-confirmed | Yes |
| LCD CS | 38 | output | active low; idle high | KiCad U2.31 → CSX/SPI_CS → FPC9; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| LCD SCLK | 39 | output | SPI mode 0, idle low | KiCad U2.32 → DCX/SPI_SCLK → FPC10; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| LCD DC | 40 | output | command/data select | KiCad U2.33 → IO40 → FPC11; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| LCD MOSI/SDI | 41 | output | SPI data | KiCad U2.34 → IO41 → FPC13; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| LCD MISO/SDO | 42 | input | optional for basic rendering | KiCad U2.35 → IO42 → FPC14 | schematic-confirmed; build-confirmed | Yes |
| UART0 TX fallback TP14 | 43 | output | 3.3 V UART | KiCad U2.37 → TXD → TP14 | schematic-confirmed; build-confirmed | Yes |
| UART0 RX fallback TP15 | 44 | input | 3.3 V UART | KiCad U2.36 → RXD → TP15 | schematic-confirmed; build-confirmed | Yes |
| Strap pull-up | 45 | reserved input/strap | external 10 kΩ pull-up | KiCad U2.26 → R11 | schematic-confirmed; build-confirmed | Yes |
| Strap pull-down | 46 | reserved input/strap | external 10 kΩ pull-down | KiCad U2.16 → R10 | schematic-confirmed; build-confirmed | Yes |
| Electrical row output 2 | 47 | output when enabled | inactive high; selected low | KiCad U2.24; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| Electrical row output 3 | 48 | output when enabled | inactive high; selected low | KiCad U2.25; PCBWay reply | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| BOOT K2 / TP6 | 0 | reserved strap input | active low | KiCad U2.27 → BOOT → K2/TP6 | schematic-confirmed; PCBWay-test-confirmed; build-confirmed | Yes |
| RESET/EN K1 / TP5 | not a GPIO | hardware reset input | active low | KiCad EN/RESET → K1/TP5 | schematic-confirmed; PCBWay-test-confirmed | Yes |
| LCD TE | not connected | none | untouched | KiCad FPC8 has no MCU route | schematic-confirmed | Yes |
| LCD IM1 | hardware strap | input to panel | hardware-pulled high | final schematic/FPC | schematic-confirmed | Yes |
| LCD IM2 | hardware strap | input to panel | hardware-pulled high | final schematic/FPC | schematic-confirmed | Yes |

## Production PlatformIO environments

Normal full firmware:

```text
[env:numos-esp32-s3-wroom-1u-n16r8]
```

Full firmware plus bounded read-only reporting:

```text
[env:numos-esp32-s3-wroom-1u-n16r8-bringup]
```

Both select `boards/numos-esp32-s3-wroom-1u-n16r8.json`. The bring-up
environment extends the normal production environment; it is not a demonstration
sketch. Both compile the real NumOS app, Giac, LVGL, display HAL, storage, and
input integration.

The pinned stack is PlatformIO Espressif32 6.12.0, Arduino-ESP32 2.0.17,
ESP-IDF 4.4.7, and Xtensa GCC 8.4.0. For this exact core:

- `ARDUINO_USB_MODE=1` selects Hardware CDC and JTAG;
- `ARDUINO_USB_CDC_ON_BOOT=1` enables CDC at boot;
- `qio_opi` selects QIO flash runtime support and 80 MHz octal PSRAM;
- Arduino's QIO menu intentionally emits a DIO ROM image header with a QIO
  second-stage/runtime (`flash_mode=dio`, `boot=qio`).

No TinyUSB/OTG feature, UART bridge, CH343 driver, eFuse, secure boot, or flash
encryption change is part of this target.

## Native USB startup and first flash

Normal production firmware performs zero waiting for CDC or `Serial` readiness
and continues booting immediately without a USB host. Only the explicitly
selected bring-up environment has a maximum three-second CDC enumeration
window. If a host connects after that window or after `setup()` completes, the
non-blocking bring-up service emits its bounded report once. GPIO19/GPIO20 are
compile-time protected from display and matrix allocation.

First flash with the PCBA's USB-C connector:

1. Build: `pio run -e numos-esp32-s3-wroom-1u-n16r8`.
2. Connect a data-capable USB-C cable directly to the PCBA.
3. Hold K2 **BOOT**.
4. Tap and release K1 **RESET**, then release K2.
5. Identify the newly enumerated Espressif USB Serial/JTAG port.
6. Upload:
   `pio run -e numos-esp32-s3-wroom-1u-n16r8 -t upload --upload-port <PORT>`.
7. The port may disappear and re-enumerate after the bootloader/application
   handoff. Re-select the new port for monitoring.
8. Tap RESET if the application does not start automatically.

Recovery repeats steps 3–8. No eFuse burn is needed or permitted.

UART0 is a fallback only: use a **3.3 V** external adapter with adapter RX to
TP14 (ESP TX), adapter TX to TP15 (ESP RX), and a common ground. Enter ROM
download mode with the same BOOT/RESET sequence. There is no onboard
USB-to-UART bridge and the normal upload path remains native USB.

## Flash and partition contract

The production target preserves the pinned framework
`default_16MB.csv` layout. This task does not introduce OTA behavior; the two
existing app slots remain present but NumOS does not add an OTA workflow.

| Region/payload | Offset | Size / upper bound | End |
|---|---:|---:|---:|
| second-stage bootloader | `0x000000` | must end before `0x008000` | `< 0x008000` |
| partition table | `0x008000` | one 4 KiB sector | `0x009000` |
| NVS | `0x009000` | `0x005000` | `0x00E000` |
| OTA selector / `boot_app0.bin` | `0x00E000` | `0x002000` | `0x010000` |
| `app0` / normal firmware | `0x010000` | `0x640000` (6,553,600 bytes) | `0x650000` |
| `app1` retained existing slot | `0x650000` | `0x640000` | `0xC90000` |
| LittleFS (`spiffs` partition subtype) | `0xC90000` | `0x360000` | `0xFF0000` |
| coredump | `0xFF0000` | `0x010000` | `0x1000000` |

The table ends exactly at 16 MB with no overlap. The `spiffs` subtype is the
Arduino partition-table identifier; the board filesystem and firmware API are
LittleFS. No repository `data/` payload is required, so the ordinary build has
no state-bearing `littlefs.bin`; first boot uses the existing firmware mount
policy. A deliberate filesystem image may be built separately when a reviewed
payload exists.

Build artifacts are under the configured PlatformIO build directory:

```text
C:\.piobuild\numOS\numos-esp32-s3-wroom-1u-n16r8\
  bootloader.bin        @ 0x000000
  partitions.bin        @ 0x008000
  firmware.bin          @ 0x010000
```

Arduino's standard upload also supplies its framework `boot_app0.bin` at
`0x00E000`. PlatformIO 6.12.0 has no built-in `mergebin` target. Use the
production-only target
`pio run -e numos-esp32-s3-wroom-1u-n16r8 -t factory_image` when a single
factory artifact is required. It invokes the pinned esptool `merge_bin` command
with `keep` header settings, checks every component bound, and pads
`numos-production-pcba-factory.bin` to exactly 16 MB. Do not use the
CAM-specific `scripts/esp32_boot.py`; its CH343, JEDEC, ROM-header, and
provisioning contract was not transferred to production.

## Display and backlight contract

The production target uses the existing TFT_eSPI hardware-SPI and LVGL path:

- logical 320 × 240 landscape;
- ILI9341, RGB565;
- GPIO42 is declared as connected MISO but reads are not required to render;
- initial SPI limit is 10 MHz for both writes and optional reads;
- CS is driven high before display initialization;
- reset is inactive high, then intentionally pulsed low for 10 ms;
- backlight is driven low before display/reset activity;
- GRAM is cleared black before a bounded PWM level of 96/255 is enabled;
- ordinary firmware clamps backlight requests to 192/255.

PCBWay used a 320 × 240 window and reported MADCTL `0x28`. TFT_eSPI rotation 1
with the provisional BGR selection is configured to represent that starting
point. Orientation, RGB/BGR order, inversion, controller offsets, and maximum
stable SPI frequency are all **physically unvalidated gates**. Production does
not inherit the CAM target's unconditional display inversion.

The bring-up build includes
`DisplayDriver::runBoundedProductionDisplayDiagnostic()`. It is not called by
either checked-in boot path. An explicitly instrumented first-board build may
enable `NUMOS_PRODUCTION_BRINGUP_DISPLAY_AUTORUN`; the routine starts black,
steps backlight through 0/32/96/192, fills black/white/red/green/blue, draws
color-order blocks, gradients, a coordinate grid, labelled corners/edges and
text-baseline markers, then restores the normal LVGL screen. The interactive,
persisted profile workflow is specified in `docs/PROD-DISPLAY-BRINGUP-01.md`.

## Deterministic safe startup

At the first Arduino-controlled instruction:

1. LCD CS is set high before becoming an output.
2. Backlight is set low before becoming an output.
3. LCD reset is set inactive high before becoming an output.
4. SCLK is set to SPI-mode-0 idle low.
5. MOSI/DC remain untouched until the SPI/display driver owns them.
6. All electrical matrix pins remain at reset/input state; no row is driven
   low and the scanner is disabled.
7. GPIO19/20, GPIO0, GPIO3, GPIO45, GPIO46, and all deliberately unassigned
   pins are untouched.
8. No Wi-Fi or Bluetooth service or PCBWay test access point is started.
9. Normal production performs zero Serial-readiness waiting; bring-up waiting
   is bounded to three seconds and the UI never requires a USB host.

The display HAL reasserts the same safe states at its ownership boundary.

## Keypad scope

The electrical arrays are:

```text
row outputs:   9, 21, 47, 48, 11
column inputs: 4, 5, 6, 7, 15, 16, 17, 18, 8, 10
```

The contract-supported profile is inactive rows high, one selected row low,
columns input-pull-up, and a pressed switch observed low. Fifty diodes are
fitted. `logicalMappingReady` and
`NUMOS_PRODUCTION_KEYPAD_MAPPING_READY` are both true.

The canonical Revision C JSON is hash-locked to
`7f6638ae7f830ef0741424d621832756268e5fa41292f2d8f8c363cdbc1a2fc3`.
`scripts/generate-production-keypad.py` validates it and deterministically
generates:

- `src/input/generated/ProductionKeypadMap.generated.h`;
- `hardware/keyboard/production-keypad-map.generated.csv`.

The KiCad-derived visual/electrical transform is intentionally explicit:
visual columns `c0..c4` map to electrical rows `eR4..eR0`, and visual rows
`r0..r9` map to electrical columns `eC0..eC9`. SW2–SW51 cover every
electrical and visual position exactly once. These coordinate systems are
never equated by index.

At deliberate `Keyboard::begin()` ownership, all row output latches are
preloaded inactive before output enable, columns become input-pull-up, and a
timestamped two-phase scanner samples one row at a time. The full-scan period
is 5 ms (200 Hz target), settling is 10 µs, debounce is four full samples
(nominal 15–20 ms transition latency), and repeat is 500/80 ms. The scanner
uses fixed storage, has independent state for all 50 intersections, supports
simultaneous software states, and exposes PRESS/RELEASE/REPEAT through a
64-entry bounded queue. It contains no `delay()` and allocates no heap.

The production normal build dispatches the mapped keypad. The bring-up build
also accepts `KEYPAD RAW ON`, `KEYPAD RAW OFF`, `KEYPAD RAW STATUS`, and
`KEYPAD HELP`; raw reporting is disabled by default.

Deliberate omissions remain: no deep-sleep keypad wake, automatic polarity
probing, measured rollover/ghosting claim, or physical debounce/diode claim.

## Power architecture and capability flags

- SW1 is the true electrical hard-power switch.
- TP4056 charging and its status LEDs are hardware-controlled.
- DW01HA plus 8205A provides battery protection.
- RT6150A enable is hardware-tied and has no software GPIO.
- There is no battery-voltage ADC path.
- Deep sleep is not electrical power-off.

The profile sets `batteryAdc=false`, `softwareRegulatorControl=false`, and
`chargerStatusGpios=false`. Firmware must not display or calculate a battery
percentage from nonexistent data. The existing OFF API remains unwired to final
sleep behavior; on production it can only force the backlight fully off and
reports that wake-source selection is deferred.

## Bounded bring-up report

The opt-in bring-up environment emits its board report once. The matrix is
driven only later by deliberate normal `Keyboard::begin()` ownership:

- board ID, exact module, build date/time revision, and reset reason;
- expected/detected flash;
- expected/detected/free PSRAM;
- native USB mode, pins, and Serial backend;
- display and electrical matrix pin profiles;
- logical-mapping state and capability flags;
- detected partition labels, offsets, and sizes (bounded to 16 entries).

The separately opt-in raw keypad mode reports electrical row/column GPIO,
switch designator, derived visual position, mapped KeyCode/label, raw and
debounced states, transitions, integrator value, active positions, and queue
overflow count. It never probes indefinitely, starts radios, runs the display
pattern automatically, or touches non-matrix pins.

## Board-arrival checklist

Record every result as pass/fail plus measurements; do not promote provisional
values silently.

1. Photograph board revision, module marking, connector orientation, and
   assembly.
2. With power disconnected, check shorts on 3.3 V, battery, USB, and ground.
3. Verify BOOT/RESET continuity and strap resistor populations.
4. Power from a current-limited USB supply with battery disconnected; measure
   input current and the 3.3 V rail.
5. Enter ROM download using BOOT/RESET and confirm native USB enumeration.
6. Flash the normal production build through native USB; record enumeration
   identities before, during, and after reset.
7. Capture the bring-up report; compare detected flash, PSRAM, and partitions.
8. Confirm normal boot has zero USB-readiness delay, completes with no USB
   host, and reconnecting USB does not stall the UI. In bring-up, connect after
   startup and confirm the bounded report is emitted once.
9. Check with an oscilloscope/logic analyzer that CS is high, BL is low, reset
   is high, and SCLK is low through early startup.
10. Run the explicit bounded display diagnostic. Determine orientation,
    RGB/BGR, inversion, offsets, reset timing, minimum useful brightness, and
    current draw.
11. Increase SPI frequency only in measured steps; retain 10 MHz on any error.
12. Verify GPIO42 reads only if a later requirement needs panel readback.
13. Send `KEYPAD RAW STATUS`, then `KEYPAD RAW ON`; confirm no raw mode was
    active automatically.
14. Verify idle-high rows, selected-low row polarity, pull-up columns, pressed
    low level, and diode direction with an oscilloscope/continuity fixture.
15. Press all 50 keys visually from top-left to bottom-right and capture the
    reported SW/electrical/visual/KeyCode record. Compare it to
    `production-keypad-map.generated.csv`.
16. Exercise same-row, same-column, diagonal, modifier-plus-key, and several-key
    chords. Record observed rollover and impossible states without claiming
    NKRO or ghost prevention.
17. Measure press/release bounce and adjust only centralized profile/debounce
    values if evidence requires it. Verify long hold, bounded repeat, rapid
    repress, scanner disable while held, and force-release recovery.
18. Run the event MVP: launcher navigation, Calculation digits/operators and
    structures, SHIFT/ALPHA/SHIFT+ALPHA, DEL/AC/EXE/HOME/BACK, then Grapher
    expression entry and a basic graph.
19. Send `KEYPAD RAW OFF` and archive the compact validation log.
20. Measure backlight-off current and document that deep sleep differs from
    SW1 hard-off.
21. Test TP14/TP15 only as an external-adapter recovery fallback.
22. Preserve logs and promote confidence labels only from recorded evidence.

## Physical validation gates and remaining limitations

- NumOS has not yet booted on an incoming PCBA.
- Native USB first-flash/re-enumeration has not been reproduced by NumOS.
- LCD orientation, color order, inversion, offsets, reset timing, and SPI
  ceiling remain provisional.
- Backlight PWM linearity, safe maximum, and current remain unmeasured.
- The electrical switch-coordinate and logical map is KiCad/layout-derived,
  host-simulated, and build-tested, but not physically validated.
- Pressed polarity, rollover, ghost prevention, debounce timing, diode
  direction, switch feel, signal integrity, and wake suitability are unverified.
- Battery/charger behavior is documented only; no ADC or status GPIO exists.
- The three named PCBWay `.ino` source files remain unavailable for direct
  audit.
