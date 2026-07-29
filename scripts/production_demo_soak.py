#!/usr/bin/env python3
"""Run the bounded demo lifecycle soak and record process-level host metrics."""

from __future__ import annotations

import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tests/emulator/scripts/production_demo_soak.numos"


def windows_working_set(pid: int) -> tuple[int, int] | None:
    if os.name != "nt":
        return None

    class ProcessMemoryCounters(ctypes.Structure):
        _fields_ = [
            ("cb", ctypes.c_ulong),
            ("PageFaultCount", ctypes.c_ulong),
            ("PeakWorkingSetSize", ctypes.c_size_t),
            ("WorkingSetSize", ctypes.c_size_t),
            ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
            ("QuotaPagedPoolUsage", ctypes.c_size_t),
            ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
            ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
            ("PagefileUsage", ctypes.c_size_t),
            ("PeakPagefileUsage", ctypes.c_size_t),
        ]

    query_information = 0x0400
    virtual_memory_read = 0x0010
    handle = ctypes.windll.kernel32.OpenProcess(
        query_information | virtual_memory_read, False, pid
    )
    if not handle:
        return None
    try:
        counters = ProcessMemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        if not ctypes.windll.psapi.GetProcessMemoryInfo(
            handle, ctypes.byref(counters), counters.cb
        ):
            return None
        return int(counters.WorkingSetSize), int(counters.PeakWorkingSetSize)
    finally:
        ctypes.windll.kernel32.CloseHandle(handle)


def find_executable() -> Path:
    configured = os.environ.get("PLATFORMIO_BUILD_DIR")
    roots = [
        Path(configured) if configured else Path("C:/.piobuild/numOS"),
        ROOT / ".pio/build",
    ]
    for root in roots:
        for name in ("program.exe", "program"):
            candidate = root / "emulator_pc" / name
            if candidate.is_file():
                return candidate
    raise FileNotFoundError("emulator_pc executable not found; build it first")


def child_environment(executable: Path) -> dict[str, str]:
    environment = dict(os.environ)
    candidates = [
        executable.parent,
        Path(os.environ.get("NUMOS_SDL2_ROOT", "")) / "bin",
        Path(os.environ.get("SDL2_DIR", "")) / "bin",
        Path("C:/SDL2/x86_64-w64-mingw32/bin"),
    ]
    for candidate in candidates:
        if candidate and (candidate / "SDL2.dll").is_file():
            environment["PATH"] = str(candidate) + os.pathsep + environment["PATH"]
            break
    return environment


def run_once(executable: Path, index: int) -> dict[str, object]:
    command = [
        str(executable),
        "--headless",
        "--deterministic",
        "--quiet",
        "--frames",
        "3000",
        "--script",
        str(SCRIPT.relative_to(ROOT)),
        "--fs-sandbox",
    ]
    rss_samples: list[int] = []
    peak_rss_samples: list[int] = []
    started = time.perf_counter()
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as output:
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            env=child_environment(executable),
            stdout=output,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            import psutil  # type: ignore

            monitored = psutil.Process(process.pid)
        except (ImportError, OSError):
            monitored = None
        while process.poll() is None:
            if monitored is not None:
                try:
                    rss_samples.append(monitored.memory_info().rss)
                except OSError:
                    monitored = None
            else:
                memory = windows_working_set(process.pid)
                if memory is not None:
                    rss_samples.append(memory[0])
                    peak_rss_samples.append(memory[1])
            time.sleep(0.02)
        elapsed = time.perf_counter() - started
        output.seek(0)
        text = output.read()
    if process.returncode != 0 or "demo soak: PASS" not in text:
        raise RuntimeError(
            f"soak run {index} failed ({process.returncode})\n{text[-4000:]}"
        )
    return {
        "run": index,
        "cycles": 10,
        "elapsed_seconds": round(elapsed, 3),
        "rss_first_bytes": rss_samples[0] if rss_samples else None,
        "rss_last_bytes": rss_samples[-1] if rss_samples else None,
        "rss_min_bytes": min(rss_samples) if rss_samples else None,
        "rss_max_bytes": max(rss_samples) if rss_samples else None,
        "peak_rss_bytes": max(peak_rss_samples) if peak_rss_samples else (
            max(rss_samples) if rss_samples else None
        ),
        "retained_expression_assertion": "zero-after-each-grapher-exit",
        "event_queue_overflow": "covered-by-production-keypad-suite",
        "psram": "not-available-on-host",
        "reset_reason": "clean-process-exit",
        "last_successful_step": "demo soak: PASS cycles=10",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "out/PROD-DEMO-HARDEN-01/host-soak.json",
    )
    args = parser.parse_args()
    if args.runs < 1 or args.runs > 100:
        parser.error("--runs must be between 1 and 100")

    executable = find_executable()
    runs = [run_once(executable, index + 1) for index in range(args.runs)]
    report = {
        "schema": 1,
        "phase": "PROD-DEMO-HARDEN-01",
        "executable": str(executable),
        "run_count": args.runs,
        "iteration_count": args.runs * 10,
        "elapsed_seconds": round(
            sum(float(run["elapsed_seconds"]) for run in runs), 3
        ),
        "runs": runs,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        f"production demo soak: PASS runs={args.runs} "
        f"cycles={args.runs * 10} report={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
