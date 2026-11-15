"""W3 Table and Trace check (pixels -> glyph OCR -> oracle).

Neither the Table cells nor the Trace readout have a dump hook, so they are
read from the screenshot: text bands are binarised and split into glyphs at
blank columns; glyph templates are LEARNED from calibration screens whose
strings are known in advance (constants y=K, table x column, trace at the
centre x=0). A case is then decoded glyph by glyph (nearest template) and:
  Table: decoded f1(x) cells (x=-5..-2) vs the oracle at x (to the shown 6
         significant digits, "--" iff the oracle is invalid) and vs the
         `%.6g` of the sampled float (rendering);
  Trace: after k RIGHT presses the readout "x=X y=Y" must satisfy
         Y = f(x') for some |x'-X| <= 5e-4 (both shown with 3 decimals), or
         "y=---" iff the oracle is invalid at X.
One emulator process per case (no state reuse), single job.

py -3.12 tests/e2e/grapher/w3_tabletrace.py --seed 10 --count 520 [--calib-only]
"""
from __future__ import annotations

import argparse
import json
import math
import random
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import keys as K  # noqa: E402
from common.emulator import run_script, script_header  # noqa: E402
from grapher import surface as G  # noqa: E402
from grapher.gcore import EMU, point_reals  # noqa: E402

OUT = Path(__file__).resolve().parents[3] / "out" / "correctness-e2e-01" / "grapher" / "tabletrace"
TABLE_BANDS = [(87, 98), (126, 137), (165, 176), (204, 215)]   # rows x=-5..-2
TABLE_XS = [-5, -4, -3, -2]
COL_F1 = (163, 306)
COL_X = (2, 150)
TRACE_BAND = (222, 240)
TRACE_COLS = (0, 250)
CALIB = ["1 2 3 4 5 6", "1 7 . 1 3 4", "2 1 . 4 7 1", "1 1 . 7 4 1", "neg 4 7 . 1 1 1", "7 7 . 4 4 1", "neg 7 . 8 9", "0 . 0 3 4 5", "9 8 7 6 5 . 4", "1 . 5 * 1 0 ^ 1 2 right",
         "neg 2 . 0 6 1 / 3", "4 0 . 1 2 8", "neg 0 . 9 7 5", "3 1 . 6 / 7", "neg 5 / 9 0 0 0 0 0 0 right",
         "8 . 2 4 6", "neg 6 1 . 7 3 9"]


def glyphs(img, band, cols):
    """Binarised glyph bitmaps of one text band, left to right."""
    a = np.asarray(img.convert("L"))[band[0]:band[1] + 1, cols[0]:cols[1]] < 128
    colhas = a.any(axis=0)
    out, start = [], None
    for c, h in enumerate(list(colhas) + [False]):
        if h and start is None:
            start = c
        elif not h and start is not None:
            g = a[:, start:c]
            rows = np.where(g.any(axis=1))[0]
            out.append((start, g[rows[0]:rows[-1] + 1], rows[0]))
            start = None
    return out


def key_of(g):
    bm, top = g[1], g[2]
    return (bm.shape, top, bm.tobytes())


class Templates:
    def __init__(self):
        self.t = defaultdict(Counter)

    def learn(self, gl, text):
        chars = [c for c in text if c != " "]
        if len(chars) != len(gl):
            return False
        for g, ch in zip(gl, chars):
            self.t[key_of(g)][ch] += 1
        return True

    def decode(self, gl, gaps=True):
        out, prev_end = "", None
        for g in gl:
            k = key_of(g)
            if k in self.t:
                ch = self.t[k].most_common(1)[0][0]
            else:
                ch = self.nearest(g)
                if ch == "?":
                    ch = self.split(g)
            if gaps and prev_end is not None and g[0] - prev_end > 6:
                out += " "
            out += ch
            prev_end = g[0] + g[1].shape[1]
        return out

    def split(self, g, depth=0):
        """Touching glyphs (kerning): try every cut into known pieces."""
        bm, top = g[1], g[2]
        if depth > 2 or bm.shape[1] < 4:
            return "?"
        for cut in range(2, bm.shape[1] - 1):
            parts = []
            for sub in (bm[:, :cut], bm[:, cut:]):
                rows = np.where(sub.any(axis=1))[0]
                cols = np.where(sub.any(axis=0))[0]
                if not len(rows):
                    break
                parts.append((0, sub[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1], top + rows[0]))
            if len(parts) != 2:
                continue
            a = self.t.get(key_of(parts[0]))
            if a is None:
                continue
            b = self.t.get(key_of(parts[1]))
            if b is not None:
                return a.most_common(1)[0][0] + b.most_common(1)[0][0]
            rest = self.split(parts[1], depth + 1)
            if "?" not in rest:
                return a.most_common(1)[0][0] + rest
        return "?"

    def nearest(self, g):
        best, bd = "?", 10 ** 9
        for (shape, top, raw), cnt in self.t.items():
            if abs(shape[1] - g[1].shape[1]) > 1 or abs(shape[0] - g[1].shape[0]) > 1:
                continue
            t = np.frombuffer(raw, dtype=bool).reshape(shape)
            h, w = min(shape[0], g[1].shape[0]), min(shape[1], g[1].shape[1])
            d = int((t[:h, :w] != g[1][:h, :w]).sum()) + abs(shape[0] - g[1].shape[0]) * w + \
                abs(shape[1] - g[1].shape[1]) * h + abs(top - g[2]) * 4
            if d < bd:
                best, bd = cnt.most_common(1)[0][0], d
        return best if bd <= 6 else "?"


def script(keys, rights, shot_t, shot_g, angle="rad"):
    lines = script_header("Grapher") + [f"set_angle_mode {angle}", "key down", "key enter"]
    lines += [K.to_lines([t])[0] for t in keys]
    lines += ["key enter", "wait 3", "dump_graph tt 0 " + " ".join(map(str, TABLE_XS)),
              "key table", "wait 20", f"screenshot {shot_t}", "key graph", "wait 30"]
    lines += ["key right"] * rights + ["wait 15", f"screenshot {shot_g}"]
    return lines


def run_case(keys, rights, angle, tmp):
    st, sg = f"{tmp}/t.ppm", f"{tmp}/g.ppm"
    r = run_script(script(keys, rights, st, sg, angle), EMU, timeout=10)
    try:
        return r, Image.open(st).copy(), Image.open(sg).copy()
    except OSError:
        return r, None, None


def calibrate(tpl, tmp):
    n_ok = 0
    for i, c in enumerate(CALIB):
        keys = [("key", k) for k in ("y = " + c).split()]
        r, it, ig = run_case(keys, 0, "rad", tmp)
        d = r.by_tag.get("tt") or {}
        vals = d.get("values") or []
        if it is None or not vals:
            continue
        for (band, x), (_, v) in zip(zip(TABLE_BANDS, TABLE_XS), vals):
            n_ok += tpl.learn(glyphs(it, band, COL_X), str(x))
            n_ok += tpl.learn(glyphs(it, band, COL_F1), "--" if v is None else "%.6g" % v)
        v0 = vals[0][1]
        if v0 is not None:
            n_ok += tpl.learn(glyphs(ig, TRACE_BAND, TRACE_COLS), "Trace: x=0.000 y=%.3f" % v0)
    return n_ok


def parse_trace(s):
    s = s.replace(" ", "").split("(")[0]
    if "x=" not in s or "y=" not in s:
        return None
    try:
        xs = s.split("x=")[1].split("y=")[0]
        ys = s.split("y=")[1]
        return float(xs), (None if ys.startswith("---") else float(ys.split("(")[0]))
    except ValueError:
        return None


def check_case(case, tpl, tmp):
    s = case["slots"][0]
    toks, _ = G.slot_tokens(s, "direct", random.Random(0))
    rights = random.Random(case["id"]).randint(0, 40)
    r, it, ig = run_case(toks, rights, case["angle"], tmp)
    d = r.by_tag.get("tt") or {}
    rec = {"id": case["id"], "keys": K.to_compact(toks), "angle": case["angle"], "valid": d.get("valid"),
           "rc": r.returncode, "table": [], "trace": None, "problems": []}
    if it is None or not d.get("valid"):
        rec["skip"] = "no screenshot" if it is None else "slot invalid"
        return rec
    R = G.oracle_of(case)[0]["rhs"]
    for (band, x), (_, v) in zip(zip(TABLE_BANDS, TABLE_XS), d.get("values") or []):
        shown = tpl.decode(glyphs(it, band, COL_F1), gaps=False)
        expect_fmt = "--" if v is None else "%.6g" % v
        orc = point_reals(R, case["angle"], G.f32(x))
        cell = {"x": x, "shown": shown, "sample_fmt": expect_fmt, "oracle": orc[:2]}
        rec["table"].append(cell)
        if "?" in shown:
            cell["unreadable"] = True
            continue
        if shown == "-0" and expect_fmt in ("0", "-0"):
            rec.setdefault("cosmetic", []).append(f"table x={x}: negative zero shown as '-0'")
            continue
        if shown != expect_fmt:
            rec["problems"].append(f"table render x={x}: shown {shown} != %.6g(sample) {expect_fmt}")
        if not orc:
            continue
        if shown == "--":
            if all(c is not None for c in orc) and not any(abs(c) > 3e38 for c in orc):
                rec["problems"].append(f"table x={x}: '--' but oracle {orc[:1]}")
        else:
            val = float(shown)
            digits = shown.split("e")[0].replace("-", "").replace(".", "").lstrip("0")
            exp10 = math.floor(math.log10(abs(val))) if val else 0
            ulp = 10 ** (exp10 - max(len(digits), 1) + 1)
            if not any(c is not None and abs(c - val) <= ulp for c in orc):
                rec["problems"].append(f"table x={x}: shown {shown} vs oracle {orc[:1]}")
    txt = tpl.decode(glyphs(ig, TRACE_BAND, TRACE_COLS))
    rec["trace"] = {"rights": rights, "readout": txt}
    p = parse_trace(txt)
    num_part = txt.replace(" ", "").split("x=", 1)[-1].split("(")[0]
    if p is None or "?" in num_part:
        rec["trace"]["unreadable"] = True
        return rec
    X, Yv = p
    lo, hi = point_reals(R, case["angle"], G.f32(X - 5e-4)), point_reals(R, case["angle"], G.f32(X + 5e-4))
    mid = point_reals(R, case["angle"], G.f32(X))
    cands = [c for c in lo + hi + mid if c is not None]
    if Yv is None:
        if mid and all(c is not None for c in mid) and lo and hi and all(c is not None for c in lo + hi):
            rec["problems"].append(f"trace x={X}: y=--- but oracle {mid[:1]}")
    elif cands:
        a, b = min(cands), max(cands)
        if not (a - 6e-4 - 1e-6 * abs(a) <= Yv <= b + 6e-4 + 1e-6 * abs(b)):
            rec["problems"].append(f"trace x={X}: y={Yv} outside oracle [{a}, {b}]")
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=10)
    ap.add_argument("--count", type=int, default=520)
    ap.add_argument("--calib-only", action="store_true")
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--skip", type=int, default=0, help="skip the first N bulk cases (resume)")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="w3tt_", dir=str(OUT))
    tpl = Templates()
    n = calibrate(tpl, tmp)
    print("calibration glyph strings learned:", n, "templates:", len(tpl.t),
          "ambiguous:", sum(1 for c in tpl.t.values() if len(c) > 1))
    if a.calib_only:
        return
    cases, i = [], 0
    while len(cases) < a.count:
        c = G.generate(a.seed, i)
        i += 1
        if c["family"] == "bulk":
            cases.append(c)
    cases = cases[a.skip:]
    from concurrent.futures import ThreadPoolExecutor

    from common.timeout import Overrun, call

    def one(c):
        try:
            return call(check_case, 45, c, tpl, tempfile.mkdtemp(prefix="c_", dir=tmp))
        except Overrun:
            return {"id": c["id"], "skip": "oracle timeout (45 s)", "table": [], "trace": None, "problems": []}

    recs = []
    with open(OUT / f"tabletrace-seed{a.seed}-skip{a.skip}.jsonl", "w", encoding="utf-8", newline="\n") as fh, \
            ThreadPoolExecutor(a.jobs) as ex:
        for rec in ex.map(one, cases):
            recs.append(rec)
            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
            fh.flush()
    judged = [r for r in recs if "skip" not in r]
    cells = [c for r in judged for c in r["table"]]
    summ = {"cases": len(recs), "judged": len(judged), "table_cells": len(cells),
            "table_unreadable": sum(1 for c in cells if c.get("unreadable")),
            "trace_readouts": sum(1 for r in judged if r["trace"]),
            "trace_unreadable": sum(1 for r in judged if r["trace"] and r["trace"].get("unreadable")),
            "cases_with_problems": sum(1 for r in judged if r["problems"]),
            "problems": [p for r in judged for p in r["problems"]][:40]}
    (OUT / f"summary-seed{a.seed}-skip{a.skip}.json").write_text(json.dumps(summ, indent=1), encoding="utf-8")
    print(json.dumps({k: v for k, v in summ.items() if k != "problems"}))


if __name__ == "__main__":
    main()
