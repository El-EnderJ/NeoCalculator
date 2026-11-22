"""Self-test of the Calculus checker (W4 task 3). No emulator rebuild needed.

1. Run N generated cases (direct path) on the real emulator once; keep the
   raw `dump_calculus` records (out/.../calculus/selftest/dumps.json).
2. Baseline: judge every dump with the true oracle.
3. Take the cases the baseline judged "agree" (numerically verified correct)
   and re-judge them with perturbed ORACLES:
     sign    expected derivative / integrand with a flipped sign
     chain   expected derivative computed WITHOUT the chain-rule factor
             (d/dx F(u) := F'(u); for integrals: the checker differentiates
             NumOS's antiderivative without the chain factor)
   and with planted wrong NUMOS answers (dump perturbed):
     negate  the displayed result negated (−(…) [+ C])
     swap    the displayed result of another case of the same op
   A mutant that is numerically identical to the truth at the sample points
   (e.g. dropping the chain factor of sin(x)) cannot be detected by any
   checker and is counted as "equivalent", not as a miss.

    python tests/e2e/calculus/selftest.py --seed 7 --count 200
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import sympy as sp  # noqa: E402

from common.emulator import DEFAULT_EMULATOR, run_script  # noqa: E402
from common.sexpr import parse  # noqa: E402
from calculus import check as C  # noqa: E402
from calculus import surface as S  # noqa: E402
from calculus.semantics import compare, XR, YR  # noqa: E402

OUT = HERE.parent.parent / "out" / "correctness-e2e-01" / "calculus" / "selftest"


def naive_diff(e, v):
    """Derivative that forgets every chain-rule factor (planted oracle bug)."""
    if not e.has(v):
        return sp.Integer(0)
    if e == v:
        return sp.Integer(1)
    if isinstance(e, sp.Add):
        return sp.Add(*[naive_diff(a, v) for a in e.args])
    if isinstance(e, sp.Mul):
        args = e.args
        return sp.Add(*[sp.Mul(*(args[:i] + (naive_diff(a, v),) + args[i + 1:])) for i, a in enumerate(args)])
    if isinstance(e, sp.Pow):
        b, n = e.args
        if not n.has(v):
            return n * b ** (n - 1)                   # chain factor b' dropped
        return e * sp.log(b) if not b.has(v) else sp.diff(e, v)
    if isinstance(e, sp.Function) or isinstance(e, sp.Abs):
        u = e.args[0]
        d = sp.Dummy("u", real=True)
        return sp.diff(e.func(d), d).subs(d, u)      # u' dropped
    return sp.diff(e, v)


def unparse(n):
    if n is None:
        return "null"
    if isinstance(n, str):
        return json.dumps(n)
    if n[0] == "row":
        return "[" + " ".join(unparse(c) for c in n[1]) + "]"
    return "(" + n[0] + "".join(" " + unparse(a) for a in n[1:]) + ")"


def negate_display(disp, op):
    st = parse(disp)
    st2, had_c = C._strip_c(st)
    items = [("op", "Sub"), ("paren", "paren", st2)]
    if had_c:
        items += [("op", "Add"), ("var", "C")]
    return unparse(("row", tuple(items)))


def collect(seed, count, emulator, batch=40):
    cases = [S.generate(seed, i) for i in range(count)]
    dumps = {}
    for k in range(0, count, batch):
        chunk = cases[k:k + batch]
        lines = list(S.header())
        for c in chunk:
            lines += S.case_lines_for(c, None, ["direct"])[0]
        r = run_script(lines, emulator, timeout=20 + 2 * len(chunk))
        for c in chunk:
            d = r.by_tag.get(f"{c['id']}|direct")
            if d:
                dumps[c["id"]] = d
    return cases, dumps


def equivalent(name, c, st, d):
    """Is the perturbed ORACLE numerically identical to the true one here?"""
    from calculus.semantics import evaluate_expr, giac_text_value, CS
    f = evaluate_expr(st, c["angle"], definite=True)[0]
    v = XR if d.get("variable", "x") == "x" else YR
    try:
        if name == "oracle-sign":
            t = C._target(c["op"], f, v)
            return compare(t, -t)["differ"] == 0
        if c["op"] == "diff":
            return compare(naive_diff(f, v), sp.diff(f, v))["differ"] == 0
        F = giac_text_value(d.get("exact") or "", "rad").subs(CS, 0)
        return compare(naive_diff(F, v), sp.diff(F, v))["differ"] == 0
    except Exception:  # noqa: BLE001
        return None


def dump_equivalent(c, d, d2):
    """Is the planted NumOS answer numerically identical to the original?"""
    from calculus.semantics import evaluate_expr, CS
    try:
        a = evaluate_expr(C._strip_c(parse(d["display"]))[0], c["angle"])[0].subs(CS, 0)
        b = evaluate_expr(C._strip_c(parse(d2["display"]))[0], c["angle"])[0].subs(CS, 0)
        v = XR if d.get("variable", "x") == "x" else YR
        if c["op"] == "int":
            a, b = sp.diff(a, v), sp.diff(b, v)
        return compare(a, b)["differ"] == 0
    except Exception:  # noqa: BLE001
        return None


def value_flag(j):
    return any(f[0] in ("I1",) and f[2] not in ("nthroot-name", "missing-C", "spurious-C") for f in j["findings"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--reuse", action="store_true", help="reuse dumps.json")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    dpath = OUT / f"dumps-seed{a.seed}-n{a.count}.json"
    if a.reuse and dpath.exists():
        cases = [S.generate(a.seed, i) for i in range(a.count)]
        dumps = json.loads(dpath.read_text(encoding="utf-8"))
    else:
        cases, dumps = collect(a.seed, a.count, a.emulator)
        dpath.write_text(json.dumps(dumps, indent=0, ensure_ascii=False), encoding="utf-8")

    base = {}
    for c in cases:
        d = dumps.get(c["id"])
        if not d:
            continue
        st = parse(d["input"])
        base[c["id"]] = (c, st, d, C.judge(c["op"], c["angle"], st, d))
    ok = [k for k, (c, st, d, j) in base.items() if j.get("value") == "agree" and not value_flag(j)]
    stats = {"cases": len(cases), "dumps": len(dumps),
             "baseline_verdicts": {}, "baseline_correct_judged": len(ok)}
    for k, (c, st, d, j) in base.items():
        stats["baseline_verdicts"][j["verdict"]] = stats["baseline_verdicts"].get(j["verdict"], 0) + 1

    orig_target, orig_side = C._target, C._numos_side
    results = {}

    def run_mutant(name, setup, teardown, dump_mut=None, applies=lambda c: True):
        det = miss = equiv = na = 0
        misses = []
        for k in ok:
            c, st, d, j0 = base[k]
            if not applies(c):
                na += 1
                continue
            d2 = dict(d)
            if dump_mut:
                nd = dump_mut(c, d2)
                if nd is None:
                    na += 1
                    continue
                d2 = nd
            setup()
            try:
                j = C.judge(c["op"], c["angle"], st, d2)
            finally:
                teardown()
            eq = dump_equivalent(c, d, d2) if dump_mut else equivalent(name, c, st, d)
            flagged = value_flag(j) or j["verdict"] == "finding"
            if flagged:
                det += 1
            elif eq:
                equiv += 1
            else:
                miss += 1
                misses.append({"case": k, "tree": S.M.linear(c["tree"]), "op": c["op"], "verdict": j["verdict"],
                               "value": j.get("value")})
        results[name] = {"detected": det, "equivalent_undetectable": equiv, "missed": miss,
                         "not_applicable": na, "misses": misses[:10]}

    # oracle mutants
    run_mutant("oracle-sign",
               lambda: setattr(C, "_target", lambda op, f, v: -orig_target(op, f, v) if orig_target(op, f, v) is not None else None),
               lambda: setattr(C, "_target", orig_target))
    run_mutant("oracle-chain",
               lambda: (setattr(C, "_target", lambda op, f, v: naive_diff(f, v) if op == "diff" else f),
                        setattr(C, "_numos_side", lambda op, s, v: s if op == "diff" else naive_diff(s, v))),
               lambda: (setattr(C, "_target", orig_target), setattr(C, "_numos_side", orig_side)))
    # NumOS-side planted wrong answers
    by_op = {}
    for k in ok:
        by_op.setdefault(base[k][0]["op"], []).append(k)

    def neg(c, d):
        if d.get("display") in (None, "null"):
            return None
        d["display"] = negate_display(d["display"], c["op"])
        return d

    def swap(c, d):
        pool = [k for k in by_op[c["op"]] if base[k][2].get("exact") != d.get("exact")
                and base[k][2].get("display") not in (None, "null")]
        if not pool or d.get("display") in (None, "null"):
            return None
        other = base[pool[(hash(c["id"]) % len(pool))]][2]
        d["display"] = other["display"]
        d["exact"] = other["exact"]
        return d

    noop = lambda: None  # noqa: E731
    run_mutant("numos-negated-result", noop, noop, dump_mut=neg)
    run_mutant("numos-swapped-result", noop, noop, dump_mut=swap)
    report = {"stats": stats, "mutants": results}
    (OUT / f"selftest-seed{a.seed}-n{a.count}.json").write_text(json.dumps(report, indent=1, ensure_ascii=False),
                                                              encoding="utf-8")
    print(json.dumps(report, indent=1, ensure_ascii=False))


if __name__ == "__main__":
    main()
