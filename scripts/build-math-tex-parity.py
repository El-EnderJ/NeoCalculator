#!/usr/bin/env python3
"""Build isolated parity candidates, without publication or hardware access."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--profile', choices=('native', 'pool', 'wroom', 'cam'), required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = Path('C:/.piobuild/numOS-parity-02') / args.profile
published = Path('C:/.piobuild/numOS/emulator_pc').resolve()
assert build.resolve() != published and build.resolve() not in published.parents
out = root / 'out/math-tex-parity-02/build' / args.profile
out.mkdir(parents=True, exist_ok=True)
config = re.sub(r'^build_dir\s*=.*', 'build_dir = ' + build.as_posix(),
                (root / 'platformio.ini').read_text(encoding='utf8'), flags=re.M)
if args.profile == 'pool':
    config = config.replace('-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB',
                            '-DLV_USE_STDLIB_MALLOC=LV_STDLIB_BUILTIN')
ini = out / 'candidate.ini'
ini.write_text(config, encoding='utf8')
target = {'wroom': 'numos-esp32-s3-wroom-1u-n16r8', 'cam': 'esp32s3_n16r8'}.get(args.profile, 'emulator_pc')
env = dict(os.environ)
env.pop('PLATFORMIO_BUILD_DIR', None)
env['PATH'] = 'C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;' + env.get('PATH', '')
command = ['C:/.platformio/penv/Scripts/platformio.exe', 'run', '-c', str(ini),
           '--disable-auto-clean', '-j', '4', '-e', target]
stamp = time.strftime('%Y%m%d-%H%M%S')
log = out / (stamp + '.log')
start = time.perf_counter()
with log.open('wb') as stream:
    result = subprocess.run(command, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT)
artifacts = []
for name in ('program.exe', 'firmware.bin', 'firmware.elf'):
    path = build / target / name
    if path.exists():
        artifacts.append(dict(path=str(path), bytes=path.stat().st_size,
                              sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
record = dict(command=command, exit=result.returncode, seconds=time.perf_counter()-start,
              artifacts=artifacts, log=str(log))
(out / (stamp + '.json')).write_text(json.dumps(record, indent=2), encoding='utf8')
print(json.dumps(record, indent=2))
raise SystemExit(result.returncode)
