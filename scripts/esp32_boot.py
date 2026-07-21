#!/usr/bin/env python3
"""Build, validate, package, and flash the official NumOS ESP32-S3 image.

ESP32-S3 ROM must load the second-stage bootloader through a DIO image header.
That second stage is built for QIO flash and OPI PSRAM.  This tool keeps those
two phases distinct and refuses artifacts which violate the contract.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PRODUCTION_ENV = "esp32s3_n16r8"
BOARD_ID = "numos-esp32-s3-n16r8-cam"
BOARD_MANIFEST = PROJECT_ROOT / "boards" / f"{BOARD_ID}.json"
DEFAULT_OUTPUT = PROJECT_ROOT / "out" / "esp32-boot-01" / "package"

CHIP = "esp32s3"
FLASH_SIZE = 16 * 1024 * 1024
FLASH_FREQ_HZ = 80_000_000
FLASH_FREQ_NAME = "80m"
FLASH_MODE_DIO = 2
FLASH_MODE_NAMES = {0: "qio", 1: "qout", 2: "dio", 3: "dout"}
IMAGE_MAGIC = 0xE9
PARTITION_MAGIC = 0x50AA
PARTITION_MD5_MAGIC = 0xEBEB
PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SECTOR_SIZE = 0x1000

EXPECTED_FLASH_MANUFACTURER = "68"
EXPECTED_FLASH_DEVICE = "4018"
EXPECTED_PSRAM_TEXT = "Embedded PSRAM 8MB"

DIAGNOSTIC_DEFINES = {
    "NUMOS_GIAC_DIAGNOSTICS",
    "NUMOS_MATH_ENGINE_DIAGNOSTICS",
    "NUMOS_MATH_VISUAL_VERIFY",
    "NUMOS_MATH_RENDER_TRACE_ONCE",
    "NUMOS_HW_UX_ACCEPTANCE",
    "NUMOS_GIAC_FEAS_NOGIAC",
}


class ContractError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def run_capture(command: Sequence[str], cwd: Path = PROJECT_ROOT) -> str:
    completed = subprocess.run(
        list(command),
        cwd=str(cwd),
        check=False,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if completed.returncode != 0:
        raise ContractError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stdout}"
        )
    return completed.stdout


def run_stream(command: Sequence[str], cwd: Path = PROJECT_ROOT) -> None:
    print("+", subprocess.list2cmdline(list(command)), flush=True)
    completed = subprocess.run(list(command), cwd=str(cwd), check=False)
    if completed.returncode != 0:
        raise ContractError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
        )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_value(*args: str) -> str:
    return run_capture(["git", *args]).strip()


def parse_json_output(output: str) -> Dict[str, Any]:
    start = output.find("{")
    require(start >= 0, "PlatformIO did not emit JSON metadata")
    try:
        value = json.loads(output[start:])
    except json.JSONDecodeError as error:
        raise ContractError(f"invalid PlatformIO JSON metadata: {error}") from error
    require(isinstance(value, dict), "PlatformIO metadata root is not an object")
    return value


def project_metadata() -> Dict[str, Any]:
    output = run_capture(
        ["pio", "project", "metadata", "-e", PRODUCTION_ENV, "--json-output"]
    )
    metadata = parse_json_output(output)
    require(PRODUCTION_ENV in metadata, f"missing metadata for {PRODUCTION_ENV}")
    return metadata[PRODUCTION_ENV]


def configured_board_id() -> str:
    text = (PROJECT_ROOT / "platformio.ini").read_text(encoding="utf-8")
    match = re.search(
        rf"(?ms)^\[env:{re.escape(PRODUCTION_ENV)}\]\s*(.*?)^(?:\[|\Z)", text
    )
    require(match is not None, f"missing [env:{PRODUCTION_ENV}]")
    board = re.search(r"(?m)^\s*board\s*=\s*([^;\s]+)", match.group(1))
    require(board is not None, f"{PRODUCTION_ENV} does not select a board")
    return board.group(1)


def parse_hz(value: Any) -> int:
    text = str(value).strip().rstrip("Ll")
    return int(text, 0)


def validate_board_contract() -> Dict[str, Any]:
    require(configured_board_id() == BOARD_ID, "production environment uses wrong board")
    require(BOARD_MANIFEST.is_file(), f"missing board manifest: {BOARD_MANIFEST}")
    board = json.loads(BOARD_MANIFEST.read_text(encoding="utf-8"))
    build = board.get("build", {})
    arduino = build.get("arduino", {})
    upload = board.get("upload", {})

    require(build.get("mcu") == CHIP, "board MCU must be esp32s3")
    require(build.get("flash_mode") == "dio", "ROM image header mode must be DIO")
    require(build.get("boot") == "qio", "second-stage bootloader must enable QIO")
    require(
        arduino.get("memory_type") == "qio_opi",
        "Arduino memory type must be qio_opi",
    )
    require(parse_hz(build.get("f_flash", 0)) == FLASH_FREQ_HZ, "flash must be 80 MHz")
    require(upload.get("flash_size") == "16MB", "flash size must be 16 MB")
    require(
        arduino.get("partitions") == "default_16MB.csv",
        "partition table must remain default_16MB.csv",
    )
    require(build.get("filesystem") == "littlefs", "filesystem must be LittleFS")
    return board


def c_string(data: bytes) -> str:
    return data.split(b"\0", 1)[0].decode("utf-8", errors="replace")


def inspect_image(path: Path) -> Dict[str, Any]:
    require(path.is_file(), f"missing image: {path}")
    data = path.read_bytes()
    require(len(data) >= 24, f"image is too short: {path}")
    require(data[0] == IMAGE_MAGIC, f"invalid image magic in {path.name}")
    mode = data[2]
    params = data[3]
    size_code = params >> 4
    freq_code = params & 0x0F
    size_names = {
        0: "1MB",
        1: "2MB",
        2: "4MB",
        3: "8MB",
        4: "16MB",
        5: "32MB",
        6: "64MB",
        7: "128MB",
    }
    freq_names = {0: "40m", 1: "26m", 2: "20m", 0xF: "80m"}
    return {
        "magic": f"0x{data[0]:02x}",
        "segments": data[1],
        "flash_mode": FLASH_MODE_NAMES.get(mode, f"unknown:{mode}"),
        "flash_mode_byte": mode,
        "flash_size": size_names.get(size_code, f"unknown:{size_code}"),
        "flash_frequency": freq_names.get(freq_code, f"unknown:{freq_code}"),
        "entry_point": f"0x{struct.unpack_from('<I', data, 4)[0]:08x}",
    }


def inspect_app_identity(path: Path) -> Dict[str, Any]:
    data = path.read_bytes()
    if len(data) < 208 or struct.unpack_from("<I", data, 0x20)[0] != 0xABCD5432:
        return {"descriptor": "not-found"}
    return {
        "secure_version": struct.unpack_from("<I", data, 0x24)[0],
        "version": c_string(data[0x30:0x50]),
        "project_name": c_string(data[0x50:0x70]),
        "build_time": c_string(data[0x70:0x80]),
        "build_date": c_string(data[0x80:0x90]),
        "idf_version": c_string(data[0x90:0xB0]),
        "elf_sha256": data[0xB0:0xD0].hex(),
    }


def validate_image_contract(path: Path, role: str) -> Dict[str, Any]:
    image = inspect_image(path)
    require(image["segments"] > 0, f"{role} has no loadable segments")
    require(
        image["flash_mode_byte"] == FLASH_MODE_DIO,
        f"{role} header is {image['flash_mode']}; ESP32-S3 ROM contract requires DIO",
    )
    require(image["flash_size"] == "16MB", f"{role} does not declare 16 MB flash")
    require(
        image["flash_frequency"] == FLASH_FREQ_NAME,
        f"{role} does not declare 80 MHz flash",
    )
    return image


def parse_partitions(path: Path) -> List[Dict[str, Any]]:
    data = path.read_bytes()
    require(len(data) <= PARTITION_TABLE_SECTOR_SIZE, "partition table exceeds one sector")
    entries: List[Dict[str, Any]] = []
    for position in range(0, len(data) - 31, 32):
        magic = struct.unpack_from("<H", data, position)[0]
        if magic in (0xFFFF, PARTITION_MD5_MAGIC):
            break
        require(magic == PARTITION_MAGIC, f"bad partition magic at 0x{position:x}")
        _, ptype, subtype, offset, size, raw_label, flags = struct.unpack_from(
            "<HBBII16sI", data, position
        )
        entries.append(
            {
                "label": c_string(raw_label),
                "type": ptype,
                "subtype": subtype,
                "offset": offset,
                "size": size,
                "end": offset + size,
                "flags": flags,
            }
        )
    require(entries, "partition table has no entries")

    ordered = sorted(entries, key=lambda item: item["offset"])
    for previous, current in zip(ordered, ordered[1:]):
        require(
            previous["end"] <= current["offset"],
            f"partitions overlap: {previous['label']} and {current['label']}",
        )
    require(ordered[-1]["end"] <= FLASH_SIZE, "partition table exceeds 16 MB flash")
    return entries


def partition_by_label(partitions: Iterable[Dict[str, Any]], label: str) -> Dict[str, Any]:
    for partition in partitions:
        if partition["label"] == label:
            return partition
    raise ContractError(f"missing {label} partition")


def validate_region(path: Path, offset: int, limit: int, role: str) -> Dict[str, Any]:
    require(path.is_file(), f"missing {role}: {path}")
    size = path.stat().st_size
    require(size > 0, f"empty {role}: {path}")
    require(size <= limit, f"{role} is {size} bytes but limit is {limit}")
    require(offset + size <= FLASH_SIZE, f"{role} exceeds physical flash")
    return {"offset": offset, "size": size, "end": offset + size}


def validate_non_overlapping(regions: List[Tuple[str, int, int]]) -> None:
    ordered = sorted(regions, key=lambda item: item[1])
    for previous, current in zip(ordered, ordered[1:]):
        require(
            previous[2] <= current[1],
            f"payloads overlap: {previous[0]} and {current[0]}",
        )


def production_artifacts(metadata: Dict[str, Any]) -> Dict[str, Path]:
    require(metadata.get("env_name") == PRODUCTION_ENV, "metadata is not production")
    defines = {str(value).split("=", 1)[0] for value in metadata.get("defines", [])}
    enabled_diagnostics = sorted(defines & DIAGNOSTIC_DEFINES)
    require(
        not enabled_diagnostics,
        "production metadata contains diagnostics: " + ", ".join(enabled_diagnostics),
    )

    elf = Path(metadata["prog_path"])
    result = {
        "elf": elf,
        "application": elf.with_suffix(".bin"),
    }
    flash_images = metadata.get("extra", {}).get("flash_images", [])
    wanted = {0x0000: "bootloader", 0x8000: "partitions", 0xE000: "boot_app0"}
    for image in flash_images:
        offset = int(str(image["offset"]), 0)
        if offset in wanted:
            result[wanted[offset]] = Path(image["path"])
    require(int(metadata.get("extra", {}).get("application_offset", "0"), 0) == 0x10000,
            "application offset must be 0x10000")
    for role in ("elf", "bootloader", "partitions", "boot_app0", "application"):
        require(role in result and result[role].is_file(), f"missing {role} artifact")
    return result


def component_record(
    filename: str,
    source: Path,
    offset: int,
    region: Dict[str, Any],
    header: Optional[Dict[str, Any]] = None,
    partition: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    record: Dict[str, Any] = {
        "name": filename,
        "offset": f"0x{offset:08x}",
        "size": source.stat().st_size,
        "end": f"0x{region['end']:08x}",
        "sha256": sha256(source),
    }
    if header is not None:
        record["image_header"] = header
    if partition is not None:
        record["partition"] = {
            "label": partition["label"],
            "offset": f"0x{partition['offset']:08x}",
            "size": partition["size"],
            "margin": partition["size"] - source.stat().st_size,
        }
    return record


def build_package(args: argparse.Namespace) -> Path:
    board = validate_board_contract()
    if not args.skip_build:
        run_stream(["pio", "run", "-e", PRODUCTION_ENV])
    metadata = project_metadata()
    artifacts = production_artifacts(metadata)
    partitions = parse_partitions(artifacts["partitions"])
    app0 = partition_by_label(partitions, "app0")
    otadata = partition_by_label(partitions, "otadata")
    filesystem = partition_by_label(partitions, "spiffs")

    boot_header = validate_image_contract(artifacts["bootloader"], "bootloader")
    app_header = validate_image_contract(artifacts["application"], "application")

    regions: Dict[str, Dict[str, Any]] = {
        "bootloader.bin": validate_region(
            artifacts["bootloader"], 0x0000, PARTITION_TABLE_OFFSET, "bootloader"
        ),
        "partitions.bin": validate_region(
            artifacts["partitions"],
            PARTITION_TABLE_OFFSET,
            PARTITION_TABLE_SECTOR_SIZE,
            "partition table",
        ),
        "boot_app0.bin": validate_region(
            artifacts["boot_app0"], otadata["offset"], otadata["size"], "boot_app0"
        ),
        "firmware.bin": validate_region(
            artifacts["application"], app0["offset"], app0["size"], "application"
        ),
    }

    payloads: List[Tuple[str, Path, int, Dict[str, Any], Optional[Dict[str, Any]]]] = [
        ("bootloader.bin", artifacts["bootloader"], 0x0000, regions["bootloader.bin"], None),
        ("partitions.bin", artifacts["partitions"], PARTITION_TABLE_OFFSET,
         regions["partitions.bin"], None),
        ("boot_app0.bin", artifacts["boot_app0"], otadata["offset"],
         regions["boot_app0.bin"], otadata),
        ("firmware.bin", artifacts["application"], app0["offset"],
         regions["firmware.bin"], app0),
    ]

    if args.littlefs_image is not None:
        littlefs = Path(args.littlefs_image).resolve()
        fs_region = validate_region(
            littlefs, filesystem["offset"], filesystem["size"], "LittleFS image"
        )
        regions["littlefs.bin"] = fs_region
        payloads.append(("littlefs.bin", littlefs, filesystem["offset"], fs_region, filesystem))

    validate_non_overlapping(
        [(name, region["offset"], region["end"]) for name, _, _, region, _ in payloads]
    )

    output = Path(args.output_dir).resolve()
    require(output != PROJECT_ROOT, "package output cannot be the repository root")
    output.mkdir(parents=True, exist_ok=True)

    records: List[Dict[str, Any]] = []
    merged_payloads: List[Tuple[Path, int]] = []
    for name, source, offset, region, partition in payloads:
        destination = output / name
        shutil.copy2(source, destination)
        header = None
        if name == "bootloader.bin":
            header = boot_header
        elif name == "firmware.bin":
            header = app_header
        records.append(component_record(name, destination, offset, region, header, partition))
        merged_payloads.append((destination, offset))

    merged_name = "numos-esp32s3-n16r8-factory-recovery.bin"
    merged_path = output / merged_name
    merged = bytearray(b"\xFF") * FLASH_SIZE
    for source, offset in merged_payloads:
        data = source.read_bytes()
        merged[offset:offset + len(data)] = data
    merged_path.write_bytes(merged)
    merged_header = validate_image_contract(merged_path, "merged recovery image")

    git_status = git_value("status", "--short")
    app_identity = inspect_app_identity(output / "firmware.bin")
    manifest = {
        "schema": 1,
        "task": "ESP32-BOOT-01",
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "environment": PRODUCTION_ENV,
        "board": BOARD_ID,
        "git": {
            "branch": git_value("branch", "--show-current"),
            "head": git_value("rev-parse", "--short", "HEAD"),
            "dirty": bool(git_status),
        },
        "hardware_contract": {
            "chip": CHIP,
            "flash_size": FLASH_SIZE,
            "rom_header_mode": "dio",
            "runtime_flash_mode": "qio",
            "flash_frequency_hz": FLASH_FREQ_HZ,
            "psram_mode": "opi",
            "psram_frequency_hz": FLASH_FREQ_HZ,
            "psram_size": 8 * 1024 * 1024,
            "arduino_memory_type": board["build"]["arduino"]["memory_type"],
        },
        "build_identity": {
            "application": app_identity,
            "elf_size": artifacts["elf"].stat().st_size,
            "elf_sha256": sha256(artifacts["elf"]),
            "diagnostic_defines_present": [],
        },
        "filesystem_policy": (
            "provided image" if args.littlefs_image is not None
            else "partition erased; production firmware initializes LittleFS on first boot"
        ),
        "partitions": [
            {
                "label": item["label"],
                "type": item["type"],
                "subtype": item["subtype"],
                "offset": f"0x{item['offset']:08x}",
                "size": item["size"],
                "end": f"0x{item['end']:08x}",
            }
            for item in partitions
        ],
        "components": records,
        "merged_image": {
            "name": merged_name,
            "offset": "0x00000000",
            "size": merged_path.stat().st_size,
            "sha256": sha256(merged_path),
            "image_header": merged_header,
        },
    }
    manifest_path = output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"validated production package: {output}")
    print(f"manifest: {manifest_path}")
    print(f"application margin: {app0['size'] - artifacts['application'].stat().st_size} bytes")
    return manifest_path


def load_and_verify_package(package_dir: Path) -> Dict[str, Any]:
    validate_board_contract()
    manifest_path = package_dir / "manifest.json"
    require(manifest_path.is_file(), f"missing package manifest: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest.get("environment") == PRODUCTION_ENV, "package is not production")
    require(manifest.get("board") == BOARD_ID, "package board does not match")
    require(
        not manifest.get("build_identity", {}).get("diagnostic_defines_present"),
        "package contains a diagnostic application",
    )
    contract = manifest.get("hardware_contract", {})
    require(contract.get("rom_header_mode") == "dio", "package ROM mode is not DIO")
    require(contract.get("runtime_flash_mode") == "qio", "package runtime mode is not QIO")
    require(contract.get("psram_mode") == "opi", "package PSRAM mode is not OPI")
    require(contract.get("flash_size") == FLASH_SIZE, "package flash size is not 16 MB")
    require(
        contract.get("flash_frequency_hz") == FLASH_FREQ_HZ,
        "package flash frequency is not 80 MHz",
    )
    require(contract.get("psram_size") == 8 * 1024 * 1024, "package PSRAM size is not 8 MB")
    require(
        contract.get("psram_frequency_hz") == FLASH_FREQ_HZ,
        "package PSRAM frequency is not 80 MHz",
    )
    require(
        contract.get("arduino_memory_type") == "qio_opi",
        "package Arduino memory type is not qio_opi",
    )

    records = manifest.get("components", [])
    require(isinstance(records, list), "package component list is invalid")
    by_name = {record.get("name"): record for record in records}
    require(len(by_name) == len(records), "package contains duplicate component names")
    required = {"bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin"}
    allowed = required | {"littlefs.bin"}
    require(required <= set(by_name), "package is missing required production binaries")
    require(set(by_name) <= allowed, "package contains an unexpected payload")

    partition_path = package_dir / "partitions.bin"
    require(partition_path.is_file(), "missing package component: partitions.bin")
    partitions = parse_partitions(partition_path)
    app0 = partition_by_label(partitions, "app0")
    otadata = partition_by_label(partitions, "otadata")
    filesystem = partition_by_label(partitions, "spiffs")
    expected_offsets = {
        "bootloader.bin": 0x0000,
        "partitions.bin": PARTITION_TABLE_OFFSET,
        "boot_app0.bin": otadata["offset"],
        "firmware.bin": app0["offset"],
        "littlefs.bin": filesystem["offset"],
    }
    limits = {
        "bootloader.bin": PARTITION_TABLE_OFFSET,
        "partitions.bin": PARTITION_TABLE_SECTOR_SIZE,
        "boot_app0.bin": otadata["size"],
        "firmware.bin": app0["size"],
        "littlefs.bin": filesystem["size"],
    }

    regions: List[Tuple[str, int, int]] = []
    for record in records:
        path = package_dir / record["name"]
        require(path.is_file(), f"missing package component: {record['name']}")
        offset = int(record.get("offset", "-1"), 0)
        require(
            offset == expected_offsets[record["name"]],
            f"offset mismatch: {record['name']}",
        )
        require(path.stat().st_size == record["size"], f"size mismatch: {record['name']}")
        require(sha256(path) == record["sha256"], f"hash mismatch: {record['name']}")
        region = validate_region(path, offset, limits[record["name"]], record["name"])
        require(record.get("end") == f"0x{region['end']:08x}", f"end mismatch: {record['name']}")
        regions.append((record["name"], offset, region["end"]))
        if record["name"] in ("bootloader.bin", "firmware.bin"):
            validate_image_contract(path, record["name"])
    validate_non_overlapping(regions)

    merged_record = manifest.get("merged_image", {})
    merged = package_dir / merged_record.get("name", "")
    require(merged.is_file(), "missing merged recovery image")
    require(merged.stat().st_size == FLASH_SIZE, "merged image is not exactly 16 MB")
    require(sha256(merged) == merged_record.get("sha256"), "merged image hash mismatch")
    validate_image_contract(merged, "merged recovery image")
    expected_merged = bytearray(b"\xFF") * FLASH_SIZE
    for record in records:
        offset = int(record["offset"], 0)
        payload = (package_dir / record["name"]).read_bytes()
        expected_merged[offset:offset + len(payload)] = payload
    require(merged.read_bytes() == expected_merged, "merged image does not match components")
    return manifest


def platformio_core_dir() -> Path:
    output = run_capture(["pio", "system", "info"])
    match = re.search(r"(?m)^PlatformIO Core Directory\s+(.+?)\s*$", output)
    require(match is not None, "cannot locate PlatformIO core directory")
    return Path(match.group(1).strip())


def esptool_command() -> List[str]:
    explicit = os.environ.get("ESPTOOL")
    if explicit:
        return [explicit]
    executable = shutil.which("esptool") or shutil.which("esptool.py")
    if executable:
        return [executable]
    script = platformio_core_dir() / "packages" / "tool-esptoolpy" / "esptool.py"
    require(script.is_file(), "cannot locate esptool; install the PlatformIO espressif32 platform")
    return [sys.executable, str(script)]


def target_probe(port: str, baud: int) -> str:
    base = [*esptool_command(), "--chip", CHIP, "--port", port, "--baud", str(baud)]
    flash_id = run_capture([*base, "flash_id"])
    require("Chip is ESP32-S3" in flash_id, "connected target is not ESP32-S3")
    require(EXPECTED_PSRAM_TEXT in flash_id, "target does not report embedded 8 MB PSRAM")
    require("Detected flash size: 16MB" in flash_id, "target flash is not 16 MB")
    require(
        f"Manufacturer: {EXPECTED_FLASH_MANUFACTURER}" in flash_id
        and f"Device: {EXPECTED_FLASH_DEVICE}" in flash_id,
        "target JEDEC flash identity is not the validated 0x68:0x4018 device",
    )
    security = run_capture([*base, "get_security_info"])
    require("Secure Boot: Disabled" in security, "secure-boot state is not explicitly disabled; stop")
    require(
        "Flash Encryption: Disabled" in security,
        "flash-encryption state is not explicitly disabled; stop",
    )
    print(flash_id, end="")
    print(security, end="")
    return flash_id + security


def flash_full(args: argparse.Namespace) -> None:
    require(args.confirm_erase, "full provisioning requires --confirm-erase")
    package_dir = Path(args.package_dir).resolve()
    manifest = load_and_verify_package(package_dir)
    target_probe(args.port, args.baud)
    merged = package_dir / manifest["merged_image"]["name"]
    base = [
        *esptool_command(), "--chip", CHIP, "--port", args.port,
        "--baud", str(args.baud), "--before", "default_reset",
    ]
    run_stream([*base, "--after", "no_reset", "erase_flash"])
    run_stream(
        [
            *base, "--after", "hard_reset", "write_flash",
            "--flash_mode", "keep", "--flash_freq", "keep",
            "--flash_size", "keep", "0x0", str(merged),
        ]
    )


def flash_application(args: argparse.Namespace) -> None:
    package_dir = Path(args.package_dir).resolve()
    manifest = load_and_verify_package(package_dir)
    target_probe(args.port, args.baud)
    application_record = next(
        (item for item in manifest["components"] if item["name"] == "firmware.bin"), None
    )
    require(application_record is not None, "package has no production application")
    require(application_record["offset"] == "0x00010000", "application offset mismatch")
    firmware = package_dir / "firmware.bin"
    run_stream(
        [
            *esptool_command(), "--chip", CHIP, "--port", args.port,
            "--baud", str(args.baud), "--before", "default_reset",
            "--after", "hard_reset", "write_flash", "--flash_mode", "keep",
            "--flash_freq", "keep", "--flash_size", "keep", "0x10000",
            str(firmware),
        ]
    )


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subcommands = result.add_subparsers(dest="command", required=True)

    package = subcommands.add_parser("package", help="build and create a guarded recovery package")
    package.add_argument("--skip-build", action="store_true", help="validate existing production artifacts")
    package.add_argument("--littlefs-image", help="optional LittleFS image to include")
    package.add_argument("--output-dir", default=str(DEFAULT_OUTPUT))
    package.set_defaults(handler=build_package)

    verify = subcommands.add_parser("verify", help="verify an existing package without flashing")
    verify.add_argument("--package-dir", default=str(DEFAULT_OUTPUT))
    def verify_package(args: argparse.Namespace) -> None:
        package_dir = Path(args.package_dir).resolve()
        load_and_verify_package(package_dir)
        print(f"validated package: {package_dir}")

    verify.set_defaults(handler=verify_package)

    probe = subcommands.add_parser("probe", help="read target identity and security state")
    probe.add_argument("--port", required=True)
    probe.add_argument("--baud", type=int, default=921600)
    probe.set_defaults(handler=lambda args: target_probe(args.port, args.baud))

    full = subcommands.add_parser("flash-full", help="erase and flash the validated recovery image")
    full.add_argument("--port", required=True)
    full.add_argument("--baud", type=int, default=921600)
    full.add_argument("--package-dir", default=str(DEFAULT_OUTPUT))
    full.add_argument("--confirm-erase", action="store_true")
    full.set_defaults(handler=flash_full)

    application = subcommands.add_parser("flash-app", help="flash only production firmware at 0x10000")
    application.add_argument("--port", required=True)
    application.add_argument("--baud", type=int, default=921600)
    application.add_argument("--package-dir", default=str(DEFAULT_OUTPUT))
    application.set_defaults(handler=flash_application)
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        args.handler(args)
        return 0
    except ContractError as error:
        print(f"ESP32-BOOT-01 REJECTED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
