#!/usr/bin/env python3
"""Counterbalanced native probe timings; no hardware performance inference."""
import argparse,hashlib,json,os,statistics,subprocess,time
from datetime import datetime,timezone
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--candidate',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--pairs',type=int,default=5)
p.add_argument('--layouts',type=int,default=1000)
p.add_argument('--verify-only',action='store_true',help='Check retained input identities without executing either probe')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
groups={};identities={};runs=[]
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
utc=lambda:datetime.now(timezone.utc).isoformat()
for label,directory in (('baseline',a.baseline),('candidate',a.candidate)):
 executable=directory/'resources.exe';manifest=directory/'source-manifest.json';source=json.loads(manifest.read_text(encoding='utf-8'))
 for item in source['files']:
  root=Path(__file__).resolve().parents[1] if item.get('dependencyFromCurrentTree') else Path(source['source'])
  assert sha(root/item['path'])==item['sha256'],(label,'source changed',item['path'])
 linked_path=directory/'linked-manifest.json';linked=json.loads(linked_path.read_text(encoding='utf-8'))
 for item in linked:assert sha(Path(item['path']))==item['sha256'],(label,'linked artifact changed',item['path'])
 identities[label]={'executable':str(executable.resolve()),'sha256':sha(executable),'sourceManifestSha256':sha(manifest),'linkedManifestSha256':sha(linked_path),'verifiedSourceFiles':len(source['files']),'verifiedLinkedArtifacts':len(linked)}
if a.verify_only:
 (a.out/'prepared-identities.json').write_text(json.dumps(dict(verifiedAtUtc=utc(),workloadsExecuted=False,identities=identities),indent=2),encoding='utf-8')
 print('PASS retained probe identities; no workloads executed')
 raise SystemExit(0)
for repeat in range(a.pairs):
 for label,directory in ([('baseline',a.baseline),('candidate',a.candidate)] if repeat%2==0 else [('candidate',a.candidate),('baseline',a.baseline)]):
  assert sha(directory/'resources.exe')==identities[label]['sha256'],('probe changed',label)
  started=utc();clock=time.perf_counter()
  run=subprocess.run([str((directory/'resources.exe').resolve()),str(a.layouts)],env=env,capture_output=True)
  log=a.out/f'{repeat}-{label}.log';log.write_bytes(run.stdout+run.stderr)
  runs.append(dict(pair=repeat,label=label,startedAtUtc=started,finishedAtUtc=utc(),elapsedSeconds=time.perf_counter()-clock,exitCode=run.returncode,log=log.name,logSha256=sha(log)))
  assert run.returncode==0,(label,repeat,run.stderr[-2000:])
  rows=[json.loads(line) for line in run.stdout.decode('utf-8').splitlines() if line.startswith('{')]
  assert rows[-1]['summary'] and rows[-1]['failures']==0
  for r in rows[:-1]:groups.setdefault((r['case'],r['style'],r['phase']),{}).setdefault(label,[]).append(r)
result=[]
for (case,style,phase),labels in groups.items():
 row=dict(case=case,style=style,phase=phase)
 for label,items in labels.items():
  values=[r['elapsedNs']/r['repeats'] for r in items]
  counters={key:items[0][key]/items[0]['repeats'] for key in items[0] if key not in ('case','style','phase','repeats','elapsedNs','cursorNs','maxLayoutDepth')}
  for key in counters:
   assert all(r[key]==items[0][key] for r in items),(case,style,phase,label,key,'non-deterministic count')
  assert all(r['maxLayoutDepth']==items[0]['maxLayoutDepth'] for r in items)
  row[label]={'medianNs':statistics.median(values),'minNs':min(values),'maxNs':max(values),'countersPerRepeat':counters,'maxLayoutDepth':items[0]['maxLayoutDepth'],
   'medianCursorNs':statistics.median(r['cursorNs']/r['cursorCalls'] for r in items) if items[0]['cursorCalls'] else None}
 row['medianRatio']=row['candidate']['medianNs']/row['baseline']['medianNs']
 result.append(row)
report=dict(pairs=a.pairs,layoutsPerCaseStylePerRun=a.layouts,identities=identities,runs=runs,rows=result,
 limitations=['Native O1 private probes include observer overhead; ratios do not predict ESP32 latency.','Baseline public layout has no admission API; candidate measures checked admission plus layout.','Refresh/navigation includes the onDraw layout inside the measured interval, plus LVGL draw tasks and editor navigation; it is not pure paint. Exclusive cursor duration is separate.','No cache added. Counts must be deterministic across pairs.','Concurrent host work can affect timings; timing ratios are observations, not standalone acceptance thresholds.'])
(a.out/'comparison.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
md='# Counterbalanced private renderer timing\n\nFive alternating-order pairs by default; counts are stable across repeats. Times are native observations including instrumentation, not device performance or a standalone pass/fail threshold. Candidate layout includes structural/coordinate admission.\n\n| Case / style / phase | Baseline ns | Candidate ns | Ratio | Candidate glyph lookups | Candidate admission calls | Cursor layouts |\n|---|---:|---:|---:|---:|---:|---:|\n'
for r in result:
 c=r['candidate']['countersPerRepeat'];md+=f"| {r['case']} / {r['style']} / {r['phase']} | {r['baseline']['medianNs']:.1f} | {r['candidate']['medianNs']:.1f} | {r['medianRatio']:.3f} | {c['glyphLookups']:.1f} | {c['admissionCalls']:.1f} | {c['cursorLayoutCalls']:.0f} |\n"
(a.out/'comparison.md').write_text(md,encoding='utf-8')
print('PASS',len(result),'paired phase records; counters deterministic')
