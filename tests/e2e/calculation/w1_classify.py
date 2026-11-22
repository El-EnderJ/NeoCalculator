"""Group the confirmed findings of W1 run directories into candidate root-cause
classes (heuristic signature) and print counts + examples for manual triage.

    py -3.12 tests/e2e/calculation/w1_classify.py <run dir> [<run dir> ...] [--json out.json]
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path


def features(tree: str, angle: str) -> list:
    t = tree or ""
    f = []
    if re.search(r"\b(sin|cos|tan)\(", t):
        f.append("trig")
    if re.search(r"\ba(sin|cos|tan)\(", t):
        f.append("invtrig")
    if re.search(r"\b(ln|log)\(|log_", t):
        f.append("log")
    if "root[" in t or "√" in t:
        f.append("root")
    if "|" in t:
        f.append("abs")
    if "Ans" in t:
        f.append("ans")
    if re.search(r"\d{14,}", t):
        f.append("bigint")
    if "×10^" in t:
        f.append("sci")
    if "□" in t:
        f.append("empty")
    f.append(angle or "?")
    return f


def classify(f: dict, rec: dict) -> str:
    inv, sev, path, title = f["invariant"], f["severity"], f["path"], f["title"]
    d = f.get("detail", {})
    tri = (f.get("triage") or {}).get("verdict", "")
    if inv in ("CRASH", "HANG"):
        rc = re.search(r"0x[0-9a-f]+", title)
        return f"{inv} {rc.group(0) if rc else ''}".strip()
    t = title.split(": ", 1)[-1] if ":" in title and path not in ("direct", "toolbox", "edited", "naive") else title
    kind = "naive" if path == "naive" else ("canonical" if path in ("direct", "toolbox", "edited") else "post-result")
    tv = "giac==oracle" if "pipeline" in tri else ("giac!=oracle" if "Giac semantics" in tri else
                                                   ("giac refuses" if "refuses" in tri else "no-triage"))
    disp = str(d.get("display") or "")
    shown = str(d.get("shown") or "")
    oracle = str(d.get("oracle") or rec.get("oracle") or "")
    extra = []
    if "nan" in disp.lower():
        extra.append("NaN")
    if "I" in shown and "I" not in oracle:
        extra.append("complex-shown-real-oracle")
    if "I" in oracle and "I" not in shown and shown:
        extra.append("real-shown-complex-oracle")
    if "zoo" in oracle or "nonfinite" in oracle:
        extra.append("oracle-nonfinite")
    if d.get("format"):
        extra.append(f"fmt={d.get('format')}")
    return f"{inv}/{sev} {kind} | {t[:70]} | {tv} | {' '.join(extra)}"


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--json", default=None)
    ap.add_argument("--examples", type=int, default=3)
    a = ap.parse_args(argv)
    groups = collections.defaultdict(list)
    total_cases = 0
    for dname in a.dirs:
        d = Path(dname)
        recs = {}
        for line in open(d / "results.jsonl", encoding="utf-8"):
            r = json.loads(line)
            recs[r["id"]] = r
            total_cases += 1
        for f in json.load(open(d / "findings.json", encoding="utf-8")):
            rec = recs.get(f["case"], {})
            key = classify(f, rec)
            groups[key].append((d.name, f, rec))
    out = []
    for key, items in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        cases = sorted({(dn, f["case"]) for dn, f, _ in items})
        fe = collections.Counter(x for _, f, rec in items for x in features(rec.get("tree"), rec.get("angle")))
        print(f"\n== {len(items)} findings / {len(cases)} cases :: {key}")
        print("   features:", dict(fe.most_common(8)))
        for dn, f, rec in items[: a.examples]:
            d = f.get("detail", {})
            print(f"   - {dn}/{f['case']} [{f['path']}] tree={rec.get('tree')}")
            print(f"       keys={d.get('keys')}")
            print(f"       exact={d.get('exact')} display={d.get('display')} shown={str(d.get('shown'))[:80]} "
                  f"oracle={str(d.get('oracle'))[:80]} giac_in={d.get('giac_in')}")
        out.append({"class": key, "findings": len(items), "cases": [f"{dn}/{c}" for dn, c in cases]})
    print(f"\n{total_cases} cases, {sum(len(v) for v in groups.values())} findings, {len(groups)} signatures")
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1, ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
