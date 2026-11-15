"""Equations surface model: typing projections, expected structure, numeric
semantics of an equation side (mpmath, 60 digits, real-domain tracking).

An equation is a pair of rows (lhs, rhs) over the common expression model
(common/expr.py) with variables x, y, z and parameters A-F. The app solves for
x (one equation), (x, y) or (x, y, z) (systems of 2 or 3 equations).

Key paths (all observed on the emulator, probe logs in
out/correctness-e2e-01/scratch/w5/):
  direct    script `key` names, RIGHT leaves every slot; |x| and the indexed
            root have no `key` name in Equations and are typed through the
            production keypad resolver (SHIFT+sqrt, SHIFT+^), as on hardware.
  physical  every key through `equations_physical ROW COL` (production
            keypad map + KeySemanticResolver, the hardware path). log_b has no
            physical key and is inserted through the Toolbox on this path.
  toolbox   ln/log/asin/acos/atan/log_b/|x|/nthroot inserted via Toolbox search.
  edited    direct + a corrected typo / detour / reopen-and-retype of E1.
  naive     `)` used to close brackets (absorbing fraction/power exits), as a
            human who expects `)` to close everything inside. In Equations `)`
            is CursorController::moveRight(), not closeParen().
"""
from __future__ import annotations

import random

import mpmath

from common import keys as K
from common.expr import (Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row,
                         Sqrt, Var, linear, to_struct)
from common.sexpr import as_row

# Electrical (row, col) of each physical key (src/input/generated/ProductionKeypadMap.generated.h).
PHYS = {
    "0": "4 9", "1": "4 8", "2": "3 8", "3": "2 8", "4": "4 7", "5": "3 7", "6": "2 7",
    "7": "4 6", "8": "3 6", "9": "2 6", ".": "3 9", "+": "1 8", "-": "0 8", "neg": "1 9",
    "*": "1 7", "divide": "0 7", "/": "3 2", "^": "1 2", "sqrt": "2 2", "(": "1 4", ")": "0 4",
    "x": "4 2", "=": "2 5", "sin": "2 3", "cos": "1 3", "tan": "0 3", "ln": "3 3", "log": "4 3",
    "pi": "3 4", "e": "2 4", "right": "1 1", "left": "3 1", "up": "2 0", "down": "2 1",
    "del": "1 6", "ac": "0 6", "exe": "0 9", "enter": "0 9", "shift": "4 0", "alpha": "3 0",
    "var": "4 1", "toolbox": "0 1", "exp": "2 9",
}
# ALPHA plane letters (plane 2 of kMathPlaneDefinitions).
ALPHA_POS = {"A": "4 2", "B": "3 2", "C": "2 2", "D": "1 2", "E": "0 2", "F": "4 3"}
PARAMS = ("A", "B", "C", "D", "E", "F")
EQ = Op("=")  # marker only; never inside a Row


def phys(name):
    return ("phys", PHYS[name])


class EqTyper(K._Typer):
    """Typing of one equation side. mode: direct | physical | toolbox."""

    def __init__(self, mode):
        super().__init__("toolbox" if mode == "toolbox" else "direct")
        self.emode = mode

    def k(self, *names):
        for n in names:
            if self.emode == "physical":
                if n == "shift" or n == "alpha":
                    self.out.append(("phys", PHYS[n]))
                elif n in PHYS:
                    self.out.append(("phys", PHYS[n]))
                else:
                    raise KeyError(n)
            else:
                self.out.append(("key", n))

    def row(self, r):
        # `=` only appears inside a row in the deliberately malformed cases
        if not any(isinstance(e, Op) and e.kind == "=" for e in r.items):
            return super().row(r)
        seg = []
        for e in list(r.items) + [None]:
            if e is None or (isinstance(e, Op) and e.kind == "="):
                super().row(Row(tuple(seg)))
                seg = []
                if e is not None:
                    self.k("=")
            else:
                seg.append(e)

    def shifted(self, base):
        # production resolver path (SHIFT plane), identical on every path
        self.out += [("phys", PHYS["shift"]), ("phys", PHYS[base])]

    def atom(self, a):
        if isinstance(a, Var):
            if a.name == "x":
                self.k("x")
            elif a.name == "y":
                if self.emode == "physical":
                    self.shifted("x")          # SHIFT+x,t,θ,n = variable_y
                else:
                    self.out.append(("key", "y"))
            elif a.name == "z":
                # VAR picker (x, y, z): the only product path to lower-case z
                self.k("var", "right", "right", "enter")
            elif a.name in ALPHA_POS:
                self.out += [("phys", PHYS["alpha"]), ("phys", ALPHA_POS[a.name])]
            else:
                raise ValueError(a.name)
            return
        if isinstance(a, Abs) and self.emode != "toolbox":
            self.shifted("sqrt")              # SHIFT+sqrt = absolute_value
            self.row(a.body)
            self.exit("abs")
            return
        if isinstance(a, Root) and self.emode != "toolbox":
            self.shifted("^")                 # SHIFT+^ = nth_root_template (degree first)
            self.row(a.index)
            self.exit("root_index")
            self.row(a.body)
            self.exit("root_body")
            return
        if isinstance(a, Fn) and a.name in ("asin", "acos", "atan") and self.emode != "toolbox":
            self.shifted(a.name[1:])
            self.row(a.arg)
            self.exit("fn")
            return
        if isinstance(a, LogB) and self.emode == "physical":
            self.out += K.toolbox_keys(K.TOOLBOX_QUERY["logb"])
            self.row(a.base)
            self.exit("logb_base")
            self.row(a.arg)
            self.exit("logb_arg")
            return
        super().atom(a)


def raw_side(r: Row, mode: str):
    t = EqTyper(mode)
    t.row(r)
    return t.out


def _canon(raw, mode):
    right = ("phys", PHYS["right"]) if mode == "physical" else ("key", "right")
    return [right if tok[0] == "EXIT" else tok for tok in raw]


def eq_tokens(lhs: Row, rhs: Row, mode: str = "direct", naive: bool = False):
    """Tokens typing `lhs = rhs` in a blank editor (no ENTER)."""
    eqk = ("phys", PHYS["="]) if mode == "physical" else ("key", "=")
    raw = raw_side(lhs, mode) + [eqk] + raw_side(rhs, mode)
    if naive:
        toks, absorbed = K._naive(raw)
        return toks, absorbed
    return _canon(raw, mode), 0


def toolbox_used(lhs, rhs):
    from common.expr import walk
    for side in (lhs, rhs):
        for n in walk(side):
            if isinstance(n, (Abs, Root, LogB)) or (isinstance(n, Fn) and n.name in K.TOOLBOX_QUERY):
                return True
    return False


def has_logb(lhs, rhs):
    from common.expr import walk
    return any(isinstance(n, LogB) for s in (lhs, rhs) for n in walk(s))


def to_lines(tokens):
    out = []
    for kind, val in tokens:
        if kind == "key":
            out.append(f"key {val}")
        elif kind == "text":
            out.append(f"sdl_text {val}")
        elif kind == "phys":
            out.append(f"equations_physical {val}")
        else:
            raise ValueError(kind)
    return out


def compact(tokens):
    inv = {v: k for k, v in PHYS.items() if k not in ("enter",)}
    out = []
    for kind, val in tokens:
        if kind == "key":
            out.append(val)
        elif kind == "text":
            out.append(f"<{val}>")
        else:
            out.append("#" + inv.get(val, val.replace(" ", ",")))
    return " ".join(out)


# ── expected structure ───────────────────────────────────────────────────────
TB_DIRECT = frozenset()        # |x| and roots via resolver semantics: no explicit ×
TB_TOOLBOX = frozenset({"root", "abs", "logb", "fn:ln", "fn:log", "fn:asin", "fn:acos", "fn:atan"})
TB_PHYSICAL = frozenset({"logb"})


def row_struct(r, tb):
    segs, cur = [], []
    for e in r.items:
        if isinstance(e, Op) and e.kind == "=":
            segs.append(cur)
            cur = []
        else:
            cur.append(e)
    segs.append(cur)
    out = []
    for i, seg in enumerate(segs):
        if i:
            out.append(("op", "Eq"))
        out += list(to_struct(Row(tuple(seg)), tb)[1])
    return tuple(out)


def eq_struct(lhs, rhs, tb=TB_DIRECT):
    return ("row", row_struct(lhs, tb) + (("op", "Eq"),) + row_struct(rhs, tb))


def split_eq(struct):
    """Split a parsed equation row at its top-level `=`; None unless exactly one."""
    if not struct or struct[0] != "row":
        return None
    items = struct[1]
    idx = [i for i, e in enumerate(items) if e == ("op", "Eq")]
    if len(idx) != 1:
        return None
    i = idx[0]
    return ("row", tuple(items[:i])), ("row", tuple(items[i + 1:]))


def eq_linear(lhs, rhs):
    return linear(lhs) + " = " + linear(rhs)


# ── numeric semantics of a parsed side (mpmath) ──────────────────────────────
class Domain(Exception):
    """The side is not defined (pole, log of 0) or not real in real mode."""


class Incomplete(Exception):
    pass


DPS = 60
_ZERO_TOL = None


def _ztol():
    return mpmath.mpf(10) ** (-(DPS - 15))


class MPEval:
    """Evaluates a parsed VPAM structure at numeric variable values.

    Semantics mirror common/evaluator.py (documented VPAM reading, principal
    branches). With real=True any non-real intermediate value raises Domain
    (the real-domain reading of an equation: sqrt/log/even roots/asin/acos
    must receive admissible real arguments); odd roots of negatives use the
    real root."""

    def __init__(self, env, angle="rad", real=False):
        self.env = env
        self.deg = angle == "deg"
        self.real = real

    def chk(self, v):
        if not (mpmath.isfinite(v.real) and mpmath.isfinite(v.imag)):
            raise Domain("non-finite")
        if self.real and abs(v.imag) > _ztol() * max(1, abs(v.real)):
            raise Domain("non-real intermediate")
        if self.real:
            return mpmath.mpc(v.real, 0)
        return v

    def eval(self, node):
        if node is None:
            raise Incomplete("null")
        h = node[0]
        f = getattr(self, "_" + h, None)
        if f is None:
            raise Incomplete(f"unsupported node {h}")
        return f(*node[1:])

    def slot(self, n):
        return self.row(as_row(n))

    def row(self, r):
        items = list(r[1])
        if not items:
            raise Incomplete("empty row")
        seq, expect, neg = [], True, False
        for e in items:
            if e is not None and e[0] == "op":
                name = e[1]
                if expect:
                    if name == "Sub":
                        neg = not neg
                    elif name != "Add":
                        raise Incomplete(f"operator {name} without left operand")
                else:
                    if name not in ("Add", "Sub", "Mul", "Div"):
                        raise Incomplete(f"operator {name}")
                    seq.append(name)
                    expect = True
                continue
            if e is not None and e[0] == "sym":
                continue
            v = self.eval(e)
            if neg:
                v, neg = -v, False
            if not expect:
                seq.append("Mul")
            seq.append(v)
            expect = False
        if expect:
            raise Incomplete("dangling operator")
        terms, cur, pending, k = [], seq[0], "Add", 1
        while k < len(seq):
            op, rhs = seq[k], seq[k + 1]
            if op == "Mul":
                cur = self.chk(cur * rhs)
            elif op == "Div":
                cur = self.div(cur, rhs)
            else:
                terms.append((pending, cur))
                pending, cur = op, rhs
            k += 2
        terms.append((pending, cur))
        total = mpmath.mpc(0)
        for op, v in terms:
            total = total + v if op == "Add" else total - v
        return self.chk(total)

    def div(self, a, b):
        if abs(b) <= mpmath.mpf(10) ** (-(DPS - 5)):
            raise Domain("division by zero")
        return self.chk(a / b)

    def _row(self, items):
        return self.row(("row", items))

    def _num(self, t):
        if t.startswith("."):
            t = "0" + t
        if t.endswith("."):
            t = t + "0"
        return mpmath.mpc(mpmath.mpf(t))

    def _empty(self):
        raise Incomplete("empty slot")

    def _frac(self, n, d):
        return self.div(self.slot(n), self.slot(d))

    def _pow(self, base, exp):
        b, e = self.slot(base), self.slot(exp)
        return self.power(b, e)

    def power(self, b, e):
        if abs(b) == 0:
            if abs(e) == 0:
                raise Domain("0^0")
            if e.real < 0:
                raise Domain("0^negative")
            return mpmath.mpc(0)
        if self.real and abs(e.imag) == 0 and b.real < 0 and abs(b.imag) == 0:
            # integer exponent fine; odd-denominator rational -> real root
            er = e.real
            if mpmath.isint(er):
                return self.chk(mpmath.power(b.real, er))
            frac = _as_fraction(er)
            if frac is not None and frac[1] % 2 == 1:
                mag = mpmath.power(-b.real, er)
                return mpmath.mpc(mag if frac[0] % 2 == 0 else -mag)
            raise Domain("negative base, non-integer exponent")
        return self.chk(mpmath.power(b, e))

    def _sqrt(self, body):
        v = self.slot(body)
        if self.real and v.real < -_ztol():
            raise Domain("sqrt of negative")
        if self.real and v.real < 0:
            v = mpmath.mpc(0)
        return self.chk(mpmath.sqrt(v))

    def _root(self, body, index):
        a, n = self.slot(body), self.slot(index)
        if abs(n) == 0:
            raise Domain("0th root")
        if abs(n.imag) == 0 and mpmath.isint(n.real) and abs(a.imag) == 0 and a.real < 0:
            ni = int(n.real)
            if ni % 2:
                return mpmath.mpc(-mpmath.power(-a.real, mpmath.mpf(1) / ni))
            if self.real:
                raise Domain("even root of negative")
        return self.chk(mpmath.power(a, 1 / n))

    def _paren(self, kind, body):
        v = self.slot(body)
        if kind == "bar":
            return mpmath.mpc(abs(v))
        return v

    def _fn(self, name, arg):
        v = self.slot(arg)
        if name in ("sin", "cos", "tan"):
            x = v * mpmath.pi / 180 if self.deg else v
            if name == "tan":
                c = mpmath.cos(x)
                if abs(c) < mpmath.mpf(10) ** (-(DPS - 10)):
                    raise Domain("tan pole")
                return self.chk(mpmath.tan(x))
            return self.chk(mpmath.sin(x) if name == "sin" else mpmath.cos(x))
        if name in ("asin", "acos", "atan"):
            if self.real and name != "atan" and abs(v.real) > 1 + _ztol():
                raise Domain(f"{name} outside [-1,1]")
            if name != "atan" and abs(v.imag) == 0 and abs(v.real) <= 1:
                r = {"asin": mpmath.asin, "acos": mpmath.acos}[name](v.real)
                r = mpmath.mpc(r)
            else:
                r = {"asin": mpmath.asin, "acos": mpmath.acos, "atan": mpmath.atan}[name](v)
            if self.deg:
                r = r * 180 / mpmath.pi
            return self.chk(r)
        if name in ("ln", "log"):
            if abs(v) < mpmath.mpf(10) ** (-(DPS - 5)):
                raise Domain("log of 0")
            if self.real and (v.real <= 0):
                raise Domain("log of non-positive")
            r = mpmath.log(v)
            if name == "log":
                r = r / mpmath.log(10)
            return self.chk(r)
        raise Incomplete(f"fn {name}")

    def _logb(self, base, arg):
        b, a = self.slot(base), self.slot(arg)
        if abs(a) == 0 or abs(b) == 0 or abs(b - 1) == 0:
            raise Domain("logb undefined")
        if self.real and (a.real <= 0 or b.real <= 0):
            raise Domain("logb of non-positive")
        return self.chk(mpmath.log(a) / mpmath.log(b))

    def _const(self, name):
        if name == "i":
            if self.real:
                raise Domain("i in real mode")
            return mpmath.mpc(0, 1)
        return mpmath.mpc(mpmath.pi if name == "pi" else mpmath.e)

    def _var(self, name):
        if name in self.env:
            return mpmath.mpc(self.env[name])
        raise Incomplete(f"unbound variable {name}")


def _as_fraction(x, maxden=1000):
    from fractions import Fraction
    f = Fraction(str(mpmath.nstr(x, 40))).limit_denominator(maxden)
    if abs(mpmath.mpf(f.numerator) / f.denominator - x) < mpmath.mpf(10) ** -40:
        return f.numerator, f.denominator
    return None


def side_value(struct, env, angle, real):
    with mpmath.workdps(DPS):
        return MPEval(env, angle, real).row(as_row(struct))


def residual(lhs_s, rhs_s, env, angle, real, tol=None):
    """(ok, info) - is `env` a solution of lhs=rhs? Both sides must be defined
    (and real in real mode); |l-r| <= 1e-40 * scale."""
    with mpmath.workdps(DPS):
        try:
            l = MPEval(env, angle, real).row(as_row(lhs_s))
            r = MPEval(env, angle, real).row(as_row(rhs_s))
        except Domain as exc:
            return False, f"domain: {exc}"
        except (ZeroDivisionError, ValueError, OverflowError) as exc:
            return False, f"domain: {type(exc).__name__}"
        scale = max(mpmath.mpf(1), abs(l), abs(r))
        d = abs(l - r)
        t = mpmath.mpf(10) ** -40 if tol is None else mpmath.mpf(tol)
        return d <= t * scale, f"|l-r|={mpmath.nstr(d, 5)}"
