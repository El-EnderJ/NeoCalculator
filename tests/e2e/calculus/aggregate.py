"""Run table for the Calculus surface: per run directory, counts of cases,
executed keystroke paths, families/ops/angles, throughput, confirmed finding
classes (cases per class), skips and unresolved desyncs.

    python tests/e2e/calculus/aggregate.py RUN_DIR [RUN_DIR ...] [--json OUT]
"""
from __future__ import annotations

import argparse
import collections
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

from calculus.findings import class_key  # noqa: E402


def one(rd):
    rd = Path(rd)
    summ = json.loads((rd / "summary.json").read_text(encoding="utf-8"))
    stats = json.loads((rd / "stats.json").read_text(encoding="utf-8"))
    fnd = json.loads((rd / "findings.json").read_text(encoding="utf-8"))
    paths = collections.Counter()
    verdicts = collections.Counter()
    skips = collections.Counter()
    fam = collections.Counter()
    opang = collections.Counter()
    sym = collections.Counter()
    with open(rd / "results.jsonl", encoding="utf-8") as fh:
        for line in fh:
            r = json.loads(line)
            fam[str(r.get("family"))] += 1
            if r.get("analysis_timeout") or r.get("harness_error"): verdicts["analysis_timeout_or_error"] += 1
            opang[f"{r.get('op')}/{r.get('angle')}"] += 1
            for p, v in (r.get("paths") or {}).items():
                paths[p] += 1
                verdicts[v.get("verdict") or ("desync" if v.get("desync") else "none")] += 1
                if v.get("skip"):
                    skips[v["skip"][:60]] += 1
                if "symbolic_equal" in v:
                    sym[str(v["symbolic_equal"])] += 1
    classes = collections.defaultdict(set)
    sev = {}
    for f in fnd:
        k = class_key(f)
        classes[k].add(f["case"])
        sev.setdefault(k, set()).add(f["severity"])
    return {"run": rd.name, "seed": summ["seed"], "count": summ["count"],
            "path_executions": dict(paths), "path_verdicts": dict(verdicts),
            "families": dict(sorted(fam.items())), "op_angle": dict(sorted(opang.items())),
            "cases_per_second_total": round(stats["cases_per_second_total"], 3),
            "t_total_s": round(stats["t_total"], 1), "emulator_processes": stats.get("processes"),
            "emulator_sha256": stats.get("emulator_sha256"),
            "cases_with_findings": summ["cases_with_findings"], "confirmations": summ.get("confirmations"),
            "unresolved_desync": summ.get("unresolved_desync"),
            "classes": {k: {"cases": len(v), "severity": sorted(sev[k])} for k, v in sorted(classes.items())},
            "skips": dict(skips.most_common(8)), "symbolic_simplification": dict(sym)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("runs", nargs="+")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    rows = [one(r) for r in a.runs]
    tot = collections.Counter()
    for r in rows:
        tot["cases"] += r["count"]
        tot["path_executions"] += sum(r["path_executions"].values())
        tot["seconds"] += r["t_total_s"]
    allc = collections.defaultdict(int)
    for r in rows:
        for k, v in r["classes"].items():
            allc[k] += v["cases"]
    out = {"runs": rows, "total": dict(tot), "classes_total_cases": dict(sorted(allc.items()))}
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1, ensure_ascii=False), encoding="utf-8")
    print("| seed | cases | path runs | cases/s | wall s | cases w/ findings | confirmations | unresolved desync |")
    print("|---|---|---|---|---|---|---|---|")
    for r in rows:
        print(f"| {r['seed']} | {r['count']} | {sum(r['path_executions'].values())} | {r['cases_per_second_total']} | "
              f"{r['t_total_s']} | {r['cases_with_findings']} | {r['confirmations']} | {r['unresolved_desync']} |")
    print(f"total cases {tot['cases']}, path executions {tot['path_executions']}, wall {tot['seconds']:.0f} s")
    print(json.dumps(out["classes_total_cases"], indent=1))


if __name__ == "__main__":
    main()
