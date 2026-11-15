"""Minimal standalone reproducers for the Equations finding classes (W5).

For every class: a hand-minimised case (the smallest equation of the class
seen in the seeded runs), typed with real keys on the direct path, solved,
dumped, plus the third voice (`giac_eval` on canonical Giac text, bypassing
keys/AST/serializer). Writes
  out/correctness-e2e-01/equations/findings/<ID>/repro.numos
  out/correctness-e2e-01/equations/findings/<ID>/finding.json
and re-runs every reproducer twice, each in a fresh emulator process.

    py -3.12 tests/e2e/equations/repro.py [--only W5-F01]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import sympy as sp  # noqa: E402

from common.emulator import DEFAULT_EMULATOR  # noqa: E402
from common.expr import Abs, Const, Fn, Frac, LogB, Num, Op, Pow, Root, Row, Sqrt, Var  # noqa: E402
from equations import flow, surface as S  # noqa: E402
from equations.gen import poly_row  # noqa: E402

X, Y = Var("x"), Var("y")
N = lambda n: Num(str(n))  # noqa: E731


def R(*a):
    return Row(tuple(a))


def case(cid, eqs, truth, policy="real", angle="rad", vars_=None):
    return {"id": cid, "seed": 0, "index": 1, "family": "repro", "sub": cid, "policy": policy, "angle": angle,
            "eqs": eqs, "vars": vars_ or ["x", "y", "z"][:len(eqs)], "truth": truth, "edit_seed": 0}


def edit_case(cid, keys, policy="real"):
    return {"id": cid, "seed": 0, "index": 1, "family": "cursor", "sub": "repro", "policy": policy, "angle": "rad",
            "tokens": [("key", k) for k in keys.split()], "template": None, "ops": [], "eqs": [], "vars": ["x"],
            "truth": {"kind": "visual"}, "edit_seed": 0}


CLASSES = [
    dict(id="W5-F01", invariant="I1", severity="S1", tclass="pipeline (GiacEngine solve adapter) / giac",
         title="x under an indexed root or a fractional power: \"No solution\" shown although solutions exist",
         cases=[case("W5-F01", [(R(Root(R(N(3)), R(X))), R(N(2)))], {"kind": "finite", "sols": [sp.Integer(8)]}),
                case("W5-F01b", [(R(Pow(X, R(Frac(R(N(1)), R(N(3)))))), R(N(2)))], {"kind": "finite", "sols": [sp.Integer(8)]}),
                case("W5-F01c", [(R(Root(R(N(4)), R(X))), R(N(2)))], {"kind": "finite", "sols": [sp.Integer(16)]}, policy="complex")],
         extra_giac=["solve(surd(x,3)=2,x)", "solve(surd(x,3)-2,x)", "solve(x^(1/3)=2,x)", "solve(x^(1/3)-2,x)"],
         oracle_how="constructed: (cube of 2) = 8; residual |surd(8,3)-2| = 0 at 60 digits; SymPy solveset agrees",
         confidence="high"),
    dict(id="W5-F02", invariant="I1", severity="S1", tclass="giac (gsolve) / pipeline adapter",
         title="nonlinear 2x2 system: a solution pair is silently missing",
         cases=[case("W5-F02", [(R(Y), R(Pow(X, R(N(2))))), (R(Y), R(N(2), X))],
                     {"kind": "tuples", "sols": [(sp.Integer(0), sp.Integer(0)), (sp.Integer(2), sp.Integer(4))]})],
         extra_giac=["solve([y=x^2,y=2*x],[x,y])", "solve([y-x^2,y-2*x],[x,y])"],
         oracle_how="constructed: x^2 = 2x -> x in {0,2}; both pairs verified by substitution (60 digits); SymPy solve agrees",
         confidence="high"),
    dict(id="W5-F03", invariant="I5", severity="S3", tclass="pipeline (GiacEngine verification of gsolve output) / giac",
         title="nonlinear 2x2 system refused as unsupported (\"could not be verified\") although a finite solution set exists",
         cases=[case("W5-F03", [(R(X, Op("+"), Y), R(N(3))), (R(X, Y), R(N(2)))],
                     {"kind": "tuples", "sols": [(sp.Integer(1), sp.Integer(2)), (sp.Integer(2), sp.Integer(1))]}),
                case("W5-F03b", [(R(Pow(X, R(N(2))), Op("+"), Pow(Y, R(N(2)))), R(N(2))), (R(Y), R(X))],
                     {"kind": "tuples", "sols": [(sp.Integer(1), sp.Integer(1)), (sp.Integer(-1), sp.Integer(-1))]})],
         extra_giac=["solve([x+y=3,x*y=2],[x,y])", "solve([x^2+y^2=2,y=x],[x,y])"],
         oracle_how="constructed (Vieta / substitution); SymPy solve agrees",
         confidence="high"),
    dict(id="W5-F04", invariant="I1", severity="S1", tclass="giac (ln merge) / pipeline (candidate verification only rejects poles)",
         title="extraneous root of a logarithmic equation shown as a solution",
         cases=[case("W5-F04", [(R(Fn("ln", R(X)), Op("+"), Fn("ln", poly_row({1: 1, 0: -1}))), R(Fn("ln", R(N(6)))))],
                     {"kind": "finite", "sols": [sp.Integer(3)]}),
                case("W5-F04b", [(R(Fn("ln", R(X)), Op("+"), Fn("ln", poly_row({1: 1, 0: -1}))), R(Fn("ln", R(N(6)))))],
                     {"kind": "finite", "sols": [sp.Integer(3)]}, policy="complex")],
         extra_giac=["solve(ln(x)+ln(x-1)=ln(6),x)", "simplify(ln(-2)+ln(-3)-ln(6))"],
         oracle_how="constructed: x(x-1)=6 -> {3,-2}; -2 rejected: ln(-2) is undefined over the reals and, with principal "
                    "complex logs, ln(-2)+ln(-3) = ln 6 + 2*pi*i != ln 6 (residual check, 60 digits)",
         confidence="high"),
    dict(id="W5-F05", invariant="I1", severity="S1", tclass="pipeline (real-domain check of radicands) / giac",
         title="real policy: value making a radicand negative shown as a real solution",
         cases=[case("W5-F05", [(R(Sqrt(poly_row({1: 1, 0: -4}))), R(Sqrt(poly_row({1: -1, 0: -5}))))], {"kind": "empty"})],
         extra_giac=["solve(sqrt(x-4)=sqrt(-x-5),x)"],
         oracle_how="x-4 = -x-5 -> x = -1/2, but then x-4 = -9/2 < 0: no real solution (real-domain residual check)",
         confidence="high (real policy); in the complex policy -1/2 IS a solution (principal sqrt is injective)"),
    dict(id="W5-F06", invariant="I1", severity="S2", tclass="policy (needs policy decision)",
         title="complex policy: exponential equations shown with a finite 'N solutions | Complex' list although the complex "
               "solution set is infinite",
         cases=[case("W5-F06", [(R(Pow(N(2), R(X))), R(N(4)))], {"kind": "cplx_infinite", "real": [sp.Integer(2)]}, policy="complex"),
                case("W5-F06b", [(R(Pow(Const("e"), R(X))), R(Op("-"), N(1)))], {"kind": "cplx_infinite", "real": []}, policy="complex")],
         extra_giac=["csolve(2^x=4,x)", "csolve(exp(x)=-1,x)"],
         oracle_how="2^z = 4 <=> z = 2 + 2*pi*i*k/ln 2 (k in Z); e^z = -1 <=> z = i*pi + 2*pi*i*k",
         confidence="medium: correct for real roots; whether the complex policy must show complex families is a policy question"),
    dict(id="W5-F07", invariant="CAP", severity="S4", tclass="capability",
         title="real policy, DEG: periodic solve declined for k*sin(x)=c, only representatives listed",
         cases=[case("W5-F07", [(R(N(3), Fn("sin", R(X))), R(Frac(R(N(3), Sqrt(R(N(2)))), R(N(2)))))],
                     {"kind": "periodic", "fams": [(sp.Integer(45), sp.Integer(360)), (sp.Integer(135), sp.Integer(360))], "extra": []},
                     angle="deg")],
         extra_giac=[],
         oracle_how="sin x = sqrt(2)/2 (DEG): x = 45 + 360k or 135 + 360k",
         confidence="high (behaviour); impact cosmetic/capability: title says 'representatives'"),
    dict(id="W5-F08", invariant="I5", severity="S3", tclass="pipeline (decimal serialization -> Giac float solve) / giac",
         title="real policy: quadratic with decimal coefficients and no real root -> 'undefined' error instead of 'No solution'",
         cases=[case("W5-F08", [(R(Pow(X, R(N(2))), Op("-"), Num("4.1"), X, Op("+"), Num("6.9")), R(N(0)))], {"kind": "empty"})],
         extra_giac=["solve(x^2-4.1*x+6.9=0,x)", "solve(x^2-41/10*x+69/10=0,x)"],
         oracle_how="discriminant 4.1^2-4*6.9 = -10.79 < 0: no real root (exact rationals)",
         confidence="high"),
    dict(id="W5-F09", invariant="HANG", severity="S0", tclass="giac (complex-mode solve of a quadratic) / pipeline",
         title="complex policy: quadratic with large or near-degenerate coefficients never returns (emulator killed)",
         cases=[case("W5-F09", [(R(Pow(X, R(N(2))), Op("-"), N(12596), X, Op("-"), N(3923289500)), R(N(0)))],
                     {"kind": "finite", "sols": [sp.Integer(69250), sp.Integer(-56654)]}, policy="complex")],
         extra_giac=[], timeout=40,
         oracle_how="constructed (x-69250)(x+56654); the same equation in the real policy solves instantly",
         confidence="high (hang reproduced in fresh processes)"),
    dict(id="W5-F10", invariant="I1", severity="S1", tclass="display/format (engine result tree -> VPAM AST drops grouping)",
         title="complex root (1+i)*sqrt(2)/2 displayed as (1 + i*sqrt(2))/2",
         cases=[case("W5-F10", [(R(Pow(X, R(N(4)))), R(Op("-"), N(1)))],
                     {"kind": "finite", "sols": [s * sp.sqrt(2) / 2 for s in (1 + sp.I, 1 - sp.I, -1 + sp.I, -1 - sp.I)]},
                     policy="complex")],
         extra_giac=["csolve(x^4=-1,x)"],
         oracle_how="x^4=-1: x = (+-1 +- i)/sqrt(2); displayed formula read from the result canvas dumpTree and evaluated",
         confidence="high (engine text correct, displayed tree wrong)"),
    dict(id="W5-F11", invariant="I2", severity="S1", tclass="pipeline (CursorController digit insertion -> adjacent NodeNumber; serializer emits a product)",
         title="digit typed after moving the cursor left of a number: the screen shows one numeral (53), Giac solves a product (5*3)",
         surface="edit",
         cases=[edit_case("W5-F11", "x = 3 left 5"), edit_case("W5-F11b", "1 3 = 7 left 1 x - 1 left x right")],
         screenshot="x = 3 left 5",
         extra_giac=[], oracle_how="visual reading of the displayed row (adjacent number nodes render as one numeral, see "
                    "editor.png); x = 53 -> {53}; Giac-bound text '5*3' denotes 15",
         confidence="high"),
]


def screenshot(emulator, lines, png):
    """Second fresh process that stops in the editor, frame saved as PNG."""
    import subprocess, tempfile
    from common.emulator import _env, count_frames
    from PIL import Image
    tmp = Path(tempfile.mkdtemp(dir=str(png.parent)))
    (tmp / "fs").mkdir()
    (tmp / "s.numos").write_text(chr(10).join(lines) + chr(10), encoding="utf-8", newline=chr(10))
    ppm = tmp / "frame.ppm"
    subprocess.run([emulator, "--headless", "--deterministic", "--quiet", "--frames", str(count_frames(lines) + 20),
                    "--script", str(tmp / "s.numos"), "--fs-sandbox-dir", str(tmp / "fs"), "--screenshot", str(ppm)],
                   env=_env(), capture_output=True, timeout=60)
    if ppm.exists():
        Image.open(ppm).save(png)
    import shutil
    shutil.rmtree(tmp, ignore_errors=True)
    return png.exists()


def run_class(cls, emulator, root):
    d = root / "findings" / cls["id"]
    d.mkdir(parents=True, exist_ok=True)
    surf = cls.get("surface", S)
    S_ = S
    lines = S.header()
    metas = {}
    for c in cls["cases"]:
        l, m = (surf.case_lines_for(c, None, ["direct"]) if surf is S else surf.case_lines_for(c, None, ["edit"]))
        lines += l
        metas[c["id"]] = m
        lines += S.third_voice(c) if surf is S else []
    for i, t in enumerate(cls.get("extra_giac", [])):
        lines += ["set_angle_mode rad", f"giac_eval {cls['id']}|extra{i} {t}"]
    if surf is not S:
        hdr0 = [f"# {c['id']}: keys {surf.case_lines_for(c, None, ['edit'])[1]['edit']['keys']}" for c in cls["cases"]]
    else:
        hdr0 = []
    hdr = hdr0 + [f"# {cls['id']} {cls['invariant']}/{cls['severity']}: {cls['title']}",
           f"# tentative class: {cls['tclass']}",
           "# run (Git Bash, ASCII paths): PATH=/c/mingw64/bin:/c/SDL2/x86_64-w64-mingw32/bin:$PATH \\",
           "#   C:/.piobuild/e2e-01/emulator_pc/program.exe --headless --deterministic --quiet --frames 3000 \\",
           "#   --script repro.numos --fs-sandbox-dir <empty dir>",
           "# look for the @E2E lines (tags <case>|direct and <case>|giac) and the [TUTOR_TRACE]/[PERIODIC_RESULT] lines"]
    for c in (cls["cases"] if surf is S else []):
        hdr.append(f"# {c['id']}: {S.eq_linear(*c['eqs'][0]) if len(c['eqs']) == 1 else ' ; '.join(S.eq_linear(*e) for e in c['eqs'])}"
                   f"   policy={c['policy']} angle={c['angle']}   oracle: {S.truth_text(c['truth'])}")
    (d / "repro.numos").write_text("\n".join(hdr + lines) + "\n", encoding="utf-8", newline="\n")
    reruns = []
    for k in range(2):
        r = flow.run_script(lines, emulator, workdir=str(root / "tmp"), timeout=cls.get("timeout", 120))
        per = {}
        for c in cls["cases"]:
            pth = "direct" if surf is S else "edit"
            dd = r.by_tag.get(f"{c['id']}|{pth}")
            rec, fnd, des = surf.analyze(c, r.by_tag, metas[c["id"]], {"second": "agree"})
            g = r.by_tag.get(f"{c['id']}|giac") or {}
            per[c["id"]] = {
                "keys": metas[c["id"]][pth]["keys"],
                "displayed_ast": dd.get("equations") if dd else None,
                "giac_bound": ((dd or {}).get("trace") or {}).get("authored"),
                "status": (dd or {}).get("status"), "set_kind": (dd or {}).get("set_kind"),
                "raw_exact": (dd or {}).get("raw_exact"), "diag": (dd or {}).get("diag"),
                "numos": rec.get("paths", {}).get(pth, {}).get("numos"),
                "ui_title": rec.get("paths", {}).get(pth, {}).get("title"),
                "visual_reading": rec.get("paths", {}).get(pth, {}).get("visual"),
                "groups": (dd or {}).get("groups"),
                "families": ((dd or {}).get("view") or {}).get("families"),
                "oracle": S.truth_text(c["truth"]) if surf is S else rec.get("paths", {}).get(pth, {}).get("oracle_visual"),
                "third_voice": {"text": g.get("giac_in"), "status": g.get("status"), "exact": g.get("exact")},
                "findings": [f"{f.invariant}/{f.severity}: {f.title}" for f in fnd],
                "desync": des}
        extra = {}
        for i, t in enumerate(cls.get("extra_giac", [])):
            g = r.by_tag.get(f"{cls['id']}|extra{i}") or {}
            extra[t] = {"status": g.get("status"), "exact": g.get("exact")}
        reruns.append({"returncode": r.returncode, "cases": per, "extra_giac": extra})
    if cls["invariant"] == "HANG":
        reproduced = all(run["returncode"] == -999 for run in reruns)
    else:
        reproduced = all(any(cls["invariant"] in f for f in run["cases"][cls["cases"][0]["id"]]["findings"]) for run in reruns)
    if cls.get("screenshot"):
        shot_lines = S.header() + flow.settings("real", "rad") + list(flow.RESET) + ["key enter", "key enter"] +             [f"key {k}" for k in cls["screenshot"].split()] + ["wait 5"]
        reruns[0]["screenshot_editor_png"] = str(d / "editor.png") if screenshot(emulator, shot_lines, d / "editor.png") else None
    rec = {k: cls[k] for k in ("id", "invariant", "severity", "tclass", "title", "oracle_how", "confidence")}
    rec["reproduced_in_both_fresh_runs"] = reproduced
    rec["identical_reruns"] = json.dumps(reruns[0], sort_keys=True) == json.dumps(reruns[1], sort_keys=True)
    rec["runs"] = reruns
    (d / "finding.json").write_text(json.dumps(rec, indent=1, ensure_ascii=False, default=str), encoding="utf-8")
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--only", default=None)
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "equations"))
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    from equations.edit import surface as ES
    for cls in CLASSES:
        if cls.get("surface") == "edit":
            cls["surface"] = ES
        if a.only and cls["id"] != a.only:
            continue
        rec = run_class(cls, a.emulator, Path(a.out))
        c0 = rec["runs"][0]["cases"][cls["cases"][0]["id"]]
        print(cls["id"], "reproduced x2:", rec["reproduced_in_both_fresh_runs"], "identical:", rec["identical_reruns"],
              "|", c0["numos"], "| oracle", c0["oracle"], "| giac3:", c0["third_voice"].get("exact"))
        for t, v in rec["runs"][0]["extra_giac"].items():
            print("     giac_eval", t, "->", v)


if __name__ == "__main__":
    main()
