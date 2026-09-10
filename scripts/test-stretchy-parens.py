#!/usr/bin/env python3
"""Generate candidate-only real-AST parenthesis frames and delimiter traces."""
import argparse, csv, json, os, re, subprocess
from pathlib import Path

CASES = ['parX','parSum','parSin','parFrac','parFracSum','parRoot','parRootPower',
         'parTall','parTallSum','parNested','parNestedSin','parNestedRoot','parLog','parMixed',
         'parAssembly','parAssemblyNested']

def measure_frame(path, log, name, verify=True):
    """Measure dark ink in each delimiter column, independently of layout height.

    A two-row vertical halo catches stroke-cap overshoot. The font's faint
    antialias fringe is included; the pale canvas background is excluded.
    """
    data = path.read_bytes()
    header = b'P6\n320 240\n255\n'
    assert data.startswith(header) and len(data) == len(header) + 320*240*3
    pixels = data[len(header):]

    def bounds(x1, y1, x2, y2):
        points = []
        for y in range(max(54, y1), min(232, y2)):
            for x in range(max(0, x1), min(320, x2)):
                offset = (y*320+x)*3
                if max(pixels[offset:offset+3]) < 250:
                    points.append((x, y))
        if not points:
            return None
        return [min(x for x,y in points), min(y for x,y in points),
                max(x for x,y in points), max(y for x,y in points)]

    records = []
    final_log = log.split('id=stretch_'+name)[-1]
    for line in dict.fromkeys(re.findall(r'\[DELIMITER\] (.*)', final_log)):
        m = {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)', line)}
        base, pw, cx, width = m['baseline'], m['pw'], m['contentX'], m['width']
        top, bottom = base-m['delimA'], base+m['delimD']
        rx = cx+width+(cx-m['x']-pw)
        reach = pw if verify else max(pw+2, m['target']//5+3)
        margin = 0 if verify else 2
        left = bounds(m['x']-margin, top-2, m['x']+reach, bottom+2)
        right = bounds(rx-margin, top-2, rx+reach, bottom+2)
        m.update(case=name,
            content_box=[cx,base-m['contentA'],cx+width-1,base+m['contentD']-1],
            left_box=[m['x'],top,m['x']+pw-1,bottom-1],
            right_box=[rx,top,rx+pw-1,bottom-1],
            left_raster=left,right_raster=right,
            content_raster=bounds(cx,base-m['contentA'],cx+width,base+m['contentD']))
        if left:
            m.update(actual_height=left[3]-left[1]+1,
                top_clearance=base-m['contentA']-left[1],
                bottom_clearance=left[3]-(base+m['contentD']-1))
        if verify:
            assert left and right, (name,'missing delimiter')
            # Authentic paired STIX glyphs share their typographic box. A
            # transparent edge row may differ by one pixel after rasterization;
            # do not distort the font outlines to force identical black pixels.
            assert abs(left[1]-right[1]) <= 1 and abs(left[3]-right[3]) <= 1, (name,'pair alignment',left,right)
            assert top <= left[1] <= left[3] < bottom, (name,'vertical overshoot')
            assert left[2] < cx and right[0] >= cx+width, (name,'intersection')
            assert m['top_clearance'] >= 0 and m['bottom_clearance'] >= 0
            for side in (left, right):
                for y in range(side[1], side[3] + 1):
                    assert bounds(side[0], y, side[2] + 1, y + 1), (name, 'broken delimiter stroke', y)
        records.append(m)
    assert records, (name,'missing delimiter trace')
    return records


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bin',type=Path,default=Path('.pio/build/emulator_pc/program'))
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument("--record-only", action="store_true", help="Record an unfixed baseline without asserting corrected geometry")
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    catalog=Path('src/math/MathRenderVisualCases.cpp').read_text().split('kCases[] = {')[1]
    ids=re.findall(r'\{ "([^"]+)"',catalog)
    records=[]
    for name in CASES:
        slot=ids.index('stretch_'+name)
        script='wait 200\nopen_app MathVisual\nwait 60\n'+'key DOWN\n'*slot+'wait 30\nassert_app MathVisual\n'
        path=args.out/(name+'.numos');path.write_text(script)
        result=subprocess.run([str(args.bin.resolve()),'--headless','--deterministic','--quiet','--frames','1600','--script',str(path),'--screenshot',str(args.out/(name+'.ppm'))],env=dict(os.environ,NUMOS_DELIMITER_METRICS='1'),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        (args.out/(name+'.log')).write_bytes(result.stdout)
        assert result.returncode==0,name
        assert (args.out/(name+'.ppm')).read_bytes().startswith(b'P6\n320 240\n255\n'),name
        records.extend(measure_frame(args.out/(name+'.ppm'),result.stdout.decode(errors='replace'),name,not args.record_only))
        print('PASS',name,flush=True)
    (args.out/'metrics.json').write_text(json.dumps(records,indent=2))
    headers=['case','baseline','contentA','contentD','inkA','inkD','delimA','delimD',
             'target','actual_height','top_clearance','bottom_clearance']
    with (args.out/'metrics.csv').open('w',newline='') as output:
        writer=csv.DictWriter(output,fieldnames=headers,extrasaction='ignore')
        writer.writeheader();writer.writerows(records)

if __name__=='__main__': main()
