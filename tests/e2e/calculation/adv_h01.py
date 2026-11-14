"""ADV-H01 pattern probe on Calculation (CORRECTNESS-E2E-01 item 7).

For each pattern: the intended tree, the naive `)`-closing keystrokes a human
would type, and the canonical (RIGHT) keystrokes. Both are run on the
emulator; the dump gives the displayed input AST, the Giac-bound text and the
result. Verdict:
  * naive tree == intended                     -> OK
  * naive tree != intended, renders differently, result == oracle(displayed)
      -> not a wrong answer (display shows what is computed); in Calculation
         `)` is plain RIGHT, so it leaves only the innermost slot
  * naive tree != intended, renders identically -> S1 (WYSIWYC hidden)
  * result != oracle(displayed)                 -> S1 (wrong value)

    python tests/e2e/calculation/adv_h01.py [--emulator PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

from common import keys as K  # noqa: E402
from common.emulator import DEFAULT_EMULATOR, run_script, script_header  # noqa: E402
from common.evaluator import Ctx, close, evaluate  # noqa: E402
from common.expr import (Abs, Const, Fn, Frac, LogB, Num, Op, Paren, Pow, Row, Sqrt, Var,  # noqa: E402
                         linear, to_struct)
from common.invariants import compare_value, visual  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from calculation.driver import ENV, preamble  # noqa: E402


def R(*items):
    return Row(tuple(items))


N = Num
X = Var("x")
PATTERNS = [
    ("group then ×", R(Paren(R(X, Op("+"), N("1"))), Op("*"), N("2"))),
    ("group then × (numeric)", R(Paren(R(N("2"), Op("+"), N("1"))), Op("*"), N("2"))),
    ("bracket closed inside exponent", R(Pow(N("2"), R(Paren(R(N("1"), Op("+"), N("1"))))), Op("*"), N("3"))),
    ("bracket closed inside denominator", R(Frac(R(N("1")), R(Paren(R(N("2"), Op("+"), N("3"))))), Op("*"), N("4"))),
    ("bracket closed inside numerator", R(Frac(R(Paren(R(N("1"), Op("+"), N("2")))), R(N("3"))), Op("*"), N("4"))),
    ("group around fraction then ×", R(Paren(R(Frac(R(N("1")), R(N("2"))), Op("+"), N("1"))), Op("*"), N("2"))),
    ("root radicand group then ×", R(Sqrt(R(Paren(R(N("1"), Op("+"), N("3"))))), Op("*"), N("2"))),
    ("log base then argument then ×", R(LogB(R(N("2")), R(N("8"))), Op("*"), N("3"))),
    ("log argument ends in fraction", R(LogB(R(N("2")), R(Frac(R(N("1")), R(N("4"))))), Op("*"), N("3"))),
    ("ln argument ends in power", R(Fn("ln", R(Pow(Const("e"), R(N("2"))))), Op("*"), N("3"))),
    ("abs then ×", R(Abs(R(Op("-"), N("3"), Op("+"), N("1"))), Op("*"), N("2"))),
    ("abs ends in fraction then ×", R(Abs(R(Op("-"), Frac(R(N("1")), R(N("2"))))), Op("*"), N("4"))),
    ("nested groups", R(Paren(R(Paren(R(N("1"), Op("+"), N("2"))), Op("*"), N("3"))), Op("+"), N("1"))),
    ("nested group ends in fraction", R(Paren(R(N("1"), Op("+"), Frac(R(N("1")), R(N("2"))))), Op("*"), N("4"))),
    ("function power sin(x)^2", R(Pow(Fn("sin", R(X)), R(N("2"))))),
    ("function power sin(π/6)^2", R(Pow(Fn("sin", R(Frac(R(Const("pi")), R(N("6"))))), R(N("2"))))),
    ("function arg ends in power then +", R(Fn("cos", R(Pow(N("2"), R(N("2"))))), Op("+"), N("1"))),
    ("implicit product then group", R(N("2"), Paren(R(X, Op("+"), N("1"))), Op("*"), N("3"))),
    ("implicit product, group ends in fraction", R(N("2"), Paren(R(N("1"), Op("+"), Frac(R(N("1")), R(N("3"))))), Op("*"), N("3"))),
    ("sqrt typed like sqrt( ... )", R(Sqrt(R(N("1"), Op("+"), N("3"))), Op("*"), N("2"))),
    # Literal human sequences (no RIGHT at all): the orchestrator's probe.
    ("exponent group, no RIGHT: 2^(1+1)*3", R(Pow(N("2"), R(Paren(R(N("1"), Op("+"), N("1"))))), Op("*"), N("3")),
     "2 ^ ( 1 + 1 ) * 3"),
    ("fraction den group, no RIGHT: 1/(2+3)*4", R(Frac(R(N("1")), R(Paren(R(N("2"), Op("+"), N("3"))))), Op("*"), N("4")),
     "/ 1 right ( 2 + 3 ) * 4"),
    ("sqrt group, no RIGHT: sqrt((1+3))*2", R(Sqrt(R(Paren(R(N("1"), Op("+"), N("3"))))), Op("*"), N("2")),
     "sqrt ( 1 + 3 ) * 2"),
    ("x^2 inside group, no RIGHT: (x^2+1)*2", R(Paren(R(Pow(X, R(N("2"))), Op("+"), N("1"))), Op("*"), N("2")),
     "( x ^ 2 ) + 1 ) * 2"),
]


def run(emulator, out_dir):
    lines = script_header("Calculation") + preamble() + ["set_angle_mode rad"]
    plan = []
    for i, pat in enumerate(PATTERNS):
        name, tree = pat[0], pat[1]
        raw = K._raw(tree, "direct")
        naive, absorbed = K._naive(raw)
        if len(pat) > 2:
            naive = [("key", k) for k in pat[2].split()]
        canon = K._canonical(raw)
        for tag, toks in (("naive", naive), ("canon", canon)):
            lines += ["key ac", "key ac"] + K.to_lines(toks) + ["key enter", f"dump_calc p{i}|{tag} tree"]
        plan.append((name, tree, naive, canon, absorbed))
    r = run_script(lines, emulator, timeout=120)
    ctx = Ctx(angle="rad", env=dict(ENV))
    rows = []
    for i, (name, tree, naive, canon, absorbed) in enumerate(plan):
        intended = to_struct(tree)
        o_int = evaluate(intended, ctx)
        row = {"pattern": name, "intended": linear(tree), "intended_value": o_int.text()}
        for tag, toks in (("naive", naive), ("canon", canon)):
            d = r.by_tag.get(f"p{i}|{tag}")
            inp = parse(d["input"])
            o_disp = evaluate(inp, ctx)
            verdict, sev, info = compare_value(o_disp, d, ctx)
            same = inp == intended
            vis_same = visual(inp) == visual(intended)
            if verdict != "ok":
                v = f"{sev}: result disagrees with oracle(displayed) ({info.get('reason')})"
            elif same:
                v = "OK: builds the intended tree"
            elif vis_same:
                v = "S1: different tree that renders identically (WYSIWYC hidden)"
            else:
                v = ("not a wrong answer: different tree from the intended one, but the input "
                 "display shows exactly the computed structure")
            row[tag] = {"keys": K.to_compact(toks), "displayed_input": slinear(inp),
                        "giac_in": d.get("giac_in"), "status": d.get("status"), "exact": d.get("exact"),
                        "displayed_value_oracle": o_disp.text(), "verdict": v,
                        "dumpTree": d.get("tree")}
        rows.append(row)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "adv_h01.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
    md = ["| pattern | naive keys | displayed input | NumOS value | oracle(displayed) | oracle(intended) | verdict |",
          "|---|---|---|---|---|---|---|"]
    for row in rows:
        n = row["naive"]
        md.append(f"| {row['pattern']} | `{n['keys']}` | {n['displayed_input']} | {n['exact']} | "
                  f"{n['displayed_value_oracle']} | {row['intended_value']} | {n['verdict']} |")
    md.append("")
    md.append("Canonical (RIGHT) path verdicts: " + "; ".join(
        f"{row['pattern']}: {row['canon']['verdict'].split(':')[0]}" for row in rows))
    (out_dir / "adv_h01.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return rows, r


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "calculation" / "adv-h01"))
    a = ap.parse_args()
    rows, r = run(a.emulator, Path(a.out))
    print(Path(a.out, "adv_h01.md").read_text(encoding="utf-8"))
    print("rc", r.returncode)
