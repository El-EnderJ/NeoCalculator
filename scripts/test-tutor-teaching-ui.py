#!/usr/bin/env python3
"""Review complete native teaching pages, with dynamically measured scroll bounds."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("eq_guided_review", ROOT / "scripts/test-equations-rebuild.py")
eq = importlib.util.module_from_spec(spec)
spec.loader.exec_module(eq)
CASES = {
    "notation-factor": ["2 x ^ 2 RIGHT + 3 x - 5 = 0"],
    "isolated": ["x = 1"],
    "reversed": ["1 = x"],
    "physical-negative": ["x = - 3"],
    "linear": ["3 * x + 5 = 2 0"],
    "both-sides": ["2 * x + 3 = x - 4"],
    "distribution": ["3 * ( 2 * x - 1 ) = 9"],
    "fraction": ["( x - 1 ) / 2 RIGHT + ( x + 1 ) / 3 RIGHT = 5"],
    "square": ["x ^ 2 RIGHT = 9"],
    "zero-product": ["x ^ 2 RIGHT - x = 0"],
    "factoring": ["x ^ 2 RIGHT - 5 * x + 6 = 0"],
    "quadratic": ["2 * x ^ 2 RIGHT + 3 * x - 4 = 0"],
    "no-real": ["x ^ 2 RIGHT + 1 = 0"],
    "rational": ["( x ^ 2 RIGHT - 1 ) / ( x - 1 ) RIGHT = 0"],
    "conditional": ["x / x RIGHT = 1"],
    "system": ["x + y = 3", "x - y = 1"],
    "dependent": ["x + y = 2", "2 * x + 2 * y = 4"],
    "inconsistent": ["x + y = 2", "2 * x + 2 * y = 5"],
    "system3": ["x + y + ALPHA y = 6", "x - y + ALPHA y = 2", "x + y - ALPHA y = 0"],
    "complex": ["x ^ 2 RIGHT + 1 = 0"],
    "quadratic-complex": ["2 * x ^ 2 RIGHT + 3 * x + 4 = 0"],
    "linear-unfamiliar": ["7 - 4 * x = 2 * x + 1"],
    "quadratic-unfamiliar": ["3 * x ^ 2 RIGHT + 2 * x - 2 = 0"],
    "quadratic-negative-b": ["2 * x ^ 2 RIGHT - 3 * x - 4 = 0"],
    "rational-unfamiliar": ["( ( 3 * x - 2 ) * ( x + 4 ) ) / ( x + 4 ) RIGHT = 0"],
    "system-unfamiliar": ["2 * x + 3 * y = 1 3", "5 * x - 2 * y = 4"],
    # Legacy tutor UI coverage retained by this complete-sequence successor.
    "negative": ["3 * x = - 9"],
    "isolated-negative": ["x = - 3"],
    "repeated": ["( x - 1 ) ^ 2 RIGHT = 0"],
}

def stable(d):
    return {k: ([{x: y for x, y in s.items() if x != "text"} for s in v] if k == "steps" else v)
            for k, v in d.items() if k not in ("micros",)}

def ast_nodes(node):
    """Inspect the existing typed display AST; this does not parse mathematics."""
    yield node
    for child in node["children"]:
        yield from ast_nodes(child)

def ast_text(node):
    children = [ast_text(child) for child in node["children"]]
    if node["type"] == 4:  # existing VPAM Fraction
        return "(" + children[0] + ")/(" + children[1] + ")"
    if node["type"] == 5:  # existing VPAM Power
        return "(" + children[0] + ")^(" + children[1] + ")"
    if node["type"] == 7:  # existing VPAM Paren
        return "(" + "".join(children) + ")"
    return node["text"] + "".join(children)

def scalar_text(text):
    # Only compare exact scalar spellings from typed coefficient nodes.
    return re.sub(r"[()\s]", "", text)

def render_contacts(out, records):
    from PIL import Image, ImageDraw
    for record in records:
        rows = record["pages"]
        width = max(len(row["frames"]) for row in rows) * 320
        sheet = Image.new("RGB", (width, len(rows) * 264), "#dddddd")
        draw = ImageDraw.Draw(sheet)
        for y, row in enumerate(rows):
            for x, name in enumerate(row["frames"]):
                frame = Image.open(out / (name + ".ppm"))
                assert frame.size == (320, 240)
                frame.save(out / (name + ".png"))
                draw.text((x * 320 + 4, y * 264 + 4), name, fill="black")
                sheet.paste(frame, (x * 320, y * 264 + 24))
        prefix = record["case"] + "-" + record["mode"]
        sheet.save(out / (prefix + "-contact.png"))
        for part, top in enumerate(range(0, sheet.height, 264 * 6)):
            sheet.crop((0, top, sheet.width, min(sheet.height, top + 264 * 6))).save(
                out / (prefix + f"-contact-part{part + 1}.png"))

def check_presentation(name, mode, trace, views):
    """Check evidence links and instructional content, independently of LVGL layout."""
    states, steps = trace["states"], trace["steps"]
    equation_refs = []
    for page in views:
        assert 0 <= page["step"] <= page["lastStep"] < len(steps), (name, "invalid page step")
        assert len(page["formulas"]) <= 4, (name, "unbounded formula widgets")
        assert not any(marker in page["prose"] for marker in ("sqrt(", "^", "*", "+/-", "!=")), (name, "source math in prose")
        for formula in page["formulas"]:
            assert steps[formula["step"]]["verdict"] == 1, (name, "formula from unverified step")
            assert formula["ast"] and formula["ast"]["children"], (name, "missing structured formula")
            kind = formula["kind"]
            if kind == "equation":
                state = states[formula["state"]]
                branch = state["branches"][formula["branch"]]
                assert formula["row"] < len(branch["equations"])
                equation_refs.append((formula["state"], formula["branch"], formula["row"]))
                if page["kind"] == "final":
                    assert not branch["rejected"], (name, "excluded candidate displayed as final solution")
            elif kind == "authored":
                assert formula["row"] < len(trace["authored"])
            elif kind == "conditions":
                conditions = states[formula["state"]]["conditions"]
                assert conditions
                assert sum(n["text"] == "≠" for n in ast_nodes(formula["ast"])) == len(conditions), (name, "condition missing structured not-equal")
            elif kind in ("operand", "row_operation"):
                assert steps[formula["step"]]["operand"] or steps[formula["step"]]["rule"] == "system.swap"
            else:
                step = steps[formula["step"]]
                assert step["rule"] == "quadratic.formula" and len(step["auxiliaries"]) == 4, (name, "quadratic facts lack checked source")
                a, b, c, disc = step["auxiliaries"]
                if kind == "coefficients":
                    assert scalar_text(ast_text(formula["ast"])) == scalar_text(f"a={a},b={b},c={c}"), (name, "coefficient values differ from checked auxiliaries")
                if kind == "discriminant_values":
                    assert scalar_text(ast_text(formula["ast"])).endswith("=" + scalar_text(disc)), (name, "wrong displayed discriminant")
                    if b.startswith("-"):
                        powers = [n for n in ast_nodes(formula["ast"]) if n["type"] == 5 and scalar_text(ast_text(n["children"][0])) == scalar_text(b)]
                        assert powers and all(n["children"][0]["type"] == 7 for n in powers), (name, "negative squared coefficient lost parentheses")
                    if c.startswith("-"):
                        assert any(n["type"] == 7 and scalar_text(ast_text(n)) == scalar_text(c) for n in ast_nodes(formula["ast"])), (name, "negative product coefficient lost parentheses")
                if kind in ("general_formula", "substituted_formula"):
                    assert any(n["text"] == "±" for n in ast_nodes(formula["ast"])), (name, "missing plus-minus AST")
                    assert any(n["type"] == 4 for n in ast_nodes(formula["ast"]))
                    assert any(n["type"] == 6 for n in ast_nodes(formula["ast"]))
                    # The glyph pixels are separately captured and reviewed:
                    # an AST assertion alone cannot establish visible ± ink.
                if kind == "substituted_formula":
                    roots = [n for n in ast_nodes(formula["ast"]) if n["type"] == 6]
                    assert any(scalar_text(ast_text(n)) == scalar_text(disc) for n in roots), (name, "substitution lost actual discriminant")
        final = states[steps[page["lastStep"]]["after"]]
        if page["kind"] == "final":
            if page["step"] != page["lastStep"]:
                assert steps[page["step"]]["rule"] == "quadratic.formula"
                assert steps[page["lastStep"]]["rule"] == "terminal.finish"
                assert steps[page["lastStep"]]["text"] in page["prose"], (name, "lost terminal explanation")
                assert all(f["step"] == page["lastStep"] for f in page["formulas"]), (name, "lost terminal formula provenance")
            shown = {(f["state"], f["branch"], f["row"]) for f in page["formulas"] if f["kind"] == "equation"}
            if final["conclusion"] in (1, 3, 4):
                expected = {(steps[page["lastStep"]]["after"], bi, ri)
                            for bi, branch in enumerate(final["branches"]) if not branch["rejected"]
                            for ri in range(len(branch["equations"]))}
                assert shown == expected, (name, "final omitted accepted solution or system relationship")
            if final["conditions"]:
                assert any(f["kind"] == "conditions" and f["state"] == steps[page["lastStep"]]["after"] for f in page["formulas"]), (name, "final omitted original restrictions")
        if page["kind"] == "coefficients" and states[steps[page["step"]]["before"]]["branches"][0]["equations"][0][1] != "0":
            assert any(f["kind"] == "coefficient_equation" for f in page["formulas"]), (name, "coefficient normalization bridge missing")
    if name == "linear":
        displayed = [states[s]["branches"][b]["equations"][r] for s, b, r in equation_refs]
        assert ["3*x", "15"] in displayed, (mode, "linear intermediate hidden")
        assert ["x", "5"] in displayed
    if name in ("quadratic", "quadratic-complex", "quadratic-negative-b", "quadratic-unfamiliar"):
        assert len(views) == 4 and views[-1]["kind"] == "final", (name, "redundant quadratic conclusion")
        assert not any(v["kind"] == "roots" for v in views), (name, "duplicate evaluated roots page")
    if name in ("isolated", "physical-negative", "isolated-negative"):
        assert len(views) == 1, (name, "artificial isolated-equation operations")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", default="C:/.piobuild/numOS/emulator_pc/program.exe")
    parser.add_argument("--out", type=Path, default=Path("out/tutor-teaching-ux-01/ui"))
    parser.add_argument("--cases", nargs="*")
    parser.add_argument("--render-only", action="store_true", help="Render existing framebuffer evidence; do not rerun or report tests")
    args = parser.parse_args()
    binary = Path(args.bin).resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if args.render_only:
        render_contacts(out, json.loads((out / "results.json").read_text(encoding="utf-8")))
        return
    os.chdir(ROOT)
    env = dict(os.environ, NUMOS_EQUATIONS_BOUNDS="1")
    dll = eq.helper.sdl2_dll_dir(str(binary))
    if dll:
        env["PATH"] = dll + os.pathsep + env.get("PATH", "")

    def path_arg(path):
        try:
            return path.relative_to(ROOT).as_posix()
        except ValueError:
            return path.as_posix()
    mapping = {}
    data = (ROOT / "src/input/generated/ProductionKeypadMap.generated.h").read_text(encoding="utf-8")
    data = data.split("kProductionKeypadMap = {{", 1)[1].split("}};", 1)[0]
    for line in data.splitlines():
        match = re.search(r"\{(\d+), (\d+),.*KeyCode::(\w+),", line)
        if match:
            mapping[match[3]] = (match[1], match[2])

    def physical(*codes):
        return "".join("equations_physical " + " ".join(mapping[c]) + "\n" for c in codes)

    def run(name, script):
        script += "log TEACHING_GUIDED_COMPLETE\n"
        path = out / (name + ".numos")
        path.write_text(script, encoding="utf-8")
        frames = sum(int(line.split()[1]) if line.startswith("wait ") else 1 for line in script.splitlines()) + 100
        proc = subprocess.run([str(binary), "--headless", "--deterministic", "--quiet", "--frames", str(frames),
                               "--script", path_arg(path)], cwd=ROOT, env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        (out / (name + ".log")).write_bytes(proc.stdout)
        if proc.returncode or b"TEACHING_GUIDED_COMPLETE" not in proc.stdout:
            raise RuntimeError((name, proc.returncode, proc.stdout[-3000:].decode(errors="replace")))
        rows = proc.stdout.decode(errors="replace").splitlines()
        return {key: [json.loads(line.split(prefix, 1)[1]) for line in rows if prefix in line]
                for key, prefix in [("views", "[TUTOR_VIEW] "), ("traces", "[TUTOR_TRACE] ")]}

    def shot(name):
        return ("wait 4\nassert_equations view bounded\nassert_equations trace formulas\n"
                "assert_equations view dump\nscreenshot " + path_arg(out / (name + ".ppm")) + "\n")

    records = []
    english_traces = {}
    pan_evidence = None
    for name, equations in CASES.items():
        if args.cases and name not in args.cases:
            continue
        start = (eq.single(equations[0]) if len(equations) == 1 else eq.system(equations)) + eq.keys("tools")
        if name == "physical-negative":
            start = eq.OPEN + physical("EXE", "EXE", "VAR_X", "EQUAL", "NEGATE", "NUM_3", "EXE", "DOWN", "DOWN", "EXE", "TOOLBOX")
        if name in ("complex", "quadratic-complex"):
            start = start.replace("policy real", "policy complex")
        modes = [("guided", "")]
        if name in ("linear", "quadratic", "rational", "system", "linear-unfamiliar"):
            modes.append(("summary", eq.keys("EXE")))
        if name == "linear":
            modes.extend([(locale, "assert_equations locale " + locale + "\n") for locale in ("es", "fr", "pseudo")])
        if name == "quadratic":
            modes.append(("pseudo", "assert_equations locale pseudo\n"))
        for mode, switch in modes:
            label = name + "-" + mode
            begin = start + switch
            found = run(label + "-discover", begin + "assert_equations trace complete\nassert_equations trace check\nassert_equations trace dump\nassert_equations view dump\n")
            count = found["views"][0]["count"]
            trace = found["traces"][0]
            if mode == "guided":
                english_traces[name] = stable(trace)
            else:
                assert stable(trace) == english_traces[name], (name, mode, "language/mode changed derivation")
            script = begin
            for page in range(count):
                script += f"assert_equations view page {page}\nassert_equations view dump\n" + eq.keys("RIGHT")
            metadata = run(label + "-metadata", script)["views"]
            assert len(metadata) == count
            check_presentation(name, mode, trace, metadata)
            script = begin + "assert_equations trace dump\n"
            captures = []
            for page, view in enumerate(metadata):
                script += f"assert_equations view page {page}\n"
                max_scroll = view["maxScroll"]
                names = []
                for scroll in range(math.ceil(max_scroll / 84) + 1):
                    frame = f"{label}-{page:02}-scroll{scroll}"
                    names.append(frame)
                    script += (eq.keys("DOWN DOWN DOWN") if scroll else "") + shot(frame)
                script += eq.keys("DOWN " * 12) + "assert_equations view bounded\nassert_equations view dump\n" + eq.keys("RIGHT")
                captures.append({"page": page, "kind": view["kind"], "metadata": view, "frames": names})
            script += "assert_equations trace dump\n" + eq.keys("EXE") + "assert_equations trace dump\n" + eq.keys("EXE")
            script += "assert_equations trace dump\nassert_equations trace check\nassert_equations trace builds 1\n"
            if mode == "guided":
                # Retain legacy open/close/cache/HOME coverage on every family.
                for _ in range(12):
                    script += eq.keys("BACK tools") + "assert_equations trace builds 1\n"
                script += eq.keys("BACK BACK") + "assert_equations epochs current\n"
                script += eq.keys("HOME") + "wait 30\nassert_equations closed\nopen_app Equations\nwait 30\nassert_equations count 0\nassert_equations trace builds 0\n"
            result = run(label + "-complete", script)
            assert all(stable(t) == stable(result["traces"][0]) for t in result["traces"])
            cursor = 0
            for page in captures:
                views = result["views"][cursor:cursor + len(page["frames"]) + 1]
                assert len(views) == len(page["frames"]) + 1
                assert views[0]["scrollY"] == 0, (label, page["page"], "new page did not start at top")
                assert views[-1]["scrollY"] == views[-1]["maxScroll"], (label, page["page"], "bottom not reached")
                assert all(v["page"] == page["page"] for v in views), (label, "scroll unexpectedly changed page")
                cursor += len(views)
            assert cursor == len(result["views"])
            (out / (label + "-trace.json")).write_text(json.dumps(trace, indent=2), encoding="utf-8")
            (out / (label + "-views.json")).write_text(json.dumps(result["views"], indent=2), encoding="utf-8")
            records.append({"case": name, "mode": mode, "page_count": count, "pages": captures, "pass": True})
            (out / "results.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
            print(label, count, "pages", flush=True)
    from PIL import Image, ImageDraw
    if not args.cases or "wide-pan" in args.cases:
        wide = "( ( 3 1 2 3 4 5 * x - 2 1 2 3 4 5 ) * ( x + 4 1 2 3 4 5 ) ) / ( x + 4 1 2 3 4 5 ) RIGHT = 0"
        script = eq.single(wide) + eq.keys("tools") + "assert_equations trace complete\nassert_equations trace dump\n"
        pan_names = []
        for i in range(33):
            name = f"wide-pan-{i:02}"
            pan_names.append(name)
            script += (eq.keys("VAR") if i else "") + "assert_equations view page 0\n" + shot(name)
        script += "assert_equations trace dump\nassert_equations trace builds 1\n"
        result = run("wide-pan", script)
        assert stable(result["traces"][0]) == stable(result["traces"][1])
        pixels = []
        for name in pan_names:
            frame = Image.open(out / (name + ".ppm"))
            frame.save(out / (name + ".png"))
            # Exclude the clock; inspect the unchanged actual content viewport.
            pixels.append(frame.crop((0, 54, 320, 220)).tobytes())
        assert any(p != pixels[0] for p in pixels[1:]), "VAR did not pan the wide formula"
        restored = next((i for i in range(1, len(pixels)) if pixels[i] == pixels[0]), None)
        assert restored is not None, "VAR cannot recover the starting viewport"
        pan_evidence = {"frames": pan_names, "restored_at": restored, "pass": True}
        (out / "wide-pan-results.json").write_text(json.dumps(pan_evidence, indent=2), encoding="utf-8")
    render_contacts(out, records)
    (out / "manifest.json").write_text(json.dumps({"binary": str(binary),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "sequences": len(records), "pages": sum(r["page_count"] for r in records),
        "frames": sum(len(p["frames"]) for r in records for p in r["pages"]),
        "horizontal_pan": pan_evidence,
        "visual_review_required": "The captured plus-minus glyph ink and complete teaching sequences require visual review; AST assertions alone do not establish readable pixels."
    }, indent=2), encoding="utf-8")

if __name__ == "__main__":
    main()
