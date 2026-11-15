"""Calculus surface model: the common VPAM expression model plus the two
Calculus-only Toolbox templates, their expected dump structure, keystroke
projections and the canonical Giac text (third voice).

Additions over common/expr.py (which is shared and must not be edited):
  * DefInt(lower, upper, body, var)   Toolbox "Definite integral" (NodeDefIntegral,
    dumped as `(node13 lower upper body var)`, serialized
    `integrate((body),(var),(lower),(upper))`).
  * DiffCall(body, var)               Toolbox "Derivative" (NodeCall "diff",
    dumped as `(call "diff" body var)`).
  * Var("y") is typed with the dedicated y key (common keys.py only knows A-F/x/Ans).

Observed on the emulator (out/correctness-e2e-01/scratch/w4/probe2.numos):
  * Toolbox search "Definite integral" inserts the template with the cursor in
    the lower bound; RIGHT moves lower -> upper -> integrand -> variable -> out.
  * Toolbox search "Derivative" inserts diff(□,□); RIGHT moves expr -> var -> out.
  * In Calculus `)` is CursorController::moveRight (CalculusApp.cpp:650), the
    same as Calculation; the Grapher ADV-H01 fix (closeParen) is not applied.
"""
from __future__ import annotations

import random
from dataclasses import dataclass

from common import keys as K
from common.expr import (Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row,
                         Sqrt, Var, is_atom)
from common.expr import DIRECT_TOOLBOX, TOOLBOX_TOOLBOX  # noqa: F401  (re-export)

DEFINT_QUERY = "Definite integral"
DIFF_QUERY = "Derivative"


@dataclass(frozen=True)
class DefInt:
    lower: Row
    upper: Row
    body: Row
    var: str = "x"


@dataclass(frozen=True)
class DiffCall:
    body: Row
    var: str = "x"


ATOMS = (Num, Const, Var, Paren, Frac, Pow, Sqrt, Root, Abs, Fn, LogB, Empty, DefInt, DiffCall)


def is_atom_c(e) -> bool:
    return isinstance(e, ATOMS)


def first_atom(a):
    if isinstance(a, Pow) and not a.sci:
        return first_atom(a.base)
    if isinstance(a, Frac) and a.capture:
        return first_atom(a.num.items[0])
    return a


def toolbox_inserted(a, tb) -> bool:
    f = first_atom(a)
    if isinstance(f, (DefInt, DiffCall)):
        return True
    if isinstance(f, Root):
        return "root" in tb
    if isinstance(f, Abs):
        return "abs" in tb
    if isinstance(f, LogB):
        return "logb" in tb
    if isinstance(f, Fn):
        return ("fn:" + f.name) in tb
    return False


_OPNAME = {"+": "Add", "-": "Sub", "*": "Mul", "/": "Div"}


def to_struct(x, tb=DIRECT_TOOLBOX):
    """Expected parsed-dump structure (see common/expr.to_struct)."""
    if isinstance(x, Row):
        out, prev_atom = [], False
        for e in x.items:
            if isinstance(e, Pow) and e.sci:
                out.append(("op", "Mul"))
            elif prev_atom and is_atom_c(e) and toolbox_inserted(e, tb):
                out.append(("op", "Mul"))
            out.append(to_struct(e, tb))
            prev_atom = is_atom_c(e)
        return ("row", tuple(out))
    if isinstance(x, Op):
        return ("op", _OPNAME[x.kind])
    if isinstance(x, Num):
        return ("num", x.text)
    if isinstance(x, Const):
        return ("const", x.name)
    if isinstance(x, Var):
        return ("var", x.name)
    if isinstance(x, Paren):
        return ("paren", "paren", to_struct(x.body, tb))
    if isinstance(x, Frac):
        return ("frac", to_struct(x.num, tb), to_struct(x.den, tb))
    if isinstance(x, Pow):
        return ("pow", to_struct(Row((x.base,)), tb), to_struct(x.exp, tb))
    if isinstance(x, Sqrt):
        return ("sqrt", to_struct(x.body, tb))
    if isinstance(x, Root):
        return ("root", to_struct(x.body, tb), to_struct(x.index, tb))
    if isinstance(x, Abs):
        return ("paren", "bar", to_struct(x.body, tb))
    if isinstance(x, Fn):
        return ("fn", x.name, to_struct(x.arg, tb))
    if isinstance(x, LogB):
        return ("logb", to_struct(x.base, tb), to_struct(x.arg, tb))
    if isinstance(x, Empty):
        return ("empty",)
    if isinstance(x, DefInt):
        return ("node13", to_struct(x.lower, tb), to_struct(x.upper, tb), to_struct(x.body, tb),
                ("row", (("var", x.var),)))
    if isinstance(x, DiffCall):
        return ("call", "diff", to_struct(x.body, tb), ("row", (("var", x.var),)))
    raise TypeError(x)


def walk(x):
    yield x
    if isinstance(x, Row):
        for e in x.items:
            yield from walk(e)
    elif is_atom_c(x):
        for f in x.__dataclass_fields__:
            v = getattr(x, f)
            if isinstance(v, Row) or is_atom_c(v):
                yield from walk(v)


def size(x) -> int:
    return sum(1 for n in walk(x) if not isinstance(n, Row))


def has_empty(x) -> bool:
    return any(isinstance(n, Empty) for n in walk(x))


def uses(x, cls) -> bool:
    return any(isinstance(n, cls) for n in walk(x))


def linear(x) -> str:
    if isinstance(x, Row):
        parts = []
        for e in x.items:
            parts.append("×10^{" + linear(e.exp) + "}" if isinstance(e, Pow) and e.sci else linear(e))
        return " ".join(parts) if parts else "□"
    if isinstance(x, Op):
        return {"*": "×", "/": "÷"}.get(x.kind, x.kind)
    if isinstance(x, Num):
        return x.text
    if isinstance(x, Const):
        return "π" if x.name == "pi" else "e"
    if isinstance(x, Var):
        return x.name
    if isinstance(x, Paren):
        return "(" + linear(x.body) + ")"
    if isinstance(x, Frac):
        return "{" + linear(x.num) + "}/{" + linear(x.den) + "}"
    if isinstance(x, Pow):
        return "{" + linear(Row((x.base,))) + "}^{" + linear(x.exp) + "}"
    if isinstance(x, Sqrt):
        return "√{" + linear(x.body) + "}"
    if isinstance(x, Root):
        return "root[" + linear(x.index) + "]{" + linear(x.body) + "}"
    if isinstance(x, Abs):
        return "|" + linear(x.body) + "|"
    if isinstance(x, Fn):
        return x.name + "(" + linear(x.arg) + ")"
    if isinstance(x, LogB):
        return "log_{" + linear(x.base) + "}(" + linear(x.arg) + ")"
    if isinstance(x, Empty):
        return "□"
    if isinstance(x, DefInt):
        return "∫_{" + linear(x.lower) + "}^{" + linear(x.upper) + "}(" + linear(x.body) + ")d" + x.var
    if isinstance(x, DiffCall):
        return "diff(" + linear(x.body) + "," + x.var + ")"
    raise TypeError(x)


# ── JSON (findings, reproducers) ─────────────────────────────────────────────
_CLASSES = {c.__name__: c for c in (Op, Num, Const, Var, Paren, Frac, Pow, Sqrt, Root, Abs, Fn, LogB, Empty,
                                    DefInt, DiffCall)}


def to_json(x):
    if isinstance(x, Row):
        return ["Row", [to_json(e) for e in x.items]]
    d = {}
    for f in x.__dataclass_fields__:
        v = getattr(x, f)
        d[f] = to_json(v) if (isinstance(v, (Row, Op)) or is_atom_c(v)) else v
    return [type(x).__name__, d]


def from_json(j):
    name, payload = j
    if name == "Row":
        return Row(tuple(from_json(e) for e in payload))
    kw = {}
    for k, v in payload.items():
        kw[k] = (from_json(v) if isinstance(v, list) and len(v) == 2 and isinstance(v[0], str)
                 and (v[0] == "Row" or v[0] in _CLASSES) else v)
    return _CLASSES[name](**kw)


# ── keystrokes ───────────────────────────────────────────────────────────────
# Slot-to-slot moves (a human needs RIGHT/DOWN, never `)`), extended with the
# Calculus templates' inner slots.
SLOT_MOVES = {"frac_num", "root_index", "logb_base", "di_lower", "di_upper", "di_body", "diff_body"}


class Typer(K._Typer):
    def atom(self, a):
        if isinstance(a, Var) and a.name == "y":
            self.k("y")
            return
        if isinstance(a, DefInt):
            self.out += K.toolbox_keys(DEFINT_QUERY)
            self.row(a.lower)
            self.exit("di_lower")
            self.row(a.upper)
            self.exit("di_upper")
            self.row(a.body)
            self.exit("di_body")
            self.k(a.var)
            self.exit("di_var")
            return
        if isinstance(a, DiffCall):
            self.out += K.toolbox_keys(DIFF_QUERY)
            self.row(a.body)
            self.exit("diff_body")
            self.k(a.var)
            self.exit("diff_var")
            return
        super().atom(a)


def raw(tree: Row, mode: str) -> list:
    t = Typer(mode)
    t.row(tree)
    return t.out


def canonical(rawtoks: list) -> list:
    return [("key", "right") if t[0] == "EXIT" else t for t in rawtoks]


def naive(rawtoks: list, close_key: str = ")"):
    """common.keys._naive with the Calculus slot moves (see SLOT_MOVES)."""
    out, absorbed, i = [], 0, 0
    while i < len(rawtoks):
        if rawtoks[i][0] != "EXIT":
            out.append(rawtoks[i])
            i += 1
            continue
        j = i
        while j < len(rawtoks) and rawtoks[j][0] == "EXIT":
            j += 1
        run = [t[1] for t in rawtoks[i:j]]
        last_close = max((n for n, kind in enumerate(run) if kind in K.PAREN_LIKE), default=-1)
        for n, kind in enumerate(run):
            if n <= last_close:
                if kind in K.PAREN_LIKE:
                    out.append(("key", close_key))
                elif kind in SLOT_MOVES:
                    out.append(("key", "right"))
                else:
                    absorbed += 1
            else:
                out.append(("key", "right"))
        i = j
    return out, absorbed


def project(tree: Row, path: str, rng: random.Random | None = None):
    if path == "direct":
        return canonical(raw(tree, "direct")), {}
    if path == "toolbox":
        rt = raw(tree, "toolbox")
        if rt == raw(tree, "direct"):
            return None, {"skip": "no toolbox-insertable node beyond the Calculus templates"}
        return canonical(rt), {}
    if path == "naive":
        toks, absorbed = naive(raw(tree, "direct"))
        if not absorbed:
            return None, {"skip": "no absorbed exit"}
        return toks, {"absorbed": absorbed}
    if path == "edited":
        rng = rng or random.Random(0)
        base = canonical(raw(tree, "direct"))
        variant = rng.choice(["typo_digit", "trailing", "detour"])
        return K.edit(base, variant, rng), {"variant": variant}
    raise ValueError(path)


# ── canonical Giac text (third voice) ───────────────────────────────────────
_FN = {"sin": "sin", "cos": "cos", "tan": "tan", "asin": "asin", "acos": "acos",
       "atan": "atan", "ln": "ln", "log": "log10"}


def giac_text(x, deg: bool = False) -> str:
    """Canonical Giac text of the model, with DEG made explicit (pi/180)."""
    if isinstance(x, Row):
        out, expect, neg = [], True, False
        for e in x.items:
            if isinstance(e, Op):
                if expect:
                    if e.kind == "-":
                        neg = not neg
                    continue
                out.append(e.kind)
                expect = True
                continue
            if not expect or (isinstance(e, Pow) and e.sci):
                out.append("*")
            a = giac_text(e, deg)
            out.append(f"(-({a}))" if neg else a)
            neg, expect = False, False
        return "(" + "".join(out) + ")"
    if isinstance(x, Num):
        t = x.text
        if "." in t:
            ip, fp = t.split(".")
            digits = (ip + fp).lstrip("0") or "0"
            return f"({digits}/10^{len(fp)})" if fp else f"({ip or '0'})"
        return f"({t})"
    if isinstance(x, Const):
        return "(pi)" if x.name == "pi" else "(exp(1))"
    if isinstance(x, Var):
        return f"({x.name})"
    if isinstance(x, Paren):
        return giac_text(x.body, deg)
    if isinstance(x, Frac):
        return f"({giac_text(x.num, deg)}/{giac_text(x.den, deg)})"
    if isinstance(x, Pow):
        return f"({giac_text(Row((x.base,)), deg)}^{giac_text(x.exp, deg)})"
    if isinstance(x, Sqrt):
        return f"sqrt({giac_text(x.body, deg)})"
    if isinstance(x, Root):
        return f"surd({giac_text(x.body, deg)},{giac_text(x.index, deg)})"
    if isinstance(x, Abs):
        return f"abs({giac_text(x.body, deg)})"
    if isinstance(x, Fn):
        arg = giac_text(x.arg, deg)
        if deg and x.name in ("sin", "cos", "tan"):
            return f"{x.name}({arg}*pi/180)"
        if deg and x.name in ("asin", "acos", "atan"):
            return f"(180/pi*{x.name}({arg}))"
        return f"{_FN[x.name]}({arg})"
    if isinstance(x, LogB):
        return f"logb({giac_text(x.arg, deg)},{giac_text(x.base, deg)})"
    if isinstance(x, DefInt):
        return (f"integrate({giac_text(x.body, deg)},{x.var},{giac_text(x.lower, deg)},"
                f"{giac_text(x.upper, deg)})")
    if isinstance(x, DiffCall):
        return f"diff({giac_text(x.body, deg)},{x.var})"
    if isinstance(x, Empty):
        raise ValueError("empty slot")
    raise TypeError(x)
