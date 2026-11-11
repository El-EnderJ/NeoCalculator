#!/usr/bin/env python3
"""Replay existing product gates on an explicitly supplied, immutable executable.

No builds, source edits, golden promotion, publication or hardware access.
Evidence names distinguish baseline from final; final requires rebuilt host and
read-only observer executables to claim the full invariance gate.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--phase', choices=('baseline', 'final'), required=True)
    p.add_argument('--observer', type=Path)
    p.add_argument('--corpus-bin-dir', type=Path)
    p.add_argument('--corpus', type=Path, default=ROOT / 'out/unit-catalog-01/baseline-corpus')
    p.add_argument('--only', nargs='*')
    p.add_argument('--workers', type=int, default=3)
    a = p.parse_args()
    a.bin = a.bin.resolve()
    # Legacy screenshot grammar is space-delimited. Paths inside this workspace
    # must stay relative even though the caller may supply an absolute --out.
    a.out = Path(os.path.relpath(a.out.resolve(), ROOT))
    a.out.mkdir(parents=True, exist_ok=True)
    sha = lambda x: hashlib.sha256(x.read_bytes()).hexdigest()
    env = dict(os.environ, PYTHONIOENCODING='utf-8', PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;' + os.environ.get('PATH', ''))
    jobs = []
    for name in ('calculation-input', 'calculation-quantities', 'temperature-product',
                 'unit-picker', 'calculation-brackets', 'toolbox', 'scientific-product',
                 'scientific-visuals', 'quantity-viewport', 'calculus-rebuild',
                 'tutor-i18n-ui', 'math-spacing-apps'):
        script='math-tex-parity-product-toolbox.py' if name=='toolbox' else 'test-'+name+'.py'
        jobs.append((name, [sys.executable, str(ROOT/'scripts'/script),
                           '--bin', str(a.bin), '--out', str(a.out/name)]))
    jobs.append(('tutor-composition-ui', [sys.executable, str(ROOT/'scripts/test-tutor-composition-ui.py'),
                                       '--bin', str(a.bin), '--out', str(a.out/'tutor-composition-ui'),
                                       '--bilingual']))
    if a.observer:
        for name in ('calculation-viewport', 'calculation-format', 'calculation-format-presentation'):
            script='math-tex-parity-product-viewport.py' if name=='calculation-viewport' else 'test-'+name+'.py'
            jobs.append((name, [sys.executable, str(ROOT/'scripts'/script),
                               '--bin', str(a.observer.resolve()), '--out', str(a.out/name)]))
    if a.corpus_bin_dir:
        jobs.append(('corpus900', [sys.executable, str(ROOT/'scripts/replay-unit-baseline.py'),
                                  '--baseline', str(a.corpus.resolve()), '--bin-dir', str(a.corpus_bin_dir.resolve()),
                                  '--out', str(a.out/'corpus900')]))
    if a.only:
        unknown = set(a.only) - {n for n, _ in jobs}
        if unknown:
            p.error('unavailable suite names: ' + ', '.join(sorted(unknown)))
        jobs = [(n, c) for n, c in jobs if n in a.only]
    identity = {'phase': a.phase, 'binary': {'path': str(a.bin), 'sha256': sha(a.bin)},
                'scripts': {n: sha(Path(c[1])) for n, c in jobs},
                'scope': 'executed existing gates; no counts inferred from historical reports',
                'missingFullGateInputs': []}
    if a.observer:
        identity['observer'] = {'path': str(a.observer.resolve()), 'sha256': sha(a.observer)}
    else:
        identity['missingFullGateInputs'].append('matching read-only FORMAT/viewport observer')
    if a.corpus_bin_dir:
        identity['hostBinaries'] = {n: sha(a.corpus_bin_dir/n) for n in ('tutor_engine_main.exe', 'periodic_result_main.exe')}
    else:
        identity['missingFullGateInputs'].append('matching rebuilt 900-trace host binaries')
    (a.out/'identity.json').write_text(json.dumps(identity, indent=2), encoding='utf8')

    def run(job):
        name, command = job
        start = time.perf_counter()
        with (a.out/(name+'.log')).open('wb') as log:
            try:
                r = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
                code = r.returncode
            except subprocess.TimeoutExpired:
                code = -999
        record = dict(name=name, command=command, exit=code, passed=code == 0, seconds=time.perf_counter()-start)
        print(name, 'PASS' if record['passed'] else 'FAIL', flush=True)
        return record

    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
        results = list(pool.map(run, jobs))
    unchanged = sha(a.bin) == identity['binary']['sha256']
    summary = {'phase': a.phase, 'binaryUnchanged': unchanged, 'runs': results,
               'allRequestedPassed': unchanged and all(r['passed'] for r in results),
               'fullGateInputsSupplied': not a.only and not identity['missingFullGateInputs'],
               'fullGateClaim': unchanged and all(r['passed'] for r in results) and not a.only and not identity['missingFullGateInputs']}
    (a.out/'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf8')
    return 0 if summary['allRequestedPassed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
