#!/usr/bin/env python3
"""Run existing private layout/draw instrumentation with frozen-source headers.

The source snapshot need not copy PlatformIO dependencies: the unchanged LVGL
include directory is resolved explicitly from the corresponding original root.
No product object, published executable or source file is modified.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import statistics
import re

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for flag in ('source','build','compile-commands','out'):
    p.add_argument('--'+flag,type=Path,required=True)
p.add_argument('--dependency-root',type=Path,default=ROOT)
p.add_argument('--prepare-only',action='store_true',help='Compile private instrumentation without executing any timed workloads')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
db=json.loads(a.compile_commands.read_text(encoding='utf8'))
dependency=(a.dependency_root.resolve()/'.pio/libdeps/emulator_pc/lvgl').as_posix()
assert Path(dependency).is_dir(),dependency
for entry in db:
    entry['directory']=str(a.source.resolve())
    # Longest first: these are include tokens, not arbitrary substrings of a
    # source command. Quoting keeps the user's Unicode/spaced workspace intact.
    for suffix in ('\\src',''):
        entry['command']=entry['command'].replace('-I.pio\\libdeps\\emulator_pc\\lvgl'+suffix,
                                                  '"-I'+dependency+suffix.replace('\\','/')+'"')
private_db=a.out/'compile-commands.json';private_db.write_text(json.dumps(db,indent=2),encoding='utf8')
source_files=['src/math/MathAST.cpp','src/math/MathAST.h','src/ui/MathRenderer.cpp','src/ui/MathRenderer.h','src/apps/CalculationApp.cpp','src/hal/NativeHal.cpp']
source_files += [f for f in ('src/math/MathVectorDelimiterGeometry.h','src/math/MathTypography.h','src/math/MathRowLayout.h') if (a.source/f).is_file()]
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
identity=dict(source=str(a.source.resolve()),build=str(a.build.resolve()),sources={f:sha(a.source/f) for f in source_files},
              baselineBinary=sha(a.build/'program.exe'),instrumentation=sha(ROOT/'scripts/measure-calculation-renderer.py'))
(a.out/'identity.json').write_text(json.dumps(identity,indent=2),encoding='utf8')
instrumenter=(ROOT/'scripts/measure-calculation-renderer.py').read_text(encoding='utf8')
replay_anchor="    run(name,[out/'profile.exe','--headless','--deterministic','--quiet','--frames',frames,'--script',path])"
assert instrumenter.count(replay_anchor)==1
instrumenter=instrumenter.replace(replay_anchor,"    sandbox=out/('fs-'+name);sandbox.mkdir(exist_ok=True)\n"+replay_anchor.replace("'--script',path]","'--script',path,'--fs-sandbox-dir',sandbox]"))
supplement=ROOT/'scripts/math-tex-parity-product-profile-workloads.py'
identity['supplementWorkloads']=sha(supplement)
(a.out/'identity.json').write_text(json.dumps(identity,indent=2),encoding='utf8')
# The real Tutor lives in Equations. Private log markers allow timing its
# existing actions without routing a Calculation-only assertion to that app.
anchor="units={'CalculationApp.cpp':app,'MathAST.cpp':ast,'MathRenderer.cpp':renderer}"
assert instrumenter.count(anchor)==1
native_probe = '''
native=(source/'src/hal/NativeHal.cpp').read_text(encoding='utf-8')
marker='        case ScriptCmdType::Log:'
assert native.count(marker)==1
native=native.replace(marker,marker+"\\n            if (sc.strArg == \\\"product-profile-begin\\\") { calc_profile::begin(); break; }\\n            if (sc.strArg.compare(0,23,\\\"product-profile-report \\\")==0) { calc_profile::report(sc.strArg.c_str()+23); break; }")
units['NativeHal.cpp']=native
'''
instrumenter=instrumenter.replace(anchor,anchor+native_probe)
anchor='# Same active app: its 50-entry history stays owned throughout replacement.'
assert instrumenter.count(anchor)==1
instrumenter=instrumenter.replace(anchor,'exec(Path('+repr(str(supplement.resolve()))+').read_text(encoding="utf8"),globals())\n\n'+anchor)
if 'calculateLayoutChecked(' in (a.source/'src/math/MathAST.cpp').read_text(encoding='utf8'):
    # Preserve original method timings while observing the new checked boundary
    # separately. The extra scope exists only in this private executable.
    anchor='} counters;';assert instrumenter.count(anchor)==1
    instrumenter=instrumenter.replace(anchor,' unsigned long long checkedCalls=0,checkedNs=0,checkedLayoutNs=0;\n'+anchor)
    anchor='struct DrawScope {';assert instrumenter.count(anchor)==1
    instrumenter=instrumenter.replace(anchor,'''struct CheckedScope {
 unsigned long long start=now(),layoutStart=counters.layoutNs;
 CheckedScope(){if(active)++counters.checkedCalls;}
 ~CheckedScope(){if(active){counters.checkedNs+=now()-start;counters.checkedLayoutNs+=counters.layoutNs-layoutStart;}}
};
'''+anchor)
    anchor=' active=false;const auto& c=counters;';assert instrumenter.count(anchor)==1
    instrumenter=instrumenter.replace(anchor,anchor+'\n std::printf("[CHECKED-COST] %s calls=%llu total_ns=%llu guard_ns=%llu\\n",label,c.checkedCalls,c.checkedNs,c.checkedNs-c.checkedLayoutNs);')
    anchor='assert count>=24,count';assert instrumenter.count(anchor)==1
    instrumenter=instrumenter.replace(anchor,anchor+'''\ncheckedAnchor='LayoutStatus calculateLayoutChecked(MathNode& root, const FontMetrics& fm) {'
assert ast.count(checkedAnchor)==1
ast=ast.replace(checkedAnchor,checkedAnchor+'\\n    calc_profile::CheckedScope measuredChecked;')''')
private_runner=a.out/'measure-with-checked-layout.py';private_runner.write_text(instrumenter,encoding='utf8')
command=[sys.executable,str(private_runner.resolve()),'--source',str(a.source.resolve()),
         '--build',str(a.build.resolve()),'--compile-commands',str(private_db.resolve()),'--out',str(a.out.resolve())]
if not a.prepare_only:command.append('--run')
with (a.out/'runner.log').open('wb') as log:
    result=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
(a.out/'result.json').write_text(json.dumps(dict(command=command,exit=result.returncode,passed=result.returncode==0,prepareOnly=a.prepare_only),indent=2),encoding='utf8')
if result.returncode==0 and not a.prepare_only:
    rows=json.loads((a.out/'cost.json').read_text(encoding='utf8'))['records']
    groups={}
    for row in rows:groups.setdefault(row['id'].rsplit('-',1)[0],[]).append(row)
    summary={key:{metric:statistics.median(r[metric] for r in records) for metric in records[0] if metric!='id'} for key,records in groups.items()}
    (a.out/'medians.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
    supplemental=json.loads((a.out/'cost-supplement.json').read_text(encoding='utf8'))['records']
    supplemental_groups={}
    for row in supplemental:supplemental_groups.setdefault(row['id'].rsplit('-',1)[0],[]).append(row)
    supplemental_medians={key:{metric:statistics.median(r[metric] for r in records) for metric in records[0] if metric!='id'} for key,records in supplemental_groups.items()}
    (a.out/'supplement-medians.json').write_text(json.dumps(supplemental_medians,indent=2),encoding='utf8')
    checked=[]
    for line in ((a.out/'cost.log').read_text(encoding='utf8',errors='replace')+'\n'+(a.out/'cost-supplement.log').read_text(encoding='utf8',errors='replace')).splitlines():
        if line.startswith('[CHECKED-COST] '):
            parts=line.split();checked.append(dict(id=parts[1],**{k:int(v) for k,v in (p.split('=') for p in parts[2:])}))
    (a.out/'checked-cost.json').write_text(json.dumps(dict(scope='Private checked-layout boundary; total includes recursive layout; guard_ns subtracts the separately counted recursive layout. Original COST timings remain unchanged in definition.',records=checked),indent=2),encoding='utf8')
    assert identity['sources']=={f:sha(a.source/f) for f in source_files},'source changed during private profile; repeat on stable sources'
print('PASS' if result.returncode==0 else 'FAIL',str(a.out))
raise SystemExit(result.returncode)
