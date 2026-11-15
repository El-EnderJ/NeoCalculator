"""W1 I4 round-trip surface, surface name `calculation.rt`.

Generator: the common families (common.gen) with case ids `r<seed>-<i>`.
Per case (direct keystroke path only), after the result:

  1  AC AC, `1 + 1` ENTER, UP UP      -> hist2   recalled input == original input
     ENTER                            -> histev  re-evaluated result == oracle, exact unchanged
  2  S<->D (FORMAT key)               -> sd      decimal view (I1 format-aware)
     STO <V> while the DECIMAL view is shown, AC AC, ALPHA <V> ENTER
                                      -> rcl     the CANONICAL value is stored (not the 12-digit
                                                 decimal shown): exact == original exact
  3  AC AC, 2 × Ans ENTER             -> ans2    2·oracle (Ans holds the recalled value)
  4  V in A–E: retype the preamble value, ENTER, STO <V>, recall
                                      -> restore V back to its preamble value
V is drawn from A–F per case. Judged only when the direct result itself
matched the oracle (status ok, I1 ok).
"""
from __future__ import annotations

import random

import sympy as sp

from common import keys as K
from common.evaluator import Value, close
from common.expr import linear, to_struct
from common.gen import VAR_VALUES_TEXT, generate as _generate
from common.invariants import Finding, compare_value, giac_text_value
from common.sexpr import linear as slinear, parse

from .. import driver
from .. import surface as base

NAME = "calculation.rt"
PATHS = ("direct",)
oracle_of = base.oracle_of
oracle_lite = base.oracle_lite
header = base.header
check_header = base.check_header
third_voice = base.third_voice
triage = base.triage
VARS = "ABCDEF"


def generate(seed, index):
    c = _generate(seed, index)
    c["id"] = f"r{seed}-{index}"
    return c


def _var(case):
    return random.Random(case["id"] + "|rtvar").choice(VARS)


def case_lines(case, oracle, do_i4):
    return case_lines_for(case, oracle, PATHS, do_i4)


def case_lines_for(case, oracle, paths, do_i4, tree_dump=False):
    cid = case["id"]
    toks, _ = K.project(case["tree"], "direct")
    v = _var(case)
    dk = K.VAR_KEY[v]
    lines = [f"# case {cid} {case['family']} {case['angle']} :: {linear(case['tree'])} (V={v})",
             "key ac", "key ac", f"set_angle_mode {case['angle']}"]
    if case.get("ans"):
        lines += driver.key_lines(driver.VALUE_KEYS[case["ans"]]) + ["key enter", "key ac"]
    lines += K.to_lines(toks) + ["key enter", f"dump_calc {cid}|direct" + (" tree" if tree_dump else "")]
    lines += ["key ac", "key ac", "key 1", "key +", "key 1", "key enter", "key up", "key up",
              f"dump_calc {cid}|hist2", "key enter", f"dump_calc {cid}|histev"]
    lines += ["key format", f"dump_calc {cid}|sd", "key sto", f"key {dk}",
              "key ac", "key ac", "key alpha", f"key {dk}", "key enter", f"dump_calc {cid}|rcl"]
    lines += ["key ac", "key ac", "key 2", "key *", "key ans", "key enter", f"dump_calc {cid}|ans2"]
    if v in VAR_VALUES_TEXT:
        lines += ["key ac", "key ac"] + driver.key_lines(driver.VALUE_KEYS[VAR_VALUES_TEXT[v]])
        lines += ["key enter", "key sto", f"key {dk}", "key ac", "key ac", "key alpha", f"key {dk}",
                  "key enter", f"dump_calc {cid}|restore"]
    meta = {"direct": {"keys": K.to_compact(toks)}, "var": v}
    return lines, meta


def _val(text, ctx):
    if not text:
        return None
    v, err = giac_text_value(text, ctx)
    return None if err else v


def analyze(case, by_tag, meta, oracle=None):
    ctx = driver.ctx_for(case)
    if oracle is None or isinstance(oracle, dict):
        oracle = driver.oracle_of(case)
    cid = case["id"]
    rec = {"id": cid, "family": case["family"], "angle": case["angle"], "tree": linear(case["tree"]),
           "oracle": oracle.text(), "oracle_kind": oracle.kind, "oracle_notes": oracle.notes,
           "ans": case.get("ans"), "var": meta.get("var"), "paths": {}, "rt": {}}
    findings, desync = [], []
    d = by_tag.get(f"{cid}|direct")
    why = driver._desync(d)
    if why:
        return rec, findings, [f"direct: {why}"]
    inp = parse(d["input"])
    v, sev, info = compare_value(oracle, d, ctx)
    rec["paths"]["direct"] = {"keys": meta["direct"]["keys"], "input": slinear(inp), "status": d.get("status"),
                              "exact": d.get("exact"), "I1": v,
                              "structure_as_intended": inp == to_struct(case["tree"])}
    if inp != to_struct(case["tree"]):
        return rec, findings, ["direct: structure differs from intended (typing desync)"]
    if v in ("I1", "I5"):
        findings.append(Finding(cid, "direct", v, sev, info.get("reason", ""),
                                dict(info, keys=meta["direct"]["keys"], giac_in=d.get("giac_in"), tree=rec["tree"])))
    if v != "ok" or d.get("status") != "ok" or oracle.kind != "finite":
        return rec, findings, desync
    exact0 = d.get("exact")
    if case.get("ans"):
        # Re-evaluating the recalled entry uses the CURRENT Ans (= 2, from the
        # intervening 1+1): that is the documented meaning of Ans, so the
        # round trip is judged against the oracle with Ans = 2.
        from common.evaluator import Ctx, evaluate
        ctx = Ctx(angle=case["angle"], env=dict(ctx.env, Ans=sp.Integer(2)))
        oracle = evaluate(to_struct(case["tree"]), ctx)
        if oracle.kind != "finite":
            return rec, findings, desync
        exact0 = None
    e0 = _val(exact0, ctx)

    def get(tag):
        x = by_tag.get(f"{cid}|{tag}")
        w = driver._desync(x)
        if w:
            desync.append(f"{tag}: {w}")
            return None
        return x

    def judge(tag, dd, want: Value, inv="I4"):
        vv, ss, ii = compare_value(want, dd, ctx)
        rec["rt"][tag] = {"I1": vv, "status": dd.get("status"), "exact": dd.get("exact"),
                          "format": dd.get("format_name"),
                          "display": slinear(parse(dd["display"])) if dd.get("display") else dd.get("display_label")}
        if vv in ("I1", "I5"):
            findings.append(Finding(cid, tag, inv, ss, f"{tag}: " + ii.get("reason", ""),
                                    dict(ii, tree=rec["tree"], keys=meta["direct"]["keys"], var=meta.get("var"))))
        return vv

    h = get("hist2")
    if h is not None:
        if h.get("input") != d.get("input"):
            findings.append(Finding(cid, "hist2", "I4", "S2", "hist2: UP UP does not recall the original input",
                                    {"before": slinear(inp), "after": slinear(parse(h["input"])) if h.get("input") else None,
                                     "tree": rec["tree"]}))
    he = get("histev")
    if he is not None:
        judge("histev", he, oracle)
        if exact0 is not None and he.get("exact") != exact0:
            findings.append(Finding(cid, "histev", "I4", "S2", "histev: re-evaluated history entry changes the exact result",
                                    {"before": exact0, "after": he.get("exact"), "tree": rec["tree"]}))
    if exact0 is None and he is not None:
        exact0 = he.get("exact")
        e0 = _val(exact0, ctx)
    sd = get("sd")
    if sd is not None:
        judge("sd", sd, oracle, inv="I1")
    rc = get("rcl")
    if rc is not None:
        judge("rcl", rc, oracle)
        ev = _val(rc.get("exact"), ctx)
        if e0 is not None and ev is not None and "." not in (exact0 or "") and \
                not close(ev, e0, rel=sp.Rational(1, 10**25)):
            findings.append(Finding(cid, "rcl", "I4", "S2" if close(ev, e0, rel=sp.Rational(1, 10**9)) else "S1",
                                    "rcl: STO from the decimal view stores a different value than the canonical result",
                                    {"before": exact0, "recalled_exact": rc.get("exact"), "var": meta.get("var"),
                                     "tree": rec["tree"]}))
    a2 = get("ans2")
    if a2 is not None:
        judge("ans2", a2, Value("finite", [2 * c for c in oracle.finite()]))
    rs = by_tag.get(f"{cid}|restore")
    if rs is not None and meta.get("var") in driver.ENV:
        if driver._desync(rs) or rs.get("status") != "ok":
            desync.append("restore failed")
        else:
            vv, _, _ = compare_value(Value("finite", [driver.ENV[meta["var"]]]), rs, ctx)
            if vv != "ok":
                desync.append(f"restore of {meta['var']} does not hold the preamble value")
    return rec, findings, desync
