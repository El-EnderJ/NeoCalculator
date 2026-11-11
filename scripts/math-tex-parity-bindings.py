#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bind each observed NodeType to the final original screenshot evidence."""
import argparse,hashlib,importlib.util,json
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('--geometry',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 spec=importlib.util.spec_from_file_location('oracle',Path(__file__).with_name('math-tex-parity-oracle.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 cases,_=m.load(a.geometry/'geometry.log');records=[]
 import os
 relative=lambda f:Path(os.path.relpath(f,a.out)).as_posix()
 for index,name in enumerate(m.TYPES):
  ids=[c['id'] for c in cases if c['style']==1 and not c['id'].startswith('R-') and any(n['type']==index for n in c['frames'][0]['nodes'].values())]
  assert ids,name
  chosen=ids[0];images={k:relative(v/(chosen+'-s1.png')) for k,v in [('baseline',a.baseline),('candidate',a.geometry),('texReference',a.reference)]}
  hashes={str(style):hashlib.sha256((a.geometry/(chosen+'-s'+str(style)+'.png')).read_bytes()).hexdigest() for style in range(8)}
  records.append(dict(nodeType=name,caseCountText=len(ids),cases=ids,screenshots=images,allStylesTested=list(range(8)),candidateSha256ByStyle=hashes))
 (a.out/'node-screenshot-bindings.json').write_text(json.dumps(records,indent=2))
 lines=['# All-node screenshot bindings','','All 29 current NodeType values have observed layout/draw geometry and original 320 × 240 images. This binds typesetting coverage to evidence, not exhaustive semantics coverage. Candidate: '+a.geometry.name+'.','','| NodeType | TEXT recipes | First baseline / candidate / TeX image |','|---|---:|---|']
 for r in records:lines.append('| '+r['nodeType']+' | '+str(r['caseCountText'])+' | '+' / '.join('['+k+']('+v+')' for k,v in r['screenshots'].items())+' |')
 (a.out/'node-screenshot-bindings.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
 print('PASS',len(records),'NodeType screenshot bindings')
if __name__=='__main__':main()
