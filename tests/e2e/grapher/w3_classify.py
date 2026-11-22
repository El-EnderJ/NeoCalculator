"""Map confirmed W3 findings to root-cause classes (W3-F01…) and list the rest.

py -3.12 tests/e2e/grapher/w3_classify.py out/correctness-e2e-01/grapher/main-seed10-s*
"""
from __future__ import annotations

import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

FN = r"(sin|cos|tan|ln|log|√)"


def classify(f):
    d = f["detail"]
    inp = d.get("input") or ""
    reason = d.get("reason") or ""
    inv = f["invariant"]
    if inv in ("POLE", "FLOAT"):
        return "W3-F06"
    if re.search(r"[<>]\*", d.get("text") or "") or re.search(r"\{[<>]\}/", inp):
        return "W3-F01"
    if inv == "I5":
        if "too long" in reason:
            return "CAP-LEN"
        if re.search(r"unknown: (x|y|pi|e)\d", reason):
            return "W3-F02"
        if re.search(r"(\{[<>]\}/|[<>] ×)", inp):
            return "W3-F01"
        if re.search(r"(Mul|Add|Sub|[×+\-]) - ", inp) or re.search(r"[×+\-] -\s*\d", inp):
            return "W3-F07"
        if re.search(r"unknown: [a-z]$", reason) or reason == "syntax":
            return "W3-F03"
        return "I5-other"
    if inv == "I1":
        if re.search(r"\^\{- ", inp):
            return "W3-F09"
        if re.search(r"(\d|\)|\}) e\b", inp):
            return "W3-F04"
        if d.get("angle") == "deg" and re.search(FN + r"\([^x()]*\)", inp) and "x" in inp:
            return "W3-F05"
        return "I1-other"
    if inv == "I2":
        return "I2-other"
    return inv + "-other"


def main():
    counts, cases, ex = Counter(), defaultdict(set), defaultdict(list)
    for dname in sys.argv[1:]:
        p = Path(dname) / "findings.json"
        if not p.exists():
            continue
        for f in json.load(open(p, encoding="utf-8")):
            c = classify(f)
            counts[c] += 1
            cases[c].add(f["case"])
            if len(ex[c]) < 6:
                d = f["detail"]
                ex[c].append({"case": f["case"], "inv": f["invariant"], "sub": d.get("sub"), "angle": d.get("angle"),
                              "text": d.get("text"), "giac": d.get("giac"), "reason": d.get("reason"),
                              "samples": str(d.get("samples", ""))[:220], "keys": d.get("keys", "")[:120]})
    out = {c: {"findings": counts[c], "cases": len(cases[c]), "examples": ex[c]} for c in sorted(counts)}
    print(json.dumps(out, indent=1, ensure_ascii=False))


if __name__ == "__main__":
    main()
