# PROD-DISPLAY-BRINGUP-01

## Scope and safety boundary

This bring-up surface is compiled only when both
`NUMOS_BOARD_PROD_WROOM1U_N16R8` and `NUMOS_PRODUCTION_BRINGUP` are enabled.
It does not change the native-USB configuration, keypad ownership, boot straps,
partition table, or any CAM target. Normal production starts and remains usable
without a USB host.

The immutable `SAFE` profile is the sole fallback:

| Field | SAFE value |
|---|---:|
| Profile ID | `SAFE` |
| Logical orientation | rotation 1, 320 x 240 landscape |
| ILI9341 MADCTL | `0x28` |
| Color order | BGR |
| Inversion | off |
| X/Y offset | `0`, `0` |
| Write/read SPI | 10 MHz / 10 MHz |
| Reset low/recovery | 10 ms / 120 ms |
| Initial/maximum backlight | 96 / 192 |

`SAFE` is a `constexpr` object. Validation rejects a record that uses the
`SAFE` identifier with any changed field.

## Profile bounds and presets

The profile is a fixed-size, allocation-free value object. Bring-up accepts
only landscape rotations 1 and 3, whole-megahertz SPI values from 1 through
40 MHz, offsets from -32 through +32 pixels, reset-low timing from 1 through
200 ms, recovery timing from 5 through 500 ms, and backlight values from 0
through 192. Initial backlight may not exceed the profile maximum.

| ID | Rotation | MADCTL | Order | Invert | Offset | SPI MHz | Reset ms | Backlight |
|---|---:|---:|---|---|---|---|---|---|
| `SAFE` | 1 | `0x28` | BGR | off | 0,0 | 10/10 | 10/120 | 96/192 |
| `ROT3-BGR` | 3 | `0xE8` | BGR | off | 0,0 | 10/10 | 10/120 | 96/192 |
| `ROT1-RGB` | 1 | `0x20` | RGB | off | 0,0 | 10/10 | 10/120 | 96/192 |
| `ROT1-BGR-INV` | 1 | `0x28` | BGR | on | 0,0 | 10/10 | 10/120 | 96/192 |

Any accepted field edit changes the active identifier to `CUSTOM`. No edit is
automatically persisted.

## TFT_eSPI runtime audit

The installed dependency is TFT_eSPI 2.5.43.

| Setting | Status | Implementation |
|---|---|---|
| Rotation/orientation | Runtime | Public `TFT_eSPI::setRotation()` keeps the library dimensions and viewport coherent. Only rotations 1 and 3 are accepted because NumOS remains 320 x 240 landscape. |
| MADCTL and RGB/BGR | Runtime at the target seam | Rotation is the sole MX/MY/MV authority. NumOS derives `0x20`/`0xE0` from rotation 1/3, ORs only the independently validated BGR bit `0x08`, and writes ILI9341 command `0x36`. Raw MADCTL is not a command or persistent field. |
| Display inversion | Runtime | Public `TFT_eSPI::invertDisplay(bool)`. |
| X/Y offsets | Runtime at the NumOS seam | TFT_eSPI's `colstart` and `rowstart` are private. The LVGL flush translates and clips each row without allocation. Direct bring-up drawings use the same active offsets. |
| Write/read SPI frequency | Runtime at the target seam | TFT_eSPI exposes no public SPI-frequency setter. On ESP32 it evaluates `SPI_FREQUENCY` and `SPI_READ_FREQUENCY` when opening each transaction. The production environment alone binds those macros to validated runtime variables. |
| Reset timings | Runtime at the NumOS seam | The target-specific reset/reinitialization routine uses the validated low and recovery delays before `_tft.init()`. |
| Backlight | Runtime | NumOS PWM, always clamped to the active profile maximum and the global 192 ceiling. |
| Controller family and initialization table | Compile-time only | `ILI9341_DRIVER`; changing controller family requires a separate build environment. |
| Pins, SPI host/mode, native dimensions | Compile-time only | CS/SCLK/DC/MOSI/MISO/reset/backlight, FSPI, mode 0, and native 240 x 320 remain build flags. |
| Pixel format and compiled fonts | Compile-time only | RGB565 and the TFT_eSPI font selection remain fixed. |

The smallest reinitialization seam is
`DisplayDriver::applyProductionDisplayProfile(profile, resetController)`.
Settings that only require controller commands use `resetController=false`;
selecting a preset or issuing `DISPLAY RESET` performs the bounded hardware
reset and target reinitialization.

The controller configuration order is fixed:

1. `TFT_eSPI::setRotation(profile.rotation)` writes the library rotation and
   updates TFT_eSPI's private logical dimensions.
2. NumOS derives the expected logical geometry from that same rotation and
   requires TFT_eSPI to report exactly 320 x 240.
3. NumOS installs the validated X/Y flush offsets.
4. NumOS derives the color-order bit from the `ColorOrder` enum.
5. NumOS derives and writes the final MADCTL (`0x20`, `0x28`, `0xE0`, or
   `0xE8`) from rotation plus color order only.
6. NumOS applies inversion.

This gives persistence one coherent source of truth: rotation owns axes and
logical geometry; color order owns only bit `0x08`. Changing BGR cannot change
geometry. The expert MADCTL decoder accepts exactly the four derived values
above for validation/tests, but no raw-MADCTL command is exposed and no raw byte
can be saved.

## Offset clipping and flush invariants

The SAFE zero-offset path retains the established bulk flush and 320 x 240
geometry. A nonzero-offset flush is translated and clipped by a fixed-value
plan, then written one visible row at a time:

- left/top clipping advances the source pointer by the exact removed columns
  and rows;
- right/bottom clipping shortens the row count/width without advancing past
  the source rectangle;
- the final exclusive source index is bounded by the original source pixel
  count;
- completely clipped areas perform no TFT write;
- one-pixel and full-screen areas remain valid;
- the LVGL completion callback is structurally invoked exactly once for every
  flush, including fully clipped areas;
- the planner/executor uses no heap, `std::function`, virtual dispatch, or
  PSRAM allocation.

Host coverage includes zero offsets, positive and negative X/Y offsets,
partial clipping on all four edges, complete clipping on all four edges,
one-pixel rectangles, full-screen flush, source-pointer adjustment, source
upper-bound checks, exactly-once completion, allocation counting, and unchanged
SAFE geometry.

## Bring-up commands

The parser is case-insensitive, has an inclusive 79-character bound and a
five-token bound, does not allocate, and rejects extra or missing arguments.
Any malformed or unsupported `DISPLAY` command immediately reapplies `SAFE`.

```text
DISPLAY HELP
DISPLAY INFO
DISPLAY TEST
DISPLAY PROFILE LIST
DISPLAY PROFILE SET <SAFE|ROT3-BGR|ROT1-RGB|ROT1-BGR-INV>
DISPLAY ROTATE <1|3>
DISPLAY BGR <ON|OFF>
DISPLAY INVERT <ON|OFF>
DISPLAY OFFSET <-32..32> <-32..32>
DISPLAY SPI <1..40>
DISPLAY BACKLIGHT <0..192>
DISPLAY SAVE
DISPLAY RESET
DISPLAY SAFE
```

`DISPLAY RESET` means display-controller reset, not ESP32 reset.
`DISPLAY SAFE` changes only the active profile; use `DISPLAY SAVE` explicitly
if the safe value should replace an earlier saved profile.

## DISPLAY TEST sequence

The test is explicitly invoked and bounded. It keeps the backlight off during
its initial black clear, then reports and visits stages 0, 32, 96, and the
active bounded high. It displays, in order:

1. black, white, red, green, and blue full-screen fills;
2. labelled RGB reference blocks;
3. a one-pixel edge rectangle and 40-pixel coordinate grid;
4. individually labelled TL, TR, BL, and BR corners;
5. TOP, BOTTOM, LEFT, and RIGHT labels;
6. horizontal and vertical RGB565 gradients;
7. text-baseline rules, small glyphs, and the panel identifier.

It clears to black, restores the active profile's initial backlight, invalidates
the LVGL screen, and returns to the normal launcher.

## Persistence and rollback

`DISPLAY SAVE` writes one 48-byte Preferences record under namespace
`numosdisp`, key `profile`. The record contains magic `NDP2`, format version 2,
size 48, schema tag `0x5A320002`, profile identifier, rotation, color order,
inversion, signed X/Y offsets, write/read SPI rates, reset-low/recovery times,
initial/maximum backlight, and required-zero reserved bytes. Raw MADCTL and boot
attempt state are not persistent profile fields. CRC-32 covers bytes 0 through
43 inclusive; the checksum occupies bytes 44 through 47. Magic, version, size,
schema, reserved bytes, checksum, enum values, preset immutability, and every
bound must validate. Unknown, old, truncated, or corrupt records resolve to the
immutable `SAFE` profile, so build changes cannot silently reinterpret version
1 or another schema.

Boot recovery uses two separate pieces of state:

- NVS key `failures` is a one-byte genuine-failure count.
- An RTC no-init marker contains a magic value, a CRC identity over the complete
  48-byte saved record, and that identity's inverse. It is armed immediately
  before a saved profile is allowed to initialize the panel.

The marker is acknowledged and cleared only after `SystemApp::begin()` has
loaded the launcher, its 200 ms transition has completed, and the splash has
been destroyed. A matching armed marker increments `failures` only when
`esp_reset_reason()` reports panic, interrupt watchdog, task watchdog, or other
watchdog. Power-on and brownout are classified as power interruption;
external, software, deep-sleep and SDIO resets are classified as clean. Those
classes, plus unknown reset reasons or an unmatched marker, never increment the
count.

The failure threshold is two. The first matching genuine failed boot increments
NVS to one and retries the saved profile; the second increments to two, selects
`SAFE`, reports `safe-rollback`, and stops arming the saved attempt. A successful
launcher acknowledgement clears a nonzero count once. `DISPLAY SAVE` replaces
the record, clears a nonzero count, clears the RTC marker, and is the only
operator action that releases a quarantined saved profile. `DISPLAY SAFE`
always restores the compile-time immutable profile in RAM and does not depend
on the record being readable.

NVS is therefore not written on every ordinary boot: it is written only by
explicit `DISPLAY SAVE`, by a matching panic/watchdog failure increment, or by
the first healthy acknowledgement after a nonzero failure count. SW1 hard-power
interruption loses or invalidates the RTC marker and the next power-on does not
increment; an ordinary software reset also does not increment. A panic or
watchdog before launcher acknowledgement increments only when the marker
matches the same saved record.

This rollback detects repeated crashing initialization, not visual
incorrectness. A merely wrong orientation, RGB/BGR selection, inversion,
offset, brightness, or marginal-but-noncrashing SPI setting can still reach the
launcher and be acknowledged; automatic recovery is not claimed for those
cases.

No boot chord is enabled in this revision. It is optional, and adding one before
the display owns its pins would broaden the already validated keypad/strap
startup contract. `DISPLAY SAFE`, CRC/schema fallback, and the boot-attempt
guard provide rollback without requiring USB for normal operation.

## Expected first-board USB transcript

This is the expected protocol transcript, not a hardware capture; the PCBAs
have not arrived.

```text
> DISPLAY INFO
[DISPLAY] active id=SAFE rotation=1 madctl=0x28 order=BGR invert=0 offset=(0,0) write_mhz=10 read_mhz=10 reset_ms=10/120 backlight=96 initial=96 max=192
[DISPLAY] source=safe-no-record driver=TFT_eSPI-2.5.43 failures=0/2 reset=power-interruption runtime=rotation,madctl-rgb-bgr,inversion,offset,spi,backlight compile_time=controller,pins,bus,geometry,pixel-format
> DISPLAY PROFILE LIST
[DISPLAY] preset id=SAFE rotation=1 madctl=0x28 order=BGR invert=0 offset=(0,0) write_mhz=10 read_mhz=10 reset_ms=10/120 backlight=96 initial=96 max=192
[DISPLAY] preset id=ROT3-BGR rotation=3 madctl=0xE8 order=BGR invert=0 offset=(0,0) write_mhz=10 read_mhz=10 reset_ms=10/120 backlight=96 initial=96 max=192
[DISPLAY] preset id=ROT1-RGB rotation=1 madctl=0x20 order=RGB invert=0 offset=(0,0) write_mhz=10 read_mhz=10 reset_ms=10/120 backlight=96 initial=96 max=192
[DISPLAY] preset id=ROT1-BGR-INV rotation=1 madctl=0x28 order=BGR invert=1 offset=(0,0) write_mhz=10 read_mhz=10 reset_ms=10/120 backlight=96 initial=96 max=192
> DISPLAY SPI 41
[DISPLAY] ERROR parse=unsupported-value; immutable SAFE restored
> DISPLAY TEST
[DISPLAY] TEST BEGIN bounded; launcher resumes on completion
[DISPLAY] TEST backlight=off level=0
[DISPLAY] TEST backlight=low level=32
[DISPLAY] TEST backlight=normal level=96
[DISPLAY] TEST backlight=bounded-high level=192
[DISPLAY] TEST END launcher restored
> DISPLAY SAVE
[DISPLAY] SAVE OK version=2 crc32=valid failures=0
```

## Build footprint and verification

The phase delta is measured against the immediate PROD-KEYPAD-01 artifacts
supplied for acceptance, not by subtracting the two current environments:

| Environment | Previous flash | Current flash | Phase flash delta | Previous RAM | Current RAM | Phase RAM delta |
|---|---:|---:|---:|---:|---:|---:|
| Production normal | 5,352,509 B | 5,356,701 B | +4,192 B | 118,904 B | 118,960 B | +56 B |
| Production bring-up | 5,356,301 B | 5,368,113 B | +11,812 B | 119,040 B | 119,144 B | +104 B |

The current bring-up overhead relative to current normal is a separate
comparison, not the phase delta:

| Metric | Current normal | Current bring-up | Bring-up overhead |
|---|---:|---:|---:|
| PlatformIO flash usage | 5,356,701 B | 5,368,113 B | +11,412 B |
| `firmware.bin` | 5,357,072 B | 5,368,480 B | +11,408 B |
| Total DRAM usage | 118,960 B | 119,144 B | +184 B |
| `.dram0.data` | 21,256 B | 21,256 B | 0 B |
| `.dram0.bss` | 97,704 B | 97,888 B | +184 B |
| `.iram0.text` | 60,407 B | 60,407 B | 0 B |
| `.flash.text` | 3,758,255 B | 3,765,691 B | +7,436 B |
| `.flash.rodata` | 1,515,756 B | 1,519,732 B | +3,976 B |

Xtensa disassembly reports a 96-byte local frame for the bounded parser and a
96-byte frame for the bring-up service, for a 192-byte own-code command-path
stack chain. The diagnostic local frame is 80 bytes. Parsing, diagnostics,
offset flushes, profile application, and CRC operations allocate no heap and
use no PSRAM. Preferences/NVS activity is isolated to boot record loading,
genuine-failure/healthy-acknowledgement recovery transitions, and explicit
`DISPLAY SAVE`; it is not in the render or command parsing hot path.

Verification completed on the host:

- production normal and bring-up: PASS;
- all four CAM environments (`esp32s3_n16r8`, `_validate`, `_giacdiag`,
  `_mathdiag`): PASS;
- production display profile/parser/persistence suite: PASS;
- production keypad suite: PASS, all 50 mappings;
- production manifest and 16 MB partition overlap/offset checks: PASS;
- `emulator_pc`: PASS, including deterministic 800-frame headless launcher;
- `emulator_pc_neo_smoke`: PASS, 60 commands over 2,400 frames;
- deterministic visual candidates: 42/42 generated;
- accepted goldens/masks: 18 compared, 0 mismatches;
- Giac: 177 + 26 + 44 + 14 = 261/261 PASS.

The final CAM matrix remained at 117,552 B RAM in every environment:

| CAM environment | Flash | RAM | Production-display symbols |
|---|---:|---:|---:|
| `esp32s3_n16r8` | 5,346,821 B | 117,552 B | 0 |
| `esp32s3_n16r8_validate` | 5,380,521 B | 117,552 B | 0 |
| `esp32s3_n16r8_giacdiag` | 5,351,045 B | 117,552 B | 0 |
| `esp32s3_n16r8_mathdiag` | 5,385,681 B | 117,552 B | 0 |

The 40-byte reduction relative to the earlier CAM audit is explained by
compiling the production-only runtime globals and implementation completely
out of CAM firmware. No CAM resources or display behavior changed.

Web acceptance used the already installed, repository-pinned Emscripten 6.0.3
(`283e2d130132859fde6a4e4c87fd254b38127651`), its pinned Node 22.16.0,
Playwright 1.54.1, and the existing Chromium/Firefox/WebKit cache. No toolchain
or browser was upgraded. The authoritative tree plus uncommitted overlay was
copied to the ASCII-only snapshot
`C:\.codex-cache\numos-display-acceptance-7654806-20260729`; 1,110 files matched
byte-for-byte before compilation with manifest SHA-256
`1fce1447e0aa83aef1dc118ab6643ab2fd8b1ebcc9a0688d58519aaba1db0071`.

Both Release and Debug `emulator_web` and reusable-component artifacts built,
as did Release and Debug WASM-MATH. Release and Debug emulator smoke and
persistence passed. The reusable component passed in Chromium, Firefox, and
WebKit in both configurations. WASM-MATH passed its full Chromium, Firefox, and
WebKit matrix in both configurations. The logical key catalog passed 79/79.
Generated CMake/build metadata and the explicit 68-translation-unit
emulator/web source closure contain no `ProductionDisplay*`, `DisplayDriver`,
or production bring-up implementation.

## Board-arrival checklist

1. Flash the production bring-up image; leave the normal image available for
   immediate rollback.
2. Confirm reset, CS, and backlight idle levels with backlight initially off;
   verify there is no bright startup flash.
3. Run `DISPLAY SAFE`, `DISPLAY INFO`, then `DISPLAY TEST`.
4. Identify the physical top, check all four corner labels and the one-pixel
   rectangle, and select rotation 1 or 3.
5. Compare the red/green/blue reference blocks with known color labels; toggle
   `DISPLAY BGR ON|OFF` if necessary.
6. Check full fills for unexpected inversion; toggle inversion only if the
   panel requires it.
7. Inspect every edge and the coordinate grid; adjust offsets in one-pixel
   steps only if a controller window is demonstrably shifted.
8. Validate small glyphs and baseline rules with a loupe or close photograph.
9. Measure 0/32/96/192 backlight current and luminance. Reduce the chosen
   maximum if heat, current, or useful brightness demands it.
10. Increase SPI in measured steps while repeatedly running fills, gradients,
    grid, launcher navigation, and long-duration refresh. Return to 10 MHz at
    the first corruption or timing fault.
11. Power-cycle repeatedly before saving. Confirm the launcher is usable with
    USB disconnected.
12. Save only the physically validated profile, power-cycle again, and verify
    `source=saved`.
13. Confirm that an SW1 hard-power interruption and an ordinary software reset
    before acknowledgement do not increment `failures`.
14. In a deliberately instrumented image, force a panic/watchdog before
    launcher acknowledgement twice for the same saved record: the first boot
    must retry with `failures=1/2`; the second must select
    `source=safe-rollback` with `failures=2/2`.
15. Issue `DISPLAY SAFE` while the saved record is quarantined, then explicitly
    save a physically validated profile and verify that the quarantine clears.
16. Reflash the normal production image and repeat launcher/keypad operation
    without USB.

## Physically unvalidated values

Until the first PCBA and Z320IT008-D are measured, the following remain
provisional: final orientation, RGB versus BGR, inversion, controller offsets,
reset timing margin, optional MISO/read reliability, maximum reliable write and
read SPI frequency, PWM polarity/linearity, backlight current, useful minimum,
safe maximum, uniformity, thermal behavior, and long-duration signal integrity.
