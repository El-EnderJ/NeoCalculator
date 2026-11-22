#!/usr/bin/env python3
"""Snapshot the scientific candidate delta and verify preservation/source provenance.

No staging, commit, cleanup, publication, or network access. Run after all writers
have finished; the manifest deliberately excludes its own bytes.
"""
from pathlib import Path
import hashlib,json,subprocess,zipfile
from datetime import datetime,timezone

root=Path(__file__).resolve().parents[1]
base=root/'out/scientific-quantities-01';out=base/'E-final'
out.mkdir(parents=True,exist_ok=True)
def git(*args):return subprocess.check_output(['git',*args],cwd=root)
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
def record(p):return {'path':p.relative_to(root).as_posix() if p.is_relative_to(root) else p.as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)}
head=git('rev-parse','HEAD').decode().strip()
assert head=='ab4017764a62a03e0befd51a50fa82f3c31b4b70',head
assert '<!-- FINAL_' not in (root/'docs/SCIENTIFIC_QUANTITIES_01.md').read_text(encoding='utf8'),'Report is not complete'
assert not git('diff','--cached','--name-only').strip(),'Index changed'
protected=[]
for current,saved in [('docs/RESUMEN_CHAT_TOOLBOX.png','RESUMEN_CHAT_TOOLBOX.png'),('.vscode/settings.json','settings.json')]:
 a=root/current;b=base/'A-baseline'/saved
 assert sha(a)==sha(b),(current,'protected content changed')
 protected.append(record(a))
changed=set(filter(None,git('diff','--name-only','-z').decode('utf8').split('\0')))
changed.update(filter(None,git('ls-files','--others','--exclude-standard','-z').decode('utf8').split('\0')))
changed.discard('docs/RESUMEN_CHAT_TOOLBOX.png')
files=[record(root/rel) for rel in sorted(changed) if (root/rel).is_file()]
assert not any('font' in x['path'].lower() for x in files),'Unexpected font modification'
with zipfile.ZipFile(out/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as z:
 for r in files:z.write(root/r['path'],r['path'])
 z.writestr('BASELINE.txt',head+'\nThis archive is the candidate delta; apply over A-baseline/HEAD.zip.\n')
(out/'working.patch').write_bytes(git('diff','--binary','HEAD'))

# Production source provenance must match the locally tested WASM snapshot.
web_manifests=sorted((out/'web-build').glob('*-source.json'))
assert web_manifests,'Missing web source snapshot'
web=json.loads(web_manifests[-1].read_text(encoding='utf8'))
production=[r for r in web if r['path'].startswith(('src/','include/','wasm/','boards/')) or r['path']=='platformio.ini']
for r in production:
 p=root/r['path']
 assert p.exists() and sha(p)==r['sha256'],('Web snapshot mismatch',r['path'])
artifacts=[]
for path in [Path('C:/.piobuild/numOS-scientific-build/native/emulator_pc/program.exe'),Path('C:/.piobuild/numOS-scientific-build/pool/emulator_pc/program.exe'),Path('C:/.piobuild/numOS/emulator_pc/program.exe'),Path('C:/.piobuild/numOS/emulator_pc/previous-program.exe')]:
 if path.exists():artifacts.append(record(path))
for board in ('numos-esp32-s3-wroom-1u-n16r8','esp32s3_n16r8'):
 p=Path('C:/.piobuild/numOS-scientific-build/firmware')/board/'firmware.bin'
 if p.exists():artifacts.append(record(p))
evidence=[]
for p in sorted(base.rglob('*')):
 if p.is_file() and p.name not in ('final-manifest.json','final-manifest.sha256'):
  evidence.append(record(p))
manifest={'schema':1,'created':datetime.now(timezone.utc).isoformat(),'baseline':head,'branch':git('branch','--show-current').decode().strip(),'subject':git('show','-s','--format=%s','HEAD').decode().strip(),'indexUnchanged':True,'status':git('status','--short').decode('utf8'),'worktrees':git('worktree','list','--porcelain').decode('utf8'),'protected':protected,'changedFiles':files,'webSourceManifest':record(web_manifests[-1]),'productionSourceIdentityVerified':True,'artifacts':artifacts,'evidence':evidence,'sourceArchiveScope':'Changed/new files only, paired with preserved baseline; no font files','hardware':'not executed','commitPushDeploy':'not performed'}
dest=out/'final-manifest.json';dest.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
(out/'final-manifest.sha256').write_text(sha(dest)+'  final-manifest.json\n',encoding='ascii')
print(json.dumps({'changedFiles':len(files),'evidenceFiles':len(evidence),'manifest':str(dest),'sha256':sha(dest)}))
