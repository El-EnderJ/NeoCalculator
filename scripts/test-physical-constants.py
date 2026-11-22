#!/usr/bin/env python3
"""Offline independent data contracts, PSD validation, compiled table mutations."""
import argparse
from decimal import Decimal
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/scientific-quantities-01/B-data'

def eigenvalues(matrix):
    # Jacobi diagonalization independent of production variance accumulation.
    a=[row[:] for row in matrix]
    n=len(a)
    for unused in range(20000):
        p,q=max(itertools.combinations(range(n),2),key=lambda ij:abs(a[ij[0]][ij[1]]))
        if abs(a[p][q])<1e-15:
            return sorted(a[i][i] for i in range(n))
        angle=.5*math.atan2(2*a[p][q],a[q][q]-a[p][p])
        c,s=math.cos(angle),math.sin(angle)
        pp,qq,pq=a[p][p],a[q][q],a[p][q]
        for k in range(n):
            if k in (p,q):continue
            kp,kq=a[k][p],a[k][q]
            a[k][p]=a[p][k]=c*kp-s*kq
            a[k][q]=a[q][k]=s*kp+c*kq
        a[p][p]=c*c*pp-2*c*s*pq+s*s*qq
        a[q][q]=s*s*pp+2*c*s*pq+c*c*qq
        a[p][q]=a[q][p]=0
    raise AssertionError('PSD oracle did not converge')

def validate(dataset):
    constants=dataset['constants']
    assert dataset['dataset']=='CODATA 2022' and dataset['nist_web_version']=='9.0'
    assert dataset['dataset_version_id']==1
    assert len(constants)==15
    for field in ('constant_id','reference_id','symbol','key'):
        assert len({c[field] for c in constants})==len(constants),field
    # Independent SI unit dimensions, distinct from production UnitRegistry.
    dimensions={'m s^-1':[1,0,-1,0,0,0,0],'m':[1,0,0,0,0,0,0],
                'kg':[0,1,0,0,0,0,0],'J':[2,1,-2,0,0,0,0],
                's':[0,0,1,0,0,0,0],'J s':[2,1,-1,0,0,0,0],
                'N':[1,1,-2,0,0,0,0],'K':[0,0,0,0,1,0,0],
                'm s^-2':[1,0,-2,0,0,0,0],'C':[0,0,1,1,0,0,0],
                'm^3 kg^-1 s^-2':[3,-1,-2,0,0,0,0]}
    for c in constants:
        assert c['dimension']==dimensions[c['unit']]
        u=Decimal(c['standard_uncertainty'])
        assert u.is_finite() and (u==0 if c['exact'] else u>0)
        assert c['source_url'].startswith('https://physics.nist.gov/')
        for field in ('name_en','name_es','symbol','note'):
            assert '?' not in c[field] and '\ufffd' not in c[field],(field,c[field])
    ids=[c['constant_id'] for c in constants if not c['exact']]
    matrix=[[1 if i==j else None for j in ids] for i in ids]
    pairs=set()
    for p in dataset['correlations']:
        a,b,rho=p['a'],p['b'],float(p['rho'])
        assert a<b and (a,b) not in pairs
        assert math.isfinite(rho) and -1<=rho<=1
        assert p['basis'] in ('published','derived_from_G')
        pairs.add((a,b));i,j=ids.index(a),ids.index(b)
        matrix[i][j]=matrix[j][i]=rho
    assert len(pairs)==55 and all(x is not None for row in matrix for x in row)
    values=eigenvalues(matrix)
    assert min(values)>=-1e-12,values
    return values

def run(command, expected=0):
    result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf8',errors='replace')
    if expected==0 and result.returncode:
        raise AssertionError(f"command failed: {command}\n{result.stdout}\n{result.stderr}")
    if expected!=0 and not result.returncode:
        raise AssertionError(f"mutation survived: {command}")
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx',default=os.environ.get('CXX') or shutil.which('g++'))
    parser.add_argument('--no-mutations',action='store_true')
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    run([sys.executable,str(ROOT/'scripts/generate-physical-constants.py'),'--check'])
    data=json.loads((ROOT/'src/math/scientific/physical_constants.json').read_text(encoding='utf8'))
    values=validate(data)
    source=json.loads((ROOT/'data/scientific/codata2022/source_capture.json').read_text(encoding='utf8'))
    oracle=json.loads((ROOT/'data/scientific/codata2022/independent_oracle.json').read_text(encoding='utf8'))
    assert oracle['independently_queried_reversed_pair']['rho']=='-0.99999'
    assert len(source['constants'])==15 and len(source['correlations'])==36
    assert data['source_capture_sha256']==hashlib.sha256((ROOT/'data/scientific/codata2022/source_capture.json').read_bytes()).hexdigest()
    if not args.cxx:raise SystemExit('C++17 compiler required (pass --cxx)')
    cpp=ROOT/'src/math/scientific/PhysicalConstants.cpp'
    text=cpp.read_text(encoding='utf8')
    suffix='.exe' if os.name=='nt' else ''
    executable=OUT/('physical_constants_contract'+suffix)
    # MinGW's linker uses narrow path arguments; relative repository paths
    # preserve reproducibility when the Windows account has non-ASCII letters.
    relative=lambda path:os.path.relpath(path,ROOT)
    base=[args.cxx,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',
          '-Isrc','-Isrc/math/scientific','tests/physical_constants_contract.cpp']
    run(base+[relative(cpp),'-o',relative(executable)])
    result=run([str(executable)])
    mutations={
        'rho_zero':('{2,3,-99999}','{2,3,0}'),
        'rho_wrong_sign':('{2,3,-99999}','{2,3,99999}'),
        'rho_outside_range':('{2,3,-99999}','{2,3,-100001}'),
        'measured_is_exact':('1.5e-15,false,"G"','1.5e-15,true,"G"'),
        'exact_has_uncertainty':('},0,true,"c"','},1,true,"c"'),
        'reference_identity_swapped':('ConstantId::BohrRadius,2,','ConstantId::BohrRadius,3,'),
        'unknown_is_zero':('if (!a || !b || a->exact || b->exact) return false;',
                           'if (!a || !b || a->exact || b->exact) { rho=0; return true; }'),
    }
    killed=[]
    if not args.no_mutations:
        for name,(before,after) in mutations.items():
            assert text.count(before)==1,(name,'mutation match must be unique')
            mutated=OUT/(name+'.cpp');mutated.write_text(text.replace(before,after),encoding='utf8',newline='\n')
            mutant=OUT/(name+suffix)
            run(base+[relative(mutated),'-o',relative(mutant)])
            failure=run([str(mutant)],expected=1)
            (OUT/(name+'.log')).write_text(failure.stdout+failure.stderr,encoding='utf8')
            killed.append(name)
    report={'dataset':'CODATA 2022','constants':15,'published_correlation_pairs':36,
            'derived_correlation_pairs':19,'uncertain_matrix_order':11,'eigenvalues':values,
            'native_contract':result.stdout.strip(),'mutations_killed':killed,
            'offline':True,'compiler':args.cxx}
    (OUT/'data_validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
