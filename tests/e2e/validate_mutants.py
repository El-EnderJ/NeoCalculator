"""Oracle validation with planted bugs (CORRECTNESS-E2E-01 step 6).

Runs the same seeded cases against the unmutated emulator and a mutant, and
reports the findings that appear ONLY with the mutant (attributable), the
invariant that caught them, cases-to-first-detection, and a shrunk
reproducer of the first attributable finding.

    python tests/e2e/validate_mutants.py --surface calculation --seed 7 --count 300 \
        --base C:/.piobuild/e2e-01/emulator_pc/program.exe \
        --mutant m3=C:/.piobuild/e2e-01-mut-m3/emulator_pc/program.exe
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
OUT = HERE.parent.parent / "out" / "correctness-e2e-01" / "mutants"


def keyset(findings):
    return {(f["case"], f["path"], f["invariant"]) for f in findings}


def main():
    from common.runner import Runner
    from common.shrink import shrink_one, write_reproducer
    from common.runner import load_surface
    from common.expr import linear

    ap = argparse.ArgumentParser()
    ap.add_argument("--surface", default="calculation")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--base", required=True)
    ap.add_argument("--mutant", action="append", required=True, help="name=path")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--batch", type=int, default=25)
    ap.add_argument("--i4-every", type=int, default=2)
    a = ap.parse_args()
    batch = 1 if a.surface == "grapher" else a.batch
    base_dir = OUT / f"{a.surface}-base-seed{a.seed}-n{a.count}"
    if (base_dir / "findings.json").exists():
        base_findings = json.loads((base_dir / "findings.json").read_text(encoding="utf-8"))
    else:
        r = Runner(a.surface, a.base, base_dir, jobs=a.jobs, batch=batch, i4_every=a.i4_every)
        _, _, base_findings, _, _ = r.run(a.seed, a.count)
    base_keys = keyset(base_findings)
    table = []
    surface = load_surface(a.surface)
    for spec in a.mutant:
        name, exe = spec.split("=", 1)
        out = OUT / f"{a.surface}-{name}-seed{a.seed}-n{a.count}"
        if (out / "findings.json").exists():  # resume an interrupted validation
            findings = json.loads((out / "findings.json").read_text(encoding="utf-8"))
            cases = [surface.generate(a.seed, i) for i in range(a.count)]
        else:
            r = Runner(a.surface, exe, out, jobs=a.jobs, batch=batch, i4_every=a.i4_every)
            summary, stats, findings, cases, oracles = r.run(a.seed, a.count)
        attributable = [f for f in findings if (f["case"], f["path"], f["invariant"]) not in base_keys]
        by_inv = {}
        for f in attributable:
            by_inv[f["invariant"]] = by_inv.get(f["invariant"], 0) + 1
        index_of = {c["id"]: c["index"] for c in cases}
        first = min(attributable, key=lambda f: (index_of[f["case"]], f["path"])) if attributable else None
        row = {"mutant": name, "emulator": exe, "cases": a.count, "attributable_findings": len(attributable),
               "cases_with_attributable": len({f["case"] for f in attributable}),
               "by_invariant": by_inv, "first_case_index": index_of[first["case"]] if first else None,
               "cases_to_first_detection": (index_of[first["case"]] - 0 + 1) if first else None,
               "first": first}
        if first:
            case = next(c for c in cases if c["id"] == first["case"])
            res = shrink_one(surface, exe, case, first, out)
            if res.get("status") == "shrunk":
                d, rec = write_reproducer(surface, exe, res, first, out)
                row["shrunk"] = linear(res["case"]["tree"])
                row["shrunk_keys"] = rec["keys"]
                row["reproducer"] = str(d)
                # the shrunk reproducer must NOT fail on the unmutated build
                from common.shrink import _still_fails
                ok_base, *_ = _still_fails(surface, a.base, res["case"], res["paths"], res["do_i4"],
                                           first["invariant"], first["path"], str(out / "tmp"))
                row["shrunk_fails_on_base"] = ok_base
            else:
                row["shrunk"] = res
        table.append(row)
        print(json.dumps({k: v for k, v in row.items() if k != "first"}, indent=1, ensure_ascii=False), flush=True)
        (out / "validation_row.json").write_text(json.dumps(row, indent=1, ensure_ascii=False, default=str),
                                                 encoding="utf-8")
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{a.surface}-validation-seed{a.seed}-n{a.count}.json").write_text(
        json.dumps({"base_findings": len(base_findings), "rows": table}, indent=1, ensure_ascii=False, default=str),
        encoding="utf-8")


if __name__ == "__main__":
    main()
