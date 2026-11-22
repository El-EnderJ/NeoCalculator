#!/usr/bin/env python3
"""Seeded real-key editing stress with structural and navigation invariants.

No evaluator is invoked on generated/incomplete trees. Baseline/final compare
authored structure and cursor slot/index independently of measured pixel boxes.
"""
import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random
import re
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('product_corpus',ROOT/'scripts/math-tex-parity-product-corpus.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--seed',type=int,default=20261009)
    p.add_argument('--compare',type=Path)
    p.add_argument('--cases',type=int,default=96)
    a=p.parse_args();os.chdir(ROOT);a.out=Path(os.path.relpath(a.out.resolve(),ROOT));a.out.mkdir(parents=True,exist_ok=True)
    rng=random.Random(a.seed)
    templates=[('fraction','/'),('power','^'),('root','SQRT'),('paren','('),('bracket','['),('function','sin'),('log-base','LOGBASE')]
    templates+=[('indexed',c.pick('Indexed root',111)),('measurement',c.pick('measurement',750)),('call',c.pick('Derivative',171)),('integral',c.pick('Definite integral',170))]
    fixtures=[]
    for i in range(a.cases):
        name,template=templates[i%len(templates)]
        # Every third case starts as an empty template; the others exercise
        # captured operands and nesting within the existing supported limits.
        prefix='' if i%3==0 else c.keys(str(rng.randint(1,9)))
        body=prefix+(template if '\n' in template else c.keys(template))
        edits=[]
        for step in range(12):
            if step%4==0:token=str(rng.randint(1,9))
            elif step%4==1:token=rng.choice(['LEFT','RIGHT','UP','DOWN'])
            elif step%4==2:token=rng.choice(['DEL','LEFT','RIGHT'])
            else:token=rng.choice(['ADD','NEGATE','SQRT','POW','FRAC'])
            edits.append(token)
        fixtures.append(dict(id=f'{i:03}-{name}',template=name,body=body,edits=edits))
    (a.out/'fixtures.json').write_text(json.dumps(fixtures,indent=2),encoding='utf8')
    env=dict(os.environ,NUMOS_CALC_INPUT_TRACE='1',PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))

    def semantic_blocks(log):
        blocks=[]
        for match in re.finditer(r'(\[CALC-INPUT\].*?)(?=\[ASSERT\])',log,re.S):
            block=match[1]
            block=re.sub(r'\s+\[-?\d+x-?\d+ a-?\d+/d-?\d+\]','',block)
            blocks.append('\n'.join(l.rstrip() for l in block.splitlines() if l.strip()))
        return blocks

    def run(f):
        folder=a.out/f['id'];folder.mkdir(exist_ok=True);fs=folder/'fs';fs.mkdir(exist_ok=True)
        body='wait 200\nopen_app Calculation\nwait 30\n'+f['body']+'assert_calc_input structure\nassert_calc_input dump\n'
        for step,token in enumerate(f['edits']):
            body+=c.keys(token)+'assert_calc_input structure\nassert_calc_input dump\n'
            if step in (3,7,11):body+='wait 3\nscreenshot '+(folder/f'edit-{step}.ppm').as_posix()+'\n'
        body+='log PARITY_EDITING_DONE\n';path=folder/'repro.numos';path.write_text(body,encoding='utf8')
        cmd=[str(a.bin.resolve()),'--headless','--deterministic','--quiet','--frames','650','--script',str(path),'--fs-sandbox-dir',str(fs)]
        r=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,timeout=90)
        log=(r.stdout+r.stderr).decode('utf8',errors='replace');(folder/'run.log').write_text(log,encoding='utf8')
        states=semantic_blocks(log)
        record=dict(id=f['id'],passed=r.returncode==0 and 'PARITY_EDITING_DONE' in log and len(states)==13,exit=r.returncode,states=states)
        if a.compare:
            old=json.loads((a.compare/'results.json').read_text(encoding='utf8'))
            prior=next(x for x in old if x['id']==f['id'])
            record['structureAndCursorUnchanged']=prior['states']==states
            record['passed'] &= record['structureAndCursorUnchanged']
        for path in folder.glob('*.ppm'):
            im=Image.open(path);assert im.size==(320,240);im.save(path.with_suffix('.png'))
        print('PASS' if record['passed'] else 'FAIL',f['id'],flush=True)
        return record

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(run,fixtures))
    (a.out/'results.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
    (a.out/'summary.json').write_text(json.dumps(dict(seed=a.seed,cases=len(rows),passed=sum(r['passed'] for r in rows),states=sum(len(r['states']) for r in rows),binary=str(a.bin.resolve()),sha256=hashlib.sha256(a.bin.read_bytes()).hexdigest(),scope='editing only; no evaluation of random incomplete ASTs'),indent=2),encoding='utf8')
    return 0 if all(r['passed'] for r in rows) else 1


if __name__=='__main__':raise SystemExit(main())
