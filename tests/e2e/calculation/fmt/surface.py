"""W1 dedicated FORMAT surface (FIX / SCI / ENG heavy), surface name `calculation.fmt`.

Two phases per case (driven by calculation/run_w1.py, W1Runner):
  A  type the case, ENTER, open the FORMAT menu and dump its real choice list
     (`menu_choices`, enum values) — no prediction of availability needed;
  B  type the case again, then a deterministic plan of FORMAT selections by
     exact menu index: FIX at 3 places (one chosen where the oracle value is
     closest to a rounding tie), SCI, ENG (+ engineering shift LEFT/RIGHT),
     Decimal, Recurring, Extended, Mixed, Polar/Exponential when offered; then
     back to Standard (I4: exact unchanged) and Ans (I4: canonical value).

Checks per format dump: I1 (format-aware, common.invariants.compare_value),
the format actually applied, FIX shows exactly `places` decimals, SCI mantissa
in [1,10), ENG exponent multiple of 3 (plus 3*shift), exact text unchanged.
"""
from __future__ import annotations

import random

import mpmath
import sympy as sp

from common.emulator import script_header
from common.evaluator import Ctx, Value, evaluate, numeric
from common.expr import linear, to_struct
from common.invariants import Finding, compare_value, single_decimal
from common import keys as K
from common.sexpr import linear as slinear, parse

from .. import driver
from ..w1gen import FMT_FAMILIES, fmt_builder, generate_weighted

NAME = "calculation.fmt"
PATHS = ("direct",)
TWO_PHASE = True
F_STANDARD, F_DECIMAL, F_PERIODIC, F_EXTENDED, F_SCI, F_ENG, F_MIXED, F_PRIME, F_POLAR, F_EXPO, F_FIX = range(11)
LABEL = {0: "Standard (exact)", 1: "Decimal", 2: "Recurring decimal", 3: "Extended decimal",
         4: "Scientific (SCI)", 5: "Engineering (ENG)", 6: "Mixed fraction", 7: "Prime factors",
         8: "Polar / phasor", 9: "Exponential form", 10: "Fixed decimals (FIX)"}
TAGNAME = {1: "dec", 2: "rec", 3: "ext", 4: "sci", 5: "eng", 6: "mix", 8: "polar", 9: "expo"}


def generate(seed, index):
    return generate_weighted(seed, index, FMT_FAMILIES, prefix="f",
                             builders={"fmt_value": fmt_builder}, magnitude_limit=1e80)


def oracle_of(case):
    return driver.oracle_of(case)


def oracle_lite(value):
    d = driver.lite(value)
    if not isinstance(value, dict) and value.kind == "finite":
        try:
            n = numeric(value.principal)
            if n is not None:
                with mpmath.workdps(60):
                    d["re"] = mpmath.nstr(n.real, 45)
                    d["im"] = mpmath.nstr(n.imag, 45)
        except Exception:  # noqa: BLE001
            pass
    return d


def header():
    return script_header("Calculation") + driver.preamble()


def check_header(by_tag):
    return [f"variable {v} not stored as expected" for v in driver.check_preamble(by_tag)]


def _type_lines(case):
    toks, _ = K.project(case["tree"], "direct")
    lines = ["key ac", "key ac", f"set_angle_mode {case['angle']}"]
    if case.get("ans"):
        lines += driver.key_lines(driver.VALUE_KEYS[case["ans"]]) + ["key enter", "key ac"]
    return lines + K.to_lines(toks) + ["key enter"], K.to_compact(toks)


def tie_places(re_text: str, rng: random.Random):
    """FIX places ordered by closeness of the value to a rounding tie."""
    try:
        with mpmath.workdps(60):
            x = abs(mpmath.mpf(re_text))
            dist = []
            for p in range(10):
                frac = mpmath.frac(x * mpmath.mpf(10) ** p)
                dist.append((float(abs(frac - mpmath.mpf("0.5"))), p))
        dist.sort()
        return [p for _, p in dist]
    except Exception:  # noqa: BLE001
        ps = list(range(10))
        rng.shuffle(ps)
        return ps


def plan(case, oracle):
    """List of ops: (tag, enum, fix_places|None, eng_shift|None)."""
    rng = random.Random(case["id"] + "|fmtplan")
    menu = case.get("menu") or []
    ops = []
    lite = oracle if isinstance(oracle, dict) else oracle_lite(oracle)
    if F_FIX in menu:
        order = tie_places(lite.get("re", "0"), rng)
        places = [order[0]]
        rest = [p for p in range(10) if p != order[0]]
        rng.shuffle(rest)
        places += rest[:2]
        for p in places:
            ops.append((f"fix{p}", F_FIX, p, None))
    for f in (F_SCI, F_ENG, F_DECIMAL, F_PERIODIC, F_EXTENDED, F_MIXED, F_POLAR, F_EXPO):
        if f in menu:
            if f == F_ENG:
                s = rng.choice([0, 0, -1, 1, -2, 2])
                ops.append(("eng", F_ENG, None, s))
            else:
                ops.append((TAGNAME[f], f, None, None))
    return ops


def case_lines(case, oracle, do_i4):
    return case_lines_for(case, oracle, PATHS, do_i4)


def case_lines_for(case, oracle, paths, do_i4, tree_dump=False):
    cid = case["id"]
    tl, keys = _type_lines(case)
    lines = [f"# case {cid} {case['family']} {case['angle']} :: {linear(case['tree'])}"] + tl
    lines += [f"dump_calc {cid}|direct" + (" tree" if tree_dump else "")]
    meta = {"direct": {"keys": keys}}
    if "menu" not in case:  # phase A
        lines += ["key shift", "key alpha", "key format", f"dump_calc {cid}|menu", "key ac"]
        meta["phase"] = "A"
        return lines, meta
    meta["phase"] = "B"
    menu = case["menu"]
    ops = plan(case, oracle)
    meta["ops"] = ops
    for tag, f, p, s in ops:
        idx = menu.index(f)
        lines += ["key shift", "key alpha", "key format"] + ["key up"] * 14 + ["key down"] * idx + ["key enter"]
        if p is not None:
            lines += ["key up"] * 9 + ["key down"] * p + ["key enter"]
        lines += [f"dump_calc {cid}|{tag}"]
        if s:
            lines += [("key left" if s < 0 else "key right")] * abs(s)
            lines += [f"dump_calc {cid}|{tag}{s:+d}"]
    if menu:
        lines += ["key shift", "key alpha", "key format"] + ["key up"] * 14 + ["key enter", f"dump_calc {cid}|std"]
    lines += ["key ac", "key ac", "key ans", "key enter", f"dump_calc {cid}|ans"]
    return lines, meta


def _desync(d):
    return driver._desync(d)


def _exact_value(text, ctx):
    from common.invariants import giac_text_value
    if not text:
        return None
    v, err = giac_text_value(text, ctx)
    return None if err else v


def _decimals(lit: str) -> int:
    t = lit.lower().split("e")[0]
    return len(t.split(".")[1]) if "." in t else 0


def _shape_checks(tag, f, p, s, d, st):
    """Format-shape checks independent of the value. Returns list of reasons."""
    out = []
    if st is None or st[0] != "row":
        return out
    items = list(st[1])
    if items and items[0] == ("op", "Sub"):
        items = items[1:]
    if not items or items[0][0] != "num":
        return out
    lit = items[0][1]
    pow10 = None
    if len(items) == 3 and items[1] == ("op", "Mul") and items[2][0] == "pow":
        er = items[2][2]
        er = er[1] if er and er[0] == "row" else (er,)
        sign = 1
        if er and er[0] == ("op", "Sub"):
            sign, er = -1, er[1:]
        if len(er) == 1 and er[0][0] == "num" and er[0][1].isdigit():
            pow10 = sign * int(er[0][1])
    if f == F_FIX and len(items) == 1 and _decimals(lit) != p:
        out.append(f"FIX {p} shows {_decimals(lit)} decimals ({lit})")
    if f == F_SCI and pow10 is not None:
        m = abs(float(lit))
        if m != 0 and not (1 <= m < 10):
            out.append(f"SCI mantissa {lit} outside [1,10)")
    if f == F_ENG and pow10 is not None:
        if pow10 % 3:
            out.append(f"ENG exponent {pow10} not a multiple of 3")
        m = abs(float(lit))
        if not s and m != 0 and not (1 <= m < 1000):
            out.append(f"ENG mantissa {lit} outside [1,1000) at shift 0")
    return out


def analyze(case, by_tag, meta, oracle=None):
    ctx = driver.ctx_for(case)
    if oracle is None or isinstance(oracle, dict):
        oracle = driver.oracle_of(case)
    rec = {"id": case["id"], "family": case["family"], "angle": case["angle"], "tree": linear(case["tree"]),
           "oracle": oracle.text(), "oracle_kind": oracle.kind, "oracle_notes": oracle.notes,
           "ans": case.get("ans"), "paths": {}, "fmt": {}}
    findings, desync = [], []
    d = by_tag.get(f"{case['id']}|direct")
    why = _desync(d)
    if why:
        return rec, findings, [f"direct: {why}"]
    inp = parse(d["input"])
    intended = to_struct(case["tree"])
    v, sev, info = compare_value(oracle, d, ctx)
    rec["paths"]["direct"] = {"keys": meta["direct"]["keys"], "input": slinear(inp), "giac_in": d.get("giac_in"),
                              "status": d.get("status"), "exact": d.get("exact"), "approx": d.get("approx"),
                              "display": slinear(parse(d["display"])) if d.get("display") else d.get("display_label"),
                              "I1": v, "structure_as_intended": inp == intended}
    if inp != intended:
        desync.append("direct: structure differs from intended (typing desync)")
        return rec, findings, desync
    if v in ("I1", "I5"):
        findings.append(Finding(case["id"], "direct", v, sev, info.get("reason", ""),
                                dict(info, keys=meta["direct"]["keys"], giac_in=d.get("giac_in"), tree=rec["tree"])))
    if meta.get("phase") == "A":
        m = by_tag.get(f"{case['id']}|menu")
        rec["menu"] = (m or {}).get("menu_choices")
        return rec, findings, desync
    if v != "ok" or d.get("status") != "ok":
        return rec, findings, desync  # formats are judged only from a correct base
    exact0, disp0 = d.get("exact"), d.get("display")
    for tag, f, p, s in meta.get("ops", []):
        for t, shift in ((tag, None),) + (((f"{tag}{s:+d}", s),) if s else ()):
            fd = by_tag.get(f"{case['id']}|{t}")
            w = _desync(fd)
            if w:
                desync.append(f"{t}: {w}")
                continue
            r = {"format": fd.get("format_name"), "fixed_places": fd.get("fixed_places"),
                 "eng_shift": fd.get("eng_shift"),
                 "display": slinear(parse(fd["display"])) if fd.get("display") else fd.get("display_label")}
            rec["fmt"][t] = r
            if fd.get("format_name") != LABEL[f]:
                findings.append(Finding(case["id"], t, "I4", "S2", f"{t}: format not applied",
                                        {"expected": LABEL[f], "got": fd.get("format_name"), "tree": rec["tree"]}))
                continue
            if f == F_FIX and str(fd.get("fixed_places")) != str(p):
                findings.append(Finding(case["id"], t, "I4", "S2", f"{t}: FIX places not applied",
                                        {"expected": p, "got": fd.get("fixed_places"), "tree": rec["tree"]}))
            if fd.get("exact") != exact0 or fd.get("status") != "ok":
                findings.append(Finding(case["id"], t, "I4", "S2", f"{t}: canonical exact result changed",
                                        {"before": exact0, "after": fd.get("exact"), "tree": rec["tree"]}))
            vv, ss, ii = _compare(oracle, fd, ctx, f)
            r["I1"] = vv
            if vv in ("I1", "I5"):
                findings.append(Finding(case["id"], t, vv, ss, f"{t}: " + ii.get("reason", ""),
                                        dict(ii, tree=rec["tree"], keys=meta["direct"]["keys"],
                                             fixed_places=fd.get("fixed_places"), eng_shift=fd.get("eng_shift"),
                                             oracle_digits=_digits(oracle))))
            st = parse(fd["display"]) if fd.get("display") else None
            for reason in _shape_checks(t, f, p, shift, fd, st):
                findings.append(Finding(case["id"], t, "I1", "S2", f"{t}: format shape: {reason}",
                                        {"display": r["display"], "tree": rec["tree"]}))
    sd = by_tag.get(f"{case['id']}|std")
    if sd is not None and not _desync(sd):
        if sd.get("exact") != exact0 or sd.get("display") != disp0:
            findings.append(Finding(case["id"], "std", "I4", "S2", "std: back to Standard does not restore the result",
                                    {"before": [exact0, disp0], "after": [sd.get("exact"), sd.get("display")],
                                     "tree": rec["tree"]}))
    ad = by_tag.get(f"{case['id']}|ans")
    if ad is not None and not _desync(ad):
        vv, ss, ii = compare_value(oracle, ad, ctx)
        rec["fmt"]["ans"] = {"I1": vv, "exact": ad.get("exact"), "format": ad.get("format_name")}
        if vv in ("I1", "I5"):
            findings.append(Finding(case["id"], "ans", "I4", ss, "ans after FORMAT changes: " + ii.get("reason", ""),
                                    dict(ii, tree=rec["tree"])))
        ev = _exact_value(ad.get("exact"), ctx)
        e0 = _exact_value(exact0, ctx)
        if ev is not None and e0 is not None and "." not in (exact0 or ""):
            from common.evaluator import close
            if not close(ev, e0, rel=sp.Rational(1, 10**25)):
                findings.append(Finding(case["id"], "ans", "I4", "S1",
                                        "ans after FORMAT changes is not the canonical value",
                                        {"before": exact0, "ans_exact": ad.get("exact"), "tree": rec["tree"]}))
    return rec, findings, desync


def _compare(oracle, fd, ctx, f):
    """compare_value, except long decimal expansions of an exact rational
    (Extended / Recurring) are checked exactly: the common numeric oracle runs
    at 50-60 digits and would misjudge a 200-digit expansion (harness
    limitation found in the smoke run, not a product issue). Truncation of
    the expansion (error <= 1 ulp) is accepted for these formats."""
    if f in (F_EXTENDED, F_PERIODIC) and oracle.kind == "finite" and oracle.principal.is_Rational and fd.get("display"):
        st = parse(fd["display"])
        sdv = single_decimal(st)
        if sdv is not None:
            value, ulp, sig = sdv
            err = abs(value - oracle.principal)
            if err <= ulp:
                return "ok", None, {"exact_check": True}
            return "I1", "S1" if err > 10 * ulp else "S2", {
                "reason": "long decimal expansion differs from the exact rational",
                "err_over_ulp": str(sp.N(err / ulp, 6)), "display": slinear(st)}
    return compare_value(oracle, fd, ctx)


def _digits(oracle: Value):
    try:
        n = numeric(oracle.principal)
        with mpmath.workdps(50):
            return mpmath.nstr(n, 40)
    except Exception:  # noqa: BLE001
        return None


def third_voice(case):
    try:
        text = driver.canonical_giac(case)
    except Exception:  # noqa: BLE001 - never let triage text kill a run
        return []
    if not text:
        return []
    return [f"set_angle_mode {case['angle']}", f"giac_eval {case['id']}|giac {text}"]


def triage(case, by_tag, record):
    from .. import surface as base
    return base.triage(case, by_tag, record)
