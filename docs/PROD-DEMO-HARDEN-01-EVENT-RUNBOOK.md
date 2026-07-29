# PROD-DEMO-HARDEN-01 Event Runbook

This is the board-day sequence. Do not change firmware, display geometry,
partitions, USB mode, or keypad mapping at the event. No physical reliability
claim exists until the checks below pass on each real board.

## Laptop preparation

1. Copy the complete `out/PROD-DEMO-HARDEN-01` directory to the event laptop.
2. Install the pinned PlatformIO Python environment/esptool and a known data
   USB-C cable. Production uses ESP32-S3 native USB; it has no UART bridge.
3. For each package, run:

   ```powershell
   .\verify-package.ps1
   ```

4. Record the explicit native-USB port for each board. Never guess a COM port.
5. Keep normal, bring-up, and demo package directories together. Do not use
   `scripts/esp32_boot.py`, any CAM package, or any eFuse tool.

## Board A: flash and validate

1. Connect Board A by USB only. Do not connect an unvalidated battery source.
2. From the bring-up package:

   ```powershell
   .\first-flash.ps1 -Port COM7
   ```

3. If ROM download does not start: hold BOOT/K2, tap RESET/K1, release BOOT
   when esptool connects. Tap RESET once after flashing.
4. Confirm: no bright backlight flash; black clear precedes backlight; launcher
   appears without a PC handshake; `DISPLAY INFO` reports this build/board.
5. Run `DISPLAY SAFE`, `DISPLAY TEST`, and check all corners, RGB labels,
   inversion, one-pixel border, text, and stability. Select only a listed
   profile:

   ```text
   DISPLAY PROFILE LIST
   DISPLAY PROFILE SET <listed-name>
   DISPLAY SAVE
   ```

   Reboot twice. If uncertain, use `DISPLAY SAFE` and do not save a guess.
6. Flash the demo package with `first-flash.ps1 -Port COM7`.
7. Run the exact MVP checklist at the end of this document.

## Board B: promote the demo candidate

Use the same hash-verified demo package; do not rebuild:

```powershell
.\first-flash.ps1 -Port COM8
```

Run the complete MVP checklist independently. Board B becomes the backup only
after it passes. Label A/B and their native-USB ports.

## Board C: duplicate a known-good candidate

If a third board is available, flash the same reviewed demo package—not a
read-back of another board:

```powershell
.\first-flash.ps1 -Port COM9
```

This duplicates the known binary while avoiding accidental transfer of corrupt
or attendee-created filesystem state.

## 50-key check

Open the native-USB serial monitor at 115200 and issue:

```text
DEMO KEYPAD RAW ON
```

Press and release SW2 through SW51 once, comparing row/column and logical
behavior with the Revision C sheet. Confirm every key produces both edges,
HOME/BACK/DEL/AC/EXE are correct, SHIFT, ALPHA, and SHIFT+ALPHA work, held-key
repeat is controlled, and overflow remains zero:

```text
DEMO KEYPAD RAW STATUS
DEMO KEYPAD RAW OFF
```

## Recovery

- App/modal stuck: press HOME. Then try BACK from the launcher; it must not
  underflow or reboot.
- Safe display only: `DEMO DISPLAY SAFE CONFIRM`.
- Inspect state: `DEMO INFO`.
- Clear safe mode: `DEMO CLEAR SAFE CONFIRM`, then `DEMO REBOOT CONFIRM`.
- Reset demo variables/settings: `DEMO FACTORY RESET CONFIRM`.
- Format a known-corrupt filesystem only in safe mode and only after preserving
  diagnostics: `DEMO FS FORMAT CONFIRM`.
- App-only update, preserving state:

  ```powershell
  .\app-only-update.ps1 -Port COM7
  ```

- Full recovery, replacing all 16 MB and all state:

  ```powershell
  .\recovery.ps1 -Port COM7 -ConfirmFactoryReset
  ```

Every script verifies package hashes and board identity before writing.

## USB and UART fallback

For native-USB recovery, hold BOOT/K2, tap RESET/K1, and release BOOT after the
ROM port appears. Try the known cable and a direct laptop port before changing
software.

The schematic exposes 3.3 V UART0 TX on TP14/GPIO43 and RX on TP15/GPIO44, but
there is no onboard UART bridge. UART fallback requires a qualified operator,
a 3.3 V USB-UART adapter, crossed TX/RX, and a documented common ground; never
apply 5 V to TP14/TP15. BOOT/K2 and RESET/K1 are still required to enter the ROM
loader. If the ground/power access is not positively identified on the delivered
assembly, stop rather than improvise.

## Optional physical soak

Never run this during attendee use. With the board powered, supervised, and a
serial log open:

```text
DEMO SOAK START CONFIRM
DEMO SOAK STATUS
DEMO SOAK STOP
```

It is bounded to 100 mixed lifecycle iterations, never starts at boot, and
reports elapsed time, heap/PSRAM before/after, minimum heap, keypad overflow,
retained expressions, last step, and reset reason.

## Event handling rules

- Use stable USB power. The production evidence confirms native USB but does
  not validate a battery system; do not show battery percentage or introduce a
  battery/charger at the event.
- Do not save a new display profile after both boards are accepted.
- Do not update apps, packages, libraries, masks, or goldens.
- Do not enable Wi-Fi/Bluetooth, diagnostics autorun, deep sleep, eFuses,
  secure boot, encryption, or CAM provisioning.
- Keep one accepted board powered off as the immediate backup.

## Exact board-arrival MVP checklist

1. Cold boot three times with USB host absent; launcher usable each time.
2. Confirm backlight-off/black-clear startup and the accepted display profile.
3. Confirm all 50 physical keys and zero keypad overflows.
4. Calculation: `2+3`, `1/2+1/3`, power, square root, SHIFT trig, ALPHA variable,
   edit with DEL, EXE, AC.
5. Grapher: enter and graph `y=x^2`; pan/navigate; HOME.
6. Open Equations, Calculus, and Settings; BACK unwinds; HOME always returns.
7. Hold a repeating key, press HOME, release it, and confirm no stuck key or
   modifier after re-entry.
8. Reboot with USB absent and confirm no wait.
9. Confirm `DEMO INFO`: correct commit/environment/board, reset reason, flash,
   PSRAM, display profile, filesystem, safe-mode state, and zero overflow.
10. Simulate recoverable state only on Board A: factory reset, reboot, launcher.
11. Repeat steps 1–9 on Board B. Promote it only after independent success.
12. Record board labels, package SHA256SUMS, ports, display profile, boot time,
    and any failure. Any unexplained reset or drift blocks event promotion.
