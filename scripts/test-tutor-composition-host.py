#!/usr/bin/env python3
"""Link focused checks to a matching native snapshot, preserving the Giac ABI.

Build emulator_pc first. No vendor or engine object is recompiled with a second
macro/profile: in particular, strict c++17 and gnu++17 differ in WIN32 on MinGW,
which affects the pinned Giac ref_count_t layout. Outputs stay outside source.
"""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, shlex

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,required=True)
p.add_argument('--build',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--compiler',default='g++')
p.add_argument('--compile-commands',type=Path,help='Pinned native database when other targets share the source directory')
p.add_argument('--tests',nargs='*',default=['tutor_composition_checks','tutor_composition_budget_failure','tutor_nonlinear_checks',
    'tutor_transcendental_checks','tutor_trig_checks','periodic_results_checks',
    'tutor_i18n_checks','tutor_teaching_math','tutor_conclusion_presentation',
    'math_notation_main','tutor_nonlinear_allocation','periodic_results_allocation',
    'tutor_engine_main','tutor_i18n_main','periodic_result_main'])
a=p.parse_args();source=a.source.resolve();build=a.build.resolve();out=a.out.resolve()
out.mkdir(parents=True,exist_ok=True)
objects=[p for p in (build/'src').rglob('*.o') if 'math' in p.parts or 'fonts' in p.parts or p.name in ['FileSystem.o','MathTypography.o']]
libraries=[*build.rglob('liblvgl.a'),*build.rglob('libgiac.a'),*build.rglob('libtommath.a')]
assert objects and len(libraries)==3,'Build matching emulator_pc first'
database=json.loads((a.compile_commands or source/'compile_commands.json').read_text())
native=next(x for x in database if x['file'].endswith('EquationsApp.cpp') and 'NATIVE_SIM' in x['command'])
# WHY: an abandoned discovery build may leave a different LVGL checkout in
# .pio/libdeps. Headers must match the linked library, including glyph ABI.
nativeFlags=shlex.split(native['command'].replace('\\','/'))
lvgl=next(Path(x[2:]) for x in nativeFlags if x.startswith('-I') and x.rstrip('/').endswith('/lvgl'))
if not lvgl.is_absolute():lvgl=source/lvgl
includes=[source/'src',source,lvgl]
if os.name=='nt':includes.append(Path('C:/SDL2/x86_64-w64-mingw32/include'))
flags=['-std=c++17','-O1','-ffunction-sections','-fdata-sections','-DNATIVE_SIM','-DSDL_MAIN_HANDLED',
       '-DLV_CONF_INCLUDE_SIMPLE','-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB',*['-I'+str(p) for p in includes]]
env=dict(os.environ)
if os.name=='nt':env['PATH']='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+env.get('PATH','')
records=[]
def run(name,command):
    r=subprocess.run(command,cwd=source,env=env,capture_output=True)
    (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
    records.append(dict(name=name,command=command,cwd=str(source),exit=r.returncode))
    (out/'commands.json').write_text(json.dumps(records,indent=2))
    r.check_returncode()
for name in a.tests:
    # Ordinary Giac suites use their established diagnostics-only engine build
    # (scripts/build-giac-host-harness.sh), not this native-object runner.
    obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
    run(name+'-compile',[a.compiler,*flags,'-c','tests/host/'+name+'.cpp','-o',str(obj)])
    testObjects=objects
    if name in ('toolbox_visual_checks','math_brackets_checks','scientific_layout_checks'):
        testObjects=[*objects,build/'src/ui/MathRenderer.o']
    if name=='math_spacing_checks':
        testObjects=[*objects,build/'src/ui/MathRenderer.o']
    if name in ('calculation_input_checks', 'toolbox_checks'):
        # Exercise the existing node allocator boundary as well as C++ vectors;
        # this instrumented object is never linked into a product image.
        allocator=out/'MathAST-test-allocator.o'
        run(name+'-allocator',[a.compiler,*flags,'-DNUMOS_MATH_AST_TEST_ALLOCATOR',
            '-c','src/math/MathAST.cpp','-o',str(allocator)])
        testObjects=[allocator if p.name=='MathAST.o' else p for p in objects]
    extra=[]
    if 'bool setting_complex_enabled' not in (source/'tests/host'/str(name+'.cpp')).read_text(encoding='utf8'):
        stub=out/'settings.cpp';stub.write_text('bool setting_complex_enabled=false;\n')
        run(name+'-settings',[a.compiler,*flags,'-c',str(stub),'-o',str(out/'settings.o')]);extra=[out/'settings.o']
    rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+p.as_posix()+'"' for p in [obj,*extra,*testObjects,*libraries]))
    run(name+'-link',[a.compiler,'@'+str(rsp),'-Wl,--gc-sections','-o',str(binary)])
    if name not in ['tutor_engine_main','tutor_i18n_main','periodic_result_main','tutor_teaching_math']:
        run(name,[str(binary)]);print(name,'PASS',flush=True)
(out/'objects.json').write_text(json.dumps([dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in [*objects,*libraries]],indent=2))
