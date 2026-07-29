# PROD-DEMO-HARDEN-01

Pre-board software hardening for an unsupervised event build. This phase makes
no physical reliability claim and does not make the demo repository-default.

## 1. Baseline

- Branch: `main`
- Immediate pre-phase HEAD: `58d0baeeae66af4d52ecb22ca9592ba4f67010c9`
- Commit: `feat(display): add safe production display bring-up`
- Clean worktree at capture.
- Production normal: 5,356,701 B flash; 118,960 B static RAM.
- Production bring-up: 5,368,113 B flash; 119,144 B static RAM.
- CAM normal: 5,346,821 B flash; 117,552 B static RAM.
- Normal launcher IDs: 0–10 and 19 — Calculation, Grapher, Equations,
  Calculus, Statistics, Probability, Regression, Sequences, Python, Matrices,
  Settings, and Fractals. IDs 11–18 were constructed but already hidden as
  experimental; ID 20 Math Visual remained compile-time diagnostic-only.
- The launcher owns app selection; LVGL apps are loaded by `SystemApp` and
  teardown is deferred 250 ms after the launcher fade to avoid deleting the
  source screen during animation.
- HOME forced the physical scanner and modifiers clear; BACK previously
  returned directly to the launcher rather than unwinding modal state.
- Scanner force-release emitted held releases but did not clear queued
  repeat/press events.
- The launcher was constructed before filesystem I/O, but normal firmware used
  `LittleFS.begin(true)` and could auto-format on mount failure.
- Reset reason was reported by ESP; the pinned IDF task watchdog was enabled
  with panic at 5 s for idle core 0, while Arduino loop WDT was not subscribed.
- Giac exposes cooperative interrupt flags, but no validated independent timer
  drives them. Synchronous calls can therefore exceed a UI watchdog interval.
- The existing factory target produced a merged normal-production image; CAM
  provisioning and extensive emulator/hardware acceptance scripts already
  existed.

## 2. Demo environment

`numos-esp32-s3-wroom-1u-n16r8-demo` extends the normal production target and
adds `NUMOS_PRODUCTION_DEMO_PROFILE=1`. Board manifest, pins, native USB,
partition table, keypad, display profiles, safe GPIO startup, Giac, and app
sources are inherited. `default_envs` is unchanged.

## 3. Visible allowlist

The centralized `DemoProfile.h` table preserves normal app IDs:

| App | Reason |
|---|---|
| Calculation | Primary event workflow; Giac-authoritative structured evaluation |
| Grapher | Primary event workflow; parse-once retained Giac graph evaluation |
| Equations | Existing bounded Giac solve UI with global recovery |
| Calculus | Existing bounded Giac derivative/integral UI with global recovery |
| Settings | Reduced existing settings surface and operator display recovery |

Safe mode exposes only Calculation and Settings. All other code remains present
for normal firmware but is neither constructed nor shown in demo firmware.

## 4. Deterministic startup

Demo startup never waits for USB, uses the established GPIO ownership sequence,
keeps backlight off through display init and black clear, chooses immutable SAFE
when state is absent/invalid or safe mode is active, initializes keypad after
display ownership, suppresses the splash delay/random content, and acknowledges
health only after a first launcher frame. It logs setup-entry-to-launcher
milliseconds; the web Release smoke measured 40.9 ms to module readiness and
1,817.6 ms from browser launch to the first usable launcher. Those are host
measurements, not PCB timing. Physical observation uses the same
`[BOOT] launcher-usable-ms` serial marker.

No network/radio service is started. LittleFS is mounted only after the launcher
is active.

## 5. HOME/BACK recovery

HOME is intercepted before app routing and force-releases physical and LVGL
input, clears modifiers, and returns to the launcher. BACK asks the active app
to close its topmost modal/state; a root BACK returns one level. The navigation
stack is an enum/state machine, so repeated BACK cannot underflow. Failed app
activation records a bounded failure and falls back to the launcher without
rebooting.

## 6. Key/modifier cleanup

Every app transition clears queued logical events, emits releases only for
actually dispatched held keys, inhibits still-physical holds until release,
resets the LVGL indev, clears repeat timing, and resets SHIFT/ALPHA/STO state.

## 7. Safe mode

An RTC/no-init checksummed record tracks only genuine pre-launcher panic loops
and watchdog resets. Three consecutive failures enter safe mode. Power,
brownout, external, software, and deep-sleep resets clear false accumulation;
ordinary boot does not write NVS. Display-profile quarantine remains the
separate existing mechanism.

Safe mode uses immutable SAFE display values, ignores optional UI state, skips
optional variable/settings load, and shows Calculation plus Settings/recovery.
`DEMO CLEAR SAFE CONFIRM` explicitly clears it.

Safe mode can bypass corrupt optional state, restore SAFE display operation,
format LittleFS after explicit confirmation, and recover to a minimal launcher.
It cannot repair hardware, flash/PSRAM faults, a corrupt application image, an
invalid partition table, or an uninterruptible Giac call.

## 8. Settings/filesystem fault tolerance

Demo variable and settings records are fixed-size, versioned, checksummed, and
bounds-validated. Bad fields or variable slots fall back individually.
Truncated, random, stale-version, checksum-bad, invalid-enum/precision, and
invalid-denominator cases are injected by the host suite. Total corruption is
preserved as `/vars.bad` or `/settings.bad` when possible.

Demo mount uses `LittleFS.begin(false)`: failure is recorded but the active
launcher remains usable. No automatic reformat occurs. Formatting requires safe
mode plus `DEMO FS FORMAT CONFIRM`. Normal firmware retains its pre-phase mount
policy.

## 9. Watchdog coverage and limitation

Demo firmware subscribes the Arduino loop only after launcher readiness and
feeds it only after LVGL and `SystemApp` both return, proving UI-loop progress.
Watchdog reset reason participates in safe-mode policy.

Because the pinned Giac integration lacks a validated independent timeout
driver, the loop WDT is deliberately suspended around synchronous Giac calls
and restored afterward. The policy detects a wedged ordinary UI loop but does
not claim protection or cancellation inside Giac. Structural input limits are
the safe prefilter; unsafe thread termination was not added. CAM is unchanged.

## 10. Calculation/Grapher recovery

The Giac serializer now enforces the existing 40-level structural depth limit
in addition to source/node limits. Errors stay bounded and return to input.
Grapher continues to compile once per expression and never parses per point.
Every function slot, including all retained Giac handles, is reset on app exit
before the destroyed UI can receive a result.

## 11. Hidden diagnostics

The `DEMO` serial surface reports commit/environment/board, reset and failure
health, flash/PSRAM, display profile, keypad layout hash/overflow/raw state,
filesystem, watchdog coverage, modifier state, and launcher time. It supports
SAFE restore, safe-mode control, bounded factory reset, safe-mode-only format,
raw keypad, reboot, and an explicitly started 100-iteration physical soak.
Destructive/state-changing actions require literal `CONFIRM`.

## 12. Factory package

Normal, bring-up, and demo `factory_image` targets each emit a package with:
bootloader, partitions, boot_app0, firmware, optional explicitly reviewed
LittleFS image, 16 MB merged image, SHA256SUMS, deterministic JSON metadata,
commit/source-state hash, offsets, and PowerShell verify/first-flash/app-update/
recovery scripts. The scripts require an explicit port for writes, validate the
hard-coded production board identity and hashes, print BOOT/RESET instructions,
and contain no eFuse or CAM provisioning call.

## 13. Operator runbook

See `PROD-DEMO-HARDEN-01-EVENT-RUNBOOK.md`.

## 14. Scripted acceptance

`production_demo_acceptance.numos` covers boot, Calculation arithmetic,
fraction, power, square root, SHIFT trig, ALPHA variable, DEL/EXE, HOME,
Grapher entry/evaluation/navigation, modal BACK, held-key transition, repeated
switching, all visible apps, and zero retained graph handles. Host companions
inject persistence/mount/app-init policy failures and traverse the physical
Revision C mapping/scanner path.

## 15. Soak

The automatic host soak runs 10 mixed lifecycle cycles per process and records
run count, elapsed time, process memory where available, retained-expression
assertions, queue-overflow coverage, reset outcome, and last step. The hidden
physical soak is opt-in and bounded to 100 iterations.

The completed host run covered 100 cycles in 13.955 s. Peak working set stayed
between 100,642,816 and 101,621,760 B across the ten independent runs (978,944 B
spread, non-monotonic), and every Grapher teardown asserted zero live retained
handles. First/last samples are process-lifecycle observations rather than
firmware heap evidence. Physical minimum heap, PSRAM, LVGL count, and reset
behavior remain board gates.

## 16. Firmware/RAM deltas

Sizes use the linker-map convention established by the preceding production
phases: flash is IRAM vectors + IRAM text + DRAM data + flash text + flash
rodata; static RAM is DRAM data + BSS.

| Environment | Pre-phase flash | Final flash | Delta | Pre-phase RAM | Final RAM | Delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| production normal | 5,356,701 B | 5,357,393 B | +692 B | 118,960 B | 118,976 B | +16 B |
| production bring-up | 5,368,113 B | 5,368,873 B | +760 B | 119,144 B | 119,160 B | +16 B |
| CAM normal | 5,346,821 B | 5,347,493 B | +672 B | 117,552 B | 117,568 B | +16 B |
| CAM validate | 5,380,521 B | 5,381,177 B | +656 B | 117,552 B | 117,568 B | +16 B |
| CAM Giac diagnostic | 5,351,045 B | 5,351,733 B | +688 B | 117,552 B | 117,568 B | +16 B |
| CAM math diagnostic | 5,385,681 B | 5,386,345 B | +664 B | 117,552 B | 117,568 B | +16 B |

The demo environment is new: 5,298,137 B flash, 119,120 B static RAM and
60,407 B total IRAM text. Relative to the final normal production image it is
59,256 B smaller in flash and 144 B larger in static RAM; relative to the
immediate pre-phase normal baseline it is 58,564 B smaller and 160 B larger.

## 17. Regression record

Completed before packaging:

- production normal, bring-up, and demo firmware builds: PASS;
- all four CAM firmware targets (normal, validation, Giac diagnostic, math
  diagnostic): PASS, with no demo macro present;
- production keypad suite: PASS (50 mappings, 2,880 scanner cases, 32 event
  cases, 64 queue cases);
- production display suite and target/manifest/partition validation: PASS;
- `emulator_pc` build and full 179-command/5,000-frame demo acceptance: PASS;
- Neo smoke: PASS (60 commands, 2,400 frames);
- 10-run/100-cycle automatic host soak: PASS;
- emulator web Release and Debug: PASS;
- WASM-MATH Release and Debug: PASS;
- Playwright math, component and persistence suites in Chromium, Firefox and
  WebKit for Release and Debug: PASS;
- web smoke: PASS, including 21 app launch/return cycles and zero live retained
  Grapher handles;
- clean Giac suite: 261/261 PASS (177 engine, 26 calculus, 44 Neo, 14
  cross-application);
- candidate cases: 42/42 PASS;
- accepted goldens: 18/18 PASS;
- key catalog: 79/79 PASS;
- demo host/contract/fault-injection tests and package tamper/wrong-board
  rejection tests: PASS.

No golden or mask was updated. CAM firmware was built and host/web behavior was
exercised, but no CAM hardware runtime or physical soak claim is made because
no board is attached.

## 18. Remaining physical gates

Native-USB enumeration/recovery, boot/backlight electrical behavior, display
orientation/color/inversion/SPI margin, all 50 switches, actual launcher time,
heap/PSRAM physical soak, watchdog reset classification, corrupt-flash recovery,
and two-board repeatability remain board-arrival gates.

## 19. Board-arrival MVP checklist

Use the exact 12-step checklist in the event runbook. No board is promoted on
software-only evidence.

## 20. Staging command

The final report provides a path-exact `git add` command after the worktree is
audited.

## 21. Commit message

`feat(production): harden reproducible event demo profile`
