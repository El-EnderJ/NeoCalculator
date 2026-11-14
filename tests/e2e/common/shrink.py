"""Greedy shrinker: reduce a failing case to a minimal keystroke reproducer.

Each round enumerates one-step reductions of the tree (drop a term, drop a
factor, drop a unary minus, replace an atom by `2`/`1`, hoist a slot's
content, shorten a literal), keeps only keypad-typeable trees, re-runs them
on the emulator, and keeps the smallest one that still fails with the same
invariant on the same path. The result is written as a standalone `.numos`
script plus a JSON record (oracle value, dumpTree, Giac-bound text).
"""
from __future__ import annotations

import dataclasses
import json
from pathlib import Path

from .emulator import run_script
from .expr import (Abs, Const, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row, Sqrt, Var, Empty,
                   is_atom, linear, size, to_json, walk)
from .gen import can_follow


def _rows_ok(r: Row) -> bool:
    items = r.items
    for i, e in enumerate(items):
        if isinstance(e, Pow) and e.sci:
            if i == 0 or not isinstance(items[i - 1], Num):
                return False
        if i and is_atom(e) and is_atom(items[i - 1]) and not can_follow(items[i - 1], e):
            return False
        if isinstance(e, Frac) and not e.capture and i and is_atom(items[i - 1]):
            return False
        if isinstance(e, Frac) and e.capture and not (len(e.num.items) == 1 and is_atom(e.num.items[0])):
            return False
        if isinstance(e, Pow) and not e.sci and isinstance(e.base, Pow) and e.base.sci:
            return False
    return True


def typeable(x) -> bool:
    return all(n.items and _rows_ok(n) for n in walk(x) if isinstance(n, Row))


def _terms(items):
    """Split a row at top-level binary +/- into (op, [elements]) chunks."""
    out, cur, op = [], [], None
    expect = True
    for e in items:
        if isinstance(e, Op) and not expect and e.kind in "+-":
            out.append((op, cur))
            cur, op, expect = [], e, True
            continue
        cur.append(e)
        expect = isinstance(e, Op)
    out.append((op, cur))
    return out


def _join(chunks):
    items = []
    for i, (op, els) in enumerate(chunks):
        if i:
            items.append(op if op is not None else Op("+"))
        items += els
    return tuple(items)


def row_reductions(r: Row):
    items = r.items
    chunks = _terms(items)
    if len(chunks) > 1:
        for i in range(len(chunks)):
            rest = chunks[:i] + chunks[i + 1:]
            rest = [(None if j == 0 else op, els) for j, (op, els) in enumerate(rest)]
            yield Row(_join(rest))
            yield Row(tuple(chunks[i][1]))
    for i, e in enumerate(items):
        if isinstance(e, Op):
            if e.kind == "-" and (i == 0 or isinstance(items[i - 1], Op)):
                yield Row(items[:i] + items[i + 1:])
            if e.kind in "*/" and i + 1 < len(items):
                yield Row(items[:i] + items[i + 2:])
                yield Row(items[:i - 1] + items[i + 1:]) if i >= 1 else None
        elif i + 1 < len(items) and is_atom(items[i + 1]) and is_atom(e):
            yield Row(items[:i] + items[i + 1:])
            yield Row(items[:i + 1] + items[i + 2:])


def atom_reductions(a):
    if isinstance(a, Empty):
        return
    if not isinstance(a, Num) or a.text not in ("1", "2"):
        yield Num("2")
        yield Num("1")
    if isinstance(a, Num):
        t = a.text
        if "." in t:
            yield Num(t.split(".")[0] or "0")
        if len(t) > 1:
            yield Num(t[0] if t[0] != "." else "1")
            yield Num(t[:len(t) // 2] if t[:len(t) // 2].strip(".") else "1")
    for f in getattr(a, "__dataclass_fields__", {}):
        v = getattr(a, f)
        if isinstance(v, Row):
            if len(v.items) == 1 and is_atom(v.items[0]):
                yield v.items[0]
            elif v.items:
                yield Paren(v)
        elif is_atom(v) and not isinstance(v, (str, bool)) and dataclasses.is_dataclass(v):
            yield v


def reductions(x):
    """All one-step reductions of node `x` (Row or atom)."""
    if isinstance(x, Row):
        for r in row_reductions(x):
            if r is not None and r.items:
                yield r
        for i, e in enumerate(x.items):
            if isinstance(e, Op):
                continue
            for e2 in reductions(e):
                yield Row(x.items[:i] + (e2,) + x.items[i + 1:])
        return
    yield from atom_reductions(x)
    for f in getattr(x, "__dataclass_fields__", {}):
        v = getattr(x, f)
        if isinstance(v, Row) or (dataclasses.is_dataclass(v) and is_atom(v)):
            for v2 in reductions(v):
                if isinstance(v, Row) and not isinstance(v2, Row):
                    continue
                yield dataclasses.replace(x, **{f: v2})


def _fix_capture(x):
    """Re-validate Frac.capture after a reduction."""
    if isinstance(x, Row):
        return Row(tuple(_fix_capture(e) for e in x.items))
    if isinstance(x, Op) or not dataclasses.is_dataclass(x):
        return x
    kw = {}
    for f in x.__dataclass_fields__:
        v = getattr(x, f)
        if isinstance(v, Row) or (dataclasses.is_dataclass(v) and is_atom(v)):
            kw[f] = _fix_capture(v)
    y = dataclasses.replace(x, **kw) if kw else x
    if isinstance(y, Frac) and y.capture and not (len(y.num.items) == 1 and is_atom(y.num.items[0])):
        y = dataclasses.replace(y, capture=False)
    return y


def candidates(tree: Row):
    seen = set()
    out = []
    for t in reductions(tree):
        t = _fix_capture(t)
        if not isinstance(t, Row) or not typeable(t):
            continue
        k = repr(t)
        if k in seen or size(t) >= size(tree):
            continue
        seen.add(k)
        out.append(t)
    out.sort(key=size)
    return out


def _still_fails(surface, emulator, case, paths, do_i4, inv, fpath, workdir):
    oracle = surface.oracle_of(case)
    lines = list(surface.header())
    l, meta = surface.case_lines_for(case, oracle, paths, do_i4)
    lines += l
    r = run_script(lines, emulator, workdir=workdir, timeout=45)
    rc = r.returncode
    if inv == "CRASH":
        return rc not in (0, -999, 4) and rc != 0, r, meta, oracle
    if inv == "HANG":
        return rc == -999, r, meta, oracle
    rec, fnd, des = surface.analyze(case, r.by_tag, meta, oracle)
    ok = any(f.invariant == inv and f.path == fpath for f in fnd)
    return ok, r, meta, oracle


def shrink_one(surface, emulator, case, finding, out_dir, max_rounds=30, jobs=4):
    from concurrent.futures import ThreadPoolExecutor
    inv, fpath = finding["invariant"], finding["path"]
    paths = [fpath] if fpath in surface.PATHS else ["direct"]
    do_i4 = fpath not in surface.PATHS and fpath != "-"
    if fpath == "-":
        paths = ["direct"]
    workdir = str(Path(out_dir) / "tmp")
    cur = dict(case)
    ok, r, meta, oracle = _still_fails(surface, emulator, cur, paths, do_i4, inv, fpath, workdir)
    if not ok and inv in ("CRASH", "HANG"):
        paths = list(surface.PATHS)  # crash may come from another path's keys
        ok, r, meta, oracle = _still_fails(surface, emulator, cur, paths, do_i4, inv, fpath, workdir)
    if not ok:
        return {"status": "not reproduced with restricted paths", "case": case["id"]}
    steps = 0
    for _ in range(max_rounds):
        cands = candidates(cur["tree"])[:24]
        if not cands:
            break
        trials = [dict(cur, tree=t, id=f"{case['id']}~{steps}.{i}") for i, t in enumerate(cands)]
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            res = list(ex.map(lambda c: _still_fails(surface, emulator, c, paths, do_i4, inv, fpath, workdir), trials))
        win = next((t for t, rr in zip(trials, res) if rr[0]), None)
        if win is None:
            break
        cur = win
        steps += 1
    return {"status": "shrunk", "steps": steps, "case": cur, "paths": paths, "do_i4": do_i4}


def write_reproducer(surface, emulator, result, finding, out_root):
    case = result["case"]
    paths, do_i4 = result["paths"], result["do_i4"]
    oracle = surface.oracle_of(case)
    lines = list(surface.header())
    l, meta = surface.case_lines_for(case, oracle, paths, do_i4, tree_dump=True)
    lines += l
    lines += surface.third_voice(case)
    name = f"{finding['case']}-{finding['invariant']}-{finding['path']}".replace("|", "_").replace("~", "_")
    d = Path(out_root) / "findings" / name
    d.mkdir(parents=True, exist_ok=True)
    (d / "repro.numos").write_text(
        f"# Minimal reproducer for {finding['invariant']}/{finding['severity']} on {finding['path']}\n"
        f"# {finding['title']}\n# intended: {linear(case['tree'])}\n# oracle: {oracle.text()}\n"
        f"# run: program.exe --headless --deterministic --quiet --frames 4000 --script repro.numos --fs-sandbox-dir <empty dir>\n"
        + "\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    r = run_script(lines, emulator, workdir=str(Path(out_root) / "tmp"), timeout=45)
    rec, fnd, des = ({}, [], [])
    if finding["invariant"] not in ("CRASH", "HANG"):
        rec, fnd, des = surface.analyze(case, r.by_tag, meta, oracle)
    dumps = {k: v for k, v in r.by_tag.items() if k and not str(k).startswith("pre|")}
    record = {"finding": finding, "shrunk_tree": linear(case["tree"]), "shrunk_tree_json": to_json(case["tree"]),
              "keys": {p: meta.get(p, {}).get("keys") for p in paths}, "oracle": oracle.text(),
              "oracle_kind": oracle.kind, "oracle_notes": oracle.notes, "returncode": r.returncode,
              "dumps": dumps, "reproduced_findings": [f.to_dict() for f in fnd], "steps": result.get("steps")}
    (d / "finding.json").write_text(json.dumps(record, indent=1, ensure_ascii=False, sort_keys=True),
                                    encoding="utf-8", newline="\n")
    return d, record


def signature(f):
    return (f["invariant"], f["severity"], f["title"].split(":")[0][:60],
            f["path"] if f["path"] in ("naive",) else "*")


def shrink_findings(surface_name, emulator, out_dir, findings, cases, oracles, limit=10, log=print):
    from .runner import load_surface
    surface = load_surface(surface_name)
    by_id = {c["id"]: c for c in cases}
    seen, done = set(), []
    for f in sorted(findings, key=lambda f: (f["severity"], f["case"], f["path"])):
        sig = signature(f)
        if sig in seen or f["case"] not in by_id:
            continue
        seen.add(sig)
        log(f"[shrink] {f['case']} {f['invariant']}/{f['severity']} {f['path']}: {f['title']}")
        res = shrink_one(surface, emulator, by_id[f["case"]], f, out_dir)
        if res.get("status") != "shrunk":
            log(f"[shrink]   {res}")
            done.append({"finding": f, "result": res})
            continue
        d, rec = write_reproducer(surface, emulator, res, f, out_dir)
        log(f"[shrink]   -> {linear(res['case']['tree'])}  ({res['steps']} steps) {d}")
        done.append({"finding": f, "dir": str(d), "shrunk": linear(res["case"]["tree"])})
        if len(done) >= limit:
            break
    Path(out_dir, "shrunk.json").write_text(json.dumps(done, indent=1, ensure_ascii=False, default=str),
                                            encoding="utf-8", newline="\n")
    return done
