#!/usr/bin/env python3
"""Matched native tutor navigation. Timings require the isolated page probe overlay.

Raw logs retain every sample. Host times are diagnostic, never an ESP32 oracle.
"""
import argparse, importlib.util, json, math, os, statistics, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('teaching',ROOT/'scripts/test-tutor-teaching-ui.py')
teaching=importlib.util.module_from_spec(spec);spec.loader.exec_module(teaching);eq=teaching.eq
CASES={k:teaching.CASES[k] for k in ['isolated','linear','quadratic','complex','rational','system','dependent','system3']}
CASES['wide']=['( ( 3 1 2 3 4 5 * x - 2 1 2 3 4 5 ) * ( x + 4 1 2 3 4 5 ) ) / ( x + 4 1 2 3 4 5 ) RIGHT = 0']
def main():
 p=argparse.ArgumentParser();p.add_argument('--bin',required=True);p.add_argument('--out',required=True);p.add_argument('--samples',type=int,default=30);p.add_argument('--warmup',type=int,default=5);p.add_argument('--cases',nargs='+',choices=list(CASES));p.add_argument('--expect-no-polynomial-scan',action='store_true');a=p.parse_args()
 out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True);env=os.environ.copy();env['PATH']='C:/SDL2/x86_64-w64-mingw32/bin;C:/mingw64/bin;'+env['PATH']
 def run(name,script):
  script+='log PAGE_BENCHMARK_COMPLETE\n';file=out/(name+'.numos');file.write_text(script,encoding='utf-8')
  frames=sum(int(x.split()[1]) if x.startswith('wait ') else 1 for x in script.splitlines())+100
  r=subprocess.run([str(Path(a.bin).resolve()),'--headless','--deterministic','--quiet','--frames',str(frames),'--script',os.path.relpath(file,ROOT)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
  (out/(name+'.log')).write_bytes(r.stdout);assert r.returncode==0 and b'PAGE_BENCHMARK_COMPLETE' in r.stdout,(name,r.returncode,r.stdout[-3000:])
  return r.stdout.decode(errors='replace')
 results=[]
 for name,expressions in CASES.items():
  if a.cases and name not in a.cases:continue
  start=(eq.single(expressions[0]) if len(expressions)==1 else eq.system(expressions))+eq.keys('tools')
  if name=='complex':start=start.replace('policy real','policy complex')
  found=run(name+'-discover',start+'assert_equations view dump\n')
  view=json.loads(next(x.split('[TUTOR_VIEW] ',1)[1] for x in found.splitlines() if '[TUTOR_VIEW] ' in x));count=view['count']
  script=start+'log PERF first-final\n'+eq.keys('RIGHT '*(count-1))+'assert_equations view dump\nassert_equations trace dump\n'
  for workload,keys in [('repeat','RIGHT'),('reopen','BACK tools'),('revisit','LEFT RIGHT'),('mode','EXE EXE')]:
   script+='log PERF warmup-'+workload+'\n'+eq.keys((keys+' ')*a.warmup)
   script+='log PERF '+workload+'\n'
   for sample in range(a.samples):
    script+=eq.keys(keys)+'assert_equations trace builds 1\nassert_equations view dump\n'
  script+='log PERF finish\nassert_equations trace check\nassert_equations trace dump\n'
  log=run(name,script);group='initial';groups={};last_views=[]
  for line in log.splitlines():
   if 'PERF ' in line:group=line.split('PERF ',1)[1].strip()
   if '[PAGE_PROFILE] ' in line:groups.setdefault(group,[]).append(json.loads(line.split('[PAGE_PROFILE] ',1)[1]))
   if '[TUTOR_VIEW] ' in line:last_views.append(json.loads(line.split('[TUTOR_VIEW] ',1)[1]))
  for workload in ['repeat','reopen','revisit','mode']:
   rows=groups.get(workload,[])
   # Revisit/mode perform two preparations; retain both in raw, summarize final guided page.
   rows=rows[1::2] if workload in ('revisit','mode') else rows
   if a.expect_no_polynomial_scan:
    assert rows,(name,workload,'instrumented page counters are required')
   if rows:
    assert len(rows)==a.samples,(name,workload,len(rows))
    if a.expect_no_polynomial_scan and name in ('quadratic','complex'):
     assert all(r['capture_calls']==0 for r in rows),(name,workload,'closed roots retried the polynomial scanner')
    metrics={k:dict(n=len(rows),median=statistics.median(r[k] for r in rows),p95=sorted(r[k] for r in rows)[math.ceil(.95*len(rows))-1],maximum=max(r[k] for r in rows)) for k in rows[0] if k.endswith('_us')}
    results.append(dict(case=name,workload=workload,page=count-1,metrics=metrics))
  (out/(name+'-samples.json')).write_text(json.dumps(groups,indent=2),encoding='utf-8')
  if a.expect_no_polynomial_scan and name in ('quadratic','complex'):
   first=groups['first-final'][-1]
   assert first['index']==count-1 and first['capture_calls']==0,(name,'cold final page retried polynomial scanner')
  traces=[json.loads(x.split('[TUTOR_TRACE] ',1)[1]) for x in log.splitlines() if '[TUTOR_TRACE] ' in x]
  assert len(traces)==2 and teaching.stable(traces[0])==teaching.stable(traces[1]),(name,'trace mutated')
  print(name,count,'pages: replay PASS',flush=True)
 (out/'summary.json').write_text(json.dumps(dict(samples=a.samples,warmup=a.warmup,results=results),indent=2),encoding='utf-8')
if __name__=='__main__':main()
