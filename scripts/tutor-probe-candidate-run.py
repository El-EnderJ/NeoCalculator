"""Isolate reviewer challenge cases; timeout kills only this host test process."""
import collections, json, pathlib, subprocess
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'out/tutor-engine-01/review-math'
cases=json.loads((root/'tests/host/tutor_probe_challenge.json').read_text())['cases']
cases += [{'id':'mutation-domain-injection','equations':['1=1'],'variables':['x']}]
cases += [{'id':'mutation-zero-root-division','equations':['x^2=x'],'variables':['x']}]
cases += [{'id':'mutation-binding-side-effect','equations':['A*x=10'],'variables':['x'],'binding_test':True}]
summary=[]
mutations=collections.defaultdict(collections.Counter)
context_checks=[]
for case in cases:
    args=[str(out/'tutor_probe_candidate.exe'),';'.join(case['equations']),','.join(case['variables'])]
    if case.get('domain')=='complex':args.append('complex')
    if case.get('binding_test'):args.append('--binding-test')
    with (out/(case['id']+'.jsonl')).open('w',encoding='utf8') as stdout, (out/(case['id']+'.stderr')).open('w',encoding='utf8') as stderr:
        try:
            result=subprocess.run(args,cwd=root,stdout=stdout,stderr=stderr,timeout=10)
            record={'case':case['id'],'exit':result.returncode}
        except subprocess.TimeoutExpired:
            record={'case':case['id'],'timeout_seconds':10}
    try:
        rows=[json.loads(x) for x in (out/(case['id']+'.jsonl')).read_text().splitlines()]
        record.update(status=rows[0].get('status'),diagnostic=rows[0].get('diagnostic'),steps=len(rows[0].get('steps',[])),accepted_mutations=[r['mutation'] for r in rows if r.get('mutation') and r['verdict']==1],context_failures=[r['context_check'] for r in rows if r.get('context_check') and not r['pass']])
        for row in rows:
            if 'mutation' in row:mutations[row['mutation']][row['verdict']]+=1
            if 'context_check' in row:context_checks.append(row)
        if rows[0].get('status')==2 and not any(r.get('replay_verdict')==1 for r in rows):record['replay_failed']=True
    except (IndexError,json.JSONDecodeError):pass
    summary.append(record)
(out/'challenge-summary.json').write_text(json.dumps(summary,indent=2))
(out/'mutation-results.json').write_text(json.dumps({'cases':len(cases),'steps':sum(c.get('steps',0) for c in summary),'statuses':dict(collections.Counter(c.get('status') for c in summary)),'mutations':{k:dict(v) for k,v in mutations.items()},'context_checks':context_checks},indent=2))
print(json.dumps(summary,indent=2))
required=('balance_wrong_sign_result','explanation_wrong_operand','division_by_x_loses_zero','missing_branch','removed_denominator_exclusions','row_wrong_result','stale_epoch','forged_verified_state')
bad=any(c.get('exit')!=0 or c.get('accepted_mutations') or c.get('context_failures') or c.get('replay_failed') for c in summary)
bad=bad or any(not mutations[m] or set(mutations[m])!={2} for m in required)
raise SystemExit(1 if bad else 0)
