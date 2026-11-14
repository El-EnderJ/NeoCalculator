"""Isolation, determinism and throughput check for the batch runner.

Runs the same seeded batch (a) serially, (b) again serially and (c) with N
parallel emulator processes, each process in its own filesystem sandbox, and
compares results.jsonl / findings.json byte for byte.

    python tests/e2e/isolation.py --seed 11 --count 400 --jobs 4
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
OUT = HERE.parent.parent / "out" / "correctness-e2e-01" / "isolation"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    from common.emulator import DEFAULT_EMULATOR
    from common.runner import Runner

    ap = argparse.ArgumentParser()
    ap.add_argument("--surface", default="calculation")
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--count", type=int, default=400)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--batch", type=int, default=40)
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    a = ap.parse_args()
    report = {"seed": a.seed, "count": a.count, "batch": a.batch, "runs": {}}
    for name, jobs, parallel in (("serial-1", 1, False), ("serial-2", 1, False), (f"parallel-{a.jobs}", a.jobs, True)):
        out = OUT / f"{a.surface}-seed{a.seed}-n{a.count}" / name
        r = Runner(a.surface, a.emulator, out, jobs=jobs, batch=a.batch)
        summary, stats, *_ = r.run(a.seed, a.count, parallel=parallel)
        report["runs"][name] = {
            "results_sha256": sha(out / "results.jsonl"), "findings_sha256": sha(out / "findings.json"),
            "t_total": round(stats["t_total"], 2), "t_emulator_wall": round(stats["t_emulator_wall"], 2),
            "cases_per_second_total": round(stats["cases_per_second_total"], 2),
            "cases_per_second_emulator_wall": round(stats["cases_per_second_emulator_wall"], 2),
            "processes": stats["processes"], "frames": stats["frames"]}
        print(name, json.dumps(report["runs"][name]), flush=True)
    hashes = {(v["results_sha256"], v["findings_sha256"]) for v in report["runs"].values()}
    report["identical"] = len(hashes) == 1
    (OUT / f"{a.surface}-seed{a.seed}-n{a.count}" / "isolation.json").write_text(json.dumps(report, indent=1))
    print("IDENTICAL" if report["identical"] else "DIFFERENT", flush=True)


if __name__ == "__main__":
    main()
