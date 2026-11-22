"""W3 probe: run key sequences in Grapher slot(s), print dump_graph summaries.

Usage: py -3.12 tests/e2e/grapher/probe.py FILE   (FILE: one case per line,
`name | keys ... | x1 x2 ...`; keys are emulator key names, `<txt>` = sdl_text)
Each case runs in its own fresh emulator process.
"""
from __future__ import annotations
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common.emulator import run_script, script_header  # noqa: E402

EMU = "C:/.piobuild/e2e-01/emulator_pc/program.exe"


def lines_for(keys, xs, angle="rad", slot=0):
    out = [f"set_angle_mode {angle}", "key down", "key enter"]
    for k in keys.split():
        if k.startswith("<") and k.endswith(">"):
            out.append(f"sdl_text {k[1:-1]}")
        elif k == "NEXT":       # commit and open the next slot
            out += ["key enter", "wait 3", "key down", "key enter"]
        else:
            out.append(f"key {k}")
    out += ["key enter", "wait 3"]
    return out


def run(name, keys, xs, angle="rad", slots=1, extra=()):
    lines = script_header("Grapher") + lines_for(keys, xs, angle)
    for s in range(slots):
        lines.append(f"dump_graph {name}#{s} {s} {' '.join(map(str, xs))}")
    lines += list(extra)
    r = run_script(lines, EMU, timeout=10)
    return r


if __name__ == "__main__":
    for raw in Path(sys.argv[1]).read_text().splitlines():
        if not raw.strip() or raw.startswith("#"):
            continue
        parts = [p.strip() for p in raw.split("|")]
        name, keys = parts[0], parts[1]
        xs = [float(v) for v in parts[2].split()] if len(parts) > 2 and parts[2] else [2.0]
        angle = parts[3] if len(parts) > 3 and parts[3] else "rad"
        slots = keys.count("NEXT") + 1
        r = run(name, keys, xs, angle, slots)
        for s in range(slots):
            d = r.by_tag.get(f"{name}#{s}") or {}
            print(json.dumps({"name": name, "slot": s, "rc": r.returncode, "input": d.get("input"),
                              "text": d.get("text"), "kind": d.get("kind"), "valid": d.get("valid"),
                              "reason": d.get("reason"), "giac": d.get("giac_rpn"),
                              "giac_l": d.get("giac_rpn_l"), "giac_y": d.get("giac_rpn_y"),
                              "values": d.get("values")}, ensure_ascii=False))
