#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run structured notation/glyph tests with a matching pinned native build.

No solver strings are parsed by this harness. --phase also runs the existing
MathEnginePhaseRegression against exactly the same MathAST object files.
"""
import argparse,json,os,subprocess,hashlib
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',default='g++')
    p.add_argument('--phase',action='store_true')
    a=p.parse_args();source=a.source.resolve();build=a.build.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    names=['MathAST.o','CalculationEngine.o','MathGlyphAssembly.o','MathTypography.o',
           'MathRenderVisualCases.o','CursorController.o']
    objects=[path for path in (build/'src').rglob('*.o') if path.name in names or 'fonts' in path.parts or 'math' in path.parts or path.name=='FileSystem.o']
    assert all(any(p.name==n for p in objects) for n in names),names
    lvgl=list(build.rglob('liblvgl.a'));assert len(lvgl)==1
    libraries=[*lvgl,*build.rglob('libgiac.a'),*build.rglob('libtommath.a')]
    flags=['-std=gnu++17','-ffunction-sections','-fdata-sections','-DNATIVE_SIM','-DLV_CONF_INCLUDE_SIMPLE',
           '-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB','-I.','-Isrc','-I.pio/libdeps/emulator_pc/lvgl']
    records=[]
    env=dict(os.environ);env['PATH']='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+env.get('PATH','')
    def run(name,command):
        r=subprocess.run(command,cwd=source,env=env,capture_output=True)
        (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
        records.append(dict(name=name,command=command,exit=r.returncode));(out/'commands.json').write_text(json.dumps(records,indent=2))
        r.check_returncode()
    cases=[('notation','tests/host/math_notation_main.cpp')]
    # Same host-owned settings flag as the ordinary Giac harness; no math stub.
    settings=out/'settings.cpp';settings.write_text('bool setting_complex_enabled = true;\n')
    settings_obj=out/'settings.o';run('settings-compile',[a.compiler,*flags,'-c',str(settings),'-o',str(settings_obj)])
    objects.append(settings_obj)
    stress_obj=out/'MathStressExpressions.o'
    run('stress-compile',[a.compiler,*flags,'-c','src/math/MathStressExpressions.cpp','-o',str(stress_obj)])
    objects.append(stress_obj)
    if a.phase:cases.append(('phase','tests/MathEnginePhaseRegression.cpp'))
    for name,unit in cases:
        obj=out/(name+'.o');run(name+'-compile',[a.compiler,*flags,'-c',unit,'-o',str(obj)])
        rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+p.as_posix()+'"' for p in [obj,*objects,*libraries]))
        binary=out/(name+'.exe');run(name+'-link',[a.compiler,'@'+str(rsp),'-Wl,--gc-sections','-o',str(binary)])
        run(name,[str(binary)]);print(name,'PASS',flush=True)
    (out/'objects.json').write_text(json.dumps([dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in objects],indent=2))

if __name__=='__main__':main()
