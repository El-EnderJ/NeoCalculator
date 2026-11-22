"""Equations surface adapter for common.runner (CORRECTNESS-E2E-01 W5).

    py -3.12 tests/e2e/run.py --surface equations --seed 1 --count 2000 \
        --emulator C:/.piobuild/e2e-01/emulator_pc/program.exe --jobs 4

Per case: the equation(s) are typed on several key paths (model.py), each
path solves once and dumps (flow.py). analyze() judges every path against the
CONSTRUCTED truth (gen.py), substitutes every NumOS solution into the
displayed equation (mpmath 60 digits, real-domain semantics in the real
policy), compares the Giac-bound strings with the displayed rows, the
displayed result formulas with the engine values, and the paths with each
other. SymPy solveset + a numeric root scan run as a second check
(oracle_of); cases where the constructed truth and the second check disagree
are reported as `oracle_conflict`, never as NumOS findings.

Override (local to processes importing this module): flow.install() routes
common.runner/common.shrink run_script through flow.run_script, which folds
the [TUTOR_TRACE]/[PERIODIC_RESULT] lines printed by the always-passing
`assert_equations trace dump` / `assert_equations periodic dump` hooks into
the tagged dump (dump_equations alone lacks the Giac-bound text and the
periodic families).
"""
from __future__ import annotations

import random
import re

import mpmath
import sympy as sp

from common import keys as K
from common.canon import giac as canon_giac
from common.emulator import script_header
from common.evaluator import Value
from common.expr import linear, walk
from common.giacparse import parse_eval
from common.invariants import Finding, decimal_literal_info
from common.sexpr import linear as slinear, parse

from . import flow
from .gen import generate as _generate
from .model import (TB_DIRECT, TB_PHYSICAL, TB_TOOLBOX, Domain, Incomplete, MPEval, eq_linear, eq_struct,
                    eq_tokens, residual, split_eq, to_lines, compact, toolbox_used)
from .oracle import PSYM, X, Y, Z, classify_set, scan_real_roots, sym_equation, sym_side, sympy_solveset, to_mp

flow.install()

NAME = "equations"
PATHS = ("direct", "physical", "toolbox", "edited", "naive")
CANONICAL = ("direct", "physical", "toolbox", "edited")
MAX_RESULTS = 4
SET_KIND = {0: "solutions", 1: "no_solution", 2: "all_values", 3: "unsupported", 4: "periodic"}
ENV_PARSE = {"e": sp.E, "y": Y, "z": Z, **PSYM}
TB = {"direct": TB_DIRECT, "edited": TB_DIRECT, "physical": TB_PHYSICAL, "toolbox": TB_TOOLBOX}
EXACT_REL = mpmath.mpf(10) ** -30


# ── runner interface ─────────────────────────────────────────────────────────
def generate(seed, index):
    return _generate(seed, index)


def header():
    return script_header("Equations")


def check_header(by_tag):
    return []


def oracle_lite(v):
    if isinstance(v, dict):
        return v
    return {"second": "unknown", "detail": getattr(v, "detail", str(v))}


def paths_for(case):
    out = ["direct"]
    i = case["index"]
    if i % 3 == 0:
        out.append("physical")
    if any(toolbox_used(l, r) for l, r in case["eqs"]):
        out.append("toolbox")
    if i % 2 == 1:
        out.append("edited")
    if any(eq_tokens(l, r, "direct", naive=True)[1] for l, r in case["eqs"]):
        out.append("naive")
    return out


def _tokens(case, path, l, r):
    if path == "naive":
        return eq_tokens(l, r, "direct", naive=True)[0]
    mode = {"direct": "direct", "edited": "direct", "physical": "physical", "toolbox": "toolbox"}[path]
    return eq_tokens(l, r, mode)[0]


def _wrong_variant(l, r):
    """A plausible wrong first version: rhs with `+1` appended."""
    from common.expr import Num, Op, Row
    return l, Row(tuple(r.items) + (Op("+"), Num("1"))) if r.items else Row((Num("1"),))


def case_lines_for(case, oracle, paths, do_i4=False, tree_dump=False):
    lines, meta = [], {"paths": []}
    for path in paths:
        tag = f"{case['id']}|{path}"
        body = flow.settings(case["policy"], case["angle"]) + list(flow.RESET)
        keys_desc = []
        if path == "edited":
            rng = random.Random(case["edit_seed"])
            variant = rng.choice(["typo_digit", "trailing", "detour", "reopen", "reopen"])
            if variant == "reopen":
                l0, r0 = case["eqs"][0]
                wl, wr = _wrong_variant(l0, r0)
                wrong = _tokens(case, "direct", wl, wr)
                body += flow.add_equation(to_lines(wrong))
                keys_desc.append("E1(wrong): " + compact(wrong))
                for l, r in case["eqs"][1:]:
                    t = _tokens(case, "direct", l, r)
                    body += flow.add_equation(to_lines(t))
                    keys_desc.append(compact(t))
                right = _tokens(case, "direct", l0, r0)
                body += ["key up"] * 4 + ["key enter", "key ac"] + to_lines(right) + ["key enter"]
                keys_desc.append("reopen E1, AC, " + compact(right))
            else:
                for l, r in case["eqs"]:
                    base = _tokens(case, "direct", l, r)
                    t = K.edit(base, variant, rng)
                    body += flow.add_equation(to_lines(t))
                    keys_desc.append(compact(t))
            meta.setdefault("variant", variant)
        else:
            for l, r in case["eqs"]:
                t = _tokens(case, path, l, r)
                body += flow.add_equation(to_lines(t))
                keys_desc.append(compact(t))
        body += flow.solve_lines(tag)
        lines += body
        meta["paths"].append(path)
        meta[path] = {"keys": " || ".join(keys_desc)}
    return lines, meta


def case_lines(case, oracle, do_i4):
    return case_lines_for(case, oracle, paths_for(case), do_i4)


# ── truth helpers ────────────────────────────────────────────────────────────
def truth_text(t):
    k = t["kind"]
    if k in ("finite",):
        return "{" + ", ".join(str(v) for v in t["sols"]) + "}"
    if k == "tuples":
        return "{" + ", ".join("(" + ", ".join(str(c) for c in s) + ")" for s in t["sols"]) + "}"
    if k == "periodic":
        return " ∪ ".join(f"{o} + {p}·k" for o, p in t["fams"]) + (" (complex policy)" if t.get("complex_policy") else "")
    if k == "all":
        return "all values" + (f" except {t['excluded']}" if t.get("excluded") else "")
    if k == "param":
        return "{" + ", ".join(str(v) for v in t["sols"]) + "} (generic parameters)"
    if k == "cplx_infinite":
        return f"infinite complex set; real members {t['real']}"
    return k


def _eq_structs(case, tb=TB_DIRECT):
    return [eq_struct(l, r, tb) for l, r in case["eqs"]]


def oracle_of(case):
    """Second check of the constructed truth (SymPy solveset + numeric scan)."""
    t = case["truth"]
    out = {"truth": truth_text(t), "second": "skipped"}
    if t["kind"] in ("incomplete", "param") or len(case["eqs"]) > 1:
        if len(case["eqs"]) > 1 and t["kind"] in ("tuples", "empty", "dependent"):
            out.update(_system_second(case))
        return out
    st = _eq_structs(case)[0]
    if case["policy"] == "real" or t["kind"] in ("finite", "empty", "all"):
        res = sympy_solveset(st, "real" if case["policy"] == "real" or t["kind"] == "cplx_infinite" else "complex", case["angle"])
        out["sympy"] = _res_text(res)
        out["second"], notes = _agree(case, t, res, split_eq(st))
        if notes:
            out["sympy_gaps"] = notes
    if case["policy"] == "real" and t["kind"] in ("finite", "empty", "periodic"):
        parts = split_eq(st)
        span = 12
        if t["kind"] == "finite":
            reals = [to_mp(v) for v in t["sols"]]
            reals = [v.real for v in reals if v is not None and abs(v.imag) < 1e-30]
            if reals:
                span = max(12, float(max(abs(v) for v in reals)) * 1.5 + 5)
            if span > 1e4:
                return out
        if t["kind"] == "periodic":
            span = float(max(to_mp(p).real for _, p in t["fams"])) * 2.2 + 1
        try:
            roots = scan_real_roots(parts[0], parts[1], case["angle"], -span, span, n=1200)
        except Exception as exc:  # noqa: BLE001
            out["scan"] = f"error {type(exc).__name__}"
            return out
        extra = [r for r in roots if not _in_truth_real(case, t, r)]
        out["scan_roots"] = len(roots)
        if extra:
            out["scan_extra"] = [mpmath.nstr(r, 20) for r in extra]
            out["second"] = "disagree"
    return out


def _system_second(case):
    try:
        syms = [X, Y, Z][:len(case["eqs"])]
        eqs = []
        for st in _eq_structs(case):
            l, r, _ = sym_equation(st, case["angle"])
            eqs.append(sp.Eq(l, r))
        sols = sp.solve(eqs, syms, dict=True)
    except Exception as exc:  # noqa: BLE001
        return {"second": "unknown", "detail": f"sympy solve {type(exc).__name__}"}
    t = case["truth"]
    if t["kind"] == "empty":
        return {"second": "agree" if not sols else "disagree", "sympy": str(sols)}
    if t["kind"] == "dependent":
        free = any(len(s) < len(syms) or any(v.free_symbols for v in s.values()) for s in sols)
        return {"second": "agree" if free else "disagree", "sympy": str(sols)}
    got = []
    for s in sols:
        if len(s) < len(syms):
            return {"second": "disagree", "sympy": str(sols)}
        tup = tuple(s[v] for v in syms)
        if case["policy"] == "real" and not all(sp.im(c) == 0 for c in tup):
            continue
        got.append(tup)
    want = t["sols"]
    ok = len(got) == len(want) and all(any(all(_close(a, b) for a, b in zip(g, w)) for g in got) for w in want)
    return {"second": "agree" if ok else "disagree", "sympy": str(got)}


def _close(a, b, rel=EXACT_REL):
    na, nb = to_mp(a), to_mp(b)
    if na is None or nb is None:
        return na is None and nb is None
    with mpmath.workdps(60):
        return abs(na - nb) <= rel * max(1, abs(na), abs(nb))


def _res_text(res):
    if res[0] == "finite":
        return "{" + ", ".join(str(v) for v in res[1]) + "}"
    if res[0] == "periodic":
        return " ∪ ".join(f"{o}+{p}k" for o, p in res[1]) + (f" ∪ {res[2]}" if res[2] else "")
    return f"{res[0]} {res[1] if res[1] else ''}".strip()


def _agree(case, t, res, parts):
    """Second check. SymPy is not trusted blindly either (known solveset gaps:
    ln/sqrt domain in Reals, x/x -> 1): a disagreement counts only when a
    SymPy member passes the residual check (60 digits, domain-aware) but is
    absent from the truth, or a truth member fails it."""
    k = res[0]
    notes = []
    if k in ("unknown", "incomplete"):
        return "unknown", notes
    tk = t["kind"]
    real = case["policy"] == "real" or tk == "cplx_infinite"
    if tk == "cplx_infinite":
        tk, t = "finite", {"sols": t["real"]}
        if not t["sols"]:
            tk = "empty"

    def verified(v):
        m = to_mp(v)
        if m is None:
            return False
        return residual(parts[0], parts[1], {"x": m}, case["angle"], real)[0]

    if tk in ("empty", "finite"):
        tsols = list(t.get("sols", []))
        if any(not verified(v) for v in tsols):
            return "disagree", ["truth member fails residual"]
        if k == "finite":
            extra = [v for v in res[1] if not any(_close(v, w) for w in tsols)]
            for v in extra:
                if verified(v):
                    return "disagree", [f"sympy member {v} verified, absent from truth"]
                notes.append(f"sympy member {v} rejected by residual (sympy gap)")
            missing = [w for w in tsols if not any(_close(v, w) for v in res[1])]
            notes += [f"truth member {w} verified, absent from sympy (sympy gap)" for w in missing]
            return "agree", notes
        if k == "empty":
            return "agree", notes + ([f"sympy empty, truth {tsols} verified"] if tsols else [])
        return "disagree", [f"sympy kind {k}"]
    if tk == "all":
        if k != "all":
            return "disagree", [f"sympy kind {k}"]
        ex_t, ex_s = t.get("excluded", []), list(res[1])
        for v in ex_t:
            if verified(v):
                return "disagree", [f"excluded point {v} verifies"]
        if any(not any(_close(v, w) for w in ex_t) for v in ex_s):
            return "disagree", ["sympy excludes a point the truth keeps"]
        return "agree", notes + (["sympy omits excluded points (sympy gap)"] if len(ex_s) < len(ex_t) else [])
    if tk == "periodic":
        if k != "periodic":
            return "disagree", [f"sympy kind {k}"]
        w = _window(list(res[1]) + list(t["fams"]))
        miss, extra = _member_diff(_members(res[1], window=w), _members(t["fams"], window=w), w)
        return ("agree" if not miss and not extra else "disagree"), notes
    return "unknown", notes


def _window(fams):
    pmax = max(float(sp.N(p, 30)) for _, p in fams)
    omax = max(abs(float(sp.N(o, 30))) for o, _ in fams)
    return omax + 3 * pmax


def _members(fams, case=None, window=None):
    w = window or _window(fams)
    out = set()
    for o, p in fams:
        on, pn = float(sp.N(o, 40)), float(sp.N(p, 40))
        if pn <= 0:
            continue
        k0 = int((-w - on) // pn) - 1
        for k in range(k0, k0 + int(2 * w / pn) + 4):
            v = on + k * pn
            if -w <= v <= w:
                out.add(round(v, 8))
    return out


def _member_diff(mine, want, w):
    """(members of `want` absent from `mine`, members of `mine` absent from
    `want`), with 1e-6 tolerance and ignoring the window edges."""
    def inside(v):
        return abs(v) < w - 1e-6
    miss = sorted(v for v in want if inside(v) and not any(abs(v - u) < 1e-6 for u in mine))
    extra = sorted(v for v in mine if inside(v) and not any(abs(v - u) < 1e-6 for u in want))
    return miss, extra


def _in_truth_real(case, t, r):
    with mpmath.workdps(40):
        if t["kind"] == "finite":
            return any(_close(sp.Float(mpmath.nstr(r, 40), 40), v, rel=mpmath.mpf(10) ** -12) for v in t["sols"])
        if t["kind"] == "periodic":
            for o, p in t["fams"]:
                on, pn = to_mp(o).real, to_mp(p).real
                q = (r - on) / pn
                if abs(q - mpmath.nint(q)) < mpmath.mpf(10) ** -10:
                    return True
            return False
        return False


# ── NumOS answer ─────────────────────────────────────────────────────────────
_NUM = re.compile(r"\d+\.\d*(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+")


def parse_value(text):
    """(sympy value, ulp or None). ulp is set for decimal (numerical) texts."""
    v = parse_eval(text, "rad", ENV_PARSE)
    lits = _NUM.findall(text)
    ulp = None
    if lits:
        ulp = max(decimal_literal_info(s)[1] for s in lits)
    return v, ulp


def numos_answer(d):
    view = d.get("view") or {}
    groups = []
    for g in d.get("groups") or []:
        vals = []
        for v in g:
            try:
                val, ulp = parse_value(v["exact"])
            except Exception as exc:  # noqa: BLE001
                val, ulp = None, f"unparsed: {exc}"
            vals.append({"var": v["var"], "text": v["exact"], "value": val, "ulp": ulp, "real": v.get("real")})
        groups.append(vals)
    fams = []
    for f in view.get("families") or []:
        try:
            fams.append((parse_eval(f["offset"], "rad", ENV_PARSE), parse_eval(f["period"], "rad", ENV_PARSE),
                         f["offset"], f["period"]))
        except Exception as exc:  # noqa: BLE001
            fams.append((None, None, f["offset"], f["period"]))
    return {"status": d.get("status"), "set_kind": d.get("set_kind"), "groups": groups, "fams": fams,
            "coverage": view.get("scope"), "origin": view.get("origin"), "title": view.get("title"),
            "formulas": view.get("formulas") or [], "raw": d.get("raw_exact"), "diag": d.get("diag")}


def answer_text(a):
    if a["status"] != "ok":
        return f"{a['status']} ({(a.get('diag') or '')[:80]})"
    sk = SET_KIND.get(a["set_kind"], a["set_kind"])
    if a["set_kind"] == 4:
        return "periodic " + " ∪ ".join(f"{f[2]} + {f[3]}·k" for f in a["fams"])
    if a["groups"]:
        if len(a["groups"][0]) > 1:
            return f"{sk} " + "{" + ", ".join("(" + ", ".join(v["text"] for v in g) + ")" for g in a["groups"]) + "}"
        return f"{sk} " + "{" + ", ".join(v["text"] for g in a["groups"] for v in g) + "}"
    return str(sk)


def answer_key(a):
    """Normalised answer for path comparison (I3)."""
    if a["status"] != "ok":
        return ("status", a["status"])
    if a["set_kind"] == 4:
        try:
            return ("periodic", tuple(sorted(_members([(f[0], f[1]) for f in a["fams"]], window=40))))
        except Exception:  # noqa: BLE001
            return ("periodic", tuple(f[2] + "|" + f[3] for f in a["fams"]))
    vals = []
    for g in a["groups"]:
        tup = []
        for v in g:
            n = to_mp(v["value"]) if v["value"] is not None and not v["value"].free_symbols else None
            tup.append((round(float(n.real), 9), round(float(n.imag), 9)) if n is not None else v["text"])
        vals.append(tuple(tup))
    return (SET_KIND.get(a["set_kind"]), tuple(sorted(vals, key=str)))


# ── dumpTree reader (displayed result formulas) ──────────────────────────────
_OPSYM = {"+": "Add", "-": "Sub", "×": "Mul", "÷": "Div", "=": "Eq", "±": "PlusMinus", "≠": "Ne"}
_FN = {"sin": "sin", "cos": "cos", "tan": "tan", "sin⁻¹": "asin", "cos⁻¹": "acos", "tan⁻¹": "atan",
       "ln": "ln", "log": "log"}


def parse_dumptree(text):
    lines = [l for l in text.split("\n") if l.strip()]
    pos = 0

    def ind(l):
        return (len(l) - len(l.lstrip(" "))) // 2

    def strip_metrics(s):
        return re.sub(r"\s+\[\d+x\d+ a-?\d+/d-?\d+\]$", "", s.strip())

    def node(level):
        nonlocal pos
        line = lines[pos]
        s = strip_metrics(line)
        pos += 1

        def slots(names):
            out = {}
            while pos < len(lines) and ind(lines[pos]) == level + 1 and lines[pos].strip().rstrip(":") in names:
                nm = lines[pos].strip().rstrip(":")
                nonlocal_pos_inc()
                out[nm] = node(level + 2)
            return out

        if s == "Row":
            kids = []
            while pos < len(lines) and ind(lines[pos]) == level + 1:
                kids.append(node(level + 1))
            return ("row", tuple(kids))
        if s.startswith("Number "):
            return ("num", s[len("Number "):].strip('"'))
        if s.startswith("Operator "):
            return ("op", _OPSYM.get(s[len("Operator "):], s[len("Operator "):]))
        if s.startswith("Constant "):
            c = s[len("Constant "):]
            return ("const", {"π": "pi", "e": "e", "i": "i"}.get(c, c))
        if s.startswith("Variable "):
            return ("var", s[len("Variable "):].strip('"'))
        if s.startswith("Empty"):
            return ("empty",)
        if s.startswith("Symbol "):
            return ("sym", s[len("Symbol "):].strip('"'))
        if s.startswith("SpecialValue"):
            return ("special", s[len("SpecialValue "):])
        if s == "Fraction":
            sl = slots({"num", "den"})
            return ("frac", sl.get("num"), sl.get("den"))
        if s == "Power":
            sl = slots({"base", "exp"})
            return ("pow", sl.get("base"), sl.get("exp"))
        if s.startswith("Root"):
            sl = slots({"radicand", "degree"})
            if "degree" in sl:
                return ("root", sl.get("radicand"), sl.get("degree"))
            return ("sqrt", sl.get("radicand"))
        if s.startswith("Paren"):
            kind = {"()": "paren", "||": "bar", "[]": "bracket", "{}": "brace"}.get(s[len("Paren "):], "paren")
            sl = slots({"content"})
            return ("paren", kind, sl.get("content"))
        if s.startswith("Function "):
            sl = slots({"arg"})
            return ("fn", _FN.get(s[len("Function "):], s[len("Function "):]), sl.get("arg"))
        if s == "LogBase":
            sl = slots({"base", "arg"})
            return ("logb", sl.get("base"), sl.get("arg"))
        # unknown node: consume its children
        while pos < len(lines) and ind(lines[pos]) > level:
            pos += 1
        return ("unknown", s)

    def nonlocal_pos_inc():
        nonlocal pos
        pos += 1

    return node(ind(lines[0]))


def formula_value(struct):
    """Numeric value of a displayed formula (constants only), or None."""
    try:
        with mpmath.workdps(60):
            return MPEval({}, "rad", False).row(struct if struct[0] == "row" else ("row", (struct,)))
    except (Domain, Incomplete, ZeroDivisionError, ValueError, OverflowError, TypeError):
        return None


# ── judging ──────────────────────────────────────────────────────────────────
def _f(case, path, inv, sev, title, **detail):
    return Finding(case["id"], path, inv, sev, title, detail)


def _env_for(case, sol):
    if isinstance(sol, dict):
        return sol
    return {"x": sol}


def _solution_ok(case, parts_list, env, decimal=False):
    """Residual check of `env` against every displayed equation. A decimal
    (numerical, ~12 significant digits) value gets a 1e-8 relative residual
    tolerance instead of 1e-40."""
    real = case["policy"] == "real"
    infos = []
    for lhs, rhs in parts_list:
        ok, info = residual(lhs, rhs, env, case["angle"], real, tol="1e-8" if decimal else None)
        infos.append(info)
        if not ok:
            return False, "; ".join(infos)
    return True, "; ".join(infos)


def _mp_env(names, values):
    env = {}
    for n, v in zip(names, values):
        m = to_mp(v)
        if m is None:
            return None
        env[n] = m
    return env


def judge(case, path, a, parts_list, truth, keys, extra):
    """Compare NumOS answer `a` with `truth` for the displayed equations."""
    F = []
    tk = truth["kind"]
    info = dict(keys=keys, numos=answer_text(a), oracle=truth_text(truth), **extra)
    if tk == "incomplete":
        if a["status"] == "ok":
            F.append(_f(case, path, "I5", "S2", "NumOS solved a structurally incomplete equation", **info))
        return F
    if a["status"] != "ok":
        sev = "S3"
        F.append(_f(case, path, "I5", sev, f"refusal ({a['status']}) where the oracle has an answer", **info))
        return F
    sk = a["set_kind"]
    vars_ = case["vars"]
    if sk == 3:
        F.append(_f(case, path, "I5", "S3", "unsupported/unresolved result where the oracle has an answer", **info))
        return F

    # ---- the four non-finite truth kinds
    if tk == "empty":
        if sk == 1:
            return F
        if sk == 2:
            F.append(_f(case, path, "I1", "S1", "all values claimed, equation has no solution", **info))
            return F
        if sk == 4:
            F.append(_f(case, path, "I1", "S1", "periodic families claimed, equation has no solution", **info))
            return F
        return F + _spurious_only(case, path, a, parts_list, info, expect_none=True)
    if tk in ("all", "dependent"):
        if sk == 2:
            title = (a.get("title") or "").lower()
            if tk == "all" and truth.get("excluded") and "condition" not in title:
                F.append(_f(case, path, "I1", "S2", "identity with excluded values shown as unconditional", **info))
            return F
        what = {0: "a finite solution set", 1: "no solution", 4: "periodic families"}.get(sk, str(sk))
        F.append(_f(case, path, "I1", "S1", f"{what} shown, but every value solves the equation(s)"
                    if tk == "all" else f"{what} shown for a system with infinitely many solutions", **info))
        return F
    if tk == "periodic":
        return F + _judge_periodic(case, path, a, parts_list, truth, info)
    if tk == "cplx_infinite":
        return F + _judge_cplx_infinite(case, path, a, parts_list, truth, info)
    if tk == "param":
        return F + _judge_param(case, path, a, truth, info)

    # ---- finite / tuples
    if sk == 1:
        F.append(_f(case, path, "I1", "S1", "\"No solution\" shown, but solutions exist", **info))
        return F
    if sk == 2:
        F.append(_f(case, path, "I1", "S1", "all values claimed for an equation with finitely many solutions", **info))
        return F
    if sk == 4:
        F.append(_f(case, path, "I1", "S1", "periodic families claimed for a finite solution set", **info))
        return F
    want = [tuple(s) if isinstance(s, tuple) else (s,) for s in truth["sols"]]
    got = []
    for g in a["groups"]:
        got.append(g)
    matched_t = [False] * len(want)
    problems = []
    for g in got:
        if any(v["value"] is None for v in g):
            problems.append(("unparsed", g))
            continue
        if any(v["value"].free_symbols for v in g):
            problems.append(("symbolic", g))
            continue
        best = None
        for ti, w in enumerate(want):
            if len(w) != len(g):
                continue
            errs = []
            for v, c in zip(g, w):
                nv, nc = to_mp(v["value"]), to_mp(c)
                if nv is None or nc is None:
                    errs = None
                    break
                with mpmath.workdps(60):
                    errs.append((abs(nv - nc), v["ulp"], abs(nc)))
            if errs is None:
                continue
            if all(e[1] is None and e[0] <= EXACT_REL * max(1, e[2]) for e in errs):
                best = (ti, "exact")
                break
            if all(e[1] is not None or e[0] <= EXACT_REL * max(1, e[2]) for e in errs):
                worst = max((e[0] / e[1]) if e[1] else 0 for e in errs)
                if worst <= 0.5 * (1 + 1e-9):
                    best = (ti, "decimal_ok")
                    break
                if worst <= 1 + 1e-9:
                    best = best or (ti, "decimal_last_digit")
        if best is None:
            problems.append(("unmatched", g))
        else:
            matched_t[best[0]] = True
            if best[1] == "decimal_last_digit":
                F.append(_f(case, path, "I1", "S2", "numerical candidate off by one unit in the last displayed digit",
                            values=[v["text"] for v in g], **info))
    # duplicates
    keys_ = [tuple(v["text"] for v in g) for g in got]
    if len(set(keys_)) < len(keys_):
        F.append(_f(case, path, "I1", "S2", "the same solution is listed more than once", **info))
    for kind, g in problems:
        txt = [v["text"] for v in g]
        if kind == "unparsed":
            F.append(_f(case, path, "I1", "S4", "solution text not readable by the harness (unchecked)", values=txt, **info))
            continue
        if kind == "symbolic":
            F.append(_f(case, path, "I1", "S1", "solution contains a free variable/parameter", values=txt, **info))
            continue
        env = _mp_env([v["var"] for v in g], [v["value"] for v in g])
        if env is None:
            F.append(_f(case, path, "I1", "S1", "non-finite value shown as a solution", values=txt, **info))
            continue
        decimal = any(v["ulp"] for v in g)
        ok, rinfo = _solution_ok(case, parts_list, env)
        if ok and not decimal:
            info["oracle_conflict"] = f"NumOS value {txt} satisfies the equation but is not in the constructed truth"
            F.append(_f(case, path, "ORACLE", "S4", "constructed truth misses a verified solution (oracle bug)",
                        values=txt, residual=rinfo, **info))
        elif decimal:
            F.append(_f(case, path, "I1", "S1", "numerical candidate does not approximate any solution to its digits",
                        values=txt, residual=rinfo, **info))
        else:
            F.append(_f(case, path, "I1", "S1", "spurious solution: value does not satisfy the displayed equation",
                        values=txt, residual=rinfo, **info))
    for ti, w in enumerate(want):
        if not matched_t[ti]:
            env = _mp_env(vars_[:len(w)], w)
            ok, rinfo = _solution_ok(case, parts_list, env) if env else (False, "nonfinite truth")
            if not ok:
                F.append(_f(case, path, "ORACLE", "S4", "constructed truth member fails the residual check (oracle bug)",
                            member=str(w), residual=rinfo, **info))
            else:
                F.append(_f(case, path, "I1", "S1", "missing solution", member=str(w if len(w) > 1 else w[0]), **info))
    return F


def _spurious_only(case, path, a, parts_list, info, expect_none):
    F = []
    for g in a["groups"]:
        txt = [v["text"] for v in g]
        if any(v["value"] is None or v["value"].free_symbols for v in g):
            F.append(_f(case, path, "I1", "S1", "solution(s) shown, equation has no solution", values=txt, **info))
            continue
        env = _mp_env([v["var"] for v in g], [v["value"] for v in g])
        ok, rinfo = _solution_ok(case, parts_list, env, any(v["ulp"] for v in g)) if env else (False, "nonfinite")
        if ok:
            F.append(_f(case, path, "ORACLE", "S4", "constructed 'no solution' truth contradicted by a verified value",
                        values=txt, residual=rinfo, **info))
        else:
            F.append(_f(case, path, "I1", "S1", "spurious solution shown for an equation with no solution",
                        values=txt, residual=rinfo, **info))
    return F


def _judge_periodic(case, path, a, parts_list, truth, info):
    F = []
    sk = a["set_kind"]
    fams_t = truth["fams"]
    if sk == 1:
        return [_f(case, path, "I1", "S1", "\"No solution\" shown, but periodic solution families exist", **info)]
    if sk == 2:
        return [_f(case, path, "I1", "S1", "all values claimed for a periodic solution set", **info)]
    if sk == 4:
        fams = [(f[0], f[1]) for f in a["fams"]]
        if any(o is None for o, _ in fams):
            return [_f(case, path, "I1", "S4", "periodic family text not readable (unchecked)", **info)]
        w = _window(fams_t + fams)
        miss, extra = _member_diff(_members(fams, window=w), _members(fams_t, window=w), w)
        if miss:
            F.append(_f(case, path, "I1", "S1", "periodic answer misses solutions (a family or members absent)",
                        missing_members=miss[:6], **info))
        if extra:
            bad = []
            for e in extra[:4]:
                ok, rinfo = _solution_ok(case, parts_list, {"x": mpmath.mpf(e)})
                ok2 = False
                if not ok:
                    # rounding of the 8-digit member key: verify at full precision
                    for o, p in fams:
                        on, pn = to_mp(o).real, to_mp(p).real
                        k = mpmath.nint((e - on) / pn)
                        ok2, _ = _solution_ok(case, parts_list, {"x": on + k * pn})
                        if ok2 and abs(on + k * pn - e) < 1e-6:
                            break
                        ok2 = False
                if not (ok or ok2):
                    bad.append(e)
            if bad:
                F.append(_f(case, path, "I1", "S1", "periodic answer contains non-solutions", extra_members=bad, **info))
            elif extra:
                F.append(_f(case, path, "ORACLE", "S4", "periodic truth misses verified members (oracle bug)",
                            extra_members=extra[:4], **info))
        return F
    # finite list: representatives
    covered = [False] * len(fams_t)
    for g in a["groups"]:
        v = g[0]
        if v["value"] is None:
            F.append(_f(case, path, "I1", "S4", "representative not readable (unchecked)", **info))
            continue
        n = to_mp(v["value"])
        ok, rinfo = _solution_ok(case, parts_list, {"x": n}, bool(v["ulp"])) if n is not None else (False, "nonfinite")
        if not ok:
            F.append(_f(case, path, "I1", "S1", "representative is not a solution", value=v["text"], residual=rinfo, **info))
            continue
        for i, (o, p) in enumerate(fams_t):
            q = (n - to_mp(o)) / to_mp(p)
            if abs(q.imag) < 1e-20 and abs(q.real - mpmath.nint(q.real)) < mpmath.mpf(10) ** -20:
                covered[i] = True
    if a["groups"] and not all(covered):
        # a family with no representative at all
        miss = [f"{o} + {p}k" for (o, p), c in zip(fams_t, covered) if not c]
        same = _members([fams_t[i] for i in range(len(fams_t)) if covered[i]]) >= _members([f for f, c in zip(fams_t, covered) if not c]) if any(covered) else False
        if not same:
            F.append(_f(case, path, "I1", "S2" if a.get("coverage") == 2 else "S1",
                        "listed solutions omit a whole periodic family" + (" (shown as 'representatives': needs policy decision)" if a.get("coverage") == 2 else ""),
                        uncovered=miss, coverage=a.get("coverage"), ui_title=a.get("title"), **info))
    if not a["groups"]:
        F.append(_f(case, path, "I1", "S1", "empty solution list for a periodic solution set", **info))
    if case["policy"] == "real" and a.get("coverage") == 2:
        info2 = dict(info, ui_title=a.get("title"))
        F.append(_f(case, path, "CAP", "S4", "periodic solve declined in the real policy: only representatives listed", **info2))
    return F


def _judge_cplx_infinite(case, path, a, parts_list, truth, info):
    F = []
    sk = a["set_kind"]
    reals = truth["real"]
    if sk == 1:
        return [_f(case, path, "I1", "S1", "\"No complex solution\" shown, but the complex solution set is infinite "
                   "(needs policy decision)", policy_question=True, **info)]
    if sk != 0:
        return F
    covered = [False] * len(reals)
    for g in a["groups"]:
        v = g[0]
        if v["value"] is None:
            continue
        n = to_mp(v["value"])
        ok, rinfo = _solution_ok(case, parts_list, {"x": n}, bool(v["ulp"])) if n is not None else (False, "nonfinite")
        if not ok:
            F.append(_f(case, path, "I1", "S1", "shown value is not a solution", value=v["text"], residual=rinfo, **info))
        for i, r in enumerate(reals):
            if n is not None and abs(n - to_mp(r)) < EXACT_REL * max(1, abs(n)):
                covered[i] = True
    for r, c in zip(reals, covered):
        if not c:
            F.append(_f(case, path, "I1", "S1", "missing real solution (complex policy)", member=str(r), **info))
    F.append(_f(case, path, "I1", "S2", "finite list presented as the complete complex solution set of an equation "
                "with infinitely many complex solutions (needs policy decision)", policy_question=True,
                ui_title=a.get("title"), **info))
    return F


def _judge_param(case, path, a, truth, info):
    if a["set_kind"] != 0:
        return [_f(case, path, "I1", "S1", f"{SET_KIND.get(a['set_kind'])} shown for a generic parametric equation", **info)]
    got = [g[0]["value"] for g in a["groups"]]
    if any(v is None for v in got):
        return [_f(case, path, "I1", "S4", "parametric solution not readable (unchecked)", **info)]
    rng = random.Random(7)
    for _ in range(3):
        subs = {PSYM[p]: sp.Rational(rng.randint(2, 40), rng.randint(1, 7)) * (1 if truth.get("assume_pos") else rng.choice([1, -1]))
                for p in truth["params"]}
        gv = sorted([complex(sp.N(v.subs(subs), 30)) for v in got], key=lambda c: (round(c.real, 9), round(c.imag, 9)))
        tv = sorted([complex(sp.N(v.subs(subs), 30)) for v in truth["sols"]], key=lambda c: (round(c.real, 9), round(c.imag, 9)))
        if len(gv) != len(tv) or any(abs(x - y) > 1e-12 * max(1, abs(y)) for x, y in zip(gv, tv)):
            return [_f(case, path, "I1", "S1", "parametric solution differs from the oracle", substitution=str(subs), **info)]
    return []


def check_display(case, path, a, info):
    """Displayed result formulas (dumpTree of the result canvases) vs the
    engine's exact values."""
    F = []
    if a["status"] != "ok" or a["set_kind"] != 0 or not a["formulas"]:
        return F
    if (a.get("title") or "").lower().startswith(("numerical", "exact text")):
        return F
    flat = []
    pages = len(case["eqs"]) > 1 or len(a["groups"]) > MAX_RESULTS
    for gi, g in enumerate(a["groups"][:1] if pages else a["groups"]):
        flat += g
    for v, ftxt in zip(flat, a["formulas"]):
        if v["value"] is None or v["value"].free_symbols:
            continue
        try:
            st = parse_dumptree(ftxt)
        except Exception as exc:  # noqa: BLE001
            continue
        shown = formula_value(st)
        want = to_mp(v["value"])
        if shown is None or want is None:
            continue
        rel = EXACT_REL if not v["ulp"] else mpmath.mpf(10) ** -9
        with mpmath.workdps(60):
            if abs(shown - want) > rel * max(1, abs(want)):
                F.append(_f(case, path, "I1", "S1", "displayed solution formula differs from the engine value",
                            engine_text=v["text"], shown_formula=slinear(st), displayed_value=mpmath.nstr(shown, 15),
                            engine_value=mpmath.nstr(want, 15), **info))
    return F


def check_giac_text(case, path, d, disp_structs):
    """I2(b): do the Giac-bound strings denote the displayed equations?"""
    tr = d.get("trace") or {}
    authored = tr.get("authored")
    if not authored or len(authored) != len(disp_structs):
        return None
    rng = random.Random(11)
    env_pts = [{"x": sp.Rational(7, 3), "y": sp.Rational(-5, 4), "z": sp.Rational(3, 7)},
               {"x": sp.Rational(-13, 5), "y": sp.Rational(9, 2), "z": sp.Rational(-2, 3)},
               {"x": sp.Rational(11, 17), "y": sp.Rational(2, 9), "z": sp.Rational(5, 2)}]
    for st, (gl, gr) in zip(disp_structs, authored):
        parts = split_eq(st)
        if parts is None:
            continue
        for side, txt in zip(parts, (gl, gr)):
            try:
                g = parse_eval(txt, case["angle"], dict(ENV_PARSE, x=sp.Symbol("x")))
                s = sym_side(side, case["angle"])
            except Exception as exc:  # noqa: BLE001
                return {"unparsed": f"{txt}: {str(exc)[:80]}"}
            for pt in env_pts:
                subs = {sp.Symbol(k): v for k, v in pt.items()}
                subs.update({PSYM[p]: sp.Rational(3 + i, 2) for i, p in enumerate("ABCDEF")})
                try:
                    a_ = to_mp(s.subs(subs))
                    b_ = to_mp(g.subs(subs))
                except Exception:  # noqa: BLE001
                    continue
                if a_ is None or b_ is None:
                    if (a_ is None) != (b_ is None):
                        return {"mismatch": txt, "point": str(pt)}
                    continue
                if abs(a_ - b_) > mpmath.mpf(10) ** -25 * max(1, abs(a_)):
                    return {"mismatch": txt, "displayed": slinear(side), "point": str(pt),
                            "displayed_value": mpmath.nstr(a_, 12), "giac_value": mpmath.nstr(b_, 12)}
    return None


def analyze(case, by_tag, meta, oracle):
    rec = {"id": case["id"], "family": case["family"], "sub": case["sub"], "policy": case["policy"],
           "angle": case["angle"], "equations": [eq_linear(l, r) for l, r in case["eqs"]],
           "truth": truth_text(case["truth"]), "paths": {}}
    if isinstance(oracle, dict):
        rec["second"] = oracle.get("second")
        if oracle.get("second") == "disagree":
            rec["second_detail"] = {k: oracle.get(k) for k in ("sympy", "scan_extra", "detail")}
    findings, desync = [], []
    answers = {}
    second_bad = isinstance(oracle, dict) and oracle.get("second") == "disagree"
    for path in meta.get("paths", []):
        d = by_tag.get(f"{case['id']}|{path}")
        keys = meta.get(path, {}).get("keys")
        if not d or not d.get("ok") or d.get("app") != "Equations":
            desync.append(f"{path}: missing/invalid dump")
            continue
        if "view" not in d or "trace" not in d:
            desync.append(f"{path}: side dumps missing")
            continue
        try:
            disp = [parse(s) for s in d.get("equations") or []]
        except Exception as exc:  # noqa: BLE001
            desync.append(f"{path}: equation unparsable {exc}")
            continue
        n_expected = len(case["eqs"])
        if len(disp) != n_expected and case["truth"]["kind"] != "incomplete":
            desync.append(f"{path}: {len(disp)} rows, expected {n_expected}")
            continue
        a = numos_answer(d)
        answers[path] = a
        prec = {"keys": keys, "displayed": [slinear(s) for s in disp], "giac_bound": (d.get("trace") or {}).get("authored"),
                "numos": answer_text(a), "title": a.get("title")}
        rec["paths"][path] = prec
        if case["truth"]["kind"] == "incomplete":
            findings += judge(case, path, a, [], case["truth"], keys, {"displayed": prec["displayed"]})
            continue
        parts_list = [split_eq(s) for s in disp]
        extra = {"displayed": prec["displayed"], "giac_bound": prec["giac_bound"], "policy": case["policy"],
                 "angle": case["angle"]}
        if path in CANONICAL:
            intended = _eq_structs(case, TB[path])
            if disp != intended:
                findings.append(_f(case, path, "I2", "S2", "displayed equation differs from the typed one (canonical keys)",
                                   intended=[slinear(s) for s in intended], **dict(extra, keys=keys)))
                if any(p is None for p in parts_list):
                    continue
            truth = case["truth"]
        else:
            # naive `)` path: the displayed equation is its own truth source
            if any(p is None for p in parts_list):
                if a["status"] == "ok":
                    findings.append(_f(case, path, "I5", "S2", "NumOS solved a row without exactly one top-level '='",
                                       **dict(extra, keys=keys, numos=answer_text(a))))
                continue
            truth = _truth_from_sympy(case, disp)
            prec["oracle_displayed"] = truth_text(truth) if truth else "unknown"
            if truth is None:
                continue
        if any(p is None for p in parts_list):
            continue
        g = check_giac_text(case, path, d, disp)
        if g and "mismatch" in g:
            findings.append(_f(case, path, "I2", "S1", "Giac-bound text does not denote the displayed equation",
                               **dict(extra, keys=keys, check=g, numos=answer_text(a))))
        elif g:
            prec["giac_check"] = g
        if second_bad and path in CANONICAL:
            prec["note"] = "second oracle check disagrees with the constructed truth: case excluded from I1"
            continue
        fs = judge(case, path, a, parts_list, truth, keys, extra)
        findings += fs
        findings += check_display(case, path, a, dict(keys=keys, displayed=prec["displayed"]))
    # I3 path independence among canonical paths that judged cleanly
    keys_by_path = {p: answer_key(a) for p, a in answers.items() if p in CANONICAL}
    if len(set(map(str, keys_by_path.values()))) > 1:
        findings.append(_f(case, "*", "I3", "S2", "canonical key paths give different answers",
                           answers={p: answer_text(answers[p]) for p in keys_by_path},
                           keys={p: meta.get(p, {}).get("keys") for p in keys_by_path}))
    rec["finding_titles"] = sorted({f.title for f in findings})
    return rec, findings, desync


def _truth_from_sympy(case, disp):
    if len(disp) != 1:
        return None
    pol = case["policy"]
    res = sympy_solveset(disp[0], pol, case["angle"])
    if res[0] == "finite":
        return {"kind": "finite", "sols": list(res[1])}
    if res[0] == "empty":
        return {"kind": "empty"}
    if res[0] == "all":
        return {"kind": "all", "excluded": list(res[1])}
    if res[0] == "periodic" and not res[2]:
        return {"kind": "periodic", "fams": res[1], "extra": []}
    return None


# ── third voice / triage ─────────────────────────────────────────────────────
def canonical_text(case):
    env = {n: n for n in ("y", "z", "A", "B", "C", "D", "E", "F")}
    try:
        eqs = [f"{canon_giac(l, env)}={canon_giac(r, env)}" for l, r in case["eqs"]]
    except Exception:  # noqa: BLE001
        return None
    cmd = "solve" if case["policy"] == "real" else "csolve"
    if len(eqs) == 1:
        return f"{cmd}({eqs[0]},x)"
    return f"{cmd}([{','.join(eqs)}],[{','.join(case['vars'])}])"


def third_voice(case):
    t = canonical_text(case)
    if not t:
        return []
    return [f"set_angle_mode {case['angle']}", f"giac_eval {case['id']}|giac {t}"]


def triage(case, by_tag, record):
    g = by_tag.get(f"{case['id']}|giac")
    if not g:
        return {"third_voice": "unavailable"}
    return {"canonical_giac": g.get("giac_in"), "giac_status": g.get("status"), "giac_exact": g.get("exact"),
            "giac_approx": g.get("approx"), "giac_diag": (g.get("diag") or "")[:200]}
