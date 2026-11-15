"""W3 Grapher entry point: the shared runner + residual capture.

Same CLI as tests/e2e/run.py (always --surface grapher). The shared runner
only hands `@E2E` dumps to the surface; implicit/inequality residuals are
printed by `assert_graph_eval_near` PASS lines (after a `log R|case|slot|x|y`
marker), so this wrapper parses them into by_tag["__residuals__"] =
{case_id: {(slot, x, y): value | None}} (None = NaN; the FAIL stops the
script). common/ is not modified: the override is a subclass hook installed
at import time in this process only.

py -3.12 tests/e2e/grapher/run_w3.py --seed S --count N --jobs 3 --batch 25 --case-timeout 10 \
    --emulator C:/.piobuild/e2e-01/emulator_pc/program.exe --out out/correctness-e2e-01/grapher/<run>
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))

from common import runner as _runner  # noqa: E402
from common.emulator import run_script  # noqa: E402

_PASS = re.compile(r"PASS - assert_graph_eval_near \d+ \([^)]*\) = (\S+)")


def parse_residual_stream(stdout: str, stderr: str) -> dict:
    out, pending = {}, None
    for line in stdout.splitlines():
        if line.startswith("[SCRIPT] R|"):
            _, cid, slot, x, y = line[len("[SCRIPT] "):].split("|")
            pending = (cid, (int(slot), float(x), float(y)))
            continue
        m = _PASS.search(line)
        if m and pending:
            out.setdefault(pending[0], {})[pending[1]] = float(m.group(1))
            pending = None
    if pending and "FAIL - assert_graph_eval_near" in stderr:
        out.setdefault(pending[0], {})[pending[1]] = None
    return out


def _run_lines(self, lines, timeout):
    r = run_script(lines, self.emulator, workdir=self.workdir, timeout=timeout)
    self.stats["processes"] += 1
    self.stats["emulator_seconds"] += r.seconds
    self.stats["frames"] += r.frames
    r.by_tag["__residuals__"] = parse_residual_stream(r.stdout, r.stderr)
    return r


_runner.Runner._run_lines = _run_lines

if __name__ == "__main__":
    import run as _run  # tests/e2e/run.py
    argv = sys.argv[1:]
    if "--surface" not in argv:
        argv = ["--surface", "grapher"] + argv
    sys.exit(_run.main(argv))
