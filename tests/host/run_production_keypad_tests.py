#!/usr/bin/env python3
"""Build and run the allocation-free production keypad host suite."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="numos-keypad-") as temp:
    executable = Path(temp) / "production-keypad-test.exe"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Isrc",
            "tests/host/production_keypad_test.cpp",
            "src/input/KeyboardManager.cpp",
            "src/input/KeySemanticResolver.cpp",
            "src/input/ProductionKeypadScanner.cpp",
            "-o",
            str(executable),
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run([str(executable)], cwd=ROOT, check=True)
