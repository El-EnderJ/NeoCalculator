"""Reference semantics for VPAM structures (oracle side).

`evaluate(struct, ctx)` evaluates a parsed structure (see sexpr.py; the same
shape is produced from the expression model by expr.to_struct) with SymPy,
following the calculator's documented reading (docs/MATH_ENGINE.md) and
PRINCIPAL complex branches. It never looks at NumOS output or Giac.

Policy-ambiguous points do not have a single "right" answer on a calculator.
They are represented as a CANDIDATE SET (first = principal). Any candidate is
accepted by the checkers; the ambiguity is recorded as a note:
  * 0^0                         -> {1, undefined}
  * (-a)^(p/q), q odd            -> {principal complex, real root}
  * asin/acos(x), |x|>1 (on cut) -> {principal, conjugate}
  * nth root (surd) of a negative with even n -> {principal complex, undefined}
  * a pole/undefined point        -> kind "nonfinite" (∞, -∞, undefined and an
                                    error status are equivalent outcomes)
"""
from __future__ import annotations

from dataclasses import dataclass, field

import mpmath
import sympy as sp

from .sexpr import as_row

X = sp.Symbol("x")
_IU = sp.Symbol("e2e_imaginary_unit")
DPS = 50
MAX_CANDIDATES = 6
# Sample points for symbolic (x-dependent) comparisons: generic rationals.
X_SAMPLES = (sp.Rational(7, 3), sp.Rational(-13, 5))


class Incomplete(Exception):
    """Structurally incomplete input (empty slot, dangling operator)."""


class Unsupported(Exception):
    """Structure the reference semantics does not model."""


@dataclass
class Ctx:
    angle: str = "rad"                 # 'rad' | 'deg'
    env: dict = field(default_factory=dict)   # 'A'..'F','Ans','PreAns' -> sympy
    mixed: bool = False                # display: juxtaposed int+frac = sum
    notes: list = field(default_factory=list)


def _nonfinite(v) -> bool:
    try:
        return bool(v.has(sp.zoo, sp.oo, -sp.oo, sp.nan)) or v in (sp.zoo, sp.oo, -sp.oo, sp.nan)
    except Exception:
        return True


def _dedupe(vals):
    out = []
    for v in vals:
        if any(_same(v, w) for w in out):
            continue
        out.append(v)
        if len(out) >= MAX_CANDIDATES:
            break
    return out


def _same(a, b) -> bool:
    if _nonfinite(a) or _nonfinite(b):
        return _nonfinite(a) and _nonfinite(b)
    try:
        return close(a, b, rel=sp.Rational(1, 10**35))
    except Exception:
        return False


def numeric(v, dps=DPS):
    """Complex mpmath value of a constant SymPy expression (None if nonfinite).

    Fixed-precision evaluation (lambdify to mpmath at `dps`+10 digits) rather
    than SymPy's adaptive evalf, which can spin on expressions that are
    exactly zero. Cancellation below ~1e-(dps) is absorbed by close()'s
    absolute floor."""
    if _nonfinite(v):
        return None
    with mpmath.workdps(dps + 10):
        try:
            # sp.I would print as Python 1j: int*1j is a double-precision
            # complex. Route the imaginary unit through an mpc argument.
            f = sp.lambdify((_IU,), v.subs(sp.I, _IU), modules="mpmath")
            n = mpmath.mpc(f(mpmath.mpc(0, 1)))
        except (TypeError, ValueError, ZeroDivisionError, OverflowError, NameError, AttributeError):
            n = None
        if n is None:
            e = sp.N(v, dps)
            if e.has(sp.zoo, sp.oo, -sp.oo, sp.nan):
                return None
            re, im = e.as_real_imag()
            n = mpmath.mpc(mpmath.mpf(str(sp.N(re, dps))), mpmath.mpf(str(sp.N(im, dps))))
        if not (mpmath.isfinite(n.real) and mpmath.isfinite(n.imag)):
            return None
        return n


def close(a, b, rel=sp.Rational(1, 10**30), absol=None) -> bool:
    """Numeric equality of two SymPy values (x sampled at generic points)."""
    fa, fb = a.free_symbols, b.free_symbols
    if fa or fb:
        for s in X_SAMPLES:
            if not close(a.subs(X, s), b.subs(X, s), rel, absol):
                return False
        return True
    na, nb = numeric(a), numeric(b)
    if na is None or nb is None:
        return na is None and nb is None
    with mpmath.workdps(DPS):
        diff = abs(na - nb)
        scale = max(abs(na), abs(nb))
        tol = mpmath.mpf(str(sp.N(rel, 20))) * scale
        if absol is not None:
            tol = max(tol, mpmath.mpf(str(absol)))
        else:
            tol = max(tol, mpmath.mpf(10) ** (-(DPS - 8)))
        return diff <= tol


class Evaluator:
    def __init__(self, ctx: Ctx):
        self.ctx = ctx

    # Each method returns a non-empty list of candidate SymPy values.
    def eval(self, node):
        if node is None:
            raise Incomplete("null")
        head = node[0]
        fn = getattr(self, "_" + head, None)
        if fn is None:
            raise Unsupported(head)
        return fn(*node[1:])

    def slot(self, node):
        return self.row(as_row(node))

    def _row(self, items):
        return self.row(("row", items))

    def row(self, r):
        items = list(r[1])
        if not items:
            raise Incomplete("empty row")
        # Tokenise: [value-candidates, op, value-candidates, ...]
        seq = []
        expect = True
        neg = False
        i = 0
        while i < len(items):
            e = items[i]
            if e is not None and e[0] == "op":
                name = e[1]
                if expect:
                    if name == "Sub":
                        neg = not neg
                    elif name == "Add":
                        pass  # unary plus is a no-op
                    else:
                        raise Incomplete(f"operator {name} without left operand")
                else:
                    if name not in ("Add", "Sub", "Mul", "Div"):
                        raise Unsupported(f"operator {name}")
                    seq.append(name)
                    expect = True
                i += 1
                continue
            if e is not None and e[0] == "sym":
                i += 1  # unit/angle marks in results (°, rad): not a value
                continue
            vals = self.eval(e)
            if neg:
                vals = [-v for v in vals]
                neg = False
            if not expect:
                if (self.ctx.mixed and e[0] == "frac" and len(seq) >= 1
                        and isinstance(seq[-1], list)):
                    # Mixed-fraction display convention: "3 1/7" means 3 + 1/7.
                    prev = seq.pop()
                    vals = _dedupe([p + v if not (p.is_number and p < 0) else p - v
                                    for p in prev for v in vals])
                    seq.append(vals)
                    expect = False
                    i += 1
                    continue
                seq.append("Mul")  # implicit multiplication
            seq.append(vals)
            expect = False
            i += 1
        if expect:
            raise Incomplete("dangling operator")
        # Fold * and / (incl. implicit) left to right, then + and -.
        terms = []          # list of (sign_op, candidates)
        cur = seq[0]
        pending = "Add"
        k = 1
        while k < len(seq):
            op, rhs = seq[k], seq[k + 1]
            if op in ("Mul", "Div"):
                cur = _dedupe([(sp.nan if (_nonfinite(a) or _nonfinite(b)) else
                                (a * b if op == "Mul" else self._div(a, b))) for a in cur for b in rhs])
            else:
                terms.append((pending, cur))
                pending, cur = op, rhs
            k += 2
        terms.append((pending, cur))
        total = [sp.Integer(0)]
        for op, vals in terms:
            total = _dedupe([(sp.nan if (_nonfinite(t) or _nonfinite(v)) else (t + v if op == "Add" else t - v))
                             for t in total for v in vals])
        return total

    def _div(self, a, b):
        if _nonfinite(a) or _nonfinite(b):
            return sp.nan
        if b == 0:
            return sp.zoo
        return a / b

    def _num(self, text):
        t = text
        if t.startswith("."):
            t = "0" + t
        if t.endswith("."):
            t = t + "0"
        return [sp.Rational(t)]

    def _empty(self):
        raise Incomplete("empty slot")

    def _frac(self, n, d):
        return _dedupe([self._div(a, b) for a in self.slot(n) for b in self.slot(d)])

    def _pow(self, base, exp):
        out = []
        for b in self.slot(base):
            for e in self.slot(exp):
                out += self.power(b, e)
        return _dedupe(out)

    def power(self, b, e):
        if _nonfinite(b) or _nonfinite(e):
            return [sp.nan]
        if b == 0 and e == 0:
            self.ctx.notes.append("0^0 policy")
            return [sp.Integer(1), sp.nan]
        if b == 0 and e.is_number and e.is_real and e < 0:
            return [sp.zoo]
        v = sp.Pow(b, e)
        out = [v]
        if (b.is_number and b.is_real and b < 0 and e.is_Rational and not e.is_Integer
                and e.q % 2 == 1):
            self.ctx.notes.append("negative base, odd-denominator exponent")
            out.append((-1) ** e.p * sp.Pow(-b, e))
        return out

    def _sqrt(self, body):
        return _dedupe([sp.sqrt(v) for v in self.slot(body)])

    def _root(self, body, index):
        out = []
        for a in self.slot(body):
            for n in self.slot(index):
                if n == 0:
                    out.append(sp.nan)
                    continue
                if n.is_Integer and a.is_number and a.is_real and a < 0:
                    if n % 2 == 1:
                        out.append(-sp.Pow(-a, sp.Rational(1, n)))
                    else:
                        self.ctx.notes.append("even root of negative")
                        out += [sp.Pow(a, sp.Rational(1, n)), sp.nan]
                else:
                    if not n.is_Integer:
                        self.ctx.notes.append("non-integer root index")
                    out.append(sp.Pow(a, 1 / n))
        return _dedupe(out)

    def _paren(self, kind, body):
        vals = self.slot(body)
        if kind == "bar":
            return _dedupe([sp.Abs(v) for v in vals])
        return vals

    def _fn(self, name, arg):
        deg = self.ctx.angle == "deg"
        out = []
        for v in self.slot(arg):
            if _nonfinite(v):
                out.append(sp.nan)
                continue
            if name in ("sin", "cos", "tan"):
                x = v * sp.pi / 180 if deg else v
                # SymPy maps tan at its poles (odd multiples of pi/2) to zoo.
                out.append({"sin": sp.sin, "cos": sp.cos, "tan": sp.tan}[name](x))
            elif name in ("asin", "acos", "atan"):
                r = {"asin": sp.asin, "acos": sp.acos, "atan": sp.atan}[name](v)
                rs = [r]
                if name in ("asin", "acos") and v.is_number and v.is_real and abs(v) > 1:
                    self.ctx.notes.append(f"{name} on branch cut")
                    rs.append(sp.conjugate(r))
                if deg:
                    rs = [q * 180 / sp.pi for q in rs]
                out += rs
            elif name == "ln":
                out.append(sp.zoo if v == 0 else sp.log(v))
            elif name == "log":
                out.append(sp.zoo if v == 0 else sp.log(v) / sp.log(10))
            elif name == "fact":
                out.append(sp.factorial(v) if (v.is_Integer and v >= 0) else sp.gamma(v + 1))
            else:
                raise Unsupported(name)
        return _dedupe(out)

    def _logb(self, base, arg):
        out = []
        for b in self.slot(base):
            for a in self.slot(arg):
                if a == 0 or b == 0 or b == 1:
                    out.append(sp.zoo)
                else:
                    out.append(sp.log(a) / sp.log(b))
        return _dedupe(out)

    def _const(self, name):
        return [{"pi": sp.pi, "e": sp.E, "i": sp.I}[name]]

    def _var(self, name):
        if name == "x":
            return [X]
        if name in self.ctx.env:
            return [self.ctx.env[name]]
        raise Unsupported(f"unknown variable {name}")

    def _periodic(self, sign, ip, nonrep, rep):
        base = sp.Rational(ip + "." + nonrep if nonrep else ip)
        if rep:
            v = base + sp.Rational(int(rep), (10 ** len(rep) - 1) * 10 ** len(nonrep))
        else:
            v = base
        return [-v if sign == "-" else v]

    def _special(self, label):
        if label in ("∞", "+∞"):
            return [sp.oo]
        if label == "-∞":
            return [-sp.oo]
        return [sp.nan]


@dataclass
class Value:
    kind: str                    # 'finite' | 'nonfinite' | 'incomplete' | 'unsupported'
    candidates: list = field(default_factory=list)   # SymPy values (finite ones first)
    notes: list = field(default_factory=list)
    detail: str = ""

    @property
    def principal(self):
        return self.candidates[0] if self.candidates else None

    def finite(self):
        return [c for c in self.candidates if not _nonfinite(c)]

    def text(self) -> str:
        if self.kind != "finite":
            return self.kind + (f" ({self.detail})" if self.detail else "")
        try:
            return str(sp.nsimplify(self.principal)) if False else str(self.principal)
        except Exception:
            return str(self.principal)


def evaluate(struct, ctx: Ctx) -> Value:
    try:
        vals = Evaluator(ctx).eval(struct)
    except Incomplete as exc:
        return Value("incomplete", detail=str(exc), notes=list(ctx.notes))
    except Unsupported as exc:
        return Value("unsupported", detail=str(exc), notes=list(ctx.notes))
    except (ZeroDivisionError, ValueError, TypeError, OverflowError) as exc:
        return Value("unsupported", detail=f"{type(exc).__name__}: {exc}", notes=list(ctx.notes))
    finite = [v for v in vals if not _nonfinite(v)]
    nonfin = [v for v in vals if _nonfinite(v)]
    if not finite:
        return Value("nonfinite", nonfin, list(dict.fromkeys(ctx.notes)))
    return Value("finite", finite + nonfin, list(dict.fromkeys(ctx.notes)))
