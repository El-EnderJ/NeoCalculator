"""Deduplicate Calculus findings by suspected root cause, shrink one
representative per class on the emulator, write standalone reproducers and
re-run each reproducer twice in fresh processes (W4 task 5).

    python tests/e2e/calculus/findings.py RUN_DIR [RUN_DIR ...] [--out DIR] [--max-rounds 10]

Class key = the checker's `klass` (see check.py) — CRASH split by exit code.
Representative = the smallest generated tree among the confirmed cases of the
class; shrinking = greedy one-step reductions (common.shrink.reductions,
which works on any dataclass node incl. DefInt/DiffCall) kept only while the
SAME class still reproduces on the SAME keystroke path in a fresh process.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

from common.emulator import DEFAULT_EMULATOR, run_script  # noqa: E402
from common.shrink import _rows_ok, reductions  # noqa: E402
from common.expr import Row  # noqa: E402
from calculus import model as M  # noqa: E402
from calculus import surface as S  # noqa: E402

OUT = HERE.parent.parent / "out" / "correctness-e2e-01" / "calculus"
SEV_ORDER = {"S0": 0, "S1": 1, "S2": 2, "S3": 3, "S4": 4}
NO_SHRINK = {"HANG"}


def class_key(f):
    if f["invariant"] == "CRASH":
        m = re.search(r"exit code (0x[0-9a-f]+)", f["title"])
        return "crash-" + (m.group(1) if m else "?")
    if f["invariant"] == "HANG":
        return "HANG"
    return f["detail"].get("klass") or f["invariant"]


def case_of(cid):
    m = re.match(r"c(\d+)-(\d+)$", cid)
    return S.generate(int(m.group(1)), int(m.group(2)))


def typeable(tree):
    return all(n.items and _rows_ok(n) for n in M.walk(tree) if isinstance(n, Row))


def probe(case, path, key, emulator, workdir):
    """Run one case on one path (fresh process). Returns (reproduces, run, meta, rec, findings)."""
    paths = list(S.PATHS) if path == "-" else [path]
    lines = list(S.header())
    l, meta = S.case_lines_for(case, None, paths)
    lines += l + S.third_voice(case)
    r = run_script(lines, emulator, workdir=workdir, timeout=15 if key == "HANG" else 30)
    if key.startswith("crash-"):
        return r.returncode not in (0, -999, 4), r, meta, None, []
    if key == "HANG":
        return r.returncode == -999, r, meta, None, []
    rec, fnd, des = S.analyze(case, r.by_tag, meta)
    ok = any(class_key(f.to_dict()) == key and (path == "-" or f.path == path) for f in fnd)
    return ok, r, meta, rec, fnd


def shrink(case, path, key, emulator, workdir, max_rounds=10, log=print, jobs=4):
    cur = case
    steps = 0
    for _ in range(max_rounds):
        seen, cands = set(), []
        for t in reductions(cur["tree"]):
            if not isinstance(t, Row) or not t.items or not typeable(t):
                continue
            k = repr(t)
            if k in seen or M.size(t) >= M.size(cur["tree"]):
                continue
            seen.add(k)
            cands.append(t)
        cands.sort(key=M.size)
        cands = cands[:16]
        if not cands:
            break
        trials = [dict(cur, tree=t, id=f"{case['id']}s{steps}x{i}") for i, t in enumerate(cands)]
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            res = list(ex.map(lambda c: probe(c, path, key, emulator, workdir)[0], trials))
        win = next((t for t, ok in zip(trials, res) if ok), None)
        if win is None:
            break
        cur = win
        steps += 1
        log(f"   shrink step {steps}: {M.linear(cur['tree'])}")
    return cur, steps


def write_case(fid, key, rep, case, path, finding, emulator, out_root):
    d = out_root / "findings" / fid
    d.mkdir(parents=True, exist_ok=True)
    paths = list(S.PATHS) if path == "-" else [path]
    lines = list(S.header())
    l, meta = S.case_lines_for(case, None, paths)
    lines += l + S.third_voice(case)
    oracle = S.oracle_of(case)
    head = [f"# {fid} ({key}): {finding['invariant']}/{finding['severity']} on path {path}",
            f"# {finding['title']}",
            f"# intended: {M.linear(case['tree'])}   op={case['op']}  angle={case['angle']}",
            f"# oracle f(x): {oracle.text()}",
            "# run: program.exe --headless --deterministic --quiet --frames 2000 --script repro.numos "
            "--fs-sandbox-dir <empty dir>"]
    (d / "repro.numos").write_text("\n".join(head + lines) + "\n", encoding="utf-8", newline="\n")
    reruns = []
    for k in range(2):
        r = run_script(lines, emulator, workdir=str(out_root / "tmp"), timeout=15 if key == "HANG" else 30)
        if key.startswith("crash-") or key == "HANG":
            reruns.append({"returncode": r.returncode, "reproduced": probe_rc(key, r.returncode)})
            continue
        rec, fnd, des = S.analyze(case, r.by_tag, meta)
        same = [f.to_dict() for f in fnd if class_key(f.to_dict()) == key]
        reruns.append({"returncode": r.returncode, "reproduced": bool(same),
                       "dump": r.by_tag.get(f"{case['id']}|{paths[0]}"),
                       "third_voice": r.by_tag.get(f"{case['id']}|giac"),
                       "triage": S.triage(case, r.by_tag, rec) if rec else None,
                       "finding": same[0] if same else None})
    first = reruns[0]
    dump = first.get("dump") or {}
    fd = (first.get("finding") or finding)["detail"]
    record = {
        "id": fid, "class": key, "invariant": finding["invariant"], "severity": finding["severity"],
        "title": finding["title"], "path": path, "op": case["op"], "angle": case["angle"],
        "tree": M.linear(case["tree"]), "tree_json": M.to_json(case["tree"]),
        "keys": meta.get(paths[0], {}).get("keys"),
        "displayed_input_ast": dump.get("input"), "giac_bound": dump.get("giac_in"),
        "numos_status": dump.get("status"), "numos_exact": dump.get("exact"),
        "numos_display": dump.get("display"), "numos_variable": dump.get("variable"),
        "numos_unevaluated": dump.get("unevaluated"),
        "oracle_input_f": oracle.text(),
        "oracle_method": ("SymPy over the displayed input (real x), derivative by sp.diff / integral checked by "
                          "differentiating NumOS's antiderivative; numeric comparison with mpmath at 40 digits at "
                          "x in {-23/10,-7/11,3/8,5/7,17/10,29/10}; constant definite integrals by SymPy "
                          "exact + mpmath quad cross-check"),
        "oracle_target": fd.get("target") or fd.get("oracle_target"),
        "comparison": (fd.get("triage") or {}).get("points_user") or fd.get("points"),
        "triage_readings": {k: v for k, v in (fd.get("triage") or {}).items() if k != "points_user"},
        "third_voice": first.get("third_voice"), "third_voice_triage": first.get("triage"),
        "reruns_fresh_process": [{"returncode": x["returncode"], "reproduced": x["reproduced"]} for x in reruns],
        "representative_of": rep,
    }
    (d / "finding.json").write_text(json.dumps(record, indent=1, ensure_ascii=False, default=str),
                                    encoding="utf-8", newline="\n")
    return record


def probe_rc(key, rc):
    return rc == -999 if key == "HANG" else rc not in (0, -999, 4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("runs", nargs="+")
    ap.add_argument("--out", default=str(OUT))
    ap.add_argument("--emulator", default=DEFAULT_EMULATOR)
    ap.add_argument("--max-rounds", type=int, default=10)
    ap.add_argument("--only", default=None, help="comma-separated class keys")
    ap.add_argument("--jobs", type=int, default=4)
    a = ap.parse_args()
    out_root = Path(a.out)
    allf = []
    for rd in a.runs:
        fs = json.loads((Path(rd) / "findings.json").read_text(encoding="utf-8"))
        for f in fs:
            f["_run"] = str(rd)
        allf += fs
    classes = {}
    for f in allf:
        classes.setdefault(class_key(f), []).append(f)
    order = sorted(classes, key=lambda k: (min(SEV_ORDER.get(f["severity"], 9) for f in classes[k]), k))
    summary = []
    workdir = str(out_root / "tmp")
    for n, key in enumerate(order, 1):
        fs = classes[key]
        if a.only and key not in a.only.split(","):
            continue
        fid = f"W4-F{n:02d}"
        cases = sorted({f["case"] for f in fs})
        paths = sorted({f["path"] for f in fs})
        # representative: smallest tree, prefer the direct path
        cand = sorted(fs, key=lambda f: (M.size(case_of(f["case"])["tree"]), f["path"] != "direct", f["case"]))
        f0 = cand[0]
        case, path = case_of(f0["case"]), f0["path"]
        rep_id = f0["case"]
        print(f"[{fid}] {key}: {len(fs)} findings, {len(cases)} cases, paths {paths}; rep {f0['case']} {path} "
              f"{M.linear(case['tree'])}")
        steps = 0
        if key not in NO_SHRINK:
            ok = probe(case, path, key, a.emulator, workdir)[0]
            if ok:
                case, steps = shrink(case, path, key, a.emulator, workdir, a.max_rounds, jobs=a.jobs)
            else:
                print("   representative did not reproduce alone; trying the next ones")
                for f1 in cand[1:6]:
                    c1 = case_of(f1["case"])
                    if probe(c1, f1["path"], key, a.emulator, workdir)[0]:
                        case, path, rep_id, f0 = c1, f1["path"], f1["case"], f1
                        case, steps = shrink(case, path, key, a.emulator, workdir, a.max_rounds, jobs=a.jobs)
                        break
        rec = write_case(fid, key, rep_id, case, path, f0, a.emulator, out_root)
        sev = sorted({f["severity"] for f in fs}, key=lambda s: SEV_ORDER.get(s, 9))
        summary.append({"id": fid, "class": key, "invariant": f0["invariant"], "severities": sev,
                        "findings": len(fs), "cases": len(cases), "paths": paths,
                        "families": sorted({case_of(c)["family"] for c in cases}),
                        "ops": sorted({case_of(c)["op"] for c in cases}),
                        "angles": sorted({case_of(c)["angle"] for c in cases}),
                        "example_cases": cases[:8], "shrink_steps": steps,
                        "minimal_tree": rec["tree"], "op": rec["op"], "angle": rec["angle"],
                        "keys": rec["keys"], "numos_exact": rec["numos_exact"],
                        "oracle_target": rec["oracle_target"],
                        "reruns": rec["reruns_fresh_process"],
                        "repro": str(out_root / "findings" / fid / "repro.numos")})
        print(f"   -> {rec['tree']}  reruns {rec['reruns_fresh_process']}")
    (out_root / "findings-summary.json").write_text(json.dumps(summary, indent=1, ensure_ascii=False),
                                                   encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
