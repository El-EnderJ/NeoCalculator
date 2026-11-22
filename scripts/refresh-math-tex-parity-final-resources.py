#!/usr/bin/env python3
"""Refresh post-review resource evidence read-only; run outside timing windows."""
import hashlib
import json
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'out/math-tex-parity-02/resources/final-performance'
PRIOR = ROOT / 'out/math-tex-parity-02/resources/aftercritic1'
FIRMWARE = OUT / 'firmware'
FIRMWARE.mkdir(parents=True, exist_ok=True)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
utc = lambda: datetime.now(timezone.utc).isoformat()
native = Path('C:/.piobuild/numOS-parity-02/native/emulator_pc/program.exe')
assert sha(native) == '4987f4ea928aaa138c90ca3aecdb92bd1a6c5d6edbf28046a17633d03b7f3058'
calls = []

def run(label, script, *args):
    path = ROOT / 'scripts' / script
    command = [sys.executable, str(path), *map(str, args)]
    started = utc()
    clock = time.perf_counter()
    result = subprocess.run(command, cwd=ROOT, capture_output=True)
    log = OUT / (label + '.log')
    log.write_bytes(result.stdout + result.stderr)
    calls.append(dict(label=label, command=command, scriptSha256=sha(path),
                      startedAtUtc=started, finishedAtUtc=utc(),
                      elapsedSeconds=time.perf_counter()-clock, exitCode=result.returncode,
                      log=log.name, logSha256=sha(log)))
    (OUT / 'resource-refresh-executions.json').write_text(json.dumps(calls, indent=2), encoding='utf-8')
    assert result.returncode == 0, (label, result.stderr[-2000:])

for board, target in [('wroom', 'numos-esp32-s3-wroom-1u-n16r8'), ('cam', 'esp32s3_n16r8')]:
    build = Path('C:/.piobuild/numOS-parity-02') / board
    old = json.loads((PRIOR / 'firmware' / (target + '.json')).read_text(encoding='utf-8'))
    elf = build / target / 'firmware.elf'
    assert sha(elf) == old['elfSha256']
    assert sha(build / target / 'firmware.bin') == old['imageSha256']
    run(board+'-resources', 'profile-math-tex-parity.py', '--build', build, '--out', FIRMWARE)
    run(board+'-types', 'profile-math-tex-parity-types.py', '--elf', elf,
        '--expected-sha256', old['elfSha256'],
        '--baseline', ROOT / 'out/math-tex-parity-02/resources' / ('baseline-'+board+'-types.json'),
        '--out', FIRMWARE / (board+'-types.json'))
    run(board+'-stack', 'profile-math-tex-parity-stack.py',
        '--resources', FIRMWARE / (target+'.json'), '--out', FIRMWARE / (board+'-stack-model.json'))
run('post-identities', 'compare-math-tex-parity-hotpaths.py',
    '--baseline', ROOT / 'out/math-tex-parity-02/resources/hotpaths-baseline',
    '--candidate', PRIOR / 'hotpaths', '--out', OUT / 'post-verification', '--verify-only')
print('PASS fresh read-only resource/ABI/frame profiles and post-run probe identities')
