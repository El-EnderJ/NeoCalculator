"""NumOS keystroke-to-answer oracle harness (CORRECTNESS-E2E-01).

    python tests/e2e/run.py --surface calculation --seed 1 --count 20000 \
        --emulator C:/.piobuild/e2e-01/emulator_pc/program.exe

Outputs (git-ignored) under out/correctness-e2e-01/<surface>/<run-name>/:
results.jsonl (deterministic per seed), findings.json, summary.json,
stats.json (timings), findings/<id>/ (shrunk reproducers).
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
REPO = HERE.parent.parent
DEFAULT_OUT = REPO / "out" / "correctness-e2e-01"


def main(argv=None):
    from common.emulator import DEFAULT_EMULATOR, emulator_sha256
    from common.runner import Runner

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--surface", default="calculation")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--jobs", type=int, default=4, help="parallel emulator processes")
    ap.add_argument("--batch", type=int, default=40, help="cases per emulator process")
    ap.add_argument("--i4-every", type=int, default=2, help="run I4 round trips on every Nth case (0=off)")
    ap.add_argument("--serial", action="store_true", help="one emulator process at a time")
    ap.add_argument("--no-confirm", action="store_true")
    ap.add_argument("--shrink", type=int, default=0, help="shrink up to N distinct confirmed findings")
    ap.add_argument("--out", default=None, help="output directory")
    ap.add_argument("--name", default=None, help="run name (default seed<seed>-n<count>)")
    ap.add_argument("--reshrink", default=None, help="shrink the findings of an existing run directory")
    a = ap.parse_args(argv)
    if a.reshrink:
        from common.runner import load_surface
        from common.shrink import shrink_findings
        d = Path(a.reshrink)
        s = json.loads((d / "summary.json").read_text(encoding="utf-8"))
        surf = load_surface(s["surface"])
        cases = [surf.generate(s["seed"], i) for i in range(s["start"], s["start"] + s["count"])]
        findings = json.loads((d / "findings.json").read_text(encoding="utf-8"))
        shrink_findings(s["surface"], a.emulator, d, findings, cases, {}, limit=a.shrink or 10)
        return 0

    name = a.name or f"seed{a.seed}-n{a.count}" + (f"-s{a.start}" if a.start else "")
    out = Path(a.out) if a.out else DEFAULT_OUT / a.surface / name
    r = Runner(a.surface, a.emulator, out, jobs=a.jobs, batch=a.batch, i4_every=a.i4_every)
    summary, stats, findings, cases, oracles = r.run(a.seed, a.count, start=a.start,
                                                     confirm=not a.no_confirm, parallel=not a.serial)
    stats["emulator_sha256"] = emulator_sha256(a.emulator)
    with open(out / "stats.json", "w", encoding="utf-8", newline="\n") as fh:
        json.dump(stats, fh, sort_keys=True, indent=1)
    if a.shrink and findings:
        from common.shrink import shrink_findings
        shrink_findings(a.surface, a.emulator, out, findings, cases, oracles, limit=a.shrink)
    print(json.dumps(summary, indent=1, sort_keys=True))
    print(f"[run] {stats['cases']} cases in {stats['t_total']:.1f}s "
          f"({stats['cases_per_second_total']:.2f} cases/s total, "
          f"{stats['cases_per_second_emulator_wall']:.2f} cases/s emulator wall) -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
