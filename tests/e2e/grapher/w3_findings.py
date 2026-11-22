"""W3 finding classes: minimal standalone reproducers, run twice in fresh processes.

py -3.12 tests/e2e/grapher/w3_findings.py [--only W3-F04]
Writes out/correctness-e2e-01/grapher/findings/<ID>/{repro.numos,finding.json,run1.json,run2.json}.
Each repro is keys only (+ dump_graph / giac_eval / assert lines); oracle values
are computed here with mpmath (50 digits) independently of NumOS and Giac.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import mpmath as mp

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common.emulator import emulator_sha256, run_script, script_header  # noqa: E402
from grapher.gcore import EMU, parse_residuals  # noqa: E402

mp.mp.dps = 50
OUT = Path(__file__).resolve().parents[3] / "out" / "correctness-e2e-01" / "grapher" / "findings"
TB_LOG = ["toolbox", "up", "right", "right", "right", "enter", "<Logarithm>", "down", "enter"]


def F(id_, title, inv, sev, cls, keys, xs, oracle, third, angle="rad", note="", extra=(), conf="high",
      expect=None, residual=None):
    return dict(id=id_, title=title, invariant=inv, severity=sev, cls=cls, keys=keys, xs=xs,
                oracle=oracle, third=third, angle=angle, note=note, extra=list(extra), confidence=conf,
                expect=expect or {}, residual=residual)


D2R = mp.pi / 180
FINDINGS = [
    F("W3-F01", "Inequality symbol stored as an operand atom: `/` right after `<`/`>` captures the symbol "
      "as numerator (and a Toolbox insertion after it gets an automatic ×); the well-formed relation is refused",
      "I5", "S3", "pipeline (editor)", "y lt / 1 right x".split(), [],
      {"relation": "y < 1/x", "note": "well-formed; e.g. (x,y)=(2,0): 0 < 0.5 true"},
      [], expect={"valid": False},
      extra=["y lt " + " ".join(TB_LOG) + " 2 right x"],
      note="With `=` both key sequences build the intended tree (adv-h01 table). Variant in `extra`."),
    F("W3-F02", "Decimal literal right after a variable/constant is refused (`y = x 2.5` -> unknown: x2)",
      "I5", "S3", "pipeline (Grapher text tokenizer)", "y = x 2 . 5".split(), [2.0],
      {"y(2)": "5"}, ["x*2.5"], expect={"valid": False},
      note="`x 2`, `x .5`, `x*2.5`, `2.5x` are accepted; also `pi 2.5`, `e 0.5`, `x = y 2.5`."),
    F("W3-F03", "Implicit multiplication before a function or sqrt is refused (`2sin(x)`, `x ln(x)`, `x√x`)",
      "I5", "S3", "pipeline (Grapher text tokenizer)", "y = 2 sin x".split(), [1.0],
      {"y(1)": mp.nstr(2 * mp.sin(1), 20)}, ["2*sin(x)"], expect={"valid": False},
      extra=["y = x ln x", "y = x sqrt x"],
      note="Reasons: `2 sin x` -> syntax; `x ln x` -> unknown: l; `x sqrt x` -> unknown: s; "
           "`(x) ln x` -> syntax. `x log_2(x)` (template) is accepted."),
    F("W3-F04", "Number juxtaposed with the constant e is read as scientific notation: `y = 2e + x` plots 2x, "
      "`y = 3e` plots 3",
      "I1", "S1", "pipeline (Grapher text tokenizer)", "y = 2 e + x".split(), [1.0, -2.5],
      {"y(1)": mp.nstr(2 * mp.e + 1, 20), "y(-2.5)": mp.nstr(2 * mp.e - 2.5, 20)},
      ["2*e+x"], extra=["y = 3 e", "y = 6 e - x", "y = pi + 3 e"],
      note="Display shows `2e + x`; Giac-bound text is `((2)*x)`. `x e`, `e 3`, `2 pi`, `2 e^x` are correct."),
    F("W3-F05", "DEG mode ignored for a trig function of a constant inside an x-dependent expression: "
      "`y = sin(30)·x` uses sin(30 rad)",
      "I1", "S1", "pipeline (Grapher angle handling)", "y = sin 3 0 right * x".split(), [1.0, 2.0],
      {"y(1)": "0.5", "y(2)": "1"}, ["sin(30)*x"], angle="deg",
      extra=["y = cos 3 right + x", "x = sin 3 0 right + y"],
      note="`y = cos(3)` alone and `cos(3x)`, `cos(x+3)` respect DEG; adding x makes the constant trig RAD."),
    F("W3-F06", "Finite value plotted at an exact pole: DEG `y = tan(x)` at x=90 gives 1.633e16 as a valid sample",
      "I1", "S4", "giac/float (needs policy decision)", "y = tan x".split(), [90.0, 270.0, -90.0],
      {"y(90)": "undefined (pole)", "y(270)": "undefined (pole)"}, ["tan(90)"], angle="deg",
      conf="medium",
      note="x·π/180 in double is not exactly π/2; also RAD `log_2(cos(πx))` at x=0.5. Policy: report poles as gaps?"),
    F("W3-F07", "Unary minus after an operator is refused (`y = x*-2`, `y = 2--x`)",
      "I5", "S3", "capability (binary-only classifier parser)", "y = x * neg 2".split(), [1.0],
      {"y(1)": "-2"}, ["x*(-2)"], expect={"valid": False}, extra=["y = 2 - neg x"],
      note="Leading unary minus (`y = -x+1`) and `(-x)` are accepted."),
    F("W3-F08", "Keys silently dropped by the Grapher editor: EXP (`2 EXP 3` types `23`) and SHIFT "
      "(SHIFT sin types sin, no inverse trig)",
      "I5", "S3", "capability (editor key map)", "y = 2 exp 3 * x".split(), [1.0],
      {"intended y(1)": "2000", "built": "23x"}, ["2E3*x"], extra=["y = shift sin x"], conf="medium",
      note="Display shows the built tree (23·x, sin(x)), so no WYSIWYC violation; the keystroke is lost silently. "
           "The Grapher Toolbox offers only Fraction/Power/Paren/sqrt/10^/log_b/x/y (abs, nthroot: "
           "'Unavailable in this editor')."),
    F("W3-F09", "Negative exponent typed with unary minus: `y = x^(-1)` / `x^(-2)` have no points for x < 0",
      "I1", "S3", "pipeline/giac (exponent `(0)-(1)` evaluated as a real power)", "y = x ^ neg 1 right".split(),
      [-2.5, -1.0, 2.0], {"y(-2.5)": "-0.4", "y(-1)": "-1", "y(2)": "0.5"}, ["subst(x^(-1),x=-2.5)"],
      extra=["y = x ^ neg 2 right", "y = x ^ 2 - 3 right", "y = x ^ 3 right"],
      note="Displayed {x}^{-1}; Giac text `(x^((0)-(1)))`. Gap instead of the left branch of 1/x; arguably S1 "
           "(visibly wrong graph)."),
    F("W3-F10", "Undefined sub-expression absorbed: `y = 1/((x-x)^x)` plots 0 at x<0 where (0)^(negative) is a pole",
      "I1", "S1", "giac (projective arithmetic: 1/oo = 0) — needs policy decision",
      "y = / 1 right ( x - x right ^ x right right".split(), [-2.5, -1.0, 2.0],
      {"y(-2.5)": "undefined (0^-2.5 pole)", "y(-1)": "undefined", "y(2)": "undefined (1/0)"},
      ["subst(1/((x-x)^x),x=-2.5)"], extra=["y = / 1 right 0 ^ x right right"], conf="medium",
      note="Same family as Calculation's '6 ÷ (E/0) = 0'. Policy: harness treats any undefined sub-expression "
           "as undefined; NumOS/Giac use projective infinity."),
]


def lines_for(f, keys):
    out = script_header("Grapher") + [f"set_angle_mode {f['angle']}", "key down", "key enter"]
    for k in keys:
        out.append(f"sdl_text {k[1:-1]}" if k.startswith("<") else f"key {k}")
    return out + ["key enter", "wait 3"]


def run_f(f):
    variants = [f["keys"]] + [e.split() for e in f["extra"]]
    res = []
    for vi, keys in enumerate(variants):
        lines = lines_for(f, keys) + [f"dump_graph v{vi} 0 {' '.join(map(str, f['xs'] or [1.0]))}"]
        if vi == 0:
            lines += [f"giac_eval tv{j} {t}" for j, t in enumerate(f["third"])]
        r = run_script(lines, EMU, timeout=10)
        d = r.by_tag.get(f"v{vi}") or {}
        res.append({"keys": " ".join(keys), "rc": r.returncode, "input": d.get("input"), "text": d.get("text"),
                    "valid": d.get("valid"), "kind": d.get("kind"), "reason": d.get("reason"),
                    "giac": d.get("giac_rpn"), "values": d.get("values"),
                    "third": {k: {"giac_in": v.get("giac_in"), "approx": v.get("approx"), "exact": v.get("exact")}
                              for k, v in r.by_tag.items() if k and k.startswith("tv")}})
    return res, lines_for(f, f["keys"]) + [f"dump_graph v0 0 {' '.join(map(str, f['xs'] or [1.0]))}"] + \
        [f"giac_eval tv{j} {t}" for j, t in enumerate(f["third"])]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    a = ap.parse_args()
    sha = emulator_sha256(EMU)
    for f in FINDINGS:
        if a.only and f["id"] != a.only:
            continue
        d = OUT / f["id"]
        d.mkdir(parents=True, exist_ok=True)
        r1, script = run_f(f)
        r2, _ = run_f(f)
        (d / "repro.numos").write_text("# " + f["id"] + ": " + f["title"] + "\n" + "\n".join(script) + "\n",
                                       encoding="utf-8", newline="\n")
        for k, r in (("run1", r1), ("run2", r2)):
            (d / f"{k}.json").write_text(json.dumps(r, indent=1, ensure_ascii=False), encoding="utf-8")
        stable = json.dumps(r1, sort_keys=True) == json.dumps(r2, sort_keys=True)
        fj = {k: f[k] for k in ("id", "title", "invariant", "severity", "cls", "angle", "note", "confidence")}
        fj.update({"keys": " ".join(f["keys"]), "variants": [x["keys"] for x in r1],
                   "displayed_ast": r1[0]["input"], "slot_text": r1[0]["text"], "giac_string": r1[0]["giac"],
                   "numos_valid": r1[0]["valid"], "numos_reason": r1[0]["reason"], "numos_values": r1[0]["values"],
                   "variant_results": [{k: x[k] for k in ("keys", "text", "valid", "reason", "giac", "values")}
                                       for x in r1[1:]],
                   "oracle": f["oracle"], "oracle_method": "mpmath 50 digits / SymPy on the intended tree",
                   "third_voice_giac_eval": r1[0]["third"], "reruns_identical": stable,
                   "emulator_sha256": sha})
        (d / "finding.json").write_text(json.dumps(fj, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f["id"], "stable" if stable else "UNSTABLE", "|", r1[0]["text"], r1[0]["valid"], r1[0]["reason"],
              r1[0]["giac"], r1[0]["values"], "|", [(x["text"], x["valid"], x["reason"], x["values"]) for x in r1[1:]])


if __name__ == "__main__":
    main()
