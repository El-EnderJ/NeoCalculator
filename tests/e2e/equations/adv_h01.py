"""ADV-H01 pattern probe on Equations (CORRECTNESS-E2E-01 W5, task 1).

Equations still maps `)` to CursorController::moveRight() (EquationsApp.cpp
handleEditor: `case KeyCode::RPAREN: cc.moveRight();`). For each pattern the
intended equation is typed twice in one fresh emulator process:
  naive  - `)` closes brackets (absorbing fraction/power exits), or the literal
           human sequence given in the table;
  canon  - RIGHT leaves every slot.
Recorded per run: keys, displayed equation (dump AST), Giac-bound strings
(tutor snapshot `authored`, i.e. exactly what solveStructured received),
NumOS solutions, SymPy solveset of the DISPLAYED equation and of the INTENDED
equation, and each NumOS solution substituted into the displayed equation
(mpmath 60 digits).

Verdict per naive run:
  S1  display == intended (renders the same) but NumOS solves something else,
      or NumOS solutions != oracle(displayed);
  S2  Giac-bound text != displayed equation but same solutions;
  "differs, WYSIWYC holds" - the displayed equation differs from the
      intended one and NumOS solves exactly what is displayed;
  OK  naive keys build the intended equation and the answer is right.

    py -3.12 tests/e2e/equations/adv_h01.py [--emulator PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import sympy as sp  # noqa: E402

from common import keys as K  # noqa: E402
from common.emulator import DEFAULT_EMULATOR, script_header  # noqa: E402
from common.expr import Abs, Const, Fn, Frac, LogB, Num, Op, Paren, Pow, Row, Sqrt, Var  # noqa: E402
from common.giacparse import parse_eval  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from equations import flow  # noqa: E402
from equations.model import eq_linear, eq_struct, eq_tokens, residual, split_eq, to_lines  # noqa: E402
from equations.oracle import sympy_solveset, to_mp  # noqa: E402


def R(*items):
    return Row(tuple(items))


N, X = Num, Var("x")
PATTERNS = [
    # (name, lhs, rhs, policy, literal naive keys or None)
    ("group then ×: (x+1)·2=6", R(Paren(R(X, Op("+"), N("1"))), Op("*"), N("2")), R(N("6")), "real", None),
    ("implicit product: 2(x+1)=6", R(N("2"), Paren(R(X, Op("+"), N("1")))), R(N("6")), "real", None),
    ("bracket inside exponent: 2^(x+1)·3=24", R(Pow(N("2"), R(Paren(R(X, Op("+"), N("1"))))), Op("*"), N("3")), R(N("24")), "real",
     "2 ^ ( x + 1 ) * 3 = 2 4"),
    ("bracket inside exponent, then RIGHT: 2^(x+1)·3=24", R(Pow(N("2"), R(Paren(R(X, Op("+"), N("1"))))), Op("*"), N("3")), R(N("24")), "real",
     "2 ^ ( x + 1 ) right * 3 = 2 4"),
    ("bracket inside denominator: 1/(x+2)·4=1", R(Frac(R(N("1")), R(Paren(R(X, Op("+"), N("2"))))), Op("*"), N("4")), R(N("1")), "real",
     "/ 1 right ( x + 2 ) * 4 = 1"),
    ("bracket inside numerator: (x+2)/3·4=8", R(Frac(R(Paren(R(X, Op("+"), N("2")))), R(N("3"))), Op("*"), N("4")), R(N("8")), "real", None),
    ("group around fraction: ((x+1)/2+1)·2=6", R(Paren(R(Frac(R(Paren(R(X, Op("+"), N("1")))), R(N("2"))), Op("+"), N("1"))), Op("*"), N("2")), R(N("6")), "real", None),
    ("root radicand group: √((x+3))·2=4", R(Sqrt(R(Paren(R(X, Op("+"), N("3"))))), Op("*"), N("2")), R(N("4")), "real",
     "sqrt ( x + 3 ) * 2 = 4"),
    ("sqrt typed like sqrt( : √(x+3)·2=4", R(Sqrt(R(X, Op("+"), N("3"))), Op("*"), N("2")), R(N("4")), "real", None),
    ("log base then argument: log_2(x)·3=9", R(LogB(R(N("2")), R(X)), Op("*"), N("3")), R(N("9")), "real", None),
    ("log argument ends in fraction: log_2(x/4)·3=3", R(LogB(R(N("2")), R(Frac(R(X), R(N("4")), capture=True))), Op("*"), N("3")), R(N("3")), "real", None),
    ("ln argument ends in power: ln(e^x)·3=6", R(Fn("ln", R(Pow(Const("e"), R(X)))), Op("*"), N("3")), R(N("6")), "real", None),
    ("abs then ×: |x-3|·2=4", R(Abs(R(X, Op("-"), N("3"))), Op("*"), N("2")), R(N("4")), "real", None),
    ("abs ends in fraction: |-(x/2)|·4=8", R(Abs(R(Op("-"), Frac(R(X), R(N("2")), capture=True))), Op("*"), N("4")), R(N("8")), "real", None),
    ("nested groups: ((x+2)·3)+1=7", R(Paren(R(Paren(R(X, Op("+"), N("2"))), Op("*"), N("3"))), Op("+"), N("1")), R(N("7")), "real", None),
    ("nested group ends in fraction: (x+1/2)·4=6", R(Paren(R(X, Op("+"), Frac(R(N("1")), R(N("2")), capture=True))), Op("*"), N("4")), R(N("6")), "real", None),
    ("nested group ends in fraction, literal", R(Paren(R(X, Op("+"), Frac(R(N("1")), R(N("2")), capture=True))), Op("*"), N("4")), R(N("6")), "real",
     "( x + 1 / 2 ) * 4 = 6"),
    ("function power: sin(x)^2=1/4", R(Pow(Fn("sin", R(X)), R(N("2")))), R(Frac(R(N("1")), R(N("4")))), "real", None),
    ("function power, literal sin(x)^2=1/4", R(Pow(Fn("sin", R(X)), R(N("2")))), R(Frac(R(N("1")), R(N("4")))), "real",
     "sin x ) ^ 2 right = 1 / 4"),
    ("function arg ends in power: ln(x^2)+1=1", R(Fn("ln", R(Pow(X, R(N("2"))))), Op("+"), N("1")), R(N("1")), "real", None),
    ("implicit then group: 2(x+1)·3=12", R(N("2"), Paren(R(X, Op("+"), N("1"))), Op("*"), N("3")), R(N("12")), "real", None),
    ("implicit, group ends in fraction: 2(1+x/3)·3=12", R(N("2"), Paren(R(N("1"), Op("+"), Frac(R(X), R(N("3")), capture=True))), Op("*"), N("3")), R(N("12")), "real", None),
    ("x^2 inside group, literal: (x^2+1)·2=10", R(Paren(R(Pow(X, R(N("2"))), Op("+"), N("1"))), Op("*"), N("2")), R(N("10")), "real",
     "( x ^ 2 ) + 1 ) * 2 = 1 0"),
    ("x^2 inside group, missing exponent exit: (x^2+1)·2=10", R(Paren(R(Pow(X, R(N("2"))), Op("+"), N("1"))), Op("*"), N("2")), R(N("10")), "real",
     "( x ^ 2 + 1 ) * 2 = 1 0"),
    ("RHS brackets: 6=(x+1)·2", R(N("6")), R(Paren(R(X, Op("+"), N("1"))), Op("*"), N("2")), "real", None),
    ("RHS exponent group, literal: x=2^(1+1)·3", R(X), R(Pow(N("2"), R(Paren(R(N("1"), Op("+"), N("1"))))), Op("*"), N("3")), "real",
     "x = 2 ^ ( 1 + 1 ) * 3"),
    ("both sides: (x+1)^2=(2x+2)", R(Pow(Paren(R(X, Op("+"), N("1"))), R(N("2")))), R(Paren(R(N("2"), X, Op("+"), N("2")))), "real", None),
    ("both sides, literal: (x+1)^2=(2x+2)", R(Pow(Paren(R(X, Op("+"), N("1"))), R(N("2")))), R(Paren(R(N("2"), X, Op("+"), N("2")))), "real",
     "( x + 1 ) ^ 2 = ( 2 x + 2 )"),
    ("fraction both sides, literal: (x+1)/(x-1)=3", R(Frac(R(X, Op("+"), N("1")), R(X, Op("-"), N("1")))), R(N("3")), "real",
     "( x + 1 ) / ( x - 1 ) = 3"),
    ("power of group, literal: (x-1)^(2)=4", R(Pow(Paren(R(X, Op("-"), N("1"))), R(N("2")))), R(N("4")), "real",
     "( x - 1 ) ^ ( 2 ) = 4"),
]


def solutions_of(d):
    from common.giacparse import parse_eval as pe
    env = {"e": sp.E}
    out = []
    for g in d.get("groups") or []:
        for v in g:
            try:
                out.append(pe(v["exact"], "rad", env))
            except Exception:  # noqa: BLE001
                out.append(None)
    return out


def oracle_text(res):
    if res[0] == "finite":
        return "{" + ", ".join(str(v) for v in res[1]) + "}"
    if res[0] == "periodic":
        return " ∪ ".join(f"{o}+{p}ℤ" for o, p in res[1]) + (f" ∪ {res[2]}" if res[2] else "")
    return f"{res[0]} {res[1] if res[1] else ''}".strip()


def numos_text(d):
    if not d:
        return "missing dump"
    sk = {0: "solutions", 1: "no solution", 2: "all values", 3: "unsupported", 4: "periodic"}.get(d.get("set_kind"), d.get("set_kind"))
    if d.get("status") != "ok":
        return f"{d.get('status')} ({d.get('diag')})"
    if d.get("set_kind") == 4:
        fams = (d.get("view") or {}).get("families", [])
        return "periodic " + " ∪ ".join(f"{f['offset']}+{f['period']}ℤ" for f in fams)
    vals = [v["exact"] for g in d.get("groups") or [] for v in g]
    return f"{sk} {{{', '.join(vals)}}}" if vals else str(sk)


def same_set(res, d):
    """Does NumOS's answer equal oracle set `res` (displayed equation)?"""
    if d is None:
        return False
    if res[0] in ("incomplete",):
        return d.get("status") == "parse_error"
    if d.get("status") != "ok":
        return False
    sk = d.get("set_kind")
    if res[0] == "empty":
        return sk == 1
    if res[0] == "all":
        return sk == 2
    if res[0] == "finite":
        if sk != 0:
            return False
        got = solutions_of(d)
        if any(g is None for g in got) or len(got) != len(res[1]):
            return False
        import mpmath
        for t in res[1]:
            tn = to_mp(t)
            if not any(g is not None and abs(to_mp(g) - tn) < mpmath.mpf(10) ** -30 * max(1, abs(tn)) for g in got):
                return False
        return True
    if res[0] == "periodic":
        if sk != 4:
            return False
        import mpmath
        fams = (d.get("view") or {}).get("families", [])
        env = {"e": sp.E}
        mine = [(parse_eval(f["offset"], "rad", env), parse_eval(f["period"], "rad", env)) for f in fams]
        return members(mine) == members(res[1])
    return False


def members(fams, lo=-30, hi=30):
    import mpmath
    out = set()
    for o, p in fams:
        on, pn = float(sp.N(o, 30)), float(sp.N(p, 30))
        k0 = int((lo - on) // pn) - 1
        for k in range(k0, k0 + int((hi - lo) / pn) + 4):
            v = on + k * pn
            if lo <= v <= hi:
                out.add(round(v, 9))
    return out


def run(emulator, out_dir):
    lines = script_header("Equations")
    plan = []
    for i, (name, lhs, rhs, policy, literal) in enumerate(PATTERNS):
        canon, _ = eq_tokens(lhs, rhs, "direct")
        if literal:
            naive = [("key", k) for k in literal.split()]
        else:
            naive, absorbed = eq_tokens(lhs, rhs, "direct", naive=True)
        for tag, toks in (("naive", naive), ("canon", canon)):
            lines += flow.settings(policy, "rad") + flow.RESET + flow.add_equation(to_lines(toks)) + flow.solve_lines(f"p{i}|{tag}")
        plan.append((name, lhs, rhs, policy, naive, canon))
    r = flow.run_script(lines, emulator, workdir=str(out_dir / "tmp"), timeout=300)
    rows = []
    for i, (name, lhs, rhs, policy, naive, canon) in enumerate(plan):
        intended = eq_struct(lhs, rhs)
        o_int = sympy_solveset(intended, policy)
        row = {"pattern": name, "intended": eq_linear(lhs, rhs), "oracle_intended": oracle_text(o_int)}
        for tag, toks in (("naive", naive), ("canon", canon)):
            d = r.by_tag.get(f"p{i}|{tag}")
            disp = parse(d["equations"][0]) if d and d.get("equations") else None
            o_disp = sympy_solveset(disp, policy) if disp else ("incomplete", "no row")
            authored = (d.get("trace") or {}).get("authored") if d else None
            # I2(b): Giac-bound text vs displayed equation, sampled at x=7/3, -13/5
            i2 = None
            parts = split_eq(disp) if disp else None
            if authored and parts:
                from equations.oracle import sym_side
                bad = []
                for side, txt in zip(parts, authored[0]):
                    try:
                        a = sym_side(side)
                        b = parse_eval(txt, "rad", {"e": sp.E})
                        for xv in (sp.Rational(7, 3), sp.Rational(-13, 5)):
                            va, vb = a.subs(sp.Symbol("x"), xv), b.subs(sp.Symbol("x"), xv)
                            if sp.N(va - vb, 30) != 0 and abs(complex(sp.N(va - vb, 30))) > 1e-25:
                                bad.append((txt, str(xv)))
                    except Exception as exc:  # noqa: BLE001
                        bad.append(("unparsed", str(exc)[:80]))
                i2 = "ok" if not bad else f"MISMATCH {bad}"
            # each NumOS solution substituted into the displayed equation
            subs = []
            if parts and d and d.get("status") == "ok" and d.get("set_kind") == 0:
                for v in solutions_of(d):
                    if v is None:
                        subs.append("unparsed")
                        continue
                    ok, info = residual(parts[0], parts[1], {"x": to_mp(v)}, "rad", policy == "real")
                    subs.append("ok" if ok else f"FAIL {info}")
            agrees = same_set(o_disp, d)
            same_tree = disp == intended
            if not agrees:
                verdict = "S1: NumOS solution set != oracle(displayed equation)"
            elif i2 and i2 != "ok":
                verdict = "S2: Giac-bound text != displayed equation (same solutions)"
            elif same_tree:
                verdict = "OK: builds the intended equation, correct solutions"
            elif o_disp[0] == "incomplete" or (d and d.get("status") == "parse_error"):
                verdict = "refused: displayed row is not one complete equality (the `=` went into a slot); error shown, no answer"
            else:
                verdict = "differs from intended; WYSIWYC holds (NumOS solves exactly the displayed equation)"
            row[tag] = {"keys": " ".join(t[1] for t in toks), "displayed": slinear(disp) if disp else None,
                        "giac_bound": authored, "status": d.get("status") if d else None,
                        "numos": numos_text(d), "oracle_displayed": oracle_text(o_disp),
                        "substitution": subs, "i2_giac_vs_display": i2,
                        "same_as_intended": same_tree, "verdict": verdict}
        rows.append(row)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "adv_h01.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
    (out_dir / "adv_h01.stdout.txt").write_text(r.stdout, encoding="utf-8")
    md = ["| pattern | naive keys | displayed equation | Giac-bound | NumOS | oracle(displayed) | oracle(intended) | verdict |",
          "|---|---|---|---|---|---|---|---|"]
    for row in rows:
        n = row["naive"]
        md.append(f"| {row['pattern']} | `{n['keys']}` | {n['displayed']} | `{n['giac_bound']}` | {n['numos']} | "
                  f"{n['oracle_displayed']} | {row['oracle_intended']} | {n['verdict']} |")
    md.append("")
    md.append("Canonical (RIGHT) path: " + "; ".join(f"{row['pattern']}: {row['canon']['verdict'].split(':')[0]}" for row in rows))
    (out_dir / "adv_h01.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return rows, r


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "equations" / "adv-h01"))
    a = ap.parse_args()
    rows, r = run(a.emulator, Path(a.out))
    sys.stdout.reconfigure(encoding="utf-8")
    print(Path(a.out, "adv_h01.md").read_text(encoding="utf-8"))
    print("rc", r.returncode, r.stderr[-300:])
