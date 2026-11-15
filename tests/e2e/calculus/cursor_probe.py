"""Targeted cursor-insertion probe on Calculus (re-plan item b).

Calculation: `3 LEFT 5` displays 53 but computes 5*3 = 15. Here each pattern
moves the cursor and then inserts a digit/variable at a node boundary, in
both tabs. For every run: displayed input AST, what the eye reads (adjacent
Number nodes concatenated), Giac-bound string, result, and check.judge
(incl. the adjacent-number-merge class).

    python tests/e2e/calculus/cursor_probe.py [--emulator PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

from common.emulator import DEFAULT_EMULATOR, run_script, script_header  # noqa: E402
from common.sexpr import linear as slinear, parse  # noqa: E402
from calculus.check import judge, merge_adjacent_numbers  # noqa: E402

PATTERNS = [
    "3 left 5", "x * 3 left 5", "3 left 5 x", "2 x left left 4", "1 2 left 5",
    "1 2 left left 5", "x ^ 2 right 3 left 5", "3 left x", "x left 3", "5 left .",
    "2 . 5 left left 1", "sin x right 3 left 4", "/ 1 right 2 right 3 left 4",
    "( x + 1 right 2 left 3", "3 x left 4", "4 left neg", "x left 2 ^ 3",
    "1 0 left 0", "e left 2", "pi left 2", "2 left pi",
]


def run(emulator, out_dir):
    lines = script_header("Calculus") + ["set_angle_mode rad"]
    for i, keys in enumerate(PATTERNS):
        for op, mk in (("diff", "f1"), ("int", "f2")):
            lines += ["key ac", "key ac", f"key {mk}"] + [f"key {k}" for k in keys.split()] + \
                     ["key enter", f"dump_calculus k{i}|{op}"]
    r = run_script(lines, emulator, timeout=120)
    rows = []
    for i, keys in enumerate(PATTERNS):
        for op in ("diff", "int"):
            d = r.by_tag.get(f"k{i}|{op}")
            if not d:
                rows.append({"keys": keys, "op": op, "verdict": "NO DUMP"})
                continue
            st = parse(d["input"])
            j = judge(op, "rad", st, d)
            rows.append({"keys": keys, "op": op, "input": slinear(st),
                         "eye_reads": slinear(merge_adjacent_numbers(st)), "giac_in": d.get("giac_in"),
                         "status": d.get("status"), "exact": d.get("exact"), "verdict": j["verdict"],
                         "findings": [f[:4] for f in j["findings"]]})
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "cursor_probe.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
    md = ["| keys | tab | input AST | eye reads | Giac-bound | result | verdict / findings |", "|---|---|---|---|---|---|---|"]
    for x in rows:
        md.append(f"| `{x['keys']}` | {x['op']} | {x.get('input')} | {x.get('eye_reads')} | `{x.get('giac_in')}` | "
                  f"`{x.get('exact')}` | {x['verdict']} {[f[2] for f in x.get('findings', [])]} |")
    (out_dir / "cursor_probe.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return rows


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--out", default=str(HERE.parent.parent / "out" / "correctness-e2e-01" / "calculus" / "cursor"))
    a = ap.parse_args()
    run(a.emulator, Path(a.out))
    print(Path(a.out, "cursor_probe.md").read_text(encoding="utf-8"))
