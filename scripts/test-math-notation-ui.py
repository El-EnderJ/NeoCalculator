#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check real tutor notation, optionally against an immediate baseline capture.

Capture with test-tutor-teaching-ui.py (also accepts notation-factor). This
compares every proof field except elapsed micros; presentation differences are
asserted individually, never removed from the mathematical replay wholesale.
"""
import argparse, json
from pathlib import Path

def nodes(n):
    yield n
    for c in n['children']: yield from nodes(c)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--before',type=Path)
    a=p.parse_args(); checked=0; invariant=0
    for path in a.after.glob('*-trace.json'):
        trace=json.loads(path.read_text(encoding='utf-8'))
        if a.before and (a.before/path.name).exists():
            old=json.loads((a.before/path.name).read_text(encoding='utf-8'))
            old.pop('micros',None);trace.pop('micros',None)
            assert trace==old,('proof changed',path.name);invariant+=1
        views=json.loads(path.with_name(path.name.replace('-trace','-views')).read_text(encoding='utf-8'))
        old_views_path=a.before/path.name.replace('-trace','-views') if a.before else None
        if old_views_path and old_views_path.exists():
            old_views=json.loads(old_views_path.read_text(encoding='utf-8'))
            old_pages={v['page']:v for v in old_views}
            for v in views:
                old=old_pages[v['page']]
                for key in ('page','count','guided','step','lastStep','kind','title','prose','heading'):
                    assert v[key]==old[key],('teaching semantics changed',path.name,key)
                provenance=lambda fs:[{k:value for k,value in f.items() if k!='ast'} for f in fs]
                assert provenance(v['formulas'])==provenance(old['formulas']),('formula provenance changed',path.name)
        for v in views:
            for f in v['formulas']:
                ns=list(nodes(f['ast'])); texts=[n['text'] for n in ns]
                if f['kind'] in ('discriminant_definition','discriminant_values','general_formula'):
                    assert '\u0394' in texts and 'D' not in texts and '\u2206' not in texts,(path.name,f['kind'])
                    if f['kind']!='discriminant_values':
                        assert '×' not in texts,(path.name,'algebraic product still explicit')
                    else:
                        assert texts.count('×')==2,('numeric instructional products hidden',texts)
                        # Checked discriminant binding, not a computed replacement.
                        step=trace['steps'][f['step']]
                        assert step['auxiliaries'][-1] in texts,(step,texts)
                if path.name.startswith('notation-factor') and v['page']==0 and f['kind']=='equation' and f['state']==1:
                    assert '×' not in texts and '*' not in texts
                    assert '2' in texts and '5' in texts and '1' in texts
                checked+=1
    assert checked>0
    if a.before:assert invariant>0
    result=dict(pass_=True,formulas=checked,exact_trace_pairs=invariant,excluded_trace_fields=['micros'])
    (a.after/'notation-checks.json').write_text(json.dumps(result,indent=2));print(result)

if __name__=='__main__':main()
