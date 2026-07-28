#!/usr/bin/env python3
"""Host validation for the isolated production WROOM-1U target."""

from __future__ import annotations

import csv
import json
import os
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
PROD_BOARD = ROOT / "boards" / "numos-esp32-s3-wroom-1u-n16r8.json"
CAM_BOARD = ROOT / "boards" / "numos-esp32-s3-n16r8-cam.json"
PLATFORMIO_INI = ROOT / "platformio.ini"
MAIN_CPP = ROOT / "src" / "main.cpp"
BRINGUP_CPP = ROOT / "src" / "hardware" / "ProductionBringup.cpp"
FLASH_BYTES = 16 * 1024 * 1024


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def find_partition_table(name: str) -> Path:
    roots = []
    configured = os.environ.get("PLATFORMIO_CORE_DIR")
    if configured:
        roots.append(Path(configured))
    roots.extend((Path.home() / ".platformio", Path("C:/.platformio")))
    for core in roots:
        candidate = (
            core
            / "packages"
            / "framework-arduinoespressif32"
            / "tools"
            / "partitions"
            / name
        )
        if candidate.is_file():
            return candidate
    raise AssertionError(f"cannot locate pinned framework partition table {name}")


def parse_size(value: str) -> int:
    return int(value.strip(), 0)


def validate_partitions(path: Path) -> None:
    regions: list[tuple[str, int, int]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if not row or row[0].lstrip().startswith("#"):
                continue
            require(len(row) >= 5, f"malformed partition row: {row}")
            label = row[0].strip()
            offset = parse_size(row[3])
            size = parse_size(row[4])
            regions.append((label, offset, size))

    expected = {
        "nvs": (0x009000, 0x005000),
        "otadata": (0x00E000, 0x002000),
        "app0": (0x010000, 0x640000),
        "app1": (0x650000, 0x640000),
        "spiffs": (0xC90000, 0x360000),
        "coredump": (0xFF0000, 0x010000),
    }
    require({label for label, _, _ in regions} == set(expected),
            "unexpected N16R8 partition set")
    for label, offset, size in regions:
        require((offset, size) == expected[label],
                f"{label} partition differs from validated N16R8 layout")
        require(offset + size <= FLASH_BYTES,
                f"{label} extends beyond 16 MB flash")

    ordered = sorted(regions, key=lambda item: item[1])
    for left, right in zip(ordered, ordered[1:]):
        require(left[1] + left[2] <= right[1],
                f"partition overlap: {left[0]} and {right[0]}")
    require(ordered[-1][1] + ordered[-1][2] == FLASH_BYTES,
            "partition map must end exactly at 16 MB")


def section(text: str, name: str) -> str:
    match = re.search(
        rf"^\[{re.escape(name)}\]\s*$([\s\S]*?)(?=^\[|\Z)",
        text,
        flags=re.MULTILINE,
    )
    require(match is not None, f"missing PlatformIO section [{name}]")
    return match.group(1)


def main() -> int:
    prod = json.loads(PROD_BOARD.read_text(encoding="utf-8"))
    cam = json.loads(CAM_BOARD.read_text(encoding="utf-8"))
    build = prod["build"]
    arduino = build["arduino"]
    upload = prod["upload"]
    hardware = prod["numos_hardware_contract"]

    require(prod["name"] != cam["name"], "production/CAM names must differ")
    require("CAM" not in prod["name"] and "CH343" not in prod["name"],
            "production manifest contains CAM/bridge identity")
    require(build["mcu"] == "esp32s3", "production MCU must be ESP32-S3")
    require(arduino["memory_type"] == "qio_opi",
            "production memory type must be qio_opi")
    require(build["boot"] == "qio" and build["flash_mode"] == "dio",
            "pinned Arduino QIO boot/runtime contract changed")
    require(build["f_flash"] == "80000000L", "flash frequency must be 80 MHz")
    require(upload["flash_size"] == "16MB", "manifest must report 16 MB flash")
    require(upload["maximum_size"] == 0x640000,
            "maximum firmware size must equal app0 size")
    require(hardware["module"] == "ESP32-S3-WROOM-1U-N16R8",
            "exact production module identity missing")
    require(hardware["external_flash_bytes"] == FLASH_BYTES,
            "explicit flash capacity metadata is wrong")
    require(hardware["external_psram_bytes"] == 8 * 1024 * 1024,
            "manifest must report 8 MB PSRAM")
    require(hardware["psram_bus"] == "OPI" and
            hardware["psram_frequency_hz"] == 80_000_000,
            "manifest must report pinned 80 MHz OPI PSRAM")
    require(not hardware["onboard_uart_bridge"],
            "production manifest must not claim an onboard UART bridge")
    flags = set(build["extra_flags"])
    for flag in (
        "-DNUMOS_BOARD_PROD_WROOM1U_N16R8=1",
        "-DARDUINO_USB_MODE=1",
        "-DARDUINO_USB_CDC_ON_BOOT=1",
        "-DBOARD_HAS_PSRAM",
    ):
        require(flag in flags, f"missing board flag {flag}")
    require(["0x303A", "0x1001"] in build["hwids"],
            "native ESP USB Serial/JTAG VID/PID missing")

    ini = PLATFORMIO_INI.read_text(encoding="utf-8")
    normal = section(ini, "env:numos-esp32-s3-wroom-1u-n16r8")
    bringup = section(ini, "env:numos-esp32-s3-wroom-1u-n16r8-bringup")
    require("board      = numos-esp32-s3-wroom-1u-n16r8" in normal,
            "normal environment does not select production manifest")
    require("extends = env:esp32s3_n16r8" not in normal,
            "production environment inherits CAM")
    require("-DARDUINO_USB_MODE\n" not in normal,
            "production environment strips its required USB mode")
    require("-DNUMOS_SERIAL_BACKEND_USB_CDC=1" in normal,
            "production serial backend is not native USB CDC")
    require("-DTFT_MISO=42" in normal and "-DSPI_FREQUENCY=10000000" in normal,
            "production display contract missing")
    require("extends = env:numos-esp32-s3-wroom-1u-n16r8" in bringup,
            "bring-up must extend the normal production environment")
    require("-DNUMOS_PRODUCTION_BRINGUP=1" in bringup,
            "bring-up instrumentation flag missing")

    main_cpp = MAIN_CPP.read_text(encoding="utf-8")
    bringup_cpp = BRINGUP_CPP.read_text(encoding="utf-8")
    require("waitForProductionBringupSerial();" in main_cpp,
            "bring-up serial wait seam is not wired")
    require(
        re.search(
            r"#if NUMOS_BOARD_PROD_WROOM1U_N16R8\s+"
            r"// Normal production boot never waits[\s\S]*?"
            r"#if defined\(NUMOS_PRODUCTION_BRINGUP\)\s+"
            r"numos::hardware::waitForProductionBringupSerial\(\);\s+"
            r"#endif\s+#else",
            main_cpp,
        )
        is not None,
        "production Serial wait is not exclusively gated by bring-up",
    )
    require("kMaximumSerialWaitMs = 3000U" in bringup_cpp,
            "bring-up Serial wait must be bounded to three seconds")
    require("serviceProductionBringupReporting();" in main_cpp,
            "late USB connection reporting seam is missing")
    require("g_reportDeliveredToConnectedHost || !NUMOS_SERIAL" in bringup_cpp,
            "late USB report is not one-shot and non-blocking")

    partition_path = find_partition_table(arduino["partitions"])
    validate_partitions(partition_path)
    print(f"production target manifest: PASS ({PROD_BOARD.name})")
    print(f"partition overlap/offset validation: PASS ({partition_path})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, ValueError) as error:
        print(f"production target validation: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
