"""Grapher surface (W3): every relation kind, four keystroke paths, batched.

Families (by case index, deterministic mix of 20):
  bulk     (8/20)  y=f(x), direct canonical keys                      -> budget (d)
  edited   (5/20)  any kind, edited path (typo/trailing/detour/deep) -> budget (b)
  template (2/20)  any kind with log_b inserted through the Toolbox   -> budget (b)
  naive    (1/20)  any kind, `)`-closing path (WYSIWYC only)          -> budget (b)
  edge     (4/20)  poles, domain boundaries, branch cuts, huge/tiny,
                   DEG trig, empty/incomplete, multi-slot, unary minus -> budget (c)

Batching: each case starts from an empty expression list (DEL x6 in list
focus removes every slot; ENTER on the "add" row creates and edits slot 0),
so several cases share one emulator process. A case whose dump tags are
missing (e.g. a NaN residual stopped the script) is re-run alone by the shared
runner (desync recovery). Serial-vs-batched equivalence: see RUNS.md.

Reads: `dump_graph` (slot AST, Giac text, kind/reason, values at float32
sample points through GraphModel) and implicit/inequality residuals
G(x,y)=lhs-rhs through `assert_graph_eval_near S X Y 0 1e38` PASS lines.
"""
from __future__ import annotations

import os
import struct

import sympy as sp

from common import keys as K
from common.emulator import script_header
from common.expr import (Const, DIRECT_TOOLBOX, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Row, Sqrt,
                         Var, linear, to_struct)
from common.gen import case_rng, can_follow
from common.invariants import Finding
from common.sexpr import linear as slinear, parse

from common.expr import TOOLBOX_TOOLBOX  # noqa: F401
from grapher.gcore import (X, Y, agree, float_eval, merge_nums, oracle_expr, point_reals, real_candidates,
                           split_relation)

GRAPHER_TB = DIRECT_TOOLBOX | {"logb"}  # toolbox-inserted log_b gets an explicit ×

NAME = "grapher"
PATHS = ("direct", "edited", "toolbox", "naive")
FAMILY_MIX = (["bulk"] * 8 + ["edited"] * 5 + ["template"] * 2 + ["naive"] * 1 + ["edge"] * 4)
XS = (-2.5, -1.0, -0.25, 0.0, 0.5, 1.0, 2.0, 3.75)
XS_DEG = (-90.0, 0.0, 30.0, 45.0, 90.0, 180.0, 270.0, 37.5)
PTS = ((0.5, 1.0), (2.0, -1.0), (-1.5, 0.25), (3.25, 2.5), (1.0, 0.0), (-0.75, -2.0))
REL_KEY = {"=": "=", "<": "lt", ">": "gt"}
REL_OP = {"=": "Eq", "<": "Lt", ">": "Gt"}
PERTURB = os.environ.get("E2E_GRAPHER_PERTURB", "")  # oracle self-test: "sign" | "coef"


def f32(v: float) -> sp.Rational:
    """Exact rational of the float32 NumOS receives for sample `v`."""
    return sp.Rational(struct.unpack("f", struct.pack("f", float(v)))[0])


# ── generator ──────────────────────────────────────────────────────────────
class GGen:
    def __init__(self, rng, var="x", other=None):
        self.r, self.v, self.other = rng, var, other

    def num(self):
        r = self.r
        if r.random() < 0.15:
            return Num(r.choice(["0.5", "1.5", "2.5", "0.25", "3.2"]))
        return Num(str(r.choice([1, 2, 3, 4, 5, 6, 7, 9, 10, 12])))

    def var(self):
        if self.other and self.r.random() < 0.4:
            return Var(self.other)
        return Var(self.v)

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
                    if prev is None or self._follow(prev, a):
                        break
                else:
                    break
                if prev is not None and not self._follow(prev, a):
                    break
                items.append(a)
                prev = a
        return Row(tuple(items))

    @staticmethod
    def _follow(prev, a):
        # W3-F02: a decimal/number right after a non-number atom is refused by
        # the Grapher ("x2.5" -> unknown x2); keep juxtaposed numbers out of
        # the bulk families (covered by the dedicated probe).
        from common.gen import first_atom
        if isinstance(first_atom(a), Num) or (isinstance(a, Pow) and isinstance(a.base, Num)):
            return False
        # W3-F03: an atom juxtaposed before a function/sqrt ("2ln(x)", "x sin(x)")
        # is refused; keep it out of the bulk families (dedicated edge sub-family).
        if isinstance(first_atom(a), (Fn, Sqrt)):
            return False
        # W3-F04: a number juxtaposed with the constant e ("3e", "2e+x") is read
        # as scientific notation; dedicated edge sub-family "num_e".
        if isinstance(prev, (Num, Pow)) and isinstance(first_atom(a), Const) and first_atom(a).name == "e":
            return False
        return can_follow(prev, a)

    def atom(self, depth):
        r = self.r
        if depth <= 0:
            return r.choice([self.num(), self.var(), self.var(), Const(r.choice(["pi", "e"]))])
        k = r.choices(["num", "x", "paren", "frac", "pow", "sqrt", "fn", "logb"],
                      weights=[3, 4, 5, 3, 4, 2, 5, 1])[0]
        d = depth - 1
        if k == "num":
            return self.num()
        if k == "x":
            return self.var()
        if k == "paren":
            return Paren(self.row(d, r.choice([2, 2, 3])))
        if k == "frac":
            return Frac(self.row(d, 1), self.row(d, r.choice([1, 2])))
        if k == "pow":
            base = r.choice([self.var(), Paren(self.row(d, 2)), self.num(), Const("e")])
            ex = r.choice([Row((Num(str(r.choice([2, 2, 3, 4]))),)), Row((self.var(),)),
                           Row((Frac(Row((Num("1"),)), Row((Num(str(r.choice([2, 3]))),))),))])
            return Pow(base, ex)
        if k == "sqrt":
            return Sqrt(self.row(d, r.choice([1, 2])))
        if k == "logb":
            return LogB(Row((Num(str(r.choice([2, 3, 10]))),)), self.row(d, r.choice([1, 2])))
        return Fn(r.choice(["sin", "cos", "tan", "ln", "log"]), self.row(d, r.choice([1, 2])))


def _short(row, limit=56):
    from common.canon import giac as canon
    try:
        return len(canon(row, {})) <= limit
    except Exception:  # noqa: BLE001
        return True


def _slot(rng, kind, depth):
    """One relation; regenerates (deterministically) while the Giac-like text
    would exceed the Grapher's 64-char slot text ("expression too long")."""
    for _ in range(12):
        s = _slot1(rng, kind, depth)
        first = s["rhs"].items[0] if s["rhs"].items else None
        if kind == "ineq" and isinstance(first, Frac) and not first.capture:
            continue  # W3-F01: `/` right after `<`/`>` captures the symbol
        if _short(s["rhs"]):
            return s
    return s


def _slot1(rng, kind, depth):
    """One relation: dict(lhs Row|None, rel, rhs Row, kind)."""
    if kind == "y":
        return {"kind": "y", "lhs": Row((Var("y"),)), "rel": "=", "rhs": GGen(rng).row(depth)}
    if kind == "x":
        return {"kind": "x", "lhs": Row((Var("x"),)), "rel": "=", "rhs": GGen(rng, "y").row(depth)}
    if kind == "imp":
        lhs = rng.choice([Row((Pow(Var("y"), Row((Num("2"),))),)),
                          Row((Var("x"), Var("y"))),
                          Row((Pow(Var("x"), Row((Num("2"),))), Op("+"), Pow(Var("y"), Row((Num("2"),)))))])
        return {"kind": "imp", "lhs": lhs, "rel": "=", "rhs": GGen(rng, "x", "y").row(max(depth - 1, 1))}
    rel = rng.choice(["<", ">"])
    return {"kind": "ineq", "lhs": Row((Var("y"),)), "rel": rel, "rhs": GGen(rng).row(depth)}


def _contains(row, cls):
    from common.expr import walk
    return any(isinstance(n, cls) for n in walk(row))


def _R(*items):
    return Row(tuple(items))


def _N(v):
    return Num(str(v))


_x = Var("x")


def _fr(n, d):
    return Frac(_R(*n), _R(*d))


def _pw(base, *exp):
    return Pow(base, _R(*exp))


EDGE = [
    # (label, kind, rhs builder(rng) -> Row, angle)
    ("pole_recip", "y", lambda r: _R(_fr([_N(r.choice([1, 3]))], [_x, Op("-"), _N(r.choice([1, 2]))])), "rad"),
    ("pole_recip0", "y", lambda r: _R(_fr([_N(1)], [_x])), "rad"),
    ("pole_sq", "y", lambda r: _R(_fr([_N(1)], [_pw(_x, _N(2)), Op("-"), _N(1)])), "rad"),
    ("ln_dom", "y", lambda r: _R(Fn("ln", _R(_x, Op("-"), _N(r.choice([1, 2]))))), "rad"),
    ("log_dom", "y", lambda r: _R(Fn("log", _R(_x))), "rad"),
    ("sqrt_dom", "y", lambda r: _R(Sqrt(_R(_x, Op("-"), _N(r.choice([1, 2]))))), "rad"),
    ("sqrt_dom2", "y", lambda r: _R(Sqrt(_R(_N(4), Op("-"), _pw(_x, _N(2))))), "rad"),
    ("logb_dom", "y", lambda r: _R(LogB(_R(_N(r.choice([2, 3]))), _R(_x))), "rad"),
    ("cbrt_neg", "y", lambda r: _R(_pw(_x, _fr([_N(1)], [_N(3)]))), "rad"),
    ("pow_neg_exp", "y", lambda r: _R(_pw(_x, Op("-"), _N(r.choice([1, 2])))), "rad"),
    ("x_pow_x", "y", lambda r: _R(_pw(_x, _x)), "rad"),
    ("zero_pow", "y", lambda r: _R(_pw(_x, _N(0))), "rad"),
    ("huge_exp", "y", lambda r: _R(_pw(_N(10), _N(r.choice([12, 30, 38, 39, 45])), _x)), "rad"),
    ("huge_e", "y", lambda r: _R(_pw(Const("e"), _N(r.choice([20, 40, 60, 90])), _x)), "rad"),
    ("tiny", "y", lambda r: _R(_fr([_x], [_pw(_N(10), _N(r.choice([20, 40, 46])))])), "rad"),
    ("cancel", "y", lambda r: (lambda k: _R(Paren(_R(_x, Op("+"), _pw(_N(10), _N(k)))), Op("-"),
                                            _pw(_N(10), _N(k))))(r.choice([7, 8, 9])), "rad"),
    ("deg_trig", "y", lambda r: _R(Fn(r.choice(["sin", "cos", "tan"]), _R(_x))), "deg"),
    ("deg_trig2", "y", lambda r: _R(_N(2), Fn(r.choice(["sin", "cos"]), _R(_N(3), _x)), Op("+"),
                                    Fn("tan", _R(_x, Op("+"), _N(45)))), "deg"),
    ("rad_tan", "y", lambda r: _R(Fn("tan", _R(_x))), "rad"),
    ("unary_mid", "y", lambda r: _R(_x, Op(r.choice(["*", "-", "+"])), Op("-"), _N(2)), "rad"),
    ("unary_lead", "y", lambda r: _R(Op("-"), _x, Op("+"), _N(1)), "rad"),
    ("const_only", "y", lambda r: _R(_N(r.choice([0, 3, 7]))), "rad"),
    ("juxt_fn", "y", lambda r: _R(r.choice([_N(2), _x, Const("pi"), Paren(_R(_x, Op("+"), _N(1)))]),
                                  r.choice([Fn(r.choice(["sin", "cos", "tan", "ln", "log"]), _R(_x)),
                                            Sqrt(_R(_x))])), "rad"),
    ("num_e", "y", lambda r: r.choice([_R(_N(r.choice([2, 3, 6])), Const("e")),
                                       _R(_N(r.choice([2, 3])), Const("e"), Op(r.choice("+-")), _x),
                                       _R(_x, Op("+"), _N(2), Const("e"))]), "rad"),
    ("deg_const_trig", "y", lambda r: _R(Fn(r.choice(["sin", "cos", "tan"]), _R(_N(r.choice([3, 30, 60])))),
                                         Op(r.choice("+*")), _x), "deg"),
    ("juxt_dec", "y", lambda r: _R(r.choice([_x, Const("pi"), Const("e")]), _N(r.choice(["2.5", "0.5", "1.25"]))), "rad"),
]
INCOMPLETE = [  # (label, keys after the relation)  -> must be refused
    ("empty", []), ("dangling_add", ["x", "+"]), ("empty_fn", ["sin"]), ("empty_frac_den", ["1", "/"]),
    ("empty_sqrt", ["sqrt"]), ("empty_pow", ["x", "^"]), ("empty_logb_arg", ["logbase", "2", "right"]),
]


def generate(seed, index):
    rng = case_rng(seed, index)
    fam = FAMILY_MIX[index % len(FAMILY_MIX)]
    case = {"id": f"g{seed}-{index}", "seed": seed, "index": index, "family": fam, "angle": "rad",
            "sub": "", "slots": [], "path": "direct", "variant": None}
    if fam == "bulk":
        case["slots"] = [_slot(rng, "y", rng.choice([1, 2, 2, 3]))]
        case["angle"] = "deg" if rng.random() < 0.1 else "rad"
    elif fam in ("edited", "naive"):
        kind = rng.choices(["y", "x", "imp", "ineq"], weights=[4, 2, 2, 2])[0]
        case["slots"] = [_slot(rng, kind, rng.choice([1, 2, 2, 3]))]
        case["path"] = fam
        case["sub"] = kind
        if fam == "naive":
            for _ in range(8):  # prefer trees where `)` absorbs a slot exit
                toks, absorbed = K._naive(_raw(case["slots"][0]["rhs"], "direct"))
                if absorbed:
                    break
                case["slots"] = [_slot(rng, kind, 3)]
    elif fam == "template":
        kind = rng.choices(["y", "x", "imp", "ineq"], weights=[4, 2, 2, 2])[0]
        s = _slot(rng, kind, rng.choice([1, 2, 2]))
        var = "y" if kind == "x" else "x"
        lb = LogB(Row((Num(str(rng.choice([2, 3, 10]))),)), GGen(rng, var).row(1, rng.choice([1, 2])))
        items = s["rhs"].items
        pos = rng.randrange(1 if kind == "ineq" else 0, len(items) + 1)  # W3-F01 after `<`
        if pos and not isinstance(items[pos - 1], Op):
            new = items[:pos] + (Op(rng.choice("+-*")), lb) + items[pos:]
        else:
            new = items[:pos] + (lb,) + ((Op("+"),) if pos < len(items) and not isinstance(items[pos], Op) else ()) + items[pos:]
        s["rhs"] = Row(tuple(new))
        case["slots"] = [s]
        case["path"] = "toolbox"
        case["sub"] = kind
    else:  # edge
        pick = rng.random()
        if pick < 0.12:
            lab, ks = rng.choice(INCOMPLETE)
            case["sub"] = "incomplete:" + lab
            case["slots"] = [{"kind": "y", "lhs": Row((Var("y"),)), "rel": "=", "rhs": None, "raw": ks}]
        elif pick < 0.27:
            n = rng.choice([2, 3])
            case["sub"] = f"multi{n}"
            case["slots"] = [_slot(rng, rng.choice(["y", "y", "x", "imp", "ineq"]), rng.choice([1, 2])) for _ in range(n)]
        elif pick < 0.40:
            kind = rng.choice(["x", "imp", "ineq"])
            case["sub"] = "rel_" + kind
            case["slots"] = [_slot(rng, kind, rng.choice([1, 2, 3]))]
        else:
            lab, kind, build, ang = rng.choice(EDGE)
            case["sub"] = lab
            case["angle"] = ang
            case["slots"] = [{"kind": kind, "lhs": Row((Var("y"),)), "rel": "=", "rhs": build(rng)}]
    if fam in ("edited", "template", "naive") and rng.random() < 0.1:
        case["angle"] = "deg"
    case["variant_seed"] = rng.randrange(1 << 30)
    return case


# ── oracle ─────────────────────────────────────────────────────────────────
def _float_explains(v, fv):
    import math
    if v is None:
        return fv != fv or math.isinf(fv)
    if fv != fv or math.isinf(fv):
        return False
    return abs(v - fv) <= 1e-3 * max(1.0, abs(fv))


def _tb(case):
    return GRAPHER_TB if case["path"] == "toolbox" else DIRECT_TOOLBOX


def _oracle_slot(s, case):
    """Intended structures; values are computed point-wise (gcore.point_reals)."""
    if s.get("rhs") is None:
        return {"rhs": None}
    tb = _tb(case)
    return {"lhs": merge_nums(to_struct(s["lhs"], tb)), "rhs": merge_nums(to_struct(s["rhs"], tb))}


def _perturb(cands):
    """Oracle self-test (E2E_GRAPHER_PERTURB): flip the sign / add 1."""
    if not PERTURB:
        return cands
    if PERTURB == "sign":
        return [None if c is None else -c for c in cands]
    return [None if c is None else c + 1 for c in cands]


def pr(row, angle, xv=None, yv=None, perturb=False):
    c = point_reals(row, angle, xv, yv)
    return _perturb(c) if perturb else c


def oracle_of(case):
    return [_oracle_slot(s, case) for s in case["slots"]]


def oracle_lite(o):
    return [{k: (v is not None) for k, v in s.items()} for s in o]


def header():
    return script_header("Grapher") + ["key down"]


def check_header(by_tag):
    return []


# ── keys ───────────────────────────────────────────────────────────────────
class _GTyper(K._Typer):
    """Grapher toolbox: only log_b is discoverable (Logarithm)."""

    def atom(self, a):
        if isinstance(a, Var) and a.name == "y":
            self.k("y")
        elif isinstance(a, LogB) and self.mode == "gtoolbox":
            self.out += K.toolbox_keys("Logarithm")
            self.row(a.base)
            self.exit("logb_base")
            self.row(a.arg)
            self.exit("logb_arg")
        else:
            super().atom(a)


def _raw(row, mode):
    t = _GTyper(mode)
    t.row(row)
    return t.out


def slot_tokens(s, path, rng):
    pre = K._canonical(_raw(s["lhs"], "direct")) if s["lhs"] is not None else []
    pre = pre + [("key", REL_KEY[s["rel"]])]
    if s.get("rhs") is None:
        return pre + [("key", k) for k in s["raw"]], {}
    if path == "toolbox":
        body = K._canonical(_raw(s["rhs"], "gtoolbox"))
    elif path == "naive":
        body, _ = K._naive(_raw(s["rhs"], "direct"))
    else:
        body = K._canonical(_raw(s["rhs"], "direct"))
    info = {}
    if path == "edited":
        variant = rng.choice(["typo_digit", "trailing", "detour", "deep_detour", "op_typo", "start_insert"])
        info["variant"] = variant
        if variant == "deep_detour":
            k = rng.randint(3, 8)
            body = body + [("key", "left")] * k + [("key", "7"), ("key", "del")] + [("key", "right")] * k
        elif variant == "op_typo":
            # type a wrong operator first, delete it, then the real expression
            body = [("key", rng.choice(["+", "*", "x", "("])), ("key", "del")] + body
        elif variant == "start_insert":
            # type the body, walk back to its start, insert and delete a digit
            n = sum(1 for t in body if t[0] == "key" and t[1] not in ("right",))
            k = max(1, min(n, rng.randint(1, 12)))
            body = body + [("key", "left")] * k + [("key", "9"), ("key", "del")] + [("key", "right")] * k
        else:
            body = K.edit(body, variant, rng)
    return pre + body, info


def residual_points(case, o, s):
    """Points where the oracle residual is real and finite (NaN would stop the script)."""
    pts = []
    for x, y in PTS:
        if case["angle"] == "deg":
            x = x * 40
        lv = point_reals(o["lhs"], case["angle"], f32(x), f32(y))
        rv = point_reals(o["rhs"], case["angle"], f32(x), f32(y))
        if lv and rv and all(a is not None for a in lv) and all(b is not None for b in rv):
            pts.append((x, y))
    return pts


_ORACLE_CACHE = {}


def full_oracle(case):
    o = _ORACLE_CACHE.get(case["id"])
    if o is None:
        o = _ORACLE_CACHE[case["id"]] = oracle_of(case)
    return o


def case_lines(case, oracle, do_i4=False):
    import random
    oracle = full_oracle(case)
    rng = random.Random(case["variant_seed"])
    lines = [f"set_angle_mode {case['angle']}"] + ["key del"] * 6
    metas = []
    xs = XS_DEG if case["angle"] == "deg" else XS
    for i, s in enumerate(case["slots"]):
        toks, info = slot_tokens(s, case["path"], rng)
        if i:
            lines.append("key down")
        lines.append("key enter")
        lines += K.to_lines(toks)
        lines += ["key enter", "wait 2"]
        metas.append({"keys": K.to_compact(toks), **info})
    lines.append("wait 2")
    res_plan = []
    for i, s in enumerate(case["slots"]):
        lines.append(f"dump_graph {case['id']}|s{i} {i} {' '.join(map(str, xs))}")
    for i, s in enumerate(case["slots"]):
        if s["kind"] in ("imp", "ineq") and s.get("rhs") is not None and oracle and oracle[i].get("lhs") is not None:
            for x, y in residual_points(case, oracle[i], s):
                lines.append(f"log R|{case['id']}|{i}|{x}|{y}")
                lines.append(f"assert_graph_eval_near {i} {x} {y} 0 1e38")
                res_plan.append((i, x, y))
    lines.append(f"dump_graph {case['id']}|end 0")
    return lines, {"slots": metas, "res_plan": res_plan, "xs": list(xs)}


def case_lines_for(case, oracle, paths, do_i4, tree_dump=False):
    return case_lines(case, oracle, do_i4)


# ── analysis ───────────────────────────────────────────────────────────────
KIND_EXPECT = {"y": "explicitY", "x": "explicitX", "imp": "implicit", "ineq": "ineqStrict"}


def _residuals(by_tag, case_id):
    return by_tag.get("__residuals__", {}).get(case_id, {})


def analyze(case, by_tag, meta, oracle):
    oracle = full_oracle(case)  # the runner hands over a JSON-lite oracle
    rec ={"id": case["id"], "family": case["family"], "sub": case["sub"], "angle": case["angle"],
           "path": case["path"], "slots": []}
    findings, desync = [], []
    residuals = _residuals(by_tag, case["id"])
    nan_stop = any(v is None for v in residuals.values())
    if f"{case['id']}|end" not in by_tag and not nan_stop:
        return rec, [], ["missing end dump (script stopped or desync)"]
    for i, s in enumerate(case["slots"]):
        d = by_tag.get(f"{case['id']}|s{i}")
        sm = meta["slots"][i]
        keys = sm["keys"]
        if not d or not d.get("ok"):
            desync.append(f"missing dump slot {i}")
            continue
        if d.get("count") != len(case["slots"]):
            desync.append(f"slot count {d.get('count')} != {len(case['slots'])}")
            continue
        srec = {"keys": keys, "input": None, "giac": d.get("giac_rpn"), "giac_l": d.get("giac_rpn_l"),
                "valid": d.get("valid"), "kind": d.get("kind"), "reason": d.get("reason"),
                "values": d.get("values"), "raw_input": d.get("input"), "text": d.get("text"),
                "residuals": sorted([list(k) + [v] for k, v in residuals.items() if k[0] == i])}
        rec["slots"].append(srec)
        try:
            inp = parse(d["input"])
        except Exception as exc:  # noqa: BLE001
            desync.append(f"input unparsable {exc}")
            continue
        srec["input"] = slinear(inp)
        lhs, op, rhs = split_relation(inp)
        ev = {"keys": keys, "input": slinear(inp), "text": d.get("text"), "giac": d.get("giac_rpn"),
              "giac_l": d.get("giac_rpn_l"), "kind": d.get("kind"), "reason": d.get("reason"),
              "angle": case["angle"], "family": case["family"], "sub": case["sub"], "slot": i}
        if s.get("rhs") is None:  # incomplete input must be refused
            if d.get("valid"):
                findings.append(Finding(case["id"], f"s{i}", "I5", "S2",
                                        "incomplete Grapher slot accepted as a valid curve", ev))
            continue
        # I2: structure (not for the naive path, which is WYSIWYC-only)
        intended_l, intended_r = oracle[i]["lhs"], oracle[i]["rhs"]
        same = (lhs is not None and merge_nums(lhs) == intended_l and merge_nums(rhs) == intended_r
                and op == REL_OP[s["rel"]])
        if not same and case["path"] != "naive":
            if lhs is None:
                desync.append("no relation in slot")  # keys lost: verify single
            findings.append(Finding(case["id"], f"s{i}", "I2", "S2",
                                    "Grapher slot structure differs from the typed relation",
                                    dict(ev, intended=f"{linear(s['lhs'])} {s['rel']} {linear(s['rhs'])}")))
        if lhs is None:
            continue
        # oracle of the intended tree when NumOS built it, else of the DISPLAYED tree (WYSIWYC)
        L, R = (intended_l, intended_r) if same else (merge_nums(lhs), merge_nums(rhs))
        ang = case["angle"]
        kind = s["kind"]
        xs = meta["xs"]
        if not d.get("valid"):
            # I5: a refusal must be agreed by the oracle (no real value at any sample)
            reason = d.get("reason") or ""
            if kind in ("y", "x"):
                vals = [pr(R, ang, *((f32(t), None) if kind == "y" else (None, f32(t)))) for t in xs]
            else:
                vals = [pr(R, ang, f32(x), f32(y)) for x, y in PTS]
            if any(c is not None for v in vals for c in v):
                sev = "S4" if "too long" in reason else "S3"
                findings.append(Finding(case["id"], f"s{i}", "I5", sev,
                                        f"Grapher refuses a well-formed relation ({reason})", ev))
            continue
        srec["kind_expected"] = KIND_EXPECT[kind]  # recorded only (x=y is classified explicitY)
        bad, pole, flt = [], [], []
        if kind in ("y", "x"):
            for t, v in d.get("values") or []:
                c = pr(R, ang, *((f32(t), None) if kind == "y" else (None, f32(t))), perturb=True)
                if not c:
                    continue  # oracle unsupported here
                big = any(cc is not None and abs(cc) > 3e38 for cc in c)
                if big and (v is None):
                    continue  # beyond float32: invalid is the representable outcome
                if not agree(v, c):
                    if v is not None and all(cc is None for cc in c):
                        # exact sample hits a pole/zero of the exact value; float32/double
                        # evaluation lands next to it. Neighbour oracle decides policy.
                        tn = f32(t) + sp.Rational(1, 1024)
                        cn = pr(R, ang, *((tn, None) if kind == "y" else (None, tn)))
                        if cn and any(cc is not None for cc in cn):
                            pole.append({"t": t, "numos": v, "oracle": c, "neighbour": cn[:1]})
                            continue
                    fv = float_eval(R, ang, *((float(f32(t)), None) if kind == "y" else (None, float(f32(t)))))
                    if _float_explains(v, fv):
                        flt.append({"t": t, "numos": v, "oracle": c, "ieee_double": fv})
                        continue
                    bad.append({"t": t, "numos": v, "oracle": c})
        else:
            for (si, x, y), val in sorted(residuals.items()):
                if si != i:
                    continue
                a = pr(L, ang, f32(x), f32(y))
                b = pr(R, ang, f32(x), f32(y), perturb=True)
                c = [None if (p is None or q is None) else p - q for p in a for q in b]
                if c and not agree(val, c, rel=1e-5, absol=5e-6):
                    bad.append({"x": x, "y": y, "numos": val, "oracle": c})
            planned = [p for p in meta["res_plan"] if p[0] == i]
            got = [k for k in residuals if k[0] == i]
            if len(got) < len(planned) and not nan_stop:
                missing = [p for p in planned if p not in residuals]
                bad.append({"residual_missing": missing[:1]})
        if flt and not bad:
            findings.append(Finding(case["id"], f"s{i}", "FLOAT", "S4",
                                    "value differs from the exact oracle but equals IEEE double evaluation "
                                    "(exact pole/zero hit, huge trig argument, NaN^0=1; needs policy decision)",
                                    dict(ev, samples=flt[:4], oracle_tree=slinear(R))))
        if pole and not bad:
            findings.append(Finding(case["id"], f"s{i}", "POLE", "S4",
                                    "finite value plotted at an exact pole/singular sample (needs policy decision)",
                                    dict(ev, samples=pole[:4], oracle_tree=slinear(R))))
        if bad:
            title = "Grapher value differs from the oracle" + ("" if same else " of the displayed tree")
            findings.append(Finding(case["id"], f"s{i}", "I1", "S1", title,
                                    dict(ev, samples=bad[:4], oracle_tree=slinear(R))))
    return rec, findings, desync


def third_voice(case):
    from common.canon import giac as canon
    out = []
    for i, s in enumerate(case["slots"]):
        if s.get("rhs") is None:
            continue
        try:
            txt = canon(s["rhs"], {})
        except Exception:  # noqa: BLE001
            continue
        v = "y" if s["kind"] == "x" else "x"
        t = "30" if case["angle"] == "deg" else "2"
        out.append(f"giac_eval {case['id']}|tv{i} subst({txt},{v}={t})")
    return out


def triage(case, by_tag, record):
    return {f"tv{i}": (by_tag.get(f"{case['id']}|tv{i}") or {}).get("approx")
            for i in range(len(case["slots"]))}
