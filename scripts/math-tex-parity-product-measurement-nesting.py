#!/usr/bin/env python3
"""Real-key regressions for critic1's nested Measurement ink-gap finding.

Functional/semantic success does not assert pixel correctness. Final acceptance
also requires the independent oracle's actual-ink gap evidence, attached by the
geometry gate once available. Before-fix originals are never overwritten.
"""
import argparse
import hashlib
import html
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('corpus',ROOT/'scripts/math-tex-parity-product-corpus.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)


def fixtures():
    # These four sequences are the independent reviewer's real-product repros.
    return {
        'reciprocal-root-measurement':c.keys('1 FRAC SQRT')+c.pick('measurement',750)+c.keys('1 0 0 RIGHT 4'),
        'script-root-measurement':c.keys('2 POW SQRT')+c.pick('measurement',750)+c.keys('1 0 0 RIGHT 4'),
        'incomplete-script-root-measurement':c.keys('2 POW SQRT')+c.pick('measurement',750)+c.keys('RIGHT RIGHT 4'),
        'reciprocal-measurement-ratio':c.keys('1 FRAC 1 FRAC')+c.pick('measurement',750)+c.keys('2 RIGHT 0 . 1'),
    }


def semantic_blocks(log):
    result=[]
    for match in re.finditer(r'(\[CALC-INPUT\].*?)(?=\[ASSERT\])',log,re.S):
        block=re.sub(r'\s+\[-?\d+x-?\d+ a-?\d+/d-?\d+\]','',match[1])
        result.append('\n'.join(line.rstrip() for line in block.splitlines() if line.strip()))
    return result


def finalize(folder, geometry_report, reviewed):
    """Require the independent submitted-ink oracle, not cached layout geometry."""
    assert reviewed,'Direct original review must be explicitly attested after viewing all eight frames'
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    rows=json.loads((folder/'results.json').read_text(encoding='utf8'))
    identity=json.loads((folder/'identity.json').read_text(encoding='utf8'))
    assert len(rows)==4 and all(r['functionalPassed'] and r['structureAndCursorUnchanged'] is True for r in rows)
    assert sha(Path(identity['binary']))==identity['sha256']
    report=json.loads(geometry_report.read_text(encoding='utf8'));assert report['version']==1
    mapping={
        'reciprocal-root-measurement':'K-measurement-frac-root',
        'script-root-measurement':'K-measurement-script-root',
        'incomplete-script-root-measurement':'L-measurement-script-root-empty',
        'reciprocal-measurement-ratio':'K-measurement-reciprocal-ratio',
    }
    selected=[]
    for product,recipe in mapping.items():
        cases=[c for c in report['cases'] if c['id']==recipe]
        assert len(cases)==8 and {c['style'] for c in cases}==set(range(8)),(product,'missing independent styles')
        for case in cases:
            assert case['compoundChecks'] and case['gapChecks'],(product,'empty quantitative checks')
            assert any(c['nodeType']=='Measurement' for c in case['compoundChecks'])
            assert all(c['enclosed'] is True for c in case['compoundChecks']),(product,case['style'],'actual submitted ink escapes reported compound bounds')
            assert all(c['actualGap']>=c['minimumGap'] for c in case['gapChecks']),(product,case['style'],'actual ink gap below independent MATH minimum')
            assert case['pass_'] is True and not case['findings'],(product,case['style'],'independent findings')
        selected.append({'productCase':product,'oracleRecipe':recipe,'styles':8,
                         'compoundChecks':sum(len(c['compoundChecks']) for c in cases),
                         'gapChecks':sum(len(c['gapChecks']) for c in cases),
                         'minimumObservedGap':min(g['actualGap'] for c in cases for g in c['gapChecks'])})
    assert all(c['pass_'] is True and not c['findings'] for c in report['cases']),'Other independent observed-ink findings remain'
    geometry=Path(report['geometry']);manifest=geometry/'source-object-manifest.json'
    assert sha(manifest)==report['sourceObjectManifestSha256']
    assert sha(geometry/'geometry.log')==report['geometrySha256']
    assert sha(geometry/'probe.exe')==report['captureExecutableSha256']
    source_records=json.loads(manifest.read_text(encoding='utf8'))
    bindings={}
    for relative in ('src/math/MathAST.cpp','src/math/MathAST.h','src/ui/MathRenderer.cpp'):
        match=[r for r in source_records if r['path'].replace('\\','/').endswith('/'+relative)]
        assert len(match)==1 and sha(ROOT/relative)==match[0]['sha256'],(relative,'oracle source differs from current candidate freeze')
        bindings[relative]=match[0]['sha256']
    images=[]
    for row in rows:
        for frame in row['images']:
            assert sha(folder/frame['path'])==frame['sha256']
            images.append({'case':row['id'],'frame':frame['frame'],'sha256':frame['sha256']})
    record={'passed':True,'candidateSha256':identity['sha256'],'verifierScriptSha256':sha(Path(__file__)),'oracleReport':str(geometry_report),
            'oracleReportSha256':sha(geometry_report),'sourceBindings':bindings,
            'geometrySha256':report['geometrySha256'],'sourceObjectManifestSha256':report['sourceObjectManifestSha256'],
            'captureExecutableSha256':report['captureExecutableSha256'],'mappedCases':selected,
            'directlyReviewedOriginals':images,'scope':'All four native real-key repros retain tree/cursor states; independent actual submitted primitive unions and minimum ink gaps pass in all eight styles; all eight current originals directly reviewed'}
    (folder/'geometry-closure.json').write_text(json.dumps(record,indent=2),encoding='utf8')
    subset={'sourceReportSha256':sha(geometry_report),'policy':report['policy'],
            'cases':[c for c in report['cases'] if c['id'] in mapping.values()]}
    (folder/'geometry-samples.json').write_text(json.dumps(subset,indent=2),encoding='utf8')
    page=folder/'index.html';original_page=page.read_text(encoding='utf8')
    marker='<section id="quantitative-closure">'
    if marker in original_page:original_page=original_page.split(marker)[0]
    compound_count=sum(r['compoundChecks'] for r in selected);gap_count=sum(r['gapChecks'] for r in selected)
    page.write_text(original_page+marker+f'<h2>Independent geometry closure: PASS</h2><p>All four mapped fixtures pass in eight styles: {compound_count} actual compound-ink containment checks and {gap_count} actual ink-gap checks. All eight current native originals were directly reviewed. The before images are the preserved e019 review checkpoint.</p><p><a href="geometry-closure.json">Source, executable and review hashes</a> · <a href="geometry-samples.json">Actual ink bounds and gap measurements</a></p></section>',encoding='utf8')
    summary=json.loads((folder/'summary.json').read_text(encoding='utf8'))
    summary.update(geometryPassed=True,acceptancePassed=True,pending=None)
    (folder/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
    identity.update(geometryClaim=True,geometryReason='Independent actual-ink gate and direct original review passed',geometryClosure='geometry-closure.json')
    (folder/'identity.json').write_text(json.dumps(identity,indent=2),encoding='utf8')
    print('PASS nested Measurement geometry closure:',sum(r['compoundChecks'] for r in selected),'compound checks;',sum(r['gapChecks'] for r in selected),'actual-ink gaps; eight reviewed native originals')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin',type=Path)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compare',type=Path)
    p.add_argument('--critic-originals',type=Path)
    p.add_argument('--verify-existing',action='store_true',help='Attach checked independent geometry evidence to an existing after-run')
    p.add_argument('--geometry-report',type=Path)
    p.add_argument('--reviewed-originals',action='store_true',help='Reviewer has directly viewed all eight current 320x240 originals')
    a=p.parse_args();os.chdir(ROOT)
    a.out=Path(os.path.relpath(a.out.resolve(),ROOT));a.out.mkdir(parents=True,exist_ok=True)
    if a.verify_existing:
        assert a.geometry_report,'Independent observed-ink report required'
        finalize(a.out,a.geometry_report,a.reviewed_originals);return 0
    assert a.bin,'Executable required for a new native replay'
    assert not (a.out/'summary.json').exists(),'Use a fresh output folder to preserve previous originals'
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    identity={'binary':str(a.bin.resolve()),'sha256':sha(a.bin),'scriptSha256':sha(Path(__file__)),
              'independentFixtureOrigin':'reviews/iteration-1/measurement-native-repro.py',
              'geometryClaim':False,'geometryReason':'Separate independent ink-gap oracle required; screenshots and status alone cannot pass the visual gate'}
    previous={row['id']:row for row in json.loads((a.compare/'results.json').read_text(encoding='utf8'))} if a.compare else None
    env=dict(os.environ,NUMOS_CALC_INPUT_TRACE='1',PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
    rows=[];cards=[]
    for name,body in fixtures().items():
        folder=a.out/name;folder.mkdir();(folder/'fs').mkdir()
        script='wait 200\nopen_app Calculation\nwait 30\n'+body
        for frame,events in [('input',''),('unfocused',c.keys('RIGHT RIGHT RIGHT'))]:
            script+=events+'assert_calc_input structure\nassert_calc_input dump\nwait 5\nscreenshot '+(folder/(frame+'.ppm')).as_posix()+'\n'
        script+='log MEASUREMENT_NESTING_DONE\n'
        path=folder/'repro.numos';path.write_text(script,encoding='utf8')
        command=[str(a.bin.resolve()),'--headless','--deterministic','--quiet','--frames','500','--script',str(path),'--fs-sandbox-dir',str(folder/'fs')]
        r=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,timeout=90)
        log=(r.stdout+r.stderr).decode('utf8',errors='replace');(folder/'run.log').write_text(log,encoding='utf8')
        states=semantic_blocks(log)
        ok=r.returncode==0 and 'MEASUREMENT_NESTING_DONE' in log and len(states)==2
        ok &= all('Measurement[2]' in state for state in states)
        if name.startswith('incomplete-'):ok &= all('Empty' in state for state in states)
        unchanged=previous[name]['states']==states if previous else None
        if previous:ok &= unchanged
        images=[];figures=[]
        for frame in ('input','unfocused'):
            ppm=folder/(frame+'.ppm');target=ppm.with_suffix('.png')
            with Image.open(ppm) as im:
                assert im.size==(320,240);im.save(target)
            record={'frame':frame,'path':target.relative_to(a.out).as_posix(),'sha256':sha(target),'size':[320,240]}
            if a.critic_originals:
                original=a.critic_originals/name/(frame+'.png');assert original.is_file()
                copied=folder/(frame+'-critic-e019.png');copied.write_bytes(original.read_bytes())
                record['criticOriginal']={'path':copied.relative_to(a.out).as_posix(),'sha256':sha(original)}
            if a.compare:
                original=a.compare/name/(frame+'.png');copied=folder/(frame+'-before.png');copied.write_bytes(original.read_bytes())
                figures.append('<figure><img src="'+copied.relative_to(a.out).as_posix()+'"><figcaption>e019 before — '+frame+'</figcaption></figure>')
            figures.append('<figure><img src="'+record['path']+'"><figcaption>'+frame+'</figcaption></figure>');images.append(record)
        rows.append({'id':name,'functionalPassed':bool(ok),'exit':r.returncode,'states':states,'structureAndCursorUnchanged':unchanged,'images':images})
        cards.append('<section><h2>'+html.escape(name)+'</h2>'+''.join(figures)+'</section>')
        print('PASS' if ok else 'FAIL',name,flush=True)
    assert sha(a.bin)==identity['sha256'],'Executable changed during test'
    (a.out/'identity.json').write_text(json.dumps(identity,indent=2),encoding='utf8')
    (a.out/'results.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
    summary={'functionalPassed':all(r['functionalPassed'] for r in rows),'cases':len(rows),'snapshots':sum(len(r['states']) for r in rows),'originalImages':8,'geometryPassed':False,'acceptancePassed':False,'pending':'Independent actual-ink gap detector result and direct before/after original review'}
    (a.out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
    (a.out/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Nested Measurement product repros</title><style>body{font:16px system-ui}section{margin:20px}figure{display:inline-block}img{width:320px;height:240px;image-rendering:pixelated}</style><h1>Nested Measurement product repros</h1><p>Original 320×240 frames. Functional pass alone is not a visual-geometry pass.</p>'+''.join(cards),encoding='utf8')
    return 0 if summary['functionalPassed'] else 1


if __name__=='__main__':raise SystemExit(main())
