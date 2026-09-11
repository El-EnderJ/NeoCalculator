#!/usr/bin/env python3
"""Independent, read-only inspection of production tutor traces and wording."""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "out/tutor-engine-01/review-teaching"
ACCEPTANCE = [
    [x] for x in ["x=1", "1=x", "3*x+5=20", "3*(2*x-1)=9", "2*x+3=x-4",
    "(x-1)/2+(x+1)/3=5", "0*x=0", "0*x=1", "x^2=x", "x^2=9", "x^2-2=0",
    "x^2-5*x+6=0", "(x-1)^2=0", "2*x^2+3*x-4=0", "x^2+1=0",
    "(x^2-1)/(x-1)=0", "x/x=1"]
] + [["x+y=3", "x-y=1"], ["x+y=2", "2*x+2*y=4"],
     ["x+y=2", "2*x+2*y=5"], ["x+y+z=6", "x-y+z=2", "x+y-z=0"]]
# Deliberately written by the reviewer after the production plan existed.
CHALLENGE = [
    ["-3*x-5=10"], ["5-x=2*x-1"], ["x+2=3*x+8"], ["-2*x+1=-5*x-8"],
    ["(2*x-3)*(3*x+4)=0"], ["(x-7)^2=0"], ["(2*x+1)^2=9"],
    ["x^2=2*x^2-9"], ["3*x^2+1=x^2+9"], ["x^2=0"],
    ["x^2+1=0", "--complex"], ["2*x^2+3*x+4=0", "--complex"],
    ["2*y^2+3*y-4=0", "--variables=y"], ["x/(x-1)=1/(x-1)"],
    ["(x-2)/(x-2)=0"], ["(x+1)/(x-2)=1"], ["x^2=4/9"],
    ["y=2", "x+y=5"], ["2*x+y=4", "3*x-y=1"],
    ["2*x+2*y=4", "x+y=2"], ["x+y=1", "-x-y=-2"],
    ["x+y+z=1", "2*x+2*y+2*z=2", "3*x+3*y+3*z=3"],
]

def stable(trace):
    # Wording and elapsed time may vary by presentation; mathematics must not.
    trace = json.loads(json.dumps(trace))
    trace.pop("micros", None)
    for step in trace["steps"]:
        step.pop("text", None)
    return trace

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--compile-boundary", action="store_true",
                    help="independently compile current boundary; reuse the fixed cached Giac vendor object")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / "tutor-teaching.exe"
    if not args.skip_build:
        obj = OUT / "tutor-teaching-main.o"
        subprocess.run(["g++", "-std=gnu++17", "-O1", "-DNUMOS_GIAC_HOST_HARNESS=1",
                        "-Isrc", "-c", "tests/host/tutor-teaching-main.cpp", "-o", str(obj.relative_to(ROOT))], cwd=ROOT, check=True)
        objects = (ROOT / "out/tutor-engine-01/host/link.rsp").read_text().splitlines()
        objects = [p for p in objects if "tutor_engine_main.cpp.o" not in p and "out/tutor-engine-01/host/GiacEngine.cpp.o" not in p and "kgen.cc.o" not in p]
        boundary = OUT / "reviewed-GiacEngine.o"
        if args.compile_boundary:
            watched = ["src/math/giac/GiacEngine.cpp", "src/math/giac/GiacEngine.h", "src/math/giac/GiacTutor.inc",
                       "src/math/tutor/Derivation.h", "src/math/tutor/Messages.inc", "src/math/tutor/TraceAllocator.h"]
            def hashes():
                return {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in watched}
            before = hashes()
            definitions = ["-D" + d for d in "HAVE_CONFIG_H IN_GIAC GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED USE_GMP_REPLACEMENTS UMAP DOUBLEVAL".split()]
            with (OUT / "independent-build.log").open("w", encoding="utf-8") as log:
                subprocess.run(["g++", "-std=gnu++17", "-O1", "-ffunction-sections", "-fdata-sections",
                                "-D_USE_MATH_DEFINES", "-DNUMOS_GIAC_HOST_HARNESS=1", "-Isrc", *definitions,
                                "-Ilib/giac", "-Ilib/giac/src", "-Ilib/libtommath", "-D__MINGW_H", "-fpermissive",
                                "-Wno-deprecated-declarations", "-c", "src/math/giac/GiacEngine.cpp", "-o",
                                str(boundary.relative_to(ROOT))], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
            if hashes() != before:
                raise RuntimeError("Boundary source changed during independent compilation; rerun after edits settle")
        else:
            shutil.copy2(ROOT / "out/tutor-engine-01/host/GiacEngine.cpp.o", boundary)
        objects.append('"' + boundary.relative_to(ROOT).as_posix() + '"')
        vendor = OUT / "reviewed-kgen.o"
        shutil.copy2(ROOT / "out/tutor-engine-01/host/kgen.cc.o", vendor)
        objects.append('"' + vendor.relative_to(ROOT).as_posix() + '"')
        objects.append('"' + obj.relative_to(ROOT).as_posix() + '"')
        rsp = OUT / "link.rsp"
        rsp.write_text("\n".join(objects), encoding="utf-8")
        subprocess.run(["g++", "@" + str(rsp.relative_to(ROOT)), "-Wl,--gc-sections", "-static", "-lpsapi", "-o", str(exe.relative_to(ROOT))], cwd=ROOT, check=True)
        sources = ["src/math/tutor/Derivation.h", "src/math/tutor/TraceAllocator.h", "src/math/tutor/Messages.inc", "src/math/giac/GiacTutor.inc",
                   "lib/giac/src/kgen.cc", "out/tutor-engine-01/review-teaching/reviewed-GiacEngine.o",
                   "out/tutor-engine-01/review-teaching/reviewed-kgen.o", "out/tutor-engine-01/review-teaching/tutor-teaching.exe"]
        build_metadata = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in sources}
        (OUT / "linked-snapshot.json").write_text(json.dumps(build_metadata, indent=2), encoding="utf-8")
    results = []
    failures = []
    human = []
    for corpus, cases in [("acceptance", ACCEPTANCE), ("challenge", CHALLENGE)]:
        for number, case in enumerate(cases):
            try:
                proc = subprocess.run([str(exe), *case], cwd=ROOT, capture_output=True, text=True, timeout=20)
            except subprocess.TimeoutExpired as exc:
                phase = exc.stderr or ""
                if isinstance(phase, bytes):
                    phase = phase.decode("utf-8", errors="replace")
                failures.append({"case": case, "issue": "whole-process timeout (20 seconds)", "phases": phase})
                continue
            if proc.returncode:
                failures.append({"case": case, "execution": proc.returncode, "stderr": proc.stderr})
                continue
            data = json.loads(proc.stdout)
            english = data["locales"][0]
            item = {"corpus": corpus, "case": case, **data}
            results.append(item)
            if not data["catalog_valid"]:
                failures.append({"case": case, "issue": "catalog schema"})
            if any(stable(locale) != stable(english) for locale in data["locales"]):
                failures.append({"case": case, "issue": "localized mathematics differ"})
            if english["status"] != 2:
                failures.append({"case": case, "issue": "incomplete", "status": english["status"], "diagnostic": english["diagnostic"]})
            if english["status"] == 2 and data["replay_verdict"] != 1:
                failures.append({"case": case, "issue": "complete trace did not independently replay"})
            for mutation in data["wording_mutations"]:
                if mutation["verdict"] != 2:
                    failures.append({"case": case, "issue": "wording mutation not rejected", "mutation": mutation})
            for step in english["steps"]:
                words = step["text"]
                if words.startswith(("Add -", "Subtract -")):
                    failures.append({"case": case, "issue": "negative magnitude in balancing wording", "text": words})
                if step["rule"] == "square.branches" and english["complex"] and "Positive and negative" in words:
                    failures.append({"case": case, "issue": "ordered terminology for complex roots", "text": words})
                if step["rule"] in {"equation.add", "system.add"} and step["operand"] == "0":
                    failures.append({"case": case, "issue": "spurious addition of zero"})
                if step["rule"] in {"equation.divide", "system.scale", "rational.clear"} and step["operand"] == "1":
                    failures.append({"case": case, "issue": "spurious unit scaling"})
                if step["relation"] == 0 and english["states"][step["before"]]["fingerprint"] == english["states"][step["after"]]["fingerprint"]:
                    failures.append({"case": case, "issue": "identical equivalent transformation"})
            if case == ["x=1"] and [s["rule"] for s in english["steps"]] != ["terminal.isolated"]:
                failures.append({"case": case, "issue": "already-solved equation padded with operations"})
            if case == ["x^2=9"] and "square.branches" not in [s["rule"] for s in english["steps"]]:
                failures.append({"case": case, "issue": "pure-square method missing"})
            if case == ["5-x=2*x-1"] and "algebra.expand" in [s["rule"] for s in english["steps"]]:
                failures.append({"case": case, "issue": "ordering-only change described as expansion"})
            if case == ["(2*x-3)*(3*x+4)=0"]:
                split = next((s for s in english["steps"] if s["rule"] == "product.zero"), None)
                if not split or all(b["equations"][0][0] == "x" for b in english["states"][split["after"]]["branches"]):
                    failures.append({"case": case, "issue": "factor equations skipped in detail"})
                if sum(s["rule"] == "equation.divide" for s in english["steps"]) != 2:
                    failures.append({"case": case, "issue": "factor scaling steps missing"})
            consumed = []
            for i, step in enumerate(english["steps"]):
                substeps = step.get("substeps", [])
                if substeps:
                    if step["relation"] != 0 or substeps != list(range(i + 1, i + 1 + len(substeps))) or len(substeps) > 2:
                        failures.append({"case": case, "issue": "invalid compact grouping", "step": i})
                    for child in substeps:
                        if child >= len(english["steps"]) or english["steps"][child]["relation"] != 0 or english["steps"][child].get("substeps") or english["steps"][child].get("branch") != step.get("branch"):
                            failures.append({"case": case, "issue": "compact grouping hides non-equivalent step", "step": i})
                    consumed.extend(substeps)
            if len(consumed) != len(set(consumed)):
                failures.append({"case": case, "issue": "overlapping compact groups"})
            if english["status"] == 2:
                repeated = subprocess.run([str(exe), *case], cwd=ROOT, capture_output=True, text=True, timeout=20)
                if repeated.returncode or stable(json.loads(repeated.stdout)["locales"][0]) != stable(english):
                    failures.append({"case": case, "issue": "nondeterministic replay"})
            human.append("\n" + corpus + " " + str(number) + ": " + "; ".join(case))
            human.append("status=" + str(english["status"]) + " replay=" + str(data["replay_verdict"]))
            for step in english["steps"]:
                state = english["states"][step["after"]]
                equations = " OR ".join("; ".join("=".join(eq) for eq in b["equations"]) + (" [REJECTED]" if b["rejected"] else "") for b in state["branches"])
                human.append("  " + step["text"] + " => " + equations + " conditions=" + repr(state["conditions"]))
    metadata = {"linked_snapshot_sha256": json.loads((OUT / "linked-snapshot.json").read_text()),
                "cases": len(results), "wording_mutations": sum(len(r["wording_mutations"]) for r in results),
                "assertion_failures": failures}
    (OUT / "traces.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    (OUT / "traces.txt").write_text("\n".join(human), encoding="utf-8")
    (OUT / "summary.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(json.dumps(metadata, indent=2))
    raise SystemExit(1 if failures else 0)

if __name__ == "__main__":
    main()
