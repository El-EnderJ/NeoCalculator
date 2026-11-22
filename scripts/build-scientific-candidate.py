#!/usr/bin/env python3
"""Local-only isolated build; never touches the published emulator or a board."""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess, time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',choices=('native','pool','firmware'),default='native')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
out=root/'out/scientific-quantities-01/E-final/build';out.mkdir(parents=True,exist_ok=True)
build=Path('C:/.piobuild/numOS-scientific-build')/a.profile
published=Path('C:/.piobuild/numOS/emulator_pc').resolve()
assert build.resolve()!=published and build.resolve() not in published.parents
ini=out/(a.profile+'.ini')
text=re.sub(r'^build_dir\s*=.*','build_dir = '+build.as_posix(),(root/'platformio.ini').read_text(encoding='utf8'),flags=re.M)
if a.profile=='pool':text=text.replace('-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB','-DLV_USE_STDLIB_MALLOC=LV_STDLIB_BUILTIN')
ini.write_text(text,encoding='utf8')
env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
env.pop('PLATFORMIO_BUILD_DIR',None)
targets=['numos-esp32-s3-wroom-1u-n16r8','esp32s3_n16r8'] if a.profile=='firmware' else ['emulator_pc']
command=['C:/.platformio/penv/Scripts/platformio.exe','run','-c',str(ini),'--disable-auto-clean','-j','4']
for target in targets:command+=['-e',target]
stamp=time.strftime('%Y%m%d-%H%M%S');log=out/(a.profile+'-'+stamp+'.log')
start=time.perf_counter()
with log.open('wb') as f:r=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT)
record={'command':command,'seconds':time.perf_counter()-start,'exit':r.returncode,'log':str(log),'artifacts':[]}
for target in targets:
 for name in ('program.exe','firmware.bin','firmware.elf'):
  artifact=build/target/name
  if artifact.exists():record['artifacts'].append({'path':str(artifact),'bytes':artifact.stat().st_size,'sha256':hashlib.sha256(artifact.read_bytes()).hexdigest()})
(out/(a.profile+'-'+stamp+'.json')).write_text(json.dumps(record,indent=2),encoding='utf8')
print(json.dumps(record,indent=2));raise SystemExit(r.returncode)
