"""W1 generators layered on common.gen (no edits to common/).

* `generate_weighted`  - common.gen.generate with a caller-chosen family table.
* `EdgeGen`            - dedicated edge families: huge/tiny magnitudes, int64
                         boundary rationals, periodic decimals, domain
                         boundaries, poles, branch cuts, DEG/RAD trig incl.
                         huge arguments, empty/incomplete slots.
* `fmt_tree`           - values aimed at FORMAT rounding: exact ties, near ties
                         (double rounding of the 12-digit companion), carries,
                         terminating/periodic rationals, irrationals scaled by
                         10^k.

Every tree is a common.expr Row that the common keystroke projections can
type (checked with common.shrink.typeable).
"""
from __future__ import annotations

from common.expr import (Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row,
                         Sqrt, Var, size)
from common.gen import Gen, case_rng, quick_magnitude_ok
from common.shrink import typeable

R = lambda *items: Row(tuple(items))  # noqa: E731


def neg(atom):
    return (Op("-"), atom)


def sci(mant: str, k: int):
    """mantissa ×10^k as typed with EXP."""
    exp = R(Op("-"), Num(str(-k))) if k < 0 else R(Num(str(k)))
    return (Num(mant), Pow(Num("10"), exp, sci=True))


def generate_weighted(seed, index, families, prefix="w", max_depth=5, magnitude_limit=1e60,
                      builders=None):
    """Like common.gen.generate, with `families` = [(name, weight)].
    `builders` maps extra family names to callables (rng, angle) -> Row."""
    rng = case_rng(seed, index)
    fam = rng.choices([f for f, _ in families], weights=[w for _, w in families])[0]
    angle = rng.choice(["rad", "deg"])
    depth = rng.choice([1, 2, 2, 3, 3, 4, 5])
    depth = min(depth, max_depth)
    t = None
    g = Gen(rng, fam if fam in {"general", "rational", "irrational", "trig", "magnitude", "domain",
                                 "incomplete", "symbolic", "decimal", "bracket"} else "general", angle)
    for _attempt in range(40):
        if builders and fam in builders:
            t = builders[fam](rng, angle, g)
        else:
            g = Gen(rng, fam, angle)
            t = g.tree(depth)
        if t is None or not typeable(t) or size(t) > 28:
            continue
        from common.timeout import Overrun, call
        try:
            if magnitude_limit is None or call(quick_magnitude_ok, 5, t, magnitude_limit):
                break
        except Overrun:
            continue
    from common.expr import walk
    uses_ans = g.use_ans or any(isinstance(n, Var) and n.name == "Ans" for n in walk(t))
    ans = rng.choice(["5", "2/3", "-7", "1.25"]) if uses_ans else None
    return {"id": f"{prefix}{seed}-{index}", "seed": seed, "index": index, "family": fam,
            "angle": angle, "depth": depth, "tree": t, "ans": ans}


# ── edge families ──────────────────────────────────────────────────────────
def _small(rng):
    return Num(str(rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 12])))


def _wrap(rng, row: Row, g) -> Row:
    """Optionally embed an edge row in a little context (sum, product, fraction,
    function) so the edge value travels through the rest of the pipeline."""
    x = rng.random()
    if x < 0.45:
        return row
    if x < 0.60:
        return Row(row.items + (Op(rng.choice("+-*")), _small(rng)))
    if x < 0.70:
        return R(_small(rng), Op(rng.choice("+-*")), Paren(row))
    if x < 0.80:
        return R(Frac(row, R(_small(rng))))
    if x < 0.88:
        return R(Frac(R(_small(rng)), row))
    if x < 0.94:
        return R(Abs(row))
    return R(Pow(Paren(row), R(Num(str(rng.choice([2, 3]))))))


def magnitude_tree(rng, angle, g):
    """Huge / tiny magnitudes incl. beyond double range, cancellation."""
    k1 = rng.choice([rng.randint(-99, 99), rng.randint(-400, 400), rng.choice([99, -99, 100, -100, 308, 309, -308, -324, -330])])
    m1 = rng.choice(["1", "9", "2.5", "9.99999999999", "1.00000000001", "7", "3.14159"])
    k2 = rng.randint(-99, 99)
    m2 = rng.choice(["1", "3", "4.5", "1.0000001"])
    op = rng.choice("+-*/")
    shape = rng.random()
    if shape < 0.3:
        return R(*sci(m1, k1))
    if shape < 0.6:
        if op == "/":
            return R(Frac(R(*sci(m1, k1)), R(*sci(m2, k2))))
        return R(*sci(m1, k1), Op(op), *sci(m2, k2))
    if shape < 0.75:
        # catastrophic cancellation: (a×10^k + b) - a×10^k
        return R(Paren(R(*sci(m1, abs(k1) % 30 + 10), Op("+"), _small(rng))), Op("-"), *sci(m1, abs(k1) % 30 + 10))
    if shape < 0.88:
        return R(Fn(rng.choice(["ln", "log"]), R(*sci(m1, k1))))
    return R(Sqrt(R(*sci(m1, k1))))


def bigrational_tree(rng, angle, g):
    """Exact rationals around the int64 boundary (ExactVal is int64)."""
    big = rng.choice(["9223372036854775807", "9223372036854775808", "9223372036854775806",
                      "18446744073709551616", "4611686018427387904", "999999999999999999",
                      "1000000000000", "1000000000001", "999999999999", "123456789012345678"])
    den = rng.choice(["2", "3", "7", "9223372036854775807", "10", "1000000007"])
    shape = rng.random()
    if shape < 0.35:
        return R(Frac(R(Num(big)), R(Num(den))))
    if shape < 0.55:
        return R(Num(big), Op(rng.choice("+-*")), Num(rng.choice(["1", "2", big])))
    if shape < 0.7:
        return R(Op("-"), Frac(R(Num(big)), R(Num(den))))
    if shape < 0.85:
        return R(Frac(R(Num(big)), R(Num(den))), Op(rng.choice("+-")), Frac(R(Num("1")), R(Num(den))))
    return R(Frac(R(Num(rng.choice(["1", "2", "5"]))), R(Num(big))))


def periodic_tree(rng, angle, g):
    q = rng.choice([3, 6, 7, 9, 11, 12, 13, 17, 19, 23, 81, 97, 99, 101, 333, 999, 9999, 7 * 8, 3 * 125, 17 * 16])
    p = rng.randint(1, 3 * q)
    shape = rng.random()
    if shape < 0.5:
        return R(Frac(R(Num(str(p))), R(Num(str(q)))))
    if shape < 0.7:
        return R(Op("-"), Frac(R(Num(str(p))), R(Num(str(q)))))
    if shape < 0.85:
        # typed decimal with a repeating tail, e.g. 0.333333333333
        rep = rng.choice(["3", "6", "9", "142857", "09", "1"])
        lit = rng.choice(["0", "1", "2"]) + "." + (rep * 20)[: rng.randint(8, 18)]
        return R(Num(lit))
    return R(Num(str(rng.randint(1, 9))), Op("+"), Frac(R(Num(str(p))), R(Num(str(q)))))


def domain_edge_tree(rng, angle, g):
    deg = angle == "deg"
    pi2 = R(Frac(R(Const("pi")), R(Num("2"))))
    choices = [
        # poles
        R(Fn("tan", R(Num(rng.choice(["90", "270", "450"])) if deg else pi2.items[0]))),
        R(Fn("tan", R(Op("-"), Num("90")) if deg else R(Op("-"), Frac(R(Const("pi")), R(Num("2")))))),
        R(Frac(R(Num("1")), R(Fn("sin", R(Num("180")) if deg else R(Const("pi")))))),
        R(Frac(R(Num("1")), R(Fn("cos", R(Num("90")) if deg else pi2)))),
        R(Frac(R(Num("1")), R(Paren(R(Num("2"), Op("-"), Num("2")))))),
        R(Frac(R(Num("0")), R(Num("0")))),
        R(Frac(R(Num("1")), R(Frac(R(Num("1")), R(Num("0")))))),
        R(Num("1"), Op("/"), Paren(R(Num("1"), Op("/"), Num("0")))),
        R(Num("2"), Op("-"), Frac(R(Num("1")), R(Num("0")))),
        R(Fn("ln", R(Num("0")))), R(Fn("log", R(Num("0")))),
        R(LogB(R(Num("1")), R(Num(str(rng.randint(2, 9)))))),
        R(LogB(R(Num("0")), R(Num("5")))),
        R(Pow(Num("0"), R(Num("0")))), R(Pow(Num("0"), R(Op("-"), Num("2")))),
        R(Pow(Num("0"), R(Frac(R(Num("1")), R(Num("2")))))),
        # boundaries (finite)
        R(Fn("asin", R(Num("1")))), R(Fn("acos", R(Op("-"), Num("1")))), R(Fn("atan", R(Num("1")))),
        R(Fn("asin", R(Op("-"), Num("1")))), R(Fn("acos", R(Num("1")))), R(Sqrt(R(Num("0")))),
        R(Fn("ln", R(Num("1")))), R(Fn("ln", R(Const("e")))), R(Fn("log", R(Num("1000")))),
        R(LogB(R(Num("2")), R(Num("1024")))), R(LogB(R(Num("10")), R(Num("0.001")))),
        R(LogB(R(Frac(R(Num("1")), R(Num("2")))), R(Num("8")))),
        R(Fn("asin", R(Num("0.99999999999")))), R(Fn("acos", R(Num("0.9999999999999")))),
        R(Fn("ln", R(Num("1.000000000001")))),
        # branch cuts (complex results)
        R(Sqrt(R(Op("-"), Num(str(rng.choice([1, 2, 3, 4, 9, 16, 0.5])))))),
        R(Fn("ln", R(Op("-"), Num(str(rng.choice([1, 2, 10])))))),
        R(Fn("log", R(Op("-"), Num("100")))),
        R(Fn("asin", R(Num(rng.choice(["2", "1.5", "10"]))))),
        R(Fn("acos", R(Op("-"), Num(rng.choice(["2", "1.5", "10"]))))),
        R(Fn("asin", R(Op("-"), Num("2")))), R(Fn("acos", R(Num("2")))),
        R(Pow(Paren(R(Op("-"), Num("8"))), R(Frac(R(Num("1")), R(Num("3")))))),
        R(Pow(Paren(R(Op("-"), Num("8"))), R(Frac(R(Num("2")), R(Num("3")))))),
        R(Pow(Paren(R(Op("-"), Num("1"))), R(Frac(R(Num("1")), R(Num("2")))))),
        R(Pow(Paren(R(Op("-"), Num("2"))), R(Num("0.5")))),
        R(Root(R(Num("3")), R(Op("-"), Num("27")))), R(Root(R(Num("4")), R(Op("-"), Num("16")))),
        R(Root(R(Num("5")), R(Op("-"), Num("32")))), R(Root(R(Num("2")), R(Op("-"), Num("9")))),
        R(LogB(R(Op("-"), Num("2")), R(Num("4")))), R(LogB(R(Num("2")), R(Op("-"), Num("8")))),
        R(Fn("atan", R(Num(rng.choice(["1000000", "0.000001"]))))),
    ]
    return _wrap(rng, rng.choice(choices), g)


def trig_edge_tree(rng, angle, g):
    deg = angle == "deg"
    fn = rng.choice(["sin", "cos", "tan"])
    x = rng.random()
    if x < 0.25:
        # huge arguments (argument reduction)
        arg = R(*sci(rng.choice(["1", "3", "7.5"]), rng.choice([5, 10, 15, 16, 22, 30])))
    elif x < 0.40:
        arg = R(Num(str(rng.choice([10**6, 10**9, 123456789, 1000000007, 314159265]))))
    elif x < 0.60:
        if deg:
            arg = R(Num(str(rng.choice([0, 15, 18, 22.5, 30, 36, 45, 54, 60, 72, 75, 90, 105, 120, 135,
                                         150, 180, 225, 270, 300, 315, 330, 360, 540, 720, 1080]))))
        else:
            k, n = rng.choice([1, 2, 3, 5, 7, 11]), rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12])
            num = R(Num(str(k)), Const("pi")) if k != 1 else R(Const("pi"))
            arg = R(Frac(num, R(Num(str(n)))))
    elif x < 0.70:
        # degree-looking value in RAD / radian-looking in DEG
        arg = (R(Frac(R(Const("pi")), R(Num(str(rng.choice([2, 3, 4, 6])))))) if deg
               else R(Num(str(rng.choice([30, 45, 60, 90])))))
    elif x < 0.85:
        # inverse trig of exact values and compositions
        inv = rng.choice(["asin", "acos", "atan"])
        v = rng.choice([R(Num("0")), R(Num("1")), R(Op("-"), Num("1")), R(Frac(R(Num("1")), R(Num("2")))),
                        R(Frac(R(Sqrt(R(Num("3")))), R(Num("2")), capture=True)),
                        R(Op("-"), Frac(R(Sqrt(R(Num("2")))), R(Num("2")), capture=True)),
                        R(Sqrt(R(Num("3")))), R(Num("0.5"))])
        inner = R(Fn(inv, v))
        if rng.random() < 0.5:
            return R(Fn(fn, inner))
        return _wrap(rng, inner, g)
    else:
        # tiny arguments
        arg = R(*sci(rng.choice(["1", "2.5"]), rng.choice([-5, -8, -12, -20])))
    t = R(Fn(fn, arg))
    return _wrap(rng, t, g)


def incomplete_edge_tree(rng, angle, g):
    e = R(Empty())
    a = _small(rng)
    choices = [
        R(Frac(R(a), e)), R(Pow(a, e)), R(Sqrt(e)), R(Fn(rng.choice(["sin", "cos", "tan", "ln", "log", "asin"]), e)),
        R(LogB(R(a), e)), R(Paren(e)), R(Abs(e)), R(Root(R(a), e)),
        R(a, Op(rng.choice("+-*/"))), R(Op("-")),
        R(a, Op("+"), Paren(e)), R(Sqrt(R(a, Op("+"), Paren(e)))), R(Frac(R(a), R(a, Op("*")))),
        R(Fn("sin", R(Frac(R(a), e)))), R(Num("2"), Pow(Num("10"), e, sci=True)),
        R(Pow(a, R(Frac(R(a), e)))), R(Abs(R(a, Op("-")))),
    ]
    return rng.choice(choices)


EDGE_FAMILIES = [("e_magnitude", 18), ("e_bigrational", 12), ("e_periodic", 10), ("e_domain", 22),
                 ("e_trig", 22), ("e_incomplete", 6), ("magnitude", 5), ("domain", 5)]
EDGE_BUILDERS = {"e_magnitude": magnitude_tree, "e_bigrational": bigrational_tree,
                 "e_periodic": periodic_tree, "e_domain": domain_edge_tree, "e_trig": trig_edge_tree,
                 "e_incomplete": incomplete_edge_tree}


# ── FORMAT-oriented values ─────────────────────────────────────────────────
def _tie_literal(rng):
    """A decimal literal whose digit after a FIX cut is exactly 5 (exact tie)."""
    ip = rng.choice(["0", "1", "2", "9", "12", "99", "999", str(rng.randint(0, 9999))])
    nd = rng.randint(1, 9)
    fp = "".join(rng.choice("0123456789") for _ in range(nd - 1))
    if rng.random() < 0.3:
        fp = "9" * (nd - 1)  # carry propagation
    return ip + "." + fp + "5"


def _near_tie_literal(rng):
    """x.yyy4999999999999 / x.yyy5000000000001: the 12-digit companion rounds
    onto the tie, so FIX rounding from the companion can go the wrong way."""
    ip = rng.choice(["0", "1", "2", "12", "123"])
    nd = rng.randint(1, 5)
    fp = "".join(rng.choice("0123456789") for _ in range(nd))
    if rng.random() < 0.5:
        tail = "4" + "9" * rng.randint(9, 14)
    else:
        tail = "5" + "0" * rng.randint(9, 13) + "1"
    return ip + "." + fp + tail


def fmt_tree(rng, angle, g):
    x = rng.random()
    if x < 0.22:
        lit = _tie_literal(rng)
        t = R(Num(lit))
        if rng.random() < 0.3:
            t = R(Op("-"), Num(lit))
        return t
    if x < 0.34:
        lit = _near_tie_literal(rng)
        return R(Op("-"), Num(lit)) if rng.random() < 0.3 else R(Num(lit))
    if x < 0.50:
        # terminating rationals p/(2^a 5^b): exact ties at some FIX place
        q = (2 ** rng.randint(0, 7)) * (5 ** rng.randint(0, 4))
        if q == 1:
            q = 8
        p = rng.randint(1, 4 * q)
        t = R(Frac(R(Num(str(p))), R(Num(str(q)))))
        return R(Op("-"), *t.items) if rng.random() < 0.3 else t
    if x < 0.60:
        return periodic_tree(rng, angle, g)
    if x < 0.80:
        # irrationals scaled by 10^k
        core = rng.choice([R(Sqrt(R(Num(str(rng.choice([2, 3, 5, 7, 10, 99, 101])))))),
                           R(Const("pi")), R(Const("e")),
                           R(Fn("ln", R(Num(str(rng.choice([2, 3, 7, 10])))))),
                           R(Pow(Const("e"), R(Num(str(rng.randint(2, 30)))))),
                           R(Pow(Const("pi"), R(Num(str(rng.randint(2, 20)))))),
                           R(Root(R(Num("3")), R(Num(str(rng.choice([2, 3, 10, 100]))))))])
        k = rng.choice([-12, -9, -6, -5, -3, -1, 0, 0, 1, 3, 5, 8, 11, 12, 15])
        if k:
            return R(*core.items, Op("*"), *sci("1", k))
        return core
    if x < 0.90:
        # values with long carries: 9.99...95, 0.000...5 and integers
        return R(Num(rng.choice(["9.9999999995", "99.995", "0.0005", "0.00049999", "999999.5",
                                 "0.5", "1.5", "2.5", "1234567.89", "0.000000000125",
                                 "123456789012", "1234567890123", "99999999999.5"])))
    return None  # fall back to the common generator


FMT_FAMILIES = [("fmt_value", 70), ("general", 10), ("rational", 6), ("irrational", 6), ("decimal", 4),
                ("magnitude", 2), ("trig", 2)]


def fmt_builder(rng, angle, g):
    t = fmt_tree(rng, angle, g)
    if t is None:
        g2 = Gen(rng, rng.choice(["general", "irrational", "decimal"]), angle)
        t = g2.tree(rng.choice([1, 2, 3]))
    return t
