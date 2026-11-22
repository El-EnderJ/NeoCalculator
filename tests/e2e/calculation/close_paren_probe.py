"""W1 task 1: independent re-test of `)` (and editing around it) in Calculation.

Question: is there ANY keystroke sequence around `)` / bracket editing where
the DISPLAYED input and the COMPUTED tree differ (a true I2 violation)?

For every pattern the emulator runs the literal keys, then `dump_calc ... tree`
and a screenshot of the frame. The verdict compares
  * oracle(displayed input AST)  vs  NumOS result     (value of what is shown)
  * the Giac-bound string        vs  displayed AST    (I2(b), independent reader)
  * `visual()` of the input AST (adjacent numbers concatenate, as the eye sees)
    vs the human reading, to catch trees that RENDER like something else.
Screenshots (PNG) let a reviewer check the rendered input by eye.

    python tests/e2e/calculation/close_paren_probe.py [--emulator PATH] [--out DIR]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

from common.emulator import DEFAULT_EMULATOR, run_script, script_header  # noqa: E402
from common.evaluator import Ctx, evaluate  # noqa: E402
from common.invariants import check_serialization, compare_value, visual  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from calculation.driver import ENV, preamble  # noqa: E402

# (name, keys typed after AC AC, what a human reads on the keypad / expects)
PATTERNS = [
    # ── ADV-H01 core ──────────────────────────────────────────────────────
    ("adv: (x+1)*2", "( x + 1 ) * 2", "(x+1)*2"),
    ("adv: (2+1)*2", "( 2 + 1 ) * 2", "6"),
    ("adv: 2(1+1)*3 implicit", "2 ( 1 + 1 ) * 3", "12"),
    ("adv: 2^(1+1)*3 no RIGHT", "2 ^ ( 1 + 1 ) * 3", "12 (human) / 64 (tree)"),
    ("adv: 1/(2+3)*4 no RIGHT", "/ 1 right ( 2 + 3 ) * 4", "4/5 (human)"),
    ("adv: sqrt((1+3))*2 no RIGHT", "sqrt ( 1 + 3 ) * 2", "4 (human)"),
    ("adv: sin(30))^2 DEG-free", "sin 3 0 ) ^ 2", "sin(30)^2"),
    ("adv: ((1+2)*3)+1", "( ( 1 + 2 ) * 3 ) + 1", "10"),
    ("adv: (1+2)(3+4)", "( 1 + 2 ) ( 3 + 4 )", "21"),
    ("adv: (1+2)3 implicit after group", "( 1 + 2 ) 3", "9"),
    ("adv: 2(3) implicit", "2 ( 3 )", "6"),
    ("adv: ln(e^2)*3 naive", "ln e ^ 2 ) * 3", "6 (human)"),
    ("adv: (2/3)^2", "( / 2 right 3 right ) ^ 2", "4/9"),
    ("adv: (x+1)^2 ) )", "( x + 1 ) ^ 2", "(x+1)^2"),
    # ── stray / extra `)` ─────────────────────────────────────────────────
    ("stray ) at end wraps to row start, then digit", "2 + 3 ) 5", "2+3 then 5?"),
    ("stray ) at end then *4", "2 + 3 ) * 4", "(2+3)*4 (human)"),
    ("stray ) after a closed group", "( 1 + 2 ) ) * 3", "(1+2)*3"),
    ("double ) leaves fn and paren", "( sin 3 0 ) ) * 2", "sin(30)*2"),
    ("stray ) then 7 at row start", "1 2 ) 7", "127? / 7*12?"),
    # ── typing digits next to an existing number (cursor not at end) ──────
    ("digit inserted before a number (LEFT)", "3 left 5", "53"),
    ("digit inserted before a multi-digit number", "1 2 left 7", "712"),
    ("digit inserted after LEFT into a group", "( 1 2 ) left left 4", "(124)? / 4 before 12"),
    ("decimal point before number", "5 left .", ".5?"),
    # ── deleting operators / brackets between numbers ─────────────────────
    ("delete × between numbers", "2 * 3 left del", "23"),
    ("delete + between numbers", "2 + 3 left del", "23"),
    ("delete paren around number after a number", "2 ( 3 ) del", "23 (paren unwrapped)"),
    ("delete paren around group after a number", "2 ( 3 + 4 ) del", "23+4"),
    ("delete ( then the group dissolves", "( 2 + 3 ) * 4 left left del", "2+3*4 ?"),
    ("delete inside group then retype", "( 2 + 3 ) left del 5 ) * 2", "(2+5)*2"),
    ("delete group end digit, reinsert )", "( 2 + 3 ) left del right * 2", "(2+)*2 -> error"),
    ("unwrap fraction between numbers", "2 / 3 right del", "2/3 then DEL"),
    ("delete sqrt after number", "2 sqrt 3 right del", "2√3 -> DEL"),
    ("delete exponent template between numbers", "2 ^ 3 right del", "2^3 -> DEL"),
    ("power then digit", "2 ^ 3 right 4", "2^3·4"),
    ("number after paren then delete paren", "( 3 ) 4 left del", "34?"),
    # ── implicit multiplication with digits across slots ──────────────────
    ("implicit product num then var", "2 alpha 1", "2A = 6"),
    ("implicit product var then num", "alpha 1 2", "A2 = 6"),
    ("implicit product var then num then delete var", "alpha 1 2 left left del", "2?"),
    ("pi between numbers deleted", "2 pi 3 left del", "23"),
    # ── after a result: `)` / digits continue editing ─────────────────────
    ("result then ) then digit", "2 + 3 enter ) 7 enter", "?"),
    ("result then digit", "2 + 3 enter 7 enter", "?"),
    ("result then * 2", "2 + 3 enter * 2 enter", "Ans*2 or (2+3)*2?"),
    # ── history and Ans ───────────────────────────────────────────────────
    ("history recall of (1+2)*3 then edit", "( 1 + 2 ) * 3 enter up left left del 4 enter",
     "recalled (1+2)*3, edited"),
    ("history recall of naive 2^(1+1)*3", "2 ^ ( 1 + 1 ) * 3 enter ac ac 1 enter up up enter",
     "recalled tree"),
    ("Ans in group", "5 enter ac ac ( ans + 1 ) * 2", "12"),
    ("Ans then digit", "5 enter ac ac ans 2", "Ans·2 = 10"),
    ("digit then Ans", "5 enter ac ac 2 ans", "2·Ans = 10"),
    ("Ans^2 group", "5 enter ac ac ( ans ) ^ 2", "25"),
    ("shift ) closeBracket", "( 1 + 2 shift ) * 3", "?"),
]


def _keys(s):
    return [f"key {k}" for k in s.split()]


def run(emulator, out_dir: Path, angle="rad"):
    shots = out_dir / "shots"
    shots.mkdir(parents=True, exist_ok=True)
    lines = script_header("Calculation") + preamble() + [f"set_angle_mode {angle}"]
    for i, (name, keys, _h) in enumerate(PATTERNS):
        ks = keys.split()
        # `enter` inside the pattern evaluates intermediate results; the last
        # evaluation is the one dumped.
        lines += ["key ac", "key ac"] + _keys(keys)
        lines += [f"dump_calc p{i}|pre tree", f"screenshot {(shots / f'p{i:02d}_input.ppm').as_posix()}"]
        if ks[-1] != "enter":
            lines += ["key enter"]
        lines += [f"dump_calc p{i}|res tree", f"screenshot {(shots / f'p{i:02d}_result.ppm').as_posix()}"]
    r = run_script(lines, emulator, timeout=300)
    ctx = Ctx(angle=angle, env=dict(ENV, Ans=__import__("sympy").Integer(5)))
    rows = []
    for i, (name, keys, human) in enumerate(PATTERNS):
        d = r.by_tag.get(f"p{i}|res")
        row = {"i": i, "pattern": name, "keys": keys, "human": human}
        if not d:
            row["verdict"] = "no dump"
            rows.append(row)
            continue
        inp = parse(d["input"]) if d.get("input") else None
        o = evaluate(inp, ctx) if inp is not None else None
        row.update({"input": slinear(inp) if inp else None, "visual": visual(inp) if inp else None,
                    "giac_in": d.get("giac_in"), "status": d.get("status"), "exact": d.get("exact"),
                    "approx": d.get("approx"),
                    "display": slinear(parse(d["display"])) if d.get("display") else d.get("display_label"),
                    "oracle_of_displayed": o.text() if o else None})
        verdict = []
        if o is not None:
            v, sev, info = compare_value(o, d, ctx)
            if v != "ok":
                verdict.append(f"{v}/{sev}: {info.get('reason')}")
            ser = check_serialization(d, o, ctx)
            if ser and ser.get("mismatch"):
                verdict.append(f"I2(b) giac text != displayed AST: {ser}")
            elif ser and ser.get("unparsed"):
                verdict.append(f"giac text unparsed: {ser['unparsed']}")
        # adjacent Number nodes render as one numeral
        if inp is not None and _adjacent_numbers(inp):
            verdict.append("ADJACENT NUMBER NODES (render as one numeral)")
        row["verdict"] = "; ".join(verdict) or "ok (display == computed)"
        row["tree"] = d.get("tree")
        rows.append(row)
    _convert(shots)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "close_paren_probe.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
    md = ["| # | pattern | keys | displayed input AST | visual | Giac-bound | NumOS exact | verdict |",
          "|---|---|---|---|---|---|---|---|"]
    for row in rows:
        md.append(f"| {row['i']} | {row['pattern']} | `{row['keys']}` | {row.get('input')} | `{row.get('visual')}` | "
                  f"`{row.get('giac_in')}` | {row.get('status')}: {row.get('exact')} | {row['verdict']} |")
    (out_dir / "close_paren_probe.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    (out_dir / "stdout.txt").write_text(r.stdout, encoding="utf-8")
    return rows, r


def _adjacent_numbers(node) -> bool:
    if not isinstance(node, tuple):
        return False
    if node and node[0] == "row":
        kids = node[1]
        for a, b in zip(kids, kids[1:]):
            if isinstance(a, tuple) and isinstance(b, tuple) and a and b and a[0] == "num" and b[0] == "num":
                return True
        return any(_adjacent_numbers(k) for k in kids)
    return any(_adjacent_numbers(k) for k in node[1:] if isinstance(k, tuple))


def _convert(shots: Path):
    try:
        from PIL import Image
    except ImportError:
        return
    for p in shots.glob("*.ppm"):
        try:
            Image.open(p).save(p.with_suffix(".png"))
            p.unlink()
        except Exception:  # noqa: BLE001
            pass


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--angle", default="rad")
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "calculation" / "close-paren"))
    a = ap.parse_args()
    rows, r = run(a.emulator, Path(a.out), a.angle)
    print(Path(a.out, "close_paren_probe.md").read_text(encoding="utf-8"))
    print("rc", r.returncode)
