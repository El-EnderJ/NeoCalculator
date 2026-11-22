#!/usr/bin/env python3
"""Compile and execute independent uncertainty oracles without Giac, LVGL or UI."""
import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
a.source=a.source.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
files=['tests/host/scientific_uncertainty_checks.cpp','src/math/ScientificUncertainty.cpp','src/math/ScientificUncertainty.h','src/math/scientific/PhysicalConstants.cpp','src/math/scientific/PhysicalConstants.h']
hashes={f:hashlib.sha256((a.source/f).read_bytes()).hexdigest() for f in files}
binary=a.out/('scientific.exe' if os.name=='nt' else 'scientific')
compiler='C:/mingw64/bin/g++.exe' if os.name=='nt' else 'g++'
cmd=[compiler,'-std=c++17','-O1','-Wall','-Wextra','-I'+str(a.source/'src'),str(a.source/files[0]),str(a.source/files[1]),str(a.source/files[3]),'-o',str(binary)]
env=dict(os.environ)
if os.name=='nt':env['PATH']='C:/mingw64/bin;'+env.get('PATH','')
records=[]
for label,command in [('compile',cmd),('run',[str(binary)])]:
 start=time.perf_counter();r=subprocess.run(command,cwd=a.source,env=env,capture_output=True)
 (a.out/(label+'.log')).write_bytes(r.stdout+r.stderr);records.append(dict(name=label,command=command,exit=r.returncode,seconds=time.perf_counter()-start))
 counts=re.search(rb'independent assertions=(\d+)',r.stdout) if label=='run' else None
 (a.out/'results.json').write_text(json.dumps(dict(scope='10 test groups; 128 generated triples are scenarios; assertions are not distinct cases',assertions=int(counts[1]) if counts else None,inputs=hashes,commands=records,passed=all(c['exit']==0 for c in records)),indent=2))
 print((r.stdout+r.stderr).decode('utf8',errors='replace'),end='',flush=True);r.check_returncode()
