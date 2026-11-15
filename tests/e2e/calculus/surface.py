"""Calculus surface adapter for common.runner (W4, CORRECTNESS-E2E-01).

Per emulator process: boot + `open_app Calculus`. Per case and keystroke path:

    key ac / key ac            leave RESULT, clear the editor
    key f1 | key f2            Derivative | Integral tab (setMode keeps input)
    set_angle_mode rad|deg
    <path keys>  key enter     computeResult() is synchronous
    dump_calculus <case>|<path>

Paths: direct (RIGHT exits), toolbox (ln/log/asin/acos/atan/log_b also via
Toolbox), edited (typo + DEL / trailing / detour), naive (`)` to close
brackets — NOT expected to build the intended tree; judged against the tree
it displays). Definite integrals and diff() are Toolbox templates on every
path; |x| and n-th roots too (no keys).
"""
from __future__ import annotations

import random

from common.emulator import script_header
from common.evaluator import Value
from common.invariants import Finding, visual
from common import keys as K
from common.sexpr import linear as slinear, parse

from . import model as M
from .check import judge
from .gen import generate as _generate
from .semantics import compare, evaluate_expr, free_var_order, giac_text_value, XR, YR, CS

NAME = "calculus"
PATHS = ("direct", "toolbox", "edited", "naive")
SYMBOLIC_EVERY = 4      # bounded SymPy simplification on every Nth case (direct path)


def generate(seed, index):
    return _generate(seed, index)


def oracle_of(case):
    st = M.to_struct(case["tree"])
    f, kind, detail, _ = evaluate_expr(st, case["angle"])
    return Value(kind, [f] if f is not None else [], detail=detail)


def oracle_lite(value):
    if isinstance(value, dict):
        return value
    return {"kind": value.kind, "text": value.text() if value.candidates else value.detail}


def header():
    return script_header("Calculus")


def check_header(by_tag):
    return []


def case_lines_for(case, oracle, paths, do_i4=False, tree_dump=False):
    tree = case["tree"]
    lines = [f"# case {case['id']} {case['family']} {case['op']} {case['angle']} :: {M.linear(tree)}"]
    meta = {}
    modekey = "f1" if case["op"] == "diff" else "f2"
    for path in paths:
        toks, info = M.project(tree, path, random.Random(case["id"] + "|" + path))
        if toks is None:
            meta[path] = {"skipped": info.get("skip")}
            continue
        meta[path] = {"keys": K.to_compact(toks), **info}
        lines += ["key ac", "key ac", f"key {modekey}", f"set_angle_mode {case['angle']}"]
        lines += K.to_lines(toks)
        lines += ["key enter", f"dump_calculus {case['id']}|{path}"]
    return lines, meta


def case_lines(case, oracle, do_i4):
    return case_lines_for(case, oracle, PATHS, do_i4)


def _desync(d):
    if d is None:
        return "missing dump"
    if not d.get("ok") or d.get("app") != "Calculus":
        return f"app={d.get('app')}"
    if d.get("state") not in ("result", "editing"):
        return f"state={d.get('state')}"
    return None


def _jkey(d):
    return (d.get("input"), d.get("status"), d.get("exact"), d.get("display"), d.get("variable"),
            d.get("unevaluated"), d.get("giac_in"), d.get("state"))


def analyze(case, by_tag, meta, oracle=None):
    op, angle = case["op"], case["angle"]
    rec = {"id": case["id"], "family": case["family"], "sub": case.get("sub"), "op": op, "angle": angle,
           "tree": M.linear(case["tree"]), "paths": {}}
    findings, desync, cache, results = [], [], {}, {}
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
        intended = M.to_struct(case["tree"], M.TOOLBOX_TOOLBOX if path == "toolbox" else M.DIRECT_TOOLBOX)
        same = inp == intended
        key = _jkey(d)
        if key in cache:
            j = cache[key]
        else:
            want_sym = path == "direct" and case["index"] % SYMBOLIC_EVERY == 0
            j = judge(op, angle, inp, d, want_symbolic=want_sym)
            cache[key] = j
        prec = {"keys": m["keys"], "input": slinear(inp), "giac_in": d.get("giac_in"), "status": d.get("status"),
                "exact": d.get("exact"), "variable": d.get("variable"), "unevaluated": d.get("unevaluated"),
                "display": slinear(parse(d["display"])) if d.get("display") not in (None, "null") else None,
                "structure_as_intended": same, "verdict": j.get("verdict"), "value": j.get("value"),
                "i2b": j.get("i2b"), "notes": j.get("notes")}
        if "symbolic_equal" in j:
            prec["symbolic_equal"] = j["symbolic_equal"]
        if j.get("skip"):
            prec["skip"] = j["skip"]
        for inv, sev, klass, title, det in j["findings"]:
            findings.append(Finding(case["id"], path, inv, sev, title,
                                    dict(det, klass=klass, keys=m["keys"], input=slinear(inp),
                                         giac_in=d.get("giac_in"), exact=d.get("exact"),
                                         variable=d.get("variable"), op=op, angle=angle, tree=rec["tree"])))
        if path != "naive" and not same:
            vsame = visual(inp) == visual(intended)
            f_int = evaluate_expr(intended, angle)[0]
            f_inp = evaluate_expr(inp, angle)[0]
            vals_equal = (f_int is not None and f_inp is not None and
                          compare(f_inp, f_int)["differ"] == 0)
            sev = "S1" if (vsame and not vals_equal) else ("S4" if vals_equal else "S2")
            findings.append(Finding(case["id"], path, "I2" if path == "direct" else "I3", sev,
                                    "keystrokes produced a different structure than intended"
                                    + (" (renders identically)" if vsame else ""),
                                    {"klass": "structure-" + path, "keys": m["keys"],
                                     "intended": slinear(intended), "input": slinear(inp),
                                     "values_equal": vals_equal, "tree": rec["tree"], "op": op}))
        if path == "naive" and not same:
            prec["naive_divergence"] = {"renders_identically": visual(inp) == visual(intended)}
            if visual(inp) == visual(intended):
                findings.append(Finding(case["id"], path, "I2", "S1",
                                        "naive close-bracket typing builds a different tree that renders identically",
                                        {"klass": "naive-hidden", "keys": m["keys"], "intended": slinear(intended),
                                         "input": slinear(inp)}))
        results[path] = d
        rec["paths"][path] = prec
    # I3: canonical paths agree on the result
    if "direct" in results:
        dd = results["direct"]
        for path in ("toolbox", "edited"):
            if path in results:
                pd = results[path]
                if (pd.get("status"), pd.get("exact")) != (dd.get("status"), dd.get("exact")):
                    findings.append(Finding(case["id"], path, "I3", "S2",
                                            "same structure, different result across paths",
                                            {"klass": "path-result", "direct": [dd.get("status"), dd.get("exact")],
                                             path: [pd.get("status"), pd.get("exact")], "op": op}))
    return rec, findings, desync


def case_lines_third(case):
    return third_voice(case)


def third_voice(case):
    try:
        text = M.giac_text(case["tree"], deg=case["angle"] == "deg")
    except (ValueError, TypeError):
        return []
    any_v, _ = free_var_order(M.to_struct(case["tree"]))
    v = any_v or "x"
    cmd = f"diff({text},{v})" if case["op"] == "diff" else f"integrate({text},{v})"
    return ["set_angle_mode rad", f"giac_eval {case['id']}|giac {cmd}", f"set_angle_mode {case['angle']}"]


def triage(case, by_tag, record):
    g = by_tag.get(f"{case['id']}|giac")
    if not g:
        return {"third_voice": "unavailable"}
    out = {"canonical_giac": g.get("giac_in"), "giac_status": g.get("status"), "giac_exact": g.get("exact")}
    if g.get("status") != "ok":
        out["verdict"] = f"giac(canonical) refuses: {g.get('status')}"
        return out
    st = M.to_struct(case["tree"])
    f, kind, detail, _ = evaluate_expr(st, case["angle"], definite=True)
    if kind != "finite":
        out["verdict"] = (f"oracle: input is {kind} ({detail}); giac(canonical) returns "
                          f"{g.get('status')} {g.get('exact')!r}")
        return out
    any_v, _ = free_var_order(st)
    v = YR if any_v == "y" else XR
    try:
        gx = giac_text_value(g.get("exact") or "", "rad").subs(CS, 0)
        from .semantics import derivative
        lhs = gx if case["op"] == "diff" else derivative(gx, v)
        rhs = derivative(f, v) if case["op"] == "diff" else f
        c = compare(lhs, rhs, rel=1e-9 if "." in (g.get("exact") or "") else 1e-20)
        out["agree"], out["differ"] = c["agree"], c["differ"]
        if c["differ"]:
            out["verdict"] = "giac(canonical) != oracle -> Giac semantics or oracle policy"
        elif c["agree"]:
            out["verdict"] = "giac(canonical) == oracle -> NumOS pipeline (keys/AST/serializer/display)"
        else:
            out["verdict"] = "giac(canonical) not comparable at the sample points"
    except Exception as exc:  # noqa: BLE001
        out["verdict"] = f"giac exact unparsed: {exc}"[:200]
    return out
