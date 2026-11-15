"""W1 (Calculation core) entry point for the dedicated runs of CORRECTNESS-E2E-01.

    py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.fmt  --seed 201 --count 3000
    py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.edge --seed 301 --count 3000
    py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.rt   --seed 401 --count 2000

Same outputs as tests/e2e/run.py (results.jsonl, findings.json, summary.json,
stats.json). Differences, all local to tests/e2e/calculation/ (common/ is
shared with other workers and is not modified):

* W1Runner.run_cases runs a surface's optional phase A first (FORMAT surface:
  read the real FORMAT menu choices, attach them to the case dicts) and then
  the normal batches (phase B) — the single-process confirmation re-runs reuse
  the attached choices.
* `--workers` bounds the SymPy oracle/analysis process pools (common.runner
  uses cpu_count-1 = 15; several audit workers share this machine).
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
REPO = HERE.parent.parent
DEFAULT_OUT = REPO / "out" / "correctness-e2e-01" / "calculation"


def make_runner_class():
    from common.runner import Runner, _analyze_job, _analyze_timeout, _oracle_job, _oracle_timeout
    from common.hardpool import map_hard
    from common import runner as R

    class W1Runner(Runner):
        workers = 6

        def oracles(self, cases):
            vals = map_hard(_oracle_job, [(self.name, c) for c in cases], self.workers,
                            R.ORACLE_SECONDS + 15, _oracle_timeout)
            return {c["id"]: v for c, v in zip(cases, vals)}

        def analyze_all(self, cases, runs, oracles):
            jobs = [(self.name, c, runs[c["id"]][0], runs[c["id"]][1], oracles[c["id"]]) for c in cases]
            res = map_hard(_analyze_job, jobs, self.workers, R.ANALYZE_SECONDS + 15, _analyze_timeout)
            return {c["id"]: r for c, r in zip(cases, res)}

        def run_cases(self, cases, oracles, extra=None, parallel=True):
            need_a = [c for c in cases if getattr(self.surf, "TWO_PHASE", False) and "menu" not in c]
            if need_a:
                self.log(f"[w1] phase A for {len(need_a)} case(s)")
                runs_a = super().run_cases(need_a, oracles, None, parallel)
                for c in need_a:
                    by_tag = runs_a[c["id"]][0]
                    m = by_tag.get(f"{c['id']}|menu") or {}
                    c["menu"] = list(m.get("menu_choices") or []) if m.get("format_menu") else []
                    c["phase_a_rc"] = runs_a[c["id"]][2]["rc"]
            return super().run_cases(cases, oracles, extra, parallel)

    return W1Runner


def main(argv=None):
    from common.emulator import DEFAULT_EMULATOR, emulator_sha256
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--surface", default="calculation.fmt")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--batch", type=int, default=40)
    ap.add_argument("--i4-every", type=int, default=1)
    ap.add_argument("--workers", type=int, default=6, help="SymPy oracle/analysis processes")
    ap.add_argument("--no-confirm", action="store_true")
    ap.add_argument("--out", default=None)
    ap.add_argument("--name", default=None)
    a = ap.parse_args(argv)
    W1Runner = make_runner_class()
    W1Runner.workers = a.workers
    name = a.name or f"w1-{a.surface.split('.')[-1]}-seed{a.seed}-n{a.count}" + (f"-s{a.start}" if a.start else "")
    out = Path(a.out) if a.out else DEFAULT_OUT / name
    r = W1Runner(a.surface, a.emulator, out, jobs=a.jobs, batch=a.batch, i4_every=a.i4_every)
    summary, stats, findings, cases, oracles = r.run(a.seed, a.count, start=a.start, confirm=not a.no_confirm)
    stats["emulator_sha256"] = emulator_sha256(a.emulator)
    stats["surface"] = a.surface
    with open(out / "stats.json", "w", encoding="utf-8", newline="\n") as fh:
        json.dump(stats, fh, sort_keys=True, indent=1)
    # keep the phase-A menus with the run (needed to re-run a case by hand)
    menus = {c["id"]: c.get("menu") for c in cases if "menu" in c}
    if menus:
        with open(out / "menus.json", "w", encoding="utf-8", newline="\n") as fh:
            json.dump(menus, fh, sort_keys=True)
    print(json.dumps(summary, indent=1, sort_keys=True))
    print(f"[run] {stats['cases']} cases in {stats['t_total']:.1f}s "
          f"({stats['cases_per_second_total']:.2f} cases/s total) -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
