# ESP32-S3 Provisioning and Recovery

This document is the release and recovery authority for the physical NumOS
ESP32-S3 target. The authoritative machine-readable hardware contract is
`boards/numos-esp32-s3-n16r8-cam.json`; every firmware environment inherits it
through `esp32s3_n16r8`.

## Validated hardware

The July 2026 physical prototype reports:

| Item | Validated value |
|---|---|
| Carrier | ESP32-S3 N16R8 CAM-class board with CH343 USB-UART bridge |
| USB bridge | VID:PID `1a86:55d3` |
| MCU/package | ESP32-S3, QFN56, revision 0.2, ESP32-S3R8 class |
| Flash | external 16 MB QSPI, JEDEC manufacturer `0x68`, device `0x4018`, 3.3 V |
| Flash eFuse | four data lines; default SPI pads; no remapping |
| PSRAM | in-package 8 MB AP Memory 3.3 V octal PSRAM |
| Security | secure boot disabled; flash encryption disabled; download mode enabled |
| CPU/flash clock | 240 MHz / 80 MHz |

Board/module silkscreen is a release input: record it with the unit evidence.
Do not substitute a board merely because its listing says “N16R8”. Stop if its
MCU package, flash ID/capacity, PSRAM identity, voltage, or security state does
not match the probe above.

## The two-stage flash contract

The correct contract is:

- ROM boot header: **DIO, 80 MHz, 16 MB**;
- software bootloader/runtime flash: **QIO, 80 MHz**;
- PSRAM: **OPI, 80 MHz, 8 MB**;
- Arduino memory type: `qio_opi`.

These values are not contradictory. ESP32-S3 ROM does not support loading the
software bootloader through a QIO or QOUT header. It first loads a DIO image;
the QIO-configured second-stage bootloader identifies the flash, enables quad
mode, and maps the application in QIO mode. PlatformIO therefore builds
`bootloader_qio_80m.elf` and wraps it with `elf2image --flash_mode dio`.

The former board manifest declared `flash_mode = qio`. PlatformIO silently
normalized that declaration to a DIO image header for ordinary builds and
uploads. A manually merged full image overrode the normalized header back to
QIO. On the physical S3 this loaded the first ROM segment and failed at
`ets_loader.c:78`, before the second-stage entry point, followed by TG0 and RTC
watchdog resets. Application-only writes appeared healthy because they did not
replace the working DIO boot header.

The board manifest now expresses both phases independently:

```text
build.flash_mode          = dio       # ROM-visible image header
build.boot                = qio       # second-stage bootloader variant
build.arduino.memory_type = qio_opi   # runtime flash + PSRAM libraries
```

Never pass `--flash-mode qio` while merging or writing an image at offset zero.

## Artifact layout

| Offset | Payload | Bound |
|---:|---|---:|
| `0x00000000` | `bootloader.bin` | before `0x8000` |
| `0x00008000` | `partitions.bin` | one 4 KiB sector |
| `0x0000e000` | `boot_app0.bin` / initial OTA data | 8 KiB `otadata` partition |
| `0x00010000` | production `firmware.bin` | 6,400 KiB `app0` partition |
| `0x00c90000` | optional `littlefs.bin` | 3,456 KiB filesystem partition |

The 16 MB partition layout remains unchanged: two 6,400 KiB OTA application
slots, a 3,456 KiB filesystem partition, and a 64 KiB coredump partition.

## Build and package

From the repository root:

```sh
python scripts/esp32_boot.py package
python scripts/esp32_boot.py verify
```

The first command builds only `esp32s3_n16r8`. It rejects diagnostic or
acceptance defines, missing files, a non-DIO bootloader/application header,
wrong size/frequency fields, overlapping payloads, partition overflow, or an
application larger than `app0`. It writes only ignored artifacts under
`out/esp32-boot-01/package/`:

- the four component binaries;
- optional `littlefs.bin`;
- `numos-esp32s3-n16r8-factory-recovery.bin`;
- `manifest.json`, with offsets, sizes, SHA-256 hashes, Git/build identity,
  image headers, partition margins, and the flash/PSRAM contract.

To package an intentional filesystem image:

```sh
python scripts/esp32_boot.py package --littlefs-image path/to/littlefs.bin
```

Without one, the merged image leaves the filesystem partition erased. Current
product policy is `LittleFS.begin(true)`: first boot formats and initializes it.
For recovery which must preserve state, first read and supply an exact valid
filesystem image; do not label an erased-filesystem image state-preserving.

## Read-only target probe

Before provisioning a new unit:

```sh
python scripts/esp32_boot.py probe --port <PORT>
```

The probe checks the ESP32-S3 identity, embedded 8 MB PSRAM, 16 MB flash,
validated JEDEC ID, secure-boot state, and flash-encryption state. It writes no
eFuses. The flash commands repeat this probe and stop on a mismatch.

## Application-only development upload

Build/package first, then write only `app0`:

```sh
python scripts/esp32_boot.py flash-app --port <PORT>
```

This writes only `firmware.bin` at `0x10000`; it does not touch the bootloader,
partition table, OTA selector, NVS, or LittleFS. The standard PlatformIO
`upload` target is not the official application-only path: with this Arduino
platform it appends the bootloader, partition table, and `boot_app0` to the
esptool command.

## Clean provisioning and full recovery

Both an erased-chip factory provision and recovery use the same validated
package:

```sh
python scripts/esp32_boot.py package
python scripts/esp32_boot.py flash-full --port <PORT> --confirm-erase
```

`flash-full` verifies all package hashes and headers, repeats the target and
security probe, erases the chip, and writes the merged 16 MB image at offset
zero using `--flash-mode keep`. `--confirm-erase` is deliberately mandatory.
No command in this workflow burns an eFuse.

To restore normal firmware after any diagnostic application:

```sh
python scripts/esp32_boot.py package
python scripts/esp32_boot.py flash-app --port <PORT>
```

Never package or leave `esp32s3_n16r8_mathdiag`, `esp32s3_n16r8_giacdiag`,
`esp32s3_n16r8_validate`, or `esp32s3_n16r8_hwux` as production firmware. The
packager accepts only the base production environment and rejects their defines.

## Reset, power, USB, and expected boot markers

The validated carrier exposes UART0 through its CH343 USB bridge. Production
firmware deliberately uses `UART0/CH343 Mode`; native ESP32-S3 USB CDC is
disabled in the base environment and remains an opt-in validation environment.
The CH343 port must disappear on USB power removal, enumerate again on power
restore, and carry both ROM and NumOS output at 115200 baud.

A healthy reset or cold power-up contains:

```text
ESP-ROM:esp32s3-20210327
mode:DIO, clock div:1
load:0x3fce3808,...
load:0x403c9700,...
load:0x403cc700,...
entry 0x403c98d0
>>> NumOS: System Ready (UART0/CH343 Mode)
[PSRAM] 8183 KB libres
[SYSTEM] LittleFS OK, ...
[GUI] Initial scroll to App ID 0: Done.
[BOOT] OK
```

`mode:DIO` is the expected ROM phase; it does not mean the application is
running with dual-I/O flash. `clock div:1` is 80 MHz.

## Failure diagnosis

| Last marker | Failure stage | Action |
|---|---|---|
| no ROM output | power, UART bridge, reset/strap, or wrong port | check power, CH343 enumeration, GPIO0/GPIO46, and 115200 baud |
| `mode:QIO` then `ets_loader.c 78` | unsupported QIO ROM header | discard the image; rebuild/package; recover with the DIO-header image |
| one `load:` line then watchdog loop | ROM image loading | inspect byte 2 and validation hash of the bootloader image |
| all three `load:` lines but no `entry` | bootloader checksum/segment problem | verify bootloader hash and offset zero |
| `entry` but no NumOS marker | second stage or application mapping | verify partition/app offsets and application header/hash |
| NumOS marker but no PSRAM line | OPI PSRAM configuration | verify `qio_opi`, 8 MB eFuse identity, and 3.3 V |
| PSRAM works but LittleFS fails | filesystem payload/policy | verify partition offset/size; allow first-boot format only when data loss is acceptable |
| launcher works but USB serial vanishes | bridge cable/power/driver or UART backend mismatch | verify CH343 enumeration and production UART0 flags |

Stop rather than experimenting if security is enabled, identities differ,
behavior is nondeterministic, or correct DIO boot configuration breaks PSRAM,
LittleFS, USB serial, or the launcher.

## Release validation

For a physical release candidate, retain complete command and serial logs and
perform three erase/full-flash cycles, three cold power cycles after each,
three application-only updates, and one deliberate erased-flash recovery. On
each run verify DIO ROM loading, QIO-configured second-stage/application entry,
16 MB flash, 8 MB OPI PSRAM, LittleFS policy, CH343 reconnect, launcher, `2+2`,
and a Grapher expression. Finish with the normal production application.
