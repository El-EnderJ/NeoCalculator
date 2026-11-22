#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Freeze and fully rebuild a local web candidate in a new ASCII workspace."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=ROOT);p.add_argument('--scratch',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--package-root',type=Path,help='New immutable staging source root; leaves a previous active browser package untouched');p.add_argument('--prior-package',type=Path,help='Explicit last package for refresh when an earlier run used an external staging root');p.add_argument('--emsdk',type=Path,default=Path('C:/.codex-cache/calculation-renderer-consolidation-01/tools/emsdk'));p.add_argument('--jobs',type=int,default=4);p.add_argument('--check-only',action='store_true');p.add_argument('--refresh',action='store_true',help='Archive prior local package, refresh changed frozen-source files, and reuse this private build cache');a=p.parse_args()
 source=a.source.resolve();scratch=a.scratch.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 published=Path('C:/.piobuild/numOS/emulator_pc').resolve();assert scratch!=published and scratch not in published.parents and published not in scratch.parents
 assert str(scratch).isascii(),'MinGW/Emscripten candidate workspace must be ASCII'
 spec=importlib.util.spec_from_file_location('closure',source/'scripts/emulator_sources.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 closure=module.emulator_sources(source);required={'stix_operators_18.c','stix_operators_12.c','stix_operators_8.c'};present={x.name for x in closure};assert required<=present,'Missing operator fonts in emulator_pc source allowlist: '+str(required-present)
 record=dict(source=str(source),scratch=str(scratch),operatorFonts=sorted(required),sourceClosure=[str(x.relative_to(source)) for x in closure],checkOnly=a.check_only)
 (out/'preflight.json').write_text(json.dumps(record,indent=2))
 if a.check_only:print('PASS native/web source closure includes all operator fonts');return
 frozen=scratch/'source';build=scratch/'build';sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest()
 if a.refresh:
  assert frozen.is_dir() and (build/'CMakeCache.txt').exists(),'Refresh requires this runner’s prior private workspace'
  prior_package=a.prior_package.resolve() if a.prior_package else Path(json.loads((scratch/'last-package.json').read_text())['webRoot']) if (scratch/'last-package.json').is_file() else frozen/'out/wasm/dist/release'
  archive=scratch/'archives'/time.strftime('%Y%m%d-%H%M%S')
  shutil.copytree(prior_package,archive/'package',copy_function=shutil.copy2)
  prior=[dict(path=x.relative_to(frozen).as_posix(),sha256=sha(x),bytes=x.stat().st_size) for folder in ('src','lib','wasm','scripts','.pio') for x in (frozen/folder).rglob('*') if x.is_file()]
  prior.append(dict(path='platformio.ini',sha256=sha(frozen/'platformio.ini'),bytes=(frozen/'platformio.ini').stat().st_size))
  (archive/'package-identity.json').write_text(json.dumps(dict(webRoot=str(prior_package),files={x.name:sha(x) for x in prior_package.iterdir() if x.is_file()}),indent=2))
  (archive/'source-manifest.json').write_text(json.dumps(prior,indent=2));record['priorArchive']=str(archive)
 else:
  assert not scratch.exists(),'Use a new scratch directory or explicit --refresh; prior evidence remains archived'
  scratch.mkdir(parents=True);frozen.mkdir()
 copies=[]
 changed=[]
 def copy(original,target):
  if not target.exists() or sha(original)!=sha(target):
   target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(original,target);changed.append(target.relative_to(frozen).as_posix())
 for folder in ('src','lib','wasm'):
  for original in (source/folder).rglob('*'):
   if original.is_file():copy(original,frozen/original.relative_to(source))
 for file in ('platformio.ini','scripts/emulator_sources.py'):
  copy(source/file,frozen/file)
 for original in (source/'.pio/libdeps/emulator_pc/lvgl').rglob('*'):
  if original.is_file():copy(original,frozen/original.relative_to(source))
 (out/'refreshed-files.json').write_text(json.dumps(changed,indent=2))
 paths=[frozen/'platformio.ini',*[x for folder in ('src','lib','wasm','scripts','.pio') for x in (frozen/folder).rglob('*')]]
 for path in paths:
  if path.is_file():
   relative=path.relative_to(frozen);digest=sha(path);assert digest==sha(source/relative),str(relative)
   copies.append(dict(path=relative.as_posix(),sha256=digest,bytes=path.stat().st_size))
 (out/'frozen-source.json').write_text(json.dumps(copies,indent=2))
 emsdk=a.emsdk.resolve();emroot=emsdk/'upstream/emscripten';assert (emroot/'emcmake.py').exists()
 env=dict(os.environ,PATH=str(emroot)+';C:/mingw64/bin;'+os.environ.get('PATH',''),EMSDK=str(emsdk),PYTHONIOENCODING='utf-8',GIT_DIR=str(ROOT/'.git'),GIT_WORK_TREE=str(ROOT))
 if (emsdk/'.emscripten').exists():env['EM_CONFIG']=str(emsdk/'.emscripten')
 commands=[]
 def run(name,cmd):
  started=time.perf_counter()
  with (out/(name+'.log')).open('wb') as log:r=subprocess.run(cmd,cwd=frozen,env=env,stdout=log,stderr=subprocess.STDOUT)
  commands.append(dict(name=name,command=cmd,cwd=str(frozen),exit=r.returncode,seconds=time.perf_counter()-started));(out/'commands.json').write_text(json.dumps(commands,indent=2));r.check_returncode()
 run('configure',[sys.executable,str(emroot/'emcmake.py'),'cmake','-S',str(frozen/'wasm'),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/ninja.exe'])
 generated=(build/'generated/emulator_sources.cmake').read_text();assert all(x in generated for x in required)
 run('build',['cmake','--build',str(build),'--target','emulator_web','--parallel',str(a.jobs)])
 package_source=frozen
 if a.package_root:
  package_source=a.package_root.resolve();assert not package_source.exists(),'Package staging must be a new immutable directory'
  assert package_source!=published and package_source not in published.parents and published not in package_source.parents
  shutil.copytree(frozen/'wasm',package_source/'wasm',copy_function=shutil.copy2)
  shutil.copytree(frozen/'out/wasm/release',package_source/'out/wasm/release',copy_function=shutil.copy2)
 run('package',['node',str(package_source/'wasm/package.mjs'),'Release'])
 run('package-validate',['node',str(package_source/'wasm/package.mjs'),'Release','--validate'])
 database=json.loads((build/'compile_commands.json').read_text());assert all(any(Path(row['file']).name==font for row in database) for font in required)
 shutil.copy2(build/'compile_commands.json',out/'compile_commands.json')
 assert all(sha(frozen/row['path'])==row['sha256'] for row in copies),'Frozen source mutated during build'
 package=package_source/'out/wasm/dist/release';record.update(passed=True,webRoot=str(package),compileDatabase=str(out/'compile_commands.json'),package={x.name:sha(x) for x in package.iterdir() if x.is_file()},frozenSourceUnchanged=True)
 (scratch/'last-package.json').write_text(json.dumps(dict(webRoot=str(package),package=record['package']),indent=2))
 (out/'summary.json').write_text(json.dumps(record,indent=2));print('PASS isolated full web rebuild',package)
if __name__=='__main__':main()
