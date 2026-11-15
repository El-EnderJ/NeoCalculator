"""Targeted Equations surface: cursor edits and templates (W5 re-plan).

    py -3.12 tests/e2e/run.py --surface equations.edit --seed 51 --count 1500 --jobs 3 \
        --out out/correctness-e2e-01/equations/edit-seed51-n1500

Question: when the cursor is moved and a digit/variable is inserted next to
existing content (Calculation: `3 LEFT 5` displays 53 but computes 5*3), does
the Equations input diverge between what is DISPLAYED and what is SOLVED?

Cases (no prediction of the editor's behaviour is needed):
  cursor    a base equation (linear/quadratic, multi-digit coefficients) is
            typed with canonical keys, with 1-3 cursor detours injected at
            random token positions: LEFT x m, then a digit / `x` / `.` / DEL,
            then RIGHT x m' (m' may differ from m: typing continues wherever
            the cursor is). Half the detours hit the exact `<number> LEFT
            <digit>` pattern.
  template  Polynomial / Exponential / Logarithmic template, then random
            LEFT / DEL / digit edits replacing the A, B, C slots.

Oracle = the equation AS DISPLAYED: the dumped row is read the way the
screen renders it, i.e. adjacent number nodes concatenate (`(num 5)(num 3)`
renders "53"). Checks:
  I2  the Giac-bound strings must denote that visual reading (sampled at
      x, y, z points), else WYSIWYC is broken (S1 if the answer changes);
  I1  NumOS's solution set vs SymPy solveset + the residual check of the
      visual equation (single variable x, no free parameters);
  I5  no answer for a row that is not one complete equality.
"""
from __future__ import annotations

import random

from common.emulator import script_header
from common.gen import case_rng
from common.invariants import Finding
from common.sexpr import linear as slinear, parse

from .. import flow, surface as S
from ..gen import g_linear, g_quadratic
from ..model import eq_tokens, split_eq, to_lines, compact

NAME = "equations.edit"
PATHS = ("edit",)
TEMPLATES = {1: "Polynomial A x^2 + B x + C = 0", 2: "Exponential A e^x + B = 0", 3: "Logarithmic ln(x) + A = 0"}


def generate(seed, index):
    rng = case_rng(seed, index)
    policy = "real" if rng.random() < 0.6 else "complex"
    if rng.random() < 0.7:
        # base equation with multi-digit numbers
        while True:
            sub, eqs, truth = (g_linear(rng) if rng.random() < 0.6 else g_quadratic(rng, policy))
            toks, _ = eq_tokens(eqs[0][0], eqs[0][1], "direct")
            if len(toks) >= 4:
                break
        toks = [t for t in toks]
        nops = rng.choice([1, 1, 2, 3])
        ops = []
        for _ in range(nops):
            # prefer positions right after a digit (the `3 LEFT 5` pattern)
            digit_pos = [i + 1 for i, t in enumerate(toks) if t[0] == "key" and t[1].isdigit()]
            p = rng.choice(digit_pos) if (digit_pos and rng.random() < 0.6) else rng.randrange(1, len(toks) + 1)
            m = rng.choice([1, 1, 1, 2, 3])
            ins = rng.choices(["digit", "digit", "digit", "x", "dot", "del"], k=1)[0]
            new = [("key", "left")] * m
            if ins == "digit":
                new.append(("key", str(rng.randint(1, 9))))
            elif ins == "x":
                new.append(("key", "x"))
            elif ins == "dot":
                new += [("key", "."), ("key", str(rng.randint(0, 9)))]
            else:
                new.append(("key", "del"))
            back = m if rng.random() < 0.7 else rng.choice([0, m + 1, max(0, m - 1)])
            new += [("key", "right")] * back
            toks = toks[:p] + new + toks[p:]
            ops.append(f"@{p}:L{m}+{ins}+R{back}")
        kind, template = "cursor", None
        base = S.eq_linear(*eqs[0])
    else:
        kind = "template"
        template = rng.choice([1, 1, 2, 3])
        toks, ops = [], []
        for _ in range(rng.choice([0, 1, 2, 3, 4])):
            m = rng.randint(0, 6)
            toks += [("key", "left")] * m
            if rng.random() < 0.6:
                toks.append(("key", "del"))
            toks += [("key", str(rng.randint(1, 9))) for _ in range(rng.choice([1, 1, 2]))]
            ops.append(f"L{m}")
        base = TEMPLATES[template]
    return {"id": f"e{seed}-{index}", "seed": seed, "index": index, "family": kind, "sub": base,
            "policy": policy, "angle": "rad", "tokens": toks, "template": template, "ops": ops,
            "eqs": [], "vars": ["x"], "truth": {"kind": "visual"}, "edit_seed": 0}


def header():
    return script_header("Equations")


def check_header(by_tag):
    return []


def oracle_of(case):
    return {"second": "skipped"}


def oracle_lite(v):
    return v if isinstance(v, dict) else {"second": "unknown"}


def case_lines_for(case, oracle, paths, do_i4=False, tree_dump=False):
    tag = f"{case['id']}|edit"
    body = flow.settings(case["policy"], case["angle"]) + list(flow.RESET)
    if case["template"]:
        body += ["key enter"] + ["key down"] * case["template"] + ["key enter"]
    else:
        body += ["key enter", "key enter"]
    body += to_lines(case["tokens"]) + ["key enter", "key down"] + flow.solve_lines(tag)
    keys = (f"[template {case['template']}] " if case["template"] else "") + compact(case["tokens"])
    return body, {"paths": ["edit"], "edit": {"keys": keys}}


def case_lines(case, oracle, do_i4):
    return case_lines_for(case, oracle, PATHS, do_i4)


def merge_numbers(node):
    """Visual reading: adjacent number nodes render as one numeral."""
    if not isinstance(node, tuple) or not node:
        return node
    if node[0] == "row":
        out = []
        for c in node[1]:
            c = merge_numbers(c)
            if out and isinstance(c, tuple) and c[:1] == ("num",) and isinstance(out[-1], tuple) and out[-1][:1] == ("num",):
                out[-1] = ("num", out[-1][1] + c[1])
            else:
                out.append(c)
        return ("row", tuple(out))
    return tuple(merge_numbers(c) if isinstance(c, tuple) else c for c in node)


def _valid_numerals(node):
    if isinstance(node, tuple) and node:
        if node[0] == "num":
            return node[1].count(".") <= 1
        return all(_valid_numerals(c) for c in node[1:] if isinstance(c, tuple)) if node[0] != "row" else \
            all(_valid_numerals(c) for c in node[1])
    return True


def _free_ok(struct):
    """True when the only variable is x (no parameters / y / z)."""
    stack = [struct]
    while stack:
        n = stack.pop()
        if isinstance(n, tuple) and n:
            if n[0] == "var" and n[1] != "x":
                return False
            stack.extend(c for c in (n[1] if n[0] == "row" else n[1:]) if isinstance(c, tuple))
    return True


def analyze(case, by_tag, meta, oracle):
    rec = {"id": case["id"], "family": case["family"], "sub": case["sub"], "policy": case["policy"],
           "ops": case["ops"], "paths": {}}
    d = by_tag.get(f"{case['id']}|edit")
    keys = meta["edit"]["keys"]
    if not d or not d.get("ok") or d.get("app") != "Equations" or "view" not in d or "trace" not in d:
        return rec, [], ["edit: missing dump"]
    rows = d.get("equations") or []
    if len(rows) != 1:
        rec["paths"]["edit"] = {"keys": keys, "rows": len(rows)}
        return rec, [], [] if not rows else [f"edit: {len(rows)} rows"]
    disp = parse(rows[0])
    visual = merge_numbers(disp)
    adjacent = visual != disp
    a = S.numos_answer(d)
    prec = {"keys": keys, "displayed": slinear(disp), "visual": slinear(visual) if adjacent else None,
            "giac_bound": (d.get("trace") or {}).get("authored"), "numos": S.answer_text(a), "adjacent_numbers": adjacent}
    rec["paths"]["edit"] = prec
    F = []
    case2 = dict(case, eqs=[None])
    parts = split_eq(visual)
    info = dict(keys=keys, displayed=[slinear(disp)], visual=prec["visual"], giac_bound=prec["giac_bound"],
                policy=case["policy"], angle="rad")
    if parts is None or not parts[0][1] or not parts[1][1]:
        if a["status"] == "ok":
            F.append(Finding(case["id"], "edit", "I5", "S2", "NumOS solved a row without exactly one complete equality",
                             dict(info, numos=prec["numos"])))
        return rec, F, []
    if not _valid_numerals(visual):
        prec["note"] = "visual reading has a numeral with two decimal points"
        return rec, F, []
    g = S.check_giac_text(case2, "edit", d, [visual])
    if g and "mismatch" in g:
        g_raw = S.check_giac_text(case2, "edit", d, [disp])
        F.append(Finding(case["id"], "edit", "I2", "S1" if adjacent else "S2",
                         "Giac-bound text does not denote the equation as displayed"
                         + (" (adjacent number nodes: screen shows one numeral, Giac gets a product)" if adjacent and not (g_raw and "mismatch" in g_raw) else ""),
                         dict(info, check=g, matches_raw_ast=not (g_raw and "mismatch" in g_raw), numos=prec["numos"])))
    if not _free_ok(visual):
        return rec, F, []
    truth = S._truth_from_sympy(case2, [visual])
    prec["oracle_visual"] = S.truth_text(truth) if truth else "unknown"
    if truth is None:
        return rec, F, []
    F += S.judge(case2, "edit", a, [parts], truth, keys, {k: v for k, v in info.items() if k != "keys"})
    F += S.check_display(case2, "edit", a, dict(keys=keys, displayed=[slinear(disp)]))
    rec["finding_titles"] = sorted({f.title for f in F})
    return rec, F, []


def third_voice(case):
    return []


def triage(case, by_tag, record):
    return {}
