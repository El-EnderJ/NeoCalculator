"""Reproduce the authorized dirty source snapshot and run isolated host checks.

This never resets/stages the shared workspace or modifies a pre-existing checkout.
WASM commands are run separately against the same recorded ASCII snapshot.
"""
import argparse, hashlib, json, os, pathlib, shutil, subprocess

root=pathlib.Path(__file__).resolve().parents[1]
out=root/'out/tutor-engine-01/review-math/regressions'
snapshot=pathlib.Path('C:/.codex-cache/numos-tutor-engine-01-final')
ap=argparse.ArgumentParser();ap.add_argument('action',choices=['snapshot','overlay','host']);args=ap.parse_args()
out.mkdir(parents=True,exist_ok=True)
if args.action=='snapshot':
    if snapshot.exists():raise SystemExit('Refusing to replace a pre-existing snapshot')
    subprocess.run(['git','clone','--shared','--no-checkout',str(root),str(snapshot)],check=True)
    # The original index is only read. Its copy preserves the source repository's
    # staged baseline, so build identity correctly reports the dirty overlay.
    shutil.copy2(root/'.git/index',snapshot/'.git/index')
    names=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=root).decode().split('\0')
    manifest=[]
    for name in sorted(set(names)):
        if not name:continue
        src=root/name
        if not src.is_file():manifest.append({'path':name,'missing':True});continue
        data=src.read_bytes();target=snapshot/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target)
        if hashlib.sha256(target.read_bytes()).digest()!=hashlib.sha256(data).digest():raise RuntimeError('Snapshot mismatch '+name)
        manifest.append({'path':name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)})
    for item in manifest:
        if 'sha256' in item and hashlib.sha256((root/item['path']).read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Source changed during snapshot '+item['path'])
    for dep in ('.pio/libdeps/emulator_pc/lvgl','tests/wasm/node_modules','out/equations-rebuild/final/giac-host/obj'):
        shutil.copytree(root/dep,snapshot/dep,copy_function=shutil.copy2)
    encoded=json.dumps(manifest,separators=(',',':'),sort_keys=True).encode()
    record={'root':str(root),'snapshot':str(snapshot),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_fingerprint':hashlib.sha256(encoded).hexdigest(),'files':manifest,'copied_dependencies':['.pio/libdeps/emulator_pc/lvgl','tests/wasm/node_modules','out/equations-rebuild/final/giac-host/obj']}
    (out/'source-snapshot.json').write_text(json.dumps(record,indent=2))
    print(json.dumps({k:v for k,v in record.items() if k!='files'},indent=2))
elif args.action=='overlay':
    record=json.loads((out/'source-snapshot.json').read_text())
    version=record.get('overlay_version',0)+1;shutil.copy2(out/'source-snapshot.json',out/f'source-snapshot-before-overlay-{version}.json')
    record['overlay_version']=version
    requested=['src/apps/EquationsApp.cpp','src/apps/EquationsApp.h','src/math/tutor/Derivation.h','src/math/tutor/Messages.inc','src/math/giac/GiacEngine.cpp','src/math/giac/GiacTutor.inc','scripts/test-tutor-engine.py','scripts/test-equations-rebuild.py','src/hal/NativeHal.cpp','scripts/build-giac-host-harness.sh','scripts/build-tutor-host.py','tests/host/tutor_engine_main.cpp','tests/wasm/smoke.mjs']
    requested += ['docs/TUTOR_GIAC_REUSE.md']
    requested += [p.relative_to(root).as_posix() for pattern in ('scripts/tutor-probe-*','tests/host/tutor_probe*') for p in root.glob(pattern) if p.is_file()]
    requested += [x['path'] for x in record['files'] if x['path'].startswith('tests/emulator/scripts/') and x['path'].endswith('.numos')]
    table={x['path']:x for x in record['files']};changes=[]
    for name in requested:
        source=root/name;data=source.read_bytes();digest=hashlib.sha256(data).hexdigest();prior=table.get(name,{}).get('sha256')
        if digest==prior:continue
        target=snapshot/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        if hashlib.sha256(target.read_bytes()).hexdigest()!=digest:raise RuntimeError('Overlay mismatch '+name)
        # The origin file can be older than an object built from the preceding
        # snapshot. Refresh the destination timestamp so Ninja must consume the
        # changed bytes instead of trusting a source-only mtime comparison.
        os.utime(target,None)
        table[name]={'path':name,'sha256':digest,'bytes':len(data)};changes.append({'path':name,'before':prior,'after':digest})
    record['files']=[table[name] for name in sorted(table)];record['prior_fingerprint']=record['source_fingerprint']
    encoded=json.dumps(record['files'],separators=(',',':'),sort_keys=True).encode();record['source_fingerprint']=hashlib.sha256(encoded).hexdigest();record['overlay']=changes
    (out/'source-snapshot.json').write_text(json.dumps(record,indent=2));(out/'final-overlay.json').write_text(json.dumps(changes,indent=2))
    print(json.dumps({'source_fingerprint':record['source_fingerprint'],'overlay':changes},indent=2))
else:
    record=json.loads((out/'source-snapshot.json').read_text())
    for item in record['files']:
        if 'sha256' in item and hashlib.sha256((snapshot/item['path']).read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Snapshot changed '+item['path'])
    cache=snapshot/'out/tutor-engine-01/review-math/giac-host'
    if not (cache/'obj').exists():
        shutil.copytree(snapshot/'out/equations-rebuild/final/giac-host/obj',cache/'obj',copy_function=shutil.copy2)
    # The official harness always recompiles project/test TUs. Its vendor cache
    # still checks source mtimes, including the reviewed kgen portability fix.
    env=os.environ.copy();env['GIAC_HOST_OUT']=cache.as_posix();env['JOBS']='6'
    with (out/'host-build-and-tests.log').open('w') as log:
        result=subprocess.run(['C:/Program Files/Git/bin/bash.exe','scripts/build-giac-host-harness.sh'],cwd=snapshot,env=env,stdout=log,stderr=subprocess.STDOUT)
    (out/'host-run.json').write_text(json.dumps({'exit':result.returncode,'snapshot_fingerprint':record['source_fingerprint'],'cache':str(cache)},indent=2))
    print('host exit',result.returncode)
    raise SystemExit(result.returncode)
