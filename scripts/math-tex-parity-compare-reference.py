#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Normalize observed NumOS and actual LuaHBTeX glyph origins to root baseline."""
import argparse,collections,importlib.util,json,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--geometry',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 spec=importlib.util.spec_from_file_location('oracle',ROOT/'scripts/math-tex-parity-oracle.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
 cases,_=mod.load(a.geometry/'geometry.log');tex={(e['id'],e['style']):e for e in (json.loads(x) for x in (a.reference/'tex-geometry.jsonl').read_text().splitlines())}
 rows=[];distributions=collections.defaultdict(list);groups=collections.defaultdict(lambda:collections.defaultdict(list))
 for c in cases:
  key=(c['id'],c['style'])
  if key not in tex:continue
  r=tex[key];frame=c['frames'][0];root=frame['nodes']['r'];ng=frame['glyphs'];tg=[x for x in r['nodes'] if x['kind']=='glyph'];same=[x['cp'] for x in ng]==[x['cp'] for x in tg]
  row=dict(id=c['id'],style=c['style'],sequenceEqual=same,numosGlyphs=len(ng),referenceGlyphs=len(tg),numosBox=[root['width'],root['ascent'],root['descent']],referenceBox=[r['width'],r['ascent'],r['descent']],glyphs=[])
  if same:
   for n,t in zip(ng,tg):
    dx=n['x']-root['x']-t['x'];dy=n['baseline']-root['baseline']-t['baseline']
    row['glyphs'].append(dict(cp=n['cp'],numosPath=n['path'],dx=round(dx,5),dy=round(dy,5),texFont=t['fontSize']))
    distributions['dx'].append(dx);distributions['dy'].append(dy)
    groups[c['id'].split('-')[0]]['dx'].append(dx);groups[c['id'].split('-')[0]]['dy'].append(dy)
  else:row['limit']='Glyph runs differ because structural operator/normalization policies differ; no naive pixel equality is computed.'
  rows.append(row)
 (a.out/'normalized-tex-diff.json').write_text(json.dumps(rows,indent=2))
 def stats(d):return {k:dict(count=len(v),mean=statistics.mean(v),median=statistics.median(v),p95Absolute=sorted(abs(x) for x in v)[int(len(v)*.95)],maxAbsolute=max(abs(x) for x in v)) for k,v in d.items()}
 summary=dict(cases=len(rows),exactGlyphSequence=sum(x['sequenceEqual'] for x in rows),positions=stats(distributions),perFamily={k:stats(v) for k,v in groups.items()},policy='Diagnostic, not a blanket pass threshold. Same font identity and 18/12/8pt, but outline design metrics differ from LVGL rounded bitmap advances. Exact ordered glyph submissions are required; opposite paint ordering such as postfix labels is conservatively excluded. Inspect systematic bias per family and script/bar normalized origins.')
 (a.out/'normalized-tex-summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
