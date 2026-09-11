#!/usr/bin/env python3
"""Build the production tutor boundary against the exact cached Giac host closure."""
import argparse, pathlib, subprocess, sys, os, shutil
root=pathlib.Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--cache',default='out/tutor-engine-01/giac-cache/obj');args=ap.parse_args()
out=root/'out/tutor-engine-01/host';out.mkdir(parents=True,exist_ok=True)
cache=root/args.cache
if not (cache/'kgen.cc.o').exists():
    # Fresh checkouts build the authoritative closure, rather than relying on
    # any developer's previous Equations investigation artifacts.
    bash='C:/Program Files/Git/bin/bash.exe' if os.name=='nt' else shutil.which('bash')
    if not bash: raise SystemExit('Bash is required by the existing Giac host build.')
    env=dict(os.environ,GIAC_HOST_OUT=os.path.relpath(cache.parent,root).replace('\\','/'))
    subprocess.run([bash,'scripts/build-giac-host-harness.sh','--build-only'],cwd=root,env=env,check=True)
common=['-std=gnu++17','-O1','-Wall','-Wextra','-Wno-unused-parameter','-ffunction-sections','-fdata-sections','-D_USE_MATH_DEFINES','-DNUMOS_GIAC_HOST_HARNESS=1','-Isrc']
defs=['-D'+d for d in 'HAVE_CONFIG_H IN_GIAC GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED USE_GMP_REPLACEMENTS UMAP DOUBLEVAL'.split()]
extra=defs+['-Ilib/giac','-Ilib/giac/src','-Ilib/libtommath','-Wno-deprecated-declarations']
link=['-Wl,--gc-sections']
if os.name=='nt': extra+=['-D__MINGW_H','-fpermissive'];link+=['-static','-lpsapi']
else:
    extra+=['-include','unistd.h']
    if pathlib.Path('/usr/include/libintl.h').exists(): extra+=['-include','libintl.h']
for source,flags in [('src/math/giac/GiacEngine.cpp',extra),('tests/host/tutor_engine_main.cpp',[])]:
    subprocess.run(['g++',*common,*flags,'-fstack-usage','-c',source,'-o',str((out/(pathlib.Path(source).name+'.o')).relative_to(root))],cwd=root,check=True)
# Recompile the single vendored portability fix with the exact embedded macro
# profile; other vendor objects must be current (never reuse a stale source).
source='lib/giac/src/kgen.cc'
subprocess.run(['g++','-std=gnu++17','-O1','-fexceptions','-ffunction-sections','-fdata-sections','-D_USE_MATH_DEFINES',*extra,'-w','-c',source,'-o',str((out/'kgen.cc.o').relative_to(root))],cwd=root,check=True)
objects=[os.path.relpath(p,root) for p in cache.glob('*.o') if not p.name.endswith('_main.cpp.o') and p.name not in ('GiacEngine.cpp.o','kgen.cc.o')]
objects += [str(p.relative_to(root)) for p in out.glob('*.o')]
response=out/'link.rsp';response.write_text('\n'.join('"'+p.replace('\\','/')+'"' for p in objects),encoding='utf-8')
subprocess.run(['g++','@'+str(response.relative_to(root)),*link,'-o',str((out/'tutor_engine.exe').relative_to(root))],cwd=root,check=True)
print(out/'tutor_engine.exe')
