"""Assign every confirmed finding of W1 run directories to a W1 root-cause class.

Decision procedure per finding (first match wins), all on recorded data:
  CRASH 0xC00000FD -> F13, CRASH 0xC0000005 -> F14, HANG -> F15
  path naive with a structure different from the intended tree -> NAIVE (expected; not a finding)
  'nan' in the shown result                                  -> F12
  oracle non-finite, finite value shown                      -> F03
  harness: FORMAT-menu prediction desync on angle results    -> H-FMTPRED
  value of NumOS's own exact text == oracle but the display differs:
        exact text contains i                                 -> F02 (display drops parentheses)
        otherwise                                             -> DISPLAY-OTHER
  decimal formats (FIX/Decimal/SCI/ENG/Recurring) with an exact text equal to the oracle:
        FIX 'claims more precision'                           -> F16 (FIX pads the 12-digit companion)
        FIX off by one ulp                                    -> F17 (FIX double rounding) / F18
        Decimal/SCI/ENG small error (< 1e-9 rel)              -> F18 (12-digit companion inaccurate)
        larger error, huge-argument trig in tree              -> F11
  exact text != oracle (Giac semantics), by tree features:
        DEG and inverse trig                                  -> F04
        abs in tree                                           -> F07
        root/surd/sqrt/power                                  -> F06/F08/F10 family (GIAC-RADICAL)
        otherwise                                             -> GIAC-OTHER
Unmatched findings are listed for manual review.

    py -3.12 tests/e2e/calculation/w1_rootcause.py <run dir>... [--json out.json] [--show CLASS]
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import sympy as sp  # noqa: E402

from common.evaluator import Ctx, close  # noqa: E402
from common import giacparse  # noqa: E402

ENV = {"A": sp.Integer(3), "B": sp.Rational(1, 2), "C": sp.Integer(-4), "D": sp.sqrt(2), "E": sp.Rational(3, 4)}
ANS = {"5": sp.Integer(5), "2/3": sp.Rational(2, 3), "-7": sp.Integer(-7), "1.25": sp.Rational(5, 4)}
DEC = {"Fixed decimals (FIX)", "Decimal", "Scientific (SCI)", "Engineering (ENG)", "Recurring decimal",
       "Extended decimal"}


def exact_matches_oracle(exact, oracle_text, angle, ans):
    if not exact or not oracle_text or oracle_text.startswith(("nonfinite", "incomplete", "unsupported")):
        return None
    env = dict(ENV)
    if ans:
        env["Ans"] = ANS.get(ans, sp.Integer(5))
    try:
        gv = giacparse.parse_eval(exact, angle, env)
        ov = sp.sympify(oracle_text.replace("I", "I"), locals={"E": sp.E, "I": sp.I, "pi": sp.pi,
                                                              "x": sp.Symbol("x")})
        rel = sp.Rational(1, 10**9) if "." in exact else sp.Rational(1, 10**25)
        return bool(close(gv, ov, rel=rel))
    except Exception:  # noqa: BLE001
        return None


def classify(f, rec):
    inv, path, title = f["invariant"], f["path"], f["title"]
    d = f.get("detail", {})
    tree = rec.get("tree") or d.get("tree") or ""
    angle = rec.get("angle", "rad")
    if inv == "CRASH":
        return "F13" if "c00000fd" in title else ("F14" if "c0000005" in title else "CRASH-OTHER")
    if inv == "HANG":
        return "F15"
    if path == "naive" and "renders identically" in title:
        return "NAIVE-WYSIWYC"
    shown = str(d.get("display") or "") + " " + str(d.get("exact") or "")
    if "nan" in shown.lower():
        return "F12"
    if "undefined/pole" in title:
        return "F03"
    if inv == "I4" and path in ("fmt", "hist", "ans", "sto") and angle == "deg" and \
            re.search(r"\ba(sin|cos|tan)\(", tree) and not re.search(r"\b(sin|cos|tan)\(", tree.replace("asin(", "").replace("acos(", "").replace("atan(", "")):
        return "H-FMTPRED"
    if inv in ("I4", "I1") and path in ("hist", "ans", "sto", "fmt") and (d.get("after") == "" or "Syntax" in str(d.get("display_after"))):
        return "H-FMTPRED"
    if "I3" == inv:
        return "I3-" + title[:40]
    fmt = d.get("format")
    m = exact_matches_oracle(d.get("exact"), str(d.get("oracle") or rec.get("oracle") or ""), angle, rec.get("ans"))
    if fmt in DEC or path in ("sd", "fmt", "sd2") or re.match(r"(fix\d|sci|eng|dec|rec|ext)", path or ""):
        if "FIX" in str(fmt) and "claim more precision" in title:
            return "F16"
        if "FIX" in str(fmt) and "off by one unit" in title:
            return "F17"
        if "claim more precision" in title or "off by one unit" in title:
            return "F18"
        if re.search(r"(sin|cos|tan)\(", tree) and (re.search(r"\d{9,}", tree) or "×10^" in tree or re.search(r"\}\^\{\d", tree)):
            return "F11"
        if "real decimal shown for a non-real" in title:
            return "DEC-NONREAL"
        return "DEC-OTHER"
    if m is True:
        return "F02" if re.search(r"\bi\b|\*i|i\*", str(d.get("exact"))) else "DISPLAY-OTHER"
    if "decimal digits off by one" in title or "claim more precision" in title:
        return "F18"
    if angle == "deg" and re.search(r"\ba(sin|cos|tan)\(", tree) and "i" in str(d.get("exact")):
        return "F04"
    if "|" in tree and m is False:
        return "F07?"
    if re.search(r"root\[|√|\^", tree):
        return "GIAC-RADICAL/POWER"
    if re.search(r"\ba(sin|cos)\(", tree):
        return "GIAC-INVTRIG"
    return "GIAC-OTHER" if m is False else "UNMATCHED"


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--json", default=None)
    ap.add_argument("--show", default=None)
    a = ap.parse_args(argv)
    per_class = collections.defaultdict(lambda: {"findings": 0, "cases": set()})
    examples = collections.defaultdict(list)
    for dname in a.dirs:
        dd = Path(dname)
        recs = {}
        for line in open(dd / "results.jsonl", encoding="utf-8"):
            r = json.loads(line)
            recs[r["id"]] = r
        for f in json.load(open(dd / "findings.json", encoding="utf-8")):
            rec = recs.get(f["case"], {})
            c = classify(f, rec)
            per_class[c]["findings"] += 1
            per_class[c]["cases"].add(f"{dd.name}/{f['case']}")
            examples[c].append((dd.name, f, rec))
    rows = []
    for c, v in sorted(per_class.items(), key=lambda kv: -len(kv[1]["cases"])):
        print(f"{c:22s} findings={v['findings']:5d} cases={len(v['cases']):5d}  e.g. {sorted(v['cases'])[:3]}")
        rows.append({"class": c, "findings": v["findings"], "cases": len(v["cases"]),
                     "case_ids": sorted(v["cases"])})
    if a.show:
        for dn, f, rec in examples.get(a.show, [])[:40]:
            d = f["detail"]
            print(f"\n{dn}/{f['case']} [{f['path']}] {rec.get('angle')} {f['title'][:70]}\n  tree={rec.get('tree')}\n"
                  f"  exact={d.get('exact')} display={d.get('display')} oracle={str(d.get('oracle'))[:100]}")
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1), encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
