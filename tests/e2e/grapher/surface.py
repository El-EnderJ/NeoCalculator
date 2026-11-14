"""Minimal Grapher driver (enough to validate the oracle against ADV-H01/M1).

The full Grapher surface (relations, implicit curves, trace, tables) belongs
to a later worker. This driver types `y = f(x)` into slot 0 with real keys,
commits it, and reads the slot's AST, its Giac text and sampled values through
`dump_graph`. In Grapher `)` is closeParen() (exits the nearest enclosing round
group even from a nested fraction/power), so the natural `)`-closing
keystroke path IS expected to denote the intended tree.

One case per emulator process (slot clearing is not modelled yet).
"""
from __future__ import annotations

import random

import sympy as sp

from common import keys as K
from common.emulator import script_header
from common.evaluator import Ctx, X, _nonfinite, evaluate
from common.expr import (Const, DIRECT_TOOLBOX, Fn, Frac, Num, Op, Paren, Pow, Row, Sqrt, Var,
                         linear, to_struct)
from common.gen import case_rng, can_follow
from common.invariants import Finding
from common.sexpr import linear as slinear, parse

NAME = "grapher"
XS = (-1.5, 0.5, 2.0, 3.25)


class GGen:
    """y = f(x) over the Grapher's frozen token vocabulary."""

    def __init__(self, rng):
        self.r = rng

    def num(self):
        return Num(str(self.r.choice([1, 2, 3, 4, 5, 7, 10, 12])))

    def row(self, depth, n=None):
        r = self.r
        n = n or r.choice([1, 2, 2, 3])
        items = []
        for i in range(n):
            if i:
                items.append(Op(r.choice("+-*")))
            prev = None
            for _ in range(1 if r.random() < 0.7 else 2):
                for _try in range(6):
                    a = self.atom(depth)
                    if prev is None or can_follow(prev, a):
                        break
                else:
                    break
                if prev is not None and not can_follow(prev, a):
                    break
                items.append(a)
                prev = a
        return Row(tuple(items))

    def atom(self, depth):
        r = self.r
        if depth <= 0:
            return r.choice([self.num(), Var("x"), Var("x"), Const("pi")])
        k = r.choices(["num", "x", "paren", "frac", "pow", "sqrt", "fn"],
                      weights=[3, 4, 6, 3, 4, 2, 4])[0]
        d = depth - 1
        if k == "num":
            return self.num()
        if k == "x":
            return Var("x")
        if k == "paren":
            return Paren(self.row(d, r.choice([2, 2, 3])))
        if k == "frac":
            return Frac(self.row(d, 1), self.row(d, r.choice([1, 2])))
        if k == "pow":
            base = r.choice([Var("x"), Paren(self.row(d, 2)), self.num()])
            return Pow(base, Row((Num(str(r.choice([2, 2, 3]))),)))
        if k == "sqrt":
            return Sqrt(self.row(d, r.choice([1, 2])))
        return Fn(r.choice(["sin", "cos", "ln"]), self.row(d, r.choice([1, 2])))


def generate(seed, index):
    rng = case_rng(seed, index)
    g = GGen(rng)
    tree = g.row(rng.choice([1, 2, 2, 3]))
    return {"id": f"g{seed}-{index}", "seed": seed, "index": index, "family": "explicit",
            "angle": "rad", "tree": tree, "ans": None}


def oracle_of(case):
    return evaluate(to_struct(case["tree"]), Ctx(angle="rad", env={}))


def oracle_lite(value):
    return {"kind": value.kind, "text": value.text()}


def header():
    return script_header("Grapher")


def check_header(by_tag):
    return []


def keystrokes(tree):
    # Natural typing: `)` closes brackets (absorbing fraction/power exits).
    toks, _ = K._naive(K._raw(tree, "direct"))
    return toks


def case_lines(case, oracle, do_i4):
    toks = keystrokes(case["tree"])
    xs = " ".join(str(x) for x in XS)
    lines = ["set_angle_mode rad", "key down", "key enter", "key y", "key ="]
    lines += K.to_lines(toks)
    lines += ["key enter", "wait 3", f"dump_graph {case['id']}|graph 0 {xs}"]
    return lines, {"graph": {"keys": "y = " + K.to_compact(toks)}}


PATHS = ("graph",)


def case_lines_for(case, oracle, paths, do_i4, tree_dump=False):
    return case_lines(case, oracle, do_i4)


def _y(value, x):
    v = value.subs(X, sp.Rational(str(x)))
    if _nonfinite(v):
        return None
    n = complex(sp.N(v, 30))
    return None if abs(n.imag) > 1e-12 else n.real


def analyze(case, by_tag, meta, oracle):
    if oracle is None or isinstance(oracle, dict):
        oracle = oracle_of(case)
    d = by_tag.get(f"{case['id']}|graph")
    rec = {"id": case["id"], "tree": linear(case["tree"]), "oracle": oracle.text(), "paths": {}}
    if not d or not d.get("ok"):
        return rec, [], ["missing/invalid graph dump"]
    findings = []
    keys = meta["graph"]["keys"]
    try:
        inp = parse(d["input"])
    except Exception as exc:  # noqa: BLE001
        return rec, [], [f"input unparsable {exc}"]
    # slot AST = [y, =, <rhs...>]; compare the RHS with the intended tree
    items = inp[1] if inp and inp[0] == "row" else ()
    rhs = ("row", tuple(items[2:])) if len(items) >= 2 and items[0] == ("var", "y") else inp
    intended = to_struct(case["tree"], DIRECT_TOOLBOX)
    rec["paths"]["graph"] = {"keys": keys, "input": slinear(inp), "giac": d.get("giac_rpn"),
                             "values": d.get("values"), "valid": d.get("valid")}
    if rhs != intended:
        findings.append(Finding(case["id"], "graph", "I2", "S1",
                                "Grapher slot structure differs from the typed expression",
                                {"keys": keys, "intended": slinear(intended), "input": slinear(rhs)}))
    shown_oracle = evaluate(rhs, Ctx(angle="rad", env={}))
    if oracle.kind == "finite" and not d.get("valid"):
        reason = d.get("reason") or ""
        findings.append(Finding(case["id"], "graph", "I5", "S4" if "too long" in reason else "S3",
                                f"Grapher refuses a well-formed y=f(x) ({reason})",
                                {"keys": keys, "input": slinear(inp), "text": d.get("text")}))
    if oracle.kind == "finite" and d.get("valid"):
        bad = []
        for x, y in d.get("values") or []:
            want = _y(oracle.principal, x)
            if want is None:
                continue
            if y is None or abs(y - want) > 1e-4 * max(1.0, abs(want)):
                bad.append({"x": x, "numos": y, "oracle": want})
        if bad:
            findings.append(Finding(case["id"], "graph", "I1", "S1",
                                    "Grapher value differs from the oracle of the typed expression",
                                    {"keys": keys, "samples": bad, "input": slinear(inp),
                                     "giac": d.get("giac_rpn"), "displayed_oracle": shown_oracle.text()}))
    return rec, findings, []


def third_voice(case):
    return []


def triage(case, by_tag, record):
    return {}
