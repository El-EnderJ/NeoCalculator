#!/usr/bin/env python3
"""Detect BR-02..05 through real Calculation events and framebuffer evidence.

--bin is the private read-only Calculation observer (see recovery report).
No formulas or results are injected. --baseline requires all seven detectors
to expose the preserved failures; it does not bless new goldens.
"""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--baseline', action='store_true')
a = p.parse_args()
os.chdir(ROOT)
a.out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, NUMOS_CALC_INPUT_TRACE='1', PATH=
           'C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH', ''))
records = []
physical = {'P_FORMAT':(4,5),'P_ALPHA':(3,0),'P_SHIFT':(4,0),'P_LEFT':(3,1),'P_RIGHT':(1,1),'P_UP':(2,0),'P_DOWN':(2,1)}
keys = lambda s: ''.join(('calc_physical %d %d\n' % physical[k]) if k in physical else 'key '+k+'\n' for k in s.split())
# DEVICE-INPUT-PATH-01: a result scrolls with SHIFT + arrow; a plain arrow
# re-edits the expression.
cases = {
    'input-horizontal-pan': [('start', ' + '.join(str(i%9+1) for i in range(27))+' ENTER'), ('end', 'P_SHIFT P_RIGHT '*50), ('edge', 'P_SHIFT P_RIGHT '*10), ('back', 'P_SHIFT P_LEFT '*60)],
    'input-vertical-pan': [('start', '1 / '*12+'2 ENTER'), ('end', 'P_SHIFT P_DOWN '*30), ('edge', 'P_SHIFT P_DOWN '*10), ('back', 'P_SHIFT P_UP '*40)],
    'fraction-caret': [('start', '1 / '*12+'2'), ('end', 'LEFT')],
    'deep-root': [('start', 'SQRT '*14+'2')],
    'exact-pan': [('start', '2 ^ 2 0 0 ENTER'), ('end', 'P_SHIFT P_RIGHT '*120),
                  ('edge', 'P_SHIFT P_RIGHT '*10), ('back', 'P_SHIFT P_LEFT '*120)],
    'periodic-pan': [('start', '1 / 9 7 ENTER '+('sd' if a.baseline else 'P_SHIFT P_ALPHA P_FORMAT DOWN DOWN ENTER')), ('end', 'P_SHIFT P_RIGHT '*120),
                     ('edge', 'P_SHIFT P_RIGHT '*10), ('back', 'P_SHIFT P_LEFT '*120)],
    'extended-bound': [('start', '1 / 7 ENTER '+('sd sd' if a.baseline else 'P_SHIFT P_ALPHA P_FORMAT DOWN DOWN DOWN ENTER')), ('end', 'P_SHIFT P_RIGHT '*120),
                       ('edge', 'P_SHIFT P_RIGHT '*10), ('back', 'P_SHIFT P_LEFT '*120)],
}
for name, phases in cases.items():
    folder = a.out/name
    folder.mkdir(exist_ok=True)
    script = 'wait 200\nopen_app Calculation\nwait 30\n'
    for phase, sequence in phases:
        script += keys(sequence)+'wait 3\nlog PHASE_'+phase+'\nassert_calc_input dump\n'
        script += 'screenshot '+(folder/(phase+'.ppm')).as_posix()+'\n'
    script += 'log VIEWPORT_DONE\n'
    replay = folder/'repro.numos'
    replay.write_text(script)
    frames = sum(int(l.split()[1])+1 if l.startswith('wait ') else 1 for l in script.splitlines())+100
    result = subprocess.run([str(a.bin.resolve()), '--headless', '--deterministic', '--quiet',
                             '--frames', str(frames), '--script', str(replay)],
                            env=env, capture_output=True, timeout=45)
    log = (result.stdout+result.stderr).decode('utf-8', errors='replace')
    (folder/'run.log').write_text(log, encoding='utf-8')
    assert result.returncode == 0 and 'VIEWPORT_DONE' in log, name
    images = {}
    for phase, _ in phases:
        im = Image.open(folder/(phase+'.ppm')).convert('RGB')
        assert im.size == (320, 240)
        im.save(folder/(phase+'.png'))
        im.resize((1280,960), Image.Resampling.NEAREST).save(folder/(phase+'-4x.png'))
        images[phase] = im
    metrics = {}
    if name == 'fraction-caret':
        rows = re.findall(r'\[BREAK-GEOMETRY\] canvas=([\d,-]+) cursor=([\d,-]+)', log)
        assert len(rows) == len(phases), 'observer missing'
        box, cursor = (list(map(int, s.split(','))) for s in rows[-1])
        metrics = dict(canvas=box, cursor=cursor)
        passed = (box[0] <= cursor[0] <= cursor[2] <= box[2] and
                  box[1] <= cursor[1] < cursor[3] <= box[3] and cursor[3]-cursor[1] >= 5)
    elif name == 'deep-root':
        reds = sum(r>g+70 and r>b+70 for r,g,b in images['start'].crop((6,25,314,234)).get_flattened_data())
        metrics['redTruncationPixels'] = reds
        passed = reds == 0
    else:
        tag = 'GEOMETRY' if name.startswith('input-') else 'RESULT'
        boxes = re.findall(r'\[BREAK-'+tag+r'\] canvas=([\d,-]+)', log)
        # Physical events also emit observations before the named capture.
        boxes = re.findall(r'PHASE_\w+\s+\[RESULT-EXACT\].*?\[BREAK-'+tag+r'\] canvas=([\d,-]+)', log, re.S)
        assert len(boxes) == len(phases), 'observer missing'
        x1,y1,x2,y2 = map(int, boxes[0].split(','))
        crop = (max(0,x1), max(24,y1), min(320,x2+1), min(240,y2+1))
        ink = lambda im: sum(max(rgb)<160 for rgb in im.crop(crop).get_flattened_data())
        same = lambda l,r: ImageChops.difference(images[l].crop(crop),images[r].crop(crop)).getbbox() is None
        metrics = dict(endInk=ink(images['end']), moved=not same('start','end'),
                       endpointStable=same('end','edge'), roundTrip=same('start','back'))
        passed = metrics['endInk'] > 15 if name == 'extended-bound' else metrics['moved']
        if not a.baseline:
            passed = passed and metrics['endpointStable'] and metrics['roundTrip']
    records.append(dict(id=name, repaired=passed, metrics=metrics))
    print(name, 'REPAIRED' if passed else 'DETECTED', metrics, flush=True)
(a.out/'results.json').write_text(json.dumps(dict(
    binary=str(a.bin.resolve()), sha256=hashlib.sha256(a.bin.read_bytes()).hexdigest(),
    baseline=a.baseline, checks=records), indent=2))
assert all(r['repaired'] != a.baseline for r in records), records
