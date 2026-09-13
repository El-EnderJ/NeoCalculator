#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Capture real 320x240 notation/Delta fixtures; check measured Delta ink.

Uses the existing MathVisual diagnostic catalog and compiled glyphs. No golden
images or masks are updated. Inspect the resulting contact sheet as well.
"""
import argparse,json,os,re,subprocess
from pathlib import Path
from PIL import Image,ImageDraw

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    source=Path('src/math/MathRenderVisualCases.cpp').read_text(encoding='utf-8').split('kCases[] = {')[1]
    ids=re.findall(r'\{ "([^"]+)"',source); selected=[s for s in ids if s.startswith('notation_')]
    env=dict(os.environ);env['PATH']='C:/SDL2/x86_64-w64-mingw32/bin;C:/mingw64/bin;'+env.get('PATH','')
    sheet=Image.new('RGB',(960,264*((len(selected)+2)//3)),'#dddddd');draw=ImageDraw.Draw(sheet)
    records=[]
    for i,name in enumerate(selected):
        script=a.out/(name+'.numos');ppm=a.out/(name+'.ppm')
        script.write_text('wait 200\nopen_app MathVisual\nwait 60\n'+'key DOWN\n'*ids.index(name)+'wait 30\nassert_app MathVisual\n')
        cmd=[str(a.bin.resolve()),'--headless','--deterministic','--quiet','--frames','1600','--script',str(script),'--screenshot',str(ppm)]
        r=subprocess.run(cmd,env=env,capture_output=True,timeout=120);(a.out/(name+'.log')).write_bytes(r.stdout+r.stderr);assert r.returncode==0
        frame=Image.open(ppm);assert frame.size==(320,240);frame.save(a.out/(name+'.png'))
        sheet.paste(frame,((i%3)*320,(i//3)*264+24));draw.text(((i%3)*320+4,(i//3)*264+4),name,fill='black')
        record=dict(name=name,command=cmd,pass_=True)
        if name.startswith('notation_delta'):
            log=r.stdout.decode(errors='replace').split('id='+name)[-1]
            canvas=list(map(int,re.search(r'canvas obj=\(([-\d,]+)\)',log)[1].split(',')))
            fields=dict((k,int(v)) for k,v in re.findall(r'(\w+)=(-?\d+)',re.search(r'\[0\] (?:Symbol|Root|Frac|Pow)[^\n]+',log)[0]))
            padding=int(re.search(r'PADDING_LEFT\s*=\s*(\d+)',Path('src/ui/MathRenderer.h').read_text())[1])
            x0=canvas[0]+padding;y0=fields['childYTop'];box=[x0,y0,x0+fields['w']-1,y0+fields['h']-1]
            points=[(x,y) for y in range(canvas[1]+4,canvas[3]-3) for x in range(canvas[0]+4,canvas[2]-3) if max(frame.getpixel((x,y)))<180]
            assert points
            ink=[min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points)]
            assert box[0]<=ink[0]<=ink[2]<=box[2] and box[1]<=ink[1]<=ink[3]<=box[3],(box,ink)
            record.update(codepoint='U+0394',box=box,ink=ink)
        records.append(record)
    sheet.save(a.out/'contact.png');(a.out/'results.json').write_text(json.dumps(records,indent=2));print('PASS',len(records))

if __name__=='__main__':main()
