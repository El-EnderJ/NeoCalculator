"""Oracle semantics for the Calculus surface (independent of NumOS and Giac).

* `CEval` extends common.evaluator.Evaluator (not edited) with: real symbols
  x, y; the integration constant C; NodeDefIntegral (`node13`) as a SymPy
  Integral over a bound dummy; Toolbox `diff(f,v)`; result-only calls (sign,
  erf, Si, ...); and a ROOT READING switch:
    - "surd"      VPAM radical = principal REAL n-th root for odd n (the input
                  serializer emits surd(), docs and host harness agree);
    - "principal" x^(1/n) (what a Giac `Root` result node means).
* `GiacText` reads the Giac-bound text / Giac exact text with SymPy under
  Giac's reading (`^` right-assoc above unary minus, log = ln, surd = real
  root, integrate/diff, x/y/C symbols). Shares no code with NumOS.
* Comparison is numeric at fixed rational sample points (mpmath, 40 digits)
  plus an optional SymPy simplification attempt (bounded, recorded only).
"""
from __future__ import annotations

import re

import mpmath
import sympy as sp

from common.evaluator import Ctx, Evaluator, Incomplete, Unsupported, _dedupe, _nonfinite
from common.sexpr import as_row

XR = sp.Symbol("x", real=True)
YR = sp.Symbol("y", real=True)
CS = sp.Symbol("C")
DPS = 40
# Generic sample points: both signs, inside and outside [-1, 1], no
# special values of the generated templates (poles are skipped per point).
SAMPLES = (sp.Rational(-23, 10), sp.Rational(-7, 11), sp.Rational(3, 8), sp.Rational(5, 7),
           sp.Rational(17, 10), sp.Rational(29, 10))
Y_SAMPLE = sp.Rational(13, 7)     # y as a symbolic parameter

RESULT_CALLS = {
    "sign": sp.sign, "erf": sp.erf, "erfc": sp.erfc, "erfi": sp.erfi, "Si": sp.Si, "Ci": sp.Ci,
    "Ei": sp.Ei, "sinh": sp.sinh, "cosh": sp.cosh, "tanh": sp.tanh, "asinh": sp.asinh,
    "acosh": sp.acosh, "atanh": sp.atanh, "floor": sp.floor, "ceil": sp.ceiling,
    "re": sp.re, "im": sp.im, "conj": sp.conjugate, "arg": sp.arg, "Heaviside": sp.Heaviside,
    "Dirac": sp.DiracDelta, "Gamma": sp.gamma, "gamma": sp.gamma, "exp": sp.exp,
    "sec": sp.sec, "csc": sp.csc, "cot": sp.cot, "acot": sp.acot, "ln": sp.log, "log": sp.log,
    "sqrt": sp.sqrt, "abs": sp.Abs, "sin": sp.sin, "cos": sp.cos, "tan": sp.tan,
    "asin": sp.asin, "acos": sp.acos, "atan": sp.atan, "log10": lambda v: sp.log(v) / sp.log(10),
    "fresnelS": sp.fresnels, "fresnelC": sp.fresnelc, "LambertW": sp.LambertW,
}


class CEval(Evaluator):
    def __init__(self, ctx: Ctx, roots: str = "surd", powers: str = "principal"):
        super().__init__(ctx)
        self.roots = roots
        self.powers = powers      # "principal" | "real": (-a)^(p/q), q odd (README policy set)
        self.bound = {}
        self.unevaluated = False

    def power(self, b, e):
        out = super().power(b, e)
        if self.powers == "real" and e.is_Rational and not e.is_Integer and e.q % 2 == 1:
            if b.free_symbols:
                return [sp.sign(b) ** e.p * sp.Abs(b) ** e]
            return list(reversed(out))
        return out

    def _var(self, name):
        if name in self.bound:
            return [self.bound[name]]
        if name == "x":
            return [XR]
        if name == "y":
            return [YR]
        if name == "C":
            return [CS]
        return super()._var(name)

    def _root(self, body, index):
        bodies, idxs = self.slot(body), self.slot(index)
        if self.roots == "surd" and not any(a.free_symbols for a in bodies):
            return Evaluator._root(self, body, index)   # numeric: policy candidate sets
        out = []
        for a in bodies:
            for n in idxs:
                if n == 0:
                    out.append(sp.nan)
                elif self.roots == "surd" and n.is_Integer and n > 0 and n % 2 == 1:
                    out.append(sp.sign(a) * sp.Abs(a) ** sp.Rational(1, n))
                else:
                    out.append(sp.Pow(a, 1 / n))
        return _dedupe(out)

    def _node13(self, lower, upper, body, var):
        vr = as_row(var)[1]
        if len(vr) != 1 or vr[0][0] != "var":
            raise Incomplete("integral variable")
        name = vr[0][1]
        lo = self.slot(lower)[0]
        hi = self.slot(upper)[0]
        t = sp.Dummy(name + "_b", real=True)
        saved = dict(self.bound)
        self.bound[name] = t
        try:
            f = self.slot(body)[0]
        finally:
            self.bound = saved
        if _nonfinite(lo) or _nonfinite(hi):
            return [sp.nan]
        return [sp.Integral(f, (t, lo, hi))]

    def _call(self, name, *args):
        if name == "diff" and len(args) == 2:
            vr = as_row(args[1])[1]
            if len(vr) != 1 or vr[0][0] != "var":
                raise Incomplete("diff variable")
            v = self._var(vr[0][1])[0]
            return [sp.diff(f, v) for f in self.slot(args[0])[:1]]
        if name == "integrate":
            self.unevaluated = True
            vals = [self.slot(a)[0] for a in args]
            if len(vals) == 2:
                return [sp.Integral(vals[0], vals[1])]
            if len(vals) == 4:
                return [sp.Integral(vals[0], (vals[1], vals[2], vals[3]))]
        if name in RESULT_CALLS and len(args) == 1:
            return [RESULT_CALLS[name](v) for v in self.slot(args[0])[:1]]
        if name == "atan2" and len(args) == 2:
            return [sp.atan2(self.slot(args[0])[0], self.slot(args[1])[0])]
        if name == "NTHROOT" and len(args) == 2:
            # Giac's internal name for surd(a, n): NTHROOT(n, a), real root for odd n.
            n, a = self.slot(args[0])[0], self.slot(args[1])[0]
            if n.is_Integer and n % 2 == 1:
                return [sp.sign(a) * sp.Abs(a) ** sp.Rational(1, n)]
            return [sp.Pow(a, 1 / n)]
        raise Unsupported(f"call {name}/{len(args)}")

    def _sym(self, name):
        raise Unsupported(f"symbol {name}")


def evaluate_expr(struct, angle="rad", roots="surd", powers="principal", definite=False):
    """(sympy expr | None, kind, detail, unevaluated).

    definite=True (oracle side) resolves every definite integral whose limits
    are constants: SymPy exact first (divergent -> nonfinite), mpmath quad
    cross-check; an unresolvable or conflicting one makes the case unsupported."""
    ev = CEval(Ctx(angle=angle), roots=roots, powers=powers)
    try:
        vals = ev.eval(struct)
        v = vals[0]
        if definite and v.has(sp.Integral):
            v = resolve_definite(v)
    except Incomplete as exc:
        return None, "incomplete", str(exc), False
    except Unsupported as exc:
        return None, "unsupported", str(exc), False
    except (ZeroDivisionError, ValueError, TypeError, OverflowError, AttributeError,
            RecursionError) as exc:
        return None, "unsupported", f"{type(exc).__name__}: {exc}", False
    pol = "policy:nonfinite-candidate" if len(vals) > 1 and any(_nonfinite(c) for c in vals) else ""
    return v, ("nonfinite" if _nonfinite(v) and not v.free_symbols else "finite"), pol, ev.unevaluated


_DEF_CACHE = {}


def definite_value(body, t, lo, hi):
    """Exact value of a constant definite integral; sp.nan if divergent."""
    key = (sp.srepr(body), sp.srepr(t), sp.srepr(lo), sp.srepr(hi))
    if key in _DEF_CACHE:
        return _DEF_CACHE[key]
    from common.timeout import Overrun, call
    exact = None
    try:
        r = call(lambda: sp.integrate(body, (t, lo, hi)), 8)
        if not r.has(sp.Integral):
            exact = r
    except Overrun:
        exact = None
    except Exception:  # noqa: BLE001
        exact = None
    divergent = exact is not None and (exact.has(sp.oo, -sp.oo, sp.zoo, sp.nan) or
                                       isinstance(exact, sp.AccumBounds))
    q = None
    try:
        q = _quad(body, t, lo, hi, split=True)
    except Exception:  # noqa: BLE001
        q = None
    if divergent:
        val = sp.nan
    elif exact is not None:
        if q is not None:
            ne = complex(sp.N(exact, 30))
            nq = complex(q)
            if abs(ne - nq) > 1e-12 * max(1.0, abs(ne)):
                raise Unsupported(f"oracle conflict: sympy {exact} vs quad {q}")
        val = exact
    elif q is not None:
        val = q
    else:
        raise Unsupported("definite integral: sympy and quad both inconclusive")
    _DEF_CACHE[key] = val
    return val


def resolve_definite(expr):
    def rep(node):
        if isinstance(node, sp.Integral) and len(node.limits) == 1 and len(node.limits[0]) == 3:
            t, lo, hi = node.limits[0]
            if lo.free_symbols or hi.free_symbols or (node.function.free_symbols - {t}):
                return node
            if lo == hi:
                return sp.Integer(0)
            return definite_value(node.function, t, lo, hi)
        return node
    return expr.replace(lambda n: isinstance(n, sp.Integral), rep)


def free_var_order(struct):
    """First x/y in the dump's child order (NumOS CalculusApp::detectAuthoredVariable)
    and first FREE x/y (ignoring an integral's own bound variable)."""
    first_any, first_free = None, None

    def walk(n, bound):
        nonlocal first_any, first_free
        if not isinstance(n, tuple) or not n:
            return
        h = n[0]
        if h == "var" and n[1] in ("x", "y"):
            if first_any is None:
                first_any = n[1]
            if first_free is None and n[1] not in bound:
                first_free = n[1]
            return
        if h == "row":
            for c in n[1]:
                walk(c, bound)
            return
        if h == "node13":
            vr = as_row(n[4])[1]
            bv = vr[0][1] if len(vr) == 1 and vr[0][0] == "var" else None
            walk(n[1], bound)
            walk(n[2], bound)
            walk(n[3], bound | ({bv} if bv else set()))
            walk(n[4], bound | ({bv} if bv else set()))
            return
        for c in n[1:]:
            if isinstance(c, tuple):
                walk(c, bound)

    walk(struct, frozenset())
    return first_any, first_free


# ── numeric evaluation ───────────────────────────────────────────────────────
def _quad(integrand, var, lo, hi, split=True):
    f = sp.lambdify((var,), integrand, modules="mpmath")
    with mpmath.workdps(30):
        a, b = mpmath.mpmathify(sp.N(lo, 35)), mpmath.mpmathify(sp.N(hi, 35))
        pts = [a, b]
        if split:
            # Break at 0 and at the integer points strictly inside: kinks of
            # |.|/sign and the generated poles sit there.
            lo_f, hi_f = sorted([a.real, b.real])
            inner = [mpmath.mpf(k) for k in range(int(mpmath.ceil(lo_f)), int(mpmath.floor(hi_f)) + 1)
                     if lo_f < k < hi_f]
            pts = [a] + (inner if a.real < b.real else list(reversed(inner))) + [b]
        v, err = mpmath.quad(lambda t: mpmath.mpc(f(t)), pts, error=True, maxdegree=8)
        if not mpmath.isfinite(v) or (abs(err) > mpmath.mpf("1e-12") * max(1, abs(v))):
            raise ValueError(f"quad not converged (err {mpmath.nstr(err, 3)})")
        return sp.Float(str(v.real), 30) + sp.I * sp.Float(str(v.imag), 30)


def resolve_integrals(expr):
    """Replace each definite Integral with a numeric (mpmath quad) value once
    its limits are constant; unevaluated indefinite integrals stay."""
    def rep(node):
        if isinstance(node, sp.Integral) and len(node.limits) == 1 and len(node.limits[0]) == 3:
            t, lo, hi = node.limits[0]
            body = node.function
            if lo.free_symbols or hi.free_symbols or (body.free_symbols - {t}):
                return node
            if lo == hi:
                return sp.Integer(0)
            return _quad(body, t, lo, hi)
        return node
    return expr.replace(lambda n: isinstance(n, sp.Integral), rep)


def numeric_at(expr, x0, y0=Y_SAMPLE):
    """Complex mpmath value of expr at x=x0 (y=y0, C=0), None if nonfinite."""
    try:
        v = expr.subs({XR: x0, YR: y0, CS: 0})
        if v.has(sp.Integral):
            try:
                v = resolve_definite(v)
            except Unsupported:
                return "quadfail"
        if v.has(sp.Integral) or v.has(sp.Derivative):
            return "unsupported"
        if _nonfinite(v):
            return None
        with mpmath.workdps(DPS + 10):
            f = sp.lambdify((), v, modules="mpmath")
            try:
                n = mpmath.mpc(f())
            except (TypeError, ValueError, ZeroDivisionError, OverflowError, NameError, AttributeError):
                e = sp.N(v, DPS)
                if e.has(sp.zoo, sp.oo, -sp.oo, sp.nan):
                    return None
                re_, im_ = e.as_real_imag()
                n = mpmath.mpc(mpmath.mpf(str(sp.N(re_, DPS))), mpmath.mpf(str(sp.N(im_, DPS))))
            if not (mpmath.isfinite(n.real) and mpmath.isfinite(n.imag)):
                return None
            return n
    except (ValueError, TypeError, ZeroDivisionError, OverflowError, RecursionError, AttributeError) as exc:
        if "quad not converged" in str(exc):
            return "quadfail"
        return None


def compare(numos_expr, oracle_expr, rel=1e-20, points=SAMPLES):
    """Per-point comparison. Returns dict(agree, differ, skipped, unsupported, detail)."""
    res = {"agree": 0, "differ": 0, "skipped": 0, "unsupported": 0, "points": []}
    for x0 in points:
        a = numeric_at(numos_expr, x0)
        b = numeric_at(oracle_expr, x0)
        if isinstance(a, str) or isinstance(b, str):
            res["unsupported"] += 1
            res["points"].append([str(x0), "unsupported", str(a)[:40], str(b)[:40]])
            continue
        if a is None or b is None:
            res["skipped"] += 1
            res["points"].append([str(x0), "skip", None if a is None else mpmath.nstr(a, 12),
                                  None if b is None else mpmath.nstr(b, 12)])
            continue
        with mpmath.workdps(DPS):
            d = abs(a - b)
            tol = max(mpmath.mpf(rel) * max(abs(a), abs(b)), mpmath.mpf(10) ** -(DPS - 12))
            ok = d <= tol
        res["agree" if ok else "differ"] += 1
        res["points"].append([str(x0), "ok" if ok else "DIFF", mpmath.nstr(a, 15), mpmath.nstr(b, 15)])
    return res


def symbolic_equal(a, b, seconds=3):
    """Bounded SymPy simplification attempt (recorded only, never decisive)."""
    from common.timeout import Overrun, call
    try:
        d = call(lambda: sp.simplify(a - b), seconds)
        return bool(d == 0)
    except Overrun:
        return None
    except Exception:  # noqa: BLE001
        return None


def derivative(expr, var):
    try:
        return sp.diff(expr, var)
    except Exception:  # noqa: BLE001
        return None


# ── Giac text reader ─────────────────────────────────────────────────────────
_TOKEN = re.compile(r"\s*(?:(\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)|([A-Za-z_][A-Za-z_0-9]*)|(.))")


class GiacTextError(ValueError):
    pass


def giac_text_value(text: str, angle: str = "rad"):
    """SymPy value of Giac text (input or exact result), Giac's reading.
    `angle='deg'` reads trig in degrees (authored text in a DEG context)."""
    toks = []
    pos = 0
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        num, ident, sym = m.groups()
        if num is not None:
            toks.append(("num", num))
        elif ident is not None:
            toks.append(("id", ident))
        elif sym is not None and not sym.isspace():
            toks.append(("sym", sym))
    i = 0
    deg = angle == "deg"
    bound = {}

    def peek():
        return toks[i] if i < len(toks) else (None, None)

    def take(kind=None, val=None):
        nonlocal i
        t = peek()
        if t[0] is None or (kind and t[0] != kind) or (val and t[1] != val):
            raise GiacTextError(f"expected {val or kind} at token {i}: {t}")
        i += 1
        return t

    def add():
        v = mul()
        while peek() in (("sym", "+"), ("sym", "-")):
            op = take()[1]
            r = mul()
            v = v + r if op == "+" else v - r
        return v

    def mul():
        v = unary()
        while peek() in (("sym", "*"), ("sym", "/")):
            op = take()[1]
            r = unary()
            v = v * r if op == "*" else (sp.zoo if r == 0 else v / r)
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
            return sp.Pow(b, e)
        return b

    def postfix():
        v = primary()
        while peek() == ("sym", "!"):
            take()
            v = sp.factorial(v)
        return v

    def arglist():
        take("sym", "(")
        out = [add()]
        while peek() == ("sym", ","):
            take()
            out.append(add())
        take("sym", ")")
        return out

    def primary():
        nonlocal i
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
                if name in ("integrate", "diff", "derive"):
                    return special_call(name)
                return call(name, arglist())
            if name in bound:
                return bound[name]
            consts = {"pi": sp.pi, "i": sp.I, "infinity": sp.oo, "inf": sp.oo, "oo": sp.oo, "x": XR, "y": YR,
                      "C": CS, "e": sp.E}
            if name in consts:
                return consts[name]
            if name == "undef":
                return sp.nan
            raise GiacTextError(f"unknown identifier {name}")
        raise GiacTextError(f"unexpected token {t}")

    def special_call(name):
        # integrate(f, v[, a, b]) / diff(f, v): the variable is bound in f.
        nonlocal i
        take("sym", "(")
        start = i
        depth = 0
        # find the variable token after the first top-level comma
        j = i
        while j < len(toks):
            if toks[j] == ("sym", "("):
                depth += 1
            elif toks[j] == ("sym", ")"):
                if depth == 0:
                    break
                depth -= 1
            elif toks[j] == ("sym", ",") and depth == 0:
                break
            j += 1
        k = j + 1
        while k < len(toks) and toks[k] == ("sym", "("):
            k += 1
        if k >= len(toks) or toks[k][0] != "id":
            raise GiacTextError("calculus call variable")
        vname = toks[k][1]
        definite = False
        # look ahead: count top-level args
        depth, n_args, m = 0, 1, start
        while m < len(toks):
            tk = toks[m]
            if tk == ("sym", "("):
                depth += 1
            elif tk == ("sym", ")"):
                if depth == 0:
                    break
                depth -= 1
            elif tk == ("sym", ",") and depth == 0:
                n_args += 1
            m += 1
        definite = name == "integrate" and n_args == 4
        dummy = sp.Dummy(vname + "_g", real=True) if definite else None
        if definite:
            bound[vname] = dummy
        try:
            f = add()
        finally:
            if definite:
                bound.pop(vname, None)
        take("sym", ",")
        v = add()
        rest = []
        while peek() == ("sym", ","):
            take()
            rest.append(add())
        take("sym", ")")
        if name in ("diff", "derive"):
            return sp.diff(f, v)
        if definite:
            return sp.Integral(f, (dummy, rest[0], rest[1]))
        return sp.Integral(f, v)

    def call(name, a):
        if name in ("sin", "cos", "tan") and deg:
            return {"sin": sp.sin, "cos": sp.cos, "tan": sp.tan}[name](a[0] * sp.pi / 180)
        if name in ("asin", "acos", "atan") and deg:
            return {"asin": sp.asin, "acos": sp.acos, "atan": sp.atan}[name](a[0]) * 180 / sp.pi
        if name == "logb":
            return sp.log(a[0]) / sp.log(a[1])
        if name == "surd":
            x, n = a
            if n.is_Integer and n % 2 == 1:
                return sp.sign(x) * sp.Abs(x) ** sp.Rational(1, n)
            return sp.Pow(x, 1 / n)
        if name == "factorial":
            return sp.factorial(a[0])
        if name == "atan2":
            return sp.atan2(a[0], a[1])
        if name == "NTHROOT" and len(a) == 2:
            n, x = a
            if n.is_Integer and n % 2 == 1:
                return sp.sign(x) * sp.Abs(x) ** sp.Rational(1, n)
            return sp.Pow(x, 1 / n)
        if name in RESULT_CALLS and len(a) == 1:
            return RESULT_CALLS[name](a[0])
        raise GiacTextError(f"unknown function {name}/{len(a)}")

    v = add()
    if i != len(toks):
        raise GiacTextError(f"trailing tokens from {i}: {toks[i:i+5]}")
    return v
