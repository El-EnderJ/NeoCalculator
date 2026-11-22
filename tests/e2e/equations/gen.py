"""Seeded generator of Equations cases with KNOWN solution sets.

Every case is built backwards from a chosen solution set (roots, periodic
families, empty / all-values sets, tuples for systems), so its truth does
not come from any solver. SymPy solveset and a numeric scan are only second
checks (oracle.py / surface.py).

Case: {id, seed, index, family, sub, policy ('real'|'complex'), angle,
       eqs: [(lhs Row, rhs Row)], vars: ['x'] | ['x','y'] | ['x','y','z'],
       truth: {...}}
truth kinds:
  finite   {"sols": [sympy]}               single equation, exact set
  tuples   {"sols": [(sympy, ...)]}        systems
  empty    {}
  all      {"excluded": [sympy]}           identity (minus excluded points)
  dependent {}                             system with infinitely many solutions
  periodic {"fams": [(offset, period)], "extra": [sympy]}  (angle units of the case)
  param    {"sols": [sympy in A..F], "params": [...], "assume_pos": bool}
  cplx_infinite {"real": [sympy]}          complex policy, infinite complex set
                                           (e^x=c): every real solution must
                                           appear; the finite claim is a policy question
  incomplete {}                            expected refusal (parse error)
"""
from __future__ import annotations

import math
from fractions import Fraction

import sympy as sp

from common.expr import Abs, Const, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row, Sqrt, Var, Empty
from common.gen import case_rng

X, Y, Z = Var("x"), Var("y"), Var("z")
SX, SY, SZ = sp.Symbol("x"), sp.Symbol("y"), sp.Symbol("z")

FAMILIES = [
    ("linear", 9), ("quadratic", 11), ("polynomial", 8), ("rational", 9), ("radical", 8),
    ("indexed_root", 2), ("explog", 8), ("trig", 13), ("abs", 6), ("noid", 5), ("param", 3),
    ("system", 11), ("edge", 5), ("decimal", 3), ("incomplete", 2),
]


def R(*items):
    out = []
    for it in items:
        if isinstance(it, (list, tuple)) and not isinstance(it, Row):
            out.extend(it)
        else:
            out.append(it)
    return Row(tuple(out))


def N(n):
    return Num(str(n))


# ── literals and terms ───────────────────────────────────────────────────────
def lit(q, allow_frac=True):
    """Elements typing the rational q (unary minus for negatives)."""
    q = sp.Rational(q)
    neg = q < 0
    a = -q if neg else q
    if a.q == 1:
        body = [N(a.p)]
    elif allow_frac:
        body = [Frac(R(N(a.p)), R(N(a.q)))]
    else:
        raise ValueError("fraction not allowed")
    return ([Op("-")] if neg else []) + body


def starts_digit(items):
    f = items[0]
    while isinstance(f, Pow) and not f.sci:
        f = f.base
    return isinstance(f, Num) or (isinstance(f, Frac) and not f.capture)


def term(c, factor):
    """c * factor as row elements, sign included (first term style)."""
    c = sp.Rational(c)
    if factor is None or factor == []:
        return lit(c)
    neg = c < 0
    a = -c if neg else c
    out = [Op("-")] if neg else []
    if a == 1:
        return out + list(factor)
    coef = lit(a)
    if starts_digit(factor) or isinstance(factor[0], Frac):
        return out + coef + [Op("*")] + list(factor)
    return out + coef + list(factor)


def xpow(k, var=X):
    if k == 0:
        return None
    if k == 1:
        return [var]
    return [Pow(var, R(N(k)))]


def poly_row(coeffs, var=X, rng=None):
    """sum c_k var^k (descending), coeffs: {k: Rational}; zero row -> '0'."""
    items = []
    for k in sorted(coeffs, reverse=True):
        c = sp.Rational(coeffs[k])
        if c == 0:
            continue
        t = term(c, xpow(k, var))
        if items:
            if t[0] == Op("-"):
                items += [Op("-")] + t[1:]
            else:
                items += [Op("+")] + t
        else:
            items += t
    return Row(tuple(items)) if items else R(N(0))


def poly_coeffs(expr, var=SX):
    p = sp.Poly(sp.expand(expr), var)
    return {m[0]: c for m, c in zip(p.monoms(), p.coeffs())}


def small_q(rng, num=6, dens=(1, 1, 1, 2, 3)):
    d = rng.choice(dens)
    n = rng.randint(-num * d, num * d)
    return sp.Rational(n, d)


def nz_int(rng, lo=1, hi=9):
    v = rng.randint(lo, hi)
    return v if rng.random() < 0.5 else -v


def split_sides(rng, coeffs, var=X):
    """Typed equation sum c_k x^k = 0, optionally moving terms to the RHS."""
    ks = [k for k in coeffs if coeffs[k] != 0]
    move = [k for k in ks if rng.random() < 0.3] if len(ks) > 1 else []
    if len(move) == len(ks):
        move = move[:-1]
    lhs = {k: coeffs[k] for k in ks if k not in move}
    rhs = {k: -coeffs[k] for k in move}
    return poly_row(lhs, var), poly_row(rhs, var)


# ── families ────────────────────────────────────────────────────────────────
def g_linear(rng):
    r = small_q(rng)
    form = rng.choice(["axb_cxd", "a_paren", "x_over_a", "frac_coef", "rhs_x"])
    if form == "axb_cxd":
        a, c = nz_int(rng), nz_int(rng)
        while a == c:
            c = nz_int(rng)
        b = rng.randint(-9, 9)
        d = b + (a - c) * r
        lhs = poly_row({1: a, 0: b})
        rhs = poly_row({1: c, 0: d})
    elif form == "a_paren":
        a, p = nz_int(rng), rng.randint(-9, 9)
        q = a * (r + p)
        lhs = R(lit(a) if a != 1 else [], Paren(poly_row({1: 1, 0: p})))
        rhs = R(lit(q))
    elif form == "x_over_a":
        a, b = rng.randint(2, 9), rng.randint(-9, 9)
        c = r / a + b
        lhs = R(Frac(R(X), R(N(a)), capture=True), [Op("+"), N(b)] if b > 0 else ([Op("-"), N(-b)] if b < 0 else []))
        rhs = R(lit(c))
    elif form == "frac_coef":
        p, q = rng.randint(1, 9), rng.randint(2, 9)
        s = sp.Rational(p, q) * r
        lhs = R(Frac(R(N(p)), R(N(q))), X)
        rhs = R(lit(s))
    else:
        a = nz_int(rng)
        b = rng.randint(-9, 9)
        lhs = R(lit(a * r + b))
        rhs = poly_row({1: a, 0: b})
    return form, [(lhs, rhs)], {"kind": "finite", "sols": [r]}


def _roots_set(expr, policy):
    """Exact roots of a univariate polynomial with known factorisation."""
    rs = sp.roots(sp.Poly(sp.expand(expr), SX))
    out = []
    for v in rs:
        v = sp.nsimplify(v) if v.is_Rational else v
        if policy == "real" and not v.is_real:
            continue
        out.append(v)
    return out


def g_quadratic(rng, policy):
    kind = rng.choice(["rational", "rational", "irrational", "double", "negdisc", "factored", "pure"])
    if kind in ("rational", "factored"):
        r1, r2 = small_q(rng, 7, (1, 1, 2)), small_q(rng, 7, (1, 1, 3))
        a = sp.ilcm(r1.q, r2.q) * rng.choice([1, 1, 2, -1, 3])
        expr = a * (SX - r1) * (SX - r2)
        if kind == "factored":
            lhs = R(lit(a) if abs(a) != 1 else ([Op("-")] if a < 0 else []),
                    Paren(poly_row(poly_coeffs(r1.q * (SX - r1)))), Paren(poly_row(poly_coeffs(r2.q * (SX - r2)))))
            k = rng.choice([0, 0, 1])
            if k:
                # move a constant: a(..)(..) = c  <=> roots of a(..)(..) - c with r1,r2 adjusted -> keep 0
                pass
            return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": list({r1, r2})}
        lhs, rhs = split_sides(rng, poly_coeffs(expr))
        return kind, [(lhs, rhs)], {"kind": "finite", "sols": list({r1, r2})}
    if kind == "irrational":
        b = rng.randint(-8, 8)
        while True:
            c = rng.randint(-12, 12)
            disc = b * b - 4 * c
            if disc > 0 and math.isqrt(disc) ** 2 != disc:
                break
        a = rng.choice([1, 1, 2, -1])
        expr = a * (SX ** 2 + b * SX + c)
        sols = [(-b + sp.sqrt(disc)) / 2, (-b - sp.sqrt(disc)) / 2]
        lhs, rhs = split_sides(rng, poly_coeffs(expr))
        return kind, [(lhs, rhs)], {"kind": "finite", "sols": sols}
    if kind == "double":
        r = small_q(rng, 6, (1, 1, 2))
        a = r.q * rng.choice([1, -1, 2])
        expr = a * (SX - r) ** 2
        if rng.random() < 0.4:
            lhs = R(Pow(Paren(poly_row(poly_coeffs(r.q * (SX - r)))), R(N(2))))
            return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": [r]}
        lhs, rhs = split_sides(rng, poly_coeffs(expr))
        return kind, [(lhs, rhs)], {"kind": "finite", "sols": [r]}
    if kind == "negdisc":
        b = rng.randint(-6, 6)
        c = rng.randint(b * b // 4 + 1, b * b // 4 + 12)
        expr = SX ** 2 + b * SX + c
        disc = b * b - 4 * c
        sols = [] if policy == "real" else [(-b + sp.sqrt(disc)) / 2, (-b - sp.sqrt(disc)) / 2]
        lhs, rhs = split_sides(rng, poly_coeffs(expr))
        return kind, [(lhs, rhs)], ({"kind": "empty"} if policy == "real" else {"kind": "finite", "sols": sols})
    # pure: x^2 = k
    k = rng.choice([4, 9, 2, 3, 5, 0, -4, -1, sp.Rational(1, 4), sp.Rational(9, 4)])
    k = sp.Rational(k)
    lhs = R(Pow(X, R(N(2))))
    sols = list({sp.sqrt(k), -sp.sqrt(k)})
    if policy == "real" and k < 0:
        return kind, [(lhs, R(lit(k)))], {"kind": "empty"}
    return kind, [(lhs, R(lit(k)))], {"kind": "finite", "sols": sols}


def g_polynomial(rng, policy):
    kind = rng.choice(["cubic3", "cubic_irr", "cubic_cplx", "biquad", "xn_k", "triple", "double_simple", "factored3"])
    if kind in ("cubic3", "factored3"):
        rs = [sp.Integer(rng.randint(-5, 5)) for _ in range(3)]
        expr = (SX - rs[0]) * (SX - rs[1]) * (SX - rs[2])
        if kind == "factored3":
            lhs = R(*[Paren(poly_row({1: 1, 0: -r})) for r in rs])
            return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": list(set(rs))}
    elif kind == "cubic_irr":
        r, k = sp.Integer(rng.randint(-4, 4)), rng.choice([2, 3, 5, 6, 7])
        expr = (SX - r) * (SX ** 2 - k)
        rs = [r, sp.sqrt(k), -sp.sqrt(k)]
    elif kind == "cubic_cplx":
        r, k = sp.Integer(rng.randint(-4, 4)), rng.choice([1, 2, 4, 3])
        expr = (SX - r) * (SX ** 2 + k)
        rs = [r] + ([] if policy == "real" else [sp.I * sp.sqrt(k), -sp.I * sp.sqrt(k)])
    elif kind == "biquad":
        a, b = rng.sample([1, 4, 9, 2, 3, -1, -4], 2)
        expr = (SX ** 2 - a) * (SX ** 2 - b)
        rs = []
        for v in (a, b):
            if v > 0 or policy == "complex":
                rs += [sp.sqrt(v), -sp.sqrt(v)]
    elif kind == "xn_k":
        n = rng.choice([3, 4, 5])
        base = rng.choice([1, 2, 3])
        k = base ** n * rng.choice([1, -1])
        lhs = R(Pow(X, R(N(n))))
        allr = list(sp.roots(SX ** n - k, SX))
        rs = [v for v in allr if policy == "complex" or v.is_real]
        return kind, [(lhs, R(lit(k)))], {"kind": "finite", "sols": rs}
    elif kind == "triple":
        r = sp.Integer(rng.randint(-4, 4))
        expr = (SX - r) ** 3
        rs = [r]
    else:
        r1, r2 = sp.Integer(rng.randint(-4, 4)), sp.Integer(rng.randint(-4, 4))
        expr = (SX - r1) ** 2 * (SX - r2)
        rs = list({r1, r2})
    lhs, rhs = split_sides(rng, poly_coeffs(expr))
    return kind, [(lhs, rhs)], {"kind": "finite", "sols": list(dict.fromkeys(rs))}


def g_rational(rng, policy):
    kind = rng.choice(["p_over_x_a", "hole", "excluded_empty", "mobius", "a_over_x_eq_x", "identity_hole", "two_fracs"])
    if kind == "p_over_x_a":
        a, p, q = rng.randint(-6, 6), nz_int(rng), nz_int(rng)
        lhs = R(Frac(R(N(abs(p))), poly_row({1: 1, 0: -a})))
        if p < 0:
            lhs = R(Op("-"), *lhs.items)
        return kind, [(lhs, R(lit(q)))], {"kind": "finite", "sols": [a + sp.Rational(p, q)]}
    if kind == "hole":
        a = nz_int(rng, 1, 6)
        lhs = R(Frac(poly_row({2: 1, 0: -a * a}), poly_row({1: 1, 0: -a})))
        return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": [sp.Integer(-a)]}
    if kind == "excluded_empty":
        a = nz_int(rng, 1, 6)
        lhs = R(Frac(R(X), poly_row({1: 1, 0: -a})))
        rhs = R(Frac(R(lit(a)), poly_row({1: 1, 0: -a}))) if a > 0 else R(Op("-"), Frac(R(N(-a)), poly_row({1: 1, 0: -a})))
        return kind, [(lhs, rhs)], {"kind": "empty"}
    if kind == "mobius":
        a, b = rng.randint(-6, 6), rng.randint(-6, 6)
        c = rng.choice([2, 3, -1, -2, sp.Rational(1, 2)])
        x0 = (a - c * b) / (c - 1)
        if x0 == -b:
            return g_rational(rng, policy)
        lhs = R(Frac(poly_row({1: 1, 0: a}), poly_row({1: 1, 0: b})))
        return kind, [(lhs, R(lit(c)))], {"kind": "finite", "sols": [sp.Rational(x0)]}
    if kind == "a_over_x_eq_x":
        a = rng.choice([4, 9, 2, 3, -4, 1])
        lhs = R(Frac(R(N(abs(a))), R(X)))
        if a < 0:
            lhs = R(Op("-"), *lhs.items)
        sols = [sp.sqrt(a), -sp.sqrt(a)]
        if policy == "real" and a < 0:
            return kind, [(lhs, R(X))], {"kind": "empty"}
        return kind, [(lhs, R(X))], {"kind": "finite", "sols": sols}
    if kind == "identity_hole":
        a = nz_int(rng, 1, 5)
        lhs = R(Frac(poly_row({2: 1, 0: -a * a}), poly_row({1: 1, 0: -a})))
        rhs = poly_row({1: 1, 0: a})
        return kind, [(lhs, rhs)], {"kind": "all", "excluded": [sp.Integer(a)]}
    # 1/(x-a) + 1/(x-b) = 0 -> x = (a+b)/2 if a != b
    a, b = rng.sample(range(-5, 6), 2)
    lhs = R(Frac(R(N(1)), poly_row({1: 1, 0: -a})), Op("+"), Frac(R(N(1)), poly_row({1: 1, 0: -b})))
    return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": [sp.Rational(a + b, 2)]}


def g_radical(rng, policy):
    kind = rng.choice(["sqrt_lin_extraneous", "sqrt_lin_extraneous", "sqrt_const", "sqrt_neg_const", "sqrt_ax_eq_x", "two_sqrt"])
    if kind == "sqrt_lin_extraneous":
        # sqrt(x+a) = x+b with roots r (r+b>=0) and s; s rejected iff s+b<0
        while True:
            r, s = rng.randint(-6, 8), rng.randint(-8, 8)
            if (r + s) % 2 == 1:
                break
        b = (1 - r - s) // 2
        a = b * b - r * s
        sols = [sp.Integer(v) for v in (r, s) if v + b >= 0]
        lhs = R(Sqrt(poly_row({1: 1, 0: a})))
        rhs = poly_row({1: 1, 0: b})
        if rng.random() < 0.3:
            lhs, rhs = rhs, lhs
        return kind, [(lhs, rhs)], ({"kind": "finite", "sols": list(set(sols))} if sols else {"kind": "empty"})
    if kind == "sqrt_const":
        a, c = rng.randint(-9, 9), rng.randint(0, 6)
        return kind, [(R(Sqrt(poly_row({1: 1, 0: a}))), R(N(c)))], {"kind": "finite", "sols": [sp.Integer(c * c - a)]}
    if kind == "sqrt_neg_const":
        a, c = rng.randint(-9, 9), rng.randint(1, 6)
        # principal sqrt is never negative (real or complex principal branch)
        return kind, [(R(Sqrt(poly_row({1: 1, 0: a}))), R(Op("-"), N(c)))], {"kind": "empty"}
    if kind == "sqrt_ax_eq_x":
        a = rng.randint(1, 9)
        return kind, [(R(Sqrt(R(N(a), X))), R(X))], {"kind": "finite", "sols": [sp.Integer(0), sp.Integer(a)]}
    # sqrt(x+a) = sqrt(b - x) -> x = (b-a)/2 when admissible
    a, b = rng.randint(-6, 6), rng.randint(-6, 6)
    x0 = sp.Rational(b - a, 2)
    ok = x0 + a >= 0
    lhs = R(Sqrt(poly_row({1: 1, 0: a})))
    rhs = R(Sqrt(poly_row({1: -1, 0: b})))
    # principal sqrt is injective on C: in the complex policy x0 always solves
    return kind, [(lhs, rhs)], ({"kind": "finite", "sols": [x0]} if (ok or policy == "complex") else {"kind": "empty"})


def g_indexed_root(rng, policy):
    kind = rng.choice(["cbrt_const", "pow_third", "root4_const"])
    if kind == "cbrt_const":
        c = rng.randint(-4, 4)
        a = rng.randint(-5, 5)
        body = poly_row({1: 1, 0: a})
        # real cube root (odd index): x + a = c^3 ; complex policy: principal
        # surd is the real cube root in Giac/NumOS semantics -> same answer
        return kind, [(R(Root(R(N(3)), body)), R(lit(c)))], {"kind": "finite", "sols": [sp.Integer(c ** 3 - a)]}
    if kind == "pow_third":
        c = rng.randint(1, 4)
        return kind, [(R(Pow(X, R(Frac(R(N(1)), R(N(3)))))), R(N(c)))], {"kind": "finite", "sols": [sp.Integer(c ** 3)]}
    c = rng.randint(1, 3)
    return kind, [(R(Root(R(N(4)), R(X))), R(N(c)))], {"kind": "finite", "sols": [sp.Integer(c ** 4)]}


def g_explog(rng, policy):
    kind = rng.choice(["ex_c", "ex_nonpos", "pow2", "a_b_pow", "pow10", "ln_k", "log_k", "ln_shift", "logb_k",
                       "ln_sum_extraneous", "exp_quadratic", "ln_sq"])
    cinf = policy == "complex"
    if kind == "ex_c":
        c = rng.choice([1, 2, 3, 5, sp.Rational(1, 2), 7])
        c = sp.Rational(c)
        lhs = R(Pow(Const("e"), R(X)))
        sol = [sp.log(c)]
        return kind, [(lhs, R(lit(c)))], ({"kind": "cplx_infinite", "real": sol} if cinf else {"kind": "finite", "sols": sol})
    if kind == "ex_nonpos":
        c = rng.choice([0, -1, -2, -5])
        lhs = R(Pow(Const("e"), R(X)))
        if cinf and c < 0:
            return kind, [(lhs, R(lit(c)))], {"kind": "cplx_infinite", "real": []}
        return kind, [(lhs, R(lit(c)))], {"kind": "empty"}
    if kind == "pow2":
        b = rng.choice([2, 3, 5])
        k = rng.randint(-3, 5)
        lhs = R(Pow(N(b), R(X)))
        rhs = R(lit(sp.Rational(b) ** k))
        return kind, [(lhs, rhs)], ({"kind": "cplx_infinite", "real": [sp.Integer(k)]} if cinf else {"kind": "finite", "sols": [sp.Integer(k)]})
    if kind == "a_b_pow":
        a, b, p, k = rng.choice([2, 3, 5]), rng.choice([2, 3]), rng.randint(-3, 3), rng.randint(0, 4)
        lhs = R(N(a), Op("*"), Pow(N(b), poly_row({1: 1, 0: p})))
        rhs = R(N(a * b ** k))
        sol = [sp.Integer(k - p)]
        return kind, [(lhs, rhs)], ({"kind": "cplx_infinite", "real": sol} if cinf else {"kind": "finite", "sols": sol})
    if kind == "pow10":
        c = rng.choice([1000, 100, 10, 1, 2, 50])
        sol = [sp.log(c) / sp.log(10)]
        lhs = R(Pow(N(10), R(X)))
        return kind, [(lhs, R(N(c)))], ({"kind": "cplx_infinite", "real": sol} if cinf else {"kind": "finite", "sols": sol})
    if kind == "ln_k":
        k = small_q(rng, 3, (1, 1, 2))
        return kind, [(R(Fn("ln", R(X))), R(lit(k)))], {"kind": "finite", "sols": [sp.exp(k)]}
    if kind == "log_k":
        k = rng.randint(-2, 3)
        return kind, [(R(Fn("log", R(X))), R(lit(k)))], {"kind": "finite", "sols": [sp.Rational(10) ** k]}
    if kind == "ln_shift":
        a, b = rng.randint(-5, 5), rng.randint(1, 9)
        return kind, [(R(Fn("ln", poly_row({1: 1, 0: a}))), R(Fn("ln", R(N(b)))))], {"kind": "finite", "sols": [sp.Integer(b - a)]}
    if kind == "logb_k":
        b, k = rng.choice([2, 3, 5]), rng.randint(-2, 4)
        return kind, [(R(LogB(R(N(b)), R(X))), R(lit(k)))], {"kind": "finite", "sols": [sp.Rational(b) ** k]}
    if kind == "ln_sum_extraneous":
        # ln x + ln(x - a) = ln(r(r-a)), r > a > 0 ; other root a - r < 0 rejected
        a = rng.randint(1, 4)
        r = rng.randint(a + 1, a + 4)
        lhs = R(Fn("ln", R(X)), Op("+"), Fn("ln", poly_row({1: 1, 0: -a})))
        rhs = R(Fn("ln", R(N(r * (r - a)))))
        if policy == "complex":
            # principal-branch reading: ln(a-r) + ln(-r) = ln(r(r-a)) + 2πi  -> still rejected
            pass
        return kind, [(lhs, rhs)], {"kind": "finite", "sols": [sp.Integer(r)]}
    if kind == "exp_quadratic":
        a, b = rng.sample([1, 2, 3, 4], 2)
        u = Pow(Const("e"), R(X))
        lhs = R(Pow(Const("e"), R(N(2), X)), Op("-"), N(a + b), u, Op("+"), N(a * b))
        sol = [sp.log(a), sp.log(b)]
        return kind, [(lhs, R(N(0)))], ({"kind": "cplx_infinite", "real": sol} if cinf else {"kind": "finite", "sols": sol})
    # ln(x^2) = ln(k^2): x = ±k  (x^2 > 0)
    k = rng.randint(1, 5)
    return kind, [(R(Fn("ln", R(Pow(X, R(N(2)))))), R(Fn("ln", R(N(k * k)))))], {"kind": "finite", "sols": [sp.Integer(k), sp.Integer(-k)]}


SIN_VALS = [(0, 0), (sp.Rational(1, 2), 1), (-sp.Rational(1, 2), 1), (1, 0), (-1, 0),
            (sp.sqrt(2) / 2, 2), (sp.sqrt(3) / 2, 2), (-sp.sqrt(3) / 2, 2), (sp.Rational(1, 3), 3)]
TAN_VALS = [0, 1, -1, sp.sqrt(3), -sp.sqrt(3), sp.sqrt(3) / 3, 2]


def value_row(v):
    """Typed form of a special trig value."""
    v = sp.nsimplify(v)
    if v.is_Rational:
        return R(lit(v))
    neg = bool(v.could_extract_minus_sign())
    a = -v if neg else v
    pre = [Op("-")] if neg else []
    if a == sp.sqrt(2) / 2:
        return R(pre, Frac(R(Sqrt(R(N(2)))), R(N(2))))
    if a == sp.sqrt(3) / 2:
        return R(pre, Frac(R(Sqrt(R(N(3)))), R(N(2))))
    if a == sp.sqrt(3):
        return R(pre, Sqrt(R(N(3))))
    if a == sp.sqrt(3) / 3:
        return R(pre, Frac(R(Sqrt(R(N(3)))), R(N(3))))
    raise ValueError(v)


def g_trig(rng, policy, angle):
    fn = rng.choice(["sin", "sin", "cos", "cos", "tan"])
    full = sp.Integer(360) if angle == "deg" else 2 * sp.pi
    half = full / 2
    to_unit = (lambda t: t * 180 / sp.pi) if angle == "deg" else (lambda t: t)
    form = rng.choice(["plain", "plain", "linear_arg", "scaled", "outside", "power"])
    if fn == "tan":
        c = sp.nsimplify(rng.choice(TAN_VALS))
        base = [to_unit(sp.atan(c))]
        per_u = half
    else:
        if rng.random() < 0.12:
            c = sp.Rational(rng.choice([sp.Rational(3, 2), -2, sp.Rational(5, 4)]))
            base, per_u = None, full
        else:
            c = sp.nsimplify(rng.choice(SIN_VALS)[0])
            if fn == "sin":
                t = sp.asin(c)
                base = [to_unit(t), to_unit(sp.pi - t)]
            else:
                t = sp.acos(c)
                base = [to_unit(t), to_unit(-t)]
            per_u = full
    # argument a*x + b (u = a x + b)
    if form == "linear_arg":
        a = rng.choice([2, 3, sp.Rational(1, 2)])
        b = rng.choice([0, 1, -1]) * (sp.Integer(30) if angle == "deg" else sp.pi / 6)
        a = sp.Rational(a)
    else:
        a, b = sp.Integer(1), sp.Integer(0)
    arg_items = []
    if a != 1:
        arg_items += ([Frac(R(N(a.p)), R(N(a.q)))] if a.q != 1 else [N(a.p)])
    arg_items.append(X)
    if b != 0:
        if angle == "deg":
            arg_items += [Op("+") if b > 0 else Op("-"), N(abs(int(b)))]
        else:
            arg_items += [Op("+") if b > 0 else Op("-"), Frac(R(Const("pi")), R(N(6)))]
    f_atom = Fn(fn, Row(tuple(arg_items)))
    if form == "power" and fn != "tan" and c != 0:
        # f(x)^2 = c^2  ->  f(x) = ±c
        lhs = R(Pow(f_atom, R(N(2))))
        rhs = R(lit(c * c)) if (c * c).is_Rational else value_row(sp.nsimplify(c * c))
        if base is None:
            fams = None
        else:
            if fn == "sin":
                t = sp.asin(c)
                base = [to_unit(t), to_unit(sp.pi - t), to_unit(-t), to_unit(sp.pi + t)]
            else:
                t = sp.acos(c)
                base = [to_unit(t), to_unit(-t), to_unit(sp.pi - t), to_unit(t - sp.pi)]
    elif form == "scaled" and c != 0:
        k = rng.choice([2, 3])
        lhs = R(N(k), f_atom)
        cv = sp.nsimplify(k * c)
        rhs = R(lit(cv)) if cv.is_Rational else _scaled_value_row(k, c)
    elif form == "outside":
        # f(x) - c = 0 with rational c only
        if not sp.nsimplify(c).is_Rational:
            lhs, rhs = R(f_atom), value_row(c)
        else:
            lhs = R(f_atom, [Op("-")] + lit(-c)[1:] if c > 0 else ([Op("+")] + lit(-c) if c < 0 else []))
            rhs = R(N(0))
    else:
        lhs = R(f_atom)
        rhs = value_row(c)
    if base is None:
        truth = {"kind": "empty"}
    else:
        fams = []
        for o in base:
            off = (o - b) / a
            per = per_u / abs(a)
            fams.append((sp.nsimplify(off), sp.nsimplify(per)))
        truth = {"kind": "periodic", "fams": fams, "extra": []}
    if policy == "complex":
        if fn != "tan" and base is None:
            truth = {"kind": "cplx_infinite", "real": []}   # sin z = 3/2 has complex roots
        elif truth["kind"] == "periodic":
            truth = dict(truth, complex_policy=True)
    return f"{fn}_{form}", [(lhs, rhs)], truth


def _scaled_value_row(k, c):
    c = sp.nsimplify(c)
    neg = c < 0
    a = -c if neg else c
    pre = [Op("-")] if neg else []
    # k * a where a = sqrt(m)/n
    num = sp.nsimplify(k * a)
    if num == sp.sqrt(2):
        return R(pre, Sqrt(R(N(2))))
    if num == sp.sqrt(3):
        return R(pre, Sqrt(R(N(3))))
    if num == 3 * sp.sqrt(3) / 2:
        return R(pre, Frac(R(N(3), Sqrt(R(N(3)))), R(N(2))))
    if num == 3 * sp.sqrt(2) / 2:
        return R(pre, Frac(R(N(3), Sqrt(R(N(2)))), R(N(2))))
    return R(pre, N(k), Op("*"), *value_row(a).items)


def g_abs(rng, policy):
    kind = rng.choice(["xa_b", "xa_b", "xa_zero", "xa_neg", "lin_extraneous", "abs_abs"])
    if kind in ("xa_b", "xa_zero", "xa_neg"):
        a = rng.randint(-6, 6)
        b = {"xa_b": rng.randint(1, 7), "xa_zero": 0, "xa_neg": -rng.randint(1, 5)}[kind]
        lhs = R(Abs(poly_row({1: 1, 0: -a})))
        if b < 0:
            return kind, [(lhs, R(lit(b)))], {"kind": "empty"}
        return kind, [(lhs, R(lit(b)))], {"kind": "finite", "sols": list({sp.Integer(a - b), sp.Integer(a + b)})}
    if kind == "lin_extraneous":
        # |x - a| = c x + d : candidates from both signs, keep c x + d >= 0
        a, c, d = rng.randint(-5, 5), rng.choice([2, 3, -2, sp.Rational(1, 2)]), rng.randint(-6, 6)
        c = sp.Rational(c)
        sols = set()
        for s in (1, -1):
            # s(x-a) = c x + d -> x (s - c) = d + s a
            if s - c != 0:
                xv = (d + s * a) / (s - c)
                if s * (xv - a) >= 0 and c * xv + d >= 0:
                    sols.add(sp.Rational(xv))
        lhs = R(Abs(poly_row({1: 1, 0: -a})))
        rhs = poly_row({1: c, 0: d})
        return kind, [(lhs, rhs)], ({"kind": "finite", "sols": list(sols)} if sols else {"kind": "empty"})
    a, b = rng.sample(range(-6, 7), 2)
    lhs = R(Abs(poly_row({1: 1, 0: -a})))
    rhs = R(Abs(poly_row({1: 1, 0: -b})))
    return kind, [(lhs, rhs)], {"kind": "finite", "sols": [sp.Rational(a + b, 2)]}


def g_noid(rng, policy):
    kind = rng.choice(["identity", "contradiction", "identity_expand", "identity_x_over_x"])
    a, b = nz_int(rng), rng.randint(-9, 9)
    if kind == "identity":
        return kind, [(poly_row({1: a, 0: b}), poly_row({0: b, 1: a}))], {"kind": "all", "excluded": []}
    if kind == "contradiction":
        d = b + rng.choice([1, 2, -3, 5])
        return kind, [(poly_row({1: a, 0: b}), poly_row({1: a, 0: d}))], {"kind": "empty"}
    if kind == "identity_expand":
        # a(x + b) = a x + a b
        lhs = R(lit(a) if a != 1 else [], Paren(poly_row({1: 1, 0: b})))
        rhs = poly_row({1: a, 0: a * b})
        return kind, [(lhs, rhs)], {"kind": "all", "excluded": []}
    lhs = R(Frac(R(X), R(X)))
    return kind, [(lhs, R(N(1)))], {"kind": "all", "excluded": [sp.Integer(0)]}


def g_param(rng, policy):
    kind = rng.choice(["Ax_B", "Ax_eq_B", "x2_A", "x_plus_A"])
    A, B = Var("A"), Var("B")
    SA, SB = sp.Symbol("A"), sp.Symbol("B")
    if kind == "Ax_B":
        return kind, [(R(A, X, Op("+"), B), R(N(0)))], {"kind": "param", "sols": [-SB / SA], "params": ["A", "B"]}
    if kind == "Ax_eq_B":
        c = rng.randint(1, 9)
        return kind, [(R(A, X), R(N(c)))], {"kind": "param", "sols": [sp.Integer(c) / SA], "params": ["A"]}
    if kind == "x2_A":
        return kind, [(R(Pow(X, R(N(2)))), R(A))], {"kind": "param", "sols": [sp.sqrt(SA), -sp.sqrt(SA)], "params": ["A"],
                                                     "assume_pos": True}
    c = rng.randint(-9, 9)
    return kind, [(R(X, Op("+"), A), R(lit(c)))], {"kind": "param", "sols": [c - SA], "params": ["A"]}


def g_system(rng, policy):
    kind = rng.choice(["lin2", "lin2", "lin2_incons", "lin2_dep", "lin3", "lin3", "lin3_incons", "lin3_dep",
                       "nl_sum_prod", "nl_circle_line", "nl_parabola_line"])
    if kind.startswith("lin2"):
        sol = (small_q(rng, 5, (1, 1, 2)), small_q(rng, 5, (1, 1, 2)))
        while True:
            m = [[rng.randint(-5, 5) for _ in range(2)] for _ in range(2)]
            if kind == "lin2" and m[0][0] * m[1][1] - m[0][1] * m[1][0] != 0:
                break
            if kind != "lin2" and any(m[0]):
                k = rng.choice([2, -1, 3, sp.Rational(1, 2)])
                m[1] = [k * m[0][0], k * m[0][1]]
                break
        rhs = [m[i][0] * sol[0] + m[i][1] * sol[1] for i in range(2)]
        if kind == "lin2_incons":
            rhs[1] += rng.choice([1, -2, 3])
        eqs = [(lin_row(m[i], ("x", "y")), R(lit(rhs[i]))) for i in range(2)]
        truth = {"kind": "tuples", "sols": [sol]} if kind == "lin2" else ({"kind": "empty"} if kind == "lin2_incons" else {"kind": "dependent"})
        return kind, eqs, truth
    if kind.startswith("lin3"):
        sol = tuple(sp.Integer(rng.randint(-4, 4)) for _ in range(3))
        while True:
            m = [[rng.randint(-3, 3) for _ in range(3)] for _ in range(3)]
            det = sp.Matrix(m).det()
            if kind == "lin3" and det != 0:
                break
            if kind != "lin3" and any(m[0]) and any(m[1]):
                a, b = rng.choice([1, -1, 2]), rng.choice([1, -1, 2])
                m[2] = [a * m[0][j] + b * m[1][j] for j in range(3)]
                if sp.Matrix(m[:2]).rank() == 2:
                    break
        rhs = [sum(m[i][j] * sol[j] for j in range(3)) for i in range(3)]
        if kind == "lin3_incons":
            rhs[2] += rng.choice([1, -1, 2])
        eqs = [(lin_row(m[i], ("x", "y", "z")), R(lit(rhs[i]))) for i in range(3)]
        truth = {"kind": "tuples", "sols": [sol]} if kind == "lin3" else ({"kind": "empty"} if kind == "lin3_incons" else {"kind": "dependent"})
        return kind, eqs, truth
    if kind == "nl_sum_prod":
        r1, r2 = rng.randint(-4, 5), rng.randint(-4, 5)
        eqs = [(R(X, Op("+"), Y), R(lit(r1 + r2))), (R(X, Y), R(lit(r1 * r2)))]
        sols = list({(sp.Integer(r1), sp.Integer(r2)), (sp.Integer(r2), sp.Integer(r1))})
        return kind, eqs, {"kind": "tuples", "sols": sols}
    if kind == "nl_circle_line":
        # x^2 + y^2 = r^2, y = x + c  (c chosen so roots are nice: c = 0, r^2 = 2k^2)
        k = rng.randint(1, 4)
        eqs = [(R(Pow(X, R(N(2))), Op("+"), Pow(Y, R(N(2)))), R(N(2 * k * k))), (R(Y), R(X))]
        sols = [(sp.Integer(k), sp.Integer(k)), (sp.Integer(-k), sp.Integer(-k))]
        return kind, eqs, {"kind": "tuples", "sols": sols}
    # y = x^2, y = k x  -> (0,0), (k, k^2)
    k = nz_int(rng, 1, 4)
    eqs = [(R(Y), R(Pow(X, R(N(2))))), (R(Y), poly_row({1: k}))]
    return kind, eqs, {"kind": "tuples", "sols": [(sp.Integer(0), sp.Integer(0)), (sp.Integer(k), sp.Integer(k * k))]}


def lin_row(coefs, names):
    items = []
    for c, n in zip(coefs, names):
        c = sp.Rational(c)
        if c == 0:
            continue
        t = term(c, [Var(n)])
        if items:
            items += ([Op("-")] + t[1:]) if t[0] == Op("-") else ([Op("+")] + t)
        else:
            items += t
    return Row(tuple(items)) if items else R(N(0))


def g_edge(rng, policy):
    kind = rng.choice(["huge_int", "tiny_frac", "huge_root", "high_mult", "near_degenerate", "big_quadratic", "sci"])
    if kind == "huge_int":
        a, b = rng.randint(10 ** 8, 10 ** 12), rng.randint(10 ** 8, 10 ** 15)
        return kind, [(R(N(a), X), R(N(b)))], {"kind": "finite", "sols": [sp.Rational(b, a)]}
    if kind == "tiny_frac":
        e = rng.randint(4, 12)
        c = rng.randint(1, 9)
        return kind, [(R(Frac(R(X), R(N(10 ** e)), capture=True)), R(N(c)))], {"kind": "finite", "sols": [sp.Integer(c * 10 ** e)]}
    if kind == "huge_root":
        r = sp.Integer(rng.randint(10 ** 6, 10 ** 9))
        return kind, [(R(Pow(X, R(N(2)))), R(N(r * r)))], {"kind": "finite", "sols": [r, -r]}
    if kind == "high_mult":
        r = sp.Integer(rng.randint(-2, 2))
        n = rng.choice([4, 5, 6])
        lhs = R(Pow(Paren(poly_row({1: 1, 0: -r})), R(N(n))))
        return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": [r]}
    if kind == "near_degenerate":
        e = rng.choice([6, 8, 10])
        # (x-1)^2 = 1/10^(2e)  -> x = 1 ± 10^-e
        lhs = R(Pow(Paren(poly_row({1: 1, 0: -1})), R(N(2))))
        rhs = R(Frac(R(N(1)), R(N(10 ** (2 * e)))))
        return kind, [(lhs, rhs)], {"kind": "finite", "sols": [1 + sp.Rational(1, 10 ** e), 1 - sp.Rational(1, 10 ** e)]}
    if kind == "big_quadratic":
        r1, r2 = rng.randint(1000, 99999), -rng.randint(1000, 99999)
        expr = (SX - r1) * (SX - r2)
        lhs, rhs = split_sides(rng, poly_coeffs(expr))
        return kind, [(lhs, rhs)], {"kind": "finite", "sols": [sp.Integer(r1), sp.Integer(r2)]}
    # x = 3 ×10^k via EXP key  (Pow sci after a number mantissa)
    m, k = rng.randint(1, 9), rng.choice([12, 20, -7, 30])
    rhs = R(N(m), Pow(N(10), R(lit(k)), sci=True))
    return kind, [(R(N(2), X), rhs)], {"kind": "finite", "sols": [sp.Rational(m) * sp.Rational(10) ** k / 2]}


def g_decimal(rng, policy):
    kind = rng.choice(["lin", "quad"])
    if kind == "lin":
        a = sp.Rational(rng.randint(1, 99), 10)
        b = sp.Rational(rng.randint(-999, 999), 100)
        c = sp.Rational(rng.randint(-99, 99), 10)
        lhs = R(Num(_dec(a)), X, Op("+") if b >= 0 else Op("-"), Num(_dec(abs(b))))
        return kind, [(lhs, R(lit_dec(c)))], {"kind": "finite", "sols": [(c - b) / a]}
    # x^2 - s x + p with decimal s, p: roots exact via quadratic formula
    s = sp.Rational(rng.randint(-50, 50), 10)
    p = sp.Rational(rng.randint(-99, 99), 10)
    lhs = R(Pow(X, R(N(2))), Op("-") if s >= 0 else Op("+"), Num(_dec(abs(s))), X,
            Op("+") if p >= 0 else Op("-"), Num(_dec(abs(p))))
    disc = s * s - 4 * p
    if disc < 0:
        if policy == "real":
            return kind, [(lhs, R(N(0)))], {"kind": "empty"}
        sols = [(s + sp.sqrt(disc)) / 2, (s - sp.sqrt(disc)) / 2]
    else:
        sols = list({(s + sp.sqrt(disc)) / 2, (s - sp.sqrt(disc)) / 2})
    return kind, [(lhs, R(N(0)))], {"kind": "finite", "sols": sols}


def _dec(q):
    q = sp.Rational(q)
    s = format(float(q), "f").rstrip("0")
    s = s[:-1] if s.endswith(".") else s
    assert sp.Rational(s) == q, (s, q)
    return s


def lit_dec(q):
    q = sp.Rational(q)
    return ([Op("-")] if q < 0 else []) + [Num(_dec(abs(q)))]


def g_incomplete(rng, policy):
    kind = rng.choice(["empty_lhs", "empty_rhs", "dangling_op", "empty_exponent", "two_eq"])
    if kind == "empty_lhs":
        return kind, [(R(), R(N(5)))], {"kind": "incomplete"}
    if kind == "empty_rhs":
        return kind, [(R(N(2), X), R())], {"kind": "incomplete"}
    if kind == "dangling_op":
        return kind, [(R(X, Op("+")), R(N(3)))], {"kind": "incomplete"}
    if kind == "empty_exponent":
        return kind, [(R(Pow(X, R(Empty()))), R(N(4)))], {"kind": "incomplete"}
    return kind, [(R(X), R(N(2), Op("="), N(3)))], {"kind": "incomplete"}


def generate(seed, index):
    rng = case_rng(seed, index)
    fam = rng.choices([f for f, _ in FAMILIES], weights=[w for _, w in FAMILIES])[0]
    if fam == "trig":
        policy = "real" if rng.random() < 0.75 else "complex"
        angle = "deg" if rng.random() < 0.4 else "rad"
    elif fam == "explog":
        policy = "real" if rng.random() < 0.8 else "complex"
        angle = "rad"
    elif fam == "abs":
        # |z-a| = b is a circle over C: the complex reading is not a calculator policy
        policy, angle = "real", "rad"
    elif fam in ("system", "incomplete", "noid", "param"):
        policy = "real" if rng.random() < 0.6 else "complex"
        angle = "rad"
    else:
        policy = "real" if rng.random() < 0.6 else "complex"
        angle = "deg" if rng.random() < 0.1 else "rad"
    fn = {"linear": lambda: g_linear(rng), "quadratic": lambda: g_quadratic(rng, policy),
          "polynomial": lambda: g_polynomial(rng, policy), "rational": lambda: g_rational(rng, policy),
          "radical": lambda: g_radical(rng, policy), "indexed_root": lambda: g_indexed_root(rng, policy),
          "explog": lambda: g_explog(rng, policy), "trig": lambda: g_trig(rng, policy, angle),
          "abs": lambda: g_abs(rng, policy), "noid": lambda: g_noid(rng, policy),
          "param": lambda: g_param(rng, policy), "system": lambda: g_system(rng, policy),
          "edge": lambda: g_edge(rng, policy), "decimal": lambda: g_decimal(rng, policy),
          "incomplete": lambda: g_incomplete(rng, policy)}[fam]
    sub, eqs, truth = fn()
    if truth["kind"] in ("finite", "tuples") and not truth["sols"]:
        truth = {"kind": "empty"}
    nvars = len(eqs)
    return {"id": f"q{seed}-{index}", "seed": seed, "index": index, "family": fam, "sub": sub,
            "policy": policy, "angle": angle, "eqs": eqs, "vars": ["x", "y", "z"][:nvars], "truth": truth,
            "edit_seed": rng.randrange(1 << 30)}
