"""Seeded generator of keypad-typeable expression trees.

Case identity: case `index` of run `seed` uses RNG seeded with
sha256(f"{seed}:{index}") so every case is reproducible on its own.

Typeability rules (all observed on the emulator, see keys.py):
  * two adjacent atoms are an implicit product, EXCEPT when the right atom's
    first key is a digit after a number (digits would merge: 2 3 -> 23), or the
    right atom is a template-first fraction (`/` would capture the left atom);
  * a template-first fraction needs an operator or row start before it;
  * a captured fraction (`<atom> /`) needs a one-atom numerator;
  * `^` captures exactly one atom (the base);
  * EXP (×10^) only follows a number mantissa;
  * variables A–E (F is reserved for STO round trips), x, Ans.
"""
from __future__ import annotations

import hashlib
import random

import mpmath

from .expr import (Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row,
                   Sqrt, Var, to_struct)
from .sexpr import as_row

FAMILIES = [
    ("general", 34), ("rational", 10), ("irrational", 10), ("trig", 12),
    ("magnitude", 8), ("domain", 9), ("incomplete", 4), ("symbolic", 4),
    ("decimal", 5), ("bracket", 4),
]

# Values stored into A–E by the per-process preamble (typed with real keys).
VAR_VALUES_TEXT = {"A": "3", "B": "1/2", "C": "-4", "D": "sqrt(2)", "E": "3/4"}
ANS_CHOICES = ["5", "2/3", "-7", "1.25"]
MAX_NODES = 28


def case_rng(seed: int, index: int) -> random.Random:
    h = hashlib.sha256(f"{seed}:{index}".encode()).hexdigest()
    return random.Random(int(h[:16], 16))


def first_atom(a):
    """The atom whose key is typed first when `a` is typed."""
    if isinstance(a, Pow) and not a.sci:
        return first_atom(a.base)
    if isinstance(a, Frac) and a.capture:
        return first_atom(a.num.items[0])
    return a


def starts_with_digit(a) -> bool:
    return isinstance(first_atom(a), Num)


def can_follow(left, right) -> bool:
    """May `right` be typed directly after atom `left` (implicit product)?"""
    f = first_atom(right)
    if isinstance(f, Frac) and not f.capture:
        return False  # its `/` would capture `left`
    if isinstance(right, Pow) and right.sci:
        return isinstance(left, Num)
    if isinstance(left, Pow) and left.sci:
        return False
    if isinstance(left, Num) and starts_with_digit(right):
        return False
    return True


class TooBig(Exception):
    pass


def quick_magnitude_ok(tree: Row, limit=1e60) -> bool:
    """Cheap float pre-check so the exact oracle never meets 7^(7^7)."""
    from .evaluator import Ctx, Evaluator

    class F(Evaluator):
        def __init__(self):
            super().__init__(Ctx())

        def power(self, b, e):
            import sympy as sp
            try:
                fb, fe = complex(sp.N(b, 15)), complex(sp.N(e, 15))
            except (TypeError, ValueError):
                return [sp.Pow(b, e)]
            if abs(fe) > 400 or (abs(fb) > 1 and abs(fe) * mpmath.log10(abs(fb)) > 60):
                raise TooBig()
            return super().power(b, e)

    import sympy as sp
    env = {"A": sp.Integer(3), "B": sp.Rational(1, 2), "C": sp.Integer(-4),
           "D": sp.sqrt(2), "E": sp.Rational(3, 4), "Ans": sp.Integer(5)}
    ev = F()
    ev.ctx.env = env
    try:
        vals = ev.eval(to_struct(tree))
    except TooBig:
        return False
    except Exception:
        return True  # incomplete/unsupported: the checker will classify it
    for v in vals:
        try:
            if v.free_symbols:
                continue
            c = complex(sp.N(v, 15))
            if abs(c) > limit:
                return False
        except (TypeError, ValueError, OverflowError):
            continue
    return True


class Gen:
    def __init__(self, rng: random.Random, family: str, angle: str):
        self.r = rng
        self.family = family
        self.angle = angle
        self.use_ans = False

    # ── leaves ──────────────────────────────────────────────────────────
    def integer(self, small=False):
        r = self.r
        if small:
            return Num(str(r.choice([1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 12])))
        x = r.random()
        if x < 0.55:
            return Num(str(r.randint(0, 9)))
        if x < 0.85:
            return Num(str(r.randint(10, 99)))
        if x < 0.97:
            return Num(str(r.randint(100, 9999)))
        return Num(str(r.randint(10**14, 10**22)))

    def decimal(self):
        r = self.r
        ip = r.choice(["0", "1", "2", "3", "12", "0", str(r.randint(0, 999))])
        fp = "".join(r.choice("0123456789") for _ in range(r.randint(1, 4)))
        if fp.endswith("0") and r.random() < 0.7:
            fp = fp[:-1] + str(r.randint(1, 9))
        return Num(ip + "." + fp)

    def number(self):
        if self.family == "decimal" or self.r.random() < 0.12:
            return self.decimal()
        return self.integer()

    def var(self):
        r = self.r
        if self.family == "symbolic" and r.random() < 0.6:
            return Var("x")
        if r.random() < 0.15:
            self.use_ans = True
            return Var("Ans")
        return Var(r.choice(["A", "B", "C", "D", "E"]))

    # ── rows ────────────────────────────────────────────────────────────
    def row(self, depth, terms=None, ops="+-*/", allow_unary=True) -> Row:
        r = self.r
        n = terms or r.choice([1, 1, 2, 2, 2, 3, 3, 4])
        items = []
        for t in range(n):
            if t:
                items.append(Op(r.choice([o for o in ops])))
            if allow_unary and r.random() < 0.12:
                items.append(Op("-"))
            self.factors(items, depth)
        return Row(tuple(items))

    def factors(self, items, depth):
        """Append 1..3 juxtaposed atoms (implicit product)."""
        r = self.r
        k = 1 if r.random() < 0.75 else r.choice([2, 2, 3])
        prev = None
        for _ in range(k):
            for _attempt in range(8):
                a = self.atom(depth, after=prev)
                if prev is None or can_follow(prev, a):
                    break
            else:
                break
            if prev is not None and not can_follow(prev, a):
                break
            items.append(a)
            prev = a
            if isinstance(a, Num) and r.random() < (0.35 if self.family == "magnitude" else 0.05):
                e = self.r.randint(-30, 30) if self.family == "magnitude" else self.r.randint(-5, 5)
                exp = Row((Op("-"), Num(str(-e)))) if e < 0 else Row((Num(str(e)),))
                sci = Pow(Num("10"), exp, sci=True)
                items.append(sci)
                prev = sci

    def exp_row(self, depth):
        """Exponents stay small so the exact oracle stays fast."""
        r = self.r
        x = r.random()
        if x < 0.55:
            return Row((self.integer(small=True),))
        if x < 0.70:
            return Row((Op("-"), self.integer(small=True)))
        if x < 0.85:
            return Row((Frac(Row((Num(str(r.randint(1, 3))),)), Row((Num(str(r.randint(2, 4))),))),))
        return self.row(min(depth, 1), terms=r.choice([1, 2]), ops="+-*")

    # ── atoms ───────────────────────────────────────────────────────────
    def atom(self, depth, after=None):
        r = self.r
        fam = self.family
        if depth <= 0:
            x = r.random()
            if x < 0.62:
                return self.number()
            if x < 0.80:
                return Const(r.choice(["pi", "e"]))
            return self.var()
        weights = {
            "num": 30, "const": 6, "var": 7, "paren": 8, "frac": 12, "pow": 10, "sqrt": 6,
            "root": 2, "abs": 3, "fn": 9, "logb": 3,
        }
        if fam == "rational":
            weights.update(const=0, var=2, sqrt=0, root=0, fn=0, logb=0, abs=2, frac=22)
        elif fam == "irrational":
            weights.update(sqrt=14, root=6, fn=10, logb=6, const=10)
        elif fam == "trig":
            weights.update(fn=40, frac=10, pow=4, logb=0, root=0)
        elif fam == "bracket":
            weights.update(paren=20, fn=16, frac=14, pow=14)
        kind = r.choices(list(weights), weights=list(weights.values()))[0]
        d = depth - 1
        if kind == "num":
            return self.number()
        if kind == "const":
            return Const(r.choice(["pi", "e"]))
        if kind == "var":
            return self.var()
        if kind == "paren":
            return Paren(self.row(d, terms=r.choice([1, 2, 2, 3])))
        if kind == "frac":
            num = self.row(d, terms=r.choice([1, 1, 2]))
            den = self.row(d, terms=r.choice([1, 1, 2]))
            single = len(num.items) == 1
            capture = single and r.random() < 0.5
            if after is not None and not capture:
                capture = single
            return Frac(num, den, capture=capture)
        if kind == "pow":
            base = self.atom(max(d - 1, 0))
            while isinstance(base, Pow) and base.sci:
                base = self.number()
            if isinstance(base, Num) and len(base.text) > 3:
                base = Num(base.text[:2] if "." not in base.text[:2] else "7")
            return Pow(base, self.exp_row(d))
        if kind == "sqrt":
            return Sqrt(self.row(d, terms=r.choice([1, 1, 2])))
        if kind == "root":
            idx = Row((Num(str(r.choice([2, 3, 3, 4, 5]))),))
            return Root(idx, self.row(d, terms=r.choice([1, 1, 2])))
        if kind == "abs":
            return Abs(self.row(d, terms=r.choice([1, 2])))
        if kind == "fn":
            if fam == "trig":
                return self.trig_atom(d)
            name = r.choice(["sin", "cos", "tan", "asin", "acos", "atan", "ln", "log", "ln", "log"])
            return Fn(name, self.row(d, terms=r.choice([1, 1, 2])))
        if kind == "logb":
            base = Row((self.integer(small=True),)) if r.random() < 0.7 else self.row(d, terms=1)
            return LogB(base, self.row(d, terms=r.choice([1, 1, 2])))
        raise AssertionError(kind)

    def trig_atom(self, d):
        r = self.r
        name = r.choice(["sin", "cos", "tan", "sin", "cos", "tan", "asin", "acos", "atan"])
        if name in ("sin", "cos", "tan"):
            if self.angle == "deg":
                arg = Row((Num(str(r.choice([0, 30, 45, 60, 90, 120, 135, 150, 180, 210, 270, 300, 360, 720, 15, 1, 37]))),))
            else:
                k = r.choice([1, 1, 2, 3, 5])
                n = r.choice([1, 2, 3, 4, 6, 12])
                if r.random() < 0.3:
                    arg = Row((self.integer(small=True),))
                else:
                    num = Row((Num(str(k)), Const("pi"))) if k != 1 else Row((Const("pi"),))
                    arg = Row((Frac(num, Row((Num(str(n)),)), capture=False),))
        else:
            vals = [Row((Num("1"),)), Row((Op("-"), Num("1"))), Row((Num("0"),)),
                    Row((Frac(Row((Num("1"),)), Row((Num("2"),))),)),
                    Row((Frac(Row((Sqrt(Row((Num("3"),))),)), Row((Num("2"),)), capture=True),)),
                    Row((Frac(Row((Sqrt(Row((Num("2"),))),)), Row((Num("2"),)), capture=True),)),
                    Row((Sqrt(Row((Num("3"),))),)), Row((Num("2"),))]
            arg = r.choice(vals)
        return Fn(name, arg)

    def domain_tree(self) -> Row:
        r = self.r
        n0, n1 = Row((Num("0"),)), Row((Num("1"),))
        neg = lambda t: Row((Op("-"),) + Row((Num(t),)).items)
        choices = [
            Row((Frac(n1, n0),)),
            Row((Fn("tan", Row((Num("90"),)) if self.angle == "deg" else
                    Row((Frac(Row((Const("pi"),)), Row((Num("2"),))),))),)),
            Row((Fn("ln", n0),)), Row((Fn("log", n0),)),
            Row((Fn("ln", neg("1")),)), Row((Sqrt(neg(str(r.choice([1, 4, 9, 2])))),)),
            Row((Fn("asin", Row((Num("2"),))),)), Row((Fn("acos", neg("2")),)),
            Row((Pow(Num("0"), Row((Num("0"),))),)),
            Row((Pow(Num("0"), Row((Op("-"), Num("1")))),)),
            Row((LogB(Row((Num("1"),)), Row((Num("5"),))),)),
            Row((LogB(Row((Num("2"),)), n0),)),
            Row((Pow(Paren(neg("8")), Row((Frac(n1, Row((Num("3"),))),))),)),
            Row((Root(Row((Num("3"),)), neg("8")),)),
            Row((Root(Row((Num("4"),)), neg("16")),)),
            Row((Abs(neg(str(r.randint(1, 50)))),)),
            Row((Fn("sqrt" if False else "atan", Row((Num("1"),))),)),
            Row((Sqrt(n0),)), Row((Fn("ln", n1),)),
            Row((Fn("acos", neg("1")),)), Row((Fn("asin", n1),)),
            Row((Frac(Row((Num(str(r.randint(1, 9))),)), Row((Num("0"), Op("*"), Const("pi"))),),)),
        ]
        t = r.choice(choices)
        if r.random() < 0.4:
            t = Row(t.items + (Op(r.choice("+*")), self.integer(small=True)))
        return t

    def incomplete_tree(self, depth) -> Row:
        """A well-formed tree with one slot left empty (EXE must refuse).

        Only LAST/single slots are left empty: RIGHT does not leave an empty
        first slot of a two-slot template (observed: `/ RIGHT 2` -> 2/□), so
        an empty first slot is not reachable by forward typing."""
        r = self.r
        e = Row((Empty(),))
        a = self.integer(small=True)
        choices = [
            Row((Frac(Row((a,)), e),)),
            Row((Pow(a, e),)), Row((Sqrt(e),)), Row((Fn(r.choice(["sin", "ln", "log"]), e),)),
            Row((LogB(Row((a,)), e),)),
            Row((Paren(e),)), Row((a, Op(r.choice("+*")), Frac(Row((a,)), e))),
            Row((Abs(e),)), Row((Root(Row((a,)), e),)), Row((a, Op(r.choice("+-*/")))),
        ]
        return r.choice(choices)

    def tree(self, depth) -> Row:
        if self.family == "domain":
            return self.domain_tree()
        if self.family == "incomplete":
            return self.incomplete_tree(depth)
        return self.row(depth)


def generate(seed: int, index: int, max_depth: int = 5):
    """Return a case dict: id, family, angle, tree, ans (or None)."""
    rng = case_rng(seed, index)
    fam = rng.choices([f for f, _ in FAMILIES], weights=[w for _, w in FAMILIES])[0]
    angle = rng.choice(["rad", "deg"])
    depth = rng.choice([1, 2, 2, 3, 3, 4, 5][: max(1, min(7, max_depth + 2))])
    depth = min(depth, max_depth)
    from .expr import size
    for _attempt in range(40):
        g = Gen(rng, fam, angle)
        t = g.tree(depth)
        # Keep cases human-sized (<= MAX_NODES nodes): giant trees mostly
        # measure Giac's patience, and shrinking then dominates the budget.
        if size(t) <= MAX_NODES:
            from .timeout import Overrun, call
            try:
                if call(quick_magnitude_ok, 5, t):
                    break
            except Overrun:
                continue
    ans = rng.choice(ANS_CHOICES) if g.use_ans else None
    return {"id": f"s{seed}-{index}", "seed": seed, "index": index, "family": fam,
            "angle": angle, "depth": depth, "tree": t, "ans": ans}
