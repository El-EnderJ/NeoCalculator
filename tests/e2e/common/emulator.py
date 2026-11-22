"""Emulator process driver for the e2e oracle harness.

Writes a `.numos` script, runs the SDL2 emulator headless and deterministic
in its own filesystem sandbox, and parses the `@E2E {json}` dump lines that
the NativeHal test hooks print (dump_calc / dump_graph / dump_calculus /
dump_equations / giac_eval).

Every run gets a fresh, private sandbox directory, so no state (vars.dat,
toolbox favourites, settings) leaks between runs or parallel workers.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

DEFAULT_EMULATOR = "C:/.piobuild/e2e-01/emulator_pc/program.exe"
PATH_PREFIX = ["C:/mingw64/bin", "C:/SDL2/x86_64-w64-mingw32/bin"]

# Frames the launcher needs before `open_app` is accepted (splash ~2 s at
# 16 ms/frame) plus the app-open settle. Measured: 200 + 60 is what every
# existing semantic smoke uses; the harness keeps the same proven margin.
BOOT_WAIT = 200
OPEN_WAIT = 60


def _env() -> dict:
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(PATH_PREFIX + [env.get("PATH", "")])
    env.pop("NUMOS_CALC_INPUT_TRACE", None)
    return env


@dataclass
class RunResult:
    dumps: list[dict]
    returncode: int
    stdout: str
    stderr: str
    script: str
    frames: int
    seconds: float = 0.0
    by_tag: dict = field(default_factory=dict)

    def __post_init__(self):
        for d in self.dumps:
            self.by_tag.setdefault(d.get("tag"), d)


def parse_dumps(stdout: str) -> list[dict]:
    out = []
    for line in stdout.splitlines():
        if line.startswith("@E2E "):
            try:
                out.append(json.loads(line[5:]))
            except json.JSONDecodeError as exc:  # pragma: no cover - diagnostic
                out.append({"tag": None, "parse_error": str(exc), "raw": line})
    return out


def count_frames(lines: list[str]) -> int:
    frames = 0
    for line in lines:
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        frames += 1
        if s.startswith("wait "):
            frames += int(s.split()[1])
    return frames


def script_header(app: str) -> list[str]:
    return [f"wait {BOOT_WAIT}", f"open_app {app}", f"wait {OPEN_WAIT}"]


def run_script(lines: list[str], emulator: str = DEFAULT_EMULATOR,
               workdir: str | None = None, keep: bool = False,
               timeout: float = 600.0, extra_frames: int = 50) -> RunResult:
    """Run one emulator process for `lines` (a full script incl. header)."""
    import time
    frames = count_frames(lines) + extra_frames
    base = Path(workdir) if workdir else Path(tempfile.gettempdir())
    base.mkdir(parents=True, exist_ok=True)
    run_dir = Path(tempfile.mkdtemp(prefix="e2e_", dir=str(base)))
    script_path = run_dir / "case.numos"
    sandbox = run_dir / "fs"
    sandbox.mkdir()
    text = "\n".join(lines) + "\n"
    script_path.write_text(text, encoding="utf-8", newline="\n")
    cmd = [emulator, "--headless", "--deterministic", "--quiet", "--frames", str(frames),
           "--script", str(script_path), "--fs-sandbox-dir", str(sandbox)]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, cwd=str(run_dir), env=_env(), capture_output=True,
                              timeout=timeout)
        rc, so, se = proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired as exc:
        rc, so, se = -999, exc.stdout or b"", (exc.stderr or b"") + b"\n[e2e] TIMEOUT"
    dt = time.perf_counter() - t0
    stdout = so.decode("utf-8", "replace")
    stderr = se.decode("utf-8", "replace")
    if not keep:
        shutil.rmtree(run_dir, ignore_errors=True)
    return RunResult(parse_dumps(stdout), rc, stdout, stderr, text, frames, dt)


def emulator_sha256(path: str = DEFAULT_EMULATOR) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()
