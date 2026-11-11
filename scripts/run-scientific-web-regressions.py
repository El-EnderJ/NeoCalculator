#!/usr/bin/env python3
"""Run legacy browser regression suites against one frozen local WASM package.

Fresh browser/surface runs and an additional accumulated WebKit run are recorded
separately. Repeated fixtures are never added as new unique coverage.
"""
import argparse,concurrent.futures,hashlib,json,os,subprocess,time
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--web-root',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
a.out=a.out.resolve();a.web_root=a.web_root.resolve();a.out.mkdir(parents=True,exist_ok=True)
def identity():
 return {f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(a.web_root.iterdir()) if f.is_file()}
before=identity();assert before and any(k.endswith('.wasm') for k in before)
(a.out/'package-before.json').write_text(json.dumps(before,indent=2))
env=dict(os.environ,NUMOS_WEB_ROOT=str(a.web_root))
for key in ('NUMOS_BROWSER','NUMOS_SURFACE','NUMOS_LOCALES','NUMOS_ACCUMULATED'):
 env.pop(key,None)
jobs=[('quantities','calculation-quantities.mjs','NUMOS_QUANTITY_OUT',12),
 ('format','calculation-format.mjs','NUMOS_FORMAT_OUT',42),
 ('viewport','calculation-viewport.mjs','NUMOS_VIEWPORT_OUT',18),
 ('locales','system-locales.mjs','NUMOS_LOCALE_OUT',24)]
def run(job,accumulated=False):
 name,script,output_name,expected=job;job_env=dict(env)
 job_env[output_name]=str(a.out/name)
 if accumulated:job_env.update(NUMOS_BROWSER='webkit',NUMOS_ACCUMULATED='1')
 command=['node',str(root/'tests/wasm'/script)];start=time.perf_counter()
 with (a.out/(name+'.log')).open('wb') as log:
  r=subprocess.run(command,cwd=root,env=job_env,stdout=log,stderr=subprocess.STDOUT)
 rows=[];path=a.out/name/'results.json'
 if path.exists():rows=json.loads(path.read_text(encoding='utf8'))
 surfaces=sorted({(v['browser'],v['surface']) for v in rows})
 passed=r.returncode==0 and len(rows)==expected and len(surfaces)==(2 if accumulated else 6)
 if name in ('quantities','locales','webkit-accumulated'):
  passed=passed and sum(v.get('passed') is True for v in rows)==(6 if name=='quantities' else expected)
 result=dict(name=name,command=command,exit=r.returncode,passed=passed,seconds=time.perf_counter()-start,
  records=len(rows),surfaces=surfaces,scope='repeated WebKit locale fixtures in one browser process' if accumulated else 'fresh contexts per surface; locales also use fresh browser processes')
 print(name,'PASS' if passed else 'FAIL',flush=True);return result
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 results=list(pool.map(run,jobs))
results.append(run(('webkit-accumulated','system-locales.mjs','NUMOS_LOCALE_OUT',8),True))
after=identity();(a.out/'package-after.json').write_text(json.dumps(after,indent=2))
summary=dict(packageUnchanged=before==after,runs=results,
 counting='quantities:6 surfaces; format:7 fixtures per surface; viewport:3 fixtures per surface; locales:4 locales per surface; accumulated WebKit repeats existing8 locale fixtures')
(a.out/'summary.json').write_text(json.dumps(summary,indent=2))
raise SystemExit(0 if before==after and all(r['passed'] for r in results) else 1)
