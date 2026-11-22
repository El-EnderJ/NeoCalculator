"""Seeded generator of keypad-typeable Calculus inputs (functions of x).

Case id `c<seed>-<index>`, RNG = sha256(f"{seed}:{index}") (common.gen.case_rng),
so every case is reproducible on its own. Each case has: family, op
('diff' = Derivative tab, 'int' = Integral tab), angle ('rad'|'deg') and a tree.

Typeability follows common.gen.can_follow (observed editor rules). Abs and
n-th roots have no key (Toolbox), definite integrals and diff() are Toolbox
templates (model.DefInt / model.DiffCall).
"""
from __future__ import annotations


from common.expr import Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row, Sqrt, Var
from common.gen import can_follow, case_rng

from .model import DefInt, DiffCall, size

FAMILIES = [
    ("poly", 7), ("rational", 8), ("power_root", 8), ("exp_log", 9), ("trig", 10), ("invtrig", 6),
    ("abs", 4), ("composition", 10), ("product_chain", 10), ("parameter", 3), ("defint", 10),
    ("ftc", 4), ("diffcall", 3), ("domain", 6), ("incomplete", 2),
]
MAX_NODES = 26
EDGE_SEED_MIN = 100
EDGE_FAMILIES = ("domain", "defint", "ftc", "incomplete", "diffcall", "parameter", "abs")
X = Var("x")
Y = Var("y")


def R(*items):
    return Row(tuple(items))


def N(v):
    return Num(str(v))


class CGen:
    def __init__(self, rng, family, angle, var="x"):
        self.r = rng
        self.family = family
        self.angle = angle
        self.v = Var(var)

    # ── small pieces ────────────────────────────────────────────────────
    def k(self, lo=1, hi=9):
        r = self.r
        if r.random() < 0.06:
            return Num(r.choice(["0.5", "1.5", "2.5", "0.25", "1.2"]))
        return N(r.randint(lo, hi))

    def lin(self):
        """a x + b in typed form: x, 2x, x+1, 3x-2, -x+4, x/2 ..."""
        r, v = self.r, self.v
        c = r.random()
        if c < 0.25:
            return R(v)
        if c < 0.45:
            return R(self.k(2, 5), v)
        if c < 0.75:
            return R(v, Op(r.choice("+-")), self.k())
        if c < 0.9:
            return R(self.k(2, 5), v, Op(r.choice("+-")), self.k())
        return R(Op("-"), v, Op("+"), self.k())

    def poly(self, maxdeg=4, terms=None):
        r, v = self.r, self.v
        n = terms or r.choice([1, 2, 2, 3])
        degs = sorted(r.sample(range(0, maxdeg + 1), min(n, maxdeg + 1)), reverse=True)
        items = []
        for i, d in enumerate(degs):
            if i:
                items.append(Op(r.choice("+-")))
            elif r.random() < 0.15:
                items.append(Op("-"))
            c = self.k() if (d == 0 or r.random() < 0.5) else None
            if d == 0:
                items.append(c)
                continue
            if c is not None:
                items.append(c)
            items.append(v if d == 1 else Pow(v, R(N(d))))
        return Row(tuple(items))

    def group(self, row):
        """Row -> one atom (parenthesised unless already a single atom)."""
        if len(row.items) == 1 and not isinstance(row.items[0], Op):
            return row.items[0]
        return Paren(row)

    def exponent(self):
        r = self.r
        c = r.random()
        if c < 0.5:
            return R(N(r.choice([2, 2, 3, 3, 4, 5])))
        if c < 0.68:
            return R(Op("-"), N(r.choice([1, 2, 3])))
        if c < 0.88:
            return R(Frac(R(N(r.choice([1, 1, 2, 3]))), R(N(r.choice([2, 3, 4, 5])))))
        return R(Op("-"), Frac(R(N(1)), R(N(r.choice([2, 3])))))

    def trig_arg(self):
        r = self.r
        if self.angle == "deg" and r.random() < 0.3:
            return R(N(r.choice([2, 3, 30, 45, 90])), self.v)
        return self.lin() if r.random() < 0.7 else R(Pow(self.v, R(N(2))))

    # ── generic atom grammar (for compositions/products) ────────────────
    def atom(self, depth):
        r, v = self.r, self.v
        if depth <= 0:
            return v if r.random() < 0.75 else self.k()
        kinds = ["fn", "pow", "sqrt", "exp", "frac", "paren", "abs", "root", "logb", "x"]
        weights = [10, 6, 4, 4, 4, 3, 2, 1, 1, 3]
        kind = r.choices(kinds, weights)[0]
        d = depth - 1
        inner = lambda: self.inner_row(d)
        if kind == "x":
            return v
        if kind == "fn":
            name = r.choice(["sin", "cos", "tan", "ln", "ln", "atan", "asin", "acos", "log", "sin", "cos"])
            return Fn(name, inner())
        if kind == "pow":
            base = r.choice([v, v, Paren(self.lin()), Fn(r.choice(["sin", "cos", "ln"]), inner())])
            return Pow(base, self.exponent())
        if kind == "sqrt":
            return Sqrt(inner())
        if kind == "exp":
            return Pow(Const("e"), inner())
        if kind == "frac":
            return Frac(R(self.k(1, 5)), inner())
        if kind == "paren":
            return Paren(R(self.atom(d), Op(r.choice("+-")), self.k()))
        if kind == "abs":
            return Abs(inner())
        if kind == "root":
            return Root(R(N(r.choice([3, 3, 4, 5]))), inner())
        return LogB(R(N(r.choice([2, 3, 10]))), inner())

    def inner_row(self, d):
        r = self.r
        if d <= 0:
            return self.lin() if r.random() < 0.6 else R(self.v)
        c = r.random()
        if c < 0.6:
            return R(self.atom(d))
        if c < 0.85:
            return R(self.atom(d), Op(r.choice("+-")), self.k())
        k = self.k(2, 5)
        for _ in range(6):
            a = self.atom(d)
            if can_follow(k, a):
                return R(k, a)
        return R(self.atom(d))

    # ── families ───────────────────────────────────────────────────────
    def fam_poly(self):
        return self.poly(maxdeg=self.r.choice([2, 3, 4, 5]))

    def fam_rational(self):
        r, v = self.r, self.v
        c = r.random()
        if c < 0.3:
            return R(Frac(self.poly(2), self.poly(2, terms=2)))
        if c < 0.5:
            return R(Frac(R(self.k()), R(Pow(Paren(self.lin()), R(N(r.choice([1, 2, 3])))))))
        if c < 0.7:
            return R(Frac(self.lin(), self.lin()))
        if c < 0.85:
            return R(Frac(R(N(1)), R(Pow(v, R(N(2))), Op("+"), self.k())))
        return R(self.k(), Op("/"), Paren(self.lin()))  # ÷ operator

    def fam_power_root(self):
        r, v = self.r, self.v
        c = r.random()
        if c < 0.25:
            return R(Pow(v, self.exponent()))
        if c < 0.45:
            return R(Sqrt(self.lin()))
        if c < 0.6:
            return R(Root(R(N(r.choice([3, 4, 5]))), self.lin()))
        if c < 0.75:
            return R(Pow(Paren(self.lin()), self.exponent()))
        if c < 0.88:
            return R(Sqrt(self.poly(2, terms=2)))
        return R(v, Sqrt(self.lin()))

    def fam_exp_log(self):
        r, v = self.r, self.v
        c = r.random()
        opts = [
            lambda: R(Pow(Const("e"), self.lin())),
            lambda: R(Pow(Const("e"), R(Pow(v, R(N(2)))))),
            lambda: R(Fn("ln", self.lin())),
            lambda: R(Fn("ln", self.poly(2, terms=2))),
            lambda: R(Fn("log", self.lin())),
            lambda: R(LogB(R(N(r.choice([2, 3, 5]))), self.lin())),
            lambda: R(v, Pow(Const("e"), self.lin())),
            lambda: R(v, Fn("ln", R(v))),
            lambda: R(Pow(N(r.choice([2, 3, 10])), self.lin())),
            lambda: R(Pow(Fn("ln", R(v)), R(N(2)))),
            lambda: R(Frac(R(Fn("ln", R(v))), R(v))),
        ]
        return r.choice(opts)()

    def fam_trig(self):
        r, v = self.r, self.v
        t = lambda: Fn(r.choice(["sin", "cos", "tan"]), self.trig_arg())
        opts = [
            lambda: R(t()),
            lambda: R(Pow(Fn(r.choice(["sin", "cos", "tan"]), R(v)), R(N(r.choice([2, 3]))))),
            lambda: R(Fn("sin", R(v)), Fn("cos", R(v))),
            lambda: R(v, t()),
            lambda: R(Frac(R(Fn("sin", R(v))), R(Fn("cos", R(v))))),
            lambda: R(t(), Op(r.choice("+-*")), t()),
            lambda: R(Pow(v, R(N(2))), Fn("cos", R(v))),
            lambda: R(Pow(Const("e"), R(v)), Fn("sin", R(v))),
            lambda: R(Frac(R(N(1)), R(Pow(Fn("cos", R(v)), R(N(2)))))),
        ]
        return r.choice(opts)()

    def fam_invtrig(self):
        r, v = self.r, self.v
        name = r.choice(["asin", "acos", "atan"])
        opts = [
            lambda: R(Fn(name, R(v))),
            lambda: R(Fn(name, R(Frac(R(v), R(N(r.choice([2, 3]))), capture=True)))),
            lambda: R(Fn(name, self.lin())),
            lambda: R(v, Fn(name, R(v))),
            lambda: R(Fn("atan", R(Pow(v, R(N(2)))))),
            lambda: R(Pow(Fn(name, R(v)), R(N(2)))),
        ]
        return r.choice(opts)()

    def fam_abs(self):
        r, v = self.r, self.v
        opts = [
            lambda: R(Abs(self.lin())),
            lambda: R(v, Abs(R(v))),
            lambda: R(Abs(R(Fn("sin", R(v))))),
            lambda: R(Abs(self.poly(2, terms=2))),
            lambda: R(Frac(R(Abs(R(v))), R(v))),
            lambda: R(Fn("ln", R(Abs(R(v))))),
        ]
        return r.choice(opts)()

    def fam_composition(self):
        r = self.r
        depth = r.choice([2, 3, 3, 4, 5])
        a = self.atom(depth)
        if r.random() < 0.3:
            return R(a, Op(r.choice("+-")), self.k())
        return R(a)

    def fam_product_chain(self):
        r = self.r
        a, b = self.atom(r.choice([1, 2])), self.atom(r.choice([1, 2]))
        c = r.random()
        if c < 0.4 and can_follow(a, b):
            return R(a, b)
        if c < 0.7:
            return R(Frac(R(a), R(b)))
        return R(a, Op("*"), b)

    def fam_parameter(self):
        r = self.r
        if r.random() < 0.5:
            return R(Y, Pow(X, R(N(2))), Op("+"), Fn("sin", R(Y, X)))
        opts = [
            lambda: R(Pow(X, R(N(2))), Y),
            lambda: R(Y, Pow(X, R(N(3)))),
            lambda: R(Fn("sin", R(X, Y))),
            lambda: R(Pow(Const("e"), R(Y, X))),
            lambda: R(Frac(R(X), R(Y))),
            lambda: R(Pow(Y, R(N(2))), Op("+"), X),
        ]
        return r.choice(opts)()

    def defint_body(self, var):
        r = self.r
        g = CGen(r, "poly", self.angle, var)
        c = r.random()
        if c < 0.35:
            return g.poly(3)
        if c < 0.55:
            return R(Fn(r.choice(["sin", "cos"]), R(Var(var))))
        if c < 0.7:
            return R(Pow(Const("e"), R(Var(var))))
        if c < 0.8:
            return R(Frac(R(N(1)), R(Pow(Var(var), R(N(2))), Op("+"), N(1))))
        return R(Sqrt(R(Var(var))))

    def limits(self):
        r = self.r
        kind = r.choices(["int", "decimal", "expr", "swap", "equal", "sym"], [5, 2, 2, 1, 1, 2])[0]
        if kind == "int":
            a = r.randint(-2, 2)
            b = a + r.randint(1, 3)
            return kind, self.signed(a), self.signed(b)
        if kind == "decimal":
            return kind, R(Num(r.choice(["0.5", "0.25", "1.5"]))), R(Num(r.choice(["2.5", "3.75", "1.75"])))
        if kind == "expr":
            hi = r.choice([R(Const("pi")), R(Frac(R(Const("pi")), R(N(2)))), R(Const("e")), R(Sqrt(R(N(2))))])
            return kind, R(N(r.choice([0, 1]))), hi
        if kind == "swap":
            return kind, R(N(r.randint(2, 4))), R(N(r.randint(0, 1)))
        if kind == "equal":
            a = self.signed(r.randint(-1, 3))
            return kind, a, a
        a = r.randint(1, 3)
        return kind, self.signed(-a), self.signed(a)

    @staticmethod
    def signed(n):
        return R(Op("-"), N(-n)) if n < 0 else R(N(n))

    def fam_defint(self):
        r = self.r
        sub = r.choices(["regular", "singular_ok", "divergent", "discontinuity", "odd_sym"], [9, 2, 2, 1, 2])[0]
        var = "y" if r.random() < 0.6 else "x"
        V = Var(var)
        if sub == "regular":
            kind, lo, hi = self.limits()
            body = self.defint_body(var)
        elif sub == "singular_ok":
            kind, lo, hi = sub, R(N(0)), R(N(1))
            body = r.choice([R(Frac(R(N(1)), R(Sqrt(R(V))))), R(Fn("ln", R(V))),
                             R(Pow(V, R(Op("-"), Frac(R(N(1)), R(N(3))))))])
        elif sub == "divergent":
            kind = sub
            lo, hi, body = r.choice([
                (R(N(0)), R(N(1)), R(Frac(R(N(1)), R(V)))),
                (R(Op("-"), N(1)), R(N(1)), R(Frac(R(N(1)), R(Pow(V, R(N(2))))))),
                (R(N(0)), R(N(2)), R(Frac(R(N(1)), R(V, Op("-"), N(1))))),
            ])
        elif sub == "discontinuity":
            kind = sub
            lo, hi, body = r.choice([
                (R(Op("-"), N(1)), R(N(2)), R(Abs(R(V)))),
                (R(Op("-"), N(2)), R(N(3)), R(Frac(R(Abs(R(V))), R(V)))),
                (R(N(0)), R(N(3)), R(Abs(R(V, Op("-"), N(1))))),
            ])
        else:
            kind = sub
            a = r.randint(1, 3)
            lo, hi = self.signed(-a), self.signed(a)
            body = r.choice([R(Pow(V, R(N(3)))), R(Fn("sin", R(V))), R(V, Fn("cos", R(V))),
                             R(Pow(V, R(N(5))), Op("-"), V)])
        d = DefInt(lo, hi, body, var)
        c = r.random()
        if var == "y" and c < 0.25:
            return R(Pow(X, R(N(2))), Op("+"), d), sub
        if var == "y" and c < 0.4:
            return R(d, Op("+"), X), sub
        if var == "y" and c < 0.5:
            return R(X, Op("*"), d), sub
        return R(d), sub

    def fam_ftc(self):
        r = self.r
        upper = r.choice([R(X), R(Pow(X, R(N(2)))), R(N(2), X), R(Fn("sin", R(X))), R(X, Op("+"), N(1))])
        lower = R(N(r.choice([0, 1]))) if r.random() < 0.8 else R(X)
        g = CGen(r, "poly", self.angle, "y")
        body = r.choice([g.poly(2), R(Fn("cos", R(Y))), R(Pow(Const("e"), R(Y))), R(Sqrt(R(Y, Op("+"), N(1)))),
                         R(Frac(R(N(1)), R(Y, Op("+"), N(2))))])
        return R(DefInt(lower, upper, body, "y")), "ftc"

    def fam_diffcall(self):
        r = self.r
        inner = r.choice([self.poly(3), R(Fn("sin", R(X))), R(Pow(Const("e"), R(N(2), X))),
                          R(X, Fn("ln", R(X))), R(Pow(X, R(N(4))))])
        d = DiffCall(inner, "x")
        if r.random() < 0.3:
            return R(d, Op("+"), X), "diffcall"
        return R(d), "diffcall"

    def fam_domain(self):
        r, v = self.r, self.v
        opts = [
            R(Sqrt(R(v, Op("-"), N(3)))), R(Fn("ln", R(v, Op("-"), N(2)))), R(Frac(R(N(1)), R(v, Op("-"), N(1)))),
            R(Fn("tan", R(v))), R(Fn("asin", R(N(2), v))), R(Fn("ln", R(Op("-"), v))), R(Sqrt(R(Op("-"), v))),
            R(Frac(R(N(1)), R(v))), R(Frac(R(N(1)), R(Pow(v, R(N(2))), Op("-"), N(1)))),
            R(Sqrt(R(N(1), Op("-"), Pow(v, R(N(2)))))), R(Fn("acos", R(v))), R(Pow(v, R(Frac(R(N(1)), R(N(3)))))),
            R(Fn("ln", R(Fn("sin", R(v))))), R(Frac(R(N(1)), R(Fn("sin", R(v))))), R(Fn("ln", R(N(0)))),
            R(Frac(R(N(1)), R(N(0)))), R(Pow(N(0), R(N(0)))), R(Fn("log", R(Abs(R(v))))),
            R(Frac(R(Pow(v, R(N(2))), Op("-"), N(1)), R(v, Op("-"), N(1)))), R(Root(R(N(3)), R(v))),
            R(Fn("atan", R(Frac(R(N(1)), R(v))))),
        ]
        return r.choice(opts)

    def fam_incomplete(self):
        r, v = self.r, self.v
        e = Row((Empty(),))
        return r.choice([
            R(Frac(R(v), e)), R(Pow(v, e)), R(Sqrt(e)), R(Fn(r.choice(["sin", "ln"]), e)),
            R(LogB(R(N(2)), e)), R(Paren(e)), R(v, Op("+"), Frac(R(N(1)), e)), R(Abs(e)),
            R(v, Op(r.choice("+-*"))),
        ])


def generate(seed: int, index: int):
    rng = case_rng(seed, index)
    fams = FAMILIES
    if seed >= EDGE_SEED_MIN:  # edge-family runs: seeds >= 100 draw only the edge families
        fams = [(f, w) for f, w in FAMILIES if f in EDGE_FAMILIES]
    fam = rng.choices([f for f, _ in fams], weights=[w for _, w in fams])[0]
    angle = "deg" if rng.random() < (0.45 if fam in ("trig", "invtrig") else 0.2) else "rad"
    if fam in ("defint",):
        op = "int" if rng.random() < 0.65 else "diff"
    elif fam == "ftc":
        op = "diff" if rng.random() < 0.8 else "int"
    else:
        op = "diff" if rng.random() < 0.5 else "int"
    sub = None
    for _attempt in range(30):
        g = CGen(rng, fam, angle)
        res = getattr(g, "fam_" + fam)()
        tree, sub = res if isinstance(res, tuple) else (res, None)
        if size(tree) <= MAX_NODES:
            break
    return {"id": f"c{seed}-{index}", "seed": seed, "index": index, "family": fam, "sub": sub,
            "angle": angle, "op": op, "tree": tree}
