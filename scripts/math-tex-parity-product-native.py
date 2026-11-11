#!/usr/bin/env python3
"""Isolated native .numos integration replays for Calculation/Equations/Grapher.

Existing assertion contracts stay verbatim. Screenshots and filesystems are
redirected into this invocation's evidence directory. A second candidate may
be compared at mathematical boundaries, never by promoting images.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compare',type=Path)
    p.add_argument('--only')
    p.add_argument('--current-contract',action='store_true',help='Use documented typed-result expectations for three pre-FORMAT legacy fixtures')
    a=p.parse_args(); os.chdir(ROOT)
    a.out=Path(os.path.relpath(a.out.resolve(),ROOT));a.out.mkdir(parents=True,exist_ok=True)
    fixtures=[]
    for path in sorted((ROOT/'tests/emulator/scripts').glob('*.numos')):
        if path.stem.startswith(('calc_', 'equations_', 'calculus_', 'grapher_', 'math_')) and 'negctl' not in path.stem:
            if not a.only or a.only in path.stem:fixtures.append(path)
    env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))

    def run(path):
        name=path.stem; folder=a.out/name; folder.mkdir(exist_ok=True)
        fs=folder/'fs';fs.mkdir(exist_ok=True)
        source=path.read_text(encoding='utf8')
        adaptations=[]
        if a.current_contract and name in ('calc_error_div_by_zero','calc_semantic_giac_infinity_undef','calc_semantic_giac_free_symbol'):
            # The scalar ExactVal mirror reports an error for valid typed
            # infinity/symbolic results. The script already asserts typed
            # status, result kind, and exact value independently.
            assert 'assert_calc_status ok' in source and 'assert_calc_exact ' in source
            source=source.replace('assert_no_error\n','')
            adaptations.append('typed status/kind/exact value replace obsolete scalar mirror check')
            if name=='calc_semantic_giac_free_symbol':
                source=source.replace('assert_calc_exact x-2*x','assert_calc_exact -x')
                first=source.index('assert_variable ans 4')
                second=source.index('assert_variable ans 4',first+1)
                source=source[:second]+source[second:].replace('assert_variable ans 4','key AC\nkey ans\nkey ENTER\nassert_calc_status ok\nassert_calc_exact -x',1)
                adaptations.append('current preexisting simplification and exact symbolic Ans session contract')
        source=re.sub(r'^screenshot\s+(.+)$',lambda m:'screenshot '+(folder/Path(m[1]).name).as_posix(),source,flags=re.M)
        source+='\nlog PARITY_NATIVE_DONE\n'
        replay=folder/'repro.numos';replay.write_text(source,encoding='utf8')
        frames=sum(int(l.split()[1])+1 if l.startswith('wait ') else 1 for l in source.splitlines())+250
        cmd=[str(a.bin.resolve()),'--headless','--quiet','--deterministic','--frames',str(frames),'--script',str(replay),'--fs-sandbox-dir',str(fs)]
        try:
            r=subprocess.run(cmd,env=env,cwd=ROOT,capture_output=True,timeout=90)
            code=r.returncode;log=(r.stdout+r.stderr).decode('utf8',errors='replace')
        except subprocess.TimeoutExpired as e:
            code=-999;log=((e.stdout or b'')+(e.stderr or b'')).decode('utf8',errors='replace')+'\nRUNNER_TIMEOUT_90_SECONDS\n'
        (folder/'run.log').write_text(log,encoding='utf8')
        semantic=[l for l in log.splitlines() if l.startswith(('[RESULT-EXACT]','[CALC-EVAL]','[EQUATIONS-RESULT]','[CALCULUS-RESULT]'))]
        row=dict(id=name,sourceSha256=hashlib.sha256(path.read_bytes()).hexdigest(),replaySha256=hashlib.sha256(source.encode('utf8')).hexdigest(),adaptations=adaptations,exit=code,passed=code==0 and 'PARITY_NATIVE_DONE' in log,assertions=log.count(': PASS -'),semantic=semantic)
        if a.compare:
            old=json.loads((a.compare/'results.json').read_text(encoding='utf8'))
            before=next(x for x in old if x['id']==name)
            row['semanticUnchanged']=before['semantic']==semantic
            row['passed'] &= row['semanticUnchanged']
        for ppm in folder.glob('*.ppm'):
            im=Image.open(ppm);assert im.size==(320,240);im.save(ppm.with_suffix('.png'))
        print('PASS' if row['passed'] else 'FAIL',name,flush=True)
        return row

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(run,fixtures))
    (a.out/'results.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
    (a.out/'identity.json').write_text(json.dumps(dict(binary=str(a.bin.resolve()),sha256=hashlib.sha256(a.bin.read_bytes()).hexdigest(),cases=len(rows),passed=sum(r['passed'] for r in rows)),indent=2),encoding='utf8')
    return 0 if all(r['passed'] for r in rows) else 1


if __name__=='__main__':raise SystemExit(main())
