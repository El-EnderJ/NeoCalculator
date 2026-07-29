#!/usr/bin/env python3
"""Static and executable contracts not covered by the narrow C++ host suite."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
EXPECTED_BOARD = "numos-esp32-s3-wroom-1u-n16r8"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def section(text: str, name: str) -> str:
    match = re.search(
        rf"^\[{re.escape(name)}\]\s*$([\s\S]*?)(?=^\[|\Z)",
        text,
        flags=re.MULTILINE,
    )
    require(match is not None, f"missing [{name}]")
    return match.group(1)


def test_profile_isolation() -> None:
    ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    demo = section(ini, "env:numos-esp32-s3-wroom-1u-n16r8-demo")
    require(
        "extends = env:numos-esp32-s3-wroom-1u-n16r8" in demo,
        "demo does not inherit production",
    )
    require(
        "-DNUMOS_PRODUCTION_DEMO_PROFILE=1" in demo,
        "demo capability absent",
    )
    require(
        "numos-esp32-s3-wroom-1u-n16r8-demo"
        not in section(ini, "platformio"),
        "demo became a default environment",
    )
    for name in (
        "env:esp32s3_n16r8",
        "env:esp32s3_n16r8_validate",
        "env:esp32s3_n16r8_validate_overlay",
        "env:esp32s3_n16r8_validate_sup1",
        "env:emulator_pc",
        "env:emulator_pc_neo_smoke",
    ):
        require(
            "NUMOS_PRODUCTION_DEMO_PROFILE" not in section(ini, name),
            f"demo capability leaked into {name}",
        )
    for relative in ("CMakeLists.txt", "tests/wasm/CMakeLists.txt"):
        path = ROOT / relative
        if path.is_file():
            require(
                "NUMOS_PRODUCTION_DEMO_PROFILE=1"
                not in path.read_text(encoding="utf-8"),
                f"demo capability leaked into {relative}",
            )

    settings = (ROOT / "src/apps/SettingsApp.cpp").read_text(encoding="utf-8")
    require(
        "#if NUMOS_PRODUCTION_DEMO_PROFILE" in settings
        and '#include "../demo/DemoSettingsRecord.h"' in settings
        and "#elif defined(__EMSCRIPTEN__)" in settings
        and "SETTINGS_FORMAT_VERSION = 1" in settings,
        "demo settings format must not replace normal WASM persistence",
    )


def test_recovery_and_watchdog_contracts() -> None:
    system = (ROOT / "src/SystemApp.cpp").read_text(encoding="utf-8")
    require(
        "if (ev.code == KeyCode::HOME)" in system
        and "clearTransitionInput();" in system
        and "_keypad.forceReleaseAll();" in system
        and "vpam::KeyboardManager::instance().reset();" in system,
        "global escape/input cleanup contract incomplete",
    )
    require(
        "if (!unwindTopmostDemoState()) returnToMenu();" in system,
        "BACK topmost-state policy absent",
    )
    require(
        re.search(
            r"LittleFS\.begin\(\s*#if NUMOS_PRODUCTION_DEMO_PROFILE\s*"
            r"false\s*#else\s*true\s*#endif\s*\)",
            system,
        )
        is not None,
        "demo no-format mount or unchanged normal mount missing",
    )

    diagnostics = (ROOT / "src/demo/DemoDiagnostics.cpp").read_text(
        encoding="utf-8"
    )
    for command in (
        "DISPLAY SAFE CONFIRM",
        "SAFE ON CONFIRM",
        "CLEAR SAFE CONFIRM",
        "FACTORY RESET CONFIRM",
        "FS FORMAT CONFIRM",
        "REBOOT CONFIRM",
    ):
        require(command in diagnostics, f"confirmation missing for {command}")

    giac = (ROOT / "src/math/giac/GiacEngine.cpp").read_text(encoding="utf-8")
    liveness = (ROOT / "src/demo/DemoLiveness.cpp").read_text(encoding="utf-8")
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    require(
        "struct CallGuard" in giac
        and "disableLoopWDT" in liveness
        and "enableLoopWDT" in liveness,
        "bounded Giac watchdog exclusion seam missing",
    )
    require(
        "noteUiLoopProgress();" in main,
        "watchdog is not fed from UI progress",
    )
    calculation = (ROOT / "src/math/CalculationEngine.cpp").read_text(
        encoding="utf-8"
    )
    require(
        "kMaxTreeDepth" in calculation and "expression nesting too deep" in calculation,
        "math structural depth bound missing",
    )


def write_sums(root: Path, names: list[str]) -> None:
    lines = []
    for name in names:
        digest = hashlib.sha256((root / name).read_bytes()).hexdigest()
        lines.append(f"{digest}  {name}\n")
    (root / "SHA256SUMS").write_text("".join(lines), encoding="ascii")


def run_verifier(root: Path, expect_success: bool) -> None:
    shell = shutil.which("pwsh") or shutil.which("powershell")
    require(shell is not None, "PowerShell is required for package verification")
    result = subprocess.run(
        [
            shell,
            "-NoProfile",
            "-File",
            str(root / "verify-package.ps1"),
            "-PackageRoot",
            str(root),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    require(
        (result.returncode == 0) == expect_success,
        "package verifier result mismatch:\n" + result.stdout + result.stderr,
    )


def test_package_hash_and_board_rejection() -> None:
    with tempfile.TemporaryDirectory(prefix="numos-package-contract-") as temp:
        root = Path(temp)
        for name in ("PackageCommon.ps1", "verify-package.ps1"):
            shutil.copyfile(ROOT / "scripts/event" / name, root / name)
        (root / "firmware.bin").write_bytes(b"bounded-test-image")
        metadata = {
            "schema": 1,
            "environment": "numos-esp32-s3-wroom-1u-n16r8-demo",
            "profile": "demo",
            "board_identifier": EXPECTED_BOARD,
            "flash_bytes": 16 * 1024 * 1024,
            "source": {
                "commit": "0" * 40,
                "tree_dirty": False,
                "source_state_sha256": "0" * 64,
            },
        }
        (root / "build-metadata.json").write_text(
            json.dumps(metadata), encoding="utf-8"
        )
        hash_names = [
            "PackageCommon.ps1",
            "build-metadata.json",
            "firmware.bin",
            "verify-package.ps1",
        ]
        write_sums(root, hash_names)
        run_verifier(root, True)

        (root / "firmware.bin").write_bytes(b"tampered")
        run_verifier(root, False)

        (root / "firmware.bin").write_bytes(b"bounded-test-image")
        metadata["board_identifier"] = "esp32s3_n16r8_cam"
        (root / "build-metadata.json").write_text(
            json.dumps(metadata), encoding="utf-8"
        )
        write_sums(root, hash_names)
        run_verifier(root, False)


def main() -> int:
    test_profile_isolation()
    test_recovery_and_watchdog_contracts()
    test_package_hash_and_board_rejection()
    print("production demo contract suite: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"production demo contract suite: FAIL: {error}")
        raise SystemExit(1)
