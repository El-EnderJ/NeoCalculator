"""Re-check HANG cases: standalone repro, run twice at 10 s and once at 60 s.

py -3.12 tests/e2e/grapher/w3_hang.py g10-8735 g10-9431
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common.emulator import run_script  # noqa: E402
from grapher import surface as G  # noqa: E402
from grapher.gcore import EMU  # noqa: E402

OUT = Path(__file__).resolve().parents[3] / "out" / "correctness-e2e-01" / "grapher" / "findings" / "W3-F00-hang"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rep = {}
    for cid in sys.argv[1:]:
        seed, idx = cid[1:].split("-")
        case = G.generate(int(seed), int(idx))
        lines, meta = G.case_lines(case, None)
        script = G.header() + lines
        (OUT / f"{cid}.numos").write_text("\n".join(script) + "\n", encoding="utf-8", newline="\n")
        runs = []
        for t in (10, 10, 60):
            r = run_script(script, EMU, timeout=t)
            d = r.by_tag.get(f"{cid}|s0") or {}
            runs.append({"timeout": t, "rc": r.returncode, "seconds": round(r.seconds, 2), "frames": r.frames,
                         "dumps": sorted(k for k in r.by_tag if k), "valid": d.get("valid"),
                         "reason": d.get("reason"), "text_len": len(d.get("text") or "")})
        rep[cid] = {"keys": meta["slots"][0]["keys"], "n_keys": len(meta["slots"][0]["keys"].split()), "runs": runs}
        print(cid, json.dumps(runs))
    (OUT / "hang.json").write_text(json.dumps(rep, indent=1, ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    main()
