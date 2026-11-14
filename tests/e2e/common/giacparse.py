"""Independent reader of the Giac-bound text that NumOS produced (I2).

Parses the subset of Giac syntax the Calculation serializer emits and
evaluates it with SymPy under Giac's reading (`^` right-associative and above
unary minus, `*`/`/` left-associative, `log10`, `logb(x,b)`, `surd(x,n)`,
degree-mode trig). It shares no code with NumOS; its only job is to answer
"does this string denote the source tree?".
"""
from __future__ import annotations

import re

import sympy as sp

from .evaluator import X, _nonfinite

_TOKEN = re.compile(r"\s*(?:(\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)|([A-Za-z_][A-Za-z_0-9]*)|(.))")


class GiacParseError(ValueError):
    pass


def _tokens(text):
    pos, out = 0, []
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        num, ident, sym = m.groups()
        if num is not None:
            out.append(("num", num))
        elif ident is not None:
            out.append(("id", ident))
        elif sym is not None and not sym.isspace():
            out.append(("sym", sym))
    return out


def parse_eval(text: str, angle: str = "rad", env: dict | None = None):
    env = env or {}
    toks = _tokens(text)
    i = 0
    deg = angle == "deg"

    def peek():
        return toks[i] if i < len(toks) else (None, None)

    def take(kind=None, val=None):
        nonlocal i
        t = peek()
        if t[0] is None or (kind and t[0] != kind) or (val and t[1] != val):
            raise GiacParseError(f"expected {val or kind} at token {i}: {t}")
        i += 1
        return t

    def add():
        v = mul()
        while peek() in (("sym", "+"), ("sym", "-")):
            op = take()[1]
            r = mul()
            v = sp.nan if (_nonfinite(v) or _nonfinite(r)) else (v + r if op == "+" else v - r)
        return v

    def mul():
        v = unary()
        while peek() in (("sym", "*"), ("sym", "/")):
            op = take()[1]
            r = unary()
            if _nonfinite(v) or _nonfinite(r):
                v = sp.nan
            elif op == "*":
                v = v * r
            else:
                v = sp.zoo if r == 0 else v / r
        return v

    def unary():
        if peek() == ("sym", "-"):
            take()
            return -unary()
        if peek() == ("sym", "+"):
            take()
            return unary()
        return power()

    def power():
        b = postfix()
        if peek() == ("sym", "^"):
            take()
            e = unary()
            if _nonfinite(b) or _nonfinite(e):
                return sp.nan
            if b == 0 and e == 0:
                return sp.nan
            if b == 0 and e.is_number and e.is_real and e < 0:
                return sp.zoo
            return sp.Pow(b, e)
        return b

    def postfix():
        v = primary()
        while peek() == ("sym", "!"):
            take()
            v = sp.factorial(v)
        return v

    def args():
        take("sym", "(")
        out = [add()]
        while peek() == ("sym", ","):
            take()
            out.append(add())
        take("sym", ")")
        return out

    def primary():
        t = peek()
        if t[0] == "num":
            take()
            return sp.Rational(t[1]) if "e" not in t[1].lower() else sp.Rational(sp.Float(t[1], 30))
        if t == ("sym", "("):
            take()
            v = add()
            take("sym", ")")
            return v
        if t[0] == "id":
            name = take()[1]
            if peek() == ("sym", "("):
                a = args()
                return call(name, a)
            if name == "pi":
                return sp.pi
            if name == "i":
                return sp.I
            if name == "infinity":
                return sp.oo
            if name == "x":
                return X
            if name == "numos_Ans":
                return env["Ans"]
            if name == "numos_PreAns":
                return env["PreAns"]
            if name in env:
                return env[name]
            raise GiacParseError(f"unknown identifier {name}")
        raise GiacParseError(f"unexpected token {t}")

    def call(name, a):
        if any(_nonfinite(x) for x in a):
            return sp.nan
        if name in ("sin", "cos", "tan"):
            x = a[0] * sp.pi / 180 if deg else a[0]
            return {"sin": sp.sin, "cos": sp.cos, "tan": sp.tan}[name](x)
        if name in ("asin", "acos", "atan"):
            r = {"asin": sp.asin, "acos": sp.acos, "atan": sp.atan}[name](a[0])
            return r * 180 / sp.pi if deg else r
        if name == "ln" or name == "log":
            return sp.zoo if a[0] == 0 else sp.log(a[0])
        if name == "log10":
            return sp.zoo if a[0] == 0 else sp.log(a[0]) / sp.log(10)
        if name == "logb":
            x, b = a
            return sp.zoo if (x == 0 or b == 0 or b == 1) else sp.log(x) / sp.log(b)
        if name == "sqrt":
            return sp.sqrt(a[0])
        if name == "abs":
            return sp.Abs(a[0])
        if name == "exp":
            return sp.exp(a[0])
        if name == "surd":
            x, n = a
            if n.is_Integer and x.is_number and x.is_real and x < 0 and n % 2 == 1:
                return -sp.Pow(-x, sp.Rational(1, n))
            return sp.Pow(x, 1 / n)
        if name == "factorial":
            return sp.factorial(a[0])
        raise GiacParseError(f"unknown function {name}")

    v = add()
    if i != len(toks):
        raise GiacParseError(f"trailing tokens from {i}: {toks[i:i+5]}")
    return v
