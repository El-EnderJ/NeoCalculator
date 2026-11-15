"""Self-test and isolation summaries for W3 runs.

py -3.12 tests/e2e/grapher/w3_compare.py selftest  -> base vs sign vs coef detection rates
py -3.12 tests/e2e/grapher/w3_compare.py isolation -> batch-25 vs batch-1 record diff
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

O = Path(__file__).resolve().parents[3] / "out" / "correctness-e2e-01" / "grapher"


def load(name):
    return {json.loads(l)["id"]: json.loads(l) for l in open(O / name / "results.jsonl", encoding="utf-8")}


def has_i1(r):
    return any(f.startswith("I1/") for f in r.get("findings", []))


def judged(r):
    """Cases whose slots were valid and had at least one real oracle comparison."""
    return any(s.get("valid") for s in r.get("slots", [])) and not r.get("desync")


def selftest():
    base, sign, coef = load("selftest-base"), load("selftest-sign"), load("selftest-coef")
    out = {}
    for name, mut in (("sign", sign), ("coef", coef)):
        ids = [i for i, r in base.items() if judged(r) and not has_i1(r) and i in mut and judged(mut[i])]
        caught = sum(1 for i in ids if has_i1(mut[i]))
        out[name] = {"judged_clean_cases": len(ids), "flagged_I1": caught,
                     "missed": [i for i in ids if not has_i1(mut[i])][:10]}
    out["base_I1_cases"] = sum(1 for r in base.values() if has_i1(r))
    print(json.dumps(out, indent=1))


def isolation():
    a, b = load("selftest-base"), load("isolation-batch1")
    diff = []
    for i in sorted(a):
        ra, rb = dict(a[i]), dict(b.get(i, {}))
        if ra.get("desync") or rb.get("desync"):
            continue
        ra.pop("confirmation", None)
        rb.pop("confirmation", None)
        if json.dumps(ra, sort_keys=True) != json.dumps(rb, sort_keys=True):
            diff.append(i)
    des_a = sum(1 for r in a.values() if r.get("desync"))
    des_b = sum(1 for r in b.values() if r.get("desync"))
    print(json.dumps({"cases": len(a), "compared": len(a) - len([1 for i in a if a[i].get('desync') or b.get(i, {}).get('desync')]),
                      "differing_records": len(diff), "diff_ids": diff[:20], "batch25_desync": des_a,
                      "batch1_desync": des_b}, indent=1))


if __name__ == "__main__":
    {"selftest": selftest, "isolation": isolation}[sys.argv[1]]()
