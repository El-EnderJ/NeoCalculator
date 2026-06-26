#!/usr/bin/env python3
# generate-emulator-candidates.py — produce candidate screenshots from the
# native NumOS SDL2 emulator for Phase 4B-A visual review.
#
# Runs the bundled deterministic .numos replay scripts headless and dumps each
# final frame to a PPM under out/emulator-candidates/ (git-ignored — see
# .gitignore `out/`). These are CANDIDATES: machine-generated images for a human
# to review and, if correct, promote to an accepted golden under
# tests/emulator/golden/. This script NEVER writes into the golden directory.
#
# Stdlib only, cross-platform (Windows + Linux). No image libraries.
#
# Usage:
#   python scripts/generate-emulator-candidates.py
#   python scripts/generate-emulator-candidates.py --bin .pio/build/emulator_pc/program
#   python scripts/generate-emulator-candidates.py --out-dir out/emulator-candidates
#
# Exit codes:
#   0  all candidates generated and validated (P6, 320x240, 230415 bytes)
#   1  at least one candidate failed to generate or validate
#   2  emulator binary not found

import argparse
import hashlib
import os
import subprocess
import sys

# Logical screen geometry is fixed at 320x240 (NativeHal.cpp:125-126); a P6
# header "P6\n320 240\n255\n" is 15 bytes, so a full frame is exactly
# 15 + 320*240*3 = 230415 bytes (NativeHal.cpp:679-702).
EXPECTED_HEADER = b"P6\n320 240\n255\n"
EXPECTED_SIZE = 230415

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Fixed candidate set: script stem -> (script path, --frames budget).
# Frame budgets mirror the documented "Run:" lines inside each .numos script.
CANDIDATES = [
    ("launcher_smoke", "tests/emulator/scripts/launcher_smoke.numos", 800),
    ("calc_1_plus_2", "tests/emulator/scripts/calc_1_plus_2.numos", 1400),
    ("calc_fraction_sum", "tests/emulator/scripts/calc_fraction_sum.numos", 1400),
    # Phase 5A: additional safe apps (Settings + Math Showcase). Each script
    # opens its app via `open_app`, screenshots, and asserts the active app name;
    # a failed assert_app exits 4, which this generator reports as FAIL. No golden
    # is blessed here — the compare step warns (not fails) when a golden is absent.
    ("settings_smoke", "tests/emulator/scripts/settings_smoke.numos", 800),
    ("math_showcase_smoke", "tests/emulator/scripts/math_showcase_smoke.numos", 800),
    # Phase 6A: additional safe LVGL-only educational apps (Statistics + Probability).
    # Each script opens its app via `open_app`, screenshots, and asserts the active
    # app name; a failed assert_app exits 4, which this generator reports as FAIL.
    # No golden is blessed here — the compare step warns (not fails) when a golden
    # is absent.
    ("statistics_smoke", "tests/emulator/scripts/statistics_smoke.numos", 800),
    ("probability_smoke", "tests/emulator/scripts/probability_smoke.numos", 800),
    # Phase 6C: data-driven interaction smokes that go beyond the default open state.
    # statistics_data_smoke types the dataset {1,2,3} and switches to the computed
    # Stats tab; probability_edit_smoke edits the mu parameter and recomputes the
    # PDF/CDF. Each script also asserts the active app name (exit 4 -> FAIL here). No
    # golden is blessed here — the compare step warns (not fails) when a golden is absent.
    ("statistics_data_smoke", "tests/emulator/scripts/statistics_data_smoke.numos", 800),
    ("probability_edit_smoke", "tests/emulator/scripts/probability_edit_smoke.numos", 800),
    # Phase 7A: SequencesApp (LVGL-only 2-tab Define/Table). sequences_smoke opens the
    # app on its default Define tab; sequences_edit_smoke edits u(n) -> "2*n+5" and views
    # the recomputed Table tab. Each script also asserts the active app name (exit 4 ->
    # FAIL here). No golden is blessed here — the compare step warns (not fails) when a
    # golden is absent.
    ("sequences_smoke", "tests/emulator/scripts/sequences_smoke.numos", 800),
    ("sequences_edit_smoke", "tests/emulator/scripts/sequences_edit_smoke.numos", 800),
    # Phase 7C: RegressionApp (LVGL-only 3-tab Data/Equation/Graph). regression_smoke opens
    # the app on its default empty Data tab; regression_data_smoke types the three points
    # {(1,2),(2,4),(3,6)} and switches to the computed Equation tab (linear fit y=2x). Each
    # script also asserts the active app name (exit 4 -> FAIL here). No golden is blessed
    # here — the compare step warns (not fails) when a golden is absent.
    ("regression_smoke", "tests/emulator/scripts/regression_smoke.numos", 800),
    ("regression_data_smoke", "tests/emulator/scripts/regression_data_smoke.numos", 800),
    # Phase 8G: GrapherApp (LVGL-native NumWorks-style grapher, backed by the legacy
    # RPN pipeline native-ported in this phase). grapher_smoke opens the app on its
    # default Expressions tab (no function entry, no plotting, no Graph/Table switch)
    # and asserts the active app name (exit 4 -> FAIL here). No golden is blessed here
    # — the compare step warns (not fails) when a golden is absent.
    ("grapher_smoke", "tests/emulator/scripts/grapher_smoke.numos", 800),
    # Phase 8H: one step deeper — grapher_expr_smoke types the function "x" into the
    # first slot (down, enter, x, enter) and commits it, staying on the Expressions
    # tab (no Graph tab, no plotting). It also asserts the active app name (exit 4 ->
    # FAIL here). No golden is blessed here — the compare step warns (not fails) when
    # a golden is absent.
    ("grapher_expr_smoke", "tests/emulator/scripts/grapher_expr_smoke.numos", 800),
    # Phase 8I: Graph-tab render — grapher_graph_smoke commits the plottable function
    # y=x, switches to the Graph tab (createGraphPanel + native PSRAMBuffer std::malloc
    # allocation), and flushes the app's deferred first replot (ac + neutral key, no
    # pan/zoom/trace) so the grid + axes + curve rasterize into the native buffer. It
    # asserts the active app name (exit 4 -> FAIL here). Run1-vs-run2 is byte-identical
    # (deterministic graph body). No golden is blessed here — the compare step warns
    # (not fails) when a golden is absent.
    ("grapher_graph_smoke", "tests/emulator/scripts/grapher_graph_smoke.numos", 800),
    # Phase 8L: Table-tab render — grapher_table_smoke commits y=x and switches to the
    # Table tab (`key table` -> switchTab(Tab::TABLE) -> synchronous rebuildTable(); no
    # deferral/flush). It captures the sticky header (x / f1(x)) over the zebra-striped
    # value table and asserts the active app name (exit 4 -> FAIL here). The Table body
    # is pure numbers (no stix_math_18 no-space tofu). Run1-vs-run2 is byte-identical.
    # No golden is blessed here — the compare step warns (not fails) when a golden is
    # absent.
    ("grapher_table_smoke", "tests/emulator/scripts/grapher_table_smoke.numos", 800),
    # Phase 8N: Trace mode — grapher_trace_smoke commits y=x, switches to the Graph tab,
    # and advances the trace cursor once (`right`) to reveal the crosshair + floating
    # value pill (drawTraceCursor) over the plotted curve. It asserts the active app
    # name (exit 4 -> FAIL here). The trace pill plain-prose readout was switched to
    # lv_font_montserrat_14 in this phase (no tofu). Run1-vs-run2 is byte-identical (the
    # async POI timer is frame-deterministic). No golden is blessed here — the compare
    # step warns (not fails) when a golden is absent.
    ("grapher_trace_smoke", "tests/emulator/scripts/grapher_trace_smoke.numos", 800),
]


def resolve_binary(explicit):
    """Locate the emulator binary. Resolution order matches the run scripts /
    quickstart: --bin > PLATFORMIO_BUILD_DIR > build_dir in platformio.ini
    (ignored on non-Windows when it is a C:\\ path) > .pio/build."""
    exe = "program.exe" if os.name == "nt" else "program"
    if explicit:
        return explicit

    candidates = []
    env_dir = os.environ.get("PLATFORMIO_BUILD_DIR")
    if env_dir:
        candidates.append(os.path.join(env_dir, "emulator_pc", exe))

    # build_dir from platformio.ini (top [platformio] section).
    ini = os.path.join(REPO_ROOT, "platformio.ini")
    try:
        with open(ini, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                s = line.strip()
                if s.startswith("build_dir") and "=" in s:
                    val = s.split("=", 1)[1].strip()
                    # On non-Windows a C:\ path is meaningless; skip it.
                    if os.name != "nt" and len(val) >= 2 and val[1] == ":":
                        break
                    candidates.append(os.path.join(val, "emulator_pc", exe))
                    break
    except OSError:
        pass

    candidates.append(os.path.join(REPO_ROOT, ".pio", "build", "emulator_pc", exe))

    for c in candidates:
        if os.path.isfile(c):
            return c
    # Fall back to the last candidate so the error message is concrete.
    return candidates[-1]


def sdl2_dll_dir(binary):
    """On Windows, locate the directory containing SDL2.dll so it can be prepended
    to PATH for the emulator subprocess. Mirrors run-emulator-windows.ps1's order:
    next to the exe > NUMOS_SDL2_ROOT / SDL2_DIR / SDL2_ROOT \\bin >
    C:\\SDL2\\x86_64-w64-mingw32\\bin. Returns a dir or None. On Linux/macOS this
    returns None (the system loader resolves libSDL2 with no PATH help)."""
    if os.name != "nt":
        return None
    exe_dir = os.path.dirname(os.path.abspath(binary))
    if os.path.isfile(os.path.join(exe_dir, "SDL2.dll")):
        return exe_dir
    roots = [
        os.environ.get("NUMOS_SDL2_ROOT"),
        os.environ.get("SDL2_DIR"),
        os.environ.get("SDL2_ROOT"),
        r"C:\SDL2\x86_64-w64-mingw32",
    ]
    for r in roots:
        if not r:
            continue
        cand = os.path.join(r, "bin", "SDL2.dll")
        if os.path.isfile(cand):
            return os.path.dirname(cand)
    return None


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def validate(path):
    """Return (ok, message). Checks P6 320x240 header and exact 230415-byte size."""
    try:
        size = os.path.getsize(path)
    except OSError as exc:
        return False, "missing: %s" % exc
    if size != EXPECTED_SIZE:
        return False, "size %d != expected %d" % (size, EXPECTED_SIZE)
    with open(path, "rb") as f:
        head = f.read(len(EXPECTED_HEADER))
    if head != EXPECTED_HEADER:
        return False, "header %r != expected P6 320x240" % head
    return True, "ok"


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate NumOS emulator candidate screenshots (Phase 4B-A)."
    )
    parser.add_argument("--bin", default=None, help="path to the emulator binary")
    parser.add_argument(
        "--out-dir",
        default=os.path.join("out", "emulator-candidates"),
        help="directory for candidate PPMs (default: out/emulator-candidates)",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=None,
        help="override the per-script --frames budget",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="per-run wall-clock timeout in seconds (default 120)",
    )
    args = parser.parse_args(argv)

    binary = resolve_binary(args.bin)
    if not os.path.isfile(binary):
        print("ERROR: emulator binary not found: %s" % binary, file=sys.stderr)
        print("Build it first: pio run -e emulator_pc", file=sys.stderr)
        return 2
    print("emulator binary: %s" % binary)

    out_dir = args.out_dir if os.path.isabs(args.out_dir) else os.path.join(REPO_ROOT, args.out_dir)
    os.makedirs(out_dir, exist_ok=True)
    # The bundled scripts also do an in-script `screenshot out/<name>.ppm`
    # (relative to CWD); ensure out/ exists so those writes don't fail (exit 3).
    os.makedirs(os.path.join(REPO_ROOT, "out"), exist_ok=True)

    # Deterministic + headless even on display-less hosts.
    env = dict(os.environ)
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    dll_dir = sdl2_dll_dir(binary)
    if dll_dir:
        env["PATH"] = dll_dir + os.pathsep + env.get("PATH", "")
        print("SDL2.dll dir   : %s" % dll_dir)

    failures = 0
    for name, script_rel, frames in CANDIDATES:
        script_abs = os.path.join(REPO_ROOT, script_rel)
        if not os.path.isfile(script_abs):
            print("FAIL %-18s missing script %s" % (name, script_rel))
            failures += 1
            continue
        cand = os.path.join(out_dir, name + ".ppm")  # absolute, for validation
        # The emulator opens paths through the narrow-CRT (std::ifstream/fopen),
        # which mangles non-ASCII absolute paths on Windows (e.g. a "Juan Ramón"
        # home dir). Pass ASCII paths RELATIVE to the (correctly wide) cwd instead;
        # the OS resolves them against REPO_ROOT regardless of its encoding.
        script_arg = script_rel
        try:
            cand_arg = os.path.relpath(cand, REPO_ROOT).replace(os.sep, "/")
        except ValueError:
            cand_arg = cand  # different drive; fall back to absolute
        nframes = args.frames if args.frames is not None else frames
        cmd = [
            binary,
            "--headless",
            "--deterministic",
            "--script", script_arg,
            "--frames", str(nframes),
            "--screenshot", cand_arg,
            "--quiet",
        ]
        print("--- %s (frames=%d) ---" % (name, nframes))
        try:
            proc = subprocess.run(
                cmd, cwd=REPO_ROOT, env=env, timeout=args.timeout,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
        except subprocess.TimeoutExpired:
            print("FAIL %-18s timed out after %ds" % (name, args.timeout))
            failures += 1
            continue
        if proc.returncode != 0:
            tail = proc.stdout.decode("utf-8", "replace").strip().splitlines()[-5:]
            print("FAIL %-18s emulator exit %d" % (name, proc.returncode))
            for ln in tail:
                print("      | %s" % ln)
            failures += 1
            continue
        ok, msg = validate(cand)
        if not ok:
            print("FAIL %-18s %s" % (name, msg))
            failures += 1
            continue
        print("OK   %-18s %s  sha256=%s" % (name, cand, sha256_file(cand)))

    total = len(CANDIDATES)
    print("=== %d/%d candidates generated into %s ===" % (total - failures, total, out_dir))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
