"""W3 Grapher core: script building, dump/residual parsing and the oracle of a
DISPLAYED slot AST (relation split, x/y variables, real-plot semantics).

Overrides of common/ (kept local, documented in RUNS.md):
  * adjacent number nodes `(num "5") (num "3")` are read as the digit string
    "53" (what the Grapher editor renders and what GrapherApp serializes),
    whereas common/evaluator reads juxtaposition as implicit multiplication.
  * variable `y` is supported through Ctx.env.
"""
from __future__ import annotations

import math
import re
import sys
from pathlib import Path

import sympy as sp

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common.evaluator import Ctx, X, _nonfinite, evaluate  # noqa: E402
from common.sexpr import parse  # noqa: E402

Y = sp.Symbol("y")
REL_OPS = ("Eq", "Lt", "Gt", "Le", "Ge")
# the Grapher editor stores inequality symbols as variable atoms
REL_VARS = {"<": "Lt", ">": "Gt", "≤": "Le", "≥": "Ge"}
EMU = "C:/.piobuild/e2e-01/emulator_pc/program.exe"
_PASS = re.compile(r"\[ASSERT\] .*?:(\d+): PASS - assert_graph_eval_near (\d+) \(([^)]*)\) = (\S+)")
_FAIL = re.compile(r"\[ASSERT\] .*?:(\d+): FAIL - (.*)")


def merge_nums(node):
    """Merge adjacent num nodes in every row (display reading)."""
    if not isinstance(node, tuple):
        return node
    if node and node[0] == "row":
        out = []
        for it in node[1]:
            it = merge_nums(it)
            if (out and isinstance(it, tuple) and it[:1] == ("num",) and isinstance(out[-1], tuple)
                    and out[-1][:1] == ("num",) and "." not in out[-1][1] + it[1]):
                out[-1] = ("num", out[-1][1] + it[1])
            else:
                out.append(it)
        return ("row", tuple(out))
    return tuple(merge_nums(c) if isinstance(c, tuple) else c for c in node)


def split_relation(inp):
    """Row -> (lhs_row, op, rhs_row) at top level, or (None, None, row)."""
    items = inp[1] if inp and inp[0] == "row" else ()
    idx = [i for i, it in enumerate(items) if isinstance(it, tuple) and (
        (it[:1] == ("op",) and it[1] in REL_OPS) or (it[:1] == ("var",) and it[1] in REL_VARS))]
    if len(idx) != 1:
        return None, None, inp
    i = idx[0]
    op = items[i][1] if items[i][0] == "op" else REL_VARS[items[i][1]]
    return ("row", tuple(items[:i])), op, ("row", tuple(items[i + 1:]))


def oracle_expr(row, angle="rad"):
    """Value of a displayed row (sympy candidates, may contain x and y)."""
    return evaluate(merge_nums(row), Ctx(angle=angle, env={"y": Y}))


def real_candidates(value, subs):
    """Real float candidates of `value` at subs; None entries = invalid point."""
    if value.kind != "finite":
        return [None]
    out = []
    for c in value.candidates:
        try:
            v = c.subs(subs) if subs else c
            if _nonfinite(v):
                out.append(None)
                continue
            n = complex(sp.N(v, 30))
        except Exception:  # noqa: BLE001
            out.append(None)
            continue
        if abs(n.imag) > 1e-9 * max(1.0, abs(n.real)):
            out.append(None)
        else:
            out.append(n.real)
    return out or [None]


def agree(numos, cands, rel=2e-5, absol=2e-5):
    """NumOS float (or None/NaN = invalid) agrees with any candidate."""
    if numos is not None and isinstance(numos, float) and math.isnan(numos):
        numos = None
    for c in cands:
        if c is None and numos is None:
            return True
        if c is not None and numos is not None:
            if not math.isfinite(c):
                continue
            if abs(numos - c) <= absol + rel * abs(c):
                return True
            # float32 sampling: allow 1 ulp-ish of a float32 at large magnitude
            if abs(c) > 1e6 and abs(numos - c) <= 2.5e-7 * abs(c):
                return True
    return False


def residual_lines(slot, pts):
    return [f"assert_graph_eval_near {slot} {x} {y} 0 1e38" for x, y in pts]


def parse_residuals(stdout, stderr):
    """[(slot, x, y, value)] from PASS lines; NaN stops the script (FAIL)."""
    out = []
    for m in _PASS.finditer(stdout):
        x, y = (float(v) for v in m.group(3).split(","))
        out.append((int(m.group(2)), x, y, float(m.group(4))))
    fails = [m.group(2) for m in _FAIL.finditer(stderr)]
    return out, fails


def keyline(tok):
    if tok.startswith("<") and tok.endswith(">"):
        return f"sdl_text {tok[1:-1]}"
    return f"key {tok}"


def tb(query):
    return ["toolbox", "up", "right", "right", "right", "enter", f"<{query}>", "down", "enter"]


# ── point-wise oracle (policy sets applied at the sample point) ─────────────
from common.evaluator import Evaluator, Incomplete, Unsupported, Value  # noqa: E402


class _PointEval(Evaluator):
    def __init__(self, ctx, xv, yv):
        super().__init__(ctx)
        self.xv, self.yv = xv, yv

    def _var(self, name):
        if name == "x" and self.xv is not None:
            return [self.xv]
        if name == "y" and self.yv is not None:
            return [self.yv]
        return super()._var(name)


def point_value(row, angle, xv=None, yv=None):
    """Value of a displayed row with x/y replaced by exact sample numbers BEFORE
    evaluation, so 0^0, 0/0, ln(0)… follow the documented candidate policy at
    that point instead of being simplified away symbolically."""
    ctx = Ctx(angle=angle, env={"y": Y})
    try:
        vals = _PointEval(ctx, xv, yv).eval(merge_nums(row))
    except Incomplete as exc:
        return Value("incomplete", detail=str(exc))
    except Unsupported as exc:
        return Value("unsupported", detail=str(exc))
    except (ZeroDivisionError, ValueError, TypeError, OverflowError, RecursionError) as exc:
        return Value("unsupported", detail=f"{type(exc).__name__}: {exc}")
    finite = [v for v in vals if not _nonfinite(v)]
    nonfin = [v for v in vals if _nonfinite(v)]
    if not finite:
        return Value("nonfinite", nonfin)
    return Value("finite", finite + nonfin)


def point_reals(row, angle, xv=None, yv=None):
    """Real float candidates (None = invalid point) at the sample point."""
    v = point_value(row, angle, xv, yv)
    if v.kind == "nonfinite":
        return [None]
    if v.kind != "finite":
        return []          # unsupported/incomplete: no judgement
    return real_candidates(v, None)


def _nonreal(v):
    try:
        if _nonfinite(v):
            return False
        c = complex(sp.N(v, 25))
        return abs(c.imag) > 1e-20 * max(1.0, abs(c.real))
    except Exception:  # noqa: BLE001
        return False


def _point_eval_with_real_policy(self, node):
    """Real-plot policy: a non-real intermediate value makes 'undefined' an
    accepted outcome (the Grapher evaluates in the reals), alongside the
    principal-complex result when that happens to be real."""
    vals = Evaluator.eval(self, node)
    if node is not None and node[0] not in ("row", "num", "const", "var", "op") and any(_nonreal(v) for v in vals):
        if not any(_nonfinite(v) for v in vals):
            vals = list(vals) + [sp.nan]
    return vals


_PointEval.eval = _point_eval_with_real_policy


# ── IEEE double "second oracle" (identifies float artefacts, not truth) ─────
def float_eval(node, angle, xv=None, yv=None):
    import math as m
    deg = angle == "deg"
    nan = float("nan")

    def div(a, b):
        if b == 0:
            return nan if a == 0 or a != a else m.copysign(m.inf, a) * (1 if m.copysign(1, b) > 0 else -1)
        return a / b

    def pw(a, b):
        try:
            r = m.pow(a, b)
            return r
        except (ValueError, ZeroDivisionError):
            return nan
        except OverflowError:
            return m.inf

    def row(items):
        seq, expect, neg = [], True, False
        for e in items:
            if e[0] == "op":
                if expect:
                    if e[1] == "Sub":
                        neg = not neg
                    continue
                seq.append(e[1])
                expect = True
                continue
            v = ev(e)
            if neg:
                v, neg = -v, False
            if not expect:
                seq.append("Mul")
            seq.append(v)
            expect = False
        if not seq:
            return nan
        terms, cur, pend, k = [], seq[0], "Add", 1
        while k + 1 < len(seq):
            op, rhs = seq[k], seq[k + 1]
            if op in ("Mul", "Div"):
                cur = cur * rhs if op == "Mul" else div(cur, rhs)
            else:
                terms.append((pend, cur))
                pend, cur = op, rhs
            k += 2
        terms.append((pend, cur))
        t = 0.0
        for op, v in terms:
            t = t + v if op == "Add" else t - v
        return t

    def slot(n):
        return row(n[1]) if n and n[0] == "row" else ev(n)

    def fn(name, v):
        try:
            if name in ("sin", "cos", "tan"):
                return getattr(m, name)(v * m.pi / 180 if deg else v)
            if name == "ln":
                return m.log(v) if v > 0 else (-m.inf if v == 0 else nan)
            if name == "log":
                return m.log10(v) if v > 0 else (-m.inf if v == 0 else nan)
        except (ValueError, OverflowError):
            return nan
        return nan

    def ev(n):
        h = n[0]
        if h == "row":
            return row(n[1])
        if h == "num":
            t = n[1]
            return float(("0" + t) if t.startswith(".") else t)
        if h == "const":
            return {"pi": m.pi, "e": m.e}[n[1]]
        if h == "var":
            return float(xv) if n[1] == "x" else float(yv)
        if h == "paren":
            v = slot(n[2])
            return abs(v) if n[1] == "bar" else v
        if h == "frac":
            return div(slot(n[1]), slot(n[2]))
        if h == "pow":
            return pw(slot(n[1]), slot(n[2]))
        if h == "sqrt":
            v = slot(n[1])
            return m.sqrt(v) if v >= 0 else nan
        if h == "fn":
            return fn(n[1], slot(n[2]))
        if h == "logb":
            b, a = slot(n[1]), slot(n[2])
            try:
                return m.log(a) / m.log(b)
            except (ValueError, ZeroDivisionError):
                return nan
        return nan

    try:
        v = ev(merge_nums(node))
    except (OverflowError, ValueError, ZeroDivisionError, TypeError, KeyError):
        return float("nan")
    return v
