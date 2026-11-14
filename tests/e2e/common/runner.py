"""Batch runner: many cases per emulator process, N processes in parallel,
single-process confirmation of every flagged case, third-voice triage and
deterministic output files.

A surface module must provide:
    generate(seed, index) -> case dict
    oracle_of(case) -> Value
    header() -> list[str]                     boot + open app (+ preamble)
    check_header(by_tag) -> list[str]          problems in the preamble dumps
    case_lines(case, oracle, do_i4) -> (lines, meta)
    analyze(case, by_tag, meta, oracle) -> (record, findings, desync)
    third_voice(case) -> list[str]             extra lines (giac_eval) or []
    triage(case, by_tag, record) -> dict       uses the third-voice dumps
"""
from __future__ import annotations

import json
import os
import time
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from pathlib import Path

from .emulator import run_script


ORACLE_SECONDS = 20
ANALYZE_SECONDS = 25


def _oracle_job(args):
    from .evaluator import Value
    from .timeout import Overrun, call
    surface_name, case = args
    surf = load_surface(surface_name)
    try:
        return surf.oracle_lite(call(surf.oracle_of, ORACLE_SECONDS, case))
    except Overrun:
        return surf.oracle_lite(Value("unsupported", detail=f"oracle timeout ({ORACLE_SECONDS}s)"))


def load_surface(name):
    import importlib
    return importlib.import_module(f"{name}.surface")


def _analyze_job(args):
    surface_name, case, by_tag, meta, oracle = args
    surf = load_surface(surface_name)
    from .timeout import Overrun, call
    try:
        rec, findings, desync = call(surf.analyze, ANALYZE_SECONDS, case, by_tag, meta, oracle)
    except Overrun:
        return ({"id": case["id"], "analysis_timeout": ANALYZE_SECONDS, "findings": []}, [], [])
    except Exception:  # noqa: BLE001 - a harness bug must not hide other cases
        import traceback
        return ({"id": case["id"], "harness_error": traceback.format_exc()[-1500:]}, [],
                ["harness error"])
    return rec, [f.to_dict() for f in findings], desync


class Runner:
    def __init__(self, surface_name, emulator, out_dir, jobs=4, batch=40, i4_every=2,
                 workdir=None, log=print):
        self.name = surface_name
        self.surf = load_surface(surface_name)
        self.emulator = emulator
        self.out = Path(out_dir)
        self.jobs = jobs
        self.batch = batch
        self.i4_every = i4_every
        self.workdir = workdir or str(self.out / "tmp")
        self.log = log
        self.stats = {"emulator_seconds": 0.0, "processes": 0, "frames": 0}
        self.single_timeout = 20

    # ── emulator ───────────────────────────────────────────────────────
    def _run_lines(self, lines, timeout):
        r = run_script(lines, self.emulator, workdir=self.workdir, timeout=timeout)
        self.stats["processes"] += 1
        self.stats["emulator_seconds"] += r.seconds
        self.stats["frames"] += r.frames
        return r

    def _script(self, cases, oracles, extra=None):
        lines = list(self.surf.header())
        metas = {}
        for c in cases:
            do_i4 = self.i4_every > 0 and c["index"] % self.i4_every == 0
            l, m = self.surf.case_lines(c, oracles[c["id"]], do_i4)
            lines += l
            metas[c["id"]] = m
            if extra:
                lines += extra(c)
        return lines, metas

    def run_cases(self, cases, oracles, extra=None, parallel=True):
        """Run `cases` in batches; returns {case_id: (by_tag, meta, run_info)}."""
        chunks = [cases[i:i + self.batch] for i in range(0, len(cases), self.batch)]
        out = {}

        def one(chunk):
            lines, metas = self._script(chunk, oracles, extra)
            # Budget: normal cases take well under a second each; Giac can
            # spin for minutes on some exact expressions (observed), so a
            # process gets a bounded wall-clock budget and is killed after.
            timeout = self.single_timeout if len(chunk) == 1 else 20 + 1.5 * len(chunk)
            r = self._run_lines(lines, timeout)
            info = {"rc": r.returncode, "seconds": r.seconds, "frames": r.frames,
                    "header_problems": self.surf.check_header(r.by_tag)}
            if r.returncode != 0:
                info["rc"] = r.returncode & 0xFFFFFFFF if r.returncode > 0 else r.returncode
                info["stderr_tail"] = r.stderr[-400:]
            return chunk, metas, r.by_tag, info

        workers = self.jobs if parallel else 1
        with ThreadPoolExecutor(max_workers=workers) as ex:
            for chunk, metas, by_tag, info in ex.map(one, chunks):
                for c in chunk:
                    out[c["id"]] = (by_tag, metas[c["id"]], info)
        return out

    # ── pipeline ───────────────────────────────────────────────────────
    def oracles(self, cases):
        with ProcessPoolExecutor(max_workers=max(1, os.cpu_count() - 1)) as ex:
            vals = list(ex.map(_oracle_job, [(self.name, c) for c in cases], chunksize=8))
        return {c["id"]: v for c, v in zip(cases, vals)}

    def analyze_all(self, cases, runs, oracles):
        jobs = [(self.name, c, runs[c["id"]][0], runs[c["id"]][1], oracles[c["id"]]) for c in cases]
        with ProcessPoolExecutor(max_workers=max(1, os.cpu_count() - 1)) as ex:
            res = list(ex.map(_analyze_job, jobs, chunksize=4))
        return {c["id"]: r for c, r in zip(cases, res)}

    def run(self, seed, count, start=0, confirm=True, parallel=True):
        t0 = time.perf_counter()
        self.log(f"[runner] generating {count} cases")
        cases = [self.surf.generate(seed, i) for i in range(start, start + count)]
        t_gen = time.perf_counter()
        self.log(f"[runner] oracles ({t_gen - t0:.1f}s)")
        oracles = self.oracles(cases)
        t_or = time.perf_counter()
        self.log(f"[runner] emulator batches ({time.perf_counter() - t0:.1f}s)")
        runs = self.run_cases(cases, oracles, parallel=parallel)
        t_emu = time.perf_counter()
        self.log(f"[runner] analysis ({t_emu - t0:.1f}s)")
        analyzed = self.analyze_all(cases, runs, oracles)
        t_an = time.perf_counter()
        header_problems = sorted({p for r in runs.values() for p in r[2]["header_problems"]})
        flagged = [c for c in cases if analyzed[c["id"]][1] or analyzed[c["id"]][2]]
        confirmations = {}
        if confirm and flagged:
            self.log(f"[runner] confirming {len(flagged)} flagged case(s) one process per case")
            saved = self.batch
            self.batch = 1
            reruns = self.run_cases(flagged, oracles, extra=self.surf.third_voice, parallel=parallel)
            self.batch = saved
            re_an = self.analyze_all(flagged, reruns, oracles)
            for c in flagged:
                rec, fnd, des = re_an[c["id"]]
                rinfo = reruns[c["id"]][2]
                if rinfo["rc"] not in (0, -999, 4):
                    from .invariants import Finding
                    fnd = fnd + [Finding(c["id"], "-", "CRASH", "S0",
                                         f"emulator process crashed (exit code {rinfo['rc']:#x}) on this case "
                                         "(single case, fresh process)",
                                         {"last_dump": sorted(reruns[c["id"]][0])[-1:] if reruns[c["id"]][0] else [],
                                          "tree": rec.get("tree"), "stderr_tail": rinfo.get("stderr_tail", "")}).to_dict()]
                if rinfo["rc"] == -999:
                    from .invariants import Finding
                    fnd = fnd + [Finding(c["id"], "-", "HANG", "S0",
                                         f"evaluation did not finish: emulator killed after {self.single_timeout}s "
                                         "(single case, fresh process)",
                                         {"last_dump": sorted(reruns[c["id"]][0])[-1:] if reruns[c["id"]][0] else [],
                                          "tree": rec.get("tree")}).to_dict()]
                tri = {}
                if fnd:
                    from .timeout import Overrun, call
                    try:
                        tri = call(self.surf.triage, ANALYZE_SECONDS, c, reruns[c["id"]][0], rec)
                    except Overrun:
                        tri = {"third_voice": "triage timeout"}
                batch_f = {(f["path"], f["invariant"]) for f in analyzed[c["id"]][1]}
                single_f = {(f["path"], f["invariant"]) for f in fnd}
                confirmations[c["id"]] = {
                    "batch_findings": sorted(batch_f), "single_findings": sorted(single_f),
                    "batch_desync": analyzed[c["id"]][2], "single_desync": des,
                    "status": ("confirmed" if fnd else ("desync_recovered" if analyzed[c["id"]][2]
                                                        else "batch_artifact")),
                    "triage": tri}
                for f in fnd:
                    f["triage"] = tri
                analyzed[c["id"]] = (rec, fnd, des)
        t_conf = time.perf_counter()
        self.log(f"[runner] done ({t_conf - t0:.1f}s)")
        records = []
        findings = []
        for c in cases:
            rec, fnd, des = analyzed[c["id"]]
            rec = dict(rec)
            rec["findings"] = [f"{f['invariant']}/{f['severity']}/{f['path']}" for f in fnd]
            if des:
                rec["desync"] = des
            if c["id"] in confirmations:
                rec["confirmation"] = confirmations[c["id"]]["status"]
            records.append(rec)
            findings += fnd
        self.out.mkdir(parents=True, exist_ok=True)
        with open(self.out / "results.jsonl", "w", encoding="utf-8", newline="\n") as fh:
            for r in records:
                fh.write(json.dumps(r, sort_keys=True, ensure_ascii=False) + "\n")
        with open(self.out / "findings.json", "w", encoding="utf-8", newline="\n") as fh:
            json.dump(findings, fh, sort_keys=True, ensure_ascii=False, indent=1)
        summary = {"surface": self.name, "seed": seed, "start": start, "count": count,
                   "cases_with_findings": sum(1 for r in records if r["findings"]),
                   "findings_by_invariant": _count(findings, "invariant"),
                   "findings_by_severity": _count(findings, "severity"),
                   "families": _count(records, "family"),
                   "confirmations": _count(list(confirmations.values()), "status"),
                   "header_problems": header_problems,
                   "unresolved_desync": sum(1 for r in records if r.get("desync"))}
        with open(self.out / "summary.json", "w", encoding="utf-8", newline="\n") as fh:
            json.dump(summary, fh, sort_keys=True, indent=1)
        stats = dict(self.stats, cases=count, jobs=self.jobs if parallel else 1, batch=self.batch,
                     t_generate=t_gen - t0, t_oracle=t_or - t_gen, t_emulator_wall=t_emu - t_or,
                     t_analyze=t_an - t_emu, t_confirm=t_conf - t_an, t_total=t_conf - t0)
        stats["cases_per_second_total"] = count / stats["t_total"]
        stats["cases_per_second_emulator_wall"] = count / max(stats["t_emulator_wall"], 1e-9)
        with open(self.out / "stats.json", "w", encoding="utf-8", newline="\n") as fh:
            json.dump(stats, fh, sort_keys=True, indent=1)
        return summary, stats, findings, cases, oracles


def _count(items, key):
    out = {}
    for it in items:
        k = it.get(key) if isinstance(it, dict) else getattr(it, key)
        out[str(k)] = out.get(str(k), 0) + 1
    return dict(sorted(out.items()))
