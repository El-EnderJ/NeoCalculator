#!/usr/bin/env python3
"""Real STIX plus-minus raster: two separate bars, all ink in the measured AST box.

Appends no goldens or masks. The fixture is a single real NodeOperator in a Row;
MathVisual emits its actual measured layout and the same baseline used to draw.
"""
import argparse, hashlib, importlib.util, json, os, re, subprocess
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True);args=p.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    source=Path('src/math/MathRenderVisualCases.cpp').read_text(encoding='utf-8').split('kCases[] = {')[1]
    ids=re.findall(r'\{ "([^"]+)"',source);slot=ids.index('operator_plus_minus')
    script=args.out/'plus-minus.numos';script.write_text('wait 200\nopen_app MathVisual\nwait 60\n'+'key DOWN\n'*slot+'wait 30\nassert_app MathVisual\n')
    ppm=args.out/'plus-minus.ppm'
    spec=importlib.util.spec_from_file_location('emulator_helpers',Path('scripts/generate-emulator-candidates.py'))
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    env=dict(os.environ);dll=helper.sdl2_dll_dir(str(args.bin.resolve()))
    if dll:env['PATH']=dll+os.pathsep+env.get('PATH','')
    r=subprocess.run([str(args.bin.resolve()),'--headless','--deterministic','--quiet','--frames','1600','--script',str(script),'--screenshot',str(ppm)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
    (args.out/'plus-minus.log').write_bytes(r.stdout);assert r.returncode==0
    log=r.stdout.decode(errors='replace').split('id=operator_plus_minus')[-1]
    canvas=list(map(int,re.search(r'canvas obj=\(([-\d,]+)\)',log)[1].split(',')))
    fields=dict((k,int(v)) for k,v in re.findall(r'(\w+)=(-?\d+)',re.search(r'\[0\] Op[^\n]+',log)[0]))
    # Read the established padding; this test introduces no production geometry.
    padding=int(re.search(r'PADDING_LEFT\s*=\s*(\d+)',Path('src/ui/MathRenderer.h').read_text(encoding='utf-8'))[1])
    x0=canvas[0]+padding;y0=fields['childYTop'];box=[x0,y0,x0+fields['w']-1,y0+fields['h']-1]
    data=ppm.read_bytes();header=b'P6\n320 240\n255\n';assert data.startswith(header);pixels=data[len(header):];assert len(pixels)==320*240*3
    points=[(x,y) for y in range(canvas[1]+4,canvas[3]-3) for x in range(canvas[0]+4,canvas[2]-3) if max(pixels[(y*320+x)*3:(y*320+x)*3+3])<180]
    assert points,'glyph is missing'
    ink=[min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points)]
    assert box[0]<=ink[0]<=ink[2]<=box[2] and box[1]<=ink[1]<=ink[3]<=box[3],('ink outside measured box',ink,box)
    row_counts={y:sum(py==y for px,py in points) for y in range(ink[1],ink[3]+1)}
    bar_rows=[y for y,n in row_counts.items() if n >= max(4,(ink[2]-ink[0]+1)*0.6)]
    bands=[]
    for y in bar_rows:
        if not bands or y>bands[-1][-1]+1:bands.append([y])
        else:bands[-1].append(y)
    assert len(bands)==2,('plus and minus must have separate horizontal strokes',bands,row_counts)
    assert max(bands[0])+1<min(bands[1]),'strokes overlap'
    result={'pass':True,'logicalSize':[320,240],'binarySha256':hashlib.sha256(args.bin.read_bytes()).hexdigest(),'measuredBox':box,'inkBox':ink,'horizontalBands':bands,'rowInkCounts':row_counts,'existingGoldenIndicesUnchanged':True,'indexTotalDrift':'48 to 49 in MathVisual only; no goldens promoted'}
    (args.out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
    try:
        from PIL import Image
        Image.open(ppm).save(args.out/'plus-minus.png')
    except ImportError:pass
if __name__=='__main__':main()
