#!/usr/bin/env python3
"""Standalone bounded sensitivity/variance benchmark; excludes CAS/render/LVGL."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,default=Path('out/scientific-quantities-01/C-kernel/performance'))
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=(root/a.out).resolve();out.mkdir(parents=True,exist_ok=True)
build=Path('C:/.piobuild/scientific-kernel');build.mkdir(parents=True,exist_ok=True)
exe=build/'scientific-profile.exe'
sources=['tests/host/scientific_kernel_profile.cpp','src/math/ScientificUncertainty.cpp','src/math/scientific/PhysicalConstants.cpp']
cmd=['C:/mingw64/bin/g++.exe','-std=c++17','-O2','-Isrc',*sources,'-o',str(exe)]
compile=subprocess.run(cmd,cwd=root,text=True,capture_output=True)
(out/'compile.log').write_text(compile.stdout+compile.stderr,encoding='utf8');compile.check_returncode()
env=dict(os.environ,PATH='C:/mingw64/bin;'+os.environ.get('PATH',''))
r=subprocess.run([str(exe)],cwd=root,env=env,text=True,capture_output=True)
(out/'raw.jsonl').write_text(r.stdout,encoding='utf8');(out/'stderr.log').write_text(r.stderr,encoding='utf8');r.check_returncode()
records=[json.loads(line) for line in r.stdout.splitlines()]
result=dict(scope='MinGW native -O2 host only, one run after300 warmups, 20000 operations per row; no full CAS/UI timing; allocation bytes exclude malloc allocator overhead and C/Giac/LVGL heaps',compiler='C:/mingw64/bin/g++.exe',command=cmd,records=records,sources={s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in sources})
(out/'profile.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps(result,indent=2))
