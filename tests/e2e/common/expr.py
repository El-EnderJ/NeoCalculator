"""Typed VPAM-shaped expression model.

A tree is a `Row` of elements. An element is an operator `Op` or an atom.
Rows follow the calculator's documented reading (docs/MATH_ENGINE.md):
  * `+`/`-` lowest, `*`/`/`(the ÷ key) and implicit multiplication (adjacent
    atoms) next, all left-to-right;
  * a `-` at the start of a row or after another operator is a unary minus that
    negates the following factor (powers bind tighter: -2^2 = -4);
  * templates (fraction, power, roots, |x|, functions, log_b) are atoms whose
    slots are rows.

This mirrors the structure NumOS builds, so the same tree yields
 (a) keystrokes (keys.py), (b) the oracle value (oracle.py), and
 (c) the expected structural S-expression compared with the dumped AST.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Tuple, Union

VARS = ("A", "B", "C", "D", "E")       # F is reserved for STO round trips
FUNCS = ("sin", "cos", "tan", "asin", "acos", "atan", "ln", "log")
PAREN_LIKE_EXITS = ("paren", "fn", "logb_arg", "abs")


@dataclass(frozen=True)
class Op:
    kind: str  # '+', '-', '*', '/'(÷ key)


@dataclass(frozen=True)
class Num:
    text: str  # non-negative decimal literal as typed: "12", "0.5", "3."


@dataclass(frozen=True)
class Const:
    name: str  # 'pi' | 'e'


@dataclass(frozen=True)
class Var:
    name: str  # 'A'..'F', 'x', 'Ans'


@dataclass(frozen=True)
class Row:
    items: Tuple["Elem", ...] = ()


@dataclass(frozen=True)
class Paren:
    body: Row


@dataclass(frozen=True)
class Frac:
    num: Row
    den: Row
    capture: bool = False  # typed as  <atom> /  (numerator captured)


@dataclass(frozen=True)
class Pow:
    base: "Atom"
    exp: Row
    sci: bool = False      # typed with EXP (×10^): base is Num('10') and a
                           # preceding Op('*') is produced by the same key


@dataclass(frozen=True)
class Sqrt:
    body: Row


@dataclass(frozen=True)
class Root:
    index: Row
    body: Row


@dataclass(frozen=True)
class Abs:
    body: Row


@dataclass(frozen=True)
class Fn:
    name: str
    arg: Row


@dataclass(frozen=True)
class LogB:
    base: Row
    arg: Row


@dataclass(frozen=True)
class Empty:
    pass


Atom = Union[Num, Const, Var, Paren, Frac, Pow, Sqrt, Root, Abs, Fn, LogB, Empty]
Elem = Union[Op, Atom]


def is_atom(e) -> bool:
    return not isinstance(e, Op)


def row(*items) -> Row:
    return Row(tuple(items))


# ── Expected structure (normalised to the parsed dump form) ─────────────────
_OPNAME = {"+": "Add", "-": "Sub", "*": "Mul", "/": "Div"}


def first_atom(a):
    """The atom whose key is typed first when `a` is typed."""
    if isinstance(a, Pow) and not a.sci:
        return first_atom(a.base)
    if isinstance(a, Frac) and a.capture:
        return first_atom(a.num.items[0])
    return a


DIRECT_TOOLBOX = frozenset({"root", "abs"})
TOOLBOX_TOOLBOX = DIRECT_TOOLBOX | {"logb", "fn:ln", "fn:log", "fn:asin", "fn:acos", "fn:atan"}


def toolbox_inserted(a, tb) -> bool:
    f = first_atom(a)
    if isinstance(f, Root):
        return "root" in tb
    if isinstance(f, Abs):
        return "abs" in tb
    if isinstance(f, LogB):
        return "logb" in tb
    if isinstance(f, Fn):
        return ("fn:" + f.name) in tb
    return False


def to_struct(x, tb=DIRECT_TOOLBOX):
    """Return the parsed-sexpr tuple the NumOS input dump should equal.

    `tb` names the node kinds inserted through the Toolbox in this keystroke
    path: a Toolbox insertion right after an operand adds an explicit ×
    (observed: "2" + Toolbox |x| -> 2 × |x|), unlike typing a key."""
    if isinstance(x, Row):
        out = []
        prev_atom = False
        for e in x.items:
            if isinstance(e, Pow) and e.sci:
                # EXP inserts "× 10^□"; the × belongs to the same keystroke.
                out.append(("op", "Mul"))
            elif prev_atom and is_atom(e) and toolbox_inserted(e, tb):
                out.append(("op", "Mul"))
            out.append(to_struct(e, tb))
            prev_atom = is_atom(e)
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
    raise TypeError(x)


def has_empty(x) -> bool:
    """An authored empty slot is modelled as Row((Empty(),))."""
    return any(isinstance(n, Empty) for n in walk(x))


def is_atom_obj(v) -> bool:
    return isinstance(v, (Num, Const, Var, Paren, Frac, Pow, Sqrt, Root, Abs, Fn, LogB, Empty))


def walk(x):
    """Yield every element/row in the tree (pre-order)."""
    yield x
    if isinstance(x, Row):
        for e in x.items:
            yield from walk(e)
    elif is_atom_obj(x):
        for f in x.__dataclass_fields__:
            v = getattr(x, f)
            if isinstance(v, Row) or is_atom_obj(v):
                yield from walk(v)


def uses(x, cls) -> bool:
    return any(isinstance(n, cls) for n in walk(x))


def size(x) -> int:
    return sum(1 for n in walk(x) if not isinstance(n, Row))


def linear(x) -> str:
    """Readable rendering of the intended tree (reports only)."""
    if isinstance(x, Row):
        parts = []
        for e in x.items:
            if isinstance(e, Pow) and e.sci:
                parts.append("×10^{" + linear(e.exp) + "}")
            else:
                parts.append(linear(e))
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
        return "{" + linear(x.base) + "}^{" + linear(x.exp) + "}"
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
    raise TypeError(x)


# ── Serialisation of the model to/from JSON (case files, reproducers) ───────
def to_json(x):
    if isinstance(x, Row):
        return ["Row", [to_json(e) for e in x.items]]
    name = type(x).__name__
    d = {}
    for f in x.__dataclass_fields__:
        v = getattr(x, f)
        d[f] = to_json(v) if (isinstance(v, Row) or is_atom_obj(v) or isinstance(v, Op)) else v
    return [name, d]


_CLASSES = {c.__name__: c for c in (Op, Num, Const, Var, Paren, Frac, Pow, Sqrt, Root, Abs, Fn, LogB, Empty)}


def from_json(j):
    name, payload = j
    if name == "Row":
        return Row(tuple(from_json(e) for e in payload))
    cls = _CLASSES[name]
    kw = {}
    for k, v in payload.items():
        kw[k] = from_json(v) if isinstance(v, list) and len(v) == 2 and isinstance(v[0], str) and (v[0] == "Row" or v[0] in _CLASSES) else v
    return cls(**kw)
