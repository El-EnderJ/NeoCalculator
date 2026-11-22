#!/usr/bin/env python3
"""Local browser matrix on one frozen UI package; never builds or deploys.

Runs existing semantic/interaction assertions on Chromium/Firefox/WebKit and
shell/component. The legacy accumulated WebKit trial stays a separate record.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import statistics
from PIL import Image, ImageChops

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--web-root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--only',nargs='*')
    p.add_argument('--prior-spacing',type=Path)
    p.add_argument('--prior-events',type=Path,help='Prior event-timings directory for workload-matched median comparisons')
    p.add_argument('--workers',type=int,default=2)
    a=p.parse_args();a.web_root=a.web_root.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    identity=lambda:{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(a.web_root.iterdir()) if p.is_file()}
    before=identity();assert any(n.endswith('.wasm') for n in before)
    (a.out/'package-before.json').write_text(json.dumps(before,indent=2),encoding='utf8')
    base=dict(os.environ,NUMOS_WEB_ROOT=str(a.web_root),PYTHONIOENCODING='utf8')
    for k in ('NUMOS_BROWSER','NUMOS_SURFACE','NUMOS_LOCALES','NUMOS_ACCUMULATED','NUMOS_SPACING_WEB_BASELINE'):base.pop(k,None)
    jobs=[]
    for name,script,outvar in [('scientific','scientific-quantities.mjs','NUMOS_SCIENTIFIC_OUT'),
                              ('temperatures','calculation-temperatures.mjs','NUMOS_QUANTITY_OUT'),
                              ('brackets','calculation-brackets.mjs','NUMOS_BRACKET_OUT'),
                              ('spacing','math-spacing.mjs','NUMOS_SPACING_WEB_OUT')]:
        env=dict(base);env[outvar]=str(a.out/name)
        if name=='spacing' and a.prior_spacing:env['NUMOS_SPACING_WEB_BASELINE']=str(a.prior_spacing.resolve())
        jobs.append((name,['node',str(ROOT/'tests/wasm'/script)],env))
    jobs.append(('legacy',[sys.executable,str(ROOT/'scripts/run-scientific-web-regressions.py'),
                           '--web-root',str(a.web_root),'--out',str(a.out/'legacy')],base))
    # This historical runner resolves the package through its source root.
    # Reject an unexpected hierarchy instead of silently testing a different UI.
    source=a.web_root.parents[3]
    if source/'out/wasm/dist/release'==a.web_root:
        for browser in ('chromium','firefox','webkit'):
            env=dict(base,NUMOS_SOURCE=str(source),NUMOS_BROWSER=browser,NUMOS_CALC_OUT=str(a.out/'input'))
            jobs.append(('input-'+browser,['node',str(ROOT/'tests/wasm/calculation-input.mjs')],env))
    timing_env=dict(base,NUMOS_PARITY_EVENTS_OUT=str(a.out/'event-timings'))
    timing_job=('event-timings',['node',str(ROOT/'tests/wasm/math-tex-parity-product-events.mjs')],timing_env)
    jobs.append(timing_job)
    if a.only:
        assert set(a.only)<={n for n,_,_ in jobs},'unknown or unavailable suite'
        jobs=[j for j in jobs if j[0] in a.only]

    def run(job):
        name,command,env=job;start=time.perf_counter()
        with (a.out/(name+'.log')).open('wb') as log:
            try:r=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=3600);code=r.returncode
            except subprocess.TimeoutExpired:code=-999
        row=dict(name=name,command=command,exit=code,passed=code==0,seconds=time.perf_counter()-start)
        print('PASS' if row['passed'] else 'FAIL',name,flush=True)
        return row

    # Measure browser latency without this runner's other concurrent workloads.
    parallel=[job for job in jobs if job[0]!='event-timings']
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:rows=list(pool.map(run,parallel))
    if any(job[0]=='event-timings' for job in jobs):rows.append(run(timing_job))
    if any(r['name']=='event-timings' and r['passed'] for r in rows):
        timing=a.out/'event-timings'
        def event_medians(folder):
            grouped={}
            for sample in json.loads((folder/'results.json').read_text(encoding='utf8')):
                name='/'.join(sample[k] for k in ('browser','surface','id'))
                grouped.setdefault(name,[]).append(sample)
            medians={}
            for name,samples in grouped.items():
                values={metric:statistics.median(s[metric] for s in samples) for metric in ('wallMs','processingMs','presentationMs','heapBytes','usedHeapBytes')}
                values.update({('cumulativeFrame'+metric.upper()+'Ms'):statistics.median(s['frameMs'][metric] for s in samples) for metric in ('p50','p95','max')})
                medians[name]=dict(repetitions=len(samples),**values)
            return medians
        medians=event_medians(timing)
        (timing/'medians.json').write_text(json.dumps(medians,indent=2),encoding='utf8')
        if a.prior_events:
            previous=event_medians(a.prior_events.resolve());assert previous.keys()==medians.keys(),'timing workloads/surfaces differ'
            comparison={name:{metric:dict(before=previous[name][metric],after=after,delta=after-previous[name][metric],ratio=after/previous[name][metric] if previous[name][metric] else None)
                              for metric,after in values.items() if metric!='repetitions'} for name,values in medians.items()}
            (timing/'before-after.json').write_text(json.dumps(dict(scope='Workload-matched host end-to-end medians; frame distributions are cumulative process diagnostics, not per-event samples; timing differences are observations, not automatic failures',workloads=comparison),indent=2),encoding='utf8')
    if any(r['name']=='spacing' and r['passed'] for r in rows):
        spacing=a.out/'spacing';reference=json.loads((spacing/'chromium/results.json').read_text(encoding='utf8'))
        comparison=[]
        for browser in ('firefox','webkit'):
            diagnostics=json.loads((spacing/browser/'results.json').read_text(encoding='utf8'))
            paths=list((spacing/'chromium').glob('*.png'))
            identical=sum(ImageChops.difference(Image.open(path).convert('RGB').crop((0,25,320,240)),
                                               Image.open(spacing/browser/path.name).convert('RGB').crop((0,25,320,240))).getbbox() is None for path in paths)
            comparison.append(dict(browser=browser,diagnosticsEqual=diagnostics==reference,framesCompared=len(paths),identicalMathFrames=identical,
                                   excludedRegion='status bar y=0..24 only; cursor differences are not masked'))
        (spacing/'cross-browser.json').write_text(json.dumps(comparison,indent=2),encoding='utf8')
    after=identity();(a.out/'package-after.json').write_text(json.dumps(after,indent=2),encoding='utf8')
    summary=dict(packageUnchanged=before==after,requestedRuns=rows,
                 allRequestedPassed=before==after and all(r['passed'] for r in rows),
                 accumulatedWebKit='See legacy/summary.json; independent of fresh contexts, not added to unique-case count')
    (a.out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
    return 0 if summary['allRequestedPassed'] else 1


if __name__=='__main__':raise SystemExit(main())
