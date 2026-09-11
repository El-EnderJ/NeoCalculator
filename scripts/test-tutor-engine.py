#!/usr/bin/env python3
"""Seeded exact-family tests of production traces; challenge corpus stays separate."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def cases(seed):
    acceptance = [
        'x=1', '1=x', '3*x+5=20', '3*(2*x-1)=9', '2*x+3=x-4',
        '(x-1)/2+(x+1)/3=5', '0*x=0', '0*x=1', 'x^2=x', 'x^2=9',
        'x^2-2=0', 'x^2-5*x+6=0', '(x-1)^2=0', '2*x^2+3*x-4=0',
        'x^2+1=0', '(x^2-1)/(x-1)=0', 'x/x=1',
        'x+y=3;x-y=1', 'x+y=2;2*x+2*y=4', 'x+y=2;2*x+2*y=5',
        'x+y+z=6;x-y+z=2;x+y-z=0',
    ]
    for i, equation in enumerate(acceptance):
        yield {'id': f'acceptance-{i:02}', 'equations': equation.split(';')}
    yield {'id': 'acceptance-complex', 'equations': ['x^2+1=0'], 'complex': True}
    rng = random.Random(seed)
    nonzero = lambda: rng.choice([i for i in range(-7, 8) if i])
    for i in range(48):
        a, b, c, d = nonzero(), rng.randint(-9, 9), rng.randint(-6, 6), rng.randint(-9, 9)
        forms = [f'{a}*x+({b})={c}*x+({d})',
                 f'{d}+({c})*x={b}+({a})*x',
                 f'({a}*x+({b}))/3=({c}*x+({d}))/3']
        expected = {'classification': 3 if b == d else 2} if a == c else {'roots': [str(Fraction(d-b, a-c))]}
        yield {'id': f'seed-linear-{i}', 'equations': [forms[i % 3]], **expected}
    for i in range(32):
        a, b, r, s = nonzero(), nonzero(), rng.randint(-6, 6), rng.randint(-6, 6)
        product = f'({a}*x+({r}))*({b}*x+({s}))'
        expanded = f'{a*b}*x^2+({a*s+b*r})*x+({r*s})'
        forms = [product+'=0', expanded+'=0', '0='+expanded]
        yield {'id': f'seed-product-{i}', 'equations': [forms[i % 3]],
               'roots': sorted({str(Fraction(-r, a)), str(Fraction(-s, b))})}
    for i in range(24):
        r, excluded = rng.randint(-7, 7), rng.randint(-7, 7)
        yield {'id': f'seed-rational-{i}',
               'equations': [f'((x-({r}))*(x-({excluded})))/(x-({excluded}))=0'],
               'roots': [] if r == excluded else [str(r)], 'conditions': 1}
    for i in range(24):
        x, y, a, b, c, d = (rng.randint(-5, 5) for _ in range(6))
        if a*d == b*c: a, b, c, d = 2, 1, 1, -1
        yield {'id': f'seed-system-{i}',
               'equations': [f'{a}*x+({b})*y={a*x+b*y}', f'{c}*x+({d})*y={c*x+d*y}'],
               'tuple': [str(x), str(y)]}
    for i in range(24):
        values = [rng.randint(-5, 5) for _ in range(3)]
        matrix = [[rng.randint(-5, 5) for _ in range(3)] for _ in range(3)]
        a, b, c = matrix
        det = a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
        if not det: matrix = [[2, 1, -1], [1, -2, 1], [1, 1, 2]]
        equations = []
        for row in matrix:
            left = '+'.join(f'({coefficient})*{variable}' for coefficient, variable in zip(row, 'xyz'))
            right = sum(coefficient*value for coefficient, value in zip(row, values))
            equations.append(f'({left})/2={right}/2' if i % 2 else f'{left}={right}')
        yield {'id': f'seed-system3-{i}', 'equations': equations, 'tuple': list(map(str, values))}
    for i, equation in enumerate(['sqrt(x+2)=x', 'ln(x)=1', 'abs(x)=2', 'exp(x)=3', 'sin(x)=0', 'x^3=7']):
        yield {'id': f'extension-{i}', 'equations': [equation], 'unsupported': True}


def check(case, d):
    if case.get('unsupported'):
        assert d['status'] == 0, d['diagnostic']
        return
    assert d['status'] == 2, d['diagnostic']
    assert all(d[k] == 1 for k in ['validity', 'completeness', 'candidates', 'reconciliation'])
    assert d['authored'] == [e.split('=') for e in case['equations']]
    assert len(d['states']) == len(d['steps']) + 1
    assert d['bytes'] <= 65536 and d['peakVectorHeapBytes'] <= 131072
    seen = set()
    for i, step in enumerate(d['steps']):
        assert step['before'] == i and step['after'] == i+1 and step['verdict'] == 1
        before, after = d['states'][i:i+2]
        assert step['prerequisites'] == list(range(len(before['conditions'])))
        if step['rule'] != 'domain.nonzero': assert before['conditions'] == after['conditions']
        if step['relation'] == 0:
            assert before['branches'] != after['branches'] and after['fingerprint'] not in seen
        seen.add(before['fingerprint'])
        assert all(term not in step['text'].lower() for term in ['processing', 'applying algorithm', 'subtract -'])
        assert step['text'] and step['key'] != 'unknown'
        assert not any(token in step['text'] for token in ['sqrt(', '^', '!=', '+/-', '*']), 'source formula leaked into instructional prose'
        if step['key'] in ['equation.add', 'equation.subtract', 'equation.divide', 'equation.divide_square']:
            assert len(step['parameters']) == 2 and step['parameters'][1] == 'x'
            amount = Fraction(step['parameters'][0])
            assert amount == (abs(Fraction(step['operand'])) if step['rule'] == 'equation.add' else Fraction(step['operand']))
        if step['rule'] == 'equation.add': assert Fraction(step['operand']) != 0 if 'x' not in step['operand'] else True
        if step['rule'] in ['equation.divide', 'system.scale']: assert Fraction(step['operand']) not in (0, 1)
        for j, index in enumerate(step['substeps']):
            child = d['steps'][index]
            assert index == i+j+1 and child['group'] == step['group'] and child['branch'] == step['branch']
            assert child['relation'] == step['relation'] == 0 and not child['substeps']
    final = d['states'][-1]
    if 'classification' in case: assert final['conclusion'] == case['classification']
    if 'conditions' in case: assert len(final['conditions']) == case['conditions']
    if 'roots' in case:
        actual = [] if final['conclusion'] == 2 else [str(Fraction(b['equations'][0][1])) for b in final['branches'] if not b['rejected']]
        assert set(actual) == set(case['roots']), (actual, case['roots'])
    if 'tuple' in case:
        assert [str(Fraction(e[1])) for e in final['branches'][0]['equations']] == case['tuple']
    if case['id'] == 'acceptance-00': assert [s['rule'] for s in d['steps']] == ['terminal.isolated']
    if case['id'] == 'acceptance-02':
        teaching = [(s['key'], s['text']) for s in d['steps'][:2]]
        assert teaching == [('equation.subtract', 'Subtract 5 from both sides to isolate the term containing x.'),
                            ('equation.divide', 'Divide both sides by 3 to isolate x.')]
    if case['id'] == 'acceptance-09': assert d['steps'][0]['rule'] == 'square.branches'


def stable(d):
    return {k: v for k, v in d.items() if k not in ['micros', 'vectorHeapBytes', 'peakVectorHeapBytes']}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bin', default='out/tutor-engine-01/host/tutor_engine.exe')
    ap.add_argument('--out', default='out/tutor-engine-01/seeded')
    ap.add_argument('--seed', type=int, default=20260910)
    args = ap.parse_args()
    out = ROOT / args.out; out.mkdir(parents=True, exist_ok=True)
    corpus = list(cases(args.seed))
    (out/'inputs.json').write_text(json.dumps({'seed': args.seed, 'cases': corpus}, indent=2))
    results = []; traces = []
    def run(case):
        r = subprocess.run([str(ROOT/args.bin), *case['equations'], *(['--complex'] if case.get('complex') else [])], cwd=ROOT,
                           capture_output=True, timeout=30)
        assert r.returncode == 0, (r.returncode, r.stderr.decode(errors='replace'))
        return json.loads(r.stdout)
    for i, case in enumerate(corpus):
        try:
            trace = run(case); traces.append({'id': case['id'], 'trace': trace}); check(case, trace)
            if i % 7 == 0: assert stable(trace) == stable(run(case)), 'deterministic replay differs'
            result = {'id': case['id'], 'pass': True, 'steps': len(trace['steps']), 'bytes': trace['bytes'], 'micros': trace['micros']}
        except Exception as e: result = {'id': case['id'], 'pass': False, 'error': str(e)}
        results.append(result); print(('PASS ' if result['pass'] else 'FAIL ')+case['id']+' '+result.get('error', ''), flush=True)
        (out/'results.json').write_text(json.dumps(results, indent=2))
        (out/'replay.jsonl').write_text('\n'.join(json.dumps(t) for t in traces)+'\n')
    raise SystemExit(0 if all(r['pass'] for r in results) else 1)


if __name__ == '__main__': main()
