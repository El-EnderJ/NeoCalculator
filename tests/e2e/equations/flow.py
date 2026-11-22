"""App navigation for the Equations surface and the dump reader.

Observed app states (src/apps/EquationsApp.cpp, probes in
out/correctness-e2e-01/scratch/w5/):
  * list items: E1..En, "+ Add an equation" (n<3), "Solve ..." (n>0);
  * ENTER on "+ Add" opens the template list, ENTER on "Blank equation" opens
    an empty editor; ENTER/EXE in the editor confirms (focus -> that row);
  * DEL on a focused equation row removes it; AC in RESULT goes back to the
    list, AC in the list is a no-op. BACK in the list LEAVES the app (to the
    launcher), so the reset never uses BACK;
  * solving is synchronous inside the ENTER handler.

Per solve the case emits three always-passing read-only hooks:
  dump_equations TAG          -> @E2E {...}   (rows, status, set kind, groups)
  assert_equations trace dump -> [TUTOR_TRACE] {...}  (authored = Giac-bound
                                 lhs/rhs strings, exactly what was solved)
  assert_equations periodic dump -> [PERIODIC_RESULT] {...} (coverage, origin,
                                 title, periodic families, dumpTree of every
                                 displayed result formula)
"""
from __future__ import annotations

import json

from common import emulator as E

RESET = ["key ac", "key up", "key up", "key up", "key up", "key del", "key del", "key del"]


def add_equation(token_lines):
    return ["key enter", "key enter"] + list(token_lines) + ["key enter", "key down"]


def solve_lines(tag):
    return ["key down", "key down", "key down", "key enter", "wait 1",
            f"dump_equations {tag}", "assert_equations trace dump", "assert_equations periodic dump"]


def settings(policy, angle):
    return [f"set_equations_complex_policy {policy}", f"set_angle_mode {angle}"]


def attach_side_dumps(stdout: str) -> dict:
    """Parse stdout; return {tag: dump} where each dump_equations record gets
    the following [TUTOR_TRACE]/[PERIODIC_RESULT] lines as `trace`/`view`."""
    out, last = {}, None
    for line in stdout.splitlines():
        if line.startswith("@E2E "):
            try:
                d = json.loads(line[5:])
            except json.JSONDecodeError:
                continue
            out.setdefault(d.get("tag"), d)
            last = d if d.get("surface") == "equations" else None
        elif line.startswith("[TUTOR_TRACE] ") and last is not None and "trace" not in last:
            try:
                t = json.loads(line[14:])
                last["trace"] = {"authored": t.get("authored"), "variables": t.get("variables"),
                                 "bindings": t.get("bindings"), "complex": t.get("complex"),
                                 "degrees": t.get("degrees"), "status": t.get("status")}
            except json.JSONDecodeError:
                last["trace"] = {"error": "unparsable"}
        elif line.startswith("[PERIODIC_RESULT] ") and last is not None and "view" not in last:
            try:
                last["view"] = json.loads(line[18:])
            except json.JSONDecodeError:
                last["view"] = {"error": "unparsable"}
    return out


_orig_run_script = E.run_script


def run_script(lines, emulator=E.DEFAULT_EMULATOR, workdir=None, keep=False, timeout=600.0, extra_frames=50):
    """common.emulator.run_script + the side dumps folded into by_tag."""
    r = _orig_run_script(lines, emulator, workdir=workdir, keep=keep, timeout=timeout, extra_frames=extra_frames)
    r.by_tag = attach_side_dumps(r.stdout)
    return r


def install():
    """Route the shared runner/shrinker through run_script above (override
    local to processes that import the equations surface)."""
    import common.runner as R
    import common.shrink as S
    R.run_script = run_script
    S.run_script = run_script
