"""Aggregate Equations findings of several run directories into suspected
root-cause classes (W5-Fxx). Rules use observable features of the finding
(invariant, title, displayed equation, residual reason, family); anything a
rule does not claim stays visible as UNCLASSIFIED with its signature.

    py -3.12 tests/e2e/equations/classify.py <run dir> [<run dir> ...]
"""
from __future__ import annotations

import collections
import json
import sys
from pathlib import Path


def disp(f):
    return " ; ".join(f["detail"].get("displayed") or [])


RULES = [
    ("W5-F01", lambda f, r: f["invariant"] == "I1" and ("root[" in disp(f) or "^{{1}/" in disp(f))
     and ("No solution" in f["title"] or "missing" in f["title"])),
    ("W5-F02", lambda f, r: f["invariant"] == "I1" and r.get("family") == "system" and "missing solution" in f["title"]),
    ("W5-F03", lambda f, r: f["invariant"] == "I5" and r.get("family") == "system" and r.get("sub", "").startswith("nl_")),
    ("W5-F04", lambda f, r: f["invariant"] == "I1" and "spurious" in f["title"]
     and ("log" in str(f["detail"].get("residual", "")) or "ln(" in disp(f) or "log" in disp(f))),
    ("W5-F05", lambda f, r: f["invariant"] == "I1" and "spurious" in f["title"] and "sqrt" in str(f["detail"].get("residual", ""))),
    ("W5-F06", lambda f, r: f["detail"].get("policy_question") is True),
    ("W5-F07", lambda f, r: f["invariant"] == "CAP"),
    ("W5-F08", lambda f, r: f["invariant"] == "I5" and "undefined" in f["title"]
     and any("." in s for s in (f["detail"].get("displayed") or []))),
    ("W5-F09", lambda f, r: f["invariant"] in ("HANG", "CRASH")),
    ("W5-F10", lambda f, r: "displayed solution formula differs" in f["title"]),
    # harness artefact fixed after seed 1's analysis: decimal (12-digit) values
    # checked with the exact 1e-40 residual tolerance in the cplx_infinite branch
    ("HARNESS-decimal-residual", lambda f, r: f["title"] == "shown value is not a solution"
     and "|l-r|=" in str(f["detail"].get("residual", "")) and "e-1" in str(f["detail"].get("residual", ""))),
]


def main(dirs):
    out_root = Path(dirs[0]).parent
    classes = collections.defaultdict(lambda: {"findings": 0, "cases": set(), "paths": collections.Counter(),
                                               "policies": collections.Counter(), "subs": collections.Counter(),
                                               "examples": []})
    totals = {"cases": 0, "cases_with_findings": 0, "second_disagree": 0, "desync": 0, "harness_error": 0,
              "analysis_timeout": 0, "confirm": collections.Counter(), "paths_judged": collections.Counter()}
    for d in dirs:
        d = Path(d)
        recs = {}
        for line in open(d / "results.jsonl", encoding="utf-8"):
            r = json.loads(line)
            recs[r["id"]] = r
            totals["cases"] += 1
            totals["cases_with_findings"] += bool(r.get("findings"))
            totals["second_disagree"] += r.get("second") == "disagree"
            totals["desync"] += bool(r.get("desync"))
            totals["harness_error"] += bool(r.get("harness_error"))
            totals["analysis_timeout"] += bool(r.get("analysis_timeout"))
            if r.get("confirmation"):
                totals["confirm"][r["confirmation"]] += 1
            for p in r.get("paths", {}):
                totals["paths_judged"][p] += 1
        for f in json.load(open(d / "findings.json", encoding="utf-8")):
            r = recs.get(f["case"], {})
            cid = next((c for c, rule in RULES if rule(f, r)), None)
            if cid is None:
                cid = f"UNCLASSIFIED {f['invariant']}/{f['severity']} {f['title'][:70]} [{r.get('family')}:{r.get('sub')}]"
            c = classes[cid]
            c["findings"] += 1
            c["cases"].add(f["case"])
            c["paths"][f["path"]] += 1
            c["policies"][r.get("policy")] += 1
            c["subs"][f"{r.get('family')}:{r.get('sub')}"] += 1
            if len(c["examples"]) < 4:
                c["examples"].append({"case": f["case"], "path": f["path"], "severity": f["severity"], "title": f["title"],
                                      "displayed": f["detail"].get("displayed"), "numos": f["detail"].get("numos"),
                                      "oracle": f["detail"].get("oracle"), "triage": f.get("triage")})
    res = {k: dict(v, cases=len(v["cases"]), paths=dict(v["paths"]), policies=dict(v["policies"]), subs=dict(v["subs"]))
           for k, v in sorted(classes.items())}
    totals["confirm"] = dict(totals["confirm"])
    totals["paths_judged"] = dict(totals["paths_judged"])
    (out_root / "classes.json").write_text(json.dumps({"totals": totals, "classes": res}, indent=1, ensure_ascii=False),
                                           encoding="utf-8")
    md = ["| class | findings | cases | paths | policies | sub-families |", "|---|---|---|---|---|---|"]
    for k, v in res.items():
        md.append(f"| {k} | {v['findings']} | {v['cases']} | {v['paths']} | {v['policies']} | {dict(list(v['subs'].items())[:6])} |")
    (out_root / "classes.md").write_text(json.dumps(totals) + "\n\n" + "\n".join(md) + "\n", encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps(totals, indent=1))
    print("\n".join(md))


if __name__ == "__main__":
    main(sys.argv[1:])
