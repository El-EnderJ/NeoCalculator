#!/usr/bin/env python3
"""Build and run the allocation-free production display profile suite."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="numos-display-") as temp:
    executable = Path(temp) / "production-display-test.exe"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Isrc",
            "tests/host/production_display_profile_test.cpp",
            "src/display/ProductionDisplayProfile.cpp",
            "-o",
            str(executable),
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run([str(executable)], cwd=ROOT, check=True)
