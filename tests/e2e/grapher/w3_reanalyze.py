"""Re-run the W3 analysis offline from results.jsonl (raw slot dumps are stored),
so oracle-policy changes never need new emulator runs.

py -3.12 tests/e2e/grapher/w3_reanalyze.py RUN_DIR...   -> RUN_DIR/reanalysis.json
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from grapher import surface as G  # noqa: E402
from grapher.w3_classify import classify  # noqa: E402


def by_tag_of(case, rec):
    bt = {f"{case['id']}|end": {"ok": True}, "__residuals__": {case["id"]: {}}}
    for i, s in enumerate(rec.get("slots", [])):
        bt[f"{case['id']}|s{i}"] = {"ok": True, "count": len(case["slots"]), "input": s["raw_input"],
                                    "text": s.get("text"), "giac_rpn": s.get("giac"), "giac_rpn_l": s.get("giac_l"),
                                    "valid": s.get("valid"), "kind": s.get("kind"), "reason": s.get("reason"),
                                    "values": s.get("values")}
        for si, x, y, v in s.get("residuals", []):
            bt["__residuals__"][case["id"]][(si, x, y)] = v
    return bt


def main():
    for d in sys.argv[1:]:
        d = Path(d)
        summ = json.load(open(d / "summary.json"))
        out, cls, fam = [], Counter(), Counter()
        for line in open(d / "results.jsonl", encoding="utf-8"):
            rec = json.loads(line)
            idx = int(rec["id"].split("-")[1])
            case = G.generate(summ["seed"], idx)
            fam[case["family"]] += 1
            if not rec.get("slots") or len(rec["slots"]) != len(case["slots"]):
                cls["UNJUDGED(desync/analysis)"] += 1
                continue
            _, meta = G.case_lines(case, None)
            _, fnd, _ = G.analyze(case, by_tag_of(case, rec), meta, None)
            for f in fnd:
                f = f.to_dict()
                f["class"] = classify(f)
                cls[f["class"]] += 1
                out.append(f)
        (d / "reanalysis.json").write_text(json.dumps({"families": fam, "classes": cls, "findings": out},
                                                      indent=1, ensure_ascii=False, default=str), encoding="utf-8")
        print(d.name, dict(fam), dict(cls))


if __name__ == "__main__":
    main()
