"""W1 finding writer: minimal standalone reproducers + sidecar finding.json.

For every registered finding class (FINDINGS below, filled from the W1 runs):
  * writes out/correctness-e2e-01/calculation/findings/<ID>/repro.numos
    (boot + open Calculation [+ A–E preamble only if needed] + keys + dumps +
    third-voice `giac_eval` on a canonical string);
  * runs it RUNS (>=2) times, each in a fresh emulator process and sandbox;
  * evaluates the oracle (SymPy exact + mpmath, >= 30 digits) of the value the
    human reads/intends, independently of NumOS and Giac;
  * writes finding.json with the dumps of every run and a reproducibility flag.

    py -3.12 tests/e2e/calculation/w1_findings.py [--only W1-F01,W1-F02] [--runs 2]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import mpmath  # noqa: E402
import sympy as sp  # noqa: E402

from common.emulator import DEFAULT_EMULATOR, emulator_sha256, run_script, script_header  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from calculation.driver import preamble  # noqa: E402

OUT = HERE.parent.parent / "out" / "correctness-e2e-01" / "calculation" / "findings"


def oracle_digits(expr_text: str, angle: str = "rad", digits: int = 40):
    """Value of a SymPy expression string at `digits` significant digits.
    `deg(x)` in the text means x degrees -> x*pi/180."""
    if expr_text is None:
        return None
    ns = {"deg": lambda x: x * sp.pi / 180, "I": sp.I, "pi": sp.pi, "E": sp.E}
    e = sp.sympify(expr_text, locals=ns)
    if e.has(sp.zoo, sp.oo, -sp.oo, sp.nan):
        return {"sympy": str(e), "value": "nonfinite"}
    with mpmath.workdps(digits + 20):
        v = sp.N(e, digits + 10)
    return {"sympy": str(e), "value": str(sp.N(e, digits)), "method": f"SymPy exact simplification + sp.N at {digits} digits (mpmath)"}


def script(f):
    lines = script_header("Calculation")
    if f.get("needs_vars"):
        lines += preamble()
    lines += [f"set_angle_mode {f.get('angle', 'rad')}"]
    for step in f["steps"]:
        lines += ["key ac", "key ac"] if step.get("ac", True) else []
        for tok in step["keys"].split():
            if tok == "TB":  # open Toolbox and focus its search field
                lines += [f"key {k}" for k in "toolbox up right right right enter".split()]
            elif tok.startswith("@t:"):
                lines.append(f"sdl_text {tok[3:]}")       # Toolbox search text
            elif tok.startswith("@angle:"):
                lines.append(f"set_angle_mode {tok[7:]}")
            else:
                lines.append(f"key {tok}")
        for tag in step.get("dumps", []):
            lines += [f"dump_calc {f['id']}|{tag} tree"]
    if f.get("giac"):
        lines += [f"giac_eval {f['id']}|giac {f['giac']}"]
    return lines


def run_one(f, emulator, runs):
    d = OUT / f["id"]
    d.mkdir(parents=True, exist_ok=True)
    lines = script(f)
    head = [f"# {f['id']} {f['invariant']}/{f['severity']}: {f['title']}",
            f"# keys: {f['keys_summary']}",
            f"# human reading: {f.get('human', '')}",
            "# run: program.exe --headless --deterministic --quiet --frames 3000 --script repro.numos --fs-sandbox-dir <empty dir>"]
    (d / "repro.numos").write_text("\n".join(head + lines) + "\n", encoding="utf-8", newline="\n")
    results = []
    for i in range(runs):
        r = run_script(lines, emulator, workdir=str(d / "tmp"), timeout=90)
        dumps = {k: v for k, v in r.by_tag.items() if k and not str(k).startswith("pre|")}
        for v in dumps.values():
            for key in ("input", "display"):
                if v.get(key):
                    try:
                        v[key + "_linear"] = slinear(parse(v[key]))
                    except Exception:  # noqa: BLE001
                        pass
        results.append({"run": i + 1, "returncode": r.returncode, "dumps": dumps})
    import shutil
    shutil.rmtree(d / "tmp", ignore_errors=True)
    strip = lambda rr: json.dumps({k: {kk: vv for kk, vv in v.items() if kk != "tree"} for k, v in rr["dumps"].items()},
                                  sort_keys=True)
    same = all(strip(r) == strip(results[0]) and r["returncode"] == results[0]["returncode"] for r in results)
    rec = {k: v for k, v in f.items() if k not in ("steps",)}
    rec["oracle"] = oracle_digits(f.get("oracle_expr"), f.get("angle", "rad")) if f.get("oracle_expr") else f.get("oracle_text")
    rec["runs"] = results
    rec["reproduced_identically_in_fresh_processes"] = same
    rec["runs_count"] = runs
    rec["emulator_sha256"] = emulator_sha256(emulator)
    (d / "finding.json").write_text(json.dumps(rec, indent=1, ensure_ascii=False, sort_keys=True),
                                    encoding="utf-8", newline="\n")
    return d, rec


def main(argv=None):
    from calculation.w1_registry import FINDINGS
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--only", default="")
    ap.add_argument("--runs", type=int, default=2)
    a = ap.parse_args(argv)
    only = set(x for x in a.only.split(",") if x)
    for f in FINDINGS:
        if only and f["id"] not in only:
            continue
        d, rec = run_one(f, a.emulator, a.runs)
        last = rec["runs"][-1]
        print(f"{f['id']}: rc={last['returncode']} identical={rec['reproduced_identically_in_fresh_processes']}")
        for tag, v in last["dumps"].items():
            if "|giac" in tag:
                print(f"   {tag}: {v.get('status')} exact={v.get('exact')} approx={v.get('approx')}")
            else:
                print(f"   {tag}: input={v.get('input_linear')} giac_in={v.get('giac_in')} status={v.get('status')} "
                      f"exact={v.get('exact')} approx={v.get('approx')} display={v.get('display_linear') or v.get('display_label')} "
                      f"fmt={v.get('format_name')}/{v.get('fixed_places')}")
        print(f"   oracle: {rec['oracle']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
