"""Build a reproducible, production-only 16 MB factory package.

The target composes artifacts from the pinned Arduino/PlatformIO build.  It
does not probe hardware, erase flash, alter image headers, burn eFuses, or
reuse any CAM provisioning contract.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

Import("env")  # noqa: F821 - supplied by PlatformIO/SCons


FLASH_BYTES = 16 * 1024 * 1024
BOARD_IDENTIFIER = "numos-esp32-s3-wroom-1u-n16r8"
PROFILES = {
    "numos-esp32-s3-wroom-1u-n16r8": "normal",
    "numos-esp32-s3-wroom-1u-n16r8-bringup": "bringup",
    "numos-esp32-s3-wroom-1u-n16r8-demo": "demo",
}
FLASH_COMPONENTS = (
    ("bootloader.bin", 0x000000, 0x008000),
    ("partitions.bin", 0x008000, 0x001000),
    ("boot_app0.bin", 0x00E000, 0x002000),
    ("firmware.bin", 0x010000, 0x640000),
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_output(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=repo,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def source_state(repo: Path) -> tuple[str, bool, str]:
    commit = git_output(repo, "rev-parse", "HEAD")
    status = git_output(repo, "status", "--porcelain=v1", "--untracked-files=all")
    tracked_diff = subprocess.run(
        ["git", "diff", "--binary", "HEAD"],
        cwd=repo,
        check=True,
        capture_output=True,
    ).stdout
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
        cwd=repo,
        check=True,
        capture_output=True,
    ).stdout.split(b"\0")
    state_digest = hashlib.sha256(tracked_diff)
    for relative_bytes in sorted(path for path in untracked if path):
        relative = relative_bytes.decode("utf-8")
        state_digest.update(b"\0UNTRACKED\0")
        state_digest.update(relative_bytes)
        state_digest.update(b"\0")
        state_digest.update((repo / relative).read_bytes())
    return commit, bool(status), state_digest.hexdigest()


def merge_factory_image(source, target, env):
    pioenv = env.subst("$PIOENV")
    if pioenv not in PROFILES:
        raise RuntimeError(
            f"factory package rejects non-production environment: {pioenv}"
        )

    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))
    framework_dir = Path(
        env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    )
    esptool_dir = Path(env.PioPlatform().get_package_dir("tool-esptoolpy"))
    profile = PROFILES[pioenv]
    output = build_dir / f"numos-production-{profile}-factory.bin"
    package_dir = build_dir / "factory-package"
    package_dir.mkdir(parents=True, exist_ok=True)

    inputs = {
        "bootloader.bin": build_dir / "bootloader.bin",
        "partitions.bin": build_dir / "partitions.bin",
        "boot_app0.bin": (
            framework_dir / "tools" / "partitions" / "boot_app0.bin"
        ),
        "firmware.bin": build_dir / f"{env.subst('$PROGNAME')}.bin",
    }
    for name, offset, bound in FLASH_COMPONENTS:
        path = inputs[name]
        if not path.is_file():
            raise RuntimeError(f"missing factory-image component: {path}")
        if path.stat().st_size > bound:
            raise RuntimeError(
                f"{name} size {path.stat().st_size} exceeds "
                f"0x{bound:x} bound at 0x{offset:x}"
            )
    reviewed_path = None
    reviewed_fs = os.environ.get("NUMOS_REVIEWED_FS_IMAGE", "").strip()
    if reviewed_fs:
        reviewed_path = Path(reviewed_fs).resolve()
        if not reviewed_path.is_file():
            raise RuntimeError(
                f"NUMOS_REVIEWED_FS_IMAGE does not exist: {reviewed_path}"
            )
        if reviewed_path.stat().st_size > 0x360000:
            raise RuntimeError("reviewed filesystem image exceeds 0x360000 bytes")

    command = [
        sys.executable,
        str(esptool_dir / "esptool.py"),
        "--chip",
        "esp32s3",
        "merge_bin",
        "--output",
        str(output),
        "--flash_mode",
        "keep",
        "--flash_freq",
        "keep",
        "--flash_size",
        "keep",
        "--fill-flash-size",
        "16MB",
    ]
    for name, offset, _ in FLASH_COMPONENTS:
        command.extend((hex(offset), str(inputs[name])))
    if reviewed_path is not None:
        command.extend(("0xc90000", str(reviewed_path)))
    subprocess.run(command, check=True)
    if output.stat().st_size != FLASH_BYTES:
        raise RuntimeError(
            f"factory image is {output.stat().st_size} bytes, expected "
            f"{FLASH_BYTES}"
        )
    # Preserve the established normal-production filename for existing users.
    if profile == "normal":
        shutil.copyfile(
            output, build_dir / "numos-production-pcba-factory.bin"
        )

    copied: list[Path] = []
    for name, _, _ in FLASH_COMPONENTS:
        destination = package_dir / name
        shutil.copyfile(inputs[name], destination)
        copied.append(destination)
    factory_destination = package_dir / "factory-16mb.bin"
    shutil.copyfile(output, factory_destination)
    copied.append(factory_destination)

    filesystem = None
    if reviewed_path is not None:
        filesystem_path = package_dir / "littlefs-reviewed.bin"
        shutil.copyfile(reviewed_path, filesystem_path)
        copied.append(filesystem_path)
        filesystem = {
            "filename": filesystem_path.name,
            "offset": "0xc90000",
            "reviewed": True,
            "source_sha256": sha256(reviewed_path),
        }

    commit, dirty, diff_hash = source_state(project_dir)
    metadata = {
        "schema": 1,
        "phase": "PROD-DEMO-HARDEN-01",
        "environment": pioenv,
        "profile": profile,
        "board_identifier": BOARD_IDENTIFIER,
        "module": "ESP32-S3-WROOM-1U-N16R8",
        "flash_bytes": FLASH_BYTES,
        "source": {
            "commit": commit,
            "tree_dirty": dirty,
            "source_state_sha256": diff_hash,
        },
        "components": [
            {
                "filename": name,
                "offset": f"0x{offset:x}",
                "maximum_bytes": bound,
                "actual_bytes": (package_dir / name).stat().st_size,
                "sha256": sha256(package_dir / name),
            }
            for name, offset, bound in FLASH_COMPONENTS
        ],
        "factory_image": {
            "filename": factory_destination.name,
            "offset": "0x0",
            "actual_bytes": FLASH_BYTES,
            "sha256": sha256(factory_destination),
        },
        "filesystem_image": filesystem,
    }
    metadata_path = package_dir / "build-metadata.json"
    metadata_path.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    copied.append(metadata_path)

    operator_scripts = (
        "PackageCommon.ps1",
        "first-flash.ps1",
        "app-only-update.ps1",
        "recovery.ps1",
        "verify-package.ps1",
    )
    event_dir = project_dir / "scripts" / "event"
    for name in operator_scripts:
        source_script = event_dir / name
        if not source_script.is_file():
            raise RuntimeError(f"missing operator script: {source_script}")
        destination = package_dir / name
        shutil.copyfile(source_script, destination)
        copied.append(destination)
    runbook_source = (
        project_dir / "docs" / "PROD-DEMO-HARDEN-01-EVENT-RUNBOOK.md"
    )
    if not runbook_source.is_file():
        raise RuntimeError(f"missing event runbook: {runbook_source}")
    runbook_destination = package_dir / "EVENT-RUNBOOK.md"
    shutil.copyfile(runbook_source, runbook_destination)
    copied.append(runbook_destination)

    sums_path = package_dir / "SHA256SUMS"
    hash_targets = sorted(copied, key=lambda path: path.name.lower())
    sums_path.write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in hash_targets),
        encoding="ascii",
        newline="\n",
    )

    print(
        f"Production {profile} factory package: {package_dir} "
        f"({FLASH_BYTES} byte merged image)"
    )


env.AddCustomTarget(
    name="factory_image",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=merge_factory_image,
    title="Production factory package",
    description="Merge and hash the production ESP32-S3 16 MB factory package",
)
