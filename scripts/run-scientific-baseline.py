#!/usr/bin/env python3
"""Preserve executed baseline evidence, explicitly identifying reused executables."""
import argparse, concurrent.futures, hashlib, json, os, subprocess, time
from pathlib import Path

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--source', type=Path, required=True)
ap.add_argument('--evidence-root', type=Path, required=True)
ap.add_argument('--bin', type=Path, required=True)
ap.add_argument('--corpus-bin-dir', type=Path, required=True)
ap.add_argument('--corpus', type=Path, required=True)
ap.add_argument('--observer', type=Path, help='Matching private observer required for viewport assertions')
ap.add_argument('--phase', choices=('baseline','final'), default='baseline')
a = ap.parse_args()
a.evidence_root.mkdir(parents=True, exist_ok=True)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
env = dict(os.environ, PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;' + os.environ['PATH'], PYTHONIOENCODING='utf-8')
jobs = []
for name in ('calculation-input', 'calculation-quantities', 'temperature-product', 'unit-picker', 'calculation-brackets', 'toolbox'):
    jobs.append((name, ['python', str(a.source / 'scripts' / ('test-' + name + '.py')), '--bin', str(a.bin), '--out', str(a.evidence_root / name)]))
jobs.append(('corpus900', ['python', str(a.source / 'scripts/replay-unit-baseline.py'), '--baseline', str(a.corpus), '--bin-dir', str(a.corpus_bin_dir), '--out', str(a.evidence_root / 'corpus900')]))
if a.observer:
    jobs.append(('viewport', ['python', str(a.source / 'scripts/test-calculation-viewport.py'), '--bin', str(a.observer), '--out', str(a.evidence_root / 'viewport')]))
identity = {'scope': 'baseline execution of preserved executables; this is not a rebuilt final candidate', 'binary': {'path': str(a.bin), 'sha256': sha(a.bin)}, 'corpusBinaries': {p.name: sha(p) for p in a.corpus_bin_dir.glob('*.exe')}, 'scripts': {cmd[1]: sha(Path(cmd[1])) for _, cmd in jobs}}
if a.phase=='final':identity['scope']='final regression execution against supplied rebuilt candidate and host corpus binaries'
if a.observer:
    identity['observer'] = dict(path=str(a.observer), sha256=sha(a.observer))
(a.evidence_root / 'identity.json').write_text(json.dumps(identity, indent=2), encoding='utf8')
def run(job):
    name, cmd = job
    start = time.perf_counter()
    with (a.evidence_root / (name + '.log')).open('wb') as log:
        process = subprocess.run(cmd, cwd=a.source, env=env, stdout=log, stderr=subprocess.STDOUT)
    result = dict(name=name, command=cmd, exit=process.returncode, passed=process.returncode == 0, seconds=time.perf_counter() - start)
    print(name, process.returncode, flush=True)
    return result
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    results = list(pool.map(run, jobs))
(a.evidence_root / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf8')
raise SystemExit(not all(r['passed'] for r in results))
