"""Calculation surface driver: script building and per-case analysis.

Per process: header (boot, open Calculation) + preamble that stores A–E with
real keys (and verifies each by recall). Per case and keystroke path:

    key ac / key ac          reset (a 2nd AC also closes a stray FORMAT menu)
    set_angle_mode deg|rad
    [Ans pre-step]           type a known value, ENTER, AC
    <path keys>  key enter
    dump_calc <case>|<path>

After the direct path, optional I4 round trips: S<->D toggles, a FORMAT-menu
choice, history recall, Ans recall, STO F / recall F.
"""
from __future__ import annotations

import random

import sympy as sp

from common import keys as K
from common.canon import giac as canon_giac
from common.evaluator import Ctx, Value, evaluate, _nonfinite
from common.expr import DIRECT_TOOLBOX, TOOLBOX_TOOLBOX, linear, to_struct
from common.gen import VAR_VALUES_TEXT
from common.invariants import (Finding, check_serialization, compare_value, visual)
from common.sexpr import linear as slinear, parse

ENV = {"A": sp.Integer(3), "B": sp.Rational(1, 2), "C": sp.Integer(-4),
       "D": sp.sqrt(2), "E": sp.Rational(3, 4)}
VALUE_KEYS = {
    "3": ["3"], "1/2": ["/", "1", "right", "2", "right"], "-4": ["neg", "4"],
    "sqrt(2)": ["sqrt", "2", "right"], "3/4": ["/", "3", "right", "4", "right"],
    "5": ["5"], "2/3": ["/", "2", "right", "3", "right"], "-7": ["neg", "7"],
    "1.25": ["1", ".", "2", "5"],
}
ANS_VALUES = {"5": sp.Integer(5), "2/3": sp.Rational(2, 3), "-7": sp.Integer(-7),
              "1.25": sp.Rational(5, 4)}
CANON_ENV_TEXT = {"A": "3", "B": "1/2", "C": "-4", "D": "sqrt(2)", "E": "3/4"}
PATHS = ("direct", "toolbox", "edited", "naive")
FORMAT_ORDER = ["Standard (exact)", "Decimal", "Recurring decimal", "Extended decimal",
                "Scientific (SCI)", "Engineering (ENG)", "Mixed fraction", "Prime factors",
                "Polar / phasor", "Exponential form", "Fixed decimals (FIX)"]


def key_lines(names):
    return [f"key {n}" for n in names]


def preamble() -> list:
    lines = []
    for var, digit in zip("ABCDE", "12345"):
        lines += ["key ac", "key ac"] + key_lines(VALUE_KEYS[VAR_VALUES_TEXT[var]])
        lines += ["key enter", "key sto", f"key {digit}"]
    for var, digit in zip("ABCDE", "12345"):
        lines += ["key ac", "key ac", "key alpha", f"key {digit}", "key enter", f"dump_calc pre|{var}"]
    return lines


def check_preamble(by_tag) -> list:
    bad = []
    for var in "ABCDE":
        d = by_tag.get(f"pre|{var}")
        if not d or d.get("status") != "ok":
            bad.append(var)
            continue
        v = evaluate(parse(d["display"]), Ctx(env=ENV)) if d.get("display") else None
        if v is None or v.kind != "finite" or not sp.simplify(v.principal - ENV[var]) == 0:
            bad.append(var)
    return bad


def ctx_for(case) -> Ctx:
    env = dict(ENV)
    if case.get("ans"):
        env["Ans"] = ANS_VALUES[case["ans"]]
    return Ctx(angle=case["angle"], env=env)


def predict_formats(value: Value):
    """Predict the FORMAT menu entries (enum order) for a finite result."""
    if value.kind != "finite":
        return []
    v = value.principal
    out = ["Standard (exact)", "Decimal"]
    if v.free_symbols:
        return ["Standard (exact)"]
    rational = v.is_Rational and abs(v.p) < 2**63 and v.q < 2**63
    real = v.is_real
    if rational and v.q > 1:
        out += ["Recurring decimal", "Extended decimal"]
    if real:
        out += ["Scientific (SCI)", "Engineering (ENG)"]
    if rational and v.q > 1 and abs(v.p) >= v.q:
        out.append("Mixed fraction")
    if rational and v.q == 1 and 1 < abs(v.p) <= 10**12:
        out.append("Prime factors")
    if not real:
        out += ["Polar / phasor", "Exponential form"]
    if real:
        out.append("Fixed decimals (FIX)")
    return out


def lite(oracle) -> dict:
    """Pickle-cheap summary of an oracle Value for the main process.
    (Pickling SymPy results between processes was measured to dominate the
    run time, so full values never leave the worker that computes them.)"""
    if isinstance(oracle, dict):
        return oracle
    return {"kind": oracle.kind, "text": oracle.text(), "formats": predict_formats(oracle)}


def i4_plan(case, oracle):
    rng = random.Random(case["id"] + "|i4")
    formats = lite(oracle)["formats"]
    fmts = [f for f in formats
            if f in ("Decimal", "Recurring decimal", "Scientific (SCI)", "Engineering (ENG)",
                     "Mixed fraction", "Fixed decimals (FIX)")]
    plan = {"fmt": None, "fix": None}
    if fmts:
        plan["fmt"] = rng.choice(fmts)
        plan["fmt_index"] = formats.index(plan["fmt"])
        if plan["fmt"] == "Fixed decimals (FIX)":
            plan["fix"] = rng.randint(0, 9)
    return plan


def case_lines(case, paths, oracle, do_i4: bool, tree_dump: bool = False):
    tree = case["tree"]
    lines = [f"# case {case['id']} {case['family']} {case['angle']} :: {linear(tree)}"]
    meta = {}
    for path in paths:
        toks, info = K.project(tree, path, random.Random(case["id"] + "|" + path))
        if toks is None:
            meta[path] = {"skipped": info.get("skip")}
            continue
        meta[path] = {"keys": K.to_compact(toks), **info}
        lines += ["key ac", "key ac", f"set_angle_mode {case['angle']}"]
        if case.get("ans"):
            lines += key_lines(VALUE_KEYS[case["ans"]]) + ["key enter", "key ac"]
        lines += K.to_lines(toks)
        lines += ["key enter", f"dump_calc {case['id']}|{path}" + (" tree" if tree_dump else "")]
        if path == "direct" and do_i4 and lite(oracle)["kind"] == "finite":
            plan = i4_plan(case, oracle)
            meta["i4"] = plan
            lines += ["key format", f"dump_calc {case['id']}|sd",
                      "key format", f"dump_calc {case['id']}|sd2"]
            if plan["fmt"]:
                lines += ["key shift", "key alpha", "key format"] + ["key down"] * plan["fmt_index"] + ["key enter"]
                if plan["fix"] is not None:
                    lines += ["key up"] * 9 + ["key down"] * plan["fix"] + ["key enter"]
                lines += [f"dump_calc {case['id']}|fmt"]
            # UP while a result is shown recalls the newest history entry
            # (AC first would turn UP into cursor movement: navigateHistory
            # only runs with a visible result or inside history).
            lines += ["key up", f"dump_calc {case['id']}|hist",
                      "key ac", "key ac", "key ans", "key enter", f"dump_calc {case['id']}|ans",
                      "key sto", "key 6", "key ac", "key ac", "key alpha", "key 6", "key enter",
                      f"dump_calc {case['id']}|sto"]
    return lines, meta


def oracle_of(case) -> Value:
    return evaluate(to_struct(case["tree"]), ctx_for(case))


def _desync(d) -> str | None:
    if d is None:
        return "missing dump"
    if not d.get("ok") or d.get("app") != "Calculation":
        return f"app={d.get('app')}"
    if d.get("toolbox") or d.get("format_menu"):
        return "modal open at dump"
    return None


def analyze(case, by_tag, meta, oracle: Value | None = None):
    """Return (record, findings, desync_reasons)."""
    ctx = ctx_for(case)
    if oracle is None or isinstance(oracle, dict):
        oracle = oracle_of(case)
    rec = {"id": case["id"], "family": case["family"], "angle": case["angle"],
           "tree": linear(case["tree"]), "oracle": oracle.text(), "oracle_kind": oracle.kind,
           "oracle_notes": oracle.notes, "ans": case.get("ans"), "paths": {}}
    findings, desync = [], []
    results = {}
    for path in PATHS:
        m = meta.get(path)
        if not m or m.get("skipped"):
            continue
        d = by_tag.get(f"{case['id']}|{path}")
        why = _desync(d)
        if why:
            desync.append(f"{path}: {why}")
            rec["paths"][path] = {"desync": why}
            continue
        try:
            inp = parse(d["input"])
        except Exception as exc:  # noqa: BLE001
            desync.append(f"{path}: input unparsable {exc}")
            continue
        intended = to_struct(case["tree"], TOOLBOX_TOOLBOX if path == "toolbox" else DIRECT_TOOLBOX)
        same = inp == intended
        disp_oracle = oracle if same else evaluate(inp, ctx)
        prec = {"keys": m["keys"], "input": slinear(inp), "giac_in": d.get("giac_in"),
                "status": d.get("status"), "exact": d.get("exact"), "approx": d.get("approx"),
                "display": slinear(parse(d["display"])) if d.get("display") else d.get("display_label"),
                "structure_as_intended": same}
        if path == "naive":
            prec["displayed_oracle"] = disp_oracle.text()
        verdict, sev, info = compare_value(disp_oracle, d, ctx)
        prec["I1"] = verdict
        if verdict in ("I1", "I5"):
            findings.append(Finding(case["id"], path, verdict, sev, info.get("reason", ""),
                                    dict(info, keys=m["keys"], input=slinear(inp), giac_in=d.get("giac_in"),
                                         tree=rec["tree"])))
        ser = check_serialization(d, disp_oracle, ctx)
        if ser and ser.get("mismatch"):
            wrong_shown = verdict == "I1"
            findings.append(Finding(case["id"], path, "I2", "S1" if wrong_shown else "S2",
                                    "Giac-bound text does not denote the displayed input",
                                    dict(ser, keys=m["keys"], input=slinear(inp), giac_in=d.get("giac_in"),
                                         tree=rec["tree"])))
        elif ser and ser.get("unparsed"):
            prec["giac_unparsed"] = ser["unparsed"]
        if path != "naive" and not same:
            vsame = visual(inp) == visual(intended)
            vals_equal = (disp_oracle.kind == oracle.kind and
                          (oracle.kind != "finite" or _same_value(disp_oracle, oracle)))
            sev = "S1" if (vsame and not vals_equal) else ("S4" if vals_equal else "S2")
            findings.append(Finding(case["id"], path, "I2" if path == "direct" else "I3", sev,
                                    "keystrokes produced a different structure than intended"
                                    + (" (renders identically)" if vsame else ""),
                                    {"keys": m["keys"], "intended": slinear(intended), "input": slinear(inp),
                                     "intended_visual": visual(intended), "input_visual": visual(inp),
                                     "values_equal": vals_equal, "tree": rec["tree"]}))
        if path == "naive" and not same:
            prec["naive_divergence"] = {"intended_value": oracle.text(),
                                        "displayed_value": disp_oracle.text(),
                                        "renders_identically": visual(inp) == visual(intended)}
            if visual(inp) == visual(intended):
                findings.append(Finding(case["id"], path, "I2", "S1",
                                        "naive close-bracket typing builds a different tree that renders identically",
                                        {"keys": m["keys"], "intended": slinear(intended), "input": slinear(inp)}))
        results[path] = (inp, d)
        rec["paths"][path] = prec
    # I3: canonical paths agree on the result (structures are checked
    # against each path's own expected structure above)
    if "direct" in results:
        din, dd = results["direct"]
        for path in ("toolbox", "edited"):
            if path in results:
                pin, pd = results[path]
                if (pd.get("status"), pd.get("exact")) != (dd.get("status"), dd.get("exact")):
                    findings.append(Finding(case["id"], path, "I3", "S2",
                                            "same structure, different result across paths",
                                            {"direct": [dd.get("status"), dd.get("exact")],
                                             path: [pd.get("status"), pd.get("exact")]}))
    # I4 round trips (only when the direct result is a finite, OK value)
    if "direct" in results and meta.get("i4"):
        din, dd = results["direct"]
        # Round trips are judged only from a result that itself agreed with
        # the oracle (a NaN/garbage result is not committed to Ans, by design).
        base_ok = dd.get("status") == "ok" and rec["paths"].get("direct", {}).get("I1") == "ok"
        rec["i4"] = {}
        for tag in ("sd", "sd2", "fmt", "hist", "ans", "sto"):
            d = by_tag.get(f"{case['id']}|{tag}")
            if d is None:
                continue
            why = _desync(d)
            if why:
                desync.append(f"{tag}: {why}")
                continue
            r = {"format": d.get("format_name"), "status": d.get("status"),
                 "display": slinear(parse(d["display"])) if d.get("display") else d.get("display_label")}
            if not base_ok:
                rec["i4"][tag] = r
                continue
            if tag in ("sd", "sd2", "fmt", "hist"):
                if d.get("exact") != dd.get("exact") or d.get("status") != dd.get("status"):
                    findings.append(Finding(case["id"], tag, "I4", "S2",
                                            f"{tag}: canonical exact result changed",
                                            {"before": dd.get("exact"), "after": d.get("exact")}))
            if tag == "sd2" and d.get("display") != dd.get("display"):
                findings.append(Finding(case["id"], tag, "I4", "S2", "S<->D twice does not restore the display",
                                        {"before": dd.get("display"), "after": d.get("display")}))
            if tag == "hist" and (d.get("input") != dd.get("input") or d.get("display") != dd.get("display")):
                findings.append(Finding(case["id"], tag, "I4", "S2", "history recall differs",
                                        {"input_before": dd.get("input"), "input_after": d.get("input"),
                                         "display_before": dd.get("display"), "display_after": d.get("display")}))
            verdict, sev, info = compare_value(oracle, d, ctx)
            r["I1"] = verdict
            if verdict in ("I1", "I5"):
                findings.append(Finding(case["id"], tag, "I4" if tag in ("ans", "sto", "hist") else "I1",
                                        sev, f"{tag}: " + info.get("reason", ""), dict(info, tree=rec["tree"])))
            rec["i4"][tag] = r
    return rec, findings, desync


def _same_value(a: Value, b: Value) -> bool:
    from common.evaluator import close
    try:
        return any(close(x, y) for x in a.finite() for y in b.finite())
    except Exception:  # noqa: BLE001
        return False


def canonical_giac(case) -> str | None:
    env_text = dict(CANON_ENV_TEXT)
    if case.get("ans"):
        env_text["Ans"] = case["ans"]
    try:
        return canon_giac(case["tree"], env_text)
    except ValueError:
        return None
