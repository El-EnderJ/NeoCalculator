"""Per-dump judgement for the Calculus surface (I1, I2b, I5, +C, variable).

judge(op, angle, struct, dump) compares what NumOS SHOWS (the result canvas
AST, or the exact-text fallback label) with the oracle of the DISPLAYED input:

  derivative   numos(x)  vs  d/dv f(x)               (SymPy diff, real symbol)
  integral     d/dv numos(x) (C dropped)  vs  f(x)   (any antiderivative form)

at fixed generic sample points (mpmath, 40 digits), plus a bounded SymPy
simplification (recorded, never decisive). v is the variable NumOS reports
(printed on screen as f(v)); the documented rule "first authored x/y" is
checked separately, as is the choice of an integral's bound variable.

Oracle candidate set (README policy): (-a)^(p/q) with q odd may be read as the
principal complex power OR the real root; a result is accepted if it matches
either reading at every compared point. Constant definite integrals are
resolved exactly by SymPy (divergent -> undefined) and cross-checked with
mpmath quad.

Readings used to triage a disagreement (each rescue is its own class):
  * user   : UI angle mode for trig in the result, VPAM radical = real root
  * rad    : trig in the result read in radians (what Giac computed)
  * proot  : result radical read as principal x^(1/n) (Giac Root node)
  * giac   : Giac exact text under Giac semantics (radians)
"""
from __future__ import annotations

import re

import sympy as sp

from common.sexpr import linear as slinear, parse

from .semantics import (CS, XR, YR, compare, derivative, evaluate_expr, free_var_order,
                        giac_text_value, numeric_at, symbolic_equal, SAMPLES)

EXACT_REL = 1e-20
FLOAT_REL = 1e-9
SPECIAL_FUNCS = (sp.erf, sp.erfi, sp.Si, sp.Ci, sp.Ei, sp.li, sp.fresnels, sp.fresnelc, sp.gamma,
                 sp.uppergamma, sp.lowergamma, sp.hyper, sp.meijerg, sp.polylog, sp.LambertW, sp.Shi,
                 sp.Chi, sp.expint)


def _strip_c(st):
    """Remove a trailing `+ C` from the result row. Returns (struct, had_c)."""
    if not st or st[0] != "row":
        return st, False
    items = list(st[1])
    if len(items) >= 2 and items[-1] == ("var", "C") and items[-2] == ("op", "Add"):
        return ("row", tuple(items[:-2])), True
    return st, False


def _has_var_c(st):
    if st == ("var", "C"):
        return True
    if isinstance(st, tuple):
        return any(_has_var_c(c) for c in st if isinstance(c, tuple))
    return False


def _neg_only(cmp):
    diffs = [p for p in cmp["points"] if p[1] == "DIFF"]
    return bool(diffs) and all(p[0].startswith("-") for p in diffs)


def _verdict(cmp):
    if cmp["differ"]:
        return "differ"
    if cmp["agree"] >= 2 or (cmp["agree"] >= 1 and cmp["skipped"] + cmp["unsupported"] >= len(SAMPLES) - 1):
        return "agree"
    return "unjudged"


def _target(op, f, v):
    return derivative(f, v) if op == "diff" else f


def _numos_side(op, shown, v):
    return shown if op == "diff" else derivative(shown, v)


def oracle_defined_somewhere(f):
    for x0 in SAMPLES:
        n = numeric_at(f, x0)
        if n is not None and not isinstance(n, str):
            return True
    return False


def merge_adjacent_numbers(n):
    """What the eye reads: consecutive Number nodes in a row concatenate (3 LEFT 5 -> '53')."""
    if not isinstance(n, tuple) or not n:
        return n
    if n[0] == "row":
        out = []
        for c in n[1]:
            c = merge_adjacent_numbers(c)
            if out and isinstance(c, tuple) and c[:1] == ("num",) and out[-1][:1] == ("num",):
                out[-1] = ("num", out[-1][1] + c[1])
            else:
                out.append(c)
        return ("row", tuple(out))
    return (n[0],) + tuple(merge_adjacent_numbers(c) if isinstance(c, tuple) else c for c in n[1:])


def real_points(f):
    pts = []
    for x0 in SAMPLES:
        n = numeric_at(f, x0)
        if n is None or isinstance(n, str):
            continue
        if abs(n.imag) <= 1e-25 * max(1, abs(n.real)):
            pts.append(x0)
    return tuple(pts)


def try_antiderivative(f, v, seconds=8):
    """SymPy's own attempt (bounded). Returns (expr|None, note)."""
    from common.timeout import Overrun, call
    try:
        F = call(lambda: sp.integrate(f, v), seconds)
    except Overrun:
        return None, "sympy integrate timeout"
    except Exception as exc:  # noqa: BLE001
        return None, f"sympy integrate error {type(exc).__name__}"
    if F.has(sp.Integral):
        return None, "sympy cannot integrate either"
    c = compare(derivative(F, v), f)
    if _verdict(c) != "agree":
        return None, "sympy antiderivative not verified"
    return F, "sympy finds and verifies an antiderivative"


def _calc_text(t):
    return any(k in t for k in ("integrate(", "diff(", "derive(", "int("))


def judge(op, angle, struct, dump, want_symbolic=False):
    """Return dict with verdict in {ok, finding, skip} and findings: list of
    (invariant, severity, klass, title, detail)."""
    out = {"findings": [], "notes": []}
    f, kind, detail, _ = evaluate_expr(struct, angle, definite=True)
    var_any, var_free = free_var_order(struct)
    nv = dump.get("variable") or "x"
    v = XR if nv == "x" else YR
    status = dump.get("status")
    state = dump.get("state")
    exact = dump.get("exact") or ""
    out.update({"oracle_kind": kind, "variable": nv, "rule_variable": var_any or "x"})

    def find(inv, sev, klass, title, **det):
        out["findings"].append((inv, sev, klass, title, det))

    def done(verdict=None):
        out["verdict"] = verdict or ("finding" if out["findings"] else "ok")
        return out

    # ── input-side checks ──────────────────────────────────────────────────
    if kind == "unsupported":
        out["skip"] = "oracle unsupported: " + detail
        return done("skip")
    if state == "editing":
        if not struct[1]:
            return done("ok")      # empty input: NumOS keeps editing ("Empty input")
        find("I5", "S3", "no-result", "EXE left the editor without a result", state=state)
        return done()
    if kind == "incomplete":
        if status != "parse_error":
            find("I5", "S2", "evaluated-incomplete", f"structurally incomplete input reported as {status}",
                 exact=exact)
        return done()
    merged = merge_adjacent_numbers(struct)
    if merged != struct:
        fm = evaluate_expr(merged, angle, definite=True)[0]
        same_val = f is not None and fm is not None and compare(fm, f)["differ"] == 0
        out["notes"].append("adjacent number nodes render concatenated")
        if not same_val:
            find("I2", "S1", "adjacent-number-merge",
                 "the input shows adjacent digits as one number (e.g. 53) but the tree holds two numbers "
                 "that are computed as a product", visual=slinear(merged), computed=str(f)[:120],
                 visual_value=str(fm)[:120])
    if nv != (var_any or "x"):
        find("I2", "S2", "variable-rule", "variable differs from the first authored x/y",
             numos=nv, rule=var_any or "x")
    if var_free and var_any and var_free != var_any and nv == var_any:
        find("I2", "S2", "bound-variable-chosen",
             "the variable chosen is an integral's bound variable while a free variable exists "
             "(needs policy decision)", numos=nv, free=var_free)

    # I2(b): the Giac-bound text denotes the displayed input
    gi = dump.get("giac_in") or ""
    if gi and status != "parse_error" and kind == "finite":
        try:
            gv = giac_text_value(gi, angle)
            c = compare(gv, f, rel=EXACT_REL)
            out["i2b"] = _verdict(c)
            if c["differ"]:
                find("I2", "S1", "serializer", "Giac-bound text does not denote the displayed input",
                     giac_in=gi, points=c["points"])
        except Exception as exc:  # noqa: BLE001
            out["i2b"] = f"unparsed: {exc}"[:120]

    # ── refusals ───────────────────────────────────────────────────────────
    if status != "ok":
        if kind == "nonfinite" or detail.startswith("policy:") or (f is not None and not oracle_defined_somewhere(f)):
            out["notes"].append(f"refusal agreed ({status})")
            return done()
        target = _target(op, f, v)
        tdef = target is not None and oracle_defined_somewhere(target)
        sev, klass = "S3", "refusal"
        if status == "unsupported" and "too long" in (dump.get("diag") or "") + exact:
            sev, klass = "S4", "input-limit"
        find("I5", sev, klass, f"NumOS status {status} where the oracle has a value",
             diag=dump.get("diag"), oracle_target=str(target)[:200], target_defined=tdef)
        return done()

    # ── undefined input shown with a value (divergent integrals, ...) ─────
    if kind == "nonfinite":
        shown_nonfinite = bool(re.search(r"infinity|undef|\binf\b|\boo\b", exact))
        if not shown_nonfinite:
            find("I1", "S1", "undefined-shown-finite",
                 "the input is undefined (divergent definite integral or undefined everywhere) "
                 "but a valid-looking result is shown", exact=exact,
                 display=slinear(parse(dump["display"])) if dump.get("display") not in (None, "null") else None)
        return done()

    # ── unevaluated ──────────────────────────────────────────────────────
    false_uneval = False
    if dump.get("unevaluated"):
        if not _calc_text(exact):
            false_uneval = True
            find("I1", "S2", "false-unevaluated",
                 "a computed result is labelled 'unevaluated' and shown only as exact text "
                 "(containsCalculusOperation budget)", exact=exact[:300])
        elif op == "diff":
            find("I5", "S3", "unevaluated-derivative", "derivative left unevaluated", exact=exact)
            return done()
        else:
            F, note = try_antiderivative(f, v)
            out["notes"].append(note)
            if F is not None:
                special = F.has(*SPECIAL_FUNCS)
                find("I5", "S3", "unevaluated-integral" + ("-special" if special else ""),
                     "integral left unevaluated where a closed-form antiderivative exists ("
                     + ("special functions" if special else "elementary") + "; capability)",
                     exact=exact, sympy=str(F)[:300])
            return done()

    # ── value ─────────────────────────────────────────────────────────────
    disp = dump.get("display")
    st = parse(disp) if disp and disp != "null" and not false_uneval else None
    if st is None:
        try:
            shown_user = giac_text_value(exact, "rad")
        except Exception as exc:  # noqa: BLE001
            out["skip"] = f"text fallback unparsed: {exc}"[:160]
            return done("skip")
        shown_rad = shown_proot = shown_user
        out["notes"].append("text fallback")
    else:
        if '(call "NTHROOT"' in disp:
            find("I1", "S4", "nthroot-name",
                 "result display shows Giac's internal call NTHROOT(n, a) instead of a radical",
                 display=slinear(st), exact=exact)
        st2, had_c = _strip_c(st)
        if op == "diff" and had_c and not _has_var_c(struct):
            find("I1", "S2", "spurious-C", "derivative shown with + C", display=slinear(st))
        if op == "int" and not had_c:
            find("I1", "S2", "missing-C", "antiderivative shown without + C", display=slinear(st))
        shown_user, k1, d1, _ = evaluate_expr(st2, angle, "surd")
        if shown_user is None:
            out["skip"] = f"display not evaluable: {d1}"
            return done("skip")
        shown_rad = evaluate_expr(st2, "rad", "surd")[0] if angle == "deg" else shown_user
        shown_proot = evaluate_expr(st2, angle, "principal")[0]
    if shown_user.has(CS):
        shown_user = shown_user.subs(CS, 0)
    floaty = "." in exact or bool(disp and re.search(r'\(num "[0-9]*\.', disp))
    rel = FLOAT_REL if floaty else EXACT_REL
    targets = [("principal", _target(op, f, v))]
    f_real = evaluate_expr(struct, angle, powers="real", definite=True)[0]
    if f_real is not None and f_real != f:
        targets.append(("real-branch", _target(op, f_real, v)))
    if targets[0][1] is None:
        out["skip"] = "oracle derivative failed"
        return done("skip")
    ns = _numos_side(op, shown_user, v)
    real_pts = real_points(f)
    pts = real_pts if len(real_pts) >= 2 else SAMPLES
    out["real_points"] = len(real_pts)
    best = None
    for name, target in targets:
        if target is None:
            continue
        c = compare(ns, target, rel=rel, points=pts)
        if best is None or (_verdict(c) == "agree" and _verdict(best[1]) != "agree"):
            best = (name, c, target)
        if _verdict(c) == "agree":
            break
    name, c_user, target = best
    out["cmp_user"] = {k: c_user[k] for k in ("agree", "differ", "skipped", "unsupported")}
    vu = _verdict(c_user)
    out["value"] = vu
    if name != "principal" and vu == "agree":
        out["notes"].append("matches the real-branch candidate (q-odd rational power)")
    if vu == "unjudged":
        if c_user["unsupported"]:
            out["skip"] = "result not numerically evaluable at the sample points"
            return done("skip") if not out["findings"] else done()
        out["notes"].append("all sample points outside the common domain")
        return done()
    if vu == "agree":
        if want_symbolic:
            out["symbolic_equal"] = symbolic_equal(ns, target)
        if len(pts) < len(SAMPLES):
            c_all = compare(ns, target, rel=rel)
            if c_all["differ"]:
                out["notes"].append("differs only where the input is non-real (outside the real domain)")
                find("I1", "S4", "complex-region-only",
                     "result valid on the real domain of the input, differs where the input is non-real "
                     "(policy note, real-mode antiderivative/derivative)", exact=exact[:200],
                     points=c_all["points"])
        return done()
    # disagreement: triage the readings (same point set)
    target = targets[0][1]

    tri = {"points_user": c_user["points"]}
    rescued = None
    if angle == "deg" and shown_rad is not None:
        c_rad = compare(_numos_side(op, shown_rad, v), target, rel=rel, points=pts)
        tri["rad_reading"] = _verdict(c_rad)
        if _verdict(c_rad) == "agree":
            rescued = "deg-display"
    if rescued is None and shown_proot is not None and shown_proot != shown_user:
        c_pr = compare(_numos_side(op, shown_proot, v), target, rel=rel, points=pts)
        tri["principal_root_reading"] = _verdict(c_pr)
        if _verdict(c_pr) == "agree":
            rescued = "root-reading"
    try:
        gx = giac_text_value(exact, "rad")
        c_g = None
        for _n, tg in targets:
            c_g = compare(_numos_side(op, gx.subs(CS, 0), v), tg, rel=rel, points=pts)
            if _verdict(c_g) == "agree":
                break
        tri["giac_exact"] = _verdict(c_g)
    except Exception as exc:  # noqa: BLE001
        tri["giac_exact"] = f"unparsed {exc}"[:80]
    tri["negative_x_only"] = _neg_only(c_user)
    tri["symbolic_equal"] = symbolic_equal(ns, target)
    disp_txt = slinear(st) if st is not None else "Exact: " + exact
    common = dict(display=disp_txt, exact=exact, target=str(target)[:300], triage=tri)
    if rescued == "deg-display":
        find("I1", "S2", "deg-display",
             "DEG mode: result trig is in radian form (cos(πx/180)) but the screen is in DEG; "
             "read in the mode shown the formula is a different function (needs policy decision)", **common)
    elif rescued == "root-reading":
        find("I1", "S2", "root-reading",
             "result radical denotes Giac's principal x^(1/n), not the real root the same "
             "radical means in the input (differs for negative radicands; needs policy decision)", **common)
    elif tri["giac_exact"] == "agree" and st is not None:
        find("I1", "S1", "display-vs-exact",
             "Giac's exact text is right but the result canvas shows a different expression "
             "(result tree -> MathAST conversion)", **common)
    elif len(real_pts) < 2:
        find("I1", "S1", "value-nonreal-domain",
             "input is non-real at (almost) every sample point and the shown result is not an "
             "antiderivative/derivative there either (needs policy decision: real-mode refusal expected?)", **common)
    elif tri["negative_x_only"]:
        find("I1", "S1", "value-negative-x",
             "displayed result wrong for negative x only (branch/domain; check policy)", **common)
    else:
        find("I1", "S1", "value", "displayed result differs from the oracle", **common)
    return done()
