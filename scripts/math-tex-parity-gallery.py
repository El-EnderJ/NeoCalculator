#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package original native/TeX frames for review, with no font assets."""
import argparse,html,json,shutil,zipfile
from pathlib import Path
from PIL import Image
def main():
 p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--oracle',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--archive-name',default='MATH-TEX-PARITY-02-gallery.zip');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 corpus=json.loads((a.candidate/'master-corpus.json').read_text(encoding='utf-8'))['cases'];parts=[]
 for c in corpus:
  images=[]
  for label,source,kind in [('Baseline',a.baseline,'baseline'),('Candidate',a.candidate,'candidate'),('LuaHBTeX / STIX 18-12-8',a.reference,'reference')]:
   frames=[]
   for style in range(8):
    original=source/(c['id']+'-s'+str(style)+'.png');target=a.out/kind/original.name;visibility='' if style==1 else ' hidden'
    if original.exists():
     with Image.open(original) as im:assert im.size==(320,240),(original,im.size)
     target.parent.mkdir(exist_ok=True);shutil.copy2(original,target);frames.append('<img loading="lazy" data-style="'+str(style)+'"'+visibility+' width="320" height="240" src="'+kind+'/'+original.name+'" alt="'+html.escape(c['formula'])+'">')
    else:frames.append('<p data-style="'+str(style)+'"'+visibility+'>This source has no frame for the selected style.</p>')
   images.append('<figure><figcaption>'+label+'</figcaption>'+''.join(frames)+'</figure>')
  overlay=a.oracle/'overlays'/(c['id']+'.png')
  if overlay.exists():
   (a.out/'overlay').mkdir(exist_ok=True);shutil.copy2(overlay,a.out/'overlay'/overlay.name);images.append('<figure data-style="1"><figcaption>Logical boxes / baselines</figcaption><img width="320" height="240" src="overlay/'+overlay.name+'" alt="Geometry overlay"></figure>')
  parts.append('<article data-group="'+c['group']+'" id="'+c['id']+'"><h2>'+html.escape(c['id'])+' · '+html.escape(c['formula'])+'</h2><div class="frames">'+''.join(images)+'</div></article>')
 page='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>NumOS MATH-TEX-PARITY-02 review</title><style>body{margin:24px;background:#f2f4f6;color:#142431;font:15px system-ui}h1{font-size:26px}header{position:sticky;top:0;background:#f2f4f6;padding:12px 0;z-index:1;border-bottom:1px solid #abc}article{padding:16px 0;border-bottom:1px solid #bbc}h2{font-size:16px}.frames{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0;width:320px}figcaption{font-weight:600;padding:6px 0}img{background:#fff;image-rendering:pixelated;border:1px solid #bbc}select{padding:5px}a{color:#0756a0}</style><header><h1>NumOS · mathematical typesetting review</h1><p>Original 320×240 frames. Native uses integer bitmap metrics; LuaHBTeX uses the same STIX input font with 18/12/8 sizes and outline rasterization. Baselines are recorded independently, so screen positions need not match. Blue boxes and red baselines are diagnostic overlays.</p><label>Math style <select id="style"><option value="0">Display</option><option value="1" selected>Text</option><option value="2">Script</option><option value="3">ScriptScript</option><option value="4">Display cramped</option><option value="5">Text cramped</option><option value="6">Script cramped</option><option value="7">ScriptScript cramped</option></select></label> <label>Corpus group <select id="group"><option value="all">All A–N</option>'''+''.join('<option>'+g+'</option>' for g in 'ABCDEFGHIJKLMN')+'''</select></label> · <a href="master-corpus.json">Corpus</a> · <a href="oracle-summary.json">Geometry verdict</a> · <a href="reference-version.json">Reference provenance</a></header>'''+''.join(parts)+'''<script>document.querySelector('#style').addEventListener('change',e=>document.querySelectorAll('[data-style]').forEach(i=>i.hidden=i.dataset.style!==e.target.value));document.querySelector('#group').addEventListener('change',e=>document.querySelectorAll('article').forEach(a=>a.hidden=e.target.value!=='all'&&a.dataset.group!==e.target.value))</script></html>'''
 (a.out/'index.html').write_text(page,encoding='utf-8');shutil.copy2(a.candidate/'master-corpus.json',a.out/'master-corpus.json');shutil.copy2(a.oracle/'summary.json',a.out/'oracle-summary.json');shutil.copy2(a.reference/'tex-reference-version.json',a.out/'reference-version.json')
 files=list(a.out.rglob('*'));archive=a.out.parent/a.archive_name
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  for f in files:
   if f.is_file():
    assert f.suffix.lower() not in ['.ttf','.otf','.woff','.woff2','.pdf'];z.write(f,f.relative_to(a.out))
 with zipfile.ZipFile(archive) as z:assert z.testzip() is None
 print('PASS',len(corpus),'gallery recipes; ZIP',archive)
if __name__=='__main__':main()
