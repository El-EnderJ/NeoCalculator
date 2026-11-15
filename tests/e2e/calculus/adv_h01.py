"""ADV-H01 close-bracket probe on the Calculus app (W4 task 1).

ADV-H01 (Grapher, fixed there by closeParen()): typing `(x+1)*2` evaluated
as 4 at x=2. CalculusApp.cpp:650 still maps `)` (RPAREN) to moveRight().

For every pattern and BOTH tabs (Derivative, Integral):
  * naive keys  – what a human types, closing brackets with `)`
  * canon keys  – RIGHT to leave every slot (the intended tree)
Each is run on the emulator; the dump gives the displayed input AST, the
Giac-bound text and the result. Verdict per (pattern, tab, keys):
  OK         naive tree == intended tree and the result matches the oracle
  NOT-WRONG  naive tree != intended (renders differently), result equals the
             oracle of the DISPLAYED tree (the screen shows what is computed)
  S1-hidden  naive tree != intended but renders identically
  S1-value   result != oracle(displayed)       (wrong value shown)

    python tests/e2e/calculus/adv_h01.py [--emulator PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

from common.emulator import DEFAULT_EMULATOR, run_script, script_header  # noqa: E402
from common.expr import (Abs, Const, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row, Sqrt,  # noqa: E402
                         Var)
from common.invariants import visual  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from common import keys as K  # noqa: E402
from calculus import model as M  # noqa: E402
from calculus.check import judge  # noqa: E402
from calculus.semantics import evaluate_expr  # noqa: E402


def R(*items):
    return Row(tuple(items))


N = Num
X = Var("x")
PATTERNS = [
    ("(x+1)*2  [ADV-H01 original]", R(Paren(R(X, Op("+"), N("1"))), Op("*"), N("2")), "( x + 1 ) * 2"),
    ("(x+1)*2 typed with RIGHT", R(Paren(R(X, Op("+"), N("1"))), Op("*"), N("2")), "( x + 1 right * 2"),
    ("x^(1+1)*3  bracket inside exponent", R(Pow(X, R(Paren(R(N("1"), Op("+"), N("1"))))), Op("*"), N("3")),
     "x ^ ( 1 + 1 ) * 3"),
    ("2^(x+1)*3  bracket inside exponent", R(Pow(N("2"), R(Paren(R(X, Op("+"), N("1"))))), Op("*"), N("3")),
     "2 ^ ( x + 1 ) * 3"),
    ("e^(x+1)*2  bracket inside exponent", R(Pow(Const("e"), R(Paren(R(X, Op("+"), N("1"))))), Op("*"), N("2")),
     "e ^ ( x + 1 ) * 2"),
    ("1/(x+3)*4  bracket inside denominator", R(Frac(R(N("1")), R(Paren(R(X, Op("+"), N("3"))))), Op("*"), N("4")),
     "/ 1 right ( x + 3 ) * 4"),
    ("(x+2)/3*4  bracket inside numerator", R(Frac(R(Paren(R(X, Op("+"), N("2")))), R(N("3"))), Op("*"), N("4")),
     "/ ( x + 2 ) right 3 ) * 4"),
    ("sqrt((x+3))*2  bracket inside root", R(Sqrt(R(Paren(R(X, Op("+"), N("3"))))), Op("*"), N("2")),
     "sqrt ( x + 3 ) * 2"),
    ("sqrt(x+3)*2  root closed with )", R(Sqrt(R(X, Op("+"), N("3"))), Op("*"), N("2")), "sqrt x + 3 ) * 2"),
    ("nthroot 3 of (x+1) then *2 (Toolbox)", R(Root(R(N("3")), R(Paren(R(X, Op("+"), N("1"))))), Op("*"), N("2")),
     None),
    ("log_2(x+1)*3  log base/argument", R(LogB(R(N("2")), R(Paren(R(X, Op("+"), N("1"))))), Op("*"), N("3")),
     "logbase 2 right ( x + 1 ) * 3"),
    ("log_2(x)*3  log argument closed with )", R(LogB(R(N("2")), R(X)), Op("*"), N("3")),
     "logbase 2 right x ) * 3"),
    ("ln(x^2)*3  ln argument ends in power", R(Fn("ln", R(Pow(X, R(N("2"))))), Op("*"), N("3")),
     "ln x ^ 2 ) * 3"),
    ("ln(x/2)*3  ln argument ends in fraction", R(Fn("ln", R(Frac(R(X), R(N("2")), capture=True))), Op("*"), N("3")),
     "ln x / 2 ) * 3"),
    ("|x+1|*2  abs (Toolbox)", R(Abs(R(X, Op("+"), N("1"))), Op("*"), N("2")), None),
    ("|x/2|*4  abs ends in fraction", R(Abs(R(Frac(R(X), R(N("2")), capture=True))), Op("*"), N("4")), None),
    ("((x+1)*3)+1  nested groups", R(Paren(R(Paren(R(X, Op("+"), N("1"))), Op("*"), N("3"))), Op("+"), N("1")),
     "( ( x + 1 ) * 3 ) + 1"),
    ("(x+1/2)*4  nested group ends in fraction", R(Paren(R(X, Op("+"), Frac(R(N("1")), R(N("2")), capture=True))),
                                                Op("*"), N("4")), "( x + 1 / 2 ) * 4"),
    ("sin(x)^2  function power", R(Pow(Fn("sin", R(X)), R(N("2")))), "sin x ) ^ 2"),
    ("sin(x^2)+1  function arg ends in power", R(Fn("sin", R(Pow(X, R(N("2"))))), Op("+"), N("1")),
     "sin x ^ 2 ) + 1"),
    ("cos(2x)^3  function power", R(Pow(Fn("cos", R(N("2"), X)), R(N("3")))), "cos 2 x ) ^ 3"),
    ("2(x+1)*3  after implicit multiplication", R(N("2"), Paren(R(X, Op("+"), N("1"))), Op("*"), N("3")),
     "2 ( x + 1 ) * 3"),
    ("2(x+1/3)*3  implicit product, group ends in fraction",
     R(N("2"), Paren(R(X, Op("+"), Frac(R(N("1")), R(N("3")), capture=True))), Op("*"), N("3")),
     "2 ( x + 1 / 3 ) * 3"),
    ("(x^2+1)*2  power inside group", R(Paren(R(Pow(X, R(N("2"))), Op("+"), N("1"))), Op("*"), N("2")),
     "( x ^ 2 ) + 1 ) * 2"),
    ("(x^2+1)*2  power inside group, one )", R(Paren(R(Pow(X, R(N("2"))), Op("+"), N("1"))), Op("*"), N("2")),
     "( x ^ 2 + 1 ) * 2"),
    ("1/(x+1)^2  power of group in denominator", R(Frac(R(N("1")), R(Pow(Paren(R(X, Op("+"), N("1"))), R(N("2")))))),
     "/ 1 right ( x + 1 ) ^ 2"),
    ("x(x+1)^2*3  implicit, power of group", R(X, Pow(Paren(R(X, Op("+"), N("1"))), R(N("2"))), Op("*"), N("3")),
     "x ( x + 1 ) ^ 2 ) * 3"),
]


def run(emulator, out_dir):
    lines = script_header("Calculus") + ["set_angle_mode rad"]
    plan = []
    for i, (name, tree, literal) in enumerate(PATTERNS):
        rawt = M.raw(tree, "direct")
        if literal:
            naive = [("key", k) for k in literal.split()]
        else:
            naive, _ = M.naive(rawt)
        canon = M.canonical(rawt)
        for op, modekey in (("diff", "f1"), ("int", "f2")):
            for tag, toks in (("naive", naive), ("canon", canon)):
                lines += ["key ac", "key ac", f"key {modekey}"] + K.to_lines(toks) + \
                         ["key enter", f"dump_calculus p{i}|{op}|{tag}"]
        plan.append((name, tree, naive, canon))
    r = run_script(lines, emulator, timeout=300)
    rows = []
    for i, (name, tree, naive, canon) in enumerate(plan):
        intended = M.to_struct(tree)
        f_int = evaluate_expr(intended, "rad")[0]
        row = {"pattern": name, "intended": M.linear(tree), "intended_f": str(f_int)}
        for op in ("diff", "int"):
            for tag, toks in (("naive", naive), ("canon", canon)):
                d = r.by_tag.get(f"p{i}|{op}|{tag}")
                if not d:
                    row[f"{op}|{tag}"] = {"verdict": "NO DUMP"}
                    continue
                inp = parse(d["input"])
                j = judge(op, "rad", inp, d)
                same = inp == intended
                vis = visual(inp) == visual(intended)
                bad = [f for f in j["findings"] if f[0] == "I1" or f[0] == "I2"]
                if bad and any(f[1] == "S1" for f in bad):
                    v = "S1-value: " + "; ".join(f[3] for f in bad)
                elif j["verdict"] == "skip":
                    v = "SKIP: " + j.get("skip", "")
                elif same:
                    v = "OK"
                elif vis:
                    v = "S1-hidden: different tree that renders identically"
                else:
                    v = "NOT-WRONG: different tree, display shows the computed structure"
                f_disp = evaluate_expr(inp, "rad")[0]
                row[f"{op}|{tag}"] = {
                    "keys": K.to_compact(toks), "displayed_input": slinear(inp), "giac_in": d.get("giac_in"),
                    "status": d.get("status"), "exact": d.get("exact"), "variable": d.get("variable"),
                    "display": slinear(parse(d["display"])) if d.get("display") not in (None, "null") else None,
                    "oracle_f_displayed": str(f_disp), "i2b": j.get("i2b"), "value_vs_displayed": j.get("value"),
                    "findings": [list(f[:4]) for f in j["findings"]], "verdict": v}
        rows.append(row)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "adv_h01.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
    md = ["| pattern | tab | naive keys | displayed input | Giac-bound | NumOS result | f(displayed) | f(intended) | verdict |",
          "|---|---|---|---|---|---|---|---|---|"]
    for row in rows:
        for op in ("diff", "int"):
            n = row[f"{op}|naive"]
            md.append(f"| {row['pattern']} | {op} | `{n.get('keys')}` | {n.get('displayed_input')} | "
                      f"`{n.get('giac_in')}` | `{n.get('exact')}` | {n.get('oracle_f_displayed')} | "
                      f"{row['intended_f']} | {n['verdict']} |")
    md.append("")
    md.append("Canonical (RIGHT) path verdicts: " + "; ".join(
        f"{row['pattern']} [{op}]: {row[op + '|canon']['verdict'].split(':')[0]}"
        for row in rows for op in ("diff", "int")))
    (out_dir / "adv_h01.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return rows, r


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "calculus" / "adv-h01"))
    a = ap.parse_args()
    rows, r = run(a.emulator, Path(a.out))
    print(Path(a.out, "adv_h01.md").read_text(encoding="utf-8"))
    print("rc", r.returncode)
