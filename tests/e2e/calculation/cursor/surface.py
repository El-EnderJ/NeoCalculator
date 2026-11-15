"""W1 edited-path / cursor-insertion surface, surface name `calculation.cursor`.

The common generator only types expressions left to right. This surface types
a small base expression and then EDITS it: cursor moves (LEFT/RIGHT, some
UP/DOWN, `)` which is moveRight in Calculation and wraps at the end of the
root row) followed by an insertion at whatever boundary the cursor reached
(digit, '.', ×10^, (-), variable, Ans, π/e, '(', √, fraction, power, ×/+,
Toolbox |x| / nthroot / log_b at a mid-expression cursor) or a DEL. Modes:
  live    - edit right after typing the base
  history - base ENTER, UP (recall), then edit
  result  - base ENTER, then edit directly (the result is cleared, the
            expression stays editable)

There is no "intended" tree for an edit, so the judgement is about
what-you-see-is-what-is-computed on the FINAL input:
  I2a  the eye's reading of the input (adjacent number nodes concatenate,
       a number before a power with a numeric base joins that base) must
       denote the same value as the computed tree           (S1 if not)
  I2b  the Giac-bound string must denote the displayed AST  (common check)
  I1   NumOS result == oracle(displayed AST)                (common check)
  I5   a refusal where the eye's reading has a value         (S3)

    py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.cursor --seed 501 --count 2000 --jobs 3
"""
from __future__ import annotations

import random

from common import keys as K
from common.emulator import script_header
from common.evaluator import Ctx, evaluate, close
from common.expr import linear
from common.gen import Gen, case_rng
from common.invariants import Finding, check_serialization, compare_value, visual
from common.sexpr import linear as slinear, parse

from .. import driver

NAME = "calculation.cursor"
PATHS = ("edit",)
MODES = [("live", 60), ("history", 25), ("result", 15)]
TB = {"abs": "abs", "nthroot": "nthroot", "logb": "Logarithm"}


def _base(rng, angle):
    fam = rng.choice(["general", "general", "decimal", "rational", "bracket", "irrational"])
    g = Gen(rng, fam, angle)
    for _ in range(20):
        t = g.row(rng.choice([1, 1, 2]), terms=rng.choice([2, 2, 3]))
        from common.expr import size
        if size(t) <= 10:
            return t, fam
    return t, fam


def _action(rng):
    x = rng.random()
    acts = [
        (0.25, lambda: [("key", str(rng.randint(0, 9)))]),
        (0.31, lambda: [("key", ".")]),
        (0.36, lambda: [("key", "exp"), ("key", str(rng.randint(1, 9)))]),
        (0.42, lambda: [("key", "neg")]),
        (0.50, lambda: [("key", "alpha"), ("key", rng.choice("12345"))]),
        (0.56, lambda: [("key", "ans")]),
        (0.62, lambda: [("key", rng.choice(["pi", "e"]))]),
        (0.70, lambda: [("key", ")")]),
        (0.74, lambda: [("key", "(")]),
        (0.89, lambda: [("key", "del")] * rng.choice([1, 1, 2])),
        (0.92, lambda: [("key", rng.choice(["sqrt", "/"]))] + [("key", str(rng.randint(2, 9)))]),
        (0.94, lambda: [("key", "^"), ("key", str(rng.randint(2, 3)))]),
        (0.97, lambda: [("key", rng.choice(["*", "+", "-"]))]),
        (1.01, lambda: K.toolbox_keys(TB[rng.choice(list(TB))]) + [("key", str(rng.randint(2, 9)))]),
    ]
    for p, f in acts:
        if x < p:
            return f()
    return acts[0][1]()


def _moves(rng):
    out = []
    for _ in range(rng.choice([0, 1, 1, 2, 2, 3, 4, 6])):
        y = rng.random()
        out.append(("key", "left" if y < 0.6 else "right" if y < 0.85 else ")" if y < 0.93 else
                    rng.choice(["up", "down"])))
    return out


def generate(seed, index):
    rng = case_rng(seed, index)
    angle = rng.choice(["rad", "deg"])
    mode = rng.choices([m for m, _ in MODES], weights=[w for _, w in MODES])[0]
    tree, fam = _base(rng, angle)
    base = K.project(tree, "direct")[0]
    toks = list(base)
    if mode == "history":
        toks += [("key", "enter"), ("key", "up")]
    elif mode == "result":
        toks += [("key", "enter")]
    kinds = []
    for _ in range(rng.choice([1, 1, 2, 3])):
        mv = _moves(rng)
        act = _action(rng)
        kinds.append(act[0][1] if act[0][0] == "key" else "toolbox")
        toks += mv + act
    toks += [("key", "enter")]
    return {"id": f"c{seed}-{index}", "seed": seed, "index": index, "family": f"{mode}:{fam}",
            "angle": angle, "depth": 0, "tree": tree, "ans": "5", "toks": toks, "edit_kinds": kinds}


def oracle_of(case):
    from common.evaluator import Value
    return Value("unsupported", detail="cursor surface judges the final input AST")


def oracle_lite(value):
    return {"kind": "unsupported", "text": "n/a", "formats": []}


def header():
    return script_header("Calculation") + driver.preamble()


def check_header(by_tag):
    return [f"variable {v} not stored as expected" for v in driver.check_preamble(by_tag)]


def case_lines(case, oracle, do_i4):
    return case_lines_for(case, oracle, PATHS, do_i4)


def case_lines_for(case, oracle, paths, do_i4, tree_dump=False):
    lines = [f"# case {case['id']} {case['family']} {case['angle']} :: base {linear(case['tree'])}",
             "key ac", "key ac", f"set_angle_mode {case['angle']}",
             "key 5", "key enter", "key ac", "key ac"]
    lines += K.to_lines(case["toks"])
    lines += [f"dump_calc {case['id']}|edit" + (" tree" if tree_dump else "")]
    return lines, {"edit": {"keys": K.to_compact(case["toks"])}}


def merge_visual(node):
    """The eye's reading: adjacent number nodes concatenate; a number directly
    before a power whose base is a single number joins that base."""
    if not isinstance(node, tuple) or not node:
        return node
    if node[0] == "row":
        out = []
        for k in node[1]:
            k = merge_visual(k)
            if out and isinstance(out[-1], tuple) and out[-1][:1] == ("num",):
                if isinstance(k, tuple) and k[:1] == ("num",):
                    out[-1] = ("num", out[-1][1] + k[1])
                    continue
                if isinstance(k, tuple) and k[:1] == ("pow",):
                    b = k[1]
                    if isinstance(b, tuple) and b[0] == "row" and len(b[1]) == 1 and b[1][0][:1] == ("num",):
                        prev = out.pop()
                        out.append(("pow", ("row", (("num", prev[1] + b[1][0][1]),)), k[2]))
                        continue
            out.append(k)
        return ("row", tuple(out))
    return (node[0],) + tuple(merge_visual(a) if isinstance(a, tuple) else a for a in node[1:])


def _same(a, b):
    if a.kind != b.kind:
        return False
    if a.kind != "finite":
        return True
    try:
        return any(close(x, y) for x in a.finite() for y in b.finite())
    except Exception:  # noqa: BLE001
        return False


def analyze(case, by_tag, meta, oracle=None):
    ctx = driver.ctx_for(case)
    rec = {"id": case["id"], "family": case["family"], "angle": case["angle"], "tree": linear(case["tree"]),
           "edit_kinds": case.get("edit_kinds"), "oracle": "", "paths": {}}
    d = by_tag.get(f"{case['id']}|edit")
    why = driver._desync(d)
    if why:
        return rec, [], [f"edit: {why}"]
    keys = meta["edit"]["keys"]
    inp = parse(d["input"])
    if not case["family"].startswith("live"):
        # After `base ENTER` Ans holds the base result (pre-step Ans = 5 was
        # used while typing the base). Unknown base value -> skip Ans inputs.
        from common.expr import to_struct
        import sympy as sp
        bv = evaluate(to_struct(case["tree"]), Ctx(angle=case["angle"], env=dict(ctx.env)))
        if bv.kind == "finite" and len(bv.finite()) == 1:
            ctx = Ctx(angle=case["angle"], env=dict(ctx.env, Ans=bv.principal))
        elif "Ans" in d["input"]:
            return rec, [], []
    od = evaluate(inp, ctx)
    rec["oracle"] = od.text()
    vis = merge_visual(inp)
    findings = []
    p = {"keys": keys, "input": slinear(inp), "visual": visual(inp), "giac_in": d.get("giac_in"),
         "status": d.get("status"), "exact": d.get("exact"),
         "display": slinear(parse(d["display"])) if d.get("display") else d.get("display_label"),
         "oracle_of_ast": od.text()}
    if od.kind != "unsupported":
        v, sev, info = compare_value(od, d, ctx)
        p["I1"] = v
        if v in ("I1", "I5"):
            findings.append(Finding(case["id"], "edit", v, sev, info.get("reason", ""),
                                    dict(info, keys=keys, input=slinear(inp), giac_in=d.get("giac_in"),
                                         tree=rec["tree"])))
        ser = check_serialization(d, od, ctx)
        if ser and ser.get("mismatch"):
            findings.append(Finding(case["id"], "edit", "I2", "S1" if p.get("I1") == "I1" else "S2",
                                    "Giac-bound text does not denote the displayed input",
                                    dict(ser, keys=keys, input=slinear(inp), giac_in=d.get("giac_in"))))
    if vis != inp:
        ov = evaluate(vis, ctx)
        p["eye_reading"] = slinear(vis)
        p["eye_value"] = ov.text()
        if ov.kind == "finite" and d.get("status") != "ok":
            findings.append(Finding(case["id"], "edit", "I5", "S3",
                                    "input reads as a valid number/expression but is refused (adjacent number nodes)",
                                    {"keys": keys, "input": slinear(inp), "eye_reading": slinear(vis),
                                     "eye_value": ov.text(), "status": d.get("status")}))
        elif not _same(ov, od) and d.get("status") == "ok":
            findings.append(Finding(case["id"], "edit", "I2", "S1",
                                    "input renders as a different expression than the one computed "
                                    "(adjacent number nodes)",
                                    {"keys": keys, "input": slinear(inp), "eye_reading": slinear(vis),
                                     "eye_value": ov.text(), "computed_ast_value": od.text(),
                                     "exact": d.get("exact"), "giac_in": d.get("giac_in")}))
    rec["paths"]["edit"] = p
    return rec, findings, []


def third_voice(case):
    return []


def triage(case, by_tag, record):
    return {}
