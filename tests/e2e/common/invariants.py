"""Independent invariant checkers producing structured findings.

I1 value        displayed result == oracle(displayed input), format-aware.
I2 WYSIWYC      (a) input AST == intended structure (canonical paths);
                (b) the Giac-bound text denotes the displayed input.
I3 path         every canonical path (direct/toolbox/edited) gives the same
                AST and result.
I4 round trips  Ans, history recall, STO/RCL, FORMAT toggles keep the value.
I5 no silent    NumOS refusal/undefined only where the oracle agrees.
    loss

Severity: S1 wrong value shown as valid; S2 right value shown misleadingly
(structure/sign/digits) or WYSIWYC broken without value change; S3 refusal
where a correct answer exists; S4 cosmetic / policy note.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
from fractions import Fraction

import sympy as sp

from . import giacparse
from .evaluator import Ctx, Value, _nonfinite, close, evaluate, numeric
from .sexpr import as_row, linear, parse

DECIMAL_FORMATS = {"Decimal", "Scientific (SCI)", "Engineering (ENG)", "Fixed decimals (FIX)"}


@dataclass
class Finding:
    case: str
    path: str
    invariant: str
    severity: str
    title: str
    detail: dict = field(default_factory=dict)

    def key(self):
        return (self.case, self.path, self.invariant)

    def to_dict(self):
        return asdict(self)


def visual(node) -> str:
    """What the eye sees: adjacent numbers concatenate, products are silent."""
    if node is None:
        return "null"
    if isinstance(node, str):
        return node
    h = node[0]
    if h == "row":
        return "".join(visual(c) for c in node[1]) or "□"
    a = node[1:]
    if h == "num":
        return a[0]
    if h == "op":
        return {"Add": "+", "Sub": "-", "Mul": "×", "Div": "÷"}.get(a[0], a[0])
    if h == "frac":
        return "\\frac{" + visual(as_row(a[0])) + "}{" + visual(as_row(a[1])) + "}"
    if h == "pow":
        return visual(as_row(a[0])) + "^{" + visual(as_row(a[1])) + "}"
    if h == "sqrt":
        return "\\sqrt{" + visual(as_row(a[0])) + "}"
    if h == "root":
        return "\\sqrt[" + visual(as_row(a[1])) + "]{" + visual(as_row(a[0])) + "}"
    if h == "paren":
        l, r = {"bar": "||", "bracket": "[]"}.get(a[0], "()")
        return l + visual(as_row(a[1])) + r
    if h == "fn":
        return a[0] + "(" + visual(as_row(a[1])) + ")"
    if h == "logb":
        return "log_{" + visual(as_row(a[0])) + "}(" + visual(as_row(a[1])) + ")"
    return linear(node)


# ── display analysis ────────────────────────────────────────────────────────
def decimal_literal_info(text: str):
    """(exact Rational, ulp Rational, significant digits) of a decimal literal."""
    t = text
    exp = 0
    if "e" in t.lower():
        t, e = t.lower().split("e")
        exp = int(e)
    if "." in t:
        ip, fp = t.split(".")
    else:
        ip, fp = t, ""
    value = sp.Rational((ip or "0") + ("." + fp if fp else "")) * sp.Integer(10) ** exp
    ulp = sp.Integer(10) ** (exp - len(fp))
    sig = len((ip + fp).lstrip("0")) or 1
    return value, ulp, sig


def single_decimal(display):
    """If the display is one real decimal number (sign, mantissa, ×10^k),
    return (value, ulp, sig). Else None."""
    if not display or display[0] != "row":
        return None
    items = list(display[1])
    sign = 1
    if items and items[0] == ("op", "Sub"):
        sign = -1
        items = items[1:]
    if not items or items[0][0] != "num":
        return None
    v, ulp, sig = decimal_literal_info(items[0][1])
    rest = items[1:]
    if not rest:
        return sign * v, ulp, sig
    if (len(rest) == 2 and rest[0] == ("op", "Mul") and rest[1][0] == "pow"
            and as_row(rest[1][1]) == ("row", (("num", "10"),))):
        er = as_row(rest[1][2])[1]
        esign = 1
        if er and er[0] == ("op", "Sub"):
            esign, er = -1, er[1:]
        if len(er) == 1 and er[0][0] == "num" and er[0][1].isdigit():
            k = esign * int(er[0][1])
            scale = sp.Integer(10) ** k
            return sign * v * scale, ulp * scale, sig
    return None


def min_decimal_digits(node):
    """Smallest significant-digit count among decimal literals (None if none)."""
    best = None
    stack = [node]
    while stack:
        n = stack.pop()
        if isinstance(n, tuple):
            if n and n[0] == "num" and "." in n[1]:
                s = decimal_literal_info(n[1])[2]
                best = s if best is None else min(best, s)
            stack.extend(x for x in n[1:] if isinstance(x, tuple))
            if n and n[0] == "row":
                stack.extend(n[1])
    return best


def _polar(st, ctx):
    """'r ∠ θ unit' (Polar / phasor format) -> r·e^{iθ} (θ in rad/°/gon)."""
    items = list(st[1])
    k = next((i for i, e in enumerate(items) if e[0] == "sym" and "∠" in e[1]), None)
    if k is None:
        return None
    unit = items[-1][1] if items[-1][0] == "sym" else " rad"
    theta_items = items[k + 1:-1] if items[-1][0] == "sym" else items[k + 1:]
    r = evaluate(("row", tuple(items[:k])), ctx)
    t = evaluate(("row", tuple(theta_items)), ctx)
    if r.kind != "finite" or t.kind != "finite":
        return Value("unsupported", detail="polar parts")
    th = t.principal
    if "°" in unit:
        th = th * sp.pi / 180
    elif "gon" in unit:
        th = th * sp.pi / 200
    return Value("finite", [r.principal * sp.exp(sp.I * th)])


def shown_value(dump: dict, ctx: Ctx) -> tuple[Value, object]:
    """Value of what the result area shows (structured canvas or text label)."""
    if dump.get("display") is not None:
        st = parse(dump["display"])
        if dump.get("format_name") == "Polar / phasor":
            pv = _polar(st, Ctx(angle=ctx.angle, env=ctx.env))
            if pv is not None:
                return pv, st
        c = Ctx(angle=ctx.angle, env=ctx.env, mixed=dump.get("format_name") == "Mixed fraction")
        return evaluate(st, c), st
    label = dump.get("display_label")
    if label:
        text = label.split("): ", 1)[-1]
        try:
            v = giacparse.parse_eval(text, ctx.angle, ctx.env)
            kind = "nonfinite" if _nonfinite(v) else "finite"
            return Value(kind, [v], ["text fallback label"]), None
        except Exception as exc:  # noqa: BLE001
            return Value("unsupported", detail=f"label: {exc}"), None
    return Value("unsupported", detail="no display"), None


def _match_any(cands, shown, rel):
    return any(close(c, shown, rel=rel) for c in cands)


def compare_value(oracle: Value, dump: dict, ctx: Ctx):
    """Core I1/I5 decision for one dump. Returns (verdict, severity, info)
    where verdict in {'ok','I1','I5','skip'}."""
    status = dump.get("status")
    info = {"oracle": oracle.text(), "oracle_notes": oracle.notes, "status": status,
            "exact": dump.get("exact"), "approx": dump.get("approx"),
            "format": dump.get("format_name")}
    if oracle.kind == "unsupported":
        return "skip", None, dict(info, reason="oracle unsupported: " + oracle.detail)
    if status != "ok":
        if oracle.kind == "incomplete":
            if status == "parse_error":
                return "ok", None, info
            return "I5", "S4", dict(info, reason=f"incomplete input reported as {status}")
        if oracle.kind == "nonfinite" or any(_nonfinite(c) for c in oracle.candidates):
            return "ok", None, info
        return "I5", "S3", dict(info, reason=f"NumOS status {status} but oracle has a finite value")
    if oracle.kind == "incomplete":
        return "I5", "S2", dict(info, reason="NumOS evaluated a structurally incomplete input")
    disp = str(dump.get("display") or "") + str(dump.get("display_label") or "")
    if "nan" in disp.lower().replace("unsupported", ""):
        if oracle.kind == "finite" and len(oracle.finite()) == len(oracle.candidates):
            return "I5", "S3", dict(info, reason="NaN shown as the result (status ok) where a finite value exists",
                                    display=dump.get("display"))
        return "ok", None, info
    shown, st = shown_value(dump, ctx)
    info["shown"] = shown.text()
    info["display"] = linear(st) if st is not None else dump.get("display_label")
    if shown.kind == "unsupported":
        return "skip", None, dict(info, reason="display not parseable: " + shown.detail)
    if shown.kind == "incomplete":
        return "I1", "S2", dict(info, reason="display structurally incomplete")
    o_fin = oracle.finite()
    if shown.kind == "nonfinite":
        if oracle.kind == "nonfinite" or len(o_fin) < len(oracle.candidates):
            return "ok", None, info
        return "I1", "S1", dict(info, reason="non-finite result shown for a finite value")
    if not o_fin:
        return "I1", "S1", dict(info, reason="finite value shown where oracle is undefined/pole")
    sv = shown.principal
    fmt = dump.get("format_name")
    sd = single_decimal(st) if st is not None else None
    if sd is not None and (fmt in DECIMAL_FORMATS or "." in str(dump.get("display"))):
        value, ulp, sig = sd
        best = None
        import mpmath
        for c in o_fin:
            if c.free_symbols:
                continue
            n = numeric(c)
            if n is None:
                continue
            with mpmath.workdps(60):
                if abs(n.imag) > mpmath.mpf("1e-45") * max(1, abs(n.real)):
                    continue
                shown_mp = mpmath.mpf(value.p) / mpmath.mpf(value.q)
                err = sp.Float(mpmath.nstr(abs(n.real - shown_mp), 30), 30)
            best = err if best is None or err < best else best
        info["ulp"] = str(ulp)
        if best is None:
            return "I1", "S1", dict(info, reason="real decimal shown for a non-real value")
        info["abs_error_vs_ulp"] = str(sp.N(best / ulp, 6))
        if best <= ulp / 2 * (1 + sp.Rational(1, 10**9)):
            return "ok", None, info
        if best <= ulp * (1 + sp.Rational(1, 10**9)):
            return "I1", "S2", dict(info, reason="displayed decimal digits off by one unit in the last place (rounding)")
        if abs(value) > 0 and best / abs(value) < sp.Rational(1, 10**9):
            return "I1", "S2", dict(info, reason="displayed digits claim more precision than the value carries "
                                                 "(error beyond the last shown digit, relative error < 1e-9)")
        return "I1", "S1", dict(info, reason="displayed decimal value wrong beyond its digits")
    if fmt == "Extended decimal" and st is not None:
        d = min_decimal_digits(st)
        rel = sp.Rational(1, 10 ** max((d or 30) - 2, 6))
    else:
        d = min_decimal_digits(st) if st is not None else None
        rel = sp.Rational(1, 10 ** 30) if d is None else sp.Rational(5, 10 ** max(d - 1, 5))
    if _match_any(o_fin, sv, rel):
        return "ok", None, info
    return "I1", "S1", dict(info, reason="displayed value differs from oracle", rel_tol=str(rel))


def giac_text_value(giac_in: str, ctx: Ctx):
    try:
        return giacparse.parse_eval(giac_in, ctx.angle, ctx.env), None
    except Exception as exc:  # noqa: BLE001
        return None, f"{type(exc).__name__}: {exc}"


def check_serialization(dump: dict, displayed_oracle: Value, ctx: Ctx):
    """I2(b): does the Giac-bound text denote the DISPLAYED input?"""
    if not dump.get("giac_complete") or displayed_oracle.kind in ("incomplete", "unsupported"):
        return None
    gv, err = giac_text_value(dump.get("giac_in", ""), ctx)
    if err:
        return {"unparsed": err}
    if _nonfinite(gv):
        if displayed_oracle.kind == "nonfinite" or len(displayed_oracle.finite()) < len(displayed_oracle.candidates):
            return None
        return {"mismatch": True, "giac_value": str(gv)}
    if displayed_oracle.kind == "nonfinite":
        return {"mismatch": True, "giac_value": str(gv), "oracle": "nonfinite"}
    if _match_any(displayed_oracle.candidates, gv, sp.Rational(1, 10**25)):
        return None
    return {"mismatch": True, "giac_value": str(sp.N(gv, 20)) if not gv.free_symbols else str(gv),
            "oracle": displayed_oracle.text()}
