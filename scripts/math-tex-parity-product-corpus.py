#!/usr/bin/env python3
"""A–N native product corpus, entered through keys and the real Toolbox.

Original 320x240 framebuffers and boundary traces are retained. This is a
product/input oracle, not an independent oracle for TeX pixel geometry.
"""
import argparse
import concurrent.futures
import hashlib
import html
import json
import os
from pathlib import Path
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PHYSICAL = {'SHIFT': (4, 0), 'ALPHA': (3, 0), 'FORMAT': (4, 5)}


def keys(s):
    return ''.join('calc_physical %d %d\n' % PHYSICAL[k] if k in PHYSICAL else
                   'key TOOLBOX\nsdl_text z (lowercase)\nkey DOWN\nassert_calc_input toolbox selected 225 0\nkey ENTER\n' if k == 'z' else
                   'key '+k+'\n' for k in s.split())


def pick(query, item=None):
    return keys('TOOLBOX')+'sdl_text '+query+'\n'+keys('DOWN')+(f'assert_calc_input toolbox selected {item} 0\n' if item else '')+keys('ENTER')


def corpus():
    rows = []

    def add(group, name, seq='', body=None, complete=True, evaluate=True, app='Calculation'):
        rows.append(dict(group=group, id=group+'-'+name, keys=seq, body=keys(seq) if body is None else body,
                         complete=complete, evaluate=evaluate, app=app))

    for name, seq in [('unary-negative', 'neg 3'), ('add-subtract', 'x + y - z'),
                      ('multiply', 'x * y'), ('implicit', '2 x'),
                      ('two-groups', '( 2 x + 5 ) ( x - 1 )'),
                      ('scientific-fraction', '6 6 . 6 3 / 2 exp neg 3')]:
        add('A', name, seq)
    add('A', 'relation-negative', 'x = neg 3', app='Equations', evaluate=False)
    for name, seq in [('square', 'x ^ 2'), ('negative-exponent', 'x ^ neg 2'),
                      ('sum-exponent', 'x ^ x + y'), ('nested-exponent', 'x ^ y ^ 2'),
                      ('ten-small', '1 0 ^ neg 1 2')]:
        add('B', name, seq)
    add('B', 'e-exponent', body=pick("Euler's number", 181)+keys('POW x ^ 2 RIGHT + 1'))
    for name, seq in [('half', '1 / 2'), ('sums', '( x + y ) / ( x + z )'),
                      ('fraction-over-fraction', '( 1 / x RIGHT ) / ( 1 / y RIGHT )'),
                      ('continued', '1 / 1 + 1 / x'), ('script-fraction', 'x ^ 1 / 2'),
                      ('power-of-fraction', '( 1 / 2 RIGHT ) ^ 3'),
                      ('root-fraction', 'SQRT 1 / x')]:
        add('C', name, seq)
    for name, seq in [('simple', 'SQRT x'), ('polynomial', 'SQRT x ^ 2 RIGHT + 1'),
                      ('fraction', 'SQRT ( x + y ) / ( x + z )'),
                      ('nested', 'SQRT 2 + SQRT 3 + SQRT x')]:
        add('D', name, seq, evaluate=name != 'nested')
    add('D', 'indexed', body=pick('Indexed root', 111)+keys('3 RIGHT x'))
    add('D', 'index-expression', body=pick('Indexed root', 111)+keys('1 + 2 RIGHT x ^ 2 RIGHT + 1'))
    for name, seq in [('paren', '( x )'), ('bracket', '[ x ]'),
                      ('fraction', '( ( x + y ) / ( x + z ) RIGHT )'),
                      ('tall-bracket', '[ 1 / 1 + 1 / x RIGHT RIGHT ]'),
                      ('nested', '( [ ( x + 1 ) / y RIGHT ] )')]:
        add('E', name, seq)
    add('E', 'absolute', body=pick('Absolute value', 103)+keys('x + 1'))
    for name, seq in [('sine', 'sin x'), ('sine-power', 'sin x ^ 2'),
                      ('ln-fraction', 'ln ( x + y ) / ( x + z )'),
                      ('log-base', 'LOGBASE 2 RIGHT x'), ('inverse-sine', 'SHIFT sin x')]:
        add('F', name, seq)
    add('F', 'hyperbolic', body=pick('Hyperbolic sine', 140)+keys('x'))
    add('G', 'discriminant', 'y ^ 2 RIGHT - 4 x z')
    add('G', 'quadratic-formula-positive', '( neg y + SQRT y ^ 2 RIGHT - 4 x z RIGHT ) / 2 x')
    add('G', 'factored', '( x + 1 ) ( x - 1 ) ( x ^ 2 RIGHT + 1 )')
    add('G', 'rational-polynomial', '( x ^ 3 RIGHT + 2 x ^ 2 RIGHT - x + 1 ) / ( x ^ 2 RIGHT - 1 )')
    add('H', 'periodic-input', 'sin x RIGHT = 1 / 2', app='Equations', evaluate=False)
    add('I', 'derivative-template', body=pick('Derivative', 171)+keys('x ^ 2 RIGHT RIGHT x'))
    add('I', 'integral-template', body=pick('Definite integral', 170)+keys('0 RIGHT 1 RIGHT x ^ 2 RIGHT RIGHT x'))
    units = [('acceleration', '9 . 8 1', 'metre', 32769, 'FRAC', 'Second', 32771, 'POW 2'),
             ('resistance', '2', 'Kilohm', None, '', '', None, ''),
             ('capacitance', '2', 'microfarad', None, '', '', None, ''),
             ('area', '2', 'cm²', None, '', '', None, ''),
             ('volume', '2', 'm³', None, '', '', None, ''),
             ('temperature-point', '2 0', 'Degree Celsius', 32791, '', '', None, ''),
             ('temperature-difference', '1 8', 'delta f', 32831, '', '', None, '')]
    for name, seq, unit, ident, between, unit2, ident2, tail in units:
        body=keys(seq)+pick(unit, ident)
        if unit2:
            body+=keys(between)+pick(unit2, ident2)+keys(tail)
        add('J', name, body=body)
    add('K', 'measurement', body=pick('measurement', 750)+keys('1 0 RIGHT 1 RIGHT')+pick('metre', 32769))
    add('K', 'temperature-measurement', body=pick('measurement', 750)+keys('2 0 . 0 RIGHT 0 . 5 RIGHT')+pick('Degree Celsius', 32791))
    add('K', 'G', body=pick('gravitational constant', 16443))
    add('K', 'correlated', body=pick('Bohr radius', 16386)+keys('MUL')+pick('electron mass', 16387))
    for name, seq in [('empty-root', ''), ('power-both', '^'), ('power-base', '^ 2'),
                      ('power-exponent', 'x ^'), ('fraction-both', '/'),
                      ('fraction-numerator', '/ RIGHT RIGHT 2'), ('fraction-denominator', '1 /'),
                      ('radicand', 'SQRT'), ('function', 'sin'), ('nested', '1 / SQRT x ^')]:
        add('L', name, seq, complete=False)
    add('L', 'indexed-both', body=pick('Indexed root', 111), complete=False)
    add('L', 'indexed-radicand', body=pick('Indexed root', 111)+keys('3 RIGHT'), complete=False)
    add('L', 'indexed-index', body=pick('Indexed root', 111)+keys('RIGHT RIGHT x'), complete=False)
    add('L', 'log-base', 'LOGBASE', complete=False)
    add('L', 'log-argument', 'LOGBASE 2 RIGHT', complete=False)
    for name, suffix in [('measurement-both',''), ('measurement-nominal','RIGHT RIGHT 1'), ('measurement-uncertainty','1 RIGHT')]:
        add('L', name, body=pick('measurement', 750)+keys(suffix), complete=False)
    add('L', 'derivative-empty', body=pick('Derivative', 171), complete=False)
    add('L', 'integral-empty', body=pick('Definite integral', 170), complete=False)
    add('M', 'very-wide', ' + '.join('x ^ 2 RIGHT' for _ in range(16)), evaluate=False)
    add('M', 'wide-tall', '1 / '*5+' + '.join('x ^ 2 RIGHT' for _ in range(10)), evaluate=False)
    for d in range(2, 7):
        add('N', 'power-depth-'+str(d), 'x ^ '*d+'2', evaluate=False)
    for d in range(2, 6):
        add('N', 'fraction-depth-'+str(d), '1 / '*d+'x', evaluate=False)
    add('N', 'alternating', '1 / SQRT x ^ 1 / SQRT y ^ 2', evaluate=False)
    add('N', 'delimiters-tall', '[ 1 / SQRT x ^ 1 / y RIGHT RIGHT RIGHT RIGHT ]', evaluate=False)
    return rows


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--phase', choices=('baseline','final'), required=True)
    p.add_argument('--compare', type=Path)
    p.add_argument('--only')
    a=p.parse_args()
    os.chdir(ROOT)
    a.out=Path(os.path.relpath(a.out.resolve(), ROOT))
    a.out.mkdir(parents=True, exist_ok=True)
    env=dict(os.environ, NUMOS_CALC_INPUT_TRACE='1', PYTHONIOENCODING='utf8', PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
    rows=corpus()
    if a.only:
        rows=[r for r in rows if a.only in r['id']]
    (a.out/'corpus.json').write_text(json.dumps(rows, indent=2), encoding='utf8')

    def run(row):
        name=row['id']; folder=a.out/name; folder.mkdir(exist_ok=True)
        fs=folder/'fs'; fs.mkdir(exist_ok=True)
        body='wait 200\nopen_app '+row['app']+'\nwait 30\n'
        if row['app']=='Equations':
            body+=keys('ENTER ENTER')
        body+=row['body']
        shot=lambda suffix:'wait 3\nscreenshot '+(folder/(suffix+'.ppm')).as_posix()+'\n'
        if row['app']=='Calculation':
            body+='assert_calc_input structure\nassert_calc_input dump\n'
            # The legacy scalar completeness probe deliberately refuses typed
            # units/measurements. Their real quantity evaluator is the gate.
            if row['group'] not in ('J','K'):
                body+='assert_calc_input '+('complete' if row['complete'] else 'incomplete')+'\n'
        body+=shot('input')
        if row['group'] in ('L','M','N'):
            body+=keys('LEFT')+shot('cursor-left')+keys('RIGHT')+shot('cursor-return')
        if row['evaluate'] and row['app']=='Calculation':
            body+=keys('ENTER')+'assert_calc_input dump\n'+shot('result')
            body+='assert_calc_status '+('ok' if row['complete'] else 'parse_error')+'\n'
        body+='log PARITY_PRODUCT_DONE\n'
        path=folder/'repro.numos'; path.write_text(body, encoding='utf8')
        frames=sum(int(l.split()[1])+1 if l.startswith('wait ') else 1 for l in body.splitlines())+100
        command=[str(a.bin.resolve()), '--headless','--deterministic','--quiet','--frames',str(frames),'--script',str(path),'--fs-sandbox-dir',str(fs)]
        try:
            result=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,timeout=120)
            code=result.returncode
            log=(result.stdout+result.stderr).decode('utf8',errors='replace')
        except subprocess.TimeoutExpired as error:
            code=-999
            log=((error.stdout or b'')+(error.stderr or b'')).decode('utf8',errors='replace')+'\nRUNNER_TIMEOUT_120_SECONDS\n'
        (folder/'run.log').write_text(log,encoding='utf8')
        semantic=[s for s in log.splitlines() if s.startswith(('[CALC-INPUT]','[CALC-EVAL]','[RESULT-EXACT]','[QUANTITY]','[UNCERTAINTY]'))]
        record=dict(id=name,group=row['group'],passed=code==0 and 'PARITY_PRODUCT_DONE' in log,exit=code,semantic=semantic,images=[])
        for ppm in folder.glob('*.ppm'):
            im=Image.open(ppm); assert im.size==(320,240),str(ppm)
            png=ppm.with_suffix('.png'); im.save(png)
            record['images'].append(str(png.relative_to(a.out)).replace('\\','/'))
        if a.compare:
            prior=json.loads((a.compare/'results.json').read_text(encoding='utf8'))
            old=next(x for x in prior if x['id']==name)
            record['semanticUnchanged']=old['semantic']==semantic
            record['passed'] &= record['semanticUnchanged']
        print('PASS' if record['passed'] else 'FAIL',name,flush=True)
        return record

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        results=list(pool.map(run,rows))
    (a.out/'results.json').write_text(json.dumps(results,indent=2),encoding='utf8')
    (a.out/'identity.json').write_text(json.dumps({'phase':a.phase,'binary':str(a.bin.resolve()),'sha256':hashlib.sha256(a.bin.read_bytes()).hexdigest(),'cases':len(results),'passed':sum(r['passed'] for r in results),'fontFilesIncluded':False},indent=2),encoding='utf8')
    cards=[]
    for row in results:
        cards.append('<section><h2>'+html.escape(row['id'])+'</h2><a href="'+row['id']+'/repro.numos">Reproducer</a>'+''.join('<figure><img width="320" height="240" src="'+html.escape(i)+'"><figcaption>'+html.escape(i)+'</figcaption></figure>' for i in row['images'])+'</section>')
    (a.out/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>NumOS parity '+a.phase+'</title><style>body{font:16px system-ui;background:#eef2f7;color:#182337}section{background:white;margin:20px;padding:16px}figure{display:inline-block;margin:8px}img{image-rendering:pixelated;border:1px solid #b9c0cc}figcaption{max-width:320px;font-size:12px}</style><h1>Native product corpus — '+a.phase+'</h1><p>Original 320×240 screenshots. Mathematical geometry is reviewed separately against the independent oracle.</p>'+''.join(cards),encoding='utf8')
    return 0 if all(r['passed'] for r in results) else 1


if __name__=='__main__':
    raise SystemExit(main())
