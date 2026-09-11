#!/usr/bin/env python3
"""Build a separate teaching candidate and compare its proof graph with baseline."""
import argparse, collections, hashlib, json, pathlib, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'out/tutor-teaching-ux-01/review-math'
def run(args, **kwargs):
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)
def build():
    target=OUT/'candidate-host';target.mkdir(parents=True,exist_ok=True)
    flags=['-std=gnu++17','-O1','-fexceptions','-ffunction-sections','-fdata-sections',
           '-D_USE_MATH_DEFINES','-DNUMOS_GIAC_HOST_HARNESS=1','-D__MINGW_H','-fpermissive',
           '-Ilib/giac','-Ilib/giac/src','-Ilib/libtommath','-Isrc','-w']
    flags+=['-D'+x for x in 'HAVE_CONFIG_H IN_GIAC GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED USE_GMP_REPLACEMENTS UMAP DOUBLEVAL'.split()]
    watched=['src/math/giac/GiacEngine.cpp','src/math/giac/GiacTutor.inc','src/math/tutor/Derivation.h','src/math/tutor/Messages.inc','src/math/tutor/TeachingPlan.h','src/math/MathAST.h','src/math/MathAST.cpp']
    before={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in watched}
    for source in ['src/math/giac/GiacEngine.cpp','tests/host/tutor_engine_main.cpp','tests/host/tutor_teaching_math.cpp']:
        run(['g++',*flags,'-c',source,'-o',str((target/(pathlib.Path(source).name+'.o')).relative_to(ROOT))])
    after={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in watched}
    if before!=after:raise RuntimeError('Source changed while compiling; rerun')
    (OUT/'candidate-source-hashes.json').write_text(json.dumps(after,indent=2))
    closure=(ROOT/'out/tutor-engine-01/host/link.rsp').read_text().splitlines()
    closure=[x for x in closure if not any(n in x for n in ['GiacEngine.cpp.o','tutor_engine_main.cpp.o'])]
    # Rebuild project closure after headers change; vendor objects alone are cached.
    sources={p.name:p for folder in ['src','tests/host'] for p in (ROOT/folder).rglob('*.cpp')}
    for index,item in enumerate(closure):
        filename=pathlib.Path(item.strip('"')).name.removesuffix('.o')
        if filename not in sources:continue
        source=sources[filename];obj=target/(filename+'.o')
        run(['g++',*flags,'-c',str(source.relative_to(ROOT)),'-o',str(obj.relative_to(ROOT))])
        closure[index]='"'+str(obj.relative_to(ROOT)).replace('\\','/')+'"'
    after={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in watched}
    if before!=after:raise RuntimeError('Source changed while compiling project closure; rerun')
    (OUT/'candidate-source-hashes.json').write_text(json.dumps(after,indent=2))
    closure.append('"'+str((target/'GiacEngine.cpp.o').relative_to(ROOT)).replace('\\','/')+'"')
    for stem,main in [('tutor_engine','tutor_engine_main.cpp.o'),('mutations','tutor_teaching_math.cpp.o')]:
        rsp=target/(stem+'.rsp');rsp.write_text('\n'.join(closure+['"'+str((target/main).relative_to(ROOT)).replace('\\','/')+'"']))
        run(['g++','@'+str(rsp.relative_to(ROOT)),'-Wl,--gc-sections','-static','-lpsapi','-o',str((target/(stem+'.exe')).relative_to(ROOT))])
def graph(trace):
    trace=json.loads(json.dumps(trace))
    for name in ['bytes','vectorHeapBytes','peakVectorHeapBytes','calls','micros','diagnostic']:trace.pop(name,None)
    for step in trace['steps']:
        for name in ['key','text','parameters','parameterKinds']:step.pop(name,None)
    return trace
def compare():
    baseline={r['id']:r['trace'] for r in map(json.loads,(OUT/'baseline-traces/replay.jsonl').read_text().splitlines())}
    candidate={r['id']:r['trace'] for r in map(json.loads,(OUT/'candidate-traces/replay.jsonl').read_text().splitlines())}
    assert baseline.keys()==candidate.keys()
    bad=[name for name in baseline if graph(baseline[name])!=graph(candidate[name])]
    result={'cases':len(baseline),'identical_mathematical_graphs':len(baseline)-len(bad),'differences':bad,'excluded_fields':['message key/text/parameters/kinds','resource/timing diagnostics']}
    (OUT/'graph-invariance.json').write_text(json.dumps(result,indent=2));print(json.dumps(result));assert not bad
def mutations():
    cases=json.loads((ROOT/'tests/host/tutor_probe_challenge.json').read_text())['cases']
    cases += [{'id':'domain-injection','equations':['1=1'],'variables':['x']},
              {'id':'zero-root-division','equations':['x^2=x'],'variables':['x']},
              {'id':'binding-side-effect','equations':['A*x=10'],'variables':['x'],'binding_test':True}]
    folder=OUT/'mutations';folder.mkdir(parents=True,exist_ok=True)
    totals=collections.defaultdict(collections.Counter);summary=[]
    exe=OUT/'candidate-host/mutations.exe'
    for case in cases:
        args=[str(exe),';'.join(case['equations']),','.join(case['variables'])]
        if case.get('domain')=='complex':args.append('complex')
        if case.get('binding_test'):args.append('--binding-test')
        result=run(args,capture_output=True,text=True,timeout=15)
        (folder/(case['id']+'.jsonl')).write_text(result.stdout,encoding='utf-8')
        rows=[json.loads(x) for x in result.stdout.splitlines()]
        assert not any(x.get('verdict')==1 for x in rows if 'mutation' in x)
        assert not any(not x['pass'] for x in rows if 'context_check' in x)
        assert any(x.get('projection_review') and x['pass'] for x in rows)
        if rows[0]['status']==2:assert any(x.get('replay_verdict')==1 for x in rows)
        for row in rows:
            if 'mutation' in row:totals[row['mutation']][row['verdict']]+=1
        summary.append({'id':case['id'],'status':rows[0]['status'],'steps':len(rows[0]['steps'])})
    required=['balance_wrong_sign_result','explanation_wrong_operand','division_by_x_loses_zero','missing_branch','removed_denominator_exclusions','row_wrong_result','stale_epoch','forged_verified_state','wrong_isolation_goal','wrong_goal_variable','wrong_structured_quadratic_coefficient']
    for name in required:assert set(totals[name])=={2},name
    for mode in ['context','snapshot']:
        result=run([str(exe),'--'+mode+'-test'],capture_output=True,text=True)
        (folder/(mode+'.jsonl')).write_text(result.stdout,encoding='utf-8')
    record={'cases':summary,'mutations':{k:dict(v) for k,v in totals.items()},'pass':True}
    (OUT/'mutation-results.json').write_text(json.dumps(record,indent=2));print('mutations pass',sum(sum(x.values()) for x in totals.values()))
def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['build','test','mutations','compare']);args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    if args.action=='build':build()
    elif args.action=='test':
        run(['python','scripts/test-tutor-engine.py','--bin',str(OUT/'candidate-host/tutor_engine.exe'),'--out',str(OUT/'candidate-traces')]);compare();mutations()
    elif args.action=='mutations':mutations()
    else:compare()
if __name__=='__main__':main()
