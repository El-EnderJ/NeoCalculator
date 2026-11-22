#!/usr/bin/env python3
"""Generate matching native compilation commands and restore tracked database byte for byte."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--ini',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True);database=a.source/'compile_commands.json';before=database.read_bytes() if database.exists() else None
if before is not None:(a.out/'compile_commands-before.json').write_bytes(before)
env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ['PATH']);env.pop('PLATFORMIO_BUILD_DIR',None)
cmd=['C:/.platformio/penv/Scripts/platformio.exe','run','-c',str(a.ini.resolve()),'-e','emulator_pc','-t','compiledb','--disable-auto-clean']
try:
 with (a.out/'compiledb.log').open('wb') as f:r=subprocess.run(cmd,cwd=a.source,env=env,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,r.returncode
 (a.out/'native-compile-commands.json').write_bytes(database.read_bytes())
finally:
 if before is not None:database.write_bytes(before)
 elif database.exists():database.unlink()
assert before is None or database.read_bytes()==before
(a.out/'identity.json').write_text(json.dumps({'command':cmd,'restoredByteExact':True,'beforeSha256':hashlib.sha256(before).hexdigest() if before else None,'databaseSha256':hashlib.sha256((a.out/'native-compile-commands.json').read_bytes()).hexdigest()},indent=2))
print('PASS matching compilation database saved; original restored byte-exact')
