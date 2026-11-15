"""Oracle side of the Equations surface.

Truth sources, in order of authority:
  1. the case's CONSTRUCTED solution set (the generator builds equations
     backwards from chosen roots / families, so the truth does not depend on
     any solver);
  2. SymPy `solveset` (Reals or Complexes) on the displayed equation, as a
     second, independent check (and the truth when no construction applies,
     e.g. the naive `)` path builds a different equation);
  3. a numeric scan (mpmath, 60 digits) of lhs-rhs over a real window, to
     detect real roots the constructed/SymPy set would have missed.
Every NumOS solution is substituted into the ORIGINAL displayed equation with
mpmath (60 digits, real-domain tracking in the real policy) - membership never
relies on Giac or on the oracle's own set.
"""
from __future__ import annotations

import mpmath
import sympy as sp

from common.evaluator import Ctx, Evaluator, Incomplete as EvIncomplete, Unsupported, _nonfinite
from common.sexpr import as_row

from .model import Domain, Incomplete, MPEval, residual, split_eq

X, Y, Z = sp.Symbol("x"), sp.Symbol("y"), sp.Symbol("z")
XR = sp.Symbol("x", real=True)
PSYM = {p: sp.Symbol(p) for p in "ABCDEF"}
VARSYM = {"x": X, "y": Y, "z": Z}


class SymEval(Evaluator):
    """common.evaluator semantics, with y, z and parameters as symbols.
    Principal branch only (first candidate)."""

    def _var(self, name):
        if name in VARSYM:
            return [VARSYM[name]]
        if name in PSYM and name not in self.ctx.env:
            return [PSYM[name]]
        return super()._var(name)


def sym_side(struct, angle="rad", env=None):
    ctx = Ctx(angle=angle, env=dict(env or {}))
    try:
        vals = SymEval(ctx).row(as_row(struct))
    except (EvIncomplete, Unsupported) as exc:
        raise Incomplete(str(exc))
    return vals[0]


def sym_equation(eq_struct, angle="rad"):
    parts = split_eq(eq_struct)
    if parts is None:
        raise Incomplete("not exactly one top-level '='")
    l, r = parts
    if not l[1] or not r[1]:
        raise Incomplete("empty side")
    return sym_side(l, angle), sym_side(r, angle), parts


def to_mp(v, dps=60):
    """Exact SymPy constant -> mpmath mpc (None if not finite)."""
    from common.evaluator import numeric
    with mpmath.workdps(dps):
        n = numeric(sp.sympify(v), dps=dps)
    return n


def sympy_solveset(eq_struct, policy, angle="rad", var="x", timeout_hint=True):
    """SymPy's solution set of the displayed single equation in x.
    Returns ("finite", [values]) | ("empty", []) | ("all", excluded) |
    ("periodic", [(offset, period)]) | ("unknown", reason)."""
    try:
        l, r, _ = sym_equation(eq_struct, angle)
    except Incomplete as exc:
        return ("incomplete", str(exc))
    if (l - r).free_symbols - {X}:
        return ("unknown", "parameters")
    dom = sp.S.Reals if policy == "real" else sp.S.Complexes
    try:
        s = sp.solveset(sp.Eq(l, r), X, domain=dom)
    except Exception as exc:  # noqa: BLE001
        return ("unknown", f"solveset: {type(exc).__name__}")
    return classify_set(s)


def classify_set(s):
    if s == sp.S.EmptySet:
        return ("empty", [])
    if isinstance(s, sp.FiniteSet):
        return ("finite", list(s))
    if s in (sp.S.Reals, sp.S.Complexes):
        return ("all", [])
    if isinstance(s, sp.Complement) and s.args[0] in (sp.S.Reals, sp.S.Complexes) and isinstance(s.args[1], sp.FiniteSet):
        return ("all", list(s.args[1]))
    fams = []
    parts = s.args if isinstance(s, sp.Union) else (s,)
    extra = []
    for p in parts:
        if isinstance(p, sp.ImageSet):
            lam = p.lamda
            if len(lam.variables) != 1 or p.base_sets[0] != sp.S.Integers:
                return ("unknown", "imageset shape")
            n = lam.variables[0]
            e = sp.expand(lam.expr)
            per = sp.simplify(e.coeff(n))
            off = sp.simplify(e - per * n)
            if per.free_symbols or off.free_symbols or per == 0:
                return ("unknown", "imageset not affine")
            fams.append((off, abs(per)))
        elif isinstance(p, sp.FiniteSet):
            extra += list(p)
        else:
            return ("unknown", f"set {type(p).__name__}")
    if fams:
        return ("periodic", fams, extra)
    return ("finite", extra)


# ── numeric scan for real roots of lhs-rhs (single variable x) ───────────────
def scan_real_roots(lhs_s, rhs_s, angle, lo, hi, n=1600):
    """Real roots of lhs-rhs in [lo, hi]: sign changes and near-tangencies,
    refined with mpmath.findroot; every refined root is re-verified with the
    residual check (real-domain semantics). Returns sorted mpf list."""
    import math
    xs, fs = [], []
    with mpmath.workdps(30):
        step = (hi - lo) / n
        for i in range(n + 1):
            xv = lo + step * i
            try:
                l = MPEval({"x": xv}, angle, True).row(as_row(lhs_s))
                r = MPEval({"x": xv}, angle, True).row(as_row(rhs_s))
                fv = float((l - r).real)
                if not math.isfinite(fv):
                    fv = None
            except (Domain, Incomplete, ZeroDivisionError, ValueError, OverflowError):
                fv = None
            xs.append(xv)
            fs.append(fv)
    seeds = []
    for i in range(len(xs) - 1):
        a, b = fs[i], fs[i + 1]
        if a is None or b is None:
            continue
        if a == 0:
            seeds.append((xs[i], xs[i]))
        elif a * b < 0:
            seeds.append((xs[i], xs[i + 1]))
    for i in range(1, len(xs) - 1):
        a, b, c = fs[i - 1], fs[i], fs[i + 1]
        if None in (a, b, c):
            continue
        if abs(b) <= abs(a) and abs(b) <= abs(c) and abs(b) < 1e-3 * max(1.0, abs(a), abs(c)):
            seeds.append((xs[i], xs[i]))
    roots = []

    def f(t):
        l = MPEval({"x": t}, angle, True).row(as_row(lhs_s))
        r = MPEval({"x": t}, angle, True).row(as_row(rhs_s))
        return (l - r).real

    with mpmath.workdps(60):
        for a, b in seeds:
            try:
                if a == b:
                    t = mpmath.findroot(f, mpmath.mpf(a), tol=mpmath.mpf(10) ** -45, maxsteps=200)
                else:
                    t = mpmath.findroot(f, (mpmath.mpf(a), mpmath.mpf(b)), solver="anderson",
                                        tol=mpmath.mpf(10) ** -45, maxsteps=200)
            except Exception:  # noqa: BLE001
                try:
                    t = mpmath.findroot(f, mpmath.mpf((a + b) / 2), tol=mpmath.mpf(10) ** -40, maxsteps=400)
                except Exception:  # noqa: BLE001
                    continue
            t = mpmath.mpf(mpmath.re(t))
            ok, _ = residual(lhs_s, rhs_s, {"x": t}, angle, True)
            if not ok:
                # tangential roots converge slowly: accept a looser residual
                with mpmath.workdps(60):
                    try:
                        if abs(f(t)) > mpmath.mpf(10) ** -25:
                            continue
                    except Exception:  # noqa: BLE001
                        continue
            if lo - 1e-9 <= t <= hi + 1e-9 and not any(abs(t - u) < mpmath.mpf(10) ** -12 * max(1, abs(t)) for u in roots):
                roots.append(t)
    return sorted(roots)
