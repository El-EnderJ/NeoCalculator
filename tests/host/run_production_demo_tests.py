#!/usr/bin/env python3
"""Build and run the bounded production-demo host suite."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="numos-demo-") as temp:
    temp_path = Path(temp)
    executable = temp_path / "production-demo-test.exe"
    fs_root = temp_path / "fs"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-ffunction-sections",
            "-fdata-sections",
            "-DNUMOS_PRODUCTION_DEMO_PROFILE=1",
            "-Isrc",
            "tests/host/production_demo_test.cpp",
            "src/demo/DemoBootHealth.cpp",
            "src/input/KeyboardManager.cpp",
            "src/input/ProductionKeypadScanner.cpp",
            "src/math/VariableManager.cpp",
            "src/hal/FileSystem.cpp",
            "-Wl,--gc-sections",
            "-o",
            str(executable),
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run([str(executable), str(fs_root)], cwd=ROOT, check=True)

subprocess.run(
    ["python", "tests/host/production_demo_contract_test.py"],
    cwd=ROOT,
    check=True,
)
