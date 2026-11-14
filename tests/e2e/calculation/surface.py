"""Calculation surface adapter for common.runner."""
from __future__ import annotations

from common.emulator import script_header
from common.evaluator import close, evaluate, _nonfinite
from common.gen import generate as _generate
from common.invariants import giac_text_value
from common.expr import to_struct

from . import driver

NAME = "calculation"


def generate(seed, index):
    return _generate(seed, index)


def oracle_of(case):
    return driver.oracle_of(case)


def oracle_lite(value):
    return driver.lite(value)


def header():
    return script_header("Calculation") + driver.preamble()


def check_header(by_tag):
    bad = driver.check_preamble(by_tag)
    return [f"variable {v} not stored as expected" for v in bad]


def case_lines(case, oracle, do_i4):
    return driver.case_lines(case, driver.PATHS, oracle, do_i4)


PATHS = driver.PATHS


def case_lines_for(case, oracle, paths, do_i4, tree_dump=False):
    return driver.case_lines(case, paths, oracle, do_i4, tree_dump)


def analyze(case, by_tag, meta, oracle):
    return driver.analyze(case, by_tag, meta, oracle)


def third_voice(case):
    text = driver.canonical_giac(case)
    if not text:
        return []
    return [f"set_angle_mode {case['angle']}", f"giac_eval {case['id']}|giac {text}"]


def triage(case, by_tag, record):
    """Classify a confirmed discrepancy: pipeline (keys/AST/serializer/format)
    vs Giac vs oracle policy, using Giac on a canonical string (third voice)."""
    g = by_tag.get(f"{case['id']}|giac")
    if not g:
        return {"third_voice": "unavailable"}
    ctx = driver.ctx_for(case)
    oracle = driver.oracle_of(case)
    out = {"canonical_giac": g.get("giac_in"), "giac_status": g.get("status"),
           "giac_exact": g.get("exact"), "giac_approx": g.get("approx")}
    gv, err = giac_text_value(g.get("exact") or "", ctx) if g.get("status") == "ok" else (None, "status")
    if err or gv is None:
        if g.get("status") != "ok":
            agrees = oracle.kind != "finite" or any(_nonfinite(c) for c in oracle.candidates)
            out["verdict"] = "giac refuses too" + (" (oracle agrees)" if agrees else " (oracle disagrees: giac capability/policy)")
        else:
            out["verdict"] = f"giac exact text unparsed: {err}"
        return out
    if oracle.kind == "finite" and any(close(c, gv, rel=__import__('sympy').Rational(1, 10**20)) for c in oracle.finite()):
        out["verdict"] = "giac(canonical) == oracle -> NumOS pipeline (keys/AST/serializer/formatting)"
    else:
        out["verdict"] = "giac(canonical) != oracle -> Giac semantics or oracle policy"
    return out
