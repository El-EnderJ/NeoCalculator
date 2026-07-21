# MATH-HW-UX-01 Hardware Acceptance Audit

Date: 2026-07-21  
Target: ESP32-S3 N16R8, device on COM6  
Baseline: `d6d2850` (`main`), initially clean

## Verdict

PASS with one hardware-coverage limitation: Calculation, Grapher, Equations,
Calculus, and NeoLanguage all reached their unconditional Giac paths through
real application navigation and key-event dispatch, painted results on the
ESP32 display path, returned to the launcher safely, and survived a 25-switch
soak without memory drift, reboot, hang, or stale app ownership. The board's
physical matrix is compiled out (`CONNECTED_COLS=0`), so electrical keypad scan
and debounce were not testable; SerialBridge injected the same `KeyEvent` path
used after scanning.

No mathematical semantics, rollback engine, custom CAS behavior, UI design,
renderer geometry, masks, or goldens were changed.

## Method

The `esp32s3_n16r8_hwux` environment extends the production target with a
single acceptance flag. Flag-gated probes surround the real app operations and
record:

- key-to-result latency;
- result-to-first-complete-paint latency by forcing `lv_refr_now()` through the
  synchronous TFT flush path;
- internal heap and PSRAM deltas;
- loop-task stack high-water mark;
- backend, result representation, status, and a privacy-safe result hash.

The probe has no effect in `esp32s3_n16r8`. Expressions and results are never
printed by the probe.

## Representative flows

All times are microseconds. Heap deltas are free bytes after minus before, so a
negative value is a transient/retained allocation at the measurement point.

| App / flow | Backend / result | Result | First paint | Internal | PSRAM | Stack minimum |
|---|---|---:|---:|---:|---:|---:|
| Calculation `2+2` | Giac / structured exact | 24,061 | 61,426 | -264 | -4,372 | 58,428 |
| Calculation `x-2*x` | Giac / structured symbolic | 24,493 | 62,768 | 0 | -1,816 | 58,428 |
| Calculation `0/0` | Giac / undefined | 21,807 | 60,261 | 0 | -700 | 58,428 |
| Calculation `1/0` | Giac / honest text fallback | 22,617 | 68,641 | 0 | -424 | 58,428 |
| Grapher `x^2` | Giac / raster, one valid curve | 92,544 | 100,422 | 0 | -91,536 | 58,428 |
| Equations `x=2` | Giac / structured solve | 68,605 | 118,611 | 0 | -67,360 | 54,284 |
| Calculus derivative `x^2` | Giac / structured | 69,982 | 127,721 | 0 | -67,512 | 58,428 |
| Calculus integral `x` | Giac / structured | 76,730 | 135,152 | 0 | -1,864 | 54,284 |
| Calculus complex/duplicate input | Giac / text fallback | 441,958 | 505,586 | 0 | -748 | 54,284 |
| NeoLanguage `print(2+2)` | Giac selected / math | 14,746 | 55,364 | 0 | -32,900 | 54,284 |

The malformed first Equations attempt was caused by leaving the cursor inside
the exponent template. Its probe correctly reported `aborted`; the valid
`x=2` retry above passed and the navigation mistake was not classified as a
product defect.

## Memory and stability

- Boot: internal heap 147,536 bytes free, PSRAM 8,193,819 bytes free, and
  60,348 bytes minimum free loop-task stack (ESP-IDF reports bytes, unlike
  vanilla FreeRTOS).
- Grapher's expected 91,520-byte Kandinsky PSRAM buffer was released on exit.
- Giac retained approximately 67 KiB after its first solve in the process-wide
  context. This was stable thereafter and is context lifetime, not per-app
  leakage.
- After warm-up, every exit in five complete cycles across all five apps
  returned to exactly 147,032 internal bytes free, 8,053,447 PSRAM bytes free,
  and stack margin 54,284. LVGL returned to 34% after each exit.
- Peak observed LVGL pool use was 67%; no crash, reboot, hang, UAF symptom, or
  progressive heap/PSRAM loss occurred in 25 app switches.

PlatformIO production footprint before acceptance instrumentation was 116,792
bytes static RAM (35.6%) and 5,325,917 bytes flash (81.3%). The instrumented
image was 5,328,837 bytes flash; the 2,920-byte delta is absent when the flag is
off.

After the audit, the flag-off `esp32s3_n16r8` application was restored on COM6.
The launcher then held steady at 147,536 internal bytes free, 8,193,819 PSRAM
bytes free, and 60,252 bytes minimum free stack. An ELF string inspection also
confirmed that the production image contains no `[HWUX]` telemetry format.

## Reproducible defect fixed

SerialBridge accepted only F1/F2 keywords even though the device's compiled-out
matrix left it as the active keypad transport and NeoLanguage requires F5 to
run. F3-F5 and the existing named scientific keys were added as append-only
keyword mappings. This restores access to the same application `KeyEvent` path;
it does not add a backend or alter app behavior.

The ESP Arduino VFS prints an error when `LittleFS.exists("/neolang.nl")` checks
an absent first-run file. NeoLanguage already treats absence as normal, returns
without a UI error, and owns no handle afterward. It was recorded as harmless
framework diagnostic noise and intentionally not changed.

## Evidence

Raw UART captures are in `out/math-hw-ux-01/`:

- `calculation.log`
- `cross-app.log`
- `equations-calculus-retry.log`
- `calculus-integral.log`
- `switch-soak.log`

The available USB camera produced an effectively black frame, so it was not
used as visual evidence. First-paint timing is based on completion of the real,
blocking LVGL-to-TFT flush. Pixel-level/typographic visual acceptance remains
outside this run; no rendering or layout formulas were modified.

A direct full-image deployment using a QIO bootloader header was also observed
to watchdog before application entry on this particular board. Restoring the
bootloader header in DIO mode recovered normal operation, after which the
production app was uploaded with PlatformIO's normal application-only target.
This is a deployment-configuration risk outside the permitted UX/ownership/
rendering/performance fix scope; it did not occur during the acceptance flows.
