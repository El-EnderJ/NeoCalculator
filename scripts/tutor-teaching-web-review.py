#!/usr/bin/env python3
"""Explicit ASCII source snapshot for the teaching UX web regression.

Only the new candidate is written. Prior TUTOR-ENGINE evidence and the frozen
teaching baseline remain untouched. Overlay manifests preserve source history.
"""
import argparse, hashlib, json, os, pathlib, shutil, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
BASELINE=pathlib.Path('C:/.codex-cache/numos-teaching-ux-baseline')
SNAPSHOT=pathlib.Path('C:/.codex-cache/numos-teaching-ux-candidate')
OUT=ROOT/'out/tutor-teaching-ux-01/review-math/web'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write_record(record):
    record['fingerprint']=hashlib.sha256(json.dumps(record['files'],sort_keys=True,separators=(',',':')).encode()).hexdigest()
    (OUT/'source-snapshot.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
    print(record['fingerprint'])
def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['snapshot','overlay','verify']);args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    if args.action=='snapshot':
        if SNAPSHOT.exists():raise RuntimeError('Refusing to replace a pre-existing candidate')
        baseline=json.loads((BASELINE/'snapshot-manifest.json').read_text())
        subprocess.run(['git','clone','--shared','--no-checkout',str(ROOT),str(SNAPSHOT)],check=True)
        shutil.copy2(ROOT/'.git/index',SNAPSHOT/'.git/index')
        for item in baseline['files']:
            if 'sha256' not in item:continue
            source=BASELINE/item['path'];target=SNAPSHOT/item['path']
            assert digest(source)==item['sha256'],item['path']
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        for dependency in ['.pio/libdeps/emulator_pc/lvgl','tests/wasm/node_modules']:
            shutil.copytree(ROOT/dependency,SNAPSHOT/dependency,copy_function=shutil.copy2)
        write_record({'snapshot':str(SNAPSHOT),'baseline':baseline['fingerprint'],'files':baseline['files'],'overlays':0})
        return
    record=json.loads((OUT/'source-snapshot.json').read_text())
    if args.action=='overlay':
        number=record['overlays']+1;shutil.copy2(OUT/'source-snapshot.json',OUT/f'source-before-overlay-{number}.json')
        names=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT).decode().split('\0')
        table={x['path']:x for x in record['files']};changes=[]
        for name in sorted(set(names)):
            if not name:continue
            source=ROOT/name
            if not source.is_file():continue
            data=source.read_bytes();value=hashlib.sha256(data).hexdigest()
            if table.get(name,{}).get('sha256')==value:continue
            target=SNAPSHOT/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);os.utime(target,None)
            changes.append({'path':name,'before':table.get(name,{}).get('sha256'),'after':value})
            table[name]={'path':name,'sha256':value,'bytes':len(data)}
        record['files']=[table[n] for n in sorted(table)];record['overlays']=number
        (OUT/f'overlay-{number}.json').write_text(json.dumps(changes,indent=2));write_record(record)
    for item in record['files']:
        if 'sha256' in item:assert digest(SNAPSHOT/item['path'])==item['sha256'],item['path']
    print('snapshot verified')
if __name__=='__main__':main()
