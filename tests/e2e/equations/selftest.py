"""Self-test of the Equations driver (task 3): does the judge catch a wrong
answer, and does it pass correct ones?

No mutant emulator can be built in this audit (no pio). Instead, real
emulator dumps are collected for N generated cases (direct path), then:

  baseline     every case is judged unchanged; a case is "clean" when it
               produces no finding at all.
  NumOS-side   on clean cases the recorded NumOS answer is perturbed (what a
  plants       buggy product would have shown) and must be flagged:
                 drop_root      one solution group removed (single group ->
                                "No solution")
                 spurious_root  an extra group (first value + 1/7)
                 sign_flip      a non-zero value negated
                 drop_family    a periodic family removed (single family ->
                                its offset shifted by half a period)
                 wrong_kind     solutions <-> "No solution"
                 giac_text      the Giac-bound lhs text altered (x -> (x+1))
                 display        a digit of the displayed result formula altered
  oracle-side  on clean cases the EXPECTED set is perturbed (an oracle that
  plants       expects something else) and must be flagged:
                 t_drop_root, t_spurious_root, t_sign_flip, t_drop_family

    py -3.12 tests/e2e/equations/selftest.py --seed 900 --count 400
Writes out/correctness-e2e-01/equations/selftest/selftest.json.
"""
from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import sympy as sp  # noqa: E402

from common.emulator import DEFAULT_EMULATOR  # noqa: E402
from equations import flow, surface as S  # noqa: E402


def collect(seed, count, emulator, workdir, batch=40):
    cases = [S.generate(seed, i) for i in range(count)]
    dumps, metas = {}, {}
    for i in range(0, len(cases), batch):
        chunk = cases[i:i + batch]
        lines = S.header()
        for c in chunk:
            l, m = S.case_lines_for(c, None, ["direct"])
            lines += l
            metas[c["id"]] = m
        r = flow.run_script(lines, emulator, workdir=workdir, timeout=20 + 1.5 * len(chunk))
        for c in chunk:
            d = r.by_tag.get(f"{c['id']}|direct")
            if d:
                dumps[c["id"]] = d
    return cases, dumps, metas


def judge(case, d, meta):
    rec, fnd, des = S.analyze(case, {f"{case['id']}|direct": d}, meta, {"second": "agree"})
    return [f for f in fnd if f.severity != "S4" or f.invariant == "ORACLE"], des


def flagged(fs):
    return any(f.invariant in ("I1", "I2", "I5", "ORACLE") for f in fs)


def numos_plants(case, d):
    out = []
    sk, groups = d.get("set_kind"), d.get("groups") or []
    if d.get("status") == "ok" and sk == 0 and groups:
        x = copy.deepcopy(d)
        if len(groups) >= 2:
            x["groups"] = groups[:-1]
        else:
            x["groups"], x["set_kind"] = [], 1
        out.append(("drop_root", x))
        x = copy.deepcopy(d)
        g = copy.deepcopy(groups[0])
        g[0]["exact"] = f"({g[0]['exact']})+1/7"
        x["groups"] = groups + [g]
        out.append(("spurious_root", x))
        for gi, g in enumerate(groups):
            try:
                v = S.parse_value(g[0]["exact"])[0]
            except Exception:  # noqa: BLE001
                continue
            if v != 0 and not any(_same(-v, S.parse_value(h[0]["exact"])[0]) for h in groups):
                x = copy.deepcopy(d)
                x["groups"][gi][0]["exact"] = f"-({g[0]['exact']})"
                out.append(("sign_flip", x))
                break
        x = copy.deepcopy(d)
        x["groups"], x["set_kind"] = [], 1
        out.append(("wrong_kind", x))
    if d.get("status") == "ok" and sk == 1:
        x = copy.deepcopy(d)
        x["set_kind"] = 0
        x["groups"] = [[{"var": v, "exact": "0", "approx": "0", "real": 0.0} for v in case["vars"]]]
        out.append(("wrong_kind", x))
    fams = (d.get("view") or {}).get("families") or []
    if d.get("status") == "ok" and sk == 4 and fams:
        x = copy.deepcopy(d)
        if len(fams) >= 2:
            x["view"]["families"] = fams[:-1]
        else:
            f = x["view"]["families"][0]
            f["offset"] = f"({f['offset']})+({f['period']})/2"
        out.append(("drop_family", x))
    tr = (d.get("trace") or {}).get("authored")
    if tr and "x" in tr[0][0]:
        x = copy.deepcopy(d)
        x["trace"]["authored"][0][0] = tr[0][0].replace("x", "(x+1)")
        out.append(("giac_text", x))
    forms = (d.get("view") or {}).get("formulas") or []
    if d.get("status") == "ok" and sk == 0 and forms and not (d.get("view") or {}).get("title", "").lower().startswith("numerical"):
        import re
        f0 = forms[0]
        m = re.search(r'Number "(\d)', f0)
        if m:
            new = str((int(m.group(1)) + 3) % 10)
            x = copy.deepcopy(d)
            x["view"]["formulas"][0] = f0[:m.start(1)] + new + f0[m.end(1):]
            out.append(("display", x))
    return out


def _same(a, b):
    try:
        return abs(complex(sp.N(a - b, 30))) < 1e-20
    except Exception:  # noqa: BLE001
        return False


def truth_plants(case):
    t = case["truth"]
    out = []
    if t["kind"] in ("finite", "tuples") and t["sols"]:
        x = copy.deepcopy(case)
        x["truth"] = dict(t, sols=t["sols"][1:]) if len(t["sols"]) > 1 else {"kind": "empty"}
        out.append(("t_drop_root", x))
        x = copy.deepcopy(case)
        s0 = t["sols"][0]
        bump = (tuple(s0[:1]) and (s0[0] + sp.Rational(1, 7),) + tuple(s0[1:])) if isinstance(s0, tuple) else s0 + sp.Rational(1, 7)
        x["truth"] = dict(t, sols=list(t["sols"]) + [bump])
        out.append(("t_spurious_root", x))
        for i, s in enumerate(t["sols"]):
            v = s[0] if isinstance(s, tuple) else s
            neg = (-v,) + tuple(s[1:]) if isinstance(s, tuple) else -v
            if v != 0 and not any(_same(neg[0] if isinstance(neg, tuple) else neg, w[0] if isinstance(w, tuple) else w) for w in t["sols"]):
                x = copy.deepcopy(case)
                sols = list(t["sols"])
                sols[i] = neg
                x["truth"] = dict(t, sols=sols)
                out.append(("t_sign_flip", x))
                break
    if t["kind"] == "periodic" and len(t["fams"]) >= 1:
        x = copy.deepcopy(case)
        if len(t["fams"]) >= 2:
            x["truth"] = dict(t, fams=t["fams"][:-1])
        else:
            o, p = t["fams"][0]
            x["truth"] = dict(t, fams=[(o + p / 2, p)])
        out.append(("t_drop_family", x))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=900)
    ap.add_argument("--count", type=int, default=400)
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "equations" / "selftest"))
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    cases, dumps, metas = collect(a.seed, a.count, a.emulator, str(out / "tmp"))
    stats = {"cases": len(cases), "dumped": len(dumps), "baseline_clean": 0, "baseline_flagged": 0,
             "baseline_flagged_titles": {}, "plants": {}}
    misses = []
    for c in cases:
        d = dumps.get(c["id"])
        if not d:
            continue
        meta = metas[c["id"]]
        fs, des = judge(c, d, meta)
        if des:
            continue
        if fs:
            stats["baseline_flagged"] += 1
            for f in fs:
                stats["baseline_flagged_titles"][f.title] = stats["baseline_flagged_titles"].get(f.title, 0) + 1
            continue
        stats["baseline_clean"] += 1
        for name, x in numos_plants(c, d):
            fs2, _ = judge(c, x, meta)
            s = stats["plants"].setdefault(name, {"applied": 0, "flagged": 0})
            s["applied"] += 1
            if flagged(fs2):
                s["flagged"] += 1
            else:
                misses.append({"case": c["id"], "plant": name, "family": c["family"], "sub": c["sub"]})
        for name, cx in truth_plants(c):
            fs2, _ = judge(cx, d, meta)
            s = stats["plants"].setdefault(name, {"applied": 0, "flagged": 0})
            s["applied"] += 1
            if flagged(fs2):
                s["flagged"] += 1
            else:
                misses.append({"case": c["id"], "plant": name, "family": c["family"], "sub": c["sub"]})
    stats["misses"] = misses[:50]
    stats["missed_total"] = len(misses)
    (out / "selftest.json").write_text(json.dumps(stats, indent=1, ensure_ascii=False, default=str), encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps({k: v for k, v in stats.items() if k != "misses"}, indent=1, ensure_ascii=False))
    for m in misses[:20]:
        print("MISS", m)


if __name__ == "__main__":
    main()
