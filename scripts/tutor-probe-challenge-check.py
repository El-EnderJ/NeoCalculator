"""Review exact challenge sets using existing vendored Giac as arithmetic oracle.

This audits output values/constraints as additional evidence, not proof of steps.
The independent C++ mutation runner and production replay check transitions.
"""
import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out/tutor-engine-01/review-math'
def exact_zero(expr):
    result=subprocess.run([str(out/'tutor_probe_giac.exe'),'--case','exact_review','0',f'normal({expr})'],cwd=root,capture_output=True,text=True,timeout=4,check=True)
    return json.loads(result.stdout)['answer']=='0'
results=[]
for case in json.loads((root/'tests/host/tutor_probe_challenge.json').read_text())['cases']:
    result={'case':case['id'],'errors':[]}
    data=(out/(case['id']+'.jsonl')).read_text(encoding='utf-8-sig').splitlines()
    if not data:result['errors'].append('host ordinary-answer timeout');results.append(result);continue
    trace=json.loads(data[0]);result['status']=trace['status']
    if trace['status'] not in (2,4):
        if case.get('classification')=='unsupported' or case['id'] in ('challenge-variable-name','challenge-undefined-everywhere'):
            result['limitation']=trace.get('diagnostic') or 'x/y/z solve-variable contract'
        else:result['errors'].append('supported challenge did not complete: '+trace.get('diagnostic',''))
        results.append(result);continue
    state=trace['states'][-1];active=[b for b in state['branches'] if not b['rejected']]
    if 'roots' in case:
        actual=[] if state['conclusion']==2 else [b['equations'][0][1] for b in active]
        expected=case['roots']
        if len(actual)!=len(expected) or any(not any(exact_zero(f'({a})-({e})') for a in actual) for e in expected):
            result['errors'].append({'expected_roots':expected,'actual_roots':actual})
    if 'tuples' in case:
        actual=[] if state['conclusion']==2 else [[dict(b['equations']).get(v) for v in case['variables']] for b in active]
        expected=case['tuples']
        if len(actual)!=len(expected) or any(not any(all(a and exact_zero(f'({a})-({e})') for a,e in zip(at,et)) for at in actual) for et in expected):
            result['errors'].append({'expected_tuples':expected,'actual_tuples':actual})
    classification=case.get('classification')
    if classification in ('identity','conditional_identity') and state['conclusion']!=3:result['errors'].append('identity classification missing')
    if classification=='family' and state['conclusion']!=4:result['errors'].append('family classification missing')
    for value in case.get('exclusions',[]):
        if not any(exact_zero(f'subst(({c}),{case["variables"][0]},({value}))') for c in state['conditions']):
            result['errors'].append('missing expected exclusion '+value)
    if 'relationship' in case:
        var,rhs=case['relationship'].split('=')
        for b in active:
            for lhs,right in b['equations']:
                if not exact_zero(f'subst(({lhs})-({right}),{var},({rhs}))'):
                    result['errors'].append('family relationship not preserved')
    results.append(result)
(out/'challenge-exact-check.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
raise SystemExit(1 if any(result['errors'] for result in results) else 0)
