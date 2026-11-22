"""Ad-hoc W1 probe: run literal key sequences (and giac_eval strings) in ONE
fresh emulator process and print input / Giac-bound text / result / display
next to an independent SymPy+mpmath value.

    py -3.12 tests/e2e/calculation/w1_probe.py <probe-file.json> [--out DIR]

probe file: [{"name":..., "angle": "rad|deg", "keys": "...", "oracle": "<sympy expr>",
              "giac": "<canonical giac text>"(optional), "vars": true|false}, ...]
`@t:<text>` in keys = Toolbox search text; `deg(x)` in the oracle = x degrees.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import sympy as sp  # noqa: E402

from common.emulator import DEFAULT_EMULATOR, run_script, script_header  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from calculation.driver import preamble  # noqa: E402


def keylines(keys):
    out = []
    for tok in keys.split():
        if tok.startswith("@t:"):
            out.append(f"sdl_text {tok[3:]}")
        elif tok.startswith("@angle:"):
            out.append(f"set_angle_mode {tok[7:]}")
        else:
            out.append(f"key {tok}")
    return out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("probes")
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--out", default=None)
    a = ap.parse_args(argv)
    probes = json.loads(Path(a.probes).read_text(encoding="utf-8"))
    lines = script_header("Calculation")
    if any(p.get("vars") for p in probes):
        lines += preamble()
    for i, p in enumerate(probes):
        lines += [f"set_angle_mode {p.get('angle', 'rad')}", "key ac", "key ac"] + keylines(p["keys"])
        lines += [f"dump_calc q{i}"]
        if p.get("giac"):
            lines += [f"giac_eval q{i}|g {p['giac']}"]
    r = run_script(lines, a.emulator, timeout=300)
    rows = []
    ns = {"deg": lambda x: x * sp.pi / 180}
    for i, p in enumerate(probes):
        d = r.by_tag.get(f"q{i}") or {}
        g = r.by_tag.get(f"q{i}|g") or {}
        o = None
        if p.get("oracle"):
            try:
                o = str(sp.N(sp.sympify(p["oracle"], locals=ns), 30))
            except Exception as exc:  # noqa: BLE001
                o = f"oracle error {exc}"
        row = {"name": p["name"], "angle": p.get("angle", "rad"), "keys": p["keys"],
               "input": slinear(parse(d["input"])) if d.get("input") else None, "giac_in": d.get("giac_in"),
               "status": d.get("status"), "exact": d.get("exact"), "approx": d.get("approx"),
               "display": slinear(parse(d["display"])) if d.get("display") else d.get("display_label"),
               "format": d.get("format_name"), "oracle30": o,
               "giac_eval": {"text": p.get("giac"), "status": g.get("status"), "exact": g.get("exact"),
                             "approx": g.get("approx")} if p.get("giac") else None}
        rows.append(row)
        print(f"[{i}] {p['name']} ({row['angle']}) keys: {p['keys']}")
        print(f"     input={row['input']}  giac_in={row['giac_in']}")
        print(f"     {row['status']}: exact={row['exact']} approx={row['approx']} display={row['display']} fmt={row['format']}")
        print(f"     oracle={o}")
        if row["giac_eval"]:
            print(f"     giac_eval({p['giac']}) -> {g.get('status')} {g.get('exact')} ~ {g.get('approx')}")
    print("rc", r.returncode)
    if a.out:
        Path(a.out).write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
