#!/usr/bin/env python3
"""Curate original native product frames into a font-free HTML/ZIP review pack."""
import argparse
import hashlib
import html
import json
from pathlib import Path
import shutil
import zipfile
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--corpus',type=Path,required=True)
p.add_argument('--gates',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--phase',choices=('baseline','final'),required=True)
p.add_argument('--baseline',type=Path)
p.add_argument('--measurement',type=Path,help='Closed nested-Measurement native before/after and quantitative evidence to include')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
selected=[]
for key in ['A-add-subtract','A-unary-negative','A-scientific-fraction','B-negative-exponent','B-nested-exponent','B-e-exponent',
            'C-half','C-continued','C-script-fraction','D-polynomial','D-nested','D-indexed','D-index-expression',
            'E-fraction','E-tall-bracket','F-log-base','F-hyperbolic','G-quadratic-formula-positive','I-derivative-template','I-integral-template',
            'J-acceleration','J-resistance','J-capacitance','J-temperature-difference','K-measurement','K-temperature-measurement',
            'L-power-both','L-fraction-numerator','L-fraction-denominator','L-indexed-index','L-indexed-radicand','L-log-base',
            'L-measurement-nominal','L-measurement-uncertainty','L-integral-empty','M-very-wide','M-wide-tall','N-power-depth-6','N-fraction-depth-5','N-alternating']:
    selected.append((key,a.corpus/key/'input.png'))
for name in ['format-standard','format-relative','format-expanded','format-parenthetic','temperature-fahrenheit','constant-gravity','codata-correlated-product',
             'en-US-measure','en-GB-measure','es-ES-measure','es-419-measure','es-ES-menu','es-419-modes']:
    selected.append(('science-'+name,a.gates/'scientific-product'/(name+'.png')))
for name in ['correlated-start','correlated-end','correlated-return']:
    selected.append(('pan-'+name,a.gates/'scientific-visuals'/(name+'.png')))
for name in ['derivative-result','derivative-tall-input','integral-x2-result']:
    path=a.gates/'calculus-rebuild'/(name+'.ppm')
    assert path.is_file(),path
    selected.append(('calculus-'+name,path))
for locale in ['guided','es']:
    paths=sorted((a.gates/'tutor-composition-ui').glob('composition-sine-'+locale+'-*-scroll0.png'))
    assert paths,'missing Tutor product frames'
    selected.append(('tutor-periodic-'+locale,paths[-1]))
    tail=sorted((a.gates/'tutor-composition-ui').glob('composition-sine-'+locale+'-*-scroll*.png'))
    selected.append(('tutor-periodic-parameter-'+locale,tail[-1]))
    paths=sorted((a.gates/'tutor-composition-ui').glob('composition-tangent-'+locale+'-*-scroll0.png'))
    assert paths,'missing tangent product frames'
    selected.append(('tutor-tangent-'+locale,paths[-1]))

manifest=[];cards=[]
for label,path in selected:
    assert path.is_file(),path
    im=Image.open(path);assert im.size==(320,240),(path,im.size)
    target=a.out/(label+'.png');im.save(target)
    record=dict(id=label,source=str(path),file=target.name,width=320,height=240,sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    figures=''
    if a.baseline:
        prior=a.baseline/target.name;assert prior.is_file(),prior
        old=a.out/(label+'-baseline.png');old.write_bytes(prior.read_bytes())
        record['baseline']=old.name
        figures+='<figure><img src="'+old.name+'"><figcaption>Baseline</figcaption></figure>'
    figures+='<figure><img src="'+target.name+'"><figcaption>'+a.phase+'</figcaption></figure>'
    cards.append('<section><h2>'+html.escape(label)+'</h2>'+figures+'</section>')
    manifest.append(record)
(a.out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
(a.out/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>NumOS product '+a.phase+'</title><style>body{font:16px system-ui;background:#eef2f6;color:#122237;margin:20px}section{background:#fff;margin:16px 0;padding:12px;border-radius:8px}h2{font-size:17px}figure{display:inline-block;margin:8px}img{width:320px;height:240px;image-rendering:pixelated;border:1px solid #b8c0cc}figcaption{text-align:center}</style><h1>NumOS native product '+a.phase+'</h1><p>All frames are original 320×240 pixels. Review is of the native emulator. No fonts, external resources or hardware claims.</p>'+''.join(cards)+'</html>',encoding='utf8')
if a.measurement:
    status=json.loads((a.measurement/'summary.json').read_text(encoding='utf8'))
    assert status['acceptancePassed'] and status['geometryPassed'],'Nested Measurement quantitative/review gate not closed'
    destination=a.out/'measurement-nesting'
    for source in a.measurement.rglob('*'):
        if source.is_file() and source.suffix in ('.png','.html','.json') and 'fs' not in source.relative_to(a.measurement).parts:
            target=destination/source.relative_to(a.measurement);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    page=a.out/'index.html'
    page.write_text(page.read_text(encoding='utf8').replace('</html>','<section><h2>Nested Measurement review fix</h2><p><a href="measurement-nesting/index.html">Four real-key cases, eight before/after originals and independent actual-ink checks</a>. These before images identify the e019 review checkpoint separately from the original product baseline above.</p></section></html>'),encoding='utf8')
with zipfile.ZipFile(a.out/'product-gallery.zip','w',zipfile.ZIP_DEFLATED) as package:
    for path in sorted(a.out.rglob('*')):
        if path.is_file() and path.suffix in ('.png','.html','.json'):package.write(path,path.relative_to(a.out).as_posix())
with zipfile.ZipFile(a.out/'product-gallery.zip') as package:
    assert package.testzip() is None
    assert all(Path(p).suffix in ('.png','.html','.json') for p in package.namelist())
print('PASS',len(manifest),'original product images; relative HTML and CRC-validated font-free ZIP')
