"""ADV-H01 `)` pattern table for Grapher, every relation kind.

For each nesting context the expression is typed twice: NAIVE (a human closes
with `)`) and CANONICAL (RIGHT leaves each slot). For each relation kind
(y=f(x), x=f(y), implicit y^2=f(x), inequality y<f(x)) we record the slot AST,
the Giac text, and check that the values NumOS computes equal the oracle of
the DISPLAYED AST (WYSIWYC, I1/I2), and whether NAIVE builds the same tree as
CANONICAL. One fresh emulator process per (pattern, style, kind).

py -3.12 tests/e2e/grapher/adv_h01.py [--out out/correctness-e2e-01/grapher/adv-h01]
"""
from __future__ import annotations

import argparse
import json
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common.emulator import run_script, script_header  # noqa: E402
from common.sexpr import linear, parse  # noqa: E402
from grapher.gcore import (EMU, X, Y, agree, keyline, oracle_expr, parse_residuals,  # noqa: E402
                           real_candidates, residual_lines, split_relation, tb)

# name: (naive, canonical)  -- expressions in x
PATTERNS = {
    "paren_mul":        ("( x + 1 ) * 2", "( x + 1 right * 2"),
    "exp_paren_mul":    ("2 ^ ( x + 1 ) * 3", "2 ^ ( x + 1 right right * 3"),
    "exp_noparen":      ("x ^ 2 ) + 1", "x ^ 2 right + 1"),
    "frac_num_paren":   ("/ ( x + 1 ) * 2 right 3", "/ ( x + 1 right * 2 right 3"),
    "frac_den_paren":   ("1 / ( x + 1 ) * 2", "1 / ( x + 1 right right * 2"),
    "frac_den_noparen": ("1 / x + 1 ) * 2", "1 / x + 1 right * 2"),
    "sqrt_noparen":     ("sqrt x + 1 ) * 2", "sqrt x + 1 right * 2"),
    "sqrt_paren":       ("sqrt ( x + 1 ) * 2", "sqrt ( x + 1 right right * 2"),
    "fn_arg":           ("sin x + 1 ) * 2", "sin x + 1 right * 2"),
    "fn_power":         ("sin x ) ^ 2", "sin x right ^ 2"),
    "ln_paren":         ("ln ( x ) + 1", "ln ( x right right + 1"),
    "logbase_arg":      ("logbase 2 right x + 1 ) * 3", "logbase 2 right x + 1 right * 3"),
    # Grapher toolbox only offers Fraction/Power/Paren/sqrt/10^/LogBase/x/y
    # (abs and nthroot answer "Unavailable in this editor"), so the template
    # path uses the toolbox Logarithm (log_b) entry.
    "tb_logbase_arg":   (" ".join(tb("Logarithm")) + " 2 right x + 1 ) * 3",
                         " ".join(tb("Logarithm")) + " 2 right x + 1 right * 3"),
    "tb_logbase_frac":  (" ".join(tb("Logarithm")) + " 3 right 1 / x ) + 1",
                         " ".join(tb("Logarithm")) + " 3 right 1 / x right right + 1"),
    "ineq_frac_first":  ("/ 1 right x + 1 ) * 2", "/ 1 right x + 1 right * 2"),
    "implicit_mul":     ("2 ( x + 1 ) x", "2 ( x + 1 right x"),
    "paren_paren":      ("( x + 1 ) ( x - 3 )", "( x + 1 right ( x - 3 right"),
    "nested_paren":     ("( ( x + 1 ) * 2 ) + 1", "( ( x + 1 right * 2 right + 1"),
    "paren_in_exp_out": ("( x ^ 2 ) * 3", "( x ^ 2 right right * 3"),
    "paren_frac_out":   ("( 1 / x ) + 1", "( 1 / x right right + 1"),
    "paren_sqrt_out":   ("( sqrt x ) * 2", "( sqrt x right right * 2"),
    "exp_fn_double":    ("2 ^ sin x ) ) + 1", "2 ^ sin x right right + 1"),
    "frac_den_fn":      ("1 / sin x ) + 2", "1 / sin x right right + 2"),
}
# Adjacent-number editor probe (Calculation/Calculus defect S1: `x * 3 LEFT 5`
# shows "x × 53" but Giac receives x*5*3). Style "naive" = the edit; "canonical"
# = typing the displayed digits directly.
ADJACENT = {
    "adj_mul":      ("x * 3 left 5", "x * 5 3"),
    "adj_plain":    ("3 left 5 + x", "5 + x 3"),
    "adj_after_x":  ("x 3 left 5", "x 5 3"),
    "adj_two":      ("1 2 left 7 * x", "7 1 2 * x"),
    "adj_exp":      ("x ^ 3 left 5", "x ^ 5 3"),
    "adj_den":      ("x / 3 left 5", "x / 5 3"),
    "adj_sqrt":     ("sqrt 3 left 5 right * x", "sqrt 5 3 right * x"),
    "adj_fn":       ("sin 3 left 5 right + x", "sin 5 3 right + x"),
    "adj_decimal":  ("2 . 5 left 1 * x", "1 2 . 5 * x"),
    "adj_dec2":     ("2 left . 5 * x", ". 5 2 * x"),
    "adj_paren":    ("( x + 3 left 5 right * 2", "( x + 5 3 right * 2"),
}
KINDS = {  # prefix keys, variable of the pattern, kind of check
    "y=f(x)": (["y", "="], "x", "param"),
    "x=f(y)": (["x", "="], "y", "param"),
    "implicit": (["y", "^", "2", "right", "="], "x", "residual"),
    "ineq_lt": (["y", "lt"], "x", "residual"),
    "ineq_gt": (["y", "gt"], "x", "residual"),
}
XS = (-1.5, 0.5, 2.0, 3.25)
PTS = ((0.5, 1.0), (2.0, -1.0), (3.25, 2.5), (-1.5, 0.25))


def keys_for(expr, var):
    out = []
    for k in expr.split():
        out.append(("y" if (var == "y" and k == "x") else k))
    return out


def run_one(args):
    pname, style, kname = args
    expr = PATTERNS[pname][0 if style == "naive" else 1]
    prefix, var, check = KINDS[kname]
    toks = prefix + keys_for(expr, var)
    lines = script_header("Grapher") + ["set_angle_mode rad", "key down", "key enter"]
    lines += [keyline(t) for t in toks] + ["key enter", "wait 3",
                                            f"dump_graph p 0 {' '.join(map(str, XS))}"]
    if check == "residual":
        lines += residual_lines(0, PTS)
    r = run_script(lines, EMU, timeout=10)
    d = r.by_tag.get("p") or {}
    res, fails = parse_residuals(r.stdout, r.stderr)
    return {"pattern": pname, "style": style, "kind": kname, "keys": " ".join(toks), "rc": r.returncode,
            "input": d.get("input"), "text": d.get("text"), "slot_kind": d.get("kind"),
            "valid": d.get("valid"), "reason": d.get("reason"), "giac": d.get("giac_rpn"),
            "giac_l": d.get("giac_rpn_l"), "values": d.get("values"), "residuals": res, "fails": fails}


def judge(rec):
    """WYSIWYC: NumOS values vs oracle of the displayed AST."""
    try:
        inp = parse(rec["input"])
    except Exception as exc:  # noqa: BLE001
        return {"verdict": f"unparsable {exc}"}
    lhs, op, rhs = split_relation(inp)
    _, var, check = KINDS[rec["kind"]]
    bad = []
    if not rec["valid"]:
        return {"verdict": "refused", "shown": linear(inp)}
    if check == "param":
        val = oracle_expr(rhs)
        sym = X if var == "x" else Y
        for t, v in rec["values"] or []:
            c = real_candidates(val, {sym: sp_rat(t)})
            if not agree(v, c):
                bad.append({"t": t, "numos": v, "oracle": c})
    else:
        lv, rv = oracle_expr(lhs), oracle_expr(rhs)
        for _s, x, y, v in rec["residuals"]:
            a = real_candidates(lv, {X: sp_rat(x), Y: sp_rat(y)})
            b = real_candidates(rv, {X: sp_rat(x), Y: sp_rat(y)})
            c = [None if (p is None or q is None) else p - q for p in a for q in b]
            if not agree(v, c, rel=1e-5, absol=2e-5):
                bad.append({"x": x, "y": y, "numos": v, "oracle": c})
        if rec["fails"]:
            # a NaN residual stops the script: accept only if the oracle is invalid there
            done = len(rec["residuals"])
            if done < len(PTS):
                x, y = PTS[done]
                a = real_candidates(lv, {X: sp_rat(x), Y: sp_rat(y)})
                b = real_candidates(rv, {X: sp_rat(x), Y: sp_rat(y)})
                if not any(p is None or q is None for p in a for q in b):
                    bad.append({"x": x, "y": y, "numos": "NaN", "oracle": [p - q for p in a for q in b]})
    return {"verdict": "MISMATCH" if bad else "ok", "bad": bad, "shown": linear(inp)}


def sp_rat(v):
    import sympy as sp
    return sp.Rational(str(v))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="out/correctness-e2e-01/grapher/adv-h01")
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--set", default="adv", choices=("adv", "adjacent"))
    a = ap.parse_args()
    if a.set == "adjacent":
        PATTERNS.clear()
        PATTERNS.update(ADJACENT)
    jobs = [(p, s, k) for p in PATTERNS for k in KINDS for s in ("naive", "canonical")]
    with ThreadPoolExecutor(a.jobs) as ex:
        recs = list(ex.map(run_one, jobs))
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    by = {}
    for r in recs:
        r["judge"] = judge(r)
        by[(r["pattern"], r["kind"], r["style"])] = r
    with open(out / "records.jsonl", "w", encoding="utf-8", newline="\n") as fh:
        for r in recs:
            fh.write(json.dumps(r, sort_keys=True, ensure_ascii=False) + "\n")
    rows = ["| pattern | kind | naive shown | naive==canonical tree | WYSIWYC naive | WYSIWYC canonical |",
            "|---|---|---|---|---|---|"]
    for p in PATTERNS:
        for k in KINDS:
            n, c = by[(p, k, "naive")], by[(p, k, "canonical")]
            same = n["input"] == c["input"]
            rows.append(f"| {p} | {k} | `{n['judge'].get('shown', n['input'])}` | {'yes' if same else 'NO'} | "
                        f"{n['judge']['verdict']} | {c['judge']['verdict']} |")
    (out / "adv_h01.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
    tot = len(recs)
    mism = sum(1 for r in recs if r["judge"]["verdict"] == "MISMATCH")
    refused = sum(1 for r in recs if r["judge"]["verdict"] == "refused")
    diff = sum(1 for p in PATTERNS for k in KINDS
               if by[(p, k, "naive")]["input"] != by[(p, k, "canonical")]["input"])
    print(json.dumps({"runs": tot, "wysiwyc_mismatch": mism, "refused": refused,
                      "naive_tree_differs": diff, "crash_rc": sum(1 for r in recs if r["rc"] not in (0, 4))}))


if __name__ == "__main__":
    main()
